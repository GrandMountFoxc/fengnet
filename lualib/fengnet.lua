-- read https://github.com/cloudwu/fengnet/wiki/FAQ for the module "fengnet.core"
local c = require "libfengnet.core"
local fengnet_require = require "fengnet.require"
local tostring = tostring
local coroutine = coroutine
local assert = assert
local pairs = pairs
local pcall = pcall
local table = table
local next = next
local tremove = table.remove
local tinsert = table.insert
local tpack = table.pack
local tunpack = table.unpack
local traceback = debug.traceback

local cresume = coroutine.resume
local running_thread = nil
local init_thread = nil

local function coroutine_resume(co, ...)
	running_thread = co
	return cresume(co, ...)
end
local coroutine_yield = coroutine.yield
local coroutine_create = coroutine.create

local proto = {}
local fengnet = {
	-- read fengnet.h
	PTYPE_TEXT = 0,
	PTYPE_RESPONSE = 1,
	PTYPE_MULTICAST = 2,
	PTYPE_CLIENT = 3,
	PTYPE_SYSTEM = 4,
	PTYPE_HARBOR = 5,
	PTYPE_SOCKET = 6,
	PTYPE_ERROR = 7,
	PTYPE_QUEUE = 8,	-- used in deprecated mqueue, use fengnet.queue instead
	PTYPE_DEBUG = 9,
	PTYPE_LUA = 10,
	PTYPE_SNAX = 11,
	PTYPE_TRACE = 12,	-- use for debug trace
}

-- code cache
fengnet.cache = require "fengnet.codecache"
fengnet._proto = proto

function fengnet.register_protocol(class)
	local name = class.name
	local id = class.id
	assert(proto[name] == nil and proto[id] == nil)
	assert(type(name) == "string" and type(id) == "number" and id >=0 and id <=255)
	proto[name] = class
	proto[id] = class
end

local session_id_coroutine = {}
local session_coroutine_id = {}
local session_coroutine_address = {}
local session_coroutine_tracetag = {}
local unresponse = {}

local wakeup_queue = {}
local sleep_session = {}

local watching_session = {}
local error_queue = {}
local fork_queue = { h = 1, t = 0 }

local auxsend, auxtimeout
do ---- avoid session rewind conflict
	local csend = c.send
	local cintcommand = c.intcommand
	local dangerzone
	local dangerzone_size = 0x1000
	local dangerzone_low = 0x70000000
	local dangerzone_up	= dangerzone_low + dangerzone_size

	local set_checkrewind	-- set auxsend and auxtimeout for safezone
	local set_checkconflict -- set auxsend and auxtimeout for dangerzone

	local function reset_dangerzone(session)
		dangerzone_up = session
		dangerzone_low = session
		dangerzone = { [session] = true }
		for s in pairs(session_id_coroutine) do
			if s < dangerzone_low then
				dangerzone_low = s
			elseif s > dangerzone_up then
				dangerzone_up = s
			end
			dangerzone[s] = true
		end
		dangerzone_low = dangerzone_low - dangerzone_size
	end

	-- in dangerzone, we should check if the next session already exist.
	local function checkconflict(session)
		if session == nil then
			return
		end
		local next_session = session + 1
		if next_session > dangerzone_up then
			-- leave dangerzone
			reset_dangerzone(session)
			assert(next_session > dangerzone_up)
			set_checkrewind()
		else
			while true do
				if not dangerzone[next_session] then
					break
				end
				if not session_id_coroutine[next_session] then
					reset_dangerzone(session)
					break
				end
				-- skip the session already exist.
				next_session = c.genid() + 1
			end
		end
		-- session will rewind after 0x7fffffff
		if next_session == 0x80000000 and dangerzone[1] then
			assert(c.genid() == 1)
			return checkconflict(1)
		end
	end

	local function auxsend_checkconflict(addr, proto, msg, sz)
		local session = csend(addr, proto, nil, msg, sz)
		checkconflict(session)
		return session
	end

	local function auxtimeout_checkconflict(timeout)
		local session = cintcommand("TIMEOUT", timeout)
		checkconflict(session)
		return session
	end

	local function auxsend_checkrewind(addr, proto, msg, sz)
		local session = csend(addr, proto, nil, msg, sz)
		if session and session > dangerzone_low and session <= dangerzone_up then
			-- enter dangerzone
			set_checkconflict(session)
		end
		return session
	end

	local function auxtimeout_checkrewind(timeout)
		local session = cintcommand("TIMEOUT", timeout)
		if session and session > dangerzone_low and session <= dangerzone_up then
			-- enter dangerzone
			set_checkconflict(session)
		end
		return session
	end

	set_checkrewind = function()
		auxsend = auxsend_checkrewind
		auxtimeout = auxtimeout_checkrewind
	end

	set_checkconflict = function(session)
		reset_dangerzone(session)
		auxsend = auxsend_checkconflict
		auxtimeout = auxtimeout_checkconflict
	end

	-- in safezone at the beginning
	set_checkrewind()
end

do ---- request/select
	local function send_requests(self)
		local sessions = {}
		self._sessions = sessions
		local request_n = 0
		local err
		for i = 1, #self do
			local req = self[i]
			local addr = req[1]
			local p = proto[req[2]]
			assert(p.unpack)
			local tag = session_coroutine_tracetag[running_thread]
			if tag then
				c.trace(tag, "call", 4)
				c.send(addr, fengnet.PTYPE_TRACE, 0, tag)
			end
			local session = auxsend(addr, p.id , p.pack(tunpack(req, 3, req.n)))
			if session == nil then
				err = err or {}
				err[#err+1] = req
			else
				sessions[session] = req
				watching_session[session] = addr
				session_id_coroutine[session] = self._thread
				request_n = request_n + 1
			end
		end
		self._request = request_n
		return err
	end

	local function request_thread(self)
		while true do
			local succ, msg, sz, session = coroutine_yield "SUSPEND"
			if session == self._timeout then
				self._timeout = nil
				self.timeout = true
			else
				watching_session[session] = nil
				local req = self._sessions[session]
				local p = proto[req[2]]
				if succ then
					self._resp[session] = tpack( p.unpack(msg, sz) )
				else
					self._resp[session] = false
				end
			end
			fengnet.wakeup(self)
		end
	end

	local function request_iter(self)
		return function()
			if self._error then
				-- invalid address
				local e = tremove(self._error)
				if e then
					return e
				end
				self._error = nil
			end
			local session, resp = next(self._resp)
			if session == nil then
				if self._request == 0 then
					return
				end
				if self.timeout then
					return
				end
				fengnet.wait(self)
				if self.timeout then
					return
				end
				session, resp = next(self._resp)
			end

			self._request = self._request - 1
			local req = self._sessions[session]
			self._resp[session] = nil
			self._sessions[session] = nil
			return req, resp
		end
	end

	local request_meta = {}	; request_meta.__index = request_meta

	function request_meta:add(obj)
		assert(type(obj) == "table" and not self._thread)
		self[#self+1] = obj
		return self
	end

	request_meta.__call = request_meta.add

	function request_meta:close()
		if self._request > 0 then
			local resp = self._resp
			for session, req in pairs(self._sessions) do
				if not resp[session] then
					session_id_coroutine[session] = "BREAK"
					watching_session[session] = nil
				end
			end
			self._request = 0
		end
		if self._timeout then
			session_id_coroutine[self._timeout] = "BREAK"
			self._timeout = nil
		end
	end

	request_meta.__close = request_meta.close

	function request_meta:select(timeout)
		assert(self._thread == nil)
		self._thread = coroutine_create(request_thread)
		self._error = send_requests(self)
		self._resp = {}
		if timeout then
			self._timeout = auxtimeout(timeout)
			session_id_coroutine[self._timeout] = self._thread
		end

		local running = running_thread
		coroutine_resume(self._thread, self)
		running_thread = running
		return request_iter(self), nil, nil, self
	end

	function fengnet.request(obj)
		local ret = setmetatable({}, request_meta)
		if obj then
			return ret(obj)
		end
		return ret
	end
end

-- suspend is function
local suspend

----- monitor exit

local function dispatch_error_queue()
	local session = tremove(error_queue,1)
	if session then
		local co = session_id_coroutine[session]
		session_id_coroutine[session] = nil
		return suspend(co, coroutine_resume(co, false, nil, nil, session))
	end
end

local function _error_dispatch(error_session, error_source)
	fengnet.ignoreret()	-- don't return for error
	if error_session == 0 then
		-- error_source is down, clear unreponse set
		for resp, address in pairs(unresponse) do
			if error_source == address then
				unresponse[resp] = nil
			end
		end
		for session, srv in pairs(watching_session) do
			if srv == error_source then
				tinsert(error_queue, session)
			end
		end
	else
		-- capture an error for error_session
		if watching_session[error_session] then
			tinsert(error_queue, error_session)
		end
	end
end

-- coroutine reuse

local coroutine_pool = setmetatable({}, { __mode = "kv" })

local function co_create(f)
	local co = tremove(coroutine_pool)
	if co == nil then
		co = coroutine_create(function(...)
			f(...)
			while true do
				local session = session_coroutine_id[co]
				if session and session ~= 0 then
					local source = debug.getinfo(f,"S")
					fengnet.error(string.format("Maybe forgot response session %s from %s : %s:%d",
						session,
						fengnet.address(session_coroutine_address[co]),
						source.source, source.linedefined))
				end
				-- coroutine exit
				local tag = session_coroutine_tracetag[co]
				if tag ~= nil then
					if tag then c.trace(tag, "end")	end
					session_coroutine_tracetag[co] = nil
				end
				local address = session_coroutine_address[co]
				if address then
					session_coroutine_id[co] = nil
					session_coroutine_address[co] = nil
				end

				-- recycle co into pool
				f = nil
				coroutine_pool[#coroutine_pool+1] = co
				-- recv new main function f
				f = coroutine_yield "SUSPEND"
				f(coroutine_yield())
			end
		end)
	else
		-- pass the main function f to coroutine, and restore running thread
		local running = running_thread
		coroutine_resume(co, f)
		running_thread = running
	end
	return co
end

local function dispatch_wakeup()
	while true do
		local token = tremove(wakeup_queue,1)
		if token then
			local session = sleep_session[token]
			if session then
				local co = session_id_coroutine[session]
				local tag = session_coroutine_tracetag[co]
				if tag then c.trace(tag, "resume") end
				session_id_coroutine[session] = "BREAK"
				return suspend(co, coroutine_resume(co, false, "BREAK", nil, session))
			end
		else
			break
		end
	end
	return dispatch_error_queue()
end

-- suspend is local function
function suspend(co, result, command)
	if not result then
		local session = session_coroutine_id[co]
		if session then -- coroutine may fork by others (session is nil)
			local addr = session_coroutine_address[co]
			if session ~= 0 then
				-- only call response error
				local tag = session_coroutine_tracetag[co]
				if tag then c.trace(tag, "error") end
				c.send(addr, fengnet.PTYPE_ERROR, session, "")
			end
			session_coroutine_id[co] = nil
		end
		session_coroutine_address[co] = nil
		session_coroutine_tracetag[co] = nil
		fengnet.fork(function() end)	-- trigger command "SUSPEND"
		local tb = traceback(co,tostring(command))
		coroutine.close(co)
		error(tb)
	end
	if command == "SUSPEND" then
		return dispatch_wakeup()
	elseif command == "QUIT" then
		coroutine.close(co)
		-- service exit
		return
	elseif command == "USER" then
		-- See fengnet.coutine for detail
		error("Call fengnet.coroutine.yield out of fengnet.coroutine.resume\n" .. traceback(co))
	elseif command == nil then
		-- debug trace
		return
	else
		error("Unknown command : " .. command .. "\n" .. traceback(co))
	end
end

local co_create_for_timeout
local timeout_traceback

function fengnet.trace_timeout(on)
	local function trace_coroutine(func, ti)
		local co
		co = co_create(function()
			timeout_traceback[co] = nil
			func()
		end)
		local info = string.format("TIMER %d+%d : ", fengnet.now(), ti)
		timeout_traceback[co] = traceback(info, 3)
		return co
	end
	if on then
		timeout_traceback = timeout_traceback or {}
		co_create_for_timeout = trace_coroutine
	else
		timeout_traceback = nil
		co_create_for_timeout = co_create
	end
end

fengnet.trace_timeout(false)	-- turn off by default

function fengnet.timeout(ti, func)
	local session = auxtimeout(ti)
	assert(session)
	local co = co_create_for_timeout(func, ti)
	assert(session_id_coroutine[session] == nil)
	session_id_coroutine[session] = co
	return co	-- for debug
end

local function suspend_sleep(session, token)
	local tag = session_coroutine_tracetag[running_thread]
	if tag then c.trace(tag, "sleep", 2) end
	session_id_coroutine[session] = running_thread
	assert(sleep_session[token] == nil, "token duplicative")
	sleep_session[token] = session

	return coroutine_yield "SUSPEND"
end

function fengnet.sleep(ti, token)
	local session = auxtimeout(ti)
	assert(session)
	token = token or coroutine.running()
	local succ, ret = suspend_sleep(session, token)
	sleep_session[token] = nil
	if succ then
		return
	end
	if ret == "BREAK" then
		return "BREAK"
	else
		error(ret)
	end
end

function fengnet.yield()
	return fengnet.sleep(0)
end

function fengnet.wait(token)
	local session = c.genid()
	token = token or coroutine.running()
	suspend_sleep(session, token)
	sleep_session[token] = nil
	session_id_coroutine[session] = nil
end

function fengnet.killthread(thread)
	local session
	-- find session
	if type(thread) == "string" then
		for k,v in pairs(session_id_coroutine) do
			local thread_string = tostring(v)
			if thread_string:find(thread) then
				session = k
				break
			end
		end
	else
		local t = fork_queue.t
		for i = fork_queue.h, t do
			if fork_queue[i] == thread then
				table.move(fork_queue, i+1, t, i)
				fork_queue[t] = nil
				fork_queue.t = t - 1
				return thread
			end
		end
		for k,v in pairs(session_id_coroutine) do
			if v == thread then
				session = k
				break
			end
		end
	end
	local co = session_id_coroutine[session]
	if co == nil then
		return
	end
	local addr = session_coroutine_address[co]
	if addr then
		session_coroutine_address[co] = nil
		session_coroutine_tracetag[co] = nil
		local session = session_coroutine_id[co]
		if session > 0 then
			c.send(addr, fengnet.PTYPE_ERROR, session, "")
		end
		session_coroutine_id[co] = nil
	end
	if watching_session[session] then
		session_id_coroutine[session] = "BREAK"
		watching_session[session] = nil
	else
		session_id_coroutine[session] = nil
	end
	for k,v in pairs(sleep_session) do
		if v == session then
			sleep_session[k] = nil
			break
		end
	end
	coroutine.close(co)
	return co
end

function fengnet.self()
	return c.addresscommand "REG"
end

function fengnet.localname(name)
	return c.addresscommand("QUERY", name)
end

fengnet.now = c.now
fengnet.hpc = c.hpc	-- high performance counter

local traceid = 0
function fengnet.trace(info)
	fengnet.error("TRACE", session_coroutine_tracetag[running_thread])
	if session_coroutine_tracetag[running_thread] == false then
		-- force off trace log
		return
	end
	traceid = traceid + 1

	local tag = string.format(":%08x-%d",fengnet.self(), traceid)
	session_coroutine_tracetag[running_thread] = tag
	if info then
		c.trace(tag, "trace " .. info)
	else
		c.trace(tag, "trace")
	end
end

function fengnet.tracetag()
	return session_coroutine_tracetag[running_thread]
end

local starttime

function fengnet.starttime()
	if not starttime then
		starttime = c.intcommand("STARTTIME")
	end
	return starttime
end

function fengnet.time()
	return fengnet.now()/100 + (starttime or fengnet.starttime())
end

function fengnet.exit()
	fork_queue = { h = 1, t = 0 }	-- no fork coroutine can be execute after fengnet.exit
	fengnet.send(".launcher","lua","REMOVE",fengnet.self(), false)
	-- report the sources that call me
	for co, session in pairs(session_coroutine_id) do
		local address = session_coroutine_address[co]
		if session~=0 and address then
			c.send(address, fengnet.PTYPE_ERROR, session, "")
		end
	end
	for session, co in pairs(session_id_coroutine) do
		if type(co) == "thread" and co ~= running_thread then
			coroutine.close(co)
		end
	end
	for resp in pairs(unresponse) do
		resp(false)
	end
	-- report the sources I call but haven't return
	local tmp = {}
	for session, address in pairs(watching_session) do
		tmp[address] = true
	end
	for address in pairs(tmp) do
		c.send(address, fengnet.PTYPE_ERROR, 0, "")
	end
	c.callback(function() end)
	c.command("EXIT")
	-- quit service
	coroutine_yield "QUIT"
end

function fengnet.getenv(key)
	return (c.command("GETENV",key))
end

function fengnet.setenv(key, value)
	assert(c.command("GETENV",key) == nil, "Can't setenv exist key : " .. key)
	c.command("SETENV",key .. " " ..value)
end

function fengnet.send(addr, typename, ...)
	local p = proto[typename]
	return c.send(addr, p.id, 0 , p.pack(...))
end

function fengnet.rawsend(addr, typename, msg, sz)
	local p = proto[typename]
	return c.send(addr, p.id, 0 , msg, sz)
end

fengnet.genid = assert(c.genid)

fengnet.redirect = function(dest,source,typename,...)
	return c.redirect(dest, source, proto[typename].id, ...)
end

fengnet.pack = assert(c.pack)
fengnet.packstring = assert(c.packstring)
fengnet.unpack = assert(c.unpack)
fengnet.tostring = assert(c.tostring)
fengnet.trash = assert(c.trash)

local function yield_call(service, session)
	watching_session[session] = service
	session_id_coroutine[session] = running_thread
	local succ, msg, sz = coroutine_yield "SUSPEND"
	watching_session[session] = nil
	if not succ then
		error "call failed"
	end
	return msg,sz
end

function fengnet.call(addr, typename, ...)
	local tag = session_coroutine_tracetag[running_thread]
	if tag then
		c.trace(tag, "call", 2)
		c.send(addr, fengnet.PTYPE_TRACE, 0, tag)
	end

	local p = proto[typename]
	local session = auxsend(addr, p.id , p.pack(...))
	if session == nil then
		error("call to invalid address " .. fengnet.address(addr))
	end
	return p.unpack(yield_call(addr, session))
end

function fengnet.rawcall(addr, typename, msg, sz)
	local tag = session_coroutine_tracetag[running_thread]
	if tag then
		c.trace(tag, "call", 2)
		c.send(addr, fengnet.PTYPE_TRACE, 0, tag)
	end
	local p = proto[typename]
	local session = assert(auxsend(addr, p.id , msg, sz), "call to invalid address")
	return yield_call(addr, session)
end

function fengnet.tracecall(tag, addr, typename, msg, sz)
	c.trace(tag, "tracecall begin")
	c.send(addr, fengnet.PTYPE_TRACE, 0, tag)
	local p = proto[typename]
	local session = assert(auxsend(addr, p.id , msg, sz), "call to invalid address")
	local msg, sz = yield_call(addr, session)
	c.trace(tag, "tracecall end")
	return msg, sz
end

function fengnet.ret(msg, sz)
	msg = msg or ""
	local tag = session_coroutine_tracetag[running_thread]
	if tag then c.trace(tag, "response") end
	local co_session = session_coroutine_id[running_thread]
	if co_session == nil then
		error "No session"
	end
	session_coroutine_id[running_thread] = nil
	if co_session == 0 then
		if sz ~= nil then
			c.trash(msg, sz)
		end
		return false	-- send don't need ret
	end
	local co_address = session_coroutine_address[running_thread]
	local ret = c.send(co_address, fengnet.PTYPE_RESPONSE, co_session, msg, sz)
	if ret then
		return true
	elseif ret == false then
		-- If the package is too large, returns false. so we should report error back
		c.send(co_address, fengnet.PTYPE_ERROR, co_session, "")
	end
	return false
end

function fengnet.context()
	local co_session = session_coroutine_id[running_thread]
	local co_address = session_coroutine_address[running_thread]
	return co_session, co_address
end

function fengnet.ignoreret()
	-- We use session for other uses
	session_coroutine_id[running_thread] = nil
end

function fengnet.response(pack)
	pack = pack or fengnet.pack

	local co_session = assert(session_coroutine_id[running_thread], "no session")
	session_coroutine_id[running_thread] = nil
	local co_address = session_coroutine_address[running_thread]
	if co_session == 0 then
		--  do not response when session == 0 (send)
		return function() end
	end
	local function response(ok, ...)
		if ok == "TEST" then
			return unresponse[response] ~= nil
		end
		if not pack then
			error "Can't response more than once"
		end

		local ret
		if unresponse[response] then
			if ok then
				ret = c.send(co_address, fengnet.PTYPE_RESPONSE, co_session, pack(...))
				if ret == false then
					-- If the package is too large, returns false. so we should report error back
					c.send(co_address, fengnet.PTYPE_ERROR, co_session, "")
				end
			else
				ret = c.send(co_address, fengnet.PTYPE_ERROR, co_session, "")
			end
			unresponse[response] = nil
			ret = ret ~= nil
		else
			ret = false
		end
		pack = nil
		return ret
	end
	unresponse[response] = co_address

	return response
end

function fengnet.retpack(...)
	return fengnet.ret(fengnet.pack(...))
end

function fengnet.wakeup(token)
	if sleep_session[token] then
		tinsert(wakeup_queue, token)
		return true
	end
end

function fengnet.dispatch(typename, func)
	local p = proto[typename]
	if func then
		local ret = p.dispatch
		p.dispatch = func
		return ret
	else
		return p and p.dispatch
	end
end

local function unknown_request(session, address, msg, sz, prototype)
	fengnet.error(string.format("Unknown request (%s): %s", prototype, c.tostring(msg,sz)))
	error(string.format("Unknown session : %d from %x", session, address))
end

function fengnet.dispatch_unknown_request(unknown)
	local prev = unknown_request
	unknown_request = unknown
	return prev
end

local function unknown_response(session, address, msg, sz)
	fengnet.error(string.format("Response message : %s" , c.tostring(msg,sz)))
	error(string.format("Unknown session : %d from %x", session, address))
end

function fengnet.dispatch_unknown_response(unknown)
	local prev = unknown_response
	unknown_response = unknown
	return prev
end

function fengnet.fork(func,...)
	local n = select("#", ...)
	local co
	if n == 0 then
		co = co_create(func)
	else
		local args = { ... }
		co = co_create(function() func(table.unpack(args,1,n)) end)
	end
	local t = fork_queue.t + 1
	fork_queue.t = t
	fork_queue[t] = co
	return co
end

local trace_source = {}

local function raw_dispatch_message(prototype, msg, sz, session, source)
	-- fengnet.PTYPE_RESPONSE = 1, read fengnet.h
	if prototype == 1 then
		local co = session_id_coroutine[session]
		if co == "BREAK" then
			session_id_coroutine[session] = nil
		elseif co == nil then
			unknown_response(session, source, msg, sz)
		else
			local tag = session_coroutine_tracetag[co]
			if tag then c.trace(tag, "resume") end
			session_id_coroutine[session] = nil
			suspend(co, coroutine_resume(co, true, msg, sz, session))
		end
	else
		local p = proto[prototype]
		if p == nil then
			if prototype == fengnet.PTYPE_TRACE then
				-- trace next request
				trace_source[source] = c.tostring(msg,sz)
			elseif session ~= 0 then
				c.send(source, fengnet.PTYPE_ERROR, session, "")
			else
				unknown_request(session, source, msg, sz, prototype)
			end
			return
		end

		local f = p.dispatch
		if f then
			local co = co_create(f)
			session_coroutine_id[co] = session
			session_coroutine_address[co] = source
			local traceflag = p.trace
			if traceflag == false then
				-- force off
				trace_source[source] = nil
				session_coroutine_tracetag[co] = false
			else
				local tag = trace_source[source]
				if tag then
					trace_source[source] = nil
					c.trace(tag, "request")
					session_coroutine_tracetag[co] = tag
				elseif traceflag then
					-- set running_thread for trace
					running_thread = co
					fengnet.trace()
				end
			end
			suspend(co, coroutine_resume(co, session,source, p.unpack(msg,sz)))
		else
			trace_source[source] = nil
			if session ~= 0 then
				c.send(source, fengnet.PTYPE_ERROR, session, "")
			else
				unknown_request(session, source, msg, sz, proto[prototype].name)
			end
		end
	end
end

function fengnet.dispatch_message(...)
	local succ, err = pcall(raw_dispatch_message,...)
	while true do
		if fork_queue.h > fork_queue.t then
			-- queue is empty
			fork_queue.h = 1
			fork_queue.t = 0
			break
		end
		-- pop queue
		local h = fork_queue.h
		local co = fork_queue[h]
		fork_queue[h] = nil
		fork_queue.h = h + 1

		local fork_succ, fork_err = pcall(suspend,co,coroutine_resume(co))
		if not fork_succ then
			if succ then
				succ = false
				err = tostring(fork_err)
			else
				err = tostring(err) .. "\n" .. tostring(fork_err)
			end
		end
	end
	assert(succ, tostring(err))
end

function fengnet.newservice(name, ...)
	return fengnet.call(".launcher", "lua" , "LAUNCH", "snlua", name, ...)
end

function fengnet.uniqueservice(global, ...)
	if global == true then
		return assert(fengnet.call(".service", "lua", "GLAUNCH", ...))
	else
		return assert(fengnet.call(".service", "lua", "LAUNCH", global, ...))
	end
end

function fengnet.queryservice(global, ...)
	if global == true then
		return assert(fengnet.call(".service", "lua", "GQUERY", ...))
	else
		return assert(fengnet.call(".service", "lua", "QUERY", global, ...))
	end
end

function fengnet.address(addr)
	if type(addr) == "number" then
		return string.format(":%08x",addr)
	else
		return tostring(addr)
	end
end

function fengnet.harbor(addr)
	return c.harbor(addr)
end

fengnet.error = c.error
fengnet.tracelog = c.trace

-- true: force on
-- false: force off
-- nil: optional (use fengnet.trace() to trace one message)
function fengnet.traceproto(prototype, flag)
	local p = assert(proto[prototype])
	p.trace = flag
end

----- register protocol
do
	local REG = fengnet.register_protocol

	REG {
		name = "lua",
		id = fengnet.PTYPE_LUA,
		pack = fengnet.pack,
		unpack = fengnet.unpack,
	}

	REG {
		name = "response",
		id = fengnet.PTYPE_RESPONSE,
	}

	REG {
		name = "error",
		id = fengnet.PTYPE_ERROR,
		unpack = function(...) return ... end,
		dispatch = _error_dispatch,
	}
end

fengnet.init = fengnet_require.init
-- fengnet.pcall is deprecated, use pcall directly
fengnet.pcall = pcall

function fengnet.init_service(start)
	local function main()
		fengnet_require.init_all()
		start()
	end
	local ok, err = xpcall(main, traceback)
	if not ok then
		fengnet.error("init service failed: " .. tostring(err))
		fengnet.send(".launcher","lua", "ERROR")
		fengnet.exit()
	else
		fengnet.send(".launcher","lua", "LAUNCHOK")
	end
end

function fengnet.start(start_func)
	c.callback(fengnet.dispatch_message)
	init_thread = fengnet.timeout(0, function()
		fengnet.init_service(start_func)
		init_thread = nil
	end)
end

function fengnet.endless()
	return (c.intcommand("STAT", "endless") == 1)
end

function fengnet.mqlen()
	return c.intcommand("STAT", "mqlen")
end

function fengnet.stat(what)
	return c.intcommand("STAT", what)
end

function fengnet.task(ret)
	if ret == nil then
		local t = 0
		for session,co in pairs(session_id_coroutine) do
			if co ~= "BREAK" then
				t = t + 1
			end
		end
		return t
	end
	if ret == "init" then
		if init_thread then
			return traceback(init_thread)
		else
			return
		end
	end
	local tt = type(ret)
	if tt == "table" then
		for session,co in pairs(session_id_coroutine) do
			local key = string.format("%s session: %d", tostring(co), session)
			if co == "BREAK" then
				ret[key] = "BREAK"
			elseif timeout_traceback and timeout_traceback[co] then
				ret[key] = timeout_traceback[co]
			else
				ret[key] = traceback(co)
			end
		end
		return
	elseif tt == "number" then
		local co = session_id_coroutine[ret]
		if co then
			if co == "BREAK" then
				return "BREAK"
			else
				return traceback(co)
			end
		else
			return "No session"
		end
	elseif tt == "thread" then
		for session, co in pairs(session_id_coroutine) do
			if co == ret then
				return session
			end
		end
		return
	end
end

function fengnet.uniqtask()
	local stacks = {}
	for session, co in pairs(session_id_coroutine) do
		local stack = traceback(co)
		local info = stacks[stack] or {count = 0, sessions = {}}
		info.count = info.count + 1
		if info.count < 10 then
			info.sessions[#info.sessions+1] = session
		end
		stacks[stack] = info
	end
	local ret = {}
	for stack, info in pairs(stacks) do
		local count = info.count
		local sessions = table.concat(info.sessions, ",")
		if count > 10 then
			sessions = sessions .. "..."
		end
		local head_line = string.format("%d\tsessions:[%s]\n", count, sessions)
		ret[head_line] = stack
	end
	return ret
end

function fengnet.term(service)
	return _error_dispatch(0, service)
end

function fengnet.memlimit(bytes)
	debug.getregistry().memlimit = bytes
	fengnet.memlimit = nil	-- set only once
end

-- Inject internal debug framework
local debug = require "fengnet.debug"
debug.init(fengnet, {
	dispatch = fengnet.dispatch_message,
	suspend = suspend,
	resume = coroutine_resume,
})

return fengnet

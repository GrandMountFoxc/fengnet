local fengnet = require "fengnet"
local socket = require "fengnet.socket"
local cluster = require "fengnet.cluster.core"
local ignoreret = fengnet.ignoreret

local clusterd, gate, fd = ...
clusterd = tonumber(clusterd)
gate = tonumber(gate)
fd = tonumber(fd)

local large_request = {}
local inquery_name = {}

local register_name_mt = { __index =
	function(self, name)
		local waitco = inquery_name[name]
		if waitco then
			local co=coroutine.running()
			table.insert(waitco, co)
			fengnet.wait(co)
			return rawget(self, name)
		else
			waitco = {}
			inquery_name[name] = waitco
			local addr = fengnet.call(clusterd, "lua", "queryname", name:sub(2))	-- name must be '@xxxx'
			if addr then
				self[name] = addr
			end
			inquery_name[name] = nil
			for _, co in ipairs(waitco) do
				fengnet.wakeup(co)
			end
			return addr
		end
	end
}

local function new_register_name()
	return setmetatable({}, register_name_mt)
end

local register_name = new_register_name()

local tracetag

local function dispatch_request(_,_,addr, session, msg, sz, padding, is_push)
	ignoreret()	-- session is fd, don't call fengnet.ret
	if session == nil then
		-- trace
		tracetag = addr
		return
	end
	if padding then
		local req = large_request[session] or { addr = addr , is_push = is_push, tracetag = tracetag }
		tracetag = nil
		large_request[session] = req
		cluster.append(req, msg, sz)
		return
	else
		local req = large_request[session]
		if req then
			tracetag = req.tracetag
			large_request[session] = nil
			cluster.append(req, msg, sz)
			msg,sz = cluster.concat(req)
			addr = req.addr
			is_push = req.is_push
		end
		if not msg then
			tracetag = nil
			local response = cluster.packresponse(session, false, "Invalid large req")
			socket.write(fd, response)
			return
		end
	end
	local ok, response
	if addr == 0 then
		local name = fengnet.unpack(msg, sz)
		fengnet.trash(msg, sz)
		local addr = register_name["@" .. name]
		if addr then
			ok = true
			msg = fengnet.packstring(addr)
		else
			ok = false
			msg = "name not found"
		end
		sz = nil
	else
		if cluster.isname(addr) then
			addr = register_name[addr]
		end
		if addr then
			if is_push then
				fengnet.rawsend(addr, "lua", msg, sz)
				return	-- no response
			else
				if tracetag then
					ok , msg, sz = pcall(fengnet.tracecall, tracetag, addr, "lua", msg, sz)
					tracetag = nil
				else
					ok , msg, sz = pcall(fengnet.rawcall, addr, "lua", msg, sz)
				end
			end
		else
			ok = false
			msg = "Invalid name"
		end
	end
	if ok then
		response = cluster.packresponse(session, true, msg, sz)
		if type(response) == "table" then
			for _, v in ipairs(response) do
				socket.lwrite(fd, v)
			end
		else
			socket.write(fd, response)
		end
	else
		response = cluster.packresponse(session, false, msg)
		socket.write(fd, response)
	end
end

fengnet.start(function()
	fengnet.register_protocol {
		name = "client",
		id = fengnet.PTYPE_CLIENT,
		unpack = cluster.unpackrequest,
		dispatch = dispatch_request,
	}
	-- fd can write, but don't read fd, the data package will forward from gate though client protocol.
	fengnet.call(gate, "lua", "forward", fd)

	fengnet.dispatch("lua", function(_,source, cmd, ...)
		if cmd == "exit" then
			socket.close_fd(fd)
			fengnet.exit()
		elseif cmd == "namechange" then
			register_name = new_register_name()
		else
			fengnet.error(string.format("Invalid command %s from %s", cmd, fengnet.address(source)))
		end
	end)
end)

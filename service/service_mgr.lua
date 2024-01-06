local fengnet = require "fengnet"
require "fengnet.manager"	-- import fengnet.register
local snax = require "fengnet.snax"

local cmd = {}
local service = {}

local function request(name, func, ...)
	local ok, handle = pcall(func, ...)
	local s = service[name]
	assert(type(s) == "table")
	if ok then
		service[name] = handle
	else
		service[name] = tostring(handle)
	end

	for _,v in ipairs(s) do
		fengnet.wakeup(v.co)
	end

	if ok then
		return handle
	else
		error(tostring(handle))
	end
end

local function waitfor(name , func, ...)
	local s = service[name]
	if type(s) == "number" then
		return s
	end
	local co = coroutine.running()

	if s == nil then
		s = {}
		service[name] = s
	elseif type(s) == "string" then
		error(s)
	end

	assert(type(s) == "table")

	local session, source = fengnet.context()

	if s.launch == nil and func then
		s.launch = {
			session = session,
			source = source,
			co = co,
		}
		return request(name, func, ...)
	end

	table.insert(s, {
		co = co,
		session = session,
		source = source,
	})
	fengnet.wait()
	s = service[name]
	if type(s) == "string" then
		error(s)
	end
	assert(type(s) == "number")
	return s
end

local function read_name(service_name)
	if string.byte(service_name) == 64 then -- '@'
		return string.sub(service_name , 2)
	else
		return service_name
	end
end

function cmd.LAUNCH(service_name, subname, ...)
	local realname = read_name(service_name)

	if realname == "snaxd" then
		return waitfor(service_name.."."..subname, snax.rawnewservice, subname, ...)
	else
		return waitfor(service_name, fengnet.newservice, realname, subname, ...)
	end
end

function cmd.QUERY(service_name, subname)
	local realname = read_name(service_name)

	if realname == "snaxd" then
		return waitfor(service_name.."."..subname)
	else
		return waitfor(service_name)
	end
end

local function list_service()
	local result = {}
	for k,v in pairs(service) do
		if type(v) == "string" then
			v = "Error: " .. v
		elseif type(v) == "table" then
			local querying = {}
			if v.launch then
				local session = fengnet.task(v.launch.co)
				local launching_address = fengnet.call(".launcher", "lua", "QUERY", session)
				if launching_address then
					table.insert(querying, "Init as " .. fengnet.address(launching_address))
					table.insert(querying,  fengnet.call(launching_address, "debug", "TASK", "init"))
					table.insert(querying, "Launching from " .. fengnet.address(v.launch.source))
					table.insert(querying, fengnet.call(v.launch.source, "debug", "TASK", v.launch.session))
				end
			end
			if #v > 0 then
				table.insert(querying , "Querying:" )
				for _, detail in ipairs(v) do
					table.insert(querying, fengnet.address(detail.source) .. " " .. tostring(fengnet.call(detail.source, "debug", "TASK", detail.session)))
				end
			end
			v = table.concat(querying, "\n")
		else
			v = fengnet.address(v)
		end

		result[k] = v
	end

	return result
end


local function register_global()
	function cmd.GLAUNCH(name, ...)
		local global_name = "@" .. name
		return cmd.LAUNCH(global_name, ...)
	end

	function cmd.GQUERY(name, ...)
		local global_name = "@" .. name
		return cmd.QUERY(global_name, ...)
	end

	local mgr = {}

	function cmd.REPORT(m)
		mgr[m] = true
	end

	local function add_list(all, m)
		local harbor = "@" .. fengnet.harbor(m)
		local result = fengnet.call(m, "lua", "LIST")
		for k,v in pairs(result) do
			all[k .. harbor] = v
		end
	end

	function cmd.LIST()
		local result = {}
		for k in pairs(mgr) do
			pcall(add_list, result, k)
		end
		local l = list_service()
		for k, v in pairs(l) do
			result[k] = v
		end
		return result
	end
end

local function register_local()
	local function waitfor_remote(cmd, name, ...)
		local global_name = "@" .. name
		local local_name
		if name == "snaxd" then
			local_name = global_name .. "." .. (...)
		else
			local_name = global_name
		end
		return waitfor(local_name, fengnet.call, "SERVICE", "lua", cmd, global_name, ...)
	end

	function cmd.GLAUNCH(...)
		return waitfor_remote("LAUNCH", ...)
	end

	function cmd.GQUERY(...)
		return waitfor_remote("QUERY", ...)
	end

	function cmd.LIST()
		return list_service()
	end

	fengnet.call("SERVICE", "lua", "REPORT", fengnet.self())
end

fengnet.start(function()
	fengnet.dispatch("lua", function(session, address, command, ...)
		local f = cmd[command]
		if f == nil then
			fengnet.ret(fengnet.pack(nil, "Invalid command " .. command))
			return
		end

		local ok, r = pcall(f, ...)

		if ok then
			fengnet.ret(fengnet.pack(r))
		else
			fengnet.ret(fengnet.pack(nil, r))
		end
	end)
	local handle = fengnet.localname ".service"
	if  handle then
		fengnet.error(".service is already register by ", fengnet.address(handle))
		fengnet.exit()
	else
		fengnet.register(".service")
	end
	if fengnet.getenv "standalone" then
		fengnet.register("SERVICE")
		register_global()
	else
		register_local()
	end
end)

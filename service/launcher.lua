local fengnet = require "fengnet"
local core = require "libfengnet.core"
require "fengnet.manager"	-- import manager apis
local string = string

local services = {}
local command = {}
local instance = {} -- for confirm (function command.LAUNCH / command.ERROR / command.LAUNCHOK)
local launch_session = {} -- for command.QUERY, service_address -> session

local function handle_to_address(handle)
	return tonumber("0x" .. string.sub(handle , 2))
end

local NORET = {}

function command.LIST()
	local list = {}
	for k,v in pairs(services) do
		list[fengnet.address(k)] = v
	end
	return list
end

local function list_srv(ti, fmt_func, ...)
	local list = {}
	local sessions = {}
	local req = fengnet.request()
	for addr in pairs(services) do
		local r = { addr, "debug", ... }
		req:add(r)
		sessions[r] = addr
	end
	for req, resp in req:select(ti) do
		local addr = req[1]
		if resp then
			local stat = resp[1]
			list[fengnet.address(addr)] = fmt_func(stat, addr)
		else
			list[fengnet.address(addr)] = fmt_func("ERROR", addr)
		end
		sessions[req] = nil
	end
	for session, addr in pairs(sessions) do
		list[fengnet.address(addr)] = fmt_func("TIMEOUT", addr)
	end
	return list
end

function command.STAT(addr, ti)
	return list_srv(ti, function(v) return v end, "STAT")
end

function command.KILL(_, handle)
	fengnet.kill(handle)
	local ret = { [fengnet.address(handle)] = tostring(services[handle]) }
	services[handle] = nil
	return ret
end

function command.MEM(addr, ti)
	return list_srv(ti, function(kb, addr)
		local v = services[addr]
		if type(kb) == "string" then
			return string.format("%s (%s)", kb, v)
		else
			return string.format("%.2f Kb (%s)",kb,v)
		end
	end, "MEM")
end

function command.GC(addr, ti)
	for k,v in pairs(services) do
		fengnet.send(k,"debug","GC")
	end
	return command.MEM(addr, ti)
end

function command.REMOVE(_, handle, kill)
	services[handle] = nil
	local response = instance[handle]
	if response then
		-- instance is dead
		response(not kill)	-- return nil to caller of newservice, when kill == false
		instance[handle] = nil
		launch_session[handle] = nil
	end

	-- don't return (fengnet.ret) because the handle may exit
	return NORET
end

local function launch_service(service, ...)
	local param = table.concat({...}, " ")
	local inst = fengnet.launch(service, param)
	local session = fengnet.context()
	local response = fengnet.response()
	if inst then
		services[inst] = service .. " " .. param
		instance[inst] = response
		launch_session[inst] = session
	else
		response(false)
		return
	end
	return inst
end

function command.LAUNCH(_, service, ...)
	launch_service(service, ...)
	return NORET
end

function command.LOGLAUNCH(_, service, ...)
	local inst = launch_service(service, ...)
	if inst then
		core.command("LOGON", fengnet.address(inst))
	end
	return NORET
end

function command.ERROR(address)
	-- see serivce-src/service_lua.c
	-- init failed
	local response = instance[address]
	if response then
		response(false)
		launch_session[address] = nil
		instance[address] = nil
	end
	services[address] = nil
	return NORET
end

function command.LAUNCHOK(address)
	-- init notice
	local response = instance[address]
	if response then
		response(true, address)
		instance[address] = nil
		launch_session[address] = nil
	end

	return NORET
end

function command.QUERY(_, request_session)
	for address, session in pairs(launch_session) do
		if session == request_session then
			return address
		end
	end
end

-- for historical reasons, launcher support text command (for C service)

fengnet.register_protocol {
	name = "text",
	id = fengnet.PTYPE_TEXT,
	unpack = fengnet.tostring,
	dispatch = function(session, address , cmd)
		if cmd == "" then
			command.LAUNCHOK(address)
		elseif cmd == "ERROR" then
			command.ERROR(address)
		else
			error ("Invalid text command " .. cmd)
		end
	end,
}

fengnet.dispatch("lua", function(session, address, cmd , ...)
	cmd = string.upper(cmd)
	local f = command[cmd]
	if f then
		local ret = f(address, ...)
		if ret ~= NORET then
			fengnet.ret(fengnet.pack(ret))
		end
	else
		fengnet.ret(fengnet.pack {"Unknown command"} )
	end
end)

fengnet.start(function() end)

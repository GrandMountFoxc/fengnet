local fengnet = require "fengnet"
local c = require "libfengnet.core"

local function number_address(name)
	local t = type(name)
	if t == "number" then
		return name
	elseif t == "string" then
		local hex = name:match "^:(%x+)"
		if hex then
			return tonumber(hex, 16)
		end
	end
end

function fengnet.launch(...)
	local addr = c.command("LAUNCH", table.concat({...}," "))
	if addr then
		return tonumber(string.sub(addr , 2), 16)
	end
end

function fengnet.kill(name)
	local addr = number_address(name)
	if addr then
		fengnet.send(".launcher","lua","REMOVE", addr, true)
		name = fengnet.address(addr)
	end
	c.command("KILL",name)
end

function fengnet.abort()
	c.command("ABORT")
end

local function globalname(name, handle)
	local c = string.sub(name,1,1)
	assert(c ~= ':')
	if c == '.' then
		return false
	end

	assert(#name < 16)	-- GLOBALNAME_LENGTH is 16, defined in fengnet_harbor.h
	assert(tonumber(name) == nil)	-- global name can't be number

	local harbor = require "fengnet.harbor"

	harbor.globalname(name, handle)

	return true
end

function fengnet.register(name)
	if not globalname(name) then
		c.command("REG", name)
	end
end

function fengnet.name(name, handle)
	if not globalname(name, handle) then
		c.command("NAME", name .. " " .. fengnet.address(handle))
	end
end

local dispatch_message = fengnet.dispatch_message

function fengnet.forward_type(map, start_func)
	c.callback(function(ptype, msg, sz, ...)
		local prototype = map[ptype]
		if prototype then
			dispatch_message(prototype, msg, sz, ...)
		else
			local ok, err = pcall(dispatch_message, ptype, msg, sz, ...)
			c.trash(msg, sz)
			if not ok then
				error(err)
			end
		end
	end, true)
	fengnet.timeout(0, function()
		fengnet.init_service(start_func)
	end)
end

function fengnet.filter(f ,start_func)
	c.callback(function(...)
		dispatch_message(f(...))
	end)
	fengnet.timeout(0, function()
		fengnet.init_service(start_func)
	end)
end

function fengnet.monitor(service, query)
	local monitor
	if query then
		monitor = fengnet.queryservice(true, service)
	else
		monitor = fengnet.uniqueservice(true, service)
	end
	assert(monitor, "Monitor launch failed")
	c.command("MONITOR", string.format(":%08x", monitor))
	return monitor
end

return fengnet

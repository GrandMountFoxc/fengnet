local fengnet = require "fengnet"

local datacenter = {}

function datacenter.get(...)
	return fengnet.call("DATACENTER", "lua", "QUERY", ...)
end

function datacenter.set(...)
	return fengnet.call("DATACENTER", "lua", "UPDATE", ...)
end

function datacenter.wait(...)
	return fengnet.call("DATACENTER", "lua", "WAIT", ...)
end

return datacenter


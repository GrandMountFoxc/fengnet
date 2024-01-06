local fengnet = require "fengnet"
local cluster = require "fengnet.cluster"
require "fengnet.manager"	-- inject fengnet.forward_type

local node, address = ...

fengnet.register_protocol {
	name = "system",
	id = fengnet.PTYPE_SYSTEM,
	unpack = function (...) return ... end,
}

local forward_map = {
	[fengnet.PTYPE_SNAX] = fengnet.PTYPE_SYSTEM,
	[fengnet.PTYPE_LUA] = fengnet.PTYPE_SYSTEM,
	[fengnet.PTYPE_RESPONSE] = fengnet.PTYPE_RESPONSE,	-- don't free response message
}

fengnet.forward_type( forward_map ,function()
	local clusterd = fengnet.uniqueservice("clusterd")
	local n = tonumber(address)
	if n then
		address = n
	end
	local sender = fengnet.call(clusterd, "lua", "sender", node)
	fengnet.dispatch("system", function (session, source, msg, sz)
		if session == 0 then
			fengnet.send(sender, "lua", "push", address, msg, sz)
		else
			fengnet.ret(fengnet.rawcall(sender, "lua", skynet.pack("req", address, msg, sz)))
		end
	end)
end)

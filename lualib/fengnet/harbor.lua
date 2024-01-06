local fengnet = require "fengnet"

local harbor = {}

function harbor.globalname(name, handle)
	handle = handle or fengnet.self()
	fengnet.send(".cslave", "lua", "REGISTER", name, handle)
end

function harbor.queryname(name)
	return fengnet.call(".cslave", "lua", "QUERYNAME", name)
end

function harbor.link(id)
	fengnet.call(".cslave", "lua", "LINK", id)
end

function harbor.connect(id)
	fengnet.call(".cslave", "lua", "CONNECT", id)
end

function harbor.linkmaster()
	fengnet.call(".cslave", "lua", "LINKMASTER")
end

return harbor

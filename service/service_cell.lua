local fengnet = require "fengnet"

local service_name = (...)
local init = {}

function init.init(code, ...)
	local start_func
	fengnet.start = function(f)
		start_func = f
	end
	fengnet.dispatch("lua", function() error("No dispatch function")	end)
	local mainfunc = assert(load(code, service_name))
	assert(fengnet.pcall(mainfunc,...))
	if start_func then
		start_func()
	end
	fengnet.ret()
end

fengnet.start(function()
	fengnet.dispatch("lua", function(_,_,cmd,...)
		init[cmd](...)
	end)
end)

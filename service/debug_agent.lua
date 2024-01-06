local fengnet = require "fengnet"
local debugchannel = require "fengnet.debugchannel"

local CMD = {}

local channel

function CMD.start(address, fd)
	assert(channel == nil, "start more than once")
	fengnet.error(string.format("Attach to :%08x", address))
	local handle
	channel, handle = debugchannel.create()
	local ok, err = pcall(fengnet.call, address, "debug", "REMOTEDEBUG", fd, handle)
	if not ok then
		fengnet.ret(fengnet.pack(false, "Debugger attach failed"))
	else
		-- todo hook
		fengnet.ret(fengnet.pack(true))
	end
	fengnet.exit()
end

function CMD.cmd(cmdline)
	channel:write(cmdline)
end

function CMD.ping()
	fengnet.ret()
end

fengnet.start(function()
	fengnet.dispatch("lua", function(_,_,cmd,...)
		local f = CMD[cmd]
		f(...)
	end)
end)

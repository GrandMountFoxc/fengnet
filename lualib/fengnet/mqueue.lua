-- This is a deprecated module, use fengnet.queue instead.

local fengnet = require "fengnet"
local c = require "libfengnet.core"

local mqueue = {}
local init_once
local thread_id
local message_queue = {}

fengnet.register_protocol {
	name = "queue",
	-- please read fengnet.h for magic number 8
	id = 8,
	pack = fengnet.pack,
	unpack = fengnet.unpack,
	dispatch = function(session, from, ...)
		table.insert(message_queue, {session = session, addr = from, ... })
		if thread_id then
			fengnet.wakeup(thread_id)
			thread_id = nil
		end
	end
}

local function do_func(f, msg)
	return pcall(f, table.unpack(msg))
end

local function message_dispatch(f)
	while true do
		if #message_queue==0 then
			thread_id = coroutine.running()
			fengnet.wait()
		else
			local msg = table.remove(message_queue,1)
			local session = msg.session
			if session == 0 then
				local ok, msg = do_func(f, msg)
				if ok then
					if msg then
						fengnet.fork(message_dispatch,f)
						error(string.format("[:%x] send a message to [:%x] return something", msg.addr, fengnet.self()))
					end
				else
					fengnet.fork(message_dispatch,f)
					error(string.format("[:%x] send a message to [:%x] throw an error : %s", msg.addr, fengnet.self(),msg))
				end
			else
				local data, size = fengnet.pack(do_func(f,msg))
				-- 1 means response
				c.send(msg.addr, 1, session, data, size)
			end
		end
	end
end

function mqueue.register(f)
	assert(init_once == nil)
	init_once = true
	fengnet.fork(message_dispatch,f)
end

local function catch(succ, ...)
	if succ then
		return ...
	else
		error(...)
	end
end

function mqueue.call(addr, ...)
	return catch(fengnet.call(addr, "queue", ...))
end

function mqueue.send(addr, ...)
	return fengnet.send(addr, "queue", ...)
end

function mqueue.size()
	return #message_queue
end

return mqueue

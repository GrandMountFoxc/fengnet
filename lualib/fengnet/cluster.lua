local fengnet = require "fengnet"

local clusterd
local cluster = {}
local sender = {}
local task_queue = {}

local function repack(address, ...)
	return address, fengnet.pack(...)
end

local function request_sender(q, node)
	local ok, c = pcall(fengnet.call, clusterd, "lua", "sender", node)
	if not ok then
		fengnet.error(c)
		c = nil
	end
	-- run tasks in queue
	local confirm = coroutine.running()
	q.confirm = confirm
	q.sender = c
	for _, task in ipairs(q) do
		if type(task) == "string" then
			if c then
				fengnet.send(c, "lua", "push", repack(fengnet.unpack(task)))
			end
		else
			fengnet.wakeup(task)
			fengnet.wait(confirm)
		end
	end
	task_queue[node] = nil
	sender[node] = c
end

local function get_queue(t, node)
	local q = {}
	t[node] = q
	fengnet.fork(request_sender, q, node)
	return q
end

setmetatable(task_queue, { __index = get_queue } )

local function get_sender(node)
	local s = sender[node]
	if not s then
		local q = task_queue[node]
		local task = coroutine.running()
		table.insert(q, task)
		fengnet.wait(task)
		fengnet.wakeup(q.confirm)
		return q.sender
	end
	return s
end

function cluster.call(node, address, ...)
	-- fengnet.pack(...) will free by cluster.core.packrequest
	local s = sender[node]
	if not s then
		local task = fengnet.packstring(address, ...)
		return fengnet.call(get_sender(node), "lua", "req", repack(fengnet.unpack(task)))
	end
	return fengnet.call(s, "lua", "req", address, fengnet.pack(...))
end

function cluster.send(node, address, ...)
	-- push is the same with req, but no response
	local s = sender[node]
	if not s then
		table.insert(task_queue[node], fengnet.packstring(address, ...))
	else
		fengnet.send(sender[node], "lua", "push", address, fengnet.pack(...))
	end
end

function cluster.open(port, maxclient)
	if type(port) == "string" then
		return fengnet.call(clusterd, "lua", "listen", port, nil, maxclient)
	else
		return fengnet.call(clusterd, "lua", "listen", "0.0.0.0", port, maxclient)
	end
end

function cluster.reload(config)
	fengnet.call(clusterd, "lua", "reload", config)
end

function cluster.proxy(node, name)
	return fengnet.call(clusterd, "lua", "proxy", node, name)
end

function cluster.snax(node, name, address)
	local snax = require "fengnet.snax"
	if not address then
		address = cluster.call(node, ".service", "QUERY", "snaxd" , name)
	end
	local handle = fengnet.call(clusterd, "lua", "proxy", node, address)
	return snax.bind(handle, name)
end

function cluster.register(name, addr)
	assert(type(name) == "string")
	assert(addr == nil or type(addr) == "number")
	return fengnet.call(clusterd, "lua", "register", name, addr)
end

function cluster.unregister(name)
	assert(type(name) == "string")
	return fengnet.call(clusterd, "lua", "unregister", name)
end

function cluster.query(node, name)
	return fengnet.call(get_sender(node), "lua", "req", 0, fengnet.pack(name))
end

fengnet.init(function()
	clusterd = fengnet.uniqueservice("clusterd")
end)

return cluster

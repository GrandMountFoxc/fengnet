local fengnet = require "fengnet"
require "fengnet.manager"
local cluster = require "fengnet.cluster.core"

local config_name = fengnet.getenv "cluster"
local node_address = {}
local node_sender = {}
local node_sender_closed = {}
local command = {}
local config = {}
local nodename = cluster.nodename()

local connecting = {}

local function open_channel(t, key)
	local ct = connecting[key]
	if ct then
		local co = coroutine.running()
		local channel
		while ct do
			table.insert(ct, co)
			fengnet.wait(co)
			channel = ct.channel
			ct = connecting[key]
			-- reload again if ct ~= nil
		end
		return assert(node_address[key] and channel)
	end
	ct = {}
	connecting[key] = ct
	local address = node_address[key]
	if address == nil and not config.nowaiting then
		local co = coroutine.running()
		assert(ct.namequery == nil)
		ct.namequery = co
		fengnet.error("Waiting for cluster node [".. key.."]")
		fengnet.wait(co)
		address = node_address[key]
	end
	local succ, err, c
	if address then
		local host, port = string.match(address, "([^:]+):(.*)$")
		c = node_sender[key]
		if c == nil then
			c = fengnet.newservice("clustersender", key, nodename, host, port)
			if node_sender[key] then
				-- double check
				fengnet.kill(c)
				c = node_sender[key]
			else
				node_sender[key] = c
			end
		end

		succ = pcall(fengnet.call, c, "lua", "changenode", host, port)

		if succ then
			t[key] = c
			ct.channel = c
                        node_sender_closed[key] = nil
		else
			err = string.format("changenode [%s] (%s:%s) failed", key, host, port)
		end
	elseif address == false then
		c = node_sender[key]
		if c == nil or node_sender_closed[key] then
			-- no sender or closed, always succ
			succ = true
		else
			-- trun off the sender
			succ, err = pcall(fengnet.call, c, "lua", "changenode", false)
                        if succ then --trun off failed, wait next index todo turn off
                                node_sender_closed[key] = true
                        end
		end
	else
		err = string.format("cluster node [%s] is absent.", key)
	end
	connecting[key] = nil
	for _, co in ipairs(ct) do
		fengnet.wakeup(co)
	end
	if node_address[key] ~= address then
		return open_channel(t,key)
	end
	assert(succ, err)
	return c
end

local node_channel = setmetatable({}, { __index = open_channel })

local function loadconfig(tmp)
	if tmp == nil then
		tmp = {}
		if config_name then
			local f = assert(io.open(config_name))
			local source = f:read "*a"
			f:close()
			assert(load(source, "@"..config_name, "t", tmp))()
		end
	end
	local reload = {}
	for name,address in pairs(tmp) do
		if name:sub(1,2) == "__" then
			name = name:sub(3)
			config[name] = address
			fengnet.error(string.format("Config %s = %s", name, address))
		else
			assert(address == false or type(address) == "string")
			if node_address[name] ~= address then
				-- address changed
				if node_sender[name] then
					-- reset connection if node_sender[name] exist
					node_channel[name] = nil
					table.insert(reload, name)
				end
				node_address[name] = address
			end
			local ct = connecting[name]
			if ct and ct.namequery and not config.nowaiting then
				fengnet.error(string.format("Cluster node [%s] resloved : %s", name, address))
				fengnet.wakeup(ct.namequery)
			end
		end
	end
	if config.nowaiting then
		-- wakeup all connecting request
		for name, ct in pairs(connecting) do
			if ct.namequery then
				fengnet.wakeup(ct.namequery)
			end
		end
	end
	for _, name in ipairs(reload) do
		-- open_channel would block
		fengnet.fork(open_channel, node_channel, name)
	end
end

function command.reload(source, config)
	loadconfig(config)
	fengnet.ret(fengnet.pack(nil))
end

function command.listen(source, addr, port, maxclient)
	local gate = fengnet.newservice("gate")
	if port == nil then
		local address = assert(node_address[addr], addr .. " is down")
		addr, port = string.match(address, "(.+):([^:]+)$")
		port = tonumber(port)
		assert(port ~= 0)
		fengnet.call(gate, "lua", "open", { address = addr, port = port, maxclient = maxclient })
		fengnet.ret(fengnet.pack(addr, port))
	else
		local realaddr, realport = fengnet.call(gate, "lua", "open", { address = addr, port = port, maxclient = maxclient })
		fengnet.ret(fengnet.pack(realaddr, realport))
	end
end

function command.sender(source, node)
	fengnet.ret(fengnet.pack(node_channel[node]))
end

function command.senders(source)
	fengnet.retpack(node_sender)
end

local proxy = {}

function command.proxy(source, node, name)
	if name == nil then
		node, name = node:match "^([^@.]+)([@.].+)"
		if name == nil then
			error ("Invalid name " .. tostring(node))
		end
	end
	local fullname = node .. "." .. name
	local p = proxy[fullname]
	if p == nil then
		p = fengnet.newservice("clusterproxy", node, name)
		-- double check
		if proxy[fullname] then
			fengnet.kill(p)
			p = proxy[fullname]
		else
			proxy[fullname] = p
		end
	end
	fengnet.ret(fengnet.pack(p))
end

local cluster_agent = {}	-- fd:service
local register_name = {}

local function clearnamecache()
	for fd, service in pairs(cluster_agent) do
		if type(service) == "number" then
			fengnet.send(service, "lua", "namechange")
		end
	end
end

function command.register(source, name, addr)
	assert(register_name[name] == nil)
	addr = addr or source
	local old_name = register_name[addr]
	if old_name then
		register_name[old_name] = nil
		clearnamecache()
	end
	register_name[addr] = name
	register_name[name] = addr
	fengnet.ret(nil)
	fengnet.error(string.format("Register [%s] :%08x", name, addr))
end

function command.unregister(_, name)
	if not register_name[name] then
		return fengnet.ret(nil)
	end
	local addr = register_name[name]
	register_name[addr] = nil
	register_name[name] = nil
	clearnamecache()
	fengnet.ret(nil)
	fengnet.error(string.format("Unregister [%s] :%08x", name, addr))
end

function command.queryname(source, name)
	fengnet.ret(fengnet.pack(register_name[name]))
end

function command.socket(source, subcmd, fd, msg)
	if subcmd == "open" then
		fengnet.error(string.format("socket accept from %s", msg))
		-- new cluster agent
		cluster_agent[fd] = false
		local agent = fengnet.newservice("clusteragent", fengnet.self(), source, fd)
		local closed = cluster_agent[fd]
		cluster_agent[fd] = agent
		if closed then
			fengnet.send(agent, "lua", "exit")
			cluster_agent[fd] = nil
		end
	else
		if subcmd == "close" or subcmd == "error" then
			-- close cluster agent
			local agent = cluster_agent[fd]
			if type(agent) == "boolean" then
				cluster_agent[fd] = true
			elseif agent then
				fengnet.send(agent, "lua", "exit")
				cluster_agent[fd] = nil
			end
		else
			fengnet.error(string.format("socket %s %d %s", subcmd, fd, msg or ""))
		end
	end
end

fengnet.start(function()
	loadconfig()
	fengnet.dispatch("lua", function(session , source, cmd, ...)
		local f = assert(command[cmd])
		f(source, ...)
	end)
end)

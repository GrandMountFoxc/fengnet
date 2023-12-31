local fengnet = require "fengnet"
local sc = require "fengnet.socketchannel"
local socket = require "fengnet.socket"
local cluster = require "fengnet.cluster.core"

local channel
local session = 1
local node, nodename, init_host, init_port = ...

local command = {}

local function send_request(addr, msg, sz)
	-- msg is a local pointer, cluster.packrequest will free it
	local current_session = session
	local request, new_session, padding = cluster.packrequest(addr, session, msg, sz)
	session = new_session

	local tracetag = fengnet.tracetag()
	if tracetag then
		if tracetag:sub(1,1) ~= "(" then
			-- add nodename
			local newtag = string.format("(%s-%s-%d)%s", nodename, node, session, tracetag)
			fengnet.tracelog(tracetag, string.format("session %s", newtag))
			tracetag = newtag
		end
		fengnet.tracelog(tracetag, string.format("cluster %s", node))
		channel:request(cluster.packtrace(tracetag))
	end
	return channel:request(request, current_session, padding)
end

function command.req(...)
	local ok, msg = pcall(send_request, ...)
	if ok then
		if type(msg) == "table" then
			fengnet.ret(cluster.concat(msg))
		else
			fengnet.ret(msg)
		end
	else
		fengnet.error(msg)
		fengnet.response()(false)
	end
end

function command.push(addr, msg, sz)
	local request, new_session, padding = cluster.packpush(addr, session, msg, sz)
	if padding then	-- is multi push
		session = new_session
	end

	channel:request(request, nil, padding)
end

local function read_response(sock)
	local sz = socket.header(sock:read(2))
	local msg = sock:read(sz)
	return cluster.unpackresponse(msg)	-- session, ok, data, padding
end

function command.changenode(host, port)
	if not host then
		fengnet.error(string.format("Close cluster sender %s:%d", channel.__host, channel.__port))
		channel:close()
	else
		channel:changehost(host, tonumber(port))
		channel:connect(true)
	end
	fengnet.ret(fengnet.pack(nil))
end

fengnet.start(function()
	channel = sc.channel {
			host = init_host,
			port = tonumber(init_port),
			response = read_response,
			nodelay = true,
		}
	fengnet.dispatch("lua", function(session , source, cmd, ...)
		local f = assert(command[cmd])
		f(...)
	end)
end)

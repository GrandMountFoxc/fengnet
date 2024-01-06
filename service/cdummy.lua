local fengnet = require "fengnet"
require "fengnet.manager"	-- import fengnet.launch, ...

local globalname = {}
local queryname = {}
local harbor = {}
local harbor_service

fengnet.register_protocol {
	name = "harbor",
	id = fengnet.PTYPE_HARBOR,
	pack = function(...) return ... end,
	unpack = fengnet.tostring,
}

fengnet.register_protocol {
	name = "text",
	id = fengnet.PTYPE_TEXT,
	pack = function(...) return ... end,
	unpack = fengnet.tostring,
}

local function response_name(name)
	local address = globalname[name]
	if queryname[name] then
		local tmp = queryname[name]
		queryname[name] = nil
		for _,resp in ipairs(tmp) do
			resp(true, address)
		end
	end
end

function harbor.REGISTER(name, handle)
	assert(globalname[name] == nil)
	globalname[name] = handle
	response_name(name)
	fengnet.redirect(harbor_service, handle, "harbor", 0, "N " .. name)
end

function harbor.QUERYNAME(name)
	if name:byte() == 46 then	-- "." , local name
		fengnet.ret(fengnet.pack(fengnet.localname(name)))
		return
	end
	local result = globalname[name]
	if result then
		fengnet.ret(fengnet.pack(result))
		return
	end
	local queue = queryname[name]
	if queue == nil then
		queue = { fengnet.response() }
		queryname[name] = queue
	else
		table.insert(queue, fengnet.response())
	end
end

function harbor.LINK(id)
	fengnet.ret()
end

function harbor.CONNECT(id)
	fengnet.error("Can't connect to other harbor in single node mode")
end

fengnet.start(function()
	local harbor_id = tonumber(fengnet.getenv "harbor")
	assert(harbor_id == 0)

	fengnet.dispatch("lua", function (session,source,command,...)
		local f = assert(harbor[command])
		f(...)
	end)
	fengnet.dispatch("text", function(session,source,command)
		-- ignore all the command
	end)

	harbor_service = assert(fengnet.launch("harbor", harbor_id, fengnet.self()))
end)

local fengnet = require "fengnet"
local sd = require "fengnet.sharedata.corelib"

local service

fengnet.init(function()
	service = fengnet.uniqueservice "sharedatad"
end)

local sharedata = {}
local cache = setmetatable({}, { __mode = "kv" })

local function monitor(name, obj, cobj)
	local newobj = cobj
	while true do
		newobj = fengnet.call(service, "lua", "monitor", name, newobj)
		if newobj == nil then
			break
		end
		sd.update(obj, newobj)
		fengnet.send(service, "lua", "confirm" , newobj)
	end
	if cache[name] == obj then
		cache[name] = nil
	end
end

function sharedata.query(name)
	if cache[name] then
		return cache[name]
	end
	local obj = fengnet.call(service, "lua", "query", name)
	if cache[name] and cache[name].__obj == obj then
		fengnet.send(service, "lua", "confirm" , obj)
		return cache[name]
	end
	local r = sd.box(obj)
	fengnet.send(service, "lua", "confirm" , obj)
	fengnet.fork(monitor,name, r, obj)
	cache[name] = r
	return r
end

function sharedata.new(name, v, ...)
	fengnet.call(service, "lua", "new", name, v, ...)
end

function sharedata.update(name, v, ...)
	fengnet.call(service, "lua", "update", name, v, ...)
end

function sharedata.delete(name)
	fengnet.call(service, "lua", "delete", name)
end

function sharedata.flush()
	for name, obj in pairs(cache) do
		sd.flush(obj)
	end
	collectgarbage()
end

function sharedata.deepcopy(name, ...)
	if cache[name] then
		local cobj = cache[name].__obj
		return sd.copy(cobj, ...)
	end

	local cobj = fengnet.call(service, "lua", "query", name)
	local ret = sd.copy(cobj, ...)
	fengnet.send(service, "lua", "confirm" , cobj)
	return ret
end

return sharedata

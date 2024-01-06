local fengnet = require "fengnet"
local service = require "fengnet.service"
local core = require "fengnet.datasheet.core"

local datasheet_svr

fengnet.init(function()
	datasheet_svr = service.query "datasheet"
end)

local datasheet = {}
local sheets = setmetatable({}, {
	__gc = function(t)
		fengnet.send(datasheet_svr, "lua", "close")
	end,
})

local function querysheet(name)
	return fengnet.call(datasheet_svr, "lua", "query", name)
end

local function updateobject(name)
	local t = sheets[name]
	if not t.object then
		t.object = core.new(t.handle)
	end
	local function monitor()
		local handle = t.handle
		local newhandle = fengnet.call(datasheet_svr, "lua", "monitor", handle)
		core.update(t.object, newhandle)
		t.handle = newhandle
		fengnet.send(datasheet_svr, "lua", "release", handle)
		return monitor()
	end
	fengnet.fork(monitor)
end

function datasheet.query(name)
	local t = sheets[name]
	if not t then
		t = {}
		sheets[name] = t
	end
	if t.error then
		error(t.error)
	end
	if t.object then
		return t.object
	end
	if t.queue then
		local co = coroutine.running()
		table.insert(t.queue, co)
		fengnet.wait(co)
	else
		t.queue = {}	-- create wait queue for other query
		local ok, handle = pcall(querysheet, name)
		if ok then
			t.handle = handle
			updateobject(name)
		else
			t.error = handle
		end
		local q = t.queue
		t.queue = nil
		for _, co in ipairs(q) do
			fengnet.wakeup(co)
		end
	end
	if t.error then
		error(t.error)
	end
	return t.object
end

return datasheet

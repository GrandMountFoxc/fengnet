-- fengnet module two-step initialize . When you require a fengnet module :
-- 1. Run module main function as official lua module behavior.
-- 2. Run the functions register by fengnet.init() during the step 1,
--      unless calling `require` in main thread .
-- If you call `require` in main thread ( service main function ), the functions
-- registered by fengnet.init() do not execute immediately, they will be executed
-- by fengnet.start() before start function.

local M = {}

local mainthread, ismain = coroutine.running()
assert(ismain, "fengnet.require must initialize in main thread")

local context = {
	[mainthread] = {},
}

do
	local require = _G.require
	local loaded = package.loaded
	local loading = {}

	function M.require(name)
		local m = loaded[name]
		if m ~= nil then
			return m
		end

		local co, main = coroutine.running()
		if main then
			-- print("out: ", require(name))
			return require(name)
		end

		local filename = package.searchpath(name, package.path)
		if not filename then
			return require(name)
		end

		local modfunc = loadfile(filename)
		if not modfunc then
			return require(name)
		end

		local loading_queue = loading[name]
		if loading_queue then
			assert(loading_queue.co ~= co, "circular dependency")
			-- Module is in the init process (require the same mod at the same time in different coroutines) , waiting.
			local fengnet = require "fengnet"
			loading_queue[#loading_queue+1] = co
			fengnet.wait(co)
			local m = loaded[name]
			if m == nil then
				error(string.format("require %s failed", name))
			end
			return m
		end

		loading_queue = {co = co}
		loading[name] = loading_queue

		local old_init_list = context[co]
		local init_list = {}
		context[co] = init_list

		-- We should call modfunc in lua, because modfunc may yield by calling M.require recursive.
		local function execute_module()
			local m = modfunc(name, filename)

			for _, f in ipairs(init_list) do
				f()
			end

			if m == nil then
				m = true
			end

			loaded[name] = m
		end

		local ok, err = xpcall(execute_module, debug.traceback)

		context[co] = old_init_list

		local waiting = #loading_queue
		if waiting > 0 then
			local fengnet = require "fengnet"
			for i = 1, waiting do
				fengnet.wakeup(loading_queue[i])
			end
		end
		loading[name] = nil

		if ok then
			return loaded[name]
		else
			error(err)
		end
	end
end

function M.init_all()
	for _, f in ipairs(context[mainthread]) do
		f()
	end
	context[mainthread] = nil
end

function M.init(f)
	assert(type(f) == "function")
	local co = coroutine.running()
	table.insert(context[co], f)
end

return M
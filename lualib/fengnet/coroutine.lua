-- You should use this module (fengnet.coroutine) instead of origin lua coroutine in fengnet framework

local coroutine = coroutine
-- origin lua coroutine module
local coroutine_resume = coroutine.resume
local coroutine_yield = coroutine.yield
local coroutine_status = coroutine.status
local coroutine_running = coroutine.running
local coroutine_close = coroutine.close

local select = select
local fengnetco = {}

fengnetco.isyieldable = coroutine.isyieldable
fengnetco.running = coroutine.running
fengnetco.status = coroutine.status

local fengnet_coroutines = setmetatable({}, { __mode = "kv" })
-- true : fengnet coroutine
-- false : fengnet suspend
-- nil : exit

function fengnetco.create(f)
	local co = coroutine.create(f)
	-- mark co as a fengnet coroutine
	fengnet_coroutines[co] = true
	return co
end

do -- begin fengnetco.resume
	local function unlock(co, ...)
		fengnet_coroutines[co] = true
		return ...
	end

	local function fengnet_yielding(co, ...)
		fengnet_coroutines[co] = false
		return unlock(co, coroutine_resume(co, coroutine_yield(...)))
	end

	local function resume(co, ok, ...)
		if not ok then
			return ok, ...
		elseif coroutine_status(co) == "dead" then
			-- the main function exit
			fengnet_coroutines[co] = nil
			return true, ...
		elseif (...) == "USER" then
			return true, select(2, ...)
		else
			-- blocked in fengnet framework, so raise the yielding message
			return resume(co, fengnet_yielding(co, ...))
		end
	end

	-- record the root of coroutine caller (It should be a fengnet thread)
	local coroutine_caller = setmetatable({} , { __mode = "kv" })

	function fengnetco.resume(co, ...)
		local co_status = fengnet_coroutines[co]
		if not co_status then
			if co_status == false then
				-- is running
				return false, "cannot resume a fengnet coroutine suspend by fengnet framework"
			end
			if coroutine_status(co) == "dead" then
				-- always return false, "cannot resume dead coroutine"
				return coroutine_resume(co, ...)
			else
				return false, "cannot resume none fengnet coroutine"
			end
		end
		local from = coroutine_running()
		local caller = coroutine_caller[from] or from
		coroutine_caller[co] = caller
		return resume(co, coroutine_resume(co, ...))
	end

	function fengnetco.thread(co)
		co = co or coroutine_running()
		if fengnet_coroutines[co] ~= nil then
			return coroutine_caller[co] , false
		else
			return co, true
		end
	end

end -- end of fengnetco.resume

function fengnetco.status(co)
	local status = coroutine_status(co)
	if status == "suspended" then
		if fengnet_coroutines[co] == false then
			return "blocked"
		else
			return "suspended"
		end
	else
		return status
	end
end

function fengnetco.yield(...)
	return coroutine_yield("USER", ...)
end

do -- begin fengnetco.wrap

	local function wrap_co(ok, ...)
		if ok then
			return ...
		else
			error(...)
		end
	end

function fengnetco.wrap(f)
	local co = fengnetco.create(function(...)
		return f(...)
	end)
	return function(...)
		return wrap_co(fengnetco.resume(co, ...))
	end
end

end	-- end of fengnetco.wrap

function fengnetco.close(co)
	fengnet_coroutines[co] = nil
	return coroutine_close(co)
end

return fengnetco

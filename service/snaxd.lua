local fengnet = require "fengnet"
local c = require "libfengnet.core"
local snax_interface = require "snax.interface"
local profile = require "fengnet.profile"
local snax = require "fengnet.snax"

local snax_name = tostring(...)
local loaderpath = fengnet.getenv"snax_loader"
local loader = loaderpath and assert(dofile(loaderpath))
local func, pattern = snax_interface(snax_name, _ENV, loader)
local snax_path = pattern:sub(1,pattern:find("?", 1, true)-1) .. snax_name ..  "/"
package.path = snax_path .. "?.lua;" .. package.path

SERVICE_NAME = snax_name
SERVICE_PATH = snax_path

local profile_table = {}

local function update_stat(name, ti)
	local t = profile_table[name]
	if t == nil then
		t = { count = 0,  time = 0 }
		profile_table[name] = t
	end
	t.count = t.count + 1
	t.time = t.time + ti
end

local traceback = debug.traceback

local function return_f(f, ...)
	return fengnet.ret(fengnet.pack(f(...)))
end

local function timing( method, ... )
	local err, msg
	profile.start()
	if method[2] == "accept" then
		-- no return
		err,msg = xpcall(method[4], traceback, ...)
	else
		err,msg = xpcall(return_f, traceback, method[4], ...)
	end
	local ti = profile.stop()
	update_stat(method[3], ti)
	assert(err,msg)
end

fengnet.start(function()
	local init = false
	local function dispatcher( session , source , id, ...)
		local method = func[id]

		if method[2] == "system" then
			local command = method[3]
			if command == "hotfix" then
				local hotfix = require "snax.hotfix"
				fengnet.ret(fengnet.pack(hotfix(func, ...)))
			elseif command == "profile" then
				fengnet.ret(fengnet.pack(profile_table))
			elseif command == "init" then
				assert(not init, "Already init")
				local initfunc = method[4] or function() end
				initfunc(...)
				fengnet.ret()
				fengnet.info_func(function()
					return profile_table
				end)
				init = true
			else
				assert(init, "Never init")
				assert(command == "exit")
				local exitfunc = method[4] or function() end
				exitfunc(...)
				fengnet.ret()
				init = false
				fengnet.exit()
			end
		else
			assert(init, "Init first")
			timing(method, ...)
		end
	end
	fengnet.dispatch("snax", dispatcher)

	-- set lua dispatcher
	function snax.enablecluster()
		fengnet.dispatch("lua", dispatcher)
	end
end)

local table = table
local extern_dbgcmd = {}

local function init(fengnet, export)
	local internal_info_func

	function fengnet.info_func(func)
		internal_info_func = func
	end

	local dbgcmd

	local function init_dbgcmd()
		dbgcmd = {}

		function dbgcmd.MEM()
			local kb = collectgarbage "count"
			fengnet.ret(fengnet.pack(kb))
		end

		local gcing = false
		function dbgcmd.GC()
			if gcing then
				return
			end
			gcing = true
			local before = collectgarbage "count"
			local before_time = fengnet.now()
			collectgarbage "collect"
			-- skip subsequent GC message
			fengnet.yield()
			local after = collectgarbage "count"
			local after_time = fengnet.now()
			fengnet.error(string.format("GC %.2f Kb -> %.2f Kb, cost %.2f sec", before, after, (after_time - before_time) / 100))
			gcing = false
		end

		function dbgcmd.STAT()
			local stat = {}
			stat.task = fengnet.task()
			stat.mqlen = fengnet.stat "mqlen"
			stat.cpu = fengnet.stat "cpu"
			stat.message = fengnet.stat "message"
			fengnet.ret(fengnet.pack(stat))
		end

		function dbgcmd.KILLTASK(threadname)
			local co = fengnet.killthread(threadname)
			if co then
				fengnet.error(string.format("Kill %s", co))
				fengnet.ret()
			else
				fengnet.error(string.format("Kill %s : Not found", threadname))
				fengnet.ret(fengnet.pack "Not found")
			end
		end

		function dbgcmd.TASK(session)
			if session then
				fengnet.ret(fengnet.pack(fengnet.task(session)))
			else
				local task = {}
				fengnet.task(task)
				fengnet.ret(fengnet.pack(task))
			end
		end

		function dbgcmd.UNIQTASK()
			fengnet.ret(fengnet.pack(fengnet.uniqtask()))
		end

		function dbgcmd.INFO(...)
			if internal_info_func then
				fengnet.ret(fengnet.pack(internal_info_func(...)))
			else
				fengnet.ret(fengnet.pack(nil))
			end
		end

		function dbgcmd.EXIT()
			fengnet.exit()
		end

		function dbgcmd.RUN(source, filename, ...)
			local inject = require "fengnet.inject"
			local args = table.pack(...)
			local ok, output = inject(fengnet, source, filename, args, export.dispatch, fengnet.register_protocol)
			collectgarbage "collect"
			fengnet.ret(fengnet.pack(ok, table.concat(output, "\n")))
		end

		function dbgcmd.TERM(service)
			fengnet.term(service)
		end

		function dbgcmd.REMOTEDEBUG(...)
			local remotedebug = require "fengnet.remotedebug"
			remotedebug.start(export, ...)
		end

		function dbgcmd.SUPPORT(pname)
			return fengnet.ret(fengnet.pack(fengnet.dispatch(pname) ~= nil))
		end

		function dbgcmd.PING()
			return fengnet.ret()
		end

		function dbgcmd.LINK()
			fengnet.response()	-- get response , but not return. raise error when exit
		end

		function dbgcmd.TRACELOG(proto, flag)
			if type(proto) ~= "string" then
				flag = proto
				proto = "lua"
			end
			fengnet.error(string.format("Turn trace log %s for %s", flag, proto))
			fengnet.traceproto(proto, flag)
			fengnet.ret()
		end

		return dbgcmd
	end -- function init_dbgcmd

	local function _debug_dispatch(session, address, cmd, ...)
		dbgcmd = dbgcmd or init_dbgcmd() -- lazy init dbgcmd
		local f = dbgcmd[cmd] or extern_dbgcmd[cmd]
		assert(f, cmd)
		f(...)
	end

	fengnet.register_protocol {
		name = "debug",
		id = assert(fengnet.PTYPE_DEBUG),
		pack = assert(fengnet.pack),
		unpack = assert(fengnet.unpack),
		dispatch = _debug_dispatch,
	}
end

local function reg_debugcmd(name, fn)
	extern_dbgcmd[name] = fn
end

return {
	init = init,
	reg_debugcmd = reg_debugcmd,
}

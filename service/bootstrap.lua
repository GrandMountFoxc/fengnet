local fengnet = require "fengnet"
local harbor = require "fengnet.harbor"
local service = require "fengnet.service"
require "fengnet.manager"	-- import fengnet.launch, ...

fengnet.start(function()
	local standalone = fengnet.getenv "standalone"

	local launcher = assert(fengnet.launch("snlua","launcher"))
	fengnet.name(".launcher", launcher)

	local harbor_id = tonumber(fengnet.getenv "harbor" or 0)
	if harbor_id == 0 then
		assert(standalone ==  nil)
		standalone = true
		fengnet.setenv("standalone", "true")

		local ok, slave = pcall(fengnet.newservice, "cdummy")
		if not ok then
			fengnet.abort()
		end
		fengnet.name(".cslave", slave)

	else
		if standalone then
			if not pcall(fengnet.newservice,"cmaster") then
				fengnet.abort()
			end
		end

		local ok, slave = pcall(fengnet.newservice, "cslave")
		if not ok then
			fengnet.abort()
		end
		fengnet.name(".cslave", slave)
	end

	if standalone then
		local datacenter = fengnet.newservice "datacenterd"
		fengnet.name("DATACENTER", datacenter)
	end
	fengnet.newservice "service_mgr"

	local enablessl = fengnet.getenv "enablessl"
	if enablessl then
		service.new("ltls_holder", function ()
			local c = require "ltls.init.c"
			c.constructor()
		end)
	end

	pcall(fengnet.newservice,fengnet.getenv "start" or "main")
	fengnet.exit()
end)

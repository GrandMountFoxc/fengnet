-- loader.lua
-- 从加载bootstrap.lua的过程来看应该就是把文件加载成函数再调用
local args = {}
for word in string.gmatch(..., "%S+") do
	table.insert(args, word)
end

-- print(table.unpack(args))

SERVICE_NAME = args[1]

local main, pattern

local err = {}
for pat in string.gmatch(LUA_SERVICE, "([^;]+);*") do
	local filename = string.gsub(pat, "?", SERVICE_NAME)
	local f, msg = loadfile(filename)
	if not f then
		table.insert(err, msg)
	else
		pattern = pat
		main = f
		break
	end
end

if not main then
	error(table.concat(err, "\n"))
end

LUA_SERVICE = nil
package.path , LUA_PATH = LUA_PATH
package.cpath , LUA_CPATH = LUA_CPATH

local service_path = string.match(pattern, "(.*/)[^/?]+$")

-- print(service_path, package.path, package.cpath, LUA_PRELOAD)

if service_path then
	service_path = string.gsub(service_path, "?", args[1])
	package.path = service_path .. "?.lua;" .. package.path
	SERVICE_PATH = service_path
else
	local p = string.match(pattern, "(.*/).+$")
	SERVICE_PATH = p
end

if LUA_PRELOAD then
	local f = assert(loadfile(LUA_PRELOAD))
	f(table.unpack(args))
	LUA_PRELOAD = nil
end

_G.require = (require "skynet.require").require	-- _G是全局变量表，那这里应该就可以理解成是把原本lua的require替换成skynet的require

main(select(2, table.unpack(args)))

local p = premake
local api = p.api

p.PS4 = "ps4"

api.addAllowed("system", p.PS4)

local osoption = p.option.get("os")
if osoption ~= nil then
    table.insert(osoption.allowed, { "ps4", "Sony Playstation 4" })
end

filter { "system:ps4", "kind:ConsoleApp or WindowedApp" }
	targetextension ".exe"
	
filter { "system:ps4", "kind:SharedLib" }
	targetprefix ""
	targetextension ".so"

filter { "system:ps4", "kind:StaticLib" }
	targetprefix "lib"
	targetextension ".a"

return function(cfg)
	return (cfg.system == p.PS4)
end

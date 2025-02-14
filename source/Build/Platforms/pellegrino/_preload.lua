local p = premake
local api = p.api

p.Pellegrino = "pellegrino"

api.addAllowed("system", p.Pellegrino)

local osoption = p.option.get("os")
if osoption ~= nil then
    table.insert(osoption.allowed, { "pellegrino", "Pellegrino" })
end

filter { "system:pellegrino", "kind:ConsoleApp or WindowedApp" }
	targetextension ".exe"
	
filter { "system:pellegrino", "kind:SharedLib" }
	targetprefix ""
	targetextension ".so"

filter { "system:pellegrino", "kind:StaticLib" }
	targetprefix "lib"
	targetextension ".a"

return function(cfg)
	return (cfg.system == p.Pellegrino)
end

local p = premake
local api = p.api

p.XBOXONE = "xboxone"

api.addAllowed("system", p.XBOXONE)
api.addAllowed("architecture", { "x86", "x64" })
api.addAllowed("flags",{ 
		"NoGenerateManifest",
		"NoEmbedManifest"
})

local osoption = p.option.get("os")
if osoption ~= nil then
	table.insert(osoption.allowed, { "xboxone",  "Microsoft Xbox One" })
end

-- add system tags for XBOXONE
os.systemTags[p.XBOXONE] = { "xboxone" }

filter { "system:xboxone", "kind:ConsoleApp or WindowedApp" }
	targetextension ".exe"
	
filter { "system:xboxone", "kind:SharedLib" }
	targetprefix ""
	targetextension ".dll"
	implibextension ".lib"

filter { "system:xboxone", "kind:StaticLib" }
	targetprefix ""
	targetextension ".lib"

api.register {
	name = "targetlayoutdir",
	scope = "config",
	kind = "string",
	tokens = true,
}

return function(cfg)
	return (cfg.system == p.XBOXONE)
end

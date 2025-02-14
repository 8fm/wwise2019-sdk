local p = premake
local api = p.api

p.gdx = "gdx"

api.addAllowed("system", p.gdx)
api.addAllowed("architecture", { "Gaming.Desktop.x64" })
api.addAllowed("flags",{ 
		"NoGenerateManifest",
		"NoEmbedManifest"
})

local osoption = p.option.get("os")
if osoption ~= nil then
	table.insert(osoption.allowed, { "gdx",  "Microsoft Gaming Desktop x64" })
end

-- add system tags for gdx
os.systemTags[p.gdx] = { "gdx" }

filter { "system:gdx", "kind:ConsoleApp or WindowedApp" }
	targetextension ".exe"
	
filter { "system:gdx", "kind:SharedLib" }
	targetprefix ""
	targetextension ".dll"
	implibextension ".lib"

filter { "system:gdx", "kind:StaticLib" }
	targetprefix ""
	targetextension ".lib"

api.register {
	name = "targetlayoutdir",
	scope = "config",
	kind = "string",
	tokens = true,
}

return function(cfg)
	return (cfg.system == p.gdx)
end

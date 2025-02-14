local p = premake

if not p.modules.gdx then

	include ( "_preload.lua" )

	require ("vstudio")
	p.modules.gdx = {}

	include("gdx_vcxproj.lua")	
end

return p.modules.gdx

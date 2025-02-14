local p = premake

if not p.modules.pellegrino then

	include ( "_preload.lua" )

	require ("vstudio")
	p.modules.pellegrino = {}

	include("pellegrino_vcxproj.lua")
end

return p.modules.pellegrino

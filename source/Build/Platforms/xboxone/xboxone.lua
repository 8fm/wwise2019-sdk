local p = premake

if not p.modules.xboxone then

	include ( "_preload.lua" )

	require ("vstudio")
	p.modules.xboxone = {}

	include("xboxone_vcxproj.lua")
	include("xboxone_vstudio.lua")
end

return p.modules.xboxone

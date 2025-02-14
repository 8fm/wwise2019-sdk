local p = premake

if not p.modules.ps4 then

	include ( "_preload.lua" )

	require ("vstudio")
	p.modules.ps4 = {}

	include("ps4_vcxproj.lua")
end

return p.modules.ps4

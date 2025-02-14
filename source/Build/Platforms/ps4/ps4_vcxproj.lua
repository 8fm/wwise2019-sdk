local p = premake

local vstudio = p.vstudio
local vc2010 = p.vstudio.vc2010

if vstudio.vs2010_architectures ~= nil then
	vstudio.vs2010_architectures.ps4 = "ORBIS"
end

premake.override(vc2010, "optimization", function(oldfn, cfg, condition)
	oldfn(cfg, condition)
	if cfg.system == p.PS4 then
		if cfg.symbols == p.ON then
			vc2010.element(cfg, "GenerateDebugInformation", nil, "true")
		end
		if cfg.optimize == "Speed" then
			vc2010.element(cfg, "OptimizationLevel", nil, "Level3")
			vc2010.element(cfg, "FastMath", nil, "true")
		end
	end
end)

-- So that 'flags { "FatalWarning" }' adds -Werror
premake.override(vc2010, "additionalCompileOptions", function(oldfn, cfg, condition)
	if cfg.system == p.PS4 then
		if cfg.flags.FatalCompileWarnings and cfg.warnings ~= p.OFF then
			table.insert(cfg.buildoptions, "-Werror")
		end
	end
	oldfn(cfg, condition)
end)

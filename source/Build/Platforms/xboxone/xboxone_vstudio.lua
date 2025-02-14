local p = premake

local xboxone = p.modules.xboxone
local vstudio = p.vstudio

premake.override(vstudio, "archFromConfig", function (oldfn, cfg, win32)
	-- Bypass that pesky Win32 hack by not passing win32 down
	if cfg.system == premake.XBOXONE and _ACTION >= "vs2015" then
		
		local archMap = {
			["xboxone"] = "Durango",
			["xboxone"] = "Durango",
		}
		
		if cfg.architecture ~= nil and archMap[cfg.architecture] ~= nil then
			return archMap[cfg.architecture]
		else			
			return oldfn(cfg)
		end			
	end
	
	return oldfn(cfg, win32)
end)

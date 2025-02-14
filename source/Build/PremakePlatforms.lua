--[[----------------------------------------------------------------------------
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

  Copyright (c) 2020 Audiokinetic Inc.
------------------------------------------------------------------------------]]

if not AK then AK = {} end
if not AK.Platform then

	-- These are not capitalized
	local platformRename = {
		aix = "AIX",
		bsd = "BSD",
		ios = "iOS",
		macosx = "macOS", -- Note: discrepancy in name here!
		ps4 = "PS4",
		xboxone = "XboxOne",
		nx = "NX",
		tvos = "tvOS",
		uwp = "UWP",
		qnx = "QNX",
		ggp = "GGP",
		gx = "GX",
		gdx = "GDX",
		chinook = "Chinook",
	}

	local platformFileName = os.target()

	if platformRename[platformFileName] ~= nil then
		platformFileName = platformRename[platformFileName]
	else
		platformFileName = platformFileName:sub(1,1):upper()..platformFileName:sub(2):lower() -- Capitalize
	end

	AK.Platform = require ("Platforms." .. platformFileName)
end

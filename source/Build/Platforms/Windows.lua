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
if not AK.Platforms then AK.Platforms = {} end

AK.Platforms.Windows =
{
	name = "Windows",
	directories = {
		src = {
			__default__ = "Win32",
			CommunicationCentral = "PC",
			IntegrationDemo = "Windows",
			AkSoundEngineDLL = "Win32",
		},
		project = {
			__default__ = "Win32",
			GameSimulator = "PC",
			CommunicationCentral = "PC",
			LuaLib = "PC",
			ToLuaLib = "PC",
			IntegrationDemoSln = "Windows",
			IntegrationDemoMotionSln = "Windows",
		},
		simd = "SIMD",
		lualib = "Win32",
		lowlevelio = "Win32",
		luasln = "GameSimulator/source/",
	},
	kinds = {
		GameSimulator = "ConsoleApp",
		IntegrationDemo = "WindowedApp"
	},
	suffix = {
		__default__ = "Windows",
		CommunicationCentral = "PC",
		GameSimulator = "PC",
		LuaLib = "PC",
		ToLuaLib = "PC",
		LuaSolutions = "PC",
		AllEffectsSln = "PC",
		SamplePluginsSln = "PC",
		AkStreamMgrSln = "PC",
	},

	configurations =
	{
		"Debug",
		"Debug(StaticCRT)",
		"Profile",
		"Profile_EnableAsserts",
		"Profile(StaticCRT)" ,
		"Profile(StaticCRT)_EnableAsserts" ,
		"Release",
		"Release(StaticCRT)",
	},
	platforms = { "Win32", "x64" },
	avx2archs = { "x64" },
	avxarchs = { "x64" },
	features = { "Motion", "iZotope", "UnitTests", "sceAudio3d", "SampleSink", "IntegrationDemo", "SoundEngineDLL", "fastcall" },
	validActions = { "vs2015", "vs2017", "vs2019" },

	AdditionalSoundEngineProjects = function()
		return {}
	end,
	AddActionSuffixToDllProjects = true,

	-- API
	---------------------------------
	ImportAdditionalWwiseSDKProjects = function()
	end,

	-- Project factory. Creates "StaticLib" target by default. Static libs (only) are added to the global list of targets.
	-- Other target types supported by premake are "WindowedApp", "ConsoleApp" and "SharedLib".
	-- Upon returning from this method, the current scope is the newly created project.
	CreateProject = function(in_fileName, in_targetName, in_projectLocation, in_suffix, pathPCH, in_targetType)
		verbosef("        Creating project: %s", in_targetName)

		-- Make sure that directory exist
		os.mkdir(AkMakeAbsolute(in_projectLocation))

		-- Create project
		local prj = project(in_targetName)
			if not _AK_BUILD_AUTHORING then
				platforms({"Win32", "x64"})
			end
			location(AkRelativeToCwd(in_projectLocation))
			targetname(in_targetName)
			if in_targetType == nil or in_targetType == "StaticLib" then
				kind("StaticLib")
				-- Add to global table
				_AK_TARGETS[in_targetName] = in_fileName
			else
				kind(in_targetType)
			end
			language("C++")
			uuid(GenerateUuid(in_fileName))
			filename(in_fileName)
			symbols ("on")
			symbolspath "$(OutDir)$(TargetName).pdb"
			flags {
				-- Treat all warnings as errors
				"FatalWarnings",
				-- We never want .user files, we always want .filters files.
				"OmitUserFiles",
				"ForceFiltersFiles"
			}

			-- Common flags.
			characterset "Unicode"
			exceptionhandling "Default"

			-- Precompiled headers.
			if pathPCH ~= nil then
				files
				{
					AkRelativeToCwd(pathPCH) .. "stdafx.cpp",
					AkRelativeToCwd(pathPCH) .. "stdafx.h",
				}
				--pchheader ( AkRelativeToCwd(pathPCH) .. "stdafx.h" )
				pchheader "stdafx.h"
				pchsource ( AkRelativeToCwd(pathPCH) .. "stdafx.cpp" )
				--pchsource "stdafx.cpp"
			end

			if _OPTIONS["lowmemorytesting"] then
				defines "MSTC_SYSTEMATIC_MEMORY_STRESS"
				defines "AK_MEMDEBUG"
			end

			if _OPTIONS["floatingpointexception"] then
				floatingpoint "Precise"
				floatingpointexceptions 'On'
				buildoptions { "/wd4305" } -- Disable loss-of-precision conversion warnings
			end
			
			-- Standard configuration settings.
			filter ("Debug*")
				defines "_DEBUG"

			filter ("Profile*")
				defines "NDEBUG"
				optimize ("Speed")

			filter ("Release*")
				defines "NDEBUG"
				optimize ("Speed")

			filter "*EnableAsserts"
				defines "AK_ENABLE_ASSERTS"

			filter {}

			-- Only vs2017+ require specifying Full symbols
			filter ("Debug*", "action:vs2017 or vs2019")
				symbols "Full"

			-- Disable small exception handling optimizations introduced in VS 2019 16.3
			-- since they add a dependency on the combined 2015-2019 vc++ redists (for vc140runtime_1.dll)
			filter "toolset:msc-v142"
				buildoptions { "/d2FH4-" }
				linkoptions { "/d2:-FH4-" }

			filter {}

			if not _AK_BUILD_AUTHORING then
			-- Note: The AuthoringRelease config is "profile", really. It must not be AK_OPTIMIZED.
			filter "Release*"
				defines "AK_OPTIMIZED"
			end

			-- Add configuration specific options.
			filter "*_fastcall"
				callingconvention "FastCall"

			-- Add architecture specific libdirs.
			filter "platforms:Win32"
				architecture "x86"
				defines "WIN32"
				vectorextensions "SSE"
			filter "platforms:x64"
				architecture "x86_64"
				defines "WIN64"

			filter "action:vs2015"
				systemversion "10.0.14393.0"

			filter "action:vs2017 or vs2019"
				systemversion "10.0.17763.0"

			filter {}

			defines "WIN32_LEAN_AND_MEAN"

			-- Style sheets.
			local ssext = ".props"

			if in_targetType == "SharedLib" then
				if _AK_BUILD_AUTHORING then
					filter "Debug*"
						vs_propsheet(AkRelativeToCwd(_AK_ROOT_DIR) .. "PropertySheets/Win32/Debug" .. GetSuffixFromCurrentAction() .. ssext)
					filter "Profile* or Release*"
						vs_propsheet(AkRelativeToCwd(_AK_ROOT_DIR) .. "PropertySheets/Win32/NDebug" .. GetSuffixFromCurrentAction() .. ssext)
				else
					filter "Debug*"
						vs_propsheet(AkRelativeToCwd(_AK_ROOT_DIR) .. "PropertySheets/Win32/Debug_StaticCRT" .. in_suffix .. ssext)
					filter "Profile* or Release*"
						vs_propsheet(AkRelativeToCwd(_AK_ROOT_DIR) .. "PropertySheets/Win32/NDebug_StaticCRT" .. in_suffix .. ssext)
				end

			else
				filter "*Debug or Debug_fastcall"
					vs_propsheet(AkRelativeToCwd(_AK_ROOT_DIR) .. "PropertySheets/Win32/Debug" .. in_suffix .. ssext)
				filter "*Debug(StaticCRT)*"
					vs_propsheet(AkRelativeToCwd(_AK_ROOT_DIR) .. "PropertySheets/Win32/Debug_StaticCRT" .. in_suffix .. ssext)
				filter "*Profile or *Profile_EnableAsserts or *Release or Profile_fastcall or Release_fastcall"
					vs_propsheet(AkRelativeToCwd(_AK_ROOT_DIR) .. "PropertySheets/Win32/NDebug" .. in_suffix .. ssext)
				filter "*Profile(StaticCRT)* or *Release(StaticCRT)*"
					vs_propsheet(AkRelativeToCwd(_AK_ROOT_DIR) .. "PropertySheets/Win32/NDebug_StaticCRT" .. in_suffix .. ssext)
			end

			DisablePropSheetElements()
			filter {}
				removeelements {
					"TargetExt"
				}

			-- Set the scope back to current project
			project(in_targetName)
		
		ApplyPlatformExceptions(prj.name, prj)

		return prj
	end,

	-- Plugin factory.
	-- Upon returning from this method, the current scope is the newly created plugin project.
	CreatePlugin = function(in_fileName, in_targetName, in_projectLocation, in_suffix, pathPCH, in_targetType)
		local prj = AK.Platforms.Windows.CreateProject(in_fileName, in_targetName, in_projectLocation, in_suffix, pathPCH, in_targetType)
		return prj
	end,

	Exceptions = {
		AkSoundEngine = function(prj)
			includedirs {
				"$(FrameworkSdkDir)/include/um"
			}
			local prjLoc = AkRelativeToCwd(prj.location)
			if not _AK_BUILD_AUTHORING then
				filter { "files:" .. prjLoc .. "/../../SoundEngineProxy/**.cpp", "Release*" }
					flags { "ExcludeFromBuild" }
				filter {}
			end
			defines({"AKSOUNDENGINE_DLL", "AKSOUNDENGINE_EXPORTS"})
		end,
		AkSoundEngineDLL = function(prj)
			links {	"msacm32", "ws2_32" }
			runtime "Debug"
		end,
		AkSoundEngineTests = function(prj)
			filter "Debug*"
				links "ws2_32"
			filter "Profile*"
				links "ws2_32"
			filter {}
		end,
		AkMemoryMgr = function(prj)
			defines({"AKSOUNDENGINE_DLL", "AKSOUNDENGINE_EXPORTS"})
		end,
		AkMusicEngine = function(prj)
			local prjLoc = AkRelativeToCwd(prj.location)
			if not _AK_BUILD_AUTHORING then
				filter { "files:" .. prjLoc .. "/../../SoundEngineProxy/**.cpp", "Release*" }
					-- This is how we exclude files per config in Visual Studio
					flags { "ExcludeFromBuild" }
				filter {}
			end

			defines({"AKSOUNDENGINE_DLL", "AKSOUNDENGINE_EXPORTS"})
		end,
		AkSpatialAudio = function(prj)
			defines({"AKSOUNDENGINE_DLL", "AKSOUNDENGINE_EXPORTS"})
		end,
		AkStreamMgr = function(prj)
			defines({"AKSOUNDENGINE_DLL", "AKSOUNDENGINE_EXPORTS"})
		end,
		AkVorbisDecoder = function(prj)
			filter "*Debug or Debug_fastcall or *Profile or *Release or Profile_fastcall or Release_fastcall"
				defines({"AKSOUNDENGINE_DLL"})
			filter {}
		end,
		AkOpusDecoder = function(prj)
			configuration { "*Debug or Debug_fastcall or *Profile or *Release or Profile_fastcall or Release_fastcall" }
				defines({"AKSOUNDENGINE_DLL"})
			configuration {}
		end,
		AkMotionSink = function(prj)
			g_PluginDLL["AkMotion"].extralibs = {
				"dinput8"
			}
		end,
		AkSink = function(prj)
		end,
		AkConvolutionReverbFX = function(prj)
			defines({"AK_USE_PREFETCH"})
		end,
		MasteringSuiteFX = function(prj)
			defines {"SCE_AUDIO_MASTER_USE_AK_SIMD"}
		end,
		SceAudio3dEngine = function(prj)
			-- Remove from AK_TARGETS on Windows. Don't want the Windows IntegrationDemo to link with it.
			_AK_TARGETS['SceAudio3dEngine'] = nil

			local prjLoc = AkRelativeToCwd(prj.location)
			-- directly including PS4 materials because Authoring can output to a simulated sceAudio3d-based sink
			includedirs { 
				prjLoc .. "/../../../../../Authoring/source/3rdParty/Sony/Audio3d/include",
				prjLoc .. "/../PS4",
			}
			files 
			{
				prjLoc .. "/../PS4/*.cpp", 
				prjLoc .. "/../PS4/*.h",
			}
		end,
		iZTrashBoxModelerFX = function(prj)
			local prjLoc = AkRelativeToCwd(prj.location)
			files {
				prjLoc .. "/../../../../iZBaseConsole/src/iZBase/Util/CriticalSection.*",
			}
		end,

		GameSimulator = function(prj)
			local prjLoc = AkRelativeToCwd(prj.location)
			local integrationDemoLocation = prjLoc .. "/../../../../SDK/samples/IntegrationDemo"
			local suffix = GetSuffixFromCurrentAction()
			includedirs {
				prjLoc .. "/../../../../SDK/samples/Motion/Win32",
			}
			files {
				prjLoc .. "/*.rc",
				integrationDemoLocation .. "/Windows/InputMgr.*",
				integrationDemoLocation .. "/MenuSystem/UniversalInput.*",
				integrationDemoLocation .. "/Common/Helpers.cpp",
				prjLoc .. "/../../../../SDK/samples/Motion/Win32/AkDirectInputHelper.*"
			}

			libdirs {
				prjLoc .. "/../../../$(Platform)" .. suffix .. "/$(Configuration)/lib"
			}

			entrypoint "mainCRTStartup"

			-- lua libs
			links {
				"LuaLib",
				"ToLuaLib",

				"ws2_32",
				"dinput8",
				"Dsound",
				"shlwapi",
				"Msacm32",
				"Dbghelp",
				"Winmm",

				"iZHybridReverbFX",
				"iZTrashBoxModelerFX",
				"iZTrashDelayFX",
				"iZTrashDistortionFX",
				"iZTrashDynamicsFX",
				"iZTrashFiltersFX", 
				"iZTrashMultibandDistortionFX",

				"AkSink"
			}
			filter "Debug*"
				libdirs {
					prjLoc .. "/../../../../SDK/$(Platform)" .. suffix .. "/Debug/lib"
				}
			filter "Profile*"
				libdirs {
					prjLoc .. "/../../../../SDK/$(Platform)" .. suffix .. "/Profile/lib"
				}
			filter "Release*"
				libdirs {
					prjLoc .. "/../../../../SDK/$(Platform)" .. suffix .. "/Release/lib"
				}
			filter {}

			-- IMPORTANT! This path below MUST be added AFTER SDK/ above!
			libdirs {
				prjLoc .. "/../../../../Authoring/$(Platform)/$(Configuration)/lib"
			}
		end,
		LuaLib = function(prj)
			-- Generating AkLuaFramework.c: only during development on Windows, now that the AkLuaFramework.cpp file is in the repository.
			prebuildcommands("..\\..\\..\\..\\Tools\\Win32\\bin\\lua2c ..\\..\\..\\Scripts\\audiokinetic\\AkLuaFramework.lua ..\\..\\src\\libraries\\Common\\AkLuaFramework.cpp")
		end,
		IntegrationDemoMotion = function(prj)
			AK.Platforms.Windows.Exceptions.IntegrationDemo(prj)
		end,
		IntegrationDemo = function(prj)
			local prjLoc = AkRelativeToCwd(prj.location)
			local integrationDemoLocation = prjLoc .. "/../../../../SDK/samples/IntegrationDemo"
			local autoGenLoc = '%{cfg.objdir}/AutoGen/'

			local use_d3d12 = false

			entrypoint "WinMainCRTStartup"
			includedirs
			{
				autoGenLoc,
			}
			libdirs { prjLoc .. "/../../../$(Platform)" .. GetSuffixFromCurrentAction() .. "/$(Configuration)/lib" }

			-- System
			links {
				"ws2_32",
				"dinput8",
				"Dsound",
				"Msacm32",
				"Dbghelp",
				"Winmm",
			}

			if use_d3d12 then
				includedirs { prjLoc .. "/D3D12/" }
				links { "d3d12", "dxgi", "dxguid" }
				files { prjLoc .. "/D3D12/*" }
				defines { "INTDEMO_RENDER_D3D12" }
			else
				links { "d3d11" }
				defines { "INTDEMO_RENDER_D3D11" }
			end

			filter "Debug*"
				links "CommunicationCentral"
			filter "Profile*"
				links "CommunicationCentral"
			filter {}

			-- Custom build step to compile HLSL into header files
			files { prjLoc .. "/Shaders/*.hlsl" }
			shaderobjectfileoutput ""
			shadermodel "5.0"

			filter 'files:**.hlsl'
				flags "ExcludeFromBuild"
			filter 'files:**Vs.hlsl'
				removeflags "ExcludeFromBuild"
				shadertype "Vertex"
				shaderentry "VsMain"
				shaderheaderfileoutput ("" .. autoGenLoc .. "%{file.basename}.h")
			filter 'files:**Ps.hlsl'
				removeflags "ExcludeFromBuild"
				shadertype "Pixel"
				shaderentry "PsMain"
				shaderheaderfileoutput ("" .. autoGenLoc .. "%{file.basename}.h")
			filter{}
		end,
		SoundEngineDllProject = function(prj)
			libdirs{"$(OutDir)../../$(Configuration)(StaticCRT)/lib"}
			staticruntime "On"
		end,
		ExternalPlugin = function(prj)
			removeflags { "FatalWarnings" }
		end,
	},

	Exclusions = {
		GameSimulator = function(prjLoc)
			excludes {
				prjLoc .. "/../../src/libraries/Common/UniversalScrollBuffer.*"
			}
		end,
		IntegrationDemo = function(projectLocation)
			excludes {
				projectLocation .. "../Common/stdafx.cpp"
			}
		end
	}
}
return AK.Platforms.Windows

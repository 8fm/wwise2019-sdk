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

require "xboxone/xboxone"

AK.Platforms.XboxOne =
{
	name = "XboxOne",
	directories = {
		src = {
			__default__ = "XboxOne",
			AkMusicEngine = "Win32",
			AkMemoryMgr = "Win32",
			GameSimulator = "Win32",
			AkSink = "Win32",
			AkVorbisDecoder = "Win32",
			AkSoundEngineDLL = "Win32",
			AkStreamMgr = "Win32",
		},
		simd = "SIMD",
		project = "XboxOne",
		lualib = "Win32",
		lowlevelio = "Win32",
		luasln = "GameSimulator/source/",
	},
	kinds = {
		GameSimulator = "ConsoleApp",
		IntegrationDemo = "WindowedApp"
	},
	suffix = {
		__default__ = "XboxOne",
		IntegrationDemoSln = "",
		IntegrationDemoMotionSln = "",
		IntegrationDemo = "",
		IntegrationDemoMotion = ""
	},

	platforms = { "Durango" },
	avxarchs = { "Durango" },
	features = { "Motion", "iZotope", "IntegrationDemo" },	
	configurations =
	{
		"Debug",
		"Debug_NoIteratorDebugging",
		"Profile",
		"Profile_EnableAsserts",
		"Release",
	},
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

			platforms {"Durango"}
			system(premake.XBOXONE)

			location(AkRelativeToCwd(in_projectLocation))
			targetname(in_targetName)
			targetlayoutdir("$(OutDir)..\\Layout")
			syslibdirs { "$(Console_SdkLibPath)" }
			sysincludedirs { "$(Console_SdkIncludeRoot)\\um;$(Console_SdkIncludeRoot)\\shared;$(Console_SdkIncludeRoot)\\winrt" }
			bindirs { "$(Console_SdkRoot)\\bin;$(VCInstallDir)bin\\x86_amd64;$(VCInstallDir)bin;$(WindowsSDK_ExecutablePath_x86);$(VSInstallDir)Common7\\Tools\\bin;$(VSInstallDir)Common7\\tools;$(VSInstallDir)Common7\\ide;$(ProgramFiles)\\HTML Help Workshop;$(MSBuildToolsPath32);$(FxCopDir);$(PATH);" }

			if in_targetType == nil or in_targetType == "StaticLib" then
				kind("StaticLib")
				targetprefix("")
				targetextension(".lib")
				-- Add to global table
				_AK_TARGETS[in_targetName] = in_fileName
			else
				kind(in_targetType)
				targetprefix("")
				targetextension(".dll")
				libdirs{"$(OutDir)../lib",}
				links{"combase", "kernelx", "uuid"}
			end
			language("C++")
			uuid(GenerateUuid(in_fileName))
			filename(in_fileName)
			symbols "on"
			symbolspath "$(OutDir)$(TargetName).pdb"
			flags {
				-- Treat all warnings as errors
				"FatalWarnings",
				-- We never want .user files, we always want .filters files.
				"OmitUserFiles",
				"ForceFiltersFiles"
			}

			-- Common flags.
			linksparent "Disabled"

			characterset "Unicode"
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
			end

			-- Standard configuration settings.
			filter "*Debug*"
				defines "_DEBUG"

			filter "Profile*"
				defines "NDEBUG"
				optimize "Speed"

			filter "*Release*"
				defines "NDEBUG"
				optimize "Speed"

			if not _AK_BUILD_AUTHORING then
				filter "Release*"
					defines ("AK_OPTIMIZED")
			end

			filter "Debug_NoIteratorDebugging"
				defines "_HAS_ITERATOR_DEBUGGING=0"

			filter "*EnableAsserts"
				defines "AK_ENABLE_ASSERTS"

			-- Style sheets.
			local ssext = ".props"
			if in_targetType == nil or in_targetType == "StaticLib" then
				filter "*Debug*"
					vs_propsheet(AkRelativeToCwd(_AK_ROOT_DIR) .. "PropertySheets/XboxOne/Debug" .. in_suffix .. ssext)
				filter "Profile* or *Release*"
					vs_propsheet(AkRelativeToCwd(_AK_ROOT_DIR) .. "PropertySheets/XboxOne/NDebug" .. in_suffix .. ssext)
				DisablePropSheetElements()
			end
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
		local prj = AK.Platforms.XboxOne.CreateProject(in_fileName, in_targetName, in_projectLocation, in_suffix, pathPCH, in_targetType)
		return prj
	end,

	Exceptions = {
		AkSoundEngine = function(prj)
			local prjLoc = AkRelativeToCwd(prj.location)
			if not _AK_BUILD_AUTHORING then
				filter { "files:" .. prjLoc .. "/../../SoundEngineProxy/**.cpp", "Release*" }
					flags { "ExcludeFromBuild" }
				filter {}
			end
			includedirs {
				prjLoc .. "/../Win32"
			}
			files {
				prjLoc .. "/../Win32/*.cpp",
				prjLoc .. "/../*.h",
			}
		end,		
		AkSoundEngineDLL = function(prj)
			links {"MMDevApi", "d3d11_x", "ws2_32", "xaudio2", "acphal"}
		end,
		AkOpusDecoder = function(prj)
			local prjLoc = AkRelativeToCwd(prj.location)
			includedirs { 	
				prjLoc .. "/../../../AkAudiolib/Win32",
			}
		end,		
		AkMotionSink = function(prj)
			flags { "WinRT" }
			buildoptions { "/wd4530" } -- C++ exception handler used, but unwind semantics are not enabled.
			linkoptions { "/ignore:4264" } -- Occurs when producing a WinRT static lib
			filter "*"
				optimize "Full"
			filter {}
		end,
		MasteringSuiteFX = function(prj)
			defines {"SCE_AUDIO_MASTER_USE_AK_SIMD"}
		end,
		AkSink = function(prj)
			local prjLoc = AkRelativeToCwd(prj.location)
			includedirs {
				prjLoc .. "/../../../../AkAudiolib/Win32"
			}
		end,
		AkSpatialAudio = function(prj)
			local prjLoc = AkRelativeToCwd(prj.location)
			includedirs { 
				prjLoc .. "/../Win32",
				prjLoc .. "/../../SoundEngine/AkAudiolib/Win32",
			}
			files {
				prjLoc .. "/../Win32/*.cpp",
				prjLoc .. "/../Win32/*.h",
			}
		end,
		AkStreamMgr = function(prj)
			local prjLoc = AkRelativeToCwd(prj.location)
			-- We add XboxOne here so it comes before the Win32 include
			includedirs {
				prjLoc .. "/../XboxOne",
			}
			files {
				prjLoc .. "/../XboxOne/*.cpp",
				prjLoc .. "/../XboxOne/*.h",
			}
		end,
		AkVorbisDecoder = function(prj)			
			local prjLoc = AkRelativeToCwd(prj.location)
			includedirs { 
				prjLoc .. "/../../../AkAudiolib/XboxOne" ,					
			}			
		end,
		CommunicationCentral = function(prj)
			local prjLoc = AkRelativeToCwd(prj.location)
			includedirs { 
				prjLoc .. "/../Win32"
			}
			files {
				prjLoc .. "/../PC/*.cpp",
				prjLoc .. "/../PC/*.h",
			}
		end,
		PluginFactory = function(prj)
			local prjLoc = AkRelativeToCwd(prj.location)
			includedirs {
				prjLoc .. "/../../../../AkAudiolib/Win32",
			}
		end,
		iZTrashBoxModelerFX = function(prj)
			local prjLoc = AkRelativeToCwd(prj.location)
			files {
				prjLoc .. "/../../../../iZBaseConsole/src/iZBase/Util/CriticalSection.*",
			}
		end,
		LuaLib = function(prj)
			buildoptions {
				"/wd4334" -- Harmless 32bit-to-64bit bitshift warning in third-party code
			}
		end,
		GameSimulator = function(prj)
			local prjLoc = AkRelativeToCwd(prj.location)
			local suffix = GetSuffixFromCurrentAction()
			local integrationDemoLocation = prjLoc .. "/../../../../SDK/samples/IntegrationDemo"
			local autoGenLoc = '%{cfg.objdir}/AutoGen/'

			defines {
				"_CRT_SECURE_NO_WARNINGS",
			}

			flags {
				"WinRT",
			}

			includedirs {
				prjLoc .. "/../../../../SDK/samples/Motion/Win32",
				autoGenLoc,
				integrationDemoLocation .. "/FreetypeRenderer",
				integrationDemoLocation .. "/FreetypeRenderer/Assets",
			}
			
			files {
				prjLoc .. "/Package.appxmanifest",
				prjLoc .. "/*.png",
				prjLoc .. "/../../src/libraries/Win32/*.cpp",
				integrationDemoLocation .. "/Common/Drawing.*",
				integrationDemoLocation .. "/FreetypeRenderer/*",
				integrationDemoLocation .. "/FreetypeRenderer/Assets/*",
				integrationDemoLocation .. "/XboxOne/InputMgr.*",
				integrationDemoLocation .. "/Windows/Render_D3D12.cpp",
				integrationDemoLocation .. "/Windows/D3D12/*",
			}

			links {
				"combase",
				"d3d12_x",
				"etwplus",
				"kernelx",
				"ws2_32",
				"xaudio2",
				"xg_x",
				"acphal",
				"D3DCompiler",
				"MMDevApi",

				"iZHybridReverbFX",
				"iZTrashBoxModelerFX",
				"iZTrashDelayFX",
				"iZTrashDistortionFX",
				"iZTrashDynamicsFX",
				"iZTrashFiltersFX", 
				"iZTrashMultibandDistortionFX",

				"LuaLib",
				"ToLuaLib"
			}
			filter "Debug*"
				libdirs {prjLoc .. "/../../../../SDK/XboxOne" .. suffix .. "/Debug/lib"}
			filter "Profile*"
				libdirs {prjLoc .. "/../../../../SDK/XboxOne" .. suffix .. "/Profile/lib"}
			filter "Release*"
				libdirs {prjLoc .. "/../../../../SDK/XboxOne" .. suffix .. "/Release/lib"}
			filter {}

			vs_propsheet(prjLoc .. "/GameSimulator.props")

			-- Custom build step to compile HLSL into header files
			files { integrationDemoLocation .. "/Windows/Shaders/*.hlsl" }
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
		IntegrationDemoMotion = function(prj)
			AK.Platforms.XboxOne.Exceptions.IntegrationDemo(prj, "Motion")
		end,
		IntegrationDemo = function(prj, in_kind)
			if in_kind == nil then in_kind = "" end
			local prjLoc = AkRelativeToCwd(prj.location)
			local autoGenLoc = '%{cfg.objdir}/AutoGen/'

			includedirs {
				prjLoc .. "/../../../samples/SoundEngine/Win32",
				autoGenLoc
			}
			files {
				prjLoc .. "/../../SoundEngine/Common/AkDefaultLowLevelIODispatcher.*",
				prjLoc .. "/../../SoundEngine/Common/AkFilePackageLowLevelIO.*",
				prjLoc .. "/../../SoundEngine/Win32/AkFileHelpers.h",
				prjLoc .. "/../Windows/Render_D3D12.cpp",
				prjLoc .. "/../Windows/D3D12/*",
				prjLoc .. "/*.png",
				prjLoc .. "/Package" .. in_kind .. ".appxmanifest",
			}
			defines {
				"MIN_EXPECTED_XDK_VER=_XDK_VER",
				"MAX_EXPECTED_XDK_VER=_XDK_VER",
				"_DURANGO",
				"WINAPI_FAMILY=WINAPI_FAMILY_TV_TITLE",
				"_CRT_SECURE_NO_WARNINGS",
			}
			links {
				"acphal.lib",
				"d3d12_x.lib",
				"D3DCompiler.lib",
				"dxguid.lib",
				"MMDevApi.lib",
				"ws2_32.lib",
				"xaudio2.lib",
			}
			filter "Debug*"
				links "CommunicationCentral"
			filter "Profile*"
				links "CommunicationCentral"
			filter {}
			
			flags {
				"NoGenerateManifest",
			}
			vs_propsheet(prjLoc .. "/IntegrationDemo" .. in_kind .. ".props")
			
			-- Custom build step to compile HLSL into header files
			files { prjLoc .. "/../Windows/Shaders/*.hlsl" }
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

			local actionsuffix = GetSuffixFromCurrentAction()

			libdirs {
				prjLoc .. "/../../../XboxOne" .. actionsuffix .. "/$(Configuration)/lib"
			}

			-- specify output layout directory ()
			targetlayoutdir("$(OutDir)..\\..\\Layout\\IntegrationDemo" .. in_kind .. "")
			
			project(prj.name)
		end,
		ExternalPlugin = function(prj)
			removeflags { "FatalWarnings" }
		end,
	},

	Exclusions = {
		AkSoundEngine = function(prjLoc)
			excludes { 
				prjLoc .. "../Win32/stdafx.cpp",
				prjLoc .. "../Win32/stdafx.h",
				prjLoc .. "../Win32/AkSinkDirectSound.cpp",
				prjLoc .. "../Win32/AkSinkDirectSound.h",
				prjLoc .. "../Win32/AkLEngine.cpp",
				prjLoc .. "../Win32/AkLEngine.h",
			}
		end,
		AkSpatialAudio = function(prjLoc)
			excludes { 
				prjLoc .. "/../Win32/stdafx.cpp",
				prjLoc .. "/../Win32/stdafx.h",
			}
		end,
		AkStreamMgr = function(prjLoc)
			excludes { 
				prjLoc .. "/../Win32/stdafx.cpp",
				prjLoc .. "/../Win32/stdafx.h",
			}
		end,
		CommunicationCentral = function(prjLoc)
			excludes { 
				prjLoc .. "/../PC/stdafx.cpp",
				prjLoc .. "/../PC/stdafx.h",
			}
		end,
		IntegrationDemo = function(prjLoc)
			excludes {
				prjLoc .. "/../Common/SoundInputMgrBase.*",
				prjLoc .. "/../Common/stdafx.*",
				prjLoc .. "/../DemoPages/DemoMicrophone.*",
				prjLoc .. "/SoundInput.*",
				prjLoc .. "/SoundInputMgr.*",
			}
		end
	}
}
return AK.Platforms.XboxOne

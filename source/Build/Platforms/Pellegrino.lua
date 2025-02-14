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

require "pellegrino/pellegrino"

AK.Platforms.Pellegrino =
{
	name = "Pellegrino",
	directories = {
		src = {
			__default__ = "Pellegrino"
		},
		project = "Pellegrino",
		simd = "SIMD",
		lualib = "Pellegrino",
		lowlevelio = "Pellegrino",
		luasln = "GameSimulator/source/",
	},
	kinds = {
		GameSimulator = "ConsoleApp",
		IntegrationDemo = "WindowedApp"
	},
	suffix = "Pellegrino",

	platforms = { "Prospero" },
	avxarchs = {"Prospero"},
	avx2archs = {"Prospero"},
	configurations =
	{
		"Debug",
		"Profile",
		"Profile_EnableAsserts",
		"Release",
	},
	features = { "iZotope", "UnitTests", "sceAudio3d", "Motion", "IntegrationDemo" },
	validActions =  { "vs2017", "vs2019" },
		
	AdditionalSoundEngineProjects = function()
		return {}
	end,

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

			platforms {"Prospero"}
			system "pellegrino"
			characterset "MBCS"
			toolset "clang"
			symbols "On"

			location(AkRelativeToCwd(in_projectLocation))
			targetname(in_targetName)

			if in_targetType == nil or in_targetType == "StaticLib" then
				kind("StaticLib")
				-- Add to global table
				_AK_TARGETS[in_targetName] = in_fileName
			else
				kind(in_targetType)
				libdirs{"$(OutDir)../lib",}
			end
			language("C++")

			uuid(GenerateUuid(in_fileName))
			filename(in_fileName)
			flags { "OmitUserFiles", "ForceFiltersFiles" }		-- We never want .user files, we always want .filters files.

			-- Common flags.
			filter {}
				defines("AUDIOKINETIC")
			

			-- Precompiled headers.
			if pathPCH ~= nil then
				files
				{
					AkRelativeToCwd(pathPCH) .. "stdafx.cpp",
					AkRelativeToCwd(pathPCH) .. "stdafx.h",
				}
				pchheader "stdafx.h"
				pchsource ( AkRelativeToCwd(pathPCH) .. "stdafx.cpp" )
			end

			buildoptions
			{
				"-Wno-invalid-offsetof",
				"-Wno-switch",
				"-Wno-unused-variable",
				"-Wno-unused-function",
				"-Wno-missing-braces",
				"-Wno-unused-value",
			}

			-- Standard configuration settings.
			filter "*Debug*"
				defines ("_DEBUG")
				vs_propsheet(AkRelativeToCwd(_AK_ROOT_DIR) .. "PropertySheets/Pellegrino/Pellegrino.props")

			filter "Profile*"
				defines ("NDEBUG")
				optimize ("Speed")
				vs_propsheet(AkRelativeToCwd(_AK_ROOT_DIR) .. "PropertySheets/Pellegrino/Pellegrino.props")

			filter "*Release*"
				defines ("NDEBUG")
				optimize ("Speed")
				vs_propsheet(AkRelativeToCwd(_AK_ROOT_DIR) .. "PropertySheets/Pellegrino/Pellegrino.props")

			DisablePropSheetElements()
			filter {}
				removeelements {
					"TargetExt"
				}

			if not _AK_BUILD_AUTHORING then
				filter "Release*"
					defines ("AK_OPTIMIZED")
			end

			filter "*EnableAsserts"
				defines( "AK_ENABLE_ASSERTS" )

			-- Set the scope back to current project
			project(in_targetName)

		ApplyPlatformExceptions(prj.name, prj)

		return prj
	end,

	-- Plugin factory.
	-- Upon returning from this method, the current scope is the newly created plugin project.
	CreatePlugin = function(in_fileName, in_targetName, in_projectLocation, in_suffix, pathPCH, in_targetType)
		local prj = AK.Platforms.Pellegrino.CreateProject(in_fileName, in_targetName, in_projectLocation, in_suffix, pathPCH, in_targetType)
		return prj
	end,

	Exceptions = {
		AkMotionSink = function(prj)
			g_PluginDLL["AkMotion"].extralibs = {
				"SceUserService_stub_weak",
				"SceAudioOut2_stub_weak",
				"ScePad_stub_weak",
				"SceNet_stub_weak",
				"SceNetCtl_stub_weak",
				"SceAjm_stub_weak",
				"ScePosix_stub_weak",
			}
		end,

		-- AK_PELLEGRINO_TODO Revisit this
		-- WG-36323 The reported bug.  The solution is the same as WG-34226 (see below)
		-- WG-34226 The compiler on PS4 replaces VDIV (division instruction) with VRCP and VMUL (low-precision reciprocal followed by mul)
		-- This causes serious trouble in the vocoder algorithm.  Prevent this optimization.
		AkGuitarDistortionFX = function(prj)
			buildoptions{"-fno-reciprocal-math"}
		end,
		AkTimeStretchFX = function(prj)
			buildoptions{"-fno-reciprocal-math"}
		end,
		AkHarmonizerFX = function(prj)
			buildoptions{"-fno-reciprocal-math"}
		end,
		AkConvolutionReverbFX = function(prj)
			g_PluginDLL["AkConvolutionReverb"].extralibs = {"SceAcm_stub_weak"}
			files
			{
				prj.location ..  "/../../../Common/ConvolutionEngine/Pellegrino/AkAcmConvolutionEngine.*"
			}
		end,
		AkOpusDecoder = function(prj)
			defines { "AK_DECLARE_FSEEKO_FTELLO" } -- fseeko, ftello used by opusfile are POSIX, not standard C, and not declared in PS4's standard library headers
		end,
		SceAudio3dEngine = function(prj)
			g_PluginDLL["SceAudio3d"].extralibs = {
				"SceAudioOut2_stub_weak",
				"SceSysmodule_stub_weak"
			}

			local prjLoc = AkRelativeToCwd(prj.location)
			includedirs { 
				prjLoc .. "/../Pellegrino",
			}
			files 
			{
				prjLoc .. "/../Pellegrino/*.cpp", 
				prjLoc .. "/../Pellegrino/*.h",
			}
		end,
		MasteringSuiteFX = function(prj)
			g_PluginDLL["MasteringSuite"].extralibs = {
				"SceAudioOut2_stub_weak"
			}
			defines {"SCE_AUDIO_MASTER_USE_AK_SIMD"}
		end,
		AkSoundEngineTests = function(prj)
			defines {"CATCH_CONFIG_NO_POSIX_SIGNALS", "CATCH_CONFIG_COLOUR_NONE"}

			local prjLoc = AkRelativeToCwd(prj.location)
			libdirs {
				prjLoc .. "/../../../../Pellegrino/$(Configuration)/lib",
				"$(SCE_PROSPERO_SDK_DIR)/target/lib"
			}

			links {
				"SceUserService_stub_weak",
				"SceAudioOut2_stub_weak",
				"ScePad_stub_weak",
				"SceNet_stub_weak",
				"SceNetCtl_stub_weak",
				"SceAjm_stub_weak",
				"ScePosix_stub_weak",
				"SceSysmodule_stub_weak",
			}
		end,

		IntegrationDemoMotion = function(prj)
			-- Apply same exceptions as IntegrationDemo
			AK.Platforms.Pellegrino.Exceptions.IntegrationDemo(prj)
		end,

		IntegrationDemo = function(prj)
			local prjLoc = AkRelativeToCwd(prj.location)
			local autoGenLoc = '%{cfg.objdir}/AutoGen/'
			includedirs { autoGenLoc }
			libdirs {
				prjLoc .. "/../../../Pellegrino/$(Configuration)/lib",
				"$(SCE_PROSPERO_SDK_DIR)/target/lib"
			}

			filter "*Debug*"
				links "CommunicationCentral"
			filter "*Profile*"
				links "CommunicationCentral"
			filter {}

			links {
				"SceAcm_stub_weak",
				"SceAgc",
				"SceAgc_stub_weak",
				"SceAgcCore",
				"SceAgcDriver_stub_weak",
				"SceAgcGpuAddress",
				"SceAjm_stub_weak",
				"SceAmpr",
				"SceAudioOut2_stub_weak",
				"SceNet_stub_weak",
				"SceNetCtl_stub_weak",
				"ScePad_stub_weak",
				"ScePosix_stub_weak",
				"SceSysmodule_stub_weak",
				"SceUserService_stub_weak",
				"SceVideoOut_stub_weak"
			}
			
			-- Custom build step to compile vertex/pixel PSSL shaders into binary ELFs, and then convert them to relocatable (and linkable) ELFs
			files
			{
				prjLoc .. "/*.pssl"
			}
			filter 'files:**_vp.pssl'
				buildmessage 'Compiling vertex/pixel PSSL file %{file.relpath}'
				buildcommands 
				{
					'prospero-wave-psslc -profile sce_vs_vs_prospero %{file.relpath} -entry VsMain -o %{cfg.objdir}/%{file.basename}_vv.ags.o',
					'prospero-wave-psslc -profile sce_ps_prospero    %{file.relpath} -entry PsMain -o %{cfg.objdir}/%{file.basename}_p.ags.o ',
					'prospero-elfedit --output-type rel --output-mach x86_64 %{cfg.objdir}/%{file.basename}_vv.ags.o ',
					'prospero-elfedit --output-type rel --output-mach x86_64 %{cfg.objdir}/%{file.basename}_p.ags.o ',
				}
				buildoutputs { '%{cfg.objdir}/%{file.basename}_vv.ags.o', '%{cfg.objdir}/%{file.basename}_p.ags.o' }
			filter 'files:**_c.pssl'
				buildmessage 'Compiling compute PSSL file %{file.relpath}'
				buildcommands 
				{
					'prospero-wave-psslc -profile sce_cs_prospero %{file.relpath} -entry CsMain -o %{cfg.objdir}/%{file.basename}_c.ags.o',
					'prospero-elfedit --output-type rel --output-mach x86_64 %{cfg.objdir}/%{file.basename}_c.ags.o ',
				}
				buildoutputs { '%{cfg.objdir}/%{file.basename}_c.ags.o' }
			filter{}
		end,

		GameSimulator = function(prj)
			local prjLoc = AkRelativeToCwd(prj.location)
			local integrationDemoLocation = prjLoc .. "/../../../../SDK/samples/IntegrationDemo"
			local autoGenLoc = '%{cfg.objdir}/AutoGen/'
			includedirs { 
				autoGenLoc,
				integrationDemoLocation .. "/FreetypeRenderer",
				integrationDemoLocation .. "/FreetypeRenderer/Assets",
			}
			files {
				integrationDemoLocation .. "/Common/Drawing.*",
				integrationDemoLocation .. "/FreetypeRenderer/*",
				integrationDemoLocation .. "/FreetypeRenderer/Assets/*",
				integrationDemoLocation .. "/MenuSystem/UniversalInput.*",
				integrationDemoLocation .. "/Pellegrino/InputMgr.*",
				integrationDemoLocation .. "/Pellegrino/Render_AGC.cpp",
			}
			libdirs {
				prjLoc .. "/../../../Pellegrino/$(Configuration)/lib",
				prjLoc .. "/../../../../SDK/Pellegrino/$(Configuration)/lib",
			}

			filter "Debug*"
				libdirs	{ prjLoc .. "/../../../../SDK/Pellegrino/Debug/lib" }
			filter "Profile*"
				libdirs	{ prjLoc .. "/../../../../SDK/Pellegrino/Profile/lib" }
			filter "Release*"
				libdirs	{ prjLoc .. "/../../../../SDK/Pellegrino/Release/lib" }
			filter {}

			links {
				"LuaLib",
				"ToLuaLib",
				"SceAcm_stub_weak",
				"SceAgc",
				"SceAgc_stub_weak",
				"SceAgcCore",
				"SceAgcDriver_stub_weak",
				"SceAgcGpuAddress",
				"SceAjm_stub_weak",
				"SceAmpr",
				"SceAudioOut2_stub_weak",
				"SceJobManager_stub_weak",
				"SceNet_stub_weak",
				"SceNetCtl_stub_weak",
				"SceNgs2_stub_weak",
				"ScePad_stub_weak",
				"ScePosix_stub_weak",
				"SceSysmodule_stub_weak",
				"SceUserService_stub_weak",
				"SceVideoOut_stub_weak"
			}

			-- Custom build step to compile vertex/pixel PSSL shaders into binary ELFs, and then convert them to relocatable (and linkable) ELFs
			files
			{
				integrationDemoLocation .. "/Pellegrino/*.pssl"
			}
			filter 'files:**_vp.pssl'
				buildmessage 'Compiling vertex/pixel PSSL file %{file.relpath}'
				buildcommands 
				{
					'prospero-wave-psslc -profile sce_vs_vs_prospero %{file.relpath} -entry VsMain -o %{cfg.objdir}/%{file.basename}_vv.ags.o',
					'prospero-wave-psslc -profile sce_ps_prospero    %{file.relpath} -entry PsMain -o %{cfg.objdir}/%{file.basename}_p.ags.o ',
					'prospero-elfedit --output-type rel --output-mach x86_64 %{cfg.objdir}/%{file.basename}_vv.ags.o ',
					'prospero-elfedit --output-type rel --output-mach x86_64 %{cfg.objdir}/%{file.basename}_p.ags.o ',
				}
				buildoutputs { '%{cfg.objdir}/%{file.basename}_vv.ags.o', '%{cfg.objdir}/%{file.basename}_p.ags.o' }
			filter 'files:**_c.pssl'
				buildmessage 'Compiling compute PSSL file %{file.relpath}'
				buildcommands 
				{
					'prospero-wave-psslc -profile sce_cs_prospero %{file.relpath} -entry CsMain -o %{cfg.objdir}/%{file.basename}_c.ags.o',
					'prospero-elfedit --output-type rel --output-mach x86_64 %{cfg.objdir}/%{file.basename}_c.ags.o ',
				}
				buildoutputs { '%{cfg.objdir}/%{file.basename}_c.ags.o' }
			filter{}
			entrypoint("mainCRTStartup")
		end,
	},

	Exclusions = {
		GameSimulator = function(prjLoc)
			excludes {
				prjLoc .. "/../../src/libraries/Common/UniversalScrollBuffer.*"
			}
		end,
		IntegrationDemo = function(projectLocation)
		end
	}
}
return AK.Platforms.Pellegrino

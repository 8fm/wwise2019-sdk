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

require "ps4/ps4"

AK.Platforms.PS4 =
{
	name = "PS4",
	directories = {
		src = {
			__default__ = "PS4"
		},
		project = "PS4",
		simd = "SIMD",
		lualib = "PS4",
		lowlevelio = "PS4",
		luasln = "GameSimulator/source/",
	},
	kinds = {
		GameSimulator = "ConsoleApp",
		IntegrationDemo = "WindowedApp"
	},
	suffix = "PS4",

	platforms = { "ORBIS" },
	avxarchs = {"ORBIS"},
	configurations =
	{
		"Debug",
		"Profile",
		"Profile_EnableAsserts",
		"Release",
	},
	features = { "Motion", "iZotope", "sceAudio3d", "IntegrationDemo", "UnitTests"},
	validActions =  { "vs2015", "vs2017", "vs2019" },	
	
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

			platforms {"ORBIS"}
			system "ps4"
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
			flags {
				-- Treat all warnings as errors
				"FatalWarnings",
				-- We never want .user files, we always want .filters files.
				"OmitUserFiles",
				"ForceFiltersFiles"
			}

			-- Common flags.
			filter {}
				includedirs{"$(SCE_ORBIS_SDK_DIR)/target/include",
						"$(SCE_ORBIS_SDK_DIR)/target/include_common",
						"$(SCE_ORBIS_SDK_DIR)/host_tools/lib/clang/3.1/include"}

				defines ("SN_TARGET_ORBIS;__SCE__;AUDIOKINETIC")


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
				vs_propsheet(AkRelativeToCwd(_AK_ROOT_DIR) .. "PropertySheets/PS4/PS4.props")

			filter "Profile*"
				defines ("NDEBUG")
				optimize ("Speed")
				vs_propsheet(AkRelativeToCwd(_AK_ROOT_DIR) .. "PropertySheets/PS4/PS4.props")

			filter "*Release*"
				defines ("NDEBUG")
				optimize ("Speed")
				vs_propsheet(AkRelativeToCwd(_AK_ROOT_DIR) .. "PropertySheets/PS4/PS4.props")

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
		local prj = AK.Platforms.PS4.CreateProject(in_fileName, in_targetName, in_projectLocation, in_suffix, pathPCH, in_targetType)
		return prj
	end,

	Exceptions = {
		AkMotionSink = function(prj)
			g_PluginDLL["AkMotion"].extralibs = {
				"SceGnmx",
				"SceGnm",
				"SceGpuAddress",
				"SceShaderBinary",
				"ScePad_stub_weak",
				"SceGnf",
				"ScePosix_stub_weak",
				"SceAjm_stub_weak",
				"SceMove_stub_weak",
				"sceuserservice_stub_weak",
				"SceSysmodule_stub_weak"
			}
		end,
		
		iZTrashBoxModelerFX = function(prj)
			files {
				AkRelativeToCwd(prj.location) .. "/../../../../iZBaseConsole/src/iZBase/Util/PS4/CriticalSection_PS4.*",
			}
		end,

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
		MasteringSuiteFX = function(prj)
			defines {"SCE_AUDIO_MASTER_USE_AK_SIMD"}
		end,
		AkOpusDecoder = function(prj)
			defines { "AK_DECLARE_FSEEKO_FTELLO" } -- fseeko, ftello used by opusfile are POSIX, not standard C, and not declared in PS4's standard library headers
		end,
		SceAudio3dEngine = function(prj)
			g_PluginDLL["SceAudio3d"].extralibs = {
				"SceAudio3d_stub_weak",
				"SceSysmodule_stub_weak"
			}
			
			local prjLoc = AkRelativeToCwd(prj.location)
			includedirs { 
				prjLoc .. "/../PS4",
			}
			files 
			{
				prjLoc .. "/../PS4/*.cpp", 
				prjLoc .. "/../PS4/*.h",
			}
		end,

		AkSoundEngineTests = function(prj)
			defines {"CATCH_CONFIG_NO_POSIX_SIGNALS", "CATCH_CONFIG_COLOUR_NONE"}

			local prjLoc = AkRelativeToCwd(prj.location)
			libdirs {
				prjLoc .. "/../../../../PS4/$(Configuration)/lib",
				"$(SCE_ORBIS_SDK_DIR)/target/lib"
			}

			links {
				"SceAjm_stub_weak",
				"SceAudioOut_stub_weak",
				"SceNet_stub_weak",
				"SceNetCtl_stub_weak",
				"ScePad_stub_weak",
				"ScePosix_stub_weak",
				"SceUserService_stub_weak",
			}
		end,

		IntegrationDemoMotion = function(prj)
			-- Apply same exceptions as IntegrationDemo
			AK.Platforms.PS4.Exceptions.IntegrationDemo(prj)
		end,

		IntegrationDemo = function(prj)
			local prjLoc = AkRelativeToCwd(prj.location)
			local autoGenLoc = '%{cfg.objdir}/AutoGen/'
			includedirs { autoGenLoc }
			libdirs {
				prjLoc .. "/../../../PS4/$(Configuration)/lib",
				"$(SCE_ORBIS_SDK_DIR)/target/lib"
			}

			filter "*Debug*"
				links "CommunicationCentral"
			filter "*Profile*"
				links "CommunicationCentral"
			filter {}

			links {
				"SceAjm_stub_weak",
				"SceAudioOut_stub_weak",
				"SceAudio3d_stub_weak",
				"SceFios2_stub_weak",
				"SceGnf",
				"SceGnm",
				"SceGnmDriver_stub_weak",
				"SceGnmx",
				"SceGpuAddress",
				"SceMove_stub_weak",
				"SceNet_stub_weak",
				"SceNetCtl_stub_weak",
				"ScePad_stub_weak",
				"ScePosix_stub_weak",
				"SceShaderBinary",
				"SceSysmodule_stub_weak",
				"SceUserService_stub_weak",
				"SceVideoOut_stub_weak",
			}
			
			-- Custom build step to compile vertex/pixel PSSL shaders into header files
			files
			{
				prjLoc .. "/*.pssl"
			}
			filter 'files:**_vp.pssl'
				buildmessage 'Compiling vertex/pixel PSSL file %{file.relpath}'
				buildcommands 
				{
					'orbis-wave-psslc -profile sce_vs_vs_orbis %{file.relpath} -entry VsMain -o ' .. autoGenLoc .. '%{file.basename}_vs.sb',
					'orbis-wave-psslc -profile sce_ps_orbis    %{file.relpath} -entry PsMain -o ' .. autoGenLoc .. '%{file.basename}_ps.sb',
					'orbis-bin2h ' .. autoGenLoc .. '%{file.basename}_vs.sb --output '.. autoGenLoc .. '%{file.basename}_vs.h --name g_VsMain --quiet',
					'orbis-bin2h ' .. autoGenLoc .. '%{file.basename}_ps.sb --output '.. autoGenLoc .. '%{file.basename}_ps.h --name g_PsMain --quiet',
				}
				buildoutputs { autoGenLoc .. '%{file.basename}_vs.h', autoGenLoc .. '%{file.basename}_ps.h' }
			filter 'files:**_c.pssl'
				buildmessage 'Compiling compute PSSL file %{file.relpath}'
				buildcommands 
				{
					'orbis-wave-psslc -profile sce_cs_orbis %{file.relpath} -entry CsMain -o ' .. autoGenLoc .. '%{file.basename}_cs.sb',
					'orbis-bin2h ' .. autoGenLoc .. '%{file.basename}_cs.sb --output '.. autoGenLoc .. '%{file.basename}_cs.h --name g_CsMain --quiet',
				}
				buildoutputs { autoGenLoc .. '%{file.basename}_cs.h'	}
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
				integrationDemoLocation .. "/PS4/InputMgr.*",
				integrationDemoLocation .. "/PS4/Render_GNM.cpp",
			}
			libdirs {
				prjLoc .. "/../../../PS4/$(Configuration)/lib",
				prjLoc .. "/../../../../SDK/PS4/$(Configuration)/lib",
				"$(SCE_ORBIS_SDK_DIR)/target/lib"
			}

			filter "Debug*"
				libdirs	{ prjLoc .. "/../../../../SDK/PS4/Debug/lib" }
			filter "Profile*"
				libdirs	{ prjLoc .. "/../../../../SDK/PS4/Profile/lib" }
			filter "Release*"
				libdirs	{ prjLoc .. "/../../../../SDK/PS4/Release/lib" }
			filter {}

			links {
				"LuaLib",
				"ToLuaLib",
				"SceAjm_stub_weak",
				"SceAudio3d_stub_weak",
				"SceAudioOut_stub_weak",
				"SceFios2_stub_weak",
				"SceGnf",
				"SceGnm",
				"SceGnmDriver_stub_weak",
				"SceGnmx",
				"SceGpuAddress",
				"SceMove_stub_weak",
				"SceNet_stub_weak",
				"SceNetCtl_stub_weak",
				"ScePad_stub_weak",
				"ScePosix_stub_weak",
				"SceShaderBinary",
				"SceSysmodule_stub_weak",
				"SceUserService_stub_weak",
				"SceVideoOut_stub_weak",
			}

			-- Custom build step to compile vertex/pixel PSSL shaders into header files
			files
			{
				integrationDemoLocation .. "/PS4/*.pssl"
			}
			filter 'files:**_vp.pssl'
				buildmessage 'Compiling vertex/pixel PSSL file %{file.relpath}'
				buildcommands 
				{
					'orbis-wave-psslc -profile sce_vs_vs_orbis %{file.relpath} -entry VsMain -o ' .. autoGenLoc .. '%{file.basename}_vs.sb',
					'orbis-wave-psslc -profile sce_ps_orbis    %{file.relpath} -entry PsMain -o ' .. autoGenLoc .. '%{file.basename}_ps.sb',
					'orbis-bin2h ' .. autoGenLoc .. '%{file.basename}_vs.sb --output '.. autoGenLoc .. '%{file.basename}_vs.h --name g_VsMain --quiet',
					'orbis-bin2h ' .. autoGenLoc .. '%{file.basename}_ps.sb --output '.. autoGenLoc .. '%{file.basename}_ps.h --name g_PsMain --quiet',
				}
				buildoutputs { autoGenLoc .. '%{file.basename}_vs.h', autoGenLoc .. '%{file.basename}_ps.h' }
			filter 'files:**_c.pssl'
				buildmessage 'Compiling compute PSSL file %{file.relpath}'
				buildcommands 
				{
					'orbis-wave-psslc -profile sce_cs_orbis %{file.relpath} -entry CsMain -o ' .. autoGenLoc .. '%{file.basename}_cs.sb',
					'orbis-bin2h ' .. autoGenLoc .. '%{file.basename}_cs.sb --output '.. autoGenLoc .. '%{file.basename}_cs.h --name g_CsMain --quiet',
				}
				buildoutputs { autoGenLoc .. '%{file.basename}_cs.h'	}
			filter{}
			entrypoint("mainCRTStartup")
		end,
		ExternalPlugin = function(prj)
			removeflags{ "FatalWarnings" }
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
return AK.Platforms.PS4

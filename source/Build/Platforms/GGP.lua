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

require "ggp/ggp"

AK.Platforms.GGP =
{
	name = "GGP",
	directories = {
		src = {
			__default__ = "GGP",
			CommunicationCentral = "POSIX",
			IntegrationDemo = "GGP"
		},
		simd = "SIMD",
		include = {
			AkMusicEngine = "POSIX"
		},
		project = "GGP",
		lualib = "GGP",
		lowlevelio = "POSIX",
		luasln = "GameSimulator/source/",
	},
	kinds = {
		GameSimulator = "ConsoleApp",
		IntegrationDemo = "WindowedApp"
    },
    suffix = {
		__default__ = "GGP",
		IntegrationDemo = "",
		--IntegrationDemoMotion = "",
	},
	configurations =
	{
		"Debug",
		"Profile",
		"Profile_EnableAsserts",
		"Release",
	},
	platforms = { "GGP" },
--	avxarchs = { "GGP" },
	features = { "POSIX", "UnitTests", "iZotope", "IntegrationDemo", "Motion" },
    validActions = { "vs2017" },
    
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
		local prj = project (in_targetName)
			location(AkRelativeToCwd(in_projectLocation))
			targetname(in_targetName)
			toolset("ggp_clang")
			if in_targetType == nil or in_targetType == "StaticLib" then
				kind("StaticLib")

				-- Add to global table
				_AK_TARGETS[in_targetName] = in_fileName
				defines("AKSOUNDENGINE_EXPORTS")
			else
				kind(in_targetType)
			end
			language("C++")
			cppdialect("C++11")
			uuid(GenerateUuid(in_fileName))
			filename(in_fileName)
			rtti("Off")
			exceptionhandling("Off")
			buildoptions { "-fPIC", "-fvisibility=hidden -Wno-invalid-offsetof" }

			configuration "*"
				flags { "MultiProcessorCompile"}

			-- Standard configuration settings.
			filter "*Debug*"
				defines ("_DEBUG")
				symbols ("On")

			filter "Profile*"
				defines ("NDEBUG")
				optimize ("Speed")
				symbols ("On")

			filter "*Release*"
				defines ({"NDEBUG","AK_OPTIMIZED"})
				optimize ("Speed")
				symbols ("On")

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
		local prj = AK.Platforms.GGP.CreateProject(in_fileName, in_targetName, in_projectLocation, in_suffix, pathPCH, in_targetType)
		return prj
	end,

	Exceptions = {
		AkSoundEngine = function(prj)
			local prjLoc = AkRelativeToCwd(prj.location)
			files {
				prjLoc .. "/../GGP/*.cpp",
				prjLoc .. "/../GGP/*.h",
				prjLoc .. "/../Linux/AkSink*.cpp",
				prjLoc .. "/../Linux/AkSink*.h",
				prjLoc .. "/../Linux/IDynamicAPI.h",
				prjLoc .. "/../Linux/PulseAudioAPI.*",
			}
			links {
				"libggp.so"
			}
		end,
		AkSoundEngineTests = function(prj)
			local prjLoc = AkRelativeToCwd(prj.location)
			links {
				"libasound.so",
				"libdl.so",
				"libpthread.so",
				"libggp.so"
			}
			libdirs { prjLoc .. "../../../../../GGP/$(Configuration)/lib" }
		end,
		AkStreamMgr = function(prj)
			filename("AkStreamMgr")
		end,
		-- AkMotionSink = function(prj)
		-- 	prj.location = prj.location .. "/../GGP/"
		-- end,
		AuroHeadphoneFX = function(prj)
			cppdialect "C++11"
		end,
		MasteringSuiteFX = function(prj)
			defines {"SCE_AUDIO_MASTER_USE_AK_SIMD"}
		end,
		Cube = function(prj)
			local prjLoc = AkRelativeToCwd(prj.location)
			local wwiseSdkDir = AkRelativeToCwd(prjLoc .. "/../../../../Wwise/SDK/")
			defines { "HAS_SOCKLEN_T" }
			links {
				"GL",
				"GLU",
				"libasound.so",
				"libpthread.so",
				"z"
			}
			libdirs { wwiseSdkDir .. "/GGP/$(Configuration)/lib" }
			filter "Debug*"
				links { "CommunicationCentral" }
			filter "Profile*"
				links { "CommunicationCentral" }
			filter "Release*"
			filter {}
		end,
		iZotopePluginFactory = function(prj)
			buildoptions { "-std=c++11" }
		end,
		GameSimulator = function(prj)
			local prjLoc = AkRelativeToCwd(prj.location)

			defines("LUA_USE_LINUX")
			files {
				prjLoc .. "/../../src/libraries/Win32/*.cpp"
			}
			libdirs	{
				prjLoc .. "/../../../GGP/$(Configuration)/lib",
				prjLoc .. "/../../../../SDK/GGP/$(Configuration)/lib"
			}
			links {
				"LuaLib",
				"ToLuaLib",
				"libasound.so",
				"libdl.so",
				"libpthread.so",
				"libggp.so"
			}
		end,
		LuaLib = function(prj)
			defines("LUA_USE_LINUX")
		end,
		ToLuaLib = function(prj)
			defines("LUA_USE_LINUX")
		end,
		IntegrationDemoMotion = function(prj)
		 	AK.Platforms.GGP.Exceptions.IntegrationDemo(prj, "Motion")
		end,
		IntegrationDemo = function(prj, in_kind)
			if in_kind == nil then in_kind = "" end
			local prjLoc = AkRelativeToCwd(prj.location)

			includedirs {
				prjLoc .. "/../FreetypeRenderer",
				prjLoc .. "/vulkan_utils",
				"$(GgpHeaderDir)"
			}
			files {
				prjLoc .. "/../FreetypeRenderer/*.cpp",
				prjLoc .. "/../FreetypeRenderer/*.h",
				prjLoc .. "/shaders/*.frag",
				prjLoc .. "/shaders/*.vert"

				-- Uncomment the following lines to debug the renderer using RenderDoc
				-- prjLoc .. "/vulkan_utils/*.h",
				-- prjLoc .. "/vulkan_utils/*.cpp",
				-- prjLoc .. "/vulkan_utils/*.c",
			}
			libdirs	{prjLoc .. "/../../../GGP/$(Configuration)/lib"}
			filter "Debug*"
				links "CommunicationCentral"
			filter "Profile*"
				links "CommunicationCentral"
			filter "Release*"
			filter {}
			links {
				"libasound.so",
				"libdl.so",
				"libpthread.so",
				"libvulkan.so",
				"libggp.so"
			}

			-- Set default Debugging launch properties
			vs_propsheet(prjLoc .. "/IntegrationDemo" .. in_kind .. ".props")

			-- Custom build step to convert shaders to header files
			filter "files:**.vert"
				buildmessage 'Compiling %{file.directory}/%{file.name}'
			    buildcommands '"glslangValidator.exe" -o "%{file.directory}/%{file.basename}_vert.h" -V "%{file.directory}%{file.name}" --vn "g_%{file.basename}_vert"'
			    buildoutputs "%{file.directory}/%{file.basename}_vert.h"

			filter "files:**.frag"
				buildmessage 'Compiling %{file.directory}/%{file.name}'
			    buildcommands '"glslangValidator.exe" -o "%{file.directory}/%{file.basename}_frag.h" -V "%{file.directory}%{file.name}" --vn "g_%{file.basename}_frag"'
			    buildoutputs "%{file.directory}/%{file.basename}_frag.h"
		    
		    filter{}
		end,
		SoundEngineDllProject = function(prj)
			linkoptions{"-Wl,--export-dynamic"}
			libdirs{AkRelativeToCwd(_AK_ROOT_DIR .. "../../GGP//$(Configuration)/lib")}
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
return AK.Platforms.GGP

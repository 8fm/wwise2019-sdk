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

AK.Platforms.Linux =
{
	name = "Linux",
	directories = {
		src = {
			__default__ = "Linux",
			CommunicationCentral = "POSIX"
		},
		simd = "SIMD",
		include = {
			AkMusicEngine = "POSIX"
		},
		project = "Linux",
		lualib = "Linux",
		lowlevelio = "POSIX",
		luasln = "GameSimulator/source/",
	},
	kinds = {
		GameSimulator = "ConsoleApp",
		IntegrationDemo = "WindowedApp"
	},
	suffix = {
		__default__ = "Linux",
		IntegrationDemo = "",
		IntegrationDemoMotion = "",
		AllEffectsSln = "Linux"
	},

	configurations =
	{
		"Debug",
		"Profile",
		"Profile_EnableAsserts",
		"Release",
	},
	features = { "POSIX", "UnitTests", "iZotope", "IntegrationDemo" },
	validActions = { "gmake" },
	
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
			toolset "clang"
			system "linux"

			location(AkRelativeToCwd(in_projectLocation))
			targetname(in_targetName)
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

			if not _AK_BUILD_AUTHORING then
			-- Note: The AuthoringRelease config is "profile", really. It must not be AK_OPTIMIZED.
			filter "Release*"
				defines "AK_OPTIMIZED"
			end

			-- Standard configuration settings.
			filter "*Debug*"
				defines ("_DEBUG")
				symbols ("On")

			filter "Profile*"
				defines ("NDEBUG")
				optimize ("Speed")
				symbols ("On")

			filter "*Release*"
				defines ({"NDEBUG"})
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
		local prj = AK.Platforms.Linux.CreateProject(in_fileName, in_targetName, in_projectLocation, in_suffix, pathPCH, in_targetType)
		return prj
	end,

	Exceptions = {
		AkSoundEngineTests = function(prj)
			links {
				"SDL2",
				"dl",
				"pthread",
			}
		end,
		AkStreamMgr = function(prj)
			filename("AkStreamMgr")
		end,
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
				"SDL2",
				"pthread",
				"z",
			}
			filter "Debug*"
				libdirs { wwiseSdkDir .. "/Linux_$(AK_LINUX_ARCH)/Debug/lib" }
				links { "CommunicationCentral" }
			filter "Profile*"
				libdirs { wwiseSdkDir .. "/Linux_$(AK_LINUX_ARCH)/Profile/lib" }
				links { "CommunicationCentral" }
			filter "Release*"
				libdirs { wwiseSdkDir .. "/Linux_$(AK_LINUX_ARCH)/Release/lib" }
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
			filter "*Debug*"
				libdirs	{
					prjLoc .. "/../../../Linux_$(AK_LINUX_ARCH)/Debug/lib",
					prjLoc .. "/../../../../SDK/Linux_$(AK_LINUX_ARCH)/Debug/lib"
				}
			filter "*Profile*"
				libdirs	{
					prjLoc .. "/../../../Linux_$(AK_LINUX_ARCH)/Profile/lib",
					prjLoc .. "/../../../../SDK/Linux_$(AK_LINUX_ARCH)/Profile/lib"
				}
			filter "*Release*"
				libdirs	{
					prjLoc .. "/../../../Linux_$(AK_LINUX_ARCH)/Release/lib",
					prjLoc .. "/../../../../SDK/Linux_$(AK_LINUX_ARCH)/Release/lib"
				}
			filter {}	
			links {
				"LuaLib",
				"ToLuaLib",
				"SDL2",
				"dl",
				"pthread",
				"c",
				"rt",
			}
		end,
		LuaLib = function(prj)
			defines("LUA_USE_LINUX")
		end,
		ToLuaLib = function(prj)
			defines("LUA_USE_LINUX")
		end,
		IntegrationDemoMotion = function(prj)
			AK.Platforms.Linux.Exceptions.IntegrationDemo(prj)
		end,
		IntegrationDemo = function(prj)
			local prjLoc = AkRelativeToCwd(prj.location)

			filter "Debug*"
				libdirs	{prjLoc .. "/../../../Linux_$(AK_LINUX_ARCH)/Debug/lib"}
				links "CommunicationCentral"
			filter "Profile*"
				libdirs	{prjLoc .. "/../../../Linux_$(AK_LINUX_ARCH)/Profile/lib"}
				links "CommunicationCentral"
			filter "Release*"
				libdirs	{prjLoc .. "/../../../Linux_$(AK_LINUX_ARCH)/Release/lib"}
			filter {}
			links {
				"SDL2",
				"dl",
				"pthread"
			}
		end,
		SoundEngineDllProject = function(prj)
			linkoptions{"-Wl,--export-dynamic"}
			filter "Debug*"
				libdirs{AkRelativeToCwd(_AK_ROOT_DIR .. "../../Linux_$(AK_LINUX_ARCH)/Debug/lib")}
			filter "Profile*"
				libdirs{AkRelativeToCwd(_AK_ROOT_DIR .. "../../Linux_$(AK_LINUX_ARCH)/Profile/lib")}
			filter "Release*"
				libdirs{AkRelativeToCwd(_AK_ROOT_DIR .. "../../Linux_$(AK_LINUX_ARCH)/Release/lib")}
			filter {}
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
return AK.Platforms.Linux

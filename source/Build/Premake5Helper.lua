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

-- This module requires Premake5 (Lua 5.3)
--

-- Creates a solution, with given configurations, and builds the list of projects passed in.
-- Note: Configurations MUST be created before projects. This helper is the preferred way of creating solutions.
-- Returns a variable to the newly created solution.
-- Arguments:
-- in_slnName		: solution (file) name
-- in_slnPath		: solution path
-- in_platformName	: platform
-- in_configurations: table of configuration names
-- in_suffix		: IDE specific suffix
-- in_projects		: List of project factory functions. After having created the solution and configurations, each project factory is invoked.
function CreateSolution(in_slnName, in_slnPath, in_platformName, in_configurations, in_suffix, in_projects)

	-- Create solution.
	verbosef("    Creating workspace %s", in_slnName)
	local wks = workspace(in_slnName)
		location(AkRelativeToCwd(in_slnPath))
		flags {"DisallowMakefile"}
		local cfgplatforms = AK.Platform.platforms
		if type(cfgplatforms) == 'function' then
			cfgplatforms()
		else
			platforms(cfgplatforms)
		end


	-- Create configs.
	-- From Premake documentation :
	-- The list of configurations must be specified before any projects are defined,
	-- and once a project has been defined the configuration list may no longer be changed.
	CreateConfigs(in_platformName, in_configurations, in_suffix)

	-- Create projects.
	CreateProjects(in_projects, in_platformName, in_suffix)

	workspace(in_slnName)

	return wks
end

-- Creates a solution filter (for projects)
-- Solution filters are seen as empty projects by Premake.
-- Call within the scope of a solution.
-- @param name - Filter name
function CreateSolutionFilter(name)
	if _AK_BUILD_AUTHORING ~= nil then
		name = "SDK/" .. name
	end
	group(name)
	group ""
end

-- Overrides the current project to use this solution filter
function SetProjectToSolutionFilter(name)
	if _AK_BUILD_AUTHORING ~= nil then
		name = "SDK/" .. name
	end
	project().group = name
end

-- Sub helpers.
function CreateConfigs(platformName, in_configurations, in_suffix)

	configurations(in_configurations)

	-- By default, root is 2 folders above premake path (that is, in SDK/).
	-- If a target requires a different path, call SetLibOutputDirectory again with proper values.
	local root = _AK_ROOT_DIR .. "../../"
	local hasFastcall = false
	for _,cfg in pairs(in_configurations) do
		filter (cfg)
			SetLibOutputDirectory(root, platformName, cfg, in_suffix)
			if string.find(cfg, "Debug") then
				runtime "Debug"
			else
				runtime "Release"
			end
			if string.find(cfg, "fastcall") then
				hasFastcall = true
			end
	end
	filter {}

	if platformName == "Windows" and not hasFastcall then
		-- We do not have fastcall versions, but we can still connect to them
		configmap {
			["Debug_fastcall"] = "Debug",
			["Profile_fastcall"] = "Profile",
			["Release_fastcall"] = "Release"
		}
	end

end

-- Create all projects.
function CreateProjects(in_projects, in_platformName, in_suffix)
	for i,v in pairs(in_projects) do
		v(in_platformName, in_suffix)
	end
end

-- Action specific helpers
-----------------------------------------------------------

-- Returns IDE specific suffic based on current action.
function GetSuffixFromCurrentAction()
	return ActionToSuffix(_ACTION)
end

function ActionToSuffix(in_action)
	local result
	if in_action == "vs2015" then
		result = "_vc140"
	elseif in_action == "vs2017" then
		result = "_vc150"
	elseif in_action == "vs2019" then
		result = "_vc160"
	else
		result = ""
	end
	if _AK_BUILD_AUTHORING ~= nil then
		result = result .. 'Auth'
	end
	return result
end

function IsVisualStudio()
	return _ACTION == "vs2015" or _ACTION == "vs2017" or _ACTION == "vs2019"
end

function IsXcode()
	return _ACTION == "xcode4"
end

function IsPosix(in_platformName)
	return PlatformSupports("POSIX")
end

function ConditionalAddAVX2Files(in_filename)
	if (AK.Platform.avx2archs ~= nil) then
		filter {"platforms:" .. table.concat(AK.Platform.avx2archs, " or ")}
			files
			{
				in_filename
			}
		filter {}

		filter {"files:" .. in_filename,"platforms:" .. table.concat(AK.Platform.avx2archs, " or ")}
			vectorextensions "AVX2"
			isaextensions "FMA"
		filter {}
	end
end

function ConditionalAddAVXFiles(in_filename)
	if (AK.Platform.avxarchs ~= nil) then
		filter {"platforms:" .. table.concat(AK.Platform.avxarchs, " or ")}
			files
			{
				in_filename
			}
		filter {}

		filter {"files:" .. in_filename,"platforms:" .. table.concat(AK.Platform.avxarchs, " or ")}
			vectorextensions "AVX"			
		filter {}
	end
end

-----------------------------------------------------------

-- Tables
function table.find(t, value)
	for i,v in pairs(t) do
		  if v == value then return i end
	 end
	 return nil
end

-- UUIDs
function touuid(guid)
	return guid:sub(1,8) .. "-" .. guid:sub(9,12)	.. "-" .. guid:sub(13,16) .. "-" .. guid:sub(17,20) .. "-" .. guid:sub(21,32)
end

function GenerateUuid(in_projName)
	return touuid(string.sha1(in_projName))
end

function AddProjectReference(in_projectName, in_platformName, in_suffix, projectPath)
	_AK_TARGETS[in_projectName] = in_projectName..in_platformName..in_suffix
	AK.Platform.CreateProject(_AK_TARGETS[in_projectName], in_projectName, projectPath, in_suffix, nil, "Reference")
end

-- Standard output directories.

-- Create the target and object paths for all static libaries (SDK/$(Platform)/$(Config)/lib + obj).
-- Path is relative to premake's execution, that is, SDK/source/Build/.
function SetLibOutputDirectory(in_root, in_platformName, in_config, in_suffix)
	SetOutputDirectory(in_root, in_platformName, in_config, in_suffix, "lib")
end

function SetBinOutputDirectory(in_root,in_platformName, in_config, in_suffix)
	SetOutputDirectory(in_root, in_platformName, in_config, in_suffix, "bin")
end


function SetLibOutputDirectoryByApp(in_root, in_platformName, in_config, in_suffix, in_appName)
	SetOutputDirectory(in_root, in_platformName, in_config, in_suffix, "lib/" .. in_appName)
end

function SetBinOutputDirectoryByApp(in_root,in_platformName, in_config, in_suffix, in_appName)
	SetOutputDirectory(in_root, in_platformName, in_config, in_suffix, "bin/" .. in_appName)
end


function SetOutputDirectory(in_root, in_platformName, in_config, in_suffix, targettype)
	local root = AkRelativeToCwd(in_root)
	if (in_platformName == "iOS" or in_platformName == "tvOS") then
		targetdir (root .. in_platformName .. "/" .. in_config .. "$(EFFECTIVE_PLATFORM_NAME)" ..  "/" .. targettype)
		objdir ("!" .. root .. in_platformName .. "/" .. in_config .. "$(EFFECTIVE_PLATFORM_NAME)"..  "/obj")
	elseif (in_platformName == "UWP") then
		targetdir (root .. in_platformName .. "_$(Platform)" .. in_suffix .. "/$(Configuration)/" .. targettype)
		objdir ("!" .. root .. in_platformName .. "_$(Platform)" .. in_suffix .. "/$(Configuration)/obj/$(ProjectName)")
	elseif (in_platformName == "XboxOne" or in_platformName == "GX" or in_platformName == "GDX") then
		targetdir (root .. in_platformName .. in_suffix .. "/$(Configuration)/" .. targettype)
		objdir ("!" .. root .. in_platformName .. in_suffix .. "/$(Configuration)/obj/$(ProjectName)")
	elseif (in_platformName == "Windows"  ) then
		targetdir (root .. "$(Platform)" .. in_suffix .. "/" .. in_config .. "/" .. targettype)
		objdir ("!" .. root .. "$(Platform)" .. in_suffix .. "/" .. in_config .. "/obj/$(ProjectName)")
	elseif (in_platformName == "NX" or in_platformName == "GGP") then -- no VS version suffix for NX
		targetdir (root .. "$(Platform)" .. "/$(Configuration)/" .. targettype)
		objdir ("!" .. root .. "$(Platform)" .. "/$(Configuration)/obj/$(ProjectName)")
	elseif (in_platformName == "Android") then
		if _ACTION == "androidmk" then
			-- Everything is built to the lib folder so we can get the final files
			local androidtargetdir = "lib"
			targetdir (root .. "Android_$(APP_ABI)/$(CONFIGURATION)/" .. androidtargetdir)
			objdir ("!" .. root .. "Android_$(APP_ABI)/$(CONFIGURATION)/obj/$(ProjectName)")
		else
			targetdir (root .. "Android_$(ArchAbi)/$(Configuration)/" .. targettype)
			objdir ("!" .. root .. "Android_$(ArchAbi)/$(Configuration)/obj/$(ProjectName)")
		end
	elseif (in_platformName == "QNX" ) then
		targetdir (root .. "QNX_$(AK_ARCH)_$(AK_TOOLCHAIN)/" .. in_config ..  "/" .. targettype)
		objdir ("!" .. root .. "QNX_$(AK_ARCH)_$(AK_TOOLCHAIN)/" .. in_config .. "/obj/$(PREMAKE4_BUILDTARGET_BASENAME)")
	elseif (in_platformName == "PS4") then
		targetdir (root .. in_platformName .. "/" .. in_config ..  "/" .. targettype)
		objdir ("!" .. root .. in_platformName .. "/" .. in_config .. "/obj/$(ProjectName)")
	elseif (in_platformName == "Linux" ) then
		targetdir (root .. "Linux_$(AK_LINUX_ARCH)/" .. in_config ..  "/" .. targettype)
		objdir ("!" .. root .. "Linux_$(AK_LINUX_ARCH)/" .. in_config .. "/obj/$(PREMAKE4_BUILDTARGET_BASENAME)")
	elseif (in_platformName == "LinuxAuto" ) then
		targetdir (root .. "Linux_$(AK_ARCH)_$(AK_TOOLCHAIN)/" .. in_config ..  "/" .. targettype)
		objdir ("!" .. root .. "Linux_$(AK_ARCH)_$(AK_TOOLCHAIN)/" .. in_config .. "/obj/$(PREMAKE4_BUILDTARGET_BASENAME)")
	elseif (in_platformName == "Pellegrino") then
		targetdir (root .. in_platformName .. "/" .. in_config ..  "/" .. targettype)
		objdir ("!" .. root .. in_platformName .. "/" .. in_config .. "/obj/$(ProjectName)")
	elseif (in_platformName == "Chinook") then
		targetdir (root .. in_platformName .. in_suffix .. "/$(Configuration)/" .. targettype)
		objdir ("!" .. root .. in_platformName .. in_suffix .. "/$(Configuration)/obj/$(ProjectName)")
	else
		targetdir (root .. in_platformName .. "/" .. in_config ..  "/" .. targettype)
		objdir ("!" .. root .. in_platformName .. "/" .. in_config .. "/obj/$(PREMAKE4_BUILDTARGET_BASENAME)")
	end
end

function LinkAllAkTargets(in_platformName, in_linkExterns, in_importProjects)
	local all = {}
	for target,_ in pairs(_AK_TARGETS) do
		if _ then		-- Valid, not nil
			table.insert(all, target)
		end
	end

	if in_linkExterns == true then
		for target,_ in pairs(_AK_TARGETS_EXTERN) do
			if _ then		-- Valid, not nil
				table.insert(all, target)
			end
		end
	end

	table.sort(all)

	filter {}
		for _, target in ipairs(all) do
			if in_importProjects then importproject(target) end
			if target ~= "AkSink" then -- AkSink is a toy plugin. It must not be linked to anything.
				links(target)
			end
		end
end

function ImportAllAkTargets(in_withExterns)
	local all = {}
	for target,_ in pairs(_AK_TARGETS) do
		if _ then		-- Valid, not nil
			table.insert(all, target)
		end
	end

	if in_withExterns == true then
		for target,_ in pairs(_AK_TARGETS_EXTERN) do
			if _ then		-- Valid, not nil
				table.insert(all, target)
			end
		end
	end

	table.sort(all)

	filter {}
		for _, target in ipairs(all) do
			importproject(target)
		end
end

function ImportProjects(projects)
	for idx, project in pairs(projects) do
		importproject(project)
	end
end

function DisablePropSheetElements()
	if IsVisualStudio() then
		filter {}
			-- These are things in our property sheet. We wish to disable them! (Keep in sync with SDK/Build/AkPlugins)
			removeelements {
				"IgnoreWarnCompileDuplicatedFilename",
				"DebugInformationFormat",
				"FunctionLevelLinking",
				"IntrinsicFunctions",
				"MinimalRebuild",
				"StringPooling",
				"RuntimeLibrary"
			}
	end
end

function ApplyPlatformExceptions(prjName, prj)
	prjName = NormalizeProjectName(prjName)
	if (AK.Platform.Exceptions[prjName]) then
		AK.Platform.Exceptions[prjName](prj)
	end
end

function ApplyPlatformExclusions(prjName, projectLocation)
	prjName = NormalizeProjectName(prjName)
	if AK.Platform.Exclusions[prjName] ~= nil then
		AK.Platform.Exclusions[prjName](projectLocation)
	end
end

function NormalizeProjectName(prjName)
	if _AK_BUILD_AUTHORING then
		local idx = string.find(prjName, "SDK_")
		if idx ~= nil and idx > 0 then
			prjName = string.sub(prjName, idx + 4)
		end
	end
	return prjName
end

function PlatformSupports(feature)
	for k,v in pairs(AK.Platform.features) do
		if v == feature then
			return true
		end
	end
	return false
end

function PlatformSuffix(exception)
	return ReadOverridableProperty(AK.Platform.suffix, exception)
end

function PlatformDirectory(purpose, exception)
	return ReadOverridableProperty(AK.Platform.directories[purpose], exception)
end

function PlatformKind(project)
	return ReadOverridableProperty(AK.Platform.kinds, project)
end

function ReadOverridableProperty(val, override)
	if type(val) == 'table' then
		if val[override] then
			val = val[override]
		else
			val = val.__default__
		end
	end
	return val
end

-- Make sure we have a root folder, or "" if none. This is useful when the SDK premake is being included elsewhere,
-- such as in the Authoring. This must point to the Build/ folder.
if _AK_ROOT_DIR == nil then
	_AK_ROOT_DIR = ""
end

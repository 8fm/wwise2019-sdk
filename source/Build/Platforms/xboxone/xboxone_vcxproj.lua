local p = premake

local xboxone = p.modules.xboxone
local vc200x = p.vstudio.vc200x
local vc2010 = p.vstudio.vc2010
local vstudio = p.vstudio
local project = p.project
local config = p.config

if vstudio.vs2010_architectures ~= nil then
	vstudio.vs2010_architectures.xboxone = "Durango"
end

premake.override(vc2010.elements, "project", function(oldfn, prj)
	local elements = oldfn(prj)
	if prj.system == p.XBOXONE then
		elements = table.join(elements, {
			xboxone.xboxonePaths,
			xboxone.xboxoneAssemblies,
		})
	end
	return elements
end)

premake.override(vc2010.elements, "globals", function(oldfn, prj)
	local elements = oldfn(prj)
	if prj.system == p.XBOXONE then
		elements = table.join(elements, {
			xboxone.xboxoneGlobals,
		})
	end
	return elements
end)

premake.override(vc2010.elements, "configurationProperties", function(oldfn, cfg)
	local elements = oldfn(cfg)
	if cfg.kind ~= p.UTILITY and cfg.system == p.XBOXONE then
		elements = table.join(elements, {
			xboxone.noEmbedManifest,
			xboxone.noGenerateManifest,
		})
	end
	return elements
end)

premake.override(vc2010, "outDir", function(oldfn, cfg)
	oldfn(cfg)
	if ( cfg.kind == "ConsoleApp" or cfg.kind == "WindowedApp" ) then
		if (cfg.targetlayoutdir ~= nil and cfg.targetlayoutdir ~= '') then
			vc2010.element(cfg, "LayoutDir", nil, "%s\\", premake.esc(cfg.targetlayoutdir))
		end
	end
end)

premake.override(vc2010, "keyword", function(oldfn, prj)
	if prj.system == p.XBOXONE then
		vc2010.element(prj, "Keyword", nil, "Win32Proj")
		vc2010.element(prj, "RootNamespace", nil, "%s", prj.name)
	else
		oldfn(prj)
	end
end)

function xboxone.xboxonePaths(prj)
	for cfg in project.eachconfig(prj) do
		local prjcfg = vstudio.projectConfig(cfg, arch)
		local name = cfg.name
		p.push('<PropertyGroup Condition="\'$(Configuration)|$(Platform)\'==\'%s\'">', premake.esc(name))
		p.w('<ReferencePath>$(Console_SdkLibPath);$(Console_SdkWindowsMetadataPath)</ReferencePath>')
		p.w('<LibraryWPath>$(Console_SdkLibPath);$(Console_SdkWindowsMetadataPath)</LibraryWPath>')
		p.pop('</PropertyGroup>')
	end
end

function xboxone.xboxoneAssemblies(prj)
	local cfg = project.getfirstconfig(prj)
	if cfg.kind == "ConsoleApp" or cfg.kind == "WindowedApp" then
		p.push('<ItemGroup>')
		p.w('<SDKReference Include="Xbox Services API, Version=8.0" />')
		p.pop('</ItemGroup>')
	end
end

function xboxone.xboxoneGlobals(prj)
	vc2010.element(prj, "ApplicationEnvironment", nil, "%s", "title")
	if (_ACTION == "vs2015") then
		vc2010.element(prj, "PlatformToolset", nil, "%s", "v140")
		vc2010.element(prj, "MinimumVisualStudioVersion", nil, "%s", "14.0")
		vc2010.element(prj, "TargetRuntime", nil, "%s", "Native")
	end
end

function xboxone.noEmbedManifest(cfg)
	if cfg.flags.NoEmbedManifest then
		vc2010.element(cfg, "EmbedManifest", nil, 'false')
	end
end

function xboxone.noGenerateManifest(cfg)
	if cfg.flags.NoGenerateManifest then
		vc2010.element(cfg, "GenerateManifest", nil, 'false')
	end
end

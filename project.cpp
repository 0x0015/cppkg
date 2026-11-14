#include "project.hpp"
#include <iostream>
#include <set>

nlohmann::json project::serialize() const{
	nlohmann::json output{};
	output["name"] = name;
	output["type"] = std::array<std::string_view, 2>{{"library", "executable"}}[(unsigned int)type];
	output["binName"] = binName;
	output["version"] = version;

	output["source"] = norm.sourceFiles;
	output["compileFlags"] = norm.compileFlags;
	output["linkFlags"] = norm.linkFlags;

	//don't include these if they will just be empty
	if(!debug_extra.sourceFiles.empty() || !debug_extra.compileFlags.empty() || !debug_extra.linkFlags.empty()){
		auto& debug = output["debug"];
		debug["source"] = debug_extra.sourceFiles;
		debug["compileFlags"] = debug_extra.compileFlags;
		debug["linkFlags"] = debug_extra.linkFlags;
	}

	if(!release_extra.sourceFiles.empty() || !release_extra.compileFlags.empty() || !release_extra.linkFlags.empty()){
		auto& release = output["release"];
		release["source"] = release_extra.sourceFiles;
		release["compileFlags"] = release_extra.compileFlags;
		release["linkFlags"] = release_extra.linkFlags;
	}

	if(!deps.empty())
		output["deps"] = deps;

	if(!customCommands.empty())
		output["customCommands"] = customCommands;

	if(!prebuildCommands.empty())
		output["prebuildCommands"] = prebuildCommands;

	if(!postbuildCommands.empty())
		output["postbuildCommands"] = postbuildCommands;

	if(!extraCleanCommands.empty())
		output["extraCleanCommands"] = extraCleanCommands;

	if(norm.externalBuildCommand)
		output["externalBuildCommand"] = *norm.externalBuildCommand;


	// Serialize OS-specific options
	std::set<osVer> os_keys;
	for(const auto& [os, _] : osNorm_ExtraOpts) os_keys.insert(os);
	for(const auto& [os, _] : osDebug_ExtraOpts) os_keys.insert(os);
	for(const auto& [os, _] : osRelease_ExtraOpts) os_keys.insert(os);

	for(const auto& os : os_keys){
		std::string osStr = os.createOsString();
		auto& osObj = output[osStr];
		
		if(osNorm_ExtraOpts.contains(os)){
			const auto& opts = osNorm_ExtraOpts.at(os);
			if(!opts.sourceFiles.empty()) osObj["source"] = opts.sourceFiles;
			if(!opts.compileFlags.empty()) osObj["compileFlags"] = opts.compileFlags;
			if(!opts.linkFlags.empty()) osObj["linkFlags"] = opts.linkFlags;
			if(opts.externalBuildCommand) osObj["externalBuildCommand"] = *opts.externalBuildCommand;
		}
		
		if(osDebug_ExtraOpts.contains(os)){
			const auto& opts = osDebug_ExtraOpts.at(os);
			if(!opts.sourceFiles.empty() || !opts.compileFlags.empty() || !opts.linkFlags.empty() || opts.externalBuildCommand){
				auto& dbgObj = osObj["debug"];
				if(!opts.sourceFiles.empty()) dbgObj["source"] = opts.sourceFiles;
				if(!opts.compileFlags.empty()) dbgObj["compileFlags"] = opts.compileFlags;
				if(!opts.linkFlags.empty()) dbgObj["linkFlags"] = opts.linkFlags;
				if(opts.externalBuildCommand) dbgObj["externalBuildCommand"] = *opts.externalBuildCommand;
			}
		}
		
		if(osRelease_ExtraOpts.contains(os)){
			const auto& opts = osRelease_ExtraOpts.at(os);
			if(!opts.sourceFiles.empty() || !opts.compileFlags.empty() || !opts.linkFlags.empty() || opts.externalBuildCommand){
				auto& relObj = osObj["release"];
				if(!opts.sourceFiles.empty()) relObj["source"] = opts.sourceFiles;
				if(!opts.compileFlags.empty()) relObj["compileFlags"] = opts.compileFlags;
				if(!opts.linkFlags.empty()) relObj["linkFlags"] = opts.linkFlags;
				if(opts.externalBuildCommand) relObj["externalBuildCommand"] = *opts.externalBuildCommand;
			}
		}
	}

	return output;
}

void project::deserialize(const nlohmann::json& j){
	init_default();
	trySetVal(j, "name", name);
	if(j.contains("type")){
		const std::string& typestr = j["type"];
		if(typestr == "library" || typestr == "lib")
			type = LIBRARY;
		else if(typestr == "executable" || typestr == "exe")
			type = EXECUTABLE;
		else
			std::cerr<<"Error: unknown type \""<<typestr<<"\". Please specify either \"library\" or \"executable\""<<std::endl;
	}
	trySetVal(j, "binName", binName);
	trySetVal(j, "version", version);
	trySetVal(j, "source", norm.sourceFiles);
	trySetVal(j, "compileFlags", norm.compileFlags);
	trySetVal(j, "linkFlags", norm.linkFlags);
	if(j.contains("externalBuildCommand"))
		norm.externalBuildCommand = j["externalBuildCommand"].get<std::string>();
	if(j.contains("debug")){
		const auto& debug = j["debug"];
		trySetVal(debug, "source", debug_extra.sourceFiles);
		trySetVal(debug, "compileFlags", debug_extra.compileFlags);
		trySetVal(debug, "linkFlags", debug_extra.linkFlags);
		if(debug.contains("externalBuildCommand"))
			debug_extra.externalBuildCommand = debug["externalBuildCommand"].get<std::string>();
	}
	if(j.contains("release")){
		const auto& release = j["release"];
		trySetVal(release, "source", release_extra.sourceFiles);
		trySetVal(release, "compileFlags", release_extra.compileFlags);
		trySetVal(release, "linkFlags", release_extra.linkFlags);
		if(release.contains("externalBuildCommand"))
			release_extra.externalBuildCommand = release["externalBuildCommand"].get<std::string>();
	}
	trySetVal(j, "deps", deps);
	trySetVal(j, "customCommands", customCommands);
	trySetVal(j, "prebuildCommands", prebuildCommands);
	trySetVal(j, "postbuildCommands", postbuildCommands);
	trySetVal(j, "extraCleanCommands", extraCleanCommands);

	//try an detect os specific flags.  We'll just do a very basic try here
	for(const auto& [name, vals] : j.items()){
		if((name.find("lin") != std::string::npos || name.find("win") != std::string::npos) && name.find("-") != std::string::npos){
			//it probably matches an os string (couldn't reasonably match something else)
			auto osStr = osVer::parseOsString(name);
			if(!osStr){
				std::cerr<<"Error: failed to determine os version for specific flags with error: "<<osVer::lastOsStringParseError<<std::endl;
				continue;
			}
			auto& osVerSpecificOpts = osNorm_ExtraOpts[*osStr];	
			trySetVal(vals, "source", osVerSpecificOpts.sourceFiles);
			trySetVal(vals, "compileFlags", osVerSpecificOpts.compileFlags);
			trySetVal(vals, "linkFlags", osVerSpecificOpts.linkFlags);
			if(vals.contains("externalBuildCommand"))
				osVerSpecificOpts.externalBuildCommand = vals["externalBuildCommand"].get<std::string>();
			if(vals.contains("debug")){
				auto& osVerSpecificDebugOpts = osDebug_ExtraOpts[*osStr];	
				const auto& debug = vals["debug"];
				trySetVal(debug, "source", osVerSpecificDebugOpts.sourceFiles);
				trySetVal(debug, "compileFlags", osVerSpecificDebugOpts.compileFlags);
				trySetVal(debug, "linkFlags", osVerSpecificDebugOpts.linkFlags);
				if(debug.contains("externalBuildCommand"))
					osVerSpecificDebugOpts.externalBuildCommand = debug["externalBuildCommand"].get<std::string>();
			}
			if(vals.contains("release")){
				auto& osVerSpecificReleaseOpts = osRelease_ExtraOpts[*osStr];	
				const auto& release = vals["release"];
				trySetVal(release, "source", osVerSpecificReleaseOpts.sourceFiles);
				trySetVal(release, "compileFlags", osVerSpecificReleaseOpts.compileFlags);
				trySetVal(release, "linkFlags", osVerSpecificReleaseOpts.linkFlags);
				if(release.contains("externalBuildCommand"))
					osVerSpecificReleaseOpts.externalBuildCommand = release["externalBuildCommand"].get<std::string>();
			}
		}
	}
}

void project::init_default(){
	name = "New Project";
	binName = "project";
	type = EXECUTABLE;
	version = "0.0.1";
	norm.sourceFiles = {"main.cpp"};
	norm.compileFlags = {"-std=c++20"};
	debug_extra.compileFlags = {"-g", "-Wall"};
	release_extra.compileFlags = {"-O2"};
}

minimalProjectInfo::minimalProjectInfo(const project& proj, bool isDebug, osVer os) : proj(proj), name(proj.name), type(proj.type), binName(proj.binName), version(proj.version), stdCompileOpts(proj.norm), deps(proj.deps), customCommands(proj.customCommands), prebuildCommands(proj.prebuildCommands), postbuildCommands(proj.postbuildCommands), extraCleanCommands(proj.extraCleanCommands), os(os), mode(isDebug ? DEBUG : RELEASE){
	if(isDebug)
		extraCompileOpts = proj.debug_extra;
	else
		extraCompileOpts = proj.release_extra;

	if(proj.osNorm_ExtraOpts.contains(os)){
		const auto& osNormExtra = proj.osNorm_ExtraOpts.at(os);
		for(const auto& extraSource : osNormExtra.sourceFiles)
			extraCompileOpts.sourceFiles.push_back(extraSource);
		for(const auto& extraCompFlag : osNormExtra.compileFlags)
			extraCompileOpts.compileFlags.push_back(extraCompFlag);
		for(const auto& extraLinkFlag : osNormExtra.linkFlags)
			extraCompileOpts.linkFlags.push_back(extraLinkFlag);

		if(osNormExtra.externalBuildCommand)
			extraCompileOpts.externalBuildCommand = osNormExtra.externalBuildCommand;
	}

	if(isDebug){
		if(proj.osDebug_ExtraOpts.contains(os)){
			const auto& osDebugExtra = proj.osDebug_ExtraOpts.at(os);
			for(const auto& extraSource : osDebugExtra.sourceFiles)
				extraCompileOpts.sourceFiles.push_back(extraSource);
			for(const auto& extraCompFlag : osDebugExtra.compileFlags)
				extraCompileOpts.compileFlags.push_back(extraCompFlag);
			for(const auto& extraLinkFlag : osDebugExtra.linkFlags)
				extraCompileOpts.linkFlags.push_back(extraLinkFlag);
	
			if(osDebugExtra.externalBuildCommand)
				extraCompileOpts.externalBuildCommand = osDebugExtra.externalBuildCommand;
		}
	}else{
		if(proj.osRelease_ExtraOpts.contains(os)){
			const auto& osReleaseExtra = proj.osRelease_ExtraOpts.at(os);
			for(const auto& extraSource : osReleaseExtra.sourceFiles)
				extraCompileOpts.sourceFiles.push_back(extraSource);
			for(const auto& extraCompFlag : osReleaseExtra.compileFlags)
				extraCompileOpts.compileFlags.push_back(extraCompFlag);
			for(const auto& extraLinkFlag : osReleaseExtra.linkFlags)
				extraCompileOpts.linkFlags.push_back(extraLinkFlag);
	
			if(osReleaseExtra.externalBuildCommand)
				extraCompileOpts.externalBuildCommand = osReleaseExtra.externalBuildCommand;
		}
	}
}

std::string minimalProjectInfo::getTrueBinName() const{
	if(type == project::EXECUTABLE){
		if(os.type == osVer::Windows){
			return binName + ".exe";
		}else if(os.type == osVer::Linux){
			return binName;
		}else{
			std::cerr<<"Error: tried to get true bin name for a non-linux, non-windows OS"<<std::endl;
			return "";
		}
	}else if(type == project::LIBRARY){
		if(os.type == osVer::Windows){
			return binName + ".dll";
		}else if(os.type == osVer::Linux){
			return binName + ".so";
		}else{
			std::cerr<<"Error: tried to get true bin name for a non-linux, non-windows OS"<<std::endl;
			return "";
		}
	}else{
		std::cerr<<"Error: tried to get true bin name of a non-executable, non-library project"<<std::endl;
		return "";
	}
}


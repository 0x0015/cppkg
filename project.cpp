#include "project.hpp"
#include <iostream>

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

	return output;
}

void project::deserialize(const nlohmann::json& j){
	init_default();
	trySetVal(j, "name", name);
	if(j.contains("type")){
		const std::string& typestr = j["type"];
		if(typestr == "library" || typestr == "lib")
			type = LIBRARY;
		if(typestr == "executable" || typestr == "exe")
			type = EXECUTABLE;
		else
			std::cerr<<"Error: unknown type \""<<typestr<<"\". Please specify either \"library\" or \"executable\""<<std::endl;
	}
	trySetVal(j, "binName", binName);
	trySetVal(j, "version", version);
	trySetVal(j, "source", norm.sourceFiles);
	trySetVal(j, "compileFlags", norm.compileFlags);
	trySetVal(j, "linkFlags", norm.linkFlags);
	if(j.contains("debug")){
		const auto& debug = j["debug"];
		trySetVal(debug, "source", debug_extra.sourceFiles);
		trySetVal(debug, "compileFlags", debug_extra.compileFlags);
		trySetVal(debug, "linkFlags", debug_extra.linkFlags);
	}
	if(j.contains("release")){
		const auto& release = j["release"];
		trySetVal(release, "source", release_extra.sourceFiles);
		trySetVal(release, "compileFlags", release_extra.compileFlags);
		trySetVal(release, "linkFlags", release_extra.linkFlags);
	}
	trySetVal(j, "deps", deps);
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


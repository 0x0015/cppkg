#include <iostream>
#include <filesystem>
#include <fstream>
#include "project.hpp"
#include "depMan.hpp"
#include "builder.hpp"
#include "paths.hpp"
#include "customCommand.hpp"

const char* defaultProject = R"({
	"name": "New Project",
	"version": "0.0.1",
	"type": "executable",
	"binName": "project",
	"source": [
		"main.cpp"
	],
	"compileFlags": ["-std=c++20"],
	"linkFlags": [],
	"debug": {
		"compileFlags": ["-g", "-Wall"]
	},
	"release": {
		"compileFlags": ["-O2"]
	}
})";

int init(){
	if(!std::filesystem::exists("project.json")){
		std::ofstream projFile("project.json");
		projFile.write(defaultProject, strlen(defaultProject));
		std::ofstream gitignore(".gitignore", std::ios_base::app);
		gitignore << "\n_cppkg";
		return 0;
	}
	return 0;
}

int build(bool isDebug = true, osVer os = osVer::hostOsVer()){
	//read, then resolve deps, then build
	if(!std::filesystem::exists("project.json")){
		std::cerr<<"Error: no project file in current directory"<<std::endl;
		return(1);
	}
	project proj;
	proj.deserializeFromFile("project.json");
	auto minified = minimalProjectInfo(proj, isDebug, os);
	
	for(const auto& prebuildCmd : proj.prebuildCommands){
		std::cout<<"Command $ "<<prebuildCmd<<std::endl;
		system(prebuildCmd.c_str());
	}

	depMan deps(minified);
	if(!deps.resolveDeps()){
		std::cerr<<"Error: Failed to resolve dependencies"<<std::endl;
		return(1);
	}
	builder bldr(deps);
	if(!bldr.build()){
		std::cerr<<"Error: Build failed"<<std::endl;
		return(1);
	}

	if(!deps.populateDependenciesToBinDir()){
		std::cerr<<"Error: failed to populate dependencies into the bin dir"<<std::endl;
		return 1;
	}

	for(const auto& postbuildCmd : proj.postbuildCommands){
		std::cout<<"Command $ "<<postbuildCmd<<std::endl;
		system(postbuildCmd.c_str());
	}

	return 0;
}

int clean(){
	if(!std::filesystem::exists("project.json")){
		std::cerr<<"Error: no project file in current directory"<<std::endl;
		return(1);
	}
	project proj;
	proj.deserializeFromFile("project.json");
	if(proj.type == project::EXECUTABLE){
		if(std::filesystem::exists(proj.binName))
			std::filesystem::remove(proj.binName);
		if(std::filesystem::exists(proj.binName + ".exe"))
			std::filesystem::remove(proj.binName + ".exe");
	}else{
		if(std::filesystem::exists(proj.binName + ".so"))
			std::filesystem::remove(proj.binName + ".so");
		if(std::filesystem::exists(proj.binName + ".dll"))
			std::filesystem::remove(proj.binName + ".dll");
	}
	if(std::filesystem::exists("_cppkg"))
		std::filesystem::remove_all("_cppkg");

	for(const auto& cleanCmd : proj.extraCleanCommands){
		std::cout<<"Command $ "<<cleanCmd<<std::endl;
		system(cleanCmd.c_str());
	}

	return 0;
}

int package(){
	if(!std::filesystem::exists("project.json")){
		std::cerr<<"Error: no project file in current directory"<<std::endl;
		return(1);
	}
	project proj;
	proj.deserializeFromFile("project.json");
	auto buildPath = getTempPath("build");
	auto lastBuildInfoPath = buildPath / "lastBuildInfo.json";
	if(!std::filesystem::exists(lastBuildInfoPath)){
		std::cerr<<"Error: could not find last build info.  Has the package been build yet?"<<std::endl;
		return 1;
	}
	builder::buildInfo lastBuildInfo;
	lastBuildInfo.deserializeFromFile(lastBuildInfoPath.string());
	auto minified = minimalProjectInfo(proj, lastBuildInfo.mode == minimalProjectInfo::DEBUG, lastBuildInfo.os);
	depMan deps(minified);
	if(!deps.createPackage()){
		std::cerr<<"Error: failed to create package"<<std::endl;
		return(1);
	}
	return 0;
}

int run(int argc, char** argv){
	if(!std::filesystem::exists("project.json")){
		std::cerr<<"Error: no project file in current directory"<<std::endl;
		return(1);
	}
	project proj;
	proj.deserializeFromFile("project.json");
	auto buildPath = getTempPath("build");
	auto lastBuildInfoPath = buildPath / "lastBuildInfo.json";
	if(!std::filesystem::exists(lastBuildInfoPath)){
		//there is no build.  Try building now?
		bool isDebug = true;
		if(argc >= 1){
			if(std::string_view(argv[0]) == "debug")
				isDebug = true;
			else if(std::string_view(argv[0]) == "release")
				isDebug = false;
			else{
				std::cerr<<"Error: unknown parameter \""<<argv[0]<<"\""<<std::endl;
				return(1);
			}
		}
		osVer os = osVer::hostOsVer();
		if(argc >= 2){
			auto osParsed = osVer::parseOsString(argv[1]);
			if(!osParsed){
				std::cout<<"Error: failed to parse os with error "<<osVer::lastOsStringParseError<<std::endl;
				return 1;
			}
			os = *osParsed;
		}
		int buildResult = build(isDebug, os);
		if(buildResult != 0)
			return buildResult;

		if(!std::filesystem::exists(lastBuildInfoPath)){
			//if the build still doesn't exist, we just give up
			std::cerr<<"Error: could not find last build info.  Has the package been build yet?"<<std::endl;
			return 1;
		}

		argc -= 2;
		argv += 2;
	}
	builder::buildInfo lastBuildInfo;
	lastBuildInfo.deserializeFromFile(lastBuildInfoPath.string());
	auto minified = minimalProjectInfo(proj, lastBuildInfo.mode == minimalProjectInfo::DEBUG, lastBuildInfo.os);
	
	if(proj.type != project::EXECUTABLE){
		std::cerr<<"Error: cannot run a non-executable project"<<std::endl;
		return 1;
	}

	std::filesystem::path binPath = buildPath / "bin" / (minified.os.createOsString() + (minified.mode == minimalProjectInfo::DEBUG ? "-debug" : "-release"));
	if(!std::filesystem::exists(binPath)){
		std::cerr<<"Error: Last build file exists, but build dir doesn't"<<std::endl;
		return 1;
	}
	
	std::string args;
	for(int i=0;i<argc;i++)
		args += " " + std::string(argv[i]);

	return system(((binPath / minified.getTrueBinName()).string() + args).c_str());
}

template<auto printHelp> int customCommand(const std::string& commandName){
	if(!std::filesystem::exists("project.json")){
		std::cerr<<"Error: no project file in current directory"<<std::endl;
		return(1);
	}
	project proj;
	proj.deserializeFromFile("project.json");

	const auto noBuildFallback = [&](){
		std::cout<<"Command $ "<<proj.customCommands.at(commandName)<<std::endl;
		return system(proj.customCommands.at(commandName).c_str());
	};

	auto buildPath = getTempPath("build");
	auto lastBuildInfoPath = buildPath / "lastBuildInfo.json";
	if(!std::filesystem::exists(lastBuildInfoPath)){
		std::cout<<"Note: No build detected. Command path substitutions will not work"<<std::endl;
		return noBuildFallback();
	}
	builder::buildInfo lastBuildInfo;
	lastBuildInfo.deserializeFromFile(lastBuildInfoPath.string());
	auto minified = minimalProjectInfo(proj, lastBuildInfo.mode == minimalProjectInfo::DEBUG, lastBuildInfo.os);

	if(!proj.customCommands.contains(commandName))
		printHelp();

	std::cout<<"Command $ "<<substitutePathsIntoCommand(proj.customCommands.at(commandName), minified)<<std::endl;
	return runCustomCommand(proj.customCommands.at(commandName), minified);
}

int main(int argc, char** argv){
	if(argc == 2 && std::string_view(argv[1]) == "init"){
		return init();
	}else if(argc >= 2 && argc <= 4 && std::string_view(argv[1]) == "build"){
		bool isDebug = true;
		if(argc >= 3){
			if(std::string_view(argv[2]) == "debug")
				isDebug = true;
			else if(std::string_view(argv[2]) == "release")
				isDebug = false;
			else{
				std::cerr<<"Error: unknown parameter \""<<argv[2]<<"\""<<std::endl;
				return(1);
			}
		}
		osVer os = osVer::hostOsVer();
		if(argc == 4){
			auto osParsed = osVer::parseOsString(argv[3]);
			if(!osParsed){
				std::cout<<"Error: failed to parse os with error "<<osVer::lastOsStringParseError<<std::endl;
				return 1;
			}
			os = *osParsed;
		}
		return build(isDebug, os);
	}else if(argc == 2 && std::string_view(argv[1]) == "clean"){
		return clean();
	}else if(argc == 2 && std::string_view(argv[1]) == "package"){
		return package();
	}else if(argc >= 2 && argc <= 4 && std::string_view(argv[1]) == "build_package"){
		bool isDebug = true;
		if(argc >= 3){
			if(std::string_view(argv[2]) == "debug")
				isDebug = true;
			else if(std::string_view(argv[2]) == "release")
				isDebug = false;
			else{
				std::cerr<<"Error: unknown parameter \""<<argv[2]<<"\""<<std::endl;
				return(1);
			}
		}
		osVer os = osVer::hostOsVer();
		if(argc == 4){
			auto osParsed = osVer::parseOsString(argv[3]);
			if(!osParsed){
				std::cout<<"Error: failed to parse os with error "<<osVer::lastOsStringParseError<<std::endl;
				return 1;
			}
			os = *osParsed;
		}
		int buildResult = build(isDebug, os);
		if(buildResult != 0)
			return buildResult;
		return package();
	}else if(argc >= 2 && std::string_view(argv[1]) == "run"){
		return run(argc - 2, argv + 2);
	}else{
		const auto printHelp = [](){
			std::cout<<"cppkg v0.0.2 Options:\n\tinit - initialize an empty project in the current directory\n\tbuild - build the project in the current directory\n\tclean - clean the project in the current directory\n\tpackage - create a package .tar.xz containing the binary and header files\n\trun - build and run the (executable) project\n\thelp (or anything else) - print this message"<<std::endl;
			exit(0);
		};
		if(argc == 2 && std::filesystem::exists("project.json")){
			return customCommand<printHelp>(std::string(argv[1]));
		}else
			printHelp();
	}
}


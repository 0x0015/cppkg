#include "builder.hpp"
#include "paths.hpp"
#include <thread>
#include <mutex>
#include <atomic>
#include <iostream>
#include <fstream>
#include <unordered_set>
#include "customCommand.hpp"

const std::map<osVer, std::vector<std::string>> compilerNames = {
	{osVer{osVer::x86_64, osVer::Linux}, {"x86_64-linux-gnu-g++"}},
	{osVer{osVer::x86_64, osVer::Windows}, {"x86_64-w64-mingw32-g++"}},
	{osVer{osVer::x86, osVer::Windows}, {"i686-w64-mingw32-g++"}},
};

const std::map<osVer, std::vector<std::string>> arNames = {
	{osVer{osVer::x86_64, osVer::Linux}, {"x86_64-linux-gnu-ar"}},
	{osVer{osVer::x86_64, osVer::Windows}, {"x86_64-w64-mingw32-ar"}},
	{osVer{osVer::x86, osVer::Windows}, {"i686-w64-mingw32-ar"}},
};

bool builder::build(){
	auto buildPath = getTempPath("build");
	if(proj.extraCompileOpts.externalBuildCommand){
		std::cout<<"External build set, so skipping normal build and running external build command"<<std::endl;
		for(const auto& cmd : *proj.extraCompileOpts.externalBuildCommand){
			std::cout<<"Command $ "<<substitutePathsIntoCommand(cmd, proj)<<std::endl;
			runCustomCommand(cmd, proj);
		}
		buildInfo lastBuildInfo{proj.os, proj.mode};
		lastBuildInfo.serializeToFile((buildPath / "lastBuildInfo.json").string());
		return true;
	}
	if(proj.stdCompileOpts.externalBuildCommand){
		std::cout<<"External build set, so skipping normal build and running external build command"<<std::endl;
		for(const auto& cmd : *proj.stdCompileOpts.externalBuildCommand){
			std::cout<<"Command $ "<<substitutePathsIntoCommand(cmd, proj)<<std::endl;
			runCustomCommand(cmd, proj);
		}
		buildInfo lastBuildInfo{proj.os, proj.mode};
		lastBuildInfo.serializeToFile((buildPath / "lastBuildInfo.json").string());
		return true;
	}

	auto src_path = buildPath / "src";
	if(!std::filesystem::exists(src_path))
		std::filesystem::create_directories(src_path);

	const auto& runInSrcPath = [&](const std::string& cmd){
		auto oldCWD = std::filesystem::current_path();
		std::filesystem::current_path(src_path);
		auto retVal = system(cmd.c_str());
		std::filesystem::current_path(oldCWD);
		return retVal;
	};

	const auto copyReleventFile = [&](const std::filesystem::path& file){
		auto relPath = std::filesystem::relative(file);
		auto relPathParentFolder = relPath.parent_path();
		std::filesystem::create_directories(src_path / relPathParentFolder);
		std::filesystem::copy(file, src_path / relPath, std::filesystem::copy_options::overwrite_existing);
	};

	for(const std::filesystem::directory_entry& dir_entry : std::filesystem::recursive_directory_iterator(std::filesystem::current_path())){
		if(pathContainsDirectory(dir_entry.path(), "_cppkg"))
			continue;
		if(dir_entry.is_regular_file() && (dir_entry.path().extension() == ".cpp" || dir_entry.path().extension() == ".c" || dir_entry.path().extension() == ".hpp" || dir_entry.path().extension() == ".h" || dir_entry.path().extension() == ".inc" || dir_entry.path().extension() == ".inl")){
			copyReleventFile(dir_entry.path());
		}
	}

	std::string compileFlags;
	for(const auto& flag : proj.stdCompileOpts.compileFlags)
		compileFlags += " " + flag;

	for(const auto& flag : proj.extraCompileOpts.compileFlags)
		compileFlags += " " + flag;

	bool isStatic = false;
	std::string linkFlags = " -Wl,-rpath,'$ORIGIN'";
	for(const auto& flag : proj.stdCompileOpts.linkFlags){
		linkFlags += " " + flag;
		if(flag == "-static")
			isStatic = true;
	}

	for(const auto& flag : proj.extraCompileOpts.linkFlags){
		linkFlags += " " + flag;
		if(flag == "-static")
			isStatic = true;
	}


	//special case for windows executables - they should always be static by default
	//make sure to include stuff that would be shipped by default
	if(proj.os.type == osVer::Windows && proj.type == project::EXECUTABLE){
		linkFlags += " -static";
		isStatic = true;
	}


	//technically we need to worry about link order, but let's skip that for now
	for(const auto& [dep, depProj] : deps.downloadedManifests){
		auto depPath = getTempPath("pkg") / dep;
		//add include paths
		if(std::filesystem::exists(depPath / "include"))//maybe eventually add more cases to detect src, src/include, idk
			compileFlags += " -I" + (depPath / "include").string();
		else
			compileFlags += " -I" + depPath.string();

		auto minimalDepProj = minimalProjectInfo(depProj, proj.mode == minimalProjectInfo::DEBUG, proj.os);
		if(depProj.type == project::LIBRARY){
			linkFlags += " -L" + depPath.string();
			if(depProj.binName.starts_with("lib"))
				linkFlags += " -l" + depProj.binName.substr(3);
			else if(!depProj.binName.empty())
				linkFlags += " -l:" + (isStatic ? minimalDepProj.binName + ".a" : minimalDepProj.getTrueBinName());

			for(const auto& depLinkFlag : minimalDepProj.stdCompileOpts.linkFlags)
				linkFlags += " " + depLinkFlag;
			for(const auto& depLinkFlag : minimalDepProj.extraCompileOpts.linkFlags)
				linkFlags += " " + depLinkFlag;
		}
		const auto addCompileFlagIfRelevent = [&](const std::string& flag){
			if(flag.find("-I") != std::string::npos || flag.find("-D") != std::string::npos)
				compileFlags += " " + flag;
		};
		for(const auto& depCompileFlag : minimalDepProj.stdCompileOpts.compileFlags)
			addCompileFlagIfRelevent(depCompileFlag);
		for(const auto& depCompileFlag : minimalDepProj.extraCompileOpts.compileFlags)
			addCompileFlagIfRelevent(depCompileFlag);
	}

	if(proj.type == project::LIBRARY){
		compileFlags += " -fPIC";//make sure to add this if we're building a lib, as then we will be linking with -fPIC
	}

	const auto checkCommand = [](const std::string& command){return system((command + "  > /dev/null 2>&1").c_str()) == 0;};

	std::string compiler;
	if(proj.os == osVer::hostOsVer()){//we can use the normal system compiler
		if(checkCommand("g++ --version")){
			compiler = "g++";
		}else if(checkCommand("clang++ --version")){
			compiler = "clang++";
		}else{
			std::cerr<<"Error: failed to detect c++ compiler"<<std::endl;
			return false;
		}
	}else{//we need to really make sure we know which compiler we're using
		if(!compilerNames.contains(proj.os)){
			std::cerr<<"Error: no known c++ compiler matches the requested OS/arch combo ("<<proj.os.createOsString()<<")"<<std::endl;
			return false;
		}
		auto compilers = compilerNames.at(proj.os);
		for(const auto& comp : compilers){
			if(checkCommand(comp + " --version")){
				compiler = comp;
				break;
			}
		}
		if(compiler.empty()){
			std::cerr<<"Error: failed to detect c++ compiler"<<std::endl;
			return false;
		}
	}

	if(checkCommand("ccache --print-version")){
		std::cout<<"Note: ccache found. Utilizing..."<<std::endl;
		compiler = "ccache " + compiler;
	}else{
		std::cout<<"Note: ccache not found.  It is highly recommended to utilize ccache for repeated compilations"<<std::endl;
	}

	auto sourceFilesRemaining = proj.stdCompileOpts.sourceFiles;
	for(const auto& file : proj.extraCompileOpts.sourceFiles)
		sourceFilesRemaining.push_back(file);

	auto sourceFilesToLink = sourceFilesRemaining;

	std::filesystem::path currentPath = std::filesystem::current_path();
	std::filesystem::current_path(src_path);

	std::mutex sourceFilesRemaining_mut;
	std::atomic<bool> jobFailed = false;
	std::string jobFailedSourceFile;

	std::vector<std::thread> workerThreads;
	for(unsigned int i=0;i<std::thread::hardware_concurrency();i++){
		workerThreads.push_back(std::thread([&](){
			while(true){
				sourceFilesRemaining_mut.lock();
				if(jobFailed || sourceFilesRemaining.empty()){
					sourceFilesRemaining_mut.unlock();
					return;
				}
				std::string sourceFile = sourceFilesRemaining.back();
				sourceFilesRemaining.pop_back();
				sourceFilesRemaining_mut.unlock();
				auto sourceFileBuildPath = src_path / sourceFile;
				std::string compileCommand = compiler + " " + sourceFileBuildPath.string() + compileFlags + " -c -o " + sourceFileBuildPath.string() + ".o";
				std::cout<<"Compile $ "<<compileCommand<<std::endl;
				if(system((compileCommand + " > " + sourceFileBuildPath.string() + ".log 2>&1").c_str()) != 0){
					jobFailed = true;
					jobFailedSourceFile = sourceFile;
				}
			}
		}));
	}

	for(auto& thread : workerThreads)
		thread.join();

	std::filesystem::current_path(currentPath);

	if(jobFailed){
		std::cout<<"Compilation Failed :("<<std::endl;
		std::cout<<"Error occurred while compiling "<<jobFailedSourceFile<<std::endl;
		auto sourceFileBuildPath = src_path / jobFailedSourceFile;
		std::ifstream logFile(sourceFileBuildPath.string() + ".log");
		if(logFile.is_open())
			std::cout<<"Error log: "<<logFile.rdbuf()<<std::endl;
		else
			std::cout<<"Error: failed to open job build log"<<std::endl;
		return false;
	}

	std::filesystem::path binPath = buildPath / "bin" / (proj.os.createOsString() + (proj.mode == minimalProjectInfo::DEBUG ? "-debug" : "-release"));
	if(!std::filesystem::exists(binPath))
		std::filesystem::create_directories(binPath);

	std::string linkCommand = compiler;
	for(const auto& sourceFile : sourceFilesToLink){
		auto sourceFileBuildPath = src_path / sourceFile;
		linkCommand += " " + sourceFileBuildPath.string() + ".o";
	}
	linkCommand += compileFlags;
	linkCommand += " -o " + (binPath / proj.getTrueBinName()).string();
	linkCommand += linkFlags;
	if(proj.type == project::LIBRARY){
		linkCommand += " -fPIC -shared";//make sure to actually build it as a shared lib
	}
	std::cout<<"Link $ "<<linkCommand<<std::endl;
	if(runInSrcPath(linkCommand + " > " + (buildPath/"link.log").string() + " 2>&1") != 0){
		std::cout<<"Linking Failed :("<<std::endl;
		std::ifstream logFile((buildPath/"link.log").string());
		if(logFile.is_open())
			std::cout<<"Error log: "<<logFile.rdbuf()<<std::endl;
		else
			std::cout<<"Error: failed to open job link log"<<std::endl;
		return false;
	}

	if(proj.type == project::LIBRARY){//we should also create a static version.
		std::string ar;
		if(proj.os == osVer::hostOsVer()){
			if(checkCommand("ar --version"))
				ar = "ar";
			else{
				std::cerr<<"Error: failed to detect ar"<<std::endl;
				return false;
			}
		}else{
			if(!arNames.contains(proj.os)){
				std::cerr<<"Error: no known ar matches the requested OS/arch combo ("<<proj.os.createOsString()<<")"<<std::endl;
				return false;
			}
			auto ars = arNames.at(proj.os);
			for(const auto& ar_try : ars){
				if(checkCommand(ar_try + " --version")){
					ar = ar_try;
					break;
				}
			}
			if(ar.empty()){
				std::cerr<<"Error: failed to detect ar"<<std::endl;
				return false;
			}
		}


		std::string arCommand = ar + " rcs " + (binPath / (proj.binName + ".a")).string();
		for(const auto& sourceFile : sourceFilesToLink){
			auto sourceFileBuildPath = src_path / sourceFile;
			arCommand += " " + sourceFileBuildPath.string() + ".o";
		}

		std::cout<<"AR $ "<<arCommand<<std::endl;
		if(runInSrcPath(arCommand + " > " + (buildPath/"ar.log").string() + " 2>&1") != 0){
			std::cout<<"Static Lib Creation Failed :("<<std::endl;
			std::ifstream logFile((buildPath/"ar.log").string());
			if(logFile.is_open())
				std::cout<<"Error log: "<<logFile.rdbuf()<<std::endl;
			else
				std::cout<<"Error: failed to open job ar log"<<std::endl;
			return false;
		}
	}

	std::cout<<"Build finished successfully :)"<<std::endl;
	buildInfo lastBuildInfo{proj.os, proj.mode};
	lastBuildInfo.serializeToFile((buildPath / "lastBuildInfo.json").string());

	return true;
}

nlohmann::json builder::buildInfo::serialize() const{
	nlohmann::json j{};
	j["os"] = os.createOsString();
	j["mode"] = (mode == minimalProjectInfo::DEBUG ? "debug" : "release");
	return j;
}

void builder::buildInfo::deserialize(const nlohmann::json& j){
	if(j.contains("os")){
		auto parseTry = osVer::parseOsString(j["os"]);
		if(parseTry)
			os = *parseTry;
	}
	if(j.contains("mode")){
		if(j["mode"] == "debug")
			mode = minimalProjectInfo::DEBUG;
		else if(j["mode"] == "release")
			mode = minimalProjectInfo::RELEASE;
	}
}


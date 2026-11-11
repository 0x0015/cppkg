#include "builder.hpp"
#include "paths.hpp"
#include <thread>
#include <mutex>
#include <atomic>
#include <iostream>
#include <fstream>

bool pathContainsDirectory(const std::filesystem::path& p, const std::string& dirName){
	for(const auto& component : p){
		if(component.string() == dirName)
			return true;
	}
	return false;
}

bool builder::build(){
	auto buildPath = getTempPath("build");
	auto src_path = buildPath / "src";
	if(!std::filesystem::exists(src_path))
		std::filesystem::create_directories(src_path);

	const auto copyReleventFile = [&](const std::filesystem::path& file){
		auto relPath = std::filesystem::relative(file);
		auto relPathParentFolder = relPath.parent_path();
		std::filesystem::create_directories(src_path / relPathParentFolder);
		std::filesystem::copy(file, src_path / relPath, std::filesystem::copy_options::overwrite_existing);
	};

	for(const std::filesystem::directory_entry& dir_entry : std::filesystem::recursive_directory_iterator(std::filesystem::current_path())){
		if(pathContainsDirectory(dir_entry.path(), "_cppkg"))
			continue;
		if(dir_entry.is_regular_file() && (dir_entry.path().extension() == ".cpp" || dir_entry.path().extension() == ".c" || dir_entry.path().extension() == ".hpp" || dir_entry.path().extension() == ".h" || dir_entry.path().extension() == ".inc")){
			copyReleventFile(dir_entry.path());
		}
	}

	std::string compileFlags;
	for(const auto& flag : proj.norm.compileFlags)
		compileFlags += " " + flag;

	if(mode == DEBUG)
		for(const auto& flag : proj.debug_extra.compileFlags)
			compileFlags += " " + flag;
	if(mode == RELEASE)
		for(const auto& flag : proj.release_extra.compileFlags)
			compileFlags += " " + flag;

	std::string linkFlags;
	for(const auto& flag : proj.norm.linkFlags)
		linkFlags += " " + flag;

	if(mode == DEBUG)
		for(const auto& flag : proj.debug_extra.linkFlags)
			linkFlags += " " + flag;
	if(mode == RELEASE)
		for(const auto& flag : proj.release_extra.linkFlags)
			linkFlags += " " + flag;

	std::string compiler;
	if(system("g++ --version > /dev/null 2>&1") == 0){
		compiler = "g++";
	}else if(system("clang++ --version > /dev/null 2>&1") == 0){
		compiler = "clang++";
	}else{
		std::cerr<<"Error: failed to detect c++ compiler"<<std::endl;
		return false;
	}

	if(system("ccache --print-version > /dev/null 2>&1") == 0){
		std::cout<<"Note: ccache found. Utilizing..."<<std::endl;
		compiler = "ccache " + compiler;
	}

	auto sourceFilesRemaining = proj.norm.sourceFiles;
	if(mode == DEBUG)
		for(const auto& file : proj.debug_extra.sourceFiles)
			sourceFilesRemaining.push_back(file);
	if(mode == RELEASE)
		for(const auto& file : proj.release_extra.sourceFiles)
			sourceFilesRemaining.push_back(file);
	auto sourceFilesToLink = sourceFilesRemaining;

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

	std::string linkCommand = compiler;
	for(const auto& sourceFile : sourceFilesToLink){
		auto sourceFileBuildPath = src_path / sourceFile;
		linkCommand += " " + sourceFileBuildPath.string() + ".o";
	}
	linkCommand += compileFlags;
	linkCommand += " -o " + proj.binName;
	if(proj.type == project::LIBRARY)
		linkCommand += ".so";
	linkCommand += linkFlags;
	std::cout<<"Link $ "<<linkCommand<<std::endl;
	if(system((linkCommand + " > " + (buildPath/"link.log").string() + " 2>&1").c_str()) != 0){
		std::cout<<"Linking Failed :("<<std::endl;
		std::ifstream logFile((buildPath/"link.log").string());
		if(logFile.is_open())
			std::cout<<"Error log: "<<logFile.rdbuf()<<std::endl;
		else
			std::cout<<"Error: failed to open job build log"<<std::endl;
		return false;
	}

	return true;
}


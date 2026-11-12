#include "paths.hpp"

std::filesystem::path getTempPath(const std::string& subname){
	std::filesystem::path cwd = std::filesystem::current_path();

	auto cppkg_path = cwd / "_cppkg";
	if(!std::filesystem::exists(cppkg_path))
		std::filesystem::create_directories(cppkg_path);

	auto subname_path = cppkg_path / subname;
	if(!std::filesystem::exists(subname_path))
		std::filesystem::create_directories(subname_path);

	return subname_path;
}

bool pathContainsDirectory(const std::filesystem::path& p, const std::string& dirName){
	for(const auto& component : p){
		if(component.string() == dirName)
			return true;
	}
	return false;
}


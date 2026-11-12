#pragma once
#include "project.hpp"

class depMan{
public:
	project& proj;
	depMan(project& p) : proj(p){}
	bool resolveDeps();
	std::unordered_map<std::string, project> downloadedManifests;
	bool createPackage();
};


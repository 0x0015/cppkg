#pragma once
#include "project.hpp"

class depMan{
public:
	minimalProjectInfo& proj;
	depMan(minimalProjectInfo& p) : proj(p){}
	bool resolveDeps();
	std::unordered_map<std::string, project> downloadedManifests;
	bool createPackage();
};


#pragma once
#include "project.hpp"

class depMan{
public:
	project& proj;
	depMan(project& p) : proj(p){}
	bool resolveDeps();
};


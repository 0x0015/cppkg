#pragma once
#include "project.hpp"
#include "depMan.hpp"

class builder{
public:
	project& proj;
	depMan& deps;
	builder(depMan& d) : proj(d.proj), deps(d){}
	enum build_mode{
		DEBUG=0,
		RELEASE=1
	} mode = DEBUG;
	bool build();
};


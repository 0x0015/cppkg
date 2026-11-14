#pragma once
#include "project.hpp"
#include "depMan.hpp"

class builder{
public:
	minimalProjectInfo& proj;
	depMan& deps;
	builder(depMan& d) : proj(d.proj), deps(d){}
	bool build();

	class buildInfo : public jsonSerializable{
	public:
		buildInfo() = default;
		buildInfo(osVer _os, minimalProjectInfo::build_mode _mode) : os(_os), mode(_mode){}
		osVer os;
		minimalProjectInfo::build_mode mode;
		nlohmann::json serialize() const override;
		void deserialize(const nlohmann::json& j) override;
	};
};


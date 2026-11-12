#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "json.hpp"

class project : public jsonSerializable{
public:
	std::string name;
	enum projec_type{
		LIBRARY = 0,
		EXECUTABLE = 1
	} type;
	std::string binName;
	std::string version;
	struct compOptions{
		std::vector<std::string> sourceFiles;
		std::vector<std::string> compileFlags;
		std::vector<std::string> linkFlags;
	};
	compOptions norm;
	compOptions debug_extra;
	compOptions release_extra;
	std::vector<std::string> deps;
	std::unordered_map<std::string, std::string> customCommands;
	std::vector<std::string> prebuildCommands, postbuildCommands;
	std::vector<std::string> extraCleanCommands;
	std::optional<std::string> externalBuildCommand;
	nlohmann::json serialize() const override;
	void deserialize(const nlohmann::json& j) override;
	void init_default();
};


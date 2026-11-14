#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include "json.hpp"
#include "osVer.hpp"

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

		std::optional<std::string> externalBuildCommand;
	};
	compOptions norm;
	compOptions debug_extra;
	compOptions release_extra;

	std::map<osVer, compOptions> osNorm_ExtraOpts;
	std::map<osVer, compOptions> osDebug_ExtraOpts;
	std::map<osVer, compOptions> osRelease_ExtraOpts;

	std::vector<std::string> deps;

	std::unordered_map<std::string, std::string> customCommands;
	std::vector<std::string> prebuildCommands, postbuildCommands;
	std::vector<std::string> extraCleanCommands;

	nlohmann::json serialize() const override;
	void deserialize(const nlohmann::json& j) override;
	void init_default();
};

class minimalProjectInfo{
public:
	minimalProjectInfo(const project& proj, bool isDebug, osVer os);
	const project& proj;
	const std::string& name;
	const project::projec_type& type;
	const std::string& binName;
	const std::string& version;
	const project::compOptions& stdCompileOpts;
	project::compOptions extraCompileOpts;
	const std::vector<std::string>& deps;
	const std::unordered_map<std::string, std::string>& customCommands;
	const std::vector<std::string>& prebuildCommands;
	const std::vector<std::string>& postbuildCommands;
	const std::vector<std::string>& extraCleanCommands;
	enum build_mode{
		DEBUG=0,
		RELEASE=1
	} mode;
	osVer os;
	std::string getTrueBinName() const;
};


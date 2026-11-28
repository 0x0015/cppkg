#include "customCommand.hpp"
#include <iostream>
#include "paths.hpp"

void stringReplaceAll(std::string& s, std::string searchString, std::string replaceString) {
    if (s == "" || searchString == "") {
        return;
    }

    int sSize = searchString.size();
    std::size_t index = s.find(searchString);

    while (index != std::string::npos)
    {
        s.replace(index, sSize, replaceString);
        index = s.find(searchString);
    }
}

std::string substitutePathsIntoCommand(const std::string& command, const minimalProjectInfo& proj){
	std::string modifiedCommand = command;

	auto buildPath = getTempPath("build");
	stringReplaceAll(modifiedCommand, "{build}", buildPath.string());

	std::filesystem::path binPath = buildPath / "bin" / (proj.os.createOsString() + (proj.mode == minimalProjectInfo::DEBUG ? "-debug" : "-release"));
	stringReplaceAll(modifiedCommand, "{bin}", binPath.string());

	return modifiedCommand;
}

int runCustomCommand(const std::string& command, const minimalProjectInfo& proj){
	return system(substitutePathsIntoCommand(command, proj).c_str());
}


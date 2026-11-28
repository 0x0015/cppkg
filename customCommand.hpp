#pragma once
#include "project.hpp"

std::string substitutePathsIntoCommand(const std::string& command, const minimalProjectInfo& proj);
int runCustomCommand(const std::string& command, const minimalProjectInfo& proj);


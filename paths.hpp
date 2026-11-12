#pragma once
#include <filesystem>

std::filesystem::path getTempPath(const std::string& subname);

bool pathContainsDirectory(const std::filesystem::path& p, const std::string& dirName);


#include "osVer.hpp"
#include <array>
#include <iostream>

std::string osVer::createOsString() const{
	constexpr std::array<const char*, 4> archStrs = {"x86", "x86_64", "arm", "aarch64"};
	constexpr std::array<std::string_view, 2> osStrs = {"linux", "win"};
	return std::string(osStrs[(unsigned int)type]) + "-" + archStrs[arch];
}

std::optional<osVer> osVer::parseOsString(const std::string& osStr){
	std::string_view str(osStr);
	osVer::osType osTy;
	if(str.starts_with("linux")){
		osTy = osVer::Linux;
		str = str.substr(5);
	}else if(str.starts_with("lin")){
		osTy = osVer::Linux;
		str = str.substr(3);
	}else if(str.starts_with("windows")){
		osTy = osVer::Windows;
		str = str.substr(7);
	}else if(str.starts_with("win")){
		osTy = osVer::Windows;
		str = str.substr(3);
	}else{
		lastOsStringParseError = "parsing OS string \"" + osStr + "\": unknown OS type";
		return {};
	}

	if(!str.starts_with("-")){
		lastOsStringParseError = "parsing OS string \"" + osStr + "\": syntax error";
		return {};
	}
	str = str.substr(1);

	osVer::osArch osArch;
	if(str.starts_with("x86_64")){
		osArch = x86_64;
	}else if(str.starts_with("x86")){
		osArch = x86;
	}else if(str.starts_with("arm")){
		osArch = arm;
	}else if(str.starts_with("aarch64")){
		osArch = aarch64;
	}else{
		lastOsStringParseError = "parsing OS string \"" + osStr + "\": unknown OS arch";
		return {};
	}

	return osVer{osArch, osTy};
}


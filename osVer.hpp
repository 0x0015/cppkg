#pragma once
#include <string>
#include <optional>

struct osVer{
	enum osArch{
		x86 = 0,
		x86_64 = 1,
		arm = 2,
		aarch64 = 3
	} arch;
	enum osType{
		Linux = 0,
		Windows = 1
	} type;
	static constexpr osVer hostOsVer(){
		osType ty{};
#if defined(_WIN32) || defined(_WIN64)
		ty = Windows;
#elif defined(__linux__) || defined(__gnu_linux__)
		ty = Linux;
#else
		static_assert(false, "Unable to detect host os type");
#endif
		osArch ach{};
#if defined(__x86_64__) || defined(_M_X64)
		ach = x86_64;
#elif defined(__i386) || defined(_M_IX86)
		ach = x86;
#elif defined(__aarch64__) || defined(_M_ARM64)
		ach = aarch64;
#elif defined(__arm__) || defined(_M_ARM)
		ach = arm;
#else
		static_assert(false, "Unable to detect host os arch");
#endif
		return osVer{ach, ty};
	}
	std::string createOsString() const;
	static std::optional<osVer> parseOsString(const std::string& osStr);
	constexpr auto operator<=>(const osVer& other) const = default;
	static inline std::string lastOsStringParseError;
};


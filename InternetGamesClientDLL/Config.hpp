#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

namespace Config {
#ifdef XP_GAMES
	extern std::string host;
	extern std::string port;
#else
	extern std::wstring host;
	extern std::wstring port;
#endif

	bool LoadConfig();
}

#include "Config.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <windows.h>
#pragma comment(lib, "Shell32.lib")

namespace Config {
#ifdef XP_GAMES
	std::string host = "127.0.0.1";
	std::string port = "1234";
#else
	std::wstring host = L"127.0.0.1";
	std::wstring port = L"1234";
#endif

	extern "C" IMAGE_DOS_HEADER __ImageBase;

	std::string GetConfigPath() {
		char path[MAX_PATH];
		// Get full path of the DLL (or EXE if this code is inside an EXE)
		GetModuleFileNameA((HMODULE)&__ImageBase, path, MAX_PATH);

		std::string fullPath(path);
		size_t pos = fullPath.find_last_of("\\/");
		if (pos != std::string::npos) {
			fullPath = fullPath.substr(0, pos); // remove filename
		}

		return fullPath + "\\config.ini"; // config.ini next to DLL
	}


	bool CreateDefaultConfig() {
		std::string configPath = GetConfigPath();
		std::ofstream file(configPath);
		if (!file.is_open()) {
			return false;
		}

		file << "; Configuration file for custom server\n";
		file << "host=127.0.0.1\n";
		file << "port=1234\n";
		file.close();

		return true;
	}

	bool LoadConfig() {
		std::string configPath = GetConfigPath();

		std::ifstream file(configPath);
		if (!file.is_open()) {
			printf("Config file not found at: %s\n", configPath.c_str());
			if (CreateDefaultConfig()) {
				printf("Created default config file at: %s\n", configPath.c_str());
				file.open(configPath);
				if (!file.is_open()) {
					printf("Failed to open newly created config file\n");
					return false;
				}
			}
			else {
				printf("Failed to create config file, using defaults\n");
				return false;
			}
		}

		printf("Loading config from: %s\n", configPath.c_str());
		std::string line;
		bool loadedHost = false;
		bool loadedPort = false;

		while (std::getline(file, line)) {
			if (line.empty() || line[0] == ';' || line[0] == '#')
				continue;

			std::istringstream iss(line);
			std::string key, value;
			if (std::getline(iss, key, '=') && std::getline(iss, value)) {
				key.erase(0, key.find_first_not_of(" \t"));
				key.erase(key.find_last_not_of(" \t") + 1);
				value.erase(0, value.find_first_not_of(" \t"));
				value.erase(value.find_last_not_of(" \t") + 1);

				if (key == "host") {
#ifdef XP_GAMES
					host = value;
#else
					int size_needed = MultiByteToWideChar(CP_ACP, 0, value.c_str(), (int)value.size(), NULL, 0);
					host = std::wstring(size_needed, 0);
					MultiByteToWideChar(CP_ACP, 0, value.c_str(), (int)value.size(), &host[0], size_needed);
#endif
					loadedHost = true;
					printf("Loaded host: %s\n", value.c_str());
				}
				else if (key == "port") {
#ifdef XP_GAMES
					port = value;
#else
					int size_needed = MultiByteToWideChar(CP_ACP, 0, value.c_str(), (int)value.size(), NULL, 0);
					port = std::wstring(size_needed, 0);
					MultiByteToWideChar(CP_ACP, 0, value.c_str(), (int)value.size(), &port[0], size_needed);
#endif
					loadedPort = true;
					printf("Loaded port: %s\n", value.c_str());
				}
			}
		}

		file.close();

		if (!loadedHost) {
			printf("Host not found in config, using default: %s\n",
#ifdef XP_GAMES
				host.c_str()
#else
				std::string(host.begin(), host.end()).c_str()
#endif
			);
		}
		if (!loadedPort) {
			printf("Port not found in config, using default: %s\n",
#ifdef XP_GAMES
				port.c_str()
#else
				std::string(port.begin(), port.end()).c_str()
#endif
			);
		}

		return true;
	}
}

#include "MonitorApp.hpp"
#include <iostream>

#ifdef NDEBUG
#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
#endif

int main() {
#ifdef NDEBUG
	// Hide console window in Release mode
	HWND hWnd = GetConsoleWindow();
	ShowWindow(hWnd, SW_HIDE);
#endif

	std::wcout << L"ZClient Multi-Instance Monitor Starting..." << std::endl;

	WCHAR currentDir[MAX_PATH];
	GetCurrentDirectoryW(MAX_PATH, currentDir);
	std::wcout << L"Current directory: " << currentDir << std::endl;

	ProcessMonitor monitor(L"zclientm.exe", 1000);

	if (!monitor.Initialize()) {
		std::wcout << L"Failed to initialize monitor. Exiting." << std::endl;
		return 1;
	}

	std::wcout << L"Multi-instance monitor initialized successfully." << std::endl;
	std::wcout << L"Monitoring for ALL zclientm.exe instances..." << std::endl;
	std::wcout << L"- Will run injector for EVERY new visible instance" << std::endl;
	std::wcout << L"- Supports multiple game instances (checkers, backgammon, reversi, etc.)" << std::endl;
	std::wcout << L"- Injector will run each time you start a new game" << std::endl;

#ifdef NDEBUG
	// In Release mode, run indefinitely without console interaction
	std::wcout << L"Running in background mode..." << std::endl;
	monitor.StartMonitoring();

	// Keep the program running with message loop
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
#else
	// Debug mode: show console and wait for user input
	std::wcout << L"Press Enter to exit." << std::endl;
	monitor.StartMonitoring();
	std::cin.get();
	std::wcout << L"Shutting down monitor..." << std::endl;
#endif

	return 0;
}
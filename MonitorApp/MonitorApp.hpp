#ifndef MONITORAPP_HPP
#define MONITORAPP_HPP

#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <iostream>
#include <shellapi.h>
#include <vector>
#include <map>
#include <set>

class ProcessMonitor {
private:
	std::wstring m_processName;
	DWORD m_checkInterval;
	bool m_running;

	// Track multiple instances - PID -> isTracked
	std::map<DWORD, bool> m_trackedProcesses;

	// Process management
	std::vector<DWORD> GetAllProcessIdsByName();
	bool HasVisibleWindow(DWORD pid);

	// Injector management
	bool IsInjectorRunning();
	bool LaunchInjector();

	static DWORD WINAPI MonitorThread(LPVOID lpParam);
	friend BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);

public:
	ProcessMonitor(const std::wstring& processName, DWORD checkInterval = 1000);
	~ProcessMonitor();

	bool Initialize();
	void StartMonitoring();
	void StopMonitoring();
};

#endif // MONITORAPP_HPP
#include "MonitorApp.hpp"
#include <iostream>
#include <shellapi.h>
#include <chrono>

ProcessMonitor::ProcessMonitor(const std::wstring& processName, DWORD checkInterval)
	: m_processName(processName), m_checkInterval(checkInterval),
	m_running(false) {
}

ProcessMonitor::~ProcessMonitor() {
	StopMonitoring();
}

bool ProcessMonitor::Initialize() {
#ifdef NDEBUG
	std::wcout << L"Initializing multi-instance process monitor for: " << m_processName << std::endl;
#endif
	return true;
}

std::vector<DWORD> ProcessMonitor::GetAllProcessIdsByName() {
	std::vector<DWORD> pids;

	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE) {
		return pids;
	}

	PROCESSENTRY32W pe32;
	pe32.dwSize = sizeof(PROCESSENTRY32W);

	if (!Process32FirstW(hSnapshot, &pe32)) {
		CloseHandle(hSnapshot);
		return pids;
	}

	do {
		if (_wcsicmp(pe32.szExeFile, m_processName.c_str()) == 0) {
			pids.push_back(pe32.th32ProcessID);
		}
	} while (Process32NextW(hSnapshot, &pe32));

	CloseHandle(hSnapshot);
	return pids;
}

struct EnumData {
	DWORD processId;
	bool hasVisibleWindow;
};

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
	EnumData* data = reinterpret_cast<EnumData*>(lParam);

	DWORD windowPid;
	GetWindowThreadProcessId(hwnd, &windowPid);

	if (windowPid == data->processId) {
		if (IsWindowVisible(hwnd)) {
			LONG style = GetWindowLong(hwnd, GWL_STYLE);
			LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

			if (!(exStyle & WS_EX_TOOLWINDOW)) {
				int titleLength = GetWindowTextLength(hwnd);
				if (titleLength > 0 || (style & WS_CAPTION)) {
					data->hasVisibleWindow = true;
					return FALSE; // Stop enumeration
				}
			}
		}
	}
	return TRUE; // Continue enumeration
}

bool ProcessMonitor::HasVisibleWindow(DWORD pid) {
	if (pid == 0) return false;

	EnumData data;
	data.processId = pid;
	data.hasVisibleWindow = false;

	EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data));
	return data.hasVisibleWindow;
}

bool ProcessMonitor::IsInjectorRunning() {
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE) {
		return false;
	}

	PROCESSENTRY32W pe32;
	pe32.dwSize = sizeof(PROCESSENTRY32W);

	bool found = false;
	if (Process32FirstW(hSnapshot, &pe32)) {
		do {
			if (_wcsicmp(pe32.szExeFile, L"DLLInjector_XP.exe") == 0) {
				found = true;
				break;
			}
		} while (Process32NextW(hSnapshot, &pe32));
	}

	CloseHandle(hSnapshot);
	return found;
}

bool ProcessMonitor::LaunchInjector() {
	if (IsInjectorRunning()) {
#ifdef NDEBUG
		std::wcout << L"DLLInjector_XP.exe is already running" << std::endl;
#endif
		return true;
	}

	SHELLEXECUTEINFOW sei = { 0 };
	sei.cbSize = sizeof(SHELLEXECUTEINFOW);
	sei.lpFile = L"DLLInjector_XP.exe";
	sei.nShow = SW_SHOWNORMAL;

	if (ShellExecuteExW(&sei)) {
#ifdef NDEBUG
		std::wcout << L"Launched DLLInjector_XP.exe" << std::endl;
#endif
		return true;
	}
	else {
#ifdef NDEBUG
		std::wcout << L"Failed to launch DLLInjector_XP.exe. Error: " << GetLastError() << std::endl;
#endif
		return false;
	}
}

DWORD WINAPI ProcessMonitor::MonitorThread(LPVOID lpParam) {
	ProcessMonitor* pMonitor = static_cast<ProcessMonitor*>(lpParam);
	int consecutiveNoProcessChecks = 0;

	while (pMonitor->m_running) {
		// Get ALL current instances
		std::vector<DWORD> currentPids = pMonitor->GetAllProcessIdsByName();

		bool hasNewVisibleInstance = false;

#ifdef NDEBUG
		if (!currentPids.empty()) {
			std::wcout << L"Found " << currentPids.size() << L" instance(s) of " << pMonitor->m_processName << std::endl;
		}
#endif

		// Track which PIDs we've seen in this iteration
		std::set<DWORD> currentVisiblePids;

		// Check each process individually
		for (DWORD pid : currentPids) {
			bool hasWindow = pMonitor->HasVisibleWindow(pid);

			if (hasWindow) {
				currentVisiblePids.insert(pid);

				// Check if this is a NEW visible process we haven't tracked before
				if (pMonitor->m_trackedProcesses.find(pid) == pMonitor->m_trackedProcesses.end()) {
					hasNewVisibleInstance = true;
#ifdef NDEBUG
					std::wcout << L"NEW visible instance detected (PID: " << pid << L") - will launch injector" << std::endl;
#endif
				}

				// Track this visible process
				pMonitor->m_trackedProcesses[pid] = true;
			}
		}

		// Clean up dead processes from tracking (processes that are no longer running)
		for (auto it = pMonitor->m_trackedProcesses.begin(); it != pMonitor->m_trackedProcesses.end(); ) {
			DWORD pid = it->first;

			// Check if process is still alive and visible
			bool isStillAliveAndVisible = false;
			if (currentVisiblePids.find(pid) != currentVisiblePids.end()) {
				isStillAliveAndVisible = true;
			}
			else {
				// Double check if process is still running
				HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
				if (hProcess) {
					DWORD exitCode;
					if (GetExitCodeProcess(hProcess, &exitCode) && exitCode == STILL_ACTIVE) {
						// Process is still running but no longer visible? Check window again
						if (pMonitor->HasVisibleWindow(pid)) {
							isStillAliveAndVisible = true;
							currentVisiblePids.insert(pid); // Add back to current visible
						}
					}
					CloseHandle(hProcess);
				}
			}

			if (!isStillAliveAndVisible) {
				it = pMonitor->m_trackedProcesses.erase(it);
#ifdef NDEBUG
				std::wcout << L"Process no longer visible or running (PID: " << pid << L") - removed from tracking" << std::endl;
#endif
			}
			else {
				++it;
			}
		}

		// LAUNCH INJECTOR WHENEVER NEW VISIBLE INSTANCES APPEAR
		if (hasNewVisibleInstance) {
#ifdef NDEBUG
			std::wcout << L"New visible instance(s) detected - LAUNCHING INJECTOR" << std::endl;
#endif
			if (pMonitor->LaunchInjector()) {
#ifdef NDEBUG
				std::wcout << L"Injector launched successfully for new instance(s)" << std::endl;
#endif
			}
			else {
#ifdef NDEBUG
				std::wcout << L"Failed to launch injector" << std::endl;
#endif
			}
		}

		// Adaptive sleep based on activity
		if (currentPids.empty()) {
			consecutiveNoProcessChecks++;
			if (consecutiveNoProcessChecks > 10) {
				Sleep(2000); // Sleep longer when no processes for a while
			}
			else if (consecutiveNoProcessChecks > 5) {
				Sleep(1500);
			}
			else {
				Sleep(pMonitor->m_checkInterval);
			}
		}
		else {
			consecutiveNoProcessChecks = 0;
			Sleep(pMonitor->m_checkInterval);
		}

		// Process window messages
		MSG msg;
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return 0;
}

void ProcessMonitor::StartMonitoring() {
	if (m_running) return;

	m_running = true;
	HANDLE hThread = CreateThread(NULL, 0, MonitorThread, this, 0, NULL);

	if (hThread) {
		CloseHandle(hThread);
#ifdef NDEBUG
		std::wcout << L"Started multi-instance monitoring for: " << m_processName << std::endl;
		std::wcout << L"Will run injector for ALL new visible instances" << std::endl;
#endif
	}
	else {
		m_running = false;
#ifdef NDEBUG
		std::wcout << L"Failed to create monitor thread" << std::endl;
#endif
	}
}

void ProcessMonitor::StopMonitoring() {
	m_running = false;
	m_trackedProcesses.clear();
}
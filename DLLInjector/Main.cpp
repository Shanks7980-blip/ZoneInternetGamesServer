#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <cctype>
#include <vector>

#ifdef NDEBUG
#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
#endif

// ============================================================================
// Utility Functions
// ============================================================================

void ShowError(const std::string& msg) {
	std::cout << "[ERROR] " << msg << " failed with error code: " << GetLastError() << std::endl;
}

void LogInfo(const std::string& msg) {
	std::cout << "[INFO] " << msg << std::endl;
}

std::string WideToString(const WCHAR* wideStr) {
	if (wideStr == NULL) return "";

	int bufferSize = WideCharToMultiByte(CP_ACP, 0, wideStr, -1, NULL, 0, NULL, NULL);
	if (bufferSize == 0) return "";

	std::string result(bufferSize, 0);
	WideCharToMultiByte(CP_ACP, 0, wideStr, -1, &result[0], bufferSize, NULL, NULL);

	// Remove null terminator
	if (!result.empty() && result[result.size() - 1] == '\0') {
		result.pop_back();
	}

	return result;
}

bool CaseInsensitiveCompare(const std::string& str1, const std::string& str2) {
	if (str1.size() != str2.size()) return false;

	for (size_t i = 0; i < str1.size(); ++i) {
		if (std::tolower(static_cast<unsigned char>(str1[i])) !=
			std::tolower(static_cast<unsigned char>(str2[i]))) {
			return false;
		}
	}
	return true;
}

// ============================================================================
// Process Management
// ============================================================================

std::vector<DWORD> FindAllProcessesByName(const std::string& processName) {
	std::vector<DWORD> pids;

	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE) {
		ShowError("CreateToolhelp32Snapshot");
		return pids;
	}

	PROCESSENTRY32W pe32;
	pe32.dwSize = sizeof(PROCESSENTRY32W);

	if (!Process32FirstW(hSnapshot, &pe32)) {
		ShowError("Process32First");
		CloseHandle(hSnapshot);
		return pids;
	}

	LogInfo("Scanning for all " + processName + " instances...");

	do {
		std::string currentProcessName = WideToString(pe32.szExeFile);

		if (CaseInsensitiveCompare(currentProcessName, processName)) {
			pids.push_back(pe32.th32ProcessID);
			LogInfo("Found " + processName + " instance (PID: " + std::to_string(pe32.th32ProcessID) + ")");
		}

	} while (Process32NextW(hSnapshot, &pe32));

	CloseHandle(hSnapshot);
	return pids;
}

DWORD FindProcessByName(const std::string& processName) {
	auto pids = FindAllProcessesByName(processName);
	return pids.empty() ? 0 : pids[0];
}

std::vector<DWORD> GetProcessIdsManual() {
	std::vector<DWORD> pids;
	std::cout << "Enter zclientm.exe process IDs (one per line, 0 to finish):" << std::endl;

	DWORD pid;
	while (true) {
		std::cout << "PID: ";
		std::cin >> pid;
		if (pid == 0) break;
		pids.push_back(pid);
	}

	return pids;
}

// ============================================================================
// DLL Injection
// ============================================================================

bool IsAlreadyInjected(DWORD pid, const std::string& dllName) {
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
	if (hSnapshot == INVALID_HANDLE_VALUE) {
		return false;
	}

	MODULEENTRY32W me32;
	me32.dwSize = sizeof(MODULEENTRY32W);

	bool alreadyInjected = false;
	if (Module32FirstW(hSnapshot, &me32)) {
		do {
			std::string currentModule = WideToString(me32.szModule);
			if (CaseInsensitiveCompare(currentModule, dllName)) {
				alreadyInjected = true;
				break;
			}
		} while (Module32NextW(hSnapshot, &me32));
	}

	CloseHandle(hSnapshot);
	return alreadyInjected;
}

bool InjectDLL(DWORD pid, const std::string& dllPath, const std::string& dllName) {
	// Check if already injected to avoid duplicate injection
	if (IsAlreadyInjected(pid, dllName)) {
		LogInfo("DLL already injected into PID: " + std::to_string(pid) + " - skipping");
		return true;
	}

	HANDLE hProcess = NULL;
	HANDLE hThread = NULL;
	LPVOID pRemoteMemory = NULL;
	bool success = false;

	LogInfo("Attempting to inject into PID: " + std::to_string(pid));

	// Open target process
	hProcess = OpenProcess(
		PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
		PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
		FALSE, pid
	);

	if (hProcess == NULL) {
		ShowError("OpenProcess for PID " + std::to_string(pid));
		return false;
	}

	LogInfo("Process " + std::to_string(pid) + " opened successfully");

	// Allocate memory in target process
	size_t pathLen = dllPath.length() + 1;
	pRemoteMemory = VirtualAllocEx(hProcess, NULL, pathLen,
		MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

	if (pRemoteMemory == NULL) {
		ShowError("VirtualAllocEx");
		goto cleanup;
	}

	LogInfo("Memory allocated in process " + std::to_string(pid));

	// Write DLL path to target process
	if (!WriteProcessMemory(hProcess, pRemoteMemory, dllPath.c_str(), pathLen, NULL)) {
		ShowError("WriteProcessMemory");
		goto cleanup;
	}

	LogInfo("DLL path written to process " + std::to_string(pid));

	// Get LoadLibraryA address
	FARPROC pLoadLibrary = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
	if (pLoadLibrary == NULL) {
		ShowError("GetProcAddress");
		goto cleanup;
	}

	LogInfo("LoadLibraryA address obtained");

	// Create remote thread
	hThread = CreateRemoteThread(hProcess, NULL, 0,
		(LPTHREAD_START_ROUTINE)pLoadLibrary,
		pRemoteMemory, 0, NULL);

	if (hThread == NULL) {
		ShowError("CreateRemoteThread");
		goto cleanup;
	}

	LogInfo("Remote thread created in process " + std::to_string(pid));

	// Wait for thread completion
	if (WaitForSingleObject(hThread, 5000) == WAIT_OBJECT_0) {
		success = true;
		LogInfo("Injection completed for PID: " + std::to_string(pid));
	}
	else {
		LogInfo("Thread timeout for PID " + std::to_string(pid) + " - injection may still have worked");
		success = true; // Often still successful despite timeout
	}

cleanup:
	// Cleanup resources
	if (hThread != NULL) CloseHandle(hThread);
	if (pRemoteMemory != NULL) VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
	if (hProcess != NULL) CloseHandle(hProcess);

	return success;
}

bool InjectMultipleProcesses(const std::vector<DWORD>& pids, const std::string& dllPath, const std::string& dllName) {
	if (pids.empty()) {
		LogInfo("No processes to inject");
		return false;
	}

	LogInfo("Injecting into " + std::to_string(pids.size()) + " process(es)");

	int successCount = 0;
	for (DWORD pid : pids) {
		if (InjectDLL(pid, dllPath, dllName)) {
			successCount++;
		}
	}

	LogInfo("Successfully injected into " + std::to_string(successCount) + " of " + std::to_string(pids.size()) + " processes");
	return successCount > 0;
}

// ============================================================================
// Main Function
// ============================================================================

int main() {
#ifdef NDEBUG
	// Hide console window in Release mode
	HWND hWnd = GetConsoleWindow();
	ShowWindow(hWnd, SW_HIDE);
#else
	// Show console window in Debug mode
	HWND hWnd = GetConsoleWindow();
	ShowWindow(hWnd, SW_SHOW);
#endif

	// Configuration
	const std::string TARGET_PROCESS = "zclientm.exe";
	const std::string DLL_NAME = "InternetGamesClientDLL_XP.dll";

#ifdef NDEBUG
	// Minimal output in Release mode
	std::cout << "Multi-Instance DLL Injector Running..." << std::endl;
#else
	// Full output in Debug mode
	std::cout << "Multi-Instance zClientm.exe DLL Injector" << std::endl;
	std::cout << "========================================" << std::endl << std::endl;
#endif

	// Get current directory and DLL path
	char dirBuffer[MAX_PATH];
	if (GetCurrentDirectoryA(MAX_PATH, dirBuffer) == 0) {
		ShowError("GetCurrentDirectoryA");
		return 1;
	}

	std::string dllPath = std::string(dirBuffer) + "\\" + DLL_NAME;

#ifndef NDEBUG
	std::cout << "Current directory: " << dirBuffer << std::endl;
	std::cout << "DLL Path: " << dllPath << std::endl;
#endif

	// Verify DLL exists
	if (GetFileAttributesA(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
		std::cout << "Error: " << DLL_NAME << " not found in current directory!" << std::endl;
#ifdef NDEBUG
		return 1;
#else
		system("pause");
		return 1;
#endif
	}

#ifndef NDEBUG
	std::cout << "DLL file found." << std::endl;
	std::cout << std::endl << "Searching for all " << TARGET_PROCESS << " instances..." << std::endl;
#endif

	// Find all target processes
	std::vector<DWORD> pids = FindAllProcessesByName(TARGET_PROCESS);

	if (pids.empty()) {
#ifdef NDEBUG
		// In release mode, just exit if no processes found
		return 1;
#else
		// In debug mode, allow manual PID input
		std::cout << std::endl << "No " << TARGET_PROCESS << " instances found!" << std::endl;
		std::cout << "Please make sure " << TARGET_PROCESS << " is running." << std::endl;

		pids = GetProcessIdsManual();
		if (pids.empty()) {
			return 1;
		}
#endif
	}

#ifndef NDEBUG
	std::cout << std::endl << "Found " << pids.size() << " " << TARGET_PROCESS << " instance(s)" << std::endl;
	for (DWORD pid : pids) {
		std::cout << "  - PID: " << pid << std::endl;
	}
	std::cout << std::endl << "Injecting DLL into all instances..." << std::endl;
#endif

	// Perform injection on all processes
	if (InjectMultipleProcesses(pids, dllPath, DLL_NAME)) {
#ifndef NDEBUG
		std::cout << std::endl << "DLL injection completed successfully!" << std::endl;
		std::cout << "Exiting injector..." << std::endl;
#endif
		return 0;
	}
	else {
#ifndef NDEBUG
		std::cout << std::endl << "DLL injection failed for all processes!" << std::endl;
		system("pause");
#endif
		return 1;
	}
}
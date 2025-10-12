#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>  // ADD THIS LINE for ShellExecuteEx
#include <string>
#include <fstream>
#include <sstream>

// Dialog IDs
#define IDD_MAIN_DIALOG                   2000
#define IDD_FINISH_DIALOG                 2001

// Control IDs for Main Dialog
#define IDC_HOST                          1001
#define IDC_PORT                          1002
#define IDC_ALLUSERS                      1003
#define IDC_STARTMONITOR                  1004
#define IDC_NEXT                          1005
#define IDC_CANCEL                        1006

// Control IDs for Finish Dialog  
#define IDC_BACK                          1007
#define IDC_FINISH                        1008
#define IDC_CANCEL2                       1009

// Resource IDs for embedded files
#define IDR_CONFIGMAIN                    3001
#define IDR_MONITORAPP                    3002
#define IDR_DLLINJECTOR                   3003
#define IDR_CLIENTDLL                     3004

class Installer {
private:
	HINSTANCE hInstance;
	HWND hMainDlg;
	HWND hFinishDlg;
	std::wstring host;
	std::wstring port;
	bool installAllUsers;
	bool startMonitor;

public:
	Installer(HINSTANCE hInst);
	~Installer();

	int Run();

private:
	static INT_PTR CALLBACK MainDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK FinishDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

	INT_PTR MainDlgProcImpl(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
	INT_PTR FinishDlgProcImpl(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

	bool CreateConfigFile(const std::wstring& path);
	bool CreateShortcut(const std::wstring& targetPath, const std::wstring& shortcutPath, const std::wstring& description);
	bool ExtractFiles();
	bool GetInstallPath(std::wstring& path);
	bool GetDesktopPath(std::wstring& path, bool allUsers);
	bool GetStartupPath(std::wstring& path, bool allUsers);
	void StartMonitorApp();

	// Resource extraction methods
	bool ExtractResource(int resourceId, const std::wstring& outputPath, const wchar_t* resourceType);
	bool WriteDataToFile(const std::wstring& path, const void* data, DWORD size);
};
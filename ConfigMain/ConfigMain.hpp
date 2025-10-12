#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <shellapi.h>
#include <commctrl.h>
#include <fstream>
#include <sstream>
#include "resource.h"

#ifdef BUILDING_TRAYICON_DLL
#define TRAYICON_API __declspec(dllexport)
#else
#define TRAYICON_API 
#endif

class TrayManager {
private:
	HWND m_hHiddenWindow = nullptr;
	NOTIFYICONDATAW m_nid = {};
	HINSTANCE m_hInstance = nullptr;

	std::wstring m_currentHost;
	std::wstring m_currentPort;

	static const UINT WM_TRAYICON = WM_APP + 1;
	static const UINT ID_TRAYICON = 1;
	static const UINT IDM_CONFIG = 1001;
	static const UINT IDM_EXIT = 1002;

	static TrayManager* GetThisFromWindow(HWND hWnd) {
		return reinterpret_cast<TrayManager*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
	}

	static LRESULT CALLBACK HiddenWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK ConfigDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

	LRESULT HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	INT_PTR HandleConfigDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	void SaveConfigToFile();
	void LoadConfigFromFile();

public:  // CHANGED: RunMessageLoop is now public
	TrayManager();
	~TrayManager();

	bool Initialize(HINSTANCE hInstance);
	void Show();
	void Hide();
	void Cleanup();
	void UpdateConfig(LPCWSTR host, LPCWSTR port);
	bool GetCurrentConfig(LPWSTR host, LPWSTR port, DWORD bufferSize);
	void RunMessageLoop();  // MOVED to public
};

extern "C" {
	TRAYICON_API BOOL InitializeTrayIcon();
	TRAYICON_API BOOL ShowTrayIcon();
	TRAYICON_API BOOL HideTrayIcon();
	TRAYICON_API BOOL UpdateTrayConfig(LPCWSTR host, LPCWSTR port);
	TRAYICON_API BOOL GetCurrentConfig(LPWSTR host, LPWSTR port, DWORD bufferSize);
	TRAYICON_API BOOL CleanupTrayIcon();
}
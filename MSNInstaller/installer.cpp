#include "installer.h"
#include "resource.h"
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")

Installer::Installer(HINSTANCE hInst) : hInstance(hInst), hMainDlg(NULL), hFinishDlg(NULL),
installAllUsers(false), startMonitor(true) {
}

Installer::~Installer() {
}

int Installer::Run() {
	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&icex);

	DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_MAIN_DIALOG), NULL, MainDlgProc, (LPARAM)this);
	return 0;
}

INT_PTR CALLBACK Installer::MainDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	Installer* pInstaller;

	if (msg == WM_INITDIALOG) {
		pInstaller = (Installer*)lParam;
		SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)pInstaller);
		pInstaller->hMainDlg = hDlg;
	}
	else {
		pInstaller = (Installer*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
	}

	if (pInstaller) {
		return pInstaller->MainDlgProcImpl(hDlg, msg, wParam, lParam);
	}

	return FALSE;
}

INT_PTR CALLBACK Installer::FinishDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	Installer* pInstaller;

	if (msg == WM_INITDIALOG) {
		pInstaller = (Installer*)lParam;
		SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)pInstaller);
		pInstaller->hFinishDlg = hDlg;
	}
	else {
		pInstaller = (Installer*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
	}

	if (pInstaller) {
		return pInstaller->FinishDlgProcImpl(hDlg, msg, wParam, lParam);
	}

	return FALSE;
}

INT_PTR Installer::MainDlgProcImpl(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_INITDIALOG: {
		// Set default values
		SetDlgItemText(hDlg, IDC_HOST, L"192.168.1.92");
		SetDlgItemText(hDlg, IDC_PORT, L"80");
		CheckDlgButton(hDlg, IDC_ALLUSERS, BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_STARTMONITOR, BST_CHECKED);
		return TRUE;
	}

	case WM_COMMAND: {
		switch (LOWORD(wParam)) {
		case IDC_NEXT: {
			wchar_t buffer[256];

			// Get host and port
			GetDlgItemText(hDlg, IDC_HOST, buffer, sizeof(buffer) / sizeof(buffer[0]));
			host = buffer;

			GetDlgItemText(hDlg, IDC_PORT, buffer, sizeof(buffer) / sizeof(buffer[0]));
			port = buffer;

			installAllUsers = IsDlgButtonChecked(hDlg, IDC_ALLUSERS) == BST_CHECKED;
			startMonitor = IsDlgButtonChecked(hDlg, IDC_STARTMONITOR) == BST_CHECKED;

			// Validate input
			if (host.empty() || port.empty()) {
				MessageBox(hDlg, L"Please enter both host and port.", L"Error", MB_ICONERROR);
				return TRUE;
			}

			// Validate port is numeric
			for (wchar_t c : port) {
				if (!iswdigit(c)) {
					MessageBox(hDlg, L"Port must be a numeric value.", L"Error", MB_ICONERROR);
					return TRUE;
				}
			}

			// Perform installation steps
			std::wstring installPath;
			if (!GetInstallPath(installPath)) {
				MessageBox(hDlg, L"Failed to get installation path.", L"Error", MB_ICONERROR);
				return TRUE;
			}

			// Create directory
			if (!CreateDirectory(installPath.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
				MessageBox(hDlg, L"Failed to create installation directory.", L"Error", MB_ICONERROR);
				return TRUE;
			}

			// Extract embedded files
			if (!ExtractFiles()) {
				MessageBox(hDlg, L"Failed to extract files.", L"Error", MB_ICONERROR);
				return TRUE;
			}

			if (!CreateConfigFile(installPath)) {
				MessageBox(hDlg, L"Failed to create config file.", L"Error", MB_ICONERROR);
				return TRUE;
			}

			// Create shortcuts
			std::wstring desktopPath;
			if (GetDesktopPath(desktopPath, installAllUsers)) {
				std::wstring configShortcut = desktopPath + L"\\Game Client Config.lnk";
				std::wstring configTarget = installPath + L"\\ConfigMain.exe";
				CreateShortcut(configTarget, configShortcut, L"Game Client Configuration");
			}

			std::wstring startupPath;
			if (GetStartupPath(startupPath, installAllUsers)) {
				std::wstring monitorShortcut = startupPath + L"\\Game Monitor.lnk";
				std::wstring monitorTarget = installPath + L"\\MonitorApp.exe";
				CreateShortcut(monitorTarget, monitorShortcut, L"Game Monitor");
			}

			// Show finish dialog
			EndDialog(hDlg, 0);
			DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_FINISH_DIALOG), NULL, FinishDlgProc, (LPARAM)this);
			return TRUE;
		}

		case IDC_CANCEL: {
			if (MessageBox(hDlg, L"Are you sure you want to cancel installation?", L"Confirm", MB_YESNO | MB_ICONQUESTION) == IDYES) {
				EndDialog(hDlg, 0);
			}
			return TRUE;
		}
		}
		break;
	}

	case WM_CLOSE: {
		if (MessageBox(hDlg, L"Are you sure you want to cancel installation?", L"Confirm", MB_YESNO | MB_ICONQUESTION) == IDYES) {
			EndDialog(hDlg, 0);
		}
		return TRUE;
	}
	}

	return FALSE;
}

INT_PTR Installer::FinishDlgProcImpl(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_INITDIALOG: {
		return TRUE;
	}

	case WM_COMMAND: {
		switch (LOWORD(wParam)) {
		case IDC_BACK: {
			EndDialog(hDlg, 0);
			DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_MAIN_DIALOG), NULL, MainDlgProc, (LPARAM)this);
			return TRUE;
		}

		case IDC_FINISH: {
			if (startMonitor) {
				StartMonitorApp();
			}
			EndDialog(hDlg, 0);
			return TRUE;
		}

		case IDC_CANCEL2: {
			EndDialog(hDlg, 0);
			return TRUE;
		}
		}
		break;
	}

	case WM_CLOSE: {
		EndDialog(hDlg, 0);
		return TRUE;
	}
	}

	return FALSE;
}

bool Installer::GetInstallPath(std::wstring& path) {
	wchar_t programFiles[MAX_PATH];

	if (installAllUsers) {
		if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES, NULL, 0, programFiles))) {
			path = std::wstring(programFiles) + L"\\GameClient";
			return true;
		}
	}
	else {
		wchar_t appData[MAX_PATH];
		if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, appData))) {
			path = std::wstring(appData) + L"\\GameClient";
			return true;
		}
	}

	return false;
}

bool Installer::GetDesktopPath(std::wstring& path, bool allUsers) {
	wchar_t desktop[MAX_PATH];

	if (allUsers) {
		if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_COMMON_DESKTOPDIRECTORY, NULL, 0, desktop))) {
			path = desktop;
			return true;
		}
	}
	else {
		if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_DESKTOP, NULL, 0, desktop))) {
			path = desktop;
			return true;
		}
	}

	return false;
}

bool Installer::GetStartupPath(std::wstring& path, bool allUsers) {
	wchar_t startup[MAX_PATH];

	if (allUsers) {
		if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_COMMON_STARTUP, NULL, 0, startup))) {
			path = startup;
			return true;
		}
	}
	else {
		if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_STARTUP, NULL, 0, startup))) {
			path = startup;
			return true;
		}
	}

	return false;
}

bool Installer::CreateConfigFile(const std::wstring& path) {
	std::wstring configPath = path + L"\\config.ini";

	std::wofstream file(configPath.c_str());
	if (!file.is_open()) {
		return false;
	}

	file << L"; Game Server Configuration\n";
	file << L"host=" << host << L"\n";
	file << L"port=" << port << L"\n";

	file.close();
	return true;
}

bool Installer::CreateShortcut(const std::wstring& targetPath, const std::wstring& shortcutPath, const std::wstring& description) {
	HRESULT hres;
	IShellLink* pShellLink = NULL;
	IPersistFile* pPersistFile = NULL;

	hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&pShellLink);
	if (FAILED(hres)) return false;

	hres = pShellLink->QueryInterface(IID_IPersistFile, (LPVOID*)&pPersistFile);
	if (FAILED(hres)) {
		pShellLink->Release();
		return false;
	}

	pShellLink->SetPath(targetPath.c_str());
	pShellLink->SetDescription(description.c_str());
	pShellLink->SetWorkingDirectory(targetPath.substr(0, targetPath.find_last_of(L'\\')).c_str());

	hres = pPersistFile->Save(shortcutPath.c_str(), TRUE);

	pPersistFile->Release();
	pShellLink->Release();

	return SUCCEEDED(hres);
}

bool Installer::ExtractFiles() {
	std::wstring installPath;
	if (!GetInstallPath(installPath)) {
		return false;
	}

	// Extract each embedded file
	bool success = true;

	success &= ExtractResource(IDR_CONFIGMAIN, installPath + L"\\ConfigMain.exe", L"BINARY");
	success &= ExtractResource(IDR_MONITORAPP, installPath + L"\\MonitorApp.exe", L"BINARY");
	success &= ExtractResource(IDR_DLLINJECTOR, installPath + L"\\DLLInjector_XP.exe", L"BINARY");
	success &= ExtractResource(IDR_CLIENTDLL, installPath + L"\\InternetGamesClientDLL_XP.dll", L"BINARY");

	return success;
}

bool Installer::ExtractResource(int resourceId, const std::wstring& outputPath, const wchar_t* resourceType) {
	HRSRC hResource = FindResource(hInstance, MAKEINTRESOURCE(resourceId), resourceType);
	if (!hResource) {
		return false;
	}

	HGLOBAL hResourceData = LoadResource(hInstance, hResource);
	if (!hResourceData) {
		return false;
	}

	void* resourceData = LockResource(hResourceData);
	if (!resourceData) {
		return false;
	}

	DWORD resourceSize = SizeofResource(hInstance, hResource);
	if (resourceSize == 0) {
		return false;
	}

	return WriteDataToFile(outputPath, resourceData, resourceSize);
}

bool Installer::WriteDataToFile(const std::wstring& path, const void* data, DWORD size) {
	HANDLE hFile = CreateFile(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		return false;
	}

	DWORD bytesWritten;
	BOOL success = WriteFile(hFile, data, size, &bytesWritten, NULL);
	CloseHandle(hFile);

	return success && (bytesWritten == size);
}

void Installer::StartMonitorApp() {
	std::wstring installPath;
	if (GetInstallPath(installPath)) {
		std::wstring monitorPath = installPath + L"\\MonitorApp.exe";

		SHELLEXECUTEINFO sei = { 0 };
		sei.cbSize = sizeof(SHELLEXECUTEINFO);
		sei.fMask = SEE_MASK_NOCLOSEPROCESS;
		sei.lpFile = monitorPath.c_str();
		sei.lpDirectory = installPath.c_str();
		sei.nShow = SW_SHOWNORMAL;

		ShellExecuteEx(&sei);
	}
}
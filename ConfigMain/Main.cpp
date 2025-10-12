#include <windows.h>
#include <commctrl.h>
#include <fstream>
#include <string>
#include <tchar.h>
#include "resource.h"

class ConfigManager {
private:
	HINSTANCE m_hInstance;
	HWND m_hMainWindow;

	std::wstring m_currentHost;
	std::wstring m_currentPort;

	static ConfigManager* GetThisFromWindow(HWND hWnd) {
		return reinterpret_cast<ConfigManager*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
	}

	static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK ConfigDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

	LRESULT HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	INT_PTR HandleConfigDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	void SaveConfigToFile();
	void LoadConfigFromFile();
	void ShowConfigDialog();

public:
	ConfigManager();
	~ConfigManager();

	bool Initialize(HINSTANCE hInstance);
	void Run();
};

// Global instance
ConfigManager* g_configManager = nullptr;

LRESULT CALLBACK ConfigManager::MainWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	ConfigManager* pThis = GetThisFromWindow(hWnd);
	if (pThis) {
		return pThis->HandleMessage(hWnd, message, wParam, lParam);
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

INT_PTR CALLBACK ConfigManager::ConfigDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	if (message == WM_INITDIALOG) {
		SetWindowLongPtr(hDlg, GWLP_USERDATA, lParam);
		ConfigManager* pThis = reinterpret_cast<ConfigManager*>(lParam);
		if (pThis) {
			SetProp(hDlg, TEXT("ConfigManager"), (HANDLE)pThis);
			return pThis->HandleConfigDialog(hDlg, message, wParam, lParam);
		}
	}

	ConfigManager* pThis = static_cast<ConfigManager*>(GetProp(hDlg, TEXT("ConfigManager")));
	if (pThis) {
		return pThis->HandleConfigDialog(hDlg, message, wParam, lParam);
	}
	return FALSE;
}

ConfigManager::ConfigManager() : m_hInstance(nullptr), m_hMainWindow(nullptr) {
	m_currentHost = TEXT("127.0.0.1");
	m_currentPort = TEXT("1234");
}

ConfigManager::~ConfigManager() {
	if (m_hMainWindow) {
		DestroyWindow(m_hMainWindow);
	}
}

bool ConfigManager::Initialize(HINSTANCE hInstance) {
	m_hInstance = hInstance;

	// Register main window class
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpfnWndProc = MainWndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = TEXT("ServerConfigApp");
	wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TRAY_ICON));
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

	if (!RegisterClassEx(&wc)) return false;

	// Create main window (hidden)
	m_hMainWindow = CreateWindowEx(
		0,
		wc.lpszClassName,
		TEXT("Game Server Configuration"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
		nullptr,
		nullptr,
		hInstance,
		nullptr
	);

	if (!m_hMainWindow) return false;

	SetWindowLongPtr(m_hMainWindow, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

	LoadConfigFromFile();
	return true;
}

void ConfigManager::Run() {
	// Show configuration dialog immediately
	ShowConfigDialog();

	// Message loop
	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

void ConfigManager::ShowConfigDialog() {
	DialogBoxParam(m_hInstance, MAKEINTRESOURCE(IDD_CONFIG_DIALOG), m_hMainWindow, ConfigDialogProc, reinterpret_cast<LPARAM>(this));
}

LRESULT ConfigManager::HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

INT_PTR ConfigManager::HandleConfigDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_INITDIALOG: {
		SetDlgItemText(hDlg, IDC_HOST_EDIT, m_currentHost.c_str());
		SetDlgItemText(hDlg, IDC_PORT_EDIT, m_currentPort.c_str());

		// Center the dialog
		RECT rcOwner, rcDlg;
		GetWindowRect(GetDesktopWindow(), &rcOwner);
		GetWindowRect(hDlg, &rcDlg);
		SetWindowPos(hDlg, HWND_TOP,
			(rcOwner.right - (rcDlg.right - rcDlg.left)) / 2,
			(rcOwner.bottom - (rcDlg.bottom - rcDlg.top)) / 2,
			0, 0, SWP_NOSIZE);
		return TRUE;
	}

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK_CONFIG: {
			wchar_t host[256], port[6];
			GetDlgItemTextW(hDlg, IDC_HOST_EDIT, host, 256);
			GetDlgItemTextW(hDlg, IDC_PORT_EDIT, port, 6);

			if (wcslen(host) > 0 && wcslen(port) > 0) {
				m_currentHost = host;
				m_currentPort = port;
				SaveConfigToFile();

				// Show success message
				std::wstring message = TEXT("Configuration saved!\n\nServer: ") + m_currentHost + TEXT(":") + m_currentPort;
				MessageBox(hDlg, message.c_str(), TEXT("Success"), MB_OK | MB_ICONINFORMATION);
			}
			EndDialog(hDlg, IDOK_CONFIG);
			PostQuitMessage(0); // Exit after saving
			return TRUE;
		}

		case IDCANCEL_CONFIG:
			EndDialog(hDlg, IDCANCEL_CONFIG);
			PostQuitMessage(0); // Exit on cancel
			return TRUE;
		}
		break;
	}
	return FALSE;
}

void ConfigManager::SaveConfigToFile() {
	wchar_t path[MAX_PATH];
	if (GetModuleFileNameW(nullptr, path, MAX_PATH)) {
		std::wstring exePath(path);
		size_t lastSlash = exePath.find_last_of(L"\\/");

		if (lastSlash != std::wstring::npos) {
			std::wstring configPath = exePath.substr(0, lastSlash + 1) + L"config.ini";
			std::wofstream file(configPath);
			if (file.is_open()) {
				file << L"; Game Server Configuration\n";
				file << L"host=" << m_currentHost << L"\n";
				file << L"port=" << m_currentPort << L"\n";
				file.close();
			}
		}
	}
}

void ConfigManager::LoadConfigFromFile() {
	wchar_t path[MAX_PATH];
	if (GetModuleFileNameW(nullptr, path, MAX_PATH)) {
		std::wstring exePath(path);
		size_t lastSlash = exePath.find_last_of(L"\\/");

		if (lastSlash != std::wstring::npos) {
			std::wstring configPath = exePath.substr(0, lastSlash + 1) + L"config.ini";
			std::wifstream file(configPath);
			if (file.is_open()) {
				std::wstring line;
				while (std::getline(file, line)) {
					if (line.empty() || line[0] == L';' || line[0] == L'#') continue;
					size_t pos = line.find(L'=');
					if (pos != std::wstring::npos) {
						std::wstring key = line.substr(0, pos);
						std::wstring value = line.substr(pos + 1);

						// Trim whitespace
						key.erase(0, key.find_first_not_of(L" \t"));
						key.erase(key.find_last_not_of(L" \t") + 1);
						value.erase(0, value.find_first_not_of(L" \t"));
						value.erase(value.find_last_not_of(L" \t") + 1);

						if (key == L"host") {
							m_currentHost = value;
						}
						else if (key == L"port") {
							m_currentPort = value;
						}
					}
				}
				file.close();
			}
		}
	}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	// Initialize common controls
	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&icex);

	g_configManager = new ConfigManager();

	if (!g_configManager->Initialize(hInstance)) {
		MessageBox(nullptr, TEXT("Failed to initialize configuration manager!"), TEXT("Error"), MB_OK | MB_ICONERROR);
		delete g_configManager;
		return 1;
	}

	// Run the application
	g_configManager->Run();

	delete g_configManager;
	return 0;
}
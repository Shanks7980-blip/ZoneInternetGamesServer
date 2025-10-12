#include <windows.h>
#include "installer.h"

#pragma comment(linker, "/SUBSYSTEM:WINDOWS")

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	// Initialize COM for shell operations
	CoInitialize(NULL);

	Installer installer(hInstance);
	int result = installer.Run();

	CoUninitialize();
	return result;
}
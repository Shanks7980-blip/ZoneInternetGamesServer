#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

#include "Breakpoints.hpp"
#include "Config.hpp"
#include "Functions.hpp"

// Credit to https://www.codereversing.com/archives/138

// Disable console in Release build for better performance
#ifdef _DEBUG
#define DEBUG_CONSOLE 1
#else
#define DEBUG_CONSOLE 0
#endif

static void* exceptionHandler = nullptr;

/** Setup functions */
void SetUpDisableTLS();

/* Entry point for the injected DLL */
BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);

#if DEBUG_CONSOLE
		// Show console only in debug mode
		if (AllocConsole())
		{
			freopen("CONOUT$", "w", stdout);
			freopen("CONOUT$", "w", stderr);
			SetConsoleTitle(L"Debug Console");
			SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
			printf("DLL loaded successfully!\n");
		}
#endif

		// Load configuration from INI file
		Config::LoadConfig();

		if (!GetFunctions())
		{
#if DEBUG_CONSOLE
			printf("Couldn't get pointers to all required functions!\n");
#endif
			return FALSE;
		}

		exceptionHandler = AddVectoredExceptionHandler(TRUE, VectoredExceptionHandler);
		if (!exceptionHandler)
		{
#if DEBUG_CONSOLE
			printf("Couldn't create vectored exception handler!\n");
#endif
			return FALSE;
		}

		if (!SetBreakpoints())
		{
#if DEBUG_CONSOLE
			printf("Couldn't set required breakpoints!\n");
#endif
			return FALSE;
		}

#if DEBUG_CONSOLE
		printf("Breakpoints set successfully!\n");
#endif

#ifndef XP_GAMES
		SetUpDisableTLS();
#endif
		break;
	}
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		RemoveVectoredExceptionHandler(exceptionHandler);
#if DEBUG_CONSOLE
		printf("DLL unloaded successfully!\n");
#endif
		break;
	}
	return TRUE;
}

/** Setup functions */
#ifndef XP_GAMES
void SetUpDisableTLS()
{
	HKEY regZoneCom = NULL;
	HKEY regZgmprxy = NULL;

	// Set the "DisableTLS" registry value under "HKEY_CURRENT_USER\Software\Microsoft\zone.com\Zgmprxy" to 1
	if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\zone.com", 0, KEY_WRITE, &regZoneCom) != ERROR_SUCCESS)
	{
#if DEBUG_CONSOLE
		printf("Couldn't open \"zone.com\" registry key.\n");
#endif
		return;
	}

	// By default, the "Zgmprxy" key doesn't exist, so create it, if necessary
	if (RegCreateKeyEx(regZoneCom, L"Zgmprxy", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &regZgmprxy, NULL) != ERROR_SUCCESS)
	{
#if DEBUG_CONSOLE
		printf("Couldn't create/open \"zone.com\\Zgmprxy\" registry key.\n");
#endif
		RegCloseKey(regZoneCom);
		return;
	}

	const DWORD regDisableTLSValue = 1;
	if (RegSetValueEx(regZgmprxy, L"DisableTLS", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&regDisableTLSValue), sizeof(regDisableTLSValue)) != ERROR_SUCCESS)
	{
#if DEBUG_CONSOLE
		printf("Couldn't set \"DisableTLS\" DWORD 32-bit value to 1.\n");
#endif
	}
#if DEBUG_CONSOLE
	else
	{
		printf("Successfully set up DisableTLS registry value\n");
	}
#endif

	// Close opened registry keys
	if (regZoneCom)
		RegCloseKey(regZoneCom);
	if (regZgmprxy)
		RegCloseKey(regZgmprxy);
}
#endif
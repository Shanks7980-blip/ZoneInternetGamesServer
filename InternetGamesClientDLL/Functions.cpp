#include "Functions.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

/* Function pointers */
#ifdef XP_GAMES
void* inet_addr = nullptr;
void* htons = nullptr;
#else
void* GetAddrInfoW = nullptr;
#endif

// Credit to https://www.codereversing.com/archives/138
static FARPROC WINAPI GetExport(const HMODULE hModule, const char* name)
{
	const FARPROC procAddress = GetProcAddress(hModule, name);
	if (!procAddress)
	{
		printf("Couldn't get address of \"%s\": %X\n", name, GetLastError());
		return NULL;
	}
	return procAddress;
}

bool GetFunctions()
{
	const HMODULE ws2_32Dll = GetModuleHandle(L"ws2_32.dll");
	if (ws2_32Dll == NULL)
	{
		printf("Couldn't get handle to \"ws2_32.dll\": %X\n", GetLastError());
		return false;
	}

#ifdef XP_GAMES
	inet_addr = GetExport(ws2_32Dll, "inet_addr");
	if (!inet_addr)
	{
		printf("Failed to get inet_addr function pointer\n");
		return false;
	}

	htons = GetExport(ws2_32Dll, "htons");
	if (!htons)
	{
		printf("Failed to get htons function pointer\n");
		return false;
	}
#else
	GetAddrInfoW = GetExport(ws2_32Dll, "GetAddrInfoW");
	if (!GetAddrInfoW)
	{
		printf("Failed to get GetAddrInfoW function pointer\n");
		return false;
	}
#endif

	return true;
}
#define _CRT_SECURE_NO_WARNINGS

#include "Breakpoints.hpp"
#include "Config.hpp"
#include <array>
#include <stdio.h>
#include <ws2def.h>

#include "Functions.hpp"

// Credit to https://www.codereversing.com/archives/138

/* Setting breakpoints on functions */
#ifdef _DEBUG
#define DEBUG_PRINTF printf
#else
#define DEBUG_PRINTF(...) // Disabled in release
#endif

/* Setting breakpoints on functions */
bool AddBreakpoint(void* address)
{
	MEMORY_BASIC_INFORMATION memInfo = { 0 };
	if (!VirtualQuery(address, &memInfo, sizeof(MEMORY_BASIC_INFORMATION)))
	{
		DEBUG_PRINTF("VirtualQuery failed on %p: %X\n", address, GetLastError());
		return false;
	}

	DWORD oldProtections = 0;
	if (!VirtualProtect(address, sizeof(DWORD_PTR), memInfo.Protect | PAGE_GUARD, &oldProtections))
	{
		DEBUG_PRINTF("VirtualProtect failed on %p: %X\n", address, GetLastError());
		return false;
	}

	DEBUG_PRINTF("Breakpoint set at: %p\n", address);
	return true;
}

bool SetBreakpoints()
{
	bool success = true;
#ifdef XP_GAMES
	DEBUG_PRINTF("Setting breakpoints for XP games (inet_addr, htons)\n");
	success &= AddBreakpoint(inet_addr);
	success &= AddBreakpoint(htons);
#else
	DEBUG_PRINTF("Setting breakpoint for GetAddrInfoW\n");
	success &= AddBreakpoint(GetAddrInfoW);
#endif

	return success;
}

// Helper function for getting function parameters from CONTEXT (x86-64) or WOW64_CONTEXT (x86-32).
template<size_t count>
#if _WIN64
std::array<DWORD64*, count> GetParamsFromContext(CONTEXT* c)
{
	std::array<DWORD64*, count> params = {
		&c->Rcx,
		&c->Rdx,
		&c->R8,
		&c->R9
	};

	DWORD64* currParam = reinterpret_cast<DWORD64*>(c->Rsp) + 4;
	for (size_t i = 4; i < count; ++i)
		params[i] = ++currParam;

	return params;
}

template<>
std::array<DWORD64*, 1> GetParamsFromContext<1>(CONTEXT* c)
{
	return {
		&c->Rcx
	};
}
template<>
std::array<DWORD64*, 2> GetParamsFromContext<2>(CONTEXT* c)
{
	return {
		&c->Rcx,
		&c->Rdx
	};
}
template<>
std::array<DWORD64*, 3> GetParamsFromContext<3>(CONTEXT* c)
{
	return {
		&c->Rcx,
		&c->Rdx,
		&c->R8,
	};
}
template<>
std::array<DWORD64*, 4> GetParamsFromContext<4>(CONTEXT* c)
{
	return {
		&c->Rcx,
		&c->Rdx,
		&c->R8,
		&c->R9
	};
}
#else
std::array<DWORD*, count> GetParamsFromContext(PCONTEXT c)
{
	std::array<DWORD*, count> params;

	DWORD* currParam = reinterpret_cast<DWORD*>(c->Esp);
	for (size_t i = 0; i < count; ++i)
		params[i] = ++currParam;

	return params;
}
#endif

/* Exception handler */
LONG CALLBACK VectoredExceptionHandler(EXCEPTION_POINTERS* exceptionInfo)
{
	if (exceptionInfo->ExceptionRecord->ExceptionCode == STATUS_GUARD_PAGE_VIOLATION)
	{
#ifdef _DEBUG
		DEBUG_PRINTF("Guard page violation detected at address: %p\n", exceptionInfo->ExceptionRecord->ExceptionAddress);
#endif
		exceptionInfo->ContextRecord->EFlags |= 0x100;

		const DWORD_PTR exceptionAddress = reinterpret_cast<DWORD_PTR>(exceptionInfo->ExceptionRecord->ExceptionAddress);

		PCONTEXT context = exceptionInfo->ContextRecord;

		// Perform actions, based on the function
#ifdef XP_GAMES
		if (exceptionAddress == reinterpret_cast<DWORD_PTR>(inet_addr))
		{
#ifdef _DEBUG
			DEBUG_PRINTF("Intercepted inet_addr call!\n");
#endif
			const auto params = GetParamsFromContext<1>(context);

			// Set custom address from config
			strcpy(*reinterpret_cast<CHAR**>(params[0]), Config::host.c_str());

#ifdef _DEBUG
			DEBUG_PRINTF("inet_addr interception completed\n");
#endif
		}
		else if (exceptionAddress == reinterpret_cast<DWORD_PTR>(htons))
		{
#ifdef _DEBUG
			DEBUG_PRINTF("Intercepted htons call!\n");
#endif
			const auto params = GetParamsFromContext<1>(context);

			// Set custom port from config
			*reinterpret_cast<UINT16*>(params[0]) = static_cast<UINT16>(std::stoi(Config::port));

#ifdef _DEBUG
			DEBUG_PRINTF("htons interception completed\n");
#endif
		}
#else
		if (exceptionAddress == reinterpret_cast<DWORD_PTR>(GetAddrInfoW))
		{
#ifdef _DEBUG
			DEBUG_PRINTF("Intercepted GetAddrInfoW call!\n");
#endif
			const auto params = GetParamsFromContext<3>(context);

			// Check if it's trying to connect to the default game server
			PCWSTR originalNodeName = *reinterpret_cast<PCWSTR*>(params[0]);
			if (originalNodeName && !wcscmp(originalNodeName, L"mpmatch.games.msn.com"))
			{
				// Set custom address and port from config
				wcscpy(*reinterpret_cast<WCHAR**>(params[0]), Config::host.c_str());
				wcscpy(*reinterpret_cast<WCHAR**>(params[1]), Config::port.c_str());

				// Set properties, required for connecting to the custom server
				ADDRINFOW* addrInfo = *reinterpret_cast<ADDRINFOW**>(params[2]);
				addrInfo->ai_family = AF_INET;
				addrInfo->ai_protocol = IPPROTO_TCP;

#ifdef _DEBUG
				DEBUG_PRINTF("GetAddrInfoW interception completed\n");
#endif
			}
		}
#endif

		return EXCEPTION_CONTINUE_EXECUTION;
	}

	if (exceptionInfo->ExceptionRecord->ExceptionCode == STATUS_SINGLE_STEP)
	{
#ifdef _DEBUG
		DEBUG_PRINTF("Single step exception, resetting breakpoints\n");
#endif
		SetBreakpoints();
		return EXCEPTION_CONTINUE_EXECUTION;
	}
	return EXCEPTION_CONTINUE_SEARCH;
}
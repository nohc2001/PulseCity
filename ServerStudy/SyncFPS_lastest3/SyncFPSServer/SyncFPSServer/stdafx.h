// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "NWLib/targetver.h"

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers


#ifdef _WIN32
#include <Ws2tcpip.h>
#include <winsock2.h>
#include <tchar.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

#include <assert.h>
#include <exception>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <cstdint>

#include <fstream>
#include <chrono>
#include <thread>
#include <vector>
#include <unordered_map>

#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>

#include "NWLib/CustomNWLib.h"
#include "vecset.h"
#include "SpaceMath.h"

using namespace std;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma region dbglogDefines
#define dbglog1(format, A) \
	{\
		wchar_t str[128] = {}; \
		swprintf_s(str, 128, format, A); \
		OutputDebugString(str);\
	}
#define dbglog2(format, A, B) \
	{\
		wchar_t str[128] = {}; \
		swprintf_s(str, 128, format, A, B); \
		OutputDebugString(str);\
	}
#define dbglog3(format, A, B, C) \
	{\
		wchar_t str[128] = {}; \
		swprintf_s(str, 128, format, A, B, C); \
		OutputDebugString(str);\
	}
#define dbglog4(format, A, B, C, D) \
	{\
		wchar_t str[128] = {}; \
		swprintf_s(str, 128, format, A, B, C, D); \
		OutputDebugString(str);\
	}
#pragma endregion

typedef unsigned long long ui64;
#define QUERYPERFORMANCE_HZ 10000000.0//Hz
static inline ui64 GetTicks()
{
	LARGE_INTEGER ticks;
	QueryPerformanceCounter(&ticks);
	return ticks.QuadPart;
}
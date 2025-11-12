#pragma once

#include <SDKDDKVer.h>

#include <Ws2tcpip.h>
#include <assert.h>
#include <exception>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <tchar.h>
#include <winsock2.h>
#include <cstdint>
#include <fstream>

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include <DXGIDebug.h>
#include <vector>
#include <string>
#include <unordered_map>

#include "NWLib/CustomNWLib.h"

#include "vecset.h"
#include "SpaceMath.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
//using Microsoft::WRL::ComPtr; -> question 001

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

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

#define QUERYPERFORMANCE_HZ 10000000//Hz
static inline ui64 GetTicks()
{
	LARGE_INTEGER ticks;
	QueryPerformanceCounter(&ticks);
	return ticks.QuadPart;
}
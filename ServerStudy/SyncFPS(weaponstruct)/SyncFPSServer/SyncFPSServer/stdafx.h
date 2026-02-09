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

/*
* 설명 : 출력창에 로드를 찍기 위한 매크로 함수
*/
#pragma region dbglogDefines
#define dbglog1(format, A) \
	{\
		wchar_t str[512] = {}; \
		wchar_t format0[128] = L"%s line %d : "; \
		wcscpy_s(format0+13, 128-13, format);\
		swprintf_s(str, 512, format0, _T(__FUNCTION__), __LINE__, A); \
		OutputDebugString(str);\
	}
#define dbglog2(format, A, B) \
	{\
		wchar_t str[512] = {}; \
		wchar_t format0[128] = L"%s line %d : "; \
		wcscpy_s(format0+13, 128-13, format);\
		swprintf_s(str, 512, format0, _T(__FUNCTION__), __LINE__, A, B); \
		OutputDebugString(str);\
	}
#define dbglog3(format, A, B, C) \
	{\
		wchar_t str[512] = {}; \
		wchar_t format0[128] = L"%s line %d : "; \
		wcscpy_s(format0+13, 128-13, format);\
		swprintf_s(str, 512, format0, _T(__FUNCTION__), __LINE__, A, B, C); \
		OutputDebugString(str);\
	}
#define dbglog4(format, A, B, C, D) \
	{\
		wchar_t str[512] = {}; \
		wchar_t format0[128] = L"%s line %d : "; \
		wcscpy_s(format0+13, 128-13, format);\
		swprintf_s(str, 512, format0, _T(__FUNCTION__), __LINE__, A, B, C, D); \
		OutputDebugString(str);\
	}

#define dbglog1a(format, A) \
	{\
		char str[512] = {}; \
		char format0[128] = "%s line %d : "; \
		strcpy_s(format0+13, 128-13, format);\
		sprintf_s(str, 512, format0, __FUNCTION__, __LINE__, A); \
		OutputDebugStringA(str);\
	}
#define dbglog2a(format, A, B) \
	{\
		char str[512] = {}; \
		char format0[128] = "%s line %d : "; \
		strcpy_s(format0+13, 128-13, format);\
		sprintf_s(str, 512, format0, __FUNCTION__, __LINE__, A, B); \
		OutputDebugStringA(str);\
	}
#define dbglog3a(format, A, B, C) \
	{\
		char str[512] = {}; \
		char format0[128] = "%s line %d : "; \
		strcpy_s(format0+13, 128-13, format);\
		sprintf_s(str, 512, format0, __FUNCTION__, __LINE__, A, B, C); \
		OutputDebugStringA(str);\
	}
#define dbglog4a(format, A, B, C, D) \
	{\
		char str[512] = {}; \
		char format0[128] = "%s line %d : "; \
		strcpy_s(format0+13, 128-13, format);\
		sprintf_s(str, 512, format0, __FUNCTION__, __LINE__, A, B, C, D); \
		OutputDebugStringA(str);\
	}
#pragma endregion

typedef unsigned long long ui64;
#define QUERYPERFORMANCE_HZ 10000000.0//Hz
/*
* 설명 : 현재의 QueryPerformanceCounter를 반환함.
* 이것을 통해 시간을 잴 수 있다.
*/
static inline ui64 GetTicks()
{
	LARGE_INTEGER ticks;
	QueryPerformanceCounter(&ticks);
	return ticks.QuadPart;
}


/*
* 설명 : BitBoolArr의 bool 비트 하나를 참조하는 자료구조.
* data 주소에 있는 64비트 중, bitindex 번째 비트를 가리킨다.
* 
* Sentinal Value :
* NULL = (data == nullptr)
*/
struct BitBoolArr_ui64 {
	ui64* data;
	int bitindex;

	operator bool() {
		return *data & ((ui64)1 << (ui64)bitindex);
	}

	/*
	* 설명 : 해당 비트에 bool 값을 넣는 함수
	* 매개변수 : 
	* bool b : 넣을 bool 값
	*/
	void operator=(bool b) {
		ui64 n = (ui64)1 << (ui64)bitindex;
		if (b) {
			*data |= n;
		}
		else {
			n = ~n;
			*data &= n;
		}
	}
};

/*
* 설명 : bool 값 하나가 1 비트인 bool 배열.
*/
template <int n> struct BitBoolArr {
	ui64 buffer[n] = {};

	/*
	* 설명 : BitBoolArr의 index 번째 불 비트를 참조하는 자료구조를 반환
	* 매개변수 : 
	* int index : 불 비트의 인덱스
	*/
	BitBoolArr_ui64& operator[](int index) {
		BitBoolArr_ui64 d;
		d.data = &buffer[index >> 6];
		d.bitindex = index & 63;
		return d;
	}
};


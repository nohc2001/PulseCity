#pragma once

//#define PIX_DEBUGING

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
#include <iomanip>

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include <DXGIDebug.h>
#include "d3dx12.h"
#include "DDSTextureLoader12.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <random>

#include <dxcapi.h>
#include <d3d12sdklayers.h>
#include <wrl.h>

#include "NWLib/CustomNWLib.h"
#include "Utill_ImageFormating.h"

#include "vecset.h"
#include "SpaceMath.h"

#include "D3D_Util/DXSampleHelper.h"
#include "D3D_Util/ShaderUtil.h"
#include "D3D_Util/DirectXRaytracingHelper.h"

typedef unsigned char byte8;
typedef unsigned short ui16;
typedef unsigned int ui32;

using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace std;
//using Microsoft::WRL::ComPtr; -> question 001

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

void dbgbreak(bool condition);
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
		swprintf_s(str, 512, format, _T(__FUNCTION__), __LINE__, A, B, C, D); \
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
		sprintf_s(str, 512, format, __FUNCTION__, __LINE__, A, B, C, D); \
		OutputDebugStringA(str);\
	}
#pragma endregion

#define QUERYPERFORMANCE_HZ 10000000//Hz
static inline ui64 GetTicks()
{
	LARGE_INTEGER ticks;
	QueryPerformanceCounter(&ticks);
	return ticks.QuadPart;
}

/*
* 설명 : 
* GPU에 올렸거나 올릴 리소스의 디스크립터 핸들을 저장한다.
* Sentinel Value
* NULL = (hcpu == 0 && hgpu == 0)
*/
struct DescHandle {
	D3D12_CPU_DESCRIPTOR_HANDLE hcpu;
	D3D12_GPU_DESCRIPTOR_HANDLE hgpu;

	DescHandle(D3D12_CPU_DESCRIPTOR_HANDLE _hcpu, D3D12_GPU_DESCRIPTOR_HANDLE _hgpu) {
		hcpu = _hcpu;
		hgpu = _hgpu;
	}
	DescHandle() :
		hcpu{ 0 }, hgpu{ 0 }
	{
	}

	__forceinline void operator+=(unsigned long long inc) {
		hcpu.ptr += inc;
		hgpu.ptr += inc;
	}

	template<D3D12_DESCRIPTOR_HEAP_TYPE type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV>
	__forceinline DescHandle operator[](UINT index);
};

struct OBB_vertexVector {
	vec4 vertex[2][2][2] = { {{}} };
};

struct DescIndex {
	bool isShaderVisible = false;
	char type = 0; // 'n' - UAV, SRV, CBV / 'r' - RTV / 'd' - DSV
	ui32 index;
	DescIndex() {

	}

	DescIndex(bool isSV, ui32 i, char t = 'n') : isShaderVisible{ isSV }, index{ i }, type{ t } {

	}

	void Set(bool isSV, ui32 i, char t = 'n') {
		isShaderVisible = isSV;
		index = i;
		type = t;
	}
	__forceinline DescHandle GetCreationDescHandle() const;
	__forceinline DescHandle GetRenderDescHandle() const;
	__declspec(property(get = GetCreationDescHandle)) const DescHandle hCreation;
	__declspec(property(get = GetRenderDescHandle)) const DescHandle hRender;
};

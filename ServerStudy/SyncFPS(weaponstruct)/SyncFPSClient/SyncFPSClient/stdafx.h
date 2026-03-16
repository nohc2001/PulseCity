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

#pragma comment(lib, "ws2_32.lib") // 64비트 버전이라 한다. (16 - 16비트 32 -> 32이상.)
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

#pragma region STCSyncCode
struct STCMemberInfo {
	const char* name;
	unsigned int offset;
	unsigned int size;
};

template <typename T, typename M> constexpr unsigned int offset_of(M T::* member) { return reinterpret_cast<size_t>(&(((T*)0)->*member)); }

#define STC_STATICINIT_innerStruct inline static vector<STCMemberInfo> g_members; \
    struct OffsetRegister { \
        OffsetRegister(const char* name, unsigned int offset, unsigned int size) { \
            STC_CurrentStruct::g_members.push_back({name, offset, size}); \
        } \
    };
#define STC_STATICINIT_outerStruct(MyStruct) vector<STCMemberInfo> MyStruct::g_members;

#define STC_CurrentStruct int
#define STCDef(type, member) \
    type member; \
    inline static OffsetRegister reg_##member{#member, offset_of(&STC_CurrentStruct::member), sizeof(type)};

#define STCDefArr(type, member, Count) \
    type member[Count]; \
    inline static OffsetRegister reg_##member{#member, offset_of(&STC_CurrentStruct::member), sizeof(type) * Count};

#define STCDefStdArr(type, member, Count) \
    array<type, Count> member; \
    inline static OffsetRegister reg_##member{#member, offset_of(&STC_CurrentStruct::member), sizeof(array<type, Count>)};
#pragma endregion

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



struct GameObject;
struct SyncWay {
	int clientOffset;
	void (*syncfunc)(GameObject*, char*, int);
	SyncWay() {}
	SyncWay(int n) :
		clientOffset{ n }, syncfunc{ nullptr }
	{

	}
	SyncWay(void(*func)(GameObject*, char*, int)) :
		clientOffset{ -1 }, syncfunc{ func }
	{

	}
};

// virtual function pointer table <-> GameObjectType
// pair <GameObjectType, offset> <-> Client Offset
union GameObjectType {
	static constexpr int ObjectTypeCount = 6;

	short id;
	enum {
		_GameObject = 0,
		_StaticGameObject = 1,
		_DynamicGameObject = 2,
		_SkinMeshGameObject = 3,
		_Player = 4,
		_Monster = 5,
	};

	operator short() { return id; }

	static void* vptr[ObjectTypeCount];
	static vector<STCMemberInfo> Server_STCMembers[ObjectTypeCount];
	static vector<STCMemberInfo> Client_STCMembers[ObjectTypeCount];
	static unordered_map<int, SyncWay> STC_OffsetMap[ObjectTypeCount];

	// 싱크되는 변수의 서버이름과 클라이언트 이름이 다른 경우 연결을 위해 사용.
	static void LinkOffsetByName(short type, const char* ServerVarName, const char* ClientVarName);
	static void LinkOffsetAsFunction(short type, const char* ServerVarName, void (*func)(GameObject*, char*, int));
	static void STATICINIT();
};
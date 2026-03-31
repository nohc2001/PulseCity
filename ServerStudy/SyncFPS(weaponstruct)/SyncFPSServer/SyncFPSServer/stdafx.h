// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//
#pragma once

#define ChunckDEBUG

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

#include "vecset.h"
#include "SpaceMath.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;
using namespace DirectX;
using namespace DirectX::PackedVector;

struct NWAddr {
	char IPString[16] = {};
	sockaddr_in addr;

	string ToString()
	{
		char finalString[1000];
		sprintf(finalString, "%s:%d", IPString, htons(addr.sin_port));
		return finalString;
	}
};

struct Server {
	SOCKET listen_sock;
	static constexpr int rbufcapacity = 8192;
	char rbuf[rbufcapacity] = {};
	NWAddr addr;

	void Init(const char* ServerIP, unsigned short ServerPort) {
		listen_sock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

		// Set Addr
		struct sockaddr_in serveraddr;
		memset(&serveraddr, 0, sizeof(serveraddr));
		serveraddr.sin_family = AF_INET;
		inet_pton(AF_INET, ServerIP, &serveraddr.sin_addr);
		serveraddr.sin_port = htons(ServerPort);
		addr.addr = serveraddr;
		strcpy_s(addr.IPString, ServerIP);

		//bind
		if (bind(listen_sock, (sockaddr*)&serveraddr, sizeof(sockaddr)) < 0)
		{
			stringstream ss;
			ss << "bind failed:" << WSAGetLastError();
			throw ss.str().c_str();
		}

		// set non blocking
		u_long val = 1;
		int ret = ioctlsocket(listen_sock, FIONBIO, &val);
		if (ret != 0)
		{
			stringstream ss;
			ss << "bind failed:" << WSAGetLastError();
			throw ss.str().c_str();
		}
	}

	void Listen() {
		constexpr int backlog = 2000;
		listen(listen_sock, backlog);
	}

	int Accept(SOCKET& sock, string& error) {
		sock = accept(listen_sock, NULL, 0);
		if (sock == INVALID_SOCKET)
		{
			error = WSAGetLastError();
			return -1;
		}
		else
			return 0;
	}
};

#pragma region STCSyncCode
#define STC_CurrentStruct int
#define STC_STATICINIT_innerStruct struct MemberInfo; \
    static inline vector<MemberInfo> g_member; \
    struct MemberInfo { \
        const char* name; \
        unsigned int(*get_offset)(); \
        unsigned int offset; \
        unsigned int size; \
        MemberInfo(const char* n, unsigned int(*get_off)(), unsigned int siz) { \
            name = n; \
            get_offset = get_off; \
            size = siz; \
            g_member.push_back(*this); \
        } \
    };

#define STCDef(Type, Name) Type Name;\
    static unsigned int _offset_fn_##Name() {\
        STC_CurrentStruct obj{};\
        char* base = reinterpret_cast<char*>(&obj);\
        char* mem  = reinterpret_cast<char*>(&obj.Name);\
        return (mem - base);\
    }\
    inline static MemberInfo _reg_##Name{ #Name, _offset_fn_##Name, sizeof(Type)};

#define STCDefArr(Type, Name, Count) Type Name[Count];\
    static unsigned int _offset_fn_##Name() {\
        STC_CurrentStruct obj{};\
        char* base = reinterpret_cast<char*>(&obj);\
        char* mem  = reinterpret_cast<char*>(&obj.Name);\
        return (mem - base);\
    }\
    inline static MemberInfo _reg_##Name{ #Name, _offset_fn_##Name, sizeof(Type) * Count};
#pragma endregion


void dbgbreak(bool condition);
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

/*
* 설명 : 어떤 서버의 클래스에서 해당 클래스의 어떤 맴버변수가 어떤 오프셋을 가지는지에 대한 정보
*/
struct type_offset_pair {
	//클래스의 타입
	short type;
	//서버 GameObject내 어떤 맴버변수의 offset
	short offset;

	type_offset_pair(short t, short o) :
		type{ t }, offset{ o }
	{

	}

	type_offset_pair(const type_offset_pair& ref) {
		type = ref.type;
		offset = ref.offset;
	}

	bool operator==(const type_offset_pair& ref) const {
		return type == ref.type && offset == ref.offset;
	}
};

/*
* 설명 : type_offset_pair의 hash.
* offset의 제일 하위 3바이트와 그다음 하위 3바이트를 뒤바꾼후,
* type과 교차한다.
*/
template<>
class hash<type_offset_pair> {
public:
	size_t operator()(const type_offset_pair& s) const {
		ui32 offsethash = s.offset;
		offsethash = (offsethash & 177700) | ((offsethash & 7) << 3) | ((offsethash & 38) >> 3);

		//temp 2026.1.3 <아마 의도와는 다른 코드가 아닐까 생각중.>
		//offsethash = (offsethash & 177600) | ((offsethash & 7) << 3) | ((offsethash & 70) >> 3);

		ui32 typehash = s.type;
		offsethash = _pdep_u32(offsethash, 0xAAAAAAAA);
		typehash = _pdep_u32(typehash, 0x55555555);
		return offsethash | typehash;
	}
};

/*
* 설명 : 어떤 타입 T의 가상함수 테이블을 가리키는 vptr을 얻는다.
*/
template <typename T> void* GetVptr() {
	T a;
	return *(void**)&a;
}

/*
* 설명 : 게임오브젝트들의 타입을 가리키는 구조체.
*
*/
// virtual function pointer table <-> GameObjectType
// pair <GameObjectType, offset> <-> Client Offset
union GameObjectType {
	static constexpr int ObjectTypeCount = 3;

	short id = 0;
	enum {
		_GameObject = 0,
		_StaticGameObject = 1,
		_DynamicGameObject = 2,
		_SkinMeshGameObject = 3, 
		_Player = 4,
		_Monster = 5,
		_Portal = 6,
	};

	GameObjectType() {}
	GameObjectType(short s) { id = s; }

	operator short() { return id; }

	// pair <GameObjectType, offset> -> Client Offset
	static unordered_map<type_offset_pair, short, hash<type_offset_pair>> GetClientOffset;

	// virtual function pointer table -> GameObjectType
	static unordered_map<void*, GameObjectType> VptrToTypeTable;

	/*
	* 설명 : 전체 클래스의 초기화.클래스 사용전 반드시 호출해야함.
	*/
	static void STATICINIT();
};

/*
* 설명 : GetClientOffset Map에 ClientOffset 정보를 추가한다.
* 매개변수 :
* a : 게임오브젝트 타입
* b : 서버 오프셋
* c : 클라이언트 오프셋
*/
#define AddClientOffset(a, b, c) GameObjectType::GetClientOffset.insert(std::pair<type_offset_pair, short>(type_offset_pair( \
	a, b), c));

/*
* 설명 : Item의 데이터
*/
struct Item
{
	// server, client
	int id;
	// only client
	//vec4 color;
	//Mesh* MeshInInventory;

	Item(int i) : id{ i } {}
};

typedef int ItemID;

/*
* 설명 : 인벤토리 칸에 들어갈 데이터.
* Sentinal Value :
* NULL : (ItemCount == 0)
*/
struct ItemStack {
	ItemID id;
	int ItemCount;
};

/*
* 설명 : 드롭된 아이템 데이터.
* Sentinal Value :
* NULL : ItemStack=NULL
*/
struct ItemLoot {
	ItemStack itemDrop;
	vec4 pos;
};

/*
설명 : <TCP IP에 대한 오해로부터 만들어진 구조체>
때문에 이후 수정이 필요. PACK이라는 프로토콜은 쓸모가 없기 때문.
>> 음 보니까 Send 시스템 호출을 줄이고 서버의 업로드 용량을 줄이는데 도움이 될 것 같다.
여기에서 고쳐야 할건 순서를 나타내는 바이트를 없애는거 뿐인것 같다.
*/
struct SendDataSaver {
	char* buffer;
	int size = 0;
	int capacity = 0;
	int postsiz = 0;
	char* ofbuff = nullptr;

	__forceinline void Init(int _capacity) {
		capacity = _capacity;
		size = 0;
		buffer = new char[capacity];
		postsiz = 0;
	}

	__forceinline void Clear() {
		size = 0;
		postsiz = 0;
	}

	__forceinline void push_senddata(char* data, int len) {
	push_start:
		// 대부분의 상황에서는 false일태니 branch prediction을 기대해본다.
		if (size + len > capacity) {
			char* newbuffer = new char[capacity * 2];
			memcpy(newbuffer, buffer, size);
			delete[] buffer;
			buffer = newbuffer;
			capacity = capacity * 2;
			goto push_start;
		}
		memcpy(buffer + size, data, len);
		size += len;
	}

	__forceinline void postpush_start() {
		postsiz = size;
		ofbuff = buffer + postsiz;
	}

	__forceinline void postpush_reserve(int add_len) {
		if (postsiz + add_len > capacity) {
			char* newbuffer = new char[capacity * 2];
			memcpy(newbuffer, buffer, postsiz);
			delete[] buffer;
			buffer = newbuffer;
			capacity = capacity * 2;
			ofbuff = buffer + postsiz;
		}
		postsiz += add_len;
	}

	template <int offset, int len>
	__forceinline void postpush_senddata(void* data) {
		if constexpr (len == 1) {
			*(ofbuff + offset) = *(char*)data;
		}
		else if constexpr (len == 2) {
			*(ui16*)(ofbuff + offset) = *(ui16*)data;
		}
		else if constexpr (len == 4) {
			*(ui32*)(ofbuff + offset) = *(ui32*)data;
		}
		else if constexpr (len == 8) {
			*(ui64*)(ofbuff + offset) = *(ui64*)data;
		}
		else if constexpr (len == 16) {
			*(__m128i*)(ofbuff + offset) = *(__m128i*)data;
		}
		else if constexpr (len == 32) {
			*(__m256i*)(ofbuff + offset) = *(__m256i*)data;
		}
		else {
			memcpy(ofbuff + offset, data, len);
		}
	}

	__forceinline void postpush_memcpy(int offset, int len, void* data) {
		memcpy(ofbuff + offset, data, len);
	}

	__forceinline void postpush_end() {
		int len = postsiz - size;
		*(int*)(((char*)buffer) + size) = len;
		size = postsiz;
	}

	__forceinline void push_senddata(SendDataSaver& sendData) {
		push_senddata(sendData.buffer, sendData.size);
	}

	__forceinline DWORD send(SOCKET sock, DWORD flag) {
		WSABUF buf;
		buf.buf = buffer;
		buf.len = size;
		DWORD retval = 0;
		WSASend(sock, &buf, 1, &retval, flag, NULL, NULL);
		return retval;
	}
};

/*
설명 : Client의 입력을 구분하는 enum
*/
enum InputID {
	KeyboardW = 'W',
	KeyboardA = 'A',
	KeyboardS = 'S',
	KeyboardD = 'D',
	KeyboardQ = 'Q',
	Keyboard1 = '1',
	Keyboard2 = '2',
	Keyboard3 = '3',
	Keyboard4 = '4',
	Keyboard5 = '5',
	KeyboardSpace = VK_SPACE,
	MouseLbutton = 5,
	MouseRbutton = 6,
	RotationSync = 7,
};

/*
설명 : Client의 회전 입력 패킷 구조체
*/
#pragma pack(push, 1) 
struct RotationPacket {
	char id;     // InputID::RotationSync (7)
	float yaw;
	float pitch;
};
#pragma pack(pop)

/*
설명 : Server 에서 Client로 보내는 데이터 프로토콜의 타입
Sentinal Value :
NULL = (short)0
*/
union SendingType {
	enum {
		NullType = 0,
		//there is no format

		SyncGameObject = 1,
		// [int size] [int sendttype] [int newobj_index] [type of gameobject] [gameobject raw data]

		ChangeMemberOfGameObject = 2,
		// [int size] [st(2)] [obj index(int) (4)] [type of gameobject(2)] [client member offset(short)] [memberSize (2)] [rawData (member size)]

		NewRay = 3,
		// [int size] [st] [Ray raw data (include distance which determined by raycast)]

		AllocPlayerIndexes = 4,
		// [int size] [st] [client Index] [Object Index]

		DeleteGameObject = 5,
		// [int size] [st] [obj index]

		ItemDrop = 6,
		// [int size] [st] [dropindex] [lootdata]

		ItemDropRemove = 7,
		// [int size] [st] [dropindex]

		InventoryItemSync = 8,
		// [int size] [st] [lootdata] [inventory index]

		PlayerFire = 9,
		// [int size] [st(2)] [obj index(4)]

		SyncGameState = 10,
		// [int size] [st] [int DynamicGameObjectCapacity] [int StaticGameObjectCapacity]
	};

	// enum을 숫자로 나타낸 것.
	short n;
	char two_byte[2];

	SendingType(short id) { n = id; }
	operator short() { return n; }
};

#pragma region ProtocolStruct
#pragma pack(push, 1)
/*
* 설명 : 전체 오브젝트 하나를 통체로 동기화 하고 싶을때 사용된다.
* 각 게임오브젝트들의 맴버변수로 해당 데이터를 만들어 SendDataSaver에 보낼 수 있다.
*/
struct STC_SyncGameObject_Header {
	unsigned int size = 0;
	SendingType st = SendingType::SyncGameObject;
	GameObjectType type;
	int objindex;
};

/*
* 설명 : 게임오브젝트에서 어떤 오브젝트의 어떤 맴버를 변경하고 싶을때 사용된다.
*/
struct STC_ChangeMemberOfGameObject_Header {
	unsigned int size = 0;
	SendingType st = SendingType::ChangeMemberOfGameObject;
	GameObjectType type;
	int objindex;
	// 서버의 오프셋을 사용한다. (클라가 알아서 해석한다.)
	int serveroffset; 
	int datasize;
	// 이 이후로 실제 동기화할 데이터가 붙는다.
};

/*
* 설명 : 그냥 클라이언트에게 어떤 총알 궤적을 그리라 명령하는 것. 
	사실 충돌이 결정나고 보내진다.
*/
struct STC_NewRay_Header {
	unsigned int size = 34; // 크기고정
	SendingType st = SendingType::NewRay;
	XMFLOAT3 raystart;
	XMFLOAT3 rayDir;
	float distance;
};

/*
* 설명 : 클라이언트에게 서버에서 자신과 자신의 오브젝트가 어떻게 관리되고 있는지 알려준다.
*/
struct STC_AllocPlayerIndexes_Header {
	unsigned int size = 14; // 크기고정
	SendingType st = SendingType::AllocPlayerIndexes;
	
	// 데이터를 받을 클라이언트가 서버내에서 몇번째 클라이언트인지
	int clientindex;
	// 그 클라이언트가 조작하는 오브젝트가 서버에서 몇번째 Dynamic오브젝트인지.
	int server_obj_index;
};

/*
* 설명 : 특정 오브젝트가 삭제되었다는 사실을 클라이언트에게 보고한다.
*/
struct STC_DeleteGameObject_Header {
	unsigned int size = 10; // 크기고정
	SendingType st = SendingType::DeleteGameObject;
	int obj_index; // 삭제를 진행할 dynamic 오브젝트의 인덱스
};

/*
* 설명 : 아이템이 드롭되었다는 걸 클라이언트에게 알리는 역할.
*/
struct STC_ItemDrop_Header {
	unsigned int size = 48; // 크기고정
	SendingType st = SendingType::ItemDrop;
	int dropindex; // 드롭아이템 인덱스
	ItemLoot lootData; // 루팅된 아이템의 데이터
};

/*
* 설명 : 드롭 아이템이 삭제되었다는걸 클라이언트에게 알리는 역할
*/
struct STC_ItemDropRemove_Header {
	unsigned int size = 10; // 크기고정
	SendingType st = SendingType::ItemDropRemove;
	int dropindex; // 삭제된 드롭아이템의 인덱스
};

/*
* 설명 : 인벤토리의 특정 칸을 동기화 하는 역할
*/
struct STC_InventoryItemSync_Header {
	unsigned int size = 18; // 크기고정
	SendingType st = SendingType::InventoryItemSync;
	// 인벤토리에 들어갈 아이템
	ItemStack Iteminfo;
	// 인벤토리 몇번째 칸인지.
	int inventoryIndex;
};

/*
* 설명 : ???
*/
struct STC_PlayerFire_Header {
	unsigned int size = 10; // 크기고정
	SendingType st = SendingType::PlayerFire;
	int objindex;
};

/*
* 설명 : 전반적인 게임과 관련된 상태들을 공유한다.
*/
struct STC_SyncGameState_Header {
	unsigned int size = 10; // 크기고정
	SendingType st = SendingType::SyncGameState;
	int DynamicGameObjectCapacity;
	int StaticGameObjectCapacity;
	//int mapOBBCount;
};

union CTS_Protocol {
	enum {
		KeyInput = 0,
		SyncRotation = 1
	};
	short n;
	char two_byte[2];

	CTS_Protocol(short id) { n = id; }
	operator short() { return n; }
};

struct CTS_KeyInput_Header {
	unsigned int size = 8; // 크기고정
	CTS_Protocol st = CTS_Protocol::KeyInput;
	char Key;
	bool isdown;
};

struct CTS_SyncRotation_Header {
	unsigned int size = 14;
	CTS_Protocol st = CTS_Protocol::KeyInput;
	float yaw;
	float pitch;
};


#pragma pack(pop)
#pragma endregion
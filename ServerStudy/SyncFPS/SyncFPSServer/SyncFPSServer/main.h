// Including SDKDDKVer.h defines the highest available Windows platform.

// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.

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

#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>

#include "NWLib/CustomNWLib.h"
#include <chrono>
#include <thread>
#include <vector>
#include <unordered_map>

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

enum InputID {
	KeyboardW = 'W',
	KeyboardA = 'A',
	KeyboardS = 'S',
	KeyboardD = 'D',
	KeyboardSpace = VK_SPACE,
	MouseLbutton = 5,
	MouseRbutton = 6,
	MouseMove = 7,
};

union SendingType {
	enum {
		NullType = 0,
		NewGameObject = 1, // [st] [new obj index] [type of gameobject] [gameobject raw data]
		ChangeMemberOfGameObject = 2, 
		// [st(2)] [obj index(int) (4)] [type of gameobject(2)] [client member offset(short)] [memberSize (2)] [rawData (member size)]
		NewRay = 3, // [st] [Ray raw data (include distance which determined by raycast)]
		SetMeshInGameObject = 4, // [st] [obj index] [strlen] [string data]
		AllocPlayerIndexes = 5, // [st] [client Index] [Object Index]
		DeleteGameObject = 6, // [st] [obj index]
	};
	short n;
	char two_byte[2];

	SendingType(short id) { n = id; }
	operator short() { return n; }
};

struct twoPage {
	char data[8196] = {};
};

struct Mesh {
	vec4 MAXpos;

	struct Vertex {
		XMFLOAT3 position;
		Vertex() {}
		Vertex(XMFLOAT3 p)
			: position{ p }
		{
		}
	};

	void ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering = true);
	BoundingOrientedBox GetOBB();
	void CreateWallMesh(float width, float height, float depth);

	static vector<string> MeshNameArr;
	static unordered_map<string, Mesh*> meshmap;
	static int GetMeshIndex(string meshName);
	static int AddMeshName(string meshName);
};

struct GameObject {
	bool isExist = true;
	int MeshIndex;
	matrix m_worldMatrix;
	vec4 LVelocity;
	vec4 tickLVelocity;
	Mesh mesh;

	GameObject();
	virtual ~GameObject();

	virtual void Update(float deltaTime);
	virtual void Event();

	virtual BoundingOrientedBox GetOBB();

	static void CollisionMove(GameObject* gbj1, GameObject* gbj2);
	static void CollisionMove_DivideBaseline(GameObject* movObj, GameObject* colObj, BoundingOrientedBox colOBB);
	static void CollisionMove_DivideBaseline_rest(GameObject* movObj, GameObject* colObj, BoundingOrientedBox colOBB, vec4 preMove);
	virtual void OnCollision(GameObject* other);

	virtual void OnCollisionRayWithBullet();
};

struct BitBoolArr_ui64 {
	ui64* data;
	int bitindex;

	operator bool() {
		return *data & (1 << bitindex);
	}

	void operator=(bool b) {
		ui64 n = 1 << bitindex;
		if (b) {
			*data |= n;
		}
		else {
			n = ~n;
			*data &= n;
		}
	}
};

template <int n> struct BitBoolArr {
	ui64 buffer[n] = {};

	BitBoolArr_ui64& operator[](int index) {
		BitBoolArr_ui64 d;
		d.data = &buffer[index >> 6];
		d.bitindex = index & 63;
		return d;
	}
};

struct Player : public GameObject {
	static constexpr float ShootDelay = 0.5f;
	float ShootFlow = 0;
	static constexpr float recoilVelocity = 20.0f;
	float recoilFlow = 0;
	static constexpr float recoilDelay = 0.3f;
	float HP;
	float MaxHP = 100;
	vec4 DeltaMousePos;

	bool isGround = false;
	bool isMouseReturn;
	int collideCount = 0;
	float JumpVelocity = 5;
	int clientIndex = 0;

	vec4 LastMousePos;
	//bool InputBuffer[8];
	BitBoolArr<2> InputBuffer;

	matrix ViewMatrix;
	bool bFirstPersonVision = true;

	Player() : HP(100.0f) {}

	virtual void Update(float deltaTime) override;

	//virtual void Event(WinEvent evt);

	virtual void OnCollision(GameObject* other) override;

	virtual BoundingOrientedBox GetOBB();

	BoundingOrientedBox GetBottomOBB(const BoundingOrientedBox& obb);

	void TakeDamage(float damage);

	virtual void OnCollisionRayWithBullet();
};

struct Monster : public GameObject {
	vec4 m_homePos;
	vec4 m_targetPos;
	float m_speed = 2.0f;
	float m_patrolRange = 20.0f;
	float m_chaseRange = 10.0f;
	float m_patrolTimer = 0.0f;
	float m_fireDelay = 1.0f;
	float m_fireTimer = 0.0f;
	float HP = 30;
	int collideCount = 0;
	int targetSeekIndex = 0;
	Player** Target = nullptr;
	static constexpr float MAXHP = 30;
	bool m_isMove = false;
	bool isGround = false;
	
	Monster() {}
	virtual ~Monster() {}

	virtual void Update(float deltaTime) override;
	//virtual void Render();
	virtual void OnCollision(GameObject* other) override;

	virtual void OnCollisionRayWithBullet();

	void Init(const XMMATRIX& initialWorldMatrix);

	virtual BoundingOrientedBox GetOBB();
};

//socket memory size is too big -> because recieve buffer..
struct ClientData {
	Socket socket;
	XMVECTOR pos;

	bool isPosUpdate = false;
	bool Connected = false;
	Player* pObjData;
	int objindex = 0;
	static constexpr float MoveSpeed = 3.0f;
};

struct type_offset_pair {
	short type;
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

template<>
class hash<type_offset_pair> {
public:
	size_t operator()(const type_offset_pair& s) const {
		ui32 offsethash = s.offset;
		offsethash = (offsethash & 177600) | ((offsethash & 7) << 3) | ((offsethash & 70) >> 3);
		ui32 typehash = s.type;
		offsethash = _pdep_u32(offsethash, 0xAAAAAAAA);
		typehash = _pdep_u32(typehash, 0x55555555);
		return offsethash | typehash;
	}
};

template <typename T> void* GetVptr() {
	T a;
	return *(void**)&a;
}

// virtual function pointer table <-> GameObjectType
// pair <GameObjectType, offset> <-> Client Offset
union GameObjectType {
	static constexpr int ObjectTypeCount = 3;

	short id;
	enum {
		_GameObject = 0,
		_Player = 1,
		_Monster = 2,
	};

	GameObjectType(){}
	GameObjectType(short s) { id = s; }

	operator short() { return id; }
	static constexpr int ClientSizeof[ObjectTypeCount] = {
#ifdef _DEBUG
		160,
		416,
		160,
#else
		144,
		400,
		144,
#endif
	};

	static constexpr int ServerSizeof[ObjectTypeCount] =
	{
		sizeof(GameObject),
		sizeof(Player),
		sizeof(Monster),
	};

	static unordered_map<type_offset_pair, short, hash<type_offset_pair>> GetClientOffset;
	static unordered_map<void*, GameObjectType> VptrToTypeTable;
	static void STATICINIT();

	static void AddClientOffset_ptr(GameObjectType gotype, char* obj, char* member, int clientOffset);
};

#define AddClientOffset(a, b, c) GameObjectType::GetClientOffset.insert(std::pair<type_offset_pair, short>(type_offset_pair( \
	a, b), c));

struct World {
	vecset<ClientData> clients; // players
	vecset<GameObject*> gameObjects;

	twoPage tempbuffer;

	int currentIndex = 0; // vector stack??

	static constexpr float lowFrequencyDelay = 0.2f;
	float lowFrequencyFlow = 0.0f;
	__forceinline bool lowHit() {
		return lowFrequencyFlow > lowFrequencyDelay;
	}

	static constexpr float midFrequencyDelay = 0.05f;
	float midFrequencyFlow = 0.0f;
	__forceinline bool midHit() {
		return midFrequencyFlow > midFrequencyDelay;
	}

	static constexpr float highFrequencyDelay = 0.01f;
	float highFrequencyFlow = 0.0f;
	__forceinline bool highHit() {
		return highFrequencyFlow > midFrequencyDelay;
	}

	void Init();
	void Update();
	__forceinline int Receiving(int clientIndex, char* rBuffer);

	int NewObject(GameObject* obj, GameObjectType gotype);

	__forceinline int Sending_NewGameObject(int newindex, GameObject* newobj, GameObjectType gotype);
	__forceinline int Sending_ChangeGameObjectMember(int objindex, GameObject* newobj, GameObjectType gotype, void* memberAddr, int memberSize);
	__forceinline int Sending_NewRay(vec4 rayStart, vec4 rayDirection, float rayDistance);
	__forceinline int Sending_SetMeshInGameObject(int objindex, string str);
	void FireRaycast(GameObject* shooter, vec4 rayStart, vec4 rayDirection, float rayDistance);
	__forceinline int Sending_AllocPlayerIndex(int clientindex, int objindex);
	__forceinline int Sending_DeleteGameObject(int objindex);
	__forceinline void SendToAllClient(int datacap) {
		for (int i = 0; i < clients.size; ++i) {
			if (clients.isnull(i)) continue;
			clients[i].socket.Send(tempbuffer.data, datacap);
		}
	}

	void SendingAllObjectForNewClient(int new_client_index) {
		for (int i = 0; i < gameObjects.size; ++i) {
			if (gameObjects.isnull(i)) continue;
			void* vptr = *(void**)gameObjects[i];
			int datacap = Sending_NewGameObject(i, gameObjects[i], GameObjectType::VptrToTypeTable[vptr]);
			clients[new_client_index].socket.Send((char*)tempbuffer.data, datacap);
			if (gameObjects[i]->MeshIndex >= 0) {
				datacap = Sending_SetMeshInGameObject(i, Mesh::MeshNameArr[gameObjects[i]->MeshIndex]);
				clients[new_client_index].socket.Send((char*)tempbuffer.data, datacap);
			}
		}
	}
};

void PrintOffset() {
	{
		int n = sizeof(GameObject);
		dbglog1(L"class GameObject : size = %d\n", n);
		GameObject temp;

		n = (char*)&temp - (char*)&temp.isExist;
		dbglog1(L"class GameObject.isExist%d\n", n);
		/*n = (char*)&temp - (char*)&temp.diffuseTextureIndex;
		dbglog1(L"class GameObject.diffuseTextureIndex%d\n", n);*/
		n = (char*)&temp - (char*)&temp.m_worldMatrix;
		dbglog1(L"class GameObject.m_worldMatrix%d\n", n);
		n = (char*)&temp - (char*)&temp.LVelocity;
		dbglog1(L"class GameObject.LVelocity%d\n", n);
		n = (char*)&temp - (char*)&temp.tickLVelocity;
		dbglog1(L"class GameObject.tickLVelocity%d\n", n);
		/*n = (char*)&temp - (char*)&temp.m_pMesh;
		dbglog1(L"class GameObject.m_pMesh%d\n", n);
		n = (char*)&temp - (char*)&temp.m_pShader;
		dbglog1(L"class GameObject.m_pShader%d\n", n);
		n = (char*)&temp - (char*)&temp.Destpos;
		dbglog1(L"class GameObject.Destpos%d\n", n);*/
	}
	dbglog1(L"-----------------------------------%d\n\n", rand());
	{
		int n = sizeof(Player);
		dbglog1(L"class Player : size = %d\n", n);
		Player temp;

		n = (char*)&temp - (char*)&temp.isExist;
		dbglog1(L"class Player.isExist%d\n", n);
		/*n = (char*)&temp - (char*)&temp.diffuseTextureIndex;
		dbglog1(L"class Player.diffuseTextureIndex%d\n", n);*/
		n = (char*)&temp - (char*)&temp.m_worldMatrix;
		dbglog1(L"class Player.m_worldMatrix%d\n", n);
		n = (char*)&temp - (char*)&temp.LVelocity;
		dbglog1(L"class Player.LVelocity%d\n", n);
		n = (char*)&temp - (char*)&temp.tickLVelocity;
		dbglog1(L"class Player.tickLVelocity%d\n", n);
		//n = (char*)&temp - (char*)&temp.m_pMesh;
		//dbglog1(L"class Player.m_pMesh%d\n", n);
		//n = (char*)&temp - (char*)&temp.m_pShader;
		//dbglog1(L"class Player.m_pShader%d\n", n);
		//n = (char*)&temp - (char*)&temp.Destpos;
		//dbglog1(L"class Player.Destpos%d\n", n);
		dbglog1(L"-----------------------%d\n", rand());
		n = (char*)&temp - (char*)&temp.ShootFlow;
		dbglog1(L"class Player.ShootFlow%d\n", n);
		n = (char*)&temp - (char*)&temp.recoilFlow;
		dbglog1(L"class Player.recoilFlow%d\n", n);
		n = (char*)&temp - (char*)&temp.HP;
		dbglog1(L"class Player.HP%d\n", n);
		n = (char*)&temp - (char*)&temp.MaxHP;
		dbglog1(L"class Player.MaxHP%d\n", n);
		n = (char*)&temp - (char*)&temp.DeltaMousePos;
		dbglog1(L"class Player.DeltaMousePos%d\n", n);
	}
	dbglog1(L"-----------------------------------%d\n\n", rand());
	{
		int n = sizeof(Monster);
		dbglog1(L"class Monster : size = %d\n", n);
		Monster temp;

		n = (char*)&temp - (char*)&temp.isExist;
		dbglog1(L"class Monster.isExist%d\n", n);
		/*n = (char*)&temp - (char*)&temp.diffuseTextureIndex;
		dbglog1(L"class Monster.diffuseTextureIndex%d\n", n);*/
		n = (char*)&temp - (char*)&temp.m_worldMatrix;
		dbglog1(L"class Monster.m_worldMatrix%d\n", n);
		n = (char*)&temp - (char*)&temp.LVelocity;
		dbglog1(L"class Monster.LVelocity%d\n", n);
		n = (char*)&temp - (char*)&temp.tickLVelocity;
		dbglog1(L"class Monster.tickLVelocity%d\n", n);
		/*n = (char*)&temp - (char*)&temp.m_pMesh;
		dbglog1(L"class Monster.m_pMesh%d\n", n);
		n = (char*)&temp - (char*)&temp.m_pShader;
		dbglog1(L"class Monster.m_pShader%d\n", n);
		n = (char*)&temp - (char*)&temp.Destpos;
		dbglog1(L"class Monster.Destpos%d\n", n);*/
	}
}
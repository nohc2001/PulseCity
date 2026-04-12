#pragma once
#pragma region ProtocolStruct

#pragma pack(push, 1)

/*
설명 : Server 에서 Client로 보내는 데이터 프로토콜의 타입
Sentinal Value :
NULL = (short)0
*/
union STC_Protocol {
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

	STC_Protocol(short id) { n = id; }
	operator short() { return n; }
};

/*
* 설명 : 전체 오브젝트 하나를 통체로 동기화 하고 싶을때 사용된다.
* 각 게임오브젝트들의 맴버변수로 해당 데이터를 만들어 SendDataSaver에 보낼 수 있다.
*/
struct STC_SyncGameObject_Header {
	unsigned int size = 0;
	STC_Protocol st = STC_Protocol::SyncGameObject;
	GameObjectType type;
	int objindex;
};

/*
* 설명 : 게임오브젝트에서 어떤 오브젝트의 어떤 맴버를 변경하고 싶을때 사용된다.
*/
struct STC_ChangeMemberOfGameObject_Header {
	unsigned int size = 0;
	STC_Protocol st = STC_Protocol::ChangeMemberOfGameObject;
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
	STC_Protocol st = STC_Protocol::NewRay;
	XMFLOAT3 raystart;
	XMFLOAT3 rayDir;
	float distance;
};

/*
* 설명 : 클라이언트에게 서버에서 자신과 자신의 오브젝트가 어떻게 관리되고 있는지 알려준다.
*/
struct STC_AllocPlayerIndexes_Header {
	unsigned int size = 14; // 크기고정
	STC_Protocol st = STC_Protocol::AllocPlayerIndexes;

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
	STC_Protocol st = STC_Protocol::DeleteGameObject;
	int obj_index; // 삭제를 진행할 dynamic 오브젝트의 인덱스
};

/*
* 설명 : 아이템이 드롭되었다는 걸 클라이언트에게 알리는 역할.
*/
struct STC_ItemDrop_Header {
	unsigned int size = 48; // 크기고정
	STC_Protocol st = STC_Protocol::ItemDrop;
	int dropindex; // 드롭아이템 인덱스
	ItemLoot lootData; // 루팅된 아이템의 데이터
};

/*
* 설명 : 드롭 아이템이 삭제되었다는걸 클라이언트에게 알리는 역할
*/
struct STC_ItemDropRemove_Header {
	unsigned int size = 10; // 크기고정
	STC_Protocol st = STC_Protocol::ItemDropRemove;
	int dropindex; // 삭제된 드롭아이템의 인덱스
};

/*
* 설명 : 인벤토리의 특정 칸을 동기화 하는 역할
*/
struct STC_InventoryItemSync_Header {
	unsigned int size = 18; // 크기고정
	STC_Protocol st = STC_Protocol::InventoryItemSync;
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
	STC_Protocol st = STC_Protocol::PlayerFire;
	int objindex;
};

/*
* 설명 : 전반적인 게임과 관련된 상태들을 공유한다.
*/
struct STC_SyncGameState_Header {
	unsigned int size = 10; // 크기고정
	STC_Protocol st = STC_Protocol::SyncGameState;
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
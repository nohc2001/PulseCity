#pragma once
#pragma region ProtocolStruct

///���� �߿� ���� ���ǵ�.

//ûũ ������� ���� ����
//#define DEVELOPMODE_ChunckDEBUG
//���� ���� ���� ���߱� ����
#define DEVELOPMODE_SYNC_GLOBAL_ASSET
//GPUResource�� �Ҵ�� ������ GPU VA�� �Բ� �����. � ���ҽ��� ���� �Ҵ�ǰ� ���� �����Ǵ��� �� �� �ִ�.
#define DEVELOPMODE_DBG_GPURESOURCES

///

#pragma pack(push, 1)

/*
���� : Server ���� Client�� ������ ������ ���������� Ÿ��
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

		SyncPlayerMoveZone = 11,

		ServerTransfer = 12,
	};

	// enum�� ���ڷ� ��Ÿ�� ��.
	short n;
	char two_byte[2];

	STC_Protocol(short id) { n = id; }
	operator short() { return n; }
};

/*
* ���� : ��ü ������Ʈ �ϳ��� ��ü�� ����ȭ �ϰ� ������ ���ȴ�.
* �� ���ӿ�����Ʈ���� �ɹ������� �ش� �����͸� ����� SendDataSaver�� ���� �� �ִ�.
*/
struct STC_SyncGameObject_Header {
	unsigned int size = 0;
	STC_Protocol st = STC_Protocol::SyncGameObject;
	GameObjectType type;
	int objindex;
};

/*
* ���� : ���ӿ�����Ʈ���� � ������Ʈ�� � �ɹ��� �����ϰ� ������ ���ȴ�.
*/
struct STC_ChangeMemberOfGameObject_Header {
	unsigned int size = 0;
	STC_Protocol st = STC_Protocol::ChangeMemberOfGameObject;
	GameObjectType type;
	int objindex;
	// ������ �������� ����Ѵ�. (Ŭ�� �˾Ƽ� �ؼ��Ѵ�.)
	int serveroffset;
	int datasize;
	// �� ���ķ� ���� ����ȭ�� �����Ͱ� �ٴ´�.
};

/*
* ���� : �׳� Ŭ���̾�Ʈ���� � �Ѿ� ������ �׸��� �����ϴ� ��.
	��� �浹�� �������� ��������.
*/
struct STC_NewRay_Header {
	unsigned int size = 34; // ũ�����
	STC_Protocol st = STC_Protocol::NewRay;
	XMFLOAT3 raystart;
	XMFLOAT3 rayDir;
	float distance;
};

/*
* ���� : Ŭ���̾�Ʈ���� �������� �ڽŰ� �ڽ��� ������Ʈ�� ��� �����ǰ� �ִ��� �˷��ش�.
*/
struct STC_AllocPlayerIndexes_Header {
	unsigned int size = 14; // ũ�����
	STC_Protocol st = STC_Protocol::AllocPlayerIndexes;

	// �����͸� ���� Ŭ���̾�Ʈ�� ���������� ���° Ŭ���̾�Ʈ����
	int clientindex;
	// �� Ŭ���̾�Ʈ�� �����ϴ� ������Ʈ�� �������� ���° Dynamic������Ʈ����.
	int server_obj_index;
};

/*
* ���� : Ư�� ������Ʈ�� �����Ǿ��ٴ� ����� Ŭ���̾�Ʈ���� �����Ѵ�.
*/
struct STC_DeleteGameObject_Header {
	unsigned int size = 10; // ũ�����
	STC_Protocol st = STC_Protocol::DeleteGameObject;
	int obj_index; // ������ ������ dynamic ������Ʈ�� �ε���
};

/*
* ���� : �������� ��ӵǾ��ٴ� �� Ŭ���̾�Ʈ���� �˸��� ����.
*/
struct STC_ItemDrop_Header {
	unsigned int size = 48; // ũ�����
	STC_Protocol st = STC_Protocol::ItemDrop;
	int dropindex; // ��Ӿ����� �ε���
	ItemLoot lootData; // ���õ� �������� ������
};

/*
* ���� : ��� �������� �����Ǿ��ٴ°� Ŭ���̾�Ʈ���� �˸��� ����
*/
struct STC_ItemDropRemove_Header {
	unsigned int size = 10; // ũ�����
	STC_Protocol st = STC_Protocol::ItemDropRemove;
	int dropindex; // ������ ��Ӿ������� �ε���
};

/*
* ���� : �κ��丮�� Ư�� ĭ�� ����ȭ �ϴ� ����
*/
struct STC_InventoryItemSync_Header {
	unsigned int size = 18; // ũ�����
	STC_Protocol st = STC_Protocol::InventoryItemSync;
	// �κ��丮�� �� ������
	ItemStack Iteminfo;
	// �κ��丮 ���° ĭ����.
	int inventoryIndex;
};

/*
* ���� : ???
*/
struct STC_PlayerFire_Header {
	unsigned int size = 10; // ũ�����
	STC_Protocol st = STC_Protocol::PlayerFire;
	int objindex;
};

/*
* ���� : ���� ������ �⺻ ũ�⸦ ����ȭ�Ѵ�.
*/
struct STC_SyncGameState_Header {
	unsigned int size = 14;
	STC_Protocol st = STC_Protocol::SyncGameState;
	int DynamicGameObjectCapacity;
	int StaticGameObjectCapacity;
};

/*
* ���� : �������� �÷��̾ Zone �̵��� �ϰ� �Ǹ� ���޵Ǵ� ��������
*/
struct STC_PlayerMoveZone_Header {
	unsigned int size = 14;
	STC_Protocol st = STC_Protocol::SyncPlayerMoveZone;
	int clientIndex;
	int zoneId;
};

/*
* ���� : ������ �̵��� ���� Ŭ���̾�Ʈ���� ������ ������ �������� �����Ѵ�.
*/
struct STC_ServerTransfer_Header {
	unsigned int size = 48;
	STC_Protocol st = STC_Protocol::ServerTransfer;
	int dstZoneId;
	unsigned short port;
	int transferToken;
	char ip[32] = {};
};

union CTS_Protocol {
	enum {
		KeyInput = 0,
		SyncRotation = 1,
		ClientHello = 2,
		TransferConnect = 3,
		ServerPlayerTransfer = 4
	};
	short n;
	char two_byte[2];

	CTS_Protocol(short id) { n = id; }
	operator short() { return n; }
};

struct CTS_KeyInput_Header {
	unsigned int size = 8; // ũ�����
	CTS_Protocol st = CTS_Protocol::KeyInput;
	char Key;
	bool isdown;
};

struct CTS_SyncRotation_Header {
	unsigned int size = 14;
	CTS_Protocol st = CTS_Protocol::SyncRotation;
	float yaw;
	float pitch;
};

struct CTS_ClientHello_Header {
	unsigned int size = 6;
	CTS_Protocol st = CTS_Protocol::ClientHello;
};

struct CTS_TransferConnect_Header {
	unsigned int size = 10;
	CTS_Protocol st = CTS_Protocol::TransferConnect;
	int transferToken;
};

struct PlayerTransferData {
	int transferToken = 0;
	int dstZoneId = 0;
	vec4 spawnPos = vec4(0, 0, 0, 1);
	float yaw = 0.0f;
	float pitch = 0.0f;
	float HP = 0.0f;
	float MaxHP = 100.0f;
	int bullets = 0;
	int KillCount = 0;
	int DeathCount = 0;
	float HeatGauge = 0.0f;
	float MaxHeatGauge = 100.0f;
	float HealSkillCooldown = 10.0f;
	float HealSkillCooldownFlow = 0.0f;
	int m_currentWeaponType = 0;
	ItemStack Inventory[36] = {};
};

struct CTS_ServerPlayerTransfer_Header {
	unsigned int size = 0;
	CTS_Protocol st = CTS_Protocol::ServerPlayerTransfer;
	PlayerTransferData data;
};

#pragma pack(pop)
#pragma endregion

// ���� �ִ� UTF8 
   


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

		SkillCast = 13,

		StatusEffect = 14,
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

enum class PlayerJob : int {
	Juggernaut,
	Frost,
	Aegis,
	Mage,
	Healer,
	Gunner,
	Tank,
	Max
};

enum class SkillSlot : int {
	Skill1,
	Skill2,
	Ultimate,
	Max
};

enum class SkillEffectType : int {
	Mage_FireBall,
	Healer_HealAura,
	Gunner_Muzzle,
	Tank_ShockWave,
	Fire_Pillar,
	Fire_Ring,
	Electric_Arc,
	Electric_Burst,
	Ember_Shower,
	Juggernaut_FireProjectile,
	Juggernaut_Taunt,
	Juggernaut_UltimateFire,
	Frost_Cone,
	Frost_IceBlock,
	Frost_Blizzard,
	Aegis_ShieldCharge,
	Aegis_Barrier,
	Aegis_ShieldAura,
};
enum class StatusEffectType : int {
	None,
	Freeze,
	Slow,
	Taunt,
	Burn,
	Stun,
	Paralyze,
	Max
};

struct STC_SkillCast_Header {
	unsigned int size = sizeof(STC_SkillCast_Header);
	STC_Protocol st = STC_Protocol::SkillCast;
	int ownerObjIndex = -1;
	PlayerJob job = PlayerJob::Healer;
	SkillSlot slot = SkillSlot::Skill1;
	SkillEffectType effectType = SkillEffectType::Healer_HealAura;
	vec4 position = vec4(0, 0, 0, 1);
	vec4 direction = vec4(0, 0, 1, 0);
	float radius = 1.0f;
	float power = 1.0f;
	float duration = 1.0f;
};
struct STC_StatusEffect_Header {
	unsigned int size = sizeof(STC_StatusEffect_Header);
	STC_Protocol st = STC_Protocol::StatusEffect;
	int targetObjIndex = -1;
	int sourceObjIndex = -1;
	StatusEffectType statusType = StatusEffectType::None;
	bool active = false;
	float duration = 0.0f;
	float remainTime = 0.0f;
	float power = 0.0f;
	vec4 position = vec4(0, 0, 0, 1);
	vec4 extents = vec4(0.3f, 1.0f, 0.3f, 0.0f);
};

union CTS_Protocol {
	enum {
		KeyInput = 0,
		SyncRotation = 1,
		ClientHello = 2,
		TransferConnect = 3,
		ServerPlayerTransfer = 4,
		ChangeInventoryItemSlot = 5,
		UseSkill = 6,
		ChangeJob = 7,
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
	unsigned int size = sizeof(CTS_SyncRotation_Header);
	CTS_Protocol st = CTS_Protocol::SyncRotation;
	float yaw;
	float pitch;
	bool bFirstPersonVision = true;
};

struct CTS_UseSkill_Header {
	unsigned int size = sizeof(CTS_UseSkill_Header);
	CTS_Protocol st = CTS_Protocol::UseSkill;
	SkillSlot slot = SkillSlot::Skill1;
};

struct CTS_ChangeJob_Header {
	unsigned int size = sizeof(CTS_ChangeJob_Header);
	CTS_Protocol st = CTS_Protocol::ChangeJob;
	PlayerJob job = PlayerJob::Healer;
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
	float Attack = 0.0f;
	float Defense = 0.0f;
	int bullets = 0;
	int KillCount = 0;
	int DeathCount = 0;
	float HeatGauge = 0.0f;
	float MaxHeatGauge = 100.0f;
	float zoneMoveCooldownRemain = 0.0f;
	int lastBoundaryIndex = -1;
	bool wasInsideBoundary = false;
	int m_currentJob = (int)PlayerJob::Healer;
	float SkillCooldown[(int)SkillSlot::Max] = {};
	float SkillCooldownFlow[(int)SkillSlot::Max] = {};
	int m_currentWeaponType = 0;
	ItemStack Inventory[36] = {};
};

struct CTS_ServerPlayerTransfer_Header {
	unsigned int size = 0;
	CTS_Protocol st = CTS_Protocol::ServerPlayerTransfer;
	PlayerTransferData data;
};

enum _ChangeInventoryItemSlot_Type {
	CIIT_ItemCountCombine, // 같은 종류의 아이템 슬롯에 개수를 늘어나게 한다.
	CIIT_Swap, // 다른 종류의 아이템 슬롯과 스왑
	CIIT_ItemMoveToBlankSlot, // 빈 공간에 아이템 위치 바꾸기
};

struct CTS_ChangeInventoryItemSlot_Header {
	unsigned int size = 22;
	CTS_Protocol st = CTS_Protocol::ChangeInventoryItemSlot;
	int destIndex;
	int srcIndex;
	_ChangeInventoryItemSlot_Type ciitType;
	int srcCount;
};


#pragma pack(pop)
#pragma endregion

// ���� �ִ� UTF8 
   


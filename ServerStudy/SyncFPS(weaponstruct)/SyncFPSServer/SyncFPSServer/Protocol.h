#pragma once

// Network object indexes are encoded as zoneId * stride + localObjectIndex on the client.
// The stride must never depend on a zone's current monster count or grow during a transfer.
constexpr int NetworkDynamicObjectCapacityPerZone = 512;
#pragma region ProtocolStruct

///���� �߿� ���� ���ǵ�.

//ûũ �������?���� ����
//#define DEVELOPMODE_ChunckDEBUG
//���� ���� ���� ���߱� ����
#define DEVELOPMODE_SYNC_GLOBAL_ASSET
//GPUResource�� �Ҵ��?������ GPU VA�� �Բ� �����? � ���ҽ��� ���� �Ҵ�ǰ�?���� �����Ǵ��� �� �� �ִ�.
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

		NPCTalkStart = 15,

		AddQuest = 16,
		
		DeleteQuest = 17,

		SyncQuestPrograss = 18,

		BossState = 19,

		DungeonQueueUpdate = 20,

		DungeonEnter = 21,

		// [party] full list of open parties at the portal (shown in the "join party" window).
		PartyList = 22,

		// [party] start was refused because every dungeon instance is busy.
		DungeonReject = 23,

		// Tail marker for the initial/personal snapshot. Receipt proves every earlier personal packet
		// for this zone entry has reached and been parsed by the client.
		InitialSyncComplete = 24,

		// Server-authoritative confirmation that ChangeJob and its default weapon were applied.
		JobChangeAck = 25,

		DungeonResult = 26,
	};

	short n;
	char two_byte[2];

	STC_Protocol(short id) { n = id; }
	operator short() { return n; }
};

/*
* ���� : ��ü ������Ʈ �ϳ��� ��ü�� ����ȭ �ϰ� ������ ���ȴ�.
* �� ���ӿ�����Ʈ���� �ɹ������� �ش� �����͸� �����?SendDataSaver�� ���� �� �ִ�.
*/
struct STC_SyncGameObject_Header {
	unsigned int size = 0;
	STC_Protocol st = STC_Protocol::SyncGameObject;
	GameObjectType type;
	int zoneId = 0;
	int objindex;
};

/*
* ���� : ���ӿ�����Ʈ���� � ������Ʈ�� � �ɹ��� �����ϰ� ������ ���ȴ�.
*/
struct STC_ChangeMemberOfGameObject_Header {
	unsigned int size = 0;
	STC_Protocol st = STC_Protocol::ChangeMemberOfGameObject;
	GameObjectType type;
	int zoneId = 0;
	int objindex;
	// ������ �������� ����Ѵ�? (Ŭ�� �˾Ƽ� �ؼ��Ѵ�.)
	int serveroffset;
	int datasize;
	// �� ���ķ� ���� ����ȭ�� �����Ͱ� �ٴ´�.
};

/*
* ���� : �׳� Ŭ���̾�Ʈ���� � �Ѿ� ������ �׸��� �����ϴ� ��.
	���?�浹�� �������� ��������.
*/
struct STC_NewRay_Header {
	unsigned int size = 34; // ũ�����?
	STC_Protocol st = STC_Protocol::NewRay;
	XMFLOAT3 raystart;
	XMFLOAT3 rayDir;
	float distance;
};

/*
* ���� : Ŭ���̾�Ʈ���� �������� �ڽŰ� �ڽ��� ������Ʈ�� ��� �����ǰ� �ִ��� �˷��ش�.
*/
struct STC_AllocPlayerIndexes_Header {
	unsigned int size = 14; // ũ�����?
	STC_Protocol st = STC_Protocol::AllocPlayerIndexes;

	// �����͸� ���� Ŭ���̾�Ʈ�� ���������� ����?Ŭ���̾�Ʈ����
	int clientindex;
	// �� Ŭ���̾�Ʈ�� �����ϴ� ������Ʈ�� �������� ����?Dynamic������Ʈ����.
	int server_obj_index;
};

struct STC_InitialSyncComplete_Header {
	unsigned int size = sizeof(STC_InitialSyncComplete_Header);
	STC_Protocol st = STC_Protocol::InitialSyncComplete;
	int zoneId = -1;
	int playerObjIndex = -1;
};

struct STC_JobChangeAck_Header {
	unsigned int size = sizeof(STC_JobChangeAck_Header);
	STC_Protocol st = STC_Protocol::JobChangeAck;
	int job = 0;
	int weaponType = 0;
};

/*
* ���� : Ư�� ������Ʈ�� �����Ǿ��ٴ� �����?Ŭ���̾�Ʈ���� �����Ѵ�.
*/
struct STC_DeleteGameObject_Header {
	unsigned int size = sizeof(STC_DeleteGameObject_Header); // ũ�����?
	STC_Protocol st = STC_Protocol::DeleteGameObject;
	int zoneId = 0;
	int obj_index; // ������ ������ dynamic ������Ʈ�� �ε���
};

/*
* ���� : �������� ��ӵǾ��ٴ�?�� Ŭ���̾�Ʈ���� �˸��� ����.
*/
struct STC_ItemDrop_Header {
	unsigned int size = sizeof(STC_ItemDrop_Header); // ũ�����?
	STC_Protocol st = STC_Protocol::ItemDrop;
	int zoneId = 0;
	int dropindex; // ��Ӿ�����?�ε���
	ItemLoot lootData; // ���õ� �������� ������
};

/*
* ���� : ���?�������� �����Ǿ��ٴ°� Ŭ���̾�Ʈ���� �˸��� ����
*/
struct STC_ItemDropRemove_Header {
	unsigned int size = sizeof(STC_ItemDropRemove_Header); // ũ�����?
	STC_Protocol st = STC_Protocol::ItemDropRemove;
	int zoneId = 0;
	int dropindex; // ������ ��Ӿ�������?�ε���
};

/*
* ���� : �κ��丮�� Ư�� ĭ�� ����ȭ �ϴ� ����
*/
struct STC_InventoryItemSync_Header {
	unsigned int size = 18; // ũ�����?
	STC_Protocol st = STC_Protocol::InventoryItemSync;
	// �κ��丮�� ���?������
	ItemStack Iteminfo;
	// �κ��丮 ����?ĭ����.
	int inventoryIndex;
};

/*
* ���� : ???
*/
struct STC_PlayerFire_Header {
	unsigned int size = sizeof(STC_PlayerFire_Header); // ũ�����?
	STC_Protocol st = STC_Protocol::PlayerFire;
	int zoneId = 0;
	int objindex;
	unsigned char fireHand = 0; // 0: right, 1: left
};

/*
* ���� : ���� ������ �⺻ ũ�⸦ ����ȭ�Ѵ�.
*/
struct STC_SyncGameState_Header {
	unsigned int size = sizeof(STC_SyncGameState_Header);
	STC_Protocol st = STC_Protocol::SyncGameState;
	int DynamicGameObjectCapacity;
	int StaticGameObjectCapacity;
	int DynamicGameObjectCapacityPerZone;
	int ZoneCount;
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


struct STC_ServerTransfer_Header {
	unsigned int size = 48;
	STC_Protocol st = STC_Protocol::ServerTransfer;
	int dstZoneId;
	unsigned short port;
	int transferToken;
	char ip[32] = {};
};

// [party/dungeon] Dungeon waiting-queue snapshot sent to the players currently waiting at the portal.
// count = how many are waiting (0..maxCount). Per-slot objindex/hp/maxhp let the client show members.
struct STC_DungeonQueueUpdate_Header {
	unsigned int size = sizeof(STC_DungeonQueueUpdate_Header);
	STC_Protocol st = STC_Protocol::DungeonQueueUpdate;
	int count = 0;
	int maxCount = 3;
	int objindex[3] = {};
	int zoneId[3] = {};
	float hp[3] = {};
	float maxhp[3] = {};
	int dungeonDeathCount = -1;
	int dungeonDeathLimit = 3;
	int m_currentJob[3] = {};   // [party] each member's job (PlayerJob int), -1 if empty slot
	int leaderClientIndex = -1; // [party] clientIndex of the leader; client shows [start] if == own clientIndex
	int partyId = -1;           // [party] which party this snapshot belongs to (-1 = not in a party)
	int number = 0;             // [party] display number -> "파티N" (0 = none)
};

// [party] one row in the open-party list (the "join party" window renders one button per entry).
enum class DungeonResultCode : int {
	None = 0,
	FailedDeaths = 1,
	Aborted = 2,
	Success = 3,
};

struct STC_DungeonResult_Header {
	unsigned int size = sizeof(STC_DungeonResult_Header);
	STC_Protocol st = STC_Protocol::DungeonResult;
	DungeonResultCode result = DungeonResultCode::None;
	int deathCount = 0;
	int deathLimit = 3;
};

struct PartyListEntry {
	int partyId = -1;
	int number = 0;        // display number -> "파티N"
	int memberCount = 0;
	int maxCount = 3;
	int started = 0;       // 1 once the party has entered the dungeon (no longer joinable)
};

constexpr int MaxPartyListEntries = 16;

struct STC_PartyList_Header {
	unsigned int size = sizeof(STC_PartyList_Header);
	STC_Protocol st = STC_Protocol::PartyList;
	int count = 0;
	PartyListEntry parties[MaxPartyListEntries];
};

// [party] server -> leader: the start request was refused (every dungeon instance is occupied).
struct STC_DungeonReject_Header {
	unsigned int size = sizeof(STC_DungeonReject_Header);
	STC_Protocol st = STC_Protocol::DungeonReject;
	int reason = 0; // 0 = all dungeon instances busy
};

// [party/dungeon] Portal-teleport command: the client should disconnect from its current server and
// connect FRESH (ClientHello) to the dungeon server at ip:port, loading dstZoneId. Player state resets.
struct STC_DungeonEnter_Header {
	unsigned int size = sizeof(STC_DungeonEnter_Header);
	STC_Protocol st = STC_Protocol::DungeonEnter;
	unsigned short port = 0;
	int dstZoneId = 0;
	char ip[32] = {};
};

enum class PlayerJob : int {
	Juggernaut,
	Frost,
	Aegis,
	Mage,
	Healer,
	Gunner,
	DroneOperator,
	Hacker,
	Bomber,
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
	Rifle_TacticalGrenade,
	Rifle_StimPack,
	Rifle_MissileBarrage,
	Sniper_GrappleHook,
	Sniper_ModeSwitch,
	Sniper_Railgun,
	DualPistol_DeathDash,
	DualPistol_BladeMode,
	DualPistol_Awaken,
	Drone_Heal,
	Drone_Assault,
	Drone_Flight,
	Hacker_Hack,
	Hacker_EMPField,
	Hacker_EMPBurst,
	Bomber_SpeedBurst,
	Bomber_AmmoSwitch,
	Bomber_Meteor,
	Bomber_FireProjectile,
	Bomber_HealProjectile,
	Bomber_FireExplosion,
	Bomber_HealExplosion,
	Bomber_MeteorTrail,
	Blood_Hit,
	Explosion_Blast,
	Aegis_ShieldEnergy,
	Rifle_GrenadeTrail,
	Rifle_AirStrikeTrail,
	Rifle_StimField,
};
enum class StatusEffectType : int {
	None,
	Freeze,
	Slow,
	Taunt,
	Burn,
	Stun,
	Paralyze,
	Hack,
	Heal,
	Max
};

struct STC_SkillCast_Header {
	unsigned int size = sizeof(STC_SkillCast_Header);
	STC_Protocol st = STC_Protocol::SkillCast;
	int zoneId = 0;
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
	int zoneId = 0;
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

enum PeacefulNPCType {
	PNT_Shop = 0,
	PNT_Quest = 1,
	PNT_None = 2
};

struct STC_NPCTalkStart_Header {
	unsigned int size = 14;
	STC_Protocol st = STC_Protocol::NPCTalkStart;
	PeacefulNPCType NPCType = PNT_None;
	int StartID = 0;
};

struct STC_AddQuest_Header {
	unsigned int size = 10;
	STC_Protocol st = STC_Protocol::AddQuest;
	int QuestID = 0;
};

struct STC_DeleteQuest_Header {
	unsigned int size = 10;
	STC_Protocol st = STC_Protocol::DeleteQuest;
	int QuestID = 0;
};

struct STC_SyncQuestPrograss_Header {
	unsigned int size = 10;
	STC_Protocol st = STC_Protocol::SyncQuestPrograss;
	int questID;
	int questReqSiz;
	// 이후로 questReqSiz 개수 만큼의 int 를 전달.
};

struct BossSyncCoreData {
	vec4 position = vec4(0, 0, 0, 1);
	float hp = 0.0f;
	float maxHP = 0.0f;
	bool active = false;
};

struct BossSyncAoEData {
	int shape = 0;
	vec4 position = vec4(0, 0, 0, 1);
	vec4 direction = vec4(0, 0, 1, 0);
	float radius = 0.0f;
	float width = 0.0f;
	float length = 0.0f;
	float warningTime = 0.0f;
	float age = 0.0f;
	float damage = 0.0f;
	float innerDamage = 0.0f;
	float followTime = 0.0f;
	float lockTime = 0.0f;
	bool active = false;
	bool followPlayer = false;
	bool darkenOnLock = false;
	bool visualSpawned = false;
};

constexpr int BossSyncWarningCapacity = 18;

struct STC_BossState_Header {
	unsigned int size = sizeof(STC_BossState_Header);
	STC_Protocol st = STC_Protocol::BossState;
	int zoneId = 0;
	bool enabled = false;
	int bossObjIndex = -1;
	float bossHP = 0.0f;
	float bossMaxHP = 0.0f;
	vec4 center = vec4(0, 0, 0, 1);
	vec4 aimDirection = vec4(0, 0, 1, 0);
	vec4 railgunDirection = vec4(0, 0, 1, 0);
	bool shieldActive = false;
	float shieldDownTime = 0.0f;
	float groggyTime = 0.0f;
	int phase = 0;
	float phaseTime = 0.0f;
	int patternStep = 0;
	int coreCount = 0;
	BossSyncCoreData cores[3];
	int warningCount = 0;
	BossSyncAoEData warnings[BossSyncWarningCapacity];
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
		// [4단계-STEP1] 서버<->서버 상시 복제 링크 핸드셰이크.
		// 이웃 존 서버에 접속한 뒤 "나는 어느 서버다"를 알리는 첫 메시지.
		ServerLink = 8,
		// [4단계-STEP2] 서버<->서버 플레이어 고스트 동기화(내 존 플레이어 상태를 이웃에 전달).
		GhostSync = 9,
		// [4단계-STEP4] 고스트를 맞혔을 때 소유 서버로 보내는 데미지 적용 요청(RPC).
		GhostDamage = 10,
		// [4단계-STEP5] 몬스터가 경계를 넘었을 때 소유권을 이웃 서버로 넘기는 핸드오프.
		MonsterHandoff = 11,
		CTS_ChangeEquipSlotWithInventorySlot = 12,
		Client_NPCTalkSelection = 13,
		// [party/dungeon] reassigned to 14 to avoid collision with ChangeEquipSlot(12)/NPCTalk(13).
		// [party] now means: the party LEADER pressed the start button.
		DungeonStart = 14,
		// [party] portal party-lobby actions.
		PartyCreate = 15,       // make a new party (caller becomes leader+first member)
		PartyJoin = 16,         // join an existing party by id (CTS_PartyJoin_Header.partyId)
		PartyLeave = 17,        // leave current party
		PartyListRequest = 18,  // ask the server for the current open-party list
		PartyDisband = 19,      // [party] leader-only: disband the whole party (kick all members)
		DungeonAbort = 20,
		StatUp = 21,
	};
	short n;
	char two_byte[2];

	CTS_Protocol(short id) { n = id; }
	operator short() { return n; }
};

// [party/dungeon] sent when the party LEADER presses the start button to begin with current members.
struct CTS_DungeonStart_Header {
	unsigned int size = sizeof(CTS_DungeonStart_Header);
	CTS_Protocol st = CTS_Protocol::DungeonStart;
};

// [party] create a new party. The caller becomes its leader and first member.
struct CTS_DungeonAbort_Header {
	unsigned int size = sizeof(CTS_DungeonAbort_Header);
	CTS_Protocol st = CTS_Protocol::DungeonAbort;
};

struct CTS_PartyCreate_Header {
	unsigned int size = sizeof(CTS_PartyCreate_Header);
	CTS_Protocol st = CTS_Protocol::PartyCreate;
};

// [party] join an existing party by id.
struct CTS_PartyJoin_Header {
	unsigned int size = sizeof(CTS_PartyJoin_Header);
	CTS_Protocol st = CTS_Protocol::PartyJoin;
	int partyId = -1;
};

// [party] leave the party the caller currently belongs to.
struct CTS_PartyLeave_Header {
	unsigned int size = sizeof(CTS_PartyLeave_Header);
	CTS_Protocol st = CTS_Protocol::PartyLeave;
};

// [party] request the current open-party list (server replies with STC_PartyList).
struct CTS_PartyListRequest_Header {
	unsigned int size = sizeof(CTS_PartyListRequest_Header);
	CTS_Protocol st = CTS_Protocol::PartyListRequest;
};

// [party] leader-only: disband the entire party (every member is removed and the party is destroyed).
struct CTS_PartyDisband_Header {
	unsigned int size = sizeof(CTS_PartyDisband_Header);
	CTS_Protocol st = CTS_Protocol::PartyDisband;
};

struct CTS_KeyInput_Header {
	unsigned int size = 8; // ũ�����?
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

enum class PlayerStatType : int {
	HP = 0,
	Defense = 1,
	MoveSpeed = 2,
	Attack = 3,
	Max = 4,
};

struct CTS_StatUp_Header {
	unsigned int size = sizeof(CTS_StatUp_Header);
	CTS_Protocol st = CTS_Protocol::StatUp;
	PlayerStatType stat = PlayerStatType::HP;
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
	int srcZoneId = -1;
	int srcObjIndex = -1;
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
	float ShieldDurability = 0.0f;
	float MaxShieldDurability = 100.0f;
	float zoneMoveCooldownRemain = 0.0f;
	int lastBoundaryIndex = -1;
	bool wasInsideBoundary = false;
	int m_currentJob = (int)PlayerJob::Healer;
	float SkillCooldown[(int)SkillSlot::Max] = {};
	float SkillCooldownFlow[(int)SkillSlot::Max] = {};
	int m_currentWeaponType = 0;
	bool m_weaponHolstered = false;
	float ReloadRemain = 0.0f;
	bool m_sniperDmrMode = false;
	bool m_bomberHealAmmoMode = false;
	ItemStack Inventory[36] = {};
	Weapon weapon[3];
	int SelectedWeapone = 0;
	int Gold = 0;
	int Exp = 0;
	int Level = 0;
	int StatPoint = 0;
	int StatHP = 0;
	int StatDefense = 0;
	int StatMoveSpeed = 0;
	int StatAttack = 0;
	// [party] which party this player belongs to, and the open-world zone they started from.
	// The dungeon process uses partyId to group members into the same instance, and originZoneId
	// to send them back if every instance is busy.
	int partyId = -1;
	int originZoneId = -1;
	int dungeonReturnZoneId = -1;
	vec4 dungeonReturnPosition = vec4(0, 0, 0, 1);
};

struct CTS_ServerPlayerTransfer_Header {
	unsigned int size = 0;
	CTS_Protocol st = CTS_Protocol::ServerPlayerTransfer;
	PlayerTransferData data;
};

// [4단계-STEP1] 서버 간 상시 복제 링크 개설 시 첫 핸드셰이크 헤더.
// 접속을 건 서버가 자기 serverId 를 알려, 받는 쪽이 그 소켓을 'peer 링크'로 표시하게 한다.
struct CTS_ServerLink_Header {
	unsigned int size = sizeof(CTS_ServerLink_Header);
	CTS_Protocol st = CTS_Protocol::ServerLink;
	int fromServerId = -1;
};

// [4단계-STEP2] peer 링크로 보내는 플레이어 한 명의 고스트 상태(위치/시선).
struct GhostPlayerState {
	int objindex = 0;
	int objType = 0;
	int shapeindex = 0;
	vec4 pos = vec4(0, 0, 0, 1);
	float yaw = 0.0f;
	float pitch = 0.0f;
	float HP = 0.0f;
	float MaxHP = 0.0f;
	int isDead = 0;
	int partyId = -1;
};

// [4단계-STEP4] 고스트 히트 시 소유 서버로 보내는 데미지 적용 RPC.
struct CTS_GhostDamage_Header {
	unsigned int size = sizeof(CTS_GhostDamage_Header);
	CTS_Protocol st = CTS_Protocol::GhostDamage;
	int targetZoneId = -1;
	int targetObjIndex = -1;
	float damage = 0.0f;
	int sourcePartyId = -1;
};

// [4단계-STEP5] 몬스터 소유권 이양: 넘어간 쪽 서버가 이 정보로 진짜 몬스터를 새로 생성.
struct CTS_MonsterHandoff_Header {
	unsigned int size = sizeof(CTS_MonsterHandoff_Header);
	CTS_Protocol st = CTS_Protocol::MonsterHandoff;
	int dstZoneId = -1;
	int monsterType = 0;
	vec4 pos = vec4(0, 0, 0, 1);
	float HP = 0.0f;
	float MaxHP = 0.0f;
};

// [4단계-STEP2] 내 존 플레이어 전체 목록을 이웃 서버로 보내는 헤더. 뒤에 count 개의 GhostPlayerState 가 붙는다.
struct CTS_GhostSync_Header {
	unsigned int size = sizeof(CTS_GhostSync_Header);
	CTS_Protocol st = CTS_Protocol::GhostSync;
	int fromZoneId = -1;
	int count = 0;
};

enum _ChangeInventoryItemSlot_Type {
	CIIT_ItemCountCombine, // 같�? 종류???�이???�롯??개수�??�어?�게 ?�다.
	CIIT_Swap, // ?�른 종류???�이???�롯�??�왑
	CIIT_ItemMoveToBlankSlot, // �?공간???�이???�치 바꾸�?
};

struct CTS_ChangeInventoryItemSlot_Header {
	unsigned int size = 22;
	CTS_Protocol st = CTS_Protocol::ChangeInventoryItemSlot;
	int destIndex;
	int srcIndex;
	_ChangeInventoryItemSlot_Type ciitType;
	int srcCount;
};

struct CTS_ChangeEquipSlotWithInventorySlot_Header {
	unsigned int size = 22;
	CTS_Protocol st = CTS_Protocol::CTS_ChangeEquipSlotWithInventorySlot;
	int EquipIndex;
	int InventoryIndex;
};

struct CTS_Client_NPCTalkSelection_Header {
	unsigned int size = 22;
	CTS_Protocol st = CTS_Protocol::Client_NPCTalkSelection;
	int selectionID = 0;
};

#pragma pack(pop)
#pragma endregion

// 서명있는 UTF8 
   


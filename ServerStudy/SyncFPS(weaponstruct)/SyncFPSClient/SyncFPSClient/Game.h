#pragma once
#include "stdafx.h"
#include "main.h"
#include "Render.h"
#include "NetworkDefs.h"
#include "GameObject.h"
#include "MeshSimplifier.h"

extern Client client;

class UVMesh;
class Player;

struct Zone {
	static constexpr int MaxStaticObjectCount = 16384; // 8byte ptr array = 131KB.
	

	static constexpr int OffsetMulArr[3][3] = {
		{ 5, 1, 6 },
		{ 2, 0, 3 },
		{ 7, 4, 8 } };

	// Location information of Zone. Facilitate asset management through the location information.
	int x = 0;
	int y = 0;

	// all of this zone's asset is located GameAssetTable start GlobalAssetCount + Asset_OffsetMul * AssetMAXByZone;
	int Asset_OffsetMul = 0;
	vec4 BasicAABB_onlyXZ = 0;

	// index of zone table
	int zoneid;

	// zone that have near xy with this zone xy
	Zone* nearZones[9] = {};
	char Load_MapName[128];

	Zone* AssetOffsetToNearZone[9] = {};

	// 0 : Zone AABB
	// n != 0 : range that render proxy dynamic object of <n-th nearZone>
	BoundingBox ProxyDynamicObject_VisibleRange[9] = {};
	BoundingBox ProxyStaticObject_VisibleRange[9] = {};

	// Zone??�?
	GameMap* Map = nullptr;

	static constexpr float chunck_divide_Width = 50.0f;
	unordered_map<ChunkIndex, GameChunk*> chunck;
	GameObjectIncludeChunks ZoneChunk_goic;

	// chunk�� ���?������ �Ŀ� �ؾ� �Ѵ�.
	void GetZoneChunkGOIC();

	// �?��??게임?�브?�트�??�는??
	GameObjectIncludeChunks GetChunks_Include_OBB(BoundingOrientedBox obb);
	GameChunk* GetChunkFromPos(vec4 pos);
	void PushGameObject(GameObject* go);
	void PushLight(Light* light);

	// ?�에 ?�인 모든 Static Light??
	vector<Light*> LightTable;

	bool isMapLoaded = false;
	bool bReqireBakeLight_Raster = true;
	bool bReqireBakeLight_Raytracing = true;

	// �?��??붙�? ?�이?�들???�?�하�??�한 ?�로??버퍼
	GPUResource ZoneLightChuncks;

	// StructuredBuffer ?�덱???�근 방법 : zindex + yindex * ChunckCountZ + x * ChunckCountZ * ChunckCountY;
	ChunckLightData* ZoneLightChuncks_Mapped = nullptr;
	DescIndex Immortal_ZoneLightBuffer_SRV;
	int ChunckCountX = 0;
	int ChunckCountY = 0;
	int ChunckCountZ = 0;

	Zone() {

	}

	~Zone() {

	}

	static constexpr float MinimumCenter = -1575;
	static constexpr float ZoneWidth = 350;
	static constexpr float ZoneHalfWidth = ZoneWidth / 2;

	Zone(int zoneindex, const char* name, int _x, int _y) {
		x = _x;
		y = _y;
		Asset_OffsetMul = OffsetMulArr[y % 3][x % 3];
		zoneid = zoneindex;
		strcpy_s(Load_MapName, name);
		ZeroMemory(nearZones, sizeof(Zone*) * 9);
		nearZones[0] = this;

		BasicAABB_onlyXZ = vec4(
			MinimumCenter + ZoneWidth * _x - ZoneHalfWidth, 
			MinimumCenter + ZoneWidth * _y - ZoneHalfWidth, 
			MinimumCenter + ZoneWidth * _x + ZoneHalfWidth,
			MinimumCenter + ZoneWidth * _y + ZoneHalfWidth);
	}

	// �ֺ� �� ���� �����͸� ������.
	void BakeNear() {
		for (int i = 0; i < 9; ++i) {
			AssetOffsetToNearZone[i] = 0;
		}

		for (int i = 0; i < 9; ++i) {
			Zone* zone = nearZones[i];
			if (zone != nullptr) {
				int key = zone->Asset_OffsetMul;
				AssetOffsetToNearZone[key] = zone;
			}
		}
	}

	void GetImmortal_ZoneLightBuffer_SRV();

	static constexpr int MAXZoneTextureCount = 8196;
	static constexpr int MAXZoneMaterialCount = 4096;

	//Zone�� �Ҽӵ� �ؽ��� - �����������? Dynamic Alloc���� ������ �� �ִ� ���¿��� ��.
	vector<GPUResource*> TextureTable;
	static constexpr int MaxTextureZoneMargin = 81960;

	// Zone�� �Ҽӵ� ���͸����?
	vector<Material*> MaterialTable;
	static constexpr int MaxMaterialZoneMargin = 40960;

	/*
	* ������ ������ ������ ����
	* 1. GameObject Release
	* 2. Light Release
	* 3. Texture Release
	* 4. Material Release 
	* 5. Shader Visible DescHeap Release
	* 6. Non Shader Visible DescHeap Release
	* 7. ChunckData Ref Release
	* 8. Map Release - Shape Release
	*/
	void ZoneAssetRelease();
};

/*
* ?�명 : ?�라?�언?�의 게임???��??�는 ?�료구조.
*/
class Game {
public:
	vector<Zone*> ZoneTable;
	Zone* Current_Zone = nullptr;
	int currentZoneId = 0;

	// [seamless] Zone-transfer state. During a transfer we keep the old world visible, take the new
	// server's sync, then remove only objects that were NOT re-sent this transfer (by generation).
	bool isZoneTransfering = false;
	int  transferToZoneId = -1;
	int  transferGeneration = 0;
	// Per-dynamic-object: the transferGeneration in which it was last received. Parallel to
	// DynmaicGameObjects (kept as separate metadata so server/client object layout is untouched).
	std::vector<int> DynamicObjectLastSeenGeneration;

	vec4 LightDirection = vec4(-1, -2, -1);
	BoundingOrientedBox LightOBB;

	//마우???�직???X 부�?변?�량???�는??
	int m_stackMouseX = 0;
	//마우???�직???Y 부�?변?�량???�는??
	int m_stackMouseY = 0;

	// �?��??붙�? ?�이?�들???�?�하�??�한 ?�로??버퍼
	Shader* MyShader;
	//3D 공간?�서 빛처리�? ?��? ?�고, ?�만 보여주는 ?�이??
	OnlyColorShader* MyOnlyColorShader;
	ScreenShader* MyScreenShader;
	WorldTextureShader* MyWorldTextureShader;
	//PBR ?�더링을 ?�는 첫번�??�이??
	PBRShader1* MyPBRShader1;
	//?�카?�박?��? 그리???�이??
	SkyBoxShader* MySkyBoxShader;
	// �?��??붙�? ?�이?�들???�?�하�??�한 ?�로??버퍼
	ComputeTestShader* MyComputeTestShader;
	// �?��??붙�? ?�이?�들???�?�하�??�한 ?�로??버퍼
	RayTracingShader* MyRayTracingShader;

	//AnimationBlendingShader
	AnimationBlendingShader* MyAnimationBlendingShader;
	//HBoneLocalToWorldShader
	HBoneLocalToWorldShader* MyHBoneLocalToWorldShader;

	//MyScreenCharactorShader�??�용??TextRendering???�용?�는 ?�각??Plane Mesh.
	UVMesh* TextMesh;

	//HPBar�??��??�는???�용?�는 Mesh
	Mesh* HPBarMesh;
	//?�기 �??��??�는???�용?�는 Mesh
	//sus <?��? ?�기로는 ?�기??HP??Mesh가 같�??????�개가 ?�는지 모르겠음.>
	Mesh* HeatBarMesh;
	//조�??�을 ?��??�는 ?�육면체 Mesh.
	Mesh* ShootPointMesh;
	//?�재??미니 건을 로드?�다. ?�레?�어가 ?�고 ?�는 총의 Model?�다.
	Model* GunModel;

	BumpMesh* RaytracingTLASBlank = nullptr;

	Model* SniperModel = nullptr;
	Model* MachineGunModel = nullptr;
	Model* ShotGunModel = nullptr;
	Model* RifleModel = nullptr;
	Model* PistolModel = nullptr;
	Model* DualRevolverModel = nullptr;
	Model* DualGunBladeModel = nullptr;
	Model* HackerSMGModel = nullptr;
	Model* BomberGrenadeGunModel = nullptr;
	Model* SupportDroneModel = nullptr;
	Mesh* OBBDebugMesh = nullptr;
	Mesh* BossPrototypeCircleMesh = nullptr;
	Mesh* BossPrototypeCircleOutlineMesh = nullptr;
	Mesh* BossPrototypeSafeCircleMesh = nullptr;
	Mesh* BossPrototypeSafeCircleOutlineMesh = nullptr;
	Mesh* BossPrototypeRectMesh = nullptr;
	UVMesh* BossPrototypeBeamMesh = nullptr;
	UVMesh* BossPrototypeMissileMesh = nullptr;
	UVMesh* BossPrototypeShieldMesh = nullptr;
	Model* BossPrototypeBossModel = nullptr;
	Model* BossPrototypeMiniTurretModel = nullptr;
	Model* BossPrototypeCoreModel = nullptr;
	int BossPrototypeBossShapeIndex = -1;
	int BossPrototypeMiniTurretShapeIndex = -1;
	int BossPrototypeCoreShapeIndex = -1;
	int BossPrototypeBossPlatformNodeIndex = -1;
	int BossPrototypeBossHeadNodeIndex = -1;
	float BossPrototypeHeadPitch = 0.0f;
	float BossPrototypeHeadDrop = 0.0f;
	float BossPrototypeHeadRecoil = 0.0f;

	std::vector<int> MG_BarrelIndices;
	std::vector<int> SG_PumpIndices;
	std::vector<int> Pistol_SlideIndices;

	// GameObject 배열
	std::vector<StaticGameObject*> StaticGameObjects;
	std::vector<DynamicGameObject*> DynmaicGameObjects;

	// �?��??붙�? ?�이?�들???�?�하�??�한 ?�로??버퍼
	vector<ItemLoot> DropedItems;

	// �?��??붙�? ?�이?�들???�?�하�??�한 ?�로??버퍼
	Player* player;
	//RenderOnly
	static constexpr int MaxWeapon = (int)WeaponType::Max;
	GameObject* PlayerWeaponObj[MaxWeapon] = {};

	// 불릿Ray�?모아?��? vecset -> ?�짜???��??�고 ?�라지???�간?�?같으?? ?�형배열??????��.
	// <?�형배열�?고칠 ?�요가 ?�다.>
	vecset<BulletRay> bulletRays;

	// ?�면???�전?�때, x 부분�? 좌우?�전???�담?�고, y부분�? ?�하 ?�전?�담.
	// y??-200 ~ 200 까�???값만 가�????�다.
	vec4 DeltaMousePos;


	int clientIndexInServer = -1;

	int playerGameObjectIndex = -1;
	int DynamicGameObjectCapacityPerZone = 0;
	int DropedItemCapacityPerZone = 4096;

	// [seamless] Defer heavy GPU bone-buffer creation (InitRootBoneMatrixs) across frames.
	// On a server transfer dozens of skinmesh objects arrive in one frame; building all their
	// bone buffers at once stalls the main thread (~1s) and starves the input message pump.
	// We queue the net indices of newly received (unbuilt) skinmesh objects and build only a
	// few per frame in ProcessPendingSkinBoneInit().
	std::vector<int> m_pendingSkinBoneInit;
	// [seamless-B] Objects whose bone buffers were just built. They are enabled (so the per-frame
	// Update skins them) but held out of the render list for one frame, then pushed here. This
	// guarantees a skinmesh is skinned at least once before its first draw, preventing a black flash.
	std::vector<int> m_pendingSkinRenderEnable;

	void ProcessPendingSkinBoneInit();

	int GetDynamicObjectNetIndex(int zoneId, int objIndex) const {
		if (zoneId < 0 || objIndex < 0) return -1;
		if (DynamicGameObjectCapacityPerZone <= 0) return objIndex;
		return zoneId * DynamicGameObjectCapacityPerZone + objIndex;
	}

	int GetDropItemNetIndex(int zoneId, int dropIndex) const {
		if (zoneId < 0 || dropIndex < 0) return -1;
		if (DropedItemCapacityPerZone <= 0) return dropIndex;
		return zoneId * DropedItemCapacityPerZone + dropIndex;
	}

	Mesh* GunMesh;
	GPUResource GunTexture;

	GPUResource DefaultTex;
	GPUResource DefaultNoramlTex;
	GPUResource DefaultAmbientTex;
	Material DefaultMaterial;

	GPUResource LightCBResource;
	LightCB_DATA* LightCBData;

	GPUResource LightCB_withShadowResource[9];
	LightCB_DATA_withShadow* LightCBData_withShadow[9];

	// fix

	SpotLight MyDirLight[3];

	void SetLight();

	vecset<matrix> NpcHPBars;

	bool isPrepared = false;
	bool isPreparedClientIndex = false;
	bool isMapInit = false;
	bool isGlobalAssetInit = false;

	// [party/dungeon] latest party/queue snapshot received from STC_DungeonQueueUpdate.
	// The teammate's party UI reads this to draw members' HP/job. -1 job/objindex = empty slot.
	struct DungeonQueueState {
		bool  active = false;
		int   count = 0;
		int   maxCount = 3;
		int   objindex[3] = {};
		float hp[3] = {};
		float maxhp[3] = {};
		int   m_currentJob[3] = {};
	} m_dungeonQueue;

	bool isInventoryOpen = false;

	static constexpr bool DebugCollisions = false;

	bool isAssetAddingInGlobal = true;
	vector<GPUResource*> TextureTable;
	int GlobalTextureCount = 0;

	vector<Material*> MaterialTable;
	int GlobalMaterialCount = 0;

	vector<Mesh*> MeshTable;
	int GlobalMeshCount = 0;

	vector<HumanoidAnimation> HumanoidAnimationTable;
	int GlobalHumanoidAnimationCount = 0;

	vector<GPUResource*> RenderTextureTable;
	// RenderTextureTable�� �� ���� ������
	// 0 : Global / 1~9 : ZoneLocal
	int RenderTextureTableSizePerZone[10] = {};

	// �������� ���� ���?Immortal ���͸������?ShaderVisibleDescHeap�� �������?����ִ�?�迭
	vector<Material*> RenderMaterialTable;
	// �������� ���� ���?Immortal ���ϸ޽��ν��Ͻ� ���µ��� ShaderVisibleDescHeap�� �������?����ִ�?�迭 

	// RenderMaterialTable�� �� ���� ������
	// 0 : Global / 1~9 : ZoneLocal
	int RenderMaterialTableSizePerZone[10] = {};

	vector<Mesh::InstancingStruct*> RenderInstancingTable;
	//vector<HumanoidAnimation*> RenderHumanoidAnimationTable;

	int GetRenderMaterialIndexFromGlobalMaterialIndex(int globalMatIndex);

	vector<Portal*> Portals;

	void AddMesh(Mesh* mesh);

	void GameTableArrangeMent();

	static constexpr int basicTexMip = 10;
	static constexpr DXGI_FORMAT basicTexFormat = DXGI_FORMAT_BC3_UNORM;
	static constexpr char basicTexFormatStr[] = "BC3_UNORM";

	// particle
	static constexpr UINT FIRE_COUNT = 160;
	static constexpr UINT FIRE_PILLAR_COUNT = 220;
	static constexpr UINT FIRE_RING_COUNT = 160;

	GPUResource FireTextureRes;

	ParticlePool FirePool;
	ParticlePool FirePillarPool;
	ParticlePool FireRingPool;

	ParticleCompute* FireCS = nullptr;
	ParticleCompute* FirePillarCS = nullptr;
	ParticleCompute* FireRingCS = nullptr;

	ParticleCompute* ParticleCS = nullptr;
	ParticleShader* ParticleDraw = nullptr;

	UINT PresentChunkSeekDepth = 0;
	vector<GameChunk*> SameDepthChunkArr[2];

	void InitParticlePool(ParticlePool& pool, UINT count);
	void SpawnSkillEffect(SkillEffectType type, vec4 position, vec4 direction = vec4(0, 0, 1, 0), UINT ownerId = 0, float radius = 1.0f, float power = 1.0f, float duration = 1.0f);
	void SpawnStatusEffect(StatusEffectType type, int targetObjIndex, int sourceObjIndex, bool active, vec4 position, vec4 extents, float duration, float remainTime, float power);

	enum class BossPrototypePhase {
		FindBoss,
		MachineGun,
		MissileLock,
		RailgunCharge,
		Bombardment,
		SummonTurret,
		RotatingLaser,
		Rest,
	};

	enum class BossAoEShape {
		Circle,
		Rectangle,
		SafeCircle,
	};

	struct BossAoEWarning {
		BossAoEShape Shape = BossAoEShape::Circle;
		vec4 Position = vec4(0, 0, 0, 1);
		vec4 Direction = vec4(0, 0, 1, 0);
		float Radius = 2.0f;
		float Width = 2.0f;
		float Length = 6.0f;
		float WarningTime = 1.0f;
		float Age = 0.0f;
		float Damage = 8.0f;
		float InnerDamage = 0.0f;
		float FollowTime = 0.0f;
		float LockTime = 0.0f;
		float CoreDamage = 0.0f;
		bool DamageApplied = false;
		bool Active = true;
		bool FollowPlayer = false;
		bool DarkenOnLock = false;
		bool VisualSpawned = false;
	};

	struct BossPrototypeCore {
		vec4 Position = vec4(0, 0, 0, 1);
		float HP = 1200.0f;
		float MaxHP = 1200.0f;
		float HitFlashTimer = 0.0f;
		float HitFlashDuration = 0.45f;
		bool Active = true;
	};

	struct BossMissileVisual {
		vec4 Start = vec4(0, 0, 0, 1);
		vec4 Target = vec4(0, 0, 0, 1);
		float Age = 0.0f;
		float Duration = 1.0f;
		float Radius = 1.0f;
		bool Active = true;
	};

	bool BossPrototypeEnabled = true;
	int BossPrototypeIndex = -1;
	BossPrototypePhase BossPrototypePhaseState = BossPrototypePhase::FindBoss;
	float BossPrototypePhaseTime = 0.0f;
	float BossPrototypePatternCooldown = 0.0f;
	float BossPrototypeShieldDownTime = 0.0f;
	float BossPrototypeGroggyTime = 0.0f;
	int BossPrototypePatternStep = 0;
	bool BossPrototypeConfigured = false;
	bool BossPrototypeCoresInitialized = false;
	bool BossPrototypeShieldActive = true;
	bool BossPrototypeServerSynced = false;
	bool BossPrototypeVisualGroundInitialized = false;
	bool ModelRenderTintOverrideActive = false;
	vec4 ModelRenderTintOverride = vec4(0.0f, 1.0f, 1.0f, 1.0f);
	float BossPrototypeVisualGroundY = 0.0f;
	vec4 BossPrototypeCenter = vec4(0, 0, 0, 1);
	vec4 BossPrototypeAimDirection = vec4(0, 0, 1, 0);
	vec4 BossPrototypeRailgunDirection = vec4(0, 0, 1, 0);
	vec4 BossPrototypeVisualLookDirection = vec4(0, 0, 1, 0);
	std::vector<BossPrototypeCore> BossPrototypeCores;
	std::vector<BossAoEWarning> BossAoEWarnings;
	std::vector<BossMissileVisual> BossMissileVisuals;
	void ApplyBossState(const STC_BossState_Header& header);
	void UpdateBossPrototype(float deltaTime);
	void RenderBossPrototypeObjects();
	void RenderBossPrototypeAoEs();
	void RenderBossPrototypeShield();
	void RenderAegisShieldVisuals();
	void RenderFrostBlizzardSnowWaves();
	void RenderStatusEffectOverlays();
	void RenderBossPrototypeBeam();
	void RenderPlayerRailgunBeams();
	void RenderRailgunOrbitVisuals();
	void RenderDualBladeFloorVisuals();
	void RenderHackerEmpVisuals();
	void RenderSupportDrones();
	void RenderBossPrototypeMissiles();

	Game() {}
	~Game() {}
	/*
	* 1.GameObjectType::STATICINIT(); ???�해 �?GameObject ??vptr???�어?�다.
	* 2.DropedItems, NpcHPBars, bulletRays �?초기???�다.
	* 3.커맨?�리?�트�?리셋?�여 리소?�들??만들�??�작?�다.
	* 3-1. 기본 ?�스쳐들??만든??  Default~~~Tex
	* 3-2. ?�버???�보�?받아 ?�데?�트 ?????�도�?GlobalTextureArr ??Tile, Wall, Monster ?�스쳐�? 만든??
	* 3-3. 맵을 로드?�다
	* 3-4. SetLight?�수�??�이?��? 초기???�다.
	* 3-5. 뷰포?�의 ?�영?�렬??초기???�다.
	* 3-6. Item ?�의 Mesh�?로드?�고 ?�이???�이블에 ?�이?�들???�는??
	* 3-7. 각종 ?�이?��? 초기?�한??
	* 3-8. 추�??�으�??�요??Shape?�을 로드?�고 ?�?�한??
	* 3-9. 커맨?�리?�트�??�고 GPU???�행?�킨??
	*/
	void Init();

#pragma region UIDefine
	//Global Variable
	vec4 CurrentCursorPos;
	bool LBtnDown = false;
	bool RBtnDown = false;
	wchar_t CurrentCompleteCharactor;
	WPARAM CurrentKeyDown;
	// ?�떤 Key가 ?�려?�는지 ?�현?�는 배열
	UCHAR pKeyBuffer[256];
	DXEvent evt;
	vector<DXPage*> UIPageTable;
	vector<DXPage*> mainPageStack;

	vector<DXPage*>* CurrentPageStack = nullptr;
	vec4 CurrentUICenter = vec4(0, 0, 0, 0);
	static constexpr float ui_depth_epsilon = 0.0001f;
	static constexpr float uipage_depth_epsilon = 0.001f;
	static constexpr float uiwindow_depth_epsilon = 0.01f;

	void WindowNormalizeCoordToDirectXRenderCoord_vec4(vec4& v, float W, float H);
	bool RectContainPos(vec4 rt, vec4 pos);
	bool RectContainRect(vec4 rt, vec4 rt2);
	void UIDraw_TextureRect(vec4 loc, vec4 color, float depth, int uitextureid);
	void UIDraw_TextureRect(vec4 loc, vec4 color, float depth, GPUResource* tex);
	void UIDraw_SolidRect(vec4 loc, vec4 color, float depth);
	// �ؽ��� �������� �׸��� �Լ�
	void UIDraw_TextureLine(vec4 startToEnd, vec4 color, float depth, float LineWidth, int uitextureid);
	
	void UI_Init();
	void UpdateGameplaySkillHUD(float deltaTime);
	void UpdateFloatingDamageTexts(float deltaTime);
	void UpdateDamageFeedback(float deltaTime);
	void NotifyPlayerDamaged(float damage);
	void SpawnFloatingDamageText(vec4 worldPosition, float damage);
	void RenderGameplayStatusHUD();
	void RenderDamageFeedbackHUD();
	void RenderBossPrototypeHUD();
	void RenderBossPrototypeCoreHealthPlates();
	void RenderMonsterHealthPlates();
	void RenderFloatingDamageTexts();
	void RenderGameplaySkillHUD();
	void RenderAmmoHUD();
	void RenderSniperScopeHUD();
	int ScopeOverlayTextureIndex = -1;
	int AmmoHUDFrameTextureIndex = -1;
	int AmmoHUDBulletTextureIndex = -1;
	int AmmoHUDReloadTextureIndex = -1;
	int AmmoHUDWeaponTextureIndices[(int)WeaponType::Max] = { -1, -1, -1, -1, -1, -1, -1, -1, -1 };
	float UltimateChargePercent = 0.0f;
	float UltimateChargePassiveFlow = 0.0f;
	float LastUltimateCooldownFlow = 0.0f;
	int LastUltimateKillCount = 0;
	int LastUltimateJob = -1;
	float DamageBorderAlpha = 0.0f;
	float DamageBorderFadeDuration = 0.30f;
	float LastObservedPlayerHP = -1.0f;
	float LastObservedPlayerMaxHP = -1.0f;
	int LastObservedPlayerJob = -1;
	float PlayerDamageFeedbackSuppressionTime = 0.0f;
	float CameraShakeTimer = 0.0f;
	float CameraShakeDuration = 0.0f;
	float CameraShakeStrength = 0.0f;
	float WhiteDamageFlashAlpha = 0.0f;
	struct FloatingDamageText {
		vec4 WorldPosition = vec4(0, 0, 0, 1);
		float Damage = 0.0f;
		float Age = 0.0f;
		float Duration = 0.85f;
		float SideOffset = 0.0f;
		bool Active = false;
	};
	std::vector<FloatingDamageText> FloatingDamageTexts;

	float depth_min = 0.9999f;
	float depth_max = 0;
	int depthlevel_Count = 0;
	float GetDepth(int level) {
		float rate = (float)level / (float)depthlevel_Count;
		rate = clamp<float>(rate, 0, 1);
		return depth_min + (depth_max - depth_min) * rate;
	}
	void AlignUIDepth();
	bool hasToAlginUIDepth = false;

	vector<GPUResource*> UITextureTable;

	DXUI* GetSlotUIFromPos(vec4 pos);
	SlotData CurrentGrabSlotData;

	static constexpr int inventorySlotCount = 49;
	DXUI* InventorySlots[inventorySlotCount] = {};

#pragma endregion

	// 기존 맵을 모두 ?�제???? ?�로??맵을 로드?�는�?
	void MoveZone(int zoneid, bool keepObjects = false);   // keepObjects=true: seamless transfer path (keeps objects/player); default false = original full reload
	bool BeginServerTransfer(const char* ip, unsigned short port, int dstZoneId, int transferToken);
	void BeginPortalEnter(const char* ip, unsigned short port, int dstZoneId);   // [party/dungeon] fresh reconnect to dungeon server
	void ResendHeldMovementKeys();
	void SetCurrentZoneStaticObjects(int zoneId);
	vec4 GetZoneWorldOffset(int zoneId) const;
	vec4 GetRenderedZoneOffset(int zoneId) const;
	void LoadLinkedZoneMaps();
	void RefreshLoadedZoneMapTransforms();
	void RebuildStaticChunks();
	void ApplyZoneOffsetToStaticObject(GameObject* go);
	void ApplyZoneOffsetToDynamicObject(DynamicGameObject* go);
	void ApplyZoneOffsetToPortal(Portal* portal);

	Zone* GetZoneByPosition(int x, int y);

	/*
	*/
	void Render();

	void Render_RayTracing();

	/*
	*/
	void Render_ShadowPass();

	GPUResource DirLightRes;
	DirLightInfo* MappedDirLightData = nullptr;
	DescIndex DirLightResCBV;
	void InitDirLightGPURes();

	//-----------dynamic Global-----------
	inline static ViewportData* renderViewPort;
	// ?�재 ?�더링을 ?�행?�는 ?�이???�??
	inline static ShaderType PresentShaderType = ShaderType::RenderNormal;
	// 1?�칭 ?��?
	bool bFirstPersonVision = false;
	// delta time
	float DeltaTime;
	double PerfGPUWaitMs = 0.0;
	double PerfGPUPreWaitMs = 0.0;
	double PerfGPUShadowWaitMs = 0.0;
	double PerfGPUMainWaitMs = 0.0;
	double PerfGPUComputeWaitMs = 0.0;
	double PerfGPUFinalWaitMs = 0.0;
	double PerfPresentMs = 0.0;
	int AutoLODShadowStabilityLevel = 0;

	UINT TourID = 0;
	// ?�재 ?�더링을 ?�행?�는 ?�이???�??
	bool SceneRenderBatch = false;
	// ?�더커맨?��? ?�입?�때 ?�브?�트???�더�??�수�?교체?�는 bool 변??
	bool CurrentRenderBatch = false;

	void SetRenderMod(bool isbatch);
	void ClearAllMeshInstancing();


	GameChunk* GetChunckFromPos(vec4 pos);
	GameChunk* GameChunckFind(ChunkIndex& ci) {
		vec4 center = ci.GetAABB().Center;
		bool b = Current_Zone->BasicAABB_onlyXZ.x <= center.x && center.x <= Current_Zone->BasicAABB_onlyXZ.z;
		b = b && Current_Zone->BasicAABB_onlyXZ.y <= center.z && center.z <= Current_Zone->BasicAABB_onlyXZ.w;
		if (b) {
			auto f = Current_Zone->chunck.find(ci);
			if (f != Current_Zone->chunck.end()) return f->second;
		}
		for (int i = 1; i < 9; ++i) {
			if (Current_Zone->nearZones[i] == nullptr) continue;
			if (Current_Zone->nearZones[i]->isMapLoaded == false) continue;
			auto f = Current_Zone->nearZones[i]->chunck.find(ci);
			if (f != Current_Zone->nearZones[i]->chunck.end()) {
				GameChunk* gc = f->second;
				Zone* nz = Current_Zone->nearZones[i];
				bool b = nz->BasicAABB_onlyXZ.x <= gc->AABB.Center.x && gc->AABB.Center.x <= nz->BasicAABB_onlyXZ.z;
				b = b && nz->BasicAABB_onlyXZ.y <= gc->AABB.Center.z && gc->AABB.Center.z <= nz->BasicAABB_onlyXZ.w;
				if (b) {
					return gc;
				}
			}
		}
		return nullptr;
	}


	template <bool isSkinMesh>
	void RenderTour()
	{
		Zone* zone = Current_Zone;
		matrix idmat;
		idmat.Id();
		renderViewPort->UpdateFrustum();
		PresentChunkSeekDepth = 0;
		SameDepthChunkArr[0].clear();
		SameDepthChunkArr[1].clear();
		
		GameChunk* gc = GetChunckFromPos(renderViewPort->Camera_Pos);
		int SDCAIndex = PresentChunkSeekDepth & 1;
		int SDCANextIndex = (PresentChunkSeekDepth + 1) & 1;
		TourID += 1;
		if (gc == nullptr) goto GAMEOBJECTS_RENDER_END;
		SameDepthChunkArr[0].push_back(gc);
		while (gc != nullptr) {
			SDCAIndex = PresentChunkSeekDepth & 1;
			SDCANextIndex = (PresentChunkSeekDepth + 1) & 1;

			if (SameDepthChunkArr[SDCAIndex].size() == 0) break;
			for (int k = 0; k < SameDepthChunkArr[SDCAIndex].size(); ++k) {
				gc = SameDepthChunkArr[SDCAIndex][k];
				if (gc != nullptr) {
					if constexpr (isSkinMesh == false) {
						for (int i = 0; i < gc->Static_gameobjects.size; ++i) {
							if (gc->Static_gameobjects.isnull(i)) continue;
							if (gc->Static_gameobjects[i]->TourID != TourID) {
								(gc->Static_gameobjects[i]->*StaticGameObject::CurrentRenderFunc)(idmat);
								gc->Static_gameobjects[i]->TourID = TourID;
							}
						}

						for (int i = 0; i < gc->Dynamic_gameobjects.size; ++i) {
							if (gc->Dynamic_gameobjects.isnull(i)) continue;
							if (gc->Dynamic_gameobjects[i]->TourID != TourID) {
								(gc->Dynamic_gameobjects[i]->*DynamicGameObject::CurrentRenderFunc)(idmat);
								gc->Dynamic_gameobjects[i]->TourID = TourID;
							}
						}
					}
					else {
						for (int i = 0; i < gc->SkinMesh_gameobjects.size; ++i) {
							if (gc->SkinMesh_gameobjects.isnull(i)) continue;
							if (gc->SkinMesh_gameobjects[i]->TourID != TourID) {
								(gc->SkinMesh_gameobjects[i]->*SkinMeshGameObject::CurrentRenderFunc)(idmat);
								gc->SkinMesh_gameobjects[i]->TourID = TourID;
							}
						}
					}
				}

				for (int ix = -1; ix < 2; ix += 2) {
					ChunkIndex ci = gc->cindex;
				IX_CHUNKFIND:
					ci.x += ix;

					GameChunk* gc0 = GameChunckFind(ci);
					if (gc0 != nullptr) {
						if (gc0->TourID != TourID) {
							if (renderViewPort->m_xmFrustumWorld.Intersects(gc->AABB)) {
								SameDepthChunkArr[SDCANextIndex].push_back(gc0);
							}
							gc0->TourID = TourID;
						}
					}
					else {
						if (renderViewPort->m_xmFrustumWorld.Intersects(ci.GetAABB())) {
							goto IX_CHUNKFIND;
						}
					}
				}
				for (int iy = -1; iy < 2; iy += 2) {
					ChunkIndex ci = gc->cindex;
				IY_CHUNKFIND:
					ci.y += iy;
					GameChunk* gc0 = GameChunckFind(ci);
					if (gc0 != nullptr) {
						if (gc0->TourID != TourID) {
							if (renderViewPort->m_xmFrustumWorld.Intersects(gc->AABB)) {
								SameDepthChunkArr[SDCANextIndex].push_back(gc0);
							}
							gc0->TourID = TourID;
						}
					}
					else {
						if (renderViewPort->m_xmFrustumWorld.Intersects(ci.GetAABB())) {
							goto IY_CHUNKFIND;
						}
					}
				}
				for (int iz = -1; iz < 2; iz += 2) {
					ChunkIndex ci = gc->cindex;
				IZ_CHUNKFIND:
					ci.z += iz;
					GameChunk* gc0 = GameChunckFind(ci);
					if (gc0 != nullptr) {
						if (gc0->TourID != TourID) {
							if (renderViewPort->m_xmFrustumWorld.Intersects(gc->AABB)) {
								SameDepthChunkArr[SDCANextIndex].push_back(gc0);
							}
							gc0->TourID = TourID;
						}
					}
					else {
						if (renderViewPort->m_xmFrustumWorld.Intersects(ci.GetAABB())) {
							goto IZ_CHUNKFIND;
						}
					}
				}
			}
			PresentChunkSeekDepth += 1;
			SameDepthChunkArr[SDCAIndex].clear();
		}
	GAMEOBJECTS_RENDER_END:
		return;
	}

	void BatchRender(ID3D12GraphicsCommandList* cmd);

	/*
	*/
	void Update();

	/*
	* 매개변??:
	* char* ptr : 받�? ?�이?�의 ?�작주소
	* int totallen : 받�? ?�이?�의 바이??개수
	* 매개변??:
	* ?�재 ?�기�??�료??바이???��? 반환.
	*/
	int Receiving(char* ptr, int totallen = 0);

	/*
	* 매개변??:
	* int deltaX : X 쪽으�??�직???�도
	* int deltaY : Y 쪽으�??�직???�도
	*/
	void AddMouseInput(int deltaX, int deltaY);
	/*
	* 매개변??:
	* const wchar_t* wstr : 출력??문자??
	* int length : 문자??길이
	* vec4 Rect : 문자?�이 그려�??�역
	* float fontsiz : ?�스???�트 ?�이�?
	* float depth : ?�떤 깊이값에???�스?��? ?�더�??�는지 결정.
	*/
	void RenderText(const wchar_t* wstr, int length, vec4 Rect, float fontsiz, float depth = 0.01f);

	void RenderSDFText(const wchar_t* wstr, int length, vec4 Rect, float fontsiz, vec4 color, float* minD, float* maxD, float depth, vec4* SDFRectOut = nullptr);

	DescIndex Immortal_ZoneLightBuffer_SRV[9] = {};

	static constexpr float lowFrequencyDelay = 1.0f;
	float lowFrequencyFlow = 0.0f;
	__forceinline bool lowHit() {
		return lowFrequencyFlow > lowFrequencyDelay;
	}
	static constexpr float midFrequencyDelay = 0.2f;
	float midFrequencyFlow = 0.0f;
	__forceinline bool midHit() {
		return midFrequencyFlow > midFrequencyDelay;
	}
	static constexpr float highFrequencyDelay = 0.017f;
	float highFrequencyFlow = 0.0f;
	__forceinline bool highHit() {
		return highFrequencyFlow > highFrequencyDelay;
	}

	Material* GetMaterialFromRenderMaterialIndex(int matindex) {
		Material* mat = nullptr;
		int MaterialIndex = matindex;
		int ZoneOff = (MaterialIndex / Zone::MAXZoneMaterialCount);
		int InnerZoneIndex = MaterialIndex - ZoneOff * Zone::MAXZoneMaterialCount;
		if (ZoneOff > 0) {
			ZoneOff -= 1;
			mat = Current_Zone->AssetOffsetToNearZone[ZoneOff]->MaterialTable[InnerZoneIndex];
		}
		else {
			mat = MaterialTable[InnerZoneIndex];
		}
		return mat;
	}

	//NPC Talk Data
	vector<NPCTalkData> NPCTalkTable;
	int PresentShowedTalkID = 0;
	int PresentShopID = 0;
	int UITourID = 0;

	vector<Quest*> QuestTable;

	//STC ���� �÷��̾ ������ �ִ� ����Ʈ ���?
	vector<int> QuestArr;
	int presentShowQuestOffset; // ���� ����?����Ʈ�� �����ְ� �ִ���
	vector<Quest*> QuestPrograss;
};

extern Game game;
extern GlobalDevice gd;
//template ?�수 구현부
template <bool isSkinMesh>
void ModelNode::Render(void* model, GPUCmd& cmd, const matrix& parentMat, void* pGameobject)
{
	Model* pModel = (Model*)model;
	XMMATRIX sav;
	GameObject* obj = (GameObject*)pGameobject;
	if (obj == nullptr) sav = XMMatrixMultiply(transform, parentMat);
	else {
		int nodeindex = ((byte8*)this - (byte8*)pModel->Nodes) / sizeof(ModelNode);
		if (obj->transforms_innerModel == nullptr) {
			sav = XMMatrixMultiply(transform, parentMat);
		}
		else {
			sav = XMMatrixMultiply(obj->transforms_innerModel[nodeindex], parentMat);
		}
	}

	if (numMesh != 0 && Meshes != nullptr) {
		if constexpr (isSkinMesh == false) {
			//bump mesh
			matrix m = sav;
			m.transpose();

			cmd->SetGraphicsRoot32BitConstants(1, 16, &m, 0);
			if (game.PresentShaderType == ShaderType::RenderWithShadow) {
				vec4 hitFlash = vec4(0.0f, 1.0f, 1.0f, 1.0f);
				if (game.ModelRenderTintOverrideActive) {
					hitFlash = game.ModelRenderTintOverride;
				}
				else if (pGameobject != nullptr && obj->tag[GameObjectTag::Tag_SkinMeshObject]) {
					SkinMeshGameObject* smgo = (SkinMeshGameObject*)pGameobject;
					hitFlash = smgo->GetRenderTint();
				}
				cmd->SetGraphicsRoot32BitConstants(PBRShader1::RootParamId::Const_ModelHitFlash, 4, &hitFlash, 0);
			}
			for (int i = 0; i < numMesh; ++i) {
				Mesh* drawMesh = pModel->mMeshes[Meshes[i]];
				if (AutoLOD_IsModelLODRenderActive()) {
					Mesh* lodMesh = AutoLOD_GetLODMesh(drawMesh, AutoLOD_GetModelLODRenderLevel());
					if (lodMesh == nullptr && AutoLOD_GetModelLODRenderLevel() > 0) lodMesh = AutoLOD_GetLODMesh(drawMesh, 0);
					if (lodMesh != nullptr) drawMesh = lodMesh;
				}
				if (drawMesh->type == Mesh::MeshType::_BumpMesh) {
					BumpMesh* Bmesh = (BumpMesh*)drawMesh;
					for (int k = 0; k < Bmesh->subMeshNum; ++k) {
						using PBRRPI = PBRShader1::RootParamId;
						Material* mat = game.GetMaterialFromRenderMaterialIndex(materialIndex[k]);
						cmd->SetGraphicsRootDescriptorTable(PBRRPI::SRVTable_MaterialTextures, mat->TextureSRVTableIndex.hRender.hgpu);
						cmd->SetGraphicsRootDescriptorTable(PBRRPI::CBVTable_Material, mat->CB_Resource.descindex.hRender.hgpu);
						drawMesh->Render(cmd, 1, k);
					}
				}
			}
		}
		else {
			//skin mesh
			SkinMeshGameObject* smgo = (SkinMeshGameObject*)pGameobject;
			for (int i = 0; i < numMesh; ++i) {
				if (pModel->mMeshes[Meshes[i]]->type == Mesh::MeshType::_SkinedBumpMesh) {
					using PBRRPI = PBRShader1::RootParamId;
					BumpSkinMesh* bmesh = (BumpSkinMesh*)((BumpSkinMesh*)pModel->mMeshes[Meshes[i]]);

					if constexpr (gd.PlayAnimationByGPU == false) {
						//copying
						int skindex = Mesh_SkinMeshindex[i];
						int boneNum = pModel->mBumpSkinMeshs[skindex]->MatrixCount;
						vec4 hitFlash = smgo->GetRenderTint();
						cmd->SetGraphicsRoot32BitConstants(PBRRPI::Const_SkinMeshHitFlash, 4, &hitFlash, 0);
						UINT ncbElementBytes = (((sizeof(matrix) * 128) + 255) & ~255); //256??배수
						gd.gpucmd.ResBarrierTr(&smgo->BoneToWorldMatrixCB_Default[skindex], D3D12_RESOURCE_STATE_COPY_DEST);
						gd.gpucmd.ResBarrierTr(&smgo->BoneToWorldMatrixCB[skindex], D3D12_RESOURCE_STATE_COPY_SOURCE);
						gd.gpucmd->CopyBufferRegion(smgo->BoneToWorldMatrixCB_Default[skindex].resource, 0, smgo->BoneToWorldMatrixCB[skindex].resource, 0, ncbElementBytes);
						gd.gpucmd.ResBarrierTr(&smgo->BoneToWorldMatrixCB_Default[skindex], D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
						gd.gpucmd.ResBarrierTr(&smgo->BoneToWorldMatrixCB[skindex], D3D12_RESOURCE_STATE_GENERIC_READ);

						//Set Offset
						DescHandle OffsetMatrixCBVHandle;
						gd.ShaderVisibleDescPool.DynamicAlloc(&OffsetMatrixCBVHandle, 1);
						gd.pDevice->CopyDescriptorsSimple(1, OffsetMatrixCBVHandle.hcpu, bmesh->ToOffsetMatrixsCB.descindex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
						cmd->SetGraphicsRootDescriptorTable(PBRRPI::CBVTable_SkinMeshOffsetMatrix, OffsetMatrixCBVHandle.hgpu);

						//Set ToWorld
						DescHandle ToWorldMatrixCBVHandle;
						gd.ShaderVisibleDescPool.DynamicAlloc(&ToWorldMatrixCBVHandle, 1);
						gd.pDevice->CopyDescriptorsSimple(1, ToWorldMatrixCBVHandle.hcpu, smgo->BoneToWorldMatrixCB_Default[Mesh_SkinMeshindex[i]].descindex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
						cmd->SetGraphicsRootDescriptorTable(PBRRPI::CBVTable_SkinMeshToWorldMatrix, ToWorldMatrixCBVHandle.hgpu);

						for (int k = 0; k < bmesh->subMeshNum; ++k) {
							//Material& mat = game.MaterialTable[materialIndex[k]];

							Material* mat = game.GetMaterialFromRenderMaterialIndex(materialIndex[k]);
							
							cmd->SetGraphicsRootDescriptorTable(PBRRPI::SRVTable_SkinMeshMaterialTextures, mat->TextureSRVTableIndex.hRender.hgpu);
							cmd->SetGraphicsRootDescriptorTable(PBRRPI::CBVTable_SkinMeshMaterial, mat->CB_Resource.descindex.hRender.hgpu);

							pModel->mMeshes[Meshes[i]]->Render(cmd, 1, k);
						}
					}
					else {
						//Set Offset
						if (true) {
							vec4 hitFlash = smgo->GetRenderTint();
							cmd->SetGraphicsRoot32BitConstants(PBRRPI::Const_SkinMeshHitFlash, 4, &hitFlash, 0);
							DescHandle OffsetMatrixCBVHandle;
							gd.ShaderVisibleDescPool.DynamicAlloc(&OffsetMatrixCBVHandle, 1);
							gd.pDevice->CopyDescriptorsSimple(1, OffsetMatrixCBVHandle.hcpu, bmesh->ToOffsetMatrixsCB.descindex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
							cmd->SetGraphicsRootDescriptorTable(PBRRPI::CBVTable_SkinMeshOffsetMatrix, OffsetMatrixCBVHandle.hgpu);

							//Set ToWorld
							DescHandle ToWorldMatrixCBVHandle;
							gd.ShaderVisibleDescPool.DynamicAlloc(&ToWorldMatrixCBVHandle, 1);
							gd.pDevice->CopyDescriptorsSimple(1, ToWorldMatrixCBVHandle.hcpu, smgo->BoneToWorldMatrixCB_Default[Mesh_SkinMeshindex[i]].descindex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
							cmd->SetGraphicsRootDescriptorTable(PBRRPI::CBVTable_SkinMeshToWorldMatrix, ToWorldMatrixCBVHandle.hgpu);

							for (int k = 0; k < bmesh->subMeshNum; ++k) {
								Material* mat = game.GetMaterialFromRenderMaterialIndex(materialIndex[k]);
								cmd->SetGraphicsRootDescriptorTable(PBRRPI::SRVTable_SkinMeshMaterialTextures, mat->TextureSRVTableIndex.hRender.hgpu);
								cmd->SetGraphicsRootDescriptorTable(PBRRPI::CBVTable_SkinMeshMaterial, mat->CB_Resource.descindex.hRender.hgpu);

								pModel->mMeshes[Meshes[i]]->Render(cmd, 1, k);
							}
						}
					}
				}
			}
		}
	}

	if (numChildren != 0 && Childrens != nullptr) {
		for (int i = 0; i < numChildren; ++i) {
			Childrens[i]->Render<isSkinMesh>(model, cmd, sav, pGameobject);
		}
	}
}

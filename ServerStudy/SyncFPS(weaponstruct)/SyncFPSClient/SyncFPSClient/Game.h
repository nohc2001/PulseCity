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

	// Zone??ë§?
	GameMap* Map = nullptr;

	static constexpr float chunck_divide_Width = 50.0f;
	unordered_map<ChunkIndex, GameChunk*> chunck;
	GameObjectIncludeChunks ZoneChunk_goic;

	// chunk¸¦ ¸ğµÎ ±¸¼ºÇÑ ÈÄ¿¡ ÇØ¾ß ÇÑ´Ù.
	void GetZoneChunkGOIC();

	// ì²?¬??ê²Œì„?¤ë¸Œ?íŠ¸ë¥??£ëŠ”??
	GameObjectIncludeChunks GetChunks_Include_OBB(BoundingOrientedBox obb);
	GameChunk* GetChunkFromPos(vec4 pos);
	void PushGameObject(GameObject* go);
	void PushLight(Light* light);

	// ?¬ì— ?°ì¸ ëª¨ë“  Static Light??
	vector<Light*> LightTable;

	bool isMapLoaded = false;
	bool bReqireBakeLight_Raster = true;
	bool bReqireBakeLight_Raytracing = true;

	// ì²?¬??ë¶™ì? ?¼ì´?¸ë“¤???€?¥í•˜ê¸??„í•œ ?…ë¡œ??ë²„í¼
	GPUResource ZoneLightChuncks;

	// StructuredBuffer ?¸ë±???‘ê·¼ ë°©ë²• : zindex + yindex * ChunckCountZ + x * ChunckCountZ * ChunckCountY;
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

	// ÁÖº¯ Á¸ °ü·Ã µ¥ÀÌÅÍ¸¦ ±¸¼ºÇÔ.
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

	//Zone¿¡ ¼Ò¼ÓµÈ ÅØ½ºÃÄ - ¸¸µé¾îÁ³Áö¸¸, Dynamic AllocÀ¸·Î º¸¿©ÁÙ ¼ö ÀÖ´Â »óÅÂ¿©¾ß ÇÔ.
	vector<GPUResource*> TextureTable;
	static constexpr int MaxTextureZoneMargin = 81960;

	// Zone¿¡ ¼Ò¼ÓµÈ ¸ÓÅÍ¸®¾óµé.
	vector<Material*> MaterialTable;
	static constexpr int MaxMaterialZoneMargin = 40960;
};

/*
* ?¤ëª… : ?´ë¼?´ì–¸?¸ì˜ ê²Œì„???˜í??´ëŠ” ?ë£Œêµ¬ì¡°.
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

	//ë§ˆìš°???€ì§ì„??X ë¶€ë¶?ë³€?”ëŸ‰???“ëŠ”??
	int m_stackMouseX = 0;
	//ë§ˆìš°???€ì§ì„??Y ë¶€ë¶?ë³€?”ëŸ‰???“ëŠ”??
	int m_stackMouseY = 0;

	// ì²?¬??ë¶™ì? ?¼ì´?¸ë“¤???€?¥í•˜ê¸??„í•œ ?…ë¡œ??ë²„í¼
	Shader* MyShader;
	//3D ê³µê°„?ì„œ ë¹›ì²˜ë¦¬ë? ?˜ì? ?Šê³ , ?‰ë§Œ ë³´ì—¬ì£¼ëŠ” ?°ì´??
	OnlyColorShader* MyOnlyColorShader;
	ScreenShader* MyScreenShader;
	//PBR ?Œë”ë§ì„ ?˜ëŠ” ì²«ë²ˆì§??°ì´??
	PBRShader1* MyPBRShader1;
	//?¤ì¹´?´ë°•?¤ë? ê·¸ë¦¬???°ì´??
	SkyBoxShader* MySkyBoxShader;
	// ì²?¬??ë¶™ì? ?¼ì´?¸ë“¤???€?¥í•˜ê¸??„í•œ ?…ë¡œ??ë²„í¼
	ComputeTestShader* MyComputeTestShader;
	// ì²?¬??ë¶™ì? ?¼ì´?¸ë“¤???€?¥í•˜ê¸??„í•œ ?…ë¡œ??ë²„í¼
	RayTracingShader* MyRayTracingShader;

	//AnimationBlendingShader
	AnimationBlendingShader* MyAnimationBlendingShader;
	//HBoneLocalToWorldShader
	HBoneLocalToWorldShader* MyHBoneLocalToWorldShader;

	//MyScreenCharactorShaderë¥??¬ìš©??TextRendering???¬ìš©?˜ëŠ” ?¬ê°??Plane Mesh.
	UVMesh* TextMesh;

	//HPBarë¥??˜í??´ëŠ”???¬ìš©?˜ëŠ” Mesh
	Mesh* HPBarMesh;
	//?´ê¸° ë¥??˜í??´ëŠ”???¬ìš©?˜ëŠ” Mesh
	//sus <?´ê? ?Œê¸°ë¡œëŠ” ?´ê¸°??HP??Meshê°€ ê°™ì??????ê°œê°€ ?ˆëŠ”ì§€ ëª¨ë¥´ê² ìŒ.>
	Mesh* HeatBarMesh;
	//ì¡°ì??ì„ ?˜í??´ëŠ” ?•ìœ¡ë©´ì²´ Mesh.
	Mesh* ShootPointMesh;
	//?„ì¬??ë¯¸ë‹ˆ ê±´ì„ ë¡œë“œ?œë‹¤. ?Œë ˆ?´ì–´ê°€ ?¤ê³  ?ˆëŠ” ì´ì˜ Model?´ë‹¤.
	Model* GunModel;

	Model* SniperModel = nullptr;
	Model* MachineGunModel = nullptr;
	Model* ShotGunModel = nullptr;
	Model* RifleModel = nullptr;
	Model* PistolModel = nullptr;
	Mesh* OBBDebugMesh = nullptr;

	std::vector<int> MG_BarrelIndices;
	std::vector<int> SG_PumpIndices;
	std::vector<int> Pistol_SlideIndices;

	// GameObject ë°°ì—´
	std::vector<StaticGameObject*> StaticGameObjects;
	std::vector<DynamicGameObject*> DynmaicGameObjects;

	// ì²?¬??ë¶™ì? ?¼ì´?¸ë“¤???€?¥í•˜ê¸??„í•œ ?…ë¡œ??ë²„í¼
	vector<ItemLoot> DropedItems;

	// ì²?¬??ë¶™ì? ?¼ì´?¸ë“¤???€?¥í•˜ê¸??„í•œ ?…ë¡œ??ë²„í¼
	Player* player;
	//RenderOnly
	static constexpr int MaxWeapon = 5;
	GameObject* PlayerWeaponObj[MaxWeapon] = {};

	// ë¶ˆë¦¿Rayë¥?ëª¨ì•„?“ì? vecset -> ?´ì§œ???˜í??˜ê³  ?¬ë¼ì§€???œê°„?€ ê°™ìœ¼?? ?˜í˜•ë°°ì—´??????‹¤.
	// <?˜í˜•ë°°ì—´ë¡?ê³ ì¹  ?„ìš”ê°€ ?ˆë‹¤.>
	vecset<BulletRay> bulletRays;

	// ?”ë©´???Œì „? ë•Œ, x ë¶€ë¶„ì? ì¢Œìš°?Œì „???¹ë‹´?˜ê³ , yë¶€ë¶„ì? ?í•˜ ?Œì „?¹ë‹´.
	// y??-200 ~ 200 ê¹Œì???ê°’ë§Œ ê°€ì§????ˆë‹¤.
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
	// RenderTextureTableÀÇ °¢ Á¸ÀÇ »çÀÌÁî
	// 0 : Global / 1~9 : ZoneLocal
	int RenderTextureTableSizePerZone[10] = {};

	// ·»´õ¿¡¼­ ¾²ÀÏ ¸ğµç Immortal ¸ÓÅÍ¸®¾óµéÀÌ ShaderVisibleDescHeapÀÇ ¼ø¼­´ë·Î ´ã°ÜÀÖ´Â ¹è¿­
	vector<Material*> RenderMaterialTable;
	// ·»´õ¿¡¼­ ¾²ÀÏ ¸ğµç Immortal ´ÜÀÏ¸Ş½¬ÀÎ½ºÅÏ½Ì ¿¡¼ÂµéÀÌ ShaderVisibleDescHeapÀÇ ¼ø¼­´ë·Î ´ã°ÜÀÖ´Â ¹è¿­ 

	// RenderMaterialTableÀÇ °¢ Á¸ÀÇ »çÀÌÁî
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

	Game() {}
	~Game() {}
	/*
	* 1.GameObjectType::STATICINIT(); ???µí•´ ê°?GameObject ??vptr???»ì–´?¸ë‹¤.
	* 2.DropedItems, NpcHPBars, bulletRays ë¥?ì´ˆê¸°???œë‹¤.
	* 3.ì»¤ë§¨?œë¦¬?¤íŠ¸ë¥?ë¦¬ì…‹?˜ì—¬ ë¦¬ì†Œ?¤ë“¤??ë§Œë“¤ê¸??œì‘?œë‹¤.
	* 3-1. ê¸°ë³¸ ?ìŠ¤ì³ë“¤??ë§Œë“ ??  Default~~~Tex
	* 3-2. ?œë²„???•ë³´ë¥?ë°›ì•„ ?…ë°?´íŠ¸ ?????ˆë„ë¡?GlobalTextureArr ??Tile, Wall, Monster ?ìŠ¤ì³ë? ë§Œë“ ??
	* 3-3. ë§µì„ ë¡œë“œ?œë‹¤
	* 3-4. SetLight?¨ìˆ˜ë¡??¼ì´?¸ë? ì´ˆê¸°???œë‹¤.
	* 3-5. ë·°í¬?¸ì˜ ?¬ì˜?‰ë ¬??ì´ˆê¸°???œë‹¤.
	* 3-6. Item ?¤ì˜ Meshë¥?ë¡œë“œ?˜ê³  ?„ì´???Œì´ë¸”ì— ?„ì´?œë“¤???´ëŠ”??
	* 3-7. ê°ì¢… ?°ì´?”ë? ì´ˆê¸°?”í•œ??
	* 3-8. ì¶”ê??ìœ¼ë¡??„ìš”??Shape?¤ì„ ë¡œë“œ?˜ê³  ?€?¥í•œ??
	* 3-9. ì»¤ë§¨?œë¦¬?¤íŠ¸ë¥??«ê³  GPU???¤í–‰?œí‚¨??
	*/
	void Init();

#pragma region UIDefine
	//Global Variable
	vec4 CurrentCursorPos;
	bool LBtnDown = false;
	bool RBtnDown = false;
	wchar_t CurrentCompleteCharactor;
	WPARAM CurrentKeyDown;
	// ?´ë–¤ Keyê°€ ?Œë ¤?ˆëŠ”ì§€ ?œí˜„?˜ëŠ” ë°°ì—´
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
	// ÅØ½ºÃÄ Á÷¼±À¸·Î ±×¸®±â ÇÔ¼ö
	void UIDraw_TextureLine(vec4 startToEnd, vec4 color, float depth, float LineWidth, int uitextureid);
	
	void UI_Init();
	void UpdateGameplaySkillHUD(float deltaTime);
	void UpdateFloatingDamageTexts(float deltaTime);
	void SpawnFloatingDamageText(vec4 worldPosition, float damage);
	void RenderGameplayStatusHUD();
	void RenderMonsterHealthPlates();
	void RenderFloatingDamageTexts();
	void RenderGameplaySkillHUD();
	float UltimateChargePercent = 0.0f;
	float UltimateChargePassiveFlow = 0.0f;
	float LastUltimateCooldownFlow = 0.0f;
	int LastUltimateKillCount = 0;
	int LastUltimateJob = -1;
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

	// ê¸°ì¡´ ë§µì„ ëª¨ë‘ ?´ì œ???? ?ˆë¡œ??ë§µì„ ë¡œë“œ?˜ëŠ”ê²?
	void MoveZone(int zoneid, bool keepObjects = false);   // keepObjects=true: seamless transfer path (keeps objects/player); default false = original full reload
	bool BeginServerTransfer(const char* ip, unsigned short port, int dstZoneId, int transferToken);
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
	// ?„ì¬ ?Œë”ë§ì„ ?˜í–‰?˜ëŠ” ?°ì´???€??
	inline static ShaderType PresentShaderType = ShaderType::RenderNormal;
	// 1?¸ì¹­ ?¬ë?
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

	UINT TourID = 0;
	// ?„ì¬ ?Œë”ë§ì„ ?˜í–‰?˜ëŠ” ?°ì´???€??
	bool SceneRenderBatch = false;
	// ?Œë”ì»¤ë§¨?œë? ?½ì…? ë•Œ ?¤ë¸Œ?íŠ¸???Œë”ë§??¨ìˆ˜ë¥?êµì²´?˜ëŠ” bool ë³€??
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
	* ë§¤ê°œë³€??:
	* char* ptr : ë°›ì? ?°ì´?°ì˜ ?œì‘ì£¼ì†Œ
	* int totallen : ë°›ì? ?°ì´?°ì˜ ë°”ì´??ê°œìˆ˜
	* ë§¤ê°œë³€??:
	* ?„ì¬ ?½ê¸°ë¥??„ë£Œ??ë°”ì´???˜ë? ë°˜í™˜.
	*/
	int Receiving(char* ptr, int totallen = 0);

	/*
	* ë§¤ê°œë³€??:
	* int deltaX : X ìª½ìœ¼ë¡??€ì§ì¸ ?•ë„
	* int deltaY : Y ìª½ìœ¼ë¡??€ì§ì¸ ?•ë„
	*/
	void AddMouseInput(int deltaX, int deltaY);
	/*
	* ë§¤ê°œë³€??:
	* const wchar_t* wstr : ì¶œë ¥??ë¬¸ì??
	* int length : ë¬¸ì??ê¸¸ì´
	* vec4 Rect : ë¬¸ì?´ì´ ê·¸ë ¤ì§??ì—­
	* float fontsiz : ?ìŠ¤???°íŠ¸ ?¬ì´ì¦?
	* float depth : ?´ë–¤ ê¹Šì´ê°’ì—???ìŠ¤?¸ê? ?Œë”ë§??˜ëŠ”ì§€ ê²°ì •.
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

	//STC ÇöÀç ÇÃ·¹ÀÌ¾î°¡ °¡Áö°í ÀÖ´Â Äù½ºÆ® ¸ñ·Ï
	vector<int> QuestArr;
	int presentShowQuestOffset; // ÇöÀç ¸î¹øÂ° Äù½ºÆ®¸¦ º¸¿©ÁÖ°í ÀÖ´ÂÁö
	vector<Quest*> QuestPrograss;
};

extern Game game;
extern GlobalDevice gd;
//template ?¨ìˆ˜ êµ¬í˜„ë¶€
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
						UINT ncbElementBytes = (((sizeof(matrix) * 128) + 255) & ~255); //256??ë°°ìˆ˜
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

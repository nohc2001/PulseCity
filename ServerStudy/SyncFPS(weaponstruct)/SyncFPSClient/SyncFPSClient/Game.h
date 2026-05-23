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

//696byte
struct Zone {
	static constexpr int MaxStaticObjectCount = 16384; // 8byte ptr array = 131KB.
	static constexpr int MAXZoneTextureCount = 16384;
	static constexpr int MAXZoneMaterialCount = 8196;

	static constexpr int OffsetMulArr[3][3] = { 
		{ 5, 1, 6 }, 
		{ 2, 0, 3 }, 
		{ 7, 4, 8 } };

	// Location information of Zone. Facilitate asset management through the location information.
	int x = 0;
	int y = 0;
	
	// all of this zone's asset is located GameAssetTable start GlobalAssetCount + Asset_OffsetMul * AssetMAXByZone;
	int Asset_OffsetMul = 0;

	// index of zone table
	int zoneid;

	// zone that have near xy with this zone xy
	Zone* nearZones[9] = {};
	const char* Load_MapName;

	// 0 : Zone AABB
	// n != 0 : range that render proxy dynamic object of <n-th nearZone>
	BoundingBox ProxyDynamicObject_VisibleRange[9] = {};
	BoundingBox ProxyStaticObject_VisibleRange[9] = {};

	//Zone�� ��
	GameMap* Map = nullptr;

	//ûũ ����
	//�ϳ��� ûũ�� ������ü�� �� ���� ���̸� �����Ѵ�.
	static constexpr float chunck_divide_Width = 50.0f;
	//chunkIndex�� StaticCollision�� ���� ûũ�� ã�� ���� Map
	unordered_map<ChunkIndex, GameChunk*> chunck;

	// ûũ�� ���ӿ�����Ʈ�� �ִ´�.
	GameObjectIncludeChunks GetChunks_Include_OBB(BoundingOrientedBox obb);
	GameChunk* GetChunkFromPos(vec4 pos);
	void PushGameObject(GameObject* go);
	void PushLight(Light* light);

	// ���� ���� ��� Static Light��
	vector<Light*> LightTable;

	bool bReqireBakeLight_Raster = true;
	bool bReqireBakeLight_Raytracing = true;

	// ûũ�� ���� ����Ʈ���� �����ϱ� ���� ���ε� ����
	GPUResource ZoneLightChuncks;

	// StructuredBuffer �ε��� ���� ��� : zindex + yindex * ChunckCountZ + x * ChunckCountZ * ChunckCountY;
	ChunckLightData* ZoneLightChuncks_Mapped = nullptr;
	DescIndex Immortal_ZoneLightBuffer_SRV;
	int ChunckCountX = 0;
	int ChunckCountY = 0;
	int ChunckCountZ = 0;

	Zone() {

	}

	~Zone() {

	}

	Zone(int zoneindex, const char* name, int _x, int _y) {
		x = _x;
		y = _y;
		Asset_OffsetMul = OffsetMulArr[y % 3][x % 3];
		zoneid = zoneindex;
		Load_MapName = name;
		ZeroMemory(nearZones, sizeof(Zone*) * 9);
		nearZones[0] = this;
	}

	void GetImmortal_ZoneLightBuffer_SRV();
};

/*
* ���� : Ŭ���̾�Ʈ�� ������ ��Ÿ���� �ڷᱸ��.
*/
class Game {
public:
	vector<Zone*> ZoneTable;
	Zone* Current_Zone = nullptr;
	int currentZoneId = 0;

	// ���� �¾籤 ���� (�¾� -> ��ü ���� ����)
	vec4 LightDirection = vec4(-1, -2, -1);
	BoundingOrientedBox LightOBB;

	//���콺 �������� X �κ� ��ȭ���� �״´�.
	int m_stackMouseX = 0;
	//���콺 �������� Y �κ� ��ȭ���� �״´�.
	int m_stackMouseY = 0;

	//�ؽ��ĸ� ������� �ʴ� ���� ��ó���� �ϴ� ���̴�
	Shader* MyShader;
	//3D �������� ��ó���� ���� �ʰ�, ���� �����ִ� ���̴�
	OnlyColorShader* MyOnlyColorShader;
	//RECT�� �޾� �ؽ��ĸ� ȭ��� ������ �׸��� ���̴�. ���ڸ� ����Ҷ� ���� ���δ�.
	ScreenShader* MyScreenShader;
	//PBR �������� �ϴ� ù��° ���̴�.
	PBRShader1* MyPBRShader1;
	//��ī�̹ڽ��� �׸��� ���̴�.
	SkyBoxShader* MySkyBoxShader;
	// ������ ���̴�
	ComputeTestShader* MyComputeTestShader;
	//����Ʈ���̽� ���̴�
	RayTracingShader* MyRayTracingShader;

	//AnimationBlendingShader
	AnimationBlendingShader* MyAnimationBlendingShader;
	//HBoneLocalToWorldShader
	HBoneLocalToWorldShader* MyHBoneLocalToWorldShader;

	//MyScreenCharactorShader�� ����� TextRendering�� ���Ǵ� �簢�� Plane Mesh.
	UVMesh* TextMesh;
	
	//HPBar�� ��Ÿ���µ� ���Ǵ� Mesh
	Mesh* HPBarMesh;
	//���� �� ��Ÿ���µ� ���Ǵ� Mesh
	//sus <���� �˱�δ� ����� HP�� Mesh�� ������ �� �ΰ��� �ִ��� �𸣰���.>
	Mesh* HeatBarMesh;
	//�������� ��Ÿ���� ������ü Mesh.
	Mesh* ShootPointMesh;
	//����� �̴� ���� �ε��Ѵ�. �÷��̾ ��� �ִ� ���� Model�̴�.
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

	// GameObject �迭
	std::vector<StaticGameObject*> StaticGameObjects;
	std::vector<DynamicGameObject*> DynmaicGameObjects;

	// ��ӵ� �������� �迭
	vector<ItemLoot> DropedItems;

	// Ŭ���̾�Ʈ�� �����ϴ� �÷��̾�
	Player* player;

	// �Ҹ�Ray�� ��Ƴ��� vecset -> ��¥�� ��Ÿ���� ������� �ð��� ������, ȯ���迭�� �� ����.
	// <ȯ���迭�� ��ĥ �ʿ䰡 �ִ�.>
	vecset<BulletRay> bulletRays;

	// ȭ���� ȸ���Ҷ�, x �κ��� �¿�ȸ���� ����ϰ�, y�κ��� ���� ȸ�����.
	// y�� -200 ~ 200 ������ ���� ���� �� �ִ�.
	vec4 DeltaMousePos;


	// �������� Ŭ���̾�Ʈ�� �ε���
	int clientIndexInServer = -1;

	// ���ӿ�����Ʈ �迭���� Ŭ���̾�Ʈ�� �����ϴ� �÷��̾��� �ε���
	int playerGameObjectIndex = -1;

	//������ ���Ǿ��� ���� �޽�. 
	//<������ ����� ��������, ���߿� ����� ���������Ŷ� ���� ��� ���� ���ϰ� �� ó���� �ϴ� �ؾ���.>
	Mesh* GunMesh;
	// ������ ���Ǿ��� ���Ѹ޽��� �ؽ���
	GPUResource GunTexture;
	
	// �ؽ��İ� ������ ��ü�ϱ� ���� �⺻ ��ǻ�� �ؽ���
	GPUResource DefaultTex;
	// �ؽ��İ� ������ ��ü�ϱ� ���� �⺻ ��� �ؽ���
	GPUResource DefaultNoramlTex;
	// �ؽ��İ� ������ ��ü�ϱ� ���� �⺻ �ں��Ʈ �ؽ���
	GPUResource DefaultAmbientTex;
	// ù��° ���͸���
	Material DefaultMaterial;

	// �Ƹ� �� ������ �ʰԵ� ����Ʈ ������ CB ���ҽ� (�����찡 ���Ե� ������ ������.)
	GPUResource LightCBResource;
	LightCB_DATA* LightCBData;

	// ����Ʈ ������ ��� �ִ� ������ ������ ���� CB ���ҽ�
	GPUResource LightCB_withShadowResource[9];
	// Upload Buffer�� Mapping �� cpu �޸�
	LightCB_DATA_withShadow* LightCBData_withShadow[9];

	// <���� �ʿ�>
	// ������ ���� ������ ��ü. ���̴���� �Ǿ� ������ ���� ���̴� ������ ������ �ʴ´�.
	// ���߿� ����� ���� ������ �����ؾ� ��.
	// fix

	//  DirectionLight ������ �ִ�.
	SpotLight MyDirLight[3];

	// ����Ʈ ������ �ʱ�ȭ �Ѵ�.
	void SetLight();

	// NPC ä�¹ٵ��� ���� ����� �迭.
	vecset<matrix> NpcHPBars;

	// sus <�� ������ �� �̷��� �Ǿ� �ִ��� ���� �Ը� �ʿ�>
	// Ŭ���̾�Ʈ ������ �غ�Ǿ��ٴ� ���� �˸��� ��ȣ
	bool isPrepared = false;
	// Ŭ���̾�Ʈ �ε����� �޾Ƴ����ٴ� ��ȣ
	bool isPreparedClientIndex = false;
	// ���� ���� �Ϸ�Ǿ����� �˸��� ��ȣ
	bool isMapInit = false;
	// ���� ������ ���� �Ϸ�Ǿ����� �˸��� ��ȣ
	bool isGlobalAssetInit = false;

	// �÷��̾� �κ��丮 â�� ���ȴ��� ����
	bool isInventoryOpen = false;

	// �浹ü���� ���¸� �����ִ���
	static constexpr bool DebugCollisions = false;

	bool isAssetAddingInGlobal = true;
	// ��� �ؽ��ĵ��� ����ִ� �迭
	vector<GPUResource*> TextureTable;
	// ���� ��ü���� �������� ���� ��� �ؽ��ĵ��� ����. 
	// �ش� �ε��� ���ķδ� ���� �������� ���� ��� �ؽ��ĵ��� ����ִ� �迭 ������ �ִ�.
	int GlobalTextureCount = 0;

	// ��� ���͸������ ����� �ִ� �迭
	vector<Material*> MaterialTable;
	// ���� ��ü���� �������� ���� ��� ���͸������ ����. 
	// �ش� �ε��� ���ķδ� ���� �������� ���� ��� ���͸������ ����ִ� �迭 ������ �ִ�.
	int GlobalMaterialCount = 0;

	// ��� ���ϸ޽����� ����� �ִ� �迭
	vector<Mesh*> MeshTable;
	// ���� ��ü���� �������� ���� ��� ���ϸ޽����� ����. 
	// �ش� �ε��� ���ķδ� ���� �������� ���� ��� ���ϸ޽����� ����ִ� �迭 ������ �ִ�.
	int GlobalMeshCount = 0;

	// ��� �޸ӳ��̵� �ִϸ��̼ǵ��� ����� �ִ� �迭
	vector<HumanoidAnimation> HumanoidAnimationTable;
	// ���� ��ü���� �������� ���� ��� �޸ӳ��̵� �ִϸ��̼ǵ��� ����. 
	// �ش� �ε��� ���ķδ� ���� �������� ���� ��� �޸ӳ��̵� �ִϸ��̼ǵ��� ����ִ� �迭 ������ �ִ�.
	int GlobalHumanoidAnimationCount = 0;

	// �������� ���� ��� Immortal �ؽ��ĵ��� ShaderVisibleDescHeap�� ������� ����ִ� �迭
	vector<GPUResource*> RenderTextureTable;
	// �������� ���� ��� Immortal ���͸������ ShaderVisibleDescHeap�� ������� ����ִ� �迭
	vector<Material*> RenderMaterialTable;
	// �������� ���� ��� Immortal ���ϸ޽��ν��Ͻ� ���µ��� ShaderVisibleDescHeap�� ������� ����ִ� �迭
	vector<Mesh::InstancingStruct*> RenderInstancingTable;
	//// �������� ���� ��� Immortal �޸ӳ��̵� �ִϸ��̼ǵ��� ShaderVisibleDescHeap�� ������� ����ִ� �迭
	//vector<HumanoidAnimation*> RenderHumanoidAnimationTable;

	int GetRenderMaterialIndexFromGlobalMaterialIndex(int globalMatIndex);

	// ���� �������� ��Ż�� �迭
	vector<Portal*> Portals;

	void AddMesh(Mesh* mesh);

	void GameTableArrangeMent();

	// �⺻ �ؽ��� �Ӹ� ����
	static constexpr int basicTexMip = 10;
	// �⺻ �ؽ��� �������� ����
	static constexpr DXGI_FORMAT basicTexFormat = DXGI_FORMAT_BC3_UNORM;
	// �⺻ �ؽ��� �������� ���� ���ڿ� - dds ���鶧 ���δ�.
	static constexpr char basicTexFormatStr[] = "BC3_UNORM";

	// particle
	static constexpr UINT FIRE_COUNT = 200;
	static constexpr UINT FIRE_PILLAR_COUNT = 400;
	static constexpr UINT FIRE_RING_COUNT = 300;

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

	Game() {}
	~Game() {}
	/*
	* ���� : ������ �ʱ�ȭ �ϴ� �Լ�.
	* 1.GameObjectType::STATICINIT(); �� ���� �� GameObject �� vptr�� ����.
	* 2.DropedItems, NpcHPBars, bulletRays �� �ʱ�ȭ �Ѵ�.
	* 3.Ŀ�ǵ帮��Ʈ�� �����Ͽ� ���ҽ����� ����� �����Ѵ�.
	* 3-1. �⺻ �ؽ��ĵ��� �����.  Default~~~Tex
	* 3-2. ������ ������ �޾� ������Ʈ �� �� �ֵ��� GlobalTextureArr �� Tile, Wall, Monster �ؽ��ĸ� �����.
	* 3-3. ���� �ε��Ѵ�
	* 3-4. SetLight�Լ��� ����Ʈ�� �ʱ�ȭ �Ѵ�.
	* 3-5. ����Ʈ�� ��������� �ʱ�ȭ �Ѵ�.
	* 3-6. Item ���� Mesh�� �ε��ϰ� ������ ���̺��� �����۵��� ��´�.
	* 3-7. ���� ���̴��� �ʱ�ȭ�Ѵ�.
	* 3-8. �߰������� �ʿ��� Shape���� �ε��ϰ� �����Ѵ�.
	* 3-9. Ŀ�ǵ帮��Ʈ�� �ݰ� GPU�� �����Ų��.
	*/
	void Init();

#pragma region UIDefine
	//Global Variable
	// xy : ���콺 ��ǥ��. DirectX Render Coord��. 
	vec4 CurrentCursorPos;
	// ���� ����Ŭ���Ǿ��ִ���
	bool LBtnDown = false;
	// ���� ������Ŭ���Ǿ��ִ���
	bool RBtnDown = false;
	// �ֱٿ� IME�� ���� �ϼ��� ����.
	wchar_t CurrentCompleteCharactor;
	// �ֱٿ� ���� Key
	WPARAM CurrentKeyDown;
	// � Key�� �����ִ��� ǥ���ϴ� �迭
	UCHAR pKeyBuffer[256];
	// �ֱٿ� ����� �̺�Ʈ ��.
	DXEvent evt;
	// ��� UI Page�� ��Ƴ��� ���̺�
	vector<DXPage*> UIPageTable;
	// ���� ��Ÿ�� UI Page���� Stack
	vector<DXPage*> mainPageStack;

	vector<DXPage*>* CurrentPageStack = nullptr;
	vec4 CurrentUICenter = vec4(0, 0, 0, 0);
	static constexpr float ui_depth_epsilon = 0.0001f;
	static constexpr float uipage_depth_epsilon = 0.001f;
	static constexpr float uiwindow_depth_epsilon = 0.01f;

	// ������ ��� ����ȭ ��ǥ�� (0~1) y�� �Ʒ��� �� ���� �þ. / ���� DirectX ���� ��ǥ��� �ٲٴ� �Լ�
	// �Ϲ� 2d ���� ���� �ص� �ǰ�, vec4�� �̷���� ���簢�� ������ �����ص� ��.
	void WindowNormalizeCoordToDirectXRenderCoord_vec4(vec4& v, float W, float H);
	// � ������ � ���� ���ԵǴ��� ����
	bool RectContainPos(vec4 rt, vec4 pos);
	// � ������ � ������ ���ԵǴ��� ����
	bool RectContainRect(vec4 rt, vec4 rt2);
	// �ؽ��� ���簢�� �������� �׸��� �Լ�
	void UIDraw_TextureRect(vec4 loc, vec4 color, float depth, int uitextureid);
	// �ؽ��� �������� �׸��� �Լ�
	void UIDraw_TextureLine(vec4 startToEnd, vec4 color, float depth, float LineWidth, int uitextureid);
	//UI �ʱ�ȭ �ڵ�.
	void UI_Init();

	//UI Depthj ���� �ڵ�
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

	// UI�� ���� �ؽ��ĵ��� Table
	vector<GPUResource*> UITextureTable;

	DXUI* GetSlotUIFromPos(vec4 pos);
	SlotData CurrentGrabSlotData;

	static constexpr int inventorySlotCount = 49;
	DXUI* InventorySlots[inventorySlotCount] = {};

#pragma endregion

	// ���� ���� ��� ������ ��, ���ο� ���� �ε��ϴ°�.
	void MoveZone(int zoneid);
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

	/*
	* ���� : ������ ������ �Ѵ�.
	*/
	void Render();

	void Render_RayTracing();

	/*
	* ���� : ������ ���� �������Ѵ�.
	*/
	void Render_ShadowPass();

	GPUResource DirLightRes;
	DirLightInfo* MappedDirLightData = nullptr;
	DescIndex DirLightResCBV;
	void InitDirLightGPURes();

	//-----------dynamic Global-----------
	// ���� �������� �����ϴ� viewport
	inline static ViewportData* renderViewPort;
	// ���� �������� �����ϴ� ���̴� Ÿ��
	inline static ShaderType PresentShaderType = ShaderType::RenderNormal;
	// 1��Ī ����
	bool bFirstPersonVision = false;
	// delta time
	float DeltaTime;

	UINT TourID = 0;
	// �������� �Ҷ� ��ġó�� ������ ��� ����
	bool SceneRenderBatch = false;
	// ����Ŀ�ǵ带 �����Ҷ� ������Ʈ�� ������ �Լ��� ��ü�ϴ� bool ����
	bool CurrentRenderBatch = false;

	void SetRenderMod(bool isbatch);
	void ClearAllMeshInstancing();

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
		GameChunk* gc = zone->GetChunkFromPos(renderViewPort->Camera_Pos);
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
					auto gci = zone->chunck.find(ci);
					if (gci != zone->chunck.end()) {
						GameChunk* gc0 = gci->second;
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
					auto gci = zone->chunck.find(ci);
					if (gci != zone->chunck.end()) {
						GameChunk* gc0 = gci->second;
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
					auto gci = zone->chunck.find(ci);
					if (gci != zone->chunck.end()) {
						GameChunk* gc0 = gci->second;
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
	* ���� : ������ ������Ʈ �Ѵ�.
	*/
	void Update();
	
	/*
	* ���� : �������� ���� �����͸� �ؼ��� Ŭ���̾�Ʈ�� ����ȭ�� �Ѵ�.
	* �Ű����� : 
	* char* ptr : ���� �������� �����ּ�
	* int totallen : ���� �������� ����Ʈ ����
	* ��ȯ : 
	* ���� �б⸦ �Ϸ��� ����Ʈ ���� ��ȯ.
	*/
	int Receiving(char* ptr, int totallen = 0);

	/*
	* ���� : ���콺 �������� �Ͼ����, DeltaMousePos�� �����Ű�� �Լ�.
	* �Ű����� : 
	* int deltaX : X ������ ������ ����
	* int deltaY : Y ������ ������ ����
	*/
	void AddMouseInput(int deltaX, int deltaY);
	/*
	* ���� : �ؽ�Ʈ�� �������Ѵ�.
	* �Ű����� : 
	* const wchar_t* wstr : ����� ���ڿ�
	* int length : ���ڿ� ����
	* vec4 Rect : ���ڿ��� �׷��� ����
	* float fontsiz : �ؽ�Ʈ ��Ʈ ������
	* float depth : � ���̰����� �ؽ�Ʈ�� ������ �Ǵ��� ����.
	*/
	void RenderText(const wchar_t* wstr, int length, vec4 Rect, float fontsiz, float depth = 0.01f);

	void RenderSDFText(const wchar_t* wstr, int length, vec4 Rect, float fontsiz, vec4 color, float* minD, float* maxD, float depth, vec4* SDFRectOut = nullptr);

	//Zone���� LightStructuredBuffer�� ��� �ִ� �迭
	DescIndex Immortal_ZoneLightBuffer_SRV[9] = {};

	// �� ����
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
};

extern Game game;
extern GlobalDevice gd;
//template �Լ� ������
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
						Material* mat = game.MaterialTable[materialIndex[k]];
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
						UINT ncbElementBytes = (((sizeof(matrix) * 128) + 255) & ~255); //256�� ���
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
							Material& mat = game.MaterialTable[materialIndex[k]];
							cmd->SetGraphicsRootDescriptorTable(PBRRPI::SRVTable_SkinMeshMaterialTextures, mat.TextureSRVTableIndex.hRender.hgpu);
							cmd->SetGraphicsRootDescriptorTable(PBRRPI::CBVTable_SkinMeshMaterial, mat.CB_Resource.descindex.hRender.hgpu);

							pModel->mMeshes[Meshes[i]]->Render(cmd, 1, k);
						}
					}
					else {
						//Set Offset
						if (true) {

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
								Material* mat = game.MaterialTable[materialIndex[k]];
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

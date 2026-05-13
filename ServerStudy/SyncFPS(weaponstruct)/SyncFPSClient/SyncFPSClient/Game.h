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

	//Zone의 맵
	GameMap* Map = nullptr;

	//청크 정보
	//하나의 청크의 정육면체의 한 변의 길이를 결정한다.
	static constexpr float chunck_divide_Width = 50.0f;
	//chunkIndex로 StaticCollision을 위한 청크를 찾기 위한 Map
	unordered_map<ChunkIndex, GameChunk*> chunck;

	// 청크에 게임오브젝트를 넣는다.
	GameObjectIncludeChunks GetChunks_Include_OBB(BoundingOrientedBox obb);
	GameChunk* GetChunkFromPos(vec4 pos);
	void PushGameObject(GameObject* go);
	void PushLight(Light* light);

	// 씬에 쓰인 모든 Static Light들
	vector<Light*> LightTable;

	bool bReqireBakeLight_Raster = true;
	bool bReqireBakeLight_Raytracing = true;

	// 청크에 붙은 라이트들을 저장하기 위한 업로드 버퍼
	GPUResource ZoneLightChuncks;

	// StructuredBuffer 인덱스 접근 방법 : zindex + yindex * ChunckCountZ + x * ChunckCountZ * ChunckCountY;
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
* 설명 : 클라이언트의 게임을 나타내는 자료구조.
*/
class Game {
public:
	vector<Zone*> ZoneTable;
	Zone* Current_Zone = nullptr;
	int currentZoneId = 0;

	// 씬의 태양광 방향 (태양 -> 물체 방향 기준)
	vec4 LightDirection = vec4(-1, -2, -1);
	BoundingOrientedBox LightOBB;

	//마우스 움직임의 X 부분 변화량을 쌓는다.
	int m_stackMouseX = 0;
	//마우스 움직임의 Y 부분 변화량을 쌓는다.
	int m_stackMouseY = 0;

	//텍스쳐를 사용하지 않는 색과 빛처리만 하는 셰이더
	Shader* MyShader;
	//3D 공간에서 빛처리를 하지 않고, 색만 보여주는 셰이더
	OnlyColorShader* MyOnlyColorShader;
	//RECT를 받아 텍스쳐를 화면상에 영역에 그리는 셰이더. 글자를 출력할때 많이 쓰인다.
	ScreenShader* MyScreenShader;
	//PBR 렌더링을 하는 첫번째 셰이더.
	PBRShader1* MyPBRShader1;
	//스카이박스를 그리는 셰이더.
	SkyBoxShader* MySkyBoxShader;
	// 블러링 셰이더
	ComputeTestShader* MyComputeTestShader;
	//레이트레이싱 셰이더
	RayTracingShader* MyRayTracingShader;

	//AnimationBlendingShader
	AnimationBlendingShader* MyAnimationBlendingShader;
	//HBoneLocalToWorldShader
	HBoneLocalToWorldShader* MyHBoneLocalToWorldShader;

	//MyScreenCharactorShader를 사용해 TextRendering에 사용되는 사각형 Plane Mesh.
	UVMesh* TextMesh;
	
	//HPBar를 나타내는데 사용되는 Mesh
	Mesh* HPBarMesh;
	//열기 를 나타내는데 사용되는 Mesh
	//sus <내가 알기로는 열기든 HP든 Mesh가 같은데 왜 두개가 있는지 모르겠음.>
	Mesh* HeatBarMesh;
	//조준점을 나타내는 정육면체 Mesh.
	Mesh* ShootPointMesh;
	//현재는 미니 건을 로드한다. 플레이어가 들고 있는 총의 Model이다.
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

	// GameObject 배열
	std::vector<StaticGameObject*> StaticGameObjects;
	std::vector<DynamicGameObject*> DynmaicGameObjects;

	// 드롭된 아이템의 배열
	vector<ItemLoot> DropedItems;

	// 클라이언트가 조작하는 플레이어
	Player* player;

	// 불릿Ray를 모아놓은 vecset -> 어짜피 나타나고 사라지는 시간은 같으니, 환형배열이 더 낮다.
	// <환형배열로 고칠 필요가 있다.>
	vecset<BulletRay> bulletRays;

	// 화면을 회전할때, x 부분은 좌우회전을 당담하고, y부분은 상하 회전당담.
	// y는 -200 ~ 200 까지의 값만 가질 수 있다.
	vec4 DeltaMousePos;


	// 서버에서 클라이언트의 인덱스
	int clientIndexInServer = -1;

	// 게임오브젝트 배열에서 클라이언트가 조작하는 플레이어의 인덱스
	int playerGameObjectIndex = -1;

	//이전에 사용되었던 권총 메쉬. 
	//<지금은 사용을 안하지만, 나중에 사용이 가능해질거라서 총을 어떻게 할지 정하고 뭐 처리를 하던 해야함.>
	Mesh* GunMesh;
	// 이전에 사용되었던 권총메쉬의 텍스쳐
	GPUResource GunTexture;
	
	// 텍스쳐가 없을때 대체하기 위한 기본 디퓨즈 텍스쳐
	GPUResource DefaultTex;
	// 텍스쳐가 없을때 대체하기 위한 기본 노멀 텍스쳐
	GPUResource DefaultNoramlTex;
	// 텍스쳐가 없을때 대체하기 위한 기본 앰비언트 텍스쳐
	GPUResource DefaultAmbientTex;
	// 첫번째 머터리얼
	Material DefaultMaterial;

	// 아마 곧 쓰이지 않게될 라이트 데이터 CB 리소스 (쉐도우가 포함된 버전이 존재함.)
	GPUResource LightCBResource;
	LightCB_DATA* LightCBData;

	// 라이트 정보를 담고 있는 쉐도우 맵핑을 위한 CB 리소스
	GPUResource LightCB_withShadowResource[9];
	// Upload Buffer에 Mapping 된 cpu 메모리
	LightCB_DATA_withShadow* LightCBData_withShadow[9];

	// <수정 필요>
	// 쉐도우 맵을 만들어내는 객체. 셰이더라고 되어 있지만 실제 셰이더 역할을 하지는 않는다.
	// 나중에 기능을 따로 빼내어 정비해야 함.
	// fix

	//  DirectionLight 정보가 있다.
	SpotLight MyDirLight[3];

	// 라이트 정보를 초기화 한다.
	void SetLight();

	// NPC 채력바들의 월드 행렬의 배열.
	vecset<matrix> NpcHPBars;

	// sus <이 세개는 왜 이렇게 되어 있는지 원인 규명 필요>
	// 클라이언트 실행이 준비되었다는 것을 알리는 신호
	bool isPrepared = false;
	// 클라이언트 인덱스를 받아내었다는 신호
	bool isPreparedClientIndex = false;
	// 맵이 설정 완료되었는지 알리는 신호
	bool isMapInit = false;
	// 공용 에셋이 설정 완료되었는지 알리는 신호
	bool isGlobalAssetInit = false;

	// 플레이어 인벤토리 창이 열렸는지 여부
	bool isInventoryOpen = false;

	// 충돌체들의 상태를 보여주는지
	static constexpr bool DebugCollisions = false;

	bool isAssetAddingInGlobal = true;
	// 모든 텍스쳐들이 담겨있는 배열
	vector<GPUResource*> TextureTable;
	// 게임 전체에서 공용으로 쓰일 모든 텍스쳐들의 개수. 
	// 해당 인덱스 이후로는 현재 존에서만 쓰일 모든 텍스쳐들이 담겨있는 배열 영역이 있다.
	int GlobalTextureCount = 0;

	// 모든 머터리얼들이 담겨져 있는 배열
	vector<Material*> MaterialTable;
	// 게임 전체에서 공용으로 쓰일 모든 머터리얼들의 개수. 
	// 해당 인덱스 이후로는 현재 존에서만 쓰일 모든 머터리얼들이 담겨있는 배열 영역이 있다.
	int GlobalMaterialCount = 0;

	// 모든 단일메쉬들이 담겨져 있는 배열
	vector<Mesh*> MeshTable;
	// 게임 전체에서 공용으로 쓰일 모든 단일메쉬들의 개수. 
	// 해당 인덱스 이후로는 현재 존에서만 쓰일 모든 단일메쉬들이 담겨있는 배열 영역이 있다.
	int GlobalMeshCount = 0;

	// 모든 휴머노이드 애니메이션들이 담겨져 있는 배열
	vector<HumanoidAnimation> HumanoidAnimationTable;
	// 게임 전체에서 공용으로 쓰일 모든 휴머노이드 애니메이션들의 개수. 
	// 해당 인덱스 이후로는 현재 존에서만 쓰일 모든 휴머노이드 애니메이션들이 담겨있는 배열 영역이 있다.
	int GlobalHumanoidAnimationCount = 0;

	// 렌더에서 쓰일 모든 Immortal 텍스쳐들이 ShaderVisibleDescHeap의 순서대로 담겨있는 배열
	vector<GPUResource*> RenderTextureTable;
	// 렌더에서 쓰일 모든 Immortal 머터리얼들이 ShaderVisibleDescHeap의 순서대로 담겨있는 배열
	vector<Material*> RenderMaterialTable;
	// 렌더에서 쓰일 모든 Immortal 단일메쉬인스턴싱 에셋들이 ShaderVisibleDescHeap의 순서대로 담겨있는 배열
	vector<Mesh::InstancingStruct*> RenderInstancingTable;
	//// 렌더에서 쓰일 모든 Immortal 휴머노이드 애니메이션들이 ShaderVisibleDescHeap의 순서대로 담겨있는 배열
	//vector<HumanoidAnimation*> RenderHumanoidAnimationTable;

	int GetRenderMaterialIndexFromGlobalMaterialIndex(int globalMatIndex);

	// 현재 존에서의 포탈의 배열
	vector<Portal*> Portals;

	void AddMesh(Mesh* mesh);

	void GameTableArrangeMent();

	// 기본 텍스쳐 밉맵 레벨
	static constexpr int basicTexMip = 10;
	// 기본 텍스쳐 블럭압축 포맷
	static constexpr DXGI_FORMAT basicTexFormat = DXGI_FORMAT_BC3_UNORM;
	// 기본 텍스쳐 블럭압축 포맷 문자열 - dds 만들때 쓰인다.
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

	Game() {}
	~Game() {}
	/*
	* 설명 : 게임을 초기화 하는 함수.
	* 1.GameObjectType::STATICINIT(); 을 통해 각 GameObject 의 vptr을 얻어낸다.
	* 2.DropedItems, NpcHPBars, bulletRays 를 초기화 한다.
	* 3.커맨드리스트를 리셋하여 리소스들을 만들기 시작한다.
	* 3-1. 기본 텍스쳐들을 만든다.  Default~~~Tex
	* 3-2. 서버의 정보를 받아 업데이트 할 수 있도록 GlobalTextureArr 에 Tile, Wall, Monster 텍스쳐를 만든다.
	* 3-3. 맵을 로드한다
	* 3-4. SetLight함수로 라이트를 초기화 한다.
	* 3-5. 뷰포트의 투영행렬을 초기화 한다.
	* 3-6. Item 들의 Mesh를 로드하고 아이템 테이블에 아이템들을 담는다.
	* 3-7. 각종 셰이더를 초기화한다.
	* 3-8. 추가적으로 필요한 Shape들을 로드하고 저장한다.
	* 3-9. 커맨드리스트를 닫고 GPU에 실행시킨다.
	*/
	void Init();

#pragma region UIDefine
	//Global Variable
	// xy : 마우스 좌표값. DirectX Render Coord임. 
	vec4 CurrentCursorPos;
	// 현재 왼쪽클릭되어있는지
	bool LBtnDown = false;
	// 현재 오른쪽클릭되어있는지
	bool RBtnDown = false;
	// 최근에 IME에 의해 완성된 문자.
	wchar_t CurrentCompleteCharactor;
	// 최근에 눌린 Key
	WPARAM CurrentKeyDown;
	// 어떤 Key가 눌려있는지 표현하는 배열
	UCHAR pKeyBuffer[256];
	// 최근에 실행된 이벤트 값.
	DXEvent evt;
	// 모든 UI Page를 모아놓은 테이블
	vector<DXPage*> UIPageTable;
	// 현재 나타낼 UI Page들의 Stack
	vector<DXPage*> mainPageStack;

	vector<DXPage*>* CurrentPageStack = nullptr;
	vec4 CurrentUICenter = vec4(0, 0, 0, 0);
	static constexpr float ui_depth_epsilon = 0.0001f;
	static constexpr float uipage_depth_epsilon = 0.001f;
	static constexpr float uiwindow_depth_epsilon = 0.01f;

	// 윈도우 상대 정규화 좌표계 (0~1) y는 아래로 갈 수록 늘어남. / 에서 DirectX 렌더 좌표계로 바꾸는 함수
	// 일반 2d 점을 상대로 해도 되고, vec4로 이루어진 직사각형 영역을 연산해도 됨.
	void WindowNormalizeCoordToDirectXRenderCoord_vec4(vec4& v, float W, float H);
	// 어떤 영역에 어떤 점이 포함되는지 여부
	bool RectContainPos(vec4 rt, vec4 pos);
	// 어떤 영역에 어떤 영역이 포함되는지 여부
	bool RectContainRect(vec4 rt, vec4 rt2);
	// 텍스쳐 직사각형 영역으로 그리기 함수
	void UIDraw_TextureRect(vec4 loc, vec4 color, float depth, int uitextureid);
	// 텍스쳐 직선으로 그리기 함수
	void UIDraw_TextureLine(vec4 startToEnd, vec4 color, float depth, float LineWidth, int uitextureid);
	//UI 초기화 코드.
	void UI_Init();

	//UI Depthj 관련 코드
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

	// UI에 쓰일 텍스쳐들의 Table
	vector<GPUResource*> UITextureTable;

	DXUI* GetSlotUIFromPos(vec4 pos);
	SlotData CurrentGrabSlotData;

	static constexpr int inventorySlotCount = 49;
	DXUI* InventorySlots[inventorySlotCount] = {};

#pragma endregion

	// 기존 맵을 모두 해제한 후, 새로운 맵을 로드하는것.
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
	* 설명 : 게임을 렌더링 한다.
	*/
	void Render();

	void Render_RayTracing();

	/*
	* 설명 : 쉐도우 맵을 렌더링한다.
	*/
	void Render_ShadowPass();

	GPUResource DirLightRes;
	DirLightInfo* MappedDirLightData = nullptr;
	DescIndex DirLightResCBV;
	void InitDirLightGPURes();

	//-----------dynamic Global-----------
	// 현재 렌더링을 수행하는 viewport
	inline static ViewportData* renderViewPort;
	// 현재 렌더링을 수행하는 셰이더 타입
	inline static ShaderType PresentShaderType = ShaderType::RenderNormal;
	// 1인칭 여부
	bool bFirstPersonVision = false;
	// delta time
	float DeltaTime;

	UINT TourID = 0;
	// 렌더링을 할때 배치처리 렌더링 사용 여부
	bool SceneRenderBatch = false;
	// 렌더커맨드를 삽입할때 오브젝트의 렌더링 함수를 교체하는 bool 변수
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
	* 설명 : 게임을 업데이트 한다.
	*/
	void Update();
	
	/*
	* 설명 : 서버에서 받은 데이터를 해석해 클라이언트의 동기화를 한다.
	* 매개변수 : 
	* char* ptr : 받은 데이터의 시작주소
	* int totallen : 받은 데이터의 바이트 개수
	* 반환 : 
	* 현재 읽기를 완료한 바이트 수를 반환.
	*/
	int Receiving(char* ptr, int totallen = 0);

	/*
	* 설명 : 마우스 움직임이 일어났을때, DeltaMousePos에 적용시키는 함수.
	* 매개변수 : 
	* int deltaX : X 쪽으로 움직인 정도
	* int deltaY : Y 쪽으로 움직인 정도
	*/
	void AddMouseInput(int deltaX, int deltaY);
	/*
	* 설명 : 텍스트를 렌더링한다.
	* 매개변수 : 
	* const wchar_t* wstr : 출력할 문자열
	* int length : 문자열 길이
	* vec4 Rect : 문자열이 그려질 영역
	* float fontsiz : 텍스트 폰트 사이즈
	* float depth : 어떤 깊이값에서 텍스트가 렌더링 되는지 결정.
	*/
	void RenderText(const wchar_t* wstr, int length, vec4 Rect, float fontsiz, float depth = 0.01f);

	void RenderSDFText(const wchar_t* wstr, int length, vec4 Rect, float fontsiz, vec4 color, float* minD, float* maxD, float depth, vec4* SDFRectOut = nullptr);

	//Zone들의 LightStructuredBuffer를 담고 있는 배열
	DescIndex Immortal_ZoneLightBuffer_SRV[9] = {};

	// 빈도 제어
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
//template 함수 구현부
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
				if (pModel->mMeshes[Meshes[i]]->type == Mesh::MeshType::_BumpMesh) {
					BumpMesh* Bmesh = (BumpMesh*)((BumpMesh*)pModel->mMeshes[Meshes[i]]);
					for (int k = 0; k < Bmesh->subMeshNum; ++k) {
						using PBRRPI = PBRShader1::RootParamId;
						Material* mat = game.MaterialTable[materialIndex[k]];
						cmd->SetGraphicsRootDescriptorTable(PBRRPI::SRVTable_MaterialTextures, mat->TextureSRVTableIndex.hRender.hgpu);
						cmd->SetGraphicsRootDescriptorTable(PBRRPI::CBVTable_Material, mat->CB_Resource.descindex.hRender.hgpu);
						pModel->mMeshes[Meshes[i]]->Render(cmd, 1, k);
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
						UINT ncbElementBytes = (((sizeof(matrix) * 128) + 255) & ~255); //256의 배수
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
#include "stdafx.h"
#include "main.h"
#include "Game.h"
#include "Render.h"
#include "GameObject.h"
#include "NetworkDefs.h"

extern int dbgc[128];
void dbgbreak(bool condition) {
	if (condition) __debugbreak();
}

template <typename T> void* GetVptr() {
	T a;
	return *(void**)&a;
}

#define SERVER_RELEASE
#ifndef SERVER_RELEASE
#define SERVER_DEBUG
#endif

extern GlobalDevice gd;
Game game;

void* GameObjectType::vptr[GameObjectType::ObjectTypeCount];
vector<STCMemberInfo> GameObjectType::Server_STCMembers[GameObjectType::ObjectTypeCount];
vector<STCMemberInfo> GameObjectType::Client_STCMembers[GameObjectType::ObjectTypeCount];
unordered_map<int, SyncWay> GameObjectType::STC_OffsetMap;

// 싱크되는 변수의 서버이름과 클라이언트 이름이 다른 경우 연결을 위해 사용.
void GameObjectType::LinkOffsetByName(short type, const char* ServerVarName, const char* ClientVarName) {
	for (int k = 0;k < Server_STCMembers[type].size();++k) {
		STCMemberInfo& minfo = Server_STCMembers[type][k];
		if (strcmp(minfo.name, ServerVarName) == 0) {
			for (int u = 0;u < Client_STCMembers[type].size();++u) {
				STCMemberInfo& cminfo = Client_STCMembers[type][u];
				if (strcmp(cminfo.name, ClientVarName) == 0) {
					STC_OffsetMap.insert(pair<int, SyncWay>(minfo.offset, SyncWay(cminfo.offset)));
					return;
				}
			}
		}
	}
}

void GameObjectType::LinkOffsetAsFunction(short type, const char* ServerVarName, void (*func)(GameObject*, char*, int))
{
	for (int k = 0;k < Server_STCMembers[type].size();++k) {
		STCMemberInfo& minfo = Server_STCMembers[type][k];
		if (strcmp(minfo.name, ServerVarName) == 0) {
			STC_OffsetMap.insert(pair<int, SyncWay>(minfo.offset, SyncWay(func)));
		}
	}
}

void GameObjectType::STATICINIT() {
	vptr[GameObjectType::_GameObject] = GetVptr<GameObject>();
	vptr[GameObjectType::_StaticGameObject] = GetVptr<StaticGameObject>();
	vptr[GameObjectType::_DynamicGameObject] = GetVptr<DynamicGameObject>();
	vptr[GameObjectType::_SkinMeshGameObject] = GetVptr<SkinMeshGameObject>();
	vptr[GameObjectType::_Player] = GetVptr<Player>();
	vptr[GameObjectType::_Monster] = GetVptr<Monster>();

	//서버의 오프셋을 받는다.
	ifstream ifs{ "STC_GameObjectOffsets.txt" };
	for (int k = 0;k < ObjectTypeCount;++k) {
		int n;
		ifs >> n;
		for (int i = 0;i < n;++i) {
			STCMemberInfo stcmi;
			string str;
			ifs >> str;
			stcmi.name = new char[str.size() + 1];
			strcpy_s((char*)stcmi.name, str.size()+1, str.c_str());
			ifs >> stcmi.offset;
			ifs >> stcmi.size;
			Server_STCMembers[k].push_back(stcmi);
		}
	}

	// 클라이언트의 오프셋을 받는다.
	GameObject::STATICINIT();
	StaticGameObject::STATICINIT();
	DynamicGameObject::STATICINIT();
	SkinMeshGameObject::STATICINIT();
	Player::STATICINIT();
	Monster::STATICINIT();

	//이름이 같은 것 끼리 링크한다.
	for (int i = 0;i < ObjectTypeCount;++i) {
		for (int k = 0;k < Server_STCMembers[i].size();++k) {
			STCMemberInfo& minfo = Server_STCMembers[i][k];
			for (int u = 0;u < Client_STCMembers[i].size();++u) {
				STCMemberInfo& cminfo = Client_STCMembers[i][u];
				if (strcmp(minfo.name, cminfo.name) == 0) {
					STC_OffsetMap.insert(pair<int, SyncWay>(minfo.offset, SyncWay(cminfo.offset)));
					break;
				}
			}
		}
	}

	LinkOffsetByName(GameObjectType::_SkinMeshGameObject, "AnimationFlowTime", "DestAnimationFlowTime");
	LinkOffsetByName(GameObjectType::_Player, "AnimationFlowTime", "DestAnimationFlowTime");
	LinkOffsetByName(GameObjectType::_Monster, "AnimationFlowTime", "DestAnimationFlowTime");

	LinkOffsetAsFunction(GameObjectType::_DynamicGameObject, "worldMat", DynamicGameObject::SyncDestWolrd);
	LinkOffsetAsFunction(GameObjectType::_SkinMeshGameObject, "worldMat", DynamicGameObject::SyncDestWolrd);
	LinkOffsetAsFunction(GameObjectType::_Player, "worldMat", DynamicGameObject::SyncDestWolrd);
	LinkOffsetAsFunction(GameObjectType::_Monster, "worldMat", DynamicGameObject::SyncDestWolrd);
}

void Game::SetLight()
{
	LightCBData = new LightCB_DATA();
	UINT ncbElementBytes = ((sizeof(LightCB_DATA) + 255) & ~255); //256의 배수
	LightCBResource = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1);
	LightCBResource.resource->Map(0, NULL, (void**)&LightCBData);
	LightCBData->dirlight.gLightColor = { 0.5f, 0.5f, 0 };
	LightCBData->dirlight.gLightDirection = { 1, -1, 1 };
	for (int i = 0; i < 8; ++i) {
		PointLight& p = LightCBData->pointLights[i];
		p.LightColor = { 1, 1, 1 };
		p.LightIntencity = 100;
		p.LightRange = 120;
		p.LightPos = { (float)(rand() % 80 - 40), 1, (float)(rand() % 80 - 40) };
	}
	LightCBData->pointLights[0].LightPos = { 0, 0, 0 };
	//LightCBData->pointLights[1].LightPos = { -5, 1, 5 };
	//LightCBData->pointLights[2].LightPos = { 5, 1, 5 };
	//LightCBData->pointLights[3].LightPos = { 5, 1, -5 };
	LightCBResource.resource->Unmap(0, nullptr);

	//LightCBData_withShadow = new LightCB_DATA_withShadow();
	ncbElementBytes = ((sizeof(LightCB_DATA) + 255) & ~255); //256의 배수
	LightCB_withShadowResource = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1);
	LightCB_withShadowResource.resource->Map(0, NULL, (void**)&LightCBData_withShadow);
	LightCBData_withShadow->dirlight.gLightColor = { 0.5f, 0.5f, 0.5f };
	vec4 dir = vec4(1, -2, 1, 0);
	dir.len3 = 1;
	LightCBData_withShadow->dirlight.gLightDirection = { dir.x, dir.y, dir.z };
	for (int i = 0; i < 8; ++i) {
		PointLight& p = LightCBData_withShadow->pointLights[i];
		p.LightPos = { (float)(rand() % 40 - 20), 1, (float)(rand() % 40 - 20) };
		p.LightIntencity = 20;
		p.LightColor = { 1, 1, 1 };
		p.LightRange = 50;
	}
	LightCBData_withShadow->LightProjection = gd.viewportArr[0].ProjectMatrix;
	LightCBData_withShadow->LightView = MySpotLight.View;
	LightCBData_withShadow->LightPos = MySpotLight.LightPos.f3;
	LightCBData_withShadow->pointLights[0].LightPos = { 10, 0, 0 };
	LightCB_withShadowResource.resource->Unmap(0, nullptr);
}

void Game::AddMesh(Mesh* mesh)
{
	MeshTable.push_back(mesh);
}

GameObjectIncludeChunks Game::GetChunks_Include_OBB(BoundingOrientedBox obb)
{
	GameObjectIncludeChunks ret;
	XMFLOAT3 corners[BoundingOrientedBox::CORNER_COUNT];
	obb.GetCorners(corners);

	vec4 c[8];
	c[0].f3 = corners[0];
	vec4 minpos = c[0];
	vec4 maxpos = c[0];
	for (int i = 1; i < 8; ++i) {
		c[i].f3 = corners[i];
		minpos = _mm_min_ps(c[i], minpos);
		maxpos = _mm_max_ps(c[i], maxpos);
	}

	ret.xmin = floor(minpos.x / chunck_divide_Width);
	ret.xlen = floor(maxpos.x / chunck_divide_Width) - ret.xmin;
	ret.ymin = floor(minpos.y / chunck_divide_Width);
	ret.ylen = floor(maxpos.y / chunck_divide_Width) - ret.ymin;
	ret.zmin = floor(minpos.z / chunck_divide_Width);
	ret.zlen = floor(maxpos.z / chunck_divide_Width) - ret.zmin;
	return ret;
}

GameChunk* Game::GetChunkFromPos(vec4 pos) {
	int ix = floor(pos.x / chunck_divide_Width);
	int iy = floor(pos.y / chunck_divide_Width);
	int iz = floor(pos.z / chunck_divide_Width);
	ChunkIndex ci = ChunkIndex(ix, iy, iz);
	auto gc = chunck.find(ci);
	if (gc != chunck.end()) {
		return gc->second;
	}
	else {
		return nullptr;
	}
}

void Game::PushGameObject(GameObject* go)
{
	if (GameObject::IsType<StaticGameObject>(go)) {
		// static game object
		StaticGameObject* sgo = (StaticGameObject*)go;
		GameObjectIncludeChunks chunkIds = GetChunks_Include_OBB(sgo->GetOBB());
		int xmax = chunkIds.xmin + chunkIds.xlen;
		int ymax = chunkIds.ymin + chunkIds.ylen;
		int zmax = chunkIds.zmin + chunkIds.zlen;
		bool pushing = false;
		for (int ix = chunkIds.xmin; ix <= xmax; ++ix) {
			for (int iy = chunkIds.ymin; iy <= ymax; ++iy) {
				for (int iz = chunkIds.zmin; iz <= zmax; ++iz) {
					auto c = chunck.find(ChunkIndex(ix, iy, iz));
					GameChunk* gc;
					if (c == chunck.end()) {
						// new game chunk
						gc = new GameChunk();
						gc->SetChunkIndex(ChunkIndex(ix, iy, iz));
						chunck.insert(pair<ChunkIndex, GameChunk*>(ChunkIndex(ix, iy, iz), gc));
					}
					else gc = c->second;

					int allocN = gc->Static_gameobjects.Alloc();
					gc->Static_gameobjects[allocN] = sgo;
					pushing = true;
				}
			}
		}
	}
	else {
		if (GameObject::IsType<SkinMeshGameObject>(go)) {
			// dynamic game object
			SkinMeshGameObject* smgo = (SkinMeshGameObject*)go;
			smgo->InitialChunkSetting();
			GameObjectIncludeChunks chunkIds = GetChunks_Include_OBB(go->GetOBB());
			int xmax = chunkIds.xmin + chunkIds.xlen;
			int ymax = chunkIds.ymin + chunkIds.ylen;
			int zmax = chunkIds.zmin + chunkIds.zlen;
			int up = 0;
			for (int ix = chunkIds.xmin; ix <= xmax; ++ix) {
				for (int iy = chunkIds.ymin; iy <= ymax; ++iy) {
					for (int iz = chunkIds.zmin; iz <= zmax; ++iz) {
						auto c = chunck.find(ChunkIndex(ix, iy, iz));
						GameChunk* gc;
						if (c == chunck.end()) {
							// new game chunk
							gc = new GameChunk();
							gc->SetChunkIndex(ChunkIndex(ix, iy, iz));
							chunck.insert(pair<ChunkIndex, GameChunk*>(ChunkIndex(ix, iy, iz), gc));
						}
						else gc = c->second;
						int allocN = gc->SkinMesh_gameobjects.Alloc();
						gc->SkinMesh_gameobjects[allocN] = smgo;
						smgo->chunkAllocIndexs[up] = allocN;
						up += 1;
					}
				}
			}
		}
		else {
			// dynamic game object
			DynamicGameObject* dgo = (DynamicGameObject*)go;
			dgo->InitialChunkSetting();
			GameObjectIncludeChunks chunkIds = GetChunks_Include_OBB(go->GetOBB());
			int xmax = chunkIds.xmin + chunkIds.xlen;
			int ymax = chunkIds.ymin + chunkIds.ylen;
			int zmax = chunkIds.zmin + chunkIds.zlen;
			int up = 0;
			for (int ix = chunkIds.xmin; ix <= xmax; ++ix) {
				for (int iy = chunkIds.ymin; iy <= ymax; ++iy) {
					for (int iz = chunkIds.zmin; iz <= zmax; ++iz) {
						auto c = chunck.find(ChunkIndex(ix, iy, iz));
						GameChunk* gc;
						if (c == chunck.end()) {
							// new game chunk
							gc = new GameChunk();
							gc->SetChunkIndex(ChunkIndex(ix, iy, iz));
							chunck.insert(pair<ChunkIndex, GameChunk*>(ChunkIndex(ix, iy, iz), gc));
						}
						else gc = c->second;
						int allocN = gc->Dynamic_gameobjects.Alloc();
						gc->Dynamic_gameobjects[allocN] = dgo;
						dgo->chunkAllocIndexs[up] = allocN;
						up += 1;
					}
				}
			}
		}
	}
}

void Game::PushLight(Light* light)
{
}

void Game::InitParticlePool(ParticlePool& pool, UINT count)
{
	pool.Count = count;

	std::vector<Particle> init(count);
	for (UINT i = 0; i < count; ++i)
	{
		init[i].Life = 0.0f;
		init[i].Age = 0.0f;
		init[i].Size = 0.02f;
	}

	
	pool.Buffer = gd.CreateCommitedGPUBuffer(                 
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_DIMENSION_BUFFER,
		sizeof(Particle) * count,
		1,
		DXGI_FORMAT_UNKNOWN,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
	);

	
	GPUResource upload = gd.CreateCommitedGPUBuffer(
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		D3D12_RESOURCE_DIMENSION_BUFFER,
		sizeof(Particle) * count,
		1,
		DXGI_FORMAT_UNKNOWN
	);

	gd.UploadToCommitedGPUBuffer(
		init.data(),
		&upload,
		&pool.Buffer,
		true
	);
}

void Game::Init()
{
	GameObjectType::STATICINIT();

	DropedItems.reserve(4096);
	NpcHPBars.Init(32);
	bulletRays.Init(32);

	gd.gpucmd.Reset();

	MyShader = new Shader();
	MyShader->InitShader();

	MyOnlyColorShader = new OnlyColorShader();
	MyOnlyColorShader->InitShader();

	MyScreenCharactorShader = new ScreenCharactorShader();
	MyScreenCharactorShader->InitShader();

	MyPBRShader1 = new PBRShader1();
	MyPBRShader1->InitShader();

	MySkyBoxShader = new SkyBoxShader();
	MySkyBoxShader->InitShader();
	MySkyBoxShader->LoadSkyBox(L"Resources/GlobalTexture/SkyBox_0.dds");

	MyRayTracingShader = new RayTracingShader();
	MyRayTracingShader->Init();

	DefaultTex.CreateTexture_fromFile(L"Resources/DefaultTexture.png", game.basicTexFormat, game.basicTexMip);
	DefaultNoramlTex.CreateTexture_fromFile(L"Resources/GlobalTexture/DefaultNormalTexture.png", basicTexFormat, basicTexMip);
	DefaultAmbientTex.CreateTexture_fromFile(L"Resources/GlobalTexture/DefaultAmbientTexture.png", basicTexFormat, basicTexMip);

	GunTexture.CreateTexture_fromFile(L"Resources/m1911pistol_diffuse0.png", game.basicTexFormat, game.basicTexMip);

	// particle texture
	FireTextureRes.CreateTexture_fromFile(L"Resources/fire.jpg", DXGI_FORMAT_UNKNOWN, 1, true);

	gd.GlobalTextureArr[(int)gd.GlobalDevice::GT_TileTex].CreateTexture_fromFile(L"Resources/Tiles036_1K-PNG_Color.png", game.basicTexFormat, game.basicTexMip);
	game.TextureTable.push_back(&gd.GlobalTextureArr[(int)gd.GlobalDevice::GT_TileTex]);
	Material TileMat;
	TileMat.ti.Diffuse = game.TextureTable.size() - 1;
	TileMat.SetDescTable(); // (int)gd.GlobalDevice::GM_TileTex
	game.MaterialTable.push_back(TileMat);
	
	gd.GlobalTextureArr[(int)gd.GlobalDevice::GT_WallTex].CreateTexture_fromFile(L"Resources/Chip001_1K-PNG_Color.png", game.basicTexFormat, game.basicTexMip);
	game.TextureTable.push_back(&gd.GlobalTextureArr[(int)gd.GlobalDevice::GT_WallTex]);
	Material WallMat;
	WallMat.ti.Diffuse = game.TextureTable.size() - 1;
	WallMat.SetDescTable();
	game.MaterialTable.push_back(WallMat);

	gd.GlobalTextureArr[(int)gd.GlobalDevice::GT_Monster].CreateTexture_fromFile(L"Resources/PaintedMetal004_1K-PNG_Color.png", game.basicTexFormat, game.basicTexMip);
	game.TextureTable.push_back(&gd.GlobalTextureArr[(int)gd.GlobalDevice::GT_Monster]);
	Material MonsterMat;
	MonsterMat.ti.Diffuse = game.TextureTable.size() - 1;
	MonsterMat.SetDescTable();
	game.MaterialTable.push_back(MonsterMat);

	Map = new GameMap();
	Map->LoadMap("The_Port");

	SetLight();

	gd.viewportArr[0].ProjectMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), (float)gd.ClientFrameWidth / (float)gd.ClientFrameHeight, 0.01f, 1000.0f);

	//gd.gpucmd->Reset(gd.pCommandAllocator, NULL);

	//
	TextMesh = new UVMesh();
	TextMesh->CreateTextRectMesh();

	BumpMesh* ItemMesh = new BumpMesh();
	ItemMesh->ReadMeshFromFile_OBJ("Resources/Mesh/BulletMag001.obj", vec4(1, 1, 1, 1), true);
	ItemTable.push_back(Item(0, vec4(0, 0, 0, 0), nullptr, nullptr, L"")); // blank space in inventory.
	ItemTable.push_back(Item(1, vec4(1, 0, 0, 1), ItemMesh, &gd.GlobalTextureArr[(int)gd.GlobalDevice::GT_Monster], L"[빨간 탄알집]"));
	ItemTable.push_back(Item(2, vec4(0, 1, 0, 1), ItemMesh, &gd.GlobalTextureArr[(int)gd.GlobalDevice::GT_WallTex], L"[녹색 탄알집]"));
	ItemTable.push_back(Item(3, vec4(0, 0, 1, 1), ItemMesh, &DefaultTex, L"[하얀 탄알집]")); // test items. red, green, blue bullet mags.

	

	BumpMesh* MyMesh = (BumpMesh*)new BumpMesh();
	MyMesh->ReadMeshFromFile_OBJ("Resources/Mesh/PlayerMesh.obj", { 1, 1, 1, 1 });
	Shape::AddMesh("Player", MyMesh);

	BumpMesh* MyGroundMesh = (BumpMesh*)new BumpMesh();
	MyGroundMesh->CreateWallMesh(40.0f, 0.5f, 40.0f, { 1, 1, 1, 1 });
	Shape::AddMesh("Ground001", MyGroundMesh);

	BumpMesh* MyWallMesh = (BumpMesh*)new BumpMesh();
	MyWallMesh->CreateWallMesh(5.0f, 2.0f, 1.0f, { 1, 1, 1, 1 });
	Shape::AddMesh("Wall001", MyWallMesh);

	// LOD Sphere Mesh 생성
	Mesh* SphereHigh = new Mesh();
	SphereHigh->CreateSphereMesh(gd.gpucmd, 1.5f, 10, 5, vec4(1, 0, 0, 1)); // 100 triangles
	Shape::AddMesh("SphereHigh100", SphereHigh);

	Mesh* SphereLow = new Mesh();
	SphereLow->CreateSphereMesh(gd.gpucmd, 1.5f, 5, 2, vec4(0, 1, 0, 1));   // 20 triangles
	Shape::AddMesh("SphereLow20", SphereLow);

	//LOD Sphere Object 생성
	SphereLODObject* lodSphere = new SphereLODObject();
	lodSphere->MeshNear = SphereHigh;
	lodSphere->MeshFar = SphereLow;
	/*lodSphere->m_pShader = MyShader;
	lodSphere->MaterialIndex = 0;*/
	lodSphere->FixedPos = vec4(5, 5, 0, 1);
	lodSphere->worldMat = (XMMatrixTranslation(5.0f, 5.0f, 0.0f));
	lodSphere->SwitchDistance = 20.0f;

	game.DynmaicGameObjects.push_back(lodSphere);


	BulletRay::mesh = (Mesh*)new Mesh();
	BulletRay::mesh->ReadMeshFromFile_OBJ("Resources/Mesh/RayMesh.obj", { 1, 1, 0, 1 }, false);

	BumpMesh* MyMonsterMesh = (BumpMesh*)new BumpMesh();
	MyMonsterMesh->ReadMeshFromFile_OBJ("Resources/Mesh/PlayerMesh.obj", { 1, 0, 0, 1 });
	Shape::AddMesh("Monster001", MyMonsterMesh);

	//game.GunModel = new Model;
	//game.GunModel->LoadModelFile("Resources/Model/sniper.model");
	//game.GunModel->DebugPrintHierarchy(game.GunModel->RootNode);
	
	// 스나이퍼 모델 로드
	/*game.SniperModel = new Model;
	game.SniperModel->LoadModelFile2("Resources/Model/sniper.model");*/

	// 라이플 모델 로드
	/*game.RifleModel = new Model;
	game.RifleModel->LoadModelFile2("Resources/Model/Rifle.model");*/

	// 권총 모델 로드
	/*game.PistolModel = new Model;
	game.PistolModel->LoadModelFile2("Resources/Model/pistol.model");
	game.PistolModel->DebugPrintHierarchy(game.PistolModel->RootNode);*/

	/*game.Pistol_SlideIndices.clear();
	int upperIdx = game.PistolModel->FindNodeIndexByName("Upper_Part");
	if (upperIdx >= 0) {
		game.Pistol_SlideIndices.push_back(upperIdx);
		game.PistolModel->BindPose[upperIdx] = game.PistolModel->Nodes[upperIdx].transform;
	}*/

	// 샷건 모델 로드
	/*game.ShotGunModel = new Model;
	game.ShotGunModel->LoadModelFile2("Resources/Model/shootgun.model");*/
	//game.ShotGunModel->DebugPrintHierarchy(game.ShotGunModel->RootNode);

	//game.SG_PumpIndices.clear();
	//int pumpIdx = game.ShotGunModel->FindNodeIndexByName("handguard_low");
	//if (pumpIdx >= 0) {
	//	game.SG_PumpIndices.push_back(pumpIdx);
	//	game.ShotGunModel->BindPose[pumpIdx] = game.ShotGunModel->Nodes[pumpIdx].transform;
	//}

	//// 머신건(미니건) 모델 로드
	//game.MachineGunModel = new Model;
	//game.MachineGunModel->LoadModelFile2("Resources/Model/minigun.model");

	/*game.MG_BarrelIndices.clear();
	auto addBarrel = [&](const char* name) {
		int idx = game.MachineGunModel->FindNodeIndexByName(name);
		if (idx >= 0) game.MG_BarrelIndices.push_back(idx);
	};

	addBarrel("Cylinder.107");
	addBarrel("Cylinder.108");
	addBarrel("Cylinder.109");
	addBarrel("Cylinder.110");*/

	//game.GunModel = game.SniperModel;
	game.GunModel = nullptr;

	game.HPBarMesh = new Mesh();
	game.HPBarMesh->ReadMeshFromFile_OBJ("Resources/Mesh/RayMesh.obj", { 0, 1, 0, 1 }, false);
	//Shape::AddMesh("HPBar", HPBarMesh);

	game.HeatBarMesh = new Mesh();
	game.HeatBarMesh->ReadMeshFromFile_OBJ("Resources/Mesh/RayMesh.obj", { 1, 0, 0, 1 }, false);
	//Mesh::meshmap.insert(pair<string, Mesh*>("HeatBar", HeatBarMesh));

	game.ShootPointMesh = new Mesh();
	game.ShootPointMesh->CreateWallMesh(0.05f, 0.05f, 0.05f, { 1, 1, 1, 0.5f });
	//Mesh::meshmap.insert(pair<string, Mesh*>("ShootPoint", ShootPointMesh));

	MySpotLight.ShadowMap = gd.CreateShadowMap(4096/*gd.ClientFrameWidth*/, 4096/*gd.ClientFrameHeight*/, 0, MySpotLight);
	MySpotLight.View.mat = XMMatrixLookAtLH(vec4(0, 2, 5, 0), vec4(0, 0, 0, 0), vec4(0, 1, 0, 0));

	// particle init
	InitParticlePool(FirePool, FIRE_COUNT);
	InitParticlePool(FirePillarPool, FIRE_PILLAR_COUNT);
	InitParticlePool(FireRingPool, FIRE_RING_COUNT);

	FireCS = new ParticleCompute();
	FireCS->Init(L"Particle.hlsl", "FireCS");

	FirePillarCS = new ParticleCompute();
	FirePillarCS->Init(L"Particle.hlsl", "FirePillarCS");

	FireRingCS = new ParticleCompute();
	FireRingCS->Init(L"Particle.hlsl", "FireRingCS");

	ParticleDraw = new ParticleShader();
	ParticleDraw->InitShader();

	ParticleDraw->FireTexture = &FireTextureRes;

	gd.gpucmd.Close();
	gd.gpucmd.Execute();
	gd.WaitGPUComplete();
}

void Game::Render() {

	gd.viewportArr[0].UpdateFrustum();

	if (isPrepared == false) return;

	//load text texture
	if (gd.addTextureStack.size() > 0) {
		for (int i = 0; i < gd.addTextureStack.size(); ++i) {
			gd.AddTextTexture(gd.addTextureStack[i]);
		}
	}
	gd.addTextureStack.clear();

	gd.ShaderVisibleDescPool.DynamicReset();

	Render_ShadowPass();

	gd.gpucmd.Reset();

	game.MySpotLight.ShadowMap.AddResourceBarrierTransitoinToCommand(gd.gpucmd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	//Wait Finish Present
	D3D12_RESOURCE_BARRIER d3dResourceBarrier;
	::ZeroMemory(&d3dResourceBarrier, sizeof(D3D12_RESOURCE_BARRIER));
	d3dResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	d3dResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	d3dResourceBarrier.Transition.pResource =
		gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex];
	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	gd.gpucmd->ResourceBarrier(1, &d3dResourceBarrier);

	gd.gpucmd->RSSetViewports(1, &gd.viewportArr[0].Viewport);
	gd.gpucmd->RSSetScissorRects(1, &gd.viewportArr[0].ScissorRect);

	//Clear Render Target Command Addition
	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle =
		gd.pRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	d3dRtvCPUDescriptorHandle.ptr += (gd.CurrentSwapChainBufferIndex *
		gd.RtvDescriptorIncrementSize);
	float pfClearColor[4] = { 0.05f, 0.05f, 0.05f, 1.0f };
	gd.gpucmd->ClearRenderTargetView(d3dRtvCPUDescriptorHandle,
		pfClearColor, 0, NULL);

	//Clear Depth Stencil Buffer Command Addtion
	D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle =
		gd.pDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	gd.gpucmd->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);
	gd.gpucmd->OMSetRenderTargets(1, &d3dRtvCPUDescriptorHandle, TRUE,
		&d3dDsvCPUDescriptorHandle);
	//render begin ----------------------------------------------------------------

	MySkyBoxShader->RenderSkyBox();
	gd.gpucmd->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);

	game.MyPBRShader1->Add_RegisterShaderCommand(gd.gpucmd, ShaderType::RenderWithShadow);

	matrix view = gd.viewportArr[0].ViewMatrix;
	view *= gd.viewportArr[0].ProjectMatrix;
	view.transpose();
	gd.gpucmd->SetGraphicsRoot32BitConstants(0, 16, &view, 0);
	gd.gpucmd->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16);
	gd.gpucmd->SetGraphicsRootConstantBufferView(2, game.LightCB_withShadowResource.resource->GetGPUVirtualAddress());
	gd.gpucmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
	gd.gpucmd->SetGraphicsRootDescriptorTable(PBRShader1::SRVTable_ShadowMap, game.MySpotLight.descindex.hRender.hgpu);

	//render Objects
	for (int i = 0; i < DynmaicGameObjects.size(); ++i) {
		if (DynmaicGameObjects[i] == nullptr || DynmaicGameObjects[i]->tag[GameObjectTag::Tag_Enable] == false) continue;
		DynmaicGameObjects[i]->Render();
	}

	//Render Items
	// already droped items. (non move..)
	
	// Diffuse 셰이더를 없앴음. 그래서 다른 대채 방안 필요
	//static float itemRotate = 0;
	//itemRotate += DeltaTime;
	//matrix mat;
	//mat.trQ(vec4::getQ(vec4(0, itemRotate, 0, 0)));
	//for (int i = 0; i < DropedItems.size(); ++i) {
	//	if (DropedItems[i].itemDrop.id != 0) {
	//		mat.pos = DropedItems[i].pos;
	//		mat.pos.w = 1;
	//		matrix rmat = XMMatrixTranspose(mat);
	//		gd.gpucmd->SetGraphicsRoot32BitConstants(1, 16, &rmat, 0);
	//		MyDiffuseTextureShader->SetTextureCommand(ItemTable[DropedItems[i].itemDrop.id].tex);
	//		ItemTable[DropedItems[i].itemDrop.id].MeshInInventory->Render(gd.gpucmd, 1);
	//	}
	//}

	// Particle Render
	FireCS->Dispatch(gd.gpucmd, &FirePool.Buffer, FirePool.Count, DeltaTime);
	ParticleDraw->Render(gd.gpucmd, &FirePool.Buffer, FirePool.Count);

	FirePillarCS->Dispatch(gd.gpucmd, &FirePillarPool.Buffer, FirePillarPool.Count, DeltaTime);
	ParticleDraw->Render(gd.gpucmd, &FirePillarPool.Buffer, FirePillarPool.Count);

	FireRingCS->Dispatch(gd.gpucmd, &FireRingPool.Buffer, FireRingPool.Count, DeltaTime);
	ParticleDraw->Render(gd.gpucmd, &FireRingPool.Buffer, FireRingPool.Count);

	//////////////////

	((Shader*)MyOnlyColorShader)->Add_RegisterShaderCommand(gd.gpucmd);

	matrix viewMat = gd.viewportArr[0].ViewMatrix;
	viewMat *= gd.viewportArr[0].ProjectMatrix;
	viewMat.transpose();
	gd.gpucmd->SetGraphicsRoot32BitConstants(0, 16, &viewMat, 0);

	//gd.gpucmd->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16);

	for (int i = 0; i < bulletRays.size; ++i) {
		if (bulletRays.isnull(i)) continue;
		bulletRays[i].Render();
	}

	for (int i = 0; i < game.NpcHPBars.size; ++i)
	{
		if (game.NpcHPBars.isAlloc(i))
		{
			matrix& hpBarWorldMat = game.NpcHPBars[i];
			hpBarWorldMat.transpose();
			gd.gpucmd->SetGraphicsRoot32BitConstants(1, 16, &hpBarWorldMat, 0);
			game.HPBarMesh->Render(gd.gpucmd, 1);
		}
	}

	gd.gpucmd->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);
	//((Shader*)MyShader)->Add_RegisterShaderCommand(gd.gpucmd);

	float hhpp = 0;
	float HeatGauge = 0;
	int kill = 0;
	int death = 0;
	int bulletCount = 0;
	float HealSkillCooldownFlow = 0;

	// UI/AfterDepthClear Render
	if (game.player != nullptr) {
		game.player->Render_AfterDepthClear();
		hhpp = game.player->HP;
		HeatGauge = game.player->HeatGauge;
		kill = game.player->KillCount;
		death = game.player->DeathCount;
		bulletCount = game.player->bullets;
		HealSkillCooldownFlow = game.player->HealSkillCooldownFlow;

	}

	// HP 
	//maybe..1706x960
	float Rate = gd.ClientFrameHeight / 960.0f;
	vec4 rt = Rate * vec4(-1650, 850, -1000, 700);
	((Shader*)MyScreenCharactorShader)->Add_RegisterShaderCommand(gd.gpucmd);
	std::wstring ui_hp = L"HP: " + std::to_wstring(hhpp);
	RenderText(ui_hp.c_str(), ui_hp.length(), rt, 30);

	//Heat Gauge
	vec4 rt_heat = rt;
	rt_heat.y -= 80 * Rate;   // HP보다 약간 아래로 내림 (간격 50 정도)
	std::wstring ui_heat = L"Heat: " + std::to_wstring((int)HeatGauge);
	RenderText(ui_heat.c_str(), ui_heat.length(), rt_heat, 30);

	//Skill
		rt = Rate * vec4(-900, 850, -200, 700);
	std::wstring ui_cool = L"[Q] Heal CD: " + std::to_wstring((int)HealSkillCooldownFlow);
	RenderText(ui_cool.c_str(), ui_cool.length(), rt, 30);


	// Bullet
	rt = Rate * vec4(900, -800, 1550, -900);
	std::wstring ui_bullet = L"Bullet: " + std::to_wstring(bulletCount);
	RenderText(ui_bullet.c_str(), ui_bullet.length(), rt, 30);

	// Kill/Death Counter
	rt = Rate * vec4(1100, 920, 1550, 700);
	std::wstring ui_kd = std::to_wstring(kill) + L" / " + std::to_wstring(death);
	RenderText(L"Kill/Death", 10, rt, 30);
	rt = Rate * vec4(1190, 850, 1550, 600);
	RenderText(ui_kd.c_str(), ui_kd.length(), rt, 30);

	// Player name
	rt = Rate * vec4(-1650, 700, -1000, 500);
	std::wstring playerName = L"Player: Leo";
	RenderText(playerName.c_str(), playerName.length(), rt, 30);

	// ----------Inventory------------
	if (isInventoryOpen) {
		matrix orthoMatrix = XMMatrixOrthographicOffCenterLH(0.0f, (float)gd.ClientFrameWidth, (float)gd.ClientFrameHeight, 0.0f, 0.01f, 1.0f);
		matrix uiViewMat = orthoMatrix;
		uiViewMat.transpose();
		((Shader*)MyScreenCharactorShader)->Add_RegisterShaderCommand(gd.gpucmd);
		MyScreenCharactorShader->SetTextureCommand(&DefaultTex);
		/*gd.gpucmd->SetPipelineState(MyOnlyColorShader->pUiPipelineState);*/
		//gd.gpucmd->SetGraphicsRoot32BitConstants(0, 16, &uiViewMat, 0);

		const int gridColumns = 5;
		const int gridRows = 5;
		const float slotSize = Rate * 150.0f;
		const float itemPadding = Rate * 20.0f;
		const float slotPadding = Rate * 10.0f;

		const float actualItemSize = slotSize - (itemPadding * 2);

		float invWidth = (gridColumns * slotSize) + ((gridColumns + 1) * slotPadding);
		float invHeight = (gridRows * slotSize) + ((gridRows + 1) * slotPadding);
		float invPosX = 0;
		float invPosY = 0;

		vec4 bgColor = { 0.2f, 0.2f, 0.2f, 0.8f };
		float CB[11] = { invPosX - invWidth ,invPosY - invHeight , invPosX + invWidth ,invPosY + invHeight, bgColor.r, bgColor.g, bgColor.b, bgColor.a, gd.ClientFrameWidth, gd.ClientFrameHeight, 0.5f };
		gd.gpucmd->SetGraphicsRoot32BitConstants(0, 11, CB, 0);
		TextMesh->Render(gd.gpucmd, 1);

		vec4 slotColor = { 0.15f, 0.15f, 0.15f, 0.9f };

		float startSlotX = invPosX - invWidth + slotPadding + slotSize;
		float startSlotY = invPosY - invHeight + slotPadding + slotSize;

		for (int row = 0; row < gridRows; ++row) {
			for (int col = 0; col < gridColumns; ++col) {
				float slotCurrentX = startSlotX + col * (2 * (slotSize + slotPadding));
				float slotCurrentY = startSlotY + row * (2 * (slotSize + slotPadding));

				float CB2[11] = { slotCurrentX - slotSize ,slotCurrentY - slotSize , slotCurrentX + slotSize ,slotCurrentY + slotSize, slotColor.r, slotColor.g, slotColor.b, slotColor.a, gd.ClientFrameWidth, gd.ClientFrameHeight, 0.05f };
				gd.gpucmd->SetGraphicsRoot32BitConstants(0, 11, CB2, 0);
				TextMesh->Render(gd.gpucmd, 1);
			}
		}

		float startItemX = invPosX - Rate * 140.0f;
		float startItemY = invPosY - Rate * 130.0f;

		matrix viewMat2 = DirectX::XMMatrixLookAtLH(vec4(0, 0, 0), vec4(0, 0, 1), vec4(0, 1, 0));
		viewMat2 *= gd.viewportArr[0].ProjectMatrix;
		//gd.gpucmd->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		//	D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);
		////((Shader*)MyDiffuseTextureShader)->Add_RegisterShaderCommand(gd.gpucmd);
		//gd.gpucmd->SetGraphicsRoot32BitConstants(0, 16, &viewMat, 0);
		//gd.gpucmd->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16);
		//gd.gpucmd->SetGraphicsRootConstantBufferView(2, LightCBResource.resource->GetGPUVirtualAddress());

		//for (int i = 0; i < player->maxItem; ++i) {
		//	if (i >= gridColumns * gridRows)
		//		break;

		//	ItemStack& currentStack = player->Inventory[i];
		//	ItemID currentItemID = currentStack.id;
		//	if (currentItemID == 0) continue;

		//	if (currentItemID >= ItemTable.size())
		//		continue;

		//	Item& itemInfo = ItemTable[currentItemID];

		//	//Mesh* itemMesh = GetOrCreateColoredQuadMesh(itemInfo.color);

		//	int column = i % gridColumns;
		//	int row = i / gridColumns;

		//	float itemCurrentX = startItemX + column * (2 * (slotSize + slotPadding));
		//	float itemCurrentY = startItemY + row * (2 * (slotSize + slotPadding));

		//	/*vec4 v = gd.viewportArr[0].unproject(vec4(gd.ClientFrameWidth/2, gd.ClientFrameHeight/2, 0, 1));
		//	v *= 5;
		//	v += gd.viewportArr[0].Camera_Pos;*/

		//	/*vec4 unproj = gd.viewportArr[0].unproject(vec4(-0.5f, 0.5f, 0, 1));*/
		//	matrix itemMat;
		//	itemMat.pos = gd.viewportArr[0].Camera_Pos;
		//	//caminvMat.look.x *= -1;
		//	itemMat.pos += viewMat.look * 7;
		//	constexpr float xmul = 1.35f;
		//	constexpr float ymul = 0.825f;
		//	itemMat.pos += viewMat.right * (-2.7f + xmul * column);
		//	itemMat.pos += viewMat.up * (1.65f - ymul * row);
		//	itemMat.pos.w = 1;

		//	itemMat.transpose();
		//	gd.gpucmd->SetGraphicsRoot32BitConstants(1, 16, &itemMat, 0);
		//	MyDiffuseTextureShader->SetTextureCommand(ItemTable[currentItemID].tex);
		//	ItemTable[currentItemID].MeshInInventory->Render(gd.gpucmd, 1);
		//	//RenderUIObject(itemMesh, itemMat);
		//}

		gd.gpucmd->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
			D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);
		((Shader*)MyScreenCharactorShader)->Add_RegisterShaderCommand(gd.gpucmd);

		int i = 0;
		float top = startSlotY + (gridRows - 1) * (2 * (slotSize + slotPadding));
		for (int row = 0; row < gridRows; ++row) {
			for (int col = 0; col < gridColumns; ++col) {
				if (player->Inventory[i].id == 0) {
					i++; continue;
				}

				float slotCurrentX = startSlotX + col * (2 * (slotSize + slotPadding));
				float slotCurrentY = top - row * (2 * (slotSize + slotPadding));

				ItemStack& currentStack = player->Inventory[i];
				std::wstring countStr = L"x" + std::to_wstring(currentStack.ItemCount);
				vec4 textRect = { slotCurrentX - slotSize ,slotCurrentY - slotSize , slotCurrentX + slotSize ,slotCurrentY + slotSize };
				float fontSize = 20.0f;
				RenderText(countStr.c_str(), countStr.length(), textRect, fontSize, 0.01f);
				i++;
			}
		}
	}

	//static float stackT = 0;
	//stackT += game.DeltaTime;
	//RenderText(L"0123456789\nHello, World!\n안녕 너무 반가워~~", 36, vec4(-1000, 0, 1000, 100), 50 + 50 * sin(stackT));

	//render end ----------------------------------------------------------------

	//RenderTarget State Changing Command [RenderTarget -> Present] + wait untill finish rendering
	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	gd.gpucmd->ResourceBarrier(1, &d3dResourceBarrier);

	//Command execution
	gd.gpucmd.Close();
	gd.gpucmd.Execute();
	gd.WaitGPUComplete();

	// Present to Swapchain BackBuffer & RenderTargetIndex Update
	DXGI_PRESENT_PARAMETERS dxgiPresentParameters;
	dxgiPresentParameters.DirtyRectsCount = 0;
	dxgiPresentParameters.pDirtyRects = NULL;
	dxgiPresentParameters.pScrollRect = NULL;
	dxgiPresentParameters.pScrollOffset = NULL;
	gd.pSwapChain->Present1(1, 0, &dxgiPresentParameters);

	//Get Present RenderTarget Index
	gd.CurrentSwapChainBufferIndex = gd.pSwapChain->GetCurrentBackBufferIndex();
}

void Game::Render_RayTracing()
{
	gd.ShaderVisibleDescPool.BakeImmortalDesc();
	gd.ShaderVisibleDescPool.DynamicReset();

	//rebuild AS
	game.MyRayTracingShader->PrepareRender();

	// Reset command list and allocator.
	ID3D12GraphicsCommandList4* commandList = gd.raytracing.dxrCommandList;

	//gd.gpucmd.pCommandAllocator->Reset(); // origin : m_commandAllocators[m_backBufferIndex]->Reset()
	//commandList->Reset(gd.gpucmd.pCommandAllocator, nullptr);
	//gd.CmdReset(commandList, gd.pCommandAllocator);
	gd.gpucmd.Reset(true);

	// Transition the render target into the correct state to allow for drawing into it.
	D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	commandList->ResourceBarrier(1, &barrier);

	commandList->SetComputeRootSignature(MyRayTracingShader->pGlobalRootSignature);

	// Bind the heaps, acceleration structure and dispatch rays.    
	D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
	commandList->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
	commandList->SetComputeRootDescriptorTable(0, gd.raytracing.RTO_UAV_index.hRender.hgpu); // sub render target, raytracing output
	commandList->SetComputeRootShaderResourceView(1, MyRayTracingShader->TLAS->GetGPUVirtualAddress()); // AS SRV
	commandList->SetComputeRootConstantBufferView(2, gd.raytracing.CameraCB->GetGPUVirtualAddress()); // Camera CB CBV
	commandList->SetComputeRootDescriptorTable(3, RayTracingMesh::VBIB_DescIndex.hRender.hgpu); // Vertex, IndexBuffer SRV
	commandList->SetComputeRootDescriptorTable(4, MySkyBoxShader->CurrentSkyBox.descindex.hRender.hgpu); // SkyBox SRV
	commandList->SetComputeRootDescriptorTable(5, RayTracingMesh::UAV_VBIB_DescIndex.hRender.hgpu); // SkinMesh SRV

	commandList->SetPipelineState1(MyRayTracingShader->RTPSO);

	// Since each shader table has only one shader record, the stride is same as the size.
	//size_t v = MyRayTracingShader->HitGroupShaderTable->GetGPUVirtualAddress();
	//v = (v + 63) & ~63;
	dispatchDesc.HitGroupTable.StartAddress = MyRayTracingShader->HitGroupShaderTable->GetGPUVirtualAddress();
	dispatchDesc.HitGroupTable.SizeInBytes = MyRayTracingShader->HitGroupShaderTable->GetDesc().Width;
	dispatchDesc.HitGroupTable.StrideInBytes = MyRayTracingShader->shaderIdentifierSize + sizeof(LocalRootSigData);

	/*v = MyRayTracingShader->MissShaderTable->GetGPUVirtualAddress();
	v = (v + 63) & ~63;*/
	dispatchDesc.MissShaderTable.StartAddress = MyRayTracingShader->MissShaderTable->GetGPUVirtualAddress();
	dispatchDesc.MissShaderTable.SizeInBytes = MyRayTracingShader->MissShaderTable->GetDesc().Width;
	dispatchDesc.MissShaderTable.StrideInBytes = dispatchDesc.MissShaderTable.SizeInBytes;

	dispatchDesc.RayGenerationShaderRecord.StartAddress = MyRayTracingShader->RayGenShaderTable->GetGPUVirtualAddress();
	dispatchDesc.RayGenerationShaderRecord.SizeInBytes = MyRayTracingShader->RayGenShaderTable->GetDesc().Width;

	dispatchDesc.Width = gd.ClientFrameWidth;
	dispatchDesc.Height = gd.ClientFrameHeight;
	dispatchDesc.Depth = 1;
	commandList->DispatchRays(&dispatchDesc);

	// copy to rendertarget
	D3D12_RESOURCE_BARRIER preCopyBarriers[2];
	preCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
	preCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(gd.raytracing.RayTracingOutput, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);

	commandList->CopyResource(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], gd.raytracing.RayTracingOutput);

	D3D12_RESOURCE_BARRIER postCopyBarriers[2];
	postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
	postCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(gd.raytracing.RayTracingOutput, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(ARRAYSIZE(postCopyBarriers), postCopyBarriers);

	//Execute
	gd.gpucmd.Close(true);
	gd.gpucmd.Execute(true);
	gd.gpucmd.WaitGPUComplete();
	gd.pSwapChain->Present(1, 0);

	//Get Present RenderTarget Index
	gd.CurrentSwapChainBufferIndex = gd.pSwapChain->GetCurrentBackBufferIndex();
}

void Game::Render_ShadowPass()
{
	constexpr int ShadowResolusion = 4096;
	vec4 LightDirection = vec4(1, 2, 1);
	LightDirection.len3 = 1;
	constexpr float LightDistance = 500;
	game.MySpotLight.viewport.Viewport.Width = ShadowResolusion;
	game.MySpotLight.viewport.Viewport.Height = ShadowResolusion;
	game.MySpotLight.viewport.Viewport.MaxDepth = 1.0f;
	game.MySpotLight.viewport.Viewport.MinDepth = 0.0f;
	game.MySpotLight.viewport.Viewport.TopLeftX = 0.0f;
	game.MySpotLight.viewport.Viewport.TopLeftY = 0.0f;
	game.MySpotLight.viewport.ScissorRect = { 0, 0, (long)ShadowResolusion, (long)ShadowResolusion };

	vec4 obj = player->worldMat.pos;
	obj.w = 0;

	game.MySpotLight.viewport.Camera_Pos = obj + LightDirection * LightDistance;
	game.MySpotLight.viewport.Camera_Pos.w = 0;
	game.MySpotLight.LightPos = game.MySpotLight.viewport.Camera_Pos;

	MySpotLight.View.mat = XMMatrixLookAtLH(MySpotLight.LightPos, obj, vec4(0, 1, 0, 0));
	game.MySpotLight.viewport.ViewMatrix = MySpotLight.View;

	constexpr float rate = 1.0f / 16.0f;
	game.MySpotLight.viewport.ProjectMatrix = XMMatrixOrthographicLH(rate * ShadowResolusion, rate * ShadowResolusion, 0.1f, 1000.0f);

	matrix projmat = XMMatrixTranspose(MySpotLight.viewport.ProjectMatrix);
	LightCB_withShadowResource.resource->Map(0, NULL, (void**)&LightCBData_withShadow);
	LightCBData_withShadow->LightProjection = projmat;
	LightCBData_withShadow->LightView = XMMatrixTranspose(MySpotLight.viewport.ViewMatrix);
	LightCBData_withShadow->LightPos = MySpotLight.LightPos.f3;
	LightCB_withShadowResource.resource->Unmap(0, nullptr);

	gd.gpucmd.Reset();

	//Wait Finish Present
	D3D12_RESOURCE_BARRIER d3dResourceBarrier;
	::ZeroMemory(&d3dResourceBarrier, sizeof(D3D12_RESOURCE_BARRIER));
	d3dResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	d3dResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	d3dResourceBarrier.Transition.pResource =
		gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex];
	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	gd.gpucmd->ResourceBarrier(1, &d3dResourceBarrier);

	gd.gpucmd->RSSetViewports(1, &game.MySpotLight.viewport.Viewport);
	gd.gpucmd->RSSetScissorRects(1, &game.MySpotLight.viewport.ScissorRect);

	//Clear Render Target Command Addition
	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle =
		gd.pRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	d3dRtvCPUDescriptorHandle.ptr += (gd.CurrentSwapChainBufferIndex *
		gd.RtvDescriptorIncrementSize);
	/*float pfClearColor[4] = { 0, 0, 0, 1.0f };
	gd.gpucmd->ClearRenderTargetView(d3dRtvCPUDescriptorHandle,
		pfClearColor, 0, NULL);*/

		//Render Shadow Map (Shadow Pass)
		//spotlight view
	game.MySpotLight.ShadowMap.AddResourceBarrierTransitoinToCommand(gd.gpucmd, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	//Clear Depth Stencil Buffer Command Addtion
	D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle =
		gd.pDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_CPU_DESCRIPTOR_HANDLE hcpu = game.MySpotLight.ShadowMap.descindex.hRender.hcpu;
	gd.gpucmd->OMSetRenderTargets(0, nullptr, TRUE, &hcpu);
	//there is no render target only depth map. ??
	gd.gpucmd->ClearDepthStencilView(game.MySpotLight.ShadowMap.descindex.hRender.hcpu,
		D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);

	//MyShader->Add_RegisterShaderCommand(gd.gpucmd, ShaderType::RenderShadowMap);

	//matrix xmf4x4Projection = game.MySpotLight.viewport.ProjectMatrix;
	//xmf4x4Projection.transpose();
	////XMStoreFloat4x4(&xmf4x4Projection, XMMatrixTranspose(gd.viewportArr[0].ProjectMatrix));
	//gd.gpucmd->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4Projection, 0);

	//for (auto& gbj : m_gameObjects) {
	//	gbj->RenderShadowMap();
	//}

	game.MyPBRShader1->Add_RegisterShaderCommand(gd.gpucmd, ShaderType::RenderShadowMap);

	matrix xmf4x4View = game.MySpotLight.viewport.ViewMatrix;
	xmf4x4View *= game.MySpotLight.viewport.ProjectMatrix;
	xmf4x4View.transpose();
	gd.gpucmd->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4View, 0);
	gd.gpucmd->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16); // no matter

	gd.gpucmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);

	matrix mat2 = XMMatrixIdentity();
	//mat2.pos.y -= 1.75f;
	//game.MySpotLight.viewport.ProjectMatrix = XMMatrixOrthographicLH(rate * ShadowResolusion * 2, rate * ShadowResolusion * 2, 0.1f, 1000.0f);
	//game.MySpotLight.viewport.UpdateFrustum();
	game.MySpotLight.viewport.UpdateOrthoFrustum(0.1f, 1000.0f);
	//Game::renderViewPort = &game.MySpotLight.viewport;
	//game.Map->MapObjects[0]->Render_Inherit_CullingOrtho(mat2, ShaderType::RenderShadowMap);

	//render Objects
	for (int i = 0; i < DynmaicGameObjects.size(); ++i) {
		if (DynmaicGameObjects[i] == nullptr || DynmaicGameObjects[i]->tag[GameObjectTag::Tag_Enable] == false) continue;
		DynmaicGameObjects[i]->Render();
	}

	player->Render_AfterDepthClear();

	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	gd.gpucmd->ResourceBarrier(1, &d3dResourceBarrier);

	gd.gpucmd.Close();
	gd.gpucmd.Execute();
	gd.WaitGPUComplete();
}

void Game::SetRenderMod(bool isbatch)
{
	CurrentRenderBatch = isbatch;
	if (CurrentRenderBatch) {
		GameObject::CurrentRenderFunc = &GameObject::PushRenderBatch;
		StaticGameObject::CurrentRenderFunc = &StaticGameObject::PushRenderBatch;
		DynamicGameObject::CurrentRenderFunc = &DynamicGameObject::PushRenderBatch;
		SkinMeshGameObject::CurrentRenderFunc = &SkinMeshGameObject::PushRenderBatch;
	}
	else {
		GameObject::CurrentRenderFunc = &GameObject::Render;
		StaticGameObject::CurrentRenderFunc = &StaticGameObject::Render;
		DynamicGameObject::CurrentRenderFunc = &DynamicGameObject::Render;
		SkinMeshGameObject::CurrentRenderFunc = &SkinMeshGameObject::Render;
	}
}

void Game::ClearAllMeshInstancing()
{
	for (int i = 0; i < MeshTable.size(); ++i) {
		for (int k = 0; k < MeshTable[i]->subMeshNum; ++k) {
			//dbglog3(L"Instancing %d %d : %d \n", i, k, MeshTable[i]->InstanceData[k].Capacity);
			MeshTable[i]->InstanceData[k].ClearInstancing();
		}
	}
}

void Game::BatchRender(ID3D12GraphicsCommandList* cmd)
{
	for (int i = 0; i < MeshTable.size(); ++i) {
		MeshTable[i]->BatchRender(cmd);
	}
}

void Game::Update()
{
	static float accSend = 0.0f;
	const float SendPeriod = 0.03f;

	accSend += DeltaTime;

	if (isPrepared && !isInventoryOpen && GetActiveWindow() == hWnd) {
		while (ShowCursor(FALSE) >= 0);

		POINT center = { (LONG)gd.ClientFrameWidth / 2, (LONG)gd.ClientFrameHeight / 2 };
		ClientToScreen(hWnd, &center);
		SetCursorPos(center.x, center.y);

		RECT rect;
		GetClientRect(hWnd, &rect);
		POINT ul = { rect.left, rect.top };
		POINT lr = { rect.right, rect.bottom };
		MapWindowPoints(hWnd, NULL, &ul, 1);
		MapWindowPoints(hWnd, NULL, &lr, 1);
		rect = { ul.x, ul.y, lr.x, lr.y };
		ClipCursor(&rect);
	}
	else {
		while (ShowCursor(TRUE) < 0);
		ClipCursor(NULL);
	}

	if (player != nullptr) {
		const float rate = 0.005f;

		player->m_yaw += DeltaMousePos.x * rate;
		player->m_pitch += DeltaMousePos.y * rate;

		DeltaMousePos.x = 0;
		DeltaMousePos.y = 0;

		if (player->m_yaw > XM_2PI)  player->m_yaw -= XM_2PI;
		if (player->m_yaw < -XM_2PI) player->m_yaw += XM_2PI;
		if (player->m_pitch > XM_PIDIV2 - 0.05f) player->m_pitch = XM_PIDIV2 - 0.05f;
		if (player->m_pitch < -XM_PIDIV2 + 0.05f) player->m_pitch = -XM_PIDIV2 + 0.05f;

		if (accSend >= SendPeriod) {
			CTS_SyncRotation_Header pkt;
			pkt.size = sizeof(CTS_SyncRotation_Header);
			pkt.st = CTS_Protocol::SyncRotation;
			pkt.yaw = player->m_yaw;
			pkt.pitch = player->m_pitch;
			client.send((char*)&pkt, sizeof(CTS_SyncRotation_Header), 0);
			accSend = 0;
		}
	}

	while (true) {
		int result = client.recv(client.rBuf + client.rbufOffset, client.rbufMax - client.rbufOffset, 0);
		if (result > 0) {
			char* cptr = client.rBuf;
			result += client.rbufOffset;
			int offset = game.Receiving(cptr, result);
			memmove(client.rBuf, client.rBuf + offset, result - offset);
			client.rbufOffset = result - offset;
		}
		if (result == -1) {
			if (WSAGetLastError() == WSAEWOULDBLOCK) {
				// 아직 읽을 수 없다면 다음 루프로 넘긴다.
				break;
			}
			else {
				// 네트워크 오류가 발생되었거나 서버가 죽은 상황
				// TODO : 서버와의 연결 끊을때의 처리
				break;
			}
		}
		else if (result == 0) {
			// 서버가 종료됨.
			// TODO : 서버와의 연결 끊을때의 처리
			//client.DisConnectToServer();
			break;
		}
		else break; // ??
	}

	if (isPrepared == false) {
		if (game.isPreparedGo) {
			preparedFlow += DeltaTime;
			if (preparedFlow > 2.0f) isPrepared = true;
		}
		return;
	}

	// chunk에서 업데이트 수행.
	/*for (int i = 0; i < m_gameObjects.size(); ++i) {
		if (m_gameObjects[i] == nullptr || m_gameObjects[i]->tag[GameObjectTag::Tag_Enable] == false) continue;
		m_gameObjects[i]->Update(DeltaTime);
	}*/

	if (playerGameObjectIndex >= 0 && playerGameObjectIndex < DynmaicGameObjects.size()) {
		Player* p = (Player*)DynmaicGameObjects[playerGameObjectIndex];
		p->ClientUpdate(DeltaTime);
		player = (Player*)DynmaicGameObjects[playerGameObjectIndex];
	}

	if (player != nullptr) {

		XMMATRIX rotMat = XMMatrixRotationRollPitchYaw(0, player->m_yaw, 0);
		vec4 currentPos = player->worldMat.pos;
		player->worldMat.mat = rotMat;
		player->worldMat.pos = currentPos;

		XMVECTOR lookQuat = XMQuaternionRotationRollPitchYaw(player->m_pitch, player->m_yaw, 0);
		vec4 clook = { 0, 0, 1, 0 };
		clook = XMVector3Rotate(clook, lookQuat);

		vec4 peye = player->worldMat.pos;
		vec4 pat = player->worldMat.pos;

		if (bFirstPersonVision) {
			peye += 1.0f * player->worldMat.up;
			pat += 1.0f * player->worldMat.up;
			pat += 10.0f * clook;
		}
		else {
			peye -= 3.0f * clook;
			pat += 10.0f * clook;
			peye += 0.5f * player->worldMat.up;
			peye += 0.5f * player->worldMat.right;
		}

		XMFLOAT3 Up = { 0, 1, 0 };
		gd.viewportArr[0].ViewMatrix = XMMatrixLookAtLH(peye, pat, XMLoadFloat3(&Up));
		gd.viewportArr[0].Camera_Pos = peye;

		for (int i = 0; i < bulletRays.size; ++i) {
			if (bulletRays.isnull(i)) continue;
			bulletRays[i].Update();
			if (bulletRays[i].direction.fast_square_of_len3 < 0.01f) {
				bulletRays.Free(i);
			}
		}
	}
}

int Game::Receiving(char* ptr, int totallen)
{
	char* currentPivot = ptr;
	int offset = 0;

	int size = *(int*)currentPivot;
	if (offset + size > totallen) {
		return offset;
	}
	STCProtocol type = *(STCProtocol*)(currentPivot + sizeof(int));
	switch (type) {
	case STCProtocol::SyncGameObject:
	{
		STC_SyncGameObject_Header& header = *(STC_SyncGameObject_Header*)currentPivot;
		char* datapivot = currentPivot + sizeof(STC_SyncGameObject_Header);
		switch (header.type) {
		case GameObjectType::_GameObject:
		case GameObjectType::_StaticGameObject:
		{
			if (header.objindex >= StaticGameObjects.size()) {
				// 이 코드는 실행되지 말아야 함. 최대한. 하지만 오류가 났을때 대처하기 위해 일단 넣어놓는다.
				StaticGameObjects.reserve(header.objindex + 1);
				StaticGameObjects.resize(header.objindex + 1);
			}

			if (StaticGameObjects[header.objindex]) {
				if (*(void**)StaticGameObjects[header.objindex] != GameObjectType::vptr[header.type]) {
					delete StaticGameObjects[header.objindex];
					StaticGameObjects[header.objindex] = nullptr;
					switch (header.type) {
					case GameObjectType::_StaticGameObject:
						StaticGameObjects[header.objindex] = new StaticGameObject();
						break;
					}
				}
			}
			StaticGameObjects[header.objindex]->RecvSTC_SyncObj(datapivot);
		}
			break;
		case GameObjectType::_DynamicGameObject:
		case GameObjectType::_SkinMeshGameObject:
		case GameObjectType::_Player:
		case GameObjectType::_Monster:
		{
			if (header.objindex >= DynmaicGameObjects.size()) {
				// 이 코드는 실행되지 말아야 함. 최대한. 하지만 오류가 났을때 대처하기 위해 일단 넣어놓는다.
				DynmaicGameObjects.reserve(header.objindex + 1);
				DynmaicGameObjects.resize(header.objindex + 1);
			}

			if (DynmaicGameObjects[header.objindex]) {
				if (*(void**)DynmaicGameObjects[header.objindex] != GameObjectType::vptr[header.type]) {
					delete DynmaicGameObjects[header.objindex];
					DynmaicGameObjects[header.objindex] = nullptr;
					switch (header.type) {
					case GameObjectType::_DynamicGameObject:
						DynmaicGameObjects[header.objindex] = new DynamicGameObject();
						break;
					case GameObjectType::_SkinMeshGameObject:
						DynmaicGameObjects[header.objindex] = new SkinMeshGameObject();
						break;
					case GameObjectType::_Player:
						DynmaicGameObjects[header.objindex] = new Player();
						break;
					case GameObjectType::_Monster:
						DynmaicGameObjects[header.objindex] = new Monster();
						break;
					}
				}
			}

			DynmaicGameObjects[header.objindex]->RecvSTC_SyncObj(datapivot);
		}
			break;
		}
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STCProtocol::ChangeMemberOfGameObject:
	{
		STC_ChangeMemberOfGameObject_Header& header = *(STC_ChangeMemberOfGameObject_Header*)currentPivot;
		char* datapivot = currentPivot + sizeof(STC_ChangeMemberOfGameObject_Header);
		
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STCProtocol::NewRay:
	{
		//수정필요
		vec4 start, direction;
		float distance;
		start.f3 = *(XMFLOAT3*)&ptr[offset];
		start.w = 0;
		offset += 12;
		direction.f3 = *(XMFLOAT3*)&ptr[offset];
		direction.w = 0;
		offset += 12;
		distance = *(float*)&ptr[offset];
		offset += 4;

		int BulletIndex = bulletRays.Alloc();
		if (BulletIndex < 0) {
			return offset;
		}
		BulletRay& bray = bulletRays[BulletIndex];
		bray = BulletRay(start, direction, distance);
	}
	break;
	case STCProtocol::AllocPlayerIndexes:
	{
		int clientindex = *(int*)&ptr[offset];
		offset += 4;
		int objindex = *(int*)&ptr[offset];
		offset += 4;
		game.clientIndexInServer = clientindex;
		game.playerGameObjectIndex = objindex;

		if (game.DynmaicGameObjects.size() <= objindex || objindex < 0) {
			return offset;
		}

		if (playerGameObjectIndex >= 0 && playerGameObjectIndex < DynmaicGameObjects.size()) {
			player = (Player*)DynmaicGameObjects[playerGameObjectIndex];
			//player->Gun = game.GunMesh;
			player->GunModel = game.GunModel;

			if (player->GunModel) {
				player->gunBarrelNodeIndices.clear();
				auto addBarrel = [&](const char* name) {
					int idx = player->GunModel->FindNodeIndexByName(name);
					if (idx >= 0) player->gunBarrelNodeIndices.push_back(idx);
					};

				addBarrel("Cylinder.107");
				addBarrel("Cylinder.108");
				addBarrel("Cylinder.109");
				addBarrel("Cylinder.110");
			}

			player->gunMatrix_thirdPersonView.Id();
			player->gunMatrix_thirdPersonView.pos = vec4(0.35f, 0.5f, 0, 1);

			player->gunMatrix_firstPersonView.Id();
			player->gunMatrix_firstPersonView.pos = vec4(0.13f, -0.15f, 0.5f, 1);
			player->gunMatrix_firstPersonView.LookAt(vec4(0, 0, 5) - player->gunMatrix_firstPersonView.pos);

			player->ShootPointMesh = game.ShootPointMesh;

			player->HPBarMesh = game.HPBarMesh;
			player->HPMatrix.pos = vec4(-1, 1, 1, 1);
			player->HPMatrix.LookAt(vec4(-1, 0, 0));

			player->HeatBarMesh = game.HeatBarMesh;
			player->HeatBarMatrix.pos = vec4(-1, 1, 1, 1);
			player->HeatBarMatrix.LookAt(vec4(-1, 0, 0));

			for (int i = 0; i < 36; ++i) {
				player->Inventory[i].id = 0;
				player->Inventory[i].ItemCount = 0;
			}
		}

		game.isPreparedGo = true;
	}
	break;
	case STCProtocol::DeleteGameObject:
	{
		int objindex = *(int*)&ptr[offset];
		if (game.DynmaicGameObjects.size() <= objindex || objindex < 0) {
			return 2;
		}
		offset += 4;
		DynmaicGameObjects[objindex]->tag[GameObjectTag::Tag_Enable] = false;
		delete DynmaicGameObjects[objindex];
		DynmaicGameObjects[objindex] = nullptr;
	}
	break;
	case STCProtocol::ItemDrop:
	{
		int newindex = 0;
		ItemLoot il;
		newindex = *(int*)&ptr[offset];
		offset += sizeof(int);
		il = *(ItemLoot*)&ptr[offset];
		offset += sizeof(ItemLoot);

		if (newindex >= game.DropedItems.size()) {
			while (newindex >= game.DropedItems.size()) {
				ItemLoot til;
				til.pos = 0;
				til.itemDrop.id = 0;
				til.itemDrop.ItemCount = 0;
				game.DropedItems.push_back(til);
			}
			game.DropedItems[newindex] = il;
		}
		else {
			game.DropedItems[newindex] = il;
		}
		return offset;
	}
	break;
	case STCProtocol::ItemDropRemove:
	{
		int dindex = 0;
		ItemLoot il;
		dindex = *(int*)&ptr[offset];
		offset += sizeof(int);
		game.DropedItems[dindex].itemDrop.id = 0;
		game.DropedItems[dindex].itemDrop.ItemCount = 0;
		game.DropedItems[dindex].pos = vec4(0, 0, 0, 0);
		return offset;
	}
	break;
	case STCProtocol::InventoryItemSync:
	{
		int inventoryindex = 0;
		ItemStack il;
		il = *(ItemStack*)&ptr[offset];
		offset += sizeof(ItemStack);
		inventoryindex = *(int*)&ptr[offset];
		offset += sizeof(int);
		game.player->Inventory[inventoryindex].id = il.id;
		game.player->Inventory[inventoryindex].ItemCount = il.ItemCount;
		return offset;
	}
	break;
	case STCProtocol::PlayerFire:
	{
		int objindex = *(int*)&ptr[offset];
		offset += 4;

		if (objindex >= 0 && objindex < DynmaicGameObjects.size() && DynmaicGameObjects[objindex] != nullptr) {
			GameObject* pObj = DynmaicGameObjects[objindex];

			void* objVptr = *(void**)pObj;

			if (objVptr == GameObjectType::vptr[GameObjectType::_Player]) {
				Player* pTarget = (Player*)pObj;

				if (pTarget) {
					pTarget->weapon.OnFire();
				}
			}
		}
	}
	break;

	case STCProtocol::SyncGameState:
	{

	}
	break;
	}
	return offset;
}

void Game::AddMouseInput(int deltaX, int deltaY)
{
	if (player != nullptr)
	{
		m_stackMouseX += deltaX;
		m_stackMouseY += deltaY;

		DeltaMousePos.x += deltaX;
		DeltaMousePos.y += deltaY;

		if (DeltaMousePos.y > 200) {
			DeltaMousePos.y = 200;
		}
		if (DeltaMousePos.y < -200) {
			DeltaMousePos.y = -200;
		}
	}
}

//this function must call after ScreenCharactorShader register in pipeline.
void Game::RenderText(const wchar_t* wstr, int length, vec4 Rect, float fontsiz, float depth)
{
	//fontsize = 100 -> 0.1x | 10 -> 0.01x
	vec4 pos = vec4(Rect.x, Rect.y, 0, 0);
	constexpr float Default_LineHeight = 750;
	float mul = fontsiz / Default_LineHeight;

	constexpr float lineheight_mul = 2.5f;
	float lineheight = lineheight_mul * fontsiz;
	for (int i = 0; i < length; ++i) {
		wchar_t wc = wstr[i];
		if (wc == L'\n') {
			pos.x = Rect.x;
			pos.y -= lineheight;
			continue;
		}
		else if (wc == L' ') {
			pos.x += fontsiz;
			continue;
		}
		bool textureExist = false;
		GPUResource* texture = nullptr;
		Glyph g;
		for (int k = 0; k < gd.FontCount; ++k) {
			if (gd.font_texture_map[k].find(wc) != gd.font_texture_map[k].end()) {
				textureExist = true;
				texture = &gd.font_texture_map[k][wc];
				g = gd.font_data[k].glyphs[wc];
				break;
			}
		}

		if (textureExist == false) {
			gd.addTextureStack.push_back(wc);
			continue;
		}

		//set root variables
		vec4 textRt = vec4(pos.x + g.bounding_box[0] * mul, pos.y + g.bounding_box[1] * mul, pos.x + g.bounding_box[2] * mul, pos.y + g.bounding_box[3] * mul);
		float tConst[11] = { textRt.x, textRt.y, textRt.z, textRt.w, 1, 1, 1, 1, gd.ClientFrameWidth, gd.ClientFrameHeight, depth };
		/*gd.gpucmd->SetGraphicsRoot32BitConstants(0, 4, &textRt, 0);
		gd.gpucmd->SetGraphicsRoot32BitConstants(0, 1, &gd.ClientFrameWidth, 4);
		gd.gpucmd->SetGraphicsRoot32BitConstants(0, 1, &gd.ClientFrameHeight, 5);*/
		gd.gpucmd->SetGraphicsRoot32BitConstants(0, 11, &tConst, 0);
		MyScreenCharactorShader->SetTextureCommand(texture);

		//Render Text
		TextMesh->Render(gd.gpucmd, 1);

		//calculate next location of text
		pos.x += g.advance_width * mul;
		if (pos.x > Rect.z) {
			pos.x = Rect.x;
			pos.y -= lineheight;
			if (pos.y > Rect.w) {
				return;
			}
		}
	}
}

#include "stdafx.h"
#include "main.h"
#include "Render.h"
#include "Game.h"
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

extern GlobalDevice gd;
Game game;

void* GameObjectType::vptr[GameObjectType::ObjectTypeCount];
vector<MemberInfo> GameObjectType::Server_STCMembers[GameObjectType::ObjectTypeCount];
vector<MemberInfo> GameObjectType::Client_STCMembers[GameObjectType::ObjectTypeCount];
unordered_map<int, SyncWay> GameObjectType::STC_OffsetMap[GameObjectType::ObjectTypeCount];

// 싱크되는 변수의 서버이름과 클라이언트 이름이 다른 경우 연결을 위해 사용.
void GameObjectType::LinkOffsetByName(short type, const char* ServerVarName, const char* ClientVarName) {
	for (int k = 0;k < Server_STCMembers[type].size();++k) {
		MemberInfo& minfo = Server_STCMembers[type][k];
		if (strcmp(minfo.name, ServerVarName) == 0) {
			for (int u = 0;u < Client_STCMembers[type].size();++u) {
				MemberInfo& cminfo = Client_STCMembers[type][u];
				if (strcmp(cminfo.name, ClientVarName) == 0) {
					auto f = STC_OffsetMap[type].find(minfo.offset);
					if (f != STC_OffsetMap[type].end()) {
						f->second = SyncWay(cminfo.offset);
					}
					else {
						STC_OffsetMap[type].insert(pair<int, SyncWay>(minfo.offset, SyncWay(cminfo.offset)));
					}
					return;
				}
			}
		}
	}
}

void GameObjectType::LinkOffsetAsFunction(short type, const char* ServerVarName, void (*func)(GameObject*, char*, int))
{
	for (int k = 0;k < Server_STCMembers[type].size();++k) {
		MemberInfo& minfo = Server_STCMembers[type][k];
		if (strcmp(minfo.name, ServerVarName) == 0) {
			auto f = STC_OffsetMap[type].find(minfo.offset);
			if (f != STC_OffsetMap[type].end()) {
				f->second = SyncWay(func);
			}
			else {
				STC_OffsetMap[type].insert(pair<int, SyncWay>(minfo.offset, SyncWay(func)));
			}
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
	vptr[GameObjectType::_Portal] = GetVptr<Portal>();

	//서버의 오프셋을 받는다.
	ifstream ifs{ "STC_GameObjectOffsets.txt" };
	for (int k = 0;k < ObjectTypeCount;++k) {
		string currentType;
		ifs >> currentType;
		int n;
		ifs >> n;
		for (int i = 0;i < n;++i) {
			MemberInfo stcmi;
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
	Portal::STATICINIT();

	//이름이 같은 것 끼리 링크한다.
	for (int i = 0;i < ObjectTypeCount;++i) {
		for (int k = 0;k < Server_STCMembers[i].size();++k) {
			MemberInfo minfo = Server_STCMembers[i][k];
			//dbgbreak(strcmp(minfo.name, "DeathCount") == 0);
			for (int u = 0;u < Client_STCMembers[i].size();++u) {
				MemberInfo cminfo = Client_STCMembers[i][u];
				if (strcmp(minfo.name, cminfo.name) == 0) {
					//dbgbreak(strcmp(minfo.name, "DeathCount") == 0);
					STC_OffsetMap[i].insert(pair<int, SyncWay>(minfo.offset, SyncWay(cminfo.offset)));
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
		PointLightCBData& p = LightCBData->pointLights[i];
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
		PointLightCBData& p = LightCBData_withShadow->pointLights[i];
		p.LightPos = { (float)(rand() % 40 - 20), 1, (float)(rand() % 40 - 20) };
		p.LightIntencity = 20;
		p.LightColor = { 1, 1, 1 };
		p.LightRange = 50;
	}
	LightCBData_withShadow->LightProjection = gd.viewportArr[0].ProjectMatrix;
	LightCBData_withShadow->LightView = MyDirLight.View;
	LightCBData_withShadow->LightPos = MyDirLight.LightPos.f3;
	LightCBData_withShadow->pointLights[0].LightPos = { 10, 0, 0 };
	LightCB_withShadowResource.resource->Unmap(0, nullptr);

	gd.ShaderVisibleDescPool.ImmortalAlloc(&LightCB_withShadowResource.descindex, 1);
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvdesc;
	cbvdesc.BufferLocation = LightCB_withShadowResource.resource->GetGPUVirtualAddress();
	cbvdesc.SizeInBytes = ncbElementBytes;
	gd.pDevice->CreateConstantBufferView(&cbvdesc, LightCB_withShadowResource.descindex.hCreation.hcpu);
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
	vec4 pos = go->worldMat.pos;
	if (pos.y < -100.0f || pos.y > 500.0f ||
		pos.x < -1000.0f || pos.x > 1000.0f ||
		pos.z < -1000.0f || pos.z > 1000.0f) {
		return;
	}

	if (GameObject::IsType<Portal>(go)) {
		StaticGameObject* sgo = (StaticGameObject*)go;
		GameObjectIncludeChunks chunkIds = GetChunks_Include_OBB(sgo->GetOBB());
		int xmax = chunkIds.xmin + chunkIds.xlen;
		int ymax = chunkIds.ymin + chunkIds.ylen;
		int zmax = chunkIds.zmin + chunkIds.zlen;
		for (int ix = chunkIds.xmin; ix <= xmax; ++ix) {
			for (int iy = chunkIds.ymin; iy <= ymax; ++iy) {
				for (int iz = chunkIds.zmin; iz <= zmax; ++iz) {
					auto c = chunck.find(ChunkIndex(ix, iy, iz));
					GameChunk* gc;
					if (c == chunck.end()) {
						gc = new GameChunk();
						gc->SetChunkIndex(ChunkIndex(ix, iy, iz));
						chunck.insert(pair<ChunkIndex, GameChunk*>(ChunkIndex(ix, iy, iz), gc));
					}
					else gc = c->second;
					int allocN = gc->Static_gameobjects.Alloc();
					gc->Static_gameobjects[allocN] = sgo;
				}
			}
		}
		return;
	}


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
		if (go->tag[GameObjectTag::Tag_SkinMeshObject]) {
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
						if (up < smgo->chunkAllocIndexsCapacity) {
							smgo->chunkAllocIndexs[up] = allocN;
						}
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
						if (up < dgo->chunkAllocIndexsCapacity) {
							dgo->chunkAllocIndexs[up] = allocN;
						}
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
		1,
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
	InitDirLightGPURes();

	GameObjectType::STATICINIT();

	DropedItems.reserve(4096);
	NpcHPBars.Init(32);
	bulletRays.Init(32);

	gd.gpucmd.Reset();
	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

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

	MyComputeTestShader = new ComputeTestShader();
	MyComputeTestShader->InitShader();

	MyAnimationBlendingShader = new AnimationBlendingShader();
	MyAnimationBlendingShader->InitShader();

	MyHBoneLocalToWorldShader = new HBoneLocalToWorldShader();
	MyHBoneLocalToWorldShader->InitShader();

	DefaultTex.CreateTexture_fromFile(L"Resources/DefaultTexture.png", game.basicTexFormat, game.basicTexMip);
	DefaultNoramlTex.CreateTexture_fromFile(L"Resources/GlobalTexture/DefaultNormalTexture.png", basicTexFormat, basicTexMip);
	DefaultAmbientTex.CreateTexture_fromFile(L"Resources/GlobalTexture/DefaultAmbientTexture.png", basicTexFormat, basicTexMip);

	GunTexture.CreateTexture_fromFile(L"Resources/m1911pistol_diffuse0.png", game.basicTexFormat, game.basicTexMip);

	// particle texture
	FireTextureRes.CreateTexture_fromFile(L"Resources/fire.jpg", DXGI_FORMAT_UNKNOWN, 1, true);

	Map = new GameMap();
	Map->LoadMap("The_Port");
	for (int i = 0; i < Map->MapObjects.size(); ++i) {
		PushGameObject(Map->MapObjects[i]);
	}

	/*ofstream ofs{ "ClientStaticGameObjectOBBData.txt" };
	for (int i = 0;i < Map->MapObjects.size();++i) {
		ofs << i << " obj : \n";
		for (int k = 0;k < 4;++k) {
			for (int j = 0;j < 4;++j) {
				ofs << Map->MapObjects[i]->worldMat.f16.m[k][j] << ", ";
			}
			ofs << endl;
		}

		BoundingOrientedBox obb = Map->MapObjects[i]->GetOBB();
		if (obb.Extents.x <= 0) {
			ofs << "invalid obb" << endl;
		}
		else {
			ofs << obb.Center.x << ", ";
			ofs << obb.Center.y << ", ";
			ofs << obb.Center.z << endl;
			ofs << obb.Extents.x << ", ";
			ofs << obb.Extents.y << ", ";
			ofs << obb.Extents.z << endl;
			ofs << obb.Orientation.x << ", ";
			ofs << obb.Orientation.y << ", ";
			ofs << obb.Orientation.z << ", ";
			ofs << obb.Orientation.w << endl;
		}
	}
	ofs.close();*/

	SetLight();

	gd.viewportArr[0].ProjectMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), (float)gd.ClientFrameWidth / (float)gd.ClientFrameHeight, 0.01f, 1000.0f);

	//gd.gpucmd->Reset(gd.pCommandAllocator, NULL);

	//
	TextMesh = new UVMesh();
	TextMesh->CreateTextRectMesh();

	BumpMesh* ItemMesh = new BumpMesh();
	ItemMesh->ReadMeshFromFile_OBJ("Resources/Mesh/BulletMag001.obj", vec4(1, 1, 1, 1), true);
	ItemTable.push_back(Item(0, vec4(0, 0, 0, 0), nullptr, nullptr, L"")); // blank space in inventory.
	ItemTable.push_back(Item(1, vec4(1, 0, 0, 1), ItemMesh, &DefaultTex, L"[빨간 탄알집]"));
	ItemTable.push_back(Item(2, vec4(0, 1, 0, 1), ItemMesh, &DefaultTex, L"[녹색 탄알집]"));
	ItemTable.push_back(Item(3, vec4(0, 0, 1, 1), ItemMesh, &DefaultTex, L"[하얀 탄알집]")); // test items. red, green, blue bullet mags.

	HumanoidAnimation hanim;
	hanim.LoadHumanoidAnimation("Resources/Animation/BreakDance1990.Humanoid_animation");
	HumanoidAnimationTable.push_back(hanim);

	Model* PlayerModel = new Model();
	PlayerModel->LoadModelFile2("Resources/Model/Remy.model");
	PlayerModel->Retargeting_Humanoid(); // 휴머노이드 리타겟팅
	int playerMesh_index = Shape::AddModel("Player", PlayerModel);

	Model* MonsterModel = new Model();
	MonsterModel->LoadModelFile2("Resources/Model/Remy.model");
	MonsterModel->Retargeting_Humanoid();
	int monsterMesh_index = Shape::AddModel("Monster001", MonsterModel);

	Mesh* portalMesh = new Mesh();
	portalMesh->CreateWallMesh(2.0f, 3.0f, 0.2f, 0.0f);
	Shape::AddMesh("Portal", portalMesh);

	//// LOD Sphere Mesh 생성
	//Mesh* SphereHigh = new Mesh();
	//SphereHigh->CreateSphereMesh(gd.gpucmd, 1.5f, 10, 5, vec4(1, 0, 0, 1)); // 100 triangles
	//Shape::AddMesh("SphereHigh100", SphereHigh);

	//Mesh* SphereLow = new Mesh();
	//SphereLow->CreateSphereMesh(gd.gpucmd, 1.5f, 5, 2, vec4(0, 1, 0, 1));   // 20 triangles
	//Shape::AddMesh("SphereLow20", SphereLow);

	////LOD Sphere Object 생성
	//SphereLODObject* lodSphere = new SphereLODObject();
	//lodSphere->MeshNear = SphereHigh;
	//lodSphere->MeshFar = SphereLow;
	///*lodSphere->m_pShader = MyShader;
	//lodSphere->MaterialIndex = 0;*/
	//lodSphere->FixedPos = vec4(5, 5, 0, 1);
	//lodSphere->worldMat = (XMMatrixTranslation(5.0f, 5.0f, 0.0f));
	//lodSphere->SwitchDistance = 20.0f;

	//game.DynmaicGameObjects.push_back(lodSphere);

	BulletRay::mesh = (Mesh*)new Mesh();
	BulletRay::mesh->ReadMeshFromFile_OBJ("Resources/Mesh/RayMesh.obj", { 1, 1, 0, 1 }, false);

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

	MyDirLight.ShadowMap = gd.CreateShadowMap(4096, 4096, gd.GetDirLightCascadingShadowDSVIndex(0), MyDirLight);
	MyDirLight.View.mat = XMMatrixLookAtLH(vec4(0, 2, 5, 0), vec4(0, 0, 0, 0), vec4(0, 1, 0, 0));

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

void Game::InitDirLightGPURes() {
	UINT ncbElementBytes = ((sizeof(DirLightInfo) + 255) & ~255); //256의 배수
	DirLightRes = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1);
	DirLightRes.resource->Map(0, NULL, (void**)&MappedDirLightData);

	gd.ShaderVisibleDescPool.ImmortalAlloc(&DirLightResCBV, 1);
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvdesc;
	cbvdesc.BufferLocation = DirLightRes.resource->GetGPUVirtualAddress();
	cbvdesc.SizeInBytes = ncbElementBytes;
	gd.pDevice->CreateConstantBufferView(&cbvdesc, DirLightResCBV.hCreation.hcpu);
}

void Game::Render() {
	// 1. DRED 활성화
	D3D12EnableExperimentalFeatures(1, &D3D12ExperimentalShaderModels, nullptr, nullptr);

	//2. 프러스텀 업데이트
	gd.viewportArr[0].UpdateFrustum();

	//2.5. 인스턴싱 미리 계산
	if (SceneRenderBatch) {
		game.renderViewPort = &gd.viewportArr[0];
		SetRenderMod(SceneRenderBatch);
		ClearAllMeshInstancing();
		RenderTour<false>();
		SetRenderMod(false);

		//for (int i = 0; i < MeshTable.size(); ++i) {
		//	for (int k = 0; k < MeshTable[i]->subMeshNum; ++k) {
		//		//dbglog2(L"Instancing %d : %llx\n", MeshTable[i]->InstanceData[k].InstancingSRVIndex.index, MeshTable[i]->InstanceData[k].StructuredBuffer.resource->GetGPUVirtualAddress());
		//		uint64_t raw[4];
		//		memcpy(raw, (void*)MeshTable[i]->InstanceData[k].InstancingSRVIndex.hCreation.hcpu.ptr, 32);
		//		uint64_t gpuVA = raw[3];
		//		if (MeshTable[i]->InstanceData[k].StructuredBuffer.resource->GetGPUVirtualAddress() != gpuVA)
		//		{
		//			dbglog2(L"desc error! %llx %llx \n", MeshTable[i]->InstanceData[k].StructuredBuffer.resource->GetGPUVirtualAddress(), gpuVA);
		//		}
		//	}
		//}
	}

	//3. Shader Visible Desc Heap Dynamic Reset
	if (Material::LastMaterialStructureBufferUp < game.MaterialTable.size()) {
		Material::InitMaterialStructuredBuffer();
	}
	gd.ShaderVisibleDescPool.BakeImmortalDesc();
	gd.ShaderVisibleDescPool.DynamicReset();
	

	//4. 필요해진 텍스트 텍스쳐 로딩
	//SDF text texture loading
	if (gd.addSDFTextureStack.size() > 0) {
		for (int i = 0; i < gd.addSDFTextureStack.size(); ++i) {
			gd.AddTextSDFTexture(gd.addSDFTextureStack[i]);
		}
	}
	if (gd.gpucmd.isClose == false) {
		gd.gpucmd.Close();
		gd.gpucmd.Execute(true);
		gd.gpucmd.WaitGPUComplete();
	}
	gd.addSDFTextureStack.clear();

	//5. 쉐도우 패스
	Render_ShadowPass();

	//gd.DeviceRemoveResonDebug();

	//6. 렌더패스 시작, 커맨드리스트 리셋
	HRESULT hResult = gd.gpucmd.Reset();

	//7. 쉐도우 맵의 STATE 를 PIXEL SHADER RESOURCE로 변환 (쉐도우 맵으로 쓰기 위해서)
	gd.gpucmd.ResBarrierTr(&game.MyDirLight.ShadowMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	//8. 서브렌더타겟의 STATE를 PRESENT에서 RENDER TARGET으로 변환 (서브렌더타겟에 그려야 되서)
	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	//9. 뷰포트/시저렉트 설정
	gd.gpucmd->RSSetViewports(1, &gd.viewportArr[0].Viewport);
	gd.gpucmd->RSSetScissorRects(1, &gd.viewportArr[0].ScissorRect);
	game.renderViewPort = &gd.viewportArr[0];

	//10. 뎁스 스텐실 버퍼를 가리키는 핸들을 가져온다.
	D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle =
		gd.pDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	//11. 서브렌더타겟으로 렌더타겟을 설정
	gd.gpucmd->OMSetRenderTargets(1, &gd.SubRenderTarget.rtvHandle.hcpu, TRUE,
		&d3dDsvCPUDescriptorHandle);

	//12. 렌더타겟을 클리어
	float pfClearColor[4] = { 0, 0, 0, 1.0f };
	gd.gpucmd->ClearRenderTargetView(gd.SubRenderTarget.rtvHandle.hcpu, pfClearColor, 0, NULL);

	//render begin ----------------------------------------------------------------

	// 13. 스카이박스를 렌더링
	MySkyBoxShader->RenderSkyBox();

	// 14. 모든 물체는 스카이박스보다 앞에 와야 하기 때문에 DepthStencil을 클리어
	gd.gpucmd->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);

	// 15. 카메라로 사용할 정보 초기화
	matrix view = gd.viewportArr[0].ViewMatrix;
	view *= gd.viewportArr[0].ProjectMatrix;
	view.transpose();

	// 16 ~ 17. 터레인 테셀레이션 생략

	// 18. 모든 게임오브젝트들을 출력한다.
	dbgc[0] = 0;
	//gd.AverageTimerStart();
	matrix idmat;
	idmat.Id();
	SetRenderMod(SceneRenderBatch);
	if (CurrentRenderBatch) {
		gd.gpucmd.SetShader(MyPBRShader1, ShaderType::InstancingWithShadow);
		game.PresentShaderType = ShaderType::InstancingWithShadow;
		gd.gpucmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
		{
			using PRID = PBRShader1::RootParamId;
			// 18-1. 카메라 정보 
			gd.gpucmd->SetGraphicsRoot32BitConstants(PRID::Const_Camera, 16, &view, 0);
			gd.gpucmd->SetGraphicsRoot32BitConstants(PRID::Const_Camera, 4, &gd.viewportArr[0].Camera_Pos, 16);
			// 18-2. dirlight 정보
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::CBVTable_Instancing_DirLightData, game.DirLightResCBV.hRender.hgpu);
			// 18-3. Material Structured Buffer
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_Instancing_MaterialPool, Material::MaterialStructuredBufferSRV.hRender.hgpu);
			// 18-4. ShadowMap
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_Instancing_ShadowMap, game.MyDirLight.descindex.hRender.hgpu);
			// 18-5. TextureArr[]
			DescIndex texarrSRV = DescIndex(true, gd.ShaderVisibleDescPool.TextureSRVStart);
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_Instancing_MaterialTexturePool, texarrSRV.hRender.hgpu);
		}
		BatchRender(gd.gpucmd);
		SetRenderMod(false);
	}
	else {
		//18-1. 그림자와 함께 출력하기 위해 PBRShader Set, Root 변수중 기본 고정되는 정보를 Set
		gd.gpucmd.SetShader(MyPBRShader1, ShaderType::RenderWithShadow);
		game.PresentShaderType = ShaderType::RenderWithShadow;
		gd.gpucmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
		{
			using PRID = PBRShader1::RootParamId;
			// 18-1. 카메라 정보 
			gd.gpucmd->SetGraphicsRoot32BitConstants(PRID::Const_Camera, 16, &view, 0);
			gd.gpucmd->SetGraphicsRoot32BitConstants(PRID::Const_Camera, 4, &gd.viewportArr[0].Camera_Pos, 16);
			// 18-2. 빛 정보 CBV
			gd.gpucmd->SetGraphicsRootConstantBufferView(PRID::CBV_StaticLight, game.LightCB_withShadowResource.resource->GetGPUVirtualAddress());
			// 18-3. Direction Light 의 쉐도우 맵을 적용.
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_ShadowMap, game.MyDirLight.descindex.hRender.hgpu);
		}

		RenderTour<false>();

		// 18-2. 스킨 메쉬들을 출력하기 위해 Shader를 Set.
		gd.gpucmd.SetShader(MyPBRShader1, ShaderType::SkinMeshRender);
		game.PresentShaderType = ShaderType::SkinMeshRender;
		gd.gpucmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
		{
			using PRID = PBRShader1::RootParamId;
			// 18-1. 카메라 정보 
			gd.gpucmd->SetGraphicsRoot32BitConstants(PRID::Const_Camera, 16, &view, 0);
			gd.gpucmd->SetGraphicsRoot32BitConstants(PRID::Const_Camera, 4, &gd.viewportArr[0].Camera_Pos, 16);
			// 18-2. 빛 정보 CBV
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::CBVTable_SkinMeshLightData, game.LightCB_withShadowResource.descindex.hRender.hgpu);
			// 18-3. Direction Light 의 쉐도우 맵을 적용.
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_SkinMeshShadowMaps, game.MyDirLight.descindex.hRender.hgpu);
		}
		RenderTour<true>();
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

	// 19. 파티클을 계산하고 출력한다.
	FireCS->Dispatch(gd.gpucmd, &FirePool.Buffer, FirePool.Count, DeltaTime);
	ParticleDraw->Render(gd.gpucmd, &FirePool.Buffer, FirePool.Count);
	FirePillarCS->Dispatch(gd.gpucmd, &FirePillarPool.Buffer, FirePillarPool.Count, DeltaTime);
	ParticleDraw->Render(gd.gpucmd, &FirePillarPool.Buffer, FirePillarPool.Count);
	FireRingCS->Dispatch(gd.gpucmd, &FireRingPool.Buffer, FireRingPool.Count, DeltaTime);
	ParticleDraw->Render(gd.gpucmd, &FireRingPool.Buffer, FireRingPool.Count);

	//20~21. 거울 렌더링 생략

	//22 Ray 출력
	{
		gd.gpucmd.SetShader(MyOnlyColorShader);
		view = gd.viewportArr[0].ViewMatrix * gd.viewportArr[0].ProjectMatrix;
		view.transpose();
		gd.gpucmd->SetGraphicsRoot32BitConstants(0, 16, &view, 0);

		for (int i = 0; i < bulletRays.size; ++i) {
			if (bulletRays.isnull(i)) continue;
			bulletRays[i].Render();
		}

		//gc->RenderChunkDbg();
		// 임시 포탈 렌더링
		int portalCount = 0;
		for (int i = 0; i < Portals.size(); ++i) {
			if (Portals[i] == nullptr) continue;
			portalCount++;
			Mesh* mesh = nullptr;
			Model* model = nullptr;
			Portals[i]->shape.GetRealShape(mesh, model);
			if (mesh == nullptr) continue;
			matrix world = Portals[i]->worldMat;
			world.transpose();
			gd.gpucmd->SetGraphicsRoot32BitConstants(1, 16, &world, 0);
			mesh->Render(gd.gpucmd, 1);
		}
	}

	//23. NPC 들의 HP 바를 빌보드 출력
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

	// 24. UI 출력을 위해 DepthStencil을 Clear한다. (모든 UI는 이전에 그렸던 모든 것 위에 그려져야 하기 때문)
	gd.gpucmd->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);

	// 25. 플레이어의 일부가 가장 앞에 렌더링되어야 할때 렌더링 (ex> 총기)
	float hhpp = 0;
	float HeatGauge = 0;
	int kill = 0;
	int death = 0;
	int bulletCount = 0;
	float HealSkillCooldownFlow = 0;
	if (game.player != nullptr) {
		game.player->Render_AfterDepthClear();
		hhpp = game.player->HP;
		HeatGauge = game.player->HeatGauge;
		kill = game.player->KillCount;
		death = game.player->DeathCount;
		bulletCount = game.player->bullets;
		HealSkillCooldownFlow = game.player->HealSkillCooldownFlow;
	}

	// 26. UI 텍스트 렌더링
	// HP 
	//maybe..1706x960
	float Rate = gd.ClientFrameHeight / 960.0f;
	vec4 rt = Rate * vec4(-1650, 850, -1000, 700);
	((Shader*)MyScreenCharactorShader)->Add_RegisterShaderCommand(gd.gpucmd);
	//std::wstring ui_hp = L"HP: " + std::to_wstring(hhpp);
	//RenderText(ui_hp.c_str(), ui_hp.length(), rt, 30);
	////Heat Gauge
	//vec4 rt_heat = rt;
	//rt_heat.y -= 80 * Rate;   // HP보다 약간 아래로 내림 (간격 50 정도)
	//std::wstring ui_heat = L"Heat: " + std::to_wstring((int)HeatGauge);
	//RenderText(ui_heat.c_str(), ui_heat.length(), rt_heat, 30);
	////Skill
	//	rt = Rate * vec4(-900, 850, -200, 700);
	//std::wstring ui_cool = L"[Q] Heal CD: " + std::to_wstring((int)HealSkillCooldownFlow);
	//RenderText(ui_cool.c_str(), ui_cool.length(), rt, 30);
	//// Bullet
	//rt = Rate * vec4(900, -800, 1550, -900);
	//std::wstring ui_bullet = L"Bullet: " + std::to_wstring(bulletCount);
	//RenderText(ui_bullet.c_str(), ui_bullet.length(), rt, 30);
	//// Kill/Death Counter
	//rt = Rate * vec4(1100, 920, 1550, 700);
	//std::wstring ui_kd = std::to_wstring(kill) + L" / " + std::to_wstring(death);
	//RenderText(L"Kill/Death", 10, rt, 30);
	//rt = Rate * vec4(1190, 850, 1550, 600);
	//RenderText(ui_kd.c_str(), ui_kd.length(), rt, 30);
	//// Player name
	//rt = Rate * vec4(-1650, 700, -1000, 500);
	//std::wstring playerName = L"Player: Leo";
	//RenderText(playerName.c_str(), playerName.length(), rt, 30);

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

	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	//gd.DeviceRemoveResonDebug();

	//Command execution
	hResult = gd.gpucmd.Close();
	gd.gpucmd.Execute();
	gd.gpucmd.WaitGPUComplete();

	//gd.DeviceRemoveResonDebug();

	//Bluring (Compute Shader)
	hResult = gd.CScmd.Reset();
	gd.CScmd.SetShader(MyComputeTestShader);
	gd.CScmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
	float WHarr[2] = { gd.ClientFrameWidth , gd.ClientFrameHeight };
	gd.CScmd->SetComputeRoot32BitConstants(0, 2, WHarr, 0); // screen width height
	gd.CScmd->SetComputeRootDescriptorTable(1, gd.SubRenderTarget.srvHandle.hRender.hgpu); // SRV for current
	gd.CScmd->SetComputeRootDescriptorTable(2, gd.BlurTexture.handle.hRender.hgpu); // UAV output
	int disPatchW = (gd.ClientFrameWidth / 8) + 1;
	int disPatchH = (gd.ClientFrameHeight / 8) + 1;
	gd.CScmd->Dispatch(disPatchW, disPatchH, 1);

	if constexpr (gd.PlayAnimationByGPU) {
		//Adding Animation Compute Shaders Execute Command
		SkinMeshGameObject::collection.clear();
		SkinMeshGameObject::CurrentRenderFunc = &SkinMeshGameObject::CollectSkinMeshObject;
		RenderTour<true>();
		gd.CScmd.SetShader(game.MyAnimationBlendingShader);
		for (int i = 0;i < SkinMeshGameObject::collection.size();++i) {
			SkinMeshGameObject::collection[i]->BlendingAnimation();
		}
		gd.CScmd.SetShader(game.MyHBoneLocalToWorldShader);
		for (int i = 0;i < SkinMeshGameObject::collection.size();++i) {
			SkinMeshGameObject::collection[i]->ModifyLocalToWorld();
		}
	}
	
	hResult = gd.CScmd.Close();
	gd.CScmd.Execute();
	gd.CScmd.WaitGPUComplete();

	//gd.DeviceRemoveResonDebug();

	// retuen to graphic command list. to copy resource to render back buffer;
	hResult = gd.gpucmd.Reset();

	gd.gpucmd.ResBarrierTr(gd.BlurTexture.texture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	gd.gpucmd.ResBarrierTr(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);

	gd.gpucmd->CopyResource(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], gd.BlurTexture.texture);

	gd.gpucmd.ResBarrierTr(gd.BlurTexture.texture, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	gd.gpucmd.ResBarrierTr(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);

	//render end ----------------------------------------------------------------
	gd.gpucmd->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);

	//RenderTarget State Changing Command [RenderTarget -> Present] + wait untill finish rendering
	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PRESENT);

	gd.gpucmd.ResBarrierTr(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	//Command execution
	hResult = gd.gpucmd.Close();
	gd.gpucmd.Execute();
	gd.gpucmd.WaitGPUComplete();

	//gd.DeviceRemoveResonDebug();

	// Present to Swapchain BackBuffer & RenderTargetIndex Update
	DXGI_PRESENT_PARAMETERS dxgiPresentParameters;
	dxgiPresentParameters.DirtyRectsCount = 0;
	dxgiPresentParameters.pDirtyRects = NULL;
	dxgiPresentParameters.pScrollRect = NULL;
	dxgiPresentParameters.pScrollOffset = NULL;
	gd.pSwapChain->Present1(1, 0, &dxgiPresentParameters);

	//gd.DeviceRemoveResonDebug();

	//Get Present RenderTarget Index
	gd.CurrentSwapChainBufferIndex = gd.pSwapChain->GetCurrentBackBufferIndex();

	gd.DeviceRemoveResonDebug();
}

void Game::Render_RayTracing()
{
	if (Material::LastMaterialStructureBufferUp < game.MaterialTable.size()) {
		Material::InitMaterialStructuredBuffer();
	}
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

	commandList->SetComputeRootDescriptorTable(6, Material::MaterialStructuredBufferSRV.hRender.hgpu); // Material Arr
	DescIndex texarrSRV = DescIndex(true, gd.ShaderVisibleDescPool.TextureSRVStart);
	commandList->SetComputeRootDescriptorTable(7, texarrSRV.hRender.hgpu); // Texture Arr

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
	// 1. 디렉션 라이트를 초기화
	constexpr int ShadowResolusion = 4096;
	vec4 LightDirection = vec4(1, 2, 1);
	LightDirection.len3 = 1;
	constexpr float LightDistance = 500;
	game.MyDirLight.viewport.Viewport.Width = ShadowResolusion;
	game.MyDirLight.viewport.Viewport.Height = ShadowResolusion;
	game.MyDirLight.viewport.Viewport.MaxDepth = 1.0f;
	game.MyDirLight.viewport.Viewport.MinDepth = 0.0f;
	game.MyDirLight.viewport.Viewport.TopLeftX = 0.0f;
	game.MyDirLight.viewport.Viewport.TopLeftY = 0.0f;
	game.MyDirLight.viewport.ScissorRect = { 0, 0, (long)ShadowResolusion, (long)ShadowResolusion };
	// 1-1 (플레이어를 바라보게 한다.)
	vec4 obj = 0;
	if(player) obj = player->worldMat.pos;
	obj.w = 0;

	game.MyDirLight.viewport.Camera_Pos = obj + LightDirection * LightDistance;
	game.MyDirLight.viewport.Camera_Pos.w = 0;
	game.MyDirLight.LightPos = game.MyDirLight.viewport.Camera_Pos;
	MyDirLight.View.mat = XMMatrixLookAtLH(MyDirLight.LightPos, obj, vec4(0, 1, 0, 0));
	game.MyDirLight.viewport.ViewMatrix = MyDirLight.View;
	// 1-2. 셰도우 맵 1m 당 몇개의 픽셀을 담을건지 결정한다.
	constexpr float rate = 1.0f / 8.0f;
	game.MyDirLight.viewport.ProjectMatrix = XMMatrixOrthographicLH(rate * ShadowResolusion, rate * ShadowResolusion, 0.1f, 1000.0f);
	// 1-3. Light CB 데이터를 초기화한다.
	matrix projmat = XMMatrixTranspose(MyDirLight.viewport.ProjectMatrix);
	LightCB_withShadowResource.resource->Map(0, NULL, (void**)&LightCBData_withShadow);
	LightCBData_withShadow->LightProjection = projmat;
	LightCBData_withShadow->LightView = XMMatrixTranspose(MyDirLight.viewport.ViewMatrix);
	LightCBData_withShadow->LightPos = MyDirLight.LightPos.f3;
	LightCB_withShadowResource.resource->Unmap(0, nullptr);
	// 1-4. Dir Light 전용 Upload Buffer의 값을 설정한다.
	MappedDirLightData->DirLightView = LightCBData_withShadow->LightView;
	MappedDirLightData->DirLightProjection = projmat;
	MappedDirLightData->DirLightPos = MyDirLight.LightPos.f3;
	MappedDirLightData->DirLightDir = LightDirection;
	MappedDirLightData->DirLightColor = vec4(1, 1, 1, 1);

	// 2. 렌더링을 시작한다.
	HRESULT hResult = gd.gpucmd.Reset();

	// 2-2. 뷰포트 설정
	gd.gpucmd->RSSetViewports(1, &game.MyDirLight.viewport.Viewport);
	gd.gpucmd->RSSetScissorRects(1, &game.MyDirLight.viewport.ScissorRect);
	// 2-3. ShadowMap의 STATE를 DEPTH WRITE로 설정한다.
	gd.gpucmd.ResBarrierTr(&game.MyDirLight.ShadowMap, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	// 2-4. 렌더타겟을 ShadowMap으로 Set하고 Depth Stencil을 클리어한다.
	//D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle =
	//	gd.pDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	DescHandle dc = game.MyDirLight.ShadowMap.descindex.hRender;
	gd.gpucmd->OMSetRenderTargets(0, nullptr, TRUE, &dc.hcpu);
	gd.gpucmd->ClearDepthStencilView(game.MyDirLight.ShadowMap.descindex.hRender.hcpu,
		D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);

	// 2-5. 셰도우 맵을 렌더링 하기 위해 Shader 를 Set한다.
	gd.gpucmd.SetShader(MyPBRShader1, ShaderType::RenderShadowMap);
	matrix xmf4x4View = game.MyDirLight.viewport.ViewMatrix;
	xmf4x4View *= game.MyDirLight.viewport.ProjectMatrix;
	xmf4x4View.transpose();
	gd.gpucmd->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4View, 0);
	gd.gpucmd->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16); // no matter
	gd.gpucmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
	game.PresentShaderType = ShaderType::RenderShadowMap;
	game.renderViewPort = &game.MyDirLight.viewport;
	game.renderViewPort->UpdateOrthoFrustum(0.1f, 1000.0f);

	// 2-6. 현재 플레이어가 위치한 청크 주변의 청크들만 쉐도우 렌더에 참여.
	GameChunk* center = game.GetChunkFromPos(obj);
	if (center != nullptr) {
		int chunck_extents = 2;
		for (int ix = center->cindex.x - chunck_extents; ix <= center->cindex.x + chunck_extents; ++ix) {
			for (int iy = center->cindex.y - chunck_extents; iy <= center->cindex.y + chunck_extents; ++iy) {
				for (int iz = center->cindex.z - chunck_extents; iz <= center->cindex.z + chunck_extents; ++iz) {
					auto neibor = chunck.find(ChunkIndex(ix, iy, iz));
					if (neibor == chunck.end()) continue;
					GameChunk* chck = neibor->second;
					for (int i = 0;i < chck->Static_gameobjects.size; ++i) {
						if (chck->Static_gameobjects.isnull(i)) continue;
						StaticGameObject* sgo = chck->Static_gameobjects[i];
						if (sgo == nullptr || sgo->tag[GameObjectTag::Tag_Enable] == false) continue;
						sgo->Render();
					}

					for (int i = 0;i < chck->Dynamic_gameobjects.size; ++i) {
						if (chck->Dynamic_gameobjects.isnull(i)) continue;
						DynamicGameObject* dgo = chck->Dynamic_gameobjects[i];
						if (dgo == nullptr || dgo->tag[GameObjectTag::Tag_Enable] == false) continue;
						dgo->Render();
					}
				}
			}
		}
	}

	if (player) {
		//player->Render_AfterDepthClear();
	}

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
		//dbglog1(L"playerpos y : %f \n", player->worldMat.pos.y);

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

	//gd.AverageSecPer60Start(Update_ClientRecv);
	while (true) {
		int result = client.recv(client.rBuf + client.rbufOffset, client.rbufMax - client.rbufOffset);
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
	//gd.AverageSecPer60End(Update_ClientRecv);

	gd.AverageSecPer60Start(Update_ChunksUpdate);
	// chunk에서 업데이트 수행.
	for (int i = 0; i < DynmaicGameObjects.size(); ++i) {
		if (DynmaicGameObjects[i] == nullptr || DynmaicGameObjects[i]->tag[GameObjectTag::Tag_Enable] == false) continue;
		if (DynmaicGameObjects[i]->shape.FlagPtr == 0) continue;
		DynmaicGameObjects[i]->Update(DeltaTime);
	}
	gd.AverageSecPer60End(Update_ChunksUpdate);

	//gd.AverageSecPer60Start(Update_ClientUpdate);
	if (playerGameObjectIndex >= 0 && playerGameObjectIndex < DynmaicGameObjects.size()) {
		Player* p = (Player*)DynmaicGameObjects[playerGameObjectIndex];
		if (p == nullptr) return;
		p->ClientUpdate(DeltaTime);
		player = (Player*)DynmaicGameObjects[playerGameObjectIndex];
	}
	//gd.AverageSecPer60End(Update_ClientUpdate);

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
		if (gd.isRaytracingRender) {
			gd.raytracing.SetRaytracingCamera(peye, pat - peye);
		}

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
	unsigned int size;
	STCProtocol type = STCProtocol::SyncGameObject;
READ_START:
	size = *(unsigned int*)currentPivot;
	if (offset + size >= totallen) {
		return offset;
	}
	type = *(STCProtocol*)(currentPivot + sizeof(int));
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
			game.PushGameObject(StaticGameObjects[header.objindex]);
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
			else {
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

			DynmaicGameObjects[header.objindex]->RecvSTC_SyncObj(datapivot);
			/*if (DynmaicGameObjects[header.objindex]->tag[GameObjectTag::Tag_SkinMeshObject]) {
				SkinMeshGameObject* smgo = (SkinMeshGameObject*)DynmaicGameObjects[header.objindex];
				smgo->InitRootBoneMatrixs();
			}*/
			game.PushGameObject(DynmaicGameObjects[header.objindex]);
		}
			break;
		case GameObjectType::_Portal:
		{
			Portal* portal = new Portal();
			portal->RecvSTC_SyncObj(datapivot);
			game.Portals.push_back(portal);
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

		auto f = GameObjectType::STC_OffsetMap[header.type].find(header.serveroffset);
		if (f != GameObjectType::STC_OffsetMap[header.type].end()) {
			SyncWay& sw = f->second;
			char* source = datapivot;
			if (sw.clientOffset == -1) {
				sw.syncfunc(DynmaicGameObjects[header.objindex], source, header.datasize);
			}
			else {
				char* dest = (((char*)DynmaicGameObjects[header.objindex]) + sw.clientOffset);
				memcpy(dest, source, header.datasize);
			}
		}

		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STCProtocol::NewRay:
	{
		STC_NewRay_Header& header = *(STC_NewRay_Header*)currentPivot;
		int BulletIndex = bulletRays.Alloc();
		if (BulletIndex < 0) {
			return offset;
		}
		BulletRay& bray = bulletRays[BulletIndex];
		bray = BulletRay(header.raystart, header.rayDir, header.distance);
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STCProtocol::AllocPlayerIndexes:
	{
		STC_AllocPlayerIndexes_Header& header = *(STC_AllocPlayerIndexes_Header*)currentPivot;

		if (game.isPreparedGo) {
			for (auto& pair : chunck) {
				delete pair.second;
			}
			chunck.clear();
			for (auto p : Portals) delete p;
			Portals.clear();
			// 맵 재등록
			for (int i = 0; i < Map->MapObjects.size(); ++i) {
				PushGameObject(Map->MapObjects[i]);
			}
			// ★ Dynamic 오브젝트도 재등록
			for (int i = 0; i < DynmaicGameObjects.size(); ++i) {
				if (DynmaicGameObjects[i] == nullptr) continue;
				if (DynmaicGameObjects[i]->tag[GameObjectTag::Tag_Enable] == false) continue;
				// 청크 정보 리셋
				if (DynmaicGameObjects[i]->chunkAllocIndexs) {
					delete[] DynmaicGameObjects[i]->chunkAllocIndexs;
					DynmaicGameObjects[i]->chunkAllocIndexs = nullptr;
					DynmaicGameObjects[i]->chunkAllocIndexsCapacity = 0;
				}
				PushGameObject(DynmaicGameObjects[i]);
			}
		}

		game.clientIndexInServer = header.clientindex;
		game.playerGameObjectIndex = header.server_obj_index;

		if (game.DynmaicGameObjects.size() <= header.server_obj_index || header.server_obj_index < 0) {
			return offset;
		}

		if (playerGameObjectIndex >= 0 && playerGameObjectIndex < DynmaicGameObjects.size()) {
			player = (Player*)DynmaicGameObjects[playerGameObjectIndex];
			//player->Gun = game.GunMesh;
			// 
			 //Zone 이동 시 즉시 위치 설정
			if (game.isPreparedGo) {
				player->worldMat.pos = player->DestPos;
				player->worldMat.pos.w = 1;
			}

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

		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STCProtocol::DeleteGameObject:
	{
		STC_DeleteGameObject_Header& header = *(STC_DeleteGameObject_Header*)currentPivot;
		if (game.DynmaicGameObjects.size() <= header.obj_index || header.obj_index < 0) {
			return 2;
		}
		if (DynmaicGameObjects[header.obj_index] != nullptr) {
			DynmaicGameObjects[header.obj_index]->tag[GameObjectTag::Tag_Enable] = false;
			// delete 안 함 — 청크가 아직 참조 중
		}
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STCProtocol::ItemDrop:
	{
		STC_ItemDrop_Header& header = *(STC_ItemDrop_Header*)currentPivot;
		if (header.dropindex >= game.DropedItems.size()) {
			while (header.dropindex >= game.DropedItems.size()) {
				ItemLoot til;
				til.pos = 0;
				til.itemDrop.id = 0;
				til.itemDrop.ItemCount = 0;
				game.DropedItems.push_back(til);
			}
			game.DropedItems[header.dropindex] = header.lootData;
		}
		else {
			game.DropedItems[header.dropindex] = header.lootData;
		}
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STCProtocol::ItemDropRemove:
	{
		STC_ItemDropRemove_Header& header = *(STC_ItemDropRemove_Header*)currentPivot;
		game.DropedItems[header.dropindex].itemDrop.id = 0;
		game.DropedItems[header.dropindex].itemDrop.ItemCount = 0;
		game.DropedItems[header.dropindex].pos = vec4(0, 0, 0, 0);
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STCProtocol::InventoryItemSync:
	{
		STC_InventoryItemSync_Header& header = *(STC_InventoryItemSync_Header*)currentPivot;
		game.player->Inventory[header.inventoryIndex].id = header.Iteminfo.id;
		game.player->Inventory[header.inventoryIndex].ItemCount = header.Iteminfo.ItemCount;
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STCProtocol::PlayerFire:
	{
		STC_PlayerFire_Header& header = *(STC_PlayerFire_Header*)currentPivot;
		if (header.objindex >= 0 && header.objindex < DynmaicGameObjects.size() && DynmaicGameObjects[header.objindex] != nullptr) {
			GameObject* pObj = DynmaicGameObjects[header.objindex];

			void* objVptr = *(void**)pObj;

			if (objVptr == GameObjectType::vptr[GameObjectType::_Player]) {
				Player* pTarget = (Player*)pObj;

				if (pTarget) {
					pTarget->weapon.OnFire();
				}
			}
		}
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STCProtocol::SyncGameState:
	{
		STC_SyncGameState_Header& header = *(STC_SyncGameState_Header*)currentPivot;
		game.DynmaicGameObjects.reserve(header.DynamicGameObjectCapacity);
		game.DynmaicGameObjects.resize(header.DynamicGameObjectCapacity);
		game.StaticGameObjects.reserve(header.StaticGameObjectCapacity);
		game.StaticGameObjects.resize(header.StaticGameObjectCapacity);
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	}

	goto READ_START;

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
			if (gd.font_sdftexture_map[k].find(wc) != gd.font_sdftexture_map[k].end()) {
				textureExist = true;
				texture = &gd.font_sdftexture_map[k][wc];
				g = gd.font_data[k].glyphs[wc];
				break;
			}
		}

		if (textureExist == false) {
			gd.addSDFTextureStack.push_back(wc);
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

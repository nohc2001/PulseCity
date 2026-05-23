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
	for (int k = 0; k < Server_STCMembers[type].size(); ++k) {
		MemberInfo& minfo = Server_STCMembers[type][k];
		if (strcmp(minfo.name, ServerVarName) == 0) {
			for (int u = 0; u < Client_STCMembers[type].size(); ++u) {
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
	for (int k = 0; k < Server_STCMembers[type].size(); ++k) {
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
	for (int k = 0; k < ObjectTypeCount; ++k) {
		string currentType;
		ifs >> currentType;
		int n;
		ifs >> n;
		for (int i = 0; i < n; ++i) {
			MemberInfo stcmi;
			string str;
			ifs >> str;
			stcmi.name = new char[str.size() + 1];
			strcpy_s((char*)stcmi.name, str.size() + 1, str.c_str());
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
	for (int i = 0; i < ObjectTypeCount; ++i) {
		for (int k = 0; k < Server_STCMembers[i].size(); ++k) {
			MemberInfo minfo = Server_STCMembers[i][k];
			//dbgbreak(strcmp(minfo.name, "DeathCount") == 0);
			for (int u = 0; u < Client_STCMembers[i].size(); ++u) {
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
	ncbElementBytes = ((sizeof(LightCB_DATA_withShadow) + 255) & ~255); //256의 배수
	LightCB_withShadowResource = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1);
	LightCB_withShadowResource.resource->Map(0, NULL, (void**)&LightCBData_withShadow);
	LightCBData_withShadow->dirlight.gLightColor = { 1, 1, 1 };
	vec4 dir = vec4(1, -2, 1, 0);
	dir.len3 = 1;
	LightCBData_withShadow->dirlight.gLightDirection = { dir.x, dir.y, dir.z };
	for (int i = 0; i < 3; ++i)
	{
		LightCBData_withShadow->LightProjection[i] = gd.viewportArr[0].ProjectMatrix;
		LightCBData_withShadow->LightView[i] = MyDirLight[i].View;
		LightCBData_withShadow->LightPos[i] = MyDirLight[i].LightPos.f3;
	}
	gd.ShaderVisibleDescPool.ImmortalAlloc(&LightCB_withShadowResource.descindex, 1);
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvdesc;
	cbvdesc.BufferLocation = LightCB_withShadowResource.resource->GetGPUVirtualAddress();
	cbvdesc.SizeInBytes = ncbElementBytes;
	gd.pDevice->CreateConstantBufferView(&cbvdesc, LightCB_withShadowResource.descindex.hCreation.hcpu);
}

int Game::GetRenderMaterialIndexFromGlobalMaterialIndex(int globalMatIndex)
{
	if (MaterialTable.size() > globalMatIndex) {
		Material* pmat = MaterialTable[globalMatIndex];
		int n = pmat->CB_Resource.descindex.index - gd.ShaderVisibleDescPool.InitDescArrCap - gd.ShaderVisibleDescPool.TextureSRVCap;
		return n;
	}
	else return 0;
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
	/*if (pos.y < -100.0f || pos.y > 500.0f ||
		pos.x < -1000.0f || pos.x > 1000.0f ||
		pos.z < -1000.0f || pos.z > 1000.0f) {
		return;
	}*/

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
						dbgbreak(up >= smgo->chunkAllocIndexsCapacity);
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
						dbgbreak(up >= dgo->chunkAllocIndexsCapacity);
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
	BoundingBox MapBB;
	vec4 start, end;
	start = game.Map->AABB[0].f3;
	end = game.Map->AABB[1].f3;
	BoundingBox::CreateFromPoints(MapBB, start, end);

	vec4 pos = light->pos;
	GameObjectIncludeChunks chunkIds = GetChunks_Include_OBB(light->GetOBB());
	int xmax = chunkIds.xmin + chunkIds.xlen;
	int ymax = chunkIds.ymin + chunkIds.ylen;
	int zmax = chunkIds.zmin + chunkIds.zlen;
	for (int ix = chunkIds.xmin; ix <= xmax; ++ix) {
		for (int iy = chunkIds.ymin; iy <= ymax; ++iy) {
			for (int iz = chunkIds.zmin; iz <= zmax; ++iz) {
				ChunkIndex ci = ChunkIndex(ix, iy, iz);
				if (MapBB.Intersects(ci.GetAABB())) {
					auto c = chunck.find(ci);
					GameChunk* gc;
					if (c == chunck.end()) {
						gc = new GameChunk();
						gc->SetChunkIndex(ChunkIndex(ix, iy, iz));
						chunck.insert(pair<ChunkIndex, GameChunk*>(ChunkIndex(ix, iy, iz), gc));
					}
					else gc = c->second;
					gc->Lights.push_back(light);
				}
			}
		}
	}
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
	GameObjectType::STATICINIT();

	// 2. DirLight를 초기화한다.
	InitDirLightGPURes();

	// 미리 베이크된 텍스트의 SDF 텍스쳐들을 가져온다.
	gd.GetBakedSDFs(); // later

	// 아이템, NPCHP바, Ray의 1차 용량을 설정한다.
	DropedItems.reserve(4096);
	NpcHPBars.Init(1024);
	bulletRays.Init(1024);

	// 여러 리소스들을 초기화하기 위해 커맨드리스트를 Reset해 명령을 넣을 준비를 한다.
	gd.gpucmd.Reset();
	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	// 셰이더를 초기화한다.
	{
		MyShader = new Shader();
		MyShader->InitShader();

		MyOnlyColorShader = new OnlyColorShader();
		MyOnlyColorShader->InitShader();

		MyScreenShader = new ScreenShader();
		MyScreenShader->InitShader();

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
	}

	// 클라이언트에만 사용되는 아주 기본적인 에셋들을 가져온다.
	{
		DefaultTex.CreateTexture_fromFile(L"Resources/DefaultTexture.png", game.basicTexFormat, game.basicTexMip);
		DefaultNoramlTex.CreateTexture_fromFile(L"Resources/GlobalTexture/DefaultNormalTexture.png", basicTexFormat, basicTexMip);
		DefaultAmbientTex.CreateTexture_fromFile(L"Resources/GlobalTexture/DefaultAmbientTexture.png", basicTexFormat, basicTexMip);
		// particle texture
		FireTextureRes.CreateTexture_fromFile(L"Resources/fire.jpg", game.basicTexFormat, game.basicTexMip /*DXGI_FORMAT_UNKNOWN, 1*/, true);

		//텍스트 출력에 사용할 메쉬를 가져온다.
		TextMesh = new UVMesh();
		TextMesh->CreateTextRectMesh();

		BulletRay::mesh = (Mesh*)new Mesh();
		BulletRay::mesh->ReadMeshFromFile_OBJ("Resources/Mesh/RayMesh.obj", { 1, 1, 0, 1 }, false);

		game.HPBarMesh = new Mesh();
		game.HPBarMesh->ReadMeshFromFile_OBJ("Resources/Mesh/RayMesh.obj", { 0, 1, 0, 1 }, false);

		game.HeatBarMesh = new Mesh();
		game.HeatBarMesh->ReadMeshFromFile_OBJ("Resources/Mesh/RayMesh.obj", { 1, 0, 0, 1 }, false);

		game.ShootPointMesh = new Mesh();
		game.ShootPointMesh->CreateWallMesh(0.05f, 0.05f, 0.05f, { 1, 1, 1, 0.5f });

		for (int i = 0; i < 3; ++i) {
			MyDirLight[i].ShadowMap = gd.CreateShadowMap(4096, 4096, gd.GetDirLightCascadingShadowDSVIndex(i), MyDirLight[i]);
			MyDirLight[i].View.mat = XMMatrixLookAtLH(vec4(0, 2, 5, 0), vec4(0, 0, 0, 0), vec4(0, 1, 0, 0));
		}

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

		OBBDebugMesh = new Mesh();
		OBBDebugMesh->CreateWallMesh(1.0f, 1.0f, 1.0f, vec4(1, 1, 1, 1));

		SetLight();

		gd.viewportArr[0].ProjectMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), (float)gd.ClientFrameWidth / (float)gd.ClientFrameHeight, 0.01f, 1000.0f);
	}

	// 어떤 존에서나 공동으로 사용가능한 글로벌 에셋들을 가져온다.
	// isAssetAddingInGlobal = true 면 이제 에셋이 공용으로 추가되기 시작한다. 그 신호를 준다.
	isAssetAddingInGlobal = true;
	{
		//BumpMesh* ItemMesh = new BumpMesh();
		//ItemMesh->ReadMeshFromFile_OBJ("Resources/Mesh/BulletMag001.obj", vec4(1, 1, 1, 1), true);

		HumanoidAnimation animIdle;
		animIdle.LoadHumanoidAnimation("Resources/Animation/Idle.Humanoid_animation");
		HumanoidAnimationTable.push_back(animIdle); // Idle

		HumanoidAnimation animWalk;
		animWalk.LoadHumanoidAnimation("Resources/Animation/Walk.Humanoid_animation");
		HumanoidAnimationTable.push_back(animWalk); // Walk

		HumanoidAnimation animRun;
		animRun.LoadHumanoidAnimation("Resources/Animation/Run.Humanoid_animation");
		HumanoidAnimationTable.push_back(animRun);  // Run

		HumanoidAnimation animAim;
		animAim.LoadHumanoidAnimation("Resources/Animation/Aim.Humanoid_animation");
		HumanoidAnimationTable.push_back(animAim);  // 3: Aim

		HumanoidAnimation animShoot;
		animShoot.LoadHumanoidAnimation("Resources/Animation/Shoot.Humanoid_animation");
		HumanoidAnimationTable.push_back(animShoot); // 4: Shoot

		Model* PlayerModel = new Model();
		PlayerModel->LoadModelFile2("Resources/Model/Remy.model");
		PlayerModel->Retargeting_Humanoid(); // 휴머노이드 리타겟팅
		int playerMesh_index = Shape::AddModel("Player", PlayerModel);

		Model* MonsterModel = new Model();
		MonsterModel->LoadModelFile2("Resources/Model/Exo.model");
		MonsterModel->Retargeting_Humanoid();
		int monsterMesh_index = Shape::AddModel("Monster001", MonsterModel);

		Mesh* portalMesh = new Mesh();
		portalMesh->CreateWallMesh(2.0f, 3.0f, 0.2f, 0.0f);
		Shape::AddMesh("Portal", portalMesh);

		//Item 정의
		{
			int globalitem_index = 0;
			Shape BlackShape;
			BlackShape.FlagPtr = 0;
			ItemTable.push_back(Item(globalitem_index, ItemType::_NULL, vec4(1, 1, 1, 1), BlackShape, nullptr, nullptr, L"")); // blank space in inventory.
			globalitem_index += 1;

			auto AddItemFunc = [&](int id, ItemType t, const char* ItemName, const char* modelfilepath, const wchar_t* ItemIconPath, const wchar_t* ItemDescription) -> Model* {
				Model* ItemModel = new Model();
				ItemModel->LoadModelFile2(modelfilepath);
				int Item_shapeindex = Shape::AddModel(ItemName, ItemModel);
				GPUResource* ItemIcon = new GPUResource();
				ItemIcon->CreateTexture_fromFile(ItemIconPath, game.basicTexFormat, game.basicTexMip, false);
				ItemTable.push_back(Item(globalitem_index, t, vec4(1, 1, 1, 1), Shape::ShapeTable[Item_shapeindex], nullptr, ItemIcon, ItemDescription));
				return ItemModel;
				};


			AddItemFunc(globalitem_index, ItemType::_Consumable, "BioFix",
				"Resources/Model/ItemModel/BioFix.model",
				L"Resources/UI/ItemIcons/ItemIcon_BioFix.png",
				L"[바이오픽스] : 신체 조직을 재생하는 가성비 의료 주사기! \n HP+20");
			globalitem_index += 1;

			AddItemFunc(globalitem_index, ItemType::_Consumable, "Tier4Gear",
				"Resources/Model/ItemModel/Tier4Gear.model",
				L"Resources/UI/ItemIcons/ItemIcon_Tier4Gear.png",
				L"[티어4 부품] : 티어 4 도구 제작에 사용되는 주요재료. 많이 모으면 무기나 도구를 제작하는데 도움이 된다.");
			globalitem_index += 1;

			Model* Sniper_IronSight = AddItemFunc(globalitem_index, ItemType::_Weapon, "Sniper_IronSight",
				"Resources/Model/sniper.model",
				L"Resources/UI/ItemIcons/ItemIcon_IronSight.png",
				L"[스나이퍼 - 아이언사이트] : 광학 장비조차 제대로 달리지 않은 구식 실탄 저격소총.");
			globalitem_index += 1;
			game.SniperModel = Sniper_IronSight;

			// 라이플 모델 로드
			Model* Rifle_StreetSweeper = AddItemFunc(globalitem_index, ItemType::_Weapon, "Rifle_StreetSweeper",
				"Resources/Model/Rifle.model",
				L"Resources/UI/ItemIcons/ItemIcon_StreetSweeper.png",
				L"[돌격소총 - 스트리트스위퍼] : 한때 거리를 쓸어버렸던 클래식 돌격소총. 이제는 흔해 빠진 모델이다.");
			globalitem_index += 1;
			game.RifleModel = Rifle_StreetSweeper;

			// 권총 모델 로드
			Model* Pistol_DoubleTroble = AddItemFunc(globalitem_index, ItemType::_Weapon, "Pistol_DoubleTroble",
				"Resources/Model/pistol.model",
				L"Resources/UI/ItemIcons/ItemIcon_DoubleTroble.png",
				L"[쌍권총 - 더블트러블] : 성능은 낮지만 두 배로 쏘아붙이는 저가형 쌍권총.");
			globalitem_index += 1;
			game.PistolModel = Pistol_DoubleTroble;
			/*game.Pistol_SlideIndices.clear();
			{
				int upperIdx = game.PistolModel->FindNodeIndexByName("Upper_Part");
				if (upperIdx >= 0) {
					game.Pistol_SlideIndices.push_back(upperIdx);
					game.PistolModel->BindPose[upperIdx] = game.PistolModel->Nodes[upperIdx].transform;
				}
			}*/

			// 샷건 모델 로드
			Model* ShotGun_SlagShot = AddItemFunc(globalitem_index, ItemType::_Weapon, "ShotGun_SlagShot",
				"Resources/Model/shootgun.model",
				L"Resources/UI/ItemIcons/ItemIcon_SlagShot.png",
				L"[샷건 - 슬래그슛] : 제련 찌꺼기를 쏘는 듯한 거칠고 어려운 총. 구형 모델이라 성능도 그다지 좋지 않다.");
			globalitem_index += 1;
			game.ShotGunModel = ShotGun_SlagShot;
			/*game.SG_PumpIndices.clear();
			{
				int pumpIdx = game.ShotGunModel->FindNodeIndexByName("handguard_low");
				if (pumpIdx >= 0) {
					game.SG_PumpIndices.push_back(pumpIdx);
					game.ShotGunModel->BindPose[pumpIdx] = game.ShotGunModel->Nodes[pumpIdx].transform;
				}
			}*/

			// 머신건(미니건) 모델 로드
			Model* MachineGun_Ratler = AddItemFunc(globalitem_index, ItemType::_Weapon, "MachineGun_Ratler",
				"Resources/Model/minigun.model",
				L"Resources/UI/ItemIcons/ItemIcon_Ratler.png",
				L"[머신건 - 라틀러] : 낡은 부품이 덜덜거리는 소리에서 따온 이름. 언제 만들어졌는지 알 수 없다.");
			globalitem_index += 1;
			game.MachineGunModel = MachineGun_Ratler;
			/*game.MG_BarrelIndices.clear();
			{
				auto addBarrel = [&](const char* name) {
					int idx = game.MachineGunModel->FindNodeIndexByName(name);
					if (idx >= 0) game.MG_BarrelIndices.push_back(idx);
					};

				addBarrel("Cylinder.107");
				addBarrel("Cylinder.108");
				addBarrel("Cylinder.109");
				addBarrel("Cylinder.110");
			}*/
		}

		game.GunModel = game.SniperModel;
	}

	// 글로벌 에셋들의 개수를 서버와 동기화하기 위해 파일로 저장한다.
	GlobalTextureCount = TextureTable.size();
	GlobalMaterialCount = MaterialTable.size();
	GlobalMeshCount = MeshTable.size();
	GlobalHumanoidAnimationCount = HumanoidAnimationTable.size();

#ifdef DEVELOPMODE_SYNC_GLOBAL_ASSET
	ofstream GlobalAssetCountFile{ "../../GlobalAssetCounter.txt" };
	if (GlobalAssetCountFile.is_open()) {
		GlobalAssetCountFile << GlobalTextureCount << " ";
		GlobalAssetCountFile << GlobalMaterialCount << " ";
		GlobalAssetCountFile << GlobalMeshCount << " ";
		GlobalAssetCountFile << GlobalHumanoidAnimationCount << " ";
		GlobalAssetCountFile << Shape::ShapeTable.size();
	}
	GlobalAssetCountFile.close();
#endif

	gd.ShaderVisibleDescPool.ImmortalAlloc(&Immortal_ZoneLightBuffer_SRV, 1);

	// 기존 LoadMap 코드. 이제 안쓰일듯?
	if (false)
	{
		// 근데 이건 서버에서 Zone 을 움직이면 그때 가져와야 되는 거 아님?
	// 이제 존 마다 따로 가지고 있는 에셋들을 가져온다.

	// isAssetAddingInGlobal = false 면 이제 에셋이 존(맵)에 추가되기 시작한다. 그 신호를 준다.
		isAssetAddingInGlobal = false;
		Map = new GameMap();
		Map->LoadMap("The_Port");
		//Map->LoadMap("OfficeDungeon_1floor");
		game.StaticGameObjects.reserve(Map->MapObjects.size());
		for (int i = 0; i < Map->MapObjects.size(); ++i) {
			PushGameObject(Map->MapObjects[i]);
			game.StaticGameObjects.push_back(Map->MapObjects[i]);
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

		// 앞으로 쓰일 모든 글로벌 Asset들을 SVDescHeap에 올려놓는 작업을 한다.
		// 만약 이 크기가 너무 커지게 된다면 클라이언트에서 어떻게 조절을 할지도 생각해야 한다.
		for (int i = 0; i < GlobalMaterialCount; ++i) {
			Material* mat = MaterialTable[i];
			if (isAssetAddingInGlobal == false) {
				mat->SetDescTable();
				RenderMaterialTable.push_back(mat);
				// MaterialTable to DescIndex > CBResource.resource.descindex.index에 있음.
				// CBResource.resource.descindex > MaterialTable ?? 이거 필요한가? 
				// >> 필요한 상황이 떠오르지 않는데? 그냥 이대로 해도 괜찮지 않나?
			}
		}
	}

	UI_Init();

	gd.gpucmd.Close();
	gd.gpucmd.Execute();
	gd.WaitGPUComplete();

	isGlobalAssetInit = true;

	LightDirection = vec4(-1, -2, -1);
	LightDirection.len3 = 1;
}

void Game::AlignUIDepth()
{
	depthlevel_Count = mainPageStack.size();
	for (int i = 0; i < depthlevel_Count; ++i) {
		mainPageStack[i]->depth_min = GetDepth(i);
		mainPageStack[i]->depth_max = GetDepth(i + 1);
		mainPageStack[i]->AlignUIDepth();
	}
}

DXUI* Game::GetSlotUIFromPos(vec4 pos)
{
	DXUI* selected = nullptr;
	for (int i = CurrentPageStack->size() - 1; i >= 0; ++i) {
		DXPage* page = CurrentPageStack->at(i);
		selected = page->GetSlotUIFromPos(pos);
		if (selected != nullptr) return selected;
	}
	return nullptr;
}

void Game::MoveZone(int zoneid) {
	//Release Prev Zone Shapes
	bool CmdInitStateIsClose = gd.gpucmd.isClose;
	if (CmdInitStateIsClose) {
		gd.gpucmd.Reset();
	}
	gd.ShaderVisibleDescPool.DynamicReset();

	// 모든 DynamicGameObject와 StaticGameObject, GameChunck 들을 삭제한다.
	for (int i = 0; i < StaticGameObjects.size(); ++i) {
		if (StaticGameObjects[i]) {
			StaticGameObjects[i]->Release();
			delete StaticGameObjects[i];
			StaticGameObjects[i] = nullptr;
		}
	}
	StaticGameObjects.clear();
	for (int i = 0; i < DynmaicGameObjects.size(); ++i) {
		if (DynmaicGameObjects[i]) {
			DynmaicGameObjects[i]->Release();
			delete DynmaicGameObjects[i];
			DynmaicGameObjects[i] = nullptr;
		}
	}
	DynmaicGameObjects.clear();

	GameChunk** gcarr = new GameChunk * [chunck.size()];
	int index = 0;
	for (auto c : chunck) {
		GameChunk* gc = c.second;
		gcarr[index] = gc;
		index += 1;
	}
	chunck.clear();

	for (int i = 0; i < index; ++i) {
		GameChunk* gc = gcarr[i];
		gc->Release();
		delete gc;
		gc = nullptr;
	}

	int last = 0;
	//Zone에 존재하는 Shape 들을 모두 삭제한다.
	if (Map) {
		int i = Map->StartShapeIndex;
		for (; i < Shape::ShapeTable.size(); ++i) {
			Mesh* mesh = nullptr;
			Model* model = nullptr;
			Shape::ShapeTable[i].GetRealShape(mesh, model);
			if (mesh) {
				mesh->Release();
				delete mesh;
			}
			else if (model) {
				model->Release();
				delete model;
			}
		}
		last = Shape::ShapeTable.size() - 1;
		for (; last >= Map->StartShapeIndex; --last) {
			Shape::ShapeTable.erase(Shape::ShapeTable.begin() + last);
		}
	}

	//Release Prev Zone Assets
	last = TextureTable.size() - 1;
	for (; last >= GlobalTextureCount; --last) {
		TextureTable.erase(TextureTable.begin() + last);
	}
	last = MaterialTable.size() - 1;
	for (; last >= GlobalMaterialCount; --last) {
		MaterialTable.erase(MaterialTable.begin() + last);
	}
	last = MeshTable.size() - 1;
	for (; last >= GlobalMeshCount; --last) {
		MeshTable.erase(MeshTable.begin() + last);
	}
	last = HumanoidAnimationTable.size() - 1;
	for (; last >= GlobalHumanoidAnimationCount; --last) {
		HumanoidAnimationTable.erase(HumanoidAnimationTable.begin() + last);
	}
	// Clear Render Asset Table And Clear SVDescHeap
	RenderTextureTable.clear();
	RenderMaterialTable.clear();
	RenderInstancingTable.clear();
	gd.ShaderVisibleDescPool.ImmortalReset_ExcludeInit();

	//ReLoadMap
	if (Map) {
		Map->Release();
	}
	else {
		Map = new GameMap();
	}

	// isAssetAddingInGlobal = false 면 이제 에셋이 존(맵)에 추가되기 시작한다. 그 신호를 준다.
	isAssetAddingInGlobal = false;

	Map->LoadMap(ZoneIDToMapName[zoneid]);
	game.StaticGameObjects.reserve(Map->MapObjects.size());
	Map->AABB[0] = INFINITY;
	Map->AABB[1] = -INFINITY;
	for (int i = 0; i < Map->MapObjects.size(); ++i) {
		PushGameObject(Map->MapObjects[i]);
		game.StaticGameObjects.push_back(Map->MapObjects[i]);

		BoundingOrientedBox obb = Map->MapObjects[i]->GetOBB();
		XMFLOAT3 corners[8];
		obb.GetCorners(corners);
		for (int k = 0; k < 8; ++k) {
			vec4 c = corners[k];
			Map->AABB[0] = _mm_min_ps(c, Map->AABB[0]);
			Map->AABB[1] = _mm_max_ps(c, Map->AABB[1]);
		}
	}
	Map->AABB[0].w = -INFINITY;
	Map->AABB[1].w = INFINITY;

	for (int i = 0; i < game.LightTable.size(); ++i) {
		game.PushLight(game.LightTable[i]);
	}


	// 앞으로 쓰일 모든 글로벌 Asset들을 SVDescHeap에 올려놓는 작업을 한다.
	// 만약 이 크기가 너무 커지게 된다면 클라이언트에서 어떻게 조절을 할지도 생각해야 한다.
	for (int i = 0; i < GlobalMaterialCount; ++i) {
		Material* mat = MaterialTable[i];
		if (isAssetAddingInGlobal == false) {
			mat->SetDescTable();
			RenderMaterialTable.push_back(mat);
			// MaterialTable to DescIndex > CBResource.resource.descindex.index에 있음.
			// CBResource.resource.descindex > MaterialTable ?? 이거 필요한가? 
			// >> 필요한 상황이 떠오르지 않는데? 그냥 이대로 해도 괜찮지 않나?
		}
	}

	gd.gpucmd.Close();
	gd.gpucmd.Execute();
	gd.gpucmd.WaitGPUComplete();
	if (CmdInitStateIsClose == false) {
		gd.gpucmd.Reset();
	}

	isMapInit = true;

	// Zone의 에셋을 모두 불러왔으니 다시 공용 에셋을 로드하는 모드로 변경한다.
	isAssetAddingInGlobal = true;
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

	if (game.bReqireBakeLight_Raster) {
		if (ZoneLightChuncks.resource) ZoneLightChuncks.Release();
		if (ZoneLightChuncks_Mapped) ZoneLightChuncks_Mapped = nullptr;

		vec4 Counts = (Map->AABB[1] - Map->AABB[0]) / game.chunck_divide_Width;
		ChunckCountX = floor(Counts.x + 1);
		ChunckCountY = floor(Counts.y + 1);
		ChunckCountZ = floor(Counts.z + 1);

		int ix = floor(Map->AABB[0].x / chunck_divide_Width);
		int iy = floor(Map->AABB[0].y / chunck_divide_Width);
		int iz = floor(Map->AABB[0].z / chunck_divide_Width);
		ChunkIndex startci = ChunkIndex(ix, iy, iz);
		BoundingBox bb = startci.GetAABB();
		LightCBData_withShadow->ChunckStart = bb.Center;
		LightCBData_withShadow->ChunckStart -= vec4(bb.Extents);
		LightCBData_withShadow->ChunckCount[0] = ChunckCountX;
		LightCBData_withShadow->ChunckCount[1] = ChunckCountY;
		LightCBData_withShadow->ChunckCount[2] = ChunckCountZ;

		UINT ncbElementBytes = ((sizeof(ChunckLightData) * (ChunckCountX * ChunckCountY * ChunckCountZ) + 255) & ~255); //256의 배수
		ZoneLightChuncks = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1);
		ZoneLightChuncks.resource->Map(0, NULL, (void**)&ZoneLightChuncks_Mapped);
		ZeroMemory(ZoneLightChuncks_Mapped, ncbElementBytes);
		for (auto f : chunck) {
			GameChunk* gc = f.second;
			ChunkIndex ci = f.first;
			ChunkIndex rci;
			rci.x = ci.x - startci.x;
			rci.y = ci.y - startci.y;
			rci.z = ci.z - startci.z;
			int index = rci.z + rci.y * ChunckCountZ + rci.x * ChunckCountZ * ChunckCountY;
			ChunckLightData& LightChunck = ZoneLightChuncks_Mapped[index];
			int maxSiz = min(gc->Lights.size(), 32);
			for (int i = 0; i < maxSiz; ++i) {
				LightChunck.lights[i] = *gc->Lights[i];
			}
			LightChunck.lights[0].MaxLightCount = maxSiz;
		}
		ZoneLightChuncks.resource->Unmap(0, nullptr);

		//MaterialStructuredBufferSRV를 재할당하지 않는다. (같은 자리를 차지한다.)
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = ChunckCountX * ChunckCountY * ChunckCountZ;
		srvDesc.Buffer.StructureByteStride = sizeof(ChunckLightData);
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		gd.pDevice->CreateShaderResourceView(ZoneLightChuncks.resource, &srvDesc, Immortal_ZoneLightBuffer_SRV.hCreation.hcpu);

		game.bReqireBakeLight_Raster = false;
	}

	for (int i = 0; i < gd.addSDFTextureStack.size(); ++i) {
		gd.AddTextSDFTexture(gd.addSDFTextureStack[i]);
	}
	gd.addSDFTextureStack.clear();

	for (int i = gd.SDFTextureArr_immortalSize; i < gd.SDFTextureArr.size(); ++i) {
		SDFTextPageTextureBuffer* page = gd.SDFTextureArr[i];
		page->BakeSDF();
	}
	game.MyScreenShader->ClearSDFInstance();
	game.MyScreenShader->SDFInstance_StructuredBuffer.resource->Map(0, nullptr, (void**)&game.MyScreenShader->MappedSDFInstance);
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

	if (gd.gpucmd.isClose == false) {
		gd.gpucmd.Close();
		gd.gpucmd.Execute(true);
		gd.gpucmd.WaitGPUComplete();
	}
	gd.addSDFTextureStack.clear();

	//5. 쉐도우 패스
	Render_ShadowPass();

	//6. 렌더패스 시작, 커맨드리스트 리셋
	HRESULT hResult = gd.gpucmd.Reset();

	//7. 쉐도우 맵의 STATE 를 PIXEL SHADER RESOURCE로 변환 (쉐도우 맵으로 쓰기 위해서)
	for (int i = 0; i < 3; ++i) {
		gd.gpucmd.ResBarrierTr(&game.MyDirLight[i].ShadowMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	//8. 서브렌더타겟의 STATE를 PRESENT에서 RENDER TARGET으로 변환 (서브렌더타겟에 그려야 되서)
	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET);

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

	auto BindStaticPBRRenderState = [&]() {
		matrix pbrView = gd.viewportArr[0].ViewMatrix;
		pbrView *= gd.viewportArr[0].ProjectMatrix;
		pbrView.transpose();

		gd.gpucmd.SetShader(MyPBRShader1, ShaderType::RenderWithShadow);
		game.PresentShaderType = ShaderType::RenderWithShadow;
		gd.gpucmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
		{
			using PRID = PBRShader1::RootParamId;
			gd.gpucmd->SetGraphicsRoot32BitConstants(PRID::Const_Camera, 16, &pbrView, 0);
			gd.gpucmd->SetGraphicsRoot32BitConstants(PRID::Const_Camera, 4, &gd.viewportArr[0].Camera_Pos, 16);
			gd.gpucmd->SetGraphicsRootConstantBufferView(PRID::CBV_StaticLight, game.LightCB_withShadowResource.resource->GetGPUVirtualAddress());
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_ShadowMap, game.MyDirLight[0].descindex.hRender.hgpu);
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_EnvionmentMap, game.MySkyBoxShader->CurrentSkyBox.descindex.hRender.hgpu);
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_Chunck_StaticLightStructuredBuffer, game.Immortal_ZoneLightBuffer_SRV.hRender.hgpu);
		}
		};

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
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_Instancing_ShadowMap, game.MyDirLight[0].descindex.hRender.hgpu);
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
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_ShadowMap, game.MyDirLight[0].descindex.hRender.hgpu);
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_EnvionmentMap, game.MySkyBoxShader->CurrentSkyBox.descindex.hRender.hgpu);
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_Chunck_StaticLightStructuredBuffer, game.Immortal_ZoneLightBuffer_SRV.hRender.hgpu);
		}

		RenderTour<false>();

		//temp code
		{
			matrix idmat = XMMatrixScaling(2, 2, 2);
			idmat.pos.y = 10;
			SniperModel->Render<false>(gd.gpucmd, idmat, nullptr);
		}

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
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_SkinMeshShadowMaps, game.MyDirLight[0].descindex.hRender.hgpu);
			// 18-4. 환경맵 바인딩
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_SKinMeshEnvironmentMap, game.MySkyBoxShader->CurrentSkyBox.descindex.hRender.hgpu);
			// 18-5. 청크 스테틱 라이트 정보
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_SKinMesh_Chunck_StaticLightStructuredBuffer, game.Immortal_ZoneLightBuffer_SRV.hRender.hgpu);
		}
		RenderTour<true>();

		if (game.player != nullptr && game.bFirstPersonVision == false) {
			BindStaticPBRRenderState();
			game.player->Render_ThirdPersonWeapon();
		}
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



	if (game.DebugCollisions) {
		using OCSRP = OnlyColorShader::RootParamId;
		gd.gpucmd.SetShader(MyOnlyColorShader, ShaderType::Debug_OBB);
		gd.gpucmd->SetGraphicsRoot32BitConstants(OCSRP::Const_Camera, 16, &view, 0);
		int cnt = 0;
		for (int i = 0; i < game.StaticGameObjects.size(); ++i) {
			StaticGameObject* sto = game.StaticGameObjects[i];

			if (true) {
				if (sto->obbArr.size() == 0) {
					BoundingOrientedBox obb = sto->GetOBB();
					if (obb.Extents.x <= 0) continue;
					matrix worldMat = XMMatrixScaling(obb.Extents.x, obb.Extents.y, obb.Extents.z);
					worldMat.trQ(obb.Orientation);
					worldMat.pos.f3 = obb.Center;
					worldMat.pos.w = 1;
					worldMat.transpose();
					gd.gpucmd->SetGraphicsRoot32BitConstants(OCSRP::Const_Transform, 16, &worldMat, 0);
					game.OBBDebugMesh->Render(gd.gpucmd, 1, 0);
				}
				else {
					for (int k = 0; k < sto->obbArr.size(); ++k) {
						BoundingOrientedBox obb = sto->obbArr[k];
						if (obb.Extents.x <= 0) continue;
						matrix worldMat = XMMatrixScaling(obb.Extents.x, obb.Extents.y, obb.Extents.z);
						worldMat.trQ(obb.Orientation);
						worldMat.pos.f3 = obb.Center;
						worldMat.pos.w = 1;
						worldMat.transpose();
						gd.gpucmd->SetGraphicsRoot32BitConstants(OCSRP::Const_Transform, 16, &worldMat, 0);
						game.OBBDebugMesh->Render(gd.gpucmd, 1, 0);
					}
				}
			}
		}
	}

	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	gd.gpucmd.ResBarrierTr(gd.pDepthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	//Command execution
	// 24-1. ������ ���� �޴� ��ü���� ��Ǻ�� + Depth�� ���� ������� ����
	// 24-2. GPU�� �ִϸ��̼� ������ ���
	hResult = gd.gpucmd.Close();
	gd.gpucmd.Execute();
	gd.gpucmd.WaitGPUComplete();

	//Bluring + DepthConvolution(�ָ��ִ� ��ü�� �帮�� �����.) (Compute Shader)
	hResult = gd.CScmd.Reset();
	gd.CScmd.SetShader(MyComputeTestShader);
	gd.CScmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
	float WHarr[2] = { gd.ClientFrameWidth , gd.ClientFrameHeight };
	gd.CScmd->SetComputeRoot32BitConstants(0, 2, WHarr, 0); // screen width height

	//gd.SubRenderTarget.srvHandle.hRender.hgpu
	gd.CScmd->SetComputeRootDescriptorTable(1, gd.raytracing.RTO_UAV_index.hRender.hgpu); // UAV sub RenderTarget
	gd.CScmd->SetComputeRootDescriptorTable(2, gd.MainDS_SRV.hRender.hgpu); // SRV DS
	gd.CScmd->SetComputeRootDescriptorTable(3, gd.BlurTexture.handle.hRender.hgpu); // UAV output
	int disPatchW = (gd.ClientFrameWidth / 8) + 1;
	int disPatchH = (gd.ClientFrameHeight / 8) + 1;
	gd.CScmd->Dispatch(disPatchW, disPatchH, 1);

	if constexpr (gd.PlayAnimationByGPU) {
		//Adding Animation Compute Shaders Execute Command
		SkinMeshGameObject::collection.clear();
		SkinMeshGameObject::CurrentRenderFunc = &SkinMeshGameObject::CollectSkinMeshObject;
		RenderTour<true>();
		gd.CScmd.SetShader(game.MyAnimationBlendingShader);
		for (int i = 0; i < SkinMeshGameObject::collection.size(); ++i) {
			SkinMeshGameObject::collection[i]->BlendingAnimation();
		}
		gd.CScmd.SetShader(game.MyHBoneLocalToWorldShader);
		for (int i = 0; i < SkinMeshGameObject::collection.size(); ++i) {
			SkinMeshGameObject::collection[i]->ModifyLocalToWorld();
		}
	}

	hResult = gd.CScmd.Close();
	gd.CScmd.Execute();
	gd.CScmd.WaitGPUComplete();

	gd.gpucmd.Reset();
	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET);
	gd.gpucmd.ResBarrierTr(gd.pDepthStencilBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	//9. ����Ʈ/������Ʈ ����
	gd.gpucmd->RSSetViewports(1, &gd.viewportArr[0].Viewport);
	gd.gpucmd->RSSetScissorRects(1, &gd.viewportArr[0].ScissorRect);
	game.renderViewPort = &gd.viewportArr[0];

	//10. ���� ���ٽ� ���۸� ����Ű�� �ڵ��� �����´�.
	d3dDsvCPUDescriptorHandle =
		gd.pDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	//11. ���귻��Ÿ������ ����Ÿ���� ����
	gd.gpucmd->OMSetRenderTargets(1, &gd.SubRenderTarget.rtvHandle.hcpu, TRUE,
		&d3dDsvCPUDescriptorHandle);

	// 24. UI 출력을 위해 DepthStencil을 Clear한다. (모든 UI는 이전에 그렸던 모든 것 위에 그려져야 하기 때문)
	gd.gpucmd->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);

	{
		//using OCSRP = OnlyColorShader::RootParamId;
		//gd.gpucmd.SetShader(MyOnlyColorShader, ShaderType::Debug_OBB);
		//gd.gpucmd->SetGraphicsRoot32BitConstants(OCSRP::Const_Camera, 16, &view, 0);
		//matrix worldMat = XMMatrixScaling(LightOBB.Extents.x, LightOBB.Extents.y, LightOBB.Extents.z);
		//worldMat.trQ(LightOBB.Orientation);
		//worldMat.pos.f3 = LightOBB.Center;
		//worldMat.pos.w = 1;
		//worldMat.transpose();
		//gd.gpucmd->SetGraphicsRoot32BitConstants(OCSRP::Const_Transform, 16, &worldMat, 0);
		//game.OBBDebugMesh->Render(gd.gpucmd, 1, 0);

		//for (int i = 0; i < 8; ++i) {
		//	BoundingOrientedBox obb;
		//	obb = BoundingOrientedBox(ViewportData::PresentFrustumCorner[i].f3, { 0.1f, 0.1f, 0.1f }, vec4(0, 0, 0, 1));
		//	worldMat = XMMatrixScaling(obb.Extents.x, obb.Extents.y, obb.Extents.z);
		//	worldMat.trQ(obb.Orientation);
		//	worldMat.pos.f3 = obb.Center;
		//	worldMat.pos.w = 1;
		//	worldMat.transpose();
		//	gd.gpucmd->SetGraphicsRoot32BitConstants(OCSRP::Const_Transform, 16, &worldMat, 0);
		//	game.OBBDebugMesh->Render(gd.gpucmd, 1, 0);
		//}
	}

	// 25. 플레이어의 일부가 가장 앞에 렌더링되어야 할때 렌더링 (ex> 총기)
	float hhpp = 0;
	float HeatGauge = 0;
	int kill = 0;
	int death = 0;
	int bulletCount = 0;
	float SkillCooldownFlow = 0;
	if (game.player != nullptr) {
		BindStaticPBRRenderState();
		game.player->Render_AfterDepthClear();
		hhpp = game.player->HP;
		HeatGauge = game.player->HeatGauge;
		kill = game.player->KillCount;
		death = game.player->DeathCount;
		bulletCount = game.player->bullets;
		SkillCooldownFlow = game.player->SkillCooldownFlow[(int)SkillSlot::Skill1];
	}

	// 26. UI 텍스트 렌더링
	float Rate = gd.ClientFrameHeight / 960.0f;
	vec4 rt = Rate * vec4(-1650, 850, -1000, 700);
	game.RenderSDFText(L"이어가그요다", 6, rt, 30, vec4(1, 1, 1, 1), nullptr, nullptr, 0.001f);

	gd.gpucmd.SetShader(game.MyScreenShader, ShaderType::RenderNormal);
	vector<DXPage*>* savePageStack = game.CurrentPageStack;
	game.CurrentPageStack = &game.mainPageStack;
	for (int i = 0; i < game.CurrentPageStack->size(); ++i) {
		DXPage* page = game.CurrentPageStack->at(i);
		page->Render();
	}
	// 27.
	MyScreenShader->RenderAllSDFTexts();

	//std::wstring ui_hp = L"HP: " + std::to_wstring(hhpp);
	//RenderText(ui_hp.c_str(), ui_hp.length(), rt, 30);
	////Heat Gauge
	//vec4 rt_heat = rt;
	//rt_heat.y -= 80 * Rate;   // HP보다 약간 아래로 내림 (간격 50 정도)
	//std::wstring ui_heat = L"Heat: " + std::to_wstring((int)HeatGauge);
	//RenderText(ui_heat.c_str(), ui_heat.length(), rt_heat, 30);
	////Skill
	//	rt = Rate * vec4(-900, 850, -200, 700);
	//std::wstring ui_cool = L"[Q] Heal CD: " + std::to_wstring((int)SkillCooldownFlow);
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

	//// ----------Inventory------------
	//if (isInventoryOpen) {
	//	matrix orthoMatrix = XMMatrixOrthographicOffCenterLH(0.0f, (float)gd.ClientFrameWidth, (float)gd.ClientFrameHeight, 0.0f, 0.01f, 1.0f);
	//	matrix uiViewMat = orthoMatrix;
	//	uiViewMat.transpose();
	//	((Shader*)MyScreenShader)->Add_RegisterShaderCommand(gd.gpucmd);
	//	MyScreenShader->SetTextureCommand(&DefaultTex);
	//	/*gd.gpucmd->SetPipelineState(MyOnlyColorShader->pUiPipelineState);*/
	//	//gd.gpucmd->SetGraphicsRoot32BitConstants(0, 16, &uiViewMat, 0);

	//	const int gridColumns = 5;
	//	const int gridRows = 5;
	//	const float slotSize = Rate * 150.0f;
	//	const float itemPadding = Rate * 20.0f;
	//	const float slotPadding = Rate * 10.0f;

	//	const float actualItemSize = slotSize - (itemPadding * 2);

	//	float invWidth = (gridColumns * slotSize) + ((gridColumns + 1) * slotPadding);
	//	float invHeight = (gridRows * slotSize) + ((gridRows + 1) * slotPadding);
	//	float invPosX = 0;
	//	float invPosY = 0;

	//	vec4 bgColor = { 0.2f, 0.2f, 0.2f, 0.8f };
	//	float CB[11] = { invPosX - invWidth ,invPosY - invHeight , invPosX + invWidth ,invPosY + invHeight, bgColor.r, bgColor.g, bgColor.b, bgColor.a, gd.ClientFrameWidth, gd.ClientFrameHeight, 0.5f };
	//	gd.gpucmd->SetGraphicsRoot32BitConstants(0, 11, CB, 0);
	//	TextMesh->Render(gd.gpucmd, 1);

	//	vec4 slotColor = { 0.15f, 0.15f, 0.15f, 0.9f };

	//	float startSlotX = invPosX - invWidth + slotPadding + slotSize;
	//	float startSlotY = invPosY - invHeight + slotPadding + slotSize;

	//	for (int row = 0; row < gridRows; ++row) {
	//		for (int col = 0; col < gridColumns; ++col) {
	//			float slotCurrentX = startSlotX + col * (2 * (slotSize + slotPadding));
	//			float slotCurrentY = startSlotY + row * (2 * (slotSize + slotPadding));

	//			float CB2[11] = { slotCurrentX - slotSize ,slotCurrentY - slotSize , slotCurrentX + slotSize ,slotCurrentY + slotSize, slotColor.r, slotColor.g, slotColor.b, slotColor.a, gd.ClientFrameWidth, gd.ClientFrameHeight, 0.05f };
	//			gd.gpucmd->SetGraphicsRoot32BitConstants(0, 11, CB2, 0);
	//			TextMesh->Render(gd.gpucmd, 1);
	//		}
	//	}

	//	float startItemX = invPosX - Rate * 140.0f;
	//	float startItemY = invPosY - Rate * 130.0f;

	//	matrix viewMat2 = DirectX::XMMatrixLookAtLH(vec4(0, 0, 0), vec4(0, 0, 1), vec4(0, 1, 0));
	//	viewMat2 *= gd.viewportArr[0].ProjectMatrix;
	//	//gd.gpucmd->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
	//	//	D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);
	//	////((Shader*)MyDiffuseTextureShader)->Add_RegisterShaderCommand(gd.gpucmd);
	//	//gd.gpucmd->SetGraphicsRoot32BitConstants(0, 16, &viewMat, 0);
	//	//gd.gpucmd->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16);
	//	//gd.gpucmd->SetGraphicsRootConstantBufferView(2, LightCBResource.resource->GetGPUVirtualAddress());
	//	//for (int i = 0; i < player->maxItem; ++i) {
	//	//	if (i >= gridColumns * gridRows)
	//	//		break;
	//	//	ItemStack& currentStack = player->Inventory[i];
	//	//	ItemID currentItemID = currentStack.id;
	//	//	if (currentItemID == 0) continue;
	//	//	if (currentItemID >= ItemTable.size())
	//	//		continue;
	//	//	Item& itemInfo = ItemTable[currentItemID];
	//	//	//Mesh* itemMesh = GetOrCreateColoredQuadMesh(itemInfo.color);
	//	//	int column = i % gridColumns;
	//	//	int row = i / gridColumns;
	//	//	float itemCurrentX = startItemX + column * (2 * (slotSize + slotPadding));
	//	//	float itemCurrentY = startItemY + row * (2 * (slotSize + slotPadding));
	//	//	/*vec4 v = gd.viewportArr[0].unproject(vec4(gd.ClientFrameWidth/2, gd.ClientFrameHeight/2, 0, 1));
	//	//	v *= 5;
	//	//	v += gd.viewportArr[0].Camera_Pos;*/
	//	//	/*vec4 unproj = gd.viewportArr[0].unproject(vec4(-0.5f, 0.5f, 0, 1));*/
	//	//	matrix itemMat;
	//	//	itemMat.pos = gd.viewportArr[0].Camera_Pos;
	//	//	//caminvMat.look.x *= -1;
	//	//	itemMat.pos += viewMat.look * 7;
	//	//	constexpr float xmul = 1.35f;
	//	//	constexpr float ymul = 0.825f;
	//	//	itemMat.pos += viewMat.right * (-2.7f + xmul * column);
	//	//	itemMat.pos += viewMat.up * (1.65f - ymul * row);
	//	//	itemMat.pos.w = 1;
	//	//	itemMat.transpose();
	//	//	gd.gpucmd->SetGraphicsRoot32BitConstants(1, 16, &itemMat, 0);
	//	//	MyDiffuseTextureShader->SetTextureCommand(ItemTable[currentItemID].tex);
	//	//	ItemTable[currentItemID].MeshInInventory->Render(gd.gpucmd, 1);
	//	//	//RenderUIObject(itemMesh, itemMat);
	//	//}

	//	gd.gpucmd->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
	//		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);
	//	((Shader*)MyScreenShader)->Add_RegisterShaderCommand(gd.gpucmd);

	//	int i = 0;
	//	float top = startSlotY + (gridRows - 1) * (2 * (slotSize + slotPadding));
	//	for (int row = 0; row < gridRows; ++row) {
	//		for (int col = 0; col < gridColumns; ++col) {
	//			if (player->Inventory[i].id == 0) {
	//				i++; continue;
	//			}

	//			float slotCurrentX = startSlotX + col * (2 * (slotSize + slotPadding));
	//			float slotCurrentY = top - row * (2 * (slotSize + slotPadding));

	//			ItemStack& currentStack = player->Inventory[i];
	//			std::wstring countStr = L"x" + std::to_wstring(currentStack.ItemCount);
	//			vec4 textRect = { slotCurrentX - slotSize ,slotCurrentY - slotSize , slotCurrentX + slotSize ,slotCurrentY + slotSize };
	//			float fontSize = 20.0f;
	//			RenderText(countStr.c_str(), countStr.length(), textRect, fontSize, 0.01f);
	//			i++;
	//		}
	//	}
	//}

	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
	gd.gpucmd.ResBarrierTr(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);

	gd.gpucmd->CopyResource(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], gd.SubRenderTarget.resource);

	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	gd.gpucmd.ResBarrierTr(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);

	//Command execution
	hResult = gd.gpucmd.Close();
	gd.gpucmd.Execute();
	gd.gpucmd.WaitGPUComplete();

	// Present to Swapchain BackBuffer & RenderTargetIndex Update
	DXGI_PRESENT_PARAMETERS dxgiPresentParameters;
	dxgiPresentParameters.DirtyRectsCount = 0;
	dxgiPresentParameters.pDirtyRects = NULL;
	dxgiPresentParameters.pScrollRect = NULL;
	dxgiPresentParameters.pScrollOffset = NULL;
	gd.pSwapChain->Present1(1, 0, &dxgiPresentParameters);

	//Get Present RenderTarget Index
	gd.CurrentSwapChainBufferIndex = gd.pSwapChain->GetCurrentBackBufferIndex();

	gd.DeviceRemoveResonDebug();
}

void Game::Render_RayTracing()
{
	gd.raytracing.MappedCB->DirLight_invDirection = vec4(vec4(0) - LightDirection).f3;
	if (bReqireBakeLight_Raytracing) {
		vec4 Counts = (Map->AABB[1] - Map->AABB[0]) / game.chunck_divide_Width;
		ChunckCountX = floor(Counts.x + 1);
		ChunckCountY = floor(Counts.y + 1);
		ChunckCountZ = floor(Counts.z + 1);

		int ix = floor(Map->AABB[0].x / chunck_divide_Width);
		int iy = floor(Map->AABB[0].y / chunck_divide_Width);
		int iz = floor(Map->AABB[0].z / chunck_divide_Width);
		ChunkIndex startci = ChunkIndex(ix, iy, iz);
		BoundingBox bb = startci.GetAABB();
		gd.raytracing.MappedCB->ChunckStart = bb.Center;
		gd.raytracing.MappedCB->ChunckStart -= vec4(bb.Extents);
		gd.raytracing.MappedCB->ChunckCount[0] = ChunckCountX;
		gd.raytracing.MappedCB->ChunckCount[1] = ChunckCountY;
		gd.raytracing.MappedCB->ChunckCount[2] = ChunckCountZ;
		bReqireBakeLight_Raytracing = false;
	}

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
	commandList->SetComputeRootDescriptorTable(1, gd.raytracing.MainDepth_UAV.hRender.hgpu); // DSV

	commandList->SetComputeRootShaderResourceView(2, MyRayTracingShader->TLAS->GetGPUVirtualAddress()); // AS SRV
	commandList->SetComputeRootConstantBufferView(3, gd.raytracing.CameraCB->GetGPUVirtualAddress()); // Camera CB CBV
	commandList->SetComputeRootDescriptorTable(4, RayTracingMesh::VBIB_DescIndex.hRender.hgpu); // Vertex, IndexBuffer SRV
	commandList->SetComputeRootDescriptorTable(5, MySkyBoxShader->CurrentSkyBox.descindex.hRender.hgpu); // SkyBox SRV
	commandList->SetComputeRootDescriptorTable(6, RayTracingMesh::UAV_VBIB_DescIndex.hRender.hgpu); // SkinMesh SRV

	commandList->SetComputeRootDescriptorTable(7, Material::MaterialStructuredBufferSRV.hRender.hgpu); // Material Arr
	DescIndex texarrSRV = DescIndex(true, gd.ShaderVisibleDescPool.TextureSRVStart);
	commandList->SetComputeRootDescriptorTable(8, game.Immortal_ZoneLightBuffer_SRV.hRender.hgpu); // ZoneLightChuncks Arr
	commandList->SetComputeRootDescriptorTable(9, texarrSRV.hRender.hgpu); // Texture Arr

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

	gd.gpucmd.Close(true);
	gd.gpucmd.Execute(true);
	gd.gpucmd.WaitGPUComplete();

	HRESULT hResult;
	//Blur
	{
		//Bluring (Compute Shader)
		hResult = gd.CScmd.Reset();
		gd.CScmd.SetShader(MyComputeTestShader);
		gd.CScmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
		float WHarr[2] = { gd.ClientFrameWidth , gd.ClientFrameHeight };
		gd.CScmd->SetComputeRoot32BitConstants(0, 2, WHarr, 0); // screen width height
		//gd.SubRenderTarget.srvHandle.hRender.hgpu
		gd.CScmd->SetComputeRootDescriptorTable(1, gd.raytracing.RTO_UAV_index.hRender.hgpu); // UAV SubRenderTarget
		gd.CScmd->SetComputeRootDescriptorTable(2, gd.raytracing.MainDepth_SRV.hRender.hgpu); // SRV DS
		gd.CScmd->SetComputeRootDescriptorTable(3, gd.BlurTexture.handle.hRender.hgpu); // UAV output
		int disPatchW = (gd.ClientFrameWidth / 8) + 1;
		int disPatchH = (gd.ClientFrameHeight / 8) + 1;
		gd.CScmd->Dispatch(disPatchW, disPatchH, 1);

		hResult = gd.CScmd.Close();
		gd.CScmd.Execute();
		gd.CScmd.WaitGPUComplete();

		//gd.DeviceRemoveResonDebug();
	}

	// retuen to graphic command list. to copy resource to render back buffer;
	hResult = gd.gpucmd.Reset(true);

	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	gd.gpucmd.ResBarrierTr(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);

	gd.gpucmd->CopyResource(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], gd.SubRenderTarget.resource);

	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	gd.gpucmd.ResBarrierTr(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);

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
	static vector<SkinMeshGameObject*> ShadowRenderSkinMeshObjArr;
	// 0. 프러스텀을 모두 포함하면서 Extent.z 방향이 빛 방향인 OBB를 구성
	constexpr float CascadeRange[4] = { 0.01f, 50.0f, 200.0f, 1000.0f };

	// 2. 렌더링을 시작한다.
	HRESULT hResult = gd.gpucmd.Reset();

	for (int i = 0; i < 3; ++i) {
		matrix viewproj;
		viewproj = gd.viewportArr[0].ViewMatrix;
		matrix proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), (float)gd.ClientFrameWidth / (float)gd.ClientFrameHeight, CascadeRange[i], CascadeRange[i + 1]);
		viewproj *= proj;
		vec4 LightDirQ = vec4::DirectionToQuaternion(LightDirection);
		LightOBB = game.MyDirLight[0].viewport.GetOBB_IncludeFrustum(viewproj, LightDirQ);

		// 1. 디렉션 라이트를 초기화
		constexpr int ShadowResolusion = 4096;
		constexpr float LightDistance = 1000;
		vec4 obj = LightOBB.Center;
		obj += LightDirection * LightOBB.Extents.z;

		float MaxWidth = max(LightOBB.Extents.x, LightOBB.Extents.y) * 2;
		game.MyDirLight[i].viewport.Viewport.Width = ShadowResolusion;
		game.MyDirLight[i].viewport.Viewport.Height = ShadowResolusion;
		game.MyDirLight[i].viewport.Viewport.MaxDepth = 1.0f;
		game.MyDirLight[i].viewport.Viewport.MinDepth = 0.0f;
		game.MyDirLight[i].viewport.Viewport.TopLeftX = 0.0f;
		game.MyDirLight[i].viewport.Viewport.TopLeftY = 0.0f;
		game.MyDirLight[i].viewport.ScissorRect = { 0, 0, (long)ShadowResolusion, (long)ShadowResolusion };

		// 1-1 카메라 방향 정하기

		game.MyDirLight[i].viewport.Camera_Pos = obj - (LightDirection * LightDistance);
		game.MyDirLight[i].viewport.Camera_Pos.w = 0;
		game.MyDirLight[i].LightPos = game.MyDirLight[i].viewport.Camera_Pos;

		vec4 up = vec4(0, 1, 0, 0);
		if (fabs(LightDirection.dot3(up)) > 0.99f) {
			up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
		}
		MyDirLight[i].View.mat = XMMatrixLookAtLH(MyDirLight[i].LightPos, obj, up);
		game.MyDirLight[i].viewport.ViewMatrix = MyDirLight[i].View;

		// 1-2. 셰도우 맵 1m 당 몇개의 픽셀을 담을건지 결정한다.
		constexpr float rate = 1.0f / 64.0f;
		game.MyDirLight[i].viewport.ProjectMatrix = XMMatrixOrthographicLH(MaxWidth, MaxWidth/*rate * ShadowResolusion, rate * ShadowResolusion*/, 0.1f, 4000.0f);

		// 1-3. Light CB 데이터를 초기화한다.
		matrix projmat = XMMatrixTranspose(MyDirLight[i].viewport.ProjectMatrix);
		LightCB_withShadowResource.resource->Map(0, NULL, (void**)&LightCBData_withShadow);
		LightCBData_withShadow->LightProjection[i] = projmat;
		LightCBData_withShadow->LightView[i] = XMMatrixTranspose(MyDirLight[i].viewport.ViewMatrix);
		LightCBData_withShadow->LightPos[i] = MyDirLight[i].LightPos.f3;
		LightCB_withShadowResource.resource->Unmap(0, nullptr);

		// 1-4. Dir Light 전용 Upload Buffer의 값을 설정한다.
		MappedDirLightData->DirLightView = LightCBData_withShadow->LightView[i];
		MappedDirLightData->DirLightProjection = projmat;
		MappedDirLightData->DirLightPos = MyDirLight[i].LightPos.f3;
		MappedDirLightData->DirLightDir = LightDirection;
		MappedDirLightData->DirLightColor = vec4(1, 1, 1, 1);

		// 2-2. 뷰포트 설정
		gd.gpucmd->RSSetViewports(1, &game.MyDirLight[i].viewport.Viewport);
		gd.gpucmd->RSSetScissorRects(1, &game.MyDirLight[i].viewport.ScissorRect);
		// 2-3. ShadowMap의 STATE를 DEPTH WRITE로 설정한다.
		gd.gpucmd.ResBarrierTr(&game.MyDirLight[i].ShadowMap, D3D12_RESOURCE_STATE_DEPTH_WRITE);

		// 2-4. 렌더타겟을 ShadowMap으로 Set하고 Depth Stencil을 클리어한다.
		//D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle =
		//	gd.pDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		DescHandle dc = game.MyDirLight[i].ShadowMap.descindex.hRender;
		gd.gpucmd->OMSetRenderTargets(0, nullptr, TRUE, &dc.hcpu);
		gd.gpucmd->ClearDepthStencilView(game.MyDirLight[i].ShadowMap.descindex.hRender.hcpu,
			D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);

		// 2-5. 셰도우 맵을 렌더링 하기 위해 Shader 를 Set한다.
		gd.gpucmd.SetShader(MyPBRShader1, ShaderType::RenderShadowMap);
		matrix xmf4x4View = game.MyDirLight[i].viewport.ViewMatrix;
		xmf4x4View *= game.MyDirLight[i].viewport.ProjectMatrix;
		xmf4x4View.transpose();
		gd.gpucmd->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4View, 0);
		gd.gpucmd->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16); // no matter
		gd.gpucmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
		game.PresentShaderType = ShaderType::RenderShadowMap;
		game.renderViewPort = &game.MyDirLight[i].viewport;
		game.renderViewPort->UpdateOrthoFrustum(0.1f, 1000.0f);

		// 2-6. 현재 플레이어가 위치한 청크 주변의 청크들만 쉐도우 렌더에 참여.
		ShadowRenderSkinMeshObjArr.clear();
		game.TourID += 1;
		GameObjectIncludeChunks goic = game.GetChunks_Include_OBB(LightOBB);
		//goic.ylen -= (1 + game.Map->AABB[1].y / game.chunck_divide_Width) - goic.ymin;
		ChunkIndex ci = ChunkIndex(goic.xmin, goic.ymin, goic.zmin);
		int ChunckSiz = goic.GetChunckSize();
		for (; ci.extra < ChunckSiz; goic.Inc(ci)) {
			auto f = game.chunck.find(ci);
			if (f != game.chunck.end()) {
				GameChunk* gc = f->second;

				for (int i = 0; i < gc->Static_gameobjects.size; ++i) {
					if (gc->Static_gameobjects.isnull(i)) continue;
					StaticGameObject* sgo = gc->Static_gameobjects[i];
					if (sgo == nullptr || sgo->TourID == game.TourID) continue;
					sgo->Render();
					sgo->TourID = game.TourID;
				}

				for (int i = 0; i < gc->Dynamic_gameobjects.size; ++i) {
					if (gc->Dynamic_gameobjects.isnull(i)) continue;
					DynamicGameObject* dgo = gc->Dynamic_gameobjects[i];
					if ((dgo == nullptr || dgo->tag[GameObjectTag::Tag_Enable] == false) || dgo->TourID == game.TourID) continue;
					dgo->Render();
					dgo->TourID = game.TourID;
				}

				for (int i = 0; i < gc->SkinMesh_gameobjects.size; ++i) {
					if (gc->SkinMesh_gameobjects.isnull(i)) continue;
					SkinMeshGameObject* smgo = gc->SkinMesh_gameobjects[i];
					if ((smgo == nullptr || smgo->tag[GameObjectTag::Tag_Enable] == false) || smgo->TourID == game.TourID) continue;
					ShadowRenderSkinMeshObjArr.push_back(smgo);
					smgo->TourID = game.TourID;
				}
			}
		}

		gd.gpucmd.SetShader(MyPBRShader1, ShaderType::SkinMeshRenderShadowMap);
		// 18-1. 카메라 정보 
		gd.gpucmd->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4View, 0);
		gd.gpucmd->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16);
		//// 18-2. 빛 정보 CBV
		//gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::CBVTable_SkinMeshLightData, game.LightCB_withShadowResource.descindex.hRender.hgpu);
		//// 18-3. Direction Light 의 쉐도우 맵을 적용.
		//gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_SkinMeshShadowMaps, game.MyDirLight[0].descindex.hRender.hgpu);

		gd.gpucmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
		for (int i = 0; i < ShadowRenderSkinMeshObjArr.size(); ++i) {
			Mesh* mesh = nullptr;
			Model* model = nullptr;
			ShadowRenderSkinMeshObjArr[i]->shape.GetRealShape(mesh, model);
			model->RootNode->SkinMeshShadowRender(model, gd.gpucmd, XMMatrixIdentity(), ShadowRenderSkinMeshObjArr[i]);
		}
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
	lowFrequencyFlow += game.DeltaTime;
	midFrequencyFlow += game.DeltaTime;
	highFrequencyFlow += game.DeltaTime;

	if (mainPageStack.size() >= 1) {
		if (lowHit() || hasToAlginUIDepth) {
			AlignUIDepth();
			hasToAlginUIDepth = false;
		}

		game.CurrentPageStack = &game.mainPageStack;
		mainPageStack[mainPageStack.size() - 1]->Update(game.DeltaTime);
	}

	LightDirection = LightDirection * XMMatrixRotationAxis(vec4(-1, 0, 1), DeltaTime / 120.0f);
	LightCBData_withShadow->dirlight.gLightDirection = LightDirection.f3;

	static float accSend = 0.0f;
	const float SendPeriod = 0.05f;

	accSend += DeltaTime;
	if ((isPrepared && !isInventoryOpen) && (GetActiveWindow() == hWnd && mainPageStack.size() == 0)) {
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

	// 네트워크 패킷 받기
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

	if (isPrepared) {
		// 플레이어 회전 정보 전송
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

		//gd.AverageSecPer60Start(Update_ChunksUpdate);
	// chunk에서 업데이트 수행.
		for (int i = 0; i < DynmaicGameObjects.size(); ++i) {
			if (DynmaicGameObjects[i] == nullptr || DynmaicGameObjects[i]->tag[GameObjectTag::Tag_Enable] == false) continue;
			if (DynmaicGameObjects[i]->shape.FlagPtr == 0) continue;
			DynmaicGameObjects[i]->Update(DeltaTime);
		}
		//gd.AverageSecPer60End(Update_ChunksUpdate);

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
				peye += 1.55f * player->worldMat.up;
				pat += 1.55f * player->worldMat.up;
				pat += 10.0f * clook;
			}
			else {
				peye += 1.35f * player->worldMat.up;
				peye -= 4.0f * clook;
				peye += 1.10f * player->worldMat.right;
				pat += 1.20f * player->worldMat.up;
				pat += 10.0f * clook;
				pat += 0.35f * player->worldMat.right;
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

	if (lowHit()) {
		lowFrequencyFlow = 0;
	}
	if (midHit()) {
		midFrequencyFlow = 0;
	}
	if (highHit()) {
		highFrequencyFlow = 0;
	}
}

int Game::Receiving(char* ptr, int totallen)
{
	char* currentPivot = ptr;
	int offset = 0;
	unsigned int size;
	STC_Protocol type = STC_Protocol::SyncGameObject;
READ_START:
	size = *(unsigned int*)currentPivot;
	if (offset + size >= totallen) {
		return offset;
	}
	type = *(STC_Protocol*)(currentPivot + sizeof(int));
	switch (type) {
	case STC_Protocol::SyncGameObject:
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
	case STC_Protocol::ChangeMemberOfGameObject:
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
	case STC_Protocol::NewRay:
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
	case STC_Protocol::AllocPlayerIndexes:
	{
		STC_AllocPlayerIndexes_Header& header = *(STC_AllocPlayerIndexes_Header*)currentPivot;

		if (false) {
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
			// ★ Dynamic 오브젝트도 재등록 // ?? 주석에 별표 뭐임 AI 씀? 표시하세요.
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
			if (game.isPreparedClientIndex) {
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

			//for (int i = 0; i < 36; ++i) {
			//	player->Inventory[i].id = 0;
			//	player->Inventory[i].ItemCount = 0;
			//}
		}

		game.isPreparedClientIndex = true;

		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STC_Protocol::DeleteGameObject:
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
	case STC_Protocol::ItemDrop:
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
	case STC_Protocol::ItemDropRemove:
	{
		STC_ItemDropRemove_Header& header = *(STC_ItemDropRemove_Header*)currentPivot;
		game.DropedItems[header.dropindex].itemDrop.id = 0;
		game.DropedItems[header.dropindex].itemDrop.ItemCount = 0;
		game.DropedItems[header.dropindex].pos = vec4(0, 0, 0, 0);
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STC_Protocol::InventoryItemSync:
	{
		STC_InventoryItemSync_Header& header = *(STC_InventoryItemSync_Header*)currentPivot;
		//game.player->Inventory[header.inventoryIndex].id = header.Iteminfo.id;
		//game.player->Inventory[header.inventoryIndex].ItemCount = header.Iteminfo.ItemCount;

		DXUI* Slot = game.InventorySlots[header.inventoryIndex];
		DXSlotParam* pslot = (DXSlotParam*)Slot->pParamterData;
		pslot->objid = header.Iteminfo.id;
		pslot->itemCount = header.Iteminfo.ItemCount;

		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STC_Protocol::PlayerFire:
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
	case STC_Protocol::SyncGameState:
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
	case STC_Protocol::SyncPlayerMoveZone:
	{
		STC_PlayerMoveZone_Header& header = *(STC_PlayerMoveZone_Header*)currentPivot;

		game.isPrepared = false;
		game.isPreparedClientIndex = false;
		game.isMapInit = false;
		game.MoveZone(header.zoneId);
		game.bReqireBakeLight_Raster = true;
		game.bReqireBakeLight_Raytracing = true;

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
	if (player != nullptr && mainPageStack.size() == 0)
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
		MyScreenShader->SetTextureCommand(texture);

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

void Game::RenderSDFText(const wchar_t* wstr, int length, vec4 Rect, float fontsiz, vec4 color, float* minD, float* maxD, float depth, vec4* SDFRectOut)
{
	//fontsize = 100 -> 0.1x | 10 -> 0.01x
	vec4 pos = vec4(Rect.x, Rect.w, 0, 0);
	constexpr float Default_LineHeight = 750 * 2;
	float mul = fontsiz / Default_LineHeight;
	float basic_minD = -1;
	float basic_maxD = 0;
	bool isbasicDistance = false;
	if (minD == nullptr || maxD == nullptr) {
		isbasicDistance = true;
	}
	constexpr float lineheight_mul = 1.5f;
	float lineheight = lineheight_mul * fontsiz;
	pos.y -= lineheight;
	for (int i = 0; i < length; ++i) {
		wchar_t wc = wstr[i];
		if (wc == L'\n') {
			if (SDFRectOut) {
				SDFRectOut[i] = vec4(pos.x, pos.y, pos.x + fontsiz, pos.y + lineheight);
			}
			pos.x = Rect.x;
			pos.y -= lineheight;
			continue;
		}
		else if (wc == L' ') {
			pos.x += fontsiz;
			if (SDFRectOut) {
				SDFRectOut[i] = vec4(pos.x - fontsiz, pos.y, pos.x, pos.y + lineheight);
			}
			continue;
		}
		bool textureExist = false;
		GPUResource* texture = nullptr;
		SDFTextSection* section = nullptr;
		Glyph g;
		for (int k = 0; k < gd.FontCount; ++k) {
			auto f = SDFTextPageTextureBuffer::SDFSectionMap.find(wc);
			auto f2 = gd.font_data[k].glyphs.find(wc);
			if (f != SDFTextPageTextureBuffer::SDFSectionMap.end() &&
				f2 != gd.font_data[k].glyphs.end()) {
				textureExist = true;
				section = f->second;
				g = f2->second;
				break;
			}
		}

		if (textureExist == false) {
			bool isexist = false;
			for (int i = 0; i < gd.addSDFTextureStack.size(); ++i) {
				if (gd.addSDFTextureStack[i] == wc) {
					isexist = true;
					break;
				}
			}
			if (isexist == false) {
				gd.addSDFTextureStack.push_back(wc);
			}

			continue;
		}

		//set root variables
		vec4 textRt = vec4(pos.x + g.bounding_box[0] * mul, pos.y + g.bounding_box[1] * mul, pos.x + g.bounding_box[2] * mul, pos.y + g.bounding_box[3] * mul);
		//float tConst[14] = { textRt.x, textRt.y, textRt.z, textRt.w, gd.ClientFrameWidth , gd.ClientFrameHeight, depth, 0, color.x, color.y, color.z, color.w, minD[i], maxD[i] };
		if (SDFRectOut) {
			SDFRectOut[i] = textRt;
		}

		SDFInstance sdfins;
		sdfins.Color = color;
		sdfins.depth = depth;
		sdfins.MinD = (isbasicDistance) ? basic_minD : minD[i];
		sdfins.MaxD = (isbasicDistance) ? basic_maxD : maxD[i];
		sdfins.pageId = section->pageindex;
		sdfins.rect = textRt;
		sdfins.uvrange.x = (float)section->sx / (float)SDFTextPageTextureBuffer::MaxWidth;
		sdfins.uvrange.y = (float)section->sy / (float)SDFTextPageTextureBuffer::MaxWidth;
		sdfins.uvrange.z = (float)(section->sx + section->width) / (float)SDFTextPageTextureBuffer::MaxWidth;
		sdfins.uvrange.w = (float)(section->sy + section->height) / (float)SDFTextPageTextureBuffer::MaxHeight;
		game.MyScreenShader->PushSDFInstance(sdfins);

		//calculate next location of text
		pos.x += g.advance_width * mul;
		if (pos.x > Rect.z) {
			pos.x = Rect.x;
			pos.y -= lineheight;
			if (pos.y < Rect.w) {
				return;
			}
		}
	}
}

void Game::WindowNormalizeCoordToDirectXRenderCoord_vec4(vec4& v, float W, float H)
{
	v -= 0.5f;
	v * 2.0f;
	v.x *= W;
	v.z *= W;
	v.y *= -H;
	v.w *= -H;
	swap(v.y, v.w);
}

bool Game::RectContainPos(vec4 rt, vec4 pos)
{
	bool b = clamp(pos.x, rt.x, rt.z) == pos.x;
	b = b && clamp(pos.y, rt.y, rt.w) == pos.y;
	return b;
}

bool Game::RectContainRect(vec4 rt, vec4 rt2)
{
	bool b = clamp(rt2.x, rt.x, rt.z) == rt2.x;
	b = b && clamp(rt2.y, rt.y, rt.w) == rt2.y;
	b = b && clamp(rt2.z, rt.x, rt.z) == rt2.z;
	b = b && clamp(rt2.w, rt.y, rt.w) == rt2.w;
	return b;
}

void Game::UIDraw_TextureRect(vec4 loc, vec4 color, float depth, int uitextureid)
{
	vec4 line = vec4(loc.x, (loc.y + loc.w) * 0.5f, loc.z, (loc.y + loc.w) * 0.5f);
	float wid = gd.ClientFrameWidth;
	float hei = gd.ClientFrameHeight;
	float lineWidth = loc.w - loc.y;
	gd.gpucmd->SetGraphicsRoot32BitConstants(0, 1, &wid, 0);
	gd.gpucmd->SetGraphicsRoot32BitConstants(0, 1, &hei, 1);
	gd.gpucmd->SetGraphicsRoot32BitConstants(0, 1, &depth, 2);
	gd.gpucmd->SetGraphicsRoot32BitConstants(0, 1, &lineWidth, 3);
	gd.gpucmd->SetGraphicsRoot32BitConstants(0, 4, &line, 4);
	gd.gpucmd->SetGraphicsRoot32BitConstants(0, 4, &color, 8);

	DescHandle di;
	gd.ShaderVisibleDescPool.DynamicAlloc(&di, 1);
	gd.pDevice->CopyDescriptorsSimple(1, di.hcpu, game.UITextureTable[uitextureid]->descindex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	gd.gpucmd->SetGraphicsRootDescriptorTable(1, di.hgpu);
	game.TextMesh->Render(gd.gpucmd, 1);
}

void Game::UIDraw_TextureLine(vec4 startToEnd, vec4 color, float depth, float LineWidth, int uitextureid)
{
	float wid = gd.ClientFrameWidth;
	float hei = gd.ClientFrameHeight;
	gd.gpucmd->SetGraphicsRoot32BitConstants(0, 1, &wid, 0);
	gd.gpucmd->SetGraphicsRoot32BitConstants(0, 1, &hei, 1);
	gd.gpucmd->SetGraphicsRoot32BitConstants(0, 1, &depth, 2);
	gd.gpucmd->SetGraphicsRoot32BitConstants(0, 1, &LineWidth, 3);
	gd.gpucmd->SetGraphicsRoot32BitConstants(0, 4, &startToEnd, 4);
	gd.gpucmd->SetGraphicsRoot32BitConstants(0, 4, &color, 8);

	DescHandle di;
	gd.ShaderVisibleDescPool.DynamicAlloc(&di, 1);
	gd.pDevice->CopyDescriptorsSimple(1, di.hcpu, game.UITextureTable[uitextureid]->descindex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	gd.gpucmd->SetGraphicsRootDescriptorTable(1, di.hgpu);
	game.TextMesh->Render(gd.gpucmd, 1);
}

// 여기서부터 UI를 제어하는 코드임.
#pragma region UICode
void UIRenderDefaultBtn(DXUI* ui) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	GPUResource* baseTex = game.UITextureTable[pbtn->Base_UITextureIndex];
	vec4 color = vec4(1, 1, 1, 1);
	float depth = ui->depth - game.ui_depth_epsilon;
	float rate = AnimOperUtil::EaseOut(pbtn->flow / pbtn->maxtime, 5);
	constexpr float margin = 10.0f;
	vec4 renderLoc = ui->location + game.CurrentUICenter;

	renderLoc.x += margin * rate;
	renderLoc.y += margin * rate;
	renderLoc.z -= margin * rate;
	renderLoc.w -= margin * rate;

	game.UIDraw_TextureRect(renderLoc, color, depth, 0);
	game.RenderSDFText(pbtn->text, wcslen(pbtn->text), renderLoc, 20, vec4(0, 0, 0, 1), nullptr, nullptr, -0.01f + depth);
}

void UIRender_CyberBtn001(DXUI* ui) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	vec4 color = vec4(1, 1, 1, 1);
	float depth = ui->depth - game.ui_depth_epsilon;
	float rate = 1.0f - AnimOperUtil::EaseOut(pbtn->flow / pbtn->maxtime, 5);
	constexpr float margin = 10.0f;
	vec4 renderLoc = ui->location + game.CurrentUICenter;
	renderLoc.x += margin * rate;
	renderLoc.y += margin * rate;
	renderLoc.z -= margin * rate;
	renderLoc.w -= margin * rate;
	game.UIDraw_TextureRect(renderLoc, color, depth, pbtn->Base_UITextureIndex);

	renderLoc.x += 10;
	renderLoc.w -= 5;
	game.RenderSDFText(pbtn->text, wcslen(pbtn->text), renderLoc, 15, vec4(1, 1, 1, 1), nullptr, nullptr, -0.01f + depth);
}

void UIUpdateDefaultBtn(DXUI* ui, float deltaTime) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	pbtn->flow += deltaTime;
	if (pbtn->flow > pbtn->maxtime) {
		pbtn->flow = pbtn->maxtime;
	}
}

void UIEventSaveBtn(DXUI* ui) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	vec4 evtloc = ui->location + game.CurrentUICenter;
	if (game.evt.uMsg == WM_LBUTTONDOWN && game.RectContainPos(evtloc, game.CurrentCursorPos)) {
		pbtn->flow = 0;

		if (game.CurrentPageStack->size() >= 1) {
			game.CurrentPageStack->pop_back();
		}
		// save logic
	}
}

void UIEventCloseBtn(DXUI* ui) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	vec4 evtloc = ui->location + game.CurrentUICenter;
	if (game.evt.uMsg == WM_LBUTTONDOWN && game.RectContainPos(evtloc, game.CurrentCursorPos)) {
		pbtn->flow = 0;

		// close logic
		if (game.CurrentPageStack->size() >= 1) {
			game.CurrentPageStack->pop_back();
		}
	}
}

void UIEventOpen_Inventory(DXUI* ui) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	vec4 evtloc = ui->location + game.CurrentUICenter;
	if (game.evt.uMsg == WM_LBUTTONDOWN && game.RectContainPos(evtloc, game.CurrentCursorPos)) {
		pbtn->flow = 0;

		// Showing Inventory Window Code
		DXUI* parentUI = (DXUI*)pbtn->addtionalPtr[0];
		parentUI->enable = true;
	}
}

void UIRenderDefaultEdit(DXUI* ui) {
	static vector<vec4> tempSDFRange;

	DXEditParam* pedit = (DXEditParam*)ui->pParamterData;
	vec4 color = vec4(1, 1, 1, 1);
	float depth = ui->depth - game.ui_depth_epsilon;
	float rate = 1.0f - AnimOperUtil::EaseOut(pedit->flow / pedit->maxtime, 5);
	constexpr float margin = 2.0f;
	vec4 renderLoc = ui->location + game.CurrentUICenter;

	renderLoc.x += margin * rate;
	renderLoc.y += margin * rate;
	renderLoc.z -= margin * rate;
	renderLoc.w -= margin * rate;

	game.UIDraw_TextureRect(renderLoc, color, depth, pedit->Base_UITextureIndex);

	renderLoc.x += 20;
	renderLoc.w -= 5;
	if (pedit->wstr.size() == 0) {
		game.RenderSDFText(pedit->text, wcslen(pedit->text), renderLoc, 15, vec4(0.5, 0.5, 0.5, 1), nullptr, nullptr, -0.01f + depth);
	}
	else {
		const wchar_t* wstr = pedit->wstr.c_str();
		int len = wcslen(wstr);
		if (tempSDFRange.capacity() < len) {
			tempSDFRange.reserve(len);
		}
		tempSDFRange.resize(len);
		game.RenderSDFText(wstr, len, renderLoc, 15, vec4(1, 1, 1, 1), nullptr, nullptr, -0.01f + depth, tempSDFRange.data());

		if (pedit->wstr.size() > pedit->editCursor) {
			vec4 CursorRange = tempSDFRange[pedit->editCursor];
			CursorRange.w = CursorRange.y;
			CursorRange.y -= 5;
			vec4 CursorColor = vec4(0.75f, 0.75f, 0.75f, 1.0f);
			game.UIDraw_TextureRect(CursorRange, CursorColor, -0.02f + depth, 0);
		}
	}
}

void UIUpdateDefaultEdit(DXUI* ui, float deltaTime) {
	DXEditParam* pedit = (DXEditParam*)ui->pParamterData;
	pedit->flow += deltaTime;
	if (pedit->flow > pedit->maxtime) {
		pedit->flow = pedit->maxtime;
	}
}

void UIEventDefaultEdit(DXUI* ui) {
	DXEditParam* pedit = (DXEditParam*)ui->pParamterData;
	vec4 evtloc = ui->location + game.CurrentUICenter;
	if (game.evt.uMsg == WM_LBUTTONDOWN && game.RectContainPos(evtloc, game.CurrentCursorPos)) {
		if (ui->isFocus == false) {
			ui->isFocus = true;
		}
		pedit->flow = 0;
	}

	if (ui->isFocus) {
		if (game.evt.uMsg == WM_CHAR) {
			//제외할 문자를 나열
			bool b = (game.evt.wParam != VK_BACK);
			b = b && (game.evt.wParam != VK_RETURN);
			b = b && (game.evt.wParam != VK_LEFT);
			b = b && (game.evt.wParam != VK_RIGHT);
			if (b == false) {
				return;
			}

			pedit->flow = 0;
			wchar_t addText[2] = { 0, 0 };
			addText[0] = game.CurrentCompleteCharactor;
			if (pedit->editCursor != pedit->wstr.size()) {
				pedit->wstr.insert(pedit->editCursor, addText);
			}
			else {
				pedit->wstr.push_back(game.CurrentCompleteCharactor);
			}
			pedit->editCursor = min(pedit->editCursor + 1, pedit->wstr.size());
		}
		else if (game.evt.uMsg == WM_KEYDOWN) {
			pedit->flow = 0;
			if (game.evt.wParam == VK_RETURN) {
				wchar_t addText[2] = { 0, 0 };
				addText[0] = L'\n';
				if (pedit->editCursor != pedit->wstr.size()) {
					pedit->wstr.insert(pedit->editCursor, addText);
				}
				else {
					pedit->wstr.push_back(L'\n');
				}
				pedit->editCursor = min(pedit->editCursor + 1, pedit->wstr.size());
			}
			if (game.evt.wParam == VK_BACK) {
				// 지우기
				if (pedit->wstr.size() >= 1 && pedit->editCursor >= 1) {
					if (pedit->editCursor >= pedit->wstr.size()) {
						pedit->wstr.pop_back();
						pedit->editCursor = pedit->wstr.size();
					}
					else {
						pedit->wstr.erase(pedit->editCursor, 1);
						pedit->editCursor = max(pedit->editCursor - 1, 0);
					}
				}
			}
			else if (game.evt.wParam == VK_LEFT) {
				pedit->editCursor = max(pedit->editCursor - 1, 0);
			}
			else if (game.evt.wParam == VK_RIGHT) {
				pedit->editCursor = min(pedit->editCursor + 1, pedit->wstr.size());
			}
		}
	}
}

void UIRenderDefaultSlider(DXUI* ui) {
	constexpr float presentValue_Margin = 5.0f;
	constexpr float textWid = 50.0f;
	constexpr float textHei = 35.0f;
	DXSliderParam* pslider = (DXSliderParam*)ui->pParamterData;
	vec4 present_color = vec4(0.5f, 0.5f, 0.5f, 0.5f);
	if (ui->isFocus) present_color *= 2;

	wchar_t ValueStr[64] = {};
	if (pslider->ShowValueMode != 0) {
		switch (pslider->mod) {
		case 'n':
		{
			int v = (int)(pslider->min + (pslider->max - pslider->min) * pslider->setter);
			_itow_s(v, ValueStr, 64, 10);
		}
		break;
		case 'f':
		{
			float v = (float)(pslider->min + (pslider->max - pslider->min) * pslider->setter);
			swprintf_s(ValueStr, 64, L"%g", v);
		}
		break;
		}
	}

	vec4 presentLoc = ui->location + game.CurrentUICenter;
	if (pslider->horizontal) {
		//base
		vec4 LineLoc = ui->location + game.CurrentUICenter;
		float centerY = (LineLoc.y + LineLoc.w) * 0.5f;
		LineLoc.y = centerY - 2;
		LineLoc.w = centerY + 2;
		game.UIDraw_TextureRect(LineLoc, vec4(0, 0, 0, 1), ui->depth - game.ui_depth_epsilon, 0);

		vec4 enableLoc = LineLoc;
		if (pslider->inverse_direction == false) {
			enableLoc.z = (LineLoc.z - LineLoc.x) * pslider->setter + LineLoc.x;
			presentLoc.x = enableLoc.z - presentValue_Margin;
			presentLoc.z = enableLoc.z + presentValue_Margin;
		}
		else {
			enableLoc.x = LineLoc.z - (LineLoc.z - LineLoc.x) * pslider->setter;
			presentLoc.x = enableLoc.x - presentValue_Margin;
			presentLoc.z = enableLoc.x + presentValue_Margin;
		}
		game.UIDraw_TextureRect(enableLoc, vec4(0.5, 0.5, 0.5, 1), ui->depth - game.ui_depth_epsilon * 2, 0);
		game.UIDraw_TextureRect(presentLoc, present_color, ui->depth - game.ui_depth_epsilon * 3, 0);

		if (pslider->ShowValueMode != 0) {
			vec4 textLoc = ui->location + game.CurrentUICenter;
			switch (pslider->ShowValueMode) {
			case 'f':
			{
				textLoc.z = textLoc.x;
				textLoc.x = textLoc.x - textWid;
			}
			break;
			case 'b':
			{
				textLoc.x = textLoc.z;
				textLoc.z = textLoc.z + textWid;
			}
			break;
			case 'q':
			{
				textLoc.x = presentLoc.x - textWid * 0.5f;
				textLoc.z = presentLoc.x + textWid * 0.5f;
				textLoc.y = presentLoc.w + 10;
				textLoc.w = textLoc.y + textHei;
			}
			break;
			case 'p':
			{
				textLoc.x = presentLoc.x - textWid * 0.5f;
				textLoc.z = presentLoc.x + textWid * 0.5f;
				textLoc.w = presentLoc.y - 10;
				textLoc.y = textLoc.w - textHei;
			}
			break;
			}
			game.RenderSDFText(ValueStr, wcslen(ValueStr), textLoc, 10, vec4(1, 1, 1, 1), nullptr, nullptr, ui->depth - game.ui_depth_epsilon * 2, nullptr);
		}
	}
	else {
		vec4 LineLoc = ui->location + game.CurrentUICenter;
		float centerX = (LineLoc.x + LineLoc.z) * 0.5f;
		LineLoc.x = centerX - 2;
		LineLoc.z = centerX + 2;
		game.UIDraw_TextureRect(LineLoc, vec4(0, 0, 0, 1), ui->depth - game.ui_depth_epsilon, 0);

		vec4 enableLoc = LineLoc;
		if (pslider->inverse_direction == false) {
			enableLoc.w = (LineLoc.w - LineLoc.y) * pslider->setter + LineLoc.y;
			presentLoc.y = enableLoc.w - presentValue_Margin;
			presentLoc.w = enableLoc.w + presentValue_Margin;
		}
		else {
			enableLoc.y = LineLoc.w - (LineLoc.w - LineLoc.y) * pslider->setter;
			presentLoc.y = enableLoc.y - presentValue_Margin;
			presentLoc.w = enableLoc.y + presentValue_Margin;
		}
		game.UIDraw_TextureRect(enableLoc, vec4(0.5, 0.5, 0.5, 1), ui->depth - game.ui_depth_epsilon * 2, 0);
		game.UIDraw_TextureRect(presentLoc, present_color, ui->depth - game.ui_depth_epsilon * 3, 0);

		if (pslider->ShowValueMode != 0) {
			vec4 textLoc = ui->location + game.CurrentUICenter;
			switch (pslider->ShowValueMode) {
			case 'f':
			{
				textLoc.w = textLoc.y;
				textLoc.y = textLoc.y - textHei;
			}
			break;
			case 'b':
			{
				textLoc.y = textLoc.w;
				textLoc.w = textLoc.w + textHei;
			}
			break;
			case 'q':
			{
				textLoc.y = presentLoc.y - textWid * 0.5f;
				textLoc.w = presentLoc.y + textWid * 0.5f;
				textLoc.x = presentLoc.z + 10;
				textLoc.z = textLoc.x + textWid;
			}
			break;
			case 'p':
			{
				textLoc.y = presentLoc.y - textWid * 0.5f;
				textLoc.w = presentLoc.y + textWid * 0.5f;
				textLoc.z = presentLoc.x - 10;
				textLoc.x = textLoc.z - textWid;
			}
			break;
			}
			game.RenderSDFText(ValueStr, wcslen(ValueStr), textLoc, 10, vec4(1, 1, 1, 1), nullptr, nullptr, ui->depth - game.ui_depth_epsilon * 2, nullptr);
		}
	}
}

void UIRenderCyberSlider(DXUI* ui) {
	constexpr float presentValue_Margin = 5.0f;
	constexpr float textWid = 50.0f;
	constexpr float textHei = 35.0f;
	DXSliderParam* pslider = (DXSliderParam*)ui->pParamterData;
	vec4 present_color = vec4(1, 1, 1, 0.75f);
	if (ui->isFocus)  present_color = vec4(1, 1, 1, 1);

	wchar_t ValueStr[64] = {};
	if (pslider->ShowValueMode != 0) {
		switch (pslider->mod) {
		case 'n':
		{
			int v = (int)(pslider->min + (pslider->max - pslider->min) * pslider->setter);
			_itow_s(v, ValueStr, 64, 10);
		}
		break;
		case 'f':
		{
			float v = (float)(pslider->min + (pslider->max - pslider->min) * pslider->setter);
			swprintf_s(ValueStr, 64, L"%g", v);
		}
		break;
		}
	}

	vec4 presentLoc = ui->location + game.CurrentUICenter;
	if (pslider->horizontal) {
		//base
		vec4 LineLoc = ui->location + game.CurrentUICenter;
		float centerY = (LineLoc.y + LineLoc.w) * 0.5f;
		LineLoc.y = centerY - 2;
		LineLoc.w = centerY + 2;
		presentLoc.y = centerY + pslider->NotchLoc.y;
		presentLoc.w = centerY + pslider->NotchLoc.w;
		game.UIDraw_TextureRect(LineLoc, vec4(0, 0, 0, 1), ui->depth - game.ui_depth_epsilon, 0);

		vec4 enableLoc = LineLoc;
		if (pslider->inverse_direction == false) {
			enableLoc.z = (LineLoc.z - LineLoc.x) * pslider->setter + LineLoc.x;
			presentLoc.x = enableLoc.z + pslider->NotchLoc.x;
			presentLoc.z = enableLoc.z + pslider->NotchLoc.z;
		}
		else {
			enableLoc.x = LineLoc.z - (LineLoc.z - LineLoc.x) * pslider->setter;
			presentLoc.x = enableLoc.z + pslider->NotchLoc.x;
			presentLoc.z = enableLoc.z + pslider->NotchLoc.z;
		}
		game.UIDraw_TextureRect(enableLoc, vec4(0.5, 0.5, 0.5, 1), ui->depth - game.ui_depth_epsilon * 2, 0);
		game.UIDraw_TextureRect(presentLoc, present_color, ui->depth - game.ui_depth_epsilon * 3, pslider->Base_UITextureIndex);

		if (pslider->ShowValueMode != 0) {
			vec4 textLoc = ui->location + game.CurrentUICenter;
			switch (pslider->ShowValueMode) {
			case 'f':
			{
				textLoc.z = textLoc.x;
				textLoc.x = textLoc.x - textWid;
			}
			break;
			case 'b':
			{
				textLoc.x = textLoc.z;
				textLoc.z = textLoc.z + textWid;
			}
			break;
			case 'q':
			{
				textLoc.x = presentLoc.x - textWid * 0.5f;
				textLoc.z = presentLoc.x + textWid * 0.5f;
				textLoc.y = presentLoc.w + 10;
				textLoc.w = textLoc.y + textHei;
			}
			break;
			case 'p':
			{
				textLoc.x = presentLoc.x - textWid * 0.5f;
				textLoc.z = presentLoc.x + textWid * 0.5f;
				textLoc.w = presentLoc.y - 10;
				textLoc.y = textLoc.w - textHei;
			}
			break;
			}
			game.RenderSDFText(ValueStr, wcslen(ValueStr), textLoc, 10, vec4(1, 1, 1, 1), nullptr, nullptr, ui->depth - game.ui_depth_epsilon * 2, nullptr);
		}
	}
	else {
		vec4 LineLoc = ui->location + game.CurrentUICenter;
		float centerX = (LineLoc.x + LineLoc.z) * 0.5f;
		LineLoc.x = centerX - 2;
		LineLoc.z = centerX + 2;
		presentLoc.x = centerX + pslider->NotchLoc.x;
		presentLoc.z = centerX + pslider->NotchLoc.z;
		game.UIDraw_TextureRect(LineLoc, vec4(0, 0, 0, 1), ui->depth - game.ui_depth_epsilon, 0);

		vec4 enableLoc = LineLoc;
		if (pslider->inverse_direction == false) {
			enableLoc.w = (LineLoc.w - LineLoc.y) * pslider->setter + LineLoc.y;
			presentLoc.y = enableLoc.w + pslider->NotchLoc.y;
			presentLoc.w = enableLoc.w + pslider->NotchLoc.w;
		}
		else {
			enableLoc.y = LineLoc.w - (LineLoc.w - LineLoc.y) * pslider->setter;
			presentLoc.y = enableLoc.y + pslider->NotchLoc.y;
			presentLoc.w = enableLoc.y + pslider->NotchLoc.w;
		}
		game.UIDraw_TextureRect(enableLoc, vec4(0.5, 0.5, 0.5, 1), ui->depth - game.ui_depth_epsilon * 2, 0);
		game.UIDraw_TextureRect(presentLoc, present_color, ui->depth - game.ui_depth_epsilon * 3, pslider->Base_UITextureIndex);

		if (pslider->ShowValueMode != 0) {
			vec4 textLoc = ui->location + game.CurrentUICenter;
			switch (pslider->ShowValueMode) {
			case 'f':
			{
				textLoc.w = textLoc.y;
				textLoc.y = textLoc.y - textHei;
			}
			break;
			case 'b':
			{
				textLoc.y = textLoc.w;
				textLoc.w = textLoc.w + textHei;
			}
			break;
			case 'q':
			{
				textLoc.y = presentLoc.y - textWid * 0.5f;
				textLoc.w = presentLoc.y + textWid * 0.5f;
				textLoc.x = presentLoc.z + 10;
				textLoc.z = textLoc.x + textWid;
			}
			break;
			case 'p':
			{
				textLoc.y = presentLoc.y - textWid * 0.5f;
				textLoc.w = presentLoc.y + textWid * 0.5f;
				textLoc.z = presentLoc.x - 10;
				textLoc.x = textLoc.z - textWid;
			}
			break;
			}
			game.RenderSDFText(ValueStr, wcslen(ValueStr), textLoc, 10, vec4(1, 1, 1, 1), nullptr, nullptr, ui->depth - game.ui_depth_epsilon * 2, nullptr);
		}
	}
}

void UIUpdateDefaultSlider(DXUI* ui, float deltaTime) {
	DXSliderParam* pslider = (DXSliderParam*)ui->pParamterData;
	*(float*)pslider->obj = pslider->setter;
}

void UIEventDefaultSlider(DXUI* ui) {
	constexpr float presentValue_Margin = 5.0f;
	DXSliderParam* pslider = (DXSliderParam*)ui->pParamterData;
	vec4 uiloc = ui->location + game.CurrentUICenter;
	vec4 focusRt = uiloc;
	if (pslider->horizontal) {
		float centerx = pslider->setter * (focusRt.z - focusRt.x);
		if (pslider->inverse_direction == false) {
			centerx = focusRt.x + centerx;
		}
		else {
			centerx = focusRt.z - centerx;
		}
		focusRt.x = centerx - presentValue_Margin;
		focusRt.z = centerx + presentValue_Margin;
	}
	else {
		float centery = pslider->setter * (focusRt.w - focusRt.y);
		if (pslider->inverse_direction == false) {
			centery = centery + focusRt.y;
		}
		else {
			centery = focusRt.w - centery;
		}
		focusRt.y = centery - presentValue_Margin;
		focusRt.w = centery + presentValue_Margin;
	}

	if (game.evt.uMsg == WM_LBUTTONDOWN && game.RectContainPos(focusRt, game.CurrentCursorPos)) {
		if (ui->isFocus == false) {
			ui->isFocus = true;
		}
	}

	if (ui->isFocus) {
		float rate = 0;
		if (pslider->horizontal) {
			if (pslider->inverse_direction == false) {
				rate = (game.CurrentCursorPos.x - uiloc.x) / (uiloc.z - uiloc.x);
			}
			else {
				rate = (uiloc.z - game.CurrentCursorPos.x) / (uiloc.z - uiloc.x);
			}
		}
		else {
			if (pslider->inverse_direction == false) {
				rate = (game.CurrentCursorPos.y - uiloc.y) / (uiloc.w - uiloc.y);
			}
			else {
				rate = (uiloc.w - game.CurrentCursorPos.y) / (uiloc.w - uiloc.y);
			}
		}
		rate = max(min(rate, 1), 0);
		pslider->setter = rate;
	}

	if (game.evt.uMsg == WM_LBUTTONUP) {
		ui->isFocus = false;
	}
}

void UIRenderDefaultWindow(DXUI* ui) {
	DXWindowParam* pWindow = (DXWindowParam*)ui->pParamterData;
	if (pWindow->page_stack.size() > 0) {
		//PageRender
		vector<DXPage*>* savePageStack = game.CurrentPageStack;
		game.CurrentPageStack = &pWindow->page_stack;
		vec4 SaveCenter = game.CurrentUICenter;
		game.CurrentUICenter = vec4(ui->location.x + ui->location.z, ui->location.y + ui->location.w, ui->location.x + ui->location.z, ui->location.y + ui->location.w);
		game.CurrentUICenter *= 0.5f;
		game.CurrentUICenter += SaveCenter;
		float lastDepth = 0;
		for (int i = 0; i < pWindow->page_stack.size(); ++i) {
			DXPage* page = pWindow->page_stack[i];
			lastDepth = page->GetDepth(0);
			page->Render();
		}

		game.CurrentUICenter = SaveCenter;
		game.CurrentPageStack = savePageStack;

		//Header Render
		vec4 uiloc = ui->location + game.CurrentUICenter;
		vec4 headerloc = uiloc;
		constexpr float headerMargin = 50;
		headerloc.y = headerloc.w - headerMargin;
		game.UIDraw_TextureRect(headerloc, vec4(0, 0, 1, 1.0), lastDepth - game.ui_depth_epsilon, 0);
	}
}

void UIRenderCyberWindow(DXUI* ui) {
	DXWindowParam* pWindow = (DXWindowParam*)ui->pParamterData;
	game.UIDraw_TextureRect(ui->location, vec4(1, 1, 1, 1), ui->depth, pWindow->WindowImageIndex);

	if (pWindow->page_stack.size() > 0) {
		//PageRender
		vector<DXPage*>* savePageStack = game.CurrentPageStack;
		game.CurrentPageStack = &pWindow->page_stack;
		vec4 SaveCenter = game.CurrentUICenter;
		game.CurrentUICenter = vec4(ui->location.x + ui->location.z, ui->location.y + ui->location.w, ui->location.x + ui->location.z, ui->location.y + ui->location.w);
		game.CurrentUICenter *= 0.5f;
		game.CurrentUICenter += SaveCenter;
		float lastDepth = 0;
		for (int i = 0; i < pWindow->page_stack.size(); ++i) {
			DXPage* page = pWindow->page_stack[i];
			lastDepth = page->GetDepth(0);
			page->Render();
		}

		game.CurrentUICenter = SaveCenter;
		game.CurrentPageStack = savePageStack;
	}
}

void UIUpdateDefaultWindow(DXUI* ui, float deltaTime) {
	DXWindowParam* pWindow = (DXWindowParam*)ui->pParamterData;
	if (pWindow->page_stack.size() > 0) {
		vector<DXPage*>* savePageStack = game.CurrentPageStack;
		vec4 SaveCenter = game.CurrentUICenter;
		game.CurrentUICenter = vec4(ui->location.x + ui->location.z, ui->location.y + ui->location.w, ui->location.x + ui->location.z, ui->location.y + ui->location.w);
		game.CurrentUICenter *= 0.5f;
		game.CurrentUICenter += SaveCenter;
		game.CurrentPageStack = &pWindow->page_stack;
		for (int i = 0; i < pWindow->page_stack.size(); ++i) {
			DXPage* page = pWindow->page_stack[i];
			page->Update(deltaTime);
		}
		game.CurrentUICenter = SaveCenter;
		game.CurrentPageStack = savePageStack;
	}
}

void UIEventDefaultWindow(DXUI* ui) {
	DXWindowParam* pWindow = (DXWindowParam*)ui->pParamterData;
	if (pWindow->page_stack.size() > 0) {
		vector<DXPage*>* savePageStack = game.CurrentPageStack;
		game.CurrentPageStack = &pWindow->page_stack;
		vec4 SaveCenter = game.CurrentUICenter;
		game.CurrentUICenter = vec4(ui->location.x + ui->location.z, ui->location.y + ui->location.w, ui->location.x + ui->location.z, ui->location.y + ui->location.w);
		game.CurrentUICenter *= 0.5f;
		game.CurrentUICenter += SaveCenter;
		// 마지막 페이지만을 대상으로 UI의 Event 를 처리한다.
		if (pWindow->page_stack.size() >= 1) {
			DXPage* page = pWindow->page_stack[pWindow->page_stack.size() - 1];
			page->Event();
		}

		game.CurrentUICenter = SaveCenter;
		game.CurrentPageStack = savePageStack;

		if (ui->enable) {
			//Window Event
			vec4 uiloc = ui->location + game.CurrentUICenter;
			if (game.evt.uMsg == WM_LBUTTONDOWN && game.RectContainPos(uiloc, game.CurrentCursorPos)) {
				if (ui->isFocus == false) {
					ui->isFocus = true;
					pWindow->LastFocusedTime = chrono::system_clock::now();
					game.hasToAlginUIDepth = true;
				}
			}

			if (game.evt.uMsg == WM_LBUTTONUP) {
				ui->isFocus = false;
				pWindow->Focus_Move = false;
			}

			if (ui->isFocus) {
				vec4 headerloc = uiloc;
				constexpr float headerMargin = 50;
				headerloc.y = headerloc.w - headerMargin;
				if (game.evt.uMsg == WM_LBUTTONDOWN && game.RectContainPos(headerloc, game.CurrentCursorPos)) {
					if (pWindow->Focus_Move == false) {
						pWindow->Focus_Move = true;
						pWindow->MovePivot = game.CurrentCursorPos;
					}
				}

				if (pWindow->Focus_Move) {
					vec4 MoveVector = pWindow->MovePivot - game.CurrentCursorPos;
					ui->location.x -= MoveVector.x;
					ui->location.z -= MoveVector.x;
					ui->location.y -= MoveVector.y;
					ui->location.w -= MoveVector.y;
					pWindow->MovePivot = game.CurrentCursorPos;
				}
			}
		}
	}
}

void UIInitDefaultWindow(DXUI* ui) {
	DXWindowParam* pWindow = (DXWindowParam*)ui->pParamterData;
	DXPage* sample_page = new DXPage();
	sample_page->BackGroundColor = vec4(0, 0, 0, 0.5f);
	float WindowW = ui->location.z - ui->location.x;
	float WindowH = ui->location.w - ui->location.y;
	sample_page->location = vec4(-((float)WindowW * 0.5f), -((float)WindowH * 0.5f), (float)WindowW * 0.5f, (float)WindowH * 0.5f);
	pWindow->page_table.push_back(sample_page);
	{
		// 저장 버튼
		vec4 rateloc = vec4(0.5, 0.8, 0.7, 0.9);
		pWindow->NormalizeCoordToWindowCoord_vec4(rateloc);
		DXUI* SaveBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn = (DXBtnParam*)SaveBtn->pParamterData;
		pbtn->Set(0, 1, 0, L"저장하기");
		SaveBtn->SetFunctions(UIRenderDefaultBtn, UIUpdateDefaultBtn, UIEventSaveBtn);
		sample_page->uiArr.push_back(SaveBtn);
	}
	pWindow->page_stack.push_back(sample_page);
}

void UIEvent_InventoryCloseBtn(DXUI* ui) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	vec4 evtloc = ui->location + game.CurrentUICenter;
	if (game.evt.uMsg == WM_LBUTTONDOWN && game.RectContainPos(evtloc, game.CurrentCursorPos)) {
		pbtn->flow = 0;

		//close inventory
		DXUI* parentUI = (DXUI*)pbtn->addtionalPtr[0];
		parentUI->enable = false;
		DXWindowParam* pWin = (DXWindowParam*)parentUI->pParamterData;
		pWin->Focus_Move = false;
	}
}

void UIEvent_InventorySortBtn(DXUI* ui) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	vec4 evtloc = ui->location + game.CurrentUICenter;
	if (game.evt.uMsg == WM_LBUTTONDOWN && game.RectContainPos(evtloc, game.CurrentCursorPos)) {
		pbtn->flow = 0;

		//sort Inventory
	}
}

void UIEvent_InventoryChangeItemTypeBtn(DXUI* ui) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	vec4 evtloc = ui->location + game.CurrentUICenter;
	if (game.evt.uMsg == WM_LBUTTONDOWN && game.RectContainPos(evtloc, game.CurrentCursorPos)) {
		pbtn->flow = 0;

		//change item type of Inventory
		DXUI* parentUI = (DXUI*)pbtn->addtionalPtr[0];
		DXWindowParam* winParam = (DXWindowParam*)parentUI->pParamterData;
		winParam->addtionalParams_int[0] = pbtn->addtionalParams_int[2];
	}
}

void UIRender_InventoryItemSlot(DXUI* ui) {
	DXSlotParam* pslot = (DXSlotParam*)ui->pParamterData;
	vec4& slotShowRt = *(vec4*)pslot->addtionalPtr[2];
	if (game.RectContainRect(slotShowRt, ui->location)) {
		vec4 color = vec4(1, 1, 1, 1);
		float depth = ui->depth - game.ui_depth_epsilon;
		float rate = 1.0f - AnimOperUtil::EaseOut(pslot->flow / pslot->maxtime, 5);
		constexpr float margin = 10.0f;
		vec4 renderLoc = ui->location + game.CurrentUICenter;

		renderLoc.x += margin * rate;
		renderLoc.y += margin * rate;
		renderLoc.z -= margin * rate;
		renderLoc.w -= margin * rate;

		game.UIDraw_TextureRect(renderLoc, color, depth, pslot->Base_UITextureIndex);
		wchar_t NumText[64] = {};
		_itow_s(pslot->itemCount, NumText, 64, 10);
		game.RenderSDFText(NumText, wcslen(NumText), renderLoc, 15, vec4(1, 1, 1, 1), nullptr, nullptr, -0.01f + depth);
	}
}

void UIUpdate_InventoryItemSlot(DXUI* ui, float deltaTime) {
	DXSlotParam* pslot = (DXSlotParam*)ui->pParamterData;
	pslot->flow += deltaTime;
	if (pslot->flow > pslot->maxtime) {
		pslot->flow = pslot->maxtime;
	}
	constexpr float slot_Margin = 80;
	//슬라이더의 파라미터에 따라 Y 값만 조정한다.
	float SliderY = *(float*)pslot->addtionalPtr[1];
	float StartY = pslot->addtionalParams[0];
	float MAXY = StartY + pslot->addtionalParams[1] * slot_Margin;
	float present_w = SliderY * (MAXY - StartY) + StartY;
	float height = ui->location.w - ui->location.y;
	ui->location.w = present_w;
	ui->location.y = ui->location.w - height;
}

void UIEvent_InventoryItemSlot(DXUI* ui) {
	DXSlotParam* pslot = (DXSlotParam*)ui->pParamterData;
	vec4& slotShowRt = *(vec4*)pslot->addtionalPtr[2];
	if (game.RectContainRect(slotShowRt, ui->location))
	{
		vec4 evtloc = ui->location + game.CurrentUICenter;

		if (game.CurrentGrabSlotData.itemCnt != 0) {
			if (game.RectContainPos(evtloc, game.CurrentCursorPos)) {
				if (pslot->itemCount == 0 && game.evt.uMsg == WM_LBUTTONDOWN) {
					pslot->itemCount = game.CurrentGrabSlotData.itemCnt;
					pslot->objid = game.CurrentGrabSlotData.objid;
					DXSlotParam* srcslot = (DXSlotParam*)game.CurrentGrabSlotData.selectedSlot->pParamterData;
					srcslot->itemCount -= game.CurrentGrabSlotData.itemCnt;

					//  빈 공간에 아이템 전체 놓기
					CTS_ChangeInventoryItemSlot_Header header;
					header.size = sizeof(CTS_ChangeInventoryItemSlot_Header);
					header.st = CTS_Protocol::KeyInput;
					header.ciitType = _ChangeInventoryItemSlot_Type::CIIT_ItemMoveToBlankSlot;
					header.destIndex = pslot->addtionalParams_int[7];
					header.srcIndex = srcslot->addtionalParams_int[7];
					header.srcCount = game.CurrentGrabSlotData.itemCnt;
					client.send((char*)&header, sizeof(CTS_KeyInput_Header), 0);

					game.CurrentGrabSlotData.itemCnt = 0;
					game.CurrentGrabSlotData.objid = 0;
					game.CurrentGrabSlotData.selectedSlot = nullptr;
				}
				else if (pslot->itemCount == 0 && game.evt.uMsg == WM_RBUTTONDOWN) {
					if (game.CurrentGrabSlotData.itemCnt > 0) {
						pslot->itemCount = 1;
						pslot->objid = game.CurrentGrabSlotData.objid;
						DXSlotParam* srcslot = (DXSlotParam*)game.CurrentGrabSlotData.selectedSlot->pParamterData;
						srcslot->itemCount -= 1;

						//  빈 공간에 아이템 전체 놓기
						CTS_ChangeInventoryItemSlot_Header header;
						header.size = sizeof(CTS_ChangeInventoryItemSlot_Header);
						header.st = CTS_Protocol::KeyInput;
						header.ciitType = _ChangeInventoryItemSlot_Type::CIIT_ItemMoveToBlankSlot;
						header.destIndex = pslot->addtionalParams_int[7];
						header.srcIndex = srcslot->addtionalParams_int[7];
						header.srcCount = 1;
						client.send((char*)&header, sizeof(CTS_KeyInput_Header), 0);

						game.CurrentGrabSlotData.itemCnt = srcslot->itemCount;
						if (srcslot->itemCount == 0) {
							game.CurrentGrabSlotData.itemCnt = 0;
							game.CurrentGrabSlotData.objid = 0;
							game.CurrentGrabSlotData.selectedSlot = nullptr;
						}
						// 빈 공간에 아이템 하나씩 놓기
					}
				}
				else {
					if (pslot->objid == game.CurrentGrabSlotData.objid && game.evt.uMsg == WM_LBUTTONDOWN) {
						// 같은 종류의 아이템을 놓을 경우
						pslot->itemCount += game.CurrentGrabSlotData.itemCnt;
						DXSlotParam* srcslot = (DXSlotParam*)game.CurrentGrabSlotData.selectedSlot->pParamterData;
						srcslot->itemCount -= game.CurrentGrabSlotData.itemCnt;

						//  같은 종류의 아이템을 놓을 경우
						CTS_ChangeInventoryItemSlot_Header header;
						header.size = sizeof(CTS_ChangeInventoryItemSlot_Header);
						header.st = CTS_Protocol::KeyInput;
						header.ciitType = _ChangeInventoryItemSlot_Type::CIIT_ItemCountCombine;
						header.destIndex = pslot->addtionalParams_int[7];
						header.srcIndex = srcslot->addtionalParams_int[7];
						header.srcCount = game.CurrentGrabSlotData.itemCnt;
						client.send((char*)&header, sizeof(CTS_KeyInput_Header), 0);

						game.CurrentGrabSlotData.itemCnt = 0;
						game.CurrentGrabSlotData.objid = 0;
						game.CurrentGrabSlotData.selectedSlot = nullptr;
					}
					else if (pslot->objid == game.CurrentGrabSlotData.objid && game.evt.uMsg == WM_RBUTTONDOWN) {
						// 같은 종류의 아이템을 놓을 경우
						pslot->itemCount += 1;
						DXSlotParam* srcslot = (DXSlotParam*)game.CurrentGrabSlotData.selectedSlot->pParamterData;

						//  같은 종류의 아이템을 놓을 경우
						CTS_ChangeInventoryItemSlot_Header header;
						header.size = sizeof(CTS_ChangeInventoryItemSlot_Header);
						header.st = CTS_Protocol::KeyInput;
						header.ciitType = _ChangeInventoryItemSlot_Type::CIIT_ItemCountCombine;
						header.destIndex = pslot->addtionalParams_int[7];
						header.srcIndex = srcslot->addtionalParams_int[7];
						header.srcCount = 1;
						client.send((char*)&header, sizeof(CTS_KeyInput_Header), 0);

						srcslot->itemCount -= 1;
						game.CurrentGrabSlotData.itemCnt = srcslot->itemCount;
						if (srcslot->itemCount == 0) {
							game.CurrentGrabSlotData.itemCnt = 0;
							game.CurrentGrabSlotData.objid = 0;
							game.CurrentGrabSlotData.selectedSlot = nullptr;
						}
					}
					else if (game.evt.uMsg == WM_LBUTTONDOWN || game.evt.uMsg == WM_RBUTTONDOWN) {
						DXSlotParam* srcslot = (DXSlotParam*)game.CurrentGrabSlotData.selectedSlot->pParamterData;
						if (srcslot->itemCount == game.CurrentGrabSlotData.itemCnt) {
							// 다른 종류의 아이템을 놓을 경우 - 스왑
							int itemId = pslot->objid;
							int itemCnt = pslot->itemCount;
							pslot->itemCount = game.CurrentGrabSlotData.itemCnt;
							pslot->objid = game.CurrentGrabSlotData.objid;

							// 기존 슬롯과 스왑
							DXSlotParam* srcslot = (DXSlotParam*)game.CurrentGrabSlotData.selectedSlot->pParamterData;
							srcslot->itemCount = itemCnt;
							srcslot->objid = itemId;
							game.CurrentGrabSlotData.itemCnt = itemCnt;
							game.CurrentGrabSlotData.objid = itemId;

							//  같은 종류의 아이템을 놓을 경우
							CTS_ChangeInventoryItemSlot_Header header;
							header.size = sizeof(CTS_ChangeInventoryItemSlot_Header);
							header.st = CTS_Protocol::KeyInput;
							header.ciitType = _ChangeInventoryItemSlot_Type::CIIT_Swap;
							header.destIndex = pslot->addtionalParams_int[7];
							header.srcIndex = srcslot->addtionalParams_int[7];
							header.srcCount = 0;
							client.send((char*)&header, sizeof(CTS_KeyInput_Header), 0);
						}
					}
				}
			}
		}

		if (game.RectContainPos(evtloc, game.CurrentCursorPos)) {
			// grab logic
			if (game.evt.uMsg == WM_LBUTTONDOWN && game.CurrentGrabSlotData.itemCnt == 0 && pslot->itemCount > 0) {
				pslot->flow = 0;
				game.CurrentGrabSlotData.objid = pslot->objid;
				game.CurrentGrabSlotData.itemCnt = pslot->itemCount;
				game.CurrentGrabSlotData.selectedSlot = ui;
			}

			// divide logic
			if (game.evt.uMsg == WM_RBUTTONDOWN && game.CurrentGrabSlotData.itemCnt == 0 && pslot->itemCount > 0) {
				pslot->flow = 0;
				game.CurrentGrabSlotData.objid = pslot->objid;
				game.CurrentGrabSlotData.itemCnt = (int)ceilf((float)pslot->itemCount / 2.0f);
				game.CurrentGrabSlotData.selectedSlot = ui;
			}
		}
	}
}

void UIInitInventoryWindow(DXUI* ui) {
	DXWindowParam* pWindow = (DXWindowParam*)ui->pParamterData;
	DXPage* sample_page = new DXPage();
	sample_page->BackGroundColor = vec4(0, 0, 0, 0);
	float WindowW = ui->location.z - ui->location.x;
	float WindowH = ui->location.w - ui->location.y;
	sample_page->location = vec4(-((float)WindowW * 0.5f), -((float)WindowH * 0.5f), (float)WindowW * 0.5f, (float)WindowH * 0.5f);
	pWindow->page_table.push_back(sample_page);
	{
		// 인벤토리 UI

		//1. Close Btn (568, 12, W27, H27)
		vec4 rateloc = sample_page->location;
		rateloc.x += 568;
		rateloc.w -= 12;
		rateloc.z = rateloc.x + 27;
		rateloc.y = rateloc.w - 27;
		DXUI* CloseBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn = (DXBtnParam*)CloseBtn->pParamterData;
		pbtn->Set(0, 1, 3, L"");
		pbtn->addtionalPtr[0] = ui;
		CloseBtn->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, UIEvent_InventoryCloseBtn);
		sample_page->uiArr.push_back(CloseBtn);

		//2. Item Search EditBox (15, 51, W400, H40)
		//에디트 박스
		rateloc = sample_page->location;
		rateloc.x += 15;
		rateloc.w -= 51;
		rateloc.z = rateloc.x + 400;
		rateloc.y = rateloc.w - 40;
		DXUI* ItemSearchEditBox = new DXUI(DXUI_TYPE::DXUI_Edit, sizeof(DXEditParam), rateloc, 0, new DXEditParam());
		DXEditParam* pEdit;
		pEdit = (DXEditParam*)ItemSearchEditBox->pParamterData;
		pEdit->Set(0, 1, 0, 5, L"아이템 검색창...");
		ItemSearchEditBox->SetFunctions(UIRenderDefaultEdit, UIUpdateDefaultEdit, UIEventDefaultEdit);
		sample_page->uiArr.push_back(ItemSearchEditBox);

		//3. Item Sort Btn (555, 52, W40, H40)
		rateloc = sample_page->location;
		rateloc.x += 555;
		rateloc.w -= 52;
		rateloc.z = rateloc.x + 40;
		rateloc.y = rateloc.w - 40;
		DXUI* ItemSortBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		pbtn = (DXBtnParam*)ItemSortBtn->pParamterData;
		pbtn->Set(0, 1, 4, L"");
		ItemSortBtn->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, UIEvent_InventorySortBtn);
		sample_page->uiArr.push_back(ItemSortBtn);

		//4. Item Type Select Tab Btn * 4
		/*
		* (18, 95, W130, H50) (157, 95, W130, H50) (297, 95, W130, H50) (437, 95, W130, H50)
		*/
		int startx = 18;
		const wchar_t ItemTypes[4][32] = { L"장비", L"소비", L"기타", L"전체" };
		for (int i = 0; i < 4; ++i) {
			rateloc = sample_page->location;
			rateloc.x += startx;
			rateloc.w -= 95;
			rateloc.z = rateloc.x + 130;
			rateloc.y = rateloc.w - 50;
			DXUI* ItemTypeBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
			pbtn = (DXBtnParam*)ItemTypeBtn->pParamterData;
			pbtn->Set(0, 1, 6, ItemTypes[i]);
			pbtn->addtionalPtr[0] = ui;
			pbtn->addtionalParams_int[2] = i;
			ItemTypeBtn->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, UIEvent_InventorySortBtn);
			sample_page->uiArr.push_back(ItemTypeBtn);

			startx += 139;
		}

		//5. Show Range Select Slider (574, 137, W32, H510)
		rateloc = sample_page->location;
		rateloc.x += 574;
		rateloc.w -= 137;
		rateloc.z = rateloc.x + 32;
		rateloc.y = rateloc.w - 510;
		DXUI* RangeSelectSlider = new DXUI(DXUI_TYPE::DXUI_Slider, sizeof(DXSliderParam), rateloc, 0, new DXSliderParam());
		RangeSelectSlider->SetFunctions(UIRenderCyberSlider, UIUpdateDefaultSlider, UIEventDefaultSlider);
		DXSliderParam* pRangeSelectSlider = (DXSliderParam*)RangeSelectSlider->pParamterData;
		pRangeSelectSlider->Set(false, 'f', true, 0, 1, '\0', &pWindow->addtionalParams[1], 7, vec4(-15, -15, 25, 15));
		sample_page->uiArr.push_back(RangeSelectSlider);

		//Item Slot 들. (12, 151, 75, 75) inc 80 7x7
		//pWindow->addtionalPtr[1] = new vec4();
		vec4& slotShowRt = *(vec4*)pWindow->addtionalPtr[1];
		rateloc = sample_page->location;
		rateloc.x += 12;
		rateloc.w -= 151;
		rateloc.z = rateloc.x + 80;
		rateloc.y = rateloc.w - 80;
		slotShowRt = vec4(rateloc.x - 10, rateloc.w - 570, rateloc.x + 570, rateloc.w + 10);
		constexpr float slot_Margin = 80;
		vec4 startRateLoc = rateloc;
		constexpr int inventory_slotCount = 49;
		for (int i = 0; i < inventory_slotCount; ++i) {
			DXUI* ItemSlot = new DXUI(DXUI_TYPE::DXUI_Slot, sizeof(DXSlotParam), rateloc, 0, new DXSlotParam());
			ItemSlot->SetFunctions(UIRender_InventoryItemSlot, UIUpdate_InventoryItemSlot, UIEvent_InventoryItemSlot);
			DXSlotParam* slotparam = (DXSlotParam*)ItemSlot->pParamterData;
			slotparam->Set(0, 1, SlotKind::_Item, 8);
			slotparam->addtionalParams[0] = rateloc.w; // 기본 y 시작값
			slotparam->addtionalParams[1] = inventory_slotCount / 7.0f; // 슬롯 행 개수
			slotparam->addtionalPtr[1] = &pWindow->addtionalParams[1]; // 슬라이더 값 포인터
			slotparam->addtionalPtr[2] = &slotShowRt; // Slot 이 보일 수 있는 영역
			slotparam->addtionalParams_int[7] = i; // 슬롯번호
			sample_page->uiArr.push_back(ItemSlot);
			game.InventorySlots[i] = ItemSlot;
			if ((i + 1) % 7 == 0) {
				rateloc.x = startRateLoc.x;
				rateloc.z = startRateLoc.z;
				rateloc.y -= slot_Margin;
				rateloc.w -= slot_Margin;
			}
			else {
				rateloc.x += slot_Margin;
				rateloc.z += slot_Margin;
			}
		}
	}
	pWindow->page_stack.push_back(sample_page);
}

void UIInitEquipWindow(DXUI* ui) {
	DXWindowParam* pWindow = (DXWindowParam*)ui->pParamterData;
	DXPage* sample_page = new DXPage();
	sample_page->BackGroundColor = vec4(0, 0, 0, 0);
	float WindowW = ui->location.z - ui->location.x;
	float WindowH = ui->location.w - ui->location.y;
	sample_page->location = vec4(-((float)WindowW * 0.5f), -((float)WindowH * 0.5f), (float)WindowW * 0.5f, (float)WindowH * 0.5f);
	pWindow->page_table.push_back(sample_page);
	{
		// 장비창 UI

		//1. Close Btn (526, 10, W27, H27)
		vec4 rateloc = sample_page->location;
		rateloc.x += 526;
		rateloc.w -= 10;
		rateloc.z = rateloc.x + 27;
		rateloc.y = rateloc.w - 27;
		DXUI* CloseBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn = (DXBtnParam*)CloseBtn->pParamterData;
		pbtn->Set(0, 1, 3, L"");
		pbtn->addtionalPtr[0] = ui;
		CloseBtn->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, UIEvent_InventoryCloseBtn);
		sample_page->uiArr.push_back(CloseBtn);


	}
	pWindow->page_stack.push_back(sample_page);
}

void Game::UI_Init()
{
	//TextureInit
	UITextureTable.push_back(&game.DefaultTex);
	GPUResource* NeonLight = new GPUResource();
	NeonLight->CreateTexture_fromFile(L"Resources/UI/NeonLight.png", game.basicTexFormat, 1, false);
	UITextureTable.push_back(NeonLight);

	constexpr int UITextureCount = 40;
	const wchar_t TextureNames[UITextureCount][512] = {
		L"Resources/UI/UI_Inventory_Window.png", // 2
		L"Resources/UI/UI_CloseBtn.png", //3
		L"Resources/UI/UI_Inventory_SortBtn.png", // 4
		L"Resources/UI/UI_Inventory_SearchEditBox.png", // 5
		L"Resources/UI/UI_Inventory_TabBtn.png", // 6
		L"Resources/UI/UI_SliderNotch.png", // 7
		L"Resources/UI/UI_Inventory_ItemSlot.png", // 8
		L"Resources/UI/UI_Equip_Window.png", // 9
		L"Resources/UI/UI_Equip_Slot.png", // 10
		L"Resources/UI/UI_Equip_Desc.png", // 11
		L"Resources/UI/UI_Shop_Window.png", // 12
		L"Resources/UI/UI_Shop_TabBtn.png", // 13
		L"Resources/UI/UI_Shop_ItemObject.png", //14
		L"Resources/UI/UI_Shop_AddSellItem.png", //15
		L"Resources/UI/UI_SellBuy_ItemSlot.png", //16
		L"Resources/UI/UI_SellBuy_ItemCountEdit.png", // 17
		L"Resources/UI/UI_SellBuy_SliderNotch.png", //18
		L"Resources/UI/UI_SellBuy_SliderBar.png", // 19
		L"Resources/UI/UI_SellBuy_Btn.png", //20
		L"Resources/UI/UI_QuestCashObject.png", //21
		L"Resources/UI/UI_QuestCompleteObject.png", //22
		L"Resources/UI/UI_QuestDangerObject.png", //23
		L"Resources/UI/UI_Quest_Window.png", //24
		L"Resources/UI/UI_Quest_CashDesc.png", //25
		L"Resources/UI/UI_Quest_CompleteDesc.png", //26
		L"Resources/UI/UI_Quest_DangerDesc.png", //27
		L"Resources/UI/UI_KeyBind_Window.png", //28
		L"Resources/UI/UI_KeyBind_KeySlot.png", //29
		L"Resources/UI/UI_Social_Window.png", //30
		L"Resources/UI/UI_Social_MessageEdit.png", //31
		L"Resources/UI/UI_Social_SendBtn.png", //32
		L"Resources/UI/UI_Social_PersonSlot.png", //33
		L"Resources/UI/UI_Social_BlueBtn.png", //34
		L"Resources/UI/UI_Social_OtherMessage.png", //35
		L"Resources/UI/UI_Social_MyMessage.png", //36
		L"Resources/UI/UI_Party_Window.png", //37
		L"Resources/UI/UI_Party_LeaderSlot.png", //38
		L"Resources/UI/UI_GamePlay_Passive.png", //39
		L"Resources/UI/UI_GamePlay_HPBar.png", //40
		L"Resources/UI/UI_GamePlay_HPBarGage.png", //41
	};
	for (int i = 0; i < UITextureCount; ++i) {
		GPUResource* temp = new GPUResource();
		temp->CreateTexture_fromFile(TextureNames[i], game.basicTexFormat, 1, false);
		UITextureTable.push_back(temp);
	}

	mainPageStack.reserve(32);
	UIPageTable.reserve(32);
	DXPage* sample_page = new DXPage();
	sample_page->BackGroundColor = vec4(0, 0, 0, 0.5f);
	sample_page->location = vec4(-((float)gd.ClientFrameWidth * 0.5f), -((float)gd.ClientFrameHeight * 0.5f), (float)gd.ClientFrameWidth * 0.5f, (float)gd.ClientFrameHeight * 0.5f);
	UIPageTable.push_back(sample_page);
	float ScreenW = gd.ClientFrameWidth;
	float ScreenH = gd.ClientFrameHeight;

	//UI Test 때 쓰인 코드
	if (false)
	{
		// 저장 버튼
		vec4 rateloc = vec4(0.5, 0.8, 0.7, 0.9);
		WindowNormalizeCoordToDirectXRenderCoord_vec4(rateloc, ScreenW, ScreenH);
		DXUI* SaveBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn = (DXBtnParam*)SaveBtn->pParamterData;
		pbtn->flow = 0;
		pbtn->maxtime = 1;
		wcscpy_s(pbtn->text, 64, L"저장하기");
		ZeroMemory(pbtn->addtionalParams, sizeof(float) * 16);
		pbtn->Base_UITextureIndex = 0;
		SaveBtn->RenderFunc = UIRenderDefaultBtn;
		SaveBtn->UpdateFunc = UIUpdateDefaultBtn;
		SaveBtn->EventFunc = UIEventSaveBtn;
		sample_page->uiArr.push_back(SaveBtn);

		// 닫기 버튼
		rateloc = vec4(0.75, 0.8, 0.95, 0.9);
		WindowNormalizeCoordToDirectXRenderCoord_vec4(rateloc, ScreenW, ScreenH);
		DXUI* CloseBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		pbtn = (DXBtnParam*)CloseBtn->pParamterData;
		pbtn->flow = 0;
		pbtn->maxtime = 1;
		wcscpy_s(pbtn->text, 64, L"닫기");
		ZeroMemory(pbtn->addtionalParams, sizeof(float) * 16);
		pbtn->Base_UITextureIndex = 0;
		CloseBtn->RenderFunc = UIRenderDefaultBtn;
		CloseBtn->UpdateFunc = UIUpdateDefaultBtn;
		CloseBtn->EventFunc = UIEventCloseBtn;
		sample_page->uiArr.push_back(CloseBtn);

		//에디트 박스
		rateloc = vec4(0.1, 0.1, 0.9, 0.5);
		WindowNormalizeCoordToDirectXRenderCoord_vec4(rateloc, ScreenW, ScreenH);
		DXUI* TextEdit = new DXUI(DXUI_TYPE::DXUI_Edit, sizeof(DXEditParam), rateloc, 0, new DXEditParam());
		DXEditParam* pEdit;
		pEdit = (DXEditParam*)TextEdit->pParamterData;
		pEdit->flow = 0;
		pEdit->maxtime = 1;
		pEdit->ReturnMode = 0;
		pEdit->Base_UITextureIndex = 0;
		pEdit->editCursor = 0;
		ZeroMemory(&pEdit->wstr, sizeof(wstring));
		pEdit->wstr.reserve(64);
		pEdit->wstr.clear();
		TextEdit->RenderFunc = UIRenderDefaultEdit;
		TextEdit->UpdateFunc = UIUpdateDefaultEdit;
		TextEdit->EventFunc = UIEventDefaultEdit;
		wcscpy_s(pEdit->text, 64, L"무언가를 입력해보세요!");
		ZeroMemory(pEdit->addtionalParams, sizeof(float) * 16);
		sample_page->uiArr.push_back(TextEdit);

		// 가로 슬라이더
		rateloc = vec4(0.1, 0.6, 0.5, 0.67);
		WindowNormalizeCoordToDirectXRenderCoord_vec4(rateloc, ScreenW, ScreenH);
		DXUI* SliderX = new DXUI(DXUI_TYPE::DXUI_Slider, sizeof(DXSliderParam), rateloc, 0, new DXSliderParam());
		SliderX->RenderFunc = UIRenderDefaultSlider;
		SliderX->UpdateFunc = UIUpdateDefaultSlider;
		SliderX->EventFunc = UIEventDefaultSlider;
		DXSliderParam* pSliderX = (DXSliderParam*)SliderX->pParamterData;
		pSliderX->horizontal = true;
		pSliderX->ShowValueMode = 'q';
		pSliderX->inverse_direction = false;
		pSliderX->min = -100.0f;
		pSliderX->max = 100.0f;
		pSliderX->setter = 0;
		pSliderX->mod = 'f';
		pSliderX->obj = new float(); // 보통 Set할 외부 변수의 주소를 넣는다. 다만 지금은 new로 새로 만든다.
		ZeroMemory(pSliderX->addtionalParams, sizeof(float) * 16);
		sample_page->uiArr.push_back(SliderX);

		// 세로 슬라이더
		rateloc = vec4(0.05, 0.6, 0.1, 0.9);
		WindowNormalizeCoordToDirectXRenderCoord_vec4(rateloc, ScreenW, ScreenH);
		DXUI* SliderY = new DXUI(DXUI_TYPE::DXUI_Slider, sizeof(DXSliderParam), rateloc, 0, new DXSliderParam());
		SliderY->RenderFunc = UIRenderDefaultSlider;
		SliderY->UpdateFunc = UIUpdateDefaultSlider;
		SliderY->EventFunc = UIEventDefaultSlider;
		DXSliderParam* pSliderY = (DXSliderParam*)SliderY->pParamterData;
		pSliderY->horizontal = false;
		pSliderY->ShowValueMode = 'q';
		pSliderY->inverse_direction = true;
		pSliderY->min = -100.0f;
		pSliderY->max = 100.0f;
		pSliderY->setter = 0;
		pSliderY->mod = 'n';
		pSliderY->obj = new float(); // 보통 Set할 외부 변수의 주소를 넣는다. 다만 지금은 new로 새로 만든다.
		ZeroMemory(pSliderY->addtionalParams, sizeof(float) * 16);
		sample_page->uiArr.push_back(SliderY);

		// 윈도우 창
		rateloc = vec4(0.5, 0.1, 0.9, 0.5);
		WindowNormalizeCoordToDirectXRenderCoord_vec4(rateloc, ScreenW, ScreenH);
		DXUI* SampleWindow = new DXUI(DXUI_TYPE::DXUI_Window, sizeof(DXWindowParam), rateloc, 0, new DXWindowParam());
		SampleWindow->RenderFunc = UIRenderDefaultWindow;
		SampleWindow->UpdateFunc = UIUpdateDefaultWindow;
		SampleWindow->EventFunc = UIEventDefaultWindow;
		DXWindowParam* pSampleWindow = (DXWindowParam*)SampleWindow->pParamterData;
		ZeroMemory(pSampleWindow, sizeof(DXWindowParam));
		pSampleWindow->origin = SampleWindow;
		UIInitEquipWindow(SampleWindow);
		sample_page->uiArr.push_back(SampleWindow);
	}

	//실제 UI Init Code
	{

		/*
		0 :인벤토리 (완)
		1: 장비창 (완)
		2. 상점 (완)
		3. 퀘스트창(완)
		4. 키바인더(완)
		5. 소셜 창(완)
		6. 파티창(완)
		*/
		//1. 윈도우를 여는 버튼 6개 구성

		// 인벤토리를 여는 탭 버튼. 6:(134x54)
		vec4 rateloc = vec4(0.01f, 0.01f, 0.01f, 0.01f);
		WindowNormalizeCoordToDirectXRenderCoord_vec4(rateloc, ScreenW, ScreenH);
		rateloc.z += 134;
		rateloc.y -= 54;
		DXUI* InventoryWindowOpenBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn_openInven = (DXBtnParam*)InventoryWindowOpenBtn->pParamterData;
		pbtn_openInven->Set(0, 1, 6, L"인벤토리");
		InventoryWindowOpenBtn->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, UIEventOpen_Inventory);
		sample_page->uiArr.push_back(InventoryWindowOpenBtn);

		// 장비창을 여는 탭 버튼. 6:(134x54)
		rateloc.x += 140;
		rateloc.z += 140;
		DXUI* EquipWindowOpenBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn_openequip = (DXBtnParam*)EquipWindowOpenBtn->pParamterData;
		pbtn_openequip->Set(0, 1, 6, L"장비창");
		EquipWindowOpenBtn->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, UIEventOpen_Inventory);
		sample_page->uiArr.push_back(EquipWindowOpenBtn);

		//인벤토리 윈도우 2:(605x710)
		rateloc = vec4(0.1, 0.1, 0.1, 0.1);
		WindowNormalizeCoordToDirectXRenderCoord_vec4(rateloc, ScreenW, ScreenH);
		rateloc.z += 605;
		rateloc.y -= 710;
		DXUI* InventoryWindow = new DXUI(DXUI_TYPE::DXUI_Window, sizeof(DXWindowParam), rateloc, 0, new DXWindowParam());
		InventoryWindow->SetFunctions(UIRenderCyberWindow, UIUpdateDefaultWindow, UIEventDefaultWindow);
		DXWindowParam* pInventoryWindow = (DXWindowParam*)InventoryWindow->pParamterData;
		pInventoryWindow->Set(InventoryWindow, 2);
		pInventoryWindow->addtionalParams_int[0] = 0; // 현재 선택된 아이템의 타입. (0 : 장비 / 1 : 소비 / 2 : 기타 / 3 : 특수)
		pInventoryWindow->addtionalParams[1] = 0; // 현재 보여주는 범위 선택 슬라이더의 값
		pInventoryWindow->addtionalPtr[1] = new vec4(); // 아이템 슬롯이 보일 수 있는 영역을 저장.
		pInventoryWindow->LastFocusedTime = chrono::system_clock::now();
		UIInitInventoryWindow(InventoryWindow);
		sample_page->uiArr.push_back(InventoryWindow);
		// 버튼을 눌렀을땨 윈도우가 켜지도록 연결
		pbtn_openInven->addtionalPtr[0] = InventoryWindow;

		//장비창 윈도우 9:(567 429)
		rateloc = vec4(0.1, 0.1, 0.1, 0.1);
		WindowNormalizeCoordToDirectXRenderCoord_vec4(rateloc, ScreenW, ScreenH);
		rateloc.z += 567;
		rateloc.y -= 429;
		DXUI* EquipWindow = new DXUI(DXUI_TYPE::DXUI_Window, sizeof(DXWindowParam), rateloc, 0, new DXWindowParam());
		EquipWindow->SetFunctions(UIRenderCyberWindow, UIUpdateDefaultWindow, UIEventDefaultWindow);
		DXWindowParam* pEquipWindow = (DXWindowParam*)EquipWindow->pParamterData;
		pEquipWindow->Set(EquipWindow, 9);
		pEquipWindow->LastFocusedTime = chrono::system_clock::now();
		UIInitEquipWindow(EquipWindow);
		sample_page->uiArr.push_back(EquipWindow);
		// 버튼을 눌렀을땨 윈도우가 켜지도록 연결
		pbtn_openequip->addtionalPtr[0] = EquipWindow;
	}
}

void DXPage::AlignUIDepth() {
	temp_WindowArr.clear();
	for (int i = 0; i < uiArr.size(); ++i) {
		DXUI* ui = uiArr[i];
		if (ui->type == DXUI_TYPE::DXUI_Window) {
			temp_WindowArr.push_back(ui);
			continue;
		}
		else {
			ui->depth = GetDepth(0) - game.ui_depth_epsilon;
		}
	}

	std::sort(temp_WindowArr.begin(), temp_WindowArr.end(), [](DXUI* ui0, DXUI* ui1) {
		DXWindowParam* w0 = (DXWindowParam*)ui0->pParamterData;
		DXWindowParam* w1 = (DXWindowParam*)ui1->pParamterData;
		return w0->LastFocusedTime < w1->LastFocusedTime;
		});

	depthlevel_Count = temp_WindowArr.size() + 1;
	// 마지막으로 포커스된 윈도우 순서대로 정렬됨.
	for (int i = 0; i < temp_WindowArr.size(); ++i) {
		DXWindowParam* w = (DXWindowParam*)temp_WindowArr[i]->pParamterData;
		temp_WindowArr[i]->depth = GetDepth(i + 1);
		w->depth_min = GetDepth(i + 1);
		w->depth_max = GetDepth(i + 2);
		w->AlignUIDepth();
	}
}

void DXPage::Render() {
	//Page BackGround Render
	GPUResource* baseTex = game.UITextureTable[0];
	vec4 renderloc = location + game.CurrentUICenter;
	game.UIDraw_TextureRect(renderloc, BackGroundColor, GetDepth(0), 0);

	for (int i = 0; i < uiArr.size(); ++i) {
		DXUI* ui = uiArr[i];
		if (ui->enable == false) continue;
		if (ui->RenderFunc) ui->RenderFunc(ui);
	}
}

DXUI* DXPage::GetSlotUIFromPos(vec4 pos) {
	temp_LocStack.clear();
	for (int i = temp_WindowArr.size() - 1; i >= 0; --i) {
		bool b = true;
		for (vec4 rt : temp_LocStack) {
			b = b && !game.RectContainPos(rt, pos);
		}
		if (b == false) {
			break;
		}
		DXWindowParam* w = (DXWindowParam*)temp_WindowArr[i]->pParamterData;
		vec4 loc = temp_WindowArr[i]->location;
		loc = vec4(loc.x + loc.z, loc.y + loc.w, loc.x + loc.z, loc.y + loc.w);
		loc *= 0.5f;
		vec4 temp_pos = pos + loc;
		DXUI* ui = w->GetSlotUIFromPos(temp_pos);
		temp_LocStack.push_back(w->origin->location);
		if (ui != nullptr) return ui;
	}

	//해당 Page에 있는 UI 탐색
	for (int i = 0; i < uiArr.size(); ++i) {
		if (game.RectContainPos(uiArr[i]->location, pos)) {
			return uiArr[i];
		}
	}

	return nullptr;
}
#pragma endregion
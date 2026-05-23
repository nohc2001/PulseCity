#include "stdafx.h"
#include "main.h"
#include "Render.h"
#include "Game.h"
void BoxLOD_ClearQueue();
void BoxLOD_BeginFrame();
void BoxLOD_FlushQueued();
void BoxLOD_DebugUpdate(float deltaTime);

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

namespace {
	// StaticGameObjects에 등록된 Mesh/Model을 Auto LOD 프리로드 대상으로 수집한다.
	void PrebuildStaticObjectAutoLOD()
	{
		AutoLOD_ResetPreloadQueue();

		for (int i = 0; i < game.StaticGameObjects.size(); ++i) {
			StaticGameObject* obj = game.StaticGameObjects[i];
			if (obj == nullptr) continue;

			Mesh* mesh = nullptr;
			Model* model = nullptr;
			obj->shape.GetRealShape(mesh, model);

			if (mesh != nullptr) {
				AutoLOD_QueueMeshForPreload(mesh);
			}
			else if (model != nullptr) {
				AutoLOD_QueueModelForPreload(model);
			}
		}

		AutoLOD_ProcessPreloadQueue();
	}
}

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

	for (int k = 0; k < 9; ++k) {
		//LightCBData_withShadow = new LightCB_DATA_withShadow();
		ncbElementBytes = ((sizeof(LightCB_DATA_withShadow) + 255) & ~255); // 256의 배수
		LightCB_withShadowResource[k] = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1);
		LightCB_withShadowResource[k].resource->Map(0, NULL, (void**)&LightCBData_withShadow[k]);
		LightCBData_withShadow[k]->dirlight.gLightColor = {1, 1, 1};
		vec4 dir = vec4(1, -2, 1, 0);
		dir.len3 = 1;
		LightCBData_withShadow[k]->dirlight.gLightDirection = {dir.x, dir.y, dir.z};
		for (int i = 0; i < 3; ++i)
		{
			LightCBData_withShadow[k]->LightProjection[i] = gd.viewportArr[0].ProjectMatrix;
			LightCBData_withShadow[k]->LightView[i] = MyDirLight[i].View;
			LightCBData_withShadow[k]->LightPos[i] = MyDirLight[i].LightPos.f3;
		}
		gd.ShaderVisibleDescPool.ImmortalAlloc(&LightCB_withShadowResource[k].descindex, 1);
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvdesc;
		cbvdesc.BufferLocation = LightCB_withShadowResource[k].resource->GetGPUVirtualAddress();
		cbvdesc.SizeInBytes = ncbElementBytes;
		gd.pDevice->CreateConstantBufferView(&cbvdesc, LightCB_withShadowResource[k].descindex.hCreation.hcpu);
	}
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

void Game::GameTableArrangeMent()
{

}

GameObjectIncludeChunks Zone::GetChunks_Include_OBB(BoundingOrientedBox obb)
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

GameChunk* Zone::GetChunkFromPos(vec4 pos) {
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

void Zone::PushGameObject(GameObject* go)
{
	vec4 pos = go->worldMat.pos;
	/*if (pos.y < -100.0f || pos.y > 500.0f ||
		pos.x < -1000.0f || pos.x > 1000.0f ||
		pos.z < -1000.0f || pos.z > 1000.0f) {
		return;
	}*/

	if (GameObject::IsType<Portal>(go)) {
		// static game object
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
						// new game chunk
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

void Zone::PushLight(Light* light)
{
	BoundingBox MapBB;
	vec4 start, end;
	start = game.Current_Zone->Map->AABB[0].f3;
	end = game.Current_Zone->Map->AABB[1].f3;
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

void Zone::GetImmortal_ZoneLightBuffer_SRV()
{
	Immortal_ZoneLightBuffer_SRV = game.Immortal_ZoneLightBuffer_SRV[Asset_OffsetMul];
}

namespace
{
	enum class ParticleSpriteSlot
	{
		FirePrimary = 0,
		FireSecondary,
		FireFlipbook,
		Electric,
		Ice,
	};

	GPUResource gParticleSecondaryTexture;
	GPUResource gParticleFlipbookTexture;
	GPUResource gParticleElectricTexture;
	GPUResource gParticleIceTexture;

	constexpr UINT ELECTRIC_COUNT = 180;
	constexpr UINT ELECTRIC_BURST_COUNT = 96;
	constexpr UINT EMBER_SHOWER_COUNT = 220;
	constexpr UINT FROST_CONE_COUNT = 180;
	constexpr UINT FROST_ICE_BLOCK_COUNT = 220;
	constexpr UINT FROST_BLIZZARD_COUNT = 320;
	constexpr UINT MUZZLE_FLASH_COUNT = 96;
	constexpr UINT BULLET_TRACER_COUNT = 256;
	constexpr UINT PARTICLE_FLAG_COLLIDE_GROUND = 1u;
	ParticlePool gElectricPool;
	ParticlePool gElectricBurstPool;
	ParticlePool gEmberShowerPool;
	ParticlePool gFrostConePool;
	ParticlePool gFrostIceBlockPool;
	ParticlePool gFrostBlizzardPool;
	ParticlePool gMuzzleFlashPool;
	ParticlePool gBulletTracerPool;
	ParticleCompute* gElectricCS = nullptr;
	ParticleCompute* gElectricBurstCS = nullptr;
	ParticleCompute* gEmberShowerCS = nullptr;
	ParticleCompute* gFrostConeCS = nullptr;
	ParticleCompute* gFrostIceBlockCS = nullptr;
	ParticleCompute* gFrostBlizzardCS = nullptr;
	GPUResource gMuzzleFlashUpload;
	GPUResource gBulletTracerUpload;
	std::vector<Particle> gMuzzleFlashParticles;
	std::vector<Particle> gBulletTracerParticles;

	ParticleSpriteSlot gFireSpriteSlot = ParticleSpriteSlot::FirePrimary;
	ParticleSpriteSlot gFirePillarSpriteSlot = ParticleSpriteSlot::FirePrimary;
	ParticleSpriteSlot gFireRingSpriteSlot = ParticleSpriteSlot::FirePrimary;
	ParticleSpriteSlot gElectricSpriteSlot = ParticleSpriteSlot::Electric;
	ParticleSpriteSlot gElectricBurstSpriteSlot = ParticleSpriteSlot::Electric;
	ParticleSpriteSlot gEmberShowerSpriteSlot = ParticleSpriteSlot::FirePrimary;
	ParticleSpriteSlot gFrostConeSpriteSlot = ParticleSpriteSlot::Ice;
	ParticleSpriteSlot gFrostIceBlockSpriteSlot = ParticleSpriteSlot::Ice;
	ParticleSpriteSlot gFrostBlizzardSpriteSlot = ParticleSpriteSlot::Ice;
	ParticleSpriteSlot gMuzzleFlashSpriteSlot = ParticleSpriteSlot::FirePrimary;
	ParticleSpriteSlot gBulletTracerSpriteSlot = ParticleSpriteSlot::Electric;
	UINT gTransientParticleSeed = 1u;

	constexpr UINT PARTICLE_EMITTER_FLAG_RESET = 1u;

	struct ParticleEffectRuntime
	{
		SkillEffectType Type;
		ParticlePool* Pool;
		ParticleCompute** Compute;
		ParticleSpriteSlot* SpriteSlot;
		const char* EntryPoint;
		UINT Count;
		ParticleEmitterCB Emitter;
		bool Active;
		bool PendingReset;
	};

	struct StatusEffectVisual
	{
		bool Active = false;
		StatusEffectType Type = StatusEffectType::None;
		int TargetObjIndex = -1;
		int SourceObjIndex = -1;
		float Duration = 0.0f;
		float RemainTime = 0.0f;
		float Power = 0.0f;
		vec4 Position = vec4(0, 0, 0, 1);
		vec4 Extents = vec4(0.3f, 1.0f, 0.3f, 0.0f);
	};

	std::vector<StatusEffectVisual> gStatusEffectVisuals;

	GPUResource* GetParticleSpriteResource(ParticleSpriteSlot slot)
	{
		switch (slot) {
		case ParticleSpriteSlot::FireSecondary:
			return &gParticleSecondaryTexture;
		case ParticleSpriteSlot::FireFlipbook:
			return &gParticleFlipbookTexture;
		case ParticleSpriteSlot::Electric:
			return &gParticleElectricTexture;
		case ParticleSpriteSlot::Ice:
			return &gParticleIceTexture;
		case ParticleSpriteSlot::FirePrimary:
		default:
			return &game.FireTextureRes;
		}
	}

	vec4 NormalizeOrFallback(vec4 v, vec4 fallback);

	ParticleEmitterCB MakeParticleEmitter(vec4 position, vec4 direction, float radius, float power, float duration, UINT ownerId)
	{
		direction = NormalizeOrFallback(direction, vec4(0, 0, 1, 0));

		ParticleEmitterCB emitter{};
		emitter.Position = XMFLOAT3(position.x, position.y, position.z);
		emitter.Radius = radius;
		emitter.Direction = XMFLOAT3(direction.x, direction.y, direction.z);
		emitter.Power = power;
		emitter.Duration = duration;
		emitter.Age = 0.0f;
		emitter.OwnerId = ownerId;
		emitter.Flags = 0u;
		return emitter;
	}

	auto GetParticleEffectRuntimes() -> std::vector<ParticleEffectRuntime>&
	{
		static std::vector<ParticleEffectRuntime> effects;
		if (!effects.empty()) return effects;

		effects.reserve(9);
		ParticleEmitterCB idleEmitter = MakeParticleEmitter(vec4(0.0f, 0.0f, 0.0f, 1.0f), vec4(0, 1, 0, 0), 1.0f, 1.0f, 0.0f, 0);
		effects.push_back(ParticleEffectRuntime{ SkillEffectType::Mage_FireBall, &game.FirePool, &game.FireCS, &gFireSpriteSlot, "FireCS", Game::FIRE_COUNT, idleEmitter, false, false });
		effects.push_back(ParticleEffectRuntime{ SkillEffectType::Fire_Pillar, &game.FirePillarPool, &game.FirePillarCS, &gFirePillarSpriteSlot, "FirePillarCS", Game::FIRE_PILLAR_COUNT, idleEmitter, false, false });
		effects.push_back(ParticleEffectRuntime{ SkillEffectType::Fire_Ring, &game.FireRingPool, &game.FireRingCS, &gFireRingSpriteSlot, "FireRingCS", Game::FIRE_RING_COUNT, idleEmitter, false, false });
		effects.push_back(ParticleEffectRuntime{ SkillEffectType::Electric_Arc, &gElectricPool, &gElectricCS, &gElectricSpriteSlot, "ElectricArcCS", ELECTRIC_COUNT, idleEmitter, false, false });
		effects.push_back(ParticleEffectRuntime{ SkillEffectType::Electric_Burst, &gElectricBurstPool, &gElectricBurstCS, &gElectricBurstSpriteSlot, "ElectricBurstCS", ELECTRIC_BURST_COUNT, idleEmitter, false, false });
		effects.push_back(ParticleEffectRuntime{ SkillEffectType::Ember_Shower, &gEmberShowerPool, &gEmberShowerCS, &gEmberShowerSpriteSlot, "EmberShowerCS", EMBER_SHOWER_COUNT, idleEmitter, false, false });
		effects.push_back(ParticleEffectRuntime{ SkillEffectType::Frost_Cone, &gFrostConePool, &gFrostConeCS, &gFrostConeSpriteSlot, "FrostConeCS", FROST_CONE_COUNT, idleEmitter, false, false });
		effects.push_back(ParticleEffectRuntime{ SkillEffectType::Frost_IceBlock, &gFrostIceBlockPool, &gFrostIceBlockCS, &gFrostIceBlockSpriteSlot, "FrostIceBlockCS", FROST_ICE_BLOCK_COUNT, idleEmitter, false, false });
		effects.push_back(ParticleEffectRuntime{ SkillEffectType::Frost_Blizzard, &gFrostBlizzardPool, &gFrostBlizzardCS, &gFrostBlizzardSpriteSlot, "FrostBlizzardCS", FROST_BLIZZARD_COUNT, idleEmitter, false, false });
		return effects;
	}

	ParticleEffectRuntime* FindParticleEffectRuntime(SkillEffectType type)
	{
		std::vector<ParticleEffectRuntime>& effects = GetParticleEffectRuntimes();
		for (ParticleEffectRuntime& effect : effects) {
			if (effect.Type == type) return &effect;
		}
		return nullptr;
	}

	float RandomUnit()
	{
		return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
	}

	float RandomRange(float minValue, float maxValue)
	{
		return minValue + (maxValue - minValue) * RandomUnit();
	}

	vec4 NormalizeOrFallback(vec4 v, vec4 fallback)
	{
		if (v.fast_square_of_len3 > 0.0001f) {
			v.len3 = 1.0f;
			return v;
		}
		return fallback;
	}

	void KillParticle(Particle& particle)
	{
		particle.Position = XMFLOAT3(0.0f, -1000.0f, 0.0f);
		particle.Age = 1.0f;
		particle.Velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
		particle.LifeTime = 0.0f;
		particle.StartColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		particle.EndColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		particle.StartSize = 0.0f;
		particle.EndSize = 0.0f;
		particle.Rotation = 0.0f;
		particle.RotationSpeed = 0.0f;
		particle.Drag = 0.0f;
		particle.GravityScale = 0.0f;
		particle.Stretch = 0.0f;
		particle.CollisionRadius = 0.0f;
		particle.Flags = 0;
		particle.RandomSeed = 0;
		particle.FrameIndex = 0;
		particle.FrameCount = 1;
		particle.FrameCols = 1;
	}

	Particle* AllocateTransientParticle(std::vector<Particle>& particles)
	{
		for (Particle& particle : particles) {
			if (particle.LifeTime <= 0.0f || particle.Age >= particle.LifeTime) {
				return &particle;
			}
		}

		if (particles.empty()) return nullptr;

		Particle* oldest = &particles[0];
		float maxAge = particles[0].Age;
		for (Particle& particle : particles) {
			if (particle.Age > maxAge) {
				maxAge = particle.Age;
				oldest = &particle;
			}
		}
		return oldest;
	}

	void InitTransientParticlePool(ParticlePool& pool, GPUResource& upload, std::vector<Particle>& particles, UINT count)
	{
		particles.resize(count);
		for (Particle& particle : particles) {
			KillParticle(particle);
			particle.RandomSeed = gTransientParticleSeed++;
		}

		pool.Count = count;
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

		upload = gd.CreateCommitedGPUBuffer(
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_DIMENSION_BUFFER,
			sizeof(Particle) * count,
			1,
			DXGI_FORMAT_UNKNOWN
		);

		gd.UploadToCommitedGPUBuffer(
			particles.data(),
			&upload,
			&pool.Buffer,
			true
		);
	}

	void TickTransientParticles(std::vector<Particle>& particles, float dt)
	{
		for (Particle& particle : particles) {
			if (particle.LifeTime <= 0.0f || particle.Age >= particle.LifeTime) continue;

			particle.Age += dt;
			if (particle.Age >= particle.LifeTime) {
				KillParticle(particle);
				continue;
			}

			particle.Velocity.y += -9.81f * particle.GravityScale * dt;
			float dragFactor = max(0.0f, 1.0f - particle.Drag * dt);
			particle.Velocity.x *= dragFactor;
			particle.Velocity.y *= dragFactor;
			particle.Velocity.z *= dragFactor;

			particle.Position.x += particle.Velocity.x * dt;
			particle.Position.y += particle.Velocity.y * dt;
			particle.Position.z += particle.Velocity.z * dt;
			particle.Rotation += particle.RotationSpeed * dt;

			if ((particle.Flags & PARTICLE_FLAG_COLLIDE_GROUND) != 0u) {
				float floorY = 0.0f + particle.CollisionRadius;
				if (particle.Position.y < floorY) {
					particle.Position.y = floorY;
					if (particle.Velocity.y < 0.0f) {
						particle.Velocity.y *= -0.25f;
						particle.Velocity.x *= 0.72f;
						particle.Velocity.z *= 0.72f;
					}
				}
			}

			float lifeT = min(1.0f, particle.Age / max(particle.LifeTime, 0.0001f));
			UINT frameCount = max(particle.FrameCount, 1u);
			particle.FrameIndex = min(static_cast<UINT>(lifeT * static_cast<float>(frameCount - 1)), frameCount - 1);
		}
	}

	void UploadTransientParticles(ParticlePool& pool, GPUResource& upload, const std::vector<Particle>& particles)
	{
		if (particles.empty()) return;
		gd.UploadToCommitedGPUBuffer(
			const_cast<Particle*>(particles.data()),
			&upload,
			&pool.Buffer,
			true
		);
	}

	void TryGetMuzzleBasis(Player* player, vec4& origin, vec4& forward, vec4& right, vec4& up)
	{
		if (player == game.player && game.bFirstPersonVision) {
			matrix view = gd.viewportArr[0].ViewMatrix;
			XMMATRIX invViewXm = XMMatrixInverse(nullptr, (XMMATRIX)view);
			matrix invView = invViewXm;

			forward = NormalizeOrFallback(invView.look, vec4(0, 0, 1, 0));
			right = NormalizeOrFallback(invView.right, vec4(1, 0, 0, 0));
			up = NormalizeOrFallback(invView.up, vec4(0, 1, 0, 0));
			origin = gd.viewportArr[0].Camera_Pos + forward * 0.72f + right * 0.15f - up * 0.10f;
			return;
		}

		forward = NormalizeOrFallback(player->worldMat.look, vec4(0, 0, 1, 0));
		right = NormalizeOrFallback(player->worldMat.right, vec4(1, 0, 0, 0));
		up = NormalizeOrFallback(player->worldMat.up, vec4(0, 1, 0, 0));
		origin = player->worldMat.pos + right * 0.30f + up * 1.35f + forward * 0.48f;
	}

	void SpawnMuzzleFlash(Player* player)
	{
		if (player == nullptr) return;

		vec4 origin;
		vec4 forward;
		vec4 right;
		vec4 up;
		TryGetMuzzleBasis(player, origin, forward, right, up);

		for (int i = 0; i < 12; ++i) {
			Particle* particle = AllocateTransientParticle(gMuzzleFlashParticles);
			if (particle == nullptr) break;

			vec4 dir = NormalizeOrFallback(
				forward
				+ right * RandomRange(-0.35f, 0.35f)
				+ up * RandomRange(-0.22f, 0.22f),
				forward
			);
			vec4 pos = origin
				+ forward * RandomRange(0.00f, 0.08f)
				+ right * RandomRange(-0.04f, 0.04f)
				+ up * RandomRange(-0.04f, 0.04f);

			particle->Position = XMFLOAT3(pos.x, pos.y, pos.z);
			particle->Velocity = XMFLOAT3(
				dir.x * RandomRange(5.5f, 11.0f) + up.x * RandomRange(0.4f, 1.1f),
				dir.y * RandomRange(5.5f, 11.0f) + up.y * RandomRange(0.4f, 1.1f),
				dir.z * RandomRange(5.5f, 11.0f) + up.z * RandomRange(0.4f, 1.1f));
			particle->Age = 0.0f;
			particle->LifeTime = RandomRange(0.05f, 0.11f);
			particle->StartColor = XMFLOAT4(2.2f, 1.35f, 0.72f, 1.0f);
			particle->EndColor = XMFLOAT4(0.45f, 0.18f, 0.05f, 0.0f);
			particle->StartSize = RandomRange(0.10f, 0.18f);
			particle->EndSize = 0.015f;
			particle->Rotation = RandomRange(0.0f, XM_2PI);
			particle->RotationSpeed = RandomRange(-5.0f, 5.0f);
			particle->Drag = RandomRange(6.0f, 10.0f);
			particle->GravityScale = -0.10f;
			particle->Stretch = RandomRange(0.8f, 1.6f);
			particle->CollisionRadius = 0.0f;
			particle->Flags = 0u;
			particle->RandomSeed = gTransientParticleSeed++;
			particle->FrameIndex = 0;
			particle->FrameCount = 36u;
			particle->FrameCols = 6u;
		}
	}

	void SpawnElectricTracer(vec4 start, vec4 direction, float distance)
	{
		vec4 forward = NormalizeOrFallback(direction, vec4(0, 0, 1, 0));
		vec4 upHint = abs(forward.y) > 0.85f ? vec4(1, 0, 0, 0) : vec4(0, 1, 0, 0);
		vec4 right = forward.cross(upHint);
		right = NormalizeOrFallback(right, vec4(1, 0, 0, 0));
		vec4 up = right.cross(forward);
		up = NormalizeOrFallback(up, vec4(0, 1, 0, 0));

		Particle* core = AllocateTransientParticle(gBulletTracerParticles);
		if (core != nullptr) {
			vec4 corePos = start + forward * 0.08f;
			core->Position = XMFLOAT3(corePos.x, corePos.y, corePos.z);
			core->Velocity = XMFLOAT3(
				forward.x * RandomRange(42.0f, 52.0f),
				forward.y * RandomRange(42.0f, 52.0f),
				forward.z * RandomRange(42.0f, 52.0f));
			core->Age = 0.0f;
			core->LifeTime = min(0.13f, 0.045f + distance * 0.00055f);
			core->StartColor = XMFLOAT4(2.05f, 2.95f, 3.75f, 1.0f);
			core->EndColor = XMFLOAT4(0.10f, 0.52f, 1.12f, 0.0f);
			core->StartSize = RandomRange(0.060f, 0.082f);
			core->EndSize = 0.020f;
			core->Rotation = RandomRange(0.0f, XM_2PI);
			core->RotationSpeed = RandomRange(-2.0f, 2.0f);
			core->Drag = RandomRange(0.25f, 0.55f);
			core->GravityScale = 0.0f;
			core->Stretch = RandomRange(6.2f, 7.6f);
			core->CollisionRadius = 0.0f;
			core->Flags = 0u;
			core->RandomSeed = gTransientParticleSeed++;
			core->FrameIndex = 0;
			core->FrameCount = 1u;
			core->FrameCols = 1u;
		}

		const float tracerLife = min(0.14f, 0.04f + distance * 0.0008f);
		const int tracerCount = 4;
		for (int i = 0; i < tracerCount; ++i) {
			Particle* particle = AllocateTransientParticle(gBulletTracerParticles);
			if (particle == nullptr) break;

			float along = distance * (static_cast<float>(i) / static_cast<float>(tracerCount + 1));
			vec4 pos = start
				+ forward * (0.03f + along * 0.05f)
				+ right * RandomRange(-0.007f, 0.007f)
				+ up * RandomRange(-0.007f, 0.007f);
			vec4 lateral = right * RandomRange(-0.35f, 0.35f) + up * RandomRange(-0.35f, 0.35f);

			particle->Position = XMFLOAT3(pos.x, pos.y, pos.z);
			particle->Velocity = XMFLOAT3(
				forward.x * RandomRange(30.0f, 38.0f) + lateral.x,
				forward.y * RandomRange(30.0f, 38.0f) + lateral.y,
				forward.z * RandomRange(30.0f, 38.0f) + lateral.z);
			particle->Age = RandomRange(0.0f, tracerLife * 0.30f);
			particle->LifeTime = tracerLife + RandomRange(-0.01f, 0.01f);
			particle->StartColor = XMFLOAT4(0.42f, 1.12f, 2.05f, 0.72f);
			particle->EndColor = XMFLOAT4(0.02f, 0.14f, 0.52f, 0.0f);
			particle->StartSize = RandomRange(0.028f, 0.040f);
			particle->EndSize = 0.006f;
			particle->Rotation = RandomRange(0.0f, XM_2PI);
			particle->RotationSpeed = RandomRange(-1.5f, 1.5f);
			particle->Drag = RandomRange(0.55f, 0.95f);
			particle->GravityScale = 0.0f;
			particle->Stretch = RandomRange(3.8f, 4.8f);
			particle->CollisionRadius = 0.0f;
			particle->Flags = 0u;
			particle->RandomSeed = gTransientParticleSeed++;
			particle->FrameIndex = 0;
			particle->FrameCount = 6u;
			particle->FrameCols = 6u;
		}
	}
}

void Game::SpawnSkillEffect(SkillEffectType type, vec4 position, vec4 direction, UINT ownerId, float radius, float power, float duration)
{
	SkillEffectType runtimeType = type;
	switch (type) {
	case SkillEffectType::Healer_HealAura:
		runtimeType = SkillEffectType::Electric_Arc;
		break;
	case SkillEffectType::Tank_ShockWave:
		runtimeType = SkillEffectType::Fire_Ring;
		break;
	case SkillEffectType::Juggernaut_FireProjectile:
		runtimeType = SkillEffectType::Mage_FireBall;
		break;
	case SkillEffectType::Juggernaut_Taunt:
		runtimeType = SkillEffectType::Fire_Ring;
		break;
	case SkillEffectType::Juggernaut_UltimateFire:
		runtimeType = SkillEffectType::Ember_Shower;
		break;
	case SkillEffectType::Aegis_Barrier:
	case SkillEffectType::Aegis_ShieldAura:
		runtimeType = SkillEffectType::Electric_Burst;
		break;
	case SkillEffectType::Aegis_ShieldCharge:
		runtimeType = SkillEffectType::Electric_Arc;
		break;
	default:
		break;
	}

	ParticleEffectRuntime* effect = FindParticleEffectRuntime(runtimeType);
	if (effect == nullptr) return;

	if (type == SkillEffectType::Healer_HealAura) {
		*effect->SpriteSlot = ParticleSpriteSlot::Electric;
	}
	else if (type == SkillEffectType::Tank_ShockWave) {
		*effect->SpriteSlot = ParticleSpriteSlot::FireSecondary;
	}
	else if (type == SkillEffectType::Juggernaut_FireProjectile || type == SkillEffectType::Juggernaut_Taunt || type == SkillEffectType::Juggernaut_UltimateFire) {
		*effect->SpriteSlot = ParticleSpriteSlot::FireSecondary;
	}
	else if (type == SkillEffectType::Frost_Cone || type == SkillEffectType::Frost_IceBlock || type == SkillEffectType::Frost_Blizzard) {
		*effect->SpriteSlot = ParticleSpriteSlot::Ice;
	}
	else if (type == SkillEffectType::Aegis_ShieldCharge || type == SkillEffectType::Aegis_Barrier || type == SkillEffectType::Aegis_ShieldAura) {
		*effect->SpriteSlot = ParticleSpriteSlot::Electric;
	}

	effect->Emitter = MakeParticleEmitter(position, direction, radius, power, duration, ownerId);
	effect->Active = true;
	effect->PendingReset = true;
}
void Game::SpawnStatusEffect(StatusEffectType type, int targetObjIndex, int sourceObjIndex, bool active, vec4 position, vec4 extents, float duration, float remainTime, float power)
{
	if (type == StatusEffectType::None) return;

	if (targetObjIndex >= 0 && targetObjIndex < (int)DynmaicGameObjects.size() && DynmaicGameObjects[targetObjIndex] != nullptr) {
		position = DynmaicGameObjects[targetObjIndex]->worldMat.pos;
		position.y += max(0.6f, extents.y * 0.5f);
	}

	int visualIndex = -1;
	for (int i = 0; i < (int)gStatusEffectVisuals.size(); ++i) {
		if (gStatusEffectVisuals[i].TargetObjIndex == targetObjIndex && gStatusEffectVisuals[i].Type == type) {
			visualIndex = i;
			break;
		}
	}

	if (active == false) {
		if (visualIndex >= 0) gStatusEffectVisuals[visualIndex].Active = false;
		return;
	}

	if (visualIndex < 0) {
		StatusEffectVisual visual;
		visualIndex = (int)gStatusEffectVisuals.size();
		gStatusEffectVisuals.push_back(visual);
	}

	StatusEffectVisual& visual = gStatusEffectVisuals[visualIndex];
	visual.Active = true;
	visual.Type = type;
	visual.TargetObjIndex = targetObjIndex;
	visual.SourceObjIndex = sourceObjIndex;
	visual.Duration = duration;
	visual.RemainTime = remainTime;
	visual.Power = power;
	visual.Position = position;
	visual.Extents = extents;

	float radius = max(max(extents.x, extents.z) * 2.0f, 1.0f);
	float visualDuration = max(remainTime, duration);
	if (visualDuration <= 0.0f) visualDuration = 1.0f;

	switch (type) {
	case StatusEffectType::Freeze:
	case StatusEffectType::Slow:
		SpawnSkillEffect(SkillEffectType::Frost_IceBlock, position, vec4(0, 1, 0, 0), (UINT)targetObjIndex, radius, max(power, 1.0f), visualDuration);
		break;
	case StatusEffectType::Burn:
		SpawnSkillEffect(SkillEffectType::Ember_Shower, position, vec4(0, 1, 0, 0), (UINT)targetObjIndex, radius, max(power, 1.0f), min(visualDuration, 1.5f));
		break;
	case StatusEffectType::Stun:
	case StatusEffectType::Paralyze:
		SpawnSkillEffect(SkillEffectType::Electric_Burst, position, vec4(0, 1, 0, 0), (UINT)targetObjIndex, radius, max(power, 1.0f), min(visualDuration, 1.2f));
		break;
	case StatusEffectType::Taunt:
		SpawnSkillEffect(SkillEffectType::Fire_Ring, position, vec4(0, 1, 0, 0), (UINT)targetObjIndex, radius, max(power, 1.0f), min(visualDuration, 1.0f));
		break;
	default:
		break;
	}
}
void Game::InitParticlePool(ParticlePool& pool, UINT count)
{
	pool.Count = count;

	std::vector<Particle> init(count);
	for (UINT i = 0; i < count; ++i)
	{
		init[i].Age = 0.0f;
		init[i].LifeTime = 0.0f;
		init[i].Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
		init[i].Velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
		init[i].StartColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		init[i].EndColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		init[i].StartSize = 0.05f;
		init[i].EndSize = 0.01f;
		init[i].Rotation = 0.0f;
		init[i].RotationSpeed = 0.0f;
		init[i].Drag = 0.0f;
		init[i].GravityScale = 1.0f;
		init[i].Stretch = 0.0f;
		init[i].CollisionRadius = 0.0f;
		init[i].Flags = 0;
		init[i].RandomSeed = i * 9781u + 1u;
		init[i].FrameIndex = 0;
		init[i].FrameCount = 36;
		init[i].FrameCols = 6;
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

	StaticGameObjects.reserve(Zone::MaxStaticObjectCount * 9); // 1MB
	StaticGameObjects.resize(Zone::MaxStaticObjectCount * 9); // 1MB
	//1. Zone Init. - //fix - planning . unity map exporter export zone info. and client and server use it.

	{
		Zone* Zone_ThePort = new Zone(0, "The_Port", 0, 0);
		game.ZoneTable.push_back(Zone_ThePort);

		Zone* Zone_OfficeDungeon_1floor = new Zone(1, "OfficeDungeon_1floor", 1, 0);
		game.ZoneTable.push_back(Zone_OfficeDungeon_1floor);

		Zone_ThePort->nearZones[1] = Zone_OfficeDungeon_1floor;
	}
	Current_Zone = game.ZoneTable[0];

	// 2. DirLight를 초기화한다.
	InitDirLightGPURes();

	// 이름 목록과 텍스트 SDF 텍스처들을 가져온다.
	gd.GetBakedSDFs(); // later

	// 플레이어, NPCHP바, Ray를 1차 용량으로 초기화한다.
	DropedItems.reserve(4096);
	NpcHPBars.Init(1024);
	bulletRays.Init(1024);

	// 모든 리소스 초기화를 위해 커맨드리스트를 Reset한 상태로 준비한다.
	gd.gpucmd.Reset();
	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

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

	{
		DefaultTex.CreateTexture_fromFile(L"Resources/DefaultTexture.png", game.basicTexFormat, game.basicTexMip);
		DefaultNoramlTex.CreateTexture_fromFile(L"Resources/GlobalTexture/DefaultNormalTexture.png", basicTexFormat, basicTexMip);
		DefaultAmbientTex.CreateTexture_fromFile(L"Resources/GlobalTexture/DefaultAmbientTexture.png", basicTexFormat, basicTexMip);
		// particle texture
		FireTextureRes.CreateTexture_fromFile(L"Resources/fire.jpg", game.basicTexFormat, game.basicTexMip /*DXGI_FORMAT_UNKNOWN, 1*/, true);
		gParticleSecondaryTexture.CreateTexture_fromFile(L"Resources/fire2.jpg", game.basicTexFormat, game.basicTexMip, true);
		gParticleFlipbookTexture.CreateTexture_fromFile(L"Resources/fire.dds", game.basicTexFormat, game.basicTexMip, true);
		gParticleElectricTexture.CreateTexture_fromFile(L"Resources/elect.jpg", game.basicTexFormat, game.basicTexMip, true);
		gParticleIceTexture.CreateTexture_fromFile(L"Resources/ice.png", game.basicTexFormat, game.basicTexMip, true);

		//텍스트 출력에 사용할 메쉬를 가져온다.
		TextMesh = new UVMesh();
		TextMesh->CreateTextRectMesh();

		BulletRay::mesh = (Mesh*)new Mesh();
		BulletRay::mesh->ReadMeshFromFile_OBJ("Resources/Mesh/RayMesh.obj", { 1.0f, 0.62f, 0.12f, 1.0f }, false);

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
		{
			auto& effects = GetParticleEffectRuntimes();
			for (const ParticleEffectRuntime& effect : effects) {
				InitParticlePool(*effect.Pool, effect.Count);
			}
			for (const ParticleEffectRuntime& effect : effects) {
				*effect.Compute = new ParticleCompute();
				(*effect.Compute)->Init(L"Particle.hlsl", effect.EntryPoint);
			}

			InitTransientParticlePool(gMuzzleFlashPool, gMuzzleFlashUpload, gMuzzleFlashParticles, MUZZLE_FLASH_COUNT);
			InitTransientParticlePool(gBulletTracerPool, gBulletTracerUpload, gBulletTracerParticles, BULLET_TRACER_COUNT);
		}

		ParticleDraw = new ParticleShader();
		ParticleDraw->InitShader();

		ParticleDraw->FireTexture = &FireTextureRes;

		OBBDebugMesh = new Mesh();
		OBBDebugMesh->CreateWallMesh(1.0f, 1.0f, 1.0f, vec4(1, 1, 1, 1));

		SetLight();

		gd.viewportArr[0].ProjectMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), (float)gd.ClientFrameWidth / (float)gd.ClientFrameHeight, 0.01f, 1000.0f);
	}
	// 어떤 존에서도 사용할 수 있는 글로벌 에셋들을 가져온다.

	// isAssetAddingInGlobal = true 면 이후 생성되는 리소스가 글로벌로 추가된다. 순서 보존이 중요하다.
	isAssetAddingInGlobal = true;
	const int syncedGlobalMaterialStart = MaterialTable.size();
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

			Model* Rifle_StreetSweeper = AddItemFunc(globalitem_index, ItemType::_Weapon, "Rifle_StreetSweeper",
				"Resources/Model/Rifle.model",
				L"Resources/UI/ItemIcons/ItemIcon_StreetSweeper.png",
				L"[돌격소총 - 스트리트스위퍼] : 한때 거리를 쓸어버렸던 클래식 돌격소총. 이제는 흔해 빠진 모델이다.");
			globalitem_index += 1;
			game.RifleModel = Rifle_StreetSweeper;

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
    const int syncedGlobalMaterialCount = GlobalMaterialCount - syncedGlobalMaterialStart;

#ifdef DEVELOPMODE_SYNC_GLOBAL_ASSET
	ofstream GlobalAssetCountFile{ "../../GlobalAssetCounter.txt" };
	if (GlobalAssetCountFile.is_open()) {
		GlobalAssetCountFile << GlobalTextureCount << " ";
        GlobalAssetCountFile << syncedGlobalMaterialCount << " ";
		GlobalAssetCountFile << GlobalMeshCount << " ";
		GlobalAssetCountFile << GlobalHumanoidAnimationCount << " ";
		GlobalAssetCountFile << Shape::ShapeTable.size();
	}
	GlobalAssetCountFile.close();
#endif

	for (int i = 0; i < 9; ++i) {
		gd.ShaderVisibleDescPool.ImmortalAlloc(&Immortal_ZoneLightBuffer_SRV[i], 1);
	}
	for (int i = 0; i < ZoneTable.size(); ++i) {
		ZoneTable[i]->GetImmortal_ZoneLightBuffer_SRV();
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
		mainPageStack[i]->depth_max = GetDepth(i+1);
		mainPageStack[i]->AlignUIDepth();
	}
}

DXUI* Game::GetSlotUIFromPos(vec4 pos)
{
	DXUI* selected = nullptr;
	for (int i = CurrentPageStack->size() - 1; i >= 0 ; ++i) {
		DXPage* page = CurrentPageStack->at(i);
		selected = page->GetSlotUIFromPos(pos);
		if (selected != nullptr) return selected;
	}
	return nullptr;
}

namespace {
void ComputeMapBounds(GameMap* map, vec4& outMin, vec4& outMax)
{
	outMin = vec4(INFINITY, INFINITY, INFINITY, 1.0f);
	outMax = vec4(-INFINITY, -INFINITY, -INFINITY, 1.0f);
	if (map == nullptr) return;
	for (int i = 0; i < map->MapObjects.size(); ++i) {
		StaticGameObject* go = map->MapObjects[i];
		if (go == nullptr) continue;
		BoundingOrientedBox obb = go->GetOBB();
		XMFLOAT3 corners[8];
		obb.GetCorners(corners);
		for (int k = 0; k < 8; ++k) {
			vec4 c = corners[k];
			outMin = _mm_min_ps(outMin, c);
			outMax = _mm_max_ps(outMax, c);
		}
	}
}

void ShiftMapObjects(GameMap* map, vec4 offset)
{
	if (map == nullptr) return;
	for (int i = 0; i < map->MapObjects.size(); ++i) {
		StaticGameObject* go = map->MapObjects[i];
		if (go == nullptr) continue;
		go->worldMat.pos += offset;
		go->worldMat.pos.w = 1.0f;
		if (gd.isSupportRaytracing) {
			go->RaytracingUpdateTransform();
		}
	}
}
}

void Game::SetCurrentZoneStaticObjects(int zoneId)
{
	//if (zoneId < 0 || zoneId >= LoadedZoneMaps.size()) return;
	//GameMap* currentMap = LoadedZoneMaps[zoneId];
	//if (currentMap == nullptr) return;
	//for (int i = 0; i < currentMap->MapObjects.size(); ++i) {
	//	StaticGameObjects.push_back(currentMap->MapObjects[i]);
	//}
}

vec4 Game::GetZoneWorldOffset(int zoneId) const
{
	//if (zoneId < 0 || zoneId >= ZoneCount) return vec4(0, 0, 0, 0);
	//return LinkedZoneWorldOffset[zoneId];
	return vec4(0);
}

vec4 Game::GetRenderedZoneOffset(int zoneId) const
{
	//return GetZoneWorldOffset(zoneId) - CurrentWorldShift;
	return vec4(0);
}

void Game::ApplyZoneOffsetToStaticObject(GameObject* go)
{
	if (go == nullptr) return;
	vec4 offset = GetRenderedZoneOffset(currentZoneId);
	go->worldMat.pos += offset;
	go->worldMat.pos.w = 1.0f;
}

void Game::ApplyZoneOffsetToDynamicObject(DynamicGameObject* go)
{
	if (go == nullptr) return;
	vec4 offset = GetRenderedZoneOffset(currentZoneId);
	go->DestPos += offset;
	go->DestPos.w = 1.0f;
	go->worldMat.pos = go->DestPos;
	go->worldMat.pos.w = 1.0f;
}

void Game::ApplyZoneOffsetToPortal(Portal* portal)
{
	if (portal == nullptr) return;
	vec4 offset = GetRenderedZoneOffset(currentZoneId);
	portal->worldMat.pos += offset;
	portal->worldMat.pos.w = 1.0f;
}

void Game::RefreshLoadedZoneMapTransforms()
{
	//for (int zoneId = 0; zoneId < LoadedZoneMaps.size(); ++zoneId) {
	//	GameMap* map = LoadedZoneMaps[zoneId];
	//	if (map == nullptr) continue;
	//	if (zoneId >= LoadedZoneOriginalPositions.size()) continue;

	//	vec4 offset = GetRenderedZoneOffset(zoneId);
	//	int count = min((int)map->MapObjects.size(), (int)LoadedZoneOriginalPositions[zoneId].size());
	//	for (int i = 0; i < count; ++i) {
	//		StaticGameObject* go = map->MapObjects[i];
	//		if (go == nullptr) continue;

	//		go->worldMat.pos = vec4(LoadedZoneOriginalPositions[zoneId][i]) + offset;
	//		go->worldMat.pos.w = 1.0f;
	//		if (gd.isSupportRaytracing) {
	//			go->RaytracingUpdateTransform();
	//		}
	//	}
	//}
}

void Game::RebuildStaticChunks()
{
	//GameChunk** gcarr = new GameChunk * [chunck.size()];
	//int index = 0;
	//for (auto c : chunck) {
	//	gcarr[index] = c.second;
	//	index += 1;
	//}
	//chunck.clear();
	//for (int i = 0; i < index; ++i) {
	//	GameChunk* gc = gcarr[i];
	//	gc->Release();
	//	delete gc;
	//}
	//delete[] gcarr;
}

// maybe .. present zone 's nearzone's map loading
/*
* spec :
* 1. must load all nearzone.
*/
void Game::LoadLinkedZoneMaps()
{
	bool loadingMap = false;
	for (int i = 0; i < 9; ++i) {
		Zone* nearzone = game.Current_Zone->nearZones[i];
		if (nearzone == nullptr) continue;
		if (nearzone->Map != nullptr) continue;
		loadingMap = true;
		nearzone->Map = new GameMap();
		nearzone->Map->LoadMap(nearzone->Load_MapName, nearzone->zoneid);

		//push object to chunck and bake aabb
		nearzone->Map->AABB[0] = INFINITY;
		nearzone->Map->AABB[1] = -INFINITY;
		for (int u = 0; u < nearzone->Map->MapObjects.size(); ++u) {
			nearzone->PushGameObject(nearzone->Map->MapObjects[u]);
			game.StaticGameObjects[Zone::MaxStaticObjectCount * i + u] = nearzone->Map->MapObjects[u];
			BoundingOrientedBox obb = nearzone->Map->MapObjects[u]->GetOBB();
			XMFLOAT3 corners[8];
			obb.GetCorners(corners);
			for (int k = 0; k < 8; ++k) {
				vec4 c = corners[k];
				nearzone->Map->AABB[0] = _mm_min_ps(c, nearzone->Map->AABB[0]);
				nearzone->Map->AABB[1] = _mm_max_ps(c, nearzone->Map->AABB[1]);
			}
		}
		nearzone->Map->AABB[0].w = -INFINITY;
		nearzone->Map->AABB[1].w = INFINITY;

		for (int u = 0; u < nearzone->LightTable.size(); ++u) {
			nearzone->PushLight(nearzone->LightTable[u]);
		}

		PrebuildStaticObjectAutoLOD();
	}
	if (loadingMap == false) return;
}

bool Game::BeginServerTransfer(const char* ip, unsigned short port, int dstZoneId, int transferToken)
{
	char ipLocal[64] = {};
	if (ip) {
		strncpy_s(ipLocal, ip, _TRUNCATE);
	}
	{
		char _dbg[256] = {};
		sprintf_s(_dbg, "[BeginServerTransfer] ENTER ip=\"%s\" port=%u dst=%d token=%d\n",
			ipLocal, (unsigned)port, dstZoneId, transferToken);
		OutputDebugStringA(_dbg); printf("%s", _dbg); fflush(stdout);
	}
	isPrepared = false;
	isPreparedClientIndex = false;
	isMapInit = false;
	clientIndexInServer = -1;
	playerGameObjectIndex = -1;
	player = nullptr;

	client.Disconnect();
	bool connected = client.Init(ipLocal, port);
	{
		char _dbg[128] = {};
		sprintf_s(_dbg, "[BeginServerTransfer] client.Init returned %d (lastErr=%d)\n",
			(int)connected, WSAGetLastError());
		OutputDebugStringA(_dbg); printf("%s", _dbg); fflush(stdout);
	}
	if (connected == false) {
		return false;
	}

	CTS_TransferConnect_Header header;
	header.size = sizeof(CTS_TransferConnect_Header);
	header.st = CTS_Protocol::TransferConnect;
	header.transferToken = transferToken;
	DWORD _sent = client.send((char*)&header, sizeof(CTS_TransferConnect_Header), 0);
	{
		char _dbg[128] = {};
		sprintf_s(_dbg, "[BeginServerTransfer] sent TransferConnect bytes=%u (lastErr=%d)\n",
			(unsigned)_sent, WSAGetLastError());
		OutputDebugStringA(_dbg); printf("%s", _dbg); fflush(stdout);
	}

	MoveZone(dstZoneId);
	isPrepared = true;
	return true;
}

void Game::ResendHeldMovementKeys()
{
	GetKeyboardState(pKeyBuffer);
	const int keys[] = { 'W', 'A', 'S', 'D', VK_SPACE };
	for (int i = 0; i < _countof(keys); ++i) {
		CTS_KeyInput_Header header;
		header.size = sizeof(CTS_KeyInput_Header);
		header.st = CTS_Protocol::KeyInput;
		header.Key = (char)keys[i];
		header.isdown = (pKeyBuffer[keys[i]] & 0xF0) != 0;
		client.send((char*)&header, sizeof(CTS_KeyInput_Header), 0);
	}
}

void Game::MoveZone(int zoneid) {
	bool CmdInitStateIsClose = gd.gpucmd.isClose;
	if (CmdInitStateIsClose) {
		gd.gpucmd.Reset();
	}
	gd.ShaderVisibleDescPool.DynamicReset();

	for (int i = 0; i < DynmaicGameObjects.size(); ++i) {
		if (DynmaicGameObjects[i]) {
			DynmaicGameObjects[i]->Release();
			delete DynmaicGameObjects[i];
			DynmaicGameObjects[i] = nullptr;
		}
	}
	DynmaicGameObjects.clear();
	player = nullptr;
	playerGameObjectIndex = -1;

	for (int i = 0; i < Portals.size(); ++i) {
		if (Portals[i]) {
			delete Portals[i];
		}
	}
	Portals.clear();

	isAssetAddingInGlobal = false;
	LoadLinkedZoneMaps();

	//ReLoadMap
	//if (Map) {
	//	Map->Release();
	//}
	//else {
	//	Map = new GameMap();
	//}
	//isAssetAddingInGlobal = false;
	//Map->LoadMap(ZoneIDToMapName[zoneid]);
	//game.StaticGameObjects.reserve(Map->MapObjects.size());
	//Map->AABB[0] = INFINITY;
	//Map->AABB[1] = -INFINITY;
	//for (int i = 0; i < Map->MapObjects.size(); ++i) {
	//	PushGameObject(Map->MapObjects[i]);
	//	game.StaticGameObjects.push_back(Map->MapObjects[i]);
	//	BoundingOrientedBox obb = Map->MapObjects[i]->GetOBB();
	//	XMFLOAT3 corners[8];
	//	obb.GetCorners(corners);
	//	for (int k = 0; k < 8; ++k) {
	//		vec4 c = corners[k];
	//		Map->AABB[0] = _mm_min_ps(c, Map->AABB[0]);
	//		Map->AABB[1] = _mm_max_ps(c, Map->AABB[1]);
	//	}
	//}
	//Map->AABB[0].w = -INFINITY;
	//Map->AABB[1].w = INFINITY;
	//for (int i = 0; i < game.LightTable.size(); ++i) {
	//	game.PushLight(game.LightTable[i]);
	//}


	for (int i = 0; i < GlobalMaterialCount; ++i) {
		Material* mat = MaterialTable[i];
		if (isAssetAddingInGlobal == false) {
			mat->SetDescTable();
			RenderMaterialTable.push_back(mat);
		}
	}

	gd.gpucmd.Close();
	gd.gpucmd.Execute();
	gd.gpucmd.WaitGPUComplete();
	if (CmdInitStateIsClose == false) {
		gd.gpucmd.Reset();
	}

	currentZoneId = zoneid;
	//if (zoneid >= 0 && zoneid < LoadedZoneMaps.size()) {
	//	//CurrentWorldShift = LinkedZoneWorldOffset[zoneid];
	//	//RefreshLoadedZoneMapTransforms();
	//	//Map = LoadedZoneMaps[zoneid];
	//	//SetCurrentZoneStaticObjects(zoneid);

	//
	//}
	//RebuildStaticChunks();

	isMapInit = true;
	isAssetAddingInGlobal = true;
	ResendHeldMovementKeys();
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

	for (int i = 0; i < 9; ++i) {
		Zone* nearzone = game.Current_Zone->nearZones[i];
		if (nearzone == nullptr) continue;
		if (nearzone->bReqireBakeLight_Raster) {
			if (nearzone->ZoneLightChuncks.resource) nearzone->ZoneLightChuncks.Release();
			if (nearzone->ZoneLightChuncks_Mapped) nearzone->ZoneLightChuncks_Mapped = nullptr;

			vec4 Counts = (nearzone->Map->AABB[1] - nearzone->Map->AABB[0]) / Zone::chunck_divide_Width;
			nearzone->ChunckCountX = floor(Counts.x + 1);
			nearzone->ChunckCountY = floor(Counts.y + 1);
			nearzone->ChunckCountZ = floor(Counts.z + 1);

			int ix = floor(nearzone->Map->AABB[0].x / Zone::chunck_divide_Width);
			int iy = floor(nearzone->Map->AABB[0].y / Zone::chunck_divide_Width);
			int iz = floor(nearzone->Map->AABB[0].z / Zone::chunck_divide_Width);
			ChunkIndex startci = ChunkIndex(ix, iy, iz);
			BoundingBox bb = startci.GetAABB();
			LightCBData_withShadow[nearzone->Asset_OffsetMul]->ChunckStart = bb.Center;
			LightCBData_withShadow[nearzone->Asset_OffsetMul]->ChunckStart -= vec4(bb.Extents);
			LightCBData_withShadow[nearzone->Asset_OffsetMul]->ChunckCount[0] = nearzone->ChunckCountX;
			LightCBData_withShadow[nearzone->Asset_OffsetMul]->ChunckCount[1] = nearzone->ChunckCountY;
			LightCBData_withShadow[nearzone->Asset_OffsetMul]->ChunckCount[2] = nearzone->ChunckCountZ;

			UINT ncbElementBytes = ((sizeof(ChunckLightData) * (nearzone->ChunckCountX * nearzone->ChunckCountY * nearzone->ChunckCountZ) + 255) & ~255); // 256의 배수
			nearzone->ZoneLightChuncks = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1);
			nearzone->ZoneLightChuncks.resource->Map(0, NULL, (void**)&nearzone->ZoneLightChuncks_Mapped);
			ZeroMemory(nearzone->ZoneLightChuncks_Mapped, ncbElementBytes);
			for (auto f : nearzone->chunck) {
				GameChunk* gc = f.second;
				ChunkIndex ci = f.first;
				ChunkIndex rci;
				rci.x = ci.x - startci.x;
				rci.y = ci.y - startci.y;
				rci.z = ci.z - startci.z;
				int index = rci.z + rci.y * nearzone->ChunckCountZ + rci.x * nearzone->ChunckCountZ * nearzone->ChunckCountY;
				if (index > nearzone->ChunckCountX * nearzone->ChunckCountY * nearzone->ChunckCountZ) continue;
				ChunckLightData& LightChunck = nearzone->ZoneLightChuncks_Mapped[index];
				int maxSiz = min(gc->Lights.size(), 32);
				for (int i = 0; i < maxSiz; ++i) {
					LightChunck.lights[i] = *gc->Lights[i];
				}
				LightChunck.lights[0].MaxLightCount = maxSiz;
			}
			nearzone->ZoneLightChuncks.resource->Unmap(0, nullptr);

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.NumElements = nearzone->ChunckCountX * nearzone->ChunckCountY * nearzone->ChunckCountZ;
			srvDesc.Buffer.StructureByteStride = sizeof(ChunckLightData);
			srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			gd.pDevice->CreateShaderResourceView(nearzone->ZoneLightChuncks.resource, &srvDesc, nearzone->Immortal_ZoneLightBuffer_SRV.hCreation.hcpu);

			nearzone->bReqireBakeLight_Raster = false;
		}
	}


	for (int i = 0;i < gd.addSDFTextureStack.size();++i) {
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
	BoxLOD_BeginFrame();

	//2.5. 인스턴싱 이름 수집
	if (SceneRenderBatch) {
		game.renderViewPort = &gd.viewportArr[0];
		SetRenderMod(SceneRenderBatch);
		ClearAllMeshInstancing();
		BoxLOD_ClearQueue();
		RenderTour<false>();
		SetRenderMod(false);
		BoxLOD_ClearQueue();

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

	//gd.DeviceRemoveResonDebug();

	//6. 커맨드리스트 리셋 및 렌더 패스 준비
	HRESULT hResult = gd.gpucmd.Reset();

	for (int i = 0; i < 3; ++i) {
		gd.gpucmd.ResBarrierTr(&game.MyDirLight[i].ShadowMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	//8. 서브렌더타겟 STATE를 RENDER_TARGET으로 전환
	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET);

	//9. 뷰포트/시저렉트 설정
	gd.gpucmd->RSSetViewports(1, &gd.viewportArr[0].Viewport);
	gd.gpucmd->RSSetScissorRects(1, &gd.viewportArr[0].ScissorRect);
	game.renderViewPort = &gd.viewportArr[0];

	//10. 뎁스 스텐실 버퍼 핸들을 가져온다.
	D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle =
		gd.pDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	//11. 서브렌더타겟으로 렌더타겟 설정
	gd.gpucmd->OMSetRenderTargets(1, &gd.SubRenderTarget.rtvHandle.hcpu, TRUE,
		&d3dDsvCPUDescriptorHandle);

	//12. 렌더타겟 클리어
	float pfClearColor[4] = { 0, 0, 0, 1.0f };
	gd.gpucmd->ClearRenderTargetView(gd.SubRenderTarget.rtvHandle.hcpu, pfClearColor, 0, NULL);

	//render begin ----------------------------------------------------------------

	MySkyBoxShader->RenderSkyBox();

	// 14. 하늘 상자가 모든 오브젝트 뒤에 보이도록 DepthStencil을 클리어
	gd.gpucmd->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);

	// 15. 카메라로 사용할 행렬 초기화
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
			gd.gpucmd->SetGraphicsRootConstantBufferView(PRID::CBV_StaticLight, game.LightCB_withShadowResource[game.Current_Zone->Asset_OffsetMul].resource->GetGPUVirtualAddress());
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_ShadowMap, game.MyDirLight[0].descindex.hRender.hgpu);
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_EnvionmentMap, game.MySkyBoxShader->CurrentSkyBox.descindex.hRender.hgpu);
			if(game.Current_Zone->bReqireBakeLight_Raster == false) gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_Chunck_StaticLightStructuredBuffer, game.Current_Zone->Immortal_ZoneLightBuffer_SRV.hRender.hgpu);
		}
	};


	// 16 ~ 17. 터레인 테셀레이션 생략

	// 18. 모든 게임오브젝트들을 출력한다.
	dbgc[0] = 0;
	//gd.AverageTimerStart();
	matrix idmat;
	idmat.Id();
	if (!SceneRenderBatch) BoxLOD_BeginFrame();
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
		BoxLOD_FlushQueued();
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
			gd.gpucmd->SetGraphicsRootConstantBufferView(PRID::CBV_StaticLight, game.LightCB_withShadowResource[game.Current_Zone->Asset_OffsetMul].resource->GetGPUVirtualAddress());
			// 18-3. Direction Light
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_ShadowMap, game.MyDirLight[0].descindex.hRender.hgpu);
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_EnvionmentMap, game.MySkyBoxShader->CurrentSkyBox.descindex.hRender.hgpu);
			if (game.Current_Zone->bReqireBakeLight_Raster == false)gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_Chunck_StaticLightStructuredBuffer, game.Current_Zone->Immortal_ZoneLightBuffer_SRV.hRender.hgpu);
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
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::CBVTable_SkinMeshLightData, game.LightCB_withShadowResource[game.Current_Zone->Asset_OffsetMul].descindex.hRender.hgpu);
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_SkinMeshShadowMaps, game.MyDirLight[0].descindex.hRender.hgpu);
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_SKinMeshEnvironmentMap, game.MySkyBoxShader->CurrentSkyBox.descindex.hRender.hgpu);
			if (game.Current_Zone->bReqireBakeLight_Raster == false)gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_SKinMesh_Chunck_StaticLightStructuredBuffer, game.Current_Zone->Immortal_ZoneLightBuffer_SRV.hRender.hgpu);
		}
		RenderTour<true>();
		for (int i = 0; i < DynmaicGameObjects.size(); ++i) {
			SkinMeshGameObject* smgo = dynamic_cast<SkinMeshGameObject*>(DynmaicGameObjects[i]);
			if (smgo == nullptr) continue;
			if (smgo->tag[GameObjectTag::Tag_Enable] == false) continue;
			if (smgo->tag[GameObjectTag::Tag_SkinMeshObject] == false) continue;
			if (smgo->TourID == TourID) continue;
			(smgo->*SkinMeshGameObject::CurrentRenderFunc)(idmat);
			smgo->TourID = TourID;
		}

		if (game.player != nullptr && game.bFirstPersonVision == false) {
			BindStaticPBRRenderState();
			game.player->Render_ThirdPersonWeapon();
		}
		BoxLOD_FlushQueued();
	}

	//Render Items
	// already droped items. (non move..)
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

	{
		auto& effects = GetParticleEffectRuntimes();
		for (ParticleEffectRuntime& effect : effects) {
			if (!effect.Active || *effect.Compute == nullptr) continue;

			effect.Emitter.Age += DeltaTime;
			if (effect.Emitter.Duration > 0.0f && effect.Emitter.Age > effect.Emitter.Duration) {
				effect.Active = false;
				continue;
			}

			ParticleEmitterCB emitter = effect.Emitter;
			if (effect.PendingReset) emitter.Flags |= PARTICLE_EMITTER_FLAG_RESET;
			(*effect.Compute)->Dispatch(gd.gpucmd, &effect.Pool->Buffer, effect.Pool->Count, DeltaTime, emitter);
			effect.PendingReset = false;

			ParticleDraw->FireTexture = GetParticleSpriteResource(*effect.SpriteSlot);
			ParticleDraw->Render(gd.gpucmd, &effect.Pool->Buffer, effect.Pool->Count);
		}

		TickTransientParticles(gMuzzleFlashParticles, DeltaTime);
		TickTransientParticles(gBulletTracerParticles, DeltaTime);
		UploadTransientParticles(gMuzzleFlashPool, gMuzzleFlashUpload, gMuzzleFlashParticles);
		UploadTransientParticles(gBulletTracerPool, gBulletTracerUpload, gBulletTracerParticles);

		ParticleDraw->FireTexture = GetParticleSpriteResource(gMuzzleFlashSpriteSlot);
		ParticleDraw->Render(gd.gpucmd, &gMuzzleFlashPool.Buffer, gMuzzleFlashPool.Count);

		ParticleDraw->FireTexture = GetParticleSpriteResource(gBulletTracerSpriteSlot);
		ParticleDraw->Render(gd.gpucmd, &gBulletTracerPool.Buffer, gBulletTracerPool.Count);
	}


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
		for (int i = 0;i < game.StaticGameObjects.size();++i) {
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
	hResult = gd.gpucmd.Close();
	gd.gpucmd.Execute();
	gd.gpucmd.WaitGPUComplete();

	//Bluring + DepthConvolution(주목받는 물체의 선을 더욱 블러링 시킨다.) (Compute Shader)
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
		for (int i = 0; i < DynmaicGameObjects.size(); ++i) {
			SkinMeshGameObject* smgo = dynamic_cast<SkinMeshGameObject*>(DynmaicGameObjects[i]);
			if (smgo == nullptr) continue;
			if (smgo->tag[GameObjectTag::Tag_Enable] == false) continue;
			if (smgo->tag[GameObjectTag::Tag_SkinMeshObject] == false) continue;
			if (smgo->TourID == TourID) continue;
			(smgo->*SkinMeshGameObject::CurrentRenderFunc)(XMMatrixIdentity());
			smgo->TourID = TourID;
		}
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

	SkinMeshGameObject::CurrentRenderFunc = &SkinMeshGameObject::Render;
	TourID += 1;

	gd.gpucmd.Reset();
	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET);
	gd.gpucmd.ResBarrierTr(gd.pDepthStencilBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	//9. 뷰포트/시저렉트 설정
	gd.gpucmd->RSSetViewports(1, &gd.viewportArr[0].Viewport);
	gd.gpucmd->RSSetScissorRects(1, &gd.viewportArr[0].ScissorRect);
	game.renderViewPort = &gd.viewportArr[0];

	//10. 뎁스 스텐실 버퍼 핸들을 가져온다.
	d3dDsvCPUDescriptorHandle =
		gd.pDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	//11. 서브렌더타겟으로 렌더타겟 설정
	gd.gpucmd->OMSetRenderTargets(1, &gd.SubRenderTarget.rtvHandle.hcpu, TRUE,
		&d3dDsvCPUDescriptorHandle);

	//gd.gpucmd.pCommandAllocator->Reset(); // origin : m_commandAllocators[m_backBufferIndex]->Reset()
	gd.gpucmd->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);

	// 25. 플레이어를 DepthStencil 클리어 후 추가 렌더링한다.
	float hhpp = 0;
	float Attack = 0;
	float Defense = 0;
	float HeatGauge = 0;
	int kill = 0;
	int death = 0;
	int bulletCount = 0;
	float SkillCooldownFlow = 0;
	if (game.player != nullptr) {
		BindStaticPBRRenderState();
		game.player->Render_AfterDepthClear();
		hhpp = game.player->HP;
		Attack = game.player->Attack;
		Defense = game.player->Defense;
		HeatGauge = game.player->HeatGauge;
		kill = game.player->KillCount;
		death = game.player->DeathCount;
		bulletCount = game.player->bullets;
		SkillCooldownFlow = game.player->SkillCooldownFlow[(int)SkillSlot::Skill1];
	}

	// 26. UI 텍스트 렌더링
	// HP
	//maybe..1706x960
	float Rate = gd.ClientFrameHeight / 960.0f;
	vec4 rt = Rate * vec4(-1650, 850, -1000, 700);


	gd.gpucmd.SetShader(game.MyScreenShader, ShaderType::RenderNormal);
	vector<DXPage*>* savePageStack = game.CurrentPageStack;
	game.CurrentPageStack = &game.mainPageStack;
	for (int i = 0; i < game.CurrentPageStack->size(); ++i) {
		DXPage* page = game.CurrentPageStack->at(i);
		page->Render();
	}
	// 27.
	MyScreenShader->RenderAllSDFTexts();

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
	for (int i = 0; i < 9; ++i) {
		gd.raytracing.MappedCB[i]->DirLight_invDirection = vec4(vec4(0) - LightDirection).f3;
	}

	for (int i = 0; i < 9; ++i) {
		Zone* nearZone = game.Current_Zone->nearZones[i];
		if (nearZone == nullptr) continue;
		if (nearZone->bReqireBakeLight_Raytracing) {
			vec4 Counts = (nearZone->Map->AABB[1] - nearZone->Map->AABB[0]) / Zone::chunck_divide_Width;
			nearZone->ChunckCountX = floor(Counts.x + 1);
			nearZone->ChunckCountY = floor(Counts.y + 1);
			nearZone->ChunckCountZ = floor(Counts.z + 1);

			int ix = floor(nearZone->Map->AABB[0].x / Zone::chunck_divide_Width);
			int iy = floor(nearZone->Map->AABB[0].y / Zone::chunck_divide_Width);
			int iz = floor(nearZone->Map->AABB[0].z / Zone::chunck_divide_Width);
			ChunkIndex startci = ChunkIndex(ix, iy, iz);
			BoundingBox bb = startci.GetAABB();
			gd.raytracing.MappedCB[nearZone->Asset_OffsetMul]->ChunckStart = bb.Center;
			gd.raytracing.MappedCB[nearZone->Asset_OffsetMul]->ChunckStart -= vec4(bb.Extents);
			gd.raytracing.MappedCB[nearZone->Asset_OffsetMul]->ChunckCount[0] = nearZone->ChunckCountX;
			gd.raytracing.MappedCB[nearZone->Asset_OffsetMul]->ChunckCount[1] = nearZone->ChunckCountY;
			gd.raytracing.MappedCB[nearZone->Asset_OffsetMul]->ChunckCount[2] = nearZone->ChunckCountZ;
			nearZone->bReqireBakeLight_Raytracing = false;
		}
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
	commandList->SetComputeRootConstantBufferView(3, gd.raytracing.CameraCB[game.Current_Zone->Asset_OffsetMul]->GetGPUVirtualAddress()); // Camera CB CBV
	commandList->SetComputeRootDescriptorTable(4, RayTracingMesh::VBIB_DescIndex.hRender.hgpu); // Vertex, IndexBuffer SRV
	commandList->SetComputeRootDescriptorTable(5, MySkyBoxShader->CurrentSkyBox.descindex.hRender.hgpu); // SkyBox SRV
	commandList->SetComputeRootDescriptorTable(6, RayTracingMesh::UAV_VBIB_DescIndex.hRender.hgpu); // SkinMesh SRV

	commandList->SetComputeRootDescriptorTable(7, Material::MaterialStructuredBufferSRV.hRender.hgpu); // Material Arr
	DescIndex texarrSRV = DescIndex(true, gd.ShaderVisibleDescPool.TextureSRVStart);

	commandList->SetComputeRootDescriptorTable(8, game.Current_Zone->Immortal_ZoneLightBuffer_SRV.hRender.hgpu); // ZoneLightChuncks Arr
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
	constexpr float CascadeRange[4] = { 0.01f, 50.0f, 200.0f, 1000.0f };


	// 2. 렌더링을 시작한다.
	HRESULT hResult = gd.gpucmd.Reset();


	for (int i = 0; i < 3; ++i) {
		matrix viewproj;
		viewproj = gd.viewportArr[0].ViewMatrix;
		matrix proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), (float)gd.ClientFrameWidth / (float)gd.ClientFrameHeight, CascadeRange[i], CascadeRange[i+1]);
		viewproj *= proj;
		vec4 LightDirQ = vec4::DirectionToQuaternion(LightDirection);
		LightOBB = game.MyDirLight[0].viewport.GetOBB_IncludeFrustum(viewproj, LightDirQ);

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
		game.MyDirLight[i].viewport.ScissorRect = {0, 0, (long)ShadowResolusion, (long)ShadowResolusion};


		game.MyDirLight[i].viewport.Camera_Pos = obj - (LightDirection * LightDistance);
		game.MyDirLight[i].viewport.Camera_Pos.w = 0;
		game.MyDirLight[i].LightPos = game.MyDirLight[i].viewport.Camera_Pos;

		vec4 up = vec4(0, 1, 0, 0);
		if (fabs(LightDirection.dot3(up)) > 0.99f) {
			up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
		}
		MyDirLight[i].View.mat = XMMatrixLookAtLH(MyDirLight[i].LightPos, obj, up);
		game.MyDirLight[i].viewport.ViewMatrix = MyDirLight[i].View;

		constexpr float rate = 1.0f / 64.0f;
		game.MyDirLight[i].viewport.ProjectMatrix = XMMatrixOrthographicLH(MaxWidth, MaxWidth/*rate * ShadowResolusion, rate * ShadowResolusion*/, 0.1f, 4000.0f);

		matrix projmat = XMMatrixTranspose(MyDirLight[i].viewport.ProjectMatrix);
		LightCB_withShadowResource[game.Current_Zone->Asset_OffsetMul].resource->Map(0, NULL, (void**)&LightCBData_withShadow);
		LightCBData_withShadow[game.Current_Zone->Asset_OffsetMul]->LightProjection[i] = projmat;
		LightCBData_withShadow[game.Current_Zone->Asset_OffsetMul]->LightView[i] = XMMatrixTranspose(MyDirLight[i].viewport.ViewMatrix);
		LightCBData_withShadow[game.Current_Zone->Asset_OffsetMul]->LightPos[i] = MyDirLight[i].LightPos.f3;
		LightCB_withShadowResource[game.Current_Zone->Asset_OffsetMul].resource->Unmap(0, nullptr);

		MappedDirLightData->DirLightView = LightCBData_withShadow[game.Current_Zone->Asset_OffsetMul]->LightView[i];
		MappedDirLightData->DirLightProjection = projmat;
		MappedDirLightData->DirLightPos = MyDirLight[i].LightPos.f3;
		MappedDirLightData->DirLightDir = LightDirection;
		MappedDirLightData->DirLightColor = vec4(1, 1, 1, 1);

		gd.gpucmd->RSSetViewports(1, &game.MyDirLight[i].viewport.Viewport);
		gd.gpucmd->RSSetScissorRects(1, &game.MyDirLight[i].viewport.ScissorRect);
		gd.gpucmd.ResBarrierTr(&game.MyDirLight[i].ShadowMap, D3D12_RESOURCE_STATE_DEPTH_WRITE);

		//D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle =
		//	gd.pDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		DescHandle dc = game.MyDirLight[i].ShadowMap.descindex.hRender;
		gd.gpucmd->OMSetRenderTargets(0, nullptr, TRUE, &dc.hcpu);
		gd.gpucmd->ClearDepthStencilView(game.MyDirLight[i].ShadowMap.descindex.hRender.hcpu,
			D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);

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
		const bool previousAutoLODRenderActive = AutoLOD_IsModelLODRenderActive();
		const int previousAutoLODRenderLevel = AutoLOD_GetModelLODRenderLevel();
		AutoLOD_SetModelLODRenderActive(false);
		AutoLOD_SetModelLODRenderLevel(0);

		ShadowRenderSkinMeshObjArr.clear();
		game.TourID += 1;
		GameObjectIncludeChunks goic = game.Current_Zone->GetChunks_Include_OBB(LightOBB);
		//goic.ylen -= (1 + game.Map->AABB[1].y / game.chunck_divide_Width) - goic.ymin;
		ChunkIndex ci = ChunkIndex(goic.xmin, goic.ymin, goic.zmin);
		int ChunckSiz = goic.GetChunckSize();
		for (; ci.extra < ChunckSiz; goic.Inc(ci)) {
			auto f = game.Current_Zone->chunck.find(ci);
			if (f != game.Current_Zone->chunck.end()) {
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
		gd.gpucmd->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4View, 0);
		gd.gpucmd->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16);
		//gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::CBVTable_SkinMeshLightData, game.LightCB_withShadowResource.descindex.hRender.hgpu);
		//gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_SkinMeshShadowMaps, game.MyDirLight[0].descindex.hRender.hgpu);

		gd.gpucmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
		for (int i = 0; i < ShadowRenderSkinMeshObjArr.size(); ++i) {
			Mesh* mesh = nullptr;
			Model* model = nullptr;
			ShadowRenderSkinMeshObjArr[i]->shape.GetRealShape(mesh, model);
			model->RootNode->SkinMeshShadowRender(model, gd.gpucmd, XMMatrixIdentity(), ShadowRenderSkinMeshObjArr[i]);
		}
		AutoLOD_SetModelLODRenderLevel(previousAutoLODRenderLevel);
		AutoLOD_SetModelLODRenderActive(previousAutoLODRenderActive);
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
	AutoLOD_ProcessRuntimeQueue(1);
	BoxLOD_DebugUpdate(DeltaTime);

	static bool wasLODKeyDown = false;
	const bool isLODKeyDown = (GetAsyncKeyState('L') & 0x8000) != 0;
	if (isLODKeyDown && !wasLODKeyDown && GetActiveWindow() == hWnd) {
		const bool active = !AutoLOD_IsModelLODRenderActive();
		AutoLOD_SetModelLODRenderActive(active);
		OutputDebugStringA(active ? "[AutoLOD] render toggle: ON\n" : "[AutoLOD] render toggle: OFF\n");
	}
	wasLODKeyDown = isLODKeyDown;

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
	for (int i = 0; i < 9; ++i) {
		LightCBData_withShadow[i]->dirlight.gLightDirection = LightDirection.f3;
	}

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
            OutputDebugStringA("[ClientRecv] server closed connection\n");
			//client.DisConnectToServer();
			break;
		}
		else break; // 더 읽을 패킷이 없으면 종료
	}
	//gd.AverageSecPer60End(Update_ClientRecv);

	if (player == nullptr && playerGameObjectIndex >= 0 && playerGameObjectIndex < DynmaicGameObjects.size()) {
		Player* localPlayer = dynamic_cast<Player*>(DynmaicGameObjects[playerGameObjectIndex]);
		if (localPlayer != nullptr) {
			player = localPlayer;
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
			if (isPreparedClientIndex && isMapInit) {
				isPrepared = true;
			}
		}
	}

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
		{
			static int _dbgCnt = 0;
			if ((_dbgCnt++ % 120) == 0) {
				int total = (int)DynmaicGameObjects.size();
				int nonNull = 0, enabled = 0, withShape = 0;
				for (int i = 0; i < total; ++i) {
					if (DynmaicGameObjects[i] == nullptr) continue;
					nonNull++;
					if (DynmaicGameObjects[i]->tag[GameObjectTag::Tag_Enable]) enabled++;
					if (DynmaicGameObjects[i]->shape.FlagPtr != 0) withShape++;
				}
				int chunkCnt = (int)Current_Zone->chunck.size();
				int chunkSkin = 0, chunkDyn = 0, chunkStatic = 0;
				for (auto& _ch : Current_Zone->chunck) {
					GameChunk* _c = _ch.second;
					chunkSkin += _c->SkinMesh_gameobjects.size;
					chunkDyn += _c->Dynamic_gameobjects.size;
					chunkStatic += _c->Static_gameobjects.size;
				}
				char _dbg[320] = {};
				sprintf_s(_dbg, "[DynStat] total=%d nonNull=%d enabled=%d withShape=%d player=%p | chunks=%d skin=%d dyn=%d static=%d\n",
					total, nonNull, enabled, withShape, (void*)player,
					chunkCnt, chunkSkin, chunkDyn, chunkStatic);
				OutputDebugStringA(_dbg);
				printf("%s", _dbg);
				fflush(stdout);
				if (player != nullptr) {
					BoundingOrientedBox _obb = player->GetOBB();
					char _dbg2[320] = {};
					sprintf_s(_dbg2, "[PlayerInfo] pos=(%.2f,%.2f,%.2f) obbCenter=(%.2f,%.2f,%.2f) obbExt=(%.2f,%.2f,%.2f) shape=%p\n",
						player->worldMat.pos.f3.x, player->worldMat.pos.f3.y, player->worldMat.pos.f3.z,
						_obb.Center.x, _obb.Center.y, _obb.Center.z,
						_obb.Extents.x, _obb.Extents.y, _obb.Extents.z,
						player->shape.FlagPtr);
					printf("%s", _dbg2); fflush(stdout);
				}
				{
					auto& _vp = gd.viewportArr[0];
					vec4 _cp = _vp.Camera_Pos;
					char _dbg3[200] = {};
					sprintf_s(_dbg3, "[Cam] pos=(%.2f,%.2f,%.2f)\n",
						_cp.f3.x, _cp.f3.y, _cp.f3.z);
					printf("%s", _dbg3); fflush(stdout);
				}
			}
		}
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
    char dbg[128] = {};
    //sprintf_s(dbg, "[ClientReceiving] bytes=%d\n", totallen);
    OutputDebugStringA(dbg);
	int offset = 0;
	unsigned int size;
	STC_Protocol type = STC_Protocol::SyncGameObject;
READ_START:
	if (offset + (int)sizeof(unsigned int) > totallen) {
		return offset;
	}
	size = *(unsigned int*)currentPivot;
	if (offset + size >= totallen) {
		return offset;
	}
	type = *(STC_Protocol*)(currentPivot + sizeof(int));
    //sprintf_s(dbg, "[ClientReceiving] size=%u type=%d offset=%d\n", size, (int)type, offset);
    //OutputDebugStringA(dbg);
	switch (type) {
	case STC_Protocol::SyncGameObject:
	{
        //OutputDebugStringA("[ClientReceiving] SyncGameObject\n");
		STC_SyncGameObject_Header& header = *(STC_SyncGameObject_Header*)currentPivot;
		char* datapivot = currentPivot + sizeof(STC_SyncGameObject_Header);
		switch (header.type) {
		case GameObjectType::_GameObject:
		case GameObjectType::_StaticGameObject:
		{
			if (header.objindex >= StaticGameObjects.size()) {
				break;
			}

			if (StaticGameObjects[header.objindex]) {
				if (*(void**)StaticGameObjects[header.objindex] != GameObjectType::vptr[header.type]) {
					delete StaticGameObjects[header.objindex];
					StaticGameObjects[header.objindex] = nullptr;
				}
			}
			if (StaticGameObjects[header.objindex] == nullptr) {
				switch (header.type) {
				case GameObjectType::_StaticGameObject:
					StaticGameObjects[header.objindex] = new StaticGameObject();
					break;
				case GameObjectType::_GameObject:
					StaticGameObjects[header.objindex] = new StaticGameObject();
					break;
				}
			}
			StaticGameObjects[header.objindex]->RecvSTC_SyncObj(datapivot);
			game.ApplyZoneOffsetToStaticObject(StaticGameObjects[header.objindex]);
			game.Current_Zone->PushGameObject(StaticGameObjects[header.objindex]);
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
			game.ApplyZoneOffsetToDynamicObject(DynmaicGameObjects[header.objindex]);
			if (DynmaicGameObjects[header.objindex]->tag[GameObjectTag::Tag_SkinMeshObject]) {
				SkinMeshGameObject* smgo = (SkinMeshGameObject*)DynmaicGameObjects[header.objindex];
				smgo->InitRootBoneMatrixs();
			}
			game.Current_Zone->PushGameObject(DynmaicGameObjects[header.objindex]);
			{
				DynamicGameObject* _dgo = DynmaicGameObjects[header.objindex];
				char _dbg[256] = {};
				sprintf_s(_dbg, "[Dyn] idx=%d type=%d enable=%d shapeFlag=%p pos=(%.2f,%.2f,%.2f)\n",
					header.objindex, (int)header.type,
					(int)_dgo->tag[GameObjectTag::Tag_Enable], _dgo->shape.FlagPtr,
					_dgo->worldMat.pos.f3.x, _dgo->worldMat.pos.f3.y, _dgo->worldMat.pos.f3.z);
				OutputDebugStringA(_dbg);
				printf("%s", _dbg);
				fflush(stdout);
			}
			if (header.type == GameObjectType::_Player && game.playerGameObjectIndex == header.objindex) {
				game.player = (Player*)DynmaicGameObjects[header.objindex];
				game.player->GunModel = game.GunModel;
				if (game.player->GunModel) {
					game.player->gunBarrelNodeIndices.clear();
					auto addBarrel = [&](const char* name) {
						int idx = game.player->GunModel->FindNodeIndexByName(name);
						if (idx >= 0) game.player->gunBarrelNodeIndices.push_back(idx);
					};
					addBarrel("Cylinder.107");
					addBarrel("Cylinder.108");
					addBarrel("Cylinder.109");
					addBarrel("Cylinder.110");
				}
				game.player->gunMatrix_thirdPersonView.Id();
				game.player->gunMatrix_thirdPersonView.pos = vec4(0.35f, 0.5f, 0, 1);
				game.player->gunMatrix_firstPersonView.Id();
				game.player->gunMatrix_firstPersonView.pos = vec4(0.13f, -0.15f, 0.5f, 1);
				game.player->gunMatrix_firstPersonView.LookAt(vec4(0, 0, 5) - game.player->gunMatrix_firstPersonView.pos);
				game.player->ShootPointMesh = game.ShootPointMesh;
				game.player->HPBarMesh = game.HPBarMesh;
				game.player->HPMatrix.pos = vec4(-1, 1, 1, 1);
				game.player->HPMatrix.LookAt(vec4(-1, 0, 0));
				game.player->HeatBarMesh = game.HeatBarMesh;
				game.player->HeatBarMatrix.pos = vec4(-1, 1, 1, 1);
				game.player->HeatBarMatrix.LookAt(vec4(-1, 0, 0));
				if (game.isPreparedClientIndex && game.isMapInit) {
					game.isPrepared = true;
				}
			}
		}
			break;
		case GameObjectType::_Portal:
		{
			Portal* portal = new Portal();
			portal->RecvSTC_SyncObj(datapivot);
			game.ApplyZoneOffsetToPortal(portal);
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

		short _typeShort = (short)header.type;
		if (_typeShort < 0 || _typeShort >= GameObjectType::ObjectTypeCount) {
			char _dbg[192] = {};
			sprintf_s(_dbg, "[ChangeMember] BAD type=%d objidx=%d srvoff=%d datasiz=%d size=%u\n",
				(int)_typeShort, header.objindex, header.serveroffset, header.datasize, header.size);
			OutputDebugStringA(_dbg); printf("%s", _dbg); fflush(stdout);
		}
		else if (header.objindex < 0 || header.objindex >= (int)DynmaicGameObjects.size()
			|| DynmaicGameObjects[header.objindex] == nullptr) {
			static int _badIdxCnt = 0;
			if ((_badIdxCnt++ % 60) == 0) {
				char _dbg[192] = {};
				sprintf_s(_dbg, "[ChangeMember] BAD objidx=%d (size=%zu null=%d) type=%d srvoff=%d\n",
					header.objindex, DynmaicGameObjects.size(),
					(header.objindex >= 0 && header.objindex < (int)DynmaicGameObjects.size()) ? (DynmaicGameObjects[header.objindex] == nullptr ? 1 : 0) : -1,
					(int)_typeShort, header.serveroffset);
				OutputDebugStringA(_dbg); printf("%s", _dbg); fflush(stdout);
			}
		}
		else {
			auto& curMap = GameObjectType::STC_OffsetMap[_typeShort];
			auto f = curMap.find(header.serveroffset);
			if (f != curMap.end()) {
				SyncWay& sw = f->second;
				char* source = datapivot;
				if (sw.clientOffset == -1) {
					if (sw.syncfunc) {
						sw.syncfunc(DynmaicGameObjects[header.objindex], source, header.datasize);
					}
				}
				else {
					char* dest = (((char*)DynmaicGameObjects[header.objindex]) + sw.clientOffset);
					memcpy(dest, source, header.datasize);
				}
			}
			else {
				static int _missCnt = 0;
				if ((_missCnt++ % 240) == 0) {
					char _dbg[192] = {};
					sprintf_s(_dbg, "[ChangeMember] MISS type=%d srvoff=%d mapSize=%zu\n",
						(int)_typeShort, header.serveroffset, curMap.size());
					OutputDebugStringA(_dbg); printf("%s", _dbg); fflush(stdout);
				}
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
		if (BulletIndex >= 0) {
			BulletRay& bray = bulletRays[BulletIndex];
			bray = BulletRay(header.raystart, header.rayDir, header.distance);
		}
		SpawnElectricTracer(header.raystart, header.rayDir, header.distance);
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STC_Protocol::AllocPlayerIndexes:
	{
        //OutputDebugStringA("[ClientReceiving] AllocPlayerIndexes\n");
		STC_AllocPlayerIndexes_Header& header = *(STC_AllocPlayerIndexes_Header*)currentPivot;

		game.clientIndexInServer = header.clientindex;
		game.playerGameObjectIndex = header.server_obj_index;

		if (header.server_obj_index >= 0 && game.DynmaicGameObjects.size() > header.server_obj_index) {
			player = (Player*)DynmaicGameObjects[playerGameObjectIndex];
			//player->Gun = game.GunMesh;
			//
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
		if (game.player != nullptr && game.isMapInit) {
			game.isPrepared = true;
		}

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
					SpawnMuzzleFlash(pTarget);
				}
			}
		}
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STC_Protocol::SkillCast:
	{
		STC_SkillCast_Header& header = *(STC_SkillCast_Header*)currentPivot;
		SpawnSkillEffect(header.effectType, header.position, header.direction, (UINT)header.ownerObjIndex, header.radius, header.power, header.duration);
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STC_Protocol::StatusEffect:
	{
		STC_StatusEffect_Header& header = *(STC_StatusEffect_Header*)currentPivot;
		SpawnStatusEffect(header.statusType, header.targetObjIndex, header.sourceObjIndex, header.active, header.position, header.extents, header.duration, header.remainTime, header.power);
		currentPivot += header.size;
		offset += header.size;
	}
	break;	case STC_Protocol::SyncGameState:
	{
        //OutputDebugStringA("[ClientReceiving] SyncGameState\n");
		STC_SyncGameState_Header& header = *(STC_SyncGameState_Header*)currentPivot;
		game.DynmaicGameObjects.reserve(header.DynamicGameObjectCapacity);
		game.DynmaicGameObjects.resize(header.DynamicGameObjectCapacity);
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STC_Protocol::SyncPlayerMoveZone:
	{
        //OutputDebugStringA("[ClientReceiving] SyncPlayerMoveZone\n");
		STC_PlayerMoveZone_Header& header = *(STC_PlayerMoveZone_Header*)currentPivot;

		game.isPrepared = false;
		game.isPreparedClientIndex = false;
		game.isMapInit = false;
		game.MoveZone(header.zoneId);
		currentPivot += header.size;
		offset += header.size;
	}
		break;
	case STC_Protocol::ServerTransfer:
	{
       // OutputDebugStringA("[ClientReceiving] ServerTransfer\n");
		STC_ServerTransfer_Header& header = *(STC_ServerTransfer_Header*)currentPivot;
		//{
		//	char _dbg[256] = {};
		//	sprintf_s(_dbg, "[ClientReceiving] ServerTransfer size=%u ip=\"%s\" port=%u dst=%d token=%d\n",
		//		header.size, header.ip, (unsigned)header.port, header.dstZoneId, header.transferToken);
		//	OutputDebugStringA(_dbg); printf("%s", _dbg); fflush(stdout);
		//}
		game.BeginServerTransfer(header.ip, header.port, header.dstZoneId, header.transferToken);
		return totallen;
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
	constexpr float Default_LineHeight = 750*2;
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
			for (int i = 0;i < gd.addSDFTextureStack.size();++i) {
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

		//float tConst[14] = { textRt.x, textRt.y, textRt.z, textRt.w, gd.ClientFrameWidth , gd.ClientFrameHeight, depth, 0, color.x, color.y, color.z, color.w, minD[i], maxD[i] };

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
					}
				}
				else {
					if (pslot->objid == game.CurrentGrabSlotData.objid && game.evt.uMsg == WM_LBUTTONDOWN) {
						pslot->itemCount += game.CurrentGrabSlotData.itemCnt;
						DXSlotParam* srcslot = (DXSlotParam*)game.CurrentGrabSlotData.selectedSlot->pParamterData;
						srcslot->itemCount -= game.CurrentGrabSlotData.itemCnt;

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
						pslot->itemCount += 1;
						DXSlotParam* srcslot = (DXSlotParam*)game.CurrentGrabSlotData.selectedSlot->pParamterData;

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
							int itemId = pslot->objid;
							int itemCnt = pslot->itemCount;
							pslot->itemCount = game.CurrentGrabSlotData.itemCnt;
							pslot->objid = game.CurrentGrabSlotData.objid;

							DXSlotParam* srcslot = (DXSlotParam*)game.CurrentGrabSlotData.selectedSlot->pParamterData;
							srcslot->itemCount = itemCnt;
							srcslot->objid = itemId;
							game.CurrentGrabSlotData.itemCnt = itemCnt;
							game.CurrentGrabSlotData.objid = itemId;

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
		const wchar_t ItemTypes[4][32] = { L"???", L"???", L"???", L"??u" };
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

	if (false)
	{
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

		rateloc = vec4(0.75, 0.8, 0.95, 0.9);
		WindowNormalizeCoordToDirectXRenderCoord_vec4(rateloc, ScreenW, ScreenH);
		DXUI* CloseBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		pbtn = (DXBtnParam*)CloseBtn->pParamterData;
		pbtn->flow = 0;
		pbtn->maxtime = 1;
		wcscpy_s(pbtn->text, 64, L"???");
		ZeroMemory(pbtn->addtionalParams, sizeof(float) * 16);
		pbtn->Base_UITextureIndex = 0;
		CloseBtn->RenderFunc = UIRenderDefaultBtn;
		CloseBtn->UpdateFunc = UIUpdateDefaultBtn;
		CloseBtn->EventFunc = UIEventCloseBtn;
		sample_page->uiArr.push_back(CloseBtn);

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

		vec4 rateloc = vec4(0.01f, 0.01f, 0.01f, 0.01f);
		WindowNormalizeCoordToDirectXRenderCoord_vec4(rateloc, ScreenW, ScreenH);
		rateloc.z += 134;
		rateloc.y -= 54;
		DXUI* InventoryWindowOpenBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn_openInven = (DXBtnParam*)InventoryWindowOpenBtn->pParamterData;
		pbtn_openInven->Set(0, 1, 6, L"인벤토리");
		InventoryWindowOpenBtn->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, UIEventOpen_Inventory);
		sample_page->uiArr.push_back(InventoryWindowOpenBtn);

		rateloc.x += 140;
		rateloc.z += 140;
		DXUI* EquipWindowOpenBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn_openequip = (DXBtnParam*)EquipWindowOpenBtn->pParamterData;
		pbtn_openequip->Set(0, 1, 6, L"???a");
		EquipWindowOpenBtn->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, UIEventOpen_Inventory);
		sample_page->uiArr.push_back(EquipWindowOpenBtn);

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
		pbtn_openInven->addtionalPtr[0] = InventoryWindow;

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

	for (int i = 0; i < uiArr.size(); ++i) {
		if (game.RectContainPos(uiArr[i]->location, pos)) {
			return uiArr[i];
		}
	}

	return nullptr;
}
#pragma endregion

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
}

namespace
{
	enum class ParticleSpriteSlot
	{
		FirePrimary = 0,
		FireSecondary,
		FireFlipbook,
		Electric,
	};

	GPUResource gParticleSecondaryTexture;
	GPUResource gParticleFlipbookTexture;
	GPUResource gParticleElectricTexture;

	constexpr UINT ELECTRIC_COUNT = 180;
	constexpr UINT ELECTRIC_BURST_COUNT = 96;
	constexpr UINT EMBER_SHOWER_COUNT = 220;
	constexpr UINT MUZZLE_FLASH_COUNT = 96;
	constexpr UINT BULLET_TRACER_COUNT = 256;
	constexpr UINT PARTICLE_FLAG_COLLIDE_GROUND = 1u;
	ParticlePool gElectricPool;
	ParticlePool gElectricBurstPool;
	ParticlePool gEmberShowerPool;
	ParticlePool gMuzzleFlashPool;
	ParticlePool gBulletTracerPool;
	ParticleCompute* gElectricCS = nullptr;
	ParticleCompute* gElectricBurstCS = nullptr;
	ParticleCompute* gEmberShowerCS = nullptr;
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
	ParticleSpriteSlot gMuzzleFlashSpriteSlot = ParticleSpriteSlot::FirePrimary;
	ParticleSpriteSlot gBulletTracerSpriteSlot = ParticleSpriteSlot::Electric;
	UINT gTransientParticleSeed = 1u;

	struct ParticleEffectRuntime
	{
		ParticlePool* Pool;
		ParticleCompute** Compute;
		ParticleSpriteSlot* SpriteSlot;
		const char* EntryPoint;
		UINT Count;
	};

	GPUResource* GetParticleSpriteResource(ParticleSpriteSlot slot)
	{
		switch (slot) {
		case ParticleSpriteSlot::FireSecondary:
			return &gParticleSecondaryTexture;
		case ParticleSpriteSlot::FireFlipbook:
			return &gParticleFlipbookTexture;
		case ParticleSpriteSlot::Electric:
			return &gParticleElectricTexture;
		case ParticleSpriteSlot::FirePrimary:
		default:
			return &game.FireTextureRes;
		}
	}

	auto GetParticleEffectRuntimes() -> std::vector<ParticleEffectRuntime>
	{
		std::vector<ParticleEffectRuntime> effects;
		effects.reserve(4);
		effects.push_back(ParticleEffectRuntime{ &game.FirePool, &game.FireCS, &gFireSpriteSlot, "FireCS", Game::FIRE_COUNT });
		effects.push_back(ParticleEffectRuntime{ &game.FirePillarPool, &game.FirePillarCS, &gFirePillarSpriteSlot, "FirePillarCS", Game::FIRE_PILLAR_COUNT });
		effects.push_back(ParticleEffectRuntime{ &game.FireRingPool, &game.FireRingCS, &gFireRingSpriteSlot, "FireRingCS", Game::FIRE_RING_COUNT });
		effects.push_back(ParticleEffectRuntime{ &gElectricPool, &gElectricCS, &gElectricSpriteSlot, "ElectricArcCS", ELECTRIC_COUNT });
		return effects;
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
		}
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
	}
	
	// 클라이언트에만 사용되는 아주 기본적인 에셋들을 가져온다.
	{
		DefaultTex.CreateTexture_fromFile(L"Resources/DefaultTexture.png", game.basicTexFormat, game.basicTexMip);
		DefaultNoramlTex.CreateTexture_fromFile(L"Resources/GlobalTexture/DefaultNormalTexture.png", basicTexFormat, basicTexMip);
		DefaultAmbientTex.CreateTexture_fromFile(L"Resources/GlobalTexture/DefaultAmbientTexture.png", basicTexFormat, basicTexMip);
		// particle texture
		FireTextureRes.CreateTexture_fromFile(L"Resources/fire.jpg", game.basicTexFormat, game.basicTexMip /*DXGI_FORMAT_UNKNOWN, 1*/, true);
		gParticleSecondaryTexture.CreateTexture_fromFile(L"Resources/fire2.jpg", game.basicTexFormat, game.basicTexMip, true);
		gParticleFlipbookTexture.CreateTexture_fromFile(L"Resources/fire.dds", game.basicTexFormat, game.basicTexMip, true);
		gParticleElectricTexture.CreateTexture_fromFile(L"Resources/elect.jpg", game.basicTexFormat, game.basicTexMip, true);

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

		MyDirLight.ShadowMap = gd.CreateShadowMap(4096, 4096, gd.GetDirLightCascadingShadowDSVIndex(0), MyDirLight);
		MyDirLight.View.mat = XMMatrixLookAtLH(vec4(0, 2, 5, 0), vec4(0, 0, 0, 0), vec4(0, 1, 0, 0));

		// particle init
		{
			const auto effects = GetParticleEffectRuntimes();
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
	
	// 어떤 존에서나 공동으로 사용가능한 글로벌 에셋들을 가져온다.
	
	// isAssetAddingInGlobal = true 면 이제 에셋이 공용으로 추가되기 시작한다. 그 신호를 준다.
	isAssetAddingInGlobal = true;
	{
		BumpMesh* ItemMesh = new BumpMesh();
		ItemMesh->ReadMeshFromFile_OBJ("Resources/Mesh/BulletMag001.obj", vec4(1, 1, 1, 1), true);
		ItemTable.push_back(Item(0, vec4(0, 0, 0, 0), nullptr, nullptr, L"")); // blank space in inventory.
		ItemTable.push_back(Item(1, vec4(1, 0, 0, 1), ItemMesh, &DefaultTex, L"[빨간 탄알집]"));
		ItemTable.push_back(Item(2, vec4(0, 1, 0, 1), ItemMesh, &DefaultTex, L"[녹색 탄알집]"));
		ItemTable.push_back(Item(3, vec4(0, 0, 1, 1), ItemMesh, &DefaultTex, L"[하얀 탄알집]")); // test items. red, green, blue bullet mags.

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

		constexpr bool kLoadSniperModel = true;
		constexpr bool kLoadRifleModel = true;
		constexpr bool kLoadPistolModel = true;
		constexpr bool kLoadShotGunModel = true;
		constexpr bool kLoadMachineGunModel = true;

		// 스나이퍼 모델 로드
		game.SniperModel = nullptr;
		if (kLoadSniperModel) {
			game.SniperModel = new Model;
			game.SniperModel->LoadModelFile2("Resources/Model/sniper.model");
		}

		// 라이플 모델 로드
		game.RifleModel = nullptr;
		if (kLoadRifleModel) {
			game.RifleModel = new Model;
			game.RifleModel->LoadModelFile2("Resources/Model/Rifle.model");
		}

		// 권총 모델 로드
		game.PistolModel = nullptr;
		if (kLoadPistolModel) {
			game.PistolModel = new Model;
			game.PistolModel->LoadModelFile2("Resources/Model/pistol.model");
		}

		/*game.Pistol_SlideIndices.clear();
		{
			int upperIdx = game.PistolModel->FindNodeIndexByName("Upper_Part");
			if (upperIdx >= 0) {
				game.Pistol_SlideIndices.push_back(upperIdx);
				game.PistolModel->BindPose[upperIdx] = game.PistolModel->Nodes[upperIdx].transform;
			}
		}*/

		// 샷건 모델 로드
		game.ShotGunModel = nullptr;
		if (kLoadShotGunModel) {
			game.ShotGunModel = new Model;
			game.ShotGunModel->LoadModelFile2("Resources/Model/shootgun.model");
		}
		//game.ShotGunModel->DebugPrintHierarchy(game.ShotGunModel->RootNode);

		/*game.SG_PumpIndices.clear();
		{
			int pumpIdx = game.ShotGunModel->FindNodeIndexByName("handguard_low");
			if (pumpIdx >= 0) {
				game.SG_PumpIndices.push_back(pumpIdx);
				game.ShotGunModel->BindPose[pumpIdx] = game.ShotGunModel->Nodes[pumpIdx].transform;
			}
		}*/

		//// 머신건(미니건) 모델 로드
		game.MachineGunModel = nullptr;
		if (kLoadMachineGunModel) {
			game.MachineGunModel = new Model;
			game.MachineGunModel->LoadModelFile2("Resources/Model/minigun.model");
		}

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

		game.GunModel = game.SniperModel;

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

	// 기존 LoadMap 코드. 이제 안쓰일듯?
	if(false)
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

	gd.gpucmd.Close();
	gd.gpucmd.Execute();
	gd.WaitGPUComplete();

	isGlobalAssetInit = true;
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
	for (int i = 0; i < Map->MapObjects.size(); ++i) {
		PushGameObject(Map->MapObjects[i]);
		game.StaticGameObjects.push_back(Map->MapObjects[i]);
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

	for (int i = 0;i < gd.addSDFTextureStack.size();++i) {
		gd.AddTextSDFTexture(gd.addSDFTextureStack[i]);
	}
	gd.addSDFTextureStack.clear();
	for (int i = gd.SDFTextureArr_immortalSize; i < gd.SDFTextureArr.size(); ++i) {
		SDFTextPageTextureBuffer* page = gd.SDFTextureArr[i];
		page->BakeSDF();
	}
	game.MyScreenCharactorShader->ClearSDFInstance();
	game.MyScreenCharactorShader->SDFInstance_StructuredBuffer.resource->Map(0, nullptr, (void**)&game.MyScreenCharactorShader->MappedSDFInstance);
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

	//gd.DeviceRemoveResonDebug();

	//6. 렌더패스 시작, 커맨드리스트 리셋
	HRESULT hResult = gd.gpucmd.Reset();

	//7. 쉐도우 맵의 STATE 를 PIXEL SHADER RESOURCE로 변환 (쉐도우 맵으로 쓰기 위해서)
	gd.gpucmd.ResBarrierTr(&game.MyDirLight.ShadowMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

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
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_ShadowMap, game.MyDirLight.descindex.hRender.hgpu);
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
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_SkinMeshShadowMaps, game.MyDirLight.descindex.hRender.hgpu);
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
	{
		const auto effects = GetParticleEffectRuntimes();
		for (const ParticleEffectRuntime& effect : effects) {
			if (*effect.Compute == nullptr) continue;
			(*effect.Compute)->Dispatch(gd.gpucmd, &effect.Pool->Buffer, effect.Pool->Count, DeltaTime);
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

	// 25. 플레이어의 일부가 가장 앞에 렌더링되어야 할때 렌더링 (ex> 총기)
	float hhpp = 0;
	float HeatGauge = 0;
	int kill = 0;
	int death = 0;
	int bulletCount = 0;
	float HealSkillCooldownFlow = 0;
	if (game.player != nullptr) {
		BindStaticPBRRenderState();
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
	game.RenderSDFText(L"이어가그요다", 6, rt, 30, vec4(1, 1, 1, 1), nullptr, nullptr, 0.001f);

	// 27. �׾Ƴ� Text ���� ����ϱ�
	MyScreenCharactorShader->RenderAllSDFTexts();

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
	commandList->SetComputeRootDescriptorTable(8, texarrSRV.hRender.hgpu); // Texture Arr

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
	game.TourID += 1;
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
						if (sgo == nullptr || sgo->TourID == game.TourID) continue;
						sgo->Render();
						sgo->TourID = game.TourID;
					}

					for (int i = 0;i < chck->Dynamic_gameobjects.size; ++i) {
						if (chck->Dynamic_gameobjects.isnull(i)) continue;
						DynamicGameObject* dgo = chck->Dynamic_gameobjects[i];
						if ((dgo == nullptr || dgo->tag[GameObjectTag::Tag_Enable] == false) || dgo->TourID == game.TourID) continue;
						dgo->Render();
						dgo->TourID = game.TourID;
					}

					/*for (int i = 0;i < chck->SkinMesh_gameobjects.size; ++i) {
						if (chck->SkinMesh_gameobjects.isnull(i)) continue;
						SkinMeshGameObject* dgo = chck->SkinMesh_gameobjects[i];
						if (dgo == nullptr || dgo->tag[GameObjectTag::Tag_Enable] == false) continue;
						dgo->Render();
					}*/
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
	const float SendPeriod = 0.05f;

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

			for (int i = 0; i < 36; ++i) {
				player->Inventory[i].id = 0;
				player->Inventory[i].ItemCount = 0;
			}
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
		game.player->Inventory[header.inventoryIndex].id = header.Iteminfo.id;
		game.player->Inventory[header.inventoryIndex].ItemCount = header.Iteminfo.ItemCount;
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

//this function must call after ScreenCharactorShader register in pipeline.
void Game::RenderSDFText(const wchar_t* wstr, int length, vec4 Rect, float fontsiz, vec4 color, float* minD, float* maxD, float depth)
{
	//fontsize = 100 -> 0.1x | 10 -> 0.01x
	vec4 pos = vec4(Rect.x, Rect.y, 0, 0);
	constexpr float Default_LineHeight = 750;
	float mul = fontsiz / Default_LineHeight;

	float basic_minD = -1;
	float basic_maxD = 0;
	bool isbasicDistance = false;
	if (minD == nullptr || maxD == nullptr) {
		isbasicDistance = true;
	}

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
		game.MyScreenCharactorShader->PushSDFInstance(sdfins);

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

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

#include <unordered_set>

extern int dbgc[128];
extern bool g_playGunSound;
void UIRenderDefaultEdit(DXUI* ui);
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
	constexpr int kShadowCascadeResolutions[3] = { 4096, 2048, 1024 };
	constexpr ui64 kShadowCascadeUpdateIntervals[4][3] = {
		{ 1, 2, 4 },
		{ 1, 3, 6 },
		{ 1, 4, 8 },
		{ 2, 4, 12 },
	};
	constexpr float kAutoLODMidSubMeshDistance = 35.0f;
	constexpr int kAutoLODMidSubMeshLimit = 6;
	constexpr float kAutoLODFarSubMeshDistance = 80.0f;
	constexpr int kAutoLODFarSubMeshLimit = 3;
	constexpr float kAutoLODVeryFarSubMeshDistance = 130.0f;
	constexpr int kAutoLODVeryFarSubMeshLimit = 2;
	constexpr float kAutoLODExtremeSubMeshDistance = 200.0f;
	constexpr int kAutoLODExtremeSubMeshLimit = 1;
	constexpr float kBossPrototypeNormalScale = 4.20f;
	constexpr float kBossPrototypeGroggyScale = 4.20f;
	constexpr float kBossPrototypeHeightOffset = -1.50f;
	constexpr float kBossPrototypeShieldVisualRadius = 5.25f;

	template <typename WaitFunc>
	void MeasureGPUWait(double& phaseWaitMs, WaitFunc&& waitFunc)
	{
		const ui64 start = GetTicks();
		waitFunc();
		const double waitMs = 1000.0 * double(GetTicks() - start) / double(QUERYPERFORMANCE_HZ);
		game.PerfGPUWaitMs += waitMs;
		phaseWaitMs += waitMs;
	}

	template <typename PresentFunc>
	void MeasurePresent(PresentFunc&& presentFunc)
	{
		const ui64 start = GetTicks();
		presentFunc();
		game.PerfPresentMs += 1000.0 * double(GetTicks() - start) / double(QUERYPERFORMANCE_HZ);
	}

	constexpr float BULLET_RAY_MISS_DISTANCE = 49.95f;
	constexpr bool SHOW_BULLET_RAY_DEBUG_MESH = false;

	// StaticGameObjects에 등록된 Mesh/Model을 Auto LOD 프리로드 대상으로 수집한다.
	void PrebuildStaticObjectAutoLOD()
	{
		AutoLOD_ResetPreloadQueue();

		auto queueObject = [](StaticGameObject* obj) {
			if (obj == nullptr) return;

			Mesh* mesh = nullptr;
			Model* model = nullptr;
			obj->shape.GetRealShape(mesh, model);

			if (mesh != nullptr) {
				AutoLOD_QueueMeshForPreload(mesh);
			}
			else if (model != nullptr) {
				AutoLOD_QueueModelForPreload(model);
			}
		};

		for (int i = 0; i < game.StaticGameObjects.size(); ++i) {
			queueObject(game.StaticGameObjects[i]);
		}

		if (game.Current_Zone != nullptr) {
			for (int zi = 0; zi < 9; ++zi) {
				Zone* zone = game.Current_Zone->nearZones[zi];
				if (zone == nullptr || !zone->isMapLoaded) continue;

				for (auto& chunkEntry : zone->chunck) {
					GameChunk* chunk = chunkEntry.second;
					if (chunk == nullptr) continue;

					for (int i = 0; i < chunk->Static_gameobjects.size; ++i) {
						if (chunk->Static_gameobjects.isnull(i)) continue;
						queueObject(chunk->Static_gameobjects[i]);
					}
				}
			}
		}

		AutoLOD_ProcessPreloadQueue();
	}

	int GetModelNodeIndex(Model* model, ModelNode* node)
	{
		if (model == nullptr || node == nullptr || model->Nodes == nullptr) return -1;
		return static_cast<int>(((char*)node - (char*)model->Nodes) / sizeof(ModelNode));
	}

	void DebugDumpModelNodes(Model* model, const char* label)
	{
		if (model == nullptr || model->Nodes == nullptr) return;

		char msg[1024] = {};
		sprintf_s(msg, "[ModelDump:%s] nodes=%d meshes=%u materials=%u\n",
			label != nullptr ? label : "model",
			model->nodeCount,
			model->mNumMeshes,
			model->mNumMaterials);
		OutputDebugStringA(msg);
		printf("%s", msg);

		for (int i = 0; i < model->nodeCount; ++i) {
			ModelNode& node = model->Nodes[i];
			const int parentIndex = GetModelNodeIndex(model, node.parent);
			sprintf_s(msg,
				"[ModelDump:%s] node=%03d parent=%03d name=\"%s\" child=%u meshSlots=%u pos=(%.3f,%.3f,%.3f)\n",
				label != nullptr ? label : "model",
				i,
				parentIndex,
				node.name.c_str(),
				node.numChildren,
				node.numMesh,
				node.transform.r[3].m128_f32[0],
				node.transform.r[3].m128_f32[1],
				node.transform.r[3].m128_f32[2]);
			OutputDebugStringA(msg);
			printf("%s", msg);

			for (unsigned int meshSlot = 0; meshSlot < node.numMesh; ++meshSlot) {
				const unsigned int meshIndex = node.Meshes != nullptr ? node.Meshes[meshSlot] : 0;
				Mesh* mesh = meshIndex < model->mNumMeshes ? model->mMeshes[meshIndex] : nullptr;
				const int subMeshCount = mesh != nullptr ? mesh->subMeshNum : 0;
				const int meshType = mesh != nullptr ? static_cast<int>(mesh->type) : -1;
				const int materialIndex = node.materialIndex != nullptr ? node.materialIndex[meshSlot] : -1;
				const char* materialName = "null";
				if (materialIndex >= 0 && materialIndex < static_cast<int>(game.MaterialTable.size()) && game.MaterialTable[materialIndex] != nullptr) {
					materialName = game.MaterialTable[materialIndex]->name;
				}

				sprintf_s(msg,
					"[ModelDump:%s]   meshSlot=%u mesh=%u type=%d subMeshes=%d material=%d \"%s\"\n",
					label != nullptr ? label : "model",
					meshSlot,
					meshIndex,
					meshType,
					subMeshCount,
					materialIndex,
					materialName);
				OutputDebugStringA(msg);
				printf("%s", msg);
			}
		}
		fflush(stdout);
	}

	void ScaleModelNodeVisual(Model* model, const char* nodeName, float scale)
	{
		if (model == nullptr || nodeName == nullptr) return;

		const int nodeIndex = model->FindNodeIndexByName(nodeName);
		if (nodeIndex < 0) return;

		ModelNode& node = model->Nodes[nodeIndex];
		node.transform.r[0] = XMVectorScale(node.transform.r[0], scale);
		node.transform.r[1] = XMVectorScale(node.transform.r[1], scale);
		node.transform.r[2] = XMVectorScale(node.transform.r[2], scale);
	}

	void FixTurretBossModelParts(Model* model)
	{
		if (model == nullptr) return;

		ScaleModelNodeVisual(model, "sci-fi turret cable", 0.01f);
		ScaleModelNodeVisual(model, "sci-fi turret gun", 0.01f);
		model->BakeAABB();
	}

	void ApplyModelNodeTransforms(GameObject* object, Model* model)
	{
		if (object == nullptr || model == nullptr || model->Nodes == nullptr || model->nodeCount <= 0) return;

		if (object->transforms_innerModel != nullptr) {
			delete[] object->transforms_innerModel;
			object->transforms_innerModel = nullptr;
		}

		object->transforms_innerModel = new matrix[model->nodeCount];
		for (int i = 0; i < model->nodeCount; ++i) {
			object->transforms_innerModel[i] = model->Nodes[i].transform;
		}
	}

	void EnsureBossObjectUsesBossModel(Monster* boss)
	{
		if (boss == nullptr || game.BossPrototypeBossModel == nullptr) return;
		if (game.BossPrototypeBossShapeIndex < 0 || game.BossPrototypeBossShapeIndex >= static_cast<int>(Shape::ShapeTable.size())) return;

		Model* currentModel = boss->shape.GetModel();
		const bool modelChanged = currentModel != game.BossPrototypeBossModel;
		bool needsShapeInit = modelChanged;
		if (boss->transforms_innerModel == nullptr) {
			needsShapeInit = true;
		}
		if (game.BossPrototypeBossModel->mNumSkinMesh > 0 && boss->BoneToWorldMatrixCB.empty()) {
			needsShapeInit = true;
		}
		if (gd.isSupportRaytracing && boss->RaytracingWorldMatInput_Model == nullptr) {
			needsShapeInit = true;
		}

		if (needsShapeInit) {
			if (modelChanged) {
				boss->SetRaytracingInstanceEnabled(false);
			}
			if (boss->transforms_innerModel != nullptr) {
				delete[] boss->transforms_innerModel;
				boss->transforms_innerModel = nullptr;
			}
			const int renderZoneId = boss->zoneId >= 0 ? boss->zoneId :
				(boss->zoneid >= 0 ? boss->zoneid : game.currentZoneId);
			boss->SetShape(Shape::ShapeTable[game.BossPrototypeBossShapeIndex], renderZoneId);
			boss->zoneId = renderZoneId;
			boss->zoneid = renderZoneId;

			char dbg[256] = {};
			sprintf_s(dbg, "[BossProto] boss model SetShape zone=%d modelChanged=%d skin=%d rt=%d\n",
				renderZoneId, modelChanged ? 1 : 0,
				game.BossPrototypeBossModel->mNumSkinMesh,
				boss->RaytracingWorldMatInput_Model != nullptr ? 1 : 0);
			OutputDebugStringA(dbg);
			printf("%s", dbg);
			fflush(stdout);
			return;
		}

		boss->shape = Shape::ShapeTable[game.BossPrototypeBossShapeIndex];
		ApplyModelNodeTransforms(boss, game.BossPrototypeBossModel);
	}

	void ReleaseBossPrototypeCoreObject(Game::BossPrototypeCore& core)
	{
		if (core.BossProtoTypeCoreObj == nullptr) return;
		core.BossProtoTypeCoreObj->Release();
		delete core.BossProtoTypeCoreObj;
		core.BossProtoTypeCoreObj = nullptr;
	}

	void ApplyBossWorldAndRefreshChunks(Monster* boss, const matrix& bossWorld)
	{
		if (boss == nullptr) return;

		boss->tag[GameObjectTag::Tag_Enable] = true;
		const int renderZoneId = boss->zoneId >= 0 ? boss->zoneId :
			(boss->zoneid >= 0 ? boss->zoneid : game.currentZoneId);
		Zone* renderZone = (renderZoneId >= 0 && renderZoneId < static_cast<int>(game.ZoneTable.size()))
			? game.ZoneTable[renderZoneId]
			: game.Current_Zone;

		if (renderZone == nullptr) {
			boss->worldMat = bossWorld;
			return;
		}

		boss->zoneId = renderZone->zoneid;
		boss->zoneid = renderZone->zoneid;

		if (boss->chunkAllocIndexs == nullptr) {
			boss->worldMat = bossWorld;
			renderZone->PushGameObject(boss);
			return;
		}

		matrix previousWorld = boss->worldMat;
		GameObjectIncludeChunks beforeChunks = boss->IncludeChunks;
		boss->worldMat = bossWorld;
		GameObjectIncludeChunks afterChunks = renderZone->GetChunks_Include_OBB(boss->GetOBB());
		if (beforeChunks != afterChunks && renderZone == game.Current_Zone) {
			boss->worldMat = previousWorld;
			boss->MoveChunck(bossWorld, beforeChunks, afterChunks);
		}
		else {
			boss->worldMat = bossWorld;
		}
	}

	vec4 NormalizeOrFallback(vec4 v, vec4 fallback);

	vec4 GetBossPrototypeVisualLookDirection(vec4 desiredDirection)
	{
		desiredDirection.y = 0.0f;
		desiredDirection = NormalizeOrFallback(desiredDirection, vec4(0, 0, 1, 0));
		return NormalizeOrFallback(vec4(desiredDirection.z, 0.0f, -desiredDirection.x, 0.0f), vec4(0, 0, 1, 0));
	}

	float Smooth01(float value)
	{
		value = min(1.0f, max(0.0f, value));
		return value * value * (3.0f - 2.0f * value);
	}

	float ApproachFloat(float current, float target, float rate, float dt)
	{
		float blend = 1.0f - expf(-max(rate, 0.01f) * max(dt, 0.0f));
		return current + (target - current) * blend;
	}

	vec4 ApproachDirection(vec4 current, vec4 target, float rate, float dt)
	{
		current.y = 0.0f;
		target.y = 0.0f;
		current = NormalizeOrFallback(current, target);
		target = NormalizeOrFallback(target, current);
		float blend = 1.0f - expf(-max(rate, 0.01f) * max(dt, 0.0f));
		return NormalizeOrFallback(current + (target - current) * blend, target);
	}

	bool BossPrototypeShouldAimBody(Game::BossPrototypePhase phase)
	{
		return phase == Game::BossPrototypePhase::MissileLock ||
			phase == Game::BossPrototypePhase::Bombardment ||
			phase == Game::BossPrototypePhase::RailgunCharge ||
			phase == Game::BossPrototypePhase::RotatingLaser;
	}

	vec4 GetBossPrototypePatternDirection(Game::BossPrototypePhase phase, vec4 defaultDirection)
	{
		if (phase == Game::BossPrototypePhase::RailgunCharge ||
			phase == Game::BossPrototypePhase::RotatingLaser) {
			return game.BossPrototypeRailgunDirection;
		}
		if (phase == Game::BossPrototypePhase::MissileLock ||
			phase == Game::BossPrototypePhase::Bombardment) {
			return game.BossPrototypeAimDirection;
		}
		return defaultDirection;
	}

	void ApplyBossPrototypeTurretPose(Monster* boss)
	{
		if (boss == nullptr || game.BossPrototypeBossModel == nullptr) return;
		if (boss->transforms_innerModel == nullptr) {
			ApplyModelNodeTransforms(boss, game.BossPrototypeBossModel);
		}
		if (boss->transforms_innerModel == nullptr) return;

		for (int i = 0; i < game.BossPrototypeBossModel->nodeCount; ++i) {
			boss->transforms_innerModel[i] = game.BossPrototypeBossModel->Nodes[i].transform;
		}

		if (game.BossPrototypeBossHeadNodeIndex < 0 ||
			game.BossPrototypeBossHeadNodeIndex >= game.BossPrototypeBossModel->nodeCount) {
			return;
		}

		matrix head = game.BossPrototypeBossModel->Nodes[game.BossPrototypeBossHeadNodeIndex].transform;
		vec4 localPos = head.pos;

		float targetPitch = game.BossPrototypeHeadPitch;
		float targetDrop = game.BossPrototypeHeadDrop;
		float targetRecoil = game.BossPrototypeHeadRecoil;
		float yaw = 0.0f;
		const float phaseTime = game.BossPrototypePhaseTime;
		const float dt = min(game.DeltaTime, 1.0f / 20.0f);

		if (game.BossPrototypeGroggyTime > 0.0f) {
			float slump = 1.0f - expf(-game.BossPrototypeGroggyTime * 2.0f);
			targetPitch = XMConvertToRadians(-28.0f) * slump;
			targetDrop = -0.22f * slump;
			targetRecoil = 0.0f;
		}
		else if (game.BossPrototypePhaseState == Game::BossPrototypePhase::MissileLock ||
			game.BossPrototypePhaseState == Game::BossPrototypePhase::Bombardment) {
			float missilePoseTime = phaseTime;
			if (game.BossPrototypePhaseState == Game::BossPrototypePhase::Bombardment) {
				missilePoseTime = fmodf(max(phaseTime, 0.0f), 2.2f);
			}
			float lift = Smooth01(missilePoseTime / 0.85f);
			float settle = 1.0f - Smooth01((missilePoseTime - 1.35f) / 0.75f);
			float amount = max(0.16f, lift * max(settle, 0.0f));
			targetPitch = XMConvertToRadians(45.0f) * amount;
			targetDrop = 0.0f;
			targetRecoil = -0.07f * amount;
		}
		else if (game.BossPrototypePhaseState == Game::BossPrototypePhase::RailgunCharge ||
			game.BossPrototypePhaseState == Game::BossPrototypePhase::RotatingLaser) {
			float brace = Smooth01(phaseTime / 0.45f);
			targetPitch = XMConvertToRadians(3.0f) * brace;
			targetDrop = 0.0f;
			targetRecoil = -0.035f * brace;
		}

		game.BossPrototypeHeadPitch = ApproachFloat(game.BossPrototypeHeadPitch, targetPitch, 5.2f, dt);
		game.BossPrototypeHeadDrop = ApproachFloat(game.BossPrototypeHeadDrop, targetDrop, 5.0f, dt);
		game.BossPrototypeHeadRecoil = ApproachFloat(game.BossPrototypeHeadRecoil, targetRecoil, 7.0f, dt);

		vec4 pitchAxis = NormalizeOrFallback(head.look, vec4(0, 0, 1, 0));
		XMMATRIX rot = XMMatrixRotationAxis(pitchAxis, game.BossPrototypeHeadPitch);
		head.right = XMVector3TransformNormal(head.right, rot);
		head.up = XMVector3TransformNormal(head.up, rot);
		head.look = XMVector3TransformNormal(head.look, rot);
		head.pos = localPos + vec4(0.0f, game.BossPrototypeHeadDrop, game.BossPrototypeHeadRecoil, 0.0f);
		boss->transforms_innerModel[game.BossPrototypeBossHeadNodeIndex] = head;
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
	PeacefulNPC::STATICINIT();

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
	// Weapon is polymorphic. Never memcpy the server's vtable pointer into the client object.
	LinkOffsetAsFunction(GameObjectType::_Player, "weapon", Player::SyncWeapons);
	LinkOffsetAsFunction(GameObjectType::_Monster, "worldMat", DynamicGameObject::SyncDestWolrd);
	LinkOffsetAsFunction(GameObjectType::_Monster, "HP", Monster::SyncHP);
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

void Game::RemoveMesh(Mesh* mesh)
{
	if (mesh == nullptr) return;
	for (Mesh*& registeredMesh : MeshTable) {
		if (registeredMesh == mesh) registeredMesh = nullptr;
	}
}

void Game::GameTableArrangeMent()
{

}

void Zone::GetZoneChunkGOIC()
{
	BoundingOrientedBox MapObb;
	vec4 minpos = vec4(BasicAABB_onlyXZ.x, 0, BasicAABB_onlyXZ.y);
	vec4 maxpos = vec4(BasicAABB_onlyXZ.z, 0, BasicAABB_onlyXZ.w);
	vec4 centerpos = minpos + maxpos;
	centerpos *= 0.5f;
	vec4 Center = Map->AABB[0] + Map->AABB[1];
	Center *= 0.5f;
	Center.x = centerpos.x;
	Center.z = centerpos.z;
	MapObb.Center = Center.f3;
	vec4 Ext = vec4(ZoneHalfWidth, Map->AABB[1].y - Center.y, ZoneHalfWidth);
	MapObb.Extents = Ext.f3;
	MapObb.Orientation = vec4(0, 0, 0, 1);
	ZoneChunk_goic = GetChunks_Include_OBB(MapObb);
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
	if (go == nullptr) return;
	vec4 pos = go->worldMat.pos;
	/*if (pos.y < -100.0f || pos.y > 500.0f ||
		pos.x < -1000.0f || pos.x > 1000.0f ||
		pos.z < -1000.0f || pos.z > 1000.0f) {
		return;
	}*/

	if (GameObject::IsType<Portal>(go)) {
		// static game object
		StaticGameObject* sgo = (StaticGameObject*)go;
		sgo->zoneid = zoneid;
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
		sgo->zoneid = zoneid;
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
		if (go->tag.Test(GameObjectTag::Tag_SkinMeshObject)) {
			SkinMeshGameObject* smgo = (SkinMeshGameObject*)go;
			smgo->zoneid = zoneid;
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
			dgo->zoneid = zoneid;
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

void Zone::ZoneAssetRelease()
{
	isMapLoaded = false;
	bReqireBakeLight_Raster = false;
	bReqireBakeLight_Raytracing = false;

	RayTracingMesh::ReleaseZone_FromAssetOffset(Asset_OffsetMul);

	// Static GameObject Release
	ZeroMemory(&game.StaticGameObjects[MaxStaticObjectCount * Asset_OffsetMul], sizeof(StaticGameObject*) * MaxStaticObjectCount);

	if (Map != nullptr) {
		Map->Release();
		delete Map;
		Map = nullptr;
	}
	for (auto f : chunck) {
		GameChunk* gc = f.second;
		gc->Release_Asset();
	}

	// Shader Visible Desc Heap 초기화
	ZeroMemory(&game.RenderMaterialTable[Zone::MAXZoneMaterialCount * (1 + Asset_OffsetMul)], sizeof(Material*) * Zone::MAXZoneMaterialCount);
	game.RenderMaterialTableSizePerZone[1 + Asset_OffsetMul] = 0;
	ZeroMemory(&game.RenderTextureTable[Zone::MAXZoneTextureCount * (1 + Asset_OffsetMul)], sizeof(GPUResource*) * Zone::MAXZoneTextureCount);
	game.RenderTextureTableSizePerZone[1 + Asset_OffsetMul] = 0;
	gd.ShaderVisibleDescPool.Reset_Zone_FromAssetOffset(Asset_OffsetMul);

	for (int i = 0; i < TextureTable.size(); ++i) {
		TextureTable[i]->Release();
		delete TextureTable[i];
		TextureTable[i] = nullptr;
	}
	TextureTable.clear();

	for (int i = 0; i < MaterialTable.size(); ++i) {
		MaterialTable[i]->Release();
		delete MaterialTable[i];
		MaterialTable[i] = nullptr;
	}
	MaterialTable.clear();

	for (int i = 0; i < LightTable.size(); ++i) {
		delete LightTable[i];
		LightTable[i] = nullptr;
	}
	LightTable.clear();

	ZoneLightChuncks.Release();
	ZoneLightChuncks_Mapped = nullptr;
	//Immortal_ZoneLightBuffer_SRV 는 어짜피 참조만 해서 상관이 없음.
}

void Zone::LightBake() {
	if (ZoneLightChuncks.resource) ZoneLightChuncks.Release();
	if (ZoneLightChuncks_Mapped) ZoneLightChuncks_Mapped = nullptr;
	if (Map == nullptr) return;

	vec4 Counts = (Map->AABB[1] - Map->AABB[0]) / Zone::chunck_divide_Width;
	ChunckCountX = floor(Counts.x + 1);
	ChunckCountY = floor(Counts.y + 1);
	ChunckCountZ = floor(Counts.z + 1);
	const int64_t totalChunkCount64 = (int64_t)ChunckCountX * ChunckCountY * ChunckCountZ;
	if (ChunckCountX <= 0 || ChunckCountY <= 0 || ChunckCountZ <= 0 ||
		totalChunkCount64 <= 0 || totalChunkCount64 > 1024 * 1024) {
		OutputDebugStringA("[LightBake] rejected invalid chunk dimensions\n");
		return;
	}
	const int totalChunkCount = (int)totalChunkCount64;

	int ix = floor(Map->AABB[0].x / Zone::chunck_divide_Width);
	int iy = floor(Map->AABB[0].y / Zone::chunck_divide_Width);
	int iz = floor(Map->AABB[0].z / Zone::chunck_divide_Width);
	ChunkIndex startci = ChunkIndex(ix, iy, iz);

	UINT ncbElementBytes = ((sizeof(ChunckLightData) * totalChunkCount + 255) & ~255); // 256의 배수
	ZoneLightChuncks = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1);
	if (ZoneLightChuncks.resource == nullptr) return;
	ZoneLightChuncks.resource->Map(0, NULL, (void**)&ZoneLightChuncks_Mapped);
	if (ZoneLightChuncks_Mapped == nullptr) return;
	ZeroMemory(ZoneLightChuncks_Mapped, ncbElementBytes);
	for (auto f : chunck) {
		GameChunk* gc = f.second;
		if (gc == nullptr) continue;
		ChunkIndex ci = f.first;
		ChunkIndex rci;
		rci.x = ci.x - startci.x;
		rci.y = ci.y - startci.y;
		rci.z = ci.z - startci.z;
		int index = rci.z + rci.y * ChunckCountZ + rci.x * ChunckCountZ * ChunckCountY;
		// Reloaded zones retain their chunk containers. Dynamic objects may have created chunks just
		// outside the static map bounds, so both the negative and one-past-end cases must be rejected.
		if (index < 0 || index >= totalChunkCount) continue;
		ChunckLightData& LightChunck = ZoneLightChuncks_Mapped[index];
		int validLightCount = 0;
		const int lightCount = min((int)gc->Lights.size(), 32);
		for (int i = 0; i < lightCount; ++i) {
			if (gc->Lights[i] == nullptr) continue;
			LightChunck.lights[validLightCount++] = *gc->Lights[i];
		}
		LightChunck.lights[0].MaxLightCount = validLightCount;
	}
	ZoneLightChuncks.resource->Unmap(0, nullptr);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = totalChunkCount;
	srvDesc.Buffer.StructureByteStride = sizeof(ChunckLightData);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	gd.pDevice->CreateShaderResourceView(ZoneLightChuncks.resource, &srvDesc, Immortal_ZoneLightBuffer_SRV.hCreation.hcpu);
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
		Blood,
		Explosion,
		Energy,
		AirStrike,
		Charge,
		Beam,
		Shield,
		Yellow,
		Hack,
		Heal,
	};

	GPUResource gParticleSecondaryTexture;
	GPUResource gParticleFlipbookTexture;
	GPUResource gParticleElectricTexture;
	GPUResource gParticleIceTexture;
	GPUResource gParticleBloodTexture;
	GPUResource gLoadingTex;            // [loading] fullscreen loading image
	GPUResource gStartScreenTex;       // [loading] fullscreen start screen image (shown until a key is pressed)
	GPUResource gSelectJobBgTex;       // [jobselect] background ("SELECT YOUR JOB")
	GPUResource gConfirmTex;           // [jobselect] CONFIRM / CANCEL button bar (single image, two buttons)
	GPUResource gJobCardTex[9];        // [jobselect] 9 job cards, indexed by PlayerJob enum order
	GPUResource gParticleExplosionTexture;
	GPUResource gParticleEnergyTexture;
	GPUResource gParticleAirStrikeTexture;
	GPUResource gParticleSkillAirStrikeTexture;
	GPUResource gParticleChargeTexture;
	GPUResource gParticleBeamTexture;
	GPUResource gParticleBossBeamTexture;
	GPUResource gParticleShieldTexture;
	GPUResource gParticleYellowTexture;
	GPUResource gParticleHackTexture;
	GPUResource gParticleHealTexture;
	GPUResource gParticleSnowTexture;

	constexpr UINT ELECTRIC_COUNT = 120;
	constexpr UINT ELECTRIC_BURST_COUNT = 72;
	constexpr UINT EMBER_SHOWER_COUNT = 120;
	constexpr UINT FROST_CONE_COUNT = 120;
	constexpr UINT FROST_ICE_BLOCK_COUNT = 160;
	constexpr UINT FROST_BLIZZARD_COUNT = 540;
	constexpr UINT MUZZLE_FLASH_COUNT = 384;
	constexpr UINT BULLET_TRACER_COUNT = 256;
	constexpr UINT ICE_PROJECTILE_COUNT = 224;
	constexpr float MAX_PARTICLE_EFFECT_RADIUS = 36.0f;
	constexpr float MAX_PARTICLE_EFFECT_POWER = 60.0f;
	constexpr float MAX_PARTICLE_EFFECT_DURATION = 4.0f;
	constexpr UINT PARTICLE_FLAG_COLLIDE_GROUND = 1u;
	ParticlePool gElectricPool;
	ParticlePool gElectricBurstPool;
	ParticlePool gEmberShowerPool;
	ParticlePool gFrostConePool;
	ParticlePool gFrostIceBlockPool;
	ParticlePool gFrostBlizzardPool;
	ParticlePool gBloodHitPool;
	ParticlePool gExplosionBlastPool;
	ParticlePool gAegisShieldEnergyPool;
	ParticlePool gAegisShieldOrbitPool;
	ParticlePool gRifleGrenadeTrailPool;
	ParticlePool gRifleAirStrikeTrailPool;

	ParticlePool gRifleStimFieldPool;
	ParticlePool gBomberFireProjectilePool;
	ParticlePool gBomberHealProjectilePool;
	ParticlePool gBomberFireExplosionPool;
	ParticlePool gBomberHealExplosionPool;
	ParticlePool gBomberMeteorTrailPool;
	ParticlePool gBomberMeteorPool;
	ParticlePool gMuzzleFlashPool;
	ParticlePool gBulletTracerPool;
	ParticlePool gIceProjectilePool;
	ParticlePool gBossExplosionPool;
	ParticlePool gStatusIcePool;
	ParticlePool gStatusFirePool;
	ParticlePool gStatusYellowPool;
	ParticlePool gStatusHackPool;
	ParticlePool gStatusHealPool;
	ParticleCompute* gElectricCS = nullptr;
	ParticleCompute* gElectricBurstCS = nullptr;
	ParticleCompute* gEmberShowerCS = nullptr;
	ParticleCompute* gFrostConeCS = nullptr;
	ParticleCompute* gFrostIceBlockCS = nullptr;
	ParticleCompute* gFrostBlizzardCS = nullptr;
	ParticleCompute* gBloodHitCS = nullptr;
	ParticleCompute* gExplosionBlastCS = nullptr;
	ParticleCompute* gAegisShieldEnergyCS = nullptr;
	ParticleCompute* gAegisShieldOrbitCS = nullptr;
	ParticleCompute* gRifleGrenadeTrailCS = nullptr;
	ParticleCompute* gRifleAirStrikeTrailCS = nullptr;

	ParticleCompute* gRifleStimFieldCS = nullptr;
	ParticleCompute* gBomberFireProjectileCS = nullptr;
	ParticleCompute* gBomberHealProjectileCS = nullptr;
	ParticleCompute* gBomberFireExplosionCS = nullptr;
	ParticleCompute* gBomberHealExplosionCS = nullptr;
	ParticleCompute* gBomberMeteorTrailCS = nullptr;
	ParticleCompute* gBomberMeteorCS = nullptr;
	GPUResource gMuzzleFlashUpload;
	GPUResource gBulletTracerUpload;
	GPUResource gIceProjectileUpload;
	GPUResource gBossExplosionUpload;
	GPUResource gStatusIceUpload;
	GPUResource gStatusFireUpload;
	GPUResource gStatusYellowUpload;
	GPUResource gStatusHackUpload;
	GPUResource gStatusHealUpload;
	std::vector<Particle> gMuzzleFlashParticles;
	std::vector<Particle> gBulletTracerParticles;
	std::vector<Particle> gIceProjectileParticles;
	std::vector<Particle> gBossExplosionParticles;
	std::vector<Particle> gStatusIceParticles;
	std::vector<Particle> gStatusFireParticles;
	std::vector<Particle> gStatusYellowParticles;
	std::vector<Particle> gStatusHackParticles;
	std::vector<Particle> gStatusHealParticles;
	struct DelayedWeaponFireVisual {
		int ZoneId = 0;
		int ObjectIndex = -1;
		float Delay = 0.0f;
		bool LeftHand = false;
	};
	std::vector<DelayedWeaponFireVisual> gDelayedWeaponFireVisuals;

	// [party/dungeon] dungeon-entry portal rendered as swirling blue particles (replaces the cube mesh).
	ParticlePool gPortalPool;
	GPUResource gPortalUpload;
	std::vector<Particle> gPortalParticles;

	ParticleSpriteSlot gFireSpriteSlot = ParticleSpriteSlot::FirePrimary;
	ParticleSpriteSlot gFirePillarSpriteSlot = ParticleSpriteSlot::FirePrimary;
	ParticleSpriteSlot gFireRingSpriteSlot = ParticleSpriteSlot::FirePrimary;
	ParticleSpriteSlot gElectricSpriteSlot = ParticleSpriteSlot::Electric;
	ParticleSpriteSlot gElectricBurstSpriteSlot = ParticleSpriteSlot::Electric;
	ParticleSpriteSlot gEmberShowerSpriteSlot = ParticleSpriteSlot::FirePrimary;
	ParticleSpriteSlot gFrostConeSpriteSlot = ParticleSpriteSlot::Ice;
	ParticleSpriteSlot gFrostIceBlockSpriteSlot = ParticleSpriteSlot::Ice;
	ParticleSpriteSlot gFrostBlizzardSpriteSlot = ParticleSpriteSlot::Ice;
	ParticleSpriteSlot gBloodHitSpriteSlot = ParticleSpriteSlot::Blood;
	ParticleSpriteSlot gExplosionBlastSpriteSlot = ParticleSpriteSlot::Explosion;
	ParticleSpriteSlot gAegisShieldEnergySpriteSlot = ParticleSpriteSlot::Energy;
	ParticleSpriteSlot gAegisShieldOrbitSpriteSlot = ParticleSpriteSlot::Energy;
	ParticleSpriteSlot gRifleGrenadeTrailSpriteSlot = ParticleSpriteSlot::Explosion;
	ParticleSpriteSlot gRifleAirStrikeTrailSpriteSlot = ParticleSpriteSlot::Explosion;

	ParticleSpriteSlot gRifleStimFieldSpriteSlot = ParticleSpriteSlot::Electric;
	ParticleSpriteSlot gBomberFireProjectileSpriteSlot = ParticleSpriteSlot::FireSecondary;
	ParticleSpriteSlot gBomberHealProjectileSpriteSlot = ParticleSpriteSlot::Heal;
	ParticleSpriteSlot gBomberFireExplosionSpriteSlot = ParticleSpriteSlot::Explosion;
	ParticleSpriteSlot gBomberHealExplosionSpriteSlot = ParticleSpriteSlot::Heal;
	ParticleSpriteSlot gBomberMeteorTrailSpriteSlot = ParticleSpriteSlot::FireSecondary;
	ParticleSpriteSlot gBomberMeteorSpriteSlot = ParticleSpriteSlot::Explosion;
	ParticleSpriteSlot gMuzzleFlashSpriteSlot = ParticleSpriteSlot::FirePrimary;
	ParticleSpriteSlot gBulletTracerSpriteSlot = ParticleSpriteSlot::Electric;
	ParticleSpriteSlot gBossExplosionSpriteSlot = ParticleSpriteSlot::Explosion;
	UINT gTransientParticleSeed = 1u;

	constexpr UINT PARTICLE_EMITTER_FLAG_RESET = 1u;
	constexpr UINT BOSS_EXPLOSION_PARTICLE_COUNT = 384u;
	constexpr UINT STATUS_PARTICLE_COUNT = 192u;

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
		float VisualPulseTimer = 0.0f;
		vec4 Position = vec4(0, 0, 0, 1);
		vec4 Extents = vec4(0.3f, 1.0f, 0.3f, 0.0f);
	};

	std::vector<StatusEffectVisual> gStatusEffectVisuals;

	struct AegisShieldVisual
	{
		bool Active = false;
		UINT OwnerId = 0;
		vec4 Position = vec4(0, 0, 0, 1);
		float Radius = 1.5f;
		float Age = 0.0f;
		float Duration = 0.35f;
		bool GroundDisk = false;
	};

	std::vector<AegisShieldVisual> gAegisShieldVisuals;

	struct RailgunBeamVisual
	{
		bool Active = false;
		vec4 Origin = vec4(0, 0, 0, 1);
		vec4 Direction = vec4(0, 0, 1, 0);
		float Age = 0.0f;
		float Duration = 0.22f;
		float Length = 70.0f;
		float Width = 0.82f;
		vec4 Tint = vec4(0.66f, 0.96f, 1.45f, 0.16f);
		bool UseEnergyTexture = false;
	};

	std::vector<RailgunBeamVisual> gRailgunBeamVisuals;

	vec4 GetSupportDronePosition(const Player* player, bool leftDrone, float bobOffset = 0.0f)
	{
		if (player == nullptr) return vec4(0, 0, 0, 1);
		const float side = leftDrone ? -1.0f : 1.0f;
		vec4 position = player->worldMat.pos + player->worldMat.right * (1.35f * side) -
			player->worldMat.look * 0.55f + player->worldMat.up * (1.75f + bobOffset);
		position.w = 1.0f;
		return position;
	}

	void QueueSupportBeam(vec4 origin, vec4 target, float width, float duration, vec4 tint,
		bool useEnergyTexture = false)
	{
		vec4 direction = target - origin;
		float length = direction.len3;
		if (length <= 0.001f) return;
		direction.len3 = 1.0f;

		RailgunBeamVisual* slot = nullptr;
		for (RailgunBeamVisual& visual : gRailgunBeamVisuals) {
			if (!visual.Active) {
				slot = &visual;
				break;
			}
		}
		if (slot == nullptr) {
			gRailgunBeamVisuals.push_back(RailgunBeamVisual{});
			slot = &gRailgunBeamVisuals.back();
		}
		slot->Active = true;
		slot->Origin = origin;
		slot->Direction = direction;
		slot->Age = 0.0f;
		slot->Duration = max(duration, 0.04f);
		slot->Length = length;
		slot->Width = width;
		slot->Tint = tint;
		slot->UseEnergyTexture = useEnergyTexture;
	}

	struct RailgunOrbitVisual
	{
		bool Active = false;
		UINT OwnerId = 0;
		vec4 Position = vec4(0, 0, 0, 1);
		float Age = 0.0f;
		float Duration = 0.75f;
		float Radius = 0.72f;
		bool Yellow = false;
	};

	std::vector<RailgunOrbitVisual> gRailgunOrbitVisuals;

	struct DualBladeFloorVisual
	{
		bool Active = false;
		UINT OwnerId = 0;
		vec4 Position = vec4(0, 0, 0, 1);
		float Age = 0.0f;
		float Duration = 10.0f;
		float Radius = 1.15f;
	};

	std::vector<DualBladeFloorVisual> gDualBladeFloorVisuals;

	struct HackerEmpVisual
	{
		bool Active = false;
		bool GroundField = false;
		UINT OwnerId = 0;
		vec4 Position = vec4(0, 0, 0, 1);
		float Age = 0.0f;
		float Duration = 0.75f;
		float Radius = 12.0f;
	};

	std::vector<HackerEmpVisual> gHackerEmpVisuals;

	struct FrostSnowWaveVisual
	{
		bool Active = false;
		bool HealWave = false;
		vec4 Position = vec4(0, 0, 0, 1);
		float Age = 0.0f;
		float Duration = 1.05f;
		float Radius = 8.0f;
	};

	std::vector<FrostSnowWaveVisual> gFrostSnowWaveVisuals;

	struct SkillMissileVisual
	{
		bool Active = false;
		vec4 Start = vec4(0, 0, 0, 1);
		vec4 Target = vec4(0, 0, 0, 1);
		float Age = 0.0f;
		float Duration = 0.72f;
		float Radius = 1.0f;
	};

	std::vector<SkillMissileVisual> gSkillMissileVisuals;

	vec4 GetStatusEffectTint(StatusEffectType type)
	{
		switch (type) {
		case StatusEffectType::Freeze:
			return vec4(0.70f, 0.04f, 0.48f, 1.00f);
		case StatusEffectType::Slow:
			return vec4(0.58f, 0.16f, 0.68f, 1.00f);
		case StatusEffectType::Taunt:
			return vec4(0.68f, 1.00f, 0.70f, 0.00f);
		case StatusEffectType::Burn:
			return vec4(0.70f, 1.00f, 0.08f, 0.00f);
		case StatusEffectType::Stun:
			return vec4(0.72f, 1.00f, 0.88f, 0.05f);
		case StatusEffectType::Paralyze:
			return vec4(0.70f, 0.48f, 0.05f, 1.00f);
		case StatusEffectType::Hack:
			return vec4(0.82f, 0.08f, 1.00f, 0.55f);
		case StatusEffectType::Heal:
			return vec4(0.10f, 1.00f, 0.35f, 0.35f);
		case StatusEffectType::None:
		default:
			return vec4(0.0f, 1.0f, 1.0f, 1.0f);
		}
	}

	int GetStatusEffectTintPriority(StatusEffectType type)
	{
		switch (type) {
		case StatusEffectType::Freeze:
			return 60;
		case StatusEffectType::Stun:
			return 50;
		case StatusEffectType::Paralyze:
			return 45;
		case StatusEffectType::Hack:
			return 45;
		case StatusEffectType::Taunt:
			return 40;
		case StatusEffectType::Burn:
			return 30;
		case StatusEffectType::Heal:
			return 25;
		case StatusEffectType::Slow:
			return 20;
		case StatusEffectType::None:
		default:
			return 0;
		}
	}

	StatusEffectType GetActiveStatusEffectType(int targetObjIndex)
	{
		StatusEffectType selectedType = StatusEffectType::None;
		int selectedPriority = 0;
		for (const StatusEffectVisual& visual : gStatusEffectVisuals) {
			if (visual.Active == false || visual.TargetObjIndex != targetObjIndex) continue;

			int priority = GetStatusEffectTintPriority(visual.Type);
			if (priority > selectedPriority) {
				selectedType = visual.Type;
				selectedPriority = priority;
			}
		}

		return selectedType;
	}

	vec4 GetStatusEffectHudColor(StatusEffectType type)
	{
		switch (type) {
		case StatusEffectType::Freeze:
			return vec4(0.06f, 0.74f, 1.00f, 1.00f);
		case StatusEffectType::Slow:
			return vec4(0.34f, 0.88f, 1.00f, 1.00f);
		case StatusEffectType::Burn:
			return vec4(1.00f, 0.19f, 0.04f, 1.00f);
		case StatusEffectType::Taunt:
			return vec4(1.00f, 0.88f, 0.06f, 1.00f);
		case StatusEffectType::Stun:
			return vec4(1.00f, 0.68f, 0.02f, 1.00f);
		case StatusEffectType::Paralyze:
			return vec4(0.78f, 0.28f, 1.00f, 1.00f);
		case StatusEffectType::Hack:
			return vec4(0.95f, 0.18f, 1.00f, 1.00f);
		case StatusEffectType::Heal:
			return vec4(0.18f, 1.00f, 0.48f, 1.00f);
		case StatusEffectType::None:
		default:
			return vec4(0.94f, 0.06f, 0.10f, 1.00f);
		}
	}

	void RefreshMonsterStatusTint(int targetObjIndex)
	{
		if (targetObjIndex < 0 || targetObjIndex >= (int)game.DynmaicGameObjects.size()) return;

		Monster* monster = dynamic_cast<Monster*>(game.DynmaicGameObjects[targetObjIndex]);
		if (monster == nullptr) return;

		if (game.BossPrototypeEnabled && targetObjIndex == game.BossPrototypeIndex) {
			monster->SetStatusTint(vec4(0.0f, 1.0f, 1.0f, 1.0f));
			return;
		}
		monster->SetStatusTint(GetStatusEffectTint(GetActiveStatusEffectType(targetObjIndex)));
	}

	void SpawnBurningStatusEffect(vec4 center, vec4 extents);
	void SpawnIceStatusEffect(vec4 center, vec4 extents);
	void SpawnParalyzeStatusEffect(vec4 center, vec4 extents);
	void SpawnHackStatusEffect(vec4 center, vec4 extents);
	void SpawnHealStatusEffect(vec4 center, vec4 extents, int count = 7);

	void UpdateStatusEffectVisuals(float deltaTime)
	{
		const float dt = max(0.0f, deltaTime);

		for (StatusEffectVisual& visual : gStatusEffectVisuals) {
			if (visual.Active == false) continue;

			if (visual.RemainTime > 0.0f) {
				visual.RemainTime = max(0.0f, visual.RemainTime - dt);
				if (visual.RemainTime <= 0.0f) {
					visual.Active = false;
					RefreshMonsterStatusTint(visual.TargetObjIndex);
					continue;
				}
			}

			if (visual.TargetObjIndex >= 0 && visual.TargetObjIndex < (int)game.DynmaicGameObjects.size() &&
				game.DynmaicGameObjects[visual.TargetObjIndex] != nullptr) {
				visual.Position = game.DynmaicGameObjects[visual.TargetObjIndex]->worldMat.pos;
				visual.Position.y += max(0.6f, visual.Extents.y * 0.5f);
			}

			visual.VisualPulseTimer -= dt;
			if (visual.VisualPulseTimer > 0.0f) continue;

			switch (visual.Type) {
			case StatusEffectType::Freeze:
			case StatusEffectType::Slow:
				SpawnIceStatusEffect(visual.Position, visual.Extents);
				visual.VisualPulseTimer = 0.28f;
				break;
			case StatusEffectType::Burn:
				SpawnBurningStatusEffect(visual.Position, visual.Extents);
				visual.VisualPulseTimer = 0.16f;
				break;
			case StatusEffectType::Stun:
			case StatusEffectType::Paralyze:
				SpawnParalyzeStatusEffect(visual.Position, visual.Extents);
				visual.VisualPulseTimer = 0.20f;
				break;
			case StatusEffectType::Hack:
				SpawnHackStatusEffect(visual.Position, visual.Extents);
				visual.VisualPulseTimer = 0.24f;
				break;
			case StatusEffectType::Heal:
				SpawnHealStatusEffect(visual.Position, visual.Extents);
				visual.VisualPulseTimer = 0.26f;
				break;
			default:
				visual.VisualPulseTimer = 0.35f;
				break;
			}
		}
	}

	GPUResource* UseLoadedOrFallback(GPUResource& preferred, GPUResource& fallback)
	{
		return preferred.resource != nullptr ? &preferred : &fallback;
	}

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
		case ParticleSpriteSlot::Blood:
			return &gParticleBloodTexture;
		case ParticleSpriteSlot::Explosion:
			return &gParticleExplosionTexture;
		case ParticleSpriteSlot::Energy:
			return &gParticleEnergyTexture;
		case ParticleSpriteSlot::AirStrike:
			return UseLoadedOrFallback(gParticleSkillAirStrikeTexture, gParticleAirStrikeTexture);
		case ParticleSpriteSlot::Charge:
			return UseLoadedOrFallback(gParticleChargeTexture, gParticleElectricTexture);
		case ParticleSpriteSlot::Beam:
			return UseLoadedOrFallback(gParticleBeamTexture, gParticleElectricTexture);
		case ParticleSpriteSlot::Shield:
			return UseLoadedOrFallback(gParticleShieldTexture, gParticleEnergyTexture);
		case ParticleSpriteSlot::Yellow:
			return UseLoadedOrFallback(gParticleYellowTexture, gParticleElectricTexture);
		case ParticleSpriteSlot::Hack:
			return UseLoadedOrFallback(gParticleHackTexture, gParticleElectricTexture);
		case ParticleSpriteSlot::Heal:
			return UseLoadedOrFallback(gParticleHealTexture, gParticleElectricTexture);
		case ParticleSpriteSlot::FirePrimary:
		default:
			return &game.FireTextureRes;
		}
	}

	vec4 NormalizeOrFallback(vec4 v, vec4 fallback);

	ParticleEmitterCB MakeParticleEmitter(vec4 position, vec4 direction, float radius, float power, float duration, UINT ownerId)
	{
		direction = NormalizeOrFallback(direction, vec4(0, 0, 1, 0));
		radius = min(max(radius, 0.1f), MAX_PARTICLE_EFFECT_RADIUS);
		power = min(max(power, 0.1f), MAX_PARTICLE_EFFECT_POWER);
		duration = min(max(duration, 0.05f), MAX_PARTICLE_EFFECT_DURATION);

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

		effects.reserve(24);
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
		effects.push_back(ParticleEffectRuntime{ SkillEffectType::Blood_Hit, &gBloodHitPool, &gBloodHitCS, &gBloodHitSpriteSlot, "BloodHitCS", 96, idleEmitter, false, false });
		effects.push_back(ParticleEffectRuntime{ SkillEffectType::Explosion_Blast, &gExplosionBlastPool, &gExplosionBlastCS, &gExplosionBlastSpriteSlot, "ExplosionBlastCS", 112, idleEmitter, false, false });
		effects.push_back(ParticleEffectRuntime{ SkillEffectType::Aegis_ShieldEnergy, &gAegisShieldEnergyPool, &gAegisShieldEnergyCS, &gAegisShieldEnergySpriteSlot, "AegisShieldEnergyCS", 48, idleEmitter, false, false });
		effects.push_back(ParticleEffectRuntime{ SkillEffectType::Aegis_ShieldAura, &gAegisShieldOrbitPool, &gAegisShieldOrbitCS, &gAegisShieldOrbitSpriteSlot, "AegisShieldOrbitCS", 64, idleEmitter, false, false });
		effects.push_back(ParticleEffectRuntime{ SkillEffectType::Rifle_GrenadeTrail, &gRifleGrenadeTrailPool, &gRifleGrenadeTrailCS, &gRifleGrenadeTrailSpriteSlot, "RifleGrenadeTrailCS", 48, idleEmitter, false, false });
		effects.push_back(ParticleEffectRuntime{ SkillEffectType::Rifle_AirStrikeTrail, &gRifleAirStrikeTrailPool, &gRifleAirStrikeTrailCS, &gRifleAirStrikeTrailSpriteSlot, "RifleAirStrikeTrailCS", 80, idleEmitter, false, false });
		effects.push_back(ParticleEffectRuntime{ SkillEffectType::Rifle_StimField, &gRifleStimFieldPool, &gRifleStimFieldCS, &gRifleStimFieldSpriteSlot, "RifleStimFieldCS", 72, idleEmitter, false, false });
		effects.push_back(ParticleEffectRuntime{ SkillEffectType::Bomber_FireProjectile, &gBomberFireProjectilePool, &gBomberFireProjectileCS, &gBomberFireProjectileSpriteSlot, "BomberFireProjectileCS", 48, idleEmitter, false, false });
		effects.push_back(ParticleEffectRuntime{ SkillEffectType::Bomber_HealProjectile, &gBomberHealProjectilePool, &gBomberHealProjectileCS, &gBomberHealProjectileSpriteSlot, "BomberHealProjectileCS", 48, idleEmitter, false, false });
		effects.push_back(ParticleEffectRuntime{ SkillEffectType::Bomber_FireExplosion, &gBomberFireExplosionPool, &gBomberFireExplosionCS, &gBomberFireExplosionSpriteSlot, "ExplosionBlastCS", 128, idleEmitter, false, false });
		effects.push_back(ParticleEffectRuntime{ SkillEffectType::Bomber_HealExplosion, &gBomberHealExplosionPool, &gBomberHealExplosionCS, &gBomberHealExplosionSpriteSlot, "BomberHealExplosionCS", 72, idleEmitter, false, false });
		effects.push_back(ParticleEffectRuntime{ SkillEffectType::Bomber_MeteorTrail, &gBomberMeteorTrailPool, &gBomberMeteorTrailCS, &gBomberMeteorTrailSpriteSlot, "FirePillarCS", 144, idleEmitter, false, false });
		effects.push_back(ParticleEffectRuntime{ SkillEffectType::Bomber_Meteor, &gBomberMeteorPool, &gBomberMeteorCS, &gBomberMeteorSpriteSlot, "ExplosionBlastCS", 256, idleEmitter, false, false });
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

	GPUResource* GetBossBeamTextureResource()
	{
		return UseLoadedOrFallback(gParticleBossBeamTexture, gParticleBeamTexture);
	}

	GPUResource* GetPlayerBeamTextureResource()
	{
		return GetParticleSpriteResource(ParticleSpriteSlot::Beam);
	}

	GPUResource* GetBossShieldTextureResource()
	{
		return GetParticleSpriteResource(ParticleSpriteSlot::Shield);
	}

	GPUResource* GetBossAirStrikeTextureResource()
	{
		return UseLoadedOrFallback(gParticleAirStrikeTexture, gParticleExplosionTexture);
	}

	GPUResource* GetSkillAirStrikeTextureResource()
	{
		return GetParticleSpriteResource(ParticleSpriteSlot::AirStrike);
	}

	float GetBossVisualGroundY()
	{
		constexpr float BossVisualGroundLift = 1.3f;
		game.BossPrototypeVisualGroundY = game.BossPrototypeCenter.y + BossVisualGroundLift;
		game.BossPrototypeVisualGroundInitialized = true;
		return game.BossPrototypeVisualGroundY;
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

	void TryGetMuzzleBasis(Player* player, bool leftHand, vec4& origin, vec4& forward, vec4& right, vec4& up)
	{
		if (player == game.player && game.bFirstPersonVision) {
			matrix view = gd.viewportArr[0].ViewMatrix;
			XMMATRIX invViewXm = XMMatrixInverse(nullptr, (XMMATRIX)view);
			matrix invView = invViewXm;

			forward = NormalizeOrFallback(invView.look, vec4(0, 0, 1, 0));
			right = NormalizeOrFallback(invView.right, vec4(1, 0, 0, 0));
			up = NormalizeOrFallback(invView.up, vec4(0, 1, 0, 0));
			const WeaponType weaponType = (WeaponType)player->m_currentWeaponType;
			const bool dualPistol = weaponType == WeaponType::DualPistol;
			float sideOffset = dualPistol ? (leftHand ? -0.48f : 0.48f) : 0.15f;
			if (weaponType == WeaponType::Rifle && player->m_isZooming) {
				const float zoomAlpha = min(1.0f, max(0.0f,
					(60.0f - player->m_currentFov) / (60.0f - 40.0f)));
				sideOffset *= 1.0f - zoomAlpha;
			}
			origin = gd.viewportArr[0].Camera_Pos + forward * 0.72f + right * sideOffset - up * 0.10f;
			return;
		}

		if (leftHand && player->cachedThirdPersonLeftHandMatrixValid) {
			origin = player->cachedThirdPersonLeftHandMatrix.pos;
			origin.w = 1.0f;
			forward = NormalizeOrFallback(player->worldMat.look, vec4(0, 0, 1, 0));
			right = NormalizeOrFallback(player->worldMat.right, vec4(1, 0, 0, 0));
			up = NormalizeOrFallback(player->worldMat.up, vec4(0, 1, 0, 0));
		}

		else if (player->cachedThirdPersonRightHandMatrixValid) {
			const matrix& rightHandWorld = player->cachedThirdPersonRightHandMatrix;
			float muzzlePitch = 0.0f;
			if (player->m_currentUpperState == Player::UpperState::AIM ||
				player->m_currentUpperState == Player::UpperState::SHOOT) {
				const float maxMuzzlePitch = XMConvertToRadians(45.0f);
				const float muzzlePitchBias = XMConvertToRadians(-10.0f);
				muzzlePitch = max(-maxMuzzlePitch, min(maxMuzzlePitch,
					player->m_pitch + muzzlePitchBias));
			}
			vec4 aimForward = XMVector3TransformNormal(
				player->worldMat.look,
				XMMatrixRotationAxis(player->worldMat.right, muzzlePitch));
			aimForward = NormalizeOrFallback(aimForward, player->worldMat.look);
			vec4 handAxes[3] = {
				NormalizeOrFallback(rightHandWorld.right, player->worldMat.right),
				NormalizeOrFallback(rightHandWorld.up, player->worldMat.up),
				NormalizeOrFallback(rightHandWorld.look, player->worldMat.look)
			};
			auto dot3 = [](const vec4& a, const vec4& b) {
				return a.x * b.x + a.y * b.y + a.z * b.z;
			};

			if (player->cachedRightHandForwardAxisIndex < 0 ||
				player->cachedRightHandRightAxisIndex < 0) {
				int forwardAxisIndex = 0;
				float bestForwardDot = fabsf(dot3(handAxes[0], aimForward));
				for (int axisIndex = 1; axisIndex < 3; ++axisIndex) {
					float axisDot = fabsf(dot3(handAxes[axisIndex], aimForward));
					if (axisDot > bestForwardDot) {
						bestForwardDot = axisDot;
						forwardAxisIndex = axisIndex;
					}
				}

				int rightAxisIndex = (forwardAxisIndex == 0) ? 1 : 0;
				float bestRightDot = -1.0f;
				for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
					if (axisIndex == forwardAxisIndex) continue;
					float axisDot = fabsf(dot3(handAxes[axisIndex], player->worldMat.right));
					if (axisDot > bestRightDot) {
						bestRightDot = axisDot;
						rightAxisIndex = axisIndex;
					}
				}

				player->cachedRightHandForwardAxisIndex = forwardAxisIndex;
				player->cachedRightHandRightAxisIndex = rightAxisIndex;
				player->cachedRightHandForwardAxisSign =
					dot3(handAxes[forwardAxisIndex], aimForward) < 0.0f ? -1.0f : 1.0f;
				player->cachedRightHandRightAxisSign =
					dot3(handAxes[rightAxisIndex], player->worldMat.right) < 0.0f ? -1.0f : 1.0f;
			}

			forward = handAxes[player->cachedRightHandForwardAxisIndex] *
				player->cachedRightHandForwardAxisSign;
			right = handAxes[player->cachedRightHandRightAxisIndex] *
				player->cachedRightHandRightAxisSign;
			up = NormalizeOrFallback(XMVector3Cross(forward, right), player->worldMat.up);
			origin = rightHandWorld.pos;
			origin.w = 1.0f;
		}
		else {
			forward = NormalizeOrFallback(player->worldMat.look, vec4(0, 0, 1, 0));
			right = NormalizeOrFallback(player->worldMat.right, vec4(1, 0, 0, 0));
			up = NormalizeOrFallback(player->worldMat.up, vec4(0, 1, 0, 0));
			origin = player->worldMat.pos + right * 0.30f + up * 1.35f + forward * 0.48f;
		}

		vec4 muzzleOffset(0.0f, 0.0f, 0.0f, 0.0f);
		switch ((WeaponType)player->m_currentWeaponType) {
		case WeaponType::MachineGun: muzzleOffset = vec4(0.10f, -1.20f, 1.15f, 0.0f); break;
		case WeaponType::Shotgun:    muzzleOffset = vec4(-0.10f, -1.20f, 1.15f, 0.0f); break;
		case WeaponType::Pistol:     muzzleOffset = vec4(-0.10f, 0.00f, 0.70f, 0.0f); break;
		case WeaponType::DualPistol: muzzleOffset = vec4(-0.10f, 0.00f, 0.72f, 0.0f); break;
		case WeaponType::DronePistol: muzzleOffset = vec4(-0.10f, 0.00f, 0.70f, 0.0f); break;
		case WeaponType::SMG:        muzzleOffset = vec4(-0.10f, 0.00f, 0.70f, 0.0f); break;
		case WeaponType::GrenadeGun: muzzleOffset = vec4(-0.10f, 0.00f, 0.70f, 0.0f); break;
		case WeaponType::Rifle:      muzzleOffset = vec4(0.15f, -0.60f, 0.85f, 0.0f); break;
		case WeaponType::Sniper:     muzzleOffset = vec4(-0.10f, -1.20f, 1.15f, 0.0f); break;
		default: break;
		}
		origin += right * muzzleOffset.x;
		origin += up * muzzleOffset.y;
		origin += forward * muzzleOffset.z;
	}

	void SpawnPortalParticles(const vec4& center)
	{
		const int count = 24;   // denser
		for (int i = 0; i < count; ++i) {
			Particle* particle = AllocateTransientParticle(gPortalParticles);
			if (particle == nullptr) break;
			float angle = RandomRange(0.0f, XM_2PI);
			float rad = RandomRange(1.6f, 2.2f);
			float px = RandomRange(-0.3f, 0.3f);          // thin in X (disc faces X)
			float py = sinf(angle) * rad + 2.2f;          // vertical, raised so the ring stands up
			float pz = cosf(angle) * rad;                 // horizontal
			vec4 pos = center + vec4(px, py, pz, 0.0f);
			vec4 vel = vec4(0.0f, cosf(angle), -sinf(angle), 0.0f);   // swirl tangentially in YZ
			float spd = RandomRange(0.8f, 1.6f);

			particle->Position = XMFLOAT3(pos.x, pos.y, pos.z);
			particle->Velocity = XMFLOAT3(vel.x * spd, vel.y * spd, vel.z * spd);
			particle->Age = 0.0f;
			particle->LifeTime = RandomRange(0.6f, 1.1f);
			particle->StartColor = XMFLOAT4(0.50f, 1.40f, 3.00f, 1.00f);   // bright blue
			particle->EndColor = XMFLOAT4(0.10f, 0.40f, 0.90f, 0.0f);
			particle->StartSize = RandomRange(0.35f, 0.60f);              // bigger
			particle->EndSize = 0.06f;
			particle->Rotation = RandomRange(0.0f, XM_2PI);
			particle->RotationSpeed = RandomRange(-4.0f, 4.0f);
			particle->Drag = 0.4f;
			particle->GravityScale = 0.0f;
			particle->Stretch = 1.0f;
			particle->CollisionRadius = 0.0f;
			particle->Flags = 0u;
			particle->RandomSeed = gTransientParticleSeed++;
			particle->FrameIndex = 0;
			particle->FrameCount = 29u;
			particle->FrameCols = 5u;
		}
	}

	void SpawnMuzzleFlash(Player* player, bool leftHand = false)
	{
		if (player == nullptr || player->m_weaponHolstered) return;
		if (player == game.player && !game.bFirstPersonVision &&
			(WeaponType)player->m_currentWeaponType == WeaponType::Sniper &&
			player->m_isZooming && player->m_currentFov < 25.0f) return;
		if (player != game.player || game.bFirstPersonVision == false) {
			player->UpdateThirdPersonWeaponAttachmentCache();
		}

		vec4 origin;
		vec4 forward;
		vec4 right;
		vec4 up;
		TryGetMuzzleBasis(player, leftHand, origin, forward, right, up);

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

	void SpawnElectricTracer(vec4 start, vec4 direction, float distance, float sizeScale = 1.0f)
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
			core->StartSize = RandomRange(0.060f, 0.082f) * sizeScale;
			core->EndSize = 0.020f * sizeScale;
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
			particle->StartSize = RandomRange(0.028f, 0.040f) * sizeScale;
			particle->EndSize = 0.006f * sizeScale;
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

	void PlayPlayerFireVisual(Player* player, bool leftHand)
	{
		if (player == nullptr) return;
		if ((WeaponType)player->m_currentWeaponType == WeaponType::DualPistol &&
			player->m_dualBladeVisualTimer > 0.0f) return;
		if (player->SelectedWeapon >= 0 && player->SelectedWeapon < 3) {
			player->weapon[player->SelectedWeapon].OnFire();
		}
		player->TriggerUpperShoot();
		player->TriggerDualPistolRecoil(leftHand);
		SpawnMuzzleFlash(player, leftHand);
		if (player == game.player) {
			g_playGunSound = true;
		}
	}

	void UpdateDelayedWeaponFireVisuals(float deltaTime)
	{
		for (size_t i = 0; i < gDelayedWeaponFireVisuals.size();) {
			DelayedWeaponFireVisual& event = gDelayedWeaponFireVisuals[i];
			event.Delay -= max(0.0f, deltaTime);
			if (event.Delay > 0.0f) {
				++i;
				continue;
			}

			const int netObjIndex = game.GetDynamicObjectNetIndex(event.ZoneId, event.ObjectIndex);
			if (netObjIndex >= 0 && netObjIndex < (int)game.DynmaicGameObjects.size()) {
				GameObject* object = game.DynmaicGameObjects[netObjIndex];
				if (object != nullptr &&
					object->tag[GameObjectTag::Tag_Enable] &&
					*(void**)object == GameObjectType::vptr[GameObjectType::_Player]) {
					PlayPlayerFireVisual((Player*)object, event.LeftHand);
				}
			}
			gDelayedWeaponFireVisuals.erase(gDelayedWeaponFireVisuals.begin() + i);
		}
	}

	bool IsMonsterEffectOwner(UINT ownerId)
	{
		if (ownerId >= game.DynmaicGameObjects.size()) return false;
		GameObject* owner = game.DynmaicGameObjects[ownerId];
		if (owner == nullptr) return false;
		return *(void**)owner == GameObjectType::vptr[GameObjectType::_Monster];
	}

	void SpawnMonsterGunShotEffect(vec4 position, vec4 direction, float radius, float power)
	{
		constexpr float kMonsterGunEffectScale = 1.5f;
		vec4 forward = NormalizeOrFallback(direction, vec4(0, 0, 1, 0));
		vec4 upHint = abs(forward.y) > 0.85f ? vec4(1, 0, 0, 0) : vec4(0, 1, 0, 0);
		vec4 right = forward.cross(upHint);
		right = NormalizeOrFallback(right, vec4(1, 0, 0, 0));
		vec4 up = right.cross(forward);
		up = NormalizeOrFallback(up, vec4(0, 1, 0, 0));

		float flashRadius = max(radius, 0.28f) * kMonsterGunEffectScale;
		for (int i = 0; i < 9; ++i) {
			Particle* particle = AllocateTransientParticle(gMuzzleFlashParticles);
			if (particle == nullptr) break;

			vec4 dir = NormalizeOrFallback(
				forward + right * RandomRange(-0.24f, 0.24f) + up * RandomRange(-0.16f, 0.16f),
				forward);
			vec4 pos = position
				+ forward * RandomRange(0.00f, 0.10f)
				+ right * RandomRange(-0.035f, 0.035f) * flashRadius
				+ up * RandomRange(-0.030f, 0.030f) * flashRadius;

			particle->Position = XMFLOAT3(pos.x, pos.y, pos.z);
			particle->Velocity = XMFLOAT3(
				dir.x * RandomRange(6.5f, 13.0f),
				dir.y * RandomRange(6.5f, 13.0f),
				dir.z * RandomRange(6.5f, 13.0f));
			particle->Age = 0.0f;
			particle->LifeTime = RandomRange(0.055f, 0.115f);
			particle->StartColor = XMFLOAT4(2.45f, 1.35f, 0.55f, 1.0f);
			particle->EndColor = XMFLOAT4(0.18f, 0.55f, 1.25f, 0.0f);
			particle->StartSize = RandomRange(0.085f, 0.155f) * (1.0f + flashRadius * 0.32f) * kMonsterGunEffectScale;
			particle->EndSize = 0.012f * kMonsterGunEffectScale;
			particle->Rotation = RandomRange(0.0f, XM_2PI);
			particle->RotationSpeed = RandomRange(-5.0f, 5.0f);
			particle->Drag = RandomRange(5.5f, 9.0f);
			particle->GravityScale = -0.08f;
			particle->Stretch = RandomRange(1.0f, 2.2f);
			particle->CollisionRadius = 0.0f;
			particle->Flags = 0u;
			particle->RandomSeed = gTransientParticleSeed++;
			particle->FrameIndex = 0;
			particle->FrameCount = 36u;
			particle->FrameCols = 6u;
		}

		SpawnElectricTracer(position, forward, max(power, 16.0f), kMonsterGunEffectScale);
	}

	void SpawnBulletImpact(vec4 start, vec4 direction, float distance)
	{
		vec4 forward = NormalizeOrFallback(direction, vec4(0, 0, 1, 0));
		vec4 upHint = abs(forward.y) > 0.85f ? vec4(1, 0, 0, 0) : vec4(0, 1, 0, 0);
		vec4 right = forward.cross(upHint);
		right = NormalizeOrFallback(right, vec4(1, 0, 0, 0));
		vec4 up = right.cross(forward);
		up = NormalizeOrFallback(up, vec4(0, 1, 0, 0));
		vec4 impactPos = start + forward * distance;

		for (int i = 0; i < 14; ++i) {
			Particle* particle = AllocateTransientParticle(gMuzzleFlashParticles);
			if (particle == nullptr) break;

			vec4 scatter = right * RandomRange(-1.0f, 1.0f) + up * RandomRange(-0.35f, 0.95f) - forward * RandomRange(0.35f, 1.0f);
			scatter = NormalizeOrFallback(scatter, forward * -1.0f);
			vec4 pos = impactPos - forward * 0.025f + right * RandomRange(-0.035f, 0.035f) + up * RandomRange(-0.025f, 0.045f);

			particle->Position = XMFLOAT3(pos.x, pos.y, pos.z);
			particle->Velocity = XMFLOAT3(
				scatter.x * RandomRange(1.8f, 4.8f),
				scatter.y * RandomRange(1.8f, 4.8f),
				scatter.z * RandomRange(1.8f, 4.8f));
			particle->Age = 0.0f;
			particle->LifeTime = RandomRange(0.09f, 0.18f);
			particle->StartColor = XMFLOAT4(2.0f, 0.38f, 0.18f, 0.95f);
			particle->EndColor = XMFLOAT4(0.34f, 0.02f, 0.01f, 0.0f);
			particle->StartSize = RandomRange(0.045f, 0.085f);
			particle->EndSize = 0.006f;
			particle->Rotation = RandomRange(0.0f, XM_2PI);
			particle->RotationSpeed = RandomRange(-7.0f, 7.0f);
			particle->Drag = RandomRange(3.8f, 7.2f);
			particle->GravityScale = 0.15f;
			particle->Stretch = RandomRange(1.0f, 2.1f);
			particle->CollisionRadius = 0.0f;
			particle->Flags = 0u;
			particle->RandomSeed = gTransientParticleSeed++;
			particle->FrameIndex = 0;
			particle->FrameCount = 36u;
			particle->FrameCols = 6u;
		}
	}

	void SpawnStatusParticles(std::vector<Particle>& particles, ParticleSpriteSlot slot, vec4 center, vec4 extents, int count, float radiusScale)
	{
		vec4 ground = center;
		ground.y -= max(0.35f, extents.y * 0.45f);
		float radius = max(max(extents.x, extents.z), 0.35f) * radiusScale;

		for (int i = 0; i < count; ++i) {
			Particle* particle = AllocateTransientParticle(particles);
			if (particle == nullptr) break;

			float angle = RandomRange(0.0f, XM_2PI);
			float ring = RandomRange(0.10f, 1.0f) * radius;
			vec4 pos = ground + vec4(cosf(angle) * ring, RandomRange(0.0f, max(0.15f, extents.y * 0.35f)), sinf(angle) * ring, 0.0f);
			vec4 drift = vec4(cosf(angle) * RandomRange(0.05f, 0.55f), RandomRange(0.75f, 2.4f), sinf(angle) * RandomRange(0.05f, 0.55f), 0.0f);

			particle->Position = XMFLOAT3(pos.x, pos.y, pos.z);
			particle->Velocity = XMFLOAT3(drift.x, drift.y, drift.z);
			particle->Age = 0.0f;
			particle->Rotation = RandomRange(0.0f, XM_2PI);
			particle->RotationSpeed = RandomRange(-2.6f, 2.6f);
			particle->Drag = RandomRange(0.75f, 1.75f);
			particle->CollisionRadius = 0.0f;
			particle->Flags = 0u;
			particle->RandomSeed = gTransientParticleSeed++;

			switch (slot) {
			case ParticleSpriteSlot::Ice:
				particle->LifeTime = RandomRange(0.42f, 0.72f);
				particle->StartColor = XMFLOAT4(0.72f, 1.40f, 2.90f, 0.90f);
				particle->EndColor = XMFLOAT4(0.20f, 0.62f, 1.35f, 0.0f);
				particle->StartSize = RandomRange(0.18f, 0.34f) * max(radiusScale, 0.9f);
				particle->EndSize = 0.035f;
				particle->GravityScale = -0.06f;
				particle->Stretch = RandomRange(0.9f, 1.7f);
				particle->FrameIndex = rand() % 29;
				particle->FrameCount = 29u;
				particle->FrameCols = 5u;
				break;
			case ParticleSpriteSlot::FireSecondary:
				particle->LifeTime = RandomRange(0.36f, 0.64f);
				particle->StartColor = XMFLOAT4(2.75f, 0.92f, 0.20f, 0.92f);
				particle->EndColor = XMFLOAT4(0.38f, 0.04f, 0.00f, 0.0f);
				particle->StartSize = RandomRange(0.16f, 0.32f) * max(radiusScale, 0.9f);
				particle->EndSize = 0.020f;
				particle->GravityScale = -0.22f;
				particle->Stretch = RandomRange(1.7f, 3.2f);
				particle->FrameIndex = 0;
				particle->FrameCount = 10u;
				particle->FrameCols = 5u;
				break;
			case ParticleSpriteSlot::Yellow:
				particle->LifeTime = RandomRange(0.20f, 0.36f);
				particle->StartColor = XMFLOAT4(2.85f, 2.35f, 0.28f, 0.95f);
				particle->EndColor = XMFLOAT4(0.90f, 0.32f, 0.02f, 0.0f);
				particle->StartSize = RandomRange(0.24f, 0.48f);
				particle->EndSize = 0.040f;
				particle->GravityScale = 0.0f;
				particle->Stretch = RandomRange(1.2f, 2.5f);
				particle->FrameIndex = rand() % 16;
				particle->FrameCount = 16u;
				particle->FrameCols = 4u;
				break;
			case ParticleSpriteSlot::Hack:
				particle->LifeTime = RandomRange(0.46f, 0.78f);
				particle->StartColor = XMFLOAT4(1.75f, 0.34f, 2.85f, 0.90f);
				particle->EndColor = XMFLOAT4(0.36f, 0.02f, 0.78f, 0.0f);
				particle->StartSize = RandomRange(0.28f, 0.58f);
				particle->EndSize = 0.045f;
				particle->GravityScale = -0.04f;
				particle->Stretch = RandomRange(0.9f, 1.8f);
				particle->FrameIndex = rand() % 16;
				particle->FrameCount = 16u;
				particle->FrameCols = 4u;
				break;
			case ParticleSpriteSlot::Heal:
				particle->LifeTime = RandomRange(0.52f, 0.86f);
				particle->StartColor = XMFLOAT4(0.36f, 2.65f, 0.96f, 0.88f);
				particle->EndColor = XMFLOAT4(0.02f, 0.56f, 0.28f, 0.0f);
				particle->StartSize = RandomRange(0.24f, 0.52f);
				particle->EndSize = 0.035f;
				particle->GravityScale = -0.12f;
				particle->Stretch = RandomRange(1.0f, 2.0f);
				particle->FrameIndex = rand() % 16;
				particle->FrameCount = 16u;
				particle->FrameCols = 4u;
				break;
			default:
				break;
			}
		}
	}

	void SpawnBurningStatusEffect(vec4 center, vec4 extents)
	{
		SpawnStatusParticles(gStatusFireParticles, ParticleSpriteSlot::FireSecondary, center, extents, 7, 1.18f);
	}

	void SpawnIceStatusEffect(vec4 center, vec4 extents)
	{
		SpawnStatusParticles(gStatusIceParticles, ParticleSpriteSlot::Ice, center, extents, 6, 1.20f);
	}

	void SpawnParalyzeStatusEffect(vec4 center, vec4 extents)
	{
		SpawnStatusParticles(gStatusYellowParticles, ParticleSpriteSlot::Yellow, center + vec4(0.0f, extents.y * 0.35f, 0.0f, 0.0f), extents, 5, 1.08f);
	}

	void SpawnHackStatusEffect(vec4 center, vec4 extents)
	{
		SpawnStatusParticles(gStatusHackParticles, ParticleSpriteSlot::Hack, center, extents, 6, 1.12f);
	}

	void SpawnHealStatusEffect(vec4 center, vec4 extents, int count)
	{
		SpawnStatusParticles(gStatusHealParticles, ParticleSpriteSlot::Heal, center, extents, max(count, 1), 1.25f);
	}

	vec4 RotateDirectionYaw(vec4 direction, float degree)
	{
		direction.y = 0.0f;
		direction = NormalizeOrFallback(direction, vec4(0, 0, 1, 0));
		XMVECTOR quaternion = XMQuaternionRotationRollPitchYaw(0.0f, degree * XM_PI / 180.0f, 0.0f);
		return XMVector3Rotate(direction, quaternion);
	}

	void SpawnJuggernautFireball(vec4 position, vec4 direction, float range, float radius)
	{
		vec4 forward = NormalizeOrFallback(direction, vec4(0, 0, 1, 0));
		vec4 upHint = abs(forward.y) > 0.85f ? vec4(1, 0, 0, 0) : vec4(0, 1, 0, 0);
		vec4 right = forward.cross(upHint);
		right = NormalizeOrFallback(right, vec4(1, 0, 0, 0));
		vec4 up = right.cross(forward);
		up = NormalizeOrFallback(up, vec4(0, 1, 0, 0));
		vec4 origin = position + up * 1.12f + forward * 0.9f;

		int particleCount = 34;
		for (int i = 0; i < particleCount; ++i) {
			Particle* particle = AllocateTransientParticle(gMuzzleFlashParticles);
			if (particle == nullptr) break;

			float shell = RandomRange(0.0f, 1.0f);
			float angle = RandomRange(0.0f, XM_2PI);
			float ballRadius = max(radius, 0.65f) * RandomRange(0.10f, 0.38f);
			vec4 radial = right * (cosf(angle) * ballRadius) + up * (sinf(angle) * ballRadius * 0.78f);
			vec4 pos = origin
				+ forward * RandomRange(0.0f, 0.38f)
				+ radial;
			vec4 velocity = NormalizeOrFallback(forward + radial * 0.18f + up * RandomRange(-0.025f, 0.08f), forward);

			particle->Position = XMFLOAT3(pos.x, pos.y, pos.z);
			float speed = RandomRange(max(12.0f, range * 0.45f), max(18.0f, range * 0.72f));
			particle->Velocity = XMFLOAT3(velocity.x * speed, velocity.y * speed, velocity.z * speed);
			particle->Age = 0.0f;
			particle->LifeTime = RandomRange(0.30f, 0.52f);
			particle->StartColor = shell > 0.68f ? XMFLOAT4(3.2f, 1.45f, 0.38f, 0.96f) : XMFLOAT4(2.15f, 0.62f, 0.12f, 0.88f);
			particle->EndColor = XMFLOAT4(0.28f, 0.015f, 0.0f, 0.0f);
			particle->StartSize = RandomRange(0.18f, 0.34f) * max(radius, 0.8f);
			particle->EndSize = 0.035f;
			particle->Rotation = RandomRange(0.0f, XM_2PI);
			particle->RotationSpeed = RandomRange(-4.0f, 4.0f);
			particle->Drag = RandomRange(0.55f, 1.2f);
			particle->GravityScale = -0.02f;
			particle->Stretch = RandomRange(1.4f, 2.6f);
			particle->CollisionRadius = 0.0f;
			particle->Flags = 0u;
			particle->RandomSeed = gTransientParticleSeed++;
			particle->FrameIndex = 0;
			particle->FrameCount = 36u;
			particle->FrameCols = 6u;
		}
	}

	void SpawnFrostShardProjectile(vec4 position, vec4 direction, float range, float radius)
	{
		vec4 forward = NormalizeOrFallback(direction, vec4(0, 0, 1, 0));
		vec4 upHint = abs(forward.y) > 0.85f ? vec4(1, 0, 0, 0) : vec4(0, 1, 0, 0);
		vec4 right = forward.cross(upHint);
		right = NormalizeOrFallback(right, vec4(1, 0, 0, 0));
		vec4 up = right.cross(forward);
		up = NormalizeOrFallback(up, vec4(0, 1, 0, 0));
		vec4 origin = position + up * 1.08f + forward * 0.92f;
		float shardRange = max(range, 18.0f);
		float shardRadius = max(radius, 1.1f);

		const int coreCount = 38;
		for (int i = 0; i < coreCount; ++i) {
			Particle* particle = AllocateTransientParticle(gIceProjectileParticles);
			if (particle == nullptr) break;

			float t = RandomRange(0.0f, 1.0f);
			float angle = RandomRange(0.0f, XM_2PI);
			float width = shardRadius * RandomRange(0.02f, 0.16f) * (1.0f + t * 0.35f);
			vec4 radial = right * (cosf(angle) * width) + up * (sinf(angle) * width * 0.72f);
			vec4 pos = origin + forward * RandomRange(0.0f, 0.65f) + radial;
			vec4 velocity = NormalizeOrFallback(forward + radial * 0.10f + up * RandomRange(-0.02f, 0.06f), forward);
			float speed = RandomRange(shardRange * 0.80f, shardRange * 1.12f);

			particle->Position = XMFLOAT3(pos.x, pos.y, pos.z);
			particle->Velocity = XMFLOAT3(velocity.x * speed, velocity.y * speed, velocity.z * speed);
			particle->Age = RandomRange(0.0f, 0.035f);
			particle->LifeTime = RandomRange(0.34f, 0.58f);
			particle->StartColor = XMFLOAT4(0.45f, 1.25f, 2.25f, 0.92f);
			particle->EndColor = XMFLOAT4(0.04f, 0.24f, 0.62f, 0.0f);
			particle->StartSize = RandomRange(0.12f, 0.24f) * (1.0f + shardRadius * 0.18f);
			particle->EndSize = 0.018f;
			particle->Rotation = RandomRange(0.0f, XM_2PI);
			particle->RotationSpeed = RandomRange(-7.0f, 7.0f);
			particle->Drag = RandomRange(0.35f, 0.85f);
			particle->GravityScale = -0.015f;
			particle->Stretch = RandomRange(2.8f, 5.4f);
			particle->CollisionRadius = 0.0f;
			particle->Flags = 0u;
			particle->RandomSeed = gTransientParticleSeed++;
			particle->FrameIndex = 0;
			particle->FrameCount = 29u;
			particle->FrameCols = 5u;
		}

		const int mistCount = 30;
		for (int i = 0; i < mistCount; ++i) {
			Particle* particle = AllocateTransientParticle(gIceProjectileParticles);
			if (particle == nullptr) break;

			float t = RandomRange(0.0f, 1.0f);
			float width = shardRadius * (0.12f + t * 0.58f);
			vec4 lateral = right * RandomRange(-width, width) + up * RandomRange(-width * 0.24f, width * 0.46f);
			vec4 pos = origin - forward * RandomRange(0.08f, 0.55f) + lateral;
			vec4 velocity = NormalizeOrFallback(forward * RandomRange(0.55f, 0.9f) + lateral * 0.10f, forward);
			float speed = RandomRange(shardRange * 0.35f, shardRange * 0.62f);

			particle->Position = XMFLOAT3(pos.x, pos.y, pos.z);
			particle->Velocity = XMFLOAT3(velocity.x * speed, velocity.y * speed, velocity.z * speed);
			particle->Age = RandomRange(0.0f, 0.08f);
			particle->LifeTime = RandomRange(0.28f, 0.52f);
			particle->StartColor = XMFLOAT4(0.28f, 0.88f, 1.70f, 0.55f);
			particle->EndColor = XMFLOAT4(0.02f, 0.12f, 0.36f, 0.0f);
			particle->StartSize = RandomRange(0.18f, 0.42f) * (1.0f + t * 0.45f);
			particle->EndSize = 0.04f;
			particle->Rotation = RandomRange(0.0f, XM_2PI);
			particle->RotationSpeed = RandomRange(-2.0f, 2.0f);
			particle->Drag = RandomRange(1.2f, 2.4f);
			particle->GravityScale = -0.03f;
			particle->Stretch = RandomRange(1.0f, 2.2f);
			particle->CollisionRadius = 0.0f;
			particle->Flags = 0u;
			particle->RandomSeed = gTransientParticleSeed++;
			particle->FrameIndex = 0;
			particle->FrameCount = 29u;
			particle->FrameCols = 5u;
		}
	}

	void SpawnJuggernautFlamethrower(vec4 position, vec4 direction, float range, float radius)
	{
		vec4 forward = NormalizeOrFallback(direction, vec4(0, 0, 1, 0));
		vec4 upHint = abs(forward.y) > 0.85f ? vec4(1, 0, 0, 0) : vec4(0, 1, 0, 0);
		vec4 right = forward.cross(upHint);
		right = NormalizeOrFallback(right, vec4(1, 0, 0, 0));
		vec4 up = right.cross(forward);
		up = NormalizeOrFallback(up, vec4(0, 1, 0, 0));
		vec4 nozzle = position + up * 1.12f + forward * 0.28f;
		float flameRange = max(range, 40.0f);
		float flameRadius = max(radius, 1.9f);

		const int coreCount = 42;
		for (int i = 0; i < coreCount; ++i) {
			Particle* particle = AllocateTransientParticle(gMuzzleFlashParticles);
			if (particle == nullptr) break;

			float t = RandomRange(0.0f, 1.0f);
			float along = flameRange * (0.015f + t * 0.86f);
			float width = flameRadius * (0.10f + t * 0.62f);
			vec4 lateral = right * RandomRange(-width, width) + up * RandomRange(-width * 0.28f, width * 0.48f);
			vec4 pos = nozzle + forward * along + lateral;
			vec4 velocity = NormalizeOrFallback(forward + lateral * (0.05f + t * 0.02f) + up * RandomRange(-0.035f, 0.065f), forward);
			float heat = 1.0f - t;

			particle->Position = XMFLOAT3(pos.x, pos.y, pos.z);
			float speed = RandomRange(18.0f, 34.0f) * (0.72f + heat * 0.35f);
			particle->Velocity = XMFLOAT3(velocity.x * speed, velocity.y * speed, velocity.z * speed);
			particle->Age = RandomRange(0.0f, 0.07f);
			particle->LifeTime = RandomRange(0.20f, 0.46f) * (1.0f + t * 0.55f);
			particle->StartColor = t < 0.33f ? XMFLOAT4(3.8f, 1.62f, 0.45f, 0.92f) : XMFLOAT4(2.1f, 0.42f, 0.08f, 0.78f);
			particle->EndColor = XMFLOAT4(0.18f, 0.015f, 0.0f, 0.0f);
			particle->StartSize = RandomRange(0.18f, 0.42f) * (1.0f + t * 1.4f);
			particle->EndSize = 0.02f;
			particle->Rotation = RandomRange(0.0f, XM_2PI);
			particle->RotationSpeed = RandomRange(-6.0f, 6.0f);
			particle->Drag = RandomRange(0.85f, 2.0f);
			particle->GravityScale = -0.035f;
			particle->Stretch = RandomRange(2.2f, 4.6f);
			particle->CollisionRadius = 0.0f;
			particle->Flags = 0u;
			particle->RandomSeed = gTransientParticleSeed++;
			particle->FrameIndex = 0;
			particle->FrameCount = 36u;
			particle->FrameCols = 6u;
		}
	}

	void SpawnBossExplosionParticles(vec4 position, float radius, float power)
	{
		position.y += 0.35f;
		float blastRadius = max(radius, 1.0f);
		float blastPower = max(power, 1.0f);
		const int particleCount = 58;

		for (int i = 0; i < particleCount; ++i) {
			Particle* particle = AllocateTransientParticle(gBossExplosionParticles);
			if (particle == nullptr) break;

			float t = static_cast<float>(i) / static_cast<float>(max(particleCount - 1, 1));
			float angle = RandomRange(0.0f, XM_2PI);
			float ring = sqrtf(RandomRange(0.0f, 1.0f));
			vec4 radial = vec4(cosf(angle) * ring, 0.0f, sinf(angle) * ring, 0.0f);
			vec4 scatter = NormalizeOrFallback(radial + vec4(0.0f, RandomRange(0.12f, 0.82f), 0.0f, 0.0f), vec4(0, 1, 0, 0));
			vec4 pos = position
				+ radial * (blastRadius * RandomRange(0.02f, 0.34f))
				+ vec4(0.0f, RandomRange(0.0f, 0.28f), 0.0f, 0.0f);

			bool coreFlash = i < 10;
			bool smoke = t > 0.62f;
			float speed = coreFlash ? RandomRange(2.2f, 5.2f) : RandomRange(3.2f, 8.8f);
			speed *= 0.55f + blastPower * 0.018f;

			particle->Position = XMFLOAT3(pos.x, pos.y, pos.z);
			particle->Velocity = XMFLOAT3(
				scatter.x * speed,
				scatter.y * speed * (coreFlash ? 0.45f : 0.72f),
				scatter.z * speed);
			particle->Age = smoke ? RandomRange(0.04f, 0.14f) : 0.0f;
			particle->LifeTime = coreFlash ? RandomRange(0.22f, 0.34f) : RandomRange(0.48f, 0.92f);
			particle->StartColor = coreFlash
				? XMFLOAT4(3.6f, 2.2f, 0.92f, 1.0f)
				: (smoke ? XMFLOAT4(0.82f, 0.58f, 0.38f, 0.58f) : XMFLOAT4(2.4f, 1.08f, 0.36f, 0.92f));
			particle->EndColor = smoke
				? XMFLOAT4(0.10f, 0.09f, 0.08f, 0.0f)
				: XMFLOAT4(0.30f, 0.08f, 0.02f, 0.0f);
			particle->StartSize = (coreFlash ? RandomRange(0.70f, 1.20f) : RandomRange(0.46f, 1.05f)) * (0.75f + blastRadius * 0.18f);
			particle->EndSize = smoke ? RandomRange(1.10f, 2.15f) * (0.85f + blastRadius * 0.15f) : RandomRange(0.12f, 0.32f);
			particle->Rotation = RandomRange(0.0f, XM_2PI);
			particle->RotationSpeed = RandomRange(-4.0f, 4.0f);
			particle->Drag = smoke ? RandomRange(1.4f, 2.8f) : RandomRange(2.4f, 4.6f);
			particle->GravityScale = smoke ? -0.10f : 0.10f;
			particle->Stretch = coreFlash ? 0.25f : RandomRange(0.45f, 1.15f);
			particle->CollisionRadius = 0.0f;
			particle->Flags = 0u;
			particle->RandomSeed = gTransientParticleSeed++;
			particle->FrameIndex = 0;
			particle->FrameCount = 64u;
			particle->FrameCols = 8u;
		}
	}
}

void Game::SpawnSkillEffect(SkillEffectType type, vec4 position, vec4 direction, UINT ownerId, float radius, float power, float duration)
{
	if (type == SkillEffectType::Drone_Heal) {
		vec4 beamDirection = NormalizeOrFallback(direction, vec4(0, 0, 1, 0));
		vec4 target = position + beamDirection * max(radius, 1.0f);
		QueueSupportBeam(position, target, 0.12f, max(duration, 0.20f), vec4(0.28f, 1.65f, 0.62f, 0.24f));
		SpawnHealStatusEffect(target, vec4(0.65f, 1.15f, 0.65f, 0.0f), 4);
		return;
	}
	else if (type == SkillEffectType::Drone_Assault) {
		Player* ownerPlayer = ownerId < DynmaicGameObjects.size() ? dynamic_cast<Player*>(DynmaicGameObjects[ownerId]) : nullptr;
		if (ownerPlayer != nullptr) ownerPlayer->m_droneAssaultVisualTimer = max(ownerPlayer->m_droneAssaultVisualTimer, duration);
		if (duration <= 0.5f) {
			vec4 beamDirection = NormalizeOrFallback(direction, vec4(0, 0, 1, 0));
			vec4 target = position + beamDirection * max(radius, 1.0f);
			QueueSupportBeam(position, target, 0.10f, max(duration, 0.16f), vec4(2.15f, 1.55f, 0.18f, 0.22f));
			SpawnStatusParticles(gStatusYellowParticles, ParticleSpriteSlot::Yellow, position,
				vec4(0.32f, 0.32f, 0.32f, 0.0f), 2, 0.72f);
		}
		return;
	}
	else if (type == SkillEffectType::Drone_Flight) {
		Player* ownerPlayer = ownerId < DynmaicGameObjects.size() ? dynamic_cast<Player*>(DynmaicGameObjects[ownerId]) : nullptr;
		if (ownerPlayer != nullptr) ownerPlayer->m_droneFlightVisualTimer = max(ownerPlayer->m_droneFlightVisualTimer, duration);
		SpawnStatusParticles(gStatusYellowParticles, ParticleSpriteSlot::Yellow, position,
			vec4(0.75f, 0.45f, 0.75f, 0.0f), duration > 1.0f ? 5 : 2, 0.82f);

		FrostSnowWaveVisual healWave;
		healWave.Active = true;
		healWave.HealWave = true;
		if (ownerPlayer != nullptr) healWave.Position = ownerPlayer->worldMat.pos;
		else healWave.Position = position - vec4(0, 0.9f, 0, 0);
		healWave.Position.w = 1.0f;
		healWave.Age = 0.0f;
		healWave.Duration = 0.92f;
		healWave.Radius = 8.5f;
		gFrostSnowWaveVisuals.push_back(healWave);
		return;
	}
	else if (type == SkillEffectType::Juggernaut_FireProjectile) {
		const float fireAngles[] = { -30.0f, 0.0f, 30.0f };
		for (float angle : fireAngles) {
			SpawnJuggernautFireball(position, RotateDirectionYaw(direction, angle), max(power, 18.0f), max(radius, 0.85f));
		}
		return;
	}
	else if (type == SkillEffectType::Juggernaut_UltimateFire) {
		SpawnJuggernautFlamethrower(position, direction, 56.0f, max(radius, 1.6f));
		return;
	}
	else if (type == SkillEffectType::Aegis_ShieldAura) {
		AegisShieldVisual* slot = nullptr;
		for (AegisShieldVisual& visual : gAegisShieldVisuals) {
			if (visual.Active && visual.OwnerId == ownerId && !visual.GroundDisk) {
				slot = &visual;
				break;
			}
		}
		if (slot == nullptr) {
			for (AegisShieldVisual& visual : gAegisShieldVisuals) {
				if (!visual.Active) {
					slot = &visual;
					break;
				}
			}
		}
		if (slot == nullptr) {
			gAegisShieldVisuals.push_back(AegisShieldVisual{});
			slot = &gAegisShieldVisuals.back();
		}
		slot->Active = true;
		slot->OwnerId = ownerId;
		slot->Position = position;
		slot->Radius = min(max(radius, 1.35f), 2.35f);
		slot->Age = 0.0f;
		slot->Duration = min(max(duration, 0.36f), 1.05f);
		slot->GroundDisk = false;
		return;
	}
	else if (type == SkillEffectType::Sniper_Railgun) {
		RailgunBeamVisual* slot = nullptr;
		for (RailgunBeamVisual& visual : gRailgunBeamVisuals) {
			if (!visual.Active) {
				slot = &visual;
				break;
			}
		}
		if (slot == nullptr) {
			gRailgunBeamVisuals.push_back(RailgunBeamVisual{});
			slot = &gRailgunBeamVisuals.back();
		}
		slot->Active = true;
		slot->Origin = position;
		slot->Direction = NormalizeOrFallback(direction, vec4(0, 0, 1, 0));
		slot->Origin.y += 0.04f;
		slot->Origin += slot->Direction * 1.85f;
		slot->Age = 0.0f;
		slot->Duration = max(duration, 0.42f);
		slot->Length = 118.0f;
		slot->Width = min(max(radius * 0.48f, 0.32f), 0.60f);
		slot->Tint = vec4(0.66f, 0.96f, 1.45f, 0.16f);
		return;
	}
	else if (type == SkillEffectType::Electric_Arc && radius <= 0.75f && duration <= 0.22f) {
		RailgunOrbitVisual* slot = nullptr;
		for (RailgunOrbitVisual& visual : gRailgunOrbitVisuals) {
			if (visual.Active && visual.OwnerId == ownerId) {
				slot = &visual;
				break;
			}
		}
		if (slot == nullptr) {
			for (RailgunOrbitVisual& visual : gRailgunOrbitVisuals) {
				if (!visual.Active) {
					slot = &visual;
					break;
				}
			}
		}
		if (slot == nullptr) {
			gRailgunOrbitVisuals.push_back(RailgunOrbitVisual{});
			slot = &gRailgunOrbitVisuals.back();
		}
		slot->Active = true;
		slot->OwnerId = ownerId;
		slot->Position = position;
		slot->Position.w = 1.0f;
		slot->Age = 0.0f;
		slot->Duration = 0.95f;
		slot->Radius = 0.74f;
		slot->Yellow = false;
		return;
	}
	else if (type == SkillEffectType::DualPistol_Awaken) {
		RailgunOrbitVisual* slot = nullptr;
		for (RailgunOrbitVisual& visual : gRailgunOrbitVisuals) {
			if (visual.Active && visual.OwnerId == ownerId && visual.Yellow) {
				slot = &visual;
				break;
			}
		}
		if (slot == nullptr) {
			gRailgunOrbitVisuals.push_back(RailgunOrbitVisual{});
			slot = &gRailgunOrbitVisuals.back();
		}
		slot->Active = true;
		slot->OwnerId = ownerId;
		slot->Position = position;
		slot->Position.w = 1.0f;
		slot->Age = 0.0f;
		slot->Duration = max(duration, 0.95f);
		slot->Radius = 0.68f;
		slot->Yellow = true;
		return;
	}
	else if (type == SkillEffectType::DualPistol_BladeMode) {
		// Long casts own the persistent floor marker. Short casts only drive the blade swing.
		if (duration > 0.5f) {
			DualBladeFloorVisual* slot = nullptr;
			for (DualBladeFloorVisual& visual : gDualBladeFloorVisuals) {
				if (visual.Active && visual.OwnerId == ownerId) {
					slot = &visual;
					break;
				}
			}
			if (slot == nullptr) {
				gDualBladeFloorVisuals.push_back(DualBladeFloorVisual{});
				slot = &gDualBladeFloorVisuals.back();
			}
			slot->Active = true;
			slot->OwnerId = ownerId;
			slot->Position = position;
			slot->Position.w = 1.0f;
			slot->Age = 0.0f;
			slot->Duration = duration;
			slot->Radius = 1.15f;
		}
		return;
	}
	else if (type == SkillEffectType::Hacker_EMPField || type == SkillEffectType::Hacker_EMPBurst) {
		HackerEmpVisual* slot = nullptr;
		const bool groundField = type == SkillEffectType::Hacker_EMPField;
		for (HackerEmpVisual& visual : gHackerEmpVisuals) {
			if (visual.GroundField == groundField && visual.OwnerId == ownerId) {
				slot = &visual;
				break;
			}
		}
		if (slot == nullptr) {
			for (HackerEmpVisual& visual : gHackerEmpVisuals) {
				if (!visual.Active) {
					slot = &visual;
					break;
				}
			}
		}
		if (slot == nullptr) {
			gHackerEmpVisuals.push_back(HackerEmpVisual{});
			slot = &gHackerEmpVisuals.back();
		}
		slot->Active = true;
		slot->GroundField = groundField;
		slot->OwnerId = ownerId;
		slot->Position = position;
		slot->Position.w = 1.0f;
		slot->Age = 0.0f;
		slot->Duration = groundField ? max(duration, 0.72f) : max(duration, 1.0f);
		slot->Radius = groundField ? max(radius, 12.0f) : max(radius, 16.0f);
		return;
	}
	else if (type == SkillEffectType::Frost_Cone) {
		SpawnFrostShardProjectile(position, direction, max(power, 20.0f), max(radius, 1.1f));
		return;
	}
	else if (type == SkillEffectType::Rifle_GrenadeTrail && IsMonsterEffectOwner(ownerId)) {
		SpawnMonsterGunShotEffect(position, direction, radius, power);
		return;
	}

	if (type == SkillEffectType::Aegis_ShieldEnergy && radius >= 3.0f && duration > 1.0f) {
		AegisShieldVisual* slot = nullptr;
		for (AegisShieldVisual& visual : gAegisShieldVisuals) {
			if (visual.Active && visual.OwnerId == ownerId && visual.GroundDisk) {
				slot = &visual;
				break;
			}
		}
		if (slot == nullptr) {
			for (AegisShieldVisual& visual : gAegisShieldVisuals) {
				if (!visual.Active) {
					slot = &visual;
					break;
				}
			}
		}
		if (slot == nullptr) {
			gAegisShieldVisuals.push_back(AegisShieldVisual{});
			slot = &gAegisShieldVisuals.back();
		}
		slot->Active = true;
		slot->OwnerId = ownerId;
		slot->Position = position;
		slot->Radius = radius;
		slot->Age = 0.0f;
		slot->Duration = duration;
		slot->GroundDisk = true;
		return;
	}

	SkillEffectType runtimeType = type;
	switch (type) {
	case SkillEffectType::Healer_HealAura:
		runtimeType = SkillEffectType::Electric_Arc;
		break;
	case SkillEffectType::Tank_ShockWave:
		runtimeType = SkillEffectType::Fire_Ring;
		break;
	case SkillEffectType::Juggernaut_Taunt:
		runtimeType = SkillEffectType::Fire_Ring;
		break;
	case SkillEffectType::Aegis_Barrier:
		runtimeType = SkillEffectType::Electric_Burst;
		break;
	case SkillEffectType::Aegis_ShieldAura:
		runtimeType = SkillEffectType::Aegis_ShieldAura;
		break;
	case SkillEffectType::Aegis_ShieldCharge:
		runtimeType = SkillEffectType::Electric_Burst;
		break;
	case SkillEffectType::Rifle_TacticalGrenade:
	case SkillEffectType::Rifle_MissileBarrage:
		runtimeType = SkillEffectType::Explosion_Blast;
		break;
	case SkillEffectType::Rifle_GrenadeTrail:
		runtimeType = SkillEffectType::Rifle_GrenadeTrail;
		break;
	case SkillEffectType::Rifle_AirStrikeTrail:
		runtimeType = SkillEffectType::Rifle_AirStrikeTrail;
		break;
	case SkillEffectType::Rifle_StimField:
		runtimeType = SkillEffectType::Rifle_StimField;
		break;
	case SkillEffectType::Blood_Hit:
		runtimeType = SkillEffectType::Blood_Hit;
		break;
	case SkillEffectType::Explosion_Blast:
		runtimeType = SkillEffectType::Explosion_Blast;
		break;
	case SkillEffectType::Aegis_ShieldEnergy:
		runtimeType = SkillEffectType::Aegis_ShieldEnergy;
		break;
	case SkillEffectType::Rifle_StimPack:
	case SkillEffectType::Sniper_ModeSwitch:
	case SkillEffectType::DualPistol_DeathDash:
	case SkillEffectType::DualPistol_Awaken:
		runtimeType = SkillEffectType::Electric_Burst;
		break;
	case SkillEffectType::Sniper_GrappleHook:
	case SkillEffectType::Sniper_Railgun:
	case SkillEffectType::DualPistol_BladeMode:
		runtimeType = SkillEffectType::Electric_Arc;
		break;
	case SkillEffectType::Hacker_Hack:
	case SkillEffectType::Hacker_EMPBurst:
		runtimeType = SkillEffectType::Electric_Burst;
		break;
	case SkillEffectType::Hacker_EMPField:
		runtimeType = SkillEffectType::Frost_Blizzard;
		break;
	case SkillEffectType::Bomber_SpeedBurst:
		runtimeType = SkillEffectType::Bomber_HealExplosion;
		break;
	case SkillEffectType::Bomber_AmmoSwitch:
		return;
	default:
		break;
	}

	ParticleEffectRuntime* effect = FindParticleEffectRuntime(runtimeType);
	if (effect == nullptr) return;

	if (type == SkillEffectType::Healer_HealAura) {
		*effect->SpriteSlot = ParticleSpriteSlot::Heal;
		SpawnHealStatusEffect(position, vec4(max(radius * 0.55f, 0.45f), max(radius * 0.75f, 1.0f), max(radius * 0.55f, 0.45f), 0.0f));
	}
	else if (type == SkillEffectType::Tank_ShockWave) {
		*effect->SpriteSlot = ParticleSpriteSlot::FireSecondary;
	}
	else if (type == SkillEffectType::Juggernaut_FireProjectile || type == SkillEffectType::Juggernaut_Taunt || type == SkillEffectType::Juggernaut_UltimateFire) {
		*effect->SpriteSlot = ParticleSpriteSlot::FireSecondary;
	}
	else if (type == SkillEffectType::Frost_Cone || type == SkillEffectType::Frost_IceBlock || type == SkillEffectType::Frost_Blizzard) {
		*effect->SpriteSlot = ParticleSpriteSlot::Ice;
		if (type == SkillEffectType::Frost_Blizzard) {
			FrostSnowWaveVisual wave;
			wave.Active = true;
			wave.Position = position;
			wave.Position.w = 1.0f;
			wave.Duration = 1.10f;
			wave.Radius = max(radius * 1.12f, 9.0f);
			gFrostSnowWaveVisuals.push_back(wave);
			radius *= 1.22f;
		}
	}
	else if (type == SkillEffectType::Aegis_ShieldCharge) {
		*effect->SpriteSlot = ParticleSpriteSlot::Electric;
		radius = min(max(radius, 1.05f), 1.35f);
		duration = min(duration, 0.22f);
	}
	else if (type == SkillEffectType::Aegis_ShieldAura) {
		*effect->SpriteSlot = ParticleSpriteSlot::Shield;
	}
	else if (type == SkillEffectType::Aegis_Barrier) {
		*effect->SpriteSlot = ParticleSpriteSlot::Electric;
		position.y += 0.35f;
		radius = min(max(radius, 1.15f), 1.65f);
		power = max(power, 18.0f);
		duration = min(max(duration, 0.55f), 0.85f);
	}
	else if (type == SkillEffectType::Rifle_TacticalGrenade || type == SkillEffectType::Rifle_MissileBarrage) {
		*effect->SpriteSlot = ParticleSpriteSlot::Explosion;
	}
	else if (type == SkillEffectType::Rifle_GrenadeTrail) {
		*effect->SpriteSlot = ParticleSpriteSlot::Explosion;
	}
	else if (type == SkillEffectType::Rifle_AirStrikeTrail) {
		*effect->SpriteSlot = ParticleSpriteSlot::AirStrike;
		SkillMissileVisual visual;
		vec4 dropDir = NormalizeOrFallback(direction, vec4(0, -1, 0, 0));
		float dropDistance = min(max(position.y, 8.0f), 24.0f);
		visual.Target = position + dropDir * dropDistance;
		visual.Target.y += 0.25f;
		visual.Target.w = 1.0f;
		visual.Start = visual.Target + vec4(RandomRange(-1.2f, 1.2f), dropDistance, RandomRange(-1.2f, 1.2f), 0.0f);
		visual.Start.w = 1.0f;
		visual.Duration = max(duration, 0.55f);
		visual.Radius = max(radius, 0.8f);
		visual.Active = true;
		gSkillMissileVisuals.push_back(visual);
	}
	else if (type == SkillEffectType::Rifle_StimField) {
		*effect->SpriteSlot = ParticleSpriteSlot::Electric;
	}
	else if (type == SkillEffectType::Hacker_Hack) {
		*effect->SpriteSlot = ParticleSpriteSlot::Hack;
		radius = min(max(radius, 0.55f), 1.10f);
		power = max(power, 10.0f);
		duration = min(max(duration, 0.16f), 0.28f);
	}
	else if (type == SkillEffectType::Hacker_EMPField) {
		*effect->SpriteSlot = ParticleSpriteSlot::Hack;
		position.y += 0.08f;
		radius = min(max(radius, 8.0f), 12.0f);
		power = max(power, 20.0f);
		duration = min(max(duration, 0.38f), 0.62f);
	}
	else if (type == SkillEffectType::Hacker_EMPBurst) {
		*effect->SpriteSlot = ParticleSpriteSlot::Hack;
		radius = min(max(radius, 12.0f), 16.0f);
		power = max(power, 30.0f);
		duration = min(max(duration, 0.65f), 1.10f);
	}
	else if (type == SkillEffectType::Bomber_SpeedBurst) {
		*effect->SpriteSlot = ParticleSpriteSlot::Heal;
		position.y += 0.25f;
		radius = min(max(radius, 1.5f), 2.2f);
		power = max(power, 18.0f);
		duration = min(max(duration, 0.65f), 0.95f);
		SpawnHealStatusEffect(position, vec4(1.0f, 1.5f, 1.0f, 0.0f));
	}
	else if (type == SkillEffectType::Bomber_FireProjectile) {
		*effect->SpriteSlot = ParticleSpriteSlot::FireSecondary;
		radius = min(max(radius, 0.55f), 0.85f);
		power = max(power, 12.0f);
		duration = min(max(duration, 0.16f), 0.24f);
	}
	else if (type == SkillEffectType::Bomber_HealProjectile) {
		*effect->SpriteSlot = ParticleSpriteSlot::Heal;
		radius = min(max(radius, 0.55f), 0.85f);
		power = max(power, 10.0f);
		duration = min(max(duration, 0.16f), 0.24f);
	}
	else if (type == SkillEffectType::Bomber_FireExplosion) {
		*effect->SpriteSlot = ParticleSpriteSlot::Explosion;
		radius = min(max(radius, 6.0f), 8.0f);
		power = max(power, 20.0f);
		duration = min(max(duration, 0.72f), 1.05f);
	}
	else if (type == SkillEffectType::Bomber_HealExplosion) {
		*effect->SpriteSlot = ParticleSpriteSlot::Heal;
		radius = min(max(radius, 6.0f), 8.0f);
		power = max(power, 18.0f);
		duration = min(max(duration, 0.72f), 1.05f);
		SpawnHealStatusEffect(position, vec4(radius * 0.65f, 1.8f, radius * 0.65f, 0.0f), 4);
	}
	else if (type == SkillEffectType::Bomber_MeteorTrail) {
		*effect->SpriteSlot = ParticleSpriteSlot::FireSecondary;
		radius = min(max(radius, 1.8f), 2.8f);
		power = max(power, 35.0f);
		duration = min(max(duration, 0.20f), 0.30f);
	}
	else if (type == SkillEffectType::Bomber_Meteor) {
		*effect->SpriteSlot = ParticleSpriteSlot::Explosion;
		radius = min(max(radius, 24.0f), 36.0f);
		power = max(power, 90.0f);
		duration = min(max(duration, 1.10f), 1.45f);
		SpawnHealStatusEffect(position, vec4(radius * 0.22f, 2.5f, radius * 0.22f, 0.0f), 12);
	}	else if (type == SkillEffectType::DualPistol_BladeMode) {
		*effect->SpriteSlot = ParticleSpriteSlot::Electric;
		radius = min(max(radius * 0.48f, 0.65f), 0.95f);
		power = min(max(power * 0.35f, 0.35f), 0.75f);
		duration = min(max(duration, 0.18f), 0.38f);
	}
	else if (type == SkillEffectType::Blood_Hit) {
		*effect->SpriteSlot = ParticleSpriteSlot::Blood;
		radius = min(max(radius * 0.62f, 0.45f), 1.15f);
		power = min(max(power, 1.0f), 24.0f);
		duration = min(max(duration, 0.24f), 0.42f);
	}
	else if (type == SkillEffectType::Explosion_Blast) {
		*effect->SpriteSlot = ParticleSpriteSlot::Explosion;
	}
	else if (type == SkillEffectType::Aegis_ShieldEnergy) {
		*effect->SpriteSlot = ParticleSpriteSlot::Shield;
	}
	else if (type == SkillEffectType::Rifle_StimPack ||
		type == SkillEffectType::Sniper_GrappleHook ||
		type == SkillEffectType::Sniper_ModeSwitch ||
		type == SkillEffectType::DualPistol_DeathDash ||
		type == SkillEffectType::DualPistol_BladeMode ||
		type == SkillEffectType::DualPistol_Awaken) {
		*effect->SpriteSlot = ParticleSpriteSlot::Electric;
	}
	else if (type == SkillEffectType::Sniper_Railgun) {
		*effect->SpriteSlot = ParticleSpriteSlot::Beam;
	}
	else if (type == SkillEffectType::Electric_Burst) {
		*effect->SpriteSlot = ParticleSpriteSlot::Electric;
	}

	effect->Emitter = MakeParticleEmitter(position, direction, radius, power, duration, ownerId);
	effect->Active = true;
	effect->PendingReset = true;
}

void Game::NotifyPlayerDamaged(float damage)
{
	if (damage <= 0.0f) return;

	float hpRate = 1.0f;
	if (player != nullptr && player->MaxHP > 0.0f) {
		hpRate = min(1.0f, max(0.0f, player->HP / player->MaxHP));
	}

	DamageBorderAlpha = max(DamageBorderAlpha, 0.70f);
	DamageBorderFadeDuration = hpRate <= 0.53f ? 0.52f : 0.30f;

	if (damage >= 70.0f) {
		CameraShakeStrength = max(CameraShakeStrength, 0.22f);
		CameraShakeDuration = 0.34f;
		CameraShakeTimer = CameraShakeDuration;
		WhiteDamageFlashAlpha = max(WhiteDamageFlashAlpha, 0.42f);
	}
	else if (damage >= 40.0f) {
		CameraShakeStrength = max(CameraShakeStrength, 0.13f);
		CameraShakeDuration = 0.26f;
		CameraShakeTimer = CameraShakeDuration;
	}
	else if (damage >= 20.0f) {
		CameraShakeStrength = max(CameraShakeStrength, 0.065f);
		CameraShakeDuration = 0.18f;
		CameraShakeTimer = CameraShakeDuration;
	}
}

void Game::UpdateDamageFeedback(float deltaTime)
{
	const float dt = max(0.0f, deltaTime);
	if (player != nullptr && player->MaxHP > 0.0f) {
		const bool jobChanged = LastObservedPlayerJob >= 0 &&
			player->m_currentJob != LastObservedPlayerJob;
		const bool maxHPChanged = LastObservedPlayerMaxHP >= 0.0f &&
			abs(player->MaxHP - LastObservedPlayerMaxHP) > 0.01f;
		if (jobChanged || maxHPChanged) {
			// Job changes legitimately replace MaxHP and may clamp current HP. That is
			// loadout normalization, not combat damage, so reset the observation baseline.
			PlayerDamageFeedbackSuppressionTime = max(PlayerDamageFeedbackSuppressionTime, 0.18f);
		}

		if (PlayerDamageFeedbackSuppressionTime > 0.0f) {
			PlayerDamageFeedbackSuppressionTime -= dt;
			if (PlayerDamageFeedbackSuppressionTime < 0.0f) PlayerDamageFeedbackSuppressionTime = 0.0f;
		}
		else if (LastObservedPlayerHP >= 0.0f && player->HP < LastObservedPlayerHP - 0.01f) {
			const float damage = LastObservedPlayerHP - player->HP;
			player->TriggerHitFlash(1.0f);
			NotifyPlayerDamaged(damage);
			SpawnSkillEffect(SkillEffectType::Blood_Hit,
				player->worldMat.pos + vec4(0.0f, 1.0f, 0.0f, 0.0f),
				vec4(0, 1, 0, 0), (UINT)playerGameObjectIndex, 0.95f, max(damage, 1.0f), 0.36f);
		}
		LastObservedPlayerHP = player->HP;
		LastObservedPlayerMaxHP = player->MaxHP;
		LastObservedPlayerJob = player->m_currentJob;
	}
	else {
		LastObservedPlayerHP = -1.0f;
		LastObservedPlayerMaxHP = -1.0f;
		LastObservedPlayerJob = -1;
		PlayerDamageFeedbackSuppressionTime = 0.0f;
	}

	if (DamageBorderAlpha > 0.0f) {
		DamageBorderAlpha -= dt / max(DamageBorderFadeDuration, 0.05f) * 0.70f;
		if (DamageBorderAlpha < 0.0f) DamageBorderAlpha = 0.0f;
	}
	if (CameraShakeTimer > 0.0f) {
		CameraShakeTimer -= dt;
		if (CameraShakeTimer <= 0.0f) {
			CameraShakeTimer = 0.0f;
			CameraShakeDuration = 0.0f;
			CameraShakeStrength = 0.0f;
		}
	}
	if (WhiteDamageFlashAlpha > 0.0f) {
		WhiteDamageFlashAlpha -= dt * 2.8f;
		if (WhiteDamageFlashAlpha < 0.0f) WhiteDamageFlashAlpha = 0.0f;
	}
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
	const bool shouldSpawnVisual = (visualIndex < 0) || (gStatusEffectVisuals[visualIndex].Active == false);
	const float previousRemainTime = (visualIndex >= 0) ? gStatusEffectVisuals[visualIndex].RemainTime : 0.0f;

	if (active == false) {
		if (visualIndex >= 0) gStatusEffectVisuals[visualIndex].Active = false;
		RefreshMonsterStatusTint(targetObjIndex);
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
	if (shouldSpawnVisual) {
		visual.VisualPulseTimer = 0.0f;
	}
	visual.Position = position;
	visual.Extents = extents;
	RefreshMonsterStatusTint(targetObjIndex);

	float radius = max(max(extents.x, extents.z) * 2.6f, 1.35f);
	float visualDuration = max(remainTime, duration);
	if (visualDuration <= 0.0f) visualDuration = 1.0f;

	bool refreshFreezeVisual = (type == StatusEffectType::Freeze || type == StatusEffectType::Slow) &&
		shouldSpawnVisual == false && previousRemainTime <= 0.45f;
	if (shouldSpawnVisual || refreshFreezeVisual) {
		switch (type) {
		case StatusEffectType::Freeze:
		case StatusEffectType::Slow:
			SpawnIceStatusEffect(position, extents);
			break;
		case StatusEffectType::Burn:
			SpawnBurningStatusEffect(position, extents);
			break;
		case StatusEffectType::Stun:
		case StatusEffectType::Paralyze:
			SpawnParalyzeStatusEffect(position, extents);
			break;
		case StatusEffectType::Hack:
			SpawnHackStatusEffect(position, extents);
			break;
		case StatusEffectType::Heal:
			SpawnHealStatusEffect(position, extents);
			break;
		case StatusEffectType::Taunt:
			SpawnSkillEffect(SkillEffectType::Fire_Ring, position, vec4(0, 1, 0, 0), (UINT)targetObjIndex, radius, max(power, 1.0f), min(visualDuration, 1.0f));
			break;
		default:
			break;
		}
	}
}

void Game::ApplyBossState(const STC_BossState_Header& header)
{
	BossPrototypeServerSynced = true;
	BossPrototypeEnabled = header.enabled;
	if (!BossPrototypeEnabled) {
		BossPrototypeVisualGroundInitialized = false;
		return;
	}

	bool wasShieldActive = BossPrototypeShieldActive;
	std::vector<BossPrototypeCore> previousCores = BossPrototypeCores;

	BossPrototypeIndex = GetDynamicObjectNetIndex(header.zoneId, header.bossObjIndex);
	BossPrototypeHP = header.bossHP;
	BossPrototypeMaxHP = max(header.bossMaxHP, 1.0f);
	BossPrototypeCenter = header.center;
	BossPrototypeAimDirection = header.aimDirection;
	BossPrototypeRailgunDirection = header.railgunDirection;
	BossPrototypeShieldActive = header.shieldActive;
	BossPrototypeShieldDownTime = header.shieldDownTime;
	BossPrototypeGroggyTime = header.groggyTime;
	BossPrototypePhaseState = (BossPrototypePhase)header.phase;
	BossPrototypePhaseTime = header.phaseTime;
	BossPrototypePatternStep = header.patternStep;
	BossPrototypeConfigured = true;
	BossPrototypeCoresInitialized = true;
	const float visualGroundY = GetBossVisualGroundY();

	if (BossPrototypeIndex >= 0 && BossPrototypeIndex < (int)DynmaicGameObjects.size()) {
		Monster* boss = dynamic_cast<Monster*>(DynmaicGameObjects[BossPrototypeIndex]);
		if (boss != nullptr) {
			const float prevBossHP = boss->HP;
			boss->HP = header.bossHP;
			boss->MaxHP = header.bossMaxHP;
			if (boss->HP < prevBossHP - 0.01f) {
				boss->TriggerHitFlash(1.0f);
			}
			boss->SetStatusTint(vec4(0.0f, 1.0f, 1.0f, 1.0f));
			EnsureBossObjectUsesBossModel(boss);

			vec4 bossDirection = BossPrototypeVisualLookDirection;
			if (BossPrototypeShouldAimBody(BossPrototypePhaseState)) {
				bossDirection = GetBossPrototypePatternDirection(BossPrototypePhaseState, BossPrototypeAimDirection);
				bossDirection.y = 0.0f;
				bossDirection = NormalizeOrFallback(bossDirection, vec4(0, 0, 1, 0));
				bossDirection = GetBossPrototypeVisualLookDirection(bossDirection);
				BossPrototypeVisualLookDirection = ApproachDirection(BossPrototypeVisualLookDirection, bossDirection, 3.2f, DeltaTime);
			}
			bossDirection = BossPrototypeVisualLookDirection;

			const float BossScale = BossPrototypeGroggyTime > 0.0f ? kBossPrototypeGroggyScale : kBossPrototypeNormalScale;
			matrix bossWorld;
			bossWorld.LookAt(bossDirection);
			bossWorld.right *= BossScale;
			bossWorld.up *= BossScale;
			bossWorld.look *= BossScale;
			bossWorld.pos = BossPrototypeCenter;
			bossWorld.pos.y = visualGroundY;
			if (BossPrototypeBossModel != nullptr) {
				bossWorld.pos -= bossWorld.right * BossPrototypeBossModel->OBB_Tr.x;
				bossWorld.pos -= bossWorld.up * BossPrototypeBossModel->AABB[0].y;
				bossWorld.pos -= bossWorld.look * BossPrototypeBossModel->OBB_Tr.z;
				bossWorld.pos.y += 1.45f + kBossPrototypeHeightOffset;
			}
			bossWorld.pos.w = 1.0f;
			ApplyBossWorldAndRefreshChunks(boss, bossWorld);
			XMMatrixDecompose((XMVECTOR*)&boss->DestScale, (XMVECTOR*)&boss->DestRot, (XMVECTOR*)&boss->DestPos, bossWorld);
			boss->LVelocity = vec4(0, 0, 0, 0);
			ApplyBossPrototypeTurretPose(boss);
			if (gd.isRaytracingRender && boss->RaytracingWorldMatInput_Model != nullptr) {
				boss->RaytracingUpdateTransform();
				boss->SetRaytracingInstanceEnabled(true);
			}
		}
		else {
			static int s_lastBossNonMonsterIndex = -1;
			if (s_lastBossNonMonsterIndex != BossPrototypeIndex) {
				s_lastBossNonMonsterIndex = BossPrototypeIndex;
				char dbg[256] = {};
				sprintf_s(dbg, "[BossProto] boss object is not Monster zone=%d serverIdx=%d clientIdx=%d dynSize=%zu\n",
					header.zoneId, header.bossObjIndex, BossPrototypeIndex, DynmaicGameObjects.size());
				OutputDebugStringA(dbg);
				printf("%s", dbg);
				fflush(stdout);
			}
		}
	}
	else {
		static int s_lastMissingBossServerIndex = -1;
		static int s_lastMissingBossZone = -1;
		if (s_lastMissingBossServerIndex != header.bossObjIndex || s_lastMissingBossZone != header.zoneId) {
			s_lastMissingBossServerIndex = header.bossObjIndex;
			s_lastMissingBossZone = header.zoneId;
			char dbg[256] = {};
			sprintf_s(dbg, "[BossProto] boss sync missing zone=%d serverIdx=%d clientIdx=%d dynSize=%zu\n",
				header.zoneId, header.bossObjIndex, BossPrototypeIndex, DynmaicGameObjects.size());
			OutputDebugStringA(dbg);
			printf("%s", dbg);
			fflush(stdout);
		}
	}

	BossPrototypeCores.clear();
	for (int i = 0; i < min(header.coreCount, 3); ++i) {
		BossPrototypeCore core;
		core.Position = header.cores[i].position;
		core.Position.y = visualGroundY;
		core.HP = header.cores[i].hp;
		core.MaxHP = header.cores[i].maxHP;
		core.Active = header.cores[i].active;

		if (i < (int)previousCores.size()) {
			BossPrototypeCore& prev = previousCores[i];
			core.BossProtoTypeCoreObj = prev.BossProtoTypeCoreObj;
			prev.BossProtoTypeCoreObj = nullptr;
			core.HitFlashTimer = prev.HitFlashTimer;
			core.HitFlashDuration = prev.HitFlashDuration;
			if (core.HP < prev.HP) {
				core.HitFlashDuration = 0.55f;
				core.HitFlashTimer = core.HitFlashDuration;
				SpawnFloatingDamageText(core.Position + vec4(0.0f, 1.05f, 0.0f, 0.0f), prev.HP - core.HP);
				SpawnSkillEffect(SkillEffectType::Electric_Burst, core.Position + vec4(0.0f, 0.8f, 0.0f, 0.0f),
					vec4(0, 1, 0, 0), 0, 1.6f, max(prev.HP - core.HP, 1.0f), 0.7f);
			}
			if (prev.Active && !core.Active) {
				SpawnSkillEffect(SkillEffectType::Explosion_Blast, core.Position + vec4(0.0f, 0.7f, 0.0f, 0.0f),
					vec4(0, 1, 0, 0), 0, 2.0f, 24.0f, 0.8f);
				SpawnBossExplosionParticles(core.Position, 2.0f, 32.0f);
			}
		}
		if (core.BossProtoTypeCoreObj == nullptr) {
			core.BossProtoTypeCoreObj = new GameObject();
			core.BossProtoTypeCoreObj->SetShape(BossPrototypeCoreModel, header.zoneId);
		}
		core.BossProtoTypeCoreObj->SetRaytracingInstanceEnabled(core.Active);
		BossPrototypeCores.push_back(core);
	}
	for (BossPrototypeCore& oldCore : previousCores) {
		ReleaseBossPrototypeCoreObject(oldCore);
	}

	if (wasShieldActive && !BossPrototypeShieldActive) {
		SpawnSkillEffect(SkillEffectType::Aegis_ShieldEnergy, BossPrototypeCenter + vec4(0.0f, 2.0f, 0.0f, 0.0f),
			vec4(0, 1, 0, 0), 0, 5.0f, 30.0f, 1.2f);
	}

	BossAoEWarnings.clear();
	for (int i = 0; i < min(header.warningCount, BossSyncWarningCapacity); ++i) {
		BossAoEWarning warning;
		if (header.warnings[i].shape == 2) {
			warning.Shape = BossAoEShape::SafeCircle;
		}
		else {
			warning.Shape = header.warnings[i].shape == 1 ? BossAoEShape::Rectangle : BossAoEShape::Circle;
		}
		warning.Position = header.warnings[i].position;
		warning.Direction = header.warnings[i].direction;
		warning.Position.y = visualGroundY;
		warning.Radius = header.warnings[i].radius;
		warning.Width = header.warnings[i].width;
		warning.Length = header.warnings[i].length;
		warning.WarningTime = header.warnings[i].warningTime;
		warning.Age = header.warnings[i].age;
		warning.Damage = header.warnings[i].damage;
		warning.InnerDamage = header.warnings[i].innerDamage;
		warning.FollowTime = header.warnings[i].followTime;
		warning.LockTime = header.warnings[i].lockTime;
		warning.Active = header.warnings[i].active;
		warning.FollowPlayer = false;
		warning.DarkenOnLock = header.warnings[i].darkenOnLock;
		warning.VisualSpawned = header.warnings[i].visualSpawned;
		BossAoEWarnings.push_back(warning);

		if (warning.Shape == BossAoEShape::Circle && warning.Age >= warning.FollowTime && warning.Age < warning.WarningTime) {
			bool alreadySpawned = false;
			for (const BossMissileVisual& visual : BossMissileVisuals) {
				vec4 delta = visual.Target - warning.Position;
				delta.y = 0.0f;
				if (delta.fast_square_of_len3 < 0.35f * 0.35f) {
					alreadySpawned = true;
					break;
				}
			}
			if (!alreadySpawned) {
				BossMissileVisual visual;
				visual.Target = warning.Position;
				visual.Target.y = warning.Position.y + 0.25f;
				visual.Target.w = 1.0f;
				visual.Start = visual.Target + vec4(RandomRange(-1.4f, 1.4f), 18.0f, RandomRange(-1.4f, 1.4f), 0.0f);
				visual.Start.w = 1.0f;
				visual.Duration = max(warning.WarningTime - warning.Age, 0.55f);
				visual.Radius = max(warning.Radius, 0.8f);
				visual.Active = true;
				BossMissileVisuals.push_back(visual);
			}
		}
	}
}

void Game::UpdateBossPrototype(float deltaTime)
{
	if (currentZoneId < 100) {
		BossPrototypeEnabled = false;
		return;
	}
	if (!BossPrototypeEnabled || player == nullptr) return;

	const float dt = max(0.0f, deltaTime);
	Monster* syncedBoss = nullptr;
	if (BossPrototypeIndex >= 0 && BossPrototypeIndex < (int)DynmaicGameObjects.size()) {
		syncedBoss = dynamic_cast<Monster*>(DynmaicGameObjects[BossPrototypeIndex]);
		if (syncedBoss != nullptr && (syncedBoss->isDead || syncedBoss->tag[GameObjectTag::Tag_Enable] == false)) {
			syncedBoss = nullptr;
		}
		EnsureBossObjectUsesBossModel(syncedBoss);
	}
	auto PointInRectangleWarning = [](const BossAoEWarning& warning, vec4 point) {
		vec4 forward = NormalizeOrFallback(warning.Direction, vec4(0, 0, 1, 0));
		vec4 right = NormalizeOrFallback(vec4(forward.z, 0, -forward.x, 0), vec4(1, 0, 0, 0));
		vec4 delta = point - warning.Position;
		float along = delta.x * forward.x + delta.z * forward.z;
		float side = delta.x * right.x + delta.z * right.z;
		return along >= 0.0f && along <= warning.Length && fabsf(side) <= warning.Width * 0.5f;
	};
	auto SpawnBossMissileVisual = [&](vec4 target, float duration, float radius) {
		BossMissileVisual visual;
		visual.Target = target;
		visual.Target.y = target.y + 0.25f;
		visual.Target.w = 1.0f;
		visual.Start = visual.Target + vec4(RandomRange(-1.4f, 1.4f), 18.0f, RandomRange(-1.4f, 1.4f), 0.0f);
		visual.Start.w = 1.0f;
		visual.Duration = max(duration, 0.18f);
		visual.Radius = max(radius, 0.8f);
		visual.Active = true;
		BossMissileVisuals.push_back(visual);
	};

	if (BossPrototypeServerSynced) {
		for (BossMissileVisual& visual : BossMissileVisuals) {
			if (!visual.Active) continue;
			visual.Age += dt;
			if (visual.Age >= visual.Duration + 0.08f) {
				SpawnBossExplosionParticles(visual.Target, visual.Radius, 35.0f);
				visual.Active = false;
			}
		}
		BossMissileVisuals.erase(std::remove_if(BossMissileVisuals.begin(), BossMissileVisuals.end(),
			[](const BossMissileVisual& visual) {
				return !visual.Active;
			}), BossMissileVisuals.end());
		for (BossAoEWarning& warning : BossAoEWarnings) {
			if (!warning.Active) continue;
			warning.Age += dt;
			if (warning.Shape == BossAoEShape::Circle && warning.Age >= warning.FollowTime && warning.Age < warning.WarningTime) {
				bool alreadySpawned = false;
				for (const BossMissileVisual& visual : BossMissileVisuals) {
					vec4 delta = visual.Target - warning.Position;
					delta.y = 0.0f;
					if (delta.fast_square_of_len3 < 0.35f * 0.35f) {
						alreadySpawned = true;
						break;
					}
				}
				if (!alreadySpawned) {
					SpawnBossMissileVisual(warning.Position, max(warning.WarningTime - warning.Age, 0.55f), warning.Radius);
				}
			}
		}
		return;
	}

	for (BossMissileVisual& visual : BossMissileVisuals) {
		if (!visual.Active) continue;
		visual.Age += dt;
		if (visual.Age >= visual.Duration + 0.08f) {
			visual.Active = false;
		}
	}
	BossMissileVisuals.erase(std::remove_if(BossMissileVisuals.begin(), BossMissileVisuals.end(),
		[](const BossMissileVisual& visual) {
			return !visual.Active;
		}), BossMissileVisuals.end());

	for (BossAoEWarning& warning : BossAoEWarnings) {
		if (!warning.Active) continue;
		warning.Age += dt;
		if (warning.FollowPlayer && warning.Age < warning.FollowTime) {
			warning.Position.x = player->worldMat.pos.x;
			warning.Position.z = player->worldMat.pos.z;
			warning.Position.y = GetBossVisualGroundY();
			warning.Position.w = 1.0f;
		}
		if (warning.Shape == BossAoEShape::Circle && !warning.VisualSpawned && warning.Age >= warning.FollowTime) {
			SpawnBossMissileVisual(warning.Position, max(warning.WarningTime - warning.Age, 0.25f), warning.Radius);
			warning.VisualSpawned = true;
		}
		if (warning.Age < warning.WarningTime || warning.DamageApplied) continue;

		vec4 playerPos = player->worldMat.pos;
		bool hit = false;
		float damage = warning.Damage;
		if (warning.Shape == BossAoEShape::Circle) {
			vec4 delta = playerPos - warning.Position;
			delta.y = 0.0f;
			hit = delta.fast_square_of_len3 <= warning.Radius * warning.Radius;
			if (hit && warning.InnerDamage > 0.0f && delta.fast_square_of_len3 <= warning.Radius * warning.Radius * 0.35f) {
				damage = warning.InnerDamage;
			}
		}
		else {
			hit = PointInRectangleWarning(warning, playerPos);
		}

		if (hit) {
			player->HP = max(0.0f, player->HP - damage);
			NotifyPlayerDamaged(damage);
			player->TriggerHitFlash(1.0f);
			SpawnFloatingDamageText(playerPos, damage);
			SpawnSkillEffect(SkillEffectType::Blood_Hit, playerPos + vec4(0.0f, 1.0f, 0.0f, 0.0f),
				vec4(0, 1, 0, 0), (UINT)playerGameObjectIndex, 1.3f, damage, 0.45f);
		}

		warning.DamageApplied = true;
		warning.Active = false;
		SpawnBossExplosionParticles(warning.Position, max(warning.Radius, warning.Width), 35.0f);
	}

	BossAoEWarnings.erase(std::remove_if(BossAoEWarnings.begin(), BossAoEWarnings.end(),
		[](const BossAoEWarning& warning) {
			return !warning.Active && warning.Age > warning.WarningTime + 0.25f;
		}), BossAoEWarnings.end());

	Monster* boss = syncedBoss;

	if (boss == nullptr) {
		BossPrototypeIndex = -1;
		for (int i = 0; i < (int)DynmaicGameObjects.size(); ++i) {
			Monster* candidate = dynamic_cast<Monster*>(DynmaicGameObjects[i]);
			if (candidate == nullptr || candidate->isDead || candidate->tag[GameObjectTag::Tag_Enable] == false) continue;
			BossPrototypeIndex = i;
			boss = candidate;
			BossPrototypePhaseState = BossPrototypePhase::FindBoss;
			BossPrototypePhaseTime = 0.0f;
			BossPrototypePatternCooldown = 0.0f;
			BossPrototypeConfigured = false;
			BossPrototypeCoresInitialized = false;
			OutputDebugStringA("[BossProto] selected first live monster as turret boss prototype\n");
			break;
		}
		if (boss == nullptr) return;
	}

	if (!BossPrototypeConfigured) {
		BossPrototypeCenter = boss->worldMat.pos;
		BossPrototypeCenter.w = 1.0f;
		boss->MaxHP = 7500.0f;
		boss->HP = 7500.0f;
		EnsureBossObjectUsesBossModel(boss);
		BossPrototypeConfigured = true;
	}

	if (!BossPrototypeCoresInitialized || (!BossPrototypeShieldActive && BossPrototypeShieldDownTime <= 0.0f)) {
		for (BossPrototypeCore& oldCore : BossPrototypeCores) {
			ReleaseBossPrototypeCoreObject(oldCore);
		}
		BossPrototypeCores.clear();
		BossAoEWarnings.clear();
		const float coreRadius = 20.0f;
		const float coreGroundY = GetBossVisualGroundY();
		for (int i = 0; i < 3; ++i) {
			float angle = XM_2PI * (float)i / 3.0f + XM_PIDIV2;
			BossPrototypeCore core;
			core.Position = BossPrototypeCenter + vec4(cosf(angle) * coreRadius, 0.0f, sinf(angle) * coreRadius, 0.0f);
			core.Position.y = coreGroundY;
			core.Position.w = 1.0f;
			core.HP = 1200.0f;
			core.MaxHP = 1200.0f;
			core.Active = true;
			BossPrototypeCores.push_back(core);
		}
		BossPrototypeShieldActive = true;
		BossPrototypeCoresInitialized = true;
		BossPrototypeShieldDownTime = 0.0f;
		BossPrototypeGroggyTime = 0.0f;
		BossPrototypePhaseState = BossPrototypePhase::Rest;
		BossPrototypePhaseTime = 0.0f;
		BossPrototypePatternCooldown = 0.8f;
	}

	if (!BossPrototypeShieldActive) {
		bool wasGroggy = BossPrototypeGroggyTime > 0.0f;
		BossPrototypeShieldDownTime -= dt;
		BossPrototypeGroggyTime = max(0.0f, BossPrototypeGroggyTime - dt);
		if (wasGroggy && BossPrototypeGroggyTime <= 0.0f) {
			BossAoEWarnings.clear();
			BossPrototypePhaseState = BossPrototypePhase::Rest;
			BossPrototypePhaseTime = 0.0f;
			BossPrototypePatternCooldown = 0.6f;
		}
	}

	BossPrototypeAimDirection = NormalizeOrFallback(player->worldMat.pos - BossPrototypeCenter, vec4(0, 0, 1, 0));
	BossPrototypeAimDirection.y = 0.0f;
	BossPrototypeAimDirection = NormalizeOrFallback(BossPrototypeAimDirection, vec4(0, 0, 1, 0));

	vec4 bossFacingDirection = BossPrototypeVisualLookDirection;
	if (BossPrototypeShouldAimBody(BossPrototypePhaseState)) {
		bossFacingDirection = GetBossPrototypePatternDirection(BossPrototypePhaseState, BossPrototypeAimDirection);
		bossFacingDirection.y = 0.0f;
		bossFacingDirection = NormalizeOrFallback(bossFacingDirection, BossPrototypeAimDirection);
		bossFacingDirection = GetBossPrototypeVisualLookDirection(bossFacingDirection);
		BossPrototypeVisualLookDirection = ApproachDirection(BossPrototypeVisualLookDirection, bossFacingDirection, 3.2f, DeltaTime);
	}
	bossFacingDirection = BossPrototypeVisualLookDirection;

	const float BossScale = BossPrototypeGroggyTime > 0.0f ? kBossPrototypeGroggyScale : kBossPrototypeNormalScale;
	matrix bossWorld;
	bossWorld.LookAt(bossFacingDirection);
	bossWorld.right *= BossScale;
	bossWorld.up *= BossScale;
	bossWorld.look *= BossScale;
	bossWorld.pos = BossPrototypeCenter;
	bossWorld.pos.y = GetBossVisualGroundY();
	if (BossPrototypeBossModel != nullptr) {
		bossWorld.pos -= bossWorld.right * BossPrototypeBossModel->OBB_Tr.x;
		bossWorld.pos -= bossWorld.up * BossPrototypeBossModel->AABB[0].y;
		bossWorld.pos -= bossWorld.look * BossPrototypeBossModel->OBB_Tr.z;
		bossWorld.pos.y += 1.45f + kBossPrototypeHeightOffset;
	}
	bossWorld.pos.w = 1.0f;
	boss->worldMat = bossWorld;
	XMMatrixDecompose((XMVECTOR*)&boss->DestScale, (XMVECTOR*)&boss->DestRot, (XMVECTOR*)&boss->DestPos, bossWorld);
	boss->LVelocity = vec4(0, 0, 0, 0);
	boss->ChangeState(Monster::State::IDLE);
	ApplyBossPrototypeTurretPose(boss);

	BossPrototypePhaseTime += dt;
	switch (BossPrototypePhaseState) {
	case BossPrototypePhase::FindBoss:
		BossPrototypePhaseState = BossPrototypePhase::Rest;
		BossPrototypePhaseTime = 0.0f;
		BossPrototypePatternCooldown = 1.0f;
		break;
	case BossPrototypePhase::MissileLock:
		if (BossPrototypePhaseTime <= dt + 0.001f) {
			BossAoEWarning warning;
			warning.Shape = BossAoEShape::Circle;
			warning.Position = player->worldMat.pos;
			warning.Position.y = GetBossVisualGroundY();
			warning.Position.w = 1.0f;
			warning.Radius = 2.4f;
			warning.WarningTime = 5.0f;
			warning.FollowTime = 4.0f;
			warning.LockTime = 1.0f;
			warning.Damage = 30.0f;
			warning.InnerDamage = 55.0f;
			warning.FollowPlayer = true;
			warning.DarkenOnLock = true;
			BossAoEWarnings.push_back(warning);
			SpawnSkillEffect(SkillEffectType::Rifle_AirStrikeTrail, BossPrototypeCenter + vec4(0.0f, 3.0f, 0.0f, 0.0f),
				BossPrototypeAimDirection, (UINT)BossPrototypeIndex, 1.4f, 18.0f, 0.8f);
		}
		if (BossPrototypePhaseTime >= 5.25f) {
			BossPrototypePhaseState = BossPrototypePhase::Rest;
			BossPrototypePhaseTime = 0.0f;
			BossPrototypePatternCooldown = 2.0f;
		}
		break;
	case BossPrototypePhase::RailgunCharge:
		if (BossPrototypePhaseTime <= dt + 0.001f) {
			BossPrototypeRailgunDirection = BossPrototypeAimDirection;
			BossAoEWarning warning;
			warning.Shape = BossAoEShape::Rectangle;
			warning.Position = BossPrototypeCenter;
			warning.Position.y = GetBossVisualGroundY();
			warning.Direction = BossPrototypeRailgunDirection;
			warning.Width = 2.3f;
			warning.Length = 36.0f;
			warning.Radius = 2.0f;
			warning.WarningTime = 2.8f;
			warning.Damage = 95.0f;
			BossAoEWarnings.push_back(warning);
			SpawnSkillEffect(SkillEffectType::Sniper_Railgun, BossPrototypeCenter + BossPrototypeRailgunDirection * 2.0f + vec4(0.0f, 2.0f, 0.0f, 0.0f),
				BossPrototypeRailgunDirection, (UINT)BossPrototypeIndex, 2.0f, 30.0f, 2.6f);
		}
		if (BossPrototypePhaseTime >= 4.75f) {
			BossPrototypePhaseState = BossPrototypePhase::Rest;
			BossPrototypePhaseTime = 0.0f;
			BossPrototypePatternCooldown = 2.4f;
		}
		break;
	case BossPrototypePhase::Bombardment:
		if (BossPrototypePhaseTime <= dt + 0.001f) {
			const int bombCount = 3;
			for (int i = 0; i < bombCount; ++i) {
				float angle = XM_2PI * (float)i / (float)bombCount;
				float dist = 3.5f + (float)(i % 2) * 2.2f;
				BossAoEWarning warning;
				warning.Shape = BossAoEShape::Circle;
				warning.Position = player->worldMat.pos + vec4(cosf(angle) * dist, 0.0f, sinf(angle) * dist, 0.0f);
				warning.Position.y = GetBossVisualGroundY();
				warning.Position.w = 1.0f;
				warning.Radius = 2.1f;
				warning.WarningTime = 1.8f;
				warning.Damage = 35.0f;
				BossAoEWarnings.push_back(warning);
			}
			SpawnSkillEffect(SkillEffectType::Rifle_AirStrikeTrail, BossPrototypeCenter + vec4(0.0f, 3.0f, 0.0f, 0.0f),
				vec4(0, 1, 0, 0), (UINT)BossPrototypeIndex, 2.2f, 18.0f, 1.0f);
		}
		if (BossPrototypePhaseTime >= 2.1f) {
			BossPrototypePhaseState = BossPrototypePhase::Rest;
			BossPrototypePhaseTime = 0.0f;
			BossPrototypePatternCooldown = 2.8f;
		}
		break;
	case BossPrototypePhase::Rest:
		if (BossPrototypePhaseTime >= BossPrototypePatternCooldown) {
			int step = BossPrototypePatternStep++ % 3;
			if (step == 0) BossPrototypePhaseState = BossPrototypePhase::MissileLock;
			else if (step == 1) BossPrototypePhaseState = BossPrototypePhase::RailgunCharge;
			else BossPrototypePhaseState = BossPrototypePhase::Bombardment;
			BossPrototypePhaseTime = 0.0f;
		}
		break;
	default:
		break;
	}
}
void Game::RenderBossPrototypeObjects()
{
	if (!BossPrototypeEnabled || BossPrototypeCoreModel == nullptr || BossPrototypeCores.empty() || Current_Zone == nullptr) return;

	matrix view = gd.viewportArr[0].ViewMatrix * gd.viewportArr[0].ProjectMatrix;
	view.transpose();

	if (gd.isRaytracingRender) {
		for (const BossPrototypeCore& core : BossPrototypeCores) {
			if (!core.Active) continue;
			if (core.BossProtoTypeCoreObj == nullptr) continue;
			matrix world;
			world.Id();
			world.right *= 1.65f;
			world.up *= 1.65f;
			world.look *= 1.65f;
			world.pos = core.Position;
			if (BossPrototypeCoreModel != nullptr) {
				world.pos -= world.up * BossPrototypeCoreModel->AABB[0].y;
			}
			world.pos.y += 0.16f;
			world.pos.w = 1.0f;
			float hitRate = core.HitFlashDuration > 0.0f ? min(1.0f, max(0.0f, core.HitFlashTimer / core.HitFlashDuration)) : 0.0f;
			ModelRenderTintOverrideActive = true;
			ModelRenderTintOverride = vec4(hitRate * 1.35f, 1.0f, 0.0f, 0.0f);
			//BossPrototypeCoreModel->Render<false>(gd.gpucmd, world, nullptr);
			core.BossProtoTypeCoreObj->worldMat = world;
			core.BossProtoTypeCoreObj->RaytracingUpdateTransform();
			core.BossProtoTypeCoreObj->SetRaytracingHitFlashRate(hitRate);
			ModelRenderTintOverrideActive = false;
		}
	}
	else {
		gd.gpucmd.SetShader(MyPBRShader1, ShaderType::RenderWithShadow);
		game.PresentShaderType = ShaderType::RenderWithShadow;
		gd.gpucmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
		{
			using PRID = PBRShader1::RootParamId;
			gd.gpucmd->SetGraphicsRoot32BitConstants(PRID::Const_Camera, 16, &view, 0);
			gd.gpucmd->SetGraphicsRoot32BitConstants(PRID::Const_Camera, 4, &gd.viewportArr[0].Camera_Pos, 16);
			gd.gpucmd->SetGraphicsRootConstantBufferView(PRID::CBV_StaticLight, game.LightCB_withShadowResource[game.Current_Zone->Asset_OffsetMul].resource->GetGPUVirtualAddress());
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_ShadowMap, game.MyDirLight[0].descindex.hRender.hgpu);
			gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_EnvionmentMap, game.MySkyBoxShader->CurrentSkyBox.descindex.hRender.hgpu);
			if (game.Current_Zone->bReqireBakeLight_Raster == false) {
				gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_Chunck_StaticLightStructuredBuffer, game.Current_Zone->Immortal_ZoneLightBuffer_SRV.hRender.hgpu);
			}
		}

		for (const BossPrototypeCore& core : BossPrototypeCores) {
			if (!core.Active) continue;
			matrix world;
			world.Id();
			world.right *= 1.65f;
			world.up *= 1.65f;
			world.look *= 1.65f;
			world.pos = core.Position;
			if (BossPrototypeCoreModel != nullptr) {
				world.pos -= world.up * BossPrototypeCoreModel->AABB[0].y;
			}
			world.pos.y += 0.16f;
			world.pos.w = 1.0f;
			float hitRate = core.HitFlashDuration > 0.0f ? min(1.0f, max(0.0f, core.HitFlashTimer / core.HitFlashDuration)) : 0.0f;
			ModelRenderTintOverrideActive = true;
			ModelRenderTintOverride = vec4(hitRate * 1.35f, 1.0f, 0.0f, 0.0f);
			BossPrototypeCoreModel->Render<false>(gd.gpucmd, world, nullptr);
			ModelRenderTintOverrideActive = false;
		}
	}
}

void Game::RenderBossPrototypeAoEs()
{
	if (!BossPrototypeEnabled || BossAoEWarnings.empty() || BossPrototypeCircleMesh == nullptr ||
		BossPrototypeCircleOutlineMesh == nullptr || BossPrototypeSafeCircleMesh == nullptr ||
		BossPrototypeSafeCircleOutlineMesh == nullptr || BossPrototypeRectMesh == nullptr) return;

	using OCSRP = OnlyColorShader::RootParamId;
	gd.gpucmd.SetShader(MyOnlyColorShader);
	matrix view = gd.viewportArr[0].ViewMatrix * gd.viewportArr[0].ProjectMatrix;
	view.transpose();
	gd.gpucmd->SetGraphicsRoot32BitConstants(OCSRP::Const_Camera, 16, &view, 0);

	for (const BossAoEWarning& warning : BossAoEWarnings) {
		if (!warning.Active) continue;
		if (warning.Shape == BossAoEShape::Rectangle && warning.Length >= 38.0f && warning.Age >= warning.WarningTime) continue;

		matrix world;
		world.Id();
		if (warning.Shape == BossAoEShape::Circle || warning.Shape == BossAoEShape::SafeCircle) {
			float pulse = 1.0f + 0.10f * sinf(warning.Age * 18.0f);
			Mesh* fillMesh = warning.Shape == BossAoEShape::SafeCircle ? BossPrototypeSafeCircleMesh : BossPrototypeCircleMesh;
			Mesh* outlineMesh = warning.Shape == BossAoEShape::SafeCircle ? BossPrototypeSafeCircleOutlineMesh : BossPrototypeCircleOutlineMesh;
			world = XMMatrixScaling(warning.Radius * pulse, 0.035f, warning.Radius * pulse);
			world.pos = warning.Position + vec4(0.0f, 0.255f, 0.0f, 0.0f);
			world.pos.w = 1.0f;
			world.transpose();
			gd.gpucmd->SetGraphicsRoot32BitConstants(OCSRP::Const_Transform, 16, &world, 0);
			fillMesh->Render(gd.gpucmd, 1, 0);

			matrix ringWorld = XMMatrixScaling(warning.Radius * 1.02f * pulse, 1.0f, warning.Radius * 1.02f * pulse);
			ringWorld.pos = warning.Position + vec4(0.0f, 0.270f, 0.0f, 0.0f);
			ringWorld.pos.w = 1.0f;
			ringWorld.transpose();
			gd.gpucmd->SetGraphicsRoot32BitConstants(OCSRP::Const_Transform, 16, &ringWorld, 0);
			outlineMesh->Render(gd.gpucmd, 1, 0);
		}
		else {
			vec4 forward = NormalizeOrFallback(warning.Direction, vec4(0, 0, 1, 0));
			world.LookAt(forward);
			world.right *= warning.Width * 0.5f;
			world.up *= 0.035f;
			world.look *= warning.Length * 0.5f;
			world.pos = warning.Position + forward * (warning.Length * 0.5f) + vec4(0.0f, 0.255f, 0.0f, 0.0f);
			world.pos.w = 1.0f;
			world.transpose();
			gd.gpucmd->SetGraphicsRoot32BitConstants(OCSRP::Const_Transform, 16, &world, 0);
			BossPrototypeRectMesh->Render(gd.gpucmd, 1, 0);
		}
	}
}

void Game::RenderBossPrototypeShield()
{
	if (!BossPrototypeEnabled || !BossPrototypeShieldActive || MyWorldTextureShader == nullptr || BossPrototypeShieldMesh == nullptr) return;

	GPUResource* shieldTexture = GetBossShieldTextureResource();
	if (shieldTexture == nullptr || shieldTexture->resource == nullptr) return;

	matrix view = gd.viewportArr[0].ViewMatrix * gd.viewportArr[0].ProjectMatrix;
	view.transpose();

	const float pulse = 0.5f + 0.5f * sinf(BossPrototypePhaseTime * 2.8f);
	const float shieldRadius = kBossPrototypeShieldVisualRadius + 0.14f * pulse;
	matrix world = XMMatrixRotationY(BossPrototypePhaseTime * 0.35f) * XMMatrixScaling(shieldRadius, shieldRadius, shieldRadius);
	world.pos = BossPrototypeCenter;
	world.pos.y = GetBossVisualGroundY() + 1.55f;
	world.pos.w = 1.0f;
	world.transpose();

	vec4 tint = vec4(0.48f, 1.35f, 2.25f, 0.24f + 0.06f * pulse);

	using WTSRP = WorldTextureShader::RootParamId;
	gd.gpucmd.SetShader(MyWorldTextureShader);
	gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Camera, 16, &view, 0);
	gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Transform, 16, &world, 0);
	gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Tint, 4, &tint, 0);
	vec4 uvAnim = vec4(1.0f, 1.0f, 0.0f, 0.0f);
	gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_UVAnim, 4, &uvAnim, 0);
	MyWorldTextureShader->SetTextureCommand(shieldTexture);
	if (gd.isRaytracingRender) {
		vec4 depthInfo = gd.viewportArr[0].Camera_Pos;
		depthInfo.w = 1.0f;
		gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_DepthInfo, 4, &depthInfo, 0);
	}
	BossPrototypeShieldMesh->Render(gd.gpucmd, 1);
}

void Game::RenderAegisShieldVisuals()
{
	if (MyWorldTextureShader == nullptr || BossPrototypeShieldMesh == nullptr) return;

	GPUResource* shieldTexture = GetBossShieldTextureResource();
	if (shieldTexture == nullptr || shieldTexture->resource == nullptr) return;

	bool hasActive = false;
	for (AegisShieldVisual& visual : gAegisShieldVisuals) {
		if (!visual.Active) continue;
		visual.Age += max(0.0f, DeltaTime);
		if (visual.Age > visual.Duration) {
			visual.Active = false;
			continue;
		}
		hasActive = true;
	}
	if (!hasActive) return;
	static float sAegisShieldVisualTime = 0.0f;
	sAegisShieldVisualTime += max(0.0f, DeltaTime);

	matrix view = gd.viewportArr[0].ViewMatrix * gd.viewportArr[0].ProjectMatrix;
	view.transpose();

	using WTSRP = WorldTextureShader::RootParamId;
	gd.gpucmd.SetShader(MyWorldTextureShader);
	gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Camera, 16, &view, 0);
	MyWorldTextureShader->SetTextureCommand(shieldTexture);
	if (gd.isRaytracingRender) {
		vec4 depthInfo = gd.viewportArr[0].Camera_Pos;
		depthInfo.w = 1.0f;
		gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_DepthInfo, 4, &depthInfo, 0);
	}

	for (AegisShieldVisual& visual : gAegisShieldVisuals) {
		if (!visual.Active) continue;

		vec4 center = visual.Position;
		if (visual.OwnerId < DynmaicGameObjects.size() && DynmaicGameObjects[visual.OwnerId] != nullptr) {
			center = DynmaicGameObjects[visual.OwnerId]->worldMat.pos + (visual.GroundDisk ? vec4(0.0f, 0.08f, 0.0f, 0.0f) : vec4(0.0f, 1.05f, 0.0f, 0.0f));
		}

		float normalizedAge = visual.Age / max(visual.Duration, 0.001f);
		float pulse = 0.5f + 0.5f * sinf((sAegisShieldVisualTime + normalizedAge) * 8.0f);
		float radius = visual.Radius * (1.0f + 0.035f * pulse);

		matrix world = XMMatrixRotationY(sAegisShieldVisualTime * 0.45f) *
			(visual.GroundDisk ? XMMatrixScaling(radius, 0.035f, radius) : XMMatrixScaling(radius, radius, radius));
		world.pos = center;
		world.pos.w = 1.0f;
		world.transpose();

		vec4 tint = visual.GroundDisk
			? vec4(0.28f, 0.92f, 1.95f, 0.13f + 0.025f * pulse)
			: vec4(0.42f, 1.22f, 2.15f, 0.11f + 0.025f * pulse);
		vec4 uvAnim = vec4(1.0f, 1.0f, sAegisShieldVisualTime * 0.04f, sAegisShieldVisualTime * 0.015f);
		gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Transform, 16, &world, 0);
		gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Tint, 4, &tint, 0);
		gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_UVAnim, 4, &uvAnim, 0);
		BossPrototypeShieldMesh->Render(gd.gpucmd, 1);
	}
}

void Game::RenderFrostBlizzardSnowWaves()
{
	if (MyWorldTextureShader == nullptr || BossPrototypeShieldMesh == nullptr || gFrostSnowWaveVisuals.empty()) return;
	GPUResource* snowTexture = (gParticleSnowTexture.resource != nullptr) ? &gParticleSnowTexture : &gParticleIceTexture;
	if (snowTexture == nullptr || snowTexture->resource == nullptr || gParticleHealTexture.resource == nullptr) return;

	bool hasActive = false;
	for (FrostSnowWaveVisual& visual : gFrostSnowWaveVisuals) {
		if (!visual.Active) continue;
		visual.Age += max(0.0f, DeltaTime);
		if (visual.Age > visual.Duration) {
			visual.Active = false;
			continue;
		}
		hasActive = true;
	}
	gFrostSnowWaveVisuals.erase(std::remove_if(gFrostSnowWaveVisuals.begin(), gFrostSnowWaveVisuals.end(),
		[](const FrostSnowWaveVisual& visual) {
			return !visual.Active;
		}), gFrostSnowWaveVisuals.end());
	if (!hasActive) return;

	static float sFrostSnowWaveTime = 0.0f;
	sFrostSnowWaveTime += max(0.0f, DeltaTime);

	matrix view = gd.viewportArr[0].ViewMatrix * gd.viewportArr[0].ProjectMatrix;
	view.transpose();

	using WTSRP = WorldTextureShader::RootParamId;
	gd.gpucmd.SetShader(MyWorldTextureShader);
	gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Camera, 16, &view, 0);

	for (const FrostSnowWaveVisual& visual : gFrostSnowWaveVisuals) {
		if (!visual.Active) continue;
		MyWorldTextureShader->SetTextureCommand(visual.HealWave ? &gParticleHealTexture : snowTexture);

		float t = min(max(visual.Age / max(visual.Duration, 0.001f), 0.0f), 1.0f);
		float eased = 1.0f - powf(1.0f - t, 2.35f);
		float radius = max(1.0f, visual.Radius * eased);
		float alpha = sinf(t * XM_PI) * (visual.HealWave ? 0.30f : 0.24f);
		float spin = sFrostSnowWaveTime * 0.42f + visual.Position.x * 0.017f + visual.Position.z * 0.021f;

		matrix world = XMMatrixRotationY(spin) * XMMatrixScaling(radius, 0.030f, radius);
		world.pos = visual.Position + vec4(0.0f, 0.10f, 0.0f, 0.0f);
		world.pos.w = 1.0f;
		world.transpose();

		const int frameCols = 4;
		const int frameRows = 4;
		int frame = min((int)(t * 13.0f), frameCols * frameRows - 1);
		vec4 uvAnim = vec4(1.0f / (float)frameCols, 1.0f / (float)frameRows,
			(float)(frame % frameCols) / (float)frameCols,
			(float)(frame / frameCols) / (float)frameRows);
		vec4 tint = visual.HealWave
			? vec4(0.25f, 1.65f, 0.72f, alpha)
			: vec4(0.72f, 1.08f, 1.95f, alpha);
		gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Transform, 16, &world, 0);
		gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Tint, 4, &tint, 0);
		gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_UVAnim, 4, &uvAnim, 0);
		BossPrototypeShieldMesh->Render(gd.gpucmd, 1);
	}
}

void Game::RenderStatusEffectOverlays()
{
	return;
}

void Game::RenderRailgunOrbitVisuals()
{
	if (MyWorldTextureShader == nullptr || BossPrototypeShieldMesh == nullptr || gRailgunOrbitVisuals.empty()) return;
	if (gParticleElectricTexture.resource == nullptr && gParticleYellowTexture.resource == nullptr) return;

	bool hasActive = false;
	for (RailgunOrbitVisual& visual : gRailgunOrbitVisuals) {
		if (!visual.Active) continue;
		visual.Age += max(0.0f, DeltaTime);
		if (visual.Age > visual.Duration) {
			visual.Active = false;
			continue;
		}
		hasActive = true;
	}
	if (!hasActive) return;

	static float sRailgunOrbitTime = 0.0f;
	sRailgunOrbitTime += max(0.0f, DeltaTime);

	matrix view = gd.viewportArr[0].ViewMatrix * gd.viewportArr[0].ProjectMatrix;
	view.transpose();

	using WTSRP = WorldTextureShader::RootParamId;
	gd.gpucmd.SetShader(MyWorldTextureShader);
	gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Camera, 16, &view, 0);
	for (const RailgunOrbitVisual& visual : gRailgunOrbitVisuals) {
		if (!visual.Active) continue;
		GPUResource* orbitTexture = visual.Yellow && gParticleYellowTexture.resource != nullptr
			? &gParticleYellowTexture : &gParticleElectricTexture;
		if (orbitTexture->resource == nullptr) continue;
		MyWorldTextureShader->SetTextureCommand(orbitTexture);

		vec4 center = visual.Position;
		if (visual.OwnerId < DynmaicGameObjects.size() && DynmaicGameObjects[visual.OwnerId] != nullptr) {
			center = DynmaicGameObjects[visual.OwnerId]->worldMat.pos + vec4(0.0f, visual.Yellow ? 0.92f : 1.05f, 0.0f, 0.0f);
		}

		float life = min(max(visual.Age / max(visual.Duration, 0.001f), 0.0f), 1.0f);
		float alpha = min(life * 5.0f, 1.0f) * min((1.0f - life) * 7.0f, 1.0f);
		const int orbitCount = visual.Yellow ? 6 : 5;
		for (int i = 0; i < orbitCount; ++i) {
			float angle = sRailgunOrbitTime * 5.8f + i * (XM_2PI / (float)orbitCount);
			float bob = sinf(sRailgunOrbitTime * 8.0f + i * 1.7f) * 0.12f;
			vec4 orbPos = center + vec4(cosf(angle) * visual.Radius, bob, sinf(angle) * visual.Radius, 0.0f);
			float orbScale = 0.22f + 0.040f * sinf(sRailgunOrbitTime * 11.0f + i);

			matrix world = XMMatrixScaling(orbScale, orbScale, orbScale);
			world.pos = orbPos;
			world.pos.w = 1.0f;
			world.transpose();

			vec4 tint = visual.Yellow
				? vec4(2.55f, 1.48f, 0.18f, 0.56f * alpha)
				: vec4(0.50f, 1.02f, 2.55f, 0.50f * alpha);
			int frame = visual.Yellow
				? (((int)(sRailgunOrbitTime * 14.0f) + i * 2) % 16)
				: (((int)(sRailgunOrbitTime * 16.0f) + i * 3) % 32);
			vec4 uvAnim = visual.Yellow
				? vec4(0.25f, 0.25f, (float)(frame % 4) * 0.25f, (float)(frame / 4) * 0.25f)
				: vec4(0.25f, 0.125f, (float)(frame % 4) * 0.25f, (float)(frame / 4) * 0.125f);
			gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Transform, 16, &world, 0);
			gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Tint, 4, &tint, 0);
			gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_UVAnim, 4, &uvAnim, 0);
			BossPrototypeShieldMesh->Render(gd.gpucmd, 1);
		}
	}
}

void Game::RenderDualBladeFloorVisuals()
{
	if (MyWorldTextureShader == nullptr || BossPrototypeShieldMesh == nullptr || gDualBladeFloorVisuals.empty()) return;
	if (gParticleElectricTexture.resource == nullptr) return;

	bool hasActive = false;
	for (DualBladeFloorVisual& visual : gDualBladeFloorVisuals) {
		if (!visual.Active) continue;
		visual.Age += max(0.0f, DeltaTime);
		if (visual.Age > visual.Duration) {
			visual.Active = false;
			continue;
		}
		hasActive = true;
	}
	if (!hasActive) return;

	static float sDualBladeFloorTime = 0.0f;
	sDualBladeFloorTime += max(0.0f, DeltaTime);
	matrix view = gd.viewportArr[0].ViewMatrix * gd.viewportArr[0].ProjectMatrix;
	view.transpose();

	using WTSRP = WorldTextureShader::RootParamId;
	gd.gpucmd.SetShader(MyWorldTextureShader);
	gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Camera, 16, &view, 0);
	MyWorldTextureShader->SetTextureCommand(&gParticleElectricTexture);

	for (const DualBladeFloorVisual& visual : gDualBladeFloorVisuals) {
		if (!visual.Active) continue;
		vec4 center = visual.Position;
		if (visual.OwnerId < DynmaicGameObjects.size() && DynmaicGameObjects[visual.OwnerId] != nullptr) {
			center = DynmaicGameObjects[visual.OwnerId]->worldMat.pos;
		}
		float life = min(max(visual.Age / max(visual.Duration, 0.001f), 0.0f), 1.0f);
		float fade = min(life * 7.0f, 1.0f) * min((1.0f - life) * 8.0f, 1.0f);
		float pulse = 0.94f + sinf(sDualBladeFloorTime * 5.5f) * 0.06f;
		matrix world = XMMatrixRotationY(sDualBladeFloorTime * 0.65f) *
			XMMatrixScaling(visual.Radius * pulse, 0.025f, visual.Radius * pulse);
		world.pos = center + vec4(0.0f, 0.07f, 0.0f, 0.0f);
		world.pos.w = 1.0f;
		world.transpose();
		vec4 tint = vec4(0.30f, 0.82f, 2.10f, 0.20f * fade);
		int frame = ((int)(sDualBladeFloorTime * 12.0f)) % 32;
		vec4 uvAnim = vec4(0.25f, 0.125f, (float)(frame % 4) * 0.25f, (float)(frame / 4) * 0.125f);
		gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Transform, 16, &world, 0);
		gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Tint, 4, &tint, 0);
		gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_UVAnim, 4, &uvAnim, 0);
		BossPrototypeShieldMesh->Render(gd.gpucmd, 1);
	}
}

void Game::RenderHackerEmpVisuals()
{
	if (MyWorldTextureShader == nullptr || BossPrototypeShieldMesh == nullptr || gHackerEmpVisuals.empty()) return;
	if (gParticleHackTexture.resource == nullptr) return;
	static float visualTime = 0.0f;
	bool active = false;
	for (HackerEmpVisual& visual : gHackerEmpVisuals) {
		if (!visual.Active) continue;
		visual.Age += max(0.0f, DeltaTime);
		if (visual.Age > visual.Duration) {
			visual.Active = false;
			continue;
		}
		active = true;
	}
	if (!active) return;
	visualTime = fmodf(visualTime + max(0.0f, DeltaTime), 40.0f);

	matrix view = gd.viewportArr[0].ViewMatrix * gd.viewportArr[0].ProjectMatrix;
	view.transpose();
	using WTSRP = WorldTextureShader::RootParamId;
	gd.gpucmd.SetShader(MyWorldTextureShader);
	gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Camera, 16, &view, 0);
	MyWorldTextureShader->SetTextureCommand(&gParticleHackTexture);
	for (const HackerEmpVisual& visual : gHackerEmpVisuals) {
		if (!visual.Active) continue;
		vec4 center = visual.Position;
		if (visual.GroundField && visual.OwnerId < DynmaicGameObjects.size() && DynmaicGameObjects[visual.OwnerId] != nullptr)
			center = DynmaicGameObjects[visual.OwnerId]->worldMat.pos;
		float t = min(max(visual.Age / max(visual.Duration, 0.001f), 0.0f), 1.0f);
		float eased = 1.0f - powf(1.0f - t, 2.6f);
		float scale = visual.GroundField ? visual.Radius : max(0.25f, visual.Radius * eased);
		matrix world = XMMatrixRotationY(visualTime * 0.28f) * XMMatrixScaling(scale,
			visual.GroundField ? 0.035f : scale, scale);
		world.pos = center + vec4(0.0f, visual.GroundField ? 0.09f : 1.0f, 0.0f, 0.0f);
		world.pos.w = 1.0f;
		world.transpose();
		float alpha = visual.GroundField ? 0.18f : powf(1.0f - t, 0.65f) * 0.42f;
		vec4 tint = vec4(1.65f, 0.28f, 2.55f, alpha);
		vec4 uvAnim = vec4(1, 1, 0, 0);
		gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Transform, 16, &world, 0);
		gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Tint, 4, &tint, 0);
		gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_UVAnim, 4, &uvAnim, 0);
		BossPrototypeShieldMesh->Render(gd.gpucmd, 1);
	}
}

void Game::RenderBossPrototypeBeam()
{
	if (!BossPrototypeEnabled || MyWorldTextureShader == nullptr || BossPrototypeBeamMesh == nullptr) return;
	bool isRailgun = BossPrototypePhaseState == BossPrototypePhase::RailgunCharge;
	bool isRotatingLaser = BossPrototypePhaseState == BossPrototypePhase::RotatingLaser;
	if (!isRailgun && !isRotatingLaser) return;

	const float fireStart = isRotatingLaser ? 0.85f : 2.55f;
	const float fireEnd = isRotatingLaser ? 8.1f : 4.65f;
	if (BossPrototypePhaseTime < fireStart || BossPrototypePhaseTime > fireEnd) return;

	GPUResource* beamTexture = GetBossBeamTextureResource();
	if (beamTexture == nullptr || beamTexture->resource == nullptr) return;

	matrix view = gd.viewportArr[0].ViewMatrix * gd.viewportArr[0].ProjectMatrix;
	view.transpose();
	using WTSRP = WorldTextureShader::RootParamId;
	gd.gpucmd.SetShader(MyWorldTextureShader);
	gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Camera, 16, &view, 0);
	MyWorldTextureShader->SetTextureCommand(beamTexture);

	auto SetBeamFrame = [&](float age, int seedOffset, float cropU0, float cropU1, float cropV0, float cropV1) {
		const int frameCols = 3;
		const int frameRows = 2;
		const int frameCount = frameCols * frameRows;
		int frame = ((int)(age * 18.0f) + seedOffset) % frameCount;
		if (frame < 0) frame += frameCount;
		const float cellU = 1.0f / (float)frameCols;
		const float cellV = 1.0f / (float)frameRows;
		vec4 uvAnim = vec4(cellU * (cropU1 - cropU0), cellV * (cropV1 - cropV0),
			(float)(frame % frameCols) * cellU + cropU0 * cellU,
			(float)(frame / frameCols) * cellV + cropV0 * cellV);
		gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_UVAnim, 4, &uvAnim, 0);
	};

	auto RenderBeamPass = [&](vec4 origin, vec4 beamDirection, float beamLength, float halfWidth, float halfHeight, float pulse, vec4 tint, float volumeAlphaScale) {
		vec4 beamCenter = origin + beamDirection * (beamLength * 0.5f);
		vec4 side = NormalizeOrFallback(vec4(0, 1, 0, 0).cross(beamDirection), vec4(1, 0, 0, 0));
		vec4 up = NormalizeOrFallback(beamDirection.cross(side), vec4(0, 1, 0, 0));
		auto RenderPlane = [&](vec4 planeSide, float widthScale, vec4 planeTint) {
			matrix world;
			vec4 planeUp = NormalizeOrFallback(beamDirection.cross(planeSide), up);
			world.Id();
			world.right = planeSide * halfWidth * widthScale * (1.0f + 0.06f * pulse);
			world.up = planeUp * halfHeight;
			world.look = beamDirection * (beamLength * 0.5f);
			world.pos = beamCenter;
			world.pos.w = 1.0f;
			world.transpose();
			gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Transform, 16, &world, 0);
			gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Tint, 4, &planeTint, 0);
			BossPrototypeBeamMesh->Render(gd.gpucmd, 1);
		};

		RenderPlane(side, 1.0f, tint);

		if (volumeAlphaScale > 0.001f) {
			vec4 volumeTint = vec4(tint.x * 0.72f, tint.y * 0.78f, tint.z * 0.88f, tint.w * volumeAlphaScale);
			RenderPlane(up, 0.86f, volumeTint);
			RenderPlane(NormalizeOrFallback(side + up, side), 0.70f, volumeTint);
			RenderPlane(NormalizeOrFallback(side - up, side), 0.70f, volumeTint);
		}
	};

	if (isRotatingLaser) {
		for (const BossAoEWarning& warning : BossAoEWarnings) {
			if (!warning.Active || warning.Shape != BossAoEShape::Rectangle || warning.Length < 38.0f) continue;
			if (warning.Age < warning.WarningTime) continue;

			vec4 beamDirection = NormalizeOrFallback(warning.Direction, BossPrototypeAimDirection);
			beamDirection.y = 0.0f;
			beamDirection = NormalizeOrFallback(beamDirection, vec4(0, 0, 1, 0));

			const float beamAge = warning.Age - warning.WarningTime;
			const float pulse = 0.82f + 0.18f * sinf((BossPrototypePhaseTime + warning.Age) * 82.0f);
			SetBeamFrame(beamAge, (int)(warning.Direction.x * 17.0f + warning.Direction.z * 23.0f), 0.22f, 0.82f, 0.08f, 0.92f);
			vec4 muzzleOrigin = BossPrototypeCenter + beamDirection * 1.15f;
			muzzleOrigin.y = GetBossVisualGroundY() + 1.95f;
			muzzleOrigin.w = 1.0f;
			RenderBeamPass(muzzleOrigin, beamDirection, warning.Length, 1.08f, 1.16f, pulse,
				vec4(1.55f, 0.32f, 0.28f, 0.20f + 0.05f * pulse), 0.52f);
		}
		return;
	}

	const float beamLength = isRotatingLaser ? 40.0f : 36.0f;
	const float beamHalfWidth = isRotatingLaser ? 1.05f : 1.18f;
	const float beamHalfHeight = 1.16f;
	const float t = (BossPrototypePhaseTime - fireStart) / max(fireEnd - fireStart, 0.001f);
	const float pulse = (isRotatingLaser ? 0.82f : 0.72f) + (isRotatingLaser ? 0.18f : 0.28f) * sinf(BossPrototypePhaseTime * 82.0f);
	vec4 beamDirection = NormalizeOrFallback(BossPrototypeRailgunDirection, BossPrototypeAimDirection);
	beamDirection.y = 0.0f;
	beamDirection = NormalizeOrFallback(beamDirection, vec4(0, 0, 1, 0));

	vec4 tint = isRotatingLaser ? vec4(1.55f, 0.32f, 0.28f, 0.20f + 0.05f * pulse)
		: vec4(1.60f, 0.34f, 0.30f, 0.22f + 0.05f * pulse + 0.02f * (1.0f - t));
	SetBeamFrame(BossPrototypePhaseTime - fireStart, 0, 0.22f, 0.82f, 0.08f, 0.92f);
	vec4 railOrigin = BossPrototypeCenter + beamDirection * 1.15f;
	railOrigin.y = GetBossVisualGroundY() + 1.95f;
	railOrigin.w = 1.0f;
	RenderBeamPass(railOrigin, beamDirection, beamLength, beamHalfWidth, beamHalfHeight, pulse, tint, 0.52f);
}

void Game::RenderSupportDrones()
{
	if (SupportDroneModel == nullptr) return;
	static float droneBobTime = 0.0f;
	droneBobTime += max(0.0f, DeltaTime);

	if (gd.isRaytracingRender) {
		for (GameObject* object : DynmaicGameObjects) {
			Player* droneOwner = dynamic_cast<Player*>(object);
			if (droneOwner == nullptr) continue;
			const bool showDrones = droneOwner->tag[GameObjectTag::Tag_Enable] &&
				(PlayerJob)droneOwner->m_currentJob == PlayerJob::DroneOperator;
			for (int droneIndex = 0; droneIndex < 2; ++droneIndex) {
				if (droneOwner->DronObj[droneIndex] != nullptr) {
					droneOwner->DronObj[droneIndex]->SetRaytracingInstanceEnabled(showDrones);
				}
			}
			if (!showDrones) continue;

			for (int droneIndex = 0; droneIndex < 2; ++droneIndex) {
				if (droneOwner->DronObj[droneIndex] == nullptr) continue;
				const bool leftDrone = droneIndex == 0;
				const float phase = droneBobTime * 2.6f + (leftDrone ? 0.0f : XM_PI);
				const float bob = sinf(phase) * 0.09f;
				vec4 dronePosition = GetSupportDronePosition(droneOwner, leftDrone, bob);

				matrix droneWorld = XMMatrixRotationY(XMConvertToRadians(-90.0f)) *
					(XMMATRIX)droneOwner->worldMat;
				droneWorld.right *= 0.24f;
				droneWorld.up *= 0.24f;
				droneWorld.look *= 0.24f;
				droneWorld.pos = dronePosition;
				droneWorld.pos.w = 1.0f;

				droneOwner->DronObj[droneIndex]->worldMat = droneWorld;
				droneOwner->DronObj[droneIndex]->RaytracingUpdateTransform();

				if (droneOwner->m_droneFlightVisualTimer > 0.0f) {
					const float side = leftDrone ? -1.0f : 1.0f;
					vec4 tetherTarget = droneOwner->worldMat.pos + droneOwner->worldMat.up * 1.05f +
						droneOwner->worldMat.right * (0.28f * side);
					tetherTarget.w = 1.0f;
					QueueSupportBeam(dronePosition, tetherTarget, 0.035f, 0.14f,
						vec4(0.32f, 1.48f, 0.82f, 0.34f));
					QueueSupportBeam(dronePosition, tetherTarget, 0.072f, 0.14f,
						vec4(0.72f, 1.72f, 1.35f, 0.25f), true);
				}
			}
		}
	}
	else {
		for (GameObject* object : DynmaicGameObjects) {
			Player* droneOwner = dynamic_cast<Player*>(object);
			if (droneOwner == nullptr || (PlayerJob)droneOwner->m_currentJob != PlayerJob::DroneOperator) continue;
			if (!droneOwner->tag[GameObjectTag::Tag_Enable]) continue;

			for (int droneIndex = 0; droneIndex < 2; ++droneIndex) {
				const bool leftDrone = droneIndex == 0;
				const float phase = droneBobTime * 2.6f + (leftDrone ? 0.0f : XM_PI);
				const float bob = sinf(phase) * 0.09f;
				vec4 dronePosition = GetSupportDronePosition(droneOwner, leftDrone, bob);

				matrix droneWorld = XMMatrixRotationY(XMConvertToRadians(-90.0f)) *
					(XMMATRIX)droneOwner->worldMat;
				droneWorld.right *= 0.24f;
				droneWorld.up *= 0.24f;
				droneWorld.look *= 0.24f;
				droneWorld.pos = dronePosition;
				droneWorld.pos.w = 1.0f;
				SupportDroneModel->Render<false>(gd.gpucmd, droneWorld, nullptr);

				if (droneOwner->m_droneFlightVisualTimer > 0.0f) {
					const float side = leftDrone ? -1.0f : 1.0f;
					vec4 tetherTarget = droneOwner->worldMat.pos + droneOwner->worldMat.up * 1.05f +
						droneOwner->worldMat.right * (0.28f * side);
					tetherTarget.w = 1.0f;
					QueueSupportBeam(dronePosition, tetherTarget, 0.035f, 0.14f,
						vec4(0.32f, 1.48f, 0.82f, 0.34f));
					QueueSupportBeam(dronePosition, tetherTarget, 0.072f, 0.14f,
						vec4(0.72f, 1.72f, 1.35f, 0.25f), true);
				}
			}
		}
	}
}

void Game::RenderPlayerRailgunBeams()
{
	if (MyWorldTextureShader == nullptr || BossPrototypeBeamMesh == nullptr) return;

	bool hasActive = false;
	for (RailgunBeamVisual& visual : gRailgunBeamVisuals) {
		if (!visual.Active) continue;
		visual.Age += max(0.0f, DeltaTime);
		if (visual.Age > visual.Duration) {
			visual.Active = false;
			continue;
		}
		hasActive = true;
	}
	if (!hasActive) return;

	GPUResource* beamTexture = GetPlayerBeamTextureResource();
	if (beamTexture == nullptr || beamTexture->resource == nullptr) return;

	matrix view = gd.viewportArr[0].ViewMatrix * gd.viewportArr[0].ProjectMatrix;
	view.transpose();
	using WTSRP = WorldTextureShader::RootParamId;
	gd.gpucmd.SetShader(MyWorldTextureShader);
	gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Camera, 16, &view, 0);
	static float supportBeamAnimationTime = 0.0f;
	supportBeamAnimationTime = fmodf(supportBeamAnimationTime + max(0.0f, DeltaTime), 30.0f);

	auto SetBeamFrame = [&](float age, bool useEnergyTexture) {
		const int frameCols = useEnergyTexture ? 8 : 3;
		const int frameRows = useEnergyTexture ? 8 : 2;
		const int frameCount = frameCols * frameRows;
		int frame = ((int)(age * (useEnergyTexture ? 28.0f : 22.0f))) % frameCount;
		const float cellU = 1.0f / (float)frameCols;
		const float cellV = 1.0f / (float)frameRows;
		vec4 uvAnim = useEnergyTexture
			? vec4(cellU, cellV, (float)(frame % frameCols) * cellU,
				(float)(frame / frameCols) * cellV)
			: vec4(cellU * 0.60f, cellV * 0.84f,
				(float)(frame % frameCols) * cellU + 0.22f * cellU,
				(float)(frame / frameCols) * cellV + 0.08f * cellV);
		gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_UVAnim, 4, &uvAnim, 0);
	};

	for (const RailgunBeamVisual& visual : gRailgunBeamVisuals) {
		if (!visual.Active) continue;

		float t = visual.Age / max(visual.Duration, 0.001f);
		float fade = powf(max(1.0f - t, 0.0f), 0.55f);
		float pulse = 0.75f + 0.25f * sinf(visual.Age * 95.0f);
		vec4 dir = NormalizeOrFallback(visual.Direction, vec4(0, 0, 1, 0));
		vec4 center = visual.Origin + dir * (visual.Length * 0.5f);
		vec4 side = NormalizeOrFallback(vec4(0, 1, 0, 0).cross(dir), vec4(1, 0, 0, 0));
		vec4 up = NormalizeOrFallback(dir.cross(side), vec4(0, 1, 0, 0));
		float halfWidth = visual.Width * (1.0f + 0.05f * pulse);
		float halfHeight = visual.Width * 0.62f;
		vec4 tint = visual.Tint;
		tint.w *= (0.82f + 0.18f * pulse) * fade;

		const bool useEnergyTexture = visual.UseEnergyTexture && gParticleEnergyTexture.resource != nullptr;
		MyWorldTextureShader->SetTextureCommand(useEnergyTexture ? &gParticleEnergyTexture : beamTexture);
		SetBeamFrame(useEnergyTexture ? supportBeamAnimationTime : visual.Age, useEnergyTexture);
		auto RenderPlane = [&](vec4 planeSide, float widthScale, vec4 planeTint) {
			matrix world;
			vec4 planeUp = NormalizeOrFallback(dir.cross(planeSide), up);
			world.Id();
			world.right = planeSide * halfWidth * widthScale;
			world.up = planeUp * halfHeight;
			world.look = dir * (visual.Length * 0.5f);
			world.pos = center;
			world.pos.w = 1.0f;
			world.transpose();
			gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Transform, 16, &world, 0);
			gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Tint, 4, &planeTint, 0);
			BossPrototypeBeamMesh->Render(gd.gpucmd, 1);
		};

		RenderPlane(side, 1.0f, tint);
		vec4 afterImageTint = vec4(tint.x * 0.65f, tint.y * 0.72f, tint.z, tint.w * 0.22f);
		RenderPlane(side, 1.55f, afterImageTint);
		vec4 volumeTint = vec4(tint.x * 0.72f, tint.y * 0.78f, tint.z * 0.88f, tint.w * 0.24f);
		RenderPlane(up, 0.84f, volumeTint);
	}
}

void Game::RenderBossPrototypeMissiles()
{
	for (SkillMissileVisual& visual : gSkillMissileVisuals) {
		if (!visual.Active) continue;
		visual.Age += max(0.0f, DeltaTime);
		if (visual.Age >= visual.Duration + 0.08f) {
			visual.Active = false;
		}
	}
	gSkillMissileVisuals.erase(std::remove_if(gSkillMissileVisuals.begin(), gSkillMissileVisuals.end(),
		[](const SkillMissileVisual& visual) {
			return !visual.Active;
		}), gSkillMissileVisuals.end());

	if (MyWorldTextureShader == nullptr || BossPrototypeMissileMesh == nullptr ||
		(BossMissileVisuals.empty() && gSkillMissileVisuals.empty())) return;

	GPUResource* bossMissileTexture = GetBossAirStrikeTextureResource();
	GPUResource* skillMissileTexture = GetSkillAirStrikeTextureResource();
	bool canRenderBossMissiles = bossMissileTexture != nullptr && bossMissileTexture->resource != nullptr;
	bool canRenderSkillMissiles = skillMissileTexture != nullptr && skillMissileTexture->resource != nullptr;
	if (!canRenderBossMissiles && !canRenderSkillMissiles) return;

	matrix view = gd.viewportArr[0].ViewMatrix * gd.viewportArr[0].ProjectMatrix;
	view.transpose();

	using WTSRP = WorldTextureShader::RootParamId;
	gd.gpucmd.SetShader(MyWorldTextureShader);
	gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Camera, 16, &view, 0);

	if (canRenderBossMissiles) {
		MyWorldTextureShader->SetTextureCommand(bossMissileTexture);
		if (gd.isRaytracingRender) {
			vec4 depthInfo = gd.viewportArr[0].Camera_Pos;
			depthInfo.w = 1.0f;
			gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_DepthInfo, 4, &depthInfo, 0);
		}
		for (const BossMissileVisual& visual : BossMissileVisuals) {
			if (!visual.Active) continue;

			float t = min(max(visual.Age / max(visual.Duration, 0.001f), 0.0f), 1.0f);
			float eased = t * t * (3.0f - 2.0f * t);
			vec4 pos = visual.Start * (1.0f - eased) + visual.Target * eased;
			pos.w = 1.0f;

			vec4 dir = NormalizeOrFallback(visual.Target - visual.Start, vec4(0, -1, 0, 0));
			float flare = 1.0f + 0.55f * sinf((visual.Age + visual.Radius) * 28.0f);
			float fadeIn = min(t * 4.0f, 1.0f);
			float fadeOut = min((1.0f - t) * 5.0f, 1.0f);

			matrix world;
			world.LookAt(dir);
			world.right *= visual.Radius * 1.18f * flare;
			world.up *= visual.Radius * 1.18f * flare;
			world.look *= 4.2f + visual.Radius * 1.25f;
			world.pos = pos;
			world.pos.w = 1.0f;
			world.transpose();

			vec4 tint = vec4(3.2f, 1.75f, 0.72f, 1.0f * fadeIn * fadeOut);
			const int frameCols = 4;
			const int frameRows = 3;
			const int frame = 0;
			vec4 uvAnim = vec4(1.0f / (float)frameCols, 1.0f / (float)frameRows,
				(float)(frame % frameCols) / (float)frameCols,
				(float)(frame / frameCols) / (float)frameRows);
			gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_UVAnim, 4, &uvAnim, 0);
			gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Transform, 16, &world, 0);
			gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Tint, 4, &tint, 0);
			BossPrototypeMissileMesh->Render(gd.gpucmd, 1);
		}
	}

	if (canRenderSkillMissiles) {
		MyWorldTextureShader->SetTextureCommand(skillMissileTexture);
		if (gd.isRaytracingRender) {
			vec4 depthInfo = gd.viewportArr[0].Camera_Pos;
			depthInfo.w = 1.0f;
			gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_DepthInfo, 4, &depthInfo, 0);
		}
		for (const SkillMissileVisual& visual : gSkillMissileVisuals) {
			if (!visual.Active) continue;

			float t = min(max(visual.Age / max(visual.Duration, 0.001f), 0.0f), 1.0f);
			float eased = t * t * (3.0f - 2.0f * t);
			vec4 pos = visual.Start * (1.0f - eased) + visual.Target * eased;
			pos.w = 1.0f;

			vec4 dir = NormalizeOrFallback(visual.Target - visual.Start, vec4(0, -1, 0, 0));
			float flare = 1.0f + 0.55f * sinf((visual.Age + visual.Radius) * 28.0f);
			float fadeIn = min(t * 4.0f, 1.0f);
			float fadeOut = min((1.0f - t) * 5.0f, 1.0f);

			matrix world;
			world.LookAt(dir);
			world.right *= visual.Radius * 1.18f * flare;
			world.up *= visual.Radius * 1.18f * flare;
			world.look *= 4.2f + visual.Radius * 1.25f;
			world.pos = pos;
			world.pos.w = 1.0f;
			world.transpose();

			vec4 tint = vec4(0.74f, 1.32f, 3.4f, 1.0f * fadeIn * fadeOut);
			const int frameCols = 4;
			const int frameRows = 3;
			const int frame = 0;
			vec4 uvAnim = vec4(1.0f / (float)frameCols, 1.0f / (float)frameRows,
				(float)(frame % frameCols) / (float)frameCols,
				(float)(frame / frameCols) / (float)frameRows);
			gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_UVAnim, 4, &uvAnim, 0);
			gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Transform, 16, &world, 0);
			gd.gpucmd->SetGraphicsRoot32BitConstants(WTSRP::Const_Tint, 4, &tint, 0);
			BossPrototypeMissileMesh->Render(gd.gpucmd, 1);
		}
	}
}

void Game::RenderQuestCompleteShow()
{
	if (game.QuestCompleteShowFlow > 0) {
		vec4 rateloc = vec4(0.75f, 0.5f, 1.0f, 0.6f);
		WindowNormalizeCoordToDirectXRenderCoord_vec4(rateloc, gd.ClientFrameWidth, gd.ClientFrameHeight);
		game.UIDraw_TextureRect(rateloc, vec4(1, 1, 1, game.QuestCompleteShowFlow), 0.02f, 16);
		game.RenderSDFText(L"임무 완수!", 7, rateloc, 20, vec4(0, 0, 0, game.QuestCompleteShowFlow), nullptr, nullptr, 0.01f);
		rateloc.w -= 30;
		rateloc.y -= 30;
		if (game.CurrentCompleteQuest != nullptr)
		{
			game.RenderSDFText(game.CurrentCompleteQuest->QuestName, wcslen(game.CurrentCompleteQuest->QuestName), rateloc, 20, vec4(0, 0, 0, game.QuestCompleteShowFlow), nullptr, nullptr, 0.01f);
		}
		game.QuestCompleteShowFlow -= DeltaTime;
	}
}

void Game::RenderQuestPrograssShow()
{
	if (game.QuestPrograssShowFlow > 0) {
		vec4 rateloc = vec4(0.75f, 0.1f, 1.0f, 0.5f);
		WindowNormalizeCoordToDirectXRenderCoord_vec4(rateloc, gd.ClientFrameWidth, gd.ClientFrameHeight);
		game.UIDraw_TextureRect(rateloc, vec4(1, 1, 1, game.QuestPrograssShowFlow), 0.02f, 16);
		game.RenderSDFText(L"임무 수행 현황", 8, rateloc, 20, vec4(0, 0, 0, game.QuestPrograssShowFlow), nullptr, nullptr, 0.01f);
		rateloc.w -= 40;
		rateloc.y -= 40;
		if (game.CurrentPrograssQuest != nullptr)
		{
			game.RenderSDFText(game.CurrentPrograssQuest->QuestName, wcslen(game.CurrentPrograssQuest->QuestName), rateloc, 20, vec4(0, 0, 0, game.QuestPrograssShowFlow), nullptr, nullptr, 0.01f);
			rateloc.w -= 40;
			rateloc.y -= 40;

			for (int i = 0; i < game.CurrentPrograssQuest->requp; ++i) {
				wchar_t Text[256] = {};
				if (game.CurrentPrograssQuest->ReqArr[i].type == QuestType::KillMonster) {
					if (game.CurrentPrograssQuest->ReqArr[i].ObjID == (int)MonsterType::Walker) {
						swprintf_s(Text, L"네온 하이브 조직원 : %d / %d", game.CurrentPrograssQuest->ReqArr[i].PresentCnt, game.CurrentPrograssQuest->ReqArr[i].Cnt);
					}
					else if (game.CurrentPrograssQuest->ReqArr[i].ObjID == (int)MonsterType::Dron) {
						swprintf_s(Text, L"불법 개조 드론 : %d / %d", game.CurrentPrograssQuest->ReqArr[i].PresentCnt, game.CurrentPrograssQuest->ReqArr[i].Cnt);
					}
					else if (game.CurrentPrograssQuest->ReqArr[i].ObjID == (int)MonsterType::Tower) {
						swprintf_s(Text, L"포탑 몬스터 : %d / %d", game.CurrentPrograssQuest->ReqArr[i].PresentCnt, game.CurrentPrograssQuest->ReqArr[i].Cnt);
					}
					game.RenderSDFText(Text, wcslen(Text), rateloc, 20, vec4(0, 0, 0, game.QuestPrograssShowFlow), nullptr, nullptr, 0.01f);
					rateloc.w -= 40;
					rateloc.y -= 40;
				}
			}
		}
		game.QuestPrograssShowFlow -= DeltaTime;
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

Zone* Game::GetZoneByPosition(int x, int y) {
	for (int i = 0; i < ZoneTable.size(); ++i) {
		Zone* zone = ZoneTable[i];
		if (zone->x == x && zone->y == y) {
			return zone;
		}
	}
	return nullptr;
}

void Game::Init()
{
	GameObjectType::STATICINIT();

	//Quest Table
	{
		Quest* q = new Quest(L"불법 개조 드론 10개 파괴", L"저 빌어먹을 [네온 하이브] 놈들이 드디어 미쳐 날뛰고 있어.\n놈들이 시장 외곽 폐기물 처리장에 불법 개조한 드론들을 풀어놨거든?\n방금 전에도 물건을 떼러 가던 내 조카가 다리를 뜯길 뻔했어.\n구역 보안관 놈들은 뇌물을 처먹었는지 움직이지도 않아.\n네가 가서 그 쇳덩어리 개새끼들 10마리만 고철로 만들어 주면, \n 내 창고에 있는 최고급 방수 사이버웨어를 넘겨주지.어때?\n - 요구사항 : 불법 개조 드론 10개 파괴.");
		q->PushReq(QuestType::KillMonster, (int)MonsterType::Dron, 10); // 1번째 몬스터 10마리 처치
		q->PushReward(QuestRewardType::QRT_Exp, 0, 100);
		constexpr int RewardItemID = 1;
		q->PushReward(QuestRewardType::QRT_Item, RewardItemID, 1);
		QuestTable.push_back(q);

		q = new Quest(L"네온 하이브 조직원 15명 소탕", L"어이, 용병. 네가 개새끼들을 고철로 만든 게 네온 하이브 놈들 귀에 들어갔나 봐.\n방금 전 놈들이 내 진료소 문 앞에 '경고장'이랍시고 잘린 사람 손목을 던져두고 갔어.\n내 조카 녀석은 겁에 질려 울고 있고... 더는 못 참겠다.\n이 구역 놈들을 소탕해야 끝날 일이야.\n구역 보안관 놈들은 뇌물을 처먹었는지 움직이지도 않아.\n그 녀석들을 닥치는 대로 소탕해줘. 한 15명쯤은 필요해.\n - 요구사항 : 네온 하이브 조직원 15명 소탕");
		q->PushReq(QuestType::KillMonster, (int)MonsterType::Walker, 15); // 0번째 몬스터 15마리 처치
		q->PushReward(QuestRewardType::QRT_Money, 0, 1000);
		q->PushReward(QuestRewardType::QRT_Exp, 0, 200);
		QuestTable.push_back(q);

		q = new Quest(L"사일러스와 접신", L"결국 그들의 보스를 잡으면 끝나지 않을거야.\n내 정보 브로커 친구인 사일러스에게 연락해 뒀으니, 이 앞쪽에 있는 버스 정류장으로 가봐.\n그가 놈들의 아지트 좌표를 넘겨줄 거야. 보스를 잡으러 갈거지?\n - 요구사항 : 버스정류장에 있는 사일러스와 접신.");
		q->PushReq(QuestType::TalkNPC, 1, 0); // 첫번째 NPC와 대화하기
		q->PushReward(QuestRewardType::QRT_Exp, 0, 100);
		QuestTable.push_back(q);

		q = new Quest(L"하이브 타워 격파", L"용병, 내가 데이터 패드에서 대어를 낚았어.\n네온 하이브는 단순한 갱단이 아니야.\n상층 구역의 '하이브 타워' 전체가 놈들의 거대 실험실이자 본거지였어.\n그리고 그 정점에는 '마더 하이브' 엘리시아가 있지.\n놈은 하층 구역 인간들을 납치해 거대 연산 장치의 '생체 부품'으로 쓰고 있었어.\n이제 꼬리를 잘랐으니 몸통을 부술 차례야.\n내가 타워의 메인 프레임을 해킹해 엘리터 스카이 리프트(엘리베이터)를 열어줄 테니, 펜트하우스로 직행해.\n저 큰 사거리 옆 파란색 큰 건물단지 안에 그들의 건물이 있어.\n겁 먹지 말고, 어서 가봐. 넌 할 수 있을 거야.\n - 요구사항 : 네온 하이브 본거지 박살내기");
		q->PushReq(QuestType::DungeonClear, 0, 0); // 0번째 던전 클리어
		q->PushReward(QuestRewardType::QRT_Money, 0, 10000);
		q->PushReward(QuestRewardType::QRT_Exp, 0, 1000);
		QuestTable.push_back(q);
	}

	//NPC Talk Table
	{
		// 첫번째 퀘스트 : 불법 개조 드론 10개 파괴
		NPCTalkTable.push_back(NPCTalkData(L"맥스", L"어이, 용병. 돈 되는 일 하나 할래?")); // 0
		NPCTalkTable.push_back(NPCTalkData(L"맥스", L"저 빌어먹을 [네온 하이브] 놈들이 드디어 미쳐 날뛰고 있어.")); // 1
		NPCTalkTable.push_back(NPCTalkData(L"맥스", L"놈들이 시장 외곽 폐기물 처리장에 불법 개조한 드론들을 풀어놨거든?")); // 2
		NPCTalkTable.push_back(NPCTalkData(L"맥스", L"방금 전에도 물건을 떼러 가던 내 조카가 다리를 뜯길 뻔했어.")); // 3
		NPCTalkTable.push_back(NPCTalkData(L"맥스", L"구역 보안관 놈들은 뇌물을 처먹었는지 움직이지도 않아.")); // 4
		NPCTalkTable.push_back(NPCTalkData(L"맥스", L"네가 가서 그 쇳덩어리 개새끼들 10마리만 고철로 만들어 주면, \n 내 창고에 있는 최고급 방수 사이버웨어를 넘겨주지.어때?", false, TalkSelection(L"수락한다", false, 0), TalkSelection(L"거절한다.", true, 6))); // 5
		NPCTalkTable.push_back(NPCTalkData(L"맥스", L"음 그래 니가 그렇다면 어쩔 수 없지.", true)); // 6

		// 두번째 퀘스트 : 네온 하이브 조직원들을 15명 소탕
		NPCTalkTable.push_back(NPCTalkData(L"맥스", L"어이, 용병. 네가 개새끼들을 고철로 만든 게 네온 하이브 놈들 귀에 들어갔나 봐.")); // 7
		NPCTalkTable.push_back(NPCTalkData(L"맥스", L"방금 전 놈들이 내 진료소 문 앞에 '경고장'이랍시고 잘린 사람 손목을 던져두고 갔어.")); // 8
		NPCTalkTable.push_back(NPCTalkData(L"맥스", L"내 조카 녀석은 겁에 질려 울고 있고... 더는 못 참겠다.")); // 9
		NPCTalkTable.push_back(NPCTalkData(L"맥스", L"이 구역 놈들을 소탕해야 끝날 일이야.")); // 10
		NPCTalkTable.push_back(NPCTalkData(L"맥스", L"구역 보안관 놈들은 뇌물을 처먹었는지 움직이지도 않아.")); // 11
		NPCTalkTable.push_back(NPCTalkData(L"맥스", L"그 녀석들을 닥치는 대로 소탕해줘. 한 15명쯤은 필요해.", false, TalkSelection(L"수락한다", false, 0), TalkSelection(L"거절한다.", true, 13))); // 12
		NPCTalkTable.push_back(NPCTalkData(L"맥스", L"음 그래 니가 그렇다면 어쩔 수 없지.", true)); // 13

		// 세번째 퀘스트 : 사일러스와 접신
		NPCTalkTable.push_back(NPCTalkData(L"맥스", L"젠장. 수가 너무 많아. 결국 그들의 보스를 잡으면 끝나지 않을거야.")); // 14
		NPCTalkTable.push_back(NPCTalkData(L"맥스", L"내 정보 브로커 친구인 사일러스에게 연락해 뒀으니, 이 앞쪽에 있는 버스 정류장으로 가봐.")); // 15
		NPCTalkTable.push_back(NPCTalkData(L"맥스", L"그가 놈들의 아지트 좌표를 넘겨줄 거야. 보스를 잡으러 갈거지?", false, TalkSelection(L"수락한다", false, 0), TalkSelection(L"거절한다.", true, 13))); // 16
		NPCTalkTable.push_back(NPCTalkData(L"맥스", L"음 그래 니가 그렇다면 어쩔 수 없지.", true)); // 17

		// 네번째 퀘스트 : 하이브 타워 격파
		NPCTalkTable.push_back(NPCTalkData(L"사일러스", L"용병, 내가 데이터 패드에서 대어를 낚았어.")); // 18
		NPCTalkTable.push_back(NPCTalkData(L"사일러스", L"네온 하이브는 단순한 갱단이 아니야.")); // 19
		NPCTalkTable.push_back(NPCTalkData(L"사일러스", L"상층 구역의 '하이브 타워' 전체가 놈들의 거대 실험실이자 본거지였어.")); // 20
		NPCTalkTable.push_back(NPCTalkData(L"사일러스", L"그리고 그 정점에는 '마더 하이브' 엘리시아가 있지.")); // 21
		NPCTalkTable.push_back(NPCTalkData(L"사일러스", L"놈은 하층 구역 인간들을 납치해 거대 연산 장치의 '생체 부품'으로 쓰고 있었어.")); // 22
		NPCTalkTable.push_back(NPCTalkData(L"사일러스", L"이제 꼬리를 잘랐으니 몸통을 부술 차례야.")); // 23
		NPCTalkTable.push_back(NPCTalkData(L"사일러스", L"내가 타워의 메인 프레임을 해킹해 엘리터 스카이 리프트(엘리베이터)를 열어줄 테니, 펜트하우스로 직행해.")); // 24
		NPCTalkTable.push_back(NPCTalkData(L"사일러스", L"저 큰 사거리 옆 파란색 큰 건물단지 안에 그들의 건물이 있어.")); // 25
		NPCTalkTable.push_back(NPCTalkData(L"사일러스", L"겁 먹지 말고, 어서 가봐.", false, TalkSelection(L"수락한다", false, 0), TalkSelection(L"거절한다.", true, 27))); // 26
		NPCTalkTable.push_back(NPCTalkData(L"사일러스", L"음 그래 니가 그렇다면 어쩔 수 없지.", true)); // 27

		// 엔딩 : 마지막 퀘스트 완료 후 사일러스 대화 (마지막 줄 '닫는다' 클릭 시 서버가 스탯포인트 10 지급)
		NPCTalkTable.push_back(NPCTalkData(L"사일러스", L"결국 해냈군.")); // 28
		NPCTalkTable.push_back(NPCTalkData(L"사일러스", L"아무리 실력이 좋아도 단번에 하이브 타워를 격파할 줄이야.")); // 29
		NPCTalkTable.push_back(NPCTalkData(L"사일러스", L"대단하군. 이걸주지.", false, TalkSelection(L"닫기", false, 0))); // 30
	}

	StaticGameObjects.reserve(Zone::MaxStaticObjectCount * 9); // 1MB
	StaticGameObjects.resize(Zone::MaxStaticObjectCount * 9); // 1MB
	//1. Zone Init. - //fix - planning . unity map exporter export zone info. and client and server use it.

	game.RenderMaterialTable.reserve(Zone::MaxMaterialZoneMargin * 10);
	game.RenderTextureTable.reserve(Zone::MaxTextureZoneMargin * 10);
	game.RenderMaterialTable.resize(Zone::MaxMaterialZoneMargin * 10);
	game.RenderTextureTable.resize(Zone::MaxTextureZoneMargin * 10);
	
	{
		char ZoneName[128] = "Zone_0_0";
		for (int iz = 0; iz < 10; ++iz) {
			for (int ix = 0; ix < 10; ++ix) {
				ZoneName[7] = '0' + iz;
				ZoneName[5] = '0' + ix;

				// Make Zone
				Zone* tempZone = new Zone(iz * 10 + ix, ZoneName, ix, iz);
				game.ZoneTable.push_back(tempZone);
			}
		}

		// [party/dungeon] client must mirror the server's dungeon-floor zones (same ids/coords) so
		// MoveZone(100..105) is valid for entry + floor transitions. MUST match the server exactly
		// (see World dungeon zone creation: 2 instances x 3 floors = zones 100..105).
		{
			// instance 0: zones 100..102.
			game.ZoneTable.push_back(new Zone(100, "OfficeDungeon_1floor", 1000, 1002)); // inst0 1F
			game.ZoneTable.push_back(new Zone(101, "OfficeDungeon_2floor", 1001, 1000)); // inst0 2F
			game.ZoneTable.push_back(new Zone(102, "OfficeDungeon_Boss",   1001, 1010)); // inst0 Boss
			// instance 1: zones 103..105 (instance-0 coords + x100; non-adjacent so a client only ever
			// loads one instance -> any Asset_OffsetMul slot reuse is harmless, never co-loaded).
			game.ZoneTable.push_back(new Zone(103, "OfficeDungeon_1floor", 1100, 1102)); // inst1 1F
			game.ZoneTable.push_back(new Zone(104, "OfficeDungeon_2floor", 1101, 1100)); // inst1 2F
			game.ZoneTable.push_back(new Zone(105, "OfficeDungeon_Boss",   1101, 1110)); // inst1 Boss
		}

		//Set Near Zones
		for (int i = 0; i < game.ZoneTable.size(); ++i) {
			Zone* tempZone = game.ZoneTable[i];
			int nearSiz = 1;
			int nearSiz_5 = 5;
			for (int k = 0; k < game.ZoneTable.size(); ++k) {
				Zone* tempZone2 = game.ZoneTable[k];
				if (tempZone == tempZone2) {
					continue;
				}
				if (abs(tempZone2->x - tempZone->x) + abs(tempZone2->y - tempZone->y) <= 1) {
					tempZone->nearZones[nearSiz] = tempZone2;
					nearSiz += 1;
					continue;
				}
				if (abs(tempZone2->x - tempZone->x) <= 1 && abs(tempZone2->y - tempZone->y) <= 1) {
					tempZone->nearZones[nearSiz_5] = tempZone2;
					nearSiz_5 += 1;
				}
			}
			tempZone->BakeNear();
		}

		/*Zone* Zone_ThePort = new Zone(0, "The_Port", 0, 0);
		game.ZoneTable.push_back(Zone_ThePort);
		Zone* Zone_OfficeDungeon_1floor = new Zone(1, "OfficeDungeon_1floor", 1, 0);
		game.ZoneTable.push_back(Zone_OfficeDungeon_1floor);*/
		//Zone_ThePort->nearZones[1] = Zone_OfficeDungeon_1floor;
	}
	Current_Zone = game.GetZoneByPosition(4, 8);

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

		MyWorldTextureShader = new WorldTextureShader();
		MyWorldTextureShader->InitShader();

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
		gParticleElectricTexture.CreateTexture_fromFile(L"Resources/elect.png", game.basicTexFormat, game.basicTexMip, true);
		gParticleIceTexture.CreateTexture_fromFile(L"Resources/ice.png", game.basicTexFormat, game.basicTexMip, true);
		gParticleBloodTexture.CreateTexture_fromFile(L"Resources/blood.png", game.basicTexFormat, game.basicTexMip, true);
		gLoadingTex.CreateTexture_fromFile(L"Resources/Loading.png", game.basicTexFormat, game.basicTexMip);          // [loading] loading screen image
		gStartScreenTex.CreateTexture_fromFile(L"Resources/StartScreen.png", game.basicTexFormat, game.basicTexMip);  // [loading] start screen image
		// [jobselect] job-selection screen textures. Card index MUST match the PlayerJob enum order
		// (Juggernaut, Frost, Aegis, Mage, Sniper, Gunner, DroneOperator, Hacker, Bomber); mapped by weapon.
		gSelectJobBgTex.CreateTexture_fromFile(L"Resources/selectJop.png", game.basicTexFormat, game.basicTexMip);
		gConfirmTex.CreateTexture_fromFile(L"Resources/Confirm.png", game.basicTexFormat, game.basicTexMip);
		gJobCardTex[0].CreateTexture_fromFile(L"Resources/jug.png", game.basicTexFormat, game.basicTexMip);  // 0 Juggernaut  (JUGGERNAUT)
		gJobCardTex[1].CreateTexture_fromFile(L"Resources/Fro.png", game.basicTexFormat, game.basicTexMip);  // 1 Frost       (FROST)
		gJobCardTex[2].CreateTexture_fromFile(L"Resources/Shi.png", game.basicTexFormat, game.basicTexMip);  // 2 Aegis       (SHIELDER)
		gJobCardTex[3].CreateTexture_fromFile(L"Resources/Sol.png", game.basicTexFormat, game.basicTexMip);  // 3 Mage        (SOLDIER, rifle)
		gJobCardTex[4].CreateTexture_fromFile(L"Resources/Sni.png", game.basicTexFormat, game.basicTexMip);  // 4 Sniper
		gJobCardTex[5].CreateTexture_fromFile(L"Resources/Gun.png", game.basicTexFormat, game.basicTexMip);  // 5 Gunner      (GUNNER)
		gJobCardTex[6].CreateTexture_fromFile(L"Resources/Mec.png", game.basicTexFormat, game.basicTexMip);  // 6 DroneOperator (MECHANIC)
		gJobCardTex[7].CreateTexture_fromFile(L"Resources/Hak.png", game.basicTexFormat, game.basicTexMip);  // 7 Hacker      (HACKER)
		gJobCardTex[8].CreateTexture_fromFile(L"Resources/Lau.png", game.basicTexFormat, game.basicTexMip);  // 8 Bomber      (LAUNCHER)
		gParticleExplosionTexture.CreateTexture_fromFile(L"Resources/explosion.png", game.basicTexFormat, game.basicTexMip, true);
		gParticleEnergyTexture.CreateTexture_fromFile(L"Resources/energy.jpg", game.basicTexFormat, game.basicTexMip, true);
		gParticleAirStrikeTexture.CreateTexture_fromFile(L"Resources/misaile.png", game.basicTexFormat, game.basicTexMip, true);
		gParticleSkillAirStrikeTexture.CreateTexture_fromFile(L"Resources/rifle_missile_blue.png", game.basicTexFormat, game.basicTexMip, true);
		gParticleChargeTexture.CreateTexture_fromFile(L"Resources/charge.png", game.basicTexFormat, game.basicTexMip, true);
		gParticleBeamTexture.CreateTexture_fromFile(L"Resources/beam.png", game.basicTexFormat, game.basicTexMip, true);
		gParticleBossBeamTexture.CreateTexture_fromFile(L"Resources/boss_beam_red.png", game.basicTexFormat, game.basicTexMip, true);
		gParticleShieldTexture.CreateTexture_fromFile(L"Resources/shield.png", game.basicTexFormat, game.basicTexMip, true);
		gParticleYellowTexture.CreateTexture_fromFile(L"Resources/yellowEffect.png", game.basicTexFormat, game.basicTexMip, true);
		gParticleHackTexture.CreateTexture_fromFile(L"Resources/HackingEffect.png", game.basicTexFormat, game.basicTexMip, true);
		gParticleHealTexture.CreateTexture_fromFile(L"Resources/HealEffect.png", game.basicTexFormat, game.basicTexMip, true);
		gParticleSnowTexture.CreateTexture_fromFile(L"Resources/snow.png", game.basicTexFormat, game.basicTexMip, true);

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

		game.RaytracingTLASBlank = new BumpMesh();
		RaytracingTLASBlank->CreateWallMesh(1, 1, 1, vec4(1, 1, 1, 1));

		for (int i = 0; i < 3; ++i) {
			MyDirLight[i].ShadowMap = gd.CreateShadowMap(kShadowCascadeResolutions[i], kShadowCascadeResolutions[i], gd.GetDirLightCascadingShadowDSVIndex(i), MyDirLight[i]);
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
			InitTransientParticlePool(gIceProjectilePool, gIceProjectileUpload, gIceProjectileParticles, ICE_PROJECTILE_COUNT);
			InitTransientParticlePool(gPortalPool, gPortalUpload, gPortalParticles, 1024);
			InitTransientParticlePool(gBossExplosionPool, gBossExplosionUpload, gBossExplosionParticles, BOSS_EXPLOSION_PARTICLE_COUNT);
			InitTransientParticlePool(gStatusIcePool, gStatusIceUpload, gStatusIceParticles, STATUS_PARTICLE_COUNT);
			InitTransientParticlePool(gStatusFirePool, gStatusFireUpload, gStatusFireParticles, STATUS_PARTICLE_COUNT);
			InitTransientParticlePool(gStatusYellowPool, gStatusYellowUpload, gStatusYellowParticles, STATUS_PARTICLE_COUNT);
			InitTransientParticlePool(gStatusHackPool, gStatusHackUpload, gStatusHackParticles, STATUS_PARTICLE_COUNT);
			InitTransientParticlePool(gStatusHealPool, gStatusHealUpload, gStatusHealParticles, STATUS_PARTICLE_COUNT);
		}

		ParticleDraw = new ParticleShader();
		ParticleDraw->InitShader();

		ParticleDraw->FireTexture = &FireTextureRes;

		OBBDebugMesh = new Mesh();
		OBBDebugMesh->CreateWallMesh(1.0f, 1.0f, 1.0f, vec4(1, 1, 1, 1));

		BossPrototypeCircleMesh = new Mesh();
		BossPrototypeCircleMesh->CreateFlatDiskMesh(1.0f, 0.0f, 72, vec4(1.0f, 0.02f, 0.02f, 0.16f));

		BossPrototypeCircleOutlineMesh = new Mesh();
		BossPrototypeCircleOutlineMesh->CreateFlatDiskMesh(1.0f, 0.965f, 72, vec4(1.0f, 0.02f, 0.02f, 0.72f));

		BossPrototypeSafeCircleMesh = new Mesh();
		BossPrototypeSafeCircleMesh->CreateFlatDiskMesh(1.0f, 0.0f, 72, vec4(0.05f, 0.35f, 1.0f, 0.20f));

		BossPrototypeSafeCircleOutlineMesh = new Mesh();
		BossPrototypeSafeCircleOutlineMesh->CreateFlatDiskMesh(1.0f, 0.965f, 72, vec4(0.12f, 0.62f, 1.0f, 0.86f));

		BossPrototypeRectMesh = new Mesh();
		BossPrototypeRectMesh->CreateWallMesh(1.0f, 1.0f, 1.0f, vec4(1.0f, 0.02f, 0.02f, 0.42f));

		BossPrototypeBeamMesh = new UVMesh();
		BossPrototypeBeamMesh->CreateBeamMesh(vec4(1.0f, 1.0f, 1.0f, 1.0f), 1.0f, 1.0f);

		BossPrototypeMissileMesh = new UVMesh();
		BossPrototypeMissileMesh->CreateMissileSpriteMesh(vec4(1.0f, 1.0f, 1.0f, 1.0f), 1.0f, 1.0f);

		BossPrototypeShieldMesh = new UVMesh();
		BossPrototypeShieldMesh->CreateSphereMesh(1.0f, 48, 24, vec4(1.0f, 1.0f, 1.0f, 0.55f));

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

		HumanoidAnimation animRifleAimIdle;
		animRifleAimIdle.LoadHumanoidAnimation("Resources/Animation/rifle_aim_idle.Humanoid_animation");
		HumanoidAnimationTable.push_back(animRifleAimIdle); // 3: Rifle Aim Idle

		HumanoidAnimation animRifleAimShoot;
		animRifleAimShoot.LoadHumanoidAnimation("Resources/Animation/rifle_aim_shoot.Humanoid_animation");
		HumanoidAnimationTable.push_back(animRifleAimShoot); // 4: Rifle Aim Shoot

		HumanoidAnimation animPistolAimIdle;
		animPistolAimIdle.LoadHumanoidAnimation("Resources/Animation/pistol_aim_idle.Humanoid_animation");
		HumanoidAnimationTable.push_back(animPistolAimIdle); // 5: Pistol Aim Idle

		HumanoidAnimation animPistolAimShoot;
		animPistolAimShoot.LoadHumanoidAnimation("Resources/Animation/pistol_aim_shoot.Humanoid_animation");
		HumanoidAnimationTable.push_back(animPistolAimShoot); // 6: Pistol Aim Shoot

		HumanoidAnimation animJump;
		animJump.LoadHumanoidAnimation("Resources/Animation/jump.Humanoid_animation");
		HumanoidAnimationTable.push_back(animJump); // 7: Jump

		HumanoidAnimation animReload;
		animReload.LoadHumanoidAnimation("Resources/Animation/Reloading.Humanoid_animation");
		HumanoidAnimationTable.push_back(animReload); // 8: Rifle Reload

		HumanoidAnimation animPistolReload;
		animPistolReload.LoadHumanoidAnimation("Resources/Animation/DualGun_Reload.Humanoid_animation");
		HumanoidAnimationTable.push_back(animPistolReload); // 9: Pistol Reload

		HumanoidAnimation animDualGunAim;
		animDualGunAim.LoadHumanoidAnimation("Resources/Animation/DualGun_Aim.Humanoid_animation");
		HumanoidAnimationTable.push_back(animDualGunAim); // 10: Dual Gun Aim

		HumanoidAnimation animDualGunShoot;
		animDualGunShoot.LoadHumanoidAnimation("Resources/Animation/DualGun_Aim_Shoot.Humanoid_animation");
		HumanoidAnimationTable.push_back(animDualGunShoot); // 11: Dual Gun Shoot

		Model* PlayerModel = new Model();
		PlayerModel->LoadModelFile2("Resources/Model/Remy.model");
		PlayerModel->Retargeting_Humanoid(); // 휴머노이드 리타겟팅
		int playerMesh_index = Shape::AddModel("Player", PlayerModel);

		Model* MonsterModel = new Model();
		MonsterModel->LoadModelFile2("Resources/Model/Exo.model");
		MonsterModel->Retargeting_Humanoid();
		int monsterMesh_index = Shape::AddModel("Monster001", MonsterModel);

		Model* DroneModel = new Model();
		DroneModel->LoadModelFile2("Resources/Model/Drone.model", -1, false);
		int droneMesh_index = Shape::AddModel("MonsterDrone", DroneModel);

		Model* SupportDroneModel = new Model();
		SupportDroneModel->LoadModelFile2("Resources/Model/Drone.model", -1, true);
		int sdroneMesh_index = Shape::AddModel("SupportDrone", SupportDroneModel);
		game.SupportDroneModel = SupportDroneModel;

		Model* TurretModel = new Model();
		TurretModel->LoadModelFile2("Resources/Model/turret.model");
		int turretMesh_index = Shape::AddModel("MonsterTurret", TurretModel);

		BossPrototypeBossModel = new Model();
		BossPrototypeBossModel->LoadModelFile2("Resources/Model/turret_boss_mini.model");
		BossPrototypeBossShapeIndex = Shape::AddModel("TurretBoss", BossPrototypeBossModel);
		BossPrototypeBossPlatformNodeIndex = BossPrototypeBossModel->FindNodeIndexByName("Rotational Platform");
		BossPrototypeBossHeadNodeIndex = BossPrototypeBossModel->FindNodeIndexByName("Turret Head");

		BossPrototypeMiniTurretModel = new Model();
		BossPrototypeMiniTurretModel->LoadModelFile2("Resources/Model/turret_boss_mini.model");
		BossPrototypeMiniTurretShapeIndex = Shape::AddModel("TurretBossMini", BossPrototypeMiniTurretModel);

		BossPrototypeCoreModel = new Model();
		BossPrototypeCoreModel->LoadModelFile2("Resources/Model/turret_core.model", -1, true);
		BossPrototypeCoreShapeIndex = Shape::AddModel("TurretCore", BossPrototypeCoreModel);

		Mesh* portalMesh = new Mesh();
		portalMesh->CreateWallMesh(2.0f, 3.0f, 0.2f, 0.0f);
		Shape::AddMesh("Portal", portalMesh);

		{
			int globalitem_index = 0;
			Shape BlackShape;
			BlackShape.FlagPtr = 0;
			ItemTable.push_back(Item(globalitem_index, ItemType::_NULL, vec4(1, 1, 1, 1), BlackShape, nullptr, nullptr, L"")); // blank space in inventory.
			globalitem_index += 1;

			auto AddItemFunc = [&](int id, ItemType t, const char* ItemName, const char* modelfilepath, const wchar_t* ItemIconPath, const wchar_t* ItemDescription, void* data) -> Model* {
				Model* ItemModel = new Model();
				ItemModel->LoadModelFile2(modelfilepath);
				int Item_shapeindex = Shape::AddModel(ItemName, ItemModel);
				GPUResource* ItemIcon = new GPUResource();
				ItemIcon->CreateTexture_fromFile(ItemIconPath, game.basicTexFormat, game.basicTexMip, false);
				ItemTable.push_back(Item(globalitem_index, t, vec4(1, 1, 1, 1), Shape::ShapeTable[Item_shapeindex], nullptr, ItemIcon, ItemDescription, data));
				return ItemModel;
				};

			AddItemFunc(globalitem_index, ItemType::_Consumable, "BioFix",
				"Resources/Model/ItemModel/BioFix.model",
				L"Resources/UI/ItemIcons/ItemIcon_BioFix.png",
				L"[바이오픽스] : 신체 조직을 재생하는 가성비 의료 주사기! \n HP+20", nullptr);
			globalitem_index += 1;

			AddItemFunc(globalitem_index, ItemType::_Consumable, "Tier4Gear",
				"Resources/Model/ItemModel/Tier4Gear.model",
				L"Resources/UI/ItemIcons/ItemIcon_Tier4Gear.png",
				L"[티어4 부품] : 티어 4 도구 제작에 사용되는 주요재료. 많이 모으면 무기나 도구를 제작하는데 도움이 된다.", nullptr);
			globalitem_index += 1;

			Model* Sniper_IronSight = AddItemFunc(globalitem_index, ItemType::_Weapon, "Sniper_IronSight",
				"Resources/Model/sniper.model",
				L"Resources/UI/ItemIcons/ItemIcon_IronSight.png",
				L"[스나이퍼 - 아이언사이트] : 광학 장비조차 제대로 달리지 않은 구식 실탄 저격소총.", new Weapon(WeaponType::Sniper));
			globalitem_index += 1;
			game.SniperModel = Sniper_IronSight;
			PlayerWeaponObj[(int)WeaponType::Sniper] = new GameObject();
			PlayerWeaponObj[(int)WeaponType::Sniper]->worldMat = 0;
			PlayerWeaponObj[(int)WeaponType::Sniper]->SetShape(game.SniperModel);

			Model* Rifle_StreetSweeper = AddItemFunc(globalitem_index, ItemType::_Weapon, "Rifle_StreetSweeper",
				"Resources/Model/Rifle.model",
				L"Resources/UI/ItemIcons/ItemIcon_StreetSweeper.png",
				L"[돌격소총 - 스트리트스위퍼] : 한때 거리를 쓸어버렸던 클래식 돌격소총. 이제는 흔해 빠진 모델이다.", new Weapon(WeaponType::Rifle));
			globalitem_index += 1;
			game.RifleModel = Rifle_StreetSweeper;
			PlayerWeaponObj[(int)WeaponType::Rifle] = new GameObject();
			PlayerWeaponObj[(int)WeaponType::Rifle]->worldMat = 0;
			PlayerWeaponObj[(int)WeaponType::Rifle]->SetShape(game.RifleModel);

			Model* Pistol_DoubleTroble = AddItemFunc(globalitem_index, ItemType::_Weapon, "Pistol_DoubleTroble",
				"Resources/Model/pistol.model",
				L"Resources/UI/ItemIcons/ItemIcon_DoubleTroble.png",
				L"[쌍권총 - 더블트러블] : 성능은 낮지만 두 배로 쏘아붙이는 저가형 쌍권총.", new Weapon(WeaponType::Pistol));
			globalitem_index += 1;
			game.PistolModel = Pistol_DoubleTroble;
			PlayerWeaponObj[(int)WeaponType::Pistol] = new GameObject();
			PlayerWeaponObj[(int)WeaponType::Pistol]->worldMat = 0;
			PlayerWeaponObj[(int)WeaponType::Pistol]->SetShape(game.PistolModel);
			game.PistolModel->BindPose.resize(game.PistolModel->nodeCount);
			for (int i = 0; i < game.PistolModel->nodeCount; ++i) {
				game.PistolModel->BindPose[i] = game.PistolModel->Nodes[i].transform;
			}
			game.Pistol_SlideIndices.clear();
			{
				int upperIdx = game.PistolModel->FindNodeIndexByName("Upper_Part");
				if (upperIdx >= 0) {
					game.Pistol_SlideIndices.push_back(upperIdx);
					game.PistolModel->BindPose[upperIdx] = game.PistolModel->Nodes[upperIdx].transform;
				}
			}

			Model* ShotGun_SlagShot = AddItemFunc(globalitem_index, ItemType::_Weapon, "ShotGun_SlagShot",
				"Resources/Model/shootgun.model",
				L"Resources/UI/ItemIcons/ItemIcon_SlagShot.png",
				L"[샷건 - 슬래그슛] : 제련 찌꺼기를 쏘는 듯한 거칠고 어려운 총. 구형 모델이라 성능도 그다지 좋지 않다.", new Weapon(WeaponType::Shotgun));
			globalitem_index += 1;
			game.ShotGunModel = ShotGun_SlagShot;
			PlayerWeaponObj[(int)WeaponType::Shotgun] = new GameObject();
			PlayerWeaponObj[(int)WeaponType::Shotgun]->worldMat = 0;
			PlayerWeaponObj[(int)WeaponType::Shotgun]->SetShape(game.ShotGunModel);
			game.ShotGunModel->BindPose.resize(game.ShotGunModel->nodeCount);
			for (int i = 0; i < game.ShotGunModel->nodeCount; ++i) {
				game.ShotGunModel->BindPose[i] = game.ShotGunModel->Nodes[i].transform;
			}
			game.SG_PumpIndices.clear();
			{
				int pumpIdx = game.ShotGunModel->FindNodeIndexByName("handguard_low");
				if (pumpIdx >= 0) {
					game.SG_PumpIndices.push_back(pumpIdx);
					game.ShotGunModel->BindPose[pumpIdx] = game.ShotGunModel->Nodes[pumpIdx].transform;
				}
			}

			Model* MachineGun_Ratler = AddItemFunc(globalitem_index, ItemType::_Weapon, "MachineGun_Ratler",
				"Resources/Model/minigun.model",
				L"Resources/UI/ItemIcons/ItemIcon_Ratler.png",
				L"[머신건 - 라틀러] : 낡은 부품이 덜덜거리는 소리에서 따온 이름. 언제 만들어졌는지 알 수 없다.", new Weapon(WeaponType::MachineGun));
			globalitem_index += 1;
			game.MachineGunModel = MachineGun_Ratler;
			PlayerWeaponObj[(int)WeaponType::MachineGun] = new GameObject();
			PlayerWeaponObj[(int)WeaponType::MachineGun]->worldMat = 0;
			PlayerWeaponObj[(int)WeaponType::MachineGun]->SetShape(game.MachineGunModel);
			game.MachineGunModel->BindPose.resize(game.MachineGunModel->nodeCount);
			for (int i = 0; i < game.MachineGunModel->nodeCount; ++i) {
				game.MachineGunModel->BindPose[i] = game.MachineGunModel->Nodes[i].transform;
			}
			game.MG_BarrelIndices.clear();
			{
				auto addBarrel = [&](const char* name) {
					int idx = game.MachineGunModel->FindNodeIndexByName(name);
					if (idx >= 0) game.MG_BarrelIndices.push_back(idx);
					};

				addBarrel("Cylinder.107");
				addBarrel("Cylinder.108");
				addBarrel("Cylinder.109");
				addBarrel("Cylinder.110");
			}

			auto LoadJobWeaponModel = [&](const char* shapeName, const char* modelPath) -> Model* {
				Model* model = new Model();
				model->LoadModelFile2(modelPath);
				Shape::AddModel(shapeName, model);
				return model;
			};
			game.DualRevolverModel = LoadJobWeaponModel("DualRevolver", "Resources/Model/revolver.model");
			game.DualGunBladeModel = LoadJobWeaponModel("DualGunBlade", "Resources/Model/GunBlade.model");
			game.HackerSMGModel = LoadJobWeaponModel("HackerSMG", "Resources/Model/smg.model");
			game.BomberGrenadeGunModel = LoadJobWeaponModel("BomberGrenadeGun", "Resources/Model/granadeGun.model");

			auto RegisterPlayerWeaponObject = [&](WeaponType type, Model* model) {
				const int weaponIndex = (int)type;
				if (weaponIndex < 0 || weaponIndex >= Game::MaxWeapon || model == nullptr) return;
				PlayerWeaponObj[weaponIndex] = new GameObject();
				PlayerWeaponObj[weaponIndex]->worldMat = 0;
				PlayerWeaponObj[weaponIndex]->SetShape(model);
			};
			RegisterPlayerWeaponObject(WeaponType::DualPistol, game.DualRevolverModel);
			RegisterPlayerWeaponObject(WeaponType::DronePistol, game.PistolModel);
			RegisterPlayerWeaponObject(WeaponType::SMG, game.HackerSMGModel);
			RegisterPlayerWeaponObject(WeaponType::GrenadeGun, game.BomberGrenadeGunModel);
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
	vec4 offset = GetRenderedZoneOffset(go->zoneId);
	go->worldMat.pos += offset;
	go->worldMat.pos.w = 1.0f;
}

void Game::ApplyZoneOffsetToDynamicObject(DynamicGameObject* go)
{
	if (go == nullptr) return;
	vec4 offset = GetRenderedZoneOffset(go->zoneId);
	go->DestPos += offset;
	go->DestPos.w = 1.0f;
	go->worldMat.pos = go->DestPos;
	go->worldMat.pos.w = 1.0f;
}

void Game::ApplyZoneOffsetToPortal(Portal* portal)
{
	if (portal == nullptr) return;
	vec4 offset = GetRenderedZoneOffset(portal->zoneId);
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
		if (nearzone->isMapLoaded) continue;

			loadingMap = true;
			if (nearzone->Map == nullptr) nearzone->Map = new GameMap();
			bool isLoadSuccess = nearzone->Map->LoadMap(nearzone->Load_MapName, nearzone->zoneid);
			if (isLoadSuccess == false) {
				nearzone->Map->Release();
				continue;
			}

			//push object to chunck and bake aabb
			nearzone->Map->AABB[0] = INFINITY;
			nearzone->Map->AABB[1] = -INFINITY;
			for (int u = 0; u < nearzone->Map->MapObjects.size(); ++u) {
				nearzone->PushGameObject(nearzone->Map->MapObjects[u]);
				int off = nearzone->Asset_OffsetMul;
				game.StaticGameObjects[Zone::MaxStaticObjectCount * off + u] = nearzone->Map->MapObjects[u];
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

			nearzone->GetZoneChunkGOIC();

			nearzone->isMapLoaded = true;
			nearzone->bReqireBakeLight_Raster = true;
			nearzone->bReqireBakeLight_Raytracing = true;
	}

	if (loadingMap == false) return;
	else {
		PrebuildStaticObjectAutoLOD();
	}
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
	isServerSyncComplete = false;
	isInitialJobConfirmed = true;
	clientIndexInServer = -1;
	playerGameObjectIndex = -1;
	player = nullptr;

	// Only open-world -> open-world transfers may suppress loading. Dungeon exits switch to a
	// fundamentally different map and must wait for map/global/server snapshot readiness.
	{
		extern bool g_suppressLoadingScreen;
		const bool sourceWasDungeon = currentZoneId >= 100 && currentZoneId < 106;
		g_suppressLoadingScreen = !sourceWasDungeon && dstZoneId < 100;
	}

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
	DWORD _sent = client.send_all((char*)&header, sizeof(CTS_TransferConnect_Header), 0, 250);
	{
		char _dbg[128] = {};
		sprintf_s(_dbg, "[BeginServerTransfer] sent TransferConnect bytes=%u (lastErr=%d)\n",
			(unsigned)_sent, WSAGetLastError());
		OutputDebugStringA(_dbg); printf("%s", _dbg); fflush(stdout);
	}
	if (_sent != sizeof(CTS_TransferConnect_Header)) {
		client.Disconnect();
		return false;
	}

	// A server transfer creates a new authoritative player object. Retaining the source-zone
	// dynamics here resurrects the old local player/ghost beside the destination player.
	MoveZone(dstZoneId, false);
	// Receiving AllocPlayerIndexes is the authoritative completion signal. Marking this true here
	// allowed a rotated camera with no bound player when transfer setup was delayed or failed.
	isPrepared = IsPresentationReady();
	return true;
}

// [party/dungeon][phase2] Portal-teleport: drop the current connection and reconnect FRESH to the
// dungeon server. The dungeon server creates a brand-new player (state reset), like a first login.
void Game::BeginPortalEnter(const char* ip, unsigned short port, int dstZoneId)
{
	char ipLocal[64] = {};
	if (ip) strncpy_s(ipLocal, ip, _TRUNCATE);

	isPrepared = false;
	isPreparedClientIndex = false;
	isMapInit = false;
	isServerSyncComplete = false;
	isInitialJobConfirmed = true;
	clientIndexInServer = -1;
	playerGameObjectIndex = -1;
	player = nullptr;

	client.Disconnect();
	bool connected = client.Init(ipLocal, port);
	if (connected == false) return;

	CTS_ClientHello_Header hello;
	hello.size = sizeof(CTS_ClientHello_Header);
	hello.st = CTS_Protocol::ClientHello;
	strcpy_s(hello.playerId, sizeof(hello.playerId), PlayerId);
	client.send((char*)&hello, sizeof(CTS_ClientHello_Header), 0);

	MoveZone(dstZoneId);
	isPrepared = false;
}

void Game::ResendHeldMovementKeys()
{
	// [transfer] While a server transfer froze the main thread (~1s), key-down/up
	// messages piled up in the OS message queue. If left, the main loop replays
	// them all at once after the freeze (the "stale inputs fire in a burst" bug).
	// Drain them here; the correct current key state is resent right below.
	MSG _km;
	while (PeekMessage(&_km, nullptr, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE)) {}

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

void Game::MoveZone(int zoneid, bool keepObjects) {
	if (zoneid < 0 || zoneid >= (int)ZoneTable.size()) return;
	double globalRoughnessBefore = 0.0;
	for (int i = 0; i < GlobalMaterialCount; ++i) {
		if (MaterialTable[i] != nullptr) globalRoughnessBefore += MaterialTable[i]->roughnessFactor;
	}
	Zone* prevZone = Current_Zone;
	Zone* nextZone = ZoneTable[zoneid];
	if (prevZone == nullptr || nextZone == nullptr) {
		OutputDebugStringA("[MoveZone] rejected null source/destination zone\n");
		isMapInit = false;
		return;
	}
	{
		char dbg[128] = {};
		sprintf_s(dbg, "[MoveZone] src=%d dst=%d mode=%s\n", currentZoneId, zoneid,
			keepObjects ? "seamless" : "full");
		OutputDebugStringA(dbg);
		printf("%s", dbg);
		fflush(stdout);
	}

	// 다음 존의 nearZone에 들어오지 않는 존을 Release
	for (int i = 0; i < 9; ++i) {
		Zone* nz = prevZone->nearZones[i];
		if (nz != nullptr && nz->isMapLoaded) {
			bool isRelease = true;
			for (int k = 0; k < 9; ++k) {
				Zone* nzk = nextZone->nearZones[k];
				if (nz == nzk) {
					isRelease = false;
					break;
				}
			}

			if (isRelease) {
				nz->ZoneAssetRelease();
			}
		}
	}

	// [seamless] Light transfer path: keep all dynamic objects + the player, just switch the active
	// zone and load any not-yet-loaded neighbor maps (LoadLinkedZoneMaps skips already-loaded ones).
	// Old-zone objects are removed afterward by generation-based cleanup. This path is currently only
	// reached when a caller passes keepObjects=true; default callers still use the full reload below,
	// so existing behavior is unchanged until the transfer code opts in.
	if (keepObjects) {
		Current_Zone = nextZone;
		currentZoneId = zoneid;
		LoadLinkedZoneMaps();
		isMapInit = nextZone->isMapLoaded && nextZone->Map != nullptr;
		return;
	}

	// The previous frame may still reference dungeon textures, BLAS/TLAS resources, descriptors,
	// and dynamic objects. A dungeon -> open-world transfer replaces all of them, so releasing first
	// can turn the next GPU access into a device removal / client process exit.
	gd.gpucmd.WaitGPUComplete();

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

	// [seamless-B] The objects these queues referenced were just deleted; drop the stale net indices
	// so ProcessPendingSkinBoneInit never touches freed/reused slots after a transfer.
	m_pendingSkinBoneInit.clear();
	m_pendingSkinRenderEnable.clear();

	// [seamless-B] Every DynmaicGameObject was just deleted above, so ANY dynamic/skinmesh pointer
	// still sitting in ANY loaded zone's chunks is now dangling. Previously only prevZone's chunks
	// were cleared, so other zones kept stale pointers that accumulated across transfers and were
	// eventually dereferenced by the shadow pass (SkinMeshShadowRender -> garbage descindex crash).
	// Clear the dynamic + skinmesh chunk arrays for every zone. Static objects are untouched here.
	for (size_t z = 0; z < ZoneTable.size(); ++z) {
		Zone* zone = ZoneTable[z];
		if (zone == nullptr) continue;
		for (auto& chunkPair : zone->chunck) {
			GameChunk* chunk = chunkPair.second;
			if (chunk == nullptr) continue;
			chunk->Dynamic_gameobjects.Release();
			chunk->Dynamic_gameobjects.Init(32);
			chunk->SkinMesh_gameobjects.Release();
			chunk->SkinMesh_gameobjects.Init(32);
		}
	}

	player = nullptr;
	playerGameObjectIndex = -1;

	for (int i = 0; i < Portals.size(); ++i) {
		if (Portals[i]) {
			delete Portals[i];
		}
	}
	Portals.clear();

	Current_Zone = nextZone;
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

	// Preserve the committed renderer's first-load ordering: global materials are finalized only
	// after the destination map has populated and stabilized the descriptor heap. The old code did
	// this on every MoveZone, which reallocated descriptors and repeatedly changed roughness.
	// Finalize once, then only refresh the shared structured buffer on later transfers.
	if (!GlobalMaterialDescriptorsInitialized) {
		for (int i = 0; i < GlobalMaterialCount; ++i) {
			Material* mat = MaterialTable[i];
			if (mat == nullptr) continue;
			mat->SetDescTable(-1);
			RenderMaterialTable[i] = mat;
		}
		GlobalMaterialDescriptorsInitialized = true;
	}
	Material::InitMaterialStructuredBuffer(false);
	double globalRoughnessAfter = 0.0;
	for (int i = 0; i < GlobalMaterialCount; ++i) {
		if (MaterialTable[i] != nullptr) globalRoughnessAfter += MaterialTable[i]->roughnessFactor;
	}
	{
		char dbg[160] = {};
		sprintf_s(dbg, "[GlobalMaterialInvariant] roughness %.6f -> %.6f changed=%d\n",
			globalRoughnessBefore, globalRoughnessAfter,
			fabs(globalRoughnessAfter - globalRoughnessBefore) > 0.000001 ? 1 : 0);
		OutputDebugStringA(dbg);
		printf("%s", dbg);
		fflush(stdout);
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

	isMapInit = nextZone->isMapLoaded && nextZone->Map != nullptr;
	if (!isMapInit) {
		char dbg[128] = {};
		sprintf_s(dbg, "[MoveZone] destination map is not ready. zone=%d loaded=%d map=%p\n",
			zoneid, (int)nextZone->isMapLoaded, nextZone->Map);
		OutputDebugStringA(dbg);
		printf("%s", dbg);
		fflush(stdout);
	}
	isAssetAddingInGlobal = true;
	//ResendHeldMovementKeys();
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

// [loading] Standalone present that draws ONLY a fullscreen image (default Loading.png). Safe to call
// before any zone/map is loaded (does not touch Current_Zone/player). Uses the PRESENT-base-state present
// pattern so it works from a cold start and can be called every frame while the game is not yet prepared.
void Game::DrawLoadingScreen(GPUResource* tex) {
	GPUResource* img = tex ? tex : &gLoadingTex;
	if (img == nullptr || img->resource == nullptr) return;

	if (gd.gpucmd.isClose) {
		gd.gpucmd.Reset();
	}
	gd.ShaderVisibleDescPool.DynamicReset();

	gd.gpucmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET);
	/*gd.gpucmd.ResBarrierTr(gd.pDepthStencilBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);*/

	gd.gpucmd->RSSetViewports(1, &gd.viewportArr[0].Viewport);
	gd.gpucmd->RSSetScissorRects(1, &gd.viewportArr[0].ScissorRect);
	game.renderViewPort = &gd.viewportArr[0];

	D3D12_CPU_DESCRIPTOR_HANDLE dsv = gd.pDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	gd.gpucmd->OMSetRenderTargets(1, &gd.SubRenderTarget.rtvHandle.hcpu, TRUE, &dsv);

	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	gd.gpucmd->ClearRenderTargetView(gd.SubRenderTarget.rtvHandle.hcpu, clearColor, 0, NULL);
	gd.gpucmd->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);

	gd.gpucmd.SetShader(game.MyScreenShader, ShaderType::RenderNormal);
	// UI coords are center-origin pixels, range +-(W/2, H/2); the texture maps across the whole rect.
	float _lw = (float)gd.ClientFrameWidth * 0.5f;
	float _lh = (float)gd.ClientFrameHeight * 0.5f;
	game.UIDraw_TextureRect(vec4(-_lw, -_lh, _lw, _lh), vec4(1, 1, 1, 1), 0.001f, img);

	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
	gd.gpucmd.ResBarrierTr(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
	gd.gpucmd->CopyResource(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], gd.SubRenderTarget.resource);
	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	gd.gpucmd.ResBarrierTr(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
	/*gd.gpucmd.ResBarrierTr(gd.pDepthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);*/

	gd.gpucmd.Close();
	gd.gpucmd.Execute();
	gd.gpucmd.WaitGPUComplete();
	gd.pSwapChain->Present(1, 0);
	gd.CurrentSwapChainBufferIndex = gd.pSwapChain->GetCurrentBackBufferIndex();
}

void Game::DrawStartScreen() { DrawLoadingScreen(gStartScreenTex.resource ? &gStartScreenTex : &gLoadingTex); }   // [loading] start screen image

void Game::DrawStartScreen(const char* playerId, bool idInvalid, bool idTooLong) {
	GPUResource* img = gStartScreenTex.resource ? &gStartScreenTex : &gLoadingTex;
	if (img == nullptr || img->resource == nullptr) return;

	if (gd.gpucmd.isClose) {
		gd.gpucmd.Reset();
	}
	gd.ShaderVisibleDescPool.DynamicReset();

	gd.gpucmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET);

	gd.gpucmd->RSSetViewports(1, &gd.viewportArr[0].Viewport);
	gd.gpucmd->RSSetScissorRects(1, &gd.viewportArr[0].ScissorRect);
	game.renderViewPort = &gd.viewportArr[0];

	D3D12_CPU_DESCRIPTOR_HANDLE dsv = gd.pDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	gd.gpucmd->OMSetRenderTargets(1, &gd.SubRenderTarget.rtvHandle.hcpu, TRUE, &dsv);

	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	gd.gpucmd->ClearRenderTargetView(gd.SubRenderTarget.rtvHandle.hcpu, clearColor, 0, NULL);
	gd.gpucmd->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);

	gd.gpucmd.SetShader(game.MyScreenShader, ShaderType::RenderNormal);

	for (int i = 0; i < gd.addSDFTextureStack.size(); ++i) {
		gd.AddTextSDFTexture(gd.addSDFTextureStack[i]);
	}
	gd.addSDFTextureStack.clear();

	for (int i = gd.SDFTextureArr_immortalSize; i < gd.SDFTextureArr.size(); ++i) {
		SDFTextPageTextureBuffer* page = gd.SDFTextureArr[i];
		if (page != nullptr) {
			page->BakeSDF();
		}
	}

	game.MyScreenShader->ClearSDFInstance();
	if (game.MyScreenShader->SDFMappedCnt == 0 && game.MyScreenShader->SDFInstance_StructuredBuffer.resource != nullptr) {
		game.MyScreenShader->SDFInstance_StructuredBuffer.resource->Map(0, nullptr, (void**)&game.MyScreenShader->MappedSDFInstance);
		game.MyScreenShader->SDFMappedCnt += 1;
	}

	const float halfW = (float)gd.ClientFrameWidth * 0.5f;
	const float halfH = (float)gd.ClientFrameHeight * 0.5f;

	const float panelW = min(620.0f, (float)gd.ClientFrameWidth * 0.72f);
	const float panelH = 178.0f;
	const float panelY = -halfH * 0.42f;
	const vec4 panel = vec4(-panelW * 0.5f, panelY, panelW * 0.5f, panelY + panelH);
	const bool hasText = playerId != nullptr && playerId[0] != '\0';

	const vec4 inputRect = vec4(panel.x + 36.0f, panel.y + 72.0f, panel.z - 36.0f, panel.y + 118.0f);
	const vec4 inputColor = (idInvalid || idTooLong) ? vec4(0.23f, 0.035f, 0.035f, 0.96f) : vec4(0.02f, 0.075f, 0.10f, 0.96f);

	game.UIDraw_TextureRect(vec4(-halfW, -halfH, halfW, halfH), vec4(1, 1, 1, 1), 0.900f, img);
	game.UIDraw_SolidRect(panel + vec4(6.0f, -6.0f, 6.0f, -6.0f), vec4(0, 0, 0, 0.48f), 0.24f);
	game.UIDraw_SolidRect(panel, vec4(0.015f, 0.035f, 0.048f, 0.92f), 0.23f);
	game.UIDraw_SolidRect(vec4(panel.x, panel.w - 3.0f, panel.z, panel.w), vec4(0.10f, 0.70f, 1.0f, 0.95f), 0.22f);

	game.UIDraw_SolidRect(inputRect + vec4(3.0f, -3.0f, 3.0f, -3.0f), vec4(0, 0, 0, 0.40f), 0.19f);
	game.UIDraw_SolidRect(inputRect, inputColor, 0.18f);
	game.UIDraw_SolidRect(vec4(inputRect.x, inputRect.y, inputRect.z, inputRect.y + 2.0f), vec4(0.10f, 0.78f, 0.96f, 0.95f), 0.17f);
	game.UIDraw_SolidRect(vec4(inputRect.x, inputRect.w - 2.0f, inputRect.z, inputRect.w), vec4(0.10f, 0.78f, 0.96f, 0.95f), 0.17f);
	game.UIDraw_SolidRect(vec4(inputRect.x, inputRect.y, inputRect.x + 2.0f, inputRect.w), vec4(0.10f, 0.78f, 0.96f, 0.95f), 0.17f);
	game.UIDraw_SolidRect(vec4(inputRect.z - 2.0f, inputRect.y, inputRect.z, inputRect.w), vec4(0.10f, 0.78f, 0.96f, 0.95f), 0.17f);

	char idText[CTS_ClientHello_Header::MaxPlayerIdLength + 2] = {};
	if (playerId != nullptr && playerId[0] != '\0') {
		strcpy_s(idText, playerId);
		strcat_s(idText, "_");
	}
	else {
		strcpy_s(idText, "TYPE ID");
	}
	wchar_t idWide[CTS_ClientHello_Header::MaxPlayerIdLength + 2] = {};
	for (int i = 0; i < CTS_ClientHello_Header::MaxPlayerIdLength + 1 && idText[i] != '\0'; ++i) {
		idWide[i] = (wchar_t)idText[i];
	}

	RenderSDFText(L"PLAYER ID", 9, vec4(panel.x + 38.0f, panel.w - 44.0f, panel.z - 38.0f, panel.w - 14.0f),
		24.0f, vec4(0.52f, 0.92f, 1.0f, 1.0f), nullptr, nullptr, 0.15f);
	RenderSDFText(idWide, (int)wcslen(idWide), vec4(inputRect.x + 24.0f, inputRect.y + 8.0f, inputRect.z - 12.0f, inputRect.w - 4.0f),
		28.0f, hasText ? vec4(0.95f, 1.0f, 1.0f, 1.0f) : vec4(0.38f, 0.68f, 0.78f, 1.0f), nullptr, nullptr, 0.14f);

	RenderSDFText(L"A Z 0 9 ONLY", 12, vec4(panel.x + 38.0f, panel.y + 34.0f, panel.z - 38.0f, panel.y + 60.0f),
		18.0f, vec4(0.76f, 0.86f, 0.92f, 1.0f), nullptr, nullptr, 0.15f);

	if (hasText) {
		RenderSDFText(L"PRESS ENTER", 11, vec4(panel.x + 38.0f, panel.y + 8.0f, panel.z - 38.0f, panel.y + 32.0f),
			19.0f, vec4(0.96f, 0.82f, 0.34f, 1.0f), nullptr, nullptr, 0.15f);
	}
	MyScreenShader->RenderAllSDFTexts();

	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
	gd.gpucmd.ResBarrierTr(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
	gd.gpucmd->CopyResource(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], gd.SubRenderTarget.resource);
	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	gd.gpucmd.ResBarrierTr(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);

	gd.gpucmd.Close();
	gd.gpucmd.Execute();
	gd.gpucmd.WaitGPUComplete();
	gd.pSwapChain->Present(1, 0);
	gd.CurrentSwapChainBufferIndex = gd.pSwapChain->GetCurrentBackBufferIndex();
}

// [jobselect] Computes the 9 job-card rects (3x3 grid) and the two confirm/cancel button rects, in the
// same center-origin, y-up pixel space that UIDraw_TextureRect and game.CurrentCursorPos use.
void Game::ComputeJobSelectLayout(vec4 cardRects[9], vec4& confirmRect, vec4& cancelRect) {
	const float W = (float)gd.ClientFrameWidth;
	const float H = (float)gd.ClientFrameHeight;
	const float aspect = 1672.0f / 941.0f;   // shared aspect of every card image and Confirm.png

	const float gxHalf = 0.28f * W;
	const float gyTop  =  0.37f * H;
	const float gyBot  = -0.18f * H;
	const float regionW = 2.0f * gxHalf;
	const float regionH = gyTop - gyBot;
	const float cellW = regionW / 3.0f;
	const float cellH = regionH / 3.0f;

	const float pad = 0.10f;
	const float availW = cellW * (1.0f - pad);
	const float availH = cellH * (1.0f - pad);
	float cardW, cardH;
	if (availW / availH > aspect) { cardH = availH; cardW = availH * aspect; }
	else                          { cardW = availW; cardH = availW / aspect; }

	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			const int idx = r * 3 + c;
			const float cx = -gxHalf + cellW * ((float)c + 0.5f);
			const float cy =  gyTop  - cellH * ((float)r + 0.5f);
			cardRects[idx] = vec4(cx - cardW * 0.5f, cy - cardH * 0.5f,
			                      cx + cardW * 0.5f, cy + cardH * 0.5f);
		}
	}

	// CONFIRM(blue) x in [0.057,0.476], CANCEL(red) x in [0.524,0.942] of the Confirm.png image width.
	const float barW = 0.26f * W;
	const float barH = barW / aspect;
	const float barCy = -0.36f * H;
	const float barL = -barW * 0.5f;
	const float barB = barCy - barH * 0.5f;
	const float barT = barCy + barH * 0.5f;
	confirmRect = vec4(barL + barW * 0.057f, barB, barL + barW * 0.476f, barT);
	cancelRect  = vec4(barL + barW * 0.524f, barB, barL + barW * 0.942f, barT);
}

// [jobselect] Renders one frame of the job-selection screen (background + 9 cards + CONFIRM/CANCEL bar).
void Game::DrawJobSelectScreen(int hovered, int selected, bool confirmHover, bool cancelHover) {
	if (gSelectJobBgTex.resource == nullptr) { DrawStartScreen(); return; }

	gd.gpucmd.Reset();
	gd.ShaderVisibleDescPool.DynamicReset();
	gd.gpucmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET);
	/*gd.gpucmd.ResBarrierTr(gd.pDepthStencilBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);*/

	gd.gpucmd->RSSetViewports(1, &gd.viewportArr[0].Viewport);
	gd.gpucmd->RSSetScissorRects(1, &gd.viewportArr[0].ScissorRect);
	game.renderViewPort = &gd.viewportArr[0];

	D3D12_CPU_DESCRIPTOR_HANDLE dsv = gd.pDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	gd.gpucmd->OMSetRenderTargets(1, &gd.SubRenderTarget.rtvHandle.hcpu, TRUE, &dsv);

	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	gd.gpucmd->ClearRenderTargetView(gd.SubRenderTarget.rtvHandle.hcpu, clearColor, 0, NULL);
	gd.gpucmd->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);

	gd.gpucmd.SetShader(game.MyScreenShader, ShaderType::RenderNormal);

	for (int i = 0; i < gd.addSDFTextureStack.size(); ++i) {
		gd.AddTextSDFTexture(gd.addSDFTextureStack[i]);
	}
	gd.addSDFTextureStack.clear();

	for (int i = gd.SDFTextureArr_immortalSize; i < gd.SDFTextureArr.size(); ++i) {
		SDFTextPageTextureBuffer* page = gd.SDFTextureArr[i];
		page->BakeSDF();
	}

	game.MyScreenShader->ClearSDFInstance();
	if (game.MyScreenShader->SDFMappedCnt == 0) {
		game.MyScreenShader->SDFInstance_StructuredBuffer.resource->Map(0, nullptr, (void**)&game.MyScreenShader->MappedSDFInstance);
		game.MyScreenShader->SDFMappedCnt += 1;
	}

	const float _lw = (float)gd.ClientFrameWidth * 0.5f;
	const float _lh = (float)gd.ClientFrameHeight * 0.5f;
	game.UIDraw_TextureRect(vec4(-_lw, -_lh, _lw, _lh), vec4(1, 1, 1, 1), 0.900f, &gSelectJobBgTex);

	vec4 cards[9], confirmR, cancelR;
	ComputeJobSelectLayout(cards, confirmR, cancelR);
	for (int i = 0; i < 9; ++i) {
		const bool isSel = (i == selected);
		const bool isHov = (i == hovered);
		vec4 tint;
		if (isSel)      tint = vec4(1.00f, 1.00f, 1.00f, 1.0f);
		else if (isHov) tint = vec4(0.92f, 0.92f, 0.98f, 1.0f);
		else            tint = vec4(0.55f, 0.55f, 0.62f, 1.0f);
		vec4 rc = cards[i];
		if (isSel || isHov) {
			const float gx = 0.06f * (rc.z - rc.x);
			const float gy = 0.06f * (rc.w - rc.y);
			rc = vec4(rc.x - gx, rc.y - gy, rc.z + gx, rc.w + gy);
		}
		game.UIDraw_TextureRect(rc, tint, isSel ? 0.30f : 0.50f, &gJobCardTex[i]);
	}

	const float barW = 0.26f * (float)gd.ClientFrameWidth;
	const float barL = -barW * 0.5f;
	const vec4 barRect = vec4(barL, confirmR.y, barL + barW, confirmR.w);
	const vec4 barTint = (selected >= 0) ? vec4(1.0f, 1.0f, 1.0f, 1.0f) : vec4(0.62f, 0.62f, 0.68f, 1.0f);
	game.UIDraw_TextureRect(barRect, barTint, 0.25f, &gConfirmTex);
	MyScreenShader->RenderAllSDFTexts();

	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
	gd.gpucmd.ResBarrierTr(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
	gd.gpucmd->CopyResource(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], gd.SubRenderTarget.resource);
	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	gd.gpucmd.ResBarrierTr(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
	//gd.gpucmd.ResBarrierTr(gd.pDepthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	gd.gpucmd.Close();
	gd.gpucmd.Execute();
	gd.gpucmd.WaitGPUComplete();
	gd.pSwapChain->Present(1, 0);
	gd.CurrentSwapChainBufferIndex = gd.pSwapChain->GetCurrentBackBufferIndex();
}

void Game::Render() {
	PerfGPUWaitMs = 0.0;
	PerfGPUPreWaitMs = 0.0;
	PerfGPUShadowWaitMs = 0.0;
	PerfGPUMainWaitMs = 0.0;
	PerfGPUComputeWaitMs = 0.0;
	PerfGPUFinalWaitMs = 0.0;
	PerfPresentMs = 0.0;
	PerfAutoLODMainInstances = 0;
	PerfAutoLODMainDraws = 0;
	PerfAutoLODMainTrimmedSubMeshes = 0;
	PerfAutoLODShadowSourceSubMeshes = 0;
	PerfAutoLODShadowRenderedSubMeshes = 0;
	PerfAutoLODShadowCulledObjects = 0;
	PerfAutoLODShadowCulledSubMeshes = 0;
	PerfRaytracingLODVisitedObjects = 0;
	PerfRaytracingLODDescMisses = 0;
	PerfRaytracingLODTypeRejects = 0;
	PerfRaytracingLODDistanceRejects = 0;
	PerfRaytracingLODMeshMisses = 0;
	PerfRaytracingLODNoMeshMisses = 0;
	PerfRaytracingLODReductionRejects = 0;
	PerfRaytracingLODBLASMisses = 0;
	PerfRaytracingLODHitGroupMisses = 0;
	PerfRaytracingLODCheckedMeshes = 0;
	PerfRaytracingLODAppliedMeshes = 0;
	// 1. DRED 활성화
	D3D12EnableExperimentalFeatures(1, &D3D12ExperimentalShaderModels, nullptr, nullptr);

	for (int i = 0; i < 9; ++i) {
		Zone* nearzone = game.Current_Zone->nearZones[i];
		if (nearzone == nullptr) continue;

		if (nearzone->isMapLoaded && (nearzone->bReqireBakeLight_Raster && nearzone->bReqireBakeLight_Raytracing)) {
			nearzone->LightBake();
		}

		if (nearzone->isMapLoaded && nearzone->bReqireBakeLight_Raster) {
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
	dbgc[10] += 1;
	if (game.MyScreenShader->SDFMappedCnt == 0) {
		game.MyScreenShader->SDFInstance_StructuredBuffer.resource->Map(0, nullptr, (void**)&game.MyScreenShader->MappedSDFInstance);
		game.MyScreenShader->SDFMappedCnt += 1;
	}
	
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
		MeasureGPUWait(PerfGPUPreWaitMs, [&]() { gd.gpucmd.WaitGPUComplete(); });
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

	// UI is rendered with the same depth buffer at the end of the previous
	// frame. Clear that depth before drawing the far-plane skybox so full-screen
	// overlays cannot mask the sky on the following frame.
	gd.gpucmd->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);

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
	ClearAutoLODInstancing();

	if (true) {
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

			// Batch only generated AutoLOD meshes. Unresolved objects remain on the
			// original path, so enabling LOD cannot make them disappear.
			gd.gpucmd.SetShader(MyPBRShader1, ShaderType::InstancingWithShadow);
			game.PresentShaderType = ShaderType::InstancingWithShadow;
			gd.gpucmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
			{
				using PRID = PBRShader1::RootParamId;
				gd.gpucmd->SetGraphicsRoot32BitConstants(PRID::Const_Camera, 16, &view, 0);
				gd.gpucmd->SetGraphicsRoot32BitConstants(PRID::Const_Camera, 4, &gd.viewportArr[0].Camera_Pos, 16);
				gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::CBVTable_Instancing_DirLightData, game.DirLightResCBV.hRender.hgpu);
				gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_Instancing_MaterialPool, Material::MaterialStructuredBufferSRV.hRender.hgpu);
				gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_Instancing_ShadowMap, game.MyDirLight[0].descindex.hRender.hgpu);
				DescIndex texarrSRV = DescIndex(true, gd.ShaderVisibleDescPool.TextureSRVStart);
				gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_Instancing_MaterialTexturePool, texarrSRV.hRender.hgpu);
			}
			RenderAutoLODInstancing(gd.gpucmd);

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

			// Dungeon players can temporarily fall out of the camera-driven chunk tour while their
			// interpolated OBB crosses a chunk boundary (most visible during jump). Render only the
			// enabled skinmeshes that the tour missed; TourID prevents duplicate draws.
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
			// Job 7 owns two shoulder drones. They are separate support models rather
			// than part of the player skin, so render them explicitly in the PBR pass.
			BindStaticPBRRenderState();
			RenderSupportDrones();
			BoxLOD_FlushQueued();
		}
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

	if(true)
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

		// [party/dungeon] feed swirling particles at every portal each frame.
		for (int i = 0; i < Portals.size(); ++i) {
			if (Portals[i] == nullptr) continue;
			SpawnPortalParticles(Portals[i]->worldMat.pos);
		}
		// [PORTAL-DBG] throttled: confirms client has the portal + particles are being produced.
		{
			static int _pdbg = 0;
			if (++_pdbg % 120 == 0) {
				char _d[256] = {};
				vec4 pp = (Portals.size() > 0 && Portals[0]) ? Portals[0]->worldMat.pos : vec4(0);
				sprintf_s(_d, "[PORTAL-DBG] Portals=%d portalParticles=%d pool=%d pos0=(%.1f,%.1f,%.1f)\n",
					(int)Portals.size(), (int)gPortalParticles.size(), (int)gPortalPool.Count, pp.x, pp.y, pp.z);
				OutputDebugStringA(_d); printf("%s", _d);
			}
		}

		TickTransientParticles(gMuzzleFlashParticles, DeltaTime);
		TickTransientParticles(gBulletTracerParticles, DeltaTime);
		TickTransientParticles(gIceProjectileParticles, DeltaTime);
		TickTransientParticles(gPortalParticles, DeltaTime);
		TickTransientParticles(gBossExplosionParticles, DeltaTime);
		TickTransientParticles(gStatusIceParticles, DeltaTime);
		TickTransientParticles(gStatusFireParticles, DeltaTime);
		TickTransientParticles(gStatusYellowParticles, DeltaTime);
		TickTransientParticles(gStatusHackParticles, DeltaTime);
		TickTransientParticles(gStatusHealParticles, DeltaTime);
		UploadTransientParticles(gMuzzleFlashPool, gMuzzleFlashUpload, gMuzzleFlashParticles);
		UploadTransientParticles(gBulletTracerPool, gBulletTracerUpload, gBulletTracerParticles);
		UploadTransientParticles(gIceProjectilePool, gIceProjectileUpload, gIceProjectileParticles);
		UploadTransientParticles(gPortalPool, gPortalUpload, gPortalParticles);
		UploadTransientParticles(gBossExplosionPool, gBossExplosionUpload, gBossExplosionParticles);
		UploadTransientParticles(gStatusIcePool, gStatusIceUpload, gStatusIceParticles);
		UploadTransientParticles(gStatusFirePool, gStatusFireUpload, gStatusFireParticles);
		UploadTransientParticles(gStatusYellowPool, gStatusYellowUpload, gStatusYellowParticles);
		UploadTransientParticles(gStatusHackPool, gStatusHackUpload, gStatusHackParticles);
		UploadTransientParticles(gStatusHealPool, gStatusHealUpload, gStatusHealParticles);

		ParticleDraw->FireTexture = GetParticleSpriteResource(gMuzzleFlashSpriteSlot);
		ParticleDraw->Render(gd.gpucmd, &gMuzzleFlashPool.Buffer, gMuzzleFlashPool.Count);

		ParticleDraw->FireTexture = GetParticleSpriteResource(gBulletTracerSpriteSlot);
		ParticleDraw->Render(gd.gpucmd, &gBulletTracerPool.Buffer, gBulletTracerPool.Count);

		ParticleDraw->FireTexture = &gParticleIceTexture;
		ParticleDraw->Render(gd.gpucmd, &gIceProjectilePool.Buffer, gIceProjectilePool.Count);

		ParticleDraw->FireTexture = &gParticleIceTexture;
		ParticleDraw->Render(gd.gpucmd, &gPortalPool.Buffer, gPortalPool.Count);

		ParticleDraw->FireTexture = &gParticleIceTexture;
		ParticleDraw->Render(gd.gpucmd, &gStatusIcePool.Buffer, gStatusIcePool.Count);

		ParticleDraw->FireTexture = GetParticleSpriteResource(ParticleSpriteSlot::FireSecondary);
		ParticleDraw->Render(gd.gpucmd, &gStatusFirePool.Buffer, gStatusFirePool.Count);

		ParticleDraw->FireTexture = GetParticleSpriteResource(ParticleSpriteSlot::Yellow);
		ParticleDraw->Render(gd.gpucmd, &gStatusYellowPool.Buffer, gStatusYellowPool.Count);

		ParticleDraw->FireTexture = GetParticleSpriteResource(ParticleSpriteSlot::Hack);
		ParticleDraw->Render(gd.gpucmd, &gStatusHackPool.Buffer, gStatusHackPool.Count);

		ParticleDraw->FireTexture = GetParticleSpriteResource(ParticleSpriteSlot::Heal);
		ParticleDraw->Render(gd.gpucmd, &gStatusHealPool.Buffer, gStatusHealPool.Count);

		RenderBossPrototypeObjects();
		RenderBossPrototypeShield();
		RenderAegisShieldVisuals();
		RenderFrostBlizzardSnowWaves();
		RenderStatusEffectOverlays();
		RenderBossPrototypeMissiles();
		RenderBossPrototypeBeam();
		RenderRailgunOrbitVisuals();
		RenderDualBladeFloorVisuals();
		RenderHackerEmpVisuals();
		RenderPlayerRailgunBeams();
		RenderBossPrototypeAoEs();
		ParticleDraw->FireTexture = GetParticleSpriteResource(gBossExplosionSpriteSlot);
		ParticleDraw->Render(gd.gpucmd, &gBossExplosionPool.Buffer, gBossExplosionPool.Count);
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
			// [party/dungeon] portal is drawn as particles now (see SpawnPortalParticles); skip the cube mesh.
			//mesh->Render(gd.gpucmd, 1);
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
	MeasureGPUWait(PerfGPUMainWaitMs, [&]() { gd.gpucmd.WaitGPUComplete(); });

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
		// Skin matrices computed here are displayed on the next render pass.
		// Cache weapon attachments at the same point so the weapon and hand use
		// the same animation/world frame instead of drifting by one frame.
		for (SkinMeshGameObject* skinObject : SkinMeshGameObject::collection) {
			Player* playerObject = dynamic_cast<Player*>(skinObject);
			if (playerObject != nullptr) {
				playerObject->UpdateThirdPersonWeaponAttachmentCache();
			}
		}
	}

	hResult = gd.CScmd.Close();
	gd.CScmd.Execute();
	MeasureGPUWait(PerfGPUComputeWaitMs, [&]() { gd.CScmd.WaitGPUComplete(); });

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
	// Composite the full-screen damage feedback first. Screen UI currently uses
	// a depth-writing pipeline, so clear only its depth afterward and draw every
	// HUD element and SDF label cleanly on top of the retained color overlay.
	RenderDamageFeedbackHUD();
	gd.gpucmd->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);
	// The scope belongs above the world but below every gameplay HUD element.
	// Clear only depth again so ammo/cooldown SDF text cannot be rejected by it.
	RenderSniperScopeHUD();
	gd.gpucmd->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);


	if (game.mainPageStack.size() == 0) {
		RenderGameplayStatusHUD();
		RenderDungeonPartyHUD();
		RenderPartyLobbyUI();
		RenderBossPrototypeHUD();
		RenderBossPrototypeCoreHealthPlates();
		RenderMonsterHealthPlates();
		RenderFloatingDamageTexts();
		RenderGameplaySkillHUD();
		RenderAmmoHUD();
		RenderQuestCompleteShow();
		RenderQuestPrograssShow();
	}
	RenderDungeonDeathHUD();
	vector<DXPage*>* savePageStack = game.CurrentPageStack;
	game.CurrentPageStack = &game.mainPageStack;
	for (int i = 0; i < game.CurrentPageStack->size(); ++i) {
		DXPage* page = game.CurrentPageStack->at(i);
		page->Render();
	}
	//render Current Grab Slot
	if(game.CurrentPageStack->size() > 0 && CurrentGrabSlotData.itemCnt > 0 && CurrentGrabSlotData.objid > 0)
	{
		constexpr float margin = 40.0f;
		vec4 renderLoc = CurrentCursorPos;
		renderLoc.z = renderLoc.x;
		renderLoc.w = renderLoc.y;
		renderLoc.z += 2*margin;
		renderLoc.w += 2*margin;
		wchar_t NumText[64] = {};
		if (CurrentGrabSlotData.itemCnt > 0) {
			Item& item = ItemTable[CurrentGrabSlotData.objid];
			game.UIDraw_TextureRect(renderLoc, vec4(1, 1, 1, 1), 0.01f, item.icon);
		}

		_itow_s(CurrentGrabSlotData.itemCnt, NumText, 64, 10);
		game.RenderSDFText(NumText, wcslen(NumText), renderLoc, 15, vec4(1, 1, 1, 1), nullptr, nullptr, 0.001f);
	}
	
	// 27.
	// UI quads write depth. Clear it once more so the batched SDF pass (HP/ammo/keys/party labels)
	// cannot disappear behind dungeon HUD textures or map-dependent depth state.
	gd.gpucmd->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);
	MyScreenShader->RenderAllSDFTexts();

	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
	gd.gpucmd.ResBarrierTr(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);

	gd.gpucmd->CopyResource(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], gd.SubRenderTarget.resource);

	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	gd.gpucmd.ResBarrierTr(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);

	//Command execution
	hResult = gd.gpucmd.Close();
	gd.gpucmd.Execute();
	MeasureGPUWait(PerfGPUFinalWaitMs, [&]() { gd.gpucmd.WaitGPUComplete(); });

	// Present to Swapchain BackBuffer & RenderTargetIndex Update
	DXGI_PRESENT_PARAMETERS dxgiPresentParameters;
	dxgiPresentParameters.DirtyRectsCount = 0;
	dxgiPresentParameters.pDirtyRects = NULL;
	dxgiPresentParameters.pScrollRect = NULL;
	dxgiPresentParameters.pScrollOffset = NULL;
	MeasurePresent([&]() { gd.pSwapChain->Present1(1, 0, &dxgiPresentParameters); });

	//Get Present RenderTarget Index
	gd.CurrentSwapChainBufferIndex = gd.pSwapChain->GetCurrentBackBufferIndex();

	gd.DeviceRemoveResonDebug();
}

int Maped = 0;
void Game::Render_RayTracing()
{
	//dbgbreak(game.currentZoneId == 74);
	PerfGPUWaitMs = 0.0;
	PerfGPUPreWaitMs = 0.0;
	PerfGPUShadowWaitMs = 0.0;
	PerfGPUMainWaitMs = 0.0;
	PerfGPUComputeWaitMs = 0.0;
	PerfGPUFinalWaitMs = 0.0;
	PerfPresentMs = 0.0;
	PerfRaytracingLODVisitedObjects = 0;
	PerfRaytracingLODDescMisses = 0;
	PerfRaytracingLODTypeRejects = 0;
	PerfRaytracingLODDistanceRejects = 0;
	PerfRaytracingLODMeshMisses = 0;
	PerfRaytracingLODNoMeshMisses = 0;
	PerfRaytracingLODReductionRejects = 0;
	PerfRaytracingLODBLASMisses = 0;
	PerfRaytracingLODHitGroupMisses = 0;
	PerfRaytracingLODCheckedMeshes = 0;
	PerfRaytracingLODAppliedMeshes = 0;

	RenderSupportDrones();
	RenderBossPrototypeObjects();
	game.player->Render_ThirdPersonWeapon();
	game.player->Render_AfterDepthClear();

	game.MyScreenShader->ClearSDFInstance();

	if (game.MyScreenShader->SDFMappedCnt == 0) {
		game.MyScreenShader->SDFInstance_StructuredBuffer.resource->Map(0, nullptr, (void**)&game.MyScreenShader->MappedSDFInstance);
		game.MyScreenShader->SDFMappedCnt += 1;
	}

	for (int i = 0; i < 9; ++i) {
		gd.raytracing.MappedCB[i]->DirLight_invDirection = vec4(vec4(0) - LightDirection).f3;
		gd.raytracing.MappedCB[i]->padding = AutoLOD_IsModelLODRenderActive() ? 1.0f : 0.0f;
	}

	for (int i = 0; i < 9; ++i) {
		Zone* nearZone = game.Current_Zone->nearZones[i];
		if (nearZone == nullptr) continue;

		if (nearZone->isMapLoaded && (nearZone->bReqireBakeLight_Raster && nearZone->bReqireBakeLight_Raytracing)) {
			nearZone->LightBake();
		}

		if (nearZone->isMapLoaded && nearZone->bReqireBakeLight_Raytracing) {
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

	static bool hadRaytracingMeshLOD = false;
	static const Zone* previousRaytracingMeshLODZone = nullptr;
	static uint64_t raytracingMeshLODRefreshFrame = 0;
	const bool raytracingMeshLODActive = AutoLOD_IsModelLODRenderActive();
	const bool raytracingMeshLODZoneChanged = previousRaytracingMeshLODZone != game.Current_Zone;
	const bool raytracingMeshLODPeriodicRefresh =
		raytracingMeshLODActive && ((raytracingMeshLODRefreshFrame++ % 120) == 0);
	const bool refreshRaytracingMeshLOD =
		(raytracingMeshLODActive != hadRaytracingMeshLOD) ||
		raytracingMeshLODZoneChanged ||
		raytracingMeshLODPeriodicRefresh;
	if (refreshRaytracingMeshLOD) {
		std::unordered_set<StaticGameObject*> visitedRaytracingLODObjects;
		visitedRaytracingLODObjects.reserve(16384);

		auto updateRaytracingLODObject = [&](StaticGameObject* obj) {
			if (obj == nullptr) return;
			if (!visitedRaytracingLODObjects.insert(obj).second) return;

			Mesh* mesh = nullptr;
			Model* model = nullptr;
			obj->shape.GetRealShape(mesh, model);
			if (mesh == nullptr && model == nullptr) return;

			++PerfRaytracingLODVisitedObjects;
			obj->RaytracingUpdateTransform();
		};

		for (StaticGameObject* obj : StaticGameObjects) {
			updateRaytracingLODObject(obj);
		}

		gd.viewportArr[0].UpdateFrustum();
		if (game.Current_Zone != nullptr) {
			for (auto& chunkEntry : game.Current_Zone->chunck) {
				GameChunk* chunk = chunkEntry.second;
				if (chunk == nullptr) continue;

				for (int i = 0; i < chunk->Static_gameobjects.size; ++i) {
					if (chunk->Static_gameobjects.isnull(i)) continue;
					updateRaytracingLODObject(chunk->Static_gameobjects[i]);
				}
			}

			for (int zi = 0; zi < 9; ++zi) {
				Zone* nearZone = game.Current_Zone->nearZones[zi];
				if (nearZone == nullptr || nearZone == game.Current_Zone) continue;
				if (!nearZone->isMapLoaded) continue;

				for (auto& chunkEntry : nearZone->chunck) {
					GameChunk* chunk = chunkEntry.second;
					if (chunk == nullptr) continue;
					if (!gd.viewportArr[0].m_xmFrustumWorld.Intersects(chunk->AABB)) continue;

					for (int i = 0; i < chunk->Static_gameobjects.size; ++i) {
						if (chunk->Static_gameobjects.isnull(i)) continue;
						updateRaytracingLODObject(chunk->Static_gameobjects[i]);
					}
				}
			}
		}
	}
	hadRaytracingMeshLOD = raytracingMeshLODActive;
	previousRaytracingMeshLODZone = game.Current_Zone;

	//rebuild AS
	if (true) {
		game.MyRayTracingShader->PrepareRender();
	}
	

	// Reset command list and allocator.
	ID3D12GraphicsCommandList4* commandList = gd.raytracing.dxrCommandList;

	//gd.gpucmd.pCommandAllocator->Reset(); // origin : m_commandAllocators[m_backBufferIndex]->Reset()
	//commandList->Reset(gd.gpucmd.pCommandAllocator, nullptr);
	//gd.CmdReset(commandList, gd.pCommandAllocator);
	if (gd.gpucmd.isClose) {
		gd.gpucmd.Reset(true);
	}

	gd.gpucmd.ResBarrierTr(&gd.raytracing.DepthBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	// Transition the render target into the correct state to allow for drawing into it.
	D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	commandList->ResourceBarrier(1, &barrier);

	if(true)
	{
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
	}
	
	// 3D UI ���
	{
		gd.gpucmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
		gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET);
		//9. ����Ʈ/������Ʈ ����
		gd.gpucmd->RSSetViewports(1, &gd.viewportArr[0].Viewport);
		gd.gpucmd->RSSetScissorRects(1, &gd.viewportArr[0].ScissorRect);
		game.renderViewPort = &gd.viewportArr[0];
		//10. ���� ���ٽ� ���۸� ����Ű�� �ڵ��� �����´�.
		D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle =
			gd.pDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		//11. ���귻��Ÿ������ ����Ÿ���� ����
		gd.gpucmd->OMSetRenderTargets(1, &gd.SubRenderTarget.rtvHandle.hcpu, TRUE,
			&d3dDsvCPUDescriptorHandle);
		//22 Ray ���
		{
			gd.gpucmd.SetShader(MyOnlyColorShader);
			matrix view = gd.viewportArr[0].ViewMatrix * gd.viewportArr[0].ProjectMatrix;
			view.transpose();
			gd.gpucmd->SetGraphicsRoot32BitConstants(0, 16, &view, 0);

			for (int i = 0; i < bulletRays.size; ++i) {
				if (bulletRays.isnull(i)) continue;
				bulletRays[i].Render();
			}
		}

		////23. NPC ���� HP �ٸ� ����� ���
		//for (int i = 0; i < game.NpcHPBars.size; ++i)
		//{
		//	if (game.NpcHPBars.isAlloc(i))
		//	{
		//		matrix& hpBarWorldMat = game.NpcHPBars[i];
		//		hpBarWorldMat.transpose();
		//		gd.gpucmd->SetGraphicsRoot32BitConstants(1, 16, &hpBarWorldMat, 0);
		//		game.HPBarMesh->Render(gd.gpucmd, 1);
		//	}
		//}

		matrix vmat = gd.viewportArr[0].ViewMatrix;
		vmat *= gd.viewportArr[0].ProjectMatrix;
		vmat.transpose();
		gd.gpucmd->SetGraphicsRoot32BitConstants(0, 16, &vmat, 0);

		matrix viewmat = gd.viewportArr[0].ViewMatrix.RTInverse;
		matrix spmat = viewmat;
		spmat.pos += spmat.look * 10;
		spmat.transpose();
		gd.gpucmd->SetGraphicsRoot32BitConstants(1, 16, &spmat, 0);
		ShootPointMesh->Render(gd.gpucmd, 1);
	}
	gd.gpucmd.Close(true);
	gd.gpucmd.Execute(true);
	MeasureGPUWait(PerfGPUMainWaitMs, [&]() { gd.gpucmd.WaitGPUComplete(); }); // ?
	//gd.gpucmd.WaitGPUComplete();

	HRESULT hResult;

	//Blur
	if(false)
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
		MeasureGPUWait(PerfGPUComputeWaitMs, [&]() { gd.CScmd.WaitGPUComplete(); });

		//gd.DeviceRemoveResonDebug();
	}
	
	hResult = gd.gpucmd.Reset(true);
	gd.gpucmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
	//gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET);
	//gd.gpucmd.ResBarrierTr(gd.pDepthStencilBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	gd.gpucmd.ResBarrierTr(&gd.raytracing.DepthBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	//9. ����Ʈ/������Ʈ ����
	gd.gpucmd->RSSetViewports(1, &gd.viewportArr[0].Viewport);
	gd.gpucmd->RSSetScissorRects(1, &gd.viewportArr[0].ScissorRect);
	game.renderViewPort = &gd.viewportArr[0];

	//10. ���� ���ٽ� ���۸� ����Ű�� �ڵ��� �����´�.
	D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle =
		gd.pDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	//11. ���귻��Ÿ������ ����Ÿ���� ����
	gd.gpucmd->OMSetRenderTargets(1, &gd.SubRenderTarget.rtvHandle.hcpu, TRUE,
		&d3dDsvCPUDescriptorHandle);

	if (true)
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

		// [party/dungeon] feed swirling particles at every portal each frame.
		for (int i = 0; i < Portals.size(); ++i) {
			if (Portals[i] == nullptr) continue;
			SpawnPortalParticles(Portals[i]->worldMat.pos);
		}
		// [PORTAL-DBG] throttled: confirms client has the portal + particles are being produced.
		{
			static int _pdbg = 0;
			if (++_pdbg % 120 == 0) {
				char _d[256] = {};
				vec4 pp = (Portals.size() > 0 && Portals[0]) ? Portals[0]->worldMat.pos : vec4(0);
				sprintf_s(_d, "[PORTAL-DBG] Portals=%d portalParticles=%d pool=%d pos0=(%.1f,%.1f,%.1f)\n",
					(int)Portals.size(), (int)gPortalParticles.size(), (int)gPortalPool.Count, pp.x, pp.y, pp.z);
				OutputDebugStringA(_d); printf("%s", _d);
			}
		}

		TickTransientParticles(gMuzzleFlashParticles, DeltaTime);
		TickTransientParticles(gBulletTracerParticles, DeltaTime);
		TickTransientParticles(gIceProjectileParticles, DeltaTime);
		TickTransientParticles(gPortalParticles, DeltaTime);
		TickTransientParticles(gBossExplosionParticles, DeltaTime);
		TickTransientParticles(gStatusIceParticles, DeltaTime);
		TickTransientParticles(gStatusFireParticles, DeltaTime);
		TickTransientParticles(gStatusYellowParticles, DeltaTime);
		TickTransientParticles(gStatusHackParticles, DeltaTime);
		TickTransientParticles(gStatusHealParticles, DeltaTime);
		UploadTransientParticles(gMuzzleFlashPool, gMuzzleFlashUpload, gMuzzleFlashParticles);
		UploadTransientParticles(gBulletTracerPool, gBulletTracerUpload, gBulletTracerParticles);
		UploadTransientParticles(gIceProjectilePool, gIceProjectileUpload, gIceProjectileParticles);
		UploadTransientParticles(gPortalPool, gPortalUpload, gPortalParticles);
		UploadTransientParticles(gBossExplosionPool, gBossExplosionUpload, gBossExplosionParticles);
		UploadTransientParticles(gStatusIcePool, gStatusIceUpload, gStatusIceParticles);
		UploadTransientParticles(gStatusFirePool, gStatusFireUpload, gStatusFireParticles);
		UploadTransientParticles(gStatusYellowPool, gStatusYellowUpload, gStatusYellowParticles);
		UploadTransientParticles(gStatusHackPool, gStatusHackUpload, gStatusHackParticles);
		UploadTransientParticles(gStatusHealPool, gStatusHealUpload, gStatusHealParticles);

		ParticleDraw->FireTexture = GetParticleSpriteResource(gMuzzleFlashSpriteSlot);
		ParticleDraw->Render(gd.gpucmd, &gMuzzleFlashPool.Buffer, gMuzzleFlashPool.Count);

		ParticleDraw->FireTexture = GetParticleSpriteResource(gBulletTracerSpriteSlot);
		ParticleDraw->Render(gd.gpucmd, &gBulletTracerPool.Buffer, gBulletTracerPool.Count);

		ParticleDraw->FireTexture = &gParticleIceTexture;
		ParticleDraw->Render(gd.gpucmd, &gIceProjectilePool.Buffer, gIceProjectilePool.Count);

		ParticleDraw->FireTexture = &gParticleIceTexture;
		ParticleDraw->Render(gd.gpucmd, &gPortalPool.Buffer, gPortalPool.Count);

		ParticleDraw->FireTexture = &gParticleIceTexture;
		ParticleDraw->Render(gd.gpucmd, &gStatusIcePool.Buffer, gStatusIcePool.Count);

		ParticleDraw->FireTexture = GetParticleSpriteResource(ParticleSpriteSlot::FireSecondary);
		ParticleDraw->Render(gd.gpucmd, &gStatusFirePool.Buffer, gStatusFirePool.Count);

		ParticleDraw->FireTexture = GetParticleSpriteResource(ParticleSpriteSlot::Yellow);
		ParticleDraw->Render(gd.gpucmd, &gStatusYellowPool.Buffer, gStatusYellowPool.Count);

		ParticleDraw->FireTexture = GetParticleSpriteResource(ParticleSpriteSlot::Hack);
		ParticleDraw->Render(gd.gpucmd, &gStatusHackPool.Buffer, gStatusHackPool.Count);

		ParticleDraw->FireTexture = GetParticleSpriteResource(ParticleSpriteSlot::Heal);
		ParticleDraw->Render(gd.gpucmd, &gStatusHealPool.Buffer, gStatusHealPool.Count);

		RenderBossPrototypeShield();
		RenderAegisShieldVisuals();
		RenderFrostBlizzardSnowWaves();
		RenderStatusEffectOverlays();
		RenderBossPrototypeMissiles();
		RenderBossPrototypeBeam();
		RenderRailgunOrbitVisuals();
		RenderDualBladeFloorVisuals();
		RenderHackerEmpVisuals();
		RenderPlayerRailgunBeams();
		RenderBossPrototypeAoEs();
		ParticleDraw->FireTexture = GetParticleSpriteResource(gBossExplosionSpriteSlot);
		ParticleDraw->Render(gd.gpucmd, &gBossExplosionPool.Buffer, gBossExplosionPool.Count);
	}

	// 24. UI ????? ???? DepthStencil?? Clear???. (??? UI?? ?????? ???? ??? ?? ???? ??????? ??? ????)
	gd.gpucmd->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);


	float hhpp = 0;
	float HeatGauge = 0;
	int kill = 0;
	int death = 0;
	int bulletCount = 0;
	float HealSkillCooldownFlow = 0;
	if (game.player != nullptr) {
		hhpp = game.player->HP;
		HeatGauge = game.player->HeatGauge;
		kill = game.player->KillCount;
		death = game.player->DeathCount;
		bulletCount = game.player->bullets;
		//HealSkillCooldownFlow = game.player->HealSkillCooldownFlow;
	}

	float Rate = gd.ClientFrameHeight / 960.0f;
	vec4 rt = Rate * vec4(-1650, 850, -1000, 700);

	gd.gpucmd.SetShader(game.MyScreenShader, ShaderType::RenderNormal);
	// Composite the full-screen damage feedback first. Screen UI currently uses
	// a depth-writing pipeline, so clear only its depth afterward and draw every
	// HUD element and SDF label cleanly on top of the retained color overlay.

	RenderDamageFeedbackHUD();
	gd.gpucmd->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);
	// The scope belongs above the world but below every gameplay HUD element.
	// Clear only depth again so ammo/cooldown SDF text cannot be rejected by it.
	RenderSniperScopeHUD();
	gd.gpucmd->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);

	if (game.mainPageStack.size() == 0) {
		RenderGameplayStatusHUD();
		RenderDungeonPartyHUD();
		RenderPartyLobbyUI();
		RenderBossPrototypeHUD();
		RenderBossPrototypeCoreHealthPlates();
		RenderMonsterHealthPlates();
		RenderFloatingDamageTexts();
		RenderGameplaySkillHUD();
		RenderAmmoHUD();
		RenderQuestCompleteShow();
		RenderQuestPrograssShow();
	}
	RenderDungeonDeathHUD();
	
	vector<DXPage*>* savePageStack = game.CurrentPageStack;
	game.CurrentPageStack = &game.mainPageStack;
	for (int i = 0; i < game.CurrentPageStack->size(); ++i) {
		DXPage* page = game.CurrentPageStack->at(i);
		page->Render();
	}

	//render Current Grab Slot
	if (game.CurrentPageStack->size() > 0 && CurrentGrabSlotData.itemCnt > 0 && CurrentGrabSlotData.objid > 0)
	{
		constexpr float margin = 40.0f;
		vec4 renderLoc = CurrentCursorPos;
		renderLoc.z = renderLoc.x;
		renderLoc.w = renderLoc.y;
		renderLoc.z += 2 * margin;
		renderLoc.w += 2 * margin;
		wchar_t NumText[64] = {};
		if (CurrentGrabSlotData.itemCnt > 0) {
			Item& item = ItemTable[CurrentGrabSlotData.objid];
			game.UIDraw_TextureRect(renderLoc, vec4(1, 1, 1, 1), 0.01f, item.icon);
		}

		_itow_s(CurrentGrabSlotData.itemCnt, NumText, 64, 10);
		game.RenderSDFText(NumText, wcslen(NumText), renderLoc, 15, vec4(1, 1, 1, 1), nullptr, nullptr, 0.001f);
	}

	// 27.
	gd.gpucmd->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);
	MyScreenShader->RenderAllSDFTexts();

	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
	gd.gpucmd.ResBarrierTr(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);

	gd.gpucmd->CopyResource(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], gd.SubRenderTarget.resource);

	gd.gpucmd.ResBarrierTr(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	gd.gpucmd.ResBarrierTr(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);

	//Execute
	gd.gpucmd.Close(true);
	gd.gpucmd.Execute(true);
	/*MeasureGPUWait(PerfGPUFinalWaitMs, [&]() { gd.gpucmd.WaitGPUComplete(); });
	MeasurePresent([&]() { gd.pSwapChain->Present(1, 0); });*/
	gd.gpucmd.WaitGPUComplete();
	gd.pSwapChain->Present(1, 0);

	//Get Present RenderTarget Index
	gd.CurrentSwapChainBufferIndex = gd.pSwapChain->GetCurrentBackBufferIndex();
}

void Game::Render_ShadowPass()
{
	static vector<SkinMeshGameObject*> ShadowRenderSkinMeshObjArr;
	static bool shadowCascadeInitialized[3] = { false, false, false };
	static const void* previousShadowZone = nullptr;
	static ui64 shadowFrameIndex = 0;
	constexpr float CascadeRange[4] = { 0.01f, 50.0f, 200.0f, 1000.0f };

	// 2. 렌더링을 시작한다.
	HRESULT hResult = gd.gpucmd.Reset();
	const bool enableShadowMeshLOD = AutoLOD_IsModelLODRenderActive();
	const ui64 currentShadowFrame = shadowFrameIndex++;
	const int shadowStabilityLevel = (std::max)(0, (std::min)(game.AutoLODShadowStabilityLevel, 3));
	if (previousShadowZone != game.Current_Zone) {
		shadowCascadeInitialized[0] = false;
		shadowCascadeInitialized[1] = false;
		shadowCascadeInitialized[2] = false;
		previousShadowZone = game.Current_Zone;
	}

	for (int i = 0; i < 3; ++i) {
		const bool updateShadowCascade =
			!enableShadowMeshLOD ||
			!shadowCascadeInitialized[i] ||
			(currentShadowFrame % kShadowCascadeUpdateIntervals[shadowStabilityLevel][i]) == 0;
		if (!updateShadowCascade) continue;
		shadowCascadeInitialized[i] = true;

		matrix viewproj;
		viewproj = gd.viewportArr[0].ViewMatrix;
		matrix proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), (float)gd.ClientFrameWidth / (float)gd.ClientFrameHeight, CascadeRange[i], CascadeRange[i+1]);
		viewproj *= proj;
		vec4 LightDirQ = vec4::DirectionToQuaternion(LightDirection);
		LightOBB = game.MyDirLight[0].viewport.GetOBB_IncludeFrustum(viewproj, LightDirQ);

		const int shadowResolution = kShadowCascadeResolutions[i];
		constexpr float LightDistance = 1000;
		vec4 obj = LightOBB.Center;
		obj += LightDirection * LightOBB.Extents.z;

		float MaxWidth = max(LightOBB.Extents.x, LightOBB.Extents.y) * 2;
		game.MyDirLight[i].viewport.Viewport.Width = shadowResolution;
		game.MyDirLight[i].viewport.Viewport.Height = shadowResolution;
		game.MyDirLight[i].viewport.Viewport.MaxDepth = 1.0f;
		game.MyDirLight[i].viewport.Viewport.MinDepth = 0.0f;
		game.MyDirLight[i].viewport.Viewport.TopLeftX = 0.0f;
		game.MyDirLight[i].viewport.Viewport.TopLeftY = 0.0f;
		game.MyDirLight[i].viewport.ScissorRect = {0, 0, (long)shadowResolution, (long)shadowResolution};


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
		const bool previousAutoLODFrameStatsTracking = AutoLOD_IsFrameStatsTracking();
		ViewportData* const shadowRenderViewPort = game.renderViewPort;
		game.renderViewPort = &gd.viewportArr[0];
		AutoLOD_SetFrameStatsTracking(false);
		AutoLOD_SetModelLODRenderActive(enableShadowMeshLOD);
		AutoLOD_SetModelLODRenderLevel(0);

		ShadowRenderSkinMeshObjArr.clear();

		for (int zi = 0; zi < 9; ++zi) {
			Zone* nearZone = game.Current_Zone->nearZones[zi];
			if (nearZone == nullptr) continue;
			if (nearZone->isMapLoaded == false) continue;
			game.TourID += 1;
			GameObjectIncludeChunks goic = nearZone->GetChunks_Include_OBB(LightOBB);
			GameObjectIncludeChunks Zone_goic = nearZone->ZoneChunk_goic;
			goic &= Zone_goic;

			//goic.ylen -= (1 + game.Map->AABB[1].y / game.chunck_divide_Width) - goic.ymin;
			ChunkIndex ci = ChunkIndex(goic.xmin, goic.ymin, goic.zmin);
			int ChunckSiz = goic.GetChunckSize();
			for (; ci.extra < ChunckSiz; goic.Inc(ci)) {
				GameChunk* gc = game.GameChunckFind(ci);
				if (gc != nullptr) {
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
		}
		game.renderViewPort = shadowRenderViewPort;
		AutoLOD_SetFrameStatsTracking(previousAutoLODFrameStatsTracking);

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
			// [crash-guard] GetRealShape only writes 'model' when the shape is a Model; if the shape is a
			// Mesh or unset (FlagPtr==0) it stays null, and model->RootNode below would dereference null.
			// Such an object cannot be skinmesh-shadow-rendered, so skip it instead of crashing.
			if (model == nullptr || model->RootNode == nullptr) continue;
			model->RootNode->SkinMeshShadowRender(model, gd.gpucmd, XMMatrixIdentity(), ShadowRenderSkinMeshObjArr[i]);
		}
		AutoLOD_SetModelLODRenderLevel(previousAutoLODRenderLevel);
		AutoLOD_SetModelLODRenderActive(previousAutoLODRenderActive);
	}
	// The instancing shader samples cascade 0 only. Keep its matrices paired
	// with shadow map 0 even when a farther cascade was refreshed last.
	const int zoneLightIndex = game.Current_Zone->Asset_OffsetMul;
	MappedDirLightData->DirLightView = LightCBData_withShadow[zoneLightIndex]->LightView[0];
	MappedDirLightData->DirLightProjection = LightCBData_withShadow[zoneLightIndex]->LightProjection[0];
	MappedDirLightData->DirLightPos = LightCBData_withShadow[zoneLightIndex]->LightPos[0];
	MappedDirLightData->DirLightDir = LightDirection;
	MappedDirLightData->DirLightColor = vec4(1, 1, 1, 1);

	gd.gpucmd.Close();
	gd.gpucmd.Execute();
	MeasureGPUWait(PerfGPUShadowWaitMs, [&]() { gd.WaitGPUComplete(); });
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
		if (MeshTable[i] == nullptr || MeshTable[i]->InstanceData == nullptr) continue;
		for (int k = 0; k < MeshTable[i]->subMeshNum; ++k) {
			//dbglog3(L"Instancing %d %d : %d \n", i, k, MeshTable[i]->InstanceData[k].Capacity);
			MeshTable[i]->InstanceData[k].ClearInstancing();
		}
	}
}

GameChunk* Game::GetChunckFromPos(vec4 pos)
{
	int ix = floor(pos.x / Zone::chunck_divide_Width);
	int iy = floor(pos.y / Zone::chunck_divide_Width);
	int iz = floor(pos.z / Zone::chunck_divide_Width);
	ChunkIndex ci = ChunkIndex(ix, iy, iz);
	GameChunk* gc = GameChunckFind(ci);
	return gc;
}

void Game::BatchRender(ID3D12GraphicsCommandList* cmd)
{
	for (int i = 0; i < MeshTable.size(); ++i) {
		Mesh* mesh = MeshTable[i];
		if (mesh == nullptr) continue;
		if (AutoLOD_IsModelLODRenderActive() && mesh->InstanceData != nullptr) {
			for (int subMesh = 0; subMesh < mesh->subMeshNum; ++subMesh) {
				if (mesh->InstanceData[subMesh].InstanceSize > 0) ++PerfAutoLODMainDraws;
			}
		}
		mesh->BatchRender(cmd);
	}
}

void Game::SelectAutoLODSubMeshes(BumpMesh* mesh, const matrix& transposedWorld,
	std::vector<int>& selectedSubMeshes)
{
	selectedSubMeshes.clear();
	if (mesh == nullptr || mesh->SubMeshIndexStart == nullptr || mesh->subMeshNum <= 0) return;

	selectedSubMeshes.reserve(mesh->subMeshNum);
	for (int i = 0; i < mesh->subMeshNum; ++i) {
		if (mesh->SubMeshIndexStart[i + 1] > mesh->SubMeshIndexStart[i]) {
			selectedSubMeshes.push_back(i);
		}
	}

	matrix objectWorld = transposedWorld;
	objectWorld.transpose();
	vec4 cameraDelta = objectWorld.pos - gd.viewportArr[0].Camera_Pos;
	cameraDelta.w = 0.0f;
	const float distance = cameraDelta.getlen3();
	const uint64_t sourceSubMeshCount = static_cast<uint64_t>(selectedSubMeshes.size());

	if (PresentShaderType == ShaderType::RenderShadowMap) {
		PerfAutoLODShadowSourceSubMeshes += sourceSubMeshCount;
		PerfAutoLODShadowRenderedSubMeshes += sourceSubMeshCount;
		return;
	}

	int subMeshLimit = static_cast<int>(selectedSubMeshes.size());
	if (distance >= kAutoLODExtremeSubMeshDistance) subMeshLimit = kAutoLODExtremeSubMeshLimit;
	else if (distance >= kAutoLODVeryFarSubMeshDistance) subMeshLimit = kAutoLODVeryFarSubMeshLimit;
	else if (distance >= kAutoLODFarSubMeshDistance) subMeshLimit = kAutoLODFarSubMeshLimit;
	else if (distance >= kAutoLODMidSubMeshDistance) subMeshLimit = kAutoLODMidSubMeshLimit;

	if (static_cast<int>(selectedSubMeshes.size()) <= subMeshLimit) {
		return;
	}
	std::sort(selectedSubMeshes.begin(), selectedSubMeshes.end(),
		[mesh](int lhs, int rhs) {
			const int lhsIndexCount = mesh->SubMeshIndexStart[lhs + 1] - mesh->SubMeshIndexStart[lhs];
			const int rhsIndexCount = mesh->SubMeshIndexStart[rhs + 1] - mesh->SubMeshIndexStart[rhs];
			return lhsIndexCount > rhsIndexCount;
		});
	selectedSubMeshes.resize(subMeshLimit);
}

bool Game::QueueAutoLODInstance(Mesh* mesh, const matrix& world, const int* materialIndices, int materialCount)
{
	if (!AutoLOD_IsModelLODRenderActive() || PresentShaderType != ShaderType::RenderWithShadow ||
		mesh == nullptr || mesh->type != Mesh::MeshType::_BumpMesh || materialIndices == nullptr) {
		return false;
	}

	BumpMesh* bumpMesh = static_cast<BumpMesh*>(mesh);
	if (!bumpMesh->IsAutoLODGenerated || bumpMesh->InstanceData == nullptr ||
		bumpMesh->subMeshNum <= 0 || materialCount < bumpMesh->subMeshNum) {
		return false;
	}

	std::vector<int> queuedSubMeshes;
	SelectAutoLODSubMeshes(bumpMesh, world, queuedSubMeshes);
	if (queuedSubMeshes.empty()) return false;

	// Never resize an instance buffer while recording this frame. Resizing updates
	// its SRV in the creation heap, but the shader-visible heap was baked earlier;
	// using that new buffer immediately can leave the GPU reading a stale SRV.
	for (int i : queuedSubMeshes) {
		const int materialIndex = materialIndices[i];
		if (materialIndex < 0 ||
			materialIndex >= static_cast<int>(RenderMaterialTable.size()) ||
			materialIndex >= static_cast<int>(gd.ShaderVisibleDescPool.MaterialCBVCap) ||
			RenderMaterialTable[materialIndex] == nullptr ||
			RenderMaterialTable[materialIndex]->CB_Resource.resource == nullptr ||
			RenderMaterialTable[materialIndex]->TextureSRVTableIndex.hRender.hgpu.ptr == 0) {
			return false;
		}
		if (bumpMesh->InstanceData[i].InstanceSize >= bumpMesh->InstanceData[i].Capacity) {
			return false;
		}
	}

	bool queuedAny = false;
	for (int i : queuedSubMeshes) {
		const int materialIndex = materialIndices[i];
		queuedAny |= bumpMesh->InstanceData[i].PushInstance(RenderInstanceData(world, materialIndex)) >= 0;
	}
	if (queuedAny) {
		++PerfAutoLODMainInstances;
		const uint64_t sourceSubMeshes = static_cast<uint64_t>(bumpMesh->subMeshNum);
		const uint64_t renderedSubMeshes = static_cast<uint64_t>(queuedSubMeshes.size());
		if (sourceSubMeshes > renderedSubMeshes) {
			PerfAutoLODMainTrimmedSubMeshes += sourceSubMeshes - renderedSubMeshes;
		}
	}
	return queuedAny;
}

void Game::ClearAutoLODInstancing()
{
	for (BumpMesh* bumpMesh : AutoLOD_GetGeneratedMeshes()) {
		if (bumpMesh == nullptr || bumpMesh->InstanceData == nullptr ||
			bumpMesh->SubMeshIndexStart == nullptr || bumpMesh->subMeshNum <= 0) continue;
		for (int i = 0; i < bumpMesh->subMeshNum; ++i) {
			bumpMesh->InstanceData[i].ClearInstancing();
		}
	}
}

void Game::RenderAutoLODInstancing(ID3D12GraphicsCommandList* cmd)
{
	if (!AutoLOD_IsModelLODRenderActive() || cmd == nullptr) return;
	size_t sourceDraws = 0;
	size_t renderedDraws = 0;
	for (BumpMesh* bumpMesh : AutoLOD_GetGeneratedMeshes()) {
		if (bumpMesh == nullptr || bumpMesh->InstanceData == nullptr ||
			bumpMesh->SubMeshIndexStart == nullptr || bumpMesh->subMeshNum <= 0) continue;
		for (int i = 0; i < bumpMesh->subMeshNum; ++i) {
			const int instanceCount = bumpMesh->InstanceData[i].InstanceSize;
			if (instanceCount <= 0) continue;
			sourceDraws += static_cast<size_t>(instanceCount);
			++renderedDraws;
		}
		bumpMesh->BatchRender(cmd);
	}
	PerfAutoLODMainDraws += static_cast<uint64_t>(renderedDraws);
	AutoLOD_RecordCombinedLOD1(renderedDraws > 0, sourceDraws, renderedDraws);
}

// [seamless] Build queued skinmesh GPU bone buffers a few per frame so a server transfer never
// freezes the main thread. The local player's own object is built first (camera/controls need it
// immediately); other objects pop in over a few frames. Built objects are then enabled and pushed.
void Game::ProcessPendingSkinBoneInit() {
	auto requiresBoneBuffers = [](DynamicGameObject* obj) {
		if (obj == nullptr || !obj->tag[GameObjectTag::Tag_SkinMeshObject]) return false;
		Model* model = obj->shape.GetModel();
		return model != nullptr && model->mNumSkinMesh > 0;
	};

	// Stage 2: objects whose bone buffers were built last frame have now been skinned at least once
	// by the per-frame Update loop, so they are safe to draw. Push them into the render list now.
	for (size_t i = 0; i < m_pendingSkinRenderEnable.size(); ++i) {
		int netIdx = m_pendingSkinRenderEnable[i];
		if (netIdx < 0 || netIdx >= (int)DynmaicGameObjects.size()) continue;
		DynamicGameObject* obj = DynmaicGameObjects[netIdx];
		if (obj == nullptr) continue;
		if (obj->tag[GameObjectTag::Tag_Enable] == false) continue;   // disabled/cleaned meanwhile
		// If this slot was deleted and reused by a not-yet-built skinmesh, its bone buffers are empty;
		// don't push it (it will be pushed properly once its own queue entry builds it).
		if (requiresBoneBuffers(obj) && ((SkinMeshGameObject*)obj)->BoneToWorldMatrixCB.empty()) continue;
		int zid = obj->zoneId;
		Zone* rz = (zid >= 0 && zid < (int)ZoneTable.size()) ? ZoneTable[zid] : Current_Zone;
		if (rz && obj->chunkAllocIndexs == nullptr) rz->PushGameObject(obj);
	}
	m_pendingSkinRenderEnable.clear();

	if (m_pendingSkinBoneInit.empty()) return;
	// Bring the local player's own object to the front (needed without delay).
	for (size_t i = 1; i < m_pendingSkinBoneInit.size(); ++i) {
		if (m_pendingSkinBoneInit[i] == playerGameObjectIndex) {
			int t = m_pendingSkinBoneInit[0];
			m_pendingSkinBoneInit[0] = m_pendingSkinBoneInit[i];
			m_pendingSkinBoneInit[i] = t;
			break;
		}
	}
	int budget = 2;   // per-frame cap so one frame never stalls long
	if (m_pendingSkinBoneInit[0] == playerGameObjectIndex) budget += 1;
	int done = 0;
	while (!m_pendingSkinBoneInit.empty() && done < budget) {
		int netIdx = m_pendingSkinBoneInit.front();
		m_pendingSkinBoneInit.erase(m_pendingSkinBoneInit.begin());
		done++;
		if (netIdx < 0 || netIdx >= (int)DynmaicGameObjects.size()) continue;
		DynamicGameObject* obj = DynmaicGameObjects[netIdx];
		if (obj == nullptr) continue;
		if (requiresBoneBuffers(obj)) {
			SkinMeshGameObject* skin = (SkinMeshGameObject*)obj;
			skin->InitRootBoneMatrixs();
			if (skin->BoneToWorldMatrixCB.empty()) {
				// Allocation can fail transiently after a device-memory pressure spike. Keep the object
				// disabled and retry next frame instead of rendering it with null bone resources.
				obj->tag[GameObjectTag::Tag_Enable] = false;
				m_pendingSkinBoneInit.push_back(netIdx);
				OutputDebugStringA("[SkinBoneInit] GPU allocation failed; queued for retry\n");
				break;
			}
		}
		// Register the object in its real zone before enabling it. Otherwise this frame's
		// Monster::Update sees chunkAllocIndexs==nullptr and used the old zoneid=-1.
		int zid = obj->zoneId;
		Zone* rz = (zid >= 0 && zid < (int)ZoneTable.size()) ? ZoneTable[zid] : Current_Zone;
		if (rz != nullptr && obj->chunkAllocIndexs == nullptr) {
			obj->zoneid = rz->zoneid;
			rz->PushGameObject(obj);
		}
		// Stage 1: enable now so THIS frame's Update loop skins it; defer the render-list push to
		// next frame (Stage 2 above) so the object is never drawn before it has been skinned once.
		obj->tag[GameObjectTag::Tag_Enable] = true;
		m_pendingSkinRenderEnable.push_back(netIdx);
	}
}

int stackPacket = 0;
void Game::Update()
{
	AutoLOD_ProcessRuntimeQueue(1);
	BoxLOD_DebugUpdate(DeltaTime);
	UpdateGameplaySkillHUD(DeltaTime);
	UpdateDamageFeedback(DeltaTime);
	UpdateStatusEffectVisuals(DeltaTime);
	UpdateFloatingDamageTexts(DeltaTime);
	UpdateDelayedWeaponFireVisuals(DeltaTime);

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

	// Keep the sun direction fixed. Continuously rotating it moved every shadow
	// between frames and caused thin or distant cascade shadows to shimmer.
	for (int i = 0; i < 9; ++i) {
		LightCBData_withShadow[i]->dirlight.gLightDirection = LightDirection.f3;
	}

	static float accSend = 0.0f;
	const float SendPeriod = 0.05f;

	accSend += DeltaTime;
	// [party] free the cursor while the party menu / join list is open so its buttons can be clicked.
	const bool partyUIOpen = (m_partyMenuOpen || m_partyJoinListOpen ||
		(m_dungeonQueue.active && m_dungeonQueue.partyId >= 0 && currentZoneId < 100));
	if ((isPrepared && !isInventoryOpen && !partyUIOpen && !StatWindowOpen) && (GetActiveWindow() == hWnd && mainPageStack.size() == 0)) {
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
		//dbgbreak(stackPacket < 1458);
		int result = client.recv(client.rBuf + client.rbufOffset, client.rbufMax - client.rbufOffset);
		if (result > 0) {
			stackPacket += result;
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
			//client.Disconnect();
			break;
		}
		else break; // 더 읽을 패킷이 없으면 종료
	}
	//gd.AverageSecPer60End(Update_ClientRecv);

	// [seamless] Spread heavy GPU bone-buffer creation across frames (prevents the ~1s transfer freeze).
	ProcessPendingSkinBoneInit();

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
			isPrepared = IsPresentationReady();
		}
	}

	if (isPrepared) {
		// 플레이어 회전 정보 전송
		if (player != nullptr) {
			//dbglog1(L"playerpos y : %f \n", player->worldMat.pos.y);
			if (!std::isfinite(player->m_pitch) || !std::isfinite(player->m_yaw)) {
				player->m_pitch = 0.0f;
				player->m_yaw = 0.0f;
			}

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
				pkt.bFirstPersonVision = bFirstPersonVision;
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

		UpdateBossPrototype(DeltaTime);
		for (BossPrototypeCore& core : BossPrototypeCores) {
			if (core.HitFlashTimer > 0.0f) {
				core.HitFlashTimer -= DeltaTime;
				if (core.HitFlashTimer < 0.0f) core.HitFlashTimer = 0.0f;
			}
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
				peye += 1.55f * player->worldMat.up;
				pat += 1.55f * player->worldMat.up;
				pat += 10.0f * clook;
			}
			else {
				peye += 1.35f * player->worldMat.up;
				peye -= 4.0f * clook;
				peye += 1.10f * player->worldMat.right;
				pat = peye + 10.0f * clook;
			}

			const float minCameraY = player->worldMat.pos.y + 0.3f;
			if (peye.y < minCameraY) {
				const float cameraLift = minCameraY - peye.y;
				peye.y = minCameraY;
				pat.y += cameraLift;
			}

			if (CameraShakeTimer > 0.0f && CameraShakeDuration > 0.0f && CameraShakeStrength > 0.0f) {
				float shakeRate = min(1.0f, max(0.0f, CameraShakeTimer / CameraShakeDuration));
				float shakeAmount = CameraShakeStrength * shakeRate * shakeRate;
				float phase = (CameraShakeDuration - CameraShakeTimer) * 78.0f;
				vec4 shakeRight = NormalizeOrFallback(player->worldMat.right, vec4(1, 0, 0, 0));
				vec4 shakeUp = NormalizeOrFallback(player->worldMat.up, vec4(0, 1, 0, 0));
				vec4 shakeOffset = shakeRight * (sinf(phase * 1.37f) * shakeAmount)
					+ shakeUp * (cosf(phase * 1.91f) * shakeAmount * 0.62f);
				peye += shakeOffset;
				pat -= shakeOffset * 0.35f;
			}

			XMFLOAT3 Up = { 0, 1, 0 };
			gd.viewportArr[0].ViewMatrix = XMMatrixLookAtLH(peye, pat, XMLoadFloat3(&Up));
			gd.viewportArr[0].Camera_Pos = peye;
			if (gd.isRaytracingRender) {
				gd.raytracing.SetRaytracingCamera(peye, pat - peye, vec4(0, 1, 0, 0), player->m_currentFov);
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
	if (offset + size > totallen) {
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
			Zone* renderZone = game.Current_Zone;
			if (header.zoneId >= 0 && header.zoneId < (int)game.ZoneTable.size()) {
				renderZone = game.ZoneTable[header.zoneId];
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
			StaticGameObjects[header.objindex]->zoneId = header.zoneId;
			StaticGameObjects[header.objindex]->RecvSTC_SyncObj(datapivot);
			game.ApplyZoneOffsetToStaticObject(StaticGameObjects[header.objindex]);
			if (renderZone) {
				renderZone->PushGameObject(StaticGameObjects[header.objindex]);
			}
		}
			break;
		case GameObjectType::_DynamicGameObject:
		case GameObjectType::_SkinMeshGameObject:
		case GameObjectType::_Player:
		case GameObjectType::_Monster:
		case GameObjectType::_PeacefulNPC:
		{
			int netObjIndex = game.GetDynamicObjectNetIndex(header.zoneId, header.objindex);
			if (netObjIndex < 0) {
				break;
			}
			Zone* renderZone = game.Current_Zone;
			if (header.zoneId >= 0 && header.zoneId < (int)game.ZoneTable.size()) {
				renderZone = game.ZoneTable[header.zoneId];
			}
			if (netObjIndex >= DynmaicGameObjects.size()) {
				// 이 코드는 실행되지 말아야 함. 최대한. 하지만 오류가 났을때 대처하기 위해 일단 넣어놓는다.
				DynmaicGameObjects.reserve(netObjIndex + 1);
				DynmaicGameObjects.resize(netObjIndex + 1);
			}

			if (DynmaicGameObjects[netObjIndex]) {
				if (*(void**)DynmaicGameObjects[netObjIndex] != GameObjectType::vptr[header.type]) {
					// A zone handoff may reuse the same network slot for a different object type.
					// Release the old object instead of deleting it directly: Player owns independent
					// weapon/drone TLAS instances which otherwise survive as weapon-only ghosts.
					for (size_t k = 0; k < game.m_pendingSkinBoneInit.size();) {
						if (game.m_pendingSkinBoneInit[k] == netObjIndex) game.m_pendingSkinBoneInit.erase(game.m_pendingSkinBoneInit.begin() + k);
						else ++k;
					}
					for (size_t k = 0; k < game.m_pendingSkinRenderEnable.size();) {
						if (game.m_pendingSkinRenderEnable[k] == netObjIndex) game.m_pendingSkinRenderEnable.erase(game.m_pendingSkinRenderEnable.begin() + k);
						else ++k;
					}
					DynmaicGameObjects[netObjIndex]->Release();
					delete DynmaicGameObjects[netObjIndex];
					DynmaicGameObjects[netObjIndex] = nullptr;
					switch (header.type) {
					case GameObjectType::_DynamicGameObject:
						DynmaicGameObjects[netObjIndex] = new DynamicGameObject();
						break;
					case GameObjectType::_SkinMeshGameObject:
						DynmaicGameObjects[netObjIndex] = new SkinMeshGameObject();
						break;
					case GameObjectType::_Player:
						DynmaicGameObjects[netObjIndex] = new Player();
						break;
					case GameObjectType::_Monster:
						DynmaicGameObjects[netObjIndex] = new Monster();
						break;
					case GameObjectType::_PeacefulNPC:
						DynmaicGameObjects[netObjIndex] = new PeacefulNPC();
						break;
					} 
				}
			}
			else {
				switch (header.type) {
				case GameObjectType::_DynamicGameObject:
					DynmaicGameObjects[netObjIndex] = new DynamicGameObject();
					break;
				case GameObjectType::_SkinMeshGameObject:
					DynmaicGameObjects[netObjIndex] = new SkinMeshGameObject();
					break;
				case GameObjectType::_Player:
					DynmaicGameObjects[netObjIndex] = new Player();
					break;
				case GameObjectType::_Monster:
					DynmaicGameObjects[netObjIndex] = new Monster();
					break;
				case GameObjectType::_PeacefulNPC:
					DynmaicGameObjects[netObjIndex] = new PeacefulNPC();
					break;
				}
			}

			DynmaicGameObjects[netObjIndex]->zoneId = header.zoneId;
			DynmaicGameObjects[netObjIndex]->zoneid = header.zoneId;
			const bool deferSkinInit = header.type == GameObjectType::_SkinMeshGameObject ||
				header.type == GameObjectType::_Player || header.type == GameObjectType::_Monster ||
				header.type == GameObjectType::_PeacefulNPC;
			const bool previousDeferSkinInit = game.m_deferNetworkSkinBoneInit;
			game.m_deferNetworkSkinBoneInit = deferSkinInit;
			DynmaicGameObjects[netObjIndex]->RecvSTC_SyncObj(datapivot);
			game.m_deferNetworkSkinBoneInit = previousDeferSkinInit;
			game.ApplyZoneOffsetToDynamicObject(DynmaicGameObjects[netObjIndex]);
			if (DynmaicGameObjects[netObjIndex]->tag[GameObjectTag::Tag_SkinMeshObject]) {
				SkinMeshGameObject* smgo = (SkinMeshGameObject*)DynmaicGameObjects[netObjIndex];
				Model* skinModel = smgo->shape.GetModel();
				const bool requiresBoneBuffers = skinModel != nullptr && skinModel->mNumSkinMesh > 0;
				// [seamless-B] Building GPU bone buffers (InitRootBoneMatrixs) does several full GPU
				// flushes. On a server transfer ~20 skinmesh objects arrive in ONE frame; building all
				// at once freezes the main thread ~1s and starves the input pump. Instead, disable the
				// object now (so it is neither updated nor rendered, and cannot crash without bone
				// buffers) and queue it. ProcessPendingSkinBoneInit builds a few per frame, then enables
				// and renders them once their buffers exist.
				// Only defer objects that have NOT been built yet (empty bone buffers); an already-built
				// object that re-syncs through this path is left enabled. Dedup the queue so the same
				// object is never enqueued (and later render-pushed) twice.
				if (requiresBoneBuffers && smgo->BoneToWorldMatrixCB.empty()) {
					smgo->tag[GameObjectTag::Tag_Enable] = false;
					bool alreadyQueued = false;
					for (size_t k = 0; k < game.m_pendingSkinBoneInit.size(); ++k) {
						if (game.m_pendingSkinBoneInit[k] == netObjIndex) { alreadyQueued = true; break; }
					}
					if (!alreadyQueued) game.m_pendingSkinBoneInit.push_back(netObjIndex);
				}
			}
			// [seamless] Skip push for objects we just deferred (Enable turned off); rendering a
			// skinmesh without bone buffers crashes. ProcessPendingSkinBoneInit pushes them once built.
			if (renderZone && DynmaicGameObjects[netObjIndex]->tag[GameObjectTag::Tag_Enable] &&
				DynmaicGameObjects[netObjIndex]->chunkAllocIndexs == nullptr) {
				renderZone->PushGameObject(DynmaicGameObjects[netObjIndex]);
			}
			{
				DynamicGameObject* _dgo = DynmaicGameObjects[netObjIndex];
				char _dbg[256] = {};
				sprintf_s(_dbg, "[Dyn] zone=%d idx=%d net=%d type=%d enable=%d shapeFlag=%p pos=(%.2f,%.2f,%.2f)\n",
					header.zoneId, header.objindex, netObjIndex, (int)header.type,
					(int)_dgo->tag[GameObjectTag::Tag_Enable], _dgo->shape.FlagPtr,
					_dgo->worldMat.pos.f3.x, _dgo->worldMat.pos.f3.y, _dgo->worldMat.pos.f3.z);
				OutputDebugStringA(_dbg);
				printf("%s", _dbg);
				fflush(stdout);
			}
			if (header.type == GameObjectType::_Player && game.playerGameObjectIndex == netObjIndex) {
				game.player = (Player*)DynmaicGameObjects[netObjIndex];
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
				game.isPrepared = game.IsPresentationReady();
			}
		}
			break;
		case GameObjectType::_Portal:
		{
			Portal* portal = new Portal();
			portal->zoneId = header.zoneId;
			portal->RecvSTC_SyncObj(datapivot);
			game.ApplyZoneOffsetToPortal(portal);
			game.Portals.push_back(portal);
			{ char _p[200]; sprintf_s(_p, "[PORTAL] recv zone=%d pos=(%.1f,%.1f,%.1f) total=%d (currentZone=%d)\n", portal->zoneId, portal->worldMat.pos.f3.x, portal->worldMat.pos.f3.y, portal->worldMat.pos.f3.z, (int)game.Portals.size(), game.currentZoneId); OutputDebugStringA(_p); printf("%s", _p); fflush(stdout); }
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
		int netObjIndex = game.GetDynamicObjectNetIndex(header.zoneId, header.objindex);

		short _typeShort = (short)header.type;
		if (_typeShort < 0 || _typeShort >= GameObjectType::ObjectTypeCount) {
			char _dbg[192] = {};
			sprintf_s(_dbg, "[ChangeMember] BAD type=%d zone=%d objidx=%d net=%d srvoff=%d datasiz=%d size=%u\n",
				(int)_typeShort, header.zoneId, header.objindex, netObjIndex, header.serveroffset, header.datasize, header.size);
			OutputDebugStringA(_dbg); printf("%s", _dbg); fflush(stdout);
		}
		else if (netObjIndex < 0 || netObjIndex >= (int)DynmaicGameObjects.size()
			|| DynmaicGameObjects[netObjIndex] == nullptr) {
			static int _badIdxCnt = 0;
			if ((_badIdxCnt++ % 60) == 0) {
				char _dbg[192] = {};
				sprintf_s(_dbg, "[ChangeMember] BAD zone=%d objidx=%d net=%d (size=%zu null=%d) type=%d srvoff=%d\n",
					header.zoneId, header.objindex, netObjIndex, DynmaicGameObjects.size(),
					(netObjIndex >= 0 && netObjIndex < (int)DynmaicGameObjects.size()) ? (DynmaicGameObjects[netObjIndex] == nullptr ? 1 : 0) : -1,
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
						sw.syncfunc(DynmaicGameObjects[netObjIndex], source, header.datasize);
					}
				}
				else {
					char* dest = (((char*)DynmaicGameObjects[netObjIndex]) + sw.clientOffset);
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
		if constexpr (SHOW_BULLET_RAY_DEBUG_MESH) {
			int BulletIndex = bulletRays.Alloc();
			if (BulletIndex >= 0) {
				BulletRay& bray = bulletRays[BulletIndex];
				bray = BulletRay(header.raystart, header.rayDir, header.distance);
			}
		}
		if (header.distance < BULLET_RAY_MISS_DISTANCE) {
			SpawnBulletImpact(header.raystart, header.rayDir, header.distance);
		}
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STC_Protocol::AllocPlayerIndexes:
	{
        //OutputDebugStringA("[ClientReceiving] AllocPlayerIndexes\n");
		stackPacket = 0;
		STC_AllocPlayerIndexes_Header& header = *(STC_AllocPlayerIndexes_Header*)currentPivot;

		game.clientIndexInServer = header.clientindex;
		game.playerGameObjectIndex = game.GetDynamicObjectNetIndex(game.currentZoneId, header.server_obj_index);

		if (game.playerGameObjectIndex >= 0 && game.DynmaicGameObjects.size() > game.playerGameObjectIndex) {
			player = (Player*)DynmaicGameObjects[playerGameObjectIndex];
			//player->Gun = game.GunMesh;
			//
			if (player != nullptr && game.isPreparedClientIndex) {
				player->worldMat.pos = player->DestPos;
				player->worldMat.pos.w = 1;
			}

			if (player != nullptr) {
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
			}

			//for (int i = 0; i < 36; ++i) {
			//	player->Inventory[i].id = 0;
			//	player->Inventory[i].ItemCount = 0;
			//}
		}
		game.isPreparedClientIndex = true;
		game.isPrepared = game.IsPresentationReady();

		// [seamless] The new server just bound this player. If a movement key (W/A/S/D/Space) is
		// being held, the new server never saw a key-down for it, so the player sits still until
		// Windows key-repeat fires (~1s). Resend the currently-held keys now so the new server
		// resumes movement immediately instead of waiting for the next key-repeat.
		game.ResendHeldMovementKeys();

		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STC_Protocol::DeleteGameObject:
	{
		STC_DeleteGameObject_Header& header = *(STC_DeleteGameObject_Header*)currentPivot;
		int netObjIndex = game.GetDynamicObjectNetIndex(header.zoneId, header.obj_index);
		if (netObjIndex < 0 || game.DynmaicGameObjects.size() <= netObjIndex) {
			return 2;
		}
		if (DynmaicGameObjects[netObjIndex] != nullptr) {
			if (*(void**)DynmaicGameObjects[netObjIndex] == GameObjectType::vptr[GameObjectType::_Player]) {
				Player* deletedPlayer = (Player*)DynmaicGameObjects[netObjIndex];
				deletedPlayer->ClearThirdPersonWeaponVisuals();
				deletedPlayer->SetRaytracingVisualsEnabled(false);
				for (size_t i = 0; i < gDelayedWeaponFireVisuals.size();) {
					const DelayedWeaponFireVisual& event = gDelayedWeaponFireVisuals[i];
					if (event.ZoneId == header.zoneId && event.ObjectIndex == header.obj_index) {
						gDelayedWeaponFireVisuals.erase(gDelayedWeaponFireVisuals.begin() + i);
					}
					else {
						++i;
					}
				}
			}
			else {
				DynmaicGameObjects[netObjIndex]->SetRaytracingInstanceEnabled(false);
			}
			DynmaicGameObjects[netObjIndex]->tag[GameObjectTag::Tag_Enable] = false;
		}
		// [seamless-B] If this object is still waiting in the deferred bone-init queues, drop it so
		// ProcessPendingSkinBoneInit doesn't later re-enable/re-render a deleted object (clone artifact).
		for (size_t k = 0; k < game.m_pendingSkinBoneInit.size(); ) {
			if (game.m_pendingSkinBoneInit[k] == netObjIndex) game.m_pendingSkinBoneInit.erase(game.m_pendingSkinBoneInit.begin() + k);
			else ++k;
		}
		for (size_t k = 0; k < game.m_pendingSkinRenderEnable.size(); ) {
			if (game.m_pendingSkinRenderEnable[k] == netObjIndex) game.m_pendingSkinRenderEnable.erase(game.m_pendingSkinRenderEnable.begin() + k);
			else ++k;
		}
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STC_Protocol::ItemDrop:
	{
		STC_ItemDrop_Header& header = *(STC_ItemDrop_Header*)currentPivot;
		int netDropIndex = game.GetDropItemNetIndex(header.zoneId, header.dropindex);
		if (netDropIndex >= 0 && netDropIndex >= game.DropedItems.size()) {
			while (netDropIndex >= game.DropedItems.size()) {
				ItemLoot til;
				til.pos = 0;
				til.itemDrop.id = 0;
				til.itemDrop.ItemCount = 0;
				game.DropedItems.push_back(til);
			}
			game.DropedItems[netDropIndex] = header.lootData;
		}
		else if (netDropIndex >= 0) {
			game.DropedItems[netDropIndex] = header.lootData;
		}
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STC_Protocol::ItemDropRemove:
	{
		STC_ItemDropRemove_Header& header = *(STC_ItemDropRemove_Header*)currentPivot;
		int netDropIndex = game.GetDropItemNetIndex(header.zoneId, header.dropindex);
		if (netDropIndex >= 0 && netDropIndex < (int)game.DropedItems.size()) {
			game.DropedItems[netDropIndex].itemDrop.id = 0;
			game.DropedItems[netDropIndex].itemDrop.ItemCount = 0;
			game.DropedItems[netDropIndex].pos = vec4(0, 0, 0, 0);
		}
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
		int netObjIndex = game.GetDynamicObjectNetIndex(header.zoneId, header.objindex);
		if (netObjIndex >= 0 && netObjIndex < DynmaicGameObjects.size() && DynmaicGameObjects[netObjIndex] != nullptr) {
			GameObject* pObj = DynmaicGameObjects[netObjIndex];

			void* objVptr = *(void**)pObj;

			if (objVptr == GameObjectType::vptr[GameObjectType::_Player]) {
				Player* pTarget = (Player*)pObj;

				if (pTarget) {
					if (header.fireHand == 2 && (WeaponType)pTarget->m_currentWeaponType == WeaponType::DualPistol) {
						pTarget->m_dualBladeAttackVisualTimer = 0.34f;
						pTarget->m_dualBladeAttackAlternate = !pTarget->m_dualBladeAttackAlternate;
						currentPivot += header.size;
						offset += header.size;
						break;
					}

					const bool leftHand = header.fireHand != 0;
					if ((WeaponType)pTarget->m_currentWeaponType == WeaponType::DualPistol && !leftHand) {
						gDelayedWeaponFireVisuals.push_back({ header.zoneId, header.objindex, 0.11f, false });
					}
					else {
						PlayPlayerFireVisual(pTarget, leftHand);
					}
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
		int netOwnerIndex = game.GetDynamicObjectNetIndex(header.zoneId, header.ownerObjIndex);
		if (header.effectType == SkillEffectType::DualPistol_BladeMode &&
			netOwnerIndex >= 0 && netOwnerIndex < (int)game.DynmaicGameObjects.size()) {
			Player* ownerPlayer = dynamic_cast<Player*>(game.DynmaicGameObjects[netOwnerIndex]);
				if (ownerPlayer != nullptr && header.duration > 0.5f) {
					ownerPlayer->m_dualBladeVisualTimer = max(ownerPlayer->m_dualBladeVisualTimer, header.duration);
					for (size_t i = 0; i < gDelayedWeaponFireVisuals.size();) {
						const DelayedWeaponFireVisual& event = gDelayedWeaponFireVisuals[i];
						if (event.ZoneId == header.zoneId && event.ObjectIndex == header.ownerObjIndex) {
							gDelayedWeaponFireVisuals.erase(gDelayedWeaponFireVisuals.begin() + i);
						}
						else {
							++i;
						}
					}
				}
		}
		SpawnSkillEffect(header.effectType, header.position, header.direction, (UINT)netOwnerIndex, header.radius, header.power, header.duration);
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STC_Protocol::StatusEffect:
	{
		STC_StatusEffect_Header& header = *(STC_StatusEffect_Header*)currentPivot;
		int netTargetIndex = game.GetDynamicObjectNetIndex(header.zoneId, header.targetObjIndex);
		int netSourceIndex = game.GetDynamicObjectNetIndex(header.zoneId, header.sourceObjIndex);
		SpawnStatusEffect(header.statusType, netTargetIndex, netSourceIndex, header.active, header.position, header.extents, header.duration, header.remainTime, header.power);
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STC_Protocol::BossState:
	{
		STC_BossState_Header& header = *(STC_BossState_Header*)currentPivot;
		ApplyBossState(header);
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STC_Protocol::DungeonQueueUpdate:
	{
		STC_DungeonQueueUpdate_Header& header = *(STC_DungeonQueueUpdate_Header*)currentPivot;
		if (header.size != sizeof(STC_DungeonQueueUpdate_Header)) {
			OutputDebugStringA("[ProtocolMismatch] DungeonQueueUpdate ignored\n");
			currentPivot += header.size;
			offset += header.size;
			break;
		}
		game.m_dungeonQueue.count = header.count;
		game.m_dungeonQueue.maxCount = header.maxCount;
		game.m_dungeonQueue.active = (header.count > 0);
		game.m_dungeonQueue.leaderClientIndex = header.leaderClientIndex;
		game.m_dungeonQueue.partyId = header.partyId;
		game.m_dungeonQueue.number = header.number;
		game.m_dungeonQueue.dungeonDeathCount = header.dungeonDeathCount;
		game.m_dungeonQueue.dungeonDeathLimit = header.dungeonDeathLimit;
		for (int i = 0; i < 3; ++i) {
			game.m_dungeonQueue.objindex[i] = header.objindex[i];
			game.m_dungeonQueue.zoneId[i] = header.zoneId[i];
			game.m_dungeonQueue.hp[i] = header.hp[i];
			game.m_dungeonQueue.maxhp[i] = header.maxhp[i];
			game.m_dungeonQueue.m_currentJob[i] = header.m_currentJob[i];
		}
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STC_Protocol::DungeonResult:
	{
		STC_DungeonResult_Header& header = *(STC_DungeonResult_Header*)currentPivot;
		if (header.size != sizeof(STC_DungeonResult_Header)) {
			OutputDebugStringA("[ProtocolMismatch] DungeonResult ignored\n");
			currentPivot += header.size;
			offset += header.size;
			break;
		}
		game.m_dungeonResult = header.result;
		game.m_dungeonResultDeaths = header.deathCount;
		game.m_dungeonResultLimit = header.deathLimit;
		game.m_dungeonResultFlash = 5.0f;
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STC_Protocol::PartyList:
	{
		STC_PartyList_Header& header = *(STC_PartyList_Header*)currentPivot;
		int n = header.count; if (n < 0) n = 0; if (n > 16) n = 16;
		game.m_partyList.count = n;
		for (int i = 0; i < n; ++i) {
			game.m_partyList.entries[i].partyId = header.parties[i].partyId;
			game.m_partyList.entries[i].number = header.parties[i].number;
			game.m_partyList.entries[i].memberCount = header.parties[i].memberCount;
			game.m_partyList.entries[i].maxCount = header.parties[i].maxCount;
			game.m_partyList.entries[i].started = header.parties[i].started;
		}
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STC_Protocol::DungeonReject:
	{
		STC_DungeonReject_Header& header = *(STC_DungeonReject_Header*)currentPivot;
		game.m_dungeonRejectFlash = 4.0f; // show "던전이 가득 찼습니다" for ~4s
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STC_Protocol::DungeonEnter:
	{
		STC_DungeonEnter_Header& header = *(STC_DungeonEnter_Header*)currentPivot;
		game.BeginPortalEnter(header.ip, header.port, header.dstZoneId);
		return totallen;
	}
	break;
	case STC_Protocol::SyncGameState:
	{
        //OutputDebugStringA("[ClientReceiving] SyncGameState\n");
		STC_SyncGameState_Header& header = *(STC_SyncGameState_Header*)currentPivot;
		game.DynamicGameObjectCapacityPerZone = header.DynamicGameObjectCapacityPerZone > 0 ? header.DynamicGameObjectCapacityPerZone : header.DynamicGameObjectCapacity;
		game.DynmaicGameObjects.reserve(header.DynamicGameObjectCapacity);
		game.DynmaicGameObjects.resize(header.DynamicGameObjectCapacity);
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STC_Protocol::InitialSyncComplete:
	{
		STC_InitialSyncComplete_Header& header = *(STC_InitialSyncComplete_Header*)currentPivot;
		const int expectedNetIndex = game.GetDynamicObjectNetIndex(header.zoneId, header.playerObjIndex);
		if (header.zoneId == game.currentZoneId && expectedNetIndex == game.playerGameObjectIndex &&
			game.player != nullptr) {
			game.isServerSyncComplete = true;
		}
		game.isPrepared = game.IsPresentationReady();
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STC_Protocol::JobChangeAck:
	{
		STC_JobChangeAck_Header& header = *(STC_JobChangeAck_Header*)currentPivot;
		if (!game.isInitialJobConfirmed && game.expectedInitialJob >= 0 && game.player != nullptr) {
			const bool validSelectedWeapon = game.player->SelectedWeapon >= 0 && game.player->SelectedWeapon < 3;
			const int localWeaponType = validSelectedWeapon
				? (int)game.player->weapon[game.player->SelectedWeapon].m_info.type : -1;
			if (header.job == game.expectedInitialJob && game.player->m_currentJob == game.expectedInitialJob &&
				game.player->m_currentWeaponType == header.weaponType && localWeaponType == header.weaponType) {
				game.isInitialJobConfirmed = true;
			}
		}
		game.isPrepared = game.IsPresentationReady();
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
		game.isServerSyncComplete = false;
		game.MoveZone(header.zoneId, false);
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
		/*currentPivot += header.size;
		offset += header.size;*/
		return totallen;
	}
		break;
	case STC_Protocol::NPCTalkStart:
	{
		STC_NPCTalkStart_Header& header = *(STC_NPCTalkStart_Header*)currentPivot;
		if (header.NPCType == PNT_Quest) {
			PresentShowedTalkID = header.StartID;
			game.mainPageStack.push_back(game.UIPageTable[1]);
			UITourID += 1;
		}
		else if(header.NPCType == PNT_Shop){
			PresentShopID = header.StartID;
		}
		currentPivot += header.size;
		offset += header.size;
	}
		break;
	case STC_Protocol::AddQuest:
	{
		STC_AddQuest_Header& header = *(STC_AddQuest_Header*)currentPivot;
		game.UITourID += 1;
		if (0 <= header.QuestID && header.QuestID < (int)game.QuestTable.size() && game.QuestTable[header.QuestID] != nullptr) {
			int questSlot = -1;
			for (int i = 0; i < (int)game.QuestArr.size(); ++i) {
				if (game.QuestArr[i] == header.QuestID) {
					questSlot = i;
					break;
				}
			}
			if (questSlot < 0) {
				game.QuestArr.push_back(header.QuestID);
				Quest* prograss = new Quest();
				game.QuestTable[header.QuestID]->Copy(prograss);
				game.QuestPrograss.push_back(prograss);
			}
		}
		currentPivot += header.size;
		offset += header.size;
	}
		break;
	case STC_Protocol::DeleteQuest:
	{
		STC_DeleteQuest_Header& header = *(STC_DeleteQuest_Header*)currentPivot;
		game.UITourID += 1;
		for (int i = 0; i < game.QuestArr.size(); ++i) {
			if (game.QuestArr[i] == header.QuestID) {
				const int completedQuestId = game.QuestArr[i];
				if (i < (int)game.QuestPrograss.size() && game.QuestPrograss[i] != nullptr) {
					delete game.QuestPrograss[i];
				}
				game.QuestArr.erase(game.QuestArr.begin() + i);
				if (i < (int)game.QuestPrograss.size()) {
					game.QuestPrograss.erase(game.QuestPrograss.begin() + i);
				}
				// 퀘스트 완수 처리
				QuestCompleteShowFlow = 4.0f;
				CurrentCompleteQuest = (completedQuestId >= 0 && completedQuestId < game.QuestTable.size())
					? game.QuestTable[completedQuestId] : nullptr;
				game.mainPageStack.clear();
				break;
			}
		}
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case STC_Protocol::SyncQuestPrograss:
	{
		STC_SyncQuestPrograss_Header& header = *(STC_SyncQuestPrograss_Header*)currentPivot;
		game.UITourID += 1;
		char* ptr = currentPivot + sizeof(STC_SyncQuestPrograss_Header);
		int questSlot = -1;
		for (int i = 0; i < (int)game.QuestArr.size(); ++i) {
			if (game.QuestArr[i] == header.questID) {
				questSlot = i;
				break;
			}
		}
		if (questSlot < 0 && 0 <= header.questID && header.questID < (int)game.QuestTable.size() && game.QuestTable[header.questID] != nullptr) {
			game.QuestArr.push_back(header.questID);
			Quest* prograss = new Quest();
			game.QuestTable[header.questID]->Copy(prograss);
			game.QuestPrograss.push_back(prograss);
			questSlot = (int)game.QuestArr.size() - 1;
		}
		if (0 <= questSlot && questSlot < (int)game.QuestPrograss.size()) {
			Quest* prograss = game.QuestPrograss[questSlot];
			if (prograss != nullptr) {
				int reqCount = min(prograss->requp, header.questReqSiz);
				for (int k = 0; k < reqCount; ++k) {
					prograss->ReqArr[k].PresentCnt = *(int*)ptr;
					ptr += sizeof(int);
				}
				QuestPrograssShowFlow = 3;
				CurrentPrograssQuest = prograss;
			}
		}

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
	// [party] suppress camera look while the party menu / join list / room is open (cursor is free then).
	const bool partyUIOpen = (m_partyMenuOpen || m_partyJoinListOpen ||
		(m_dungeonQueue.active && m_dungeonQueue.partyId >= 0 && currentZoneId < 100));
	if (player != nullptr && mainPageStack.size() == 0 && !partyUIOpen && !StatWindowOpen)
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
	if (uitextureid < 0 || uitextureid >= (int)game.UITextureTable.size()) return;
	GPUResource* uiTex = game.UITextureTable[uitextureid];
	if (uiTex == nullptr || uiTex->resource == nullptr) return;
	if (uiTex->descindex.hCreation.hcpu.ptr == 0) return;

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
	if (di.hcpu.ptr == 0 || di.hgpu.ptr == 0) return;
	gd.pDevice->CopyDescriptorsSimple(1, di.hcpu, uiTex->descindex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	gd.gpucmd->SetGraphicsRootDescriptorTable(1, di.hgpu);
	game.TextMesh->Render(gd.gpucmd, 1);
}

void Game::UIDraw_TextureRect(vec4 loc, vec4 color, float depth, GPUResource* tex) {
	if (tex == nullptr || tex->resource == nullptr) return;
	if (tex->descindex.hCreation.hcpu.ptr == 0) return;
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
	if (di.hcpu.ptr == 0 || di.hgpu.ptr == 0) return;
	gd.pDevice->CopyDescriptorsSimple(1, di.hcpu, tex->descindex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	gd.gpucmd->SetGraphicsRootDescriptorTable(1, di.hgpu);
	game.TextMesh->Render(gd.gpucmd, 1);
}

void Game::UIDraw_TextureLine(vec4 startToEnd, vec4 color, float depth, float LineWidth, int uitextureid)
{
	if (uitextureid < 0 || uitextureid >= (int)game.UITextureTable.size()) return;
	GPUResource* uiTex = game.UITextureTable[uitextureid];
	if (uiTex == nullptr || uiTex->resource == nullptr) return;
	if (uiTex->descindex.hCreation.hcpu.ptr == 0) return;

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
	if (di.hcpu.ptr == 0 || di.hgpu.ptr == 0) return;
	gd.pDevice->CopyDescriptorsSimple(1, di.hcpu, uiTex->descindex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	gd.gpucmd->SetGraphicsRootDescriptorTable(1, di.hgpu);
	game.TextMesh->Render(gd.gpucmd, 1);
}

void Game::UpdateGameplaySkillHUD(float deltaTime)
{
	constexpr float PassiveChargeInterval = 5.0f;
	constexpr float PassiveChargeAmount = 1.0f;
	constexpr float KillChargeAmount = 20.0f;

	if (player == nullptr) {
		UltimateChargePercent = 0.0f;
		UltimateChargePassiveFlow = 0.0f;
		LastUltimateCooldownFlow = 0.0f;
		LastUltimateKillCount = 0;
		LastUltimateJob = -1;
		return;
	}

	if (LastUltimateJob != player->m_currentJob) {
		UltimateChargePercent = 0.0f;
		UltimateChargePassiveFlow = 0.0f;
		LastUltimateKillCount = player->KillCount;
		LastUltimateCooldownFlow = player->SkillCooldownFlow[(int)SkillSlot::Ultimate];
		LastUltimateJob = player->m_currentJob;
		return;
	}

	int killDelta = player->KillCount - LastUltimateKillCount;
	if (killDelta > 0) {
		UltimateChargePercent += KillChargeAmount * (float)killDelta;
	}
	LastUltimateKillCount = player->KillCount;

	UltimateChargePassiveFlow += max(0.0f, deltaTime);
	while (UltimateChargePassiveFlow >= PassiveChargeInterval) {
		UltimateChargePercent += PassiveChargeAmount;
		UltimateChargePassiveFlow -= PassiveChargeInterval;
	}

	float ultimateCooldownFlow = player->SkillCooldownFlow[(int)SkillSlot::Ultimate];
	if (LastUltimateCooldownFlow <= 0.0f && ultimateCooldownFlow > 0.0f) {
		UltimateChargePercent = 0.0f;
		UltimateChargePassiveFlow = 0.0f;
	}
	LastUltimateCooldownFlow = ultimateCooldownFlow;

	UltimateChargePercent = min(100.0f, max(0.0f, UltimateChargePercent));
}

void Game::UpdateFloatingDamageTexts(float deltaTime)
{
	for (FloatingDamageText& text : FloatingDamageTexts) {
		if (!text.Active) continue;
		text.Age += deltaTime;
		if (text.Age >= text.Duration) text.Active = false;
	}
}

void Game::SpawnFloatingDamageText(vec4 worldPosition, float damage)
{
	if (damage < 0.5f) return;

	constexpr size_t MaxFloatingDamageTextCount = 64;
	FloatingDamageText* slot = nullptr;
	for (FloatingDamageText& text : FloatingDamageTexts) {
		if (!text.Active) {
			slot = &text;
			break;
		}
	}

	if (slot == nullptr) {
		if (FloatingDamageTexts.size() < MaxFloatingDamageTextCount) {
			FloatingDamageTexts.push_back(FloatingDamageText{});
			slot = &FloatingDamageTexts.back();
		}
		else {
			slot = &FloatingDamageTexts[0];
			for (FloatingDamageText& text : FloatingDamageTexts) {
				if (text.Age > slot->Age) slot = &text;
			}
		}
	}

	static int spawnCounter = 0;
	float sidePattern = (float)((spawnCounter % 5) - 2);
	spawnCounter++;

	slot->WorldPosition = worldPosition + vec4(0.0f, 1.85f, 0.0f, 0.0f);
	slot->Damage = damage;
	slot->Age = 0.0f;
	slot->Duration = 0.78f;
	slot->SideOffset = sidePattern * 9.0f;
	slot->Active = true;
}

static vec4 GetUltimateHUDColor(int job)
{
	switch (job % 7) {
	case 0: return vec4(0.25f, 0.78f, 1.00f, 1.00f);
	case 1: return vec4(1.00f, 0.55f, 0.20f, 1.00f);
	case 2: return vec4(1.00f, 0.28f, 0.16f, 1.00f);
	case 3: return vec4(0.52f, 0.95f, 0.42f, 1.00f);
	case 4: return vec4(0.70f, 0.50f, 1.00f, 1.00f);
	case 5: return vec4(1.00f, 0.82f, 0.22f, 1.00f);
	default: return vec4(0.35f, 0.95f, 0.82f, 1.00f);
	}
}

static int GetClientExpLimitForLevel(int level)
{
	static constexpr int ExpLimit[100] = {
		100, 105, 110, 116, 122, 128, 134, 141, 148, 155, 163, 171, 180, 189, 198, 208,
		218, 229, 241, 253, 265, 279, 293, 307, 323, 339, 356, 373, 392, 412, 432, 454,
		476, 500, 525, 552, 579, 608, 639, 670, 704, 739, 776, 815, 856, 899, 943, 991,
		1040, 1092, 1147, 1204, 1264, 1327, 1394, 1464, 1537, 1614, 1694, 1779, 1868,
		1961, 2059, 2162, 2270, 2384, 2503, 2628, 2760, 2898, 3043, 3195, 3355, 3522,
		3698, 3883, 4077, 4281, 4495, 4720, 4956, 5204, 5464, 5737, 6024, 6325, 6642,
		6974, 7322, 7689, 8073, 8477, 8901, 9346, 9813, 10303, 10819, 11360, 11928, 12524
	};
	level = max(0, min(99, level));
	return ExpLimit[level];
}

void Game::RenderGameplayStatusHUD()
{
	if (player == nullptr || UITextureTable.empty()) return;

	constexpr int FirstJobIndex = 0;

	const float screenWidth = (float)gd.ClientFrameWidth;
	const float screenHeight = (float)gd.ClientFrameHeight;
	const float scale = max(0.75f, screenHeight / 960.0f);
	const float depth = 0.021f;
	const float left = -screenWidth * 0.5f + 44.0f * scale;
	const float panelBottom = -screenHeight * 0.5f + 170.0f * scale;
	const float panelWidth = 330.0f * scale;
	const float panelHeight = 92.0f * scale;

	float maxHP = max(player->MaxHP, 1.0f);
	float hpRate = min(1.0f, max(0.0f, player->HP / maxHP));
	vec4 panel = vec4(left, panelBottom, left + panelWidth, panelBottom + panelHeight);
	vec4 shadow = panel + vec4(5.0f, -5.0f, 5.0f, -5.0f) * scale;
	UIDraw_SolidRect(shadow, vec4(0.0f, 0.0f, 0.0f, 0.45f), depth + ui_depth_epsilon);
	UIDraw_SolidRect(panel, vec4(0.04f, 0.05f, 0.06f, 0.82f), depth);

	vec4 hpBack = vec4(panel.x + 18.0f * scale, panel.y + 26.0f * scale, panel.z - 18.0f * scale, panel.y + 56.0f * scale);
	vec4 hpFill = hpBack;
	hpFill.z = hpFill.x + (hpBack.z - hpBack.x) * hpRate;
	UIDraw_SolidRect(hpBack, vec4(0.11f, 0.03f, 0.04f, 0.96f), depth - ui_depth_epsilon);
	UIDraw_SolidRect(hpFill, vec4(0.95f, 0.08f, 0.16f, 1.0f), depth - ui_depth_epsilon * 2);
	UIDraw_SolidRect(vec4(hpBack.x, hpBack.w - 4.0f * scale, hpFill.z, hpBack.w), vec4(1.0f, 0.28f, 0.36f, 0.88f), depth - ui_depth_epsilon * 3);

	wchar_t hpText[32] = {};
	swprintf_s(hpText, L"%.0f / %.0f", max(0.0f, player->HP), maxHP);
	vec4 hpTextRt = vec4(hpBack.x + 8.0f * scale, panel.y + 54.0f * scale, hpBack.z, panel.w - 8.0f * scale);
	RenderSDFText(hpText, (int)wcslen(hpText), hpTextRt, 21.0f * scale, vec4(1.0f, 1.0f, 1.0f, 1.0f), nullptr, nullptr, depth - ui_depth_epsilon * 4);

	vec4 labelRt = vec4(panel.x + 18.0f * scale, panel.y + 5.0f * scale, panel.x + 120.0f * scale, panel.y + 30.0f * scale);
	RenderSDFText(L"HP", 2, labelRt, 18.0f * scale, vec4(1.0f, 0.24f, 0.32f, 1.0f), nullptr, nullptr, depth - ui_depth_epsilon * 4);

	int requiredExp = max(1, GetClientExpLimitForLevel(player->Level));
	float expRate = min(1.0f, max(0.0f, (float)player->Exp / (float)requiredExp));
	vec4 expBack = vec4(hpBack.x, hpBack.y - 14.0f * scale, hpBack.z, hpBack.y - 7.0f * scale);
	vec4 expFill = expBack;
	expFill.z = expFill.x + (expBack.z - expBack.x) * expRate;
	UIDraw_SolidRect(expBack, vec4(0.02f, 0.05f, 0.08f, 0.94f), depth - ui_depth_epsilon);
	UIDraw_SolidRect(expFill, vec4(0.14f, 0.68f, 1.0f, 0.98f), depth - ui_depth_epsilon * 2);
	wchar_t levelText[32] = {};
	swprintf_s(levelText, L"LV %d", player->Level);
	vec4 levelRt = vec4(hpBack.z - 72.0f * scale, panel.y + 5.0f * scale, hpBack.z, panel.y + 30.0f * scale);
	RenderSDFText(levelText, (int)wcslen(levelText), levelRt, 17.0f * scale, vec4(0.45f, 0.88f, 1.0f, 1.0f), nullptr, nullptr, depth - ui_depth_epsilon * 4);

	constexpr int AegisJobIndex = 2;
	const float gaugeWidth = 176.0f * scale;
	const float gaugeHeight = 9.0f * scale;
	const float gaugeCenterX = 0.0f;
	const float gaugeY = -58.0f * scale;
	vec4 gaugeBack = vec4(gaugeCenterX - gaugeWidth * 0.5f, gaugeY, gaugeCenterX + gaugeWidth * 0.5f, gaugeY + gaugeHeight);
	vec4 gaugeFill = gaugeBack;

	if (player->m_currentJob == FirstJobIndex) {
		float maxHeat = max(player->MaxHeatGauge, 1.0f);
		float heatRate = min(1.0f, max(0.0f, player->HeatGauge / maxHeat));
		gaugeFill.z = gaugeFill.x + (gaugeBack.z - gaugeBack.x) * heatRate;
		UIDraw_SolidRect(gaugeBack, vec4(0.08f, 0.0f, 0.0f, 0.72f), depth - ui_depth_epsilon);
		UIDraw_SolidRect(gaugeFill, vec4(1.0f, 0.10f, 0.06f, 0.98f), depth - ui_depth_epsilon * 2);
	}
	else if (player->m_currentJob == AegisJobIndex) {
		float maxShield = max(player->MaxShieldDurability, 1.0f);
		float shieldRate = min(1.0f, max(0.0f, player->ShieldDurability / maxShield));
		gaugeFill.z = gaugeFill.x + (gaugeBack.z - gaugeBack.x) * shieldRate;
		UIDraw_SolidRect(gaugeBack, vec4(0.0f, 0.03f, 0.10f, 0.76f), depth - ui_depth_epsilon);
		UIDraw_SolidRect(gaugeFill, vec4(0.12f, 0.58f, 1.0f, 0.98f), depth - ui_depth_epsilon * 2);
		UIDraw_SolidRect(vec4(gaugeBack.x, gaugeBack.w - 2.0f * scale, gaugeFill.z, gaugeBack.w),
			vec4(0.68f, 0.92f, 1.0f, 0.72f), depth - ui_depth_epsilon * 3);
	}

	if (StatWindowOpen) RenderStatWindowHUD();
}

void Game::RenderStatWindowHUD()
{
	if (player == nullptr) return;
	const float screenHeight = (float)gd.ClientFrameHeight;
	const float scale = max(0.75f, screenHeight / 960.0f);
	const float depth = 0.012f;
	vec4 panel = vec4(-265.0f, -180.0f, 265.0f, 215.0f) * scale;
	UIDraw_SolidRect(panel + vec4(5.0f, -5.0f, 5.0f, -5.0f) * scale, vec4(0, 0, 0, 0.45f), depth + ui_depth_epsilon);
	UIDraw_SolidRect(panel, vec4(0.018f, 0.030f, 0.040f, 0.94f), depth);
	UIDraw_SolidRect(vec4(panel.x, panel.w - 3.0f * scale, panel.z, panel.w), vec4(1.0f, 0.48f, 0.08f, 0.90f), depth - ui_depth_epsilon);

	wchar_t title[64] = {};
	swprintf_s(title, L"STATUS POINT %d", player->StatPoint);
	RenderSDFText(title, (int)wcslen(title), vec4(panel.x + 24.0f * scale, panel.w - 48.0f * scale, panel.z - 24.0f * scale, panel.w - 12.0f * scale),
		24.0f * scale, vec4(0.96f, 0.98f, 1.0f, 1.0f), nullptr, nullptr, depth - ui_depth_epsilon * 3);

	const wchar_t* names[4] = { L"HP", L"DEF", L"MOVE", L"ATK" };
	int values[4] = { player->StatHP, player->StatDefense, player->StatMoveSpeed, player->StatAttack };
	const wchar_t* effects[4] = { L"+10 Max HP", L"+3 DEF", L"+0.2% Speed", L"+0.5 ATK / +0.5% Skill" };
	for (int i = 0; i < 4; ++i) {
		float top = panel.w - (82.0f + i * 78.0f) * scale;
		vec4 row = vec4(panel.x + 22.0f * scale, top - 56.0f * scale, panel.z - 22.0f * scale, top);
		UIDraw_SolidRect(row, vec4(0.04f, 0.065f, 0.078f, 0.88f), depth - ui_depth_epsilon);
		vec4 icon = vec4(row.x + 10.0f * scale, row.y + 6.0f * scale, row.x + 58.0f * scale, row.w - 6.0f * scale);
		if (StatIconTextureIndices[i] >= 0) UIDraw_TextureRect(icon, vec4(1, 1, 1, 0.95f), depth - ui_depth_epsilon * 2, StatIconTextureIndices[i]);
		RenderSDFText(names[i], (int)wcslen(names[i]), vec4(row.x + 72.0f * scale, row.y + 23.0f * scale, row.x + 150.0f * scale, row.w - 6.0f * scale),
			20.0f * scale, vec4(1.0f, 0.66f, 0.20f, 1.0f), nullptr, nullptr, depth - ui_depth_epsilon * 3);
		wchar_t valueText[32] = {};
		swprintf_s(valueText, L"%d", values[i]);
		RenderSDFText(valueText, (int)wcslen(valueText), vec4(row.x + 158.0f * scale, row.y + 20.0f * scale, row.x + 215.0f * scale, row.w - 6.0f * scale),
			22.0f * scale, vec4(0.92f, 0.98f, 1.0f, 1.0f), nullptr, nullptr, depth - ui_depth_epsilon * 3);
		RenderSDFText(effects[i], (int)wcslen(effects[i]), vec4(row.x + 220.0f * scale, row.y + 18.0f * scale, row.z - 70.0f * scale, row.w - 7.0f * scale),
			17.0f * scale, vec4(0.72f, 0.86f, 0.92f, 1.0f), nullptr, nullptr, depth - ui_depth_epsilon * 3);
		vec4 plus = vec4(row.z - 50.0f * scale, row.y + 10.0f * scale, row.z - 10.0f * scale, row.w - 10.0f * scale);
		StatButtonRects[i] = plus;
		vec4 btnColor = player->StatPoint > 0 ? vec4(0.10f, 0.54f, 0.72f, 0.94f) : vec4(0.12f, 0.16f, 0.18f, 0.72f);
		UIDraw_SolidRect(plus, btnColor, depth - ui_depth_epsilon * 2);
		RenderSDFText(L"+", 1, plus, 27.0f * scale, vec4(1, 1, 1, 1), nullptr, nullptr, depth - ui_depth_epsilon * 4);
	}
}

bool Game::HandleStatWindowClick(vec4 cursorPos)
{
	if (!StatWindowOpen || player == nullptr || player->StatPoint <= 0) return false;
	for (int i = 0; i < 4; ++i) {
		if (!RectContainPos(StatButtonRects[i], cursorPos)) continue;
		CTS_StatUp_Header header;
		header.stat = (PlayerStatType)i;
		if (client.send_all((char*)&header, sizeof(header), 0) == sizeof(header)) {
			player->StatPoint -= 1;
			switch ((PlayerStatType)i) {
			case PlayerStatType::HP: player->StatHP += 1; break;
			case PlayerStatType::Defense: player->StatDefense += 1; break;
			case PlayerStatType::MoveSpeed: player->StatMoveSpeed += 1; break;
			case PlayerStatType::Attack: player->StatAttack += 1; break;
			default: break;
			}
			return true;
		}
		return false;
	}
	return false;
}

static const wchar_t* GetPartyJobName(int job)
{
	static const wchar_t* JobNames[] = {
		L"JUGGERNAUT", L"FROST", L"AEGIS", L"MAGE", L"HEALER",
		L"GUNNER", L"DRONE", L"HACKER", L"BOMBER"
	};
	return (job >= 0 && job < (int)(sizeof(JobNames) / sizeof(JobNames[0]))) ? JobNames[job] : L"UNKNOWN";
}

void Game::RenderDungeonPartyHUD()
{
	// Show the roster both inside a dungeon AND while sitting in a lobby party (partyId >= 0),
	// so members can see each other's HP/job before the leader starts.
	if (player == nullptr || !m_dungeonQueue.active || m_dungeonQueue.count <= 0) return;
	if (currentZoneId < 100 && m_dungeonQueue.partyId < 0) return;
	if (UITextureTable.size() <= 41) return;

	constexpr int FillTexture = 0;
	constexpr int PartySlotTexture = 38;
	constexpr int HPBarTexture = 40;
	constexpr int HPFillTexture = 41;

	const float screenWidth = (float)gd.ClientFrameWidth;
	const float screenHeight = (float)gd.ClientFrameHeight;
	const float scale = max(0.72f, screenHeight / 960.0f);
	const float depth = 0.020f;
	// Keep one visual row per unique player. This also protects the HUD from a transient duplicate
	// snapshot while a dungeon connection is being replaced.
	int memberSlots[3] = { -1, -1, -1 };
	int memberCount = 0;
	const int snapshotCount = min(3, max(0, m_dungeonQueue.count));
	for (int i = 0; i < snapshotCount; ++i) {
		const int objectIndex = m_dungeonQueue.objindex[i];
		if (objectIndex < 0) continue;
		bool duplicate = false;
		for (int k = 0; k < memberCount; ++k) {
			if (m_dungeonQueue.objindex[memberSlots[k]] == objectIndex &&
				m_dungeonQueue.zoneId[memberSlots[k]] == m_dungeonQueue.zoneId[i]) {
				duplicate = true;
				break;
			}
		}
		if (!duplicate) memberSlots[memberCount++] = i;
	}
	if (memberCount <= 0) return;

	const float panelLeft = -screenWidth * 0.5f + 34.0f * scale;
	const float panelTop = screenHeight * 0.5f - 78.0f * scale;
	const float panelWidth = 346.0f * scale;
	const float headerHeight = 44.0f * scale;
	const float slotHeight = 88.0f * scale;
	const float slotGap = 8.0f * scale;
	const float panelHeight = headerHeight + 18.0f * scale + memberCount * slotHeight + max(0, memberCount - 1) * slotGap;
	const vec4 panel(panelLeft, panelTop - panelHeight, panelLeft + panelWidth, panelTop);

	UIDraw_TextureRect(panel + vec4(5.0f, -5.0f, 5.0f, -5.0f) * scale,
		vec4(0.0f, 0.0f, 0.0f, 0.48f), depth + ui_depth_epsilon, FillTexture);
	UIDraw_TextureRect(panel, vec4(0.018f, 0.035f, 0.045f, 0.92f), depth, FillTexture);
	// The original party-window bitmap contains a decorative fake HP row. Use clean cyan edge strips
	// instead, while retaining the original party-slot and gameplay HP resources for actual members.
	const float edge = 2.0f * scale;
	const vec4 frameColor(0.18f, 0.72f, 0.90f, 0.82f);
	UIDraw_SolidRect(vec4(panel.x, panel.y, panel.z, panel.y + edge), frameColor, depth - ui_depth_epsilon);
	UIDraw_SolidRect(vec4(panel.x, panel.w - edge, panel.z, panel.w), frameColor, depth - ui_depth_epsilon);
	UIDraw_SolidRect(vec4(panel.x, panel.y, panel.x + edge, panel.w), frameColor, depth - ui_depth_epsilon);
	UIDraw_SolidRect(vec4(panel.z - edge, panel.y, panel.z, panel.w), frameColor, depth - ui_depth_epsilon);

	vec4 titleRt(panel.x + 20.0f * scale, panel.w - 40.0f * scale,
		panel.z - 18.0f * scale, panel.w - 8.0f * scale);
	RenderSDFText(L"PARTY", 5, titleRt, 24.0f * scale,
		vec4(0.48f, 0.90f, 1.0f, 1.0f), nullptr, nullptr, depth - ui_depth_epsilon * 3);

	for (int i = 0; i < memberCount; ++i) {
		const int snapshotIndex = memberSlots[i];

		const float slotTop = panel.w - headerHeight - 12.0f * scale - i * (slotHeight + slotGap);
		const vec4 slot(panel.x + 14.0f * scale, slotTop - slotHeight,
			panel.z - 14.0f * scale, slotTop);
		const int job = m_dungeonQueue.m_currentJob[snapshotIndex];
		const vec4 jobColor = GetUltimateHUDColor(job);

		UIDraw_TextureRect(slot, vec4(0.03f, 0.055f, 0.07f, 0.92f), depth - ui_depth_epsilon * 2, FillTexture);
		UIDraw_TextureRect(slot, vec4(jobColor.r, jobColor.g, jobColor.b, 0.42f),
			depth - ui_depth_epsilon * 3, PartySlotTexture);

		const vec4 portrait(slot.x + 10.0f * scale, slot.y + 10.0f * scale,
			slot.x + 76.0f * scale, slot.w - 10.0f * scale);
		UIDraw_TextureRect(portrait, vec4(0.02f, 0.04f, 0.055f, 0.94f), depth - ui_depth_epsilon * 4, PartySlotTexture);
		if (job >= 0 && job < 9) {
			UIDraw_TextureRect(portrait, vec4(0.78f, 0.94f, 1.0f, 0.90f),
				depth - ui_depth_epsilon * 5, &gJobCardTex[job]);
		}

		wchar_t memberText[32] = {};
		swprintf_s(memberText, L"P%d  %ls", i + 1, GetPartyJobName(job));
		const vec4 memberRt(portrait.z + 12.0f * scale, slot.w - 31.0f * scale,
			slot.z - 10.0f * scale, slot.w - 5.0f * scale);
		RenderSDFText(memberText, (int)wcslen(memberText), memberRt, 18.0f * scale,
			vec4(0.92f, 0.98f, 1.0f, 1.0f), nullptr, nullptr, depth - ui_depth_epsilon * 7);

		const float maxHP = max(1.0f, m_dungeonQueue.maxhp[snapshotIndex]);
		const float currentHP = min(maxHP, max(0.0f, m_dungeonQueue.hp[snapshotIndex]));
		const float hpRate = currentHP / maxHP;
		const vec4 hpBack(portrait.z + 12.0f * scale, slot.y + 15.0f * scale,
			slot.z - 70.0f * scale, slot.y + 35.0f * scale);
		vec4 hpFill = hpBack;
		hpFill.z = hpFill.x + (hpBack.z - hpBack.x) * hpRate;
		UIDraw_TextureRect(hpBack, vec4(0.15f, 0.04f, 0.055f, 0.96f),
			depth - ui_depth_epsilon * 4, HPBarTexture);
		if (hpRate > 0.0f) {
			const vec4 hpColor = hpRate <= 0.25f ? vec4(1.0f, 0.10f, 0.12f, 1.0f) :
				vec4(jobColor.r, jobColor.g, jobColor.b, 1.0f);
			UIDraw_TextureRect(hpFill, hpColor, depth - ui_depth_epsilon * 5, HPFillTexture);
		}

		wchar_t hpText[32] = {};
		swprintf_s(hpText, L"%.0f / %.0f", currentHP, maxHP);
		const vec4 hpTextRt(hpBack.z + 7.0f * scale, slot.y + 12.0f * scale,
			slot.z - 6.0f * scale, slot.y + 37.0f * scale);
		RenderSDFText(hpText, (int)wcslen(hpText), hpTextRt, 15.0f * scale,
			vec4(0.96f, 0.98f, 1.0f, 1.0f), nullptr, nullptr, depth - ui_depth_epsilon * 7);
	}
}

void Game::RenderDungeonDeathHUD()
{
	if (m_dungeonResultFlash > 0.0f) m_dungeonResultFlash = max(0.0f, m_dungeonResultFlash - DeltaTime);
	const float screenWidth = (float)gd.ClientFrameWidth;
	const float screenHeight = (float)gd.ClientFrameHeight;
	const float scale = max(0.72f, screenHeight / 960.0f);
	const float depth = 0.019f;

	if (currentZoneId >= 100 && currentZoneId < 106 && m_dungeonQueue.dungeonDeathCount >= 0) {
		const float right = screenWidth * 0.5f - 34.0f * scale;
		const float top = screenHeight * 0.5f - 78.0f * scale;
		const float width = 250.0f * scale;
		const float height = 82.0f * scale;
		const vec4 panel(right - width, top - height, right, top);
		const bool danger = m_dungeonQueue.dungeonDeathCount >= max(0, m_dungeonQueue.dungeonDeathLimit - 1);
		UIDraw_SolidRect(panel, vec4(0.02f, 0.035f, 0.05f, 0.92f), depth);
		const vec4 edgeColor = danger ? vec4(1.0f, 0.22f, 0.18f, 0.95f) : vec4(0.18f, 0.72f, 0.90f, 0.85f);
		const float edge = 2.0f * scale;
		UIDraw_SolidRect(vec4(panel.x, panel.y, panel.z, panel.y + edge), edgeColor, depth - ui_depth_epsilon);
		UIDraw_SolidRect(vec4(panel.x, panel.w - edge, panel.z, panel.w), edgeColor, depth - ui_depth_epsilon);
		UIDraw_SolidRect(vec4(panel.x, panel.y, panel.x + edge, panel.w), edgeColor, depth - ui_depth_epsilon);
		UIDraw_SolidRect(vec4(panel.z - edge, panel.y, panel.z, panel.w), edgeColor, depth - ui_depth_epsilon);
		wchar_t text[48] = {};
		swprintf_s(text, L"DEATH  %d / %d", m_dungeonQueue.dungeonDeathCount, m_dungeonQueue.dungeonDeathLimit);
		const vec4 textRect(panel.x + 18.0f * scale, panel.y + 18.0f * scale,
			panel.z - 12.0f * scale, panel.w - 12.0f * scale);
		RenderSDFText(text, (int)wcslen(text), textRect, 25.0f * scale,
			danger ? vec4(1.0f, 0.35f, 0.30f, 1.0f) : vec4(0.65f, 0.92f, 1.0f, 1.0f),
			nullptr, nullptr, depth - ui_depth_epsilon * 3);
	}

	if (m_dungeonResultFlash > 0.0f && m_dungeonResult != DungeonResultCode::None) {
		const wchar_t* resultText = L"던전 실패";
		vec4 color(1.0f, 0.25f, 0.20f, 1.0f);
		if (m_dungeonResult == DungeonResultCode::Aborted) resultText = L"던전 실패";
		else if (m_dungeonResult == DungeonResultCode::Success) {
			resultText = L"던전 클리어";
			color = vec4(0.35f, 1.0f, 0.55f, 1.0f);
		}
		const float halfWidth = 220.0f * scale;
		const vec4 resultRect(-halfWidth, 80.0f * scale, halfWidth, 140.0f * scale);
		RenderSDFText(resultText, (int)wcslen(resultText), resultRect, 34.0f * scale,
			color, nullptr, nullptr, depth - ui_depth_epsilon * 8);
	}
}

// [party] -------- portal party-lobby UI (immediate mode; clicks handled in WndProc) --------

void Game::PartySendSimple(int protocolType) {
	// Header-only messages are {unsigned int size; CTS_Protocol st;} = 6 bytes packed on the wire.
	// Build the exact 6-byte layout by hand so client/server agree regardless of struct padding.
	char buf[6];
	*(unsigned int*)(buf + 0) = 6;
	*(short*)(buf + 4) = (short)protocolType;
	client.send_all(buf, 6, 0);
}

void Game::UpdateDungeonPortalProximity() {
	m_nearDungeonPortal = false;
	if (player == nullptr) return;
	if (currentZoneId >= 100) return; // only in the open world (dungeon has no entry portal)
	vec4 pp = player->worldMat.pos;
	for (int i = 0; i < (int)Portals.size(); ++i) {
		Portal* portal = Portals[i];
		if (portal == nullptr) continue;
		if (portal->dstzoneId != 100) continue; // dungeon-entry portal marker (DungeonZoneId)
		vec4 d = portal->worldMat.pos - pp;
		float r = portal->radius + 3.0f; // a little generous so the menu opens before you stand dead-center
		if (d.x * d.x + d.z * d.z <= r * r) { m_nearDungeonPortal = true; break; }
	}
}

void Game::RenderPartyLobbyUI() {
	if (m_dungeonRejectFlash > 0.0f) m_dungeonRejectFlash -= DeltaTime;

	UpdateDungeonPortalProximity();

	// Am I already in a party (lobby room)? partyId>=0 and not yet inside the dungeon.
	const bool inParty = (m_dungeonQueue.active && m_dungeonQueue.partyId >= 0 && currentZoneId < 100);
	if (!inParty) {
		// auto open/close the create/join menu from portal proximity only.
		if (m_nearDungeonPortal) {
			if (!m_partyMenuClosed) m_partyMenuOpen = true;   // stays closed if the user pressed X until they leave
		}
		else { m_partyMenuOpen = false; m_partyJoinListOpen = false; m_partyMenuClosed = false; }
	} else {
		m_partyMenuOpen = false;     // the party room replaces the menu once you have joined
		m_partyJoinListOpen = false;
	}

	m_partyButtonCount = 0;
	auto addButton = [&](vec4 rect, int action, int param) {
		if (m_partyButtonCount < 24) {
			m_partyButtons[m_partyButtonCount].rect = rect;
			m_partyButtons[m_partyButtonCount].action = action;
			m_partyButtons[m_partyButtonCount].param = param;
			m_partyButtonCount++;
		}
	};

	const float sw = (float)gd.ClientFrameWidth;
	const float sh = (float)gd.ClientFrameHeight;
	const float scale = max(0.72f, sh / 960.0f);
	const float depth = 0.015f;
	constexpr int Fill = 0;
	const vec4 frameCol(0.18f, 0.72f, 0.90f, 0.85f);

	auto drawPanel = [&](vec4 p) {
		UIDraw_SolidRect(p, vec4(0.02f, 0.04f, 0.06f, 0.93f), depth);
		const float e = 2.0f * scale;
		UIDraw_SolidRect(vec4(p.x, p.y, p.z, p.y + e), frameCol, depth - ui_depth_epsilon);
		UIDraw_SolidRect(vec4(p.x, p.w - e, p.z, p.w), frameCol, depth - ui_depth_epsilon);
		UIDraw_SolidRect(vec4(p.x, p.y, p.x + e, p.w), frameCol, depth - ui_depth_epsilon);
		UIDraw_SolidRect(vec4(p.z - e, p.y, p.z, p.w), frameCol, depth - ui_depth_epsilon);
	};
	auto drawButton = [&](vec4 r, const wchar_t* label, vec4 col, int action, int param) {
		UIDraw_SolidRect(r, col, depth - ui_depth_epsilon * 2);
		const float e = 1.5f * scale;
		UIDraw_SolidRect(vec4(r.x, r.y, r.z, r.y + e), frameCol, depth - ui_depth_epsilon * 3);
		UIDraw_SolidRect(vec4(r.x, r.w - e, r.z, r.w), frameCol, depth - ui_depth_epsilon * 3);
		vec4 tr(r.x + 10.0f * scale, r.y + 4.0f * scale, r.z - 10.0f * scale, r.w - 4.0f * scale);
		RenderSDFText(label, (int)wcslen(label), tr, 20.0f * scale, vec4(0.95f, 0.99f, 1.0f, 1.0f), nullptr, nullptr, depth - ui_depth_epsilon * 5);
		addButton(r, action, param);
	};

	// reject flash (centered, near top).
	if (m_dungeonRejectFlash > 0.0f) {
		vec4 rt(-220.0f * scale, sh * 0.5f - 150.0f * scale, 220.0f * scale, sh * 0.5f - 110.0f * scale);
		RenderSDFText(L"던전이 가득 찼습니다", 11, rt, 26.0f * scale, vec4(1.0f, 0.35f, 0.30f, 1.0f), nullptr, nullptr, depth - ui_depth_epsilon * 6);
	}

	const float bw = 240.0f * scale;
	const float bh = 52.0f * scale;
	const float gap = 14.0f * scale;

	if (inParty) {
		// ---- compact party room, anchored at the top so it doesn't block the view ----
		// title "파티N" + [X] (leader: disband / member: leave) on the header row, 인원 below, [시작] for the leader.
		const float cx = 0.0f;
		const float pad = 14.0f * scale;
		const float titleH = 30.0f * scale;
		const float countH = 24.0f * scale;
		const float bh2 = 42.0f * scale;   // compact button height
		const bool amLeader = (m_dungeonQueue.leaderClientIndex >= 0 && m_dungeonQueue.leaderClientIndex == clientIndexInServer);

		const float innerW = bw;
		const float panelW = innerW + pad * 2.0f;
		const float contentH = titleH + 6.0f * scale + countH + (amLeader ? (gap + bh2) : 0.0f);
		const float panelTopY = sh * 0.5f - 20.0f * scale;   // near the top edge
		const float panelBotY = panelTopY - (pad * 2.0f + contentH);
		vec4 panel(cx - panelW * 0.5f, panelBotY, cx + panelW * 0.5f, panelTopY);
		drawPanel(panel);

		wchar_t title[32];
		swprintf_s(title, L"파티%d", m_dungeonQueue.number);
		vec4 titleRt(panel.x + pad, panel.w - pad - titleH, panel.z - 46.0f * scale, panel.w - pad);
		RenderSDFText(title, (int)wcslen(title), titleRt, 22.0f * scale, vec4(0.5f, 0.92f, 1.0f, 1.0f), nullptr, nullptr, depth - ui_depth_epsilon * 4);

		drawButton(vec4(panel.z - 42.0f * scale, panel.w - pad - titleH, panel.z - pad, panel.w - pad),
			L"X", vec4(0.40f, 0.12f, 0.14f, 0.95f), amLeader ? PUI_Disband : PUI_Leave, 0);

		wchar_t memline[48];
		swprintf_s(memline, L"인원 %d / %d", m_dungeonQueue.count, m_dungeonQueue.maxCount);
		const float countTop = panel.w - pad - titleH - 6.0f * scale;
		vec4 memRt(panel.x + pad, countTop - countH, panel.z - pad, countTop);
		RenderSDFText(memline, (int)wcslen(memline), memRt, 18.0f * scale, vec4(0.9f, 0.96f, 1.0f, 1.0f), nullptr, nullptr, depth - ui_depth_epsilon * 4);

		if (amLeader) {
			const float by = countTop - countH - gap;
			drawButton(vec4(cx - innerW * 0.5f, by - bh2, cx + innerW * 0.5f, by), L"시작", vec4(0.10f, 0.45f, 0.22f, 0.95f), PUI_Start, 0);
		}
		return;
	}

	if (!m_partyMenuOpen) return;

	if (!m_partyJoinListOpen) {
		// ---- create/join menu ----
		const float cx = 0.0f;
		float by = 80.0f * scale;
		vec4 panel(cx - bw * 0.5f - 16.0f * scale, by - bh * 2 - gap - 50.0f * scale, cx + bw * 0.5f + 16.0f * scale, by + 36.0f * scale);
		drawPanel(panel);
		vec4 titleRt(panel.x + 16.0f * scale, panel.w - 34.0f * scale, panel.z - 48.0f * scale, panel.w - 4.0f * scale);
		RenderSDFText(L"던전 파티", 5, titleRt, 22.0f * scale, vec4(0.5f, 0.92f, 1.0f, 1.0f), nullptr, nullptr, depth - ui_depth_epsilon * 4);
		// close (X) on the title row.
		drawButton(vec4(panel.z - 42.0f * scale, panel.w - 36.0f * scale, panel.z - 8.0f * scale, panel.w - 6.0f * scale), L"X", vec4(0.40f, 0.12f, 0.14f, 0.95f), PUI_CloseMenu, 0);
		drawButton(vec4(cx - bw * 0.5f, by - bh, cx + bw * 0.5f, by), L"파티 만들기", vec4(0.10f, 0.30f, 0.50f, 0.95f), PUI_Create, 0);
		by -= bh + gap;
		drawButton(vec4(cx - bw * 0.5f, by - bh, cx + bw * 0.5f, by), L"파티 참가하기", vec4(0.12f, 0.32f, 0.40f, 0.95f), PUI_OpenJoin, 0);
		return;
	}

	// ---- join list ----
	{
		const float cx = 0.0f;
		const float listTop = 140.0f * scale;
		const int n = m_partyList.count;
		const float rowH = 46.0f * scale;
		const float panelH = 70.0f * scale + (n > 0 ? n : 1) * (rowH + 8.0f * scale) + 60.0f * scale;
		vec4 panel(cx - bw * 0.5f - 18.0f * scale, listTop - panelH, cx + bw * 0.5f + 18.0f * scale, listTop + 36.0f * scale);
		drawPanel(panel);
		vec4 titleRt(panel.x + 16.0f * scale, panel.w - 34.0f * scale, panel.z - 90.0f * scale, panel.w - 4.0f * scale);
		RenderSDFText(L"파티 참가", 5, titleRt, 22.0f * scale, vec4(0.5f, 0.92f, 1.0f, 1.0f), nullptr, nullptr, depth - ui_depth_epsilon * 4);
		// refresh + close on the title row.
		drawButton(vec4(panel.z - 84.0f * scale, panel.w - 36.0f * scale, panel.z - 46.0f * scale, panel.w - 6.0f * scale), L"↻", vec4(0.10f, 0.30f, 0.40f, 0.95f), PUI_Refresh, 0);
		drawButton(vec4(panel.z - 42.0f * scale, panel.w - 36.0f * scale, panel.z - 8.0f * scale, panel.w - 6.0f * scale), L"X", vec4(0.40f, 0.12f, 0.14f, 0.95f), PUI_CloseJoin, 0);

		float ry = listTop - 50.0f * scale;
		if (n <= 0) {
			vec4 rt(panel.x + 16.0f * scale, ry - rowH, panel.z - 16.0f * scale, ry);
			RenderSDFText(L"열린 파티가 없습니다", 11, rt, 18.0f * scale, vec4(0.8f, 0.85f, 0.9f, 1.0f), nullptr, nullptr, depth - ui_depth_epsilon * 4);
		}
		for (int i = 0; i < n; ++i) {
			PartyListState::Entry& e = m_partyList.entries[i];
			wchar_t label[48];
			swprintf_s(label, L"파티%d  (%d/%d)%ls", e.number, e.memberCount, e.maxCount, e.started ? L" 진행중" : L"");
			vec4 r(cx - bw * 0.5f, ry - rowH, cx + bw * 0.5f, ry);
			bool joinable = (e.started == 0 && e.memberCount < e.maxCount);
			vec4 col = joinable ? vec4(0.10f, 0.28f, 0.42f, 0.95f) : vec4(0.18f, 0.18f, 0.20f, 0.9f);
			UIDraw_SolidRect(r, col, depth - ui_depth_epsilon * 2);
			vec4 tr(r.x + 10.0f * scale, r.y + 4.0f * scale, r.z - 10.0f * scale, r.w - 4.0f * scale);
			RenderSDFText(label, (int)wcslen(label), tr, 18.0f * scale, vec4(0.95f, 0.99f, 1.0f, 1.0f), nullptr, nullptr, depth - ui_depth_epsilon * 5);
			if (joinable) addButton(r, PUI_Join, e.partyId);
			ry -= rowH + 8.0f * scale;
		}
	}
}

void Game::HandlePartyClick() {
	for (int i = 0; i < m_partyButtonCount; ++i) {
		if (!RectContainPos(m_partyButtons[i].rect, CurrentCursorPos)) continue;
		switch (m_partyButtons[i].action) {
		case PUI_Create:   PartySendSimple(CTS_Protocol::PartyCreate); break;
		case PUI_OpenJoin: m_partyJoinListOpen = true; PartySendSimple(CTS_Protocol::PartyListRequest); break;
		case PUI_Refresh:  PartySendSimple(CTS_Protocol::PartyListRequest); break;
		case PUI_CloseJoin:m_partyJoinListOpen = false; break;
		case PUI_CloseMenu:m_partyMenuOpen = false; m_partyJoinListOpen = false; m_partyMenuClosed = true; break;
		case PUI_Start:    PartySendSimple(CTS_Protocol::DungeonStart); break;
		case PUI_Leave:    PartySendSimple(CTS_Protocol::PartyLeave); break;
		case PUI_Disband:  PartySendSimple(CTS_Protocol::PartyDisband); break;
		case PUI_Join:
		{
			CTS_PartyJoin_Header pkt;
			pkt.size = sizeof(pkt);
			pkt.st = CTS_Protocol::PartyJoin;
			pkt.partyId = m_partyButtons[i].param;
			client.send_all((char*)&pkt, sizeof(pkt), 0);
			break;
		}
		default: break;
		}
		break; // one click acts on at most one button
	}
}

void Game::RenderAmmoHUD()
{
	if (player == nullptr || AmmoHUDFrameTextureIndex < 0 || AmmoHUDBulletTextureIndex < 0) return;

	int weaponTypeIndex = player->m_currentWeaponType;
	if (weaponTypeIndex < 0 || weaponTypeIndex >= (int)WeaponType::Max) return;
	int weaponTextureIndex = AmmoHUDWeaponTextureIndices[weaponTypeIndex];
	if (weaponTextureIndex < 0 || weaponTextureIndex >= (int)UITextureTable.size()) return;

	const float screenWidth = (float)gd.ClientFrameWidth;
	const float screenHeight = (float)gd.ClientFrameHeight;
	const float scale = max(0.72f, screenHeight / 960.0f);
	const float depth = 0.0185f;
	const float panelWidth = 320.0f * scale;
	const float panelHeight = 168.0f * scale;
	const float right = screenWidth * 0.5f - 34.0f * scale;
	const float bottom = -screenHeight * 0.5f + 34.0f * scale;
	const float left = right - panelWidth;
	const float top = bottom + panelHeight;
	vec4 panelRt = vec4(left, bottom, right, top);

	UIDraw_TextureRect(panelRt, vec4(1.0f, 1.0f, 1.0f, 0.96f), depth, AmmoHUDFrameTextureIndex);

	float iconWidth = 196.0f * scale;
	float iconHeight = 66.0f * scale;
	switch ((WeaponType)weaponTypeIndex) {
	case WeaponType::Sniper:
		iconWidth = 212.0f * scale;
		iconHeight = 50.0f * scale;
		break;
	case WeaponType::Shotgun:
		iconWidth = 198.0f * scale;
		iconHeight = 55.0f * scale;
		break;
	case WeaponType::Rifle:
		iconWidth = 212.0f * scale;
		iconHeight = 64.0f * scale;
		break;
	case WeaponType::Pistol:
	case WeaponType::DronePistol:
		iconWidth = 100.0f * scale;
		iconHeight = 72.0f * scale;
		break;
	case WeaponType::DualPistol:
		// The dual-pistol source has generous transparent vertical padding.
		iconWidth = 230.0f * scale;
		iconHeight = 150.0f * scale;
		break;
	case WeaponType::SMG:
		iconWidth = 260.0f * scale;
		iconHeight = 120.0f * scale;
		break;
	case WeaponType::GrenadeGun:
		iconWidth = 250.0f * scale;
		iconHeight = 145.0f * scale;
		break;
	case WeaponType::MachineGun:
	default:
		break;
	}

	const float iconCenterX = left + panelWidth * 0.56f;
	const float iconCenterY = bottom + panelHeight * 0.69f;
	vec4 weaponRt = vec4(iconCenterX - iconWidth * 0.5f, iconCenterY - iconHeight * 0.5f,
		iconCenterX + iconWidth * 0.5f, iconCenterY + iconHeight * 0.5f);
	bool isReloading = player->ReloadRemain > 0.0f;
	vec4 weaponColor = isReloading ? vec4(0.46f, 0.54f, 0.58f, 0.72f) : vec4(1.0f, 1.0f, 1.0f, 0.96f);
	UIDraw_TextureRect(weaponRt, weaponColor, depth - ui_depth_epsilon, weaponTextureIndex);

	vec4 bulletRt = vec4(left + 25.0f * scale, bottom + 25.0f * scale,
		left + 48.0f * scale, bottom + 78.0f * scale);
	UIDraw_TextureRect(bulletRt, vec4(1.0f, 1.0f, 1.0f, 0.94f), depth - ui_depth_epsilon * 2, AmmoHUDBulletTextureIndex);

	// WeaponType is the authoritative replicated selection. Use the shared table
	// for HUD capacity so a transient/stale loadout slot cannot display garbage.
	WeaponData weaponData = GWeaponTable[weaponTypeIndex];
	if ((WeaponType)weaponTypeIndex == WeaponType::Sniper) {
		weaponData.maxBullets = player->m_sniperDmrMode ? 15 : 5;
		weaponData.reloadTime = player->m_sniperDmrMode ? 1.4f : 2.0f;
	}
	int magazineCapacity = max(0, weaponData.maxBullets);
	int currentBullets = min(max(0, player->bullets), magazineCapacity);
	wchar_t currentText[16] = {};
	wchar_t capacityText[16] = {};
	swprintf_s(currentText, L"%d", currentBullets);
	swprintf_s(capacityText, L"%d", magazineCapacity);

	vec4 currentRt = vec4(left + 58.0f * scale, bottom + 18.0f * scale,
		left + 181.0f * scale, bottom + 83.0f * scale);
	vec4 slashRt = vec4(left + 174.0f * scale, bottom + 24.0f * scale,
		left + 208.0f * scale, bottom + 76.0f * scale);
	vec4 capacityRt = vec4(left + 202.0f * scale, bottom + 25.0f * scale,
		right - 20.0f * scale, bottom + 75.0f * scale);
	vec4 currentColor = currentBullets <= max(1, magazineCapacity / 4)
		? vec4(1.0f, 0.30f, 0.20f, 1.0f)
		: vec4(1.0f, 1.0f, 1.0f, 1.0f);
	RenderSDFText(currentText, (int)wcslen(currentText), currentRt, 43.0f * scale, currentColor, nullptr, nullptr, depth - ui_depth_epsilon * 3);
	RenderSDFText(L"/", 1, slashRt, 31.0f * scale, vec4(0.18f, 0.88f, 1.0f, 1.0f), nullptr, nullptr, depth - ui_depth_epsilon * 3);
	RenderSDFText(capacityText, (int)wcslen(capacityText), capacityRt, 27.0f * scale, vec4(0.92f, 0.95f, 0.98f, 1.0f), nullptr, nullptr, depth - ui_depth_epsilon * 3);

	if ((WeaponType)weaponTypeIndex == WeaponType::Sniper) {
		const wchar_t* modeText = player->m_sniperDmrMode ? L"DMR" : L"SR";
		vec4 modeRt = vec4(right - 116.0f * scale, top - 48.0f * scale,
			right - 22.0f * scale, top - 13.0f * scale);
		RenderSDFText(modeText, (int)wcslen(modeText), modeRt, 24.0f * scale,
			vec4(0.24f, 0.90f, 1.0f, 1.0f), nullptr, nullptr, depth - ui_depth_epsilon * 3);
	}

	if ((PlayerJob)player->m_currentJob == PlayerJob::Bomber && (WeaponType)weaponTypeIndex == WeaponType::GrenadeGun) {
		const wchar_t* modeText = player->m_bomberHealAmmoMode ? L"HEAL" : L"FIRE";
		vec4 modeRt = vec4(right - 126.0f * scale, top - 49.0f * scale,
			right - 20.0f * scale, top - 12.0f * scale);
		vec4 modeColor = player->m_bomberHealAmmoMode
			? vec4(0.24f, 1.0f, 0.48f, 1.0f)
			: vec4(1.0f, 0.38f, 0.10f, 1.0f);
		RenderSDFText(modeText, (int)wcslen(modeText), modeRt, 22.0f * scale,
			modeColor, nullptr, nullptr, depth - ui_depth_epsilon * 3);
	}
	if (isReloading && AmmoHUDReloadTextureIndex >= 0) {
		float reloadTime = max(weaponData.reloadTime, 0.01f);
		float reloadRate = min(1.0f, max(0.0f, 1.0f - player->ReloadRemain / reloadTime));
		vec4 reloadRt = vec4(right - 54.0f * scale, top - 55.0f * scale,
			right - 20.0f * scale, top - 21.0f * scale);
		UIDraw_TextureRect(reloadRt, vec4(0.72f, 0.96f, 1.0f, 0.92f), depth - ui_depth_epsilon * 2, AmmoHUDReloadTextureIndex);
		vec4 reloadBack = vec4(left + 24.0f * scale, bottom + 12.0f * scale,
			right - 24.0f * scale, bottom + 17.0f * scale);
		vec4 reloadFill = reloadBack;
		reloadFill.z = reloadFill.x + (reloadBack.z - reloadBack.x) * reloadRate;
		UIDraw_SolidRect(reloadBack, vec4(0.02f, 0.08f, 0.10f, 0.82f), depth - ui_depth_epsilon * 4);
		UIDraw_SolidRect(reloadFill, vec4(0.16f, 0.88f, 1.0f, 0.98f), depth - ui_depth_epsilon * 5);
		vec4 reloadTextRt = vec4(left + 92.0f * scale, top - 43.0f * scale,
			right - 60.0f * scale, top - 14.0f * scale);
		RenderSDFText(L"RELOADING", 9, reloadTextRt, 17.0f * scale,
			vec4(0.72f, 0.96f, 1.0f, 1.0f), nullptr, nullptr, depth - ui_depth_epsilon * 3);
	}
}

void Game::RenderSniperScopeHUD()
{
	if (player == nullptr || ScopeOverlayTextureIndex < 0) return;
	if (ScopeOverlayTextureIndex >= (int)UITextureTable.size()) return;
	if (!player->m_isZooming) return;
	if ((WeaponType)player->m_currentWeaponType != WeaponType::Sniper) return;

	const float screenWidth = (float)gd.ClientFrameWidth;
	const float screenHeight = (float)gd.ClientFrameHeight;
	if (screenWidth <= 0.0f || screenHeight <= 0.0f) return;

	const float zoomAlpha = min(1.0f, max(0.0f, (60.0f - player->m_currentFov) / (60.0f - 9.0f)));
	const float alpha = 0.10f + 0.50f * zoomAlpha;
	const float depth = 0.001f;
	vec4 scopeRt = vec4(-screenWidth * 0.5f, -screenHeight * 0.5f, screenWidth * 0.5f, screenHeight * 0.5f);
	UIDraw_TextureRect(scopeRt, vec4(1.0f, 1.0f, 1.0f, alpha), depth, ScopeOverlayTextureIndex);
}

void Game::RenderDamageFeedbackHUD()
{
	if (UITextureTable.empty()) return;

	const float screenWidth = (float)gd.ClientFrameWidth;
	const float screenHeight = (float)gd.ClientFrameHeight;
	if (screenWidth <= 0.0f || screenHeight <= 0.0f) return;

	float hpRate = 1.0f;
	if (player != nullptr && player->MaxHP > 0.0f) {
		hpRate = min(1.0f, max(0.0f, player->HP / player->MaxHP));
	}

	float lowHpAlpha = 0.0f;
	if (hpRate <= 0.13f) {
		lowHpAlpha = 0.16f + 0.13f * (0.5f + 0.5f * sinf(highFrequencyFlow * 9.0f));
	}
	else if (hpRate <= 0.27f) {
		lowHpAlpha = 0.16f;
	}

	const float alpha = min(0.78f, max(DamageBorderAlpha, lowHpAlpha));
	// Screen-space UI is clipped outside the D3D depth range. Keep this overlay
	// just in front of the regular HUD instead of using a negative depth.
	const float depth = 0.0002f;
	if (WhiteDamageFlashAlpha > 0.0f) {
		vec4 full = vec4(-screenWidth * 0.5f, -screenHeight * 0.5f, screenWidth * 0.5f, screenHeight * 0.5f);
		UIDraw_SolidRect(full, vec4(1.0f, 0.94f, 0.88f, min(0.45f, WhiteDamageFlashAlpha)), 0.0001f);
	}
	if (alpha <= 0.001f) return;

	const float scale = max(0.75f, screenHeight / 960.0f);
	const float edge = min(screenHeight * 0.16f, 112.0f * scale);
	const float halfW = screenWidth * 0.5f;
	const float halfH = screenHeight * 0.5f;

	// Build a soft vignette from narrow layers. The outer edge stays vivid while
	// the inner layers rapidly fade, matching a damage-border image without an
	// extra texture asset.
	constexpr int LayerCount = 9;
	for (int layer = 0; layer < LayerCount; ++layer) {
		const float outerRate = (float)layer / (float)LayerCount;
		const float innerRate = (float)(layer + 1) / (float)LayerCount;
		const float outer = edge * outerRate;
		const float inner = edge * innerRate;
		const float fade = 1.0f - outerRate;
		const float layerAlpha = alpha * (0.075f + 0.19f * fade * fade);
		const vec4 red = vec4(1.0f, 0.015f, 0.025f, layerAlpha);
		const vec4 sideRed = vec4(1.0f, 0.015f, 0.025f, layerAlpha * 0.82f);

		UIDraw_SolidRect(vec4(-halfW + outer, halfH - inner, halfW - outer, halfH - outer), red, depth);
		UIDraw_SolidRect(vec4(-halfW + outer, -halfH + outer, halfW - outer, -halfH + inner), red, depth);
		UIDraw_SolidRect(vec4(-halfW + outer, -halfH + inner, -halfW + inner, halfH - inner), sideRed, depth);
		UIDraw_SolidRect(vec4(halfW - inner, -halfH + inner, halfW - outer, halfH - inner), sideRed, depth);
	}
}

void Game::RenderBossPrototypeHUD()
{
	if (!BossPrototypeEnabled || player == nullptr || UITextureTable.empty()) return;

	float bossHP = BossPrototypeHP;
	float bossMaxHP = BossPrototypeMaxHP;
	vec4 bossPosition = BossPrototypeCenter;
	if (BossPrototypeIndex >= 0 && BossPrototypeIndex < (int)DynmaicGameObjects.size()) {
		Monster* boss = dynamic_cast<Monster*>(DynmaicGameObjects[BossPrototypeIndex]);
		if (boss != nullptr) {
			bossHP = boss->HP;
			bossMaxHP = boss->MaxHP;
			bossPosition = boss->worldMat.pos;
		}
	}
	if (bossMaxHP <= 0.0f || bossHP <= 0.0f) return;

	constexpr int FillTexture = 0; // temp: DefaultTex
	constexpr float VisibleDistance = 72.0f;

	vec4 delta = player->worldMat.pos - bossPosition;
	float distanceSq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
	if (distanceSq > VisibleDistance * VisibleDistance) return;

	const float screenWidth = (float)gd.ClientFrameWidth;
	const float screenHeight = (float)gd.ClientFrameHeight;
	const float scale = max(0.75f, screenHeight / 960.0f);
	const float depth = 0.018f;
	const float barWidth = min(screenWidth * 0.62f, 760.0f * scale);
	const float barHeight = 20.0f * scale;
	const float top = screenHeight * 0.5f - 58.0f * scale;
	const float left = -barWidth * 0.5f;
	const float right = barWidth * 0.5f;
	const float bottom = top - barHeight;

	float hpRate = min(1.0f, max(0.0f, bossHP / max(1.0f, bossMaxHP)));
	vec4 barBack = vec4(left, bottom, right, top);
	vec4 barFill = barBack;
	barFill.z = barFill.x + (barBack.z - barBack.x) * hpRate;
	vec4 frame = barBack + vec4(-5.0f, -5.0f, 5.0f, 5.0f) * scale;
	vec4 shadow = frame + vec4(4.0f, -4.0f, 4.0f, -4.0f) * scale;

	UIDraw_TextureRect(shadow, vec4(0.0f, 0.0f, 0.0f, 0.36f), depth + ui_depth_epsilon, FillTexture);
	UIDraw_TextureRect(frame, vec4(0.015f, 0.010f, 0.010f, 0.78f), depth, FillTexture);
	UIDraw_TextureRect(barBack, vec4(0.12f, 0.015f, 0.018f, 0.94f), depth - ui_depth_epsilon, FillTexture);
	UIDraw_TextureRect(barFill, vec4(0.88f, 0.04f, 0.08f, 1.0f), depth - ui_depth_epsilon * 2, FillTexture);
	UIDraw_TextureRect(vec4(barBack.x, barBack.w - 4.0f * scale, barFill.z, barBack.w),
		vec4(1.0f, 0.25f, 0.30f, 0.72f), depth - ui_depth_epsilon * 3, FillTexture);

	if (BossPrototypeShieldActive) {
		vec4 shieldLine = vec4(barBack.x, barBack.y - 5.0f * scale, barBack.z, barBack.y - 2.0f * scale);
		UIDraw_TextureRect(shieldLine, vec4(0.12f, 0.48f, 1.0f, 0.76f), depth - ui_depth_epsilon * 4, FillTexture);
	}

	vec4 titleRt = vec4(-220.0f * scale, top + 8.0f * scale, 220.0f * scale, top + 42.0f * scale);
	RenderSDFText(L"Turret Boss", 11, titleRt + vec4(1.5f, -1.5f, 1.5f, -1.5f) * scale,
		28.0f * scale, vec4(0.0f, 0.0f, 0.0f, 0.68f), nullptr, nullptr, depth - ui_depth_epsilon * 5);
	RenderSDFText(L"Turret Boss", 11, titleRt,
		28.0f * scale, vec4(1.0f, 0.86f, 0.82f, 1.0f), nullptr, nullptr, depth - ui_depth_epsilon * 6);
}

void Game::RenderBossPrototypeCoreHealthPlates()
{
	if (!BossPrototypeEnabled || player == nullptr || UITextureTable.empty() || BossPrototypeCores.empty()) return;

	constexpr int FillTexture = 0; // temp: DefaultTex
	constexpr float VisibleDistance = 92.0f;
	constexpr float AnchorOffsetY = 2.18f;

	const float screenWidth = (float)gd.ClientFrameWidth;
	const float screenHeight = (float)gd.ClientFrameHeight;
	const float baseScale = max(0.82f, screenHeight / 960.0f);
	const float depth = 0.0185f;
	matrix cameraWorld = gd.viewportArr[0].ViewMatrix.RTInverse;
	vec4 cameraPos = cameraWorld.pos;

	for (const BossPrototypeCore& core : BossPrototypeCores) {
		if (!core.Active || core.MaxHP <= 0.0f || core.HP <= 0.0f) continue;

		vec4 anchor = core.Position + vec4(0.0f, AnchorOffsetY, 0.0f, 0.0f);
		float dx = anchor.x - cameraPos.x;
		float dy = anchor.y - cameraPos.y;
		float dz = anchor.z - cameraPos.z;
		float distance = sqrtf(dx * dx + dy * dy + dz * dz);
		if (distance > VisibleDistance) continue;

		XMFLOAT3 screenPos = {};
		XMStoreFloat3(&screenPos, gd.viewportArr[0].project(anchor));
		if (screenPos.z < 0.0f || screenPos.z > 1.0f) continue;
		if (screenPos.x < -90.0f || screenPos.x > screenWidth + 90.0f ||
			screenPos.y < -70.0f || screenPos.y > screenHeight + 70.0f) continue;

		float distanceRate = min(1.0f, max(0.0f, (distance - 18.0f) / max(1.0f, VisibleDistance - 18.0f)));
		float plateScale = baseScale * (1.00f - distanceRate * 0.18f);
		float alpha = min(0.98f, max(0.58f, 0.96f - distanceRate * 0.24f));
		float hpRate = min(1.0f, max(0.0f, core.HP / core.MaxHP));
		float uiX = screenPos.x - screenWidth * 0.5f;
		float uiY = screenHeight * 0.5f - screenPos.y;

		float barWidth = 148.0f * plateScale;
		float barHeight = 13.0f * plateScale;
		float pad = 4.0f * plateScale;
		vec4 plate = vec4(uiX - barWidth * 0.5f - pad, uiY - barHeight * 0.5f - pad,
			uiX + barWidth * 0.5f + pad, uiY + barHeight * 0.5f + pad);
		vec4 barBack = vec4(uiX - barWidth * 0.5f, uiY - barHeight * 0.5f,
			uiX + barWidth * 0.5f, uiY + barHeight * 0.5f);
		vec4 barFill = barBack;
		barFill.z = barFill.x + (barBack.z - barBack.x) * hpRate;

		UIDraw_TextureRect(plate + vec4(1.5f, -1.5f, 1.5f, -1.5f) * plateScale, vec4(0.0f, 0.0f, 0.0f, 0.26f * alpha), depth + ui_depth_epsilon, FillTexture);
		UIDraw_TextureRect(plate, vec4(0.02f, 0.025f, 0.035f, 0.58f * alpha), depth, FillTexture);
		UIDraw_TextureRect(barBack, vec4(0.07f, 0.025f, 0.035f, 0.80f * alpha), depth - ui_depth_epsilon, FillTexture);
		UIDraw_TextureRect(barFill, vec4(1.0f, 0.10f, 0.16f, 0.96f * alpha), depth - ui_depth_epsilon * 2, FillTexture);
		UIDraw_TextureRect(vec4(barBack.x, barBack.w - 2.0f * plateScale, barFill.z, barBack.w),
			vec4(1.0f, 0.42f, 0.46f, 0.62f * alpha), depth - ui_depth_epsilon * 3, FillTexture);

		vec4 labelRt = vec4(uiX - 40.0f * plateScale, uiY + 9.0f * plateScale,
			uiX + 40.0f * plateScale, uiY + 31.0f * plateScale);
		RenderSDFText(L"CORE", 4, labelRt + vec4(1.0f, -1.0f, 1.0f, -1.0f) * plateScale,
			15.0f * plateScale, vec4(0.0f, 0.0f, 0.0f, 0.60f * alpha), nullptr, nullptr, depth - ui_depth_epsilon * 4);
		RenderSDFText(L"CORE", 4, labelRt,
			15.0f * plateScale, vec4(0.90f, 0.96f, 1.0f, alpha), nullptr, nullptr, depth - ui_depth_epsilon * 5);
	}
}

void Game::RenderMonsterHealthPlates()
{
	if (UITextureTable.empty()) return;

	constexpr int FillTexture = 0; // temp: DefaultTex
	constexpr float DamagedVisibleDistance = 42.0f;
	constexpr float HitVisibleDistance = 62.0f;
	constexpr float HeadOffsetY = 1.92f;

	const float screenWidth = (float)gd.ClientFrameWidth;
	const float screenHeight = (float)gd.ClientFrameHeight;
	const float baseScale = max(0.75f, screenHeight / 960.0f);
	const float depth = 0.019f;
	matrix cameraWorld = gd.viewportArr[0].ViewMatrix.RTInverse;
	vec4 cameraPos = cameraWorld.pos;

	for (int i = 0; i < (int)DynmaicGameObjects.size(); ++i) {
		if (i == BossPrototypeIndex) continue;
		DynamicGameObject* dgo = DynmaicGameObjects[i];
		if (dgo == nullptr || dgo->tag[GameObjectTag::Tag_Enable] == false) continue;

		Monster* monster = dynamic_cast<Monster*>(dgo);
		if (monster == nullptr || monster->isDead || monster->MaxHP <= 0.0f || monster->HP <= 0.0f) continue;

		vec4 anchor = monster->worldMat.pos + vec4(0.0f, HeadOffsetY, 0.0f, 0.0f);
		float dx = anchor.x - cameraPos.x;
		float dy = anchor.y - cameraPos.y;
		float dz = anchor.z - cameraPos.z;
		float distance = sqrtf(dx * dx + dy * dy + dz * dz);
		float hpRate = min(1.0f, max(0.0f, monster->HP / monster->MaxHP));
		bool isDamaged = hpRate < 0.995f;
		bool wasRecentlyHit = monster->HitFlashTimer > 0.0f;
		if (!wasRecentlyHit && !isDamaged) continue;
		if (wasRecentlyHit) {
			if (distance > HitVisibleDistance) continue;
		}
		else if (isDamaged) {
			if (distance > DamagedVisibleDistance) continue;
		}

		XMFLOAT3 screenPos = {};
		XMStoreFloat3(&screenPos, gd.viewportArr[0].project(anchor));
		if (screenPos.z < 0.0f || screenPos.z > 1.0f) continue;
		if (screenPos.x < -80.0f || screenPos.x > screenWidth + 80.0f || screenPos.y < -60.0f || screenPos.y > screenHeight + 60.0f) continue;

		float visibleDistance = wasRecentlyHit ? HitVisibleDistance : DamagedVisibleDistance;
		float distanceRate = min(1.0f, max(0.0f, (distance - 14.0f) / max(1.0f, visibleDistance - 14.0f)));
		float plateScale = baseScale * (0.84f - distanceRate * 0.28f);
		float hitRate = monster->GetHitFlashRate();
		float alpha = (0.72f - distanceRate * 0.28f) + hitRate * 0.24f;
		alpha = min(0.92f, max(0.32f, alpha));
		StatusEffectType statusType = GetActiveStatusEffectType(i);
		bool hasStatusEffect = statusType != StatusEffectType::None;
		vec4 statusColor = GetStatusEffectHudColor(statusType);
		vec4 backColor = hasStatusEffect ? vec4(statusColor.r * 0.10f, statusColor.g * 0.10f, statusColor.b * 0.10f, 0.68f * alpha) : vec4(0.18f, 0.03f, 0.035f, 0.62f * alpha);
		vec4 fillColor = hasStatusEffect
			? vec4(statusColor.r, statusColor.g, statusColor.b, min(1.0f, alpha + 0.08f))
			: vec4(0.94f, 0.06f + hitRate * 0.12f, 0.10f + hitRate * 0.10f, alpha);

		float uiX = screenPos.x - screenWidth * 0.5f;
		float uiY = screenHeight * 0.5f - screenPos.y;
		float barWidth = 74.0f * plateScale;
		float barHeight = 4.0f * plateScale;
		float platePad = 2.0f * plateScale;
		vec4 plate = vec4(uiX - barWidth * 0.5f - platePad, uiY - barHeight * 0.5f - platePad,
			uiX + barWidth * 0.5f + platePad, uiY + barHeight * 0.5f + platePad);
		vec4 barBack = vec4(uiX - barWidth * 0.5f, uiY - barHeight * 0.5f, uiX + barWidth * 0.5f, uiY + barHeight * 0.5f);
		vec4 barFill = barBack;
		barFill.z = barFill.x + (barBack.z - barBack.x) * hpRate;

		UIDraw_TextureRect(plate + vec4(1.0f, -1.0f, 1.0f, -1.0f) * plateScale, vec4(0.0f, 0.0f, 0.0f, 0.24f * alpha), depth + ui_depth_epsilon, FillTexture);
		UIDraw_TextureRect(plate, vec4(0.015f, 0.012f, 0.012f, 0.50f * alpha), depth, FillTexture);
		if (hasStatusEffect) {
			UIDraw_TextureRect(plate + vec4(-1.0f, -1.0f, 1.0f, 1.0f) * plateScale, vec4(statusColor.r, statusColor.g, statusColor.b, 0.18f * alpha), depth + ui_depth_epsilon * 0.5f, FillTexture);
		}
		UIDraw_TextureRect(barBack, backColor, depth - ui_depth_epsilon, FillTexture);
		UIDraw_TextureRect(barFill, fillColor, depth - ui_depth_epsilon * 2, FillTexture);
		if (hasStatusEffect) {
			UIDraw_TextureRect(vec4(barBack.x, barBack.w - 1.0f * plateScale, barFill.z, barBack.w), vec4(1.0f, 1.0f, 1.0f, 0.18f * alpha), depth - ui_depth_epsilon * 3, FillTexture);
		}
	}
}

void Game::RenderFloatingDamageTexts()
{
	if (FloatingDamageTexts.empty()) return;

	const float screenWidth = (float)gd.ClientFrameWidth;
	const float screenHeight = (float)gd.ClientFrameHeight;
	const float baseScale = max(0.75f, screenHeight / 960.0f);
	const float depth = 0.017f;
	matrix cameraWorld = gd.viewportArr[0].ViewMatrix.RTInverse;
	vec4 cameraPos = cameraWorld.pos;

	for (FloatingDamageText& text : FloatingDamageTexts) {
		if (!text.Active) continue;

		float rate = min(1.0f, max(0.0f, text.Age / max(0.01f, text.Duration)));
		float rise = 0.55f * rate;
		vec4 anchor = text.WorldPosition + vec4(0.0f, rise, 0.0f, 0.0f);

		float dx = anchor.x - cameraPos.x;
		float dy = anchor.y - cameraPos.y;
		float dz = anchor.z - cameraPos.z;
		float distance = sqrtf(dx * dx + dy * dy + dz * dz);
		if (distance > 72.0f) continue;

		XMFLOAT3 screenPos = {};
		XMStoreFloat3(&screenPos, gd.viewportArr[0].project(anchor));
		if (screenPos.z < 0.0f || screenPos.z > 1.0f) continue;
		if (screenPos.x < -100.0f || screenPos.x > screenWidth + 100.0f || screenPos.y < -80.0f || screenPos.y > screenHeight + 80.0f) continue;

		float distanceRate = min(1.0f, max(0.0f, (distance - 12.0f) / 60.0f));
		float alpha = min(1.0f, rate < 0.18f ? rate / 0.18f : (1.0f - rate) / 0.82f);
		alpha = max(0.0f, alpha) * (1.0f - distanceRate * 0.35f);
		float scale = baseScale * (1.0f - distanceRate * 0.30f) * (1.0f + (1.0f - rate) * 0.12f);

		float uiX = screenPos.x - screenWidth * 0.5f + text.SideOffset * (1.0f - rate * 0.35f) * scale;
		float uiY = screenHeight * 0.5f - screenPos.y + 12.0f * rate * scale;
		float width = 82.0f * scale;
		float height = 34.0f * scale;
		vec4 textRt = vec4(uiX - width * 0.5f, uiY - height * 0.5f, uiX + width * 0.5f, uiY + height * 0.5f);
		vec4 shadowRt = textRt + vec4(1.5f, -1.5f, 1.5f, -1.5f) * scale;

		wchar_t damageText[16] = {};
		swprintf_s(damageText, L"%.0f", text.Damage);
		RenderSDFText(damageText, (int)wcslen(damageText), shadowRt, 22.0f * scale, vec4(0.0f, 0.0f, 0.0f, 0.62f * alpha), nullptr, nullptr, depth - ui_depth_epsilon);
		RenderSDFText(damageText, (int)wcslen(damageText), textRt, 22.0f * scale, vec4(1.0f, 0.88f, 0.34f, alpha), nullptr, nullptr, depth - ui_depth_epsilon * 2);
	}
}

void Game::RenderGameplaySkillHUD()
{
	if (player == nullptr || UITextureTable.size() <= 39) return;

	constexpr int SkillFrameTexture = 8;      // temp: Resources/UI/UI_Inventory_ItemSlot.png
	constexpr int SkillIconTexture = 39;      // temp: Resources/UI/UI_GamePlay_Passive.png
	constexpr int ReadyGlowTexture = 1;       // temp: Resources/UI/NeonLight.png
	constexpr int FillTexture = 0;            // temp: DefaultTex

	const float screenWidth = (float)gd.ClientFrameWidth;
	const float screenHeight = (float)gd.ClientFrameHeight;
	const float scale = max(0.75f, screenHeight / 960.0f);
	const float slotSize = 74.0f * scale;
	const float slotGap = 14.0f * scale;
	const float ultSize = 94.0f * scale;
	const float bottom = -screenHeight * 0.5f + 48.0f * scale;
	const float left = -screenWidth * 0.5f + 44.0f * scale;
	const float skillY0 = bottom;
	const float depth = 0.020f;
	const vec4 jobColor = GetUltimateHUDColor(player->m_currentJob);

	auto drawSkillSlot = [&](SkillSlot slot, const wchar_t* keyText, float x0, float size)
		{
			const int slotIndex = (int)slot;
			float cooldownRemain = max(0.0f, player->SkillCooldownFlow[slotIndex]);
			bool canUse = cooldownRemain <= 0.0f;
			vec4 rt = vec4(x0, skillY0, x0 + size, skillY0 + size);

			if (canUse) {
				vec4 glowRt = rt + vec4(-8.0f, -8.0f, 8.0f, 8.0f) * scale;
				UIDraw_TextureRect(glowRt, vec4(jobColor.r, jobColor.g, jobColor.b, 0.32f), depth + ui_depth_epsilon, ReadyGlowTexture);
			}

			UIDraw_TextureRect(rt, vec4(0.10f, 0.12f, 0.14f, 0.86f), depth, SkillFrameTexture);
			vec4 iconRt = rt + vec4(8.0f, 8.0f, -8.0f, -8.0f) * scale;
			vec4 iconColor = canUse ? vec4(1.20f, 1.20f, 1.20f, 1.00f) : vec4(0.22f, 0.22f, 0.22f, 0.88f);
			UIDraw_TextureRect(iconRt, iconColor, depth - ui_depth_epsilon, SkillIconTexture);

			vec4 keyRt = vec4(rt.x + 4.0f * scale, rt.y + 4.0f * scale, rt.x + 38.0f * scale, rt.y + 34.0f * scale);
			UIDraw_TextureRect(keyRt, vec4(0.0f, 0.0f, 0.0f, 0.86f), depth - ui_depth_epsilon * 2, FillTexture);
			RenderSDFText(keyText, (int)wcslen(keyText), keyRt, 20.0f * scale, vec4(1, 1, 1, 1), nullptr, nullptr, depth - ui_depth_epsilon * 3);

			if (!canUse) {
				UIDraw_TextureRect(iconRt, vec4(0.0f, 0.0f, 0.0f, 0.46f), depth - ui_depth_epsilon * 4, FillTexture);

				wchar_t cooldownText[16] = {};
				swprintf_s(cooldownText, L"%.0f", ceilf(cooldownRemain));
				RenderSDFText(cooldownText, (int)wcslen(cooldownText), rt, 28.0f * scale, vec4(1, 1, 1, 1), nullptr, nullptr, depth - ui_depth_epsilon * 5);
			}
		};

	float x = left;
	drawSkillSlot(SkillSlot::Skill1, L"E", x, slotSize);
	x += slotSize + slotGap;
	drawSkillSlot(SkillSlot::Skill2, L"Z", x, slotSize);
	x += slotSize + slotGap;

	float ultimateChargePercent = UltimateChargePercent;
	bool canUseUltimate = ultimateChargePercent >= 100.0f && player->SkillCooldownFlow[(int)SkillSlot::Ultimate] <= 0.0f;
	vec4 ultRt = vec4(x, bottom - (ultSize - slotSize) * 0.5f, x + ultSize, bottom - (ultSize - slotSize) * 0.5f + ultSize);

	if (canUseUltimate) {
		vec4 glowRt = ultRt + vec4(-12.0f, -12.0f, 12.0f, 12.0f) * scale;
		UIDraw_TextureRect(glowRt, vec4(jobColor.r, jobColor.g, jobColor.b, 0.58f), depth + ui_depth_epsilon, ReadyGlowTexture);
	}

	UIDraw_TextureRect(ultRt, vec4(0.12f, 0.10f, 0.06f, 0.90f), depth, SkillFrameTexture);
	vec4 ultIconRt = ultRt + vec4(10.0f, 10.0f, -10.0f, -10.0f) * scale;
	vec4 ultIconColor = canUseUltimate ? vec4(jobColor.r * 1.25f, jobColor.g * 1.25f, jobColor.b * 1.25f, 1.00f) : vec4(0.24f, 0.24f, 0.24f, 0.90f);
	UIDraw_TextureRect(ultIconRt, ultIconColor, depth - ui_depth_epsilon, SkillIconTexture);

	float fillRate = ultimateChargePercent / 100.0f;
	vec4 fillRt = vec4(ultRt.x + 8.0f * scale, ultRt.y + 8.0f * scale, ultRt.x + 8.0f * scale + (ultSize - 16.0f * scale) * fillRate, ultRt.y + 14.0f * scale);
	UIDraw_TextureRect(fillRt, vec4(jobColor.r, jobColor.g, jobColor.b, canUseUltimate ? 1.0f : 0.95f), depth - ui_depth_epsilon * 2, FillTexture);

	wchar_t percentText[16] = {};
	swprintf_s(percentText, L"%d%%", (int)(ultimateChargePercent + 0.5f));
	RenderSDFText(percentText, (int)wcslen(percentText), ultRt, 22.0f * scale, canUseUltimate ? vec4(jobColor.r, jobColor.g, jobColor.b, 1.0f) : vec4(1, 1, 1, 1), nullptr, nullptr, depth - ui_depth_epsilon * 4);

	vec4 keyRt = vec4(ultRt.x + 6.0f * scale, ultRt.y + 6.0f * scale, ultRt.x + 44.0f * scale, ultRt.y + 39.0f * scale);
	UIDraw_TextureRect(keyRt, vec4(0.0f, 0.0f, 0.0f, 0.88f), depth - ui_depth_epsilon * 5, FillTexture);
	RenderSDFText(L"Q", 1, keyRt, 22.0f * scale, vec4(1, 1, 1, 1), nullptr, nullptr, depth - ui_depth_epsilon * 6);
}

#pragma region UICode

#pragma region UIDesign_GlobalUIs
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

void UIEventGameEscapeBtn(DXUI* ui) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	vec4 evtloc = ui->location + game.CurrentUICenter;
	if (game.evt.uMsg == WM_LBUTTONDOWN && game.RectContainPos(evtloc, game.CurrentCursorPos)) {
		pbtn->flow = 0;

		PostQuitMessage(0);
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
	vec4 renderLoc = ui->location + game.CurrentUICenter;
	game.UIDraw_TextureRect(renderLoc, vec4(1, 1, 1, 1), ui->depth, pWindow->WindowImageIndex);

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
		pbtn->Set(0, 1, 0, L"\xC800\xC7A5\xD558\xAE30");
		SaveBtn->SetFunctions(UIRenderDefaultBtn, UIUpdateDefaultBtn, UIEventSaveBtn);
		sample_page->uiArr.push_back(SaveBtn);
	}
	pWindow->page_stack.push_back(sample_page);
}
#pragma endregion

#pragma region UIDesign_Inventory
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

		if (pslot->itemCount > 0) {
			Item& item = ItemTable[pslot->objid];
			game.UIDraw_TextureRect(renderLoc, color, depth - 0.01f, item.icon);
		}

		_itow_s(pslot->itemCount, NumText, 64, 10);
		game.RenderSDFText(NumText, wcslen(NumText), renderLoc, 15, vec4(1, 1, 1, 1), nullptr, nullptr, -0.02f + depth);
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
		else if (game.RectContainPos(evtloc, game.CurrentCursorPos)) {
			// grab logic
			if (game.evt.uMsg == WM_LBUTTONDOWN && game.CurrentGrabSlotData.itemCnt == 0 && pslot->itemCount > 0) {
				pslot->flow = 0;
				game.CurrentGrabSlotData.objid = pslot->objid;
				game.CurrentGrabSlotData.itemCnt = pslot->itemCount;
				game.CurrentGrabSlotData.selectedSlot = ui;
			}
			// divide logic
			else if (game.evt.uMsg == WM_RBUTTONDOWN && game.CurrentGrabSlotData.itemCnt == 0 && pslot->itemCount > 0) {
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
#pragma endregion

#pragma region UIDesign_Equip
void UIRender_EquipItemSlot(DXUI* ui) {
	DXSlotParam* pslot = (DXSlotParam*)ui->pParamterData;
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

	if (pslot->itemCount > 0 && pslot->objid > 0) {
		Item& item = ItemTable[pslot->objid];
		game.UIDraw_TextureRect(renderLoc, vec4(1, 1, 1, 1), depth - 0.01f, item.icon);
	}
}

void UIUpdate_EquipItemSlot(DXUI* ui, float deltaTime) {
	DXSlotParam* pslot = (DXSlotParam*)ui->pParamterData;
	pslot->flow += deltaTime;
	if (pslot->flow > pslot->maxtime) {
		pslot->flow = pslot->maxtime;
	}
}

void UIEvent_EquipItemSlot(DXUI* ui) {
	DXSlotParam* pslot = (DXSlotParam*)ui->pParamterData;

	vec4 evtloc = ui->location + game.CurrentUICenter;
	Item& grabItem = ItemTable[game.CurrentGrabSlotData.objid];
	if (pslot->slotObType == SlotKind::_Item_Weapone) {
		if (game.CurrentGrabSlotData.itemCnt != 0 && grabItem.itemtype == ItemType::_Weapon) {
			if (game.RectContainPos(evtloc, game.CurrentCursorPos)) {
				if (game.evt.uMsg == WM_LBUTTONDOWN)
				{
					pslot->objid = game.CurrentGrabSlotData.objid;
					pslot->itemCount = 1;

					game.CurrentGrabSlotData.itemCnt = 0;

					DXUI* Inv_slotUI = game.CurrentGrabSlotData.selectedSlot;
					DXSlotParam* invslot = (DXSlotParam*)Inv_slotUI->pParamterData;

					// ����
					CTS_ChangeEquipSlotWithInventorySlot_Header header;
					header.size = sizeof(CTS_ChangeEquipSlotWithInventorySlot_Header);
					header.st = CTS_Protocol::CTS_ChangeEquipSlotWithInventorySlot;
					header.EquipIndex = pslot->addtionalParams_int[0];
					header.InventoryIndex = invslot->addtionalParams_int[7];
					client.send((char*)&header, sizeof(CTS_ChangeEquipSlotWithInventorySlot_Header), 0);

					//Ŭ���̾�Ʈ ����
					game.player->weapon[pslot->addtionalParams_int[0] - 4] = *(Weapon*)(ItemTable[invslot->objid].pItemData);
				}
			}
		}
	}
	else if(pslot->slotObType == SlotKind::_Item_Equipment) {
		if (game.CurrentGrabSlotData.itemCnt != 0 && grabItem.itemtype == ItemType::_Equipment) {
			if (game.RectContainPos(evtloc, game.CurrentCursorPos)) {
				if (game.evt.uMsg == WM_LBUTTONDOWN)
				{
					pslot->objid = game.CurrentGrabSlotData.objid;
					pslot->itemCount = 1;

					game.CurrentGrabSlotData.itemCnt = 0;

					DXUI* Inv_slotUI = game.CurrentGrabSlotData.selectedSlot;
					DXSlotParam* invslot = (DXSlotParam*)Inv_slotUI->pParamterData;

					// ����
					CTS_ChangeEquipSlotWithInventorySlot_Header header;
					header.size = sizeof(CTS_ChangeEquipSlotWithInventorySlot_Header);
					header.st = CTS_Protocol::CTS_ChangeEquipSlotWithInventorySlot;
					header.EquipIndex = pslot->addtionalParams_int[0];
					header.InventoryIndex = invslot->addtionalParams_int[7];
					client.send((char*)&header, sizeof(CTS_ChangeEquipSlotWithInventorySlot_Header), 0);
				}
			}
		}
	}
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

		//2. Equip Slots
		//2-1. Head Equip
		rateloc = sample_page->location;
		rateloc.x += 150;
		rateloc.w -= 70;
		rateloc.z = rateloc.x + 80;
		rateloc.y = rateloc.w - 80;
		DXUI* EquipSlot_Head = new DXUI(DXUI_TYPE::DXUI_Slot, sizeof(DXSlotParam), rateloc, 0, new DXSlotParam());
		DXSlotParam* pslot_Head = (DXSlotParam*)EquipSlot_Head->pParamterData;
		pslot_Head->Set(0, 1, SlotKind::_Item, 10);
		pslot_Head->addtionalParams_int[0] = 0;
		EquipSlot_Head->SetFunctions(UIRender_EquipItemSlot, UIUpdate_EquipItemSlot, UIEvent_EquipItemSlot);
		sample_page->uiArr.push_back(EquipSlot_Head);

		//2-2. Chest Equip
		rateloc.w -= 80;
		rateloc.y -= 80;
		DXUI* EquipSlot_Chest = new DXUI(DXUI_TYPE::DXUI_Slot, sizeof(DXSlotParam), rateloc, 0, new DXSlotParam());
		DXSlotParam* pslot_Chest = (DXSlotParam*)EquipSlot_Chest->pParamterData;
		pslot_Chest->Set(0, 1, SlotKind::_Item, 10);
		pslot_Chest->addtionalParams_int[0] = 1;
		EquipSlot_Chest->SetFunctions(UIRender_EquipItemSlot, UIUpdate_EquipItemSlot, UIEvent_EquipItemSlot);
		sample_page->uiArr.push_back(EquipSlot_Chest);

		//2-3. Leg Equip
		rateloc.w -= 80;
		rateloc.y -= 80;
		DXUI* EquipSlot_Leg = new DXUI(DXUI_TYPE::DXUI_Slot, sizeof(DXSlotParam), rateloc, 0, new DXSlotParam());
		DXSlotParam* pslot_Leg = (DXSlotParam*)EquipSlot_Leg->pParamterData;
		pslot_Leg->Set(0, 1, SlotKind::_Item, 10);
		pslot_Leg->addtionalParams_int[0] = 2;
		EquipSlot_Leg->SetFunctions(UIRender_EquipItemSlot, UIUpdate_EquipItemSlot, UIEvent_EquipItemSlot);
		sample_page->uiArr.push_back(EquipSlot_Leg);

		//2-4. Shoes Equip
		rateloc.w -= 80;
		rateloc.y -= 80;
		DXUI* EquipSlot_Shoe = new DXUI(DXUI_TYPE::DXUI_Slot, sizeof(DXSlotParam), rateloc, 0, new DXSlotParam());
		DXSlotParam* pslot_Shoe = (DXSlotParam*)EquipSlot_Shoe->pParamterData;
		pslot_Shoe->Set(0, 1, SlotKind::_Item, 10);
		pslot_Shoe->addtionalParams_int[0] = 3;
		EquipSlot_Shoe->SetFunctions(UIRender_EquipItemSlot, UIUpdate_EquipItemSlot, UIEvent_EquipItemSlot);
		sample_page->uiArr.push_back(EquipSlot_Shoe);

		//2-5. Wepone0 Equip
		rateloc = sample_page->location;
		rateloc.x += 237;
		rateloc.w -= 110;
		rateloc.z = rateloc.x + 80;
		rateloc.y = rateloc.w - 80;
		DXUI* EquipSlot_Weapone0 = new DXUI(DXUI_TYPE::DXUI_Slot, sizeof(DXSlotParam), rateloc, 0, new DXSlotParam());
		DXSlotParam* pslot_Weapone0 = (DXSlotParam*)EquipSlot_Weapone0->pParamterData;
		pslot_Weapone0->Set(0, 1, SlotKind::_Item_Weapone, 10);
		pslot_Weapone0->addtionalParams_int[0] = 4;
		EquipSlot_Weapone0->SetFunctions(UIRender_EquipItemSlot, UIUpdate_EquipItemSlot, UIEvent_EquipItemSlot);
		sample_page->uiArr.push_back(EquipSlot_Weapone0);

		//2-5. Wepone1 Equip
		rateloc.w -= 80;
		rateloc.y -= 80;
		DXUI* EquipSlot_Weapone1 = new DXUI(DXUI_TYPE::DXUI_Slot, sizeof(DXSlotParam), rateloc, 0, new DXSlotParam());
		DXSlotParam* pslot_Weapone1 = (DXSlotParam*)EquipSlot_Weapone1->pParamterData;
		pslot_Weapone1->Set(0, 1, SlotKind::_Item_Weapone, 10);
		pslot_Weapone1->addtionalParams_int[0] = 5;
		EquipSlot_Weapone1->SetFunctions(UIRender_EquipItemSlot, UIUpdate_EquipItemSlot, UIEvent_EquipItemSlot);
		sample_page->uiArr.push_back(EquipSlot_Weapone1);

		//2-5. Wepone2 Equip
		rateloc.w -= 80;
		rateloc.y -= 80;
		DXUI* EquipSlot_Weapone2 = new DXUI(DXUI_TYPE::DXUI_Slot, sizeof(DXSlotParam), rateloc, 0, new DXSlotParam());
		DXSlotParam* pslot_Weapone2 = (DXSlotParam*)EquipSlot_Weapone2->pParamterData;
		pslot_Weapone2->Set(0, 1, SlotKind::_Item_Weapone, 10);
		pslot_Weapone2->addtionalParams_int[0] = 6;
		EquipSlot_Weapone2->SetFunctions(UIRender_EquipItemSlot, UIUpdate_EquipItemSlot, UIEvent_EquipItemSlot);
		sample_page->uiArr.push_back(EquipSlot_Weapone2);

		// 2-6. Wepone0 Desc Dumy Btn
		rateloc = sample_page->location;
		rateloc.x += 320;
		rateloc.w -= 110;
		rateloc.z = rateloc.x + 225;
		rateloc.y = rateloc.w - 75;
		DXUI* DescDumyBtn_Weapone0 = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn_desc_Weapone0 = (DXBtnParam*)DescDumyBtn_Weapone0->pParamterData;
		pbtn_desc_Weapone0->Set(0, 1, 11, L"");
		DescDumyBtn_Weapone0->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, nullptr);
		sample_page->uiArr.push_back(DescDumyBtn_Weapone0);

		// 2-7. Wepone1 Desc Dumy Btn
		rateloc.w -= 80;
		rateloc.y -= 80;
		DXUI* DescDumyBtn_Weapone1 = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn_desc_Weapone1 = (DXBtnParam*)DescDumyBtn_Weapone1->pParamterData;
		pbtn_desc_Weapone1->Set(0, 1, 11, L"");
		DescDumyBtn_Weapone1->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, nullptr);
		sample_page->uiArr.push_back(DescDumyBtn_Weapone1);

		// 2-8. Wepone2 Desc Dumy Btn
		rateloc.w -= 80;
		rateloc.y -= 80;
		DXUI* DescDumyBtn_Weapone2 = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn_desc_Weapone2 = (DXBtnParam*)DescDumyBtn_Weapone2->pParamterData;
		pbtn_desc_Weapone2->Set(0, 1, 11, L"");
		DescDumyBtn_Weapone2->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, nullptr);
		sample_page->uiArr.push_back(DescDumyBtn_Weapone2);
	}
	pWindow->page_stack.push_back(sample_page);
}
#pragma endregion

#pragma region UIDesign_Shop
void UIEvent_ShopTabBtn(DXUI* ui) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	vec4 evtloc = ui->location + game.CurrentUICenter;
	if (game.evt.uMsg == WM_LBUTTONDOWN && game.RectContainPos(evtloc, game.CurrentCursorPos)) {
		pbtn->flow = 0;

		//change TabType
	}
}

void UIRender_ShopItemObject(DXUI* ui) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	vec4& slotShowRt = *(vec4*)pbtn->addtionalPtr[2];
	if (game.RectContainRect(slotShowRt, ui->location)) {
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
}

void UIUpdate_ShopItemObject(DXUI* ui, float deltaTime) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	pbtn->flow += deltaTime;
	if (pbtn->flow > pbtn->maxtime) {
		pbtn->flow = pbtn->maxtime;
	}
	constexpr float slot_Margin = 80;
	//�����̴��� �Ķ���Ϳ� ���� Y ���� �����Ѵ�.
	float SliderY = *(float*)pbtn->addtionalPtr[1];
	float StartY = pbtn->addtionalParams[1];
	float MAXY = StartY + 7 * slot_Margin;
	float present_w = SliderY * (MAXY - StartY) + StartY;
	float height = ui->location.w - ui->location.y;
	ui->location.w = present_w;
	ui->location.y = ui->location.w - height;
}

void UIEvent_ShopItemObject(DXUI* ui) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	vec4& slotShowRt = *(vec4*)pbtn->addtionalPtr[2];
	if (game.RectContainRect(slotShowRt, ui->location))
	{
		vec4 evtloc = ui->location + game.CurrentUICenter;
		if (game.evt.uMsg == WM_LBUTTONDOWN && game.RectContainPos(evtloc, game.CurrentCursorPos)) {
			pbtn->flow = 0;

			//open Buy/Sell Window
			DXWindowParam* pWindow = (DXWindowParam*)pbtn->addtionalPtr[3];
			pWindow->page_stack.push_back((DXPage*)pbtn->addtionalPtr[4]);
		}
	}
}

void UIEvent_SellBuyCloseBtn(DXUI* ui) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	vec4 evtloc = ui->location + game.CurrentUICenter;
	if (game.evt.uMsg == WM_LBUTTONDOWN && game.RectContainPos(evtloc, game.CurrentCursorPos)) {
		pbtn->flow = 0;

		//close currentPage
		DXWindowParam* pWin = (DXWindowParam*)pbtn->addtionalPtr[0];
		pWin->page_stack.pop_back();
	}
}

void UIEvent_SellBuyConfimBtn(DXUI* ui) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	vec4 evtloc = ui->location + game.CurrentUICenter;
	if (game.evt.uMsg == WM_LBUTTONDOWN && game.RectContainPos(evtloc, game.CurrentCursorPos)) {
		pbtn->flow = 0;

		//close currentPage
		DXWindowParam* pWin = (DXWindowParam*)pbtn->addtionalPtr[0];
		pWin->page_stack.pop_back();
	}
}

void UIRenderBuySellWindow(DXUI* ui) {
	DXWindowParam* pWindow = (DXWindowParam*)ui->pParamterData;
	vec4 renderLoc = ui->location + game.CurrentUICenter;
	game.UIDraw_TextureRect(renderLoc, vec4(1, 1, 1, 1), ui->depth, pWindow->WindowImageIndex);

	if (pWindow->page_stack.size() > 0) {
		//PageRender
		vector<DXPage*>* savePageStack = game.CurrentPageStack;
		game.CurrentPageStack = &pWindow->page_stack;
		vec4 SaveCenter = game.CurrentUICenter;
		game.CurrentUICenter = vec4(ui->location.x + ui->location.z, ui->location.y + ui->location.w, ui->location.x + ui->location.z, ui->location.y + ui->location.w);
		game.CurrentUICenter *= 0.5f;
		game.CurrentUICenter += SaveCenter;

		vec4 renderLoc = ui->location + game.CurrentUICenter;
		game.UIDraw_TextureRect(renderLoc, vec4(1, 1, 1, 1), ui->depth, pWindow->WindowImageIndex);

		float lastDepth = 0;
		for (int i = 0; i < pWindow->page_stack.size(); ++i) {
			DXPage* page = pWindow->page_stack[i];
			lastDepth = page->GetDepth(0);
			page->Render();
		}

		// �� ��ġ�� ������ ��
		wchar_t NumText[64] = L"-5000G";
		renderLoc.x += 8;
		renderLoc.w -= 203;
		renderLoc.z = renderLoc.x + 198;
		renderLoc.y = renderLoc.w - 97;
		game.RenderSDFText(NumText, wcslen(NumText), renderLoc, 20, vec4(1, 1, 0, 1), nullptr, nullptr, pWindow->page_stack[0]->GetDepth(0) - game.uipage_depth_epsilon, nullptr);

		game.CurrentUICenter = SaveCenter;
		game.CurrentPageStack = savePageStack;
	}
}

void UIUpdateBuySellWindow(DXUI* ui, float deltaTime) {
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

void UIEventBuySellWindow(DXUI* ui) {
	DXWindowParam* pWindow = (DXWindowParam*)ui->pParamterData;
	if (pWindow->page_stack.size() > 0) {
		vector<DXPage*>* savePageStack = game.CurrentPageStack;
		game.CurrentPageStack = &pWindow->page_stack;
		vec4 SaveCenter = game.CurrentUICenter;
		game.CurrentUICenter = vec4(ui->location.x + ui->location.z, ui->location.y + ui->location.w, ui->location.x + ui->location.z, ui->location.y + ui->location.w);
		game.CurrentUICenter *= 0.5f;
		game.CurrentUICenter += SaveCenter;
		// ������ ���������� ������� UI�� Event �� ó���Ѵ�.
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

void UIInitBuySellWindow(DXUI* ui) {
	DXWindowParam* pWindow = (DXWindowParam*)ui->pParamterData;
	DXPage* sample_page = new DXPage();
	sample_page->BackGroundColor = vec4(0, 0, 0, 0);
	float WindowW = ui->location.z - ui->location.x;
	float WindowH = ui->location.w - ui->location.y;
	sample_page->location = vec4(-((float)WindowW * 0.5f), -((float)WindowH * 0.5f), (float)WindowW * 0.5f, (float)WindowH * 0.5f);
	pWindow->page_table.push_back(sample_page);
	{
		//���� / �Ǹ�â

		//1. Close Btn (371, 11, W27, H27)
		vec4 rateloc = sample_page->location;
		rateloc.x += 371;
		rateloc.w -= 11;
		rateloc.z = rateloc.x + 27;
		rateloc.y = rateloc.w - 27;
		DXUI* CloseBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn = (DXBtnParam*)CloseBtn->pParamterData;
		pbtn->Set(0, 1, 3, L"");
		pbtn->addtionalPtr[0] = pWindow->addtionalPtr[0];
		CloseBtn->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, UIEvent_SellBuyCloseBtn);
		sample_page->uiArr.push_back(CloseBtn);

		//2. Dumy Item Slot (12, 77, 100, 100)
		rateloc = sample_page->location;
		rateloc.x += 12;
		rateloc.w -= 77;
		rateloc.z = rateloc.x + 100;
		rateloc.y = rateloc.w - 100;
		DXUI* DescDumyBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn_desc = (DXBtnParam*)DescDumyBtn->pParamterData;
		pbtn_desc->Set(0, 1, 16, L"");
		DescDumyBtn->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, nullptr);
		sample_page->uiArr.push_back(DescDumyBtn);

		// Slider (125, 160, 165, 20)
		rateloc = sample_page->location;
		rateloc.x += 125;
		rateloc.w -= 160;
		rateloc.z = rateloc.x + 165;
		rateloc.y = rateloc.w - 20;
		DXUI* CostSelectSlider = new DXUI(DXUI_TYPE::DXUI_Slider, sizeof(DXSliderParam), rateloc, 0, new DXSliderParam());
		CostSelectSlider->SetFunctions(UIRenderCyberSlider, UIUpdateDefaultSlider, UIEventDefaultSlider);
		DXSliderParam* pCostSelectSlider = (DXSliderParam*)CostSelectSlider->pParamterData;
		pCostSelectSlider->Set(true, 'f', false, 0, 1, '\0', &pWindow->addtionalParams[0], 18, vec4(-8, -30, 8, 30));
		sample_page->uiArr.push_back(CostSelectSlider);

		// Edit Box 17 : (302, 135, 92, 60)
		rateloc = sample_page->location;
		rateloc.x += 302;
		rateloc.w -= 135;
		rateloc.z = rateloc.x + 92;
		rateloc.y = rateloc.w - 60;
		DXUI* ItemCountEditBox = new DXUI(DXUI_TYPE::DXUI_Edit, sizeof(DXEditParam), rateloc, 0, new DXEditParam());
		DXEditParam* pEdit;
		pEdit = (DXEditParam*)ItemCountEditBox->pParamterData;
		pEdit->Set(0, 1, 1, 17, L"ItemNum"); // ������ �޴´�.
		ItemCountEditBox->SetFunctions(UIRenderDefaultEdit, UIUpdateDefaultEdit, UIEventDefaultEdit);
		sample_page->uiArr.push_back(ItemCountEditBox);

		// Confim Btn 20 : (214, 214, 82, 46)
		rateloc = sample_page->location;
		rateloc.x += 214;
		rateloc.w -= 214;
		rateloc.z = rateloc.x + 82;
		rateloc.y = rateloc.w - 46;
		DXUI* ConfimBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn_confim = (DXBtnParam*)ConfimBtn->pParamterData;
		pbtn_confim->Set(0, 1, 20, L"Ȯ��");
		ConfimBtn->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, UIEvent_SellBuyConfimBtn);
		pbtn_confim->addtionalPtr[0] = pWindow->addtionalPtr[0];
		sample_page->uiArr.push_back(ConfimBtn);

		// Cancel Btn 20 : (306, 214, 82, 46)
		rateloc = sample_page->location;
		rateloc.x += 306;
		rateloc.w -= 214;
		rateloc.z = rateloc.x + 82;
		rateloc.y = rateloc.w - 46;
		DXUI* CancelBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn_cancel = (DXBtnParam*)CancelBtn->pParamterData;
		pbtn_cancel->Set(0, 1, 20, L"Cancel");
		pbtn_cancel->addtionalPtr[0] = pWindow->addtionalPtr[0];
		CancelBtn->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, UIEvent_SellBuyCloseBtn);
		sample_page->uiArr.push_back(CancelBtn);
	}
	pWindow->page_stack.push_back(sample_page);
}

void UIInitShopWindow(DXUI* ui) {
	DXWindowParam* pWindow = (DXWindowParam*)ui->pParamterData;
	DXPage* sample_page = new DXPage();
	sample_page->BackGroundColor = vec4(0, 0, 0, 0);
	float WindowW = ui->location.z - ui->location.x;
	float WindowH = ui->location.w - ui->location.y;
	sample_page->location = vec4(-((float)WindowW * 0.5f), -((float)WindowH * 0.5f), (float)WindowW * 0.5f, (float)WindowH * 0.5f);
	pWindow->page_table.push_back(sample_page);

	DXPage* BuySell_page = new DXPage();
	BuySell_page->BackGroundColor = vec4(0, 0, 0, 0.5f);
	BuySell_page->location = vec4(-((float)WindowW * 0.5f), -((float)WindowH * 0.5f), (float)WindowW * 0.5f, (float)WindowH * 0.5f);
	pWindow->page_table.push_back(BuySell_page);
	{
		//����/�Ǹ� ������ 12 : (405, 275)
		vec4 rateloc = sample_page->location;
		rateloc.x += 0;
		rateloc.w -= 0;
		rateloc.z = rateloc.x + 405;
		rateloc.y = rateloc.w - 405;
		DXUI* BuySell_Window = new DXUI(DXUI_TYPE::DXUI_Window, sizeof(DXWindowParam), rateloc, 0, new DXWindowParam());
		BuySell_Window->SetFunctions(UIRenderCyberWindow, UIUpdateDefaultWindow, UIEventDefaultWindow);
		DXWindowParam* pBuySellWindow = (DXWindowParam*)BuySell_Window->pParamterData;
		pBuySellWindow->Set(BuySell_Window, 42);
		pBuySellWindow->LastFocusedTime = chrono::system_clock::now();
		pBuySellWindow->addtionalPtr[0] = pWindow; // �θ� ������
		UIInitBuySellWindow(BuySell_Window);
		BuySell_Window->enable = true;
		BuySell_page->uiArr.push_back(BuySell_Window);
	}

	{
		// ���â UI

		//1. Close Btn (568, 11, W27, H27)
		vec4 rateloc = sample_page->location;
		rateloc.x += 568;
		rateloc.w -= 11;
		rateloc.z = rateloc.x + 27;
		rateloc.y = rateloc.w - 27;
		DXUI* CloseBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn = (DXBtnParam*)CloseBtn->pParamterData;
		pbtn->Set(0, 1, 3, L"");
		pbtn->addtionalPtr[0] = ui;
		CloseBtn->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, UIEvent_InventoryCloseBtn);
		sample_page->uiArr.push_back(CloseBtn);

		//2. Shop Buy Tab Btn 13 : (10, 50, 123, 35)
		rateloc = sample_page->location;
		rateloc.x += 10;
		rateloc.w -= 50;
		rateloc.z = rateloc.x + 123;
		rateloc.y = rateloc.w - 35;
		DXUI* ShopBuyTabBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn_ShopBuyTabBtn = (DXBtnParam*)ShopBuyTabBtn->pParamterData;
		pbtn_ShopBuyTabBtn->Set(0, 1, 13, L"����");
		pbtn_ShopBuyTabBtn->addtionalPtr[0] = ui;
		ShopBuyTabBtn->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, UIEvent_ShopTabBtn);
		sample_page->uiArr.push_back(ShopBuyTabBtn);

		rateloc.x += 127;
		rateloc.z += 127;
		DXUI* ShopSellTabBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn_ShopSellTabBtn = (DXBtnParam*)ShopSellTabBtn->pParamterData;
		pbtn_ShopSellTabBtn->Set(0, 1, 13, L"�Ǹ�");
		pbtn_ShopSellTabBtn->addtionalPtr[0] = ui;
		ShopSellTabBtn->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, UIEvent_ShopTabBtn);
		sample_page->uiArr.push_back(ShopSellTabBtn);

		//3. Show Range Select Slider (570, 88, W32, H614)
		rateloc = sample_page->location;
		rateloc.x += 570;
		rateloc.w -= 88;
		rateloc.z = rateloc.x + 32;
		rateloc.y = rateloc.w - 614;
		DXUI* RangeSelectSlider = new DXUI(DXUI_TYPE::DXUI_Slider, sizeof(DXSliderParam), rateloc, 0, new DXSliderParam());
		RangeSelectSlider->SetFunctions(UIRenderCyberSlider, UIUpdateDefaultSlider, UIEventDefaultSlider);
		DXSliderParam* pRangeSelectSlider = (DXSliderParam*)RangeSelectSlider->pParamterData;
		pRangeSelectSlider->Set(false, 'f', true, 0, 1, '\0', &pWindow->addtionalParams[0], 7, vec4(-15, -15, 25, 15));
		sample_page->uiArr.push_back(RangeSelectSlider);

		// ȭ�鿡 ��Ÿ�� �� �ִ� �ִ��� ��ǰ ����
		constexpr int Max_Item = 50;
		// 14 : (10, 90, 556, 75)
		rateloc = sample_page->location;
		rateloc.x += 10;
		rateloc.w -= 90;
		rateloc.z = rateloc.x + 556;
		rateloc.y = rateloc.w - 80;
		*(vec4*)(pWindow->addtionalPtr[1]) = rateloc;
		vec4& ItemShowRange = *(vec4*)(pWindow->addtionalPtr[1]);
		ItemShowRange.w += 20;
		ItemShowRange.y -= 80 * 7;
		for (int i = 0; i < Max_Item; ++i) {
			DXUI* ShopItemObject = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
			DXBtnParam* pbtn_ShopItemObject = (DXBtnParam*)ShopItemObject->pParamterData;
			pbtn_ShopItemObject->Set(0, 1, 14, L"");
			pbtn_ShopItemObject->addtionalParams_int[0] = i; // ���� �������� �ε����� ��� �ִ�.
			pbtn_ShopItemObject->addtionalParams[1] = rateloc.w; // ������ ������Ʈ�� ���� y ��
			pbtn_ShopItemObject->addtionalPtr[1] = &pWindow->addtionalParams[0]; // �����̴� �� ������
			pbtn_ShopItemObject->addtionalPtr[2] = &ItemShowRange; // ������ ������Ʈ�� ������ ����
			pbtn_ShopItemObject->addtionalPtr[3] = pWindow;
				// ���� ������ ������ �������� �߰��� ���� ������
			pbtn_ShopItemObject->addtionalPtr[4] = BuySell_page;
				// �߰��� �������� ��ȣ
			ShopItemObject->SetFunctions(UIRender_ShopItemObject, UIUpdate_ShopItemObject, UIEvent_ShopItemObject);
			sample_page->uiArr.push_back(ShopItemObject);
			rateloc.y -= 80;
			rateloc.w -= 80;
		}
	}

	pWindow->page_stack.push_back(sample_page);
}
#pragma endregion

#pragma region UIDesign_Quest

void UIRender_QuestItemObject(DXUI* ui) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	vec4& slotShowRt = *(vec4*)pbtn->addtionalPtr[2];
	DXWindowParam* thisWindow = (DXWindowParam*)pbtn->addtionalPtr[3];
	DXPage* descPage = (DXPage*)pbtn->addtionalPtr[4];
	bool bRenderd = true;
	for (int i = 0; i < thisWindow->page_stack.size(); ++i) {
		if (descPage == thisWindow->page_stack[i]) bRenderd = false;
	}
	if (bRenderd && game.RectContainRect(slotShowRt, ui->location)) {
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
}

void UIUpdate_QuestItemObject(DXUI* ui, float deltaTime) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;

	static int TourID = 0;
	if (TourID != game.UITourID) {
		if (0 <= pbtn->addtionalParams_int[0] && pbtn->addtionalParams_int[0] < game.QuestArr.size()) {
			int questindex = game.QuestArr[pbtn->addtionalParams_int[0]];
			Quest* q = game.QuestTable[questindex];
			wcscpy_s(pbtn->text, 256, q->QuestName);
		}
	}

	pbtn->flow += deltaTime;
	if (pbtn->flow > pbtn->maxtime) {
		pbtn->flow = pbtn->maxtime;
	}
	constexpr float slot_Margin = 100;
	//�����̴��� �Ķ���Ϳ� ���� Y ���� �����Ѵ�.
	float SliderY = *(float*)pbtn->addtionalPtr[1];
	float StartY = pbtn->addtionalParams[1];
	float MAXY = StartY + 7 * slot_Margin;
	float present_w = SliderY * (MAXY - StartY) + StartY;
	float height = ui->location.w - ui->location.y;
	ui->location.w = present_w;
	ui->location.y = ui->location.w - height;
}

void UIUpdate_QuestDescObject(DXUI* ui, float deltaTime) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;

	static int TourID = 0;
	if (TourID != game.UITourID) {
		if (0 <= pbtn->addtionalParams_int[0] && pbtn->addtionalParams_int[0] < game.QuestArr.size()) {
			int questindex = game.QuestArr[pbtn->addtionalParams_int[0]];
			Quest* q = game.QuestTable[questindex];
			wcscpy_s(pbtn->text, 256, q->QuestDesc);
		}
	}
}

void UIEvent_QuestItemObject(DXUI* ui) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	vec4& slotShowRt = *(vec4*)pbtn->addtionalPtr[2];
	if (game.RectContainRect(slotShowRt, ui->location))
	{
		vec4 evtloc = ui->location + game.CurrentUICenter;
		if (game.evt.uMsg == WM_LBUTTONDOWN && game.RectContainPos(evtloc, game.CurrentCursorPos)) {
			pbtn->flow = 0;

			// push quest desc page
			DXWindowParam* pWin = (DXWindowParam*)pbtn->addtionalPtr[3];
			pWin->page_stack.push_back((DXPage*)pbtn->addtionalPtr[4]);
			game.presentShowQuestOffset = pbtn->addtionalParams_int[0];
		}
	}
}

void UIEvent_QuestDescCloseBtn(DXUI* ui) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	vec4 evtloc = ui->location + game.CurrentUICenter;
	if (game.evt.uMsg == WM_LBUTTONDOWN && game.RectContainPos(evtloc, game.CurrentCursorPos)) {
		pbtn->flow = 0;

		//close currentPage
		DXWindowParam* pWin = (DXWindowParam*)pbtn->addtionalPtr[0];
		pWin->page_stack.pop_back();
	}
}

void UIInitQuestWindow(DXUI* ui) {
	DXWindowParam* pWindow = (DXWindowParam*)ui->pParamterData;

	// ����Ʈ �ڼ��� ���� â
	DXPage* DescPage = new DXPage();
	DescPage->BackGroundColor = vec4(0, 0, 0, 0.5f);
	float WindowW = ui->location.z - ui->location.x;
	float WindowH = ui->location.w - ui->location.y;
	DescPage->location = vec4(-((float)WindowW * 0.5f), -((float)WindowH * 0.5f), (float)WindowW * 0.5f, (float)WindowH * 0.5f);
	pWindow->page_table.push_back(DescPage);
	{
		//1. Close Btn (568, 61, W27, H27)
		vec4 rateloc = DescPage->location;
		rateloc.x += 568;
		rateloc.w -= 61;
		rateloc.z = rateloc.x + 27;
		rateloc.y = rateloc.w - 27;
		DXUI* CloseBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn = (DXBtnParam*)CloseBtn->pParamterData;
		pbtn->Set(0, 1, 3, L"");
		pbtn->addtionalPtr[0] = pWindow;
		CloseBtn->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, UIEvent_QuestDescCloseBtn);
		DescPage->uiArr.push_back(CloseBtn);

		//3. Show Range Select Slider (570, 88, W32, H614)
		rateloc = DescPage->location;
		rateloc.x += 570;
		rateloc.w -= 100;
		rateloc.z = rateloc.x + 32;
		rateloc.y = rateloc.w - 623;
		DXUI* RangeSelectSlider = new DXUI(DXUI_TYPE::DXUI_Slider, sizeof(DXSliderParam), rateloc, 0, new DXSliderParam());
		RangeSelectSlider->SetFunctions(UIRenderCyberSlider, UIUpdateDefaultSlider, UIEventDefaultSlider);
		DXSliderParam* pRangeSelectSlider = (DXSliderParam*)RangeSelectSlider->pParamterData;
		pRangeSelectSlider->Set(false, 'f', true, 0, 1, '\0', &pWindow->addtionalParams[0], 7, vec4(-15, -15, 25, 15));
		DescPage->uiArr.push_back(RangeSelectSlider);

		//1. Desc Btn (23, 106, W535, H619)
		rateloc = DescPage->location;
		rateloc.x += 23;
		rateloc.w -= 106;
		rateloc.z = rateloc.x + 535;
		rateloc.y = rateloc.w - 619;
		DXUI* DescDumyBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn_DescDumyBtn = (DXBtnParam*)DescDumyBtn->pParamterData;
		pbtn_DescDumyBtn->Set(0, 1, 25, L"");
		pbtn_DescDumyBtn->addtionalParams_int[0] = 0; // â�� �������� ���⿡ ���õ� �������� �ε����� �����ؾ� ��.
		DescDumyBtn->SetFunctions(UIRender_CyberBtn001, UIUpdate_QuestDescObject, nullptr);
		DescPage->uiArr.push_back(DescDumyBtn);
	}

	DXPage* sample_page = new DXPage();
	sample_page->BackGroundColor = vec4(0, 0, 0, 0);
	sample_page->location = vec4(-((float)WindowW * 0.5f), -((float)WindowH * 0.5f), (float)WindowW * 0.5f, (float)WindowH * 0.5f);
	pWindow->page_table.push_back(sample_page);
	{
		// ����Ʈ UI

		//1. Close Btn (568, 11, W27, H27)
		vec4 rateloc = sample_page->location;
		rateloc.x += 568;
		rateloc.w -= 11;
		rateloc.z = rateloc.x + 27;
		rateloc.y = rateloc.w - 27;
		DXUI* CloseBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn = (DXBtnParam*)CloseBtn->pParamterData;
		pbtn->Set(0, 1, 3, L"");
		pbtn->addtionalPtr[0] = ui;
		CloseBtn->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, UIEvent_InventoryCloseBtn);
		sample_page->uiArr.push_back(CloseBtn);

		//3. Show Range Select Slider (570, 88, W32, H614)
		rateloc = sample_page->location;
		rateloc.x += 570;
		rateloc.w -= 100;
		rateloc.z = rateloc.x + 32;
		rateloc.y = rateloc.w - 623;
		DXUI* RangeSelectSlider = new DXUI(DXUI_TYPE::DXUI_Slider, sizeof(DXSliderParam), rateloc, 0, new DXSliderParam());
		RangeSelectSlider->SetFunctions(UIRenderCyberSlider, UIUpdateDefaultSlider, UIEventDefaultSlider);
		DXSliderParam* pRangeSelectSlider = (DXSliderParam*)RangeSelectSlider->pParamterData;
		pRangeSelectSlider->Set(false, 'f', true, 0, 1, '\0', &pWindow->addtionalParams[0], 7, vec4(-15, -15, 25, 15));
		sample_page->uiArr.push_back(RangeSelectSlider);

		// ȭ�鿡 ��Ÿ�� �� �ִ� �ִ��� ��ǰ ����
		constexpr int Max_Item = 50;
		// 14 : (10, 90, 556, 75)
		rateloc = sample_page->location;
		rateloc.x += 23;
		rateloc.w -= 105;
		rateloc.z = rateloc.x + 535;
		rateloc.y = rateloc.w - 97;
		*(vec4*)(pWindow->addtionalPtr[1]) = rateloc;
		vec4& ItemShowRange = *(vec4*)(pWindow->addtionalPtr[1]);
		ItemShowRange.w += 20;
		ItemShowRange.y -= 100 * 6;
		for (int i = 0; i < Max_Item; ++i) {
			DXUI* QuestItemObject = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
			DXBtnParam* pbtn_QuestItemObject = (DXBtnParam*)QuestItemObject->pParamterData;
			pbtn_QuestItemObject->Set(0, 1, 21, L"");
			pbtn_QuestItemObject->addtionalParams_int[0] = i; // ���� �������� �ε����� ��� �ִ�.
			pbtn_QuestItemObject->addtionalParams[1] = rateloc.w; // ������ ������Ʈ�� ���� y ��
			pbtn_QuestItemObject->addtionalPtr[1] = &pWindow->addtionalParams[0]; // �����̴� �� ������
			pbtn_QuestItemObject->addtionalPtr[2] = &ItemShowRange; // ������ ������Ʈ�� ������ ����
			pbtn_QuestItemObject->addtionalPtr[3] = pWindow;
			// ����Ʈ ������ ������ �������� �߰��� ���� ������
			pbtn_QuestItemObject->addtionalPtr[4] = DescPage;

			QuestItemObject->SetFunctions(UIRender_QuestItemObject, UIUpdate_QuestItemObject, UIEvent_QuestItemObject);
			sample_page->uiArr.push_back(QuestItemObject);
			rateloc.y -= 100;
			rateloc.w -= 100;
		}
	}

	pWindow->page_stack.push_back(sample_page);
}

#pragma endregion

#pragma region UIDesign_KeyBinding

void UIRender_KeyDumyBtn(DXUI* ui) {
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

	renderLoc.x += 2;
	game.RenderSDFText(pbtn->text, wcslen(pbtn->text), renderLoc, 10, vec4(1, 0, 0, 1), nullptr, nullptr, -0.01f + depth);
}

void UIRender_KeyBindingSlot(DXUI* ui) {
	DXSlotParam* pslot = (DXSlotParam*)ui->pParamterData;
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
}

void UIUpdate_KeyBindingSlot(DXUI* ui, float deltaTime) {
	DXSlotParam* pslot = (DXSlotParam*)ui->pParamterData;
	pslot->flow += deltaTime;
	if (pslot->flow > pslot->maxtime) {
		pslot->flow = pslot->maxtime;
	}
}

void UIEvent_KeyBindingSlot(DXUI* ui) {
}

void UIInitKeyBindingWindow(DXUI* ui) {
	DXWindowParam* pWindow = (DXWindowParam*)ui->pParamterData;
	DXPage* sample_page = new DXPage();
	sample_page->BackGroundColor = vec4(0, 0, 0, 0);
	float WindowW = ui->location.z - ui->location.x;
	float WindowH = ui->location.w - ui->location.y;
	sample_page->location = vec4(-((float)WindowW * 0.5f), -((float)WindowH * 0.5f), (float)WindowW * 0.5f, (float)WindowH * 0.5f);
	pWindow->page_table.push_back(sample_page);
	{
		// ����Ʈ UI

		//1. Close Btn (336, 11, W27, H27)
		vec4 rateloc = sample_page->location;
		rateloc.x += 336;
		rateloc.w -= 11;
		rateloc.z = rateloc.x + 27;
		rateloc.y = rateloc.w - 27;
		DXUI* CloseBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn = (DXBtnParam*)CloseBtn->pParamterData;
		pbtn->Set(0, 1, 3, L"");
		pbtn->addtionalPtr[0] = ui;
		CloseBtn->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, UIEvent_InventoryCloseBtn);
		sample_page->uiArr.push_back(CloseBtn);

		wchar_t KeyBindStr[12][16] = {
			L"Q",
			L"E",
			L"Shift",
			L"RBtn",
			L"T",
			L"F",
			L"G",
			L"Z",
			L"X",
			L"C",
			L"V",
			L"Ctrl"
		};

		vec4 rateloc_Slot;
		vec4 rateloc_KeyBtn;
		rateloc_Slot = sample_page->location;
		rateloc_Slot.x += sample_page->location.x + 22;
		rateloc_Slot.w -= 101;
		rateloc_Slot.z = rateloc_Slot.x + 75;
		rateloc_Slot.y = rateloc_Slot.w - 75;

		rateloc_KeyBtn = sample_page->location;
		rateloc_KeyBtn.x += 39;
		rateloc_KeyBtn.w -= 73;
		rateloc_KeyBtn.z = rateloc_KeyBtn.x + 38;
		rateloc_KeyBtn.y = rateloc_KeyBtn.w - 25;
		for (int iy = 0; iy < 3; ++iy) {
			rateloc_Slot.x = sample_page->location.x + 22;
			rateloc_Slot.z = rateloc_Slot.x + 75;
			rateloc_KeyBtn.x = sample_page->location.x + 39;
			rateloc_KeyBtn.z = rateloc_KeyBtn.x + 38;

			for (int ix = 0; ix < 4; ++ix) {
				int keyindex = iy * 4 + ix;

				//Ű ���� ��ư (336, 11, W27, H27)
				DXUI* KeyDumy = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc_KeyBtn, 0, new DXBtnParam());
				DXBtnParam* pbtn_KeyDumy = (DXBtnParam*)KeyDumy->pParamterData;
				pbtn_KeyDumy->Set(0, 1, 29, KeyBindStr[keyindex]);
				KeyDumy->SetFunctions(UIRender_KeyDumyBtn, UIUpdateDefaultBtn, nullptr);
				sample_page->uiArr.push_back(KeyDumy);

				//���ε� ����
				DXUI* KeyBindingSlot = new DXUI(DXUI_TYPE::DXUI_Slot, sizeof(DXSlotParam), rateloc_Slot, 0, new DXSlotParam());
				DXSlotParam* pslot_KeyBinding = (DXSlotParam*)KeyBindingSlot->pParamterData;
				pslot_KeyBinding->Set(0, 1, (SlotKind)(SlotKind::_Item_Consumable | SlotKind::_Skill), 10);
				KeyBindingSlot->SetFunctions(UIRender_KeyBindingSlot, UIUpdate_KeyBindingSlot, UIEvent_KeyBindingSlot);
				sample_page->uiArr.push_back(KeyBindingSlot);

				rateloc_Slot.x += 85;
				rateloc_Slot.z += 85;
				rateloc_KeyBtn.x += 85;
				rateloc_KeyBtn.z += 85;
			}

			rateloc_Slot.y -= 134;
			rateloc_Slot.w -= 134;
			rateloc_KeyBtn.y -= 134;
			rateloc_KeyBtn.w -= 134;
		}
	}

	pWindow->page_stack.push_back(sample_page);
}
#pragma endregion

#pragma region UIDesign_Social
void UIInitSocialWindow(DXUI* ui) {
	DXWindowParam* pWindow = (DXWindowParam*)ui->pParamterData;
	DXPage* sample_page = new DXPage();
	sample_page->BackGroundColor = vec4(0, 0, 0, 0);
	float WindowW = ui->location.z - ui->location.x;
	float WindowH = ui->location.w - ui->location.y;
	sample_page->location = vec4(-((float)WindowW * 0.5f), -((float)WindowH * 0.5f), (float)WindowW * 0.5f, (float)WindowH * 0.5f);
	pWindow->page_table.push_back(sample_page);
	{
		// ����Ʈ UI

		//1. Close Btn (336, 11, W27, H27)
		vec4 rateloc = sample_page->location;
		rateloc.x += 336;
		rateloc.w -= 11;
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
#pragma endregion

#pragma region UIDesign_NPCTalk
void UIRender_NPCTalkDummyBtn(DXUI* ui) {
	if (game.PresentShowedTalkID < 0) return;
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	vec4 color = vec4(1, 1, 1, 1);
	float depth = ui->depth - game.ui_depth_epsilon;
	float rate = 1.0f - AnimOperUtil::EaseOut(pbtn->flow / pbtn->maxtime, 5);
	constexpr float margin = 10.0f;
	vec4 renderLoc = ui->location + game.CurrentUICenter;
	game.UIDraw_TextureRect(renderLoc, color, depth, pbtn->Base_UITextureIndex);
	renderLoc.x += margin;
	renderLoc.w -= margin;
	game.RenderSDFText(pbtn->text, wcslen(pbtn->text), renderLoc, 15, vec4(1, 1, 1, 1), nullptr, nullptr, -0.01f + depth);
}

void UIUpdate_NPCTalkDummyBtn(DXUI* ui, float deltaTime) {
	if (game.PresentShowedTalkID < 0) return;
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	static int TourId = 0;
	if (game.UITourID != TourId) {
		TourId = game.UITourID;
		if (0 <= game.PresentShowedTalkID && game.PresentShowedTalkID < game.NPCTalkTable.size()) {
			wcscpy_s((wchar_t*)pbtn->text, 256, (wchar_t*)game.NPCTalkTable[game.PresentShowedTalkID].text);
		}
	}
}

void UIUpdate_NPCNameDummyBtn(DXUI* ui, float deltaTime) {
	if (game.PresentShowedTalkID < 0) return;
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	static int TourId = 0;
	if (game.UITourID != TourId) {
		TourId = game.UITourID;
		if (0 <= game.PresentShowedTalkID && game.PresentShowedTalkID < game.NPCTalkTable.size()) {
			wcscpy_s((wchar_t*)pbtn->text, 256, (wchar_t*)game.NPCTalkTable[game.PresentShowedTalkID].SpeakerName);
		}
	}
}

void UIEvent_NPCTalkDummyBtn(DXUI* ui) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
}

void UIRender_TalkLeftBtn(DXUI* ui) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	vec4 color = vec4(1, 1, 1, 1);
	float depth = ui->depth - game.ui_depth_epsilon;
	float rate = 1.0f - AnimOperUtil::EaseOut(pbtn->flow / pbtn->maxtime, 5);
	constexpr float margin = 10.0f;
	vec4 renderLoc = ui->location + game.CurrentUICenter;
	game.UIDraw_TextureRect(renderLoc, color, depth, pbtn->Base_UITextureIndex);
	renderLoc.x += margin;
	renderLoc.w -= margin;
	game.RenderSDFText(pbtn->text, wcslen(pbtn->text), renderLoc, 15, vec4(1, 1, 1, 1), nullptr, nullptr, -0.01f + depth);
}

void UIUpdate_TalkLeftBtn(DXUI* ui, float deltaTime) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
}

void UIEvent_TalkLeftBtn(DXUI* ui) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	if (game.PresentShowedTalkID < 0) return;
	vec4 evtloc = ui->location + game.CurrentUICenter;
	if (game.evt.uMsg == WM_LBUTTONDOWN && game.RectContainPos(evtloc, game.CurrentCursorPos)) {
		pbtn->flow = 0;

		//talk left
	}
}

void UIRender_TalkNextBtn(DXUI* ui) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	if (game.PresentShowedTalkID < 0) return;
	if (pbtn->addtionalParams_int[0]) {
		vec4 color = vec4(1, 1, 1, 1);
		float depth = ui->depth - game.ui_depth_epsilon;
		float rate = 1.0f - AnimOperUtil::EaseOut(pbtn->flow / pbtn->maxtime, 5);
		constexpr float margin = 10.0f;
		vec4 renderLoc = ui->location + game.CurrentUICenter;
		game.UIDraw_TextureRect(renderLoc, color, depth, pbtn->Base_UITextureIndex);
		renderLoc.x += margin;
		renderLoc.w -= margin;
		game.RenderSDFText(pbtn->text, wcslen(pbtn->text), renderLoc, 15, vec4(1, 1, 1, 1), nullptr, nullptr, -0.01f + depth);
	}
}

void UIUpdate_TalkNextBtn(DXUI* ui, float deltaTime) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	if (game.PresentShowedTalkID < 0) return;
	if (game.NPCTalkTable[game.PresentShowedTalkID].selectCnt > 0) {
		pbtn->addtionalParams_int[0] = 0;
	}
	else {
		pbtn->addtionalParams_int[0] = 1;
	}
}

void UIEvent_TalkNextBtn(DXUI* ui) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	if (game.PresentShowedTalkID < 0) return;
	if (pbtn->addtionalParams_int[0]) {
		vec4 evtloc = ui->location + game.CurrentUICenter;
		if (game.evt.uMsg == WM_LBUTTONDOWN && game.RectContainPos(evtloc, game.CurrentCursorPos)) {
			pbtn->flow = 0;

			//talk next
			if (game.NPCTalkTable[game.PresentShowedTalkID].NextEscape) {
				game.PresentShowedTalkID = -1;
				game.mainPageStack.pop_back();
			}
			else {
				game.PresentShowedTalkID += 1;
				game.UITourID += 1;
			}
		}
	}
}

void UIRender_TalkSelectionBtn(DXUI* ui) {
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	if (game.PresentShowedTalkID < 0) return;
	if (pbtn->addtionalParams_int[1]) {
		vec4 color = vec4(1, 1, 1, 1);
		float depth = ui->depth - game.ui_depth_epsilon;
		float rate = 1.0f - AnimOperUtil::EaseOut(pbtn->flow / pbtn->maxtime, 5);
		constexpr float margin = 10.0f;
		vec4 renderLoc = ui->location + game.CurrentUICenter;
		game.UIDraw_TextureRect(renderLoc, color, depth, pbtn->Base_UITextureIndex);
		renderLoc.x += margin;
		renderLoc.w -= margin;
		game.RenderSDFText(pbtn->text, wcslen(pbtn->text), renderLoc, 15, vec4(1, 1, 1, 1), nullptr, nullptr, -0.01f + depth);
	}
}

void UIUpdate_TalkSelectionBtn(DXUI* ui, float deltaTime) {
	if (game.PresentShowedTalkID < 0) return;
	static int TourID = 0;
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	int selindex = pbtn->addtionalParams_int[0];
	int talkID = game.PresentShowedTalkID;
	if (game.NPCTalkTable[talkID].selectCnt > selindex) {
		pbtn->addtionalParams_int[1] = 1;
	}
	else {
		pbtn->addtionalParams_int[1] = 0;
	}

	if (game.UITourID != TourID) {
		if (game.NPCTalkTable[talkID].selectCnt > selindex) {
			wcscpy_s(pbtn->text, 256, (wchar_t*)game.NPCTalkTable[talkID].sel[selindex].selectionText);
		}
	}
}

void UIEvent_TalkSelectionBtn(DXUI* ui) {
	if (game.PresentShowedTalkID < 0) return;
	DXBtnParam* pbtn = (DXBtnParam*)ui->pParamterData;
	if (pbtn->addtionalParams_int[1]) {
		vec4 evtloc = ui->location + game.CurrentUICenter;
		if (game.evt.uMsg == WM_LBUTTONDOWN && game.RectContainPos(evtloc, game.CurrentCursorPos)) {
			pbtn->flow = 0;

			//talk next
			int talkID = game.PresentShowedTalkID;
			int selindex = pbtn->addtionalParams_int[0];
			bool isExistNext = false;
			if (game.NPCTalkTable[talkID].sel[selindex].gotoOtherTalk) {
				game.PresentShowedTalkID = game.NPCTalkTable[talkID].sel[selindex].goto_otherTalkID;
				isExistNext = true;
			}

			//Send Selection Packet
			CTS_Client_NPCTalkSelection_Header header;
			header.size = sizeof(CTS_Client_NPCTalkSelection_Header);
			header.st = CTS_Protocol::Client_NPCTalkSelection;
			header.selectionID = selindex;
			client.send((char*)&header, sizeof(CTS_Client_NPCTalkSelection_Header), 0);

			if (isExistNext == false) {
				game.PresentShowedTalkID = -1;
				game.mainPageStack.pop_back();

				CTS_KeyInput_Header header;
				header.size = sizeof(CTS_KeyInput_Header);
				header.st = CTS_Protocol::KeyInput;
				header.Key = VK_TAB;
				header.isdown = false;
				client.send_all((char*)&header, sizeof(CTS_KeyInput_Header), 0);
			}

			game.UITourID += 1;
		}
	}
}
#pragma endregion

void Game::UI_Init()
{
	//TextureInit
	UITextureTable.push_back(&game.DefaultTex);
	GPUResource* NeonLight = new GPUResource();
	NeonLight->CreateTexture_fromFile(L"Resources/UI/NeonLight.png", game.basicTexFormat, 1, false);
	UITextureTable.push_back(NeonLight);

	constexpr int UITextureCount = 41;
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
		L"Resources/UI/UI_SellBuy_Window.png", // 42
	};

	for (int i = 0; i < UITextureCount; ++i) {
		GPUResource* temp = new GPUResource();
		temp->CreateTexture_fromFile(TextureNames[i], game.basicTexFormat, 1, false);
		UITextureTable.push_back(temp);
	}

	GPUResource* ScopeOverlay = new GPUResource();
	ScopeOverlay->CreateTexture_fromFile(L"Resources/UI/zoom_scope.png", game.basicTexFormat, 1, false);
	ScopeOverlayTextureIndex = (int)UITextureTable.size();
	UITextureTable.push_back(ScopeOverlay);

	auto LoadAmmoHUDTexture = [&](const wchar_t* path) {
		GPUResource* texture = new GPUResource();
		texture->CreateTexture_fromFile(path, game.basicTexFormat, 1, false);
		int index = (int)UITextureTable.size();
		UITextureTable.push_back(texture);
		return index;
	};
	AmmoHUDFrameTextureIndex = LoadAmmoHUDTexture(L"Resources/UI/AmmoHUD/AmmoHUD_Frame.png");
	AmmoHUDWeaponTextureIndices[(int)WeaponType::MachineGun] = LoadAmmoHUDTexture(L"Resources/UI/AmmoHUD/AmmoHUD_MachineGun.png");
	AmmoHUDWeaponTextureIndices[(int)WeaponType::Sniper] = LoadAmmoHUDTexture(L"Resources/UI/AmmoHUD/AmmoHUD_Sniper.png");
	AmmoHUDWeaponTextureIndices[(int)WeaponType::Shotgun] = LoadAmmoHUDTexture(L"Resources/UI/AmmoHUD/AmmoHUD_Shotgun.png");
	AmmoHUDWeaponTextureIndices[(int)WeaponType::Rifle] = LoadAmmoHUDTexture(L"Resources/UI/AmmoHUD/AmmoHUD_Rifle.png");
	AmmoHUDWeaponTextureIndices[(int)WeaponType::Pistol] = LoadAmmoHUDTexture(L"Resources/UI/AmmoHUD/AmmoHUD_Pistol.png");
	AmmoHUDWeaponTextureIndices[(int)WeaponType::DualPistol] = LoadAmmoHUDTexture(L"Resources/UI/AmmoHUD/AmmoHUD_DualPistol.png");
	AmmoHUDWeaponTextureIndices[(int)WeaponType::DronePistol] = AmmoHUDWeaponTextureIndices[(int)WeaponType::Pistol];
	AmmoHUDWeaponTextureIndices[(int)WeaponType::SMG] = LoadAmmoHUDTexture(L"Resources/UI/AmmoHUD/AmmoHUD_SMG.png");
	AmmoHUDWeaponTextureIndices[(int)WeaponType::GrenadeGun] = LoadAmmoHUDTexture(L"Resources/UI/AmmoHUD/AmmoHUD_GrenadeGun.png");
	AmmoHUDBulletTextureIndex = LoadAmmoHUDTexture(L"Resources/UI/AmmoHUD/AmmoHUD_Bullet.png");
	AmmoHUDReloadTextureIndex = LoadAmmoHUDTexture(L"Resources/UI/AmmoHUD/AmmoHUD_Reload.png");
	StatIconTextureIndices[(int)PlayerStatType::HP] = LoadAmmoHUDTexture(L"Resources/UI/Stat_HP.png");
	StatIconTextureIndices[(int)PlayerStatType::Defense] = LoadAmmoHUDTexture(L"Resources/UI/Stat_DEF.png");
	StatIconTextureIndices[(int)PlayerStatType::MoveSpeed] = LoadAmmoHUDTexture(L"Resources/UI/Stat_MOVE.png");
	StatIconTextureIndices[(int)PlayerStatType::Attack] = LoadAmmoHUDTexture(L"Resources/UI/Stat_ATK.png");

	mainPageStack.reserve(32);
	UIPageTable.reserve(32);
	DXPage* sample_page = new DXPage();
	sample_page->BackGroundColor = vec4(0, 0, 0, 0.5f);
	sample_page->location = vec4(-((float)gd.ClientFrameWidth * 0.5f), -((float)gd.ClientFrameHeight * 0.5f), (float)gd.ClientFrameWidth * 0.5f, (float)gd.ClientFrameHeight * 0.5f);
	UIPageTable.push_back(sample_page);
	float ScreenW = gd.ClientFrameWidth;
	float ScreenH = gd.ClientFrameHeight;
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
		pbtn_openInven->Set(0, 1, 6, L"Inventory");
		InventoryWindowOpenBtn->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, UIEventOpen_Inventory);
		sample_page->uiArr.push_back(InventoryWindowOpenBtn);

		rateloc.x += 140;
		rateloc.z += 140;
		DXUI* EquipWindowOpenBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn_openequip = (DXBtnParam*)EquipWindowOpenBtn->pParamterData;
		pbtn_openequip->Set(0, 1, 6, L"Equip");
		EquipWindowOpenBtn->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, UIEventOpen_Inventory);
		sample_page->uiArr.push_back(EquipWindowOpenBtn);

		// ����â�� ���� �� ��ư. 6:(134x54)
		rateloc.x += 140;
		rateloc.z += 140;
		DXUI* ShopWindowOpenBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn_openShop = (DXBtnParam*)ShopWindowOpenBtn->pParamterData;
		pbtn_openShop->Set(0, 1, 6, L"상점");
		ShopWindowOpenBtn->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, UIEventOpen_Inventory);
		if (false) {
			sample_page->uiArr.push_back(ShopWindowOpenBtn);
		}

		// ����Ʈ â�� ���� �� ��ư. 6:(134x54)
		rateloc.x += 140;
		rateloc.z += 140;
		DXUI* QuestWindowOpenBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn_openQuest = (DXBtnParam*)QuestWindowOpenBtn->pParamterData;
		pbtn_openQuest->Set(0, 1, 6, L"Quest");
		QuestWindowOpenBtn->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, UIEventOpen_Inventory);
		sample_page->uiArr.push_back(QuestWindowOpenBtn);

		// Ű ���ε� â�� ���� �� ��ư. 6:(134x54)
		rateloc.x += 140;
		rateloc.z += 140;
		DXUI* KeyBindingWindowOpenBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn_KeyBinding = (DXBtnParam*)KeyBindingWindowOpenBtn->pParamterData;
		pbtn_KeyBinding->Set(0, 1, 6, L"키 바인딩");
			KeyBindingWindowOpenBtn->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, UIEventOpen_Inventory);
		if (false) {
			sample_page->uiArr.push_back(KeyBindingWindowOpenBtn);
		}

		// �Ҽ� â�� ���� �� ��ư. 6:(886x783)
		rateloc.x += 886;
		rateloc.z += 783;
		DXUI* SocialWindowOpenBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn_SocialWindowOpenBtn = (DXBtnParam*)SocialWindowOpenBtn->pParamterData;
		pbtn_SocialWindowOpenBtn->Set(0, 1, 6, L"소셜");
		SocialWindowOpenBtn->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, UIEventOpen_Inventory);
		if (false) {
			sample_page->uiArr.push_back(SocialWindowOpenBtn);
		}

		// �Ҽ� â�� ���� �� ��ư. 6:(886x783)
		rateloc = vec4(0.99f, 0.99f, 0.99f, 0.99f);
		WindowNormalizeCoordToDirectXRenderCoord_vec4(rateloc, ScreenW, ScreenH);
		rateloc.x -= 134;
		rateloc.w += 54;
		DXUI* GameEscapeBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn_GameEscapeBtn = (DXBtnParam*)GameEscapeBtn->pParamterData;
		pbtn_GameEscapeBtn->Set(0, 1, 6, L"게임종료");
		GameEscapeBtn->SetFunctions(UIRender_CyberBtn001, UIUpdateDefaultBtn, UIEventGameEscapeBtn);
		sample_page->uiArr.push_back(GameEscapeBtn);

		//�κ��丮 ������ 2:(605x710)
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
		InventoryWindow->enable = false;
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
		EquipWindow->enable = false;
		sample_page->uiArr.push_back(EquipWindow);
		pbtn_openequip->addtionalPtr[0] = EquipWindow;

		if (false) {
			//����â ������ 12 : (602, 737)
			rateloc = vec4(0.1, 0.1, 0.1, 0.1);
			WindowNormalizeCoordToDirectXRenderCoord_vec4(rateloc, ScreenW, ScreenH);
			rateloc.z += 602;
			rateloc.y -= 737;
			DXUI* ShopWindow = new DXUI(DXUI_TYPE::DXUI_Window, sizeof(DXWindowParam), rateloc, 0, new DXWindowParam());
			ShopWindow->SetFunctions(UIRenderCyberWindow, UIUpdateDefaultWindow, UIEventDefaultWindow);
			DXWindowParam* pShopWindow = (DXWindowParam*)ShopWindow->pParamterData;
			pShopWindow->Set(ShopWindow, 12);
			pShopWindow->LastFocusedTime = chrono::system_clock::now();
			// param 0 �� �����̴��� �����ϰ� �־ 0��°�� �ȵ�.
			pShopWindow->addtionalPtr[1] = new vec4(); // ���� ���� �������� ������ ����
			UIInitShopWindow(ShopWindow);
			ShopWindow->enable = false;
			sample_page->uiArr.push_back(ShopWindow);
			// ��ư�� �������x �����찡 �������� ����
			pbtn_openShop->addtionalPtr[0] = ShopWindow;
		}
		

		//����Ʈ â ������ 12 : (602, 737)
		rateloc = vec4(0.1, 0.1, 0.1, 0.1);
		WindowNormalizeCoordToDirectXRenderCoord_vec4(rateloc, ScreenW, ScreenH);
		rateloc.z += 602;
		rateloc.y -= 755;
		DXUI* QuestWindow = new DXUI(DXUI_TYPE::DXUI_Window, sizeof(DXWindowParam), rateloc, 0, new DXWindowParam());
		QuestWindow->SetFunctions(UIRenderCyberWindow, UIUpdateDefaultWindow, UIEventDefaultWindow);
		DXWindowParam* pQuestWindow = (DXWindowParam*)QuestWindow->pParamterData;
		pQuestWindow->Set(QuestWindow, 24);
		pQuestWindow->LastFocusedTime = chrono::system_clock::now();
		// param 0 �� �����̴��� �����ϰ� �־ 0��°�� �ȵ�.
		pQuestWindow->addtionalPtr[1] = new vec4(); // ���� ���� �������� ������ ����
		UIInitQuestWindow(QuestWindow);
		QuestWindow->enable = false;
		sample_page->uiArr.push_back(QuestWindow);
		// ��ư�� �������x �����찡 �������� ����
		pbtn_openQuest->addtionalPtr[0] = QuestWindow;

		if (false) {
			//Ű ���ε� â ������ 12 : (372, 505)
			rateloc = vec4(0.1, 0.1, 0.1, 0.1);
			WindowNormalizeCoordToDirectXRenderCoord_vec4(rateloc, ScreenW, ScreenH);
			rateloc.z += 372;
			rateloc.y -= 505;
			DXUI* KeyBindWindow = new DXUI(DXUI_TYPE::DXUI_Window, sizeof(DXWindowParam), rateloc, 0, new DXWindowParam());
			KeyBindWindow->SetFunctions(UIRenderCyberWindow, UIUpdateDefaultWindow, UIEventDefaultWindow);
			DXWindowParam* pKeyBindWindow = (DXWindowParam*)KeyBindWindow->pParamterData;
			pKeyBindWindow->Set(KeyBindWindow, 28);
			pKeyBindWindow->LastFocusedTime = chrono::system_clock::now();
			// param 0 �� �����̴��� �����ϰ� �־ 0��°�� �ȵ�.
			UIInitKeyBindingWindow(KeyBindWindow);
			KeyBindWindow->enable = false;
			sample_page->uiArr.push_back(KeyBindWindow);
			// ��ư�� �������x �����찡 �������� ����
			pbtn_KeyBinding->addtionalPtr[0] = KeyBindWindow;
		}
		

		if (false) {
			// �Ҽ� ������
			rateloc = vec4(0.1, 0.1, 0.1, 0.1);
			WindowNormalizeCoordToDirectXRenderCoord_vec4(rateloc, ScreenW, ScreenH);
			rateloc.z += 886;
			rateloc.y -= 783;
			DXUI* SocialWindow = new DXUI(DXUI_TYPE::DXUI_Window, sizeof(DXWindowParam), rateloc, 0, new DXWindowParam());
			SocialWindow->SetFunctions(UIRenderCyberWindow, UIUpdateDefaultWindow, UIEventDefaultWindow);
			DXWindowParam* pSocialWindow = (DXWindowParam*)SocialWindow->pParamterData;
			pSocialWindow->Set(SocialWindow, 30);
			pSocialWindow->LastFocusedTime = chrono::system_clock::now();
			// param 0 �� �����̴��� �����ϰ� �־ 0��°�� �ȵ�.
			UIInitSocialWindow(SocialWindow);
			SocialWindow->enable = false;
			sample_page->uiArr.push_back(SocialWindow);
			// ��ư�� �������x �����찡 �������� ����
			pbtn_SocialWindowOpenBtn->addtionalPtr[0] = SocialWindow;
		}
	}

	DXPage* NPCTalkPage = new DXPage();
	NPCTalkPage->BackGroundColor = vec4(0, 0, 0, 0.1f);
	NPCTalkPage->location = vec4(-((float)gd.ClientFrameWidth * 0.5f), -((float)gd.ClientFrameHeight * 0.5f), (float)gd.ClientFrameWidth * 0.5f, (float)gd.ClientFrameHeight * 0.5f);
	UIPageTable.push_back(NPCTalkPage);
	{
		vec4 rateloc = vec4(0.1f, 0.7f, 0.9f, 0.9f);
		WindowNormalizeCoordToDirectXRenderCoord_vec4(rateloc, ScreenW, ScreenH);
		DXUI* NPCTalkDumyBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn_NPCTalkDumyBtn = (DXBtnParam*)NPCTalkDumyBtn->pParamterData;
		pbtn_NPCTalkDumyBtn->Set(0, 1, 35, L"대화창");
		NPCTalkDumyBtn->SetFunctions(UIRender_NPCTalkDummyBtn, UIUpdate_NPCTalkDummyBtn, UIEvent_NPCTalkDummyBtn);
		NPCTalkPage->uiArr.push_back(NPCTalkDumyBtn);

		rateloc = vec4(0.1f, 0.6f, 0.2f, 0.7f);
		WindowNormalizeCoordToDirectXRenderCoord_vec4(rateloc, ScreenW, ScreenH);
		DXUI* NPCNameDumyBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn_NPCNameDumyBtn = (DXBtnParam*)NPCNameDumyBtn->pParamterData;
		pbtn_NPCNameDumyBtn->Set(0, 1, 6, L"이름");
		NPCNameDumyBtn->SetFunctions(UIRender_NPCTalkDummyBtn, UIUpdate_NPCNameDummyBtn, UIEvent_NPCTalkDummyBtn);
		NPCTalkPage->uiArr.push_back(NPCNameDumyBtn);

		//rateloc = vec4(0.8f, 0.85f, 0.9f, 0.95f);
		//WindowNormalizeCoordToDirectXRenderCoord_vec4(rateloc, ScreenW, ScreenH);
		//DXUI* TalkLeftDumyBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		//DXBtnParam* pbtn_TalkLeftDumyBtn = (DXBtnParam*)TalkLeftDumyBtn->pParamterData;
		//pbtn_TalkLeftDumyBtn->Set(0, 1, 6, L"��ȭ ������");
		//TalkLeftDumyBtn->SetFunctions(UIRender_TalkLeftBtn, UIUpdate_TalkLeftBtn, UIEvent_TalkLeftBtn);
		//NPCTalkPage->uiArr.push_back(TalkLeftDumyBtn);

		rateloc = vec4(0.8f, 0.6f, 0.9f, 0.7f);
		WindowNormalizeCoordToDirectXRenderCoord_vec4(rateloc, ScreenW, ScreenH);
		DXUI* TalkNextBtn = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn_TalkNextBtn = (DXBtnParam*)TalkNextBtn->pParamterData;
		pbtn_TalkNextBtn->Set(0, 1, 6, L"다음");
		pbtn_TalkNextBtn->addtionalParams_int[0] = 1;
		TalkNextBtn->SetFunctions(UIRender_TalkNextBtn, UIUpdate_TalkNextBtn, UIEvent_TalkNextBtn);
		NPCTalkPage->uiArr.push_back(TalkNextBtn);

		// Selection
		rateloc = vec4(0.8f, 0.6f, 0.9f, 0.7f);
		WindowNormalizeCoordToDirectXRenderCoord_vec4(rateloc, ScreenW, ScreenH);
		DXUI* TalkSelectionBtn1 = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn_TalkSelectionBtn1 = (DXBtnParam*)TalkSelectionBtn1->pParamterData;
		pbtn_TalkSelectionBtn1->Set(0, 1, 6, L"선택지1");
		pbtn_TalkSelectionBtn1->addtionalParams_int[0] = 0;
		TalkSelectionBtn1->SetFunctions(UIRender_TalkSelectionBtn, UIUpdate_TalkSelectionBtn, UIEvent_TalkSelectionBtn);
		NPCTalkPage->uiArr.push_back(TalkSelectionBtn1);

		rateloc = vec4(0.8f, 0.5f, 0.9f, 0.6f);
		WindowNormalizeCoordToDirectXRenderCoord_vec4(rateloc, ScreenW, ScreenH);
		DXUI* TalkSelectionBtn2 = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn_TalkSelectionBtn2 = (DXBtnParam*)TalkSelectionBtn2->pParamterData;
		pbtn_TalkSelectionBtn2->Set(0, 1, 6, L"선택지2");
		pbtn_TalkSelectionBtn2->addtionalParams_int[0] = 1;
		TalkSelectionBtn2->SetFunctions(UIRender_TalkSelectionBtn, UIUpdate_TalkSelectionBtn, UIEvent_TalkSelectionBtn);
		NPCTalkPage->uiArr.push_back(TalkSelectionBtn2);

		rateloc = vec4(0.8f, 0.4f, 0.9f, 0.5f);
		WindowNormalizeCoordToDirectXRenderCoord_vec4(rateloc, ScreenW, ScreenH);
		DXUI* TalkSelectionBtn3 = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn_TalkSelectionBtn3 = (DXBtnParam*)TalkSelectionBtn3->pParamterData;
		pbtn_TalkSelectionBtn3->Set(0, 1, 6, L"선택지3");
		pbtn_TalkSelectionBtn3->addtionalParams_int[0] = 2;
		TalkSelectionBtn3->SetFunctions(UIRender_TalkSelectionBtn, UIUpdate_TalkSelectionBtn, UIEvent_TalkSelectionBtn);
		NPCTalkPage->uiArr.push_back(TalkSelectionBtn3);

		rateloc = vec4(0.8f, 0.3f, 0.9f, 0.4f);
		WindowNormalizeCoordToDirectXRenderCoord_vec4(rateloc, ScreenW, ScreenH);
		DXUI* TalkSelectionBtn4 = new DXUI(DXUI_TYPE::DXUI_Btn, sizeof(DXBtnParam), rateloc, 0, new DXBtnParam());
		DXBtnParam* pbtn_TalkSelectionBtn4 = (DXBtnParam*)TalkSelectionBtn4->pParamterData;
		pbtn_TalkSelectionBtn4->Set(0, 1, 6, L"선택지4");
		pbtn_TalkSelectionBtn4->addtionalParams_int[0] = 3;
		TalkSelectionBtn4->SetFunctions(UIRender_TalkSelectionBtn, UIUpdate_TalkSelectionBtn, UIEvent_TalkSelectionBtn);
		NPCTalkPage->uiArr.push_back(TalkSelectionBtn4);
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


void Game::UIDraw_SolidRect(vec4 loc, vec4 color, float depth)
{
	// A negative blue channel selects the texture-free branch in the screen
	// shader. The shader restores the original positive blue value.
	color.b = -max(color.b, 0.0001f);
	UIDraw_TextureRect(loc, color, depth, 0);
}

#include "stdafx.h"
#include "Render.h"
#include "Game.h"
#include "GameObject.h"
#include "MeshSimplifier.h"

extern int dbgc[128];

Mesh* BulletRay::mesh = nullptr;
extern GlobalDevice gd;
extern Game game;

static float GetPlayerUpperBodyYawCorrection(int animationIndex);

namespace {
	constexpr bool kEnableMeshLODRender = true;
	constexpr float kMeshLODEnterScreenRadius = 190.0f;
	constexpr float kMeshLODLeaveScreenRadius = 260.0f;
	constexpr float kMeshLOD2EnterScreenRadius = 55.0f;
	constexpr float kMeshLOD2LeaveScreenRadius = 85.0f;
	constexpr float kBoxLODEnterScreenRadius = 55.0f;
	constexpr float kBoxLODLeaveScreenRadius = 85.0f;
	constexpr size_t kMeshLODMinTriangleCount = 180;
	constexpr float kMeshLODMaxRenderedTriangleRatio = 0.98f;
	constexpr size_t kMeshLODMediumTriangleCount = 1800;
	constexpr size_t kMeshLODSmallCullTriangleCount = 180;
	constexpr float kBoxLODVisualShrink = 1.0f;
	constexpr float kBoxLODThinProxyMinExtent = 0.14f;
	constexpr float kBoxLODThinProxyLongExtent = 1.4f;
	constexpr float kBoxLODMeshProxyMaxExtent = 6.0f;
	constexpr float kBoxLODPartMaxExtent = 2.2f;
	constexpr size_t kBoxLODMeshProxyMaxTriangleCount = 4500;
	constexpr unsigned int kBoxLODProxyInitialInstanceCapacity = 4096;
	bool gEnableBoxLOD = false;
	std::unordered_map<const void*, bool> gMeshLODState;
	std::unordered_map<const void*, bool> gBoxLODState;
	BumpMesh* gBoxLODProxyMesh = nullptr;
	int gBoxLODProxyMaterialIndex = -1;

	bool IsTurretRenderModel(Model* model)
	{
		static Model* turretModel = nullptr;
		static bool initialized = false;
		if (!initialized) {
			int turretShapeIndex = Shape::GetShapeIndex("MonsterTurret");
			if (turretShapeIndex >= 0 && turretShapeIndex < (int)Shape::ShapeTable.size()) {
				turretModel = Shape::ShapeTable[turretShapeIndex].GetModel();
			}
			initialized = true;
		}
		return model != nullptr && model == turretModel;
	}

	bool IsDroneRenderModel(Model* model)
	{
		static Model* droneModel = nullptr;
		static bool initialized = false;
		if (!initialized) {
			int droneShapeIndex = Shape::GetShapeIndex("MonsterDrone");
			if (droneShapeIndex >= 0 && droneShapeIndex < (int)Shape::ShapeTable.size()) {
				droneModel = Shape::ShapeTable[droneShapeIndex].GetModel();
			}
			initialized = true;
		}
		return model != nullptr && model == droneModel;
	}

	matrix GetModelRenderWorld(Model* model, const matrix& world)
	{
		if (IsTurretRenderModel(model)) {
			matrix modelFix = XMMatrixRotationX(XM_PI);
			modelFix *= XMMatrixRotationY(XMConvertToRadians(90.0f));
			return modelFix * (XMMATRIX)world;
		}
		return world;
	}

	matrix GetSkinMeshRenderWorld(Model* model, const matrix& world)
	{
		if (IsDroneRenderModel(model)) {
			matrix modelFix = XMMatrixRotationY(XMConvertToRadians(-90.0f));
			return modelFix * (XMMATRIX)world;
		}
		return world;
	}

	int GetBoxLODProxyMaterialIndex()
	{
		if (gBoxLODProxyMaterialIndex < 0) {
			Material* mat = new Material();
			mat->clr.base = vec4(0.48f, 0.48f, 0.46f, 1.0f);
			mat->clr.specular = vec4(0.0f, 0.0f, 0.0f, 0.0f);
			mat->clr.reflective = vec4(0.0f, 0.0f, 0.0f, 0.0f);
			mat->roughnessFactor = 1.0f;
			mat->metallicFactor = 0.0f;
			mat->specularFactor = 0.0f;
			mat->SetDescTable(-1);
			gBoxLODProxyMaterialIndex = static_cast<int>(game.MaterialTable.size());
			game.MaterialTable.push_back(mat);
			game.RenderMaterialTable.push_back(mat);
		}
		return gBoxLODProxyMaterialIndex;
	}

	BumpMesh* GetBoxLODProxyMesh()
	{
		if (gBoxLODProxyMesh == nullptr) {
			gBoxLODProxyMesh = new BumpMesh();
			gBoxLODProxyMesh->IsAutoLODGenerated = true;
			gBoxLODProxyMesh->CreateWallMesh(1.0f, 1.0f, 1.0f, vec4(1, 1, 1, 1));
			gBoxLODProxyMesh->subMeshNum = 1;
			gBoxLODProxyMesh->SubMeshIndexStart = new int[2];
			gBoxLODProxyMesh->SubMeshIndexStart[0] = 0;
			gBoxLODProxyMesh->SubMeshIndexStart[1] = 36;
			gBoxLODProxyMesh->InstanceData = new Mesh::InstancingStruct[1];
			gBoxLODProxyMesh->InstanceData[0].Init(kBoxLODProxyInitialInstanceCapacity, gBoxLODProxyMesh);
			game.AddMesh(gBoxLODProxyMesh);
		}
		return gBoxLODProxyMesh;
	}

	matrix MakeBoxLODWorldMatrix(const BoundingOrientedBox& obb)
	{
		matrix world = XMMatrixScaling(obb.Extents.x * kBoxLODVisualShrink, obb.Extents.y * kBoxLODVisualShrink, obb.Extents.z * kBoxLODVisualShrink);
		world.trQ(obb.Orientation);
		world.pos.f3 = obb.Center;
		world.pos.w = 1;
		return world;
	}

	struct BoxLODProxyInstance {
		BoundingOrientedBox obb;
		int materialIndex = -1;
	};

	struct BoxLODMeshInstance {
		Mesh* drawMesh = nullptr;
		Mesh* sourceMesh = nullptr;
		const int* materials = nullptr;
		matrix world;
	};

	struct BoxLODHybridProxy {
		std::vector<BoxLODProxyInstance> boxes;
		std::vector<BoxLODMeshInstance> meshes;
	};

	enum class AutoLODPartClass {
		CullIntoProxy,
		MeshSimplify,
		BoxProxy,
	};

	std::unordered_map<const GameObject*, BoxLODHybridProxy> gBoxLODProxyCache;

	bool IsValidMaterialIndex(int materialIndex)
	{
		return materialIndex >= 0 && materialIndex < static_cast<int>(game.MaterialTable.size());
	}

	bool IsMeshLODRenderWorthUsing(Mesh* mesh)
	{
		if (mesh == nullptr || mesh->type != Mesh::MeshType::_BumpMesh) return false;
		BumpMesh* bumpMesh = static_cast<BumpMesh*>(mesh);
		return bumpMesh->sourceIndexData.size() >= kMeshLODMinTriangleCount;
	}

	size_t GetMeshTriangleCount(Mesh* mesh)
	{
		if (mesh == nullptr || mesh->type != Mesh::MeshType::_BumpMesh) return 0;
		BumpMesh* bumpMesh = static_cast<BumpMesh*>(mesh);
		if (!bumpMesh->sourceIndexData.empty()) return bumpMesh->sourceIndexData.size();
		if (bumpMesh->SubMeshIndexStart != nullptr && bumpMesh->subMeshNum > 0) {
			return static_cast<size_t>(bumpMesh->SubMeshIndexStart[bumpMesh->subMeshNum] / 3);
		}
		return 0;
	}

	size_t GetMeshDrawCount(Mesh* mesh)
	{
		if (mesh == nullptr || mesh->type != Mesh::MeshType::_BumpMesh) return 0;
		BumpMesh* bumpMesh = static_cast<BumpMesh*>(mesh);
		return static_cast<size_t>(max(1, bumpMesh->subMeshNum));
	}

	bool IsFragileLODStructureMesh(Mesh* mesh)
	{
		if (mesh == nullptr || mesh->type != Mesh::MeshType::_BumpMesh) return false;
		BumpMesh* bumpMesh = static_cast<BumpMesh*>(mesh);
		const float ex = max(bumpMesh->OBB_Ext.x, 1e-4f);
		const float ey = max(bumpMesh->OBB_Ext.y, 1e-4f);
		const float ez = max(bumpMesh->OBB_Ext.z, 1e-4f);
		const float maxExt = max(ex, max(ey, ez));
		const float minExt = min(ex, min(ey, ez));
		const float midExt = ex + ey + ez - maxExt - minExt;
		const float aspect = maxExt / minExt;
		const size_t triangleCount = GetMeshTriangleCount(mesh);
		if (triangleCount < 48) return false;
		if (aspect >= 12.0f) return true;
		return aspect >= 7.0f && midExt <= maxExt * 0.18f;
	}

	bool IsFragileLODStructureModel(Model* model)
	{
		if (model == nullptr || model->mNumMeshes < 4) return false;
		const BoundingOrientedBox obb = model->GetOBB();
		const float ex = max(obb.Extents.x, 1e-4f);
		const float ey = max(obb.Extents.y, 1e-4f);
		const float ez = max(obb.Extents.z, 1e-4f);
		const float maxExt = max(ex, max(ey, ez));
		const float minExt = min(ex, min(ey, ez));
		const float modelAspect = maxExt / minExt;

		int fragileMeshCount = 0;
		size_t fragileTriangles = 0;
		size_t totalTriangles = 0;
		for (unsigned int i = 0; i < model->mNumMeshes; ++i) {
			Mesh* mesh = model->mMeshes[i];
			const size_t meshTriangles = GetMeshTriangleCount(mesh);
			totalTriangles += meshTriangles;
			if (IsFragileLODStructureMesh(mesh)) {
				++fragileMeshCount;
				fragileTriangles += meshTriangles;
			}
		}

		if (fragileMeshCount >= 3 && modelAspect >= 3.0f) return true;
		return fragileMeshCount >= 2 && totalTriangles > 0 && fragileTriangles >= totalTriangles / 3;
	}

	void AccumulateSourceTrianglesForMesh(Mesh* mesh, size_t& sourceTriangles)
	{
		sourceTriangles += GetMeshTriangleCount(mesh);
	}

	void AccumulateSourceTrianglesForModel(Model* model, size_t& sourceTriangles)
	{
		if (model == nullptr) return;
		for (int i = 0; i < model->mNumMeshes; ++i) {
			AccumulateSourceTrianglesForMesh(model->mMeshes[i], sourceTriangles);
		}
	}

	void AccumulateSourceDrawsForModel(Model* model, size_t& sourceDraws)
	{
		if (model == nullptr) return;
		for (int i = 0; i < model->mNumMeshes; ++i) {
			sourceDraws += GetMeshDrawCount(model->mMeshes[i]);
		}
	}

	Mesh* GetEffectiveLODMesh(Mesh* sourceMesh, int lodLevel)
	{
		if (!IsMeshLODRenderWorthUsing(sourceMesh)) return nullptr;
		Mesh* lodMesh = AutoLOD_GetLODMesh(sourceMesh, lodLevel);
		if (lodMesh == nullptr && lodLevel > 0) lodMesh = AutoLOD_GetLODMesh(sourceMesh, 0);
		if (lodMesh == nullptr) return nullptr;
		const size_t sourceTriangles = GetMeshTriangleCount(sourceMesh);
		const size_t lodTriangles = GetMeshTriangleCount(lodMesh);
		if (sourceTriangles == 0 || lodTriangles == 0) return nullptr;
		if (float(lodTriangles) > float(sourceTriangles) * kMeshLODMaxRenderedTriangleRatio) return nullptr;
		return lodMesh;
	}

	void AccumulateLODStatsForMesh(Mesh* sourceMesh, size_t& sourceTriangles, size_t& renderedTriangles)
	{
		Mesh* lodMesh = GetEffectiveLODMesh(sourceMesh, 0);
		if (lodMesh == nullptr) return;
		sourceTriangles += GetMeshTriangleCount(sourceMesh);
		renderedTriangles += GetMeshTriangleCount(lodMesh);
	}

	void AccumulateLODDrawStatsForMesh(Mesh* sourceMesh, size_t& sourceDraws, size_t& renderedDraws)
	{
		Mesh* lodMesh = GetEffectiveLODMesh(sourceMesh, 0);
		if (lodMesh == nullptr) return;
		sourceDraws += GetMeshDrawCount(sourceMesh);
		renderedDraws += GetMeshDrawCount(lodMesh);
	}

	void AccumulateLODStatsForModel(Model* model, size_t& sourceTriangles, size_t& renderedTriangles)
	{
		if (model == nullptr) return;
		for (int i = 0; i < model->mNumMeshes; ++i) {
			AccumulateLODStatsForMesh(model->mMeshes[i], sourceTriangles, renderedTriangles);
		}
	}

	void AccumulateLODDrawStatsForModel(Model* model, size_t& sourceDraws, size_t& renderedDraws)
	{
		if (model == nullptr) return;
		for (int i = 0; i < model->mNumMeshes; ++i) {
			AccumulateLODDrawStatsForMesh(model->mMeshes[i], sourceDraws, renderedDraws);
		}
	}

	void RecordLODFrameMissesForModel(Model* model, int lodLevel)
	{
		if (model == nullptr) return;
		for (int i = 0; i < model->mNumMeshes; ++i) {
			Mesh* sourceMesh = model->mMeshes[i];
			if (GetEffectiveLODMesh(sourceMesh, lodLevel) != nullptr) continue;
			AutoLOD_RecordFrameMiss(sourceMesh, GetMeshTriangleCount(sourceMesh));
		}
	}

	void AccumulateLODStatsForProxy(const BoxLODHybridProxy& proxy, size_t& sourceTriangles, size_t& renderedTriangles)
	{
		for (const BoxLODMeshInstance& meshInstance : proxy.meshes) {
			sourceTriangles += GetMeshTriangleCount(meshInstance.sourceMesh);
			renderedTriangles += GetMeshTriangleCount(meshInstance.drawMesh);
		}
		renderedTriangles += proxy.boxes.size() * 12;
	}

	void AccumulateRenderedTrianglesForProxy(const BoxLODHybridProxy& proxy, size_t& renderedTriangles)
	{
		for (const BoxLODMeshInstance& meshInstance : proxy.meshes) {
			renderedTriangles += GetMeshTriangleCount(meshInstance.drawMesh);
		}
		renderedTriangles += proxy.boxes.size() * 12;
	}

	AutoLODPartClass ClassifyLODPart(BumpMesh* mesh, const BoundingOrientedBox& worldOBB)
	{
		if (mesh == nullptr) return AutoLODPartClass::BoxProxy;
		const float minExtent = min(worldOBB.Extents.x, min(worldOBB.Extents.y, worldOBB.Extents.z));
		const float maxExtent = max(worldOBB.Extents.x, max(worldOBB.Extents.y, worldOBB.Extents.z));
		const float midExtent = worldOBB.Extents.x + worldOBB.Extents.y + worldOBB.Extents.z - minExtent - maxExtent;
		const size_t triangleCount = mesh->sourceIndexData.size();
		const bool thinDetail = minExtent <= kBoxLODThinProxyMinExtent && maxExtent >= kBoxLODThinProxyLongExtent;
		const bool boxLikeSolid =
			minExtent >= maxExtent * 0.10f &&
			midExtent >= maxExtent * 0.22f;
		if (thinDetail) return AutoLODPartClass::CullIntoProxy;
		if (triangleCount < kMeshLODSmallCullTriangleCount && maxExtent <= 1.0f) return AutoLODPartClass::CullIntoProxy;
		if (boxLikeSolid && maxExtent >= 0.8f && maxExtent <= kBoxLODMeshProxyMaxExtent) return AutoLODPartClass::BoxProxy;
		if (triangleCount >= kMeshLODMinTriangleCount && GetEffectiveLODMesh(mesh, 0) != nullptr) return AutoLODPartClass::MeshSimplify;
		return AutoLODPartClass::MeshSimplify;
	}

	bool ShouldSkipThinBoxProxy(const BoundingOrientedBox& obb)
	{
		const float minExtent = min(obb.Extents.x, min(obb.Extents.y, obb.Extents.z));
		const float maxExtent = max(obb.Extents.x, max(obb.Extents.y, obb.Extents.z));
		return minExtent <= kBoxLODThinProxyMinExtent && maxExtent >= kBoxLODThinProxyLongExtent;
	}
	float GetMaxExtent(const BoundingOrientedBox& obb)
	{
		return max(obb.Extents.x, max(obb.Extents.y, obb.Extents.z));
	}

	bool ShouldUseMeshProxyInsideBoxLOD(BumpMesh* mesh, const BoundingOrientedBox& worldOBB)
	{
		if (mesh == nullptr) return false;
		if (GetEffectiveLODMesh(mesh, 0) == nullptr) return false;
		return ClassifyLODPart(mesh, worldOBB) == AutoLODPartClass::MeshSimplify;
	}
	bool TryBuildSubMeshOBB(BumpMesh* mesh, int subMeshIndex, BoundingOrientedBox& outOBB)
	{
		if (mesh == nullptr) return false;
		if (mesh->sourceVertexData.empty() || mesh->sourceIndexData.empty() || mesh->sourceSubMeshIndexStart.size() < 2) {
			outOBB = mesh->GetOBB();
			return outOBB.Extents.x > 0 && outOBB.Extents.y > 0 && outOBB.Extents.z > 0;
		}
		if (subMeshIndex < 0 || subMeshIndex + 1 >= static_cast<int>(mesh->sourceSubMeshIndexStart.size())) return false;

		const int startIndex = mesh->sourceSubMeshIndexStart[subMeshIndex];
		const int endIndex = mesh->sourceSubMeshIndexStart[subMeshIndex + 1];
		if (endIndex <= startIndex) return false;

		XMFLOAT3 minPos(FLT_MAX, FLT_MAX, FLT_MAX);
		XMFLOAT3 maxPos(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		bool hasVertex = false;
		const int startTri = startIndex / 3;
		const int endTri = (endIndex + 2) / 3;
		for (int tri = startTri; tri < endTri && tri < static_cast<int>(mesh->sourceIndexData.size()); ++tri) {
			const TriangleIndex& index = mesh->sourceIndexData[tri];
			for (int corner = 0; corner < 3; ++corner) {
				const unsigned int vertexIndex = index.v[corner];
				if (vertexIndex >= mesh->sourceVertexData.size()) continue;
				const XMFLOAT3& p = mesh->sourceVertexData[vertexIndex].position;
				minPos.x = min(minPos.x, p.x);
				minPos.y = min(minPos.y, p.y);
				minPos.z = min(minPos.z, p.z);
				maxPos.x = max(maxPos.x, p.x);
				maxPos.y = max(maxPos.y, p.y);
				maxPos.z = max(maxPos.z, p.z);
				hasVertex = true;
			}
		}
		if (!hasVertex) return false;

		const XMFLOAT3 center(
			(minPos.x + maxPos.x) * 0.5f,
			(minPos.y + maxPos.y) * 0.5f,
			(minPos.z + maxPos.z) * 0.5f);
		const XMFLOAT3 extents(
			(maxPos.x - minPos.x) * 0.5f,
			(maxPos.y - minPos.y) * 0.5f,
			(maxPos.z - minPos.z) * 0.5f);
		outOBB = BoundingOrientedBox(center, extents, XMFLOAT4(0, 0, 0, 1));
		return extents.x > 0 && extents.y > 0 && extents.z > 0;
	}

	void PushMeshProxyInstances(BumpMesh* mesh, const matrix& world, const int* materials, std::vector<BoxLODProxyInstance>& outInstances)
	{
		if (mesh == nullptr) return;
		const int fallbackMaterialIndex = GetBoxLODProxyMaterialIndex();
		const int subMeshCount = max(1, mesh->subMeshNum);
		for (int subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex) {
			BoundingOrientedBox localOBB;
			if (!TryBuildSubMeshOBB(mesh, subMeshIndex, localOBB)) continue;
			BoundingOrientedBox worldOBB;
			localOBB.Transform(worldOBB, world);
			if (worldOBB.Extents.x <= 0 || worldOBB.Extents.y <= 0 || worldOBB.Extents.z <= 0) continue;
			const AutoLODPartClass partClass = ClassifyLODPart(mesh, worldOBB);
			if (partClass == AutoLODPartClass::CullIntoProxy) continue;
			if (partClass == AutoLODPartClass::MeshSimplify) continue;
			if (GetMaxExtent(worldOBB) > kBoxLODPartMaxExtent) continue;
			if (ShouldSkipThinBoxProxy(worldOBB)) continue;

			int materialIndex = materials != nullptr ? materials[subMeshIndex] : fallbackMaterialIndex;
			if (!IsValidMaterialIndex(materialIndex)) materialIndex = fallbackMaterialIndex;
			outInstances.push_back({ worldOBB, materialIndex });
		}
	}

	void PushModelProxyInstances(ModelNode* node, Model* model, const matrix& parentMat, GameObject* obj, BoxLODHybridProxy& outProxy)
	{
		if (node == nullptr || model == nullptr) return;
		XMMATRIX sav = XMMatrixMultiply(node->transform, parentMat);
		if (obj != nullptr) {
			int nodeindex = ((char*)node - (char*)model->RootNode) / sizeof(ModelNode);
			sav = XMMatrixMultiply(obj->transforms_innerModel[nodeindex], parentMat);
		}

		if (node->numMesh != 0 && node->Meshes != nullptr) {
			for (unsigned int meshSlot = 0; meshSlot < node->numMesh; ++meshSlot) {
				Mesh* rawMesh = model->mMeshes[node->Meshes[meshSlot]];
				if (rawMesh == nullptr || rawMesh->type != Mesh::MeshType::_BumpMesh) continue;
				BumpMesh* bumpMesh = static_cast<BumpMesh*>(rawMesh);
				BoundingOrientedBox meshOBB;
				bumpMesh->GetOBB().Transform(meshOBB, sav);
				if (ShouldUseMeshProxyInsideBoxLOD(bumpMesh, meshOBB)) {
					outProxy.meshes.push_back({ GetEffectiveLODMesh(bumpMesh, 0), bumpMesh, node->materialIndex, sav });
					continue;
				}
				PushMeshProxyInstances(bumpMesh, sav, node->materialIndex, outProxy.boxes);
			}
		}

		for (unsigned int childIndex = 0; childIndex < node->numChildren; ++childIndex) {
			PushModelProxyInstances(node->Childrens[childIndex], model, sav, obj, outProxy);
		}
	}

	BoxLODHybridProxy BuildBoxLODProxyInstances(GameObject* obj, Mesh* mesh, Model* model, const matrix& world, const BoundingOrientedBox& fallbackOBB, const int* materials)
	{
		BoxLODHybridProxy proxy;
		if (mesh != nullptr && mesh->type == Mesh::MeshType::_BumpMesh) {
			BumpMesh* bumpMesh = static_cast<BumpMesh*>(mesh);
			if (ShouldUseMeshProxyInsideBoxLOD(bumpMesh, fallbackOBB)) {
				proxy.meshes.push_back({ GetEffectiveLODMesh(bumpMesh, 0), bumpMesh, materials, world });
			}
			else {
				PushMeshProxyInstances(bumpMesh, world, materials, proxy.boxes);
			}
		}
		else if (model != nullptr && model->RootNode != nullptr) {
			PushModelProxyInstances(model->RootNode, model, world, obj, proxy);
		}

		return proxy;
	}

	const BoxLODHybridProxy& GetCachedBoxLODProxyInstances(GameObject* obj, Mesh* mesh, Model* model, const matrix& world, const BoundingOrientedBox& fallbackOBB, const int* materials)
	{
		auto it = gBoxLODProxyCache.find(obj);
		if (it != gBoxLODProxyCache.end()) return it->second;
		auto inserted = gBoxLODProxyCache.emplace(obj, BuildBoxLODProxyInstances(obj, mesh, model, world, fallbackOBB, materials));
		return inserted.first->second;
	}

	void RenderBoxLODMeshInstance(ID3D12GraphicsCommandList* cmd, const BoxLODMeshInstance& meshInstance)
	{
		if (meshInstance.drawMesh == nullptr || meshInstance.drawMesh->type != Mesh::MeshType::_BumpMesh || meshInstance.materials == nullptr) return;
		BumpMesh* bumpMesh = static_cast<BumpMesh*>(meshInstance.drawMesh);
		matrix meshWorld = meshInstance.world;
		meshWorld.transpose();
		cmd->SetGraphicsRoot32BitConstants(1, 16, &meshWorld, 0);
		using PBRRPI = PBRShader1::RootParamId;
		for (int i = 0; i < bumpMesh->subMeshNum; ++i) {
			const int materialIndex = meshInstance.materials[i];
			if (!IsValidMaterialIndex(materialIndex)) continue;
			Material* mat = game.MaterialTable[materialIndex];
			cmd->SetGraphicsRootDescriptorTable(PBRRPI::CBVTable_Material, mat->CB_Resource.descindex.hRender.hgpu);
			cmd->SetGraphicsRootDescriptorTable(PBRRPI::SRVTable_MaterialTextures, mat->TextureSRVTableIndex.hRender.hgpu);
			bumpMesh->Render(cmd, 1, i);
		}
	}

	void PushBoxLODMeshInstance(const BoxLODMeshInstance& meshInstance)
	{
		if (meshInstance.drawMesh == nullptr || meshInstance.drawMesh->type != Mesh::MeshType::_BumpMesh || meshInstance.materials == nullptr) return;
		BumpMesh* bumpMesh = static_cast<BumpMesh*>(meshInstance.drawMesh);
		matrix meshWorld = meshInstance.world;
		meshWorld.transpose();
		for (int i = 0; i < bumpMesh->subMeshNum; ++i) {
			const int materialIndex = meshInstance.materials[i];
			if (!IsValidMaterialIndex(materialIndex)) continue;
			bumpMesh->InstanceData[i].PushInstance(RenderInstanceData(meshWorld, materialIndex));
		}
	}
	bool IsFloorLikeOBB(const BoundingOrientedBox& obb)
	{
		const float width = max(obb.Extents.x, obb.Extents.z);
		const float thin = obb.Extents.y;
		return (width >= 20.0f && thin <= 1.5f);
	}

	float ComputeScreenRadius(const BoundingOrientedBox& obb)
	{
		vec4 delta = vec4(obb.Center.x, obb.Center.y, obb.Center.z, 0.0f) - game.renderViewPort->Camera_Pos;
		delta.w = 0;
		const float distance = max(delta.getlen3(), 1.0f);
		const float radius = sqrtf(obb.Extents.x * obb.Extents.x + obb.Extents.y * obb.Extents.y + obb.Extents.z * obb.Extents.z);
		const float viewportHeight = (game.renderViewPort->Viewport.Height > 1.0f) ? game.renderViewPort->Viewport.Height : 720.0f;
		return (radius / distance) * viewportHeight;
	}

	bool ShouldUseScreenSizeLODByKey(const void* key, const BoundingOrientedBox& obb, float enterScreenRadius, float leaveScreenRadius, std::unordered_map<const void*, bool>& states)
	{
		if (!AutoLOD_IsModelLODRenderActive() || game.renderViewPort == nullptr) return false;
		if (obb.Extents.x <= 0 || obb.Extents.y <= 0 || obb.Extents.z <= 0) return false;
		if (IsFloorLikeOBB(obb)) return false;

		const float screenRadius = ComputeScreenRadius(obb);
		bool wasUsingLOD = false;
		auto it = states.find(key);
		if (it != states.end()) {
			wasUsingLOD = it->second;
		}

		bool useLOD = wasUsingLOD ? (screenRadius <= leaveScreenRadius) : (screenRadius <= enterScreenRadius);
		states[key] = useLOD;

		return useLOD;
	}

	bool ShouldUseMeshLODByKey(const void* key, const BoundingOrientedBox& obb)
	{
		if (!kEnableMeshLODRender) return false;
		return ShouldUseScreenSizeLODByKey(key, obb, kMeshLODEnterScreenRadius, kMeshLODLeaveScreenRadius, gMeshLODState);
	}

	int GetMeshLODLevelForOBB(const BoundingOrientedBox& obb)
	{
		if (game.renderViewPort == nullptr) return 0;
		return 0;
	}

	bool ShouldUseBoxLODByKey(const void* key, const BoundingOrientedBox& obb)
	{
		if (!gEnableBoxLOD) return false;
		return ShouldUseScreenSizeLODByKey(key, obb, kBoxLODEnterScreenRadius, kBoxLODLeaveScreenRadius, gBoxLODState);
	}
}
bool BoxLOD_ShouldUseForKey(const void* key, const BoundingOrientedBox& obb)
{
	return ShouldUseBoxLODByKey(key, obb);
}

void BoxLOD_QueueOBB(const BoundingOrientedBox& obb)
{
	(void)obb;
}

void BoxLOD_ClearQueue()
{
}

void BoxLOD_BeginFrame()
{
	AutoLOD_BeginFrame();
}

void BoxLOD_FlushQueued()
{
}

void BoxLOD_DebugUpdate(float deltaTime)
{
	AutoLOD_DebugUpdate(deltaTime);
}

void GameObject::StaticInit()
{
	StaticGameObject sgo;
	Vptr<StaticGameObject> = *(void**)&sgo;
	DynamicGameObject dgo;
	Vptr<DynamicGameObject> = *(void**)&dgo;
	SkinMeshGameObject smgo;
	Vptr<SkinMeshGameObject> = *(void**)&smgo;
}

void GameObject::STATICINIT(int typeindex) {
	for (int i = 0; i < GameObject::g_member.size();++i) {
		g_member[i].offset = g_member[i].get_offset();
		MemberInfo& mi = *(MemberInfo*)&g_member[i];
		int lastindex = GameObjectType::Client_STCMembers[typeindex].size();
		GameObjectType::Client_STCMembers[typeindex].emplace_back(mi.name, mi.get_offset, mi.size);
		GameObjectType::Client_STCMembers[typeindex][lastindex].offset = g_member[i].offset;
	}
}

GameObject::GameObject()
{
	tag = 0;
	tag[GameObjectTag::Tag_Enable] = false;
	tag[GameObjectTag::Tag_Dynamic] = false;
	TourID = 0;
	shape.SetMesh(nullptr);
	parent = nullptr;
	childs = nullptr;
	sibling = nullptr;
	worldMat.Id();
}

GameObject::~GameObject()
{
}

matrix GameObject::GetWorld() {
	return worldMat;
}

void GameObject::SetWorld(matrix localWorldMat)
{
	worldMat = localWorldMat;
}

void GameObject::Render(matrix parent)
{
	Mesh* mesh = nullptr;
	Model* model = nullptr;
	BoundingOrientedBox obb_local, obb;
	bool b;
	matrix world = GetWorld();
	world *= parent;

	shape.GetRealShape(mesh, model);
	if (mesh != nullptr) obb_local = mesh->GetOBB();
	else if (model != nullptr) obb_local = model->GetOBB();
	else return;

	obb_local.Transform(obb, world);
	/*b = (game.renderViewPort->*game.renderViewPort->FrustumIntersectFunc)(obb);
	if (b == false) return;*/
	const bool allowAutoLOD = (dynamic_cast<StaticGameObject*>(this) != nullptr) && !IsFragileLODStructureModel(model);
	const bool useMeshLOD = allowAutoLOD && ShouldUseMeshLODByKey(this, obb);
	const int meshLODLevel = useMeshLOD ? GetMeshLODLevelForOBB(obb) : 0;
	const bool useBoxLOD = allowAutoLOD && ShouldUseBoxLODByKey(this, obb);

	if (useBoxLOD) {
		BumpMesh* proxyMesh = GetBoxLODProxyMesh();
		if (proxyMesh != nullptr) {
			const BoxLODHybridProxy& proxy = GetCachedBoxLODProxyInstances(this, mesh, model, world, obb, material);
			if (!proxy.boxes.empty() || !proxy.meshes.empty()) {
				size_t sourceTriangles = 0;
				size_t renderedTriangles = 0;
				if (mesh != nullptr) AccumulateSourceTrianglesForMesh(mesh, sourceTriangles);
				else AccumulateSourceTrianglesForModel(model, sourceTriangles);
				AccumulateRenderedTrianglesForProxy(proxy, renderedTriangles);
				AutoLOD_RecordFrameSelection(true, true, sourceTriangles, renderedTriangles);
				using PBRRPI = PBRShader1::RootParamId;
				for (const BoxLODProxyInstance& proxyInstance : proxy.boxes) {
					if (proxyInstance.obb.Extents.x <= 0 || proxyInstance.obb.Extents.y <= 0 || proxyInstance.obb.Extents.z <= 0) continue;
					if (!IsValidMaterialIndex(proxyInstance.materialIndex)) continue;
					Material* mat = game.MaterialTable[proxyInstance.materialIndex];
					gd.gpucmd->SetGraphicsRootDescriptorTable(PBRRPI::CBVTable_Material, mat->CB_Resource.descindex.hRender.hgpu);
					gd.gpucmd->SetGraphicsRootDescriptorTable(PBRRPI::SRVTable_MaterialTextures, mat->TextureSRVTableIndex.hRender.hgpu);
					matrix proxyWorld = MakeBoxLODWorldMatrix(proxyInstance.obb);
					proxyWorld.transpose();
					gd.gpucmd->SetGraphicsRoot32BitConstants(1, 16, &proxyWorld, 0);
					proxyMesh->Render(gd.gpucmd, 1, 0);
				}
				return;
			}
		}
	}
	if (false && useMeshLOD) {
		const BoxLODHybridProxy& proxy = GetCachedBoxLODProxyInstances(this, mesh, model, world, obb, material);
		if (!proxy.meshes.empty()) {
			size_t sourceTriangles = 0;
			size_t renderedTriangles = 0;
			AccumulateLODStatsForProxy(proxy, sourceTriangles, renderedTriangles);
			AutoLOD_RecordFrameSelection(true, true, sourceTriangles, renderedTriangles);
			BumpMesh* proxyMesh = GetBoxLODProxyMesh();
			using PBRRPI = PBRShader1::RootParamId;
			if (proxyMesh != nullptr) {
				for (const BoxLODProxyInstance& proxyInstance : proxy.boxes) {
					if (proxyInstance.obb.Extents.x <= 0 || proxyInstance.obb.Extents.y <= 0 || proxyInstance.obb.Extents.z <= 0) continue;
					if (!IsValidMaterialIndex(proxyInstance.materialIndex)) continue;
					Material* mat = game.MaterialTable[proxyInstance.materialIndex];
					gd.gpucmd->SetGraphicsRootDescriptorTable(PBRRPI::CBVTable_Material, mat->CB_Resource.descindex.hRender.hgpu);
					gd.gpucmd->SetGraphicsRootDescriptorTable(PBRRPI::SRVTable_MaterialTextures, mat->TextureSRVTableIndex.hRender.hgpu);
					matrix proxyWorld = MakeBoxLODWorldMatrix(proxyInstance.obb);
					proxyWorld.transpose();
					gd.gpucmd->SetGraphicsRoot32BitConstants(1, 16, &proxyWorld, 0);
					proxyMesh->Render(gd.gpucmd, 1, 0);
				}
			}
			for (const BoxLODMeshInstance& meshInstance : proxy.meshes) {
				RenderBoxLODMeshInstance(gd.gpucmd, meshInstance);
			}
			return;
		}
	}
	if (mesh != nullptr) {
		Mesh* drawMesh = mesh;
		bool resolvedLOD = false;
		if (useMeshLOD) {
			if (Mesh* lodMesh = GetEffectiveLODMesh(mesh, meshLODLevel)) {
				drawMesh = lodMesh;
				resolvedLOD = true;
			}
		}
		if (allowAutoLOD) {
			size_t sourceTriangles = GetMeshTriangleCount(mesh);
			size_t renderedTriangles = 0;
			size_t sourceDraws = GetMeshDrawCount(mesh);
			size_t renderedDraws = 0;
			if (resolvedLOD) {
				renderedTriangles = GetMeshTriangleCount(drawMesh);
				renderedDraws = GetMeshDrawCount(drawMesh);
			}
			AutoLOD_RecordFrameSelection(useMeshLOD, resolvedLOD, sourceTriangles, renderedTriangles, 0, sourceDraws, renderedDraws);
			if (useMeshLOD && !resolvedLOD) {
				AutoLOD_RecordFrameMiss(mesh, sourceTriangles);
			}
		}

		matrix rootWorld = world;
		rootWorld.transpose();
		BumpMesh* Bmesh = (BumpMesh*)drawMesh;
		if (material == nullptr) return;
		if (Bmesh->IsAutoLODGenerated &&
			game.QueueAutoLODInstance(Bmesh, rootWorld, material, mesh->subMeshNum)) {
			return;
		}
		gd.gpucmd->SetGraphicsRoot32BitConstants(1, 16, &rootWorld, 0);
		for (int i = 0; i < Bmesh->subMeshNum; ++i) {
			if (material[i] < 0 || material[i] >= game.RenderMaterialTable.size()) continue;
			Material* Mat = game.GetMaterialFromRenderMaterialIndex(material[i]);
			if (Mat == nullptr || Mat->CB_Resource.resource == nullptr || Mat->TextureSRVTableIndex.hRender.hgpu.ptr == 0) continue;
			
			//if (Mat == nullptr) continue;
			//if (Mat->CB_Resource.descindex.hRender.hgpu.ptr == 0 || Mat->TextureSRVTableIndex.hRender.hgpu.ptr == 0) {
			//	Mat->SetDescTable();
			//}
			//if (Mat->CB_Resource.descindex.hRender.hgpu.ptr == 0 || Mat->TextureSRVTableIndex.hRender.hgpu.ptr == 0) {
			//	continue;
			//}

			using PBRRPI = PBRShader1::RootParamId;
			gd.gpucmd->SetGraphicsRootDescriptorTable(PBRRPI::CBVTable_Material, Mat->CB_Resource.descindex.hRender.hgpu);
			gd.gpucmd->SetGraphicsRootDescriptorTable(PBRRPI::SRVTable_MaterialTextures, Mat->TextureSRVTableIndex.hRender.hgpu);
			drawMesh->Render(gd.gpucmd, 1, i);
		}
	}
	else {
		bool resolvedLOD = false;
		if (useMeshLOD) {
			for (unsigned int i = 0; i < model->mNumMeshes; ++i) {
				if (GetEffectiveLODMesh(model->mMeshes[i], meshLODLevel) != nullptr) {
					resolvedLOD = true;
					break;
				}
			}
		}
		if (allowAutoLOD) {
			size_t sourceTriangles = 0;
			size_t renderedTriangles = 0;
			size_t sourceDraws = 0;
			size_t renderedDraws = 0;
			AccumulateSourceTrianglesForModel(model, sourceTriangles);
			AccumulateSourceDrawsForModel(model, sourceDraws);
			size_t resolvedSourceTriangles = 0;
			size_t resolvedSourceDraws = 0;
			if (resolvedLOD) {
				AccumulateLODStatsForModel(model, resolvedSourceTriangles, renderedTriangles);
				AccumulateLODDrawStatsForModel(model, resolvedSourceDraws, renderedDraws);
			}
			AutoLOD_RecordFrameSelection(useMeshLOD, resolvedLOD, resolvedSourceTriangles, renderedTriangles, sourceTriangles, resolvedSourceDraws, renderedDraws);
			if (useMeshLOD) {
				RecordLODFrameMissesForModel(model, meshLODLevel);
			}
		}
		const bool previousLODRenderActive = AutoLOD_IsModelLODRenderActive();
		const int previousLODRenderLevel = AutoLOD_GetModelLODRenderLevel();
		AutoLOD_SetModelLODRenderActive(useMeshLOD && resolvedLOD);
		AutoLOD_SetModelLODRenderLevel(meshLODLevel);
		model->Render(gd.gpucmd, world, this);
		AutoLOD_SetModelLODRenderLevel(previousLODRenderLevel);
		AutoLOD_SetModelLODRenderActive(previousLODRenderActive);
	}
}
void GameObject::PushRenderBatch(matrix parent)
{
	Mesh* mesh = nullptr;
	Model* model = nullptr;
	BoundingOrientedBox obb_local, obb;
	bool b;
	matrix world = GetWorld();
	world *= parent;

	shape.GetRealShape(mesh, model);
	if (mesh != nullptr) obb_local = mesh->GetOBB();
	else if (model != nullptr) obb_local = model->GetOBB();
	else return;

	obb_local.Transform(obb, world);
	/*b = (game.renderViewPort->*game.renderViewPort->FrustumIntersectFunc)(obb);
	if (b == false) return;*/
	const bool allowAutoLOD = (dynamic_cast<StaticGameObject*>(this) != nullptr) && !IsFragileLODStructureModel(model);
	const bool useMeshLOD = allowAutoLOD && ShouldUseMeshLODByKey(this, obb);
	const int meshLODLevel = useMeshLOD ? GetMeshLODLevelForOBB(obb) : 0;
	const bool useBoxLOD = allowAutoLOD && ShouldUseBoxLODByKey(this, obb);

	if (useBoxLOD) {
		BumpMesh* proxyMesh = GetBoxLODProxyMesh();
		if (proxyMesh != nullptr) {
			const BoxLODHybridProxy& proxy = GetCachedBoxLODProxyInstances(this, mesh, model, world, obb, material);
			if (!proxy.boxes.empty() || !proxy.meshes.empty()) {
				size_t sourceTriangles = 0;
				size_t renderedTriangles = 0;
				if (mesh != nullptr) AccumulateSourceTrianglesForMesh(mesh, sourceTriangles);
				else AccumulateSourceTrianglesForModel(model, sourceTriangles);
				AccumulateRenderedTrianglesForProxy(proxy, renderedTriangles);
				AutoLOD_RecordFrameSelection(true, true, sourceTriangles, renderedTriangles);
				for (const BoxLODProxyInstance& proxyInstance : proxy.boxes) {
					if (proxyInstance.obb.Extents.x <= 0 || proxyInstance.obb.Extents.y <= 0 || proxyInstance.obb.Extents.z <= 0) continue;
					if (!IsValidMaterialIndex(proxyInstance.materialIndex)) continue;
					matrix proxyWorld = MakeBoxLODWorldMatrix(proxyInstance.obb);
					proxyWorld.transpose();
					proxyMesh->InstanceData[0].PushInstance(RenderInstanceData(proxyWorld, proxyInstance.materialIndex));
				}
				return;
			}
		}
	}
	if (false && useMeshLOD) {
		const BoxLODHybridProxy& proxy = GetCachedBoxLODProxyInstances(this, mesh, model, world, obb, material);
		if (!proxy.meshes.empty()) {
			size_t sourceTriangles = 0;
			size_t renderedTriangles = 0;
			AccumulateLODStatsForProxy(proxy, sourceTriangles, renderedTriangles);
			AutoLOD_RecordFrameSelection(true, true, sourceTriangles, renderedTriangles);
			BumpMesh* proxyMesh = GetBoxLODProxyMesh();
			if (proxyMesh != nullptr) {
				for (const BoxLODProxyInstance& proxyInstance : proxy.boxes) {
					if (proxyInstance.obb.Extents.x <= 0 || proxyInstance.obb.Extents.y <= 0 || proxyInstance.obb.Extents.z <= 0) continue;
					if (!IsValidMaterialIndex(proxyInstance.materialIndex)) continue;
					matrix proxyWorld = MakeBoxLODWorldMatrix(proxyInstance.obb);
					proxyWorld.transpose();
					proxyMesh->InstanceData[0].PushInstance(RenderInstanceData(proxyWorld, proxyInstance.materialIndex));
				}
			}
			for (const BoxLODMeshInstance& meshInstance : proxy.meshes) {
				PushBoxLODMeshInstance(meshInstance);
			}
			return;
		}
	}
	if (mesh != nullptr) {
		Mesh* drawMesh = mesh;
		bool resolvedLOD = false;
		if (useMeshLOD) {
			if (Mesh* lodMesh = GetEffectiveLODMesh(mesh, meshLODLevel)) {
				drawMesh = lodMesh;
				resolvedLOD = true;
			}
		}
		if (allowAutoLOD) {
			size_t sourceTriangles = GetMeshTriangleCount(mesh);
			size_t renderedTriangles = 0;
			size_t sourceDraws = GetMeshDrawCount(mesh);
			size_t renderedDraws = 0;
			if (resolvedLOD) {
				renderedTriangles = GetMeshTriangleCount(drawMesh);
				renderedDraws = GetMeshDrawCount(drawMesh);
			}
			AutoLOD_RecordFrameSelection(useMeshLOD, resolvedLOD, sourceTriangles, renderedTriangles, 0, sourceDraws, renderedDraws);
			if (useMeshLOD && !resolvedLOD) {
				AutoLOD_RecordFrameMiss(mesh, sourceTriangles);
			}
		}

		matrix rootWorld = world;
		rootWorld.transpose();
		BumpMesh* Bmesh = (BumpMesh*)drawMesh;
		for (int i = 0; i < Bmesh->subMeshNum; ++i) {
			if (material[i] < 0 || material[i] >= static_cast<int>(game.RenderMaterialTable.size())) continue;
			if (Bmesh->InstanceData == nullptr) continue;
			Bmesh->InstanceData[i].PushInstance(RenderInstanceData(rootWorld, material[i]));
		}
	}
	else {
		bool resolvedLOD = false;
		if (useMeshLOD) {
			for (unsigned int i = 0; i < model->mNumMeshes; ++i) {
				if (GetEffectiveLODMesh(model->mMeshes[i], meshLODLevel) != nullptr) {
					resolvedLOD = true;
					break;
				}
			}
		}
		if (allowAutoLOD && resolvedLOD) {
			size_t sourceDraws = 0;
			size_t renderedDraws = 0;
			AccumulateLODDrawStatsForModel(model, sourceDraws, renderedDraws);
			AutoLOD_RecordFrameDraws(sourceDraws, renderedDraws);
		}
		const bool previousLODRenderActive = AutoLOD_IsModelLODRenderActive();
		const int previousLODRenderLevel = AutoLOD_GetModelLODRenderLevel();
		AutoLOD_SetModelLODRenderActive(useMeshLOD && resolvedLOD);
		AutoLOD_SetModelLODRenderLevel(meshLODLevel);
		model->PushRenderBatch(world, this);
		AutoLOD_SetModelLODRenderLevel(previousLODRenderLevel);
		AutoLOD_SetModelLODRenderActive(previousLODRenderActive);
	}
}
void GameObject::Release()
{
	//Raytracing Release
	Mesh* mesh = nullptr;
	Model* model = nullptr;
	shape.GetRealShape(mesh, model);
	if (mesh) {
		int TLASindex = ((char*)this->RaytracingWorldMatInput - (char*)game.MyRayTracingShader->TLAS_InstanceDescs_MappedData) / sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
		if (0 <= TLASindex && TLASindex < game.MyRayTracingShader->TLASAlloter.size) {
			ZeroMemory(&game.MyRayTracingShader->TLAS_InstanceDescs_MappedData[TLASindex], sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
			game.MyRayTracingShader->TLAS_InstanceDescs_MappedData[TLASindex].AccelerationStructure = game.RaytracingTLASBlank->rmesh.MeshDefaultInstanceData.AccelerationStructure;
			game.MyRayTracingShader->TLASAlloter.Free(TLASindex);
		}
	}
	else if (model) {
		for (int i = 0; i < model->nodeCount; ++i) {
			if (this->RaytracingWorldMatInput_Model[i] != nullptr) {
				float* ptr = this->RaytracingWorldMatInput_Model[i];

				int TLASindex = ((char*)ptr - (char*)game.MyRayTracingShader->TLAS_InstanceDescs_MappedData) / sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
				if (0 <= TLASindex && TLASindex < game.MyRayTracingShader->TLASAlloter.size) {
					ZeroMemory(&game.MyRayTracingShader->TLAS_InstanceDescs_MappedData[TLASindex], sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
					game.MyRayTracingShader->TLAS_InstanceDescs_MappedData[TLASindex].AccelerationStructure = game.RaytracingTLASBlank->rmesh.MeshDefaultInstanceData.AccelerationStructure;
					game.MyRayTracingShader->TLASAlloter.Free(TLASindex);
				}
			}
		}
	}

	tag[GameObjectTag::Tag_Enable] = false;
	gMeshLODState.erase((const void*)this);
	gBoxLODState.erase((const void*)this);
	gBoxLODProxyCache.erase(this);
	if (transforms_innerModel) {
		delete[] transforms_innerModel;
	}
}

void GameObject::SetRaytracingInstanceEnabled(bool enabled)
{
	if (!gd.isSupportRaytracing || game.MyRayTracingShader == nullptr ||
		game.MyRayTracingShader->TLAS_InstanceDescs_MappedData == nullptr) {
		return;
	}

	auto setInstanceMask = [&](float* transformPtr) {
		if (transformPtr == nullptr) return;
		const uintptr_t base = reinterpret_cast<uintptr_t>(game.MyRayTracingShader->TLAS_InstanceDescs_MappedData);
		const uintptr_t ptr = reinterpret_cast<uintptr_t>(transformPtr);
		if (ptr < base) return;
		const uintptr_t byteOffset = ptr - base;
		if (byteOffset % sizeof(D3D12_RAYTRACING_INSTANCE_DESC) != 0) return;
		const uintptr_t index = byteOffset / sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
		if (index >= static_cast<uintptr_t>(game.MyRayTracingShader->TLASAlloter.size)) return;
		game.MyRayTracingShader->TLAS_InstanceDescs_MappedData[index].InstanceMask = enabled ? 1 : 0;
	};

	Mesh* mesh = nullptr;
	Model* model = nullptr;
	shape.GetRealShape(mesh, model);
	if (mesh != nullptr) {
		setInstanceMask(RaytracingWorldMatInput);
	}
	else if (model != nullptr && RaytracingWorldMatInput_Model != nullptr) {
		for (int i = 0; i < model->nodeCount; ++i) {
			setInstanceMask(RaytracingWorldMatInput_Model[i]);
		}
	}
}

BoundingOrientedBox GameObject::GetOBB()
{
	BoundingOrientedBox obb_local;
	obb_local.Extents.x = -1;
	Mesh* mesh = nullptr;
	Model* model = nullptr;
	shape.GetRealShape(mesh, model);
	if (mesh != nullptr) obb_local = mesh->GetOBB();
	if (model != nullptr) obb_local = model->GetOBB();
	BoundingOrientedBox obb_world;
	obb_local.Transform(obb_world, worldMat);
	return obb_world;
}

void GameObject::SetShape(Shape _shape, int ZoneID)
{
	static LocalRootSigData tempLRSSaver[1024] = {};
	shape = _shape;
	Mesh* mesh = nullptr;
	Model* model = nullptr;
	shape.GetRealShape(mesh, model);
	if (model != nullptr) {
		if (transforms_innerModel == nullptr) {
			transforms_innerModel = new matrix[model->nodeCount];
			for (int i = 0; i < model->nodeCount; ++i) {
				transforms_innerModel[i] = model->Nodes[i].transform;
			}
		}

		if (gd.isSupportRaytracing) {
			RaytracingWorldMatInput_Model = new float* [model->nodeCount];
			for (int i = 0; i < model->nodeCount; ++i) {
				RaytracingWorldMatInput_Model[i] = nullptr;
				if (model->Nodes[i].numMesh > 0) {
					BumpMesh* bmesh = (BumpMesh*)model->mMeshes[model->Nodes[i].Meshes[0]];
					int matindex = model->Nodes[i].materialIndex[0]; // game.GetRenderMaterialIndexFromGlobalMaterialIndex(model->Nodes[i].materialIndex[0]);
					tempLRSSaver[0] = LocalRootSigData(bmesh->rmesh.VBStartOffset / sizeof(RayTracingMesh::Vertex), bmesh->rmesh.IBStartOffset[0] / sizeof(unsigned int), matindex, ZoneID);

					float** WorldMatInputs = game.MyRayTracingShader->push_rins_immortal(&bmesh->rmesh, worldMat, tempLRSSaver);
					/*RaytracingWorldMatInput_Model[i] = new float* [bmesh->subMeshNum];
					for (int k = 0; k < bmesh->subMeshNum; ++k) {
						RaytracingWorldMatInput_Model[i][k] = WorldMatInputs[k];
					}*/
					RaytracingWorldMatInput_Model[i] = WorldMatInputs[0];
				}
			}
			RaytracingUpdateTransform();
		}
	}
	if (mesh != nullptr) {
		if (material == nullptr) {
			material = new int[mesh->subMeshNum];
			for (int i = 0; i < mesh->subMeshNum; ++i) {
				material[i] = 0;
			}
		}

		if (gd.isSupportRaytracing) {
			BumpMesh* bmesh = (BumpMesh*)mesh;
			/*for (int i = 0; i < bmesh->subMeshNum; ++i) {
				tempLRSSaver[i] = LocalRootSigData(bmesh->rmesh.VBStartOffset / sizeof(RayTracingMesh::Vertex), bmesh->rmesh.IBStartOffset[i] / sizeof(unsigned int));
			}*/

			int matindex = material[0];// game.GetRenderMaterialIndexFromGlobalMaterialIndex(material[0]);
			tempLRSSaver[0] = LocalRootSigData(bmesh->rmesh.VBStartOffset / sizeof(RayTracingMesh::Vertex), bmesh->rmesh.IBStartOffset[0] / sizeof(unsigned int), matindex, ZoneID);

			float** WorldMatInputs = game.MyRayTracingShader->push_rins_immortal(&bmesh->rmesh, worldMat, tempLRSSaver);
			/*RaytracingWorldMatInput = new float* [bmesh->subMeshNum];
			for (int i = 0; i < bmesh->subMeshNum; ++i) {
				RaytracingWorldMatInput[i] = WorldMatInputs[i];
			}*/
			RaytracingWorldMatInput = WorldMatInputs[0];

			RaytracingUpdateTransform();
		}
	}
}

void GameObject::SetShape(Model* _shape, int ZoneID)
{
	SetShape(Shape(_shape), ZoneID);
}

void GameObject::SetShape(Mesh* _shape, int ZoneID)
{
	SetShape(Shape(_shape), ZoneID);
}

void GameObject::DbgHieraky()
{

}

void GameObject::OnRayHit(GameObject* rayFrom) {

}

void GameObject::RaytracingUpdateTransform()
{
	Mesh* mesh = nullptr;
	Model* model = nullptr;
	shape.GetRealShape(mesh, model);
	if (RaytracingWorldMatInput) {
		if (mesh != nullptr) {
			matrix mat = worldMat;
			mat.transpose();
			/*for (int i = 0; i < mesh->subMeshNum; ++i) {
				memcpy(RaytracingWorldMatInput[i], &mat, sizeof(float) * 12);
			}*/
			memcpy(RaytracingWorldMatInput, &mat, sizeof(float) * 12);
		}
		else {
			RaytracingUpdateTransform(model, model->RootNode, worldMat);
		}
	}
}

void GameObject::RaytracingUpdateTransform(Model* model, ModelNode* node, matrix parent)
{
	int nodeIndex = ((char*)node - (char*)model->Nodes) / sizeof(ModelNode);
	matrix mat = transforms_innerModel[nodeIndex];
	mat *= parent;

	for (int i = 0; i < node->numChildren; ++i) {
		RaytracingUpdateTransform(model, node->Childrens[i], mat);
	}

	if (RaytracingWorldMatInput_Model[nodeIndex] != nullptr) {
		mat.transpose();
		/*for (int i = 0; i < model->mMeshes[node->Meshes[0]]->subMeshNum; ++i) {
			memcpy(RaytracingWorldMatInput_Model[nodeIndex][i], &mat, sizeof(float) * 12);
		}*/
		memcpy(RaytracingWorldMatInput_Model[nodeIndex], &mat, sizeof(float) * 12);
	}
}

void GameObject::RecvSTC_SyncObj(char* data) {
	int offset = 0;
	STC_SyncObjData& stcsod = *(STC_SyncObjData*)(data);
	if (stcsod.shapeindex >= 0 && stcsod.shapeindex < (int)Shape::ShapeTable.size()) shape = Shape::ShapeTable[stcsod.shapeindex]; else shape.FlagPtr = 0;   // [crash-fix] out-of-range shapeindex -> null shape (avoids garbage Model pointer)
	tag = stcsod.tag;
	parent = (stcsod.parent >= 0) ? game.StaticGameObjects[stcsod.parent] : nullptr;
	childs = (stcsod.childs >= 0) ? game.StaticGameObjects[stcsod.childs] : nullptr;
	sibling = (stcsod.sibling >= 0) ? game.StaticGameObjects[stcsod.sibling] : nullptr;
	worldMat = stcsod.worldMatrix;
	offset += sizeof(STC_SyncObjData);

	Mesh* mesh = nullptr;
	Model* model = nullptr;
	shape.GetRealShape(mesh, model);
	if (mesh) {
		int& materialCount = *(int*)(data + offset);
		offset += sizeof(int);
		if (material != nullptr) {
			delete[] material;
			material = nullptr;
		}
		material = new int[materialCount];
		for (int i = 0;i < materialCount;++i) {
			material[i] = *(int*)(data + offset);
			offset += sizeof(int);
		}
	}
	else {
		int& NodeCount = *(int*)(data + offset);
		offset += sizeof(int);
		if (transforms_innerModel != nullptr) {
			delete[] transforms_innerModel;
			transforms_innerModel = nullptr;
		}
		transforms_innerModel = new matrix[NodeCount];
		for (int i = 0;i < NodeCount;++i) {
			transforms_innerModel[i] = *(matrix*)(data + offset);
			offset += sizeof(matrix);
		}
	}
}

void StaticGameObject::STATICINIT(int typeindex) {
	for (int i = 0; i < GameObject::g_member.size();++i) {
		g_member[i].offset = g_member[i].get_offset();
		MemberInfo& mi = *(MemberInfo*)&g_member[i];
		int lastindex = GameObjectType::Client_STCMembers[typeindex].size();
		GameObjectType::Client_STCMembers[typeindex].emplace_back(mi.name, mi.get_offset, mi.size);
		GameObjectType::Client_STCMembers[typeindex][lastindex].offset = g_member[i].offset;
	}
}

StaticGameObject::StaticGameObject()
{
	tag[GameObjectTag::Tag_Enable] = false;
	tag[GameObjectTag::Tag_Dynamic] = false;
}

StaticGameObject::~StaticGameObject()
{
}

matrix StaticGameObject::GetWorld() {
	return worldMat;
}

void StaticGameObject::SetWorld(matrix localWorldMat)
{
	matrix sav = localWorldMat;
	GameObject* obj = parent;
	if (obj != nullptr) {
		sav *= obj->worldMat;
	}
	worldMat = sav;
}

void StaticGameObject::Render(matrix parent)
{
	GameObject::Render(parent);
	//XMMATRIX sav = XMMatrixMultiply(worldMat, parent);
	//BoundingOrientedBox obb = GetOBBw(sav);
	//bool b = (game.renderViewPort->*game.renderViewPort->FrustumIntersectFunc)(obb);
	//if (obb.Extents.x > 0 && b) {
	//	Mesh* mesh = nullptr;
	//	Model* model = nullptr;
	//	shape.GetRealShape(mesh, model);
	//	if (model != nullptr) {
	//		model->Render(gd.gpucmd, sav);
	//	}
	//	else if (mesh != nullptr) {
	//		matrix m = sav;
	//		m.transpose();

	//		//set world matrix in shader
	//		gd.gpucmd->SetGraphicsRoot32BitConstants(1, 16, &m, 0);
	//		//set texture in shader
	//		//game.MyPBRShader1->SetTextureCommand()
	//		int materialIndex = Mesh_materialIndex;
	//		Material& mat = game.MaterialTable[materialIndex];

	//		gd.gpucmd->SetGraphicsRootDescriptorTable(3, mat.hGPU);
	//		gd.gpucmd->SetGraphicsRootDescriptorTable(4, mat.CB_Resource.hGpu);
	//		mesh->Render(gd.gpucmd, 1);
	//	}
	//}

	//StaticGameObject* obj = (StaticGameObject*)childs;
	//while (obj != nullptr) {
	//	obj->Render(sav);
	//	obj = (StaticGameObject*)obj->sibling;
	//}
}

void StaticGameObject::PushRenderBatch(matrix parent)
{
	GameObject::PushRenderBatch(parent);
}

bool StaticGameObject::Collision_Inherit(matrix parent_world, BoundingBox bb)
{
	XMMATRIX sav = XMMatrixMultiply(worldMat, parent_world);
	BoundingOrientedBox obb = GetOBBw(sav);
	if (obb.Extents.x > 0 && obb.Intersects(bb)) {
		return true;
	}

	StaticGameObject* obj = (StaticGameObject*)childs;
	while (obj != nullptr) {
		if (obj->Collision_Inherit(sav, bb)) return true;
		obj = (StaticGameObject*)obj->sibling;
	}

	return false;
}

void StaticGameObject::InitMapAABB_Inherit(void* origin, matrix parent_world)
{
	GameMap* map = (GameMap*)origin;

	XMMATRIX sav = XMMatrixMultiply(worldMat, parent_world);
	BoundingOrientedBox obb = GetOBBw(sav);
	map->ExtendMapAABB(obb);

	StaticGameObject* obj = (StaticGameObject*)childs;
	while (obj != nullptr) {
		obj->InitMapAABB_Inherit(origin, sav);
		obj = (StaticGameObject*)obj->sibling;
	}
}

BoundingOrientedBox StaticGameObject::GetOBB() {
	return GameObject::GetOBB();
}

BoundingOrientedBox StaticGameObject::GetOBBw(matrix worldMat)
{
	BoundingOrientedBox obb_local;
	Mesh* mesh = nullptr;
	Model* model = nullptr;
	shape.GetRealShape(mesh, model);
	if (model != nullptr) {
		obb_local = model->GetOBB();
	}
	else if (mesh != nullptr) {
		obb_local = mesh->GetOBB();
	}
	else {
		obb_local.Extents.x = -1;
		return obb_local;
	}
	BoundingOrientedBox obb_world;
	obb_local.Transform(obb_world, worldMat);
	return obb_world;
}

void StaticGameObject::SetShape(Shape _shape, int ZoneID) {
	GameObject::SetShape(_shape, zoneid);
	Mesh* mesh = nullptr;
	Model* model = nullptr;
	shape.GetRealShape(mesh, model);
	if (model) {
		obbArr.clear();
		model->RootNode->PushOBBs(model, worldMat, &obbArr, this);
	}
}

void StaticGameObject::Release() {
	obbArr.clear();
	GameObject::Release();
}

void StaticGameObject::RecvSTC_SyncObj(char* data) {
	int offset = 0;
	STC_SyncObjData& stcsod = *(STC_SyncObjData*)(data);
	if (stcsod.shapeindex >= 0 && stcsod.shapeindex < (int)Shape::ShapeTable.size()) shape = Shape::ShapeTable[stcsod.shapeindex]; else shape.FlagPtr = 0;   // [crash-fix] out-of-range shapeindex -> null shape (avoids garbage Model pointer)
	tag = stcsod.tag;
	parent = (stcsod.parent >= 0) ? game.StaticGameObjects[stcsod.parent] : nullptr;
	childs = (stcsod.childs >= 0) ? game.StaticGameObjects[stcsod.childs] : nullptr;
	sibling = (stcsod.sibling >= 0) ? game.StaticGameObjects[stcsod.sibling] : nullptr;
	worldMat = stcsod.worldMatrix;
	offset += sizeof(STC_SyncObjData);

	Mesh* mesh = nullptr;
	Model* model = nullptr;
	shape.GetRealShape(mesh, model);
	if (mesh) {
		int& materialCount = *(int*)(data + offset);
		offset += sizeof(int);
		if (material != nullptr) {
			delete[] material;
			material = nullptr;
		}
		material = new int[materialCount];
		for (int i = 0;i < materialCount;++i) {
			material[i] = *(int*)(data + offset);
			offset += sizeof(int);
		}
	}
	else {
		int& NodeCount = *(int*)(data + offset);
		offset += sizeof(int);
		if (transforms_innerModel != nullptr) {
			delete[] transforms_innerModel;
			transforms_innerModel = nullptr;
		}
		transforms_innerModel = new matrix[NodeCount];
		for (int i = 0;i < NodeCount;++i) {
			transforms_innerModel[i] = *(matrix*)(data + offset);
			offset += sizeof(matrix);
		}
	}

	int& aabbCount = *(int*)(data + offset);
	offset += sizeof(int);
	obbArr.reserve(aabbCount);
	obbArr.resize(aabbCount);
	for (int i = 0;i < aabbCount;++i) {
		XMFLOAT3& Center = *(XMFLOAT3*)(data + offset);
		offset += sizeof(XMFLOAT3);
		XMFLOAT3& Extents = *(XMFLOAT3*)(data + offset);
		offset += sizeof(XMFLOAT3);
		XMFLOAT4& Orientation = *(XMFLOAT4*)(data + offset);
		offset += sizeof(XMFLOAT4);
		obbArr[i] = BoundingOrientedBox(Center, Extents, Orientation);
	}
}

void DynamicGameObject::STATICINIT(int typeindex) {
	GameObject::STATICINIT(typeindex);
	for (int i = 0; i < DynamicGameObject::g_member.size();++i) {
		g_member[i].offset = g_member[i].get_offset();
		MemberInfo& mi = *(MemberInfo*)&g_member[i];
		int lastindex = GameObjectType::Client_STCMembers[typeindex].size();
		GameObjectType::Client_STCMembers[typeindex].emplace_back(mi.name, mi.get_offset, mi.size);
		GameObjectType::Client_STCMembers[typeindex][lastindex].offset = g_member[i].offset;
	}
}

void DynamicGameObject::InitialChunkSetting()
{
	if (chunkAllocIndexs == nullptr) {
		BoundingOrientedBox obb = GetOBB();
		vec4 ext = obb.Extents;
		float len = ext.len3 / Zone::chunck_divide_Width;
		chunkAllocIndexsCapacity = powf(2 * ceilf(len * 0.5f), 3);
		chunkAllocIndexs = new int[chunkAllocIndexsCapacity];
	}
}

matrix DynamicGameObject::GetWorld() {
	matrix sav = worldMat;
	GameObject* obj = parent;
	while (obj != nullptr) {
		sav *= obj->worldMat;
		if (GameObject::IsType<StaticGameObject>(obj)) break;

		//dbglog1(L"obj->parent : %p\n", obj->parent);
		obj = obj->parent;
	}
	return sav;
}

void DynamicGameObject::SetWorld(matrix localWorldMat)
{
	worldMat = localWorldMat;
}

void DynamicGameObject::Move(vec4 velocity, vec4 Q)
{
	int xmax = IncludeChunks.xmin + IncludeChunks.xlen;
	int ymax = IncludeChunks.ymin + IncludeChunks.ylen;
	int zmax = IncludeChunks.zmin + IncludeChunks.zlen;
	int up = 0;
	for (int ix = IncludeChunks.xmin; ix <= xmax; ++ix) {
		for (int iy = IncludeChunks.ymin; iy <= ymax; ++iy) {
			for (int iz = IncludeChunks.zmin; iz <= zmax; ++iz) {
				GameChunk* gc = game.Current_Zone->chunck[ChunkIndex(ix, iy, iz)];
				gc->Dynamic_gameobjects.Free(chunkAllocIndexs[up]);
				up += 1;
			}
		}
	}

	//if (Mat == nullptr) continue;
	vec4 pos = worldMat.pos;
	worldMat.trQ(Q);
	worldMat.pos = pos + velocity;
	worldMat.pos.w = 1;

	//if (Mat == nullptr) continue;
	IncludeChunks = game.Current_Zone->GetChunks_Include_OBB(GetOBB());

	xmax = IncludeChunks.xmin + IncludeChunks.xlen;
	ymax = IncludeChunks.ymin + IncludeChunks.ylen;
	zmax = IncludeChunks.zmin + IncludeChunks.zlen;
	up = 0;
	for (int ix = IncludeChunks.xmin; ix <= xmax; ++ix) {
		for (int iy = IncludeChunks.ymin; iy <= ymax; ++iy) {
			for (int iz = IncludeChunks.zmin; iz <= zmax; ++iz) {
				auto c = game.Current_Zone->chunck.find(ChunkIndex(ix, iy, iz));
				GameChunk* gc;
				if (c == game.Current_Zone->chunck.end()) {
					// new game chunk
					gc = new GameChunk();
					gc->SetChunkIndex(ChunkIndex(ix, iy, iz));
					game.Current_Zone->chunck.insert(pair<ChunkIndex, GameChunk*>(ChunkIndex(ix, iy, iz), gc));
				}
				else gc = c->second;
				int allocN = gc->Dynamic_gameobjects.Alloc();
				gc->Dynamic_gameobjects[allocN] = this;
				chunkAllocIndexs[up] = allocN;
				up += 1;
			}
		}
	}
}

void DynamicGameObject::Move(vec4 velocity, vec4 Q, GameObjectIncludeChunks afterChunkInc)
{
	int xmax = IncludeChunks.xmin + IncludeChunks.xlen;
	int ymax = IncludeChunks.ymin + IncludeChunks.ylen;
	int zmax = IncludeChunks.zmin + IncludeChunks.zlen;
	int up = 0;
	for (int ix = IncludeChunks.xmin; ix <= xmax; ++ix) {
		for (int iy = IncludeChunks.ymin; iy <= ymax; ++iy) {
			for (int iz = IncludeChunks.zmin; iz <= zmax; ++iz) {
				game.Current_Zone->chunck[ChunkIndex(ix, iy, iz)]->Dynamic_gameobjects.Free(chunkAllocIndexs[up]);
				up += 1;
			}
		}
	}

	//if (Mat == nullptr) continue;
	vec4 pos = worldMat.pos;
	worldMat.trQ(Q);
	worldMat.pos = pos + velocity;
	worldMat.pos.w = 1;

	//if (Mat == nullptr) continue;
	IncludeChunks = afterChunkInc;
	xmax = IncludeChunks.xmin + IncludeChunks.xlen;
	ymax = IncludeChunks.ymin + IncludeChunks.ylen;
	zmax = IncludeChunks.zmin + IncludeChunks.zlen;
	up = 0;
	for (int ix = IncludeChunks.xmin; ix <= xmax; ++ix) {
		for (int iy = IncludeChunks.ymin; iy <= ymax; ++iy) {
			for (int iz = IncludeChunks.zmin; iz <= zmax; ++iz) {
				auto c = game.Current_Zone->chunck.find(ChunkIndex(ix, iy, iz));
				GameChunk* gc;
				if (c == game.Current_Zone->chunck.end()) {
					// new game chunk
					gc = new GameChunk();
					gc->SetChunkIndex(ChunkIndex(ix, iy, iz));
					game.Current_Zone->chunck.insert(pair<ChunkIndex, GameChunk*>(ChunkIndex(ix, iy, iz), gc));
				}
				else gc = c->second;
				int allocN = gc->Dynamic_gameobjects.Alloc();
				gc->Dynamic_gameobjects[allocN] = this;
				chunkAllocIndexs[up] = allocN;
				up += 1;
			}
		}
	}
}

void DynamicGameObject::Render(matrix parent) {
	GameObject::Render(parent);
}

void DynamicGameObject::PushRenderBatch(matrix parent)
{
	GameObject::PushRenderBatch(parent);
}

void DynamicGameObject::Event(WinEvent evt) {

}

void DynamicGameObject::Release() {
	if (chunkAllocIndexs) {
		int xmax = IncludeChunks.xmin + IncludeChunks.xlen;
		int ymax = IncludeChunks.ymin + IncludeChunks.ylen;
		int zmax = IncludeChunks.zmin + IncludeChunks.zlen;
		int up = 0;
		for (int ix = IncludeChunks.xmin; ix <= xmax; ++ix) {
			for (int iy = IncludeChunks.ymin; iy <= ymax; ++iy) {
				for (int iz = IncludeChunks.zmin; iz <= zmax; ++iz) {
					auto chunkIt = game.Current_Zone->chunck.find(ChunkIndex(ix, iy, iz));
					if (chunkIt != game.Current_Zone->chunck.end() && chunkIt->second != nullptr) {
						chunkIt->second->Dynamic_gameobjects.Free(chunkAllocIndexs[up]);
					}
					up += 1;
				}
			}
		}
		delete[] chunkAllocIndexs;
		chunkAllocIndexs = nullptr;
	}

	GameObject::Release();
}

BoundingOrientedBox DynamicGameObject::GetOBB() {
	BoundingOrientedBox obb_local;
	Mesh* mesh = nullptr;
	Model* model = nullptr;
	shape.GetRealShape(mesh, model);
	if (mesh != nullptr) obb_local = mesh->GetOBB();
	if (model != nullptr) obb_local = model->GetOBB();

	matrix sav = GetWorld();
	BoundingOrientedBox obb_world;
	obb_local.Transform(obb_world, sav);

	return obb_world;
}

void DynamicGameObject::LookAt(vec4 look, vec4 up)
{
	worldMat.LookAt(look, up);
}

//void DynamicGameObject::CollisionMove(DynamicGameObject* gbj1, DynamicGameObject* gbj2)
//{
//	constexpr float epsillon = 0.01f;
//
//	bool bi = XMColorEqual(gbj1->tickLVelocity, XMVectorZero());/*
//	bool bia = XMColorEqual(gbj1->tickAVelocity, XMVectorZero());*/
//	bool bj = XMColorEqual(gbj2->tickLVelocity, XMVectorZero());/*
//	bool bja = XMColorEqual(gbj2->tickAVelocity, XMVectorZero());*/
//
//	DynamicGameObject* movObj = nullptr;
//	DynamicGameObject* colObj = nullptr;
//	BoundingOrientedBox obb1 = gbj1->GetOBB();
//	BoundingOrientedBox obb2 = gbj2->GetOBB();
//	//float len = vec4(gbj1->worldMat.pos - gbj2->worldMat.pos).len3;
//	//float distance = vec4(obb1.Extents).len3 + vec4(obb2.Extents).len3;
//	//if (len < distance) {
//	//Collision_By_Rotations:
//	//	if (!bia && bja) {
//	//		movObj = gbj1;
//	//		colObj = gbj2;
//	//		goto Collision_byRotation_static_vs_dynamic;
//	//	}
//	//	else if (bia && !bja) {
//	//		movObj = gbj2;
//	//		colObj = gbj1;
//	//		goto Collision_byRotation_static_vs_dynamic;
//	//	}
//	//	else if (!bia && !bja) {
//	//		goto Collision_By_Move;
//	//	}
//	//	else {
//	//		goto Collision_By_Move;
//	//	}
//	//Collision_byRotation_static_vs_dynamic:
//	//	OBB_vertexVector ovv = movObj->GetOBBVertexs();
//	//	movObj->worldMat.trQ(movObj->tickAVelocity);
//	//	obb1 = movObj->GetOBB();
//	//	OBB_vertexVector ovv_later = movObj->GetOBBVertexs();
//	//	movObj->worldMat.trQinv(movObj->tickAVelocity);
//	//	matrix imat = colObj->worldMat.RTInverse;
//	//	obb2 = colObj->GetOBB();
//	//	vec4 RayPos;
//	//	vec4 RayDir;
//	//	for (int xi = 0; xi < 2; ++xi) {
//	//		for (int yi = 0; yi < 2; ++yi) {
//	//			for (int zi = 0; zi < 2; ++zi) {
//	//				ovv_later.vertex[xi][yi][zi] *= imat;
//	//				if (obb2.Contains(ovv_later.vertex[xi][yi][zi])) {
//	//					ovv.vertex[xi][yi][zi] *= imat;
//	//					RayPos = ovv.vertex[xi][yi][zi];
//	//					RayDir = ovv_later.vertex[xi][yi][zi] - ovv.vertex[xi][yi][zi];
//	//					if (fabsf(RayDir.x) > epsillon) {
//	//						float Ex = obb2.Extents.x * (RayDir.x / fabsf(RayDir.x));
//	//						float A = (Ex - RayPos.x) / RayDir.x;
//	//						vec4 colpos = vec4(RayPos.x + RayDir.x, RayDir.y * A + RayPos.y, RayDir.z * A + RayPos.z);
//	//						vec4 bound; bound.f3 = obb2.Extents;
//	//						if (colpos.is_in_bound(bound)) {
//	//							movObj->tickLVelocity.x += colpos.x - Ex;
//	//							goto Collision_By_Move;
//	//						}
//	//					}
//	//					if (fabsf(RayDir.y) > epsillon) {
//	//						float Ex = obb2.Extents.y * (RayDir.y / fabsf(RayDir.y));
//	//						float A = (Ex - RayPos.y) / RayDir.y;
//	//						vec4 colpos = vec4(RayDir.x * A + RayPos.x, RayPos.y + RayDir.y, RayDir.z * A + RayPos.z);
//	//						vec4 bound; bound.f3 = obb2.Extents;
//	//						if (colpos.is_in_bound(bound)) {
//	//							movObj->tickLVelocity.y += colpos.y - Ex;
//	//							goto Collision_By_Move;
//	//						}
//	//					}
//	//					if (fabsf(RayDir.z) > epsillon) {
//	//						float Ex = obb2.Extents.z * (RayDir.z / fabsf(RayDir.z));
//	//						float A = (Ex - RayPos.z) / RayDir.z;
//	//						vec4 colpos = vec4(RayDir.x * A + RayPos.x, RayDir.y * A + RayPos.y, RayPos.z + RayDir.z);
//	//						vec4 bound; bound.f3 = obb2.Extents;
//	//						if (colpos.is_in_bound(bound)) {
//	//							movObj->tickLVelocity.z += colpos.z - Ex;
//	//							goto Collision_By_Move;
//	//						}
//	//					}
//	//				}
//	//			}
//	//		}
//	//	}
//	//}
//
//Collision_By_Move:
//	//if (bia) {
//	//	gbj1->worldMat.trQ(gbj1->tickAVelocity);
//	//}
//	//if (bja) {
//	//	gbj2->worldMat.trQ(gbj2->tickAVelocity);
//	//}
//
//	if (!bi && bj) {
//		// i : moving GameObject
//		// j : Collision Check GameObject
//		movObj = gbj1;
//		colObj = gbj2;
//		goto Collision_By_Move_static_vs_dynamic;
//	}
//	else if (bi && !bj) {
//		// i : Collision Check GameObject
//		// j : moving GameObject
//		movObj = gbj2;
//		colObj = gbj1;
//		goto Collision_By_Move_static_vs_dynamic;
//	}
//	else if (!bi && !bj) {
//		// i : moving GameObject
//		// j : moving GameObject
//
//		float mul = 1.0f;
//		float rate = 0.5f;
//		float maxLen = XMVector3Length(gbj1->tickLVelocity).m128_f32[0];
//		float temp = XMVector3Length(gbj2->tickLVelocity).m128_f32[0];
//		if (maxLen < temp) maxLen = temp;
//
//	CMP_INTERSECT:
//		XMVECTOR v1 = mul * gbj1->tickLVelocity;
//		XMVECTOR v2 = mul * gbj2->tickLVelocity;
//		gbj1->worldMat.pos += v1;
//		obb1 = gbj1->GetOBB();
//		gbj1->worldMat.pos -= v1;
//		gbj2->worldMat.pos += v2;
//		obb2 = gbj2->GetOBB();
//		gbj2->worldMat.pos -= v2;
//		if (obb1.Intersects(obb2)) {
//			mul -= rate;
//			rate *= 0.5f;
//			if (maxLen * rate > epsillon) goto CMP_INTERSECT;
//		}
//		else {
//			mul += rate;
//			rate *= 0.5f;
//			if (maxLen * rate > epsillon) goto CMP_INTERSECT;
//		}
//
//		gbj1->tickLVelocity = v1;
//		gbj2->tickLVelocity = v2;
//
//		return;
//	}
//	else {
//		//no move
//		return;
//	}
//
//Collision_By_Move_static_vs_dynamic:
//	movObj->worldMat.pos += movObj->tickLVelocity;
//	obb1 = movObj->GetOBB();
//	movObj->worldMat.pos -= movObj->tickLVelocity;
//	obb2 = colObj->GetOBB();
//
//	if (obb1.Intersects(obb2)) {
//		movObj->OnCollision(colObj);
//		colObj->OnCollision(movObj);
//
//		XMMATRIX basemat = colObj->worldMat;
//		XMMATRIX invmat = colObj->worldMat.RTInverse;
//		invmat.r[3] = XMVectorSet(0, 0, 0, 1);
//		XMVECTOR BaseLine = XMVector3Transform(movObj->tickLVelocity, invmat);
//		movObj->tickLVelocity = XMVectorZero();
//
//		XMVECTOR MoveVector = basemat.r[0] * BaseLine.m128_f32[0];
//		movObj->tickLVelocity += MoveVector;
//		movObj->worldMat.pos += movObj->tickLVelocity;
//		obb1 = movObj->GetOBB();
//		movObj->worldMat.pos -= movObj->tickLVelocity;
//		if (obb1.Intersects(obb2)) {
//			movObj->tickLVelocity -= MoveVector;
//		}
//
//		MoveVector = basemat.r[1] * BaseLine.m128_f32[1];
//		movObj->tickLVelocity += MoveVector;
//		movObj->worldMat.pos += movObj->tickLVelocity;
//		obb1 = movObj->GetOBB();
//		movObj->worldMat.pos -= movObj->tickLVelocity;
//		if (obb1.Intersects(obb2)) {
//			movObj->tickLVelocity -= MoveVector;
//		}
//
//		MoveVector = basemat.r[2] * BaseLine.m128_f32[2];
//		movObj->tickLVelocity += MoveVector;
//		movObj->worldMat.pos += movObj->tickLVelocity;
//		obb1 = movObj->GetOBB();
//		movObj->worldMat.pos -= movObj->tickLVelocity;
//		if (obb1.Intersects(obb2)) {
//			movObj->tickLVelocity -= MoveVector;
//		}
//	}
//
//	//if (bia) {
//	//	gbj1->worldMat.trQinv(gbj1->tickAVelocity);
//	//}
//	//if (bja) {
//	//	gbj2->worldMat.trQinv(gbj2->tickAVelocity);
//	//}
//}

void DynamicGameObject::OnCollision(GameObject* other)
{
	//GameObject Collision...
}

void DynamicGameObject::OnRayHit(GameObject* rayFrom)
{
}

void DynamicGameObject::SetShape(Shape _shape, int ZoneID)
{
	GameObject::SetShape(_shape, zoneid);
}

bool godmod = true;
DynamicGameObject::DynamicGameObject() :
	chunkAllocIndexs{ nullptr }
{
	tag[GameObjectTag::Tag_Enable] = false;
	tag[GameObjectTag::Tag_Dynamic] = true;
	//tickLVelocity = vec4(0, 0, 0, 0);
	//tickAVelocity = vec4(0, 0, 0, 0);
	//LastQuerternion = vec4(0, 0, 0, 0);
	chunkAllocIndexsCapacity = 0;
	ZeroMemory(&IncludeChunks, sizeof(GameObjectIncludeChunks));

	LVelocity = 0;
	DestPos = 0;
	DestRot = 0;
	DestScale = vec4(1, 1, 1);

	chunkAllocIndexs = nullptr;
	chunkAllocIndexsCapacity = 0;
}

DynamicGameObject::~DynamicGameObject()
{
}

void DynamicGameObject::Update(float delatTime)
{
	PositionInterpolation(delatTime);
}

void DynamicGameObject::PositionInterpolation(float deltaTime)
{
	if (this->chunkAllocIndexs == nullptr) {
		int targetZoneId = zoneid;
		if (targetZoneId < 0 || targetZoneId >= (int)game.ZoneTable.size()) {
			targetZoneId = zoneId;
		}
		Zone* zone = (targetZoneId >= 0 && targetZoneId < (int)game.ZoneTable.size())
			? game.ZoneTable[targetZoneId] : game.Current_Zone;
		if (zone != nullptr) {
			zoneid = zone->zoneid;
			zone->PushGameObject(this);
		}
		return;
	}

	if (deltaTime < 0.1f) {
		GameObjectIncludeChunks goic_before = this->IncludeChunks;
		float pow = deltaTime * 10;
		vec4 flowScale, flowRot, flowPos;
		XMMatrixDecompose((XMVECTOR*)&flowScale, (XMVECTOR*)&flowRot, (XMVECTOR*)&flowPos, worldMat);
		vec4 renderScale = (1.0f - pow) * flowScale + pow * DestScale;
		vec4 renderRot = vec4::Qlerp(flowRot, DestRot, pow);
		vec4 renderPos = (1.0f - pow) * flowPos + pow * DestPos;
		matrix mat = XMMatrixScaling(renderScale.x, renderScale.y, renderScale.z) * XMMatrixRotationQuaternion(renderRot);
		mat.pos = renderPos;
		matrix temp = worldMat;
		worldMat = mat;
		BoundingOrientedBox afterobb = GetOBB();
		GameObjectIncludeChunks goic_after = game.Current_Zone->GetChunks_Include_OBB(afterobb);
		if (goic_before != goic_after) {
			worldMat = temp;
			MoveChunck(mat, goic_before, goic_after);
		}
	}
	else {
		GameObjectIncludeChunks goic_before = this->IncludeChunks;
		vec4 renderScale = DestScale;
		vec4 renderRot = DestRot;
		vec4 renderPos = DestPos;
		matrix mat = XMMatrixScaling(renderScale.x, renderScale.y, renderScale.z) * XMMatrixRotationQuaternion(renderRot);
		mat.pos = renderPos;
		matrix temp = worldMat;
		worldMat = mat;
		BoundingOrientedBox afterobb = GetOBB();
		GameObjectIncludeChunks goic_after = game.Current_Zone->GetChunks_Include_OBB(afterobb);
		if (goic_before != goic_after) {
			worldMat = temp;
			MoveChunck(mat, goic_before, goic_after);
		}
	}
}

void DynamicGameObject::MoveChunck(const matrix& afterMat, const GameObjectIncludeChunks& beforeChunckInc, const GameObjectIncludeChunks& afterChunkInc)
{
	static int temp[512] = {};
	GameObjectIncludeChunks intersection = beforeChunckInc;
	intersection &= afterChunkInc;

	int inter_Count = intersection.GetChunckSize();
	ChunkIndex inter_ci = ChunkIndex(intersection.xmin, intersection.ymin, intersection.zmin);
	inter_ci.extra = 0;
	int inter_up = 0;
	int chunckCount = beforeChunckInc.GetChunckSize();
	ChunkIndex ci = ChunkIndex(beforeChunckInc.xmin, beforeChunckInc.ymin, beforeChunckInc.zmin);
	ci.extra = 0;
	for (; ci.extra < chunckCount; beforeChunckInc.Inc(ci)) {
		if (ci == inter_ci) { // 곂치는 부분을 Free 하지 않는다.
			intersection.Inc(inter_ci);
			temp[inter_up] = chunkAllocIndexs[ci.extra];
			inter_up += 1;
			continue;
		}

		//if (Mat == nullptr) continue;
		auto f = game.Current_Zone->chunck.find(ci);
		if (f != game.Current_Zone->chunck.end()) {
#ifdef ChunckDEBUG
			dbgbreak(f->second->Dynamic_gameobjects.isnull(chunkAllocIndexs[ci.extra]));
#endif
			f->second->Dynamic_gameobjects.Free(chunkAllocIndexs[ci.extra]);
		}
	}

	//if (Mat == nullptr) continue;
	worldMat = afterMat;

	inter_ci = ChunkIndex(intersection.xmin, intersection.ymin, intersection.zmin);
	inter_ci.extra = 0;

	chunckCount = afterChunkInc.GetChunckSize();
	ci = ChunkIndex(afterChunkInc.xmin, afterChunkInc.ymin, afterChunkInc.zmin);
	ci.extra = 0;

	inter_up = 0;
	for (; ci.extra < chunckCount; afterChunkInc.Inc(ci)) {
		if (ci == inter_ci) { // 곂치는 부분을 Free 하지 않는다.
			intersection.Inc(inter_ci);
			chunkAllocIndexs[ci.extra] = temp[inter_up];
			inter_up += 1;
			continue;
		}

		auto c = game.Current_Zone->chunck.find(ci);
		GameChunk* gc;
		if (c == game.Current_Zone->chunck.end()) {
			// new game chunk
			gc = new GameChunk();
			gc->SetChunkIndex(ci);
			game.Current_Zone->chunck.insert(pair<ChunkIndex, GameChunk*>(ci, gc));
		}
		else gc = c->second;
		int allocN = gc->Dynamic_gameobjects.Alloc();
		gc->Dynamic_gameobjects[allocN] = this;
		chunkAllocIndexs[ci.extra] = allocN;
	}

	IncludeChunks = afterChunkInc;
}

void DynamicGameObject::RecvSTC_SyncObj(char* data) {
	int offset = 0;
	STC_SyncObjData& stcsod = *(STC_SyncObjData*)(data);
	if (stcsod.shapeindex >= 0 && stcsod.shapeindex < (int)Shape::ShapeTable.size()) shape = Shape::ShapeTable[stcsod.shapeindex]; else shape.FlagPtr = 0;   // [crash-fix] out-of-range shapeindex -> null shape (avoids garbage Model pointer)
	tag = stcsod.tag;
	parent = (stcsod.parent >= 0) ? game.DynmaicGameObjects[stcsod.parent] : nullptr; // fix
	childs = (stcsod.childs >= 0) ? game.DynmaicGameObjects[stcsod.childs] : nullptr;
	sibling = (stcsod.sibling >= 0) ? game.DynmaicGameObjects[stcsod.sibling] : nullptr;
	XMMatrixDecompose((XMVECTOR*)&DestScale, (XMVECTOR*)&DestRot, (XMVECTOR*)&DestPos, stcsod.DestWorld);
	LVelocity = stcsod.LVelocity;

	offset += sizeof(STC_SyncObjData);

	Mesh* mesh = nullptr;
	Model* model = nullptr;
	shape.GetRealShape(mesh, model);
	if (mesh) {
		int& materialCount = *(int*)(data + offset);
		offset += sizeof(int);
		if (material != nullptr) {
			delete[] material;
			material = nullptr;
		}
		material = new int[materialCount];
		for (int i = 0;i < materialCount;++i) {
			material[i] = *(int*)(data + offset);
			offset += sizeof(int);
		}
	}
	else {
		int& NodeCount = *(int*)(data + offset);
		offset += sizeof(int);
		if (transforms_innerModel != nullptr) {
			delete[] transforms_innerModel;
			transforms_innerModel = nullptr;
		}
		transforms_innerModel = new matrix[NodeCount];
		for (int i = 0;i < NodeCount;++i) {
			transforms_innerModel[i] = *(matrix*)(data + offset);
			offset += sizeof(matrix);
		}
	}
}

void SkinMeshGameObject::STATICINIT(int typeindex) {
	DynamicGameObject::STATICINIT(typeindex);
	for (int i = 0; i < SkinMeshGameObject::g_member.size();++i) {
		g_member[i].offset = g_member[i].get_offset();
		MemberInfo& mi = *(MemberInfo*)&g_member[i];
		int lastindex = GameObjectType::Client_STCMembers[typeindex].size();
		GameObjectType::Client_STCMembers[typeindex].emplace_back(mi.name, mi.get_offset, mi.size);
		GameObjectType::Client_STCMembers[typeindex][lastindex].offset = g_member[i].offset;
	}
}

SkinMeshGameObject::SkinMeshGameObject()
{
	tag[GameObjectTag::Tag_Enable] = false;
	tag[GameObjectTag::Tag_Dynamic] = true;
	chunkAllocIndexsCapacity = 0;
	ZeroMemory(&IncludeChunks, sizeof(GameObjectIncludeChunks));

	OutVertexUAV = nullptr;
	modifyMeshes = nullptr;
	for (int i = 0;i < 4;++i) {
		AnimationFlowTime[4] = 0;
		DestAnimationFlowTime[i] = 0;
		PlayingAnimationIndex[i] = 0;
	}
}

SkinMeshGameObject::~SkinMeshGameObject()
{
}

void SkinMeshGameObject::InitRootBoneMatrixs()
{
	bool initialState = gd.gpucmd.isClose;

	if (BoneToWorldMatrixCB_Default.size() > 0) {
		//if (Mat == nullptr) continue;
		//if (Mat == nullptr) continue;
		//if (Mat == nullptr) continue;
		//if (Mat == nullptr) continue;

		for (int i = 0;i < BoneToWorldMatrixCB_Default.size();++i) {
			BoneToWorldMatrixCB_Default[i].Release();
			BoneToWorldMatrixCB[i].resource->Unmap(0, nullptr);
			BoneToWorldMatrixCB[i].Release();
			RootBoneMatrixs_PerSkinMesh[i] = nullptr;
			if constexpr (gd.PlayAnimationByGPU) {
				gd.DynamicDescriptorAllotter.Free(BoneToWorldMatrix_UAVDescIndex[i].index);
				NodeToBone[i].Release();
				gd.DynamicDescriptorAllotter.Free(NodeToBone_SRVDescIndex[i].index);
			}
		}
		BoneToWorldMatrixCB_Default.clear();
		BoneToWorldMatrixCB.clear();
		RootBoneMatrixs_PerSkinMesh.clear();
		if constexpr (gd.PlayAnimationByGPU) {
			BoneToWorldMatrix_UAVDescIndex.clear();
			NodeTposMatrixs.Release();
			gd.DynamicDescriptorAllotter.Free(NodeTposMatrixs_SRVDescIndex.index);
			Node_ToParentRes.Release();
			gd.DynamicDescriptorAllotter.Free(NodeToParentSRVIndex.index);
			HumanoidToNodeIndexRes.Release();
			gd.DynamicDescriptorAllotter.Free(HumanoidToNodeIndexSRVIndex.index);
			AnimationBlendConstantUploadBuffer.resource->Unmap(0, nullptr);
			AnimationBlendConstantUploadBuffer.Release();
			AnimBlendingCB_Mapped = nullptr;
			gd.DynamicDescriptorAllotter.Free(AnimBlendingCBVDescIndex.index);
			NodeToBone_SRVDescIndex.clear();
			NodeToBone.clear();
		}
		//if (Mat == nullptr) continue;
	}

	Model* pModel = shape.GetModel();
	if (pModel == nullptr || pModel->nodeCount <= 0 || pModel->Nodes == nullptr ||
		pModel->DefaultNodelocalTr == nullptr) {
		return;
	}
	for (int i = 0; i < pModel->mNumSkinMesh; ++i) {
		dbgc[5] += 1;
		int boneNum = pModel->mBumpSkinMeshs[i]->MatrixCount;
		UINT ncbElementBytes = (((sizeof(matrix) * 128) + 255) & ~255); //256의 배수
		GPUResource res_upload = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1);
		GPUResource res = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1, DXGI_FORMAT_UNKNOWN, 1, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

		BoneToWorldMatrixCB.push_back(res_upload);
		BoneToWorldMatrixCB_Default.push_back(res);
		matrix* mapped = nullptr;
		BoneToWorldMatrixCB[i].resource->Map(0, nullptr, (void**)&mapped);
		RootBoneMatrixs_PerSkinMesh.push_back(mapped);
	}

	SetRootMatrixs();

	for (int i = 0; i < pModel->mNumSkinMesh; ++i)
	{
		int boneNum = pModel->mBumpSkinMeshs[i]->MatrixCount;
		UINT ncbElementBytes = (((sizeof(matrix) * 128) + 255) & ~255);
		int n = gd.DynamicDescriptorAllotter.Alloc();
		D3D12_CPU_DESCRIPTOR_HANDLE hCPU = gd.DynamicDescriptorAllotter.GetCPUHandle(n);
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = BoneToWorldMatrixCB_Default[i].resource->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = ncbElementBytes;
		gd.pDevice->CreateConstantBufferView(&cbvDesc, hCPU);
		BoneToWorldMatrixCB_Default[i].descindex.Set(false, n);
	}

	if constexpr (gd.PlayAnimationByGPU) {
		int datasize;
		UINT ncbElementBytes;
		int index;
		Model* model = shape.GetModel();

		//Node To Parent
		{
			int nodeCount = model->nodeCount;
			datasize = sizeof(int) * nodeCount;
			ncbElementBytes = ((datasize + 255) & ~255); //256의 배수
			Node_ToParentRes = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1, DXGI_FORMAT_UNKNOWN, 1, D3D12_RESOURCE_FLAG_NONE);
			GPUResource Node_ToParentRes_upload = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1, DXGI_FORMAT_UNKNOWN, 1, D3D12_RESOURCE_FLAG_NONE);

			int* tempMapped = nullptr;
			Node_ToParentRes_upload.resource->Map(0, nullptr, (void**)&tempMapped);
			for (int i = 0;i < nodeCount;++i) {
				ModelNode* pnode = model->Nodes[i].parent;
				int pindex = 0;
				if (pnode == nullptr) {
					pindex = -1;
				}
				else {
					pindex = ((char*)pnode - (char*)model->Nodes) / sizeof(ModelNode);
				}
				tempMapped[i] = pindex;
			}
			Node_ToParentRes_upload.resource->Unmap(0, nullptr);

			if (gd.gpucmd.isClose) {
				gd.gpucmd.Reset();
			}
			gd.gpucmd.ResBarrierTr(&Node_ToParentRes, D3D12_RESOURCE_STATE_COPY_DEST);
			gd.gpucmd.ResBarrierTr(&Node_ToParentRes_upload, D3D12_RESOURCE_STATE_COPY_SOURCE);
			gd.gpucmd->CopyResource(Node_ToParentRes.resource, Node_ToParentRes_upload.resource);
			gd.gpucmd.ResBarrierTr(&Node_ToParentRes, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			gd.gpucmd.Close();
			gd.gpucmd.Execute();
			gd.gpucmd.WaitGPUComplete();
			Node_ToParentRes_upload.Release();

			//SRV
			index = gd.DynamicDescriptorAllotter.Alloc();
			NodeToParentSRVIndex.Set(false, index, 'n');
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			srvDesc.Buffer.NumElements = nodeCount;
			srvDesc.Buffer.StructureByteStride = sizeof(int);
			gd.pDevice->CreateShaderResourceView(Node_ToParentRes.resource, &srvDesc, NodeToParentSRVIndex.hCreation.hcpu);
		}

		//HumanoidToNodeIndexRes
		{
			datasize = sizeof(int) * 64;
			ncbElementBytes = ((datasize + 255) & ~255); //256의 배수
			HumanoidToNodeIndexRes = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1, DXGI_FORMAT_UNKNOWN, 1, D3D12_RESOURCE_FLAG_NONE);
			GPUResource HumanoidToNodeIndexRes_upload = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1, DXGI_FORMAT_UNKNOWN, 1, D3D12_RESOURCE_FLAG_NONE);

			int* tempMapped = nullptr;
			HumanoidToNodeIndexRes_upload.resource->Map(0, nullptr, (void**)&tempMapped);
			for (int i = 0;i < 64;++i) {
				tempMapped[i] = -1;
			}
			for (int i = 0;i < model->nodeCount;++i) {
				int hindex = model->Humanoid_retargeting[i];
				if(hindex >=0) tempMapped[hindex] = i;
			}
			HumanoidToNodeIndexRes_upload.resource->Unmap(0, nullptr);

			if (gd.gpucmd.isClose) {
				gd.gpucmd.Reset();
			}
			gd.gpucmd.ResBarrierTr(&HumanoidToNodeIndexRes, D3D12_RESOURCE_STATE_COPY_DEST);
			gd.gpucmd.ResBarrierTr(&HumanoidToNodeIndexRes_upload, D3D12_RESOURCE_STATE_COPY_SOURCE);
			gd.gpucmd->CopyResource(HumanoidToNodeIndexRes.resource, HumanoidToNodeIndexRes_upload.resource);
			gd.gpucmd.ResBarrierTr(&HumanoidToNodeIndexRes, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			gd.gpucmd.Close();
			gd.gpucmd.Execute();
			gd.gpucmd.WaitGPUComplete();
			HumanoidToNodeIndexRes_upload.Release();

			//SRV
			index = gd.DynamicDescriptorAllotter.Alloc();
			HumanoidToNodeIndexSRVIndex.Set(false, index, 'n');
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			srvDesc.Buffer.NumElements = 64;
			srvDesc.Buffer.StructureByteStride = sizeof(int);
			gd.pDevice->CreateShaderResourceView(HumanoidToNodeIndexRes.resource, &srvDesc, HumanoidToNodeIndexSRVIndex.hCreation.hcpu);
		}

		///NodeLocalMatrixs
		{
			int nodeCount = model->nodeCount;
			datasize = sizeof(matrix) * nodeCount;
			ncbElementBytes = ((datasize + 255) & ~255); //256의 배수
			NodeLocalMatrixs = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1, DXGI_FORMAT_UNKNOWN, 1, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			GPUResource NodeLocalMatrixs_upload = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1, DXGI_FORMAT_UNKNOWN, 1, D3D12_RESOURCE_FLAG_NONE);
			matrix* tempMapped = nullptr;
			NodeLocalMatrixs_upload.resource->Map(0, nullptr, (void**)&tempMapped);
			for (int i = 0;i < nodeCount;++i) {
				/*tempMapped[i] = transforms_innerModel[i];
				tempMapped[i].transpose();*/
				tempMapped[i].Id();
			}
			NodeLocalMatrixs_upload.resource->Unmap(0, nullptr);

			if (gd.gpucmd.isClose) {
				gd.gpucmd.Reset();
			}
			gd.gpucmd.ResBarrierTr(&NodeLocalMatrixs, D3D12_RESOURCE_STATE_COPY_DEST);
			gd.gpucmd.ResBarrierTr(&NodeLocalMatrixs_upload, D3D12_RESOURCE_STATE_COPY_SOURCE);
			gd.gpucmd->CopyResource(NodeLocalMatrixs.resource, NodeLocalMatrixs_upload.resource);
			gd.gpucmd.ResBarrierTr(&NodeLocalMatrixs, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			gd.gpucmd.Close();
			gd.gpucmd.Execute();
			gd.gpucmd.WaitGPUComplete();
			NodeLocalMatrixs_upload.Release();

			//UAV
			index = gd.DynamicDescriptorAllotter.Alloc();
			NodeLocalMatrixs_UAVDescIndex.Set(false, index, 'n');
			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
			uavDesc.Format = DXGI_FORMAT_UNKNOWN;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			uavDesc.Buffer.FirstElement = 0;
			uavDesc.Buffer.CounterOffsetInBytes = 0;
			uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
			uavDesc.Buffer.NumElements = nodeCount;
			uavDesc.Buffer.StructureByteStride = sizeof(matrix);
			gd.pDevice->CreateUnorderedAccessView(NodeLocalMatrixs.resource, nullptr, &uavDesc, NodeLocalMatrixs_UAVDescIndex.hCreation.hcpu);
			//SRV
			index = gd.DynamicDescriptorAllotter.Alloc();
			NodeLocalMatrixs_SRVDescIndex.Set(false, index, 'n');
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			srvDesc.Buffer.NumElements = nodeCount;
			srvDesc.Buffer.StructureByteStride = sizeof(matrix);
			gd.pDevice->CreateShaderResourceView(NodeLocalMatrixs.resource, &srvDesc, NodeLocalMatrixs_SRVDescIndex.hCreation.hcpu);
		}

		//NodeToBoneIndex
		NodeToBone.reserve(model->mNumSkinMesh);
		NodeToBone_SRVDescIndex.reserve(model->mNumSkinMesh);
		for (int i = 0;i < model->mNumSkinMesh;++i) {
			int nodeCount = model->nodeCount;
			datasize = sizeof(int) * 128;
			ncbElementBytes = ((datasize + 255) & ~255); //256의 배수
			DescIndex resdescindex;
			GPUResource res = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1, DXGI_FORMAT_UNKNOWN, 1, D3D12_RESOURCE_FLAG_NONE);
			GPUResource res_upload = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1, DXGI_FORMAT_UNKNOWN, 1, D3D12_RESOURCE_FLAG_NONE);

			int* tempMapped = nullptr;
			res_upload.resource->Map(0, nullptr, (void**)&tempMapped);
			BumpSkinMesh* bsm = model->mBumpSkinMeshs[i];
			for (int i = 0;i < 128;++i) {
				tempMapped[i] = -1;
			}
			for (int i = 0;i < bsm->MatrixCount; ++i) {
				tempMapped[bsm->toNodeIndex[i]] = i;
			}
			res_upload.resource->Unmap(0, nullptr);

			if (gd.gpucmd.isClose) {
				gd.gpucmd.Reset();
			}
			gd.gpucmd.ResBarrierTr(&res, D3D12_RESOURCE_STATE_COPY_DEST);
			gd.gpucmd.ResBarrierTr(&res_upload, D3D12_RESOURCE_STATE_COPY_SOURCE);
			gd.gpucmd->CopyResource(res.resource, res_upload.resource);
			gd.gpucmd.ResBarrierTr(&res, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			gd.gpucmd.Close();
			gd.gpucmd.Execute();
			gd.gpucmd.WaitGPUComplete();
			res_upload.Release();

			//SRV
			index = gd.DynamicDescriptorAllotter.Alloc();
			resdescindex.Set(false, index, 'n');
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			srvDesc.Buffer.NumElements = 128;
			srvDesc.Buffer.StructureByteStride = sizeof(int);
			gd.pDevice->CreateShaderResourceView(res.resource, &srvDesc, resdescindex.hCreation.hcpu);
			NodeToBone.push_back(res);
			NodeToBone_SRVDescIndex.push_back(resdescindex);
		}

		//Node Tpos Matrixs
		{
			int nodeCount = model->nodeCount;
			datasize = sizeof(matrix) * nodeCount;
			ncbElementBytes = ((datasize + 255) & ~255); //256의 배수
			NodeTposMatrixs = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1, DXGI_FORMAT_UNKNOWN, 1, D3D12_RESOURCE_FLAG_NONE);
			GPUResource NodeTposMatrixs_upload = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1, DXGI_FORMAT_UNKNOWN, 1, D3D12_RESOURCE_FLAG_NONE);
			matrix* tempMapped = nullptr;
			NodeTposMatrixs_upload.resource->Map(0, nullptr, (void**)&tempMapped);
			for (int i = 0;i < nodeCount;++i) {
				tempMapped[i] = model->DefaultNodelocalTr[i];
				tempMapped[i].transpose();
			}
			NodeTposMatrixs_upload.resource->Unmap(0, nullptr);

			if (gd.gpucmd.isClose) {
				gd.gpucmd.Reset();
			}
			gd.gpucmd.ResBarrierTr(&NodeTposMatrixs, D3D12_RESOURCE_STATE_COPY_DEST);
			gd.gpucmd.ResBarrierTr(&NodeTposMatrixs_upload, D3D12_RESOURCE_STATE_COPY_SOURCE);
			gd.gpucmd->CopyResource(NodeTposMatrixs.resource, NodeTposMatrixs_upload.resource);
			gd.gpucmd.ResBarrierTr(&NodeTposMatrixs, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			gd.gpucmd.Close();
			gd.gpucmd.Execute();
			gd.gpucmd.WaitGPUComplete();
			NodeTposMatrixs_upload.Release();

			if (initialState == false) {
				gd.gpucmd.Reset();
			}

			//SRV
			index = gd.DynamicDescriptorAllotter.Alloc();
			NodeTposMatrixs_SRVDescIndex.Set(false, index, 'n');
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			srvDesc.Buffer.NumElements = nodeCount;
			srvDesc.Buffer.StructureByteStride = sizeof(matrix);
			gd.pDevice->CreateShaderResourceView(NodeTposMatrixs.resource, &srvDesc, NodeTposMatrixs_SRVDescIndex.hCreation.hcpu);
		}

		///AnimationBlendConstantUploadBuffer
		{
			datasize = sizeof(AnimationBlendingCBStruct);
			ncbElementBytes = ((datasize + 255) & ~255); //256의 배수
			AnimationBlendConstantUploadBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1, DXGI_FORMAT_UNKNOWN, 1, D3D12_RESOURCE_FLAG_NONE);
			AnimationBlendConstantUploadBuffer.resource->Map(0, nullptr, (void**)&AnimBlendingCB_Mapped);
			//CBV
			index = gd.DynamicDescriptorAllotter.Alloc();
			AnimBlendingCBVDescIndex.Set(false, index, 'n');
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = AnimationBlendConstantUploadBuffer.resource->GetGPUVirtualAddress();
			cbvDesc.SizeInBytes = ncbElementBytes;
			gd.pDevice->CreateConstantBufferView(&cbvDesc, AnimBlendingCBVDescIndex.hCreation.hcpu);
		}

		//Make Bone ToWorld UAV
		BoneToWorldMatrix_UAVDescIndex.reserve(BoneToWorldMatrixCB_Default.size());
		for (int i = 0;i < BoneToWorldMatrixCB_Default.size();++i) {
			DescIndex di;
			BumpSkinMesh* bsm = model->mBumpSkinMeshs[i];
			//UAV
			index = gd.DynamicDescriptorAllotter.Alloc();
			di.Set(false, index, 'n');
			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
			uavDesc.Format = DXGI_FORMAT_UNKNOWN;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			uavDesc.Buffer.FirstElement = 0;
			uavDesc.Buffer.CounterOffsetInBytes = 0;
			uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
			uavDesc.Buffer.NumElements = bsm->MatrixCount;
			uavDesc.Buffer.StructureByteStride = sizeof(matrix);
			gd.pDevice->CreateUnorderedAccessView(BoneToWorldMatrixCB_Default[i].resource, nullptr, &uavDesc, di.hCreation.hcpu);
			BoneToWorldMatrix_UAVDescIndex.push_back(di);
		}
	}
}

void SkinMeshGameObject::SetRootMatrixs()
{
	//if (Mat == nullptr) continue;
	//if (Mat == nullptr) continue;
	//if (Mat == nullptr) continue;
	//if (Mat == nullptr) continue;

	static matrix TempMatBuffer[128] = {};
	Model* pModel = shape.GetModel();
	for (int i = 0;i < pModel->nodeCount;++i) {
		ModelNode* node = &pModel->Nodes[i];
		if (node->parent != nullptr) {
			ModelNode* parent_node = &pModel->Nodes[i];
			int pni = ((char*)node->parent - (char*)pModel->Nodes) / sizeof(ModelNode);
			TempMatBuffer[i] = transforms_innerModel[i] * TempMatBuffer[pni];
		}
		else {
			TempMatBuffer[i] = transforms_innerModel[i];
		}
	}

	for (int i = 0; i < pModel->mNumSkinMesh; ++i) {
		matrix* marr = RootBoneMatrixs_PerSkinMesh[i];
		BumpSkinMesh* bsm = pModel->mBumpSkinMeshs[i];
		for (int k = 0; k < bsm->MatrixCount; ++k) {
			int nodeindex = bsm->toNodeIndex[k];
			RootBoneMatrixs_PerSkinMesh[i][k] = TempMatBuffer[nodeindex];
			RootBoneMatrixs_PerSkinMesh[i][k].transpose();
			/*int ni = ((char*)node - (char*)pModel->Nodes) / sizeof(ModelNode);

			RootBoneMatrixs_PerSkinMesh[i][k].Id();
			while (node != nullptr) {
				RootBoneMatrixs_PerSkinMesh[i][k] *= transforms_innerModel[ni];

				node = node->parent;
				ni = ((char*)node - (char*)pModel->Nodes) / sizeof(ModelNode);
			}
			RootBoneMatrixs_PerSkinMesh[i][k].transpose();*/
		}
	}
}

void SkinMeshGameObject::GetBoneLocalMatrixAtTime(HumanoidAnimation* hanim, matrix* out, float time)
{
	float t = time;
	constexpr bool isLoop = true;
	if (isLoop) {
		t = time - floor(time / (float)hanim->Duration) * (float)hanim->Duration;
	}
	Model* pModel = shape.GetModel();
	for (int i = 0; i < pModel->nodeCount; ++i) {
		int n = pModel->Humanoid_retargeting[i];
		if (n >= 0) {
			out[i] = hanim->channels[n].GetLocalMatrixAtTime(t, out[i]);
		}
	}
}

void SkinMeshGameObject::RaytracingUpdateTransform() {
	Mesh* mesh = nullptr;
	Model* model = nullptr;
	shape.GetRealShape(mesh, model);
	if (model && gd.isSupportRaytracing) {
		if (RaytracingWorldMatInput_Model != nullptr) {
			matrix Id;
			Id.Id();
			for (int i = 0; i < model->nodeCount; ++i) {
				if (RaytracingWorldMatInput_Model[i]) {
					memcpy(RaytracingWorldMatInput_Model[i], &Id, sizeof(float) * 12);
				}
			}
		}
	}
}

void SkinMeshGameObject::SetShape(Shape _shape, int ZoneID)
{
	static LocalRootSigData tempLRSSaver[1024] = {};

	shape = _shape;
	Mesh* mesh = nullptr;
	Model* model = nullptr;
	shape.GetRealShape(mesh, model);
	if (model != nullptr) {
		transforms_innerModel = new matrix[model->nodeCount];
		for (int i = 0; i < model->nodeCount; ++i) {
			transforms_innerModel[i] = model->Nodes[i].transform;
			transforms_innerModel[i].transpose();
		}

		InitRootBoneMatrixs();

		if (gd.isSupportRaytracing) {
			modifyMeshes = new RayTracingMesh[model->mNumSkinMesh];
			OutVertexUAV = new DescIndex[model->mNumSkinMesh];
			for (int i = 0; i < model->mNumSkinMesh; ++i) {
				//if (Mat == nullptr) continue;
				modifyMeshes[i].AllocateRaytracingUAVMesh(model->mBumpSkinMeshs[i]->vertexData, model->mBumpSkinMeshs[i]->rmesh.IBStartOffset, model->mBumpSkinMeshs[i]->subMeshNum, model->mBumpSkinMeshs[i]->SubMeshIndexStart);
				modifyMeshes[i].IBStartOffset = new UINT64[/*model->mBumpSkinMeshs[i]->subMeshNum */1+ 1];
				modifyMeshes[i].subMeshCount = model->mBumpSkinMeshs[i]->subMeshNum;
				for (int k = 0; k < /*model->mBumpSkinMeshs[i]->subMeshNum*/1 + 1; ++k) {
					modifyMeshes[i].IBStartOffset[k] = model->mBumpSkinMeshs[i]->rmesh.IBStartOffset[k];
				}

				//Compute Geometry ������ ���� UAV ����
				int index = gd.DynamicDescriptorAllotter.Alloc();
				OutVertexUAV[i] = DescIndex(false, index);
				D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
				uavDesc.Format = DXGI_FORMAT_UNKNOWN;
				uavDesc.Buffer.FirstElement = modifyMeshes[i].UAV_VBStartOffset / sizeof(RayTracingMesh::Vertex);
				uavDesc.Buffer.NumElements = model->mBumpSkinMeshs[i]->vertexData.size();
				uavDesc.Buffer.StructureByteStride = sizeof(RayTracingMesh::Vertex);
				uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
				gd.pDevice->CreateUnorderedAccessView(RayTracingMesh::UAV_vertexBuffer, nullptr, &uavDesc, OutVertexUAV[i].hCreation.hcpu);
			}

			RaytracingWorldMatInput_Model = new float* [model->nodeCount];
			for (int i = 0; i < model->nodeCount; ++i) {
				RaytracingWorldMatInput_Model[i] = nullptr;
				if (model->Nodes[i].Mesh_SkinMeshindex != nullptr) {
					for (int k = 0; k < model->Nodes[i].numMesh; ++k) {
						if (model->Nodes[i].Mesh_SkinMeshindex != nullptr && model->Nodes[i].Mesh_SkinMeshindex[k] >= 0) {
							int skinmeshIndex = model->Nodes[i].Mesh_SkinMeshindex[k];
							BumpSkinMesh* bsmesh = model->mBumpSkinMeshs[skinmeshIndex];
							/*for (int k = 0; k < bsmesh->subMeshNum; ++k) {
								tempLRSSaver[k] = LocalRootSigData(modifyMeshes[skinmeshIndex].UAV_VBStartOffset / sizeof(RayTracingMesh::Vertex), bsmesh->rmesh.IBStartOffset[k] / sizeof(unsigned int));
							}*/
							int matindex = model->Nodes[i].materialIndex[0]; // game.GetRenderMaterialIndexFromGlobalMaterialIndex();
							tempLRSSaver[0] = LocalRootSigData(modifyMeshes[skinmeshIndex].UAV_VBStartOffset / sizeof(RayTracingMesh::Vertex), bsmesh->rmesh.IBStartOffset[0] / sizeof(unsigned int), matindex, -1);
							float** WorldMatInputs = game.MyRayTracingShader->push_rins_immortal(&modifyMeshes[skinmeshIndex], worldMat, tempLRSSaver, 1);

							//RaytracingWorldMatInput_Model[i] = new float* [bsmesh->subMeshNum];
							/*for (int k = 0; k < bsmesh->subMeshNum; ++k) {
								RaytracingWorldMatInput_Model[i][k] = WorldMatInputs[k];
							}*/
							RaytracingWorldMatInput_Model[i] = WorldMatInputs[0];
						}
						else {
							//if (Mat == nullptr) continue;
							BumpMesh* bmesh = (BumpMesh*)model->mMeshes[model->Nodes[i].Meshes[0]];
							/*for (int k = 0; k < bmesh->subMeshNum; ++k) {
								tempLRSSaver[k] = LocalRootSigData(bmesh->rmesh.VBStartOffset / sizeof(RayTracingMesh::Vertex), bmesh->rmesh.IBStartOffset[k] / sizeof(unsigned int));
							}*/
							int matindex = model->Nodes[i].materialIndex[0]; // game.GetRenderMaterialIndexFromGlobalMaterialIndex(model->Nodes[i].materialIndex[0]);
							tempLRSSaver[0] = LocalRootSigData(bmesh->rmesh.VBStartOffset / sizeof(RayTracingMesh::Vertex), bmesh->rmesh.IBStartOffset[0] / sizeof(unsigned int), matindex, -1);

							float** WorldMatInputs = game.MyRayTracingShader->push_rins_immortal(&bmesh->rmesh, worldMat, tempLRSSaver);

							/*RaytracingWorldMatInput_Model[i] = new float* [bmesh->subMeshNum];
							for (int k = 0; k < bmesh->subMeshNum; ++k) {
								RaytracingWorldMatInput_Model[i][k] = WorldMatInputs[k];
							}*/
							RaytracingWorldMatInput_Model[i] = WorldMatInputs[0];
						}
					}
				}
				else {
					if (model->Nodes[i].numMesh > 0) {
						BumpMesh* bmesh = (BumpMesh*)model->mMeshes[model->Nodes[i].Meshes[0]];
						/*for (int k = 0; k < bmesh->subMeshNum; ++k) {
							tempLRSSaver[k] = LocalRootSigData(bmesh->rmesh.VBStartOffset / sizeof(RayTracingMesh::Vertex), bmesh->rmesh.IBStartOffset[k] / sizeof(unsigned int));
						}*/
						int matindex = model->Nodes[i].materialIndex[0]; // game.GetRenderMaterialIndexFromGlobalMaterialIndex();
						tempLRSSaver[0] = LocalRootSigData(bmesh->rmesh.VBStartOffset / sizeof(RayTracingMesh::Vertex), bmesh->rmesh.IBStartOffset[0] / sizeof(unsigned int), matindex, -1);

						float** WorldMatInputs = game.MyRayTracingShader->push_rins_immortal(&bmesh->rmesh, worldMat, tempLRSSaver);

						/*RaytracingWorldMatInput_Model[i] = new float* [bmesh->subMeshNum];
						for (int k = 0; k < bmesh->subMeshNum; ++k) {
							RaytracingWorldMatInput_Model[i][k] = WorldMatInputs[k];
						}*/
						RaytracingWorldMatInput_Model[i] = WorldMatInputs[0];
					}
				}
			}
			RaytracingUpdateTransform();
		}
	}
	if (mesh != nullptr) {
		shape.SetModel(nullptr);
	}
}

void SkinMeshGameObject::Update(float delatTime)
{
	if constexpr (gd.PlayAnimationByGPU == false) {
		constexpr float frameSpeed = 1;
		Model* pModel = shape.GetModel();
		AnimationFlowTime[0] += delatTime * frameSpeed;
		//GetBoneLocalMatrixAtTime(&game.HumanoidAnimationTable[PlayingAnimationIndex[0]], transforms_innerModel, AnimationFlowTime[0]);
		for (int i = 0; i < pModel->nodeCount; ++i) {
			transforms_innerModel[i].Id();
		}
		//GetBoneLocalMatrixAtTime(&game.HumanoidAnimationTable[PlayingAnimationIndex[0]], transforms_innerModel, AnimationFlowTime[0]);
		for (int i = 0; i < pModel->nodeCount; ++i) {
			transforms_innerModel[i] *= pModel->DefaultNodelocalTr[i];
		}//?
		transforms_innerModel[0] = worldMat;
		SetRootMatrixs();
	}
	PositionInterpolation(delatTime);
}

void SkinMeshGameObject::Render(matrix parent) {
	Mesh* mesh = nullptr;
	Model* model = nullptr;
	BoundingOrientedBox obb_local, obb;
	bool b;
	matrix world = GetWorld();
	world *= parent;

	shape.GetRealShape(mesh, model);
	if (mesh != nullptr) obb_local = mesh->GetOBB();
	else if (model != nullptr) obb_local = model->GetOBB();
	else return;

	if (mesh == nullptr) {
		if (model->mNumSkinMesh == 0) {
			matrix pbrView = gd.viewportArr[0].ViewMatrix;
			pbrView *= gd.viewportArr[0].ProjectMatrix;
			pbrView.transpose();

			gd.gpucmd.SetShader(game.MyPBRShader1, ShaderType::RenderWithShadow);
			game.PresentShaderType = ShaderType::RenderWithShadow;
			gd.gpucmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
			{
				using PRID = PBRShader1::RootParamId;
				gd.gpucmd->SetGraphicsRoot32BitConstants(PRID::Const_Camera, 16, &pbrView, 0);
				gd.gpucmd->SetGraphicsRoot32BitConstants(PRID::Const_Camera, 4, &gd.viewportArr[0].Camera_Pos, 16);
				gd.gpucmd->SetGraphicsRootConstantBufferView(PRID::CBV_StaticLight, game.LightCB_withShadowResource[game.Current_Zone->Asset_OffsetMul].resource->GetGPUVirtualAddress());
				gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_ShadowMap, game.MyDirLight[0].descindex.hRender.hgpu);
				gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_EnvionmentMap, game.MySkyBoxShader->CurrentSkyBox.descindex.hRender.hgpu);
				if (game.Current_Zone->bReqireBakeLight_Raster == false) {
					gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_Chunck_StaticLightStructuredBuffer, game.Current_Zone->Immortal_ZoneLightBuffer_SRV.hRender.hgpu);
				}
			}
			matrix renderWorld = GetModelRenderWorld(model, world);
			model->Render<false>(gd.gpucmd, renderWorld, this);

			gd.gpucmd.SetShader(game.MyPBRShader1, ShaderType::SkinMeshRender);
			game.PresentShaderType = ShaderType::SkinMeshRender;
			gd.gpucmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
			{
				using PRID = PBRShader1::RootParamId;
				gd.gpucmd->SetGraphicsRoot32BitConstants(PRID::Const_Camera, 16, &pbrView, 0);
				gd.gpucmd->SetGraphicsRoot32BitConstants(PRID::Const_Camera, 4, &gd.viewportArr[0].Camera_Pos, 16);
				gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::CBVTable_SkinMeshLightData, game.LightCB_withShadowResource[game.Current_Zone->Asset_OffsetMul].descindex.hRender.hgpu);
				gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_SkinMeshShadowMaps, game.MyDirLight[0].descindex.hRender.hgpu);
				gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_SKinMeshEnvironmentMap, game.MySkyBoxShader->CurrentSkyBox.descindex.hRender.hgpu);
				if (game.Current_Zone->bReqireBakeLight_Raster == false) {
					gd.gpucmd->SetGraphicsRootDescriptorTable(PRID::SRVTable_SKinMesh_Chunck_StaticLightStructuredBuffer, game.Current_Zone->Immortal_ZoneLightBuffer_SRV.hRender.hgpu);
				}
			}
		}
		else if (BoneToWorldMatrixCB.size() != 0) {
			matrix renderWorld = GetSkinMeshRenderWorld(model, world);
			model->Render<true>(gd.gpucmd, renderWorld, this);
		}
	}
}

void SkinMeshGameObject::RenderShadow(matrix parent)
{
	Mesh* mesh = nullptr;
	Model* model = nullptr;
	BoundingOrientedBox obb_local, obb;
	bool b;
	matrix world = GetWorld();
	world *= parent;

	shape.GetRealShape(mesh, model);
	if (mesh != nullptr) obb_local = mesh->GetOBB();
	else if (model != nullptr) obb_local = model->GetOBB();
	else return;

	obb_local.Transform(obb, world);

	if (mesh == nullptr) {
		if (model->mNumSkinMesh == 0) {
			return;
		}
		else if (BoneToWorldMatrixCB.size() != 0) {
			matrix renderWorld = GetSkinMeshRenderWorld(model, world);
			model->Render<true>(gd.gpucmd, renderWorld, this);
		}
	}
}

void SkinMeshGameObject::PushRenderBatch(matrix parent)
{
	Mesh* mesh = nullptr;
	Model* model = nullptr;
	shape.GetRealShape(mesh, model);
	if (model != nullptr && model->mNumSkinMesh == 0) {
		matrix world = GetWorld();
		world *= parent;
		matrix renderWorld = GetModelRenderWorld(model, world);
		model->PushRenderBatch(renderWorld, this);
	}
}

void SkinMeshGameObject::ModifyVertexs(matrix parent)
{
	//if (Mat == nullptr) continue;
	Model* model = shape.GetModel();
	for (int i = 0; i < model->mNumSkinMesh; ++i) {
		BumpSkinMesh* bsmesh = model->mBumpSkinMeshs[i];
		unsigned int VertexSiz = bsmesh->vertexData.size();
		using SMMSRPI = SkinMeshModifyShader::RootParamId;

		int boneNum = model->mBumpSkinMeshs[i]->MatrixCount;
		UINT ncbElementBytes = (((sizeof(matrix) * 128) + 255) & ~255); //256의 배수
		gd.CScmd.ResBarrierTr(&BoneToWorldMatrixCB_Default[i], D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		//gd.CScmd.ResBarrierTr(&BoneToWorldMatrixCB[i], D3D12_RESOURCE_STATE_COPY_SOURCE);
		//gd.CScmd->CopyBufferRegion(BoneToWorldMatrixCB_Default[i].resource, 0, BoneToWorldMatrixCB[i].resource, 0, ncbElementBytes);
		//gd.CScmd.ResBarrierTr(&BoneToWorldMatrixCB_Default[i], D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		//gd.CScmd.ResBarrierTr(&BoneToWorldMatrixCB[i], D3D12_RESOURCE_STATE_GENERIC_READ);

		gd.CScmd->SetComputeRoot32BitConstants(SMMSRPI::Const_OutputVertexBufferSize, 1, &VertexSiz, 0);

		DescHandle hCBV;
		gd.ShaderVisibleDescPool.DynamicAlloc(&hCBV, 2);
		gd.pDevice->CopyDescriptorsSimple(1, hCBV[0].hcpu, bsmesh->ToOffsetMatrixsCB.descindex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		gd.pDevice->CopyDescriptorsSimple(1, hCBV[1].hcpu, BoneToWorldMatrixCB_Default[i].descindex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		gd.CScmd->SetComputeRootDescriptorTable(SMMSRPI::CBVTable_OffsetMatrixs_ToWorldMatrixs, hCBV.hgpu);

		DescHandle hSRV;
		gd.ShaderVisibleDescPool.DynamicAlloc(&hSRV, 2);
		gd.pDevice->CopyDescriptorsSimple(1, hSRV[0].hcpu, bsmesh->VertexSRV.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		gd.pDevice->CopyDescriptorsSimple(1, hSRV[1].hcpu, bsmesh->BoneSRV.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		gd.CScmd->SetComputeRootDescriptorTable(SMMSRPI::SRVTable_SourceVertexAndBoneData, hSRV.hgpu);

		DescHandle hUAV;
		gd.ShaderVisibleDescPool.DynamicAlloc(&hUAV, 1);
		gd.pDevice->CopyDescriptorsSimple(1, hUAV[0].hcpu, OutVertexUAV[i].hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		gd.CScmd->SetComputeRootDescriptorTable(SMMSRPI::UAVTable_OutputVertexData, hUAV.hgpu);

		int disPatchW = (VertexSiz / 768) + 1;
		gd.CScmd->Dispatch(disPatchW, 1, 1);
	}
	//if (Mat == nullptr) continue;
	game.MyRayTracingShader->RebuildBLASBuffer.push_back(this);
}

void SkinMeshGameObject::BlendingAnimation() {
	AnimationBlendingCBStruct* cbData = (AnimationBlendingCBStruct*)AnimBlendingCB_Mapped;

	if (game.HumanoidAnimationTable.size() == 0) return;

	if (cbData != nullptr) {
		int anim0 = PlayingAnimationIndex[0];
		if (anim0 < 0 || anim0 >= game.HumanoidAnimationTable.size()) {
			anim0 = 0;
		}
		cbData->animTime[0] = AnimationFlowTime[0];
		cbData->MAXTime[0] = game.HumanoidAnimationTable[anim0].Duration;
		cbData->animWeight[0] = 1.0f;

		int anim1 = PlayingAnimationIndex[1];
		if (anim1 >= 0 && anim1 < game.HumanoidAnimationTable.size()) {
			cbData->animTime[1] = AnimationFlowTime[1];
			cbData->MAXTime[1] = game.HumanoidAnimationTable[anim1].Duration;
			cbData->animWeight[1] = 1.0f;
		}
		else {
			cbData->animTime[1] = 0.0f;
			cbData->MAXTime[1] = 1.0f;
			cbData->animWeight[1] = 0.0f;
		}
		cbData->animTime[2] = 0.0f; cbData->animTime[3] = 0.0f;
		cbData->MAXTime[2] = 1.0f; cbData->MAXTime[3] = 1.0f;
		cbData->animWeight[2] = 0.0f; cbData->animWeight[3] = 0.0f;

		cbData->animMask[0] = m_animMask[0];
		cbData->animMask[1] = m_animMask[1];
		cbData->animMask[2] = m_animMask[2];
		cbData->animMask[3] = m_animMask[3];

		cbData->frameRate = game.HumanoidAnimationTable[anim0].frameRate;
		cbData->upperBodyYawCorrection = GetPlayerUpperBodyYawCorrection(anim1);
		cbData->upperBodyPitchCorrection = 0.0f;
		Player* player = dynamic_cast<Player*>(this);
		if (player != nullptr &&
			(player->m_currentUpperState == Player::UpperState::AIM ||
			 player->m_currentUpperState == Player::UpperState::SHOOT)) {
			const float maxUpperBodyPitch = XMConvertToRadians(45.0f);
			const float upperBodyPitchBias = XMConvertToRadians(-10.0f);
			cbData->upperBodyPitchCorrection =
				max(-maxUpperBodyPitch, min(maxUpperBodyPitch,
					player->m_pitch + upperBodyPitchBias));
		}
	}

	if (BoneToWorldMatrixCB_Default.size() == 0) return;
	using ABSRPI = AnimationBlendingShader::RootParamId;
	//gd.CScmd.SetShader(game.MyAnimationBlendingShader);
	//gd.CScmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
	gd.CScmd.ResBarrierTr(&NodeLocalMatrixs, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	DescHandle AllocDescHandle;
	gd.ShaderVisibleDescPool.DynamicAlloc(&AllocDescHandle, 7);
	DescHandle tempDescHandle_CBVTable_CBStruct = AllocDescHandle[0];
	DescHandle tempDescHandle_SRVTable_Animation1to4 = AllocDescHandle[1];
	DescHandle tempDescHandle_SRVTable_HumanoidToNodeindex = AllocDescHandle[5];
	DescHandle tempDescHandle_UAVTable_Out_LocalMatrix = AllocDescHandle[6];

	gd.pDevice->CopyDescriptorsSimple(1, tempDescHandle_CBVTable_CBStruct.hcpu, AnimBlendingCBVDescIndex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	for (int i = 0;i < 4;++i) {
		int safeAnimIndex = PlayingAnimationIndex[i];

		if (safeAnimIndex < 0 || safeAnimIndex >= game.HumanoidAnimationTable.size()) {
			safeAnimIndex = 0;
		}
		gd.pDevice->CopyDescriptorsSimple(1, tempDescHandle_SRVTable_Animation1to4[i].hcpu, game.HumanoidAnimationTable[safeAnimIndex].AnimationDescIndex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	gd.pDevice->CopyDescriptorsSimple(1, tempDescHandle_SRVTable_HumanoidToNodeindex.hcpu, HumanoidToNodeIndexSRVIndex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	gd.pDevice->CopyDescriptorsSimple(1, tempDescHandle_UAVTable_Out_LocalMatrix.hcpu, NodeLocalMatrixs_UAVDescIndex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	gd.CScmd->SetComputeRootDescriptorTable(ABSRPI::CBVTable_CBStruct, tempDescHandle_CBVTable_CBStruct.hgpu);
	gd.CScmd->SetComputeRootDescriptorTable(ABSRPI::SRVTable_Animation1to4, tempDescHandle_SRVTable_Animation1to4.hgpu);
	gd.CScmd->SetComputeRootDescriptorTable(ABSRPI::SRVTable_HumanoidToNodeindex, tempDescHandle_SRVTable_HumanoidToNodeindex.hgpu);
	gd.CScmd->SetComputeRootDescriptorTable(ABSRPI::UAVTable_Out_LocalMatrix, tempDescHandle_UAVTable_Out_LocalMatrix.hgpu);

	//const float clear[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	//gd.CScmd->ClearUnorderedAccessViewFloat(tempDescHandle_UAVTable_Out_LocalMatrix.hgpu, NodeLocalMatrixs_UAVDescIndex.hCreation.hcpu, NodeLocalMatrixs.resource, clear, 0, nullptr);
	gd.CScmd->Dispatch(1, 1, 1); // numthread(32, 1, 1)
}

void SkinMeshGameObject::ModifyLocalToWorld() {
	if (BoneToWorldMatrixCB_Default.size() == 0) return;
	Model* model = shape.GetModel();
	if (model == nullptr) return;
	matrix rootworld = GetSkinMeshRenderWorld(model, worldMat);
	rootworld.transpose();
	for (int i = 0; i < model->mNumSkinMesh; ++i) {
		using HBLTWSRPI = HBoneLocalToWorldShader::RootParamId;
		//gd.CScmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
		gd.CScmd.ResBarrierTr(&NodeLocalMatrixs, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		gd.CScmd.ResBarrierTr(&BoneToWorldMatrixCB_Default[i], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		DescHandle AllocDescHandle;
		gd.ShaderVisibleDescPool.DynamicAlloc(&AllocDescHandle, 5);
		DescHandle tempDescHandle_SRVTable_LocalMatrixs = AllocDescHandle[0];
		DescHandle tempDescHandle_SRVTable_TPOSLocalTr = AllocDescHandle[1];
		DescHandle tempDescHandle_SRVTable_toParent = AllocDescHandle[2];
		DescHandle tempDescHandle_SRVTable_NodeToBoneIndex = AllocDescHandle[3];
		DescHandle tempDescHandle_UAVTable_Out_ToWorldMatrix = AllocDescHandle[4];

		gd.pDevice->CopyDescriptorsSimple(1, tempDescHandle_SRVTable_LocalMatrixs.hcpu, NodeLocalMatrixs_SRVDescIndex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		gd.pDevice->CopyDescriptorsSimple(1, tempDescHandle_SRVTable_TPOSLocalTr.hcpu, NodeTposMatrixs_SRVDescIndex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		gd.pDevice->CopyDescriptorsSimple(1, tempDescHandle_SRVTable_toParent.hcpu,
			NodeToParentSRVIndex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		gd.pDevice->CopyDescriptorsSimple(1, tempDescHandle_SRVTable_NodeToBoneIndex.hcpu, NodeToBone_SRVDescIndex[i].hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		gd.pDevice->CopyDescriptorsSimple(1, tempDescHandle_UAVTable_Out_ToWorldMatrix.hcpu, BoneToWorldMatrix_UAVDescIndex[i].hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		gd.CScmd->SetComputeRoot32BitConstants(HBLTWSRPI::Constant_WorldMat, 16, &rootworld, 0);
		gd.CScmd->SetComputeRootDescriptorTable(HBLTWSRPI::SRVTable_LocalMatrixs, tempDescHandle_SRVTable_LocalMatrixs.hgpu);
		gd.CScmd->SetComputeRootDescriptorTable(HBLTWSRPI::SRVTable_TPOSLocalTr, tempDescHandle_SRVTable_TPOSLocalTr.hgpu);
		gd.CScmd->SetComputeRootDescriptorTable(HBLTWSRPI::SRVTable_toParent, tempDescHandle_SRVTable_toParent.hgpu);
		gd.CScmd->SetComputeRootDescriptorTable(HBLTWSRPI::SRVTable_NodeToBoneIndex, tempDescHandle_SRVTable_NodeToBoneIndex.hgpu);
		gd.CScmd->SetComputeRootDescriptorTable(HBLTWSRPI::UAVTable_Out_ToWorldMatrix, tempDescHandle_UAVTable_Out_ToWorldMatrix.hgpu);
		gd.CScmd->Dispatch(1, 1, 1); // numthread(64, 1, 1)
		gd.CScmd.ResBarrierTr(&BoneToWorldMatrixCB_Default[i], D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	}
}

void SkinMeshGameObject::AnimationComputeDispatch(matrix parent)
{
	BlendingAnimation();
	ModifyLocalToWorld();
}

void SkinMeshGameObject::RecvSTC_SyncObj(char* data) {
	int offset = 0;
	STC_SyncObjData& stcsod = *(STC_SyncObjData*)(data);
	if (stcsod.shapeindex >= 0 && stcsod.shapeindex < (int)Shape::ShapeTable.size()) shape = Shape::ShapeTable[stcsod.shapeindex]; else shape.FlagPtr = 0;   // [crash-fix] out-of-range shapeindex -> null shape (avoids garbage Model pointer)
	tag = stcsod.tag;
	parent = (stcsod.parent >= 0) ? game.DynmaicGameObjects[stcsod.parent] : nullptr;
	childs = (stcsod.childs >= 0) ? game.DynmaicGameObjects[stcsod.childs] : nullptr;
	sibling = (stcsod.sibling >= 0) ? game.DynmaicGameObjects[stcsod.sibling] : nullptr;
	XMMatrixDecompose((XMVECTOR*)&DestScale, (XMVECTOR*)&DestRot, (XMVECTOR*)&DestPos, stcsod.DestWorld);
	LVelocity = stcsod.LVelocity;
	AnimationFlowTime[0] = stcsod.AnimationFlowTime;
	PlayingAnimationIndex[0] = stcsod.PlayingAnimationIndex;
	offset += sizeof(STC_SyncObjData);

	Mesh* mesh = nullptr;
	Model* model = nullptr;
	shape.GetRealShape(mesh, model);
	if (mesh) {
		int& materialCount = *(int*)(data + offset);
		offset += sizeof(int);
		if (material != nullptr) {
			delete[] material;
			material = nullptr;
		}
		material = new int[materialCount];
		for (int i = 0;i < materialCount;++i) {
			material[i] = *(int*)(data + offset);
			offset += sizeof(int);
		}
	}
	else {
		int& NodeCount = *(int*)(data + offset);
		offset += sizeof(int);
		if (transforms_innerModel != nullptr) {
			delete[] transforms_innerModel;
			transforms_innerModel = nullptr;
		}
		transforms_innerModel = new matrix[NodeCount];
		for (int i = 0;i < NodeCount;++i) {
			transforms_innerModel[i] = *(matrix*)(data + offset);
			offset += sizeof(matrix);
		}
	}

	SetShape(shape);
}

void SkinMeshGameObject::AnimationUpdate(float deltaTime) {
	if constexpr (gd.PlayAnimationByGPU) {
		if (AnimBlendingCB_Mapped) {
			AnimBlendingCB_Mapped->frameRate = game.HumanoidAnimationTable[0].frameRate;

			AnimBlendingCB_Mapped->animWeight[0] = 1.0f;
			AnimBlendingCB_Mapped->animWeight[1] = 0;
			AnimBlendingCB_Mapped->animWeight[2] = 0;
			AnimBlendingCB_Mapped->animWeight[3] = 0;

			AnimBlendingCB_Mapped->MAXTime[0] = game.HumanoidAnimationTable[0].Duration;
			AnimBlendingCB_Mapped->MAXTime[1] = 0;
			AnimBlendingCB_Mapped->MAXTime[2] = 0;
			AnimBlendingCB_Mapped->MAXTime[3] = 0;

			for (int i = 0;i < 4;++i) {
				AnimBlendingCB_Mapped->animMask[i] = 0xFFFFFFFFFFFFFFFF;
				AnimBlendingCB_Mapped->animTime[i] += deltaTime;
				if (AnimBlendingCB_Mapped->animTime[i] > AnimBlendingCB_Mapped->MAXTime[i]) {
					//Looping
					AnimBlendingCB_Mapped->animTime[i] -= AnimBlendingCB_Mapped->MAXTime[i];

					//One Shot
					//AnimBlendingCB_Mapped->animTime[i] = AnimBlendingCB_Mapped->MAXTime[i];
				}
			}
		}
	}
	else {
		//if (Mat == nullptr) continue;
		constexpr float frameSpeed = 1;
		Model* pModel = shape.GetModel();
		if (AnimationFlowTime[0] > game.HumanoidAnimationTable[PlayingAnimationIndex[0]].Duration) AnimationFlowTime[0] = 0;
		AnimationFlowTime[0] += deltaTime * frameSpeed;

		for (int i = 0; i < pModel->nodeCount; ++i) {
			transforms_innerModel[i].Id();
		}

		//gd.AverageSecPer60Start(Update_Monster_Animation_GetBoneLocalMatrixAtTime);
		GetBoneLocalMatrixAtTime(&game.HumanoidAnimationTable[PlayingAnimationIndex[0]], transforms_innerModel, AnimationFlowTime[0]);
		//gd.AverageSecPer60End(Update_Monster_Animation_GetBoneLocalMatrixAtTime);

		for (int i = 0; i < pModel->nodeCount; ++i) {
			transforms_innerModel[i] *= pModel->DefaultNodelocalTr[i];
		}
		transforms_innerModel[0] = worldMat;

		//gd.AverageSecPer60Start(Update_Monster_Animation_SetRootMatrixs);
		SetRootMatrixs();
		//gd.AverageSecPer60End(Update_Monster_Animation_SetRootMatrixs);
	}
}

void SkinMeshGameObject::CollectSkinMeshObject(matrix parent) {
	collection.push_back(this);
}

void SkinMeshGameObject::MoveChunck(const matrix& afterMat, const GameObjectIncludeChunks& beforeChunckInc, const GameObjectIncludeChunks& afterChunkInc) {
	static int temp[512] = {};
	GameObjectIncludeChunks intersection = beforeChunckInc;
	intersection &= afterChunkInc;

#ifdef ChunckDEBUG
	cout << "objptr = " << this << " FREE";
#endif

	int inter_Count = intersection.GetChunckSize();
	ChunkIndex inter_ci = ChunkIndex(intersection.xmin, intersection.ymin, intersection.zmin);
	inter_ci.extra = 0;
	int inter_up = 0;
	int chunckCount = beforeChunckInc.GetChunckSize();
	ChunkIndex ci = ChunkIndex(beforeChunckInc.xmin, beforeChunckInc.ymin, beforeChunckInc.zmin);
	ci.extra = 0;

	for (; ci.extra < chunckCount; beforeChunckInc.Inc(ci)) {
		if (ci == inter_ci && inter_Count > 0) { // 곂치는 부분을 Free 하지 않는다.
			intersection.Inc(inter_ci);
			temp[inter_up] = chunkAllocIndexs[ci.extra];
			inter_up += 1;

			continue;
		}

		//if (Mat == nullptr) continue;
		auto f = game.Current_Zone->chunck.find(ci);
		if (f != game.Current_Zone->chunck.end()) {
#ifdef ChunckDEBUG
			dbgbreak(f->second->SkinMesh_gameobjects.isnull(chunkAllocIndexs[ci.extra]));
			cout << "ci(" << ci.x << ", " << ci.y << ", " << ci.z << ") : " << chunkAllocIndexs[ci.extra] << "\t";
#endif
			f->second->SkinMesh_gameobjects.Free(chunkAllocIndexs[ci.extra]);
		}
	}
#ifdef ChunckDEBUG
	cout << endl;
#endif

#ifdef ChunckDEBUG
	dbgbreak(inter_Count != inter_ci.extra);
#endif

	//if (Mat == nullptr) continue;
	worldMat = afterMat;

	inter_ci = ChunkIndex(intersection.xmin, intersection.ymin, intersection.zmin);
	inter_ci.extra = 0;

	chunckCount = afterChunkInc.GetChunckSize();
	ci = ChunkIndex(afterChunkInc.xmin, afterChunkInc.ymin, afterChunkInc.zmin);
	ci.extra = 0;

	ChunkIndex pastCI = ChunkIndex(-99999, -99999, -99999);

#ifdef ChunckDEBUG
	cout << "objptr = " << this << " ALLOC";
#endif
	inter_up = 0;
	for (; ci.extra < chunckCount; afterChunkInc.Inc(ci)) {
		if (ci == inter_ci && inter_Count > 0) { // 곂치는 부분을 Free 하지 않는다.
			intersection.Inc(inter_ci);
			chunkAllocIndexs[ci.extra] = temp[inter_up];
			inter_up += 1;
#ifdef ChunckDEBUG
			cout << "ci_move(" << ci.x << ", " << ci.y << ", " << ci.z << ") : " << temp[inter_up - 1] << "\t";
			dbgbreak(pastCI == ci);
			dbgbreak(afterChunkInc.isInclude(ci) == false);
			pastCI = ci;
#endif
			continue;
		}

		auto c = game.Current_Zone->chunck.find(ci);
		GameChunk* gc;
		if (c == game.Current_Zone->chunck.end()) {
			// new game chunk
			gc = new GameChunk();
			gc->SetChunkIndex(ci);
			game.Current_Zone->chunck.insert(pair<ChunkIndex, GameChunk*>(ci, gc));
		}
		else gc = c->second;
		int allocN = gc->SkinMesh_gameobjects.Alloc();
		gc->SkinMesh_gameobjects[allocN] = this;
		chunkAllocIndexs[ci.extra] = allocN;

#ifdef ChunckDEBUG
		cout << "ci(" << ci.x << ", " << ci.y << ", " << ci.z << ") : " << allocN << "\t";
#endif
	}
#ifdef ChunckDEBUG
	cout << endl;
#endif

	IncludeChunks = afterChunkInc;
}

void SkinMeshGameObject::Release() {
	RootBoneMatrixs_PerSkinMesh.clear();
	for (int i = 0; i < BoneToWorldMatrixCB.size(); ++i) {
		BoneToWorldMatrixCB[i].Release();
	}
	BoneToWorldMatrixCB.clear();

	for (int i = 0; i < BoneToWorldMatrixCB_Default.size(); ++i) {
		BoneToWorldMatrixCB_Default[i].Release();
	}
	BoneToWorldMatrixCB_Default.clear();

	for (int i = 0; i < NodeToBone.size(); ++i) {
		NodeToBone[i].Release();
	}
	NodeToBone.clear();

	for (int i = 0; i < BoneToWorldMatrix_UAVDescIndex.size(); ++i) {
		DescIndex& di = BoneToWorldMatrix_UAVDescIndex[i];
		if (di.type == 'n' && di.isShaderVisible == false) {
			gd.DynamicDescriptorAllotter.Free(di.index);
		}
	}
	BoneToWorldMatrix_UAVDescIndex.clear();

	for (int i = 0; i < NodeToBone_SRVDescIndex.size(); ++i) {
		DescIndex& di = NodeToBone_SRVDescIndex[i];
		if (di.type == 'n' && di.isShaderVisible == false) {
			gd.DynamicDescriptorAllotter.Free(di.index);
		}
	}
	NodeToBone_SRVDescIndex.clear();

	Mesh* mesh = nullptr;
	Model* model = nullptr;
	shape.GetRealShape(mesh, model);
	if (model) {
		for (int i = 0; i < model->mNumSkinMesh; ++i) {
			DescIndex& di = OutVertexUAV[i];
			if (di.type == 'n' && di.isShaderVisible == false) {
				gd.DynamicDescriptorAllotter.Free(di.index);
			}
		}

		for (int i = 0; i < model->mNumSkinMesh; ++i) {
			RayTracingMesh& rmesh = modifyMeshes[i];
			rmesh.Release();
		}
	}
	delete[] OutVertexUAV;
	OutVertexUAV = nullptr;
	delete[] modifyMeshes;

	if (chunkAllocIndexs) {
		int xmax = IncludeChunks.xmin + IncludeChunks.xlen;
		int ymax = IncludeChunks.ymin + IncludeChunks.ylen;
		int zmax = IncludeChunks.zmin + IncludeChunks.zlen;
		int up = 0;
		for (int ix = IncludeChunks.xmin; ix <= xmax; ++ix) {
			for (int iy = IncludeChunks.ymin; iy <= ymax; ++iy) {
				for (int iz = IncludeChunks.zmin; iz <= zmax; ++iz) {
					auto chunkIt = game.Current_Zone->chunck.find(ChunkIndex(ix, iy, iz));
					if (chunkIt != game.Current_Zone->chunck.end() && chunkIt->second != nullptr) {
						chunkIt->second->SkinMesh_gameobjects.Free(chunkAllocIndexs[up]);
					}
					up += 1;
				}
			}
		}
		delete[] chunkAllocIndexs;
		chunkAllocIndexs = nullptr;
	}

	GameObject::Release();
}

Monster::Monster() {
	tag = 0;
	tag[GameObjectTag::Tag_Enable] = false;
	tag[GameObjectTag::Tag_Dynamic] = true;
	tag[GameObjectTag::Tag_SkinMeshObject] = true;
	worldMat.Id();
	HP = 30;
	MaxHP = 30;
	Defense = 0;
	isDead = false;
	HPBarIndex = 0;
	HPMatrix.Id();
}

void Monster::STATICINIT(int typeindex) {
	SkinMeshGameObject::STATICINIT(typeindex);
	for (int i = 0; i < Monster::g_member.size();++i) {
		g_member[i].offset = g_member[i].get_offset();
		MemberInfo& mi = *(MemberInfo*)&g_member[i];
		int lastindex = GameObjectType::Client_STCMembers[typeindex].size();
		GameObjectType::Client_STCMembers[typeindex].emplace_back(mi.name, mi.get_offset, mi.size);
		GameObjectType::Client_STCMembers[typeindex][lastindex].offset = g_member[i].offset;
	}
}

void Monster::Update(float deltaTime)
{
	PositionInterpolation(deltaTime);
	if (HitFlashTimer > 0.0f) {
		HitFlashTimer -= deltaTime;
		if (HitFlashTimer < 0.0f) HitFlashTimer = 0.0f;
	}

	if (isDead)
	{
		//ChangeState(State::DEATH);
	}
	else
	{
		float velSpeed = sqrt(LVelocity.x * LVelocity.x + LVelocity.z * LVelocity.z);
		float dx = DestPos.x - worldMat.pos.x;
		float dz = DestPos.z - worldMat.pos.z;
		float distSpeed = sqrt(dx * dx + dz * dz);
		float currentSpeed = max(velSpeed, distSpeed);

		if (m_currentState != State::ATTACK)
		{
			if (currentSpeed > 3.0f) ChangeState(State::RUN);
			else if (currentSpeed > 0.05f) ChangeState(State::WALK);
			else ChangeState(State::IDLE);
		}
	}

	if (PlayingAnimationIndex[0] != -1)
	{
		//dbgc[10] += 1;
		//dbgbreak(dbgc[10] == 353);

		AnimationFlowTime[0] += deltaTime;
		double currentDuration = game.HumanoidAnimationTable[PlayingAnimationIndex[0]].Duration;

		if (AnimationFlowTime[0] > currentDuration)
		{
			if (m_currentState == State::DEATH) {
				AnimationFlowTime[0] = currentDuration - 0.001;
			}
			else if (m_currentState == State::ATTACK) {
				AnimationFlowTime[0] = fmod(AnimationFlowTime[0], currentDuration);
				ChangeState(State::IDLE);
			}
			else {
				AnimationFlowTime[0] = fmod(AnimationFlowTime[0], currentDuration);
			}
		}
	}

	m_animMask[0] = 0xFFFFFFFFFFFFFFFFULL;
	m_animMask[1] = 0ULL;
	m_animMask[2] = 0ULL;
	m_animMask[3] = 0ULL;

	if (this->HPBarIndex >= 0)
	{
		matrix viewMatrix = XMLoadFloat4x4((XMFLOAT4X4*)&gd.viewportArr[0].ViewMatrix);
		matrix invViewMatrix = viewMatrix.RTInverse;
		matrix cameraWorldMat = invViewMatrix;

		matrix hpBarWorldMat;
		hpBarWorldMat.Id();
		hpBarWorldMat.LookAt(cameraWorldMat.right);
		hpBarWorldMat.pos = this->worldMat.pos + vec4(0.0f, 1.0f, 0.0f, 0);

		float currentHP = max(HP, 0.0f);
		hpBarWorldMat.look *= 2 * currentHP / MaxHP;

		game.NpcHPBars[this->HPBarIndex] = hpBarWorldMat;
	}

	//gd.AverageSecPer60Start(Update_Monster_Animation);
	AnimationUpdate(deltaTime);
	//if (AnimationFlowTime < 1) {
	//	AnimationUpdate(deltaTime);
	//}
	//gd.AverageSecPer60End(Update_Monster_Animation);

}

void Monster::Render(matrix parent)
{
	if (isDead) {
		return;
	}

	SkinMeshGameObject::Render(parent);
}

void Monster::Init(const XMMATRIX& initialWorldMatrix)
{
	worldMat = (initialWorldMatrix);
	//m_homePos = worldMat.pos;
}

void Monster::RecvSTC_SyncObj(char* data) {
	int offset = 0;
	STC_SyncObjData& stcsod = *(STC_SyncObjData*)(data);
	if (stcsod.shapeindex >= 0 && stcsod.shapeindex < (int)Shape::ShapeTable.size()) shape = Shape::ShapeTable[stcsod.shapeindex]; else shape.FlagPtr = 0;   // [crash-fix] out-of-range shapeindex -> null shape (avoids garbage Model pointer)
	tag = stcsod.tag;
	//dbglog1(L"parent : %d \n", stcsod.parent);
	parent = (stcsod.parent >= 0) ? game.DynmaicGameObjects[stcsod.parent] : nullptr;
	childs = (stcsod.childs >= 0) ? game.DynmaicGameObjects[stcsod.childs] : nullptr;
	sibling = (stcsod.sibling >= 0) ? game.DynmaicGameObjects[stcsod.sibling] : nullptr;
	XMMatrixDecompose((XMVECTOR*)&DestScale, (XMVECTOR*)&DestRot, (XMVECTOR*)&DestPos, stcsod.DestWorld);
	LVelocity = stcsod.LVelocity;
	AnimationFlowTime[0] = stcsod.AnimationFlowTime;
	PlayingAnimationIndex[0] = stcsod.PlayingAnimationIndex;
	float prevHP = HP;
	HP = stcsod.HP;
	MaxHP = stcsod.MaxHP;
	Defense = stcsod.Defense;
	isDead = stcsod.isDead;
	if (HP < prevHP && !isDead) {
		TriggerHitFlash(1.4f);
		game.SpawnFloatingDamageText(worldMat.pos, prevHP - HP);
		game.SpawnSkillEffect(SkillEffectType::Blood_Hit, worldMat.pos + vec4(0.0f, 1.0f, 0.0f, 0.0f),
			vec4(0, 1, 0, 0), 0, 1.25f, max(prevHP - HP, 1.0f), 0.45f);
	}
	offset += sizeof(STC_SyncObjData);

	Mesh* mesh = nullptr;
	Model* model = nullptr;
	shape.GetRealShape(mesh, model);
	if (mesh) {
		int& materialCount = *(int*)(data + offset);
		offset += sizeof(int);
		if (material != nullptr) {
			delete[] material;
			material = nullptr;
		}
		material = new int[materialCount];
		for (int i = 0;i < materialCount;++i) {
			material[i] = *(int*)(data + offset);
			offset += sizeof(int);
		}
	}
	else {
		int& NodeCount = *(int*)(data + offset);
		offset += sizeof(int);
		if (transforms_innerModel != nullptr) {
			delete[] transforms_innerModel;
			transforms_innerModel = nullptr;
		}
		transforms_innerModel = new matrix[NodeCount];
		for (int i = 0;i < NodeCount;++i) {
			transforms_innerModel[i] = *(matrix*)(data + offset);
			offset += sizeof(matrix);
		}
	}

	SetShape(shape);
}

void Monster::SyncHP(GameObject* go, char* data, int len)
{
	if (go == nullptr || data == nullptr || len < sizeof(float)) return;
	Monster* monster = (Monster*)go;
	float newHP = *(float*)data;
	if (newHP < monster->HP && !monster->isDead) {
		monster->TriggerHitFlash(1.4f);
		game.SpawnFloatingDamageText(monster->worldMat.pos, monster->HP - newHP);
		game.SpawnSkillEffect(SkillEffectType::Blood_Hit, monster->worldMat.pos + vec4(0.0f, 1.0f, 0.0f, 0.0f),
			vec4(0, 1, 0, 0), 0, 1.25f, max(monster->HP - newHP, 1.0f), 0.45f);
	}
	monster->HP = newHP;
}

void Monster::ChangeState(State newState)
{
	if (m_currentState == newState) return;

	m_currentState = newState;
	AnimationFlowTime[0] = 0.0f;

	switch (m_currentState) {
	case State::IDLE:
		PlayingAnimationIndex[0] = 0;
		break;
	case State::WALK:
		PlayingAnimationIndex[0] = 1;
		break;
	case State::RUN:
		PlayingAnimationIndex[0] = 2;
		break;
		//if (Mat == nullptr) continue;
	case State::DEATH:
		PlayingAnimationIndex[0] = 6;
		break;
	}
}

void Monster::Release() {
	SkinMeshGameObject::Release();
}

static int GetPlayerUpperAnimationIndex(WeaponType weaponType, Player::UpperState upperState)
{
	if (weaponType == WeaponType::DualPistol) {
		if (upperState == Player::UpperState::SHOOT) return 11;
		if (upperState == Player::UpperState::AIM) return 10;
		if (upperState == Player::UpperState::RELOAD) return 9;
		return -1;
	}

	const bool usePistolAnimation =
		weaponType == WeaponType::Pistol ||
		weaponType == WeaponType::DronePistol ||
		weaponType == WeaponType::SMG ||
		weaponType == WeaponType::GrenadeGun;
	if (upperState == Player::UpperState::SHOOT) {
		return usePistolAnimation ? 6 : 4;
	}
	if (upperState == Player::UpperState::AIM) {
		return usePistolAnimation ? 5 : 3;
	}
	if (upperState == Player::UpperState::RELOAD) {
		return usePistolAnimation ? 9 : 8;
	}
	return -1;
}

static float GetPlayerUpperBodyYawCorrection(int animationIndex)
{
	if (animationIndex == 3 || animationIndex == 4) {
		return XMConvertToRadians(50.0f);
	}
	if (animationIndex == 5 || animationIndex == 6) {
		return XMConvertToRadians(20.0f);
	}
	return 0.0f;
}

Player::Player() : HP{ 100 } {
	tag = 0;
	tag[GameObjectTag::Tag_Enable] = false;
	tag[GameObjectTag::Tag_Dynamic] = true;
	tag[GameObjectTag::Tag_SkinMeshObject] = true;
	worldMat.Id();
	weapon[0] = Weapon(WeaponType::Max);
	weapon[1] = Weapon(WeaponType::Max);
	weapon[2] = Weapon(WeaponType::Max);
	SelectedWeapon = 0;
	
	HP = 100;
	MaxHP = 100;
	Attack = 0;
	Defense = 0;
	bullets = 100;
	KillCount = 0;
	DeathCount = 0;
	HeatGauge = 0;
	MaxHeatGauge = 200;

	ShieldDurability = 0;
	MaxShieldDurability = 0;
	m_currentJob = (int)PlayerJob::Healer;
	for (int i = 0; i < (int)SkillSlot::Max; ++i) {
		SkillCooldown[i] = 0.0f;
		SkillCooldownFlow[i] = 0.0f;
	}
	//HealSkillCooldown = 10.0f;
	//HealSkillCooldownFlow = 0;
	m_currentWeaponType = (int)WeaponType::Max;
	//ZeroMemory(Inventory, sizeof(ItemStack) * maxItem);

	DeltaMousePos = 0;
	GunModel = nullptr;
	ShootPointMesh = nullptr;
	gunMatrix_thirdPersonView.Id();
	cachedThirdPersonRightHandMatrix.Id();
	cachedThirdPersonRightHandMatrixValid = false;
	cachedThirdPersonLeftHandMatrix.Id();
	cachedThirdPersonLeftHandMatrixValid = false;
	cachedRightHandForwardAxisIndex = -1;
	cachedRightHandRightAxisIndex = -1;
	cachedRightHandForwardAxisSign = 1.0f;
	cachedRightHandRightAxisSign = 1.0f;
	cachedThirdPersonWeaponMatrix.Id();
	cachedThirdPersonWeaponMatrixValid = false;
	cachedThirdPersonLeftWeaponMatrix.Id();
	cachedThirdPersonLeftWeaponMatrixValid = false;
	cachedThirdPersonWeaponType = -1;
	gunMatrix_firstPersonView.Id();
	HPBarMesh = nullptr;
	HPMatrix.Id();
	HeatBarMesh = nullptr;
	HeatBarMatrix.Id();
	gunBarrelAngle = 0;
	gunBarrelSpeed = 0;
	m_isZooming = false;
	m_currentFov = 60.0f;
	m_targetFov = 60.0f;
	m_yaw = 0.0f;
	m_pitch = 0.0f;
	m_currentUpperState = UpperState::AIM;
	m_jumpPhase = JumpPhase::NONE;
	m_jumpElapsed = 0.0f;
	m_jumpGroundStableTime = 0.0f;
	m_jumpReentryCooldown = 0.0f;
	m_jumpSawDescending = false;
	PlayingAnimationIndex[1] = 5;
	AnimationFlowTime[1] = 0.0f;
	m_upperAimPoseTime = 0.0f;
}

static Model* GetWeaponRenderModel(const Player* player)
{
	if (player == nullptr) return nullptr;
	const WeaponType weaponType = (WeaponType)player->m_currentWeaponType;
	switch (weaponType)
	{
	case WeaponType::Sniper: return game.SniperModel;
	case WeaponType::MachineGun: return game.MachineGunModel;
	case WeaponType::Shotgun: return game.ShotGunModel;
	case WeaponType::Rifle: return game.RifleModel;
	case WeaponType::Pistol: return game.PistolModel;
	case WeaponType::DualPistol: return player->m_dualBladeVisualTimer > 0.0f ? game.DualGunBladeModel : game.DualRevolverModel;
	case WeaponType::DronePistol: return game.PistolModel;
	case WeaponType::SMG: return game.HackerSMGModel;
	case WeaponType::GrenadeGun: return game.BomberGrenadeGunModel;
	default: return nullptr;
	}
}

static float GetPlayerReloadTime(const Player* player)
{
	if ((WeaponType)player->m_currentWeaponType == WeaponType::Sniper) {
		return player->m_sniperDmrMode ? 1.4f : 2.0f;
	}
	return player->weapon[player->SelectedWeapon].m_info.reloadTime;
}

Player::~Player() {
}

void Player::STATICINIT(int typeindex) {
	SkinMeshGameObject::STATICINIT(typeindex);
	for (int i = 0; i < Player::g_member.size();++i) {
		g_member[i].offset = g_member[i].get_offset();
		MemberInfo& mi = *(MemberInfo*)&g_member[i];
		int lastindex = GameObjectType::Client_STCMembers[typeindex].size();
		GameObjectType::Client_STCMembers[typeindex].emplace_back(mi.name, mi.get_offset, mi.size);
		GameObjectType::Client_STCMembers[typeindex][lastindex].offset = g_member[i].offset;
	}
}

void Player::Update(float deltaTime)
{
	if (game.player != this && gd.isRaytracingRender) {
		Render_ThirdPersonWeapon();
	}

	vec4 previousPosition = worldMat.pos;
	PositionInterpolation(deltaTime);
	if (HitFlashTimer > 0.0f) {
		HitFlashTimer -= max(0.0f, deltaTime);
		if (HitFlashTimer < 0.0f) HitFlashTimer = 0.0f;
	}

	float velSpeed = sqrt(LVelocity.x * LVelocity.x + LVelocity.z * LVelocity.z);
	float movedX = worldMat.pos.x - previousPosition.x;
	float movedY = worldMat.pos.y - previousPosition.y;
	float movedZ = worldMat.pos.z - previousPosition.z;
	float interpolatedSpeed = sqrt(movedX * movedX + movedZ * movedZ) / max(deltaTime, 0.0001f);
	float verticalSpeed = movedY / max(deltaTime, 0.0001f);
	float currentSpeed = max(velSpeed, interpolatedSpeed);
	const float runEnterSpeed = 7.0f;
	const float runExitSpeed = 6.5f;

	auto changeGroundLocomotion = [&]() {
		if (currentSpeed >= runEnterSpeed ||
			(m_currentState == State::RUN && currentSpeed > runExitSpeed)) {
			ChangeState(State::RUN);
		}
		else if (currentSpeed > 0.10f) {
			ChangeState(State::WALK);
		}
		else {
			ChangeState(State::IDLE);
		}
	};

	if (m_jumpReentryCooldown > 0.0f) {
		m_jumpReentryCooldown -= deltaTime;
		if (m_jumpReentryCooldown < 0.0f) m_jumpReentryCooldown = 0.0f;
	}

	const bool hasVerticalAirMotion =
		abs(LVelocity.y) > 0.15f || abs(verticalSpeed) > 1.00f;
	if (m_jumpPhase == JumpPhase::NONE) {
		if (m_jumpReentryCooldown <= 0.0f && hasVerticalAirMotion) {
			ChangeState(State::JUMP);
			m_jumpPhase = JumpPhase::TAKEOFF;
			m_jumpElapsed = 0.0f;
			m_jumpGroundStableTime = 0.0f;
			m_jumpSawDescending = LVelocity.y < -0.80f || verticalSpeed < -0.80f;
		}
		else {
			changeGroundLocomotion();
		}
	}
	else {
		ChangeState(State::JUMP);
		m_jumpElapsed += deltaTime;
		if (LVelocity.y < -0.80f || verticalSpeed < -0.80f) {
			m_jumpSawDescending = true;
		}

		const bool canLand = m_jumpElapsed > 0.18f &&
			(m_jumpSawDescending || m_jumpElapsed > 0.90f);
		const bool nearlyStoppedVertically =
			abs(LVelocity.y) < 0.65f && abs(verticalSpeed) < 0.50f;
		const bool staleVelocityFallback =
			m_jumpElapsed > 1.50f && abs(verticalSpeed) < 0.50f;
		const bool generousGroundContact =
			(isGround && abs(LVelocity.y) < 0.80f) ||
			nearlyStoppedVertically || staleVelocityFallback;
		if (m_jumpPhase != JumpPhase::LANDING && canLand && generousGroundContact) {
			m_jumpGroundStableTime += deltaTime;
			if (m_jumpGroundStableTime >= 0.08f) {
				m_jumpPhase = JumpPhase::LANDING;
			}
		}
		else if (m_jumpPhase != JumpPhase::LANDING) {
			m_jumpGroundStableTime = 0.0f;
		}
	}

	double currentDuration = game.HumanoidAnimationTable[PlayingAnimationIndex[0]].Duration;
	if (m_currentState == State::JUMP && m_jumpPhase != JumpPhase::NONE) {
		const float jumpDuration = max((float)currentDuration, 0.01f);
		const float airPoseTime = jumpDuration * 0.50f;
		const float landingStartTime = jumpDuration * 0.62f;
		if (m_jumpPhase == JumpPhase::TAKEOFF) {
			AnimationFlowTime[0] += deltaTime * (airPoseTime / 0.22f);
			if (AnimationFlowTime[0] >= airPoseTime) {
				AnimationFlowTime[0] = airPoseTime;
				m_jumpPhase = JumpPhase::AIRBORNE;
			}
		}
		else if (m_jumpPhase == JumpPhase::AIRBORNE) {
			AnimationFlowTime[0] = airPoseTime;
		}
		else if (m_jumpPhase == JumpPhase::LANDING) {
			if (AnimationFlowTime[0] < landingStartTime) {
				AnimationFlowTime[0] = landingStartTime;
			}
			AnimationFlowTime[0] += deltaTime * ((jumpDuration - landingStartTime) / 0.20f);
			if (AnimationFlowTime[0] >= jumpDuration) {
				m_jumpPhase = JumpPhase::NONE;
				m_jumpElapsed = 0.0f;
				m_jumpGroundStableTime = 0.0f;
				m_jumpReentryCooldown = 0.12f;
				m_jumpSawDescending = false;
				changeGroundLocomotion();
			}
		}
	}
	else {
		AnimationFlowTime[0] += deltaTime;
	}

	if (m_currentState != State::JUMP && AnimationFlowTime[0] > currentDuration) {
		AnimationFlowTime[0] = fmod(AnimationFlowTime[0], currentDuration);
	}

	// A holstered weapon uses the original full-body locomotion animation.
	// Equipping restores the weapon-specific aim-idle upper pose.
	const bool isReloading = ReloadRemain > 0.0f;
	if (m_weaponHolstered) {
		m_upperShootHoldTimer = 0.0f;
		ChangeUpperState(UpperState::NONE);
	}
	else if (isReloading) {
		m_upperShootHoldTimer = 0.0f;
		ChangeUpperState(UpperState::RELOAD);
	}
	else if (m_currentUpperState == UpperState::RELOAD) {
		ChangeUpperState(UpperState::AIM);
	}
	else if (m_currentUpperState == UpperState::NONE) {
		ChangeUpperState(UpperState::AIM);
	}
	else {
		ChangeUpperState(m_currentUpperState);
	}
	if (m_upperShootHoldTimer > 0.0f) {
		m_upperShootHoldTimer -= deltaTime;
		if (m_upperShootHoldTimer < 0.0f) m_upperShootHoldTimer = 0.0f;
	}

	if (m_currentUpperState != UpperState::NONE && PlayingAnimationIndex[1] != -1)
	{
		double upperDuration = game.HumanoidAnimationTable[PlayingAnimationIndex[1]].Duration;
		if (m_currentUpperState == UpperState::RELOAD) {
			const float reloadTime = max(GetPlayerReloadTime(this), 0.01f);
			const float reloadProgress = min(1.0f, max(0.0f,
				1.0f - ReloadRemain / reloadTime));
			// The imported reload clips have a large hand offset in their first
			// frames. Skip that unstable lead-in while preserving reload timing.
			constexpr float reloadAnimationStartRatio = 0.08f;
			const float reloadAnimationProgress = reloadAnimationStartRatio +
				(1.0f - reloadAnimationStartRatio) * reloadProgress;
			AnimationFlowTime[1] = (float)upperDuration * reloadAnimationProgress;
		}
		else {
			AnimationFlowTime[1] += deltaTime;
		}

		if (AnimationFlowTime[1] > upperDuration) {
			if (m_currentUpperState == UpperState::AIM) {
				AnimationFlowTime[1] = fmod(AnimationFlowTime[1], upperDuration);
			}
			else if (m_currentUpperState == UpperState::SHOOT) {
				if (m_upperShootHoldTimer > 0.0f) {
					AnimationFlowTime[1] = fmod(AnimationFlowTime[1], upperDuration);
				}
				else {
					ChangeUpperState(UpperState::AIM);
				}
			}
			else if (m_currentUpperState == UpperState::RELOAD) {
				AnimationFlowTime[1] = (float)upperDuration;
			}
		}

		if (m_currentUpperState == UpperState::AIM) {
			m_upperAimPoseTime = AnimationFlowTime[1];
		}
	}

	if (m_currentUpperState == UpperState::NONE) {
		m_animMask[0] = 0xFFFFFFFFFFFFFFFFULL;
		m_animMask[1] = 0ULL;
	}
	else {
		m_animMask[0] = 0x7FULL;  // 0~6: lower body
		m_animMask[1] = ~0x7FULL; // 7+: upper body
	}

	AnimationUpdate(deltaTime);

	if (game.bFirstPersonVision) {
		// �÷��̾ ������ ��.

	}
	else {
		// ���� BLAS�� �־�� ��.
	}
}

void Player::SetShape(Shape _shape, int ZoneID) {
	SkinMeshGameObject::SetShape(_shape, ZoneID);
}

void Player::ClientUpdate(float deltaTime)
{
	if (m_dualBladeVisualTimer > 0.0f) {
		m_dualBladeVisualTimer = max(0.0f, m_dualBladeVisualTimer - deltaTime);
	}
	if (m_dualBladeAttackVisualTimer > 0.0f) {
		m_dualBladeAttackVisualTimer = max(0.0f, m_dualBladeAttackVisualTimer - deltaTime);
	}
	m_dualLeftRecoilTimer = max(0.0f, m_dualLeftRecoilTimer - deltaTime);
	m_dualRightRecoilTimer = max(0.0f, m_dualRightRecoilTimer - deltaTime);
	m_droneAssaultVisualTimer = max(0.0f, m_droneAssaultVisualTimer - deltaTime);
	m_droneFlightVisualTimer = max(0.0f, m_droneFlightVisualTimer - deltaTime);
	if (ReloadRemain > 0.0f) {
		ReloadRemain = max(0.0f, ReloadRemain - deltaTime);
	}

	Weapon& currentWeapon = weapon[SelectedWeapon];
	if ((int)currentWeapon.m_info.type != m_currentWeaponType)
	{
		currentWeapon = Weapon((WeaponType)m_currentWeaponType);
	}

	currentWeapon.Update(deltaTime);

	if (game.player == this) {
		float recoilT = currentWeapon.GetRecoilAlpha();
		if (recoilT > 0.7f) {
			float f = currentWeapon.m_info.recoilVelocity * 10.0f * (recoilT - 0.7f) * deltaTime;
			game.DeltaMousePos.y -= f;
		}

		if (!m_weaponHolstered && (GetAsyncKeyState(VK_RBUTTON) & 0x8000)) m_isZooming = true;
		else m_isZooming = false;

		float targetFov = 60.0f;
		if (m_isZooming) {
			if ((WeaponType)m_currentWeaponType == WeaponType::Sniper) targetFov = 9.0f;
			else if ((WeaponType)m_currentWeaponType == WeaponType::Rifle) targetFov = 30.0f;
		}

		m_currentFov += (targetFov - m_currentFov) * 15.0f * deltaTime;
		gd.viewportArr[0].ProjectMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(m_currentFov),
			(float)gd.ClientFrameWidth / (float)gd.ClientFrameHeight, 0.01f, 1000.0f);
	}

	gunMatrix_firstPersonView.Id();

	WeaponType currentType = currentWeapon.m_info.type;

	switch (currentType)
	{
	case WeaponType::Pistol:
	case WeaponType::DualPistol:
	case WeaponType::DronePistol:
	case WeaponType::SMG:
	{
		float recoilT = currentWeapon.GetRecoilAlpha();

		float zOffset = 0.3f - (0.0f * powf(recoilT, 3.0f));
		float pitchAngle = 15.0f * powf(recoilT, 2.0f);
		gunMatrix_firstPersonView.mat = XMMatrixRotationX(-XMConvertToRadians(pitchAngle)) * XMMatrixTranslation(0.0f, 0.02f * recoilT, zOffset);

		float slideMove = -0.3f * powf(recoilT, 2.0f);

		if (game.PistolModel && !game.Pistol_SlideIndices.empty()) {
			XMMATRIX slideTrans = XMMatrixTranslation(0.0f, 0.0f, slideMove);

			for (int idx : game.Pistol_SlideIndices) {
				game.PistolModel->Nodes[idx].transform = game.PistolModel->BindPose[idx] * slideTrans;
			}
		}
	}
	break;

	case WeaponType::GrenadeGun:
	{
		float recoilT = weapon[SelectedWeapon].GetRecoilAlpha();
		float zOffset = 0.3f - 0.24f * powf(recoilT, 2.0f);
		float pitchAngle = weapon[SelectedWeapon].m_info.recoilVelocity * recoilT;
		gunMatrix_firstPersonView.mat = XMMatrixRotationX(-XMConvertToRadians(pitchAngle)) *
			XMMatrixTranslation(0.0f, 0.0f, zOffset);
	}
	break;

	case WeaponType::MachineGun:
	{
		float shootrate = powf(currentWeapon.m_shootFlow / currentWeapon.m_info.shootDelay, 3);
		if (shootrate > 1.0f) shootrate = 1.0f;

		float recoilAmount = 1.0f - shootrate;
		gunMatrix_firstPersonView.pos.z = 0.3f + 0.02f * recoilAmount;

		//if (Mat == nullptr) continue;
		gunBarrelSpeed = 150.0f;
		if (shootrate < 1.0f) {
			float spinRate = powf(1.0f - shootrate, 2);
			gunBarrelAngle += gunBarrelSpeed * spinRate * deltaTime;
			if (gunBarrelAngle > XM_2PI) gunBarrelAngle -= XM_2PI;
		}
		UpdateGunBarrelNodes();
	}
	break;

	case WeaponType::Sniper:
	{
		float recoilT = currentWeapon.GetRecoilAlpha();
		float zOffset = 0.3f;
		float pitchAngle = 0.0f;

		if (recoilT > 0.70f) {
			float stageT = (recoilT - 0.70f) / 0.30f;
			float smoothT = sinf(stageT * XM_PIDIV2);
			pitchAngle = currentWeapon.m_info.recoilVelocity * smoothT;
			zOffset -= 0.2f * smoothT;
		}
		else if (recoilT > 0.60f) {

		}
		else {
			float stageT = recoilT / 0.60f;
			float wobble = sinf(stageT * XM_PI * 2.0f) * 0.08f;
			zOffset += wobble * stageT;
		}

		XMMATRIX rotMat = XMMatrixRotationX(-XMConvertToRadians(pitchAngle));
		XMMATRIX transMat = XMMatrixTranslation(0.0f, 0.0f, zOffset);
		gunMatrix_firstPersonView.mat = rotMat * transMat;
	}
	break;

	case WeaponType::Shotgun:
	{
		float currentFlow = currentWeapon.m_shootFlow;
		float shootDelay = currentWeapon.m_info.shootDelay;

		float recoilT = currentWeapon.GetRecoilAlpha();
		float zOffset = 0.3f;
		float pitchAngle = 0.0f;

		if (recoilT > 0.0f) {
			zOffset -= 0.25f * powf(recoilT, 2.0f);
			pitchAngle = currentWeapon.m_info.recoilVelocity * recoilT;
		}
		gunMatrix_firstPersonView.mat = XMMatrixRotationX(-XMConvertToRadians(pitchAngle)) * XMMatrixTranslation(0.0f, 0.0f, zOffset);

		float pumpMove = 0.0f;
		float pumpStart = 0.3f;
		float pumpEnd = 0.7f;

		if (currentFlow > pumpStart && currentFlow < pumpEnd) {
			float t = (currentFlow - pumpStart) / (pumpEnd - pumpStart);

			float curve = sinf(t * XM_PI);

			pumpMove = 0.35f * curve;
		}

		if (game.ShotGunModel && !game.SG_PumpIndices.empty()) {
			XMMATRIX pumpTrans = XMMatrixTranslation(pumpMove, 0.0f, 0.0f);
			for (int idx : game.SG_PumpIndices) {
				game.ShotGunModel->Nodes[idx].transform = game.ShotGunModel->BindPose[idx] * pumpTrans;
			}
		}
	}
	break;

	case WeaponType::Rifle:
	{
		float recoilT = currentWeapon.GetRecoilAlpha();
		float zOffset = 0.3f;
		float pitchAngle = 0.0f;
		float shakeX = 0.0f;

		if (recoilT > 0.0f) {
			zOffset -= 0.07f * powf(recoilT, 1.0f);

			pitchAngle = (currentWeapon.m_info.recoilVelocity * 0.1f) * recoilT;

			shakeX = sinf(recoilT * XM_PI * 1.0f) * 0.005f;
		}

		XMMATRIX rotMat = XMMatrixRotationX(-XMConvertToRadians(pitchAngle));
		XMMATRIX transMat = XMMatrixTranslation(shakeX, 0.0f, zOffset);

		gunMatrix_firstPersonView.mat = rotMat * transMat;
	}
	break;
	}

	// [sfx] detect reload START for the local player (m_shootFlow goes < 0) and request the reload sound.
	// Audio is played in main.cpp (Utill_Wave.h is a single-TU header); here we only set a global flag.
	{
		extern bool g_playReloadSound;
		if (this == game.player) {
			static bool s_wasReloading = false;
			bool nowReloading = (currentWeapon.m_shootFlow < 0);
			if (nowReloading && !s_wasReloading) g_playReloadSound = true;
			s_wasReloading = nowReloading;
		}
	}
	if (currentWeapon.m_shootFlow < 0) {
		float reloadProgress = 1.0f - (abs(currentWeapon.m_shootFlow) / currentWeapon.m_info.reloadTime);
		float drop = sinf(reloadProgress * XM_PI) * 0.5f;

		XMMATRIX dropMat = XMMatrixTranslation(0, -drop, 0);
		gunMatrix_firstPersonView.mat = gunMatrix_firstPersonView.mat * dropMat;
	}
}

void Player::Render(matrix parent)
{
	if (BoneToWorldMatrixCB.size() == 0) {
		return;
	}

	const bool shouldRenderBody = (game.player != this) || (game.bFirstPersonVision == false);
	if (shouldRenderBody) {
		SkinMeshGameObject::Render(parent);
	}
}

void Player::ModifyLocalToWorld() {
	if (BoneToWorldMatrixCB_Default.size() == 0) return;
	Model* model = shape.GetModel();
	if (model == nullptr) return;
	matrix rootworld = worldMat;
	// Only the local first-person body is hidden. Applying this to every Player collapses
	// remote players' skinning root to the world origin (visible inside origin-authored dungeons).
	if (game.player == this && game.bFirstPersonVision) {
		rootworld.right = vec4(0, 0, 0, 0);
		rootworld.up = vec4(0, 0, 0, 0);
		rootworld.look = vec4(0, 0, 0, 0);
		rootworld.pos = vec4(0, 0, 0, 0);
	}
	
	rootworld.transpose();
	for (int i = 0; i < model->mNumSkinMesh; ++i) {
		using HBLTWSRPI = HBoneLocalToWorldShader::RootParamId;
		//gd.CScmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
		gd.CScmd.ResBarrierTr(&NodeLocalMatrixs, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		gd.CScmd.ResBarrierTr(&BoneToWorldMatrixCB_Default[i], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		DescHandle AllocDescHandle;
		gd.ShaderVisibleDescPool.DynamicAlloc(&AllocDescHandle, 5);
		DescHandle tempDescHandle_SRVTable_LocalMatrixs = AllocDescHandle[0];
		DescHandle tempDescHandle_SRVTable_TPOSLocalTr = AllocDescHandle[1];
		DescHandle tempDescHandle_SRVTable_toParent = AllocDescHandle[2];
		DescHandle tempDescHandle_SRVTable_NodeToBoneIndex = AllocDescHandle[3];
		DescHandle tempDescHandle_UAVTable_Out_ToWorldMatrix = AllocDescHandle[4];

		gd.pDevice->CopyDescriptorsSimple(1, tempDescHandle_SRVTable_LocalMatrixs.hcpu, NodeLocalMatrixs_SRVDescIndex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		gd.pDevice->CopyDescriptorsSimple(1, tempDescHandle_SRVTable_TPOSLocalTr.hcpu, NodeTposMatrixs_SRVDescIndex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		gd.pDevice->CopyDescriptorsSimple(1, tempDescHandle_SRVTable_toParent.hcpu,
			NodeToParentSRVIndex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		gd.pDevice->CopyDescriptorsSimple(1, tempDescHandle_SRVTable_NodeToBoneIndex.hcpu, NodeToBone_SRVDescIndex[i].hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		gd.pDevice->CopyDescriptorsSimple(1, tempDescHandle_UAVTable_Out_ToWorldMatrix.hcpu, BoneToWorldMatrix_UAVDescIndex[i].hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		gd.CScmd->SetComputeRoot32BitConstants(HBLTWSRPI::Constant_WorldMat, 16, &rootworld, 0);
		gd.CScmd->SetComputeRootDescriptorTable(HBLTWSRPI::SRVTable_LocalMatrixs, tempDescHandle_SRVTable_LocalMatrixs.hgpu);
		gd.CScmd->SetComputeRootDescriptorTable(HBLTWSRPI::SRVTable_TPOSLocalTr, tempDescHandle_SRVTable_TPOSLocalTr.hgpu);
		gd.CScmd->SetComputeRootDescriptorTable(HBLTWSRPI::SRVTable_toParent, tempDescHandle_SRVTable_toParent.hgpu);
		gd.CScmd->SetComputeRootDescriptorTable(HBLTWSRPI::SRVTable_NodeToBoneIndex, tempDescHandle_SRVTable_NodeToBoneIndex.hgpu);
		gd.CScmd->SetComputeRootDescriptorTable(HBLTWSRPI::UAVTable_Out_ToWorldMatrix, tempDescHandle_UAVTable_Out_ToWorldMatrix.hgpu);
		gd.CScmd->Dispatch(1, 1, 1); // numthread(64, 1, 1)
		gd.CScmd.ResBarrierTr(&BoneToWorldMatrixCB_Default[i], D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	}
}

void Player::PlayerWeaponObjectInit()
{
	auto RegisterPlayerWeaponObject = [&](WeaponType type, Model* model) {
		const int weaponIndex = (int)type;
		if (weaponIndex < 0 || weaponIndex >= Game::MaxWeapon || model == nullptr) return;
		if (PlayerWeaponObj[weaponIndex] != nullptr) return;
		PlayerWeaponObj[weaponIndex] = new GameObject();
		PlayerWeaponObj[weaponIndex]->worldMat = 0;
		PlayerWeaponObj[weaponIndex]->SetShape(model);
		PlayerWeaponObj[weaponIndex]->SetRaytracingInstanceEnabled(false);
		};
	RegisterPlayerWeaponObject(WeaponType::Sniper, game.SniperModel);
	RegisterPlayerWeaponObject(WeaponType::Rifle, game.RifleModel);
	RegisterPlayerWeaponObject(WeaponType::Pistol, game.PistolModel);
	RegisterPlayerWeaponObject(WeaponType::Shotgun, game.ShotGunModel);
	RegisterPlayerWeaponObject(WeaponType::MachineGun, game.MachineGunModel);
	RegisterPlayerWeaponObject(WeaponType::DualPistol, game.DualRevolverModel);
	RegisterPlayerWeaponObject(WeaponType::DronePistol, game.PistolModel);
	RegisterPlayerWeaponObject(WeaponType::SMG, game.HackerSMGModel);
	RegisterPlayerWeaponObject(WeaponType::GrenadeGun, game.BomberGrenadeGunModel);

	const int weaponIndex = (int)WeaponType::DualPistol;
	if (weaponIndex >= 0 && weaponIndex < Game::MaxWeapon && game.DualRevolverModel != nullptr && LeftHand == nullptr) {
		LeftHand = new GameObject();
		LeftHand->worldMat = 0;
		LeftHand->SetShape(game.DualRevolverModel);
		LeftHand->SetRaytracingInstanceEnabled(false);
	}

	for (int i = 0; i < 2; ++i) {
		if (DronObj[i] == nullptr && game.SupportDroneModel != nullptr) {
			DronObj[i] = new GameObject();
			DronObj[i]->worldMat = 0;
			DronObj[i]->SetShape(game.SupportDroneModel);
			DronObj[i]->SetRaytracingInstanceEnabled(false);
		}

		if (Knife[i] == nullptr && game.DualGunBladeModel != nullptr) {
			Knife[i] = new GameObject();
			Knife[i]->worldMat = 0;
			Knife[i]->SetShape(game.DualGunBladeModel);
			Knife[i]->SetRaytracingInstanceEnabled(false);
		}
	}
}

static bool TryFindHumanoidNodeIndex(Model* model, HumanoidAnimation::HumanBodyBones bone, int& outNodeIndex);
static matrix GetAnimatedNodeLocalMatrix(SkinMeshGameObject* obj, Model* model, int nodeIndex);
static bool TryGetHumanoidBoneWorldMatrix(Player* player, HumanoidAnimation::HumanBodyBones bone, matrix& outBoneWorld);
static bool TryBuildThirdPersonWeaponMatrix(Player* player, bool leftHand, matrix& outWeaponWorld);

static int GetPlayerWeaponVisualKey(const Player* player)
{
	if (player == nullptr) return -1;
	const int bladeMode = player->m_dualBladeVisualTimer > 0.0f ? 1 : 0;
	return player->m_currentWeaponType + bladeMode * 256;
}

void Player::ClearThirdPersonWeaponVisuals()
{
	cachedThirdPersonRightHandMatrixValid = false;
	cachedThirdPersonLeftHandMatrixValid = false;
	cachedRightHandForwardAxisIndex = -1;
	cachedRightHandRightAxisIndex = -1;
	cachedRightHandForwardAxisSign = 1.0f;
	cachedRightHandRightAxisSign = 1.0f;
	cachedThirdPersonWeaponMatrixValid = false;
	cachedThirdPersonLeftWeaponMatrixValid = false;
	cachedThirdPersonWeaponType = -1;

	for (int i = 0; i < Game::MaxWeapon; ++i) {
		if (PlayerWeaponObj[i] == nullptr) continue;
		PlayerWeaponObj[i]->worldMat = 0;
		PlayerWeaponObj[i]->RaytracingUpdateTransform();
		PlayerWeaponObj[i]->SetRaytracingInstanceEnabled(false);
	}
	for (int i = 0; i < 2; ++i) {
		if (Knife[i] == nullptr) continue;
		Knife[i]->worldMat = 0;
		Knife[i]->RaytracingUpdateTransform();
		Knife[i]->SetRaytracingInstanceEnabled(false);
	}
	if (LeftHand != nullptr) {
		LeftHand->worldMat = 0;
		LeftHand->RaytracingUpdateTransform();
		LeftHand->SetRaytracingInstanceEnabled(false);
	}
}

void Player::UpdateRaytracingWeaponVisibility()
{
	if (!gd.isSupportRaytracing) return;
	const bool canShowWeapon = tag[GameObjectTag::Tag_Enable] && !m_weaponHolstered;
	const bool dualPistol = m_currentWeaponType == (int)WeaponType::DualPistol;
	const bool bladeMode = dualPistol && m_dualBladeVisualTimer > 0.0f;

	for (int i = 0; i < Game::MaxWeapon; ++i) {
		if (PlayerWeaponObj[i] == nullptr) continue;
		const bool visible = canShowWeapon && m_currentWeaponType == i && !(dualPistol && bladeMode);
		PlayerWeaponObj[i]->SetRaytracingInstanceEnabled(visible);
	}
	for (int i = 0; i < 2; ++i) {
		if (Knife[i] != nullptr) Knife[i]->SetRaytracingInstanceEnabled(canShowWeapon && bladeMode);
	}
	if (LeftHand != nullptr) {
		LeftHand->SetRaytracingInstanceEnabled(canShowWeapon && dualPistol && !bladeMode);
	}
}

void Player::SetRaytracingVisualsEnabled(bool enabled)
{
	SetRaytracingInstanceEnabled(enabled);
	// Attachments have their own visibility rules. Enabling the player must not revive every cached
	// weapon/drone instance; disabling the player, however, must remove all of them immediately.
	if (!enabled) {
		for (int i = 0; i < Game::MaxWeapon; ++i) {
			if (PlayerWeaponObj[i] != nullptr) PlayerWeaponObj[i]->SetRaytracingInstanceEnabled(false);
		}
		for (int i = 0; i < 2; ++i) {
			if (DronObj[i] != nullptr) DronObj[i]->SetRaytracingInstanceEnabled(false);
			if (Knife[i] != nullptr) Knife[i]->SetRaytracingInstanceEnabled(false);
		}
		if (LeftHand != nullptr) LeftHand->SetRaytracingInstanceEnabled(false);
	}
}

void Player::Render_ThirdPersonWeapon()
{
	UpdateRaytracingWeaponVisibility();
	if (tag[GameObjectTag::Tag_Enable] == false || m_weaponHolstered) {
		ClearThirdPersonWeaponVisuals();
		return;
	}
	if (game.bFirstPersonVision) return;
	const int visualKey = GetPlayerWeaponVisualKey(this);
	if (gd.isRaytracingRender) {
		matrix gunmat;
		if (!TryBuildThirdPersonWeaponMatrix(this, false, gunmat)) {
			gunmat = gunMatrix_thirdPersonView;
			gunmat *= XMMatrixRotationX(XM_PI);
			gunmat.pos.y -= 0.40f;
			gunmat.pos.x += 0.55f;
			gunmat.pos.z += 0.80f;
			gunmat *= worldMat;
		}

		for (int i = 0; i < Game::MaxWeapon; ++i) {
			if (PlayerWeaponObj[i] == nullptr) continue;

			if ((WeaponType)i == WeaponType::DualPistol) {
				matrix leftGunmat;
				const bool hasLeftGun = TryBuildThirdPersonWeaponMatrix(this, true, leftGunmat);
				if (m_dualBladeVisualTimer > 0.0f) {
					if (m_currentWeaponType != i) {
						Knife[0]->worldMat = 0;
						Knife[1]->worldMat = 0;
					}
					else {
						Knife[0]->worldMat = hasLeftGun ? leftGunmat : 0;
						Knife[1]->worldMat = gunmat;
					}
					if (LeftHand != nullptr) {
						LeftHand->worldMat = 0;
					}
					PlayerWeaponObj[i]->worldMat = 0;
				}
				else {
					if (m_currentWeaponType != i) {
						PlayerWeaponObj[i]->worldMat = 0;
						Knife[0]->worldMat = 0;
						Knife[1]->worldMat = 0;
					}
					else {
						Knife[0]->worldMat = 0;
						Knife[1]->worldMat = 0;
						PlayerWeaponObj[i]->worldMat = gunmat;
						if (LeftHand != nullptr) {
							LeftHand->worldMat = hasLeftGun ? leftGunmat : 0;
						}
					}
				}
				PlayerWeaponObj[i]->RaytracingUpdateTransform();
				Knife[0]->RaytracingUpdateTransform();
				Knife[1]->RaytracingUpdateTransform();
				if (LeftHand != nullptr) {
					LeftHand->RaytracingUpdateTransform();
				}
			}
			else {
				if ((WeaponType)m_currentWeaponType != WeaponType::DualPistol && LeftHand != nullptr) {
					LeftHand->worldMat = 0;
					LeftHand->RaytracingUpdateTransform();
				}
				if (m_currentWeaponType != i) {
					PlayerWeaponObj[i]->worldMat = 0;
				}
				else {
					PlayerWeaponObj[i]->worldMat = gunmat;
				}
				PlayerWeaponObj[i]->RaytracingUpdateTransform();
			}
		}
	}
	else {
		if (game.player == this && game.bFirstPersonVision) {
			return;
		}

		Model* pTargetModel = GetWeaponRenderModel(this);
		if (!pTargetModel) {
			return;
		}

		matrix gunmat;
		if (cachedThirdPersonWeaponMatrixValid && cachedThirdPersonWeaponType == visualKey) {
			gunmat = cachedThirdPersonWeaponMatrix;
		}
		else if (!TryBuildThirdPersonWeaponMatrix(this, false, gunmat)) {
			gunmat = gunMatrix_thirdPersonView;
			gunmat *= XMMatrixRotationX(XM_PI);
			gunmat.pos.y -= 0.40f;
			gunmat.pos.x += 0.55f;
			gunmat.pos.z += 0.80f;
			gunmat *= worldMat;
		}

		pTargetModel->Render(gd.gpucmd, gunmat, nullptr);
		if ((WeaponType)m_currentWeaponType == WeaponType::DualPistol) {
			matrix leftGunmat;
			if (cachedThirdPersonLeftWeaponMatrixValid && cachedThirdPersonWeaponType == visualKey) {
				leftGunmat = cachedThirdPersonLeftWeaponMatrix;
				pTargetModel->Render(gd.gpucmd, leftGunmat, nullptr);
			}
			else if (TryBuildThirdPersonWeaponMatrix(this, true, leftGunmat)) {
				pTargetModel->Render(gd.gpucmd, leftGunmat, nullptr);
			}
		}
	}
}

void Player::Render_AfterDepthClear()
{
	if (tag[GameObjectTag::Tag_Enable] == false || m_weaponHolstered) {
		ClearThirdPersonWeaponVisuals();
		return;
	}

	matrix viewmat = gd.viewportArr[0].ViewMatrix.RTInverse;

	struct Model* pTargetModel = nullptr;

	if (gd.isRaytracingRender) {
		if (game.bFirstPersonVision) {
			matrix gunmat = gunMatrix_firstPersonView;
			matrix leftGunMat = gunMatrix_firstPersonView;
			const WeaponType weaponType = (WeaponType)m_currentWeaponType;
			const float firstPersonReloadProgress = ReloadRemain > 0.0f
				? min(1.0f, max(0.0f, 1.0f - ReloadRemain / max(GetPlayerReloadTime(this), 0.01f)))
				: 0.0f;
			bool isdrawLeft = false;
			const float firstPersonReloadArc = sinf(firstPersonReloadProgress * XM_PI);

			if (weaponType == WeaponType::DualPistol) {
				isdrawLeft = true;
				const bool bladeMode = m_dualBladeVisualTimer > 0.0f;

				const float dualPistolScale = 3.50f;
				const vec4 dualPistolLeftPosition = vec4(-0.82f, -0.78f, 1.45f, 1.0f);
				const vec4 dualPistolRightPosition = vec4(0.82f, -0.78f, 1.45f, 1.0f);
				const float dualBladeScale = 0.5f;
				const vec4 dualBladeLeftPosition = vec4(-3.42f, -0.32f, 2.22f, 1.0f);
				const vec4 dualBladeRightPosition = vec4(-1.82f, -0.32f, 2.22f, 1.0f);

				const float bladeAttackProgress = bladeMode && m_dualBladeAttackVisualTimer > 0.0f
					? 1.0f - min(1.0f, m_dualBladeAttackVisualTimer / 0.34f) : 0.0f;
				const float bladeSwing = sinf(bladeAttackProgress * XM_PI);

				auto RenderFirstPersonHandWeapon_ray = [&](bool leftHand) {
					const bool activeBlade = leftHand == m_dualBladeAttackAlternate;
					const float handBladeSwing = activeBlade ? bladeSwing : 0.0f;
					const float modelScale = bladeMode ? dualBladeScale : dualPistolScale;
					matrix handWeapon = XMMatrixScaling(modelScale, modelScale, modelScale);
					float recoilT = 0.0f;

					if (!bladeMode) {
						const float recoilTimer = leftHand ? m_dualLeftRecoilTimer : m_dualRightRecoilTimer;
						const float recoilProgress = 1.0f - min(1.0f, recoilTimer / 0.22f);
						recoilT = recoilTimer > 0.0f ? sinf(recoilProgress * XM_PI) : 0.0f;
						handWeapon *= XMMatrixRotationX(-XMConvertToRadians(10.0f * recoilT));
						handWeapon *= XMMatrixRotationZ(XMConvertToRadians((leftHand ? -2.5f : 2.5f) * recoilT));
					}

					if (bladeMode && leftHand) {
						handWeapon *= XMMatrixRotationRollPitchYaw(
							XMConvertToRadians(0.0f), XMConvertToRadians(90.0f), XMConvertToRadians(-40.0f));
					}
					else if (bladeMode) {
						handWeapon *= XMMatrixRotationRollPitchYaw(
							XMConvertToRadians(0.0f), XMConvertToRadians(90.0f), XMConvertToRadians(-40.0f));
					}
					else if (leftHand) {
						handWeapon *= XMMatrixRotationRollPitchYaw(
							XMConvertToRadians(-90.0f), XMConvertToRadians(-90.0f), XMConvertToRadians(0.0f));
					}
					else {
						handWeapon *= XMMatrixRotationRollPitchYaw(
							XMConvertToRadians(-90.0f), XMConvertToRadians(-90.0f), XMConvertToRadians(0.0f));
					}
					if (!bladeMode && firstPersonReloadArc > 0.0f) {
						handWeapon *= XMMatrixRotationZ(XMConvertToRadians(
							(leftHand ? -16.0f : 16.0f) * firstPersonReloadArc));
					}

					if (bladeMode) {
						handWeapon.pos = leftHand ? dualBladeLeftPosition : dualBladeRightPosition;
					}
					else {
						handWeapon.pos = leftHand ? dualPistolLeftPosition : dualPistolRightPosition;
					}

					if (bladeMode && activeBlade) {
						handWeapon.pos.x += (leftHand ? 0.34f : -0.34f) * handBladeSwing;
						handWeapon.pos.y -= 0.16f * handBladeSwing;
					}
					if (!bladeMode) {
						handWeapon.pos.x += (leftHand ? -0.018f : 0.018f) * recoilT;
						handWeapon.pos.y += 0.26f * recoilT;
						handWeapon.pos.z -= 0.12f * recoilT;
						handWeapon.pos.x += (leftHand ? -0.12f : 0.12f) * firstPersonReloadArc;
						handWeapon.pos.y -= 0.68f * firstPersonReloadArc;
						handWeapon.pos.z -= 0.10f * firstPersonReloadArc;
					}

					//handWeapon *= viewmat;
					if (leftHand) {
						leftGunMat = handWeapon;
					}
					else {
						gunmat = handWeapon;
					}
					};
				RenderFirstPersonHandWeapon_ray(false);
				RenderFirstPersonHandWeapon_ray(true);
			}
			else if (weaponType == WeaponType::SMG) {
				const float recoilT = powf(min(1.0f, weapon[SelectedWeapon].GetRecoilAlpha()), 0.62f);
				gunmat = XMMatrixScaling(3.0f, 3.0f, 3.0f);
				gunmat *= XMMatrixRotationRollPitchYaw(
					XMConvertToRadians(0.0f), XMConvertToRadians(180.0f), XMConvertToRadians(0.0f));
				gunmat *= XMMatrixRotationX(-XMConvertToRadians(4.5f * recoilT));
				gunmat *= XMMatrixRotationZ(XMConvertToRadians(-14.0f * firstPersonReloadArc));
				gunmat.pos = vec4(-4.38f, -0.72f, 2.35f, 1.0f);
				gunmat.pos.x += 0.14f * firstPersonReloadArc;
				gunmat.pos.y += 0.075f * recoilT - 0.18f * firstPersonReloadArc;
				gunmat.pos.z -= 0.11f * recoilT + 0.08f * firstPersonReloadArc;
			}
			else if (weaponType == WeaponType::GrenadeGun) {
				const float recoilT = powf(min(1.0f, weapon[SelectedWeapon].GetRecoilAlpha()), 0.72f);
				gunmat = XMMatrixScaling(15.0f, 15.0f, 15.0f);
				gunmat *= XMMatrixRotationRollPitchYaw(
					XMConvertToRadians(0.0f), XMConvertToRadians(180.0f), XMConvertToRadians(0.0f));
				gunmat *= XMMatrixRotationX(-XMConvertToRadians(13.0f * recoilT));
				gunmat *= XMMatrixRotationZ(XMConvertToRadians(-20.0f * firstPersonReloadArc));
				gunmat.pos = vec4(0.92f, -0.58f, 1.50f, 1.0f);
				gunmat.pos.x += 0.18f * firstPersonReloadArc;
				gunmat.pos.y += 0.16f * recoilT - 0.78f * firstPersonReloadArc;
				gunmat.pos.z -= 0.32f * recoilT + 0.12f * firstPersonReloadArc;
			}

			if (weaponType != WeaponType::SMG && weaponType != WeaponType::GrenadeGun)
				switch ((WeaponType)m_currentWeaponType)
				{
				case WeaponType::MachineGun:
				{
					matrix modelFix = XMMatrixScaling(0.8f, 0.8f, 0.8f);
					modelFix *= XMMatrixRotationY(XMConvertToRadians(180.0f));
					modelFix *= XMMatrixRotationZ(XMConvertToRadians(180.0f));
					gunmat = modelFix * (XMMATRIX)gunMatrix_firstPersonView;
					gunmat.pos.y -= 0.60f;
					gunmat.pos.x += 0.35f;
					gunmat.pos.z += 0.90f;
					break;
				}
				case WeaponType::Sniper:
					if (m_isZooming && m_currentFov < 25.0f) {
						pTargetModel = nullptr;
						gunmat *= XMMatrixScaling(0.0f, 0.0f, 0.0f);
					}
					else {
						gunmat *= XMMatrixScaling(1.0f, 1.0f, 1.0f);
						gunmat.pos.y -= 0.40f;
						gunmat.pos.x += 0.60f;
						gunmat.pos.z += 1.90f;
					}
					break;
				case WeaponType::Pistol:
				case WeaponType::DronePistol:
					gunmat *= XMMatrixScaling(2.0f, 2.0f, 2.0f);
					gunmat.pos.y -= 1.20f;
					gunmat.pos.x += 1.00f;
					gunmat.pos.z += 1.40f;
					break;
				case WeaponType::Rifle:
				{
					matrix modelFix = XMMatrixScaling(1.5f, 1.5f, 1.5f);
					modelFix *= XMMatrixRotationY(XMConvertToRadians(-90.0f));
					gunmat = modelFix * (XMMATRIX)gunMatrix_firstPersonView;

					float zoomAlpha = (60.0f - m_currentFov) / (60.0f - 40.0f);
					if (zoomAlpha < 0) zoomAlpha = 0;
					if (zoomAlpha > 1) zoomAlpha = 1;

					float targetX = 0.0f;
					float targetY = -0.35f;
					float targetZ = 1.80f;

					gunmat.pos.x += (0.90f * (1.0f - zoomAlpha)) + (targetX * zoomAlpha);
					gunmat.pos.y -= (0.65f * (1.0f - zoomAlpha)) + (abs(targetY) * zoomAlpha);
					gunmat.pos.z += (1.50f * (1.0f - zoomAlpha)) + (targetZ * zoomAlpha);
					break;
				}
				case WeaponType::Shotgun:
				{
					matrix modelFix = XMMatrixScaling(1.0f, 1.0f, 1.0f);
					modelFix *= XMMatrixRotationY(XMConvertToRadians(90.0f));
					gunmat = modelFix * (XMMATRIX)gunMatrix_firstPersonView;
					gunmat.pos.y -= 0.70f;
					gunmat.pos.x += 0.75f;
					gunmat.pos.z += 1.70f;
					break;
				}
				//case WeaponType::DualPistol: {
				//	gunmat *= XMMatrixScaling(1.0f, 1.0f, 1.0f);
				//	gunmat.pos.y -= 1.20f;
				//	gunmat.pos.x += 1.50f;
				//	gunmat.pos.z += 2.0f;

				//	leftGunMat *= XMMatrixScaling(1.0f, 1.0f, 1.0f);
				//	leftGunMat.pos.y -= 1.20f;
				//	leftGunMat.pos.x -= 1.50f;
				//	leftGunMat.pos.z += 2.0f;
				//	break;
				//}
				}

			gunmat *= viewmat;
			leftGunMat *= viewmat;
			for (int i = 0; i < Game::MaxWeapon; ++i) {
				if (PlayerWeaponObj[i] == nullptr) continue;
				
				if ((WeaponType)i == WeaponType::DualPistol ) {
					if (m_dualBladeVisualTimer > 0.0f) {
						Knife[0]->worldMat = leftGunMat;
						Knife[1]->worldMat = gunmat;
						PlayerWeaponObj[i]->worldMat = 0;
					}
					else {
						Knife[0]->worldMat = 0;
						Knife[1]->worldMat = 0;
						PlayerWeaponObj[i]->worldMat = gunmat;
					}
					Knife[0]->RaytracingUpdateTransform();
					Knife[1]->RaytracingUpdateTransform();
					PlayerWeaponObj[i]->RaytracingUpdateTransform();
				}
				else {
					if (m_currentWeaponType != i) {
						PlayerWeaponObj[i]->worldMat = 0;
					}
					else {
						PlayerWeaponObj[i]->worldMat = gunmat;
					}
					PlayerWeaponObj[i]->RaytracingUpdateTransform();
				}
			}

			if (isdrawLeft) {
				LeftHand->worldMat = leftGunMat;
				LeftHand->RaytracingUpdateTransform();
			}
		}
	}
	else if (game.bFirstPersonVision)
	{
		pTargetModel = GetWeaponRenderModel(this);
		if (!pTargetModel) {
			goto GunRenderEnd;
		}

		if (pTargetModel) {
			matrix gunmat = gunMatrix_firstPersonView;
			const WeaponType weaponType = (WeaponType)m_currentWeaponType;
			const float firstPersonReloadProgress = ReloadRemain > 0.0f
				? min(1.0f, max(0.0f, 1.0f - ReloadRemain / max(GetPlayerReloadTime(this), 0.01f)))
				: 0.0f;
			const float firstPersonReloadArc = sinf(firstPersonReloadProgress * XM_PI);

			if (weaponType == WeaponType::DualPistol) {
				const bool bladeMode = m_dualBladeVisualTimer > 0.0f;

				const float dualPistolScale = 3.50f;
				const vec4 dualPistolLeftPosition = vec4(-0.82f, -0.78f, 1.45f, 1.0f);
				const vec4 dualPistolRightPosition = vec4(0.82f, -0.78f, 1.45f, 1.0f);
				const float dualBladeScale = 0.5f;
				const vec4 dualBladeLeftPosition = vec4(-3.42f, -0.32f, 2.22f, 1.0f);
				const vec4 dualBladeRightPosition = vec4(-1.82f, -0.32f, 2.22f, 1.0f);

				const float bladeAttackProgress = bladeMode && m_dualBladeAttackVisualTimer > 0.0f
					? 1.0f - min(1.0f, m_dualBladeAttackVisualTimer / 0.34f) : 0.0f;
				const float bladeSwing = sinf(bladeAttackProgress * XM_PI);

				auto RenderFirstPersonHandWeapon = [&](bool leftHand) {
					const bool activeBlade = leftHand == m_dualBladeAttackAlternate;
					const float handBladeSwing = activeBlade ? bladeSwing : 0.0f;
					const float modelScale = bladeMode ? dualBladeScale : dualPistolScale;
					matrix handWeapon = XMMatrixScaling(modelScale, modelScale, modelScale);
					float recoilT = 0.0f;

					if (!bladeMode) {
						const float recoilTimer = leftHand ? m_dualLeftRecoilTimer : m_dualRightRecoilTimer;
						const float recoilProgress = 1.0f - min(1.0f, recoilTimer / 0.22f);
						recoilT = recoilTimer > 0.0f ? sinf(recoilProgress * XM_PI) : 0.0f;
						handWeapon *= XMMatrixRotationX(-XMConvertToRadians(10.0f * recoilT));
						handWeapon *= XMMatrixRotationZ(XMConvertToRadians((leftHand ? -2.5f : 2.5f) * recoilT));
					}

					if (bladeMode && leftHand) {
						handWeapon *= XMMatrixRotationRollPitchYaw(
							XMConvertToRadians(0.0f), XMConvertToRadians(90.0f), XMConvertToRadians(-40.0f));
					}
					else if (bladeMode) {
						handWeapon *= XMMatrixRotationRollPitchYaw(
							XMConvertToRadians(0.0f), XMConvertToRadians(90.0f), XMConvertToRadians(-40.0f));
					}
					else if (leftHand) {
						handWeapon *= XMMatrixRotationRollPitchYaw(
							XMConvertToRadians(-90.0f), XMConvertToRadians(-90.0f), XMConvertToRadians(0.0f));
					}
					else {
						handWeapon *= XMMatrixRotationRollPitchYaw(
							XMConvertToRadians(-90.0f), XMConvertToRadians(-90.0f), XMConvertToRadians(0.0f));
					}
					if (!bladeMode && firstPersonReloadArc > 0.0f) {
						handWeapon *= XMMatrixRotationZ(XMConvertToRadians(
							(leftHand ? -16.0f : 16.0f) * firstPersonReloadArc));
					}

					if (bladeMode) {
						handWeapon.pos = leftHand ? dualBladeLeftPosition : dualBladeRightPosition;
					}
					else {
						handWeapon.pos = leftHand ? dualPistolLeftPosition : dualPistolRightPosition;
					}

					if (bladeMode && activeBlade) {
						handWeapon.pos.x += (leftHand ? 0.34f : -0.34f) * handBladeSwing;
						handWeapon.pos.y -= 0.16f * handBladeSwing;
					}
					if (!bladeMode) {
						handWeapon.pos.x += (leftHand ? -0.018f : 0.018f) * recoilT;
						handWeapon.pos.y += 0.26f * recoilT;
						handWeapon.pos.z -= 0.12f * recoilT;
						handWeapon.pos.x += (leftHand ? -0.12f : 0.12f) * firstPersonReloadArc;
						handWeapon.pos.y -= 0.68f * firstPersonReloadArc;
						handWeapon.pos.z -= 0.10f * firstPersonReloadArc;
					}

					handWeapon *= viewmat;
					pTargetModel->Render(gd.gpucmd, handWeapon);
				};
				RenderFirstPersonHandWeapon(false);
				RenderFirstPersonHandWeapon(true);
				pTargetModel = nullptr;
			}
			else if (weaponType == WeaponType::SMG) {
				const float recoilT = powf(min(1.0f, weapon[SelectedWeapon].GetRecoilAlpha()), 0.62f);
				gunmat = XMMatrixScaling(3.0f, 3.0f, 3.0f);
				gunmat *= XMMatrixRotationRollPitchYaw(
					XMConvertToRadians(0.0f), XMConvertToRadians(180.0f), XMConvertToRadians(0.0f));
				gunmat *= XMMatrixRotationX(-XMConvertToRadians(4.5f * recoilT));
				gunmat *= XMMatrixRotationZ(XMConvertToRadians(-14.0f * firstPersonReloadArc));
				gunmat.pos = vec4(-4.38f, -0.72f, 2.35f, 1.0f);
				gunmat.pos.x += 0.14f * firstPersonReloadArc;
				gunmat.pos.y += 0.075f * recoilT - 0.18f * firstPersonReloadArc;
				gunmat.pos.z -= 0.11f * recoilT + 0.08f * firstPersonReloadArc;
			}
			else if (weaponType == WeaponType::GrenadeGun) {
				const float recoilT = powf(min(1.0f, weapon[SelectedWeapon].GetRecoilAlpha()), 0.72f);
				gunmat = XMMatrixScaling(15.0f, 15.0f, 15.0f);
				gunmat *= XMMatrixRotationRollPitchYaw(
					XMConvertToRadians(0.0f), XMConvertToRadians(180.0f), XMConvertToRadians(0.0f));
				gunmat *= XMMatrixRotationX(-XMConvertToRadians(13.0f * recoilT));
				gunmat *= XMMatrixRotationZ(XMConvertToRadians(-20.0f * firstPersonReloadArc));
				gunmat.pos = vec4(0.92f, -0.58f, 1.50f, 1.0f);
				gunmat.pos.x += 0.18f * firstPersonReloadArc;
				gunmat.pos.y += 0.16f * recoilT - 0.78f * firstPersonReloadArc;
				gunmat.pos.z -= 0.32f * recoilT + 0.12f * firstPersonReloadArc;
			}

			if (pTargetModel != nullptr && weaponType != WeaponType::SMG && weaponType != WeaponType::GrenadeGun)
			switch ((WeaponType)m_currentWeaponType)
			{
			case WeaponType::MachineGun:
			{
				matrix modelFix = XMMatrixScaling(0.8f, 0.8f, 0.8f);
				modelFix *= XMMatrixRotationY(XMConvertToRadians(180.0f));
				modelFix *= XMMatrixRotationZ(XMConvertToRadians(180.0f));
				gunmat = modelFix * (XMMATRIX)gunMatrix_firstPersonView;
				gunmat.pos.y -= 0.60f;
				gunmat.pos.x += 0.35f;
				gunmat.pos.z += 0.90f;
				break;
			}
			case WeaponType::Sniper:
				if (m_isZooming && m_currentFov < 25.0f) {
					pTargetModel = nullptr;
				}
				else {
					gunmat *= XMMatrixScaling(1.0f, 1.0f, 1.0f);
					gunmat.pos.y -= 0.40f;
					gunmat.pos.x += 0.60f;
					gunmat.pos.z += 1.90f;
				}
				break;
			case WeaponType::Pistol:
			case WeaponType::DronePistol:
				gunmat *= XMMatrixScaling(2.0f, 2.0f, 2.0f);
				gunmat.pos.y -= 1.20f;
				gunmat.pos.x += 1.00f;
				gunmat.pos.z += 1.40f;
				break;
			case WeaponType::Rifle:
			{
				matrix modelFix = XMMatrixScaling(1.5f, 1.5f, 1.5f);
				modelFix *= XMMatrixRotationY(XMConvertToRadians(-90.0f));
				gunmat = modelFix * (XMMATRIX)gunMatrix_firstPersonView;

				float zoomAlpha = (60.0f - m_currentFov) / (60.0f - 40.0f);
				if (zoomAlpha < 0) zoomAlpha = 0;
				if (zoomAlpha > 1) zoomAlpha = 1;

				float targetX = 0.0f;
				float targetY = -0.35f;
				float targetZ = 1.80f;

				gunmat.pos.x += (0.90f * (1.0f - zoomAlpha)) + (targetX * zoomAlpha);
				gunmat.pos.y -= (0.65f * (1.0f - zoomAlpha)) + (abs(targetY) * zoomAlpha);
				gunmat.pos.z += (1.50f * (1.0f - zoomAlpha)) + (targetZ * zoomAlpha);
				break;
			}
			case WeaponType::Shotgun:
			{
				matrix modelFix = XMMatrixScaling(1.0f, 1.0f, 1.0f);
				modelFix *= XMMatrixRotationY(XMConvertToRadians(90.0f));
				gunmat = modelFix * (XMMATRIX)gunMatrix_firstPersonView;
				gunmat.pos.y -= 0.70f;
				gunmat.pos.x += 0.75f;
				gunmat.pos.z += 1.70f;
				break;
			}
			}

			if (pTargetModel) {
				gunmat *= viewmat;

				//if (Mat == nullptr) continue;
				if (Game::renderViewPort == &game.MyDirLight[0].viewport) {
					pTargetModel->Render(gd.gpucmd, gunmat);
					return;
				}
				else {
					pTargetModel->Render(gd.gpucmd, gunmat);
				}
			}
		}
	}

GunRenderEnd:
	if (gd.isRaytracingRender) {
		return;
	}

	Shader* shader = (Shader*)game.MyOnlyColorShader;
	shader->Add_RegisterShaderCommand(gd.gpucmd);

	matrix vmat = gd.viewportArr[0].ViewMatrix;
	vmat *= gd.viewportArr[0].ProjectMatrix;
	vmat.transpose();
	gd.gpucmd->SetGraphicsRoot32BitConstants(0, 16, &vmat, 0);

	matrix spmat = viewmat;
	spmat.pos += spmat.look * 10;
	spmat.transpose();
	gd.gpucmd->SetGraphicsRoot32BitConstants(1, 16, &spmat, 0);
	ShootPointMesh->Render(gd.gpucmd, 1);

	// Player HP/heat is rendered by the 2D gameplay HUD.
}

void Player::UpdateGunBarrelNodes()
{
	if ((WeaponType)m_currentWeaponType != WeaponType::MachineGun) return;
	if (!game.MachineGunModel || game.MG_BarrelIndices.empty()) return;

	vec4 pivot(0, 0, 0, 1);
	for (int idx : game.MG_BarrelIndices) {
		pivot.x += game.MachineGunModel->BindPose[idx].pos.x;
		pivot.y += game.MachineGunModel->BindPose[idx].pos.y;
		pivot.z += game.MachineGunModel->BindPose[idx].pos.z;
	}
	const float invCount = 1.0f / static_cast<float>(game.MG_BarrelIndices.size());
	pivot.x *= invCount;
	pivot.y *= invCount;
	pivot.z *= invCount;

	XMMATRIX axisRot = XMMatrixRotationZ(gunBarrelAngle);

	XMMATRIX barrelOrbit =
		XMMatrixTranslation(-pivot.x, -pivot.y, -pivot.z) *
		axisRot *
		XMMatrixTranslation(pivot.x, pivot.y, pivot.z);

	for (int idx : game.MG_BarrelIndices) {
		game.MachineGunModel->Nodes[idx].transform = game.MachineGunModel->BindPose[idx] * barrelOrbit;
	}
}

static bool TryFindHumanoidNodeIndex(Model* model, HumanoidAnimation::HumanBodyBones bone, int& outNodeIndex)
{
	if (!model) return false;

	if (model->Humanoid_retargeting) {
		for (int i = 0; i < model->nodeCount; ++i) {
			if (model->Humanoid_retargeting[i] == bone) {
				outNodeIndex = i;
				return true;
			}
		}
	}

	outNodeIndex = model->FindNodeIndexByName(HumanoidAnimation::HumanBoneNames[bone]);
	return (outNodeIndex >= 0);
}

static matrix GetAnimatedNodeLocalMatrix(SkinMeshGameObject* obj, Model* model, int nodeIndex)
{
	if (obj && obj->transforms_innerModel) {
		return obj->transforms_innerModel[nodeIndex];
	}
	return model->Nodes[nodeIndex].transform;
}

static bool TryGetHumanoidBoneWorldMatrix(Player* player, HumanoidAnimation::HumanBodyBones bone, matrix& outBoneWorld)
{
	if (!player) return false;

	Mesh* mesh = nullptr;
	Model* model = nullptr;
	player->shape.GetRealShape(mesh, model);
	if (!model) return false;

	int nodeIndex = -1;
	if (!TryFindHumanoidNodeIndex(model, bone, nodeIndex)) {
		return false;
	}

	std::vector<matrix> blendedLocal(model->nodeCount);
	for (int i = 0; i < model->nodeCount; ++i) {
		blendedLocal[i].Id();
	}

	int baseAnim = player->PlayingAnimationIndex[0];
	if (baseAnim >= 0 && baseAnim < game.HumanoidAnimationTable.size()) {
		player->GetBoneLocalMatrixAtTime(&game.HumanoidAnimationTable[baseAnim], blendedLocal.data(), player->AnimationFlowTime[0]);
	}

	int upperAnim = player->PlayingAnimationIndex[1];
	float upperAnimTime = player->AnimationFlowTime[1];
	if (player->m_currentUpperState == Player::UpperState::SHOOT) {
		upperAnim = GetPlayerUpperAnimationIndex(
			(WeaponType)player->m_currentWeaponType, Player::UpperState::AIM);
		upperAnimTime = player->m_upperAimPoseTime;
	}
	if (upperAnim >= 0 && upperAnim < game.HumanoidAnimationTable.size()) {
		std::vector<matrix> upperLocal(model->nodeCount);
		for (int i = 0; i < model->nodeCount; ++i) {
			upperLocal[i].Id();
		}

		player->GetBoneLocalMatrixAtTime(&game.HumanoidAnimationTable[upperAnim], upperLocal.data(), upperAnimTime);
		const float upperBodyYawCorrection = GetPlayerUpperBodyYawCorrection(upperAnim);
		const float maxUpperBodyPitch = XMConvertToRadians(45.0f);
		const float upperBodyPitchBias = XMConvertToRadians(-10.0f);
		const bool applyUpperBodyPitch =
			player->m_currentUpperState == Player::UpperState::AIM ||
			player->m_currentUpperState == Player::UpperState::SHOOT;
		const float upperBodyPitchCorrection = applyUpperBodyPitch
			? max(-maxUpperBodyPitch, min(maxUpperBodyPitch,
				player->m_pitch + upperBodyPitchBias))
			: 0.0f;
		for (int i = 0; i < model->nodeCount; ++i) {
			int humanoidIndex = model->Humanoid_retargeting ? model->Humanoid_retargeting[i] : -1;
			if (humanoidIndex == HumanoidAnimation::Spine &&
				(upperBodyYawCorrection != 0.0f || upperBodyPitchCorrection != 0.0f)) {
				vec4 spinePosition = upperLocal[i].pos;
				// Align the torso to the weapon pose first, then pitch on that corrected axis.
				upperLocal[i] *= XMMatrixRotationY(upperBodyYawCorrection);
				upperLocal[i] *= XMMatrixRotationX(upperBodyPitchCorrection);
				upperLocal[i].pos = spinePosition;
			}
			if (humanoidIndex >= 0 && humanoidIndex < 64) {
				if (player->m_animMask[1] & (1ULL << humanoidIndex)) {
					blendedLocal[i] = upperLocal[i];
				}
			}
		}
	}

	for (int i = 0; i < model->nodeCount; ++i) {
		blendedLocal[i] *= model->DefaultNodelocalTr[i];
	}
	blendedLocal[0] = player->worldMat;

	matrix boneWorld = blendedLocal[nodeIndex];
	ModelNode* node = model->Nodes[nodeIndex].parent;
	while (node != nullptr) {
		int parentIndex = (int)(((char*)node - (char*)model->Nodes) / sizeof(ModelNode));
		boneWorld *= blendedLocal[parentIndex];
		node = node->parent;
	}

	outBoneWorld = boneWorld;
	return true;
}

static bool TryBuildThirdPersonWeaponMatrix(Player* player, bool leftHand, matrix& outWeaponWorld)
{
	matrix handWorld;
	const HumanoidAnimation::HumanBodyBones handBone = leftHand
		? HumanoidAnimation::LeftHand
		: HumanoidAnimation::RightHand;
	if (!TryGetHumanoidBoneWorldMatrix(player, handBone, handWorld)) {
		return false;
	}

	matrix weaponLocal;
	weaponLocal.Id();
	const WeaponType weaponType = (WeaponType)player->m_currentWeaponType;
	const float genericRecoilT = powf(min(1.0f, player->weapon[player->SelectedWeapon].GetRecoilAlpha()), 0.65f);
	const float genericRecoilStrength = min(1.35f, max(0.65f, player->weapon[player->SelectedWeapon].m_info.recoilVelocity / 10.0f));

	if (weaponType == WeaponType::DualPistol) {
		const bool bladeMode = player->m_dualBladeVisualTimer > 0.0f;
		const float bladeAttackProgress = bladeMode && player->m_dualBladeAttackVisualTimer > 0.0f
			? 1.0f - min(1.0f, player->m_dualBladeAttackVisualTimer / 0.34f) : 0.0f;
		const float bladeSwing = sinf(bladeAttackProgress * XM_PI);
		const bool activeBlade = leftHand == player->m_dualBladeAttackAlternate;
		const float handBladeSwing = activeBlade ? bladeSwing : 0.0f;
		float recoilT = 0.0f;
		if (!bladeMode) {
			const float recoilTimer = leftHand ? player->m_dualLeftRecoilTimer : player->m_dualRightRecoilTimer;
			recoilT = powf(min(1.0f, recoilTimer / 0.22f), 0.55f);
			weaponLocal *= XMMatrixRotationX(-XMConvertToRadians(21.0f * recoilT));
			weaponLocal *= XMMatrixRotationZ(XMConvertToRadians((leftHand ? -3.0f : 3.0f) * recoilT));
		}
		if (bladeMode && leftHand) {
			weaponLocal *= XMMatrixScaling(0.3f, 0.3f, 0.3f);
			weaponLocal *= XMMatrixRotationRollPitchYaw(
				XMConvertToRadians(0.658f),
				XMConvertToRadians(-2.367f),
				XMConvertToRadians(-122.515f));
			weaponLocal.pos = vec4(0.155f + 0.22f * handBladeSwing,
				0.874f - 0.20f * handBladeSwing, -1.501f, 1.0f);
		}
		else if (bladeMode) {
			weaponLocal *= XMMatrixScaling(0.3f, 0.3f, 0.3f);
			weaponLocal *= XMMatrixRotationRollPitchYaw(
				XMConvertToRadians(179.658f),
				XMConvertToRadians(4.1289f),
				XMConvertToRadians(45.72f));
			weaponLocal.pos = vec4(-0.040f - 0.22f * handBladeSwing,
				0.754f - 0.20f * handBladeSwing, 1.594f, 1.0f);
		}
		else if (leftHand) {
			weaponLocal *= XMMatrixScaling(2.0f, 2.0f, 2.0f);
			weaponLocal *= XMMatrixRotationRollPitchYaw(
				XMConvertToRadians(-3.743f),
				XMConvertToRadians(95.270f),
				XMConvertToRadians(90.928f));
			weaponLocal.pos = vec4(-0.143f, 0.317f, 0.062f, 1.0f);
		}
		else {
			weaponLocal *= XMMatrixScaling(2.0f, 2.0f, 2.0f);
			weaponLocal *= XMMatrixRotationRollPitchYaw(
				XMConvertToRadians(-6.246f),
				XMConvertToRadians(-88.443f),
				XMConvertToRadians(93.788f));
			weaponLocal.pos = vec4(0.171f, 0.332f, 0.003f, 1.0f);
		}
		if (!bladeMode) {
			weaponLocal.pos.x += (leftHand ? -0.02f : 0.02f) * recoilT;
			weaponLocal.pos.y += 0.07f * recoilT;
			weaponLocal.pos.z -= 0.16f * recoilT;
		}

		outWeaponWorld = weaponLocal * handWorld;
		return true;
	}

	if (leftHand) return false;

	if (weaponType == WeaponType::SMG) {
		weaponLocal *= XMMatrixScaling(1.2f, 1.2f, 1.2f);
		weaponLocal *= XMMatrixRotationRollPitchYaw(
			XMConvertToRadians(78.156f),
			XMConvertToRadians(-33.953f),
			XMConvertToRadians(51.740f));
		weaponLocal.pos = vec4(0.106f, 0.585f, 2.098f, 1.0f);
		weaponLocal.pos.y += 0.035f * genericRecoilT * genericRecoilStrength;
		weaponLocal.pos.z -= 0.080f * genericRecoilT * genericRecoilStrength;
		outWeaponWorld = weaponLocal * handWorld;
		return true;
	}

	if (weaponType == WeaponType::GrenadeGun) {
		weaponLocal *= XMMatrixScaling(10.0f, 10.0f, 10.0f);
		weaponLocal *= XMMatrixRotationRollPitchYaw(
			XMConvertToRadians(80.743f),
			XMConvertToRadians(-120.033f),
			XMConvertToRadians(-392.430f));
		weaponLocal.pos = vec4(0.038f, 0.337f, -0.118f, 1.0f);
		weaponLocal.pos.y += 0.035f * genericRecoilT * genericRecoilStrength;
		weaponLocal.pos.z -= 0.080f * genericRecoilT * genericRecoilStrength;
		outWeaponWorld = weaponLocal * handWorld;
		return true;
	}

	switch (weaponType)
	{
	case WeaponType::Sniper:
		weaponLocal *= XMMatrixScaling(0.5f, 0.5f, 0.5f);
		weaponLocal *= XMMatrixRotationRollPitchYaw(
			XMConvertToRadians(-44.013f),
			XMConvertToRadians(87.456f),
			XMConvertToRadians(-26.701f));
		weaponLocal.pos = vec4(0.453f, 0.540f, 0.046f, 1.0f);
		break;
	case WeaponType::MachineGun:
		weaponLocal *= XMMatrixScaling(0.6f, 0.6f, 0.6f);
		weaponLocal *= XMMatrixRotationRollPitchYaw(
			XMConvertToRadians(127.585f),
			XMConvertToRadians(95.144f),
			XMConvertToRadians(26.07899f));
		weaponLocal.pos = vec4(0.385f, 0.374f, 0.107f, 1.0f);
		break;
	case WeaponType::Shotgun:
		weaponLocal *= XMMatrixScaling(0.8f, 0.8f, 0.8f);
		weaponLocal *= XMMatrixRotationRollPitchYaw(
			XMConvertToRadians(16.847f),
			XMConvertToRadians(155.659f),
			XMConvertToRadians(-51.269f));
		weaponLocal.pos = vec4(0.489f, 0.605f, 0.091f, 1.0f);
		break;
	case WeaponType::Rifle:
		weaponLocal *= XMMatrixRotationRollPitchYaw(
			XMConvertToRadians(-15.929f),
			XMConvertToRadians(-26.220f),
			XMConvertToRadians(52.339f));
		weaponLocal.pos = vec4(0.030f, 0.368f, -0.034f, 1.0f);
		break;
	case WeaponType::Pistol:
	case WeaponType::DronePistol:
		weaponLocal *= XMMatrixScaling(0.7f, 0.7f, 0.7f);
		weaponLocal *= XMMatrixRotationRollPitchYaw(
			XMConvertToRadians(-80.942f),
			XMConvertToRadians(61.686f),
			XMConvertToRadians(32.417f));
		weaponLocal.pos = vec4(0.147f, 0.246f, 0.015f, 1.0f);
		break;
	}

	weaponLocal.pos.y += 0.035f * genericRecoilT * genericRecoilStrength;
	weaponLocal.pos.z -= 0.080f * genericRecoilT * genericRecoilStrength;

	// The stable aim hand supplies the attachment anchor while the local offset
	// provides visible firing kick without inheriting discontinuous clip positions.
	outWeaponWorld = weaponLocal * handWorld;
	return true;
}

void Player::RecvSTC_SyncObj(char* data) {
	int offset = 0;
	// A full SyncGameObject can arrive through both PersonalSDS and CommonSDS during a zone handoff.
	// Re-running SetShape would allocate another BLAS/TLAS instance for the same client object and leave
	// the previous instance permanently visible to DXR. Reuse the existing GPU resources instead.
	const bool hasRaytracingShapeResources = gd.isSupportRaytracing && RaytracingWorldMatInput_Model != nullptr;
	STC_SyncObjData& stcsod = *(STC_SyncObjData*)(data);
	if (stcsod.shapeindex >= 0 && stcsod.shapeindex < (int)Shape::ShapeTable.size()) shape = Shape::ShapeTable[stcsod.shapeindex]; else shape.FlagPtr = 0;   // [crash-fix] out-of-range shapeindex -> null shape (avoids garbage Model pointer)
	tag = stcsod.tag;
	parent = (stcsod.parent >= 0) ? game.DynmaicGameObjects[stcsod.parent] : nullptr;
	childs = (stcsod.childs >= 0) ? game.DynmaicGameObjects[stcsod.childs] : nullptr;
	sibling = (stcsod.sibling >= 0) ? game.DynmaicGameObjects[stcsod.sibling] : nullptr;
	XMMatrixDecompose((XMVECTOR*)&DestScale, (XMVECTOR*)&DestRot, (XMVECTOR*)&DestPos, stcsod.DestWorld);
	LVelocity = stcsod.LVelocity;
	//AnimationFlowTime[0] = stcsod.AnimationFlowTime;
	//PlayingAnimationIndex[0] = stcsod.PlayingAnimationIndex;
	HP = stcsod.HP;
	MaxHP = stcsod.MaxHP;
	Attack = stcsod.Attack;
	Defense = stcsod.Defense;
	bullets = stcsod.bullets;
	KillCount = stcsod.KillCount;
	DeathCount = stcsod.DeathCount;
	HeatGauge = stcsod.HeatGauge;
	MaxHeatGauge = stcsod.MaxHeatGauge;
	ShieldDurability = stcsod.ShieldDurability;
	MaxShieldDurability = stcsod.MaxShieldDurability;
	m_currentJob = stcsod.m_currentJob;
	memcpy(SkillCooldown, stcsod.SkillCooldown, sizeof(SkillCooldown));
	memcpy(SkillCooldownFlow, stcsod.SkillCooldownFlow, sizeof(SkillCooldownFlow));
	m_currentWeaponType = stcsod.m_currentWeaponType;
	m_weaponHolstered = stcsod.m_weaponHolstered;
	ReloadRemain = stcsod.ReloadRemain;
	m_sniperDmrMode = stcsod.m_sniperDmrMode;
	m_bomberHealAmmoMode = stcsod.m_bomberHealAmmoMode;
	isGround = stcsod.isGround;
	m_yaw = stcsod.m_yaw;
	m_pitch = stcsod.m_pitch;
	weapon[0] = stcsod.weapon[0];
	weapon[1] = stcsod.weapon[1];
	weapon[2] = stcsod.weapon[2];
	SelectedWeapon = stcsod.SelectedWeapon;
	Gold = stcsod.Gold;
	Exp = stcsod.Exp;
	Level = stcsod.Level;
	//memcpy(Inventory, stcsod.Inventory, maxItem * sizeof(ItemStack));
	offset += sizeof(STC_SyncObjData);

	//quest
	int& questCnt = *(int*)(data + offset);
	offset += sizeof(int);
	if (game.player == this) {
		game.QuestArr.clear();
		game.QuestArr.reserve(questCnt);
	}
	for (int i = 0; i < questCnt; ++i) {
		int& QI = *(int*)(data + offset);
		if (game.player == this) {
			game.QuestArr[i] = QI;
		}
		offset += sizeof(int);
	}

	Mesh* mesh = nullptr;
	Model* model = nullptr;
	shape.GetRealShape(mesh, model);
	if (mesh) {
		int& materialCount = *(int*)(data + offset);
		offset += sizeof(int);
		if (material != nullptr) {
			delete[] material;
			material = nullptr;
		}
		material = new int[materialCount];
		for (int i = 0;i < materialCount;++i) {
			material[i] = *(int*)(data + offset);
			offset += sizeof(int);
		}
	}
	else {
		int& NodeCount = *(int*)(data + offset);
		offset += sizeof(int);
		if (transforms_innerModel != nullptr) {
			delete[] transforms_innerModel;
			transforms_innerModel = nullptr;
		}
		transforms_innerModel = new matrix[NodeCount];
		for (int i = 0;i < NodeCount;++i) {
			transforms_innerModel[i] = *(matrix*)(data + offset);
			offset += sizeof(matrix);
		}
	}

	if (!hasRaytracingShapeResources) {
		SetShape(shape);
	}

	PlayerWeaponObjectInit();
	SetRaytracingVisualsEnabled(true);
}

void Player::ChangeState(State newState)
{
	if (m_currentState == newState) return;

	m_currentState = newState;
	AnimationFlowTime[0] = 0.0f;

	switch (m_currentState) {
	case State::IDLE:
		PlayingAnimationIndex[0] = 0;
		break;
	case State::WALK:
		PlayingAnimationIndex[0] = 1;
		break;
	case State::RUN:
		PlayingAnimationIndex[0] = 2;
		break;
	case State::JUMP:
		PlayingAnimationIndex[0] = 7;
		break;
	}
}

void Player::ChangeUpperState(UpperState newState, bool restart)
{
	int targetAnimationIndex = GetPlayerUpperAnimationIndex(
		(WeaponType)m_currentWeaponType, newState);
	if (!restart && m_currentUpperState == newState && PlayingAnimationIndex[1] == targetAnimationIndex) return;

	UpperState previousState = m_currentUpperState;
	if (previousState == UpperState::AIM) {
		m_upperAimPoseTime = AnimationFlowTime[1];
	}

	m_currentUpperState = newState;
	AnimationFlowTime[1] =
		(newState == UpperState::AIM && previousState == UpperState::SHOOT)
		? m_upperAimPoseTime
		: 0.0f;
	PlayingAnimationIndex[1] = targetAnimationIndex;
}

void Player::TriggerUpperShoot()
{
	if (m_weaponHolstered) return;

	const bool dualPistol = (WeaponType)m_currentWeaponType == WeaponType::DualPistol;
	m_upperShootHoldTimer = dualPistol ? 0.24f : 0.18f;
	// Each hand is a distinct revolver shot, so restart the dual-gun clip for the second hand.
	ChangeUpperState(UpperState::SHOOT, dualPistol || m_currentUpperState != UpperState::SHOOT);
}

void Player::TriggerDualPistolRecoil(bool leftHand)
{
	if ((WeaponType)m_currentWeaponType != WeaponType::DualPistol) return;
	if (leftHand) m_dualLeftRecoilTimer = 0.22f;
	else m_dualRightRecoilTimer = 0.22f;
}

void Player::Release() {
	// Player attachments are independent GameObjects with their own TLAS slots. MoveZone() releases
	// the Player directly (without necessarily receiving DeleteGameObject first), so releasing only
	// the skinned body leaves the last weapon/drone BLAS instances permanently visible in DXR.
	ClearThirdPersonWeaponVisuals();
	SetRaytracingVisualsEnabled(false);

	auto releaseAttachment = [](GameObject*& attachment) {
		if (attachment == nullptr) return;
		attachment->Release();
		delete attachment;
		attachment = nullptr;
	};
	for (int i = 0; i < Game::MaxWeapon; ++i) {
		releaseAttachment(PlayerWeaponObj[i]);
	}
	releaseAttachment(LeftHand);
	for (int i = 0; i < 2; ++i) {
		releaseAttachment(DronObj[i]);
		releaseAttachment(Knife[i]);
	}

	SkinMeshGameObject::Release();

	for (int i = 0; i < MaxWeapon; ++i) {
		if (PlayerWeaponObj[i]) {
			PlayerWeaponObj[i]->Release();
			delete PlayerWeaponObj[i];
			PlayerWeaponObj[i] = nullptr;
		}
	}
	if (LeftHand) {
		LeftHand->Release();
		delete LeftHand;
		LeftHand = nullptr;
	}
	for (int i = 0; i < 2; ++i) {
		if (DronObj[i]) {
			DronObj[i]->Release();
			delete DronObj[i];
			DronObj[i] = nullptr;
		}
		
		if (Knife[i]) {
			Knife[i]->Release();
			delete Knife[i];
			Knife[i] = nullptr;
		}
	}
}

void Portal::RecvSTC_SyncObj(char* data) {
	Portal::STC_SyncObjData& d = *(Portal::STC_SyncObjData*)data;
	if (d.shapeindex >= 0 && d.shapeindex < (int)Shape::ShapeTable.size()) shape = Shape::ShapeTable[d.shapeindex]; else shape.FlagPtr = 0;   // [crash-fix] out-of-range shapeindex -> null shape
	worldMat = d.DestWorld;
	spawnX = d.spawnX;
	spawnY = d.spawnY;
	spawnZ = d.spawnZ;
	radius = d.radius;
	zoneId = d.zoneId;
	dstzoneId = d.dstzoneId;
	//if (Mat == nullptr) continue;
}

PeacefulNPC::PeacefulNPC()
{
	tag = 0;
	tag[GameObjectTag::Tag_Enable] = false;
	tag[GameObjectTag::Tag_Dynamic] = true;
	tag[GameObjectTag::Tag_SkinMeshObject] = true;
	worldMat.Id();
}

void PeacefulNPC::Update(float deltaTime) {
	PositionInterpolation(deltaTime);
}

void PeacefulNPC::Render(matrix parent) {
	if (BoneToWorldMatrixCB.size() == 0) {
		return;
	}
	SkinMeshGameObject::Render(parent);
}

void PeacefulNPC::Release() {
	SkinMeshGameObject::Release();
}

void PeacefulNPC::RecvSTC_SyncObj(char* data) {
	int offset = 0;
	STC_SyncObjData& stcsod = *(STC_SyncObjData*)(data);
	if (stcsod.shapeindex >= 0) {
		shape = Shape::ShapeTable[stcsod.shapeindex];
	}
	tag = stcsod.tag;
	parent = (stcsod.parent >= 0) ? game.DynmaicGameObjects[stcsod.parent] : nullptr;
	childs = (stcsod.childs >= 0) ? game.DynmaicGameObjects[stcsod.childs] : nullptr;
	sibling = (stcsod.sibling >= 0) ? game.DynmaicGameObjects[stcsod.sibling] : nullptr;
	XMMatrixDecompose((XMVECTOR*)&DestScale, (XMVECTOR*)&DestRot, (XMVECTOR*)&DestPos, stcsod.DestWorld);
	LVelocity = stcsod.LVelocity;
	NPCType = stcsod.npctype;
	NPCID = stcsod.NPCID;

	Mesh* mesh = nullptr;
	Model* model = nullptr;
	shape.GetRealShape(mesh, model);
	if (mesh) {
		int& materialCount = *(int*)(data + offset);
		offset += sizeof(int);
		if (material != nullptr) {
			delete[] material;
			material = nullptr;
		}
		material = new int[materialCount];
		for (int i = 0; i < materialCount; ++i) {
			material[i] = *(int*)(data + offset);
			offset += sizeof(int);
		}
	}
	else {
		int& NodeCount = *(int*)(data + offset);
		offset += sizeof(int);
		if (transforms_innerModel != nullptr) {
			delete[] transforms_innerModel;
			transforms_innerModel = nullptr;
		}
		transforms_innerModel = new matrix[NodeCount];
		for (int i = 0; i < NodeCount; ++i) {
			transforms_innerModel[i] = *(matrix*)(data + offset);
			offset += sizeof(matrix);
		}
	}

	SetShape(shape);
}

void PeacefulNPC::STATICINIT(int typeindex) {
	SkinMeshGameObject::STATICINIT(typeindex);
	for (int i = 0; i < PeacefulNPC::g_member.size(); ++i) {
		g_member[i].offset = g_member[i].get_offset();
		MemberInfo& mi = *(MemberInfo*)&g_member[i];
		int lastindex = GameObjectType::Client_STCMembers[typeindex].size();
		GameObjectType::Client_STCMembers[typeindex].emplace_back(mi.name, mi.get_offset, mi.size);
		GameObjectType::Client_STCMembers[typeindex][lastindex].offset = g_member[i].offset;
	}
}

BulletRay::BulletRay()
{
}

BulletRay::BulletRay(vec4 s, vec4 dir, float dist) {
	start = s;
	direction = dir;
	distance = dist;
	t = 0;
}

matrix BulletRay::GetRayMatrix() {
	matrix mat;
	matrix smat;
	mat.LookAt(direction);
	float rate = t / remainTime;
	mat.right *= 1 - rate;
	mat.up *= 1 - rate;
	mat.look *= distance;
	mat.pos = start;
	mat.pos.w = 1;
	return mat;
}

void BulletRay::Update() {
	if (direction.fast_square_of_len3 > 0) {
		t += game.DeltaTime;
		if (t > remainTime) {
			direction = 0;
		}
	}
}

void BulletRay::Render() {
	if (direction.fast_square_of_len3 > 0) {
		matrix mat = GetRayMatrix();
		mat.transpose();
		gd.gpucmd->SetGraphicsRoot32BitConstants(1, 16, &mat, 0);
		mesh->Render(gd.gpucmd, 1);
	}
}

void GameMap::ExtendMapAABB(BoundingOrientedBox obb)
{
	vec4 aabb[2];
	gd.GetAABBFromOBB(aabb, obb, true);

	if (AABB[0].x > aabb[0].x) AABB[0].x = aabb[0].x;
	if (AABB[0].y > aabb[0].y) AABB[0].y = aabb[0].y;
	if (AABB[0].z > aabb[0].z) AABB[0].z = aabb[0].z;

	if (AABB[1].x < aabb[1].x) AABB[1].x = aabb[1].x;
	if (AABB[1].y < aabb[1].y) AABB[1].y = aabb[1].y;
	if (AABB[1].z < aabb[1].z) AABB[1].z = aabb[1].z;
}

void GameMap::GetStartDescIndexs()
{
	StartDesc_Init = gd.ShaderVisibleDescPool.InitDescArrSiz;
	StartDesc_Texture = gd.ShaderVisibleDescPool.TextureSRVSiz;
	StartDesc_Material = gd.ShaderVisibleDescPool.MaterialCBVSiz;
	StartDesc_Instancing = gd.ShaderVisibleDescPool.InstancingSRVSiz;
}

void GameMap::GetLastDescIndexs()
{
	LastDesc_Init = gd.ShaderVisibleDescPool.InitDescArrSiz - 1;
	LastDesc_Texture = gd.ShaderVisibleDescPool.TextureSRVSiz - 1;
	LastDesc_Material = gd.ShaderVisibleDescPool.MaterialCBVSiz - 1;
	LastDesc_Instancing = gd.ShaderVisibleDescPool.InstancingSRVSiz - 1;
}

bool GameMap::LoadMap(const char* MapName, int ZoneID)
{
	GameMap* map = this;
	map->ZoneID = ZoneID;
	Zone* zone = game.ZoneTable[map->ZoneID];
	StartShapeIndex = Shape::ShapeTable.size();
	map->GetStartDescIndexs();

	string dirName = "Resources/Map/";
	dirName += MapName;

	string MapFilePath = dirName;
	MapFilePath += "/";
	MapFilePath += MapName;
	MapFilePath += ".map";

	ifstream ifs{ MapFilePath, ios_base::binary };
	if (ifs.good() == false) {
		dirName = "../../SyncFPSServer/SyncFPSServer/Resources/Map/";
		dirName += MapName;

		MapFilePath = dirName;
		MapFilePath += "/";
		MapFilePath += MapName;
		MapFilePath += ".map";

		ifs.clear();
		ifs.open(MapFilePath, ios_base::binary);
	}

	if (ifs.good() == false) {
		string msg = "ERROR : Map file open failed - ";
		msg += MapFilePath;
		msg += "\n";
		OutputDebugStringA(msg.c_str());
		return false;
	}

	int nameCount;
	int MeshCount;
	int TextureCount;
	int MaterialCount;
	int ModelCount;
	int gameObjectCount;
	ifs.read((char*)&nameCount, sizeof(int));
	ifs.read((char*)&MeshCount, sizeof(int));
	ifs.read((char*)&TextureCount, sizeof(int));
	ifs.read((char*)&MaterialCount, sizeof(int));
	ifs.read((char*)&ModelCount, sizeof(int));
	ifs.read((char*)&gameObjectCount, sizeof(int));
	if (ifs.good() == false) {
		string msg = "ERROR : Map header read failed - ";
		msg += MapFilePath;
		msg += "\n";
		OutputDebugStringA(msg.c_str());
		return false;
	}
	if (nameCount < 0 || nameCount > 100000
		|| MeshCount < 0 || MeshCount > 100000
		|| TextureCount < 0 || TextureCount > 100000
		|| MaterialCount < 0 || MaterialCount > 100000
		|| ModelCount < 0 || ModelCount > 100000
		|| gameObjectCount < 0 || gameObjectCount > 1000000) {
		string msg = "ERROR : Invalid map header counts - ";
		msg += MapFilePath;
		msg += "\n";
		OutputDebugStringA(msg.c_str());
		return false;
	}
	map->name.reserve(nameCount);
	map->name.resize(nameCount);
	map->meshes.reserve(MeshCount);
	map->meshes.resize(MeshCount);
	map->models.reserve(ModelCount);
	map->models.resize(ModelCount);
	map->MapObjects.reserve(gameObjectCount);
	map->MapObjects.resize(gameObjectCount);

	for (int i = 0; i < nameCount; ++i) {
		char TempBuffer[1024] = {};
		string name;
		int namelen = 0;
		ifs.read((char*)&namelen, sizeof(int));
		ifs.read((char*)TempBuffer, sizeof(char) * (namelen));
		TempBuffer[namelen] = 0;
		name = TempBuffer;
		map->name[i] = name;
	}

	constexpr char MeshDir[] = "Mesh/";
	constexpr char TextureDir[] = "Texture/";
	constexpr char ModelDir[] = "Model/";
	string MeshDirPath = dirName;
	MeshDirPath += "/Mesh/";

	string TextureDirPath = dirName;
	TextureDirPath += "/Texture/";

	string ModelDirPath = dirName;
	ModelDirPath += "/Model/";

	for (int i = 0; i < MeshCount; ++i) {
		int nameid = 0;
		ifs.read((char*)&nameid, sizeof(int));
		string& name = map->name[nameid];

		string filename = MeshDirPath;
		//if (Mat == nullptr) continue;
		filename += name;
		filename += ".mesh";

		BumpMesh* mesh = new BumpMesh();
		ifstream ifs2{ filename, ios_base::binary };
		int vertCnt = 0;
		int subMeshCount = 0;
		ifs2.read((char*)&vertCnt, sizeof(int));
		ifs2.read((char*)&subMeshCount, sizeof(int));

		vector<BumpMesh::Vertex> verts;
		verts.reserve(vertCnt);
		verts.resize(vertCnt);
		for (int k = 0; k < vertCnt; ++k) {
			ifs2.read((char*)&verts[k].position, sizeof(float) * 3);
			ifs2.read((char*)&verts[k].u, sizeof(float));
			ifs2.read((char*)&verts[k].v, sizeof(float));
			ifs2.read((char*)&verts[k].normal, sizeof(float) * 3);
			ifs2.read((char*)&verts[k].tangent, sizeof(float) * 3);
			float w = 0; // bitangent direction
			ifs2.read((char*)&w, sizeof(float));
		}

		vector<TriangleIndex> indexs;
		int stackSiz = 0;
		int prevSiz = 0;
		int* SubMeshSlots = new int[subMeshCount + 1];
		SubMeshSlots[0] = 0;
		for (int k = 0; k < subMeshCount; ++k) {
			int indCnt = 0;
			ifs2.read((char*)&indCnt, sizeof(int));
			int tricnt = (indCnt / 3);
			stackSiz += tricnt;
			indexs.reserve(stackSiz);
			indexs.resize(stackSiz);
			for (int u = 0; u < tricnt; ++u) {
				ifs2.read((char*)&indexs[prevSiz + u].v[0], sizeof(UINT));
				ifs2.read((char*)&indexs[prevSiz + u].v[1], sizeof(UINT));
				ifs2.read((char*)&indexs[prevSiz + u].v[2], sizeof(UINT));
				verts[indexs[prevSiz + u].v[0]].materialIndex = k;
				verts[indexs[prevSiz + u].v[1]].materialIndex = k;
				verts[indexs[prevSiz + u].v[2]].materialIndex = k;
			}
			prevSiz = stackSiz;
			SubMeshSlots[k + 1] = prevSiz * 3;
		}

		ifs2.close();
		mesh->CreateMesh_FromVertexAndIndexData(verts, indexs, subMeshCount, SubMeshSlots, true, ZoneID);
		//mesh->ReadMeshFromFile_OBJ(filename.c_str(), vec4(1, 1, 1, 1), false);

		ifs.read((char*)&mesh->OBB_Tr, sizeof(float) * 3);
		ifs.read((char*)&mesh->OBB_Ext, sizeof(float) * 3);
		map->meshes[i] = mesh;
		Shape::AddMesh(name, mesh);
	}

	TextureTableStart = 0;
	MaterialTableStart = 0;
	int ZoneMaterialOff = (zone->Asset_OffsetMul + 1) * Zone::MAXZoneMaterialCount;
	int emptyTextureNameCount = 0;
	for (int i = 0; i < TextureCount; ++i) {
		int namelen = 0;
		ifs.read((char*)&namelen, sizeof(int));
		char TempBuff[512] = {};
		ifs.read((char*)&TempBuff, namelen);
		TempBuff[namelen] = 0;
		string textureName = TempBuff;

		GPUResource* texture = nullptr;
		if (textureName.empty()) {
			emptyTextureNameCount += 1;
			texture = new GPUResource();
			texture->CreateTexture_fromFile(L"Resources/DefaultTexture.dds", game.basicTexFormat, game.basicTexMip, false, ZoneID);
		}

		int width = 0;
		int height = 0;
		int format = 0; // Unity GraphicsFormat
		string filename_dds = TextureDirPath;
		filename_dds += textureName;
		filename_dds += ".dds";
		ifstream ifs2{ filename_dds, ios_base::binary };
		if (texture == nullptr) {
			texture = new GPUResource();
			if (ifs2.good()) {
				wchar_t ddsFile[512] = {};
				int len = filename_dds.size();
				for (int i = 0; i < len; ++i) ddsFile[i] = filename_dds[i];
				ddsFile[len] = 0;
				texture->CreateTexture_fromFile(ddsFile, DXGI_FORMAT_BC3_UNORM, game.basicTexMip, false, ZoneID);
			}
			else {
				string filename = TextureDirPath;
				filename += textureName;
				filename += ".tex";

				ifstream ifs2{ filename, ios_base::binary };

				ifs2.read((char*)&width, sizeof(int));
				ifs2.read((char*)&height, sizeof(int));

				BYTE* pixels = new BYTE[width * height * 4];
				ifs2.read((char*)pixels, width * height * 4);
				ifs2.close();

				imgform::PixelImageObject pio;
				pio.data = (imgform::RGBA_pixel*)pixels;
				pio.width = width;
				pio.height = height;
				string filename_bmp = TextureDirPath;
				filename_bmp += textureName;
				filename_bmp += ".bmp";
				pio.rawDataToBMP(filename_bmp);

				const char* lastSlash = strrchr(filename_dds.c_str(), L'/');
				lastSlash += 1;

				gd.bmpTodds(game.basicTexMip, game.basicTexFormatStr, filename_bmp.c_str());

				MoveFileA(lastSlash, filename_dds.c_str());

				wchar_t ddsFile[512] = {};
				int len = filename_dds.size();
				for (int i = 0; i < len; ++i) ddsFile[i] = filename_dds[i];
				ddsFile[len] = 0;
				texture->CreateTexture_fromFile(ddsFile, game.basicTexFormat, game.basicTexMip, false, ZoneID);

				DeleteFileA(filename_bmp.c_str());
				delete[] pixels;
			}
		}
		ifs.read((char*)&width, sizeof(int));
		ifs.read((char*)&height, sizeof(int));
		ifs.read((char*)&format, sizeof(int));

		zone->TextureTable.push_back(texture);
	}
	if (emptyTextureNameCount > 0) {
		std::wstringstream ss;
		ss << L"WARN : Empty texture entries found in map. dir = " << dirName.c_str() << L", count = " << emptyTextureNameCount << L"\n";
		OutputDebugStringW(ss.str().c_str());
	}

	for (int i = 0; i < MaterialCount; ++i) {
		Material* mat = new Material();

		int namelen = 0;
		ifs.read((char*)&namelen, sizeof(int));
		char name[512] = {};
		ifs.read((char*)name, namelen * sizeof(char));
		name[namelen] = 0;

		memcpy_s(mat->name, 40, name, 40);
		mat->name[39] = 0;

		ifs.read((char*)&mat->clr.diffuse, sizeof(float) * 4);

		ifs.read((char*)&mat->metallicFactor, sizeof(float));

		float smoothness = 0;
		ifs.read((char*)&smoothness, sizeof(float));
		mat->roughnessFactor = 1.0f - smoothness;

		ifs.read((char*)&mat->clr.bumpscaling, sizeof(float));

		vec4 tiling, offset = 0;
		vec4 tiling2, offset2 = 0;
		ifs.read((char*)&tiling, sizeof(float) * 2);
		ifs.read((char*)&offset, sizeof(float) * 2);
		ifs.read((char*)&tiling2, sizeof(float) * 2);
		ifs.read((char*)&offset2, sizeof(float) * 2);
		mat->TilingX = tiling.x;
		mat->TilingY = tiling.y;
		mat->TilingOffsetX = offset.x;
		mat->TilingOffsetY = offset.y;

		bool isTransparent = false;
		ifs.read((char*)&isTransparent, sizeof(bool));
		if (isTransparent) {
			mat->gltf_alphaMode = mat->Blend;
		}
		else mat->gltf_alphaMode = mat->Opaque;

		bool emissive = 0;
		ifs.read((char*)&emissive, sizeof(bool));

		ifs.read((char*)&mat->ti.BaseColor, sizeof(int));
		ifs.read((char*)&mat->ti.Normal, sizeof(int));
		ifs.read((char*)&mat->ti.Metalic, sizeof(int));
		ifs.read((char*)&mat->ti.AmbientOcculsion, sizeof(int));
		ifs.read((char*)&mat->ti.Roughness, sizeof(int));
		ifs.read((char*)&mat->ti.Emissive, sizeof(int));

		int diffuse2, normal2 = 0;
		ifs.read((char*)&diffuse2, sizeof(int));
		ifs.read((char*)&normal2, sizeof(int));

		//mat->ShiftTextureIndexs(TextureTableStart);
		mat->SetDescTable(ZoneID);

		zone->MaterialTable.push_back(mat);
		if (game.isAssetAddingInGlobal == false) {
			int renderIndex = (zone->Asset_OffsetMul + 1) * Zone::MAXZoneMaterialCount + game.RenderMaterialTableSizePerZone[1 + zone->Asset_OffsetMul];
			if (game.RenderMaterialTable[renderIndex] != nullptr) {
				dbglog1(L"Zone ���͸��� ������ �ȵ�. ZoneID : %d \n", ZoneID);
			}
			game.RenderMaterialTable[renderIndex] = mat;
			game.RenderMaterialTableSizePerZone[1 + zone->Asset_OffsetMul] += 1;
		}
	}

	for (int i = 0; i < ModelCount; ++i) {
		int namelen = 0;
		ifs.read((char*)&namelen, sizeof(int));
		char TempBuff[512] = {};
		ifs.read((char*)&TempBuff, namelen);
		TempBuff[namelen] = 0;

		string modelName = TempBuff;
		string filename = ModelDirPath;
		//if (Mat == nullptr) continue;
		filename += modelName;
		filename += ".model";

		Model* pModel = new Model();
		pModel->LoadModelFile2(filename, ZoneID);
		map->models[i] = pModel;
		Shape::AddModel(modelName, pModel);
	}

	for (int i = 0; i < gameObjectCount; ++i) {
		StaticGameObject* go = new StaticGameObject();
		map->MapObjects[i] = go;
	}
	for (int i = 0; i < gameObjectCount; ++i) {
		StaticGameObject* go = map->MapObjects[i];
		int nameId = 0;
		ifs.read((char*)&nameId, sizeof(int));

		vec4 pos;
		ifs.read((char*)&pos, sizeof(float) * 3);
		vec4 rot = 0;
		ifs.read((char*)&rot, sizeof(float) * 3);
		vec4 scale = 0;
		ifs.read((char*)&scale, sizeof(float) * 3);

		rot *= 3.141592f / 180.0f;
		matrix world;
		world.Id();
		world *= XMMatrixScaling(scale.x, scale.y, scale.z);
		world *= XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z);
		world.pos.f3 = pos.f3;
		world.pos.w = 1;

		// ���� ���� Map ������Ʈ�� ���, �ش� Zone �� �˸��� ��ġ�� �̵����Ѿ� �Ѵ�.
		if (i == 0) {
			world.pos.x = 0.5f * (zone->BasicAABB_onlyXZ.x + zone->BasicAABB_onlyXZ.z);
			world.pos.z = 0.5f * (zone->BasicAABB_onlyXZ.y + zone->BasicAABB_onlyXZ.w);
		}
		go->SetWorld(world);

		char Mod = 'n';
		ifs.read((char*)&Mod, sizeof(char));
		if (Mod == 'n') { // ismesh
			//dbgbreak(dbgc[0] == 7);
			int meshid = 0;
			int materialNum = 0;
			int materialId = 0;
			ifs.read((char*)&meshid, sizeof(int));
			if (meshid < 0) {
				go->SetShape((Mesh*)nullptr);
				ifs.read((char*)&materialNum, sizeof(int));
			}
			else {
				ifs.read((char*)&materialNum, sizeof(int));
				go->material = new int[materialNum];
				for (int k = 0; k < materialNum; ++k) {
					ifs.read((char*)&materialId, sizeof(int));
					if (materialId < 0) go->material[k] = 0;
					else {
						go->material[k] = ZoneMaterialOff + MaterialTableStart + materialId;
					}
				}
				go->SetShape(map->meshes[meshid]);
			}

			int ColliderCount = 0;
			ifs.read((char*)&ColliderCount, sizeof(int));
			go->obbArr.reserve(ColliderCount);
			go->obbArr.resize(ColliderCount);
			for (int k = 0; k < ColliderCount; ++k) {
				XMFLOAT3 Center, Extents;
				ifs.read((char*)&Center, sizeof(XMFLOAT3));
				ifs.read((char*)&Extents, sizeof(XMFLOAT3));
				BoundingOrientedBox obb = BoundingOrientedBox(Center, Extents, vec4(0, 0, 0, 1));
				BoundingOrientedBox obb_world;
				obb.Transform(obb_world, go->worldMat);
				go->obbArr[k] = obb_world;
			}
		}
		else if (Mod == 'm') {

			// is model
			//go->rmod = GameObject::eRenderMeshMod::_Model;
			//Model

			int ModelID;
			ifs.read((char*)&ModelID, sizeof(int));
			int nodeCount = map->models[ModelID]->nodeCount;
			go->transforms_innerModel = new matrix[nodeCount];
			for (int k = 0; k < nodeCount; ++k) {
				vec4 pos;
				ifs.read((char*)&pos, sizeof(float) * 3);
				vec4 rot = 0;
				ifs.read((char*)&rot, sizeof(float) * 3);
				vec4 scale = 0;
				ifs.read((char*)&scale, sizeof(float) * 3);

				rot *= 3.141592f / 180.0f;
				matrix world;
				world.Id();
				world *= XMMatrixScaling(scale.x, scale.y, scale.z);
				world *= XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z);
				world.pos.f3 = pos.f3;
				world.pos.w = 1;
				if (k != 0)
					go->transforms_innerModel[k] = world;
				else
					go->transforms_innerModel[k].Id();
			}

			if (ModelID < 0) {
				go->SetShape((Model*)nullptr);
			}
			else {
				go->SetShape(map->models[ModelID]);
			}
		}
		else if (Mod == 'l') {
			// is light
			LightType lt;
			int n = 0;
			ifs.read((char*)&n, sizeof(int));
			lt = (LightType)n;

			go->SetShape((Mesh*)nullptr);
			go->material = nullptr;

			Light* lptr = new Light();
			lptr->lightType = lt;
			lptr->pos = go->worldMat.pos;
			lptr->dir = go->worldMat.look;

			ifs.read((char*)&lptr->range, sizeof(float));
			ifs.read((char*)&lptr->intencity, sizeof(float));
			ifs.read((char*)&lptr->spot_angle, sizeof(float));
			ifs.read((char*)&lptr->LightColor, sizeof(vec4));

			lptr->GenerateLight();
			zone->LightTable.push_back(lptr);
		}

		if (Mod != 'm') { // is not model
			int childCount = 0;
			ifs.read((char*)&childCount, sizeof(int));
			int cnt = 0;
			GameObject** temp = &go->childs;
			while (cnt < childCount) {
				int childIndex = 0;
				ifs.read((char*)&childIndex, sizeof(int));
				*temp = map->MapObjects[childIndex];
				(*temp)->parent = go;
				temp = &(*temp)->sibling;
				cnt += 1;
			}
		}
		map->MapObjects[i] = go;
		if (gd.isSupportRaytracing) {
			map->MapObjects[i]->RaytracingUpdateTransform();
		}
	}

	map->MapObjects[0]->DbgHieraky();
	map->GetLastDescIndexs();
	//BakeStaticCollision();
	return true;
}

void GameMap::Release() {
	name.clear();

	for (int i = 0; i < MapObjects.size(); ++i) {
		MapObjects[i]->Release();
		delete MapObjects[i];
		MapObjects[i] = nullptr;
	}
	MapObjects.clear();

	for (int i = 0; i < meshes.size(); ++i) {
		meshes[i]->Release();
		delete meshes[i];
		meshes[i] = nullptr;
	}
	meshes.clear();

	for (int i = 0; i < models.size(); ++i) {
		models[i]->Release(false);
		delete models[i];
		models[i] = nullptr;
	}
	models.clear();

	StartShapeIndex = 0;
	StartDesc_Init = 0;
	StartDesc_Texture = 0;
	StartDesc_Material = 0;
	StartDesc_Instancing = 0;
	LastDesc_Init = 0;
	LastDesc_Texture = 0;
	LastDesc_Material = 0;
	LastDesc_Instancing = 0;
	TextureTableStart = 0;
	MaterialTableStart = 0;
	AABB[0] = 0;
	AABB[1] = 0;
}

void SphereLODObject::Update(float deltaTime)
{
	worldMat.pos = FixedPos;
}

void SphereLODObject::Render(matrix parent)
{
	if (MeshNear && MeshFar)
	{
		vec4 cam = gd.viewportArr[0].Camera_Pos;
		vec4 pos = worldMat.pos;

		vec4 d = cam - pos;
		float dist = d.len3;

		shape.SetMesh((dist < SwitchDistance) ? MeshNear : MeshFar);
	}

	GameObject::Render();
}

void GameChunk::SetChunkIndex(ChunkIndex ci) {
	cindex = ci;
	AABB = ci.GetAABB();
}

void GameChunk::Release() {
	Static_gameobjects.Release();
	Dynamic_gameobjects.Release();
	SkinMesh_gameobjects.Release();
	cindex = ChunkIndex(0, 0, 0);
	TourID = 0;
}

void GameChunk::Release_Asset()
{
	Lights.clear();
	Static_gameobjects.Alloter.Clear();
	Dynamic_gameobjects.Alloter.Clear();
	SkinMesh_gameobjects.Alloter.Clear();
}

void Light::GenerateLight()
{
}

BoundingOrientedBox Light::GetOBB() {
	constexpr float sqrt2 = 1.414213562f;
	vec4 extents = range * sqrt2;
	return BoundingOrientedBox(pos.f3, extents.f3, vec4(0, 0, 0, 1));
}

BoundingBox ChunkIndex::GetAABB() {
	BoundingBox AABB;
	float halfW = Zone::chunck_divide_Width * 0.5f;
	AABB.Center = XMFLOAT3(Zone::chunck_divide_Width * x + halfW, Zone::chunck_divide_Width * y + halfW, Zone::chunck_divide_Width * z + halfW);
	AABB.Extents = XMFLOAT3(halfW, halfW, halfW);
	return AABB;
}

DXUI::DXUI(DXUI_TYPE t, int PCapacity, vec4 loc, float d, void* pPData)
{
	enable = true;
	type = t;
	ParameterData_Capacity = PCapacity;
	location = loc;
	if (pPData) {
		pParamterData = new char[ParameterData_Capacity];
	}
	else {
		pParamterData = pPData;
	}
	depth = d;
}

void DXWindowParam::NormalizeCoordToWindowCoord_vec4(vec4& loc)
{
	float W = origin->location.z - origin->location.x;
	float H = origin->location.w - origin->location.y;
	loc -= 0.5f;
	loc * 2.0f;
	loc.x *= W;
	loc.z *= W;
	loc.y *= -H;
	loc.w *= -H;
	swap(loc.y, loc.w);
}

DXUI* DXWindowParam::GetSlotUIFromPos(vec4 pos) {
	if (page_stack.size() == 0) return nullptr;
	DXPage* lastPage = page_stack[page_stack.size() - 1];
	return lastPage->GetSlotUIFromPos(pos);
}

void DXWindowParam::AlignUIDepth() {
	depthlevel_Count = page_stack.size();
	for (int i = 0; i < depthlevel_Count; ++i) {
		page_stack[i]->depth_min = GetDepth(i);
		page_stack[i]->depth_max = GetDepth(i + 1);
		page_stack[i]->AlignUIDepth();
	}
}

NPCTalkData::NPCTalkData(const wchar_t* speaker, const wchar_t* txt, bool nxtEsc, TalkSelection sel1, TalkSelection sel2, TalkSelection sel3, TalkSelection sel4)
{
	SpeakerName = speaker;
	text = txt;
	NextEscape = nxtEsc;
	if (sel1.selectionText == nullptr) {
		selectCnt = 0;
		sel[0] = sel1;
	}
	else if (sel2.selectionText == nullptr) {
		selectCnt = 1;
		sel[0] = sel1;
		sel[1] = sel2;
	}
	else if (sel3.selectionText == nullptr) {
		selectCnt = 2;
		sel[0] = sel1;
		sel[1] = sel2;
		sel[2] = sel3;
	}
	else if (sel4.selectionText == nullptr) {
		selectCnt = 3;
		sel[0] = sel1;
		sel[1] = sel2;
		sel[2] = sel3;
		sel[3] = sel4;
	}
}

void Player::UpdateThirdPersonWeaponAttachmentCache()
{
	matrix rightHandWorld;
	if (TryGetHumanoidBoneWorldMatrix(this, HumanoidAnimation::RightHand, rightHandWorld)) {
		cachedThirdPersonRightHandMatrix = rightHandWorld;
		cachedThirdPersonRightHandMatrixValid = true;
	}
	else {
		cachedThirdPersonRightHandMatrixValid = false;
	}
	matrix leftHandWorld;
	if (TryGetHumanoidBoneWorldMatrix(this, HumanoidAnimation::LeftHand, leftHandWorld)) {
		cachedThirdPersonLeftHandMatrix = leftHandWorld;
		cachedThirdPersonLeftHandMatrixValid = true;
	}
	else {
		cachedThirdPersonLeftHandMatrixValid = false;
	}

	matrix weaponWorld;
	if (TryBuildThirdPersonWeaponMatrix(this, false, weaponWorld)) {
		cachedThirdPersonWeaponMatrix = weaponWorld;
		cachedThirdPersonWeaponType = GetPlayerWeaponVisualKey(this);
		cachedThirdPersonWeaponMatrixValid = true;
	}
	else {
		cachedThirdPersonWeaponMatrixValid = false;
		cachedThirdPersonWeaponType = -1;
	}

	if ((WeaponType)m_currentWeaponType == WeaponType::DualPistol) {
		matrix leftWeaponWorld;
		if (TryBuildThirdPersonWeaponMatrix(this, true, leftWeaponWorld)) {
			cachedThirdPersonLeftWeaponMatrix = leftWeaponWorld;
			cachedThirdPersonLeftWeaponMatrixValid = true;
		}
		else {
			cachedThirdPersonLeftWeaponMatrixValid = false;
		}
	}
	else {
		cachedThirdPersonLeftWeaponMatrixValid = false;
	}
}

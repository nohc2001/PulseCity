#include "stdafx.h"
#include "Render.h"
#include "Game.h"
#include "GameObject.h"

extern int dbgc[128];

Mesh* BulletRay::mesh = nullptr;
ViewportData* Game::renderViewPort = nullptr;
extern GlobalDevice gd;
extern Game game;

void GameObject::StaticInit()
{
	StaticGameObject sgo;
	Vptr<StaticGameObject> = *(void**)&sgo;
	DynamicGameObject dgo;
	Vptr<DynamicGameObject> = *(void**)&dgo;
	SkinMeshGameObject smgo;
	Vptr<SkinMeshGameObject> = *(void**)&smgo;
}

GameObject::GameObject()
{
}

GameObject::~GameObject()
{
	if (shape.isMesh()) {
		if (material) {
			delete[] material;
			material = nullptr;
		}
	}
	else {
		if (transforms_innerModel) {
			delete[] transforms_innerModel;
			transforms_innerModel = nullptr;
		}
	}
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

	/*obb_local.Transform(obb, world);
	b = (game.renderViewPort->*game.renderViewPort->FrustumIntersectFunc)(obb);
	if (b == false) return;*/

	if (mesh != nullptr) {
		matrix rootWorld = world;
		rootWorld.transpose();
		BumpMesh* Bmesh = (BumpMesh*)mesh;
		gd.gpucmd->SetGraphicsRoot32BitConstants(1, 16, &rootWorld, 0);
		for (int i = 0; i < Bmesh->subMeshNum; ++i) {
			Material& Mat = game.MaterialTable[material[i]];
			using PBRRPI = PBRShader1::RootParamId;
			gd.gpucmd->SetGraphicsRootDescriptorTable(PBRRPI::CBVTable_Material, Mat.CB_Resource.descindex.hRender.hgpu);
			gd.gpucmd->SetGraphicsRootDescriptorTable(PBRRPI::SRVTable_MaterialTextures, Mat.TextureSRVTableIndex.hRender.hgpu);
		}
		mesh->Render(gd.gpucmd, 1);
	}
	else {
		model->Render(gd.gpucmd, world, this);
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

	/*obb_local.Transform(obb, world);
	b = (game.renderViewPort->*game.renderViewPort->FrustumIntersectFunc)(obb);
	if (b == false) return;*/

	if (mesh != nullptr) {
		matrix rootWorld = world;
		rootWorld.transpose();
		BumpMesh* Bmesh = (BumpMesh*)mesh;
		for (int i = 0; i < Bmesh->subMeshNum; ++i) {
			Bmesh->InstanceData[i].PushInstance(RenderInstanceData(rootWorld, material[i]));
		}
	}
	else {
		model->PushRenderBatch(world, this);
	}
}

void GameObject::Release()
{
	tag[GameObjectTag::Tag_Enable] = false;
	if (transforms_innerModel) {
		delete[] transforms_innerModel;
	}
}

BoundingOrientedBox GameObject::GetOBB()
{
	BoundingOrientedBox obb_local;
	Mesh* mesh = nullptr;
	Model* model = nullptr;
	shape.GetRealShape(mesh, model);
	if (mesh != nullptr) obb_local = mesh->GetOBB();
	if (model != nullptr) obb_local = model->GetOBB();
	BoundingOrientedBox obb_world;
	obb_local.Transform(obb_world, worldMat);
	return obb_world;
}

void GameObject::SetShape(Shape _shape)
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
		}

		if (gd.isSupportRaytracing) {
			RaytracingWorldMatInput_Model = new float** [model->nodeCount];
			for (int i = 0; i < model->nodeCount; ++i) {
				RaytracingWorldMatInput_Model[i] = nullptr;
				if (model->Nodes[i].numMesh > 0) {
					BumpMesh* bmesh = (BumpMesh*)model->mMeshes[model->Nodes[i].Meshes[0]];
					for (int k = 0; k < bmesh->subMeshNum; ++k) {
						tempLRSSaver[k] = LocalRootSigData(bmesh->rmesh.VBStartOffset / sizeof(RayTracingMesh::Vertex), bmesh->rmesh.IBStartOffset[k] / sizeof(unsigned int));
					}

					float** WorldMatInputs = game.MyRayTracingShader->push_rins_immortal(&bmesh->rmesh, worldMat, tempLRSSaver);
					RaytracingWorldMatInput_Model[i] = new float* [bmesh->subMeshNum];
					for (int k = 0; k < bmesh->subMeshNum; ++k) {
						RaytracingWorldMatInput_Model[i][k] = WorldMatInputs[k];
					}
				}
			}
			RaytracingUpdateTransform();
		}
	}
	if (mesh != nullptr) {
		material = new int[mesh->subMeshNum];
		for (int i = 0; i < mesh->subMeshNum; ++i) {
			material[i] = 0;
		}
		if (gd.isSupportRaytracing) {
			BumpMesh* bmesh = (BumpMesh*)mesh;
			for (int i = 0; i < bmesh->subMeshNum; ++i) {
				tempLRSSaver[i] = LocalRootSigData(bmesh->rmesh.VBStartOffset / sizeof(RayTracingMesh::Vertex), bmesh->rmesh.IBStartOffset[i] / sizeof(unsigned int));
			}
			float** WorldMatInputs = game.MyRayTracingShader->push_rins_immortal(&bmesh->rmesh, worldMat, tempLRSSaver);
			RaytracingWorldMatInput = new float* [bmesh->subMeshNum];
			for (int i = 0; i < bmesh->subMeshNum; ++i) {
				RaytracingWorldMatInput[i] = WorldMatInputs[i];
			}

			RaytracingUpdateTransform();
		}
	}
}

void GameObject::SetShape(Model* _shape)
{
	SetShape(Shape(_shape));
}

void GameObject::SetShape(Mesh* _shape)
{
	SetShape(Shape(_shape));
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
			for (int i = 0; i < mesh->subMeshNum; ++i) {
				memcpy(RaytracingWorldMatInput[i], &mat, sizeof(float) * 12);
			}
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
		for (int i = 0; i < model->mMeshes[node->Meshes[0]]->subMeshNum; ++i) {
			memcpy(RaytracingWorldMatInput_Model[nodeIndex][i], &mat, sizeof(float) * 12);
		}
	}
}

StaticGameObject::StaticGameObject()
{
}

StaticGameObject::~StaticGameObject()
{
	aabbArr.clear();
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

void StaticGameObject::Release() {
	GameObject::Release();
}

void DynamicGameObject::InitialChunkSetting()
{
	if (chunkAllocIndexs == nullptr) {
		BoundingOrientedBox obb = GetOBB();
		vec4 ext = obb.Extents;
		float len = ext.len3 / game.chunck_divide_Width;
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
				GameChunk* gc = game.chunck[ChunkIndex(ix, iy, iz)];
				gc->Dynamic_gameobjects.Free(chunkAllocIndexs[up]);
				up += 1;
			}
		}
	}

	// 위치 이동 / 회전
	vec4 pos = worldMat.pos;
	worldMat.trQ(Q);
	worldMat.pos = pos + velocity;
	worldMat.pos.w = 1;

	// 포함 청크 탐색
	IncludeChunks = game.GetChunks_Include_OBB(GetOBB());

	xmax = IncludeChunks.xmin + IncludeChunks.xlen;
	ymax = IncludeChunks.ymin + IncludeChunks.ylen;
	zmax = IncludeChunks.zmin + IncludeChunks.zlen;
	up = 0;
	for (int ix = IncludeChunks.xmin; ix <= xmax; ++ix) {
		for (int iy = IncludeChunks.ymin; iy <= ymax; ++iy) {
			for (int iz = IncludeChunks.zmin; iz <= zmax; ++iz) {
				auto c = game.chunck.find(ChunkIndex(ix, iy, iz));
				GameChunk* gc;
				if (c == game.chunck.end()) {
					// new game chunk
					gc = new GameChunk();
					gc->SetChunkIndex(ChunkIndex(ix, iy, iz));
					game.chunck.insert(pair<ChunkIndex, GameChunk*>(ChunkIndex(ix, iy, iz), gc));
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
				game.chunck[ChunkIndex(ix, iy, iz)]->Dynamic_gameobjects.Free(chunkAllocIndexs[up]);
				up += 1;
			}
		}
	}

	// 위치 이동 / 회전
	vec4 pos = worldMat.pos;
	worldMat.trQ(Q);
	worldMat.pos = pos + velocity;
	worldMat.pos.w = 1;

	// 포함 청크 탐색
	IncludeChunks = afterChunkInc;
	xmax = IncludeChunks.xmin + IncludeChunks.xlen;
	ymax = IncludeChunks.ymin + IncludeChunks.ylen;
	zmax = IncludeChunks.zmin + IncludeChunks.zlen;
	up = 0;
	for (int ix = IncludeChunks.xmin; ix <= xmax; ++ix) {
		for (int iy = IncludeChunks.ymin; iy <= ymax; ++iy) {
			for (int iz = IncludeChunks.zmin; iz <= zmax; ++iz) {
				auto c = game.chunck.find(ChunkIndex(ix, iy, iz));
				GameChunk* gc;
				if (c == game.chunck.end()) {
					// new game chunk
					gc = new GameChunk();
					gc->SetChunkIndex(ChunkIndex(ix, iy, iz));
					game.chunck.insert(pair<ChunkIndex, GameChunk*>(ChunkIndex(ix, iy, iz), gc));
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
	GameObject::Release();
	if (chunkAllocIndexs) {
		int xmax = IncludeChunks.xmin + IncludeChunks.xlen;
		int ymax = IncludeChunks.ymin + IncludeChunks.ylen;
		int zmax = IncludeChunks.zmin + IncludeChunks.zlen;
		int up = 0;
		for (int ix = IncludeChunks.xmin; ix <= xmax; ++ix) {
			for (int iy = IncludeChunks.ymin; iy <= ymax; ++iy) {
				for (int iz = IncludeChunks.zmin; iz <= zmax; ++iz) {
					game.chunck[ChunkIndex(ix, iy, iz)]->Dynamic_gameobjects.Free(chunkAllocIndexs[up]);
					up += 1;
				}
			}
		}
		delete[] chunkAllocIndexs;
	}
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

void DynamicGameObject::CollisionMove(DynamicGameObject* gbj1, DynamicGameObject* gbj2)
{
	constexpr float epsillon = 0.01f;

	bool bi = XMColorEqual(gbj1->tickLVelocity, XMVectorZero());/*
	bool bia = XMColorEqual(gbj1->tickAVelocity, XMVectorZero());*/
	bool bj = XMColorEqual(gbj2->tickLVelocity, XMVectorZero());/*
	bool bja = XMColorEqual(gbj2->tickAVelocity, XMVectorZero());*/

	DynamicGameObject* movObj = nullptr;
	DynamicGameObject* colObj = nullptr;
	BoundingOrientedBox obb1 = gbj1->GetOBB();
	BoundingOrientedBox obb2 = gbj2->GetOBB();
	//float len = vec4(gbj1->worldMat.pos - gbj2->worldMat.pos).len3;
	//float distance = vec4(obb1.Extents).len3 + vec4(obb2.Extents).len3;
	//if (len < distance) {
	//Collision_By_Rotations:
	//	if (!bia && bja) {
	//		movObj = gbj1;
	//		colObj = gbj2;
	//		goto Collision_byRotation_static_vs_dynamic;
	//	}
	//	else if (bia && !bja) {
	//		movObj = gbj2;
	//		colObj = gbj1;
	//		goto Collision_byRotation_static_vs_dynamic;
	//	}
	//	else if (!bia && !bja) {
	//		goto Collision_By_Move;
	//	}
	//	else {
	//		goto Collision_By_Move;
	//	}
	//Collision_byRotation_static_vs_dynamic:
	//	OBB_vertexVector ovv = movObj->GetOBBVertexs();
	//	movObj->worldMat.trQ(movObj->tickAVelocity);
	//	obb1 = movObj->GetOBB();
	//	OBB_vertexVector ovv_later = movObj->GetOBBVertexs();
	//	movObj->worldMat.trQinv(movObj->tickAVelocity);
	//	matrix imat = colObj->worldMat.RTInverse;
	//	obb2 = colObj->GetOBB();
	//	vec4 RayPos;
	//	vec4 RayDir;
	//	for (int xi = 0; xi < 2; ++xi) {
	//		for (int yi = 0; yi < 2; ++yi) {
	//			for (int zi = 0; zi < 2; ++zi) {
	//				ovv_later.vertex[xi][yi][zi] *= imat;
	//				if (obb2.Contains(ovv_later.vertex[xi][yi][zi])) {
	//					ovv.vertex[xi][yi][zi] *= imat;
	//					RayPos = ovv.vertex[xi][yi][zi];
	//					RayDir = ovv_later.vertex[xi][yi][zi] - ovv.vertex[xi][yi][zi];
	//					if (fabsf(RayDir.x) > epsillon) {
	//						float Ex = obb2.Extents.x * (RayDir.x / fabsf(RayDir.x));
	//						float A = (Ex - RayPos.x) / RayDir.x;
	//						vec4 colpos = vec4(RayPos.x + RayDir.x, RayDir.y * A + RayPos.y, RayDir.z * A + RayPos.z);
	//						vec4 bound; bound.f3 = obb2.Extents;
	//						if (colpos.is_in_bound(bound)) {
	//							movObj->tickLVelocity.x += colpos.x - Ex;
	//							goto Collision_By_Move;
	//						}
	//					}
	//					if (fabsf(RayDir.y) > epsillon) {
	//						float Ex = obb2.Extents.y * (RayDir.y / fabsf(RayDir.y));
	//						float A = (Ex - RayPos.y) / RayDir.y;
	//						vec4 colpos = vec4(RayDir.x * A + RayPos.x, RayPos.y + RayDir.y, RayDir.z * A + RayPos.z);
	//						vec4 bound; bound.f3 = obb2.Extents;
	//						if (colpos.is_in_bound(bound)) {
	//							movObj->tickLVelocity.y += colpos.y - Ex;
	//							goto Collision_By_Move;
	//						}
	//					}
	//					if (fabsf(RayDir.z) > epsillon) {
	//						float Ex = obb2.Extents.z * (RayDir.z / fabsf(RayDir.z));
	//						float A = (Ex - RayPos.z) / RayDir.z;
	//						vec4 colpos = vec4(RayDir.x * A + RayPos.x, RayDir.y * A + RayPos.y, RayPos.z + RayDir.z);
	//						vec4 bound; bound.f3 = obb2.Extents;
	//						if (colpos.is_in_bound(bound)) {
	//							movObj->tickLVelocity.z += colpos.z - Ex;
	//							goto Collision_By_Move;
	//						}
	//					}
	//				}
	//			}
	//		}
	//	}
	//}

Collision_By_Move:
	//if (bia) {
	//	gbj1->worldMat.trQ(gbj1->tickAVelocity);
	//}
	//if (bja) {
	//	gbj2->worldMat.trQ(gbj2->tickAVelocity);
	//}

	if (!bi && bj) {
		// i : moving GameObject
		// j : Collision Check GameObject
		movObj = gbj1;
		colObj = gbj2;
		goto Collision_By_Move_static_vs_dynamic;
	}
	else if (bi && !bj) {
		// i : Collision Check GameObject
		// j : moving GameObject
		movObj = gbj2;
		colObj = gbj1;
		goto Collision_By_Move_static_vs_dynamic;
	}
	else if (!bi && !bj) {
		// i : moving GameObject
		// j : moving GameObject

		float mul = 1.0f;
		float rate = 0.5f;
		float maxLen = XMVector3Length(gbj1->tickLVelocity).m128_f32[0];
		float temp = XMVector3Length(gbj2->tickLVelocity).m128_f32[0];
		if (maxLen < temp) maxLen = temp;

	CMP_INTERSECT:
		XMVECTOR v1 = mul * gbj1->tickLVelocity;
		XMVECTOR v2 = mul * gbj2->tickLVelocity;
		gbj1->worldMat.pos += v1;
		obb1 = gbj1->GetOBB();
		gbj1->worldMat.pos -= v1;
		gbj2->worldMat.pos += v2;
		obb2 = gbj2->GetOBB();
		gbj2->worldMat.pos -= v2;
		if (obb1.Intersects(obb2)) {
			mul -= rate;
			rate *= 0.5f;
			if (maxLen * rate > epsillon) goto CMP_INTERSECT;
		}
		else {
			mul += rate;
			rate *= 0.5f;
			if (maxLen * rate > epsillon) goto CMP_INTERSECT;
		}

		gbj1->tickLVelocity = v1;
		gbj2->tickLVelocity = v2;

		return;
	}
	else {
		//no move
		return;
	}

Collision_By_Move_static_vs_dynamic:
	movObj->worldMat.pos += movObj->tickLVelocity;
	obb1 = movObj->GetOBB();
	movObj->worldMat.pos -= movObj->tickLVelocity;
	obb2 = colObj->GetOBB();

	if (obb1.Intersects(obb2)) {
		movObj->OnCollision(colObj);
		colObj->OnCollision(movObj);

		XMMATRIX basemat = colObj->worldMat;
		XMMATRIX invmat = colObj->worldMat.RTInverse;
		invmat.r[3] = XMVectorSet(0, 0, 0, 1);
		XMVECTOR BaseLine = XMVector3Transform(movObj->tickLVelocity, invmat);
		movObj->tickLVelocity = XMVectorZero();

		XMVECTOR MoveVector = basemat.r[0] * BaseLine.m128_f32[0];
		movObj->tickLVelocity += MoveVector;
		movObj->worldMat.pos += movObj->tickLVelocity;
		obb1 = movObj->GetOBB();
		movObj->worldMat.pos -= movObj->tickLVelocity;
		if (obb1.Intersects(obb2)) {
			movObj->tickLVelocity -= MoveVector;
		}

		MoveVector = basemat.r[1] * BaseLine.m128_f32[1];
		movObj->tickLVelocity += MoveVector;
		movObj->worldMat.pos += movObj->tickLVelocity;
		obb1 = movObj->GetOBB();
		movObj->worldMat.pos -= movObj->tickLVelocity;
		if (obb1.Intersects(obb2)) {
			movObj->tickLVelocity -= MoveVector;
		}

		MoveVector = basemat.r[2] * BaseLine.m128_f32[2];
		movObj->tickLVelocity += MoveVector;
		movObj->worldMat.pos += movObj->tickLVelocity;
		obb1 = movObj->GetOBB();
		movObj->worldMat.pos -= movObj->tickLVelocity;
		if (obb1.Intersects(obb2)) {
			movObj->tickLVelocity -= MoveVector;
		}
	}

	//if (bia) {
	//	gbj1->worldMat.trQinv(gbj1->tickAVelocity);
	//}
	//if (bja) {
	//	gbj2->worldMat.trQinv(gbj2->tickAVelocity);
	//}
}

void DynamicGameObject::OnCollision(GameObject* other)
{
	//GameObject Collision...
}

void DynamicGameObject::OnRayHit(GameObject* rayFrom)
{
}

void DynamicGameObject::SetShape(Shape _shape)
{
	GameObject::SetShape(_shape);
}

bool godmod = true;
DynamicGameObject::DynamicGameObject() :
	chunkAllocIndexs{ nullptr }
{
	tickLVelocity = vec4(0, 0, 0, 0);
	tickAVelocity = vec4(0, 0, 0, 0);
	LastQuerternion = vec4(0, 0, 0, 0);
	chunkAllocIndexsCapacity = 0;
	ZeroMemory(&IncludeChunks, sizeof(GameObjectIncludeChunks));
}

DynamicGameObject::~DynamicGameObject()
{
	if (chunkAllocIndexs) {
		delete[] chunkAllocIndexs;
		chunkAllocIndexs = nullptr;
	}
}

void DynamicGameObject::Update(float delatTime)
{
	PositionInterpolation(delatTime);
}

void DynamicGameObject::PositionInterpolation(float deltaTime)
{
	float pow = deltaTime * 10;
	Destpos.w = 1;
	worldMat.pos = (1.0f - pow) * worldMat.pos + pow * Destpos;
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

void GameMap::LoadMap(const char* MapName)
{
	GameMap* map = this;

	string dirName = "Resources/Map/";
	dirName += MapName;

	string MapFilePath = dirName;
	MapFilePath += "/";
	MapFilePath += MapName;
	MapFilePath += ".map";

	ifstream ifs{ MapFilePath, ios_base::binary };
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
		// .map (확장자)제거
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
			float w = 0; // bitangent direction??
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
			for (int k = 0; k < tricnt; ++k) {
				ifs2.read((char*)&indexs[prevSiz + k].v[0], sizeof(UINT));
				ifs2.read((char*)&indexs[prevSiz + k].v[1], sizeof(UINT));
				ifs2.read((char*)&indexs[prevSiz + k].v[2], sizeof(UINT));
			}
			prevSiz = stackSiz;
			SubMeshSlots[k + 1] = prevSiz * 3;
		}

		ifs2.close();
		mesh->CreateMesh_FromVertexAndIndexData(verts, indexs, subMeshCount, SubMeshSlots);
		//mesh->ReadMeshFromFile_OBJ(filename.c_str(), vec4(1, 1, 1, 1), false);

		ifs.read((char*)&mesh->OBB_Tr, sizeof(float) * 3);
		ifs.read((char*)&mesh->OBB_Ext, sizeof(float) * 3);
		map->meshes[i] = mesh;
	}

	TextureTableStart = game.TextureTable.size();
	MaterialTableStart = game.MaterialTable.size();
	for (int i = 0; i < TextureCount; ++i) {
		int namelen = 0;
		ifs.read((char*)&namelen, sizeof(int));
		char TempBuff[512] = {};
		ifs.read((char*)&TempBuff, namelen);
		TempBuff[namelen] = 0;
		string textureName = TempBuff;

		int width = 0;
		int height = 0;
		int format = 0; // Unity GraphicsFormat

		string filename_dds = TextureDirPath;
		filename_dds += textureName;
		filename_dds += ".dds";
		ifstream ifs2{ filename_dds, ios_base::binary };
		GPUResource* texture = new GPUResource();

		if (ifs2.good()) {
			wchar_t ddsFile[512] = {};
			int len = filename_dds.size();
			for (int i = 0; i < len; ++i) ddsFile[i] = filename_dds[i];
			ddsFile[len] = 0;
			texture->CreateTexture_fromFile(ddsFile, DXGI_FORMAT_BC3_UNORM, 3);
		}
		else {
			string filename = TextureDirPath;
			// .map (확장자)제거
			filename += textureName;
			filename += ".tex";

			ifstream ifs2{ filename, ios_base::binary };

			ifs2.read((char*)&width, sizeof(int));
			ifs2.read((char*)&height, sizeof(int));

			BYTE* pixels = new BYTE[width * height * 4];
			ifs2.read((char*)pixels, width * height * 4);
			ifs2.close();

			//char* isnormal = &filename[filename.size() - 10];
			//char* isnormalmap = &filename[filename.size() - 13];
			//char* isnormalDX = &filename[filename.size() - 12];
			//if ((strcmp(isnormal, "Normal.tex") == 0 ||
			//	strcmp(isnormalDX, "NormalDX.tex") == 0) ||
			//	strcmp(isnormalmap, "normalmap.tex") == 0) {

			//	float xtotal = 0;
			//	float ytotal = 0;
			//	float ztotal = 0;
			//	for (int ix = 0; ix < width; ++ix) {
			//		int h = rand() % height;
			//		BYTE* p = &pixels[h * width * 4 + ix * 4];
			//		xtotal += p[0];
			//		ytotal += p[1];
			//		ztotal += p[2];
			//	}
			//	if (ztotal < ytotal) {
			//		// x = z
			//		// y = -x
			//		// z = y
			//		for (int ix = 0; ix < width; ++ix) {
			//			for (int iy = 0; iy < height; ++iy) {
			//				BYTE* p = &pixels[iy * width * 4 + ix * 4];
			//				BYTE z = p[2];
			//				BYTE y = p[1];
			//				BYTE x = p[0];
			//				p[0] = z;
			//				p[1] = 255 - x;
			//				p[2] = y;
			//			}
			//		}
			//	}
			//	if (ztotal < xtotal) {
			//		// x = z
			//		// y = y
			//		// z = x
			//		for (int ix = 0; ix < width; ++ix) {
			//			for (int iy = 0; iy < height; ++iy) {
			//				BYTE* p = &pixels[iy * width * 4 + ix * 4];
			//				BYTE k = p[2];
			//				p[2] = p[0];
			//				p[0] = k;
			//			}
			//		}
			//	}
			//}
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
			texture->CreateTexture_fromFile(ddsFile, game.basicTexFormat, game.basicTexMip);

			DeleteFileA(filename_bmp.c_str());

			//texture->CreateTexture_fromImageBuffer(width, height, pixels, DXGI_FORMAT_BC2_UNORM);

			delete[] pixels;
		}

		ifs.read((char*)&width, sizeof(int));
		ifs.read((char*)&height, sizeof(int));
		ifs.read((char*)&format, sizeof(int));

		game.TextureTable.push_back(texture);
	}

	for (int i = 0; i < MaterialCount; ++i) {
		Material mat;

		int namelen = 0;
		ifs.read((char*)&namelen, sizeof(int));
		char name[512] = {};
		ifs.read((char*)name, namelen * sizeof(char));
		name[namelen] = 0;

		memcpy_s(mat.name, 40, name, 40);
		mat.name[39] = 0;

		ifs.read((char*)&mat.clr.diffuse, sizeof(float) * 4);

		ifs.read((char*)&mat.metallicFactor, sizeof(float));

		float smoothness = 0;
		ifs.read((char*)&smoothness, sizeof(float));
		mat.roughnessFactor = 1.0f - smoothness;

		ifs.read((char*)&mat.clr.bumpscaling, sizeof(float));

		vec4 tiling, offset = 0;
		vec4 tiling2, offset2 = 0;
		ifs.read((char*)&tiling, sizeof(float) * 2);
		ifs.read((char*)&offset, sizeof(float) * 2);
		ifs.read((char*)&tiling2, sizeof(float) * 2);
		ifs.read((char*)&offset2, sizeof(float) * 2);
		mat.TilingX = tiling.x;
		mat.TilingY = tiling.y;
		mat.TilingOffsetX = offset.x;
		mat.TilingOffsetY = offset.y;

		bool isTransparent = false;
		ifs.read((char*)&isTransparent, sizeof(bool));
		if (isTransparent) {
			mat.gltf_alphaMode = mat.Blend;
		}
		else mat.gltf_alphaMode = mat.Opaque;

		bool emissive = 0;
		ifs.read((char*)&emissive, sizeof(bool));

		ifs.read((char*)&mat.ti.BaseColor, sizeof(int));
		ifs.read((char*)&mat.ti.Normal, sizeof(int));
		ifs.read((char*)&mat.ti.Metalic, sizeof(int));
		ifs.read((char*)&mat.ti.AmbientOcculsion, sizeof(int));
		ifs.read((char*)&mat.ti.Roughness, sizeof(int));
		ifs.read((char*)&mat.ti.Emissive, sizeof(int));

		int diffuse2, normal2 = 0;
		ifs.read((char*)&diffuse2, sizeof(int));
		ifs.read((char*)&normal2, sizeof(int));

		mat.ShiftTextureIndexs(TextureTableStart);
		mat.SetDescTable();

		game.MaterialTable.push_back(mat);
	}

	for (int i = 0; i < ModelCount; ++i) {
		int namelen = 0;
		ifs.read((char*)&namelen, sizeof(int));
		char TempBuff[512] = {};
		ifs.read((char*)&TempBuff, namelen);
		TempBuff[namelen] = 0;

		string modelName = TempBuff;
		string filename = ModelDirPath;
		// .map (확장자)제거
		filename += modelName;
		filename += ".model";

		Model* pModel = new Model();
		pModel->LoadModelFile2(filename);
		map->models[i] = pModel;
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
				go->SetShape(map->meshes[meshid]);
				ifs.read((char*)&materialNum, sizeof(int));
				go->material = new int[materialNum];
				for (int k = 0; k < materialNum; ++k) {
					ifs.read((char*)&materialId, sizeof(int));
					if (materialId < 0) go->material[k] = 0;
					else {
						go->material[k] = MaterialTableStart + materialId;
					}
				}
			}

			int ColliderCount = 0;
			ifs.read((char*)&ColliderCount, sizeof(int));
			go->aabbArr.reserve(ColliderCount);
			go->aabbArr.resize(ColliderCount);
			for (int k = 0; k < ColliderCount; ++k) {
				ifs.read((char*)&go->aabbArr[k].Center, sizeof(XMFLOAT3));
				ifs.read((char*)&go->aabbArr[k].Extents, sizeof(XMFLOAT3));
			}
		}
		else if (Mod == 'm') {
			// is model
			//go->rmod = GameObject::eRenderMeshMod::_Model;
			//Model

			int ModelID;
			ifs.read((char*)&ModelID, sizeof(int));
			if (ModelID < 0) {
				go->SetShape((Model*)nullptr);
			}
			else {
				go->SetShape(map->models[ModelID]);
			}

			int nodeCount = go->shape.GetModel()->nodeCount;
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
			lptr->transform = go->worldMat;
			ifs.read((char*)&lptr->range, sizeof(float));
			ifs.read((char*)&lptr->intencity, sizeof(float));
			ifs.read((char*)&lptr->spot_angle, sizeof(float));
			lptr->GenerateLight();
			game.PushLight(lptr);
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

	//BakeStaticCollision();
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

void Light::GenerateLight()
{
}

BoundingBox ChunkIndex::GetAABB() {
	BoundingBox AABB;
	float halfW = game.chunck_divide_Width * 0.5f;
	AABB.Center = XMFLOAT3(game.chunck_divide_Width * x + halfW, game.chunck_divide_Width * y + halfW, game.chunck_divide_Width * z + halfW);
	AABB.Extents = XMFLOAT3(halfW, halfW, halfW);
	return AABB;
}

void Monster::Update(float deltaTime)
{
	PositionInterpolation(deltaTime);

	if (this->HPBarIndex < 0) return;

	matrix viewMatrix = XMLoadFloat4x4((XMFLOAT4X4*)&gd.viewportArr[0].ViewMatrix);
	matrix invViewMatrix = viewMatrix.RTInverse;
	matrix cameraWorldMat = invViewMatrix;

	matrix hpBarWorldMat;
	hpBarWorldMat.Id();
	hpBarWorldMat.LookAt(cameraWorldMat.right);
	hpBarWorldMat.pos = this->worldMat.pos + vec4(0.0f, 1.0f, 0.0f, 0);
	hpBarWorldMat.look *= 2 * HP / MaxHP;

	game.NpcHPBars[this->HPBarIndex] = hpBarWorldMat;
}

void Monster::Render(matrix parent)
{
	if (isDead) {
		return;
	}
	GameObject::Render();
}

void Monster::Init(const XMMATRIX& initialWorldMatrix)
{
	worldMat = (initialWorldMatrix);
	//m_homePos = worldMat.pos;
}

void Player::Update(float deltaTime)
{
	PositionInterpolation(deltaTime);
}

void Player::ClientUpdate(float deltaTime)
{
	if ((uintptr_t)m_pWeapon > 0x00007FFFFFFFFFFF) {
		m_pWeapon = nullptr;
	}

	if (m_pWeapon == nullptr || (int)m_pWeapon->m_info.type != m_currentWeaponType)
	{
		if (m_pWeapon != nullptr) {
			delete m_pWeapon;
			m_pWeapon = nullptr;
		}

		m_pWeapon = new Weapon((WeaponType)m_currentWeaponType);

	}

	if (m_pWeapon == nullptr) return;

	m_pWeapon->Update(deltaTime);

	if (game.player == this) {
		float recoilT = m_pWeapon->GetRecoilAlpha();
		if (recoilT > 0.7f) {
			float f = m_pWeapon->m_info.recoilVelocity * 10.0f * (recoilT - 0.7f) * deltaTime;
			game.DeltaMousePos.y -= f;
		}

		if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) m_isZooming = true;
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

	WeaponType currentType = m_pWeapon->m_info.type;

	switch (currentType)
	{
	case WeaponType::Pistol:
	{
		// --- 권총 로직 ---
		float recoilT = m_pWeapon->GetRecoilAlpha();

		float zOffset = 0.3f - (0.0f * powf(recoilT, 3.0f));
		float pitchAngle = 15.0f * powf(recoilT, 2.0f);
		gunMatrix_firstPersonView.mat = XMMatrixRotationX(-XMConvertToRadians(pitchAngle)) * XMMatrixTranslation(0.0f, 0.02f * recoilT, zOffset);

		float slideMove = -0.3f * powf(recoilT, 2.0f);

		if (game.PistolModel && !game.Pistol_SlideIndices.empty()) {
			XMMATRIX slideTrans = XMMatrixTranslation(0, slideMove, 0);

			for (int idx : game.Pistol_SlideIndices) {
				game.PistolModel->Nodes[idx].transform = slideTrans * game.PistolModel->BindPose[idx];
			}
		}
	}
	break;

	case WeaponType::MachineGun:
	{
		// --- 머신건 로직 ---
		float shootrate = powf(m_pWeapon->m_shootFlow / m_pWeapon->m_info.shootDelay, 3);
		if (shootrate > 1.0f) shootrate = 1.0f;

		float recoilAmount = 1.0f - shootrate;
		gunMatrix_firstPersonView.pos.z = 0.3f + 0.02f * recoilAmount;

		// 회전 로직
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
		// --- 스나이퍼 로직 ---
		float recoilT = m_pWeapon->GetRecoilAlpha();
		float zOffset = 0.3f;
		float pitchAngle = 0.0f;

		if (recoilT > 0.70f) {
			float stageT = (recoilT - 0.70f) / 0.30f;
			float smoothT = sinf(stageT * XM_PIDIV2);
			pitchAngle = m_pWeapon->m_info.recoilVelocity * smoothT;
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
		float currentFlow = m_pWeapon->m_shootFlow;
		float shootDelay = m_pWeapon->m_info.shootDelay;

		float recoilT = m_pWeapon->GetRecoilAlpha();
		float zOffset = 0.3f;
		float pitchAngle = 0.0f;

		if (recoilT > 0.0f) {
			zOffset -= 0.25f * powf(recoilT, 2.0f);
			pitchAngle = m_pWeapon->m_info.recoilVelocity * recoilT;
		}
		gunMatrix_firstPersonView.mat = XMMatrixRotationX(-XMConvertToRadians(pitchAngle)) * XMMatrixTranslation(0.0f, 0.0f, zOffset);

		float pumpZ = 0.0f;
		float pumpStart = 0.3f;
		float pumpEnd = 0.7f;

		if (currentFlow > pumpStart && currentFlow < pumpEnd) {
			float t = (currentFlow - pumpStart) / (pumpEnd - pumpStart);

			float curve = sinf(t * XM_PI);

			pumpZ = 120.0f * curve;
		}

		if (game.ShotGunModel && !game.SG_PumpIndices.empty()) {
			XMMATRIX pumpTrans = XMMatrixTranslation(pumpZ, 0, 0);
			for (int idx : game.SG_PumpIndices) {
				game.ShotGunModel->Nodes[idx].transform = pumpTrans * game.ShotGunModel->BindPose[idx];
			}
		}
	}
	break;

	case WeaponType::Rifle:
	{
		float recoilT = m_pWeapon->GetRecoilAlpha();
		float zOffset = 0.3f;
		float pitchAngle = 0.0f;
		float shakeX = 0.0f;

		if (recoilT > 0.0f) {
			zOffset -= 0.07f * powf(recoilT, 1.0f);

			pitchAngle = (m_pWeapon->m_info.recoilVelocity * 0.1f) * recoilT;

			shakeX = sinf(recoilT * XM_PI * 1.0f) * 0.005f;
		}

		XMMATRIX rotMat = XMMatrixRotationX(-XMConvertToRadians(pitchAngle));
		XMMATRIX transMat = XMMatrixTranslation(shakeX, 0.0f, zOffset);

		gunMatrix_firstPersonView.mat = rotMat * transMat;
	}
	break;

	}

	if (m_pWeapon->m_shootFlow < 0) {
		float reloadProgress = 1.0f - (abs(m_pWeapon->m_shootFlow) / m_pWeapon->m_info.reloadTime);
		float drop = sinf(reloadProgress * XM_PI) * 0.5f;

		XMMATRIX dropMat = XMMatrixTranslation(0, -drop, 0);
		gunMatrix_firstPersonView.mat = gunMatrix_firstPersonView.mat * dropMat;
	}

	if (m_pWeapon->m_shootFlow < deltaTime)
	{
		XMVECTOR lookQuat = XMQuaternionRotationRollPitchYaw(m_pitch, m_yaw, 0);
		vec4 clook = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), lookQuat);

		XMMATRIX viewInv = XMMatrixInverse(nullptr, gd.viewportArr[0].ViewMatrix);
		XMMATRIX worldGunMat = (XMMATRIX)gunMatrix_firstPersonView * viewInv;

		vec4 localMuzzle = m_pWeapon->m_info.muzzleOffset;
		localMuzzle.w = 1.0f;
		vec4 muzzleWorldPos = XMVector4Transform(localMuzzle, worldGunMat);

		MuzzleCB mData;
		mData.MuzzlePos = muzzleWorldPos;
		mData.MuzzleDir = clook;
		mData.MuzzleDir.w = 0.15f; 
		mData.MuzzleBurst = 1.0f; 

		mData.pad[0] = 0.0f; mData.pad[1] = 0.0f; mData.pad[2] = 0.0f;

		game.MuzzleFlashCS->DispatchMuzzle(
			gd.gpucmd,
			&game.MuzzlePool.Buffer,
			game.MuzzlePool.Count,
			mData,
			deltaTime 
		);
	}
}

void Player::Render()
{
	if (game.player == this) {
		if (game.bFirstPersonVision == false) {
			GameObject::Render();

			matrix gunmat = gunMatrix_thirdPersonView;

			const float PI = 3.141592f;
			gunmat *= XMMatrixRotationY(PI);

			gunmat.pos.y -= 0.40f;
			gunmat.pos.x += 0.55f;
			gunmat.pos.z += 0.80f;

			gunmat *= worldMat;
			//gunmat.LookAt(worldMat.look);
			//gunmat.transpose();

			//gd.gpucmd->SetGraphicsRoot32BitConstants(1, 16, &gunmat, 0);
			//Gun->Render(gd.gpucmd, 1);
			if (Game::renderViewPort == &game.MySpotLight.viewport) {
				//shadowMapping
				if (GunModel) {
					
					GunModel->Render(gd.gpucmd, gunmat, nullptr);
				}
			}
			else {
				if (GunModel) {
					GunModel->Render(gd.gpucmd, gunmat, nullptr);
				}
			}

		}
	}
	else {
		GameObject::Render();
	}
}

void Player::Render_AfterDepthClear()
{
	matrix viewmat = gd.viewportArr[0].ViewMatrix.RTInverse;

	struct Model* pTargetModel = nullptr;

	if (game.bFirstPersonVision)
	{
		switch ((WeaponType)m_currentWeaponType)
		{
		case WeaponType::Sniper:     pTargetModel = game.SniperModel; break;
		case WeaponType::MachineGun: pTargetModel = game.MachineGunModel; break;
		case WeaponType::Shotgun:     pTargetModel = game.ShotGunModel; break;
		case WeaponType::Rifle:     pTargetModel = game.RifleModel; break;
		case WeaponType::Pistol:     pTargetModel = game.PistolModel; break;
		}

		if (pTargetModel) {
			matrix gunmat = gunMatrix_firstPersonView;

			switch ((WeaponType)m_currentWeaponType)
			{
			case WeaponType::MachineGun:
				gunmat *= XMMatrixScaling(0.5f, 0.5f, 0.5f);
				gunmat.pos.y -= 0.40f;
				gunmat.pos.x += 0.35f;
				gunmat.pos.z += 0.90f;
				break;
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
				gunmat *= XMMatrixScaling(2.0f, 2.0f, 2.0f);
				gunmat.pos.y -= 1.20f;
				gunmat.pos.x += 1.00f;
				gunmat.pos.z += 1.40f;
				break;
			case WeaponType::Rifle:
			{
				matrix modelFix = XMMatrixScaling(1.2f, 1.2f, 1.2f);
				modelFix *= XMMatrixRotationY(XMConvertToRadians(90.0f));
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
				matrix modelFix = XMMatrixScaling(0.15f, 0.15f, 0.15f);
				modelFix *= XMMatrixRotationY(XMConvertToRadians(-90.0f));
				gunmat = modelFix * (XMMATRIX)gunMatrix_firstPersonView;
				gunmat.pos.y -= 0.70f;
				gunmat.pos.x += 0.75f;
				gunmat.pos.z += 1.70f;
				break;
			}
			}


			if (pTargetModel) {
				gunmat *= viewmat;

				if (Game::renderViewPort == &game.MySpotLight.viewport) {
					pTargetModel->Render(gd.gpucmd, gunmat);
					return;
				}
				else {
					pTargetModel->Render(gd.gpucmd, gunmat);
				}
			}
		}
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

	//HP = (ShootFlow / ShootDelay) * MaxHP;
	matrix hpmat;
	hpmat.pos.x = -6;
	hpmat.pos.y = 2;
	hpmat.pos.z = 6;
	hpmat.LookAt(vec4(1, 0, 0));
	hpmat.look *= 2 * HP / MaxHP;
	hpmat *= viewmat;
	hpmat.transpose();
	gd.gpucmd->SetGraphicsRoot32BitConstants(1, 16, &hpmat, 0);
	HPBarMesh->Render(gd.gpucmd, 1);

	//Heat Bar
	matrix heatmat;
	heatmat.pos.x = -5;
	heatmat.pos.y = 1.5f;
	heatmat.pos.z = 5;
	heatmat.LookAt(vec4(1, 0, 0));
	heatmat.look *= 2 * HeatGauge / 200;
	heatmat *= viewmat;
	heatmat.transpose();
	gd.gpucmd->SetGraphicsRoot32BitConstants(1, 16, &heatmat, 0);
	HeatBarMesh->Render(gd.gpucmd, 1);
}

void Player::UpdateGunBarrelNodes()
{
	if ((WeaponType)m_currentWeaponType != WeaponType::MachineGun) return;
	if (!game.MachineGunModel || game.MG_BarrelIndices.empty()) return;

	matrix rot = XMMatrixRotationZ(gunBarrelAngle);

	for (int idx : game.MG_BarrelIndices) {
		game.MachineGunModel->Nodes[idx].transform = rot * game.MachineGunModel->BindPose[idx];
	}
}

SkinMeshGameObject::SkinMeshGameObject()
{
}

SkinMeshGameObject::~SkinMeshGameObject()
{
}

void SkinMeshGameObject::InitRootBoneMatrixs()
{
	Model* pModel = shape.GetModel();
	for (int i = 0; i < pModel->mNumSkinMesh; ++i) {
		int boneNum = pModel->mBumpSkinMeshs[i]->MatrixCount;
		UINT ncbElementBytes = (((sizeof(matrix) * boneNum) + 255) & ~255); //256의 배수
		GPUResource res = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, /*D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER*/D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1);
		BoneToWorldMatrixCB.push_back(res);
		matrix* mapped = nullptr;
		D3D12_RANGE range;
		range.Begin = 0;
		range.End = boneNum * sizeof(matrix);
		BoneToWorldMatrixCB[i].resource->Map(0, &range, (void**)&mapped);
		RootBoneMatrixs_PerSkinMesh.push_back(mapped);
	}

	SetRootMatrixs();

	for (int i = 0; i < pModel->mNumSkinMesh; ++i)
	{
		int boneNum = pModel->mBumpSkinMeshs[i]->MatrixCount;
		UINT ncbElementBytes = (((sizeof(matrix) * boneNum) + 255) & ~255);
		int n = gd.TextureDescriptorAllotter.Alloc();
		D3D12_CPU_DESCRIPTOR_HANDLE hCPU = gd.TextureDescriptorAllotter.GetCPUHandle(n);
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = BoneToWorldMatrixCB[i].resource->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = ncbElementBytes;
		gd.pDevice->CreateConstantBufferView(&cbvDesc, hCPU);
		BoneToWorldMatrixCB[i].descindex.Set(false, n);
	}
}

void SkinMeshGameObject::SetRootMatrixs()
{
	Model* pModel = shape.GetModel();
	for (int i = 0; i < pModel->mNumSkinMesh; ++i) {
		matrix* marr = RootBoneMatrixs_PerSkinMesh[i];
		BumpSkinMesh* bsm = pModel->mBumpSkinMeshs[i];
		for (int k = 0; k < bsm->MatrixCount; ++k) {
			int nodeindex = bsm->toNodeIndex[k];
			ModelNode* node = &pModel->Nodes[nodeindex];
			int ni = ((char*)node - (char*)pModel->Nodes) / sizeof(ModelNode);

			RootBoneMatrixs_PerSkinMesh[i][k].Id();
			while (node != nullptr) {
				RootBoneMatrixs_PerSkinMesh[i][k] *= transforms_innerModel[ni];

				node = node->parent;
				ni = ((char*)node - (char*)pModel->Nodes) / sizeof(ModelNode);
			}
			RootBoneMatrixs_PerSkinMesh[i][k].transpose();
		}
	}
}

void SkinMeshGameObject::PushHumanoidAnimation(HumanoidAnimation* hanim) {
	HumanoidAnimationArr.push_back(hanim);
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

void SkinMeshGameObject::SetShape(Shape _shape)
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
		}

		InitRootBoneMatrixs();

		if (gd.isSupportRaytracing) {
			modifyMeshes = new RayTracingMesh[model->mNumSkinMesh];
			OutVertexUAV = new DescIndex[model->mNumSkinMesh];
			for (int i = 0; i < model->mNumSkinMesh; ++i) {
				// 수정할 버택스만 만들어 rmesh에 저장. 출력시 LRS에서 버택스는 여기에서, 인덱스는 원본메쉬에서 가져온다.
				modifyMeshes[i].AllocateRaytracingUAVMesh(model->mBumpSkinMeshs[i]->vertexData, model->mBumpSkinMeshs[i]->rmesh.IBStartOffset, model->mBumpSkinMeshs[i]->subMeshNum, model->mBumpSkinMeshs[i]->SubMeshIndexStart);
				modifyMeshes[i].IBStartOffset = new UINT64[model->mBumpSkinMeshs[i]->subMeshNum + 1];
				modifyMeshes[i].subMeshCount = model->mBumpSkinMeshs[i]->subMeshNum;
				for (int k = 0; k < model->mBumpSkinMeshs[i]->subMeshNum + 1; ++k) {
					modifyMeshes[i].IBStartOffset[k] = model->mBumpSkinMeshs[i]->rmesh.IBStartOffset[k];
				}

				//Compute Geometry 변형을 위한 UAV 생성
				int index = gd.TextureDescriptorAllotter.Alloc();
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

			RaytracingWorldMatInput_Model = new float** [model->nodeCount];
			for (int i = 0; i < model->nodeCount; ++i) {
				RaytracingWorldMatInput_Model[i] = nullptr;
				if (model->Nodes[i].Mesh_SkinMeshindex != nullptr) {
					for (int k = 0; k < model->Nodes[i].numMesh; ++k) {
						if (model->Nodes[i].Mesh_SkinMeshindex != nullptr && model->Nodes[i].Mesh_SkinMeshindex[k] >= 0) {
							int skinmeshIndex = model->Nodes[i].Mesh_SkinMeshindex[k];
							BumpSkinMesh* bsmesh = model->mBumpSkinMeshs[skinmeshIndex];
							for (int k = 0; k < bsmesh->subMeshNum; ++k) {
								tempLRSSaver[k] = LocalRootSigData(modifyMeshes[skinmeshIndex].UAV_VBStartOffset / sizeof(RayTracingMesh::Vertex), bsmesh->rmesh.IBStartOffset[k] / sizeof(unsigned int));
							}

							float** WorldMatInputs = game.MyRayTracingShader->push_rins_immortal(&modifyMeshes[skinmeshIndex], worldMat, tempLRSSaver, 1);
							RaytracingWorldMatInput_Model[i] = new float* [bsmesh->subMeshNum];
							for (int k = 0; k < bsmesh->subMeshNum; ++k) {
								RaytracingWorldMatInput_Model[i][k] = WorldMatInputs[k];
							}
						}
						else {
							// BumpMesh 처리.
							BumpMesh* bmesh = (BumpMesh*)model->mMeshes[model->Nodes[i].Meshes[0]];
							for (int k = 0; k < bmesh->subMeshNum; ++k) {
								tempLRSSaver[k] = LocalRootSigData(bmesh->rmesh.VBStartOffset / sizeof(RayTracingMesh::Vertex), bmesh->rmesh.IBStartOffset[k] / sizeof(unsigned int));
							}

							float** WorldMatInputs = game.MyRayTracingShader->push_rins_immortal(&bmesh->rmesh, worldMat, tempLRSSaver);
							RaytracingWorldMatInput_Model[i] = new float* [bmesh->subMeshNum];
							for (int k = 0; k < bmesh->subMeshNum; ++k) {
								RaytracingWorldMatInput_Model[i][k] = WorldMatInputs[k];
							}
						}
					}
				}
				else {
					if (model->Nodes[i].numMesh > 0) {
						BumpMesh* bmesh = (BumpMesh*)model->mMeshes[model->Nodes[i].Meshes[0]];
						for (int k = 0; k < bmesh->subMeshNum; ++k) {
							tempLRSSaver[k] = LocalRootSigData(bmesh->rmesh.VBStartOffset / sizeof(RayTracingMesh::Vertex), bmesh->rmesh.IBStartOffset[k] / sizeof(unsigned int));
						}

						float** WorldMatInputs = game.MyRayTracingShader->push_rins_immortal(&bmesh->rmesh, worldMat, tempLRSSaver);
						RaytracingWorldMatInput_Model[i] = new float* [bmesh->subMeshNum];
						for (int k = 0; k < bmesh->subMeshNum; ++k) {
							RaytracingWorldMatInput_Model[i][k] = WorldMatInputs[k];
						}
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
	constexpr float frameSpeed = 1;
	Model* pModel = shape.GetModel();
	AnimationFlowTime += delatTime * frameSpeed;
	for (int i = 0; i < pModel->nodeCount; ++i) {
		transforms_innerModel[i].Id();
	}
	GetBoneLocalMatrixAtTime(HumanoidAnimationArr[0], transforms_innerModel, AnimationFlowTime);
	for (int i = 0; i < pModel->nodeCount; ++i) {
		transforms_innerModel[i] *= pModel->DefaultNodelocalTr[i];
	}//?
	transforms_innerModel[0] = worldMat;
	SetRootMatrixs();
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
		model->Render<true>(gd.gpucmd, world, this);
	}
}

void SkinMeshGameObject::PushRenderBatch(matrix parent)
{
}

void SkinMeshGameObject::ModifyVertexs(matrix parent)
{
	// ComputeShader가 이미 Set 되었다는 가정하에 함수가 실행된다.
	Model* model = shape.GetModel();
	for (int i = 0; i < model->mNumSkinMesh; ++i) {
		BumpSkinMesh* bsmesh = model->mBumpSkinMeshs[i];
		unsigned int VertexSiz = bsmesh->vertexData.size();
		using SMMSRPI = SkinMeshModifyShader::RootParamId;
		gd.CScmd->SetComputeRoot32BitConstants(SMMSRPI::Const_OutputVertexBufferSize, 1, &VertexSiz, 0);

		DescHandle hCBV;
		gd.ShaderVisibleDescPool.DynamicAlloc(&hCBV, 2);
		gd.pDevice->CopyDescriptorsSimple(1, hCBV[0].hcpu, bsmesh->ToOffsetMatrixsCB.descindex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		gd.pDevice->CopyDescriptorsSimple(1, hCBV[1].hcpu, BoneToWorldMatrixCB[i].descindex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
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
	// BLAS를 수행할 오브젝트를 수집한다.
	game.MyRayTracingShader->RebuildBLASBuffer.push_back(this);
}

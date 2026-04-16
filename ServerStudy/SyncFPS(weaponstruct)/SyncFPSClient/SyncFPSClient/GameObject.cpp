#include "stdafx.h"
#include "Render.h"
#include "Game.h"
#include "GameObject.h"

extern int dbgc[128];

Mesh* BulletRay::mesh = nullptr;
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

	/*obb_local.Transform(obb, world);
	b = (game.renderViewPort->*game.renderViewPort->FrustumIntersectFunc)(obb);
	if (b == false) return;*/

	if (mesh != nullptr) {
		matrix rootWorld = world;
		rootWorld.transpose();
		BumpMesh* Bmesh = (BumpMesh*)mesh;
		gd.gpucmd->SetGraphicsRoot32BitConstants(1, 16, &rootWorld, 0);
		for (int i = 0; i < Bmesh->subMeshNum; ++i) {
			Material* Mat = game.MaterialTable[material[i]];
			using PBRRPI = PBRShader1::RootParamId;
			gd.gpucmd->SetGraphicsRootDescriptorTable(PBRRPI::CBVTable_Material, Mat->CB_Resource.descindex.hRender.hgpu);
			gd.gpucmd->SetGraphicsRootDescriptorTable(PBRRPI::SRVTable_MaterialTextures, Mat->TextureSRVTableIndex.hRender.hgpu);
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

void GameObject::SetShape(Shape _shape)
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
					int matindex = game.GetRenderMaterialIndexFromGlobalMaterialIndex(model->Nodes[i].materialIndex[0]);
					tempLRSSaver[0] = LocalRootSigData(bmesh->rmesh.VBStartOffset / sizeof(RayTracingMesh::Vertex), bmesh->rmesh.IBStartOffset[0] / sizeof(unsigned int), matindex);

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

			int matindex = game.GetRenderMaterialIndexFromGlobalMaterialIndex(material[0]);
			tempLRSSaver[0] = LocalRootSigData(bmesh->rmesh.VBStartOffset / sizeof(RayTracingMesh::Vertex), bmesh->rmesh.IBStartOffset[0] / sizeof(unsigned int), matindex);

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
	shape = Shape::ShapeTable[stcsod.shapeindex];
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

void StaticGameObject::SetShape(Shape _shape) {
	GameObject::SetShape(_shape);
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
	shape = Shape::ShapeTable[stcsod.shapeindex];
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

void DynamicGameObject::SetShape(Shape _shape)
{
	GameObject::SetShape(_shape);
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
	BoundingOrientedBox beforeobb = GetOBB();
	GameObjectIncludeChunks goic_before = game.GetChunks_Include_OBB(beforeobb);
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
	GameObjectIncludeChunks goic_after = game.GetChunks_Include_OBB(afterobb);
	if (goic_before != goic_after) {
		worldMat = temp;
		MoveChunck(mat, goic_before, goic_after);
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

		// 안 곂치는 부분은 Free 한다.
		auto f = game.chunck.find(ci);
		if (f != game.chunck.end()) {
#ifdef ChunckDEBUG
			dbgbreak(f->second->Dynamic_gameobjects.isnull(chunkAllocIndexs[ci.extra]));
#endif
			f->second->Dynamic_gameobjects.Free(chunkAllocIndexs[ci.extra]);
		}
	}

	// 위치 이동 / 회전
	worldMat = afterMat;

	inter_ci = ChunkIndex(intersection.xmin, intersection.ymin, intersection.zmin);
	inter_ci.extra = 0;

	chunckCount = afterChunkInc.GetChunckSize();
	ci = ChunkIndex(afterChunkInc.xmin, afterChunkInc.ymin, afterChunkInc.zmin);
	ci.extra = 0;

	inter_up = 0;
	for (; ci.extra < chunckCount; afterChunkInc.Inc(ci)) {
		if (ci == inter_ci) { // 곂치는 부분을 Alloc 하지 않는다.
			intersection.Inc(inter_ci);
			chunkAllocIndexs[ci.extra] = temp[inter_up];
			inter_up += 1;
			continue;
		}

		auto c = game.chunck.find(ci);
		GameChunk* gc;
		if (c == game.chunck.end()) {
			// new game chunk
			gc = new GameChunk();
			gc->SetChunkIndex(ci);
			game.chunck.insert(pair<ChunkIndex, GameChunk*>(ci, gc));
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
	shape = Shape::ShapeTable[stcsod.shapeindex];
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
		// 사실 이 코드는 실행이 되면 안되지 않나? 흠..
		// >> 하지만 서버에서 자신의 플레이어 오브젝트 데이터를 통짜로 두번 보내기 때문에..
		// >> 결국 두번 되는건 어쩔 수 없는 것으로 보인다. 잘 릴리즈를 해보자.
		// >> 게임 상황에서 shape가 바뀔 수도 있잖음? ㅇㅇ..

		for (int i = 0;i < BoneToWorldMatrixCB_Default.size();++i) {
			BoneToWorldMatrixCB_Default[i].Release();
			BoneToWorldMatrixCB[i].resource->Unmap(0, nullptr);
			BoneToWorldMatrixCB[i].Release();
			RootBoneMatrixs_PerSkinMesh[i] = nullptr;
			if constexpr (gd.PlayAnimationByGPU) {
				gd.TextureDescriptorAllotter.Free(BoneToWorldMatrix_UAVDescIndex[i].index);
				NodeToBone[i].Release();
				gd.TextureDescriptorAllotter.Free(NodeToBone_SRVDescIndex[i].index);
			}
		}
		BoneToWorldMatrixCB_Default.clear();
		BoneToWorldMatrixCB.clear();
		RootBoneMatrixs_PerSkinMesh.clear();
		if constexpr (gd.PlayAnimationByGPU) {
			BoneToWorldMatrix_UAVDescIndex.clear();
			NodeTposMatrixs.Release();
			gd.TextureDescriptorAllotter.Free(NodeTposMatrixs_SRVDescIndex.index);
			Node_ToParentRes.Release();
			gd.TextureDescriptorAllotter.Free(NodeToParentSRVIndex.index);
			HumanoidToNodeIndexRes.Release();
			gd.TextureDescriptorAllotter.Free(HumanoidToNodeIndexSRVIndex.index);
			AnimationBlendConstantUploadBuffer.resource->Unmap(0, nullptr);
			AnimationBlendConstantUploadBuffer.Release();
			AnimBlendingCB_Mapped = nullptr;
			gd.TextureDescriptorAllotter.Free(AnimBlendingCBVDescIndex.index);
			NodeToBone_SRVDescIndex.clear();
			NodeToBone.clear();
		}
		// 리소스 개많아
	}

	Model* pModel = shape.GetModel();
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
		int n = gd.TextureDescriptorAllotter.Alloc();
		D3D12_CPU_DESCRIPTOR_HANDLE hCPU = gd.TextureDescriptorAllotter.GetCPUHandle(n);
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
			index = gd.TextureDescriptorAllotter.Alloc();
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
			index = gd.TextureDescriptorAllotter.Alloc();
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
			index = gd.TextureDescriptorAllotter.Alloc();
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
			index = gd.TextureDescriptorAllotter.Alloc();
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
			index = gd.TextureDescriptorAllotter.Alloc();
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
			index = gd.TextureDescriptorAllotter.Alloc();
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
			index = gd.TextureDescriptorAllotter.Alloc();
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
			index = gd.TextureDescriptorAllotter.Alloc();
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
	// 알고리즘 때문에 병목이 생겼었음. 그래서 고쳤다.
	// 다만 20명의 스킨메쉬 애니메이션 각 출력하는데 
	// 한 프레임의 0.029124 을 잡아먹는건 좀 심하다. 그럼 50명만 있어도 프레임은 개 낮아질거 아닌가?
	// 때문에 GPU 최적화 도 충분히 고려해야 할 것 같다.

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
			transforms_innerModel[i].transpose();
		}

		InitRootBoneMatrixs();

		if (gd.isSupportRaytracing) {
			modifyMeshes = new RayTracingMesh[model->mNumSkinMesh];
			OutVertexUAV = new DescIndex[model->mNumSkinMesh];
			for (int i = 0; i < model->mNumSkinMesh; ++i) {
				// 수정할 버택스만 만들어 rmesh에 저장. 출력시 LRS에서 버택스는 여기에서, 인덱스는 원본메쉬에서 가져온다.
				modifyMeshes[i].AllocateRaytracingUAVMesh(model->mBumpSkinMeshs[i]->vertexData, model->mBumpSkinMeshs[i]->rmesh.IBStartOffset, model->mBumpSkinMeshs[i]->subMeshNum, model->mBumpSkinMeshs[i]->SubMeshIndexStart);
				modifyMeshes[i].IBStartOffset = new UINT64[/*model->mBumpSkinMeshs[i]->subMeshNum */1+ 1];
				modifyMeshes[i].subMeshCount = model->mBumpSkinMeshs[i]->subMeshNum;
				for (int k = 0; k < /*model->mBumpSkinMeshs[i]->subMeshNum*/1 + 1; ++k) {
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
							int matindex = game.GetRenderMaterialIndexFromGlobalMaterialIndex(model->Nodes[i].materialIndex[0]);
							tempLRSSaver[0] = LocalRootSigData(modifyMeshes[skinmeshIndex].UAV_VBStartOffset / sizeof(RayTracingMesh::Vertex), bsmesh->rmesh.IBStartOffset[0] / sizeof(unsigned int), matindex);
							float** WorldMatInputs = game.MyRayTracingShader->push_rins_immortal(&modifyMeshes[skinmeshIndex], worldMat, tempLRSSaver, 1);

							//RaytracingWorldMatInput_Model[i] = new float* [bsmesh->subMeshNum];
							/*for (int k = 0; k < bsmesh->subMeshNum; ++k) {
								RaytracingWorldMatInput_Model[i][k] = WorldMatInputs[k];
							}*/
							RaytracingWorldMatInput_Model[i] = WorldMatInputs[0];
						}
						else {
							// BumpMesh 처리.
							BumpMesh* bmesh = (BumpMesh*)model->mMeshes[model->Nodes[i].Meshes[0]];
							/*for (int k = 0; k < bmesh->subMeshNum; ++k) {
								tempLRSSaver[k] = LocalRootSigData(bmesh->rmesh.VBStartOffset / sizeof(RayTracingMesh::Vertex), bmesh->rmesh.IBStartOffset[k] / sizeof(unsigned int));
							}*/
							int matindex = game.GetRenderMaterialIndexFromGlobalMaterialIndex(model->Nodes[i].materialIndex[0]);
							tempLRSSaver[0] = LocalRootSigData(bmesh->rmesh.VBStartOffset / sizeof(RayTracingMesh::Vertex), bmesh->rmesh.IBStartOffset[0] / sizeof(unsigned int), matindex);

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
						int matindex = game.GetRenderMaterialIndexFromGlobalMaterialIndex(model->Nodes[i].materialIndex[0]);
						tempLRSSaver[0] = LocalRootSigData(bmesh->rmesh.VBStartOffset / sizeof(RayTracingMesh::Vertex), bmesh->rmesh.IBStartOffset[0] / sizeof(unsigned int), matindex);

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
	
	if (BoneToWorldMatrixCB.size() == 0) {
		return;
	}

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
	// BLAS를 수행할 오브젝트를 수집한다.
	game.MyRayTracingShader->RebuildBLASBuffer.push_back(this);
}

void SkinMeshGameObject::BlendingAnimation() {
	AnimationBlendingCBStruct* cbData = (AnimationBlendingCBStruct*)AnimBlendingCB_Mapped;

	if (cbData != nullptr) {
		int anim0 = PlayingAnimationIndex[0];
		cbData->animTime[0] = AnimationFlowTime[0];
		cbData->MAXTime[0] = game.HumanoidAnimationTable[anim0].Duration;
		cbData->animWeight[0] = 1.0f;

		int anim1 = PlayingAnimationIndex[1];
		if (anim1 != -1) {
			cbData->animTime[1] = AnimationFlowTime[1];
			cbData->MAXTime[1] = game.HumanoidAnimationTable[anim1].Duration;
			cbData->animWeight[1] = (PlayingAnimationIndex[1] != -1) ? 1.0f : 0.0f;
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
	matrix rootworld = worldMat;
	rootworld.transpose();
	for (int i = 0;i < model->mNumSkinMesh;++i) {
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
	shape = Shape::ShapeTable[stcsod.shapeindex];
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
		// 수행시간 평균 0.0000031초
		constexpr float frameSpeed = 1;
		Model* pModel = shape.GetModel();
		if (AnimationFlowTime[0] > game.HumanoidAnimationTable[PlayingAnimationIndex[0]].Duration) AnimationFlowTime[0] = 0;
		AnimationFlowTime[0] += deltaTime * frameSpeed;

		for (int i = 0; i < pModel->nodeCount; ++i) {
			transforms_innerModel[i].Id();
		}

		gd.AverageSecPer60Start(Update_Monster_Animation_GetBoneLocalMatrixAtTime);
		GetBoneLocalMatrixAtTime(&game.HumanoidAnimationTable[PlayingAnimationIndex[0]], transforms_innerModel, AnimationFlowTime[0]);
		gd.AverageSecPer60End(Update_Monster_Animation_GetBoneLocalMatrixAtTime);

		for (int i = 0; i < pModel->nodeCount; ++i) {
			transforms_innerModel[i] *= pModel->DefaultNodelocalTr[i];
		}
		transforms_innerModel[0] = worldMat;

		gd.AverageSecPer60Start(Update_Monster_Animation_SetRootMatrixs);
		SetRootMatrixs();
		gd.AverageSecPer60End(Update_Monster_Animation_SetRootMatrixs);
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

		// 안 곂치는 부분은 Free 한다.
		auto f = game.chunck.find(ci);
		if (f != game.chunck.end()) {
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

	// 위치 이동 / 회전
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
		if (ci == inter_ci && inter_Count > 0) { // 곂치는 부분을 Alloc 하지 않는다.
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

		auto c = game.chunck.find(ci);
		GameChunk* gc;
		if (c == game.chunck.end()) {
			// new game chunk
			gc = new GameChunk();
			gc->SetChunkIndex(ci);
			game.chunck.insert(pair<ChunkIndex, GameChunk*>(ci, gc));
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
			gd.TextureDescriptorAllotter.Free(di.index);
		}
	}
	BoneToWorldMatrix_UAVDescIndex.clear();

	for (int i = 0; i < NodeToBone_SRVDescIndex.size(); ++i) {
		DescIndex& di = NodeToBone_SRVDescIndex[i];
		if (di.type == 'n' && di.isShaderVisible == false) {
			gd.TextureDescriptorAllotter.Free(di.index);
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
				gd.TextureDescriptorAllotter.Free(di.index);
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

	DynamicGameObject::Release();
}

Monster::Monster() {
	HP = 30;
	MaxHP = 30;
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

	gd.AverageSecPer60Start(Update_Monster_Animation);
	AnimationUpdate(deltaTime);
	//if (AnimationFlowTime < 1) {
	//	AnimationUpdate(deltaTime);
	//}
	gd.AverageSecPer60End(Update_Monster_Animation);

}

void Monster::Render(matrix parent)
{
	static int rc4 = 0;
	if (rc4 < 3) {
		char dbg[256];
		sprintf_s(dbg, "[Monster] pos=(%f,%f,%f) destPos=(%f,%f,%f)\n",
			worldMat.pos.x, worldMat.pos.y, worldMat.pos.z,
			DestPos.x, DestPos.y, DestPos.z);
		OutputDebugStringA(dbg);
		rc4++;
	}
	if (isDead) {
		return;
	}
	if (BoneToWorldMatrixCB.size() == 0) {
		return;
	}
	SkinMeshGameObject::Render();
}

void Monster::Init(const XMMATRIX& initialWorldMatrix)
{
	worldMat = (initialWorldMatrix);
	//m_homePos = worldMat.pos;
}

void Monster::RecvSTC_SyncObj(char* data) {
	int offset = 0;
	STC_SyncObjData& stcsod = *(STC_SyncObjData*)(data);
	shape = Shape::ShapeTable[stcsod.shapeindex];
	//dbglog1(L"parent : %d \n", stcsod.parent);
	parent = (stcsod.parent >= 0) ? game.DynmaicGameObjects[stcsod.parent] : nullptr;
	childs = (stcsod.childs >= 0) ? game.DynmaicGameObjects[stcsod.childs] : nullptr;
	sibling = (stcsod.sibling >= 0) ? game.DynmaicGameObjects[stcsod.sibling] : nullptr;
	XMMatrixDecompose((XMVECTOR*)&DestScale, (XMVECTOR*)&DestRot, (XMVECTOR*)&DestPos, stcsod.DestWorld);
	LVelocity = stcsod.LVelocity;
	AnimationFlowTime[0] = stcsod.AnimationFlowTime;
	PlayingAnimationIndex[0] = stcsod.PlayingAnimationIndex;
	HP = stcsod.HP;
	MaxHP = stcsod.MaxHP;
	isDead = stcsod.isDead;
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
		// 나중에 공격, 피격, 사망 추가 예정
	case State::DEATH:
		PlayingAnimationIndex[0] = 6;
		break;
	}
}

void Monster::Release() {
	SkinMeshGameObject::Release();
}

Player::Player() : HP{ 100 } {
	weapon = Weapon(WeaponType::Sniper);
	HP = 100;
	MaxHP = 100;
	bullets = 100;
	KillCount = 0;
	DeathCount = 0;
	HeatGauge = 0;
	MaxHeatGauge = 200;
	HealSkillCooldown = 10.0f;
	HealSkillCooldownFlow = 0;
	m_currentWeaponType = (int)WeaponType::Sniper;
	ZeroMemory(Inventory, sizeof(ItemStack) * maxItem);

	DeltaMousePos = 0;
	GunModel = nullptr;
	ShootPointMesh = nullptr;
	gunMatrix_thirdPersonView.Id();
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
}

static Model* GetWeaponRenderModel(WeaponType weaponType)
{
	switch (weaponType)
	{
	case WeaponType::Sniper: return game.SniperModel;
	case WeaponType::MachineGun: return game.MachineGunModel;
	case WeaponType::Shotgun: return game.ShotGunModel;
	case WeaponType::Rifle: return game.RifleModel;
	case WeaponType::Pistol: return game.PistolModel;
	default: return nullptr;
	}
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
	PositionInterpolation(deltaTime);
	//PlayingAnimationIndex[0] = 0;

	float velSpeed = sqrt(LVelocity.x * LVelocity.x + LVelocity.z * LVelocity.z);
	float dx = DestPos.x - worldMat.pos.x;
	float dz = DestPos.z - worldMat.pos.z;
	float distSpeed = sqrt(dx * dx + dz * dz);
	float currentSpeed = max(velSpeed, distSpeed);

	switch (m_currentState)
	{
	case State::IDLE:
		if (currentSpeed > 3.0f) ChangeState(State::RUN);
		else if (currentSpeed > 0.05f) ChangeState(State::WALK);
		break;

	case State::WALK:
		if (currentSpeed <= 0.05f) ChangeState(State::IDLE);
		else if (currentSpeed > 3.0f) ChangeState(State::RUN);
		break;

	case State::RUN:
		if (currentSpeed <= 0.05f) ChangeState(State::IDLE);
		else if (currentSpeed <= 3.0f) ChangeState(State::WALK);
		break;
	}

	AnimationFlowTime[0] += deltaTime;

	double currentDuration = game.HumanoidAnimationTable[PlayingAnimationIndex[0]].Duration;

	if (AnimationFlowTime[0] > currentDuration) {
		AnimationFlowTime[0] = fmod(AnimationFlowTime[0], currentDuration);
	}

	if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
		ChangeUpperState(UpperState::SHOOT);
	}
	else if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) {
		double upperDuration = (PlayingAnimationIndex[1] != -1) ? game.HumanoidAnimationTable[PlayingAnimationIndex[1]].Duration : 0.0;
		if (m_currentUpperState != UpperState::SHOOT ||
			(PlayingAnimationIndex[1] != -1 && AnimationFlowTime[1] >= upperDuration - 0.002))
		{
			ChangeUpperState(UpperState::AIM);
		}
	}
	else {
		if (m_currentUpperState == UpperState::SHOOT) {
			double upperDuration = game.HumanoidAnimationTable[PlayingAnimationIndex[1]].Duration;
			if (PlayingAnimationIndex[1] != -1 && AnimationFlowTime[1] >= upperDuration - 0.002) {
				ChangeUpperState(UpperState::NONE);
			}
		}
		else {
			ChangeUpperState(UpperState::NONE);
		}
	}

	if (m_currentUpperState != UpperState::NONE && PlayingAnimationIndex[1] != -1)
	{
		AnimationFlowTime[1] += deltaTime;
		double upperDuration = game.HumanoidAnimationTable[PlayingAnimationIndex[1]].Duration;

		if (AnimationFlowTime[1] > upperDuration) {
			if (m_currentUpperState == UpperState::AIM) {
				AnimationFlowTime[1] = fmod(AnimationFlowTime[1], upperDuration);
			}
			else if (m_currentUpperState == UpperState::SHOOT) {
				AnimationFlowTime[1] = upperDuration - 0.001;
			}
		}
	}

	if (m_currentUpperState != UpperState::NONE)
	{
		m_animMask[0] = 0x7FULL;  // 0~6번 뼈 (하체)
		m_animMask[1] = ~0x7FULL; // 7번 뼈 이상 (상체)
	}
	else
	{
		m_animMask[0] = 0xFFFFFFFFFFFFFFFFULL;
		m_animMask[1] = 0ULL;
	}

	AnimationUpdate(deltaTime);
}

void Player::ClientUpdate(float deltaTime)
{
	if ((int)weapon.m_info.type != m_currentWeaponType)
	{
		weapon = Weapon((WeaponType)m_currentWeaponType);

	}

	weapon.Update(deltaTime);

	if (game.player == this) {
		float recoilT = weapon.GetRecoilAlpha();
		if (recoilT > 0.7f) {
			float f = weapon.m_info.recoilVelocity * 10.0f * (recoilT - 0.7f) * deltaTime;
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

	WeaponType currentType = weapon.m_info.type;

	switch (currentType)
	{
	case WeaponType::Pistol:
	{
		// --- 권총 로직 ---
		float recoilT = weapon.GetRecoilAlpha();

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
		float shootrate = powf(weapon.m_shootFlow / weapon.m_info.shootDelay, 3);
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
		float recoilT = weapon.GetRecoilAlpha();
		float zOffset = 0.3f;
		float pitchAngle = 0.0f;

		if (recoilT > 0.70f) {
			float stageT = (recoilT - 0.70f) / 0.30f;
			float smoothT = sinf(stageT * XM_PIDIV2);
			pitchAngle = weapon.m_info.recoilVelocity * smoothT;
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
		float currentFlow = weapon.m_shootFlow;
		float shootDelay = weapon.m_info.shootDelay;

		float recoilT = weapon.GetRecoilAlpha();
		float zOffset = 0.3f;
		float pitchAngle = 0.0f;

		if (recoilT > 0.0f) {
			zOffset -= 0.25f * powf(recoilT, 2.0f);
			pitchAngle = weapon.m_info.recoilVelocity * recoilT;
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
		float recoilT = weapon.GetRecoilAlpha();
		float zOffset = 0.3f;
		float pitchAngle = 0.0f;
		float shakeX = 0.0f;

		if (recoilT > 0.0f) {
			zOffset -= 0.07f * powf(recoilT, 1.0f);

			pitchAngle = (weapon.m_info.recoilVelocity * 0.1f) * recoilT;

			shakeX = sinf(recoilT * XM_PI * 1.0f) * 0.005f;
		}

		XMMATRIX rotMat = XMMatrixRotationX(-XMConvertToRadians(pitchAngle));
		XMMATRIX transMat = XMMatrixTranslation(shakeX, 0.0f, zOffset);

		gunMatrix_firstPersonView.mat = rotMat * transMat;
	}
	break;

	}

	if (weapon.m_shootFlow < 0) {
		float reloadProgress = 1.0f - (abs(weapon.m_shootFlow) / weapon.m_info.reloadTime);
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

static bool TryFindHumanoidNodeIndex(Model* model, HumanoidAnimation::HumanBodyBones bone, int& outNodeIndex);
static matrix GetAnimatedNodeLocalMatrix(SkinMeshGameObject* obj, Model* model, int nodeIndex);
static bool TryGetHumanoidBoneWorldMatrix(Player* player, HumanoidAnimation::HumanBodyBones bone, matrix& outBoneWorld);
static bool TryBuildThirdPersonWeaponMatrix(Player* player, WeaponType weaponType, matrix& outWeaponWorld);

void Player::Render_ThirdPersonWeapon()
{
	if (game.player == this && game.bFirstPersonVision) {
		return;
	}

	Model* pTargetModel = GetWeaponRenderModel((WeaponType)m_currentWeaponType);
	if (!pTargetModel) {
		pTargetModel = GunModel;
	}
	if (!pTargetModel) {
		return;
	}

	matrix gunmat;
	if (!TryBuildThirdPersonWeaponMatrix(this, (WeaponType)m_currentWeaponType, gunmat)) {
		gunmat = gunMatrix_thirdPersonView;
		gunmat *= XMMatrixRotationX(XM_PI);
		gunmat.pos.y -= 0.40f;
		gunmat.pos.x += 0.55f;
		gunmat.pos.z += 0.80f;
		gunmat *= worldMat;
	}

	pTargetModel->Render(gd.gpucmd, gunmat, nullptr);
}

void Player::Render_AfterDepthClear()
{
	matrix viewmat = gd.viewportArr[0].ViewMatrix.RTInverse;

	struct Model* pTargetModel = nullptr;

	if (game.bFirstPersonVision)
	{
		pTargetModel = GetWeaponRenderModel((WeaponType)m_currentWeaponType);
		if (!pTargetModel) {
			pTargetModel = GunModel;
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

				if (Game::renderViewPort == &game.MyDirLight.viewport) {
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
	if (upperAnim >= 0 && upperAnim < game.HumanoidAnimationTable.size()) {
		std::vector<matrix> upperLocal(model->nodeCount);
		for (int i = 0; i < model->nodeCount; ++i) {
			upperLocal[i].Id();
		}

		player->GetBoneLocalMatrixAtTime(&game.HumanoidAnimationTable[upperAnim], upperLocal.data(), player->AnimationFlowTime[1]);
		for (int i = 0; i < model->nodeCount; ++i) {
			int humanoidIndex = model->Humanoid_retargeting ? model->Humanoid_retargeting[i] : -1;
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

static bool TryBuildThirdPersonWeaponMatrix(Player* player, WeaponType weaponType, matrix& outWeaponWorld)
{
	matrix handWorld;
	if (!TryGetHumanoidBoneWorldMatrix(player, HumanoidAnimation::RightHand, handWorld)) {
		return false;
	}

	matrix attachWorld = player->worldMat;
	attachWorld.pos = handWorld.pos;

	matrix weaponLocal;
	weaponLocal.Id();

	switch (weaponType)
	{
	case WeaponType::Sniper:
		weaponLocal *= XMMatrixScaling(0.5f, 0.5f, 0.5f);
		weaponLocal.pos.x += 0.07f;
		weaponLocal.pos.y -= 0.02f;
		weaponLocal.pos.z += 0.28f;
		break;
	case WeaponType::MachineGun:
		weaponLocal *= XMMatrixScaling(0.45f, 0.45f, 0.45f);
		weaponLocal.pos.x += 0.08f;
		weaponLocal.pos.y -= 0.02f;
		weaponLocal.pos.z += 0.02f;
		break;
	case WeaponType::Shotgun:
		weaponLocal *= XMMatrixScaling(0.16f, 0.16f, 0.16f);
		weaponLocal *= XMMatrixRotationY(XMConvertToRadians(-90.0f));
		weaponLocal.pos.x += 0.12f;
		weaponLocal.pos.y -= 0.06f;
		weaponLocal.pos.z += 0.02f;
		break;
	case WeaponType::Rifle:
		weaponLocal *= XMMatrixScaling(1.0f, 1.0f, 1.0f);
		weaponLocal *= XMMatrixRotationY(XMConvertToRadians(90.0f));
		weaponLocal.pos.x += 0.10f;
		weaponLocal.pos.y -= 0.05f;
		weaponLocal.pos.z += 0.03f;
		break;
	case WeaponType::Pistol:
		weaponLocal *= XMMatrixScaling(1.2f, 1.2f, 1.2f);
		weaponLocal.pos.x += 0.06f;
		weaponLocal.pos.y -= 0.03f;
		weaponLocal.pos.z += 0.01f;
		break;
	}

	outWeaponWorld = weaponLocal * attachWorld;
	return true;
}

void Player::RecvSTC_SyncObj(char* data) {
	int offset = 0;
	STC_SyncObjData& stcsod = *(STC_SyncObjData*)(data);
	shape = Shape::ShapeTable[stcsod.shapeindex];
	parent = (stcsod.parent >= 0) ? game.DynmaicGameObjects[stcsod.parent] : nullptr;
	childs = (stcsod.childs >= 0) ? game.DynmaicGameObjects[stcsod.childs] : nullptr;
	sibling = (stcsod.sibling >= 0) ? game.DynmaicGameObjects[stcsod.sibling] : nullptr;
	XMMatrixDecompose((XMVECTOR*)&DestScale, (XMVECTOR*)&DestRot, (XMVECTOR*)&DestPos, stcsod.DestWorld); 
	LVelocity = stcsod.LVelocity;
	//AnimationFlowTime[0] = stcsod.AnimationFlowTime;
	//PlayingAnimationIndex[0] = stcsod.PlayingAnimationIndex;
	HP = stcsod.HP;
	MaxHP = stcsod.MaxHP;
	bullets = stcsod.bullets;
	KillCount = stcsod.KillCount;
	DeathCount = stcsod.DeathCount;
	HeatGauge = stcsod.HeatGauge;
	MaxHeatGauge = stcsod.MaxHeatGauge;
	HealSkillCooldown = stcsod.HealSkillCooldown;
	HealSkillCooldownFlow = stcsod.HealSkillCooldownFlow;
	m_currentWeaponType = stcsod.m_currentWeaponType;
	memcpy(Inventory, stcsod.Inventory, maxItem * sizeof(ItemStack));
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
	}
}

void Player::ChangeUpperState(UpperState newState)
{
	if (m_currentUpperState == newState) return;

	m_currentUpperState = newState;
	AnimationFlowTime[1] = 0.0f;

	switch (m_currentUpperState) {
	case UpperState::NONE:
		PlayingAnimationIndex[1] = -1;
		break;
	case UpperState::AIM:
		PlayingAnimationIndex[1] = 3;
		break;
	case UpperState::SHOOT:
		PlayingAnimationIndex[1] = 4;
		break;
	}
}

void Player::Release() {
	SkinMeshGameObject::Release();
}

void Portal::RecvSTC_SyncObj(char* data) {
	Portal::STC_SyncObjData& d = *(Portal::STC_SyncObjData*)data;
	shape = Shape::ShapeTable[d.shapeindex];
	worldMat = d.DestWorld;
	spawnX = d.spawnX;
	spawnY = d.spawnY;
	spawnZ = d.spawnZ;
	radius = d.radius;
	zoneId = d.zoneId;
	dstzoneId = d.dstzoneId;
	// mesh/model 파싱 없음 — 클라이언트가 자체 Portal 메쉬 사용
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

void GameMap::LoadMap(const char* MapName)
{
	GameMap* map = this;
	StartShapeIndex = Shape::ShapeTable.size();
	map->GetStartDescIndexs();

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
		mesh->CreateMesh_FromVertexAndIndexData(verts, indexs, subMeshCount, SubMeshSlots);
		//mesh->ReadMeshFromFile_OBJ(filename.c_str(), vec4(1, 1, 1, 1), false);

		ifs.read((char*)&mesh->OBB_Tr, sizeof(float) * 3);
		ifs.read((char*)&mesh->OBB_Ext, sizeof(float) * 3);
		map->meshes[i] = mesh;
		Shape::AddMesh(name, mesh);
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

		mat->ShiftTextureIndexs(TextureTableStart);
		mat->SetDescTable();

		game.MaterialTable.push_back(mat);
		if (game.isAssetAddingInGlobal == false) {
			game.RenderMaterialTable.push_back(mat);
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
		// .map (확장자)제거
		filename += modelName;
		filename += ".model";

		Model* pModel = new Model();
		pModel->LoadModelFile2(filename);
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
						go->material[k] = MaterialTableStart + materialId;
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
	map->GetLastDescIndexs();
	//BakeStaticCollision();
}

void GameMap::Release() {
	name.clear();
	meshes.clear();
	models.clear();
	MapObjects.clear();
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

#include "stdafx.h"
#include "GameObject.h"
#include <set>

unordered_map<string, int> Shape::StrToShapeIndex;
vector<Shape> Shape::ShapeTable;
vector<string> Shape::ShapeStrTable;
extern World gameworld;
extern float DeltaTime;

unordered_map<type_offset_pair, short, hash<type_offset_pair>> GameObjectType::GetClientOffset;
unordered_map<void*, GameObjectType> GameObjectType::VptrToTypeTable;

void GameObjectType::STATICINIT()
{
	GameObjectType::VptrToTypeTable.insert(pair<void*, GameObjectType>(GetVptr<GameObject>(), GameObjectType::_GameObject));
	GameObjectType::VptrToTypeTable.insert(pair<void*, GameObjectType>(GetVptr<Player>(), GameObjectType::_Player));
	GameObjectType::VptrToTypeTable.insert(pair<void*, GameObjectType>(GetVptr<Monster>(), GameObjectType::_Monster));
}

void Mesh::ReadMeshFromFile_OBJ(const char* path, bool centering)
{
	XMFLOAT3 maxPos = { 0, 0, 0 };
	XMFLOAT3 minPos = { 0, 0, 0 };
	std::ifstream in{ path };

	while (in.eof() == false && (in.is_open() && in.fail() == false)) {
		char rstr[128] = {};
		in >> rstr;
		if (strcmp(rstr, "v") == 0) {
			//좌표
			XMFLOAT3 pos;
			in >> pos.x;
			in >> pos.y;
			in >> pos.z;
			if (maxPos.x < pos.x) maxPos.x = pos.x;
			if (maxPos.y < pos.y) maxPos.y = pos.y;
			if (maxPos.z < pos.z) maxPos.z = pos.z;
			if (minPos.x > pos.x) minPos.x = pos.x;
			if (minPos.y > pos.y) minPos.y = pos.y;
			if (minPos.z > pos.z) minPos.z = pos.z;
		}
	}
	// For each vertex of each triangle
	in.close();

	XMFLOAT3 CenterPos;
	if (centering) {
		Center = 0;
	}
	else {
		CenterPos.x = (maxPos.x + minPos.x) * 0.5f;
		CenterPos.y = (maxPos.y + minPos.y) * 0.5f;
		CenterPos.z = (maxPos.z + minPos.z) * 0.5f;
		Center.f3 = CenterPos;
	}
	MAXpos.x = (maxPos.x - minPos.x) * 0.5f;
	MAXpos.y = (maxPos.y - minPos.y) * 0.5f;
	MAXpos.z = (maxPos.z - minPos.z) * 0.5f;

	// fbx의 기본 크기 비율이 100 이기 때문에 이렇게 조정했음.
	// 하지만 UnitScale이 다를 경우에는 어떻게 하는가?
	// 조치가 필요하다.
	MAXpos *= 0.01f;

	subMeshNum = 1;
}

BoundingOrientedBox Mesh::GetOBB()
{
	return BoundingOrientedBox(Center.f3, MAXpos.f3, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
}

void Mesh::CreateWallMesh(float width, float height, float depth)
{
	MAXpos = XMFLOAT3(width, height, depth);
}

void Mesh::SetOBBDataWithAABB(XMFLOAT3 minpos, XMFLOAT3 maxpos)
{
	vec4 max = maxpos;
	vec4 min = minpos;
	vec4 mid = 0.5f * (max + min);
	vec4 ext = max - mid;
	MAXpos = ext.f3;
	Center = mid.f3;
}

int Shape::AddMesh(string name, Mesh* ptr)
{
	Shape s;
	s.SetMesh(ptr);
	int index = ShapeTable.size();
	ShapeTable.push_back(s);
	ShapeStrTable.push_back(name);
	StrToShapeIndex.insert(pair<string, int>(name, index));
	return index;
}

int Shape::AddModel(string name, Model* ptr)
{
	Shape s;
	s.SetModel(ptr);
	int index = ShapeTable.size();
	ShapeTable.push_back(s);
	ShapeStrTable.push_back(name);
	StrToShapeIndex.insert(pair<string, int>(name, index));
	return index;
}

GameObject::GameObject()
{
	tag[GameObjectTag::Tag_Enable] = false;
	tag[GameObjectTag::Tag_Dynamic] = false;
	worldMat.Id();
	shapeindex = -1;
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
	if (shapeindex < 0) {
		obb_local.Extents.x = -1;
		return obb_local;
	}
	Shape::ShapeTable[shapeindex].GetRealShape(mesh, model);
	if (mesh != nullptr) obb_local = mesh->GetOBB();
	if (model != nullptr) obb_local = model->GetOBB();
	BoundingOrientedBox obb_world;
	obb_local.Transform(obb_world, worldMat);
	return obb_world;
}

void GameObject::SetShape(int _shapeindex)
{
	shapeindex = _shapeindex;
	Shape& s = Shape::ShapeTable[shapeindex];
	Mesh* mesh = nullptr;
	Model* model = nullptr;
	s.GetRealShape(mesh, model);
	if (mesh) {

	}
	else {
		if (transforms_innerModel == nullptr) {
			transforms_innerModel = new matrix[model->nodeCount];
		}
		else {
		}

		for (int i = 0;i < model->nodeCount;++i) {
			transforms_innerModel[i] = model->Nodes[i].transform;
		}
	}
}

void GameObject::OnRayHit(GameObject* rayFrom) {

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
	int temp = parent;
	if (temp >= 0) {
		StaticGameObject* obj = gameworld.Static_gameObjects[temp];
		sav *= obj->worldMat;
		temp = obj->parent;
	}
	worldMat = sav;
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
	Shape::ShapeTable[shapeindex].GetRealShape(mesh, model);
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

void DynamicGameObject::InitialChunkSetting()
{
	if (chunkAllocIndexs == nullptr) {
		BoundingOrientedBox obb = GetOBB();
		vec4 ext = obb.Extents;
		float len = ext.len3 / gameworld.chunck_divide_Width;
		chunkAllocIndexsCapacity = powf(2 * ceilf(len * 0.5f), 3);
		chunkAllocIndexs = new int[chunkAllocIndexsCapacity];
	}
}

matrix DynamicGameObject::GetWorld() {
	matrix sav = worldMat;
	GameObject* obj = gameworld.Dynamic_gameObjects[parent];
	while (obj != nullptr) {
		sav *= obj->worldMat;
		if (obj->tag[GameObjectTag::Tag_Dynamic] == false) break;
		obj = gameworld.Dynamic_gameObjects[obj->parent];
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
				GameChunk* gc = gameworld.chunck[ChunkIndex(ix, iy, iz)];
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
	IncludeChunks = gameworld.GetChunks_Include_OBB(GetOBB());

	xmax = IncludeChunks.xmin + IncludeChunks.xlen;
	ymax = IncludeChunks.ymin + IncludeChunks.ylen;
	zmax = IncludeChunks.zmin + IncludeChunks.zlen;
	up = 0;
	for (int ix = IncludeChunks.xmin; ix <= xmax; ++ix) {
		for (int iy = IncludeChunks.ymin; iy <= ymax; ++iy) {
			for (int iz = IncludeChunks.zmin; iz <= zmax; ++iz) {
				auto c = gameworld.chunck.find(ChunkIndex(ix, iy, iz));
				GameChunk* gc;
				if (c == gameworld.chunck.end()) {
					// new game chunk
					gc = new GameChunk();
					gc->SetChunkIndex(ChunkIndex(ix, iy, iz));
					gameworld.chunck.insert(pair<ChunkIndex, GameChunk*>(ChunkIndex(ix, iy, iz), gc));
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
				gameworld.chunck[ChunkIndex(ix, iy, iz)]->Dynamic_gameobjects.Free(chunkAllocIndexs[up]);
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
				auto c = gameworld.chunck.find(ChunkIndex(ix, iy, iz));
				GameChunk* gc;
				if (c == gameworld.chunck.end()) {
					// new game chunk
					gc = new GameChunk();
					gc->SetChunkIndex(ChunkIndex(ix, iy, iz));
					gameworld.chunck.insert(pair<ChunkIndex, GameChunk*>(ChunkIndex(ix, iy, iz), gc));
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

void DynamicGameObject::Update(float delatTime) {

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
					gameworld.chunck[ChunkIndex(ix, iy, iz)]->Dynamic_gameobjects.Free(chunkAllocIndexs[up]);
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
	Shape::ShapeTable[shapeindex].GetRealShape(mesh, model);
	if (mesh != nullptr) obb_local = mesh->GetOBB();
	if (model != nullptr) obb_local = model->GetOBB();

	matrix sav = GetWorld();
	BoundingOrientedBox obb_world;
	obb_local.Transform(obb_world, sav);

	return obb_world;
}

void DynamicGameObject::CollisionMove(DynamicGameObject* gbj1, DynamicGameObject* gbj2)
{
	constexpr float epsillon = 0.01f;

	bool bi = XMColorEqual(gbj1->tickLVelocity, XMVectorZero());
	bool bj = XMColorEqual(gbj2->tickLVelocity, XMVectorZero());

	DynamicGameObject* movObj = nullptr;
	DynamicGameObject* colObj = nullptr;
	BoundingOrientedBox obb1 = gbj1->GetOBB();
	BoundingOrientedBox obb2 = gbj2->GetOBB();

Collision_By_Move:

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
		gbj1->worldMat.pos += gbj1->tickLVelocity;
		obb1 = gbj1->GetOBB();
		gbj1->worldMat.pos -= gbj1->tickLVelocity;
		gbj2->worldMat.pos += gbj2->tickLVelocity;
		obb2 = gbj2->GetOBB();
		gbj2->worldMat.pos -= gbj2->tickLVelocity;

		if (obb1.Intersects(obb2)) {
			gbj1->OnCollision(gbj2);
			gbj2->OnCollision(gbj1);

			float mul = 0.5f;
			float rate = 0.25f;
			float maxLen = XMVector3Length(gbj1->tickLVelocity).m128_f32[0];
			float temp = XMVector3Length(gbj2->tickLVelocity).m128_f32[0];
			if (maxLen < temp) maxLen = temp;

		CMP_INTERSECT:
			vec4 v1 = mul * gbj1->tickLVelocity;
			vec4 v2 = mul * gbj2->tickLVelocity;
			gbj1->worldMat.pos += v1;
			obb1 = gbj1->GetOBB();
			gbj1->worldMat.pos -= v1;
			gbj2->worldMat.pos += v2;
			obb2 = gbj2->GetOBB();
			gbj2->worldMat.pos -= v2;
			bool isMoveForward = false;
			if (obb1.Intersects(obb2)) {
				mul -= rate;
				rate *= 0.5f;
				if (maxLen * rate > epsillon) goto CMP_INTERSECT;
			}
			else {
				isMoveForward = true;
				mul += rate;
				rate *= 0.5f;
				if (maxLen * rate > epsillon) goto CMP_INTERSECT;
			}

			if (isMoveForward == false) {
				v1 = 0;
				v2 = 0;
			}

			vec4 preMove1 = v1;
			vec4 postMove1 = gbj1->tickLVelocity - v1;
			gbj1->tickLVelocity = postMove1;

			vec4 preMove2 = v2;
			vec4 postMove2 = gbj2->tickLVelocity - v2;
			gbj2->tickLVelocity = postMove2;

			gbj2->worldMat.pos += preMove2;
			obb2 = gbj2->GetOBB();
			gbj2->worldMat.pos -= preMove2;

			CollisionMove_DivideBaseline_rest(gbj1, gbj2, obb2, preMove1);
			gbj1->tickLVelocity += preMove1;

			gbj1->worldMat.pos += gbj1->tickLVelocity;
			obb1 = gbj1->GetOBB();
			gbj1->worldMat.pos -= gbj1->tickLVelocity;

			CollisionMove_DivideBaseline_rest(gbj2, gbj1, obb1, preMove2);
			gbj2->tickLVelocity += preMove2;
		}
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

		CollisionMove_DivideBaseline(movObj, colObj);
	}
}

void DynamicGameObject::CollisionMove_DivideBaseline(DynamicGameObject* movObj, DynamicGameObject* colObj)
{
	BoundingOrientedBox colOBB = colObj->GetOBB();
	XMMATRIX basemat = colObj->worldMat;
	XMMATRIX invmat = colObj->worldMat.RTInverse;
	invmat.r[3] = XMVectorSet(0, 0, 0, 1);
	XMVECTOR BaseLine = XMVector3Transform(movObj->tickLVelocity, invmat);
	movObj->tickLVelocity = XMVectorZero();

	XMVECTOR MoveVector = basemat.r[0] * BaseLine.m128_f32[0];
	movObj->tickLVelocity += MoveVector;
	movObj->worldMat.pos += movObj->tickLVelocity;
	BoundingOrientedBox obb1 = movObj->GetOBB();
	movObj->worldMat.pos -= movObj->tickLVelocity;
	if (obb1.Intersects(colOBB)) {
		movObj->tickLVelocity -= MoveVector;
	}

	MoveVector = basemat.r[1] * BaseLine.m128_f32[1];
	movObj->tickLVelocity += MoveVector;
	movObj->worldMat.pos += movObj->tickLVelocity;
	obb1 = movObj->GetOBB();
	movObj->worldMat.pos -= movObj->tickLVelocity;
	if (obb1.Intersects(colOBB)) {
		movObj->tickLVelocity -= MoveVector;
	}

	MoveVector = basemat.r[2] * BaseLine.m128_f32[2];
	movObj->tickLVelocity += MoveVector;
	movObj->worldMat.pos += movObj->tickLVelocity;
	obb1 = movObj->GetOBB();
	movObj->worldMat.pos -= movObj->tickLVelocity;
	if (obb1.Intersects(colOBB)) {
		movObj->tickLVelocity -= MoveVector;
	}
}

void DynamicGameObject::CollisionMove_DivideBaseline_StaticOBB(DynamicGameObject* movObj, BoundingOrientedBox colOBB)
{
	XMFLOAT3 Corners[8] = {};
	colOBB.GetCorners((XMFLOAT3*)Corners);

	matrix basemat;
	basemat.right = vec4(Corners[4]) - vec4(Corners[0]);
	basemat.right.len3 = 1;
	basemat.up = vec4(Corners[2]) - vec4(Corners[1]);
	basemat.up.len3 = 1;
	basemat.look = vec4(Corners[6]) - vec4(Corners[7]);
	basemat.look.len3 = 1;
	basemat.pos = vec4(0, 0, 0, 1);

	//XMMATRIX basemat = colObj->m_worldMatrix;
	XMMATRIX invmat = basemat.RTInverse;
	invmat.r[3] = XMVectorSet(0, 0, 0, 1);
	XMVECTOR BaseLine = XMVector3Transform(movObj->tickLVelocity, invmat);
	movObj->tickLVelocity = XMVectorZero();

	XMVECTOR MoveVector = basemat.mat.r[0] * BaseLine.m128_f32[0];
	movObj->tickLVelocity += MoveVector;
	movObj->worldMat.pos += movObj->tickLVelocity;
	BoundingOrientedBox obb1 = movObj->GetOBB();
	movObj->worldMat.pos -= movObj->tickLVelocity;
	if (obb1.Intersects(colOBB)) {
		movObj->tickLVelocity -= MoveVector;
	}

	MoveVector = basemat.mat.r[1] * BaseLine.m128_f32[1];
	movObj->tickLVelocity += MoveVector;
	movObj->worldMat.pos += movObj->tickLVelocity;
	obb1 = movObj->GetOBB();
	movObj->worldMat.pos -= movObj->tickLVelocity;
	if (obb1.Intersects(colOBB)) {
		movObj->tickLVelocity -= MoveVector;
	}

	MoveVector = basemat.mat.r[2] * BaseLine.m128_f32[2];
	movObj->tickLVelocity += MoveVector;
	movObj->worldMat.pos += movObj->tickLVelocity;
	obb1 = movObj->GetOBB();
	movObj->worldMat.pos -= movObj->tickLVelocity;
	if (obb1.Intersects(colOBB)) {
		movObj->tickLVelocity -= MoveVector;
	}
}

void DynamicGameObject::CollisionMove_DivideBaseline_rest(DynamicGameObject* movObj, DynamicGameObject* colObj, BoundingOrientedBox colOBB, vec4 preMove)
{
	movObj->worldMat.pos += preMove;

	XMMATRIX basemat = colObj->worldMat;
	XMMATRIX invmat = colObj->worldMat.RTInverse;
	invmat.r[3] = XMVectorSet(0, 0, 0, 1);
	XMVECTOR BaseLine = XMVector3Transform(movObj->tickLVelocity, invmat);
	movObj->tickLVelocity = XMVectorZero();

	XMVECTOR MoveVector = basemat.r[0] * BaseLine.m128_f32[0];
	movObj->tickLVelocity += MoveVector;
	movObj->worldMat.pos += movObj->tickLVelocity;
	BoundingOrientedBox obb1 = movObj->GetOBB();
	movObj->worldMat.pos -= movObj->tickLVelocity;
	if (obb1.Intersects(colOBB)) {
		movObj->tickLVelocity -= MoveVector;
	}

	MoveVector = basemat.r[1] * BaseLine.m128_f32[1];
	movObj->tickLVelocity += MoveVector;
	movObj->worldMat.pos += movObj->tickLVelocity;
	obb1 = movObj->GetOBB();
	movObj->worldMat.pos -= movObj->tickLVelocity;
	if (obb1.Intersects(colOBB)) {
		movObj->tickLVelocity -= MoveVector;
	}

	MoveVector = basemat.r[2] * BaseLine.m128_f32[2];
	movObj->tickLVelocity += MoveVector;
	movObj->worldMat.pos += movObj->tickLVelocity;
	obb1 = movObj->GetOBB();
	movObj->worldMat.pos -= movObj->tickLVelocity;
	if (obb1.Intersects(colOBB)) {
		movObj->tickLVelocity -= MoveVector;
	}

	movObj->worldMat.pos -= preMove;
}

void DynamicGameObject::OnCollision(GameObject* other)
{
}

void DynamicGameObject::OnStaticCollision(BoundingOrientedBox obb)
{
}

void DynamicGameObject::OnCollisionRayWithBullet(GameObject* shooter, float damage)
{
}

void DynamicGameObject::OnRayHit(GameObject* rayFrom)
{
}

SkinMeshGameObject::SkinMeshGameObject() {
	tag[GameObjectTag::Tag_Enable] = false;
	tag[GameObjectTag::Tag_Dynamic] = true;
	tag[GameObjectTag::Tag_SkinMeshObject] = true;
}

SkinMeshGameObject::~SkinMeshGameObject() {
}

void SkinMeshGameObject::Update(float delatTime) {
	AnimationFlowTime += DeltaTime;
}

void GameMap::ExtendMapAABB(BoundingOrientedBox obb)
{
	vec4 aabb[2];
	Mesh::GetAABBFromOBB(aabb, obb, true);

	if (AABB[0].x > aabb[0].x) AABB[0].x = aabb[0].x;
	if (AABB[0].y > aabb[0].y) AABB[0].y = aabb[0].y;
	if (AABB[0].z > aabb[0].z) AABB[0].z = aabb[0].z;

	if (AABB[1].x < aabb[1].x) AABB[1].x = aabb[1].x;
	if (AABB[1].y < aabb[1].y) AABB[1].y = aabb[1].y;
	if (AABB[1].z < aabb[1].z) AABB[1].z = aabb[1].z;
}

void GameMap::BakeStaticCollision()
{
	MapObjects[0]->InitMapAABB_Inherit(this, XMMatrixIdentity());
}

void GameMap::StaticCollisionMove(DynamicGameObject* obj)
{
	obj->worldMat.pos += obj->tickLVelocity;
	BoundingOrientedBox obb = obj->GetOBB();
	obj->worldMat.pos -= obj->tickLVelocity;

	// 움직인 후의 청크영역을 찾아냄.
	GameObjectIncludeChunks goic = gameworld.GetChunks_Include_OBB(obb);

	for (int ix = goic.xmin; ix <= goic.xmin + goic.xlen; ++ix) {
		for (int iy = goic.ymin; iy <= goic.ymin + goic.ylen; ++iy) {
			for (int iz = goic.zmin; iz <= goic.zmin + goic.zlen; ++iz) {
				GameChunk* ch = gameworld.chunck[ChunkIndex(ix, iy, iz)];
				//Static Object는 Enable이 false일 수 없기 때문에 할당검사는 안함.
				// >> 그럼 그냥 vector여도 상관없잖음 왜 vecset으로 함? >> fix

				//어짜피 서버면 청크를 다 만드는게 맞지 않을까? 그럼 nullptr 체크를 할 필요가 있나?
				// fix
				if (ch != nullptr) {
					for (int k = 0; k < ch->Static_gameobjects.size; ++k) {
						BoundingOrientedBox staticobb = ch->Static_gameobjects[k]->GetOBB();

						obj->worldMat.pos += obj->tickLVelocity;
						BoundingOrientedBox obb1 = obj->GetOBB();
						obj->worldMat.pos -= obj->tickLVelocity;

						if (obb1.Intersects(staticobb)) {
							obj->OnStaticCollision(staticobb);
							DynamicGameObject::CollisionMove_DivideBaseline_StaticOBB(obj, staticobb);
						}

						if (obj->tickLVelocity.fast_square_of_len3 <= 0.001f) return;
					}
				}
			}
		}
	}
}

bool GameMap::isStaticCollision(BoundingOrientedBox obb)
{
	GameObjectIncludeChunks goic = gameworld.GetChunks_Include_OBB(obb);

	for (int ix = goic.xmin; ix <= goic.xmin + goic.xlen; ++ix) {
		for (int iy = goic.ymin; iy <= goic.ymin + goic.ylen; ++iy) {
			for (int iz = goic.zmin; iz <= goic.zmin + goic.zlen; ++iz) {
				GameChunk* ch = gameworld.chunck[ChunkIndex(ix, iy, iz)];
				if (ch != nullptr) {
					for (int k = 0; k < ch->Static_gameobjects.size; ++k) {
						BoundingOrientedBox staticobb = ch->Static_gameobjects[k]->GetOBB();

						if (obb.Intersects(staticobb)) {
							return true;
						}
					}
				}
			}
		}
	}
	return false;
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

	ifstream ifs{ MapFilePath, ios_base::binary};
	if (ifs.good()) {
		char Buff[1024] = {};
		GetCurrentDirectoryA(1024, Buff);
		cout << endl;
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
	map->name.reserve(nameCount);
	map->name.resize(nameCount);
	map->meshes.reserve(MeshCount);
	map->meshes.resize(MeshCount);
	mesh_shapeindexes.reserve(MeshCount);
	mesh_shapeindexes.resize(MeshCount);
	map->models.reserve(ModelCount);
	map->models.resize(ModelCount);
	model_shapeindexes.reserve(ModelCount);
	model_shapeindexes.resize(ModelCount);
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

	TextureTableStart = 0; // fix : 현재 MaterialTexture 개수
	MaterialTableStart = gameworld.MaterialCount; // fix : 현재 Material 개수

	constexpr char MeshDir[] = "Mesh/";
	constexpr char TextureDir[] = "Texture/";
	constexpr char ModelDir[] = "Model/";
	string MeshDirPath = dirName;
	MeshDirPath += "/Mesh/";

	string TextureDirPath = dirName;
	TextureDirPath += "/Texture/";

	string ModelDirPath = dirName;
	ModelDirPath += "/Model/";

	vec4 tempRead;
	for (int i = 0; i < MeshCount; ++i) {
		int nameid = 0;
		ifs.read((char*)&nameid, sizeof(int));
		string& name = map->name[nameid];
		Mesh mesh;

		string filename = MeshDirPath;
		// .map (확장자)제거
		filename += name;
		filename += ".mesh";
		ifstream ifs2{ filename, ios_base::binary };
		int vertCnt = 0;
		int subMeshCount = 0;
		ifs2.read((char*)&vertCnt, sizeof(int));
		ifs2.read((char*)&subMeshCount, sizeof(int));
		mesh.subMeshNum = subMeshCount;
		ifs2.close();

		//string filename = MeshDirPath;
		//// .map (확장자)제거
		//filename += name;
		//filename += ".mesh";
		//Mesh mesh;
		//ifstream ifs2{ filename, ios_base::binary };
		//int vertCnt = 0;
		//int indCnt = 0;
		//ifs2.read((char*)&vertCnt, sizeof(int));
		//ifs2.read((char*)&indCnt, sizeof(int));
		//int tricnt = (indCnt / 3);
		//
		//for (int k = 0; k < vertCnt; ++k) {
		//	ifs2.read((char*)&tempRead, sizeof(float) * 3);
		//	ifs2.read((char*)&tempRead, sizeof(float) * 2);
		//	ifs2.read((char*)&tempRead, sizeof(float) * 3);
		//	ifs2.read((char*)&tempRead, sizeof(float) * 3);
		//	float w = 0; // bitangent direction??
		//	ifs2.read((char*)&w, sizeof(float));
		//}
		//for (int k = 0; k < tricnt; ++k) {
		//	ifs2.read((char*)&tempRead, sizeof(UINT));
		//	ifs2.read((char*)&tempRead, sizeof(UINT));
		//	ifs2.read((char*)&tempRead, sizeof(UINT));
		//}
		//ifs2.close();
		
		//OBB Generate
		//mesh->CreateMesh_FromVertexAndIndexData(verts, indexs);
		//mesh->ReadMeshFromFile_OBJ(filename.c_str(), vec4(1, 1, 1, 1), false);

		ifs.read((char*)&mesh.Center, sizeof(float) * 3);
		ifs.read((char*)&mesh.MAXpos, sizeof(float) * 3);
		map->meshes[i] = mesh;
		mesh_shapeindexes[i] = Shape::AddMesh(map->name[nameid], &map->meshes[i]);
	}

	for (int i = 0; i < TextureCount; ++i) {
		int namelen = 0;
		ifs.read((char*)&namelen, sizeof(int));
		char TempBuff[512] = {};
		ifs.read((char*)&TempBuff, namelen);
		TempBuff[namelen] = 0;

		int width = 0;
		int height = 0;
		int format = 0; // Unity GraphicsFormat

		ifs.read((char*)&width, sizeof(int));
		ifs.read((char*)&height, sizeof(int));
		ifs.read((char*)&format, sizeof(int));
	}

	for (int i = 0; i < MaterialCount; ++i) {
		int namelen = 0;
		ifs.read((char*)&namelen, sizeof(int));
		char name[512] = {};
		ifs.read((char*)name, namelen * sizeof(char));
		name[namelen] = 0;

		ifs.read((char*)&tempRead, sizeof(float) * 4);
		ifs.read((char*)&tempRead, sizeof(float));
		ifs.read((char*)&tempRead, sizeof(float));
		ifs.read((char*)&tempRead, sizeof(float));
		ifs.read((char*)&tempRead, sizeof(float) * 2);
		ifs.read((char*)&tempRead, sizeof(float) * 2);
		ifs.read((char*)&tempRead, sizeof(float) * 2);
		ifs.read((char*)&tempRead, sizeof(float) * 2);
		ifs.read((char*)&tempRead, sizeof(bool));
		ifs.read((char*)&tempRead, sizeof(bool));
		ifs.read((char*)&tempRead, sizeof(int));
		ifs.read((char*)&tempRead, sizeof(int));
		ifs.read((char*)&tempRead, sizeof(int));
		ifs.read((char*)&tempRead, sizeof(int));
		ifs.read((char*)&tempRead, sizeof(int));
		ifs.read((char*)&tempRead, sizeof(int));
		ifs.read((char*)&tempRead, sizeof(int));
		ifs.read((char*)&tempRead, sizeof(int));
		gameworld.MaterialCount += 1;
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
		model_shapeindexes[i] = Shape::AddModel(modelName, pModel);
	}

	gameworld.Static_gameObjects.Init(gameObjectCount);
	for (int i = 0; i < gameObjectCount; ++i) {
		StaticGameObject* go = new StaticGameObject();
		map->MapObjects[i] = go;
		go->parent = -1;
		go->childs = -1;
		go->sibling = -1;
		int index = gameworld.Static_gameObjects.Alloc();
		gameworld.Static_gameObjects[i] = go;
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

		char Mod = 'n';
		ifs.read((char*)&Mod, sizeof(char));
		if (Mod == 'n') { // mesh
			int meshid = 0;
			int materialNum = 0;
			int materialId = 0;
			ifs.read((char*)&meshid, sizeof(int));
			if (meshid < 0) {
				//go->mesh = nullptr;
				ifs.read((char*)&materialNum, sizeof(int));
			}
			else {
				go->SetShape(map->mesh_shapeindexes[meshid]);
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
		else if (Mod == 'm'){
			//Model
			int ModelID;
			ifs.read((char*)&ModelID, sizeof(int));
			if (ModelID < 0) {
				go->shapeindex = -1;
			}
			else {
				go->SetShape(map->model_shapeindexes[ModelID]);
			}

			int nodeCount = Shape::ShapeTable[go->shapeindex].GetModel()->nodeCount;
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
		}
		else if (Mod == 'l') {
			// is light (no data in server.)
			int n = 0;
			ifs.read((char*)&n, sizeof(int));
			go->shapeindex = -1;
			float f = 0;
			ifs.read((char*)&f, sizeof(float));
			ifs.read((char*)&f, sizeof(float));
			ifs.read((char*)&f, sizeof(float));
		}

		if (Mod != 'm') {
			int childCount = 0;
			ifs.read((char*)&childCount, sizeof(int));
			int cnt = 0;
			StaticGameObject** temp = &go;
			while (cnt < childCount) {
				int childIndex = 0;
				ifs.read((char*)&childIndex, sizeof(int));
				*temp = map->MapObjects[childIndex];
				(*temp)->parent = i;
				cnt += 1;
			}
		}
		else {
			go->childs = -1;
		}

		go->SetWorld(world);
		map->MapObjects[i] = go;
	}

	//BakeStaticCollision();
}

void Model::LoadModelFile2(string filename)
{
	ifstream ifs{ filename, ios_base::binary };
	ifs.read((char*)&mNumMeshes, sizeof(unsigned int));
	mMeshes = new Mesh * [mNumMeshes];

	ifs.read((char*)&nodeCount, sizeof(unsigned int));
	Nodes = new ModelNode[nodeCount];

	int MaterialTableStart = gameworld.MaterialCount;

	//new0
	unsigned int mNumTextures;
	unsigned int mNumMaterials;
	ifs.read((char*)&mNumTextures, sizeof(unsigned int));
	ifs.read((char*)&mNumMaterials, sizeof(unsigned int));

	int BSMCount = 0;
	//unsigned int MaterialTableStart = gameworld.MaterialTable.size();
	for (int i = 0; i < mNumMeshes; ++i) {
		Mesh* mesh = new Mesh();
		
		bool hasBone = false;
		ifs.read((char*)&hasBone, sizeof(bool));

		XMFLOAT3 AABB[2];
		ifs.read((char*)AABB, sizeof(XMFLOAT3) * 2);
		mesh->SetOBBDataWithAABB(AABB[0], AABB[1]);
		//new1
		//ifs.read((char*)&mesh->material_index, sizeof(int));

		XMFLOAT3 MAABB[2];
		unsigned int vertSiz = 0;
		unsigned int subMeshCount = 0;
		ifs.read((char*)&vertSiz, sizeof(unsigned int));
		ifs.read((char*)&subMeshCount, sizeof(unsigned int));
		mesh->subMeshNum = subMeshCount;

		int stackSiz = 0;
		int prevSiz = 0;
		int* SubMeshSlots = new int[subMeshCount + 1];
		SubMeshSlots[0] = 0;

		if (hasBone) {
			BSMCount += 1;

			for (int k = 0; k < vertSiz; ++k) {
				XMFLOAT3 dumy = {0, 0, 0};
				ifs.read((char*)&dumy, sizeof(XMFLOAT3));
				ifs.read((char*)&dumy, sizeof(float));
				ifs.read((char*)&dumy, sizeof(float));
				ifs.read((char*)&dumy, sizeof(XMFLOAT3));
				ifs.read((char*)&dumy, sizeof(XMFLOAT3));

				// non use
				XMFLOAT3 bitangent;
				ifs.read((char*)&bitangent, sizeof(XMFLOAT3));

				//bonedata
				int boneindex = 0;
				float boneweight = 0;
				for (int u = 0; u < 4; ++u) {
					ifs.read((char*)&boneindex, sizeof(int));
					ifs.read((char*)&boneweight, sizeof(float));
				}
			}

			for (int k = 0; k < subMeshCount; ++k) {
				int indCnt = 0;
				ifs.read((char*)&indCnt, sizeof(int));
				int tricnt = (indCnt / 3);
				stackSiz += tricnt;
				UINT dumy = 0;
				for (int k = 0; k < tricnt; ++k) {
					ifs.read((char*)&dumy, sizeof(UINT));
					ifs.read((char*)&dumy, sizeof(UINT));
					ifs.read((char*)&dumy, sizeof(UINT));
				}
				prevSiz = stackSiz;
				SubMeshSlots[k + 1] = 3 * prevSiz;
			}

			UINT dumy = 0;
			ifs.read((char*)&dumy, sizeof(int));
			for (int k = 0; k < dumy; ++k) {
				matrix offset;
				ifs.read((char*)&offset, sizeof(matrix));
			}
			for (int k = 0; k < dumy; ++k) {
				UINT dumy2 = 0;
				ifs.read((char*)&dumy2, sizeof(int));
			}

			mMeshes[i] = mesh;
		}
		else {
			mesh->SetOBBDataWithAABB(AABB[0], AABB[1]);

			for (int k = 0; k < vertSiz; ++k) {
				XMFLOAT3 dumy = { 0, 0, 0 };
				ifs.read((char*)&dumy, sizeof(XMFLOAT3));

				ifs.read((char*)&dumy, sizeof(float));
				ifs.read((char*)&dumy, sizeof(float));
				ifs.read((char*)&dumy, sizeof(XMFLOAT3));
				ifs.read((char*)&dumy, sizeof(XMFLOAT3));

				// non use
				XMFLOAT3 bitangent;
				ifs.read((char*)&bitangent, sizeof(XMFLOAT3));
			}

			for (int k = 0; k < subMeshCount; ++k) {
				int indCnt = 0;
				ifs.read((char*)&indCnt, sizeof(int));
				int tricnt = (indCnt / 3);
				stackSiz += tricnt;
				for (int k = 0; k < tricnt; ++k) {
					UINT dumy2 = 0;
					ifs.read((char*)&dumy2, sizeof(UINT));
					ifs.read((char*)&dumy2, sizeof(UINT));
					ifs.read((char*)&dumy2, sizeof(UINT));
				}
				prevSiz = stackSiz;
				SubMeshSlots[k + 1] = 3 * prevSiz;
			}

			mMeshes[i] = mesh;
		}
	}

	for (int i = 0; i < nodeCount; ++i) {
		int namelen = 0;
		ifs.read((char*)&namelen, sizeof(int));
		char name[256] = {};
		ifs.read((char*)name, namelen);
		name[namelen] = 0;

		/*vec4 pos;
		ifs.read((char*)&pos, sizeof(float) * 3);
		vec4 rot = 0;
		ifs.read((char*)&rot, sizeof(float) * 3);
		vec4 scale = 0;
		ifs.read((char*)&scale, sizeof(float) * 3);*/
		matrix WorldMat;
		ifs.read((char*)&WorldMat, sizeof(matrix));

		if (i == 0) {
			matrix mat;
			mat.Id();
			Nodes[i].transform = mat;
		}
		else {
			/*matrix mat;
			mat.Id();
			rot *= 3.141592f / 180.0f;
			mat *= XMMatrixScaling(scale.x, scale.y, scale.z);
			mat *= XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z);
			mat.pos.f3 = pos.f3;
			mat.pos.w = 1;*/

			Nodes[i].transform = WorldMat;
		}

		int parent = 0;
		ifs.read((char*)&parent, sizeof(int));
		if (parent < 0) Nodes[i].parent = nullptr;
		else Nodes[i].parent = &Nodes[parent];

		ifs.read((char*)&Nodes[i].numChildren, sizeof(unsigned int));
		if (Nodes[i].numChildren != 0) Nodes[i].Childrens = new ModelNode * [Nodes[i].numChildren];

		ifs.read((char*)&Nodes[i].numMesh, sizeof(unsigned int));
		if (Nodes[i].numMesh != 0) Nodes[i].Meshes = new unsigned int[Nodes[i].numMesh];

		for (int k = 0; k < Nodes[i].numChildren; ++k) {
			int child = 0;
			ifs.read((char*)&child, sizeof(int));
			if (child < 0) Nodes[i].Childrens[k] = nullptr;
			else Nodes[i].Childrens[k] = &Nodes[child];
		}

		int tempMeshIndexArr[256] = {};
		for (int k = 0; k < Nodes[i].numMesh; ++k) {
			ifs.read((char*)&Nodes[i].Meshes[k], sizeof(int));
			int num_materials = 0;
			ifs.read((char*)&num_materials, sizeof(int));
			Nodes[i].materialIndex = new int[num_materials];

			for (int u = 0; u < num_materials; ++u) {
				int material_id = 0;
				ifs.read((char*)&material_id, sizeof(int));
				Nodes[i].materialIndex[u] = MaterialTableStart + material_id;
			}
		}

		//ifs.read((char*)&Nodes[i].Meshes[0], sizeof(int) * Nodes[i].numMesh);

		int ColliderCount = 0;
		ifs.read((char*)&ColliderCount, sizeof(int));
		Nodes[i].aabbArr.reserve(ColliderCount);
		Nodes[i].aabbArr.resize(ColliderCount);
		for (int k = 0; k < ColliderCount; ++k) {
			ifs.read((char*)&Nodes[i].aabbArr[k].Center, sizeof(XMFLOAT3));
			ifs.read((char*)&Nodes[i].aabbArr[k].Extents, sizeof(XMFLOAT3));
		}
	}
	//new2

	//mMaterials = new Material * [mNumMaterials];
	for (int i = 0; i < mNumMaterials; ++i) {
		vec4 dumy = 0;
		int namelen = 0;
		ifs.read((char*)&dumy, sizeof(int));
		char name[512] = {};
		ifs.read((char*)name, namelen * sizeof(char));
		name[namelen] = 0;

		ifs.read((char*)&dumy, sizeof(float) * 4);

		ifs.read((char*)&dumy, sizeof(float));

		float smoothness = 0;
		ifs.read((char*)&dumy, sizeof(float));

		ifs.read((char*)&dumy, sizeof(float));

		vec4 tiling, offset = 0;
		vec4 tiling2, offset2 = 0;
		ifs.read((char*)&tiling, sizeof(float) * 2);
		ifs.read((char*)&offset, sizeof(float) * 2);
		ifs.read((char*)&tiling2, sizeof(float) * 2);
		ifs.read((char*)&offset2, sizeof(float) * 2);

		bool isTransparent = false;
		ifs.read((char*)&isTransparent, sizeof(bool));

		bool emissive = 0;
		ifs.read((char*)&emissive, sizeof(bool));

		ifs.read((char*)&dumy, sizeof(int));
		ifs.read((char*)&dumy, sizeof(int));
		ifs.read((char*)&dumy, sizeof(int));
		ifs.read((char*)&dumy, sizeof(int));
		ifs.read((char*)&dumy, sizeof(int));
		ifs.read((char*)&dumy, sizeof(int));

		int diffuse2, normal2 = 0;
		ifs.read((char*)&diffuse2, sizeof(int));
		ifs.read((char*)&normal2, sizeof(int));
		gameworld.MaterialCount += 1;
	}

	RootNode = &Nodes[0];

	BakeAABB();
}

void Model::BakeAABB()
{
	RootNode->BakeAABB(this, XMMatrixIdentity());
	OBB_Tr = 0.5f * (AABB[0] + AABB[1]);
	OBB_Ext = AABB[1] - OBB_Tr;
}

BoundingOrientedBox Model::GetOBB() {
	return BoundingOrientedBox(OBB_Tr.f3, OBB_Ext.f3, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
}

void Mesh::GetAABBFromOBB(vec4* out, BoundingOrientedBox obb, bool first) {
	matrix mat;
	OBB_vertexVector ovv;
	mat.trQ(obb.Orientation);
	vec4 xm = -obb.Extents.x * mat.right;
	vec4 xp = obb.Extents.x * mat.right;
	vec4 ym = -obb.Extents.y * mat.up;
	vec4 yp = obb.Extents.y * mat.up;
	vec4 zm = -obb.Extents.z * mat.look;
	vec4 zp = obb.Extents.z * mat.look;

	vec4 pos = vec4(obb.Center);

	vec4 curr = xm + ym + zm + pos;
	if (first == false) {
		if (out[0].x > curr.x) out[0].x = curr.x;
		if (out[0].y > curr.y) out[0].y = curr.y;
		if (out[0].z > curr.z) out[0].z = curr.z;
		if (out[1].x < curr.x) out[1].x = curr.x;
		if (out[1].y < curr.y) out[1].y = curr.y;
		if (out[1].z < curr.z) out[1].z = curr.z;
	}
	else {
		out[0] = curr;
		out[1] = curr;
	}

	curr = xm + ym + zp + pos;
	if (out[0].x > curr.x) out[0].x = curr.x;
	if (out[0].y > curr.y) out[0].y = curr.y;
	if (out[0].z > curr.z) out[0].z = curr.z;
	if (out[1].x < curr.x) out[1].x = curr.x;
	if (out[1].y < curr.y) out[1].y = curr.y;
	if (out[1].z < curr.z) out[1].z = curr.z;

	curr = xm + yp + zm + pos;
	if (out[0].x > curr.x) out[0].x = curr.x;
	if (out[0].y > curr.y) out[0].y = curr.y;
	if (out[0].z > curr.z) out[0].z = curr.z;
	if (out[1].x < curr.x) out[1].x = curr.x;
	if (out[1].y < curr.y) out[1].y = curr.y;
	if (out[1].z < curr.z) out[1].z = curr.z;

	curr = xm + yp + zp + pos;
	if (out[0].x > curr.x) out[0].x = curr.x;
	if (out[0].y > curr.y) out[0].y = curr.y;
	if (out[0].z > curr.z) out[0].z = curr.z;
	if (out[1].x < curr.x) out[1].x = curr.x;
	if (out[1].y < curr.y) out[1].y = curr.y;
	if (out[1].z < curr.z) out[1].z = curr.z;

	curr = xp + ym + zm + pos;
	if (out[0].x > curr.x) out[0].x = curr.x;
	if (out[0].y > curr.y) out[0].y = curr.y;
	if (out[0].z > curr.z) out[0].z = curr.z;
	if (out[1].x < curr.x) out[1].x = curr.x;
	if (out[1].y < curr.y) out[1].y = curr.y;
	if (out[1].z < curr.z) out[1].z = curr.z;

	curr = xp + ym + zp + pos;
	if (out[0].x > curr.x) out[0].x = curr.x;
	if (out[0].y > curr.y) out[0].y = curr.y;
	if (out[0].z > curr.z) out[0].z = curr.z;
	if (out[1].x < curr.x) out[1].x = curr.x;
	if (out[1].y < curr.y) out[1].y = curr.y;
	if (out[1].z < curr.z) out[1].z = curr.z;

	curr = xp + yp + zm + pos;
	if (out[0].x > curr.x) out[0].x = curr.x;
	if (out[0].y > curr.y) out[0].y = curr.y;
	if (out[0].z > curr.z) out[0].z = curr.z;
	if (out[1].x < curr.x) out[1].x = curr.x;
	if (out[1].y < curr.y) out[1].y = curr.y;
	if (out[1].z < curr.z) out[1].z = curr.z;

	curr = xp + yp + zp + pos;
	if (out[0].x > curr.x) out[0].x = curr.x;
	if (out[0].y > curr.y) out[0].y = curr.y;
	if (out[0].z > curr.z) out[0].z = curr.z;
	if (out[1].x < curr.x) out[1].x = curr.x;
	if (out[1].y < curr.y) out[1].y = curr.y;
	if (out[1].z < curr.z) out[1].z = curr.z;
}

void ModelNode::BakeAABB(void* origin, const matrix& parentMat)
{
	Model* model = (Model*)origin;
	XMMATRIX sav = XMMatrixMultiply(transform, parentMat);
	if (numMesh != 0 && Meshes != nullptr) {
		matrix m = sav;
		vec4 AABB[2];

		BoundingOrientedBox obb = model->mMeshes[0]->GetOBB();
		BoundingOrientedBox obb_world;
		obb.Transform(obb_world, sav);

		Mesh::GetAABBFromOBB(AABB, obb_world, true);
		for (int i = 1; i < numMesh; ++i) {
			BoundingOrientedBox obb = model->mMeshes[i]->GetOBB();
			BoundingOrientedBox obb_world;
			obb.Transform(obb_world, sav);

			Mesh::GetAABBFromOBB(AABB, obb_world);
		}

		if (model->AABB[0].x > AABB[0].x) model->AABB[0].x = AABB[0].x;
		if (model->AABB[0].y > AABB[0].y) model->AABB[0].y = AABB[0].y;
		if (model->AABB[0].z > AABB[0].z) model->AABB[0].z = AABB[0].z;

		if (model->AABB[1].x < AABB[1].x) model->AABB[1].x = AABB[1].x;
		if (model->AABB[1].y < AABB[1].y) model->AABB[1].y = AABB[1].y;
		if (model->AABB[1].z < AABB[1].z) model->AABB[1].z = AABB[1].z;
	}

	for (int i = 0; i < numChildren; ++i) {
		ModelNode* node = Childrens[i];
		node->BakeAABB(origin, sav);
	}
}

void Player::Update(float deltaTime)
{
	weapon.Update(deltaTime);

	vec4 currentPos = worldMat.pos;
	XMMATRIX rotMat = XMMatrixRotationRollPitchYaw(0, m_yaw, 0);
	worldMat.mat = rotMat;
	worldMat.pos = currentPos;

	if (collideCount == 0) isGround = false;
	collideCount = 0;

	if (isGround == false) {
		LVelocity.y -= 9.8f * deltaTime;
	}

	XMFLOAT3 xmf3Shift = XMFLOAT3(0, 0, 0);
	constexpr float speed = 6.0f;

	if (isGround) {
		if (InputBuffer[InputID::KeyboardSpace]) {
			LVelocity.y = JumpVelocity;
			isGround = false;
		}
	}
	tickLVelocity = LVelocity * deltaTime;

	if (InputBuffer[InputID::KeyboardW] == true) {
		tickLVelocity += speed * worldMat.look * deltaTime;
	}
	if (InputBuffer[InputID::KeyboardS] == true) {
		tickLVelocity -= speed * worldMat.look * deltaTime;
	}
	if (InputBuffer[InputID::KeyboardA] == true) {
		tickLVelocity -= speed * worldMat.right * deltaTime;
	}
	if (InputBuffer[InputID::KeyboardD] == true) {
		tickLVelocity += speed * worldMat.right * deltaTime;
	}

	if (HealSkillCooldownFlow >= 0.0f) {
		HealSkillCooldownFlow -= deltaTime;
		gameworld.Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HealSkillCooldownFlow);
	}

	if (InputBuffer[InputID::KeyboardQ] == true) {
		std::cout << "[Player::Update] Q pressed!  HP=" << HP
			<< " Heat=" << HeatGauge
			<< " CD=" << HealSkillCooldownFlow << std::endl;

		if (HealSkillCooldownFlow <= 0.0f && HeatGauge > 0.0f && HP < MaxHP) {

			// 회복량 HeatGauge 전부를 HP로 전환
			float healAmount = HeatGauge;
			HP += healAmount;

			if (HP > MaxHP) HP = MaxHP;

			// 게이지 소모
			HeatGauge = 0.0f;

			// 쿨타임 리셋
			HealSkillCooldownFlow = HealSkillCooldown;

			// HP, HeatGauge 서버->클라 개인 동기화
			gameworld.Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HP);
			gameworld.Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HeatGauge);
		}
	}

	//if (InputBuffer[InputID::MouseLbutton] == true) {
	//	if (ShootFlow >= ShootDelay) {
	//		if (bFirstPersonVision) {
	//			matrix shootmat = ViewMatrix.RTInverse;
	//			gameworld.FireRaycast((GameObject*)this, shootmat.pos + shootmat.look, shootmat.look, 50.0f);
	//		}
	//		else {
	//			matrix shootmat = ViewMatrix.RTInverse;
	//			gameworld.FireRaycast((GameObject*)this, shootmat.pos + shootmat.look * 3, shootmat.look, 50.0f);
	//		}
	//		ShootFlow = 0;
	//		recoilFlow = 0;
	//	}
	//}

	XMVECTOR quaternion = XMQuaternionRotationRollPitchYaw(m_pitch, m_yaw, 0);
	vec4 clook = { 0, 0, 1, 0 };
	clook = XMVector3Rotate(clook, quaternion);

	vec4 peye = worldMat.pos;
	vec4 pat = worldMat.pos;

	if (bFirstPersonVision) {
		peye += 1.0f * worldMat.up;
		pat += 1.0f * worldMat.up;
		pat += 10.0f * clook;
	}
	else {
		peye -= 3.0f * clook;
		pat += 10.0f * clook;
		peye += 0.5f * worldMat.up;
		peye += 0.5f * worldMat.right;
	}

	XMVECTOR vUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	if (InputBuffer[InputID::MouseLbutton] == true) {
		if (weapon.m_shootFlow >= weapon.m_info.shootDelay && bullets > 0 && HeatGauge <= MaxHeatGauge) {

			bullets -= 1;
			HeatGauge += 2;

			weapon.OnFire();

			vec4 shootOrigin = worldMat.pos + vec4(0, 1.0f, 0, 0);
			gameworld.FireRaycast((GameObject*)this, shootOrigin + clook * 1.5f, clook, 50.0f, weapon.m_info.damage);

			// fix 이건 이렇게 하면 안될것 같은데? Update 될때마다 패킷이 쌓임. 
			// 패킷이 많아지면 서버에서 부담이므로 차라리 특정시간마다 쏴주는게 좋다.
			gameworld.Sending_ChangeGameObjectMember<int>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &bullets);
			gameworld.Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HeatGauge);
			gameworld.Sending_PlayerFire(gameworld.CommonSDS, gameworld.clients[clientIndex].objindex);

			if (bullets <= 0) {
				weapon.m_shootFlow = -weapon.m_info.reloadTime;

				bullets = weapon.m_info.maxBullets;

				gameworld.Sending_ChangeGameObjectMember<int>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &bullets);
			}
		}
	}

	//int datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &DeltaMousePos, 8);
	//gameworld.clients[clientIndex].socket.Send(gameworld.tempbuffer.data, datacap);

	BoundingOrientedBox obb = GetOBB();
	for (int i = 0; i < gameworld.DropedItems.size; ++i) {
		if (gameworld.DropedItems.isnull(i)) continue;
		vec4 p = gameworld.DropedItems[i].pos;
		p.w = 0;
		if (obb.Contains(p)) {
			bool isexist = false;
			bool beat = false;
			int firstBlackIndex = -1;
			for (int k = 0; k < Player::maxItem; ++k) {
				if (Inventory[k].id == 0 && firstBlackIndex == -1) {
					firstBlackIndex = k;
				}
				if (Inventory[k].id == gameworld.DropedItems[i].itemDrop.id)
				{
					Inventory[k].ItemCount += gameworld.DropedItems[i].itemDrop.ItemCount;
					isexist = true;
					beat = true;
					firstBlackIndex = k;
					break;
				}
			}
			if (isexist == false && firstBlackIndex != -1) {
				Inventory[firstBlackIndex] = gameworld.DropedItems[i].itemDrop;
				beat = true;
			}

			if (beat) {
				gameworld.Sending_InventoryItemSync(gameworld.clients[clientIndex].PersonalSDS, Inventory[firstBlackIndex], firstBlackIndex);
				gameworld.DropedItems.Free(i);
				gameworld.Sending_ItemRemove(gameworld.CommonSDS, i);
				break;
			}
		}
	}

	//히트 게이지 지속 감소 (발사 안할시)

	if (HeatGauge > 0.0f) {
		float decayRate = 5.0f; // 초당 감소량
		HeatGauge -= decayRate * deltaTime;
		if (HeatGauge < 0.0f) HeatGauge = 0.0f;
	}

	static float heatSendTimer = 0.0f;
	heatSendTimer += deltaTime;

	if (heatSendTimer > 0.2f) { // 0.2초마다 서버 → 클라 전송
		heatSendTimer = 0.0f;

		gameworld.Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS
			, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HeatGauge);
	}

}

void Player::OnCollision(GameObject* other)
{
	collideCount += 1;
	float belowDist = 0;
	BoundingOrientedBox otherOBB = other->GetOBB();

	bool belowhit = otherOBB.Intersects(worldMat.pos, vec4(0, -1, 0, 0), belowDist);

	if (belowhit && belowDist < Shape::ShapeTable[shapeindex].GetMesh()->GetOBB().Extents.y + 1.0f) {
		LVelocity.y = 0;
		isGround = true;
	}
}

void Player::OnStaticCollision(BoundingOrientedBox obb)
{
	collideCount += 1;
	float belowDist = 0;
	BoundingOrientedBox otherOBB = obb;

	bool belowhit = otherOBB.Intersects(worldMat.pos, vec4(0, -1, 0, 0), belowDist);

	if (belowhit && belowDist < Shape::ShapeTable[shapeindex].GetMesh()->GetOBB().Extents.y + 1.0f) {
		LVelocity.y = 0;
		isGround = true;
	}
}

BoundingOrientedBox Player::GetOBB()
{
	BoundingOrientedBox obb_local = Shape::ShapeTable[shapeindex].GetMesh()->GetOBB();
	obb_local.Extents.x = obb_local.Extents.z;
	BoundingOrientedBox obb_world;
	matrix id = XMMatrixIdentity();
	id.pos = worldMat.pos;
	obb_local.Transform(obb_world, id);
	return obb_world;
}

void Player::TakeDamage(float damage)
{
	HP -= damage;

	gameworld.Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HP);

	if (HP <= 0) {
		tag[GameObjectTag::Tag_Enable] = false;

		DeathCount += 1;
		gameworld.Sending_ChangeGameObjectMember<int>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &DeathCount);

		Respawn();
	}
}

void Player::OnCollisionRayWithBullet(GameObject* shooter, float damage)
{
	TakeDamage(damage);
}

void Player::Respawn() {
	HP = 100;
	tag[GameObjectTag::Tag_Enable] = true;

	worldMat.Id();
	worldMat.pos.y = 2;
	//player position send

	bool isExist = true;
	gameworld.Sending_ChangeGameObjectMember<Tag>(gameworld.CommonSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &tag);
	gameworld.Sending_ChangeGameObjectMember<float>(gameworld.CommonSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HP);
}

void Monster::Update(float deltaTime)
{
	if (isDead) {
		respawntimer += deltaTime;
		if (respawntimer > 1.0f) {
			Respawn();
			respawntimer = 0;
		}
	}
	else {
		if (collideCount == 0) isGround = false;
		collideCount = 0;

		if (isGround == false) {
			LVelocity.y -= 9.81f * deltaTime;
		}
		tickLVelocity = LVelocity * deltaTime;

		vec4 monsterPos = worldMat.pos;
		monsterPos.w = 0;

		if (Target == nullptr || (Target != nullptr && *Target == nullptr)) {
			int limitseek = 4;
			if (gameworld.clients.size == 0) {
				Target = nullptr;
				return;
			}

		SEEK_TARGET_LOOP:
			for (int i = targetSeekIndex; i < gameworld.clients.size; ++i) {
				limitseek -= 1;
				if (limitseek == 0) {
					return;
				}
				if (gameworld.clients.isnull(i)) continue;
				Target = (Player**)&gameworld.Dynamic_gameObjects[gameworld.clients[i].objindex];
				break;
			}
			if (limitseek != 0 && (Target == nullptr || *Target == nullptr)) {
				targetSeekIndex = 0;
				goto SEEK_TARGET_LOOP;
			}
		}

		vec4 playerPos = (*Target)->worldMat.pos;
		playerPos.w = 0;

		vec4 toPlayer = playerPos - monsterPos;
		toPlayer.y = 0.0f;
		float distanceToPlayer = toPlayer.len3;

		// 플레이어 추적
		if (distanceToPlayer <= m_chaseRange) {
			m_targetPos = playerPos;
			m_isMove = true;

			// A* 경로가 없거나, 다 소비했으면 새로 계산
			if (path.empty() || currentPathIndex >= path.size()) {
				AstarNode* start = FindClosestNode(monsterPos.x, monsterPos.z, gameworld.allnodes);
				AstarNode* goal = FindClosestNode(playerPos.x, playerPos.z, gameworld.allnodes);

				if (start && goal) {
					path = AstarSearch(start, goal, gameworld.allnodes);
					currentPathIndex = 0;
				}
			}

			// A* 경로가 있으면 그 경로를 따라 이동
			if (!path.empty() && currentPathIndex < path.size()) {
				MoveByAstar(deltaTime);
			}
			else {
				// 경로 계산 실패했을 때만 기존 직선 추적으로 fallback
				if (distanceToPlayer > 0.0001f) {
					toPlayer.len3 = 1.0f;
					tickLVelocity += toPlayer * m_speed * deltaTime;
					worldMat.SetLook(toPlayer);
				}
			}


			if (distanceToPlayer < 2.0f) {
				tickLVelocity.x = 0;
				tickLVelocity.z = 0;
			}
			m_fireTimer += deltaTime;
			if (m_fireTimer >= m_fireDelay) {
				m_fireTimer = 0.0f;

				vec4 rayStart = monsterPos + (worldMat.look * 0.5f);
				rayStart.y += 0.7f;
				vec4 rayDirection = playerPos - rayStart;

				float InverseAccurcy = distanceToPlayer / 6;
				rayDirection.x += (-1 + (float)(rand() & 255) / 128.0f) * InverseAccurcy;
				rayDirection.y += (-1 + (float)(rand() & 255) / 128.0f) * InverseAccurcy;
				rayDirection.z += (-1 + (float)(rand() & 255) / 128.0f) * InverseAccurcy;
				rayDirection.len3 = 1.0f;

				gameworld.FireRaycast(this, rayStart, rayDirection, m_chaseRange, 10);
			}
		}
		else {
			if (!m_isMove) {
				float randomAngle = ((float)rand() / RAND_MAX) * 2.0f * XM_PI;
				float randomRadius = ((float)rand() / RAND_MAX) * m_patrolRange;

				m_targetPos.x = m_homePos.x + randomRadius * cos(randomAngle);
				m_targetPos.z = m_homePos.z + randomRadius * sin(randomAngle);
				m_targetPos.y = m_homePos.y;

				m_isMove = true;
				m_patrolTimer = 0.0f;

				AstarNode* start = FindClosestNode(monsterPos.x, monsterPos.z, gameworld.allnodes);
				AstarNode* goal = FindClosestNode(m_targetPos.x, m_targetPos.z, gameworld.allnodes);

				if (start && goal) {
					path = AstarSearch(start, goal, gameworld.allnodes);
					currentPathIndex = 0;
				}
				else {
					path.clear();
					currentPathIndex = 0;
				}
			}

			m_patrolTimer += deltaTime;

			if (m_patrolTimer >= 5.0f) {
				tickLVelocity.x = 0;
				tickLVelocity.z = 0;
				m_isMove = false;
				path.clear();
				currentPathIndex = 0;
				return;
			}

			if (!path.empty() && currentPathIndex < path.size()) {
				MoveByAstar(deltaTime);

				if (currentPathIndex >= path.size()) {
					tickLVelocity.x = 0;
					tickLVelocity.z = 0;
					m_isMove = false;
					path.clear();
					currentPathIndex = 0;
				}
			}
			else {
				vec4 currentPos = worldMat.pos;
				currentPos.w = 0;
				vec4 direction = m_targetPos - currentPos;
				direction.y = 0.0f;
				float distance = direction.len3;

				if (distance > 1.0f) {
					direction.len3 = 1.0f;
					tickLVelocity += direction * m_speed * deltaTime;
					worldMat.SetLook(direction);
				}
				else {
					tickLVelocity.x = 0;
					tickLVelocity.z = 0;
					m_isMove = false;
				}
			}
		}

		if (gameworld.lowHit()) {
			gameworld.Sending_ChangeGameObjectMember<matrix>(gameworld.CommonSDS, gameworld.currentIndex, this, GameObjectType::_Monster, &(worldMat));
		}
	}
}

void Monster::OnCollision(GameObject* other)
{
	collideCount += 1;
	float belowDist = 0;
	BoundingOrientedBox otherOBB = other->GetOBB();
	bool belowhit = otherOBB.Intersects(worldMat.pos, vec4(0, -1, 0, 0), belowDist);
	if (belowhit && belowDist < GetOBB().Extents.y + 1.0f) {
		LVelocity.y = 0;
		isGround = true;
	}
}

void Monster::OnStaticCollision(BoundingOrientedBox obb)
{
	collideCount += 1;
	float belowDist = 0;
	BoundingOrientedBox otherOBB = obb;
	bool belowhit = otherOBB.Intersects(worldMat.pos, vec4(0, -1, 0, 0), belowDist);
	if (belowhit && belowDist < GetOBB().Extents.y + 1.0f) {
		LVelocity.y = 0;
		isGround = true;
	}
}

void Monster::OnCollisionRayWithBullet(GameObject* shooter, float damage)
{
	HP -= damage;
	gameworld.Sending_ChangeGameObjectMember<float>(gameworld.CommonSDS, gameworld.currentIndex, this, GameObjectType::_Monster, &HP);

	if (HP <= 0 && isDead == false) {
		isDead = true;
		gameworld.Sending_ChangeGameObjectMember<bool>(gameworld.CommonSDS, gameworld.currentIndex, this, GameObjectType::_Monster, &isDead);

		vec4 prevpos = worldMat.pos;

		// when monster is dead, player's killcount +1
		void* vptr = *(void**)shooter;
		if (GameObjectType::VptrToTypeTable[vptr] == GameObjectType::_Player) {
			Player* p = (Player*)shooter;
			p->KillCount += 1;
			gameworld.Sending_ChangeGameObjectMember<int>(gameworld.clients[p->clientIndex].PersonalSDS, gameworld.clients[p->clientIndex].objindex, p, GameObjectType::_Player, &p->KillCount);
		}

		//when monster is dead, loot random items;
		ItemLoot il = {};
		il.itemDrop.id = 1 + (rand() % (ItemTable.size() - 1));
		il.itemDrop.ItemCount = 1 + rand() % 5;
		il.pos = prevpos;
		int newindex = gameworld.DropedItems.Alloc();
		gameworld.DropedItems[newindex] = il;
		gameworld.Sending_ItemDrop(gameworld.CommonSDS, newindex, il);
	}
}

void Monster::Init(const XMMATRIX& initialWorldMatrix)
{
	worldMat = (initialWorldMatrix);
	m_homePos = worldMat.pos;
}

void Monster::Respawn()
{
	Init(XMMatrixTranslation(rand() % 80 - 40, 10.0f, rand() % 80 - 40));
	while (gameworld.map.isStaticCollision(GetOBB())) {
		Init(XMMatrixTranslation(rand() % 80 - 40, 10.0f, rand() % 80 - 40));
	}
	m_isMove = false;
	gameworld.Sending_ChangeGameObjectMember<vec4>(gameworld.CommonSDS, gameworld.currentIndex, this, GameObjectType::_Monster, &worldMat.pos);
	isDead = false;
	gameworld.Sending_ChangeGameObjectMember<bool>(gameworld.CommonSDS, gameworld.currentIndex, this, GameObjectType::_Monster, &isDead);
	HP = 30;
	gameworld.Sending_ChangeGameObjectMember<int>(gameworld.CommonSDS, gameworld.currentIndex, this, GameObjectType::_Monster, &HP);
}

BoundingOrientedBox Monster::GetOBB()
{
	BoundingOrientedBox obb_local = Shape::ShapeTable[shapeindex].GetModel()->GetOBB();
	obb_local.Extents.x = obb_local.Extents.z;
	BoundingOrientedBox obb_world;
	matrix id = XMMatrixIdentity();
	id.pos = worldMat.pos;
	obb_local.Transform(obb_world, id);
	return obb_world;
}


/*
<설명>
A*에서 현재 노드(current) 기준으로 갈 수 있는 이웃 노드를 찾아 반환한다.
8방향(상하좌우 + 대각선) 모두 검사한다.
맵 밖으로 나가는 노드는 제외한다.
이동 불가(cango==false) 노드는 제외한다.

매변 :
current : 이웃을 구하려는 기준 노드.
allNodes : 전체 노드.
gridWidth : 그리드 가로 크기.
gridHeight : 그리드 세로 크기.

return :
vector<AstarNode*> neighbor (이동 가능한 이웃 노드 목록)
if current가 가장자리 -> 범위 검사로 이웃 수가 줄어듦
if 주변 노드가 cango=false -> 목록에서 제외됨
*/
//AI involved Code Start : <chatgpt>
vector<AstarNode*> GetNeighbors(AstarNode* current, const std::vector<AstarNode*>& allNodes, int gridWidth, int gridHeight)
{
	std::vector<AstarNode*> neighbors;

	for (int dx = -1; dx <= 1; ++dx) {
		for (int dz = -1; dz <= 1; ++dz) {
			if (dx == 0 && dz == 0)
				continue;

			int nx = current->xIndex + dx;
			int nz = current->zIndex + dz;

			// 범위 밖이면 무시
			if (nx < 0 || nz < 0 || nx >= gridWidth || nz >= gridHeight)
				continue;

			// 인덱스로 노드 접근
			AstarNode* neighbor = allNodes[nz * gridWidth + nx];
			if (neighbor->cango)
				neighbors.push_back(neighbor);
		}
	}

	return neighbors;
}
//AI involved Code End : <chatgpt>

/* <설명>
- A* 알고리즘으로 start -> destination 최단 경로를 계산해 "노드 리스트(path)"를 반환한다.
- 노드마다 g/h/f 비용과 parent를 갱신하며, 목적지 도달 시 parent를 따라 역추적해 경로를 만든다.

매변 :
<start> : 시작 노드(몬스터 위치를 FindClosestNode로 스냅한 결과).
<destination> : 목적지 노드(플레이어/순찰 목표를 FindClosestNode로 스냅한 결과).
<allNodes> : 전체 노드 목록(매 탐색마다 노드 비용/parent 초기화에 사용).

return :
vector<AstarNode*> (start부터 destination까지의 경로 노드들)
if start == nullptr or destination == nullptr -> 빈 벡터 반환
if openList가 비어서도 destination에 도달 못함 -> 빈 벡터 반환(경로 없음)
if destination 도달 -> parent를 따라 역추적한 뒤 reverse하여 정상 순서 경로 반환

1) 모든 노드의 gCost를 무한대로, parent를 nullptr로 초기화한다(이전 탐색 흔적 제거).
2) start를 openList에 넣고 g=0, h=거리(휴리스틱), f=g+h를 계산한다.
3) openList에서 f가 가장 작은 노드를 current로 뽑는다.
4) current가 destination이면, current에서 parent를 따라가며 경로를 만들고 반환한다.
5) 아니면 current를 closedList로 옮긴다.
6) current의 이웃들을 가져오고, 더 좋은 경로(tentativeG)가 나오면 neighbor의 parent/g/h/f를 갱신한다.
7) openList가 빌 때까지 반복한다.
*/

//AI Code Start : <chatgpt>
vector<AstarNode*> Monster::AstarSearch(AstarNode* start, AstarNode* destination, std::vector<AstarNode*>& allNodes)
{

	std::vector<AstarNode*> openList;
	std::vector<AstarNode*> closedList;

	if (start == nullptr || destination == nullptr)
		return {};

	// 초기화
	for (auto node : allNodes)
	{
		node->gCost = FLT_MAX;
		node->hCost = 0.0f;
		node->fCost = 0.0f;
		node->parent = nullptr;
	}

	start->gCost = 0.0f;
	start->hCost = std::abs(start->xIndex - destination->xIndex) + std::abs(start->zIndex - destination->zIndex);
	start->fCost = start->gCost + start->hCost;

	openList.push_back(start);

	while (!openList.empty())
	{
		// fCost 최소 노드 선택
		AstarNode* currentNode = openList[0];
		for (AstarNode* node : openList)
		{
			if (node->fCost < currentNode->fCost)
				currentNode = node;
		}

		// 목적지 도달 시 경로 추적
		if (currentNode == destination)
		{
			std::vector<AstarNode*> path;
			AstarNode* pathNode = currentNode;
			while (pathNode != nullptr)
			{
				path.push_back(pathNode);
				pathNode = pathNode->parent;
			}
			std::reverse(path.begin(), path.end());
			return path;
		}

		// 오픈 리스트 → 클로즈드 리스트
		openList.erase(std::remove(openList.begin(), openList.end(), currentNode), openList.end());
		closedList.push_back(currentNode);

		// 이웃 탐색
		for (AstarNode* neighbor : GetNeighbors(currentNode, allNodes, 80, 80))
		{
			// 이미 방문한 노드는 스킵
			if (std::find(closedList.begin(), closedList.end(), neighbor) != closedList.end())
				continue;

			float costToNeighbor = ((currentNode->xIndex != neighbor->xIndex && currentNode->zIndex != neighbor->zIndex) ? 1.414f : 1.0f);
			float tentativeG = currentNode->gCost + costToNeighbor;

			if (tentativeG < neighbor->gCost)
			{
				neighbor->parent = currentNode;
				neighbor->gCost = tentativeG;
				neighbor->hCost = std::abs(neighbor->xIndex - destination->xIndex) + std::abs(neighbor->zIndex - destination->zIndex);
				neighbor->fCost = neighbor->gCost + neighbor->hCost;

				if (std::find(openList.begin(), openList.end(), neighbor) == openList.end())
					openList.push_back(neighbor);
			}
		}
	}

	return {}; // 경로 없음
}
//AI Code End : <chatgpt>

/*<설명>
AstarSearch로 만들어진 path에 맞춰 이동.
path[currentPathIndex] 노드 방향으로 이동한다.
목표 노드에 가까워지면 currentPathIndex++ 하여 다음 노드를 목표로 한다.

매변 :
deltaTime : speed*deltaTime로 이동량을 계산한다.

1) 현재 목표 노드 = path[currentPathIndex]를 잡는다.
2) 몬스터 위치에서 목표 노드 월드좌표(worldx, worldz)까지 방향 벡터(dir)를 만든 다.
3) 거의 도착했으면 다음 노드로 넘어간다.
4) 아니면 dir을 정규화하고, tickLVelocity = dir * m_speed * deltaTime로 이번 프레임 이동량을 만든다.
5) 바라보는 방향(look)을 dir로 맞춘다.
*/

//AI involved Code Start : <chatgpt>
void Monster::MoveByAstar(float deltaTime)
{
	if (path.empty() || currentPathIndex >= path.size())
		return;

	AstarNode* targetNode = path[currentPathIndex];

	// 현재 몬스터 위치
	vec4 pos = worldMat.pos;
	pos.w = 1.0f;

	// A*에서 지정한 타겟 노드 위치 (y는 현재 높이 유지)
	vec4 target(targetNode->worldx, pos.y, targetNode->worldz, 1.0f);
	if (gameworld.AstarStartX > target.x) target.x = gameworld.AstarStartX;
	if (gameworld.AstarStartZ > target.z) target.z = gameworld.AstarStartZ;
	if (gameworld.AstarEndX < target.x) target.x = gameworld.AstarEndX;
	if (gameworld.AstarEndZ < target.z) target.z = gameworld.AstarEndZ;

	// 방향 벡터
	vec4 dir = target - pos;
	dir.y = 0.0f;
	float len = dir.len3;

	// 거의 도착했다고 보면 다음 노드로 넘어감
	if (len < 0.3f) {
		currentPathIndex++;
		return;
	}

	// 정규화
	dir /= len;

	tickLVelocity.x = dir.x * m_speed * deltaTime;
	tickLVelocity.z = dir.z * m_speed * deltaTime;

	worldMat.SetLook(dir);
}
//AI involved Code End : <chatgpt>

/*<설명>
- 월드 좌표(wx, wz)에 가장 가까운 "이동 가능 노드(cango==true)"를 찾아 반환한다.
- 월드 좌표는 그리드 정중앙이 아닐 수 있으므로, A*의 start/goal 노드를 잡기 위한 함수.

매변 :
wx : 월드 X 좌표.
wz : 월드 Z 좌표.
allNodes : 전체 노드 목록(가장 가까운 노드 찾기 위해 전체 순회).

return :
if 이동 가능 노드가 하나도 없으면 -> nullptr 반환
if 여러 후보가 있으면 -> 거리^2(dx^2+dz^2)가 최소인 노드 반환

1) allNodes를 전부 돌며 cango==true인 노드만 본다.
2) (node.worldx - wx)^2 + (node.worldz - wz)^2 를 계산해 가장 작은 노드를 갱신한다.
3) 최종적으로 가장 가까운 노드를 반환.
*/
AstarNode* Monster::FindClosestNode(float wx, float wz, const std::vector<AstarNode*>& allNodes)
{
	AstarNode* best = nullptr;
	float bestDist2 = FLT_MAX;
	for (AstarNode* n : allNodes) {
		if (!n->cango) continue;
		float dx = n->worldx - wx;
		float dz = n->worldz - wz;
		float d2 = dx * dx + dz * dz;
		if (d2 < bestDist2) { bestDist2 = d2; best = n; }
	}
	return best;
}

BoundingBox ChunkIndex::GetAABB() {
	BoundingBox AABB;
	float halfW = gameworld.chunck_divide_Width * 0.5f;
	AABB.Center = XMFLOAT3(gameworld.chunck_divide_Width * x + halfW, gameworld.chunck_divide_Width * y + halfW, gameworld.chunck_divide_Width * z + halfW);
	AABB.Extents = XMFLOAT3(halfW, halfW, halfW);
	return AABB;
}

void GameChunk::SetChunkIndex(ChunkIndex ci) {
	cindex = ci;
	AABB = ci.GetAABB();
}

void World::Init() {
	ItemTable.push_back(Item(0)); // blank space in inventory.
	ItemTable.push_back(Item(1));
	ItemTable.push_back(Item(2));
	ItemTable.push_back(Item(3)); // test items. red, green, blue bullet mags.

	//astar grid init
	const int gridWidth = 80;
	const int gridHeight = 80;
	const float cellSize = 1.0f;

	const float offsetX = -gridWidth * cellSize * 0.5f;  // -40
	const float offsetZ = -gridHeight * cellSize * 0.5f; // -40

	allnodes.clear();
	allnodes.reserve(gridWidth * gridHeight);

	//Grid initiolaize
	for (int z = 0; z < gridHeight; ++z)
	{
		for (int x = 0; x < gridWidth; ++x)
		{
			AstarNode* node = new AstarNode();
			node->worldx = offsetX + x * cellSize + cellSize * 0.5f;
			node->worldz = offsetZ + z * cellSize + cellSize * 0.5f;
			node->xIndex = x;
			node->zIndex = z;
			node->gCost = 0;
			node->hCost = 0;
			node->fCost = 0;
			node->parent = nullptr;

			node->cango = true;

			allnodes.push_back(node);
		}
	}

	DropedItems.Init(4096);
	clients.Init(32);
	Dynamic_gameObjects.Init(128);

	GameObjectType::STATICINIT();

#ifdef _DEBUG //server / client
	AddClientOffset(GameObjectType::_GameObject, 16, 16); // isExist
	AddClientOffset(GameObjectType::_GameObject, 32, 32); // world Matrix
	AddClientOffset(GameObjectType::_GameObject, 80, 144); // pos

	AddClientOffset(GameObjectType::_Player, 16, 16); // isExist
	AddClientOffset(GameObjectType::_Player, 32, 32); // world Matrix
	AddClientOffset(GameObjectType::_Player, 64, 144); // pos

	AddClientOffset(GameObjectType::_Player, 164, 212); // m_currentWeaponType
	AddClientOffset(GameObjectType::_Player, 128, 176); // HP
	AddClientOffset(GameObjectType::_Player, 132, 180); // MaxHP

	AddClientOffset(GameObjectType::_Player, 136, 184); // Bullets
	AddClientOffset(GameObjectType::_Player, 140, 188); // KillCount
	AddClientOffset(GameObjectType::_Player, 144, 192); // DeathCount
	AddClientOffset(GameObjectType::_Player, 148, 196); // HeatGauge
	AddClientOffset(GameObjectType::_Player, 152, 200); // MaxHeatGauge
	AddClientOffset(GameObjectType::_Player, 156, 204); // HealSkillCooldown
	AddClientOffset(GameObjectType::_Player, 160, 208); // HealSkillCooldownFlow

	AddClientOffset(GameObjectType::_Monster, 16, 16); // isExist
	AddClientOffset(GameObjectType::_Monster, 32, 32); // world Matrix
	AddClientOffset(GameObjectType::_Monster, 80, 144); // pos
	AddClientOffset(GameObjectType::_Monster, 210, 176); // isDead
	AddClientOffset(GameObjectType::_Monster, 184, 180); // HP
	AddClientOffset(GameObjectType::_Monster, 188, 184); // MaxHP
#else
	AddClientOffset(GameObjectType::_GameObject, 16, 16); // isExist
	AddClientOffset(GameObjectType::_GameObject, 32, 32); // world Matrix
	AddClientOffset(GameObjectType::_GameObject, 80, 144); // pos

	AddClientOffset(GameObjectType::_Player, 16, 16); // isExist
	AddClientOffset(GameObjectType::_Player, 32, 32); // world Matrix
	AddClientOffset(GameObjectType::_Player, 64, 144); // pos

	AddClientOffset(GameObjectType::_Player, 164, 212); // m_currentWeaponType
	AddClientOffset(GameObjectType::_Player, 128, 176); // HP
	AddClientOffset(GameObjectType::_Player, 132, 180); // MaxHP

	AddClientOffset(GameObjectType::_Player, 136, 184); // Bullets
	AddClientOffset(GameObjectType::_Player, 140, 188); // KillCount
	AddClientOffset(GameObjectType::_Player, 144, 192); // DeathCount
	AddClientOffset(GameObjectType::_Player, 148, 196); // HeatGauge
	AddClientOffset(GameObjectType::_Player, 152, 200); // MaxHeatGauge
	AddClientOffset(GameObjectType::_Player, 156, 204); // HealSkillCooldown
	AddClientOffset(GameObjectType::_Player, 160, 208); // HealSkillCooldownFlow

	AddClientOffset(GameObjectType::_Monster, 16, 16); // isExist
	AddClientOffset(GameObjectType::_Monster, 32, 32); // world Matrix
	AddClientOffset(GameObjectType::_Monster, 80, 144); // pos
	AddClientOffset(GameObjectType::_Monster, 210, 176); // isDead
	AddClientOffset(GameObjectType::_Monster, 184, 180); // HP
	AddClientOffset(GameObjectType::_Monster, 188, 184); // MaxHP
#endif

	map.LoadMap("The_Port");
	std::set<StaticGameObject*> goset;
	for (int i = 0; i < map.MapObjects.size(); ++i) {
		PushGameObject(map.MapObjects[i]);
	}

	//bulletRays.Init(32);

	HumanoidAnimation hanim;
	hanim.LoadHumanoidAnimation("Resources/Animation/BreakDance1990.Humanoid_animation");
	HumanoidAnimationTable.push_back(hanim);

	Model* PlayerModel = new Model();
	PlayerModel->LoadModelFile2("Resources/Model/Remy.model");
	//PlayerModel->Retargeting_Humanoid(); // 휴머노이드 리타겟팅
	int playerMesh_index = Shape::AddModel("Player", PlayerModel);

	Model* MonsterModel = new Model();
	MonsterModel->LoadModelFile2("Resources/Model/Remy.model");
	int monsterMesh_index = Shape::AddModel("Monster001", MonsterModel);

	int newindex = 0;
	int datacap = 0;

	for (int i = 0; i < 20; ++i) {
		Monster* myMonster_1 = new Monster();
		myMonster_1->SetShape(monsterMesh_index);
		//myMonster_1->mesh = (MyMonsterMesh);

		myMonster_1->Init(XMMatrixTranslation(rand() % 80 - 40, 20.0f, rand() % 80 - 40));
		while (gameworld.map.isStaticCollision(myMonster_1->GetOBB())) {
			myMonster_1->Init(XMMatrixTranslation(rand() % 80 - 40, 10.0f, rand() % 80 - 40));
		}

		newindex = NewObject((DynamicGameObject*)myMonster_1, GameObjectType::_Monster);
	}

	//그리드(노드)의 cango를 체크하는 함수
	gridcollisioncheck();

	const int GRID_W = 80;
	const int GRID_H = 80;

	//PrintCangoSummary(allnodes, GRID_W, GRID_H);
	PrintCangoGrid(allnodes, GRID_W, GRID_H);

	cout << "Game Init end" << endl;
}

void World::Update() {
	lowFrequencyFlow += DeltaTime;
	midFrequencyFlow += DeltaTime;
	highFrequencyFlow += DeltaTime;

	for (currentIndex = 0; currentIndex < Dynamic_gameObjects.size; ++currentIndex) {
		if (Dynamic_gameObjects.isnull(currentIndex)) continue;
		if (Dynamic_gameObjects[currentIndex]->tag[GameObjectTag::Tag_Enable] == false) {
			continue;
		}
		Dynamic_gameObjects[currentIndex]->Update(DeltaTime);
	}
	//delete 한거를 업데이트해서
	// Collision......

	//bool bFixed = false;
	for (int i = 0; i < Dynamic_gameObjects.size; ++i) {
		if (Dynamic_gameObjects.isnull(i)) continue;
		DynamicGameObject* gbj1 = Dynamic_gameObjects[i];
		//if (gbj1->tickLVelocity.fast_square_of_len3 <= 0.001f) bFixed = true;

		Shape s1 = Shape::ShapeTable[gbj1->shapeindex];
		float fsl1 = 0;
		if (s1.isMesh()) {
			fsl1 = s1.GetMesh()->MAXpos.fast_square_of_len3;
		}
		else fsl1 = s1.GetModel()->OBB_Ext.fast_square_of_len3;

		vec4 lastpos1 = gbj1->worldMat.pos + gbj1->tickLVelocity;

		for (int j = i + 1; j < Dynamic_gameObjects.size; ++j) {
			if (Dynamic_gameObjects.isnull(j)) continue;
			DynamicGameObject* gbj2 = Dynamic_gameObjects[j];
			Shape s2 = Shape::ShapeTable[gbj2->shapeindex];

			vec4 Ext2 = 0;
			if (s1.isMesh()) {
				Ext2 = s2.GetMesh()->MAXpos;
			}
			else Ext2 = s2.GetModel()->OBB_Ext;

			vec4 dist = lastpos1 - (gbj2->worldMat.pos + gbj2->tickLVelocity);
			//if (gbj2->tickLVelocity.fast_square_of_len3 <= 0.001f && bFixed) continue;

			if (fsl1 + Ext2.fast_square_of_len3 > dist.fast_square_of_len3) {
				DynamicGameObject::CollisionMove(gbj1, gbj2);
			}
			//GameObject::CollisionMove(gbj1, gbj2);
		}

		map.StaticCollisionMove(gbj1);

		//gbj1->tickLVelocity.w = 0;
		if (gbj1->tickLVelocity.fast_square_of_len3 <= 0.001f) continue;

		gbj1->worldMat.pos += gbj1->tickLVelocity;
		/*if (fabsf(gbj1->m_worldMatrix.pos.x) > 40.0f || fabsf(gbj1->m_worldMatrix.pos.z) > 40.0f) {
			gbj1->m_worldMatrix.pos -= gbj1->tickLVelocity;
		}*/
		gbj1->tickLVelocity = XMVectorZero();

		// send matrix to client
		Sending_ChangeGameObjectMember<matrix>(gameworld.CommonSDS, i, gbj1, GameObjectType::_GameObject, &gbj1->worldMat);
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

void World::gridcollisioncheck()
{
	const float radius = 0.2f;
	const float groundY = 0.0f;

	for (AstarNode* node : allnodes)
	{
		vec4 nodePos(node->worldx, groundY, node->worldz);
		collisionchecksphere sphere = { nodePos, radius };

		bool blocked = false;

		// 모든 오브젝트 중 "Wall001" 메쉬만 검사
		for (int i = 0; i < Dynamic_gameObjects.size; ++i)
		{
			if (Dynamic_gameObjects.isnull(i)) continue;
			DynamicGameObject* obj = Dynamic_gameObjects[i];

			// 벽 메쉬 식별
			if (obj->shapeindex == Shape::StrToShapeIndex["Wall001"])
			{
				vec4 wallCenter(obj->worldMat.pos.x,
					obj->worldMat.pos.y,
					obj->worldMat.pos.z);

				// Wall001이 5x2x1로 생성되었다면 절반 크기 = {2.5, 1.0, 0.5}
				vec4 wallHalfSize(2.5f, 1.0f, 0.5f);

				if (CheckAABBSphereCollision(wallCenter, wallHalfSize, sphere))
				{
					blocked = true;
					break;
				}
			}
		}
		node->cango = !blocked;
	}
}

int World::NewObject(DynamicGameObject* obj, GameObjectType gotype)
{
	int newindex = Dynamic_gameObjects.Alloc();
	Dynamic_gameObjects[newindex] = obj;
	obj->tag[GameObjectTag::Tag_Enable] = true;

	Sending_NewGameObject(CommonSDS, newindex, obj);
	return newindex;
}

int World::NewPlayer(SendDataSaver& sds, Player* obj, int clientIndex)
{
	int newindex = Dynamic_gameObjects.Alloc();
	Dynamic_gameObjects[newindex] = (DynamicGameObject*)obj;
	obj->tag[GameObjectTag::Tag_Enable] = true;

	Sending_NewGameObject(CommonSDS, newindex, obj);
	return newindex;
}

void World::Sending_NewGameObject(SendDataSaver& sds, int newindex, GameObject* newobj) {
	newobj->SendGameObject(newindex, sds);
}

void World::Sending_NewRay(SendDataSaver& sds, vec4 rayStart, vec4 rayDirection, float rayDistance) {
	sds.postpush_start();
	constexpr int reqsiz = sizeof(STC_NewRay_Header);
	sds.postpush_reserve(reqsiz);
	STC_NewRay_Header& header = *(STC_NewRay_Header*)sds.ofbuff;
	header.size = reqsiz;
	header.st = SendingType::NewRay;
	header.raystart = rayStart.f3;
	header.rayDir = rayDirection.f3;
	header.distance = rayDistance;
	sds.postpush_end();
}

void World::Sending_DeleteGameObject(SendDataSaver& sds, int objindex)
{
	sds.postpush_start();
	constexpr int reqsiz = sizeof(STC_DeleteGameObject_Header);
	sds.postpush_reserve(reqsiz);
	STC_DeleteGameObject_Header& header = *(STC_DeleteGameObject_Header*)sds.ofbuff;
	header.size = reqsiz;
	header.st = SendingType::DeleteGameObject;
	header.obj_index = objindex;
	sds.postpush_end();
}

void World::Sending_ItemDrop(SendDataSaver& sds, int dropindex, ItemLoot lootdata)
{
	sds.postpush_start();
	constexpr int reqsiz = sizeof(STC_ItemDrop_Header);
	sds.postpush_reserve(reqsiz);
	STC_ItemDrop_Header& header = *(STC_ItemDrop_Header*)sds.ofbuff;
	header.size = reqsiz;
	header.st = SendingType::ItemDrop;
	header.dropindex = dropindex;
	header.lootData = lootdata;
	sds.postpush_end();
}

void World::Sending_ItemRemove(SendDataSaver& sds, int dropindex)
{
	sds.postpush_start();
	constexpr int reqsiz = sizeof(STC_ItemDropRemove_Header);
	sds.postpush_reserve(reqsiz);
	STC_ItemDropRemove_Header& header = *(STC_ItemDropRemove_Header*)sds.ofbuff;
	header.size = reqsiz;
	header.st = SendingType::ItemDropRemove;
	header.dropindex = dropindex;
	sds.postpush_end();
}

void World::Sending_InventoryItemSync(SendDataSaver& sds, ItemStack lootdata, int inventoryIndex)
{
	sds.postpush_start();
	constexpr int reqsiz = sizeof(STC_InventoryItemSync_Header);
	sds.postpush_reserve(reqsiz);
	STC_InventoryItemSync_Header& header = *(STC_InventoryItemSync_Header*)sds.ofbuff;
	header.size = reqsiz;
	header.st = SendingType::InventoryItemSync;
	header.Iteminfo = lootdata;
	header.inventoryIndex = inventoryIndex;
	sds.postpush_end();
}

void World::Sending_PlayerFire(SendDataSaver& sds, int objIndex) {
	sds.postpush_start();
	constexpr int reqsiz = sizeof(STC_PlayerFire_Header);
	sds.postpush_reserve(reqsiz);
	STC_PlayerFire_Header& header = *(STC_PlayerFire_Header*)sds.ofbuff;
	header.size = reqsiz;
	header.st = SendingType::PlayerFire;
	header.objindex = objIndex;
	sds.postpush_end();
}

//void World::DestroyObject(int objindex)
//{
//	if (gameObjects.isnull(objindex)) return;
//
//	int datacap = Sending_DeleteGameObject(objindex);
//	SendToAllClient(datacap);
//
//	delete gameObjects[objindex];
//	gameObjects.Free(objindex);
//}
//int World::

//should i separate player delete and gameobject delete?

void World::FireRaycast(GameObject* shooter, vec4 rayStart, vec4 rayDirection, float rayDistance, float damage)
{
	vec4 rayOrigin = rayStart;
	DynamicGameObject* closestHitObject = nullptr;
	float closestDistance = rayDistance;
	int lastcurrentindex = gameworld.currentIndex;

	for (int i = 0; i < Dynamic_gameObjects.size; ++i) {
		if (Dynamic_gameObjects.isnull(i) || shooter == Dynamic_gameObjects[i]) continue;

		BoundingOrientedBox obb = Dynamic_gameObjects[i]->GetOBB();
		float distance;

		if (obb.Intersects(rayOrigin, rayDirection, distance)) {
			if (distance < closestDistance) {
				closestDistance = distance;
				closestHitObject = Dynamic_gameObjects[i];
			}
		}
	}

	if (closestHitObject != nullptr) {
		closestHitObject->OnCollisionRayWithBullet(shooter, damage);
		//std::cout << "Hit! Object Distance: " << closestDistance << std::endl;
	}

	Sending_NewRay(CommonSDS, rayStart, rayDirection, closestDistance);
}

GameObjectIncludeChunks World::GetChunks_Include_OBB(BoundingOrientedBox obb) {
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

GameChunk* World::GetChunkFromPos(vec4 pos) {
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

void World::PushGameObject(GameObject* go) {
	if (go->tag[GameObjectTag::Tag_Dynamic] == false) {
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
		if (go->tag[GameObjectTag::Tag_SkinMeshObject] == true) {
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

//그리드 그리기 함수
void World::PrintCangoGrid(const std::vector<AstarNode*>& all, int gridWidth, int gridHeight)
{
	std::cout << "\n=== CANGO GRID (.: walkable, #: blocked) ===\n";
	for (int z = 0; z < gridHeight; ++z) {
		for (int x = 0; x < gridWidth; ++x) {
			const AstarNode* n = all[z * gridWidth + x];
			std::cout << (n && n->cango ? '.' : '#');
		}
		std::cout << '\n';
	}
	std::cout << std::flush;
}

void World::PrintOffset() {
	ofstream ofs{ "STC_GameObjectOffsets.txt" };
	ofs << GameObject::g_members.size() << endl;
	GameObject::PrintOffset(ofs);
	ofs << GameObject::g_members.size() + StaticGameObject::g_members.size() << endl;
	StaticGameObject::PrintOffset(ofs);
	ofs << GameObject::g_members.size() + DynamicGameObject::g_members.size() << endl;
	DynamicGameObject::PrintOffset(ofs);
	ofs << GameObject::g_members.size() + DynamicGameObject::g_members.size() + SkinMeshGameObject::g_members.size() << endl;
	SkinMeshGameObject::PrintOffset(ofs);
	ofs << GameObject::g_members.size() + DynamicGameObject::g_members.size() + SkinMeshGameObject::g_members.size() + Player::g_members.size() << endl;
	Player::PrintOffset(ofs);
	ofs << GameObject::g_members.size() + DynamicGameObject::g_members.size() + SkinMeshGameObject::g_members.size() + Monster::g_members.size() << endl;
	Monster::PrintOffset(ofs);
	ofs.close();
}

bool World::CheckAABBSphereCollision(const vec4& boxCenter, const vec4& boxHalfSize, const collisionchecksphere& sphere)
{
	float x = max(boxCenter.x - boxHalfSize.x, min(sphere.center.x, boxCenter.x + boxHalfSize.x));
	float y = max(boxCenter.y - boxHalfSize.y, min(sphere.center.y, boxCenter.y + boxHalfSize.y));
	float z = max(boxCenter.z - boxHalfSize.z, min(sphere.center.z, boxCenter.z + boxHalfSize.z));

	float dx = sphere.center.x - x;
	float dy = sphere.center.y - y;
	float dz = sphere.center.z - z;

	float dist2 = dx * dx + dy * dy + dz * dz;
	return dist2 < (sphere.radius * sphere.radius);
}

void ClientData::DisconnectToServer(int index) {
	cout << "client " << index << " Left the Game. \n";
	int objindex = gameworld.clients[index].objindex;
	delete gameworld.clients[index].pObjData;
	gameworld.clients[index].pObjData = nullptr;
	gameworld.Dynamic_gameObjects[objindex] = nullptr;
	gameworld.Dynamic_gameObjects.Free(objindex);
	gameworld.clients.Free(index);
}

void HumanoidAnimation::LoadHumanoidAnimation(string filename) {
	ifstream ifs{ filename, ios_base::binary };
	Duration = 0;
	for (int i = 0; i < 55; ++i) {
		int posKeyNum = 0;
		int rotKeyNum = 0;
		int scaleKeyNum = 0;
		ifs.read((char*)&posKeyNum, sizeof(int));
		ifs.read((char*)&rotKeyNum, sizeof(int));
		ifs.read((char*)&scaleKeyNum, sizeof(int));
		double time;
		vec4 dumy = 0;
		for (int k = 0; k < posKeyNum; ++k) {
			ifs.read((char*)&time, sizeof(double));
			ifs.read((char*)&dumy, sizeof(XMFLOAT3));
			Duration = max(time, Duration);
		}
		for (int k = 0; k < rotKeyNum; ++k) {
			ifs.read((char*)&time, sizeof(double));
			ifs.read((char*)&dumy, sizeof(XMFLOAT4));
			Duration = max(time, Duration);
		}
		for (int k = 0; k < scaleKeyNum; ++k) {
			ifs.read((char*)&time, sizeof(double));
			ifs.read((char*)&dumy, sizeof(XMFLOAT3));
			Duration = max(time, Duration);
		}
	}
}
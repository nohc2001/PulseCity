#include "stdafx.h"
#include "GameObject.h"
#include <set>
using namespace std;

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
	GameObjectType::VptrToTypeTable.insert(pair<void*, GameObjectType>(GetVptr<StaticGameObject>(), GameObjectType::_StaticGameObject));
	GameObjectType::VptrToTypeTable.insert(pair<void*, GameObjectType>(GetVptr<DynamicGameObject>(), GameObjectType::_DynamicGameObject));
	GameObjectType::VptrToTypeTable.insert(pair<void*, GameObjectType>(GetVptr<SkinMeshGameObject>(), GameObjectType::_SkinMeshGameObject));
	GameObjectType::VptrToTypeTable.insert(pair<void*, GameObjectType>(GetVptr<Player>(), GameObjectType::_Player));
	GameObjectType::VptrToTypeTable.insert(pair<void*, GameObjectType>(GetVptr<Monster>(), GameObjectType::_Monster));
	GameObjectType::VptrToTypeTable.insert(pair<void*, GameObjectType>(GetVptr<Portal>(), GameObjectType::_Portal));
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
	auto it = StrToShapeIndex.find(name);
	if (it != StrToShapeIndex.end()) {
		return it->second;
	}
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
	auto it = StrToShapeIndex.find(name);
	if (it != StrToShapeIndex.end()) {
		return it->second;
	}
	Shape s;
	s.SetModel(ptr);
	int index = ShapeTable.size();
	ShapeTable.push_back(s);
	ShapeStrTable.push_back(name);
	StrToShapeIndex.insert(pair<string, int>(name, index));
	return index;
}

int Shape::AddMeshInZone(string name, Mesh* ptr, int zoneid)
{
	Zone* zone = &gameworld.zones[zoneid];
	auto it = StrToShapeIndex.find(name);
	if (it != StrToShapeIndex.end()) {
		return it->second;
	}
	Shape s;
	s.SetMesh(ptr);
	int index = zone->ZoneShapeTable.size();
	zone->ZoneShapeTable.push_back(s);
	zone->ZoneShapeStrTable.push_back(name);
	zone->ZoneStrToShapeIndex.insert(pair<string, int>(name, index));
	return index;
}

int Shape::AddModelInZone(string name, Model* ptr, int zoneid)
{
	Zone* zone = &gameworld.zones[zoneid];
	auto it = StrToShapeIndex.find(name);
	if (it != StrToShapeIndex.end()) {
		return it->second;
	}
	Shape s;
	s.SetModel(ptr);
	int index = zone->ZoneShapeTable.size();
	zone->ZoneShapeTable.push_back(s);
	zone->ZoneShapeStrTable.push_back(name);
	zone->ZoneStrToShapeIndex.insert(pair<string, int>(name, index));
	return index;
}

GameObject::GameObject()
{
	tag = 0;
	tag[GameObjectTag::Tag_Enable] = false;
	tag[GameObjectTag::Tag_Dynamic] = false;
	tag[GameObjectTag::Tag_SkinMeshObject] = false;
	worldMat.Id();
	shapeindex = -1;
	parent = -1;
	childs = -1;
	sibling = -1;
	material = nullptr;
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
	Zone* zone = &gameworld.zones[zoneId];
	zone->GetShape(shapeindex).GetRealShape(mesh, model);
	if (mesh != nullptr) obb_local = mesh->GetOBB();
	if (model != nullptr) obb_local = model->GetOBB();
	BoundingOrientedBox obb_world;
	obb_local.Transform(obb_world, worldMat);
	return obb_world;
}

void GameObject::SetShape(int _shapeindex)
{
	shapeindex = _shapeindex;
	Zone* zone = &gameworld.zones[zoneId];
	Shape& s = zone->GetShape(shapeindex);
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

void GameObject::SendGameObject(int objindex, SendDataSaver& sds) {
	sds.postpush_start();

	//calculate app packet siz.
	int reqsiz = sizeof(STC_SyncGameObject_Header) + sizeof(STC_SyncObjData);

	Mesh* mesh = nullptr;
	Model* model = nullptr;
	Zone* zone = &gameworld.zones[zoneId];
	if (shapeindex >= 0 && shapeindex < Shape::ShapeTable.size() + zone->ZoneShapeTable.size()) {
		Shape& s = zone->GetShape(shapeindex);
		s.GetRealShape(mesh, model);
		if (mesh) reqsiz += sizeof(int) + sizeof(int) * mesh->subMeshNum;
		else if (model) reqsiz += sizeof(int) + sizeof(matrix) * model->nodeCount;
	}
	sds.postpush_reserve(reqsiz);
	int offset = 0;

	//static pushv
	STC_SyncGameObject_Header& header = *(STC_SyncGameObject_Header*)sds.ofbuff;
	header.size = reqsiz;
	header.st = STC_Protocol::SyncGameObject;
	header.objindex = objindex;
	header.type = GameObjectType::_GameObject;
	offset += sizeof(STC_SyncGameObject_Header);

	STC_SyncObjData& static_data = *(STC_SyncObjData*)(sds.ofbuff + offset);
	static_data.tag = tag;
	static_data.shapeindex = shapeindex;
	static_data.parent = parent;
	static_data.childs = childs;
	static_data.sibling = sibling;
	static_data.worldMatrix = worldMat;
	offset += sizeof(STC_SyncObjData);

	//dynamic push
	if (mesh) {
		int& submeshNum = *(int*)(sds.ofbuff + offset);
		submeshNum = mesh->subMeshNum;
		offset += sizeof(int);
		memcpy(sds.ofbuff + offset, material, sizeof(int) * mesh->subMeshNum);
		offset += sizeof(int) * mesh->subMeshNum;
	}
	else if (model) {
		int& nodecount = *(int*)(sds.ofbuff + offset);
		nodecount = model->nodeCount;
		offset += sizeof(int);
		memcpy(sds.ofbuff + offset, transforms_innerModel, sizeof(matrix) * model->nodeCount);
		offset += sizeof(matrix) * model->nodeCount;
	}

	sds.postpush_end();
}

StaticGameObject::StaticGameObject()
{
	tag = 0;
	tag[GameObjectTag::Tag_Enable] = false;
	tag[GameObjectTag::Tag_Dynamic] = false;
}

StaticGameObject::~StaticGameObject()
{
	obbArr.clear();
}

matrix StaticGameObject::GetWorld() {
	return worldMat;
}

void StaticGameObject::SetWorld(matrix localWorldMat)
{
	Zone* zone = &gameworld.zones[zoneId];
	matrix sav = localWorldMat;
	int temp = parent;
	StaticGameObject* obj = nullptr;
	if (temp > 0) {
		obj = zone->map.MapObjects[temp];
	}
	
	if (obj != nullptr) {
		sav *= obj->worldMat;
		temp = obj->parent;
		obj = nullptr;
		if (temp > 0) {
			obj = zone->map.MapObjects[temp];
		}
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
	Zone* zone = &gameworld.zones[zoneId];
	zone->GetShape(shapeindex).GetRealShape(mesh, model);
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

void StaticGameObject::SendGameObject(int objindex, SendDataSaver& sds) {
	sds.postpush_start();

	//calculate app packet siz.
	int reqsiz = sizeof(STC_SyncGameObject_Header) + sizeof(STC_SyncObjData);
	Zone zone = gameworld.zones[zoneId];
	Shape& s = zone.GetShape(shapeindex);
	Mesh* mesh = nullptr;
	Model* model = nullptr;
	s.GetRealShape(mesh, model);
	if (mesh) reqsiz += sizeof(int) + sizeof(int) * mesh->subMeshNum;
	else reqsiz += sizeof(int) + sizeof(matrix) * model->nodeCount;
	reqsiz += sizeof(int) + obbArr.size() * (sizeof(XMFLOAT3) * 2 + sizeof(XMFLOAT4));

	sds.postpush_reserve(reqsiz);
	int offset = 0;

	//static pushv
	STC_SyncGameObject_Header& header = *(STC_SyncGameObject_Header*)sds.ofbuff;
	header.size = reqsiz;
	header.st = STC_Protocol::SyncGameObject;
	header.objindex = objindex;
	header.type = GameObjectType::_StaticGameObject;
	offset += sizeof(STC_SyncGameObject_Header);

	STC_SyncObjData& static_data = *(STC_SyncObjData*)(sds.ofbuff + offset);
	static_data.tag = tag;
	static_data.shapeindex = shapeindex;
	static_data.parent = parent;
	static_data.childs = childs;
	static_data.sibling = sibling;
	static_data.worldMatrix = worldMat;
	offset += sizeof(STC_SyncObjData);

	//dynamic push
	if (mesh) {
		int& submeshNum = *(int*)(sds.ofbuff + offset);
		submeshNum = mesh->subMeshNum;
		offset += sizeof(int);

		int*& MaterialArr = *(int**)(sds.ofbuff + offset);
		memcpy(MaterialArr, material, sizeof(int) * mesh->subMeshNum);
		offset += sizeof(int) * mesh->subMeshNum;
	}
	else {
		int& nodecount = *(int*)(sds.ofbuff + offset);
		nodecount = model->nodeCount;
		offset += sizeof(int);

		matrix* InnerModelTransformArr = (matrix*)(sds.ofbuff + offset);
		memcpy(InnerModelTransformArr, transforms_innerModel, sizeof(matrix) * model->nodeCount);
		offset += sizeof(matrix) * model->nodeCount;
	}

	int& aabbCount = *(int*)(sds.ofbuff + offset);
	aabbCount = obbArr.size();
	for (int i = 0; i < obbArr.size(); ++i) {
		XMFLOAT3& Center = *(XMFLOAT3*)(sds.ofbuff + offset);
		Center = obbArr[i].Center;
		offset += sizeof(XMFLOAT3);

		XMFLOAT3& Extents = *(XMFLOAT3*)(sds.ofbuff + offset);
		Extents = obbArr[i].Extents;
		offset += sizeof(XMFLOAT3);

		XMFLOAT4& Orientation = *(XMFLOAT4*)(sds.ofbuff + offset);
		Orientation = obbArr[i].Orientation;
		offset += sizeof(XMFLOAT3);
	}
	sds.postpush_end();
}

void StaticGameObject::Release() {
	GameObject::Release();
}

DynamicGameObject::DynamicGameObject() :
	chunkAllocIndexs{ nullptr }
{
	tag = 0;
	tag[GameObjectTag::Tag_Enable] = false;
	tag[GameObjectTag::Tag_Dynamic] = true;
	tag[GameObjectTag::Tag_SkinMeshObject] = false;
	worldMat.Id();
	shapeindex = -1;

	tickLVelocity = vec4(0, 0, 0, 0);
	tickAVelocity = vec4(0, 0, 0, 0);
	LastQuerternion = vec4(0, 0, 0, 0);
	chunkAllocIndexsCapacity = 0;
	ZeroMemory(&IncludeChunks, sizeof(GameObjectIncludeChunks));
	chunkAllocIndexs = nullptr;
	LVelocity = 0;
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
		Zone* zone = &gameworld.zones[zoneId];
		BoundingOrientedBox obb = GetOBB();
		vec4 ext = obb.Extents;
		float len = ext.len3 / zone->chunck_divide_Width;
		chunkAllocIndexsCapacity = powf(2 * ceilf(len * 0.5f), 3);
		chunkAllocIndexs = new int[chunkAllocIndexsCapacity];
	}
}

matrix DynamicGameObject::GetWorld() {
	Zone* zone = &gameworld.zones[zoneId];
	matrix sav = worldMat;
	GameObject* obj = zone->Dynamic_gameObjects[parent];
	while (obj != nullptr) {
		sav *= obj->worldMat;
		if (obj->tag[GameObjectTag::Tag_Dynamic] == false) break;
		obj = zone->Dynamic_gameObjects[obj->parent];
	}
	return sav;
}

void DynamicGameObject::SetWorld(matrix local)
{
	Zone& zone = gameworld.zones[zoneId];

	//기존 청크에서 정보 지우기
	if (chunkAllocIndexs) {
		ChunkIndex ci = ChunkIndex(IncludeChunks.xmin, IncludeChunks.ymin, IncludeChunks.zmin);
		int len = IncludeChunks.GetChunckSize();
		for (; ci.extra < len; IncludeChunks.Inc(ci)) {
			auto f = zone.chunck.find(ci);
			if (f != zone.chunck.end()) {
				GameChunk* gc = f->second;
				gc->Dynamic_gameobjects.Free(chunkAllocIndexs[ci.extra]);
			}
		}
	}
	worldMat = local;

	if (chunkAllocIndexs) {
		// 새 위치에서 청크에 넣기
		GameObjectIncludeChunks chunkIds = zone.GetChunks_Include_OBB(GetOBB());
		ChunkIndex ci = ChunkIndex(chunkIds.xmin, chunkIds.ymin, chunkIds.zmin);
		ci.extra = 0;
		int chunckCount = chunkIds.GetChunckSize();
		for (; ci.extra < chunckCount; chunkIds.Inc(ci)) {
			auto c = zone.chunck.find(ci);
			GameChunk* gc;
			if (c == zone.chunck.end()) {
				// new game chunk
				gc = new GameChunk();
				gc->SetChunkIndex(ci, &zone);
				zone.chunck.insert(pair<ChunkIndex, GameChunk*>(ci, gc));
			}
			else gc = c->second;
			int allocN = gc->Dynamic_gameobjects.Alloc();
			gc->Dynamic_gameobjects[allocN] = this;
			this->chunkAllocIndexs[ci.extra] = allocN;
		}
	}
}

void DynamicGameObject::MoveChunck(const vec4& velocity, const vec4& Q, const GameObjectIncludeChunks& beforeChunckInc, const GameObjectIncludeChunks& afterChunkInc)
{
	Zone* zone = &gameworld.zones[zoneId];
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
    int requiredChunkCapacity = beforeChunckInc.GetChunckSize();
    int afterRequiredChunkCapacity = afterChunkInc.GetChunckSize();
    if (requiredChunkCapacity < afterRequiredChunkCapacity) requiredChunkCapacity = afterRequiredChunkCapacity;
    if (requiredChunkCapacity > chunkAllocIndexsCapacity) {
        int* newArr = new int[requiredChunkCapacity];
        if (chunkAllocIndexs != nullptr) {
            memcpy(newArr, chunkAllocIndexs, sizeof(int) * chunkAllocIndexsCapacity);
            delete[] chunkAllocIndexs;
        }
        chunkAllocIndexs = newArr;
        chunkAllocIndexsCapacity = requiredChunkCapacity;
    }
	for (; ci.extra < chunckCount; beforeChunckInc.Inc(ci)) {
		if (ci == inter_ci) { // 곂치는 부분을 Free 하지 않는다.
			intersection.Inc(inter_ci);
			temp[inter_up] = chunkAllocIndexs[ci.extra];
			inter_up += 1;
			continue;
		}

		// 안 곂치는 부분은 Free 한다.
		auto f = zone->chunck.find(ci);
		if (f != zone->chunck.end()) {
#ifdef DEVELOPMODE_ChunckDEBUG
			if (f->second->Dynamic_gameobjects.isnull(chunkAllocIndexs[ci.extra])) cout << "[WARN] Dynamic chunk free skipped invalid slot\n";
#endif
			if (f->second->Dynamic_gameobjects.isnull(chunkAllocIndexs[ci.extra]) == false) f->second->Dynamic_gameobjects.Free(chunkAllocIndexs[ci.extra]);
		}
	}

	// 위치 이동 / 회전
	vec4 pos = worldMat.pos;
	worldMat.trQ(Q);
	worldMat.pos = pos + velocity;
	worldMat.pos.w = 1;

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

		auto c = zone->chunck.find(ci);
		GameChunk* gc;
		if (c == zone->chunck.end()) {
			// new game chunk
			gc = new GameChunk();
			gc->SetChunkIndex(ci, zone);
			zone->chunck.insert(pair<ChunkIndex, GameChunk*>(ci, gc));
		}
		else gc = c->second;
		int allocN = gc->Dynamic_gameobjects.Alloc();
		gc->Dynamic_gameobjects[allocN] = this;
		chunkAllocIndexs[ci.extra] = allocN;
	}

	IncludeChunks = afterChunkInc;
}

void DynamicGameObject::Update(float delatTime) {

}

void DynamicGameObject::Release() {
	Zone* zone = &gameworld.zones[zoneId];
	GameObject::Release();
	if (chunkAllocIndexs) {
		int xmax = IncludeChunks.xmin + IncludeChunks.xlen;
		int ymax = IncludeChunks.ymin + IncludeChunks.ylen;
		int zmax = IncludeChunks.zmin + IncludeChunks.zlen;
		int up = 0;
		for (int ix = IncludeChunks.xmin; ix <= xmax; ++ix) {
			for (int iy = IncludeChunks.ymin; iy <= ymax; ++iy) {
				for (int iz = IncludeChunks.zmin; iz <= zmax; ++iz) {
					zone->chunck[ChunkIndex(ix, iy, iz)]->Dynamic_gameobjects.Free(chunkAllocIndexs[up]);
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
	Zone* zone = &gameworld.zones[zoneId];
	zone->GetShape(shapeindex).GetRealShape(mesh, model);
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

void DynamicGameObject::SendGameObject(int objindex, SendDataSaver& sds) {
	sds.postpush_start();

	//calculate app packet siz.
	int reqsiz = sizeof(STC_SyncGameObject_Header) + sizeof(STC_SyncObjData);
	Zone zone = gameworld.zones[zoneId];
	Shape& s = zone.GetShape(shapeindex);
	Mesh* mesh = nullptr;
	Model* model = nullptr;
	s.GetRealShape(mesh, model);
	if (mesh) reqsiz += sizeof(int) + sizeof(int) * mesh->subMeshNum;
	else reqsiz += sizeof(int) + sizeof(matrix) * model->nodeCount;

	sds.postpush_reserve(reqsiz);
	int offset = 0;

	//static pushv
	STC_SyncGameObject_Header& header = *(STC_SyncGameObject_Header*)sds.ofbuff;
	header.size = reqsiz;
	header.st = STC_Protocol::SyncGameObject;
	header.objindex = objindex;
	header.type = GameObjectType::_DynamicGameObject;
	offset += sizeof(STC_SyncGameObject_Header);

	STC_SyncObjData& static_data = *(STC_SyncObjData*)(sds.ofbuff + offset);
	static_data.tag = tag;
	static_data.shapeindex = shapeindex;
	static_data.parent = parent;
	static_data.childs = childs;
	static_data.sibling = sibling;
	static_data.DestWorld = worldMat;
	static_data.LVelocity = LVelocity;
	offset += sizeof(STC_SyncObjData);

	//dynamic push
	if (mesh) {
		int& submeshNum = *(int*)(sds.ofbuff + offset);
		submeshNum = mesh->subMeshNum;
		offset += sizeof(int);

		int*& MaterialArr = *(int**)(sds.ofbuff + offset);
		memcpy(MaterialArr, material, sizeof(int) * mesh->subMeshNum);
		offset += sizeof(int) * mesh->subMeshNum;
	}
	else {
		int& nodecount = *(int*)(sds.ofbuff + offset);
		nodecount = model->nodeCount;
		offset += sizeof(int);

		matrix* InnerModelTransformArr = (matrix*)(sds.ofbuff + offset);
		memcpy(InnerModelTransformArr, transforms_innerModel, sizeof(matrix) * model->nodeCount);
		offset += sizeof(matrix) * model->nodeCount;
	}
	sds.postpush_end();
}

void DynamicGameObject::OnRayHit(GameObject* rayFrom)
{
}

SkinMeshGameObject::SkinMeshGameObject() {
	tag = 0;
	tag[GameObjectTag::Tag_Enable] = false;
	tag[GameObjectTag::Tag_Dynamic] = true;
	tag[GameObjectTag::Tag_SkinMeshObject] = true;
	worldMat.Id();
	shapeindex = -1;
	AnimationFlowTime = 0;
	PlayingAnimationIndex = 0;
}

SkinMeshGameObject::~SkinMeshGameObject() {
}

void SkinMeshGameObject::Update(float delatTime) {
	AnimationFlowTime += DeltaTime;
}

void SkinMeshGameObject::MoveChunck(const vec4& velocity, const vec4& Q, const GameObjectIncludeChunks& beforeChunckInc, const GameObjectIncludeChunks& afterChunkInc) {
	Zone* zone = &gameworld.zones[zoneId];
	static int temp[512] = {};
	GameObjectIncludeChunks intersection = beforeChunckInc;
	intersection &= afterChunkInc;

#ifdef DEVELOPMODE_ChunckDEBUG 
	cout << "objptr = " << this << " FREE";
#endif

	int inter_Count = intersection.GetChunckSize();
	ChunkIndex inter_ci = ChunkIndex(intersection.xmin, intersection.ymin, intersection.zmin);
	inter_ci.extra = 0;
	int inter_up = 0;
	int chunckCount = beforeChunckInc.GetChunckSize();
	ChunkIndex ci = ChunkIndex(beforeChunckInc.xmin, beforeChunckInc.ymin, beforeChunckInc.zmin);
	ci.extra = 0;
    int requiredChunkCapacity = beforeChunckInc.GetChunckSize();
    int afterRequiredChunkCapacity = afterChunkInc.GetChunckSize();
    if (requiredChunkCapacity < afterRequiredChunkCapacity) requiredChunkCapacity = afterRequiredChunkCapacity;
    if (requiredChunkCapacity > chunkAllocIndexsCapacity) {
        int* newArr = new int[requiredChunkCapacity];
        if (chunkAllocIndexs != nullptr) {
            memcpy(newArr, chunkAllocIndexs, sizeof(int) * chunkAllocIndexsCapacity);
            delete[] chunkAllocIndexs;
        }
        chunkAllocIndexs = newArr;
        chunkAllocIndexsCapacity = requiredChunkCapacity;
    }
	
	for (; ci.extra < chunckCount; beforeChunckInc.Inc(ci)) {
		if (ci == inter_ci && inter_Count > 0) { // 곂치는 부분을 Free 하지 않는다.
			intersection.Inc(inter_ci);
			temp[inter_up] = chunkAllocIndexs[ci.extra];
			inter_up += 1;
			
			continue;
		}
		// 안 곂치는 부분은 Free 한다.
		auto f = zone->chunck.find(ci);
		if (f != zone->chunck.end()) {
#ifdef DEVELOPMODE_ChunckDEBUG 
			if (f->second->SkinMesh_gameobjects.isnull(chunkAllocIndexs[ci.extra])) cout << "[WARN] SkinMesh chunk free invalid slot\n";
			cout << "ci(" << ci.x << ", " << ci.y << ", " << ci.z << ") : " << chunkAllocIndexs[ci.extra] << "\t";
#endif
			if (f->second->SkinMesh_gameobjects.isnull(chunkAllocIndexs[ci.extra]) == false) f->second->SkinMesh_gameobjects.Free(chunkAllocIndexs[ci.extra]);
		}
    }
#ifdef DEVELOPMODE_ChunckDEBUG
	cout << endl;
#endif



	// 위치 이동 / 회전
	vec4 pos = worldMat.pos;
	worldMat.trQ(Q);
	worldMat.pos = pos + velocity;
	worldMat.pos.w = 1;

	inter_ci = ChunkIndex(intersection.xmin, intersection.ymin, intersection.zmin);
	inter_ci.extra = 0;

	chunckCount = afterChunkInc.GetChunckSize();
	ci = ChunkIndex(afterChunkInc.xmin, afterChunkInc.ymin, afterChunkInc.zmin);
	ci.extra = 0;

	ChunkIndex pastCI = ChunkIndex(-99999, -99999, -99999);

#ifdef DEVELOPMODE_ChunckDEBUG 
	cout << "objptr = " << this << " ALLOC";
#endif
	inter_up = 0;
	for (; ci.extra < chunckCount; afterChunkInc.Inc(ci)) {
		if (ci == inter_ci && inter_Count > 0) { // 곂치는 부분을 Alloc 하지 않는다.
			intersection.Inc(inter_ci);
			chunkAllocIndexs[ci.extra] = temp[inter_up];
			inter_up += 1;
#ifdef DEVELOPMODE_ChunckDEBUG 
			cout << "ci_move(" << ci.x << ", " << ci.y << ", " << ci.z << ") : " << temp[inter_up-1] << "\t";
			pastCI = ci;
#endif
			continue;
		}

		auto c = zone->chunck.find(ci);
		GameChunk* gc;
		if (c == zone->chunck.end()) {
			// new game chunk
			gc = new GameChunk();
			gc->SetChunkIndex(ci, zone);
			zone->chunck.insert(pair<ChunkIndex, GameChunk*>(ci, gc));
		}
		else gc = c->second;
		int allocN = gc->SkinMesh_gameobjects.Alloc();
		gc->SkinMesh_gameobjects[allocN] = this;
		chunkAllocIndexs[ci.extra] = allocN;

#ifdef DEVELOPMODE_ChunckDEBUG
		cout << "ci(" << ci.x << ", " << ci.y << ", " << ci.z << ") : " << allocN << "\t";
#endif
	}
#ifdef DEVELOPMODE_ChunckDEBUG
	cout << endl;
#endif

	IncludeChunks = afterChunkInc;
}

void SkinMeshGameObject::SendGameObject(int objindex, SendDataSaver& sds) {
	sds.postpush_start();

	//calculate app packet siz.
	int reqsiz = sizeof(STC_SyncGameObject_Header) + sizeof(STC_SyncObjData);
	Zone zone = gameworld.zones[zoneId];
	Shape& s = zone.GetShape(shapeindex);
	Mesh* mesh = nullptr;
	Model* model = nullptr;
	s.GetRealShape(mesh, model);
	if (mesh) reqsiz += sizeof(int) + sizeof(int) * mesh->subMeshNum;
	else reqsiz += sizeof(int) + sizeof(matrix) * model->nodeCount;

	sds.postpush_reserve(reqsiz);
	int offset = 0;

	//static pushv
	STC_SyncGameObject_Header& header = *(STC_SyncGameObject_Header*)sds.ofbuff;
	header.size = reqsiz;
	header.st = STC_Protocol::SyncGameObject;
	header.objindex = objindex;
	header.type = GameObjectType::_DynamicGameObject;
	offset += sizeof(STC_SyncGameObject_Header);

	STC_SyncObjData& static_data = *(STC_SyncObjData*)(sds.ofbuff + offset);
	static_data.tag = tag;
	static_data.shapeindex = shapeindex;
	static_data.parent = parent;
	static_data.childs = childs;
	static_data.sibling = sibling;
	static_data.DestWorld = worldMat;
	static_data.LVelocity = LVelocity;
	static_data.AnimationFlowTime = AnimationFlowTime;
	static_data.PlayingAnimationIndex = PlayingAnimationIndex;
	offset += sizeof(STC_SyncObjData);

	//dynamic push

	//shape
	if (mesh) {
		int& submeshNum = *(int*)(sds.ofbuff + offset);
		submeshNum = mesh->subMeshNum;
		offset += sizeof(int);

		int*& MaterialArr = *(int**)(sds.ofbuff + offset);
		memcpy(MaterialArr, material, sizeof(int) * mesh->subMeshNum);
		offset += sizeof(int) * mesh->subMeshNum;
	}
	else {
		int& nodecount = *(int*)(sds.ofbuff + offset);
		nodecount = model->nodeCount;
		offset += sizeof(int);

		matrix* InnerModelTransformArr = (matrix*)(sds.ofbuff + offset);
		memcpy(InnerModelTransformArr, transforms_innerModel, sizeof(matrix) * model->nodeCount);
		offset += sizeof(matrix) * model->nodeCount;
	}

	sds.postpush_end();
}

void SkinMeshGameObject::SetWorld(matrix local) {
	Zone& zone = gameworld.zones[zoneId];

	//기존 청크에서 정보 지우기
	if (chunkAllocIndexs) {
		ChunkIndex ci = ChunkIndex(IncludeChunks.xmin, IncludeChunks.ymin, IncludeChunks.zmin);
		int len = IncludeChunks.GetChunckSize();
		for (; ci.extra < len; IncludeChunks.Inc(ci)) {
			auto f = zone.chunck.find(ci);
			if (f != zone.chunck.end()) {
				GameChunk* gc = f->second;
				gc->SkinMesh_gameobjects.Free(chunkAllocIndexs[ci.extra]);
			}
		}
	}
	worldMat = local;

	if (chunkAllocIndexs) {
		// 새 위치에서 청크에 넣기
		GameObjectIncludeChunks chunkIds = zone.GetChunks_Include_OBB(GetOBB());
		ChunkIndex ci = ChunkIndex(chunkIds.xmin, chunkIds.ymin, chunkIds.zmin);
		ci.extra = 0;
		int chunckCount = chunkIds.GetChunckSize();
		for (; ci.extra < chunckCount; chunkIds.Inc(ci)) {
			auto c = zone.chunck.find(ci);
			GameChunk* gc;
			if (c == zone.chunck.end()) {
				// new game chunk
				gc = new GameChunk();
				gc->SetChunkIndex(ci, &zone);
				zone.chunck.insert(pair<ChunkIndex, GameChunk*>(ci, gc));
			}
			else gc = c->second;
			int allocN = gc->SkinMesh_gameobjects.Alloc();
			gc->SkinMesh_gameobjects[allocN] = this;
			this->chunkAllocIndexs[ci.extra] = allocN;
		}
	}
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
	GameObjectIncludeChunks goic = ownerzone->GetChunks_Include_OBB(obb);

	for (int ix = goic.xmin; ix <= goic.xmin + goic.xlen; ++ix) {
		for (int iy = goic.ymin; iy <= goic.ymin + goic.ylen; ++iy) {
			for (int iz = goic.zmin; iz <= goic.zmin + goic.zlen; ++iz) {
				auto chun = ownerzone->chunck.find(ChunkIndex(ix, iy, iz));
				if (chun == ownerzone->chunck.end()) continue;
				GameChunk* ch = chun->second;
				//Static Object는 Enable이 false일 수 없기 때문에 할당검사는 안함.
				// >> 그럼 그냥 vector여도 상관없잖음 왜 vecset으로 함? >> fix

				//어짜피 서버면 청크를 다 만드는게 맞지 않을까? 그럼 nullptr 체크를 할 필요가 있나?
				// fix
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

bool GameMap::isStaticCollision(BoundingOrientedBox obb)
{
	GameObjectIncludeChunks goic = ownerzone->GetChunks_Include_OBB(obb);

	for (int ix = goic.xmin; ix <= goic.xmin + goic.xlen; ++ix) {
		for (int iy = goic.ymin; iy <= goic.ymin + goic.ylen; ++iy) {
			for (int iz = goic.zmin; iz <= goic.zmin + goic.zlen; ++iz) {
				auto it = ownerzone->chunck.find(ChunkIndex(ix, iy, iz));
				if (it == ownerzone->chunck.end()) continue;

				GameChunk* ch = it->second;
				if (ch == nullptr) continue;

				for (int k = 0; k < ch->Static_gameobjects.size; ++k) {
					if (ch->Static_gameobjects.isnull(k)) continue;

					BoundingOrientedBox staticobb = ch->Static_gameobjects[k]->GetOBB();
					if (obb.Intersects(staticobb)) {
						return true;
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

	TextureTableStart = gameworld.GlobalTextureCount; // fix : 현재 MaterialTexture 개수
	MaterialTableStart = gameworld.GlobalMaterialCount; // fix : 현재 Material 개수

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
		mesh_shapeindexes[i] = Shape::ShapeTable.size() + Shape::AddMeshInZone(map->name[nameid], &map->meshes[i], ownerzone->zoneId);
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
		ownerzone->ZoneTextureCount += 1;
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
		//gameworld.MaterialCount += 1;
		ownerzone->ZoneMaterialCount += 1;
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
		model_shapeindexes[i] = Shape::ShapeTable.size() + Shape::AddModelInZone(modelName, pModel, ownerzone->zoneId);
	}

	ownerzone->Static_gameObjects.Init(gameObjectCount);
	for (int i = 0; i < gameObjectCount; ++i) {
		StaticGameObject* go = new StaticGameObject();
		go->zoneId = ownerzone->zoneId;
		map->MapObjects[i] = go;
		go->parent = -1;
		go->childs = -1;
		go->sibling = -1;
		int index = ownerzone->Static_gameObjects.Alloc();
		ownerzone->Static_gameObjects[index] = go;
	}
	for (int i = 0; i < gameObjectCount; ++i) {
		//dbgbreak(i == 308);
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
		if (Mod == 'n') { // mesh
			int meshid = 0;
			int materialNum = 0;
			int materialId = 0;
			ifs.read((char*)&meshid, sizeof(int));
			if (meshid < 0) {
				go->shapeindex = -1;
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
			go->obbArr.reserve(ColliderCount);
			go->obbArr.resize(ColliderCount);
			for (int k = 0; k < ColliderCount; ++k) {
				XMFLOAT3 Center;
				XMFLOAT3 Extents;
				ifs.read((char*)&Center, sizeof(XMFLOAT3));
				ifs.read((char*)&Extents, sizeof(XMFLOAT3));
				BoundingOrientedBox obb = BoundingOrientedBox(Center, Extents, vec4(0, 0, 0, 1));
				BoundingOrientedBox obb_world;
				obb.Transform(obb_world, go->worldMat);
				go->obbArr[k] = obb_world;
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

			Zone* zone = &gameworld.zones[ownerzone->zoneId];
			int nodeCount = zone->GetShape(go->shapeindex).GetModel()->nodeCount;
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
			vec4 LightColor;
			ifs.read((char*)&LightColor, sizeof(vec4));
		}

		if (Mod != 'm') {
			int childCount = 0;
			ifs.read((char*)&childCount, sizeof(int));
			int cnt = 0;
			StaticGameObject* temp = go;
			while (cnt < childCount) {
				int childIndex = 0;
				ifs.read((char*)&childIndex, sizeof(int));
				if (cnt == 0) {
					go->childs = childIndex;
				}
				else {
					temp->sibling = childIndex;
				}
				temp = map->MapObjects[childIndex];
				temp->parent = i;
				cnt += 1;
			}
		}
		else {
			go->childs = -1;
		}

		if (Mod == 'm') {
			go->obbArr.clear();
			Zone* zone = &gameworld.zones[ownerzone->zoneId];
			Model* model = zone->GetShape(go->shapeindex).GetModel();
			model->RootNode->PushOBBs(model, go->worldMat, &go->obbArr, go);
			//for (int k = 0;k < model->nodeCount;++k) {
			//	ModelNode* node = &model->Nodes[k];
			//	// fix. 유니티 자식 오브젝트들의 AABB는 어떻게 구성됨? 계층구조의 영향을 받는지 확인이 필요.
			//	for (int u = 0;u < node->aabbArr.size();++u) {
			//		BoundingOrientedBox obb;
			//		obb.Center = node->aabbArr[u].Center;
			//		obb.Extents = node->aabbArr[u].Extents;
			//		obb.Orientation = vec4(0, 0, 0, 1);
			//		obb.Transform(obb, world);
			//		go->obbArr.push_back(obb);
			//	}
			//}
		}
		map->MapObjects[i] = go;
	}

	//BakeStaticCollision();
}

void Model::LoadModelFile2(string filename, Zone* zone)
{
	ifstream ifs{ filename, ios_base::binary };
	ifs.read((char*)&mNumMeshes, sizeof(unsigned int));
	mMeshes = new Mesh * [mNumMeshes];

	ifs.read((char*)&nodeCount, sizeof(unsigned int));
	Nodes = new ModelNode[nodeCount];

	int MaterialTableStart = gameworld.GlobalMaterialCount;
	if (zone) {
		MaterialTableStart + zone->ZoneMaterialCount;
	}

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

			WorldMat.transpose();
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
		
		if (zone) {
			zone->ZoneMaterialCount += 1;
		}
		else {
			gameworld.GlobalMaterialCount += 1;
		}
	}

	RootNode = &Nodes[0];

	BakeAABB();
}

void Model::BakeAABB()
{
	AABB[0] = 0;
	AABB[1] = 0;
	RootNode->BakeAABB(this, XMMatrixIdentity());
	OBB_Tr = 0.5f * (AABB[0] + AABB[1]);
	OBB_Ext = AABB[1] - OBB_Tr;
}

BoundingOrientedBox Model::GetOBB() {
	return BoundingOrientedBox(OBB_Tr.f3, OBB_Ext.f3, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
}

void Mesh::GetAABBFromOBB(vec4* out, BoundingOrientedBox obb, bool first) {
	XMFLOAT3 corners[BoundingOrientedBox::CORNER_COUNT];
	obb.GetCorners(corners);

	//dbglog3_noline(L"AABB start : (%g, %g, %g) - ", out[0].x, out[0].y, out[0].z);
	//dbglog3_noline(L"(%g, %g, %g)\n", out[1].x, out[1].y, out[1].z);

	vec4 c[8];
	c[0].f3 = corners[0];
	//dbglog4_noline(L"OBB[%d] : (%g, %g, %g)\n", 0, c[0].x, c[0].y, c[0].z);
	vec4 minpos = (first) ? c[0] : vec4(_mm_min_ps(out[0], c[0]));
	vec4 maxpos = (first) ? c[0] : vec4(_mm_max_ps(out[1], c[0]));
	for (int i = 1; i < 8; ++i) {
		c[i].f3 = corners[i];
		//dbglog4_noline(L"OBB[%d] : (%g, %g, %g)\n", i, c[i].x, c[i].y, c[i].z);
		minpos = _mm_min_ps(c[i], minpos);
		maxpos = _mm_max_ps(c[i], maxpos);
	}
	out[0] = minpos;
	out[1] = maxpos;

	//dbglog3_noline(L"AABB result : (%g, %g, %g) - ", out[0].x, out[0].y, out[0].z);
	//dbglog3_noline(L"(%g, %g, %g)\n", out[1].x, out[1].y, out[1].z);

	return;

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

		BoundingOrientedBox obb = model->mMeshes[Meshes[0]]->GetOBB();
		BoundingOrientedBox obb_world;
		obb.Transform(obb_world, sav);

		Mesh::GetAABBFromOBB(model->AABB, obb_world);
		for (int i = 1; i < numMesh; ++i) {
			BoundingOrientedBox obb = model->mMeshes[Meshes[i]]->GetOBB();
			BoundingOrientedBox obb_world;
			obb.Transform(obb_world, sav);

			Mesh::GetAABBFromOBB(model->AABB, obb_world);
		}
	}

	for (int i = 0; i < numChildren; ++i) {
		ModelNode* node = Childrens[i];
		node->BakeAABB(origin, sav);
	}
}

void ModelNode::PushOBBs(void* origin, const matrix& parentMat, vector<BoundingOrientedBox>* obbArr, void* gameobj)
{
	if (obbArr == nullptr) return;
	Model* model = (Model*)origin;
	GameObject* obj = (GameObject*)gameobj;
	XMMATRIX sav = XMMatrixMultiply(transform, parentMat);
	if (gameobj) {
		int nodeindex = ((char*)this - (char*)model->RootNode) / sizeof(ModelNode);
		sav = XMMatrixMultiply(obj->transforms_innerModel[nodeindex], parentMat);
	}
	if (this->aabbArr.size() > 0) {
		for (int i = 0; i < aabbArr.size(); ++i) {
			BoundingBox aabb = aabbArr[i];
			BoundingOrientedBox obb;
			obb = BoundingOrientedBox(aabb.Center, aabb.Extents, vec4(0, 0, 0, 1));
			BoundingOrientedBox obb_world;
			obb.Transform(obb_world, sav);
			obbArr->push_back(obb_world);
		}
	}

	for (int i = 0; i < numChildren; ++i) {
		ModelNode* node = Childrens[i];
		node->PushOBBs(origin, sav, obbArr, gameobj);
	}
}

Player::Player() {
	tag = 0;
	tag[GameObjectTag::Tag_Enable] = false;
	tag[GameObjectTag::Tag_Dynamic] = true;
	tag[GameObjectTag::Tag_SkinMeshObject] = true;
	worldMat.Id();
	shapeindex = -1;

	ApplyJob(PlayerJob::Healer);

	HP = 100;
	MaxHP = 100;
	bullets = weapon.m_info.maxBullets;
	KillCount = 0;
	DeathCount = 0;
	HeatGauge = 0;
	MaxHeatGauge = 200;
	for (int i = 0; i < (int)SkillSlot::Max; ++i) {
		SkillCooldownFlow[i] = 0.0f;
	}
	ZeroMemory(Inventory, sizeof(ItemStack) * maxItem);

	JumpVelocity = 5;
	isGround = false;
	collideCount = 0;
	clientIndex = 0;
	InputBuffer[0] = 0;
	InputBuffer[1] = 0;
	bFirstPersonVision = true;
	m_yaw = 0;
	m_pitch = 0;
}

void Player::ApplyJob(PlayerJob job)
{
	const JobData& jobData = GetJobData(job);
	m_currentJob = (int)jobData.job;
	m_currentWeaponType = (int)jobData.defaultWeapon;
	weapon = Weapon(jobData.defaultWeapon);
	bullets = weapon.m_info.maxBullets;
	MaxHP -= m_tempMaxHpBonus;
	if (MaxHP <= 0.0f) MaxHP = 100.0f;
	if (HP > MaxHP) HP = MaxHP;

	MaxHP = jobData.MaxHP;
	Attack = jobData.Attack;
	Defense = jobData.Defense;

	m_tempMaxHpBonus = 0.0f;
	m_tempMaxHpTimer = 0.0f;
	m_iceBlockTimer = 0.0f;
	m_frostPassiveUsed = false;

	for (int i = 0; i < (int)SkillSlot::Max; ++i) {
		SkillCooldown[i] = jobData.skills[i].cooldown;
		SkillCooldownFlow[i] = 0.0f;
	}

	cout << "Applied job: " << endl
	<< "MaxHP: " << jobData.MaxHP << "Attack" << jobData.Attack << "Defense : " << jobData.Defense << endl;
}

void Player::SyncJobState(Zone* zones)
{
	if (zones == nullptr) return;

	zones->Sending_ChangeGameObjectMember<int>(gameworld.clients[clientIndex].PersonalSDS,
		gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &m_currentJob);
	zones->Sending_ChangeGameObjectMember<int>(gameworld.clients[clientIndex].PersonalSDS,
		gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &m_currentWeaponType);
	zones->Sending_ChangeGameObjectMember<int>(gameworld.clients[clientIndex].PersonalSDS,
		gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &bullets);
	zones->Sending_ChangeGameObjectMember<Weapon>(gameworld.clients[clientIndex].PersonalSDS,
		gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &weapon);
	zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS,
		gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HP);
	zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS,
		gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &MaxHP);
	zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS,
		gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &Attack);
	zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS,
		gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &Defense);
	zones->Sending_ChangeGameObjectMember<decltype(SkillCooldown)>(gameworld.clients[clientIndex].PersonalSDS,
		gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, SkillCooldown);
	zones->Sending_ChangeGameObjectMember<decltype(SkillCooldownFlow)>(gameworld.clients[clientIndex].PersonalSDS,
		gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, SkillCooldownFlow);
}

void Player::UpdateSkillCooldowns(float deltaTime, Zone* zones)
{
	bool changed = false;
	for (int i = 0; i < (int)SkillSlot::Max; ++i) {
		if (SkillCooldownFlow[i] > 0.0f) {
			SkillCooldownFlow[i] -= deltaTime;
			if (SkillCooldownFlow[i] < 0.0f) SkillCooldownFlow[i] = 0.0f;
			changed = true;
		}
	}

	if (changed) {
		zones->Sending_ChangeGameObjectMember<decltype(SkillCooldownFlow)>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, SkillCooldownFlow);
	}
}

void Player::UpdateJobTimers(float deltaTime, Zone* zones)
{
	if (m_tempMaxHpTimer > 0.0f) {
		m_tempMaxHpTimer -= deltaTime;
		if (m_tempMaxHpTimer <= 0.0f && m_tempMaxHpBonus > 0.0f) {
			MaxHP -= m_tempMaxHpBonus;
			if (HP > MaxHP) HP = MaxHP;
			m_tempMaxHpBonus = 0.0f;
			zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &MaxHP);
			zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HP);
		}
	}

	if (m_iceBlockTimer > 0.0f) {
		m_iceBlockTimer -= deltaTime;
		if (m_iceBlockTimer < 0.0f) m_iceBlockTimer = 0.0f;
	}
}

bool Player::TryUseSkill(SkillSlot slot)
{
	int slotIndex = (int)slot;
	if (slotIndex < 0 || slotIndex >= (int)SkillSlot::Max) return false;
	if (SkillCooldownFlow[slotIndex] > 0.0f) return false;

	Zone* zones = gameworld.GetClientZone(clientIndex);
	const JobData& jobData = GetJobData((PlayerJob)m_currentJob);
	const SkillData& skill = jobData.skills[slotIndex];

	if (skill.heatCost > 0.0f && HeatGauge < skill.heatCost) return false;

	bool applied = true;
	if (skill.effectType == SkillEffectType::Healer_HealAura) {
		if (HP >= MaxHP && HeatGauge <= 0.0f) return false;

		float healAmount = skill.power;
		if (healAmount <= 0.0f || healAmount > HeatGauge) healAmount = HeatGauge;
		if (healAmount <= 0.0f) return false;

		HP += healAmount;
		if (HP > MaxHP) HP = MaxHP;
		HeatGauge -= healAmount;
		if (HeatGauge < 0.0f) HeatGauge = 0.0f;

		zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HP);
		zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HeatGauge);
	}
	else {
		if (skill.heatCost > 0.0f) {
			HeatGauge -= skill.heatCost;
			if (HeatGauge < 0.0f) HeatGauge = 0.0f;
			zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HeatGauge);
		}
	}

	if (skill.effectType == SkillEffectType::Frost_IceBlock) {
		HP += skill.power;
		if (HP > MaxHP) HP = MaxHP;
		m_iceBlockTimer = skill.duration;
		zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HP);
	}
	else if (skill.effectType == SkillEffectType::Juggernaut_UltimateFire || skill.effectType == SkillEffectType::Aegis_ShieldAura) {
		float bonus = skill.power;
		if (bonus > m_tempMaxHpBonus) {
			MaxHP += bonus - m_tempMaxHpBonus;
			HP += bonus - m_tempMaxHpBonus;
			m_tempMaxHpBonus = bonus;
		}
		m_tempMaxHpTimer = skill.duration;
		zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &MaxHP);
		zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HP);
	}

	if (applied == false) return false;

	SkillCooldownFlow[slotIndex] = skill.cooldown;
	zones->Sending_ChangeGameObjectMember<decltype(SkillCooldownFlow)>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, SkillCooldownFlow);

	XMVECTOR quaternion = XMQuaternionRotationRollPitchYaw(m_pitch, m_yaw, 0);
	vec4 direction = { 0, 0, 1, 0 };
	direction = XMVector3Rotate(direction, quaternion);
	zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob, slot, skill.effectType, worldMat.pos, direction, skill.radius, skill.power, skill.duration);
	return true;
}
void Player::Update(float deltaTime)
{
	Zone* zones = gameworld.GetClientZone(clientIndex);
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

	UpdateSkillCooldowns(deltaTime, zones);
	UpdateJobTimers(deltaTime, zones);

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

	if (InputBuffer[InputID::KeyboardQ] == true) {
		TryUseSkill(SkillSlot::Ultimate);
		InputBuffer[InputID::KeyboardQ] = false;
	}

	if (InputBuffer[InputID::MouseLbutton] == true) {
		bool isOverheated = ((PlayerJob)m_currentJob == PlayerJob::Juggernaut && HeatGauge >= MaxHeatGauge);
		if (weapon.m_shootFlow >= weapon.m_info.shootDelay && bullets > 0 && HeatGauge < MaxHeatGauge && isOverheated == false) {

			bullets -= 1;
			HeatGauge += 2;
			if (HeatGauge > MaxHeatGauge) HeatGauge = MaxHeatGauge;

			weapon.OnFire();

			vec4 shootOrigin = worldMat.pos + vec4(0, 1.0f, 0, 0);
			zones->FireRaycast((GameObject*)this, shootOrigin + clook * 1.5f, clook, 50.0f, weapon.m_info.damage);

			// fix 이건 이렇게 하면 안될것 같은데? Update 될때마다 패킷이 쌓임. 
			// 패킷이 많아지면 서버에서 부담이므로 차라리 특정시간마다 쏴주는게 좋다.
			zones->Sending_ChangeGameObjectMember<int>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &bullets);
			zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HeatGauge);
			zones->Sending_PlayerFire(zones->CommonSDS, gameworld.clients[clientIndex].objindex);

			if (bullets <= 0) {
				weapon.m_shootFlow = -weapon.m_info.reloadTime;

				bullets = weapon.m_info.maxBullets;

				zones->Sending_ChangeGameObjectMember<int>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &bullets);
			}
		}
	}

	//int datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &DeltaMousePos, 8);
	//gameworld.clients[clientIndex].socket.Send(gameworld.tempbuffer.data, datacap);

	BoundingOrientedBox obb = GetOBB();
	for (int i = 0; i < zones->DropedItems.size; ++i) {
		if (zones->DropedItems.isnull(i)) continue;
		vec4 p = zones->DropedItems[i].pos;
		p.w = 0;
		if (obb.Contains(p)) {
			bool isexist = false;
			bool beat = false;
			int firstBlackIndex = -1;
			for (int k = 0; k < Player::maxItem; ++k) {
				if (Inventory[k].id == 0 && firstBlackIndex == -1) {
					firstBlackIndex = k;
				}
				if (Inventory[k].id == zones->DropedItems[i].itemDrop.id)
				{
					Inventory[k].ItemCount += zones->DropedItems[i].itemDrop.ItemCount;
					isexist = true;
					beat = true;
					firstBlackIndex = k;
					break;
				}
			}
			if (isexist == false && firstBlackIndex != -1) {
				Inventory[firstBlackIndex] = zones->DropedItems[i].itemDrop;
				beat = true;
			}

			if (beat) {
				zones->Sending_InventoryItemSync(gameworld.clients[clientIndex].PersonalSDS, Inventory[firstBlackIndex], firstBlackIndex);
				zones->DropedItems.Free(i);
				zones->Sending_ItemRemove(zones->CommonSDS, i);
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

		zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS
			, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HeatGauge);
	}

}

void Player::OnCollision(GameObject* other)
{
	collideCount += 1;
	float belowDist = 0;
	BoundingOrientedBox otherOBB = other->GetOBB();

	BoundingOrientedBox bottomObb = GetOBB();
	bottomObb.Center.y += tickLVelocity.y;

	if (bottomObb.Intersects(otherOBB)) {
		LVelocity.y = 0;
		isGround = true;
	}
}

void Player::OnStaticCollision(BoundingOrientedBox obb)
{
	collideCount += 1;
	float belowDist = 0;
	BoundingOrientedBox otherOBB = obb;

	BoundingOrientedBox bottomObb = GetOBB();
	bottomObb.Center.y += tickLVelocity.y;

	if (bottomObb.Intersects(otherOBB)) {
		LVelocity.y = 0;
		isGround = true;
	}

	/*bool belowhit = otherOBB.Intersects(worldMat.pos, vec4(0, -1, 0, 0), belowDist);
	if (belowhit){
		LVelocity.y = 0;
		if(belowDist < GetOBB().Extents.y + 1.0f) {
			isGround = true;
		}
	}*/
}

BoundingOrientedBox Player::GetOBB()
{
	BoundingOrientedBox obb_local = BoundingOrientedBox({ 0, 1.0f, 0 }, { 0.3f, 1.0f, 0.3f }, vec4(0, 0, 0, 1));
	obb_local.Extents.x = obb_local.Extents.z;
	BoundingOrientedBox obb_world;
	matrix id = XMMatrixIdentity();
	id.pos = worldMat.pos;
	obb_local.Transform(obb_world, id);
	return obb_world;
}

void Player::TakeDamage(float damage)
{
	Zone* zones = gameworld.GetClientZone(clientIndex);

	float defense = Defense;
	// All Job Reduce Damage With Defense
	damage *= 100.0f / (100.0f + defense);

	if (m_iceBlockTimer > 0.0f) {
		damage *= 0.1f;
	}
	else if ((PlayerJob)m_currentJob == PlayerJob::Aegis) {
		damage *= 0.9f;
	}
	else if ((PlayerJob)m_currentJob == PlayerJob::Juggernaut) {
		float heatRate = MaxHeatGauge > 0.0f ? HeatGauge / MaxHeatGauge : 0.0f;
		if (heatRate < 0.0f) heatRate = 0.0f;
		if (heatRate > 1.0f) heatRate = 1.0f;
		damage *= 1.0f - (0.35f * heatRate);
	}
	HP -= damage;

	if ((PlayerJob)m_currentJob == PlayerJob::Frost && m_frostPassiveUsed == false && HP > 0.0f && HP <= MaxHP * 0.3f) {
		HP += MaxHP * 0.2f;
		if (HP > MaxHP) HP = MaxHP;
		m_frostPassiveUsed = true;
	}

	zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HP);

	if (HP <= 0) {
		tag[GameObjectTag::Tag_Enable] = false;

		DeathCount += 1;
		zones->Sending_ChangeGameObjectMember<int>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &DeathCount);

		Respawn();
	}

	//cout << "player's defense" << defense << " final damage is " << damage << " remain hp: " << HP << endl;
}

void Player::OnCollisionRayWithBullet(GameObject* shooter, float damage)
{
	TakeDamage(damage);
}

void Player::SendGameObject(int objindex, SendDataSaver& sds) {
	sds.postpush_start();

	//calculate app packet siz.
	int reqsiz = sizeof(STC_SyncGameObject_Header) + sizeof(STC_SyncObjData);
	Zone zone = gameworld.zones[zoneId];
	Shape& s = zone.GetShape(shapeindex);
	Mesh* mesh = nullptr;
	Model* model = nullptr;
	s.GetRealShape(mesh, model);
	if (mesh) reqsiz += sizeof(int) + sizeof(int) * mesh->subMeshNum;
	else reqsiz += sizeof(int) + sizeof(matrix) * model->nodeCount;

	sds.postpush_reserve(reqsiz);
	int offset = 0;

	//static pushv
	STC_SyncGameObject_Header& header = *(STC_SyncGameObject_Header*)sds.ofbuff;
	header.size = reqsiz;
	header.st = STC_Protocol::SyncGameObject;
	header.objindex = objindex;
	header.type = GameObjectType::_Player;
	offset += sizeof(STC_SyncGameObject_Header);

	STC_SyncObjData& static_data = *(STC_SyncObjData*)(sds.ofbuff + offset);
	static_data.tag = tag;
	static_data.shapeindex = shapeindex;
	static_data.parent = parent;
	static_data.childs = childs;
	static_data.sibling = sibling;
	static_data.DestWorld = worldMat;
	static_data.LVelocity = LVelocity;
	static_data.AnimationFlowTime = AnimationFlowTime;
	static_data.PlayingAnimationIndex = PlayingAnimationIndex;

	static_data.HP = HP;
	static_data.MaxHP = MaxHP;
	static_data.bullets = bullets;
	static_data.KillCount = KillCount;
	static_data.DeathCount = DeathCount;
	static_data.HeatGauge = HeatGauge;
	static_data.MaxHeatGauge = MaxHeatGauge;
	static_data.m_currentJob = m_currentJob;
	memcpy(static_data.SkillCooldown, SkillCooldown, sizeof(SkillCooldown));
	memcpy(static_data.SkillCooldownFlow, SkillCooldownFlow, sizeof(SkillCooldownFlow));
	static_data.m_currentWeaponType = m_currentWeaponType;
	/*memcpy(static_data.Inventory, Inventory, sizeof(ItemStack) * maxItem);*/
	static_data.weapon = weapon;
	offset += sizeof(STC_SyncObjData);

	//dynamic push

	//shape
	if (mesh) {
		int& submeshNum = *(int*)(sds.ofbuff + offset);
		submeshNum = mesh->subMeshNum;
		offset += sizeof(int);

		int*& MaterialArr = *(int**)(sds.ofbuff + offset);
		memcpy(MaterialArr, material, sizeof(int) * mesh->subMeshNum);
		offset += sizeof(int) * mesh->subMeshNum;
	}
	else {
		int& nodecount = *(int*)(sds.ofbuff + offset);
		nodecount = model->nodeCount;
		offset += sizeof(int);

		matrix* InnerModelTransformArr = (matrix*)(sds.ofbuff + offset);
		memcpy(InnerModelTransformArr, transforms_innerModel, sizeof(matrix) * model->nodeCount);
		offset += sizeof(matrix) * model->nodeCount;
	}

	sds.postpush_end();
}

void Player::Respawn() {
	Zone* zones = gameworld.GetClientZone(clientIndex);
	MaxHP -= m_tempMaxHpBonus;
	if (MaxHP <= 0.0f) MaxHP = 100.0f;
	m_tempMaxHpBonus = 0.0f;
	m_tempMaxHpTimer = 0.0f;
	m_iceBlockTimer = 0.0f;
	m_frostPassiveUsed = false;
	HP = MaxHP;
	tag[GameObjectTag::Tag_Enable] = true;

	matrix mat;
	mat.Id();
	mat.pos.y = 2;
	SetWorld(mat);

	//player position send
	bool isExist = true;
	zones->Sending_ChangeGameObjectMember<Tag>(zones->CommonSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &tag);
	zones->Sending_ChangeGameObjectMember<float>(zones->CommonSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HP);
}

Monster::Monster() {
	tag = 0;
	tag[GameObjectTag::Tag_Enable] = false;
	tag[GameObjectTag::Tag_Dynamic] = true;
	tag[GameObjectTag::Tag_SkinMeshObject] = true;
	worldMat.Id();
	shapeindex = -1;

	HP = 30;
	MaxHP = 30;
	Attack = 10.0f;
	Defense = 0.0f;
	isDead = false;

	m_homePos = 0;
	m_targetPos = 0;
	m_speed = 2.0f;
	m_patrolRange = 20.0f;
	m_chaseRange = 10.0f;
	m_patrolTimer = 0.0f;
	m_fireDelay = 1.0f;
	m_fireTimer = 0.0f;
	collideCount = 0;
	targetSeekIndex = 0;
	Target = nullptr;
	m_isMove = false;
	isGround = false;
	respawntimer = 0;
	pathfindTimer = 0.0f;
}

void Monster::ApplyMonsterData(MonsterType type)
{
	const MonsterData& data = GetMonsterData(type);

	m_monsterType = data.type;
	MaxHP = data.MaxHP;
	HP = MaxHP;
	Attack = data.Attack;
	Defense = data.Defense;
	m_speed = data.MoveSpeed;
	m_fireDelay = data.FireDelay;

	auto shape = Shape::StrToShapeIndex.find(data.shapeName);
	if (shape != Shape::StrToShapeIndex.end()) {
		SetShape(shape->second);
	}

	cout << "[MonsterData] type=" << data.name
		<< " MaxHP=" << MaxHP
		<< " Attack=" << Attack
		<< " Defense=" << Defense
		<< " Speed=" << m_speed
		<< " FireDelay=" << m_fireDelay
		<< endl;
}

void Monster::Update(float deltaTime)
{
	Zone* zone = gameworld.GetZone(zoneId);

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

		if (m_monsterType == MonsterType::Dron) {
			LVelocity = 0;
			isGround == true;
		}
		else {
			if (isGround == false) {
				LVelocity.y -= 9.81f * deltaTime;
			}
		}

		//dron is zerogravity 
		if (m_monsterType != MonsterType::Dron) {
			if (isGround == false) {
				LVelocity.y -= 9.81f * deltaTime;
			}
		}
		else {
			LVelocity.y = 0.0f;
		}
	
		if (worldMat.pos.y < -50.0f) {
			worldMat.pos.y = 100.0f;
			LVelocity.y = 0;
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
				if (gameworld.clients[i].zoneId != zoneId) continue;
				if (gameworld.clients[i].pObjData == nullptr) continue;
				if (gameworld.clients[i].objindex < 0) continue;
				if (gameworld.clients[i].objindex >= zone->Dynamic_gameObjects.size) continue;
				if (zone->Dynamic_gameObjects.isnull(gameworld.clients[i].objindex)) continue;
				Target = (Player**)&zone->Dynamic_gameObjects[gameworld.clients[i].objindex];
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
			m_isMove = m_speed > 0.0f;

			// A* 경로가 없거나, 다 소비했으면 새로 계산
			if (m_speed > 0.0f && (path.empty() || currentPathIndex >= path.size())) {
				AstarNode* start = FindClosestNode(monsterPos.x, monsterPos.z, zone->allnodes);
				AstarNode* goal = FindClosestNode(playerPos.x, playerPos.z, zone->allnodes);

				if (start && goal) {
					path = AstarSearch(start, goal, zone->allnodes);
					currentPathIndex = 0;
				}
			}

			// A* 경로가 있으면 그 경로를 따라 이동
			if (m_speed > 0.0f && !path.empty() && currentPathIndex < path.size()) {
				MoveByAstar(deltaTime);
			}
			else if (m_speed > 0.0f) {
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

				zone->FireRaycast(this, rayStart, rayDirection, m_chaseRange, Attack);
			}
		}
		else {
			if (m_speed <= 0.0f || m_patrolRange <= 0.0f) {
				tickLVelocity.x = 0;
				tickLVelocity.z = 0;
				m_isMove = false;
				path.clear();
				currentPathIndex = 0;
			}
			else
			if (!m_isMove) {
				float randomAngle = ((float)rand() / RAND_MAX) * 2.0f * XM_PI;
				float randomRadius = ((float)rand() / RAND_MAX) * m_patrolRange;

				m_targetPos.x = m_homePos.x + randomRadius * cos(randomAngle);
				m_targetPos.z = m_homePos.z + randomRadius * sin(randomAngle);
				m_targetPos.y = m_homePos.y;

				m_isMove = true;
				m_patrolTimer = 0.0f;

				AstarNode* start = FindClosestNode(monsterPos.x, monsterPos.z, zone->allnodes);
				AstarNode* goal = FindClosestNode(m_targetPos.x, m_targetPos.z, zone->allnodes);

				if (start && goal) {
					path = AstarSearch(start, goal, zone->allnodes);
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

		if (zone->lowHit()) {
			zone->Sending_ChangeGameObjectMember<matrix>(zone->CommonSDS, zone->currentIndex, this, GameObjectType::_Monster, &(worldMat));
		}
	}
}

void Monster::OnCollision(GameObject* other)
{
	collideCount += 1;
	float belowDist = 0;
	BoundingOrientedBox otherOBB = other->GetOBB();

	BoundingOrientedBox bottomObb = GetOBB();
	bottomObb.Center.y += tickLVelocity.y;

	if (bottomObb.Intersects(otherOBB)) {
		LVelocity.y = 0;
		isGround = true;
	}
}

void Monster::OnStaticCollision(BoundingOrientedBox obb)
{
	collideCount += 1;
	float belowDist = 0;
	BoundingOrientedBox otherOBB = obb;

	BoundingOrientedBox bottomObb = GetOBB();
	bottomObb.Center.y += tickLVelocity.y;

	if (bottomObb.Intersects(otherOBB)) {
		LVelocity.y = 0;
		isGround = true;
	}
}

void Monster::OnCollisionRayWithBullet(GameObject* shooter, float damage)
{
	Zone* zone = gameworld.GetZone(zoneId);
	
	// Monster take damage with player's Attack
	void* vptr = *(void**)shooter;
	if (GameObjectType::VptrToTypeTable[vptr] == GameObjectType::_Player) {
		Player* p = (Player*)shooter;
		damage = p->Attack;
		cout << "Player Attack: " << damage << endl;
	}

	// Monster Reduce Damage With Defense
	float defense = Defense;
	damage *= 100.0f / (100.0f + defense);
	HP -= damage;

	zone->Sending_ChangeGameObjectMember<float>(zone->CommonSDS, zone->currentIndex, this, GameObjectType::_Monster, &HP);

	if (HP <= 0 && isDead == false) {
		isDead = true;
		zone->Sending_ChangeGameObjectMember<bool>(zone->CommonSDS, zone->currentIndex, this, GameObjectType::_Monster, &isDead);

		vec4 prevpos = worldMat.pos;

		// when monster is dead, player's killcount +1
		void* vptr = *(void**)shooter;
		if (GameObjectType::VptrToTypeTable[vptr] == GameObjectType::_Player) {
			Player* p = (Player*)shooter;
			p->KillCount += 1;
			zone->Sending_ChangeGameObjectMember<int>(gameworld.clients[p->clientIndex].PersonalSDS, gameworld.clients[p->clientIndex].objindex, p, GameObjectType::_Player, &p->KillCount);
		}

		//when monster is dead, loot random items;
		ItemLoot il = {};
		il.itemDrop.id = 1 + (rand() % (ItemTable.size() - 1));
		il.itemDrop.ItemCount = 1 + rand() % 5;
		il.pos = prevpos;
		int newindex = zone->DropedItems.Alloc();
		zone->DropedItems[newindex] = il;
		zone->Sending_ItemDrop(zone->CommonSDS, newindex, il);
	}
}

void Monster::ApplyDamage(GameObject* source, float damage)
{
	if (isDead || damage <= 0.0f) return;

	Zone* zone = gameworld.GetZone(zoneId);
	if (zone == nullptr) return;

	float defense = Defense;
	damage *= 100.0f / (100.0f + defense);
	HP -= damage;
	zone->Sending_ChangeGameObjectMember<float>(zone->CommonSDS, zone->currentIndex, this, GameObjectType::_Monster, &HP);

	if (HP <= 0 && isDead == false) {
		isDead = true;
		zone->Sending_ChangeGameObjectMember<bool>(zone->CommonSDS, zone->currentIndex, this, GameObjectType::_Monster, &isDead);

		vec4 prevpos = worldMat.pos;
		if (source != nullptr) {
			void* vptr = *(void**)source;
			if (GameObjectType::VptrToTypeTable[vptr] == GameObjectType::_Player) {
				Player* p = (Player*)source;
				p->KillCount += 1;
				zone->Sending_ChangeGameObjectMember<int>(gameworld.clients[p->clientIndex].PersonalSDS, gameworld.clients[p->clientIndex].objindex, p, GameObjectType::_Player, &p->KillCount);
			}
		}

		ItemLoot il = {};
		il.itemDrop.id = 1 + (rand() % (ItemTable.size() - 1));
		il.itemDrop.ItemCount = 1 + rand() % 5;
		il.pos = prevpos;
		int newindex = zone->DropedItems.Alloc();
		zone->DropedItems[newindex] = il;
		zone->Sending_ItemDrop(zone->CommonSDS, newindex, il);
	}
}

void Monster::Init(const XMMATRIX& initialWorldMatrix)
{
	SetWorld(initialWorldMatrix);
	m_homePos = worldMat.pos;
}

void Monster::Respawn()
{
	Zone* zone = gameworld.GetZone(zoneId);
	Init(XMMatrixTranslation(rand() % 80 - 40, 10.0f, rand() % 80 - 40));
	while (zone->map.isStaticCollision(GetOBB())) {
		Init(XMMatrixTranslation(rand() % 80 - 40, 10.0f, rand() % 80 - 40));
	}
	m_isMove = false;
	zone->Sending_ChangeGameObjectMember<vec4>(zone->CommonSDS, zone->currentIndex, this, GameObjectType::_Monster, &worldMat);
	isDead = false;
	zone->Sending_ChangeGameObjectMember<bool>(zone->CommonSDS, zone->currentIndex, this, GameObjectType::_Monster, &isDead);
	HP = MaxHP;
	zone->Sending_ChangeGameObjectMember<float>(zone->CommonSDS, zone->currentIndex, this, GameObjectType::_Monster, &HP);
}

BoundingOrientedBox Monster::GetOBB()
{
	BoundingOrientedBox obb_local = BoundingOrientedBox({ 0, 1.0f, 0 }, { 0.3f, 1.0f, 0.3f }, vec4(0, 0, 0, 1));
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
8방향(상하좌우 + 대각선)을 모두 검사한다.
맵 범위를 벗어난 노드는 제외한다.
이동 불가(cango==false) 노드는 제외한다.

인자 :
current : 이웃을 구하려는 현재 노드.
allNodes : 전체 노드.
gridWidth : 그리드 가로 크기.
gridHeight : 그리드 세로 크기.

return :
vector<AstarNode*> neighbor (이동 가능한 이웃 노드 목록)
if current가 가장자리면 -> 범위 검사로 이웃 개수 감소
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
- A* 알고리즘으로 start -> destination 최단 경로를 찾아 "노드 리스트(path)"를 반환한다.
- 노드마다 g/h/f 값과 parent를 저장하며, 목적지를 찾은 뒤 parent를 따라 거꾸로 경로를 복원한다.

인자 :
<start> : 시작 노드(현재 위치를 FindClosestNode로 찾은 노드).
<destination> : 목적지 노드(플레이어/목표 좌표를 FindClosestNode로 찾은 노드).
<allNodes> : 전체 노드 목록(매 탐색마다 모든 노드 parent 초기화에 사용).

return :
vector<AstarNode*> (start에서 destination까지의 노드 경로)
if start == nullptr or destination == nullptr -> 빈 배열 반환
if openList가 비어서 destination을 찾지 못함 -> 빈 배열 반환(경로 없음)
if destination 도달 -> parent를 따라 거꾸로 간 뒤 reverse하여 정방향 경로 반환

1) 모든 노드의 gCost를 무한대로, parent를 nullptr로 초기화한다(이전 탐색 정보 제거).
2) start를 openList에 넣고 g=0, h=거리(맨해튼), f=g+h를 계산한다.
3) openList에서 f가 가장 낮은 노드를 current로 뽑는다.
4) current가 destination이면, current에서 parent를 따라가며 경로를 만들어 반환한다.
5) 아니면 current를 closedList로 옮긴다.
6) current의 이웃들을 검사하고, 더 짧은 비용(tentativeG)이 나오면 neighbor의 parent/g/h/f를 갱신한다.
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
AstarSearch가 만들어준 path를 따라 이동.
path[currentPathIndex] 노드 방향으로 이동한다.
목표 노드에 가까워지면 currentPathIndex++ 하여 다음 노드를 목표로 한다.

인자 :
deltaTime : speed*deltaTime으로 이동량을 계산한다.

1) 현재 목표 노드 = path[currentPathIndex]를 얻는다.
2) 현재 위치에서 목표 노드 월드좌표(worldx, worldz)까지의 방향(dir)을 구한다.
3) 목표 근처까지 오면 다음 경로로 넘어간다.
4) 아니면 dir을 정규화하고, tickLVelocity = dir * m_speed * deltaTime으로 이번 프레임 이동량을 만든다.
5) 바라보는 방향(look)을 dir로 맞춘다.
*/

//AI involved Code Start : <chatgpt>
void Monster::MoveByAstar(float deltaTime)
{
	Zone* zone = gameworld.GetZone(zoneId);
	if (path.empty() || currentPathIndex >= path.size())
		return;

	AstarNode* targetNode = path[currentPathIndex];

	// 현재 몬스터 위치
	vec4 pos = worldMat.pos;
	pos.w = 1.0f;

	// A*에서 지정한 타겟 노드 위치 (y는 현재 높이 유지)
	vec4 target(targetNode->worldx, pos.y, targetNode->worldz, 1.0f);
	if (zone->AstarStartX > target.x) target.x = zone->AstarStartX;
	if (zone->AstarStartZ > target.z) target.z = zone->AstarStartZ;
	if (zone->AstarEndX < target.x) target.x = zone->AstarEndX;
	if (zone->AstarEndZ < target.z) target.z = zone->AstarEndZ;

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

void Monster::SendGameObject(int objindex, SendDataSaver& sds) {
	sds.postpush_start();

	//calculate app packet siz.
	int reqsiz = sizeof(STC_SyncGameObject_Header) + sizeof(STC_SyncObjData);
	Zone zone = gameworld.zones[zoneId];
	Shape& s = zone.GetShape(shapeindex);
	Mesh* mesh = nullptr;
	Model* model = nullptr;
	s.GetRealShape(mesh, model);
	if (mesh) reqsiz += sizeof(int) + sizeof(int) * mesh->subMeshNum;
	else reqsiz += sizeof(int) + sizeof(matrix) * model->nodeCount;

	sds.postpush_reserve(reqsiz);
	int offset = 0;

	//static pushv
	STC_SyncGameObject_Header& header = *(STC_SyncGameObject_Header*)sds.ofbuff;
	header.size = reqsiz;
	header.st = STC_Protocol::SyncGameObject;
	header.objindex = objindex;
	header.type = GameObjectType::_Monster;
	offset += sizeof(STC_SyncGameObject_Header);

	STC_SyncObjData& static_data = *(STC_SyncObjData*)(sds.ofbuff + offset);
	static_data.tag = tag;
	static_data.shapeindex = shapeindex;
	static_data.parent = parent;
	static_data.childs = childs;
	static_data.sibling = sibling;
	static_data.DestWorld = worldMat;
	static_data.LVelocity = LVelocity;
	static_data.AnimationFlowTime = AnimationFlowTime;
	static_data.PlayingAnimationIndex = PlayingAnimationIndex;
	static_data.HP = HP;
	static_data.MaxHP = MaxHP;
	static_data.isDead = isDead;
	static_data.Defense = Defense;
	offset += sizeof(STC_SyncObjData);

	//dynamic push

	//shape
	if (mesh) {
		int& submeshNum = *(int*)(sds.ofbuff + offset);
		submeshNum = mesh->subMeshNum;
		offset += sizeof(int);

		int*& MaterialArr = *(int**)(sds.ofbuff + offset);
		memcpy(MaterialArr, material, sizeof(int) * mesh->subMeshNum);
		offset += sizeof(int) * mesh->subMeshNum;
	}
	else {
		int& nodecount = *(int*)(sds.ofbuff + offset);
		nodecount = model->nodeCount;
		offset += sizeof(int);

		matrix* InnerModelTransformArr = (matrix*)(sds.ofbuff + offset);
		memcpy_s(InnerModelTransformArr, sizeof(matrix) * model->nodeCount, transforms_innerModel, sizeof(matrix) * model->nodeCount);
		offset += sizeof(matrix) * model->nodeCount;
	}

	sds.postpush_end();
}

/*<설명>
- 월드 좌표(wx, wz)에 가장 가까운 "이동 가능 노드(cango==true)"를 찾아 반환한다.
- 월드 좌표를 그리드에 맞추지 않아도 되도록, A*의 start/goal 노드를 고르는 보조 함수.

인자 :
wx : 월드 X 좌표.
wz : 월드 Z 좌표.
allNodes : 전체 노드 목록(이동 가능한 노드를 찾기 위해 전체 순회).

return :
if 이동 가능 노드가 하나도 없으면 -> nullptr 반환
if 후보 노드가 있으면 -> 거리^2(dx^2+dz^2)이 최소인 노드 반환

1) allNodes를 돌며 cango==true인 노드만 본다.
2) (node.worldx - wx)^2 + (node.worldz - wz)^2 이 가장 작은 노드를 갱신한다.
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

BoundingOrientedBox Portal::GetOBB() {
	BoundingOrientedBox obb_local;
	Mesh* mesh = nullptr;
	Model* model = nullptr;
	if (shapeindex < 0) {
		obb_local.Extents.x = -1;
		return obb_local;
	}
	Zone zone = gameworld.zones[zoneId];
	Shape& s = zone.GetShape(shapeindex);
	s.GetRealShape(mesh, model);
	if (mesh != nullptr) obb_local = mesh->GetOBB();
	if (model != nullptr) obb_local = model->GetOBB();
	BoundingOrientedBox obb_world;
	obb_local.Transform(obb_world, worldMat);
	return obb_world;
};

void Portal::SendGameObject(int objindex, SendDataSaver& sds) {
	sds.postpush_start();

	// mesh/model 없이 기본 데이터만 전송
	int reqsiz = sizeof(STC_SyncGameObject_Header) + sizeof(Portal::STC_SyncObjData);
	sds.postpush_reserve(reqsiz);
	int offset = 0;

	STC_SyncGameObject_Header& header = *(STC_SyncGameObject_Header*)sds.ofbuff;
	header.size = reqsiz;
	header.st = STC_Protocol::SyncGameObject;
	header.objindex = objindex;
	header.type = GameObjectType::_Portal;
	offset += sizeof(STC_SyncGameObject_Header);

	Portal::STC_SyncObjData& static_data = *(Portal::STC_SyncObjData*)(sds.ofbuff + offset);
	static_data.tag = tag;
	static_data.shapeindex = shapeindex;
	static_data.parent = parent;
	static_data.childs = childs;
	static_data.sibling = sibling;
	static_data.DestWorld = worldMat;
	static_data.spawnX = spawnX;
	static_data.spawnY = spawnY;
	static_data.spawnZ = spawnZ;
	static_data.radius = radius;
	static_data.zoneId = zoneId;
	static_data.dstzoneId = dstzoneId;
	offset += sizeof(STC_SyncObjData);

	sds.postpush_end();
}


BoundingBox ChunkIndex::GetAABB(Zone* zone) {
	BoundingBox AABB;
	float halfW = zone->chunck_divide_Width * 0.5f;
	AABB.Center = XMFLOAT3(zone->chunck_divide_Width * x + halfW, zone->chunck_divide_Width * y + halfW, zone->chunck_divide_Width * z + halfW);
	AABB.Extents = XMFLOAT3(halfW, halfW, halfW);
	return AABB;
}

void GameChunk::SetChunkIndex(ChunkIndex ci, Zone* zone) {
	cindex = ci;
	AABB = ci.GetAABB(zone);
}

void World::Init() {
#ifdef DEVELOPMODE_SYNC_GLOBAL_ASSET
	ifstream GlobalAssetCounter{ "../../GlobalAssetCounter.txt" };
	if (GlobalAssetCounter.is_open()) {
		GlobalAssetCounter >> GlobalTextureCount;
		GlobalAssetCounter >> GlobalMaterialSiz;
		GlobalAssetCounter >> GlobalMeshCount;
		GlobalAssetCounter >> GlobalHumanoidAnimaionCount;
		GlobalAssetCounter >> GlobalShapeTableSyncSiz;
	}
	GlobalAssetCounter.close();
#endif

	clients.Init(32);
	GameObjectType::STATICINIT();

	// 전역 모델 로드
	{
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
		int playerMesh_index = Shape::AddModel("Player", PlayerModel);

		Model* MonsterModel = new Model();
		MonsterModel->LoadModelFile2("Resources/Model/Exo.model");
		int monsterMesh_index = Shape::AddModel("Monster001", MonsterModel);

		Mesh* portalMesh = new Mesh();
		portalMesh->CreateWallMesh(2.0f, 3.0f, 0.2f);
		Shape::AddMesh("Portal", portalMesh);

		//Item 리소스
		{
			int globalitem_index = 0;
			Shape BlackShape;
			BlackShape.FlagPtr = 0;
			ItemTable.push_back(Item(globalitem_index, ItemType::_NULL, L"")); // blank space in inventory.
			globalitem_index += 1;

			auto AddItemFunc = [&](int id, ItemType t, const char* ItemName, const char* modelfilepath, const wchar_t* ItemIconPath, const wchar_t* ItemDescription) {
				/*Model* ItemModel = new Model();
				ItemModel->LoadModelFile2(modelfilepath);
				int Item_shapeindex = Shape::AddModel(ItemName, ItemModel);
				GPUResource* ItemIcon = new GPUResource();
				ItemIcon->CreateTexture_fromFile(ItemIconPath, game.basicTexFormat, game.basicTexMip, false);*/
				ItemTable.push_back(Item(globalitem_index, t, ItemDescription));
				};

			AddItemFunc(globalitem_index, ItemType::_Consumable, "BioFix",
				"Resources/Model/ItemModel/BioFix.model",
				L"Resources/UI/ItemIcons/ItemIcon_BioFix.png",
				L"[바이오픽스] : 신체 조직을 재생하는 가성비 의료 주사기! \n HP+20");
			globalitem_index += 1;

			AddItemFunc(globalitem_index, ItemType::_Consumable, "Tier4Gear",
				"Resources/Model/ItemModel/Tier4Gear.model",
				L"Resources/UI/ItemIcons/ItemIcon_Tear4Gear.png",
				L"[티어4 부품] : 티어 4 도구 제작에 사용되는 주요재료. 많이 모으면 무기나 도구를 제작하는데 도움이 된다.");
			globalitem_index += 1;

			AddItemFunc(globalitem_index, ItemType::_Weapon, "Sniper_IronSight",
				"Resources/Model/sniper.model",
				L"Resources/UI/ItemIcons/ItemIcon_IronSight.png",
				L"[스나이퍼 - 아이언사이트] : 광학 장비조차 제대로 달리지 않은 구식 실탄 저격소총.");
			globalitem_index += 1;

			// 라이플 모델 로드
			AddItemFunc(globalitem_index, ItemType::_Weapon, "Rifle_StreetSweeper",
				"Resources/Model/Rifle.model",
				L"Resources/UI/ItemIcons/ItemIcon_StreetSweeper.png",
				L"[돌격소총 - 스트리트스위퍼] : 한때 거리를 쓸어버렸던 클래식 돌격소총. 이제는 흔해 빠진 모델이다.");
			globalitem_index += 1;

			// 샷건 모델 로드
			AddItemFunc(globalitem_index, ItemType::_Weapon, "Pistol_DoubleTroble",
				"Resources/Model/pistol.model",
				L"Resources/UI/ItemIcons/ItemIcon_DoubleTroble.png",
				L"[쌍권총 - 더블트러블] : 성능은 낮지만 두 배로 쏘아붙이는 저가형 쌍권총.");
			globalitem_index += 1;

			// 샷건 모델 로드
			AddItemFunc(globalitem_index, ItemType::_Weapon, "ShotGun_SlagShot",
				"Resources/Model/shootgun.model",
				L"Resources/UI/ItemIcons/ItemIcon_SlagShot.png",
				L"[샷건 - 슬래그슛] : 제련 찌꺼기를 쏘는 듯한 거칠고 어려운 총. 구형 모델이라 성능도 그다지 좋지 않다.");
			globalitem_index += 1;

			// 머신건(미니건) 모델 로드
			AddItemFunc(globalitem_index, ItemType::_Weapon, "MachineGun_Ratler",
				"Resources/Model/minigun.model",
				L"Resources/UI/ItemIcons/ItemIcon_Ratler.png",
				L"[머신건 - 라틀러] : 낡은 부품이 덜덜거리는 소리에서 따온 이름. 언제 만들어졌는지 알 수 없다.");
			globalitem_index += 1;
		}
	}

	/*
	{
		ItemTable.push_back(Item(0));
		ItemTable.push_back(Item(1));
		ItemTable.push_back(Item(2));
		ItemTable.push_back(Item(3));
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
		Model* SniperModel = nullptr;
		if (kLoadSniperModel) {
			SniperModel = new Model;
			SniperModel->LoadModelFile2("Resources/Model/sniper.model");
		}

		// 라이플 모델 로드
		Model* RifleModel = nullptr;
		if (kLoadRifleModel) {
			RifleModel = new Model;
			RifleModel->LoadModelFile2("Resources/Model/Rifle.model");
		}

		// 권총 모델 로드
		Model* PistolModel = nullptr;
		if (kLoadPistolModel) {
			PistolModel = new Model;
			PistolModel->LoadModelFile2("Resources/Model/pistol.model");
		}

		// 샷건 모델 로드
		Model* ShotGunModel = nullptr;
		if (kLoadShotGunModel) {
			ShotGunModel = new Model;
			ShotGunModel->LoadModelFile2("Resources/Model/shootgun.model");
		}

		//// 머신건(미니건) 모델 로드
		Model* MachineGunModel = nullptr;
		if (kLoadMachineGunModel) {
			MachineGunModel = new Model;
			MachineGunModel->LoadModelFile2("Resources/Model/minigun.model");
		}

		Model* PlayerModel = new Model();
		PlayerModel->LoadModelFile2("Resources/Model/Remy.model");
		int playerMesh_index = Shape::AddModel("Player", PlayerModel);

		Model* MonsterModel = new Model();
		MonsterModel->LoadModelFile2("Resources/Model/Exo.model");
		int monsterMesh_index = Shape::AddModel("Monster001", MonsterModel);

		Mesh* portalMesh = new Mesh();
		portalMesh->CreateWallMesh(2.0f, 3.0f, 0.2f);
		Shape::AddMesh("Portal", portalMesh);
	}
	*/

	cout << "Game Init end" << endl;
	cout << "=== Shape Index Check ===" << endl;
	for (int i = 0; i < Shape::ShapeTable.size(); ++i) {
		const string& str = Shape::ShapeStrTable[i];
		cout << str << " = " << Shape::StrToShapeIndex[str] << endl;
	}
	cout << "Total shapes = " << Shape::ShapeTable.size() << endl;

#ifdef DEVELOPMODE_SYNC_GLOBAL_ASSET
	dbgWarn(GlobalMaterialCount != GlobalMaterialSiz,
        cout << "WARN : 서버와 클라이언트 간에 로드된 에셋 배열의 항목중 일치하지 않는 것이 있습니다" << endl;);
	dbgWarn(Shape::ShapeTable.size() != GlobalShapeTableSyncSiz,
        cout << "WARN : 서버와 클라이언트 간에 로드된 Shape 개수가 일치하지 않습니다." << endl;);
	GlobalMaterialCount = GlobalMaterialSiz;
#endif

	zones.resize(zoneCount);
	// Zone 초기화
	for (int i = 0; i < zoneCount; ++i) {
		zones[i].world = this;
		zones[i].zoneId = i;
		if (IsZoneOwned(i) == false) continue;
		zones[i].Init();
	}
	for (int i = 0; i < zoneCount; ++i) {
		if (IsZoneOwned(i) == false) continue;
		zones[i].SpawnObjects();
		zones[i].Spawnboundary();
	}
}

void World::Update() {	
	for (int i = 0; i < zoneCount; ++i) {
		if (IsZoneOwned(i) == false) continue;
		zones[i].Update(DeltaTime);
	}
	for (int i = 0; i < zoneCount; ++i) {
		if (IsZoneOwned(i) == false) continue;
		zones[i].FlushSendToClients();
	}
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

//클라이언트 인덱스에 해당하는 존 찾기

bool World::SendPlayerTransferToServer(const PlayerTransferData& data) {
	SOCKET sock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, 0);
	if (sock == INVALID_SOCKET) return false;

	sockaddr_in serveraddr = {};
	serveraddr.sin_family = AF_INET;
	inet_pton(AF_INET, GetZoneIP(data.dstZoneId), &serveraddr.sin_addr);
	serveraddr.sin_port = htons(GetZonePort(data.dstZoneId));
	if (connect(sock, (sockaddr*)&serveraddr, sizeof(serveraddr)) == SOCKET_ERROR) {
		closesocket(sock);
		return false;
	}

	CTS_ServerPlayerTransfer_Header header = {};
	header.size = sizeof(CTS_ServerPlayerTransfer_Header);
	header.st = CTS_Protocol::ServerPlayerTransfer;
	header.data = data;

	int sendResult = send(sock, (const char*)&header, sizeof(header), 0);
	closesocket(sock);
	return sendResult == sizeof(header);
}

void World::AcceptClientHello(int clientIndex) {
	if (clientIndex < 0 || clientIndex >= clients.size) return;
	if (clients.isnull(clientIndex)) return;
	if (clients[clientIndex].pObjData != nullptr) return;

	int StartzoneId = ownedZoneId;
	clients[clientIndex].PersonalSDS.Init(4096);
	Sending_SyncGameState(clients[clientIndex].PersonalSDS);

	Player* p = new Player();
	p->clientIndex = clientIndex;
	p->worldMat.Id();
	p->worldMat.pos.f3.y = zones[StartzoneId].map.AABB[1].y - 1;
	p->SetShape(Shape::StrToShapeIndex["Player"]);
	for (int i = 0; i < 36; ++i) {
		p->Inventory[i].id = 0;
		p->Inventory[i].ItemCount = 0;
	}

	clients[clientIndex].pObjData = p;
	clients[clientIndex].zoneId = StartzoneId;
	vec4 spawnPos(0.0f, 10.0f, 0.0f, 1.0f);
	int objindex = zones[StartzoneId].AddPlayer(clientIndex, p, spawnPos);
	clients[clientIndex].objindex = objindex;
}

bool World::AcceptTransferConnect(int clientIndex, int transferToken) {
	auto f = pendingTransfers.find(transferToken);
	if (f == pendingTransfers.end()) {
		cout << "[AcceptTransferConnect] token not yet received client=" << clientIndex << " token=" << transferToken << " (will retry)" << endl;
		return false;
	}
	PlayerTransferData data = f->second;
	pendingTransfers.erase(f);

	clients[clientIndex].PersonalSDS.Init(4096);
	Sending_SyncGameState(clients[clientIndex].PersonalSDS);

	Player* p = new Player();
	p->clientIndex = clientIndex;
	p->worldMat.Id();
	p->SetShape(Shape::StrToShapeIndex["Player"]);
	p->HP = data.HP;
	p->MaxHP = data.MaxHP;
	p->Attack = data.Attack;
	p->Defense = data.Defense;
	p->bullets = data.bullets;
	p->KillCount = data.KillCount;
	p->DeathCount = data.DeathCount;
	p->HeatGauge = data.HeatGauge;
	p->MaxHeatGauge = data.MaxHeatGauge;
	p->m_currentJob = data.m_currentJob;
	memcpy(p->SkillCooldown, data.SkillCooldown, sizeof(p->SkillCooldown));
	memcpy(p->SkillCooldownFlow, data.SkillCooldownFlow, sizeof(p->SkillCooldownFlow));
	p->m_currentWeaponType = data.m_currentWeaponType;
	p->weapon = Weapon((WeaponType)data.m_currentWeaponType);
	p->m_yaw = data.yaw;
	p->m_pitch = data.pitch;
	for (int i = 0; i < 36; ++i) {
		p->Inventory[i] = data.Inventory[i];
	}

	clients[clientIndex].pObjData = p;
	clients[clientIndex].zoneId = data.dstZoneId;
	int objindex = zones[data.dstZoneId].AddPlayer(clientIndex, p, data.spawnPos, false);
	clients[clientIndex].objindex = objindex;
	return true;
}

void World::StoreIncomingPlayerTransfer(const PlayerTransferData& data) {
	pendingTransfers[data.transferToken] = data;
}
Zone* World::GetClientZone(int clientIndex)
{
	if (clientIndex < 0 || clientIndex >= clients.size) return nullptr;
	if (clients.isnull(clientIndex)) return nullptr;

	int zid = clients[clientIndex].zoneId;
	if (zid < 0 || zid >= zoneCount) return nullptr;

	return &zones[zid];
}

void World::PrintOffset() {
	ofstream ofs{ "../../SyncFPSClient/SyncFPSClient/STC_GameObjectOffsets.txt" };
	ofs << "GameObject" << endl;
	ofs << GameObject::g_member.size() << endl;
	GameObject::PrintOffset(ofs);

	ofs << "StaticGameObject" << endl;
	ofs << StaticGameObject::g_member.size() << endl;
	StaticGameObject::PrintOffset(ofs);

	ofs << "DynamicGameObject" << endl;
	ofs << GameObject::g_member.size() + DynamicGameObject::g_member.size() << endl;
	DynamicGameObject::PrintOffset(ofs);

	ofs << "SkinMeshGameObject" << endl;
	ofs << GameObject::g_member.size() + DynamicGameObject::g_member.size() + SkinMeshGameObject::g_member.size() << endl;
	SkinMeshGameObject::PrintOffset(ofs);

	ofs << "Player" << endl;
	ofs << GameObject::g_member.size() + DynamicGameObject::g_member.size() + SkinMeshGameObject::g_member.size() + Player::g_member.size() << endl;
	Player::PrintOffset(ofs);

	ofs << "Monster" << endl;
	ofs << GameObject::g_member.size() + DynamicGameObject::g_member.size() + SkinMeshGameObject::g_member.size() + Monster::g_member.size() << endl;
	Monster::PrintOffset(ofs);
	
	ofs << "Portal" << endl;
	ofs << GameObject::g_member.size() + Portal::g_member.size() << endl;
	Portal::PrintOffset(ofs);

	ofs.close();
}

void ClientData::DisconnectToServer(int index) {
	if (index < 0 || index >= gameworld.clients.size) return;
	if (gameworld.clients.isnull(index)) return;

	ClientData& client = gameworld.clients[index];
	Player* player = client.pObjData;

	if (client.socket != INVALID_SOCKET) {
		closesocket(client.socket);
		client.socket = INVALID_SOCKET;
	}

	if (player != nullptr) {
		Zone* zone = gameworld.GetClientZone(index);
		if (zone != nullptr && gameworld.IsZoneOwned(client.zoneId)) {
			zone->RemovePlayer(index);
		}
		delete player;
	}

	client.pObjData = nullptr;
	client.objindex = -1;
	client.rbufoffset = 0;
	memset(client.rbuf, 0, sizeof(client.rbuf));
	client.PersonalSDS.Clear();
	gameworld.clients.Free(index);

	cout << "client " << index << " Left the Game. \n";
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

void World::MovePlayerToZone(int clientIndex, int dstZoneId, vec4 spawnPos) {
	if (clientIndex < 0 || clientIndex >= clients.size) return;
	if (clients.isnull(clientIndex)) return;
	if (dstZoneId < 0 || dstZoneId >= zoneCount) return;

	int srcZoneId = clients[clientIndex].zoneId;
	if (srcZoneId < 0 || srcZoneId >= zoneCount) return;
	if (srcZoneId == dstZoneId) return;

	Player* player = clients[clientIndex].pObjData;
	if (player == nullptr) return;

	cout << "[ZoneMove] client=" << clientIndex
		<< " from=" << srcZoneId
		<< " to=" << dstZoneId << endl;

	if (IsZoneOwned(dstZoneId) == false) {
		PlayerTransferData data = {};
		data.transferToken = IssueTransferToken();
		data.dstZoneId = dstZoneId;
		data.spawnPos = spawnPos;
		data.yaw = player->m_yaw;
		data.pitch = player->m_pitch;
		data.HP = player->HP;
		data.MaxHP = player->MaxHP;
		data.Attack = player->Attack;
		data.Defense = player->Defense;
		data.bullets = player->bullets;
		data.KillCount = player->KillCount;
		data.DeathCount = player->DeathCount;
		data.HeatGauge = player->HeatGauge;
		data.MaxHeatGauge = player->MaxHeatGauge;
		data.m_currentJob = player->m_currentJob;
		memcpy(data.SkillCooldown, player->SkillCooldown, sizeof(data.SkillCooldown));
		memcpy(data.SkillCooldownFlow, player->SkillCooldownFlow, sizeof(data.SkillCooldownFlow));
		data.m_currentWeaponType = player->m_currentWeaponType;
		for (int i = 0; i < 36; ++i) {
			data.Inventory[i] = player->Inventory[i];
		}

		if (SendPlayerTransferToServer(data) == false) {
			cout << "[ZoneMove] remote transfer send failed. dstZone=" << dstZoneId << endl;
			return;
		}

		Sending_ServerTransfer(clients[clientIndex].PersonalSDS, dstZoneId, GetZoneIP(dstZoneId), GetZonePort(dstZoneId), data.transferToken);
		zones[srcZoneId].RemovePlayer(clientIndex);
		delete player;
		clients[clientIndex].pObjData = nullptr;
		clients[clientIndex].objindex = -1;
		return;
	}

	zones[srcZoneId].RemovePlayer(clientIndex);
	zones[dstZoneId].AddPlayer(clientIndex, player, spawnPos);
}


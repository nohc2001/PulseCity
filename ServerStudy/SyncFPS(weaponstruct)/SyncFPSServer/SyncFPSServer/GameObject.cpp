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
			//��ǥ
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

	// fbx�� �⺻ ũ�� ������ 100 �̱� ������ �̷��� ��������.
	// ������ UnitScale�� �ٸ� ��쿡�� ��� �ϴ°�?
	// ��ġ�� �ʿ��ϴ�.
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
	header.zoneId = zoneId;
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
		obj = gameworld.commonMap.MapObjects[temp];
	}
	
	if (obj != nullptr) {
		sav *= obj->worldMat;
		temp = obj->parent;
		obj = nullptr;
		if (temp > 0) {
			obj = gameworld.commonMap.MapObjects[temp];
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
	header.zoneId = zoneId;
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

	//���� ûũ���� ���� �����
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
		// �� ��ġ���� ûũ�� �ֱ�
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
		if (ci == inter_ci) { // ��ġ�� �κ��� Free ���� �ʴ´�.
			intersection.Inc(inter_ci);
			temp[inter_up] = chunkAllocIndexs[ci.extra];
			inter_up += 1;
			continue;
		}

		// �� ��ġ�� �κ��� Free �Ѵ�.
		auto f = zone->chunck.find(ci);
		if (f != zone->chunck.end()) {
#ifdef DEVELOPMODE_ChunckDEBUG
			if (f->second->Dynamic_gameobjects.isnull(chunkAllocIndexs[ci.extra])) cout << "[WARN] Dynamic chunk free skipped invalid slot\n";
#endif
			if (f->second->Dynamic_gameobjects.isnull(chunkAllocIndexs[ci.extra]) == false) f->second->Dynamic_gameobjects.Free(chunkAllocIndexs[ci.extra]);
		}
	}

	// ��ġ �̵� / ȸ��
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
		if (ci == inter_ci) { // ��ġ�� �κ��� Alloc ���� �ʴ´�.
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
	header.zoneId = zoneId;
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
		if (ci == inter_ci && inter_Count > 0) { // ��ġ�� �κ��� Free ���� �ʴ´�.
			intersection.Inc(inter_ci);
			temp[inter_up] = chunkAllocIndexs[ci.extra];
			inter_up += 1;
			
			continue;
		}
		// �� ��ġ�� �κ��� Free �Ѵ�.
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



	// ��ġ �̵� / ȸ��
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
		if (ci == inter_ci && inter_Count > 0) { // ��ġ�� �κ��� Alloc ���� �ʴ´�.
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
	header.zoneId = zoneId;
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

	//���� ûũ���� ���� �����
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
		// �� ��ġ���� ûũ�� �ֱ�
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

	// ������ ���� ûũ������ ã�Ƴ�.
	GameObjectIncludeChunks goic = ownerzone->GetChunks_Include_OBB(obb);

	for (int ix = goic.xmin; ix <= goic.xmin + goic.xlen; ++ix) {
		for (int iy = goic.ymin; iy <= goic.ymin + goic.ylen; ++iy) {
			for (int iz = goic.zmin; iz <= goic.zmin + goic.zlen; ++iz) {
				auto chun = ownerzone->chunck.find(ChunkIndex(ix, iy, iz));
				if (chun == ownerzone->chunck.end()) continue;
				GameChunk* ch = chun->second;
				//Static Object�� Enable�� false�� �� ���� ������ �Ҵ�˻�� ����.
				// >> �׷� �׳� vector���� ��������� �� vecset���� ��? >> fix

				//��¥�� ������ ûũ�� �� ����°� ���� ������? �׷� nullptr üũ�� �� �ʿ䰡 �ֳ�?
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

	TextureTableStart = gameworld.GlobalTextureCount; // fix : ���� MaterialTexture ����
	MaterialTableStart = gameworld.GlobalMaterialCount; // fix : ���� Material ����

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
		// .map (Ȯ����)����
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
		//// .map (Ȯ����)����
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
		// .map (Ȯ����)����
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
			//	// fix. ����Ƽ �ڽ� ������Ʈ���� AABB�� ��� ������? ���������� ������ �޴��� Ȯ���� �ʿ�.
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
	ShieldDurability = 0;
	MaxShieldDurability = 100;
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
	m_juggernautFlameTimer = 0.0f;
	m_juggernautFlameTickFlow = 0.0f;
	m_juggernautFlameEffectFlow = 0.0f;
	m_juggernautFlameRange = 0.0f;
	m_juggernautFlameRadius = 0.0f;
	m_juggernautFlameDps = 0.0f;
	m_rifleGrenadeTimer = 0.0f;
	m_rifleGrenadeDuration = 0.0f;
	m_rifleGrenadeEffectFlow = 0.0f;
	m_rifleStimTimer = 0.0f;
	m_rifleStimRegenFlow = 0.0f;
	m_rifleStimEffectFlow = 0.0f;
	m_rifleMissileTimer = 0.0f;
	m_rifleMissileTickFlow = 0.0f;
	m_rifleMissileCount = 0;
	m_sniperDmrMode = false;
	m_railgunTimer = 0.0f;
	m_railgunShots = 0;
	m_dualBladeTimer = 0.0f;
	m_dualAwakenTimer = 0.0f;
	m_aegisShieldActive = false;
	m_aegisShieldPrevInput = false;
	m_aegisShieldCooldownTimer = 0.0f;
	m_aegisShieldInactiveTimer = 0.0f;
	m_aegisShieldEffectFlow = 0.0f;
	m_aegisRepairTimer = 0.0f;
	m_aegisInvincibleTimer = 0.0f;
	m_aegisAuraEffectFlow = 0.0f;
	m_aegisChargeTimer = 0.0f;
	m_dualDashTimer = 0.0f;
	m_frostPassiveUsed = false;
	MaxShieldDurability = ((PlayerJob)m_currentJob == PlayerJob::Aegis) ? 100.0f : 0.0f;
	ShieldDurability = MaxShieldDurability;

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
	zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS,
		gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &ShieldDurability);
	zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS,
		gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &MaxShieldDurability);
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

static vec4 RotateDirectionYaw(vec4 direction, float degree)
{
	if (direction.len3 <= 0.0001f) direction = vec4(0, 0, 1, 0);
	direction.len3 = 1.0f;

	XMVECTOR quaternion = XMQuaternionRotationRollPitchYaw(0.0f, degree * XM_PI / 180.0f, 0.0f);
	return XMVector3Rotate(direction, quaternion);
}

static vec4 ApplyShotgunSpread(vec4 direction, float yawDegree, float pitchDegree)
{
	vec4 spreadDirection = RotateDirectionYaw(direction, yawDegree);
	spreadDirection.y += tanf(pitchDegree * XM_PI / 180.0f);
	if (spreadDirection.len3 <= 0.0001f) spreadDirection = direction;
	spreadDirection.len3 = 1.0f;
	return spreadDirection;
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

	if (m_juggernautFlameTimer > 0.0f) {
		m_juggernautFlameTimer -= deltaTime;
		if (m_juggernautFlameTimer < 0.0f) m_juggernautFlameTimer = 0.0f;

		XMVECTOR quaternion = XMQuaternionRotationRollPitchYaw(m_pitch, m_yaw, 0);
		vec4 direction = { 0, 0, 1, 0 };
		direction = XMVector3Rotate(direction, quaternion);

		m_juggernautFlameTickFlow += deltaTime;
		constexpr float flameTickDelay = 0.18f;
		while (m_juggernautFlameTickFlow >= flameTickDelay && m_juggernautFlameTimer > 0.0f) {
			m_juggernautFlameTickFlow -= flameTickDelay;
			zones->ApplySkillDamage(this, SkillEffectType::Juggernaut_UltimateFire, worldMat.pos, direction,
				m_juggernautFlameRange, m_juggernautFlameRadius, m_juggernautFlameDps * flameTickDelay);
		}

		m_juggernautFlameEffectFlow += deltaTime;
		if (m_juggernautFlameEffectFlow >= 0.12f || m_juggernautFlameTimer == 0.0f) {
			m_juggernautFlameEffectFlow = 0.0f;
			zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
				SkillSlot::Ultimate, SkillEffectType::Juggernaut_UltimateFire, worldMat.pos, direction,
				m_juggernautFlameRadius, m_juggernautFlameDps, 0.35f);
		}
	}
	if (m_rifleGrenadeTimer > 0.0f) {
		m_rifleGrenadeTimer -= deltaTime;
		if (m_rifleGrenadeTimer < 0.0f) m_rifleGrenadeTimer = 0.0f;

		float duration = max(m_rifleGrenadeDuration, 0.01f);
		float t = 1.0f - (m_rifleGrenadeTimer / duration);
		if (t < 0.0f) t = 0.0f;
		if (t > 1.0f) t = 1.0f;
		vec4 grenadePosition = m_rifleGrenadeOrigin + (m_rifleGrenadePosition - m_rifleGrenadeOrigin) * t;
		grenadePosition.y += sinf(t * XM_PI) * 5.2f + 1.0f;

		m_rifleGrenadeEffectFlow += deltaTime;
		if (m_rifleGrenadeEffectFlow >= 0.08f && m_rifleGrenadeTimer > 0.0f) {
			m_rifleGrenadeEffectFlow = 0.0f;
			vec4 trailDirection = m_rifleGrenadePosition - grenadePosition;
			if (trailDirection.len3 <= 0.0001f) trailDirection = vec4(0, -1, 0, 0);
			trailDirection.len3 = 1.0f;
			zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
				SkillSlot::Skill1, SkillEffectType::Rifle_GrenadeTrail, grenadePosition, trailDirection,
				0.65f, 1.0f, 0.22f);
		}

		if (m_rifleGrenadeTimer <= 0.0f) {
			zones->ApplySkillDamage(this, SkillEffectType::Rifle_TacticalGrenade, m_rifleGrenadePosition, vec4(0, 1, 0, 0),
				0.0f, m_rifleGrenadeRadius, m_rifleGrenadeDamage);
			zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
				SkillSlot::Skill1, SkillEffectType::Explosion_Blast, m_rifleGrenadePosition, vec4(0, 1, 0, 0),
				m_rifleGrenadeRadius, m_rifleGrenadeDamage, 0.9f);
		}
	}

	if (m_rifleStimTimer > 0.0f) {
		m_rifleStimTimer -= deltaTime;
		if (m_rifleStimTimer < 0.0f) m_rifleStimTimer = 0.0f;

		m_rifleStimRegenFlow += deltaTime;
		constexpr float stimHealTickDelay = 0.5f;
		while (m_rifleStimRegenFlow >= stimHealTickDelay && m_rifleStimTimer > 0.0f) {
			m_rifleStimRegenFlow -= stimHealTickDelay;
			HP += 10.0f * stimHealTickDelay;
			if (HP > MaxHP) HP = MaxHP;
			zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS,
				gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HP);
		}

		m_rifleStimEffectFlow += deltaTime;
		if (m_rifleStimEffectFlow >= 0.20f && m_rifleStimTimer > 0.0f) {
			m_rifleStimEffectFlow = 0.0f;
			zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
				SkillSlot::Skill2, SkillEffectType::Rifle_StimField, worldMat.pos, vec4(0, 1, 0, 0),
				1.45f, 1.0f, 0.35f);
		}
	}

	if (m_rifleMissileTimer > 0.0f) {
		m_rifleMissileTimer -= deltaTime;
		if (m_rifleMissileTimer < 0.0f) m_rifleMissileTimer = 0.0f;

		m_rifleMissileTickFlow += deltaTime;
		constexpr float missileTickDelay = 1.0f;
		while (m_rifleMissileTickFlow >= missileTickDelay && m_rifleMissileCount < 3) {
			m_rifleMissileTickFlow -= missileTickDelay;
			float dropDistance = 8.0f + 8.0f * (float)m_rifleMissileCount;
			vec4 dropPosition = m_rifleMissileOrigin + m_rifleMissileDirection * dropDistance;
			dropPosition.y = m_rifleMissileOrigin.y;
			vec4 skyPosition = dropPosition + vec4(0.0f, 12.0f, 0.0f, 0.0f);
			zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
				SkillSlot::Ultimate, SkillEffectType::Rifle_AirStrikeTrail, skyPosition, vec4(0, -1, 0, 0),
				1.2f, m_rifleMissileDamage, 0.65f);
			zones->ApplySkillDamage(this, SkillEffectType::Rifle_MissileBarrage, dropPosition, vec4(0, 1, 0, 0),
				0.0f, m_rifleMissileRadius, m_rifleMissileDamage);
			zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
				SkillSlot::Ultimate, SkillEffectType::Explosion_Blast, dropPosition, vec4(0, 1, 0, 0),
				m_rifleMissileRadius, m_rifleMissileDamage, 0.9f);
			++m_rifleMissileCount;
		}

		if (m_rifleMissileCount >= 3) m_rifleMissileTimer = 0.0f;
	}

	if (m_railgunTimer > 0.0f) {
		m_railgunTimer -= deltaTime;
		if (m_railgunTimer <= 0.0f) {
			m_railgunTimer = 0.0f;
			m_railgunShots = 0;
		}
	}

	if (m_dualBladeTimer > 0.0f) {
		m_dualBladeTimer -= deltaTime;
		if (m_dualBladeTimer < 0.0f) m_dualBladeTimer = 0.0f;
	}

	if (m_dualAwakenTimer > 0.0f) {
		m_dualAwakenTimer -= deltaTime;
		if (m_dualAwakenTimer < 0.0f) m_dualAwakenTimer = 0.0f;
	}

	if (m_aegisRepairTimer > 0.0f) {
		m_aegisRepairTimer -= deltaTime;
		if (m_aegisRepairTimer < 0.0f) m_aegisRepairTimer = 0.0f;
	}

	if (m_aegisInvincibleTimer > 0.0f) {
		m_aegisInvincibleTimer -= deltaTime;
		if (m_aegisInvincibleTimer < 0.0f) m_aegisInvincibleTimer = 0.0f;

		m_aegisAuraEffectFlow += deltaTime;
		if (m_aegisAuraEffectFlow >= 0.25f && m_aegisInvincibleTimer > 0.0f) {
			m_aegisAuraEffectFlow = 0.0f;
			zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
				SkillSlot::Ultimate, SkillEffectType::Aegis_ShieldAura, worldMat.pos, vec4(0, 1, 0, 0),
				2.2f, 1.0f, 0.55f);
		}
	}

	if (m_aegisChargeTimer > 0.0f) {
		m_aegisChargeTimer -= deltaTime;
		if (m_aegisChargeTimer <= 0.0f) {
			m_aegisChargeTimer = 0.0f;
			LVelocity.x = 0.0f;
			LVelocity.z = 0.0f;
		}
	}

	if (m_dualDashTimer > 0.0f) {
		m_dualDashTimer -= deltaTime;
		if (m_dualDashTimer <= 0.0f) {
			m_dualDashTimer = 0.0f;
			LVelocity.x = 0.0f;
			LVelocity.z = 0.0f;
		}
	}

	if ((PlayerJob)m_currentJob == PlayerJob::Aegis) {
		if (m_aegisShieldCooldownTimer > 0.0f) {
			m_aegisShieldCooldownTimer -= deltaTime;
			if (m_aegisShieldCooldownTimer < 0.0f) m_aegisShieldCooldownTimer = 0.0f;
		}

		bool shieldInput = InputBuffer[InputID::MouseRbutton] == true;
		bool shieldPressed = shieldInput && m_aegisShieldPrevInput == false;
		m_aegisShieldPrevInput = shieldInput;

		bool canActivateShield = m_aegisShieldCooldownTimer <= 0.0f && ShieldDurability > 0.0f;
		if (shieldPressed) {
			m_aegisShieldActive = m_aegisShieldActive ? false : canActivateShield;
		}
		if (canActivateShield == false) {
			m_aegisShieldActive = false;
		}

		if (m_aegisShieldActive) {
			m_aegisShieldInactiveTimer = 0.0f;
			m_aegisShieldEffectFlow += deltaTime;
			if (m_aegisShieldEffectFlow >= 0.24f) {
				m_aegisShieldEffectFlow = 0.0f;
				XMVECTOR quaternion = XMQuaternionRotationRollPitchYaw(0.0f, m_yaw, 0.0f);
				vec4 shieldDirection = XMVector3Rotate(vec4(0, 0, 1, 0), quaternion);
				vec4 shieldRight = XMVector3Rotate(vec4(1, 0, 0, 0), quaternion);
				vec4 shieldPosition = worldMat.pos + shieldDirection * 0.95f - shieldRight * 0.82f + vec4(0.0f, 1.02f, 0.0f, 0.0f);
				zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
					SkillSlot::Skill2, SkillEffectType::Aegis_Barrier, shieldPosition, shieldDirection,
					1.15f, ShieldDurability, 0.30f);
			}
		}
		else {
			m_aegisShieldEffectFlow = 0.24f;
			m_aegisShieldInactiveTimer += deltaTime;
			if (m_aegisShieldInactiveTimer >= 3.0f && ShieldDurability < MaxShieldDurability && m_aegisShieldCooldownTimer <= 0.0f) {
				ShieldDurability += 10.0f * deltaTime;
				if (ShieldDurability > MaxShieldDurability) ShieldDurability = MaxShieldDurability;
			}
		}
	}
	else {
		m_aegisShieldActive = false;
		m_aegisShieldPrevInput = false;
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
	if ((PlayerJob)m_currentJob == PlayerJob::Aegis &&
		ShieldDurability <= 0.0f &&
		skill.effectType != SkillEffectType::Aegis_Barrier) {
		return false;
	}
	if (skill.effectType == SkillEffectType::Aegis_ShieldCharge &&
		(m_aegisShieldActive == false || ShieldDurability <= 0.0f || m_aegisShieldCooldownTimer > 0.0f)) {
		return false;
	}

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
	else if (skill.effectType == SkillEffectType::Juggernaut_UltimateFire) {
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
	else if (skill.effectType == SkillEffectType::Aegis_ShieldAura) {
		float radiusSq = skill.radius * skill.radius;
		int casterObjIndex = gameworld.clients[clientIndex].objindex;
		for (int i = 0; i < gameworld.clients.size; ++i) {
			if (gameworld.clients[i].zoneId != gameworld.clients[clientIndex].zoneId) continue;
			Player* ally = gameworld.clients[i].pObjData;
			if (ally == nullptr) continue;

			vec4 toAlly = ally->worldMat.pos - worldMat.pos;
			if (toAlly.fast_square_of_len3 > radiusSq) continue;

			float bonus = skill.power;
			if (bonus > ally->m_tempMaxHpBonus) {
				ally->MaxHP += bonus - ally->m_tempMaxHpBonus;
				ally->HP += bonus - ally->m_tempMaxHpBonus;
				ally->m_tempMaxHpBonus = bonus;
			}
			ally->m_tempMaxHpTimer = skill.duration;
			ally->m_aegisInvincibleTimer = 5.0f;
			ally->m_aegisAuraEffectFlow = 0.25f;

			zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[i].PersonalSDS,
				gameworld.clients[i].objindex, ally, GameObjectType::_Player, &ally->MaxHP);
			zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[i].PersonalSDS,
				gameworld.clients[i].objindex, ally, GameObjectType::_Player, &ally->HP);
			zones->Sending_SkillCast(zones->CommonSDS, casterObjIndex, (PlayerJob)m_currentJob,
				SkillSlot::Ultimate, SkillEffectType::Aegis_ShieldAura, ally->worldMat.pos, vec4(0, 1, 0, 0),
				2.2f, bonus, 0.8f);
		}
	}
	if (applied == false) return false;

	SkillCooldownFlow[slotIndex] = skill.cooldown;
	zones->Sending_ChangeGameObjectMember<decltype(SkillCooldownFlow)>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, SkillCooldownFlow);

	XMVECTOR quaternion = XMQuaternionRotationRollPitchYaw(m_pitch, m_yaw, 0);
	vec4 direction = { 0, 0, 1, 0 };
	direction = XMVector3Rotate(direction, quaternion);
	if (skill.effectType == SkillEffectType::Juggernaut_FireProjectile) {
		const float fireAngles[] = { -30.0f, 0.0f, 30.0f };
		for (float angle : fireAngles) {
			vec4 shotDirection = RotateDirectionYaw(direction, angle);
			zones->ApplySkillDamage(this, skill.effectType, worldMat.pos, shotDirection, skill.range, skill.radius, skill.power);
		}
	}
	else if (skill.effectType == SkillEffectType::Juggernaut_UltimateFire) {
		m_juggernautFlameTimer = skill.duration;
		m_juggernautFlameTickFlow = 0.18f;
		m_juggernautFlameEffectFlow = 0.12f;
		m_juggernautFlameRange = max(skill.range, 50.0f);
		m_juggernautFlameRadius = max(skill.radius, 2.4f);
		m_juggernautFlameDps = max(18.0f, skill.power * 0.35f);
	}
	else if (skill.effectType == SkillEffectType::Aegis_ShieldCharge) {
		LVelocity.x = direction.x * 22.0f;
		LVelocity.z = direction.z * 22.0f;
		m_aegisChargeTimer = max(0.18f, skill.duration);
		zones->ApplySkillDamage(this, skill.effectType, worldMat.pos, direction, skill.range, skill.radius, skill.power);
	}
	else if (skill.effectType == SkillEffectType::Aegis_Barrier) {
		ShieldDurability += 30.0f;
		if (ShieldDurability > MaxShieldDurability) ShieldDurability = MaxShieldDurability;
		HP += 30.0f;
		if (HP > MaxHP) HP = MaxHP;
		m_aegisRepairTimer = max(m_aegisRepairTimer, skill.duration);
		m_aegisShieldCooldownTimer = 0.0f;
		zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS,
			gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &ShieldDurability);
		zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS,
			gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HP);
	}
	else if (skill.effectType == SkillEffectType::Rifle_TacticalGrenade) {
		m_rifleGrenadeTimer = skill.duration;
		m_rifleGrenadeDuration = skill.duration;
		m_rifleGrenadeEffectFlow = 0.08f;
		m_rifleGrenadeRadius = skill.radius;
		m_rifleGrenadeDamage = skill.power;
		m_rifleGrenadeOrigin = worldMat.pos + vec4(0.0f, 1.0f, 0.0f, 0.0f);
		m_rifleGrenadePosition = worldMat.pos + direction * min(skill.range, 14.0f);
	}
	else if (skill.effectType == SkillEffectType::Rifle_StimPack) {
		m_rifleStimTimer = skill.duration;
		m_rifleStimRegenFlow = 0.0f;
		m_rifleStimEffectFlow = 0.20f;
	}
	else if (skill.effectType == SkillEffectType::Rifle_MissileBarrage) {
		m_rifleMissileTimer = skill.duration;
		m_rifleMissileTickFlow = 1.0f;
		m_rifleMissileCount = 0;
		m_rifleMissileOrigin = worldMat.pos;
		m_rifleMissileDirection = direction;
		m_rifleMissileRadius = skill.radius;
		m_rifleMissileDamage = skill.power;
	}
	else if (skill.effectType == SkillEffectType::Sniper_GrappleHook) {
		LVelocity += direction * 22.0f;
		LVelocity.y = max(LVelocity.y, 4.0f);
	}
	else if (skill.effectType == SkillEffectType::Sniper_ModeSwitch) {
		m_sniperDmrMode = !m_sniperDmrMode;
		if ((WeaponType)m_currentWeaponType == WeaponType::Sniper) {
			weapon.m_info.shootDelay = m_sniperDmrMode ? 0.35f : 1.5f;
			weapon.m_info.damage = m_sniperDmrMode ? 35.0f : 100.0f;
			weapon.m_info.maxBullets = m_sniperDmrMode ? 10 : 5;
			weapon.m_info.reloadTime = m_sniperDmrMode ? 1.4f : 2.0f;
			if (bullets > weapon.m_info.maxBullets) bullets = weapon.m_info.maxBullets;
			zones->Sending_ChangeGameObjectMember<Weapon>(gameworld.clients[clientIndex].PersonalSDS,
				gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &weapon);
			zones->Sending_ChangeGameObjectMember<int>(gameworld.clients[clientIndex].PersonalSDS,
				gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &bullets);
		}
	}
	else if (skill.effectType == SkillEffectType::Sniper_Railgun) {
		m_railgunTimer = skill.duration;
		m_railgunShots = 10;
	}
	else if (skill.effectType == SkillEffectType::DualPistol_DeathDash) {
		LVelocity.x = direction.x * 20.0f;
		LVelocity.z = direction.z * 20.0f;
		m_dualDashTimer = max(0.18f, skill.duration);
		bullets = weapon.m_info.maxBullets;
		zones->ApplySkillDamage(this, skill.effectType, worldMat.pos, direction, skill.range, skill.radius, skill.power);
		zones->Sending_ChangeGameObjectMember<int>(gameworld.clients[clientIndex].PersonalSDS,
			gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &bullets);
	}
	else if (skill.effectType == SkillEffectType::DualPistol_BladeMode) {
		m_dualBladeTimer = skill.duration;
	}
	else if (skill.effectType == SkillEffectType::DualPistol_Awaken) {
		m_dualAwakenTimer = skill.duration;
	}	else {
		zones->ApplySkillDamage(this, skill.effectType, worldMat.pos, direction, skill.range, skill.radius, skill.power);
	}
	SkillEffectType castEffectType = skill.effectType;
	vec4 castPosition = worldMat.pos;
	vec4 castDirection = direction;
	float castRadius = skill.radius;
	float castDuration = skill.duration;
	if (skill.effectType == SkillEffectType::Rifle_TacticalGrenade) {
		castEffectType = SkillEffectType::Rifle_GrenadeTrail;
		castPosition = m_rifleGrenadeOrigin;
		castRadius = 0.65f;
		castDuration = 0.22f;
	}
	else if (skill.effectType == SkillEffectType::Rifle_StimPack) {
		castEffectType = SkillEffectType::Rifle_StimField;
		castDirection = vec4(0, 1, 0, 0);
		castRadius = 1.45f;
		castDuration = 0.35f;
	}
	else if (skill.effectType == SkillEffectType::Rifle_MissileBarrage) {
		castEffectType = SkillEffectType::Rifle_AirStrikeTrail;
		castPosition = worldMat.pos + direction * 8.0f + vec4(0.0f, 12.0f, 0.0f, 0.0f);
		castDirection = vec4(0, -1, 0, 0);
		castRadius = 1.2f;
		castDuration = 0.65f;
	}
	zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob, slot, castEffectType, castPosition, castDirection, castRadius, skill.power, castDuration);
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
	float speed = 6.0f;
	if (m_rifleStimTimer > 0.0f) speed *= 1.35f;
	if (m_aegisRepairTimer > 0.0f) speed *= 1.30f;
	if (m_dualAwakenTimer > 0.0f) speed *= 1.40f;
	const bool iceBlocked = m_iceBlockTimer > 0.0f;

	if (isGround && iceBlocked == false) {
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
		peye += 1.35f * worldMat.up;
		peye -= 4.0f * clook;
		peye += 1.10f * worldMat.right;
		pat = peye + 10.0f * clook;
	}

	XMVECTOR vUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	if (InputBuffer[InputID::KeyboardQ] == true) {
		TryUseSkill(SkillSlot::Ultimate);
		InputBuffer[InputID::KeyboardQ] = false;
	}

	if (InputBuffer[InputID::MouseLbutton] == true) {
		bool isHeatWeapon = ((PlayerJob)m_currentJob == PlayerJob::Juggernaut && (WeaponType)m_currentWeaponType == WeaponType::MachineGun);
		bool isOverheated = (isHeatWeapon && HeatGauge >= MaxHeatGauge);
		bool isBladeAttack = (m_dualBladeTimer > 0.0f && (PlayerJob)m_currentJob == PlayerJob::Gunner);
		bool isRailgunAttack = (m_railgunTimer > 0.0f && m_railgunShots > 0);
		float effectiveShootDelay = weapon.m_info.shootDelay;
		float effectiveDamage = weapon.m_info.damage;
		float effectiveRayDistance = 50.0f;
		if (m_rifleStimTimer > 0.0f) effectiveShootDelay *= 0.70f;
		if (m_dualAwakenTimer > 0.0f) effectiveShootDelay *= 0.50f;
		if ((WeaponType)m_currentWeaponType == WeaponType::Sniper && m_sniperDmrMode) {
			effectiveShootDelay = min(effectiveShootDelay, 0.35f);
			effectiveDamage = 35.0f;
		}
		if (isRailgunAttack) {
			effectiveShootDelay = 1.0f;
			effectiveDamage = 120.0f;
			effectiveRayDistance = 90.0f;
		}

		if (isBladeAttack && weapon.m_shootFlow >= 0.55f) {
			weapon.OnFire();
			int hitCount = zones->ApplySkillDamage(this, SkillEffectType::DualPistol_BladeMode, worldMat.pos, clook, 2.4f, 1.5f, 35.0f);
			if (hitCount > 0) {
				HP += 35.0f * 0.7f * (float)hitCount;
				if (HP > MaxHP) HP = MaxHP;
				zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS,
					gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HP);
			}
			zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
				SkillSlot::Skill2, SkillEffectType::DualPistol_BladeMode, worldMat.pos, clook, 1.5f, 35.0f, 0.25f);
		}
		else if (weapon.m_shootFlow >= effectiveShootDelay && (bullets > 0 || isRailgunAttack) && isOverheated == false) {

			if (isRailgunAttack) {
				--m_railgunShots;
				if (m_railgunShots <= 0) m_railgunTimer = 0.0f;
			}
			else {
				bullets -= 1;
			}
			if (isHeatWeapon) {
				HeatGauge += 2;
				if (HeatGauge > MaxHeatGauge) HeatGauge = MaxHeatGauge;
			}

			weapon.OnFire();

			vec4 shootOrigin = worldMat.pos + vec4(0, 1.0f, 0, 0);
			vec4 rayStart;
			if (bFirstPersonVision) {
				rayStart = shootOrigin + clook * 1.5f;
			}
			else {
				rayStart = peye;
			}
			vec4 aimDirection = clook;
			if ((WeaponType)m_currentWeaponType == WeaponType::Shotgun) {
				constexpr int shotgunPelletCount = 9;
				constexpr float shotgunRayDistance = 35.0f;
				const float yawOffsets[shotgunPelletCount] = { 0.0f, -3.5f, 3.5f, -7.0f, 7.0f, -2.0f, 2.0f, -5.5f, 5.5f };
				const float pitchOffsets[shotgunPelletCount] = { 0.0f, 2.0f, 2.0f, -1.5f, -1.5f, -4.0f, -4.0f, 4.5f, 4.5f };

				for (int pelletIndex = 0; pelletIndex < shotgunPelletCount; ++pelletIndex) {
					float jitterYaw = ((float)(rand() % 1000) / 1000.0f - 0.5f) * 1.4f;
					float jitterPitch = ((float)(rand() % 1000) / 1000.0f - 0.5f) * 1.0f;
					vec4 pelletDirection = ApplyShotgunSpread(aimDirection, yawOffsets[pelletIndex] + jitterYaw, pitchOffsets[pelletIndex] + jitterPitch);
					zones->FireRaycast((GameObject*)this, rayStart, pelletDirection, shotgunRayDistance, effectiveDamage);
				}
			}
			else {
				zones->FireRaycast((GameObject*)this, rayStart, aimDirection, effectiveRayDistance, effectiveDamage);
				if (isRailgunAttack) {
					zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
						SkillSlot::Ultimate, SkillEffectType::Sniper_Railgun, rayStart, aimDirection, 0.8f, effectiveDamage, 0.25f);
				}
			}

			// fix �̰� �̷��� �ϸ� �ȵɰ� ������? Update �ɶ����� ��Ŷ�� ����. 
			// ��Ŷ�� �������� �������� �δ��̹Ƿ� ���� Ư���ð����� ���ִ°� ����.
			zones->Sending_ChangeGameObjectMember<int>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &bullets);
			zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HeatGauge);
			zones->Sending_PlayerFire(zones->CommonSDS, gameworld.clients[clientIndex].objindex);

			if (isRailgunAttack == false && bullets <= 0) {
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

	//��Ʈ ������ ���� ���� (�߻� ���ҽ�)

	if (HeatGauge > 0.0f) {
		float decayRate = 5.0f; // �ʴ� ���ҷ�
		HeatGauge -= decayRate * deltaTime;
		if (HeatGauge < 0.0f) HeatGauge = 0.0f;
	}

	static float heatSendTimer = 0.0f;
	heatSendTimer += deltaTime;

	if (heatSendTimer > 0.2f) { // 0.2�ʸ��� ���� �� Ŭ�� ����
		heatSendTimer = 0.0f;

		zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS
			, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HeatGauge);
	zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS
			, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &ShieldDurability);
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

	if (m_aegisInvincibleTimer > 0.0f) {
		return;
	}

	float defense = Defense;
	// All Job Reduce Damage With Defense
	damage *= 100.0f / (100.0f + defense);

	if (m_iceBlockTimer > 0.0f) {
		damage *= 0.1f;
	}
	else if (m_dualAwakenTimer > 0.0f) {
		damage *= 0.5f;
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

	if ((PlayerJob)m_currentJob == PlayerJob::Aegis && m_aegisShieldActive && ShieldDurability > 0.0f) {
		float shieldDamage = min(ShieldDurability, damage);
		ShieldDurability -= shieldDamage;
		damage -= shieldDamage;

		if (ShieldDurability <= 0.0f) {
			ShieldDurability = 0.0f;
			m_aegisShieldActive = false;
			m_aegisShieldCooldownTimer = 10.0f;
			m_aegisShieldInactiveTimer = 0.0f;
		}

		zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS,
			gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &ShieldDurability);
		if (damage <= 0.0f) return;
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
	header.zoneId = zoneId;
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
	static_data.ShieldDurability = ShieldDurability;
	static_data.MaxShieldDurability = MaxShieldDurability;
	static_data.m_currentJob = m_currentJob;
	memcpy(static_data.SkillCooldown, SkillCooldown, sizeof(SkillCooldown));
	memcpy(static_data.SkillCooldownFlow, SkillCooldownFlow, sizeof(SkillCooldownFlow));
	static_data.m_currentWeaponType = m_currentWeaponType;
	static_data.m_yaw = m_yaw;
	static_data.m_pitch = m_pitch;
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
	m_juggernautFlameTimer = 0.0f;
	m_juggernautFlameTickFlow = 0.0f;
	m_juggernautFlameEffectFlow = 0.0f;
	m_juggernautFlameRange = 0.0f;
	m_juggernautFlameRadius = 0.0f;
	m_juggernautFlameDps = 0.0f;
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
	for (int i = 0; i < (int)StatusEffectType::Max; ++i) {
		StatusRemain[i] = 0.0f;
		StatusDuration[i] = 0.0f;
		StatusPower[i] = 0.0f;
		StatusTickFlow[i] = 0.0f;
		StatusSource[i] = nullptr;
	}
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
	for (int i = 0; i < (int)StatusEffectType::Max; ++i) {
		StatusRemain[i] = 0.0f;
		StatusDuration[i] = 0.0f;
		StatusPower[i] = 0.0f;
		StatusTickFlow[i] = 0.0f;
		StatusSource[i] = nullptr;
	}

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
		UpdateStatusEffects(deltaTime);
		if (isDead) return;

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
		bool hardControlled = HasStatusEffect(StatusEffectType::Freeze) ||
			HasStatusEffect(StatusEffectType::Stun) ||
			HasStatusEffect(StatusEffectType::Paralyze);
		if (hardControlled) {
			tickLVelocity.x = 0.0f;
			tickLVelocity.z = 0.0f;
			m_isMove = false;
			return;
		}

		vec4 monsterPos = worldMat.pos;
		monsterPos.w = 0;

		if (Target == nullptr || (Target != nullptr && *Target == nullptr)) {
			int limitseek = 4;
			if (gameworld.clients.size == 0) {
				Target = nullptr;
				TryChaseGhost(deltaTime, monsterPos);
				return;
			}

		SEEK_TARGET_LOOP:
			for (int i = targetSeekIndex; i < gameworld.clients.size; ++i) {
				limitseek -= 1;
				if (limitseek == 0) {
					TryChaseGhost(deltaTime, monsterPos);
					return;
				}
				if (gameworld.clients.isnull(i)) continue;
				int targetZoneId = gameworld.clients[i].zoneId;
				if (gameworld.IsAdjacentZone(zoneId, targetZoneId) == false) continue;
				Zone* targetZone = gameworld.GetZone(targetZoneId);
				if (targetZone == nullptr) continue;
				if (gameworld.clients[i].pObjData == nullptr) continue;
				if (gameworld.clients[i].objindex < 0) continue;
				if (gameworld.clients[i].objindex >= targetZone->Dynamic_gameObjects.size) continue;
				if (targetZone->Dynamic_gameObjects.isnull(gameworld.clients[i].objindex)) continue;
				Target = (Player**)&targetZone->Dynamic_gameObjects[gameworld.clients[i].objindex];
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

		// �÷��̾� ����
		if (distanceToPlayer <= m_chaseRange) {
			float effectiveSpeed = m_speed;
			if (HasStatusEffect(StatusEffectType::Slow)) effectiveSpeed *= 0.45f;
			m_targetPos = playerPos;
			m_isMove = effectiveSpeed > 0.0f;

			// A* ��ΰ� ���ų�, �� �Һ������� ���� ���
			if (effectiveSpeed > 0.0f && (path.empty() || currentPathIndex >= path.size())) {
				AstarNode* start = FindClosestNode(monsterPos.x, monsterPos.z, zone->allnodes);
				AstarNode* goal = FindClosestNode(playerPos.x, playerPos.z, zone->allnodes);

				if (start && goal) {
					path = AstarSearch(start, goal, zone->allnodes);
					currentPathIndex = 0;
				}
			}

			// A* ��ΰ� ������ �� ��θ� ���� �̵�
			if (effectiveSpeed > 0.0f && !path.empty() && currentPathIndex < path.size()) {
				float speedScale = (m_speed > 0.0f) ? (effectiveSpeed / m_speed) : 0.0f;
				MoveByAstar(deltaTime * speedScale);
			}
			else if (effectiveSpeed > 0.0f) {
				// ��� ��� �������� ���� ���� ���� �������� fallback
				if (distanceToPlayer > 0.0001f) {
					toPlayer.len3 = 1.0f;
					tickLVelocity += toPlayer * effectiveSpeed * deltaTime;
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

bool Monster::TryChaseGhost(float deltaTime, vec4 monsterPos)
{
	DynamicGameObject* nearest = nullptr;
	float bestDist2 = m_chaseRange * m_chaseRange;
	for (auto& g : gameworld.ghostPlayers) {
		DynamicGameObject* gh = g.second;
		if (gh == nullptr) continue;
		if (gh->tag[GameObjectTag::Tag_Enable] == false) continue;
		if ((short)GameObjectType::VptrToTypeTable[*(void**)gh] != GameObjectType::_Player) continue;
		vec4 d = gh->worldMat.pos - monsterPos; d.y = 0.0f;
		float d2 = d.fast_square_of_len3;
		if (d2 < bestDist2) { bestDist2 = d2; nearest = gh; }
	}
	if (nearest == nullptr) return false;

	vec4 playerPos = nearest->worldMat.pos; playerPos.w = 0;
	vec4 toPlayer = playerPos - monsterPos; toPlayer.y = 0.0f;
	float distanceToPlayer = toPlayer.len3;

	float effectiveSpeed = m_speed;
	if (HasStatusEffect(StatusEffectType::Slow)) effectiveSpeed *= 0.45f;
	m_targetPos = playerPos;
	m_isMove = effectiveSpeed > 0.0f;

	if (effectiveSpeed > 0.0f && (path.empty() || currentPathIndex >= path.size())) {
		AstarNode* start = FindClosestNode(monsterPos.x, monsterPos.z, zone->allnodes);
		AstarNode* goal = FindClosestNode(playerPos.x, playerPos.z, zone->allnodes);
		if (start && goal) { path = AstarSearch(start, goal, zone->allnodes); currentPathIndex = 0; }
	}

	if (effectiveSpeed > 0.0f && !path.empty() && currentPathIndex < path.size()) {
		float speedScale = (m_speed > 0.0f) ? (effectiveSpeed / m_speed) : 0.0f;
		MoveByAstar(deltaTime * speedScale);
	}
	else if (effectiveSpeed > 0.0f) {
		if (distanceToPlayer > 0.0001f) {
			toPlayer.len3 = 1.0f;
			tickLVelocity += toPlayer * effectiveSpeed * deltaTime;
			worldMat.SetLook(toPlayer);
		}
	}

	if (distanceToPlayer < 2.0f) { tickLVelocity.x = 0; tickLVelocity.z = 0; }
	return true;
}

void Monster::OnCollisionRayWithBullet(GameObject* shooter, float damage)
{
	Zone* zone = gameworld.GetZone(zoneId);
	
	// Monster take damage with player's Attack
	void* vptr = *(void**)shooter;
	if (GameObjectType::VptrToTypeTable[vptr] == GameObjectType::_Player) {
		Player* p = (Player*)shooter;
		damage += p->Attack * 0.25f;
		cout << "Player Weapon Damage: " << damage << endl;
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

void Monster::ApplyStatusEffect(GameObject* source, StatusEffectType type, float duration, float power)
{
	int statusIndex = (int)type;
	if (isDead || type == StatusEffectType::None || statusIndex <= 0 || statusIndex >= (int)StatusEffectType::Max) return;
	if (duration <= 0.0f) return;

	Zone* zone = gameworld.GetZone(zoneId);
	if (zone == nullptr) return;

	StatusRemain[statusIndex] = max(StatusRemain[statusIndex], duration);
	StatusDuration[statusIndex] = max(StatusDuration[statusIndex], duration);
	StatusPower[statusIndex] = max(StatusPower[statusIndex], power);
	StatusSource[statusIndex] = source;
	if (type == StatusEffectType::Burn) StatusTickFlow[statusIndex] = 0.0f;
	if (type == StatusEffectType::Taunt && source != nullptr) {
		void* vptr = *(void**)source;
		if (GameObjectType::VptrToTypeTable[vptr] == GameObjectType::_Player) {
			Player* player = (Player*)source;
			int clientIndex = player->clientIndex;
			if (clientIndex >= 0 && clientIndex < gameworld.clients.size && gameworld.clients.isnull(clientIndex) == false) {
				int targetZoneId = gameworld.clients[clientIndex].zoneId;
				Zone* targetZone = gameworld.GetZone(targetZoneId);
				int objIndex = gameworld.clients[clientIndex].objindex;
				if (targetZone != nullptr && gameworld.IsAdjacentZone(zoneId, targetZoneId) && objIndex >= 0 && objIndex < targetZone->Dynamic_gameObjects.size && targetZone->Dynamic_gameObjects.isnull(objIndex) == false) {
					Target = (Player**)&targetZone->Dynamic_gameObjects[objIndex];
					m_fireTimer = m_fireDelay;
					path.clear();
					currentPathIndex = 0;
				}
			}
		}
	}

	BoundingOrientedBox obb = GetOBB();
	vec4 extents = vec4(obb.Extents.x, obb.Extents.y, obb.Extents.z, 0.0f);
	zone->Sending_StatusEffect(zone->CommonSDS, zone->currentIndex, -1, type, true, duration, StatusRemain[statusIndex], StatusPower[statusIndex], worldMat.pos, extents);
}

void Monster::UpdateStatusEffects(float deltaTime)
{
	Zone* zone = gameworld.GetZone(zoneId);
	if (zone == nullptr) return;

	for (int i = 1; i < (int)StatusEffectType::Max; ++i) {
		if (StatusRemain[i] <= 0.0f) continue;

		StatusRemain[i] -= deltaTime;
		StatusEffectType type = (StatusEffectType)i;

		if (type == StatusEffectType::Burn) {
			StatusTickFlow[i] += deltaTime;
			constexpr float burnTickDelay = 0.5f;
			while (StatusTickFlow[i] >= burnTickDelay && StatusRemain[i] > 0.0f && isDead == false) {
				StatusTickFlow[i] -= burnTickDelay;
				float burnDamage = StatusPower[i] > 0.0f ? StatusPower[i] : 4.0f;
				ApplyDamage(StatusSource[i], burnDamage * burnTickDelay);
			}
		}

		if (StatusRemain[i] <= 0.0f) {
			StatusRemain[i] = 0.0f;
			StatusDuration[i] = 0.0f;
			StatusPower[i] = 0.0f;
			StatusTickFlow[i] = 0.0f;
			StatusSource[i] = nullptr;

			BoundingOrientedBox obb = GetOBB();
			vec4 extents = vec4(obb.Extents.x, obb.Extents.y, obb.Extents.z, 0.0f);
			zone->Sending_StatusEffect(zone->CommonSDS, zone->currentIndex, -1, type, false, 0.0f, 0.0f, 0.0f, worldMat.pos, extents);
		}
	}
}

bool Monster::HasStatusEffect(StatusEffectType type) const
{
	int statusIndex = (int)type;
	if (statusIndex <= 0 || statusIndex >= (int)StatusEffectType::Max) return false;
	return StatusRemain[statusIndex] > 0.0f;
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
	while (gameworld.commonMap.isStaticCollision(GetOBB())) {
		Init(XMMatrixTranslation(rand() % 80 - 40, 10.0f, rand() % 80 - 40));
	}
	m_isMove = false;
	for (int i = 0; i < (int)StatusEffectType::Max; ++i) {
		if (StatusRemain[i] > 0.0f) {
			zone->Sending_StatusEffect(zone->CommonSDS, zone->currentIndex, -1, (StatusEffectType)i, false, 0.0f, 0.0f, 0.0f, worldMat.pos, vec4(0.3f, 1.0f, 0.3f, 0.0f));
		}
		StatusRemain[i] = 0.0f;
		StatusDuration[i] = 0.0f;
		StatusPower[i] = 0.0f;
		StatusTickFlow[i] = 0.0f;
		StatusSource[i] = nullptr;
	}
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
<����>
A*���� ���� ���(current) �������� �� �� �ִ� �̿� ��带 ã�� ��ȯ�Ѵ�.
8����(�����¿� + �밢��)�� ��� �˻��Ѵ�.
�� ������ ��� ���� �����Ѵ�.
�̵� �Ұ�(cango==false) ���� �����Ѵ�.

���� :
current : �̿��� ���Ϸ��� ���� ���.
allNodes : ��ü ���.
gridWidth : �׸��� ���� ũ��.
gridHeight : �׸��� ���� ũ��.

return :
vector<AstarNode*> neighbor (�̵� ������ �̿� ��� ���)
if current�� �����ڸ��� -> ���� �˻�� �̿� ���� ����
if �ֺ� ��尡 cango=false -> ��Ͽ��� ���ܵ�
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

			// ���� ���̸� ����
			if (nx < 0 || nz < 0 || nx >= gridWidth || nz >= gridHeight)
				continue;

			// �ε����� ��� ����
			AstarNode* neighbor = allNodes[nz * gridWidth + nx];
			if (neighbor->cango)
				neighbors.push_back(neighbor);
		}
	}

	return neighbors;
}
//AI involved Code End : <chatgpt>

/* <����>
- A* �˰��������� start -> destination �ִ� ��θ� ã�� "��� ����Ʈ(path)"�� ��ȯ�Ѵ�.
- ��帶�� g/h/f ���� parent�� �����ϸ�, �������� ã�� �� parent�� ���� �Ųٷ� ��θ� �����Ѵ�.

���� :
<start> : ���� ���(���� ��ġ�� FindClosestNode�� ã�� ���).
<destination> : ������ ���(�÷��̾�/��ǥ ��ǥ�� FindClosestNode�� ã�� ���).
<allNodes> : ��ü ��� ���(�� Ž������ ��� ��� parent �ʱ�ȭ�� ���).

return :
vector<AstarNode*> (start���� destination������ ��� ���)
if start == nullptr or destination == nullptr -> �� �迭 ��ȯ
if openList�� �� destination�� ã�� ���� -> �� �迭 ��ȯ(��� ����)
if destination ���� -> parent�� ���� �Ųٷ� �� �� reverse�Ͽ� ������ ��� ��ȯ

1) ��� ����� gCost�� ���Ѵ��, parent�� nullptr�� �ʱ�ȭ�Ѵ�(���� Ž�� ���� ����).
2) start�� openList�� �ְ� g=0, h=�Ÿ�(����ư), f=g+h�� ����Ѵ�.
3) openList���� f�� ���� ���� ��带 current�� �̴´�.
4) current�� destination�̸�, current���� parent�� ���󰡸� ��θ� ����� ��ȯ�Ѵ�.
5) �ƴϸ� current�� closedList�� �ű��.
6) current�� �̿����� �˻��ϰ�, �� ª�� ���(tentativeG)�� ������ neighbor�� parent/g/h/f�� �����Ѵ�.
7) openList�� �� ������ �ݺ��Ѵ�.
*/

//AI Code Start : <chatgpt>
vector<AstarNode*> Monster::AstarSearch(AstarNode* start, AstarNode* destination, std::vector<AstarNode*>& allNodes)
{

	std::vector<AstarNode*> openList;
	std::vector<AstarNode*> closedList;

	if (start == nullptr || destination == nullptr)
		return {};

	// �ʱ�ȭ
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
		// fCost �ּ� ��� ����
		AstarNode* currentNode = openList[0];
		for (AstarNode* node : openList)
		{
			if (node->fCost < currentNode->fCost)
				currentNode = node;
		}

		// ������ ���� �� ��� ����
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

		// ���� ����Ʈ �� Ŭ����� ����Ʈ
		openList.erase(std::remove(openList.begin(), openList.end(), currentNode), openList.end());
		closedList.push_back(currentNode);

		// �̿� Ž��
		for (AstarNode* neighbor : GetNeighbors(currentNode, allNodes, 80, 80))
		{
			// �̹� �湮�� ���� ��ŵ
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

	return {}; // ��� ����
}
//AI Code End : <chatgpt>

/*<����>
AstarSearch�� ������� path�� ���� �̵�.
path[currentPathIndex] ��� �������� �̵��Ѵ�.
��ǥ ��忡 ��������� currentPathIndex++ �Ͽ� ���� ��带 ��ǥ�� �Ѵ�.

���� :
deltaTime : speed*deltaTime���� �̵����� ����Ѵ�.

1) ���� ��ǥ ��� = path[currentPathIndex]�� ��´�.
2) ���� ��ġ���� ��ǥ ��� ������ǥ(worldx, worldz)������ ����(dir)�� ���Ѵ�.
3) ��ǥ ��ó���� ���� ���� ��η� �Ѿ��.
4) �ƴϸ� dir�� ����ȭ�ϰ�, tickLVelocity = dir * m_speed * deltaTime���� �̹� ������ �̵����� �����.
5) �ٶ󺸴� ����(look)�� dir�� �����.
*/

//AI involved Code Start : <chatgpt>
void Monster::MoveByAstar(float deltaTime)
{
	Zone* zone = gameworld.GetZone(zoneId);
	if (path.empty() || currentPathIndex >= path.size())
		return;

	AstarNode* targetNode = path[currentPathIndex];

	// ���� ���� ��ġ
	vec4 pos = worldMat.pos;
	pos.w = 1.0f;

	// A*���� ������ Ÿ�� ��� ��ġ (y�� ���� ���� ����)
	vec4 target(targetNode->worldx, pos.y, targetNode->worldz, 1.0f);
	if (zone->AstarStartX > target.x) target.x = zone->AstarStartX;
	if (zone->AstarStartZ > target.z) target.z = zone->AstarStartZ;
	if (zone->AstarEndX < target.x) target.x = zone->AstarEndX;
	if (zone->AstarEndZ < target.z) target.z = zone->AstarEndZ;

	// ���� ����
	vec4 dir = target - pos;
	dir.y = 0.0f;
	float len = dir.len3;

	// ���� �����ߴٰ� ���� ���� ���� �Ѿ
	if (len < 0.3f) {
		currentPathIndex++;
		return;
	}

	// ����ȭ
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
	header.zoneId = zoneId;
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

/*<����>
- ���� ��ǥ(wx, wz)�� ���� ����� "�̵� ���� ���(cango==true)"�� ã�� ��ȯ�Ѵ�.
- ���� ��ǥ�� �׸��忡 ������ �ʾƵ� �ǵ���, A*�� start/goal ��带 ������ ���� �Լ�.

���� :
wx : ���� X ��ǥ.
wz : ���� Z ��ǥ.
allNodes : ��ü ��� ���(�̵� ������ ��带 ã�� ���� ��ü ��ȸ).

return :
if �̵� ���� ��尡 �ϳ��� ������ -> nullptr ��ȯ
if �ĺ� ��尡 ������ -> �Ÿ�^2(dx^2+dz^2)�� �ּ��� ��� ��ȯ

1) allNodes�� ���� cango==true�� ��常 ����.
2) (node.worldx - wx)^2 + (node.worldz - wz)^2 �� ���� ���� ��带 �����Ѵ�.
3) ���������� ���� ����� ��带 ��ȯ.
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

	// mesh/model ���� �⺻ �����͸� ����
	int reqsiz = sizeof(STC_SyncGameObject_Header) + sizeof(Portal::STC_SyncObjData);
	sds.postpush_reserve(reqsiz);
	int offset = 0;

	STC_SyncGameObject_Header& header = *(STC_SyncGameObject_Header*)sds.ofbuff;
	header.size = reqsiz;
	header.st = STC_Protocol::SyncGameObject;
	header.objindex = objindex;
	header.type = GameObjectType::_Portal;
	header.zoneId = zoneId;
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

void World::InitCommonMap() {
	Zone* staticZone = GetStaticChunkZone();
	if (staticZone == nullptr) return;

	commonMap.ownerzone = staticZone;
	commonMap.LoadMap(ZoneMapName[0]);
	if (commonMap.MapObjects.empty()) {
		cout << "[World] common map load failed: " << ZoneMapName[0] << endl;
		return;
	}

	commonMap.AABB[0] = INFINITY;
	commonMap.AABB[1] = -INFINITY;
	for (int i = 0; i < commonMap.MapObjects.size(); ++i) {
		StaticGameObject* obj = commonMap.MapObjects[i];
		if (obj == nullptr) continue;
		staticZone->PushGameObject(obj);

		BoundingOrientedBox obb = obj->GetOBB();
		XMFLOAT3 corners[8];
		obb.GetCorners(corners);
		for (int k = 0; k < 8; ++k) {
			vec4 c = corners[k];
			commonMap.AABB[0] = _mm_min_ps(c, commonMap.AABB[0]);
			commonMap.AABB[1] = _mm_max_ps(c, commonMap.AABB[1]);
		}
	}
	commonMap.AABB[0].w = -INFINITY;
	commonMap.AABB[1].w = INFINITY;

	cout << "[World] common map=" << ZoneMapName[0]
		<< " AABB.min=(" << commonMap.AABB[0].f3.x << ", " << commonMap.AABB[0].f3.y << ", " << commonMap.AABB[0].f3.z << ")"
		<< " AABB.max=(" << commonMap.AABB[1].f3.x << ", " << commonMap.AABB[1].f3.y << ", " << commonMap.AABB[1].f3.z << ")"
		<< " MapObjectCount=" << commonMap.MapObjects.size() << endl;
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

	// ���� �� �ε�
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

		Model* DroneModel = new Model();
		DroneModel->LoadModelFile2("Resources/Model/Drone.model");
		int droneMesh_index = Shape::AddModel("MonsterDrone", DroneModel);

		Model* TurretModel = new Model();
		TurretModel->LoadModelFile2("Resources/Model/Exo.model");
		int turretMesh_index = Shape::AddModel("MonsterTurret", TurretModel);

		Mesh* portalMesh = new Mesh();
		portalMesh->CreateWallMesh(2.0f, 3.0f, 0.2f);
		Shape::AddMesh("Portal", portalMesh);

		//Item ���ҽ�
		{
			int globalitem_index = 0;
			Shape BlackShape;
			BlackShape.FlagPtr = 0;
			ItemTable.push_back(Item(globalitem_index, ItemType::_NULL, L"")); // blank space in inventory.
			globalitem_index += 1;

			auto AddItemFunc = [&](int id, ItemType t, const char* ItemName, const char* modelfilepath, const wchar_t* ItemIconPath, const wchar_t* ItemDescription) {
				// [shape-index align] Register a placeholder shape (null model) per item so the client and
				// server GLOBAL shape tables have identical indices. Without this, the client's 7 item shapes
				// (indices 4..10) shift every later index, so a shapeindex sent by the server resolves to the
				// wrong/garbage shape on the client -> null Model -> shadow-render crash.
				// The server never renders or collides item shapes, so it only needs the index slot. It must
				// NOT load the real model/texture: the server has no graphics device, and the model files may
				// be missing from its Resources folder (loading them crashed startup). null model is safe here
				// because Shape::AddModel only stores the pointer; the server never dereferences it.
				Shape::AddModel(ItemName, nullptr);
				ItemTable.push_back(Item(globalitem_index, t, ItemDescription));
				};

			AddItemFunc(globalitem_index, ItemType::_Consumable, "BioFix",
				"Resources/Model/ItemModel/BioFix.model",
				L"Resources/UI/ItemIcons/ItemIcon_BioFix.png",
				L"[���̿��Ƚ�] : ��ü ������ ����ϴ� ������ �Ƿ� �ֻ��! \n HP+20");
			globalitem_index += 1;

			AddItemFunc(globalitem_index, ItemType::_Consumable, "Tier4Gear",
				"Resources/Model/ItemModel/Tier4Gear.model",
				L"Resources/UI/ItemIcons/ItemIcon_Tear4Gear.png",
				L"[Ƽ��4 ��ǰ] : Ƽ�� 4 ���� ���ۿ� ���Ǵ� �ֿ����. ���� ������ ���⳪ ������ �����ϴµ� ������ �ȴ�.");
			globalitem_index += 1;

			AddItemFunc(globalitem_index, ItemType::_Weapon, "Sniper_IronSight",
				"Resources/Model/sniper.model",
				L"Resources/UI/ItemIcons/ItemIcon_IronSight.png",
				L"[�������� - ���̾����Ʈ] : ���� ������� ����� �޸��� ���� ���� ��ź ���ݼ���.");
			globalitem_index += 1;

			// ������ �� �ε�
			AddItemFunc(globalitem_index, ItemType::_Weapon, "Rifle_StreetSweeper",
				"Resources/Model/Rifle.model",
				L"Resources/UI/ItemIcons/ItemIcon_StreetSweeper.png",
				L"[���ݼ��� - ��Ʈ��Ʈ������] : �Ѷ� �Ÿ��� ������ȴ� Ŭ���� ���ݼ���. ������ ���� ���� ���̴�.");
			globalitem_index += 1;

			// ���� �� �ε�
			AddItemFunc(globalitem_index, ItemType::_Weapon, "Pistol_DoubleTroble",
				"Resources/Model/pistol.model",
				L"Resources/UI/ItemIcons/ItemIcon_DoubleTroble.png",
				L"[�ֱ��� - ����Ʈ����] : ������ ������ �� ��� ��ƺ��̴� ������ �ֱ���.");
			globalitem_index += 1;

			// ���� �� �ε�
			AddItemFunc(globalitem_index, ItemType::_Weapon, "ShotGun_SlagShot",
				"Resources/Model/shootgun.model",
				L"Resources/UI/ItemIcons/ItemIcon_SlagShot.png",
				L"[���� - �����׽�] : ���� ��⸦ ��� ���� ��ĥ�� ����� ��. ���� ���̶� ���ɵ� �״��� ���� �ʴ�.");
			globalitem_index += 1;

			// �ӽŰ�(�̴ϰ�) �� �ε�
			AddItemFunc(globalitem_index, ItemType::_Weapon, "MachineGun_Ratler",
				"Resources/Model/minigun.model",
				L"Resources/UI/ItemIcons/ItemIcon_Ratler.png",
				L"[�ӽŰ� - ��Ʋ��] : ���� ��ǰ�� �����Ÿ��� �Ҹ����� ���� �̸�. ���� ����������� �� �� ����.");
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

		// �������� �� �ε�
		Model* SniperModel = nullptr;
		if (kLoadSniperModel) {
			SniperModel = new Model;
			SniperModel->LoadModelFile2("Resources/Model/sniper.model");
		}

		// ������ �� �ε�
		Model* RifleModel = nullptr;
		if (kLoadRifleModel) {
			RifleModel = new Model;
			RifleModel->LoadModelFile2("Resources/Model/Rifle.model");
		}

		// ���� �� �ε�
		Model* PistolModel = nullptr;
		if (kLoadPistolModel) {
			PistolModel = new Model;
			PistolModel->LoadModelFile2("Resources/Model/pistol.model");
		}

		// ���� �� �ε�
		Model* ShotGunModel = nullptr;
		if (kLoadShotGunModel) {
			ShotGunModel = new Model;
			ShotGunModel->LoadModelFile2("Resources/Model/shootgun.model");
		}

		//// �ӽŰ�(�̴ϰ�) �� �ε�
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
	dbgWarn(false /* silenced: benign. server's material count differs from the client's synced delta by design; GlobalMaterialCount is force-synced from the file just below */ && (GlobalMaterialCount != GlobalMaterialSiz),
        cout << "WARN : ������ Ŭ���̾�Ʈ ���� �ε�� ���� �迭�� �׸��� ��ġ���� �ʴ� ���� �ֽ��ϴ�" << endl;);
	dbgWarn(false /* silenced: benign. client registers item models as Shapes, server intentionally does not (no rendering), so the shape counts differ by design */ && (Shape::ShapeTable.size() != GlobalShapeTableSyncSiz),
        cout << "WARN : ������ Ŭ���̾�Ʈ ���� �ε�� Shape ������ ��ġ���� �ʽ��ϴ�." << endl;);
	GlobalMaterialCount = GlobalMaterialSiz;
#endif

	zones.resize(zoneCount);
	// Zone initialization owns dynamic containers only. The map is loaded once by World.
	for (int i = 0; i < zoneCount; ++i) {
		zones[i].world = this;
		zones[i].zoneId = i;
		if (IsZoneOwned(i) == false) continue;
		zones[i].Init();
	}

	InitCommonMap();

	for (int i = 0; i < zoneCount; ++i) {
		if (IsZoneOwned(i) == false) continue;
		zones[i].GridCollisionCheck();
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

//Ŭ���̾�Ʈ �ε����� �ش��ϴ� �� ã��

bool World::SendPlayerTransferToServer(const PlayerTransferData& data) {
	// [지연 개선] 상시 peer 링크가 있으면 일회성 소켓 대신 그걸로 보낸다.
	// 데이터가 클라 재접속보다 먼저 도착 -> AcceptTransferConnect 가 토큰을 바로 찾아 대기 없음.
	for (int i = 0; i < clients.size; ++i) {
		if (clients.isnull(i)) continue;
		if (clients[i].isServerPeer == false) continue;
		if (clients[i].peerServerId != data.dstZoneId) continue;
		CTS_ServerPlayerTransfer_Header header = {};
		header.size = sizeof(CTS_ServerPlayerTransfer_Header);
		header.st = CTS_Protocol::ServerPlayerTransfer;
		header.data = data;
		int sent = send(clients[i].socket, (const char*)&header, sizeof(header), 0);
		return sent == sizeof(header);
	}

	// 폴백: peer 링크가 아직 없으면 기존 일회성 소켓 사용.
	SOCKET sock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, 0);
	if (sock == INVALID_SOCKET) {
		return false;
	}

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

// [4단계-STEP1] 아직 연결 안 된 인접 존 서버에 상시 복제 링크를 건다.
// 규칙: '낮은 zoneId 서버가 높은 쪽으로' 한 번만 connect (쌍당 1개의 TCP 연결, 전이중 사용).
// 연결에 성공하면 그 소켓을 clients[] 에 isServerPeer 슬롯으로 넣어, 기존 poll/Receiving 경로로
// 메시지를 주고받게 한다(별도 폴링 인프라 불필요). 이웃이 아직 안 떠 있으면 조용히 다음 기회에 재시도.
void World::TryConnectPeers() {
	if (singleProcessAllZones) return; // 단일 프로세스는 이웃이 같은 메모리에 있어 링크가 필요 없다.

	for (int zi = 0; zi < zoneCount; ++zi) {
		if (zi == ownedZoneId) continue;
		if (IsAdjacentZone(ownedZoneId, zi) == false) continue;
		if (zi <= ownedZoneId) continue;   // 높은 쪽으로만 connect -> 쌍당 연결 1개
		if (peerLinkUp[zi]) continue;      // 이미 연결됨

		SOCKET sock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, 0);
		if (sock == INVALID_SOCKET) continue;

		sockaddr_in addr = {};
		addr.sin_family = AF_INET;
		inet_pton(AF_INET, GetZoneIP(zi), &addr.sin_addr);
		addr.sin_port = htons(GetZonePort(zi));
		if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
			closesocket(sock); // 이웃 서버가 아직 리슨 안 함 -> 다음 주기에 재시도
			continue;
		}

		// 핸드셰이크: 내가 누구인지(serverId) 먼저 알린다.
		CTS_ServerLink_Header hello;
		hello.fromServerId = serverId;
		send(sock, (const char*)&hello, sizeof(hello), 0);

		// 이 소켓을 clients[] 의 peer 슬롯으로 등록 -> 메인 루프 poll/Receiving 이 그대로 처리.
		int idx = clients.Alloc();
		if (idx < 0) { closesocket(sock); continue; }
		clients[idx].socket = sock;
		clients[idx].isServerPeer = true;
		clients[idx].peerServerId = zi;       // 이웃 zoneId == 이웃 serverId
		clients[idx].pObjData = nullptr;      // peer는 플레이어가 없음
		clients[idx].objindex = -1;
		clients[idx].zoneId = ownedZoneId;
		clients[idx].rbufoffset = 0;
		clients[idx].pendingTransferToken = 0;
		clients[idx].SetNonBlocking();

		peerLinkUp[zi] = true;
		cout << "[Peer] outbound link established to zone " << zi
			<< " (" << GetZoneIP(zi) << ":" << GetZonePort(zi) << ")" << endl;
	}
}

// [4단계-STEP1] 이웃이 connect 해와 보낸 ServerLink 핸드셰이크를 받았을 때 호출.
// 그 clients[] 슬롯을 게임 클라가 아닌 peer 링크로 확정한다.
void World::OnPeerLinkEstablished(int clientIndex, int fromServerId) {
	if (clientIndex < 0 || clientIndex >= clients.size) return;
	if (clients.isnull(clientIndex)) return;

	clients[clientIndex].isServerPeer = true;
	clients[clientIndex].peerServerId = fromServerId;
	clients[clientIndex].pObjData = nullptr;
	clients[clientIndex].objindex = -1;
	if (fromServerId >= 0 && fromServerId < zoneCount) peerLinkUp[fromServerId] = true;

	cout << "[Peer] inbound link established from server " << fromServerId << endl;
}

// [4단계-STEP2] 내 존에 있는 플레이어들의 위치/시선을 peer 링크로 이웃 서버에 매 틱 전송.
void World::SendGhostToPeers() {
	if (singleProcessAllZones) return;

	static char buf[8192];
	CTS_GhostSync_Header* h = (CTS_GhostSync_Header*)buf;
	h->st = CTS_Protocol::GhostSync;
	h->fromZoneId = ownedZoneId;

	GhostPlayerState* arr = (GhostPlayerState*)(buf + sizeof(CTS_GhostSync_Header));
	int count = 0;
	Zone& oz = zones[ownedZoneId];
	for (int i = 0; i < oz.Dynamic_gameObjects.size; ++i) {
		if (oz.Dynamic_gameObjects.isnull(i)) continue;
		DynamicGameObject* o = oz.Dynamic_gameObjects[i];
		if (o == nullptr) continue;
		if (o->tag[GameObjectTag::Tag_Enable] == false) continue;

		short t = (short)GameObjectType::VptrToTypeTable[*(void**)o];
		if (t != GameObjectType::_Player && t != GameObjectType::_Monster) continue;

		if (sizeof(CTS_GhostSync_Header) + (size_t)(count + 1) * sizeof(GhostPlayerState) > sizeof(buf)) break;
		arr[count].objindex = i;
		arr[count].objType = t;
		arr[count].shapeindex = o->shapeindex;
		arr[count].pos = o->worldMat.pos;
		arr[count].yaw = (t == GameObjectType::_Player) ? ((Player*)o)->m_yaw : 0.0f;
		arr[count].pitch = (t == GameObjectType::_Player) ? ((Player*)o)->m_pitch : 0.0f;
		if (t == GameObjectType::_Monster) {
			Monster* mo = (Monster*)o;
			arr[count].HP = mo->HP;
			arr[count].MaxHP = mo->MaxHP;
			arr[count].isDead = mo->isDead ? 1 : 0;
		}
		else {
			arr[count].HP = 0.0f;
			arr[count].MaxHP = 0.0f;
			arr[count].isDead = 0;
		}
		++count;
	}
	h->count = count;
	h->size = (unsigned int)(sizeof(CTS_GhostSync_Header) + (size_t)count * sizeof(GhostPlayerState));

	// 모든 peer 링크로 전송(count==0 이어도 보냄 -> 이웃이 사라진 고스트를 지울 수 있게).
	for (int i = 0; i < clients.size; ++i) {
		if (clients.isnull(i)) continue;
		if (clients[i].isServerPeer == false) continue;
		send(clients[i].socket, buf, (int)h->size, 0);
	}
}

// [4단계-STEP2] 이웃이 보낸 플레이어 목록으로 고스트를 생성/갱신/제거하고,
// 그 변화를 STC 패킷으로 만들어 '내가 소유한 존'의 CommonSDS 에 실어 내 클라들에게 중계한다.
// (내 클라들은 그걸 인접 존 객체로 받아 기존 렌더 경로 그대로 그린다 -> 클라 수정 불필요)
void World::OnGhostSync(char* data) {
	CTS_GhostSync_Header* h = (CTS_GhostSync_Header*)data;
	int fromZone = h->fromZoneId;
	if (fromZone < 0 || fromZone >= zoneCount) return;
	if (IsZoneOwned(ownedZoneId) == false) return;

	GhostPlayerState* arr = (GhostPlayerState*)(data + sizeof(CTS_GhostSync_Header));
	SendDataSaver& outSDS = zones[ownedZoneId].CommonSDS;

	int seen[256];
	int seenCount = 0;

	for (int k = 0; k < h->count; ++k) {
		int idx = arr[k].objindex;
		if (seenCount < 256) seen[seenCount++] = idx;

		short t = (short)arr[k].objType;
		auto it = ghostPlayers.find(idx);
		bool isNew = (it == ghostPlayers.end());
		DynamicGameObject* ghost;
		if (isNew) {
			if (t == GameObjectType::_Monster) ghost = new Monster();
			else ghost = new Player();
			ghost->SetShape(arr[k].shapeindex);
			ghost->zoneId = fromZone;
			ghost->tag[GameObjectTag::Tag_Enable] = true;
			ghostPlayers[idx] = ghost;
		}
		else {
			ghost = it->second;
		}

		ghost->worldMat.Id();
		vec4 gp = arr[k].pos; gp.w = 1.0f;
		ghost->worldMat.pos = gp;

		if (t == GameObjectType::_Monster) {
			Monster* mg = (Monster*)ghost;
			mg->HP = arr[k].HP;
			mg->MaxHP = arr[k].MaxHP;
			mg->isDead = (arr[k].isDead != 0);
		}

		if (isNew) {
			ghost->SendGameObject(idx, outSDS);  // 처음 본 고스트 -> 풀 생성 패킷 1회
		}
		else {
			zones[fromZone].Sending_ChangeGameObjectMember<matrix>(outSDS, idx, ghost, (GameObjectType)t, &ghost->worldMat);
			if (t == GameObjectType::_Monster) {
				Monster* mg = (Monster*)ghost;
				zones[fromZone].Sending_ChangeGameObjectMember<float>(outSDS, idx, mg, (GameObjectType)t, &mg->HP);
				zones[fromZone].Sending_ChangeGameObjectMember<bool>(outSDS, idx, mg, (GameObjectType)t, &mg->isDead);
			}
		}
	}

	// 이번 목록에 없는 고스트 = 이웃 존에서 사라짐 -> 삭제 패킷 보내고 정리.
	for (auto it = ghostPlayers.begin(); it != ghostPlayers.end(); ) {
		bool found = false;
		for (int s = 0; s < seenCount; ++s) { if (seen[s] == it->first) { found = true; break; } }
		if (found == false) {
			zones[fromZone].Sending_DeleteGameObject(outSDS, it->first);
			delete it->second;
			it = ghostPlayers.erase(it);
		}
		else {
			++it;
		}
	}
}

void World::SendGhostDamageToOwner(int targetZoneId, int targetObjIndex, float damage) {
	if (singleProcessAllZones) return;
	CTS_GhostDamage_Header h;
	h.targetZoneId = targetZoneId;
	h.targetObjIndex = targetObjIndex;
	h.damage = damage;
	for (int i = 0; i < clients.size; ++i) {
		if (clients.isnull(i)) continue;
		if (clients[i].isServerPeer == false) continue;
		if (clients[i].peerServerId != targetZoneId) continue;  // 그 존(객체)을 소유한 서버에게만
		send(clients[i].socket, (const char*)&h, sizeof(h), 0);
	}
}

void World::SendMonsterHandoff(int dstZoneId, Monster* m) {
	if (singleProcessAllZones) return;
	if (m == nullptr) return;
	CTS_MonsterHandoff_Header h;
	h.dstZoneId = dstZoneId;
	h.monsterType = (int)m->m_monsterType;
	h.pos = m->worldMat.pos;
	h.HP = m->HP;
	h.MaxHP = m->MaxHP;
	for (int i = 0; i < clients.size; ++i) {
		if (clients.isnull(i)) continue;
		if (clients[i].isServerPeer == false) continue;
		if (clients[i].peerServerId != dstZoneId) continue;  // 넘겨받을 서버에게만
		send(clients[i].socket, (const char*)&h, sizeof(h), 0);
	}
}

void World::SpawnHandoffMonster(int monsterType, vec4 pos, float hp, float maxhp) {
	if (IsZoneOwned(ownedZoneId) == false) return;
	if (monsterType < 0 || monsterType >= (int)MonsterType::Max) return;  // [SAFE] 잘못된 타입이면 무시(크래시 방어)
	Zone& z = zones[ownedZoneId];
	Monster* mon = new Monster();
	mon->zone = &z;
	mon->ApplyMonsterData((MonsterType)monsterType);
	mon->Init(XMMatrixTranslation(pos.f3.x, pos.f3.y, pos.f3.z));
	mon->HP = hp;
	mon->MaxHP = maxhp;
	mon->handoffCooldown = 1.0f;   // 받자마자 다시 넘어가지 않게(thrashing 방지)
	z.NewObject(mon, GameObjectType::_Monster);
	z.PushGameObject(mon);
}

void World::AcceptClientHello(int clientIndex) {
	if (clientIndex < 0 || clientIndex >= clients.size) return;
	if (clients.isnull(clientIndex)) return;
	if (clients[clientIndex].pObjData != nullptr) return;

	int StartzoneId = ownedZoneId;
	clients[clientIndex].PersonalSDS.Init(4096);
	clients[clientIndex].pendingTransferToken = 0;
	Sending_SyncGameState(clients[clientIndex].PersonalSDS);

	Player* p = new Player();
	p->clientIndex = clientIndex;
	p->worldMat.Id();
	p->worldMat.pos.f3.y = commonMap.AABB[1].y - 1;
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
		if (clientIndex >= 0 && clientIndex < clients.size && clients.isnull(clientIndex) == false) {
			clients[clientIndex].pendingTransferToken = transferToken;
		}
		return false;
	}
	PlayerTransferData data = f->second;
	pendingTransfers.erase(f);

	// [SAFE] success-path bounds guard. The token-not-found path validates clientIndex, but this
	// success path indexed clients[] / zones[] directly. If clientIndex or data.dstZoneId is
	// inconsistent (ABI mismatch / corrupted packet), that is an out-of-bounds crash. Drop instead.
	if (clientIndex < 0 || clientIndex >= clients.size || clients.isnull(clientIndex)) return false;
	if (data.dstZoneId < 0 || data.dstZoneId >= zoneCount) return false;

	clients[clientIndex].PersonalSDS.Init(4096);
	clients[clientIndex].pendingTransferToken = 0;
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
	p->ShieldDurability = data.ShieldDurability;
	p->MaxShieldDurability = data.MaxShieldDurability;
	p->zoneMoveCooldownRemain = data.zoneMoveCooldownRemain;
	p->lastBoundaryIndex = data.lastBoundaryIndex;
	p->wasInsideBoundary = data.wasInsideBoundary;
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

	for (int i = 0; i < clients.size; ++i) {
		if (clients.isnull(i)) continue;
		if (clients[i].pendingTransferToken != data.transferToken) continue;
		AcceptTransferConnect(i, data.transferToken);
		break;
	}
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

	// [4단계-STEP1] 이 슬롯이 이웃 서버와의 peer 링크면, 플레이어 정리 로직을 타지 않고
	// 링크 상태만 리셋한 뒤 종료한다. (다음 TryConnectPeers 가 재연결을 시도)
	if (client.isServerPeer) {
		int lostServerId = client.peerServerId;
		if (lostServerId >= 0 && lostServerId < World::zoneCount) gameworld.peerLinkUp[lostServerId] = false;
		if (client.socket != INVALID_SOCKET) {
			closesocket(client.socket);
			client.socket = INVALID_SOCKET;
		}
		client.isServerPeer = false;
		client.peerServerId = -1;
		client.rbufoffset = 0;
		gameworld.clients.Free(index);
		cout << "[Peer] link to server " << lostServerId << " lost.\n";
		return;
	}

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
	client.pendingTransferToken = 0;
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
		data.ShieldDurability = player->ShieldDurability;
		data.MaxShieldDurability = player->MaxShieldDurability;
		data.zoneMoveCooldownRemain = max(player->zoneMoveCooldownRemain, 0.75f);
		data.lastBoundaryIndex = player->lastBoundaryIndex;
		data.wasInsideBoundary = player->wasInsideBoundary;
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
	// [PERF/FIX ②] 심리스 로컬 이동: fullWorldSnapshot=false 로 동적객체 버스트 생략.
	// 클라는 인접 존 객체를 이미 보유 중이며, 가시성 기반 cleanup 이 그걸 유지한다.
	zones[dstZoneId].AddPlayer(clientIndex, player, spawnPos, true, false);
}


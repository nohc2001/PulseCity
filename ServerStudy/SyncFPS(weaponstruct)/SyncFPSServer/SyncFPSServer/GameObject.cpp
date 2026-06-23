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
	Zone* zone = gameworld.ZoneTable[zoneid];
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
	Zone* zone = gameworld.ZoneTable[zoneid];
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
	Zone* zone = gameworld.ZoneTable[zoneId];
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
	Zone* zone = gameworld.ZoneTable[zoneId];
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
	Zone* zone = gameworld.ZoneTable[zoneId];
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
	Zone* zone = gameworld.ZoneTable[zoneId];
	matrix sav = localWorldMat;
	int temp = parent;
	StaticGameObject* obj = nullptr;
	if (temp >= 0) {
		obj = zone->map.MapObjects[temp];
	}

	if (obj != nullptr) {
		sav *= obj->worldMat;
		temp = obj->parent;
		obj = nullptr;
		if (temp >= 0) {
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
	Zone* zone = gameworld.ZoneTable[zoneId];
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
	Zone* zone = gameworld.ZoneTable[zoneId];
	Shape& s = zone->GetShape(shapeindex);
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

		memcpy(sds.ofbuff + offset, material, sizeof(int) * mesh->subMeshNum);
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
		Zone* zone = gameworld.ZoneTable[zoneId];
		BoundingOrientedBox obb = GetOBB();
		vec4 ext = obb.Extents;
		float len = ext.len3 / zone->chunck_divide_Width;
		chunkAllocIndexsCapacity = powf(2 * ceilf(len * 0.5f), 3);
		chunkAllocIndexs = new int[chunkAllocIndexsCapacity];
	}
}

matrix DynamicGameObject::GetWorld() {
	Zone* zone = gameworld.ZoneTable[zoneId];
	matrix sav = worldMat;
	GameObject* obj = nullptr;
	if(parent != -1) obj = zone->Dynamic_gameObjects[parent];
	while (obj != nullptr) {
		sav *= obj->worldMat;
		if (obj->tag[GameObjectTag::Tag_Dynamic] == false) break;
		obj = zone->Dynamic_gameObjects[obj->parent];
	}
	return sav;
}

void DynamicGameObject::SetWorld(matrix local)
{
	Zone* zone = gameworld.ZoneTable[zoneId];

	if (chunkAllocIndexs) {
		ChunkIndex ci = ChunkIndex(IncludeChunks.xmin, IncludeChunks.ymin, IncludeChunks.zmin);
		int len = IncludeChunks.GetChunckSize();
		for (; ci.extra < len; IncludeChunks.Inc(ci)) {
			auto f = zone->chunck.find(ci);
			if (f != zone->chunck.end()) {
				GameChunk* gc = f->second;
				gc->Dynamic_gameobjects.Free(chunkAllocIndexs[ci.extra]);
			}
		}
	}
	worldMat = local;

	if (chunkAllocIndexs) {
		GameObjectIncludeChunks chunkIds = zone->GetChunks_Include_OBB(GetOBB());
		ChunkIndex ci = ChunkIndex(chunkIds.xmin, chunkIds.ymin, chunkIds.zmin);
		ci.extra = 0;
		int chunckCount = chunkIds.GetChunckSize();
		for (; ci.extra < chunckCount; chunkIds.Inc(ci)) {
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
			this->chunkAllocIndexs[ci.extra] = allocN;
		}
		IncludeChunks = chunkIds;
	}
}

void DynamicGameObject::MoveChunck(const vec4& velocity, const vec4& Q, const GameObjectIncludeChunks& beforeChunckInc, const GameObjectIncludeChunks& afterChunkInc)
{
	Zone* zone = gameworld.ZoneTable[zoneId];
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
		if (ci == inter_ci) {
			intersection.Inc(inter_ci);
			temp[inter_up] = chunkAllocIndexs[ci.extra];
			inter_up += 1;
			continue;
		}

		auto f = zone->chunck.find(ci);
		if (f != zone->chunck.end()) {
#ifdef DEVELOPMODE_ChunckDEBUG
			if (f->second->Dynamic_gameobjects.isnull(chunkAllocIndexs[ci.extra])) cout << "[WARN] Dynamic chunk free skipped invalid slot\n";
#endif
			if (f->second->Dynamic_gameobjects.isnull(chunkAllocIndexs[ci.extra]) == false) f->second->Dynamic_gameobjects.Free(chunkAllocIndexs[ci.extra]);
		}
	}

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
		if (ci == inter_ci) {
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
	Zone* zone = gameworld.ZoneTable[zoneId];
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
	Zone* zone = gameworld.ZoneTable[zoneId];
	zone->GetShape(shapeindex).GetRealShape(mesh, model);
	if (mesh != nullptr) obb_local = mesh->GetOBB();
	if (model != nullptr) obb_local = model->GetOBB();

	matrix sav = GetWorld();
	BoundingOrientedBox obb_world;
	obb_local.Transform(obb_world, sav);

	return obb_world;
}

void DynamicGameObject::LookAt(vec4 look, vec4 up)
{
	worldMat.SetLook(look, up);
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
	Zone* zone = gameworld.ZoneTable[zoneId];
	Shape& s = zone->GetShape(shapeindex);
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

		memcpy(sds.ofbuff + offset, material, sizeof(int) * mesh->subMeshNum);
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
	Zone* zone = gameworld.ZoneTable[zoneId];
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
		if (ci == inter_ci && inter_Count > 0) {
			intersection.Inc(inter_ci);
			temp[inter_up] = chunkAllocIndexs[ci.extra];
			inter_up += 1;
			
			continue;
		}

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
	Zone* zone = gameworld.ZoneTable[zoneId];
	Shape& s = zone->GetShape(shapeindex);
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

		memcpy(sds.ofbuff + offset, material, sizeof(int) * mesh->subMeshNum);
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
	Zone* zone = gameworld.ZoneTable[zoneId];

	if (chunkAllocIndexs) {
		ChunkIndex ci = ChunkIndex(IncludeChunks.xmin, IncludeChunks.ymin, IncludeChunks.zmin);
		int len = IncludeChunks.GetChunckSize();
		for (; ci.extra < len; IncludeChunks.Inc(ci)) {
			auto f = zone->chunck.find(ci);
			if (f != zone->chunck.end()) {
				GameChunk* gc = f->second;
				gc->SkinMesh_gameobjects.Free(chunkAllocIndexs[ci.extra]);
			}
		}
	}
	worldMat = local;

	if (chunkAllocIndexs) {
		GameObjectIncludeChunks chunkIds = zone->GetChunks_Include_OBB(GetOBB());
		ChunkIndex ci = ChunkIndex(chunkIds.xmin, chunkIds.ymin, chunkIds.zmin);
		ci.extra = 0;
		int chunckCount = chunkIds.GetChunckSize();
		for (; ci.extra < chunckCount; chunkIds.Inc(ci)) {
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
			this->chunkAllocIndexs[ci.extra] = allocN;
		}
		IncludeChunks = chunkIds;
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

	GameObjectIncludeChunks goic = ownerzone->GetChunks_Include_OBB(obb);

	for (int ix = goic.xmin; ix <= goic.xmin + goic.xlen; ++ix) {
		for (int iy = goic.ymin; iy <= goic.ymin + goic.ylen; ++iy) {
			for (int iz = goic.zmin; iz <= goic.zmin + goic.zlen; ++iz) {
				auto chun = ownerzone->chunck.find(ChunkIndex(ix, iy, iz));
				if (chun == ownerzone->chunck.end()) continue;
				GameChunk* ch = chun->second;
				// fix?
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
	else {
		return;
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
		// .map
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
		//// .map
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
		// .map
		filename += modelName;
		filename += ".model";

		Model* pModel = new Model();
		pModel->LoadModelFile2(filename, ownerzone);
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

		// 가장 위에 Map 오브젝트일 경우, 해당 Zone 을 알맞은 위치로 이동시켜야 한다.
		if (i == 0) {
			world.pos.x = 0.5f * (ownerzone->BasicAABB_onlyXZ.x + ownerzone->BasicAABB_onlyXZ.z);
			world.pos.z = 0.5f * (ownerzone->BasicAABB_onlyXZ.y + ownerzone->BasicAABB_onlyXZ.w);
		}

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
				go->obbArr[k] = obb;
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

			Zone* zone = gameworld.ZoneTable[ownerzone->zoneId];
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

		if (Mod == 'n') {
			for (int k = 0; k < go->obbArr.size(); ++k) {
				BoundingOrientedBox obb_world;
				go->obbArr[k].Transform(obb_world, go->worldMat);
				go->obbArr[k] = obb_world;
			}
		}

		if (Mod == 'm') {
			go->obbArr.clear();
			Model* model = ownerzone->GetShape(go->shapeindex).GetModel();
			model->RootNode->PushOBBs(model, go->worldMat, &go->obbArr, go);
			//for (int k = 0;k < model->nodeCount;++k) {
			//	ModelNode* node = &model->Nodes[k];
			//	// fix.
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
	tag[GameObjectTag::Tag_Player] = true;
	worldMat.Id();
	shapeindex = -1;

	ApplyJob(PlayerJob::Healer);
	// Keep the extra loadout slots valid without treating the Max sentinel as a weapon.
	weapon[1] = weapon[0];
	weapon[2] = weapon[0];

	HP = 100;
	MaxHP = 100;
	bullets = weapon[0].m_info.maxBullets;
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
	m_weaponHolstered = false;
	m_weaponTogglePrevInput = false;
	bFirstPersonVision = true;
	m_yaw = 0;
	m_pitch = 0;
}

void Player::ApplyJob(PlayerJob job)
{
	const JobData& jobData = GetJobData(job);
	m_currentJob = (int)jobData.job;
	SelectedWeapon = 0;
	weapon[SelectedWeapon] = Weapon(jobData.defaultWeapon);
	m_currentWeaponType = (int)jobData.defaultWeapon;
	bullets = weapon[SelectedWeapon].m_info.maxBullets;
	MaxHP -= m_tempMaxHpBonus;
	if (MaxHP <= 0.0f) MaxHP = 100.0f;
	if (HP > MaxHP) HP = MaxHP;

	MaxHP = jobData.MaxHP;
	Attack = jobData.Attack;
	Defense = jobData.Defense;

	m_tempMaxHpBonus = 0.0f;
	m_tempMaxHpTimer = 0.0f;
	m_iceBlockTimer = 0.0f;
	m_iceBlockEffectFlow = 0.0f;
	m_frostBlizzardTimer = 0.0f;
	m_frostBlizzardEffectFlow = 0.0f;
	m_frostBlizzardRadius = 0.0f;
	m_frostBlizzardPower = 0.0f;
	m_juggernautFlameTimer = 0.0f;
	m_juggernautFlameTickFlow = 0.0f;
	m_juggernautFlameEffectFlow = 0.0f;
	m_juggernautFlameRange = 0.0f;
	m_juggernautFlameRadius = 0.0f;
	m_juggernautFlameDps = 0.0f;
	m_juggernautRegenTimer = 0.0f;
	m_juggernautRegenFlow = 0.0f;
	m_rifleGrenadeTimer = 0.0f;
	m_rifleGrenadeDuration = 0.0f;
	m_rifleGrenadeEffectFlow = 0.0f;
	m_rifleGrenadeRadius = 0.0f;
	m_rifleGrenadeDamage = 0.0f;
	m_rifleGrenadeOrigin = vec4(0, 0, 0, 1);
	m_rifleGrenadePosition = vec4(0, 0, 0, 1);
	m_rifleGrenadeVelocity = vec4(0, 0, 0, 0);
	m_rifleGrenadeArmTimer = 0.0f;
	m_rifleStimTimer = 0.0f;
	m_rifleStimRegenFlow = 0.0f;
	m_rifleStimEffectFlow = 0.0f;
	m_rifleMissileTimer = 0.0f;
	m_rifleMissileTickFlow = 0.0f;
	m_rifleMissileCount = 0;
	m_rifleMissileImpacts.clear();
	ReloadRemain = 0.0f;
	m_sniperDmrMode = false;
	m_sniperSrBullets = 5;
	m_sniperDmrBullets = 15;
	m_railgunSavedDmrMode = false;
	m_sniperNextShotDamageMultiplier = 1.0f;
	m_railgunTimer = 0.0f;
	m_railgunEffectFlow = 0.0f;
	m_railgunShots = 0;
	m_dualBladeTimer = 0.0f;
	m_dualBladeEffectFlow = 0.0f;
	m_dualAwakenTimer = 0.0f;
	m_hackerProjectileTimer = 0.0f;
	m_hackerProjectileEffectFlow = 0.0f;
	m_hackerProjectilePosition = vec4(0, 0, 0, 1);
	m_hackerProjectileVelocity = vec4(0, 0, 0, 0);
	m_hackerEmpFieldTimer = 0.0f;
	m_hackerEmpHealFlow = 0.0f;
	m_hackerEmpEffectFlow = 0.0f;
	m_droneAssaultTimer = 0.0f;
	m_droneFlightTimer = 0.0f;
	m_droneFlightHealFlow = 0.0f;
	m_droneFlightEffectFlow = 0.0f;
	m_bomberHealAmmoMode = false;
	m_bomberSpeedBuffTimer = 0.0f;
	m_bomberMeteorTimer = 0.0f;
	m_bomberMeteorEffectFlow = 0.0f;
	m_bomberMeteorPosition = vec4(0, 0, 0, 1);
	m_bomberMeteorTarget = vec4(0, 0, 0, 1);
	m_bomberProjectiles.clear();
	m_aegisShieldActive = false;
	m_aegisShieldPrevInput = false;
	m_aegisShieldCooldownTimer = 0.0f;
	m_aegisShieldInactiveTimer = 0.0f;
	m_aegisShieldEffectFlow = 0.0f;
	m_aegisRepairTimer = 0.0f;
	m_aegisInvincibleTimer = 0.0f;
	m_aegisAuraEffectFlow = 0.0f;
	m_aegisAuraDamageTimer = 0.0f;
	m_aegisAuraDamageFlow = 0.0f;
	m_aegisAuraElectricFlow = 0.0f;
	m_aegisAuraRadius = 0.0f;
	m_aegisChargeTimer = 0.0f;
	m_aegisChargeEffectFlow = 0.0f;
	m_sniperBackdashTimer = 0.0f;
	m_dualDashTimer = 0.0f;
	m_dualDashDamageFlow = 0.0f;
	m_dualDashHitTargets.clear();
	m_frostPassiveUsed = false;
	MaxShieldDurability = ((PlayerJob)m_currentJob == PlayerJob::Aegis) ? 150.0f : 0.0f;
	ShieldDurability = MaxShieldDurability;

	for (int i = 0; i < (int)SkillSlot::Max; ++i) {
		SkillCooldown[i] = jobData.skills[i].cooldown;
		SkillCooldownFlow[i] = 0.0f;
	}

	/*cout << "Applied job: " << endl
	<< "MaxHP: " << jobData.MaxHP << "Attack" << jobData.Attack << "Defense : " << jobData.Defense << endl;*/
}

void Player::SyncJobState(Zone* zones)
{
	if (zones == nullptr) return;

	zones->Sending_ChangeGameObjectMember<int>(zones->CommonSDS,
		gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &m_currentJob);
	zones->Sending_ChangeGameObjectMember<int>(zones->CommonSDS,
		gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &m_currentWeaponType);
	zones->Sending_ChangeGameObjectMember<int>(zones->CommonSDS,
		gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &SelectedWeapon);
	zones->Sending_ChangeGameObjectMember<int>(gameworld.clients[clientIndex].PersonalSDS,
		gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &bullets);
	zones->Sending_ChangeGameObjectMember<Weapon>(zones->CommonSDS,
		gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &weapon);
	zones->Sending_ChangeGameObjectMember<float>(zones->CommonSDS,
		gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &ReloadRemain);
	zones->Sending_ChangeGameObjectMember<bool>(zones->CommonSDS,
		gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &m_sniperDmrMode);
	zones->Sending_ChangeGameObjectMember<bool>(zones->CommonSDS,
		gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &m_bomberHealAmmoMode);
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

static vec4 GetSupportDronePosition(const Player* player, bool leftDrone)
{
	if (player == nullptr) return vec4(0, 0, 0, 1);
	const float side = leftDrone ? -1.0f : 1.0f;
	vec4 position = player->worldMat.pos + player->worldMat.right * (1.35f * side) -
		player->worldMat.look * 0.55f + player->worldMat.up * 1.75f;
	position.w = 1.0f;
	return position;
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
		m_iceBlockEffectFlow += deltaTime;
		if (m_iceBlockEffectFlow >= 0.16f && m_iceBlockTimer > 0.0f) {
			m_iceBlockEffectFlow = 0.0f;
			zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
				SkillSlot::Skill2, SkillEffectType::Frost_IceBlock, worldMat.pos, vec4(0, 1, 0, 0),
				1.35f, 1.0f, 0.35f);
		}
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

	if (m_frostBlizzardTimer > 0.0f) {
		m_frostBlizzardTimer -= deltaTime;
		if (m_frostBlizzardTimer < 0.0f) m_frostBlizzardTimer = 0.0f;

		m_frostBlizzardEffectFlow += deltaTime;
		if (m_frostBlizzardEffectFlow >= 0.34f && m_frostBlizzardTimer > 0.0f) {
			m_frostBlizzardEffectFlow = 0.0f;
			zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
				SkillSlot::Ultimate, SkillEffectType::Frost_Blizzard, worldMat.pos, vec4(0, 1, 0, 0),
				m_frostBlizzardRadius, m_frostBlizzardPower, 0.58f);
		}
	}

	if (m_juggernautRegenTimer > 0.0f) {
		m_juggernautRegenTimer -= deltaTime;
		if (m_juggernautRegenTimer < 0.0f) m_juggernautRegenTimer = 0.0f;

		m_juggernautRegenFlow += deltaTime;
		constexpr float juggernautRegenTickDelay = 0.5f;
		while (m_juggernautRegenFlow >= juggernautRegenTickDelay && m_juggernautRegenTimer > 0.0f) {
			m_juggernautRegenFlow -= juggernautRegenTickDelay;
			HP += 7.0f * juggernautRegenTickDelay;
			if (HP > MaxHP) HP = MaxHP;
			zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS,
				gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HP);
		}
	}
	if (m_rifleGrenadeTimer > 0.0f) {
		m_rifleGrenadeTimer -= deltaTime;
		if (m_rifleGrenadeTimer < 0.0f) m_rifleGrenadeTimer = 0.0f;
		if (m_rifleGrenadeArmTimer > 0.0f) {
			m_rifleGrenadeArmTimer -= deltaTime;
			if (m_rifleGrenadeArmTimer < 0.0f) m_rifleGrenadeArmTimer = 0.0f;
		}

		m_rifleGrenadeVelocity.y -= 12.5f * deltaTime;
		m_rifleGrenadePosition += m_rifleGrenadeVelocity * deltaTime;
		m_rifleGrenadePosition.w = 1.0f;
		const bool discardRifleGrenade =
			!std::isfinite(m_rifleGrenadePosition.x) ||
			!std::isfinite(m_rifleGrenadePosition.y) ||
			!std::isfinite(m_rifleGrenadePosition.z) ||
			!std::isfinite(m_rifleGrenadeVelocity.x) ||
			!std::isfinite(m_rifleGrenadeVelocity.y) ||
			!std::isfinite(m_rifleGrenadeVelocity.z);
		if (discardRifleGrenade) {
			m_rifleGrenadeTimer = 0.0f;
			m_rifleGrenadeEffectFlow = 0.0f;
		}

		m_rifleGrenadeEffectFlow += deltaTime;
		if (!discardRifleGrenade && m_rifleGrenadeEffectFlow >= 0.08f && m_rifleGrenadeTimer > 0.0f) {
			m_rifleGrenadeEffectFlow = 0.0f;
			vec4 trailDirection = m_rifleGrenadeVelocity;
			if (trailDirection.len3 <= 0.0001f) trailDirection = vec4(0, -1, 0, 0);
			trailDirection.len3 = 1.0f;
			zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
				SkillSlot::Skill1, SkillEffectType::Rifle_GrenadeTrail, m_rifleGrenadePosition, trailDirection,
				0.65f, 1.0f, 0.22f);
		}

		bool shouldExplode = !discardRifleGrenade && m_rifleGrenadeTimer <= 0.0f;
		if (!discardRifleGrenade && m_rifleGrenadeArmTimer <= 0.0f) {
			BoundingSphere grenadeSphere;
			grenadeSphere.Center = XMFLOAT3(m_rifleGrenadePosition.x, m_rifleGrenadePosition.y, m_rifleGrenadePosition.z);
			grenadeSphere.Radius = 0.42f;

			for (int zi = 0; zi < (int)gameworld.ZoneTable.size() && shouldExplode == false; ++zi) {
				if (gameworld.IsZoneOwned(zi) == false) continue;
				if (gameworld.IsAdjacentZone(zones->zoneId, zi) == false) continue;
				if (gameworld.ZoneTable[zi] == nullptr) continue;
			Zone& testZone = *gameworld.ZoneTable[zi];
				for (int i = 0; i < testZone.Dynamic_gameObjects.size; ++i) {
					if (testZone.Dynamic_gameObjects.isnull(i)) continue;
					GameObject* object = (GameObject*)testZone.Dynamic_gameObjects[i];
					if (object == this) continue;
					if (GameObjectType::VptrToTypeTable[*(void**)object] != GameObjectType::_Monster) continue;
					Monster* monster = (Monster*)object;
					if (monster->isDead) continue;
					if (grenadeSphere.Intersects(monster->GetOBB())) {
						shouldExplode = true;
						break;
					}
				}
			}

			BoundingOrientedBox grenadeOBB;
			grenadeOBB.Center = XMFLOAT3(m_rifleGrenadePosition.x, m_rifleGrenadePosition.y, m_rifleGrenadePosition.z);
			grenadeOBB.Extents = XMFLOAT3(0.32f, 0.32f, 0.32f);
			grenadeOBB.Orientation = XMFLOAT4(0, 0, 0, 1);
			if (m_rifleGrenadePosition.y <= 0.25f ||
				(zones != nullptr && zones->map.isStaticCollision(grenadeOBB))) {
				shouldExplode = true;
			}
		}

		if (shouldExplode) {
			m_rifleGrenadeTimer = 0.0f;
			zones->ApplySkillDamage(this, SkillEffectType::Rifle_TacticalGrenade, m_rifleGrenadePosition, vec4(0, 1, 0, 0),
				0.0f, m_rifleGrenadeRadius, m_rifleGrenadeDamage);
			zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
				SkillSlot::Skill1, SkillEffectType::Explosion_Blast, m_rifleGrenadePosition, vec4(0, 1, 0, 0),
				m_rifleGrenadeRadius, m_rifleGrenadeDamage, 1.15f);
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

	for (int i = 0; i < (int)m_rifleMissileImpacts.size();) {
		RifleMissileImpact& impact = m_rifleMissileImpacts[i];
		impact.Timer -= deltaTime;
		if (impact.Timer > 0.0f) {
			++i;
			continue;
		}

		zones->ApplySkillDamage(this, SkillEffectType::Rifle_MissileBarrage, impact.Position, vec4(0, 1, 0, 0),
			0.0f, impact.Radius, impact.Damage);
		zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
			SkillSlot::Ultimate, SkillEffectType::Explosion_Blast, impact.Position, vec4(0, 1, 0, 0),
			impact.Radius, impact.Damage, 0.9f);

		m_rifleMissileImpacts.erase(m_rifleMissileImpacts.begin() + i);
	}

	if (m_rifleMissileTimer > 0.0f) {
		m_rifleMissileTimer -= deltaTime;
		if (m_rifleMissileTimer < 0.0f) m_rifleMissileTimer = 0.0f;

		m_rifleMissileTickFlow += deltaTime;
		constexpr float missileTickDelay = 0.33f;
		while (m_rifleMissileTickFlow >= missileTickDelay && m_rifleMissileCount < 9) {
			m_rifleMissileTickFlow -= missileTickDelay;
			float randomA = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
			float randomB = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
			float angle = randomA * XM_2PI;
			float scatter = sqrtf(randomB) * 7.5f;
			vec4 right = m_rifleMissileDirection.cross(vec4(0, 1, 0, 0));
			if (right.len3 <= 0.0001f) right = vec4(1, 0, 0, 0);
			right.len3 = 1.0f;
			vec4 forward = m_rifleMissileDirection;
			forward.y = 0.0f;
			if (forward.len3 <= 0.0001f) forward = vec4(0, 0, 1, 0);
			forward.len3 = 1.0f;
			vec4 dropPosition = m_rifleMissileOrigin + right * (cosf(angle) * scatter) + forward * (sinf(angle) * scatter);
			dropPosition.y = worldMat.pos.y;
			vec4 skyPosition = dropPosition + vec4(0.0f, 13.5f, 0.0f, 0.0f);
			zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
				SkillSlot::Ultimate, SkillEffectType::Rifle_AirStrikeTrail, skyPosition, vec4(0, -1, 0, 0),
				1.25f, m_rifleMissileDamage, 0.82f);
			RifleMissileImpact impact;
			impact.Position = dropPosition;
			impact.Position.w = 1.0f;
			impact.Timer = 0.82f;
			impact.Radius = m_rifleMissileRadius;
			impact.Damage = m_rifleMissileDamage;
			m_rifleMissileImpacts.push_back(impact);
			++m_rifleMissileCount;
		}

		if (m_rifleMissileCount >= 9) m_rifleMissileTimer = 0.0f;
	}

	if (m_railgunTimer > 0.0f) {
		m_railgunTimer -= deltaTime;
		if (m_railgunTimer <= 0.0f) {
			m_railgunTimer = 0.0f;
			m_railgunShots = 0;
			m_sniperDmrMode = m_railgunSavedDmrMode;
			zones->Sending_ChangeGameObjectMember<bool>(zones->CommonSDS,
				gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &m_sniperDmrMode);
		}
		else {
			m_railgunEffectFlow += deltaTime;
			if (m_railgunEffectFlow >= 0.45f) {
				m_railgunEffectFlow = 0.0f;
				zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
					SkillSlot::Ultimate, SkillEffectType::Electric_Arc, worldMat.pos + vec4(0.0f, 0.55f, 0.0f, 0.0f), vec4(0, 1, 0, 0),
					0.55f, 1.0f, 0.16f);
			}
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

	if (m_hackerProjectileTimer > 0.0f) {
		m_hackerProjectileTimer -= deltaTime;
		m_hackerProjectilePosition += m_hackerProjectileVelocity * deltaTime;
		m_hackerProjectilePosition.w = 1.0f;
		m_hackerProjectileEffectFlow += deltaTime;
		if (m_hackerProjectileEffectFlow >= 0.06f) {
			m_hackerProjectileEffectFlow = 0.0f;
			zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
				SkillSlot::Skill1, SkillEffectType::Hacker_Hack, m_hackerProjectilePosition, m_hackerProjectileVelocity,
				0.75f, 12.0f, 0.20f);
		}
		if (zones->ApplySkillDamage(this, SkillEffectType::Hacker_Hack, m_hackerProjectilePosition,
			vec4(0, 1, 0, 0), 0.0f, 1.05f, 0.0f) > 0) m_hackerProjectileTimer = 0.0f;
		if (m_hackerProjectileTimer < 0.0f) m_hackerProjectileTimer = 0.0f;
	}

	if (m_hackerEmpFieldTimer > 0.0f) {
		m_hackerEmpFieldTimer -= deltaTime;
		if (m_hackerEmpFieldTimer < 0.0f) m_hackerEmpFieldTimer = 0.0f;
		m_hackerEmpEffectFlow += deltaTime;
		if (m_hackerEmpEffectFlow >= 0.36f && m_hackerEmpFieldTimer > 0.0f) {
			m_hackerEmpEffectFlow = 0.0f;
			zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
				SkillSlot::Skill2, SkillEffectType::Hacker_EMPField, worldMat.pos, vec4(0, 1, 0, 0), 12.0f, 20.0f, 0.55f);
		}
		m_hackerEmpHealFlow += deltaTime;
		while (m_hackerEmpHealFlow >= 1.0f && m_hackerEmpFieldTimer > 0.0f) {
			m_hackerEmpHealFlow -= 1.0f;
			for (int i = 0; i < gameworld.clients.size; ++i) {
				if (gameworld.clients.isnull(i) || gameworld.clients[i].zoneId != gameworld.clients[clientIndex].zoneId) continue;
				Player* ally = gameworld.clients[i].pObjData;
				if (ally == nullptr) continue;
				vec4 allyDelta = ally->worldMat.pos - worldMat.pos;
				if (allyDelta.fast_square_of_len3 > 144.0f) continue;
				ally->HP = min(ally->MaxHP, ally->HP + 10.0f);
				zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[i].PersonalSDS,
					gameworld.clients[i].objindex, ally, GameObjectType::_Player, &ally->HP);
			}
		}
	}
	if (m_droneAssaultTimer > 0.0f) {
		m_droneAssaultTimer -= deltaTime;
		if (m_droneAssaultTimer < 0.0f) m_droneAssaultTimer = 0.0f;
	}
	if (m_droneFlightTimer > 0.0f) {
		m_droneFlightTimer -= deltaTime;
		if (m_droneFlightTimer < 0.0f) m_droneFlightTimer = 0.0f;

		m_droneFlightHealFlow += deltaTime;
		while (m_droneFlightHealFlow >= 1.0f && m_droneFlightTimer > 0.0f) {
			m_droneFlightHealFlow -= 1.0f;
			for (int i = 0; i < gameworld.clients.size; ++i) {
				if (gameworld.clients.isnull(i) || gameworld.clients[i].zoneId != gameworld.clients[clientIndex].zoneId) continue;
				Player* ally = gameworld.clients[i].pObjData;
				if (ally == nullptr || ally->HP <= 0.0f) continue;
				vec4 allyDelta = ally->worldMat.pos - worldMat.pos;
				if (allyDelta.fast_square_of_len3 > 900.0f) continue;
				ally->HP = min(ally->MaxHP, ally->HP + 20.0f);
				zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[i].PersonalSDS,
					gameworld.clients[i].objindex, ally, GameObjectType::_Player, &ally->HP);
				vec4 healOrigin = GetSupportDronePosition(this, (i & 1) == 0);
				vec4 healDirection = ally->worldMat.pos + vec4(0, 1.0f, 0, 0) - healOrigin;
				float healDistance = healDirection.len3;
				if (healDistance > 0.001f) {
					healDirection.len3 = 1.0f;
					zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
						SkillSlot::Ultimate, SkillEffectType::Drone_Heal, healOrigin, healDirection,
						healDistance, 20.0f, 0.25f);
				}
			}
		}

		m_droneFlightEffectFlow += deltaTime;
		if (m_droneFlightEffectFlow >= 0.24f && m_droneFlightTimer > 0.0f) {
			m_droneFlightEffectFlow = 0.0f;
			zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
				SkillSlot::Ultimate, SkillEffectType::Drone_Flight, worldMat.pos + vec4(0, 0.9f, 0, 0),
				vec4(0, 1, 0, 0), 2.0f, 20.0f, 0.35f);
		}
	}
	if (m_bomberSpeedBuffTimer > 0.0f) {
		m_bomberSpeedBuffTimer -= deltaTime;
		if (m_bomberSpeedBuffTimer < 0.0f) m_bomberSpeedBuffTimer = 0.0f;
	}

	for (int projectileIndex = 0; projectileIndex < (int)m_bomberProjectiles.size();) {
		BomberProjectile& projectile = m_bomberProjectiles[projectileIndex];
		projectile.Timer -= deltaTime;
		projectile.ArmTimer = max(0.0f, projectile.ArmTimer - deltaTime);
		projectile.Velocity.y -= 11.5f * deltaTime;
		projectile.Position += projectile.Velocity * deltaTime;
		projectile.Position.w = 1.0f;
		const bool projectileInvalid =
			!std::isfinite(projectile.Position.x) ||
			!std::isfinite(projectile.Position.y) ||
			!std::isfinite(projectile.Position.z) ||
			!std::isfinite(projectile.Velocity.x) ||
			!std::isfinite(projectile.Velocity.y) ||
			!std::isfinite(projectile.Velocity.z);
		if (projectileInvalid) {
			m_bomberProjectiles.erase(m_bomberProjectiles.begin() + projectileIndex);
			continue;
		}

		projectile.EffectFlow += deltaTime;
		if (projectile.EffectFlow >= 0.065f) {
			projectile.EffectFlow = 0.0f;
			vec4 trailDirection = projectile.Velocity;
			if (trailDirection.len3 <= 0.0001f) trailDirection = vec4(0, -1, 0, 0);
			trailDirection.len3 = 1.0f;
			zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
				SkillSlot::Skill2, projectile.HealMode ? SkillEffectType::Bomber_HealProjectile : SkillEffectType::Bomber_FireProjectile,
				projectile.Position, trailDirection, 0.70f, 1.0f, 0.20f);
		}

		bool explode = projectile.Timer <= 0.0f;
		if (!explode && projectile.ArmTimer <= 0.0f) {
			BoundingSphere projectileSphere;
			projectileSphere.Center = XMFLOAT3(projectile.Position.x, projectile.Position.y, projectile.Position.z);
			projectileSphere.Radius = 0.96f;
			if (projectile.HealMode) {
				for (int i = 0; i < gameworld.clients.size && !explode; ++i) {
					if (gameworld.clients.isnull(i) || i == clientIndex || gameworld.clients[i].zoneId != gameworld.clients[clientIndex].zoneId) continue;
					Player* ally = gameworld.clients[i].pObjData;
					if (ally != nullptr && ally->HP > 0.0f && projectileSphere.Intersects(ally->GetOBB())) explode = true;
				}
			}
			else {
				for (int zi = 0; zi < (int)gameworld.ZoneTable.size() && !explode; ++zi) {
					if (!gameworld.IsZoneOwned(zi) || !gameworld.IsAdjacentZone(zones->zoneId, zi)) continue;
					if (gameworld.ZoneTable[zi] == nullptr) continue;
			Zone& testZone = *gameworld.ZoneTable[zi];
					for (int i = 0; i < testZone.Dynamic_gameObjects.size; ++i) {
						if (testZone.Dynamic_gameObjects.isnull(i)) continue;
						GameObject* object = (GameObject*)testZone.Dynamic_gameObjects[i];
						if (GameObjectType::VptrToTypeTable[*(void**)object] != GameObjectType::_Monster) continue;
						Monster* monster = (Monster*)object;
						if (!monster->isDead && projectileSphere.Intersects(monster->GetOBB())) { explode = true; break; }
					}
				}
			}

			BoundingOrientedBox projectileOBB;
			projectileOBB.Center = projectileSphere.Center;
			projectileOBB.Extents = XMFLOAT3(0.68f, 0.68f, 0.68f);
			projectileOBB.Orientation = XMFLOAT4(0, 0, 0, 1);
			if (projectile.Position.y <= 0.22f ||
				(zones != nullptr && zones->map.isStaticCollision(projectileOBB))) explode = true;
		}

		if (!explode) { ++projectileIndex; continue; }

		constexpr float bomberSplashRadius = 8.0f;
		constexpr float bomberFireDamage = 45.0f;
		if (projectile.HealMode) {
			for (int i = 0; i < gameworld.clients.size; ++i) {
				if (gameworld.clients.isnull(i) || gameworld.clients[i].zoneId != gameworld.clients[clientIndex].zoneId) continue;
				Player* ally = gameworld.clients[i].pObjData;
				if (ally == nullptr || ally->HP <= 0.0f) continue;
				vec4 delta = ally->worldMat.pos - projectile.Position;
				if (delta.fast_square_of_len3 > bomberSplashRadius * bomberSplashRadius) continue;
				ally->HP = min(ally->MaxHP, ally->HP + 15.0f);
				zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[i].PersonalSDS,
					gameworld.clients[i].objindex, ally, GameObjectType::_Player, &ally->HP);
			}
		}
		else {
			zones->ApplySkillDamage(this, SkillEffectType::Bomber_FireExplosion, projectile.Position,
				vec4(0, 1, 0, 0), 0.0f, bomberSplashRadius, bomberFireDamage);
		}
		zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
			SkillSlot::Skill2, projectile.HealMode ? SkillEffectType::Bomber_HealExplosion : SkillEffectType::Bomber_FireExplosion,
			projectile.Position, vec4(0, 1, 0, 0), bomberSplashRadius, projectile.HealMode ? 15.0f : bomberFireDamage, 0.95f);
		m_bomberProjectiles.erase(m_bomberProjectiles.begin() + projectileIndex);
	}

	if (m_bomberMeteorTimer > 0.0f) {
		m_bomberMeteorTimer -= deltaTime;
		m_bomberMeteorPosition.y -= 16.0f * deltaTime;
		m_bomberMeteorEffectFlow += deltaTime;
		if (m_bomberMeteorEffectFlow >= 0.055f && m_bomberMeteorTimer > 0.0f) {
			m_bomberMeteorEffectFlow = 0.0f;
			zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
				SkillSlot::Ultimate, SkillEffectType::Bomber_MeteorTrail, m_bomberMeteorPosition,
				vec4(0, -1, 0, 0), 2.2f, 35.0f, 0.24f);
		}
		if (m_bomberMeteorTimer <= 0.0f || m_bomberMeteorPosition.y <= m_bomberMeteorTarget.y + 0.2f) {
			m_bomberMeteorTimer = 0.0f;
			zones->ApplySkillDamage(this, SkillEffectType::Bomber_Meteor, m_bomberMeteorTarget,
				vec4(0, 1, 0, 0), 0.0f, 36.0f, 90.0f);
			for (int i = 0; i < gameworld.clients.size; ++i) {
				if (gameworld.clients.isnull(i) || gameworld.clients[i].zoneId != gameworld.clients[clientIndex].zoneId) continue;
				Player* ally = gameworld.clients[i].pObjData;
				if (ally == nullptr || ally->HP <= 0.0f) continue;
				vec4 delta = ally->worldMat.pos - m_bomberMeteorTarget;
				if (delta.fast_square_of_len3 > 1296.0f) continue;
				ally->HP = min(ally->MaxHP, ally->HP + 75.0f);
				zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[i].PersonalSDS,
					gameworld.clients[i].objindex, ally, GameObjectType::_Player, &ally->HP);
			}
			zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
				SkillSlot::Ultimate, SkillEffectType::Bomber_Meteor, m_bomberMeteorTarget,
				vec4(0, 1, 0, 0), 36.0f, 90.0f, 1.35f);
		}
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

	if (m_aegisAuraDamageTimer > 0.0f) {
		m_aegisAuraDamageTimer -= deltaTime;
		if (m_aegisAuraDamageTimer < 0.0f) m_aegisAuraDamageTimer = 0.0f;

		m_aegisAuraElectricFlow += deltaTime;
		if (m_aegisAuraElectricFlow >= 0.24f && m_aegisAuraDamageTimer > 0.0f) {
			m_aegisAuraElectricFlow = 0.0f;
			zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
				SkillSlot::Ultimate, SkillEffectType::Electric_Arc, worldMat.pos, vec4(0, 1, 0, 0),
				max(m_aegisAuraRadius, 6.0f), 1.0f, 0.45f);
		}

		m_aegisAuraDamageFlow += deltaTime;
		constexpr float aegisAuraTickDelay = 1.0f;
		while (m_aegisAuraDamageFlow >= aegisAuraTickDelay && m_aegisAuraDamageTimer > 0.0f) {
			m_aegisAuraDamageFlow -= aegisAuraTickDelay;
			zones->ApplySkillDamage(this, SkillEffectType::Aegis_ShieldEnergy, worldMat.pos, vec4(0, 1, 0, 0),
				0.0f, m_aegisAuraRadius, 20.0f);
		}
	}

	if (m_aegisChargeTimer > 0.0f) {
		m_aegisChargeTimer -= deltaTime;
		if (m_aegisChargeTimer <= 0.0f) {
			m_aegisChargeTimer = 0.0f;
			LVelocity.x = 0.0f;
			LVelocity.z = 0.0f;
		}
		else {
			m_aegisChargeEffectFlow += deltaTime;
			if (m_aegisChargeEffectFlow >= 0.07f) {
				m_aegisChargeEffectFlow = 0.0f;
				vec4 chargeDirection = worldMat.look;
				chargeDirection.y = 0.0f;
				if (chargeDirection.len3 <= 0.0001f) chargeDirection = vec4(0, 0, 1, 0);
				chargeDirection.len3 = 1.0f;
				zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
					SkillSlot::Skill1, SkillEffectType::Aegis_ShieldCharge, worldMat.pos + vec4(0.0f, 0.35f, 0.0f, 0.0f), chargeDirection,
					1.25f, 1.0f, 0.18f);
				zones->ApplySkillDamage(this, SkillEffectType::Aegis_ShieldCharge, worldMat.pos, chargeDirection,
					2.8f, 3.2f, 50.0f);
			}
		}
	}

	if (m_sniperBackdashTimer > 0.0f) {
		m_sniperBackdashTimer -= deltaTime;
		if (m_sniperBackdashTimer <= 0.0f) {
			m_sniperBackdashTimer = 0.0f;
			LVelocity.x = 0.0f;
			LVelocity.z = 0.0f;
		}
	}

	if (m_dualDashTimer > 0.0f) {
		m_dualDashTimer -= deltaTime;
		m_dualDashDamageFlow += deltaTime;
		if (m_dualDashDamageFlow >= 0.05f) {
			m_dualDashDamageFlow = 0.0f;
			zones->ApplySkillDamage(this, SkillEffectType::DualPistol_DeathDash, worldMat.pos, worldMat.look,
				0.0f, 4.5f, 20.0f, &m_dualDashHitTargets);
		}
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
				vec4 shieldPosition = worldMat.pos + vec4(0.0f, 1.05f, 0.0f, 0.0f);
				zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
					SkillSlot::Skill2, SkillEffectType::Aegis_ShieldAura, shieldPosition, shieldDirection,
					1.55f, ShieldDurability, 0.30f);
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
	Weapon& currentWeapon = weapon[SelectedWeapon];
	int slotIndex = (int)slot;
	if (slotIndex < 0 || slotIndex >= (int)SkillSlot::Max) return false;
	if (SkillCooldownFlow[slotIndex] > 0.0f) return false;

	Zone* zones = gameworld.GetClientZone(clientIndex);
	const JobData& jobData = GetJobData((PlayerJob)m_currentJob);
	const SkillData& skill = jobData.skills[slotIndex];
	vec4 droneCastPosition = worldMat.pos;
	vec4 droneCastDirection = vec4(0, 0, 1, 0);
	float droneCastDistance = 1.0f;

	if (skill.effectType == SkillEffectType::Sniper_ModeSwitch && m_railgunTimer > 0.0f) return false;
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
		m_iceBlockEffectFlow = 0.16f;
		zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HP);
	}
	else if (skill.effectType == SkillEffectType::Juggernaut_Taunt || skill.effectType == SkillEffectType::Juggernaut_UltimateFire) {
		float bonus = skill.power;
		if (bonus > m_tempMaxHpBonus) {
			MaxHP += bonus - m_tempMaxHpBonus;
			HP += bonus - m_tempMaxHpBonus;
			m_tempMaxHpBonus = bonus;
		}
		m_tempMaxHpTimer = skill.duration;
		if (skill.effectType == SkillEffectType::Juggernaut_Taunt) {
			m_juggernautRegenTimer = skill.duration;
			m_juggernautRegenFlow = 0.5f;
		}
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
			ally->m_aegisInvincibleTimer = 2.0f;
			ally->m_aegisAuraEffectFlow = 0.25f;

			zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[i].PersonalSDS,
				gameworld.clients[i].objindex, ally, GameObjectType::_Player, &ally->MaxHP);
			zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[i].PersonalSDS,
				gameworld.clients[i].objindex, ally, GameObjectType::_Player, &ally->HP);
			zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[i].objindex, (PlayerJob)m_currentJob,
				SkillSlot::Ultimate, SkillEffectType::Aegis_ShieldAura, ally->worldMat.pos, vec4(0, 1, 0, 0),
				2.2f, bonus, 0.8f);
		}
		m_aegisAuraDamageTimer = skill.duration;
		m_aegisAuraDamageFlow = 1.0f;
		m_aegisAuraElectricFlow = 0.24f;
		m_aegisAuraRadius = skill.radius;
		zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
			SkillSlot::Ultimate, SkillEffectType::Aegis_ShieldEnergy, worldMat.pos, vec4(0, 1, 0, 0),
			skill.radius, skill.power, skill.duration);
	}
	else if (skill.effectType == SkillEffectType::Drone_Heal) {
		XMVECTOR aimQuaternion = XMQuaternionRotationRollPitchYaw(m_pitch, m_yaw, 0);
		vec4 aimDirection = XMVector3Rotate(vec4(0, 0, 1, 0), aimQuaternion);
		if (aimDirection.len3 <= 0.0001f) aimDirection = worldMat.look;
		aimDirection.len3 = 1.0f;
		droneCastPosition = GetSupportDronePosition(this, true);

		Player* healTarget = nullptr;
		float nearestAlongRay = max(skill.range, 35.0f);
		for (int i = 0; i < gameworld.clients.size; ++i) {
			if (gameworld.clients.isnull(i) || i == clientIndex ||
				gameworld.clients[i].zoneId != gameworld.clients[clientIndex].zoneId) continue;
			Player* ally = gameworld.clients[i].pObjData;
			if (ally == nullptr || ally->HP <= 0.0f) continue;
			vec4 targetCenter = ally->worldMat.pos + vec4(0, 1.0f, 0, 0);
			vec4 toTarget = targetCenter - droneCastPosition;
			float alongRay = toTarget.x * aimDirection.x + toTarget.y * aimDirection.y + toTarget.z * aimDirection.z;
			if (alongRay <= 0.0f || alongRay > nearestAlongRay) continue;
			vec4 perpendicular = toTarget - aimDirection * alongRay;
			if (perpendicular.fast_square_of_len3 > 3.24f) continue;
			healTarget = ally;
			nearestAlongRay = alongRay;
		}

		if (healTarget == nullptr) {
			if (HP >= MaxHP) return false;
			healTarget = this;
			nearestAlongRay = 2.0f;
		}

		healTarget->HP = min(healTarget->MaxHP, healTarget->HP + max(skill.power, 15.0f));
		zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[healTarget->clientIndex].PersonalSDS,
			gameworld.clients[healTarget->clientIndex].objindex, healTarget, GameObjectType::_Player, &healTarget->HP);
		vec4 healTargetCenter = healTarget->worldMat.pos + vec4(0, 1.0f, 0, 0);
		droneCastDirection = healTargetCenter - droneCastPosition;
		droneCastDistance = max(droneCastDirection.len3, 1.0f);
		droneCastDirection.len3 = 1.0f;
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
	else if (skill.effectType == SkillEffectType::Juggernaut_Taunt) {
		// Defensive buff is applied above; no enemy taunt effect for the revised skill.
	}
	else if (skill.effectType == SkillEffectType::Juggernaut_UltimateFire) {
		m_juggernautFlameTimer = skill.duration;
		m_juggernautFlameTickFlow = 0.18f;
		m_juggernautFlameEffectFlow = 0.12f;
		m_juggernautFlameRange = max(skill.range, 50.0f);
		m_juggernautFlameRadius = max(skill.radius, 2.4f);
		m_juggernautFlameDps = max(18.0f, skill.power * 0.35f);
	}
	else if (skill.effectType == SkillEffectType::Frost_Blizzard) {
		m_frostBlizzardTimer = skill.duration;
		m_frostBlizzardEffectFlow = 0.34f;
		m_frostBlizzardRadius = skill.radius;
		m_frostBlizzardPower = skill.power;
		zones->ApplySkillDamage(this, skill.effectType, worldMat.pos, direction, skill.range, skill.radius, skill.power);
	}
	else if (skill.effectType == SkillEffectType::Aegis_ShieldCharge) {
		LVelocity.x = direction.x * 34.0f;
		LVelocity.z = direction.z * 34.0f;
		m_aegisChargeTimer = max(0.18f, skill.duration);
		m_aegisChargeEffectFlow = 0.07f;
		zones->ApplySkillDamage(this, skill.effectType, worldMat.pos, direction, skill.range, skill.radius, skill.power);
	}
	else if (skill.effectType == SkillEffectType::Aegis_Barrier) {
		ShieldDurability += skill.power;
		if (ShieldDurability > MaxShieldDurability) ShieldDurability = MaxShieldDurability;
		m_aegisRepairTimer = max(m_aegisRepairTimer, skill.duration);
		m_aegisShieldCooldownTimer = 0.0f;
		zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS,
			gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &ShieldDurability);
		zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
			SkillSlot::Skill2, SkillEffectType::Electric_Burst, worldMat.pos + vec4(0.0f, 0.45f, 0.0f, 0.0f), vec4(0, 1, 0, 0),
			1.25f, 1.0f, 0.75f);
	}
	else if (skill.effectType == SkillEffectType::Rifle_TacticalGrenade) {
		m_rifleGrenadeTimer = skill.duration;
		m_rifleGrenadeDuration = skill.duration;
		m_rifleGrenadeEffectFlow = 0.08f;
		m_rifleGrenadeRadius = skill.radius;
		m_rifleGrenadeDamage = skill.power;
		m_rifleGrenadeArmTimer = 0.18f;
		m_rifleGrenadeOrigin = worldMat.pos + vec4(0.0f, 1.25f, 0.0f, 0.0f) + direction * 0.85f;
		m_rifleGrenadePosition = m_rifleGrenadeOrigin;
		m_rifleGrenadeVelocity = direction * 17.5f + vec4(0.0f, 6.6f, 0.0f, 0.0f);
	}
	else if (skill.effectType == SkillEffectType::Rifle_StimPack) {
		m_rifleStimTimer = skill.duration;
		m_rifleStimRegenFlow = 0.0f;
		m_rifleStimEffectFlow = 0.20f;
	}
	else if (skill.effectType == SkillEffectType::Rifle_MissileBarrage) {
		m_rifleMissileTimer = skill.duration;
		m_rifleMissileTickFlow = 0.33f;
		m_rifleMissileCount = 0;
		m_rifleMissileImpacts.clear();
		m_rifleMissileOrigin = worldMat.pos + direction * 15.0f;
		m_rifleMissileDirection = direction;
		m_rifleMissileRadius = max(skill.radius, 7.5f);
		m_rifleMissileDamage = skill.power;
	}
	else if (skill.effectType == SkillEffectType::Sniper_GrappleHook) {
		LVelocity.x = -direction.x * 23.0f;
		LVelocity.z = -direction.z * 23.0f;
		LVelocity.y = max(LVelocity.y, 7.5f);
		m_sniperBackdashTimer = 0.35f;
		m_sniperNextShotDamageMultiplier = 1.5f;
	}
	else if (skill.effectType == SkillEffectType::Sniper_ModeSwitch) {
		if (m_sniperDmrMode) {
			m_sniperDmrBullets = bullets;
		}
		else {
			m_sniperSrBullets = bullets;
		}
		m_sniperDmrMode = !m_sniperDmrMode;
		zones->Sending_ChangeGameObjectMember<bool>(zones->CommonSDS,
			gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &m_sniperDmrMode);
		if ((WeaponType)m_currentWeaponType == WeaponType::Sniper) {
			currentWeapon.m_info.shootDelay = m_sniperDmrMode ? 0.35f : 1.5f;
			currentWeapon.m_info.damage = m_sniperDmrMode ? 35.0f : 100.0f;
			currentWeapon.m_info.maxBullets = m_sniperDmrMode ? 10 : 5;
			currentWeapon.m_info.reloadTime = m_sniperDmrMode ? 1.4f : 2.0f;
			if (bullets > currentWeapon.m_info.maxBullets) bullets = currentWeapon.m_info.maxBullets;
			zones->Sending_ChangeGameObjectMember<Weapon>(gameworld.clients[clientIndex].PersonalSDS,
				gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &weapon);
			zones->Sending_ChangeGameObjectMember<int>(gameworld.clients[clientIndex].PersonalSDS,
				gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &bullets);
		}
	}
	else if (skill.effectType == SkillEffectType::Sniper_Railgun) {
		m_railgunSavedDmrMode = m_sniperDmrMode;
		m_sniperDmrMode = false;
		m_railgunTimer = skill.duration;
		m_railgunEffectFlow = 0.0f;
		m_railgunShots = 5;
		weapon[SelectedWeapon].m_shootFlow = max(weapon[SelectedWeapon].m_shootFlow, 1.0f);
		zones->Sending_ChangeGameObjectMember<bool>(zones->CommonSDS,
			gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &m_sniperDmrMode);
	}
	else if (skill.effectType == SkillEffectType::DualPistol_DeathDash) {
		LVelocity.x = direction.x * 20.0f;
		LVelocity.z = direction.z * 20.0f;
		m_dualDashTimer = max(0.18f, skill.duration);
		bullets = currentWeapon.m_info.maxBullets;
		zones->ApplySkillDamage(this, skill.effectType, worldMat.pos, direction, skill.range, skill.radius, skill.power);
		zones->Sending_ChangeGameObjectMember<int>(gameworld.clients[clientIndex].PersonalSDS,
			gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &bullets);
	}
	else if (skill.effectType == SkillEffectType::DualPistol_BladeMode) {
		m_dualBladeTimer = skill.duration;
		m_dualBladeEffectFlow = 0.50f;
		// Blade mode replaces the firearm completely. Cancel an in-progress reload
		// and make the first melee input immediately eligible.
		ReloadRemain = 0.0f;
		currentWeapon.m_shootFlow = max(currentWeapon.m_shootFlow, 0.50f);
		zones->Sending_ChangeGameObjectMember<float>(zones->CommonSDS,
			gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &ReloadRemain);
	}
	else if (skill.effectType == SkillEffectType::DualPistol_Awaken) {
		m_dualAwakenTimer = skill.duration;
	}
	else if (skill.effectType == SkillEffectType::Drone_Heal) {
		// Healing and target selection are completed before cooldown consumption.
	}
	else if (skill.effectType == SkillEffectType::Drone_Assault) {
		m_droneAssaultTimer = max(skill.duration, 10.0f);
	}
	else if (skill.effectType == SkillEffectType::Drone_Flight) {
		m_droneFlightTimer = max(skill.duration, 10.0f);
		m_droneFlightHealFlow = 1.0f;
		m_droneFlightEffectFlow = 0.24f;
		LVelocity.y = max(LVelocity.y, 4.5f);
		isGround = false;
	}
	else if (skill.effectType == SkillEffectType::Hacker_Hack) {
		m_hackerProjectileTimer = max(0.75f, skill.range / 30.0f);
		m_hackerProjectileEffectFlow = 0.06f;
		m_hackerProjectilePosition = worldMat.pos + vec4(0.0f, 1.05f, 0.0f, 0.0f) + direction * 0.85f;
		m_hackerProjectilePosition.w = 1.0f;
		m_hackerProjectileVelocity = direction * 30.0f;
	}
	else if (skill.effectType == SkillEffectType::Hacker_EMPField) {
		m_hackerEmpFieldTimer = skill.duration;
		m_hackerEmpHealFlow = 1.0f;
		m_hackerEmpEffectFlow = 0.36f;
	}
	else if (skill.effectType == SkillEffectType::Hacker_EMPBurst) {
		zones->ApplySkillDamage(this, skill.effectType, worldMat.pos, direction, 0.0f, skill.radius, skill.power);
	}
	else if (skill.effectType == SkillEffectType::Bomber_SpeedBurst) {
		for (int i = 0; i < gameworld.clients.size; ++i) {
			if (gameworld.clients.isnull(i) || gameworld.clients[i].zoneId != gameworld.clients[clientIndex].zoneId) continue;
			Player* ally = gameworld.clients[i].pObjData;
			if (ally == nullptr || ally->HP <= 0.0f) continue;
			vec4 delta = ally->worldMat.pos - worldMat.pos;
			if (delta.fast_square_of_len3 > skill.radius * skill.radius) continue;
			ally->m_bomberSpeedBuffTimer = max(ally->m_bomberSpeedBuffTimer, skill.duration);
			zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[i].objindex, (PlayerJob)m_currentJob,
				SkillSlot::Skill1, SkillEffectType::Bomber_SpeedBurst, ally->worldMat.pos,
				vec4(0, 1, 0, 0), 1.8f, 30.0f, 0.85f);
		}
	}
	else if (skill.effectType == SkillEffectType::Bomber_AmmoSwitch) {
		m_bomberHealAmmoMode = !m_bomberHealAmmoMode;
		zones->Sending_ChangeGameObjectMember<bool>(zones->CommonSDS,
			gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &m_bomberHealAmmoMode);
	}
	else if (skill.effectType == SkillEffectType::Bomber_Meteor) {
		m_bomberMeteorTimer = 1.15f;
		m_bomberMeteorEffectFlow = 0.055f;
		m_bomberMeteorTarget = worldMat.pos;
		m_bomberMeteorTarget.w = 1.0f;
		m_bomberMeteorPosition = m_bomberMeteorTarget + vec4(0.0f, 18.0f, 0.0f, 0.0f);
		m_bomberMeteorPosition.w = 1.0f;
	}
	else {
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
	else if (skill.effectType == SkillEffectType::Frost_IceBlock) {
		castRadius = 1.35f;
		castDuration = 0.35f;
	}
	else if (skill.effectType == SkillEffectType::Rifle_StimPack) {
		castEffectType = SkillEffectType::Rifle_StimField;
		castDirection = vec4(0, 1, 0, 0);
		castRadius = 1.45f;
		castDuration = 0.35f;
	}
	else if (skill.effectType == SkillEffectType::Rifle_MissileBarrage) {
		castEffectType = SkillEffectType::Rifle_AirStrikeTrail;
		castPosition = worldMat.pos + direction * 15.0f + vec4(0.0f, 13.5f, 0.0f, 0.0f);
		castDirection = vec4(0, -1, 0, 0);
		castRadius = 1.25f;
		castDuration = 0.82f;
	}
	else if (skill.effectType == SkillEffectType::Aegis_ShieldCharge) {
		castPosition = worldMat.pos + vec4(0.0f, 0.35f, 0.0f, 0.0f);
		castRadius = 1.25f;
		castDuration = 0.18f;
	}
	else if (skill.effectType == SkillEffectType::Drone_Heal) {
		castPosition = droneCastPosition;
		castDirection = droneCastDirection;
		castRadius = droneCastDistance;
		castDuration = 0.25f;
	}
	else if (skill.effectType == SkillEffectType::Bomber_Meteor) {
		castEffectType = SkillEffectType::Bomber_MeteorTrail;
		castPosition = m_bomberMeteorPosition;
		castDirection = vec4(0, -1, 0, 0);
		castRadius = 2.2f;
		castDuration = 1.15f;
	}
	else if (skill.effectType == SkillEffectType::Sniper_Railgun) {
		castEffectType = SkillEffectType::Electric_Arc;
		castPosition = worldMat.pos + vec4(0.0f, 0.55f, 0.0f, 0.0f);
		castDirection = vec4(0, 1, 0, 0);
		castRadius = 0.55f;
		castDuration = 0.18f;
	}
	zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob, slot, castEffectType, castPosition, castDirection, castRadius, skill.power, castDuration);
	return true;
}
void Player::StartReload(Zone* zones)
{
	if (zones == nullptr || m_weaponHolstered || ReloadRemain > 0.0f) return;
	if ((PlayerJob)m_currentJob == PlayerJob::Gunner &&
		(WeaponType)m_currentWeaponType == WeaponType::DualPistol &&
		m_dualBladeTimer > 0.0f) return;
	Weapon& currentWeapon = weapon[SelectedWeapon];
	if (bullets >= currentWeapon.m_info.maxBullets) return;

	currentWeapon.m_shootFlow = -currentWeapon.m_info.reloadTime;
	ReloadRemain = currentWeapon.m_info.reloadTime;
	zones->Sending_ChangeGameObjectMember<float>(zones->CommonSDS,
		gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &ReloadRemain);
}

void Player::Update(float deltaTime)
{
	Zone* zones = gameworld.GetClientZone(clientIndex);
	m_currentWeaponType = (int)weapon[SelectedWeapon].m_info.type;
	const bool wasReloading = ReloadRemain > 0.0f;
	weapon[SelectedWeapon].Update(deltaTime);
	if (wasReloading) {
		Weapon& currentWeapon = weapon[SelectedWeapon];
		ReloadRemain = max(0.0f, -currentWeapon.m_shootFlow);
		if (ReloadRemain <= 0.0f) {
			bullets = currentWeapon.m_info.maxBullets;
			if ((WeaponType)m_currentWeaponType == WeaponType::Sniper) {
				if (m_sniperDmrMode) m_sniperDmrBullets = bullets;
				else m_sniperSrBullets = bullets;
			}
			zones->Sending_ChangeGameObjectMember<int>(gameworld.clients[clientIndex].PersonalSDS,
				gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &bullets);
			zones->Sending_ChangeGameObjectMember<float>(zones->CommonSDS,
				gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &ReloadRemain);
		}
	}

	if (zone->lowHit()) {
		// 퀘스트 진행상황 체크 및 업데이트
		
		// 퀘스트가 추가되면 프로그래스도 추가됨.
		while (QuestPrograss.size() < QuestArr.size()) {
			QuestPrograss.push_back(new Quest());
		}

		// 퀘스트가 삭제되면 프로그래스도 삭제됨.
		while (QuestPrograss.size() > QuestArr.size()) {
			QuestPrograss.pop_back();
		}

		for (int i = 0; i < QuestArr.size(); ++i) {
			int n = QuestArr[i];
			Quest* q = gameworld.QuestTable[n];
			if (q->isSameQuest(QuestPrograss[i]) == false) {
				q->Copy(QuestPrograss[i]);
			}

			Quest* prograss = QuestPrograss[i];
			bool QuestUpdate = false;
			for (int k = 0; k < prograss->requp; ++k) {
				QuestRequirement& req = prograss->ReqArr[k];
				int PresentCnt = 0;
				if (req.type == QuestType::CollectItem) {
					for (int u = 0; u < maxItem; ++u) {
						if (Inventory[u].id == req.ObjID) PresentCnt += Inventory[u].ItemCount;
					}
				}
				else if (req.type == QuestType::KillMonster) {
					PresentCnt = req.PastCnt;
				}
				if (PresentCnt != req.PresentCnt) {
					req.PresentCnt = PresentCnt;
					QuestUpdate = true;
				}
			}

			if (QuestUpdate) {
				zone->Sending_SyncQuestPrograss(gameworld.clients[clientIndex].PersonalSDS, n, prograss);
			}
		}
	}

	// 플레이어 경로 기록.
	int prevTail = tailOffset - 1;
	prevTail &= 31;
	if (vec4(PosTail[prevTail] - worldMat.pos).len3 > 0.3f) {
		PosTail[tailOffset] = worldMat.pos;
		tailOffset += 1;
		tailOffset &= 31;
	}

	vec4 currentPos = worldMat.pos;
	XMMATRIX rotMat = XMMatrixRotationRollPitchYaw(0, m_yaw, 0);
	worldMat.mat = rotMat;
	worldMat.pos = currentPos;

	if (collideCount == 0) isGround = false;
	collideCount = 0;

	const bool droneFlying = (PlayerJob)m_currentJob == PlayerJob::DroneOperator && m_droneFlightTimer > 0.0f;
	if (droneFlying) {
		isGround = false;
		LVelocity.y = InputBuffer[InputID::KeyboardSpace] ? 4.5f : 0.0f;
	}
	else if (isGround == false) {
		LVelocity.y -= 9.8f * deltaTime;
	}

	XMFLOAT3 xmf3Shift = XMFLOAT3(0, 0, 0);
	float speed = 6.0f;
	if (m_rifleStimTimer > 0.0f) speed *= 1.35f;
	if (m_aegisRepairTimer > 0.0f) speed *= 1.30f;
	if (m_dualAwakenTimer > 0.0f) speed *= 1.40f;
	if (m_hackerEmpFieldTimer > 0.0f) speed *= 1.40f;
	if (m_bomberSpeedBuffTimer > 0.0f) speed *= 1.30f;
	const bool iceBlocked = m_iceBlockTimer > 0.0f;

	if (isGround && iceBlocked == false) {
		if (InputBuffer[InputID::KeyboardSpace]) {
			LVelocity.y = JumpVelocity;
			isGround = false;
		}
	}
	tickLVelocity = LVelocity * deltaTime;

	vec4 moveDirection = vec4(0, 0, 0, 0);
	if (InputBuffer[InputID::KeyboardW] == true) {
		moveDirection += worldMat.look;
	}
	if (InputBuffer[InputID::KeyboardS] == true) {
		moveDirection -= worldMat.look;
	}
	if (InputBuffer[InputID::KeyboardA] == true) {
		moveDirection -= worldMat.right;
	}
	if (InputBuffer[InputID::KeyboardD] == true) {
		moveDirection += worldMat.right;
	}
	moveDirection.y = 0.0f;
	moveDirection.w = 0.0f;
	if (moveDirection.fast_square_of_len3 > 0.001f) {
		moveDirection = XMVector3Normalize(moveDirection);
		tickLVelocity += speed * moveDirection * deltaTime;
	}

	const bool reloadInput = InputBuffer['R'] == true;
	if (reloadInput && !m_reloadPrevInput) {
		StartReload(zones);
	}
	m_reloadPrevInput = reloadInput;

	const bool weaponToggleInput = InputBuffer[InputID::KeyboardX] == true;
	if (weaponToggleInput && !m_weaponTogglePrevInput) {
		m_weaponHolstered = !m_weaponHolstered;
		if (m_weaponHolstered) {
			InputBuffer[InputID::MouseLbutton] = false;
		}
		zones->Sending_ChangeGameObjectMember<bool>(zones->CommonSDS,
			gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &m_weaponHolstered);
	}
	m_weaponTogglePrevInput = weaponToggleInput;
	if (m_weaponHolstered) {
		InputBuffer[InputID::MouseLbutton] = false;
	}

	UpdateSkillCooldowns(deltaTime, zones);
	UpdateJobTimers(deltaTime, zones);


	//if (InputBuffer[InputID::KeyboardQ] == true) {
	//	std::cout << "[Player::Update] Q pressed!  HP=" << HP
	//		<< " Heat=" << HeatGauge
	//		<< " CD=" << HealSkillCooldownFlow << std::endl;

	//	if (HealSkillCooldownFlow <= 0.0f && HeatGauge > 0.0f && HP < MaxHP) {

	//		// ȸ���� HeatGauge ���θ� HP�� ��ȯ
	//		float healAmount = HeatGauge;
	//		HP += healAmount;

	//		if (HP > MaxHP) HP = MaxHP;

	//		// ������ �Ҹ�
	//		HeatGauge = 0.0f;

	//		// ��Ÿ�� ����
	//		HealSkillCooldownFlow = HealSkillCooldown;

	//		// HP, HeatGauge ����->Ŭ�� ���� ����ȭ
	//		zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HP);
	//		zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HeatGauge);
	//	}
	//}

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

	Weapon& currentWeapon = weapon[SelectedWeapon];
	if (!m_weaponHolstered && InputBuffer[InputID::MouseLbutton] == true) {
		bool isHeatWeapon = ((PlayerJob)m_currentJob == PlayerJob::Juggernaut && (WeaponType)m_currentWeaponType == WeaponType::MachineGun);
		bool isOverheated = (isHeatWeapon && HeatGauge >= MaxHeatGauge);
		bool isBladeAttack = (m_dualBladeTimer > 0.0f && (PlayerJob)m_currentJob == PlayerJob::Gunner && (WeaponType)m_currentWeaponType == WeaponType::DualPistol);
		bool isRailgunAttack = (m_railgunTimer > 0.0f && m_railgunShots > 0);
		float effectiveShootDelay = currentWeapon.m_info.shootDelay;
		float effectiveDamage = currentWeapon.m_info.damage;
		float effectiveRayDistance = 50.0f;
		if (m_rifleStimTimer > 0.0f) effectiveShootDelay *= 0.70f;
		if (m_dualAwakenTimer > 0.0f) effectiveShootDelay *= 0.50f;
		if ((WeaponType)m_currentWeaponType == WeaponType::Sniper && m_sniperDmrMode) {
			effectiveShootDelay = min(effectiveShootDelay, 0.35f);
			effectiveDamage = 35.0f;
			effectiveRayDistance = 100.0f;
		}
		if (isRailgunAttack) {
			effectiveShootDelay = 1.0f;
			effectiveDamage = 120.0f;
			effectiveRayDistance = 145.0f;
		}
		else if ((WeaponType)m_currentWeaponType == WeaponType::Sniper && m_sniperNextShotDamageMultiplier > 1.0f) {
			effectiveDamage *= m_sniperNextShotDamageMultiplier;
		}

		const float meleeAttackDelay = (m_dualAwakenTimer > 0.0f) ? 0.25f : 0.50f;
		if (isBladeAttack && currentWeapon.m_shootFlow >= meleeAttackDelay) {
			currentWeapon.OnFire();
			int hitCount = zones->ApplySkillDamage(this, SkillEffectType::DualPistol_BladeMode,
				worldMat.pos, clook, 4.0f, 2.6f, 50.0f);
			if (hitCount > 0) {
				HP += 50.0f * 0.7f * (float)hitCount;
				if (HP > MaxHP) HP = MaxHP;
				zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS,
					gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HP);
			}
			zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
				SkillSlot::Skill2, SkillEffectType::DualPistol_BladeMode, worldMat.pos, clook, 1.5f, 50.0f, 0.25f);
			zones->Sending_PlayerFire(zones->CommonSDS, gameworld.clients[clientIndex].objindex, 2);
		}
		else if (!isBladeAttack && currentWeapon.m_shootFlow >= effectiveShootDelay &&
			(bullets > 0 || isRailgunAttack) && isOverheated == false) {
			const bool isDualPistolShot = (WeaponType)m_currentWeaponType == WeaponType::DualPistol && !isRailgunAttack;
			const int shotsToFire = isDualPistolShot ? min(bullets, 2) : 1;

			if (isRailgunAttack) {
				--m_railgunShots;
				if (m_railgunShots <= 0) {
					m_railgunTimer = 0.0f;
					m_sniperDmrMode = m_railgunSavedDmrMode;
					zones->Sending_ChangeGameObjectMember<bool>(zones->CommonSDS,
						gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &m_sniperDmrMode);
				}
			}
			else {
					bullets -= shotsToFire;
				if ((WeaponType)m_currentWeaponType == WeaponType::Sniper) {
					m_sniperNextShotDamageMultiplier = 1.0f;
					if (m_sniperDmrMode) m_sniperDmrBullets = bullets;
					else m_sniperSrBullets = bullets;
				}
			}
			if (isHeatWeapon) {
				HeatGauge += 2;
				if (HeatGauge > MaxHeatGauge) HeatGauge = MaxHeatGauge;
			}

			currentWeapon.OnFire();

			vec4 shootOrigin = worldMat.pos + vec4(0, 1.0f, 0, 0);

			vec4 rayStart;
			if (bFirstPersonVision) {
				rayStart = shootOrigin + clook * 0.2f;
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
				if ((WeaponType)m_currentWeaponType == WeaponType::GrenadeGun && !isRailgunAttack) {
					constexpr float bomberProjectileSpeed = 18.0f * 1.3f;
					constexpr float bomberProjectileLift = 4.2f * 1.3f;
					BomberProjectile projectile;
					projectile.Position = shootOrigin + aimDirection * 1.0f + vec4(0.0f, 0.35f, 0.0f, 0.0f);
					projectile.Position.w = 1.0f;
					projectile.Velocity = aimDirection * bomberProjectileSpeed + vec4(0.0f, bomberProjectileLift, 0.0f, 0.0f);
					projectile.Timer = 3.0f;
					projectile.ArmTimer = 0.14f;
					projectile.EffectFlow = 0.065f;
					projectile.HealMode = m_bomberHealAmmoMode;
					m_bomberProjectiles.push_back(projectile);
				}
				else if (isRailgunAttack) {
					zones->FirePiercingRaycast((GameObject*)this, rayStart, aimDirection, effectiveRayDistance, effectiveDamage);
					vec4 railgunVisualStart = shootOrigin + aimDirection * 1.1f;
					zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
						SkillSlot::Ultimate, SkillEffectType::Sniper_Railgun, railgunVisualStart, aimDirection, 0.8f, effectiveDamage, 0.25f);
				}
				else {
						for (int shotIndex = 0; shotIndex < shotsToFire; ++shotIndex) {
							zones->FireRaycast((GameObject*)this, rayStart, aimDirection, effectiveRayDistance, effectiveDamage);
						}
					if ((PlayerJob)m_currentJob == PlayerJob::DroneOperator && m_droneAssaultTimer > 0.0f) {
						vec4 aimPoint = rayStart + aimDirection * effectiveRayDistance;
						for (int droneIndex = 0; droneIndex < 2; ++droneIndex) {
							vec4 droneOrigin = GetSupportDronePosition(this, droneIndex == 0);
							vec4 droneDirection = aimPoint - droneOrigin;
							float droneRayDistance = droneDirection.len3;
							if (droneRayDistance <= 0.0001f) continue;
							droneDirection.len3 = 1.0f;
							zones->FireRaycast((GameObject*)this, droneOrigin, droneDirection, droneRayDistance, effectiveDamage);
							zones->Sending_SkillCast(zones->CommonSDS, gameworld.clients[clientIndex].objindex, (PlayerJob)m_currentJob,
								SkillSlot::Skill2, SkillEffectType::Drone_Assault, droneOrigin, droneDirection,
								droneRayDistance, effectiveDamage, 0.18f);
						}
					}
				}
			}

			// fix?
			zones->Sending_ChangeGameObjectMember<int>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &bullets);
			zones->Sending_ChangeGameObjectMember<float>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &HeatGauge);
			if (isDualPistolShot) {
				zones->Sending_PlayerFire(zones->CommonSDS, gameworld.clients[clientIndex].objindex, 1);
				if (shotsToFire > 1) {
					zones->Sending_PlayerFire(zones->CommonSDS, gameworld.clients[clientIndex].objindex, 0);
				}
			}
			else {
				zones->Sending_PlayerFire(zones->CommonSDS, gameworld.clients[clientIndex].objindex, 0);
			}
		}

		if (isRailgunAttack == false && bullets <= 0) {
			StartReload(zones);
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
	if (m_iceBlockTimer > 0.0f) {
		return;
	}

	float defense = Defense;
	// All Job Reduce Damage With Defense
	damage *= 100.0f / (100.0f + defense);

	if (m_dualAwakenTimer > 0.0f) {
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

bool Player::LootItem(ItemStack is)
{
	int changeSlotID = -1;
	for (int i = 0; i < maxItem; ++i) {
		if (Inventory[i].id == is.id) {
			Inventory[i].ItemCount += is.ItemCount;
			changeSlotID = i;
			goto UpdateInventorySlotEnd;
		}
	}
	for (int i = 0; i < maxItem; ++i) {
		if (Inventory[i].id == 0 || Inventory[i].ItemCount) {
			Inventory[i].id = is.id;
			Inventory[i].ItemCount = is.ItemCount;
			changeSlotID = i;
			goto UpdateInventorySlotEnd;
		}
	}

UpdateInventorySlotEnd:
	if (changeSlotID >= 0) {
		zone->Sending_InventoryItemSync(gameworld.clients[clientIndex].PersonalSDS, Inventory[changeSlotID], changeSlotID);
		return true;
	}
	else {
		return false;
	}
}

bool Player::AddGold(int delta)
{
	int next_gold = Gold + delta;
	if (next_gold < 0) return false;
	else 
	{
		Gold = next_gold;
		zone->Sending_ChangeGameObjectMember<int>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &Gold);
		return true;
	}
}

void Player::AddExp(int delta)
{
	if (delta < 0) return;
	Exp += delta;
	int IndexLevel = Level;
	if (IndexLevel > 99) IndexLevel = 99;
	if (IndexLevel < 0) IndexLevel = 0;

	bool isLevelChanged = false;
	while (ExpLimit[IndexLevel] < Exp) {
		Exp -= ExpLimit[IndexLevel];
		Level += 1;
		isLevelChanged = true;
	}

	if (isLevelChanged) {
		zone->Sending_ChangeGameObjectMember<int>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &Level);
	}
	zone->Sending_ChangeGameObjectMember<int>(gameworld.clients[clientIndex].PersonalSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &Exp);
}

void Player::RewardQuest(Quest* completeQuest)
{
	if (completeQuest == nullptr) return;
	for (int i = 0; i < completeQuest->rewardUp; ++i) {
		QuestReward& reward = completeQuest->RewardArr[i];
		if (reward.type == QuestRewardType::QRT_Money) {
			this->AddGold(reward.count);
		}
		else if (reward.type == QuestRewardType::QRT_Item) {
			ItemStack is;
			is.id = reward.objid;
			is.ItemCount = reward.count;
			this->LootItem(is);
		}
		else if (reward.type == QuestRewardType::QRT_Exp) {
			// 우리 게임에 경험치가 있었나?
			this->AddGold(reward.count);
		}
	}
}

void Player::SendGameObject(int objindex, SendDataSaver& sds) {
	sds.postpush_start();

	//calculate app packet siz.
	int reqsiz = sizeof(STC_SyncGameObject_Header) + sizeof(STC_SyncObjData);
	Zone* zone = gameworld.ZoneTable[zoneId];
	Shape& s = zone->GetShape(shapeindex);
	Mesh* mesh = nullptr;
	Model* model = nullptr;
	s.GetRealShape(mesh, model);
	if (mesh) reqsiz += sizeof(int) + sizeof(int) * mesh->subMeshNum;
	else reqsiz += sizeof(int) + sizeof(matrix) * model->nodeCount;
	reqsiz += (QuestArr.size()+1) * sizeof(int);

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
	static_data.m_weaponHolstered = m_weaponHolstered;
	static_data.ReloadRemain = ReloadRemain;
	static_data.m_sniperDmrMode = m_sniperDmrMode;
	static_data.m_bomberHealAmmoMode = m_bomberHealAmmoMode;
	static_data.isGround = isGround;
	static_data.m_yaw = m_yaw;
	static_data.m_pitch = m_pitch;
	/*memcpy(static_data.Inventory, Inventory, sizeof(ItemStack) * maxItem);*/
	static_data.weapon[0] = weapon[0];
	static_data.weapon[1] = weapon[1];
	static_data.weapon[2] = weapon[2];
	static_data.SelectedWeapone = SelectedWeapon;
	static_data.Gold = Gold;
	static_data.Exp = Exp;
	static_data.Level = Level;
	offset += sizeof(STC_SyncObjData);

	//dynamic push

	// quest
	int& questCnt = *(int*)(sds.ofbuff + offset);
	questCnt = QuestArr.size();
	offset += sizeof(int);
	for (int i = 0; i < questCnt; ++i) {
		int& QI = *(int*)(sds.ofbuff + offset);
		QI = QuestArr[i];
		offset += sizeof(int);
	}
		
	//shape
	if (mesh) {
		int& submeshNum = *(int*)(sds.ofbuff + offset);
		submeshNum = mesh->subMeshNum;
		offset += sizeof(int);

		memcpy(sds.ofbuff + offset, material, sizeof(int) * mesh->subMeshNum);
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
	if (zones == nullptr || clientIndex < 0 || clientIndex >= gameworld.clients.size ||
		gameworld.clients.isnull(clientIndex)) {
		return;
	}
	//MaxHP -= m_tempMaxHpBonus;
	//if (MaxHP <= 0.0f) MaxHP = 100.0f;
	//m_tempMaxHpBonus = 0.0f;
	//m_tempMaxHpTimer = 0.0f;
	//m_iceBlockTimer = 0.0f;
	//m_iceBlockEffectFlow = 0.0f;
	//m_juggernautFlameTimer = 0.0f;
	//m_juggernautFlameTickFlow = 0.0f;
	//m_juggernautFlameEffectFlow = 0.0f;
	//m_juggernautFlameRange = 0.0f;
	//m_juggernautFlameRadius = 0.0f;
	//m_juggernautFlameDps = 0.0f;
	//m_frostPassiveUsed = false;
	//HP = MaxHP;
	//tag[GameObjectTag::Tag_Enable] = true;

	//matrix mat;
	//mat.Id();
	//mat.pos.y = 2;
	//SetWorld(mat);

	////player position send
	//bool isExist = true;
	//zones->Sending_ChangeGameObjectMember<Tag>(zones->CommonSDS, gameworld.clients[clientIndex].objindex, this, GameObjectType::_Player, &tag);
	matrix respawnWorld = worldMat;
	respawnWorld.pos = RespawnPosition;
	respawnWorld.pos.w = 1.0f;
	SetWorld(respawnWorld);

	LVelocity = 0;
	tickLVelocity = 0;
	isGround = false;
	collideCount = 0;
	tag[GameObjectTag::Tag_Enable] = true;
	HP = MaxHP;

	const int objindex = gameworld.clients[clientIndex].objindex;
	zones->Sending_ChangeGameObjectMember<Tag>(zones->CommonSDS, objindex, this, GameObjectType::_Player, &tag);
	zones->Sending_ChangeGameObjectMember<matrix>(zones->CommonSDS, objindex, this, GameObjectType::_Player, &worldMat);
	zones->Sending_ChangeGameObjectMember<float>(zones->CommonSDS, objindex, this, GameObjectType::_Player, &HP);
}

Monster::Monster() {
	static int up = 0;
	tempId = up;
	up += 1;

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
	TargetClientIndex = -1;
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
	constexpr bool MoveByTail = true;
	Zone* zone = gameworld.GetZone(zoneId);
	if (zone != nullptr && zone->BossPrototypeEnabled && zone->BossPrototypeIndex == zone->currentIndex) {
		tickLVelocity = 0;
		LVelocity = 0;
		return;
	}

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
			LVelocity.y += dronYMover;
			if (worldMat.pos.y < 3) dronYMover += deltaTime;
			else {
				if (worldMat.pos.y > 10) LVelocity.y = 0;
				LVelocity.x = (float)(rand() % 3 - 1);
				LVelocity.z = (float)(rand() % 3 - 1);
			}
			isGround = true;
		}
		else {
			if (isGround == false) {
				LVelocity.y -= 9.81f * deltaTime;
			}
		}
	
		if (MoveByTail) {
			bool b = CurrentTarget != nullptr;
			if (b) {
				if (vec4(CurrentTarget->worldMat.pos - worldMat.pos).len3 > 40) 
					CurrentTarget = nullptr;

				if (CurrentTarget != nullptr && vec4(CurrentTarget->worldMat.pos - worldMat.pos).len3 > 3) {
					// Tail Move
					vec4 minDir = 0;
					float minDist = 999999999;
					int off = CurrentTarget->tailOffset;
					for (int i = 0; i < 32; ++i) {
						off = off - 1;
						off &= 32;
						vec4 dir = (CurrentTarget->PosTail[off] - worldMat.pos);
						vec4 diff = CurrentTarget->worldMat.pos - CurrentTarget->PosTail[off];
						GameChunk* gc = zone->GetChunkFromPos(worldMat.pos);
						if (gc != nullptr) {
							bool RayCanMove = true;
							float dist = dir.len3;
							dir.len3 = 1;
							for (int k = 0; k < gc->Static_gameobjects.size; ++k) {
								for (int oi = 0; oi < gc->Static_gameobjects[k]->obbArr.size(); ++oi) {
									BoundingOrientedBox obb = gc->Static_gameobjects[k]->obbArr[oi];
									float outdist = 0;
									if (obb.Intersects(worldMat.pos, dir, outdist)) {
										if (outdist > dist) continue;
										RayCanMove = false;
										break;
									}
								}
								if (RayCanMove) break;
							}
							if (RayCanMove) {
								minDir = dir;
								break;
							}
						}
					}

					if (minDir.x != 0 || minDir.z != 0) {
						LVelocity.x = minDir.x * 5;
						LVelocity.z = minDir.z * 5;
						LookAt(vec4(minDir.x, 0, minDir.z, 0));
						if (m_monsterType == MonsterType::Dron) {
							LVelocity.y = minDir.y * 2;
						}
						else {
							if (isGround && zone->midHit() && minDir.y > 0) {
								LVelocity.y += 3;
							}
						}
					}
				}
			}
		}

		if (worldMat.pos.y < -50.0f) {
			worldMat.pos.y = 100.0f;
			LVelocity.y = 0;
		}

		tickLVelocity = LVelocity * deltaTime;
		bool hardControlled = HasStatusEffect(StatusEffectType::Freeze) ||
			HasStatusEffect(StatusEffectType::Stun) ||
			HasStatusEffect(StatusEffectType::Paralyze) ||
			HasStatusEffect(StatusEffectType::Hack);
		if (hardControlled) {
			tickLVelocity.x = 0.0f;
			tickLVelocity.z = 0.0f;
			m_isMove = false;
			return;
		}

		vec4 monsterPos = worldMat.pos;
		monsterPos.w = 0;

		//Astar Move
		if(!MoveByTail)
		{
			// Resolve ownership every tick without dereferencing a player that may have disconnected.
			if (TargetClientIndex >= 0) {
				if (TargetClientIndex >= gameworld.clients.size || gameworld.clients.isnull(TargetClientIndex) ||
					gameworld.clients[TargetClientIndex].pObjData != Target ||
					!gameworld.IsAdjacentZone(zoneId, gameworld.clients[TargetClientIndex].zoneId)) {
					Target = nullptr;
					TargetClientIndex = -1;
				}
			}
			if (Target == nullptr) {
				int limitseek = 4;
				if (gameworld.clients.size == 0) {
					Target = nullptr;
					TargetClientIndex = -1;
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
					Target = gameworld.clients[i].pObjData;
					TargetClientIndex = i;
					break;
				}
				if (limitseek != 0 && Target == nullptr) {
					targetSeekIndex = 0;
					goto SEEK_TARGET_LOOP;
				}
			}

			vec4 playerPos = Target->worldMat.pos;
			playerPos.w = 0;

			vec4 toPlayer = playerPos - monsterPos;
			toPlayer.y = 0.0f;
			float distanceToPlayer = toPlayer.len3;
			float effectiveSpeed = m_speed;
			if (HasStatusEffect(StatusEffectType::Slow)) effectiveSpeed *= 0.45f;

			// Chase target.
			if (distanceToPlayer <= m_chaseRange) {
				m_targetPos = playerPos;
				m_isMove = effectiveSpeed > 0.0f;

				if (distanceToPlayer > 0.0001f) {
					vec4 lookToPlayer = toPlayer;
					lookToPlayer.len3 = 1.0f;
					worldMat.SetLook(lookToPlayer);
				}

				if (effectiveSpeed > 0.0f && (path.empty() || currentPathIndex >= path.size())) {
					AstarNode* start = FindClosestNode(monsterPos.x, monsterPos.z, zone->allnodes);
					AstarNode* goal = FindClosestNode(playerPos.x, playerPos.z, zone->allnodes);

					if (start && goal) {
						path = AstarSearch(start, goal, zone->allnodes);
						currentPathIndex = 0;
					}
				}

				if (effectiveSpeed > 0.0f && !path.empty() && currentPathIndex < path.size()) {
					float speedScale = (m_speed > 0.0f) ? (effectiveSpeed / m_speed) : 0.0f;
					MoveByAstar(deltaTime * speedScale);
				}
				else if (effectiveSpeed > 0.0f) {
					if (distanceToPlayer > 0.0001f) {
						toPlayer.len3 = 1.0f;
						tickLVelocity += toPlayer * effectiveSpeed * deltaTime;
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

					const float effectRadius = (m_monsterType == MonsterType::Tower) ? 0.42f : 0.35f;
					const float effectPower = 18.0f;
					const float effectDuration = 0.20f;
					vec4 effectPosition = rayStart + rayDirection * 0.35f;

					if (m_monsterType == MonsterType::Dron) {
						vec4 right = worldMat.right;
						right.y = 0.0f;
						if (right.len3 < 0.0001f) {
							right = vec4(-rayDirection.z, 0.0f, rayDirection.x, 0.0f);
						}
						right.len3 = 1.0f;

						vec4 leftMuzzle = effectPosition - right * 0.8f;
						vec4 rightMuzzle = effectPosition + right * 0.8f;
						leftMuzzle.y -= 0.40f;
						rightMuzzle.y -= 0.40f;

						zone->Sending_SkillCast(zone->CommonSDS, zone->currentIndex, PlayerJob::Gunner, SkillSlot::Skill1,
							SkillEffectType::Rifle_GrenadeTrail, leftMuzzle, rayDirection, effectRadius, effectPower, effectDuration);
						zone->Sending_SkillCast(zone->CommonSDS, zone->currentIndex, PlayerJob::Gunner, SkillSlot::Skill1,
							SkillEffectType::Rifle_GrenadeTrail, rightMuzzle, rayDirection, effectRadius, effectPower, effectDuration);
					}
					else {
						effectPosition.y += 0.6f;
						if (m_monsterType == MonsterType::Tower) {
							effectPosition.y += 0.7f;
							effectPosition += rayDirection * 0.8f;
						}
						zone->Sending_SkillCast(zone->CommonSDS, zone->currentIndex, PlayerJob::Gunner, SkillSlot::Skill1,
							SkillEffectType::Rifle_GrenadeTrail, effectPosition, rayDirection, effectRadius, effectPower, effectDuration);
					}
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
		}

		if (zone->lowHit()) {
			if (CurrentTarget == nullptr) {
				GameObjectIncludeChunks goic = this->IncludeChunks;
				goic.xmin -= 1;
				goic.ymin -= 1;
				goic.zmin -= 1;
				goic.xlen += 2;
				goic.ylen += 2;
				goic.zlen += 2;
				int chunckSize = goic.GetChunckSize();
				ChunkIndex ci = ChunkIndex(goic.xmin, goic.ymin, goic.zmin);
				float CurrentDist = 999999;
				for (; ci.extra < chunckSize; goic.Inc(ci)) {
					auto f = zone->chunck.find(ci);
					if (f != zone->chunck.end()) {
						GameChunk* gc = f->second;
						for (int k = 0; k < gc->SkinMesh_gameobjects.size; ++k) {
							if (gc->SkinMesh_gameobjects.isnull(k)) continue;
							if (gc->SkinMesh_gameobjects[k]->tag[GameObjectTag::Tag_Player]) {
								Player* p = (Player*)gc->SkinMesh_gameobjects[k];
								float dist = vec4(p->worldMat.pos - worldMat.pos).fast_square_of_len3;
								if (dist < CurrentDist) {
									// 만약 이 플레이어를 따라갈 수 있다면 채택함.
									CurrentDist = dist;
									CurrentTarget = p;
								}
							}
						}
					}
				}
			}
			

			// Monster world matrix is broadcast centrally from Zone::Update().
			// Sending it here as well doubled monster movement packets under load.
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
	bool isBossPrototype = zone != nullptr && zone->BossPrototypeEnabled && zone->BossPrototypeIndex == zone->currentIndex;
	
	// Monster take damage with player's Attack
	void* vptr = *(void**)shooter;
	if (GameObjectType::VptrToTypeTable[vptr] == GameObjectType::_Player) {
		Player* p = (Player*)shooter;
		damage += p->Attack * 0.25f;
		cout << "Player Weapon Damage: " << damage << endl;
	}

	if (isBossPrototype) {
		if (zone->BossPrototypeShieldActive) return;
		if (zone->BossPrototypeGroggyTime > 0.0f) damage *= 1.3f;
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

			// monster kill quest prograss update
			for (int i = 0; i < p->QuestPrograss.size(); ++i) {
				Quest* prograss = p->QuestPrograss[i];
				for (int k = 0; k < prograss->requp; ++k) {
					QuestRequirement& req = prograss->ReqArr[k];
					constexpr int monsterId = 0; // fix : here write monsterID
					if (req.type == QuestType::KillMonster && req.ObjID == monsterId) {
						req.PresentCnt += 1;
					}
				}
			}
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

	bool isBossPrototype = zone->BossPrototypeEnabled &&
		zone->BossPrototypeIndex >= 0 &&
		zone->BossPrototypeIndex < zone->Dynamic_gameObjects.size &&
		zone->Dynamic_gameObjects.isnull(zone->BossPrototypeIndex) == false &&
		zone->Dynamic_gameObjects[zone->BossPrototypeIndex] == this;
	if (isBossPrototype) {
		if (zone->BossPrototypeShieldActive) return;
		if (zone->BossPrototypeGroggyTime > 0.0f) damage *= 1.3f;
	}

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
					Target = player;
					TargetClientIndex = clientIndex;
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
	while (zone->map.isStaticCollision(GetOBB())) {
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
	Zone* zone = gameworld.ZoneTable[zoneId];
	Shape& s = zone->GetShape(shapeindex);
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

		memcpy(sds.ofbuff + offset, material, sizeof(int) * mesh->subMeshNum);
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
	Zone* zone = gameworld.ZoneTable[zoneId];
	Shape& s = zone->GetShape(shapeindex);
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

PeacefulNPC::PeacefulNPC()
{
	tag = 0;
	tag[GameObjectTag::Tag_Enable] = false;
	tag[GameObjectTag::Tag_Dynamic] = true;
	tag[GameObjectTag::Tag_SkinMeshObject] = true;
	tag[GameObjectTag::Tag_Player] = false;
	worldMat.Id();
	shapeindex = -1;
	parent = -1;
	childs = -1;
	sibling = -1;
}

void PeacefulNPC::Update(float deltaTime) {
	Zone* zone = gameworld.GetZone(zoneId);
	if (collideCount == 0) isGround = false;
	collideCount = 0;

	if (isGround == false) {
		LVelocity.y -= 9.8f * deltaTime;
	}
	tickLVelocity = LVelocity * deltaTime;

	// 주변 플레이어를 탐색하여 대화를 시작할 수 있는지 점검한다.
	constexpr float TalkRange = 2.0f;
	GameObjectIncludeChunks goic = IncludeChunks;
	int len = goic.GetChunckSize();
	ChunkIndex ci = ChunkIndex(goic.xmin, goic.ymin, goic.zmin);
	for (; ci.extra < len; goic.Inc(ci)) {
		auto f = zone->chunck.find(ci);
		if (f != zone->chunck.end()) {
			GameChunk* gc = f->second;
			for (int i = 0; i < gc->SkinMesh_gameobjects.size; ++i) {
				if (gc->SkinMesh_gameobjects.isnull(i)) continue;
				SkinMeshGameObject* skgo = gc->SkinMesh_gameobjects[i];
				if (skgo == nullptr) continue;
				
				Tag::TagSetter ts = skgo->tag[GameObjectTag::Tag_Player];

				if (skgo->tag[GameObjectTag::Tag_Player] == true) {
					Player* pgo = (Player*)skgo;
					vec4 distance = worldMat.pos - pgo->worldMat.pos;

					bool PressE = pgo->InputBuffer[InputID::KeyboardE];
					if ((PressE && distance.len3 < TalkRange) && pgo->PresentTalkID == -1) {

						bool QuestCompleteExist = false;
						// 완료처리하기
						for (int k = 0; k < NPCQuestList.size(); ++k) {
							int questId = NPCQuestList[k];
							if (pgo->PrograssQuestBitArr[questId] == true) {
								int selectedQI = -1;
								for (int qi = 0; qi < pgo->QuestArr.size(); ++qi) {
									if (pgo->QuestArr[qi] == questId) {
										selectedQI = qi;
										break;
									}
								}

								if (selectedQI >= 0) {
									if (pgo->QuestPrograss[selectedQI]->isComplete()) {
										// 플레이어가 보상을 얻게 한다.
										pgo->RewardQuest(pgo->QuestPrograss[selectedQI]);
										
										// Delete Quest Protocol
										zone->Sending_DeleteQuest(gameworld.clients[pgo->clientIndex].PersonalSDS, questId);

										QuestCompleteExist = true;
										pgo->EraseQuest(selectedQI);
										break;
									}
								}
							}
						}

						if (QuestCompleteExist == false) {
							// 특정 함수를 통해 StartID를 반환해야 함.
							// 만약 StartID를 반환하지 않으면 
							int StartID = gameworld.GetStartTalk(pgo, this);

							zone->Sending_NPCStartTalk(gameworld.clients[pgo->clientIndex].PersonalSDS, NPCType, StartID);

							if (gameworld.NPCTalkTable[StartID].selectCnt > 0) {
								pgo->PresentTalkID = StartID;
							}
							else {
								pgo->PresentTalkID = -1;
							}
						}
					}
				}
			}
		}
	}
}

void PeacefulNPC::Release() {

}

void PeacefulNPC::OnCollision(GameObject* other)
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

void PeacefulNPC::OnStaticCollision(BoundingOrientedBox obb)
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

void PeacefulNPC::SendGameObject(int objindex, SendDataSaver& sds) {
	sds.postpush_start();

	//calculate app packet siz.
	int reqsiz = sizeof(STC_SyncGameObject_Header) + sizeof(STC_SyncObjData);
	Zone* zone = gameworld.ZoneTable[zoneId];
	Shape& s = zone->GetShape(shapeindex);
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
	header.type = GameObjectType::_PeacefulNPC;
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
	static_data.npctype = NPCType;
	static_data.NPCID = NPCID;
	offset += sizeof(STC_SyncObjData);

	//dynamic push

	//shape
	if (mesh) {
		int& submeshNum = *(int*)(sds.ofbuff + offset);
		submeshNum = mesh->subMeshNum;
		offset += sizeof(int);

		memcpy(sds.ofbuff + offset, material, sizeof(int) * mesh->subMeshNum);
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
	commonMap.LoadMap(staticZone->zoneName);
	if (commonMap.MapObjects.empty()) {
		cout << "[World] common map load failed: " << staticZone->zoneName << endl;
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

	cout << "[World] common map=" << staticZone->zoneName
		<< " AABB.min=(" << commonMap.AABB[0].f3.x << ", " << commonMap.AABB[0].f3.y << ", " << commonMap.AABB[0].f3.z << ")"
		<< " AABB.max=(" << commonMap.AABB[1].f3.x << ", " << commonMap.AABB[1].f3.y << ", " << commonMap.AABB[1].f3.z << ")"
		<< " MapObjectCount=" << commonMap.MapObjects.size() << endl;
}

void World::Init() {
	gameworld.TIMER_STATICINIT();

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
	for (int i = 0; i < 256; ++i) {
		pendingPeerSocket[i] = INVALID_SOCKET;
	}
	GameObjectType::STATICINIT();

	//Quest Table
	{
		Quest* q = new Quest(L"First Quest", L"Collect 7 items for 1000 gold!");
		q->PushReq(QuestType::CollectItem, 1, 7);
		q->PushReward(QuestRewardType::QRT_Money, 0, 1000);

		QuestTable.push_back(q);
	}

	//NPC Talk Table
	{
		NPCTalkTable.push_back(NPCTalkData(L"안녕? 반가워!", false, TalkSelection(L"당연하지! 들어줄게!", 'q', 0), TalkSelection(L"지금은 바빠. 나중에.", true, 3)));
		NPCTalkTable.push_back(NPCTalkData());
		NPCTalkTable.push_back(NPCTalkData());
		NPCTalkTable.push_back(NPCTalkData());
	}


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

		Model* SupportDroneModel = new Model();
		SupportDroneModel->LoadModelFile2("Resources/Model/Drone.model");
		int sdroneMesh_index = Shape::AddModel("SupportDrone", SupportDroneModel);
		//game.SupportDroneModel = SupportDroneModel;

		Model* TurretModel = new Model();
		TurretModel->LoadModelFile2("Resources/Model/Exo.model");
		int turretMesh_index = Shape::AddModel("MonsterTurret", TurretModel);

		Mesh* portalMesh = new Mesh();
		portalMesh->CreateWallMesh(2.0f, 3.0f, 0.2f);
		Shape::AddMesh("Portal", portalMesh);

		//Item
		{
			int globalitem_index = 0;
			Shape BlackShape;
			BlackShape.FlagPtr = 0;
			ItemTable.push_back(Item(globalitem_index, ItemType::_NULL, L"", nullptr)); // blank space in inventory.
			globalitem_index += 1;

			auto AddItemFunc = [&](int id, ItemType t, const char* ItemName, const char* modelfilepath, const wchar_t* ItemIconPath, const wchar_t* ItemDescription, void* Dataptr) {
	// [shape-index align] Register a placeholder shape (null model) per item so the client and
	// server GLOBAL shape tables have identical indices. Without this, the client's 7 item shapes
	// (indices 4..10) shift every later index, so a shapeindex sent by the server resolves to the
	// wrong/garbage shape on the client -> null Model -> shadow-render crash.
	// The server never renders or collides item shapes, so it only needs the index slot. It must
	// NOT load the real model/texture: the server has no graphics device, and the model files may
	// be missing from its Resources folder (loading them crashed startup). null model is safe here
	// because Shape::AddModel only stores the pointer; the server never dereferences it.
				ItemTable.push_back(Item(globalitem_index, t, ItemDescription, Dataptr));
				};

			AddItemFunc(globalitem_index, ItemType::_Consumable, "BioFix",
				"Resources/Model/ItemModel/BioFix.model",
				L"Resources/UI/ItemIcons/ItemIcon_BioFix.png",
				L"바이오픽스 \n HP+20", nullptr);
			globalitem_index += 1;

			AddItemFunc(globalitem_index, ItemType::_Consumable, "Tier4Gear",
				"Resources/Model/ItemModel/Tier4Gear.model",
				L"Resources/UI/ItemIcons/ItemIcon_Tear4Gear.png",
				L"티어 4Gear", nullptr);
			globalitem_index += 1;

			
			AddItemFunc(globalitem_index, ItemType::_Weapon, "Sniper_IronSight",
				"Resources/Model/sniper.model",
				L"Resources/UI/ItemIcons/ItemIcon_IronSight.png",
				L"[�������� - ���̾����Ʈ] : ���� ������� ����� �޸��� ���� ���� ��ź ���ݼ���.",
				new Weapon(WeaponType::Sniper));
			globalitem_index += 1;

			AddItemFunc(globalitem_index, ItemType::_Weapon, "Rifle_StreetSweeper",
				"Resources/Model/Rifle.model",
				L"Resources/UI/ItemIcons/ItemIcon_StreetSweeper.png",
				L"[���ݼ��� - ��Ʈ��Ʈ������] : �Ѷ� �Ÿ��� ������ȴ� Ŭ���� ���ݼ���. ������ ���� ���� ���̴�.",
				new Weapon(WeaponType::Rifle));
			globalitem_index += 1;

			AddItemFunc(globalitem_index, ItemType::_Weapon, "Pistol_DoubleTroble",
				"Resources/Model/pistol.model",
				L"Resources/UI/ItemIcons/ItemIcon_DoubleTroble.png",
				L"[�ֱ��� - ����Ʈ����] : ������ ������ �� ��� ��ƺ��̴� ������ �ֱ���.",
				new Weapon(WeaponType::Pistol));
			globalitem_index += 1;

			AddItemFunc(globalitem_index, ItemType::_Weapon, "ShotGun_SlagShot",
				"Resources/Model/shootgun.model",
				L"Resources/UI/ItemIcons/ItemIcon_SlagShot.png",
				L"[���� - �����׽�] : ���� ��⸦ ��� ���� ��ĥ�� ����� ��. ���� ���̶� ���ɵ� �״��� ���� �ʴ�.", new Weapon(WeaponType::Shotgun));
			globalitem_index += 1;

			AddItemFunc(globalitem_index, ItemType::_Weapon, "MachineGun_Ratler",
				"Resources/Model/minigun.model",
				L"Resources/UI/ItemIcons/ItemIcon_Ratler.png",
				L"[�ӽŰ� - ��Ʋ��] : ���� ��ǰ�� �����Ÿ��� �Ҹ����� ���� �̸�. ���� ����������� �� �� ����.", new Weapon(WeaponType::MachineGun));
			globalitem_index += 1;
		}
	}

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

	gameworld.ZoneTable.reserve(256);
	char ZoneName[128] = "Zone_0_0";
	for (int iz = 0; iz < 10; ++iz) {
		for (int ix = 0; ix < 10; ++ix) {
			ZoneName[7] = '0' + iz;
			ZoneName[5] = '0' + ix;

			// Make Zone
			Zone* tempZone = new Zone(iz * 10 + ix, ZoneName, ix, iz);
			gameworld.ZoneTable.push_back(tempZone);
		}
	}

	// [party/dungeon] append the 3 isolated dungeon-floor zones as ZoneTable[100..102].
	// Far, spaced-out coords keep them out of grid adjacency AND non-adjacent to each other
	// (no peer/ghost links between floors); each gets a distinct descriptor offset.
	{
		// Floor coords chosen so the render offset (client: OffsetMulArr[y%3][x%3]) lands on FREE descriptor
		// slots, not colliding with open-world zones 73/74/83/84 (offsets 2/0/7/4). 1F->1, 2F->3, Boss->8.
		// [party] instance 0: zones 100..102.
		gameworld.ZoneTable.push_back(new Zone(World::DungeonZoneId + 0, "OfficeDungeon_1floor", 1000, 1002)); // 1F (off 1)
		gameworld.ZoneTable.push_back(new Zone(World::DungeonZoneId + 1, "OfficeDungeon_2floor", 1001, 1000)); // 2F (off 3)
		gameworld.ZoneTable.push_back(new Zone(World::DungeonZoneId + 2, "OfficeDungeon_Boss",   1001, 1010)); // Boss (off 8)
		// [party] instance 1: zones 103..105. Coords are instance-0 + x100 so they are non-adjacent to every
		// other dungeon/open-world zone (separate ghost/visibility) yet reuse the same layout. A client only
		// ever loads ONE instance (it reconnects fresh and instances are non-adjacent), so the fact that some
		// of these coords map to an already-used Asset_OffsetMul slot is harmless (never co-loaded).
		gameworld.ZoneTable.push_back(new Zone(World::DungeonZoneId + 3, "OfficeDungeon_1floor", 1100, 1102)); // inst1 1F
		gameworld.ZoneTable.push_back(new Zone(World::DungeonZoneId + 4, "OfficeDungeon_2floor", 1101, 1100)); // inst1 2F
		gameworld.ZoneTable.push_back(new Zone(World::DungeonZoneId + 5, "OfficeDungeon_Boss",   1101, 1110)); // inst1 Boss
	}

	//Set Near Zones
	for (int i = 0; i < gameworld.ZoneTable.size(); ++i) {
		Zone* tempZone = gameworld.ZoneTable[i];
		tempZone->nearZones[0] = tempZone;
		int nearSiz = 1;
		int nearSiz_5 = 5;
		for (int k = 0; k < gameworld.ZoneTable.size(); ++k) {
			Zone* tempZone2 = gameworld.ZoneTable[k];
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

	// Zone initialization owns dynamic containers only. The map is loaded once by World.
	for (int i = 0; i < gameworld.ZoneTable.size(); ++i) {
		Zone* z = gameworld.ZoneTable[i];
		z->Set_world_id_pos(this, i, z->x, z->y);
		if (IsZoneOwned(i) == false) continue;
		z->Init();
	}

	//InitCommonMap(); // 이미 Init 내부에 있음.

	for (int i = 0; i < gameworld.ZoneTable.size(); ++i) {
		Zone* z = gameworld.ZoneTable[i];
		if (IsZoneOwned(i) == false) continue;
		z->GridCollisionCheck();
		z->SpawnObjects();
		z->Spawnboundary();
		z->SpawnPortal();   // [party/dungeon] dungeon-entry portal (open-world zones only; skips dungeon)
	}
}

void World::Update() {
	for (int i = 0; i < ZoneTable.size(); ++i) {
		if (IsZoneOwned(i) == false) continue;
		gameworld.ZoneTable[i]->Update(DeltaTime);
	}

	// [party] keep every waiting party room (HP/job/leader) live each tick (queued into PersonalSDS).
	for (int p = 0; p < MaxParties; ++p) {
		if (parties[p].active && !parties[p].started) BroadcastPartyRoom(p);
	}

	// [party] dungeon server: free + reset any instance whose floors have emptied out.
	DungeonUpdateInstances();

	// [party/dungeon] in the dungeon, share the in-dungeon party's HP/job on a ~0.2s throttle
	// (few members -> light) using the same party UI.
	static float s_dungeonPartyHpTimer = 0.0f;
	s_dungeonPartyHpTimer += DeltaTime;
	if (s_dungeonPartyHpTimer >= 0.2f) {
		s_dungeonPartyHpTimer = 0.0f;
		BroadcastDungeonParty();
	}

	for (int i = 0; i < ZoneTable.size(); ++i) {
		if (IsZoneOwned(i) == false) continue;
		gameworld.ZoneTable[i]->FlushSendToClients();
	}
}

// [party] ---- portal party formation (lobby side) ----
int World::PartyFindSlotByClient(int clientIndex) {
	if (clientIndex < 0) return -1;
	for (int p = 0; p < MaxParties; ++p) {
		if (!parties[p].active) continue;
		for (int m = 0; m < parties[p].memberCount; ++m)
			if (parties[p].members[m] == clientIndex) return p;
	}
	return -1;
}

int World::PartyFindSlotById(int partyId) {
	if (partyId < 0) return -1;
	for (int p = 0; p < MaxParties; ++p)
		if (parties[p].active && parties[p].id == partyId) return p;
	return -1;
}

int World::PartyAllocSlot() {
	for (int p = 0; p < MaxParties; ++p)
		if (!parties[p].active) return p;
	return -1;
}

void World::PartyCreate(int clientIndex) {
	if (clientIndex < 0 || clientIndex >= clients.size || clients.isnull(clientIndex)) return;
	if (PartyFindSlotByClient(clientIndex) >= 0) return;   // already in a party
	int slot = PartyAllocSlot();
	if (slot < 0) return;                                   // lobby party table full

	LobbyParty& party = parties[slot];
	party = LobbyParty();
	party.active = true;
	party.started = false;
	party.id = serverId * 100000 + (nextPartyId++);         // serverId-scoped so ids never collide across lobbies
	// reuse display numbers: pick the smallest number not currently used by an active party,
	// so disbanding 파티1 frees the name and the next created party becomes 파티1 again.
	int num = 1;
	for (bool taken = true; taken; ) {
		taken = false;
		for (int p = 0; p < MaxParties; ++p) {
			if (parties[p].active && parties[p].number == num) { taken = true; ++num; break; }
		}
	}
	party.number = num;
	party.leaderClientIndex = clientIndex;
	party.members[0] = clientIndex;
	party.memberCount = 1;

	cout << "[Party] create #" << party.number << " (id=" << party.id << ") leader client=" << clientIndex << endl;
	BroadcastPartyRoom(slot);
	BroadcastPartyListToMembers();
}

void World::PartyJoin(int clientIndex, int partyId) {
	if (clientIndex < 0 || clientIndex >= clients.size || clients.isnull(clientIndex)) return;
	if (PartyFindSlotByClient(clientIndex) >= 0) return;    // already in a party
	int slot = PartyFindSlotById(partyId);
	if (slot < 0) return;                                    // no such party
	LobbyParty& party = parties[slot];
	if (party.started) return;                               // already entered the dungeon
	if (party.memberCount >= DungeonPartyMax) return;        // full

	party.members[party.memberCount++] = clientIndex;
	cout << "[Party] join #" << party.number << " client=" << clientIndex
		<< " (" << party.memberCount << "/" << DungeonPartyMax << ")" << endl;
	BroadcastPartyRoom(slot);
	BroadcastPartyListToMembers();
}

void World::PartyLeave(int clientIndex) {
	int slot = PartyFindSlotByClient(clientIndex);
	if (slot < 0) return;
	LobbyParty& party = parties[slot];

	// compact the member list, dropping the leaver.
	int w = 0;
	for (int m = 0; m < party.memberCount; ++m)
		if (party.members[m] != clientIndex) party.members[w++] = party.members[m];
	for (int m = w; m < DungeonPartyMax; ++m) party.members[m] = -1;
	party.memberCount = w;

	// clear the leaver's own party UI.
	if (clientIndex >= 0 && clientIndex < clients.size && clients.isnull(clientIndex) == false)
		Sending_DungeonQueueUpdate(clients[clientIndex].PersonalSDS, nullptr, 0, -1, -1);

	if (party.memberCount <= 0) {
		cout << "[Party] disband #" << party.number << endl;
		party.active = false;
		party.id = -1;
	} else {
		if (party.leaderClientIndex == clientIndex) party.leaderClientIndex = party.members[0]; // promote
		BroadcastPartyRoom(slot);
	}
	BroadcastPartyListToMembers();
}

void World::BroadcastPartyRoom(int partySlot) {
	if (partySlot < 0 || partySlot >= MaxParties) return;
	LobbyParty& party = parties[partySlot];
	if (!party.active) return;
	for (int m = 0; m < party.memberCount; ++m) {
		int ci = party.members[m];
		if (ci < 0 || ci >= clients.size || clients.isnull(ci)) continue;
		Sending_DungeonQueueUpdate(clients[ci].PersonalSDS, party.members, party.memberCount, party.leaderClientIndex, party.id, party.number);
	}
}

void World::PartyDisband(int clientIndex) {
	int slot = PartyFindSlotByClient(clientIndex);
	if (slot < 0) return;
	LobbyParty& party = parties[slot];
	if (party.leaderClientIndex != clientIndex) return;   // only the leader can disband
	if (party.started) return;                            // already entered the dungeon

	// clear every member's party window before tearing the party down.
	for (int m = 0; m < party.memberCount; ++m) {
		int ci = party.members[m];
		if (ci >= 0 && ci < clients.size && clients.isnull(ci) == false)
			Sending_DungeonQueueUpdate(clients[ci].PersonalSDS, nullptr, 0, -1, -1, 0);
	}

	cout << "[Party] disband(by leader) #" << party.number << " (id=" << party.id << ")" << endl;
	party.active = false;
	party.id = -1;
	party.leaderClientIndex = -1;
	for (int m = 0; m < DungeonPartyMax; ++m) party.members[m] = -1;
	party.memberCount = 0;

	BroadcastPartyListToMembers();
}

void World::BroadcastPartyListToMembers() {
	// Anyone currently sitting in a party (i.e. with a party window open) gets a fresh list.
	for (int ci = 0; ci < clients.size; ++ci) {
		if (clients.isnull(ci)) continue;
		if (clients[ci].isServerPeer || clients[ci].socket == INVALID_SOCKET) continue;
		if (PartyFindSlotByClient(ci) < 0) continue;
		Sending_PartyList(clients[ci].PersonalSDS);
	}
}

void World::Sending_PartyList(SendDataSaver& sds) {
	sds.postpush_start();
	constexpr int reqsiz = sizeof(STC_PartyList_Header);
	sds.postpush_reserve(reqsiz);
	STC_PartyList_Header& header = *(STC_PartyList_Header*)sds.ofbuff;
	header.size = reqsiz;
	header.st = STC_Protocol::PartyList;
	int n = 0;
	for (int p = 0; p < MaxParties && n < MaxPartyListEntries; ++p) {
		if (!parties[p].active) continue;
		PartyListEntry& e = header.parties[n++];
		e.partyId = parties[p].id;
		e.number = parties[p].number;
		e.memberCount = parties[p].memberCount;
		e.maxCount = DungeonPartyMax;
		e.started = parties[p].started ? 1 : 0;
	}
	header.count = n;
	sds.postpush_end();
}

// [party/dungeon] Players currently inside the dungeon floors ARE the party for that instance (the lobby
// queue was cleared on entry). Collect them and send each one the party HP/job list, reusing the same
// STC_DungeonQueueUpdate the client already renders. On servers that don't own dungeon zones this finds
// no in-dungeon players and is a no-op. (Assumes one party per dungeon instance; caps at DungeonPartyMax.)
void World::BroadcastDungeonParty() {
	// Build a roster per exact dungeon floor. A reconnecting client can briefly remain on the previous
	// floor; collecting the whole dungeon range made that stale connection appear as a duplicate member.
	for (int recipient = 0; recipient < clients.size; ++recipient) {
		if (clients.isnull(recipient)) continue;
		ClientData& target = clients[recipient];
		if (target.isServerPeer || target.socket == INVALID_SOCKET || target.pObjData == nullptr) continue;
		const int targetZone = target.zoneId;
		// per-exact-floor roster works for every instance, since each floor zone id is unique per instance.
		if (!IsDungeonZone(targetZone)) continue;

		int members[DungeonPartyMax] = { -1, -1, -1 };
		int n = 0;
		for (int ci = 0; ci < clients.size && n < DungeonPartyMax; ++ci) {
			if (clients.isnull(ci)) continue;
			ClientData& candidate = clients[ci];
			if (candidate.isServerPeer || candidate.socket == INVALID_SOCKET || candidate.pObjData == nullptr) continue;
			if (candidate.zoneId != targetZone) continue;
			if (candidate.pObjData->clientIndex != ci) continue;
			members[n++] = ci;
		}
		if (n > 0) Sending_DungeonQueueUpdate(target.PersonalSDS, members, n);
	}
}

// [party] helper: a sensible spawn point (AABB center, slightly above the floor) for a dungeon zone.
static vec4 DungeonZoneSpawn(Zone* z) {
	if (z == nullptr) return vec4(0, 5, 0, 1);
	return vec4(
		0.5f * (z->BasicAABB_onlyXZ.x + z->BasicAABB_onlyXZ.z),
		z->map.AABB[0].y + 1.5f,
		0.5f * (z->BasicAABB_onlyXZ.y + z->BasicAABB_onlyXZ.w),
		1.0f);
}

// [party] the LEADER pressed start: route every member to the dungeon entry, tagged with this party id.
// The dungeon server authoritatively assigns an instance for that party on arrival (or bounces them back
// if every instance is busy), so the lobby does not need to know which instance is free.
void World::PartyLeaderStart(int clientIndex) {
	int slot = PartyFindSlotByClient(clientIndex);
	if (slot < 0) return;
	LobbyParty& party = parties[slot];
	if (party.leaderClientIndex != clientIndex) return;   // only the leader may start
	if (party.started) return;

	int members[DungeonPartyMax];
	int n = party.memberCount;
	for (int i = 0; i < n; ++i) members[i] = party.members[i];
	const int partyId = party.id;
	const int number = party.number;

	cout << "[Party] START #" << number << " (id=" << partyId << ") members=" << n << endl;

	Zone* entryZone = (DungeonZoneId >= 0 && DungeonZoneId < (int)ZoneTable.size()) ? ZoneTable[DungeonZoneId] : nullptr;
	vec4 spawnPos = DungeonZoneSpawn(entryZone);

	for (int i = 0; i < n; ++i) {
		int ci = members[i];
		if (ci < 0 || ci >= clients.size || clients.isnull(ci)) continue;
		Sending_DungeonQueueUpdate(clients[ci].PersonalSDS, nullptr, 0, -1, -1);  // close the lobby party UI
		MovePlayerToZone(ci, DungeonZoneId, spawnPos, partyId, clients[ci].zoneId);
	}

	// the lobby hands this group off to the dungeon; remove the lobby party entry.
	party.active = false;
	party.started = true;
	party.id = -1;
	BroadcastPartyListToMembers();
}

// [party] dungeon-side instance pool -----------------------------------------------------------
int World::DungeonInstanceForPartyId(int partyId) {
	if (partyId < 0) return -1;
	for (int i = 0; i < DungeonInstanceCount; ++i)
		if (dungeonInstances[i].busy && dungeonInstances[i].partyId == partyId) return i;
	return -1;
}

int World::DungeonAllocInstanceForParty(int partyId, int number) {
	int existing = DungeonInstanceForPartyId(partyId);
	if (existing >= 0) return existing;                  // same party -> same instance
	for (int i = 0; i < DungeonInstanceCount; ++i) {
		if (dungeonInstances[i].busy) continue;
		dungeonInstances[i].busy = true;
		dungeonInstances[i].partyId = partyId;
		dungeonInstances[i].number = number;
		cout << "[Dungeon] instance " << i << " (zones " << DungeonInstanceBase(i) << ".."
			<< (DungeonInstanceBase(i) + DungeonFloorCount - 1) << ") -> party id=" << partyId << endl;
		return i;
	}
	return -1;   // every instance busy
}

int World::CountPlayersInInstance(int instanceIndex) {
	if (instanceIndex < 0 || instanceIndex >= DungeonInstanceCount) return 0;
	int base = DungeonInstanceBase(instanceIndex);
	int count = 0;
	for (int ci = 0; ci < clients.size; ++ci) {
		if (clients.isnull(ci)) continue;
		ClientData& c = clients[ci];
		if (c.isServerPeer || c.socket == INVALID_SOCKET || c.pObjData == nullptr) continue;
		if (c.zoneId >= base && c.zoneId < base + DungeonFloorCount) ++count;
	}
	return count;
}

void World::DungeonUpdateInstances() {
	if (!IsDungeonZone(ownedZoneId)) return;             // only the dungeon server manages instances
	for (int i = 0; i < DungeonInstanceCount; ++i) {
		if (!dungeonInstances[i].busy) continue;
		if (CountPlayersInInstance(i) > 0) continue;     // still occupied
		cout << "[Dungeon] instance " << i << " emptied -> reset + free (party id was "
			<< dungeonInstances[i].partyId << ")" << endl;
		ResetDungeonInstance(i);
		dungeonInstances[i].busy = false;
		dungeonInstances[i].partyId = -1;
		dungeonInstances[i].number = 0;
	}
}

void World::ResetDungeonInstance(int instanceIndex) {
	if (instanceIndex < 0 || instanceIndex >= DungeonInstanceCount) return;
	int base = DungeonInstanceBase(instanceIndex);
	for (int f = 0; f < DungeonFloorCount; ++f) {
		int zid = base + f;
		if (zid < 0 || zid >= (int)ZoneTable.size()) continue;
		if (!IsZoneOwned(zid)) continue;
		Zone* z = ZoneTable[zid];
		if (z == nullptr) continue;

		// revive every monster on this floor.
		for (int oi = 0; oi < z->Dynamic_gameObjects.size; ++oi) {
			if (z->Dynamic_gameObjects.isnull(oi)) continue;
			DynamicGameObject* obj = z->Dynamic_gameObjects[oi];
			if (obj == nullptr) continue;
			if (GameObjectType::VptrToTypeTable[*(void**)obj] != GameObjectType::_Monster) continue;
			Monster* mon = (Monster*)obj;
			mon->HP = mon->MaxHP;
			mon->isDead = false;
			mon->respawntimer = 0.0f;
			z->Sending_ChangeGameObjectMember<float>(z->CommonSDS, z->currentIndex, mon, GameObjectType::_Monster, &mon->HP);
			z->Sending_ChangeGameObjectMember<bool>(z->CommonSDS, z->currentIndex, mon, GameObjectType::_Monster, &mon->isDead);
		}

		// clear any loot left on the floor.
		for (int di = 0; di < z->DropedItems.size; ++di) {
			if (z->DropedItems.isnull(di)) continue;
			z->DropedItems.Free(di);
			z->Sending_ItemRemove(z->CommonSDS, di);
		}
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

bool World::SendPlayerTransferToServer(const PlayerTransferData& data) {
	// [지연 개선] 상시 peer 링크가 있으면 일회성 소켓 대신 그걸로 보낸다.
	// 데이터가 클라 재접속보다 먼저 도착 -> AcceptTransferConnect 가 토큰을 바로 찾아 대기 없음.
	// Drain queued ghost packets first. Never write a transfer packet directly behind a partially sent
	// snapshot, otherwise the TCP stream loses its packet boundary.
	FlushPeerSends();
	for (int i = 0; i < clients.size; ++i) {
		if (clients.isnull(i)) continue;
		if (clients[i].isServerPeer == false) continue;
		if (clients[i].peerServerId != data.dstZoneId) continue;
		if (clients[i].PendingSDS.size > clients[i].pendingSendOffset) continue;
		CTS_ServerPlayerTransfer_Header header = {};
		header.size = sizeof(CTS_ServerPlayerTransfer_Header);
		header.st = CTS_Protocol::ServerPlayerTransfer;
		header.data = data;
		const char* sendCursor = reinterpret_cast<const char*>(&header);
		int sendRemain = sizeof(header);
		const ULONGLONG sendDeadlineMs = GetTickCount64() + 100;
		while (sendRemain > 0) {
			const int sent = send(clients[i].socket, sendCursor, sendRemain, 0);
			if (sent > 0) {
				sendCursor += sent;
				sendRemain -= sent;
				continue;
			}
			if (sent != SOCKET_ERROR || WSAGetLastError() != WSAEWOULDBLOCK ||
				GetTickCount64() >= sendDeadlineMs) {
				break;
			}
			fd_set writeSet;
			FD_ZERO(&writeSet);
			FD_SET(clients[i].socket, &writeSet);
			timeval timeout = {};
			timeout.tv_usec = 10000;
			select(0, nullptr, &writeSet, nullptr, &timeout);
		}
		return sendRemain == 0;
	}

	// 폴백: peer 링크가 아직 없으면 기존 일회성 소켓 사용.
	SOCKET sock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, 0);
	if (sock == INVALID_SOCKET) {
		return false;
	}
	u_long nonBlocking = 1;
	if (ioctlsocket(sock, FIONBIO, &nonBlocking) != 0) {
		closesocket(sock);
		return false;
	}
	BOOL noDelay = TRUE;
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
		reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));

	sockaddr_in serveraddr = {};
	serveraddr.sin_family = AF_INET;
	inet_pton(AF_INET, GetZoneIP(data.dstZoneId), &serveraddr.sin_addr);
	serveraddr.sin_port = htons(GetZonePort(data.dstZoneId));
	if (connect(sock, (sockaddr*)&serveraddr, sizeof(serveraddr)) == SOCKET_ERROR) {
		const int connectError = WSAGetLastError();
		if (connectError != WSAEWOULDBLOCK && connectError != WSAEINPROGRESS &&
			connectError != WSAEALREADY && connectError != WSAEINVAL) {
			closesocket(sock);
			return false;
		}

		fd_set writeSet;
		fd_set errorSet;
		FD_ZERO(&writeSet);
		FD_ZERO(&errorSet);
		FD_SET(sock, &writeSet);
		FD_SET(sock, &errorSet);
		timeval timeout = {};
		timeout.tv_usec = 100000;
		if (select(0, nullptr, &writeSet, &errorSet, &timeout) <= 0) {
			closesocket(sock);
			return false;
		}

		int socketError = 0;
		int socketErrorLength = sizeof(socketError);
		getsockopt(sock, SOL_SOCKET, SO_ERROR,
			reinterpret_cast<char*>(&socketError), &socketErrorLength);
		if (socketError != 0 || !FD_ISSET(sock, &writeSet)) {
			closesocket(sock);
			return false;
		}
	}

	CTS_ServerPlayerTransfer_Header header = {};
	header.size = sizeof(CTS_ServerPlayerTransfer_Header);
	header.st = CTS_Protocol::ServerPlayerTransfer;
	header.data = data;

	const char* sendCursor = reinterpret_cast<const char*>(&header);
	int sendRemain = sizeof(header);
	const ULONGLONG sendDeadlineMs = GetTickCount64() + 100;
	while (sendRemain > 0) {
		const int sent = send(sock, sendCursor, sendRemain, 0);
		if (sent > 0) {
			sendCursor += sent;
			sendRemain -= sent;
			continue;
		}
		if (sent != SOCKET_ERROR || WSAGetLastError() != WSAEWOULDBLOCK ||
			GetTickCount64() >= sendDeadlineMs) {
			break;
		}

		fd_set writeSet;
		FD_ZERO(&writeSet);
		FD_SET(sock, &writeSet);
		timeval timeout = {};
		timeout.tv_usec = 10000;
		select(0, nullptr, &writeSet, nullptr, &timeout);
	}
	closesocket(sock);
	return sendRemain == 0;
}

// [4단계-STEP1] 아직 연결 안 된 인접 존 서버에 상시 복제 링크를 건다.
// 규칙: '낮은 zoneId 서버가 높은 쪽으로' 한 번만 connect (쌍당 1개의 TCP 연결, 전이중 사용).
// 연결에 성공하면 그 소켓을 clients[] 에 isServerPeer 슬롯으로 넣어, 기존 poll/Receiving 경로로
// 메시지를 주고받게 한다(별도 폴링 인프라 불필요). 이웃이 아직 안 떠 있으면 조용히 다음 기회에 재시도.
void World::TryConnectPeers() {
	if (NotMakePeer) return;
	if (singleProcessAllZones) return; // 단일 프로세스는 이웃이 같은 메모리에 있어 링크가 필요 없다.

	const ULONGLONG nowMs = GetTickCount64();
	for (int zi = 0; zi < ZoneTable.size(); ++zi) {
		if (zi == ownedZoneId) continue;
		if (IsAdjacentZone(ownedZoneId, zi) == false) continue;
		if (zi <= ownedZoneId) continue;   // Lower zoneId owns the single outbound link.
		bool alreadyRegistered = false;
		for (int ci = 0; ci < clients.size; ++ci) {
			if (clients.isnull(ci)) continue;
			if (!clients[ci].isServerPeer || clients[ci].peerServerId != zi) continue;
			alreadyRegistered = true;
			break;
		}
		if (alreadyRegistered) {
			peerLinkUp[zi] = true;
			continue;
		}
		if (peerLinkUp[zi]) continue;      // 이미 연결됨
		if (pendingPeerConnect[zi]) continue;
		if (nowMs < peerRetryAfterMs[zi]) continue;
		
		Zone* otherZone = ZoneTable[zi];
		if (World::minx > otherZone->x || otherZone->x > World::maxx) continue;
		if (World::miny > otherZone->y || otherZone->y > World::maxy) continue;

		SOCKET sock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, 0);
		if (sock == INVALID_SOCKET) continue;

		u_long nonBlocking = 1;
		if (ioctlsocket(sock, FIONBIO, &nonBlocking) != 0) {
			closesocket(sock);
			peerRetryAfterMs[zi] = nowMs + 5000;
			continue;
		}
		BOOL noDelay = TRUE;
		setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
			reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));

		sockaddr_in addr = {};
		addr.sin_family = AF_INET;
		inet_pton(AF_INET, GetZoneIP(zi), &addr.sin_addr);
		addr.sin_port = htons(GetZonePort(zi));
		const int connectResult = connect(sock, (sockaddr*)&addr, sizeof(addr));
		if (connectResult == 0) {
			if (!CompleteOutboundPeerConnection(zi, sock)) {
				peerRetryAfterMs[zi] = nowMs + 5000;
			}
			continue;
		}

		const int connectError = WSAGetLastError();
		if (connectError == WSAEWOULDBLOCK || connectError == WSAEINPROGRESS ||
			connectError == WSAEALREADY || connectError == WSAEINVAL) {
			pendingPeerSocket[zi] = sock;
			pendingPeerConnect[zi] = true;
			pendingPeerStartMs[zi] = nowMs;
			continue;
		}

		closesocket(sock);
		peerRetryAfterMs[zi] = nowMs + 5000;
	}
}

bool World::CompleteOutboundPeerConnection(int zoneId, SOCKET socket) {
	for (int i = 0; i < clients.size; ++i) {
		if (clients.isnull(i)) continue;
		if (!clients[i].isServerPeer || clients[i].peerServerId != zoneId) continue;
		closesocket(socket);
		peerLinkUp[zoneId] = true;
		return true;
	}

	CTS_ServerLink_Header hello;
	hello.fromServerId = serverId;
	const int sent = send(socket, (const char*)&hello, sizeof(hello), 0);
	if (sent != sizeof(hello)) {
		closesocket(socket);
		return false;
	}

	const int idx = clients.Alloc();
	if (idx < 0) {
		closesocket(socket);
		return false;
	}
	clients[idx].socket = socket;
	clients[idx].isServerPeer = true;
	clients[idx].peerServerId = zoneId;
	clients[idx].pObjData = nullptr;
	clients[idx].objindex = -1;
	clients[idx].zoneId = ownedZoneId;
	clients[idx].rbufoffset = 0;
	clients[idx].pendingTransferToken = 0;
	clients[idx].PendingSDS.Init(8192);
	clients[idx].pendingSendOffset = 0;

	peerLinkUp[zoneId] = true;
	peerRetryAfterMs[zoneId] = 0;
	cout << "[Peer] outbound link established to zone " << zoneId
		<< " (" << GetZoneIP(zoneId) << ":" << GetZonePort(zoneId) << ")" << endl;
	return true;
}

bool World::QueuePeerPacket(int peerClientIndex, const void* data, int size) {
	if (peerClientIndex < 0 || peerClientIndex >= clients.size || clients.isnull(peerClientIndex)) return false;
	if (data == nullptr || size <= 0) return false;
	ClientData& peer = clients[peerClientIndex];
	if (!peer.isServerPeer || peer.socket == INVALID_SOCKET) return false;

	if (peer.PendingSDS.buffer == nullptr) peer.PendingSDS.Init((std::max)(8192, size));
	if (peer.pendingSendOffset > 0) {
		const int remaining = peer.PendingSDS.size - peer.pendingSendOffset;
		if (remaining > 0) {
			memmove(peer.PendingSDS.buffer, peer.PendingSDS.buffer + peer.pendingSendOffset, remaining);
		}
		peer.PendingSDS.size = (std::max)(remaining, 0);
		peer.pendingSendOffset = 0;
	}

	// A slow/dead peer must not grow the server queue without limit.
	constexpr int MaxPeerPendingBytes = 1024 * 1024;
	if (peer.PendingSDS.size > MaxPeerPendingBytes - size) return false;
	peer.PendingSDS.push_senddata((char*)data, size);
	return true;
}

void World::FlushPeerSends() {
	int failed[256] = {};
	int failedCount = 0;
	for (int i = 0; i < clients.size; ++i) {
		if (clients.isnull(i)) continue;
		ClientData& peer = clients[i];
		if (!peer.isServerPeer || peer.socket == INVALID_SOCKET) continue;
		if (peer.PendingSDS.size <= peer.pendingSendOffset) continue;

		const int sent = send(peer.socket,
			peer.PendingSDS.buffer + peer.pendingSendOffset,
			peer.PendingSDS.size - peer.pendingSendOffset, 0);
		if (sent > 0) {
			peer.pendingSendOffset += sent;
			if (peer.pendingSendOffset >= peer.PendingSDS.size) {
				peer.PendingSDS.Clear();
				peer.pendingSendOffset = 0;
			}
			continue;
		}
		if (sent == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) continue;
		if (failedCount < 256) failed[failedCount++] = i;
	}
	for (int i = 0; i < failedCount; ++i) ClientData::DisconnectToServer(failed[i]);
}

void World::PumpPeerConnections() {
	if (singleProcessAllZones) return;

	const ULONGLONG nowMs = GetTickCount64();
	for (int zi = 0; zi < 256; ++zi) {
		if (!pendingPeerConnect[zi]) continue;
		SOCKET sock = pendingPeerSocket[zi];

		if (NotMakePeer || sock == INVALID_SOCKET || nowMs - pendingPeerStartMs[zi] >= 3000) {
			if (sock != INVALID_SOCKET) closesocket(sock);
			pendingPeerSocket[zi] = INVALID_SOCKET;
			pendingPeerConnect[zi] = false;
			peerRetryAfterMs[zi] = nowMs + 5000;
			continue;
		}

		fd_set writeSet;
		fd_set errorSet;
		FD_ZERO(&writeSet);
		FD_ZERO(&errorSet);
		FD_SET(sock, &writeSet);
		FD_SET(sock, &errorSet);
		timeval timeout = {};
		const int ready = select(0, nullptr, &writeSet, &errorSet, &timeout);
		if (ready <= 0) continue;

		int socketError = 0;
		int socketErrorLength = sizeof(socketError);
		getsockopt(sock, SOL_SOCKET, SO_ERROR,
			reinterpret_cast<char*>(&socketError), &socketErrorLength);

		pendingPeerSocket[zi] = INVALID_SOCKET;
		pendingPeerConnect[zi] = false;
		if (socketError == 0 && FD_ISSET(sock, &writeSet)) {
			if (CompleteOutboundPeerConnection(zi, sock)) continue;
		}
		else {
			closesocket(sock);
		}
		peerRetryAfterMs[zi] = nowMs + 5000;
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
	if (fromServerId >= 0 && fromServerId < ZoneTable.size()) peerLinkUp[fromServerId] = true;

	cout << "[Peer] inbound link established from server " << fromServerId << endl;
}

// [4단계-STEP2] 내 존에 있는 플레이어들의 위치/시선을 peer 링크로 이웃 서버에 매 틱 전송.
void World::SendGhostToPeers() {
	if (NotMakePeer || singleProcessAllZones) return;

	// Full snapshots do not need to follow the simulation tick. 20 Hz is enough for
	// adjacent-zone presentation and prevents peer traffic from dominating input packets.
	static float ghostSyncFlow = 0.0f;
	ghostSyncFlow += (std::max)(0.0f, DeltaTime);
	if (ghostSyncFlow < 0.05f) return;
	ghostSyncFlow = 0.0f;

	bool hasPeer = false;
	for (int i = 0; i < clients.size; ++i) {
		if (!clients.isnull(i) && clients[i].isServerPeer) {
			hasPeer = true;
			break;
		}
	}
	if (!hasPeer) return;
	
	static char buf[8192];
	CTS_GhostSync_Header* h = (CTS_GhostSync_Header*)buf;
	h->st = CTS_Protocol::GhostSync;
	h->fromZoneId = ownedZoneId;

	GhostPlayerState* arr = (GhostPlayerState*)(buf + sizeof(CTS_GhostSync_Header));
	int count = 0;
	Zone* oz = gameworld.ZoneTable[ownedZoneId];
	for (int i = 0; i < oz->Dynamic_gameObjects.size; ++i) {
		if (oz->Dynamic_gameObjects.isnull(i)) continue;
		DynamicGameObject* o = oz->Dynamic_gameObjects[i];
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
	int failedPeerIndices[256] = {};
	int failedPeerCount = 0;
	for (int i = 0; i < clients.size; ++i) {
		if (clients.isnull(i)) continue;
		if (clients[i].isServerPeer == false) continue;
		// A full snapshot supersedes the previous one. When a peer still has queued bytes, skip this
		// frame instead of accumulating stale monster snapshots and delaying transfer packets.
		if (clients[i].PendingSDS.size > clients[i].pendingSendOffset) continue;
		if (!QueuePeerPacket(i, buf, (int)h->size) && failedPeerCount < 256) {
			failedPeerIndices[failedPeerCount++] = i;
		}
	}
	for (int i = 0; i < failedPeerCount; ++i) {
		ClientData::DisconnectToServer(failedPeerIndices[i]);
	}
}

// [4단계-STEP2] 이웃이 보낸 플레이어 목록으로 고스트를 생성/갱신/제거하고,
// 그 변화를 STC 패킷으로 만들어 '내가 소유한 존'의 CommonSDS 에 실어 내 클라들에게 중계한다.
// (내 클라들은 그걸 인접 존 객체로 받아 기존 렌더 경로 그대로 그린다 -> 클라 수정 불필요)
void World::OnGhostSync(char* data) {
	CTS_GhostSync_Header* h = (CTS_GhostSync_Header*)data;
	int fromZone = h->fromZoneId;
	if (fromZone < 0 || fromZone >= ZoneTable.size()) return;
	if (IsZoneOwned(ownedZoneId) == false) return;

	GhostPlayerState* arr = (GhostPlayerState*)(data + sizeof(CTS_GhostSync_Header));
	SendDataSaver& outSDS = gameworld.ZoneTable[ownedZoneId]->CommonSDS;

	int seen[256];
	int seenCount = 0;

	for (int k = 0; k < h->count; ++k) {
		int idx = arr[k].objindex;
		if (seenCount < 256) seen[seenCount++] = idx;

		short t = (short)arr[k].objType;
		const unsigned long long ghostKey = MakeGhostKey(fromZone, idx);
		auto it = ghostPlayers.find(ghostKey);
		bool isNew = (it == ghostPlayers.end());
		DynamicGameObject* ghost;
		if (!isNew) {
			const short existingType = (short)GameObjectType::VptrToTypeTable[*(void**)it->second];
			if (existingType != t) {
				gameworld.ZoneTable[fromZone]->Sending_DeleteGameObject(outSDS, idx);
				delete it->second;
				ghostPlayers.erase(it);
				isNew = true;
			}
		}
		if (isNew) {
			if (t == GameObjectType::_Monster) ghost = new Monster();
			else ghost = new Player();
			ghost->SetShape(arr[k].shapeindex);
			ghost->zoneId = fromZone;
			ghost->tag[GameObjectTag::Tag_Enable] = true;
			ghostPlayers[ghostKey] = ghost;
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
			gameworld.ZoneTable[fromZone]->Sending_ChangeGameObjectMember<matrix>(outSDS, idx, ghost, (GameObjectType)t, &ghost->worldMat);
			if (t == GameObjectType::_Monster) {
				Monster* mg = (Monster*)ghost;
				gameworld.ZoneTable[fromZone]->Sending_ChangeGameObjectMember<float>(outSDS, idx, mg, (GameObjectType)t, &mg->HP);
				gameworld.ZoneTable[fromZone]->Sending_ChangeGameObjectMember<bool>(outSDS, idx, mg, (GameObjectType)t, &mg->isDead);
			}
		}
	}

	// 이번 목록에 없는 고스트 = 이웃 존에서 사라짐 -> 삭제 패킷 보내고 정리.
	for (auto it = ghostPlayers.begin(); it != ghostPlayers.end(); ) {
		if (GhostKeyZone(it->first) != fromZone) {
			++it;
			continue;
		}
		const int sourceObjIndex = GhostKeyObject(it->first);
		bool found = false;
		for (int s = 0; s < seenCount; ++s) { if (seen[s] == sourceObjIndex) { found = true; break; } }
		if (found == false) {
			gameworld.ZoneTable[fromZone]->Sending_DeleteGameObject(outSDS, sourceObjIndex);
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

	//AverageSecPer60Start(2);

	CTS_GhostDamage_Header h;
	h.targetZoneId = targetZoneId;
	h.targetObjIndex = targetObjIndex;
	h.damage = damage;
	for (int i = 0; i < clients.size; ++i) {
		if (clients.isnull(i)) continue;
		if (clients[i].isServerPeer == false) continue;
		if (clients[i].peerServerId != targetZoneId) continue;  // 그 존(객체)을 소유한 서버에게만
		QueuePeerPacket(i, &h, sizeof(h));
	}

	//AverageSecPer60End(2);
}

bool World::SendMonsterHandoff(int dstZoneId, Monster* m) {
	if (singleProcessAllZones || m == nullptr) return false;

	//AverageSecPer60Start(3);
	CTS_MonsterHandoff_Header h;
	h.dstZoneId = dstZoneId;
	h.monsterType = (int)m->m_monsterType;
	h.pos = m->worldMat.pos;
	// Put the new owner just beyond the seam. Without this offset the cooldown expires while the
	// monster is still inside the same boundary and ownership ping-pongs between servers.
	if (m->zone != nullptr && dstZoneId >= 0 && dstZoneId < ZoneTable.size()) {
		Zone* dst = ZoneTable[dstZoneId];
		if (dst->x < m->zone->x) h.pos.x -= 4.5f;
		else if (dst->x > m->zone->x) h.pos.x += 4.5f;
		if (dst->y < m->zone->y) h.pos.z -= 4.5f;
		else if (dst->y > m->zone->y) h.pos.z += 4.5f;
		h.pos.w = 1.0f;
	}
	h.HP = m->HP;
	h.MaxHP = m->MaxHP;
	for (int i = 0; i < clients.size; ++i) {
		if (clients.isnull(i)) continue;
		if (clients[i].isServerPeer == false) continue;
		if (clients[i].peerServerId != dstZoneId) continue;  // 넘겨받을 서버에게만
		return QueuePeerPacket(i, &h, sizeof(h));
	}
	//AverageSecPer60End(3);
	return false;
}

void World::SpawnHandoffMonster(int monsterType, vec4 pos, float hp, float maxhp) {
	if (IsZoneOwned(ownedZoneId) == false) return;
	if (monsterType < 0 || monsterType >= (int)MonsterType::Max) return;  // [SAFE] 잘못된 타입이면 무시(크래시 방어)
	Zone* z = gameworld.ZoneTable[ownedZoneId];
	Monster* mon = new Monster();
	mon->zone = z;
	mon->ApplyMonsterData((MonsterType)monsterType);
	mon->Init(XMMatrixTranslation(pos.f3.x, pos.f3.y, pos.f3.z));
	mon->HP = hp;
	mon->MaxHP = maxhp;
	mon->handoffCooldown = 1.0f;   // 받자마자 다시 넘어가지 않게(thrashing 방지)
	z->NewObject(mon, GameObjectType::_Monster);
	z->PushGameObject(mon);
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
	Zone* StartZone = gameworld.ZoneTable[StartzoneId];
	vec4 avgpos = StartZone->map.AABB[0] + StartZone->map.AABB[1];
	avgpos *= 0.5f;
	avgpos.x = 0.5f * (StartZone->BasicAABB_onlyXZ.x + StartZone->BasicAABB_onlyXZ.z);
	avgpos.z = 0.5f * (StartZone->BasicAABB_onlyXZ.y + StartZone->BasicAABB_onlyXZ.w);
	avgpos.w = 1;
	p->worldMat.pos = avgpos;
	p->worldMat.pos.f3.y = StartZone->map.AABB[1].y - 4;
	// [party/dungeon] dungeon ceiling is too high -> player spawned at the ceiling. Spawn near the floor
	// instead so they drop into the room. (Tune the +5 if needed.)
	if (IsDungeonZone(StartzoneId)) {
		p->worldMat.pos.f3.y = StartZone->map.AABB[0].y + 1.5;
	}
	p->SetShape(Shape::StrToShapeIndex["Player"]);
	for (int i = 0; i < 36; ++i) {
		p->Inventory[i].id = 0;
		p->Inventory[i].ItemCount = 0;
	}

	clients[clientIndex].pObjData = p;
	clients[clientIndex].zoneId = StartzoneId;
	vec4 spawnPos = p->worldMat.pos;
	int objindex = StartZone->AddPlayer(clientIndex, p, spawnPos);
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
	if (data.dstZoneId < 0 || data.dstZoneId >= ZoneTable.size()) return false;

	// [party] dungeon entry: assign this party an instance and re-home into its floor-1 zone. The first
	// member of a party allocates the instance; the rest follow into the same one. If every instance is
	// busy, flag a bounce: create the player, then send them back to where they came from with a reject.
	bool dungeonBounceFull = false;
	bool dungeonZoneCorrected = false;   // [party] instance != entry -> must resend the client its real zone
	if (IsDungeonZone(data.dstZoneId) && IsDungeonZone(ownedZoneId)) {
		int inst = DungeonAllocInstanceForParty(data.partyId, 0);
		if (inst >= 0) {
			int base = DungeonInstanceBase(inst);
			// The client entered believing it is at data.dstZoneId (the entry, DungeonZoneId). If we placed
			// it in a different instance's floor, AddPlayer must send SyncPlayerMoveZone so the client's
			// currentZoneId (and thus its AllocPlayerIndexes net-index binding) matches the server.
			if (base != data.dstZoneId) dungeonZoneCorrected = true;
			data.dstZoneId = base;
			Zone* iz = (base >= 0 && base < (int)ZoneTable.size()) ? ZoneTable[base] : nullptr;
			if (iz != nullptr) data.spawnPos = DungeonZoneSpawn(iz);
		} else {
			dungeonBounceFull = true;
		}
	}

	clients[clientIndex].PersonalSDS.Init(4096);
	clients[clientIndex].pendingTransferToken = 0;

	// This player was visible on the destination server as a peer ghost before transfer.
	// Remove that stale ghost immediately so the real transferred Player does not coexist
	// with a clone until the next GhostSync cleanup arrives.
	if (data.srcZoneId >= 0 && data.srcObjIndex >= 0) {
		const unsigned long long ghostKey = MakeGhostKey(data.srcZoneId, data.srcObjIndex);
		auto ghostIt = ghostPlayers.find(ghostKey);
		if (ghostIt != ghostPlayers.end()) {
			Zone* srcZone = (data.srcZoneId >= 0 && data.srcZoneId < ZoneTable.size()) ? ZoneTable[data.srcZoneId] : nullptr;
			Zone* outZone = (IsZoneOwned(ownedZoneId) && ownedZoneId >= 0 && ownedZoneId < ZoneTable.size()) ? ZoneTable[ownedZoneId] : nullptr;
			if (srcZone != nullptr && outZone != nullptr) {
				srcZone->Sending_DeleteGameObject(outZone->CommonSDS, data.srcObjIndex);
			}
			delete ghostIt->second;
			ghostPlayers.erase(ghostIt);
		}
	}

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
	
	p->weapon[0] = data.weapon[0];
	p->weapon[1] = data.weapon[1];
	p->weapon[2] = data.weapon[2];
	p->SelectedWeapon = data.SelectedWeapone;

	p->m_yaw = data.yaw;
	p->m_pitch = data.pitch;
	for (int i = 0; i < 36; ++i) {
		p->Inventory[i] = data.Inventory[i];
	}

	clients[clientIndex].pObjData = p;
	clients[clientIndex].zoneId = data.dstZoneId;
	// update_Map = dungeonZoneCorrected: only when we placed them in an instance other than the entry zone
	// must the client be told its real zone (SyncPlayerMoveZone) so its net-index binding matches the server.
	int objindex = gameworld.ZoneTable[data.dstZoneId]->AddPlayer(clientIndex, p, data.spawnPos, dungeonZoneCorrected, false);
	clients[clientIndex].objindex = objindex;

	// [party] every dungeon instance was busy: tell the player ("던전이 가득 찼습니다") and bounce them back
	// to the open-world zone they came from. The player object now exists, so reuse the normal zone-move path.
	if (dungeonBounceFull) {
		cout << "[Dungeon] all instances busy -> reject party id=" << data.partyId
			<< " client=" << clientIndex << " (bounce to zone " << data.originZoneId << ")" << endl;
		Sending_DungeonReject(clients[clientIndex].PersonalSDS, 0);
		int back = (data.originZoneId >= 0 && data.originZoneId < (int)ZoneTable.size()) ? data.originZoneId : data.dstZoneId;
		MovePlayerToZone(clientIndex, back, data.spawnPos);
	}
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
	if (zid < 0 || zid >= ZoneTable.size()) return nullptr;

	return gameworld.ZoneTable[zid];
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

int World::GetStartTalk(Player* p, PeacefulNPC* npc)
{
	if (npc->NPCID == 0) {
		if (p->CompleteQuestBitArr[0] == false) return 0;
	}
	return -1;
}

void ClientData::DisconnectToServer(int index) {
	if (index < 0 || index >= gameworld.clients.size) return;
	if (gameworld.clients.isnull(index)) return;

	ClientData& client = gameworld.clients[index];

	// [4단계-STEP1] 이 슬롯이 이웃 서버와의 peer 링크면, 플레이어 정리 로직을 타지 않고
	// 링크 상태만 리셋한 뒤 종료한다. (다음 TryConnectPeers 가 재연결을 시도)
	if (client.isServerPeer) {
		int lostServerId = client.peerServerId;
		if (lostServerId >= 0 && lostServerId < gameworld.ZoneTable.size()) gameworld.peerLinkUp[lostServerId] = false;
		if (lostServerId >= 0 && lostServerId < gameworld.ZoneTable.size() &&
			gameworld.IsZoneOwned(gameworld.ownedZoneId)) {
			SendDataSaver& outSDS = gameworld.ZoneTable[gameworld.ownedZoneId]->CommonSDS;
			for (auto it = gameworld.ghostPlayers.begin(); it != gameworld.ghostPlayers.end(); ) {
				if (World::GhostKeyZone(it->first) != lostServerId) {
					++it;
					continue;
				}
				gameworld.ZoneTable[lostServerId]->Sending_DeleteGameObject(
					outSDS, World::GhostKeyObject(it->first));
				delete it->second;
				it = gameworld.ghostPlayers.erase(it);
			}
		}
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
	gameworld.PartyLeave(index);   // [party] drop from any lobby party on disconnect (disband/promote as needed)

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
	client.PendingSDS.Clear();
	client.pendingSendOffset = 0;
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

void World::MovePlayerToZone(int clientIndex, int dstZoneId, vec4 spawnPos, int partyId, int originZoneId) {
	if (clientIndex < 0 || clientIndex >= clients.size) return;
	if (clients.isnull(clientIndex)) return;
	if (dstZoneId < 0 || dstZoneId >= ZoneTable.size()) return;

	int srcZoneId = clients[clientIndex].zoneId;
	if (srcZoneId < 0 || srcZoneId >= ZoneTable.size()) return;
	if (srcZoneId == dstZoneId) return;

	Player* player = clients[clientIndex].pObjData;
	if (player == nullptr) return;

	cout << "[ZoneMove] client=" << clientIndex
		<< " from=" << srcZoneId
		<< " to=" << dstZoneId << endl;

	if (IsZoneOwned(dstZoneId) == false) {
		PlayerTransferData data = {};
		data.transferToken = IssueTransferToken();
		data.srcZoneId = srcZoneId;
		data.srcObjIndex = clients[clientIndex].objindex;
		data.dstZoneId = dstZoneId;
		data.partyId = partyId;                                            // [party] dungeon grouping tag
		data.originZoneId = (originZoneId >= 0) ? originZoneId : srcZoneId; // [party] where to bounce back on full
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
		data.m_weaponHolstered = player->m_weaponHolstered;
		for (int i = 0; i < 36; ++i) {
			data.Inventory[i] = player->Inventory[i];
		}
		data.weapon[0] = player->weapon[0];
		data.weapon[1] = player->weapon[1];
		data.weapon[2] = player->weapon[2];
		data.SelectedWeapone = player->SelectedWeapon;

		if (SendPlayerTransferToServer(data) == false) {
			cout << "[ZoneMove] remote transfer send failed. dstZone=" << dstZoneId << endl;
			return;
		}

		Sending_ServerTransfer(clients[clientIndex].PersonalSDS, dstZoneId, GetZoneIP(dstZoneId), GetZonePort(dstZoneId), data.transferToken);
		gameworld.ZoneTable[srcZoneId]->RemovePlayer(clientIndex);
		delete player;
		clients[clientIndex].pObjData = nullptr;
		clients[clientIndex].objindex = -1;
		return;
	}

	gameworld.ZoneTable[srcZoneId]->RemovePlayer(clientIndex);
	// [PERF/FIX ②] 심리스 로컬 이동: fullWorldSnapshot=false 로 동적객체 버스트 생략.
	// 클라는 인접 존 객체를 이미 보유 중이며, 가시성 기반 cleanup 이 그걸 유지한다.
	gameworld.ZoneTable[dstZoneId]->AddPlayer(clientIndex, player, spawnPos, true, false);
}

NPCTalkData::NPCTalkData()
{
	text = nullptr;
	selectCnt = 0;
	NextEscape = true;
	sel[0] = TalkSelection();
	sel[1] = TalkSelection();
	sel[2] = TalkSelection();
	sel[3] = TalkSelection();
}

NPCTalkData::NPCTalkData(const wchar_t* txt, bool nxtEsc, TalkSelection sel1, TalkSelection sel2, TalkSelection sel3, TalkSelection sel4)
{
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

void Player::EraseQuest(int index)
{
	QuestArr.erase(QuestArr.begin() + index);
	Quest* q = QuestPrograss[index];
	delete q;
	QuestPrograss.erase(QuestPrograss.begin() + index);
}

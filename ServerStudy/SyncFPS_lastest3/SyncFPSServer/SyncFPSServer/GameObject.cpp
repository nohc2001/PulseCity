#include "stdafx.h"
#include "GameObject.h"

vector<string> Shape::ShapeNameArr;
unordered_map<string, Shape> Shape::StrToShapeMap;
unordered_map<int, Shape> Shape::IndexToShapeMap;

void Mesh::ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering)
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
		CenterPos.x = (maxPos.x + minPos.x) * 0.5f;
		CenterPos.y = (maxPos.y + minPos.y) * 0.5f;
		CenterPos.z = (maxPos.z + minPos.z) * 0.5f;
	}
	else {
		CenterPos.x = 0;
		CenterPos.y = 0;
		CenterPos.z = 0;
	}
	MAXpos.x = (maxPos.x - minPos.x) * 0.5f;
	MAXpos.y = (maxPos.y - minPos.y) * 0.5f;
	MAXpos.z = (maxPos.z - minPos.z) * 0.5f;
	MAXpos *= 0.01f;
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

int Shape::GetShapeIndex(string meshName)
{
	for (int i = 0; i < Shape::ShapeNameArr.size(); ++i) {
		if (Shape::ShapeNameArr[i] == meshName) {
			return i;
		}
	}
	return -1;
}

int Shape::AddShapeName(string meshName)
{
	int r = ShapeNameArr.size();
	ShapeNameArr.push_back(meshName);
	return r;
}

int Shape::AddMesh(string name, Mesh* ptr)
{
	int index = AddShapeName(name);
	Shape s;
	s.SetMesh(ptr);
	StrToShapeMap.insert(pair<string, Shape>(name, s));
	IndexToShapeMap.insert(pair<int, Shape>(index, s));
	return index;
}

int Shape::AddModel(string name, Model* ptr)
{
	int index = AddShapeName(name);
	Shape s;
	s.SetModel(ptr);
	StrToShapeMap.insert(pair<string, Shape>(name, s));
	IndexToShapeMap.insert(pair<int, Shape>(index, s));
	return index;
}

GameObject::GameObject()
{
	isExist = false;
	m_worldMatrix.Id();
	ShapeIndex = -1;
	LVelocity = 0;
	tickLVelocity = 0;
}

GameObject::~GameObject()
{
}

void GameObject::Update(float deltaTime)
{
}

void GameObject::Event()
{
}

BoundingOrientedBox GameObject::GetOBB()
{
	BoundingOrientedBox obb_local;
	Shape s = Shape::IndexToShapeMap[ShapeIndex];
	if (s.isMesh()) {
		obb_local = s.GetMesh()->GetOBB();
	}
	else obb_local = s.GetModel()->GetOBB();
	BoundingOrientedBox obb_world;
	obb_local.Transform(obb_world, m_worldMatrix);
	return obb_world;
}

void GameObject::CollisionMove(GameObject* gbj1, GameObject* gbj2)
{
	constexpr float epsillon = 0.01f;

	bool bi = XMColorEqual(gbj1->tickLVelocity, XMVectorZero());
	bool bj = XMColorEqual(gbj2->tickLVelocity, XMVectorZero());

	GameObject* movObj = nullptr;
	GameObject* colObj = nullptr;
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
		gbj1->m_worldMatrix.pos += gbj1->tickLVelocity;
		obb1 = gbj1->GetOBB();
		gbj1->m_worldMatrix.pos -= gbj1->tickLVelocity;
		gbj2->m_worldMatrix.pos += gbj2->tickLVelocity;
		obb2 = gbj2->GetOBB();
		gbj2->m_worldMatrix.pos -= gbj2->tickLVelocity;

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
			gbj1->m_worldMatrix.pos += v1;
			obb1 = gbj1->GetOBB();
			gbj1->m_worldMatrix.pos -= v1;
			gbj2->m_worldMatrix.pos += v2;
			obb2 = gbj2->GetOBB();
			gbj2->m_worldMatrix.pos -= v2;
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

			gbj2->m_worldMatrix.pos += preMove2;
			obb2 = gbj2->GetOBB();
			gbj2->m_worldMatrix.pos -= preMove2;

			CollisionMove_DivideBaseline_rest(gbj1, gbj2, obb2, preMove1);
			gbj1->tickLVelocity += preMove1;

			gbj1->m_worldMatrix.pos += gbj1->tickLVelocity;
			obb1 = gbj1->GetOBB();
			gbj1->m_worldMatrix.pos -= gbj1->tickLVelocity;

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
	movObj->m_worldMatrix.pos += movObj->tickLVelocity;
	obb1 = movObj->GetOBB();
	movObj->m_worldMatrix.pos -= movObj->tickLVelocity;
	obb2 = colObj->GetOBB();

	if (obb1.Intersects(obb2)) {
		movObj->OnCollision(colObj);
		colObj->OnCollision(movObj);

		CollisionMove_DivideBaseline(movObj, colObj, obb2);
	}
}

void GameObject::CollisionMove_DivideBaseline(GameObject* movObj, GameObject* colObj, BoundingOrientedBox colOBB)
{
	XMMATRIX basemat = colObj->m_worldMatrix;
	XMMATRIX invmat = colObj->m_worldMatrix.RTInverse;
	invmat.r[3] = XMVectorSet(0, 0, 0, 1);
	XMVECTOR BaseLine = XMVector3Transform(movObj->tickLVelocity, invmat);
	movObj->tickLVelocity = XMVectorZero();

	XMVECTOR MoveVector = basemat.r[0] * BaseLine.m128_f32[0];
	movObj->tickLVelocity += MoveVector;
	movObj->m_worldMatrix.pos += movObj->tickLVelocity;
	BoundingOrientedBox obb1 = movObj->GetOBB();
	movObj->m_worldMatrix.pos -= movObj->tickLVelocity;
	if (obb1.Intersects(colOBB)) {
		movObj->tickLVelocity -= MoveVector;
	}

	MoveVector = basemat.r[1] * BaseLine.m128_f32[1];
	movObj->tickLVelocity += MoveVector;
	movObj->m_worldMatrix.pos += movObj->tickLVelocity;
	obb1 = movObj->GetOBB();
	movObj->m_worldMatrix.pos -= movObj->tickLVelocity;
	if (obb1.Intersects(colOBB)) {
		movObj->tickLVelocity -= MoveVector;
	}

	MoveVector = basemat.r[2] * BaseLine.m128_f32[2];
	movObj->tickLVelocity += MoveVector;
	movObj->m_worldMatrix.pos += movObj->tickLVelocity;
	obb1 = movObj->GetOBB();
	movObj->m_worldMatrix.pos -= movObj->tickLVelocity;
	if (obb1.Intersects(colOBB)) {
		movObj->tickLVelocity -= MoveVector;
	}
}

void GameObject::CollisionMove_DivideBaseline_StaticOBB(GameObject* movObj, BoundingOrientedBox colOBB)
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
	movObj->m_worldMatrix.pos += movObj->tickLVelocity;
	BoundingOrientedBox obb1 = movObj->GetOBB();
	movObj->m_worldMatrix.pos -= movObj->tickLVelocity;
	if (obb1.Intersects(colOBB)) {
		movObj->tickLVelocity -= MoveVector;
	}

	MoveVector = basemat.mat.r[1] * BaseLine.m128_f32[1];
	movObj->tickLVelocity += MoveVector;
	movObj->m_worldMatrix.pos += movObj->tickLVelocity;
	obb1 = movObj->GetOBB();
	movObj->m_worldMatrix.pos -= movObj->tickLVelocity;
	if (obb1.Intersects(colOBB)) {
		movObj->tickLVelocity -= MoveVector;
	}

	MoveVector = basemat.mat.r[2] * BaseLine.m128_f32[2];
	movObj->tickLVelocity += MoveVector;
	movObj->m_worldMatrix.pos += movObj->tickLVelocity;
	obb1 = movObj->GetOBB();
	movObj->m_worldMatrix.pos -= movObj->tickLVelocity;
	if (obb1.Intersects(colOBB)) {
		movObj->tickLVelocity -= MoveVector;
	}
}

void GameObject::CollisionMove_DivideBaseline_rest(GameObject* movObj, GameObject* colObj, BoundingOrientedBox colOBB, vec4 preMove)
{
	movObj->m_worldMatrix.pos += preMove;

	XMMATRIX basemat = colObj->m_worldMatrix;
	XMMATRIX invmat = colObj->m_worldMatrix.RTInverse;
	invmat.r[3] = XMVectorSet(0, 0, 0, 1);
	XMVECTOR BaseLine = XMVector3Transform(movObj->tickLVelocity, invmat);
	movObj->tickLVelocity = XMVectorZero();

	XMVECTOR MoveVector = basemat.r[0] * BaseLine.m128_f32[0];
	movObj->tickLVelocity += MoveVector;
	movObj->m_worldMatrix.pos += movObj->tickLVelocity;
	BoundingOrientedBox obb1 = movObj->GetOBB();
	movObj->m_worldMatrix.pos -= movObj->tickLVelocity;
	if (obb1.Intersects(colOBB)) {
		movObj->tickLVelocity -= MoveVector;
	}

	MoveVector = basemat.r[1] * BaseLine.m128_f32[1];
	movObj->tickLVelocity += MoveVector;
	movObj->m_worldMatrix.pos += movObj->tickLVelocity;
	obb1 = movObj->GetOBB();
	movObj->m_worldMatrix.pos -= movObj->tickLVelocity;
	if (obb1.Intersects(colOBB)) {
		movObj->tickLVelocity -= MoveVector;
	}

	MoveVector = basemat.r[2] * BaseLine.m128_f32[2];
	movObj->tickLVelocity += MoveVector;
	movObj->m_worldMatrix.pos += movObj->tickLVelocity;
	obb1 = movObj->GetOBB();
	movObj->m_worldMatrix.pos -= movObj->tickLVelocity;
	if (obb1.Intersects(colOBB)) {
		movObj->tickLVelocity -= MoveVector;
	}

	movObj->m_worldMatrix.pos -= preMove;
}

void GameObject::OnCollision(GameObject* other)
{
}

void GameObject::OnStaticCollision(BoundingOrientedBox obb)
{
}

void GameObject::OnCollisionRayWithBullet(GameObject* shooter)
{
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

void GameMap::StaticCollisionMove(GameObject* obj)
{
	obj->m_worldMatrix.pos += obj->tickLVelocity;
	BoundingOrientedBox obb = obj->GetOBB();
	obj->m_worldMatrix.pos -= obj->tickLVelocity;

	vec4 AABB[2];
	vector<StaticCollisions*> SCArr;
	SCArr.reserve(8);

	Mesh::GetAABBFromOBB(AABB, obb, true);
	int xMin = floor(AABB[0].x / GameMap::chunck_divide_Width);
	int xMax = floor(AABB[1].x / GameMap::chunck_divide_Width);
	int yMin = floor(AABB[0].y / GameMap::chunck_divide_Width);
	int yMax = floor(AABB[1].y / GameMap::chunck_divide_Width);
	int zMin = floor(AABB[0].z / GameMap::chunck_divide_Width);
	int zMax = floor(AABB[1].z / GameMap::chunck_divide_Width);
	auto f = static_collision_chunck.end();
	if (xMin != xMax) {
		if (yMin != yMax) {
			if (zMin != zMax) {
				f = static_collision_chunck.find(ChunkIndex(xMin, yMin, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMin, yMin, zMax));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMin, yMax, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMin, yMax, zMax));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMax, yMin, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMax, yMin, zMax));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMax, yMax, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMax, yMax, zMax));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
			}
			else {
				f = static_collision_chunck.find(ChunkIndex(xMin, yMin, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMin, yMax, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMax, yMin, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMax, yMax, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
			}
		}
		else {
			if (zMin != zMax) {
				f = static_collision_chunck.find(ChunkIndex(xMin, yMin, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMin, yMin, zMax));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMax, yMin, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMax, yMin, zMax));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
			}
			else {
				f = static_collision_chunck.find(ChunkIndex(xMin, yMin, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMax, yMin, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
			}
		}
	}
	else {
		if (yMin != yMax) {
			if (zMin != zMax) {
				f = static_collision_chunck.find(ChunkIndex(xMin, yMin, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMin, yMin, zMax));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMin, yMax, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMin, yMax, zMax));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
			}
			else {
				f = static_collision_chunck.find(ChunkIndex(xMin, yMin, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMin, yMax, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
			}
		}
		else {
			if (zMin != zMax) {
				f = static_collision_chunck.find(ChunkIndex(xMin, yMin, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMin, yMin, zMax));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
			}
			else {
				f = static_collision_chunck.find(ChunkIndex(xMin, yMin, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
			}
		}
	}

	for (int i = 0; i < SCArr.size(); ++i) {
		for (int k = 0; k < SCArr[i]->obbs.size(); ++k) {
			BoundingOrientedBox staticobb = SCArr[i]->obbs[k];

			obj->m_worldMatrix.pos += obj->tickLVelocity;
			BoundingOrientedBox obb1 = obj->GetOBB();
			obj->m_worldMatrix.pos -= obj->tickLVelocity;

			if (obb1.Intersects(staticobb)) {
				obj->OnStaticCollision(staticobb);
				GameObject::CollisionMove_DivideBaseline_StaticOBB(obj, staticobb);
			}

			if (obj->tickLVelocity.fast_square_of_len3 <= 0.001f) return;
		}
	}
}

bool GameMap::isStaticCollision(BoundingOrientedBox obb)
{
	vec4 AABB[2];
	vector<StaticCollisions*> SCArr;
	SCArr.reserve(8);

	Mesh::GetAABBFromOBB(AABB, obb, true);
	int xMin = floor(AABB[0].x / GameMap::chunck_divide_Width);
	int xMax = floor(AABB[1].x / GameMap::chunck_divide_Width);
	int yMin = floor(AABB[0].y / GameMap::chunck_divide_Width);
	int yMax = floor(AABB[1].y / GameMap::chunck_divide_Width);
	int zMin = floor(AABB[0].z / GameMap::chunck_divide_Width);
	int zMax = floor(AABB[1].z / GameMap::chunck_divide_Width);
	auto f = static_collision_chunck.end();
	if (xMin != xMax) {
		if (yMin != yMax) {
			if (zMin != zMax) {
				f = static_collision_chunck.find(ChunkIndex(xMin, yMin, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMin, yMin, zMax));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMin, yMax, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMin, yMax, zMax));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMax, yMin, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMax, yMin, zMax));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMax, yMax, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMax, yMax, zMax));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
			}
			else {
				f = static_collision_chunck.find(ChunkIndex(xMin, yMin, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMin, yMax, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMax, yMin, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMax, yMax, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
			}
		}
		else {
			if (zMin != zMax) {
				f = static_collision_chunck.find(ChunkIndex(xMin, yMin, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMin, yMin, zMax));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMax, yMin, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMax, yMin, zMax));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
			}
			else {
				f = static_collision_chunck.find(ChunkIndex(xMin, yMin, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMax, yMin, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
			}
		}
	}
	else {
		if (yMin != yMax) {
			if (zMin != zMax) {
				f = static_collision_chunck.find(ChunkIndex(xMin, yMin, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMin, yMin, zMax));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMin, yMax, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMin, yMax, zMax));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
			}
			else {
				f = static_collision_chunck.find(ChunkIndex(xMin, yMin, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMin, yMax, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
			}
		}
		else {
			if (zMin != zMax) {
				f = static_collision_chunck.find(ChunkIndex(xMin, yMin, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
				f = static_collision_chunck.find(ChunkIndex(xMin, yMin, zMax));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
			}
			else {
				f = static_collision_chunck.find(ChunkIndex(xMin, yMin, zMin));
				if (f != static_collision_chunck.end()) SCArr.push_back(f->second);
			}
		}
	}

	for (int i = 0; i < SCArr.size(); ++i) {
		for (int k = 0; k < SCArr[i]->obbs.size(); ++k) {
			BoundingOrientedBox staticobb = SCArr[i]->obbs[k];

			if (obb.Intersects(staticobb)) {
				return true;
			}
		}
	}
	return false;
}

void GameMap::PushOBB_ToStaticCollisions(BoundingOrientedBox obb) {
	vec4 AABB[2];
	Mesh::GetAABBFromOBB(AABB, obb, true);
	int xMin = floor(AABB[0].x / GameMap::chunck_divide_Width);
	int xMax = floor(AABB[1].x / GameMap::chunck_divide_Width);
	int yMin = floor(AABB[0].y / GameMap::chunck_divide_Width);
	int yMax = floor(AABB[1].y / GameMap::chunck_divide_Width);
	int zMin = floor(AABB[0].z / GameMap::chunck_divide_Width);
	int zMax = floor(AABB[1].z / GameMap::chunck_divide_Width);
	if (xMin != xMax) {
		if (yMin != yMax) {
			if (zMin != zMax) {
				for (int xi = xMin; xi <= xMax; ++xi)
					for (int yi = yMin; yi <= yMax; ++yi)
						for (int zi = zMin; zi <= zMax; ++zi)
							PushOBB_ToStaticCollisions_WithChunkIndex(ChunkIndex(xi, yi, zi), obb);
			}
			else {
				for (int xi = xMin; xi <= xMax; ++xi)
					for (int yi = yMin; yi <= yMax; ++yi)
						PushOBB_ToStaticCollisions_WithChunkIndex(ChunkIndex(xi, yi, zMin), obb);
			}
		}
		else {
			if (zMin != zMax) {
				for (int xi = xMin; xi <= xMax; ++xi)
					for (int zi = zMin; zi <= zMax; ++zi)
						PushOBB_ToStaticCollisions_WithChunkIndex(ChunkIndex(xi, yMin, zi), obb);
			}
			else {
				for (int xi = xMin; xi <= xMax; ++xi) 
					PushOBB_ToStaticCollisions_WithChunkIndex(ChunkIndex(xi, yMin, zMin), obb);
			}
		}
	}
	else {
		if (yMin != yMax) {
			if (zMin != zMax) {
				for (int yi = yMin; yi <= yMax; ++yi)
					for (int zi = zMin; zi <= zMax; ++zi)
						PushOBB_ToStaticCollisions_WithChunkIndex(ChunkIndex(xMin, yi, zi), obb);
			}
			else {
				for (int yi = yMin; yi <= yMax; ++yi)
					PushOBB_ToStaticCollisions_WithChunkIndex(ChunkIndex(xMin, yi, zMin), obb);
			}
		}
		else {
			if (zMin != zMax) {
				for (int zi = zMin; zi <= zMax; ++zi)
					PushOBB_ToStaticCollisions_WithChunkIndex(ChunkIndex(xMin, yMin, zi), obb);
			}
			else {
				PushOBB_ToStaticCollisions_WithChunkIndex(ChunkIndex(xMin, yMin, zMin), obb);
			}
		}
	}
}

void GameMap::PushOBB_ToStaticCollisions_WithChunkIndex(ChunkIndex ci, BoundingOrientedBox obb)
{
	auto f = static_collision_chunck.find(ci);
	if (f != static_collision_chunck.end()) {
		static_collision_chunck[ci]->obbs.push_back(obb);
	}
	else {
		StaticCollisions* pSC = new StaticCollisions();
		pSC->obbs.push_back(obb);
		static_collision_chunck.insert(pair<ChunkIndex, StaticCollisions*>(ci, pSC));
	}
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

	for (int i = 0; i < gameObjectCount; ++i) {
		Hierarchy_Object* go = new Hierarchy_Object();
		map->MapObjects[i] = go;
	}
	for (int i = 0; i < gameObjectCount; ++i) {
		Hierarchy_Object* go = map->MapObjects[i];
		int nameId = 0;
		ifs.read((char*)&nameId, sizeof(int));

		vec4 pos;
		ifs.read((char*)&pos, sizeof(float) * 3);
		vec4 rot = 0;
		ifs.read((char*)&rot, sizeof(float) * 3);
		vec4 scale = 0;
		ifs.read((char*)&scale, sizeof(float) * 3);

		rot *= 3.141592f / 180.0f;
		go->m_worldMatrix *= XMMatrixScaling(scale.x, scale.y, scale.z);
		go->m_worldMatrix *= XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z);
		go->m_worldMatrix.pos.f3 = pos.f3;
		go->m_worldMatrix.pos.w = 1;
		
		bool isMeshes = false;
		ifs.read((char*)&isMeshes, sizeof(bool));
		if (isMeshes) {
			int meshid = 0;
			int materialNum = 0;
			int materialId = 0;
			ifs.read((char*)&meshid, sizeof(int));
			if (meshid == 75) {
				cout << endl;
			}
			if (meshid < 0) {
				//go->mesh = nullptr;
				ifs.read((char*)&materialNum, sizeof(int));
			}
			else {
				go->ShapeIndex = map->mesh_shapeindexes[meshid];
				ifs.read((char*)&materialNum, sizeof(int));
				for (int k = 0; k < materialNum; ++k) {
					ifs.read((char*)&materialId, sizeof(int));
					go->Mesh_materialIndex = MaterialTableStart + materialId;
				}
			}
		}
		else {
			//Model
			int ModelID;
			ifs.read((char*)&ModelID, sizeof(int));
			if (ModelID < 0) {
				go->ShapeIndex = -1;
			}
			else {
				go->ShapeIndex = map->model_shapeindexes[ModelID];
			}
		}

		if (isMeshes) {
			ifs.read((char*)&go->childCount, sizeof(int));
			go->childs.reserve(go->childCount);
			go->childs.resize(go->childCount);
			for (int k = 0; k < go->childCount; ++k) {
				int childIndex = 0;
				ifs.read((char*)&childIndex, sizeof(int));
				go->childs[k] = map->MapObjects[childIndex];
			}
		}
		else {
			go->childCount = 0;
		}
		map->MapObjects[i] = go;
	}

	BakeStaticCollision();
}

void Model::LoadModelFile2(string filename)
{
	ifstream ifs{ filename, ios_base::binary };
	ifs.read((char*)&mNumMeshes, sizeof(unsigned int));
	mMeshes = new Mesh * [mNumMeshes];

	ifs.read((char*)&nodeCount, sizeof(unsigned int));
	Nodes = new ModelNode[nodeCount];

	//new0
	int mNumTextures;
	int mNumMaterials;
	ifs.read((char*)&mNumTextures, sizeof(unsigned int));
	ifs.read((char*)&mNumMaterials, sizeof(unsigned int));
	vec4 tempRead;
	for (int i = 0; i < mNumMeshes; ++i) {
		Mesh* mesh = new Mesh();
		XMFLOAT3 AABB[2];
		ifs.read((char*)AABB, sizeof(XMFLOAT3) * 2);
		mesh->SetOBBDataWithAABB(AABB[0], AABB[1]);

		//new1
		//ifs.read((char*)&mesh->material_index, sizeof(int));

		XMFLOAT3 MAABB[2];
		unsigned int vertSiz = 0;
		unsigned int indexSiz = 0;
		ifs.read((char*)&vertSiz, sizeof(unsigned int));
		ifs.read((char*)&indexSiz, sizeof(unsigned int));

		unsigned int triSiz = indexSiz / 3;

		
		for (int k = 0; k < vertSiz; ++k) {
			ifs.read((char*)&tempRead, sizeof(XMFLOAT3));
			if (k == 0) {
				MAABB[0] = tempRead.f3;
				MAABB[1] = tempRead.f3;
			}

			if (MAABB[0].x > tempRead.x) MAABB[0].x = tempRead.x;
			if (MAABB[0].y > tempRead.y) MAABB[0].y = tempRead.y;
			if (MAABB[0].z > tempRead.z) MAABB[0].z = tempRead.z;

			if (MAABB[1].x < tempRead.x) MAABB[1].x = tempRead.x;
			if (MAABB[1].y < tempRead.y) MAABB[1].y = tempRead.y;
			if (MAABB[1].z < tempRead.z) MAABB[1].z = tempRead.z;

			ifs.read((char*)&tempRead, sizeof(XMFLOAT2));
			ifs.read((char*)&tempRead, sizeof(XMFLOAT3));
			ifs.read((char*)&tempRead, sizeof(XMFLOAT3));

			// non use
			XMFLOAT3 bitangent;
			ifs.read((char*)&bitangent, sizeof(XMFLOAT3));
		}
		for (int k = 0; k < triSiz; ++k) {
			ifs.read((char*)&tempRead, sizeof(int) * 3);
		}

		mesh->SetOBBDataWithAABB(MAABB[0], MAABB[1]);
		mMeshes[i] = mesh;
	}

	for (int i = 0; i < nodeCount; ++i) {
		int namelen = 0;
		ifs.read((char*)&namelen, sizeof(int));
		char name[256] = {};
		ifs.read((char*)name, namelen);
		name[namelen] = 0;
		//Nodes[i].name = name;

		vec4 pos;
		ifs.read((char*)&pos, sizeof(float) * 3);
		vec4 rot = 0;
		ifs.read((char*)&rot, sizeof(float) * 3);
		vec4 scale = 0;
		ifs.read((char*)&scale, sizeof(float) * 3);

		if (i == 0) {
			matrix mat;
			mat.Id();
			Nodes[i].transform = mat;
		}
		else {
			matrix mat;
			mat.Id();
			rot *= 3.141592f / 180.0f;
			mat *= XMMatrixScaling(scale.x, scale.y, scale.z);
			mat *= XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z);
			mat.pos.f3 = pos.f3;
			mat.pos.w = 1;

			Nodes[i].transform = mat;
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

		for (int k = 0; k < Nodes[i].numMesh; ++k) {
			ifs.read((char*)&Nodes[i].Meshes[k], sizeof(int));
			int num_materials = 0;
			ifs.read((char*)&num_materials, sizeof(int));
			for (int u = 0; u < num_materials; ++u) {
				int material_id = 0;
				ifs.read((char*)&material_id, sizeof(int));
			}
		}
	}

	for (int i = 0; i < mNumMaterials; ++i) {
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

bool Hierarchy_Object::Collision_Inherit(matrix parent_world, BoundingBox bb)
{
	XMMATRIX sav = XMMatrixMultiply(m_worldMatrix, parent_world);
	BoundingOrientedBox obb = GetOBBw(sav);
	if (obb.Extents.x > 0 && obb.Intersects(bb)) {
		return true;
	}

	for (int i = 0; i < childCount; ++i) {
		if (childs[i]->Collision_Inherit(sav, bb)) return true;
	}

	return false;
}

void Hierarchy_Object::InitMapAABB_Inherit(void* origin, matrix parent_world)
{
	GameMap* map = (GameMap*)origin;

	XMMATRIX sav = XMMatrixMultiply(m_worldMatrix, parent_world);
	BoundingOrientedBox obb = GetOBBw(sav);

	if (obb.Extents.x > 0) {
		map->PushOBB_ToStaticCollisions(obb);
	}
	
	map->ExtendMapAABB(obb);

	for (int i = 0; i < childCount; ++i) {
		childs[i]->InitMapAABB_Inherit(origin, sav);
	}
}

BoundingOrientedBox Hierarchy_Object::GetOBBw(matrix worldMat)
{
	BoundingOrientedBox obb_local;
	Shape s = Shape::IndexToShapeMap[ShapeIndex];
	if (ShapeIndex == -1) {
		obb_local.Extents.x = -1;
		return obb_local;
	}
	if (s.isMesh()) {
		obb_local = s.GetMesh()->GetOBB();
	}
	else{
		obb_local = s.GetModel()->GetOBB();
	}
	BoundingOrientedBox obb_world;
	obb_local.Transform(obb_world, worldMat);
	return obb_world;
}

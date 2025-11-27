#include "stdafx.h"
#include "GameObject.h"

vector<string> Mesh::MeshNameArr;
unordered_map<string, Mesh*> Mesh::meshmap;

void Mesh::ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering)
{
	XMFLOAT3 maxPos = { 0, 0, 0 };
	XMFLOAT3 minPos = { 0, 0, 0 };
	std::ifstream in{ path };

	while (in.eof() == false && (in.is_open() && in.fail() == false)) {
		char rstr[128] = {};
		in >> rstr;
		if (strcmp(rstr, "v") == 0) {
			//ÁÂÇ¥
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
	return BoundingOrientedBox(XMFLOAT3(0.0f, 0.0f, 0.0f), MAXpos.f3, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
}

void Mesh::CreateWallMesh(float width, float height, float depth)
{
	MAXpos = XMFLOAT3(width, height, depth);
}

int Mesh::GetMeshIndex(string meshName)
{
	for (int i = 0; i < MeshNameArr.size(); ++i) {
		if (MeshNameArr[i] == meshName) {
			return i;
		}
	}
	return -1;
}

int Mesh::AddMeshName(string meshName)
{
	int r = MeshNameArr.size();
	MeshNameArr.push_back(meshName);
	return r;
}

GameObject::GameObject()
{
	isExist = false;
	m_worldMatrix.Id();
	mesh.MAXpos = 0;
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
	BoundingOrientedBox obb_local = mesh.GetOBB();
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

void GameObject::OnCollisionRayWithBullet(GameObject* shooter)
{
}
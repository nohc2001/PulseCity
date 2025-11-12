#include "stdafx.h"
#include "Render.h"
#include "Game.h"
#include "GameObject.h"
#include "Mesh.h"

Mesh* BulletRay::mesh = nullptr;

GameObject::GameObject()
{
	isExist = true;
	tickLVelocity = vec4(0, 0, 0, 0);
}

GameObject::~GameObject()
{
}

void GameObject::Update(float delatTime)
{
	PositionInterpolation(delatTime);
}

void GameObject::Render()
{
	//9월8일
	if (!m_pMesh || !m_pShader) {
		static bool warned = false;
		if (!warned) {
			char dbg[256];
			snprintf(dbg, sizeof(dbg),
				"[RENDER][SKIP] obj=%p mesh=%p shader=%p (null -> not drawing)\n",
				(void*)this, (void*)m_pMesh, (void*)m_pShader);
			OutputDebugStringA(dbg);
			warned = true; // 같은 오브젝트에서 중복 스팸 방지
		}
		return;
	}

	XMFLOAT4X4 xmf4x4World;
	XMStoreFloat4x4(&xmf4x4World, DirectX::XMMatrixTranspose(m_worldMatrix));

	gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &xmf4x4World, 0);

	game.MyDiffuseTextureShader->SetTextureCommand(&gd.GlobalTextureArr[diffuseTextureIndex]);

	m_pMesh->Render(gd.pCommandList, 1);
}

void GameObject::Event(WinEvent evt)
{
}

void GameObject::SetMesh(Mesh* pMesh)
{
	m_pMesh = pMesh;
}

void GameObject::SetShader(Shader* pShader)
{
	m_pShader = pShader;
}

void GameObject::SetWorldMatrix(const XMMATRIX& worldMatrix)
{
	m_worldMatrix = worldMatrix;
}

const XMMATRIX& GameObject::GetWorldMatrix() const
{
	return m_worldMatrix;
}

//not using
BoundingOrientedBox GameObject::GetOBB()
{
	BoundingOrientedBox obb_local = m_pMesh->GetOBB();
	BoundingOrientedBox obb_world;
	obb_local.Transform(obb_world, m_worldMatrix);
	return obb_world;
}
//not using
OBB_vertexVector GameObject::GetOBBVertexs()
{
	OBB_vertexVector ovv;
	BoundingOrientedBox obb = GetOBB();
	vec4 xm = -obb.Extents.x * m_worldMatrix.right;
	vec4 xp = obb.Extents.x * m_worldMatrix.right;
	vec4 ym = -obb.Extents.y * m_worldMatrix.up;
	vec4 yp = obb.Extents.y * m_worldMatrix.up;
	vec4 zm = -obb.Extents.z * m_worldMatrix.look;
	vec4 zp = obb.Extents.z * m_worldMatrix.look;

	ovv.vertex[0][0][0] = xm + ym + zm + m_worldMatrix.pos;
	ovv.vertex[0][0][1] = xm + ym + zp + m_worldMatrix.pos;
	ovv.vertex[0][1][0] = xm + yp + zm + m_worldMatrix.pos;
	ovv.vertex[0][1][1] = xm + yp + zp + m_worldMatrix.pos;
	ovv.vertex[1][0][0] = xp + ym + zm + m_worldMatrix.pos;
	ovv.vertex[1][0][1] = xp + ym + zp + m_worldMatrix.pos;
	ovv.vertex[1][1][0] = xp + yp + zm + m_worldMatrix.pos;
	ovv.vertex[1][1][1] = xp + yp + zp + m_worldMatrix.pos;
	return ovv;
}

void GameObject::LookAt(vec4 look, vec4 up)
{
	m_worldMatrix.LookAt(look, up);
}
//not using
void GameObject::CollisionMove(GameObject* gbj1, GameObject* gbj2)
{
	constexpr float epsillon = 0.01f;

	bool bi = XMColorEqual(gbj1->tickLVelocity, XMVectorZero());/*
	bool bia = XMColorEqual(gbj1->tickAVelocity, XMVectorZero());*/
	bool bj = XMColorEqual(gbj2->tickLVelocity, XMVectorZero());/*
	bool bja = XMColorEqual(gbj2->tickAVelocity, XMVectorZero());*/

	GameObject* movObj = nullptr;
	GameObject* colObj = nullptr;
	BoundingOrientedBox obb1 = gbj1->GetOBB();
	BoundingOrientedBox obb2 = gbj2->GetOBB();
	//float len = vec4(gbj1->m_worldMatrix.pos - gbj2->m_worldMatrix.pos).len3;
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
	//	movObj->m_worldMatrix.trQ(movObj->tickAVelocity);
	//	obb1 = movObj->GetOBB();
	//	OBB_vertexVector ovv_later = movObj->GetOBBVertexs();
	//	movObj->m_worldMatrix.trQinv(movObj->tickAVelocity);
	//	matrix imat = colObj->m_worldMatrix.RTInverse;
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
	//	gbj1->m_worldMatrix.trQ(gbj1->tickAVelocity);
	//}
	//if (bja) {
	//	gbj2->m_worldMatrix.trQ(gbj2->tickAVelocity);
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
//not using
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
//not using
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
//not using
void GameObject::OnCollision(GameObject* other)
{
	//GameObject Collision...
}
//not using
void GameObject::OnCollisionRayWithBullet()
{
}

void GameObject::PositionInterpolation(float deltaTime)
{
	float pow = deltaTime * 10;
	Destpos.w = 1;
	m_worldMatrix.pos = (1.0f - pow) * m_worldMatrix.pos + pow * Destpos;

	//m_worldMatrix.pos = Destpos;
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

matrix BulletRay::GetRayMatrix(float distance) {
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
		matrix mat = GetRayMatrix(distance);
		mat.transpose();
		gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &mat, 0);
		mesh->Render(gd.pCommandList, 1);
	}
}

LMoveObject_CollisionTest::LMoveObject_CollisionTest()
{
}

LMoveObject_CollisionTest::~LMoveObject_CollisionTest()
{
}

void LMoveObject_CollisionTest::Update(float deltaTime)
{
	tickLVelocity = 3 * LVelocity * deltaTime;
	tickLVelocity.w = 0;

	if (m_worldMatrix.pos.fast_square_of_len3 > 100) {
		LVelocity = vec4(rand() % 2 - 1, rand() % 2 - 1, rand() % 2 - 1, 0);
		LVelocity -= m_worldMatrix.pos;
		LVelocity.len3 = 1;
	}
}

void LMoveObject_CollisionTest::Render()
{
	GameObject::Render();
}

void LMoveObject_CollisionTest::Event(WinEvent evt)
{
}

void LMoveObject_CollisionTest::OnCollision(GameObject* other)
{
}
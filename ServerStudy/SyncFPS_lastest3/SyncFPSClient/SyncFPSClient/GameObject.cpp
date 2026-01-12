#include "stdafx.h"
#include "Render.h"
#include "Game.h"
#include "GameObject.h"
#include "Mesh.h"

Mesh* BulletRay::mesh = nullptr;
ViewportData* Hierarchy_Object::renderViewPort = nullptr;

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
	if (!m_pMesh || !m_pShader) {
		static bool warned = false;
		if (!warned) {
			char dbg[256];
			snprintf(dbg, sizeof(dbg),
				"[RENDER][SKIP] obj=%p mesh=%p shader=%p (null -> not drawing)\n",
				(void*)this, (void*)m_pMesh, (void*)m_pShader);
			OutputDebugStringA(dbg);
			warned = true;
		}
		return;
	}

	XMFLOAT4X4 xmf4x4World;
	XMStoreFloat4x4(&xmf4x4World, DirectX::XMMatrixTranspose(m_worldMatrix));

	gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &xmf4x4World, 0);

	//game.MyDiffuseTextureShader->SetTextureCommand(&gd.GlobalTextureArr[diffuseTextureIndex]);
	int materialIndex = MaterialIndex;
	Material& mat = game.MaterialTable[materialIndex];
	gd.pCommandList->SetGraphicsRootDescriptorTable(3, mat.hGPU);
	gd.pCommandList->SetGraphicsRootDescriptorTable(4, mat.CB_Resource.hGpu);

	m_pMesh->Render(gd.pCommandList, 1);
}

void GameObject::RenderShadowMap()
{
	if (rmod == eRenderMeshMod::single_Mesh) {
		if (!m_pMesh || !m_pShader) {
			return;
		}

		XMFLOAT4X4 xmf4x4World;
		XMStoreFloat4x4(&xmf4x4World, DirectX::XMMatrixTranspose(m_worldMatrix));

		gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &xmf4x4World, 0);

		/*if (m_pTexture) {
			gd.pCommandList->SetGraphicsRootDescriptorTable(3, m_pTexture->hGpu);
		}*/

		m_pMesh->Render(gd.pCommandList, 1);
	}
	else {
		pModel->Render(gd.pCommandList, m_worldMatrix, Shader::RegisterEnum::RenderShadowMap);
	}
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

void Hierarchy_Object::Render_Inherit(matrix parent_world, Shader::RegisterEnum sre)
{
	XMMATRIX sav = XMMatrixMultiply(m_worldMatrix, parent_world);
	BoundingOrientedBox obb = GetOBBw(sav);
	if (obb.Extents.x > 0 && renderViewPort->m_xmFrustumWorld.Intersects(obb)) {
		if (rmod == GameObject::eRenderMeshMod::single_Mesh && m_pMesh != nullptr) {
			matrix m = sav;
			m.transpose();

			//set world matrix in shader
			gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &m, 0);
			//set texture in shader
			//game.MyPBRShader1->SetTextureCommand()
			int materialIndex = Mesh_materialIndex;
			Material& mat = game.MaterialTable[materialIndex];
			/*GPUResource* diffuseTex = &game.DefaultTex;
			if (mat.ti.Diffuse >= 0) diffuseTex = game.TextureTable[mat.ti.Diffuse];
			GPUResource* normalTex = &game.DefaultNoramlTex;
			if (mat.ti.Normal >= 0) normalTex = game.TextureTable[mat.ti.Normal];
			GPUResource* ambientTex = &game.DefaultTex;
			if (mat.ti.AmbientOcculsion >= 0) ambientTex = game.TextureTable[mat.ti.AmbientOcculsion];
			GPUResource* MetalicTex = &game.DefaultAmbientTex;
			if (mat.ti.Metalic >= 0) MetalicTex = game.TextureTable[mat.ti.Metalic];
			GPUResource* roughnessTex = &game.DefaultAmbientTex;
			if (mat.ti.Roughness >= 0) roughnessTex = game.TextureTable[mat.ti.Roughness];
			game.MyPBRShader1->SetTextureCommand(diffuseTex, normalTex, ambientTex, MetalicTex, roughnessTex);*/

			gd.pCommandList->SetGraphicsRootDescriptorTable(3, mat.hGPU);
			gd.pCommandList->SetGraphicsRootDescriptorTable(4, mat.CB_Resource.hGpu);
			m_pMesh->Render(gd.pCommandList, 1);
		}
		else if (rmod == GameObject::eRenderMeshMod::Model && pModel != nullptr) {
			pModel->Render(gd.pCommandList, sav, sre);
		}
	}

	for (int i = 0; i < childCount; ++i) {
		childs[i]->Render_Inherit(sav, sre);
	}
}

void Hierarchy_Object::Render_Inherit_CullingOrtho(matrix parent_world, Shader::RegisterEnum sre)
{
	XMMATRIX sav = XMMatrixMultiply(m_worldMatrix, parent_world);
	BoundingOrientedBox obb = GetOBBw(sav);
	if (obb.Extents.x > 0 && renderViewPort->OrthoFrustum.Intersects(obb)) {
		if (rmod == GameObject::eRenderMeshMod::single_Mesh && m_pMesh != nullptr) {
			matrix m = sav;
			m.transpose();

			//set world matrix in shader
			gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &m, 0);
			//set texture in shader
			//game.MyPBRShader1->SetTextureCommand()
			int materialIndex = Mesh_materialIndex;
			Material& mat = game.MaterialTable[materialIndex];
			/*GPUResource* diffuseTex = &game.DefaultTex;
			if (mat.ti.Diffuse >= 0) diffuseTex = game.TextureTable[mat.ti.Diffuse];
			GPUResource* normalTex = &game.DefaultNoramlTex;
			if (mat.ti.Normal >= 0) normalTex = game.TextureTable[mat.ti.Normal];
			GPUResource* ambientTex = &game.DefaultTex;
			if (mat.ti.AmbientOcculsion >= 0) ambientTex = game.TextureTable[mat.ti.AmbientOcculsion];
			GPUResource* MetalicTex = &game.DefaultAmbientTex;
			if (mat.ti.Metalic >= 0) MetalicTex = game.TextureTable[mat.ti.Metalic];
			GPUResource* roughnessTex = &game.DefaultAmbientTex;
			if (mat.ti.Roughness >= 0) roughnessTex = game.TextureTable[mat.ti.Roughness];
			game.MyPBRShader1->SetTextureCommand(diffuseTex, normalTex, ambientTex, MetalicTex, roughnessTex);*/

			gd.pCommandList->SetGraphicsRootDescriptorTable(3, mat.hGPU);
			gd.pCommandList->SetGraphicsRootDescriptorTable(4, mat.CB_Resource.hGpu);
			m_pMesh->Render(gd.pCommandList, 1);
		}
		else if (rmod == GameObject::eRenderMeshMod::Model && pModel != nullptr) {
			pModel->Render(gd.pCommandList, sav, sre);
		}
	}

	for (int i = 0; i < childCount; ++i) {
		childs[i]->Render_Inherit_CullingOrtho(sav, sre);
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
	map->ExtendMapAABB(obb);

	for (int i = 0; i < childCount; ++i) {
		childs[i]->InitMapAABB_Inherit(origin, sav);
	}
}

BoundingOrientedBox Hierarchy_Object::GetOBBw(matrix worldMat)
{
	BoundingOrientedBox obb_local;
	if (m_pMesh == nullptr) {
		obb_local.Extents.x = -1;
		return obb_local;
	}
	switch (rmod) {
	case GameObject::eRenderMeshMod::single_Mesh:
		obb_local = m_pMesh->GetOBB();
		break;
	case GameObject::eRenderMeshMod::Model:
		obb_local = pModel->GetOBB();
		break;
	}
	BoundingOrientedBox obb_world;
	obb_local.Transform(obb_world, worldMat);
	return obb_world;
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

ui64 GameMap::GetSpaceHash(int x, int y, int z)
{
	ui64 Flag = (ui16)(x & 1023);
	Flag = (Flag << 10) | (ui16)(y & 1023);
	Flag = (Flag << 10) | (ui16)(z & 1023);
	ui64 result = 0;
	for (int i = 0; i < pdepcnt; ++i) {
		ui64 s = _pdep_u64(Flag, pdep_src2[i]);
		result |= s;
	}
	return result;
}

GameMap::Posindex GameMap::GetInvSpaceHash(ui64 hash)
{
	ui64 result = 0;
	for (int i = 0; i < inv_pdepcnt; ++i) {
		ui64 s = _pdep_u64(hash, inv_pdep_src2[i]);
		result |= s;
	}

	GameMap::Posindex pos;
	pos.x = (result & 1023);
	pos.y = ((result >> 10) & 1023);
	pos.z = ((result >> 20) & 1023);
	return pos;
}

atomic<int> calculCnt = 0;
void CalculCollide(GameMap* map, byte8* isCollide, int xDivide, int yDivide, int zDivide, float CollisionCubeMargin, int xiStart, int xiEnd, int* FlagCount) {
	BoundingBox Cube;
	vec4 point0, point1;
	point0.x = map->AABB[0].x;
	point1.x = map->AABB[0].x + CollisionCubeMargin;
	point0.y = map->AABB[0].y;
	point1.y = map->AABB[0].y + CollisionCubeMargin;
	point0.z = map->AABB[0].z;
	point1.z = map->AABB[0].z + CollisionCubeMargin;
	BoundingBox::CreateFromPoints(Cube, point0, point1);

	vec4* AABB = map->AABB;
	for (int xi = xiStart; xi < xiEnd; ++xi) {
		Cube.Center.x = AABB[0].x + (CollisionCubeMargin * xi) + CollisionCubeMargin * 0.5f;
		dbglog1(L"CollideBake_%d\n", ++calculCnt);
		for (int yi = 0; yi < yDivide; ++yi) {
			Cube.Center.y = AABB[0].y + (CollisionCubeMargin * yi) + CollisionCubeMargin * 0.5f;
			for (int zi = 0; zi < zDivide; ++zi) {
				Cube.Center.z = AABB[0].z + (CollisionCubeMargin * zi) + CollisionCubeMargin * 0.5f;

				ui64 Flag = (ui16)zi;
				Flag = (Flag << 10) | (ui16)yi;
				Flag = (Flag << 10) | (ui16)xi;

				int n = xi * (zDivide * yDivide) + yi * (zDivide)+zi;
				if (map->MapObjects[0]->Collision_Inherit(XMMatrixIdentity(), Cube)) {
					//isCollide[xi * (zDivide * yDivide) + yi * (zDivide)+zi] = true;
					isCollide[n >> 3] |= (1 << (n & 7));
					//if collision
					for (int i = 0; i < 30; ++i) {
						ui64 f = (1 << i);
						if (Flag & f) FlagCount[i] += 1;
					}
				}
				else {
					//isCollide[xi * (zDivide * yDivide) + yi * (zDivide)+zi] = false;
					isCollide[n >> 3] &= ~(1 << (n & 7));
				}
			}
		}
	}
}

// takes 1hours..
void GameMap::BakeStaticCollision()
{
	// Get Map's AABB (pre bake in LoadMap function.)
	MapObjects[0]->InitMapAABB_Inherit(this, XMMatrixIdentity());
	// vec4 GameMap::AABB[2];

	vec4 deltaAABB = AABB[1] - AABB[0];
	float CollisionCubeMargin = 1.0f;
	CollisionCubeMargin = deltaAABB.x / 1000.0f;
	if (CollisionCubeMargin < deltaAABB.y / 1000.0f) CollisionCubeMargin = deltaAABB.y / 1000.0f;
	if (CollisionCubeMargin < deltaAABB.z / 1000.0f) CollisionCubeMargin = deltaAABB.z / 1000.0f;

	// Divide Space and repeat OBB Intersect cmp;
	int xDivide = deltaAABB.x / CollisionCubeMargin;
	int yDivide = deltaAABB.y / CollisionCubeMargin;
	int zDivide = deltaAABB.z / CollisionCubeMargin;

	BoundingBox Cube;
	vec4 point0, point1;
	point0.x = AABB[0].x;
	point1.x = AABB[0].x + CollisionCubeMargin;
	point0.y = AABB[0].y;
	point1.y = AABB[0].y + CollisionCubeMargin;
	point0.z = AABB[0].z;
	point1.z = AABB[0].z + CollisionCubeMargin;
	BoundingBox::CreateFromPoints(Cube, point0, point1);

	// whole collision calculate -> too slow (try multithread..)
	int FlagCount[30] = {};
	int FlagCount_th[8][30] = { {} };
	byte8* isCollide = new byte8[xDivide * yDivide * zDivide / 8 + 1];

	thread* threads[8];
	for (int i = 0; i < 8; ++i) {
		threads[i] = new thread(CalculCollide, this, isCollide, xDivide, yDivide, zDivide, CollisionCubeMargin, 125 * i, 125 * (i + 1), &FlagCount_th[i][0]);
	}

	for (int i = 0; i < 8; ++i) {
		threads[i]->join();
	}

	for (int i = 0; i < 8; ++i) {
		for (int k = 0; k < 30; ++k) {
			FlagCount[k] += FlagCount_th[i][k];
		}
	}

	goto JUMP001;

	for (int xi = 0; xi < xDivide; ++xi) {
		Cube.Center.x = AABB[0].x + (CollisionCubeMargin * xi) + CollisionCubeMargin * 0.5f;
		dbglog1(L"CollideBake_%d\n", xi);
		for (int yi = 0; yi < yDivide; ++yi) {
			Cube.Center.y = AABB[0].y + (CollisionCubeMargin * yi) + CollisionCubeMargin * 0.5f;
			for (int zi = 0; zi < zDivide; ++zi) {
				Cube.Center.z = AABB[0].z + (CollisionCubeMargin * zi) + CollisionCubeMargin * 0.5f;

				ui64 Flag = (ui16)zi;
				Flag = (Flag << 10) | (ui16)yi;
				Flag = (Flag << 10) | (ui16)xi;

				if (MapObjects[0]->Collision_Inherit(XMMatrixIdentity(), Cube)) {
					isCollide[xi * (zDivide * yDivide) + yi * (zDivide)+zi] = true;
					//if collision
					for (int i = 0; i < 30; ++i) {
						ui64 f = (1 << i);
						if (Flag & f) FlagCount[i] += 1;
					}
				}
				else isCollide[xi * (zDivide * yDivide) + yi * (zDivide)+zi] = false;
			}
		}
	}

JUMP001:
	//Flag Calculate
	double FlagRate[30] = {};
	double limit = pow(2, 30);
	for (int i = 0; i < 30; ++i) {
		FlagRate[i] = (double)FlagCount[i] / limit;
		FlagRate[i] = abs(FlagRate[i] - 0.5f);
	}

	int FlagIndex[30] = {};
	for (int i = 0; i < 30; ++i) { FlagIndex[i] = i; }
	for (int i = 0; i < 30; ++i) {
		for (int k = i + 1; k < 30; ++k) {
			if (FlagRate[FlagIndex[i]] > FlagRate[FlagIndex[k]]) {
				int n = FlagRate[FlagIndex[i]];
				FlagRate[FlagIndex[i]] = FlagRate[FlagIndex[k]];
				FlagRate[FlagIndex[k]] = n;

				n = FlagIndex[i];
				FlagIndex[i] = FlagIndex[k];
				FlagIndex[k] = n;
			}
		}
	}

	int FlagToFlag[30] = {};
	for (int i = 0; i < 30; ++i) {
		for (int k = 0; k < 30; ++k) {
			if (FlagIndex[k] == i) FlagToFlag[i] = k;
		}
	}

	//get hash function
	bool complete[30] = {};
	int sav = 0;
	pdepcnt = 0;
	int completecnt = 0;
	while (completecnt < 30) {
		for (int i = 0; i < 30; ++i) {
			if (complete[i] == false) {
				sav = i;
				break;
			}
		}

		for (int i = 0; i < 30; ++i) {
			if (complete[i]) continue;

			if (FlagToFlag[sav] <= FlagToFlag[i]) {
				complete[i] = true;
				pdep_src2[pdepcnt] |= (ui64)1 << (ui64)FlagToFlag[i];
				completecnt += 1;
			}
			else continue;

			sav = i;
		}
		pdepcnt += 1;
	}

	//get inv hash function
	for (int i = 0; i < 30; ++i) complete[i] = 0;
	sav = 0;
	inv_pdepcnt = 0;
	completecnt = 0;
	while (completecnt < 30) {
		for (int i = 0; i < 30; ++i) {
			if (complete[i] == false) {
				sav = i;
				break;
			}
		}

		for (int i = 0; i < 30; ++i) {
			if (complete[i]) continue;

			if (FlagIndex[sav] <= FlagIndex[i]) {
				complete[i] = true;
				inv_pdep_src2[pdepcnt] |= (ui64)1 << (ui64)FlagIndex[i];
				completecnt += 1;
			}
			else continue;

			sav = i;
		}
		inv_pdepcnt += 1;
	}

	int ilimit = 1 << 30;
	vecarr<range<ui32, bool>> Ranges;
	Ranges.Init(4096);
	bool lastValue = isCollide[0];
	range<ui32, bool> curr;
	curr.value = isCollide[0];
	for (int i = 0; i < ilimit; ++i) {
		GameMap::Posindex pos = GetInvSpaceHash(i);
		bool b = isCollide[pos.x * (zDivide * yDivide) + pos.y * (zDivide)+pos.z];
		if (b != lastValue) {
			curr.end = i - 1;
			Ranges.push_back(curr);
			curr.value = b;
		}
		lastValue = b;
	}
	curr.end = ilimit - 1;
	Ranges.push_back(curr);

	IsCollision = AddRangeArr<ui32, bool>(0, Ranges);
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
		int indCnt = 0;
		ifs2.read((char*)&vertCnt, sizeof(int));
		ifs2.read((char*)&indCnt, sizeof(int));

		vector<BumpMesh::Vertex> verts;
		verts.reserve(vertCnt);
		verts.resize(vertCnt);
		vector<TriangleIndex> indexs;
		int tricnt = (indCnt / 3);
		indexs.reserve(tricnt);
		indexs.resize(tricnt);
		for (int k = 0; k < vertCnt; ++k) {
			ifs2.read((char*)&verts[k].position, sizeof(float) * 3);
			ifs2.read((char*)&verts[k].uv, sizeof(float) * 2);
			ifs2.read((char*)&verts[k].normal, sizeof(float) * 3);
			ifs2.read((char*)&verts[k].tangent, sizeof(float) * 3);
			float w = 0; // bitangent direction??
			ifs2.read((char*)&w, sizeof(float));
		}

		for (int k = 0; k < tricnt; ++k) {
			ifs2.read((char*)&indexs[k].v[0], sizeof(UINT));
			ifs2.read((char*)&indexs[k].v[1], sizeof(UINT));
			ifs2.read((char*)&indexs[k].v[2], sizeof(UINT));
		}

		ifs2.close();
		mesh->CreateMesh_FromVertexAndIndexData(verts, indexs);
		//mesh->ReadMeshFromFile_OBJ(filename.c_str(), vec4(1, 1, 1, 1), false);

		ifs.read((char*)&mesh->OBB_Tr, sizeof(float) * 3);
		ifs.read((char*)&mesh->OBB_Ext, sizeof(float) * 3);
		map->meshes[i] = mesh;
	}
	/*gd.pCommandList->Close();
	ID3D12CommandList* ppCommandLists[] = { gd.pCommandList };
	gd.pCommandQueue->ExecuteCommandLists(1, ppCommandLists);
	gd.WaitGPUComplete();*/

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

			char* isnormal = &filename[filename.size() - 10];
			char* isnormalmap = &filename[filename.size() - 13];
			char* isnormalDX = &filename[filename.size() - 12];
			if ((strcmp(isnormal, "Normal.tex") == 0 ||
				strcmp(isnormalDX, "NormalDX.tex") == 0) ||
				strcmp(isnormalmap, "normalmap.tex") == 0) {
				float xtotal = 0;
				float ytotal = 0;
				float ztotal = 0;
				for (int ix = 0; ix < width; ++ix) {
					int h = rand() % height;
					BYTE* p = &pixels[h * width * 4 + ix * 4];
					xtotal += p[0];
					ytotal += p[1];
					ztotal += p[2];
				}
				if (ztotal < ytotal) {
					// x = z
					// y = -x
					// z = y
					for (int ix = 0; ix < width; ++ix) {
						for (int iy = 0; iy < height; ++iy) {
							BYTE* p = &pixels[iy * width * 4 + ix * 4];
							BYTE z = p[2];
							BYTE y = p[1];
							BYTE x = p[0];
							p[0] = z;
							p[1] = 255 - x;
							p[2] = y;
						}
					}
				}
				if (ztotal < xtotal) {
					// x = z
					// y = y
					// z = x
					for (int ix = 0; ix < width; ++ix) {
						for (int iy = 0; iy < height; ++iy) {
							BYTE* p = &pixels[iy * width * 4 + ix * 4];
							BYTE k = p[2];
							p[2] = p[0];
							p[0] = k;
						}
					}
				}
			}
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
	/*gd.pCommandAllocator->Reset();
	gd.pCommandList->Reset(gd.pCommandAllocator, NULL);*/

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
			go->rmod = GameObject::eRenderMeshMod::single_Mesh;
			int meshid = 0;
			int materialNum = 0;
			int materialId = 0;
			ifs.read((char*)&meshid, sizeof(int));
			if (meshid == 75) {
				cout << endl;
			}
			if (meshid < 0) {
				go->m_pMesh = nullptr;
				ifs.read((char*)&materialNum, sizeof(int));
			}
			else {
				go->m_pMesh = map->meshes[meshid];
				ifs.read((char*)&materialNum, sizeof(int));
				for (int k = 0; k < materialNum; ++k) {
					ifs.read((char*)&materialId, sizeof(int));
					go->Mesh_materialIndex = MaterialTableStart + materialId;
				}
			}
		}
		else {
			go->rmod = GameObject::eRenderMeshMod::Model;
			//Model
			int ModelID;
			ifs.read((char*)&ModelID, sizeof(int));
			if (ModelID < 0) {
				go->pModel = nullptr;
			}
			else {
				go->pModel = map->models[ModelID];
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

		go->m_pShader = game.MyPBRShader1;

		map->MapObjects[i] = go;
	}

	//BakeStaticCollision();
}

void SphereLODObject::Update(float deltaTime)
{
	m_worldMatrix.pos = FixedPos;
}

void SphereLODObject::Render()
{
	if (MeshNear && MeshFar)
	{
		vec4 cam = gd.viewportArr[0].Camera_Pos;
		vec4 pos = m_worldMatrix.pos;

		vec4 d = cam - pos;
		float dist = d.len3;

		m_pMesh = (dist < SwitchDistance) ? MeshNear : MeshFar;
	}

	GameObject::Render();
}

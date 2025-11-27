#include "stdafx.h"
#include "Monster.h"
#include "Render.h"
#include "Mesh.h"
#include "Game.h"

void Monster::Update(float deltaTime)
{
	PositionInterpolation(deltaTime);

	if (this->HPBarIndex < 0) return;

	matrix viewMatrix = XMLoadFloat4x4((XMFLOAT4X4*)&gd.viewportArr[0].ViewMatrix);
	matrix invViewMatrix = viewMatrix.RTInverse;
	matrix cameraWorldMat = invViewMatrix;

	matrix hpBarWorldMat;
	//hpBarWorldMat.right = cameraWorldMat.right;
	//hpBarWorldMat.up = cameraWorldMat.up;
	//hpBarWorldMat.look = cameraWorldMat.look;

	hpBarWorldMat.Id();

	hpBarWorldMat.LookAt(cameraWorldMat.right);

	hpBarWorldMat.pos = this->m_worldMatrix.pos + vec4(0.0f, 1.0f, 0.0f, 0);

	hpBarWorldMat.look *= 2 * HP / MaxHP;

	game.NpcHPBars[this->HPBarIndex] = hpBarWorldMat;

	/*if (collideCount == 0) isGround = false;
	collideCount = 0;

	vec4 monsterPos = m_worldMatrix.pos;
	monsterPos.w = 0;
	vec4 playerPos = game.player->GetWorldMatrix().r[3];
	playerPos.w = 0;

	vec4 toPlayer = playerPos - monsterPos;
	toPlayer.y = 0.0f;
	float distanceToPlayer = toPlayer.len3;

	LVelocity.y -= 9.81f * deltaTime;

	tickLVelocity = LVelocity * deltaTime;

	if (distanceToPlayer <= m_chaseRange) {
		m_targetPos = playerPos;
		m_isMove = true;

		toPlayer.len3 = 1.0f;
		tickLVelocity += toPlayer * m_speed * deltaTime;
		m_worldMatrix.LookAt(toPlayer);

		if (distanceToPlayer < 2.0f) {
			tickLVelocity.x = 0;
			tickLVelocity.z = 0;
		}

		m_fireTimer += deltaTime;
		if (m_fireTimer >= m_fireDelay) {
			m_fireTimer = 0.0f;

			vec4 rayStart = monsterPos + (m_worldMatrix.look * 0.5f);
			rayStart.y += 0.7f;
			vec4 rayDirection = playerPos - rayStart;

			float InverseAccurcy = distanceToPlayer / 6;
			rayDirection.x += (-1 + (float)(rand() & 255) / 128.0f) * InverseAccurcy;
			rayDirection.y += (-1 + (float)(rand() & 255) / 128.0f) * InverseAccurcy;
			rayDirection.z += (-1 + (float)(rand() & 255) / 128.0f) * InverseAccurcy;
			rayDirection.len3 = 1.0f;

			game.FireRaycast(this, rayStart, rayDirection, m_chaseRange);
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
		}

		m_patrolTimer += deltaTime;

		if (m_patrolTimer >= 5.0f) {
			tickLVelocity.x = 0;
			tickLVelocity.z = 0;
			m_isMove = false;
			return;
		}

		vec4 currentPos = m_worldMatrix.pos;
		currentPos.w = 0;
		vec4 direction = m_targetPos - currentPos;
		direction.y = 0.0f;
		float distance = direction.len3;

		if (distance > 1.0f) {
			direction.len3 = 1.0f;
			tickLVelocity += direction * m_speed * deltaTime;
			m_worldMatrix.LookAt(direction);
		}
		else {
			tickLVelocity.x = 0;
			tickLVelocity.z = 0;
			m_isMove = false;
		}
	}*/
}

void Monster::Render()
{
	if (isDead) {
		return;
	}
	GameObject::Render();
}

void Monster::OnCollision(GameObject* other)
{
	/*collideCount += 1;
	float belowDist = 0;
	BoundingOrientedBox otherOBB = other->GetOBB();
	bool belowhit = otherOBB.Intersects(m_worldMatrix.pos, vec4(0, -1, 0, 0), belowDist);
	if (belowhit && belowDist < m_pMesh->GetOBB().Extents.y + 0.4f) {
		LVelocity.y = 0;
		isGround = true;
	}*/
}

void Monster::OnCollisionRayWithBullet()
{
	//HP -= 10;
	//if (HP < 0) {
	//	isExist = false;
	//	//Destory Object.
	//}
}

void Monster::Init(const XMMATRIX& initialWorldMatrix)
{
	SetWorldMatrix(initialWorldMatrix);
	//m_homePos = m_worldMatrix.pos;
}

BoundingOrientedBox Monster::GetOBB()
{
	BoundingOrientedBox obb_local = m_pMesh->GetOBB();
	obb_local.Extents.x = obb_local.Extents.z;
	BoundingOrientedBox obb_world;
	matrix id = XMMatrixIdentity();
	id.pos = m_worldMatrix.pos;
	obb_local.Transform(obb_world, id);
	return obb_world;
}
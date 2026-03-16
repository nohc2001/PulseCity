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
	hpBarWorldMat.Id();
	hpBarWorldMat.LookAt(cameraWorldMat.right);
	hpBarWorldMat.pos = this->m_worldMatrix.pos + vec4(0.0f, 1.0f, 0.0f, 0);
	hpBarWorldMat.look *= 2 * HP / MaxHP;

	game.NpcHPBars[this->HPBarIndex] = hpBarWorldMat;
}

void Monster::Render()
{
	if (isDead) {
		return;
	}
	GameObject::Render();
}

void Monster::Init(const XMMATRIX& initialWorldMatrix)
{
	m_worldMatrix = (initialWorldMatrix);
	//m_homePos = m_worldMatrix.pos;
}
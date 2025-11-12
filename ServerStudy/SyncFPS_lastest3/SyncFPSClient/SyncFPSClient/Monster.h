#pragma once
#include "stdafx.h"
#include "GameObject.h"

class Monster : public GameObject {
private:
	/*float m_speed = 2.0f;
	float m_patrolRange = 20.0f;
	float m_chaseRange = 10.0f;
	vec4 m_homePos;
	vec4 m_targetPos;

	bool m_isMove = false;
	float m_patrolTimer = 0.0f;
	float m_fireDelay = 1.0f;
	float m_fireTimer = 0.0f;

	int collideCount = 0;
	bool isGround = false;

	float HP = 30;
	const float MAXHP = 30;*/

public:
	bool isDead;

	float HP = 30;
	float MaxHP = 30;

	int HPBarIndex;
	matrix HPMatrix;

	Monster() {}
	virtual ~Monster() {}

	virtual void Update(float deltaTime) override;

	virtual void Render();

	virtual void OnCollision(GameObject* other) override;

	virtual void OnCollisionRayWithBullet();

	void Init(const XMMATRIX& initialWorldMatrix);

	virtual BoundingOrientedBox GetOBB();

	//bool& getisDead() { return ((bool*)this)[17]; }
	//void SetidDead(bool v) { ((bool*)this)[17] = v; }

	//__declspec(property(get = getisDead, put = SetidDead)) bool isDead;
};
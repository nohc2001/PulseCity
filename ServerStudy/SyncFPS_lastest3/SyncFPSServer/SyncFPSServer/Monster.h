#pragma once
#include "stdafx.h"
#include "GameObject.h"

//astar pathfinding
struct AstarNode {
	int xIndex, zIndex;
	float worldx, worldz;
	bool cango;
	float gCost, hCost, fCost;
	AstarNode* parent;
};

AstarNode* FindClosestNode(float wx, float wz, const std::vector<AstarNode*>& allNodes);

/*
* 설명 : 간이 몬스터 클래스
*/
struct Monster : public GameObject {
	vec4 m_homePos;
	vec4 m_targetPos;
	float m_speed = 2.0f;
	float m_patrolRange = 20.0f;
	float m_chaseRange = 10.0f;
	float m_patrolTimer = 0.0f;
	float m_fireDelay = 1.0f;
	float m_fireTimer = 0.0f;
	float HP = 30;
	float MaxHP = 30;
	int collideCount = 0;
	int targetSeekIndex = 0;
	Player** Target = nullptr;
	static constexpr float MAXHP = 30;
	bool m_isMove = false;
	bool isGround = false;
	bool isDead = false;
	float respawntimer = 0;
	float pathfindTimer = 0.0f;

	Monster() {}
	virtual ~Monster() {}

	virtual void Update(float deltaTime) override;
	//virtual void Render();
	virtual void OnCollision(GameObject* other) override;

	virtual void OnStaticCollision(BoundingOrientedBox obb) override;

	virtual void OnCollisionRayWithBullet(GameObject* shooter);

	void Init(const XMMATRIX& initialWorldMatrix);

	void Respawn();

	virtual BoundingOrientedBox GetOBB();

	//astar pathfinding
	vector<AstarNode*> AstarSearch(AstarNode* start, AstarNode* destination, std::vector<AstarNode*>& allNodes);
	AstarNode* FindClosestNode(float wx, float wz, const std::vector<AstarNode*>& allNodes);
	void MoveByAstar(float deltaTime);
	std::vector<AstarNode*> path; // 현재 따라가야 할 경로
	size_t currentPathIndex = 0;
};
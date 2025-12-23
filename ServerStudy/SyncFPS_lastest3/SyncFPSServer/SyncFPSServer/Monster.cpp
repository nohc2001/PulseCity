#include "stdafx.h"
#include "Player.h"
#include "Monster.h"
#include "main.h"
#include "GameObjectTypes.h"

void Monster::Update(float deltaTime)
{
	if (isDead) {
		respawntimer += deltaTime;
		if (respawntimer > 1.0f) {
			Respawn();
			respawntimer = 0;
		}
	}
	else {
		if (collideCount == 0) isGround = false;
		collideCount = 0;

		if (isGround == false) {
			LVelocity.y -= 9.81f * deltaTime;
		}
		tickLVelocity = LVelocity * deltaTime;

		vec4 monsterPos = m_worldMatrix.pos;
		monsterPos.w = 0;

		if (Target == nullptr || (Target != nullptr && *Target == nullptr)) {
			int limitseek = 4;
			if (gameworld.clients.size == 0) {
				Target = nullptr;
				return;
			}

		SEEK_TARGET_LOOP:
			for (int i = targetSeekIndex; i < gameworld.clients.size; ++i) {
				limitseek -= 1;
				if (limitseek == 0) {
					return;
				}
				if (gameworld.clients.isnull(i)) continue;
				Target = (Player**)&gameworld.gameObjects[gameworld.clients[i].objindex];
				break;
			}
			if (limitseek != 0 && (Target == nullptr || *Target == nullptr)) {
				targetSeekIndex = 0;
				goto SEEK_TARGET_LOOP;
			}
		}

		vec4 playerPos = (*Target)->m_worldMatrix.pos;
		playerPos.w = 0;

		vec4 toPlayer = playerPos - monsterPos;
		toPlayer.y = 0.0f;
		float distanceToPlayer = toPlayer.len3;

		// 플레이어 추적
		if (distanceToPlayer <= m_chaseRange) {
			m_targetPos = playerPos;
			m_isMove = true;

			// A* 경로가 없거나, 다 소비했으면 새로 계산
			if (path.empty() || currentPathIndex >= path.size()) {
				AstarNode* start = FindClosestNode(monsterPos.x, monsterPos.z, gameworld.allnodes);
				AstarNode* goal = FindClosestNode(playerPos.x, playerPos.z, gameworld.allnodes);

				if (start && goal) {
					path = AstarSearch(start, goal, gameworld.allnodes);
					currentPathIndex = 0;
				}
			}

			// A* 경로가 있으면 그 경로를 따라 이동
			if (!path.empty() && currentPathIndex < path.size()) {
				MoveByAstar(deltaTime);
			}
			else {
				// 경로 계산 실패했을 때만 기존 직선 추적으로 fallback
				if (distanceToPlayer > 0.0001f) {
					toPlayer.len3 = 1.0f;
					tickLVelocity += toPlayer * m_speed * deltaTime;
					m_worldMatrix.SetLook(toPlayer);
				}
			}


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

				gameworld.FireRaycast(this, rayStart, rayDirection, m_chaseRange);
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

				AstarNode* start = FindClosestNode(monsterPos.x, monsterPos.z, gameworld.allnodes);
				AstarNode* goal = FindClosestNode(m_targetPos.x, m_targetPos.z, gameworld.allnodes);

				if (start && goal) {
					path = AstarSearch(start, goal, gameworld.allnodes);
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
				vec4 currentPos = m_worldMatrix.pos;
				currentPos.w = 0;
				vec4 direction = m_targetPos - currentPos;
				direction.y = 0.0f;
				float distance = direction.len3;

				if (distance > 1.0f) {
					direction.len3 = 1.0f;
					tickLVelocity += direction * m_speed * deltaTime;
					m_worldMatrix.SetLook(direction);
				}
				else {
					tickLVelocity.x = 0;
					tickLVelocity.z = 0;
					m_isMove = false;
				}
			}
		}

		if (gameworld.lowHit()) {
			int datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.currentIndex, this, GameObjectType::_Monster, &(m_worldMatrix), 64);
			gameworld.SendToAllClient(datacap);
		}
	}
}

void Monster::OnCollision(GameObject* other)
{
	collideCount += 1;
	float belowDist = 0;
	BoundingOrientedBox otherOBB = other->GetOBB();
	bool belowhit = otherOBB.Intersects(m_worldMatrix.pos, vec4(0, -1, 0, 0), belowDist);
	if (belowhit && belowDist < GetOBB().Extents.y + 1.0f) {
		LVelocity.y = 0;
		isGround = true;
	}
}

void Monster::OnStaticCollision(BoundingOrientedBox obb)
{
	collideCount += 1;
	float belowDist = 0;
	BoundingOrientedBox otherOBB = obb;
	bool belowhit = otherOBB.Intersects(m_worldMatrix.pos, vec4(0, -1, 0, 0), belowDist);
	if (belowhit && belowDist < GetOBB().Extents.y + 1.0f) {
		LVelocity.y = 0;
		isGround = true;
	}
}

void Monster::OnCollisionRayWithBullet(GameObject* shooter)
{
	HP -= 10;
	int datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.currentIndex, this, GameObjectType::_Monster, &HP, sizeof(int));
	gameworld.SendToAllClient(datacap);

	if (HP <= 0 && isDead == false) {
		isDead = true;
		int datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.currentIndex, this, GameObjectType::_Monster, &isDead, sizeof(bool));
		gameworld.SendToAllClient(datacap);

		vec4 prevpos = m_worldMatrix.pos;

		// when monster is dead, player's killcount +1
		void* vptr = *(void**)shooter;
		if (GameObjectType::VptrToTypeTable[vptr] == GameObjectType::_Player) {
			Player* p = (Player*)shooter;
			p->KillCount += 1;
			datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.clients[p->clientIndex].objindex, p, GameObjectType::_Player, &p->KillCount, sizeof(int));
			gameworld.clients[p->clientIndex].socket.Send(gameworld.tempbuffer.data, datacap);
		}

		//when monster is dead, loot random items;
		ItemLoot il = {};
		il.itemDrop.id = 1 + (rand() % (ItemTable.size() - 1));
		il.itemDrop.ItemCount = 1 + rand() % 5;
		il.pos = prevpos;
		int newindex = gameworld.DropedItems.Alloc();
		gameworld.DropedItems[newindex] = il;
		datacap = gameworld.Sending_ItemDrop(newindex, il);
		gameworld.SendToAllClient(datacap);
	}
}

void Monster::Init(const XMMATRIX& initialWorldMatrix)
{
	m_worldMatrix = (initialWorldMatrix);
	m_homePos = m_worldMatrix.pos;
}

void Monster::Respawn()
{
	Init(XMMatrixTranslation(rand() % 80 - 40, 10.0f, rand() % 80 - 40));
	m_isMove = false;
	int datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.currentIndex, this, GameObjectType::_Monster, &m_worldMatrix.pos, sizeof(float)*4);
	gameworld.SendToAllClient(datacap);

	isDead = false;
	datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.currentIndex, this, GameObjectType::_Monster, &isDead, sizeof(bool));
	gameworld.SendToAllClient(datacap);

	HP = 30;
	datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.currentIndex, this, GameObjectType::_Monster, &HP, sizeof(int));
	gameworld.SendToAllClient(datacap);
}

BoundingOrientedBox Monster::GetOBB()
{
	BoundingOrientedBox obb_local = Shape::IndexToShapeMap[ShapeIndex].GetMesh()->GetOBB();
	obb_local.Extents.x = obb_local.Extents.z;
	BoundingOrientedBox obb_world;
	matrix id = XMMatrixIdentity();
	id.pos = m_worldMatrix.pos;
	obb_local.Transform(obb_world, id);
	return obb_world;
}

vector<AstarNode*> GetNeighbors(AstarNode* current, const std::vector<AstarNode*>& allNodes, int gridWidth, int gridHeight)
{
	std::vector<AstarNode*> neighbors;

	for (int dx = -1; dx <= 1; ++dx) {
		for (int dz = -1; dz <= 1; ++dz) {
			if (dx == 0 && dz == 0)
				continue;

			int nx = current->xIndex + dx;
			int nz = current->zIndex + dz;

			// 범위 밖이면 무시
			if (nx < 0 || nz < 0 || nx >= gridWidth || nz >= gridHeight)
				continue;

			// 인덱스로 노드 접근
			AstarNode* neighbor = allNodes[nz * gridWidth + nx];
			if (neighbor->cango)
				neighbors.push_back(neighbor);
		}
	}

	return neighbors;
}

vector<AstarNode*> Monster::AstarSearch(AstarNode* start, AstarNode* destination, std::vector<AstarNode*>& allNodes)
{

	std::vector<AstarNode*> openList;
	std::vector<AstarNode*> closedList;

	if (start == nullptr || destination == nullptr)
		return {};

	// 초기화
	for (auto node : allNodes)
	{
		node->gCost = FLT_MAX;
		node->hCost = 0.0f;
		node->fCost = 0.0f;
		node->parent = nullptr;
	}

	start->gCost = 0.0f;
	start->gCost = 0.0f;
	start->hCost = std::abs(start->xIndex - destination->xIndex) + std::abs(start->zIndex - destination->zIndex);
	start->fCost = start->gCost + start->hCost;

	openList.push_back(start);

	while (!openList.empty())
	{
		// fCost 최소 노드 선택
		AstarNode* currentNode = openList[0];
		for (AstarNode* node : openList)
		{
			if (node->fCost < currentNode->fCost)
				currentNode = node;
		}

		// 목적지 도달 시 경로 추적
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

		// 오픈 리스트 → 클로즈드 리스트
		openList.erase(std::remove(openList.begin(), openList.end(), currentNode), openList.end());
		closedList.push_back(currentNode);

		// 이웃 탐색
		for (AstarNode* neighbor : GetNeighbors(currentNode, allNodes, 80, 80))
		{
			// 이미 방문한 노드는 스킵
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

	return {}; // 경로 없음
}

void Monster::MoveByAstar(float deltaTime)
{
	if (path.empty() || currentPathIndex >= path.size())
		return;

	AstarNode* targetNode = path[currentPathIndex];

	// 현재 몬스터 위치
	vec4 pos = m_worldMatrix.pos;
	pos.w = 1.0f;

	// A*에서 지정한 타겟 노드 위치 (y는 현재 높이 유지)
	vec4 target(targetNode->worldx, pos.y, targetNode->worldz, 1.0f);
	if (gameworld.AstarStartX > target.x) target.x = gameworld.AstarStartX;
	if (gameworld.AstarStartZ > target.z) target.x = gameworld.AstarStartZ;
	if (gameworld.AstarEndX < target.x) target.x = gameworld.AstarEndX;
	if (gameworld.AstarEndZ < target.z) target.z = gameworld.AstarEndZ;

	// 방향 벡터
	vec4 dir = target - pos;
	dir.y = 0.0f;
	float len = dir.len3;

	// 거의 도착했다고 보면 다음 노드로 넘어감
	if (len < 0.3f) {
		currentPathIndex++;
		return;
	}

	// 정규화
	dir /= len;

	tickLVelocity.x = dir.x * m_speed * deltaTime;
	tickLVelocity.z = dir.z * m_speed * deltaTime;

	m_worldMatrix.SetLook(dir);
}

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

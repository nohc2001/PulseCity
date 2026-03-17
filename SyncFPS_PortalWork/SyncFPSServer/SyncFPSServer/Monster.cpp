#include "stdafx.h"
#include "Player.h"
#include "Monster.h"
#include "main.h"
#include "GameObjectTypes.h"
#include "Zone.h"

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
				if (gameworld.clients[i].zoneId != zoneId) continue; 
				Target = (Player**)&gameworld.zones[zoneId].gameObjects[gameworld.clients[i].objindex];
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
				AstarNode* start = FindClosestNode(monsterPos.x, monsterPos.z, gameworld.zones[zoneId].allnodes);
				AstarNode* goal = FindClosestNode(playerPos.x, playerPos.z, gameworld.zones[zoneId].allnodes);

				if (start && goal) {
					path = AstarSearch(start, goal, gameworld.zones[zoneId].allnodes);
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

				gameworld.zones[zoneId].FireRaycast(this, rayStart, rayDirection, m_chaseRange);
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

				AstarNode* start = FindClosestNode(monsterPos.x, monsterPos.z, gameworld.zones[zoneId].allnodes);
				AstarNode* goal = FindClosestNode(m_targetPos.x, m_targetPos.z, gameworld.zones[zoneId].allnodes);

				if (start && goal) {
					path = AstarSearch(start, goal, gameworld.zones[zoneId].allnodes);
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
			gameworld.zones[zoneId].SendToAll(datacap);
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
	gameworld.zones[zoneId].SendToAll(datacap);

	if (HP <= 0 && isDead == false) {
		isDead = true;
		int datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.currentIndex, this, GameObjectType::_Monster, &isDead, sizeof(bool));
		gameworld.zones[zoneId].SendToAll(datacap);

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
		int newindex = gameworld.zones[zoneId].DropedItems.Alloc();
		gameworld.zones[zoneId].DropedItems[newindex] = il;
		datacap = gameworld.Sending_ItemDrop(newindex, il);
		gameworld.zones[zoneId].SendToAll(datacap);
	}
}

void Monster::Init(const XMMATRIX& initialWorldMatrix)
{
	m_worldMatrix = (initialWorldMatrix);
	m_homePos = m_worldMatrix.pos;
	
}

void Monster::Respawn()
{
	Zone& zone = gameworld.zones[zoneId];

	Init(XMMatrixTranslation(rand() % 80 - 40, 10.0f, rand() % 80 - 40));
	while (zone.map.isStaticCollision(GetOBB())) {
		Init(XMMatrixTranslation(rand() % 80 - 40, 10.0f, rand() % 80 - 40));
	}
	m_isMove = false;
	int datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.currentIndex, this, GameObjectType::_Monster, &m_worldMatrix.pos, sizeof(float)*4);
	gameworld.zones[zoneId].SendToAll(datacap);

	isDead = false;
	datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.currentIndex, this, GameObjectType::_Monster, &isDead, sizeof(bool));
	gameworld.zones[zoneId].SendToAll(datacap);

	HP = 30;
	datacap = gameworld.Sending_ChangeGameObjectMember(gameworld.currentIndex, this, GameObjectType::_Monster, &HP, sizeof(int));
	gameworld.zones[zoneId].SendToAll(datacap);
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


/*
<설명>
A*에서 현재 노드(current) 기준으로 갈 수 있는 이웃 노드를 찾아 반환한다.
8방향(상하좌우 + 대각선) 모두 검사한다.
맵 밖으로 나가는 노드는 제외한다.
이동 불가(cango==false) 노드는 제외한다.

매변 :
current : 이웃을 구하려는 기준 노드.
allNodes : 전체 노드.
gridWidth : 그리드 가로 크기.
gridHeight : 그리드 세로 크기.

return :
vector<AstarNode*> neighbor (이동 가능한 이웃 노드 목록)
if current가 가장자리 -> 범위 검사로 이웃 수가 줄어듦
if 주변 노드가 cango=false -> 목록에서 제외됨
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
//AI involved Code End : <chatgpt>

/* <설명>
- A* 알고리즘으로 start -> destination 최단 경로를 계산해 "노드 리스트(path)"를 반환한다.
- 노드마다 g/h/f 비용과 parent를 갱신하며, 목적지 도달 시 parent를 따라 역추적해 경로를 만든다.

매변 :
<start> : 시작 노드(몬스터 위치를 FindClosestNode로 스냅한 결과).
<destination> : 목적지 노드(플레이어/순찰 목표를 FindClosestNode로 스냅한 결과).
<allNodes> : 전체 노드 목록(매 탐색마다 노드 비용/parent 초기화에 사용).

return :
vector<AstarNode*> (start부터 destination까지의 경로 노드들)
if start == nullptr or destination == nullptr -> 빈 벡터 반환
if openList가 비어서도 destination에 도달 못함 -> 빈 벡터 반환(경로 없음)
if destination 도달 -> parent를 따라 역추적한 뒤 reverse하여 정상 순서 경로 반환

1) 모든 노드의 gCost를 무한대로, parent를 nullptr로 초기화한다(이전 탐색 흔적 제거).
2) start를 openList에 넣고 g=0, h=거리(휴리스틱), f=g+h를 계산한다.
3) openList에서 f가 가장 작은 노드를 current로 뽑는다.
4) current가 destination이면, current에서 parent를 따라가며 경로를 만들고 반환한다.
5) 아니면 current를 closedList로 옮긴다.
6) current의 이웃들을 가져오고, 더 좋은 경로(tentativeG)가 나오면 neighbor의 parent/g/h/f를 갱신한다.
7) openList가 빌 때까지 반복한다.
*/

//AI Code Start : <chatgpt>
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
//AI Code End : <chatgpt>

/*<설명>
AstarSearch로 만들어진 path에 맞춰 이동.
path[currentPathIndex] 노드 방향으로 이동한다.
목표 노드에 가까워지면 currentPathIndex++ 하여 다음 노드를 목표로 한다.

매변 :
deltaTime : speed*deltaTime로 이동량을 계산한다.

1) 현재 목표 노드 = path[currentPathIndex]를 잡는다.
2) 몬스터 위치에서 목표 노드 월드좌표(worldx, worldz)까지 방향 벡터(dir)를 만든 다.
3) 거의 도착했으면 다음 노드로 넘어간다.
4) 아니면 dir을 정규화하고, tickLVelocity = dir * m_speed * deltaTime로 이번 프레임 이동량을 만든다.
5) 바라보는 방향(look)을 dir로 맞춘다.
*/

//AI involved Code Start : <chatgpt>
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
	if (gameworld.AstarStartZ > target.z) target.z = gameworld.AstarStartZ;
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
//AI involved Code End : <chatgpt>

/*<설명>
- 월드 좌표(wx, wz)에 가장 가까운 "이동 가능 노드(cango==true)"를 찾아 반환한다.
- 월드 좌표는 그리드 정중앙이 아닐 수 있으므로, A*의 start/goal 노드를 잡기 위한 함수.

매변 :
wx : 월드 X 좌표.
wz : 월드 Z 좌표.
allNodes : 전체 노드 목록(가장 가까운 노드 찾기 위해 전체 순회).

return :
if 이동 가능 노드가 하나도 없으면 -> nullptr 반환
if 여러 후보가 있으면 -> 거리^2(dx^2+dz^2)가 최소인 노드 반환

1) allNodes를 전부 돌며 cango==true인 노드만 본다.
2) (node.worldx - wx)^2 + (node.worldz - wz)^2 를 계산해 가장 작은 노드를 갱신한다.
3) 최종적으로 가장 가까운 노드를 반환.
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
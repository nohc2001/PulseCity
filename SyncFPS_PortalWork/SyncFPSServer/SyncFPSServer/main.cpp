#define NOMINMAX
#include "stdafx.h"
#include "main.h"
#include "Zone.h"

World gameworld;
float DeltaTime = 0;
vector<Item> ItemTable;

int main(int argc, char** argv) {
	PrintOffset();

	int listenPort = 1001;
	int zoneCount = 2;  // ★ zoneId → zoneCount로 변경

	if (argc >= 2) listenPort = atoi(argv[1]);
	if (argc >= 3) zoneCount = atoi(argv[2]);  // ★ 존 개수

	Socket listenSocket(SocketType::Tcp);
	listenSocket.Bind(Endpoint("0.0.0.0", listenPort));
	listenSocket.SetReceiveBuffer(new twoPage, sizeof(twoPage));

	cout << "Server Start" << endl;

	ui64 ft, st;
	ui64 TimeStack = 0;
	st = GetTicks();
	listenSocket.SetNonblocking();
	vector<PollFD> readFds;
	listenSocket.Listen();
	double DeltaFlow = 0;
	constexpr double InvHZ = 1.0 / (double)QUERYPERFORMANCE_HZ;

	gameworld.Init(zoneCount);  // ★ 존 개수로 초기화

	while (true) {
		ft = GetTicks();
		DeltaFlow += (double)(ft - st) * InvHZ;
		if (DeltaFlow >= 0.016) {
			DeltaTime = (float)DeltaFlow;
			gameworld.Update();
			DeltaFlow = 0;
		}

		readFds.reserve(gameworld.clients.size + 1);
		readFds.clear();

		PollFD item2;
		item2.m_pollfd.events = POLLRDNORM;
		item2.m_pollfd.fd = listenSocket.m_fd;
		item2.m_pollfd.revents = 0;
		readFds.push_back(item2);

		for (int i = 0; i < gameworld.clients.size; ++i)
		{
			if (gameworld.clients.isnull(i)) continue;
			PollFD item;
			item.m_pollfd.events = POLLRDNORM;
			item.m_pollfd.fd = gameworld.clients[i].socket.m_fd;
			item.m_pollfd.revents = 0;
			readFds.push_back(item);
		}

		Poll(readFds.data(), (int)readFds.size(), 50);

		int num = 0;
		for (auto readFd : readFds)
		{
			if (readFd.m_pollfd.revents != 0)
			{
				if (num == 0) {
					Socket tcpConnection;
					string ignore;

					int newindex = gameworld.clients.Alloc();
					if (newindex >= 0) {
						listenSocket.Accept(gameworld.clients[newindex].socket, ignore);
						auto a = gameworld.clients[newindex].socket.GetPeerAddr().ToString();
						gameworld.clients[newindex].socket.SetNonblocking();
						if (gameworld.clients[newindex].socket.m_receiveBuffer == nullptr) {
							gameworld.clients[newindex].socket.SetReceiveBuffer(new twoPage, 4096 * 2);
						}

						int clientindex = newindex;

						// ★ 기본 존 설정 (0번 존에서 시작)
						int defaultZoneId = 0;
						gameworld.clients[clientindex].zoneId = defaultZoneId;

						//gameworld.SendingAllObjectForNewClient(defaultZoneId);
						gameworld.pack_factory.Clear();

						Player* p = new Player();
						p->clientIndex = clientindex;
						p->m_worldMatrix.Id();
						p->m_worldMatrix.pos.f3.y = 10;
						p->ShapeIndex = Shape::GetShapeIndex("Player");
						p->Inventory.reserve(36);
						p->Inventory.resize(36);
						for (int i = 0; i < 36; ++i) {
							p->Inventory[i].id = 0;
							p->Inventory[i].ItemCount = 0;
						}

						// ★ 존에 플레이어 추가
						int objindex = gameworld.NewPlayer(p, clientindex, defaultZoneId);
						gameworld.clients[clientindex].pObjData = p;
						gameworld.clients[clientindex].objindex = objindex;

						//int dcap = gameworld.Sending_AllocPlayerIndex(clientindex, objindex);
						//gameworld.pack_factory.push_data(dcap, gameworld.tempbuffer.data);

						//if (p->ShapeIndex >= 0) {
						//	int dcap = gameworld.Sending_SetMeshInGameObject(objindex, Shape::ShapeNameArr[p->ShapeIndex]);
						//	// ★ 같은 존 클라이언트에게만 전송
						//	gameworld.SendToZone_except(defaultZoneId, dcap, clientindex);
						//	gameworld.pack_factory.push_data(dcap, gameworld.tempbuffer.data);
						//}

						//gameworld.pack_factory.send(&gameworld.clients[clientindex].socket);

						cout << "Socket from " << a << " is accepted.\n";
						char newParticipantstr[128] = {};
						sprintf_s(newParticipantstr, "New client - %d\n", newindex);
						cout << newParticipantstr << endl;
					}
					else {
						cout << "too many client!!" << endl;
					}
				}
				else {
					int index = num - 1;
					if (gameworld.clients.isnull(index)) continue;
					int result = gameworld.clients[index].socket.Receive();
					if (result > 0) {
						char* cptr = gameworld.clients[index].socket.m_receiveBuffer;

					RECEIVE_LOOP:
						int offset = gameworld.Receiving(index, cptr);
						cptr += offset;
						result -= offset;
						if (result > 1) {
							goto RECEIVE_LOOP;
						}
					}
					else if (result <= 0) {
						cout << "client " << index << " Left the Game." << endl;

						// ★ 존에서 플레이어 제거
						int zoneId = gameworld.clients[index].zoneId;
						gameworld.zones[zoneId].RemovePlayer(index);

						delete gameworld.clients[index].pObjData;
						gameworld.clients[index].pObjData = nullptr;
						gameworld.clients.Free(index);
					}
				}
			}
			num += 1;
		}

		st = ft;
	}
	return 0;
}

//그리드 그리기 함수
void PrintCangoGrid(const std::vector<AstarNode*>& all, int gridWidth, int gridHeight)
{
	std::cout << "\n=== CANGO GRID (.: walkable, #: blocked) ===\n";
	for (int z = 0; z < gridHeight; ++z) {
		for (int x = 0; x < gridWidth; ++x) {
			const AstarNode* n = all[z * gridWidth + x];
			std::cout << (n && n->cango ? '.' : '#');
		}
		std::cout << '\n';
	}
	std::cout << std::flush;
}

//void World::SpawnPortal(int zoneId)
//{
//	Portal* portal1 = new Portal();
//	portal1->isExist = true;
//	portal1->m_worldMatrix.Id();
//
//	portal1->m_worldMatrix.pos = vec4(0.f, 1.f, 35.f, 1.f);
//	
//	portal1->ShapeIndex = Shape::GetShapeIndex("Portal");
//
//	// 도착지도 맵 내부로
//	portal1->spawnX = -35.f;
//	portal1->spawnY = 10.f;    // 낙하 후 착지하도록 여유
//	portal1->spawnZ = -35.f;
//	portal1->radius = 3.0f;
//	//portal1->dstPort = (zoneId == 1) ? 1002 : 1001;
//	portal1->dstzoneId = (zoneId == 1) ? 2 : 1;
//	//strcpy_s(portal1->dstIp, sizeof(portal1->dstIp), "127.0.0.1");
//	portal1->zoneId = zoneId;
//
//	int idx = NewObject(portal1, GameObjectType::_Portal);
//	int datacap = Sending_SetMeshInGameObject(idx, "Portal");
//
//	SendToAllClient(datacap);
//}

//void World::CheckPortalCollision(Player* p) {
//	if (!p) return;
//
//	int ci = p->clientIndex;
//	int currentZone = clients[ci].zoneId;
//	vec4 playerPos = p->m_worldMatrix.pos;
//
//	// 현재 존의 gameObjects에서만 검사
//	Zone& zone = zones[currentZone];
//
//	for (int i = 0; i < zone.gameObjects.size; ++i) {
//		if (zone.gameObjects.isnull(i)) continue;
//		GameObject* obj = zone.gameObjects[i];
//		if (!obj || !obj->isExist) continue;
//
//		void* vptr = *(void**)obj;
//		if (GameObjectType::VptrToTypeTable[vptr] != GameObjectType::_Portal)
//			continue;
//
//		Portal* portal = static_cast<Portal*>(obj);
//		vec4 portalPos = portal->m_worldMatrix.pos;
//
//		float dx = playerPos.x - portalPos.x;
//		float dy = playerPos.y - portalPos.y;
//		float dz = playerPos.z - portalPos.z;
//		float dist2 = dx * dx + dy * dy + dz * dz;
//		float r = portal->radius;
//
//		if (dist2 <= r * r) {
//			vec4 spawnPos(portal->spawnX, portal->spawnY, portal->spawnZ, 1.0f);
//
//			// 1) 클라이언트에게 존 변경 패킷 전송
//			SendPlayerDatatoOtherZone(ci, portal->dstzoneId, spawnPos);
//
//			// 2) 서버에서 존 이동 처리
//			MovePlayerToZone(ci, portal->dstzoneId, spawnPos);
//
//			return;
//		}
//	}
//}

void World::Init(int zoneCount) {
	ItemTable.push_back(Item(0)); // blank space in inventory.
	ItemTable.push_back(Item(1));
	ItemTable.push_back(Item(2));
	ItemTable.push_back(Item(3)); // test items. red, green, blue bullet mags.

	cout << "sizeof(GameObject) = " << sizeof(GameObject) << endl;
	cout << "sizeof(Player) = " << sizeof(Player) << endl;
	cout << "sizeof(Monster) = " << sizeof(Monster) << endl;
	cout << "sizeof(Portal) = " << sizeof(Portal) << endl;

	//astar grid init
	const int gridWidth = 80;
	const int gridHeight = 80;
	const float cellSize = 1.0f;

	const float offsetX = -gridWidth * cellSize * 0.5f;  // -40
	const float offsetZ = -gridHeight * cellSize * 0.5f; // -40

	GameObjectType::STATICINIT();

	clients.Init(32);
	pack_factory.Clear();

#ifdef _DEBUG //server / client
	AddClientOffset(GameObjectType::_GameObject, 16, 16); // isExist
	AddClientOffset(GameObjectType::_GameObject, 32, 32); // world Matrix
	AddClientOffset(GameObjectType::_GameObject, 80, 144); // pos

	AddClientOffset(GameObjectType::_Player, 16, 16); // isExist
	AddClientOffset(GameObjectType::_Player, 32, 32); // world Matrix
	AddClientOffset(GameObjectType::_Player, 64, 128); // pos

	AddClientOffset(GameObjectType::_Player, 128, 176); // ShootFlow
	AddClientOffset(GameObjectType::_Player, 132, 180); // recoilFlow
	AddClientOffset(GameObjectType::_Player, 136, 184); // HP
	AddClientOffset(GameObjectType::_Player, 140, 188); // MaxHP
	AddClientOffset(GameObjectType::_Player, 176, 224); // DeltaMousePos

	AddClientOffset(GameObjectType::_Player, 144, 192); // Bullets
	AddClientOffset(GameObjectType::_Player, 148, 196); // KillCount
	AddClientOffset(GameObjectType::_Player, 152, 200); // DeathCount
	AddClientOffset(GameObjectType::_Player, 156, 204); // HeatGauge
	AddClientOffset(GameObjectType::_Player, 160, 208); // MaxHeatGauge
	AddClientOffset(GameObjectType::_Player, 164, 212); // HealSkillCooldown
	AddClientOffset(GameObjectType::_Player, 168, 216); // HealSkillCooldownFlow

	AddClientOffset(GameObjectType::_Monster, 16, 16); // isExist
	AddClientOffset(GameObjectType::_Monster, 32, 32); // world Matrix
	AddClientOffset(GameObjectType::_Monster, 80, 144); // pos
	AddClientOffset(GameObjectType::_Monster, 210, 176); // isDead
	AddClientOffset(GameObjectType::_Monster, 184, 180); // HP
	AddClientOffset(GameObjectType::_Monster, 188, 184); // MaxHP

	AddClientOffset(GameObjectType::_Portal, 16, 16);    // isExist
	AddClientOffset(GameObjectType::_Portal, 32, 32);    // world Matrix
	AddClientOffset(GameObjectType::_Portal, 80, 144);   // pos (worldMatrix.pos)
	AddClientOffset(GameObjectType::_Portal, 128, 176);  // dstIp
	AddClientOffset(GameObjectType::_Portal, 144, 192);  // dstPort
	AddClientOffset(GameObjectType::_Portal, 148, 196);  // spawnX
	AddClientOffset(GameObjectType::_Portal, 152, 200);  // spawnY
	AddClientOffset(GameObjectType::_Portal, 156, 204);  // spawnZ
	AddClientOffset(GameObjectType::_Portal, 160, 208);  // radius
	AddClientOffset(GameObjectType::_Portal, 164, 212);  // zoneId

#else
	AddClientOffset(GameObjectType::_GameObject, 16, 16); // isExist
	AddClientOffset(GameObjectType::_GameObject, 32, 32); // world Matrix
	AddClientOffset(GameObjectType::_GameObject, 80, 144); // pos

	AddClientOffset(GameObjectType::_Player, 16, 16); // isExist
	AddClientOffset(GameObjectType::_Player, 32, 32); // world Matrix
	AddClientOffset(GameObjectType::_Player, 64, 128); // pos

	AddClientOffset(GameObjectType::_Player, 128, 176); // ShootFlow
	AddClientOffset(GameObjectType::_Player, 132, 180); // recoilFlow
	AddClientOffset(GameObjectType::_Player, 136, 184); // HP
	AddClientOffset(GameObjectType::_Player, 140, 188); // MaxHP
	AddClientOffset(GameObjectType::_Player, 176, 224); // DeltaMousePos

	AddClientOffset(GameObjectType::_Player, 144, 192); // Bullets
	AddClientOffset(GameObjectType::_Player, 148, 196); // KillCount
	AddClientOffset(GameObjectType::_Player, 152, 200); // DeathCount
	AddClientOffset(GameObjectType::_Player, 156, 204); // HeatGauge
	AddClientOffset(GameObjectType::_Player, 160, 208); // MaxHeatGauge
	AddClientOffset(GameObjectType::_Player, 164, 212); // HealSkillCooldown
	AddClientOffset(GameObjectType::_Player, 168, 216); // HealSkillCooldownFlow

	AddClientOffset(GameObjectType::_Monster, 16, 16); // isExist
	AddClientOffset(GameObjectType::_Monster, 32, 32); // world Matrix
	AddClientOffset(GameObjectType::_Monster, 80, 144); // pos
	AddClientOffset(GameObjectType::_Monster, 210, 176); // isDead
	AddClientOffset(GameObjectType::_Monster, 184, 180); // HP
	AddClientOffset(GameObjectType::_Monster, 188, 184); // MaxHP

	AddClientOffset(GameObjectType::_Portal, 16, 16);    // isExist
	AddClientOffset(GameObjectType::_Portal, 32, 32);    // world Matrix
	AddClientOffset(GameObjectType::_Portal, 80, 144);   // pos (worldMatrix.pos)
	AddClientOffset(GameObjectType::_Portal, 128, 176);  // dstIp
	AddClientOffset(GameObjectType::_Portal, 144, 192);  // dstPort
	AddClientOffset(GameObjectType::_Portal, 148, 196);  // spawnX
	AddClientOffset(GameObjectType::_Portal, 152, 200);  // spawnY
	AddClientOffset(GameObjectType::_Portal, 156, 204);  // spawnZ
	AddClientOffset(GameObjectType::_Portal, 160, 208);  // radius
	AddClientOffset(GameObjectType::_Portal, 164, 212);  // zoneId
	AddClientOffset(GameObjectType::_Portal, 168, 216);  // dstzoneId

#endif


	// Mesh/Shape 로드 (전역, 한 번만)
	Mesh* MyMesh = new Mesh();
	MyMesh->ReadMeshFromFile_OBJ("Resources/Mesh/PlayerMesh.obj");
	Shape::AddMesh("Player", MyMesh);

	Mesh* MyGroundMesh = new Mesh();
	MyGroundMesh->CreateWallMesh(40.0f, 0.5f, 40.0f);
	Shape::AddMesh("Ground001", MyGroundMesh);

	Mesh* MyWallMesh = new Mesh();
	MyWallMesh->CreateWallMesh(5.0f, 2.0f, 1.0f);
	Shape::AddMesh("Wall001", MyWallMesh);

	Mesh* MyPortalMesh = new Mesh();
	MyPortalMesh->CreateWallMesh(5.0f, 8.0f, 0.5f);
	Shape::AddMesh("Portal", MyPortalMesh);

	Mesh* MyMonsterMesh = new Mesh();
	MyMonsterMesh->ReadMeshFromFile_OBJ("Resources/Mesh/PlayerMesh.obj");
	Shape::AddMesh("Monster001", MyMonsterMesh);

	// ★ 존 생성
	for (int i = 0; i < zoneCount; ++i) {
		zones.emplace_back(this, i);   
	}

	// ★ 각 존 초기화
	for (int i = 0; i < (int)zones.size(); ++i) {
		zones[i].Init();
	}

	cout << "World Init end (zones: " << zoneCount << ")" << endl;
	cout << "sizeof(Portal) = " << sizeof(Portal) << endl;
	cout << "ServerSizeof[Portal] = " << GameObjectType::ServerSizeof[GameObjectType::_Portal] << endl;

	//matrix mat;
	//mat.Id();
	//for (int i = 0; i < 8; ++i) {
	//	int r = rand() % 3;
	//	float rot = 3.141592f / 4.0f;
	//	matrix trmat = XMMatrixRotationRollPitchYaw(rot * mat.mat.r[r].m128_f32[0], rot * mat.mat.r[r].m128_f32[1], rot * mat.mat.r[r].m128_f32[2]);
	//	GameObject* wallObject4 = new GameObject();
	//	wallObject4->MeshIndex = Mesh::GetMeshIndex("Wall001");
	//	wallObject4->mesh = *(MyWallMesh);
	//	wallObject4->m_worldMatrix = (XMMatrixMultiply(trmat, XMMatrixTranslation(rand() % 80 - 40, 0.0f, rand() % 80 - 40)));
	//	newindex = NewObject(wallObject4, GameObjectType::_GameObject);
	//	datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
	//	SendToAllClient(datacap);
	//}

	/*Monster* myMonster_2 = new Monster();
	myMonster_2->MeshIndex = Mesh::GetMeshIndex("Monster001");
	myMonster_2->mesh = *(MyMonsterMesh);
	myMonster_2->Init(XMMatrixTranslation(-5.0f, 0.5f, -2.5f));
	newindex = NewObject(myMonster_2, GameObjectType::_Monster);
	datacap = Sending_SetMeshInGameObject(newindex, "Monster001");
	SendToAllClient(datacap);

	Monster* myMonster_3 = new Monster();
	myMonster_3->MeshIndex = Mesh::GetMeshIndex("Monster001");
	myMonster_3->mesh = *(MyMonsterMesh);
	myMonster_3->Init(XMMatrixTranslation(5.0f, 0.5f, -2.5f));
	newindex = NewObject(myMonster_3, GameObjectType::_Monster);
	datacap = Sending_SetMeshInGameObject(newindex, "Monster001");
	SendToAllClient(datacap);*/

	//그리드(노드)의 cango를 체크하는 함수
	//gridcollisioncheck();

	const int GRID_W = 80;
	const int GRID_H = 80;

	//PrintCangoSummary(allnodes, GRID_W, GRID_H);
	//PrintCangoGrid(allnodes, GRID_W, GRID_H);

	cout << "Game Init end" << endl;
}

void World::Update() {
	lowFrequencyFlow += DeltaTime;
	midFrequencyFlow += DeltaTime;
	highFrequencyFlow += DeltaTime;

	// ★ 모든 존 업데이트
	for (Zone& zone : zones) {
		zone.Update(DeltaTime);
	}

	if (lowHit()) lowFrequencyFlow = 0;
	if (midHit()) midFrequencyFlow = 0;
	if (highHit()) highFrequencyFlow = 0;
}

//void World::gridcollisioncheck()
//{
//	const float radius = 0.2f;
//	const float groundY = 0.0f;
//
//	for (AstarNode* node : allnodes)
//	{
//		vec4 nodePos(node->worldx, groundY, node->worldz);
//		collisionchecksphere sphere = { nodePos, radius };
//
//		bool blocked = false;
//
//		// 모든 오브젝트 중 "Wall001" 메쉬만 검사
//		for (int i = 0; i < gameObjects.size; ++i)
//		{
//			if (gameObjects.isnull(i)) continue;
//			GameObject* obj = gameObjects[i];
//
//			// 벽 메쉬 식별
//			if (obj->ShapeIndex == Shape::GetShapeIndex("Wall001"))
//			{
//				vec4 wallCenter(obj->m_worldMatrix.pos.x,
//					obj->m_worldMatrix.pos.y,
//					obj->m_worldMatrix.pos.z);
//
//				// Wall001이 5x2x1로 생성되었다면 절반 크기 = {2.5, 1.0, 0.5}
//				vec4 wallHalfSize(2.5f, 1.0f, 0.5f);
//
//				if (CheckAABBSphereCollision(wallCenter, wallHalfSize, sphere))
//				{
//					blocked = true;
//					break;
//				}
//			}
//		}
//		node->cango = !blocked;
//	}
//}

int World::Receiving(int clientIndex, char* rBuffer) {
	ClientData& client = clients[clientIndex];
	ui64 putv = 0;
	if (rBuffer[1] == 'D') {
		putv = 1;
	}
	else if (rBuffer[1] == 'U') {
		putv = 0;
	}

	if (rBuffer[0] != InputID::MouseMove) {
		BitBoolArr_ui64* bit = &client.pObjData->InputBuffer[rBuffer[0]];
		*bit = putv;
		//cout << *bit->data << endl;
		//dbglog1a("key ui64 : %d \n", bit->data);
		cout << "client " << clientIndex << " Input : " << rBuffer[0] << rBuffer[1] << endl;
		return 2;
	}
	else {
		int xdelta = *(int*)&rBuffer[1];
		int ydelta = *(int*)&rBuffer[5];

		client.pObjData->DeltaMousePos.x += xdelta;
		client.pObjData->DeltaMousePos.y += ydelta;

		if (client.pObjData->DeltaMousePos.y > 200) {
			client.pObjData->DeltaMousePos.y = 200;
		}
		if (client.pObjData->DeltaMousePos.y < -200) {
			client.pObjData->DeltaMousePos.y = -200;
		}

		return 9;
	}
}

//int World::NewObject(GameObject* obj, GameObjectType gotype)
//{
//	int newindex = gameObjects.Alloc();
//	gameObjects[newindex] = obj;
//	obj->isExist = true;
//
//	switch (gotype) {
//	case GameObjectType::_GameObject:
//	{
//		int datacap = Sending_NewGameObject(newindex, obj, gotype);
//		for (int i = 0; i < clients.size; ++i) {
//			if (clients.isnull(i)) continue;
//			clients[i].socket.Send((char*)tempbuffer.data, datacap);
//		}
//		break;
//	}
//	case GameObjectType::_Player:
//	{
//		int datacap = Sending_NewGameObject(newindex, obj, gotype);
//		gameworld.SendToAllClient(datacap);
//
//		char* member = (char*)obj + sizeof(GameObject);
//		int len = 32;
//		datacap = Sending_ChangeGameObjectMember(newindex, obj, gotype, member, len);
//		gameworld.SendToAllClient(datacap);
//
//		datacap = Sending_ChangeGameObjectMember(newindex, obj, gotype, &((Player*)obj)->bullets, sizeof(int));
//		gameworld.SendToAllClient(datacap);
//
//		datacap = Sending_ChangeGameObjectMember(newindex, obj, gotype, &((Player*)obj)->KillCount, sizeof(int));
//		gameworld.SendToAllClient(datacap);
//
//		datacap = Sending_ChangeGameObjectMember(newindex, obj, gotype, &((Player*)obj)->DeathCount, sizeof(int));
//		gameworld.SendToAllClient(datacap);
//		break;
//	}
//	case GameObjectType::_Monster:
//	{
//		int datacap = Sending_NewGameObject(newindex, obj, gotype);
//		gameworld.SendToAllClient(datacap);
//
//		datacap = Sending_ChangeGameObjectMember(newindex, obj, gotype, &((Monster*)obj)->isDead, sizeof(bool));
//		gameworld.SendToAllClient(datacap);
//
//		datacap = Sending_ChangeGameObjectMember(newindex, obj, gotype, &((Monster*)obj)->HP, sizeof(float));
//		gameworld.SendToAllClient(datacap);
//		break;
//	}
//	}
//	return newindex;
//}

// World::NewPlayer() - pack_factory만 담당
int World::NewPlayer(Player* obj, int clientIndex, int zoneId)
{
	// 1) 존에 플레이어 추가 (다른 클라이언트에게 전송 + 새 클라이언트에게 존 데이터 전송)
	int newindex = zones[zoneId].AddPlayer(clientIndex, obj, obj->m_worldMatrix.pos);

	//// 2) pack_factory에 자기 자신 정보 추가
	//int datacap = Sending_NewGameObject(newindex, obj, GameObjectType::_Player);
	//pack_factory.push_data(datacap, tempbuffer.data);

	//if (obj->ShapeIndex >= 0) {
	//	datacap = Sending_SetMeshInGameObject(newindex, Shape::ShapeNameArr[obj->ShapeIndex]);
	//	pack_factory.push_data(datacap, tempbuffer.data);
	//}

	return newindex;
}

int World::Sending_NewGameObject(int newindex, GameObject* newobj, GameObjectType gotype) {
	int datacap = GameObjectType::ServerSizeof[gotype] + 8;
	SendingType st = SendingType::NewGameObject;
	memcpy(tempbuffer.data, &st, 2);
	memcpy(tempbuffer.data + 2, &newindex, 4);
	memcpy(tempbuffer.data + 6, &gotype, 2);

	//char* cptr = tempbuffer.data + 8;
	//for (int i = 0; i < GameObjectType::Sizeof[gotype]; ++i) {
	//	cptr[i] = ((char*)newobj)[i];
	//}
	memcpy(tempbuffer.data + 8, (char*)newobj, GameObjectType::ServerSizeof[gotype]);
	return datacap;
}

int World::Sending_ChangeGameObjectMember(int objindex, GameObject* ptrobj, GameObjectType gotype, void* memberAddr, int memberSize) {
	int datacap = memberSize + 12; // sendingtype + gameobjecttype + clientMemberoffset + membersize = 2*4 = 8
	SendingType st = SendingType::ChangeMemberOfGameObject;
	memcpy(tempbuffer.data, &st, 2);
	memcpy(tempbuffer.data + 2, &objindex, 4);
	memcpy(tempbuffer.data + 6, &gotype, 2);
	short serverOffset = (char*)memberAddr - (char*)ptrobj;

	short serverReleaseOffset = serverOffset;

	//#ifdef _DEBUG
	//	serverReleaseOffset -= 16;
	//#endif

	short clientOffset = GameObjectType::GetClientOffset[type_offset_pair(gotype, serverReleaseOffset)];
	if (clientOffset == 0) clientOffset = serverOffset; // temp..

	memcpy(tempbuffer.data + 8, &clientOffset, 2);
	memcpy(tempbuffer.data + 10, &memberSize, 2);
	memcpy(tempbuffer.data + 12, memberAddr, memberSize);
	return datacap;
	//clients[clientIndex].socket.Send((char*)tempbuffer.data, datacap);
}

void World::SendPlayerDatatoOtherServer(int clientIndex, const char* ip, int portnum, int dstzoneid, const vec4& spawnPos)
{
	if (clients.isnull(clientIndex)) return;

	SendingType st = SendingType::ChangeServer;

	// st(2) + ip(16) + port(4) + dstzoneid(4) + spawnPos(12) = 34
	int datacap = 2 + 16 + 4 + 4 + 12;

	memcpy(tempbuffer.data, &st, 2);

	char ipbuf[16] = {};
	strncpy_s(ipbuf, ip, 15);
	memcpy(tempbuffer.data + 2, ipbuf, 16);
	memcpy(tempbuffer.data + 18, &portnum, 4);
	memcpy(tempbuffer.data + 22, &dstzoneid, 4);
	memcpy(tempbuffer.data + 26, &spawnPos.f3, 12);

	clients[clientIndex].socket.Send((char*)tempbuffer.data, datacap);
}

void World::SendPlayerDatatoOtherZone(int clientIndex, int dstzoneId, const vec4& spawnPos)
{
	if (clients.isnull(clientIndex)) return;

	SendingType st = SendingType::ChangeMap;

	// st(2) + dstzoneId(4) + spawnPos(12) = 18 bytes
	int datacap = 2 + 4 + 12;

	memcpy(tempbuffer.data, &st, 2);
	memcpy(tempbuffer.data + 2, &dstzoneId, 4);
	memcpy(tempbuffer.data + 6, &spawnPos.f3, 12);

	// 소켓을 끊지 않고 그대로 전송
	clients[clientIndex].socket.Send((char*)tempbuffer.data, datacap);
}

int World::Sending_NewRay(vec4 rayStart, vec4 rayDirection, float rayDistance) {
	SendingType st = SendingType::NewRay;
	int datacap = sizeof(float) * 7 + 2;
	memcpy(tempbuffer.data, &st, 2);
	memcpy(tempbuffer.data + 2, &rayStart.f3, 12);
	memcpy(tempbuffer.data + 14, &rayDirection.f3, 12);
	memcpy(tempbuffer.data + 26, &rayDistance, 4);
	return datacap;
	//clients[clientIndex].socket.Send((char*)tempbuffer.data, datacap);
}

int World::Sending_SetMeshInGameObject(int objindex, string str)
{
	SendingType st = SendingType::SetMeshInGameObject;
	int datacap = 10 + str.size();
	ui32 len = str.size();
	memcpy(tempbuffer.data, &st, 2);
	memcpy(tempbuffer.data + 2, &objindex, 4);
	memcpy(tempbuffer.data + 6, &len, 4);
	memcpy(tempbuffer.data + 10, &str[0], str.size());
	return datacap;
	//clients[clientIndex].socket.Send((char*)tempbuffer.data, datacap);
}

int World::Sending_AllocPlayerIndex(int clientindex, int objindex)
{
	SendingType st = SendingType::AllocPlayerIndexes;
	int datacap = 10;
	memcpy(tempbuffer.data, &st, 2);
	memcpy(tempbuffer.data + 2, &clientindex, 4);
	memcpy(tempbuffer.data + 6, &objindex, 4);
	return datacap;
}

int World::Sending_DeleteGameObject(int objindex)
{
	SendingType st = SendingType::DeleteGameObject;
	int datacap = 6;
	memcpy(tempbuffer.data, &st, 2);
	memcpy(tempbuffer.data + 2, &objindex, 4);
	return datacap;
}

int World::Sending_ItemDrop(int dropindex, ItemLoot lootdata)
{
	SendingType st = SendingType::ItemDrop;
	int datacap = 2 + sizeof(int) + sizeof(ItemLoot);
	memcpy(tempbuffer.data, &st, 2);
	memcpy(tempbuffer.data + 2, &dropindex, sizeof(int));
	memcpy(tempbuffer.data + 6, &lootdata, sizeof(ItemLoot));
	return datacap;
}

int World::Sending_ItemRemove(int dropindex)
{
	SendingType st = SendingType::ItemDropRemove;
	int datacap = 2 + sizeof(int);
	memcpy(tempbuffer.data, &st, 2);
	memcpy(tempbuffer.data + 2, &dropindex, sizeof(int));
	return datacap;
}

int World::Sending_InventoryItemSync(ItemStack lootdata, int inventoryIndex)
{
	SendingType st = SendingType::InventoryItemSync;
	int datacap = 2 + sizeof(ItemStack) + sizeof(int);
	memcpy(tempbuffer.data, &st, 2);
	memcpy(tempbuffer.data + 2, &lootdata, sizeof(ItemStack));
	memcpy(tempbuffer.data + 2 + sizeof(ItemStack), &inventoryIndex, sizeof(int));
	return datacap;
}

void World::SendingAllObjectForNewClient(int zoneId)
{
	{
		pack_factory.Clear();
		Zone& zone = zones[zoneId];

		for (int i = 0; i < zone.gameObjects.size; ++i) {
			if (zone.gameObjects.isnull(i)) continue;

			void* vptr = *(void**)zone.gameObjects[i];
			GameObjectType gotype = GameObjectType::VptrToTypeTable[vptr];

			int datacap = Sending_NewGameObject(i, zone.gameObjects[i], gotype);
			pack_factory.push_data(datacap, tempbuffer.data);

			if (zone.gameObjects[i]->ShapeIndex >= 0) {
				datacap = Sending_SetMeshInGameObject(i, Shape::ShapeNameArr[zone.gameObjects[i]->ShapeIndex]);
				pack_factory.push_data(datacap, tempbuffer.data);
			}

			if (gotype == GameObjectType::_Monster) {
				Monster* mon = (Monster*)zone.gameObjects[i];
				datacap = Sending_ChangeGameObjectMember(i, mon, GameObjectType::_Monster, &mon->isDead, sizeof(bool));
				pack_factory.push_data(datacap, tempbuffer.data);

				datacap = Sending_ChangeGameObjectMember(i, mon, GameObjectType::_Monster, &mon->HP, sizeof(float));
				pack_factory.push_data(datacap, tempbuffer.data);

				datacap = Sending_ChangeGameObjectMember(i, mon, GameObjectType::_Monster, &mon->MaxHP, sizeof(float));
				pack_factory.push_data(datacap, tempbuffer.data);
			}
			else if (gotype == GameObjectType::_Player) {
				Player* p = (Player*)zone.gameObjects[i];
				datacap = Sending_ChangeGameObjectMember(i, p, GameObjectType::_Player, &p->KillCount, sizeof(int));
				pack_factory.push_data(datacap, tempbuffer.data);

				datacap = Sending_ChangeGameObjectMember(i, p, GameObjectType::_Player, &p->DeathCount, sizeof(int));
				pack_factory.push_data(datacap, tempbuffer.data);

				datacap = Sending_ChangeGameObjectMember(i, p, GameObjectType::_Player, &p->bullets, sizeof(int));
				pack_factory.push_data(datacap, tempbuffer.data);
			}
		}

		for (int i = 0; i < zone.DropedItems.size; ++i) {
			if (zone.DropedItems.isnull(i)) continue;
			int datacap = Sending_ItemDrop(i, zone.DropedItems[i]);
			pack_factory.push_data(datacap, tempbuffer.data);
		}
	}
}

//void World::DestroyObject(int objindex)
//{
//	if (gameObjects.isnull(objindex)) return;
//
//	int datacap = Sending_DeleteGameObject(objindex);
//	SendToAllClient(datacap);
//
//	delete gameObjects[objindex];
//	gameObjects.Free(objindex);
//}
//int World::

//should i separate player delete and gameobject delete?

//void World::FireRaycast(GameObject* shooter, vec4 rayStart, vec4 rayDirection, float rayDistance)
//{
//	vec4 rayOrigin = rayStart;
//
//	GameObject* closestHitObject = nullptr;
//
//	float closestDistance = rayDistance;
//
//	int lastcurrentindex = gameworld.currentIndex;
//
//	for (int i = 0; i < gameObjects.size; ++i) {
//		if (gameObjects.isnull(i) || shooter == gameObjects[i]) continue;
//		gameworld.currentIndex = i;
//		BoundingOrientedBox obb = gameObjects[i]->GetOBB();
//		float distance;
//
//		if (obb.Intersects(rayOrigin, rayDirection, distance)) {
//			if (distance <= rayDistance) {
//				if (distance < closestDistance) {
//					closestDistance = distance;
//					closestHitObject = gameObjects[i];
//
//					gameObjects[i]->OnCollisionRayWithBullet(shooter);
//				}
//			}
//		}
//	}
//
//	gameworld.currentIndex = lastcurrentindex;
//	int datacap = Sending_NewRay(rayStart, rayDirection, closestDistance);
//	gameworld.SendToAllClient(datacap);
//}

void PrintOffset() {
	{
		int n = sizeof(GameObject);
		dbglog1(L"class GameObject : size = %d\n", n);
		GameObject temp;

		n = (char*)&temp - (char*)&temp.isExist;
		dbglog1(L"class GameObject.isExist%d\n", n);
		/*n = (char*)&temp - (char*)&temp.diffuseTextureIndex;
		dbglog1(L"class GameObject.diffuseTextureIndex%d\n", n);*/
		n = (char*)&temp - (char*)&temp.m_worldMatrix;
		dbglog1(L"class GameObject.m_worldMatrix%d\n", n);
		n = (char*)&temp - (char*)&temp.LVelocity;
		dbglog1(L"class GameObject.LVelocity%d\n", n);
		n = (char*)&temp - (char*)&temp.tickLVelocity;
		dbglog1(L"class GameObject.tickLVelocity%d\n", n);

		/*n = (char*)&temp - (char*)&temp.m_pMesh;
		dbglog1(L"class GameObject.m_pMesh%d\n", n);
		n = (char*)&temp - (char*)&temp.m_pShader;
		dbglog1(L"class GameObject.m_pShader%d\n", n);
		n = (char*)&temp - (char*)&temp.Destpos;
		dbglog1(L"class GameObject.Destpos%d\n", n);*/
	}
	dbglog1(L"-----------------------------------%d\n\n", rand());
	{
		int n = sizeof(Player);
		dbglog1(L"class Player : size = %d\n", n);
		Player temp;

		n = (char*)&temp - (char*)&temp.isExist;
		dbglog1(L"class Player.isExist%d\n", n);
		/*n = (char*)&temp - (char*)&temp.diffuseTextureIndex;
		dbglog1(L"class Player.diffuseTextureIndex%d\n", n);*/
		n = (char*)&temp - (char*)&temp.m_worldMatrix;
		dbglog1(L"class Player.m_worldMatrix%d\n", n);
		n = (char*)&temp - (char*)&temp.LVelocity;
		dbglog1(L"class Player.LVelocity%d\n", n);
		n = (char*)&temp - (char*)&temp.tickLVelocity;
		dbglog1(L"class Player.tickLVelocity%d\n", n);
		//n = (char*)&temp - (char*)&temp.m_pMesh;
		//dbglog1(L"class Player.m_pMesh%d\n", n);
		//n = (char*)&temp - (char*)&temp.m_pShader;
		//dbglog1(L"class Player.m_pShader%d\n", n);
		//n = (char*)&temp - (char*)&temp.Destpos;
		//dbglog1(L"class Player.Destpos%d\n", n);
		dbglog1(L"-----------------------%d\n", rand());
		n = (char*)&temp - (char*)&temp.ShootFlow;
		dbglog1(L"class Player.ShootFlow%d\n", n);
		n = (char*)&temp - (char*)&temp.recoilFlow;
		dbglog1(L"class Player.recoilFlow%d\n", n);
		n = (char*)&temp - (char*)&temp.HP;
		dbglog1(L"class Player.HP%d\n", n);
		n = (char*)&temp - (char*)&temp.MaxHP;
		dbglog1(L"class Player.MaxHP%d\n", n);
		n = (char*)&temp - (char*)&temp.DeltaMousePos;
		dbglog1(L"class Player.DeltaMousePos%d\n", n);
		n = (char*)&temp - (char*)&temp.bullets;
		dbglog1(L"class Player.bullets%d\n", n);
		n = (char*)&temp - (char*)&temp.KillCount;
		dbglog1(L"class Player.KillCount%d\n", n);
		n = (char*)&temp - (char*)&temp.DeathCount;
		dbglog1(L"class Player.DeathCount%d\n", n);
		n = (char*)&temp - (char*)&temp.HeatGauge;
		dbglog1(L"class Player.Heat%d\n", n);
		n = (char*)&temp - (char*)&temp.MaxHeatGauge;
		dbglog1(L"class Player.MaxHeat%d\n", n);
		n = (char*)&temp - (char*)&temp.HealSkillCooldown;
		dbglog1(L"class Player.cooldown%d\n", n);
		n = (char*)&temp - (char*)&temp.HealSkillCooldownFlow;
		dbglog1(L"class Player.cooldownflow%d\n", n);

	}
	dbglog1(L"-----------------------------------%d\n\n", rand());
	{
		int n = sizeof(Monster);
		dbglog1(L"class Monster : size = %d\n", n);
		Monster temp;

		n = (char*)&temp - (char*)&temp.isExist;
		dbglog1(L"class Monster.isExist%d\n", n);
		/*n = (char*)&temp - (char*)&temp.diffuseTextureIndex;
		dbglog1(L"class Monster.diffuseTextureIndex%d\n", n);*/
		n = (char*)&temp - (char*)&temp.m_worldMatrix;
		dbglog1(L"class Monster.m_worldMatrix%d\n", n);
		n = (char*)&temp - (char*)&temp.LVelocity;
		dbglog1(L"class Monster.LVelocity%d\n", n);
		n = (char*)&temp - (char*)&temp.tickLVelocity;
		dbglog1(L"class Monster.tickLVelocity%d\n", n);
		n = (char*)&temp - (char*)&temp.isDead;
		dbglog1(L"class Monster.isDead%d\n", n);
		n = (char*)&temp - (char*)&temp.HP;
		dbglog1(L"class Monster.HP%d\n", n);
		n = (char*)&temp - (char*)&temp.MaxHP;
		dbglog1(L"class Monster.MaxHP%d\n", n);
		/*n = (char*)&temp - (char*)&temp.m_pMesh;
		dbglog1(L"class Monster.m_pMesh%d\n", n);
		n = (char*)&temp - (char*)&temp.m_pShader;
		dbglog1(L"class Monster.m_pShader%d\n", n);
		n = (char*)&temp - (char*)&temp.Destpos;
		dbglog1(L"class Monster.Destpos%d\n", n);*/
	}
	dbglog1(L"-----------------------------------%d\n\n", rand());
	{
		int n = sizeof(Portal);
		dbglog1(L"class Portal : size = %d\n", n);
		Portal temp;

		n = (char*)&temp - (char*)&temp.isExist;
		dbglog1(L"class Portal.isExist%d\n", n);
		n = (char*)&temp - (char*)&temp.m_worldMatrix;
		dbglog1(L"class Portal.m_worldMatrix%d\n", n);
		n = (char*)&temp - (char*)&temp.LVelocity;
		dbglog1(L"class Portal.LVelocity%d\n", n);
		n = (char*)&temp - (char*)&temp.tickLVelocity;
		dbglog1(L"class Portal.tickLVelocity%d\n", n);
		/*n = (char*)&temp - (char*)&temp.dstIp;
		dbglog1(L"class Portal.dstIp%d\n", n);
		n = (char*)&temp - (char*)&temp.dstPort;*/
		dbglog1(L"class Portal.dstPort%d\n", n);
		n = (char*)&temp - (char*)&temp.spawnX;
		dbglog1(L"class Portal.spawnX%d\n", n);
		n = (char*)&temp - (char*)&temp.spawnY;
		dbglog1(L"class Portal.spawnY%d\n", n);
		n = (char*)&temp - (char*)&temp.spawnZ;
		dbglog1(L"class Portal.spawnZ%d\n", n);
		n = (char*)&temp - (char*)&temp.radius;
		dbglog1(L"class Portal.radius%d\n", n);
		n = (char*)&temp - (char*)&temp.zoneId;
		dbglog1(L"class Portal.zoneId%d\n", n);
		n = (char*)&temp - (char*)&temp.dstzoneId;
		dbglog1(L"class Portal.dstzoneId%d\n", n);

	}
}

bool CheckAABBSphereCollision(const vec4& boxCenter, const vec4& boxHalfSize, const collisionchecksphere& sphere)
{
	float x = std::max(boxCenter.x - boxHalfSize.x, std::min(sphere.center.x, boxCenter.x + boxHalfSize.x));
	float y = std::max(boxCenter.y - boxHalfSize.y, std::min(sphere.center.y, boxCenter.y + boxHalfSize.y));
	float z = std::max(boxCenter.z - boxHalfSize.z, std::min(sphere.center.z, boxCenter.z + boxHalfSize.z));

	float dx = sphere.center.x - x;
	float dy = sphere.center.y - y;
	float dz = sphere.center.z - z;

	float dist2 = dx * dx + dy * dy + dz * dz;
	return dist2 < (sphere.radius * sphere.radius);
}

//void World::SendZoneDataToClient(int clientIndex, int zoneId)
//{
//	Zone& zone = zones[zoneId];
//
//	for (int i = 0; i < zone.gameObjects.size; ++i) {
//		if (zone.gameObjects.isnull(i)) continue;
//		void* vptr = *(void**)zone.gameObjects[i];
//		int datacap = Sending_NewGameObject(i, zone.gameObjects[i], GameObjectType::VptrToTypeTable[vptr]);
//		clients[clientIndex].socket.Send((char*)tempbuffer.data, datacap);
//	}
//}

//void World::SendToZone(int zoneId, int datacap) {
//	for (int i = 0; i < clients.size; ++i) {
//		if (clients.isnull(i)) continue;
//		if (clients[i].zoneId != zoneId) continue;
//		clients[i].socket.Send((char*)tempbuffer.data, datacap);
//	}
//}
//
//void World::SendToZone_except(int zoneId, int datacap, int except) {
//	for (int i = 0; i < clients.size; ++i) {
//		if (clients.isnull(i)) continue;
//		if (i == except) continue;
//		if (clients[i].zoneId != zoneId) continue;
//		clients[i].socket.Send((char*)tempbuffer.data, datacap);
//	}
//}

void World::MovePlayerToZone(int clientIndex, int toZoneId, vec4 spawnPos)
{
	cout << "[MoveToZone] client=" << clientIndex
		<< " from=" << clients[clientIndex].zoneId
		<< " to=" << toZoneId
		<< " objIdx=" << clients[clientIndex].objindex << endl;

	if (clients.isnull(clientIndex)) return;

	int fromZoneId = clients[clientIndex].zoneId;
	Player* player = clients[clientIndex].pObjData;
	int oldObjIdx = clients[clientIndex].objindex;

	// 1) 기존 존의 클라이언트들에게 삭제 알림
	int dcap = Sending_DeleteGameObject(oldObjIdx);
	SendToZone_except(fromZoneId, dcap, clientIndex);

	// 2) 기존 존에서 오브젝트 제거
	zones[fromZoneId].gameObjects[oldObjIdx] = nullptr;
	zones[fromZoneId].gameObjects.Free(oldObjIdx);

	// 3) 새 존에 오브젝트 추가
	int newObjIdx = zones[toZoneId].gameObjects.Alloc();
	zones[toZoneId].gameObjects[newObjIdx] = player;

	// 4) 플레이어 위치 업데이트
	player->m_worldMatrix.pos = spawnPos;

	// 5) 클라이언트 정보 업데이트
	clients[clientIndex].zoneId = toZoneId;
	clients[clientIndex].objindex = newObjIdx;

	// 6) 새 존의 클라이언트들에게 새 플레이어 알림
	dcap = Sending_NewGameObject(newObjIdx, player, GameObjectType::_Player);
	SendToZone_except(toZoneId, dcap, clientIndex);

	if (player->ShapeIndex >= 0) {
		dcap = Sending_SetMeshInGameObject(newObjIdx, Shape::ShapeNameArr[player->ShapeIndex]);
		SendToZone_except(toZoneId, dcap, clientIndex);
	}

	cout << "[MoveToZone] sending pack_factory to client=" << clientIndex << endl;

	// 7) 새 플레이어에게 새 존의 모든 오브젝트 전송
	SendingAllObjectForNewClient(toZoneId);
	int dcap2 = Sending_AllocPlayerIndex(clientIndex, newObjIdx);
	pack_factory.push_data(dcap2, tempbuffer.data);
	pack_factory.send(&clients[clientIndex].socket);
	cout << "[MoveToZone] done for client=" << clientIndex << endl;
}



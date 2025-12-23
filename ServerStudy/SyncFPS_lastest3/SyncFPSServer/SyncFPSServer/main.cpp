#define NOMINMAX
#include "stdafx.h"
#include "main.h"

World gameworld;
float DeltaTime = 0;
vector<Item> ItemTable;

int main() {
	PrintOffset();

	Socket listenSocket(SocketType::Tcp); // 1
	listenSocket.Bind(Endpoint("0.0.0.0", 1000)); // 2
	listenSocket.SetReceiveBuffer(new twoPage, sizeof(twoPage));

	cout << "Server Start" << endl;

	ui64 ft, st;
	ui64 TimeStack = 0;
	st = GetTicks();
	listenSocket.SetNonblocking();
	vector<PollFD> readFds;
	//??
	listenSocket.Listen();	// 3
	double DeltaFlow = 0;
	constexpr double InvHZ = 1.0 / (double)QUERYPERFORMANCE_HZ;

	gameworld.Init();

	while (true) {
		ft = GetTicks();
		DeltaFlow += (double)(ft - st) * InvHZ;
		if (DeltaFlow >= 0.016) { // limiting fps.
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
		readFds.push_back(item2); // first is listen socket.
		for (int i = 0; i < gameworld.clients.size; ++i)
		{
			PollFD item;
			item.m_pollfd.events = POLLRDNORM;
			item.m_pollfd.fd = gameworld.clients[i].socket.m_fd;
			item.m_pollfd.revents = 0;
			readFds.push_back(item);
		}

		// I/O 가능 이벤트가 있을 때까지 기다립니다.
		Poll(readFds.data(), (int)readFds.size(), 50);

		int num = 0;
		for (auto readFd : readFds)
		{
			if (readFd.m_pollfd.revents != 0)
			{
				if (num == 0) {
					// 4
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

						// player index sending
						int indexes[2] = {};
						int clientindex = newindex;
						gameworld.SendingAllObjectForNewClient(newindex); // pack factory

						Player* p = new Player();
						p->clientIndex = clientindex;
						p->m_worldMatrix.Id();
						p->m_worldMatrix.pos.f3.y = 10;
						p->ShapeIndex = Shape::GetShapeIndex("Player");
						//p->mesh = Mesh::StrToShapeMap["Player"];
						p->Inventory.reserve(36);
						p->Inventory.resize(36);
						for (int i = 0; i < 36; ++i) {
							p->Inventory[i].id = 0;
							p->Inventory[i].ItemCount = 0;
						}

						int objindex = gameworld.NewPlayer(p, clientindex);
						gameworld.clients[clientindex].pObjData = p;

						//int datacap = gameworld.Sending_AllocPlayerIndex(newindex, objindex);

						int dcap = gameworld.Sending_AllocPlayerIndex(clientindex, objindex);
						gameworld.pack_factory.push_data(dcap, gameworld.tempbuffer.data);
						//gameworld.clients[clientindex].socket.Send((char*)gameworld.tempbuffer.data, dcap);
						gameworld.clients[clientindex].objindex = objindex;

						if (p->ShapeIndex >= 0) {
							dcap = gameworld.Sending_SetMeshInGameObject(objindex, Shape::ShapeNameArr[p->ShapeIndex]);
							gameworld.SendToAllClient_execept(dcap, clientindex);
							gameworld.pack_factory.push_data(dcap, gameworld.tempbuffer.data);
						}
						//매쉬 데이터를 모든 클라이언트에게 전송
						gameworld.pack_factory.send(&gameworld.clients[clientindex].socket);

						//print log
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
						int objindex = gameworld.clients[index].objindex;
						delete gameworld.clients[index].pObjData;
						gameworld.clients[index].pObjData = nullptr;
						gameworld.gameObjects[objindex] = nullptr;
						gameworld.gameObjects.Free(objindex);
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

void World::Init() {
	ItemTable.push_back(Item(0)); // blank space in inventory.
	ItemTable.push_back(Item(1));
	ItemTable.push_back(Item(2));
	ItemTable.push_back(Item(3)); // test items. red, green, blue bullet mags.

	//astar grid init
	const int gridWidth = 80;
	const int gridHeight = 80;
	const float cellSize = 1.0f;

	const float offsetX = -gridWidth * cellSize * 0.5f;  // -40
	const float offsetZ = -gridHeight * cellSize * 0.5f; // -40

	allnodes.clear();
	allnodes.reserve(gridWidth * gridHeight);

	//Grid initiolaize
	for (int z = 0; z < gridHeight; ++z)
	{
		for (int x = 0; x < gridWidth; ++x)
		{
			AstarNode* node = new AstarNode();
			node->worldx = offsetX + x * cellSize + cellSize * 0.5f;
			node->worldz = offsetZ + z * cellSize + cellSize * 0.5f;
			node->xIndex = x;
			node->zIndex = z;
			node->gCost = 0;
			node->hCost = 0;
			node->fCost = 0;
			node->parent = nullptr;

			node->cango = true;

			allnodes.push_back(node);
		}
	}

	DropedItems.Init(4096);
	pack_factory.Clear();
	clients.Init(32);
	gameObjects.Init(128);

	GameObjectType::STATICINIT();

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
#else
	AddClientOffset(GameObjectType::_GameObject, 16, 16); // isExist
	AddClientOffset(GameObjectType::_GameObject, 32, 32); // world Matrix
	AddClientOffset(GameObjectType::_GameObject, 80, 144); // pos

	AddClientOffset(GameObjectType::_Player, 16, 16); // isExist
	AddClientOffset(GameObjectType::_Player, 32, 32); // world Matrix
	AddClientOffset(GameObjectType::_Player, 64, 128); // pos

	AddClientOffset(GameObjectType::_Player, 144, 160); // ShootFlow
	AddClientOffset(GameObjectType::_Player, 148, 164); // recoilFlow
	AddClientOffset(GameObjectType::_Player, 152, 168); // HP
	AddClientOffset(GameObjectType::_Player, 156, 172); // MaxHP
	AddClientOffset(GameObjectType::_Player, 160, 176); // DeltaMousePos

	AddClientOffset(GameObjectType::_Monster, 16, 16); // isExist
	AddClientOffset(GameObjectType::_Monster, 32, 32); // world Matrix
	AddClientOffset(GameObjectType::_Monster, 80, 144); // pos
#endif


	//bulletRays.Init(32);

	Mesh* MyMesh = new Mesh();
	MyMesh->ReadMeshFromFile_OBJ("Resources/Mesh/PlayerMesh.obj", { 1, 1, 1, 1 });
	int playerMesh_index = Shape::AddMesh("Player", MyMesh);

	Mesh* MyGroundMesh = new Mesh();
	MyGroundMesh->CreateWallMesh(40.0f, 0.5f, 40.0f);
	int groundMesh_index = Shape::AddMesh("Ground001", MyGroundMesh);

	Mesh* MyWallMesh = new Mesh();
	MyWallMesh->CreateWallMesh(5.0f, 2.0f, 1.0f);
	int wallMesh_index = Shape::AddMesh("Wall001", MyWallMesh);

	/*BulletRay::mesh = new Mesh();
	BulletRay::mesh->ReadMeshFromFile_OBJ("Resources/Mesh/RayMesh.obj", { 1, 1, 0, 1 }, false);*/

	Mesh* MyMonsterMesh = new Mesh();
	MyMonsterMesh->ReadMeshFromFile_OBJ("Resources/Mesh/PlayerMesh.obj", { 1, 0, 0, 1 });
	int monsterMesh_index = Shape::AddMesh("Monster001", MyMonsterMesh);

	int newindex = 0;
	int datacap = 0;

	/*Player* MyPlayer = new Player();
	MyPlayer->m_worldMatrix = (XMMatrixTranslation(5.0f, 5.0f, 3.0f));
	MyPlayer->MeshIndex = Mesh::GetMeshIndex("Player");
	newindex = NewObject(MyPlayer, GameObjectType::_Player);
	MyPlayer->mesh = *MyMesh;
	int datacap = Sending_SetMeshInGameObject(newindex, "Player");
	SendToAllClient(datacap);*/

	GameObject* groundObject = new GameObject();
	groundObject->ShapeIndex = Shape::GetShapeIndex("Ground001");
	//groundObject->mesh = MyGroundMesh;
	groundObject->m_worldMatrix = (XMMatrixTranslation(0.0f, -1.0f, 0.0f));
	newindex = NewObject(groundObject, GameObjectType::_GameObject);
	datacap = Sending_SetMeshInGameObject(newindex, "Ground001");
	SendToAllClient(datacap);

	GameObject* wallObject = new GameObject();
	wallObject->ShapeIndex = Shape::GetShapeIndex("Wall001");
	//wallObject->mesh = (MyWallMesh);
	wallObject->m_worldMatrix = (XMMatrixTranslation(0.0f, 1.0f, 5.0f));
	newindex = NewObject(wallObject, GameObjectType::_GameObject);
	datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
	SendToAllClient(datacap);

	GameObject* wallObject2 = new GameObject();
	wallObject2->ShapeIndex = Shape::GetShapeIndex("Wall001");
	//wallObject2->mesh = (MyWallMesh);
	wallObject2->m_worldMatrix = (XMMatrixMultiply(XMMatrixRotationY(45), XMMatrixTranslation(10.0f, 1.0f, 0.0f)));
	newindex = NewObject(wallObject2, GameObjectType::_GameObject);
	datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
	SendToAllClient(datacap);

	GameObject* wallObject3 = new GameObject();
	wallObject3->ShapeIndex = Shape::GetShapeIndex("Wall001");
	//wallObject3->mesh = (MyWallMesh);
	wallObject3->m_worldMatrix = (XMMatrixMultiply(XMMatrixRotationZ(3.141592f / 6), XMMatrixTranslation(5.0f, 0.0f, -5.0f)));
	newindex = NewObject(wallObject3, GameObjectType::_GameObject);
	datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
	SendToAllClient(datacap);

	GameObject* wallObject4 = new GameObject();
	wallObject4->ShapeIndex = Shape::GetShapeIndex("Wall001");
	//wallObject4->mesh = (MyWallMesh);
	wallObject4->m_worldMatrix = (XMMatrixMultiply(XMMatrixRotationZ(-3.141592f / 4), XMMatrixTranslation(-7.0f, -1.0f, 0)));
	newindex = NewObject(wallObject4, GameObjectType::_GameObject);
	datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
	SendToAllClient(datacap);

	wallObject4 = new GameObject();
	wallObject4->ShapeIndex = Shape::GetShapeIndex("Wall001");
	//wallObject4->mesh = (MyWallMesh);
	wallObject4->m_worldMatrix = (XMMatrixMultiply(XMMatrixRotationX(3.141592f / 4), XMMatrixTranslation(-10.0f, -1.0f, -10.0f)));
	newindex = NewObject(wallObject4, GameObjectType::_GameObject);
	datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
	SendToAllClient(datacap);

	wallObject2 = new GameObject();
	wallObject2->ShapeIndex = Shape::GetShapeIndex("Wall001");
	//wallObject2->mesh = (MyWallMesh);
	wallObject2->m_worldMatrix = XMMatrixRotationY(-3.141592f / 4);
	wallObject2->m_worldMatrix.pos = vec4(20.0f, 1.0f, -20.0f, 1);
	newindex = NewObject(wallObject2, GameObjectType::_GameObject);
	datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
	SendToAllClient(datacap);

	wallObject2 = new GameObject();
	wallObject2->ShapeIndex = Shape::GetShapeIndex("Wall001");
	//wallObject2->mesh = (MyWallMesh);
	wallObject2->m_worldMatrix = XMMatrixRotationY(3.141592f / 4);
	wallObject2->m_worldMatrix.pos = vec4(-20.0f, 1.0f, -20.0f, 1);
	newindex = NewObject(wallObject2, GameObjectType::_GameObject);
	datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
	SendToAllClient(datacap);

	wallObject2 = new GameObject();
	wallObject2->ShapeIndex = Shape::GetShapeIndex("Wall001");
	//wallObject2->mesh = (MyWallMesh);
	wallObject2->m_worldMatrix = XMMatrixRotationY(3.141592f / 4);
	wallObject2->m_worldMatrix.pos = vec4(20.0f, 1.0f, 20.0f, 1);
	newindex = NewObject(wallObject2, GameObjectType::_GameObject);
	datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
	SendToAllClient(datacap);

	wallObject = new GameObject();
	wallObject->ShapeIndex = Shape::GetShapeIndex("Wall001");
	//wallObject->mesh = (MyWallMesh);
	wallObject->m_worldMatrix = (XMMatrixTranslation(-23.0f, 1.0f, 15.0f));
	newindex = NewObject(wallObject, GameObjectType::_GameObject);
	datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
	SendToAllClient(datacap);

	for (int i = 0; i < 1; ++i) {
		wallObject2 = new GameObject();
		wallObject2->ShapeIndex = Shape::GetShapeIndex("Wall001");
		//wallObject2->mesh = (MyWallMesh);
		wallObject2->m_worldMatrix = (XMMatrixMultiply(XMMatrixRotationY(45), XMMatrixTranslation((float)(rand() % 80 - 40), 1.0f, (float)(rand() % 80 - 40))));
		newindex = NewObject(wallObject2, GameObjectType::_GameObject);
		datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
		SendToAllClient(datacap);

		wallObject3 = new GameObject();
		wallObject3->ShapeIndex = Shape::GetShapeIndex("Wall001");
		//wallObject3->mesh = (MyWallMesh);
		wallObject3->m_worldMatrix = (XMMatrixMultiply(XMMatrixRotationZ(3.141592f / 6), XMMatrixTranslation((float)(rand() % 80 - 40), 0.0f, (float)(rand() % 80 - 40))));
		newindex = NewObject(wallObject3, GameObjectType::_GameObject);
		datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
		SendToAllClient(datacap);

		wallObject4 = new GameObject();
		wallObject4->ShapeIndex = Shape::GetShapeIndex("Wall001");
		//wallObject4->mesh = (MyWallMesh);
		wallObject4->m_worldMatrix = (XMMatrixMultiply(XMMatrixRotationZ(-3.141592f / 4), XMMatrixTranslation((float)(rand() % 80 - 40), -1.0f, (float)(rand() % 80 - 40))));
		newindex = NewObject(wallObject4, GameObjectType::_GameObject);
		datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
		SendToAllClient(datacap);

		wallObject4 = new GameObject();
		wallObject4->ShapeIndex = Shape::GetShapeIndex("Wall001");
		//wallObject4->mesh = (MyWallMesh);
		wallObject4->m_worldMatrix = (XMMatrixMultiply(XMMatrixRotationX(3.141592f / 4), XMMatrixTranslation((float)(rand() % 80 - 40), -1.0f, (float)(rand() % 80 - 40))));
		newindex = NewObject(wallObject4, GameObjectType::_GameObject);
		datacap = Sending_SetMeshInGameObject(newindex, "Wall001");
		SendToAllClient(datacap);
	}


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

	for (int i = 0; i < 20; ++i) {
		Monster* myMonster_1 = new Monster();
		myMonster_1->ShapeIndex = Shape::GetShapeIndex("Monster001");
		//myMonster_1->mesh = (MyMonsterMesh);
		myMonster_1->Init(XMMatrixTranslation(rand() % 80 - 40, 20.0f, rand() % 80 - 40));
		newindex = NewObject(myMonster_1, GameObjectType::_Monster);
		datacap = Sending_SetMeshInGameObject(newindex, "Monster001");
		SendToAllClient(datacap);
	}

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
	gridcollisioncheck();

	const int GRID_W = 80;
	const int GRID_H = 80;

	//PrintCangoSummary(allnodes, GRID_W, GRID_H);
	PrintCangoGrid(allnodes, GRID_W, GRID_H);

	map.LoadMap("The_Port");

	cout << "Game Init end" << endl;
}

void World::Update() {
	lowFrequencyFlow += DeltaTime;
	midFrequencyFlow += DeltaTime;
	highFrequencyFlow += DeltaTime;

	for (currentIndex = 0; currentIndex < gameObjects.size; ++currentIndex) {
		if (gameObjects.isnull(currentIndex)) continue;
		if (gameObjects[currentIndex]->isExist == false) {
			continue;
		}
		gameObjects[currentIndex]->Update(DeltaTime);
	}
	//delete 한거를 업데이트해서
	// Collision......

	//bool bFixed = false;
	for (int i = 0; i < gameObjects.size; ++i) {
		if (gameObjects.isnull(i)) continue;
		GameObject* gbj1 = gameObjects[i];
		//if (gbj1->tickLVelocity.fast_square_of_len3 <= 0.001f) bFixed = true;

		Shape s1 = Shape::IndexToShapeMap[gbj1->ShapeIndex];
		float fsl1 = 0;
		if (s1.isMesh()) {
			fsl1 = s1.GetMesh()->MAXpos.fast_square_of_len3;
		}
		else fsl1 = s1.GetModel()->OBB_Ext.fast_square_of_len3;
		
		vec4 lastpos1 = gbj1->m_worldMatrix.pos + gbj1->tickLVelocity;

		for (int j = i + 1; j < gameObjects.size; ++j) {
			if (gameObjects.isnull(j)) continue;
			GameObject* gbj2 = gameObjects[j];
			Shape s2 = Shape::IndexToShapeMap[gbj2->ShapeIndex];

			vec4 Ext2 = 0;
			if (s1.isMesh()) {
				Ext2 = s2.GetMesh()->MAXpos;
			}
			else Ext2 = s2.GetModel()->OBB_Ext;

			vec4 dist = lastpos1 - (gbj2->m_worldMatrix.pos + gbj2->tickLVelocity);
			//if (gbj2->tickLVelocity.fast_square_of_len3 <= 0.001f && bFixed) continue;

			if (fsl1 + Ext2.fast_square_of_len3 > dist.fast_square_of_len3) {
				GameObject::CollisionMove(gbj1, gbj2);
			}
			//GameObject::CollisionMove(gbj1, gbj2);
		}

		map.StaticCollisionMove(gbj1);

		//gbj1->tickLVelocity.w = 0;
		if (gbj1->tickLVelocity.fast_square_of_len3 <= 0.001f) continue;

		gbj1->m_worldMatrix.pos += gbj1->tickLVelocity;
		/*if (fabsf(gbj1->m_worldMatrix.pos.x) > 40.0f || fabsf(gbj1->m_worldMatrix.pos.z) > 40.0f) {
			gbj1->m_worldMatrix.pos -= gbj1->tickLVelocity;
		}*/
		gbj1->tickLVelocity = XMVectorZero();

		// send pos to client
		int datacap = Sending_ChangeGameObjectMember(i, gbj1, GameObjectType::_GameObject, &(gbj1->m_worldMatrix.pos), 16);
		SendToAllClient(datacap);
	}

	//for (int i = 0; i < clients.size; ++i) {
	//	if (clients.isnull(i)) continue;
	//	Player& p = *clients[i].pObjData;
	//	if (p.InputBuffer[(int)InputID::KeyboardW]) {
	//		clients[i].pos += XMVectorSet(0, 0, ClientData::MoveSpeed * DeltaTime, 0);
	//		clients[i].isPosUpdate = true;
	//	}
	//	if (clients[i].InputBuffer[(int)InputID::KeyboardA]) {
	//		clients[i].pos += XMVectorSet(-ClientData::MoveSpeed * DeltaTime, 0, 0, 0);
	//		clients[i].isPosUpdate = true;
	//	}
	//	if (clients[i].InputBuffer[(int)InputID::KeyboardS]) {
	//		clients[i].pos += XMVectorSet(0, 0, -ClientData::MoveSpeed * DeltaTime, 0);
	//		clients[i].isPosUpdate = true;
	//	}
	//	if (clients[i].InputBuffer[(int)InputID::KeyboardD]) {
	//		clients[i].pos += XMVectorSet(ClientData::MoveSpeed * DeltaTime, 0, 0, 0);
	//		clients[i].isPosUpdate = true;
	//	}
	//}

	for (int i = 0; i < clients.size; ++i) {
		if (clients[i].isPosUpdate) {
			clients[i].socket.Send((char*)&clients[i].pos.m128_f32[0], 12);
			clients[i].isPosUpdate = false;
		}
	}

	if (lowHit()) {
		lowFrequencyFlow = 0;
	}
	if (midHit()) {
		midFrequencyFlow = 0;
	}
	if (highHit()) {
		highFrequencyFlow = 0;
	}
}

void World::gridcollisioncheck()
{
	const float radius = 0.2f;
	const float groundY = 0.0f;

	for (AstarNode* node : allnodes)
	{
		vec4 nodePos(node->worldx, groundY, node->worldz);
		collisionchecksphere sphere = { nodePos, radius };

		bool blocked = false;

		// 모든 오브젝트 중 "Wall001" 메쉬만 검사
		for (int i = 0; i < gameObjects.size; ++i)
		{
			if (gameObjects.isnull(i)) continue;
			GameObject* obj = gameObjects[i];

			// 벽 메쉬 식별
			if (obj->ShapeIndex == Shape::GetShapeIndex("Wall001"))
			{
				vec4 wallCenter(obj->m_worldMatrix.pos.x,
					obj->m_worldMatrix.pos.y,
					obj->m_worldMatrix.pos.z);

				// Wall001이 5x2x1로 생성되었다면 절반 크기 = {2.5, 1.0, 0.5}
				vec4 wallHalfSize(2.5f, 1.0f, 0.5f);

				if (CheckAABBSphereCollision(wallCenter, wallHalfSize, sphere))
				{
					blocked = true;
					break;
				}
			}
		}
		node->cango = !blocked;
	}
}

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
		client.pObjData->InputBuffer[rBuffer[0]] = putv;
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

int World::NewObject(GameObject* obj, GameObjectType gotype)
{
	int newindex = gameObjects.Alloc();
	gameObjects[newindex] = obj;
	obj->isExist = true;

	switch (gotype) {
	case GameObjectType::_GameObject:
	{
		int datacap = Sending_NewGameObject(newindex, obj, gotype);
		for (int i = 0; i < clients.size; ++i) {
			if (clients.isnull(i)) continue;
			clients[i].socket.Send((char*)tempbuffer.data, datacap);
		}
		break;
	}
	case GameObjectType::_Player:
	{
		int datacap = Sending_NewGameObject(newindex, obj, gotype);
		gameworld.SendToAllClient(datacap);

		char* member = (char*)obj + sizeof(GameObject);
		int len = 32;
		datacap = Sending_ChangeGameObjectMember(newindex, obj, gotype, member, len);
		gameworld.SendToAllClient(datacap);

		datacap = Sending_ChangeGameObjectMember(newindex, obj, gotype, &((Player*)obj)->bullets, sizeof(int));
		gameworld.SendToAllClient(datacap);

		datacap = Sending_ChangeGameObjectMember(newindex, obj, gotype, &((Player*)obj)->KillCount, sizeof(int));
		gameworld.SendToAllClient(datacap);

		datacap = Sending_ChangeGameObjectMember(newindex, obj, gotype, &((Player*)obj)->DeathCount, sizeof(int));
		gameworld.SendToAllClient(datacap);
		break;
	}
	case GameObjectType::_Monster:
	{
		int datacap = Sending_NewGameObject(newindex, obj, gotype);
		gameworld.SendToAllClient(datacap);

		datacap = Sending_ChangeGameObjectMember(newindex, obj, gotype, &((Monster*)obj)->isDead, sizeof(bool));
		gameworld.SendToAllClient(datacap);

		datacap = Sending_ChangeGameObjectMember(newindex, obj, gotype, &((Monster*)obj)->HP, sizeof(float));
		gameworld.SendToAllClient(datacap);
		break;
	}
	}
	return newindex;
}

int World::NewPlayer(Player* obj, int clientIndex)
{
	int newindex = gameObjects.Alloc();
	gameObjects[newindex] = obj;
	obj->isExist = true;

	int datacap = Sending_NewGameObject(newindex, obj, GameObjectType::_Player);
	gameworld.SendToAllClient_execept(datacap, clientIndex);
	pack_factory.push_data(datacap, tempbuffer.data);

	char* member = (char*)obj + sizeof(GameObject);
	int len = 32;
	datacap = Sending_ChangeGameObjectMember(newindex, obj, GameObjectType::_Player, member, len);
	gameworld.SendToAllClient_execept(datacap, clientIndex);
	pack_factory.push_data(datacap, tempbuffer.data);

	datacap = Sending_ChangeGameObjectMember(newindex, obj, GameObjectType::_Player, &((Player*)obj)->bullets, sizeof(int));
	gameworld.SendToAllClient_execept(datacap, clientIndex);
	pack_factory.push_data(datacap, tempbuffer.data);

	datacap = Sending_ChangeGameObjectMember(newindex, obj, GameObjectType::_Player, &((Player*)obj)->KillCount, sizeof(int));
	gameworld.SendToAllClient_execept(datacap, clientIndex);
	pack_factory.push_data(datacap, tempbuffer.data);

	datacap = Sending_ChangeGameObjectMember(newindex, obj, GameObjectType::_Player, &((Player*)obj)->DeathCount, sizeof(int));
	gameworld.SendToAllClient_execept(datacap, clientIndex);
	pack_factory.push_data(datacap, tempbuffer.data);
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

void World::FireRaycast(GameObject* shooter, vec4 rayStart, vec4 rayDirection, float rayDistance)
{
	vec4 rayOrigin = rayStart;

	GameObject* closestHitObject = nullptr;

	float closestDistance = rayDistance;

	int lastcurrentindex = gameworld.currentIndex;

	for (int i = 0; i < gameObjects.size; ++i) {
		if (gameObjects.isnull(i) || shooter == gameObjects[i]) continue;
		gameworld.currentIndex = i;
		BoundingOrientedBox obb = gameObjects[i]->GetOBB();
		float distance;

		if (obb.Intersects(rayOrigin, rayDirection, distance)) {
			if (distance <= rayDistance) {
				if (distance < closestDistance) {
					closestDistance = distance;
					closestHitObject = gameObjects[i];

					gameObjects[i]->OnCollisionRayWithBullet(shooter);
				}
			}
		}
	}

	gameworld.currentIndex = lastcurrentindex;
	int datacap = Sending_NewRay(rayStart, rayDirection, closestDistance);
	gameworld.SendToAllClient(datacap);
}

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

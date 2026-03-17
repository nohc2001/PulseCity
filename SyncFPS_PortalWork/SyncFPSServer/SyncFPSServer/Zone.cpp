// Zone.cpp
#include "stdafx.h"
#include "Zone.h"
#include "main.h"


void Zone::Init() {
    // Astar 그리드 초기화
    const int gridWidth = 80;
    const int gridHeight = 80;
    const float cellSize = 1.0f;
    const float offsetX = -gridWidth * cellSize * 0.5f;
    const float offsetZ = -gridHeight * cellSize * 0.5f;

    allnodes.clear();
    allnodes.reserve(gridWidth * gridHeight);

    for (int z = 0; z < gridHeight; ++z) {
        for (int x = 0; x < gridWidth; ++x) {
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

    // 배열 초기화
    gameObjects.Init(128);
    DropedItems.Init(4096);

    // 맵 로드 (존별로 다른 맵)
    LoadMapForZone(zoneId);
    if (map.MapObjects.empty()) {
        cout << "Zone " << zoneId << " map load failed!" << endl;
        return;  // 또는 기본 맵 로드 등 fallback 처리
    }

    // 오브젝트 스폰
    SpawnObjects();

    // 그리드 충돌 체크
    GridCollisionCheck();

    // 포탈 스폰
    SpawnPortal();

    cout << "Zone " << zoneId << " Init end" << endl;
}

void Zone::Update(float deltaTime) {
    // 게임오브젝트 업데이트
    for (int i = 0; i < gameObjects.size; ++i) {
        if (gameObjects.isnull(i)) continue;
        if (!gameObjects[i]->isExist) continue;
        currentIndex = i;
        world->currentIndex = i;
        gameObjects[i]->Update(deltaTime);
    }

    // GridCollisionCheck() 제거!
    // 이유: 벽은 안 움직이므로 Init()에서 한 번만 호출하면 됨

    // 충돌 처리
    for (int i = 0; i < gameObjects.size; ++i) {
        if (gameObjects.isnull(i)) continue;
        GameObject* gbj1 = gameObjects[i];

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
            if (s2.isMesh()) {
                Ext2 = s2.GetMesh()->MAXpos;
            }
            else Ext2 = s2.GetModel()->OBB_Ext;

            vec4 dist = lastpos1 - (gbj2->m_worldMatrix.pos + gbj2->tickLVelocity);

            if (fsl1 + Ext2.fast_square_of_len3 > dist.fast_square_of_len3) {
                GameObject::CollisionMove(gbj1, gbj2);
            }
        }

        map.StaticCollisionMove(gbj1);

        if (gbj1->tickLVelocity.fast_square_of_len3 <= 0.001f) continue;

        gbj1->m_worldMatrix.pos += gbj1->tickLVelocity;
        gbj1->tickLVelocity = XMVectorZero();

        // 이 존의 클라이언트들에게 위치 전송
        int datacap = world->Sending_ChangeGameObjectMember(i, gbj1, GameObjectType::_GameObject, &(gbj1->m_worldMatrix.pos), 16);
        SendToAll(datacap);
    }

    // 포탈 검사
    for (int i = 0; i < world->clients.size; ++i) {
        if (world->clients.isnull(i)) continue;
        if (world->clients[i].zoneId != this->zoneId) continue;
        Player* p = world->clients[i].pObjData;
        CheckPortalCollision(p);
    }
}

void Zone::SendToAll(int datacap) {
    for (int i = 0; i < world->clients.size; ++i) {
        if (world->clients.isnull(i)) continue;
        if (world->clients[i].zoneId != this->zoneId) continue;
        world->clients[i].socket.Send((char*)world->tempbuffer.data, datacap);
    }
}

void Zone::SendToAll_except(int datacap, int except) {
    for (int i = 0; i < world->clients.size; ++i) {
        if (world->clients.isnull(i)) continue;
        if (i == except) continue;
        if (world->clients[i].zoneId != this->zoneId) continue;
        world->clients[i].socket.Send((char*)world->tempbuffer.data, datacap);
    }
}

void Zone::SendZoneDataToClient(int clientIndex) {
    // 이 존의 모든 오브젝트 전송
    for (int i = 0; i < gameObjects.size; ++i) {
        if (gameObjects.isnull(i)) continue;

        void* vptr = *(void**)gameObjects[i];
        GameObjectType gotype = GameObjectType::VptrToTypeTable[vptr];

        int datacap = world->Sending_NewGameObject(i, gameObjects[i], gotype);
        world->clients[clientIndex].socket.Send((char*)world->tempbuffer.data, datacap);

        if (gameObjects[i]->ShapeIndex >= 0) {
            datacap = world->Sending_SetMeshInGameObject(i, Shape::ShapeNameArr[gameObjects[i]->ShapeIndex]);
            world->clients[clientIndex].socket.Send((char*)world->tempbuffer.data, datacap);
        }

        // 몬스터 추가 정보
        if (gotype == GameObjectType::_Monster) {
            Monster* mon = (Monster*)gameObjects[i];
            datacap = world->Sending_ChangeGameObjectMember(i, mon, GameObjectType::_Monster, &mon->isDead, sizeof(bool));
            world->clients[clientIndex].socket.Send((char*)world->tempbuffer.data, datacap);

            datacap = world->Sending_ChangeGameObjectMember(i, mon, GameObjectType::_Monster, &mon->HP, sizeof(float));
            world->clients[clientIndex].socket.Send((char*)world->tempbuffer.data, datacap);

            datacap = world->Sending_ChangeGameObjectMember(i, mon, GameObjectType::_Monster, &mon->MaxHP, sizeof(float));
            world->clients[clientIndex].socket.Send((char*)world->tempbuffer.data, datacap);
        }
        // 플레이어 추가 정보
        else if (gotype == GameObjectType::_Player) {
            Player* p = (Player*)gameObjects[i];
            datacap = world->Sending_ChangeGameObjectMember(i, p, GameObjectType::_Player, &p->KillCount, sizeof(int));
            world->clients[clientIndex].socket.Send((char*)world->tempbuffer.data, datacap);

            datacap = world->Sending_ChangeGameObjectMember(i, p, GameObjectType::_Player, &p->DeathCount, sizeof(int));
            world->clients[clientIndex].socket.Send((char*)world->tempbuffer.data, datacap);

            datacap = world->Sending_ChangeGameObjectMember(i, p, GameObjectType::_Player, &p->bullets, sizeof(int));
            world->clients[clientIndex].socket.Send((char*)world->tempbuffer.data, datacap);
        }
    }

    // 드롭 아이템 전송
    for (int i = 0; i < DropedItems.size; ++i) {
        if (DropedItems.isnull(i)) continue;
        int datacap = world->Sending_ItemDrop(i, DropedItems[i]);
        world->clients[clientIndex].socket.Send((char*)world->tempbuffer.data, datacap);
    }

    // 플레이어 인덱스 할당 전송
    int objIdx = world->clients[clientIndex].objindex;
    int datacap = world->Sending_AllocPlayerIndex(clientIndex, objIdx);
    world->clients[clientIndex].socket.Send((char*)world->tempbuffer.data, datacap);
}

void Zone::LoadMapForZone(int zoneId)
{
    // 존별로 다른 맵 로드
    switch (zoneId) {
    case 0:
        map.LoadMap("The_Port");
        break;
    case 1:
        map.LoadMap("The_port");  // 다른 맵
        break;
    default:
        map.LoadMap("The_Port");
        break;
    }
}

void Zone::SpawnObjects()
{

	int newindex = 0;

	GameObject* groundObject = new GameObject();
	groundObject->ShapeIndex = Shape::GetShapeIndex("Ground001");
	//groundObject->mesh = MyGroundMesh;
	groundObject->m_worldMatrix = (XMMatrixTranslation(0.0f, -1.0f, 0.0f));
	newindex = NewObject(groundObject, GameObjectType::_GameObject);
	

	GameObject* wallObject = new GameObject();
	wallObject->ShapeIndex = Shape::GetShapeIndex("Wall001");
	//wallObject->mesh = (MyWallMesh);
	wallObject->m_worldMatrix = (XMMatrixTranslation(0.0f, 1.0f, 5.0f));
	newindex = NewObject(wallObject, GameObjectType::_GameObject);
	

	GameObject* wallObject2 = new GameObject();
	wallObject2->ShapeIndex = Shape::GetShapeIndex("Wall001");
	//wallObject2->mesh = (MyWallMesh);
	wallObject2->m_worldMatrix = (XMMatrixMultiply(XMMatrixRotationY(45), XMMatrixTranslation(10.0f, 1.0f, 0.0f)));
	newindex = NewObject(wallObject2, GameObjectType::_GameObject);
	
	GameObject* wallObject3 = new GameObject();
	wallObject3->ShapeIndex = Shape::GetShapeIndex("Wall001");
	//wallObject3->mesh = (MyWallMesh);
	wallObject3->m_worldMatrix = (XMMatrixMultiply(XMMatrixRotationZ(3.141592f / 6), XMMatrixTranslation(5.0f, 0.0f, -5.0f)));
	newindex = NewObject(wallObject3, GameObjectType::_GameObject);
	
	GameObject* wallObject4 = new GameObject();
	wallObject4->ShapeIndex = Shape::GetShapeIndex("Wall001");
	//wallObject4->mesh = (MyWallMesh);
	wallObject4->m_worldMatrix = (XMMatrixMultiply(XMMatrixRotationZ(-3.141592f / 4), XMMatrixTranslation(-7.0f, -1.0f, 0)));
	newindex = NewObject(wallObject4, GameObjectType::_GameObject);
	

	wallObject4 = new GameObject();
	wallObject4->ShapeIndex = Shape::GetShapeIndex("Wall001");
	//wallObject4->mesh = (MyWallMesh);
	wallObject4->m_worldMatrix = (XMMatrixMultiply(XMMatrixRotationX(3.141592f / 4), XMMatrixTranslation(-10.0f, -1.0f, -10.0f)));
	newindex = NewObject(wallObject4, GameObjectType::_GameObject);
	
	wallObject2 = new GameObject();
	wallObject2->ShapeIndex = Shape::GetShapeIndex("Wall001");
	//wallObject2->mesh = (MyWallMesh);
	wallObject2->m_worldMatrix = XMMatrixRotationY(-3.141592f / 4);
	wallObject2->m_worldMatrix.pos = vec4(20.0f, 1.0f, -20.0f, 1);
	newindex = NewObject(wallObject2, GameObjectType::_GameObject);
	
	wallObject2 = new GameObject();
	wallObject2->ShapeIndex = Shape::GetShapeIndex("Wall001");
	//wallObject2->mesh = (MyWallMesh);
	wallObject2->m_worldMatrix = XMMatrixRotationY(3.141592f / 4);
	wallObject2->m_worldMatrix.pos = vec4(-20.0f, 1.0f, -20.0f, 1);
	newindex = NewObject(wallObject2, GameObjectType::_GameObject);
	
	wallObject2 = new GameObject();
	wallObject2->ShapeIndex = Shape::GetShapeIndex("Wall001");
	//wallObject2->mesh = (MyWallMesh);
	wallObject2->m_worldMatrix = XMMatrixRotationY(3.141592f / 4);
	wallObject2->m_worldMatrix.pos = vec4(20.0f, 1.0f, 20.0f, 1);
	newindex = NewObject(wallObject2, GameObjectType::_GameObject);
	
	wallObject = new GameObject();
	wallObject->ShapeIndex = Shape::GetShapeIndex("Wall001");
	//wallObject->mesh = (MyWallMesh);
	wallObject->m_worldMatrix = (XMMatrixTranslation(-23.0f, 1.0f, 15.0f));
	newindex = NewObject(wallObject, GameObjectType::_GameObject);
	
	for (int i = 0; i < 1; ++i) {
		wallObject2 = new GameObject();
		wallObject2->ShapeIndex = Shape::GetShapeIndex("Wall001");
		//wallObject2->mesh = (MyWallMesh);
		wallObject2->m_worldMatrix = (XMMatrixMultiply(XMMatrixRotationY(45), XMMatrixTranslation((float)(rand() % 80 - 40), 1.0f, (float)(rand() % 80 - 40))));
		newindex = NewObject(wallObject2, GameObjectType::_GameObject);
		
		wallObject3 = new GameObject();
		wallObject3->ShapeIndex = Shape::GetShapeIndex("Wall001");
		//wallObject3->mesh = (MyWallMesh);
		wallObject3->m_worldMatrix = (XMMatrixMultiply(XMMatrixRotationZ(3.141592f / 6), XMMatrixTranslation((float)(rand() % 80 - 40), 0.0f, (float)(rand() % 80 - 40))));
		newindex = NewObject(wallObject3, GameObjectType::_GameObject);
		

		wallObject4 = new GameObject();
		wallObject4->ShapeIndex = Shape::GetShapeIndex("Wall001");
		//wallObject4->mesh = (MyWallMesh);
		wallObject4->m_worldMatrix = (XMMatrixMultiply(XMMatrixRotationZ(-3.141592f / 4), XMMatrixTranslation((float)(rand() % 80 - 40), -1.0f, (float)(rand() % 80 - 40))));
		newindex = NewObject(wallObject4, GameObjectType::_GameObject);
		
		wallObject4 = new GameObject();
		wallObject4->ShapeIndex = Shape::GetShapeIndex("Wall001");
		//wallObject4->mesh = (MyWallMesh);
		wallObject4->m_worldMatrix = (XMMatrixMultiply(XMMatrixRotationX(3.141592f / 4), XMMatrixTranslation((float)(rand() % 80 - 40), -1.0f, (float)(rand() % 80 - 40))));
		newindex = NewObject(wallObject4, GameObjectType::_GameObject);
		
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
    if (zoneId == 1) return;

	for (int i = 0; i < 20; ++i) {
		Monster* myMonster_1 = new Monster();
		myMonster_1->ShapeIndex = Shape::GetShapeIndex("Monster001");
		//myMonster_1->mesh = (MyMonsterMesh);

		myMonster_1->Init(XMMatrixTranslation(rand() % 80 - 40, 20.0f, rand() % 80 - 40));
		while (map.isStaticCollision(myMonster_1->GetOBB())) {
			myMonster_1->Init(XMMatrixTranslation(rand() % 80 - 40, 10.0f, rand() % 80 - 40));
		}

		newindex = NewObject(myMonster_1, GameObjectType::_Monster);
	}
}

void Zone::FireRaycast(GameObject* shooter, vec4 rayStart, vec4 rayDirection, float rayDistance)
{
    vec4 rayOrigin = rayStart;
    GameObject* closestHitObject = nullptr;
    float closestDistance = rayDistance;

    int lastcurrentindex = currentIndex;

    for (int i = 0; i < gameObjects.size; ++i) {
        if (gameObjects.isnull(i) || shooter == gameObjects[i]) continue;

        currentIndex = i;
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

    currentIndex = lastcurrentindex;

    // 이 존의 클라이언트들에게만 전송
    int datacap = world->Sending_NewRay(rayStart, rayDirection, closestDistance);
    SendToAll(datacap);
}

int Zone::NewObject(GameObject* obj, GameObjectType gotype) {
    int newindex = gameObjects.Alloc();
    gameObjects[newindex] = obj;
    obj->isExist = true;

    switch (gotype) {
    case GameObjectType::_GameObject:
    {
        int datacap = world->Sending_NewGameObject(newindex, obj, gotype);
        SendToAll(datacap);  // 이 존의 클라이언트만
        break;
    }
    case GameObjectType::_Player:
    {
        int datacap = world->Sending_NewGameObject(newindex, obj, gotype);
        SendToAll(datacap);

        char* member = (char*)obj + sizeof(GameObject);
        int len = 32;
        datacap = world->Sending_ChangeGameObjectMember(newindex, obj, gotype, member, len);
        SendToAll(datacap);

        datacap = world->Sending_ChangeGameObjectMember(newindex, obj, gotype, &((Player*)obj)->bullets, sizeof(int));
        SendToAll(datacap);

        datacap = world->Sending_ChangeGameObjectMember(newindex, obj, gotype, &((Player*)obj)->KillCount, sizeof(int));
        SendToAll(datacap);

        datacap = world->Sending_ChangeGameObjectMember(newindex, obj, gotype, &((Player*)obj)->DeathCount, sizeof(int));
        SendToAll(datacap);
        break;
    }
    case GameObjectType::_Monster:
    {
        int datacap = world->Sending_NewGameObject(newindex, obj, gotype);
        SendToAll(datacap);

        datacap = world->Sending_ChangeGameObjectMember(newindex, obj, gotype, &((Monster*)obj)->isDead, sizeof(bool));
        SendToAll(datacap);

        datacap = world->Sending_ChangeGameObjectMember(newindex, obj, gotype, &((Monster*)obj)->HP, sizeof(float));
        SendToAll(datacap);
        break;
    }
    case GameObjectType::_Portal:
    {
        int datacap = world->Sending_NewGameObject(newindex, obj, gotype);
        SendToAll(datacap);
        break;
    }
    }

    // Mesh 정보 전송
    if (obj->ShapeIndex >= 0) {
        int datacap = world->Sending_SetMeshInGameObject(newindex, Shape::ShapeNameArr[obj->ShapeIndex]);
        SendToAll(datacap);
    }

    return newindex;
}

void Zone::SpawnPortal() {
    Portal* portal = new Portal();
    portal->ShapeIndex = Shape::GetShapeIndex("Portal");
    portal->zoneId = this->zoneId;

    if (zoneId == 0) {
        // Zone 0 → Zone 1
        portal->m_worldMatrix = XMMatrixTranslation(5.0f, 5.0f, 0.0f);
        portal->dstzoneId = 1;
        portal->spawnX = -25.0f;
        portal->spawnY = 10.0f;
        portal->spawnZ = 0.0f;
        portal->radius = 3.0f;
    }
    else if (zoneId == 1) {
        // Zone 1 → Zone 0
        portal->m_worldMatrix = XMMatrixTranslation(-30.0f, 0.0f, 0.0f);
        portal->dstzoneId = 0;
        portal->spawnX = 25.0f;
        portal->spawnY = 10.0f;
        portal->spawnZ = 0.0f;
        portal->radius = 3.0f;
    }

    NewObject(portal, GameObjectType::_Portal);
}

void Zone::RemovePlayer(int clientIndex) {
    int objIdx = world->clients[clientIndex].objindex;

    // 이 존의 클라이언트들에게 삭제 알림
    int dcap = world->Sending_DeleteGameObject(objIdx);
    SendToAll(dcap);

    // 오브젝트 배열에서 제거 (메모리 해제 X)
    gameObjects[objIdx] = nullptr;
    gameObjects.Free(objIdx);
}

int Zone::AddPlayer(int clientIndex, Player* player, vec4 spawnPos) {

    cout << "[Zone " << zoneId << "] AddPlayer clientIndex=" << clientIndex
        << " objCount=" << gameObjects.size << endl;

    // 위치 설정
    player->m_worldMatrix.pos = spawnPos;
    player->isExist = true;

    // 오브젝트 배열에 추가
    int newIdx = gameObjects.Alloc();
    gameObjects[newIdx] = player;

    // 클라이언트 정보 업데이트
    world->clients[clientIndex].objindex = newIdx;
    world->clients[clientIndex].zoneId = this->zoneId;

    // 이 존의 다른 클라이언트들에게 알림
    int dcap = world->Sending_NewGameObject(newIdx, player, GameObjectType::_Player);
    SendToAll_except(dcap, clientIndex);

    if (player->ShapeIndex >= 0) {
        dcap = world->Sending_SetMeshInGameObject(newIdx, Shape::ShapeNameArr[player->ShapeIndex]);
        SendToAll_except(dcap, clientIndex);
    }

    // pack_factory로 존 데이터 전송
    world->SendingAllObjectForNewClient(this->zoneId);

    // AllocPlayerIndex도 pack_factory에 추가
    int objIdx = world->clients[clientIndex].objindex;
    dcap = world->Sending_AllocPlayerIndex(clientIndex, objIdx);
    world->pack_factory.push_data(dcap, world->tempbuffer.data);

    // 한 번에 전송
    world->pack_factory.send(&world->clients[clientIndex].socket);

    return newIdx;
}

void Zone::GridCollisionCheck()
{
    const float radius = 0.2f;
    const float groundY = 0.0f;

    for (AstarNode* node : allnodes
        )
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

void Zone::CheckPortalCollision(Player* p) {
    if (!p) return;

    vec4 playerPos = p->m_worldMatrix.pos;

    for (int i = 0; i < gameObjects.size; ++i) {
        if (gameObjects.isnull(i)) continue;
        GameObject* obj = gameObjects[i];
        if (!obj || !obj->isExist) continue;

        void* vptr = *(void**)obj;
        if (GameObjectType::VptrToTypeTable[vptr] != GameObjectType::_Portal)
            continue;

        Portal* portal = static_cast<Portal*>(obj);
        vec4 portalPos = portal->m_worldMatrix.pos;

        float dx = playerPos.x - portalPos.x;
        float dy = playerPos.y - portalPos.y;
        float dz = playerPos.z - portalPos.z;
        float dist2 = dx * dx + dy * dy + dz * dz;
        float r = portal->radius;

        if (dist2 <= r * r) {
            vec4 spawnPos(portal->spawnX, portal->spawnY, portal->spawnZ, 1.0f);
            int ci = p->clientIndex;

            cout << "[Portal] client=" << ci
                << " entering portal, dst=" << portal->dstzoneId << endl;

            // 1) 클라이언트에게 존 변경 패킷
            world->SendPlayerDatatoOtherZone(ci, portal->dstzoneId, spawnPos);

            // 2) 서버에서 존 이동
            world->MovePlayerToZone(ci, portal->dstzoneId, spawnPos);

            return;
        }
    }
}
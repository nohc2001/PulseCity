// Zone.cpp
#include "stdafx.h"
//#include "Zone.h"
#include "main.h"

Shape& Zone::GetShape(int shapeindex)
{
    if (Shape::ShapeTable.size() > shapeindex) {
        return Shape::ShapeTable[shapeindex];
    }
    else {
        int zshapeindex = shapeindex - Shape::ShapeTable.size();
        return ZoneShapeTable[zshapeindex];
    }
}

void Zone::Init() {
    /*if (zoneId != 0) {
        Dynamic_gameObjects.Init(128);
        Static_gameObjects.Init(128);
        DropedItems.Init(4096);
        CommonSDS.Init(4096);
        map.ownerzone = this;
        cout << "Zone " << zoneId << " Init end (no map)" << endl;
        return;
    }*/

    // Astar �׸��� �ʱ�ȭ
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

    // �迭 �ʱ�ȭ
    Dynamic_gameObjects.Init(128);
    Static_gameObjects.Init(128);
    DropedItems.Init(4096);
    CommonSDS.Init(4096);

    //�� ���� �� ���� ���� �ִ�
    map.ownerzone = this;

    // �� �ε�
    LoadMapForZone(zoneId);
    if (map.MapObjects.empty()) {
        cout << "Zone " << zoneId << " map load failed!" << endl;
        return;
    }

    map.AABB[0] = INFINITY;
    map.AABB[1] = -INFINITY;
    for (int i = 0; i < map.MapObjects.size(); ++i) {
        PushGameObject(map.MapObjects[i]);
        BoundingOrientedBox obb = map.MapObjects[i]->GetOBB();
        XMFLOAT3 corners[8];
        obb.GetCorners(corners);
        for (int k = 0; k < 8; ++k) {
            vec4 c = corners[k];
            map.AABB[0] = _mm_min_ps(c, map.AABB[0]);
            map.AABB[1] = _mm_max_ps(c, map.AABB[1]);
        }
    }
    map.AABB[0].w = -INFINITY;
    map.AABB[1].w = INFINITY;

    // === DEBUG : Zone ¸Ê AABB ·Î±× ===
    cout << "[Zone " << zoneId << "] map=" << gameworld.ZoneMapName[zoneId]
        << " AABB.min=(" << map.AABB[0].f3.x << ", " << map.AABB[0].f3.y << ", " << map.AABB[0].f3.z << ")"
        << " AABB.max=(" << map.AABB[1].f3.x << ", " << map.AABB[1].f3.y << ", " << map.AABB[1].f3.z << ")"
        << " size=(" << (map.AABB[1].f3.x - map.AABB[0].f3.x) << ", "
                     << (map.AABB[1].f3.y - map.AABB[0].f3.y) << ", "
                     << (map.AABB[1].f3.z - map.AABB[0].f3.z) << ")"
        << " MapObjectCount=" << map.MapObjects.size() << endl;

    // ������Ʈ ����
    //SpawnObjects();

    // �׸��� �浹 üũ
    GridCollisionCheck();

    // ��Ż ����
    //SpawnPortal();

    cout << "Zone " << zoneId << " Init end" << endl;
}

void Zone::Update(float deltaTime) {
    // �� ���� ������Ʈ
    lowFrequencyFlow += deltaTime;
    midFrequencyFlow += deltaTime;

    static vector<indexRange> ir;
    if (ir.size() < Dynamic_gameObjects.size / 2 + 1) {
        ir.reserve(Dynamic_gameObjects.size / 2 + 1);
        ir.resize(Dynamic_gameObjects.size / 2 + 1);
    }
    int outlen = 0;
    Dynamic_gameObjects.GetTourPairs(ir.data(), &outlen);

    // Dynamic ������Ʈ ������Ʈ
    for (int ri = 0; ri < outlen; ++ri) {
        for (currentIndex = ir[ri].start; currentIndex <= ir[ri].end; ++currentIndex) {
            if (Dynamic_gameObjects[currentIndex]->tag[GameObjectTag::Tag_Enable] == false) continue;
            Dynamic_gameObjects[currentIndex]->Update(DeltaTime);
        }
    }

    // �浹 ó�� (Dynamic ������Ʈ ��)
    {
        TourID += 1;
        for (auto& ch : chunck) {
            ChunkIndex ci = ch.first;
            GameChunk* c = ch.second;
            if (c->Dynamic_gameobjects.size + c->SkinMesh_gameobjects.size <= 0) continue;

            // Tour�� ���� IndexRange ����
            int dn = (c->Dynamic_gameobjects.size / 2) + 1;
            int sn = (c->SkinMesh_gameobjects.size / 2) + 1;
            if (c->IR_Dynamic.size() < dn) {
                c->IR_Dynamic.reserve(dn);
                c->IR_Dynamic.resize(dn);
            }
            if (c->IR_SkinMesh.size() < sn) {
                c->IR_SkinMesh.reserve(sn);
                c->IR_SkinMesh.resize(sn);
            }
            c->Dynamic_gameobjects.GetTourPairs(c->IR_Dynamic.data(), &c->dynamicIRSiz);
            c->SkinMesh_gameobjects.GetTourPairs(c->IR_SkinMesh.data(), &c->SkinMeshIRSiz);
            //cout << "SkinMeshAlloter : " << c->SkinMesh_gameobjects.Alloter.AllocFlag[0] << " : start : " << c->IR_SkinMesh[0].start << " : end : " << c->IR_SkinMesh[0].end << endl;
        }

        for (int ri = 0; ri < outlen; ++ri) {
            for (int i = ir[ri].start; i <= ir[ri].end; ++i) {
                DynamicGameObject* gbj1 = Dynamic_gameObjects[i];
                if (gbj1->tag[GameObjectTag::Tag_Enable] == false) continue;
                if (gbj1->tickLVelocity.fast_square_of_len3 <= 0.001f) continue;
                dbgWarn(gbj1->chunkAllocIndexs == nullptr,
                    cout << "Zone" << zoneId << " WARN - DynamicObject_" << i << " �� ���ԵǴ� ûũ�� ���� �����Ͱ� �����ϴ�.\n";
                    continue;)

                //Shape�κ��� OBB������ �޴´�.
                BoundingOrientedBox obb = gbj1->GetOBB();
                float fsl1 = vec4(obb.Extents).fast_square_of_len3;

                BoundingOrientedBox obb_before = gbj1->GetOBB();
                vec4 lastpos1 = gbj1->worldMat.pos + gbj1->tickLVelocity;
                gbj1->worldMat.pos += gbj1->tickLVelocity;
                BoundingOrientedBox obb_after = gbj1->GetOBB();
                gbj1->worldMat.pos -= gbj1->tickLVelocity;
                GameObjectIncludeChunks goic_before = GetChunks_Include_OBB(obb_before);
                GameObjectIncludeChunks goic_after = GetChunks_Include_OBB(obb_after);
                GameObjectIncludeChunks goic_sum = goic_before;
                goic_sum += goic_after;

                ChunkIndex ci = ChunkIndex(goic_sum.xmin, goic_sum.ymin, goic_sum.zmin);
                ci.extra = 0;
                int chunckCount = goic_sum.GetChunckSize();
                for (; ci.extra < chunckCount; goic_sum.Inc(ci)) {
                    auto ch = chunck.find(ci);
                    if (ch != chunck.end()) {
                        GameChunk* c = ch->second;
                        for (int u = 0; u < c->Dynamic_gameobjects.size; ++u) {
                            if (c->Dynamic_gameobjects.isnull(u)) continue;
                            DynamicGameObject* gbj2 = c->Dynamic_gameobjects[u];
                            if (gbj2->tag[GameObjectTag::Tag_Enable] == false) continue;
                            if (gbj2 == gbj1) continue;

                            BoundingOrientedBox obb2 = gbj2->GetOBB();
                            float fsl2 = vec4(obb2.Extents).fast_square_of_len3;
                            vec4 dist = lastpos1 - (gbj2->worldMat.pos + gbj2->tickLVelocity);
                            if (fsl1 + fsl2 > dist.fast_square_of_len3) {
                                DynamicGameObject::CollisionMove(gbj1, gbj2);

                                if (gbj1->tickLVelocity.fast_square_of_len3 <= 0.001f) {
                                    // ���� ������Ʈ�� ���������� �Ѿ��.
                                    goto GOTO_NEXTOBJ;
                                }
                            }
                        }
                        for (int u = 0; u < c->SkinMesh_gameobjects.size; ++u) {
                            if (c->SkinMesh_gameobjects.isnull(u)) continue;
                            SkinMeshGameObject* gbj2 = c->SkinMesh_gameobjects[u];
                            if (gbj2->tag[GameObjectTag::Tag_Enable] == false) continue;
                            if (gbj2 == gbj1) continue;
                            BoundingOrientedBox obb2 = gbj2->GetOBB();
                            float fsl2 = vec4(obb2.Extents).fast_square_of_len3;
                            vec4 dist = lastpos1 - (gbj2->worldMat.pos + gbj2->tickLVelocity);
                            if (fsl1 + fsl2 > dist.fast_square_of_len3) {
                                DynamicGameObject::CollisionMove(gbj1, gbj2);

                                if (gbj1->tickLVelocity.fast_square_of_len3 <= 0.001f) {
                                    goto GOTO_NEXTOBJ;
                                }
                            }
                        }

                        for (int k = 0; k < c->Static_gameobjects.size; ++k) {
                            StaticGameObject* sgo = c->Static_gameobjects[k];

                            if (sgo->obbArr.size() == 0) {
                                ////Static Object에서 obb 정보가 없으면 따로 충돌체를 만들지 않게 조정한다.
                                //BoundingOrientedBox staticobb = c->Static_gameobjects[k]->GetOBB();

                                //if (obb_after.Intersects(staticobb)) {
                                //    gbj1->OnStaticCollision(staticobb);
                                //    DynamicGameObject::CollisionMove_DivideBaseline_StaticOBB(gbj1, staticobb);

                                //    if (gbj1->tickLVelocity.fast_square_of_len3 <= 0.001f) {
                                //        goto GOTO_NEXTOBJ;
                                //    }
                                //}
                            }
                            else {
                                for (int u = 0; u < sgo->obbArr.size(); ++u) {
                                    BoundingOrientedBox staticobb = sgo->obbArr[u];
                                    if (obb_after.Intersects(staticobb)) {
                                        gbj1->OnStaticCollision(staticobb);
                                        DynamicGameObject::CollisionMove_DivideBaseline_StaticOBB(gbj1, staticobb);

                                        if (gbj1->tickLVelocity.fast_square_of_len3 <= 0.001f) {
                                            goto GOTO_NEXTOBJ;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                if (gbj1->tickLVelocity.fast_square_of_len3 <= 0.001f) {
                GOTO_NEXTOBJ:
                    gbj1->tickLVelocity = XMVectorZero();
                    continue;
                }

                // �Ƹ� �浹ó���ϸ鼭 tickVelocity�� �پ����� ���� ����.
                // ������ after������ �޶�����.
                gbj1->worldMat.pos += gbj1->tickLVelocity;
                obb_after = gbj1->GetOBB();
                gbj1->worldMat.pos -= gbj1->tickLVelocity;
                goic_after = GetChunks_Include_OBB(obb_after);

                if (goic_before == goic_after) {
                    gbj1->worldMat.pos += gbj1->tickLVelocity;
                }
                else {
                    vec4 pos = gbj1->worldMat.pos + gbj1->tickLVelocity;
                    vec4 minpos = _mm_min_ps(pos, map.AABB[0]);
                    vec4 maxpos = _mm_max_ps(pos, map.AABB[1]);
                    if (map.AABB[0] == minpos &&
                        map.AABB[1] == maxpos) {
                        gbj1->MoveChunck(gbj1->tickLVelocity, vec4(0, 0, 0, 1), goic_before, goic_after);
                    }
                    else {
                        gbj1->tickLVelocity = 0;
                    }
                }
                //gbj1->worldMat.pos += gbj1->tickLVelocity;
                /*if (fabsf(gbj1->m_worldMatrix.pos.x) > 40.0f || fabsf(gbj1->m_worldMatrix.pos.z) > 40.0f) {
                    gbj1->m_worldMatrix.pos -= gbj1->tickLVelocity;
                }*/
                gbj1->tickLVelocity = XMVectorZero();

                //dbgbreak(GameObjectType::VptrToTypeTable[*(void**)gbj1] == GameObjectType::_Player);
                // send matrix to client
                Sending_ChangeGameObjectMember<matrix>(CommonSDS, i, gbj1, GameObjectType::VptrToTypeTable[*(void**)gbj1], &gbj1->worldMat);
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

    //for (int i = 0; i < Dynamic_gameObjects.size; ++i) {
    //    if (Dynamic_gameObjects.isnull(i)) continue;
    //    DynamicGameObject* gbj1 = Dynamic_gameObjects[i];
    //    if (!(gbj1->tag[Tag_Enable])) continue;

    //    if (gbj1->tickLVelocity.fast_square_of_len3 > 0.001f) {
    //        gbj1->worldMat.pos += gbj1->tickLVelocity;
    //        gbj1->tickLVelocity = XMVectorZero();

    //        // Ŭ���̾�Ʈ�� ��ġ ����
    //        Sending_ChangeGameObjectMember<matrix>(CommonSDS, i, gbj1,
    //            GameObjectType::VptrToTypeTable[*(void**)gbj1], &gbj1->worldMat);
    //    }
    //}

    // ��Ż �˻� - �� Zone�� ���� Ŭ���̾�Ʈ�鸸
    for (int i = 0; i < gameworld.clients.size; ++i) {
        if (gameworld.clients.isnull(i)) continue;
        if (gameworld.clients[i].zoneId != this->zoneId) continue;
        Player* p = (Player*)gameworld.clients[i].pObjData;
        if (p) CheckBoundaryCrossing(p, deltaTime);
    }

    // �� ���� ����
    if (lowFrequencyFlow > lowFrequencyDelay) lowFrequencyFlow = 0.0f;
    if (midFrequencyFlow > midFrequencyDelay) midFrequencyFlow = 0.0f;

    static int debugCount = 0;
    if (debugCount < 3) {
        for (int i = 0; i < Dynamic_gameObjects.size; ++i) {
            if (Dynamic_gameObjects.isnull(i)) continue;
            DynamicGameObject* obj = Dynamic_gameObjects[i];
            printf("[Zone%d obj%d] y=%f\n", zoneId, i, obj->worldMat.pos.y);
        }
        debugCount++;
    }
}

//������Ʈ ����
int Zone::NewObject(DynamicGameObject* obj, GameObjectType gotype) {
    int newindex = Dynamic_gameObjects.Alloc();
    Dynamic_gameObjects[newindex] = obj;
    obj->tag[GameObjectTag::Tag_Enable] = true;

    // CommonSDS�� ��Ŷ �ױ�
    Sending_NewGameObject(CommonSDS, newindex, obj);

    return newindex;
}

int Zone::NewPlayer(SendDataSaver& sds, Player* obj, int clientIndex) {
    int newindex = Dynamic_gameObjects.Alloc();
    Dynamic_gameObjects[newindex] = obj;
    obj->tag[GameObjectTag::Tag_Enable] = true;

    // ���� Ŭ���̾�Ʈ�鿡�Դ� CommonSDS��
    obj->SendGameObject(newindex, CommonSDS);

    return newindex;
}

void Zone::RemovePlayer(int clientIndex) {
    Zone* zone = gameworld.GetClientZone(clientIndex);
    int objindex = gameworld.clients[clientIndex].objindex;
    Player* go = gameworld.clients[clientIndex].pObjData;

    // ���� Zone�� ûũ���� �� �÷��̾ ����.
    int n = 0;
    ChunkIndex ci = ChunkIndex(go->IncludeChunks.xmin, go->IncludeChunks.ymin, go->IncludeChunks.zmin);
    ci.extra = 0;
    int chunckCount = go->IncludeChunks.GetChunckSize();
    for (; ci.extra < chunckCount; go->IncludeChunks.Inc(ci)) {
        auto f = zone->chunck.find(ci);
        if (f != zone->chunck.end()) {
            GameChunk* c = f->second;
            if (c->SkinMesh_gameobjects.isAlloc(go->chunkAllocIndexs[ci.extra]) == false) cout << "[WARN] RemovePlayer invalid skinmesh alloc index\n";
            if (c->SkinMesh_gameobjects.isAlloc(go->chunkAllocIndexs[ci.extra])) c->SkinMesh_gameobjects.Free(go->chunkAllocIndexs[ci.extra]);
        }
    }

    // �� ���� Ŭ���̾�Ʈ�鿡�� ���� �˸�
    Sending_DeleteGameObject(CommonSDS, objindex);

    // ������Ʈ �迭���� ���� (�޸� ���� X)
    Dynamic_gameObjects[objindex] = nullptr;
    Dynamic_gameObjects.Free(objindex);
}

int Zone::AddPlayer(int clientIndex, Player* player, vec4 spawnPos, bool update_Map) {
    cout << "[Zone " << zoneId << "] AddPlayer clientIndex=" << clientIndex << endl;

    //�� ����
    player->zone = this;

    // ��ġ ����
    player->worldMat.pos = spawnPos;
    player->tag[Tag_Enable] = true;

    // ������Ʈ �迭�� �߰�
    int newIdx = Dynamic_gameObjects.Alloc();
    Dynamic_gameObjects[newIdx] = player;

    // Ŭ���̾�Ʈ ���� ������Ʈ
    gameworld.clients[clientIndex].objindex = newIdx;
    gameworld.clients[clientIndex].zoneId = this->zoneId;
    cout << "[Zone " << zoneId << "] Player final pos = ("
        << player->worldMat.pos.f3.x << ", "
        << player->worldMat.pos.f3.y << ", "
        << player->worldMat.pos.f3.z << ")" << endl;


    // �� ���� �ٸ� Ŭ���̾�Ʈ�鿡�� �� �÷��̾� �˸� (CommonSDS)
    player->SendGameObject(newIdx, CommonSDS);

    // �� Ŭ���̾�Ʈ���� �� ���� ���?������ ���� (PersonalSDS)
    SendDataSaver& personalSDS = gameworld.clients[clientIndex].PersonalSDS;

    if (update_Map) {
        //// Ŭ���̾�Ʈ�� Zone �̵��� �� �� �ֵ��� ��Ŷ�� �����Ѵ�. (Ŭ���̾�Ʈ �� �ε��� ����)
        gameworld.Sending_PlayerMoveZone(personalSDS, clientIndex, zoneId);
    }

    // �� Ŭ���̾�Ʈ���� Zone�� �ִ� ���?������Ʈ�� ������ ����.
    SendingAllObjectForNewClient(personalSDS);

    // AllocPlayerIndex ����
    gameworld.Sending_AllocPlayerIndex(personalSDS, clientIndex, newIdx);

    // 플레이어 인벤토리 정보를 전송
    // 이때 필요하면 데이터베이스에서 플레이어 정보 얻기..
    for (int i = 0; i < player->maxItem; ++i) {
        Sending_InventoryItemSync(personalSDS, player->Inventory[i], i);
    }

    PushGameObject(player);
    return newIdx;
}

//��Ż ����
void Zone::SpawnPortal() {
    Portal* portal = new Portal();

    // "Portal" Shape�� ������ shapeindex�� -1�� ��
    auto it = Shape::StrToShapeIndex.find("Portal");
    if (it != Shape::StrToShapeIndex.end()) {
        portal->SetShape(it->second);
    }

    portal->zoneId = this->zoneId;

    if (zoneId == 0) {
        // Debug boundary visual : mid-zone gate for Zone 0 -> Zone 1.
        portal->worldMat =
            XMMatrixScaling(8.0f, 4.0f, 1.0f) *
            XMMatrixTranslation(0.0f, 2.0f, 24.0f);
        portal->dstzoneId = 1;
        portal->spawnX = 0.0f;
        portal->spawnY = 2.0f;
        portal->spawnZ = -4.0f;
        portal->radius = 4.0f;
    }
    else if (zoneId == 1) {
        // Debug boundary visual : mid-zone gate for Zone 1 -> Zone 0.
        portal->worldMat =
            XMMatrixScaling(8.0f, 4.0f, 1.0f) *
            XMMatrixTranslation(0.0f, 2.0f, -24.0f);
        portal->dstzoneId = 0;
        portal->spawnX = 0.0f;
        portal->spawnY = 2.0f;
        portal->spawnZ = 4.0f;
        portal->radius = 4.0f;
    }

    portal->tag[Tag_Enable] = true;
    portals.push_back(portal);
    cout << "[Zone " << zoneId << "] SpawnPortal dstzoneId=" << portal->dstzoneId
        << " pos=(" << portal->worldMat.pos.f3.x << ", " << portal->worldMat.pos.f3.y << ", " << portal->worldMat.pos.f3.z << ")" << endl;
}

void Zone::Spawnboundary()
{
    boundaries.clear();
    if (zoneId == 0) {
        Zoneboundary boundary{};
        boundary.basezoneId = 0;
        boundary.dstzoneId = 1;

        // Mid-zone debug gate for Zone 0.
        boundary.minPos = vec4(-4.0f, -100.0f, 33.0f, 1.0f);
        boundary.maxPos = vec4(4.0f, 100.0f, 35.0f, 1.0f);

        // Zone 1 spawn point after crossing the mid-zone gate
        boundary.spawnPos = vec4(0.0f, 0.0f, 2.0f, 1.0f);
        boundary.spawnYaw = XM_PIDIV2;  // face +X

        boundary.cooldownSec = 0.5f;
        boundary.enabled = true;

        boundaries.push_back(boundary);
    }
    else if (zoneId == 1) {
        Zoneboundary boundary{};
        boundary.basezoneId = 1;
        boundary.dstzoneId = 0;

        // Mid-zone debug gate for Zone 1.
        boundary.minPos = vec4(-4.0f, -100.0f, -2.0f, 1.0f);
        boundary.maxPos = vec4(-2.0f, 100.0f, 6.0f, 1.0f);

        // Zone 0 spawn point after crossing the mid-zone gate
        boundary.spawnPos = vec4(0.0f, 1.5f, 3.0f, 1.0f);
        boundary.spawnYaw = XM_PIDIV2;  // face +X (opposite of -X entry)

        boundary.cooldownSec = 0.5f;
        boundary.enabled = true;

        boundaries.push_back(boundary);
    }

    for (int i = 0; i < boundaries.size(); ++i) {
        Zoneboundary& boundary = boundaries[i];
        cout << "[Zone " << zoneId << "] Boundary created idx=" << i
            << " src=" << boundary.basezoneId
            << " dst=" << boundary.dstzoneId
            << " min=(" << boundary.minPos.f3.x << ", " << boundary.minPos.f3.y << ", " << boundary.minPos.f3.z << ")"
            << " max=(" << boundary.maxPos.f3.x << ", " << boundary.maxPos.f3.y << ", " << boundary.maxPos.f3.z << ")"
            << " spawn=(" << boundary.spawnPos.f3.x << ", " << boundary.spawnPos.f3.y << ", " << boundary.spawnPos.f3.z << ")"
            << " cooldown=" << boundary.cooldownSec
            << " enabled=" << boundary.enabled << endl;
    }

    cout << "[Zone " << zoneId << "] Total boundaries = " << boundaries.size() << endl;
}


void Zone::CheckPortalCollision(Player* p) {
    if (!p) return;

    vec4 playerPos = p->worldMat.pos;

    for (int i = 0; i < portals.size(); ++i) {
        Portal* portal = portals[i];
        vec4 portalPos = portal->worldMat.pos;

        float dx = playerPos.f3.x - portalPos.f3.x;
        float dy = playerPos.f3.y - portalPos.f3.y;
        float dz = playerPos.f3.z - portalPos.f3.z;
        float dist2 = dx * dx + dy * dy + dz * dz;
        float r = portal->radius;

        if (dist2 <= r * r) {
            vec4 spawnPos(portal->spawnX, portal->spawnY, portal->spawnZ, 1.0f);
            int ci = p->clientIndex;

            // World���� �� �̵��� ��û
            gameworld.MovePlayerToZone(ci, portal->dstzoneId, spawnPos);

            return;
        }
    }
}

void Zone::CheckBoundaryCrossing(Player* p, float deltaTime)
{
    if (p == nullptr) return;

    if (p->zoneMoveCooldownRemain > 0.0f) {
        p->zoneMoveCooldownRemain -= deltaTime;
        if (p->zoneMoveCooldownRemain < 0.0f) {
            p->zoneMoveCooldownRemain = 0.0f;
        }
    }

    vec4 playerPos = p->worldMat.pos;

    int currentBoundaryIndex = -1;

    for (int i = 0; i < boundaries.size(); ++i) {
        Zoneboundary& boundary = boundaries[i];
        if (boundary.enabled == false) continue;

        bool inside =
            playerPos.f3.x >= boundary.minPos.f3.x &&
            playerPos.f3.x <= boundary.maxPos.f3.x &&
            playerPos.f3.y >= boundary.minPos.f3.y &&
            playerPos.f3.y <= boundary.maxPos.f3.y &&
            playerPos.f3.z >= boundary.minPos.f3.z &&
            playerPos.f3.z <= boundary.maxPos.f3.z;

        if (inside) {
            currentBoundaryIndex = i;
            break;
        }
    }

    if (currentBoundaryIndex == -1) {
        p->wasInsideBoundary = false;
        p->lastBoundaryIndex = -1;
        return;
    }

    if (p->zoneMoveCooldownRemain > 0.0f) {
        p->wasInsideBoundary = true;
        p->lastBoundaryIndex = currentBoundaryIndex;
        return;
    }

    bool isNewEnter =
        (p->wasInsideBoundary == false) ||
        (p->lastBoundaryIndex != currentBoundaryIndex);

    if (isNewEnter) {
        Zoneboundary& boundary = boundaries[currentBoundaryIndex];

        p->m_yaw = boundary.spawnYaw;
        p->m_pitch = 0.0f;
        gameworld.MovePlayerToZone(
            p->clientIndex,
            boundary.dstzoneId,
            boundary.spawnPos
        );

        p->zoneMoveCooldownRemain = boundary.cooldownSec;
        p->wasInsideBoundary = true;
        p->lastBoundaryIndex = currentBoundaryIndex;
        return;
    }

    p->wasInsideBoundary = true;
    p->lastBoundaryIndex = currentBoundaryIndex;
}

void Zone::FlushSendToClients() {
    bool keepCommonSDS = false;
    for (int i = 0; i < gameworld.clients.size; ++i) {
        if (gameworld.clients.isnull(i)) continue;
        if (gameworld.clients[i].zoneId != this->zoneId) continue;
        bool isTransferring = (gameworld.clients[i].pObjData == nullptr);
        if (gameworld.clients[i].PersonalSDS.size <= 0 && (CommonSDS.size <= 0 || isTransferring)) continue;

        WSABUF sendbuf[2];
        DWORD sendbufCount = 0;
        if (gameworld.clients[i].PersonalSDS.size > 0) {
            sendbuf[sendbufCount].buf = gameworld.clients[i].PersonalSDS.buffer;
            sendbuf[sendbufCount].len = gameworld.clients[i].PersonalSDS.size;
            sendbufCount += 1;
        }
        if (CommonSDS.size > 0 && isTransferring == false) {
            sendbuf[sendbufCount].buf = CommonSDS.buffer;
            sendbuf[sendbufCount].len = CommonSDS.size;
            sendbufCount += 1;
        }

        DWORD retval = 0;
        int err = WSASend(gameworld.clients[i].socket, sendbuf, sendbufCount, &retval, 0, NULL, NULL);
        if (err == SOCKET_ERROR) {
            int wsaErr = WSAGetLastError();
            if (wsaErr == WSAEWOULDBLOCK) {
                keepCommonSDS = true;
                continue;
            }
            ClientData::DisconnectToServer(i);
            continue;
        }

        gameworld.clients[i].PersonalSDS.Clear();
    }

    if (keepCommonSDS == false) {
        CommonSDS.Clear();
    }
}

//void Zone::SendZoneDataToClient(SendDataSaver& sds) {
//    // �� ���� ���?Dynamic ������Ʈ ����
//    for (int i = 0; i < Dynamic_gameObjects.size; ++i) {
//        if (Dynamic_gameObjects.isnull(i)) continue;
//        Dynamic_gameObjects[i]->SendGameObject(i, sds);
//    }
//
//    // ���?������ ����
//    for (int i = 0; i < DropedItems.size; ++i) {
//        if (DropedItems.isnull(i)) continue;
//        // Sending_ItemDrop to personal SDS
//        sds.postpush_start();
//        constexpr int reqsiz = sizeof(STC_ItemDrop_Header);
//        sds.postpush_reserve(reqsiz);
//        STC_ItemDrop_Header& header = *(STC_ItemDrop_Header*)sds.ofbuff;
//        header.size = reqsiz;
//        header.st = SendingType::ItemDrop;
//        header.dropindex = i;
//        header.lootData = DropedItems[i];
//        sds.postpush_end();
//    }
//}

//Sending 

void Zone::SendingAllObjectForNewClient(SendDataSaver& sds)
{
    for (int i = 0; i < Dynamic_gameObjects.size; ++i) {
        if (Dynamic_gameObjects.isnull(i)) continue;
        Sending_NewGameObject(sds, i, (GameObject*)Dynamic_gameObjects[i]);
    }

    for (int i = 0; i < DropedItems.size; ++i) {
        if (DropedItems.isnull(i)) continue;
        Sending_ItemDrop(sds, i, DropedItems[i]);
    }

    for (int i = 0; i < portals.size(); ++i) {
        portals[i]->SendGameObject(i, sds);
    }
}

void Zone::Sending_NewGameObject(SendDataSaver& sds, int newindex, GameObject* newobj) {
    newobj->SendGameObject(newindex, sds);
}

void Zone::Sending_DeleteGameObject(SendDataSaver& sds, int objindex) {
    sds.postpush_start();
    constexpr int reqsiz = sizeof(STC_DeleteGameObject_Header);
    sds.postpush_reserve(reqsiz);
    STC_DeleteGameObject_Header& header = *(STC_DeleteGameObject_Header*)sds.ofbuff;
    header.size = reqsiz;
    header.st = STC_Protocol::DeleteGameObject;
    header.obj_index = objindex;
    sds.postpush_end();
}

void Zone::Sending_ItemDrop(SendDataSaver& sds, int dropindex, ItemLoot lootdata) {
    sds.postpush_start();
    constexpr int reqsiz = sizeof(STC_ItemDrop_Header);
    sds.postpush_reserve(reqsiz);
    STC_ItemDrop_Header& header = *(STC_ItemDrop_Header*)sds.ofbuff;
    header.size = reqsiz;
    header.st = STC_Protocol::ItemDrop;
    header.dropindex = dropindex;
    header.lootData = lootdata;
    sds.postpush_end();
}

void Zone::Sending_ItemRemove(SendDataSaver& sds, int dropindex) {
    sds.postpush_start();
    constexpr int reqsiz = sizeof(STC_ItemDropRemove_Header);
    sds.postpush_reserve(reqsiz);
    STC_ItemDropRemove_Header& header = *(STC_ItemDropRemove_Header*)sds.ofbuff;
    header.size = reqsiz;
    header.st = STC_Protocol::ItemDropRemove;
    header.dropindex = dropindex;
    sds.postpush_end();
}

//��/����
void Zone::LoadMapForZone(int zid) {
    if (zid < gameworld.zoneCount) {
        map.LoadMap(gameworld.ZoneMapName[zid]);
    }
    else {
        map.LoadMap(gameworld.ZoneMapName[0]);
    }
}

void Zone::SpawnObjects() {
    // �ٲ� ������ �°� ������Ʈ ����
    // Static ������Ʈ ����
    // TODO: ���� Shape �̸��� �ٲ� ������ �´��� Ȯ�� �ʿ�

    // ���� ���� (zoneId == 1 �̸� ��ŵ)
    if (zoneId == 1) return;

    static constexpr int range = 100;
    for (int i = 0; i < 20; ++i) {
        Monster* mon = new Monster();
        mon->zone = this;
        mon->SetShape(Shape::StrToShapeIndex["Monster001"]);
        // ���� ������ ���̰� ���̸� �ø�
        mon->Init(XMMatrixTranslation((float)(rand() % range - (range/2)), 10.0f, (float)(rand() % range - (range / 2))));
        while (map.isStaticCollision(mon->GetOBB())) {
            mon->Init(XMMatrixTranslation((float)(rand() % range - (range / 2)), 10.0f, (float)((rand() % range - (range / 2)))));
        }

        NewObject(mon, GameObjectType::_Monster);
        PushGameObject(mon);
    }
}

void Zone::GridCollisionCheck() {
    const float radius = 0.2f;
    const float groundY = 0.0f;

    for (AstarNode* node : allnodes) {
        vec4 nodePos(node->worldx, groundY, node->worldz);
        collisionchecksphere sphere = { nodePos, radius };
        BoundingOrientedBox obb = BoundingOrientedBox(nodePos.f3, { radius , radius , radius }, vec4(0, 0, 0, 1));
        bool blocked = false;

        GameObjectIncludeChunks goic = GetChunks_Include_OBB(obb);
        ChunkIndex ci = ChunkIndex(goic.xmin, goic.ymin, goic.zmin);
        int chunckLen = goic.GetChunckSize();
        for (; ci.extra < chunckLen; goic.Inc(ci)) {
            auto f = chunck.find(ci);
            if (f != chunck.end()) {
                GameChunk* gc = f->second;
                for (int i = 0; i < gc->Static_gameobjects.size; ++i) {
                    StaticGameObject* obj = Static_gameObjects[i];
                    BoundingOrientedBox sobb = obj->GetOBB();
                    if (sobb.Intersects(obb)) {
                        blocked = true;
                        goto COLLISION_CHECK_END;
                    }
                }
            }
        }
        COLLISION_CHECK_END:

        //// Static ������Ʈ�� �浹 �˻�
        //for (int i = 0; i < Static_gameObjects.size; ++i) {
        //    if (Static_gameObjects.isnull(i)) continue;
        //    StaticGameObject* obj = Static_gameObjects[i];

        //    int wallShapeIdx = Shape::StrToShapeIndex["Wall001"];
        //    if (obj->shapeindex == wallShapeIdx) {
        //        vec4 wallCenter = obj->worldMat.pos;
        //        vec4 wallHalfSize(2.5f, 1.0f, 0.5f);

        //        if (CheckAABBSphereCollision(wallCenter, wallHalfSize, sphere)) {
        //            blocked = true;
        //            break;
        //        }
        //    }
        //}
        node->cango = !blocked;
    }
}

void Zone::FireRaycast(GameObject* shooter, vec4 rayStart, vec4 rayDirection, float rayDistance, float damage) {
    vec4 rayOrigin = rayStart;
    float closestDistance = rayDistance;

    int lastcurrentindex = currentIndex;

    for (int i = 0; i < Dynamic_gameObjects.size; ++i) {
        if (Dynamic_gameObjects.isnull(i)) continue;
        if (shooter == (GameObject*)Dynamic_gameObjects[i]) continue;

        currentIndex = i;
        BoundingOrientedBox obb = Dynamic_gameObjects[i]->GetOBB();
        float distance;

        if (obb.Intersects(rayOrigin, rayDirection, distance)) {
            if (distance < closestDistance) {
                closestDistance = distance;
                Dynamic_gameObjects[i]->OnCollisionRayWithBullet(shooter, damage);
            }
        }
    }

    currentIndex = lastcurrentindex;

    // �� ���� Ŭ���̾�Ʈ�鿡�� Ray ����
    CommonSDS.postpush_start();
    constexpr int reqsiz = sizeof(STC_NewRay_Header);
    CommonSDS.postpush_reserve(reqsiz);
    STC_NewRay_Header& header = *(STC_NewRay_Header*)CommonSDS.ofbuff;
    header.size = reqsiz;
    header.st = STC_Protocol::NewRay;
    header.raystart = XMFLOAT3(rayStart.f3.x, rayStart.f3.y, rayStart.f3.z);
    header.rayDir = XMFLOAT3(rayDirection.f3.x, rayDirection.f3.y, rayDirection.f3.z);
    header.distance = closestDistance;
    CommonSDS.postpush_end();
}

bool Zone::CheckAABBSphereCollision(const vec4& boxCenter, const vec4& boxHalfSize, const collisionchecksphere& sphere) {
    float dx = max(0.0f, abs(sphere.center.f3.x - boxCenter.f3.x) - boxHalfSize.f3.x);
    float dy = max(0.0f, abs(sphere.center.f3.y - boxCenter.f3.y) - boxHalfSize.f3.y);
    float dz = max(0.0f, abs(sphere.center.f3.z - boxCenter.f3.z) - boxHalfSize.f3.z);
    return (dx * dx + dy * dy + dz * dz) <= (sphere.radius * sphere.radius);
}

void Zone::Sending_NewRay(SendDataSaver& sds, vec4 rayStart, vec4 rayDirection, float rayDistance) {
    sds.postpush_start();
    constexpr int reqsiz = sizeof(STC_NewRay_Header);
    sds.postpush_reserve(reqsiz);
    STC_NewRay_Header& header = *(STC_NewRay_Header*)sds.ofbuff;
    header.size = reqsiz;
    header.st = STC_Protocol::NewRay;
    header.raystart = rayStart.f3;
    header.rayDir = rayDirection.f3;
    header.distance = rayDistance;
    sds.postpush_end();
}

void Zone::Sending_InventoryItemSync(SendDataSaver& sds, ItemStack lootdata, int inventoryIndex)
{
    sds.postpush_start();
    constexpr int reqsiz = sizeof(STC_InventoryItemSync_Header);
    sds.postpush_reserve(reqsiz);
    STC_InventoryItemSync_Header& header = *(STC_InventoryItemSync_Header*)sds.ofbuff;
    header.size = reqsiz;
    header.st = STC_Protocol::InventoryItemSync;
    header.Iteminfo = lootdata;
    header.inventoryIndex = inventoryIndex;
    sds.postpush_end();
}

void Zone::Sending_PlayerFire(SendDataSaver& sds, int objIndex) {
    sds.postpush_start();
    constexpr int reqsiz = sizeof(STC_PlayerFire_Header);
    sds.postpush_reserve(reqsiz);
    STC_PlayerFire_Header& header = *(STC_PlayerFire_Header*)sds.ofbuff;
    header.size = reqsiz;
    header.st = STC_Protocol::PlayerFire;
    header.objindex = objIndex;
    sds.postpush_end();
}

void Zone::Sending_SkillCast(SendDataSaver& sds, int ownerObjIndex, PlayerJob job, SkillSlot slot, SkillEffectType effectType, vec4 position, vec4 direction, float radius, float power, float duration) {
    sds.postpush_start();
    constexpr int reqsiz = sizeof(STC_SkillCast_Header);
    sds.postpush_reserve(reqsiz);
    STC_SkillCast_Header& header = *(STC_SkillCast_Header*)sds.ofbuff;
    header.size = reqsiz;
    header.st = STC_Protocol::SkillCast;
    header.ownerObjIndex = ownerObjIndex;
    header.job = job;
    header.slot = slot;
    header.effectType = effectType;
    header.position = position;
    header.direction = direction;
    header.radius = radius;
    header.power = power;
    header.duration = duration;
    sds.postpush_end();
}
GameObjectIncludeChunks Zone::GetChunks_Include_OBB(BoundingOrientedBox obb) {
    GameObjectIncludeChunks ret;
    XMFLOAT3 corners[BoundingOrientedBox::CORNER_COUNT];
    obb.GetCorners(corners);

    vec4 c[8];
    c[0].f3 = corners[0];
    vec4 minpos = c[0];
    vec4 maxpos = c[0];
    for (int i = 1; i < 8; ++i) {
        c[i].f3 = corners[i];
        minpos = _mm_min_ps(c[i], minpos);
        maxpos = _mm_max_ps(c[i], maxpos);
    }

    ret.xmin = floor(minpos.x / chunck_divide_Width);
    ret.xlen = floor(maxpos.x / chunck_divide_Width) - ret.xmin;
    ret.ymin = floor(minpos.y / chunck_divide_Width);
    ret.ylen = floor(maxpos.y / chunck_divide_Width) - ret.ymin;
    ret.zmin = floor(minpos.z / chunck_divide_Width);
    ret.zlen = floor(maxpos.z / chunck_divide_Width) - ret.zmin;
    return ret;
}

GameChunk* Zone::GetChunkFromPos(vec4 pos) {
    int ix = floor(pos.x / chunck_divide_Width);
    int iy = floor(pos.y / chunck_divide_Width);
    int iz = floor(pos.z / chunck_divide_Width);
    ChunkIndex ci = ChunkIndex(ix, iy, iz);
    auto gc = chunck.find(ci);
    if (gc != chunck.end()) {
        return gc->second;
    }
    else {
        return nullptr;
    }
}

void Zone::PushGameObject(GameObject* go) {
    go->zoneId = zoneId;
    if (go->tag[GameObjectTag::Tag_Dynamic] == false) {
        // static game object
        StaticGameObject* sgo = (StaticGameObject*)go;
        GameObjectIncludeChunks chunkIds = GetChunks_Include_OBB(sgo->GetOBB());
        ChunkIndex ci = ChunkIndex(chunkIds.xmin, chunkIds.ymin, chunkIds.zmin);
        ci.extra = 0;
        int chunckCount = chunkIds.GetChunckSize();
        bool pushing = false;
        for (; ci.extra < chunckCount; chunkIds.Inc(ci)) {
            GameChunk* gc = nullptr;
            bool isExist = false;
            {
                auto c = chunck.find(ci);
                isExist = (c != chunck.end());
                if (isExist) gc = c->second;
            }

            if (isExist == false)
            {
                // new game chunk
                gc = new GameChunk();
                gc->SetChunkIndex(ci, this);
                chunck.insert(pair<ChunkIndex, GameChunk*>(ci, gc));
            }

            int allocN = gc->Static_gameobjects.Alloc();
            gc->Static_gameobjects[allocN] = sgo;
            pushing = true;
        }
    }
    else {
        if (go->tag[GameObjectTag::Tag_SkinMeshObject] == true) {
            // dynamic game object
            SkinMeshGameObject* smgo = (SkinMeshGameObject*)go;
            smgo->InitialChunkSetting();
            smgo->IncludeChunks = GetChunks_Include_OBB(go->GetOBB());
            GameObjectIncludeChunks& chunkIds = smgo->IncludeChunks;
            ChunkIndex ci = ChunkIndex(chunkIds.xmin, chunkIds.ymin, chunkIds.zmin);
            ci.extra = 0;
            int chunckCount = chunkIds.GetChunckSize();
            if (chunckCount > smgo->chunkAllocIndexsCapacity) {
                int* newArr = new int[chunckCount];
                if (smgo->chunkAllocIndexs != nullptr) {
                    memcpy(newArr, smgo->chunkAllocIndexs, sizeof(int) * smgo->chunkAllocIndexsCapacity);
                    delete[] smgo->chunkAllocIndexs;
                }
                smgo->chunkAllocIndexs = newArr;
                smgo->chunkAllocIndexsCapacity = chunckCount;
            }

#ifdef DEVELOPMODE_ChunckDEBUG
            cout << "objptr = " << smgo << ", ";
#endif
            for (; ci.extra < chunckCount; chunkIds.Inc(ci)) {
                auto c = chunck.find(ci);
                GameChunk* gc;
                if (c == chunck.end()) {
                    // new game chunk
                    gc = new GameChunk();
                    gc->SetChunkIndex(ci, this);
                    chunck.insert(pair<ChunkIndex, GameChunk*>(ci, gc));
                }
                else gc = c->second;
                int allocN = gc->SkinMesh_gameobjects.Alloc();
                gc->SkinMesh_gameobjects[allocN] = smgo;
                smgo->chunkAllocIndexs[ci.extra] = allocN;
#ifdef DEVELOPMODE_ChunckDEBUG
                cout << smgo->chunkAllocIndexs[ci.extra] << ", ";
#endif
            }
#ifdef DEVELOPMODE_ChunckDEBUG
            cout << endl;
#endif
        }
        else {
            // dynamic game object
            DynamicGameObject* dgo = (DynamicGameObject*)go;
            dgo->InitialChunkSetting();
            dgo->IncludeChunks = GetChunks_Include_OBB(go->GetOBB());
            GameObjectIncludeChunks& chunkIds = dgo->IncludeChunks;
            ChunkIndex ci = ChunkIndex(chunkIds.xmin, chunkIds.ymin, chunkIds.zmin);
            ci.extra = 0;
            int chunckCount = chunkIds.GetChunckSize();
            if (chunckCount > dgo->chunkAllocIndexsCapacity) {
                int* newArr = new int[chunckCount];
                if (dgo->chunkAllocIndexs != nullptr) {
                    memcpy(newArr, dgo->chunkAllocIndexs, sizeof(int) * dgo->chunkAllocIndexsCapacity);
                    delete[] dgo->chunkAllocIndexs;
                }
                dgo->chunkAllocIndexs = newArr;
                dgo->chunkAllocIndexsCapacity = chunckCount;
            }
            for (; ci.extra < chunckCount; chunkIds.Inc(ci)) {
                auto c = chunck.find(ci);
                GameChunk* gc;
                if (c == chunck.end()) {
                    // new game chunk
                    gc = new GameChunk();
                    gc->SetChunkIndex(ci, this);
                    chunck.insert(pair<ChunkIndex, GameChunk*>(ci, gc));
                }
                else gc = c->second;
                int allocN = gc->Dynamic_gameobjects.Alloc();
                gc->Dynamic_gameobjects[allocN] = dgo;
                dgo->chunkAllocIndexs[ci.extra] = allocN;
            }
        }
    }
}

//void Zone::PrintCangoGrid(const std::vector<AstarNode*>& all, int gridWidth, int gridHeight)
//{
//    std::cout << "\n=== CANGO GRID (.: walkable, #: blocked) ===\n";
//    for (int z = 0; z < gridHeight; ++z) {
//        for (int x = 0; x < gridWidth; ++x) {
//            const AstarNode* n = all[z * gridWidth + x];
//            std::cout << (n && n->cango ? '.' : '#');
//        }
//        std::cout << '\n';
//    }
//    std::cout << std::flush;
//}


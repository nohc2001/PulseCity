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

void Zone::Set_world_id_pos(World* w, int id, int _x, int _y)
{
    world = w;
    zoneId = id;
    x = _x;
    y = _y;
    BasicAABB_onlyXZ = vec4(
        MinimumCenter + ZoneWidth * _x - ZoneHalfWidth,
        MinimumCenter + ZoneWidth * _y - ZoneHalfWidth,
        MinimumCenter + ZoneWidth * _x + ZoneHalfWidth,
        MinimumCenter + ZoneWidth * _y + ZoneHalfWidth);
}

void Zone::Init() {
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

    Dynamic_gameObjects.Init(128);
    Static_gameObjects.Init(128);
    DropedItems.Init(4096);
    CommonSDS.Init(4096);

    cout << "Zone " << zoneId << " Init end (dynamic only)" << endl;
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
    cout << "[Zone " << zoneId << "] map=" << gameworld.ZoneTable[zoneId]->zoneName
        << " AABB.min=(" << map.AABB[0].f3.x << ", " << map.AABB[0].f3.y << ", " << map.AABB[0].f3.z << ")"
        << " AABB.max=(" << map.AABB[1].f3.x << ", " << map.AABB[1].f3.y << ", " << map.AABB[1].f3.z << ")"
        << " size=(" << (map.AABB[1].f3.x - map.AABB[0].f3.x) << ", "
                     << (map.AABB[1].f3.y - map.AABB[0].f3.y) << ", "
                     << (map.AABB[1].f3.z - map.AABB[0].f3.z) << ")"
        << " MapObjectCount=" << map.MapObjects.size() << endl;

    SpawnObjects();

    GridCollisionCheck();

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

                        for (int zi = 0; zi < 9; ++zi) {
                            if (zi == zoneId) continue;
                            /*if (gameworld.IsZoneOwned(zi) == false) continue;
                            if (gameworld.IsAdjacentZone(zoneId, zi) == false) continue;*/
                            Zone* nearZone = nearZones[zi];
                            if (nearZone == nullptr) continue;
                            auto nearChunkIt = nearZone->chunck.find(ci);
                            if (nearChunkIt == nearZone->chunck.end()) continue;
                            GameChunk* nearChunk = nearChunkIt->second;
                            if (nearChunk == nullptr) continue;

                            for (int u = 0; u < nearChunk->Dynamic_gameobjects.size; ++u) {
                                if (nearChunk->Dynamic_gameobjects.isnull(u)) continue;
                                DynamicGameObject* gbj2 = nearChunk->Dynamic_gameobjects[u];
                                if (gbj2 == nullptr) continue;
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

                            for (int u = 0; u < nearChunk->SkinMesh_gameobjects.size; ++u) {
                                if (nearChunk->SkinMesh_gameobjects.isnull(u)) continue;
                                SkinMeshGameObject* gbj2 = nearChunk->SkinMesh_gameobjects[u];
                                if (gbj2 == nullptr) continue;
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
                        }

                        Zone* staticZone = this;
                        if (staticZone != nullptr) {
                            auto sf = staticZone->chunck.find(ci);
                            if (sf == staticZone->chunck.end()) continue;
                            GameChunk* staticChunk = sf->second;
                            for (int k = 0; k < staticChunk->Static_gameobjects.size; ++k) {
                                if (staticChunk->Static_gameobjects.isnull(k)) continue;
                                StaticGameObject* sgo = staticChunk->Static_gameobjects[k];
                                if (sgo == nullptr) continue;

                                if (sgo->obbArr.size() == 0) {
                                    // Skip fallback collision when a static object has no OBB data.
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
                }

                if (gbj1->tickLVelocity.fast_square_of_len3 <= 0.001f) {
                GOTO_NEXTOBJ:
                    gbj1->tickLVelocity = XMVectorZero();
                    continue;
                }

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

    // [4단계-STEP5] 핸드오프는 '플레이어가 옆 서버로 넘어가 고스트로 보일 때'만 수행한다.
    // (고스트 '플레이어'가 없으면 = 아무도 안 넘어감 -> 스폰/유휴 몬스터가 위치만으로 넘어가는 churn 방지)
    bool hasGhostPlayer = false;
    for (auto& g : gameworld.ghostPlayers) {
        if (g.second == nullptr) continue;
        if ((short)GameObjectType::VptrToTypeTable[*(void**)g.second] == GameObjectType::_Player) { hasGhostPlayer = true; break; }
    }
    if (false && hasGhostPlayer)   // [임시 비활성화] step5b 몬스터 핸드오프 크래시 격리용
    for (int i = 0; i < Dynamic_gameObjects.size; ++i) {
        if (Dynamic_gameObjects.isnull(i)) continue;
        DynamicGameObject* o = Dynamic_gameObjects[i];
        if (o == nullptr || o->tag[GameObjectTag::Tag_Enable] == false) continue;
        if ((short)GameObjectType::VptrToTypeTable[*(void**)o] != GameObjectType::_Monster) continue;
        Monster* m = (Monster*)o;
        if (m->handoffCooldown > 0.0f) { m->handoffCooldown -= deltaTime; continue; }

        for (int bi = 0; bi < (int)boundaries.size(); ++bi) {
            Zoneboundary& b = boundaries[bi];
            if (b.enabled == false) continue;
            if (gameworld.IsZoneOwned(b.dstzoneId)) continue;
            float centerZ = (b.minPos.f3.z + b.maxPos.f3.z) * 0.5f;
            bool dstPlusZ = (b.spawnPos.f3.z > centerZ);
            bool crossed = dstPlusZ ? (m->worldMat.pos.f3.z > b.maxPos.f3.z)
                                    : (m->worldMat.pos.f3.z < b.minPos.f3.z);
            if (crossed == false) continue;

            gameworld.SendMonsterHandoff(b.dstzoneId, m);
            m->tag[GameObjectTag::Tag_Enable] = false;
            Sending_DeleteGameObject(CommonSDS, i);
            break;
        }
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
    // [FIX] 직렬???�에 zoneId ?�정 (AddPlayer?� ?�일???�유).
    obj->zoneId = this->zoneId;

    // CommonSDS�� ��Ŷ �ױ�
    Sending_NewGameObject(CommonSDS, newindex, obj);

    return newindex;
}

int Zone::NewPlayer(SendDataSaver& sds, Player* obj, int clientIndex) {
    int newindex = Dynamic_gameObjects.Alloc();
    Dynamic_gameObjects[newindex] = obj;
    obj->tag[GameObjectTag::Tag_Enable] = true;
    // [FIX] 직렬???�에 zoneId ?�정 (AddPlayer?� ?�일???�유).
    obj->zoneId = this->zoneId;

    // ���� Ŭ���̾�Ʈ�鿡�Դ� CommonSDS��
    obj->SendGameObject(newindex, CommonSDS);

    return newindex;
}

void Zone::RemovePlayer(int clientIndex) {
    Zone* zone = gameworld.GetClientZone(clientIndex);
    int objindex = gameworld.clients[clientIndex].objindex;
    Player* go = gameworld.clients[clientIndex].pObjData;
    cout << "[RemovePlayer] client=" << clientIndex
        << " zone=" << zoneId
        << " clientZone=" << gameworld.clients[clientIndex].zoneId
        << " objindex=" << objindex
        << " playerZoneId=" << (go ? go->zoneId : -1)
        << " pos=(" << (go ? go->worldMat.pos.f3.x : 0.0f)
        << ", " << (go ? go->worldMat.pos.f3.y : 0.0f)
        << ", " << (go ? go->worldMat.pos.f3.z : 0.0f)
        << ")" << endl;
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

int Zone::AddPlayer(int clientIndex, Player* player, vec4 spawnPos, bool update_Map, bool fullWorldSnapshot) {
    cout << "[Zone " << zoneId << "] AddPlayer clientIndex=" << clientIndex << endl;

    //�� ����
    player->zone = this;
    // [FIX] ??줄을 SendGameObject 보다 먼�? ?�야 ?�다.
    // SendGameObject(GameObject.cpp:262)??header.zoneId = this->zoneId �?직렬?�하?�데,
    // 기존?�는 zoneId가 �???PushGameObject ?�서??dst�?갱신?�어,
    // ???�레?�어 ?�킷??'?�전(src) zoneId'�??�송 -> ?�라가 ?�못??zone 버킷???�성 ->
    // AllocPlayerIndexes(dst 버킷)?� ?�덱??불일�?-> ??player ?�바?�딩 ?�패(조작/카메??멈춤).
    player->zoneId = this->zoneId;

    // ��ġ ����
    player->worldMat.pos = spawnPos;
    player->LVelocity = 0;
    player->tickLVelocity = 0;
    player->isGround = false;
    player->collideCount = 0;
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


    // Notify other clients in this zone about the new player. (CommonSDS)
    //player->SendGameObject(newIdx, CommonSDS);

    // Send current zone data to the new client. (PersonalSDS)
    SendDataSaver& personalSDS = gameworld.clients[clientIndex].PersonalSDS;

    // [PERF/FIX ?? ?�어 ?�드?�프 ?�킷??'?�드 ?�냅??버스??'보다 먼�? 보낸??
    // 멈춤???�인: ?�라가 SyncPlayerMoveZone ?�로 isPrepared=false 가 ????
    // AllocPlayerIndexes �?처리?�야 isPrepared 가 복구?�어 조작/카메?��? ?�아?�다.
    // 기존??AllocPlayerIndexes 가 20??객체(~?�십KB) 버스??'?????�잉?�어,
    // �?버스?��? ???�착/처리???�까지 ?�레?�어가 ?�어붙었??=체감 ?�레??.
    // ?�라???�서�?(1)MoveZone -> (2)???�레?�어 객체 -> (3)AllocPlayerIndexes �?바꿔
    // 조작??'즉시' 복구?�고, ?�머지 객체??�??�에 ?�트리밍?�다.
    // 주의: (2)가 반드??(3)보다 먼�??�야 ?�다. ??그러�?AllocPlayer 가 바인?�할
    //       dst 버킷 ?�롯??비어 ?�어 ?�바?�딩???�패(?�전 멈춤 버그 ?�현)?�다.

    // (1) zone move packet -> ?�라 currentZoneId �?dst �?맞춤
    if (update_Map) {
        gameworld.Sending_PlayerMoveZone(personalSDS, clientIndex, zoneId);
    }

    // (2) ???�레?�어 객체 먼�? -> AllocPlayer ?�점??dst 버킷 ?�롯??채워???�게 ??
    player->SendGameObject(newIdx, personalSDS);

    // (3) ?�당???�레?�어 ?�덱??-> ?�기??isPrepared 복구?�어 조작 즉시 가??
    gameworld.Sending_AllocPlayerIndex(personalSDS, clientIndex, newIdx);

    // ?�제 ?�머지 ?�드 ?�냅?�을 ?�송?�다. (조작?� ?��? ?�아?�음)
    // [PERF/FIX ?? ?�리???�동(fullWorldSnapshot=false)?�서???�적 객체(몬스????�??�전?�하지 ?�는??
    // ?�라???�접 �?객체�??��? 보유?�고 ?�고(지???�기??, 가?�성 기반 cleanup??그걸 ?��??��?�?
    // ?�전?��? 불필?�한 ?�킨메시 버스??=?��?)??뿐이?? ?�이???�탈?� 가벼워????�� ?�송?�다.
    SendingAllObjectForNewClient(personalSDS, fullWorldSnapshot);

    // Send player inventory data.
    // Load player data from database here when needed.
    for (int i = 0; i < player->maxItem; ++i) {
        ItemStack is;
        is.id = rand() % ItemTable.size();
        is.ItemCount = rand() % 10;
        player->Inventory[i] = is;
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

    for (int ix = 0; ix < 2; ++ix) {
        Zoneboundary boundary{};
        boundary.basezoneId = zoneId;

        int nearIndex = 0;
        if (ix == 0) {
            boundary.dstzoneId = this->nearZones[2]->zoneId;
            // Mid-zone debug gate
            boundary.minPos = vec4(BasicAABB_onlyXZ.x - 2, -100.0f, BasicAABB_onlyXZ.y, 1.0f);
            boundary.maxPos = vec4(BasicAABB_onlyXZ.x + 2, 100.0f, BasicAABB_onlyXZ.w, 1.0f);
            // Fallback spawn. Seamless transfer keeps the player's crossing position.
            float midZ = BasicAABB_onlyXZ.y + BasicAABB_onlyXZ.w;
            midZ *= 0.5f;
            boundary.spawnPos = vec4(BasicAABB_onlyXZ.x - 6, 10.0f, midZ, 1.0f);
            boundary.spawnYaw = XM_PIDIV2;  // face +X
        }
        else {
            boundary.dstzoneId = this->nearZones[3]->zoneId;
            // Mid-zone debug gate
            boundary.minPos = vec4(BasicAABB_onlyXZ.z - 2, -100.0f, BasicAABB_onlyXZ.y, 1.0f);
            boundary.maxPos = vec4(BasicAABB_onlyXZ.z + 2, 100.0f, BasicAABB_onlyXZ.w, 1.0f);
            // Fallback spawn. Seamless transfer keeps the player's crossing position.
            float midZ = BasicAABB_onlyXZ.y + BasicAABB_onlyXZ.w;
            midZ *= 0.5f;
            boundary.spawnPos = vec4(BasicAABB_onlyXZ.z + 6, 10.0f, midZ, 1.0f);
            boundary.spawnYaw = XM_PIDIV2;  // face +X
        }
        boundary.cooldownSec = 0.5f;
        boundary.enabled = true;
        boundaries.push_back(boundary);
    }

    for (int iy = 0; iy < 2; ++iy) {
        Zoneboundary boundary{};
        boundary.basezoneId = zoneId;

        int nearIndex = 0;
        if (iy == 0) {
            boundary.dstzoneId = this->nearZones[1]->zoneId;
            // Mid-zone debug gate
            boundary.minPos = vec4(BasicAABB_onlyXZ.x, -100.0f, BasicAABB_onlyXZ.y - 2, 1.0f);
            boundary.maxPos = vec4(BasicAABB_onlyXZ.z, 100.0f, BasicAABB_onlyXZ.y + 2, 1.0f);
            // Fallback spawn. Seamless transfer keeps the player's crossing position.
            float midX = BasicAABB_onlyXZ.x + BasicAABB_onlyXZ.z;
            midX *= 0.5f;
            boundary.spawnPos = vec4(midX, 10.0f, BasicAABB_onlyXZ.y - 6, 1.0f);
            boundary.spawnYaw = XM_PIDIV2;  // face +X
        }
        else {
            boundary.dstzoneId = this->nearZones[4]->zoneId;
            // Mid-zone debug gate
            boundary.minPos = vec4(BasicAABB_onlyXZ.x, -100.0f, BasicAABB_onlyXZ.w - 2, 1.0f);
            boundary.maxPos = vec4(BasicAABB_onlyXZ.z, 100.0f, BasicAABB_onlyXZ.w + 2, 1.0f);
            // Fallback spawn. Seamless transfer keeps the player's crossing position.
            float midX = BasicAABB_onlyXZ.x + BasicAABB_onlyXZ.z;
            midX *= 0.5f;
            boundary.spawnPos = vec4(midX, 10.0f, BasicAABB_onlyXZ.w + 6, 1.0f);
            boundary.spawnYaw = XM_PIDIV2;  // face +X
        }
        boundary.cooldownSec = 0.5f;
        boundary.enabled = true;
        boundaries.push_back(boundary);
    }
   
    //if (zoneId == 0) {
    //    Zoneboundary boundary{};
    //    boundary.basezoneId = zoneId;
    //    boundary.dstzoneId = 1;
    //    // Mid-zone debug gate for Zone 0.
    //    boundary.minPos = vec4(-40.0f, -100.0f, 33.0f, 1.0f);
    //    boundary.maxPos = vec4(40.0f, 100.0f, 35.0f, 1.0f);
    //    // Fallback spawn. Seamless transfer keeps the player's crossing position.
    //    boundary.spawnPos = vec4(0.0f, 10.0f, 36.0f, 1.0f);
    //    boundary.spawnYaw = XM_PIDIV2;  // face +X
    //    boundary.cooldownSec = 0.5f;
    //    boundary.enabled = true;
    //    boundaries.push_back(boundary);
    //}
    //else if (zoneId == 1) {
    //    Zoneboundary boundary{};
    //    boundary.basezoneId = 1;
    //    boundary.dstzoneId = 0;
    //    // Same physical boundary as Zone 0 because both servers load the same map.
    //    boundary.minPos = vec4(-40.0f, -100.0f, 33.0f, 1.0f);
    //    boundary.maxPos = vec4(40.0f, 100.0f, 35.0f, 1.0f);
    //    // Fallback spawn. Seamless transfer keeps the player's crossing position.
    //    boundary.spawnPos = vec4(0.0f, 10.0f, 32.0f, 1.0f);
    //    boundary.spawnYaw = XM_PIDIV2;  // face +X (opposite of -X entry)
    //    boundary.cooldownSec = 0.5f;
    //    boundary.enabled = true;
    //    boundaries.push_back(boundary);
    //}

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

        vec4 spawnPos = p->worldMat.pos;
        spawnPos.w = 1.0f;

        vec4 moveDir = p->tickLVelocity;
        moveDir.y = 0.0f;
        if (moveDir.fast_square_of_len3 < 0.0001f) {
            moveDir = p->worldMat.look;
            moveDir.y = 0.0f;
        }
        if (moveDir.fast_square_of_len3 > 0.0001f) {
            moveDir.len3 = 1.0f;
            spawnPos += moveDir * 2.0f;
            spawnPos.w = 1.0f;
        }

        p->zoneMoveCooldownRemain = boundary.cooldownSec;
        p->wasInsideBoundary = true;
        p->lastBoundaryIndex = currentBoundaryIndex;

        int clientIndex = p->clientIndex;
        gameworld.MovePlayerToZone(
            clientIndex,
            boundary.dstzoneId,
            spawnPos
        );

        return;
    }

    p->wasInsideBoundary = true;
    p->lastBoundaryIndex = currentBoundaryIndex;
}

void Zone::FlushSendToClients() {
    bool keepCommonSDS = false;
    for (int i = 0; i < gameworld.clients.size; ++i) {
        if (gameworld.clients.isnull(i)) continue;
        if (gameworld.clients[i].isServerPeer) continue;   // [4단계-STEP1] peer 링크엔 게임 데이터 안 보냄
        bool isOwnerZoneFlush = (gameworld.clients[i].zoneId == this->zoneId);
        if (gameworld.IsAdjacentZone(gameworld.clients[i].zoneId, this->zoneId) == false) continue;
        bool isTransferring = (gameworld.clients[i].pObjData == nullptr);
        if ((isOwnerZoneFlush == false || gameworld.clients[i].PersonalSDS.size <= 0) && (CommonSDS.size <= 0 || isTransferring)) continue;

        //dbgbreak(gameworld.clients[i].zoneId == 74);

        WSABUF sendbuf[2];
        DWORD sendbufCount = 0;
        if (isOwnerZoneFlush && gameworld.clients[i].PersonalSDS.size > 0) {
            sendbuf[sendbufCount].buf = gameworld.clients[i].PersonalSDS.buffer;
            sendbuf[sendbufCount].len = gameworld.clients[i].PersonalSDS.size;
            sendbufCount += 1;
        }
        if (CommonSDS.size > 0 && isTransferring == false) {
            sendbuf[sendbufCount].buf = CommonSDS.buffer;
            sendbuf[sendbufCount].len = CommonSDS.size;
            sendbufCount += 1;
        }

        if (sendbufCount != 0) {
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
        }
        
        if (isOwnerZoneFlush) {
            gameworld.clients[i].PersonalSDS.Clear();
        }
    }

    if (keepCommonSDS == false) {
        CommonSDS.Clear();
    }
}

//void Zone::SendZoneDataToClient(SendDataSaver& sds) {
//    // Send all dynamic objects in this zone.
//    for (int i = 0; i < Dynamic_gameObjects.size; ++i) {
//        if (Dynamic_gameObjects.isnull(i)) continue;
//        Dynamic_gameObjects[i]->SendGameObject(i, sds);
//    }
//
//    // Send dropped items.
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

void Zone::SendingAllObjectForNewClient(SendDataSaver& sds, bool includeDynamicObjects)
{
    for (int zi = 0; zi < 9; ++zi) {
        /*if (gameworld.IsZoneOwned(zi) == false) continue;
        if (gameworld.IsAdjacentZone(zoneId, zi) == false) continue;*/
        Zone* syncZone = nearZones[zi];
        if (syncZone == nullptr) continue;

        // [PERF/FIX ?? ?�리???�동?�서???�적 객체(?�킨메시) ?�전?�을 ?�략 -> ?�환 ?��? ?�거.
        if (includeDynamicObjects) {
            for (int i = 0; i < syncZone->Dynamic_gameObjects.size; ++i) {
                if (syncZone->Dynamic_gameObjects.isnull(i)) continue;
                syncZone->Sending_NewGameObject(sds, i, (GameObject*)syncZone->Dynamic_gameObjects[i]);
            }
        }

        for (int i = 0; i < syncZone->DropedItems.size; ++i) {
            if (syncZone->DropedItems.isnull(i)) continue;
            syncZone->Sending_ItemDrop(sds, i, syncZone->DropedItems[i]);
        }

        for (int i = 0; i < syncZone->portals.size(); ++i) {
            syncZone->portals[i]->SendGameObject(i, sds);
        }
    }

    // [4단계-STEP2] 이웃 서버에서 받아 둔 플레이어 고스트도 신규 클라에게 전송(중간 접속자 대응).
    for (auto& g : gameworld.ghostPlayers) {
        if (g.second == nullptr) continue;
        g.second->SendGameObject(g.first, sds);
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
    header.zoneId = zoneId;
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
    header.zoneId = zoneId;
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
    header.zoneId = zoneId;
    header.dropindex = dropindex;
    sds.postpush_end();
}

//��/����
void Zone::LoadMapForZone(int zid) {
    map.LoadMap(this->zoneName);
}

void Zone::SpawnObjects() {
    // �ٲ� ������ �°� ������Ʈ ����
    // Static ������Ʈ ����
    // TODO: ���� Shape �̸��� �ٲ� ������ �´��� Ȯ�� �ʿ�

    // zoneId == 1 => no spawn monster
    if (zoneId == 1) return;

    static constexpr int range = 100;
    for (int i = 0; i < 20; ++i) {
        Monster* mon = new Monster();
        mon->zone = this;
        MonsterType monsterType = (MonsterType)(rand() % (int)MonsterType::Max);
        mon->ApplyMonsterData(monsterType);
        //dron spawn higher

        float spawnY = 2.0f;

        if (monsterType == MonsterType::Dron) {
            spawnY = 4.0f;
        }

        mon->Init(XMMatrixTranslation((float)(rand() % range - (range / 2)), spawnY, (float)(rand() % range - (range / 2))));
        while (map.isStaticCollision(mon->GetOBB())) {
            mon->Init(XMMatrixTranslation((float)(rand() % range - (range / 2)), spawnY, (float)((rand() % range - (range / 2)))));
        }
    }

    //Spawn NPC
    PeacefulNPC* npc = new PeacefulNPC();
    npc->worldMat.Id();
    npc->worldMat.pos = vec4(-248.0f, 14, 1180.0f, 1);
    npc->NPCType = PeacefulNPCType::PNT_Quest;
    npc->NPCQuestList.push_back(0);
    npc->SetShape(Shape::StrToShapeIndex["Monster001"]);
    NewObject(npc, GameObjectType::_PeacefulNPC);
    PushGameObject(npc);
}

void Zone::GridCollisionCheck() {
    const float radius = 0.2f;
    const float groundY = 0.0f;

    for (AstarNode* node : allnodes) {
        vec4 nodePos(node->worldx, groundY, node->worldz);
        collisionchecksphere sphere = { nodePos, radius };
        BoundingOrientedBox obb = BoundingOrientedBox(nodePos.f3, { radius , radius , radius }, vec4(0, 0, 0, 1));
        bool blocked = false;

        Zone* staticZone = gameworld.GetStaticChunkZone();
        if (staticZone != nullptr) {
            GameObjectIncludeChunks goic = GetChunks_Include_OBB(obb);
            ChunkIndex ci = ChunkIndex(goic.xmin, goic.ymin, goic.zmin);
            int chunckLen = goic.GetChunckSize();
            for (; ci.extra < chunckLen; goic.Inc(ci)) {
                auto f = staticZone->chunck.find(ci);
                if (f != staticZone->chunck.end()) {
                    GameChunk* gc = f->second;
                    for (int i = 0; i < gc->Static_gameobjects.size; ++i) {
                        if (gc->Static_gameobjects.isnull(i)) continue;
                        StaticGameObject* obj = gc->Static_gameobjects[i];
                        if (obj == nullptr) continue;
                        BoundingOrientedBox sobb = obj->GetOBB();
                        if (sobb.Intersects(obb)) {
                            blocked = true;
                            goto COLLISION_CHECK_END;
                        }
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
    DynamicGameObject* hitObject = nullptr;
    int hitObjectIndex = -1;
    Zone* hitZone = nullptr;

    int lastcurrentindex = currentIndex;

    for (int zi = 0; zi < 9; ++zi) {
        /*if (gameworld.IsZoneOwned(zi) == false) continue;
        if (gameworld.IsAdjacentZone(zoneId, zi) == false) continue;*/
        Zone* testZone = nearZones[zi];
        if (testZone == nullptr) continue;
        for (int i = 0; i < testZone->Dynamic_gameObjects.size; ++i) {
            if (testZone->Dynamic_gameObjects.isnull(i)) continue;
            if (shooter == (GameObject*)testZone->Dynamic_gameObjects[i]) continue;

            testZone->currentIndex = i;
            BoundingOrientedBox obb = testZone->Dynamic_gameObjects[i]->GetOBB();
            float distance;

            if (obb.Intersects(rayOrigin, rayDirection, distance)) {
                if (distance < closestDistance) {
                    closestDistance = distance;
                    hitObject = testZone->Dynamic_gameObjects[i];
                    hitObjectIndex = i;
                    hitZone = testZone;
                }
            }
        }
    }

    int ghostHitIndex = -1;
    int ghostHitZone = -1;
    for (auto& g : gameworld.ghostPlayers) {
        DynamicGameObject* gh = g.second;
        if (gh == nullptr) continue;
        if (gh->tag[GameObjectTag::Tag_Enable] == false) continue;
        if (shooter == (GameObject*)gh) continue;
        BoundingOrientedBox obb = gh->GetOBB();
        float distance;
        if (obb.Intersects(rayOrigin, rayDirection, distance)) {
            if (distance < closestDistance) {
                closestDistance = distance;
                ghostHitIndex = g.first;
                ghostHitZone = gh->zoneId;
            }
        }
    }

    if (ghostHitIndex >= 0) {
        gameworld.SendGhostDamageToOwner(ghostHitZone, ghostHitIndex, damage);
    }
    else if (hitObject != nullptr) {
        hitZone->currentIndex = hitObjectIndex;
        hitObject->OnCollisionRayWithBullet(shooter, damage);
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
static StatusEffectType GetSkillStatusEffect(SkillEffectType effectType, float& statusDuration, float& statusPower)
{
    statusDuration = 0.0f;
    statusPower = 0.0f;
    switch (effectType) {
    case SkillEffectType::Juggernaut_FireProjectile:
    case SkillEffectType::Juggernaut_UltimateFire:
        statusDuration = 3.0f;
        statusPower = 6.0f;
        return StatusEffectType::Burn;
    case SkillEffectType::Juggernaut_Taunt:
        statusDuration = 3.0f;
        statusPower = 1.0f;
        return StatusEffectType::Taunt;
    case SkillEffectType::Frost_Cone:
        statusDuration = 2.4f;
        statusPower = 1.0f;
        return StatusEffectType::Freeze;
    case SkillEffectType::Frost_Blizzard:
        statusDuration = 1.6f;
        statusPower = 1.0f;
        return StatusEffectType::Freeze;
    case SkillEffectType::Tank_ShockWave:
        statusDuration = 0.8f;
        statusPower = 1.0f;
        return StatusEffectType::Stun;
    case SkillEffectType::Electric_Burst:
        statusDuration = 1.2f;
        statusPower = 1.0f;
        return StatusEffectType::Paralyze;
    default:
        return StatusEffectType::None;
    }
}
int Zone::ApplySkillDamage(GameObject* caster, SkillEffectType effectType, vec4 position, vec4 direction, float range, float radius, float damage) {
    if (caster == nullptr || damage <= 0.0f || radius <= 0.0f) return 0;

    bool selfOnly = effectType == SkillEffectType::Healer_HealAura ||
        effectType == SkillEffectType::Frost_IceBlock ||
        effectType == SkillEffectType::Aegis_ShieldAura;
    if (selfOnly) return 0;

    direction.y = 0.0f;
    if (direction.len3 <= 0.0001f) direction = vec4(0, 0, 1, 0);
    direction.len3 = 1.0f;

    bool directional = range > 0.0f;
    BoundingOrientedBox skillBox;
    BoundingSphere skillSphere;

    if (directional) {
        BoundingOrientedBox localBox;
        localBox.Center = XMFLOAT3(0.0f, 1.0f, range * 0.5f);
        localBox.Extents = XMFLOAT3(radius, 1.5f, max(0.5f, range * 0.5f));
        localBox.Orientation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

        matrix skillWorld;
        skillWorld.Id();
        skillWorld.pos = position;
        skillWorld.SetLook(direction);
        localBox.Transform(skillBox, skillWorld);
    }
    else {
        skillSphere.Center = XMFLOAT3(position.x, position.y + 1.0f, position.z);
        skillSphere.Radius = radius;
    }

    int hitCount = 0;
    int lastCurrentIndex = currentIndex;

    for (int zi = 0; zi < 9; ++zi) {
        /*if (gameworld.IsZoneOwned(zi) == false) continue;
        if (gameworld.IsAdjacentZone(zoneId, zi) == false) continue;*/
        Zone* testZone = nearZones[zi];
        if (testZone == nullptr) continue;

        for (int i = 0; i < testZone->Dynamic_gameObjects.size; ++i) {
            if (testZone->Dynamic_gameObjects.isnull(i)) continue;
            GameObject* object = (GameObject*)testZone->Dynamic_gameObjects[i];
            if (object == caster) continue;

            void* vptr = *(void**)object;
            if (GameObjectType::VptrToTypeTable[vptr] != GameObjectType::_Monster) continue;

            Monster* monster = (Monster*)object;
            if (monster->isDead) continue;

            BoundingOrientedBox monsterOBB = monster->GetOBB();
            bool hit = directional ? skillBox.Intersects(monsterOBB) : skillSphere.Intersects(monsterOBB);
            if (hit == false) continue;

            testZone->currentIndex = i;
            monster->ApplyDamage(caster, damage);
            float statusDuration = 0.0f;
            float statusPower = 0.0f;
            StatusEffectType statusType = GetSkillStatusEffect(effectType, statusDuration, statusPower);
            if (statusType != StatusEffectType::None) {
                monster->ApplyStatusEffect(caster, statusType, statusDuration, statusPower);
            }
            ++hitCount;
        }
    }

    currentIndex = lastCurrentIndex;
    return hitCount;
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
    header.zoneId = zoneId;
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
    header.zoneId = zoneId;
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

void Zone::Sending_StatusEffect(SendDataSaver& sds, int targetObjIndex, int sourceObjIndex, StatusEffectType statusType, bool active, float duration, float remainTime, float power, vec4 position, vec4 extents) {
    sds.postpush_start();
    constexpr int reqsiz = sizeof(STC_StatusEffect_Header);
    sds.postpush_reserve(reqsiz);
    STC_StatusEffect_Header& header = *(STC_StatusEffect_Header*)sds.ofbuff;
    header.size = reqsiz;
    header.st = STC_Protocol::StatusEffect;
    header.zoneId = zoneId;
    header.targetObjIndex = targetObjIndex;
    header.sourceObjIndex = sourceObjIndex;
    header.statusType = statusType;
    header.active = active;
    header.duration = duration;
    header.remainTime = remainTime;
    header.power = power;
    header.position = position;
    header.extents = extents;
    sds.postpush_end();
}

void Zone::Sending_NPCStartTalk(SendDataSaver& sds, PeacefulNPCType npctype, int StartID)
{
    sds.postpush_start();
    constexpr int reqsiz = sizeof(STC_NPCTalkStart_Header);
    sds.postpush_reserve(reqsiz);
    STC_NPCTalkStart_Header& header = *(STC_NPCTalkStart_Header*)sds.ofbuff;
    header.size = reqsiz;
    header.st = STC_Protocol::NPCTalkStart;
    header.NPCType = npctype;
    header.StartID = StartID;
    sds.postpush_end();
}

void Zone::Sending_AddQuest(SendDataSaver& sds, int questID)
{
    sds.postpush_start();
    constexpr int reqsiz = sizeof(STC_AddQuest_Header);
    sds.postpush_reserve(reqsiz);
    STC_AddQuest_Header& header = *(STC_AddQuest_Header*)sds.ofbuff;
    header.size = reqsiz;
    header.st = STC_Protocol::AddQuest;
    header.QuestID = questID;
    sds.postpush_end();
}

void Zone::Sending_DeleteQuest(SendDataSaver& sds, int questID)
{
    sds.postpush_start();
    constexpr int reqsiz = sizeof(STC_DeleteQuest_Header);
    sds.postpush_reserve(reqsiz);
    STC_DeleteQuest_Header& header = *(STC_DeleteQuest_Header*)sds.ofbuff;
    header.size = reqsiz;
    header.st = STC_Protocol::DeleteQuest;
    header.QuestID = questID;
    sds.postpush_end();
}

void Zone::Sending_SyncQuestPrograss(SendDataSaver& sds, int questID, Quest* prograss) {
    sds.postpush_start();
    int reqsiz = sizeof(STC_SyncQuestPrograss_Header) + prograss->requp * sizeof(int);
    sds.postpush_reserve(reqsiz);
    STC_SyncQuestPrograss_Header& header = *(STC_SyncQuestPrograss_Header*)sds.ofbuff;
    header.size = reqsiz;
    header.st = STC_Protocol::SyncQuestPrograss;
    header.questID = questID;
    header.questReqSiz = prograss->requp;
    char* ptr = sds.ofbuff + sizeof(STC_SyncQuestPrograss_Header);
    for (int i = 0; i < prograss->requp; ++i) {
        int& prograssI = *(int*)ptr;
        prograssI = prograss->ReqArr[i].PresentCnt;
        ptr += sizeof(int);
    }
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


#pragma once
#include "stdafx.h"
#include "GameObject.h"
#include "Monster.h"
#include "GameObjectTypes.h"
//#include "main.h"

class Player;
struct World;

struct Zone {
    // 소속 World
    World* world = nullptr;

    // 존 ID
    int zoneId = 0;

    // 게임 오브젝트 배열
    vecset<GameObject*> gameObjects;

    // 드롭된 아이템들
    vecset<ItemLoot> DropedItems;

    // 맵 데이터
    GameMap map;

    // Astar
    vector<AstarNode*> allnodes;
    static constexpr float AstarStartX = -40.0f;
    static constexpr float AstarStartZ = -40.0f;
    static constexpr float AstarEndX = 40.0f;
    static constexpr float AstarEndZ = 40.0f;

    // 현재 실행중인 오브젝트 인덱스
    int currentIndex = 0;

    // 생성자
    Zone(World* w, int id) : world(w), zoneId(id) {}
    Zone() : world(nullptr), zoneId(0) {}

    // 초기화
    void Init();

    // 업데이트
    void Update(float deltaTime);

    // 포탈 관련
    void SpawnPortal();
    void CheckPortalCollision(Player* p);

    // 오브젝트 관리
    int NewObject(GameObject* obj, GameObjectType gotype);
    int NewPlayer(Player* obj, int clientIndex);
    void RemovePlayer(int clientIndex);
    int AddPlayer(int clientIndex, Player* player, vec4 spawnPos);

    // 충돌 검사
    void GridCollisionCheck();

    // 이 존에 있는 클라이언트들에게 전송
    void SendToAll(int datacap);
    void SendToAll_except(int datacap, int except);

    // 새 클라이언트에게 존 데이터 전송
    void SendZoneDataToClient(int clientIndex);

    void LoadMapForZone(int zoneId);

    void SpawnObjects();

    void FireRaycast(GameObject* shooter, vec4 rayStart, vec4 rayDirection, float rayDistance);
};
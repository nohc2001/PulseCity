#pragma once
#include "stdafx.h"
#include "GameObject.h"

/*
* 설명 : 포탈 게임 오브젝트
* 플레이어가 포탈 범위 안에 들어오면 다른 서버로 이동시킨다.
*
* PortalInfo 데이터를 멤버로 직접 보유하며,
* GameObjectType::_Portal 로 등록되어 관리된다.
*/
struct Portal : public GameObject {
	//같은 서버내 이동으로 인해 ip, port 정보는 제거
	// 목적지 서버 IP
	//char dstIp[16] = {};
	// 목적지 서버 포트
	//int dstPort = 0;

	// 목적지 서버에서의 스폰 좌표
	float spawnX = 0.f;
	float spawnY = 0.f;
	float spawnZ = 0.f;

	// 포탈 감지 반경
	float radius = 3.0f;

	// 포탈이 속한 Zone ID
	int zoneId = 0;

	// 포탈의 목적지 zoneID
	int dstzoneId = 0;

	Portal() {}
	virtual ~Portal() {}

	/*
	* 설명 : 포탈은 움직이지 않으므로 Update는 비어있다.
	*/
	virtual void Update(float deltaTime) override {}

	/*
	* 설명 : 포탈은 충돌 반응을 하지 않는다.
	*/
	virtual void OnCollision(GameObject* other) override {}
	virtual void OnStaticCollision(BoundingOrientedBox obb) override {}
	virtual void OnCollisionRayWithBullet(GameObject* shooter) override {}

	/*
	* 설명/반환 : 포탈의 충돌 OBB를 반환한다.
	*/
	virtual BoundingOrientedBox GetOBB() override;
};


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
	// 목적지 서버 IP
	char dstIp[16] = {};
	// 목적지 서버 포트
	int dstPort = 0;

	// 목적지 서버에서의 스폰 좌표
	float spawnX = 0.f;
	float spawnY = 0.f;
	float spawnZ = 0.f;

	// 포탈 감지 반경
	float radius = 3.0f;

	// 포탈이 속한 Zone ID (선택)
	int zoneId = 0;

	// 포탈의 목적지 zoneId
	int dstzoneId = 0;

	Portal() {}
	virtual ~Portal() {}
};

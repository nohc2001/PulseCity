#pragma once
#include "stdafx.h"
#include "GameObject.h"

extern UCHAR* m_pKeyBuffer;

class Player : public GameObject {
public:
	/*bool isGround = false;
	int collideCount = 0;
	float JumpVelocity = 5;*/
	static constexpr float ShootDelay = 0.1f;
	float ShootFlow = 0;

	static constexpr float recoilVelocity = 20.0f;
	float recoilFlow = 0;
	static constexpr float recoilDelay = 0.3f;

	float HP;
	float MaxHP = 100;
	int bullets;
	int KillCount = 0;
	int DeathCount = 0;

	//이그니스 열기 게이지
	float HeatGauge = 0;
	float MaxHeatGauge = 100;

	vec4 DeltaMousePos;



	Mesh* Gun;
	Mesh* ShootPointMesh;
	matrix gunMatrix_thirdPersonView;
	matrix gunMatrix_firstPersonView;

	Mesh* HPBarMesh;
	matrix HPMatrix;

	Mesh* HeatBarMesh;
	matrix HeatBarMatrix;

	vector<ItemStack> Inventory;

	Player() : HP{ 100 } {}

	virtual void Update(float deltaTime) override;
	void ClientUpdate(float deltaTime);

	virtual void Render();
	void Render_AfterDepthClear();

	virtual void Event(WinEvent evt);

	virtual void OnCollision(GameObject* other) override;

	virtual BoundingOrientedBox GetOBB();

	BoundingOrientedBox GetBottomOBB(const BoundingOrientedBox& obb) {
		constexpr float margin = 0.1f;
		BoundingOrientedBox robb;
		robb.Center = obb.Center;
		robb.Center.y -= obb.Extents.y;
		robb.Extents = obb.Extents;
		robb.Extents.y = 0.4f;
		robb.Extents.x -= margin;
		robb.Extents.z -= margin;
		robb.Orientation = obb.Orientation;
		return robb;
	}

	void TakeDamage(float damage);

	virtual void OnCollisionRayWithBullet();
};
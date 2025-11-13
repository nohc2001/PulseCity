#pragma once
#include "stdafx.h"
#include "GameObject.h"

struct BitBoolArr_ui64 {
	ui64* data;
	int bitindex;

	operator bool() {
		return *data & (1 << bitindex);
	}

	void operator=(bool b) {
		ui64 n = 1 << bitindex;
		if (b) {
			*data |= n;
		}
		else {
			n = ~n;
			*data &= n;
		}
	}
};

template <int n> struct BitBoolArr {
	ui64 buffer[n] = {};

	BitBoolArr_ui64& operator[](int index) {
		BitBoolArr_ui64 d;
		d.data = &buffer[index >> 6];
		d.bitindex = index & 63;
		return d;
	}
};

struct Player : public GameObject {
	static constexpr float ShootDelay = 0.1f;
	float ShootFlow = 0;
	static constexpr float recoilVelocity = 20.0f;
	float recoilFlow = 0;
	static constexpr float recoilDelay = 0.3f;
	float HP;
	float MaxHP = 100;
	int bullets = 100;
	int KillCount = 0;
	int DeathCount = 0;
	//이그니스 열기 게이지
	float HeatGauge = 0;
	float MaxHeatGauge = 100;

	vec4 DeltaMousePos;


	bool isGround = false;
	bool isMouseReturn;
	int collideCount = 0;
	float JumpVelocity = 5;
	int clientIndex = 0;

	vec4 LastMousePos;
	//bool InputBuffer[8];
	BitBoolArr<2> InputBuffer;

	matrix ViewMatrix;
	bool bFirstPersonVision = true;
	vector<ItemStack> Inventory;

	Player() : HP(100.0f), HeatGauge(0), MaxHeatGauge(200) {}

	virtual void Update(float deltaTime) override;

	//virtual void Event(WinEvent evt);

	virtual void OnCollision(GameObject* other) override;

	virtual BoundingOrientedBox GetOBB();

	BoundingOrientedBox GetBottomOBB(const BoundingOrientedBox& obb);

	void TakeDamage(float damage);

	void Respawn();

	virtual void OnCollisionRayWithBullet(GameObject* shooter);
};


#pragma once
#include "stdafx.h"
#include "GameObject.h"

/*
* 설명 : 플래이어 게임 오브젝트 구조체
*/
struct Player : public GameObject {
	//사격하고 다음 사격까지 걸리는 시간
	static constexpr float ShootDelay = 0.1f;
	//다음 사격 가능 순간를 계산하기 위한 시간계산 변수
	float ShootFlow = 0;

	//<클라이언트가 해야 할 일을 서버가 하고 있다.. 고쳐야 할 필요가 있다.>
	//반동의 정도
	static constexpr float recoilVelocity = 15.0f;
	//반동을 계산하기 위한 시간 계산 변수
	float recoilFlow = 0;
	// 반동에 걸리는 시간
	static constexpr float recoilDelay = 0.3f;
	//<클라이언트가 해야 할 일을 서버가 하고 있다.. 고쳐야 할 필요가 있다.>

	//HP
	float HP;
	//최대HP
	float MaxHP = 100;
	//현재총탄개수
	int bullets = 100;
	//몬스터를 킬한 카운트 (임시 점수)
	int KillCount = 0;
	//죽음 카운트
	int DeathCount = 0;

	//열기 게이지
	float HeatGauge = 0;
	//최대 열기 게이지
	float MaxHeatGauge = 100;
	
	//스킬 쿨타임
	float HealSkillCooldown = 10.0f;
	// 쿨타임 타이머
	float HealSkillCooldownFlow = 0.0f;  

	//마우스가 얼마나 움직였는지를 나타낸다.
	//<클라이언트가 해야 할 일을 서버가 하고 있다.. 고쳐야 할 필요가 있다.>
	vec4 DeltaMousePos;

	//현재 땅에 닿아있는지를 나타낸다.
	bool isGround = false;

	// 얼마나 많은 게임 오브젝트와 충돌되었는지를 나타낸다.
	int collideCount = 0;

	// 점프력
	float JumpVelocity = 20;
	
	// 해당 플레이어는 몇번째 클라이언트가 가지고 있는지 나타낸다.
	int clientIndex = 0;

	// 플레이어가 어떤 키를 누르고 있는지를 나타내는 BoolBit 배열.
	BitBoolArr<2> InputBuffer;

	// 플레이어가 바라보고 있는 뷰 행렬
	matrix ViewMatrix;
	
	// 현재 플레이어가 1인칭 시점인지 여부 (이게 동기화가 되는 중인가??)
	bool bFirstPersonVision = true;

	// 플레이어의 인벤토리 정보 (왜 vector?? 배열이면 안됨??)
	vector<ItemStack> Inventory;

	Player() : HP(100.0f), HeatGauge(0), MaxHeatGauge(200) {}

	/*
	* 설명 : 게임오브젝트의 업데이트를 실행함.
	* 매개변수 :
	* float deltaTime : 이전 업데이트 실행 부터 현재까지의 시간 차이.
	*/
	virtual void Update(float deltaTime) override;

	/*
	* 설명 : 게임오브젝트가 충돌했을때에 호출되는 함수.
	* 매개변수 : 
	* GameObject* other : 충돌한 오브젝트
	*/
	virtual void OnCollision(GameObject* other) override;

	/*
	* 설명 : 게임오브젝트가 움직이지 않는 Static 충돌체와 충돌했을때에 호출되는 함수.
	* 매개변수 : 
	* BoundingOrientedBox other : 충돌한 OBB.
	*/
	virtual void OnStaticCollision(BoundingOrientedBox obb) override;

	/*
	* 설명/반환 : 게임오브젝트의 충돌 OBB 정보를 반환한다.
	*/
	virtual BoundingOrientedBox GetOBB();

	/*
	* 설명 : damage 만큼 플레이어에게 데미지를 준다.
	* 매개변수 :
	* float damage : 줄 데미지 양.
	*/
	void TakeDamage(float damage);

	/*
	* 설명 : 플레이어를 리스폰 시킨다.
	*/
	void Respawn();

	/*
	* 설명 : Ray와 플레이어가 충돌했을때 호출되는 함수
	* 매개변수 : 
	* GameObject* shooter : Ray를 쏜 게임오브젝트
	*/
	virtual void OnCollisionRayWithBullet(GameObject* shooter);
};


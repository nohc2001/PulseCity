#pragma once
#include "stdafx.h"
#include "GameObject.h"

extern UCHAR* m_pKeyBuffer;

struct Model;
struct ModelNode;

/*
* 설명 : 플래이어 게임 오브젝트 구조체
*/
class Player : public GameObject {
public:

	// 현재 무기 타입
	int m_currentWeaponType = 0;

	//HP
	float HP;
	//최대HP
	float MaxHP = 100;
	//현재총탄개수
	int bullets;
	//몬스터를 킬한 카운트 (임시 점수)
	int KillCount = 0;
	//죽음 카운트
	int DeathCount = 0;

	//이그니스 열기 게이지
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

	//Mesh* Gun;

	// 현재 들고 있는 총의 모델
	::Model* GunModel;

	// 조준점을 나타내는 메쉬
	Mesh* ShootPointMesh;
	// 3인칭일대 총의 월드 행렬
	matrix gunMatrix_thirdPersonView;
	//1인칭일때 총의 월드 행렬
	matrix gunMatrix_firstPersonView;

	// HPBar를 표현하는 Mesh
	Mesh* HPBarMesh;
	// HPBar를 표현하는 행렬
	matrix HPMatrix;
	// HeatBar를 표현하는 Mesh
	Mesh* HeatBarMesh;
	// HeatBar를 표현하는 행렬
	matrix HeatBarMatrix;

	// 인벤토리 데이터
	vector<ItemStack> Inventory;

	// idk
	std::vector<int> gunBarrelNodeIndices;
	float gunBarrelAngle;
	float gunBarrelSpeed;

	float m_yaw = 0.0f;
	float m_pitch = 0.0f;

	Player() : HP{ 100 } {
		m_pWeapon = new Weapon(WeaponType::Sniper);
	}

	virtual ~Player() { if (m_pWeapon) delete m_pWeapon; }

	Weapon* m_pWeapon = nullptr;

	/*
	* 설명 : 게임오브젝트의 업데이트를 실행함.
	* 매개변수 :
	* float deltaTime : 이전 업데이트 실행 부터 현재까지의 시간 차이.
	*/
	virtual void Update(float deltaTime) override;
	void ClientUpdate(float deltaTime);

	/*
	* 설명 :
	* <메쉬일 경우>
	* WorldMatrix를 Root Param 1에 0~16에 Set하고,
	* Material의 텍스쳐는 Root Param 3에 DescTable로 Set하고,
	* Material의 Constant Buffer 값은 Root Param 4에 DescTable로 Set하고,
	* 메쉬를 사용해
	* <모델일 경우>
	* //fix <처리 안함>
	*/
	virtual void Render();

	/*
	* 설명 : DepthClear 되고 렌더링되는 요소들을 렌더링
	* 보통 DepthClear가 되면 이후 UI, 항상 앞에 보여야만 하는 것들을 렌더링 한다.
	*/
	void Render_AfterDepthClear();

	// idk
	void UpdateGunBarrelNodes();
};
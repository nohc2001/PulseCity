#pragma once
#include "stdafx.h"
#include "GameObject.h"

/*
* 설명 : 간이 몬스터 클래스
*/
class Monster : public GameObject {
private:
	/*float m_speed = 2.0f;
	float m_patrolRange = 20.0f;
	float m_chaseRange = 10.0f;
	vec4 m_homePos;
	vec4 m_targetPos;

	bool m_isMove = false;
	float m_patrolTimer = 0.0f;
	float m_fireDelay = 1.0f;
	float m_fireTimer = 0.0f;

	int collideCount = 0;
	bool isGround = false;

	float HP = 30;
	const float MAXHP = 30;*/

public:
	// 몬스터 사망 여부
	bool isDead;

	// 몬스터 HP
	float HP = 30;
	// 몬스터 최대 채력
	float MaxHP = 30;

	// 여러 HP 바중 몇번째 HP 바인지. (렌더링 할때 중요.)
	int HPBarIndex;
	// HP 바를 표현하는 월드 행렬
	matrix HPMatrix;

	Monster() {}
	virtual ~Monster() {}

	/*
	* 설명 : 게임오브젝트의 업데이트를 실행함.
	* 매개변수 :
	* float deltaTime : 이전 업데이트 실행 부터 현재까지의 시간 차이.
	*/
	virtual void Update(float deltaTime) override;

	/*
	* 설명 : 게임오브젝트를 렌더링한다.
	*/
	virtual void Render();

	/*
	* 몬스터를 초기화 한다.
	* const XMMATRIX& initialWorldMatrix : 초기 월드 행렬
	*/
	void Init(const XMMATRIX& initialWorldMatrix);
};
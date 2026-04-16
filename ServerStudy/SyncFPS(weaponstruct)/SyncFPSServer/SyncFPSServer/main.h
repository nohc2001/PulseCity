#pragma once
#include "stdafx.h"
#include "GameObject.h"

extern World gameworld;
extern float DeltaTime;

/*
* 설명 : 서버의 GameObject를 상속한 구조체들의 
* Offset을 볼 수 있도록 출력하는 함수
*/
void PrintOffset();

bool CheckAABBSphereCollision(const vec4& boxCenter, const vec4& boxHalfSize, const collisionchecksphere& sphere);
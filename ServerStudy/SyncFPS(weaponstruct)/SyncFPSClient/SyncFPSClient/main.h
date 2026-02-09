#pragma once

#include "stdafx.h"

#define FRAME_BUFFER_WIDTH 1280
#define FRAME_BUFFER_HEIGHT 720

extern HINSTANCE g_hInst;
extern HWND hWnd;
extern LPCTSTR lpszClass;
extern LPCTSTR lpszWindowName;
extern int resolutionLevel;


LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);

/*
* 설명 : 네트워크 통신을 위한 각종 클래스의 오프셋과 사이즈 정보를 출력한다.
* 동기화에 필요한 작업
*/
void PrintOffset();

/*
* 설명 : 클라이언트의 입력을 구분하는 enum.
*/
enum InputID {
	KeyboardW = 'W',
	KeyboardA = 'A',
	KeyboardS = 'S',
	KeyboardD = 'D',
	Keyboard1 = '1',
	Keyboard2 = '2',
	KeyboardSpace = VK_SPACE,
	MouseLbutton = 5,
	MouseRbutton = 6,
	RotationSync = 7,
};


#pragma pack(push, 1)
struct RotationPacket {
	char id = 7; // RotationSync
	float yaw;
	float pitch;
};
#pragma pack(pop)


/*
* 설명 : WndProc 처리 (윈도우 메시지) 처리에 이용되는 구조체.
*/
struct WinEvent {
	HWND hWnd;
	UINT Message;
	WPARAM wParam;
	LPARAM lParam;	

	WinEvent(HWND hwnd, UINT msg, WPARAM wP, LPARAM lP) {
		hWnd = hwnd;
		Message = msg;
		wParam = wP;
		lParam = lP;
	}
};


#pragma once

#include "stdafx.h"

#define FRAME_BUFFER_WIDTH 1200
#define FRAME_BUFFER_HEIGHT 720

extern HINSTANCE g_hInst;
extern HWND hWnd;
extern LPCTSTR lpszClass;
extern LPCTSTR lpszWindowName;
extern int resolutionLevel;


LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);

/*
* ���� : ��Ʈ��ũ ����� ���� ���� Ŭ������ �����°� ������ ������ ����Ѵ�.
* ����ȭ�� �ʿ��� �۾�
*/
void PrintOffset();

/*
* ���� : Ŭ���̾�Ʈ�� �Է��� �����ϴ� enum.
*/
enum InputID {
	KeyboardW = 'W',
	KeyboardA = 'A',
	KeyboardS = 'S',
	KeyboardD = 'D',
	Keyboard1 = '1',
	Keyboard2 = '2',
	Keyboard3 = '3',
	Keyboard4 = '4',
	Keyboard5 = '5',
	KeyboardR = 'R',
	KeyboardX = 'X',
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
* ���� : WndProc ó�� (������ �޽���) ó���� �̿�Ǵ� ����ü.
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


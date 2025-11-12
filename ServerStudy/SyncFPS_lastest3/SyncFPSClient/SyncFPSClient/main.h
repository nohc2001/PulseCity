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

void PrintOffset();

enum InputID {
	KeyboardW = 'W',
	KeyboardA = 'A',
	KeyboardS = 'S',
	KeyboardD = 'D',
	KeyboardSpace = VK_SPACE,
	MouseLbutton = 5,
	MouseRbutton = 6,
	MouseMove = 7,
};

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


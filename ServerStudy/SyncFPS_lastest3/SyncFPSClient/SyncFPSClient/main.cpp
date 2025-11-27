#include "stdafx.h"
#include "main.h"
#include "Render.h"
#include "Game.h"
#include "GameObject.h"
#include "Player.h"
#include "Monster.h"

GlobalDevice gd;
Game game;
Socket* ClientSocket = nullptr;
UCHAR* m_pKeyBuffer;
vector<Item> ItemTable;

HINSTANCE g_hInst;
HWND hWnd;
LPCTSTR lpszClass = L"Pulse City Client 001";
LPCTSTR lpszWindowName = L"Pulse City Client 001";
int resolutionLevel = 3;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdParam, int nCmdShow)
{
	PrintOffset();

	//GameObjectType::STATICINIT();

	/*int result = ClientSocket->Receive();
	if (result > 0) {
		char* ptr = ClientSocket->m_receiveBuffer;
		game.clientIndexInServer = *(int*)ptr;
		ptr += 4;
		game.playerGameObjectIndex = *(int*)ptr;
	}*/

	//while (true) {
	//	result = ClientSocket->Receive();
	//	if (result > 0) {
	//		game.Receiving(ClientSocket->m_receiveBuffer);
	//	}
	//	else {
	//		break;
	//	}
	//}

	gd.screenWidth = GetSystemMetrics(SM_CXSCREEN);
	gd.screenHeight = GetSystemMetrics(SM_CYSCREEN);
	gd.ScreenRatio = (float)gd.screenHeight / (float)gd.screenWidth;
	ResolutionStruct* ResolutionArr = gd.GetResolutionArr();
	//question : is there any reason resolution must be setting already existed well known resolutions?
	// most game do that _ but why??

	//RawInput Mouse
	gd.RawMouse.usUsagePage = 0x01; // general input device
	gd.RawMouse.usUsage = 0x02;     // mouse
	gd.RawMouse.dwFlags = 0;        // initial setting
	gd.RawMouse.hwndTarget = hWnd;
	RegisterRawInputDevices(&gd.RawMouse, 1, sizeof(gd.RawMouse));

	MSG Message;
	WNDCLASSEX WndClass;
	g_hInst = hInstance;

	WndClass.cbSize = sizeof(WndClass);
	WndClass.style = CS_HREDRAW | CS_VREDRAW;
	WndClass.lpfnWndProc = (WNDPROC)WndProc;
	WndClass.cbClsExtra = 0;
	WndClass.cbWndExtra = 0;
	WndClass.hInstance = hInstance;
	WndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	WndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	WndClass.lpszMenuName = NULL;
	WndClass.lpszClassName = lpszClass;
	WndClass.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	RegisterClassEx(&WndClass);

	hWnd = CreateWindow(lpszClass, lpszWindowName, WS_OVERLAPPEDWINDOW, 0, 0, ResolutionArr[resolutionLevel].width, ResolutionArr[resolutionLevel].height, NULL, (HMENU)NULL, hInstance, NULL);
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	ShowCursor(FALSE);

	//init.
	gd.Init();
	game.Init();

	ClientSocket = new Socket(SocketType::Tcp);
	ClientSocket->SetReceiveBuffer(malloc(4096 * 2), 4096 * 2);
	ClientSocket->Bind(Endpoint::Any); // 2
	ClientSocket->Connect(Endpoint("127.0.0.1", 1000));
	ClientSocket->SetNonblocking();

	wchar_t m_pszFrameRate[32] = L"Pulse City Client 001 ____FPS";
	constexpr double MaxFPSFlow = 10000;
	constexpr double InvHZ = 1.0 / (double)QUERYPERFORMANCE_HZ;
	double FPSflow = 0;
	double DeltaFlow = 0;
	ui64 ft = GetTicks();
	while (1) {
		ui64 et = GetTicks();
		double f = (double)(et - ft) * InvHZ;
		FPSflow += f;
		DeltaFlow += f;
		if (::PeekMessage(&Message, NULL, 0, 0, PM_REMOVE))
		{
			if (Message.message == WM_QUIT) break;
			::TranslateMessage(&Message);
			::DispatchMessage(&Message);
		}
		else
		{
			if (DeltaFlow >= 0.016) { // limiting fps.
				game.DeltaTime = (float)DeltaFlow;
				game.Render();
				game.Update();
				DeltaFlow = 0;
			}
		}

		if (FPSflow >= 1.0) {
			double FramePerSecond = 1.0f / f;
			if (FramePerSecond < 1000) {
				::_itow_s((int)FramePerSecond, ((wchar_t*)m_pszFrameRate) + 22, 4, 10);
				for (int i = 22; i < 26; ++i) {
					if (m_pszFrameRate[i] == 0 || m_pszFrameRate[i] > 128) m_pszFrameRate[i] = L'_';
				}
				SetWindowText(hWnd, m_pszFrameRate);
			}
			FPSflow = 0.0;
		}
		ft = et;
	}

	return Message.wParam;
}

RECT rt;

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	HDC hdc;

	//game.Event(WinEvent(hWnd, uMsg, wParam, lParam));

	switch (uMsg) {
		//(14 line) outer code start : microsoft copilot. - FPS Camera Movement
	case WM_INPUT:
	{
		UINT dwSize;
		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
		if (dwSize > 4096) break;
		BYTE* lpb = gd.InputTempBuffer;
		if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) == dwSize)
		{
			RAWINPUT* raw = (RAWINPUT*)lpb;
			if (raw->header.dwType == RIM_TYPEMOUSE)
			{
				int deltaX = raw->data.mouse.lLastX;
				int deltaY = raw->data.mouse.lLastY;

				//outer code end - FPS Camera Movement

				game.AddMouseInput(deltaX, deltaY);
			}
		}
		break;
	}
	case WM_CREATE:
		GetClientRect(hWnd, &rt);
		break;
	case WM_KEYDOWN:
		switch (wParam) {
		case 'W':
		{
			if ((game.pKeyBuffer['W'] & 0xF0) == false) {
				char input[3] = "WD";
				ClientSocket->Send(input, 2);
			}
		}
		break;
		case 'A':
		{
			if ((game.pKeyBuffer['A'] & 0xF0) == false) {
				char input[3] = "AD";
				ClientSocket->Send(input, 2);
			}
		}
		break;
		case 'S':
		{
			if ((game.pKeyBuffer['S'] & 0xF0) == false) {
				char input[3] = "SD";
				ClientSocket->Send(input, 2);
			}
		}
		break;
		case 'D':
		{
			if ((game.pKeyBuffer['D'] & 0xF0) == false) {
				char input[3] = "DD";
				ClientSocket->Send(input, 2);
			}
		}
		break;
		case VK_SPACE:
		{
			if ((game.pKeyBuffer[VK_SPACE] & 0xF0) == false) {
				char input[3] = "_D";
				input[0] = VK_SPACE;
				ClientSocket->Send(input, 2);
			}
		}
		break;
		case 'E':
		{
			game.isInventoryOpen = !game.isInventoryOpen;
		}
		break;
		case VK_F9:
		{
			BOOL bFullScreenState = FALSE;
			gd.pSwapChain->GetFullscreenState(&bFullScreenState, NULL);
			gd.SetFullScreenMode(!bFullScreenState);
			break;
		}
		case VK_F5:
		{
			game.bFirstPersonVision = !game.bFirstPersonVision;
			break;
		}
		case VK_F1:
		{
			__debugbreak();
			break;
		}
		case VK_ESCAPE:
		{
			PostQuitMessage(0);
			break;
		}
		break;
		}
		GetKeyboardState(game.pKeyBuffer);
		break;
	case WM_KEYUP:
		switch (wParam) {
		case 'W':
		{
			char input[3] = "WU";
			ClientSocket->Send(input, 2);
		}
		break;
		case 'A':
		{
			char input[3] = "AU";
			ClientSocket->Send(input, 2);
		}
		break;
		case 'S':
		{
			char input[3] = "SU";
			ClientSocket->Send(input, 2);
		}
		break;
		case 'D':
		{
			char input[3] = "DU";
			ClientSocket->Send(input, 2);
		}
		break;
		case VK_SPACE:
		{
			char input[3] = "_U";
			input[0] = VK_SPACE;
			ClientSocket->Send(input, 2);
		}
		break;
		}
		GetKeyboardState(game.pKeyBuffer);
		break;
	case WM_LBUTTONDOWN:
	{
		char input[3] = "_D";
		input[0] = InputID::MouseLbutton;
		ClientSocket->Send(input, 2);
	}
	break;
	case WM_LBUTTONUP:
	{
		char input[3] = "_U";
		input[0] = InputID::MouseLbutton;
		ClientSocket->Send(input, 2);
	}
	break;
	case WM_RBUTTONDOWN:
	{
		char input[3] = "_D";
		input[0] = InputID::MouseRbutton;
		ClientSocket->Send(input, 2);
	}
	break;
	case WM_RBUTTONUP:
	{
		char input[3] = "_U";
		input[0] = InputID::MouseRbutton;
		ClientSocket->Send(input, 2);
	}
	break;
	/*case WM_MOUSEMOVE:
		game.LastMousePos.x = (float)LOWORD(lParam);
		game.LastMousePos.y = (float)HIWORD(lParam);
		if (game.LastMousePos.x > gd.ClientFrameWidth * 0.75f || game.LastMousePos.x < gd.ClientFrameWidth * 0.25f || game.LastMousePos.y > gd.ClientFrameHeight * 0.75f || game.LastMousePos.y < gd.ClientFrameHeight * 0.25f) {
			SetCursorPos(gd.ClientFrameWidth / 2, gd.ClientFrameHeight / 2);
		}
		if (false) {
			if (game.isMouseReturn == false) {
				game.DeltaMousePos.x += (float)LOWORD(lParam) - game.LastMousePos.x;
				game.DeltaMousePos.y += (float)HIWORD(lParam) - game.LastMousePos.y;
				if (game.DeltaMousePos.y > 100) {
					game.DeltaMousePos.y = 100;
				}
				if (game.DeltaMousePos.y < -100) {
					game.DeltaMousePos.y = -100;
				}
				game.LastMousePos.x = (float)LOWORD(lParam);
				game.LastMousePos.y = (float)HIWORD(lParam);
				if (game.LastMousePos.x > gd.ClientFrameWidth * 0.75f || game.LastMousePos.x < gd.ClientFrameWidth * 0.25f || game.LastMousePos.y > gd.ClientFrameHeight * 0.75f || game.LastMousePos.y < gd.ClientFrameHeight * 0.25f) {
					SetCursorPos(gd.ClientFrameWidth / 2, gd.ClientFrameHeight / 2);
				}
				game.isMouseReturn = true;
			}
			else {
				game.isMouseReturn = false;
				game.LastMousePos.x = (float)LOWORD(lParam);
				game.LastMousePos.y = (float)HIWORD(lParam);
			}
		}
		break;*/
	case WM_DESTROY:
	{
		//ClientSocket->Send(nullptr, 0); // exit signal?

		while (true) {
			int result = ClientSocket->Receive();
			if (result <= 0) {
				break;
			}
		}

		gd.Release();
		PostQuitMessage(0);
	}

	break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void PrintOffset() {
	{
		int n = sizeof(GameObject);
		dbglog1(L"class GameObject : size = %d\n", n);
		GameObject temp;

		n = (char*)&temp - (char*)&temp.isExist;
		dbglog1(L"class GameObject.isExist%d\n", n);
		n = (char*)&temp - (char*)&temp.diffuseTextureIndex;
		dbglog1(L"class GameObject.diffuseTextureIndex%d\n", n);
		n = (char*)&temp - (char*)&temp.m_worldMatrix;
		dbglog1(L"class GameObject.m_worldMatrix%d\n", n);
		n = (char*)&temp - (char*)&temp.LVelocity;
		dbglog1(L"class GameObject.LVelocity%d\n", n);
		n = (char*)&temp - (char*)&temp.tickLVelocity;
		dbglog1(L"class GameObject.tickLVelocity%d\n", n);
		n = (char*)&temp - (char*)&temp.m_pMesh;
		dbglog1(L"class GameObject.m_pMesh%d\n", n);
		n = (char*)&temp - (char*)&temp.m_pShader;
		dbglog1(L"class GameObject.m_pShader%d\n", n);
		n = (char*)&temp - (char*)&temp.Destpos;
		dbglog1(L"class GameObject.Destpos%d\n", n);
		n = (char*)&temp - (char*)&temp.rmod;
		dbglog1(L"class GameObject.rmod%d\n", n);

	}
	dbglog1(L"-----------------------------------%d\n\n", rand());
	{
		int n = sizeof(Player);
		dbglog1(L"class Player : size = %d\n", n);
		Player temp;

		n = (char*)&temp - (char*)&temp.isExist;
		dbglog1(L"class Player.isExist%d\n", n);
		n = (char*)&temp - (char*)&temp.diffuseTextureIndex;
		dbglog1(L"class Player.diffuseTextureIndex%d\n", n);
		n = (char*)&temp - (char*)&temp.m_worldMatrix;
		dbglog1(L"class Player.m_worldMatrix%d\n", n);
		n = (char*)&temp - (char*)&temp.LVelocity;
		dbglog1(L"class Player.LVelocity%d\n", n);
		n = (char*)&temp - (char*)&temp.tickLVelocity;
		dbglog1(L"class Player.tickLVelocity%d\n", n);
		n = (char*)&temp - (char*)&temp.m_pMesh;
		dbglog1(L"class Player.m_pMesh%d\n", n);
		n = (char*)&temp - (char*)&temp.m_pShader;
		dbglog1(L"class Player.m_pShader%d\n", n);
		n = (char*)&temp - (char*)&temp.Destpos;
		dbglog1(L"class Player.Destpos%d\n", n);
		dbglog1(L"-----------------------%d\n", rand());
		n = (char*)&temp - (char*)&temp.ShootFlow;
		dbglog1(L"class Player.ShootFlow%d\n", n);
		n = (char*)&temp - (char*)&temp.recoilFlow;
		dbglog1(L"class Player.recoilFlow%d\n", n);
		n = (char*)&temp - (char*)&temp.HP;
		dbglog1(L"class Player.HP%d\n", n);
		n = (char*)&temp - (char*)&temp.MaxHP;
		dbglog1(L"class Player.MaxHP%d\n", n);
		n = (char*)&temp - (char*)&temp.DeltaMousePos;
		dbglog1(L"class Player.DeltaMousePos%d\n", n);
		n = (char*)&temp - (char*)&temp.bullets;
		dbglog1(L"class Player.bullets%d\n", n);
		n = (char*)&temp - (char*)&temp.KillCount;
		dbglog1(L"class Player.KillCount%d\n", n);
		n = (char*)&temp - (char*)&temp.DeathCount;
		dbglog1(L"class Player.DeathCount%d\n", n);
		n = (char*)&temp - (char*)&temp.HeatGauge;
		dbglog1(L"class Player.HeatGauge%d\n", n);
		n = (char*)&temp - (char*)&temp.MaxHeatGauge;
		dbglog1(L"class Player.MaxHeatGauge%d\n", n);
	}
	dbglog1(L"-----------------------------------%d\n\n", rand());
	{
		int n = sizeof(Monster);
		dbglog1(L"class Monster : size = %d\n", n);
		Monster temp;

		n = (char*)&temp - (char*)&temp.isExist;
		dbglog1(L"class Monster.isExist%d\n", n);
		n = (char*)&temp - (char*)&temp.diffuseTextureIndex;
		dbglog1(L"class Monster.diffuseTextureIndex%d\n", n);
		n = (char*)&temp - (char*)&temp.m_worldMatrix;
		dbglog1(L"class Monster.m_worldMatrix%d\n", n);
		n = (char*)&temp - (char*)&temp.LVelocity;
		dbglog1(L"class Monster.LVelocity%d\n", n);
		n = (char*)&temp - (char*)&temp.tickLVelocity;
		dbglog1(L"class Monster.tickLVelocity%d\n", n);
		n = (char*)&temp - (char*)&temp.m_pMesh;
		dbglog1(L"class Monster.m_pMesh%d\n", n);
		n = (char*)&temp - (char*)&temp.m_pShader;
		dbglog1(L"class Monster.m_pShader%d\n", n);
		n = (char*)&temp - (char*)&temp.Destpos;
		dbglog1(L"class Monster.Destpos%d\n", n);
		n = (char*)&temp - (char*)&temp.isDead;
		dbglog1(L"class Monster.isDead%d\n", n);
		n = (char*)&temp - (char*)&temp.HP;
		dbglog1(L"class Monster.HP%d\n", n);
		n = (char*)&temp - (char*)&temp.MaxHP;
		dbglog1(L"class Monster.MaxHP%d\n", n);
	}
}
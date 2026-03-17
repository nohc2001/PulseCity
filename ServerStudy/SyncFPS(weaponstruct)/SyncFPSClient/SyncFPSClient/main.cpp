int dbgc[128] = {};

#include "stdafx.h"
#include "main.h"
#include "Render.h"
#include "Game.h"
#include "GameObject.h"

extern GlobalDevice gd;
extern Game game;
Client client;
UCHAR* m_pKeyBuffer;
vector<Item> ItemTable;

HINSTANCE g_hInst;
HWND hWnd;
LPCTSTR lpszClass = L"Pulse City Client 001";
LPCTSTR lpszWindowName = L"Pulse City Client 001";
int resolutionLevel = 3;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdParam, int nCmdShow)
{
	// 오류등이 한글로 표시되도록 한다.
	wcout.imbue(locale("korean"));
	//WSA 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	GameObject::StaticInit();
	PrintOffset();

	gd.Factory_Adaptor_Output_Init();
	
	gd.ClientFrameWidth = gd.EnableFullScreenMode_Resolusions[resolutionLevel].width;
	gd.ClientFrameHeight = gd.EnableFullScreenMode_Resolusions[resolutionLevel].height;

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

	hWnd = CreateWindow(lpszClass, lpszWindowName, WS_OVERLAPPEDWINDOW, 0, 0, gd.ClientFrameWidth, gd.ClientFrameHeight, NULL, (HMENU)NULL, hInstance, NULL);
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	ShowCursor(FALSE);

	//init.
	gd.Init();
	game.Init();
	
	bool Connected = client.Init("127.0.0.1", 9000);
	if (Connected == false) {
		WSACleanup();
		return 0;
	}

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
				if (gd.isRaytracingRender) {
					game.Render_RayTracing();
				}
				else {
					game.Render();
				}
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

	WSACleanup();
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
				CTS_KeyInput_Header header;
				header.size = sizeof(CTS_KeyInput_Header);
				header.st = CTS_Protocol::KeyInput;
				header.Key = 'W';
				header.isdown = true;
				client.send((char*) & header, sizeof(CTS_KeyInput_Header), 0);
			}
		}
		break;
		case 'A':
		{
			if ((game.pKeyBuffer['A'] & 0xF0) == false) {
				CTS_KeyInput_Header header;
				header.size = sizeof(CTS_KeyInput_Header);
				header.st = CTS_Protocol::KeyInput;
				header.Key = 'A';
				header.isdown = true;
				client.send((char*)&header, sizeof(CTS_KeyInput_Header), 0);
			}
		}
		break;
		case 'S':
		{
			if ((game.pKeyBuffer['S'] & 0xF0) == false) {
				CTS_KeyInput_Header header;
				header.size = sizeof(CTS_KeyInput_Header);
				header.st = CTS_Protocol::KeyInput;
				header.Key = 'S';
				header.isdown = true;
				client.send((char*)&header, sizeof(CTS_KeyInput_Header), 0);
			}
		}
		break;
		case 'D':
		{
			if ((game.pKeyBuffer['D'] & 0xF0) == false) {
				CTS_KeyInput_Header header;
				header.size = sizeof(CTS_KeyInput_Header);
				header.st = CTS_Protocol::KeyInput;
				header.Key = 'D';
				header.isdown = true;
				client.send((char*)&header, sizeof(CTS_KeyInput_Header), 0);
			}
		}
		break;
		case 'Q':
		{
			if ((game.pKeyBuffer['Q'] & 0xF0) == false) {
				CTS_KeyInput_Header header;
				header.size = sizeof(CTS_KeyInput_Header);
				header.st = CTS_Protocol::KeyInput;
				header.Key = 'Q';
				header.isdown = true;
				client.send((char*)&header, sizeof(CTS_KeyInput_Header), 0);
			}
		}
		break;

		case VK_SPACE:
		{
			if ((game.pKeyBuffer[VK_SPACE] & 0xF0) == false) {
				CTS_KeyInput_Header header;
				header.size = sizeof(CTS_KeyInput_Header);
				header.st = CTS_Protocol::KeyInput;
				header.Key = VK_SPACE;
				header.isdown = true;
				client.send((char*)&header, sizeof(CTS_KeyInput_Header), 0);
			}
		}
		break;
		case 'E':
		{
			game.isInventoryOpen = !game.isInventoryOpen;
		}
		break;
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		{
			if (!(lParam & (1 << 30))) {
				char input[3] = { (char)wParam, 'D', 0 }; 
				CTS_KeyInput_Header header;
				header.size = sizeof(CTS_KeyInput_Header);
				header.st = CTS_Protocol::KeyInput;
				header.Key = (char)wParam;
				header.isdown = true;
				client.send((char*)&header, sizeof(CTS_KeyInput_Header), 0);
			}
		}
		break;
		case VK_F9:
		{
			BOOL bFullScreenState = FALSE;
			gd.pSwapChain->GetFullscreenState(&bFullScreenState, NULL);
			gd.SetFullScreenMode(!bFullScreenState);
			break;
		}
		case VK_F8:
		{
			gd.isRaytracingRender = !gd.isRaytracingRender;
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
			CTS_KeyInput_Header header;
			header.size = sizeof(CTS_KeyInput_Header);
			header.st = CTS_Protocol::KeyInput;
			header.Key = 'W';
			header.isdown = false;
			client.send((char*)&header, sizeof(CTS_KeyInput_Header), 0);
		}
		break;
		case 'A':
		{
			CTS_KeyInput_Header header;
			header.size = sizeof(CTS_KeyInput_Header);
			header.st = CTS_Protocol::KeyInput;
			header.Key = 'A';
			header.isdown = false;
			client.send((char*)&header, sizeof(CTS_KeyInput_Header), 0);
		}
		break;
		case 'S':
		{
			CTS_KeyInput_Header header;
			header.size = sizeof(CTS_KeyInput_Header);
			header.st = CTS_Protocol::KeyInput;
			header.Key = 'S';
			header.isdown = false;
			client.send((char*)&header, sizeof(CTS_KeyInput_Header), 0);
		}
		break;
		case 'D':
		{
			CTS_KeyInput_Header header;
			header.size = sizeof(CTS_KeyInput_Header);
			header.st = CTS_Protocol::KeyInput;
			header.Key = 'D';
			header.isdown = false;
			client.send((char*)&header, sizeof(CTS_KeyInput_Header), 0);
		}
		break;
		case 'Q':
		{
			CTS_KeyInput_Header header;
			header.size = sizeof(CTS_KeyInput_Header);
			header.st = CTS_Protocol::KeyInput;
			header.Key = 'Q';
			header.isdown = false;
			client.send((char*)&header, sizeof(CTS_KeyInput_Header), 0);
		}
		break;
		case VK_SPACE:
		{
			CTS_KeyInput_Header header;
			header.size = sizeof(CTS_KeyInput_Header);
			header.st = CTS_Protocol::KeyInput;
			header.Key = VK_SPACE;
			header.isdown = false;
			client.send((char*)&header, sizeof(CTS_KeyInput_Header), 0);
		}
		break;
		}
		GetKeyboardState(game.pKeyBuffer);
		break;
	case WM_LBUTTONDOWN:
	{
		CTS_KeyInput_Header header;
		header.size = sizeof(CTS_KeyInput_Header);
		header.st = CTS_Protocol::KeyInput;
		header.Key = InputID::MouseLbutton;
		header.isdown = true;
		client.send((char*)&header, sizeof(CTS_KeyInput_Header), 0);
	}
	break;
	case WM_LBUTTONUP:
	{
		CTS_KeyInput_Header header;
		header.size = sizeof(CTS_KeyInput_Header);
		header.st = CTS_Protocol::KeyInput;
		header.Key = InputID::MouseLbutton;
		header.isdown = false;
		client.send((char*)&header, sizeof(CTS_KeyInput_Header), 0);
	}
	break;
	case WM_RBUTTONDOWN:
	{
		CTS_KeyInput_Header header;
		header.size = sizeof(CTS_KeyInput_Header);
		header.st = CTS_Protocol::KeyInput;
		header.Key = InputID::MouseRbutton;
		header.isdown = true;
		client.send((char*)&header, sizeof(CTS_KeyInput_Header), 0);
	}
	break;
	case WM_RBUTTONUP:
	{
		CTS_KeyInput_Header header;
		header.size = sizeof(CTS_KeyInput_Header);
		header.st = CTS_Protocol::KeyInput;
		header.Key = InputID::MouseRbutton;
		header.isdown = false;
		client.send((char*)&header, sizeof(CTS_KeyInput_Header), 0);
	}
	break;
	case WM_DESTROY:
	{
		//ClientSocket->Send(nullptr, 0); // exit signal?

		while (true) {
			int result = client.recv(client.rBuf, client.rbufMax);
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
	//{
	//	int n = sizeof(GameObject);
	//	dbglog1(L"class GameObject : size = %d\n", n);
	//	GameObject temp;

	//	n = (char*)&temp - (char*)&temp.tag;
	//	dbglog1(L"class GameObject.tag%d\n", n);
	//	n = (char*)&temp - (char*)&temp;
	//	dbglog1(L"class GameObject.MaterialIndex%d\n", n);
	//	n = (char*)&temp - (char*)&temp.worldMat;
	//	dbglog1(L"class GameObject.worldMat%d\n", n);
	//	n = (char*)&temp - (char*)&temp.LVelocity;
	//	dbglog1(L"class GameObject.LVelocity%d\n", n);
	//	n = (char*)&temp - (char*)&temp.tickLVelocity;
	//	dbglog1(L"class GameObject.tickLVelocity%d\n", n);
	//	n = (char*)&temp - (char*)&temp.m_pMesh;
	//	dbglog1(L"class GameObject.m_pMesh%d\n", n);
	//	n = (char*)&temp - (char*)&temp.m_pShader;
	//	dbglog1(L"class GameObject.m_pShader%d\n", n);
	//	n = (char*)&temp - (char*)&temp.Destpos;
	//	dbglog1(L"class GameObject.Destpos%d\n", n);
	//	n = (char*)&temp - (char*)&temp.rmod;
	//	dbglog1(L"class GameObject.rmod%d\n", n);

	//}
	//dbglog1(L"-----------------------------------%d\n\n", rand());
	//{
	//	int n = sizeof(Player);
	//	dbglog1(L"class Player : size = %d\n", n);
	//	Player temp;

	//	n = (char*)&temp - (char*)&temp.isExist;
	//	dbglog1(L"class Player.isExist%d\n", n);
	//	n = (char*)&temp - (char*)&temp.MaterialIndex;
	//	dbglog1(L"class Player.MaterialIndex%d\n", n);
	//	n = (char*)&temp - (char*)&temp.worldMat;
	//	dbglog1(L"class Player.worldMat%d\n", n);
	//	n = (char*)&temp - (char*)&temp.LVelocity;
	//	dbglog1(L"class Player.LVelocity%d\n", n);
	//	n = (char*)&temp - (char*)&temp.tickLVelocity;
	//	dbglog1(L"class Player.tickLVelocity%d\n", n);
	//	n = (char*)&temp - (char*)&temp.m_pMesh;
	//	dbglog1(L"class Player.m_pMesh%d\n", n);
	//	n = (char*)&temp - (char*)&temp.m_pShader;
	//	dbglog1(L"class Player.m_pShader%d\n", n);
	//	n = (char*)&temp - (char*)&temp.Destpos;
	//	dbglog1(L"class Player.Destpos%d\n", n);
	//	dbglog1(L"-----------------------%d\n", rand());
	//	n = (char*)&temp.m_pWeapon - (char*)&temp;
	//	dbglog1(L"class Player.m_pWeapon: %d\n", n);
	//	n = (char*)&temp.m_currentWeaponType - (char*)&temp;
	//	dbglog1(L"class Player.m_currentWeaponType: %d\n", n);
	//	n = (char*)&temp.m_yaw - (char*)&temp;
	//	dbglog1(L"class Player.m_yaw: %d\n", n);
	//	n = (char*)&temp.m_pitch - (char*)&temp;
	//	dbglog1(L"class Player.m_pitch: %d\n", n);
	//	n = (char*)&temp - (char*)&temp.HP;
	//	dbglog1(L"class Player.HP%d\n", n);
	//	n = (char*)&temp - (char*)&temp.MaxHP;
	//	dbglog1(L"class Player.MaxHP%d\n", n);
	//	n = (char*)&temp - (char*)&temp.DeltaMousePos;
	//	dbglog1(L"class Player.DeltaMousePos%d\n", n);
	//	n = (char*)&temp - (char*)&temp.bullets;
	//	dbglog1(L"class Player.bullets%d\n", n);
	//	n = (char*)&temp - (char*)&temp.KillCount;
	//	dbglog1(L"class Player.KillCount%d\n", n);
	//	n = (char*)&temp - (char*)&temp.DeathCount;
	//	dbglog1(L"class Player.DeathCount%d\n", n);
	//	n = (char*)&temp - (char*)&temp.HeatGauge;
	//	dbglog1(L"class Player.HeatGauge%d\n", n);
	//	n = (char*)&temp - (char*)&temp.MaxHeatGauge;
	//	dbglog1(L"class Player.MaxHeatGauge%d\n", n);
	//	n = (char*)&temp - (char*)&temp.HealSkillCooldown;
	//	dbglog1(L"class Player.Heal%d\n", n);
	//	n = (char*)&temp - (char*)&temp.HealSkillCooldownFlow;
	//	dbglog1(L"class Player.Healflow%d\n", n);
	//}
	//dbglog1(L"-----------------------------------%d\n\n", rand());
	//{
	//	int n = sizeof(Monster);
	//	dbglog1(L"class Monster : size = %d\n", n);
	//	Monster temp;

	//	n = (char*)&temp - (char*)&temp.isExist;
	//	dbglog1(L"class Monster.isExist%d\n", n);
	//	n = (char*)&temp - (char*)&temp.MaterialIndex;
	//	dbglog1(L"class Monster.MaterialIndex%d\n", n);
	//	n = (char*)&temp - (char*)&temp.worldMat;
	//	dbglog1(L"class Monster.worldMat%d\n", n);
	//	n = (char*)&temp - (char*)&temp.LVelocity;
	//	dbglog1(L"class Monster.LVelocity%d\n", n);
	//	n = (char*)&temp - (char*)&temp.tickLVelocity;
	//	dbglog1(L"class Monster.tickLVelocity%d\n", n);
	//	n = (char*)&temp - (char*)&temp.m_pMesh;
	//	dbglog1(L"class Monster.m_pMesh%d\n", n);
	//	n = (char*)&temp - (char*)&temp.m_pShader;
	//	dbglog1(L"class Monster.m_pShader%d\n", n);
	//	n = (char*)&temp - (char*)&temp.Destpos;
	//	dbglog1(L"class Monster.Destpos%d\n", n);
	//	n = (char*)&temp - (char*)&temp.isDead;
	//	dbglog1(L"class Monster.isDead%d\n", n);
	//	n = (char*)&temp - (char*)&temp.HP;
	//	dbglog1(L"class Monster.HP%d\n", n);
	//	n = (char*)&temp - (char*)&temp.MaxHP;
	//	dbglog1(L"class Monster.MaxHP%d\n", n);
	//}
}
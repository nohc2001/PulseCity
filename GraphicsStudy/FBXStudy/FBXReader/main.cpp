// 01_CreateDevice.cpp : Defines the entry point for the application.
//
#include "pch.h"
#include <Windows.h>
#include "Resource.h"
#include "D3D12Renderer.h"
#include "typedef.h"
#include "GameObject.h"

// required .lib files
#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "D3DCompiler.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

extern "C" { __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001; }

XMMATRIX CONSTANT_BUFFER_DEFAULT::ViewMatrix = {};
XMMATRIX CONSTANT_BUFFER_DEFAULT::ProjectMatrix = {};
XMFLOAT3 CONSTANT_BUFFER_DEFAULT::SCamPos = {};
PlaneEquation CONSTANT_BUFFER_DEFAULT::pexpr[6];
XMVECTOR CONSTANT_BUFFER_DEFAULT::AtVector = XMVectorSet(0, 0, 1, 0);
float CONSTANT_BUFFER_DEFAULT::farZ;
float CONSTANT_BUFFER_DEFAULT::nearZ;
XMMATRIX CONSTANT_BUFFER_DEFAULT::CameraWorldMatrix;
Pong_Material MeshComponent::Default_Material;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// D3D12 Agility SDK Runtime

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 614; }

#if defined(_M_ARM64EC)
	extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = u8".\\D3D12\\arm64\\"; }
#elif defined(_M_ARM64)
	extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = u8".\\D3D12\\arm64\\"; }
#elif defined(_M_AMD64)
	extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = u8".\\D3D12\\x64\\"; }
#elif defined(_M_IX86)
	extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = u8".\\D3D12\\x86\\"; }
#endif
//////////////////////////////////////////////////////////////////////////////////////////////////////

#define MAX_LOADSTRING 128

bool heapcheck() {
	int b = _heapchk();
	switch (b)
	{
	case _HEAPOK:
		printf(" OK - heap is fine\n");
		return true;
	case _HEAPEMPTY:
		printf(" OK - heap is empty\n");
		return true;
	case _HEAPBADBEGIN:
		printf("ERROR - bad start of heap\n");
		return false;
	case _HEAPBADNODE:
		printf("ERROR - bad node in heap\n");
		return false;
	}
}

// Global Variables:
HINSTANCE hInst = nullptr;                                // current instance
HWND g_hMainWindow = nullptr;
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

DWORD m_dwWidth = 0;
DWORD m_dwHeight = 0;

CD3D12Renderer* g_pRenderer = nullptr;
void* g_pMeshObj = nullptr;
TEXTURES_VECTOR g_TexCB;

CONSTANT_BUFFER_DEFAULT g_constBuff;
float g_fOffsetX = 0.0f;
float g_fOffsetY = 0.0f;
float g_fSpeed = 0.01f;

CONSTANT_BUFFER_DEFAULT g_constBuff2;

ULONGLONG g_PrvFrameCheckTick = 0;
ULONGLONG g_PrvUpdateTick = 0;
DWORD	g_FrameCount = 0;

Octree GameWorld;
ui8 ctable[256][256]; // 64KB // value 255 is null
ui32 ctableup = 0;
fmvecarr<MGameObjectChunckLump*> MGameObjChunckLumps;
fmvecarr<MOChunckType> MChunckTypeVector;
fmvecarr<Mesh*> MeshVec;
fmvecarr<void*> SeekChunckArr;
fmvecarr<void*> TempSeekChunckArr;
ui16 SeekFlag = 1;

void InitGame();
void RunGame();
void Update(float deltaTime);

extern unsigned char datamem[4096 * 16];

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
HWND InitInstance(HINSTANCE hInstance, int nCmdShow);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
					  _In_opt_ HINSTANCE hPrevInstance,
					  _In_ LPWSTR    lpCmdLine,
					  _In_ int       nCmdShow)
{
	//InitDA();
	fmhl.Init();
	fm = new FM_System0();
	fm->SetHeapData();
	fm->_tempPushLayer();
	

	//std::locale::global(std::locale(".UTF-8"));
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// TODO: Place code here.
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	// Initialize global strings
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_MY01CREATEDEVICE, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	g_hMainWindow = InitInstance (hInstance, nCmdShow);
	if (!g_hMainWindow)
	{
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MY01CREATEDEVICE));

	MSG msg;

	g_pRenderer = (CD3D12Renderer*)fm->_New(sizeof(CD3D12Renderer), true);
	g_pRenderer->Initialize(g_hMainWindow, TRUE, TRUE);
	//init game
	InitGame();

	//init mesh
	g_pMeshObj = g_pRenderer->CreateBasicMeshObject();

	//Texture Init
	ZeroMemory(&g_TexCB, sizeof(TEXTURES_VECTOR));
	g_TexCB.tex[0] = (TEXTURE_HANDLE*)g_pRenderer->AllocTextureDesc("Resources/material/leather_diffuse.png");
	g_TexCB.tex[1] = (TEXTURE_HANDLE*)g_pRenderer->AllocTextureDesc("Resources/material/leather_rough.png");
	g_TexCB.tex[2] = (TEXTURE_HANDLE*)g_pRenderer->AllocTextureDesc("Resources/material/leather_normal.png");

	SetWindowText(g_hMainWindow, L"DrawTriangle_SysMem");
	// Main message loop:
	//while (GetMessage(&msg, nullptr, 0, 0))
	//{
	//	if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
	//	{
	//		TranslateMessage(&msg);
	//		DispatchMessage(&msg);
	//	}
	//}
	// Main message loop:

	RECT rect;
	GetClientRect(g_hMainWindow, &rect);
	DWORD	dwWndWidth = rect.right - rect.left;
	DWORD	dwWndHeight = rect.bottom - rect.top;
	g_pRenderer->UpdateWindowSize(dwWndWidth, dwWndHeight);

	while (1)
	{
		// call WndProc
		//g_bCanUseWndProc == FALSE이면 DefWndProc호출

		BOOL	bHasMsg = PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE);

		if (bHasMsg)
		{
			if (msg.message == WM_QUIT)
			{
				break;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			RunGame();
		}
	}

	if (g_pMeshObj)
	{
		//release mesh
		g_pRenderer->DeleteBasicMeshObject(g_pMeshObj);
		g_pMeshObj = nullptr;
	}
	if (g_TexCB.tex) {
		for (int i = 0; i < 8; ++i) {
			if (g_TexCB.tex[i]) {
				g_pRenderer->FreeTextureDesc(g_TexCB.tex[i]);
				g_TexCB.tex[i] = nullptr;
			}
		}
	}
	if (g_pRenderer)
	{
		g_pRenderer->~CD3D12Renderer();
		fm->_Delete(g_pRenderer, sizeof(CD3D12Renderer));
		//delete g_pRenderer;
		g_pRenderer = nullptr;
	}
#ifdef _DEBUG
	_ASSERT(_CrtCheckMemory());
#endif
	return (int)msg.wParam;

	fm->_tempPopLayer();
}

void InitGame() {
	GameWorld.Init();
	
	for (int i = 0; i < 256; ++i) {
		for (int k = 0; k < 256; ++k) {
			ctable[i][k] = 255;
		}
	}

	SpaceChunck::Static_Init();
	Mesh::Static_Init(g_pRenderer);
	MeshComponent::Static_Init();
	ChildComponent::Static_Init();
	CubeColliderComponent::Static_Init();
	SphereColliderComponent::Static_Init();
	CilinderColliderComponent::Static_Init();
	MeshColliderComponent::Static_Init();

	SpaceChunck* sc = GameWorld.AddSpaceChunck(0, 0, 0);
	//Object* obj = Object::LoadObjectFromFBX("/Resources/Mesh/default_door.fbx");
	Object* obj = Object::LoadObjectFromFBX("/Resources/Mesh/SMG_GUN/SMG GUN FREE2.fbx");
	//Object* obj = Object::LoadObjectFromFBX("/Resources/Mesh/Lambroghini_Huracan_twin_Turbo_Lost/sto.fbx");
	sc->AddObject(obj);
}

static inline int64_t GetTicks()
{
	LARGE_INTEGER ticks;
	QueryPerformanceCounter(&ticks);
	return ticks.QuadPart;
}

ui64 ftick = 0;
ui64 etick = 0;
void RunGame()
{
	g_FrameCount++;

	// begin
	ULONGLONG CurTick = GetTickCount64();

	// game business logic
	if (CurTick - g_PrvUpdateTick > 16)
	{
		// Update Scene with 60FPS
		etick = GetTicks();
		float delta = (float)((double)(etick - ftick) / (double)QUERYPERFORMANCE_HZ);
		ftick = GetTicks();
		Update(delta);
		g_PrvUpdateTick = CurTick;
	}

	// rendering objects
	g_pRenderer->BeginRender();
	
	ui64 ft, et, delta;

	g_pRenderer->RenderMeshObject(g_pMeshObj, g_constBuff, g_TexCB);
	g_pRenderer->RenderMeshObject(g_pMeshObj, g_constBuff2, g_TexCB);

	GameWorld.GameWorldRender(g_pRenderer->GetCommandList());

	// end
	g_pRenderer->EndRender();

	// Present
	g_pRenderer->Present();

	if (CurTick - g_PrvFrameCheckTick > 1000)
	{
		g_PrvFrameCheckTick = CurTick;

		WCHAR wchTxt[64];
		swprintf_s(wchTxt, L"FPS:%u", g_FrameCount);
		SetWindowText(g_hMainWindow, wchTxt);

		g_FrameCount = 0;
	}
}

float t = 0;
bool WKeyPressed = false;
bool SKeyPressed = false;
bool AKeyPressed = false;
bool DKeyPressed = false;
float mx = 0;
float my = 0;
float deltaMX = 0;
float deltaMY = 0;
bool isMouseReturn = false;
XMVECTOR Camera_right;
XMVECTOR Camera_up;
void CameraUpdate(float deltaTime) {
	XMVECTOR up = XMVectorSet(0, 1, 0, 0);
	Camera_right = CONSTANT_BUFFER_DEFAULT::ViewMatrix.r[0];
	Camera_up = CONSTANT_BUFFER_DEFAULT::ViewMatrix.r[1];
	XMFLOAT3& scampos = CONSTANT_BUFFER_DEFAULT::SCamPos;

	if (WKeyPressed) {
		XMVECTOR c = deltaTime * CONSTANT_BUFFER_DEFAULT::AtVector;
		scampos = { scampos.x + c.m128_f32[0],scampos.y + c.m128_f32[1], scampos.z + c.m128_f32[2] };
	}
	if (SKeyPressed) {
		XMVECTOR c = -deltaTime * CONSTANT_BUFFER_DEFAULT::AtVector;
		scampos = { scampos.x + c.m128_f32[0],scampos.y + c.m128_f32[1], scampos.z + c.m128_f32[2] };
	}
	if (AKeyPressed) {
		XMVECTOR c = -deltaTime * Camera_right;
		scampos = { scampos.x + c.m128_f32[0],scampos.y + c.m128_f32[1], scampos.z + c.m128_f32[2] };
	}
	if (DKeyPressed) {
		XMVECTOR c = +deltaTime * Camera_right;
		scampos = { scampos.x + c.m128_f32[0],scampos.y + c.m128_f32[1], scampos.z + c.m128_f32[2] };
	}

	constexpr float CameraRotateRate = 0.01f;
	if (deltaMX != 0) {
		CONSTANT_BUFFER_DEFAULT::AtVector = CONSTANT_BUFFER_DEFAULT::AtVector + deltaMX * CameraRotateRate * Camera_right;
		CONSTANT_BUFFER_DEFAULT::AtVector = XMVector3Normalize(CONSTANT_BUFFER_DEFAULT::AtVector);
		deltaMX = 0;
	}
	if (deltaMY != 0) {
		CONSTANT_BUFFER_DEFAULT::AtVector = CONSTANT_BUFFER_DEFAULT::AtVector - deltaMY * CameraRotateRate * Camera_up;
		CONSTANT_BUFFER_DEFAULT::AtVector = XMVector3Normalize(CONSTANT_BUFFER_DEFAULT::AtVector);
		deltaMY = 0;
	}

	XMVECTOR& atvec = CONSTANT_BUFFER_DEFAULT::AtVector;
	XMFLOAT3 OBJ3 = { scampos.x+ atvec.m128_f32[0], scampos.y+ atvec.m128_f32[1], scampos.z+ atvec.m128_f32[2] };
	CONSTANT_BUFFER_DEFAULT::SetCamera(scampos, OBJ3, 1.0f, g_pRenderer->GetWindowWHRate());
	CONSTANT_BUFFER_DEFAULT::SetViewFrustum_Bounding();
}

void Update(float deltaTime) {
	t += deltaTime;
	g_fOffsetX += g_fSpeed;
	if (g_fOffsetX > 0.75f)
	{
		g_fSpeed *= -1.0f;
	}
	if (g_fOffsetX < -0.75f)
	{
		g_fSpeed *= -1.0f;
	}
	XMFLOAT3 pos = { 0, 0, 0 };
	XMFLOAT3 pointLightpos = { cosf(t) * 3, 0, -3 * sinf(t) };
	g_constBuff.GetCamModelCB(pos, { 0, g_fOffsetX, t }, { 0.1f, 0.1f, 0.1f });
	g_constBuff.pointLights = PointLight(pointLightpos, 100.0f, 100.0f);

	g_constBuff2.GetCamModelCB(pointLightpos, { 0, g_fOffsetX, t }, { 0.1f, 0.1f, 0.1f });
	g_constBuff2.pointLights = PointLight(pointLightpos, 100.0f, 100.0f);

	CameraUpdate(deltaTime);
}

void InputEvent(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_KEYDOWN:
	{
		if (wParam == 'W') {
			WKeyPressed = true;
		}
		if (wParam == 'S') {
			SKeyPressed = true;
		}
		if (wParam == 'A') {
			AKeyPressed = true;
		}
		if (wParam == 'D') {
			DKeyPressed = true;
		}
	}
	break;
	case WM_KEYUP:
		if (wParam == 'W') {
			WKeyPressed = false;
		}
		if (wParam == 'S') {
			SKeyPressed = false;
		}
		if (wParam == 'A') {
			AKeyPressed = false;
		}
		if (wParam == 'D') {
			DKeyPressed = false;
		}
		break;
	case WM_CHAR:
		break;
	case WM_LBUTTONDOWN:
		break;
	case WM_RBUTTONDOWN:
		break;
	case WM_MOUSEMOVE:
	{
		if (isMouseReturn == false) {
			deltaMX += (float)LOWORD(lParam) - mx;
			deltaMY += (float)HIWORD(lParam) - my;
			mx = (float)LOWORD(lParam);
			my = (float)HIWORD(lParam);
			RECT rt;
			GetWindowRect(hWnd, &rt);
			SetCursorPos((rt.right + rt.left) / 2, (rt.top + rt.bottom) / 2);
			isMouseReturn = true;
		}
		else {
			isMouseReturn = false;
			mx = (float)LOWORD(lParam);
			my = (float)HIWORD(lParam);
		}
		break;
	}
	}
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MY01CREATEDEVICE));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_MY01CREATEDEVICE);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
HWND InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
							  CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return nullptr;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return hWnd;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	InputEvent(hWnd, message, wParam, lParam);
	switch (message)
	{
		case WM_COMMAND:
			{
				int wmId = LOWORD(wParam);
				// Parse the menu selections:
				switch (wmId)
				{
					case IDM_ABOUT:
						DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
						break;
					case IDM_EXIT:
						DestroyWindow(hWnd);
						break;
					default:
						return DefWindowProc(hWnd, message, wParam, lParam);
				}
			}
			break;
		case WM_PAINT:
			{
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hWnd, &ps);
				// TODO: Add any drawing code that uses hdc here...
				EndPaint(hWnd, &ps);
			}
		case WM_SIZE:
		{
			if (g_pRenderer) {
				g_pRenderer->ClientWidth = LOWORD(lParam);
				g_pRenderer->ClientHeight = HIWORD(lParam);
				
				//RECT rect;
				//GetClientRect(hWnd, &rect);
				//DWORD	dwWndWidth = rect.right - rect.left;
				//DWORD	dwWndHeight = rect.bottom - rect.top;
				g_pRenderer->UpdateWindowSize(g_pRenderer->ClientWidth, g_pRenderer->ClientHeight);
			}
		}
			break;
		case WM_KEYDOWN:
		{
			if (wParam == VK_ESCAPE) {
				//SendMessageW(hWnd, WM_QUIT, wParam, lParam);
				PostQuitMessage(0);
			}

			if (wParam == VK_F9) {
				g_pRenderer->ChangeSwapChainState();
			}
			break;
		}
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
		case WM_INITDIALOG:
			return (INT_PTR)TRUE;

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
			{
				EndDialog(hDlg, LOWORD(wParam));
				return (INT_PTR)TRUE;
			}
			break;
	}
	return (INT_PTR)FALSE;
}

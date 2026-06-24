int dbgc[128] = {};

#include "stdafx.h"
#include "main.h"
#include "Render.h"
#include "Game.h"
#include "GameObject.h"
#include "MeshSimplifier.h"
#include "Utill_Wave.h"

vector<ID3D12Resource*> GPUResource::TextureLoadedUploadBuffers;
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
bool g_playReloadSound = false;   // [sfx] set in GameObject.cpp when local player's reload starts; played in the main loop
bool g_playGunSound = false;      // [sfx] set in Game.cpp on STC_PlayerFire for the local player (an actual shot); played in the main loop
bool g_suppressLoadingScreen = false;   // [loading] set during seamless zone transfer so the loading screen is NOT shown
bool g_inJobSelect = false;             // [jobselect] true while the job-selection screen is up; disables in-game input in WndProc

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdParam, int nCmdShow)
{
	{
		HANDLE hMainThread = GetCurrentThread();

		if (hMainThread == NULL) {
			printf("Failed to get the main thread handle\n");
			return 1;
		}

		DWORD_PTR dwThreadAffinityMask = 0xFFFF; // 예: Core 0
		if (!SetThreadAffinityMask(hMainThread, dwThreadAffinityMask)) {
			printf("SetThreadAffinityMask failed\n");
			return 1;
		}

		// Time Critical은 위험하지 않을까? ..
		if (!SetThreadPriority(hMainThread, THREAD_PRIORITY_HIGHEST)) {
			printf("SetThreadPriority failed\n");
			return 1;
		}

		int priority = GetThreadPriority(hMainThread);
		if (priority == THREAD_PRIORITY_ERROR_RETURN) {
			printf("GetThreadPriority failed\n");
			return 1;
		}
		printf("Thread priority is set to %d\n", priority);

		// yeild and revisit -> update priority and affinity.
		Sleep(100);
	}

	// 오류등이 한글로 표시되도록 한다.
	wcout.imbue(locale("korean"));
	//WSA �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;
	
	GameObject::StaticInit();
	PrintOffset();

	//Audio Init + BGM
	wave_master_channel.Init(23);
	waveOutSetVolume(wave_master_channel.hWaveDev, 0x40004000);   // [audio] master volume ~25%. Louder: raise (0x80008000=50%, 0xC000C000=75%, 0xFFFFFFFF=100%). Quieter: lower (0x20002000≈12%).
	WaveDataStruct wd = CreateWaveFromFile(L"Resources/Sound/Soundtrack002.wav");
	WaveChannel* channel0 = NewChannel();
	channel0->pushWave(wd);

	gd.Factory_Adaptor_Output_Init();
	
	RECT workArea = {};
	SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
	const int maxStartWidth = (int)((workArea.right - workArea.left) * 0.90f);
	const int maxStartHeight = (int)((workArea.bottom - workArea.top) * 0.90f);
	int bestArea = 0;
	for (int i = 0; i < gd.EnableFullScreenMode_Resolusions.size(); ++i) {
		const ResolutionStruct& res = gd.EnableFullScreenMode_Resolusions[i];
		float rate = (float)res.height / (float)res.width;
		if (fabsf(rate - 0.5625f) >= 0.001f) continue;
		if ((int)res.width > maxStartWidth || (int)res.height > maxStartHeight) continue;

		const int area = (int)(res.width * res.height);
		if (area > bestArea) {
			bestArea = area;
			resolutionLevel = i;
		}
	}
	if (bestArea == 0) {
		for (int i = 0; i < gd.EnableFullScreenMode_Resolusions.size(); ++i) {
			const ResolutionStruct& res = gd.EnableFullScreenMode_Resolusions[i];
			if ((int)res.width > maxStartWidth || (int)res.height > maxStartHeight) continue;

			const int area = (int)(res.width * res.height);
			if (area > bestArea) {
				bestArea = area;
				resolutionLevel = i;
			}
		}
	}
	gd.ClientFrameWidth = gd.EnableFullScreenMode_Resolusions[resolutionLevel].width;
	gd.ClientFrameHeight = gd.EnableFullScreenMode_Resolusions[resolutionLevel].height;


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

	if (RegisterClassEx(&WndClass) == 0) {
		WSACleanup();
		return 1;
	}

	RECT windowRect = { 0, 0, (LONG)gd.ClientFrameWidth, (LONG)gd.ClientFrameHeight };
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
	const int windowWidth = windowRect.right - windowRect.left;
	const int windowHeight = windowRect.bottom - windowRect.top;
	const int windowX = workArea.left + max(0, ((workArea.right - workArea.left) - windowWidth) / 2);
	const int windowY = workArea.top + max(0, ((workArea.bottom - workArea.top) - windowHeight) / 2);

	hWnd = CreateWindow(lpszClass, lpszWindowName, WS_OVERLAPPEDWINDOW, windowX, windowY, windowWidth, windowHeight, NULL, (HMENU)NULL, hInstance, NULL);
	if (hWnd == NULL) {
		WSACleanup();
		return 1;
	}
	gd.RawMouse.usUsagePage = 0x01;
	gd.RawMouse.usUsage = 0x02;
	gd.RawMouse.dwFlags = 0;
	gd.RawMouse.hwndTarget = hWnd;
	RegisterRawInputDevices(&gd.RawMouse, 1, sizeof(gd.RawMouse));
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	ShowCursor(FALSE);

	//init.
	gd.Init();
	game.Init();
	GetKeyboardState(game.pKeyBuffer);
	char e2eModeBuffer[8] = {};
	const bool dungeonReturnE2E = GetEnvironmentVariableA("SYNCFPS_DUNGEON_RETURN_E2E",
		e2eModeBuffer, (DWORD)sizeof(e2eModeBuffer)) > 0;
	char zoneE2EBuffer[8] = {};
	const bool openWorldZoneE2E = GetEnvironmentVariableA("SYNCFPS_OPEN_WORLD_ZONE_E2E",
		zoneE2EBuffer, (DWORD)sizeof(zoneE2EBuffer)) > 0;
	const bool automatedE2E = dungeonReturnE2E || openWorldZoneE2E;

	// [start screen] show StartScreen.png at launch; begin loading only after any key/click is pressed.
	int chosenJob = automatedE2E ? 0 : -1;   // PlayerJob enum index, or -1 if none chosen
	string chosenPlayerId = automatedE2E ? "Player1" : "";
	if (!automatedE2E) {
		g_inJobSelect = true;
		ShowCursor(TRUE);
		MSG dm;
		while (::PeekMessage(&dm, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE)) {}
		while (::PeekMessage(&dm, NULL, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE)) {}

		ULONGLONG _startScreenT0 = GetTickCount64();
		bool idInvalid = false;
		bool idTooLong = false;
		bool started = false;

		auto HandlePlayerIdChar = [&](char ch) {
			const bool isDigit = ch >= '0' && ch <= '9';
			const bool isUpper = ch >= 'A' && ch <= 'Z';
			const bool isLower = ch >= 'a' && ch <= 'z';
			if (ch == '\b') {
				if (!chosenPlayerId.empty()) {
					chosenPlayerId.pop_back();
				}
				idInvalid = false;
				idTooLong = false;
			}
			else if (ch == '\r' || ch == '\n') {
				if (!chosenPlayerId.empty()) {
					started = true;
				}
				else {
					idInvalid = true;
					idTooLong = false;
				}
			}
			else if (isDigit || isUpper || isLower) {
				if (chosenPlayerId.size() < CTS_ClientHello_Header::MaxPlayerIdLength) {
					chosenPlayerId.push_back(ch);
					idInvalid = false;
					idTooLong = false;
				}
				else {
					idInvalid = false;
					idTooLong = false;
				}
			}
			else if ((unsigned char)ch >= 32) {
				idInvalid = false;
				idTooLong = false;
			}
		};

		while (!started) {
			MSG sm;
			while (::PeekMessage(&sm, NULL, 0, 0, PM_REMOVE)) {
				if (sm.message == WM_QUIT) { WSACleanup(); return 0; }
				if (sm.message == WM_CHAR) {
					HandlePlayerIdChar((char)sm.wParam);
					continue;
				}
				if (sm.message == WM_KEYDOWN && sm.wParam == VK_BACK) {
					HandlePlayerIdChar('\b');
					continue;
				}
				if (sm.message == WM_KEYDOWN && sm.wParam == VK_RETURN) {
					HandlePlayerIdChar('\r');
					continue;
				}
				::TranslateMessage(&sm);
				::DispatchMessage(&sm);
			}

			game.DrawStartScreen(chosenPlayerId.c_str(), idInvalid, idTooLong);
		}
	}

	// [jobselect] Job-selection screen: shown after the start screen, before connecting. Hover a card in
	// the 3x3 grid, left-click to select, then click CONFIRM to lock it in (CANCEL clears). The chosen job
	// is sent to the server with CTS_ChangeJob once the connection is confirmed (in the main loop below).
	if (!automatedE2E) {
		g_inJobSelect = true;
		ShowCursor(TRUE);   // [jobselect] reveal the OS cursor for clicking (it was hidden at launch)
		MSG dm;
		while (::PeekMessage(&dm, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE)) {}
		while (::PeekMessage(&dm, NULL, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE)) {}

		auto PtIn = [](const vec4& r, float x, float y) -> bool {
			return x >= r.x && x <= r.z && y >= r.y && y <= r.w;
		};

		int  selected = -1;
		bool prevLBtn = game.LBtnDown;
		bool done = false;
		while (!done) {
			MSG sm;
			while (::PeekMessage(&sm, NULL, 0, 0, PM_REMOVE)) {
				if (sm.message == WM_QUIT) { WSACleanup(); return 0; }
				if (sm.message == WM_KEYDOWN && sm.wParam == VK_RETURN) {
					if (selected >= 0) { chosenJob = selected; done = true; }
					continue;
				}
				::TranslateMessage(&sm);
				::DispatchMessage(&sm);
			}

			vec4 cards[9], confirmR, cancelR;
			game.ComputeJobSelectLayout(cards, confirmR, cancelR);
			const float cx = game.CurrentCursorPos.x;
			const float cy = game.CurrentCursorPos.y;

			int hovered = -1;
			for (int i = 0; i < 9; ++i) { if (PtIn(cards[i], cx, cy)) { hovered = i; break; } }
			const bool overConfirm = PtIn(confirmR, cx, cy);
			const bool overCancel  = PtIn(cancelR, cx, cy);

			const bool click = game.LBtnDown && !prevLBtn;
			prevLBtn = game.LBtnDown;
			if (click) {
				if (hovered >= 0)                       selected = hovered;
				else if (overCancel)                    selected = -1;
				else if (overConfirm) {
					if (selected >= 0) { chosenJob = selected; done = true; }
				}
			}

			game.DrawJobSelectScreen(hovered, selected, overConfirm, overCancel);
		}
		ShowCursor(FALSE);   // [jobselect] hide the cursor again for FPS gameplay
		g_inJobSelect = false;
	}

	constexpr unsigned short InitServerPort = 9073;   // open world (zone 73 = the player's spawn zone). Use 9100 to test the dungeon directly.
	const char* IP0 = "127.0.0.1";
	char configuredServerIPBuffer[64] = {};
	const DWORD configuredServerIPLength = GetEnvironmentVariableA("SYNCFPS_SERVER_IP",
		configuredServerIPBuffer, (DWORD)sizeof(configuredServerIPBuffer));
	const char* configuredServerIP = (configuredServerIPLength > 0 &&
		configuredServerIPLength < sizeof(configuredServerIPBuffer)) ? configuredServerIPBuffer : IP0;
	printf("[ClientConnect] initial server=%s:%u\n", configuredServerIP, (unsigned)InitServerPort);
	bool Connected = client.Init(configuredServerIP, InitServerPort);

	if (Connected == false) {
		WSACleanup();
		return 0;
	}
	game.expectedInitialJob = chosenJob;
	game.isInitialJobConfirmed = (chosenJob < 0);
	game.isServerSyncComplete = false;
	strcpy_s(game.PlayerId, sizeof(game.PlayerId), chosenPlayerId.c_str());

	CTS_ClientHello_Header hello;
	hello.size = sizeof(CTS_ClientHello_Header);
	hello.st = CTS_Protocol::ClientHello;
	strcpy_s(hello.playerId, sizeof(hello.playerId), chosenPlayerId.c_str());
	client.send_all((char*)&hello, sizeof(CTS_ClientHello_Header), 0);

	// [jobselect] Chosen job is sent from the main loop once the connection is confirmed (the server
	// caches its Player* per recv buffer and only has it after ClientHello; sending now would be dropped).
	bool jobSent = (chosenJob < 0);   // nothing to send if no job was chosen

	wchar_t m_pszFrameRate[32] = L"Pulse City Client 001 ____FPS";
	constexpr double MaxFPSFlow = 10000;
	constexpr double InvHZ = 1.0 / (double)QUERYPERFORMANCE_HZ;
	double FPSflow = 0;
	double DeltaFlow = 0;
	double perfFlow = 0.0;
	double perfPacedFrameMs = 0.0;
	double perfFrameMs = 0.0;
	double perfRenderMs = 0.0;
	double perfUpdateMs = 0.0;
	double perfGPUWaitMs = 0.0;
	double perfGPUPreWaitMs = 0.0;
	double perfGPUShadowWaitMs = 0.0;
	double perfGPUMainWaitMs = 0.0;
	double perfGPUComputeWaitMs = 0.0;
	double perfGPUFinalWaitMs = 0.0;
	double perfPresentMs = 0.0;
	double perfAutoLODMainInstances = 0.0;
	double perfAutoLODMainDraws = 0.0;
	double perfAutoLODMainTrimmedSubMeshes = 0.0;
	double perfAutoLODShadowSourceSubMeshes = 0.0;
	double perfAutoLODShadowRenderedSubMeshes = 0.0;
	double perfAutoLODShadowCulledObjects = 0.0;
	double perfAutoLODShadowCulledSubMeshes = 0.0;
	double perfRaytracingLODVisitedObjects = 0.0;
	double perfRaytracingLODDescMisses = 0.0;
	double perfRaytracingLODTypeRejects = 0.0;
	double perfRaytracingLODDistanceRejects = 0.0;
	double perfRaytracingLODMeshMisses = 0.0;
	double perfRaytracingLODNoMeshMisses = 0.0;
	double perfRaytracingLODReductionRejects = 0.0;
	double perfRaytracingLODBLASMisses = 0.0;
	double perfRaytracingLODHitGroupMisses = 0.0;
	double perfRaytracingLODCheckedMeshes = 0.0;
	double perfRaytracingLODAppliedMeshes = 0.0;
	double perfMaxFrameMs = 0.0;
	int perfFrameCount = 0;
	bool perfLODState = AutoLOD_IsModelLODRenderActive();
	int shadowStabilityRecoverBuckets = 0;
	auto ResetPerfAccum = [&]() {
		perfFlow = 0.0;
		perfPacedFrameMs = 0.0;
		perfFrameMs = 0.0;
		perfRenderMs = 0.0;
		perfUpdateMs = 0.0;
		perfGPUWaitMs = 0.0;
		perfGPUPreWaitMs = 0.0;
		perfGPUShadowWaitMs = 0.0;
		perfGPUMainWaitMs = 0.0;
		perfGPUComputeWaitMs = 0.0;
		perfGPUFinalWaitMs = 0.0;
		perfPresentMs = 0.0;
		perfAutoLODMainInstances = 0.0;
		perfAutoLODMainDraws = 0.0;
		perfAutoLODMainTrimmedSubMeshes = 0.0;
		perfAutoLODShadowSourceSubMeshes = 0.0;
		perfAutoLODShadowRenderedSubMeshes = 0.0;
		perfAutoLODShadowCulledObjects = 0.0;
		perfAutoLODShadowCulledSubMeshes = 0.0;
		perfRaytracingLODVisitedObjects = 0.0;
		perfRaytracingLODDescMisses = 0.0;
		perfRaytracingLODTypeRejects = 0.0;
		perfRaytracingLODDistanceRejects = 0.0;
		perfRaytracingLODMeshMisses = 0.0;
		perfRaytracingLODNoMeshMisses = 0.0;
		perfRaytracingLODReductionRejects = 0.0;
		perfRaytracingLODBLASMisses = 0.0;
		perfRaytracingLODHitGroupMisses = 0.0;
		perfRaytracingLODCheckedMeshes = 0.0;
		perfRaytracingLODAppliedMeshes = 0.0;
		perfMaxFrameMs = 0.0;
		perfFrameCount = 0;
	};
	auto SetShadowStabilityLevel = [&](int newLevel) {
		newLevel = (std::max)(0, (std::min)(newLevel, 3));
		if (game.AutoLODShadowStabilityLevel == newLevel) return;
		game.AutoLODShadowStabilityLevel = newLevel;
		shadowStabilityRecoverBuckets = 0;
		char dbg[96];
		sprintf_s(dbg, "[AutoLOD] shadow stability level=%d\n", game.AutoLODShadowStabilityLevel);
		OutputDebugStringA(dbg);
	};
	ui64 ft = GetTicks();
	double loadingDiagnosticFlow = 0.0;
	int dungeonReturnE2EStage = 0;
	double dungeonReturnE2EFlow = 0.0;
	int openWorldZoneE2EStage = 0;
	double openWorldZoneE2EFlow = 0.0;
	while (1) {
		game.isPrepared = game.IsPresentationReady();

		// [jobselect] Send the picked job once, after the connection is confirmed (same path the in-game
		// 1-9 keys use). The server's Player* is valid in a later recv buffer, so this is applied.
		if (!jobSent && game.isPreparedClientIndex && game.isServerSyncComplete && game.player != nullptr) {
			CTS_ChangeJob_Header jobHeader;
			jobHeader.size = sizeof(CTS_ChangeJob_Header);
			jobHeader.st = CTS_Protocol::ChangeJob;
			jobHeader.job = (PlayerJob)chosenJob;
			DWORD sent = client.send_all((char*)&jobHeader, sizeof(CTS_ChangeJob_Header), 0);
			jobSent = (sent == sizeof(CTS_ChangeJob_Header));
		}

		ui64 et = GetTicks();
		double f = (double)(et - ft) * InvHZ;
		if (dungeonReturnE2E) {
			dungeonReturnE2EFlow += f;
			if (dungeonReturnE2EStage == 0 && game.isPrepared && game.currentZoneId < 100) {
				CTS_PartyCreate_Header header;
				client.send_all((char*)&header, sizeof(header), 0);
				printf("[DungeonReturnE2E] initial-ready zone=%d; PartyCreate sent\n", game.currentZoneId);
				fflush(stdout);
				dungeonReturnE2EStage = 1;
				dungeonReturnE2EFlow = 0.0;
			}
			else if (dungeonReturnE2EStage == 1 && dungeonReturnE2EFlow >= 1.0 &&
				game.m_dungeonQueue.active && game.m_dungeonQueue.partyId >= 0) {
				CTS_DungeonStart_Header header;
				client.send_all((char*)&header, sizeof(header), 0);
				printf("[DungeonReturnE2E] party-ready id=%d; DungeonStart sent\n", game.m_dungeonQueue.partyId);
				fflush(stdout);
				dungeonReturnE2EStage = 2;
				dungeonReturnE2EFlow = 0.0;
			}
			else if (dungeonReturnE2EStage == 2 && game.isPrepared && game.currentZoneId >= 100) {
				CTS_DungeonAbort_Header header;
				client.send_all((char*)&header, sizeof(header), 0);
				printf("[DungeonReturnE2E] dungeon-ready zone=%d; DungeonAbort sent\n", game.currentZoneId);
				fflush(stdout);
				dungeonReturnE2EStage = 3;
				dungeonReturnE2EFlow = 0.0;
			}
			else if (dungeonReturnE2EStage == 3 && game.isPrepared && game.currentZoneId < 100) {
				printf("[DungeonReturnE2E] PASS returned-ready zone=%d player=%d map=%d sync=%d\n",
					game.currentZoneId, game.player != nullptr, game.isMapInit, game.isServerSyncComplete);
				fflush(stdout);
				dungeonReturnE2EStage = 4;
				PostMessage(hWnd, WM_CLOSE, 0, 0);
			}
			else if (dungeonReturnE2EStage > 0 && dungeonReturnE2EStage < 4 && dungeonReturnE2EFlow >= 45.0) {
				printf("[DungeonReturnE2E] TIMEOUT stage=%d zone=%d prepared=%d player=%d index=%d map=%d global=%d sync=%d jobAck=%d\n",
					dungeonReturnE2EStage, game.currentZoneId, game.isPrepared, game.player != nullptr,
					game.isPreparedClientIndex, game.isMapInit, game.isGlobalAssetInit,
					game.isServerSyncComplete, game.isInitialJobConfirmed);
				fflush(stdout);
				dungeonReturnE2EStage = 4;
				PostMessage(hWnd, WM_CLOSE, 0, 0);
			}
		}
		if (openWorldZoneE2E) {
			openWorldZoneE2EFlow += f;
			if (openWorldZoneE2EStage == 1 && game.player != nullptr) {
				game.player->m_yaw = 0.0f;
				game.player->m_pitch = 0.0f;
			}
			if (openWorldZoneE2EStage == 0 && game.isPrepared && game.currentZoneId == 73) {
				CTS_KeyInput_Header header;
				header.Key = 'W';
				header.isdown = true;
				client.send_all((char*)&header, sizeof(header), 0);
				printf("[OpenWorldZoneE2E] initial-ready zone=73; move +X started\n");
				fflush(stdout);
				openWorldZoneE2EStage = 1;
				openWorldZoneE2EFlow = 0.0;
			}
			else if (openWorldZoneE2EStage == 1 && game.isPrepared && game.currentZoneId == 83) {
				CTS_KeyInput_Header header;
				header.Key = 'W';
				header.isdown = false;
				client.send_all((char*)&header, sizeof(header), 0);
				printf("[OpenWorldZoneE2E] PASS zone=83 player=%d map=%d sync=%d\n",
					game.player != nullptr, game.isMapInit, game.isServerSyncComplete);
				fflush(stdout);
				openWorldZoneE2EStage = 2;
				openWorldZoneE2EFlow = 0.0;
			}
			else if (openWorldZoneE2EStage == 2 && openWorldZoneE2EFlow >= 10.0) {
				openWorldZoneE2EStage = 3;
				PostMessage(hWnd, WM_CLOSE, 0, 0);
			}
			else if (openWorldZoneE2EStage == 1 && openWorldZoneE2EFlow >= 45.0) {
				printf("[OpenWorldZoneE2E] TIMEOUT zone=%d prepared=%d player=%d map=%d sync=%d\n",
					game.currentZoneId, game.isPrepared, game.player != nullptr,
					game.isMapInit, game.isServerSyncComplete);
				fflush(stdout);
				openWorldZoneE2EStage = 2;
				PostMessage(hWnd, WM_CLOSE, 0, 0);
			}
		}
		if (!game.isPrepared) {
			loadingDiagnosticFlow += f;
			if (loadingDiagnosticFlow >= 1.0) {
				loadingDiagnosticFlow = 0.0;
				int localJob = game.player ? game.player->m_currentJob : -1;
				int localWeapon = game.player ? game.player->m_currentWeaponType : -1;
				char readyDbg[320] = {};
				sprintf_s(readyDbg,
					"[LoadingReady] player=%d index=%d map=%d global=%d serverSync=%d jobAck=%d expectedJob=%d localJob=%d weapon=%d boneQ=%zu renderQ=%zu jobSent=%d\n",
					game.player != nullptr, game.isPreparedClientIndex, game.isMapInit, game.isGlobalAssetInit,
					game.isServerSyncComplete, game.isInitialJobConfirmed, game.expectedInitialJob,
					localJob, localWeapon, game.m_pendingSkinBoneInit.size(),
					game.m_pendingSkinRenderEnable.size(), jobSent);
				OutputDebugStringA(readyDbg);
				printf("%s", readyDbg);
				fflush(stdout);
			}
		}
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
				const ui64 perfFrameStart = GetTicks();
				bool didRender = false;
				if (game.isPrepared) {
					//gd.AverageSecPer60Start(0);
					const ui64 perfRenderStart = GetTicks();
					if (gd.isRaytracingRender) {
						game.Render_RayTracing();
					}
					else {
						game.Render();
					}
					perfRenderMs += 1000.0 * double(GetTicks() - perfRenderStart) / double(QUERYPERFORMANCE_HZ);
					perfGPUWaitMs += game.PerfGPUWaitMs;
					perfGPUPreWaitMs += game.PerfGPUPreWaitMs;
					perfGPUShadowWaitMs += game.PerfGPUShadowWaitMs;
					perfGPUMainWaitMs += game.PerfGPUMainWaitMs;
					perfGPUComputeWaitMs += game.PerfGPUComputeWaitMs;
					perfGPUFinalWaitMs += game.PerfGPUFinalWaitMs;
					perfPresentMs += game.PerfPresentMs;
					perfAutoLODMainInstances += double(game.PerfAutoLODMainInstances);
					perfAutoLODMainDraws += double(game.PerfAutoLODMainDraws);
					perfAutoLODMainTrimmedSubMeshes += double(game.PerfAutoLODMainTrimmedSubMeshes);
					perfAutoLODShadowSourceSubMeshes += double(game.PerfAutoLODShadowSourceSubMeshes);
					perfAutoLODShadowRenderedSubMeshes += double(game.PerfAutoLODShadowRenderedSubMeshes);
					perfAutoLODShadowCulledObjects += double(game.PerfAutoLODShadowCulledObjects);
					perfAutoLODShadowCulledSubMeshes += double(game.PerfAutoLODShadowCulledSubMeshes);
					perfRaytracingLODVisitedObjects += double(game.PerfRaytracingLODVisitedObjects);
					perfRaytracingLODDescMisses += double(game.PerfRaytracingLODDescMisses);
					perfRaytracingLODTypeRejects += double(game.PerfRaytracingLODTypeRejects);
					perfRaytracingLODDistanceRejects += double(game.PerfRaytracingLODDistanceRejects);
					perfRaytracingLODMeshMisses += double(game.PerfRaytracingLODMeshMisses);
					perfRaytracingLODNoMeshMisses += double(game.PerfRaytracingLODNoMeshMisses);
					perfRaytracingLODReductionRejects += double(game.PerfRaytracingLODReductionRejects);
					perfRaytracingLODBLASMisses += double(game.PerfRaytracingLODBLASMisses);
					perfRaytracingLODHitGroupMisses += double(game.PerfRaytracingLODHitGroupMisses);
					perfRaytracingLODCheckedMeshes += double(game.PerfRaytracingLODCheckedMeshes);
					perfRaytracingLODAppliedMeshes += double(game.PerfRaytracingLODAppliedMeshes);
					didRender = true;
					g_suppressLoadingScreen = false;   // back to normal -> loading screen allowed again
					//gd.AverageSecPer60End(0);
				}
				else {
					// [loading] not ready yet. Show Loading.png for initial load / dungeon entry,
					// but NOT during a seamless zone transfer (g_suppressLoadingScreen).
					if (!g_suppressLoadingScreen) game.DrawLoadingScreen();
				}
				//gd.AverageSecPer60Start(1);
				const ui64 perfUpdateStart = GetTicks();
				game.Update();
				// [sfx] reload sound: flag is set in GameObject.cpp when the local player's reload starts
				if (g_playReloadSound) {
					g_playReloadSound = false;
					static WaveDataStruct s_reload = CreateWaveFromFile(L"Resources/Sound/reload.wav");
					static WaveChannel* s_reloadCh = NewChannel();
					if (s_reloadCh) s_reloadCh->pushWave(s_reload);
				}
				// [sfx] gunshot: flag is set in Game.cpp on STC_PlayerFire for the local player (a real shot)
				if (g_playGunSound) {
					g_playGunSound = false;
					static WaveDataStruct s_gunshot = CreateWaveFromFile(L"Resources/Sound/gun.wav");
					static WaveChannel* s_gunCh = NewChannel();
					if (s_gunCh) s_gunCh->pushWave(s_gunshot);
				}
				const double updateMs = 1000.0 * double(GetTicks() - perfUpdateStart) / double(QUERYPERFORMANCE_HZ);
				//gd.AverageSecPer60End(1);
				if (didRender) {
					const bool currentLODState = AutoLOD_IsModelLODRenderActive();
					if (currentLODState != perfLODState) {
						perfLODState = currentLODState;
						if (!perfLODState) {
							SetShadowStabilityLevel(0);
						}
						ResetPerfAccum();
					}
					else {
						perfUpdateMs += updateMs;
						const double frameMs = 1000.0 * double(GetTicks() - perfFrameStart) / double(QUERYPERFORMANCE_HZ);
						perfPacedFrameMs += DeltaFlow * 1000.0;
						perfFrameMs += frameMs;
						perfMaxFrameMs = (std::max)(perfMaxFrameMs, frameMs);
						perfFlow += game.DeltaTime;
						++perfFrameCount;
						if (perfFlow >= 2.0 && perfFrameCount > 0) {
							const double invFrameCount = 1.0 / double(perfFrameCount);
							const double avgPacedFrameMs = perfPacedFrameMs * invFrameCount;
							const double avgFrameMs = perfFrameMs * invFrameCount;
							const double avgRenderMs = perfRenderMs * invFrameCount;
							const double avgUpdateMs = perfUpdateMs * invFrameCount;
							const double avgGPUWaitMs = perfGPUWaitMs * invFrameCount;
							const double avgGPUPreWaitMs = perfGPUPreWaitMs * invFrameCount;
							const double avgGPUShadowWaitMs = perfGPUShadowWaitMs * invFrameCount;
							const double avgGPUMainWaitMs = perfGPUMainWaitMs * invFrameCount;
							const double avgGPUComputeWaitMs = perfGPUComputeWaitMs * invFrameCount;
							const double avgGPUFinalWaitMs = perfGPUFinalWaitMs * invFrameCount;
							const double avgPresentMs = perfPresentMs * invFrameCount;
							const double avgAutoLODMainInstances = perfAutoLODMainInstances * invFrameCount;
							const double avgAutoLODMainDraws = perfAutoLODMainDraws * invFrameCount;
							const double avgAutoLODMainTrimmedSubMeshes = perfAutoLODMainTrimmedSubMeshes * invFrameCount;
							const double avgAutoLODShadowSourceSubMeshes = perfAutoLODShadowSourceSubMeshes * invFrameCount;
							const double avgAutoLODShadowRenderedSubMeshes = perfAutoLODShadowRenderedSubMeshes * invFrameCount;
							const double avgAutoLODShadowCulledObjects = perfAutoLODShadowCulledObjects * invFrameCount;
							const double avgAutoLODShadowCulledSubMeshes = perfAutoLODShadowCulledSubMeshes * invFrameCount;
							const double avgRaytracingLODVisitedObjects = perfRaytracingLODVisitedObjects * invFrameCount;
							const double avgRaytracingLODDescMisses = perfRaytracingLODDescMisses * invFrameCount;
							const double avgRaytracingLODTypeRejects = perfRaytracingLODTypeRejects * invFrameCount;
							const double avgRaytracingLODDistanceRejects = perfRaytracingLODDistanceRejects * invFrameCount;
							const double avgRaytracingLODMeshMisses = perfRaytracingLODMeshMisses * invFrameCount;
							const double avgRaytracingLODNoMeshMisses = perfRaytracingLODNoMeshMisses * invFrameCount;
							const double avgRaytracingLODReductionRejects = perfRaytracingLODReductionRejects * invFrameCount;
							const double avgRaytracingLODBLASMisses = perfRaytracingLODBLASMisses * invFrameCount;
							const double avgRaytracingLODHitGroupMisses = perfRaytracingLODHitGroupMisses * invFrameCount;
							const double avgRaytracingLODCheckedMeshes = perfRaytracingLODCheckedMeshes * invFrameCount;
							const double avgRaytracingLODAppliedMeshes = perfRaytracingLODAppliedMeshes * invFrameCount;
							const double avgCPURecordMs = (std::max)(0.0, avgRenderMs - avgGPUWaitMs - avgPresentMs);
							if (perfLODState) {
								const bool heavyFrame =
									avgFrameMs > 20.0 ||
									(avgFrameMs > 18.0 && avgGPUShadowWaitMs > 5.0) ||
									(avgFrameMs > 16.8 && perfMaxFrameMs > 32.0 && avgGPUShadowWaitMs > 4.0);
								if (heavyFrame) {
									SetShadowStabilityLevel(game.AutoLODShadowStabilityLevel + 1);
								}
								else if (avgFrameMs < 14.5 && perfMaxFrameMs < 24.0) {
									++shadowStabilityRecoverBuckets;
									if (shadowStabilityRecoverBuckets >= 2) {
										SetShadowStabilityLevel(game.AutoLODShadowStabilityLevel - 1);
									}
								}
								else {
									shadowStabilityRecoverBuckets = 0;
								}
							}
							else {
								SetShadowStabilityLevel(0);
							}
							char perfDbg[1792];
							sprintf_s(perfDbg,
								"[Perf] lod=%s shadowQ=%d fps=%.1f frame=%.2fms workFps=%.1f work=%.2fms maxWork=%.2fms render=%.2fms update=%.2fms gpuWait=%.2fms gpuPre=%.2fms gpuShadow=%.2fms gpuMain=%.2fms gpuCompute=%.2fms gpuFinal=%.2fms present=%.2fms cpuRecord=%.2fms lodInst=%.1f->%.1f lodTrim=%.1f shLOD=%.1f->%.1f shCull=%.1f/%.1f rtLOD=%.1f/%.1f rtVisit=%.1f rtMiss=%.1f/%.1f/%.1f/%.1f rtMesh=%.1f/%.1f/%.1f/%.1f samples=%d\n",
								perfLODState ? "ON" : "OFF",
								game.AutoLODShadowStabilityLevel,
								avgPacedFrameMs > 0.0 ? 1000.0 / avgPacedFrameMs : 0.0,
								avgPacedFrameMs,
								avgFrameMs > 0.0 ? 1000.0 / avgFrameMs : 0.0,
								avgFrameMs,
								perfMaxFrameMs,
								avgRenderMs,
								avgUpdateMs,
								avgGPUWaitMs,
								avgGPUPreWaitMs,
								avgGPUShadowWaitMs,
								avgGPUMainWaitMs,
								avgGPUComputeWaitMs,
								avgGPUFinalWaitMs,
								avgPresentMs,
								avgCPURecordMs,
								avgAutoLODMainInstances,
								avgAutoLODMainDraws,
								avgAutoLODMainTrimmedSubMeshes,
								avgAutoLODShadowSourceSubMeshes,
								avgAutoLODShadowRenderedSubMeshes,
								avgAutoLODShadowCulledObjects,
								avgAutoLODShadowCulledSubMeshes,
								avgRaytracingLODAppliedMeshes,
								avgRaytracingLODCheckedMeshes,
								avgRaytracingLODVisitedObjects,
								avgRaytracingLODDescMisses,
								avgRaytracingLODTypeRejects,
								avgRaytracingLODDistanceRejects,
								avgRaytracingLODMeshMisses,
								avgRaytracingLODNoMeshMisses,
								avgRaytracingLODReductionRejects,
								avgRaytracingLODBLASMisses,
								avgRaytracingLODHitGroupMisses,
								perfFrameCount);
							OutputDebugStringA(perfDbg);
							ResetPerfAccum();
						}
					}
				}
				
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
	game.evt.hWnd = hWnd;
	game.evt.uMsg = uMsg;
	game.evt.wParam = wParam;
	game.evt.lParam = lParam;
	//game.Event(WinEvent(hWnd, uMsg, wParam, lParam));

	//Common Event
	switch (uMsg) {
		//(14 line) outer code start : microsoft copilot. - FPS Camera Movement
	case WM_CREATE:
		GetClientRect(hWnd, &rt);
		break;
	case WM_KEYDOWN:
		game.CurrentKeyDown = wParam;
		//GetKeyboardState(game.pKeyBuffer);
		break;
	case WM_MOUSEMOVE:
		game.CurrentCursorPos.x = (float)LOWORD(lParam) - 0.5 * (float)gd.ClientFrameWidth;
		game.CurrentCursorPos.y = -1.0f * ((float)HIWORD(lParam) - 0.5 * (float)gd.ClientFrameHeight);
		//game.WindowNormalizeCoordToDirectXRenderCoord_vec4(game.CurrentCursorPos, gd.ClientFrameWidth, gd.ClientFrameHeight);
		break;
	case WM_LBUTTONDOWN:
		game.LBtnDown = true;
		if (game.StatWindowOpen == false) {
			game.HandlePartyClick(); // [party] hit-test the lobby party buttons (menu / join list / room)
		}
		break;
	case WM_LBUTTONUP:
		game.LBtnDown = false;
		break;
	case WM_RBUTTONDOWN:
		game.RBtnDown = true;
		break;
	case WM_RBUTTONUP:
		game.RBtnDown = false;
		break;
	case WM_CHAR:
		game.CurrentCompleteCharactor = wParam;
	break;
	case WM_DESTROY:
	{
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

	//Game Mode input Event
	if (game.mainPageStack.size() == 0 && !g_inJobSelect && !game.StatWindowOpen) {   // [jobselect] don't route input to in-game path during job select
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
		case WM_KEYDOWN:
			switch (wParam) {
			case 'R':
			{
				if (!(lParam & (1 << 30))) {
					CTS_KeyInput_Header header;
					header.size = sizeof(CTS_KeyInput_Header);
					header.st = CTS_Protocol::KeyInput;
					header.Key = InputID::KeyboardR;
					header.isdown = true;
					client.send_all((char*)&header, sizeof(CTS_KeyInput_Header), 0);
				}
			}
			break;
			case 'X':
			{
				if (!(lParam & (1 << 30))) {
					CTS_KeyInput_Header header;
					header.size = sizeof(CTS_KeyInput_Header);
					header.st = CTS_Protocol::KeyInput;
					header.Key = InputID::KeyboardX;
					header.isdown = true;
					client.send_all((char*)&header, sizeof(CTS_KeyInput_Header), 0);
				}
			}
			break;
			case 'W':
			{
				if ((game.pKeyBuffer['W'] & 0xF0) == false) {
					CTS_KeyInput_Header header;
					header.size = sizeof(CTS_KeyInput_Header);
					header.st = CTS_Protocol::KeyInput;
					header.Key = 'W';
					header.isdown = true;
					client.send_all((char*)&header, sizeof(CTS_KeyInput_Header), 0);
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
					client.send_all((char*)&header, sizeof(CTS_KeyInput_Header), 0);
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
					client.send_all((char*)&header, sizeof(CTS_KeyInput_Header), 0);
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
					client.send_all((char*)&header, sizeof(CTS_KeyInput_Header), 0);
				}
			}
			break;
			case 'Q':
			{
				if ((game.pKeyBuffer['Q'] & 0xF0) == false) {
					CTS_UseSkill_Header header;
					header.size = sizeof(CTS_UseSkill_Header);
					header.st = CTS_Protocol::UseSkill;
					header.slot = SkillSlot::Ultimate;
					client.send_all((char*)&header, sizeof(CTS_UseSkill_Header), 0);
				}
			}
			break;
			case 'F':
			{
				// [party/dungeon] join the party queue (server adds caller + broadcasts members' HP/job).
				if ((game.pKeyBuffer['F'] & 0xF0) == false) {
					CTS_DungeonStart_Header header;
					header.size = sizeof(CTS_DungeonStart_Header);
					header.st = CTS_Protocol::DungeonStart;
					client.send_all((char*)&header, sizeof(CTS_DungeonStart_Header), 0);
				}
			}
			break;
			case 'C':
			{
				if (!(lParam & (1 << 30))) {
					game.StatWindowOpen = !game.StatWindowOpen;
				}
			}
			break;
			case 'T':
			{
				// Dungeon validation shortcut: one edge-triggered request ends the whole active run.
				if (game.currentZoneId >= 100 && game.currentZoneId < 106 && !(lParam & (1 << 30))) {
					CTS_DungeonAbort_Header header;
					client.send_all((char*)&header, sizeof(header), 0);
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
					client.send_all((char*)&header, sizeof(CTS_KeyInput_Header), 0);
				}
			}
			break;
			case VK_TAB:
			{
				if ((game.pKeyBuffer[VK_TAB] & 0xF0) == false) {
					CTS_KeyInput_Header header;
					header.size = sizeof(CTS_KeyInput_Header);
					header.st = CTS_Protocol::KeyInput;
					header.Key = VK_TAB;
					header.isdown = true;
					client.send_all((char*)&header, sizeof(CTS_KeyInput_Header), 0);
				}
			}
			break;
			case 'E':
			{
				if ((game.pKeyBuffer['E'] & 0xF0) == false) {
					CTS_UseSkill_Header header;
					header.size = sizeof(CTS_UseSkill_Header);
					header.st = CTS_Protocol::UseSkill;
					header.slot = SkillSlot::Skill1;
					client.send_all((char*)&header, sizeof(CTS_UseSkill_Header), 0);
				}
			}
			break;
			case 'Z':
			{
				if ((game.pKeyBuffer['Z'] & 0xF0) == false) {
					CTS_UseSkill_Header header;
					header.size = sizeof(CTS_UseSkill_Header);
					header.st = CTS_Protocol::UseSkill;
					header.slot = SkillSlot::Skill2;
					client.send_all((char*)&header, sizeof(CTS_UseSkill_Header), 0);
				}
			}
			break;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			{
				if (!(lParam & (1 << 30))) {
					int jobIndex = (int)(wParam - '1');
					if (jobIndex >= 0 && jobIndex < (int)PlayerJob::Max) {
						CTS_ChangeJob_Header header;
						header.size = sizeof(CTS_ChangeJob_Header);
						header.st = CTS_Protocol::ChangeJob;
						header.job = (PlayerJob)jobIndex;
						client.send_all((char*)&header, sizeof(CTS_ChangeJob_Header), 0);
					}
				}
			}
			break;			case VK_F9:
			{
				BOOL bFullScreenState = FALSE;
				gd.pSwapChain->GetFullscreenState(&bFullScreenState, NULL);
				gd.SetFullScreenMode(!bFullScreenState);
				break;
			}
			case VK_F8:
			{
				if (gd.isSupportRaytracing) {
					gd.isRaytracingRender = !gd.isRaytracingRender;
				}
				else {
					gd.isRaytracingRender = false;
				}
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
				if (game.mainPageStack.size() == 0) {
					game.mainPageStack.push_back(game.UIPageTable[0]);
				}
				else if(game.mainPageStack[0] == game.UIPageTable[0]) {
					game.mainPageStack.pop_back();
				}
				//PostQuitMessage(0);
				break;
			}
			break;
			case 'O':
			{
				
			}
			break;
			}
			GetKeyboardState(game.pKeyBuffer);
			break;
		case WM_KEYUP:
			switch (wParam) {
			case 'R':
			{
				CTS_KeyInput_Header header;
				header.size = sizeof(CTS_KeyInput_Header);
				header.st = CTS_Protocol::KeyInput;
				header.Key = InputID::KeyboardR;
				header.isdown = false;
				client.send_all((char*)&header, sizeof(CTS_KeyInput_Header), 0);
			}
			break;
			case 'X':
			{
				CTS_KeyInput_Header header;
				header.size = sizeof(CTS_KeyInput_Header);
				header.st = CTS_Protocol::KeyInput;
				header.Key = InputID::KeyboardX;
				header.isdown = false;
				client.send_all((char*)&header, sizeof(CTS_KeyInput_Header), 0);
			}
			break;
			case 'W':
			{
				CTS_KeyInput_Header header;
				header.size = sizeof(CTS_KeyInput_Header);
				header.st = CTS_Protocol::KeyInput;
				header.Key = 'W';
				header.isdown = false;
				client.send_all((char*)&header, sizeof(CTS_KeyInput_Header), 0);
			}
			break;
			case 'A':
			{
				CTS_KeyInput_Header header;
				header.size = sizeof(CTS_KeyInput_Header);
				header.st = CTS_Protocol::KeyInput;
				header.Key = 'A';
				header.isdown = false;
				client.send_all((char*)&header, sizeof(CTS_KeyInput_Header), 0);
			}
			break;
			case 'S':
			{
				CTS_KeyInput_Header header;
				header.size = sizeof(CTS_KeyInput_Header);
				header.st = CTS_Protocol::KeyInput;
				header.Key = 'S';
				header.isdown = false;
				client.send_all((char*)&header, sizeof(CTS_KeyInput_Header), 0);
			}
			break;
			case 'D':
			{
				CTS_KeyInput_Header header;
				header.size = sizeof(CTS_KeyInput_Header);
				header.st = CTS_Protocol::KeyInput;
				header.Key = 'D';
				header.isdown = false;
				client.send_all((char*)&header, sizeof(CTS_KeyInput_Header), 0);
			}
			break;
			case 'Q':
			{
				CTS_KeyInput_Header header;
				header.size = sizeof(CTS_KeyInput_Header);
				header.st = CTS_Protocol::KeyInput;
				header.Key = 'Q';
				header.isdown = false;
				client.send_all((char*)&header, sizeof(CTS_KeyInput_Header), 0);
			}
			break;
			case 'E':
			{
				CTS_KeyInput_Header header;
				header.size = sizeof(CTS_KeyInput_Header);
				header.st = CTS_Protocol::KeyInput;
				header.Key = 'E';
				header.isdown = false;
				client.send_all((char*)&header, sizeof(CTS_KeyInput_Header), 0);
			}
			break;
			case VK_SPACE:
			{
				CTS_KeyInput_Header header;
				header.size = sizeof(CTS_KeyInput_Header);
				header.st = CTS_Protocol::KeyInput;
				header.Key = VK_SPACE;
				header.isdown = false;
				client.send_all((char*)&header, sizeof(CTS_KeyInput_Header), 0);
			}
			break;
			case VK_TAB:
			{
				CTS_KeyInput_Header header;
				header.size = sizeof(CTS_KeyInput_Header);
				header.st = CTS_Protocol::KeyInput;
				header.Key = VK_TAB;
				header.isdown = false;
				client.send_all((char*)&header, sizeof(CTS_KeyInput_Header), 0);
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
			client.send_all((char*)&header, sizeof(CTS_KeyInput_Header), 0);
			// [sfx] gunshot on fire. Add Resources/Sound/gunshot.wav (PCM, stereo, 16-bit, 44100Hz).
			// If the file is missing, CreateWaveFromFile returns empty and pushWave is a no-op (safe).
			static WaveDataStruct s_gunshot = CreateWaveFromFile(L"Resources/Sound/gun.wav");
			static WaveChannel* s_sfxChannel = NewChannel();
			static WaveDataStruct s_dryReload = CreateWaveFromFile(L"Resources/Sound/reload.wav");
			if (false) {   // [sfx] disabled: gunshot now plays on the real-fire event STC_PlayerFire (Game.cpp, g_playGunSound), not on every click
				if (game.player != nullptr && game.player->weapon[game.player->SelectedWeapon].m_shootFlow >= 0)
					s_sfxChannel->pushWave(s_gunshot);    // 발사 가능(장전 중 아님) -> 총소리
				else
					s_sfxChannel->pushWave(s_dryReload);  // 장전 중(탄약 없음) -> 장전소리
			}
		}
		break;
		case WM_LBUTTONUP:
		{
			CTS_KeyInput_Header header;
			header.size = sizeof(CTS_KeyInput_Header);
			header.st = CTS_Protocol::KeyInput;
			header.Key = InputID::MouseLbutton;
			header.isdown = false;
			client.send_all((char*)&header, sizeof(CTS_KeyInput_Header), 0);
		}
		break;
		case WM_RBUTTONDOWN:
		{
			CTS_KeyInput_Header header;
			header.size = sizeof(CTS_KeyInput_Header);
			header.st = CTS_Protocol::KeyInput;
			header.Key = InputID::MouseRbutton;
			header.isdown = true;
			client.send_all((char*)&header, sizeof(CTS_KeyInput_Header), 0);
		}
		break;
		case WM_RBUTTONUP:
		{
			CTS_KeyInput_Header header;
			header.size = sizeof(CTS_KeyInput_Header);
			header.st = CTS_Protocol::KeyInput;
			header.Key = InputID::MouseRbutton;
			header.isdown = false;
			client.send_all((char*)&header, sizeof(CTS_KeyInput_Header), 0);
		}
		break;
		}
	}
	else {
		// UI Page Event
		switch (uMsg) {
		case WM_KEYDOWN:
		{
			switch (wParam) {
			case 'C':
			{
				if (game.StatWindowOpen && !(lParam & (1 << 30))) {
					game.StatWindowOpen = false;
				}
			}
			break;
			case VK_ESCAPE:
			{
				if (game.StatWindowOpen) {
					game.StatWindowOpen = false;
				}
				else if (game.mainPageStack.size() >= 1 && game.mainPageStack[0] == game.UIPageTable[0]) {
					game.mainPageStack.pop_back();
				}
			}
			break;
			}
			GetKeyboardState(game.pKeyBuffer);
			break;
		}
		case WM_KEYUP:
			GetKeyboardState(game.pKeyBuffer);
			break;
		case WM_LBUTTONDOWN:
			if (game.StatWindowOpen) {
				game.CurrentCursorPos.x = (float)LOWORD(lParam) - 0.5f * (float)gd.ClientFrameWidth;
				game.CurrentCursorPos.y = -1.0f * ((float)HIWORD(lParam) - 0.5f * (float)gd.ClientFrameHeight);
				game.HandleStatWindowClick(game.CurrentCursorPos);
			}
			break;
		}

		if (game.mainPageStack.size() >= 1) {
			game.CurrentPageStack = &game.mainPageStack;
			game.mainPageStack[game.mainPageStack.size() - 1]->Event();
		}
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
	//	n = (char*)&temp - (char*)&temp.SkillCooldown;
	//	dbglog1(L"class Player.Heal%d\n", n);
	//	n = (char*)&temp - (char*)&temp.SkillCooldownFlow;
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
	//}
}

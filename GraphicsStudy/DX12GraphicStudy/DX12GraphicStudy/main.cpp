﻿#include "main.h"

#pragma region GlobalStaticInits
//StaticInit_TempImageMemory
#pragma endregion

Mesh* BulletRay::mesh = nullptr;

TestStruct testData;

void TestStruct::SetTestValue()
{
	UseBundle = false;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdParam, int nCmdShow) {
	testData.SetTestValue();

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

	game.Event(WinEvent(hWnd, uMsg, wParam, lParam));

	switch (uMsg) {
	//outer code start : microsoft copilot. - FPS Camera Movement
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

				game.DeltaMousePos.x += deltaX;
				game.DeltaMousePos.y += deltaY;

				if (game.DeltaMousePos.y > 100) {
					game.DeltaMousePos.y = 100;
				}
				if (game.DeltaMousePos.y < -100) {
					game.DeltaMousePos.y = -100;
				}
			}
		}
		break;
		
	}
	case WM_CREATE:
		GetClientRect(hWnd, &rt);
		break;
	case WM_KEYDOWN:
		GetKeyboardState(game.pKeyBuffer);
		switch (wParam) {
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
			DestroyWindow(hWnd);
			break;
		}
			break;
		}
		break;
	case WM_KEYUP:
		GetKeyboardState(game.pKeyBuffer);
		break;
	case WM_MOUSEMOVE:
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

		break;
	case WM_DESTROY:
		game.Release();
		gd.Release();
		PostQuitMessage(0);
		break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void GlobalDevice::InitBundleAllocator()
{
	HRESULT hr = pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE, IID_PPV_ARGS(&pBundleAllocator));
	//dbglog1(L"[ERROR] Create Bundle Allocator Error : hr = %d\n", hr);
	if (hr != S_OK) {
		dbglog1(L"[ERROR] Create Bundle Allocator Error : hr = %d\n", hr);
	}
}

int GlobalDevice::CreateNewBundle()
{
	int n = pBundles.size();
	ID3D12GraphicsCommandList* pBundle;
	pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, pBundleAllocator, NULL, IID_PPV_ARGS(&pBundle));
	pBundles.push_back(pBundle);
	return n;
}

void GlobalDevice::Init()
{
	font_filename[0] = "consola.ttf"; // english
	font_filename[1] = "malgunbd.ttf"; // korean
	for (int i = 0; i < FontCount; ++i) {
		uint8_t condition_variable = 0;
		int8_t error = TTFFontParser::parse_file(font_filename[i].c_str(), &font_data[i], &font_parsed, &condition_variable);
	}
	addTextureStack.reserve(32);
	
	HRESULT hResult;
	UINT nDXGIFactoryFlags = 0;
#if defined(_DEBUG)
	ID3D12Debug* pd3dDebugController = NULL;
	hResult = D3D12GetDebugInterface(__uuidof(ID3D12Debug), (void
		**)&pd3dDebugController);
	if (pd3dDebugController)
	{
		pd3dDebugController->EnableDebugLayer();
		pd3dDebugController->Release();
	}
	nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
	hResult = ::CreateDXGIFactory2(nDXGIFactoryFlags, __uuidof(IDXGIFactory4), (void
		**)&pFactory); // sus..
	if (FAILED(hResult)) {
		OutputDebugStringA("[ERROR] : Create Factory Error.\n");
		return;
	}

	constexpr D3D_FEATURE_LEVEL FeatureLevelPriority[11] = {
		D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1, D3D_FEATURE_LEVEL_1_0_CORE
	};

	IDXGIAdapter1* pd3dAdapter1 = NULL;
	for (int i = 0; i < 11; ++i) {
		// Find GPU that support DirectX 12_2
		for (UINT i = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(i, &pd3dAdapter1); i++)
		{
			IDXGIAdapter4* pd3dAdapter4 = NULL;
			DXGI_ADAPTER_DESC1 dxgiAdapterDesc;
			pd3dAdapter1->GetDesc1(&dxgiAdapterDesc);
			pd3dAdapter1->QueryInterface(__uuidof(IDXGIAdapter4), (void**)&pd3dAdapter4);
			if (dxgiAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
			if (SUCCEEDED(D3D12CreateDevice(pd3dAdapter4, FeatureLevelPriority[i],
				_uuidof(ID3D12Device), (void**)&pDevice))) {
				pd3dAdapter4->Release();
				pd3dAdapter1->Release();
				goto GlobalDeviceInit_InitMultisamplingVariable;
				break;
			}
			pd3dAdapter4->Release();
			pd3dAdapter1->Release();
		}
	}
	
	if (!pd3dAdapter1)
	{
		OutputDebugStringA("[ERROR] : there is no GPU that support DirectX.\n");
		return;
	}

GlobalDeviceInit_InitMultisamplingVariable:

	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS d3dMsaaQualityLevels;
	d3dMsaaQualityLevels.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	d3dMsaaQualityLevels.SampleCount = 4; //Msaa4x multi sampling.
	d3dMsaaQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	d3dMsaaQualityLevels.NumQualityLevels = 0;
	pDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&d3dMsaaQualityLevels, sizeof(D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS));
	m_nMsaa4xQualityLevels = d3dMsaaQualityLevels.NumQualityLevels;
	m_bMsaa4xEnable = (m_nMsaa4xQualityLevels > 1) ? true : false;
	//when multi sampling quality level is bigger than 1, active MSAA.

	hResult = pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
		(void**)&pFence);
	FenceValue = 0;
	hFenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	// Create Event Object to Sync with Fence. Event's Init value is FALSE, when Signal call, Event Value change to FALSE

	// Command Queue & Command Allocator & Command List (TYPE : DIRECT)
	D3D12_COMMAND_QUEUE_DESC d3dCommandQueueDesc;
	::ZeroMemory(&d3dCommandQueueDesc, sizeof(D3D12_COMMAND_QUEUE_DESC));
	d3dCommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	d3dCommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	hResult = pDevice->CreateCommandQueue(&d3dCommandQueueDesc,
		_uuidof(ID3D12CommandQueue), (void**)&pCommandQueue);
	hResult = pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
		__uuidof(ID3D12CommandAllocator), (void**)&pCommandAllocator);
	hResult = pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		pCommandAllocator, NULL, __uuidof(ID3D12GraphicsCommandList), (void
			**)&pCommandList);

	InitBundleAllocator();


	hResult = pCommandList->Close(); // why?

	ResolutionStruct* ResolutionArr = gd.GetResolutionArr();
	ClientFrameWidth = ResolutionArr[resolutionLevel].width;
	ClientFrameHeight = ResolutionArr[resolutionLevel].height;

	viewportArr[0].Viewport.TopLeftX = 0;
	viewportArr[0].Viewport.TopLeftY = 0;
	viewportArr[0].Viewport.Width = (float)ClientFrameWidth;
	viewportArr[0].Viewport.Height = (float)ClientFrameHeight;
	viewportArr[0].Viewport.MinDepth = 0.0f;
	viewportArr[0].Viewport.MaxDepth = 1.0f;
	viewportArr[0].ScissorRect = { 0, 0, (long)ClientFrameWidth, (long)ClientFrameHeight };

	MainRenderTarget_PixelFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	NewSwapChain();
	Create_RTV_DSV_DescriptorHeaps();
	CreateRenderTargetViews();
	CreateDepthStencilView();
	if (RenderMod == DeferedRendering) {
		//set gbuffer format ex>
		//GbufferPixelFormatArr[0] = DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM;

		CreateGbufferRenderTargetViews();
	}

	TextureDescriptorAllotter.Init(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 64);
	//why cannot be shader visible in Saver DescriptorHeap?
	//answer : CreateConstantBufferView, CreateShaderResourceView -> only CPU Descriptor Working. so Shader Invisible.

	ShaderVisibleDescPool.Initialize(128);
}

void GlobalDevice::Release()
{
	WaitGPUComplete();

	TextureDescriptorAllotter.Release();
	ShaderVisibleDescPool.Release();

	if (RenderMod == DeferedRendering) {
		if (GbufferDescriptorHeap) GbufferDescriptorHeap->Release();
		for (int i = 0; i < GbufferCount; ++i) {
			if(ppGBuffers[i]) ppGBuffers[i]->Release();
		}
	}

	for (int i = 0; i < SwapChainBufferCount; ++i) {
		if(ppRenderTargetBuffers[i]) ppRenderTargetBuffers[i]->Release();
	}

	if (pRtvDescriptorHeap) pRtvDescriptorHeap->Release();
	if (pDepthStencilBuffer) pDepthStencilBuffer->Release();
	if (pDsvDescriptorHeap) pDsvDescriptorHeap->Release();

	if (pCommandList) pCommandList->Release();
	if (pCommandAllocator) pCommandAllocator->Release();
	if (pCommandQueue) pCommandQueue->Release();
	
	CloseHandle(hFenceEvent);
	if (pFence) pFence->Release();

	pSwapChain->SetFullscreenState(FALSE, NULL);
	if (pSwapChain) pSwapChain->Release();

	if (pDevice) { 
		pDevice->Release(); 
	}
	if (pFactory) pFactory->Release();

	ReportLiveObjects();
}

void GlobalDevice::WaitGPUComplete()
{
	FenceValue++;
	const UINT64 nFence = FenceValue;
	HRESULT hResult = pCommandQueue->Signal(pFence, nFence);
	//Add Signal Command (it update gpu fence value.)
	if (pFence->GetCompletedValue() < nFence)
	{
		//When GPU Fence is smaller than CPU FenceValue, Wait.
		hResult = pFence->SetEventOnCompletion(nFence, hFenceEvent);
		::WaitForSingleObject(hFenceEvent, INFINITE);
	}
}

void GlobalDevice::NewSwapChain()
{
	if (pSwapChain) pSwapChain->Release();

	DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc;
	::ZeroMemory(&dxgiSwapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC1));
	dxgiSwapChainDesc.Width = ClientFrameWidth;
	dxgiSwapChainDesc.Height = ClientFrameHeight;
	dxgiSwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dxgiSwapChainDesc.SampleDesc.Count = (m_bMsaa4xEnable) ? 4 : 1;
	dxgiSwapChainDesc.SampleDesc.Quality = (m_bMsaa4xEnable) ? (m_nMsaa4xQualityLevels -
		1) : 0;
	dxgiSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	dxgiSwapChainDesc.BufferCount = SwapChainBufferCount;
	dxgiSwapChainDesc.Scaling = DXGI_SCALING_NONE;
	dxgiSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	dxgiSwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

	//must contain for fullscreen mode!!!!!
	dxgiSwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; // enable resize os resolusion..
	
	DXGI_SWAP_CHAIN_FULLSCREEN_DESC dxgiSwapChainFullScreenDesc;
	::ZeroMemory(&dxgiSwapChainFullScreenDesc, sizeof(DXGI_SWAP_CHAIN_FULLSCREEN_DESC));
	dxgiSwapChainFullScreenDesc.RefreshRate.Numerator = 60;
	dxgiSwapChainFullScreenDesc.RefreshRate.Denominator = 1;
	dxgiSwapChainFullScreenDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	dxgiSwapChainFullScreenDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	dxgiSwapChainFullScreenDesc.Windowed = TRUE;

	//Create SwapChain
	pFactory->CreateSwapChainForHwnd(pCommandQueue, hWnd,
		&dxgiSwapChainDesc, &dxgiSwapChainFullScreenDesc, NULL, (IDXGISwapChain1
			**)&pSwapChain); // is version 4 can executing without Query? sus..

	// Disable (Alt + Enter to Fullscreen Mode.)
	pFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);

	CurrentSwapChainBufferIndex = pSwapChain->GetCurrentBackBufferIndex();

	if (RenderMod == DeferedRendering) {
		NewGbuffer();
	}
}

void GlobalDevice::NewGbuffer()
{
	D3D12_HEAP_PROPERTIES heapProperty;
	heapProperty.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperty.Type = D3D12_HEAP_TYPE_DEFAULT; // type : DEFAULT HEAP
	heapProperty.VisibleNodeMask = 0xFFFFFFFF;
	heapProperty.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperty.CreationNodeMask = 0;

	D3D12_HEAP_FLAGS heapFlag;
	heapFlag = D3D12_HEAP_FLAG_NONE;

	D3D12_RESOURCE_DESC desc;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.MipLevels = 0;
	desc.Width = ClientFrameWidth;
	desc.Height = ClientFrameHeight;
	desc.Alignment = 0; // 64KB Alignment.
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	desc.DepthOrArraySize = 1; // ?
	desc.SampleDesc.Count = 0;
	desc.SampleDesc.Quality = 0; // no MSAA

	D3D12_RESOURCE_STATES initState;
	initState = D3D12_RESOURCE_STATE_RENDER_TARGET;

	D3D12_CLEAR_VALUE clearValue;
	clearValue.Color[0] = 0;
	clearValue.Color[1] = 0;
	clearValue.Color[2] = 0;
	clearValue.Color[3] = 0;

	for (int i = 0; i < GbufferCount; ++i) {
		desc.Format = GbufferPixelFormatArr[i];
		clearValue.Format = GbufferPixelFormatArr[i];

		pDevice->CreateCommittedResource(&heapProperty, heapFlag, &desc, initState, &clearValue, __uuidof(ID3D12Resource), (void**)&ppGBuffers[i]);
	}
}

void GlobalDevice::Create_RTV_DSV_DescriptorHeaps()
{
	//RTV heap
	D3D12_DESCRIPTOR_HEAP_DESC d3dDescriptorHeapDesc;
	::ZeroMemory(&d3dDescriptorHeapDesc, sizeof(D3D12_DESCRIPTOR_HEAP_DESC));
	d3dDescriptorHeapDesc.NumDescriptors = SwapChainBufferCount;
	d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	d3dDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	d3dDescriptorHeapDesc.NodeMask = 0;
	HRESULT hResult = pDevice->CreateDescriptorHeap(&d3dDescriptorHeapDesc,
		__uuidof(ID3D12DescriptorHeap), (void**)&pRtvDescriptorHeap);
	RtvDescriptorIncrementSize =
		pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	
	//DSV heap
	d3dDescriptorHeapDesc.NumDescriptors = 1;
	d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	hResult = pDevice->CreateDescriptorHeap(&d3dDescriptorHeapDesc,
		__uuidof(ID3D12DescriptorHeap), (void**)&pDsvDescriptorHeap);
	DsvDescriptorIncrementSize =
		pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	if (RenderMod == DeferedRendering) {
		D3D12_DESCRIPTOR_HEAP_DESC d3dDescriptorHeapDesc;
		::ZeroMemory(&d3dDescriptorHeapDesc, sizeof(D3D12_DESCRIPTOR_HEAP_DESC));
		d3dDescriptorHeapDesc.NumDescriptors = GbufferCount;
		d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		d3dDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		d3dDescriptorHeapDesc.NodeMask = 0;
		HRESULT hResult = pDevice->CreateDescriptorHeap(&d3dDescriptorHeapDesc,
			__uuidof(ID3D12DescriptorHeap), (void**)&GbufferDescriptorHeap);
	}
}

void GlobalDevice::CreateRenderTargetViews()
{
	D3D12_RENDER_TARGET_VIEW_DESC MainRenderTargetViewDesc;
	MainRenderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	MainRenderTargetViewDesc.Format = MainRenderTarget_PixelFormat;
	MainRenderTargetViewDesc.Texture2D.MipSlice = 0;
	MainRenderTargetViewDesc.Texture2D.PlaneSlice = 0;

	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle =
		pRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	for (UINT i = 0; i < SwapChainBufferCount; i++)
	{
		pSwapChain->GetBuffer(i, __uuidof(ID3D12Resource), (void
			**)&ppRenderTargetBuffers[i]);
		pDevice->CreateRenderTargetView(ppRenderTargetBuffers[i], &MainRenderTargetViewDesc,
			d3dRtvCPUDescriptorHandle);
		d3dRtvCPUDescriptorHandle.ptr += RtvDescriptorIncrementSize;
	}
}

void GlobalDevice::CreateGbufferRenderTargetViews()
{
	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle =
		GbufferDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	if (RenderMod == DeferedRendering) {
		for (int i = 0; i < GbufferCount; ++i) {
			D3D12_RENDER_TARGET_VIEW_DESC GbufferRenderTargetViewDesc;
			GbufferRenderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			GbufferRenderTargetViewDesc.Format = GbufferPixelFormatArr[i];
			GbufferRenderTargetViewDesc.Texture2D.MipSlice = 0;
			GbufferRenderTargetViewDesc.Texture2D.PlaneSlice = 0;

			pDevice->CreateRenderTargetView(ppGBuffers[i], &GbufferRenderTargetViewDesc, d3dRtvCPUDescriptorHandle);
		}
	}
}

void GlobalDevice::CreateDepthStencilView()
{
	D3D12_RESOURCE_DESC d3dResourceDesc;
	d3dResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	d3dResourceDesc.Alignment = 0;
	d3dResourceDesc.Width = ClientFrameWidth;
	d3dResourceDesc.Height = ClientFrameHeight;
	d3dResourceDesc.DepthOrArraySize = 1;
	d3dResourceDesc.MipLevels = 1;
	d3dResourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	d3dResourceDesc.SampleDesc.Count = (m_bMsaa4xEnable) ? 4 : 1;
	d3dResourceDesc.SampleDesc.Quality = (m_bMsaa4xEnable) ? (m_nMsaa4xQualityLevels - 1)
		: 0;
	d3dResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	d3dResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	D3D12_HEAP_PROPERTIES d3dHeapProperties;
	::ZeroMemory(&d3dHeapProperties, sizeof(D3D12_HEAP_PROPERTIES));
	d3dHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	d3dHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	d3dHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	d3dHeapProperties.CreationNodeMask = 1;
	d3dHeapProperties.VisibleNodeMask = 1;
	D3D12_CLEAR_VALUE d3dClearValue;
	d3dClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	d3dClearValue.DepthStencil.Depth = 1.0f;
	d3dClearValue.DepthStencil.Stencil = 0;
	pDevice->CreateCommittedResource(&d3dHeapProperties, D3D12_HEAP_FLAG_NONE,
		&d3dResourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &d3dClearValue,
		__uuidof(ID3D12Resource), (void**)&pDepthStencilBuffer);

	D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle =
		pDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	pDevice->CreateDepthStencilView(pDepthStencilBuffer, NULL,
		d3dDsvCPUDescriptorHandle);
}

//work in present device but.. if resolution change, it can be strange..
// why??
void GlobalDevice::SetFullScreenMode(bool isFullScreen)
{
	WaitGPUComplete();
	BOOL bFullScreenState = FALSE;
	pSwapChain->GetFullscreenState(&bFullScreenState, NULL);
	if (isFullScreen != bFullScreenState) {
		//Change FullscreenState
		pSwapChain->SetFullscreenState(isFullScreen, NULL);

		DXGI_MODE_DESC dxgiTargetParameters;
		dxgiTargetParameters.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		dxgiTargetParameters.Width = ClientFrameWidth;
		dxgiTargetParameters.Height = ClientFrameHeight;
		dxgiTargetParameters.RefreshRate.Numerator = 60;
		dxgiTargetParameters.RefreshRate.Denominator = 1;
		dxgiTargetParameters.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		dxgiTargetParameters.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		pSwapChain->ResizeTarget(&dxgiTargetParameters);

		for (int i = 0; i < SwapChainBufferCount; i++) if (ppRenderTargetBuffers[i]) ppRenderTargetBuffers[i]->Release();

		DXGI_SWAP_CHAIN_DESC dxgiSwapChainDesc;
		pSwapChain->GetDesc(&dxgiSwapChainDesc);
		pSwapChain->ResizeBuffers(SwapChainBufferCount, ClientFrameWidth,
			ClientFrameHeight, dxgiSwapChainDesc.BufferDesc.Format, dxgiSwapChainDesc.Flags);
		CurrentSwapChainBufferIndex = pSwapChain->GetCurrentBackBufferIndex();
		
		CreateRenderTargetViews();
	}
}

ResolutionStruct* GlobalDevice::GetResolutionArr()
{
	if (fabsf(ScreenRatio - 0.75f) <= 0.01f) {
		return (ResolutionStruct*)ResolutionArr_GA;
	}
	else if (fabsf(ScreenRatio - 0.5625f) <= 0.01f) {
		return (ResolutionStruct*)ResolutionArr_HD;
	}
	else if (fabsf(ScreenRatio - 0.625f) <= 0.01f) {
		return (ResolutionStruct*)ResolutionArr_WGA;
	}
	return nullptr;
}

void GlobalDevice::SetResolution(int resid, bool ClientSizeUpdate)
{
	WaitGPUComplete();
	ResolutionStruct* ResolutionArr = GetResolutionArr();

	ClientFrameWidth = ResolutionArr[resid].width;
	ClientFrameHeight = ResolutionArr[resid].height;
	if (ClientSizeUpdate) {
		SetWindowPos(hWnd, NULL, 0, 0, ClientFrameWidth, ClientFrameHeight, SWP_NOMOVE | SWP_NOZORDER);
	}

	if (RenderMod == DeferedRendering) {
		for (int i = 0; i < GbufferCount; ++i) {
			ppGBuffers[i]->Release();
		}

		NewGbuffer();
		CreateGbufferRenderTargetViews();
	}

	pDepthStencilBuffer->Release();
	CreateDepthStencilView();
}

//outer code : microsoft copilot.
int GlobalDevice::PixelFormatToPixelSize(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R32G32B32A32_FLOAT: return 16;
	case DXGI_FORMAT_R32G32B32A32_UINT:  return 16;
	case DXGI_FORMAT_R32G32B32A32_SINT:  return 16;

	case DXGI_FORMAT_R32G32B32_FLOAT:    return 12;
	case DXGI_FORMAT_R32G32B32_UINT:     return 12;
	case DXGI_FORMAT_R32G32B32_SINT:     return 12;

	case DXGI_FORMAT_R16G16B16A16_FLOAT: return 8;
	case DXGI_FORMAT_R16G16B16A16_UNORM: return 8;
	case DXGI_FORMAT_R16G16B16A16_UINT:  return 8;
	case DXGI_FORMAT_R8G8B8A8_UNORM:     return 4;
	case DXGI_FORMAT_R8G8B8A8_UINT:      return 4;
	case DXGI_FORMAT_B8G8R8A8_UNORM:     return 4;
	case DXGI_FORMAT_R32_FLOAT:          return 4;
	case DXGI_FORMAT_R32_UINT:           return 4;
	case DXGI_FORMAT_R16_FLOAT:          return 2;
	case DXGI_FORMAT_R8_UNORM:           return 1;

	case DXGI_FORMAT_D32_FLOAT:          return 4;
	case DXGI_FORMAT_D24_UNORM_S8_UINT:  return 4;

	case DXGI_FORMAT_R11G11B10_FLOAT:    return 4;
	case DXGI_FORMAT_R10G10B10A2_UNORM:  return 4;

	case DXGI_FORMAT_UNKNOWN:			 return 1;
		// 압축 포맷은 픽셀당 크기가 고정되지 않음
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC3_UNORM:
	case DXGI_FORMAT_BC4_UNORM:
	case DXGI_FORMAT_BC5_UNORM:
	case DXGI_FORMAT_BC6H_UF16:
	case DXGI_FORMAT_BC7_UNORM:
		return -1; // 블록 기반 포맷: 직접 계산 필요
	}
}

GPUResource GlobalDevice::CreateCommitedGPUBuffer(ID3D12GraphicsCommandList* commandList, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES d3dResourceStates, D3D12_RESOURCE_DIMENSION dimension, int Width, int Height, DXGI_FORMAT BufferFormat)
{
	ID3D12Resource* pBuffer = NULL;
	ID3D12Resource2* pBuffer2 = NULL;

	D3D12_HEAP_PROPERTIES d3dHeapPropertiesDesc;
	::ZeroMemory(&d3dHeapPropertiesDesc, sizeof(D3D12_HEAP_PROPERTIES));
	d3dHeapPropertiesDesc.Type = heapType; // default / upload / readback ...
	d3dHeapPropertiesDesc.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	d3dHeapPropertiesDesc.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	d3dHeapPropertiesDesc.CreationNodeMask = 1;
	d3dHeapPropertiesDesc.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC d3dResourceDesc;
	::ZeroMemory(&d3dResourceDesc, sizeof(D3D12_RESOURCE_DESC));
	d3dResourceDesc.Dimension = dimension;
	d3dResourceDesc.Alignment = 0;
	d3dResourceDesc.Width = Width;
	d3dResourceDesc.Height = Height;
	d3dResourceDesc.DepthOrArraySize = 1;
	d3dResourceDesc.MipLevels = 1;
	d3dResourceDesc.Format = BufferFormat;
	d3dResourceDesc.SampleDesc.Count = 1;
	d3dResourceDesc.SampleDesc.Quality = 0;
	if (d3dResourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
		d3dResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	}
	else {
		d3dResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	}
	d3dResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;
	if (d3dResourceDesc.Layout == D3D12_TEXTURE_LAYOUT_ROW_MAJOR && (d3dResourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)) {
		heapFlags |= D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER;	
	}

	if (heapFlags & D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER) {
		heapFlags |= D3D12_HEAP_FLAG_SHARED;
		d3dResourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
	}
	
	//D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER

	D3D12_RESOURCE_STATES d3dResourceInitialStates = D3D12_RESOURCE_STATE_COPY_DEST;
	if (heapType == D3D12_HEAP_TYPE_UPLOAD) 
		d3dResourceInitialStates = D3D12_RESOURCE_STATE_GENERIC_READ;
	else if (heapType == D3D12_HEAP_TYPE_READBACK)
		d3dResourceInitialStates = D3D12_RESOURCE_STATE_COPY_DEST;

	HRESULT hResult = gd.pDevice->CreateCommittedResource(&d3dHeapPropertiesDesc, heapFlags, &d3dResourceDesc, d3dResourceInitialStates, NULL, __uuidof(ID3D12Resource), (void**)&pBuffer);
	pBuffer->QueryInterface<ID3D12Resource2>(&pBuffer2);
	pBuffer->Release();

	if (FAILED(hResult)) {
		GPUResource gr;
		gr.resource = nullptr;
		gr.extraData = 0;
		return gr;
	}
	else {
		GPUResource gr;
		gr.resource = pBuffer2;
		gr.CurrentState_InCommandWriteLine = d3dResourceInitialStates;
		gr.extraData = 0;
		return gr;
	}
}

void GlobalDevice::UploadToCommitedGPUBuffer(ID3D12GraphicsCommandList* commandList, void* ptr, GPUResource* uploadBuffer, GPUResource* copydestBuffer, bool StateReturning)
{
	D3D12_RANGE d3dReadRange = { 0, 0 };

	D3D12_RESOURCE_DESC1 desc = uploadBuffer->resource->GetDesc1();
	
	int SizePerUnit = PixelFormatToPixelSize(desc.Format);
	ui32 BufferByteSize = desc.Width * desc.Height * SizePerUnit;
	UINT8* pBufferDataBegin = NULL;
	
	uploadBuffer->resource->Map(0, &d3dReadRange, (void**)&pBufferDataBegin);
	memcpy(pBufferDataBegin, ptr, BufferByteSize);
	uploadBuffer->resource->Unmap(0, NULL); // quest : i want know gpumem load excution shape

	if (copydestBuffer) {
		D3D12_RESOURCE_STATES uploadBufferOriginState = uploadBuffer->CurrentState_InCommandWriteLine;
		D3D12_RESOURCE_STATES copydestOriginState = copydestBuffer->CurrentState_InCommandWriteLine;

		uploadBuffer->AddResourceBarrierTransitoinToCommand(pCommandList, D3D12_RESOURCE_STATE_COPY_SOURCE);
		copydestBuffer->AddResourceBarrierTransitoinToCommand(pCommandList, D3D12_RESOURCE_STATE_COPY_DEST);

		commandList->CopyResource(copydestBuffer->resource, uploadBuffer->resource);

		if (StateReturning) {
			uploadBuffer->AddResourceBarrierTransitoinToCommand(pCommandList, uploadBufferOriginState);
			copydestBuffer->AddResourceBarrierTransitoinToCommand(pCommandList, copydestOriginState);
		}
	}
}

inline UINT64 GlobalDevice::GetRequiredIntermediateSize(ID3D12Resource* pDestinationResource, UINT FirstSubresource, UINT NumSubresources) noexcept
{
#if defined(_MSC_VER) || !defined(_WIN32)
	const auto Desc = pDestinationResource->GetDesc();
#else
	D3D12_RESOURCE_DESC tmpDesc;
	const auto& Desc = *pDestinationResource->GetDesc(&tmpDesc);
#endif
	UINT64 RequiredSize = 0;
	pDevice->GetCopyableFootprints(&Desc, FirstSubresource, NumSubresources, 0, nullptr, nullptr, nullptr, &RequiredSize);
	return RequiredSize;
}

void GlobalDevice::ReportLiveObjects()
{
#if defined(_DEBUG)
	IDXGIDebug1* pdxgiDebug = NULL;
	DXGIGetDebugInterface1(0, __uuidof(IDXGIDebug1), (void**)&pdxgiDebug);
	HRESULT hResult = pdxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL,
		DXGI_DEBUG_RLO_DETAIL);
	pdxgiDebug->Release();
#endif
}

void GlobalDevice::AddTextTexture(wchar_t key)
{
	constexpr int textureMipLevel = 16;
	constexpr int textureMipLevel_pow2 = (1 << textureMipLevel) - 1;

	constexpr int bezier_divide = 8;
	constexpr float bezier_delta = 1.0f / bezier_divide;
	int i = 0;
	for (; i < gd.FontCount; ++i) {
		if (font_data[i].glyphs.contains(key) == false) {
			continue;
		}

		break;
	}
	if (i < gd.FontCount) {
		if (font_texture_map[i].contains(key)) return;

		Glyph g = font_data[i].glyphs[key];
		int width = g.bounding_box[2] - g.bounding_box[0] + 1;
		int height = g.bounding_box[3] - g.bounding_box[1] + 1;
		float xBase = g.bounding_box[0];
		float yBase = g.bounding_box[1];
		BYTE* textureData = new BYTE[4 * width * height];
		memset(textureData, 0, 4 * width * height);

		imgform::PixelImageObject pio(width, height, textureData);

		vector<vector<XMFLOAT3>> polygons;
		//reserved??
		polygons.reserve(g.path_list.size());
		
		//outline
		for (int k = 0; k < g.path_list.size(); ++k) {
			polygons.push_back(vector<XMFLOAT3>());

			Path p = g.path_list[k];
			polygons.reserve(p.geometry.size());
			polygons[k].push_back({ p.geometry[0].p0.x, p.geometry[0].p0.y, 0.0f });
			for (int u = 0; u < p.geometry.size(); ++u) {
				Curve c = p.geometry[u];
				float_v2 startpos = c.p0;
				float_v2 endpos = c.p1;
				if (c.is_curve) {
					float_v2 controlpos = endpos;
					endpos = c.c;
					
					float t = 0;
					vec4 s = vec4(startpos.x, startpos.y, controlpos.x, controlpos.y);
					vec4 e = vec4(controlpos.x, controlpos.y, endpos.x, endpos.y);
					float_v2 prevpos = startpos;
					prevpos.x -= xBase;
					prevpos.y -= yBase;
					t += bezier_delta;
					for (; t < 1.0f + bezier_delta; t += bezier_delta) {
						vec4 r0 = s * (1.0f - t) + t * e;
						vec4 r1 = vec4(r0.z, r0.w, 0, 0);
						r1 = r0 * (1.0f - t) + t * r1;
						float_v2 newpos = { r1.x, r1.y };

						newpos.x -= xBase;
						newpos.y -= yBase;
						polygons[k].push_back({ newpos.x, newpos.y, 0.0f });
						AddLineInTexture(prevpos, newpos, textureData, width, height);
						prevpos = newpos;
					}
				}
				else {
					startpos.x -= xBase;
					startpos.y -= yBase;
					endpos.x -= xBase;
					endpos.y -= yBase;

					polygons[k].push_back({ endpos.x, endpos.y, 0.0f });
					AddLineInTexture(startpos, endpos, textureData, width, height);
				}
			}
		}
		//pio.rawDataToBMP("ImageDbg.bmp");

		//fill in
		//for (int yi = 0; yi < height; ++yi) {
		//	bool isfill = false;
		//	for (int xi = 0; xi < width; ++xi) {
		//		unsigned int* ptr = (unsigned int*)&textureData[yi * 4 * width + xi * 4];
		//		if (*ptr == 0xFFFFFFFF) {
		//			// seek : next position is inner position??
		//			vec4 vp = vec4(xi + 20, yi, 0, 0);
		//			bool b = false;
		//			for (int pi = 0; pi < polygons.size(); ++pi) {
		//				b = !b != !bPointInPolygonRange(vp, polygons[pi]); // xor
		//			}
		//			isfill = b;
		//		}
		//		if (isfill) {
		//			*ptr = 0xFFFFFFFF;
		//		}
		//	}
		//}

		for (int yi = 1; yi < height; ++yi) {
			/*if (key == L'4' && yi == 442) {
				cout << "error!" << endl;
			}*/

			bool isfill = false;
			
			bool returning = 0;
			int retcnt = 0;
		ROW_RETURN:

			int lastendxi = 0;
			int lastturncnt = 0;
			/*int last_fillstart_xi = 0;
			int llfxi = -1;*/
			
			for (int xi = 0; xi < width; ++xi) {
				unsigned int* ptr = (unsigned int*)&textureData[yi * 4 * width + xi * 4];
				int pxi = xi;
				if (*ptr == 0xFFFFFFFF) {
					bool ret = false;
					/*if (xi == width - 1) {
						ret = true;
						if (llfxi != last_fillstart_xi) {
							xi = last_fillstart_xi;
							llfxi = last_fillstart_xi;
						}
						continue;
					}*/
					
					unsigned int* beginpaint = ptr;
					while (*beginpaint == 0xFFFFFFFF && xi < width) {
						xi += 1;
						beginpaint += 1;
					}

					unsigned int* endpaint = beginpaint;
					while (*endpaint != 0xFFFFFFFF && xi < width) {
						xi += 1;
						endpaint += 1;
					}

					for (; beginpaint < endpaint; beginpaint += 1) {
						if (*(beginpaint - width) == 0xFFFFFFFF) {
							*beginpaint = 0xFFFFFFFF;
						}
						else {
							ret = true;
						}
					}

					if (xi == width && (lastturncnt == 0 && beginpaint == endpaint)) {
						xi = lastendxi-1;
						unsigned int* insptr = (unsigned int*)&textureData[yi * 4 * width + lastendxi * 4];
						*insptr = 0xFFFFFFFF;
						lastturncnt += 1;
						continue;
					}

					if (ret == false) {
						while (*endpaint == 0xFFFFFFFF && xi < width) {
							xi += 1;
							endpaint += 1;
						}
						//last_fillstart_xi = pxi;
						lastendxi = xi;
						continue;
					}
					else {
						/*if (llfxi != last_fillstart_xi) {
							xi = last_fillstart_xi;
							llfxi = last_fillstart_xi;
						}*/

						xi = ptr - (unsigned int*)&textureData[yi * 4 * width];
						returning = true;
					}
				}
				
				
			}

			if (returning) {
				retcnt += 1;
				if(retcnt < 2) goto ROW_RETURN;
			}
			else retcnt = 0;
		}

		//for (int yi = 0; yi < height; ++yi) {
		//	bool isfill = false;
		//	for (int xi = 0; xi < width; ++xi) {
		//		unsigned int* ptr = (unsigned int*)&textureData[yi * 4 * width + xi * 4];
		//		vec4 vp = vec4(xi, yi, 0, 0);
		//		bool b = false;
		//		for (int pi = 0; pi < polygons.size(); ++pi) {
		//			b = !b != !bPointInPolygonRange(vp, polygons[pi]); // xor
		//		}
		//		if (b) {
		//			*ptr = 0xFFFFFFFF;
		//		}
		//	}
		//}

		//pio.rawDataToBMP("ImageDbg.bmp");

		int mipW = width / textureMipLevel;
		int mipH = height / textureMipLevel;
		BYTE* mipTex = new BYTE[4 * mipW * mipH];
		for (int yi = 0; yi < mipH; ++yi) {
			for (int xi = 0; xi < mipW; ++xi) {
				*(unsigned int*)&mipTex[yi * (4 * mipW) + xi * 4] = *(unsigned int*)&textureData[yi * textureMipLevel * 4 * width + xi* textureMipLevel * 4];
			}
		}
		delete[] textureData;

		GPUResource texture;
		ZeroMemory(&texture, sizeof(GPUResource));
		texture.CreateTexture_fromImageBuffer(mipW, mipH, mipTex);
		font_texture_map[i].insert(pair<wchar_t, GPUResource>(key, texture));
	}
}

void GlobalDevice::AddLineInTexture(float_v2 startpos, float_v2 endpos, BYTE* texture, int width, int height)
{
	//draw line
	float dy = (endpos.y - startpos.y);
	float dx = (endpos.x - startpos.x);

	if (fabsf(dy) > fabsf(dx)) {
		//y access
		float minY = min(startpos.y, endpos.y);
		float maxY = max(startpos.y, endpos.y);

		for (float di = minY; di < maxY + 1; di += 1.0f) {
			int x = dx / dy * (di - startpos.y) + startpos.x;

			if (x < 0) x = 0;
			if (x >= width) x = width - 1;
			int y = di;
			if (y < 0) y = 0;
			if (y >= height) y = height - 1;

			unsigned int* ptr = (unsigned int*)&texture[(int)y * 4 * width + x * 4];
			*ptr = 0xFFFFFFFF;
		}
	}
	else {
		//x access

		float minX = min(startpos.x, endpos.x);
		float maxX = max(startpos.x, endpos.x);

		for (float di = minX; di < maxX + 1; di += 1.0f) {
			int y = dy / dx * (di - startpos.x) + startpos.y;

			int x = di;
			if (x < 0) x = 0;
			if (x >= width) x = width - 1;

			if (y < 0) y = 0;
			if (y >= height) y = height - 1;

			unsigned int* ptr = (unsigned int*)&texture[y * 4 * width + (int)x * 4];
			*ptr = 0xFFFFFFFF;
		}
	}
}

void Game::SetLight()
{
	LightCBData = new LightCB_DATA();
	UINT ncbElementBytes = ((sizeof(LightCB_DATA) + 255) & ~255); //256의 배수
	LightCBResource = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1);
	LightCBResource.resource->Map(0, NULL, (void**)&LightCBData);
	LightCBData->dirlight.gLightColor = { 0.5f, 0.5f, 0 };
	LightCBData->dirlight.gLightDirection = { 1, -1, 1 };
	for (int i = 0; i < 8; ++i) {
		PointLight& p = LightCBData->pointLights[i];
		p.LightColor = { 1, 1, 1 };
		p.LightIntencity = 100;
		p.LightRange = 120;
		p.LightPos = { (float)(rand() % 80 - 40), 1, (float)(rand() % 80 - 40) };
	}
	LightCBData->pointLights[0].LightPos = { 0, 0, 0 };
	//LightCBData->pointLights[1].LightPos = { -5, 1, 5 };
	//LightCBData->pointLights[2].LightPos = { 5, 1, 5 };
	//LightCBData->pointLights[3].LightPos = { 5, 1, -5 };
	LightCBResource.resource->Unmap(0, nullptr);

	LightCBData_withShadow = new LightCB_DATA_withShadow();
	ncbElementBytes = ((sizeof(LightCB_DATA) + 255) & ~255); //256의 배수
	LightCB_withShadowResource = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1);
	LightCB_withShadowResource.resource->Map(0, NULL, (void**)&LightCBData_withShadow);
	LightCBData_withShadow->dirlight.gLightColor = { 0.5f, 0.5f, 0.5f };
	LightCBData_withShadow->dirlight.gLightDirection = { 1, -1, 1 };
	for (int i = 0; i < 8; ++i) {
		PointLight& p = LightCBData_withShadow->pointLights[i];
		p.LightPos = { (float)(rand() % 40 - 20), 1, (float)(rand() % 40 - 20) };
		p.LightIntencity = 20;
		p.LightColor = { 1, 1, 1 };
		p.LightRange = 50;
	}
	LightCBData_withShadow->LightProjection = gd.viewportArr[0].ProjectMatrix;
	LightCBData_withShadow->LightView = MySpotLight.View;
	LightCBData_withShadow->LightPos = MySpotLight.LightPos.f3;
	LightCBData_withShadow->pointLights[0].LightPos = { 10, 0, 0 };
	LightCB_withShadowResource.resource->Unmap(0, nullptr);
}

void Game::Init()
{
	TestTexture.CreateTexture_fromFile("Resources/Mesh/GlobalTexture/m1911pistol_diffuse0.png");

	gd.pCommandList->Reset(gd.pCommandAllocator, NULL);

	SetLight();

	MyShader = new Shader();
	MyShader->InitShader();

	MyOnlyColorShader = new OnlyColorShader();
	MyOnlyColorShader->InitShader();

	MyDiffuseTextureShader = new DiffuseTextureShader();
	MyDiffuseTextureShader->InitShader();

	MyScreenCharactorShader = new ScreenCharactorShader();
	MyScreenCharactorShader->InitShader();

	MyShadowMappingShader = new ShadowMappingShader();
	MyShadowMappingShader->InitShader();

	TextMesh = new UVMesh();
	TextMesh->CreateTextRectMesh();

	MySpotLight.ShadowMap = MyShadowMappingShader->CreateShadowMap(gd.ClientFrameWidth, gd.ClientFrameHeight, 0);
	MySpotLight.View.mat = XMMatrixLookAtLH(vec4(0, 2, 5, 0), vec4(0, 0, 0, 0), vec4(0, 1, 0, 0));

	bulletRays.Init(32);

	Mesh* MyMesh = new Mesh();
	MyMesh->ReadMeshFromFile_OBJ("Resources/Mesh/PlayerMesh.obj", {1, 1, 1, 1});

	Mesh* MyGroundMesh = new Mesh();
	MyGroundMesh->CreateWallMesh(100.0f, 0.5f, 100.0f, {1, 1, 1, 1});

	Mesh* MyWallMesh = new Mesh();
	MyWallMesh->CreateWallMesh(5.0f, 2.0f, 1.0f, {0, 0, 1, 1});

	BulletRay::mesh = new Mesh();
	BulletRay::mesh->ReadMeshFromFile_OBJ("Resources/Mesh/RayMesh.obj", { 1, 1, 0, 1 }, false);

	Player* MyPlayer = new Player(&pKeyBuffer[0]);
	MyPlayer->SetMesh(MyMesh);
	MyPlayer->SetShader(MyShader);
	MyPlayer->SetWorldMatrix(XMMatrixTranslation(0.0f, 3.0f, 0.0f));
	m_gameObjects.push_back(MyPlayer);
	player = MyPlayer;
	player->Gun = (Mesh*) new UVMesh();
	player->Gun->ReadMeshFromFile_OBJ("Resources/Mesh/m1911pistol.obj", { 1, 1, 1, 1 });
	player->gunMatrix_thirdPersonView.Id();
	player->gunMatrix_thirdPersonView.pos = vec4(0.35f, 0.5f, 0, 1);

	player->gunMatrix_firstPersonView.Id();
	player->gunMatrix_firstPersonView.pos = vec4(0.13f, -0.15f, 0.5f, 1);
	player->gunMatrix_firstPersonView.LookAt(vec4(0, 0, 5) - player->gunMatrix_firstPersonView.pos);

	player->ShootPointMesh = new Mesh();
	player->ShootPointMesh->CreateWallMesh(0.05f, 0.05f, 0.05f, { 1, 1, 1, 0.5f });

	player->HPBarMesh = new Mesh();
	player->HPBarMesh->ReadMeshFromFile_OBJ("Resources/Mesh/RayMesh.obj", { 1, 0, 0, 1 }, false);
	player->HPMatrix.pos = vec4( -1, 1, 1, 1 );
	player->HPMatrix.LookAt(vec4(-1, 0, 0));

	GameObject* groundObject = new GameObject();
	groundObject->SetMesh(MyGroundMesh);
	groundObject->SetShader(MyShader);
	groundObject->SetWorldMatrix(XMMatrixTranslation(0.0f, -1.0f, 0.0f));
	m_gameObjects.push_back(groundObject);

	GameObject* wallObject = new GameObject();
	wallObject->SetMesh(MyWallMesh);
	wallObject->SetShader(MyShader);
	wallObject->SetWorldMatrix(XMMatrixTranslation(0.0f, 1.0f, 5.0f));
	m_gameObjects.push_back(wallObject);

	GameObject* wallObject2 = new GameObject();
	wallObject2->SetMesh(MyWallMesh);
	wallObject2->SetShader(MyShader);
	wallObject2->SetWorldMatrix(XMMatrixMultiply(XMMatrixRotationY(45), XMMatrixTranslation(10.0f, 1.0f, 0.0f)));
	m_gameObjects.push_back(wallObject2);

	GameObject* wallObject3 = new GameObject();
	wallObject3->SetMesh(MyWallMesh);
	wallObject3->SetShader(MyShader);
	wallObject3->SetWorldMatrix(XMMatrixMultiply(XMMatrixRotationZ(3.141592f / 6), XMMatrixTranslation(5.0f, 0.0f, -5.0f)));
	m_gameObjects.push_back(wallObject3);

	GameObject* wallObject4 = new GameObject();
	wallObject4->SetMesh(MyWallMesh);
	wallObject4->SetShader(MyShader);
	wallObject4->SetWorldMatrix(XMMatrixMultiply(XMMatrixRotationZ(3.141592f / 4), XMMatrixTranslation(5.0f, -1.0f, -10.0f)));
	m_gameObjects.push_back(wallObject4);

	gd.pCommandList->Close();
	ID3D12CommandList* ppCommandLists[] = { gd.pCommandList };
	gd.pCommandQueue->ExecuteCommandLists(1, ppCommandLists);
	gd.WaitGPUComplete();

	gd.viewportArr[0].ProjectMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), (float)gd.ClientFrameWidth / (float)gd.ClientFrameHeight, 0.01f, 1000.0f);

	if (testData.UseBundle) {
		gd.CreateNewBundle();
		XMFLOAT4X4 xmf4x4Projection;
		XMStoreFloat4x4(&xmf4x4Projection, XMMatrixTranspose(gd.viewportArr[0].ProjectMatrix));
		gd.pBundles[0]->SetGraphicsRootSignature(MyShader->pRootSignature);
		gd.pBundles[0]->SetPipelineState(MyShader->pPipelineState);
		gd.pBundles[0]->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4Projection, 0);
		gd.pBundles[0]->Close();
	}
}

void Game::Render() {
	if (gd.addTextureStack.size() > 0) {
		for (int i = 0; i < gd.addTextureStack.size(); ++i) {
			gd.AddTextTexture(gd.addTextureStack[i]);
		}
	}
	gd.addTextureStack.clear();

	Render_ShadowPass();

	HRESULT hResult = gd.pCommandAllocator->Reset();
	hResult = gd.pCommandList->Reset(gd.pCommandAllocator, NULL);

	game.MySpotLight.ShadowMap.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	//Wait Finish Present
	D3D12_RESOURCE_BARRIER d3dResourceBarrier;
	::ZeroMemory(&d3dResourceBarrier, sizeof(D3D12_RESOURCE_BARRIER));
	d3dResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	d3dResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	d3dResourceBarrier.Transition.pResource =
		gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex];
	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	gd.pCommandList->ResourceBarrier(1, &d3dResourceBarrier);

	gd.pCommandList->RSSetViewports(1, &gd.viewportArr[0].Viewport);
	gd.pCommandList->RSSetScissorRects(1, &gd.viewportArr[0].ScissorRect);

	//Clear Render Target Command Addition
	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle =
		gd.pRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	d3dRtvCPUDescriptorHandle.ptr += (gd.CurrentSwapChainBufferIndex *
		gd.RtvDescriptorIncrementSize);
	D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle =
		gd.pDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	gd.pCommandList->OMSetRenderTargets(1, &d3dRtvCPUDescriptorHandle, TRUE,
		&d3dDsvCPUDescriptorHandle);
	float pfClearColor[4] = { 0.05f, 0.05f, 0.05f, 1.0f };
	gd.pCommandList->ClearRenderTargetView(d3dRtvCPUDescriptorHandle, pfClearColor, 0, NULL);
	
	//render begin ----------------------------------------------------------------

	MyShader->Add_RegisterShaderCommand(gd.pCommandList, Shader::ShadowRegisterEnum::RenderWithShadow);

	XMFLOAT4X4 xmf4x4Projection;
	XMStoreFloat4x4(&xmf4x4Projection, XMMatrixTranspose(gd.viewportArr[0].ProjectMatrix));
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4Projection, 0);

	XMFLOAT4X4 xmf4x4View;
	XMStoreFloat4x4(&xmf4x4View, XMMatrixTranspose(MySpotLight.View));
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4View, 16);
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 32); // no matter

	gd.pCommandList->SetDescriptorHeaps(1, &game.MyShadowMappingShader->svdp.m_pDescritorHeap);

	//gd.pCommandList->SetGraphicsRootDescriptorTable(3, game.MyShadowMappingShader->svdp.m_pDescritorHeap->GetGPUDescriptorHandleForHeapStart());

	MyShader->SetShadowMapCommand(&game.MySpotLight.ShadowMap);

	XMStoreFloat4x4(&xmf4x4Projection, XMMatrixTranspose(gd.viewportArr[0].ProjectMatrix));

	if (testData.UseBundle) {
		gd.pCommandList->ExecuteBundle(gd.pBundles[0]);
	}
	else {
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4Projection, 0);
	}

	//camera view
	XMStoreFloat4x4(&xmf4x4View, XMMatrixTranspose(gd.viewportArr[0].ViewMatrix));
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4View, 16);
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 32);
	gd.pCommandList->SetGraphicsRootConstantBufferView(2, game.LightCB_withShadowResource.resource->GetGPUVirtualAddress());

	for (auto& gbj : m_gameObjects) {
		gbj->Render();
	}

	((Shader*)MyOnlyColorShader)->Add_RegisterShaderCommand(gd.pCommandList);

	for (int i = 0; i < bulletRays.size; ++i) {
		if (bulletRays.isnull(i)) continue;
		bulletRays[i].Render();
	}

	gd.pCommandList->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);

	//((Shader*)MyShader)->Add_RegisterShaderCommand(gd.pCommandList);

	// UI/AfterDepthClear Render
	game.player->Render_AfterDepthClear();

	/*((Shader*)MyScreenCharactorShader)->Add_RegisterShaderCommand(gd.pCommandList);*/

	/*static float stackT = 0;
	stackT += game.DeltaTime;
	RenderText(L"0123456789\nHello, World!\n안녕 너무 반가워~~", 36, vec4(-1000, 0, 1000, 100), 50 + 50 * sin(stackT));*/

	//render end ----------------------------------------------------------------
	gd.pCommandList->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);

	//RenderTarget State Changing Command [RenderTarget -> Present] + wait untill finish rendering
	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	gd.pCommandList->ResourceBarrier(1, &d3dResourceBarrier);

	//Command execution
	hResult = gd.pCommandList->Close();
	ID3D12CommandList* ppd3dCommandLists[] = { gd.pCommandList };
	gd.pCommandQueue->ExecuteCommandLists(1, ppd3dCommandLists);

	gd.WaitGPUComplete();
	
	// Present to Swapchain BackBuffer & RenderTargetIndex Update
	DXGI_PRESENT_PARAMETERS dxgiPresentParameters;
	dxgiPresentParameters.DirtyRectsCount = 0;
	dxgiPresentParameters.pDirtyRects = NULL;
	dxgiPresentParameters.pScrollRect = NULL;
	dxgiPresentParameters.pScrollOffset = NULL;
	gd.pSwapChain->Present1(1, 0, &dxgiPresentParameters);

	//Get Present RenderTarget Index
	gd.CurrentSwapChainBufferIndex = gd.pSwapChain->GetCurrentBackBufferIndex();
}

void Game::Render_ShadowPass()
{
	gd.ShaderVisibleDescPool.Reset();
	HRESULT hResult = gd.pCommandAllocator->Reset();
	hResult = gd.pCommandList->Reset(gd.pCommandAllocator, NULL);

	//Wait Finish Present
	D3D12_RESOURCE_BARRIER d3dResourceBarrier;
	::ZeroMemory(&d3dResourceBarrier, sizeof(D3D12_RESOURCE_BARRIER));
	d3dResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	d3dResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	d3dResourceBarrier.Transition.pResource =
		gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex];
	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	gd.pCommandList->ResourceBarrier(1, &d3dResourceBarrier);

	gd.pCommandList->RSSetViewports(1, &gd.viewportArr[0].Viewport);
	gd.pCommandList->RSSetScissorRects(1, &gd.viewportArr[0].ScissorRect);

	//Clear Render Target Command Addition
	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle =
		gd.pRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	d3dRtvCPUDescriptorHandle.ptr += (gd.CurrentSwapChainBufferIndex *
		gd.RtvDescriptorIncrementSize);
	/*float pfClearColor[4] = { 0, 0, 0, 1.0f };
	gd.pCommandList->ClearRenderTargetView(d3dRtvCPUDescriptorHandle,
		pfClearColor, 0, NULL);*/

		//Render Shadow Map (Shadow Pass)
		//spotlight view
	game.MySpotLight.ShadowMap.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	//Clear Depth Stencil Buffer Command Addtion
	D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle =
		gd.pDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	gd.pCommandList->OMSetRenderTargets(0, nullptr, TRUE, &game.MySpotLight.ShadowMap.hCpu);
	//there is no render target only depth map. ??
	gd.pCommandList->ClearDepthStencilView(game.MySpotLight.ShadowMap.hCpu,
		D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);

	MyShader->Add_RegisterShaderCommand(gd.pCommandList, Shader::ShadowRegisterEnum::RenderShadowMap);

	XMFLOAT4X4 xmf4x4Projection;
	XMStoreFloat4x4(&xmf4x4Projection, XMMatrixTranspose(gd.viewportArr[0].ProjectMatrix));
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4Projection, 0);

	XMFLOAT4X4 xmf4x4View;
	XMStoreFloat4x4(&xmf4x4View, XMMatrixTranspose(MySpotLight.View));
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4View, 16);
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 32); // no matter

	for (auto& gbj : m_gameObjects) {
		gbj->Render();
	}

	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	gd.pCommandList->ResourceBarrier(1, &d3dResourceBarrier);

	gd.pCommandList->Close();
	ID3D12CommandList* ppd3dCommandLists_Shadow[] = { gd.pCommandList };
	gd.pCommandQueue->ExecuteCommandLists(1, ppd3dCommandLists_Shadow);

	gd.WaitGPUComplete();
}

void Game::Update()
{
	//CameraUpdate
	XMVECTOR vEye = player->m_worldMatrix.pos;
	vec4 peye = player->m_worldMatrix.pos;
	vec4 pat = player->m_worldMatrix.pos;

	const float rate = 0.01f;

	XMVECTOR quaternion = XMQuaternionRotationRollPitchYaw(rate * game.DeltaMousePos.y, rate * game.DeltaMousePos.x, 0);
	vec4 clook = {0, 0, 1, 1};
	vec4 plook;
	clook = XMVector3Rotate(clook, quaternion);

	plook = clook;
	plook.y = 0;
	plook.len3 = 1;
	player->m_worldMatrix.LookAt(plook);

	if (bFirstPersonVision) {
		peye += 1.0f * player->m_worldMatrix.up;
		pat += 1.0f * player->m_worldMatrix.up;
		pat += 10 * clook;
	}
	else {
		peye -= 3 * clook;
		pat += 10 * clook;
		peye += 0.5f * player->m_worldMatrix.up;
		peye += 0.5f * player->m_worldMatrix.right;
	}

	XMFLOAT3 Up = { 0, 1, 0 };
	gd.viewportArr[0].ViewMatrix = XMMatrixLookAtLH(peye, pat, XMLoadFloat3(&Up));
	gd.viewportArr[0].Camera_Pos = peye;

	// spotLight Move
	static float StackTime0 = 0;
	StackTime0 += game.DeltaTime;
	MySpotLight.LightPos = //gd.viewportArr[0].Camera_Pos;
		vec4(10*cosf(StackTime0/10), 10, 10 * sinf(StackTime0/10));
	MySpotLight.View.mat = //gd.viewportArr[0].ViewMatrix.mat;
		XMMatrixLookAtLH(MySpotLight.LightPos, vec4(0, 0, 0, 0), vec4(0, 1, 0, 0));  
		
		//gd.viewportArr[0].ViewMatrix;//XMMatrixLookAtLH(MySpotLight.LightPos, vec4(0, 0, 0, 0), vec4(0, 1, 0, 0));
	matrix projmat = XMMatrixTranspose(gd.viewportArr[0].ProjectMatrix);

	LightCB_withShadowResource.resource->Map(0, NULL, (void**)&LightCBData_withShadow);
	LightCBData_withShadow->LightProjection = projmat;
	LightCBData_withShadow->LightView = XMMatrixTranspose(MySpotLight.View);
	LightCBData_withShadow->LightPos = MySpotLight.LightPos.f3;
	LightCB_withShadowResource.resource->Unmap(0, nullptr);

	for (auto& gbj : m_gameObjects) {
		gbj->Update(DeltaTime);
	}

	for (int i = 0; i < bulletRays.size; ++i) {
		if (bulletRays.isnull(i)) continue;
		bulletRays[i].Update();
		if (bulletRays[i].direction.fast_square_of_len3 == 0) {
			bulletRays.Free(i);
		}
	}

	// Collision......
	
	for (int i = 0; i < m_gameObjects.size(); ++i) {
		GameObject* gbj1 = m_gameObjects[i];

		for (int j = i + 1; j < m_gameObjects.size(); ++j) {
			GameObject* gbj2 = m_gameObjects[j];
			GameObject::CollisionMove(gbj1, gbj2);
		}

		gbj1->m_worldMatrix.trQ(gbj1->tickAVelocity);
		gbj1->m_worldMatrix.pos += gbj1->tickLVelocity;
		gbj1->tickLVelocity = XMVectorZero();
		gbj1->LastQuerternion = gbj1->m_worldMatrix.getQ();
		gbj1->tickAVelocity = 0;
	}
}

void Game::Event(WinEvent evt)
{
	for (int i = 0; i < m_gameObjects.size(); ++i) {
		m_gameObjects[i]->Event(evt);
	}
}

void Game::Release()
{
	if (MyShader) MyShader->Release();
	if (MyOnlyColorShader) MyOnlyColorShader->Release();
	if (MyDiffuseTextureShader) MyDiffuseTextureShader->Release();
	if (MyScreenCharactorShader) MyScreenCharactorShader->Release();
	for (int i = 0; i < m_gameObjects.size(); ++i) {
		if (m_gameObjects[i] != nullptr) {
			delete m_gameObjects[i];
			m_gameObjects[i] = nullptr;
		}

		//player also deleting..
	}
	m_gameObjects.clear();
	player = nullptr;

	TestTexture.Release();
	bulletRays.Release();
}

void Game::FireRaycast(vec4 rayStart, vec4 rayDirection, float rayDistance)
{
	vec4 rayOrigin = rayStart;

	GameObject* closestHitObject = nullptr;

	float closestDistance = rayDistance;

	for (int i = 0; i < m_gameObjects.size(); ++i) {
		BoundingOrientedBox obb = m_gameObjects[i]->GetOBB();
		float distance;

		if (obb.Intersects(rayOrigin, rayDirection, distance)) {
			if (distance <= rayDistance) {
				if (distance < closestDistance) {
					closestDistance = distance;
					closestHitObject = m_gameObjects[i];
				}
			}
		}
	}

	int BulletIndex = bulletRays.Alloc();
	if (BulletIndex < 0) {
		return;
	}
	BulletRay& bray = bulletRays[BulletIndex];
	bray = BulletRay(rayStart, rayDirection, closestDistance);

	if (closestHitObject) {
		// hit calculation
	}
}

//this function must call after ScreenCharactorShader register in pipeline.
void Game::RenderText(const wchar_t* wstr, int length, vec4 Rect, float fontsiz)
{
	//fontsize = 100 -> 0.1x | 10 -> 0.01x
	vec4 pos = vec4(Rect.x, Rect.y, 0, 0);
	constexpr float Default_LineHeight = 750;
	float mul = fontsiz / Default_LineHeight;

	constexpr float lineheight_mul = 2.5f;
	float lineheight = lineheight_mul * fontsiz;
	for (int i = 0; i < length; ++i) {
		wchar_t wc = wstr[i];
		if (wc == L'\n') {
			pos.x = Rect.x;
			pos.y -= lineheight;
			continue;
		}
		else if (wc == L' ') {
			pos.x += fontsiz;
			continue;
		}
		bool textureExist = false;
		GPUResource* texture = nullptr;
		Glyph g;
		for (int k = 0; k < gd.FontCount; ++k) {
			if (gd.font_texture_map[k].find(wc) != gd.font_texture_map[k].end()) {
				textureExist = true;
				texture = &gd.font_texture_map[k][wc];
				g = gd.font_data[k].glyphs[wc];
				break;
			}
		}

		if (textureExist == false) {
			gd.addTextureStack.push_back(wc);
			continue;
		}

		//set root variables
		vec4 textRt = vec4(pos.x + g.bounding_box[0] * mul, pos.y + g.bounding_box[1] * mul, pos.x + g.bounding_box[2] * mul, pos.y + g.bounding_box[3] * mul);
		float tConst[6] = { pos.x + g.bounding_box[0] * mul, pos.y + g.bounding_box[1] * mul, pos.x + g.bounding_box[2] * mul, pos.y + g.bounding_box[3] * mul , gd.ClientFrameWidth , gd.ClientFrameHeight };
		/*gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &textRt, 0);
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 1, &gd.ClientFrameWidth, 4);
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 1, &gd.ClientFrameHeight, 5);*/
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 6, &tConst, 0);
		MyScreenCharactorShader->SetTextureCommand(texture);

		//Render Text
		TextMesh->Render(gd.pCommandList, 1);

		//calculate next location of text
		pos.x += g.advance_width * mul;
		if (pos.x > Rect.z) {
			pos.x = Rect.x;
			pos.y -= lineheight;
			if (pos.y > Rect.w) {
				return;
			}
		}
	}
}

Shader::Shader() {

}
Shader::~Shader() {

}
void Shader::InitShader() {
	CreateRootSignature();
	CreateRootSignature_withShadow();
	CreatePipelineState();
	CreatePipelineState_withShadow();
	CreatePipelineState_RenderShadowMap();
}

void Shader::CreateRootSignature()
{
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;

	D3D12_ROOT_PARAMETER1 rootParam[3] = {};

	rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParam[0].Constants.Num32BitValues = 36;
	rootParam[0].Constants.ShaderRegister = 0; // b0 : Camera Matrix (Projection, View) + Camera Positon
	rootParam[0].Constants.RegisterSpace = 0;

	rootParam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParam[1].Constants.Num32BitValues = 16;
	rootParam[1].Constants.ShaderRegister = 1; // b1 : Transform Matrix
	rootParam[1].Constants.RegisterSpace = 0;

	rootParam[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParam[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParam[2].Descriptor.ShaderRegister = 2; // b2 : Light Data
	rootParam[2].Descriptor.RegisterSpace = 0;

	D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT; /*|
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;*/

	rootSigDesc1.NumParameters = 3;
	rootSigDesc1.pParameters = rootParam;
	rootSigDesc1.NumStaticSamplers = 0; // question002 : what is static samplers?
	rootSigDesc1.pStaticSamplers = NULL; //
	rootSigDesc1.Flags = d3dRootSignatureFlags;

	ID3DBlob* pd3dSignatureBlob = NULL;
	ID3DBlob* pd3dErrorBlob = NULL;
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_rootSigDesc;
	versioned_rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	versioned_rootSigDesc.Desc_1_1 = rootSigDesc1;
	D3D12SerializeVersionedRootSignature(&versioned_rootSigDesc, &pd3dSignatureBlob, &pd3dErrorBlob);
	gd.pDevice->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(),
		pd3dSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void
			**)&pRootSignature);
	if (pd3dErrorBlob) pd3dErrorBlob->Release();
	if (pd3dSignatureBlob) pd3dSignatureBlob->Release();
}

void Shader::CreateRootSignature_withShadow()
{
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;

	D3D12_ROOT_PARAMETER1 rootParam[4] = {};

	rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParam[0].Constants.Num32BitValues = 36;
	rootParam[0].Constants.ShaderRegister = 0; // b0 : Camera Matrix (Projection, View) + Camera Positon
	rootParam[0].Constants.RegisterSpace = 0;

	rootParam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParam[1].Constants.Num32BitValues = 16;
	rootParam[1].Constants.ShaderRegister = 1; // b1 : Transform Matrix
	rootParam[1].Constants.RegisterSpace = 0;

	rootParam[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParam[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParam[2].Descriptor.ShaderRegister = 2; // b2 : Static Light Data
	rootParam[2].Descriptor.RegisterSpace = 0;

	rootParam[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParam[3].DescriptorTable.NumDescriptorRanges = 1;
	D3D12_DESCRIPTOR_RANGE1 ranges[1];
	ranges[0].BaseShaderRegister = 0; // t0 //Shadow Map Texture
	ranges[0].RegisterSpace = 0;
	ranges[0].NumDescriptors = 1;
	ranges[0].OffsetInDescriptorsFromTableStart = 0;
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	rootParam[3].DescriptorTable.pDescriptorRanges = &ranges[0];

	/// default sampler
	D3D12_STATIC_SAMPLER_DESC sampler = {};
	// sampler register index.
	sampler.ShaderRegister = 0;
	// setting
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.MipLODBias = 0.0f;
	sampler.MaxAnisotropy = 16;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	sampler.MinLOD = -FLT_MAX;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

	D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;/* |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;*/

	rootSigDesc1.NumParameters = sizeof(rootParam) / sizeof(D3D12_ROOT_PARAMETER1);
	rootSigDesc1.pParameters = rootParam;
	rootSigDesc1.NumStaticSamplers = 1;
	rootSigDesc1.pStaticSamplers = &sampler;
	rootSigDesc1.Flags = d3dRootSignatureFlags;

	ID3DBlob* pd3dSignatureBlob = NULL;
	ID3DBlob* pd3dErrorBlob = NULL;
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_rootSigDesc;
	versioned_rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	versioned_rootSigDesc.Desc_1_1 = rootSigDesc1;
	HRESULT hr = D3D12SerializeVersionedRootSignature(&versioned_rootSigDesc, &pd3dSignatureBlob, &pd3dErrorBlob);
	hr = gd.pDevice->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(),
		pd3dSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void
			**)&pRootSignature_withShadow);
	if (pd3dErrorBlob) pd3dErrorBlob->Release();
	if (pd3dSignatureBlob) pd3dSignatureBlob->Release();
}

void Shader::CreatePipelineState()
{
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dVertexShaderBlob = NULL, * pd3dPixelShaderBlob = NULL;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	
	gPipelineStateDesc.pRootSignature = pRootSignature;
	
	gPipelineStateDesc.NodeMask = 0;

	//Input Asm
	gPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[3] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // color vec4
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // normal vec3
	};
	gPipelineStateDesc.InputLayout.NumElements = 3;
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/DefaultShader.hlsl", "VSMain", "vs_5_1", &pd3dVertexShaderBlob);

	//Hull Shader
	//gPipelineStateDesc.HS = NULLCODE;

	//Tessellation

	//Domain Shader
	//gPipelineStateDesc.DS = NULLCODE;

	//Geometry Shader
	//gPipelineStateDesc.GS = NULLCODE;

	//Stream Output
	//gPipelineStateDesc.StreamOutput

	//Rasterazer
	gPipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	gPipelineStateDesc.RasterizerState.FrontCounterClockwise = FALSE; // front = clock wise
	//Rasterazer-depth
	gPipelineStateDesc.RasterizerState.DepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthBiasClamp = 0;
	gPipelineStateDesc.RasterizerState.SlopeScaledDepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthClipEnable = TRUE; // if depth > 1, cliping vertex.
	//Rasterazer-MSAA
	gPipelineStateDesc.RasterizerState.MultisampleEnable = TRUE;
	gPipelineStateDesc.RasterizerState.AntialiasedLineEnable = TRUE;
	gPipelineStateDesc.RasterizerState.ForcedSampleCount = 0; // sample count of UAV rendering // question 003 : what is UAV rendering?
	//Rasterazer - Conservative Rendering On/Off - (bosujuk rendering)
	gPipelineStateDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	//Pixel Shader
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/DefaultShader.hlsl", "PSMain", "ps_5_1", &pd3dPixelShaderBlob);
	
	//Output Merger
	//Output Merger-Blend
	D3D12_BLEND_DESC d3dBlendDesc;
	::ZeroMemory(&d3dBlendDesc, sizeof(D3D12_BLEND_DESC));
	d3dBlendDesc.AlphaToCoverageEnable = FALSE;
	d3dBlendDesc.IndependentBlendEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	d3dBlendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	d3dBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	gPipelineStateDesc.BlendState = d3dBlendDesc;
	//Output Merger - MSAA
	gPipelineStateDesc.SampleDesc.Count = (gd.m_bMsaa4xEnable) ? 4 : 1;
	gPipelineStateDesc.SampleDesc.Quality = (gd.m_bMsaa4xEnable) ? (gd.m_nMsaa4xQualityLevels - 1) : 0;
	gPipelineStateDesc.SampleMask = 0xFFFFFFFF; // pass every sampling.
	//Output Merger - DepthStencil
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc; // D3D12_DEPTH_STENCIL_DESC1 is using for Stream Pipeline state.. -> what is that?
	//Output Merger - DepthStencil - depth
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	//depthStencilDesc.DepthBoundsTestEnable = FALSE; // question 004 : what is this??
	//Output Merger - DepthStencil - stencil
	depthStencilDesc.StencilEnable = FALSE;
	depthStencilDesc.StencilReadMask = 0x00;
	depthStencilDesc.StencilWriteMask = 0x00;
	depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	depthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	gPipelineStateDesc.DepthStencilState = depthStencilDesc;
	//Output Merger - RenderTagets / DepthStencil Buffer
	gPipelineStateDesc.NumRenderTargets = 1;
	gPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	
	gd.pDevice->CreateGraphicsPipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState);
	if (pd3dVertexShaderBlob) pd3dVertexShaderBlob->Release();
	if (pd3dPixelShaderBlob) pd3dPixelShaderBlob->Release();
}

void Shader::CreatePipelineState_withShadow()
{
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dVertexShaderBlob = NULL, * pd3dPixelShaderBlob = NULL;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature_withShadow;

	gPipelineStateDesc.NodeMask = 0;

	//Input Asm
	gPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[3] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // color vec4
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // normal vec3
	};
	gPipelineStateDesc.InputLayout.NumElements = 3;
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/DefaultShader_withShadow.hlsl", "VSMain", "vs_5_1", &pd3dVertexShaderBlob);

	//Hull Shader
	//gPipelineStateDesc.HS = NULLCODE;

	//Tessellation

	//Domain Shader
	//gPipelineStateDesc.DS = NULLCODE;

	//Geometry Shader
	//gPipelineStateDesc.GS = NULLCODE;

	//Stream Output
	//gPipelineStateDesc.StreamOutput

	//Rasterazer
	gPipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	gPipelineStateDesc.RasterizerState.FrontCounterClockwise = FALSE; // front = clock wise
	//Rasterazer-depth
	gPipelineStateDesc.RasterizerState.DepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthBiasClamp = 0;
	gPipelineStateDesc.RasterizerState.SlopeScaledDepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthClipEnable = TRUE; // if depth > 1, cliping vertex.
	//Rasterazer-MSAA
	gPipelineStateDesc.RasterizerState.MultisampleEnable = TRUE;
	gPipelineStateDesc.RasterizerState.AntialiasedLineEnable = TRUE;
	gPipelineStateDesc.RasterizerState.ForcedSampleCount = 0; // sample count of UAV rendering // question 003 : what is UAV rendering?
	//Rasterazer - Conservative Rendering On/Off - (bosujuk rendering)
	gPipelineStateDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	//Pixel Shader
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/DefaultShader_withShadow.hlsl", "PSMain", "ps_5_1", &pd3dPixelShaderBlob);

	//Output Merger
	//Output Merger-Blend
	D3D12_BLEND_DESC d3dBlendDesc;
	::ZeroMemory(&d3dBlendDesc, sizeof(D3D12_BLEND_DESC));
	d3dBlendDesc.AlphaToCoverageEnable = FALSE;
	d3dBlendDesc.IndependentBlendEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	d3dBlendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	d3dBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	gPipelineStateDesc.BlendState = d3dBlendDesc;
	//Output Merger - MSAA
	gPipelineStateDesc.SampleDesc.Count = (gd.m_bMsaa4xEnable) ? 4 : 1;
	gPipelineStateDesc.SampleDesc.Quality = (gd.m_bMsaa4xEnable) ? (gd.m_nMsaa4xQualityLevels - 1) : 0;
	gPipelineStateDesc.SampleMask = 0xFFFFFFFF; // pass every sampling.
	//Output Merger - DepthStencil
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc; // D3D12_DEPTH_STENCIL_DESC1 is using for Stream Pipeline state.. -> what is that?
	//Output Merger - DepthStencil - depth
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	//depthStencilDesc.DepthBoundsTestEnable = FALSE; // question 004 : what is this??
	//Output Merger - DepthStencil - stencil
	depthStencilDesc.StencilEnable = FALSE;
	depthStencilDesc.StencilReadMask = 0x00;
	depthStencilDesc.StencilWriteMask = 0x00;
	depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	depthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	gPipelineStateDesc.DepthStencilState = depthStencilDesc;
	//Output Merger - RenderTagets / DepthStencil Buffer
	gPipelineStateDesc.NumRenderTargets = 1;
	gPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	gd.pDevice->CreateGraphicsPipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState_withShadow);
	if (pd3dVertexShaderBlob) pd3dVertexShaderBlob->Release();
	if (pd3dPixelShaderBlob) pd3dPixelShaderBlob->Release();
}

void Shader::CreatePipelineState_RenderShadowMap()
{
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dVertexShaderBlob = NULL, * pd3dPixelShaderBlob = NULL;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature;

	gPipelineStateDesc.NodeMask = 0;

	//Input Asm
	gPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[3] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // color vec4
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // normal vec3
	};
	gPipelineStateDesc.InputLayout.NumElements = 3;
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/DefaultShader.hlsl", "VSMain", "vs_5_1", &pd3dVertexShaderBlob);

	//Hull Shader
	//gPipelineStateDesc.HS = NULLCODE;

	//Tessellation

	//Domain Shader
	//gPipelineStateDesc.DS = NULLCODE;

	//Geometry Shader
	//gPipelineStateDesc.GS = NULLCODE;

	//Stream Output
	//gPipelineStateDesc.StreamOutput

	//Rasterazer
	gPipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	gPipelineStateDesc.RasterizerState.FrontCounterClockwise = FALSE; // front = clock wise
	//Rasterazer-depth
	gPipelineStateDesc.RasterizerState.DepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthBiasClamp = 0;
	gPipelineStateDesc.RasterizerState.SlopeScaledDepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthClipEnable = TRUE; // if depth > 1, cliping vertex.
	//Rasterazer-MSAA
	gPipelineStateDesc.RasterizerState.MultisampleEnable = TRUE;
	gPipelineStateDesc.RasterizerState.AntialiasedLineEnable = TRUE;
	gPipelineStateDesc.RasterizerState.ForcedSampleCount = 0; // sample count of UAV rendering // question 003 : what is UAV rendering?
	//Rasterazer - Conservative Rendering On/Off - (bosujuk rendering)
	gPipelineStateDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	//Pixel Shader
	gPipelineStateDesc.PS = NULLCODE;

	//Output Merger
	//Output Merger-Blend
	D3D12_BLEND_DESC d3dBlendDesc;
	::ZeroMemory(&d3dBlendDesc, sizeof(D3D12_BLEND_DESC));
	d3dBlendDesc.AlphaToCoverageEnable = FALSE;
	d3dBlendDesc.IndependentBlendEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	d3dBlendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	d3dBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	gPipelineStateDesc.BlendState = d3dBlendDesc;
	//Output Merger - MSAA
	gPipelineStateDesc.SampleDesc.Count = (gd.m_bMsaa4xEnable) ? 4 : 1;
	gPipelineStateDesc.SampleDesc.Quality = (gd.m_bMsaa4xEnable) ? (gd.m_nMsaa4xQualityLevels - 1) : 0;
	gPipelineStateDesc.SampleMask = 0xFFFFFFFF; // pass every sampling.
	//Output Merger - DepthStencil
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc; // D3D12_DEPTH_STENCIL_DESC1 is using for Stream Pipeline state.. -> what is that?
	//Output Merger - DepthStencil - depth
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	//depthStencilDesc.DepthBoundsTestEnable = FALSE; // question 004 : what is this??
	//Output Merger - DepthStencil - stencil
	depthStencilDesc.StencilEnable = FALSE;
	depthStencilDesc.StencilReadMask = 0x00;
	depthStencilDesc.StencilWriteMask = 0x00;
	depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	depthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	gPipelineStateDesc.DepthStencilState = depthStencilDesc;
	//Output Merger - RenderTagets / DepthStencil Buffer
	//gPipelineStateDesc.NumRenderTargets = 1;
	//gPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	//gPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	gPipelineStateDesc.NumRenderTargets = 0;
	gPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	gPipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	gPipelineStateDesc.SampleDesc.Count = 1;

	gd.pDevice->CreateGraphicsPipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState_RenderShadowMap);
	if (pd3dVertexShaderBlob) pd3dVertexShaderBlob->Release();
	if (pd3dPixelShaderBlob) pd3dPixelShaderBlob->Release();
}

void Shader::Add_RegisterShaderCommand(ID3D12GraphicsCommandList* commandList, Shader::ShadowRegisterEnum reg)
{
	switch (reg) {
	case ShadowRegisterEnum::RenderNormal:
		commandList->SetPipelineState(pPipelineState);
		commandList->SetGraphicsRootSignature(pRootSignature);
		return;
	case ShadowRegisterEnum::RenderWithShadow:
		commandList->SetPipelineState(pPipelineState_withShadow);
		commandList->SetGraphicsRootSignature(pRootSignature_withShadow);
		return;
	case ShadowRegisterEnum::RenderShadowMap:
		commandList->SetPipelineState(pPipelineState_RenderShadowMap);
		commandList->SetGraphicsRootSignature(pRootSignature);
		return;
	}
}

void Shader::Release()
{
	if (pPipelineState) pPipelineState->Release();
	if (pRootSignature) pRootSignature->Release();
}

D3D12_SHADER_BYTECODE Shader::GetShaderByteCode(const WCHAR* pszFileName, LPCSTR pszShaderName, LPCSTR pszShaderProfile, ID3DBlob** ppd3dShaderBlob)
{
	UINT nCompileFlags = 0;
#if defined(_DEBUG)
	nCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
	ID3DBlob* CompileOutput;
	HRESULT hr = D3DCompileFromFile(pszFileName, NULL, NULL, pszShaderName, pszShaderProfile, nCompileFlags, 0, ppd3dShaderBlob, &CompileOutput);
	if (FAILED(hr)) {
		OutputDebugStringA((char*)CompileOutput->GetBufferPointer());
	}
	D3D12_SHADER_BYTECODE d3dShaderByteCode;
	d3dShaderByteCode.BytecodeLength = (*ppd3dShaderBlob)->GetBufferSize();
	d3dShaderByteCode.pShaderBytecode = (*ppd3dShaderBlob)->GetBufferPointer();

	constexpr bool ShaderReflection = true;
	if constexpr (ShaderReflection)
	{
		ID3D12ShaderReflection* pReflection = nullptr;
		D3DReflect((*ppd3dShaderBlob)->GetBufferPointer(), (*ppd3dShaderBlob)->GetBufferSize(), IID_ID3D12ShaderReflection, (void**)&pReflection);

		D3D12_SHADER_DESC shaderDesc;
		pReflection->GetDesc(&shaderDesc);

		for (UINT i = 0; i < shaderDesc.BoundResources; ++i) {
			D3D12_SHADER_INPUT_BIND_DESC bindDesc;
			pReflection->GetResourceBindingDesc(i, &bindDesc);
			dbglog2(L"Type:%d - register(%d)\n", bindDesc.Type, bindDesc.BindPoint);
			// bindDesc.Name, bindDesc.Type, bindDesc.BindPoint 등으로 RootParameter 구성 가능
		}
	}
	

	return(d3dShaderByteCode);
}

void Shader::SetShadowMapCommand(GPUResource* shadowMap)
{
	gd.pCommandList->SetGraphicsRootDescriptorTable(3, shadowMap->hGpu);
}

OnlyColorShader::OnlyColorShader()
{
	
}

OnlyColorShader::~OnlyColorShader()
{
}

void OnlyColorShader::InitShader()
{
	CreateRootSignature();
	CreatePipelineState();
}

void OnlyColorShader::CreateRootSignature()
{
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;

	D3D12_ROOT_PARAMETER1 rootParam[3] = {};

	rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParam[0].Constants.Num32BitValues = 32;
	rootParam[0].Constants.ShaderRegister = 0; // b0 : Camera Matrix (Projection, View)
	rootParam[0].Constants.RegisterSpace = 0;

	rootParam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParam[1].Constants.Num32BitValues = 16;
	rootParam[1].Constants.ShaderRegister = 1; // b1 : Transform Matrix
	rootParam[1].Constants.RegisterSpace = 0;

	D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	rootSigDesc1.NumParameters = 2;
	rootSigDesc1.pParameters = rootParam;
	rootSigDesc1.NumStaticSamplers = 0; // question002 : what is static samplers?
	rootSigDesc1.pStaticSamplers = NULL; //
	rootSigDesc1.Flags = d3dRootSignatureFlags;

	ID3DBlob* pd3dSignatureBlob = NULL;
	ID3DBlob* pd3dErrorBlob = NULL;
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_rootSigDesc;
	versioned_rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	versioned_rootSigDesc.Desc_1_1 = rootSigDesc1;
	D3D12SerializeVersionedRootSignature(&versioned_rootSigDesc, &pd3dSignatureBlob, &pd3dErrorBlob);
	gd.pDevice->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(),
		pd3dSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void
			**)&pRootSignature);
	if (pd3dErrorBlob) pd3dErrorBlob->Release();
	if (pd3dSignatureBlob) pd3dSignatureBlob->Release();
}

void OnlyColorShader::CreatePipelineState()
{
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dVertexShaderBlob = NULL, * pd3dPixelShaderBlob = NULL;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature;

	gPipelineStateDesc.NodeMask = 0;

	//Input Asm
	gPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[3] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // color vec4
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // normal vec3
	};
	gPipelineStateDesc.InputLayout.NumElements = 3;
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/OnlyColorShader.hlsl", "VSMain", "vs_5_1", &pd3dVertexShaderBlob);

	//Hull Shader
	//gPipelineStateDesc.HS = NULLCODE;

	//Tessellation

	//Domain Shader
	//gPipelineStateDesc.DS = NULLCODE;

	//Geometry Shader
	//gPipelineStateDesc.GS = NULLCODE;

	//Stream Output
	//gPipelineStateDesc.StreamOutput

	//Rasterazer
	gPipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	gPipelineStateDesc.RasterizerState.FrontCounterClockwise = FALSE; // front = clock wise
	//Rasterazer-depth
	gPipelineStateDesc.RasterizerState.DepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthBiasClamp = 0;
	gPipelineStateDesc.RasterizerState.SlopeScaledDepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthClipEnable = TRUE; // if depth > 1, cliping vertex.
	//Rasterazer-MSAA
	gPipelineStateDesc.RasterizerState.MultisampleEnable = TRUE;
	gPipelineStateDesc.RasterizerState.AntialiasedLineEnable = TRUE;
	gPipelineStateDesc.RasterizerState.ForcedSampleCount = 0; // sample count of UAV rendering // question 003 : what is UAV rendering?
	//Rasterazer - Conservative Rendering On/Off - (bosujuk rendering)
	gPipelineStateDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	//Pixel Shader
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/OnlyColorShader.hlsl", "PSMain", "ps_5_1", &pd3dPixelShaderBlob);

	//Output Merger
	//Output Merger-Blend
	D3D12_BLEND_DESC d3dBlendDesc;
	::ZeroMemory(&d3dBlendDesc, sizeof(D3D12_BLEND_DESC));
	d3dBlendDesc.AlphaToCoverageEnable = FALSE;
	d3dBlendDesc.IndependentBlendEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	d3dBlendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	d3dBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	gPipelineStateDesc.BlendState = d3dBlendDesc;
	//Output Merger - MSAA
	gPipelineStateDesc.SampleDesc.Count = (gd.m_bMsaa4xEnable) ? 4 : 1;
	gPipelineStateDesc.SampleDesc.Quality = (gd.m_bMsaa4xEnable) ? (gd.m_nMsaa4xQualityLevels - 1) : 0;
	gPipelineStateDesc.SampleMask = 0xFFFFFFFF; // pass every sampling.
	//Output Merger - DepthStencil
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc; // D3D12_DEPTH_STENCIL_DESC1 is using for Stream Pipeline state.. -> what is that?
	//Output Merger - DepthStencil - depth
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	//depthStencilDesc.DepthBoundsTestEnable = FALSE; // question 004 : what is this??
	//Output Merger - DepthStencil - stencil
	depthStencilDesc.StencilEnable = FALSE;
	depthStencilDesc.StencilReadMask = 0x00;
	depthStencilDesc.StencilWriteMask = 0x00;
	depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	depthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	gPipelineStateDesc.DepthStencilState = depthStencilDesc;
	//Output Merger - RenderTagets / DepthStencil Buffer
	gPipelineStateDesc.NumRenderTargets = 1;
	gPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	gd.pDevice->CreateGraphicsPipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState);
	if (pd3dVertexShaderBlob) pd3dVertexShaderBlob->Release();
	if (pd3dPixelShaderBlob) pd3dPixelShaderBlob->Release();
}

void OnlyColorShader::Release()
{
	if (pPipelineState) pPipelineState->Release();
	if (pRootSignature) pRootSignature->Release();
}

DiffuseTextureShader::DiffuseTextureShader()
{
}

DiffuseTextureShader::~DiffuseTextureShader()
{
}

void DiffuseTextureShader::InitShader()
{
	Shader::InitShader();
}

void DiffuseTextureShader::CreateRootSignature()
{
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;

	D3D12_ROOT_PARAMETER1 rootParam[4] = {};

	rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParam[0].Constants.Num32BitValues = 36;
	rootParam[0].Constants.ShaderRegister = 0; // b0 : Camera Matrix (Projection, View) + Camera Positon
	rootParam[0].Constants.RegisterSpace = 0;

	rootParam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParam[1].Constants.Num32BitValues = 16;
	rootParam[1].Constants.ShaderRegister = 1; // b1 : Transform Matrix
	rootParam[1].Constants.RegisterSpace = 0;

	rootParam[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; //Static Light
	rootParam[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParam[2].Descriptor.ShaderRegister = 2; // b2
	rootParam[2].Descriptor.RegisterSpace = 0;

	rootParam[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParam[3].DescriptorTable.NumDescriptorRanges = 1;
	D3D12_DESCRIPTOR_RANGE1 ranges[1];
	ranges[0].BaseShaderRegister = 0; // t0 //Diffuse Texture
	ranges[0].RegisterSpace = 0;
	ranges[0].NumDescriptors = 1;
	ranges[0].OffsetInDescriptorsFromTableStart = 0;
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	rootParam[3].DescriptorTable.pDescriptorRanges = &ranges[0];

	D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT/* |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS*/;

		/// default sampler
	D3D12_STATIC_SAMPLER_DESC sampler = {};
	// sampler register index.
	sampler.ShaderRegister = 0;
	// setting
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.MipLODBias = 0.0f;
	sampler.MaxAnisotropy = 16;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	sampler.MinLOD = -FLT_MAX;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

	rootSigDesc1.NumParameters = 4;
	rootSigDesc1.pParameters = rootParam;
	rootSigDesc1.NumStaticSamplers = 1; // question002 : what is static samplers?
	rootSigDesc1.pStaticSamplers = &sampler;
	rootSigDesc1.Flags = d3dRootSignatureFlags;

	ID3DBlob* pd3dSignatureBlob = NULL;
	ID3DBlob* pd3dErrorBlob = NULL;
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_rootSigDesc;
	versioned_rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	versioned_rootSigDesc.Desc_1_1 = rootSigDesc1;
	D3D12SerializeVersionedRootSignature(&versioned_rootSigDesc, &pd3dSignatureBlob, &pd3dErrorBlob);
	gd.pDevice->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(),
		pd3dSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void
			**)&pRootSignature);
	if (pd3dErrorBlob) pd3dErrorBlob->Release();
	if (pd3dSignatureBlob) pd3dSignatureBlob->Release();
}

void DiffuseTextureShader::CreateRootSignature_withShadow()
{
}

void DiffuseTextureShader::CreatePipelineState()
{
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dVertexShaderBlob = NULL, * pd3dPixelShaderBlob = NULL;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature;

	gPipelineStateDesc.NodeMask = 0;

	//Input Asm
	gPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[4] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // color vec4
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // normal vec3
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0} // uv vec2
	};
	gPipelineStateDesc.InputLayout.NumElements = 4;
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/DiffuseTextureShader.hlsl", "VSMain", "vs_5_1", &pd3dVertexShaderBlob);

	//Hull Shader
	//gPipelineStateDesc.HS = NULLCODE;

	//Tessellation

	//Domain Shader
	//gPipelineStateDesc.DS = NULLCODE;

	//Geometry Shader
	//gPipelineStateDesc.GS = NULLCODE;

	//Stream Output
	//gPipelineStateDesc.StreamOutput

	//Rasterazer
	gPipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	gPipelineStateDesc.RasterizerState.FrontCounterClockwise = FALSE; // front = clock wise
	//Rasterazer-depth
	gPipelineStateDesc.RasterizerState.DepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthBiasClamp = 0;
	gPipelineStateDesc.RasterizerState.SlopeScaledDepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthClipEnable = TRUE; // if depth > 1, cliping vertex.
	//Rasterazer-MSAA
	gPipelineStateDesc.RasterizerState.MultisampleEnable = TRUE;
	gPipelineStateDesc.RasterizerState.AntialiasedLineEnable = TRUE;
	gPipelineStateDesc.RasterizerState.ForcedSampleCount = 0; // sample count of UAV rendering // question 003 : what is UAV rendering?
	//Rasterazer - Conservative Rendering On/Off - (bosujuk rendering)
	gPipelineStateDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	//Pixel Shader
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/DiffuseTextureShader.hlsl", "PSMain", "ps_5_1", &pd3dPixelShaderBlob);

	//Output Merger
	//Output Merger-Blend
	D3D12_BLEND_DESC d3dBlendDesc;
	::ZeroMemory(&d3dBlendDesc, sizeof(D3D12_BLEND_DESC));
	d3dBlendDesc.AlphaToCoverageEnable = FALSE;
	d3dBlendDesc.IndependentBlendEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	d3dBlendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	d3dBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	gPipelineStateDesc.BlendState = d3dBlendDesc;
	//Output Merger - MSAA
	gPipelineStateDesc.SampleDesc.Count = (gd.m_bMsaa4xEnable) ? 4 : 1;
	gPipelineStateDesc.SampleDesc.Quality = (gd.m_bMsaa4xEnable) ? (gd.m_nMsaa4xQualityLevels - 1) : 0;
	gPipelineStateDesc.SampleMask = 0xFFFFFFFF; // pass every sampling.
	//Output Merger - DepthStencil
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc; // D3D12_DEPTH_STENCIL_DESC1 is using for Stream Pipeline state.. -> what is that?
	//Output Merger - DepthStencil - depth
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	//depthStencilDesc.DepthBoundsTestEnable = FALSE; // question 004 : what is this??
	//Output Merger - DepthStencil - stencil
	depthStencilDesc.StencilEnable = FALSE;
	depthStencilDesc.StencilReadMask = 0x00;
	depthStencilDesc.StencilWriteMask = 0x00;
	depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	depthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	gPipelineStateDesc.DepthStencilState = depthStencilDesc;
	//Output Merger - RenderTagets / DepthStencil Buffer
	gPipelineStateDesc.NumRenderTargets = 1;
	gPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	gd.pDevice->CreateGraphicsPipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState);
	if (pd3dVertexShaderBlob) pd3dVertexShaderBlob->Release();
	if (pd3dPixelShaderBlob) pd3dPixelShaderBlob->Release();
}

void DiffuseTextureShader::CreatePipelineState_withShadow()
{
}

void DiffuseTextureShader::CreatePipelineState_RenderShadowMap()
{
}

void DiffuseTextureShader::Release()
{
	if (pPipelineState) pPipelineState->Release();
	if (pRootSignature) pRootSignature->Release();
}

void DiffuseTextureShader::SetTextureCommand(GPUResource* texture)
{
	D3D12_CPU_DESCRIPTOR_HANDLE hcpu;
	D3D12_GPU_DESCRIPTOR_HANDLE hgpu;
	gd.ShaderVisibleDescPool.AllocDescriptorTable(&hcpu, &hgpu, 1);

	gd.pDevice->CopyDescriptorsSimple(1, hcpu, texture->hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	gd.pCommandList->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.m_pDescritorHeap);
	gd.pCommandList->SetGraphicsRootDescriptorTable(3, hgpu);
}

ScreenCharactorShader::ScreenCharactorShader()
{
}

ScreenCharactorShader::~ScreenCharactorShader()
{
}

void ScreenCharactorShader::InitShader()
{
	CreateRootSignature();
	CreatePipelineState();
}

void ScreenCharactorShader::CreateRootSignature()
{
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;

	D3D12_ROOT_PARAMETER1 rootParam[2] = {};

	rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParam[0].Constants.Num32BitValues = 6; // rect(4) + screenWidht, screenHeight
	rootParam[0].Constants.ShaderRegister = 0; // b0
	rootParam[0].Constants.RegisterSpace = 0;

	rootParam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParam[1].DescriptorTable.NumDescriptorRanges = 1;
	D3D12_DESCRIPTOR_RANGE1 ranges[1];
	ranges[0].BaseShaderRegister = 0; // t0 //Diffuse Texture
	ranges[0].RegisterSpace = 0;
	ranges[0].NumDescriptors = 1;
	ranges[0].OffsetInDescriptorsFromTableStart = 0;
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	rootParam[1].DescriptorTable.pDescriptorRanges = &ranges[0];

	D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT/* |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS*/;

		/// default sampler
	D3D12_STATIC_SAMPLER_DESC sampler = {};
	// sampler register index.
	sampler.ShaderRegister = 0;
	// setting
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.MipLODBias = 0.0f;
	sampler.MaxAnisotropy = 16;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	sampler.MinLOD = -FLT_MAX;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

	rootSigDesc1.NumParameters = 2;
	rootSigDesc1.pParameters = rootParam;
	rootSigDesc1.NumStaticSamplers = 1; // question002 : what is static samplers?
	rootSigDesc1.pStaticSamplers = &sampler;
	rootSigDesc1.Flags = d3dRootSignatureFlags;

	ID3DBlob* pd3dSignatureBlob = NULL;
	ID3DBlob* pd3dErrorBlob = NULL;
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_rootSigDesc;
	versioned_rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	versioned_rootSigDesc.Desc_1_1 = rootSigDesc1;
	D3D12SerializeVersionedRootSignature(&versioned_rootSigDesc, &pd3dSignatureBlob, &pd3dErrorBlob);
	gd.pDevice->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(),
		pd3dSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void
			**)&pRootSignature);
	if (pd3dErrorBlob) pd3dErrorBlob->Release();
	if (pd3dSignatureBlob) pd3dSignatureBlob->Release();
}

void ScreenCharactorShader::CreatePipelineState()
{
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dVertexShaderBlob = NULL, * pd3dPixelShaderBlob = NULL;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature;

	gPipelineStateDesc.NodeMask = 0;

	//Input Asm
	gPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[4] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // color vec4
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // normal vec3
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0} // uv vec2
	};
	gPipelineStateDesc.InputLayout.NumElements = 4;
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/ScreenCharactorRenderShader.hlsl", "VSMain", "vs_5_1", &pd3dVertexShaderBlob);

	ID3D12ShaderReflection* pReflection = nullptr;
	D3DReflect(gPipelineStateDesc.VS.pShaderBytecode, gPipelineStateDesc.VS.BytecodeLength, IID_PPV_ARGS(&pReflection));

	D3D12_SHADER_DESC shaderDesc;
	pReflection->GetDesc(&shaderDesc);

	for (UINT i = 0; i < shaderDesc.ConstantBuffers; ++i)
	{
		ID3D12ShaderReflectionConstantBuffer* cb = pReflection->GetConstantBufferByIndex(i);
		D3D12_SHADER_BUFFER_DESC cbDesc;
		cb->GetDesc(&cbDesc);
		std::cout << "CB Name: " << cbDesc.Name << endl;
	}

	//Hull Shader
	//gPipelineStateDesc.HS = NULLCODE;

	//Tessellation

	//Domain Shader
	//gPipelineStateDesc.DS = NULLCODE;

	//Geometry Shader
	//gPipelineStateDesc.GS = NULLCODE;

	//Stream Output
	//gPipelineStateDesc.StreamOutput

	//Rasterazer
	gPipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	gPipelineStateDesc.RasterizerState.FrontCounterClockwise = FALSE; // front = clock wise
	//Rasterazer-depth
	gPipelineStateDesc.RasterizerState.DepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthBiasClamp = 0;
	gPipelineStateDesc.RasterizerState.SlopeScaledDepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthClipEnable = TRUE; // if depth > 1, cliping vertex.
	//Rasterazer-MSAA
	gPipelineStateDesc.RasterizerState.MultisampleEnable = TRUE;
	gPipelineStateDesc.RasterizerState.AntialiasedLineEnable = TRUE;
	gPipelineStateDesc.RasterizerState.ForcedSampleCount = 0; // sample count of UAV rendering // question 003 : what is UAV rendering?
	//Rasterazer - Conservative Rendering On/Off - (bosujuk rendering)
	gPipelineStateDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	//Pixel Shader
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/ScreenCharactorRenderShader.hlsl", "PSMain", "ps_5_1", &pd3dPixelShaderBlob);

	//Output Merger
	//Output Merger-Blend
	D3D12_BLEND_DESC d3dBlendDesc;
	::ZeroMemory(&d3dBlendDesc, sizeof(D3D12_BLEND_DESC));
	d3dBlendDesc.AlphaToCoverageEnable = 0;
	d3dBlendDesc.IndependentBlendEnable = 0;
	d3dBlendDesc.RenderTarget[0] = {
		TRUE, FALSE,
		D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD,
		D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
		D3D12_LOGIC_OP_NOOP,
		D3D12_COLOR_WRITE_ENABLE_ALL,
	};
	gPipelineStateDesc.BlendState = d3dBlendDesc;
	//Output Merger - MSAA
	gPipelineStateDesc.SampleDesc.Count = (gd.m_bMsaa4xEnable) ? 4 : 1;
	gPipelineStateDesc.SampleDesc.Quality = (gd.m_bMsaa4xEnable) ? (gd.m_nMsaa4xQualityLevels - 1) : 0;
	gPipelineStateDesc.SampleMask = 0xFFFFFFFF; // pass every sampling.
	//Output Merger - DepthStencil
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc; // D3D12_DEPTH_STENCIL_DESC1 is using for Stream Pipeline state.. -> what is that?
	//Output Merger - DepthStencil - depth
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	//depthStencilDesc.DepthBoundsTestEnable = FALSE; // question 004 : what is this??
	//Output Merger - DepthStencil - stencil
	depthStencilDesc.StencilEnable = FALSE;
	depthStencilDesc.StencilReadMask = 0x00;
	depthStencilDesc.StencilWriteMask = 0x00;
	depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	depthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	gPipelineStateDesc.DepthStencilState = depthStencilDesc;
	//Output Merger - RenderTagets / DepthStencil Buffer
	gPipelineStateDesc.NumRenderTargets = 1;
	gPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	gd.pDevice->CreateGraphicsPipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState);
	if (pd3dVertexShaderBlob) pd3dVertexShaderBlob->Release();
	if (pd3dPixelShaderBlob) pd3dPixelShaderBlob->Release();
}

void ScreenCharactorShader::Release()
{
	if (pPipelineState) pPipelineState->Release();
	if (pRootSignature) pRootSignature->Release();
}

void ScreenCharactorShader::SetTextureCommand(GPUResource* texture)
{
	D3D12_CPU_DESCRIPTOR_HANDLE hcpu;
	D3D12_GPU_DESCRIPTOR_HANDLE hgpu;
	gd.ShaderVisibleDescPool.AllocDescriptorTable(&hcpu, &hgpu, 1);

	gd.pDevice->CopyDescriptorsSimple(1, hcpu, texture->hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	gd.pCommandList->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.m_pDescritorHeap);
	gd.pCommandList->SetGraphicsRootDescriptorTable(1, hgpu);
}

ShadowMappingShader::ShadowMappingShader()
{
}

ShadowMappingShader::~ShadowMappingShader()
{
}

void ShadowMappingShader::InitShader()
{
	dsvIncrement = gd.pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	CreateRootSignature();
	CreatePipelineState();

	constexpr int MAX_SHADOW_MAP = 256;
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = MAX_SHADOW_MAP;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	gd.pDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&ShadowDescriptorHeap));

	svdp.Initialize(64);
}

void ShadowMappingShader::CreateRootSignature()
{
}

void ShadowMappingShader::CreatePipelineState()
{
}

void ShadowMappingShader::Add_RegisterShaderCommand(ID3D12GraphicsCommandList* commandList)
{
	Shader::Add_RegisterShaderCommand(commandList);
}

void ShadowMappingShader::Release()
{
	Shader::Release();
}

void ShadowMappingShader::RegisterShadowMap(GPUResource* shadowMap)
{
	CurrentShadowMap = shadowMap;
}

GPUResource ShadowMappingShader::CreateShadowMap(int width, int height, int DSVoffset)
{
	GPUResource shadowMap;
	
	D3D12_RESOURCE_DESC shadowTextureDesc;
	shadowTextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	shadowTextureDesc.Alignment = 0;
	shadowTextureDesc.Width = width;
	shadowTextureDesc.Height = height;
	shadowTextureDesc.Format = DXGI_FORMAT_D32_FLOAT;
	shadowTextureDesc.DepthOrArraySize = 1;
	shadowTextureDesc.MipLevels = 1;
	shadowTextureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	shadowTextureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	shadowTextureDesc.SampleDesc.Count = 1;
	shadowTextureDesc.SampleDesc.Quality = 0;

	D3D12_CLEAR_VALUE clearValue;    // Performance tip: Tell the runtime at resource creation the desired clear value.
	clearValue.Format = DXGI_FORMAT_D32_FLOAT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	D3D12_HEAP_PROPERTIES heap_property;
	heap_property.Type = D3D12_HEAP_TYPE_DEFAULT;
	heap_property.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap_property.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heap_property.CreationNodeMask = 1;
	heap_property.VisibleNodeMask = 1;

	gd.pDevice->CreateCommittedResource(
		&heap_property,
		D3D12_HEAP_FLAG_NONE,
		&shadowTextureDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&clearValue,
		IID_PPV_ARGS(&shadowMap.resource));
	shadowMap.CurrentState_InCommandWriteLine = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	
	// Create the depth stencil view.
	D3D12_CPU_DESCRIPTOR_HANDLE hcpu;
	hcpu.ptr = ShadowDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + DSVoffset * dsvIncrement;
	gd.pDevice->CreateDepthStencilView(shadowMap.resource, nullptr, hcpu);
	shadowMap.hCpu = hcpu; // DepthStencilView DescHeap의 CPU HANDLE

	D3D12_CPU_DESCRIPTOR_HANDLE srvCpuH;
	D3D12_GPU_DESCRIPTOR_HANDLE srvGpuH;
	svdp.AllocDescriptorTable(&srvCpuH, &srvGpuH, 1);
	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
	srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
	srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv_desc.Texture2D.MostDetailedMip = 0;
	srv_desc.Texture2D.MipLevels = 1;
	srv_desc.Texture2D.ResourceMinLODClamp = 0;
	srv_desc.Texture2D.PlaneSlice = 0;
	gd.pDevice->CreateShaderResourceView(shadowMap.resource, &srv_desc, srvCpuH);
	shadowMap.hGpu = srvGpuH; // CBV, SRV, UAV DescHeap 의 GPU HANDLE

	return shadowMap;
}

Mesh::Mesh()
{
}

Mesh::~Mesh()
{
}

void Mesh::ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering) {
	std::vector< Vertex > temp_vertices;
	std::vector< TriangleIndex > TrianglePool;
	XMFLOAT3 maxPos = { 0, 0, 0 };
	XMFLOAT3 minPos = { 0, 0, 0 };
	std::ifstream in{ path };

	std::vector< XMFLOAT3 > temp_pos;
	std::vector< XMFLOAT2 > temp_uv;
	std::vector< XMFLOAT3 > temp_normal;
	while (in.eof() == false && (in.is_open() && in.fail() == false)) {
		char rstr[128] = {};
		in >> rstr;
		if (strcmp(rstr, "v") == 0) {
			//좌표
			XMFLOAT3 pos;
			in >> pos.x;
			in >> pos.y;
			in >> pos.z;
			if (maxPos.x < pos.x) maxPos.x = pos.x;
			if (maxPos.y < pos.y) maxPos.y = pos.y;
			if (maxPos.z < pos.z) maxPos.z = pos.z;
			if (minPos.x > pos.x) minPos.x = pos.x;
			if (minPos.y > pos.y) minPos.y = pos.y;
			if (minPos.z > pos.z) minPos.z = pos.z;
			temp_pos.push_back(pos);
		}
		else if (strcmp(rstr, "vt") == 0) {
			// uv 좌표
			XMFLOAT3 uv;
			in >> uv.x;
			in >> uv.y;
			in >> uv.z;
			temp_uv.push_back(XMFLOAT2(uv.x, uv.y));
		}
		else if (strcmp(rstr, "vn") == 0) {
			// 노멀
			XMFLOAT3 normal;
			in >> normal.x;
			in >> normal.y;
			in >> normal.z;
			temp_normal.push_back(normal);
		}
		else if (strcmp(rstr, "f") == 0) {
			std::string vertex1, vertex2, vertex3;
			unsigned int vertexIndex[3], uvIndex[3], normalIndex[3];
			char blank;

			in >> vertexIndex[0];
			in >> blank;
			in >> uvIndex[0];
			in >> blank;
			in >> normalIndex[0];
			temp_vertices.push_back(Vertex(temp_pos[vertexIndex[0]-1], color, temp_normal[normalIndex[0]-1]));

			//in >> blank;
			in >> vertexIndex[1];
			in >> blank;
			in >> uvIndex[1];
			in >> blank;
			in >> normalIndex[1];
			temp_vertices.push_back(Vertex(temp_pos[vertexIndex[1]-1], color, temp_normal[normalIndex[1]-1]));

			//in >> blank;
			in >> vertexIndex[2];
			in >> blank;
			in >> uvIndex[2];
			in >> blank;
			in >> normalIndex[2];
			//in >> blank;
			temp_vertices.push_back(Vertex(temp_pos[vertexIndex[2]-1], color, temp_normal[normalIndex[2]-1]));

			int n = temp_vertices.size() - 1;
			TrianglePool.push_back(TriangleIndex(n - 2, n - 1, n));
		}
	}
	// For each vertex of each triangle

	in.close();

	XMFLOAT3 CenterPos;
	if (centering) {
		CenterPos.x = (maxPos.x + minPos.x) * 0.5f;
		CenterPos.y = (maxPos.y + minPos.y) * 0.5f;
		CenterPos.z = (maxPos.z + minPos.z) * 0.5f;
	}
	else {
		CenterPos.x = 0;
		CenterPos.y = 0;
		CenterPos.z = 0;
	}
	MAXpos.x = 0;
	MAXpos.y = 0;
	MAXpos.z = 0;
	for (int i = 0; i < temp_vertices.size(); ++i) {
		temp_vertices[i].position.x -= CenterPos.x;
		temp_vertices[i].position.y -= CenterPos.y;
		temp_vertices[i].position.z -= CenterPos.z;
		temp_vertices[i].position.x /= 100.0f;
		temp_vertices[i].position.y /= 100.0f;
		temp_vertices[i].position.z /= 100.0f;
		if (fabsf(temp_vertices[i].position.x) > MAXpos.x) MAXpos.x = fabsf(temp_vertices[i].position.x);
		if (fabsf(temp_vertices[i].position.y) > MAXpos.y) MAXpos.y = fabsf(temp_vertices[i].position.y);
		if (fabsf(temp_vertices[i].position.z) > MAXpos.z) MAXpos.z = fabsf(temp_vertices[i].position.z);
	}

	int m_nVertices = temp_vertices.size();
	int m_nStride = sizeof(Vertex);
	//m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// error.. why vertex buffer and index buffer do not input? 
	// maybe.. State Error.
	VertexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, temp_vertices.size() * sizeof(Vertex), 1);
	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, temp_vertices.size() * sizeof(Vertex), 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &temp_vertices[0], &VertexUploadBuffer, &VertexBuffer, true);
	VertexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	VertexBufferView.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
	VertexBufferView.StrideInBytes = m_nStride;
	VertexBufferView.SizeInBytes = m_nStride * m_nVertices;
	
	IndexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(TriangleIndex) * TrianglePool.size(), 1);
	IndexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(TriangleIndex) * TrianglePool.size(), 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &TrianglePool[0], &IndexUploadBuffer, &IndexBuffer, true);
	IndexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_INDEX_BUFFER);
	IndexBufferView.BufferLocation = IndexBuffer.resource->GetGPUVirtualAddress();
	IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	IndexBufferView.SizeInBytes = sizeof(TriangleIndex) * TrianglePool.size();
	
	IndexNum = TrianglePool.size() * 3;
	VertexNum = temp_vertices.size();
	topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	/*MeshOBB = BoundingOrientedBox(XMFLOAT3(0.0f, 0.0f, 0.0f), MAXpos, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));*/
}

void Mesh::Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum)
{
	// question : what is StartSlot parameter??
	ui32 SlotNum = 0;
	pCommandList->IASetVertexBuffers(SlotNum, 1, &VertexBufferView);
	pCommandList->IASetPrimitiveTopology(topology);
	if (IndexBuffer.resource)
	{
		pCommandList->IASetIndexBuffer(&IndexBufferView);
		pCommandList->DrawIndexedInstanced(IndexNum, instanceNum, 0, 0, 0);
	}
	else
	{
		ui32 VertexOffset = 0;
		pCommandList->DrawInstanced(VertexNum, instanceNum, VertexOffset, 0);
	}

}

BoundingOrientedBox Mesh::GetOBB()
{
	return BoundingOrientedBox(XMFLOAT3(0.0f, 0.0f, 0.0f), MAXpos, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
}

void Mesh::CreateGroundMesh(ID3D12GraphicsCommandList* pCommandList)
{
	std::vector<Vertex> vertices;
	vertices.push_back(Vertex(XMFLOAT3(-10.0f, 0.0f, -10.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(10.0f, 0.0f, -10.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(10.0f, 0.0f, 10.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-10.0f, 0.0f, 10.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)));

	std::vector<UINT> indices;
	indices.push_back(0); 
	indices.push_back(1);
	indices.push_back(2);

	indices.push_back(2);
	indices.push_back(3);
	indices.push_back(0);

	int nVertices = vertices.size();
	int nStride = sizeof(Vertex);

	VertexBuffer = gd.CreateCommitedGPUBuffer(pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	gd.UploadToCommitedGPUBuffer(pCommandList, &vertices[0], &VertexUploadBuffer, &VertexBuffer, true);

	VertexBuffer.AddResourceBarrierTransitoinToCommand(pCommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	VertexBufferView.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
	VertexBufferView.StrideInBytes = nStride;
	VertexBufferView.SizeInBytes = nStride * nVertices;

	IndexBuffer = gd.CreateCommitedGPUBuffer(pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, indices.size() * sizeof(UINT), 1);
	IndexUploadBuffer = gd.CreateCommitedGPUBuffer(pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, indices.size() * sizeof(UINT), 1);
	gd.UploadToCommitedGPUBuffer(pCommandList, &indices[0], &IndexUploadBuffer, &IndexBuffer, true);

	IndexBuffer.AddResourceBarrierTransitoinToCommand(pCommandList, D3D12_RESOURCE_STATE_INDEX_BUFFER);

	IndexBufferView.BufferLocation = IndexBuffer.resource->GetGPUVirtualAddress();
	IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	IndexBufferView.SizeInBytes = indices.size() * sizeof(UINT);

	IndexNum = indices.size();
	VertexNum = vertices.size();
	topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

void Mesh::CreateWallMesh(float width, float height, float depth, vec4 color)
{
	std::vector<Vertex> vertices;
	std::vector<UINT> indices;
	// Front
	vertices.push_back(Vertex(XMFLOAT3(-width, -height, -depth), color, XMFLOAT3(0.0f, 0.0f, -1.0f)));
	vertices.push_back(Vertex(XMFLOAT3(width, -height, -depth), color, XMFLOAT3(0.0f, 0.0f, -1.0f)));
	vertices.push_back(Vertex(XMFLOAT3(width, height, -depth), color, XMFLOAT3(0.0f, 0.0f, -1.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-width, height, -depth), color, XMFLOAT3(0.0f, 0.0f, -1.0f)));
	// Back
	vertices.push_back(Vertex(XMFLOAT3(-width, -height, depth), color, XMFLOAT3(0.0f, 0.0f, 1.0f)));
	vertices.push_back(Vertex(XMFLOAT3(width, -height, depth), color, XMFLOAT3(0.0f, 0.0f, 1.0f)));
	vertices.push_back(Vertex(XMFLOAT3(width, height, depth), color, XMFLOAT3(0.0f, 0.0f, 1.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-width, height, depth), color, XMFLOAT3(0.0f, 0.0f, 1.0f)));
	// Top
	vertices.push_back(Vertex(XMFLOAT3(-width, height, -depth), color, XMFLOAT3(0.0f, 1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(width, height, -depth), color, XMFLOAT3(0.0f, 1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(width, height, depth), color, XMFLOAT3(0.0f, 1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-width, height, depth), color, XMFLOAT3(0.0f, 1.0f, 0.0f)));
	// Bottom
	vertices.push_back(Vertex(XMFLOAT3(-width, -height, -depth), color, XMFLOAT3(0.0f, -1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(width, -height, -depth), color, XMFLOAT3(0.0f, -1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(width, -height, depth), color, XMFLOAT3(0.0f, -1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-width, -height, depth), color, XMFLOAT3(0.0f, -1.0f, 0.0f)));
	// Left
	vertices.push_back(Vertex(XMFLOAT3(-width, -height, depth), color, XMFLOAT3(-1.0f, 0.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-width, -height, -depth), color, XMFLOAT3(-1.0f, 0.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-width, height, -depth), color, XMFLOAT3(-1.0f, 0.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-width, height, depth), color, XMFLOAT3(-1.0f, 0.0f, 0.0f)));
	// Right
	vertices.push_back(Vertex(XMFLOAT3(width, -height, -depth), color, XMFLOAT3(1.0f, 0.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(width, -height, depth), color, XMFLOAT3(1.0f, 0.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(width, height, depth), color, XMFLOAT3(1.0f, 0.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(width, height, -depth), color, XMFLOAT3(1.0f, 0.0f, 0.0f)));

	// index
	// front
	indices.push_back(0); indices.push_back(2); indices.push_back(1);
	indices.push_back(2); indices.push_back(0); indices.push_back(3);
	// back
	indices.push_back(4); indices.push_back(5); indices.push_back(6);
	indices.push_back(6); indices.push_back(7); indices.push_back(4);
	// top
	indices.push_back(8); indices.push_back(10); indices.push_back(9);
	indices.push_back(10); indices.push_back(8); indices.push_back(11);
	// bottom
	indices.push_back(12); indices.push_back(13); indices.push_back(14);
	indices.push_back(14); indices.push_back(15); indices.push_back(12);
	// left
	indices.push_back(16); indices.push_back(18); indices.push_back(17);
	indices.push_back(18); indices.push_back(16); indices.push_back(19);
	// right
	indices.push_back(20); indices.push_back(22); indices.push_back(21);
	indices.push_back(22); indices.push_back(20); indices.push_back(23);

	// OBB
	this->MAXpos = XMFLOAT3(width, height, depth);

	int nVertices = vertices.size();
	int nStride = sizeof(Vertex);

	VertexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &vertices[0], &VertexUploadBuffer, &VertexBuffer, true);

	VertexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	VertexBufferView.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
	VertexBufferView.StrideInBytes = nStride;
	VertexBufferView.SizeInBytes = nStride * nVertices;

	IndexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, indices.size() * sizeof(UINT), 1);
	IndexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, indices.size() * sizeof(UINT), 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &indices[0], &IndexUploadBuffer, &IndexBuffer, true);

	IndexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_INDEX_BUFFER);

	IndexBufferView.BufferLocation = IndexBuffer.resource->GetGPUVirtualAddress();
	IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	IndexBufferView.SizeInBytes = indices.size() * sizeof(UINT);

	IndexNum = indices.size();
	VertexNum = vertices.size();
	topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

void Mesh::Release()
{
	VertexBuffer.Release();
	VertexUploadBuffer.Release();
	IndexBuffer.Release();
	IndexUploadBuffer.Release();
}

void UVMesh::ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering)
{
	std::vector< Vertex > temp_vertices;
	std::vector< TriangleIndex > TrianglePool;
	XMFLOAT3 maxPos = { 0, 0, 0 };
	XMFLOAT3 minPos = { 0, 0, 0 };
	std::ifstream in{ path };

	std::vector< XMFLOAT3 > temp_pos;
	std::vector< XMFLOAT2 > temp_uv;
	std::vector< XMFLOAT3 > temp_normal;
	while (in.eof() == false && (in.is_open() && in.fail() == false)) {
		char rstr[128] = {};
		in >> rstr;
		if (strcmp(rstr, "v") == 0) {
			//좌표
			XMFLOAT3 pos;
			in >> pos.x;
			in >> pos.y;
			in >> pos.z;
			if (maxPos.x < pos.x) maxPos.x = pos.x;
			if (maxPos.y < pos.y) maxPos.y = pos.y;
			if (maxPos.z < pos.z) maxPos.z = pos.z;
			if (minPos.x > pos.x) minPos.x = pos.x;
			if (minPos.y > pos.y) minPos.y = pos.y;
			if (minPos.z > pos.z) minPos.z = pos.z;
			temp_pos.push_back(pos);
		}
		else if (strcmp(rstr, "vt") == 0) {
			// uv 좌표
			XMFLOAT3 uv;
			in >> uv.x;
			in >> uv.y;
			in >> uv.z;
			uv.y = 1.0f - uv.y;
			temp_uv.push_back(XMFLOAT2(uv.x, uv.y));
		}
		else if (strcmp(rstr, "vn") == 0) {
			// 노멀
			XMFLOAT3 normal;
			in >> normal.x;
			in >> normal.y;
			in >> normal.z;
			temp_normal.push_back(normal);
		}
		else if (strcmp(rstr, "f") == 0) {
			std::string vertex1, vertex2, vertex3;
			unsigned int vertexIndex[3], uvIndex[3], normalIndex[3];
			char blank;

			in >> vertexIndex[0];
			in >> blank;
			in >> uvIndex[0];
			in >> blank;
			in >> normalIndex[0];
			temp_vertices.push_back(Vertex(temp_pos[vertexIndex[0] - 1], color, temp_normal[normalIndex[0] - 1], temp_uv[uvIndex[0]-1]));

			//in >> blank;
			in >> vertexIndex[1];
			in >> blank;
			in >> uvIndex[1];
			in >> blank;
			in >> normalIndex[1];
			temp_vertices.push_back(Vertex(temp_pos[vertexIndex[1] - 1], color, temp_normal[normalIndex[1] - 1], temp_uv[uvIndex[1] - 1]));

			//in >> blank;
			in >> vertexIndex[2];
			in >> blank;
			in >> uvIndex[2];
			in >> blank;
			in >> normalIndex[2];
			//in >> blank;
			temp_vertices.push_back(Vertex(temp_pos[vertexIndex[2] - 1], color, temp_normal[normalIndex[2] - 1], temp_uv[uvIndex[2] - 1]));

			int n = temp_vertices.size() - 1;
			TrianglePool.push_back(TriangleIndex(n - 2, n - 1, n));
		}
	}
	// For each vertex of each triangle

	in.close();

	XMFLOAT3 CenterPos;
	if (centering) {
		CenterPos.x = (maxPos.x + minPos.x) * 0.5f;
		CenterPos.y = (maxPos.y + minPos.y) * 0.5f;
		CenterPos.z = (maxPos.z + minPos.z) * 0.5f;
	}
	else {
		CenterPos.x = 0;
		CenterPos.y = 0;
		CenterPos.z = 0;
	}
	MAXpos.x = 0;
	MAXpos.y = 0;
	MAXpos.z = 0;
	for (int i = 0; i < temp_vertices.size(); ++i) {
		temp_vertices[i].position.x -= CenterPos.x;
		temp_vertices[i].position.y -= CenterPos.y;
		temp_vertices[i].position.z -= CenterPos.z;
		temp_vertices[i].position.x /= 100.0f;
		temp_vertices[i].position.y /= 100.0f;
		temp_vertices[i].position.z /= 100.0f;
		if (fabsf(temp_vertices[i].position.x) > MAXpos.x) MAXpos.x = fabsf(temp_vertices[i].position.x);
		if (fabsf(temp_vertices[i].position.y) > MAXpos.y) MAXpos.y = fabsf(temp_vertices[i].position.y);
		if (fabsf(temp_vertices[i].position.z) > MAXpos.z) MAXpos.z = fabsf(temp_vertices[i].position.z);
	}

	int m_nVertices = temp_vertices.size();
	int m_nStride = sizeof(Vertex);
	//m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// error.. why vertex buffer and index buffer do not input? 
	// maybe.. State Error.
	VertexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, temp_vertices.size() * sizeof(Vertex), 1);
	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, temp_vertices.size() * sizeof(Vertex), 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &temp_vertices[0], &VertexUploadBuffer, &VertexBuffer, true);
	VertexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	VertexBufferView.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
	VertexBufferView.StrideInBytes = m_nStride;
	VertexBufferView.SizeInBytes = m_nStride * m_nVertices;

	IndexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(TriangleIndex) * TrianglePool.size(), 1);
	IndexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(TriangleIndex) * TrianglePool.size(), 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &TrianglePool[0], &IndexUploadBuffer, &IndexBuffer, true);
	IndexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_INDEX_BUFFER);
	IndexBufferView.BufferLocation = IndexBuffer.resource->GetGPUVirtualAddress();
	IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	IndexBufferView.SizeInBytes = sizeof(TriangleIndex) * TrianglePool.size();

	IndexNum = TrianglePool.size() * 3;
	VertexNum = temp_vertices.size();
	topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	/*MeshOBB = BoundingOrientedBox(XMFLOAT3(0.0f, 0.0f, 0.0f), MAXpos, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));*/
}

void UVMesh::Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum)
{
	Mesh::Render(pCommandList, instanceNum);
}

void UVMesh::Release()
{
	Mesh::Release();
}

void UVMesh::CreateTextRectMesh()
{
	std::vector<Vertex> vertices;
	std::vector<UINT> indices;

	// Front
	vec4 color = { 1, 1, 1, 1 };
	vertices.push_back(Vertex(XMFLOAT3(0, 0, 0), color, XMFLOAT3(0, 0, 0), XMFLOAT2(0.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(1, 0, 0), color, XMFLOAT3(0, 0, 0), XMFLOAT2(1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(1, 1, 0), color, XMFLOAT3(0, 0, 0), XMFLOAT2(1.0f, 1.0f)));
	vertices.push_back(Vertex(XMFLOAT3(0, 1, 0), color, XMFLOAT3(0, 0, 0), XMFLOAT2(0.0f, 1.0f)));

	// front
	indices.push_back(0); indices.push_back(2); indices.push_back(1);
	indices.push_back(2); indices.push_back(0); indices.push_back(3);

	// OBB
	this->MAXpos = XMFLOAT3(1, 1, 0);

	int nVertices = vertices.size();
	int nStride = sizeof(Vertex);

	VertexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &vertices[0], &VertexUploadBuffer, &VertexBuffer, true);

	VertexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	VertexBufferView.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
	VertexBufferView.StrideInBytes = nStride;
	VertexBufferView.SizeInBytes = nStride * nVertices;

	IndexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, indices.size() * sizeof(UINT), 1);
	IndexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, indices.size() * sizeof(UINT), 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &indices[0], &IndexUploadBuffer, &IndexBuffer, true);

	IndexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_INDEX_BUFFER);

	IndexBufferView.BufferLocation = IndexBuffer.resource->GetGPUVirtualAddress();
	IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	IndexBufferView.SizeInBytes = indices.size() * sizeof(UINT);

	IndexNum = indices.size();
	VertexNum = vertices.size();
	topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

void GPUResource::AddResourceBarrierTransitoinToCommand(ID3D12GraphicsCommandList* cmd, D3D12_RESOURCE_STATES afterState)
{
	if (CurrentState_InCommandWriteLine != afterState) {
		D3D12_RESOURCE_BARRIER barrier = GetResourceBarrierTransition(afterState);
		cmd->ResourceBarrier(1, &barrier);
		CurrentState_InCommandWriteLine = afterState;
	}
}

D3D12_RESOURCE_BARRIER GPUResource::GetResourceBarrierTransition(D3D12_RESOURCE_STATES afterState)
{
	D3D12_RESOURCE_BARRIER d3dResourceBarrier;
	::ZeroMemory(&d3dResourceBarrier, sizeof(D3D12_RESOURCE_BARRIER));
	d3dResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	d3dResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	d3dResourceBarrier.Transition.pResource = resource;
	d3dResourceBarrier.Transition.StateBefore = CurrentState_InCommandWriteLine;
	d3dResourceBarrier.Transition.StateAfter = afterState;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	return d3dResourceBarrier;
}

BOOL GPUResource::CreateTexture_fromImageBuffer(UINT Width, UINT Height, const BYTE* pInitImage)
{
	if (resource != nullptr) return FALSE;

	GPUResource uploadBuffer;

	//D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER - ??
	*this = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_DIMENSION_TEXTURE2D, Width, Height, DXGI_FORMAT_R8G8B8A8_UNORM);

	if (pInitImage)
	{
		D3D12_RESOURCE_DESC Desc = resource->GetDesc();
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint;
		UINT	Rows = 0;
		UINT64	RowSize = 0;
		UINT64	TotalBytes = 0;

		gd.pDevice->GetCopyableFootprints(&Desc, 0, 1, 0, &Footprint, &Rows, &RowSize, &TotalBytes);

		BYTE* pMappedPtr = nullptr;
		D3D12_RANGE writeRange;
		writeRange.Begin = 0;
		writeRange.End = 0;

		//return size of buffer for uploading data. (D3dx12.h)
		UINT64 uploadBufferSize = gd.GetRequiredIntermediateSize(resource, 0, 1);
		uploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, uploadBufferSize, 1, DXGI_FORMAT_UNKNOWN);

		HRESULT hr = uploadBuffer.resource->Map(0, &writeRange, reinterpret_cast<void**>(&pMappedPtr));
		if (FAILED(hr))
			return FALSE;

		const BYTE* pSrc = pInitImage;
		BYTE* pDest = pMappedPtr;
		for (UINT y = 0; y < Height; y++)
		{
			memcpy(pDest, pSrc, Width * 4);
			pSrc += (Width * 4);
			pDest += Footprint.Footprint.RowPitch;
		}
		// Unmap
		uploadBuffer.resource->Unmap(0, nullptr);

		UpdateTextureForWrite(uploadBuffer.resource);

		uploadBuffer.resource->Release();
		uploadBuffer.resource = nullptr;
	}

	// success.
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MipLevels = 1;
	
	pGPU = resource->GetGPUVirtualAddress();
	int srvIndex = gd.TextureDescriptorAllotter.Alloc();
	if (srvIndex >= 0)
	{
		hCpu = gd.TextureDescriptorAllotter.GetCPUHandle(srvIndex);
		hGpu = gd.TextureDescriptorAllotter.GetGPUHandle(srvIndex);
		gd.pDevice->CreateShaderResourceView(resource, &SRVDesc, hCpu);
	}
	else
	{
		resource->Release();
		resource = nullptr;
		return FALSE;
	}

	AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	return TRUE;
}

void GPUResource::CreateTexture_fromFile(const char* filename)
{
	int TexWidth, TexHeight, nrChannels;
	BYTE* pImageData = stbi_load(filename, &TexWidth, &TexHeight, &nrChannels, 0);
	BOOL b = CreateTexture_fromImageBuffer(TexWidth, TexHeight, pImageData);
	stbi_image_free(pImageData);
}

void GPUResource::UpdateTextureForWrite(ID3D12Resource* pSrcTexResource)
{
	const DWORD MAX_SUB_RESOURCE_NUM = 32;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint[MAX_SUB_RESOURCE_NUM] = {};
	UINT Rows[MAX_SUB_RESOURCE_NUM] = {};
	UINT64 RowSize[MAX_SUB_RESOURCE_NUM] = {};
	UINT64 TotalBytes = 0;

	D3D12_RESOURCE_DESC Desc = resource->GetDesc();
	if (Desc.MipLevels > (UINT)_countof(Footprint))
		__debugbreak();

	gd.pDevice->GetCopyableFootprints(&Desc, 0, Desc.MipLevels, 0, Footprint, Rows,
		RowSize, &TotalBytes);

	if (FAILED(gd.pCommandAllocator->Reset()))
		__debugbreak();

	if (FAILED(gd.pCommandList->Reset(gd.pCommandAllocator, nullptr)))
		__debugbreak();


	if (CurrentState_InCommandWriteLine != D3D12_RESOURCE_STATE_COPY_DEST) {
		this->AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_COPY_DEST);
	}

	for (DWORD i = 0; i < Desc.MipLevels; ++i) {
		D3D12_TEXTURE_COPY_LOCATION destLocation = {};
		destLocation.PlacedFootprint = Footprint[i];
		destLocation.pResource = resource;
		destLocation.SubresourceIndex = i;
		destLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

		D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
		srcLocation.PlacedFootprint = Footprint[i];
		srcLocation.pResource = pSrcTexResource;
		srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

		gd.pCommandList->CopyTextureRegion(&destLocation, 0, 0, 0, &srcLocation, nullptr);
	}

	this->AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	gd.pCommandList->Close();

	ID3D12CommandList* ppCommandLists[] = { gd.pCommandList };
	gd.pCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	gd.WaitGPUComplete();
}

void GPUResource::Release()
{
	if (resource) {
		resource->Release();
	}
	CurrentState_InCommandWriteLine = D3D12_RESOURCE_STATE_COMMON;
	extraData = 0;
	pGPU = 0;
	hCpu.ptr = 0;
	hGpu.ptr = 0;
}

GameObject::GameObject()
{
	tickLVelocity = vec4(0, 0, 0, 0);
	tickAVelocity = vec4(0, 0, 0, 0);
	LastQuerternion = vec4(0, 0, 0, 0);
}

GameObject::~GameObject()
{
}

void GameObject::Update(float delatTime)
{
}

void GameObject::Render()
{
	if (!m_pMesh || !m_pShader) {
		return;
	}

	XMFLOAT4X4 xmf4x4World;
	XMStoreFloat4x4(&xmf4x4World, DirectX::XMMatrixTranspose(m_worldMatrix));

	gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &xmf4x4World, 0);

	if (m_pTexture) {
		gd.pCommandList->SetGraphicsRootDescriptorTable(3, m_pTexture->hGpu);
	}

	m_pMesh->Render(gd.pCommandList, 1);
}

void GameObject::Event(WinEvent evt)
{
}

void GameObject::Release()
{
	m_pMesh = nullptr;
	m_pShader = nullptr;
	m_pTexture = nullptr;
}

void GameObject::SetMesh(Mesh* pMesh)
{
	m_pMesh = pMesh;
}

void GameObject::SetShader(Shader* pShader)
{
	m_pShader = pShader;
}

void GameObject::SetWorldMatrix(const XMMATRIX& worldMatrix)
{
	m_worldMatrix = worldMatrix;
}

const XMMATRIX& GameObject::GetWorldMatrix() const
{
	return m_worldMatrix;
}

BoundingOrientedBox GameObject::GetOBB()
{
	BoundingOrientedBox obb_local = m_pMesh->GetOBB();
	BoundingOrientedBox obb_world;
	obb_local.Transform(obb_world, m_worldMatrix);
	return obb_world;
}

OBB_vertexVector GameObject::GetOBBVertexs()
{
	OBB_vertexVector ovv;
	BoundingOrientedBox obb = GetOBB();
	vec4 xm = -obb.Extents.x * m_worldMatrix.right;
	vec4 xp = obb.Extents.x * m_worldMatrix.right;
	vec4 ym = -obb.Extents.y * m_worldMatrix.up;
	vec4 yp = obb.Extents.y * m_worldMatrix.up;
	vec4 zm = -obb.Extents.z * m_worldMatrix.look;
	vec4 zp = obb.Extents.z * m_worldMatrix.look;

	ovv.vertex[0][0][0] = xm + ym + zm + m_worldMatrix.pos;
	ovv.vertex[0][0][1] = xm + ym + zp + m_worldMatrix.pos;
	ovv.vertex[0][1][0] = xm + yp + zm + m_worldMatrix.pos;
	ovv.vertex[0][1][1] = xm + yp + zp + m_worldMatrix.pos;
	ovv.vertex[1][0][0] = xp + ym + zm + m_worldMatrix.pos;
	ovv.vertex[1][0][1] = xp + ym + zp + m_worldMatrix.pos;
	ovv.vertex[1][1][0] = xp + yp + zm + m_worldMatrix.pos;
	ovv.vertex[1][1][1] = xp + yp + zp + m_worldMatrix.pos;
	return ovv;
}

void GameObject::LookAt(vec4 look, vec4 up)
{
	m_worldMatrix.LookAt(look, up);
}

void GameObject::CollisionMove(GameObject* gbj1, GameObject* gbj2)
{
	constexpr float epsillon = 0.01f;

	bool bi = XMColorEqual(gbj1->tickLVelocity, XMVectorZero());/*
	bool bia = XMColorEqual(gbj1->tickAVelocity, XMVectorZero());*/
	bool bj = XMColorEqual(gbj2->tickLVelocity, XMVectorZero());/*
	bool bja = XMColorEqual(gbj2->tickAVelocity, XMVectorZero());*/

	GameObject* movObj = nullptr;
	GameObject* colObj = nullptr;
	BoundingOrientedBox obb1 = gbj1->GetOBB();
	BoundingOrientedBox obb2 = gbj2->GetOBB();
	//float len = vec4(gbj1->m_worldMatrix.pos - gbj2->m_worldMatrix.pos).len3;
	//float distance = vec4(obb1.Extents).len3 + vec4(obb2.Extents).len3;
	//if (len < distance) {
	//Collision_By_Rotations:
	//	if (!bia && bja) {
	//		movObj = gbj1;
	//		colObj = gbj2;
	//		goto Collision_byRotation_static_vs_dynamic;
	//	}
	//	else if (bia && !bja) {
	//		movObj = gbj2;
	//		colObj = gbj1;
	//		goto Collision_byRotation_static_vs_dynamic;
	//	}
	//	else if (!bia && !bja) {
	//		goto Collision_By_Move;
	//	}
	//	else {
	//		goto Collision_By_Move;
	//	}
	//Collision_byRotation_static_vs_dynamic:
	//	OBB_vertexVector ovv = movObj->GetOBBVertexs();
	//	movObj->m_worldMatrix.trQ(movObj->tickAVelocity);
	//	obb1 = movObj->GetOBB();
	//	OBB_vertexVector ovv_later = movObj->GetOBBVertexs();
	//	movObj->m_worldMatrix.trQinv(movObj->tickAVelocity);
	//	matrix imat = colObj->m_worldMatrix.RTInverse;
	//	obb2 = colObj->GetOBB();
	//	vec4 RayPos;
	//	vec4 RayDir;
	//	for (int xi = 0; xi < 2; ++xi) {
	//		for (int yi = 0; yi < 2; ++yi) {
	//			for (int zi = 0; zi < 2; ++zi) {
	//				ovv_later.vertex[xi][yi][zi] *= imat;
	//				if (obb2.Contains(ovv_later.vertex[xi][yi][zi])) {
	//					ovv.vertex[xi][yi][zi] *= imat;
	//					RayPos = ovv.vertex[xi][yi][zi];
	//					RayDir = ovv_later.vertex[xi][yi][zi] - ovv.vertex[xi][yi][zi];
	//					if (fabsf(RayDir.x) > epsillon) {
	//						float Ex = obb2.Extents.x * (RayDir.x / fabsf(RayDir.x));
	//						float A = (Ex - RayPos.x) / RayDir.x;
	//						vec4 colpos = vec4(RayPos.x + RayDir.x, RayDir.y * A + RayPos.y, RayDir.z * A + RayPos.z);
	//						vec4 bound; bound.f3 = obb2.Extents;
	//						if (colpos.is_in_bound(bound)) {
	//							movObj->tickLVelocity.x += colpos.x - Ex;
	//							goto Collision_By_Move;
	//						}
	//					}
	//					if (fabsf(RayDir.y) > epsillon) {
	//						float Ex = obb2.Extents.y * (RayDir.y / fabsf(RayDir.y));
	//						float A = (Ex - RayPos.y) / RayDir.y;
	//						vec4 colpos = vec4(RayDir.x * A + RayPos.x, RayPos.y + RayDir.y, RayDir.z * A + RayPos.z);
	//						vec4 bound; bound.f3 = obb2.Extents;
	//						if (colpos.is_in_bound(bound)) {
	//							movObj->tickLVelocity.y += colpos.y - Ex;
	//							goto Collision_By_Move;
	//						}
	//					}
	//					if (fabsf(RayDir.z) > epsillon) {
	//						float Ex = obb2.Extents.z * (RayDir.z / fabsf(RayDir.z));
	//						float A = (Ex - RayPos.z) / RayDir.z;
	//						vec4 colpos = vec4(RayDir.x * A + RayPos.x, RayDir.y * A + RayPos.y, RayPos.z + RayDir.z);
	//						vec4 bound; bound.f3 = obb2.Extents;
	//						if (colpos.is_in_bound(bound)) {
	//							movObj->tickLVelocity.z += colpos.z - Ex;
	//							goto Collision_By_Move;
	//						}
	//					}
	//				}
	//			}
	//		}
	//	}
	//}

Collision_By_Move:
	//if (bia) {
	//	gbj1->m_worldMatrix.trQ(gbj1->tickAVelocity);
	//}
	//if (bja) {
	//	gbj2->m_worldMatrix.trQ(gbj2->tickAVelocity);
	//}

	if (!bi && bj) {
		// i : moving GameObject
		// j : Collision Check GameObject
		movObj = gbj1;
		colObj = gbj2;
		goto Collision_By_Move_static_vs_dynamic;
	}
	else if (bi && !bj) {
		// i : Collision Check GameObject
		// j : moving GameObject
		movObj = gbj2;
		colObj = gbj1;
		goto Collision_By_Move_static_vs_dynamic;
	}
	else if (!bi && !bj) {
		// i : moving GameObject
		// j : moving GameObject

		float mul = 1.0f;
		float rate = 0.5f;
		float maxLen = XMVector3Length(gbj1->tickLVelocity).m128_f32[0];
		float temp = XMVector3Length(gbj2->tickLVelocity).m128_f32[0];
		if (maxLen < temp) maxLen = temp;

	CMP_INTERSECT:
		XMVECTOR v1 = mul * gbj1->tickLVelocity;
		XMVECTOR v2 = mul * gbj2->tickLVelocity;
		gbj1->m_worldMatrix.pos += v1;
		obb1 = gbj1->GetOBB();
		gbj1->m_worldMatrix.pos -= v1;
		gbj2->m_worldMatrix.pos += v2;
		obb2 = gbj2->GetOBB();
		gbj2->m_worldMatrix.pos -= v2;
		if (obb1.Intersects(obb2)) {
			mul -= rate;
			rate *= 0.5f;
			if (maxLen * rate > epsillon) goto CMP_INTERSECT;
		}
		else {
			mul += rate;
			rate *= 0.5f;
			if (maxLen * rate > epsillon) goto CMP_INTERSECT;
		}

		gbj1->tickLVelocity = v1;
		gbj2->tickLVelocity = v2;

		return;
	}
	else {
		//no move
		return;
	}

Collision_By_Move_static_vs_dynamic:
	movObj->m_worldMatrix.pos += movObj->tickLVelocity;
	obb1 = movObj->GetOBB();
	movObj->m_worldMatrix.pos -= movObj->tickLVelocity;
	obb2 = colObj->GetOBB();

	if (obb1.Intersects(obb2)) {
		movObj->OnCollision(colObj);
		colObj->OnCollision(movObj);

		XMMATRIX basemat = colObj->m_worldMatrix;
		XMMATRIX invmat = colObj->m_worldMatrix.RTInverse;
		invmat.r[3] = XMVectorSet(0, 0, 0, 1);
		XMVECTOR BaseLine = XMVector3Transform(movObj->tickLVelocity, invmat);
		movObj->tickLVelocity = XMVectorZero();

		XMVECTOR MoveVector = basemat.r[0] * BaseLine.m128_f32[0];
		movObj->tickLVelocity += MoveVector;
		movObj->m_worldMatrix.pos += movObj->tickLVelocity;
		obb1 = movObj->GetOBB();
		movObj->m_worldMatrix.pos -= movObj->tickLVelocity;
		if (obb1.Intersects(obb2)) {
			movObj->tickLVelocity -= MoveVector;
		}

		MoveVector = basemat.r[1] * BaseLine.m128_f32[1];
		movObj->tickLVelocity += MoveVector;
		movObj->m_worldMatrix.pos += movObj->tickLVelocity;
		obb1 = movObj->GetOBB();
		movObj->m_worldMatrix.pos -= movObj->tickLVelocity;
		if (obb1.Intersects(obb2)) {
			movObj->tickLVelocity -= MoveVector;
		}

		MoveVector = basemat.r[2] * BaseLine.m128_f32[2];
		movObj->tickLVelocity += MoveVector;
		movObj->m_worldMatrix.pos += movObj->tickLVelocity;
		obb1 = movObj->GetOBB();
		movObj->m_worldMatrix.pos -= movObj->tickLVelocity;
		if (obb1.Intersects(obb2)) {
			movObj->tickLVelocity -= MoveVector;
		}
	}

	//if (bia) {
	//	gbj1->m_worldMatrix.trQinv(gbj1->tickAVelocity);
	//}
	//if (bja) {
	//	gbj2->m_worldMatrix.trQinv(gbj2->tickAVelocity);
	//}
}

void GameObject::OnCollision(GameObject* other)
{
	//GameObject Collision...
}

void Player::Update(float deltaTime)
{
	if (m_worldMatrix.pos.fast_square_of_len3 > 10000) {
		m_worldMatrix.pos = vec4(0, 10, 0, 1);
		LVelocity = 0;
	}

	ShootFlow += deltaTime;
	if (ShootFlow >= ShootDelay) ShootFlow = ShootDelay;
	float shootrate = powf(ShootFlow / ShootDelay, 5);
	gunMatrix_firstPersonView.pos.z = 0.3f + 0.2f * shootrate;
	constexpr float RotHeight = ShootDelay * 10;
	gunMatrix_firstPersonView.LookAt(vec4(0, RotHeight - RotHeight * shootrate, 5) - gunMatrix_firstPersonView.pos);

	recoilFlow += deltaTime;
	if (recoilFlow < recoilDelay) {
		float power = 5;
		float delta_rate = (power / recoilDelay) * pow(1 - recoilFlow / recoilDelay, (power - 1));
		float f = recoilVelocity * deltaTime * delta_rate;
		game.DeltaMousePos.y -= f;
	}

	if(collideCount == 0) isGround = false;
	collideCount = 0;

	XMFLOAT3 xmf3Shift = XMFLOAT3(0, 0, 0);
	constexpr float speed = 3.0f;

	LVelocity.y -= 9.81f * deltaTime;
	if (isGround) {
		if (m_pKeyBuffer[VK_SPACE] & 0xF0) {
			LVelocity.y = JumpVelocity;
			isGround = false;
		}
	}
	tickLVelocity = LVelocity * deltaTime;

	if (m_pKeyBuffer['W'] & 0xF0) {
		tickLVelocity += speed * m_worldMatrix.look * game.DeltaTime;
	}
	if (m_pKeyBuffer['S'] & 0xF0) {
		tickLVelocity -= speed * m_worldMatrix.look * game.DeltaTime;
	}
	if (m_pKeyBuffer['A'] & 0xF0) {
		tickLVelocity -= speed * m_worldMatrix.right * game.DeltaTime;
	}
	if (m_pKeyBuffer['D'] & 0xF0) {
		tickLVelocity += speed * m_worldMatrix.right * game.DeltaTime;
	}
}

void Player::Render()
{
	if (game.bFirstPersonVision == false) {
		GameObject::Render();

		matrix gunmat = gunMatrix_thirdPersonView;
		gunmat *= m_worldMatrix;
		gunmat.LookAt(m_worldMatrix.look);
		gunmat.transpose();

		gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &gunmat, 0);
		Gun->Render(gd.pCommandList, 1);
	}
}

//float gunrot = 0;
void Player::Render_AfterDepthClear()
{
	matrix viewmat = gd.viewportArr[0].ViewMatrix.RTInverse;
	if (game.bFirstPersonVision)
	{
		((Shader*)game.MyDiffuseTextureShader)->Add_RegisterShaderCommand(gd.pCommandList);
		
		matrix gunmat = gunMatrix_firstPersonView;
		gunmat *= viewmat;
		gunmat.transpose();

		/*matrix gunmat;
		gunmat.trQ(vec4::getQ(vec4(0, gunrot, 0, 0)));
		gunmat.pos.y = 1.5f;
		gunrot += game.DeltaTime;
		gunmat.transpose();*/

		gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &gunmat, 0);

		game.MyDiffuseTextureShader->SetTextureCommand(&game.TestTexture);
		/*D3D12_CPU_DESCRIPTOR_HANDLE hcpu;
		D3D12_GPU_DESCRIPTOR_HANDLE hgpu;
		gd.ShaderVisibleDescPool.AllocDescriptorTable(&hcpu, &hgpu, 1);
		
		gd.pDevice->CopyDescriptorsSimple(1, hcpu, game.TestTexture.hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		gd.pCommandList->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.m_pDescritorHeap);
		gd.pCommandList->SetGraphicsRootDescriptorTable(3, hgpu);*/
		Gun->Render(gd.pCommandList, 1);
	}

	Shader* shader = (Shader*)game.MyOnlyColorShader;
	shader->Add_RegisterShaderCommand(gd.pCommandList);

	matrix spmat = viewmat;
	spmat.pos += spmat.look * 10;
	spmat.transpose();
	gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &spmat, 0);
	ShootPointMesh->Render(gd.pCommandList, 1);

	HP = (ShootFlow/ ShootDelay) * MaxHP;
	matrix hpmat;
	hpmat.pos.x = -6;
	hpmat.pos.y = 3;
	hpmat.pos.z = 6;
	hpmat.LookAt(vec4(1, 0, 0));
	hpmat.look *= 2 * HP / MaxHP;
	hpmat *= viewmat;
	hpmat.transpose();
	gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &hpmat, 0);
	HPBarMesh->Render(gd.pCommandList, 1);
}

void Player::Event(WinEvent evt)
{
	if (ShootFlow >= ShootDelay) {
		if (evt.Message == WM_LBUTTONDOWN) {
			if (game.bFirstPersonVision) {
				matrix shootmat = gd.viewportArr[0].ViewMatrix.RTInverse;
				game.FireRaycast(shootmat.pos + shootmat.look, shootmat.look, 50.0f);
			}
			else {
				matrix shootmat = gd.viewportArr[0].ViewMatrix.RTInverse;
				game.FireRaycast(shootmat.pos + shootmat.look * 3, shootmat.look, 50.0f);
			}
			ShootFlow = 0;
			recoilFlow = 0;
		}
	}
}

void Player::OnCollision(GameObject* other)
{
	collideCount += 1;
	float belowDist = 0;
	BoundingOrientedBox otherOBB = other->GetOBB();
	bool belowhit = otherOBB.Intersects(m_worldMatrix.pos, vec4(0, -1, 0, 0), belowDist);
	if (belowhit && belowDist < m_pMesh->GetOBB().Extents.y + 0.4f) {
		LVelocity.y = 0;
		isGround = true;
	}
}

void Player::Release()
{
	Gun = nullptr;
	ShootPointMesh = nullptr;
	HPBarMesh = nullptr;
	m_pKeyBuffer = nullptr;
}

BoundingOrientedBox Player::GetOBB()
{
	BoundingOrientedBox obb_local = m_pMesh->GetOBB();
	obb_local.Extents.x = obb_local.Extents.z;
	BoundingOrientedBox obb_world;
	matrix id = XMMatrixIdentity();
	id.pos = m_worldMatrix.pos;
	obb_local.Transform(obb_world, id);
	return obb_world;
}

BulletRay::BulletRay()
{
}

BulletRay::BulletRay(vec4 s, vec4 dir, float dist) {
	start = s;
	direction = dir;
	distance = dist;
	t = 0;
}

matrix BulletRay::GetRayMatrix(float distance) {
	matrix mat;
	matrix smat;
	mat.LookAt(direction);
	float rate = t / remainTime;
	mat.right *= 1-rate;
	mat.up *= 1-rate;
	mat.look *= distance;
	mat.pos = start;
	return mat;
}

void BulletRay::Update() {
	if (direction.fast_square_of_len3 > 0) {
		t += game.DeltaTime;
		if (t > remainTime) {
			direction = 0;
		}
	}
}

void BulletRay::Render() {
	if (direction.fast_square_of_len3 > 0) {
		matrix mat = GetRayMatrix(distance);
		mat.transpose();
		gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &mat, 0);
		mesh->Render(gd.pCommandList, 1);
	}
}

void DescriptorAllotter::Init(D3D12_DESCRIPTOR_HEAP_TYPE heapType, D3D12_DESCRIPTOR_HEAP_FLAGS Flags, int Capacity)
{
	D3D12_DESCRIPTOR_HEAP_DESC commonHeapDesc = {};
	commonHeapDesc.NumDescriptors = Capacity;
	commonHeapDesc.Type = heapType;
	commonHeapDesc.Flags = Flags;

	if (FAILED(gd.pDevice->CreateDescriptorHeap(&commonHeapDesc, IID_PPV_ARGS(&pDescHeap))))
	{
		__debugbreak();
	}

	DescriptorSize = gd.pDevice->GetDescriptorHandleIncrementSize(heapType);

	AllocFlagContainer.Init(Capacity);
}

int DescriptorAllotter::Alloc()
{
	int dwIndex = AllocFlagContainer.Alloc();
	return dwIndex;
}

void DescriptorAllotter::Free(int index)
{
	AllocFlagContainer.Free(index);
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorAllotter::GetGPUHandle(int index)
{
	if (pDescHeap->GetDesc().Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) {
		return D3D12_GPU_DESCRIPTOR_HANDLE(pDescHeap->GetGPUDescriptorHandleForHeapStart().ptr + index * DescriptorSize);
	}
	else {
		D3D12_GPU_DESCRIPTOR_HANDLE handle;
		handle.ptr = 0;
		return handle;
	}
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllotter::GetCPUHandle(int index)
{
	return D3D12_CPU_DESCRIPTOR_HANDLE(pDescHeap->GetCPUDescriptorHandleForHeapStart().ptr + index * DescriptorSize);
}

void DescriptorAllotter::Release()
{
	if (pDescHeap) pDescHeap->Release();
	AllocFlagContainer.Release();
}

void ShaderVisibleDescriptorPool::Release()
{
	if (m_pDescritorHeap)
	{
		m_pDescritorHeap->Release();
		m_pDescritorHeap = nullptr;
	}
}

BOOL ShaderVisibleDescriptorPool::Initialize(UINT MaxDescriptorCount)
{
	BOOL bResult = FALSE;
	m_MaxDescriptorCount = MaxDescriptorCount;
	m_srvDescriptorSize = gd.pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// create descriptor heap
	D3D12_DESCRIPTOR_HEAP_DESC commonHeapDesc = {};
	commonHeapDesc.NumDescriptors = m_MaxDescriptorCount;
	commonHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	commonHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	if (FAILED(gd.pDevice->CreateDescriptorHeap(&commonHeapDesc, IID_PPV_ARGS(&m_pDescritorHeap))))
	{
		__debugbreak();
		goto lb_return;
	}
	m_cpuDescriptorHandle = m_pDescritorHeap->GetCPUDescriptorHandleForHeapStart();
	m_gpuDescriptorHandle = m_pDescritorHeap->GetGPUDescriptorHandleForHeapStart();
	bResult = TRUE;
lb_return:
	return bResult;
}

BOOL ShaderVisibleDescriptorPool::AllocDescriptorTable(D3D12_CPU_DESCRIPTOR_HANDLE* pOutCPUDescriptor, D3D12_GPU_DESCRIPTOR_HANDLE* pOutGPUDescriptor, UINT DescriptorCount)
{
	BOOL bResult = FALSE;
	if (m_AllocatedDescriptorCount + DescriptorCount > m_MaxDescriptorCount)
	{
		return bResult;
	}
	UINT offset = m_AllocatedDescriptorCount + DescriptorCount;

	pOutCPUDescriptor->ptr = m_cpuDescriptorHandle.ptr + m_AllocatedDescriptorCount * m_srvDescriptorSize;
	pOutGPUDescriptor->ptr = m_gpuDescriptorHandle.ptr + m_AllocatedDescriptorCount * m_srvDescriptorSize;
	m_AllocatedDescriptorCount += DescriptorCount;
	bResult = TRUE;
	return bResult;
}

void ShaderVisibleDescriptorPool::Reset()
{
	m_AllocatedDescriptorCount = 0;
}
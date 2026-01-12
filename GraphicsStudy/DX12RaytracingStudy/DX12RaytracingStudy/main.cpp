#include "main.h"

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

	//global device init.
	try {
		gd.Init();
	}
	catch (string err) {
		MessageBoxA(NULL, err.c_str(), "예외 발생", MB_OK | MB_ICONERROR);
		dbglog1a("exeception : %s \n", err.c_str());
	}
	//game.bmpTodds(3, "BC3_UNORM", "D3DTexConv\\texture12.bmp");
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
	HRESULT hr;
#ifdef PIX_DEBUGING
	LoadLibrary(L"C:/Program Files/Microsoft PIX/2509.25/WinPixGpuCapturer.dll");
#endif

	//Font Loading
	font_filename[0] = "consola.ttf"; // english
	font_filename[1] = "malgunbd.ttf"; // korean
	for (int i = 0; i < FontCount; ++i) {
		uint8_t condition_variable = 0;
		int8_t error = TTFFontParser::parse_file(font_filename[i].c_str(), &font_data[i], &font_parsed, &condition_variable);
	}
	addTextureStack.reserve(32);

	UINT nDXGIFactoryFlags = 0;
#if defined(_DEBUG)
	// Debug Layer On
	ID3D12Debug* pd3dDebugController = NULL;
	hr = D3D12GetDebugInterface(__uuidof(ID3D12Debug), (void
		**)&pd3dDebugController);
	if (pd3dDebugController)
	{
		pd3dDebugController->EnableDebugLayer();
		pd3dDebugController->Release();
	}
	else
	{
		throw "WARNING: Direct3D Debug Device is not available";
	}

	// Debug Interface
	IDXGIInfoQueue* DebugInterface;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&DebugInterface)))) {
		debugDXGI = true;

		hr = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&pFactory)); // quest Factory를 디버깅을 하면 뭐가 좋은것인가?
		if (SUCCEEDED(hr)) {
			DebugInterface->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
			DebugInterface->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
		}
		else {
			throw "DXGI_CREATE_FACTORY_DEBUG CreateDXGIFactory2 Fail.";
		}
	}
	nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

	if (!debugDXGI)
	{
		hr = CreateDXGIFactory2(nDXGIFactoryFlags, IID_PPV_ARGS(&pFactory));
		if (SUCCEEDED(hr)) goto DXGI_FACTORY_INIT_END;

		hr = CreateDXGIFactory1(IID_PPV_ARGS(&pFactory));
		if (SUCCEEDED(hr)) goto DXGI_FACTORY_INIT_END;

		hr = CreateDXGIFactory(IID_PPV_ARGS(&pFactory));
		if (SUCCEEDED(hr)) goto DXGI_FACTORY_INIT_END;
		else {
			throw "CreateDXGIFactory Failed.";
		}
	}

	DXGI_FACTORY_INIT_END:

	//hr = ::CreateDXGIFactory2(nDXGIFactoryFlags, __uuidof(IDXGIFactory4), (void
	//	**)&pFactory); // sus..
	//if (FAILED(hr)) {
	//	OutputDebugStringA("[ERROR] : Create Factory Error.\n");
	//	return;
	//}

	constexpr D3D_FEATURE_LEVEL FeatureLevelPriority[11] = {
		D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1, D3D_FEATURE_LEVEL_1_0_CORE
	};

	//Adapter Version Check
	IDXGIAdapter1* adapter = NULL;
	IDXGIAdapter4* pd3dAdapter4 = NULL;
	IDXGIAdapter3* pd3dAdapter3 = NULL;
	IDXGIAdapter2* pd3dAdapter2 = NULL;
	pFactory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter));
	DXGI_ADAPTER_DESC1 dxgiAdapterDesc;
	adapter->GetDesc1(&dxgiAdapterDesc);
	adapter->QueryInterface(__uuidof(IDXGIAdapter4), (void**)&pd3dAdapter4);
	if (pd3dAdapter4 != nullptr) {
		adapterVersion = 4;
		goto DXGI_ADAPTER_VERSION_CHECK;
	}
	adapter->QueryInterface(__uuidof(IDXGIAdapter3), (void**)&pd3dAdapter3);
	if (pd3dAdapter4 != nullptr) {
		adapterVersion = 3;
		goto DXGI_ADAPTER_VERSION_CHECK;
	}
	adapter->QueryInterface(__uuidof(IDXGIAdapter2), (void**)&pd3dAdapter2);
	if (pd3dAdapter4 != nullptr) {
		adapterVersion = 2;
		goto DXGI_ADAPTER_VERSION_CHECK;
	}
	adapterVersion = 1;

DXGI_ADAPTER_VERSION_CHECK:

	//실제 디바이스 생성은 안함.
	// 어디까지 지원되는지 테스트 & 어댑터 선택
	for (int i = 0; i < 11; ++i) {
		minFeatureLevel = FeatureLevelPriority[i];
		for (UINT adapterID = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapterByGpuPreference(adapterID, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)); ++adapterID) {

			DXGI_ADAPTER_DESC1 desc;
			hr = adapter->GetDesc1(&desc);
			if (FAILED(hr)) {
				throw "adapter GetDesc1() Failed.";
			}

			//Code From DirectX RaytracingHelloWorld Sample Start
			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				// Don't select the Basic Render Driver adapter.
				continue;
			}
			//Code From DirectX RaytracingHelloWorld Sample End

			hr = D3D12CreateDevice(adapter, minFeatureLevel, _uuidof(ID3D12Device), nullptr);
			if (SUCCEEDED(hr))
			{
				pSelectedAdapter = adapter;
				adapterIndex = adapterID;
				wcscpy_s(adapterDescription, desc.Description);
#ifdef _DEBUG
				wchar_t buff[256] = {};
				swprintf_s(buff, L"Direct3D Adapter (%u): VID:%04X, PID:%04X - %ls\n", adapterID, desc.VendorId, desc.DeviceId, desc.Description);
				OutputDebugStringW(buff);
#endif
				goto DXGI_FINISH_SELECT_ADAPTER;
			}
		}
	}

	// if adapter is not selected Try WARP12(CPU Simulate) instead
	if (FAILED(pFactory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))))
	{
		throw "WARP12 not available. Enable the 'Graphics Tools' optional feature";
	}

DXGI_FINISH_SELECT_ADAPTER:

	hr = D3D12CreateDevice(pSelectedAdapter, minFeatureLevel, _uuidof(ID3D12Device), (void**)&pDevice);
	if (FAILED(hr)) throw "D3D12CreateDevice Failed.";

	//IDXGIAdapter1* pd3dAdapter1 = NULL;
	//for (int i = 0; i < 11; ++i) {
	//	// Find GPU that support DirectX 12_2
	//	for (UINT i = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(i, &pd3dAdapter1); i++)
	//	{
	//		IDXGIAdapter4* pd3dAdapter4 = NULL;
	//		DXGI_ADAPTER_DESC1 dxgiAdapterDesc;
	//		pd3dAdapter1->GetDesc1(&dxgiAdapterDesc);
	//		pd3dAdapter1->QueryInterface(__uuidof(IDXGIAdapter4), (void**)&pd3dAdapter4);
	//		if (dxgiAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
	//		if (SUCCEEDED(D3D12CreateDevice(pd3dAdapter4, FeatureLevelPriority[i],
	//			_uuidof(ID3D12Device), (void**)&pDevice))) {
	//			pd3dAdapter4->Release();
	//			pd3dAdapter1->Release();
	//			goto GlobalDeviceInit_InitMultisamplingVariable;
	//			break;
	//		}
	//		pd3dAdapter4->Release();
	//		pd3dAdapter1->Release();
	//	}
	//}
	
	/*if (!pd3dAdapter1)
	{
		OutputDebugStringA("[ERROR] : there is no GPU that support DirectX.\n");
		return;
	}*/

GlobalDeviceInit_InitMultisamplingVariable:

	CBV_SRV_UAV_Desc_IncrementSiz = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

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

	hr = pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
		(void**)&pFence);
	FenceValue = 0;
	hFenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	// Create Event Object to Sync with Fence. Event's Init value is FALSE, when Signal call, Event Value change to FALSE

	// Command Queue & Command Allocator & Command List (TYPE : DIRECT)
	D3D12_COMMAND_QUEUE_DESC d3dCommandQueueDesc;
	::ZeroMemory(&d3dCommandQueueDesc, sizeof(D3D12_COMMAND_QUEUE_DESC));
	d3dCommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	d3dCommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	hr = pDevice->CreateCommandQueue(&d3dCommandQueueDesc,
		_uuidof(ID3D12CommandQueue), (void**)&pCommandQueue);
	hr = pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
		__uuidof(ID3D12CommandAllocator), (void**)&pCommandAllocator);
	hr = pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		pCommandAllocator, NULL, __uuidof(ID3D12GraphicsCommandList), (void
			**)&pCommandList);

	// compute command List
	D3D12_COMMAND_QUEUE_DESC d3dComputeCommandQueueDesc;
	::ZeroMemory(&d3dComputeCommandQueueDesc, sizeof(D3D12_COMMAND_QUEUE_DESC));
	d3dComputeCommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	d3dComputeCommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	hr = pDevice->CreateCommandQueue(&d3dComputeCommandQueueDesc,
		_uuidof(ID3D12CommandQueue), (void**)&pComputeCommandQueue);
	hr = pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE,
		__uuidof(ID3D12CommandAllocator), (void**)&pComputeCommandAllocator);
	hr = pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE,
		pComputeCommandAllocator, NULL, __uuidof(ID3D12GraphicsCommandList), (void**)&pComputeCommandList);
	gd.CmdClose(gd.pComputeCommandList);

	InitBundleAllocator();

	hr = gd.CmdClose(pCommandList);

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

	ShaderVisibleDescPool.Initialize(1024 * 32);

	NewSwapChain();
	Create_RTV_DSV_DescriptorHeaps();
	CreateRenderTargetViews();
	CreateDepthStencilView();
	if (RenderMod == DeferedRendering) {
		//set gbuffer format ex>
		//GbufferPixelFormatArr[0] = DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM;

		CreateGbufferRenderTargetViews();
	}

	TextureDescriptorAllotter.Init(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 4096);
	//why cannot be shader visible in Saver DescriptorHeap?
	//answer : CreateConstantBufferView, CreateShaderResourceView -> only CPU Descriptor Working. so Shader Invisible.

	try {
		raytracing.Init(this);
	}
	catch (string err) {
		dbglog1a("RayTracing ERROR : %s\n", err);
	}
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

void GlobalDevice::WaitGPUComplete(ID3D12CommandQueue* commandQueue)
{
	ID3D12CommandQueue* selectQueue;
	if (commandQueue == nullptr) {
		selectQueue = pCommandQueue;
	}
	else selectQueue = commandQueue;
	FenceValue++;
	const UINT64 nFence = FenceValue;
	HRESULT hResult = selectQueue->Signal(pFence, nFence);
	//Add Signal Command (it update gpu fence value.)
	if (pFence->GetCompletedValue() < nFence)
	{
		//When GPU Fence is smaller than CPU FenceValue, Wait.
		hResult = pFence->SetEventOnCompletion(nFence, hFenceEvent);
		::WaitForSingleObject(hFenceEvent, INFINITE);
	}
}

HRESULT GlobalDevice::CmdReset(ID3D12GraphicsCommandList* cmdlist, ID3D12CommandAllocator* cmdalloc)
{
	isCmdClose = false;
	return cmdlist->Reset(cmdalloc, nullptr);
}

HRESULT GlobalDevice::CmdClose(ID3D12GraphicsCommandList* cmdlist) {
	isCmdClose = true;
	return cmdlist->Close();
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
	d3dDescriptorHeapDesc.NumDescriptors = SwapChainBufferCount+1;
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
		
		// renderTarget View
		pDevice->CreateRenderTargetView(ppRenderTargetBuffers[i], &MainRenderTargetViewDesc,
			d3dRtvCPUDescriptorHandle);

		// shader Resource View
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 1;
		D3D12_CPU_DESCRIPTOR_HANDLE pcpu;
		D3D12_GPU_DESCRIPTOR_HANDLE pgpu;
		gd.ShaderVisibleDescPool.ImmortalAllocDescriptorTable(&pcpu, &pgpu, 1);
		RenderTargetSRV_pGPU[i] = pgpu;
		pDevice->CreateShaderResourceView(ppRenderTargetBuffers[i], &srvDesc, pcpu);

		d3dRtvCPUDescriptorHandle.ptr += RtvDescriptorIncrementSize;
	}

	//Bluring UAV
	gd.BlurTexture = CreateTextureWithUAV(pDevice, gd.ClientFrameWidth, gd.ClientFrameHeight, DXGI_FORMAT_R8G8B8A8_UNORM);

	SubRenderTarget = CreateRenderTargetTextureWithRTV(pDevice,
		pRtvDescriptorHeap, 2, gd.ClientFrameWidth, gd.ClientFrameHeight);
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

	D3D12_RESOURCE_STATES d3dResourceInitialStates = D3D12_RESOURCE_STATE_COMMON;
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

		if (isCmdClose) {
			pCommandAllocator->Reset();
			CmdReset(pCommandList, pCommandAllocator);
		}

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

		/*imgform::PixelImageObject pio(width, height, textureData);
		pio.rawDataToBMP("test.bmp");*/

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
		texture.CreateTexture_fromImageBuffer(mipW, mipH, mipTex, DXGI_FORMAT_R8G8B8A8_UNORM);
		font_texture_map[i].insert(pair<wchar_t, GPUResource>(key, texture));
	}
}

void GlobalDevice::AddTextSDFTexture(wchar_t key)
{
	char Zero = 0;
	// gpu texture byte to signed float normalize
	// : float f = max((float)b / 127.0f, -1.0f)
	// so 127 is 0. 0x8F;
	// signed float normalize to byte
	// : b = f * 127 + 128;

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
		if (font_sdftexture_map[i].contains(key)) return;

		Glyph g = font_data[i].glyphs[key];
		int width = g.bounding_box[2] - g.bounding_box[0] + 1;
		int height = g.bounding_box[3] - g.bounding_box[1] + 1;
		float xBase = g.bounding_box[0];
		float yBase = g.bounding_box[1];
		char* textureData = new char[width * height];
		memset(textureData, (char)127, width * height);
		
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
						AddLineInSDFTexture(prevpos, newpos, textureData, width, height);
						prevpos = newpos;
					}
				}
				else {
					startpos.x -= xBase;
					startpos.y -= yBase;
					endpos.x -= xBase;
					endpos.y -= yBase;

					polygons[k].push_back({ endpos.x, endpos.y, 0.0f });
					AddLineInSDFTexture(startpos, endpos, textureData, width, height);
				}
			}
		}

		for (int yi = 1; yi < height; ++yi) {

			bool isfill = false;

			bool returning = 0;
			int retcnt = 0;
		ROW_RETURN:

			int lastendxi = 0;
			int lastturncnt = 0;

			for (int xi = 0; xi < width; ++xi) {
				char* ptr = (char*)&textureData[yi * width + xi];
				//float distance = getSDF(polygons, vec2f(xi, yi), false);
				//if(distance >= 0) *ptr = SignedFloatNormalizeToByte(distance);

				int pxi = xi;
				if (*ptr <= Zero) {
					bool ret = false;

					char* beginpaint = ptr;
					while (*beginpaint <= Zero && xi < width) {
						xi += 1;
						beginpaint += 1;
					}

					char* endpaint = beginpaint;
					while (*endpaint > Zero && xi < width) {
						xi += 1;
						endpaint += 1;
					}

					for (; beginpaint < endpaint; beginpaint += 1) {
						if (*(beginpaint - width) <= Zero) {
							*beginpaint = Zero; // distance
						}
						else {
							ret = true;
						}
					}

					if (xi == width && (lastturncnt == 0 && beginpaint == endpaint)) {
						xi = lastendxi - 1;
						char* insptr = (char*)&textureData[yi * width + lastendxi];
						*insptr = Zero; // distance
						lastturncnt += 1;
						continue;
					}

					if (ret == false) {
						while (*endpaint <= Zero && xi < width) {
							xi += 1;
							endpaint += 1;
						}
						lastendxi = xi;
						continue;
					}
					else {
						xi = ptr - (char*)&textureData[yi * width];
						returning = true;
					}
				}
			}

			if (returning) {
				retcnt += 1;
				if (retcnt < 2) goto ROW_RETURN;
			}
			else retcnt = 0;
		}

		int mipW = width / textureMipLevel;
		int mipH = height / textureMipLevel;
		int realW = mipW * 2;
		int realH = mipH * 2;
		int startX = mipW / 2;
		int startY = mipH / 2;
		char* mipTex = new char[realW * realH];
		memset(mipTex, 127, realW* realH);
		for (int yi = 0; yi < mipH; ++yi) {
			for (int xi = 0; xi < mipW; ++xi) {
				*(char*)&mipTex[(startY+yi) * (realW) +startX+xi] = *(char*)&textureData[yi * textureMipLevel * width + xi * textureMipLevel];
			}
		}
		delete[] textureData;

		imgform::PixelImageObject pio;
		pio.width = realW;
		pio.height = realH;
		vector<uint8_t> sdfbuffer = makeSDF((char*)mipTex, realW, realH, 0.25f, -1.0f * realH * 0.5f);
		pio.data = (imgform::RGBA_pixel*)sdfbuffer.data();
		//pio.rawDataToBMP("SDFTestImage.bmp", DXGI_FORMAT_R8_SNORM);

		GPUResource texture;
		ZeroMemory(&texture, sizeof(GPUResource));
		texture.CreateTexture_fromImageBuffer(realW, realH, sdfbuffer.data(), DXGI_FORMAT_R8_SNORM);
		font_sdftexture_map[i].insert(pair<wchar_t, GPUResource>(key, texture));
		delete[] mipTex;
	}
}

void GlobalDevice::AddLineInSDFTexture(float_v2 startpos, float_v2 endpos, char* texture, int width, int height)
{
	char Zero = 0;
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

			char* ptr = (char*)&texture[(int)y * width + x];
			*ptr = Zero;
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

			char* ptr = (char*)&texture[y* width + (int)x];
			*ptr = Zero;
		}
	}
}

char GlobalDevice::SignedFloatNormalizeToByte(float f)
{
	char b = (char)(f * 127.0f + 128.0f);
	return b;
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

void RayTracingDevice::Init(void* origin_gd) {
	HRESULT hr;
	origin = (GlobalDevice*)origin_gd;
	GlobalDevice* sup = (GlobalDevice*)origin_gd;

	CheckDeviceSelfRemovable();
	
	if (IsDirectXRaytracingSupported(sup->pSelectedAdapter) == false) {
		throw "Your GPU is not support RayTracing.";
	}

	bool b = InitDXC();
	if (!b) throw "Cannot Init DXC.";

	//CreateRaytracingInterfaces
	hr = origin->pDevice->QueryInterface(IID_PPV_ARGS(&dxrDevice));
	if (FAILED(hr)) throw "Couldn't get DirectX Raytracing interface for the device.";
	hr = origin->pCommandList->QueryInterface(IID_PPV_ARGS(&dxrCommandList));
	if (FAILED(hr)) throw "Couldn't get DirectX Raytracing interface for the command list.";
	
	CreateSubRenderTarget();

	CreateCameraCB();
}

void RayTracingDevice::CheckDeviceSelfRemovable()
{
	__if_exists(DXGIDeclareAdapterRemovalSupport)
	{
		if (FAILED(DXGIDeclareAdapterRemovalSupport()))
		{
			OutputDebugString(L"Warning: application failed to declare adapter removal support.\n");
		}
	}
}

inline bool RayTracingDevice::IsDirectXRaytracingSupported(IDXGIAdapter1* adapter)
{
	ID3D12Device* testDevice;
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 featureSupportData = {};

	return SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&testDevice)))
		&& SUCCEEDED(testDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &featureSupportData, sizeof(featureSupportData)))
		&& featureSupportData.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
}

void RayTracingDevice::SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ID3D12RootSignature** rootSig)
{
	HRESULT hr;
	ID3DBlob* blob;
	ID3DBlob* error;

	hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
	if (FAILED(hr)) {
		if (error)
		{
			// 에러 메시지 출력
			OutputDebugStringA((char*)error->GetBufferPointer());
			error->Release();
		}

		//throw error ? static_cast<char*>(error->GetBufferPointer()) : nullptr;
	}

	hr = dxrDevice->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig)));
	if (FAILED(hr)) throw "dxrDevice->CreateRootSignature Failed.";
}

bool RayTracingDevice::InitDXC()
{
	bool bResult = FALSE;
	HRESULT hr;
	const WCHAR* wchDllPath = nullptr;
#if defined(_M_ARM64EC)
	wchDllPath = L"./Dxc/arm64";
#elif defined(_M_ARM64)
	wchDllPath = L"./Dxc/arm64";
#elif defined(_M_AMD64)
	wchDllPath = L"./Dxc/x64";
#elif defined(_M_IX86)
	wchDllPath = L"./Dxc/x86";
#endif
	WCHAR wchOldPath[_MAX_PATH];
	GetCurrentDirectoryW(_MAX_PATH, wchOldPath);
	SetCurrentDirectoryW(wchDllPath);
	DxcCreateInstanceT DxcCreateInstanceFunc = nullptr;

	hDXC = LoadLibrary(L"dxcompiler.dll");
	if (!hDXC) goto lb_return;

	DxcCreateInstanceFunc = (DxcCreateInstanceT)GetProcAddress(hDXC, "DxcCreateInstance");

	hr = DxcCreateInstanceFunc(CLSID_DxcLibrary, IID_PPV_ARGS(&pDXCLib));
	if (FAILED(hr))
		__debugbreak();

	hr = DxcCreateInstanceFunc(CLSID_DxcCompiler, IID_PPV_ARGS(&pDXCCompiler));
	if (FAILED(hr))
		__debugbreak();

	pDXCLib->CreateIncludeHandler(&pDSCIncHandle);
	bResult = TRUE;

lb_return:
	SetCurrentDirectoryW(wchOldPath);
	return bResult;
}

SHADER_HANDLE* RayTracingDevice::CreateShaderDXC(const wchar_t* shaderPath, const WCHAR* wchShaderFileName, const WCHAR* wchEntryPoint, const WCHAR* wchShaderModel, DWORD dwFlags)
{
	BOOL				bResult = FALSE;

	SYSTEMTIME	CreationTime = {};
	SHADER_HANDLE* pNewShaderHandle = nullptr;

	WCHAR wchOldPath[MAX_PATH];
	GetCurrentDirectory(_MAX_PATH, wchOldPath);

	IDxcBlob* pBlob = nullptr;

	// case DXIL::ShaderKind::Vertex:    entry = L"VSMain"; profile = L"vs_6_1"; break;
	// case DXIL::ShaderKind::Pixel:     entry = L"PSMain"; profile = L"ps_6_1"; break;
	// case DXIL::ShaderKind::Geometry:  entry = L"GSMain"; profile = L"gs_6_1"; break;
	// case DXIL::ShaderKind::Hull:      entry = L"HSMain"; profile = L"hs_6_1"; break;
	// case DXIL::ShaderKind::Domain:    entry = L"DSMain"; profile = L"ds_6_1"; break;
	// case DXIL::ShaderKind::Compute:   entry = L"CSMain"; profile = L"cs_6_1"; break;
	// case DXIL::ShaderKind::Mesh:      entry = L"MSMain"; profile = L"ms_6_5"; break;
	// case DXIL::ShaderKind::Amplification: entry = L"ASMain"; profile = L"as_6_5"; break;

	//"vs_6_0"
	//"ps_6_0"
	//"cs_6_0"
	//"gs_6_0"
	//"ms_6_5"
	//"as_6_5"
	//"hs_6_0"
	//"lib_6_3"

	bool bDisableOptimize = true;
#ifdef _DEBUG
	bDisableOptimize = false;
#endif

	SetCurrentDirectory(shaderPath);
	HRESULT	hr = CompileShaderFromFileWithDXC(pDXCLib, pDXCCompiler, pDSCIncHandle, wchShaderFileName, wchEntryPoint, wchShaderModel, &pBlob, bDisableOptimize, &CreationTime, 0);

	if (FAILED(hr))
	{
		dbglog2(L"Failed to compile shader : %s-%s\n", wchShaderFileName, wchEntryPoint);
		throw "Failed to compile shader";
	}
	DWORD	dwCodeSize = (DWORD)pBlob->GetBufferSize();
	const char* pCodeBuffer = (const char*)pBlob->GetBufferPointer();

	DWORD	ShaderHandleSize = sizeof(SHADER_HANDLE) - sizeof(DWORD) + dwCodeSize;
	pNewShaderHandle = (SHADER_HANDLE*)malloc(ShaderHandleSize);

	if (pNewShaderHandle != nullptr) {
		memset(pNewShaderHandle, 0, ShaderHandleSize);
		memcpy(pNewShaderHandle->pCodeBuffer, pCodeBuffer, dwCodeSize);
		pNewShaderHandle->dwCodeSize = dwCodeSize;
		pNewShaderHandle->dwShaderNameLen = swprintf_s(pNewShaderHandle->wchShaderName, L"%s-%s", wchShaderFileName, wchEntryPoint);
		bResult = TRUE;
	}

lb_exit:
	if (pBlob)
	{
		pBlob->Release();
		pBlob = nullptr;
	}
	SetCurrentDirectory(wchOldPath);

	return pNewShaderHandle;
}

void RayTracingDevice::CreateSubRenderTarget()
{
	ID3D12Device5* device = dxrDevice;
	DXGI_FORMAT backbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	// Create the output resource. The dimensions and format should match the swap-chain.
	auto uavDesc = CD3DX12_RESOURCE_DESC::Tex2D(backbufferFormat, gd.ClientFrameWidth, gd.ClientFrameHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(device->CreateCommittedResource(
		&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&RayTracingOutput)));
	SetName(RayTracingOutput, L"gd.raytracing.RayTracingOutput");

	gd.ShaderVisibleDescPool.ImmortalAllocDescriptorTable(&RTO_UAV_handle.hcpu, &RTO_UAV_handle.hgpu, 1);
	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
	UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	device->CreateUnorderedAccessView(RayTracingOutput, nullptr, &UAVDesc, RTO_UAV_handle.hcpu);
}

void RayTracingDevice::CreateCameraCB()
{
	// Create the constant buffer memory and map the CPU and GPU addresses
	const D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	size_t ElementSize = sizeof(RayTracingDevice::CameraConstantBuffer);
	ElementSize = ((ElementSize + 255) & ~255);
	const D3D12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(ElementSize);

	ThrowIfFailed(dxrDevice->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&constantBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&CameraCB)));

	// Map the constant buffer and cache its heap pointers.
	// We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
	CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
	ThrowIfFailed(CameraCB->Map(0, nullptr, reinterpret_cast<void**>(&MappedCB)));

	gd.raytracing.m_eye = vec4( 0.0f, 2.0f, -5.0f, 1.0f );
	gd.raytracing.m_at = vec4( 0.0f, 0.0f, 0.0f, 1.0f );
	XMVECTOR right = { 1.0f, 0.0f, 0.0f, 0.0f };

	XMVECTOR direction = XMVector4Normalize(gd.raytracing.m_at - gd.raytracing.m_eye);
	gd.raytracing.m_up = XMVector3Normalize(XMVector3Cross(direction, right));

	// Rotate camera around Y axis.
	XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(45.0f));
	gd.raytracing.m_eye = XMVector3Transform(gd.raytracing.m_eye, rotate);
	gd.raytracing.m_up = XMVector3Transform(gd.raytracing.m_up, rotate);

	MappedCB->cameraPosition = m_eye;
	float fovAngleY = 45.0f;
	XMMATRIX view = XMMatrixLookAtLH(gd.raytracing.m_eye, gd.raytracing.m_at, gd.raytracing.m_up);
	float m_aspectRatio = (float)gd.ClientFrameWidth / (float)gd.ClientFrameHeight;
	XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(fovAngleY), m_aspectRatio, 1.0f, 125.0f);
	XMMATRIX viewProj = view * proj;

	MappedCB->projectionToWorld = XMMatrixTranspose(XMMatrixInverse(nullptr, viewProj));
	MappedCB->lightPosition = vec4(0.0f, 1.8f, -3.0f, 0.0f);
}

void RayTracingShader::Init() {
	CreateGlobalRootSignature();
	CreateLocalRootSignatures();
	CreatePipelineState();
	BuildAccelationStructure();
	BuildShaderTable();
}

void RayTracingShader::CreateGlobalRootSignature()
{
	//GRS
	CD3DX12_DESCRIPTOR_RANGE UAVDescriptor;
	UAVDescriptor.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE srvVB;
	srvVB.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1); // t1
	CD3DX12_DESCRIPTOR_RANGE srvIB;
	srvIB.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2); // t2
	CD3DX12_DESCRIPTOR_RANGE ranges[2] = { srvVB, srvIB };

	constexpr int ParamCount = 4;
	CD3DX12_ROOT_PARAMETER rootParameters[ParamCount];
	rootParameters[0].InitAsDescriptorTable(1, &UAVDescriptor); // OutputView u0
	rootParameters[1].InitAsShaderResourceView(0); // AS t0
	rootParameters[2].InitAsConstantBufferView(0, 0); // Camera CBV b0
	rootParameters[3].InitAsDescriptorTable(2, ranges); // Vertex / IndexBuffers t1 t2

	CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
	gd.raytracing.SerializeAndCreateRaytracingRootSignature(globalRootSignatureDesc, &pGlobalRootSignature);
}

void RayTracingShader::CreateLocalRootSignatures()
{
	//[0] LRS
	ID3D12RootSignature* rootsig = nullptr;
	CD3DX12_ROOT_PARAMETER rootParameters[1];
	int siz = SizeOfInUint32(DefaultCB);
	rootParameters[0].InitAsConstants(siz, 1, 0);
	CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
	localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
	gd.raytracing.SerializeAndCreateRaytracingRootSignature(localRootSignatureDesc, &rootsig);
	pLocalRootSignature.push_back(rootsig);
}

void RayTracingShader::CreatePipelineState() {
	HRESULT hr;
	CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

	// 1. DXIL library
	// 1-1. Load RayTracing Shader
	SHADER_HANDLE* shaderHandle = gd.raytracing.CreateShaderDXC(L"Resources\\Shaders\\RayTracing", L"Raytracing.hlsl", L"", L"lib_6_3", 0);
	// 1-2. Set DXIL Lib
	CD3DX12_DXIL_LIBRARY_SUBOBJECT* lib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE((void*)shaderHandle->pCodeBuffer, shaderHandle->dwCodeSize);
	lib->SetDXILLibrary(&libdxil);

	const wchar_t* RayGenShaderName = L"MyRaygenShader";
	const wchar_t* ClosestHitShaderName = L"MyClosestHitShader";
	const wchar_t* MissShaderName = L"MyMissShader";

	lib->DefineExport(RayGenShaderName);
	lib->DefineExport(ClosestHitShaderName);
	lib->DefineExport(MissShaderName);

	// 2. Hit Group
	const wchar_t* HitGroupName = L"MyHitGroup";
	CD3DX12_HIT_GROUP_SUBOBJECT* hitGroup = raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
	hitGroup->SetClosestHitShaderImport(ClosestHitShaderName);
	hitGroup->SetHitGroupExport(HitGroupName);
	hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

	// 3. Shader config
	CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT* shaderConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	UINT payloadSize = 4 * sizeof(float);   // float4 color
	UINT attributeSize = 2 * sizeof(float); // float2 barycentrics
	shaderConfig->Config(payloadSize, attributeSize);

	// 4. Local root signature and shader association
	// 4-1. Local root signature (ray gen shader)
	{
		CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT* localRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
		localRootSignature->SetRootSignature(pLocalRootSignature[0]);
		// Shader association
		CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT* rootSignatureAssociation = raytracingPipeline.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
		rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
		rootSignatureAssociation->AddExport(RayGenShaderName);
	}

	// Global root signature
	CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT* globalRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	globalRootSignature->SetRootSignature(pGlobalRootSignature);

	// Pipeline config
	CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT* pipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	UINT maxRecursionDepth = 1; // ~ primary rays only. 
	pipelineConfig->Config(maxRecursionDepth);

#if _DEBUG
	PrintStateObjectDesc(raytracingPipeline);
#endif

	// Create the state object.
	hr = gd.raytracing.dxrDevice->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&RTPSO));
	if (FAILED(hr)) {
		throw "Couldn't create DirectX Raytracing state object.";
	}
}

void RayTracingShader::BuildAccelationStructure()
{
	if (TestMesh == nullptr) {
		TestMesh = new RayTracingMesh();
		TestMesh->CreateCubeMesh();
	}
	
	if (gd.raytracing.ASBuild_ScratchResource == nullptr) {
		AllocateUAVBuffer(gd.raytracing.dxrDevice, gd.raytracing.ASBuild_ScratchResource_Maxsiz, &gd.raytracing.ASBuild_ScratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");
	}
	// Reset the command list for the acceleration structure construction.

	ID3D12Device5* device = gd.raytracing.dxrDevice;
	ID3D12GraphicsCommandList4* commandList = gd.raytracing.dxrCommandList;
	ID3D12CommandQueue* commandQueue = gd.pCommandQueue;
	ID3D12CommandAllocator* commandAllocator = gd.pCommandAllocator;

	if (gd.isCmdClose == false)
	{
		gd.CmdClose(commandList);
		ID3D12CommandList* ppd3dCommandLists[] = { commandList };
		commandQueue->ExecuteCommandLists(1, ppd3dCommandLists);
		gd.WaitGPUComplete();
	}
	gd.CmdReset(commandList, commandAllocator);

	// Get required sizes for an acceleration structure.
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS topLevelInputs = {};
	topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	topLevelInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	topLevelInputs.NumDescs = 1;
	topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
	device->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
	if (topLevelPrebuildInfo.ResultDataMaxSizeInBytes <= 0) {
		throw "GetRaytracingAccelerationStructurePrebuildInfo Failed.";
	}

	// Allocate resources for acceleration structures.
	// Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
	// Default heap is OK since the application doesn't need CPU read/write access to them. 
	// The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
	// and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
	//  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
	//  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.
	{
		D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;

		AllocateUAVBuffer(device, topLevelPrebuildInfo.ResultDataMaxSizeInBytes, &TLAS, initialResourceState, L"TopLevelAccelerationStructure");
	}

	// Create an instance desc for the bottom-level acceleration structure.
	D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
	instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1;
	instanceDesc.InstanceMask = 1;
	instanceDesc.AccelerationStructure = TestMesh->BLAS->GetGPUVirtualAddress();
	AllocateUploadBuffer(device, &instanceDesc, sizeof(instanceDesc), &TLAS_InstanceDescs_Res, L"InstanceDescs");

	// Bottom Level Acceleration Structure desc
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
	{
		bottomLevelBuildDesc.Inputs = TestMesh->BLAS_Input;
		bottomLevelBuildDesc.ScratchAccelerationStructureData = gd.raytracing.ASBuild_ScratchResource->GetGPUVirtualAddress();
		bottomLevelBuildDesc.DestAccelerationStructureData = TestMesh->BLAS->GetGPUVirtualAddress();
	}

	// Top Level Acceleration Structure desc
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
	{
		topLevelInputs.InstanceDescs = TLAS_InstanceDescs_Res->GetGPUVirtualAddress();
		topLevelBuildDesc.Inputs = topLevelInputs;
		topLevelBuildDesc.DestAccelerationStructureData = TLAS->GetGPUVirtualAddress();
		topLevelBuildDesc.ScratchAccelerationStructureData = gd.raytracing.ASBuild_ScratchResource->GetGPUVirtualAddress();
	}

	// Build acceleration structure.
	auto BuildAccelerationStructure = [&](auto* raytracingCommandList)
	{
		raytracingCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
	};

	BuildAccelerationStructure(commandList);

	gd.CmdClose(commandList);

	// Kick off acceleration structure construction.
	ID3D12CommandList* ppd3dCommandLists[] = { commandList };
	commandQueue->ExecuteCommandLists(1, ppd3dCommandLists);

	// Wait for GPU to finish as the locally created temporary GPU resources will get released once we go out of scope.
	gd.WaitGPUComplete();
}

void RayTracingShader::BuildShaderTable()
{
	HRESULT hr;

	ID3D12Device5* device = gd.raytracing.dxrDevice;

	const wchar_t* RayGenShaderName = L"MyRaygenShader";
	const wchar_t* ClosestHitShaderName = L"MyClosestHitShader";
	const wchar_t* MissShaderName = L"MyMissShader";
	const wchar_t* HitGroupName = L"MyHitGroup";

	void* rayGenShaderIdentifier;
	void* missShaderIdentifier;
	void* hitGroupShaderIdentifier;

	auto GetShaderIdentifiers = [&](auto* stateObjectProperties)
		{
			rayGenShaderIdentifier = stateObjectProperties->GetShaderIdentifier(RayGenShaderName);
			missShaderIdentifier = stateObjectProperties->GetShaderIdentifier(MissShaderName);
			hitGroupShaderIdentifier = stateObjectProperties->GetShaderIdentifier(HitGroupName);
		};

	// Get shader identifiers.
	UINT shaderIdentifierSize;
	ID3D12StateObjectProperties* stateObjectProperties = nullptr;
	hr = RTPSO->QueryInterface<ID3D12StateObjectProperties>(&stateObjectProperties);
	if (FAILED(hr) || stateObjectProperties == nullptr) {
		throw "RTPSO QueryInterface to ID3D12StateObjectProperties Failed.";
	}
	GetShaderIdentifiers(stateObjectProperties);
	shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

	// Ray gen shader table
	{
		UINT numShaderRecords = 1;
		UINT shaderRecordSize = shaderIdentifierSize;
		ShaderTable rayGenShaderTable(device, numShaderRecords, shaderRecordSize, L"RayGenShaderTable");
		rayGenShaderTable.push_back(ShaderRecord(rayGenShaderIdentifier, shaderIdentifierSize));
		RayGenShaderTable = rayGenShaderTable.GetResource();
	}

	// Miss shader table
	{
		UINT numShaderRecords = 1;
		UINT shaderRecordSize = shaderIdentifierSize;
		ShaderTable missShaderTable(device, numShaderRecords, shaderRecordSize, L"MissShaderTable");
		missShaderTable.push_back(ShaderRecord(missShaderIdentifier, shaderIdentifierSize));
		MissShaderTable = missShaderTable.GetResource();
	}

	// Hit group shader table
	{
		struct RootArguments {
			RayGenConstantBuffer cb;
		} rootArguments;

		float border = 0.1f;
		rootArguments.cb.stencil =
		{
			-1, -1 + border,
			 1 - border, 1.0f
		};
		rootArguments.cb.viewport = { -1, -1, 1, 1 };

		UINT numShaderRecords = 1;
		UINT shaderRecordSize = shaderIdentifierSize + sizeof(rootArguments);
		ShaderTable hitGroupShaderTable(device, numShaderRecords, shaderRecordSize, L"HitGroupShaderTable");
		hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderIdentifier, shaderIdentifierSize, &rootArguments, sizeof(rootArguments)));
		HitGroupShaderTable = hitGroupShaderTable.GetResource();
	}
}

void RayTracingMesh::CreateTriangleRayMesh(){
	if (gd.raytracing.ASBuild_ScratchResource == nullptr) {
		AllocateUAVBuffer(gd.raytracing.dxrDevice, gd.raytracing.ASBuild_ScratchResource_Maxsiz, &gd.raytracing.ASBuild_ScratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");
	}

	unsigned short indices[] =
	{
		0, 1, 2
	};

	float depthValue = 1.0;
	float offset = 0.7f;
	Vertex vertices[] =
	{
		Vertex({ 0, -offset, depthValue }, {0, 0, 0}),
		Vertex({ -offset, offset, depthValue }, {0, 0, 0}),
		Vertex({ offset, offset, depthValue }, {0, 0, 0})
	};

	AllocateUploadBuffer(gd.raytracing.dxrDevice, vertices, sizeof(vertices), &vertexBuffer);
	AllocateUploadBuffer(gd.raytracing.dxrDevice, indices, sizeof(indices), &indexBuffer);

	//Geometry
	GeometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	GeometryDesc.Triangles.IndexBuffer = indexBuffer->GetGPUVirtualAddress();
	GeometryDesc.Triangles.IndexCount = static_cast<UINT>(indexBuffer->GetDesc().Width) / sizeof(unsigned short);
	GeometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;
	GeometryDesc.Triangles.Transform3x4 = 0;
	GeometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	GeometryDesc.Triangles.VertexCount = static_cast<UINT>(vertexBuffer->GetDesc().Width) / sizeof(Vertex);
	GeometryDesc.Triangles.VertexBuffer.StartAddress = vertexBuffer->GetGPUVirtualAddress();
	GeometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
	GeometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE; // Closest Hit Shader

	//BLAS Input
	BLAS_Input.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	BLAS_Input.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	BLAS_Input.NumDescs = 1;
	BLAS_Input.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	BLAS_Input.pGeometryDescs = &GeometryDesc;

	//Prebuild Info
	gd.raytracing.dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&BLAS_Input, &bottomLevelPrebuildInfo);
	if (bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes <= 0) {
		throw "bottomLevelPrebuildInfo Create Failed.";
	}

	//Create BLAS Res
	D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	AllocateUAVBuffer(gd.raytracing.dxrDevice, bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &BLAS, initialResourceState, L"BottomLevelAccelerationStructure");

	// BLAS Build Desc
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
	bottomLevelBuildDesc.Inputs = BLAS_Input;
	bottomLevelBuildDesc.ScratchAccelerationStructureData = gd.raytracing.ASBuild_ScratchResource->GetGPUVirtualAddress();
	bottomLevelBuildDesc.DestAccelerationStructureData = BLAS->GetGPUVirtualAddress();

	//Build BLAS
	ID3D12Device5* device = gd.raytracing.dxrDevice;
	ID3D12GraphicsCommandList4* commandList = gd.raytracing.dxrCommandList;
	ID3D12CommandQueue* commandQueue = gd.pCommandQueue;
	ID3D12CommandAllocator* commandAllocator = gd.pCommandAllocator;
	if (gd.isCmdClose == false)
	{
		gd.CmdClose(commandList);
		ID3D12CommandList* ppd3dCommandLists[] = { commandList };
		commandQueue->ExecuteCommandLists(1, ppd3dCommandLists);
		gd.WaitGPUComplete();
	}
	gd.CmdReset(commandList, commandAllocator);
	commandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);

	CD3DX12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(BLAS);
	commandList->ResourceBarrier(1, &uavBarrier);
	gd.CmdClose(commandList);
	ID3D12CommandList* ppd3dCommandLists[] = { commandList };
	commandQueue->ExecuteCommandLists(1, ppd3dCommandLists);
}

void RayTracingMesh::CreateCubeMesh()
{
	if (gd.raytracing.ASBuild_ScratchResource == nullptr) {
		AllocateUAVBuffer(gd.raytracing.dxrDevice, gd.raytracing.ASBuild_ScratchResource_Maxsiz, &gd.raytracing.ASBuild_ScratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");
	}

	// Cube indices.
	UINT16 indices[] =
	{
		3,1,0,
		2,1,3,

		6,4,5,
		7,4,6,

		11,9,8,
		10,9,11,

		14,12,13,
		15,12,14,

		19,17,16,
		18,17,19,

		22,20,21,
		23,20,22
	};

	// Cube vertices positions and corresponding triangle normals.
	Vertex vertices[] =
	{
		Vertex(XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)),
		Vertex(XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)),
		Vertex(XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)),
		Vertex(XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)),

		Vertex(XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f)),
		Vertex(XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f)),
		Vertex(XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f)),
		Vertex(XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f)),

		Vertex( XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) ),
		Vertex( XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) ),
		Vertex( XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) ),
		Vertex( XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) ),

		Vertex( XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) ),
		Vertex( XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) ),
		Vertex(XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) ),
		Vertex(XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) ),

		Vertex(XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) ),
		Vertex(XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) ),
		Vertex(XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) ),
		Vertex(XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) ),

		Vertex(XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) ),
		Vertex(XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) ),
		Vertex(XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) ),
		Vertex(XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) )
	};

	AllocateUploadBuffer(gd.raytracing.dxrDevice, vertices, sizeof(vertices), &vertexBuffer);
	AllocateUploadBuffer(gd.raytracing.dxrDevice, indices, sizeof(indices), &indexBuffer);

	//Geometry
	GeometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	GeometryDesc.Triangles.IndexBuffer = indexBuffer->GetGPUVirtualAddress();
	GeometryDesc.Triangles.IndexCount = static_cast<UINT>(indexBuffer->GetDesc().Width) / sizeof(unsigned short);
	GeometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;
	GeometryDesc.Triangles.Transform3x4 = 0;
	GeometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	GeometryDesc.Triangles.VertexCount = static_cast<UINT>(vertexBuffer->GetDesc().Width) / sizeof(Vertex);
	GeometryDesc.Triangles.VertexBuffer.StartAddress = vertexBuffer->GetGPUVirtualAddress();
	GeometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
	GeometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE; // Closest Hit Shader

	//BLAS Input
	BLAS_Input.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	BLAS_Input.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	BLAS_Input.NumDescs = 1;
	BLAS_Input.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	BLAS_Input.pGeometryDescs = &GeometryDesc;

	//Prebuild Info
	gd.raytracing.dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&BLAS_Input, &bottomLevelPrebuildInfo);
	if (bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes <= 0) {
		throw "bottomLevelPrebuildInfo Create Failed.";
	}

	//Create BLAS Res
	D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	AllocateUAVBuffer(gd.raytracing.dxrDevice, bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &BLAS, initialResourceState, L"BottomLevelAccelerationStructure");

	// BLAS Build Desc
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
	bottomLevelBuildDesc.Inputs = BLAS_Input;
	bottomLevelBuildDesc.ScratchAccelerationStructureData = gd.raytracing.ASBuild_ScratchResource->GetGPUVirtualAddress();
	bottomLevelBuildDesc.DestAccelerationStructureData = BLAS->GetGPUVirtualAddress();

	//Build BLAS
	ID3D12Device5* device = gd.raytracing.dxrDevice;
	ID3D12GraphicsCommandList4* commandList = gd.raytracing.dxrCommandList;
	ID3D12CommandQueue* commandQueue = gd.pCommandQueue;
	ID3D12CommandAllocator* commandAllocator = gd.pCommandAllocator;
	if (gd.isCmdClose == false)
	{
		gd.CmdClose(commandList);
		ID3D12CommandList* ppd3dCommandLists[] = { commandList };
		commandQueue->ExecuteCommandLists(1, ppd3dCommandLists);
		gd.WaitGPUComplete();
	}
	gd.CmdReset(commandList, commandAllocator);
	commandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);

	CD3DX12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(BLAS);
	commandList->ResourceBarrier(1, &uavBarrier);
	gd.CmdClose(commandList);
	ID3D12CommandList* ppd3dCommandLists[] = { commandList };
	commandQueue->ExecuteCommandLists(1, ppd3dCommandLists);

	//Make SRV Table Of Vertex And Index Buffer
	gd.ShaderVisibleDescPool.ImmortalAllocDescriptorTable(&VBIB_DescHandle.hcpu, &VBIB_DescHandle.hgpu, 2);

	DescHandle descHandle = VBIB_DescHandle;
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc_VB = {};
	srvDesc_VB.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc_VB.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc_VB.Buffer.NumElements = ARRAYSIZE(vertices);
	srvDesc_VB.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc_VB.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	srvDesc_VB.Buffer.StructureByteStride = sizeof(Vertex);
	gd.pDevice->CreateShaderResourceView(vertexBuffer, &srvDesc_VB, descHandle.hcpu);

	descHandle += gd.CBV_SRV_UAV_Desc_IncrementSiz;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc_IB = {};
	srvDesc_IB.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc_IB.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc_IB.Buffer.NumElements = ARRAYSIZE(vertices);
	srvDesc_IB.Format = DXGI_FORMAT_R32_TYPELESS;
	srvDesc_IB.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
	srvDesc_IB.Buffer.StructureByteStride = 0;
	gd.pDevice->CreateShaderResourceView(vertexBuffer, &srvDesc_IB, descHandle.hcpu);
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

	//LightCBData_withShadow = new LightCB_DATA_withShadow();
	ncbElementBytes = ((sizeof(LightCB_DATA) + 255) & ~255); //256의 배수
	LightCB_withShadowResource = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1);
	LightCB_withShadowResource.resource->Map(0, NULL, (void**)&LightCBData_withShadow);
	LightCBData_withShadow->dirlight.gLightColor = { 0.5f, 0.5f, 0.5f };
	vec4 dir = vec4(1, -2, 1, 0);
	dir.len3 = 1;
	LightCBData_withShadow->dirlight.gLightDirection = { dir.x, dir.y, dir.z };
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
	for (int i = 0; i < 16; ++i) {
		MirrorPoses[i] = vec4(-1000 + i * 100, 20, 0, 1);
	}

	gd.CmdReset(gd.pCommandList, gd.pCommandAllocator);

	CD3DX12_RESOURCE_BARRIER Trans = CD3DX12_RESOURCE_BARRIER::Transition(gd.SubRenderTarget.resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT); // this present.
	gd.pCommandList->ResourceBarrier(1, &Trans);

	Guntex.CreateTexture_fromFile(L"Resources/Mesh/GlobalTexture/m1911pistol_diffuse0.png", basicTexFormat, basicTexMip);
	DefaultTex.CreateTexture_fromFile(L"Resources/GlobalTexture/DefaultTexture.png", basicTexFormat, basicTexMip);
	DefaultNoramlTex.CreateTexture_fromFile(L"Resources/GlobalTexture/DefaultNormalTexture.png", basicTexFormat, basicTexMip);
	DefaultAmbientTex.CreateTexture_fromFile(L"Resources/GlobalTexture/DefaultAmbientTexture.png", basicTexFormat, basicTexMip);
	DefaultExplosionParticle.CreateTexture_fromFile(L"Resources/GlobalTexture/explosion_particle_NC.png", basicTexFormat, basicTexMip);
	DefaultMirrorTex.CreateTexture_fromFile(L"Resources/Mesh/GlobalTexture/MirrorTex.png", basicTexFormat, basicTexMip, false);
	DefaultGrass.CreateTexture_fromFile(L"Resources/GlobalTexture/Grass01.dds", basicTexFormat, basicTexMip, true);
	MiniMapBackground.CreateTexture_fromFile(L"Resources/GlobalTexture/UI_MiniMap.png", basicTexFormat, basicTexMip);

	terrain = new TerrainObject();
	//terrain->LoadTerain("Resources/GlobalTexture/TerrainHeightMap.png", 50, 1.0f, vec4(0.0f, -10.0f, -500.0f, 0));
	terrain->LoadTessTerrain(L"Resources/GlobalTexture/TerrainHeightMap.png", 500, 50, vec4(0.0f, -10.0f, -500.0f, 0));

	TextureTable.push_back(&DefaultMirrorTex);
	Material MirrorMaterial;
	MirrorMaterial.ti.Diffuse = TextureTable.size() - 1;
	MirrorMaterial.SetDescTable();
	game.MaterialTable.push_back(MirrorMaterial);

	Model* model = new Model;
	model->LoadModelFile("Resources/Model/thunderbolt.model");

	Map = new GameMap();
	Map->LoadMap("The_Port");

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

	MyPBRShader1 = new PBRShader1();
	MyPBRShader1->InitShader();

	MySkyBoxShader = new SkyBoxShader();
	MySkyBoxShader->InitShader();
	MySkyBoxShader->LoadSkyBox(L"Resources/GlobalTexture/SkyBox_0.dds");

	MyGrassShader = new GSBilboardShader();
	MyGrassShader->InitShader();

	MyParticleShader = new ParticleShader();
	MyParticleShader->InitShader();
	MyParticle.Init(30);

	MyTesselationTestShader = new TesselationTestShader();
	MyTesselationTestShader->InitShader();

	MyComputeTestShader = new ComputeTestShader();
	MyComputeTestShader->InitShader();

	MyRayTracingShader = new RayTracingShader();
	MyRayTracingShader->Init();

	TextMesh = new UVMesh();
	TextMesh->CreateTextRectMesh();

	MytestTesselationMesh = new Mesh();
	MytestTesselationMesh->CreateTesslationTestMesh(10, 5);

	TestMirrorMesh = new BumpMesh();
	TestMirrorMesh->ReadMeshFromFile_OBJ("Resources/Mesh/Mirror001.obj", vec4(1, 1, 1, 1), false);

	MySpotLight.ShadowMap = MyShadowMappingShader->CreateShadowMap(4096, 4096, 0);
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
	
	Model* AW101 = new Model();
	AW101->LoadModelFile("Resources/Model/AW101.model");

	constexpr float height = 40.0f;

	MyPlayer->SetModel(AW101);
	MyPlayer->SetShader(MyShader);
	matrix mat = XMMatrixScaling(3, 3, 3);
	mat.pos.f3 = { 0.0f, 0, 0.0f };
	MyPlayer->SetWorldMatrix(mat);
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

	for (int i = 0; i < 20; ++i) {
		Monster* Monster1 = new Monster();
		Monster1->SetModel(AW101);
		Monster1->is_dead = false;
		Monster1->m_worldMatrix = XMMatrixScaling(3, 3, 3);
		Monster1->m_worldMatrix.pos.f3 = { (float)(rand() % 50), height, (float)(rand() % 50) };
		m_gameObjects.push_back(Monster1);
	}

	MonsterStartPtr = (Monster**) & m_gameObjects[1];

	/*GameObject* groundObject = new GameObject();
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

	GameObject* myModelObject = new GameObject();
	myModelObject->rmod = GameObject::eRenderMeshMod::_Model;
	myModelObject->pModel = model;
	myModelObject->SetShader(game.MyPBRShader1);
	myModelObject->m_worldMatrix.Id();
	myModelObject->m_worldMatrix.pos.f3 = { 0, -0.5, 1 };
	m_gameObjects.push_back(myModelObject);*/

	/*Trans = CD3DX12_RESOURCE_BARRIER::Transition(gd.ppRenderTargetBuffers[0], D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_PRESENT);
	gd.pCommandList->ResourceBarrier(1, &Trans);*/
	gd.CmdClose(gd.pCommandList);
	ID3D12CommandList* ppCommandLists[] = { gd.pCommandList };
	gd.pCommandQueue->ExecuteCommandLists(1, ppCommandLists);
	gd.WaitGPUComplete();

	/*gd.pCommandAllocator->Reset();
	gd.CmdReset(gd.pCommandList, gd.pCommandAllocator);
	Trans = CD3DX12_RESOURCE_BARRIER::Transition(gd.ppRenderTargetBuffers[1], D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_PRESENT);
	gd.pCommandList->ResourceBarrier(1, &Trans);
	gd.CmdClose(gd.pCommandList);
	ID3D12CommandList* ppCommandLists2[] = { gd.pCommandList };
	gd.pCommandQueue->ExecuteCommandLists(1, ppCommandLists2);
	gd.WaitGPUComplete();*/

	gd.viewportArr[0].ProjectMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), (float)gd.ClientFrameWidth / (float)gd.ClientFrameHeight, 0.01f, 1000.0f);

	if (testData.UseBundle) {
		gd.CreateNewBundle();
		XMFLOAT4X4 xmf4x4Projection;
		XMStoreFloat4x4(&xmf4x4Projection, XMMatrixTranspose(gd.viewportArr[0].ProjectMatrix));
		gd.pBundles[0]->SetGraphicsRootSignature(MyShader->pRootSignature);
		gd.pBundles[0]->SetPipelineState(MyShader->pPipelineState);
		gd.pBundles[0]->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4Projection, 0);
		gd.CmdClose(gd.pBundles[0]);
	}
}

void Game::Render() {
	// DRED 활성화 예시
	D3D12EnableExperimentalFeatures(1, &D3D12ExperimentalShaderModels, nullptr, nullptr);

	gd.viewportArr[0].UpdateFrustum();

	gd.ShaderVisibleDescPool.Reset();

	// normal text texture load
	if (gd.addTextureStack.size() > 0) {
		for (int i = 0; i < gd.addTextureStack.size(); ++i) {
			gd.AddTextTexture(gd.addTextureStack[i]);
		}
	}
	gd.addTextureStack.clear();

	//SDF text texture loading
	if (gd.addSDFTextureStack.size() > 0) {
		for (int i = 0; i < gd.addSDFTextureStack.size(); ++i) {
			gd.AddTextSDFTexture(gd.addSDFTextureStack[i]);
		}
	}
	gd.addSDFTextureStack.clear();

	Render_ShadowPass();

	HRESULT hResult = gd.pCommandAllocator->Reset();
	hResult = gd.CmdReset(gd.pCommandList, gd.pCommandAllocator);

	game.MySpotLight.ShadowMap.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	//Wait Finish Present
	D3D12_RESOURCE_BARRIER d3dResourceBarrier;
	::ZeroMemory(&d3dResourceBarrier, sizeof(D3D12_RESOURCE_BARRIER));
	d3dResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	d3dResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	d3dResourceBarrier.Transition.pResource = gd.SubRenderTarget.resource;
		/*gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex];*/
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
	gd.pCommandList->OMSetRenderTargets(1, &gd.SubRenderTarget.rtvHandle.hcpu/*d3dRtvCPUDescriptorHandle*/, TRUE,
		&d3dDsvCPUDescriptorHandle);
	float pfClearColor[4] = { 0.05f, 0.05f, 0.05f, 1.0f };
	gd.pCommandList->ClearRenderTargetView(gd.SubRenderTarget.rtvHandle.hcpu/*d3dRtvCPUDescriptorHandle*/, pfClearColor, 0, NULL);
	
	//render begin ----------------------------------------------------------------

	MySkyBoxShader->RenderSkyBox();
	gd.pCommandList->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);

	//tesslationTest
	MyTesselationTestShader->Add_RegisterShaderCommand(gd.pCommandList);
	matrix view001 = gd.viewportArr[0].ViewMatrix;
	view001 *= gd.viewportArr[0].ProjectMatrix;
	view001.transpose();
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &view001, 0);
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16);
	matrix Tr001;
	Tr001.pos.y = 2;
	Tr001.transpose();
	gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &Tr001, 0);
	MytestTesselationMesh->Render(gd.pCommandList, 1);

	terrain->Render();

	MyShader->Add_RegisterShaderCommand(gd.pCommandList, Shader::ShadowRegisterEnum::RenderWithShadow);

	XMFLOAT4X4 xmf4x4Projection;
	XMStoreFloat4x4(&xmf4x4Projection, XMMatrixTranspose(gd.viewportArr[0].ProjectMatrix));
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4Projection, 0);

	XMFLOAT4X4 xmf4x4View;
	XMStoreFloat4x4(&xmf4x4View, XMMatrixTranspose(MySpotLight.View));
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4View, 16);
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 32); // no matter

	gd.pCommandList->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.m_pDescritorHeap);
	//gd.pCommandList->SetGraphicsRootDescriptorTable(3, game.MyShadowMappingShader->svdp.m_pDescritorHeap->GetGPUDescriptorHandleForHeapStart());

	MyShader->SetShadowMapCommand(game.MySpotLight.renderDesc);

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

	game.MyPBRShader1->Add_RegisterShaderCommand(gd.pCommandList, Shader::ShadowRegisterEnum::RenderWithShadow);

	matrix view = gd.viewportArr[0].ViewMatrix;
	view *= gd.viewportArr[0].ProjectMatrix;
	view.transpose();
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &view, 0);
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16);
	gd.pCommandList->SetGraphicsRootConstantBufferView(2, game.LightCB_withShadowResource.resource->GetGPUVirtualAddress());
	gd.pCommandList->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.m_pDescritorHeap);
	game.MyPBRShader1->SetShadowMapCommand(game.MySpotLight.renderDesc);

	Hierarchy_Object* obj = Map->MapObjects[0];
	obj->Render_Inherit(XMMatrixIdentity());

	stackff += game.DeltaTime;

	//mirror Start
	
	vec4 rot = vec4(0, stackff, 0, 1);
	vec4 pos = vec4(0, 0, 0, 1);
	for (int i = 0; i < 16; ++i) {
		matrix mat = XMMatrixIdentity();
		mat = XMMatrixRotationY(stackff);
		mat.pos = MirrorPoses[i];
		mat.pos.x += 0.03f * cosf(-stackff);
		mat.pos.z += 0.03f * sinf(-stackff);
		mat.transpose();
		gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &mat, 0);
		TestMirrorMesh->Render(gd.pCommandList, 1);
	}
	
	gd.pCommandList->ClearDepthStencilView(d3dDsvCPUDescriptorHandle, D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);

	//mirror stencil render
	game.MyPBRShader1->Add_RegisterShaderCommand(gd.pCommandList, Shader::ShadowRegisterEnum::RenderStencil);
	gd.pCommandList->OMSetStencilRef(1);

	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &view, 0);
	int materialIndex = 0;
	Material& material = game.MaterialTable[materialIndex];
	gd.pCommandList->SetGraphicsRootConstantBufferView(2, game.LightCB_withShadowResource.resource->GetGPUVirtualAddress());
	gd.pCommandList->SetGraphicsRootDescriptorTable(3, material.hGPU);
	gd.pCommandList->SetGraphicsRootDescriptorTable(4, material.CB_Resource.hGpu);

	for (int i = 0; i < 16; ++i) {
		matrix mat2 = XMMatrixScaling(0.9f, 0.9f, 0.9f);
		mat2 *= XMMatrixRotationY(stackff);
		mat2.pos = MirrorPoses[i];
		mat2.transpose();
		gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &mat2, 0);
		TestMirrorMesh->Render(gd.pCommandList, 1);
	}

	gd.pCommandList->ClearDepthStencilView(d3dDsvCPUDescriptorHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);

	// render inner mirror;
	gd.pCommandList->OMSetStencilRef(1);
	game.MyPBRShader1->Add_RegisterShaderCommand(gd.pCommandList, Shader::ShadowRegisterEnum::RenderInnerMirror);

	int selection = 0;
	float minLen = 99999999999999;
	for (int i = 0; i < 16; ++i) {
		vec4 playerpos = player->m_worldMatrix.pos;
		playerpos.w = 0;
		float len = vec4(playerpos - MirrorPoses[i]).len3;
		if (len < minLen) {
			minLen = len;
			selection = i;
		}
	}
	mirrorPlane = XMPlaneFromPointNormal(MirrorPoses[selection], vec4(cosf(stackff), 0, sinf(stackff)));
	
	matrix reflect = XMMatrixReflect(mirrorPlane);
	matrix sav = gd.viewportArr[0].ViewMatrix;
	//gd.viewportArr[0].ViewMatrix = sav * reflect;

	view = gd.viewportArr[0].ViewMatrix;
	view *= gd.viewportArr[0].ProjectMatrix;
	view.transpose();
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &view, 0);
	//gd.viewportArr[0].UpdateFrustum();

	obj->Render_Inherit(reflect, Shader::ShadowRegisterEnum::RenderInnerMirror);
	//gd.viewportArr[0].ViewMatrix = sav;
	//gd.viewportArr[0].UpdateFrustum();

	((Shader*)MyOnlyColorShader)->Add_RegisterShaderCommand(gd.pCommandList);

	matrix proj = gd.viewportArr[0].ProjectMatrix;
	proj.transpose();
	view = gd.viewportArr[0].ViewMatrix;
	view.transpose();
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &proj, 0);
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &view, 16);

	for (int i = 0; i < bulletRays.size; ++i) {
		if (bulletRays.isnull(i)) continue;
		bulletRays[i].Render();
	}

	gd.pCommandList->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);

	//((Shader*)MyShader)->Add_RegisterShaderCommand(gd.pCommandList);

	// UI/AfterDepthClear Render
	game.player->Render_AfterDepthClear();

	MyScreenCharactorShader->Add_RegisterShaderCommand(gd.pCommandList, Shader::ShadowRegisterEnum::SDF);

	static float stackT = 0;
	stackT += game.DeltaTime;
	/*float minD[15] = {};
	float maxD[15] = {};
	for (int i = 0; i < 15; ++i) {
		float offset = i * 0.5f;
		minD[i] = 0.125f * (powf(sin(5*stackT + offset), 15) - 1.0f);
		maxD[i] = 1;
	}
	RenderSDFText(L"심장부\n Pulse City", 15, vec4(0, 0, 1000, 100), 50, vec4(0.8f, 0.2f, 0.2f, 1.0f), minD, maxD, 0.01f);*/

	MyScreenCharactorShader->Add_RegisterShaderCommand(gd.pCommandList);
	
	wchar_t KillCountStr[256] = {};
	wsprintfW(KillCountStr, L"KillCount : %d", KillCount);
	RenderText(KillCountStr, wcslen(KillCountStr), vec4(-1.0f * gd.ClientFrameWidth, -1.0f * gd.ClientFrameHeight + 100, 200, 100), 20 + 10 * KillFlow);
	KillFlow -= game.DeltaTime;
	if (KillFlow < 0) KillFlow = 0;

	//float MiniMapRootConstant[7] = { gd.ClientFrameWidth - 700, gd.ClientFrameHeight - 700,
	//gd.ClientFrameWidth, gd.ClientFrameHeight, gd.ClientFrameWidth, gd.ClientFrameHeight, 0.02f };
	//gd.pCommandList->SetGraphicsRoot32BitConstants(0, 7, MiniMapRootConstant, 0);

	//DescHandle miniMapHandle;
	//gd.ShaderVisibleDescPool.AllocDescriptorTable(&miniMapHandle.hcpu, &miniMapHandle.hgpu, 1);
	//gd.pDevice->CopyDescriptorsSimple(1, miniMapHandle.hcpu, MiniMapBackground.hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//gd.pCommandList->SetGraphicsRootDescriptorTable(1, miniMapHandle.hgpu);
	//TextMesh->Render(gd.pCommandList, 1);

	//constexpr float margin = 20.0f;

	////player Dot
	//float MiniMapDotRootConstant[7] = { gd.ClientFrameWidth - 350 - margin, gd.ClientFrameHeight - 350 - margin,
	//gd.ClientFrameWidth - 350 + margin, gd.ClientFrameHeight - 350 + margin , gd.ClientFrameWidth, gd.ClientFrameHeight, 0.01f };
	//gd.pCommandList->SetGraphicsRoot32BitConstants(0, 7, MiniMapDotRootConstant, 0);
	//DescHandle miniMapPDotHandle;
	//gd.ShaderVisibleDescPool.AllocDescriptorTable(&miniMapPDotHandle.hcpu, &miniMapPDotHandle.hgpu, 1);
	//gd.pDevice->CopyDescriptorsSimple(1, miniMapPDotHandle.hcpu, game.DefaultTex.hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//gd.pCommandList->SetGraphicsRootDescriptorTable(1, miniMapPDotHandle.hgpu);
	//TextMesh->Render(gd.pCommandList, 1);
	//
	////player_direction Dot
	//vec4 plook = player->m_worldMatrix.look;
	//plook.y = 0;
	//plook.len3 = 1;
	//float pdx = plook.x * 2 * margin;
	//float pdz = plook.z * 2 * margin;
	//float MiniMapDirDotRootConstant[7] = {gd.ClientFrameWidth - 350 - margin + pdx, gd.ClientFrameHeight - 350 - margin+pdz,
	//gd.ClientFrameWidth - 350 + margin+ pdx, gd.ClientFrameHeight - 350 + margin+pdz , gd.ClientFrameWidth, gd.ClientFrameHeight, 0.01f };
	//gd.pCommandList->SetGraphicsRoot32BitConstants(0, 7, MiniMapDirDotRootConstant, 0);
	//DescHandle miniMapPDIRDotHandle;
	//gd.ShaderVisibleDescPool.AllocDescriptorTable(&miniMapPDIRDotHandle.hcpu, &miniMapPDIRDotHandle.hgpu, 1);
	//gd.pDevice->CopyDescriptorsSimple(1, miniMapPDIRDotHandle.hcpu, game.DefaultTex.hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//gd.pCommandList->SetGraphicsRootDescriptorTable(1, miniMapPDIRDotHandle.hgpu);
	//TextMesh->Render(gd.pCommandList, 1);
	//
	//for (int i = 0; i < 20; ++i) {
	//	Monster* mon = MonsterStartPtr[i];
	//	vec4 pos = mon->m_worldMatrix.pos;
	//	pos -= player->m_worldMatrix.pos;
	//	pos *= MiniMapZoomRate;

	//	if (fabsf(pos.x) > 350 || fabsf(pos.z) > 350) continue;
	//	if (mon->is_dead) continue;

	//	float MiniMapDotRootConstant[7] = { gd.ClientFrameWidth - 350 + pos.x, gd.ClientFrameHeight - 350 + pos.z,
	//gd.ClientFrameWidth - 350 + pos.x + margin, gd.ClientFrameHeight - 350 + pos.z+margin , gd.ClientFrameWidth, gd.ClientFrameHeight, 0.01f };
	//	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 7, MiniMapDotRootConstant, 0);
	//	DescHandle miniMapDotHandle;
	//	gd.ShaderVisibleDescPool.AllocDescriptorTable(&miniMapDotHandle.hcpu, &miniMapDotHandle.hgpu, 1);
	//	gd.pDevice->CopyDescriptorsSimple(1, miniMapDotHandle.hcpu, DefaultNoramlTex.hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//	gd.pCommandList->SetGraphicsRootDescriptorTable(1, miniMapDotHandle.hgpu);
	//	TextMesh->Render(gd.pCommandList, 1);
	//}

	/*MyParticle.Render(false);
	MyParticle.Render(true);*/
	CD3DX12_RESOURCE_BARRIER Trans = CD3DX12_RESOURCE_BARRIER::Transition(gd.SubRenderTarget.resource/*gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex]*/, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	gd.pCommandList->ResourceBarrier(1, &Trans);

	//Command execution
	hResult = gd.CmdClose(gd.pCommandList);
	ID3D12CommandList* ppd3dCommandLists[] = { gd.pCommandList };
	gd.pCommandQueue->ExecuteCommandLists(1, ppd3dCommandLists);

	gd.WaitGPUComplete();

	//Bluring (Compute Shader)
	hResult = gd.pComputeCommandAllocator->Reset();
	hResult = gd.CmdReset(gd.pComputeCommandList, gd.pComputeCommandAllocator);

	game.MyComputeTestShader->Add_RegisterShaderCommand(gd.pComputeCommandList);
	gd.pComputeCommandList->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.m_pDescritorHeap);

	float WHarr[2] = { gd.ClientFrameWidth , gd.ClientFrameHeight };
	gd.pComputeCommandList->SetComputeRoot32BitConstants(0, 2, WHarr, 0); // screen width height
	gd.pComputeCommandList->SetComputeRootDescriptorTable(1, gd.SubRenderTarget.srvHandle.hgpu); // SRV for current
	gd.pComputeCommandList->SetComputeRootDescriptorTable(2, gd.BlurTexture.handle.hgpu); // UAV output
	int disPatchW = (gd.ClientFrameWidth / 8) + 1;
	int disPatchH = (gd.ClientFrameHeight / 8) + 1;
	gd.pComputeCommandList->Dispatch(disPatchW, disPatchH, 1);
	//Command execution
	hResult = gd.CmdClose(gd.pComputeCommandList);
	ID3D12CommandList* ppd3dCommandLists2[] = { gd.pComputeCommandList };
	gd.pComputeCommandQueue->ExecuteCommandLists(1, ppd3dCommandLists2);
	gd.WaitGPUComplete(gd.pComputeCommandQueue);

	// retuen to graphic command list. to copy resource to render back buffer;
	hResult = gd.pCommandAllocator->Reset();
	hResult = gd.CmdReset(gd.pCommandList, gd.pCommandAllocator);
	Trans = CD3DX12_RESOURCE_BARRIER::Transition(gd.BlurTexture.texture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	gd.pCommandList->ResourceBarrier(1, &Trans);

	Trans = CD3DX12_RESOURCE_BARRIER::Transition(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
	gd.pCommandList->ResourceBarrier(1, &Trans);

	gd.pCommandList->CopyResource(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], gd.BlurTexture.texture);

	Trans = CD3DX12_RESOURCE_BARRIER::Transition(gd.BlurTexture.texture, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	gd.pCommandList->ResourceBarrier(1, &Trans);

	Trans = CD3DX12_RESOURCE_BARRIER::Transition(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET); // this present.
	gd.pCommandList->ResourceBarrier(1, &Trans);

	//render end ----------------------------------------------------------------
	gd.pCommandList->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);

	//RenderTarget State Changing Command [RenderTarget -> Present] + wait untill finish rendering
	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE/*D3D12_RESOURCE_STATE_RENDER_TARGET*/;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	gd.pCommandList->ResourceBarrier(1, &d3dResourceBarrier);

	Trans = CD3DX12_RESOURCE_BARRIER::Transition(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT); // this present.
	gd.pCommandList->ResourceBarrier(1, &Trans);

	//Command execution
	hResult = gd.CmdClose(gd.pCommandList);;
	ID3D12CommandList* ppd3dCommandLists3[] = { gd.pCommandList };
	gd.pCommandQueue->ExecuteCommandLists(1, ppd3dCommandLists3);
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

	HRESULT hr = gd.pDevice->GetDeviceRemovedReason();
	cout << hr << endl;
}

void Game::Render_RayTracing()
{
	// Reset command list and allocator.
	ID3D12GraphicsCommandList4* commandList = gd.raytracing.dxrCommandList;

	gd.pCommandAllocator->Reset(); // origin : m_commandAllocators[m_backBufferIndex]->Reset()
	gd.CmdReset(commandList, gd.pCommandAllocator);

	// Transition the render target into the correct state to allow for drawing into it.
	D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	commandList->ResourceBarrier(1, &barrier);

	commandList->SetComputeRootSignature(MyRayTracingShader->pGlobalRootSignature);

	// Bind the heaps, acceleration structure and dispatch rays.    
	D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
	commandList->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.m_pDescritorHeap);
	commandList->SetComputeRootDescriptorTable(0, gd.raytracing.RTO_UAV_handle.hgpu); // sub render target, raytracing output
	commandList->SetComputeRootShaderResourceView(1, MyRayTracingShader->TLAS->GetGPUVirtualAddress()); // AS SRV
	commandList->SetComputeRootConstantBufferView(2, gd.raytracing.CameraCB->GetGPUVirtualAddress()); // Camera CB CBV
	commandList->SetComputeRootDescriptorTable(3, MyRayTracingShader->TestMesh->VBIB_DescHandle.hgpu); // Vertex, IndexBuffer SRV

	commandList->SetPipelineState1(MyRayTracingShader->RTPSO);

	// Since each shader table has only one shader record, the stride is same as the size.
	//size_t v = MyRayTracingShader->HitGroupShaderTable->GetGPUVirtualAddress();
	//v = (v + 63) & ~63;
	dispatchDesc.HitGroupTable.StartAddress = MyRayTracingShader->HitGroupShaderTable->GetGPUVirtualAddress();
	dispatchDesc.HitGroupTable.SizeInBytes = MyRayTracingShader->HitGroupShaderTable->GetDesc().Width;
	dispatchDesc.HitGroupTable.StrideInBytes = dispatchDesc.HitGroupTable.SizeInBytes;

	/*v = MyRayTracingShader->MissShaderTable->GetGPUVirtualAddress();
	v = (v + 63) & ~63;*/
	dispatchDesc.MissShaderTable.StartAddress = MyRayTracingShader->MissShaderTable->GetGPUVirtualAddress();
	dispatchDesc.MissShaderTable.SizeInBytes = MyRayTracingShader->MissShaderTable->GetDesc().Width;
	dispatchDesc.MissShaderTable.StrideInBytes = dispatchDesc.MissShaderTable.SizeInBytes;

	dispatchDesc.RayGenerationShaderRecord.StartAddress = MyRayTracingShader->RayGenShaderTable->GetGPUVirtualAddress();
	dispatchDesc.RayGenerationShaderRecord.SizeInBytes = MyRayTracingShader->RayGenShaderTable->GetDesc().Width;

	dispatchDesc.Width = gd.ClientFrameWidth;
	dispatchDesc.Height = gd.ClientFrameHeight;
	dispatchDesc.Depth = 1;
	commandList->DispatchRays(&dispatchDesc);

	// copy to rendertarget
	D3D12_RESOURCE_BARRIER preCopyBarriers[2];
	preCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
	preCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(gd.raytracing.RayTracingOutput, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);

	commandList->CopyResource(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], gd.raytracing.RayTracingOutput);

	D3D12_RESOURCE_BARRIER postCopyBarriers[2];
	postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(gd.ppRenderTargetBuffers[gd.CurrentSwapChainBufferIndex], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
	postCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(gd.raytracing.RayTracingOutput, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(ARRAYSIZE(postCopyBarriers), postCopyBarriers);

	//Execute
	gd.CmdClose(commandList);
	ID3D12CommandList* commandLists[] = { commandList };
	gd.pCommandQueue->ExecuteCommandLists(ARRAYSIZE(commandLists), commandLists);

	gd.WaitGPUComplete();

	gd.pSwapChain->Present(1, 0);

	//Get Present RenderTarget Index
	gd.CurrentSwapChainBufferIndex = gd.pSwapChain->GetCurrentBackBufferIndex();
}

void Game::Render_ShadowPass()
{
	constexpr int ShadowResolusion = 4096;
	vec4 LightDirection = vec4(1, 2, 1);
	LightDirection.len3 = 1;
	constexpr float LightDistance = 500;
	game.MySpotLight.viewport.Viewport.Width = ShadowResolusion;
	game.MySpotLight.viewport.Viewport.Height = ShadowResolusion;
	game.MySpotLight.viewport.Viewport.MaxDepth = 1.0f;
	game.MySpotLight.viewport.Viewport.MinDepth = 0.0f;
	game.MySpotLight.viewport.Viewport.TopLeftX = 0.0f;
	game.MySpotLight.viewport.Viewport.TopLeftY = 0.0f;
	game.MySpotLight.viewport.ScissorRect = { 0, 0, (long)ShadowResolusion, (long)ShadowResolusion };

	vec4 obj = player->m_worldMatrix.pos;
	//vec4 dir = player->m_worldMatrix.look;
	//dir.y = 0;
	//dir.len3 = 1;
	//obj += dir * 10;
	//obj.y = player->m_worldMatrix.pos.y;
	obj.w = 0;

	game.MySpotLight.viewport.Camera_Pos = obj + LightDirection * LightDistance;
	game.MySpotLight.viewport.Camera_Pos.w = 0;
	game.MySpotLight.LightPos = game.MySpotLight.viewport.Camera_Pos;
	
	MySpotLight.View.mat = XMMatrixLookAtLH(MySpotLight.LightPos, obj, vec4(0, 1, 0, 0));
	game.MySpotLight.viewport.ViewMatrix = MySpotLight.View;

	//
	constexpr float rate = 1.0f / 8.0f;
	game.MySpotLight.viewport.ProjectMatrix = XMMatrixOrthographicLH(rate*ShadowResolusion, rate*ShadowResolusion, 0.1f, 1000.0f);
		//XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), (float)gd.ClientFrameWidth / gd.ClientFrameHeight, 0.01f, 1500.0f);
		//XMMatrixOrthographicLH(ShadowResolusion, ShadowResolusion, 0.01f, 1000.0f);
		//gd.viewportArr[0].ProjectMatrix;/*XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), (float)1.0f, 0.01f, 1000.0f);*/
	//XMMatrixOrthographicLH(ShadowResolusion, ShadowResolusion, 0.01f, 1000.0f);
	//XMMatrixOrthographicOffCenterLH(0, 10, 0, 10, 0.1f, 100.0f);
	//XMMatrixOrthographicLH(ShadowResolusion, ShadowResolusion, 0.01f, 1000.0f);

	matrix projmat = XMMatrixTranspose(MySpotLight.viewport.ProjectMatrix);
	LightCB_withShadowResource.resource->Map(0, NULL, (void**)&LightCBData_withShadow);
	LightCBData_withShadow->LightProjection = projmat;
	LightCBData_withShadow->LightView = XMMatrixTranspose(MySpotLight.viewport.ViewMatrix);
	LightCBData_withShadow->LightPos = MySpotLight.LightPos.f3;
	LightCB_withShadowResource.resource->Unmap(0, nullptr);

	HRESULT hResult = gd.pCommandAllocator->Reset();
	hResult = gd.CmdReset(gd.pCommandList, gd.pCommandAllocator);

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

	gd.pCommandList->RSSetViewports(1, &game.MySpotLight.viewport.Viewport);
	gd.pCommandList->RSSetScissorRects(1, &game.MySpotLight.viewport.ScissorRect);

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
	
	matrix xmf4x4Projection = game.MySpotLight.viewport.ProjectMatrix;
	xmf4x4Projection.transpose();
	//XMStoreFloat4x4(&xmf4x4Projection, XMMatrixTranspose(gd.viewportArr[0].ProjectMatrix));
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4Projection, 0);

	matrix xmf4x4View;
	xmf4x4View = MySpotLight.View;
	xmf4x4View.transpose();
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4View, 16);
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 32); // no matter

	for (auto& gbj : m_gameObjects) {
		gbj->RenderShadowMap();
	}

	game.MyPBRShader1->Add_RegisterShaderCommand(gd.pCommandList, Shader::ShadowRegisterEnum::RenderShadowMap);

	xmf4x4View = game.MySpotLight.viewport.ViewMatrix;
	xmf4x4View *= game.MySpotLight.viewport.ProjectMatrix;
	xmf4x4View.transpose();
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4View, 0);
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16); // no matter

	gd.pCommandList->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.m_pDescritorHeap);

	game.Map->MapObjects[0]->Render_Inherit(XMMatrixIdentity(), Shader::ShadowRegisterEnum::RenderShadowMap);

	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	gd.pCommandList->ResourceBarrier(1, &d3dResourceBarrier);

	gd.CmdClose(gd.pCommandList);
	ID3D12CommandList* ppd3dCommandLists_Shadow[] = { gd.pCommandList };
	gd.pCommandQueue->ExecuteCommandLists(1, ppd3dCommandLists_Shadow);

	gd.WaitGPUComplete();
}

void Game::Update()
{
	if (gd.isRaytracingRender) {
		float elapsedTime = game.DeltaTime;

		// Rotate the camera around Y axis.
		{
			float secondsToRotateAround = 24.0f;
			float angleToRotateBy = 360.0f * (elapsedTime / secondsToRotateAround);
			XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(angleToRotateBy));
			gd.raytracing.m_eye = XMVector3Transform(gd.raytracing.m_eye, rotate);
			gd.raytracing.m_up = XMVector3Transform(gd.raytracing.m_up, rotate);
			gd.raytracing.m_at = XMVector3Transform(gd.raytracing.m_at, rotate);

			gd.raytracing.MappedCB->cameraPosition = gd.raytracing.m_eye;
			float fovAngleY = 45.0f;
			XMMATRIX view = XMMatrixLookAtLH(gd.raytracing.m_eye, gd.raytracing.m_at, gd.raytracing.m_up);
			float m_aspectRatio = (float)gd.ClientFrameWidth / (float)gd.ClientFrameHeight;
			XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(fovAngleY), m_aspectRatio, 1.0f, 125.0f);
			XMMATRIX viewProj = view * proj;

			gd.raytracing.MappedCB->projectionToWorld = XMMatrixTranspose(XMMatrixInverse(nullptr, viewProj));

			float secondsToRotateAround2 = 8.0f;
			float angleToRotateBy2 = -360.0f * (elapsedTime / secondsToRotateAround2);
			XMMATRIX rotate2 = XMMatrixRotationY(XMConvertToRadians(angleToRotateBy2));
			const XMVECTOR& prevLightPosition = gd.raytracing.MappedCB->lightPosition;
			gd.raytracing.MappedCB->lightPosition = XMVector3Transform(prevLightPosition, rotate2);
		}
	}

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
	//static float StackTime0 = 0;
	//StackTime0 += game.DeltaTime;
	//MySpotLight.LightPos = //gd.viewportArr[0].Camera_Pos;
	//	vec4(10*cosf(StackTime0/10), 10, 10 * sinf(StackTime0/10));
	//MySpotLight.View.mat = //gd.viewportArr[0].ViewMatrix.mat;
	//	XMMatrixLookAtLH(MySpotLight.LightPos, vec4(0, 0, 0, 0), vec4(0, 1, 0, 0));  
		
		//gd.viewportArr[0].ViewMatrix;//XMMatrixLookAtLH(MySpotLight.LightPos, vec4(0, 0, 0, 0), vec4(0, 1, 0, 0));

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
	if (evt.Message == WM_KEYDOWN && evt.wParam == VK_F8) {
		gd.isRaytracingRender = !gd.isRaytracingRender;
	}

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

	Guntex.Release();
	bulletRays.Release();
	DefaultTex.Release();
	DefaultNoramlTex.Release();
	DefaultAmbientTex.Release();
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
		closestHitObject->OnLazerHit();
	}
}

//this function must call after ScreenCharactorShader register in pipeline.
void Game::RenderText(const wchar_t* wstr, int length, vec4 Rect, float fontsiz, float depth)
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
		float tConst[7] = { textRt.x, textRt.y, textRt.z, textRt.w, gd.ClientFrameWidth , gd.ClientFrameHeight, depth };
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

void Game::RenderSDFText(const wchar_t* wstr, int length, vec4 Rect, float fontsiz, vec4 color, float* minD, float* maxD, float depth)
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
			if (gd.font_sdftexture_map[k].find(wc) != gd.font_sdftexture_map[k].end()) {
				textureExist = true;
				texture = &gd.font_sdftexture_map[k][wc];
				g = gd.font_data[k].glyphs[wc];
				break;
			}
		}

		if (textureExist == false) {
			gd.addSDFTextureStack.push_back(wc);
			continue;
		}

		//set root variables
		vec4 textRt = vec4(pos.x + g.bounding_box[0] * mul, pos.y + g.bounding_box[1] * mul, pos.x + g.bounding_box[2] * mul, pos.y + g.bounding_box[3] * mul);
		float tConst[14] = { textRt.x, textRt.y, textRt.z, textRt.w, gd.ClientFrameWidth , gd.ClientFrameHeight, depth, 0, color.x, color.y, color.z, color.w, minD[i], maxD[i]};
		/*gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &textRt, 0);
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 1, &gd.ClientFrameWidth, 4);
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 1, &gd.ClientFrameHeight, 5);*/
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 14, &tConst, 0);
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

void Game::bmpTodds(int mipmap_level, const char* Format, const char* filename)
{
	string cmd = "D3DTexConv\\texconv.exe -m ";
	cmd += to_string(mipmap_level);
	cmd += " -f ";
	cmd += Format;
	cmd += " ";
	cmd += filename;
	int result = system(cmd.c_str());
	cout << result << endl;
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

void Shader::CreateRootSignature_Terrain()
{
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
	gPipelineStateDesc.RasterizerState.ForcedSampleCount = 0; // sample count of UAV rendering 
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
	gPipelineStateDesc.RasterizerState.ForcedSampleCount = 0; // sample count of UAV rendering 
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

void Shader::CreatePipelineState_RenderStencil()
{
}

void Shader::CreatePipelineState_InnerMirror()
{
}

void Shader::CreatePipelineState_Terrain()
{
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

#ifdef PIX_DEBUGING
	nCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif // !PIX_DEBUGING

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

void Shader::SetShadowMapCommand(DescHandle shadowMapDesc)
{
	gd.pCommandList->SetGraphicsRootDescriptorTable(3, shadowMapDesc.hgpu);
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
	gPipelineStateDesc.RasterizerState.ForcedSampleCount = 0; // sample count of UAV rendering
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
	CreateRootSignature_SDF();

	CreatePipelineState();
	CreatePipelineState_SDF();
}

void ScreenCharactorShader::CreateRootSignature()
{
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;

	D3D12_ROOT_PARAMETER1 rootParam[2] = {};

	rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParam[0].Constants.Num32BitValues = 7; // rect(4) + screenWidht, screenHeight + depth
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
	d3dBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	d3dBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	d3dBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	d3dBlendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	d3dBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	d3dBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
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

void ScreenCharactorShader::CreateRootSignature_SDF()
{
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;

	D3D12_ROOT_PARAMETER1 rootParam[2] = {};

	rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParam[0].Constants.Num32BitValues = 14; // rect(4) + screenWidht, screenHeight  + depth + pad + Color4 + minD + maxD
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
			**)&pRootSignature_SDF);
	if (pd3dErrorBlob) pd3dErrorBlob->Release();
	if (pd3dSignatureBlob) pd3dSignatureBlob->Release();
}

void ScreenCharactorShader::CreatePipelineState_SDF()
{
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dVertexShaderBlob = NULL, * pd3dPixelShaderBlob = NULL;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature_SDF;

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
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/ScreenSDFCharactorRenderShader.hlsl", "VSMain", "vs_5_1", &pd3dVertexShaderBlob);

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
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/ScreenSDFCharactorRenderShader.hlsl", "PSMain", "ps_5_1", &pd3dPixelShaderBlob);

	//Output Merger
	//Output Merger-Blend
	D3D12_BLEND_DESC d3dBlendDesc;
	::ZeroMemory(&d3dBlendDesc, sizeof(D3D12_BLEND_DESC));
	d3dBlendDesc.AlphaToCoverageEnable = 0;
	d3dBlendDesc.IndependentBlendEnable = 0;
	d3dBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	d3dBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	d3dBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	d3dBlendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	d3dBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	d3dBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
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
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState_SDF);
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

void ScreenCharactorShader::Add_RegisterShaderCommand(ID3D12GraphicsCommandList* commandList, Shader::ShadowRegisterEnum reg) {
	if (reg == Shader::ShadowRegisterEnum::SDF) {
		commandList->SetGraphicsRootSignature(pRootSignature_SDF);
		commandList->SetPipelineState(pPipelineState_SDF);
	}
	else {
		commandList->SetGraphicsRootSignature(pRootSignature);
		commandList->SetPipelineState(pPipelineState);
	}
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
	
	gd.ShaderVisibleDescPool.ImmortalAllocDescriptorTable(&game.MySpotLight.renderDesc.hcpu, &game.MySpotLight.renderDesc.hgpu, 1);

	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
	srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
	srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv_desc.Texture2D.MostDetailedMip = 0;
	srv_desc.Texture2D.MipLevels = 1;
	srv_desc.Texture2D.ResourceMinLODClamp = 0;
	srv_desc.Texture2D.PlaneSlice = 0;
	gd.pDevice->CreateShaderResourceView(shadowMap.resource, &srv_desc, game.MySpotLight.renderDesc.hcpu);
	shadowMap.hGpu = game.MySpotLight.renderDesc.hgpu; // CBV, SRV, UAV DescHeap 의 GPU HANDLE
	return shadowMap;
}

void PBRShader1::InitShader()
{
	CreateRootSignature();
	CreateRootSignature_withShadow();
	CreateRootSignature_Terrain();
	CreateRootSignature_TesslationTerrain();

	CreatePipelineState();
	CreatePipelineState_withShadow();
	CreatePipelineState_RenderShadowMap();
	CreatePipelineState_RenderStencil();
	CreatePipelineState_InnerMirror();
	CreatePipelineState_Terrain();
	CreatePipelineState_TesslationTerrain();
}

void PBRShader1::CreateRootSignature()
{
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;

	D3D12_ROOT_PARAMETER1 rootParam[5] = {};

	rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParam[0].Constants.Num32BitValues = 16 + 4;
	rootParam[0].Constants.ShaderRegister = 0; // b0 : Camera Matrix (Projection + View) + Camera Positon
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
	D3D12_DESCRIPTOR_RANGE1 ranges[1];
	rootParam[3].DescriptorTable.NumDescriptorRanges = sizeof(ranges) / sizeof(D3D12_DESCRIPTOR_RANGE1);
	
	ranges[0].BaseShaderRegister = 0; // t0 //Diffuse Texture
	ranges[0].RegisterSpace = 0;
	ranges[0].NumDescriptors = 5; // t0 t1 t2 t3 t4 -> must be Continuous Desc in DescHeap
	ranges[0].OffsetInDescriptorsFromTableStart = 0;
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	rootParam[3].DescriptorTable.pDescriptorRanges = &ranges[0];
	
	rootParam[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	D3D12_DESCRIPTOR_RANGE1 ranges2[1];
	rootParam[4].DescriptorTable.NumDescriptorRanges = sizeof(ranges2) / sizeof(D3D12_DESCRIPTOR_RANGE1);
	ranges2[0].BaseShaderRegister = 3;
	ranges2[0].RegisterSpace = 0;
	ranges2[0].NumDescriptors = 1; // b3
	ranges2[0].OffsetInDescriptorsFromTableStart = 0;
	ranges2[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	ranges2[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	rootParam[4].DescriptorTable.pDescriptorRanges = &ranges2[0];

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

	rootSigDesc1.NumParameters = sizeof(rootParam) / sizeof(D3D12_ROOT_PARAMETER1);;
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

void PBRShader1::CreateRootSignature_withShadow()
{
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;

	D3D12_ROOT_PARAMETER1 rootParam[6] = {};

	rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParam[0].Constants.Num32BitValues = 16 + 4;
	rootParam[0].Constants.ShaderRegister = 0; // b0 : Camera Matrix (Projection + View) + Camera Positon
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
	D3D12_DESCRIPTOR_RANGE1 ranges[1];
	rootParam[3].DescriptorTable.NumDescriptorRanges = sizeof(ranges) / sizeof(D3D12_DESCRIPTOR_RANGE1);

	ranges[0].BaseShaderRegister = 0; // t0 //Diffuse Texture
	ranges[0].RegisterSpace = 0;
	ranges[0].NumDescriptors = 5; // t0 t1 t2 t3 t4 -> must be Continuous Desc in DescHeap
	ranges[0].OffsetInDescriptorsFromTableStart = 0;
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	rootParam[3].DescriptorTable.pDescriptorRanges = &ranges[0];

	//Material
	rootParam[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	D3D12_DESCRIPTOR_RANGE1 ranges2[1];
	rootParam[4].DescriptorTable.NumDescriptorRanges = sizeof(ranges2) / sizeof(D3D12_DESCRIPTOR_RANGE1);
	ranges2[0].BaseShaderRegister = 3;
	ranges2[0].RegisterSpace = 0;
	ranges2[0].NumDescriptors = 1; // b3
	ranges2[0].OffsetInDescriptorsFromTableStart = 0;
	ranges2[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	ranges2[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	rootParam[4].DescriptorTable.pDescriptorRanges = &ranges2[0];

	rootParam[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParam[5].DescriptorTable.NumDescriptorRanges = 1;
	D3D12_DESCRIPTOR_RANGE1 ranges3[1];
	ranges3[0].BaseShaderRegister = 5; // t0 //ShadowTexture
	ranges3[0].RegisterSpace = 0;
	ranges3[0].NumDescriptors = 1; // t5
	ranges3[0].OffsetInDescriptorsFromTableStart = 0;
	ranges3[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges3[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	rootParam[5].DescriptorTable.pDescriptorRanges = &ranges3[0];

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
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	sampler.MinLOD = 0;
	sampler.MaxLOD = 20;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	rootSigDesc1.NumParameters = sizeof(rootParam) / sizeof(D3D12_ROOT_PARAMETER1);
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
			**)&pRootSignature_withShadow);
	if (pd3dErrorBlob) pd3dErrorBlob->Release();
	if (pd3dSignatureBlob) pd3dSignatureBlob->Release();
}

void PBRShader1::CreateRootSignature_Terrain()
{
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;

	D3D12_ROOT_PARAMETER1 rootParam[3] = {};

	rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParam[0].Constants.Num32BitValues = 16 + 4;
	rootParam[0].Constants.ShaderRegister = 0; // b0 : Camera Matrix (Projection + View) + Camera Positon
	rootParam[0].Constants.RegisterSpace = 0;

	rootParam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParam[1].Constants.Num32BitValues = 21;
	rootParam[1].Constants.ShaderRegister = 1; // b1 : Transform Matrix
	rootParam[1].Constants.RegisterSpace = 0;

	rootParam[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; //Static Light
	rootParam[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParam[2].Descriptor.ShaderRegister = 2; // b2
	rootParam[2].Descriptor.RegisterSpace = 0;

	//rootParam[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	//rootParam[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	//D3D12_DESCRIPTOR_RANGE1 ranges[1];
	//rootParam[3].DescriptorTable.NumDescriptorRanges = sizeof(ranges) / sizeof(D3D12_DESCRIPTOR_RANGE1);
	//ranges[0].BaseShaderRegister = 0; // t0 //Diffuse Texture
	//ranges[0].RegisterSpace = 0;
	//ranges[0].NumDescriptors = 1; // t0 t1 t2 t3 t4 -> must be Continuous Desc in DescHeap
	//ranges[0].OffsetInDescriptorsFromTableStart = 0;
	//ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	//ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	//rootParam[3].DescriptorTable.pDescriptorRanges = &ranges[0];

	//rootParam[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	//rootParam[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	//D3D12_DESCRIPTOR_RANGE1 ranges2[1];
	//rootParam[4].DescriptorTable.NumDescriptorRanges = sizeof(ranges2) / sizeof(D3D12_DESCRIPTOR_RANGE1);
	//ranges2[0].BaseShaderRegister = 1;
	//ranges2[0].RegisterSpace = 0;
	//ranges2[0].NumDescriptors = 1;
	//ranges2[0].OffsetInDescriptorsFromTableStart = 0;
	//ranges2[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	//ranges2[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	//rootParam[4].DescriptorTable.pDescriptorRanges = &ranges2[0];

	//rootParam[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	//rootParam[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	//D3D12_DESCRIPTOR_RANGE1 ranges3[1];
	//rootParam[5].DescriptorTable.NumDescriptorRanges = sizeof(ranges3) / sizeof(D3D12_DESCRIPTOR_RANGE1);
	//ranges3[0].BaseShaderRegister = 2; // t0 //ShadowTexture
	//ranges3[0].RegisterSpace = 0;
	//ranges3[0].NumDescriptors = 1; // t5
	//ranges3[0].OffsetInDescriptorsFromTableStart = 0;
	//ranges3[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	//ranges3[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	//rootParam[5].DescriptorTable.pDescriptorRanges = &ranges3[0];

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

	rootSigDesc1.NumParameters = sizeof(rootParam) / sizeof(D3D12_ROOT_PARAMETER1);
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
			**)&pRootSignature_Terrain);
	if (pd3dErrorBlob) pd3dErrorBlob->Release();
	if (pd3dSignatureBlob) pd3dSignatureBlob->Release();
	
}

void PBRShader1::CreateRootSignature_TesslationTerrain()
{
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;

	constexpr int RootParamCount = 7;
	D3D12_ROOT_PARAMETER1 rootParam[RootParamCount] = {};

	rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParam[0].Constants.Num32BitValues = 16 + 4;
	rootParam[0].Constants.ShaderRegister = 0; // b0 : Camera Matrix (Projection + View) + Camera Positon
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
	D3D12_DESCRIPTOR_RANGE1 ranges[1];
	rootParam[3].DescriptorTable.NumDescriptorRanges = sizeof(ranges) / sizeof(D3D12_DESCRIPTOR_RANGE1);

	ranges[0].BaseShaderRegister = 0; // t0 //Diffuse Texture
	ranges[0].RegisterSpace = 0;
	ranges[0].NumDescriptors = 5; // t0 t1 t2 t3 t4 -> must be Continuous Desc in DescHeap
	ranges[0].OffsetInDescriptorsFromTableStart = 0;
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	rootParam[3].DescriptorTable.pDescriptorRanges = &ranges[0];

	//Material
	rootParam[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	D3D12_DESCRIPTOR_RANGE1 ranges2[1];
	rootParam[4].DescriptorTable.NumDescriptorRanges = sizeof(ranges2) / sizeof(D3D12_DESCRIPTOR_RANGE1);
	ranges2[0].BaseShaderRegister = 3;
	ranges2[0].RegisterSpace = 0;
	ranges2[0].NumDescriptors = 1; // b3
	ranges2[0].OffsetInDescriptorsFromTableStart = 0;
	ranges2[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	ranges2[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	rootParam[4].DescriptorTable.pDescriptorRanges = &ranges2[0];

	rootParam[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParam[5].DescriptorTable.NumDescriptorRanges = 1;
	D3D12_DESCRIPTOR_RANGE1 ranges3[1];
	ranges3[0].BaseShaderRegister = 5; // t0 //ShadowTexture
	ranges3[0].RegisterSpace = 0;
	ranges3[0].NumDescriptors = 1; // t5
	ranges3[0].OffsetInDescriptorsFromTableStart = 0;
	ranges3[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges3[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	rootParam[5].DescriptorTable.pDescriptorRanges = &ranges3[0];

	rootParam[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_DOMAIN;
	rootParam[6].DescriptorTable.NumDescriptorRanges = 1;
	D3D12_DESCRIPTOR_RANGE1 ranges4[1];
	ranges4[0].BaseShaderRegister = 6; // t6 //Terrain Height Map
	ranges4[0].RegisterSpace = 0;
	ranges4[0].NumDescriptors = 1; // t6
	ranges4[0].OffsetInDescriptorsFromTableStart = 0;
	ranges4[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges4[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	rootParam[6].DescriptorTable.pDescriptorRanges = &ranges4[0];

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
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	sampler.MinLOD = 0;
	sampler.MaxLOD = 20;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootSigDesc1.NumParameters = RootParamCount;
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
			**)&pRootSignature_TessTerrain);
	if (pd3dErrorBlob) pd3dErrorBlob->Release();
	if (pd3dSignatureBlob) pd3dSignatureBlob->Release();
}

void PBRShader1::CreatePipelineState()
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
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // normal vec3
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},// uv vec2
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		// float3 tangent
	};
	gPipelineStateDesc.InputLayout.NumElements = sizeof(inputElementDesc) / sizeof(D3D12_INPUT_ELEMENT_DESC);
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01.hlsl", "VSMain", "vs_5_1", &pd3dVertexShaderBlob);

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
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01.hlsl", "PSMain", "ps_5_1", &pd3dPixelShaderBlob);

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

void PBRShader1::CreatePipelineState_withShadow()
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
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[4] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // normal vec3
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},// uv vec2
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		// float3 tangent
	};
	gPipelineStateDesc.InputLayout.NumElements = sizeof(inputElementDesc) / sizeof(D3D12_INPUT_ELEMENT_DESC);
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01_withShadow.hlsl", "VSMain", "vs_5_1", &pd3dVertexShaderBlob);

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
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01_withShadow.hlsl", "PSMain", "ps_5_1", &pd3dPixelShaderBlob);

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

void PBRShader1::CreatePipelineState_RenderShadowMap()
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
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[5] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // normal vec3
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},// uv vec2
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		// float3 tangent
		{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
		// float3 bitangent
	};
	gPipelineStateDesc.InputLayout.NumElements = 5;
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01.hlsl", "VSRenderShadow", "vs_5_1", &pd3dVertexShaderBlob);

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
	gPipelineStateDesc.RasterizerState.DepthBias = 0; // Depth = Depth + DepthBias (pixel space?)
	// Depth = DepthBias * 2^k + SlopeScaledDepthBias * MaxDepthSlope


	gPipelineStateDesc.RasterizerState.DepthBiasClamp = 0; // maximun of depth bias
	
	gPipelineStateDesc.RasterizerState.SlopeScaledDepthBias = 0;
	// in gpu, according to slope of mesh, calculate dynamic bias.

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

void PBRShader1::CreatePipelineState_RenderStencil()
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
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[5] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // normal vec3
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},// uv vec2
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		// float3 tangent
		{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
		// float3 bitangent
	};
	gPipelineStateDesc.InputLayout.NumElements = 5;
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01.hlsl", "VSMain", "vs_5_1", &pd3dVertexShaderBlob);

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
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01.hlsl", "PSMain", "ps_5_1", &pd3dPixelShaderBlob);

	//Output Merger
	//Output Merger-Blend
	D3D12_BLEND_DESC d3dBlendDesc;
	::ZeroMemory(&d3dBlendDesc, sizeof(D3D12_BLEND_DESC));
	d3dBlendDesc.AlphaToCoverageEnable = FALSE;
	d3dBlendDesc.IndependentBlendEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].BlendEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	
	d3dBlendDesc.RenderTarget[0].RenderTargetWriteMask = 0;
	//do not update render target

	gPipelineStateDesc.BlendState = d3dBlendDesc;
	//Output Merger - MSAA
	gPipelineStateDesc.SampleDesc.Count = (gd.m_bMsaa4xEnable) ? 4 : 1;
	gPipelineStateDesc.SampleDesc.Quality = (gd.m_bMsaa4xEnable) ? (gd.m_nMsaa4xQualityLevels - 1) : 0;
	gPipelineStateDesc.SampleMask = 0xFFFFFFFF; // pass every sampling.
	//Output Merger - DepthStencil
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc; // D3D12_DEPTH_STENCIL_DESC1 is using for Stream Pipeline state.. -> what is that?
	//Output Merger - DepthStencil - depth
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	//depthStencilDesc.DepthBoundsTestEnable = FALSE; // question 004 : what is this??
	//Output Merger - DepthStencil - stencil
	depthStencilDesc.StencilEnable = TRUE;
	depthStencilDesc.StencilReadMask = 0x00;
	depthStencilDesc.StencilWriteMask = 0xFF;
	depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	
	depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	depthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;

	depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	gPipelineStateDesc.DepthStencilState = depthStencilDesc;
	//Output Merger - RenderTagets / DepthStencil Buffer
	gPipelineStateDesc.NumRenderTargets = 1;
	gPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	gd.pDevice->CreateGraphicsPipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState_RenderStencil);
	if (pd3dVertexShaderBlob) pd3dVertexShaderBlob->Release();
	if (pd3dPixelShaderBlob) pd3dPixelShaderBlob->Release();

	//
}

void PBRShader1::CreatePipelineState_InnerMirror()
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
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[5] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // normal vec3
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},// uv vec2
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		// float3 tangent
		{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
		// float3 bitangent
	};
	gPipelineStateDesc.InputLayout.NumElements = 5;
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01.hlsl", "VSMain", "vs_5_1", &pd3dVertexShaderBlob);

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
	gPipelineStateDesc.RasterizerState.FrontCounterClockwise = TRUE; // front = clock wise
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
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01.hlsl", "PSMain", "ps_5_1", &pd3dPixelShaderBlob);

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
	depthStencilDesc.StencilEnable = TRUE;
	depthStencilDesc.StencilReadMask = 0xFF;
	depthStencilDesc.StencilWriteMask = 0xFF;
	depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
	depthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
	gPipelineStateDesc.DepthStencilState = depthStencilDesc;
	//Output Merger - RenderTagets / DepthStencil Buffer
	gPipelineStateDesc.NumRenderTargets = 1;
	gPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	gd.pDevice->CreateGraphicsPipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState_RenderInnerMirror);
	if (pd3dVertexShaderBlob) pd3dVertexShaderBlob->Release();
	if (pd3dPixelShaderBlob) pd3dPixelShaderBlob->Release();
}

void PBRShader1::CreatePipelineState_Terrain()
{
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dVertexShaderBlob = NULL, * pd3dPixelShaderBlob = NULL;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature_Terrain;

	gPipelineStateDesc.NodeMask = 0;

	//Input Asm
	gPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[4] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // normal vec3
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},// uv vec2
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		// float3 tangent
	};
	gPipelineStateDesc.InputLayout.NumElements = sizeof(inputElementDesc) / sizeof(D3D12_INPUT_ELEMENT_DESC);
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01_TerrainWithShadow.hlsl", "VSMain", "vs_5_1", &pd3dVertexShaderBlob);

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
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01_TerrainWithShadow.hlsl", "PSMain", "ps_5_1", &pd3dPixelShaderBlob);

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
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState_Terrain);
	if (pd3dVertexShaderBlob) pd3dVertexShaderBlob->Release();
	if (pd3dPixelShaderBlob) pd3dPixelShaderBlob->Release();
}

void PBRShader1::CreatePipelineState_TesslationTerrain()
{
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dVertexShaderBlob = NULL, * pd3dPixelShaderBlob = NULL;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature_TessTerrain;

	gPipelineStateDesc.NodeMask = 0;

	//Input Asm
	gPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[4] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // normal vec3
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},// uv vec2
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		// float3 tangent
	};
	gPipelineStateDesc.InputLayout.NumElements = sizeof(inputElementDesc) / sizeof(D3D12_INPUT_ELEMENT_DESC);
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01_withShadow.hlsl", "TessTerrainVSMain", "vs_5_1", &pd3dVertexShaderBlob);

	//Hull Shader
	gPipelineStateDesc.HS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01_withShadow.hlsl", "HS_Main", "hs_5_1", &pd3dVertexShaderBlob);

	//Tessellation

	//Domain Shader
	gPipelineStateDesc.DS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01_withShadow.hlsl", "DS_Main", "ds_5_1", &pd3dVertexShaderBlob);

	//Geometry Shader
	//gPipelineStateDesc.GS = NULLCODE;

	//Stream Output
	//gPipelineStateDesc.StreamOutput

	//Rasterazer
	gPipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;//D3D12_FILL_MODE_SOLID;
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
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01_withShadow.hlsl", "TessTerrainPSMain", "ps_5_1", &pd3dPixelShaderBlob);

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
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState_TessTerrain);
	if (pd3dVertexShaderBlob) pd3dVertexShaderBlob->Release();
	if (pd3dPixelShaderBlob) pd3dPixelShaderBlob->Release();
}

void PBRShader1::Release()
{
	if (pPipelineState) pPipelineState->Release();
	if (pRootSignature) pRootSignature->Release();
}

void PBRShader1::Add_RegisterShaderCommand(ID3D12GraphicsCommandList* commandList, Shader::ShadowRegisterEnum reg)
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
	case ShadowRegisterEnum::RenderStencil:
		commandList->SetPipelineState(pPipelineState_RenderStencil);
		commandList->SetGraphicsRootSignature(pRootSignature);
		return;
	case ShadowRegisterEnum::RenderInnerMirror:
		commandList->SetPipelineState(pPipelineState_RenderInnerMirror);
		commandList->SetGraphicsRootSignature(pRootSignature);
		return;
	case ShadowRegisterEnum::RenderTerrain:
		commandList->SetPipelineState(pPipelineState_Terrain);
		commandList->SetGraphicsRootSignature(pRootSignature_Terrain);
		return;
	case ShadowRegisterEnum::TessTerrain:
		commandList->SetPipelineState(pPipelineState_TessTerrain);
		commandList->SetGraphicsRootSignature(pRootSignature_TessTerrain);
		return;
	}
}

void PBRShader1::SetTextureCommand(GPUResource* Color, GPUResource* Normal, GPUResource* AO, GPUResource* Metalic, GPUResource* Roughness)
{
	DescHandle hDesc;

	gd.ShaderVisibleDescPool.AllocDescriptorTable(&hDesc.hcpu, &hDesc.hgpu, 5);
	DescHandle hStart = hDesc;

	unsigned long long inc = gd.CBV_SRV_UAV_Desc_IncrementSiz;

	//if (gd.ShaderVisibleDescPool.IncludeHandle(Color->hCpu) == false) {
	//	
	//}

	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, Color->hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	hDesc += inc;
	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, Normal->hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	hDesc += inc;
	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, AO->hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	hDesc += inc;
	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, Metalic->hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	hDesc += inc;
	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, Roughness->hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	gd.pCommandList->SetGraphicsRootDescriptorTable(3, hStart.hgpu);
}

void PBRShader1::SetShadowMapCommand(DescHandle shadowMapDesc)
{
	gd.pCommandList->SetGraphicsRootDescriptorTable(5, shadowMapDesc.hgpu);
}

void PBRShader1::SetMaterialCBV(D3D12_GPU_DESCRIPTOR_HANDLE hgpu)
{
	gd.pCommandList->SetGraphicsRootDescriptorTable(4, hgpu);
}

void SkyBoxShader::LoadSkyBox(const wchar_t* filename)
{
	ID3D12Resource* uploadbuffer = nullptr;
	ID3D12Resource* res = CurrentSkyBox.CreateTextureResourceFromDDSFile(gd.pDevice, gd.pCommandList, (wchar_t*)filename, &uploadbuffer, D3D12_RESOURCE_STATE_GENERIC_READ, true);
	res->QueryInterface<ID3D12Resource2>(&CurrentSkyBox.resource);
	CurrentSkyBox.CurrentState_InCommandWriteLine = D3D12_RESOURCE_STATE_GENERIC_READ;

	// success.
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = res->GetDesc().Format;
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	SRVDesc.TextureCube.MipLevels = 1;
	SRVDesc.TextureCube.MostDetailedMip = 0;
	SRVDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	gd.ShaderVisibleDescPool.ImmortalAllocDescriptorTable(&CurrentSkyBox.hCpu, &CurrentSkyBox.hGpu, 1);
	gd.pDevice->CreateShaderResourceView(CurrentSkyBox.resource, &SRVDesc, CurrentSkyBox.hCpu);

	constexpr int nVertices = 36;
	D3D_PRIMITIVE_TOPOLOGY d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	XMFLOAT3 m_pxmf3Positions[nVertices] = {};

	float fx = 10.0f, fy = 10.0f, fz = 10.0f;
	// Front Quad (quads point inward)
	m_pxmf3Positions[0] = XMFLOAT3(-fx, +fx, +fx);
	m_pxmf3Positions[1] = XMFLOAT3(+fx, +fx, +fx);
	m_pxmf3Positions[2] = XMFLOAT3(-fx, -fx, +fx);
	m_pxmf3Positions[3] = XMFLOAT3(-fx, -fx, +fx);
	m_pxmf3Positions[4] = XMFLOAT3(+fx, +fx, +fx);
	m_pxmf3Positions[5] = XMFLOAT3(+fx, -fx, +fx);
	// Back Quad										
	m_pxmf3Positions[6] = XMFLOAT3(+fx, +fx, -fx);
	m_pxmf3Positions[7] = XMFLOAT3(-fx, +fx, -fx);
	m_pxmf3Positions[8] = XMFLOAT3(+fx, -fx, -fx);
	m_pxmf3Positions[9] = XMFLOAT3(+fx, -fx, -fx);
	m_pxmf3Positions[10] = XMFLOAT3(-fx, +fx, -fx);
	m_pxmf3Positions[11] = XMFLOAT3(-fx, -fx, -fx);
	// Left Quad										
	m_pxmf3Positions[12] = XMFLOAT3(-fx, +fx, -fx);
	m_pxmf3Positions[13] = XMFLOAT3(-fx, +fx, +fx);
	m_pxmf3Positions[14] = XMFLOAT3(-fx, -fx, -fx);
	m_pxmf3Positions[15] = XMFLOAT3(-fx, -fx, -fx);
	m_pxmf3Positions[16] = XMFLOAT3(-fx, +fx, +fx);
	m_pxmf3Positions[17] = XMFLOAT3(-fx, -fx, +fx);
	// Right Quad										
	m_pxmf3Positions[18] = XMFLOAT3(+fx, +fx, +fx);
	m_pxmf3Positions[19] = XMFLOAT3(+fx, +fx, -fx);
	m_pxmf3Positions[20] = XMFLOAT3(+fx, -fx, +fx);
	m_pxmf3Positions[21] = XMFLOAT3(+fx, -fx, +fx);
	m_pxmf3Positions[22] = XMFLOAT3(+fx, +fx, -fx);
	m_pxmf3Positions[23] = XMFLOAT3(+fx, -fx, -fx);
	// Top Quad											
	m_pxmf3Positions[24] = XMFLOAT3(-fx, +fx, -fx);
	m_pxmf3Positions[25] = XMFLOAT3(+fx, +fx, -fx);
	m_pxmf3Positions[26] = XMFLOAT3(-fx, +fx, +fx);
	m_pxmf3Positions[27] = XMFLOAT3(-fx, +fx, +fx);
	m_pxmf3Positions[28] = XMFLOAT3(+fx, +fx, -fx);
	m_pxmf3Positions[29] = XMFLOAT3(+fx, +fx, +fx);
	// Bottom Quad										
	m_pxmf3Positions[30] = XMFLOAT3(-fx, -fx, +fx);
	m_pxmf3Positions[31] = XMFLOAT3(+fx, -fx, +fx);
	m_pxmf3Positions[32] = XMFLOAT3(-fx, -fx, -fx);
	m_pxmf3Positions[33] = XMFLOAT3(-fx, -fx, -fx);
	m_pxmf3Positions[34] = XMFLOAT3(+fx, -fx, +fx);
	m_pxmf3Positions[35] = XMFLOAT3(+fx, -fx, -fx);

	SkyBoxMesh = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * sizeof(XMFLOAT3), 1);
	GPUResource VertexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * sizeof(XMFLOAT3), 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &m_pxmf3Positions[0], &VertexUploadBuffer, &SkyBoxMesh, true);

	SkyBoxMeshVBView.BufferLocation = SkyBoxMesh.resource->GetGPUVirtualAddress();
	SkyBoxMeshVBView.StrideInBytes = sizeof(XMFLOAT3);
	SkyBoxMeshVBView.SizeInBytes = sizeof(XMFLOAT3) * nVertices;
}

SkyBoxShader::SkyBoxShader()
{
}

SkyBoxShader::~SkyBoxShader()
{
}

void SkyBoxShader::InitShader()
{
	CreateRootSignature();
	CreatePipelineState();
}

void SkyBoxShader::CreateRootSignature()
{
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;

	D3D12_ROOT_PARAMETER1 rootParam[3] = {};

	rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParam[0].Constants.Num32BitValues = 16;
	rootParam[0].Constants.ShaderRegister = 0; // b0 : Camera Matrix (View)
	rootParam[0].Constants.RegisterSpace = 0;

	rootParam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParam[1].Constants.Num32BitValues = 16;
	rootParam[1].Constants.ShaderRegister = 1; // b1 : Transform Matrix
	rootParam[1].Constants.RegisterSpace = 0;

	rootParam[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParam[2].DescriptorTable.NumDescriptorRanges = 1;
	D3D12_DESCRIPTOR_RANGE1 ranges[1];
	ranges[0].BaseShaderRegister = 0; // t0 //Diffuse Texture
	ranges[0].RegisterSpace = 0;
	ranges[0].NumDescriptors = 1;
	ranges[0].OffsetInDescriptorsFromTableStart = 0;
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	rootParam[2].DescriptorTable.pDescriptorRanges = &ranges[0];

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

	rootSigDesc1.NumParameters = sizeof(rootParam) / sizeof(D3D12_ROOT_PARAMETER1);
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

void SkyBoxShader::CreatePipelineState()
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
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[1] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
	};
	gPipelineStateDesc.InputLayout.NumElements = 1;
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/SkyBox.hlsl", "VSSkyBox", "vs_5_1", &pd3dVertexShaderBlob);

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
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/SkyBox.hlsl", "PSSkyBox", "ps_5_1", &pd3dPixelShaderBlob);

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

void SkyBoxShader::Add_RegisterShaderCommand(ID3D12GraphicsCommandList* commandList, Shader::ShadowRegisterEnum reg)
{
	commandList->SetPipelineState(pPipelineState);
	commandList->SetGraphicsRootSignature(pRootSignature);
}

void SkyBoxShader::Release()
{
	if (pPipelineState) pPipelineState->Release();
	if (pRootSignature) pRootSignature->Release();
}

void SkyBoxShader::RenderSkyBox()
{
	Add_RegisterShaderCommand(gd.pCommandList);
	gd.pCommandList->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.m_pDescritorHeap);

	XMFLOAT4X4 xmf4x4Projection;
	XMStoreFloat4x4(&xmf4x4Projection, XMMatrixTranspose(gd.viewportArr[0].ProjectMatrix));
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4Projection, 0);

	matrix mat = gd.viewportArr[0].ViewMatrix;
	mat *= gd.viewportArr[0].ProjectMatrix;

	XMFLOAT4X4 xmf4x4View;
	XMStoreFloat4x4(&xmf4x4View, XMMatrixTranspose(mat));
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4View, 0);

	matrix mat2;
	mat2.Id();
	mat2.pos = gd.viewportArr[0].Camera_Pos;
	mat2.pos.w = 1;
	mat2.transpose();
	gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &mat2, 0);

	gd.pCommandList->SetGraphicsRootDescriptorTable(2, CurrentSkyBox.hGpu);

	gd.pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	gd.pCommandList->IASetVertexBuffers(0, 1, &SkyBoxMeshVBView);
	gd.pCommandList->DrawInstanced(36, 1, 0, 0);
}

void TesselationTestShader::InitShader() {
	CreateRootSignature();
	CreatePipelineState();
}

void TesselationTestShader::CreateRootSignature() {
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;

	constexpr int paramCount = 2;
	D3D12_ROOT_PARAMETER1 rootParam[paramCount] = {};

	rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParam[0].Constants.Num32BitValues = 20;
	rootParam[0].Constants.ShaderRegister = 0; // b0 : Camera Matrix (Projection, View) + Camera Positon
	rootParam[0].Constants.RegisterSpace = 0;

	rootParam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParam[1].Constants.Num32BitValues = 16;
	rootParam[1].Constants.ShaderRegister = 1; // b1 : Transform Matrix
	rootParam[1].Constants.RegisterSpace = 0;

	D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT; /*|
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;*/

	rootSigDesc1.NumParameters = paramCount;
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

void TesselationTestShader::CreatePipelineState() {
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dVertexShaderBlob = NULL, * pd3dPixelShaderBlob = NULL;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature;

	gPipelineStateDesc.NodeMask = 0;

	//Input Asm
	gPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[3] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // color vec4
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // normal vec3
	};
	gPipelineStateDesc.InputLayout.NumElements = 3;
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/TesselationTestShader.hlsl", "VSMain", "vs_5_1", &pd3dVertexShaderBlob);

	//Hull Shader
	gPipelineStateDesc.HS = Shader::GetShaderByteCode(L"Resources/Shaders/TesselationTestShader.hlsl", "HSBezier", "hs_5_1", &pd3dVertexShaderBlob);

	//Tessellation

	//Domain Shader
	gPipelineStateDesc.DS = Shader::GetShaderByteCode(L"Resources/Shaders/TesselationTestShader.hlsl", "DSBezier", "ds_5_1", &pd3dVertexShaderBlob);

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
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/TesselationTestShader.hlsl", "PSMain", "ps_5_1", &pd3dPixelShaderBlob);

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

void TesselationTestShader::Add_RegisterShaderCommand(ID3D12GraphicsCommandList* commandList, Shader::ShadowRegisterEnum reg) {
	commandList->SetGraphicsRootSignature(pRootSignature);
	commandList->SetPipelineState(pPipelineState);
}

void TesselationTestShader::Release() {

}

void ComputeTestShader::InitShader() {
	CreateRootSignature();
	CreatePipelineState();
}
void ComputeTestShader::CreateRootSignature() {
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;

	constexpr int RootParamCount = 3;
	D3D12_ROOT_PARAMETER1 rootParam[RootParamCount] = {};

	rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParam[0].Constants.Num32BitValues = 2;
	rootParam[0].Constants.ShaderRegister = 0; // b0 : Camera Matrix (Projection, View) + Camera Positon
	rootParam[0].Constants.RegisterSpace = 0;

	rootParam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParam[1].DescriptorTable.NumDescriptorRanges = 1;
	D3D12_DESCRIPTOR_RANGE1 ranges[1];
	ranges[0].BaseShaderRegister = 0; // t0 prev renderTarget
	ranges[0].RegisterSpace = 0;
	ranges[0].NumDescriptors = 1;
	ranges[0].OffsetInDescriptorsFromTableStart = 0;
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
	rootParam[1].DescriptorTable.pDescriptorRanges = &ranges[0];

	//rootParam[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	//rootParam[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	//rootParam[2].DescriptorTable.NumDescriptorRanges = 1;
	//D3D12_DESCRIPTOR_RANGE1 ranges2[1];
	//ranges2[0].BaseShaderRegister = 1; // t1 prev renderTarget
	//ranges2[0].RegisterSpace = 0;
	//ranges2[0].NumDescriptors = 1;
	//ranges2[0].OffsetInDescriptorsFromTableStart = 0;
	//ranges2[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	//ranges2[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
	//rootParam[2].DescriptorTable.pDescriptorRanges = &ranges2[0];

	rootParam[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParam[2].DescriptorTable.NumDescriptorRanges = 1;
	D3D12_DESCRIPTOR_RANGE1 ranges3[1];
	ranges3[0].BaseShaderRegister = 0; // u0 blur output
	ranges3[0].RegisterSpace = 0;
	ranges3[0].NumDescriptors = 1;
	ranges3[0].OffsetInDescriptorsFromTableStart = 0;
	ranges3[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	ranges3[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
	rootParam[2].DescriptorTable.pDescriptorRanges = &ranges3[0];

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
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

	rootSigDesc1.NumParameters = RootParamCount;
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
void ComputeTestShader::CreatePipelineState() {
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dComputeShaderBlob = NULL;
	D3D12_COMPUTE_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_COMPUTE_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature;
	gPipelineStateDesc.NodeMask = 0;

	//Compute Shader
	gPipelineStateDesc.CS = Shader::GetShaderByteCode(L"Resources/Shaders/TestComputeShader.hlsl", "CSMain", "cs_5_1", &pd3dComputeShaderBlob);

	gd.pDevice->CreateComputePipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState);
	if (pd3dComputeShaderBlob) pd3dComputeShaderBlob->Release();
}
void ComputeTestShader::Add_RegisterShaderCommand(ID3D12GraphicsCommandList* commandList, Shader::ShadowRegisterEnum reg) {
	commandList->SetComputeRootSignature(pRootSignature);
	commandList->SetPipelineState(pPipelineState);
}
void ComputeTestShader::Release() {

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

	XMFLOAT3 AABB[2] = {};
	AABB[0] = { 0, 0, 0 };
	AABB[1] = { 0, 0, 0 };

	for (int i = 0; i < temp_vertices.size(); ++i) {
		temp_vertices[i].position.x -= CenterPos.x;
		temp_vertices[i].position.y -= CenterPos.y;
		temp_vertices[i].position.z -= CenterPos.z;
		temp_vertices[i].position.x /= 100.0f;
		temp_vertices[i].position.y /= 100.0f;
		temp_vertices[i].position.z /= 100.0f;

		if (temp_vertices[i].position.x < AABB[0].x) AABB[0].x = temp_vertices[i].position.x;
		if (temp_vertices[i].position.y < AABB[0].y) AABB[0].y = temp_vertices[i].position.y;
		if (temp_vertices[i].position.z < AABB[0].z) AABB[0].z = temp_vertices[i].position.z;

		if (temp_vertices[i].position.x > AABB[1].x) AABB[1].x = temp_vertices[i].position.x;
		if (temp_vertices[i].position.y > AABB[1].y) AABB[1].y = temp_vertices[i].position.y;
		if (temp_vertices[i].position.z > AABB[1].z) AABB[1].z = temp_vertices[i].position.z;
	}

	SetOBBDataWithAABB(AABB[0], AABB[1]);

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
	return BoundingOrientedBox(OBB_Tr, OBB_Ext, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
}

void Mesh::SetOBBDataWithAABB(XMFLOAT3 minpos, XMFLOAT3 maxpos)
{
	vec4 max = maxpos;
	vec4 min = minpos;
	vec4 mid = 0.5f * (max + min);
	vec4 ext = max - mid;
	OBB_Ext = ext.f3;
	OBB_Tr = mid.f3;
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
	OBB_Tr = { 0, 0, 0 };
	OBB_Ext = XMFLOAT3(width, height, depth);

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

void Mesh::CreateTesslationTestMesh(float width, float heightMul)
{
	std::vector<Vertex> vertices;
	vertices.reserve(16);
	float margin = 2.0f * width / 3.0f;
	//patch
	vertices.push_back(Vertex(XMFLOAT3(-width, 0.0f, -width), XMFLOAT4(gd.randomRangef(0, 1), gd.randomRangef(0, 1), gd.randomRangef(0, 1), 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-width + margin, 0.0f, -width), XMFLOAT4(gd.randomRangef(0, 1), gd.randomRangef(0, 1), gd.randomRangef(0, 1), 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-width + 2 * margin, 0.0f, -width), XMFLOAT4(gd.randomRangef(0, 1), gd.randomRangef(0, 1), gd.randomRangef(0, 1), 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-width + 3 * margin, 0.0f, -width), XMFLOAT4(gd.randomRangef(0, 1), gd.randomRangef(0, 1), gd.randomRangef(0, 1), 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)));

	vertices.push_back(Vertex(XMFLOAT3(-width, 0.0f, -width + margin), XMFLOAT4(gd.randomRangef(0, 1), gd.randomRangef(0, 1), gd.randomRangef(0, 1), 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-width + margin, heightMul, -width + margin), XMFLOAT4(gd.randomRangef(0, 1), gd.randomRangef(0, 1), gd.randomRangef(0, 1), 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-width + 2 * margin, heightMul, -width + margin), XMFLOAT4(gd.randomRangef(0, 1), gd.randomRangef(0, 1), gd.randomRangef(0, 1), 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-width + 3 * margin, 0.0f, -width + margin), XMFLOAT4(gd.randomRangef(0, 1), gd.randomRangef(0, 1), gd.randomRangef(0, 1), 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)));

	vertices.push_back(Vertex(XMFLOAT3(-width, 0.0f, -width + 2 * margin), XMFLOAT4(gd.randomRangef(0, 1), gd.randomRangef(0, 1), gd.randomRangef(0, 1), 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-width + margin, heightMul, -width + 2 * margin), XMFLOAT4(gd.randomRangef(0, 1), gd.randomRangef(0, 1), gd.randomRangef(0, 1), 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-width + 2 * margin, heightMul, -width + 2 * margin), XMFLOAT4(gd.randomRangef(0, 1), gd.randomRangef(0, 1), gd.randomRangef(0, 1), 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-width + 3 * margin, 0.0f, -width + 2 * margin), XMFLOAT4(gd.randomRangef(0, 1), gd.randomRangef(0, 1), gd.randomRangef(0, 1), 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)));

	vertices.push_back(Vertex(XMFLOAT3(-width, 0.0f, -width + 3 * margin), XMFLOAT4(gd.randomRangef(0, 1), gd.randomRangef(0, 1), gd.randomRangef(0, 1), 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-width + margin, 0.0f, -width + 3 * margin), XMFLOAT4(gd.randomRangef(0, 1), gd.randomRangef(0, 1), gd.randomRangef(0, 1), 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-width + 2 * margin, 0.0f, -width + 3 * margin), XMFLOAT4(gd.randomRangef(0, 1), gd.randomRangef(0, 1), gd.randomRangef(0, 1), 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-width + 3 * margin, 0.0f, -width + 3 * margin), XMFLOAT4(gd.randomRangef(0, 1), gd.randomRangef(0, 1), gd.randomRangef(0, 1), 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)));

	int nVertices = vertices.size();
	int nStride = sizeof(Vertex);

	VertexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &vertices[0], &VertexUploadBuffer, &VertexBuffer, true);

	VertexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	VertexBufferView.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
	VertexBufferView.StrideInBytes = nStride;
	VertexBufferView.SizeInBytes = nStride * nVertices;

	IndexNum = 0;
	VertexNum = vertices.size();
	topology = D3D_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST;
	IndexBuffer.resource = nullptr;
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

	XMFLOAT3 AABB[2] = {};
	AABB[0] = { 0, 0, 0 };
	AABB[1] = { 0, 0, 0 };
	for (int i = 0; i < temp_vertices.size(); ++i) {
		temp_vertices[i].position.x -= CenterPos.x;
		temp_vertices[i].position.y -= CenterPos.y;
		temp_vertices[i].position.z -= CenterPos.z;
		temp_vertices[i].position.x /= 100.0f;
		temp_vertices[i].position.y /= 100.0f;
		temp_vertices[i].position.z /= 100.0f;
		
		if (temp_vertices[i].position.x < AABB[0].x) AABB[0].x = temp_vertices[i].position.x;
		if (temp_vertices[i].position.y < AABB[0].y) AABB[0].y = temp_vertices[i].position.y;
		if (temp_vertices[i].position.z < AABB[0].z) AABB[0].z = temp_vertices[i].position.z;

		if (temp_vertices[i].position.x > AABB[1].x) AABB[1].x = temp_vertices[i].position.x;
		if (temp_vertices[i].position.y > AABB[1].y) AABB[1].y = temp_vertices[i].position.y;
		if (temp_vertices[i].position.z > AABB[1].z) AABB[1].z = temp_vertices[i].position.z;
	}

	SetOBBDataWithAABB(AABB[0], AABB[1]);

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
	OBB_Tr = { 0, 0, 0 };
	OBB_Ext = XMFLOAT3(1, 1, 0);

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

void UVMesh::CreatePointMesh_FromVertexData(vector<Vertex>& vert)
{
	int m_nVertices = vert.size();
	int m_nStride = sizeof(Vertex);

	VertexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, vert.size() * sizeof(Vertex), 1);
	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, vert.size() * sizeof(Vertex), 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &vert[0], &VertexUploadBuffer, &VertexBuffer, true);
	VertexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	VertexBufferView.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
	VertexBufferView.StrideInBytes = m_nStride;
	VertexBufferView.SizeInBytes = m_nStride * m_nVertices;

	VertexNum = vert.size();
	topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
}

BumpMesh::BumpMesh()
{
}

BumpMesh::~BumpMesh()
{
}

void BumpMesh::CreateMesh_FromVertexAndIndexData(vector<Vertex>& vert, vector<TriangleIndex>& inds)
{
	int m_nVertices = vert.size();
	int m_nStride = sizeof(Vertex);

	VertexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, vert.size() * sizeof(Vertex), 1);
	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, vert.size() * sizeof(Vertex), 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &vert[0], &VertexUploadBuffer, &VertexBuffer, true);
	VertexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	VertexBufferView.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
	VertexBufferView.StrideInBytes = m_nStride;
	VertexBufferView.SizeInBytes = m_nStride * m_nVertices;

	IndexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(TriangleIndex) * inds.size(), 1);
	IndexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(TriangleIndex) * inds.size(), 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &inds[0], &IndexUploadBuffer, &IndexBuffer, true);
	IndexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_INDEX_BUFFER);
	IndexBufferView.BufferLocation = IndexBuffer.resource->GetGPUVirtualAddress();
	IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	IndexBufferView.SizeInBytes = sizeof(TriangleIndex) * inds.size();

	IndexNum = inds.size() * 3;
	VertexNum = vert.size();
	topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

void BumpMesh::ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering)
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
			XMFLOAT3 v3 = { 0, 0, 0 };

			in >> vertexIndex[0];
			in >> blank;
			in >> uvIndex[0];
			in >> blank;
			in >> normalIndex[0];
			temp_vertices.push_back(Vertex(temp_pos[vertexIndex[0] - 1], temp_normal[normalIndex[0] - 1], temp_uv[uvIndex[0] - 1], v3));

			//in >> blank;
			in >> vertexIndex[1];
			in >> blank;
			in >> uvIndex[1];
			in >> blank;
			in >> normalIndex[1];
			temp_vertices.push_back(Vertex(temp_pos[vertexIndex[1] - 1], temp_normal[normalIndex[1] - 1], temp_uv[uvIndex[1] - 1], v3));

			//in >> blank;
			in >> vertexIndex[2];
			in >> blank;
			in >> uvIndex[2];
			in >> blank;
			in >> normalIndex[2];
			//in >> blank;
			temp_vertices.push_back(Vertex(temp_pos[vertexIndex[2] - 1], temp_normal[normalIndex[2] - 1], temp_uv[uvIndex[2] - 1], v3));

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

	XMFLOAT3 AABB[2] = {};
	AABB[0] = { 0, 0, 0 };
	AABB[1] = { 0, 0, 0 };
	for (int i = 0; i < temp_vertices.size(); ++i) {
		temp_vertices[i].position.x -= CenterPos.x;
		temp_vertices[i].position.y -= CenterPos.y;
		temp_vertices[i].position.z -= CenterPos.z;
		temp_vertices[i].position.x /= 100.0f;
		temp_vertices[i].position.y /= 100.0f;
		temp_vertices[i].position.z /= 100.0f;

		if (temp_vertices[i].position.x < AABB[0].x) AABB[0].x = temp_vertices[i].position.x;
		if (temp_vertices[i].position.y < AABB[0].y) AABB[0].y = temp_vertices[i].position.y;
		if (temp_vertices[i].position.z < AABB[0].z) AABB[0].z = temp_vertices[i].position.z;

		if (temp_vertices[i].position.x > AABB[1].x) AABB[1].x = temp_vertices[i].position.x;
		if (temp_vertices[i].position.y > AABB[1].y) AABB[1].y = temp_vertices[i].position.y;
		if (temp_vertices[i].position.z > AABB[1].z) AABB[1].z = temp_vertices[i].position.z;
	}

	SetOBBDataWithAABB(AABB[0], AABB[1]);

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

BumpMesh* BumpMesh::MakeTerrainMeshFromHeightMap(const char* HeightMapTexFilename, float bumpScale, float Unit, int& XN, int& ZN, byte8** Heightmap)
{
	// minimum : 0  ~  maximum : bumpScale
	int width, height, numChannel;
	byte8* imageRaw = stbi_load(HeightMapTexFilename, &width, &height, &numChannel, 0);
	*Heightmap = imageRaw;
	constexpr int maxWidMesh = 128;
	int Divide[2] = { width / maxWidMesh , height / maxWidMesh };
	BumpMesh* meshs = new BumpMesh[Divide[0] * Divide[1]];
	for (int xi = 0; xi < Divide[0]; ++xi) {
		for (int zi = 0; zi < Divide[1]; ++zi) {
			int StartX = xi * maxWidMesh;
			int StartZ = zi * maxWidMesh;

			XMFLOAT3 AABB[2] = {};
			AABB[0] = { 0, 0, 0 };
			AABB[1] = { 0, 0, 0 };

			int VertexWid = maxWidMesh+1;

			BumpMesh* mesh = &meshs[xi * Divide[1] + zi];
			float CenterX = Unit * (float)maxWidMesh / 2.0f;
			float CenterZ = Unit * (float)maxWidMesh / 2.0f;

			int nVertices = VertexWid* VertexWid;
			int nStride = sizeof(BumpMesh::Vertex);
			D3D_PRIMITIVE_TOPOLOGY d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			Vertex* pVertices = new Vertex[nVertices];

			// add vertex arr
			for (int xi2 = 0; xi2 < VertexWid; ++xi2) {
				float u = (float)(StartX + xi2) / (float)(width);
				for (int zi2 = 0; zi2 < VertexWid; ++zi2) {
					float v = (float)(StartZ + zi2) / (float)(height);
					XMFLOAT3 pos;
					pos.y = (float)(imageRaw[(StartX + xi2) * height + (StartZ + zi2)]) * bumpScale / 256.0f;
					pos.x = Unit * (float)xi2 - CenterX;
					pos.z = Unit * (float)zi2 - CenterZ;

					if (pos.x < AABB[0].x) AABB[0].x = pos.x;
					if (pos.y < AABB[0].y) AABB[0].y = pos.y;
					if (pos.z < AABB[0].z) AABB[0].z = pos.z;
					if (pos.x > AABB[1].x) AABB[1].x = pos.x;
					if (pos.y > AABB[1].y) AABB[1].y = pos.y;
					if (pos.z > AABB[1].z) AABB[1].z = pos.z;

					pVertices[xi2 * VertexWid + zi2] = Vertex(pos, { 0, 1, 0 }, { u, v }, { 0, 0, 0 });
				}
			}

			mesh->SetOBBDataWithAABB(AABB[0], AABB[1]);

			// light to color transform.
			for (int xi2 = 1; xi2 < VertexWid-1; ++xi2) {
				for (int zi2 = 1; zi2 < VertexWid-1; ++zi2) {
					XMVECTOR normal;
					XMVECTOR center = XMLoadFloat3(&pVertices[xi2 * VertexWid + zi2].position);
					XMVECTOR p0 = XMLoadFloat3(&pVertices[(xi2)*VertexWid + (zi2 - 1)].position) - center;
					XMVECTOR p1 = XMLoadFloat3(&pVertices[(xi2 - 1) * VertexWid + (zi2 + 1)].position) - center;
					XMVECTOR p2 = XMLoadFloat3(&pVertices[(xi2)*VertexWid + (zi2 + 1)].position) - center;
					XMVECTOR p3 = XMLoadFloat3(&pVertices[(xi2)*VertexWid + (zi2 + 1)].position) - center;
					XMVECTOR p4 = XMLoadFloat3(&pVertices[(xi2 + 1) * VertexWid + (zi2)].position) - center;
					XMVECTOR p5 = XMLoadFloat3(&pVertices[(xi2 + 1) * VertexWid + (zi2 - 1)].position) - center;

					normal = XMVector3Cross(p0, p1);
					normal += XMVector3Cross(p1, p2);
					normal += XMVector3Cross(p2, p3);
					normal += XMVector3Cross(p3, p4);
					normal += XMVector3Cross(p4, p5);
					normal += XMVector3Cross(p5, p0);
					normal = XMVector3Normalize(normal);

					pVertices[xi2 * VertexWid + zi].normal = vec4(normal).f3;
					pVertices[xi2 * VertexWid + zi].CreateTBN(pVertices[xi2 * VertexWid + zi2], pVertices[(xi2 + 1) * VertexWid + (zi2)], pVertices[(xi2)*VertexWid + (zi2 + 1)]);
				}
			}

			// hmm vertex cache mistake?? -> [x][z] -> [z][x]

			// add index arr
			std::vector<TriangleIndex> TrianglePool;
			int FaceWid = maxWidMesh;
			TrianglePool.reserve(FaceWid * FaceWid * 2);
			for (int xi2 = 0; xi2 < FaceWid; ++xi2) {
				for (int zi2 = 0; zi2 < FaceWid; ++zi2) {
					TrianglePool.push_back(TriangleIndex(xi2 * VertexWid + zi2 + 1, (xi2 + 1) * VertexWid + zi2 + 1, xi2 * VertexWid + zi2));
					TrianglePool.push_back(TriangleIndex((xi2 + 1) * VertexWid + zi2 + 1, (xi2 + 1) * VertexWid + zi2, xi2 * VertexWid + zi2));
				}
			}

			mesh->VertexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * sizeof(Vertex), 1);
			mesh->VertexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * sizeof(Vertex), 1);
			gd.UploadToCommitedGPUBuffer(gd.pCommandList, pVertices, &mesh->VertexUploadBuffer, &mesh->VertexBuffer, true);
			mesh->VertexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
			mesh->VertexBufferView.BufferLocation = mesh->VertexBuffer.resource->GetGPUVirtualAddress();
			mesh->VertexBufferView.StrideInBytes = nStride;
			mesh->VertexBufferView.SizeInBytes = nStride * nVertices;

			mesh->IndexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(TriangleIndex) * TrianglePool.size(), 1);
			mesh->IndexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(TriangleIndex) * TrianglePool.size(), 1);
			gd.UploadToCommitedGPUBuffer(gd.pCommandList, &TrianglePool[0], &mesh->IndexUploadBuffer, &mesh->IndexBuffer, true);
			mesh->IndexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_INDEX_BUFFER);
			mesh->IndexBufferView.BufferLocation = mesh->IndexBuffer.resource->GetGPUVirtualAddress();
			mesh->IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
			mesh->IndexBufferView.SizeInBytes = sizeof(TriangleIndex) * TrianglePool.size();

			mesh->IndexNum = TrianglePool.size() * 3;
			mesh->VertexNum = nVertices;
			mesh->topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			delete[] pVertices;
		}
	}
	*Heightmap = imageRaw;
	//stbi_image_free(imageRaw);
	XN = Divide[0];
	ZN = Divide[1];
	return meshs;
}

//xdiv, zdiv must be even.
void BumpMesh::MakeTessTerrainMeshFromHeightMap(float EdgeLen, int xdiv, int zdiv)
{
	float W = EdgeLen / (float)xdiv;
	float H = EdgeLen / (float)zdiv;
	float MinX = -EdgeLen * 0.5f;
	float MaxX = -EdgeLen * 0.5f;

	std::vector<Vertex> vertices;
	vertices.reserve(xdiv * zdiv * 4);
	float uvdeltaX = 1.0f / (float)xdiv;
	float uvdeltaZ = 1.0f / (float)zdiv;
	for (int xi = 0; xi < xdiv; ++xi) {
		for (int zi = 0; zi < zdiv; ++zi) {
			Vertex v[4];
			v[0].position = {MinX + xi * W, 0, MinX + zi * H};
			v[0].uv = { uvdeltaX * xi, uvdeltaZ * zi};
			v[0].normal = {0, 1, 0};
			v[0].tangent = {1, 0, 0};
			v[1].position = { v[0].position.x + W, 0, v[0].position.z };
			v[1].uv = { v[0].uv.x + uvdeltaX, v[0].uv.y };
			v[1].normal = { 0, 1, 0 };
			v[1].tangent = { 1, 0, 0 };
			v[2].position = { v[0].position.x + W, 0, v[0].position.z + H };
			v[2].uv = { v[0].uv.x + uvdeltaX, v[0].uv.y + uvdeltaZ };
			v[2].normal = { 0, 1, 0 };
			v[2].tangent = { 1, 0, 0 };
			v[3].position = { v[0].position.x, 0, v[0].position.z + H };
			v[3].uv = { v[0].uv.x, v[0].uv.y + uvdeltaZ };
			v[3].normal = { 0, 1, 0 };
			v[3].tangent = { 1, 0, 0 };

			vertices.push_back(v[3]);
			vertices.push_back(v[2]);
			vertices.push_back(v[1]);
			vertices.push_back(v[0]);
		}
	}

	int nVertices = vertices.size();
	int nStride = sizeof(Vertex);

	VertexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &vertices[0], &VertexUploadBuffer, &VertexBuffer, true);

	VertexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	VertexBufferView.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
	VertexBufferView.StrideInBytes = nStride;
	VertexBufferView.SizeInBytes = nStride * nVertices;

	IndexNum = 0;
	VertexNum = vertices.size();
	topology = D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST;
	IndexBuffer.resource = nullptr;
}

void BumpMesh::Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum)
{
	Mesh::Render(pCommandList, instanceNum);
}

void BumpMesh::Release()
{
	Mesh::Release();
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

BOOL GPUResource::CreateTexture_fromImageBuffer(UINT Width, UINT Height, const BYTE* pInitImage, DXGI_FORMAT Format, bool ImmortalShaderVisible)
{
	if (resource != nullptr) return FALSE;

	GPUResource uploadBuffer;

	//D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER - ??
	*this = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_DIMENSION_TEXTURE2D, Width, Height, Format);

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
		int mul = 4;
		if (Format == DXGI_FORMAT_R8_SNORM) mul = 1;
		for (UINT y = 0; y < Height; y++)
		{
			memcpy(pDest, pSrc, Width * mul);
			pSrc += (Width * mul);
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
	SRVDesc.Format = Format;
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MipLevels = 1;
	
	//pGPU = resource->GetGPUVirtualAddress();

	if (ImmortalShaderVisible == false) {
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
	}
	else {
		bool b = gd.ShaderVisibleDescPool.ImmortalAllocDescriptorTable(&hCpu, &hGpu, 1);
		if (b) {
			gd.pDevice->CreateShaderResourceView(resource, &SRVDesc, hCpu);
		}
		else {
			resource->Release();
			resource = nullptr;
			return FALSE;
		}
	}

	AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	return TRUE;
}

void GPUResource::CreateTexture_fromFile(const wchar_t* filename, DXGI_FORMAT Format, int mipmapLevel, bool ImmortalShaderVisible)
{
	int len = wcslen(filename);
	wchar_t* DDSName = nullptr;
	//file is dds
	if(wcscmp(&filename[len - 3], L"dds") == 0) {
		DDSName = (wchar_t*)filename;
		goto TEXTURE_LOAD_START;
	}
	else {
		//if dds not exist
		wchar_t DDSFile[512] = {};
		wcscpy_s(DDSFile, filename);
		wchar_t* ddsstr = &DDSFile[len - 3];
		wcscpy_s(ddsstr, 4, L"dds");
		std::ifstream file(DDSFile);
		if (file.good()) {
			file.close();
			DDSName = DDSFile;
			// use dds file
			dbglog1(L"texture filename is exist in dds. %d\n", 0);
			goto TEXTURE_LOAD_START;
		}
		else {
			// create dds file

			// get filename as char[] (for stb_image function)
			int TexWidth, TexHeight, nrChannels=4;
			char cfilename[512] = {};

			for (int i = 0; i < len; ++i) {
				cfilename[i] = filename[i];
			}
			cfilename[len] = 0;

			// raw image data load
			BYTE* pImageData = stbi_load(cfilename, &TexWidth, &TexHeight, &nrChannels, 4);

			// create bmp image file to convert dds
			char BMPFile[512] = {};
			strcpy_s(BMPFile, cfilename);
			strcpy_s(&BMPFile[len - 3], 4, "bmp");
			imgform::PixelImageObject pio;
			pio.data = (imgform::RGBA_pixel*)pImageData;
			pio.width = TexWidth;
			pio.height = TexHeight;
			pio.rawDataToBMP(BMPFile);

			//raw image data free
			stbi_image_free(pImageData);

			// block compression format identify
			switch (Format) {
			case DXGI_FORMAT_BC1_UNORM:
				game.bmpTodds(mipmapLevel, "BC1_UNORM", BMPFile);
				break;
			case DXGI_FORMAT_BC2_UNORM:
				game.bmpTodds(mipmapLevel, "BC2_UNORM", BMPFile);
				break;
			case DXGI_FORMAT_BC3_UNORM:
				game.bmpTodds(mipmapLevel, "BC3_UNORM", BMPFile);
				break;
			case DXGI_FORMAT_BC7_UNORM: // normal map compression.
				game.bmpTodds(mipmapLevel, "BC7_UNORM", BMPFile);
				break;
			default:
				game.bmpTodds(mipmapLevel, "BC1_UNORM", BMPFile);
				break;
			}

			wchar_t* lastSlash = wcsrchr(DDSFile, L'/');
			lastSlash++;

			MoveFileW(lastSlash, DDSFile);

			DDSName = DDSFile;

			//delete original file, bmp file
			//DeleteFileW(filename);
			DeleteFileA(BMPFile);

			goto TEXTURE_LOAD_START;
		}
	}

	return;

TEXTURE_LOAD_START:
	//create texture resource
	ID3D12Resource* texture = nullptr, * uploadbuff = nullptr;
	std::unique_ptr<uint8_t[]> ddsData;
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	texture = CreateTextureResourceFromDDSFile(gd.pDevice, gd.pCommandList, DDSName, &uploadbuff, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	texture->QueryInterface<ID3D12Resource2>(&resource);
	this->CurrentState_InCommandWriteLine = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	// success.
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = Format;
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MipLevels = 1;

	//pGPU = resource->GetGPUVirtualAddress();

	if (ImmortalShaderVisible == false) {
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
			return;
		}
	}
	else {
		bool b = gd.ShaderVisibleDescPool.ImmortalAllocDescriptorTable(&hCpu, &hGpu, 1);
		if (b) {
			gd.pDevice->CreateShaderResourceView(resource, &SRVDesc, hCpu);
		}
		else {
			resource->Release();
			resource = nullptr;
			return;
		}
	}
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

	if (FAILED(gd.CmdReset(gd.pCommandList, gd.pCommandAllocator)))
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
	gd.CmdClose(gd.pCommandList);

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
	if (rmod == eRenderMeshMod::single_Mesh) {
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
	else {
		pModel->Render(gd.pCommandList, m_worldMatrix, Shader::ShadowRegisterEnum::RenderWithShadow);
		//pModel->Render(gd.pCommandList, m_worldMatrix, Shader::ShadowRegisterEnum::RenderNormal);
	}
}

void GameObject::RenderShadowMap()
{
	if (rmod == eRenderMeshMod::single_Mesh) {
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
	else {
		pModel->Render(gd.pCommandList, m_worldMatrix, Shader::ShadowRegisterEnum::RenderShadowMap);
	}
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

void GameObject::SetModel(Model* ptrModel)
{
	rmod = _Model;
	pModel = ptrModel;
	transforms_innerModel = new matrix[pModel->nodeCount];
	for (int i = 0; i < pModel->nodeCount; ++i) {
		transforms_innerModel[i] = pModel->Nodes[i].transform;
	}
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
	BoundingOrientedBox obb_local;
	switch (rmod) {
	case GameObject::eRenderMeshMod::single_Mesh:
		obb_local = m_pMesh->GetOBB();
		break;
	case GameObject::eRenderMeshMod::_Model:
		obb_local = pModel->GetOBB();
		break;
	}
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

void GameObject::OnLazerHit()
{
}

bool godmod = true;
void Player::Update(float deltaTime)
{
	if (godmod == false) {
		if (m_worldMatrix.pos.fast_square_of_len3 > 100000000) {
			m_worldMatrix.pos = vec4(0, 10, 0, 1);
			LVelocity = 0;
		}
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
	constexpr float speed = 50.0f;

	if (godmod == false) {
		LVelocity.y -= 9.81f * deltaTime;
		if (isGround) {
			if (m_pKeyBuffer[VK_SPACE] & 0xF0) {
				LVelocity.y = JumpVelocity;
				isGround = false;
			}
		}
	}
	else {
		if (m_pKeyBuffer[VK_SPACE] & 0xF0) {
			m_worldMatrix.pos.y += 1;
		}
		if (m_pKeyBuffer[VK_SHIFT] & 0xF0) {
			m_worldMatrix.pos.y -= 1;
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

float StackT = 0;
void Player::Render()
{
	StackT += 100 * game.DeltaTime;

	if (game.bFirstPersonVision == false) {
		transforms_innerModel[0] = XMMatrixRotationAxis(vec4(0, 0, 1, 0), 3.141952f) * pModel->Nodes[0].transform;
		transforms_innerModel[3] = XMMatrixRotationAxis(vec4(0, 1, 0, 0), StackT) * pModel->Nodes[3].transform;
		transforms_innerModel[2] = XMMatrixRotationAxis(vec4(1, 0, 0, 0), StackT) * pModel->Nodes[2].transform;
		pModel->Render(gd.pCommandList, m_worldMatrix, Shader::ShadowRegisterEnum::RenderWithShadow, this);
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

		game.MyDiffuseTextureShader->SetTextureCommand(&game.Guntex);
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

void Monster::Update(float deltaTime)
{
	if (is_dead == false) {
		ChangeDirflow += game.DeltaTime;
		if (ChangeDirflow > 10) {
			rand_direction = vec4(gd.randomRangef(-1, 1), 0, gd.randomRangef(-1, 1), 0);
			rand_direction.len3 = 1;
			ChangeDirflow = 0;
		}

		if (rand_direction != vec4(0, 0, 0, 0)) {
			LookAt(rand_direction);
		}
		m_worldMatrix.pos.x += rand_direction.x * Speed * game.DeltaTime;
		m_worldMatrix.pos.z += rand_direction.z * Speed * game.DeltaTime;
	}
	else {
		if (transforms_innerModel[0].pos.y <= -100) {
			is_dead = false;
			m_worldMatrix = XMMatrixScaling(3, 3, 3);
			m_worldMatrix.pos.f3 = { (float)(rand() % 50), 40, (float)(rand() % 50) };
			for (int i = 0; i < pModel->nodeCount; ++i) {
				transforms_innerModel[i] = pModel->Nodes[i].transform;
			}
		}
	}
}

void Monster::Render() {
	if (is_dead == false) {
		transforms_innerModel[0] = XMMatrixRotationAxis(vec4(0, 0, 1, 0), 3.141952f) * pModel->Nodes[0].transform;
		transforms_innerModel[3] = XMMatrixRotationAxis(vec4(0, 1, 0, 0), StackT) * pModel->Nodes[3].transform;
		transforms_innerModel[2] = XMMatrixRotationAxis(vec4(1, 0, 0, 0), StackT) * pModel->Nodes[2].transform;
		pModel->Render(gd.pCommandList, m_worldMatrix, Shader::ShadowRegisterEnum::RenderWithShadow, this);
	}
	else {
		transforms_innerModel[0].pos += Exp_LVelocity[0] * game.DeltaTime;
		transforms_innerModel[2].pos += Exp_LVelocity[1] * game.DeltaTime;
		transforms_innerModel[3].pos += Exp_LVelocity[2] * game.DeltaTime;
		for (int i = 0; i < 3; ++i) {
			Exp_LVelocity[i].y -= 9.8f * game.DeltaTime;
		}
		pModel->Render(gd.pCommandList, m_worldMatrix, Shader::ShadowRegisterEnum::RenderWithShadow, this);
	}
}

void Monster::Release() {

}

BoundingOrientedBox Monster::GetOBB() {
	if (is_dead) {
		BoundingOrientedBox obb;
		obb.Center = m_worldMatrix.pos.f3;
		obb.Extents = { 0, 0, 0 };
		obb.Orientation = vec4(0, 0, 0, 1);
		return obb;
	}
	else {
		/*BoundingOrientedBox obb;
		obb.Center = m_worldMatrix.pos.f3;
		obb.Extents = { 3, 3, 3 };
		obb.Orientation = vec4(0, 0, 0, 1);
		return obb;*/

		return GameObject::GetOBB();
	}
}

void Monster::OnLazerHit() {
	is_dead = true;
	constexpr float exp_power = 5;
	for (int i = 0; i < 3; ++i) {
		Exp_LVelocity[i] = vec4(gd.randomRangef(-exp_power, exp_power), gd.randomRangef(exp_power*0.5f, 2* exp_power), gd.randomRangef(-exp_power, exp_power), 0);
	}
	game.KillFlow = 1;
	game.KillCount += 1;
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
	m_srvImmortalDescriptorSize = 0;
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

BOOL ShaderVisibleDescriptorPool::ImmortalAllocDescriptorTable(D3D12_CPU_DESCRIPTOR_HANDLE* pOutCPUDescriptor, D3D12_GPU_DESCRIPTOR_HANDLE* pOutGPUDescriptor, UINT DescriptorCount)
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
	m_srvImmortalDescriptorSize = m_AllocatedDescriptorCount;
	bResult = TRUE;
	return bResult;
}

bool ShaderVisibleDescriptorPool::IncludeHandle(D3D12_CPU_DESCRIPTOR_HANDLE hcpu)
{
	return (m_pDescritorHeap->GetCPUDescriptorHandleForHeapStart().ptr <= hcpu.ptr) && (hcpu.ptr < m_pDescritorHeap->GetCPUDescriptorHandleForHeapStart().ptr + gd.CBV_SRV_UAV_Desc_IncrementSiz * m_MaxDescriptorCount);
}

void ShaderVisibleDescriptorPool::Reset()
{
	m_AllocatedDescriptorCount = m_srvImmortalDescriptorSize;
}

void ModelNode::Render(void* model, ID3D12GraphicsCommandList* cmdlist, const matrix& parentMat, void* pGameobject)
{
	Model* pModel = (Model*)model;
	XMMATRIX sav;
	GameObject* obj = (GameObject*)pGameobject;
	if(obj == nullptr) sav = XMMatrixMultiply(transform, parentMat);
	else {
		int nodeindex = ((byte8*)this - (byte8*)pModel->Nodes) / sizeof(ModelNode);
		sav = XMMatrixMultiply(obj->transforms_innerModel[nodeindex], parentMat);
	}

	if (numMesh != 0 && Meshes != nullptr) {
		matrix m = sav;
		m.transpose();

		//set world matrix in shader
		cmdlist->SetGraphicsRoot32BitConstants(1, 16, &m, 0);
		//set texture in shader
		//game.MyPBRShader1->SetTextureCommand()
		for (int i = 0; i < numMesh; ++i) {
			int materialIndex = ((BumpMesh*)pModel->mMeshes[Meshes[i]])->material_index;
			Material& mat = game.MaterialTable[materialIndex];
			//GPUResource* diffuseTex = &game.DefaultTex;
			//if (mat.ti.Diffuse >= 0) diffuseTex = game.TextureTable[mat.ti.Diffuse];
			//GPUResource* normalTex = &game.DefaultNoramlTex;
			//if (mat.ti.Normal >= 0) normalTex = game.TextureTable[mat.ti.Normal];
			//GPUResource* ambientTex = &game.DefaultTex;
			//if (mat.ti.AmbientOcculsion >= 0) ambientTex = game.TextureTable[mat.ti.AmbientOcculsion];
			//GPUResource* MetalicTex = &game.DefaultAmbientTex;
			//if (mat.ti.Metalic >= 0) MetalicTex = game.TextureTable[mat.ti.Metalic];
			//GPUResource* roughnessTex = &game.DefaultAmbientTex;
			//if (mat.ti.Roughness >= 0) roughnessTex = game.TextureTable[mat.ti.Roughness];
			//game.MyPBRShader1->SetTextureCommand(diffuseTex, normalTex, ambientTex, MetalicTex, roughnessTex);

			gd.pCommandList->SetGraphicsRootDescriptorTable(3, mat.hGPU);
			gd.pCommandList->SetGraphicsRootDescriptorTable(4, mat.CB_Resource.hGpu);

			pModel->mMeshes[Meshes[i]]->Render(cmdlist, 1);
		}
	}

	if (numChildren != 0 && Childrens != nullptr) {
		for (int i = 0; i < numChildren; ++i) {
			Childrens[i]->Render(model, cmdlist, sav, pGameobject);
		}
	}
}

void ModelNode::BakeAABB(void* origin, const matrix& parentMat)
{
	Model* model = (Model*)origin;
	XMMATRIX sav = XMMatrixMultiply(transform, parentMat);
	if (numMesh != 0 && Meshes != nullptr) {
		matrix m = sav;
		vec4 AABB[2];

		BoundingOrientedBox obb = model->mMeshes[0]->GetOBB();
		BoundingOrientedBox obb_world;
		obb_world.Transform(obb_world, sav);

		gd.GetAABBFromOBB(AABB, obb_world, true);
		for (int i = 1; i < numMesh; ++i) {
			BoundingOrientedBox obb = model->mMeshes[i]->GetOBB();
			BoundingOrientedBox obb_world;
			obb_world.Transform(obb_world, sav);

			gd.GetAABBFromOBB(AABB, obb_world);
		}

		if (model->AABB[0].x > AABB[0].x) model->AABB[0].x = AABB[0].x;
		if (model->AABB[0].y > AABB[0].y) model->AABB[0].y = AABB[0].y;
		if (model->AABB[0].z > AABB[0].z) model->AABB[0].z = AABB[0].z;

		if (model->AABB[1].x < AABB[1].x) model->AABB[1].x = AABB[1].x;
		if (model->AABB[1].y < AABB[1].y) model->AABB[1].y = AABB[1].y;
		if (model->AABB[1].z < AABB[1].z) model->AABB[1].z = AABB[1].z;
	}

	for (int i = 0; i < numChildren; ++i) {
		ModelNode* node = Childrens[i];
		node->BakeAABB(origin, sav);
	}
}

void Model::LoadModelFile(string filename)
{
	ifstream ifs{ filename, ios_base::binary };
	ifs.read((char*)&mNumMeshes, sizeof(unsigned int));
	mMeshes = new Mesh * [mNumMeshes];
	vertice = new vector<BumpMesh::Vertex>[mNumMeshes];
	indice = new vector<TriangleIndex>[mNumMeshes];

	ifs.read((char*)&nodeCount, sizeof(unsigned int));
	Nodes = new ModelNode[nodeCount];

	//new0
	ifs.read((char*)&mNumTextures, sizeof(unsigned int));
	ifs.read((char*)&mNumMaterials, sizeof(unsigned int));

	MaterialTableStart = game.MaterialTable.size();
	for (int i = 0; i < mNumMeshes; ++i) {
		BumpMesh* mesh = new BumpMesh();
		XMFLOAT3 AABB[2];
		ifs.read((char*)AABB, sizeof(XMFLOAT3) * 2);
		mesh->SetOBBDataWithAABB(AABB[0], AABB[1]);

		//new1
		int material_subindex = 0;
		ifs.read((char*)&material_subindex, sizeof(int));
		mesh->material_index = MaterialTableStart + material_subindex;

		unsigned int vertSiz = 0;
		unsigned int indexSiz = 0;
		ifs.read((char*)&vertSiz, sizeof(unsigned int));
		ifs.read((char*)&indexSiz, sizeof(unsigned int));

		vector<BumpMesh::Vertex>& vertices = vertice[i];
		vector<TriangleIndex>& indices = indice[i];
		vertices.reserve(vertSiz); vertices.resize(vertSiz);
		indices.reserve(indexSiz); indices.resize(indexSiz);

		for (int k = 0; k < vertSiz; ++k) {
			ifs.read((char*)&vertice[i][k].position, sizeof(XMFLOAT3));
			ifs.read((char*)&vertice[i][k].uv, sizeof(XMFLOAT2));
			ifs.read((char*)&vertice[i][k].normal, sizeof(XMFLOAT3));
			ifs.read((char*)&vertice[i][k].tangent, sizeof(XMFLOAT3));

			// non use
			XMFLOAT3 bitangent;
			ifs.read((char*)&bitangent, sizeof(XMFLOAT3));
		}
		for (int k = 0; k < indexSiz; ++k) {
			ifs.read((char*)&indice[i][k], sizeof(TriangleIndex));
		}

		mesh->CreateMesh_FromVertexAndIndexData(vertices, indices);
		mMeshes[i] = mesh;
	}

	/*gd.pCommandList->Close();
	ID3D12CommandList* ppCommandLists[] = { gd.pCommandList };
	gd.pCommandQueue->ExecuteCommandLists(1, ppCommandLists);
	gd.WaitGPUComplete();*/

	for (int i = 0; i < nodeCount; ++i) {
		int namelen = 0;
		ifs.read((char*)&namelen, sizeof(int));
		char name[256] = {};
		ifs.read((char*)name, namelen);
		Nodes[i].name = name;

		ifs.read((char*)&Nodes[i].transform, sizeof(XMMATRIX));

		int parent = 0;
		ifs.read((char*)&parent, sizeof(int));
		if (parent < 0) Nodes[i].parent = nullptr;
		else Nodes[i].parent = &Nodes[parent];

		ifs.read((char*)&Nodes[i].numChildren, sizeof(unsigned int));
		Nodes[i].Childrens = new ModelNode * [Nodes[i].numChildren];
		ifs.read((char*)&Nodes[i].numMesh, sizeof(unsigned int));
		Nodes[i].Meshes = new unsigned int[Nodes[i].numMesh];
		for (int k = 0; k < Nodes[i].numChildren; ++k) {
			int child = 0;
			ifs.read((char*)&child, sizeof(int));
			if (child < 0) Nodes[i].Childrens[k] = nullptr;
			else Nodes[i].Childrens[k] = &Nodes[child];
		}

		ifs.read((char*)&Nodes[i].Meshes[0], sizeof(int) * Nodes[i].numMesh);
	}
	//new2

	//mTextures = new GPUResource * [mNumTextures];
	TextureTableStart = game.TextureTable.size();
	for (int i = 0; i < mNumTextures; ++i) {
		GPUResource* Texture = new GPUResource();
		game.TextureTable.push_back(Texture);

		Texture->resource = nullptr;
		string DDSFilename = filename;
		for (int k = 0; k < 6; ++k) DDSFilename.pop_back();
		DDSFilename += to_string(i);
		DDSFilename += ".dds";
		ifstream ifs2{ DDSFilename , ios_base::binary };
		if (ifs2.good()) {
			ifs2.close();
			//load dds texture
			wstring wDDSFilename;
			wDDSFilename.reserve(DDSFilename.size());
			for (int k = 0; k < DDSFilename.size(); ++k) {
				wDDSFilename.push_back(DDSFilename[k]);
			}
			Texture->CreateTexture_fromFile(wDDSFilename.c_str(), game.basicTexFormat, game.basicTexMip);
		}
		else {
			//make dds texture in DDSFilename path
			int width = 0, height = 0;
			ifs.read((char*)&width, sizeof(int));
			ifs.read((char*)&height, sizeof(int));
			int datasiz = 4 * width * height;
			void* pdata = malloc(4 * width * height);
			ifs.read((char*)pdata, datasiz);

			char BMPFile[512] = {};
			strcpy_s(BMPFile, DDSFilename.c_str());
			strcpy_s(&BMPFile[DDSFilename.size() - 3], 4, "bmp");

			imgform::PixelImageObject pio;
			pio.data = (imgform::RGBA_pixel*)pdata;
			pio.width = width;
			pio.height = height;
			pio.rawDataToBMP(BMPFile);

			game.bmpTodds(game.basicTexMip, game.basicTexFormatStr, BMPFile);

			int pos = DDSFilename.find_last_of('/');
			char* onlyDDSfilename = &DDSFilename[pos + 1];
			MoveFileA(onlyDDSfilename, DDSFilename.c_str());

			wstring wDDSFilename;
			wDDSFilename.reserve(DDSFilename.size());
			for (int k = 0; k < DDSFilename.size(); ++k) {
				wDDSFilename.push_back(DDSFilename[k]);
			}
			Texture->CreateTexture_fromFile(wDDSFilename.c_str(), game.basicTexFormat, game.basicTexMip);

			DeleteFileA(BMPFile);
			//Texture->CreateTexture_fromImageBuffer(width, height, (BYTE*)pdata, DXGI_FORMAT_BC2_UNORM);
			free(pdata);
		}

		/*GPUResource* Texture = new GPUResource();
		game.TextureTable.push_back(Texture);
		Texture->resource = nullptr;
		int width = 0, height = 0;
		ifs.read((char*)&width, sizeof(int));
		ifs.read((char*)&height, sizeof(int));
		int datasiz = 4 * width * height;
		void* pdata = malloc(4 * width * height);
		ifs.read((char*)pdata, datasiz);
		Texture->CreateTexture_fromImageBuffer(width, height, (BYTE*)pdata, DXGI_FORMAT_BC2_UNORM);
		free(pdata);*/
	}

	/*gd.pCommandAllocator->Reset();
	gd.pCommandList->Reset(gd.pCommandAllocator, NULL);*/

	//mMaterials = new Material * [mNumMaterials];
	for (int i = 0; i < mNumMaterials; ++i) {
		Material newmat;
		ifs.read((char*)&newmat, Material::MaterialSiz_withoutGPUResource);
		newmat.clr.diffuse = vec4(1, 1, 1, 1);

		newmat.ShiftTextureIndexs(MaterialTableStart);

		newmat.SetDescTable();
		game.MaterialTable.push_back(newmat);
		//mMaterials[i] = mat;
	}

	RootNode = &Nodes[0];

	BakeAABB();

	delete[] vertice;
	delete[] indice;
}

void Model::LoadModelFile2(string filename)
{
	ifstream ifs{ filename, ios_base::binary };
	ifs.read((char*)&mNumMeshes, sizeof(unsigned int));
	mMeshes = new Mesh * [mNumMeshes];

	ifs.read((char*)&nodeCount, sizeof(unsigned int));
	Nodes = new ModelNode[nodeCount];

	//new0
	ifs.read((char*)&mNumTextures, sizeof(unsigned int));
	ifs.read((char*)&mNumMaterials, sizeof(unsigned int));

	MaterialTableStart = game.MaterialTable.size();
	for (int i = 0; i < mNumMeshes; ++i) {
		BumpMesh* mesh = new BumpMesh();
		XMFLOAT3 AABB[2];
		ifs.read((char*)AABB, sizeof(XMFLOAT3) * 2);
		mesh->SetOBBDataWithAABB(AABB[0], AABB[1]);

		//new1
		//ifs.read((char*)&mesh->material_index, sizeof(int));

		XMFLOAT3 MAABB[2];
		unsigned int vertSiz = 0;
		unsigned int indexSiz = 0;
		ifs.read((char*)&vertSiz, sizeof(unsigned int));
		ifs.read((char*)&indexSiz, sizeof(unsigned int));

		vector<BumpMesh::Vertex> vertices;
		unsigned int triSiz = indexSiz / 3;
		vector<TriangleIndex> indices;
		vertices.reserve(vertSiz); vertices.resize(vertSiz);
		indices.reserve(triSiz); indices.resize(triSiz);

		for (int k = 0; k < vertSiz; ++k) {
			ifs.read((char*)&vertices[k].position, sizeof(XMFLOAT3));
			if (k == 0) {
				MAABB[0] = vertices[k].position;
				MAABB[1] = vertices[k].position;
			}

			if (MAABB[0].x > vertices[k].position.x) MAABB[0].x = vertices[k].position.x;
			if (MAABB[0].y > vertices[k].position.y) MAABB[0].y = vertices[k].position.y;
			if (MAABB[0].z > vertices[k].position.z) MAABB[0].z = vertices[k].position.z;

			if (MAABB[1].x < vertices[k].position.x) MAABB[1].x = vertices[k].position.x;
			if (MAABB[1].y < vertices[k].position.y) MAABB[1].y = vertices[k].position.y;
			if (MAABB[1].z < vertices[k].position.z) MAABB[1].z = vertices[k].position.z;

			ifs.read((char*)&vertices[k].uv, sizeof(XMFLOAT2));
			ifs.read((char*)&vertices[k].normal, sizeof(XMFLOAT3));
			ifs.read((char*)&vertices[k].tangent, sizeof(XMFLOAT3));

			// non use
			XMFLOAT3 bitangent;
			ifs.read((char*)&bitangent, sizeof(XMFLOAT3));
		}
		for (int k = 0; k < triSiz; ++k) {
			ifs.read((char*)&indices[k], sizeof(TriangleIndex));
		}

		float unitMulRate = 1 * AABB[0].x / MAABB[0].x;
		for (int k = 0; k < vertSiz; ++k) {
			vertices[k].position.x *= unitMulRate;
			vertices[k].position.y *= unitMulRate;
			vertices[k].position.z *= unitMulRate;
		}

		mesh->CreateMesh_FromVertexAndIndexData(vertices, indices);
		mMeshes[i] = mesh;
	}

	/*gd.pCommandList->Close();
	ID3D12CommandList* ppCommandLists[] = { gd.pCommandList };
	gd.pCommandQueue->ExecuteCommandLists(1, ppCommandLists);
	gd.WaitGPUComplete();*/

	for (int i = 0; i < nodeCount; ++i) {
		int namelen = 0;
		ifs.read((char*)&namelen, sizeof(int));
		char name[256] = {};
		ifs.read((char*)name, namelen);
		name[namelen] = 0;
		Nodes[i].name = name;

		vec4 pos;
		ifs.read((char*)&pos, sizeof(float) * 3);
		vec4 rot = 0;
		ifs.read((char*)&rot, sizeof(float) * 3);
		vec4 scale = 0;
		ifs.read((char*)&scale, sizeof(float) * 3);

		if (i == 0) {
			matrix mat;
			mat.Id();
			Nodes[i].transform = mat;
		}
		else {
			matrix mat;
			mat.Id();
			rot *= 3.141592f / 180.0f;
			mat *= XMMatrixScaling(scale.x, scale.y, scale.z);
			mat *= XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z);
			mat.pos.f3 = pos.f3;
			mat.pos.w = 1;

			Nodes[i].transform = mat;
		}
		

		int parent = 0;
		ifs.read((char*)&parent, sizeof(int));
		if (parent < 0) Nodes[i].parent = nullptr;
		else Nodes[i].parent = &Nodes[parent];

		ifs.read((char*)&Nodes[i].numChildren, sizeof(unsigned int));
		if (Nodes[i].numChildren != 0) Nodes[i].Childrens = new ModelNode * [Nodes[i].numChildren];

		ifs.read((char*)&Nodes[i].numMesh, sizeof(unsigned int));
		if(Nodes[i].numMesh != 0) Nodes[i].Meshes = new unsigned int[Nodes[i].numMesh];

		for (int k = 0; k < Nodes[i].numChildren; ++k) {
			int child = 0;
			ifs.read((char*)&child, sizeof(int));
			if (child < 0) Nodes[i].Childrens[k] = nullptr;
			else Nodes[i].Childrens[k] = &Nodes[child];
		}

		for (int k = 0; k < Nodes[i].numMesh; ++k) {
			ifs.read((char*)&Nodes[i].Meshes[k], sizeof(int));
			int num_materials = 0;
			ifs.read((char*)&num_materials, sizeof(int));
			for (int u = 0; u < num_materials; ++u) {
				int material_id = 0;
				ifs.read((char*)&material_id, sizeof(int));
				material_id += MaterialTableStart;

				((BumpMesh*)mMeshes[Nodes[i].Meshes[k]])->material_index = material_id;
			}
		}
		//ifs.read((char*)&Nodes[i].Meshes[0], sizeof(int) * Nodes[i].numMesh);

	}
	//new2

	//mTextures = new GPUResource * [mNumTextures];
	TextureTableStart = game.TextureTable.size();
	for (int i = 0; i < mNumTextures; ++i) {
		GPUResource* Texture = new GPUResource();
		game.TextureTable.push_back(Texture);

		Texture->resource = nullptr;
		string DDSFilename = filename;
		for (int k = 0; k < 6; ++k) DDSFilename.pop_back();
		DDSFilename += to_string(i);
		DDSFilename += ".dds";

		ifstream ifs2{ DDSFilename , ios_base::binary };
		if (ifs2.good()) {
			ifs2.close();
			//load dds texture
			wstring wDDSFilename;
			wDDSFilename.reserve(DDSFilename.size());
			for (int k = 0; k < DDSFilename.size(); ++k) {
				wDDSFilename.push_back(DDSFilename[k]);
			}
			Texture->CreateTexture_fromFile(wDDSFilename.c_str(), game.basicTexFormat, game.basicTexMip);
		}
		else {
			string texfile = filename;
			for (int u = 0; u < 6; ++u) texfile.pop_back();
			texfile += to_string(i);
			texfile += ".tex";
			void* pdata = nullptr;
			int width = 0, height = 0;
			ifstream ifstex{ texfile, ios_base::binary };
			if (ifstex.good()) {
				ifstex.read((char*)&width, sizeof(int));
				ifstex.read((char*)&height, sizeof(int));
				int datasiz = 4 * width * height;
				pdata = malloc(4 * width * height);
				ifstex.read((char*)pdata, datasiz);
			}
			else {
				dbglog1(L"texture is not exist. %d\n", 0);
				return;
			}

			//make dds texture in DDSFilename path
			char BMPFile[512] = {};
			strcpy_s(BMPFile, DDSFilename.c_str());
			strcpy_s(&BMPFile[DDSFilename.size() - 3], 4, "bmp");

			imgform::PixelImageObject pio;
			pio.data = (imgform::RGBA_pixel*)pdata;
			pio.width = width;
			pio.height = height;
			pio.rawDataToBMP(BMPFile);

			game.bmpTodds(game.basicTexMip, game.basicTexFormatStr, BMPFile);

			int pos = DDSFilename.find_last_of('/');
			char* onlyDDSfilename = &DDSFilename[pos + 1];
			MoveFileA(onlyDDSfilename, DDSFilename.c_str());

			wstring wDDSFilename;
			wDDSFilename.reserve(DDSFilename.size());
			for (int k = 0; k < DDSFilename.size(); ++k) {
				wDDSFilename.push_back(DDSFilename[k]);
			}
			Texture->CreateTexture_fromFile(wDDSFilename.c_str(), game.basicTexFormat, game.basicTexMip);

			DeleteFileA(BMPFile);
			//Texture->CreateTexture_fromImageBuffer(width, height, (BYTE*)pdata, DXGI_FORMAT_BC2_UNORM);

			free(pdata);
		}
		
		// copy pdata?
		//mTextures[i] = Texture;
	}

	/*gd.pCommandAllocator->Reset();
	gd.pCommandList->Reset(gd.pCommandAllocator, NULL);*/

	//mMaterials = new Material * [mNumMaterials];
	for (int i = 0; i < mNumMaterials; ++i) {
		Material mat;
		//ifs.read((char*)mat, sizeof(Material));
		
		int namelen = 0;
		ifs.read((char*)&namelen, sizeof(int));
		char name[512] = {};
		ifs.read((char*)name, namelen*sizeof(char));
		name[namelen] = 0;
		memcpy_s(mat.name, 40, name, 40);
		mat.name[39] = 0;
		
		ifs.read((char*)&mat.clr.diffuse, sizeof(float) * 4);
		
		ifs.read((char*)&mat.metallicFactor, sizeof(float));

		float smoothness = 0;
		ifs.read((char*)&smoothness, sizeof(float));
		mat.roughnessFactor = 1.0f - smoothness;

		ifs.read((char*)&mat.clr.bumpscaling, sizeof(float));

		vec4 tiling, offset = 0;
		vec4 tiling2, offset2 = 0;
		ifs.read((char*)&tiling, sizeof(float) * 2);
		ifs.read((char*)&offset, sizeof(float) * 2);
		ifs.read((char*)&tiling2, sizeof(float) * 2);
		ifs.read((char*)&offset2, sizeof(float) * 2);
		mat.TilingX = tiling.x;
		mat.TilingY = tiling.y;
		mat.TilingOffsetX = offset.x;
		mat.TilingOffsetY = offset.y;

		bool isTransparent = false;
		ifs.read((char*)&isTransparent, sizeof(bool));
		if (isTransparent) {
			mat.gltf_alphaMode = mat.Blend;
		}
		else mat.gltf_alphaMode = mat.Opaque;

		bool emissive = 0;
		ifs.read((char*)&emissive, sizeof(bool));
		if (emissive) {
			mat.clr.emissive = vec4(1, 1, 1, 1);
		}
		else {
			mat.clr.emissive = vec4(0, 0, 0, 0);
		}
		
		ifs.read((char*)&mat.ti.BaseColor, sizeof(int));
		ifs.read((char*)&mat.ti.Normal, sizeof(int));
		ifs.read((char*)&mat.ti.Metalic, sizeof(int));
		ifs.read((char*)&mat.ti.AmbientOcculsion, sizeof(int));
		ifs.read((char*)&mat.ti.Roughness, sizeof(int));
		ifs.read((char*)&mat.ti.Emissive, sizeof(int));

		int diffuse2, normal2 = 0;
		ifs.read((char*)&diffuse2, sizeof(int));
		ifs.read((char*)&normal2, sizeof(int));

		mat.ShiftTextureIndexs(TextureTableStart);
		mat.SetDescTable();
		//mMaterials[i] = mat;
		game.MaterialTable.push_back(mat);
	}

	RootNode = &Nodes[0];

	BakeAABB();
}

void Model::BakeAABB()
{
	RootNode->BakeAABB(this, XMMatrixIdentity());
	OBB_Tr = 0.5f * (AABB[0] + AABB[1]);
	OBB_Ext = AABB[1] - OBB_Tr;
}

BoundingOrientedBox Model::GetOBB()
{
	return BoundingOrientedBox(OBB_Tr.f3, OBB_Ext.f3, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
}

void Model::Render(ID3D12GraphicsCommandList* cmdlist, matrix worldMatrix, Shader::ShadowRegisterEnum sre, void* pGameobject)
{
	if (sre == Shader::ShadowRegisterEnum::RenderShadowMap) {
		game.MyPBRShader1->Add_RegisterShaderCommand(cmdlist, Shader::ShadowRegisterEnum::RenderShadowMap);

		matrix xmf4x4Projection = game.MySpotLight.viewport.ProjectMatrix;
		matrix view = game.MySpotLight.View;
		view *= xmf4x4Projection;

		XMFLOAT4X4 xmf4x4View;
		XMStoreFloat4x4(&xmf4x4View, XMMatrixTranspose(view));
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4View, 0);
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16);
		gd.pCommandList->SetGraphicsRootConstantBufferView(2, game.LightCB_withShadowResource.resource->GetGPUVirtualAddress());
		gd.pCommandList->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.m_pDescritorHeap);

		RootNode->Render(this, cmdlist, worldMatrix, pGameobject);
	}
	else if(sre == Shader::ShadowRegisterEnum::RenderWithShadow){
		game.MyPBRShader1->Add_RegisterShaderCommand(cmdlist, Shader::ShadowRegisterEnum::RenderWithShadow);

		matrix xmf4x4Projection = gd.viewportArr[0].ProjectMatrix;
		matrix xmf4x4View = gd.viewportArr[0].ViewMatrix;
		xmf4x4View *= xmf4x4Projection;
		xmf4x4View.transpose();
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4View, 0);
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16);
		gd.pCommandList->SetGraphicsRootConstantBufferView(2, game.LightCB_withShadowResource.resource->GetGPUVirtualAddress());
		gd.pCommandList->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.m_pDescritorHeap);
		game.MyPBRShader1->SetShadowMapCommand(game.MySpotLight.renderDesc);

		RootNode->Render(this, cmdlist, worldMatrix, pGameobject);
	}
	else if(sre == Shader::ShadowRegisterEnum::RenderNormal){
		// no shadow render
		game.MyPBRShader1->Add_RegisterShaderCommand(cmdlist);

		matrix xmf4x4Projection = gd.viewportArr[0].ProjectMatrix;
		matrix xmf4x4View = gd.viewportArr[0].ViewMatrix;
		xmf4x4View *= xmf4x4Projection;
		xmf4x4View.transpose();
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4View, 0);
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16);
		gd.pCommandList->SetGraphicsRootConstantBufferView(2, game.LightCBResource.resource->GetGPUVirtualAddress());
		gd.pCommandList->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.m_pDescritorHeap);

		RootNode->Render(this, cmdlist, worldMatrix, pGameobject);
	}
	else if (sre == Shader::ShadowRegisterEnum::RenderInnerMirror) {
		// no shadow render
		game.MyPBRShader1->Add_RegisterShaderCommand(cmdlist, sre);

		matrix xmf4x4Projection = gd.viewportArr[0].ProjectMatrix;
		matrix xmf4x4View = gd.viewportArr[0].ViewMatrix;
		xmf4x4View *= xmf4x4Projection;
		xmf4x4View.transpose();
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4View, 0);
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16);
		gd.pCommandList->SetGraphicsRootConstantBufferView(2, game.LightCBResource.resource->GetGPUVirtualAddress());
		gd.pCommandList->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.m_pDescritorHeap);

		RootNode->Render(this, cmdlist, worldMatrix, pGameobject);
	}
	//game.MyShader->Add_RegisterShaderCommand(cmdlist);
}

void Hierarchy_Object::Render_Inherit(matrix parent_world, Shader::ShadowRegisterEnum sre)
{
	XMMATRIX sav = XMMatrixMultiply(m_worldMatrix, parent_world);
	BoundingOrientedBox obb = GetOBBw(sav);
	if (obb.Extents.x > 0 && gd.viewportArr[0].m_xmFrustumWorld.Intersects(obb)) {
		if (rmod == GameObject::eRenderMeshMod::single_Mesh && m_pMesh != nullptr) {
			matrix m = sav;
			m.transpose();

			//set world matrix in shader
			gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &m, 0);
			//set texture in shader
			//game.MyPBRShader1->SetTextureCommand()
			int materialIndex = Mesh_materialIndex;
			Material& mat = game.MaterialTable[materialIndex];
			/*GPUResource* diffuseTex = &game.DefaultTex;
			if (mat.ti.Diffuse >= 0) diffuseTex = game.TextureTable[mat.ti.Diffuse];
			GPUResource* normalTex = &game.DefaultNoramlTex;
			if (mat.ti.Normal >= 0) normalTex = game.TextureTable[mat.ti.Normal];
			GPUResource* ambientTex = &game.DefaultTex;
			if (mat.ti.AmbientOcculsion >= 0) ambientTex = game.TextureTable[mat.ti.AmbientOcculsion];
			GPUResource* MetalicTex = &game.DefaultAmbientTex;
			if (mat.ti.Metalic >= 0) MetalicTex = game.TextureTable[mat.ti.Metalic];
			GPUResource* roughnessTex = &game.DefaultAmbientTex;
			if (mat.ti.Roughness >= 0) roughnessTex = game.TextureTable[mat.ti.Roughness];
			game.MyPBRShader1->SetTextureCommand(diffuseTex, normalTex, ambientTex, MetalicTex, roughnessTex);*/

			gd.pCommandList->SetGraphicsRootDescriptorTable(3, mat.hGPU);
			gd.pCommandList->SetGraphicsRootDescriptorTable(4, mat.CB_Resource.hGpu);
			m_pMesh->Render(gd.pCommandList, 1);
		}
		else if (rmod == GameObject::eRenderMeshMod::_Model && pModel != nullptr) {
			pModel->Render(gd.pCommandList, sav, sre);
		}
	}

	for (int i = 0; i < childCount; ++i) {
		childs[i]->Render_Inherit(sav, sre);
	}
}

bool Hierarchy_Object::Collision_Inherit(matrix parent_world, BoundingBox bb)
{
	XMMATRIX sav = XMMatrixMultiply(m_worldMatrix, parent_world);
	BoundingOrientedBox obb = GetOBBw(sav);
	if (obb.Extents.x > 0 && obb.Intersects(bb)) {
		return true;
	}

	for (int i = 0; i < childCount; ++i) {
		if (childs[i]->Collision_Inherit(sav, bb)) return true;
	}

	return false;
}

void Hierarchy_Object::InitMapAABB_Inherit(void* origin, matrix parent_world)
{
	GameMap* map = (GameMap*)origin;

	XMMATRIX sav = XMMatrixMultiply(m_worldMatrix, parent_world);
	BoundingOrientedBox obb = GetOBBw(sav);
	map->ExtendMapAABB(obb);

	for (int i = 0; i < childCount; ++i) {
		childs[i]->InitMapAABB_Inherit(origin, sav);
	}
}

BoundingOrientedBox Hierarchy_Object::GetOBBw(matrix worldMat)
{
	BoundingOrientedBox obb_local;
	if (m_pMesh == nullptr) {
		obb_local.Extents.x = -1;
		return obb_local;
	}
	switch (rmod) {
	case GameObject::eRenderMeshMod::single_Mesh:
		obb_local = m_pMesh->GetOBB();
		break;
	case GameObject::eRenderMeshMod::_Model:
		obb_local = pModel->GetOBB();
		break;
	}
	BoundingOrientedBox obb_world;
	obb_local.Transform(obb_world, worldMat);
	return obb_world;
}

void Material::ShiftTextureIndexs(unsigned int TextureIndexStart)
{
	if (ti.Diffuse >= 0)ti.Diffuse += TextureIndexStart;
	if (ti.Specular >= 0)ti.Specular += TextureIndexStart;
	if (ti.Ambient >= 0)ti.Ambient += TextureIndexStart;
	if (ti.Emissive >= 0)ti.Emissive += TextureIndexStart;
	if (ti.Normal >= 0)ti.Normal += TextureIndexStart;
	if (ti.Roughness >= 0)ti.Roughness += TextureIndexStart;
	if (ti.Opacity >= 0)ti.Opacity += TextureIndexStart;
	if (ti.LightMap >= 0)ti.LightMap += TextureIndexStart;
	if (ti.Reflection >= 0)ti.Reflection += TextureIndexStart;
	if (ti.Sheen >= 0)ti.Sheen += TextureIndexStart;
	if (ti.ClearCoat >= 0)ti.ClearCoat += TextureIndexStart;
	if (ti.Transmission >= 0)ti.Transmission += TextureIndexStart;
	if (ti.Anisotropy >= 0)ti.Anisotropy += TextureIndexStart;
}

void Material::SetDescTable()
{
	DescHandle hDesc;
	DescHandle hOriginDesc;
	D3D12_CPU_DESCRIPTOR_HANDLE hcpu;
	gd.ShaderVisibleDescPool.ImmortalAllocDescriptorTable(&hDesc.hcpu, &hDesc.hgpu, 6);
	// [5 * tex] [1 * cbv]
	hOriginDesc = hDesc;

	GPUResource* diffuseTex = &game.DefaultTex;
	if (ti.Diffuse >= 0) diffuseTex = game.TextureTable[ti.Diffuse];
	GPUResource* normalTex = &game.DefaultNoramlTex;
	if (ti.Normal >= 0) normalTex = game.TextureTable[ti.Normal];
	GPUResource* ambientTex = &game.DefaultTex;
	if (ti.AmbientOcculsion >= 0) ambientTex = game.TextureTable[ti.AmbientOcculsion];
	GPUResource* MetalicTex = &game.DefaultAmbientTex;
	if (ti.Metalic >= 0) MetalicTex = game.TextureTable[ti.Metalic];
	GPUResource* roughnessTex = &game.DefaultAmbientTex;
	if (ti.Roughness >= 0) roughnessTex = game.TextureTable[ti.Roughness];
	
	int inc = gd.CBV_SRV_UAV_Desc_IncrementSiz;

	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, diffuseTex->hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	hDesc += inc;
	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, normalTex->hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	hDesc += inc;
	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, ambientTex->hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	hDesc += inc;
	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, MetalicTex->hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	hDesc += inc;
	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, roughnessTex->hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	hGPU = hOriginDesc.hgpu;

	UINT ncbElementBytes = ((sizeof(MaterialCB_Data) + 255) & ~255); //256의 배수
	CB_Resource = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1);
	CB_Resource.resource->Map(0, NULL, (void**)&CBData);
	*CBData = GetMatCB();

	hDesc += inc;

	CB_Resource.resource->Unmap(0, NULL);

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc;
	cbv_desc.BufferLocation = CB_Resource.resource->GetGPUVirtualAddress();
	cbv_desc.SizeInBytes = ncbElementBytes;
	gd.pDevice->CreateConstantBufferView(&cbv_desc, hDesc.hcpu);
	CB_Resource.hCpu = hDesc.hcpu;
	CB_Resource.hGpu = hDesc.hgpu;
}

void Material::SetDescTable_NOTCBV()
{
	DescHandle hDesc;
	DescHandle hOriginDesc;
	D3D12_CPU_DESCRIPTOR_HANDLE hcpu;
	gd.ShaderVisibleDescPool.ImmortalAllocDescriptorTable(&hDesc.hcpu, &hDesc.hgpu, 5);
	// [5 * tex] [1 * cbv]
	hOriginDesc = hDesc;

	GPUResource* diffuseTex = &game.DefaultTex;
	if (ti.Diffuse >= 0) diffuseTex = game.TextureTable[ti.Diffuse];
	GPUResource* normalTex = &game.DefaultNoramlTex;
	if (ti.Normal >= 0) normalTex = game.TextureTable[ti.Normal];
	GPUResource* ambientTex = &game.DefaultTex;
	if (ti.AmbientOcculsion >= 0) ambientTex = game.TextureTable[ti.AmbientOcculsion];
	GPUResource* MetalicTex = &game.DefaultAmbientTex;
	if (ti.Metalic >= 0) MetalicTex = game.TextureTable[ti.Metalic];
	GPUResource* roughnessTex = &game.DefaultAmbientTex;
	if (ti.Roughness >= 0) roughnessTex = game.TextureTable[ti.Roughness];

	int inc = gd.CBV_SRV_UAV_Desc_IncrementSiz;

	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, diffuseTex->hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	hDesc += inc;
	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, normalTex->hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	hDesc += inc;
	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, ambientTex->hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	hDesc += inc;
	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, MetalicTex->hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	hDesc += inc;
	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, roughnessTex->hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	hGPU = hOriginDesc.hgpu;
}

MaterialCB_Data Material::GetMatCB()
{
	MaterialCB_Data CBData;
	CBData.baseColor = clr.base;
	CBData.ambientColor = clr.ambient;
	CBData.emissiveColor = clr.emissive;
	CBData.metalicFactor = metallicFactor;
	CBData.smoothness = roughnessFactor;
	CBData.bumpScaling = clr.bumpscaling;
	CBData.extra = 1.0f;
	CBData.TilingX = TilingX;
	CBData.TilingY = TilingY;
	CBData.TilingOffsetX = TilingOffsetX;
	CBData.TilingOffsetY = TilingOffsetY;
	return CBData;
}

void GameMap::ExtendMapAABB(BoundingOrientedBox obb)
{
	vec4 aabb[2];
	gd.GetAABBFromOBB(aabb, obb, true);

	if (AABB[0].x > aabb[0].x) AABB[0].x = aabb[0].x;
	if (AABB[0].y > aabb[0].y) AABB[0].y = aabb[0].y;
	if (AABB[0].z > aabb[0].z) AABB[0].z = aabb[0].z;

	if (AABB[1].x < aabb[1].x) AABB[1].x = aabb[1].x;
	if (AABB[1].y < aabb[1].y) AABB[1].y = aabb[1].y;
	if (AABB[1].z < aabb[1].z) AABB[1].z = aabb[1].z;
}

ui64 GameMap::GetSpaceHash(int x, int y, int z)
{
	ui64 Flag = (ui16)(x & 1023);
	Flag = (Flag << 10) | (ui16)(y & 1023);
	Flag = (Flag << 10) | (ui16)(z & 1023);
	ui64 result = 0;
	for (int i = 0; i < pdepcnt; ++i) {
		ui64 s = _pdep_u64(Flag, pdep_src2[i]);
		result |= s;
	}
	return result;
}

GameMap::Posindex GameMap::GetInvSpaceHash(ui64 hash)
{
	ui64 result = 0;
	for (int i = 0; i < inv_pdepcnt; ++i) {
		ui64 s = _pdep_u64(hash, inv_pdep_src2[i]);
		result |= s;
	}

	GameMap::Posindex pos;
	pos.x = (result & 1023);
	pos.y = ((result>>10) & 1023);
	pos.z = ((result>>20) & 1023);
	return pos;
}

atomic<int> calculCnt = 0;
void CalculCollide(GameMap* map, byte8* isCollide, int xDivide, int yDivide, int zDivide, float CollisionCubeMargin, int xiStart, int xiEnd, int* FlagCount) {
	BoundingBox Cube;
	vec4 point0, point1;
	point0.x = map->AABB[0].x;
	point1.x = map->AABB[0].x + CollisionCubeMargin;
	point0.y = map->AABB[0].y;
	point1.y = map->AABB[0].y + CollisionCubeMargin;
	point0.z = map->AABB[0].z;
	point1.z = map->AABB[0].z + CollisionCubeMargin;
	BoundingBox::CreateFromPoints(Cube, point0, point1);

	vec4* AABB = map->AABB;
	for (int xi = xiStart; xi < xiEnd; ++xi) {
		Cube.Center.x = AABB[0].x + (CollisionCubeMargin * xi) + CollisionCubeMargin * 0.5f;
		dbglog1(L"CollideBake_%d\n", ++calculCnt);
		for (int yi = 0; yi < yDivide; ++yi) {
			Cube.Center.y = AABB[0].y + (CollisionCubeMargin * yi) + CollisionCubeMargin * 0.5f;
			for (int zi = 0; zi < zDivide; ++zi) {
				Cube.Center.z = AABB[0].z + (CollisionCubeMargin * zi) + CollisionCubeMargin * 0.5f;

				ui64 Flag = (ui16)zi;
				Flag = (Flag << 10) | (ui16)yi;
				Flag = (Flag << 10) | (ui16)xi;

				int n = xi * (zDivide * yDivide) + yi * (zDivide)+zi;
				if (map->MapObjects[0]->Collision_Inherit(XMMatrixIdentity(), Cube)) {
					//isCollide[xi * (zDivide * yDivide) + yi * (zDivide)+zi] = true;
					isCollide[n>>3] |= (1 << (n&7));
					//if collision
					for (int i = 0; i < 30; ++i) {
						ui64 f = (1 << i);
						if (Flag & f) FlagCount[i] += 1;
					}
				}
				else {
					//isCollide[xi * (zDivide * yDivide) + yi * (zDivide)+zi] = false;
					isCollide[n >> 3] &= ~(1 << (n & 7));
				}
			}
		}
	}
}

// takes 1hours..
void GameMap::BakeStaticCollision()
{
	// Get Map's AABB (pre bake in LoadMap function.)
	MapObjects[0]->InitMapAABB_Inherit(this, XMMatrixIdentity());
	// vec4 GameMap::AABB[2];

	vec4 deltaAABB = AABB[1] - AABB[0];
	float CollisionCubeMargin = 1.0f;
	CollisionCubeMargin = deltaAABB.x / 1000.0f;
	if (CollisionCubeMargin < deltaAABB.y / 1000.0f) CollisionCubeMargin = deltaAABB.y / 1000.0f;
	if (CollisionCubeMargin < deltaAABB.z / 1000.0f) CollisionCubeMargin = deltaAABB.z / 1000.0f;

	// Divide Space and repeat OBB Intersect cmp;
	int xDivide = deltaAABB.x / CollisionCubeMargin;
	int yDivide = deltaAABB.y / CollisionCubeMargin;
	int zDivide = deltaAABB.z / CollisionCubeMargin;

	BoundingBox Cube;
	vec4 point0, point1;
	point0.x = AABB[0].x;
	point1.x = AABB[0].x + CollisionCubeMargin;
	point0.y = AABB[0].y;
	point1.y = AABB[0].y + CollisionCubeMargin;
	point0.z = AABB[0].z;
	point1.z = AABB[0].z + CollisionCubeMargin;
	BoundingBox::CreateFromPoints(Cube, point0, point1);

	// whole collision calculate -> too slow (try multithread..)
	int FlagCount[30] = {};
	int FlagCount_th[8][30] = { {} };
	byte8* isCollide = new byte8[xDivide * yDivide * zDivide / 8 + 1];
	
	thread* threads[8];
	for (int i = 0; i < 8; ++i) {
		threads[i] = new thread(CalculCollide, this, isCollide, xDivide, yDivide, zDivide, CollisionCubeMargin, 125 * i, 125 * (i + 1), &FlagCount_th[i][0]);
	}

	for (int i = 0; i < 8; ++i) {
		threads[i]->join();
	}

	for (int i = 0; i < 8; ++i) {
		for (int k = 0; k < 30; ++k) {
			FlagCount[k] += FlagCount_th[i][k];
		}
	}

	goto JUMP001;

	for (int xi = 0; xi < xDivide; ++xi) {
		Cube.Center.x = AABB[0].x + (CollisionCubeMargin * xi) + CollisionCubeMargin * 0.5f;
		dbglog1(L"CollideBake_%d\n", xi);
		for (int yi = 0; yi < yDivide; ++yi) {
			Cube.Center.y = AABB[0].y + (CollisionCubeMargin * yi) + CollisionCubeMargin * 0.5f;
			for (int zi = 0; zi < zDivide; ++zi) {
				Cube.Center.z = AABB[0].z + (CollisionCubeMargin * zi) + CollisionCubeMargin * 0.5f;

				ui64 Flag = (ui16)zi;
				Flag = (Flag << 10) | (ui16)yi;
				Flag = (Flag << 10) | (ui16)xi;

				if (MapObjects[0]->Collision_Inherit(XMMatrixIdentity(), Cube)) {
					isCollide[xi * (zDivide * yDivide) + yi * (zDivide)+zi] = true;
					//if collision
					for (int i = 0; i < 30; ++i) {
						ui64 f = (1 << i);
						if (Flag & f) FlagCount[i] += 1;
					}
				}
				else isCollide[xi * (zDivide * yDivide) + yi * (zDivide)+zi] = false;
			}
		}
	}

JUMP001:


	//Flag Calculate
	double FlagRate[30] = {};
	double limit = pow(2, 30);
	for (int i = 0; i < 30; ++i) {
		FlagRate[i] = (double)FlagCount[i] / limit;
		FlagRate[i] = abs(FlagRate[i] - 0.5f);
	}

	int FlagIndex[30] = {};
	for (int i = 0; i < 30; ++i) { FlagIndex[i] = i; }
	for (int i = 0; i < 30; ++i) {
		for (int k = i + 1; k < 30; ++k) {
			if (FlagRate[FlagIndex[i]] > FlagRate[FlagIndex[k]]) {
				int n = FlagRate[FlagIndex[i]];
				FlagRate[FlagIndex[i]] = FlagRate[FlagIndex[k]];
				FlagRate[FlagIndex[k]] = n;

				n = FlagIndex[i];
				FlagIndex[i] = FlagIndex[k];
				FlagIndex[k] = n;
			}
		}
	}

	int FlagToFlag[30] = {};
	for (int i = 0; i < 30; ++i) {
		for (int k = 0; k < 30; ++k) {
			if (FlagIndex[k] == i) FlagToFlag[i] = k;
		}
	}

	//get hash function
	bool complete[30] = {};
	int sav = 0;
	pdepcnt = 0;
	int completecnt = 0;
	while (completecnt < 30) {
		for (int i = 0; i < 30; ++i) {
			if (complete[i] == false) {
				sav = i;
				break;
			}
		}

		for (int i = 0; i < 30; ++i) {
			if (complete[i]) continue;

			if (FlagToFlag[sav] <= FlagToFlag[i]) {
				complete[i] = true;
				pdep_src2[pdepcnt] |= (ui64)1 << (ui64)FlagToFlag[i];
				completecnt += 1;
			}
			else continue;

			sav = i;
		}
		pdepcnt += 1;
	}

	//get inv hash function
	for (int i = 0; i < 30; ++i) complete[i] = 0;
	sav = 0;
	inv_pdepcnt = 0;
	completecnt = 0;
	while (completecnt < 30) {
		for (int i = 0; i < 30; ++i) {
			if (complete[i] == false) {
				sav = i;
				break;
			}
		}

		for (int i = 0; i < 30; ++i) {
			if (complete[i]) continue;

			if (FlagIndex[sav] <= FlagIndex[i]) {
				complete[i] = true;
				inv_pdep_src2[pdepcnt] |= (ui64)1 << (ui64)FlagIndex[i];
				completecnt += 1;
			}
			else continue;

			sav = i;
		}
		inv_pdepcnt += 1;
	}

	int ilimit = 1 << 30;
	vecarr<range<ui32, bool>> Ranges;
	Ranges.Init(4096);
	bool lastValue = isCollide[0];
	range<ui32, bool> curr;
	curr.value = isCollide[0];
	for (int i = 0; i < ilimit; ++i) {
		GameMap::Posindex pos = GetInvSpaceHash(i);
		bool b = isCollide[pos.x * (zDivide * yDivide) + pos.y * (zDivide)+pos.z];
		if (b != lastValue) {
			curr.end = i - 1;
			Ranges.push_back(curr);
			curr.value = b;
		}
		lastValue = b;
	}
	curr.end = ilimit-1;
	Ranges.push_back(curr);

	IsCollision = AddRangeArr<ui32, bool>(0, Ranges);
}

void GameMap::LoadMap(const char* MapName)
{
	GameMap* map = this;

	string dirName = "Resources/Map/";
	dirName += MapName;

	string MapFilePath = dirName;
	MapFilePath += "/";
	MapFilePath += MapName;
	MapFilePath += ".map";

	ifstream ifs{ MapFilePath, ios_base::binary };
	int nameCount;
	int MeshCount;
	int TextureCount;
	int MaterialCount;
	int ModelCount;
	int gameObjectCount;
	ifs.read((char*)&nameCount, sizeof(int));
	ifs.read((char*)&MeshCount, sizeof(int));
	ifs.read((char*)&TextureCount, sizeof(int));
	ifs.read((char*)&MaterialCount, sizeof(int));
	ifs.read((char*)&ModelCount, sizeof(int));
	ifs.read((char*)&gameObjectCount, sizeof(int));
	map->name.reserve(nameCount);
	map->name.resize(nameCount);
	map->meshes.reserve(MeshCount);
	map->meshes.resize(MeshCount);
	map->models.reserve(ModelCount);
	map->models.resize(ModelCount);
	map->MapObjects.reserve(gameObjectCount);
	map->MapObjects.resize(gameObjectCount);

	for (int i = 0; i < nameCount; ++i) {
		char TempBuffer[1024] = {};
		string name;
		int namelen = 0;
		ifs.read((char*)&namelen, sizeof(int));
		ifs.read((char*)TempBuffer, sizeof(char) * (namelen));
		TempBuffer[namelen] = 0;
		name = TempBuffer;
		map->name[i] = name;
	}

	constexpr char MeshDir[] = "Mesh/";
	constexpr char TextureDir[] = "Texture/";
	constexpr char ModelDir[] = "Model/";
	string MeshDirPath = dirName;
	MeshDirPath += "/Mesh/";

	string TextureDirPath = dirName;
	TextureDirPath += "/Texture/";

	string ModelDirPath = dirName;
	ModelDirPath += "/Model/";

	for (int i = 0; i < MeshCount; ++i) {
		int nameid = 0;
		ifs.read((char*)&nameid, sizeof(int));
		string& name = map->name[nameid];

		string filename = MeshDirPath;
		// .map (확장자)제거
		filename += name;
		if (name == "Ship") {
			cout << endl;
		}
		filename += ".mesh";

		BumpMesh* mesh = new BumpMesh();
		ifstream ifs2{ filename, ios_base::binary };
		int vertCnt = 0;
		int indCnt = 0;
		ifs2.read((char*)&vertCnt, sizeof(int));
		ifs2.read((char*)&indCnt, sizeof(int));

		vector<BumpMesh::Vertex> verts;
		verts.reserve(vertCnt);
		verts.resize(vertCnt);
		vector<TriangleIndex> indexs;
		int tricnt = (indCnt / 3);
		indexs.reserve(tricnt);
		indexs.resize(tricnt);
		for (int k = 0; k < vertCnt; ++k) {
			ifs2.read((char*)&verts[k].position, sizeof(float) * 3);
			ifs2.read((char*)&verts[k].uv, sizeof(float) * 2);
			ifs2.read((char*)&verts[k].normal, sizeof(float) * 3);
			ifs2.read((char*)&verts[k].tangent, sizeof(float) * 3);
			float w = 0; // bitangent direction??
			ifs2.read((char*)&w, sizeof(float));
		}

		for (int k = 0; k < tricnt; ++k) {
			ifs2.read((char*)&indexs[k].v[0], sizeof(UINT));
			ifs2.read((char*)&indexs[k].v[1], sizeof(UINT));
			ifs2.read((char*)&indexs[k].v[2], sizeof(UINT));
		}

		ifs2.close();
		mesh->CreateMesh_FromVertexAndIndexData(verts, indexs);
		//mesh->ReadMeshFromFile_OBJ(filename.c_str(), vec4(1, 1, 1, 1), false);

		ifs.read((char*)&mesh->OBB_Tr, sizeof(float) * 3);
		ifs.read((char*)&mesh->OBB_Ext, sizeof(float) * 3);
		map->meshes[i] = mesh;
	}
	/*gd.pCommandList->Close();
	ID3D12CommandList* ppCommandLists[] = { gd.pCommandList };
	gd.pCommandQueue->ExecuteCommandLists(1, ppCommandLists);
	gd.WaitGPUComplete();*/

	TextureTableStart = game.TextureTable.size();
	MaterialTableStart = game.MaterialTable.size();
	for (int i = 0; i < TextureCount; ++i) {
		int namelen = 0;
		ifs.read((char*)&namelen, sizeof(int));
		char TempBuff[512] = {};
		ifs.read((char*)&TempBuff, namelen);
		TempBuff[namelen] = 0;
		string textureName = TempBuff;

		int width = 0;
		int height = 0;
		int format = 0; // Unity GraphicsFormat

		string filename_dds = TextureDirPath;
		filename_dds += textureName;
		filename_dds += ".dds";
		ifstream ifs2{ filename_dds, ios_base::binary };
		GPUResource* texture = new GPUResource();

		if (ifs2.good()) {
			wchar_t ddsFile[512] = {};
			int len = filename_dds.size();
			for (int i = 0; i < len; ++i) ddsFile[i] = filename_dds[i];
			ddsFile[len] = 0;
			texture->CreateTexture_fromFile(ddsFile, DXGI_FORMAT_BC3_UNORM, 3);
		}
		else {
			string filename = TextureDirPath;
			// .map (확장자)제거
			filename += textureName;
			filename += ".tex";
			
			ifstream ifs2{ filename, ios_base::binary };
			
			ifs2.read((char*)&width, sizeof(int));
			ifs2.read((char*)&height, sizeof(int));

			BYTE* pixels = new BYTE[width * height * 4];
			ifs2.read((char*)pixels, width * height * 4);
			ifs2.close();

			char* isnormal = &filename[filename.size() - 10];
			char* isnormalmap = &filename[filename.size() - 13];
			char* isnormalDX = &filename[filename.size() - 12];
			if ((strcmp(isnormal, "Normal.tex") == 0 ||
				strcmp(isnormalDX, "NormalDX.tex") == 0) ||
				strcmp(isnormalmap, "normalmap.tex") == 0) {
				float xtotal = 0;
				float ytotal = 0;
				float ztotal = 0;
				for (int ix = 0; ix < width; ++ix) {
					int h = rand() % height;
					BYTE* p = &pixels[h * width * 4 + ix * 4];
					xtotal += p[0];
					ytotal += p[1];
					ztotal += p[2];
				}
				if (ztotal < ytotal) {
					// x = z
					// y = -x
					// z = y
					for (int ix = 0; ix < width; ++ix) {
						for (int iy = 0; iy < height; ++iy) {
							BYTE* p = &pixels[iy * width * 4 + ix * 4];
							BYTE z = p[2];
							BYTE y = p[1];
							BYTE x = p[0];
							p[0] = z;
							p[1] = 255 - x;
							p[2] = y;
						}
					}
				}
				if (ztotal < xtotal) {
					// x = z
					// y = y
					// z = x
					for (int ix = 0; ix < width; ++ix) {
						for (int iy = 0; iy < height; ++iy) {
							BYTE* p = &pixels[iy * width * 4 + ix * 4];
							BYTE k = p[2];
							p[2] = p[0];
							p[0] = k;
						}
					}
				}
			}
			imgform::PixelImageObject pio;
			pio.data = (imgform::RGBA_pixel*)pixels;
			pio.width = width;
			pio.height = height;
			string filename_bmp = TextureDirPath;
			filename_bmp += textureName;
			filename_bmp += ".bmp";
			pio.rawDataToBMP(filename_bmp);

			const char* lastSlash = strrchr(filename_dds.c_str(), L'/');
			lastSlash += 1;

			game.bmpTodds(game.basicTexMip, game.basicTexFormatStr, filename_bmp.c_str());

			MoveFileA(lastSlash, filename_dds.c_str());

			wchar_t ddsFile[512] = {};
			int len = filename_dds.size();
			for (int i = 0; i < len; ++i) ddsFile[i] = filename_dds[i];
			ddsFile[len] = 0;
			texture->CreateTexture_fromFile(ddsFile, game.basicTexFormat, game.basicTexMip);

			DeleteFileA(filename_bmp.c_str());

			//texture->CreateTexture_fromImageBuffer(width, height, pixels, DXGI_FORMAT_BC2_UNORM);

			delete[] pixels;
		}

		ifs.read((char*)&width, sizeof(int));
		ifs.read((char*)&height, sizeof(int));
		ifs.read((char*)&format, sizeof(int));

		game.TextureTable.push_back(texture);
	}
	/*gd.pCommandAllocator->Reset();
	gd.pCommandList->Reset(gd.pCommandAllocator, NULL);*/

	for (int i = 0; i < MaterialCount; ++i) {
		Material mat;

		int namelen = 0;
		ifs.read((char*)&namelen, sizeof(int));
		char name[512] = {};
		ifs.read((char*)name, namelen * sizeof(char));
		name[namelen] = 0;

		memcpy_s(mat.name, 40, name, 40);
		mat.name[39] = 0;

		ifs.read((char*)&mat.clr.diffuse, sizeof(float) * 4);

		ifs.read((char*)&mat.metallicFactor, sizeof(float));

		float smoothness = 0;
		ifs.read((char*)&smoothness, sizeof(float));
		mat.roughnessFactor = 1.0f - smoothness;

		ifs.read((char*)&mat.clr.bumpscaling, sizeof(float));

		vec4 tiling, offset = 0;
		vec4 tiling2, offset2 = 0;
		ifs.read((char*)&tiling, sizeof(float) * 2);
		ifs.read((char*)&offset, sizeof(float) * 2);
		ifs.read((char*)&tiling2, sizeof(float) * 2);
		ifs.read((char*)&offset2, sizeof(float) * 2);
		mat.TilingX = tiling.x;
		mat.TilingY = tiling.y;
		mat.TilingOffsetX = offset.x;
		mat.TilingOffsetY = offset.y;

		bool isTransparent = false;
		ifs.read((char*)&isTransparent, sizeof(bool));
		if (isTransparent) {
			mat.gltf_alphaMode = mat.Blend;
		}
		else mat.gltf_alphaMode = mat.Opaque;

		bool emissive = 0;
		ifs.read((char*)&emissive, sizeof(bool));

		ifs.read((char*)&mat.ti.BaseColor, sizeof(int));
		ifs.read((char*)&mat.ti.Normal, sizeof(int));
		ifs.read((char*)&mat.ti.Metalic, sizeof(int));
		ifs.read((char*)&mat.ti.AmbientOcculsion, sizeof(int));
		ifs.read((char*)&mat.ti.Roughness, sizeof(int));
		ifs.read((char*)&mat.ti.Emissive, sizeof(int));

		int diffuse2, normal2 = 0;
		ifs.read((char*)&diffuse2, sizeof(int));
		ifs.read((char*)&normal2, sizeof(int));

		mat.ShiftTextureIndexs(TextureTableStart);
		mat.SetDescTable();

		game.MaterialTable.push_back(mat);
	}

	for (int i = 0; i < ModelCount; ++i) {
		int namelen = 0;
		ifs.read((char*)&namelen, sizeof(int));
		char TempBuff[512] = {};
		ifs.read((char*)&TempBuff, namelen);
		TempBuff[namelen] = 0;

		string modelName = TempBuff;
		string filename = ModelDirPath;
		// .map (확장자)제거
		filename += modelName;
		filename += ".model";

		Model* pModel = new Model();
		pModel->LoadModelFile2(filename);
		map->models[i] = pModel;
	}

	for (int i = 0; i < gameObjectCount; ++i) {
		Hierarchy_Object* go = new Hierarchy_Object();
		map->MapObjects[i] = go;
	}
	for (int i = 0; i < gameObjectCount; ++i) {
		Hierarchy_Object* go = map->MapObjects[i];
		int nameId = 0;
		ifs.read((char*)&nameId, sizeof(int));

		vec4 pos;
		ifs.read((char*)&pos, sizeof(float) * 3);
		vec4 rot = 0;
		ifs.read((char*)&rot, sizeof(float) * 3);
		vec4 scale = 0;
		ifs.read((char*)&scale, sizeof(float) * 3);

		rot *= 3.141592f / 180.0f;
		go->m_worldMatrix *= XMMatrixScaling(scale.x, scale.y, scale.z);
		go->m_worldMatrix *= XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z);
		go->m_worldMatrix.pos.f3 = pos.f3;
		go->m_worldMatrix.pos.w = 1;

		bool isMeshes = false;
		ifs.read((char*)&isMeshes, sizeof(bool));
		if (isMeshes) {
			go->rmod = GameObject::eRenderMeshMod::single_Mesh;
			int meshid = 0;
			int materialNum = 0;
			int materialId = 0;
			ifs.read((char*)&meshid, sizeof(int));
			if (meshid == 75) {
				cout << endl;
			}
			if (meshid < 0) {
				go->m_pMesh = nullptr;
				ifs.read((char*)&materialNum, sizeof(int));
			}
			else {
				go->m_pMesh = map->meshes[meshid];
				ifs.read((char*)&materialNum, sizeof(int));
				for (int k = 0; k < materialNum; ++k) {
					ifs.read((char*)&materialId, sizeof(int));
					go->Mesh_materialIndex = MaterialTableStart + materialId;
				}
			}
		}
		else {
			go->rmod = GameObject::eRenderMeshMod::_Model;
			//Model
			int ModelID;
			ifs.read((char*)&ModelID, sizeof(int));
			if (ModelID < 0) {
				go->pModel = nullptr;
			}
			else {
				go->pModel = map->models[ModelID];
			}
		}

		if (isMeshes) {
			ifs.read((char*)&go->childCount, sizeof(int));
			go->childs.reserve(go->childCount);
			go->childs.resize(go->childCount);
			for (int k = 0; k < go->childCount; ++k) {
				int childIndex = 0;
				ifs.read((char*)&childIndex, sizeof(int));
				go->childs[k] = map->MapObjects[childIndex];
			}
		}
		else {
			go->childCount = 0;
		}

		go->m_pShader = game.MyPBRShader1;

		map->MapObjects[i] = go;
	}

	//BakeStaticCollision();
}

void TerrainObject::LoadTerain(const char* filenae, float bumpS, float _Unit, vec4 Center)
{
	isTesslation = false;
	bumpScale = bumpS;
	Unit = _Unit;
	TerrainMeshs = BumpMesh::MakeTerrainMeshFromHeightMap("Resources/GlobalTexture/TerrainHeightMap.png", bumpS, Unit, XWid, ZWid, &HeightArr);
	xVertexWidth = XWid * VertexWid;
	zVertexWidth = ZWid * VertexWid;
	CenterX = Center.x;
	CenterZ = Center.z;
	CenterY = Center.y;

	BaseMaterial = LoadTerrainMaterial(
		L"Resources/GlobalTexture/TerrainMaterials/Ground015_2K-JPG_Color.jpg", 
		L"Resources/GlobalTexture/TerrainMaterials/Ground015_2K-JPG_NormalDX.jpg", 
		L"Resources/GlobalTexture/TerrainMaterials/Ground015_2K-JPG_AmbientOcclusion.jpg",
		L"Resources/GlobalTexture/TerrainMaterials/Ground015_2K-JPG_Roughness.jpg", 
		L"Resources/GlobalTexture/TerrainMaterials/Ground015_2K-JPG_Displacement.jpg", 16); // 2 ^ 8

	SelectionMaterial[0] = LoadTerrainMaterial(
		L"Resources/GlobalTexture/TerrainMaterials/Rocks011_2K-JPG_Color.jpg", 
		L"Resources/GlobalTexture/TerrainMaterials/Rocks011_2K-JPG_NormalDX.jpg", 
		L"Resources/GlobalTexture/TerrainMaterials/Rocks011_2K-JPG_AmbientOcclusion.jpg", 
		L"Resources/GlobalTexture/TerrainMaterials/Rocks011_2K-JPG_Roughness.jpg", 
		L"Resources/GlobalTexture/TerrainMaterials/Rocks011_2K-JPG_Displacement.jpg", 3); // 3 ^ 5

	SelectionMaterial[1] = LoadTerrainMaterial(
		L"Resources/GlobalTexture/TerrainMaterials/Grass002_2K-JPG_Color.jpg", 
		L"Resources/GlobalTexture/TerrainMaterials/Grass002_2K-JPG_NormalDX.jpg", 
		L"Resources/GlobalTexture/TerrainMaterials/Grass002_2K-JPG_AmbientOcclusion.jpg", 
		L"Resources/GlobalTexture/TerrainMaterials/Grass002_2K-JPG_Roughness.jpg", 
		L"Resources/GlobalTexture/TerrainMaterials/Grass002_2K-JPG_Displacement.jpg", 5); // 5 ^ 3

	SelectionMaterial[2] = LoadTerrainMaterial(
		L"Resources/GlobalTexture/TerrainMaterials/Gravel022_2K-JPG_Color.jpg", 
		L"Resources/GlobalTexture/TerrainMaterials/Gravel022_2K-JPG_NormalDX.jpg", 
		L"Resources/GlobalTexture/TerrainMaterials/Gravel022_2K-JPG_AmbientOcclusion.jpg",
		L"Resources/GlobalTexture/TerrainMaterials/Gravel022_2K-JPG_Roughness.jpg", 
		L"Resources/GlobalTexture/TerrainMaterials/Gravel022_2K-JPG_Displacement.jpg", 7); // 7 ^ 3

	SelectionMaterial[3] = LoadTerrainMaterial(
		L"Resources/GlobalTexture/TerrainMaterials/Gravel022_2K-JPG_Color.jpg",
		L"Resources/GlobalTexture/TerrainMaterials/Gravel022_2K-JPG_NormalDX.jpg",
		L"Resources/GlobalTexture/TerrainMaterials/Gravel022_2K-JPG_AmbientOcclusion.jpg",
		L"Resources/GlobalTexture/TerrainMaterials/Gravel022_2K-JPG_Roughness.jpg",
		L"Resources/GlobalTexture/TerrainMaterials/Gravel022_2K-JPG_Displacement.jpg", 343); // 7 ^ 3

	ColorPicker.CreateTexture_fromFile(
		L"Resources/GlobalTexture/TerrainMaterials/ColorPicker.png", 
		game.basicTexFormat, game.basicTexMip, true);

	vector<UVMesh::Vertex> points;
	for (int i = 0; i < 5000; ++i) {
		float x = gd.randomRangef(-xVertexWidth * Unit * 0.5f, xVertexWidth * Unit * 0.5f);
		float z = gd.randomRangef(-zVertexWidth * Unit * 0.5f, zVertexWidth * Unit * 0.5f);
		float y = GetHeight(x + CenterX, z + CenterZ);
		UVMesh::Vertex v;
		v.position = { x, y+7, z };
		v.normal = vec4(GetNormal(x, z)).f3;
		v.color = { 1, 1, 1, 1 };
		v.uv = { 0, 0 };
		points.push_back(v);
	}
	GrassPointMesh = new UVMesh();
	GrassPointMesh->CreatePointMesh_FromVertexData(points);
}

Material TerrainObject::LoadTerrainMaterial(const wchar_t* TexFileColor, const wchar_t* TexFileNormal, const wchar_t* TexFileAO, const wchar_t* TexFileRoughness, const wchar_t* TexFileDisplacement, float Tiling)
{
	Material mat;
	GPUResource* color, *normal, *ao, *roughness, *displacement;
	int texindex = game.TextureTable.size();
	color = new GPUResource();
	normal = new GPUResource();
	ao = new GPUResource();
	roughness = new GPUResource();
	displacement = new GPUResource();
	
	color->CreateTexture_fromFile(TexFileColor, game.basicTexFormat, game.basicTexMip, false);
	normal->CreateTexture_fromFile(TexFileNormal, game.basicTexFormat, game.basicTexMip, false);
	ao->CreateTexture_fromFile(TexFileAO, game.basicTexFormat, game.basicTexMip, false);
	roughness->CreateTexture_fromFile(TexFileRoughness, game.basicTexFormat, game.basicTexMip, false);
	displacement->CreateTexture_fromFile(TexFileDisplacement, game.basicTexFormat, game.basicTexMip, false);

	game.TextureTable.push_back(color);
	game.TextureTable.push_back(normal);
	game.TextureTable.push_back(ao);
	game.TextureTable.push_back(roughness);
	game.TextureTable.push_back(displacement);

	mat.ti.Diffuse = texindex;
	mat.ti.Normal = texindex+1;
	mat.ti.AmbientOcculsion = texindex + 2;
	mat.ti.Roughness = texindex + 3;
	mat.ti.Displacement = texindex + 4;

	mat.TilingX = Tiling;
	mat.TilingY = Tiling;
	mat.clr.diffuse = vec4(1, 1, 1, 1);

	mat.SetDescTable();

	return mat;
}

void TerrainObject::LoadTessTerrain(const wchar_t* filenae, float EdgeLen, int div, vec4 Center)
{
	isTesslation = true;
	TerrainMeshs = new BumpMesh();
	TerrainMeshs->MakeTessTerrainMeshFromHeightMap(EdgeLen, div, div);
	HeightMap.CreateTexture_fromFile(filenae, game.basicTexFormat, game.basicTexMip, true);

	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		HeightMap.resource,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
	);
	gd.pCommandList->ResourceBarrier(1, &barrier);

	CenterX = Center.x;
	CenterZ = Center.z;
	CenterY = Center.y;
	BaseMaterial = LoadTerrainMaterial(
		L"Resources/GlobalTexture/TerrainMaterials/Ground015_2K-JPG_Color.jpg",
		L"Resources/GlobalTexture/TerrainMaterials/Ground015_2K-JPG_NormalDX.jpg",
		L"Resources/GlobalTexture/TerrainMaterials/Ground015_2K-JPG_AmbientOcclusion.jpg",
		L"Resources/GlobalTexture/TerrainMaterials/Ground015_2K-JPG_Roughness.jpg",
		L"Resources/GlobalTexture/TerrainMaterials/Ground015_2K-JPG_Displacement.jpg", 1); // 2 ^ 8
}

TerrainObject::TerrainObject()
{
}

TerrainObject::~TerrainObject()
{
}

void TerrainObject::Render()
{
	if (isTesslation) {
		game.MyPBRShader1->Add_RegisterShaderCommand(gd.pCommandList, Shader::ShadowRegisterEnum::TessTerrain);
	}
	else {
		game.MyPBRShader1->Add_RegisterShaderCommand(gd.pCommandList, Shader::ShadowRegisterEnum::RenderWithShadow);
	}
	
	gd.pCommandList->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.m_pDescritorHeap);

	matrix view = gd.viewportArr[0].ViewMatrix;
	view *= gd.viewportArr[0].ProjectMatrix;
	view.transpose();
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &view, 0);
	gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16);
	gd.pCommandList->SetGraphicsRootConstantBufferView(2, game.LightCB_withShadowResource.resource->GetGPUVirtualAddress());

	D3D12_GPU_DESCRIPTOR_HANDLE hGPU = BaseMaterial.hGPU;
	gd.pCommandList->SetGraphicsRootDescriptorTable(3, hGPU);

	gd.pCommandList->SetGraphicsRootDescriptorTable(4, BaseMaterial.CB_Resource.hGpu);

	gd.pCommandList->SetGraphicsRootDescriptorTable(5, game.MySpotLight.renderDesc.hgpu);

	if (isTesslation) {
		gd.pCommandList->SetGraphicsRootDescriptorTable(6, HeightMap.hGpu);

		vec4 pos = vec4(CenterX, CenterY, CenterZ, 1.0f);
		matrix m;
		m.pos = pos;
		m.transpose();
		gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &m, 0);

		TerrainMeshs->Render(gd.pCommandList, 1);
	}
	else {
		for (int xi = 0; xi < XWid; ++xi) {
			for (int zi = 0; zi < ZWid; ++zi) {
				BoundingOrientedBox obb = GetOBB_TerrainMesh(xi, zi);
				if (gd.viewportArr[0].m_xmFrustumWorld.Intersects(obb)) {
					//render terrain
					matrix mat;
					mat.pos.f3.x = obb.Center.x;
					mat.pos.f3.z = obb.Center.z;
					mat.pos.f3.y = CenterY;
					mat.transpose();

					gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &mat, 0);
					//gd.pCommandList->SetGraphicsRoot32BitConstants(1, 1, &BaseMaterial.TilingX, 16);
					//gd.pCommandList->SetGraphicsRoot32BitConstants(1, 1, &SelectionMaterial[0].TilingX, 17);
					//gd.pCommandList->SetGraphicsRoot32BitConstants(1, 1, &SelectionMaterial[1].TilingX, 18);
					//gd.pCommandList->SetGraphicsRoot32BitConstants(1, 1, &SelectionMaterial[2].TilingX, 19);
					//gd.pCommandList->SetGraphicsRoot32BitConstants(1, 1, &SelectionMaterial[3].TilingX, 20);

					TerrainMeshs[xi * ZWid + zi].Render(gd.pCommandList, 1);
					//game.TestMirrorMesh->Render(gd.pCommandList, 1);
				}
			}
		}

		game.MyGrassShader->Add_RegisterShaderCommand(gd.pCommandList);

		gd.pCommandList->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.m_pDescritorHeap);

		matrix proj = gd.viewportArr[0].ProjectMatrix;
		proj.transpose();
		view = gd.viewportArr[0].ViewMatrix;
		view.transpose();

		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &proj, 0);
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &view, 16);
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 32);

		matrix world;
		world.pos.f3 = { CenterX , CenterY, CenterZ };
		world.transpose();
		gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &world, 0);

		gd.pCommandList->SetGraphicsRootDescriptorTable(2, game.DefaultGrass.hGpu);

		GrassPointMesh->Render(gd.pCommandList, 1);
	}
}

void TerrainObject::RenderShadowMap()
{
}

void TerrainObject::Release()
{
}

BoundingOrientedBox TerrainObject::GetOBB_TerrainMesh(int x, int z)
{
	BoundingOrientedBox obb = TerrainMeshs[x * ZWid + z].GetOBB();
	float edgeX = (CenterX - 0.5f * (xVertexWidth * Unit));
	float edgeZ = (CenterZ - 0.5f * (zVertexWidth * Unit));
	float MeshWidth = (Unit * (VertexWid));
	obb.Center.x = edgeX + MeshWidth * x + 0.5f * MeshWidth;
	obb.Center.z = edgeZ + MeshWidth * z + 0.5f * MeshWidth;
	obb.Center.y += CenterY;
	return obb;
}

bool TerrainObject::isCollision(GameObject* other)
{
	OBB_vertexVector obbv = other->GetOBBVertexs();
	vec4 minY = obbv.vertex[0][0][0];
	for (int x = 0; x < 2; ++x) {
		for (int y = 0; y < 2; ++y) {
			for (int z = 0; z < 2; ++z) {
				vec4 v = obbv.vertex[x][y][z];
				if (v.y < minY.y) {
					minY = v;
				}
			}
		}
	}

	float terrainHeight = GetHeight(minY.x, minY.z);
	if (terrainHeight < minY.y) return false;
	else return true;
}

void TerrainObject::OnCollision(GameObject* other)
{
	
}

GSBilboardShader::GSBilboardShader()
{
}

GSBilboardShader::~GSBilboardShader()
{
}

void GSBilboardShader::InitShader()
{
	CreateRootSignature();
	CreatePipelineState();
}

void GSBilboardShader::CreateRootSignature()
{
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;

	D3D12_ROOT_PARAMETER1 rootParam[3] = {};

	rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParam[0].Constants.Num32BitValues = 36;
	rootParam[0].Constants.ShaderRegister = 0; // b0 : Camera Matrix (Projection, View) + Camera Positon
	rootParam[0].Constants.RegisterSpace = 0;

	rootParam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParam[1].Constants.Num32BitValues = 16;
	rootParam[1].Constants.ShaderRegister = 1; // b1 : Transform Matrix
	rootParam[1].Constants.RegisterSpace = 0;

	rootParam[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParam[2].DescriptorTable.NumDescriptorRanges = 1;
	D3D12_DESCRIPTOR_RANGE1 ranges[1];
	ranges[0].BaseShaderRegister = 0; // t0 //Diffuse Texture
	ranges[0].RegisterSpace = 0;
	ranges[0].NumDescriptors = 1;
	ranges[0].OffsetInDescriptorsFromTableStart = 0;
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	rootParam[2].DescriptorTable.pDescriptorRanges = &ranges[0];

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

	rootSigDesc1.NumParameters = 3;
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

void GSBilboardShader::CreatePipelineState()
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
	gPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[4] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // color vec4
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // normal vec3
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0} // uv vec2
	};
	gPipelineStateDesc.InputLayout.NumElements = 4;
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/DiffuseTextureShader_bilboard.hlsl", "VSMain", "vs_5_1", &pd3dVertexShaderBlob);

	//Hull Shader
	//gPipelineStateDesc.HS = NULLCODE;

	//Tessellation

	//Domain Shader
	//gPipelineStateDesc.DS = NULLCODE;

	//Geometry Shader
	gPipelineStateDesc.GS = Shader::GetShaderByteCode(L"Resources/Shaders/DiffuseTextureShader_bilboard.hlsl", "GSMain", "gs_5_1", &pd3dVertexShaderBlob);

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
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/DiffuseTextureShader_bilboard.hlsl", "PSMain", "ps_5_1", &pd3dPixelShaderBlob);

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

void GSBilboardShader::Add_RegisterShaderCommand(ID3D12GraphicsCommandList* commandList, Shader::ShadowRegisterEnum reg)
{
	commandList->SetGraphicsRootSignature(pRootSignature);
	commandList->SetPipelineState(pPipelineState);
}

void GSBilboardShader::Release()
{
}

ParticleShader::ParticleShader()
{
}

ParticleShader::~ParticleShader()
{
}

void ParticleShader::InitShader()
{
	CreateRootSignature();
	CreatePipelineState();
	CreatePipelineState_StremOutput();
}

void ParticleShader::CreateRootSignature()
{
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;

	constexpr int numparam = 2;
	D3D12_ROOT_PARAMETER1 rootParam[numparam] = {};

	rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParam[0].Constants.Num32BitValues = 26;
	rootParam[0].Constants.ShaderRegister = 0; // b0 : Camera Matrix (Projection, View) + Camera Positon, DecreaseRate, DeltaTime
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
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT
		/* |
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

	rootSigDesc1.NumParameters = numparam;
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

void ParticleShader::CreatePipelineState()
{
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dVertexShaderBlob = NULL, * pd3dPixelShaderBlob = NULL, * pd3dGeometryShaderBlob = NULL;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature;

	gPipelineStateDesc.NodeMask = 0;

	//Input Asm
	gPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[4] = {
		{ "SV_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // velocity
		{ "NUMBER", 0, DXGI_FORMAT_R32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // float lifeTime
		{ "ID", 0, DXGI_FORMAT_R32_UINT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0} // uint id
	};
	gPipelineStateDesc.InputLayout.NumElements = 4;
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/ParticleShader.hlsl", "VS_DrawParticle", "vs_5_1", &pd3dVertexShaderBlob);

	//Hull Shader
	//gPipelineStateDesc.HS = NULLCODE;

	//Tessellation

	//Domain Shader
	//gPipelineStateDesc.DS = NULLCODE;

	//Geometry Shader
	gPipelineStateDesc.GS = Shader::GetShaderByteCode(L"Resources/Shaders/ParticleShader.hlsl", "GSMain", "gs_5_1", &pd3dGeometryShaderBlob);

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
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/ParticleShader.hlsl", "PSMain", "ps_5_1", &pd3dPixelShaderBlob);

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

void ParticleShader::CreatePipelineState_StremOutput()
{
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dVertexShaderBlob = NULL, * pd3dPixelShaderBlob = NULL, * pd3dGeometryShaderBlob = NULL;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature;

	gPipelineStateDesc.NodeMask = 0;

	//Input Asm
	gPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[4] = {
		{ "SV_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // velocity
		{ "NUMBER", 0, DXGI_FORMAT_R32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // float lifeTime
		{ "ID", 0, DXGI_FORMAT_R32_UINT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0} // uint id
	};
	gPipelineStateDesc.InputLayout.NumElements = 4;
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/ParticleShader.hlsl", "VS_Particle", "vs_5_1", &pd3dVertexShaderBlob);

	//Hull Shader
	//gPipelineStateDesc.HS = NULLCODE;

	//Tessellation

	//Domain Shader
	//gPipelineStateDesc.DS = NULLCODE;

	//Geometry Shader
	gPipelineStateDesc.GS = Shader::GetShaderByteCode(L"Resources/Shaders/ParticleShader.hlsl", "GS_Particle_StreamOutput", "gs_5_1", &pd3dGeometryShaderBlob);

	//Stream Output
	D3D12_SO_DECLARATION_ENTRY pSODecls[] = {
		{0, "SV_POSITION", 0, 0, 3, 0},
		{0, "NORMAL", 0, 0, 3, 0},
		{0, "NUMBER", 0, 0, 1, 0},
		{0, "ID", 0, 0, 1, 0},
	};

	D3D12_STREAM_OUTPUT_DESC StreamOutputDesc;
	StreamOutputDesc.pSODeclaration = pSODecls;
	StreamOutputDesc.NumEntries = sizeof(pSODecls) / sizeof(D3D12_SO_DECLARATION_ENTRY);
	StreamOutputDesc.pBufferStrides = NULL;
	StreamOutputDesc.NumStrides = StreamOutputDesc.RasterizedStream = 0;
	gPipelineStateDesc.StreamOutput = StreamOutputDesc;

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
	gPipelineStateDesc.NumRenderTargets = 0;
	gPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	gPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	gd.pDevice->CreateGraphicsPipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState_StreamOutput);
	if (pd3dVertexShaderBlob) pd3dVertexShaderBlob->Release();
	if (pd3dPixelShaderBlob) pd3dPixelShaderBlob->Release();
}

void ParticleShader::Add_RegisterShaderCommand(ID3D12GraphicsCommandList* commandList, Shader::ShadowRegisterEnum reg)
{
	commandList->SetGraphicsRootSignature(pRootSignature);
	if (reg == Shader::ShadowRegisterEnum::StreamOut) {
		commandList->SetPipelineState(pPipelineState_StreamOutput);
	}
	else {
		commandList->SetPipelineState(pPipelineState);
	}
}

void ParticleShader::Release()
{
}

void ParticleShader::SetShadowMapCommand(DescHandle shadowMapDesc)
{
}

void StreamOutputPoints::Init(int vertexCount)
{
	//CreateVertexBuffer(gd.pDevice, gd.pCommandList, { 0, 0, 0 }, { 0, 1, 0 }, 5, vertexCount);
	m_nVertices = vertexCount;
	m_nStride = sizeof(ParticleVertex0);

	//add random point datas
	vector<ParticleVertex0> streamoutputVertexs;
	streamoutputVertexs.reserve(vertexCount);
	streamoutputVertexs.resize(vertexCount);
	for (int i = 0; i < vertexCount; ++i) {
		streamoutputVertexs[i].pos = {};
		streamoutputVertexs[i].m_fLifetime = 1.0f;
		streamoutputVertexs[i].m_nType = 0;
		vec4 velo = vec4(gd.randomRangef(-1, 1), 1, gd.randomRangef(-1, 1), 0);
		velo.len3 = 1;
		streamoutputVertexs[i].velocity = velo.f3;
	}

	//create vertexBuffer
	VertexBufer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, m_nVertices * sizeof(ParticleVertex0), 1);
	GPUResource VertexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, m_nVertices * sizeof(ParticleVertex0), 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &streamoutputVertexs[0], &VertexUploadBuffer, &VertexBufer, true);
	VertexBuferView.BufferLocation = VertexBufer.resource->GetGPUVirtualAddress();
	VertexBuferView.StrideInBytes = sizeof(ParticleVertex0);
	VertexBuferView.SizeInBytes = sizeof(ParticleVertex0) * vertexCount;
	VertexBufer.CurrentState_InCommandWriteLine = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

	//create StreamOutBuffer
	StreamOutBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, m_nVertices * sizeof(ParticleVertex0), 1);
	//is require?
	StreamOutUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, m_nVertices * sizeof(ParticleVertex0), 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &streamoutputVertexs[0], &StreamOutUploadBuffer, &StreamOutBuffer, true);
	StreamOutBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_STREAM_OUT);

	//why DrawBuffer exist??
	DrawBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(m_nVertices * sizeof(ParticleVertex0)), 1);

	UINT64* pnBufferFilledSize = nullptr;
	BufferFilledSizeRes = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_STREAM_OUT, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(UINT64), 1);
	//BufferFilledSizeRes.resource->Map(0, NULL, (void**) & pnBufferFilledSize);

	UINT64* pnUploadBufferFilledSize = nullptr;
	UploadBufferFilledSizeRes = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(UINT64), 1);
	UploadBufferFilledSizeRes.resource->Map(0, NULL, (void**)&pnUploadBufferFilledSize);

	UINT64* pnReadBackBufferFilledSize = nullptr;
	ReadBackBufferFilledSizeRes = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(UINT64), 1);
	ReadBackBufferFilledSizeRes.resource->Map(0, NULL, (void**)&pnUploadBufferFilledSize);

	StreamOutBufferView.BufferLocation = StreamOutBuffer.resource->GetGPUVirtualAddress();
	StreamOutBufferView.SizeInBytes = m_nVertices * sizeof(ParticleVertex0);
	StreamOutBufferView.BufferFilledSizeLocation = BufferFilledSizeRes.resource->GetGPUVirtualAddress();
}

void StreamOutputPoints::Render(bool isStreamOut)
{
	if (isStreamOut) {
		// copy FilledSize to streamFilledSizeBuffer
		BufferFilledSizeRes.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_COPY_DEST);
		gd.pCommandList->CopyResource(BufferFilledSizeRes.resource, UploadBufferFilledSizeRes.resource);
		BufferFilledSizeRes.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_STREAM_OUT);

		// Stream Out Render Particle
		game.MyParticleShader->Add_RegisterShaderCommand(gd.pCommandList, Shader::ShadowRegisterEnum::StreamOut);
		gd.pCommandList->SOSetTargets(0, 1, &StreamOutBufferView);

		matrix view = gd.viewportArr[0].ViewMatrix;
		view.transpose();
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &view, 0);
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16);
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &worldMat.pos, 20);
		float DecreaseRate = 0.5f;
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 1, &DecreaseRate, 24);
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 1, &game.DeltaTime, 25);

		ui32 SlotNum = 0;
		gd.pCommandList->IASetVertexBuffers(SlotNum, 1, &VertexBuferView);
		gd.pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
		ui32 VertexOffset = 0;
		gd.pCommandList->DrawInstanced(30/*m_nVertices*/, 1, VertexOffset, 0);

		// get how many vertex stream output
		/*ReadBackBufferFilledSizeRes.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_COPY_DEST);*/
		BufferFilledSizeRes.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_COPY_SOURCE);
		gd.pCommandList->CopyResource(ReadBackBufferFilledSizeRes.resource, BufferFilledSizeRes.resource);
		BufferFilledSizeRes.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_STREAM_OUT);

		// vertex Count reinitialization According to readBackBuffer
		UINT64* pFilledSize{ NULL };
		ReadBackBufferFilledSizeRes.resource->Map(0, NULL, reinterpret_cast<void**>(&pFilledSize));
		m_nVertices = *pFilledSize / sizeof(ParticleVertex0);
		ReadBackBufferFilledSizeRes.resource->Unmap(0, NULL);

		//copy result of stream output to draw buffer
		VertexBufer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_COPY_DEST);
		StreamOutBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_COPY_SOURCE);
		gd.pCommandList->CopyResource(VertexBufer.resource, StreamOutBuffer.resource);
		VertexBufer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		StreamOutBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_STREAM_OUT);

		//VertexBuferView.SizeInBytes = sizeof(ParticleVertex0) * 30/*m_nVertices*/;
	}
	else {
		game.MyParticleShader->Add_RegisterShaderCommand(gd.pCommandList, Shader::ShadowRegisterEnum::RenderNormal);
		//gd.pCommandList->SOSetTargets(0, 1, nullptr);
		ui32 SlotNum = 0;

		matrix view = gd.viewportArr[0].ViewMatrix;
		view.transpose();
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &view, 0);
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16);
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &worldMat.pos, 20);
		float DecreaseRate = 0.5f;
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 1, &DecreaseRate, 24);
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 1, &game.DeltaTime, 25);

		DescHandle handle;
		gd.ShaderVisibleDescPool.AllocDescriptorTable(&handle.hcpu, &handle.hgpu, 1);
		gd.pDevice->CopyDescriptorsSimple(1, handle.hcpu, game.DefaultExplosionParticle.hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		gd.pCommandList->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.m_pDescritorHeap);
		gd.pCommandList->SetGraphicsRootDescriptorTable(1, handle.hgpu);

		gd.pCommandList->IASetVertexBuffers(SlotNum, 1, &VertexBuferView);
		gd.pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
		ui32 VertexOffset = 0;
		gd.pCommandList->DrawInstanced(30, 1, VertexOffset, 0);
	}
}

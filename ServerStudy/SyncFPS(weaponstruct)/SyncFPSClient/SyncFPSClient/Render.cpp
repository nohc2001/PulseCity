#include "stdafx.h"
#include "Render.h"
#include "main.h"
#include "GraphicDefs.h"

void font_parsed(void* args, void* _font_data, int error)
{
	if (error)
	{
		*(uint8_t*)args = error;
	}
	else
	{
		TTFFontParser::FontData* font_data = (TTFFontParser::FontData*)_font_data;

		{
			for (const auto& glyph_iterator : font_data->glyphs)
			{
				uint32_t num_curves = 0, num_lines = 0;
				for (const auto& path_list : glyph_iterator.second.path_list)
				{
					for (const auto& geometry : path_list.geometry)
					{
						if (geometry.is_curve)
							num_curves++;
						else
							num_lines++;
					}
				}
			}
		}

		*(uint8_t*)args = 1;
	}
}

void GlobalDevice::Factory_Adaptor_Output_Init()
{
	HRESULT hr;
	UINT nDXGIFactoryFlags = 0;
#if defined(_DEBUG)
	ID3D12Debug* pd3dDebugController = NULL;
	hr = D3D12GetDebugInterface(__uuidof(ID3D12Debug), (void
		**)&pd3dDebugController);
	if (pd3dDebugController)
	{
		pd3dDebugController->EnableDebugLayer();
		pd3dDebugController->Release();
	}
	nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

	// sus 버전에 따라 안될 수 있으니 예외처리 필요.
	hr = ::CreateDXGIFactory2(nDXGIFactoryFlags, __uuidof(IDXGIFactory4), (void
		**)&pFactory);
	if (FAILED(hr)) hr = ::CreateDXGIFactory2(nDXGIFactoryFlags, __uuidof(IDXGIFactory3), (void
		**)&pFactory);
	if (FAILED(hr)) hr = ::CreateDXGIFactory2(nDXGIFactoryFlags, __uuidof(IDXGIFactory2), (void
		**)&pFactory);
	if (FAILED(hr)) hr = ::CreateDXGIFactory2(nDXGIFactoryFlags, __uuidof(IDXGIFactory1), (void
		**)&pFactory);
	if (FAILED(hr)) {
		OutputDebugStringA("[ERROR] : Create Factory Error.\n");
		return;
	}

	IDXGIAdapter1* pd3dAdapter1 = NULL;
	hr = pFactory->EnumAdapters1(0, &pd3dAdapter1);
	if (FAILED(hr)) throw "EnumAdapters1 Error.";
	// 전체화면 모드로 전환 가능한 해상도를 얻기 위한 작업
	//AI Code Start <Microsoft Copilot>
	IDXGIOutput* output;
	hr = pd3dAdapter1->EnumOutputs(0, &output);
	if (FAILED(hr)) throw "Get Monitor Outputs Error.";
	UINT numModes = 0;
	hr = output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, nullptr);
	if (FAILED(hr)) throw "GetDisplayModeList Error";
	vector<DXGI_MODE_DESC> modeList(numModes);
	hr = output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, modeList.data());
	if (FAILED(hr)) throw "GetDisplayModeList Error";
	//AI Code End <Microsoft Copilot>

	for (int i = 0; i < numModes; ++i) {
		ResolutionStruct rs;
		rs.width = modeList[i].Width;
		rs.height = modeList[i].Height;
		bool isExist = false;
		for (int k = 0; k < EnableFullScreenMode_Resolusions.size(); ++k) {
			ResolutionStruct rs2 = EnableFullScreenMode_Resolusions[k];
			if (rs2.width == rs.width && rs2.height == rs.height) {
				isExist = true;
				break;
			}
		}
		if (!isExist) {
			EnableFullScreenMode_Resolusions.push_back(rs);
		}
	}
}

void GlobalDevice::Init()
{
	HRESULT hr;

#ifdef PIX_DEBUGING
	LoadLibrary(L"C:/Program Files/Microsoft PIX/2509.25/WinPixGpuCapturer.dll");
#endif

	font_filename[0] = "consola.ttf"; // english
	font_filename[1] = "malgunbd.ttf"; // korean
	for (int i = 0; i < FontCount; ++i) {
		uint8_t condition_variable = 0;
		int8_t error = TTFFontParser::parse_file(font_filename[i].c_str(), &font_data[i], &font_parsed, &condition_variable);
	}
	addTextureStack.reserve(32);

	constexpr D3D_FEATURE_LEVEL FeatureLevelPriority[11] = {
		D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1, D3D_FEATURE_LEVEL_1_0_CORE
	};

	IDXGIAdapter1* pd3dAdapter1 = NULL;
	for (int k = 0; k < 11; ++k) {
		// Find GPU that support DirectX 12_2
		for (UINT i = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(i, &pd3dAdapter1); i++)
		{
			IDXGIAdapter4* pd3dAdapter4 = NULL;
			DXGI_ADAPTER_DESC1 dxgiAdapterDesc;
			pd3dAdapter1->GetDesc1(&dxgiAdapterDesc);
			pd3dAdapter1->QueryInterface(__uuidof(IDXGIAdapter4), (void**)&pd3dAdapter4);
			if (dxgiAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
			if (SUCCEEDED(D3D12CreateDevice(pd3dAdapter4, FeatureLevelPriority[k],
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

	// Graphic GPUCmd
	gpucmd = GPUCmd(pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);
	hr = gpucmd.Close(); // why?

	// compute GPUCmd
	CScmd = GPUCmd(pDevice, D3D12_COMMAND_LIST_TYPE_COMPUTE);
	hr = CScmd.Close();

	ClientFrameWidth = gd.EnableFullScreenMode_Resolusions[resolutionLevel].width;
	ClientFrameHeight = gd.EnableFullScreenMode_Resolusions[resolutionLevel].height;

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

	TextureDescriptorAllotter.Init(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 4096);
	//why cannot be shader visible in Saver DescriptorHeap?
	//answer : CreateConstantBufferView, CreateShaderResourceView -> only CPU Descriptor Working. so Shader Invisible.

	ShaderVisibleDescPool.Initialize(4096);

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

	if (RenderMod == DeferedRendering) {
		if (GbufferDescriptorHeap) GbufferDescriptorHeap->Release();
		for (int i = 0; i < GbufferCount; ++i) {
			if (ppGBuffers[i]) ppGBuffers[i]->Release();
		}
	}

	for (int i = 0; i < SwapChainBufferCount; ++i) {
		if (ppRenderTargetBuffers[i]) ppRenderTargetBuffers[i]->Release();
	}

	if (pRtvDescriptorHeap) pRtvDescriptorHeap->Release();
	if (pDepthStencilBuffer) pDepthStencilBuffer->Release();
	if (pDsvDescriptorHeap) pDsvDescriptorHeap->Release();

	if (gpucmd) gpucmd.Release();
	if (CScmd) CScmd.Release();

	CloseHandle(hFenceEvent);
	if (pFence) pFence->Release();

	pSwapChain->SetFullscreenState(FALSE, NULL);
	if (pSwapChain) pSwapChain->Release();

	if (pDevice) {
		pDevice->Release();
	}
	if (pFactory) pFactory->Release();

#if defined(_DEBUG)
	IDXGIDebug1* pdxgiDebug = NULL;
	DXGIGetDebugInterface1(0, __uuidof(IDXGIDebug1), (void**)&pdxgiDebug);
	HRESULT hResult = pdxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL,
		DXGI_DEBUG_RLO_DETAIL);
	pdxgiDebug->Release();
#endif
}

void GlobalDevice::WaitGPUComplete()
{
	gpucmd.WaitGPUComplete();
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
	pFactory->CreateSwapChainForHwnd(gpucmd.pCommandQueue, hWnd,
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

void GlobalDevice::SetResolution(int resid, bool ClientSizeUpdate)
{
	WaitGPUComplete();

	ClientFrameWidth = EnableFullScreenMode_Resolusions[resid].width;
	ClientFrameHeight = EnableFullScreenMode_Resolusions[resid].height;

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

//(40 lines) outer code : microsoft copilot.
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

GPUResource GlobalDevice::CreateCommitedGPUBuffer(D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES d3dResourceStates, D3D12_RESOURCE_DIMENSION dimension, int Width, int Height, DXGI_FORMAT BufferFormat, UINT DepthOrArraySize, D3D12_RESOURCE_FLAGS AdditionalFlag)
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
	d3dResourceDesc.DepthOrArraySize = DepthOrArraySize;
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

	d3dResourceDesc.Flags |= AdditionalFlag;

	//D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER

	D3D12_RESOURCE_STATES d3dResourceInitialStates = D3D12_RESOURCE_STATE_COMMON;
	if (heapType == D3D12_HEAP_TYPE_UPLOAD)
		d3dResourceInitialStates = D3D12_RESOURCE_STATE_GENERIC_READ;
	else if (heapType == D3D12_HEAP_TYPE_READBACK)
		d3dResourceInitialStates = D3D12_RESOURCE_STATE_COPY_DEST;

	HRESULT hResult;
	D3D12_CLEAR_VALUE ClearValue;
	if (AdditionalFlag & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) {
		ClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		ClearValue.DepthStencil.Depth = 1.0f;
		ClearValue.DepthStencil.Stencil = 0;
		hResult = gd.pDevice->CreateCommittedResource(&d3dHeapPropertiesDesc, heapFlags, &d3dResourceDesc, d3dResourceInitialStates, &ClearValue, __uuidof(ID3D12Resource), (void**)&pBuffer);
	}
	else {
		hResult = gd.pDevice->CreateCommittedResource(&d3dHeapPropertiesDesc, heapFlags, &d3dResourceDesc, d3dResourceInitialStates, NULL, __uuidof(ID3D12Resource), (void**)&pBuffer);
	}

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

		dbgc[0] += 1;
		//dbgbreak(dbgc[0] == 8);
		dbglog2(L"GPUResource %llx Created. dbgc0 = %d \n", gr.resource->GetGPUVirtualAddress(), dbgc[0]);

		gr.CurrentState_InCommandWriteLine = d3dResourceInitialStates;
		gr.extraData = 0;
		return gr;
	}
}

void GlobalDevice::UploadToCommitedGPUBuffer(void* ptr, GPUResource* uploadBuffer, GPUResource* copydestBuffer, bool StateReturning)
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

		gpucmd.ResBarrierTr(uploadBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE);
		gpucmd.ResBarrierTr(copydestBuffer, D3D12_RESOURCE_STATE_COPY_DEST);

		gpucmd->CopyResource(copydestBuffer->resource, uploadBuffer->resource);

		if (StateReturning) {
			gpucmd.ResBarrierTr(uploadBuffer, uploadBufferOriginState);
			gpucmd.ResBarrierTr(copydestBuffer, copydestOriginState);
		}
	}
}

UINT64 GlobalDevice::GetRequiredIntermediateSize(ID3D12Resource* pDestinationResource, UINT FirstSubresource, UINT NumSubresources) noexcept
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

		//imgform::PixelImageObject pio(width, height, textureData);

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
						xi = lastendxi - 1;
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
				if (retcnt < 2) goto ROW_RETURN;
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
				*(unsigned int*)&mipTex[yi * (4 * mipW) + xi * 4] = *(unsigned int*)&textureData[yi * textureMipLevel * 4 * width + xi * textureMipLevel * 4];
			}
		}
		delete[] textureData;

		GPUResource texture;
		ZeroMemory(&texture, sizeof(GPUResource));
		texture.CreateTexture_fromImageBuffer(mipW, mipH, mipTex, DXGI_FORMAT_R8G8B8A8_UNORM);
		font_texture_map[i].insert(pair<wchar_t, GPUResource>(key, texture));
		delete[] mipTex;
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

void GlobalDevice::bmpTodds(int mipmap_level, const char* Format, const char* filename)
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

void GlobalDevice::CreateDefaultHeap_VB(void* ptr, GPUResource& VertexBuffer, GPUResource& VertexUploadBuffer, D3D12_VERTEX_BUFFER_VIEW& view, UINT VertexCount, UINT sizeofVertex)
{
	UINT capacity = VertexCount * sizeofVertex;
	VertexBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, capacity, 1);
	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, capacity, 1);
	gd.UploadToCommitedGPUBuffer(ptr, &VertexUploadBuffer, &VertexBuffer, true);
	gd.gpucmd.ResBarrierTr(&VertexBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	view.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
	view.StrideInBytes = sizeofVertex;
	view.SizeInBytes = sizeofVertex * VertexCount;
}

GPUResource GlobalDevice::CreateShadowMap(int width, int height, int DSVoffset, SpotLight& spotLight)
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
	hcpu.ptr = gd.pDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + DSVoffset * gd.DSVSize;
	gd.pDevice->CreateDepthStencilView(shadowMap.resource, nullptr, hcpu);
	shadowMap.descindex.Set(false, DSVoffset, 'd'); // DepthStencilView DescHeap의 CPU HANDLE

	D3D12_CPU_DESCRIPTOR_HANDLE srvCpuH;
	D3D12_GPU_DESCRIPTOR_HANDLE srvGpuH;

	gd.ShaderVisibleDescPool.ImmortalAlloc(&spotLight.descindex, 1);

	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
	srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
	srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv_desc.Texture2D.MostDetailedMip = 0;
	srv_desc.Texture2D.MipLevels = 1;
	srv_desc.Texture2D.ResourceMinLODClamp = 0;
	srv_desc.Texture2D.PlaneSlice = 0;
	gd.pDevice->CreateShaderResourceView(shadowMap.resource, &srv_desc, spotLight.descindex.hCreation.hcpu);
	//shadowMap.handle.hgpu = spotLight.descindex.hRender.hgpu; // CBV, SRV, UAV DescHeap 의 GPU HANDLE
	return shadowMap;
}

template <int indexByteSize>
void GlobalDevice::CreateDefaultHeap_IB(void* ptr, GPUResource& IndexBuffer, GPUResource& IndexUploadBuffer, D3D12_INDEX_BUFFER_VIEW& view, UINT IndexCount)
{
	int capacity = IndexCount * indexByteSize;
	IndexBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, capacity, 1);
	IndexUploadBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, capacity, 1);
	gd.UploadToCommitedGPUBuffer(ptr, &IndexUploadBuffer, &IndexBuffer, true);
	gd.gpucmd.ResBarrierTr(&IndexBuffer, D3D12_RESOURCE_STATE_INDEX_BUFFER);
	view.BufferLocation = IndexBuffer.resource->GetGPUVirtualAddress();
	if constexpr (indexByteSize == 2) {
		view.Format = DXGI_FORMAT_R16_UINT;
	}
	else if constexpr (indexByteSize == 4) {
		view.Format = DXGI_FORMAT_R32_UINT;
	}
	view.SizeInBytes = indexByteSize * IndexCount;
}

void GPUCmd::SetShader(Shader* shader, ShaderType reg) {
	shader->Add_RegisterShaderCommand(*this, reg);
}

void RayTracingDevice::Init(void* origin_gd)
{
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
	hr = origin->gpucmd.GraphicsCmdList->QueryInterface(IID_PPV_ARGS(&dxrCommandList));
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

	gd.ShaderVisibleDescPool.ImmortalAlloc(&RTO_UAV_index, 1);
	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
	UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	device->CreateUnorderedAccessView(RayTracingOutput, nullptr, &UAVDesc, RTO_UAV_index.hCreation.hcpu);
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

	gd.raytracing.m_eye = vec4(0.0f, 2.0f, -5.0f, 1.0f);
	gd.raytracing.m_at = vec4(0.0f, 0.0f, 0.0f, 1.0f);
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
	//MappedCB->lightPosition = vec4(0.0f, 1.8f, -3.0f, 0.0f);
	MappedCB->DirLight_color = XMFLOAT3(1.0f, 1.0f, 1.0f);
	MappedCB->DirLight_intencity = 1.0f;
	vec4 dir = vec4(1, 1, 1, 0);
	dir /= dir.len3;
	MappedCB->DirLight_invDirection = dir.f3;
}

void RayTracingDevice::SetRaytracingCamera(vec4 CameraPos, vec4 look, vec4 up)
{
	vec4 at = CameraPos + look;

	gd.raytracing.MappedCB->cameraPosition = CameraPos;
	float fovAngleY = 60.0f;
	XMMATRIX view = XMMatrixLookAtLH(CameraPos, at, up);
	float m_aspectRatio = (float)gd.ClientFrameWidth / (float)gd.ClientFrameHeight;
	XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(fovAngleY), m_aspectRatio, 1.0f, 1000.0f);
	XMMATRIX viewProj = view * proj;
	gd.raytracing.MappedCB->projectionToWorld = XMMatrixTranspose(XMMatrixInverse(nullptr, viewProj));
}

void RayTracingMesh::StaticInit()
{
	auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto VbufferDesc = CD3DX12_RESOURCE_DESC::Buffer(Upload_VertexBufferCapacity);
	ThrowIfFailed(gd.pDevice->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&VbufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&Upload_vertexBuffer)));
	Upload_vertexBuffer->SetName(L"UploadVertexBuffer");
	Upload_vertexBuffer->Map(0, nullptr, (void**)&pVBMappedStart);

	auto IbufferDesc = CD3DX12_RESOURCE_DESC::Buffer(Upload_IndexBufferCapacity);
	ThrowIfFailed(gd.pDevice->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&IbufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&Upload_indexBuffer)));
	Upload_indexBuffer->SetName(L"UploadSceneIndexBuffer");
	Upload_indexBuffer->Map(0, nullptr, (void**)&pIBMappedStart);

	uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto UAV_VbufferDesc = CD3DX12_RESOURCE_DESC::Buffer(UAV_Upload_VertexBufferCapacity);
	ThrowIfFailed(gd.pDevice->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&UAV_VbufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&UAV_Upload_vertexBuffer)));
	UAV_Upload_vertexBuffer->SetName(L"UploadUAV_VertexBuffer");
	UAV_Upload_vertexBuffer->Map(0, nullptr, (void**)&pUAV_VBMappedStart);

	////////////////

	auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto DefaultVbufferDesc = CD3DX12_RESOURCE_DESC::Buffer(VertexBufferCapacity);
	ThrowIfFailed(gd.pDevice->CreateCommittedResource(
		&defaultHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&DefaultVbufferDesc,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&vertexBuffer)));
	vertexBuffer->SetName(L"SceneVertexBuffer");
	//vertexBuffer->Map(0, nullptr, (void**) & pVBMappedStart);

	auto DefaultIbufferDesc = CD3DX12_RESOURCE_DESC::Buffer(IndexBufferCapacity);
	ThrowIfFailed(gd.pDevice->CreateCommittedResource(
		&defaultHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&DefaultIbufferDesc,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&indexBuffer)));
	indexBuffer->SetName(L"SceneIndexBuffer");
	//indexBuffer->Map(0, nullptr, (void**)&pIBMappedStart);

	auto UAV_DefaultVbufferDesc = CD3DX12_RESOURCE_DESC::Buffer(UAV_VertexBufferCapacity);
	UAV_DefaultVbufferDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	ThrowIfFailed(gd.pDevice->CreateCommittedResource(
		&defaultHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&UAV_DefaultVbufferDesc,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&UAV_vertexBuffer)));
	UAV_vertexBuffer->SetName(L"UAV_SceneVertexBuffer");
	//vertexBuffer->Map(0, nullptr, (void**) & pVBMappedStart);

	//Make SRV Table Of Vertex And Index Buffer
	gd.ShaderVisibleDescPool.ImmortalAlloc(&VBIB_DescIndex, 2);
	DescHandle dh = VBIB_DescIndex.hCreation;
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc_VB = {};
	srvDesc_VB.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc_VB.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc_VB.Buffer.NumElements = VertexBufferCapacity / sizeof(Vertex);
	srvDesc_VB.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc_VB.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	srvDesc_VB.Buffer.StructureByteStride = sizeof(Vertex);
	gd.pDevice->CreateShaderResourceView(vertexBuffer, &srvDesc_VB, dh.hcpu);
	dh += gd.CBVSRVUAVSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc_IB = {};
	srvDesc_IB.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc_IB.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc_IB.Buffer.NumElements = IndexBufferCapacity / sizeof(unsigned int);
	srvDesc_IB.Format = DXGI_FORMAT_UNKNOWN;//DXGI_FORMAT_R32_TYPELESS;
	srvDesc_IB.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;//D3D12_BUFFER_SRV_FLAG_RAW;
	srvDesc_IB.Buffer.StructureByteStride = sizeof(unsigned int);//0;
	gd.pDevice->CreateShaderResourceView(indexBuffer, &srvDesc_IB, dh.hcpu);

	//////////////
	//UAV 버전
	gd.ShaderVisibleDescPool.ImmortalAlloc(&UAV_VBIB_DescIndex, 2);
	dh = UAV_VBIB_DescIndex.hCreation;
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc_UAV_VB = {};
	srvDesc_UAV_VB.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc_UAV_VB.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc_UAV_VB.Buffer.NumElements = UAV_VertexBufferCapacity / sizeof(Vertex);
	srvDesc_UAV_VB.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc_UAV_VB.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	srvDesc_UAV_VB.Buffer.StructureByteStride = sizeof(Vertex);
	gd.pDevice->CreateShaderResourceView(UAV_vertexBuffer, &srvDesc_UAV_VB, dh.hcpu);
	dh += gd.CBVSRVUAVSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc_UAV_IB = {};
	srvDesc_UAV_IB.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc_UAV_IB.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc_UAV_IB.Buffer.NumElements = IndexBufferCapacity / sizeof(unsigned int);
	srvDesc_UAV_IB.Format = DXGI_FORMAT_UNKNOWN;//DXGI_FORMAT_R32_TYPELESS;
	srvDesc_UAV_IB.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;//D3D12_BUFFER_SRV_FLAG_RAW;
	srvDesc_UAV_IB.Buffer.StructureByteStride = sizeof(unsigned int);//0;
	gd.pDevice->CreateShaderResourceView(indexBuffer, &srvDesc_UAV_IB, dh.hcpu);
}

void RayTracingMesh::AllocateRaytracingMesh(vector<Vertex> vbarr, vector<TriangleIndex> ibarr, int SubMeshNum, int* SubMeshIndexes)
{
	subMeshCount = SubMeshNum;
	int* SubMeshIndexArr = SubMeshIndexes;
	if (SubMeshIndexes == nullptr) {
		subMeshCount = 1;
		SubMeshIndexArr = new int[2];
		SubMeshIndexArr[0] = 0;
		SubMeshIndexArr[1] = ibarr.size();
	}

	static bool VBisFulling = false;
	static UINT RequireByteVB = 0;
	static bool IBisFulling = false;
	static UINT RequireByteIB = 0;

	if (vertexBuffer == nullptr) {
		RayTracingMesh::StaticInit();
	}

	if (gd.raytracing.ASBuild_ScratchResource == nullptr) {
		AllocateUAVBuffer(gd.raytracing.dxrDevice, gd.raytracing.ASBuild_ScratchResource_Maxsiz, &gd.raytracing.ASBuild_ScratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");
	}

	constexpr UINT64 VBAlign = 768; //2816;
	constexpr UINT64 IBAlign = 256;//768;

	int addtionalVB_Bytesiz = vbarr.size() * sizeof(RayTracingMesh::Vertex);
	int addtionalIB_Bytesiz = 0;
	vector<int> IBByteStart(subMeshCount);
	for (int i = 0; i < subMeshCount; ++i) {
		IBByteStart[i] = addtionalIB_Bytesiz;
		addtionalIB_Bytesiz += (SubMeshIndexArr[i + 1] - SubMeshIndexArr[i]) * sizeof(UINT);
		addtionalIB_Bytesiz = IBAlign * (1 + ((addtionalIB_Bytesiz - 1) / IBAlign));
	}
	//int addtionalIB_Bytesiz = ibarr.size() * sizeof(TriangleIndex);

	bool vb_con = VertexBufferByteSize + addtionalVB_Bytesiz <= VertexBufferCapacity;
	bool ib_con = IndexBufferByteSize + addtionalIB_Bytesiz <= IndexBufferCapacity;

	IBStartOffset = new UINT64[subMeshCount + 1];
	if (vb_con && ib_con) {
		// Create New Mesh

		VBStartOffset = VertexBufferByteSize;
		for (int i = 0; i < subMeshCount; ++i) {
			IBStartOffset[i] = IndexBufferByteSize + IBByteStart[i];
		}
		IBStartOffset[subMeshCount] = IndexBufferByteSize + addtionalIB_Bytesiz;

		pVBMapped = &pVBMappedStart[0];
		pIBMapped = &pIBMappedStart[0];

		// Copy Mesh Data
		memcpy(pVBMapped, vbarr.data(), addtionalVB_Bytesiz);
		for (int i = 0; i < subMeshCount; ++i) {

			memcpy(pIBMapped + IBByteStart[i], (char*)ibarr.data() + SubMeshIndexArr[i] * sizeof(UINT), (SubMeshIndexArr[i + 1] - SubMeshIndexArr[i]) * sizeof(UINT));
			UINT lastIndex = *((UINT*)ibarr.data() + SubMeshIndexArr[i + 1] - 1);
			for (int k = 0; k < IBAlign; ++k) {
				((UINT*)(pIBMapped + IBByteStart[i]))[SubMeshIndexArr[i + 1] - SubMeshIndexArr[i] + k] = lastIndex;
			}
		}

		//Build BLAS
		ID3D12Device5* device = gd.raytracing.dxrDevice;
		ID3D12GraphicsCommandList4* commandList = gd.raytracing.dxrCommandList;

		bool initialCmdStateClose = gd.gpucmd.isClose;
		if (gd.gpucmd.isClose) {
			gd.gpucmd.Reset(true);
		}

		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
		commandList->ResourceBarrier(1, &barrier);
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(Upload_vertexBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_SOURCE);
		commandList->ResourceBarrier(1, &barrier);

		//dbglog1(L"VBSiz : %d \n", addtionalVB_Bytesiz);
		commandList->CopyBufferRegion(vertexBuffer, VBStartOffset, Upload_vertexBuffer, 0, addtionalVB_Bytesiz);

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(1, &barrier);
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(Upload_vertexBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
		commandList->ResourceBarrier(1, &barrier);

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(indexBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
		commandList->ResourceBarrier(1, &barrier);
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(Upload_indexBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_SOURCE);
		commandList->ResourceBarrier(1, &barrier);

		//dbglog1(L"IBSiz : %d \n", addtionalIB_Bytesiz);
		commandList->CopyBufferRegion(indexBuffer, IBStartOffset[0], Upload_indexBuffer, 0, addtionalIB_Bytesiz);

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(indexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(1, &barrier);
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(Upload_indexBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
		commandList->ResourceBarrier(1, &barrier);

		gd.gpucmd.Close(true);
		gd.gpucmd.Execute(true);
		gd.WaitGPUComplete();

		//Geometry
		GeometryDescs = new D3D12_RAYTRACING_GEOMETRY_DESC[subMeshCount];
		for (int i = 0; i < subMeshCount; ++i) {
			GeometryDescs[i].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
			GeometryDescs[i].Triangles.IndexBuffer = indexBuffer->GetGPUVirtualAddress() + IBStartOffset[i];
			GeometryDescs[i].Triangles.IndexCount = (SubMeshIndexArr[i + 1] - SubMeshIndexArr[i]);
			GeometryDescs[i].Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
			GeometryDescs[i].Triangles.Transform3x4 = 0;
			GeometryDescs[i].Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
			GeometryDescs[i].Triangles.VertexCount = vbarr.size();
			GeometryDescs[i].Triangles.VertexBuffer.StartAddress = vertexBuffer->GetGPUVirtualAddress() + VBStartOffset;
			GeometryDescs[i].Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
			GeometryDescs[i].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE; // Closest Hit Shader
		}

		//BLAS Input
		BLAS_Input.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		BLAS_Input.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		BLAS_Input.NumDescs = subMeshCount;
		BLAS_Input.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		BLAS_Input.pGeometryDescs = GeometryDescs;

		//Prebuild Info
		gd.raytracing.dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&BLAS_Input, &bottomLevelPrebuildInfo);
		if (bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes <= 0) {
			throw "bottomLevelPrebuildInfo Create Failed.";
		}
		if (bottomLevelPrebuildInfo.ScratchDataSizeInBytes > gd.raytracing.ASBuild_ScratchResource_Maxsiz) {
			gd.raytracing.ASBuild_ScratchResource->Release();
			gd.raytracing.ASBuild_ScratchResource = nullptr;
			AllocateUAVBuffer(gd.raytracing.dxrDevice, bottomLevelPrebuildInfo.ScratchDataSizeInBytes, &gd.raytracing.ASBuild_ScratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");
			gd.raytracing.ASBuild_ScratchResource_Maxsiz = bottomLevelPrebuildInfo.ScratchDataSizeInBytes;
		}

		//Create BLAS Res
		D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		AllocateUAVBuffer(gd.raytracing.dxrDevice, bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &BLAS, initialResourceState, L"BottomLevelAccelerationStructure");

		// BLAS Build Desc
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
		bottomLevelBuildDesc.Inputs = BLAS_Input;
		bottomLevelBuildDesc.ScratchAccelerationStructureData = gd.raytracing.ASBuild_ScratchResource->GetGPUVirtualAddress();
		bottomLevelBuildDesc.DestAccelerationStructureData = BLAS->GetGPUVirtualAddress();

		if (gd.gpucmd.isClose) {
			gd.gpucmd.Reset(true);
		}

		CD3DX12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(BLAS);
		commandList->ResourceBarrier(1, &uavBarrier);

		commandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);

		/*gd.CmdClose(commandList);
		ID3D12CommandList* ppd3dCommandLists[] = { commandList };
		commandQueue->ExecuteCommandLists(1, ppd3dCommandLists);
		gd.WaitGPUComplete();*/

		MeshDefaultInstanceData.Transform[0][0] = MeshDefaultInstanceData.Transform[1][1] = MeshDefaultInstanceData.Transform[2][2] = 1;
		MeshDefaultInstanceData.InstanceMask = 1;
		MeshDefaultInstanceData.AccelerationStructure = BLAS->GetGPUVirtualAddress();
		MeshDefaultInstanceData.InstanceContributionToHitGroupIndex = 0;


		VertexBufferByteSize += addtionalVB_Bytesiz;
		VertexBufferByteSize = VBAlign * (1 + ((VertexBufferByteSize - 1) / VBAlign));
		//VertexBufferByteSize = ((VertexBufferByteSize + 255) & ~255);


		IndexBufferByteSize += addtionalIB_Bytesiz;
		IndexBufferByteSize = IBAlign * (1 + ((IndexBufferByteSize - 1) / IBAlign));
		//IndexBufferByteSize = ((IndexBufferByteSize + 255) & ~255);
		/*if (initialCmdStateClose == false) {
			gd.CmdReset(commandList, commandAllocator);
		}*/
	}
	else {
		if (vb_con == false) {
			if (VBisFulling) {
				RequireByteVB += addtionalVB_Bytesiz;
				dbglog1(L"Raytracing VB additional capacity require! %d \n", RequireByteVB);
			}
			else {
				dbglog1(L"Raytracing VB additional capacity require! %d \n", VertexBufferByteSize + addtionalVB_Bytesiz - VertexBufferCapacity);
				RequireByteVB += VertexBufferByteSize + addtionalVB_Bytesiz - VertexBufferCapacity;
				VBisFulling = true;
			}
		}
		if (RequireByteIB > 0) {
			if (IBisFulling) {
				RequireByteIB += addtionalIB_Bytesiz;
				dbglog1(L"Raytracing IB additional capacity require! %d \n", RequireByteIB);
			}
		}
		else {
			if (IBisFulling == false) {
				IBisFulling = true;
				RequireByteIB = IndexBufferByteSize + addtionalIB_Bytesiz - IndexBufferCapacity;
			}
			else {
				RequireByteIB += addtionalIB_Bytesiz;
			}
		}
	}
}

void RayTracingMesh::AllocateRaytracingUAVMesh(vector<Vertex> vbarr, UINT64* inIBStartOffset, int SubMeshNum, int* SubMeshIndexes)
{
	static bool VBisFulling = false;
	static UINT RequireByteVB = 0;
	static bool IBisFulling = false;
	static UINT RequireByteIB = 0;

	if (UAV_vertexBuffer == nullptr) {
		RayTracingMesh::StaticInit();
	}

	if (gd.raytracing.ASBuild_ScratchResource == nullptr) {
		AllocateUAVBuffer(gd.raytracing.dxrDevice, gd.raytracing.ASBuild_ScratchResource_Maxsiz, &gd.raytracing.ASBuild_ScratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");
	}

	constexpr UINT64 VBAlign = 768; //2816;
	constexpr UINT64 IBAlign = 256; //768;

	int addtionalVB_Bytesiz = vbarr.size() * sizeof(RayTracingMesh::Vertex);

	bool vb_con = UAV_VertexBufferByteSize + addtionalVB_Bytesiz <= UAV_VertexBufferCapacity;
	if (vb_con) {
		// Create New Mesh
		UAV_VBStartOffset = UAV_VertexBufferByteSize;
		pUAV_VBMapped = &pVBMappedStart[0];

		// Copy Mesh Data
		memcpy(pUAV_VBMapped, vbarr.data(), addtionalVB_Bytesiz);

		//Build BLAS
		ID3D12Device5* device = gd.raytracing.dxrDevice;
		ID3D12GraphicsCommandList4* commandList = gd.raytracing.dxrCommandList;

		bool initialCmdStateClose = gd.gpucmd.isClose;
		if (gd.gpucmd.isClose) {
			gd.gpucmd.Reset(true);
		}

		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(UAV_vertexBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
		commandList->ResourceBarrier(1, &barrier);
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(UAV_Upload_vertexBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_SOURCE);
		commandList->ResourceBarrier(1, &barrier);

		//dbglog1(L"VBSiz : %d \n", addtionalVB_Bytesiz);
		commandList->CopyBufferRegion(UAV_vertexBuffer, UAV_VBStartOffset, UAV_Upload_vertexBuffer, 0, addtionalVB_Bytesiz);

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(UAV_vertexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(1, &barrier);
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(UAV_Upload_vertexBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
		commandList->ResourceBarrier(1, &barrier);

		gd.gpucmd.Close(true);
		gd.gpucmd.Execute(true);
		gd.WaitGPUComplete();

		//Geometry
		GeometryDescs = new D3D12_RAYTRACING_GEOMETRY_DESC[SubMeshNum];
		for (int i = 0; i < SubMeshNum; ++i) {
			GeometryDescs[i].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
			GeometryDescs[i].Triangles.IndexBuffer = indexBuffer->GetGPUVirtualAddress() + inIBStartOffset[i];
			GeometryDescs[i].Triangles.IndexCount = (SubMeshIndexes[i + 1] - SubMeshIndexes[i]);
			GeometryDescs[i].Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
			GeometryDescs[i].Triangles.Transform3x4 = 0;
			GeometryDescs[i].Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
			GeometryDescs[i].Triangles.VertexCount = vbarr.size();
			GeometryDescs[i].Triangles.VertexBuffer.StartAddress = UAV_vertexBuffer->GetGPUVirtualAddress() + UAV_VBStartOffset;
			GeometryDescs[i].Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
			GeometryDescs[i].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE; // Closest Hit Shader
		}

		//BLAS Input
		BLAS_Input.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		BLAS_Input.Flags =
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE |
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
		BLAS_Input.NumDescs = subMeshCount;
		BLAS_Input.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		BLAS_Input.pGeometryDescs = GeometryDescs;

		//Prebuild Info
		gd.raytracing.dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&BLAS_Input, &bottomLevelPrebuildInfo);
		if (bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes <= 0) {
			throw "bottomLevelPrebuildInfo Create Failed.";
		}

		if (bottomLevelPrebuildInfo.ScratchDataSizeInBytes > gd.raytracing.ASBuild_ScratchResource_Maxsiz) {
			gd.raytracing.ASBuild_ScratchResource->Release();
			gd.raytracing.ASBuild_ScratchResource = nullptr;
			AllocateUAVBuffer(gd.raytracing.dxrDevice, bottomLevelPrebuildInfo.ScratchDataSizeInBytes, &gd.raytracing.ASBuild_ScratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");
			gd.raytracing.ASBuild_ScratchResource_Maxsiz = bottomLevelPrebuildInfo.ScratchDataSizeInBytes;
		}

		//Create BLAS Res
		D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		AllocateUAVBuffer(gd.raytracing.dxrDevice, bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &BLAS, initialResourceState, L"BottomLevelAccelerationStructure");

		// BLAS Build Desc
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
		bottomLevelBuildDesc.Inputs = BLAS_Input;
		bottomLevelBuildDesc.ScratchAccelerationStructureData = gd.raytracing.ASBuild_ScratchResource->GetGPUVirtualAddress();
		bottomLevelBuildDesc.DestAccelerationStructureData = BLAS->GetGPUVirtualAddress();

		if (gd.gpucmd.isClose) {
			gd.gpucmd.Reset(true);
		}

		CD3DX12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(BLAS);
		commandList->ResourceBarrier(1, &uavBarrier);

		commandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);

		/*gd.CmdClose(commandList);
		ID3D12CommandList* ppd3dCommandLists[] = { commandList };
		commandQueue->ExecuteCommandLists(1, ppd3dCommandLists);
		gd.WaitGPUComplete();*/
		// 이 주석은 애초에 Reset 상태에서 이 함수를 호출하기 때문에 주석을 처리함.
		// 만약 Reset이 아니라면 이걸 해주는 것이 맞다.
		// fix : 이 함수가 어떤 커맨드 상태로도 실행될 수 있도록 만드는 것.

		MeshDefaultInstanceData.Transform[0][0] = MeshDefaultInstanceData.Transform[1][1] = MeshDefaultInstanceData.Transform[2][2] = 1;
		MeshDefaultInstanceData.InstanceMask = 1;
		MeshDefaultInstanceData.AccelerationStructure = BLAS->GetGPUVirtualAddress();
		MeshDefaultInstanceData.InstanceContributionToHitGroupIndex = 0;

		UAV_VertexBufferByteSize += addtionalVB_Bytesiz;
		UAV_VertexBufferByteSize = VBAlign * (1 + ((UAV_VertexBufferByteSize - 1) / VBAlign));
	}
	else {
		if (vb_con == false) {
			if (VBisFulling) {
				RequireByteVB += addtionalVB_Bytesiz;
				dbglog1(L"Raytracing VB additional capacity require! %d \n", RequireByteVB);
			}
			else {
				dbglog1(L"Raytracing VB additional capacity require! %d \n", UAV_VertexBufferByteSize + addtionalVB_Bytesiz - UAV_VertexBufferCapacity);
				RequireByteVB += UAV_VertexBufferByteSize + addtionalVB_Bytesiz - UAV_VertexBufferCapacity;
				VBisFulling = true;
			}
		}
	}
}

void RayTracingMesh::AllocateRaytracingUAVMesh_OnlyIndex(vector<TriangleIndex> ibarr, int SubMeshNum, int* SubMeshIndexes)
{
	subMeshCount = SubMeshNum;
	int* SubMeshIndexArr = SubMeshIndexes;
	if (SubMeshIndexes == nullptr) {
		subMeshCount = 1;
		SubMeshIndexArr = new int[2];
		SubMeshIndexArr[0] = 0;
		SubMeshIndexArr[1] = ibarr.size();
	}

	static bool VBisFulling = false;
	static UINT RequireByteVB = 0;
	static bool IBisFulling = false;
	static UINT RequireByteIB = 0;

	if (vertexBuffer == nullptr) {
		RayTracingMesh::StaticInit();
	}

	if (gd.raytracing.ASBuild_ScratchResource == nullptr) {
		AllocateUAVBuffer(gd.raytracing.dxrDevice, gd.raytracing.ASBuild_ScratchResource_Maxsiz, &gd.raytracing.ASBuild_ScratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");
	}

	constexpr UINT64 VBAlign = 768; //2816;
	constexpr UINT64 IBAlign = 256;//768;

	int addtionalIB_Bytesiz = 0;
	vector<int> IBByteStart(subMeshCount);
	for (int i = 0; i < subMeshCount; ++i) {
		IBByteStart[i] = addtionalIB_Bytesiz;
		addtionalIB_Bytesiz += (SubMeshIndexArr[i + 1] - SubMeshIndexArr[i]) * sizeof(UINT);
		addtionalIB_Bytesiz = IBAlign * (1 + ((addtionalIB_Bytesiz - 1) / IBAlign));
	}

	bool ib_con = IndexBufferByteSize + addtionalIB_Bytesiz <= IndexBufferCapacity;
	IBStartOffset = new UINT64[subMeshCount + 1];
	if (ib_con) {
		// Create New Mesh
		for (int i = 0; i < subMeshCount; ++i) {
			IBStartOffset[i] = IndexBufferByteSize + IBByteStart[i];
		}
		IBStartOffset[subMeshCount] = IndexBufferByteSize + addtionalIB_Bytesiz;
		pIBMapped = &pIBMappedStart[0];

		// Copy Mesh Data
		for (int i = 0; i < subMeshCount; ++i) {
			memcpy(pIBMapped + IBByteStart[i], (char*)ibarr.data() + SubMeshIndexArr[i] * sizeof(UINT), (SubMeshIndexArr[i + 1] - SubMeshIndexArr[i]) * sizeof(UINT));
			UINT lastIndex = *((UINT*)ibarr.data() + SubMeshIndexArr[i + 1] - 1);
			for (int k = 0; k < IBAlign; ++k) {
				((UINT*)(pIBMapped + IBByteStart[i]))[SubMeshIndexArr[i + 1] - SubMeshIndexArr[i] + k] = lastIndex;
			}
		}

		//Build BLAS
		ID3D12Device5* device = gd.raytracing.dxrDevice;
		ID3D12GraphicsCommandList4* commandList = gd.raytracing.dxrCommandList;

		bool initialCmdStateClose = gd.gpucmd.isClose;
		if (gd.gpucmd.isClose) {
			gd.gpucmd.Reset(true);
		}

		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(indexBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
		commandList->ResourceBarrier(1, &barrier);
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(Upload_indexBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_SOURCE);
		commandList->ResourceBarrier(1, &barrier);

		//dbglog1(L"IBSiz : %d \n", addtionalIB_Bytesiz);
		commandList->CopyBufferRegion(indexBuffer, IBStartOffset[0], Upload_indexBuffer, 0, addtionalIB_Bytesiz);

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(indexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(1, &barrier);
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(Upload_indexBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
		commandList->ResourceBarrier(1, &barrier);

		gd.gpucmd.Close(true);
		gd.gpucmd.Execute(true);
		gd.WaitGPUComplete();

		if (gd.gpucmd.isClose) {
			gd.gpucmd.Reset(true);
		}

		IndexBufferByteSize += addtionalIB_Bytesiz;
		IndexBufferByteSize = IBAlign * (1 + ((IndexBufferByteSize - 1) / IBAlign));
	}
	else {
		if (RequireByteIB > 0) {
			if (IBisFulling) {
				RequireByteIB += addtionalIB_Bytesiz;
				dbglog1(L"Raytracing IB additional capacity require! %d \n", RequireByteIB);
			}
		}
		else {
			if (IBisFulling == false) {
				IBisFulling = true;
				RequireByteIB = IndexBufferByteSize + addtionalIB_Bytesiz - IndexBufferCapacity;
			}
			else {
				RequireByteIB += addtionalIB_Bytesiz;
			}
		}
	}
}

void RayTracingMesh::UAV_BLAS_Refit()
{
	//Build BLAS
	ID3D12Device5* device = gd.raytracing.dxrDevice;
	ID3D12GraphicsCommandList4* commandList = gd.raytracing.dxrCommandList;

	D3D12_GPU_VIRTUAL_ADDRESS UsingScratchBufferVA;

	//Prebuild Info
	BLAS_Input.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;

	gd.raytracing.dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&BLAS_Input, &bottomLevelPrebuildInfo);
	if (bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes <= 0) {
		throw "bottomLevelPrebuildInfo Create Failed.";
	}
	if (gd.raytracing.UsingScratchSize + bottomLevelPrebuildInfo.ScratchDataSizeInBytes > gd.raytracing.ASBuild_ScratchResource_Maxsiz) {
		// 이전의 Scratched Buffer 사용을 모두 끝낸다.
		gd.gpucmd.Close(true);
		gd.gpucmd.Execute(true);
		gd.gpucmd.WaitGPUComplete();
		gd.gpucmd.Reset(true);
		gd.raytracing.UsingScratchSize = 0;
	}

	UINT64 AllocSiz = bottomLevelPrebuildInfo.ScratchDataSizeInBytes;
	AllocSiz = 256 * (1 + ((AllocSiz - 1) / 256));
	UsingScratchBufferVA = gd.raytracing.ASBuild_ScratchResource->GetGPUVirtualAddress() + gd.raytracing.UsingScratchSize;
	gd.raytracing.UsingScratchSize += AllocSiz;

	// BLAS Build Desc
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
	bottomLevelBuildDesc.Inputs = BLAS_Input;
	bottomLevelBuildDesc.ScratchAccelerationStructureData = UsingScratchBufferVA;

	// 이둘을 같게하면 Refit함. SourceAccelerationStructureData 0이면 build.
	bottomLevelBuildDesc.SourceAccelerationStructureData = BLAS->GetGPUVirtualAddress();
	bottomLevelBuildDesc.DestAccelerationStructureData = BLAS->GetGPUVirtualAddress();

	CD3DX12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(BLAS);
	commandList->ResourceBarrier(1, &uavBarrier);

	commandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
}

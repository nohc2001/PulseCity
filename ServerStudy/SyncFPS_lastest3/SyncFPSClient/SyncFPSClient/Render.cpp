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

	ShaderVisibleDescPool.Initialize(4096);
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
		texture.CreateTexture_fromImageBuffer(mipW, mipH, mipTex);
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
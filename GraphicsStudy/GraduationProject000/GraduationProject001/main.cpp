#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include <DXGIDebug.h>
#include <tchar.h>
using namespace DirectX;
using namespace DirectX::PackedVector;
//using Microsoft::WRL::ComPtr; -> question 001

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

#define FRAME_BUFFER_WIDTH 1280
#define FRAME_BUFFER_HEIGHT 720

typedef unsigned int ui32;
typedef unsigned long long ui64;

HINSTANCE g_hInst;
HWND hWnd;
LPCTSTR lpszClass = L"Pulse City Client 001";
LPCTSTR lpszWindowName = L"Pulse City Client 001";

// name completly later. ??
struct GlobalDevice {
	IDXGIFactory7* pFactory;// question 002 : why dxgi 6, but is type limit 7 ??
	IDXGISwapChain4* pSwapChain = nullptr;
	ID3D12Device* pDevice;

	static constexpr unsigned int SwapChainBufferCount = 2;
	ui32 CurrentSwapChainBufferIndex;

	// maybe seperate later.. ?? i don't know future..
	ui32 RtvDescriptorIncrementSize;
	ID3D12Resource* ppRenderTargetBuffers[SwapChainBufferCount];
	ID3D12DescriptorHeap* pRtvDescriptorHeap;
	ui32 ClientFrameWidth;
	ui32 ClientFrameHeight;

	ID3D12Resource* pDepthStencilBuffer;
	ID3D12DescriptorHeap* pDsvDescriptorHeap;
	ui32 DsvDescriptorIncrementSize;

	ID3D12CommandQueue* pCommandQueue;
	ID3D12CommandAllocator* pCommandAllocator;
	ID3D12GraphicsCommandList* pCommandList;

	ID3D12PipelineState* pPipelineState;

	ID3D12Fence* pFence;
	ui64 FenceValue;
	HANDLE hFenceEvent;

	D3D12_VIEWPORT Viewport;
	D3D12_RECT ScissorRect;

	// anti aliasing multi sampling
	ui32 m_nMsaa4xQualityLevels = 0; //MSAA level
	bool m_bMsaa4xEnable = false; //active MSAA

	void Init();
	void Release();
	void NewSwapChain();
	void WaitGPUComplete();
};
GlobalDevice gd;

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdParam, int nCmdShow) {
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

	hWnd = CreateWindow(lpszClass, lpszWindowName, WS_OVERLAPPEDWINDOW, 0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT, NULL, (HMENU)NULL, hInstance, NULL);
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	while (1) {
		if (::PeekMessage(&Message, NULL, 0, 0, PM_REMOVE))
		{
			if (Message.message == WM_QUIT) break;
			::TranslateMessage(&Message);
			::DispatchMessage(&Message);
			//if (!::TranslateAccelerator(Message.hwnd, hAccelTable, &Message))
			//{
			//	
			//}
		}
		else
		{
			//gGameFramework.FrameAdvance();
		}
	}
	

	return Message.wParam;
}

RECT rt;
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	HDC hdc;
	switch (uMsg) {
	case WM_CREATE:
		gd.Init();
		GetClientRect(hWnd, &rt);
		break;
	case WM_DESTROY:
		gd.Release();
		PostQuitMessage(0);
		break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void GlobalDevice::Init()
{
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
	Viewport.TopLeftX = 0;
	Viewport.TopLeftY = 0;
	Viewport.Width = (float)ClientFrameWidth;
	Viewport.Height = (float)ClientFrameHeight;
	Viewport.MinDepth = 0.0f;
	Viewport.MaxDepth = 1.0f;
	ScissorRect = { 0, 0, (long)ClientFrameWidth, (long)ClientFrameHeight };

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

	NewSwapChain();
}

void GlobalDevice::Release()
{
	if (pCommandList) pCommandList->Release();
	if (pCommandAllocator) pCommandAllocator->Release();
	if (pCommandQueue) pCommandQueue->Release();
	
	CloseHandle(hFenceEvent);
	if (pFence) pFence->Release();

	pSwapChain->SetFullscreenState(FALSE, NULL);
	if (pSwapChain) pSwapChain->Release();

	if (pDevice) pDevice->Release();
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

	RECT rcClient;
	::GetClientRect(hWnd, &rcClient);
	ClientFrameWidth = rcClient.right - rcClient.left;
	ClientFrameHeight = rcClient.bottom - rcClient.top;
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
	dxgiSwapChainDesc.Flags = 0;
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
}

#pragma once
#include "D3D12ResourceManager.h"
#include "typedef.h"
#include "SimpleConstantBufferPool.h"
#include "DescriptorPool.h"
#include "DescriptorAllocator.h"

const UINT SWAP_CHAIN_FRAME_COUNT = 2;

class CD3D12ResourceManager;
class CD3D12Renderer
{
	static const UINT MAX_DRAW_COUNT_PER_FRAME = 256;
	

	HWND	m_hWnd = nullptr;
	ID3D12Device5*	m_pD3DDevice = nullptr;
	ID3D12CommandQueue*	m_pCommandQueue = nullptr;
	CD3D12ResourceManager* m_pResourceManager = nullptr;
	ID3D12CommandAllocator* m_pCommandAllocator = nullptr;
	ID3D12GraphicsCommandList* m_pCommandList = nullptr;

	CDescriptorPool* m_pDescriptorPool = nullptr;
	CSimpleConstantBufferPool* m_pConstantBufferPool = nullptr;
	Descriptor256Allocator* m_pSingleDescriptorAllocator = nullptr;
	
	UINT64	m_ui64FenceValue = 0;

	D3D_FEATURE_LEVEL	m_FeatureLevel = D3D_FEATURE_LEVEL_11_0;
	DXGI_ADAPTER_DESC1	m_AdapterDesc = {};
	IDXGISwapChain3*	m_pSwapChain = nullptr;
	D3D12_VIEWPORT	m_Viewport = {};
	D3D12_RECT		m_ScissorRect = {};

	ID3D12Resource*				m_pRenderTargets[SWAP_CHAIN_FRAME_COUNT] = {};
	ID3D12Resource* m_pDepthStencil = nullptr;
	ID3D12DescriptorHeap*		m_pRTVHeap = nullptr;
	ID3D12DescriptorHeap*		m_pDSVHeap = nullptr;
	ID3D12DescriptorHeap*		m_pSRVHeap = nullptr;
	UINT	m_rtvDescriptorSize = 0;
	UINT	m_srvDescriptorSize = 0;
	UINT	m_dsvDescriptorSize = 0;
	UINT	m_dwSwapChainFlags = 0;
	UINT	m_uiRenderTargetIndex = 0;
	HANDLE	m_hFenceEvent = nullptr;
	ID3D12Fence* m_pFence = nullptr;
	UINT64 m_nFenceValues[SWAP_CHAIN_FRAME_COUNT] = {};

	DWORD	m_dwCurContextIndex = 0;
	
	void	CreateFence();
	void	CleanupFence();
	void	CreateCommandList();
	void	CleanupCommandList();
	BOOL	CreateDescriptorHeapForRTV();
	BOOL	CreateDescriptorHeapForDSV();
	void	CleanupDescriptorHeapForRTV();
	void	CleanupDescriptorHeapForDSV();
	//BOOL	CreateDescriptorHeap();
	//void	CleanupDescriptorHeap();

	UINT64	Fence();
	void	WaitForFenceValue();

	void	Cleanup();

public:
	UINT ClientWidth = 1920;
	UINT ClientHeight = 1080;

	BOOL	Initialize(HWND hWnd, BOOL bEnableDebugLayer, BOOL bEnableGBV);
	void	BeginRender();
	void	EndRender();
	void	Present();
	BOOL	UpdateWindowSize(DWORD dwBackBuffWid, DWORD dwBackBuffHei);
	BOOL	CreateDepthStencil(UINT Width, UINT Height);
	
	void* CreateBasicMeshObject();
	void DeleteBasicMeshObject(void* pMeshObjHandle);
	void RenderMeshObject(void* pMeshObjHandle, const CONSTANT_BUFFER_DEFAULT& constBuff, const TEXTURES_VECTOR& texvec);

	//for internal?
	ID3D12Device5* INL_GetD3DDevice() const { return m_pD3DDevice; }
	CD3D12ResourceManager* INL_GetResourceManager() { return m_pResourceManager; }

	// window size rate return
	float GetWindowWHRate() { 
		return (float)m_dwWidth / (float)m_dwHeight; 
	}

	CDescriptorPool* INL_GetDescriptorPool() { return m_pDescriptorPool; }
	CSimpleConstantBufferPool* INL_GetConstantBufferPool() { return m_pConstantBufferPool; }
	UINT INL_GetSrvDescriptorSize() { return m_srvDescriptorSize; }
	void* AllocTextureDesc(const char* path);
	void FreeTextureDesc(void* tex_handle);
	ID3D12GraphicsCommandList* GetCommandList() { return m_pCommandList; }
	
	void ChangeSwapChainState();
	void waitForGpuComplete()
	{
		//CPU 펜스의 값을 증가한다.
		UINT64 nFenceValue = ++m_nFenceValues[m_uiRenderTargetIndex];

		//GPU가 펜스의 값을 설정하는 명령을 명령 큐에 추가한다.
		HRESULT hResult = m_pCommandQueue->Signal(m_pFence, nFenceValue);

		if (m_pFence->GetCompletedValue() < nFenceValue)
		{
			//펜스의 현재 값이 설정한 값보다 작으면 펜스의 현재 값이 설정한 값이 될 때까지 기다린다.
			hResult = m_pFence->SetEventOnCompletion(nFenceValue, m_hFenceEvent);
			::WaitForSingleObject(m_hFenceEvent, INFINITE);
		}
	}

	CD3D12Renderer();
	~CD3D12Renderer();
};


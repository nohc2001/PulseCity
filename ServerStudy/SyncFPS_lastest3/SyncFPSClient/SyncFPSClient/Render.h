#pragma once
#include "stdafx.h"
#include "GraphicDefs.h"
#include "ttfParser.h"
#include "SpaceMath.h"

using namespace TTFFontParser;

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

	ID3D12Fence* pFence;
	ui64 FenceValue;
	HANDLE hFenceEvent;

	static constexpr int ViewportCount = 2;
	ViewportData viewportArr[ViewportCount];
	// CommandList->RSSetViewport
	// CommandList->RSSetScissorRect

	// anti aliasing multi sampling
	ui32 m_nMsaa4xQualityLevels = 0; //MSAA level
	bool m_bMsaa4xEnable = false; //active MSAA

	int screenWidth;
	int screenHeight;
	float ScreenRatio = 1; // 0.75 -> GA / 0.5625 -> HD / 0.625 -> WGA

	RAWINPUTDEVICE RawMouse;
	BYTE InputTempBuffer[4096] = { };

	static constexpr int FontCount = 2;
	string font_filename[FontCount];
	TTFFontParser::FontData font_data[FontCount];
	unordered_map<wchar_t, GPUResource, hash<wchar_t>> font_texture_map[FontCount];
	vector<wchar_t> addTextureStack;

	void Init();
	void Release();
	void NewSwapChain();
	void NewGbuffer();
	void WaitGPUComplete();

	RenderingMod RenderMod = ForwardRendering;
	DXGI_FORMAT MainRenderTarget_PixelFormat;
	static constexpr int GbufferCount = 1;
	DXGI_FORMAT GbufferPixelFormatArr[GbufferCount];
	ID3D12DescriptorHeap* GbufferDescriptorHeap = nullptr;
	ID3D12Resource* ppGBuffers[GbufferCount] = {};

	DescriptorAllotter TextureDescriptorAllotter;
	GPUResource GlobalTextureArr[64] = {};
	enum GlobalTexture {
		GT_TileTex = 0,
		GT_WallTex = 1,
		GT_Monster = 2,
	};

	ShaderVisibleDescriptorPool ShaderVisibleDescPool;

	//Create GPU Heaps that contain RTV, DSV Datas
	void Create_RTV_DSV_DescriptorHeaps();
	void CreateRenderTargetViews();
	void CreateGbufferRenderTargetViews();
	void CreateDepthStencilView();
	// when using Gbuffer for Defered Rendering, it require many RenderTarget

	void SetFullScreenMode(bool isFullScreen);
	void SetResolution(int resid, bool ClientSizeUpdate);
	ResolutionStruct* GetResolutionArr();

	static int PixelFormatToPixelSize(DXGI_FORMAT format);
	GPUResource CreateCommitedGPUBuffer(ID3D12GraphicsCommandList* commandList, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES d3dResourceStates, D3D12_RESOURCE_DIMENSION dimension, int Width, int Height, DXGI_FORMAT BufferFormat = DXGI_FORMAT_UNKNOWN);
	void UploadToCommitedGPUBuffer(ID3D12GraphicsCommandList* commandList, void* ptr, GPUResource* uploadBuffer, GPUResource* copydestBuffer = nullptr, bool StateReturning = true);

	UINT64 GetRequiredIntermediateSize(
		_In_ ID3D12Resource* pDestinationResource,
		_In_range_(0, D3D12_REQ_SUBRESOURCES) UINT FirstSubresource,
		_In_range_(0, D3D12_REQ_SUBRESOURCES - FirstSubresource) UINT NumSubresources) noexcept;

	//this function cannot be executing while command list update.
	void AddTextTexture(wchar_t key);

	void AddLineInTexture(float_v2 startpos, float_v2 endpos, BYTE* texture, int width, int height);
};

extern GlobalDevice gd;
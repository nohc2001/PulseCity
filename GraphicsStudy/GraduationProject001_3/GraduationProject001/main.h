#pragma once
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

#include <vector>
#include <fstream>
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
int resolutionLevel = 3;

#define QUERYPERFORMANCE_HZ 10000000//Hz
static inline ui64 GetTicks()
{
	LARGE_INTEGER ticks;
	QueryPerformanceCounter(&ticks);
	return ticks.QuadPart;
}

struct ResolutionStruct {
	ui32 width;
	ui32 height;
};

//ratio = 4 : 3
constexpr ui32 ResolutionArr_GA[10][2] = {
	{640, 480}, {800, 600}, {1024, 768}, {1152, 864},
	{1400, 1050}, {1600, 1200}, {2048, 1536}, {3200, 2400},
	{4096, 3072}, {6400, 4800}
};
enum class ResolutionName_GA {
	VGA = 0,
	SVGA = 1,
	XGA = 2,
	XGAplus = 3,
	SXGAplus = 4,
	UXGA = 5,
	QXGA = 6,
	QUXGA = 7,
	HXGA = 8,
	HUXGA = 9
};

//ratio = 16 : 9
constexpr ui32 ResolutionArr_HD[12][2] = {
	{640, 360}, {854, 480}, {960, 540}, {1280, 720}, {1600, 900},
	{1920, 1080}, {2560, 1440}, {2880, 1620}, {3200, 1800}, {3840, 2160}, {5120, 2880}, {7680, 4320}
};
enum class ResolutionName_HD {
	nHD = 0,
	SD = 1,
	qHD = 2,
	HD = 3,
	HDplus = 4,
	FHD = 5,
	QHD = 6,
	QHDplus0 = 7,
	QHDplus1 = 8,
	UHD = 9,
	UHDplus = 10,
	FUHD = 11
};

//ratio = 16 : 10
constexpr ui32 ResolutionArr_WGA[10][2] = {
	{1280, 800}, {1440, 900}, {1680, 1050}, {1920, 1200},
	{2560, 1600}, {2880, 1800}, {3072, 1920}, {3840, 2400},
	{5120, 3200}, {7680, 4800}
};
enum class ResolutionName_WGA {
	WXGA = 0,
	WXGAplus = 1,
	WSXGA = 2,
	WUXGA = 3,
	WQXGA0 = 4,
	WQXGA1 = 5,
	WQXGA2 = 6,
	WQUXGA = 7,
	WHXGA = 8,
	WHUXGA = 9
};

struct ViewportData {
	D3D12_VIEWPORT Viewport;
	D3D12_RECT ScissorRect;
	ui64 LayerFlag = 0; // layers that contained.
	XMMATRIX ViewMatrix;
	XMMATRIX ProjectMatrix;
};

enum RenderingMod {
	ForwardRendering = 0,
	DeferedRendering = 1
};

struct GPUResource {
	ID3D12Resource2* resource;

	D3D12_RESOURCE_STATES CurrentState_InCommandWriteLine;
	//when add Resource Barrier command in command list, It Simulate Change Of Resource State that executed by GPU later.
	//so this member variable is not current Resource state.
	//it is current state of present command writing line.

	ui32 extraData;

	void AddResourceBarrierTransitoinToCommand(ID3D12GraphicsCommandList* cmd, D3D12_RESOURCE_STATES afterState);
	D3D12_RESOURCE_BARRIER GetResourceBarrierTransition(D3D12_RESOURCE_STATES afterState);
};

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
};
GlobalDevice gd;

class Shader {
public:
	ID3D12PipelineState* pPipelineState = nullptr;
	ID3D12RootSignature* pRootSignature = nullptr;
	Shader();
	virtual ~Shader();

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();
	void Add_RegisterShaderCommand(ID3D12GraphicsCommandList* commandList);

	static D3D12_SHADER_BYTECODE GetShaderByteCode(const WCHAR* pszFileName, LPCSTR pszShaderName, LPCSTR pszShaderProfile, ID3DBlob** ppd3dShaderBlob);
};

struct TriangleIndex {
	UINT v[3];

	bool operator==(TriangleIndex ti) {
		v[0] = ti.v[0];
		v[1] = ti.v[1];
		v[2] = ti.v[2];
	}

	TriangleIndex() {}
	TriangleIndex(UINT a0, UINT a1, UINT a2) {
		v[0] = a0;
		v[1] = a1;
		v[2] = a2;
	}
};

struct Color {
	XMFLOAT4 v;
	operator XMFLOAT4() const { return v; }

	static Color RandomColor() {
		Color c;
		c.v.x = (float)(rand() & 255) / 255.0f;
		c.v.y = (float)(rand() & 255) / 255.0f;
		c.v.z = (float)(rand() & 255) / 255.0f;
		c.v.w = 0.5f + (float)(rand() & 127) / 127.0f;
		return c;
	}
};

class Mesh {
	GPUResource VertexBuffer;
	GPUResource VertexUploadBuffer;
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView;
	
	GPUResource IndexBuffer;
	GPUResource IndexUploadBuffer;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView;

	ui32 IndexNum = 0;
	ui32 VertexNum = 0;
	D3D12_PRIMITIVE_TOPOLOGY topology;

	XMFLOAT3 MAXpos = { 0, 0, 0 }; // it can replace Mesh OBB.
public:
	struct Vertex {
		XMFLOAT3 position;
		XMFLOAT4 color;
		XMFLOAT3 normal;

		Vertex() {}
		Vertex(XMFLOAT3 p, XMFLOAT4 col, XMFLOAT3 nor) 
			: position{ p }, color{ col }, normal{nor}
		{}
	};

	Mesh();
	virtual ~Mesh();

	virtual void ReadMeshFromFile_OBJ(ID3D12GraphicsCommandList* pCommandList, const char* path);
	virtual void Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum);
	BoundingOrientedBox GetOBB();
	void CreateGroundMesh(ID3D12GraphicsCommandList* pCommandList);
	void CreateWallMesh(ID3D12GraphicsCommandList* pCommandList);
};

class GameObject {
protected:
	XMMATRIX m_worldMatrix;
	Mesh* m_pMesh = nullptr;
	Shader* m_pShader = nullptr;
public:
	GameObject();
	virtual ~GameObject();

	virtual void Update(float delatTime);
	virtual void Render(ID3D12GraphicsCommandList* commandList);

	void SetMesh(Mesh* pMesh);
	void SetShader(Shader* pShader);
	void SetWorldMatrix(const XMMATRIX& worldMatrix);

	const XMMATRIX& GetWorldMatrix() const;

	BoundingOrientedBox GetOBB();

	virtual void OnCollision(GameObject* other);
};

class Player : public GameObject {
	XMMATRIX m_preWorldMat;
	UCHAR* m_pKeyBuffer;
public:
	Player(UCHAR* pKeyBuffer) : m_pKeyBuffer(pKeyBuffer) {}

	virtual void Update(float deltaTime) override;

	virtual void OnCollision(GameObject* other) override;

};

class Game {
	Shader* MyShader;
	std::vector<GameObject*> m_gameObjects; // GameObjets

public:
	float DeltaTime;
	UCHAR pKeyBuffer[256];
	Game(){}
	~Game() {}
	void Init();
	void Render();
	void Update();
};
Game game;

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
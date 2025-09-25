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

#include "vecset.h"
#include "SpaceMath.h"
#include "Utill_ImageFormating.h"

//// 4096 x 4096 (rgba)
//#define IMAGE_MAX_SIZE 67108864
//struct TempImageMemory {
//	static NextAllotter allotter;
//	static ui8 Data[IMAGE_MAX_SIZE];
//
//#define StaticInit_TempImageMemory \
//NextAllotter TempImageMemory::allotter;\
//ui8 TempImageMemory::Data[IMAGE_MAX_SIZE];
//
//	static void* Alloc(int siz) {
//
//	}
//};
//
//#ifndef STBI_MALLOC
//#define STBI_MALLOC(sz)           malloc(sz)
//#define STBI_REALLOC(p,newsz)     realloc(p,newsz)
//#define STBI_FREE(p)              free(p)
//#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define TTF_FONT_PARSER_IMPLEMENTATION
#include "ttfParser.h"
using namespace TTFFontParser;

using namespace std;
using namespace DirectX;
using namespace DirectX::PackedVector;
//using Microsoft::WRL::ComPtr; -> question 001

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

wstring const_str_to_wstr(const char* str) {
	wstring wstr;
	int len = strlen(str);
	wstr.reserve(len + 1);
	for (int i = 0; i < len; ++i) {
		wstr.push_back(str[i]);
	}
	return wstr;
}

#pragma region dbglogDefines
#define dbglog1(format, A) \
	{\
		wchar_t str[512] = {}; \
		wchar_t format0[128] = L"%s line %d : "; \
		wcscpy_s(format0+13, 128-13, format);\
		swprintf_s(str, 512, format0, _T(__FUNCTION__), __LINE__, A); \
		OutputDebugString(str);\
	}
#define dbglog2(format, A, B) \
	{\
		wchar_t str[512] = {}; \
		wchar_t format0[128] = L"%s line %d : "; \
		wcscpy_s(format0+13, 128-13, format);\
		swprintf_s(str, 512, format0, _T(__FUNCTION__), __LINE__, A, B); \
		OutputDebugString(str);\
	}
#define dbglog3(format, A, B, C) \
	{\
		wchar_t str[512] = {}; \
		wchar_t format0[128] = L"%s line %d : "; \
		wcscpy_s(format0+13, 128-13, format);\
		swprintf_s(str, 512, format0, _T(__FUNCTION__), __LINE__, A, B, C); \
		OutputDebugString(str);\
	}
#define dbglog4(format, A, B, C, D) \
	{\
		wchar_t str[512] = {}; \
		wchar_t format0[128] = L"%s line %d : "; \
		wcscpy_s(format0+13, 128-13, format);\
		swprintf_s(str, 512, format, _T(__FUNCTION__), __LINE__, A, B, C, D); \
		OutputDebugString(str);\
	}
#pragma endregion

struct WinEvent {
	HWND hWnd;
	UINT Message;
	WPARAM wParam;
	LPARAM lParam;

	WinEvent(HWND hwnd, UINT msg, WPARAM wP, LPARAM lP) {
		hWnd = hwnd;
		Message = msg;
		wParam = wP;
		lParam = lP;
	}
};

struct ViewportData {
	D3D12_VIEWPORT Viewport;
	D3D12_RECT ScissorRect;
	ui64 LayerFlag = 0; // layers that contained.
	matrix ViewMatrix;
	matrix ProjectMatrix;
	vec4 Camera_Pos;

	__forceinline XMVECTOR project(const vec4& vec_in_gamespace) {
		return XMVector3Project(vec_in_gamespace, Viewport.TopLeftX, Viewport.TopLeftY, Viewport.Width, Viewport.Height, Viewport.MinDepth, Viewport.MaxDepth, ProjectMatrix, ViewMatrix, XMMatrixIdentity());
	}
	__forceinline XMVECTOR unproject(const vec4& vec_in_screenspace) {
		return XMVector3Unproject(vec_in_screenspace, Viewport.TopLeftX, Viewport.TopLeftY, Viewport.Width, Viewport.Height, Viewport.MinDepth, Viewport.MaxDepth, ProjectMatrix, ViewMatrix, XMMatrixIdentity());
	}

	__forceinline void project_vecArr(vec4* invecArr_in_gamespace, vec4* outvecArr_in_screenspace, int count) {
		XMVector3ProjectStream((XMFLOAT3*)invecArr_in_gamespace, 16, (XMFLOAT3*)outvecArr_in_screenspace, 16, count, Viewport.TopLeftX, Viewport.TopLeftY, Viewport.Width, Viewport.Height, Viewport.MinDepth, Viewport.MaxDepth, ProjectMatrix, ViewMatrix, XMMatrixIdentity());
	}
	__forceinline XMVECTOR unproject_vecArr(vec4* outvecArr_in_screenspace, vec4* invecArr_in_gamespace, int count) {
		XMVector3UnprojectStream((XMFLOAT3*)invecArr_in_gamespace, 16, (XMFLOAT3*)outvecArr_in_screenspace, 16, count, Viewport.TopLeftX, Viewport.TopLeftY, Viewport.Width, Viewport.Height, Viewport.MinDepth, Viewport.MaxDepth, ProjectMatrix, ViewMatrix, XMMatrixIdentity());
	}
};

#define FRAME_BUFFER_WIDTH 1280
#define FRAME_BUFFER_HEIGHT 720

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

	D3D12_GPU_VIRTUAL_ADDRESS pGPU;
	D3D12_CPU_DESCRIPTOR_HANDLE hCpu;
	D3D12_GPU_DESCRIPTOR_HANDLE hGpu;

	void AddResourceBarrierTransitoinToCommand(ID3D12GraphicsCommandList* cmd, D3D12_RESOURCE_STATES afterState);
	D3D12_RESOURCE_BARRIER GetResourceBarrierTransition(D3D12_RESOURCE_STATES afterState);

	BOOL CreateTexture_fromImageBuffer(UINT Width, UINT Height, const BYTE* pInitImage);
	void CreateTexture_fromFile(const char* filename);

	void UpdateTextureForWrite(ID3D12Resource* pSrcTexResource);

	void Release();
};

struct DescriptorAllotter {
	BitAllotter AllocFlagContainer;
	ID3D12DescriptorHeap* pDescHeap;
	ui32 DescriptorSize;
	ui32 extraData;

	void Init(D3D12_DESCRIPTOR_HEAP_TYPE heapType, D3D12_DESCRIPTOR_HEAP_FLAGS Flags, int Capacity);
	int Alloc();
	void Free(int index);

	__forceinline D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(int index);
	__forceinline D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(int index);

	void Release();
};

struct ShaderVisibleDescriptorPool
{
	UINT	m_AllocatedDescriptorCount = 0;
	UINT	m_MaxDescriptorCount = 0;
	UINT	m_srvDescriptorSize = 0;
	ID3D12DescriptorHeap* m_pDescritorHeap = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE	m_cpuDescriptorHandle = {};
	D3D12_GPU_DESCRIPTOR_HANDLE	m_gpuDescriptorHandle = {};

	void	Release();
	BOOL	Initialize(UINT MaxDescriptorCount);
	BOOL	AllocDescriptorTable(D3D12_CPU_DESCRIPTOR_HANDLE* pOutCPUDescriptor, D3D12_GPU_DESCRIPTOR_HANDLE* pOutGPUDescriptor, UINT DescriptorCount);
	void	Reset();
};

struct TestStruct {
	bool UseBundle;

	void SetTestValue();
};

struct TextTextureKey {
	wchar_t charactor;
	unsigned int fontSize;

	TextTextureKey(wchar_t _charactor, unsigned int _fontSize) {
		charactor = _charactor;
		fontSize = _fontSize;
	}
};

template<>
class hash<TextTextureKey> {
public:
	size_t operator()(const TextTextureKey& s) const {
		size_t d0 = s.charactor;
		d0 = _pdep_u64(d0, 0x5555555555555555);
		size_t d1 = s.fontSize;
		d1 = _pdep_u64(d1, 0xAAAAAAAAAAAAAAAA);
		return d0 | d1;
	}
};

template<>
class hash<wchar_t> {
public:
	size_t operator()(const wchar_t& s) const {
		return s;
	}
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

	ID3D12CommandAllocator* pBundleAllocator;
	vector<ID3D12GraphicsCommandList*> pBundles;
	void InitBundleAllocator();
	int CreateNewBundle();
	/* bundle command limits. : 
		1. if want to Set Root Variables, it must contain SetGraphicsRootSignature Command front.
		2. bundle must set initial pipeline state. with CreateCommandList or SetPipelineState commands.
		3. bundle cannot execute resource barrier transition commands.
	*/

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
	FontData font_data[FontCount];
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

	// Returns required size of a buffer to be used for data upload
	// Origin Source is in D3dx12.h. we bring it. only this source.
	inline UINT64 GetRequiredIntermediateSize(
		_In_ ID3D12Resource* pDestinationResource,
		_In_range_(0, D3D12_REQ_SUBRESOURCES) UINT FirstSubresource,
		_In_range_(0, D3D12_REQ_SUBRESOURCES - FirstSubresource) UINT NumSubresources) noexcept;

	void ReportLiveObjects();

	//this function cannot be executing while command list update.
	void AddTextTexture(wchar_t key);

	void AddLineInTexture(float_v2 startpos, float_v2 endpos, BYTE* texture, int width, int height);
};
GlobalDevice gd;

class Shader {
public:
	ID3D12PipelineState* pPipelineState = nullptr;
	ID3D12PipelineState* pPipelineState_withShadow = nullptr;
	ID3D12PipelineState* pPipelineState_RenderShadowMap = nullptr;

	ID3D12RootSignature* pRootSignature = nullptr;
	ID3D12RootSignature* pRootSignature_withShadow = nullptr;
	Shader();
	virtual ~Shader();

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreateRootSignature_withShadow();
	virtual void CreatePipelineState();
	virtual void CreatePipelineState_withShadow();
	virtual void CreatePipelineState_RenderShadowMap();

	union ShadowRegisterEnum{
		enum ShadowRegisterEnum_memberenum{
			RenderNormal = 0,
			RenderWithShadow = 1,
			RenderShadowMap = 2,
		};
		int id;

		ShadowRegisterEnum(int i) : id{i} {}
		ShadowRegisterEnum(ShadowRegisterEnum_memberenum e) { id = (int)e; }
		operator int() { return id; }
	};
	

	void Add_RegisterShaderCommand(ID3D12GraphicsCommandList* commandList, Shader::ShadowRegisterEnum reg = Shader::ShadowRegisterEnum::RenderNormal);
	virtual void Release();

	static D3D12_SHADER_BYTECODE GetShaderByteCode(const WCHAR* pszFileName, LPCSTR pszShaderName, LPCSTR pszShaderProfile, ID3DBlob** ppd3dShaderBlob);

	void SetShadowMapCommand(GPUResource* shadowMap);
};

class OnlyColorShader : Shader {
public:
	OnlyColorShader();
	virtual ~OnlyColorShader();

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();
	virtual void Release();
};

class DiffuseTextureShader : Shader {
public:
	DiffuseTextureShader();
	virtual ~DiffuseTextureShader();

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreateRootSignature_withShadow();
	virtual void CreatePipelineState();
	virtual void CreatePipelineState_withShadow();
	virtual void CreatePipelineState_RenderShadowMap();
	virtual void Release();

	void SetTextureCommand(GPUResource* texture);
};

class ScreenCharactorShader : Shader {
public:
	ScreenCharactorShader();
	virtual ~ScreenCharactorShader();

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();
	virtual void Release();

	void SetTextureCommand(GPUResource* texture);
};

class ShadowMappingShader : public Shader{
public:
	GPUResource* CurrentShadowMap = nullptr;
	ID3D12DescriptorHeap* ShadowDescriptorHeap;
	ShaderVisibleDescriptorPool svdp;

	int dsvIncrement = 0;
	ShadowMappingShader();
	virtual ~ShadowMappingShader();

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();
	void Add_RegisterShaderCommand(ID3D12GraphicsCommandList* commandList);
	virtual void Release();

	void RegisterShadowMap(GPUResource* shadowMap);
	GPUResource CreateShadowMap(int width, int height, int DSVoffset);
};

struct SpotLight {
	matrix View;
	GPUResource ShadowMap;
	vec4 LightPos;
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
protected:
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
			: position{ p }, color{ col }, normal{ nor }
		{}
	};

	Mesh();
	virtual ~Mesh();

	virtual void ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering = true);
	virtual void Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum);
	BoundingOrientedBox GetOBB();
	void CreateGroundMesh(ID3D12GraphicsCommandList* pCommandList);
	void CreateWallMesh(float width, float height, float depth, vec4 color);

	virtual void Release();
};

class UVMesh : Mesh {
public:
	struct Vertex {
		XMFLOAT3 position;
		XMFLOAT4 color;
		XMFLOAT3 normal;
		XMFLOAT2 uv;

		Vertex() {}
		Vertex(XMFLOAT3 p, XMFLOAT4 col, XMFLOAT3 nor, XMFLOAT2 texcoord)
			: position{ p }, color{ col }, normal{ nor }, uv{texcoord}
		{
		}
	};

	virtual void ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering = true);
	virtual void Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum);

	virtual void Release();

	void CreateTextRectMesh();
};

struct OBB_vertexVector {
	vec4 vertex[2][2][2] = { {{}} };
};

struct BulletRay {
	static constexpr float remainTime = 1.0f;
	static Mesh* mesh;
	vec4 start;
	vec4 direction;
	float t;
	float distance;
	float extra[2];

	BulletRay();

	BulletRay(vec4 s, vec4 dir, float dist);
	
	matrix GetRayMatrix(float distance);

	void Update();

	void Render();
};

struct GameObject {
	matrix m_worldMatrix;
	Mesh* m_pMesh = nullptr;
	Shader* m_pShader = nullptr;
	GPUResource* m_pTexture = nullptr;

	vec4 LVelocity;
	vec4 tickLVelocity;
	vec4 tickAVelocity;
	vec4 LastQuerternion;

	GameObject();
	virtual ~GameObject();

	virtual void Update(float delatTime);
	virtual void Render();
	virtual void Event(WinEvent evt);
	virtual void Release();

	void SetMesh(Mesh* pMesh);
	void SetShader(Shader* pShader);
	void SetWorldMatrix(const XMMATRIX& worldMatrix);

	const XMMATRIX& GetWorldMatrix() const;

	virtual BoundingOrientedBox GetOBB();
	OBB_vertexVector GetOBBVertexs();

	void LookAt(vec4 look, vec4 up = { 0, 1, 0, 0 });

	static void CollisionMove(GameObject* obj1, GameObject* obj2);
	virtual void OnCollision(GameObject* other);
};

class Player : public GameObject {
	UCHAR* m_pKeyBuffer;
	vec4 velocity;
	vec4 accel;
public:
	bool isGround = false;
	int collideCount = 0;
	float JumpVelocity = 5;

	Mesh* Gun;
	Mesh* ShootPointMesh;
	matrix gunMatrix_thirdPersonView;
	matrix gunMatrix_firstPersonView;
	static constexpr float ShootDelay = 0.5f;
	float ShootFlow = 0;
	static constexpr float recoilVelocity = 20.0f;
	float recoilFlow = 0;
	static constexpr float recoilDelay = 0.3f;

	Mesh* HPBarMesh;
	float HP = 100;
	static constexpr float MaxHP = 100;
	matrix HPMatrix;

	Player(UCHAR* pKeyBuffer) : m_pKeyBuffer(pKeyBuffer) {}

	virtual void Update(float deltaTime) override;

	virtual void Render();
	void Render_AfterDepthClear();

	virtual void Event(WinEvent evt);

	virtual void OnCollision(GameObject* other) override;
	
	virtual void Release();

	virtual BoundingOrientedBox GetOBB();
};

struct DirectionLight
{
	XMFLOAT3 gLightDirection; // Light Direction
	float pad0;
	XMFLOAT3 gLightColor; // Light_Color
	float pad1;
};

struct PointLight
{
	XMFLOAT3 LightPos;
	float LightIntencity;
	XMFLOAT3 LightColor; // Light_Color
	float LightRange;
};

struct LightCB_DATA {
	DirectionLight dirlight;
	PointLight pointLights[8];
};

struct LightCB_DATA_withShadow {
	DirectionLight dirlight;
	PointLight pointLights[8];
	XMMATRIX LightProjection;
	XMMATRIX LightView;
	XMFLOAT3 LightPos;
};

class Game {
public:
	Shader* MyShader;
	OnlyColorShader* MyOnlyColorShader;
	DiffuseTextureShader* MyDiffuseTextureShader;
	ScreenCharactorShader* MyScreenCharactorShader;
	UVMesh* TextMesh;
	ShadowMappingShader* MyShadowMappingShader;
	SpotLight MySpotLight;

	std::vector<GameObject*> m_gameObjects; // GameObjets
	Player* player;
	GPUResource TestTexture;

	vecset<BulletRay> bulletRays;

	//inputs
	bool isMouseReturn;
	vec4 DeltaMousePos;
	vec4 LastMousePos;

	bool bFirstPersonVision = true;

	float DeltaTime;
	UCHAR pKeyBuffer[256];

	GPUResource LightCBResource;
	LightCB_DATA* LightCBData;

	GPUResource LightCB_withShadowResource;
	LightCB_DATA_withShadow* LightCBData_withShadow;
	void SetLight();

	Game(){}
	~Game() {}
	void Init();
	void Render();
	void Render_ShadowPass();
	void Update();
	void Event(WinEvent evt);

	// why I make Release function seperatly? : it is multi useable form.
	// 1. when I want show object release on code lines or if object is global, just call Release();
	// 2. when I want convinient of Destructor ~Class(), just put Release function on it.
	void Release();

	void FireRaycast(vec4 rayStart, vec4 rayDirection, float rayDistance);

	void RenderText(const wchar_t* wstr, int length, vec4 Rect, float fontsiz);
};
Game game;

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
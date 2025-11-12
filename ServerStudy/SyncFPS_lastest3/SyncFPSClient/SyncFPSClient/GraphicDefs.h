#pragma once
#include "stdafx.h"

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

enum RenderingMod {
	ForwardRendering = 0,
	DeferedRendering = 1
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
};

struct ShaderVisibleDescriptorPool
{
	UINT	m_AllocatedDescriptorCount = 0;
	UINT	m_MaxDescriptorCount = 0;
	UINT	m_srvDescriptorSize = 0;
	ID3D12DescriptorHeap* m_pDescritorHeap = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE	m_cpuDescriptorHandle = {};
	D3D12_GPU_DESCRIPTOR_HANDLE	m_gpuDescriptorHandle = {};

	void	Cleanup();
	BOOL	Initialize(UINT MaxDescriptorCount);
	BOOL	AllocDescriptorTable(D3D12_CPU_DESCRIPTOR_HANDLE* pOutCPUDescriptor, D3D12_GPU_DESCRIPTOR_HANDLE* pOutGPUDescriptor, UINT DescriptorCount);
	void	Reset();
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

	void UpdateTextureForWrite(ID3D12Resource* pDestTexResource, ID3D12Resource* pSrcTexResource);
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

struct PointLight
{
	XMFLOAT3 LightPos;
	float LightIntencity;
	XMFLOAT3 LightColor; // Light_Color
	float LightRange;
};

struct LightCB_DATA {
	PointLight pointLights[8];
};
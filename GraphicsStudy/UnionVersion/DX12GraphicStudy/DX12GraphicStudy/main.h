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
#include <string>
#include <thread>
#include <random>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include <dxcapi.h>
#include <d3d12sdklayers.h>
#include <wrl.h>

#include <sstream>
#include <iomanip>
#include "d3dx12.h"
#include "D3D_Util/DXSampleHelper.h"
#include "D3D_Util/ShaderUtil.h"
#include "D3D_Util/DirectXRaytracingHelper.h"

#include "vecset.h"
#include "SpaceMath.h"
#include "RangeArr.h"
#include "Utill_ImageFormating.h"
#include "DDSTextureLoader12.h"

//#define PIX_DEBUGING

#ifdef PIX_DEBUGING
#include "C:\\Users\\nohc2\\OneDrive\\Desktop\\WorkRoom\\Dev\\CollegeProjects\\Graphics\\PixEvents-main\\include\\pix3.h" // PIX 관련 헤더 (예시)
#endif

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

int dbgc[128] = {};
void dbgbreak(bool condition) {
	if (condition) __debugbreak();
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
#define dbglog1a(format, A) \
	{\
		char str[512] = {}; \
		char format0[128] = "%s line %d : "; \
		strcpy_s(format0+13, 128-13, format);\
		sprintf_s(str, 512, format0, __FUNCTION__, __LINE__, A); \
		OutputDebugStringA(str);\
	}
#define dbglog2a(format, A, B) \
	{\
		char str[512] = {}; \
		char format0[128] = "%s line %d : "; \
		strcpy_s(format0+13, 128-13, format);\
		sprintf_s(str, 512, format0, __FUNCTION__, __LINE__, A, B); \
		OutputDebugStringA(str);\
	}
#define dbglog3a(format, A, B, C) \
	{\
		char str[512] = {}; \
		char format0[128] = "%s line %d : "; \
		strcpy_s(format0+13, 128-13, format);\
		sprintf_s(str, 512, format0, __FUNCTION__, __LINE__, A, B, C); \
		OutputDebugStringA(str);\
	}
#define dbglog4a(format, A, B, C, D) \
	{\
		char str[512] = {}; \
		char format0[128] = "%s line %d : "; \
		strcpy_s(format0+13, 128-13, format);\
		sprintf_s(str, 512, format, __FUNCTION__, __LINE__, A, B, C, D); \
		OutputDebugStringA(str);\
	}
#pragma endregion

//outer code start : microsoft copilot
bool ChangeToFileDirectory(const std::string& filepath) {
	// extract directory path
	char directory[MAX_PATH];
	strcpy_s(directory, filepath.c_str());
	char* lastSlash = strrchr(directory, '/');
	if (!lastSlash) {
		dbglog1(L"유효한 경로가 아닙니다.\n", 0);
		return false;
	}
	*lastSlash = '\0';

	// change current directory
	if (SetCurrentDirectoryA(directory)) {
		return true;
	}
	else {
		dbglog1(L"디렉토리 변경 실패. \n", 0);
		return false;
	}
}
//outer code end

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

struct DescHandle {
	D3D12_CPU_DESCRIPTOR_HANDLE hcpu;
	D3D12_GPU_DESCRIPTOR_HANDLE hgpu;

	DescHandle(D3D12_CPU_DESCRIPTOR_HANDLE _hcpu, D3D12_GPU_DESCRIPTOR_HANDLE _hgpu) {
		hcpu = _hcpu;
		hgpu = _hgpu;
	}
	DescHandle() :
		hcpu{ 0 }, hgpu{ 0 }
	{
	}

	__forceinline void operator+=(unsigned long long inc) {
		hcpu.ptr += inc;
		hgpu.ptr += inc;
	}

	template<D3D12_DESCRIPTOR_HEAP_TYPE type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV>
	__forceinline DescHandle operator[](UINT index);
};

struct DescIndex {
	bool isShaderVisible = false;
	char type = 0; // 'n' - UAV, SRV, CBV / 'r' - RTV / 'd' - DSV
	ui32 index;
	DescIndex() {

	}

	DescIndex(bool isSV, ui32 i, char t = 'n') : isShaderVisible{ isSV }, index{ i }, type{ t } {

	}

	void Set(bool isSV, ui32 i, char t = 'n') {
		isShaderVisible = isSV;
		index = i;
		type = t;
	}
	__forceinline DescHandle GetCreationDescHandle() const;
	__forceinline DescHandle GetRenderDescHandle() const;
	__declspec(property(get = GetCreationDescHandle)) const DescHandle hCreation;
	__declspec(property(get = GetRenderDescHandle)) const DescHandle hRender;
};

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

float stackff = 0;
/*
* 설명 : 뷰포트 정보
*/
struct ViewportData {
	// AABB
	D3D12_VIEWPORT Viewport;
	// 시저렉트
	D3D12_RECT ScissorRect;

	// improve : <원래는 이 변수는 만들때 어떤 Layer의 게임오브젝트만 출력하기 위해 만들어놓은 것이었음.>
	// <대다수의 게임엔진에 이런 게념이 있음. 특히 UI가 3D거나 그러면 특히나.>
	// <이를 어떻게 할지를 정해야 할듯.>
	ui64 LayerFlag = 0;

	// 뷰 행렬
	matrix ViewMatrix;
	// 투영 행렬
	matrix ProjectMatrix;
	// 카메라의 위치
	vec4 Camera_Pos;

	// 원근 프러스텀 컬링을 위한 BoundingFrustum
	BoundingFrustum	m_xmFrustumWorld = BoundingFrustum();
	// 직교 프러스텀 컬링을 위한 BoundingOrientedBox
	BoundingOrientedBox OrthoFrustum;
	bool (ViewportData::*FrustumIntersectFunc)(const BoundingOrientedBox&) = nullptr;

	bool FrustumIntersectPerspective(const BoundingOrientedBox& obb) {
		return m_xmFrustumWorld.Intersects(obb);
	}

	bool FrustumIntersectOrtho(const BoundingOrientedBox& obb) {
		return OrthoFrustum.Intersects(obb);
	}

	/*
	* //sus : <제대로 실행될지 의심이 되는 것. 유닛테스트가 필요함.>
	* 설명 : vec_in_gamespace 점을 화면상의 공간으로 투영하여 내보낸다.
	* 피킹에 쓰려고 미리 만들어놨다.
	* 매개변수 :
	* const vec4& vec_in_gamespace : 월드 공간상의 점
	* 반환 :
	* vec_in_gamespace을 화면 공간상으로 투영한 지점
	*/
	__forceinline XMVECTOR project(const vec4& vec_in_gamespace) {
		return XMVector3Project(vec_in_gamespace, Viewport.TopLeftX, Viewport.TopLeftY, Viewport.Width, Viewport.Height, Viewport.MinDepth, Viewport.MaxDepth, ProjectMatrix, ViewMatrix, XMMatrixIdentity());
	}

	/*
	* //sus : <제대로 실행될지 의심이 되는 것. 유닛테스트가 필요함.>
	* 설명 : vec_in_screenspace 점을 화면상의 공간에서 월드 공간으로 변환해 내보낸다.
	* 피킹에 쓰려고 미리 만들어놨다.
	* 매개변수 :
	* const vec4& vec_in_screenspace : 화면 공간상의 점
	* 반환 :
	* vec_in_screenspace을 월드공간상으로 투영한 지점
	*/
	__forceinline XMVECTOR unproject(const vec4& vec_in_screenspace) {
		return XMVector3Unproject(vec_in_screenspace, Viewport.TopLeftX, Viewport.TopLeftY, Viewport.Width, Viewport.Height, Viewport.MinDepth, Viewport.MaxDepth, ProjectMatrix, ViewMatrix, XMMatrixIdentity());
	}

	/*
	* //sus : <제대로 실행될지 의심이 되는 것. 유닛테스트가 필요함.>
	* 설명 : invecArr_in_gamespace 점 배열들을 화면상의 공간으로 투영하여
	* outvecArr_in_screenspace 배열으로 내보낸다.
	* 피킹에 쓰려고 미리 만들어놨다.
	* 매개변수 :
	* vec4* invecArr_in_gamespace : 입력이 되는 월드공간의 점 배열
	* vec4* outvecArr_in_screenspace : 출력이 되는 화면공간상의 점 배열
	* int count : 배열의 크기
	*/
	__forceinline void project_vecArr(vec4* invecArr_in_gamespace, vec4* outvecArr_in_screenspace, int count) {
		XMVector3ProjectStream((XMFLOAT3*)invecArr_in_gamespace, 16, (XMFLOAT3*)outvecArr_in_screenspace, 16, count, Viewport.TopLeftX, Viewport.TopLeftY, Viewport.Width, Viewport.Height, Viewport.MinDepth, Viewport.MaxDepth, ProjectMatrix, ViewMatrix, XMMatrixIdentity());
	}

	/*
	* //sus : <제대로 실행될지 의심이 되는 것. 유닛테스트가 필요함.>
	* 설명 : invecArr_in_screenspace 화면상의 점 배열들을 월드 공간으로 변환하여
	* outvecArr_in_gamespace 배열으로 내보낸다.
	* 피킹에 쓰려고 미리 만들어놨다.
	* 매개변수 :
	* vec4* invecArr_in_screenspace : 입력이 되는 화면공간상의 점 배열
	* vec4* outvecArr_in_gamespace : 출력이 되는 월드공간상의 점 배열
	* int count : 배열의 크기
	*/
	__forceinline void unproject_vecArr(vec4* invecArr_in_screenspace, vec4* outvecArr_in_gamespace, int count) {
		XMVector3UnprojectStream((XMFLOAT3*)outvecArr_in_gamespace, 16, (XMFLOAT3*)invecArr_in_screenspace, 16, count, Viewport.TopLeftX, Viewport.TopLeftY, Viewport.Width, Viewport.Height, Viewport.MinDepth, Viewport.MaxDepth, ProjectMatrix, ViewMatrix, XMMatrixIdentity());
	}

	/*
	* 설명 : 현재 ViewPort 데이터에 맞게 원근 뷰 프러스텀 m_xmFrustumWorld을 업데이트한다.
	*/
	void UpdateFrustum() {
		/*m_xmFrustumWorld.Origin = { 0, 0, 0 };
		m_xmFrustumWorld.Near = 0.1f;
		m_xmFrustumWorld.Far = 1000.0f;
		m_xmFrustumWorld.Orientation = vec4::getQ(vec4(0, stackff, 0, 0));
		m_xmFrustumWorld.LeftSlope = -1;
		m_xmFrustumWorld.RightSlope = 1;
		m_xmFrustumWorld.BottomSlope = -1;
		m_xmFrustumWorld.TopSlope = 1;
		stackff += 0.01f;*/

		BoundingFrustum::CreateFromMatrix(m_xmFrustumWorld, ProjectMatrix);
		m_xmFrustumWorld.Transform(m_xmFrustumWorld, ViewMatrix.RTInverse);
		FrustumIntersectFunc = &ViewportData::FrustumIntersectPerspective;
	}

	/*
	* 설명 : 현재 ViewPort 데이터에 맞게 직교 뷰 프러스텀 OrthoFrustum을 업데이트한다.
	*/
	void UpdateOrthoFrustum(float nearF, float farF) {
		matrix v = ViewMatrix;
		v.transpose();
		vec4 Center = (Camera_Pos + v.look * nearF) + (Camera_Pos + v.look * farF); // should config it is near, far
		Center *= 0.5f;
		OrthoFrustum.Center = Center.f3;
		OrthoFrustum.Extents = { 0.5f * Viewport.Width, 0.5f * Viewport.Height, 0.5f * (farF - nearF) };
		v = ViewMatrix;
		OrthoFrustum.Orientation = vec4(XMQuaternionRotationMatrix(v.RTInverse)).f4;
		FrustumIntersectFunc = &ViewportData::FrustumIntersectOrtho;
	}
};

struct OBB_vertexVector {
	vec4 vertex[2][2][2] = { {{}} };
};

#define FRAME_BUFFER_WIDTH 1280
#define FRAME_BUFFER_HEIGHT 720

HINSTANCE g_hInst;
HWND hWnd;
LPCTSTR lpszClass = L"Pulse City Client 001";
LPCTSTR lpszWindowName = L"Pulse City Client 001";
int resolutionLevel = 0;

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
	DescIndex descindex;

	void AddResourceBarrierTransitoinToCommand(ID3D12GraphicsCommandList* cmd, D3D12_RESOURCE_STATES afterState);
	D3D12_RESOURCE_BARRIER GetResourceBarrierTransition(D3D12_RESOURCE_STATES afterState);

	BOOL CreateTexture_fromImageBuffer(UINT Width, UINT Height, const BYTE* pInitImage, DXGI_FORMAT Format, bool ImmortalShaderVisible = false);
	void CreateTexture_fromFile(const wchar_t* filename, DXGI_FORMAT Format, int mipmapLevel, bool ImmortalShaderVisible = false);

	void UpdateTextureForWrite(ID3D12Resource* pSrcTexResource);

	void Release();

	ID3D12Resource* CreateTextureResourceFromDDSFile(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, wchar_t* pszFileName, ID3D12Resource** ppd3dUploadBuffer, D3D12_RESOURCE_STATES d3dResourceStates, bool bIsCubeMap = false)
	{
		ID3D12Resource* pd3dTexture = NULL;
		std::unique_ptr<uint8_t[]> ddsData;
		std::vector<D3D12_SUBRESOURCE_DATA> vSubresources;
		DDS_ALPHA_MODE ddsAlphaMode = DDS_ALPHA_MODE_UNKNOWN;

		HRESULT hResult = DirectX::LoadDDSTextureFromFileEx(pd3dDevice, pszFileName, 0, D3D12_RESOURCE_FLAG_NONE, DDS_LOADER_DEFAULT, &pd3dTexture, ddsData, vSubresources, &ddsAlphaMode, &bIsCubeMap);

		D3D12_HEAP_PROPERTIES d3dHeapPropertiesDesc;
		::ZeroMemory(&d3dHeapPropertiesDesc, sizeof(D3D12_HEAP_PROPERTIES));
		d3dHeapPropertiesDesc.Type = D3D12_HEAP_TYPE_UPLOAD;
		d3dHeapPropertiesDesc.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		d3dHeapPropertiesDesc.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		d3dHeapPropertiesDesc.CreationNodeMask = 1;
		d3dHeapPropertiesDesc.VisibleNodeMask = 1;

		//	D3D12_RESOURCE_DESC d3dResourceDesc = pd3dTexture->GetDesc();
		//	UINT nSubResources = d3dResourceDesc.DepthOrArraySize * d3dResourceDesc.MipLevels;
		UINT nSubResources = (UINT)vSubresources.size();
		//	UINT64 nBytes = 0;
		//	pd3dDevice->GetCopyableFootprints(&d3dResourceDesc, 0, nSubResources, 0, NULL, NULL, NULL, &nBytes);
		UINT64 nBytes = GetRequiredIntermediateSize(pd3dTexture, 0, nSubResources);

		D3D12_RESOURCE_DESC d3dResourceDesc;
		::ZeroMemory(&d3dResourceDesc, sizeof(D3D12_RESOURCE_DESC));
		d3dResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; //Upload Heap에는 텍스쳐를 생성할 수 없음
		d3dResourceDesc.Alignment = 0;
		d3dResourceDesc.Width = nBytes;
		d3dResourceDesc.Height = 1;
		d3dResourceDesc.DepthOrArraySize = 1;
		d3dResourceDesc.MipLevels = 1;
		d3dResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		d3dResourceDesc.SampleDesc.Count = 1;
		d3dResourceDesc.SampleDesc.Quality = 0;
		d3dResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		d3dResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		pd3dDevice->CreateCommittedResource(&d3dHeapPropertiesDesc, D3D12_HEAP_FLAG_NONE, &d3dResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, __uuidof(ID3D12Resource), (void**)ppd3dUploadBuffer);

		//UINT nSubResources = (UINT)vSubresources.size();
		//D3D12_SUBRESOURCE_DATA *pd3dSubResourceData = new D3D12_SUBRESOURCE_DATA[nSubResources];
		//for (UINT i = 0; i < nSubResources; i++) pd3dSubResourceData[i] = vSubresources.at(i);

		//	std::vector<D3D12_SUBRESOURCE_DATA>::pointer ptr = &vSubresources[0];
		::UpdateSubresources(pd3dCommandList, pd3dTexture, *ppd3dUploadBuffer, 0, 0, nSubResources, &vSubresources[0]);

		D3D12_RESOURCE_BARRIER d3dResourceBarrier;
		::ZeroMemory(&d3dResourceBarrier, sizeof(D3D12_RESOURCE_BARRIER));
		d3dResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		d3dResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		d3dResourceBarrier.Transition.pResource = pd3dTexture;
		d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		d3dResourceBarrier.Transition.StateAfter = d3dResourceStates;
		d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		pd3dCommandList->ResourceBarrier(1, &d3dResourceBarrier);

		//	delete[] pd3dSubResourceData;

		return(pd3dTexture);
	}

	void CreateBuffer_withUpload(int sizeOfByte, GPUResource* outUploadResource);
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
	UINT	m_srvImmortalDescriptorSize = 0;
	ID3D12DescriptorHeap* m_pDescritorHeap = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE	m_cpuDescriptorHandle = {};
	D3D12_GPU_DESCRIPTOR_HANDLE	m_gpuDescriptorHandle = {};

	void	Release();
	BOOL	Initialize(UINT MaxDescriptorCount);
	BOOL	AllocDescriptorTable(D3D12_CPU_DESCRIPTOR_HANDLE* pOutCPUDescriptor, D3D12_GPU_DESCRIPTOR_HANDLE* pOutGPUDescriptor, UINT DescriptorCount);
	BOOL	ImmortalAllocDescriptorTable(D3D12_CPU_DESCRIPTOR_HANDLE* pOutCPUDescriptor, D3D12_GPU_DESCRIPTOR_HANDLE* pOutGPUDescriptor, UINT DescriptorCount);
	
	BOOL	AllocDescriptorTable(DescHandle* pOutDescriptor, UINT DescriptorCount);
	BOOL	ImmortalAllocDescriptorTable(DescHandle* pOutDescriptor, UINT DescriptorCount);

	bool	IncludeHandle(D3D12_CPU_DESCRIPTOR_HANDLE hcpu);
	void	Reset();
};

// [MainRenderTarget SRV]*SwapChainBufferCount [BlurTexture SRV] [SubRenderTarget SRV] [InstancingSRV] * cap, [TextureSRV] * cap, [MaterialCBV] * cap // immortal layer.
// //dynamic layer
struct SVDescPool2
{
	UINT AllocatedDescriptorCount = 0;
	UINT MaxDescCount = 0;
	UINT DynamicSize = 0;
	ID3D12DescriptorHeap* pNSVDescHeapForCreation = nullptr;
	DescHandle NSVDescHeapCreationHandle;

	ID3D12DescriptorHeap* pSVDescHeapForRender = nullptr;
	DescHandle SVDescHeapRenderHandle;

	ID3D12DescriptorHeap* NSVDescHeapForCopy = nullptr;
	DescHandle NSVDescHeapCopyHandle;

	ui32 InitDescArrSiz;
	union {
		ui32 InitDescArrCap;
		ui32 TextureSRVStart;
	};
	ui32 TextureSRVSiz;
	ui32 TextureSRVCap;
	ui32 MaterialCBVSiz;
	ui32 MaterialCBVCap;
	ui32 InstancingSRVSiz;
	ui32 InstancingSRVCap;

	bool isImmortalChange = false;
	__forceinline const ui32 getImmortalSiz() const {
		return InitDescArrCap + TextureSRVCap + MaterialCBVCap + InstancingSRVCap;
	}
	__declspec(property(get = getImmortalSiz)) const ui32 ImmortalSize;

	void Release();
	BOOL Initialize(UINT MaxDescriptorCount);
	BOOL DynamicAlloc(DescHandle* pHandle, UINT DescriptorCount);
	
	BOOL ImmortalAlloc(DescIndex* pindex, UINT DescriptorCount);
	BOOL ImmortalAlloc_TextureSRV(DescIndex* pindex, UINT DescriptorCount);
	BOOL ImmortalAlloc_MaterialCBV(DescIndex* pindex, UINT DescriptorCount);
	BOOL ImmortalAlloc_InstancingSRV(DescIndex* pindex, UINT DescriptorCount);

	void ExpendDescStructure(ui32 newInitDescArrCap, ui32 newTextureSRVCap, ui32 newMaterialCBVCap, ui32 newInstancingSRVCap);
	void BakeImmortalDesc();

	bool IncludeHandle(D3D12_CPU_DESCRIPTOR_HANDLE hcpu);
	void DynamicReset();
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

struct GlobalDevice;

struct RasterDevice {
	GlobalDevice* origin;
};

//Code From : megayuchi - DXR Sample Start
typedef DXC_API_IMPORT HRESULT(__stdcall* DxcCreateInstanceT)(_In_ REFCLSID rclsid, _In_ REFIID riid, _Out_ LPVOID* ppv);

#define		MAX_SHADER_NAME_BUFFER_LEN		256
#define		MAX_SHADER_NAME_LEN				(MAX_SHADER_NAME_BUFFER_LEN-1)
#define		MAX_SHADER_NUM					2048
#define		MAX_CODE_SIZE					(1024*1024)

/*
* ShaderHandle Only Created by malloc.
* size is always precalculate by ShaderBlobs;
*
* 4byte dwFlags
* 4byte dwCodeSize
* 4byte dwShaderNameLen
* 2 * 256 byte wchShaderName
* <ShaderByteCodeSize> byte pCodeBuffer
*
* sizeof ShaderHandle is not sizeof(SHADER_HANDLE).
* that is <sizeof(SHADER_HANDLE) - sizeof(DWORD) + dwCodeSize>;
*/
struct SHADER_HANDLE
{
	DWORD dwFlags;
	DWORD dwCodeSize;
	DWORD dwShaderNameLen;
	WCHAR wchShaderName[MAX_SHADER_NAME_BUFFER_LEN];
	DWORD pCodeBuffer[1];
};

struct Viewport
{
	float left;
	float top;
	float right;
	float bottom;
};

struct RayGenConstantBuffer
{
	Viewport viewport;
	Viewport stencil;
};
//Code From : megayuchi - DXR Sample End

struct RayTracingDevice {
	struct CameraConstantBuffer
	{
		XMMATRIX projectionToWorld;
		XMVECTOR cameraPosition;
		XMFLOAT3 DirLight_invDirection;
		float DirLight_intencity;
		XMFLOAT3 DirLight_color;
		float padding;
	};

	GlobalDevice* origin;
	ID3D12Device5* dxrDevice;
	ID3D12GraphicsCommandList4* dxrCommandList;

	HMODULE hDXC;
	IDxcLibrary* pDXCLib;
	IDxcCompiler* pDXCCompiler;
	IDxcIncludeHandler* pDSCIncHandle;

	//1MB Scratch GPU Mem
	inline static UINT64 ASBuild_ScratchResource_Maxsiz = 10485760 * 2;
	ID3D12Resource* ASBuild_ScratchResource = nullptr;
	UINT64 UsingScratchSize = 0; // 256의 배수여야함.

	ID3D12Resource* RayTracingOutput = nullptr;
	DescIndex RTO_UAV_index;

	ID3D12Resource* CameraCB = nullptr;
	CameraConstantBuffer* MappedCB = nullptr;

	vec4 m_eye;
	vec4 m_up;
	vec4 m_at;

	void Init(void* origin_gd);

	//Raytracing이 지원되는 버전 부터는 디바이스 제거를 처리하는 애플리케이션이 스스로를 그렇게 할 수 있다고 선언이 필요한데, 그 작업을 한다.
	void CheckDeviceSelfRemovable();

	//Code From DirectX Raytracing HelloWorld Sample
	// Returns bool whether the device supports DirectX Raytracing tier.
	inline bool IsDirectXRaytracingSupported(IDXGIAdapter1* adapter);

	void SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ID3D12RootSignature** rootSig);

	bool InitDXC();

	SHADER_HANDLE* CreateShaderDXC(const wchar_t* shaderPath, const WCHAR* wchShaderFileName, const WCHAR* wchEntryPoint, const WCHAR* wchShaderModel, DWORD dwFlags);

	void CreateSubRenderTarget();

	void CreateCameraCB();

	void SetRaytracingCamera(vec4 CameraPos, vec4 look, vec4 up = vec4(0, 1, 0, 0));
};

#define SizeOfInUint32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)

//32 byte alignment require
struct LocalRootSigData {
	unsigned int VBOffset;
	unsigned int IBOffset;
	char padding[24];

	LocalRootSigData() {}
	LocalRootSigData(unsigned int VBOff, unsigned int IBOff) :
		VBOffset{ VBOff }, IBOffset{ IBOff }
	{
	}
	LocalRootSigData(const LocalRootSigData& ref) {
		VBOffset = ref.VBOffset;
		IBOffset = ref.IBOffset;
	}

	bool operator==(const LocalRootSigData& ref) const {
		return ref.IBOffset == IBOffset && ref.VBOffset == VBOffset;
	}
};

// [float3 position] [float3 normal] [float2 uv] [float3 tangent] (기본적으로 BumpMesh임)
struct RayTracingMesh {
	//--------------- 모든 메쉬들이 공유하는 변수----------------//
	// 모든 메쉬들이 같이 공유하는 VB, IB (UploadBuffer)
	// 업로드 버퍼이므로 GPU 접근이 상대적으로 느리다. 성능이 좋지 않다면, 변하지 않는 Mesh들을 따로 
	// DefaultHeap으로 구성할 필요가 있다.
	/*
	* commandList->CopyBufferRegion(
	defaultBuffer.Get(),   // 대상(Default Heap)
	destOffset,            // 대상 오프셋
	uploadBuffer.Get(),    // 원본(Upload Heap)
	srcOffset,             // 원본 오프셋
	sizeInBytes            // 복사할 크기
	);
	*/

	//모든 메쉬들이 같이 공유하는 VB, IB (UploadBuffer)
	inline static ID3D12Resource* vertexBuffer = nullptr; // SRV
	inline static ID3D12Resource* indexBuffer = nullptr; // SRV
	inline static ID3D12Resource* UAV_vertexBuffer = nullptr; // UAV, SRV
	
	// VB, IB의 최대크기
	static constexpr int VertexBufferCapacity = 52428800; // 50MB
	static constexpr int IndexBufferCapacity = 52428800; // 10MB
	static constexpr int UAV_VertexBufferCapacity = 52428800; // 50MB

	// 업로드할 메쉬의 VB, IB 가 들어갈 데이터
	inline static ID3D12Resource* Upload_vertexBuffer = nullptr;
	inline static ID3D12Resource* Upload_indexBuffer = nullptr;
	inline static ID3D12Resource* UAV_Upload_vertexBuffer = nullptr;
	
	// 업로드 VB, IB의 최대크기
	static constexpr int Upload_VertexBufferCapacity = 20971520; // 20MB
	static constexpr int Upload_IndexBufferCapacity = 20971520; // 10MB
	static constexpr int UAV_Upload_VertexBufferCapacity = 20971520; // 20MB
	
	// 업로드 VB, IB가 매핑된 CPURAM 데이터의 주소
	inline static char* pVBMappedStart = nullptr;
	inline static char* pIBMappedStart = nullptr;
	inline static char* pUAV_VBMappedStart = nullptr;

	// VB, IB의 현재 크기
	inline static int VertexBufferByteSize = 0;
	inline static int IndexBufferByteSize = 0;
	inline static int UAV_VertexBufferByteSize = 0;

	// VB, IB를 가리키는 SRV Desc Handle
	inline static DescIndex VBIB_DescIndex;
	// UAV_VB, IB를 가리키는 SRV Desc Handle
	inline static DescIndex UAV_VBIB_DescIndex;

	//--------------- 메쉬들이 각각따로 소유한 변수---------------//
	// 메쉬의 버택스 데이터, 엔덱스 데이터가 매핑된 CPURAM 데이터의 주소
	char* pVBMapped = nullptr;
	char* pIBMapped = nullptr;
	char* pUAV_VBMapped = nullptr;

	// Ray가 발사되고 BLAS를 통과해 물체와 만났을때, Mesh의 VB, IB를 접근할 수 있도록
	// LocalRootSignature에 해당 메쉬의 VB, IB가 시작되는 바이트오프셋을 넣어 주어야 한다.
	UINT64 VBStartOffset;
	UINT64* IBStartOffset;
	UINT64 UAV_VBStartOffset;

	int subMeshCount = 1;

	// 메쉬의 BLAS
	ID3D12Resource* BLAS;
	D3D12_RAYTRACING_GEOMETRY_DESC* GeometryDescs;
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS BLAS_Input;
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};

	// TLAS에 인스턴스를 추가할때 이 값을 사용해서 렌더링 인스턴스를 삽입.
	D3D12_RAYTRACING_INSTANCE_DESC MeshDefaultInstanceData = {};

	struct Vertex {
		XMFLOAT3 position;
		float u;
		float v;
		XMFLOAT3 normal;
		XMFLOAT3 tangent;
		float padding;

		operator RayTracingMesh::Vertex() {
			return RayTracingMesh::Vertex(position, normal, XMFLOAT2(u, v), tangent);
		}

		Vertex() {}
		Vertex(XMFLOAT3 p, XMFLOAT3 nor, XMFLOAT2 _uv, XMFLOAT3 tan)
			: position{ p }, normal{ nor }, u{ _uv.x }, v{ _uv.y }, tangent{ tan }
		{
		}

		bool HaveToSetTangent() {
			bool b = ((tangent.x == 0 && tangent.y == 0) && tangent.z == 0);
			b |= ((isnan(tangent.x) || isnan(tangent.y)) || isnan(tangent.z));
			return b;
		}

		static void CreateTBN(Vertex& P0, Vertex& P1, Vertex& P2) {
			vec4 N1, N2, M1, M2;
			//XMFLOAT3 N0, N1, M0, M1;
			N1 = vec4(P1.u, P1.v, 0, 0) - vec4(P0.u, P0.v, 0, 0);
			N2 = vec4(P2.u, P2.v, 0, 0) - vec4(P0.u, P0.v, 0, 0);
			M1 = vec4(P1.position) - vec4(P0.position);
			M2 = vec4(P2.position) - vec4(P0.position);

			float f = 1.0f / (N1.x * N2.y - N2.x * N1.y);
			vec4 tan = vec4(P0.tangent);
			tan = f * (M1 * N2.y - M2 * N1.y);
			vec4 v = XMVector3Normalize(tan);
			P0.tangent = v.f3;

			if (P0.HaveToSetTangent()) {
				// why tangent is nan???
				XMVECTOR NorV = XMVectorSet(P0.normal.x, P0.normal.y, P0.normal.z, 1.0f);
				XMVECTOR TanV = XMVectorSet(0, 0, 0, 0);
				XMVECTOR BinV = XMVectorSet(0, 0, 0, 0);
				BinV = XMVector3Cross(NorV, M1);
				TanV = XMVector3Cross(NorV, BinV);
				P0.tangent = { 1, 0, 0 };
			}
		}
	};

	static void StaticInit();
	void AllocateRaytracingMesh(vector<Vertex> vbarr, vector<TriangleIndex> ibarr, int SubMeshNum = 1, int* SubMeshIndexes = nullptr);

	// 생성된 인덱스를 참조하여 생성된다.
	void AllocateRaytracingUAVMesh(vector<Vertex> vbarr, UINT64* inIBStartOffset, int SubMeshNum = 1, int* SubMeshIndexes = nullptr);

	void AllocateRaytracingUAVMesh_OnlyIndex(vector<TriangleIndex> ibarr, int SubMeshNum = 1, int* SubMeshIndexes = nullptr);

	void UAV_BLAS_Refit();
};

struct RayTracingRenderInstance {
	RayTracingMesh* pMesh;
	matrix worldMat;
};

template<>
class hash<ShaderRecord> {
public:
	// 상황에 맞는 헤쉬를 정의할 필요가 있다.
	size_t operator()(const ShaderRecord& s) const {
		size_t d0 = (reinterpret_cast<size_t>(s.shaderIdentifier.ptr) >> 8) << 8 + s.localRootArguments.size;
		d0 = _pdep_u64(d0, 0x5555555555555555);
		size_t d1 = 0;
		for (int i = 0; i < s.localRootArguments.size / 8; ++i) {
			d1 += ((size_t*)s.localRootArguments.ptr)[i];
		}
		d1 = _pdep_u64(d1, 0xAAAAAAAAAAAAAAAA);
		return d0 | d1;
	}
};

class SkinMeshModifyShader {
public:
	ID3D12PipelineState* pPipelineState = nullptr;
	ID3D12RootSignature* pRootSignature = nullptr;

	enum RootParamId {
		Const_OutputVertexBufferSize = 0,
		CBVTable_OffsetMatrixs_ToWorldMatrixs = 1,
		SRVTable_SourceVertexAndBoneData = 2,
		UAVTable_OutputVertexData = 3,
		Normal_RootParamCapacity = 4,
	};

	void InitShader();
	void CreateRootSignature();
	void CreatePipelineState();
	void Add_RegisterShaderCommand(ID3D12GraphicsCommandList* cmd);
	void Release();
};

struct RayTracingShader {
	struct DefaultCB {
		float data[8];
	};

	ID3D12RootSignature* pGlobalRootSignature = nullptr;
	vector<ID3D12RootSignature*> pLocalRootSignature;
	ID3D12StateObject* RTPSO = nullptr;
	vector<RayTracingRenderInstance> RenderCommandList;

	ID3D12Resource* TLAS;
	ID3D12Resource* TLAS_InstanceDescs_Res; // UploadBuffer
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC TLASBuildDesc = {};

	// 출력할 Mesh 물체의 개수
	static constexpr int TLAS_InstanceDescs_Capacity = 1048576;
	int TLAS_InstanceDescs_Size = 0;
	int TLAS_InstanceDescs_ImmortalSize = 0;
	D3D12_RAYTRACING_INSTANCE_DESC* TLAS_InstanceDescs_MappedData = nullptr;

	// immortal 한 인스턴스는 clear가 된 후에 추가가 가능함. 안그럼 쌩뚱맞은 인스턴스가 immortal 영역으로 들어간다.
	// 반환값은 해당 인스턴스의 월드행렬(3x4)를 가리키는 포인터를 반환한다.
	// LRSdata는 LocalRootSignature의 배열이 들어가고, 배열의 크기는 mesh.subMeshCount이다.
	//(서브메쉬마다 LocalRoot 변수들이 바인딩 된다.)
	float** push_rins_immortal(RayTracingMesh* mesh, matrix mat, LocalRootSigData* LRSdata, int hitGroupShaderIdentifyerIndex = 0);
	void clear_rins();
	float** push_rins(RayTracingMesh* mesh, matrix mat, LocalRootSigData* LRSdata, int hitGroupShaderIdentifyerIndex = 0);

	UINT shaderIdentifierSize;
	ComPtr<ID3D12Resource> RayGenShaderTable = nullptr;
	ComPtr<ID3D12Resource> MissShaderTable = nullptr;

	void** hitGroupShaderIdentifier;
	//같은 종류의 렌더메쉬의 개수
	static constexpr int HitGroupShaderTableCapavity = 1048576;
	int HitGroupShaderTableSize = 0;
	int HitGroupShaderTableImmortalSize = 0;
	ShaderTable hitGroupShaderTable;
	ComPtr<ID3D12Resource> HitGroupShaderTable = nullptr;

	// 셰이더 테이블의 Map.
	// 셰이더 레코드의 값으로 해당 레코드가 어떤 위치에 있는지 확인가능하고,
	// 해당 셰이더 레코드가 테이블에 존재하는지도 확인가능하다.
	unordered_map<ShaderRecord, int> HitGroupShaderTableToIndex;
	void InsertShaderRecord(ShaderRecord sr, int index);

	SkinMeshModifyShader* MySkinMeshModifyShader;
	vector<void*> RebuildBLASBuffer;
	

	void Init();
	void CreateGlobalRootSignature();
	void CreateLocalRootSignatures();
	void CreatePipelineState();
	void InitAccelationStructure();
	void BuildAccelationStructure();
	void InitShaderTable();
	void BuildShaderTable();
	void SkinMeshModify();
	void PrepareRender();
};

//지금은 단일인 커맨드만 일단 다룬다. 멀티스레드 커맨드는 나중에..
//template <CmdListType type>
class Shader;

union ShaderType {
	enum RegisterEnum_memberenum {
		RenderNormal = 0, // 일반 렌더링
		RenderWithShadow = 1, // 그림자와 함께 렌더링
		RenderShadowMap = 2, // 쉐도우 맵을 렌더링
		RenderStencil = 3, // 스텐실을 렌더링
		RenderInnerMirror = 4, // 스텐실이 활성화된 부분을 렌더링 (거울 속 렌더링)
		RenderTerrain = 5, // 터레인을 렌더링
		StreamOut = 6, // 스트림아웃
		SDF = 7, // SDF Text
		TessTerrain = 8, // Tess Terrain
		SkinMeshRender = 9,
		InstancingWithShadow = 10,
	};
	int id;

	ShaderType(int i) : id{ i } {}
	ShaderType(RegisterEnum_memberenum e) { id = (int)e; }
	operator int() { return id; }
};

struct SpotLight {
	matrix View;
	GPUResource ShadowMap;
	vec4 LightPos;
	DescIndex descindex;
	ViewportData viewport;
};

struct GPUCmd {
	ID3D12GraphicsCommandList* GraphicsCmdList;
	ID3D12GraphicsCommandList4* DXRCmdList;
	ID3D12CommandAllocator* pCommandAllocator;
	ID3D12CommandQueue* pCommandQueue;
	ID3D12Fence* pFence;
	ui64 FenceValue;
	HANDLE hFenceEvent;
	bool isClose = true;

	GPUCmd() {
		isClose = true;
		FenceValue = 0;
	}
	GPUCmd(ID3D12GraphicsCommandList* cmdlist, ID3D12CommandAllocator* alloc, ID3D12CommandQueue* que) : GraphicsCmdList{ cmdlist }, pCommandAllocator{ alloc },
		pCommandQueue{ que }
	{
		isClose = true;
		FenceValue = 0;
	}
	GPUCmd(ID3D12Device* pDevice, D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_QUEUE_FLAGS flag = D3D12_COMMAND_QUEUE_FLAG_NONE) {
		D3D12_COMMAND_QUEUE_DESC d3dCommandQueueDesc;
		::ZeroMemory(&d3dCommandQueueDesc, sizeof(D3D12_COMMAND_QUEUE_DESC));
		d3dCommandQueueDesc.Flags = flag;
		d3dCommandQueueDesc.Type = type;
		HRESULT hResult = pDevice->CreateCommandQueue(&d3dCommandQueueDesc,
			_uuidof(ID3D12CommandQueue), (void**)&pCommandQueue);
		hResult = pDevice->CreateCommandAllocator(type,
			__uuidof(ID3D12CommandAllocator), (void**)&pCommandAllocator);
		hResult = pDevice->CreateCommandList(0, type,
			pCommandAllocator, NULL, __uuidof(ID3D12GraphicsCommandList), (void**)&GraphicsCmdList);
		isClose = true;

		hResult = pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
			(void**)&pFence);
		FenceValue = 0;
		hFenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

		GraphicsCmdList->QueryInterface(IID_PPV_ARGS(&DXRCmdList));
	}

	operator ID3D12GraphicsCommandList*() { return GraphicsCmdList; }
	__forceinline ID3D12GraphicsCommandList* operator->() { return GraphicsCmdList; }

	HRESULT Reset(bool dxr = false) {
		isClose = false;
		pCommandAllocator->Reset();
		if (dxr) {
			return DXRCmdList->Reset(pCommandAllocator, nullptr);
		}
		else {
			return GraphicsCmdList->Reset(pCommandAllocator, nullptr);
		}
	}
	HRESULT Close(bool dxr = false) {
		isClose = true;
		if (dxr) {
			return DXRCmdList->Close();
		}
		else {
			return GraphicsCmdList->Close();
		}
	}
	void Execute(bool dxr = false) {
		ID3D12CommandList* ppd3dCommandLists[] = { GraphicsCmdList };
		if (dxr) {
			ppd3dCommandLists[0] = DXRCmdList;
		}
		pCommandQueue->ExecuteCommandLists(1, ppd3dCommandLists);
	}

	__forceinline void ResBarrierTr(ID3D12Resource* res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, UINT subRes = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAGS flag = D3D12_RESOURCE_BARRIER_FLAG_NONE) {
		CD3DX12_RESOURCE_BARRIER resBarrier = CD3DX12_RESOURCE_BARRIER::Transition(res, before, after, subRes, flag);
		GraphicsCmdList->ResourceBarrier(1, &resBarrier);
	}

	__forceinline void ResBarrierTr(GPUResource* res, D3D12_RESOURCE_STATES after) {
		res->AddResourceBarrierTransitoinToCommand(GraphicsCmdList, after);
	}

	void SetShader(Shader* shader, ShaderType reg = ShaderType::RenderNormal);

	void WaitGPUComplete() {
		ID3D12CommandQueue* selectQueue;
		if (pCommandQueue == nullptr) {
			selectQueue = pCommandQueue;
		}
		else selectQueue = pCommandQueue;
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

	void Release() {
		if (GraphicsCmdList) GraphicsCmdList->Release();
		if (pCommandAllocator) pCommandAllocator->Release();
		if (pCommandQueue) pCommandQueue->Release();
		if (pFence) pFence->Release();
	}
};

#pragma region RootParamSetTypes
union GRegID {
	unsigned int id;
	struct {
		unsigned short _num;
		char _padding;
		char _type;
	};

	GRegID() {}
	~GRegID() {}

	GRegID(char type, unsigned short num) :
		_type{ type }, _padding{ 0 }, _num{ num }
	{

	}

	GRegID(const GRegID& reg) {
		id = reg.id;
	}
};

struct RootParam1 {
	union {
		CD3DX12_ROOT_PARAMETER1 data;
		D3D12_ROOT_PARAMETER1 origin;
	};
	vector<CD3DX12_DESCRIPTOR_RANGE1> ranges;

	operator D3D12_ROOT_PARAMETER1() {
		return origin;
	}

	RootParam1() {}
	~RootParam1() {}

	void PushDescRange(GRegID regid, const char* RangeType, UINT NumDesc, D3D12_DESCRIPTOR_RANGE_FLAGS flags, UINT registerSpace = 0, UINT OffsetInDescTable = 0) {
		CD3DX12_DESCRIPTOR_RANGE1 range;
		bool error = false;
		if (strcmp(RangeType, "SRV") == 0) {
			range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			error |= regid._type != 't';
		}
		else if (strcmp(RangeType, "UAV") == 0) {
			range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			error |= regid._type != 'u';
		}
		else if (strcmp(RangeType, "CBV") == 0) {
			range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			error |= regid._type != 'b';
		}
		else if (strcmp(RangeType, "Sampler") == 0) {
			range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
			error |= regid._type != 's';
		}

		if (error) {
			dbglog2(L"ERROR set DescRange! %c %d", regid._type, regid._num);
		}
		else {
			regid._type = 0;
			range.NumDescriptors = NumDesc;
			range.BaseShaderRegister = regid.id;
			range.RegisterSpace = registerSpace;
			range.Flags = flags;
			range.OffsetInDescriptorsFromTableStart = OffsetInDescTable;
			ranges.push_back(range);
		}
	}

	void DescTable(D3D12_SHADER_VISIBILITY visible) {
		data.InitAsDescriptorTable(ranges.size(), ranges.data(), visible);
	}

	static D3D12_ROOT_PARAMETER1 Const32s(GRegID regid, UINT DataSize_num32bitValue, D3D12_SHADER_VISIBILITY visible, UINT registerSpace = 0) {
		RootParam1 rp;
		if (regid._type == 'b') {
			UINT reg = regid._num;
			rp.data.InitAsConstants(DataSize_num32bitValue, reg, registerSpace, visible);
		}
		else {
			dbglog2(L"ERROR set RootParam! Const32s %c %d", regid._type, regid._num);
		}

		return rp.origin;
	}

	static D3D12_ROOT_PARAMETER1 CBV(GRegID regid, D3D12_SHADER_VISIBILITY visible, D3D12_ROOT_DESCRIPTOR_FLAGS flags, UINT registerSpace = 0) {
		RootParam1 rp;
		if (regid._type == 'b') {
			regid._type = 0;
			rp.data.InitAsConstantBufferView(regid.id, registerSpace, flags, visible);
		}
		else {
			dbglog2(L"ERROR set RootParam! CBV %c %d", regid._type, regid._num);
		}
		return rp.origin;
	}

	static D3D12_ROOT_PARAMETER1 SRV(GRegID regid, D3D12_SHADER_VISIBILITY visible, D3D12_ROOT_DESCRIPTOR_FLAGS flags, UINT registerSpace = 0) {
		RootParam1 rp;
		if (regid._type == 't') {
			regid._type = 0;
			rp.data.InitAsShaderResourceView(regid.id, registerSpace, flags, visible);
		}
		else {
			dbglog2(L"ERROR set RootParam! SRV %c %d", regid._type, regid._num);
		}
		return rp.origin;
	}

	static D3D12_ROOT_PARAMETER1 UAV(GRegID regid, D3D12_SHADER_VISIBILITY visible, D3D12_ROOT_DESCRIPTOR_FLAGS flags, UINT registerSpace = 0) {
		RootParam1 rp;
		if (regid._type == 'u') {
			regid._type = 0;
			rp.data.InitAsUnorderedAccessView(regid.id, registerSpace, flags, visible);
		}
		else {
			dbglog2(L"ERROR set RootParam! UAV %c %d", regid._type, regid._num);
		}
		return rp.origin;
	}
};
#pragma endregion

// name completly later. ??
struct GlobalDevice {
	ui32 DSVSize;
	ui32 RTVSize;
	ui32 SamplerDescSize;
	union {
		ui32 CBVSize;
		ui32 SRVSize;
		ui32 UAVSize;
		ui32 CBVSRVUAVSize;
	};

	RayTracingDevice raytracing;
	RasterDevice raster;
	bool debugDXGI = false;
	bool isSupportRaytracing = true;
	bool isRaytracingRender = true;

	IDXGIAdapter1* pSelectedAdapter = nullptr;
	IDXGIAdapter1* pOutputAdapter = nullptr;
	int adapterVersion = 0;
	int adapterIndex = 0;
	WCHAR adapterDescription[128] = {};

	D3D_FEATURE_LEVEL minFeatureLevel;

	IDXGIFactory7* pFactory;// question 002 : why dxgi 6, but is type limit 7 ??
	IDXGISwapChain4* pSwapChain = nullptr;
	ID3D12Device* pDevice;

	static constexpr unsigned int SwapChainBufferCount = 2;
	ui32 CurrentSwapChainBufferIndex;

	// maybe seperate later.. ?? i don't know future..
	
	ID3D12Resource* ppRenderTargetBuffers[SwapChainBufferCount];
	ID3D12DescriptorHeap* pRtvDescriptorHeap;
	D3D12_GPU_DESCRIPTOR_HANDLE RenderTargetSRV_pGPU[SwapChainBufferCount];
	ui32 ClientFrameWidth;
	ui32 ClientFrameHeight;

	ID3D12Resource* pDepthStencilBuffer;
	ID3D12DescriptorHeap* pDsvDescriptorHeap;

	GPUCmd gpucmd;
	/*ID3D12CommandQueue* pCommandQueue;
	ID3D12CommandAllocator* pCommandAllocator;
	ID3D12GraphicsCommandList* pCommandList;*/

	GPUCmd CScmd;
	ID3D12CommandQueue* pComputeCommandQueue;
	ID3D12CommandAllocator* pComputeCommandAllocator;
	ID3D12GraphicsCommandList* pComputeCommandList;

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
	unordered_map<wchar_t, GPUResource, hash<wchar_t>> font_sdftexture_map[FontCount];
	vector<wchar_t> addTextureStack;
	vector<wchar_t> addSDFTextureStack;

	// OS에서 전체화면을 지원하는 해상도 리스트
	vector<ResolutionStruct> EnableFullScreenMode_Resolusions;

	/*
	* 설명 : DXGI Factory, GPU Adaptor 등을 초기화한다.
	* EnableFullScreenMode_Resolusions 에 OS 에서 제공하는 전체화면 해상도들을 얻는 작업 또한 수행한다.
	* 때문에 이는 윈도우가 만들어지기 전에 실행되며, 여기에서 크기를 받아 윈도우 사이즈를 초기에 결정한다.
	*/
	void Factory_Adaptor_Output_Init();

	void Init();
	void Release();
	void NewSwapChain();
	void NewGbuffer();
	void WaitGPUComplete(ID3D12CommandQueue* commandQueue = nullptr);

	DescriptorAllotter TextureDescriptorAllotter;

	SVDescPool2 ShaderVisibleDescPool;

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
	GPUResource CreateCommitedGPUBuffer(D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES d3dResourceStates, D3D12_RESOURCE_DIMENSION dimension, int Width, int Height, DXGI_FORMAT BufferFormat = DXGI_FORMAT_UNKNOWN, UINT DepthOrArraySize = 1, D3D12_RESOURCE_FLAGS AdditionalFlag = D3D12_RESOURCE_FLAG_NONE);
	void UploadToCommitedGPUBuffer(void* ptr, GPUResource* uploadBuffer, GPUResource* copydestBuffer = nullptr, bool StateReturning = true);

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

	void AddTextSDFTexture(wchar_t key);
	void AddLineInSDFTexture(float_v2 startpos, float_v2 endpos, char* texture, int width, int height);
	__forceinline char SignedFloatNormalizeToByte(float f);

//#include <vector>
//#include <cmath>
//#include <algorithm>
//#include <cstdint>

	struct squarePair {
		int a, b;
		squarePair() {

		}

		~squarePair(){}

		squarePair(const squarePair& ref) {
			a = ref.a;
			b = ref.b;
		}

		bool operator==(squarePair ref) {
			return (ref.a == a && ref.b == b);
		}
	};

	vector<squarePair> Spairs;

// copliot
	static inline float clampf(float v, float lo, float hi) {
		return max(lo, min(v, hi));
	}
	// 간단한 2-pass distance transform (근사)
	std::vector<float> edt(const std::vector<uint8_t>& mask, int width, int height) {
		//init..
		if (Spairs.size() == 0) {
			for (int i = 0; i < height; ++i) {
				for (int k = 0; k < height; ++k) {
					squarePair p;
					p.a = i;
					p.b = k;
					Spairs.push_back(p);
				}
			}
			std::sort(Spairs.begin(), Spairs.end(), [](const squarePair& A, const squarePair& B) {
				int an = A.a * A.a + A.b * A.b;
				int bn = B.a * B.a + B.b * B.b;
				return an < bn;
			});
			auto last = std::unique(Spairs.begin(), Spairs.end());
			Spairs.erase(last, Spairs.end());
		}

		const float INF = 1e9f;
		std::vector<float> d(width * height, INF);

		// 초기화
		for (int i = 0; i < width * height; ++i) {
			if (mask[i]) d[i] = 0.0f;
		}

		for (int yi = 0; yi < height; ++yi) {
			for (int xi = 0; xi < width; ++xi) {
				if (d[yi * width + xi] != 0.0f) {
					for (int i = 0; i < Spairs.size(); ++i) {
						float multotal = 1.0f;
						int yibmN = yi - Spairs[i].b;
						int xiamN = xi - Spairs[i].a;
						int yiamN = yi - Spairs[i].a;
						int xibmN = xi - Spairs[i].b;
						int yibpN = yi + Spairs[i].b;
						int xiapN = xi + Spairs[i].a;
						int yiapN = yi + Spairs[i].a;
						int xibpN = xi + Spairs[i].b;
						bool yibm = yibmN >= 0;
						bool xiam = xiamN >= 0;
						bool yiam = yiamN >= 0;
						bool xibm = xibmN >= 0;
						bool yibp = yibpN < height;
						bool xiap = xiapN < width;
						bool yiap = yiapN < height;
						bool xibp = xibpN < width;
						if (yibm) {
							if (xiam) multotal *= d[yibmN * width + xiamN];
							if (xiap) multotal *= d[yibmN * width + xiapN];
						}
						if (yibp) {
							if (xiam) multotal *= d[yibpN * width + xiamN];
							if (xiap) multotal *= d[yibpN * width + xiapN];
						}
						if (yiam) {
							if (xibm) multotal *= d[yiamN * width + xibmN];
							if (xibp) multotal *= d[yiamN * width + xibpN];
						}
						if (yiap) {
							if (xibm) multotal *= d[yiapN * width + xibmN];
							if (xibp) multotal *= d[yiapN * width + xibpN];
						}

						if (multotal == 0.0f) {
							squarePair& p = Spairs[i];
							d[yi * width + xi] = (float)(p.a*p.a+p.b*p.b);
							break;
						}
					}
				}
			}
		}

		// sqrt로 근사 유클리드 거리
		for (float& v : d) v = std::sqrt(v);
		return d;
	}

	std::vector<uint8_t> makeSDF(char* raw, int width, int height, float distanceMul, float radius = -1.0f) {
		int N = width * height;
		std::vector<uint8_t> mask(N);
		vector<vec2f> outlines;

		// mask 생성 (0 → 내부, 127 → 외부)
		for (int i = 0; i < N; ++i) {
			mask[i] = (raw[i] == 0) ? 1 : 0;
			if (raw[i] == 0) {
				outlines.push_back(vec2f(i % width, i / width));
			}
		}

		// 내부/외부 거리 계산
		auto d_in = edt(mask, width, height);

		std::vector<uint8_t> inv(N);
		for (int i = 0; i < N; ++i) inv[i] = 1 - mask[i];
		auto d_out = edt(inv, width, height);

		// signed distance
		std::vector<float> sdf(N);
		for (int i = 0; i < N; ++i) sdf[i] = distanceMul*d_in[i] - distanceMul*d_out[i];

		// 반경 설정 (자동)
		if (radius <= 0.0f) radius = max(1.0f, 0.05f * min(width, height));

		// 0~255 매핑
		std::vector<uint8_t> out(N);
		for (int i = 0; i < N; ++i) {
			float n = clampf(sdf[i] / radius, -1.0f, 1.0f);
			float u = (n * 0.5f + 0.5f) * 255.0f;
			out[i] = static_cast<uint8_t>(std::round(u));
		}

		return out;
	}

	void GetAABBFromOBB(vec4* out, BoundingOrientedBox obb, bool first = false) {
		matrix mat;
		OBB_vertexVector ovv;
		mat.trQ(obb.Orientation);
		vec4 xm = -obb.Extents.x * mat.right;
		vec4 xp = obb.Extents.x * mat.right;
		vec4 ym = -obb.Extents.y * mat.up;
		vec4 yp = obb.Extents.y * mat.up;
		vec4 zm = -obb.Extents.z * mat.look;
		vec4 zp = obb.Extents.z * mat.look;

		vec4 pos = vec4(obb.Center);

		vec4 curr = xm + ym + zm + pos;
		if (first == false) {
			if (out[0].x > curr.x) out[0].x = curr.x;
			if (out[0].y > curr.y) out[0].y = curr.y;
			if (out[0].z > curr.z) out[0].z = curr.z;
			if (out[1].x < curr.x) out[1].x = curr.x;
			if (out[1].y < curr.y) out[1].y = curr.y;
			if (out[1].z < curr.z) out[1].z = curr.z;
		}
		else {
			out[0] = curr;
			out[1] = curr;
		}

		curr = xm + ym + zp + pos;
		if (out[0].x > curr.x) out[0].x = curr.x;
		if (out[0].y > curr.y) out[0].y = curr.y;
		if (out[0].z > curr.z) out[0].z = curr.z;
		if (out[1].x < curr.x) out[1].x = curr.x;
		if (out[1].y < curr.y) out[1].y = curr.y;
		if (out[1].z < curr.z) out[1].z = curr.z;

		curr = xm + yp + zm + pos;
		if (out[0].x > curr.x) out[0].x = curr.x;
		if (out[0].y > curr.y) out[0].y = curr.y;
		if (out[0].z > curr.z) out[0].z = curr.z;
		if (out[1].x < curr.x) out[1].x = curr.x;
		if (out[1].y < curr.y) out[1].y = curr.y;
		if (out[1].z < curr.z) out[1].z = curr.z;

		curr = xm + yp + zp + pos;
		if (out[0].x > curr.x) out[0].x = curr.x;
		if (out[0].y > curr.y) out[0].y = curr.y;
		if (out[0].z > curr.z) out[0].z = curr.z;
		if (out[1].x < curr.x) out[1].x = curr.x;
		if (out[1].y < curr.y) out[1].y = curr.y;
		if (out[1].z < curr.z) out[1].z = curr.z;

		curr = xp + ym + zm + pos;
		if (out[0].x > curr.x) out[0].x = curr.x;
		if (out[0].y > curr.y) out[0].y = curr.y;
		if (out[0].z > curr.z) out[0].z = curr.z;
		if (out[1].x < curr.x) out[1].x = curr.x;
		if (out[1].y < curr.y) out[1].y = curr.y;
		if (out[1].z < curr.z) out[1].z = curr.z;

		curr = xp + ym + zp + pos;
		if (out[0].x > curr.x) out[0].x = curr.x;
		if (out[0].y > curr.y) out[0].y = curr.y;
		if (out[0].z > curr.z) out[0].z = curr.z;
		if (out[1].x < curr.x) out[1].x = curr.x;
		if (out[1].y < curr.y) out[1].y = curr.y;
		if (out[1].z < curr.z) out[1].z = curr.z;

		curr = xp + yp + zm + pos;
		if (out[0].x > curr.x) out[0].x = curr.x;
		if (out[0].y > curr.y) out[0].y = curr.y;
		if (out[0].z > curr.z) out[0].z = curr.z;
		if (out[1].x < curr.x) out[1].x = curr.x;
		if (out[1].y < curr.y) out[1].y = curr.y;
		if (out[1].z < curr.z) out[1].z = curr.z;

		curr = xp + yp + zp + pos;
		if (out[0].x > curr.x) out[0].x = curr.x;
		if (out[0].y > curr.y) out[0].y = curr.y;
		if (out[0].z > curr.z) out[0].z = curr.z;
		if (out[1].x < curr.x) out[1].x = curr.x;
		if (out[1].y < curr.y) out[1].y = curr.y;
		if (out[1].z < curr.z) out[1].z = curr.z;
	}

	default_random_engine dre;
	uniform_real_distribution<float> urd{ 0.0f, 1000000.0f };
	float randomRangef(float min, float max) {
		float r = urd(dre);
		return min + r * (max - min) / 1000000.0f;
	}

	struct TextureWithUAV
	{
		ID3D12Resource* texture;
		DescIndex handle;
	};

	// width, height 크기의 텍스처와 UAV를 생성하는 함수
	TextureWithUAV CreateTextureWithUAV(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format)
	{
		TextureWithUAV result{};

		// 1. 텍스처 리소스 생성
		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Alignment = 0;
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.DepthOrArraySize = 1;
		texDesc.MipLevels = 1;
		texDesc.Format = format;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

		HRESULT hr = device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS(&result.texture)
		);
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create texture resource");
		}

		// 2. UAV 생성
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = format;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;
		uavDesc.Texture2D.PlaneSlice = 0;

		ShaderVisibleDescPool.ImmortalAlloc(&result.handle, 1);
		device->CreateUnorderedAccessView(result.texture, nullptr, &uavDesc, result.handle.hCreation.hcpu);
		return result;
	}
	TextureWithUAV BlurTexture;

	struct RenderTargetBundle
	{
		ID3D12Resource* resource;
		DescHandle rtvHandle;
		DescIndex srvHandle;
	};

	RenderTargetBundle CreateRenderTargetTextureWithRTV(
		ID3D12Device* device,
		ID3D12DescriptorHeap* rtvHeap,
		UINT rtvIndex,
		UINT width,
		UINT height,
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM)
	{
		// 결과 반환
		RenderTargetBundle bundle;

		UINT rtvDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		// 리소스 설명
		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Alignment = 0;
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.DepthOrArraySize = 1;
		texDesc.MipLevels = 1;
		texDesc.Format = format;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET |
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; // UAV도 허용

		// RTV 클리어 값
		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = format;
		clearValue.Color[0] = 0.0f;
		clearValue.Color[1] = 0.0f;
		clearValue.Color[2] = 0.0f;
		clearValue.Color[3] = 1.0f;

		// 리소스 생성
		ID3D12Resource* renderTarget;
		CD3DX12_HEAP_PROPERTIES hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		HRESULT hr = device->CreateCommittedResource(
			&hp,
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_RENDER_TARGET, // 초기 상태 RTV
			&clearValue,
			IID_PPV_ARGS(&renderTarget)
		);
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create render target texture.");
		}

		// RTV 핸들 할당
		DescHandle rtvHandle;
		rtvHandle.hcpu.ptr = rtvHeap->GetCPUDescriptorHandleForHeapStart().ptr + rtvIndex * rtvDescriptorSize;

		// RTV 생성
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = format;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;

		device->CreateRenderTargetView(renderTarget, &rtvDesc, rtvHandle.hcpu);

		// shader Resource View
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.MostDetailedMip = 0;
		ShaderVisibleDescPool.ImmortalAlloc(&bundle.srvHandle, 1);
		pDevice->CreateShaderResourceView(renderTarget, &srvDesc, bundle.srvHandle.hCreation.hcpu);

		bundle.resource = renderTarget;
		bundle.rtvHandle = rtvHandle;
		return bundle;
	}

	RenderTargetBundle SubRenderTarget;

	void CreateDefaultHeap_VB(void* ptr, GPUResource& VertexBuffer, GPUResource& VertexUploadBuffer, D3D12_VERTEX_BUFFER_VIEW& view, UINT VertexCount, UINT sizeofVertex);

	// IndexCount는 TriangleIndex의 size() * 3.
	template <int indexByteSize>
	void CreateDefaultHeap_IB(void* ptr, GPUResource& IndexBuffer, GPUResource& IndexUploadBuffer, D3D12_INDEX_BUFFER_VIEW& view, UINT IndexCount);

	static constexpr int GbufferCount = 1;
	RenderingMod RenderMod = ForwardRendering;
	DXGI_FORMAT MainRenderTarget_PixelFormat;
	DXGI_FORMAT GbufferPixelFormatArr[GbufferCount];
	GPUResource ppGBuffers[GbufferCount] = {};

	static constexpr UINT max_lightProbeCount = 1024;
	__forceinline UINT GetLightProbeRTVIndex(UINT index) {
		return SwapChainBufferCount + GbufferCount + 1 + 6 * index;
	}

	static constexpr UINT max_DirLightCascadingShadowCount = 4;
	__forceinline UINT GetDirLightCascadingShadowDSVIndex(UINT index) {
		return 2 + index;
	}

	static constexpr UINT max_PointLightMaxCount = 1024;
	__forceinline UINT GetPointLightShadowDSVIndex(UINT index) {
		return 2 + max_DirLightCascadingShadowCount + 6 * index;
	}

	GPUResource CreateShadowMap(int width, int height, int DSVoffset, SpotLight& spotLight);

	ui64 tickStack[128] = {};
	ui64 ft[128] = {};
	ui32 cnt[128] = {};
	__forceinline void AverageTimerStart(int id = 0) {
		ft[id] = GetTicks();
	}
	__forceinline void AverageTimerEnd(int id = 0) {
		ui64 et = GetTicks();
		cnt[id] += 1;
		tickStack[id] += et - ft[id];
		if (cnt[id] > 128) {
			double average = (double)(tickStack[id] / 128) / (double)QUERYPERFORMANCE_HZ;
			dbglog2(L"average[%d] : %g\n", id, average);
			tickStack[id] = 0;
		}
	}
};
GlobalDevice gd;

template<D3D12_DESCRIPTOR_HEAP_TYPE type>
inline DescHandle DescHandle::operator[](UINT index)
{
	DescHandle handle = *this;
	UINT incSiz = gd.CBVSRVUAVSize;
	if constexpr (type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV) {
		incSiz = gd.RTVSize;
	}
	else if constexpr (type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV) {
		incSiz = gd.DSVSize;
	}
	else if constexpr (type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER) {
		incSiz = gd.SamplerDescSize;
	}
	handle.operator+=(index * incSiz);
	return handle;
}

class Shader {
public:
	ID3D12PipelineState* pPipelineState = nullptr;
	ID3D12PipelineState* pPipelineState_withShadow = nullptr;
	ID3D12PipelineState* pPipelineState_RenderShadowMap = nullptr;
	ID3D12PipelineState* pPipelineState_RenderStencil = nullptr;
	ID3D12PipelineState* pPipelineState_RenderInnerMirror = nullptr;
	ID3D12PipelineState* pPipelineState_Terrain = nullptr;

	ID3D12RootSignature* pRootSignature = nullptr;
	ID3D12RootSignature* pRootSignature_withShadow = nullptr;
	ID3D12RootSignature* pRootSignature_Terrain = nullptr;
	Shader();
	virtual ~Shader();

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreateRootSignature_withShadow();
	virtual void CreateRootSignature_Terrain();
	virtual void CreatePipelineState();
	virtual void CreatePipelineState_withShadow();
	virtual void CreatePipelineState_RenderShadowMap();
	virtual void CreatePipelineState_RenderStencil();
	virtual void CreatePipelineState_InnerMirror();
	virtual void CreatePipelineState_Terrain();

	virtual void Add_RegisterShaderCommand(GPUCmd& cmd, ShaderType reg = ShaderType::RenderNormal);
	virtual void Release();

	static D3D12_SHADER_BYTECODE GetShaderByteCode(const WCHAR* pszFileName, LPCSTR pszShaderName, LPCSTR pszShaderProfile, ID3DBlob** ppd3dShaderBlob, vector<D3D_SHADER_MACRO>* macros = nullptr);

	virtual void SetShadowMapCommand(DescHandle shadowMapDesc);

	enum RootParamId {
		Const_Camera = 0,
		Const_Transform = 1,
		CBV_StaticLight = 2,
		Normal_RootParamCapacity = 3,
		SRVTable_ShadowMap = 3,
		withShadow_RootParamCapacity = 4,
	};
};

class OnlyColorShader : public Shader {
public:
	OnlyColorShader();
	virtual ~OnlyColorShader();

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();
	virtual void Release();

	virtual void Add_RegisterShaderCommand(GPUCmd& cmd, ShaderType reg = ShaderType::RenderNormal);

	enum RootParamId {
		Const_Camera = 0,
		Const_Transform = 1,
		Normal_RootParamCapacity = 2,
	};
};

class DiffuseTextureShader : public Shader {
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

	virtual void Add_RegisterShaderCommand(GPUCmd& cmd, ShaderType reg = ShaderType::RenderNormal);

	void SetTextureCommand(GPUResource* texture);
};

class ScreenCharactorShader : public Shader{
public:
	ID3D12PipelineState* pPipelineState_SDF = nullptr;
	ID3D12RootSignature* pRootSignature_SDF = nullptr;

	ScreenCharactorShader();
	virtual ~ScreenCharactorShader();

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();
	virtual void CreateRootSignature_SDF();
	virtual void CreatePipelineState_SDF();
	virtual void Release();

	virtual void Add_RegisterShaderCommand(GPUCmd& cmd, ShaderType reg = ShaderType::RenderNormal);

	void SetTextureCommand(GPUResource* texture);

	enum RootParamId {
		Const_BasicInfo = 0,
		SRVTable_Texture = 1,
		SRVTable_SDFTexture = 1,
		Normal_RootParamCapacity = 2,
	};
};

class ShadowMappingShader{
public:
	ID3D12DescriptorHeap* ShadowDescriptorHeap;
	int dsvIncrement = 0;

	ShadowMappingShader();
	virtual ~ShadowMappingShader();

	void InitShader();
	void Release();
	void RegisterShadowMap(GPUResource* shadowMap);
	GPUResource CreateShadowMap(int width, int height, int DSVoffset);
};

class PBRShader1 : public Shader {
public:
	ID3D12PipelineState* pPipelineState_TessTerrain = nullptr;
	ID3D12RootSignature* pRootSignature_TessTerrain = nullptr;
	ID3D12PipelineState* pPipelineState_SkinedMesh_withShadow = nullptr;
	ID3D12RootSignature* pRootSignature_SkinedMesh_withShadow = nullptr;
	ID3D12PipelineState* pPipelineState_Instancing_withShadow = nullptr;
	ID3D12RootSignature* pRootSignature_Instancing_withShadow = nullptr;

	PBRShader1() {}
	virtual ~PBRShader1() {}

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreateRootSignature_withShadow();
	virtual void CreateRootSignature_Terrain();
	virtual void CreateRootSignature_TesslationTerrain();
	virtual void CreateRootSignature_SkinedMesh();
	virtual void CreateRootSignature_Instancing_withShadow();

	virtual void CreatePipelineState();
	virtual void CreatePipelineState_withShadow();
	virtual void CreatePipelineState_RenderShadowMap();
	virtual void CreatePipelineState_RenderStencil();
	virtual void CreatePipelineState_InnerMirror();
	virtual void CreatePipelineState_Terrain();
	virtual void CreatePipelineState_TesslationTerrain();
	virtual void CreatePipelineState_SkinedMesh();
	virtual void CreatePipelineState_Instancing_withShadow();

	void ReBuild_Shader(ShaderType st);

	virtual void Release();

	enum RootParamId {
		Const_Camera = 0,
		Const_Transform = 1,
		CBV_StaticLight = 2,
		SRVTable_MaterialTextures = 3,
		CBVTable_Material = 4,
		Normal_RootParamCapacity = 5,
		SRVTable_ShadowMap = 5,
		withShaow_RootParamCapacity = 6,

		CBVTable_SkinMeshOffsetMatrix = 1,
		CBVTable_SkinMeshToWorldMatrix = 2,
		CBVTable_SkinMeshLightData = 3,
		SRVTable_SkinMeshMaterialTextures = 4,
		CBVTable_SkinMeshMaterial = 5,
		SRVTable_SkinMeshShadowMaps = 6,
		withSkinMeshShaow_RootParamCapacity = 7,

		CBVTable_Instancing_DirLightData = 1,
		SRVTable_Instancing_RenderInstance = 2,
		SRVTable_Instancing_MaterialPool = 3,
		SRVTable_Instancing_ShadowMap = 4,
		SRVTable_Instancing_MaterialTexturePool = 5,
		Instancing_Capacity = 6,
	};

	union eTextureID {
		enum texid {
			Color = 0,
			Normal = 1,
			AO = 2,
			Matalic = 3,
			Roughness = 4,
		};
		unsigned int num;
		eTextureID(texid tid) { num = (unsigned int)tid; }
		eTextureID(unsigned int n) { num = n; }
		operator texid() { return (texid)num; }
		operator unsigned int() { return num; }
	};

	virtual void Add_RegisterShaderCommand(GPUCmd& cmd, ShaderType reg = ShaderType::RenderNormal);

	void SetTextureCommand(GPUResource* Color, GPUResource* Normal, GPUResource* AO, GPUResource* Metalic, GPUResource* Roughness);
	virtual void SetShadowMapCommand(DescHandle shadowMapDesc);
	void SetMaterialCBV(D3D12_GPU_DESCRIPTOR_HANDLE hgpu);
};

class SkyBoxShader : public Shader {
public:
	GPUResource CurrentSkyBox;
	GPUResource SkyBoxMesh;
	GPUResource VertexUploadBuffer;
	D3D12_VERTEX_BUFFER_VIEW SkyBoxMeshVBView;

	void LoadSkyBox(const wchar_t* filename);

	SkyBoxShader();
	virtual ~SkyBoxShader();

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();
	virtual void CreatePipelineState_InnerMirror();

	virtual void Add_RegisterShaderCommand(GPUCmd& cmd, ShaderType reg = ShaderType::RenderNormal);
	virtual void Release();

	void RenderSkyBox(matrix parentMat = XMMatrixIdentity(), ShaderType reg = ShaderType::RenderNormal);
};

class GSBilboardShader : public Shader {
public:
	GSBilboardShader();
	~GSBilboardShader();
	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();
	virtual void Add_RegisterShaderCommand(GPUCmd& cmd, ShaderType reg = ShaderType::RenderNormal);
	virtual void Release();
};

struct GS_OUTPUT_PARTICLE
{
	vec4 position;
	vec4 color;
	XMFLOAT3 normalW;
	vec4 positionW;
	XMFLOAT2 uv;
};

struct ParticleVertex0
{
	XMFLOAT3 pos;
	XMFLOAT3 velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	float m_fLifetime = 0.0f;
	UINT m_nType = 0;
};

struct StreamOutputPoints {
	int m_nVertices;
	int m_nStride;
	D3D_PRIMITIVE_TOPOLOGY m_d3dPrimitiveTopology;
	D3D12_STREAM_OUTPUT_BUFFER_VIEW StreamOutBufferView;
	GPUResource StreamOutBuffer;
	GPUResource StreamOutUploadBuffer;
	GPUResource DrawBuffer;
	GPUResource VertexBufer;

	GPUResource BufferFilledSizeRes;
	GPUResource UploadBufferFilledSizeRes;
	GPUResource ReadBackBufferFilledSizeRes;

	D3D12_VERTEX_BUFFER_VIEW VertexBuferView;
	matrix worldMat;

	StreamOutputPoints() {}
	~StreamOutputPoints() {}

	void Init(int vertexCount);
	void Render(bool isStreamOut);

	void CreateVertexBuffer(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, XMFLOAT3 xmf3Position, XMFLOAT3 xmf3Velocity, float fLifetime, int vertexMaxCount)
	{
		m_nVertices = vertexMaxCount;
		m_nStride = sizeof(ParticleVertex0);
		m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;

		ParticleVertex0* pVertices = new ParticleVertex0[vertexMaxCount];

		for (int i = 0; i < vertexMaxCount; ++i) {
			pVertices[i].pos = xmf3Position;
			vec4 velo = vec4(gd.randomRangef(-1, 1), 1, gd.randomRangef(-1, 1), 0);
			velo.len3 = 1;
			pVertices[i].velocity = velo.f3;
			pVertices[i].m_fLifetime = fLifetime;
			pVertices[i].m_nType = 0;
		}
		
		VertexBufer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, m_nStride * m_nVertices, 1, DXGI_FORMAT_UNKNOWN);
		GPUResource VertexUploadBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, vertexMaxCount * sizeof(ParticleVertex0), 1);
		gd.UploadToCommitedGPUBuffer(&pVertices[0], &VertexUploadBuffer, &VertexBufer, true);

		/*m_pd3dVertexBuffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, pVertices, m_nStride * m_nVertices, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dVertexUploadBuffer);*/
		VertexBuferView.BufferLocation = VertexBufer.resource->GetGPUVirtualAddress();
		VertexBuferView.StrideInBytes = m_nStride;
		VertexBuferView.SizeInBytes = m_nStride * m_nVertices;

		delete[] pVertices;
		VertexUploadBuffer.Release();

		__debugbreak();
	}
};

class ParticleShader : public Shader {
public :
	ID3D12PipelineState* pPipelineState_StreamOutput = nullptr;

	ParticleShader();
	~ParticleShader();

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();
	virtual void CreatePipelineState_StremOutput();

	virtual void Add_RegisterShaderCommand(GPUCmd& cmd, ShaderType reg = ShaderType::RenderNormal);
	virtual void Release();

	virtual void SetShadowMapCommand(DescHandle shadowMapDesc);
};

class TesselationTestShader : public Shader {
public:
	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();

	virtual void Add_RegisterShaderCommand(GPUCmd& cmd, ShaderType reg = ShaderType::RenderNormal);
	virtual void Release();
};

class ComputeTestShader : public Shader {
public:
	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();

	virtual void Add_RegisterShaderCommand(GPUCmd& cmd, ShaderType reg = ShaderType::RenderNormal);
	virtual void Release();
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

struct RenderInstanceData {
	matrix worldMat;
	UINT MaterialIndex;
	UINT extra;

	RenderInstanceData() {}
	~RenderInstanceData() {}

	RenderInstanceData(matrix world, UINT materialID)
	{
		worldMat = world;
		MaterialIndex = materialID;
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

	enum MeshType {
		_Mesh = 0,
		UVMesh = 1,
		BumpMesh = 2,
		SkinedBumpMesh = 3
	};
	MeshType type;

	XMFLOAT3 OBB_Ext;
	XMFLOAT3 OBB_Tr;

	int subMeshNum = 0;
	int* SubMeshIndexStart = nullptr; // slotNum + 1 개의 int.
	// i 번째 Mesh : (slotIndexStart[i] ~ slotIndexStart[i+1]-1)
	
	struct InstancingStruct {
		GPUResource StructuredBuffer;
		RenderInstanceData* InstanceDataArr = nullptr;
		Mesh* mesh = nullptr;
		unsigned int Capacity = 0;
		unsigned int InstanceSize = 0;
		DescIndex InstancingSRVIndex;

		InstancingStruct() {
			InstanceDataArr = nullptr;
			mesh = nullptr;
			Capacity = 0;
			InstanceSize = 0;
		}

		void Init(unsigned int capacity, Mesh* _mesh);
		int PushInstance(RenderInstanceData instance);

		//만약 항목이 업데이트가 잦지 않은 고정적인 업데이트인 경우 특정 오브젝트를 렌더에서 제외시킨다.
		void DestroyInstance(RenderInstanceData* instance);

		//매 프레임마다 인스턴싱 항목을 업데이트할 경우 이것을 사용한다.
		void ClearInstancing();
	};
	static constexpr int MinInstancingStartSize = 0;
	InstancingStruct* InstanceData = nullptr; // 서브메쉬의 개수만큼 존재.
	void InstancingInit();

	void SetOBBDataWithAABB(XMFLOAT3 minpos, XMFLOAT3 maxpos);

	Mesh() {
		type = _Mesh;
	}
	virtual ~Mesh();

	virtual void ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering = true);
	virtual void Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum, ui32 slotIndex = 0);
	
	virtual void BatchRender(ID3D12GraphicsCommandList* pCommandList);

	BoundingOrientedBox GetOBB();
	void CreateGroundMesh(ID3D12GraphicsCommandList* pCommandList);
	void CreateWallMesh(float width, float height, float depth, vec4 color);

	void CreateTesslationTestMesh(float width, float heightMul);

	virtual void Release();
};

class UVMesh : public Mesh {
public:
	UVMesh() {
		type = Mesh::MeshType::UVMesh;
	}

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
	virtual void Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum, ui32 slotIndex=0);

	virtual void Release();

	void CreateTextRectMesh();

	void CreatePointMesh_FromVertexData(vector<Vertex>& vert);
};

class BumpMesh : public Mesh {
public:
	int material_index;

	RayTracingMesh rmesh;

	typedef RayTracingMesh::Vertex Vertex;

	BumpMesh() {
		type = Mesh::MeshType::BumpMesh;
	}
	virtual ~BumpMesh();
	void CreateMesh_FromVertexAndIndexData(vector<Vertex>& vert, vector<TriangleIndex>& inds, int SlotNum = 1, int* SlotArr = nullptr, bool include_DXR = true);
	virtual void ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering = true, bool include_DXR = true);
	static BumpMesh* MakeTerrainMeshFromHeightMap(const char* HeightMapTexFilename, float bumpScale, float Unit, int& XN, int& ZN, byte8** Heightmap, bool include_DXR = true);
	void MakeTessTerrainMeshFromHeightMap(float EdgeLen, int xdiv, int zdiv);
	virtual void Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum, ui32 slotIndex = 0);
	void MakeMeshFromWChar(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList
		* pd3dCommandList, const wchar_t wchar, float fontsiz);
	virtual void Release();

	virtual void BatchRender(ID3D12GraphicsCommandList* pCommandList);
};

class BumpSkinMesh : public Mesh {
public:
	int material_index;
	matrix* OffsetMatrixs;
	int MatrixCount;
	GPUResource ToOffsetMatrixsCB;
	int* toNodeIndex;
	vector<matrix> BoneMatrixs;
	
	GPUResource BoneWeightBuffer;
	GPUResource BoneWeightUploadBuffer;
	D3D12_VERTEX_BUFFER_VIEW RenderVBufferView[2];

	// 이 둘은 연속되어 있음. DescTable로 둘다 동시 참조 가능.
	DescIndex VertexSRV; // non shader visible desc heap에 위치함.
	DescIndex BoneSRV; // non shader visible desc heap에 위치함.
	RayTracingMesh rmesh;

	// raytracing할때 수정가능한 버택스 배열을 VB에 올리기 위해 사용된다.
	

	struct BoneWeight {
		int boneID;
		float weight;

		BoneWeight() {
			boneID = 0;
			weight = 0;
		}

		BoneWeight(int bID, float w) :
			boneID{ bID }, weight{ w }
		{

		}
	};

	struct BoneWeight4 {
		BoneWeight bones[4];
		BoneWeight4() {

		}
		BoneWeight4(BoneWeight b0, BoneWeight b1, BoneWeight b2, BoneWeight b3) {
			bones[0] = b0;
			bones[1] = b1;
			bones[2] = b2;
			bones[3] = b3;
		}

		void PushBones(BoneWeight p) {
			for (int i = 0; i < 4; ++i) {
				if (bones[i].boneID < 0) {
					bones[i] = p;
					return;
				}
			}

			for (int i = 0; i < 4; ++i) {
				if (bones[i].weight < p.weight) {
					bones[i] = p;
					return;
				}
			}
		}
	};

	struct MatNode {
		matrix mat;
		int parent; // NULL = -1
	};

	//struct Vertex {
	//	XMFLOAT3 position;
	//	XMFLOAT2 uv;
	//	XMFLOAT3 normal;
	//	XMFLOAT3 tangent;
	//	BoneWeight boneWeight[4];
	//	
	//	Vertex() {}
	//	Vertex(XMFLOAT3 p, XMFLOAT3 nor, XMFLOAT2 _uv, XMFLOAT3 tan, XMFLOAT3 bit, BoneWeight b0, BoneWeight b1, BoneWeight b2, BoneWeight b3)
	//		: position{ p }, normal{ nor }, uv{ _uv }, tangent{ tan }, bitangent{ bit }
	//	{
	//		boneWeight[0] = b0;
	//		boneWeight[1] = b1;
	//		boneWeight[2] = b2;
	//		boneWeight[3] = b3;
	//	}
	//	bool HaveToSetTangent() {
	//		bool b = ((tangent.x == 0 && tangent.y == 0) && tangent.z == 0);
	//		b |= ((isnan(tangent.x) || isnan(tangent.y)) || isnan(tangent.z));
	//		b |= ((bitangent.x == 0 && bitangent.y == 0) && bitangent.z == 0);
	//		b |= ((isnan(bitangent.x) || isnan(bitangent.y)) || isnan(bitangent.z));
	//		return b;
	//	}
	//	static void CreateTBN(Vertex& P0, Vertex& P1, Vertex& P2) {
	//		vec4 N1, N2, M1, M2;
	//		//XMFLOAT3 N0, N1, M0, M1;
	//		N1 = vec4(P1.uv) - vec4(P0.uv);
	//		N2 = vec4(P2.uv) - vec4(P0.uv);
	//		M1 = vec4(P1.position) - vec4(P0.position);
	//		M2 = vec4(P2.position) - vec4(P0.position);
	//		float f = 1.0f / (N1.x * N2.y - N2.x * N1.y);
	//		vec4 tan = vec4(P0.tangent);
	//		tan = f * (M1 * N2.y - M2 * N1.y);
	//		vec4 v = XMVector3Normalize(tan);
	//		P0.tangent = v.f3;
	//		XMVECTOR bit = vec4(P0.bitangent);
	//		bit = f * (M2 * N1.x - M1 * N2.x);
	//		v = XMVector3Normalize(bit);
	//		P0.bitangent = v.f3;
	//		if (P0.HaveToSetTangent()) {
	//			// why tangent is nan???
	//			XMVECTOR NorV = XMVectorSet(P0.normal.x, P0.normal.y, P0.normal.z, 1.0f);
	//			XMVECTOR TanV = XMVectorSet(0, 0, 0, 0);
	//			XMVECTOR BinV = XMVectorSet(0, 0, 0, 0);
	//			BinV = XMVector3Cross(NorV, M1);
	//			TanV = XMVector3Cross(NorV, BinV);
	//			P0.tangent = { 1, 0, 0 };
	//			P0.bitangent = { 0, 0, 1 };
	//		}
	//	}
	//};

	typedef RayTracingMesh::Vertex Vertex;
	
	vector<Vertex> vertexData;

	struct BoneData {
		BoneWeight boneWeight[4];
	};

	BumpSkinMesh() {
		type = Mesh::MeshType::SkinedBumpMesh;
	}
	virtual ~BumpSkinMesh();
	void CreateMesh_FromVertexAndIndexData(vector<Vertex>& vert, vector<BoneData>& bonedata, vector<TriangleIndex>& inds, int SubMeshNum = 1, int* SubMeshIndexArr = nullptr, matrix* NodeLocalMatrixs = nullptr, int matrixCount = 0);
	virtual void Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum, ui32 slotIndex = 0);
	virtual void Release();

	virtual void BatchRender(ID3D12GraphicsCommandList* pCommandList);
};

enum eTextureSemantic {
	NONE = 0,
	DIFFUSE = 1,
	SPECULAR = 2,
	AMBIENT = 3,
	EMISSIVE = 4,
	HEIGHT = 5,
	NORMALS = 6,
	SHININESS = 7,
	OPACITY = 8,
	DISPLACEMENT = 9,
	LIGHTMAP = 10,
	REFLECTION = 11,
	BASE_COLOR = 12,
	NORMAL_CAMERA = 13,
	EMISSION_COLOR = 14,
	METALNESS = 15,
	DIFFUSE_ROUGHNESS = 16,
	AMBIENT_OCCLUSION = 17,
	UNKNOWN = 18,
	SHEEN = 19,
	CLEARCOAT = 20,
	TRANSMISSION = 21,
	MAYA_BASE = 22,
	MAYA_SPECULAR = 23,
	MAYA_SPECULAR_COLOR = 24,
	MAYA_SPECULAR_ROUGHNESS = 25,
	ANISOTROPY = 26,
	GLTF_METALLIC_ROUGHNESS = 27,
};

enum eBaseTextureKind {
	Diffuse = 0,
	BaseColor = 1
};

enum eSpecularTextureKind {
	Specular = 0,
	Specular_Color = 1,
	Specular_Roughness = 2,
	Metalic = 3,
	Metalic_Roughness = 4
};

enum eAmbientTextureKind {
	Ambient = 0,
	AmbientOcculsion = 1
};

enum eEmissiveTextureKind {
	Emissive = 0,
	Emissive_Color = 1,
};

enum eBumpTextureKind {
	Normal = 0,
	Normal_Camera = 1,
	Height = 2,
	Displacement = 3
};

enum eShinenessTextureKind {
	Shineness = 0,
	Roughness = 1,
	DiffuseRoughness = 2,
};

enum eOpacityTextureKind {
	Opacity = 0,
	Transparency = 1,
};

struct MaterialCB_Data {
	vec4 baseColor;
	vec4 ambientColor;
	vec4 emissiveColor;
	float metalicFactor;
	float smoothness;
	float bumpScaling;
	float extra;
	//[64]

	float TilingX;
	float TilingY;
	float TilingOffsetX;
	float TilingOffsetY;
	//[64+16 = 80]
};

struct MaterialST_Data {
	vec4 baseColor;
	vec4 ambientColor;
	vec4 emissiveColor;

	float metalicFactor;
	float smoothness;
	float bumpScaling;
	ui32 diffuseTexId;

	ui32 normalTexId;
	ui32 AOTexId;
	ui32 roughnessTexId;
	ui32 metalicTexId;

	float TilingX;
	float TilingY;
	float TilingOffsetX;
	float TilingOffsetY;
};

struct Material {
	struct CLR {
		bool blending;
		float bumpscaling;
		vec4 ambient;
		union {
			vec4 diffuse;
			vec4 base;
		};
		vec4 emissive;
		vec4 reflective;
		vec4 specular;
		float transparent;

		CLR() {}
		CLR(const CLR& ref) {
			memcpy_s(this, sizeof(CLR), &ref, sizeof(CLR));
		}
	};
	struct TextureIndex {
		union { // 1
			int Diffuse;
			int BaseColor;
		};
		union { // 3
			int Specular; // 
			int Specular_Color; // 
			int Specular_Roughness;
			int Metalic; // 0 : non metal / 1 : metal (binary texture)
			int Metalic_Roughness; // (r:metalic, g: roughness, g:none, a:ambientocculsion)
		};
		union { // 1
			int Ambient;
			int AmbientOcculsion;
		};
		union { // 1
			int Emissive;
			int EmissionColor;
		};
		union { // 2
			int Normal; // in TBN Coord
			int Normal_inCameraCoord; // in Camera Coord
			int Height;
			int Displacement;
		};
		union { // 2
			int Shineness;
			int Roughness;
			int DiffuseRoughness;
		};
		union { // 1 
			int Opacity;
			int Transparency;
		};
		int LightMap;
		int Reflection;
		int Sheen;
		int ClearCoat;
		int Transmission;
		int Anisotropy;

		TextureIndex() {
			for (int i = 0; i < (sizeof(TextureIndex) >> 2); ++i) {
				((int*)this)[i] = -1;
			}
		}
	};
	enum GLTF_alphaMod {
		Opaque = 0,
		Blend = 1,
	};
	struct PipelineChanger {
		bool twosided = false;
		bool wireframe = false;
		unsigned short TextureSementicSelector;

		enum eTextureKind {
			Base,
			Specular_Metalic,
			Ambient,
			Emissive,
			Bump,
			Shineness,
			Opacity
		};

		void SelectTextureFlag(eTextureKind kind, int selection) {
			switch (kind) {
			case eTextureKind::Base:
				TextureSementicSelector &= 0xFFFFFFFE;
				TextureSementicSelector |= selection & 0x00000001;
				break;
			case eTextureKind::Specular_Metalic:
				TextureSementicSelector &= 0xFFFFFFF1;
				TextureSementicSelector |= (selection << 1) & 0x0000000E;
				break;
			case eTextureKind::Ambient:
				TextureSementicSelector &= 0xFFFFFFEF;
				TextureSementicSelector |= (selection << 4) & 0x00000010;
				break;
			case eTextureKind::Emissive:
				TextureSementicSelector &= 0xFFFFFFDF;
				TextureSementicSelector |= (selection << 5) & 0x00000020;
				break;
			case eTextureKind::Bump:
				TextureSementicSelector &= 0xFFFFFF3F;
				TextureSementicSelector |= (selection << 6) & 0x000000C0;
				break;
			case eTextureKind::Shineness:
				TextureSementicSelector &= 0xFFFFFCFF;
				TextureSementicSelector |= (selection << 8) & 0x00000300;
				break;
			case eTextureKind::Opacity:
				TextureSementicSelector &= 0xFFFFFBFF;
				TextureSementicSelector |= (selection << 9) & 0x00000400;
				break;
			}
		}
	};
	char name[40] = {};
	CLR clr;
	TextureIndex ti;
	PipelineChanger pipelineC; // this change pipeline of materials.

	//[1bit:base][3bit:Specular][1bit:Ambient][1bit:Emissive][2bit:Normal][2bit:Shineness][1bit:Opacity]
	//https://docs.omniverse.nvidia.com/materials-and-rendering/latest/templates/OmniPBR.html#

	float metallicFactor;
	float roughnessFactor;
	float shininess;
	float opacity;
	float specularFactor;
	float clearcoat_factor;
	float clearcoat_roughnessFactor;
	GLTF_alphaMod gltf_alphaMode;
	float gltf_alpha_cutoff;

	float TilingX;
	float TilingY;
	float TilingOffsetX;
	float TilingOffsetY;

	static constexpr int MaterialSiz_withoutGPUResource = 256;
	GPUResource CB_Resource;
	MaterialCB_Data* CBData = nullptr;
	DescIndex TextureSRVTableIndex;

	inline static GPUResource MaterialStructuredBuffer;
	inline static MaterialST_Data* MappedMaterialStructuredBuffer = nullptr;
	inline static DescIndex MaterialStructuredBufferSRV;

	Material()
	{
		ZeroMemory(this, sizeof(Material));
		memset(&ti, 0xFF, sizeof(TextureIndex));
		clr.base = vec4(1, 1, 1, 1);
		TilingX = 1;
		TilingY = 1;
		TilingOffsetX = 1;
		TilingOffsetY = 1;
	}

	~Material()
	{
	}

	Material(const Material& ref)
	{
		memcpy_s(this, MaterialSiz_withoutGPUResource, &ref, MaterialSiz_withoutGPUResource);
		CB_Resource = ref.CB_Resource;
		CBData = ref.CBData;
		TilingX = ref.TilingX;
		TilingY = ref.TilingY;
		TilingOffsetX = ref.TilingOffsetX;
		TilingOffsetY = ref.TilingOffsetY;
		TextureSRVTableIndex = ref.TextureSRVTableIndex;
	}

	__forceinline void ShiftTextureIndexs(unsigned int TextureIndexStart);
	void SetDescTable();
	MaterialCB_Data GetMatCB();
	MaterialST_Data GetMatST();

	static void InitMaterialStructuredBuffer(bool reset = false);
};

struct ModelNode {
	string name;
	XMMATRIX transform;
	ModelNode* parent;
	unsigned int numChildren;
	ModelNode** Childrens;
	unsigned int numMesh;
	unsigned int* Meshes; // array of int
	unsigned int* Mesh_Materials;
	int* Mesh_SkinMeshindex = nullptr; // 해당 메쉬가 몇번째 스킨메쉬인지에 대한 배열 Model이 스킨메쉬를 가지고, Node도 스킨메쉬를 가지면 할당됨.
	vector<BoundingBox> aabbArr;
	int* materialIndex;

	//metadata
	//???

	ModelNode() {
		parent = nullptr;
		numChildren = 0;
		Childrens = nullptr;
		numMesh = 0;
		Meshes = nullptr;
		Mesh_Materials = nullptr;
	}

	template <bool isSkinMesh = false>
	void Render(void* model, GPUCmd& cmd, const matrix& parentMat, void* pGameobject = nullptr);
	void PushRenderBatch(void* model, const matrix& parentMat, void* pGameobject = nullptr);
	void BakeAABB(void* origin, const matrix& parentMat);
};

struct animKey {
	double time;
	UINT extra[2];
	vec4 value;
};

struct AnimChannel {
	vector<animKey> posKeys;
	vector<animKey> rotKeys;
	vector<animKey> scaleKeys;

	matrix GetLocalMatrixAtTime(float time, matrix original);
};

struct HumanoidAnimation {
	static constexpr UINT HumanBoneCount = 55;
	enum HumanBodyBones
	{
		Hips = 0,
		LeftUpperLeg = 1,
		RightUpperLeg = 2,
		LeftLowerLeg = 3,
		RightLowerLeg = 4,
		LeftFoot = 5,
		RightFoot = 6,
		Spine = 7,
		Chest = 8,
		Neck = 9,
		Head = 10,
		LeftShoulder = 11,
		RightShoulder = 12,
		LeftUpperArm = 13,
		RightUpperArm = 14,
		LeftLowerArm = 15,
		RightLowerArm = 16,
		LeftHand = 17,
		RightHand = 18,
		LeftToes = 19,
		RightToes = 20,
		LeftEye = 21,
		RightEye = 22,
		Jaw = 23,
		LeftThumbProximal = 24,
		LeftThumbIntermediate = 25,
		LeftThumbDistal = 26,
		LeftIndexProximal = 27,
		LeftIndexIntermediate = 28,
		LeftIndexDistal = 29,
		LeftMiddleProximal = 30,
		LeftMiddleIntermediate = 31,
		LeftMiddleDistal = 32,
		LeftRingProximal = 33,
		LeftRingIntermediate = 34,
		LeftRingDistal = 35,
		LeftLittleProximal = 36,
		LeftLittleIntermediate = 37,
		LeftLittleDistal = 38,
		RightThumbProximal = 39,
		RightThumbIntermediate = 40,
		RightThumbDistal = 41,
		RightIndexProximal = 42,
		RightIndexIntermediate = 43,
		RightIndexDistal = 44,
		RightMiddleProximal = 45,
		RightMiddleIntermediate = 46,
		RightMiddleDistal = 47,
		RightRingProximal = 48,
		RightRingIntermediate = 49,
		RightRingDistal = 50,
		RightLittleProximal = 51,
		RightLittleIntermediate = 52,
		RightLittleDistal = 53,
		UpperChest = 54,
		LastBone = 55
	};
	inline static const char HumanBoneNames[HumanBoneCount][32] = {
	"Hips",
	"LeftUpperLeg",
	"RightUpperLeg",
	"LeftLowerLeg",
	"RightLowerLeg",
	"LeftFoot",
	"RightFoot",
	"Spine",
	"Chest",
	"Neck",
	"Head",
	"LeftShoulder",
	"RightShoulder",
	"LeftUpperArm",
	"RightUpperArm",
	"LeftLowerArm",
	"RightLowerArm",
	"LeftHand",
	"RightHand",
	"LeftToes",
	"RightToes",
	"LeftEye",
	"RightEye",
	"Jaw",
	"LeftThumbProximal",
	"LeftThumbIntermediate",
	"LeftThumbDistal",
	"LeftIndexProximal",
	"LeftIndexIntermediate",
	"LeftIndexDistal",
	"LeftMiddleProximal",
	"LeftMiddleIntermediate",
	"LeftMiddleDistal",
	"LeftRingProximal",
	"LeftRingIntermediate",
	"LeftRingDistal",
	"LeftLittleProximal",
	"LeftLittleIntermediate",
	"LeftLittleDistal",
	"RightThumbProximal",
	"RightThumbIntermediate",
	"RightThumbDistal",
	"RightIndexProximal",
	"RightIndexIntermediate",
	"RightIndexDistal",
	"RightMiddleProximal",
	"RightMiddleIntermediate",
	"RightMiddleDistal",
	"RightRingProximal",
	"RightRingIntermediate",
	"RightRingDistal",
	"RightLittleProximal",
	"RightLittleIntermediate",
	"RightLittleDistal",
	"UpperChest"
	};

	AnimChannel channels[HumanBoneCount];
	double Duration = 0;

	void LoadHumanoidAnimation(string filename);
};

struct Model {
	std::string mName;
	int nodeCount = 0;
	ModelNode* RootNode;
	vector<ModelNode*> NodeArr;
	unordered_map<void*, int> nodeindexmap;
	ModelNode* Nodes;

	unsigned int mNumMeshes;
	Mesh** mMeshes;
	unsigned int mNumSkinMesh;
	BumpSkinMesh** mBumpSkinMeshs;
	vector<BumpMesh::Vertex>* vertice = nullptr;
	vector<TriangleIndex>* indice = nullptr;

	matrix* DefaultNodelocalTr = nullptr;
	matrix* NodeOffsetMatrixArr = nullptr;

	unsigned int mNumTextures;
	//GPUResource** mTextures;
	
	unsigned int mNumMaterials;
	//Material** mMaterials;

	unsigned int TextureTableStart = 0;
	unsigned int MaterialTableStart = 0;

	/*unsigned int mNumAnimations;
	Animation** mAnimations;

	unsigned int mNumSkeletons;
	_Skeleton** mSkeletons;*/

	//metadata
	float UnitScaleFactor = 1.0f;

	//void Rearrange1(ModelNode* node);
	//void Rearrange2();
	//void GetModelFromAssimp(string filename, float posmul = 0);
	//Mesh* Assimp_ReadMesh(aiMesh* mesh, const aiScene* scene, int meshindex);
	//Material* Assimp_ReadMaterial(aiMaterial* mat, const aiScene* scene);
	//Animation* Assimp_ReadAnimation(aiAnimation* anim, const aiScene* scene);

	vec4 AABB[2];
	vec4 OBB_Tr;
	vec4 OBB_Ext;

	// nodeCount 만큼의 int 배열. 노드의 인덱스를 넣으면 휴머노이드채널인덱스가 나옴.
	int* Humanoid_retargeting = nullptr;
	//void SaveModelFile(string filename);
	void LoadModelFile(string filename);
	void LoadModelFile2(string filename);
	void Retargeting_Humanoid();
	
	void BakeAABB();
	BoundingOrientedBox GetOBB();

	template <bool isSkinMesh = false>
	void Render(GPUCmd& cmd, matrix worldMatrix, void* pGameobject = nullptr);

	void PushRenderBatch(matrix worldMatrix, void* pGameobject = nullptr);
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

/*
* 설명 : Shape은 Mesh와 Model을 포함할 수 있는 모양을 나타낸 구조체.
* highest bit == 1 -> Mesh
* else -> Model
*
* Sentinal Value :
* NULL : (FlagPtr = 0);
* isMesh : (FlagPtr & 0x8000000000000000);
* isModel : !isMesh;
*/
struct Shape {
	ui64 FlagPtr = 0;

	Shape() : FlagPtr{0} {}
	Shape(Mesh* mesh) {
		SetMesh(mesh);
	}
	Shape(Model* model) {
		SetModel(model);
	}

	/*
	* 설명/반환 : Shape가 Mesh인지 여부를 반환
	*/
	__forceinline bool isMesh() {
		return FlagPtr & 0x8000000000000000;
	}

	/*
	* 설명/반환 : Shape가 가진 Mesh 포인터를 반환
	*/
	__forceinline Mesh* GetMesh() {
		if (isMesh()) {
			return reinterpret_cast<Mesh*>(FlagPtr & 0x7FFFFFFFFFFFFFFF);
		}
		else return nullptr;
	}

	/*
	* 설명 : Shape에 Mesh를 넣는다.
	*/
	__forceinline void SetMesh(Mesh* ptr) {
		FlagPtr = reinterpret_cast<ui64>(ptr);
		FlagPtr |= 0x8000000000000000;
	}

	/*
	* 설명/반환 : Shape가 가진 Model 포인터를 반환
	*/
	__forceinline Model* GetModel() {
		if (isMesh()) return nullptr;
		else {
			return reinterpret_cast<Model*>(FlagPtr);
		}
	}

	/*
	* 설명 : Shape에 Model를 넣는다.
	*/
	__forceinline void SetModel(Model* ptr) {
		FlagPtr = reinterpret_cast<ui64>(ptr);
	}

	/*
	* 설명/반환 : Shape의 이름을 받아서 해당 Shape의 인덱스를 반환한다.
	*/
	static int GetShapeIndex(string meshName);
	/*
	* 설명/반환 : Shape의 이름을 받아서 ShapeNameArr에 저장후 해당 Shape의 인덱스를 반환한다.
	*/
	static int AddShapeName(string meshName);

	// Shape의 이름 배열
	inline static vector<string> ShapeNameArr;
	// 이름에서 Shape를 얻는 map
	inline static unordered_map<string, Shape> StrToShapeMap;
	// 인덱스에서 Shape를 얻는 map
	inline static unordered_map<int, Shape> IndexToShapeMap;

	/*
	* 설명 : Mesh의 이름과 Mesh 포인터를 받아 Mesh를 추가하는 함수
	* 매개변수 :
	* string name : Mesh의 이름
	* Mesh* ptr : Mesh의 포인터
	*/
	static int AddMesh(string name, Mesh* ptr);

	/*
	* 설명 : Model의 이름과 Model 포인터를 받아 Model를 추가하는 함수
	* 매개변수 :
	* string name : Model의 이름
	* Model* ptr : Model의 포인터
	*/
	static int AddModel(string name, Model* ptr);

	void GetRealShape(Mesh*& out0, Model*& out1) {
		if(isMesh()) out0 = reinterpret_cast<Mesh*>(FlagPtr & 0x7FFFFFFFFFFFFFFF);
		else out1 = reinterpret_cast<Model*>(FlagPtr & 0x7FFFFFFFFFFFFFFF);
	}
};

union Tag {
	UINT tag = 0;
	operator UINT() { return tag; }
	operator bool() { return tag; }

	Tag() {}
	Tag(UINT n) : tag{n} {}

	struct TagSetter {
		Tag* t;
		int index;

		operator bool() { return t->tag; }

		void operator=(bool b) {
			if (b) {
				t->tag |= index;
			}
			else {
				t->tag &= ~index;
			}
		}
	};

	// bool로도 쓸 수 있음.
	TagSetter& operator[](UINT MaskIndex) {
		TagSetter ts;
		ts.t = this;
		ts.index = MaskIndex;
		return ts;
	}
};

struct GameObject {
	inline static void* StaticVptr = nullptr;
	inline static void* DynamicVptr = nullptr;
	inline static void* SkinMeshVptr = nullptr;
	template <typename T> inline static void* Vptr = nullptr;
	static void StaticInit();
	
	template <typename T> __forceinline static bool IsType(GameObject* go);

	/////////////////
	// 청크를 통해 한 틱에 게임오브젝트 당 한번씩 해야하는 작업이 있다면 이 값을 사용하자.
	UINT TourID = 0;

	/*
	* 게임오브젝트를 구분하고 탐색하기 위한 tag. 32개의 tag를 보유할 수 있다.
	* 항상 tag의 첫번째 비트는 enable이다. (게임오브젝트가 활성화 되어있는지 여부)
	*/
	Tag tag;
	enum GameObjectTag {
		Tag_Enable = 1, // 게임오브젝트 활성화 여부
		Tag_Dynamic = 2, // 게임오브젝트가 움직일 수 있는지 여부
	};

	// appearance
	Shape shape;
	union {
		int* material = nullptr; // mesh 일 경우에만 활성화됨. slotNum만큼. game.MaterialTable에서 접근.
		matrix* transforms_innerModel; // model 일 경우만 활성화됨. nodeCount 만큼.
	};
	
	// 계층구조
	GameObject* parent = nullptr;
	GameObject* childs = nullptr;
	GameObject* sibling = nullptr;
	union {
		char extra[8];
		float** RaytracingWorldMatInput = nullptr;
		float*** RaytracingWorldMatInput_Model;
	};
	
	// transform
	matrix worldMat;

	GameObject();
	virtual ~GameObject();

	virtual matrix GetWorld();
	virtual void SetWorld(matrix localWorldMat);

	// render instance를 이용한 배치처리를 하지 않는 순수한 렌더링시에 사용
	virtual void Render(matrix parent = XMMatrixIdentity());
	virtual void PushRenderBatch(matrix parent = XMMatrixIdentity());
	// 현재 사용하는 렌더함수를 가리킴.
	using RenderFuncType = void (GameObject::*)(matrix parent);
	inline static RenderFuncType CurrentRenderFunc = &GameObject::Render;

	virtual void Release();
	virtual BoundingOrientedBox GetOBB();

	virtual void SetShape(Shape _shape);
	virtual void SetShape(Model* _shape);
	virtual void SetShape(Mesh* _shape);

	virtual void OnRayHit(GameObject* rayFrom);

	void RaytracingUpdateTransform();
	void RaytracingUpdateTransform(Model* model, ModelNode* node, matrix parent);

	void DbgHieraky();
};

struct StaticGameObject : public GameObject {
	StaticGameObject();
	virtual ~StaticGameObject();
	vector<BoundingBox> aabbArr;

	virtual matrix GetWorld();
	virtual void SetWorld(matrix localWorldMat);

	virtual void Render(matrix parent = XMMatrixIdentity());
	virtual void PushRenderBatch(matrix parent = XMMatrixIdentity());
	// 현재 사용하는 렌더함수를 가리킴.
	using RenderFuncType = void (StaticGameObject::*)(matrix parent);
	inline static RenderFuncType CurrentRenderFunc = &StaticGameObject::Render;

	virtual void Release();
	virtual BoundingOrientedBox GetOBB();

	bool Collision_Inherit(matrix parent_world, BoundingBox bb);
	void InitMapAABB_Inherit(void* origin, matrix parent_world);
	BoundingOrientedBox GetOBBw(matrix worldMat);
};

struct GameObjectIncludeChunks {
	int xmin;
	int ymin;
	int zmin;
	unsigned char xlen;
	unsigned char ylen;
	unsigned char zlen;
	unsigned char extraByte;
};

struct DynamicGameObject : public GameObject {
	DynamicGameObject();
	virtual ~DynamicGameObject();

	virtual matrix GetWorld();
	virtual void SetWorld(matrix localWorldMat);

	vec4 LVelocity;
	vec4 tickLVelocity;
	vec4 tickAVelocity;
	vec4 LastQuerternion;

	GameObjectIncludeChunks IncludeChunks;
	int* chunkAllocIndexs = nullptr;
	int chunkAllocIndexsCapacity = 8;
	
	void InitialChunkSetting();
	void Move(vec4 velocity, vec4 Q);
	void Move(vec4 velocity, vec4 Q, GameObjectIncludeChunks afterChunkInc);
	virtual void Update(float delatTime);
	virtual void Render(matrix parent = XMMatrixIdentity());
	virtual void PushRenderBatch(matrix parent = XMMatrixIdentity());
	using RenderFuncType = void (DynamicGameObject::*)(matrix parent);
	inline static RenderFuncType CurrentRenderFunc = &DynamicGameObject::Render;

	virtual void Event(WinEvent evt);
	virtual void Release();
	virtual BoundingOrientedBox GetOBB();

	void LookAt(vec4 look, vec4 up = { 0, 1, 0, 0 });

	static void CollisionMove(DynamicGameObject* obj1, DynamicGameObject* obj2);
	virtual void OnCollision(GameObject* other);
	virtual void OnRayHit(GameObject* rayFrom);

	virtual void SetShape(Shape _shape);
};

//skinMesh 의 경우 shader 자체가 다르기 때문에 배치처리를 할 시에 따로 렌더링을 수행해야 한다.
struct SkinMeshGameObject : public DynamicGameObject {
	SkinMeshGameObject();
	virtual ~SkinMeshGameObject();

	// temp data. later combine one upload CBV
	vector<matrix*> RootBoneMatrixs_PerSkinMesh;
	// [bone 0] [...] [bone N]
	vector<GPUResource> BoneToWorldMatrixCB;
	vector<HumanoidAnimation*> HumanoidAnimationArr;
	float AnimationFlowTime = 0;

	// non shader visible desc heap에 위치함. 
	// model->mNumSkinMesh 만큼 존재함.
	DescIndex* OutVertexUAV; 

	// 레이트레이싱을 할때 실제로 변형되는 메쉬.
	// model의 skinmeshcount 만큼 존재함.
	RayTracingMesh* modifyMeshes = nullptr;

	void InitRootBoneMatrixs();
	void SetRootMatrixs();

	void PushHumanoidAnimation(HumanoidAnimation* hanim);
	void GetBoneLocalMatrixAtTime(HumanoidAnimation* hanim, matrix* out, float time);

	virtual void Render(matrix parent = XMMatrixIdentity());
	virtual void PushRenderBatch(matrix parent = XMMatrixIdentity());
	void ModifyVertexs(matrix parent = XMMatrixIdentity());

	using RenderFuncType = void (SkinMeshGameObject::*)(matrix parent);
	inline static RenderFuncType CurrentRenderFunc = &SkinMeshGameObject::Render;
	
	virtual void SetShape(Shape _shape);

	virtual void Update(float delatTime);
};

enum LightType {
	LT_SpotLight = 0,
	LT_DirectionLight = 1,
	LT_PointLight = 2,
};

struct Light {
	matrix transform;
	LightType lightType;
	float spot_angle;
	float range;
	float intencity;
	void* data;

	void GenerateLight();
};

class Player : public DynamicGameObject {
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

	virtual void Render(matrix parent = XMMatrixIdentity());
	void Render_AfterDepthClear();

	virtual void Event(WinEvent evt);

	virtual void OnCollision(GameObject* other) override;
	
	virtual void Release();

	virtual BoundingOrientedBox GetOBB();

	virtual void OnRayHit(GameObject* rayFrom);
};

class Monster : public DynamicGameObject {
public:
	bool is_dead = false;
	vec4 rand_direction;
	float ChangeDirflow = 0;
	float Speed = 3;
	vec4 Exp_LVelocity[3];

	virtual void Update(float deltaTime) override;

	virtual void Render(matrix parent = XMMatrixIdentity());

	virtual void Release();

	virtual BoundingOrientedBox GetOBB();

	virtual void OnRayHit(GameObject* rayFrom);
};

class TerrainObject : public DynamicGameObject {
public:
	BumpMesh* TerrainMeshs = nullptr;
	Material TerrainMaterial;
	int XWid=0, ZWid = 0;
	float bumpScale = 0;
	float Unit = 0;
	byte8* HeightArr;
	static constexpr int VertexWid = 128;
	int xVertexWidth = 0;
	int zVertexWidth = 0;
	float CenterX = 0;
	float CenterZ = 0;
	float CenterY = 0;
	int BaseMaterialIndex;
	Material SelectionMaterial[4];
	GPUResource ColorPicker;
	UVMesh* GrassPointMesh;
	bool isTesslation = true;
	GPUResource HeightMap;
	// [base tex, tex1, tex2, tex3, colorPickertex]

	void LoadTerain(const char* filenae, float bumpS, float _Unit, vec4 Center);
	static Material LoadTerrainMaterial(const wchar_t* TexFileColor, const wchar_t* TexFileNormal, const wchar_t* TexFileAO, const wchar_t* TexFileRoughness, const wchar_t* TexFileDisplacement, float Tiling);

	void LoadTessTerrain(const wchar_t* filenae, float EdgeLen, int div, vec4 Center);

	TerrainObject();
	virtual ~TerrainObject();

	virtual void Render();
	virtual void RenderShadowMap();
	virtual void Release();

	BoundingOrientedBox GetOBB_TerrainMesh(int x, int z);
	bool isCollision(GameObject* other);
	virtual void OnCollision(GameObject* other);

	__forceinline float GetHeight_FromIndex(int x, int z) {
		static float zero = 0;
		if (x < 0 || x >= xVertexWidth || z < 0 || z >= zVertexWidth) return zero;
		float f = (float)(HeightArr[x * zVertexWidth + z]);
		return CenterY + bumpScale * (f / 256.0f);
	}

	float GetHeight(float x, float z) {
		float cx = (x - CenterX) + (float)xVertexWidth/2;
		float cz = (z - CenterZ) + (float)zVertexWidth/2;

		int xindex = (cx / Unit);
		int zindex = (cz / Unit);

		float ux = (cx - xindex * Unit);
		float uz = (cz - zindex * Unit);

		float ux2 = ux * ux;
		float uz2 = uz * uz;
		float tempx = (Unit - ux);
		tempx *= tempx;
		float tempz = (Unit - uz);
		tempz *= tempz;
		if (ux + uz < Unit) {
			XMVECTOR p1 = XMVectorSet(0, GetHeight_FromIndex(xindex, zindex), 0, 0);
			XMVECTOR p2 = XMVectorSet(Unit, GetHeight_FromIndex(xindex + 1, zindex), 0, 0) - p1;
			XMVECTOR p3 = XMVectorSet(0, GetHeight_FromIndex(xindex, zindex + 1), Unit, 0) - p1;
			XMVECTOR normal = XMVector3Cross(p3, p2);
			normal = XMVector3Normalize(normal);
			XMFLOAT3 nor; XMStoreFloat3(&nor, normal);
			XMFLOAT3 ap; XMStoreFloat3(&ap, p1);
			float height = (-1.0f * (nor.x * ux + nor.z * uz) / nor.y) + ap.y;
			return height - CenterY;

			/*float h1 = GetHeight_FromIndex(xindex, zindex);
			float h1r = sqrtf(ux2 + uz2);
			float h2 = GetHeight_FromIndex(xindex + 1, zindex);
			float h2r = sqrtf(tempx + uz2);
			float h3 = GetHeight_FromIndex(xindex, zindex + 1);
			float h3r = sqrtf(ux2 + tempz);

			return CenterY + bumpScale * (h1 * h1r + h2 * h2r + h3 * h3r) / (h1r + h2r + h3r);*/
		}
		else {
			XMVECTOR p2 = XMVectorSet(Unit, GetHeight_FromIndex(xindex + 1, zindex + 1), Unit, 0);
			XMVECTOR p1 = XMVectorSet(Unit, GetHeight_FromIndex(xindex + 1, zindex), 0, 0) - p2;
			XMVECTOR p3 = XMVectorSet(0, GetHeight_FromIndex(xindex, zindex + 1), Unit, 0) - p2;
			XMVECTOR normal = XMVector3Cross(p1, p3);
			normal = XMVector3Normalize(normal);
			XMFLOAT3 nor; XMStoreFloat3(&nor, normal);
			XMFLOAT3 ap; XMStoreFloat3(&ap, p2);
			float height = (-1.0f * (nor.x * (ux - Unit) + nor.z * (uz - Unit)) / nor.y) + ap.y;
			return height - CenterY;

			/*float h1 = GetHeight_FromIndex(xindex, zindex + 1);
			float h1r = sqrtf(ux2 + tempz);
			float h2 = GetHeight_FromIndex(xindex + 1, zindex);
			float h2r = sqrtf(tempx + uz2);
			float h3 = GetHeight_FromIndex(xindex + 1, zindex + 1);
			float h3r = sqrtf(tempx + tempz);

			return CenterY + bumpScale * (h1 * h1r + h2 * h2r + h3 * h3r) / (h1r + h2r + h3r);*/
		}
	}

	XMVECTOR GetNormal(float x, float z)
	{
		float cx = (x + CenterX);
		float cz = (z + CenterZ);

		int xindex = (cx / Unit);
		int zindex = (cz / Unit);

		float ux = (cx - xindex * Unit);
		float uz = (cz - zindex * Unit);

		float ux2 = ux * ux;
		float uz2 = uz * uz;
		float tempx = (Unit - ux);
		tempx *= tempx;
		float tempz = (Unit - uz);
		tempz *= tempz;
		if (ux + uz < Unit) {
			XMVECTOR p1 = XMVectorSet(0, GetHeight_FromIndex(xindex, zindex), 0, 0);
			XMVECTOR p2 = XMVectorSet(Unit, GetHeight_FromIndex(xindex + 1, zindex), 0, 0) - p1;
			XMVECTOR p3 = XMVectorSet(0, GetHeight_FromIndex(xindex, zindex + 1), Unit, 0) - p1;
			XMVECTOR normal = XMVector3Cross(p3, p2);
			normal = XMVector3Normalize(normal);
			return normal;
		}
		else {
			XMVECTOR p2 = XMVectorSet(Unit, GetHeight_FromIndex(xindex + 1, zindex + 1), Unit, 0);
			XMVECTOR p1 = XMVectorSet(Unit, GetHeight_FromIndex(xindex + 1, zindex), 0, 0) - p2;
			XMVECTOR p3 = XMVectorSet(0, GetHeight_FromIndex(xindex, zindex + 1), Unit, 0) - p2;
			XMVECTOR normal = XMVector3Cross(p1, p3);
			normal = XMVector3Normalize(normal);
			return normal;
		}
	}
};

struct DirectionLight
{
	XMFLOAT3 gLightDirection; // Light Direction
	float pad0;
	XMFLOAT3 gLightColor; // Light_Color
	float pad1;
};

struct PointLightCBData
{
	XMFLOAT3 LightPos;
	float LightIntencity;
	XMFLOAT3 LightColor; // Light_Color
	float LightRange;

	PointLightCBData() {}

	PointLightCBData(XMFLOAT3 pos, float intencity, XMFLOAT3 color, float range) 
		: LightPos{ pos }, LightIntencity{ intencity }, LightColor{color}, 
		LightRange{ range }
	{

	}
};

struct PointLight {
	inline static GPUResource UploadCBBuffer;
	static constexpr UINT CBCapacity = 8192;
	inline static UINT CBSize = 0;
	inline static PointLightCBData* UploadCBMapped = nullptr;
	UINT CBIndex = 0;

	// 움직이지 않는 Static 오브젝트들을 미리 그려놓는 DSV CubeMap
	GPUResource StaticShadowCubeMap;
	DescHandle StaticCubeShadowMapHandleSRV;
	D3D12_CPU_DESCRIPTOR_HANDLE StaticCubeShadowMapHandleDSV[6];
	// 움직일 Dynamic 오브젝트들을 실시간으로 그리는 DSV CubeMap.
	GPUResource DynamicShadowCubeMap;
	DescHandle DynamicCubeShadowMapHandleSRV;
	D3D12_CPU_DESCRIPTOR_HANDLE DynamicCubeShadowMapHandleDSV[6];

	ViewportData FaceViewPort[6];
	int Resolution = 512;

	void CreatePointLight(PointLightCBData init, UINT resolution = 512) {
		if (UploadCBMapped == nullptr) {
			UploadCBBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(PointLightCBData) * 8192, 1);
			UploadCBBuffer.resource->Map(0, NULL, (void**)&UploadCBMapped);
		}
		CBIndex = CBSize;
		UploadCBMapped[CBIndex] = init;
		// static shadow cube map
		StaticShadowCubeMap = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_DIMENSION_TEXTURE2D, resolution, resolution, DXGI_FORMAT_D32_FLOAT, 6, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

		int i = gd.TextureDescriptorAllotter.Alloc();
		StaticCubeShadowMapHandleSRV.hcpu = gd.TextureDescriptorAllotter.GetCPUHandle(i);
		StaticCubeShadowMapHandleSRV.hgpu = gd.TextureDescriptorAllotter.GetGPUHandle(i);

		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv_desc.Texture2D.MostDetailedMip = 0;
		srv_desc.Texture2D.MipLevels = 1;
		srv_desc.Texture2D.ResourceMinLODClamp = 0;
		srv_desc.Texture2D.PlaneSlice = 0;
		gd.pDevice->CreateShaderResourceView(StaticShadowCubeMap.resource, &srv_desc, StaticCubeShadowMapHandleSRV.hcpu);

		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = gd.pDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		dsvHandle.ptr += gd.GetPointLightShadowDSVIndex(CBIndex) * gd.DSVSize;
		for (int i = 0; i < 6; ++i)
		{
			StaticCubeShadowMapHandleDSV[i] = dsvHandle;
			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
			dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			dsvDesc.Texture2DArray.MipSlice = 0;
			dsvDesc.Texture2DArray.FirstArraySlice = i; // 큐브맵의 특정 면
			dsvDesc.Texture2DArray.ArraySize = 1;

			gd.pDevice->CreateDepthStencilView(StaticShadowCubeMap.resource, &dsvDesc, StaticCubeShadowMapHandleDSV[i]);

			dsvHandle.ptr += gd.DSVSize;
		}

		//// dynamic shadow cube map
		//DynamicShadowCubeMap = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_DIMENSION_TEXTURE2D, resolution, resolution, DXGI_FORMAT_R32_FLOAT, 6, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
		//int i = gd.TextureDescriptorAllotter.Alloc();
		//DynamicCubeShadowMapHandleSRV.hcpu = gd.TextureDescriptorAllotter.GetCPUHandle(i);
		//DynamicCubeShadowMapHandleSRV.hgpu = gd.TextureDescriptorAllotter.GetGPUHandle(i);
		//D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		//srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
		//srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		//srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		//srv_desc.Texture2D.MostDetailedMip = 0;
		//srv_desc.Texture2D.MipLevels = 1;
		//srv_desc.Texture2D.ResourceMinLODClamp = 0;
		//srv_desc.Texture2D.PlaneSlice = 0;
		//gd.pDevice->CreateShaderResourceView(DynamicShadowCubeMap.resource, &srv_desc, DynamicCubeShadowMapHandleSRV.hcpu);

		//D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = gd.pDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		//dsvHandle.ptr += gd.GetPointLightShadowDSVIndex(CBIndex) * gd.DSVSize;
		//for (int i = 0; i < 6; ++i)
		//{
		//	DynamicCubeShadowMapHandleDSV[i] = dsvHandle;
		//	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		//	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		//	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
		//	dsvDesc.Texture2DArray.MipSlice = 0;
		//	dsvDesc.Texture2DArray.FirstArraySlice = i; // 큐브맵의 특정 면
		//	dsvDesc.Texture2DArray.ArraySize = 1;

		//	gd.pDevice->CreateDepthStencilView(DynamicShadowCubeMap.resource, &dsvDesc, DynamicCubeShadowMapHandleDSV[i]);

		//	dsvHandle.ptr += gd.DSVSize;
		//}

		CBSize += 1;

		vec4 eye = init.LightPos;
		matrix outProj = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, 0.1f, 1000.0f);
		matrix outView[6] = {};
		outView[0] = XMMatrixLookAtLH(eye, eye + XMVectorSet(1, 0, 0, 0), XMVectorSet(0, 1, 0, 0)); // +X
		outView[1] = XMMatrixLookAtLH(eye, eye + XMVectorSet(-1, 0, 0, 0), XMVectorSet(0, 1, 0, 0)); // -X
		outView[2] = XMMatrixLookAtLH(eye, eye + XMVectorSet(0, 1, 0, 0), XMVectorSet(0, 0, -1, 0)); // +Y (Up 벡터 주의)
		outView[3] = XMMatrixLookAtLH(eye, eye + XMVectorSet(0, -1, 0, 0), XMVectorSet(0, 0, 1, 0)); // -Y
		outView[4] = XMMatrixLookAtLH(eye, eye + XMVectorSet(0, 0, 1, 0), XMVectorSet(0, 1, 0, 0)); // +Z
		outView[5] = XMMatrixLookAtLH(eye, eye + XMVectorSet(0, 0, -1, 0), XMVectorSet(0, 1, 0, 0)); // -Z
		Resolution = resolution;
		for (int i = 0; i < 6; ++i) {
			FaceViewPort[i].ViewMatrix = outView[i];
			FaceViewPort[i].ProjectMatrix = outProj;
			FaceViewPort[i].Camera_Pos = init.LightPos;
			FaceViewPort[i].Viewport.Width = Resolution;
			FaceViewPort[i].Viewport.Height = Resolution;
			FaceViewPort[i].Viewport.MaxDepth = 1.0f;
			FaceViewPort[i].Viewport.MinDepth = 0.0f;
			FaceViewPort[i].Viewport.TopLeftX = 0.0f;
			FaceViewPort[i].Viewport.TopLeftY = 0.0f;
			FaceViewPort[i].ScissorRect = { 0, 0, (long)Resolution, (long)Resolution };
		}
	}
	
	void RenderShadow();
};

struct LightCB_DATA {
	DirectionLight dirlight;
	PointLightCBData pointLights[8];
};

struct LightCB_DATA_withShadow {
	DirectionLight dirlight;
	PointLightCBData pointLights[8];
	XMMATRIX LightProjection;
	XMMATRIX LightView;
	XMFLOAT3 LightPos;
};

//class Hierarchy_Object : public StaticGameObject {
//public:
//	int childCount = 0;
//	vector<Hierarchy_Object*> childs;
//	int Mesh_materialIndex = 0;
//	
//	vec4 AA, BB; // min, max xyz
//
//	Hierarchy_Object(){
//		childCount = 0;
//		Mesh_materialIndex = 0;
//	}
//	~Hierarchy_Object(){}
//
//	void Render_Inherit(matrix parent_world, ShaderType sre = ShaderType::RenderWithShadow);
//	bool Collision_Inherit(matrix parent_world, BoundingBox bb);
//	void InitMapAABB_Inherit(void* origin, matrix parent_world);
//	BoundingOrientedBox GetOBBw(matrix worldMat);
//};

/*
* 설명 : 청크를 찾아가기 위한 인덱스
* Sentinal Value :
* NULL = (x == -2,147,483,648 || y == -2,147,483,648 || z == -2,147,483,648)
*/
struct ChunkIndex {
	int x = 0;
	int y = 0;
	int z = 0;

	ChunkIndex() {}
	~ChunkIndex() {}

	ChunkIndex(int X, int Y, int Z) {
		x = X;
		y = Y;
		z = Z;
	}
	ChunkIndex(const ChunkIndex& ref) {
		x = ref.x;
		y = ref.y;
		z = ref.z;
	}

	bool operator==(const ChunkIndex& ci) const {
		return (x == ci.x && y == ci.y) && z == ci.z;
	}

	BoundingBox GetAABB();
};

/*
* 설명 : 청크.
* 여러개의 OBB를 소유하고 있다.
* Sentinal Value :
* NULL = (obbs.size() == 0)
*/
struct GameChunk {
	vecset<StaticGameObject*> Static_gameobjects;
	vecset<DynamicGameObject*> Dynamic_gameobjects;
	vecset<SkinMeshGameObject*> SkinMesh_gameobjects;

	ChunkIndex cindex;
	BoundingBox AABB;
	UINT TourID = 0;

	GameChunk() {
		Static_gameobjects.Init(32);
		Dynamic_gameobjects.Init(32);
		SkinMesh_gameobjects.Init(32);
	}
	void SetChunkIndex(ChunkIndex ci);

	void RenderChunkDbg();
};

/*
* 설명 : ChunckIndex의 해쉬
* x, y, z의 각 비트를 돌아가면서 적제하는 방식.
* pdep를 이용해 그것을 빠르게 구현한다.
*/
template<>
struct hash<ChunkIndex> {
	size_t operator()(const ChunkIndex& p) const noexcept {
		size_t h = _pdep_u64((size_t)p.x, 0x9249249249249249);
		h |= _pdep_u64((size_t)p.y, 0x2492492492492492);
		h |= _pdep_u64((size_t)p.z, 0x4924924924924924);
		return h;
	}
};

struct GameMap {
	GameMap(){}
	~GameMap(){}
	vector<string> name;
	vector<Mesh*> meshes;
	vector<Model*> models;
	vector<StaticGameObject*> MapObjects;
	vec4 AABB[2] = {0, 0};
	void ExtendMapAABB(BoundingOrientedBox obb);
	
	/*ui64 pdep_src2[48] = {};
	int pdepcnt = 0;
	ui64 GetSpaceHash(int x, int y, int z);*/

	struct Posindex {
		int x;
		int y;
		int z;
	};
	/*ui64 inv_pdep_src2[48] = {};
	int inv_pdepcnt = 0;
	Posindex GetInvSpaceHash(ui64 hash);*/

	RangeArr<ui32, bool> IsCollision;

	//static collision..
	void BakeStaticCollision();

	unsigned int TextureTableStart = 0;
	unsigned int MaterialTableStart = 0;

	void LoadMap(const char* MapName);
};

struct DirLightInfo {
	matrix DirLightProjection;
	matrix DirLightView;
	vec4 DirLightPos;
	vec4 DirLightDir;
	vec4 DirLightColor;
};

class Game {
public:
	//----------쉐이더----------
	Shader* MyShader;
	OnlyColorShader* MyOnlyColorShader;
	DiffuseTextureShader* MyDiffuseTextureShader;
	ScreenCharactorShader* MyScreenCharactorShader;
	PBRShader1* MyPBRShader1;
	SkyBoxShader* MySkyBoxShader;
	GSBilboardShader* MyGrassShader;
	ParticleShader* MyParticleShader;
	TesselationTestShader* MyTesselationTestShader;
	ComputeTestShader* MyComputeTestShader;
	//RayTracingShader
	RayTracingShader* MyRayTracingShader;

	Material DefaultMaterial;
	BumpMesh* TestMirrorMesh = nullptr;
	vec4 mirrorPlane;
	BumpMesh* TestCharMesh = nullptr;
	UVMesh* TextMesh;
	//ShadowMappingShader* MyShadowMappingShader;
	SpotLight MyDirLight;
	PointLight MyPointLight;

	GPUResource DirLightRes;
	DirLightInfo* MappedDirLightData = nullptr;
	DescIndex DirLightResCBV;
	void InitDirLightGPURes();

	GPUResource Guntex;
	GPUResource DefaultTex;
	GPUResource DefaultNoramlTex;
	GPUResource DefaultAmbientTex;
	GPUResource DefaultMirrorTex;
	GPUResource DefaultGrass;
	GPUResource DefaultExplosionParticle;
	GPUResource MiniMapBackground;

	vector<GPUResource*> TextureTable;
	vector<Material> MaterialTable;
	vector<Mesh*> MeshTable;
	vector<HumanoidAnimation> HumanoidAnimationTable;
	
	void AddMesh(Mesh* mesh);

	static constexpr int basicTexMip = 10;
	static constexpr DXGI_FORMAT basicTexFormat = DXGI_FORMAT_BC3_UNORM;
	static constexpr char basicTexFormatStr[] = "BC3_UNORM";

	Mesh* MytestTesselationMesh;

	//---------- GameObjects----------
	GameMap* Map;
	//std::vector<DynamicGameObject*> m_gameObjects; // GameObjets
	Player* player;
	vecset<BulletRay> bulletRays;
	//하나의 청크의 정육면체의 한 변의 길이를 결정한다.
	static constexpr float chunck_divide_Width = 50.0f;
	//chunkIndex로 StaticCollision을 위한 청크를 찾기 위한 Map
	unordered_map<ChunkIndex, GameChunk*> chunck;
	
	UINT PresentChunkSeekDepth = 0;
	vector<GameChunk*> SameDepthChunkArr[2];

	GPUResource LightCBResource;
	LightCB_DATA* LightCBData;

	GPUResource LightCB_withShadowResource;
	LightCB_DATA_withShadow* LightCBData_withShadow;
	void SetLight();

	TerrainObject* terrain;

	int KillCount = 0;
	float KillFlow = 1;

	float MiniMapZoomRate = 10.0f;
	int monsterCount = 20;

	// 거울 (나중에 구조체를 따로 만들어야 함.)
	vec4 MirrorPoses[16];

	// 청크에 게임오브젝트를 넣는다.
	GameObjectIncludeChunks GetChunks_Include_OBB(BoundingOrientedBox obb);
	GameChunk* GetChunkFromPos(vec4 pos);
	void PushGameObject(GameObject* go);
	
	void PushLight(Light* light);

	//-----------dynamic Global-----------
	
	// 현재 렌더링을 수행하는 viewport
	inline static ViewportData* renderViewPort;
	// 현재 렌더링을 수행하는 셰이더 타입
	inline static ShaderType PresentShaderType = ShaderType::RenderNormal;
	// 1인칭 여부
	bool bFirstPersonVision = false;
	// delta time
	float DeltaTime;

	UINT TourID = 0;
	// 렌더링을 할때 배치처리 렌더링 사용 여부
	bool SceneRenderBatch = false;
	// 렌더커맨드를 삽입할때 오브젝트의 렌더링 함수를 교체하는 bool 변수
	bool CurrentRenderBatch = false;
	void SetRenderMod(bool isbatch);
	void ClearAllMeshInstancing();
	
	template <bool isSkinMesh = false>
	void RenderTour();
	void BatchRender(ID3D12GraphicsCommandList* cmd);

	//----------inputs----------
	bool isMouseReturn;
	vec4 DeltaMousePos;
	vec4 LastMousePos;
	UCHAR pKeyBuffer[256];

	//------------temp---------
	/*StreamOutputPoints MyParticle;
	vec4 MirrorPoses[16] = {};*/

	Game(){}
	~Game() {}
	void Init();
	void Render();
	void Render_ShadowPass();
	void Render_RayTracing();
	void Update();
	void Event(WinEvent evt);

	// why I make Release function seperatly? : it is multi useable form.
	// 1. when I want show object release on code lines or if object is global, just call Release();
	// 2. when I want convinient of Destructor ~Class(), just put Release function on it.
	void Release();

	void FireRaycast(GameObject* owner, vec4 rayStart, vec4 rayDirection, float rayDistance);

	void RenderText(const wchar_t* wstr, int length, vec4 Rect, float fontsiz, float depth = 0.01f);

	void RenderSDFText(const wchar_t* wstr, int length, vec4 Rect, float fontsiz, vec4 color, float* minD, float* maxD, float depth = 0.01f);

	void bmpTodds(int mipmap_level, const char* Format, const char* filename);
};
Game game;

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);


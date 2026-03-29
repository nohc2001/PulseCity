#pragma once
#include "stdafx.h"
#include "ttfParser.h"
#include "SpaceMath.h"

using namespace TTFFontParser;

/*
* 설명 :
* GPU에 올렸거나 올릴 리소스의 디스크립터 핸들을 저장한다.
* Sentinel Value
* NULL = (hcpu == 0 && hgpu == 0)
*/
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

struct OBB_vertexVector {
	vec4 vertex[2][2][2] = { {{}} };
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

/*
* 설명 : 같은 종류의 셰이더에서 다르게 렌더링을 하려하기 때문에,
* 어떤 렌더링을 사용할 것인지 선택할 수 있게 하는 enum.
*/
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
	}
};

/*
* 설명 : 해상도를 나타내는 구조체
*/
struct ResolutionStruct {
	ui32 width;
	ui32 height;
};

/*
* 설명 : 렌더링을 어떤 방식으로 진행할 건지 선택가능한 enum
*/
enum RenderingMod {
	ForwardRendering = 0,
	DeferedRendering = 1,
	WithRayTracing = 2,
};

/*
* 설명 : 반영구적으로 보존되어 특정 상황마다 새로 만들 수 있고, 또 언제든 삭제될 수 있는 리소스에 대한
* Descriptor Heap을 가리키는 구조체.
* 특정 위치에 리소스의 desc를 할당/삭제할 수 있다.
*/
struct DescriptorAllotter {
	BitAllotter AllocFlagContainer;
	ID3D12DescriptorHeap* pDescHeap;
	ui32 DescriptorSize;
	ui32 extraData;

	/*
	* 설명 : DescriptorAllotter 를 초기화한다.
	* 매개변수 :
	* D3D12_DESCRIPTOR_HEAP_TYPE heapType : 리소스 디스크립터가 가지는 힙 타입
	* D3D12_DESCRIPTOR_HEAP_FLAGS Flags : 디스크립터의 힙 플래그 (보통 Non-Shader-Visible)
	* int Capacity : Desc를 저장할 수 있는 최대 개수.
	*/
	void Init(D3D12_DESCRIPTOR_HEAP_TYPE heapType, D3D12_DESCRIPTOR_HEAP_FLAGS Flags, int Capacity);

	/*
	* 설명 : 디스크립터가 들어갈 자리 하나를 할당 받는다.
	* 반환 : 할당된 디스크립터 자리의 배열 인덱스를 반환한다.
	*/
	int Alloc();

	/*
	* 설명 : 디스크립터 자리 하나를 해제한다.
	* 매개변수 :
	* int index : 해제할 디스크립터 자리의 인덱스
	*/
	void Free(int index);

	/*
	* 설명 : 어떤 index에 있는 GPU Desc Handle을 반환한다.
	* 매개변수 :
	* int index : Desc 자리 인덱스
	*/
	__forceinline D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(int index);

	/*
	* 설명 : 어떤 index에 있는 CPU Desc Handle을 반환한다.
	* 매개변수 :
	* int index : Desc 자리 인덱스
	*/
	__forceinline D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(int index) {
		D3D12_CPU_DESCRIPTOR_HANDLE handle;
		handle.ptr = pDescHeap->GetCPUDescriptorHandleForHeapStart().ptr + index * DescriptorSize;
		return handle;
	}
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

struct DirLightInfo {
	matrix DirLightProjection;
	matrix DirLightView;
	vec4 DirLightPos;
	vec4 DirLightDir;
	vec4 DirLightColor;
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
		: LightPos{ pos }, LightIntencity{ intencity }, LightColor{ color },
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

	void CreatePointLight(PointLightCBData init, UINT resolution = 512);

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

struct SpotLight {
	matrix View;
	GPUResource ShadowMap;
	vec4 LightPos;
	DescIndex descindex;
	ViewportData viewport;
};

struct Particle
{
	XMFLOAT3 Position;
	float Life;

	XMFLOAT3 Velocity;
	float Age;

	float Size;
	float Padding[3];
};

struct ParticlePool
{
	GPUResource Buffer;
	UINT Count;
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

class Shader;

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

	operator ID3D12GraphicsCommandList* () { return GraphicsCmdList; }
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

struct GlobalDevice;

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
	void MeshAddingMap();
	void MeshAddingUnMap();

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
		int materialIndex; // 4바이트 패딩을 머터리얼의 인덱스로 활용한다..

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

/*
* 설명 : DirectX 12 활용을 위한 렌더링 관련 전역변수들을 모아놓은 구조체.
*/
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
	bool debugDXGI = false;
	bool isSupportRaytracing = true;
	bool isRaytracingRender = false;

	IDXGIAdapter1* pSelectedAdapter = nullptr;
	IDXGIAdapter1* pOutputAdapter = nullptr;
	int adapterVersion = 0;
	int adapterIndex = 0;
	WCHAR adapterDescription[128] = {};

	D3D_FEATURE_LEVEL minFeatureLevel;

	//DXGI 객체를 만들기 위한 팩토리
	IDXGIFactory7* pFactory;// question 002 : why dxgi 6, but is type limit 7 ??

	//화면의 스왑체인
	IDXGISwapChain4* pSwapChain = nullptr;
	
	//DirectX 12 Device
	ID3D12Device* pDevice;

	// 스왑체인의 버퍼 개수
	static constexpr unsigned int SwapChainBufferCount = 2;
	// 현재 백 버퍼의 인덱스
	ui32 CurrentSwapChainBufferIndex;

	// RTV DESC의 증가 사이즈.
	ui32 RtvDescriptorIncrementSize;
	// 렌더타겟 리소스
	ID3D12Resource* ppRenderTargetBuffers[SwapChainBufferCount];
	// RTV Desc Heap
	ID3D12DescriptorHeap* pRtvDescriptorHeap;
	// RenderTargets SRV GPU HANDLE
	D3D12_GPU_DESCRIPTOR_HANDLE RenderTargetSRV_pGPU[SwapChainBufferCount];
	// 현재 해상도 width
	ui32 ClientFrameWidth;
	// 현재 해상도 height
	ui32 ClientFrameHeight;

	// 뎁스 스텐실 버퍼 리소스
	ID3D12Resource* pDepthStencilBuffer;
	// DSV DESC Heap
	ID3D12DescriptorHeap* pDsvDescriptorHeap;
	// DSV DESC 증가 사이즈
	ui32 DsvDescriptorIncrementSize;

	GPUCmd gpucmd;
	GPUCmd CScmd;
	ID3D12CommandQueue* pComputeCommandQueue;
	ID3D12CommandAllocator* pComputeCommandAllocator;
	ID3D12GraphicsCommandList* pComputeCommandList;

	// 펜스
	ID3D12Fence* pFence;
	// 펜스값
	ui64 FenceValue;
	// 펜스 이벤트
	HANDLE hFenceEvent;

	// 뷰포트의 개수
	static constexpr int ViewportCount = 2;
	// 뷰포트들의 데이터
	ViewportData viewportArr[ViewportCount];

	// anti aliasing multi sampling
	//MSAA level
	ui32 m_nMsaa4xQualityLevels = 0;
	//active MSAA
	bool m_bMsaa4xEnable = false; 

	// Mouse 움직임을 감지하기 위한 구조체
	RAWINPUTDEVICE RawMouse;
	// 입력 데이터 기록을 담을 데이터 배열
	BYTE InputTempBuffer[4096] = { };

	// 폰트 개수
	static constexpr int FontCount = 2;
	// 폰트이름 배열
	string font_filename[FontCount];
	// 폰트 데이터 배열
	TTFFontParser::FontData font_data[FontCount];
	// 폰트 텍스쳐들을 접근하기 위한 Map. wchar_t 하나를 받는다.
	unordered_map<wchar_t, GPUResource, hash<wchar_t>> font_sdftexture_map[FontCount];
	// 텍스쳐를 추가해야할 글자를 지정.
	vector<wchar_t> addSDFTextureStack;

	// <폰트를 저장하는 방식또한 달라지면 좋다.>

	/*
	* 설명 : DXGI Factory, GPU Adaptor 등을 초기화한다.
	* EnableFullScreenMode_Resolusions 에 OS 에서 제공하는 전체화면 해상도들을 얻는 작업 또한 수행한다.
	* 때문에 이는 윈도우가 만들어지기 전에 실행되며, 여기에서 크기를 받아 윈도우 사이즈를 초기에 결정한다.
	*/
	void Factory_Adaptor_Output_Init();

	/*
	* 설명 : Global Device를 전체적으로 초기화 한다. 선행적으로 Factory_Adaptor_Output_Init 함수가 호출되고, 윈도우가 생성되어야 한다.
	*/
	void Init();

	/*
	* 설명 : Global Device를 해제한다.
	*/
	void Release();

	/*
	* 설명 : 기존 스왑 체인을 해제하고, 새로운 스왑체인을 현재의 
	* ClientFrameWidth, ClientFrameHeight 만큼 잡아 만든다.
	*/
	void NewSwapChain();

	/*
	* 설명 : 현재 쓰이지 않는다. GBuffer를 쓰기 위해 뭔가 미리만들어놓은 함수.
	*/
	void NewGbuffer();

	/*
	* 설명 : GPU가 작업을 다 끝낼때까지 기다린다. Fence를 사용한다.
	*/
	void WaitGPUComplete();

	//렌더링 모드
	RenderingMod RenderMod = ForwardRendering;
	
	//렌더타겟의 픽셀포멧
	DXGI_FORMAT MainRenderTarget_PixelFormat;
	
	//디퍼드 렌더링의 경우, GBuffer의 개수
	static constexpr int GbufferCount = 1;
	// 디버드 렌더링 GBuffer들의 픽셀포맷
	DXGI_FORMAT GbufferPixelFormatArr[GbufferCount];
	//Gbuffer를 가리키는 DESC HEAP
	ID3D12DescriptorHeap* GbufferDescriptorHeap = nullptr;
	//Gbuffers
	ID3D12Resource* ppGBuffers[GbufferCount] = {};

	// Texutre의 DESC를 할당/해제하여 수정이 가능한 텍스쳐 출력에 쓰이게 한다.
	DescriptorAllotter TextureDescriptorAllotter;

	// CBV, SRV, UAV DESC의 증가 사이즈이다.
	unsigned long long CBV_SRV_UAV_Desc_IncrementSiz = 0;

	// 렌더링에 공통적으로 쓰이는 ShaderVisible한 DESC Heap.
	// Immortal과 Dynamic 영역으로 나뉜다.
	// Immortal은 게속 유지되는 Res DESC, 
	// Dynamic은 TextureDescriptorAllotter등의 또 다른 Non-ShaderVisible Desc Heap에 이미 존재하면서,
	// 유동적으로 자리를 차지할 수 있는 공간이다.
	SVDescPool2 ShaderVisibleDescPool;

	//Create GPU Heaps that contain RTV, DSV Datas
	void Create_RTV_DSV_DescriptorHeaps();

	//Make RTV
	void CreateRenderTargetViews();

	//non using
	void CreateGbufferRenderTargetViews();

	//Make DSV
	void CreateDepthStencilView();
	// when using Gbuffer for Defered Rendering, it require many RenderTarget

	// fix <현재 풀 스크린 모드가 안되는 것으로 알고 있다. 이것을 고쳐야 한다.>
	/*
	* 설명 : 풀 스크린 모드 / 창 모드를 전환한다.
	* 매개변수 : 
	* bool isFullScreen : true - 풀 스크린 모드로 전환 / false - 창 모드로 전환
	*/
	void SetFullScreenMode(bool isFullScreen);

	/*
	* 설명 : 현재의 해상도를 다른 해상도로 바꾼다.
	* 바뀌는 해상도는 OS에서 전체화면으로 지원하는 해상도만 바꿀 수 있다.
	* 그것은 resid 로 결정이 되며, 클 수록 더 정밀한 해상도를 결정한다.
	* 매개변수 : 
	* int resid : 가능한 해상도 id
	* bool ClientSizeUpdate : Window 사이즈도 같이 업데이트를 하는지에 대한 여부
	*/
	void SetResolution(int resid, bool ClientSizeUpdate);

	// OS에서 전체화면을 지원하는 해상도 리스트
	vector<ResolutionStruct> EnableFullScreenMode_Resolusions;

	/*
	* AI Code Start : Microsoft Copilot
	* 설명,반환 : DXGI_FORMAT을 받아 한 픽셀의 바이트 사이즈를 반환한다.
	* 매개변수 : 
	* DXGI_FORMAT format : 픽셀포맷
	*/
	static int PixelFormatToPixelSize(DXGI_FORMAT format);
	// AI Code End : Microsoft Copilot

	/*
	* 설명 : GPU 버퍼 리소스를 만든다.
	* 매개변수 : 
	* D3D12_HEAP_TYPE heapType : GPU Heap의 타입
	*	DEFAULT : GPU에서 읽고 쓰는 접근이 빠른 메모리
	*	UPLOAD : CPU RAM에서 GPU MEM으로 업로드할 수 있는 메모리
	*	READBACK : GPU MEM 에서 CPU RAM으로 읽을 수 있는 메모리
	* D3D12_RESOURCE_STATES d3dResourceStates : 초기 리소스 상태\
	*	// improve <common이 아닐때 에러가 나는 경우가 있던데 어떨때 그런가 정리가 필요함.>
	* D3D12_RESOURCE_DIMENSION dimension : 리소스가 어떤 타입의 리소스인지 결정.
	*	UNKNOWN : 알 수 없음.
	*	BUFFER : 버퍼
	*	TEXTURE 1D : 1차원 픽셀 정보
	*	TEXTURE 2D : 2차원 픽셀 텍스쳐 정보
	*	TEXTURE 3D : 3차원 픽셀 볼륨 정보
	* int Width : 텍스쳐일시 텍스쳐의 가로길이, 버퍼일시 바이트 크기
	* int Height : 텍스쳐일시 텍스쳐의 세로길이, 버퍼일시 1
	* DXGI_FORMAT BufferFormat : 한 픽셀 데이터가 어떤 포멧으로 이루어져 있는지.
	*	픽셀이 없다면 DXGI_FORMAT_UNKNOWN
	* D3D12_RESOURCE_FLAGS flags : 
	*	리소스의 플래그 설정. 다양한 enum 플래그를 or 연산으로 같이 적용시킬 수 있다.
	*	 D3D12_RESOURCE_FLAG_NONE	= 0,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET	= 0x1,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL	= 0x2,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS	= 0x4,
        D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE	= 0x8,
        D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER	= 0x10,
        D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS	= 0x20,
        D3D12_RESOURCE_FLAG_VIDEO_DECODE_REFERENCE_ONLY	= 0x40,
        D3D12_RESOURCE_FLAG_VIDEO_ENCODE_REFERENCE_ONLY	= 0x80,
        D3D12_RESOURCE_FLAG_RAYTRACING_ACCELERATION_STRUCTURE	= 0x100
	* 
	* 반환 : 해당 GPU 리소스 공간을 만들어 GPUResource에게 전달, 그것을 반환한다.
	*/
	GPUResource CreateCommitedGPUBuffer(D3D12_HEAP_TYPE heapType, 
		D3D12_RESOURCE_STATES d3dResourceStates, 
		D3D12_RESOURCE_DIMENSION dimension, 
		int Width, int Height, DXGI_FORMAT BufferFormat = DXGI_FORMAT_UNKNOWN, 
		UINT DepthOrArraySize = 1, 
		D3D12_RESOURCE_FLAGS AdditionalFlag = D3D12_RESOURCE_FLAG_NONE);

	/*
	* 설명 : (DEFAULT)copydestBuffer에 (UPLOAD)uploadBuffer를 사용해 ptr 부분의 CPU RAM 데이터를 
	*	복사한다. <해당 과정에는 커맨드리스트에 커맨드를 추가하고 GPU를 실행시키는 과정이 필요하기 때문에,
	*	커맨드리스트가 Reset 되어 있을 경우와, Close 되어있을 경우의 실행이 다르기 때문에, 이를 수정해야 한다.
		현재는 이미 Reset 이 완료되고 사용되고 있는 커맨드리스트가 있을 경우를 가정하고 코드가 작성되어 있디.>
		// fix

	* ID3D12GraphicsCommandList* commandList : 현재 사용중인 커맨드 리스트
	* void* ptr : 복사할 RAM 메모리의 시작주소
	* GPUResource* uploadBuffer : 업로드에 쓰일 업로드 버퍼
	* GPUResource* copydestBuffer : 업로드의 목적지가 될 DEFAULT HEAP
	* bool StateReturning : uploadBuffer와 copydestBuffer가 복사를 완료하고 기존의 STATE로 되돌아갈 것인지 결정.
	*/
	void UploadToCommitedGPUBuffer(void* ptr, GPUResource* uploadBuffer, 
		GPUResource* copydestBuffer = nullptr, bool StateReturning = true);
	
	/*
	 설명 : ??
	*/
	UINT64 GetRequiredIntermediateSize(
		_In_ ID3D12Resource* pDestinationResource,
		_In_range_(0, D3D12_REQ_SUBRESOURCES) UINT FirstSubresource,
		_In_range_(0, D3D12_REQ_SUBRESOURCES - FirstSubresource) UINT NumSubresources) noexcept;

	/*
	* 설명 : bmp 파일을 dds로 바꾸는 함수
	* 매개변수 : 
	* int mipmap_level : 밉맵 레벨
	* const char* Format : dds 블럭 압축 포맷
	* const char* filename : bmp 파일 경로
	*/
	static void bmpTodds(int mipmap_level, const char* Format, const char* filename);

	/*
	* 설명 : OBB 데이터를 통해 AABB 데이터를 구성한다.
	* OBB가 자유롭게 회전하여도 그것을 모두 포함하는 최소의 AABB를 얻게 한다.
	* vec4* out : 계산중이었던 AABB가 들어오고, AABB가 내보내질 공간. vec4[2] 만큼의 공간이 할당되어 있어야한다.
	* BoundingOrientedBox obb : AABB로 변환될 OBB.
	* bool first :
	*	true이면, AABB를 처음으로 계산하는 것이다. 그래서 obb를 AABB로 바꾸는 과정을 순수히 수행한다.
	*	false이면, 기존 out에 든 AABB 데이터와 obb 영역이 모두 포함되도록 하는 최소 AABB를 다시 쓴다.
	*	이런 기능은 여러 obb를 포함시키는 하나의 최소 AABB를 구하는데 쓰인다.
	*/
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

	//c++ 렌덤엔진
	default_random_engine dre;
	//렌덤이 균일하고 일정한 float 분포를 가지도록 설정.
	uniform_real_distribution<float> urd{ 0.0f, 1000000.0f };
	/*
	* 설명,반환 : min~max까지의 float 중 렌덤한 것을 독립적으로 렌덤 선택후 반환.
	* 매개변수 : 
	* float min : 최소값
	* float max : 최대값
	*/
	float randomRangef(float min, float max) {
		float r = urd(dre);
		return min + r * (max - min) / 1000000.0f;
	}

	void CreateDefaultHeap_VB(void* ptr, GPUResource& VertexBuffer, GPUResource& VertexUploadBuffer, D3D12_VERTEX_BUFFER_VIEW& view, UINT VertexCount, UINT sizeofVertex);

	// IndexCount는 TriangleIndex의 size() * 3.
	template <int indexByteSize>
	void CreateDefaultHeap_IB(void* ptr, GPUResource& IndexBuffer, GPUResource& IndexUploadBuffer, D3D12_INDEX_BUFFER_VIEW& view, UINT IndexCount);

	GPUResource CreateShadowMap(int width, int height, int DSVoffset, SpotLight& spotLight);

	//this function cannot be executing while command list update.
	/*
	* 설명 : 텍스트의 텍스쳐를 만든다.
	* 매개변수 :
	* wchar_t key : 새로 텍스쳐를 추가할 문자.
	*/
	void AddTextSDFTexture(wchar_t key);
	
	/*
	* 설명 : RAM에 저장된 가로와 세로가 width, height인 텍스쳐 texture에
	*	startpos ~ endpos 로 연결되는 하나의 선을 귿는다.
	* 매개변수 :
	* float_v2 startpos : 선의 시작점
	* float_v2 endpos : 선의 끝점
	* BYTE* texture : RAM 텍스쳐
	* int width : 가로픽셀길이
	* int height : 세로픽셀길이
	*/
	void AddLineInSDFTexture(float_v2 startpos, float_v2 endpos, char* texture, int width, int height);
	__forceinline char SignedFloatNormalizeToByte(float f);

	struct squarePair {
		int a, b;
		squarePair() {

		}

		~squarePair() {}

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
							d[yi * width + xi] = (float)(p.a * p.a + p.b * p.b);
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
		for (int i = 0; i < N; ++i) sdf[i] = distanceMul * d_in[i] - distanceMul * d_out[i];

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

	static constexpr bool PlayAnimationByGPU = true;

#pragma region TimeMeasureCode
	static constexpr int TimeMeasureSamepleCount = 32;
	static constexpr int MeasureCount = 1024;
	ui64 tickStack[MeasureCount] = {};
	ui64 tickMin[MeasureCount] = {};
	ui64 tickMax[MeasureCount] = {};
	ui64 ft[MeasureCount] = {};
	ui32 cnt[MeasureCount] = {};
	__forceinline void TIMER_STATICINIT() {
		for (int i = 0;i < MeasureCount;++i) {
			tickMin[i] = numeric_limits<unsigned long long>::lowest() - 1;
		}
	}
	__forceinline void AverageClockStart(int id = 0) {
		unsigned int aux = 0;
		ft[id] = __rdtscp(&aux);
	}
	__forceinline void AverageClockEnd(int id = 0) {
		unsigned int aux = 0;
		ui64 et = __rdtscp(&aux) - 33;
		cnt[id] += 1;
		ui64 del = et - ft[id];
		tickMin[id] = min(tickMin[id], del);
		tickMax[id] = max(tickMax[id], del);
		tickStack[id] += del;
		if (cnt[id] > TimeMeasureSamepleCount) {
			ui64 avrtick = tickStack[id] / TimeMeasureSamepleCount;
			dbglog4_noline(L"Clock Interval [%d] : Avg : %d clock \t Min : %d clock \n Max : %d clock \n", id, avrtick, tickMin[id], tickMax[id]);
			tickStack[id] = 0;
			cnt[id] = 0;
		}
	}

	__forceinline void AverageTickStart(int id = 0) {
		ft[id] = GetTicks();
	}
	__forceinline void AverageTickEnd(int id = 0) {
		ui64 et = GetTicks();
		cnt[id] += 1;
		ui64 del = et - ft[id];
		tickMin[id] = min(tickMin[id], del);
		tickMax[id] = max(tickMax[id], del);
		tickStack[id] += del;
		if (cnt[id] > TimeMeasureSamepleCount) {
			ui64 avrtick = tickStack[id] / TimeMeasureSamepleCount;
			dbglog4_noline(L"Tick Interval [%d] Avg : %d tick \t Min : %d tick \n Max : %d tick \n", id, avrtick, tickMin[id], tickMax[id]);
			tickStack[id] = 0;
			cnt[id] = 0;
		}
	}

	__forceinline void AverageSecPer60Start(int id = 0) {
		ft[id] = GetTicks();
	}
	__forceinline void AverageSecPer60End(int id = 0) {
		ui64 et = GetTicks();
		cnt[id] += 1;
		ui64 del = et - ft[id];
		tickMin[id] = min(tickMin[id], del);
		tickMax[id] = max(tickMax[id], del);
		tickStack[id] += del;
		if (cnt[id] > TimeMeasureSamepleCount) {
			double average = 60.0 * (double)(tickStack[id] / TimeMeasureSamepleCount) / (double)QUERYPERFORMANCE_HZ;
			double Min = 60.0 * ((double)tickMin[id] / (double)QUERYPERFORMANCE_HZ);
			double Max = 60.0 * ((double)tickMax[id] / (double)QUERYPERFORMANCE_HZ);
			dbglog4_noline(L"Step Interval [%d] Avg : %g step \t Min : %g step \t MAX : %g step \n", id, average, Min, Max);
			tickStack[id] = 0;
			cnt[id] = 0;
		}
	}
#pragma endregion

#pragma region GPUDebug
	void DeviceRemoveResonDebug() {
		volatile HRESULT hr = pDevice->GetDeviceRemovedReason();
		cout << hr << endl;
		dbgbreak(hr != S_OK);
	}
#pragma endregion
};

//Enum Measure Time
enum EMTime {
	Render = 0,
	Update = 1,
	Update_ClientRecv = 2,
	Update_ChunksUpdate = 3,
	Update_ClientUpdate = 4,
	Update_Monster_Animation = 5,
	Update_Monster_Animation_GetBoneLocalMatrixAtTime = 6,
	Update_Monster_Animation_SetRootMatrixs = 7,
};

/*
* 설명 : 메쉬 데이터
*/
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
	// 버택스 버퍼
	GPUResource VertexBuffer;
	// 버택스 업로드 버퍼
	GPUResource VertexUploadBuffer;
	// 버택스 버퍼 뷰
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView;

	// 인덱스 버퍼
	GPUResource IndexBuffer;
	// 인덱스 업로드 버퍼
	GPUResource IndexUploadBuffer;
	// 인덱스 버퍼 뷰
	D3D12_INDEX_BUFFER_VIEW IndexBufferView;

	// 인덱스 개수
	ui32 IndexNum = 0;
	// 버택스 개수
	ui32 VertexNum = 0;
	// Mesh의 토폴로지
	D3D12_PRIMITIVE_TOPOLOGY topology;
public:
	struct Vertex {
		XMFLOAT3 position;
		XMFLOAT4 color;
		XMFLOAT3 normal;

		Vertex() {}
		Vertex(XMFLOAT3 p, XMFLOAT4 col, XMFLOAT3 nor)
			: position{ p }, color{ col }, normal{ nor }
		{
		}
	};

	enum MeshType {
		_Mesh = 0,
		UVMesh = 1,
		BumpMesh = 2,
		SkinedBumpMesh = 3
	};
	MeshType type;

	// Mesh 의 OBB의 Extends
	XMFLOAT3 OBB_Ext;
	// Mesh 의 OBB의 Center
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

	/*
	* 설명 : AABB를 사용해 Mesh의 OBB 데이터를 구성.
	* 매개변수 :
	* XMFLOAT3 minpos : AABB의 최소 위치
	* XMFLOAT3 maxpos : AABB의 최대 위치
	*/
	void SetOBBDataWithAABB(XMFLOAT3 minpos, XMFLOAT3 maxpos);

	Mesh() {
		type = _Mesh;
	}
	virtual ~Mesh();

	/*
	* 설명 : path 경로에 있는 obj 파일의 Mesh를 불러온다. color 색 대로, centering 이 true일시에 OBB Center가 0이 된다.
	* const char* path : obj 파일 경로
	* vec4 color : Mesh에 입힐 색상
	* bool centering : Mesh의 OBB Center가 원점인지 여부 (Mesh의 정 중앙이 원점이 됨.)
	*/
	virtual void ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering = true);

	/*
	* 설명 : Mesh를 렌더링 한다.
	* 여기에서 파이프라인이나 셰이더를 설정하지 않는다.
	* 매개변수 :
	* ID3D12GraphicsCommandList* pCommandList : 현재 렌더링에 사용되는 커맨드 리스트
	* ui32 instanceNum : 렌더링 하고자 하는 Mesh의 개수. (인스턴싱의 경우 1이상의 값이 필요함.)
	*/
	virtual void Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum, ui32 slotIndex = 0);

	virtual void BatchRender(ID3D12GraphicsCommandList* pCommandList);

	/*
	* 설명/반환 : Mesh의 OBB를 얻는다.
	*/
	BoundingOrientedBox GetOBB();

	/*
	* 설명 : 각 모서리 길이가 width, height, depth이고, color 색을 가진 직육면체 Mesh를 만든다.
	* float width : 너비
	* float height : 높이
	* float depth : 폭
	* vec4 color : 색상
	*/
	void CreateWallMesh(float width, float height, float depth, vec4 color);

	/*
	* 설명 : Mesh를 해제함.
	*/
	virtual void Release();

	virtual void CreateSphereMesh(ID3D12GraphicsCommandList* pCommandList, float radius, int sliceCount, int stackCount, vec4 color);
};

/*
* 설명 : Texture Mapping을 위한 UV가 있는 Mesh.
*/
class UVMesh : public Mesh {
public:
	/*
	* 설명 : UVMesh의 버택스하나의 데이터
	*/
	struct Vertex {
		XMFLOAT3 position;
		XMFLOAT4 color;
		XMFLOAT3 normal;
		XMFLOAT2 uv;

		Vertex() {}
		Vertex(XMFLOAT3 p, XMFLOAT4 col, XMFLOAT3 nor, XMFLOAT2 texcoord)
			: position{ p }, color{ col }, normal{ nor }, uv{ texcoord }
		{
		}
	};

	/*
	* 설명 : path 경로에 있는 obj 파일의 Mesh를 불러온다. color 색 대로, centering 이 true일시에 OBB Center가 0이 된다.
	* const char* path : obj 파일 경로
	* vec4 color : Mesh에 입힐 색상
	* bool centering : Mesh의 OBB Center가 원점인지 여부 (Mesh의 정 중앙이 원점이 됨.)
	*/
	virtual void ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering = true);

	/*
	* 설명 : Mesh를 렌더링 한다.
	* 여기에서 파이프라인이나 셰이더를 설정하지 않는다.
	* 매개변수 :
	* ID3D12GraphicsCommandList* pCommandList : 현재 렌더링에 사용되는 커맨드 리스트
	* ui32 instanceNum : 렌더링 하고자 하는 Mesh의 개수. (인스턴싱의 경우 1이상의 값이 필요함.)
	*/
	virtual void Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum);

	/*
	* 설명 : 각 모서리 길이가 width, height, depth이고, color 색을 가진 직육면체 Mesh를 만든다.
	* float width : 너비
	* float height : 높이
	* float depth : 폭
	* vec4 color : 색상
	*/
	void CreateWallMesh(float width, float height, float depth, vec4 color);

	/*
	* 설명 : Mesh를 해제함.
	*/
	virtual void Release();

	/*
	* 설명 : Text 렌더링을 위한 Plane을 만드는 함수.
	*/
	void CreateTextRectMesh();
};

/*
* UV가 있어 텍스쳐 매핑을 할 수 있으면서, NormalMapping이 가능하도록 tangent 정보가 있는 Mesh.
*/
class BumpMesh : public Mesh {
public:
	// 해당 메쉬에 적용될 수 있는 머터리얼의 인덱스
	// fix <이건 게임 오브젝트에 있어야 한다. 수정이 필요함.>
	int material_index;

	RayTracingMesh rmesh;

	typedef RayTracingMesh::Vertex Vertex;

	BumpMesh() {
		type = Mesh::MeshType::BumpMesh;
	}
	virtual ~BumpMesh();
	/*
	* 설명 : 각 모서리 길이가 width, height, depth이고, color 색을 가진 직육면체 Mesh를 만든다.
	* float width : 너비
	* float height : 높이
	* float depth : 폭
	* vec4 color : 색상
	*/
	virtual void CreateWallMesh(float width, float height, float depth, vec4 color);

	void CreateMesh_FromVertexAndIndexData(vector<Vertex>& vert, vector<TriangleIndex>& inds, int SlotNum = 1, int* SlotArr = nullptr, bool include_DXR = true);

	/*
	* 설명 : path 경로에 있는 obj 파일의 Mesh를 불러온다. color 색 대로, centering 이 true일시에 OBB Center가 0이 된다.
	* const char* path : obj 파일 경로
	* vec4 color : Mesh에 입힐 색상
	* bool centering : Mesh의 OBB Center가 원점인지 여부 (Mesh의 정 중앙이 원점이 됨.)
	*/
	virtual void ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering = true, bool include_DXR = true);
	static BumpMesh* MakeTerrainMeshFromHeightMap(const char* HeightMapTexFilename, float bumpScale, float Unit, int& XN, int& ZN, byte8** Heightmap, bool include_DXR = true);
	void MakeTessTerrainMeshFromHeightMap(float EdgeLen, int xdiv, int zdiv);

	/*
	* 설명 : Mesh를 렌더링 한다.
	* 여기에서 파이프라인이나 셰이더를 설정하지 않는다.
	* 매개변수 :
	* ID3D12GraphicsCommandList* pCommandList : 현재 렌더링에 사용되는 커맨드 리스트
	* ui32 instanceNum : 렌더링 하고자 하는 Mesh의 개수. (인스턴싱의 경우 1이상의 값이 필요함.)
	*/
	virtual void Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum, ui32 slotIndex = 0);
	void MakeMeshFromWChar(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList
		* pd3dCommandList, const wchar_t wchar, float fontsiz);

	/*
	* 설명 : Mesh를 해제함.
	*/
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

/*
* 머터리얼에 적용되는 텍스쳐가 어떤 유형인지 나타내는 enum
*/
enum eTextureSemantic {
	NONE = 0,
	DIFFUSE = 1, // 색상
	SPECULAR = 2, // 정반사광
	AMBIENT = 3, // 앰비언트광
	EMISSIVE = 4, // 발광
	HEIGHT = 5, // 높이맵
	NORMALS = 6, // 노멀맵
	SHININESS = 7, // 샤인니스
	OPACITY = 8, // 불투명도
	DISPLACEMENT = 9, // 디스플레이스먼트
	LIGHTMAP = 10, // 라이트맵
	REFLECTION = 11, // 반사 맵 / 환경맵
	BASE_COLOR = 12, // 색상
	NORMAL_CAMERA = 13, // 카메라노멀
	EMISSION_COLOR = 14, // 색발광
	METALNESS = 15, // 메탈릭
	DIFFUSE_ROUGHNESS = 16, // (rgb)색상+(a)러프니스
	AMBIENT_OCCLUSION = 17, // AO
	UNKNOWN = 18, // 알 수 없음
	SHEEN = 19,
	CLEARCOAT = 20,
	TRANSMISSION = 21,
	MAYA_BASE = 22,
	MAYA_SPECULAR = 23,
	MAYA_SPECULAR_COLOR = 24,
	MAYA_SPECULAR_ROUGHNESS = 25,
	ANISOTROPY = 26, // ANISOTROPY - 넓은 경사면 밉맵 표현
	GLTF_METALLIC_ROUGHNESS = 27,
};

/*
* 설명 : 기본 색상으로 사용되는 텍스쳐의 종류
*/
enum eBaseTextureKind {
	Diffuse = 0,
	BaseColor = 1
};

/*
* 정반사 광으로 사용되는 텍스쳐의 종류
*/
enum eSpecularTextureKind {
	Specular = 0,
	Specular_Color = 1,
	Specular_Roughness = 2,
	Metalic = 3,
	Metalic_Roughness = 4
};

/*
* 앰비언트 기본광으로 사용되는 텍스쳐의 종류
*/
enum eAmbientTextureKind {
	Ambient = 0,
	AmbientOcculsion = 1
};

/*
* 발광 텍스쳐로 사용되는 텍스쳐의 종류
*/
enum eEmissiveTextureKind {
	Emissive = 0,
	Emissive_Color = 1,
};

/*
* 범프 매핑에 사용되는 텍스쳐
*/
enum eBumpTextureKind {
	Normal = 0,
	Normal_Camera = 1,
	Height = 2,
	Displacement = 3
};

/*
* 반사에 사용되는 텍스쳐
*/
enum eShinenessTextureKind {
	Shineness = 0,
	Roughness = 1,
	DiffuseRoughness = 2,
};

/*
* 투명/불투명에 사용되는 텍스쳐
*/
enum eOpacityTextureKind {
	Opacity = 0,
	Transparency = 1,
};

/*
* 설명 : 머터리얼을 표현하는 구조체.
*/
struct MaterialCB_Data {
	//색상
	vec4 baseColor;
	//앰비언트 기본광
	vec4 ambientColor;
	//발광
	vec4 emissiveColor;
	//메탈릭. 금속인 정도
	float metalicFactor;
	//반사정도
	float smoothness;
	//범프 스케일링
	float bumpScaling;
	//패딩
	float extra;
	//[64]

	// 타일링x
	float TilingX;
	// 타일링y
	float TilingY;
	// 타일링 오프셋 x
	float TilingOffsetX;
	// 타일링 오프셋 y
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

/*
* 설명 : 머터리얼 테이터
*/
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

	void ShiftTextureIndexs(unsigned int TextureIndexStart);
	void SetDescTable();
	MaterialCB_Data GetMatCB();
	MaterialST_Data GetMatST();

	static void InitMaterialStructuredBuffer(bool reset = false);
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

struct AnimGPUKey {
	vec4 pos;
	vec4 rot;
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
	double frameRate = 0;
	GPUResource AnimationRes;
	DescIndex AnimationDescIndex;
	
	

	void LoadHumanoidAnimation(string filename);
};

/*
* 설명 : 모델의 노드.
*/
struct ModelNode {
	// 모델 노드의 이름
	string name;
	// 모델 노드의 원본 transform
	XMMATRIX transform;
	// 부모 노드
	ModelNode* parent;
	// 자식노드 개수
	unsigned int numChildren;
	// 자식노드들
	ModelNode** Childrens;
	// 메쉬의 개수
	unsigned int numMesh = 0;
	// 메쉬의 인덱스 배열
	unsigned int* Meshes; // array of int
	// 메쉬가 가진 머터리얼 번호 배열
	unsigned int* Mesh_Materials;

	// 해당 메쉬가 몇번째 스킨메쉬인지에 대한 배열 Model이 스킨메쉬를 가지고, Node도 스킨메쉬를 가지면 할당됨.
	int* Mesh_SkinMeshindex = nullptr;

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

	/*
	* 설명 : 모델을 렌더할때 쓰이는 제귀호출되는 ModelNode Render 함수.
	* 매개변수 :
	* void* model : 원본모델 포인터
	* ID3D12GraphicsCommandList* cmdlist : 커맨드 리스트
	* const matrix& parentMat : 부모로부터 계승받은 월드 행렬
	*/
	template <bool isSkinMesh = false>
	void Render(void* model, GPUCmd& cmd, const matrix& parentMat, void* pGameobject = nullptr);

	void PushRenderBatch(void* model, const matrix& parentMat, void* pGameobject = nullptr);

	/*
	* 설명 : 모델 노드가 기본상태일때,
	* 해당 모델 노드의 자신과 모든 자식을 포함시키는 AABB를 구성하여
	* origin 모델의 AABB를 확장시킨다.
	* 매개변수 :
	* void* origin : Model의 인스턴스로, 해당 ModelNode를 소유한 원본 Model의 void*
	* const matrix& parentMat : 부모의 기본 trasform으로 부터 변환된 행렬
	*/
	void BakeAABB(void* origin, const matrix& parentMat);
};

/*
* 설명 : 모델
* Sentinal Value :
* NULL = (nodeCount == 0 && RootNode == nullptr && Nodes == nullptr && mNumMeshes == 0 && mMeshes == nullptr)
*/
struct Model {
	// 모델의 이름
	std::string mName;
	// 모델이 포함한 모든 노드들의 개수
	int nodeCount = 0;
	// 모델의 최상위 노드
	ModelNode* RootNode;

	// Nodes를 로드할 때만 쓰이는 변수들 (클라이언트에서 밖으로 빼낼 필요가 있음.)
	vector<ModelNode*> NodeArr;
	unordered_map<void*, int> nodeindexmap;

	// 모델 노드들의 배열
	ModelNode* Nodes;

	// 모델이 가진 메쉬의 개수
	unsigned int mNumMeshes;

	// 모델이 가진 메쉬들의 포인터 배열
	Mesh** mMeshes;
	unsigned int mNumSkinMesh;
	BumpSkinMesh** mBumpSkinMeshs;

	vector<BumpMesh::Vertex>* vertice = nullptr;
	vector<TriangleIndex>* indice = nullptr;

	matrix* DefaultNodelocalTr = nullptr;
	matrix* NodeOffsetMatrixArr = nullptr;

	//When GPU Animation
	GPUResource DefaultlocalTr_Humanoid;
	DescIndex DefaultlocalTr_Humanoid_SRV;
	GPUResource Offset_Humanoid;
	DescIndex Offset_Humanoid_CBV;

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

	// 모델의 UnitScaleFactor
	float UnitScaleFactor = 1.0f;

	//void Rearrange1(ModelNode* node);
	//void Rearrange2();
	//void GetModelFromAssimp(string filename, float posmul = 0);
	//Mesh* Assimp_ReadMesh(aiMesh* mesh, const aiScene* scene, int meshindex);
	//Material* Assimp_ReadMaterial(aiMaterial* mat, const aiScene* scene);
	//Animation* Assimp_ReadAnimation(aiAnimation* anim, const aiScene* scene);

	// 모델의 기본상태에서 모델을 모두 포함하는 가장 작은 AABB.
	vec4 AABB[2];
	// 모델의 OBB.Center
	vec4 OBB_Tr;
	// 모델의 OBB.Extends
	vec4 OBB_Ext;

	//?
	std::vector<matrix> BindPose;

	// nodeCount 만큼의 int 배열. 노드의 인덱스를 넣으면 휴머노이드채널인덱스가 나옴.
	int* Humanoid_retargeting = nullptr;

	/*
	* 설명 : Unity에서 뽑아온 맵에 존재하는 모델 바이너리 정보를 로드함.
	* 서버에서는 충돌정보만을 가져온다.
	* 매개변수:
	* string filename : 모델의 경로
	*/
	void LoadModelFile2(string filename);

	/*
	* 설명 : 계층구조를 출력한다.
	*/
	void DebugPrintHierarchy(ModelNode* node, int depth = 0);

	/*
	* 설명 : Model의 AABB를 구성한다.
	*/
	void BakeAABB();

	/*
	* 설명/반환 : Model의 기본 OBB를 반환한다.
	*/
	BoundingOrientedBox GetOBB();

	void Retargeting_Humanoid();

	/*
	* 설명 : 모델을 렌더링함
	* 매개변수 :
	* ID3D12GraphicsCommandList* cmdlist : 현재 렌더링을 수행하는 커맨드 리스트
	* matrix worldMatrix : 모델이 렌더링될 월드 행렬
	* ShaderType sre : 어떤 방식으로 렌더링 할 것인지
	*/
	template <bool isSkinMesh = false>
	void Render(GPUCmd& cmd, matrix worldMatrix, void* pGameobject = nullptr)
	{
		RootNode->Render<isSkinMesh>(this, cmd, worldMatrix, pGameobject);
	}

	void PushRenderBatch(matrix worldMatrix, void* pGameobject = nullptr);

	/*
	* 설명 : 노드의 이름으로 노드 인덱스를 찾는 함수
	* 매개변수 :
	* const std::string& name : 이름
	* 반환 : 노드의 인덱스 (찾지 못하면 -1.)
	*/
	int FindNodeIndexByName(const std::string& name);


};

/*
* 설명 : 셰이더를 나타내는 클래스.
* Sentinal Value :
* NULL = pPipelineState == nullptr && pRootSignature == nullptr
*/
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

	/*
	* 설명 : 셰이더를 초기화한다.
	*/
	virtual void InitShader();

	/*
	* 설명 : RootSignature를 만든다.
	*/
	virtual void CreateRootSignature();

	/*
	* 설명 : 파이프라인 스테이트를 만든다.
	*/
	virtual void CreatePipelineState();

	/*
	* 설명 : 셰이더를 선택해 커맨드리스트에 관련 커맨드를 등록한다.
	* 이 함수는 맴버함수이기 때문에, this가 선택하는 셰이더가 되고,
	* reg를 통해 셰이더의 렌더링 종류를 결정할 수 있다.
	* 매개변수 :
	* ID3D12GraphicsCommandList* commandList : 현재 렌더링에 쓰이는 commandList
	* ShaderType reg : 어떤 종류의 렌더링을 선택할 것인지.
	*/
	virtual void Add_RegisterShaderCommand(GPUCmd& cmd, ShaderType reg = ShaderType::RenderNormal);

	/*
	* 설명 : 셰이더의 바이트코드를 가져온다.
	* <현재는 파일로부터 GPU 바이트 코드를 가져오고 있다. 하지만 정석적인 방법은 애초에 바이트코드를>
	*/
	static D3D12_SHADER_BYTECODE GetShaderByteCode(const WCHAR* pszFileName, LPCSTR pszShaderName, LPCSTR pszShaderProfile, ID3DBlob** ppd3dShaderBlob, vector<D3D_SHADER_MACRO>* macros = nullptr);
};

/*
* 설명 : 단일 색상을 출력하는 셰이더
*/
class OnlyColorShader : public Shader {
public:
	ID3D12PipelineState* pUiPipelineState;

	OnlyColorShader();
	virtual ~OnlyColorShader();

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();
};

/*
* 설명 : 화면에 글자를 출력하는 셰이더 / 어떤 화면 영역에 텍스쳐를 렌더링 하는 셰이더
*/
class ScreenCharactorShader : public Shader {
public:
	ScreenCharactorShader();
	virtual ~ScreenCharactorShader();

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();
	virtual void Release();

	void SetTextureCommand(GPUResource* texture);
};

/*
* 설명 : PBR을 사용해 렌더링하는 셰이더
*/
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
	virtual void CreateRootSignature_SkinedMesh();
	virtual void CreateRootSignature_Instancing_withShadow();

	virtual void CreatePipelineState();
	virtual void CreatePipelineState_withShadow();
	virtual void CreatePipelineState_RenderShadowMap();
	virtual void CreatePipelineState_RenderStencil();
	virtual void CreatePipelineState_InnerMirror();
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

/*
* 설명 : 스카이박스를 렌더링하는 셰이더
* // improve <스카이박스를 만들고 적용시킬 수 있어야 함. 근데 지금은 3D겜플 샘플 데이터를 사용하고 있다. 공부해야 함.>
*/
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

class ParticleShader : public Shader {
public:
	ID3D12RootSignature* ParticleRootSig = nullptr;
	ID3D12PipelineState* ParticlePSO = nullptr;

	GPUResource* FireTexture = nullptr;

	virtual void InitShader() override;
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();

	void Render(ID3D12GraphicsCommandList* cmd, GPUResource* particleBuffer, UINT particleCount);
};

class ParticleCompute
{
public:
	ID3D12RootSignature* RootSig = nullptr;
	ID3D12PipelineState* PSO = nullptr;

	void Init(const wchar_t* hlslFile, const char* entry);
	void Dispatch(ID3D12GraphicsCommandList* cmd, GPUResource* buffer, UINT count, float dt);
};

class ComputeTestShader : public Shader {
public:
	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();

	virtual void Add_RegisterShaderCommand(GPUCmd& cmd, ShaderType reg = ShaderType::RenderNormal);
	virtual void Release();
};

class AnimationBlendingShader : public Shader {
public:
	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();

	virtual void Add_RegisterShaderCommand(GPUCmd& cmd, ShaderType reg = ShaderType::RenderNormal);
	virtual void Release();

	enum RootParamId {
		CBVTable_CBStruct = 0,
		SRVTable_Animation1to4 = 1,
		SRVTable_HumanoidToNodeindex = 2,
		UAVTable_Out_LocalMatrix = 3,
		Normal_RootParamCapacity = 4,
	};
};

class HBoneLocalToWorldShader : public Shader {
public:
	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();

	virtual void Add_RegisterShaderCommand(GPUCmd& cmd, ShaderType reg = ShaderType::RenderNormal);
	virtual void Release();

	enum RootParamId {
		Constant_WorldMat = 0,
		SRVTable_LocalMatrixs = 1,
		SRVTable_TPOSLocalTr = 2,
		SRVTable_toParent = 3,
		SRVTable_NodeToBoneIndex = 4,
		UAVTable_Out_ToWorldMatrix = 5,
		Normal_RootParamCapacity = 6,
	};
};

#pragma region DescHandleAndIndexCode
extern GlobalDevice gd;
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

DescHandle DescIndex::GetCreationDescHandle() const
{
	if (isShaderVisible && type == 'n') return gd.ShaderVisibleDescPool.NSVDescHeapCreationHandle[index];
	else if (type == 'n') return DescHandle(gd.TextureDescriptorAllotter.GetCPUHandle(index), D3D12_GPU_DESCRIPTOR_HANDLE(0));
	else if (type == 'r') return DescHandle(
		D3D12_CPU_DESCRIPTOR_HANDLE(gd.pRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + gd.RTVSize * index),
		D3D12_GPU_DESCRIPTOR_HANDLE(gd.pRtvDescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr + gd.RTVSize * index));
	else if (type == 'd') return DescHandle(
		D3D12_CPU_DESCRIPTOR_HANDLE(gd.pDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + gd.DSVSize * index),
		D3D12_GPU_DESCRIPTOR_HANDLE(gd.pDsvDescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr + gd.DSVSize * index));
}

DescHandle DescIndex::GetRenderDescHandle() const
{
	if (isShaderVisible && type == 'n') return gd.ShaderVisibleDescPool.SVDescHeapRenderHandle[index];
	else if (type == 'n') return DescHandle(D3D12_CPU_DESCRIPTOR_HANDLE(0), D3D12_GPU_DESCRIPTOR_HANDLE(0));
	else if (type == 'r') return DescHandle(
		D3D12_CPU_DESCRIPTOR_HANDLE(gd.pRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + gd.RTVSize * index),
		D3D12_GPU_DESCRIPTOR_HANDLE(0));
	else if (type == 'd') return DescHandle(
		D3D12_CPU_DESCRIPTOR_HANDLE(gd.pDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + gd.DSVSize * index),
		D3D12_GPU_DESCRIPTOR_HANDLE(0));

	return DescHandle(D3D12_CPU_DESCRIPTOR_HANDLE(0), D3D12_GPU_DESCRIPTOR_HANDLE(0));
}

#pragma endregion
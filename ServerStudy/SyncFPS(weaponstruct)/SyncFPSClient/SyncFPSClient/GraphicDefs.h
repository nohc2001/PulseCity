#pragma once
#include "stdafx.h"

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
	__forceinline D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(int index);
};

/*
* 설명 : 
*/
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

	bool	IncludeHandle(D3D12_CPU_DESCRIPTOR_HANDLE hcpu);
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

struct SpotLight {
	matrix View;
	GPUResource ShadowMap;
	vec4 LightPos;
	DescHandle renderDesc;
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
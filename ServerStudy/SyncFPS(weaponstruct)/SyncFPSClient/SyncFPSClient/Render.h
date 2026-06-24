#pragma once
#include <set>
#include "stdafx.h"
#include "ttfParser.h"
#include "SpaceMath.h"
#include "vecset.h"


using namespace TTFFontParser;

/*
* ���� :
* GPU�� �÷Ȱų� �ø� ���ҽ��� ��ũ���� �ڵ��� �����Ѵ�.
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
	short ZoneOffIndex = -1; // Zone�� ���� �������� ���. -1�̸� �۷ι���.
	ui32 index;
	DescIndex() {
		ZoneOffIndex = -1;
	}

	DescIndex(bool isSV, ui32 i, char t = 'n', short ZoneOff = -1) : isShaderVisible{ isSV }, index{ i }, type{ t }, ZoneOffIndex{ ZoneOff } {

	}

	void Set(bool isSV, ui32 i, char t = 'n', short ZoneOff = -1) {
		isShaderVisible = isSV;
		index = i;
		type = t;
		ZoneOffIndex = ZoneOff;
	}
	__forceinline DescHandle GetCreationDescHandle() const;
	__forceinline DescHandle GetRenderDescHandle() const;
	__declspec(property(get = GetCreationDescHandle)) const DescHandle hCreation;
	__declspec(property(get = GetRenderDescHandle)) const DescHandle hRender;
};

/*
* ���� : ���� ������ ���̴����� �ٸ��� �������� �Ϸ��ϱ� ������,
* � �������� �����?������ ������ �� �ְ� �ϴ� enum.
*/
union ShaderType {
	enum RegisterEnum_memberenum {
		RenderNormal = 0, // �Ϲ� ������
		RenderWithShadow = 1, // �׸��ڿ� �Բ� ������
		RenderShadowMap = 2, // ������ ���� ������
		RenderStencil = 3, // ���ٽ��� ������
		RenderInnerMirror = 4, // ���ٽ��� Ȱ��ȭ�� �κ��� ������ (�ſ� �� ������)
		RenderTerrain = 5, // �ͷ����� ������
		StreamOut = 6, // ��Ʈ���ƿ�
		SDF = 7, // SDF Text
		TessTerrain = 8, // Tess Terrain
		SkinMeshRender = 9,
		InstancingWithShadow = 10,
		Debug_OBB = 11,
		SkinMeshRenderShadowMap = 12,
	};
	int id;

	ShaderType(int i) : id{ i } {}
	ShaderType(RegisterEnum_memberenum e) { id = (int)e; }
	operator int() { return id; }
};

/*
* ���� : ����Ʈ ����
*/
struct ViewportData {
	// AABB
	D3D12_VIEWPORT Viewport;
	// ������Ʈ
	D3D12_RECT ScissorRect;

	// improve : <������ �� ������ ���鶧 � Layer�� ���ӿ�����Ʈ�� ����ϱ�?���� �������� ���̾���.>
	// <��ټ���?���ӿ����� �̷� �Գ��� ����. Ư�� UI�� 3D�ų� �׷��� Ư����.>
	// <�̸� ��� ������ ���ؾ� �ҵ�.>
	ui64 LayerFlag = 0;

	// �� ���?
	matrix ViewMatrix;
	// ���� ���?
	matrix ProjectMatrix;
	// ī�޶��� ��ġ
	vec4 Camera_Pos;

	// ���� �������� �ø��� ���� BoundingFrustum
	BoundingFrustum	m_xmFrustumWorld = BoundingFrustum();
	// ���� �������� �ø��� ���� BoundingOrientedBox
	BoundingOrientedBox OrthoFrustum;

	/*
	* //sus : <�����?�������?�ǽ��� �Ǵ� ��. �����׽�Ʈ�� �ʿ���.>
	* ���� : vec_in_gamespace ���� ȭ�����?�������� �����Ͽ� ��������.
	* ��ŷ�� ������ �̸� ��������.
	* �Ű����� :
	* const vec4& vec_in_gamespace : ���� �������� ��
	* ��ȯ :
	* vec_in_gamespace�� ȭ�� ���������� ������ ����
	*/
	__forceinline XMVECTOR project(const vec4& vec_in_gamespace) {
		return XMVector3Project(vec_in_gamespace, Viewport.TopLeftX, Viewport.TopLeftY, Viewport.Width, Viewport.Height, Viewport.MinDepth, Viewport.MaxDepth, ProjectMatrix, ViewMatrix, XMMatrixIdentity());
	}

	/*
	* //sus : <�����?�������?�ǽ��� �Ǵ� ��. �����׽�Ʈ�� �ʿ���.>
	* ���� : vec_in_screenspace ���� ȭ�����?�������� ���� �������� ��ȯ�� ��������.
	* ��ŷ�� ������ �̸� ��������.
	* �Ű����� :
	* const vec4& vec_in_screenspace : ȭ�� �������� ��
	* ��ȯ :
	* vec_in_screenspace�� �������������?������ ����
	*/
	__forceinline XMVECTOR unproject(const vec4& vec_in_screenspace) {
		return XMVector3Unproject(vec_in_screenspace, Viewport.TopLeftX, Viewport.TopLeftY, Viewport.Width, Viewport.Height, Viewport.MinDepth, Viewport.MaxDepth, ProjectMatrix, ViewMatrix, XMMatrixIdentity());
	}

	/*
	* //sus : <�����?�������?�ǽ��� �Ǵ� ��. �����׽�Ʈ�� �ʿ���.>
	* ���� : invecArr_in_gamespace �� �迭���� ȭ�����?�������� �����Ͽ�
	* outvecArr_in_screenspace �迭���� ��������.
	* ��ŷ�� ������ �̸� ��������.
	* �Ű����� :
	* vec4* invecArr_in_gamespace : �Է��� �Ǵ� ���������?�� �迭
	* vec4* outvecArr_in_screenspace : �����?�Ǵ� ȭ���������?�� �迭
	* int count : �迭�� ũ��
	*/
	__forceinline void project_vecArr(vec4* invecArr_in_gamespace, vec4* outvecArr_in_screenspace, int count) {
		XMVector3ProjectStream((XMFLOAT3*)invecArr_in_gamespace, 16, (XMFLOAT3*)outvecArr_in_screenspace, 16, count, Viewport.TopLeftX, Viewport.TopLeftY, Viewport.Width, Viewport.Height, Viewport.MinDepth, Viewport.MaxDepth, ProjectMatrix, ViewMatrix, XMMatrixIdentity());
	}

	/*
	* //sus : <�����?�������?�ǽ��� �Ǵ� ��. �����׽�Ʈ�� �ʿ���.>
	* ���� : invecArr_in_screenspace ȭ�����?�� �迭���� ���� �������� ��ȯ�Ͽ�
	* outvecArr_in_gamespace �迭���� ��������.
	* ��ŷ�� ������ �̸� ��������.
	* �Ű����� :
	* vec4* invecArr_in_screenspace : �Է��� �Ǵ� ȭ���������?�� �迭
	* vec4* outvecArr_in_gamespace : �����?�Ǵ� �����������?�� �迭
	* int count : �迭�� ũ��
	*/
	__forceinline void unproject_vecArr(vec4* invecArr_in_screenspace, vec4* outvecArr_in_gamespace, int count) {
		XMVector3UnprojectStream((XMFLOAT3*)outvecArr_in_gamespace, 16, (XMFLOAT3*)invecArr_in_screenspace, 16, count, Viewport.TopLeftX, Viewport.TopLeftY, Viewport.Width, Viewport.Height, Viewport.MinDepth, Viewport.MaxDepth, ProjectMatrix, ViewMatrix, XMMatrixIdentity());
	}

	/*
	* ���� : ���� ViewPort �����Ϳ� �°� ���� �� �������� m_xmFrustumWorld�� ������Ʈ�Ѵ�.
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
	* ���� : ���� ViewPort �����Ϳ� �°� ���� �� �������� OrthoFrustum�� ������Ʈ�Ѵ�.
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


	inline static vec4 PresentFrustumCorner[8] = {};
	/*
	* ���� : frustumViewProj ��ķ�?�����?���������� ���?�����ϸ鼭 targetOrientation�� ������ ������
	*	OBB�� ��ȯ
	* ��ȯ : ��ȯ�Ǵ� obb�� Extent.z�� ������ targetOrientation ������ ��.
	*/
	BoundingOrientedBox GetOBB_IncludeFrustum(matrix frustumViewProj, vec4 targetOrientation) {
		//�ʱ�ȭ 
		matrix invProj = XMMatrixInverse(nullptr, frustumViewProj);
		vec4 corners[8];
		vec4 ndcCorners[8] = {
			{-1,  1, 0, 1}, { 1,  1, 0, 1}, { 1, -1, 0, 1}, {-1, -1, 0, 1},
			{-1,  1, 1, 1}, { 1,  1, 1, 1}, { 1, -1, 1, 1}, {-1, -1, 1, 1}
		};
		matrix invRotation = XMMatrixRotationQuaternion(XMQuaternionInverse(targetOrientation));

		// �������� �� ���?/ ���?���� ���� ��ȸ����İ�������?��ȯ
		vec4 Average = 0;
		for (int i = 0; i < 8; ++i) {
			corners[i] = XMVector3TransformCoord(ndcCorners[i], invProj);
			PresentFrustumCorner[i] = corners[i];
			corners[i].w = 1;
			corners[i] = XMVector3TransformCoord(corners[i], invRotation);
			Average += PresentFrustumCorner[i];
		}
		Average /= 8;
		Average.w = 1;

		// ��ȸ���������� �ش� ������ ���?�����ϴ� AABB�� ����.
		BoundingBox AABB;
		BoundingBox::CreateFromPoints(AABB, 8, (XMFLOAT3*)corners, sizeof(XMVECTOR));

		// ��ȸ�������� �߽��� �ٽ� ���� �������� �ٲپ� ���� ���� ȸ������.
		vec4 Center = AABB.Center;
		Center.w = 1;
		Center.trQ(targetOrientation);

		// ��ȯ�� 8�� ���� ���?�����ϴ� OBB ����
		BoundingOrientedBox obb;
		obb = BoundingOrientedBox(Average.f3, AABB.Extents, targetOrientation);

		return obb;
	}
};

/*
* ���� : �ػ󵵸� ��Ÿ���� ����ü
*/
struct ResolutionStruct {
	ui32 width;
	ui32 height;
};

/*
* ���� : �������� � �������?������ ���� ���ð����� enum
*/
enum RenderingMod {
	ForwardRendering = 0,
	DeferedRendering = 1,
	WithRayTracing = 2,
};

/*
* ���� : �ݿ��������� �����Ǿ� Ư�� ��Ȳ���� ���� ���� �� �ְ�, �� ������ ������ �� �ִ� ���ҽ��� ����
* Descriptor Heap�� ����Ű�� ����ü.
* Ư�� ��ġ�� ���ҽ��� desc�� �Ҵ�/������ �� �ִ�.
*/
struct DescriptorAllotter {
	BitAllotter AllocFlagContainer;
	ID3D12DescriptorHeap* pDescHeap;
	ui32 DescriptorSize;
	ui32 extraData;

	//set<int> SDFRecord;

	/*
	* ���� : DescriptorAllotter �� �ʱ�ȭ�Ѵ�.
	* �Ű����� :
	* D3D12_DESCRIPTOR_HEAP_TYPE heapType : ���ҽ� ��ũ���Ͱ� ������ �� Ÿ��
	* D3D12_DESCRIPTOR_HEAP_FLAGS Flags : ��ũ������ �� �÷��� (���� Non-Shader-Visible)
	* int Capacity : Desc�� ������ �� �ִ� �ִ� ����.
	*/
	void Init(D3D12_DESCRIPTOR_HEAP_TYPE heapType, D3D12_DESCRIPTOR_HEAP_FLAGS Flags, int Capacity);

	/*
	* ���� : ��ũ���Ͱ� ���?�ڸ� �ϳ��� �Ҵ� �޴´�.
	* ��ȯ : �Ҵ��?��ũ���� �ڸ��� �迭 �ε����� ��ȯ�Ѵ�.
	*/
	int Alloc();

	/*
	* ���� : ��ũ���� �ڸ� �ϳ��� �����Ѵ�.
	* �Ű����� :
	* int index : ������ ��ũ���� �ڸ��� �ε���
	*/
	void Free(int index);

	/*
	* ���� : � index�� �ִ� GPU Desc Handle�� ��ȯ�Ѵ�.
	* �Ű����� :
	* int index : Desc �ڸ� �ε���
	*/
	__forceinline D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(int index);

	/*
	* ���� : � index�� �ִ� CPU Desc Handle�� ��ȯ�Ѵ�.
	* �Ű����� :
	* int index : Desc �ڸ� �ε���
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
	//Zone������ �ؽ��� ������.
	// 0 : �۷ι� �ؽ���
	// 1~10 : Zone ���� �ؽ���
	ui32 TextureSRVSizePerZone[10] = {};
	
	ui32 MaterialCBVSiz;
	ui32 MaterialCBVCap;
	//Zone������ �ؽ��� ������.
	// 0 : �۷ι� ���͸���
	// 1~10 : Zone ���� ���͸���
	ui32 MaterialCBVSizePerZone[10] = {};

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

	//���� �۷ι� ���ҽ� ���ε����� ZoneId �� -1�� ������ �ȴ�.
	BOOL ImmortalAlloc_TextureSRV_PerZone(DescIndex* pindex, UINT DescriptorCount, int ZoneId);
	BOOL ImmortalAlloc_MaterialCBV_PerZone(DescIndex* pindex, UINT DescriptorCount, int ZoneId);

	BOOL ImmortalAlloc_InstancingSRV(DescIndex* pindex, UINT DescriptorCount);

	void ExpendDescStructure(ui32 newInitDescArrCap, ui32 newTextureSRVCap, ui32 newMaterialCBVCap, ui32 newInstancingSRVCap);
	void BakeImmortalDesc();

	bool IncludeHandle(D3D12_CPU_DESCRIPTOR_HANDLE hcpu);
	void DynamicReset();
	void ImmortalReset_ExcludeInit();

	void Reset_Zone_FromAssetOffset(int AssetOffset);
};

struct GPUResource {
	// ���ε� ���۴� �ؽ��ĸ� ���ε��ϰ� Ŀ�ǵ尡 Execute�Ҷ����� �������� �ʰ� �����Ǿ��?�ϰ�, 
	// �۾��� ������ �����Ǿ��?�Ѵ�.
	static vector<ID3D12Resource*> TextureLoadedUploadBuffers;

	ID3D12Resource2* resource = nullptr;

	D3D12_RESOURCE_STATES CurrentState_InCommandWriteLine;
	//when add Resource Barrier command in command list, It Simulate Change Of Resource State that executed by GPU later.
	//so this member variable is not current Resource state.
	//it is current state of present command writing line.

	ui32 extraData;

	D3D12_GPU_VIRTUAL_ADDRESS pGPU;
	DescIndex descindex;

	void AddResourceBarrierTransitoinToCommand(ID3D12GraphicsCommandList* cmd, D3D12_RESOURCE_STATES afterState);
	D3D12_RESOURCE_BARRIER GetResourceBarrierTransition(D3D12_RESOURCE_STATES afterState);

	BOOL CreateTexture_fromImageBuffer(UINT Width, UINT Height, const BYTE* pInitImage, DXGI_FORMAT Format, bool ImmortalShaderVisible = false, int ZoneID = -1);
	void CreateTexture_fromFile(const wchar_t* filename, DXGI_FORMAT Format, int mipmapLevel, bool ImmortalShaderVisible = false, int ZoneID = -1);

	void UpdateTextureForWrite(ID3D12Resource* pSrcTexResource);

	void Release();

	ID3D12Resource* CreateTextureResourceFromDDSFile(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, wchar_t* pszFileName, ID3D12Resource** ppd3dUploadBuffer, D3D12_RESOURCE_STATES d3dResourceStates, bool bIsCubeMap = false)
	{
		ID3D12Resource* pd3dTexture = NULL;
		std::unique_ptr<uint8_t[]> ddsData;
		std::vector<D3D12_SUBRESOURCE_DATA> vSubresources;
		DDS_ALPHA_MODE ddsAlphaMode = DDS_ALPHA_MODE_STRAIGHT;

		HRESULT hResult = DirectX::LoadDDSTextureFromFileEx(pd3dDevice, pszFileName, 0, D3D12_RESOURCE_FLAG_NONE, DDS_LOADER_DEFAULT, &pd3dTexture, ddsData, vSubresources, &ddsAlphaMode, &bIsCubeMap);
		if (FAILED(hResult) || pd3dTexture == nullptr || vSubresources.empty()) {
            std::wstringstream ss;
            ss << L"ERROR : DDS texture load failed - " << pszFileName << L", hr = " << hResult << L"\n";
            OutputDebugStringW(ss.str().c_str());
			if (ppd3dUploadBuffer) *ppd3dUploadBuffer = nullptr;
			return nullptr;
		}

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
		d3dResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; //Upload Heap���� �ؽ��ĸ� ������ �� ����
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

	// �������� �ʴ� Static ������Ʈ���� �̸� �׷����� DSV CubeMap
	GPUResource StaticShadowCubeMap;
	DescHandle StaticCubeShadowMapHandleSRV;
	D3D12_CPU_DESCRIPTOR_HANDLE StaticCubeShadowMapHandleDSV[6];
	// ������ Dynamic ������Ʈ���� �ǽð����� �׸��� DSV CubeMap.
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
	XMMATRIX LightProjection[3];
	XMMATRIX LightView[3];
	vec4 LightPos[3];

	// ����ƽ �������� ���� �߰� ���?
	vec4 ChunckStart; // ���� �ε����� ���� ûũ ������.
	int ChunckCount[4]; // XYZ�� ûũ�� ����
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
	float Age;

	XMFLOAT3 Velocity;
	float LifeTime;

	XMFLOAT4 StartColor;
	XMFLOAT4 EndColor;

	float StartSize;
	float EndSize;
	float Rotation;
	float RotationSpeed;

	float Drag;
	float GravityScale;
	float Stretch;
	float CollisionRadius;

	UINT Flags;
	UINT RandomSeed;
	UINT FrameIndex;
	UINT FrameCount;
	UINT FrameCols;
};


struct ParticlePool
{
	GPUResource Buffer;
	UINT Count;
};

struct ParticleEmitterCB
{
	XMFLOAT3 Position;
	float Radius;

	XMFLOAT3 Direction;
	float Power;

	float Duration;
	float Age;
	UINT OwnerId;
	UINT Flags;
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

struct GlobalDevice;

class Shader;
struct GPUCmd {
	int dbgc[20] = {};
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

	void Execute(bool dxr = false);
	//void Execute(bool dxr = false) {
	//	dbgc[0] += 1;
	//	ID3D12CommandList* ppd3dCommandLists[] = { GraphicsCmdList };
	//	if (dxr) {
	//		ppd3dCommandLists[0] = DXRCmdList;
	//	}
	//	pCommandQueue->ExecuteCommandLists(1, ppd3dCommandLists);
	//}

	__forceinline void ResBarrierTr(ID3D12Resource* res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, UINT subRes = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAGS flag = D3D12_RESOURCE_BARRIER_FLAG_NONE) {
		CD3DX12_RESOURCE_BARRIER resBarrier = CD3DX12_RESOURCE_BARRIER::Transition(res, before, after, subRes, flag);
		GraphicsCmdList->ResourceBarrier(1, &resBarrier);
	}

	__forceinline void ResBarrierTr(GPUResource* res, D3D12_RESOURCE_STATES after) {
		res->AddResourceBarrierTransitoinToCommand(GraphicsCmdList, after);
	}

	void SetShader(Shader* shader, ShaderType reg = ShaderType::RenderNormal);

	void WaitGPUComplete();
	//void WaitGPUComplete() {
	//	ID3D12CommandQueue* selectQueue;
	//	if (pCommandQueue == nullptr) {
	//		selectQueue = pCommandQueue;
	//	}
	//	else selectQueue = pCommandQueue;
	//	FenceValue++;
	//	const UINT64 nFence = FenceValue;
	//	HRESULT hResult = selectQueue->Signal(pFence, nFence);
	//	//Add Signal Command (it update gpu fence value.)
	//	if (pFence->GetCompletedValue() < nFence)
	//	{
	//		//When GPU Fence is smaller than CPU FenceValue, Wait.
	//		hResult = pFence->SetEventOnCompletion(nFence, hFenceEvent);
	//		::WaitForSingleObject(hFenceEvent, INFINITE);
	//	}

	//	// Ŀ�ǵ尡 �����?���� �ε��?�ؽ��İ� ������, �ش� ���ε� ���۸� �����Ѵ�.
	//	for (int i = 0; i < GPUResource::TextureLoadedUploadBuffers.size(); ++i) {
	//		if (GPUResource::TextureLoadedUploadBuffers[i] != nullptr) {
	//			GPUResource::TextureLoadedUploadBuffers[i]->Release();
	//		}
	//	}
	//	GPUResource::TextureLoadedUploadBuffers.clear();
	//}

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

struct RayTracingDevice {
	struct CameraConstantBuffer
	{
		XMMATRIX projectionToWorld;
		XMVECTOR cameraPosition;
		XMFLOAT3 DirLight_invDirection;
		float DirLight_intencity;
		XMFLOAT3 DirLight_color;
		float padding;
		// ����ƽ �������� ���� �߰� ���?
		vec4 ChunckStart; // ���� �ε����� ���� ûũ ������.
		unsigned int ChunckCount[4]; // ûũ�� ����
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
	UINT64 UsingScratchSize = 0; // 256�� ���������?

	ID3D12Resource* RayTracingOutput = nullptr;
	DescIndex RTO_UAV_index;

	GPUResource DepthBuffer;
	DescIndex MainDepth_UAV;
	DescIndex MainDepth_SRV;

	ID3D12Resource* CameraCB[9] = {};
	CameraConstantBuffer* MappedCB[9] = {};

	vec4 m_eye;
	vec4 m_up;
	vec4 m_at;

	static constexpr bool kEnableTLASUpdate = false;

	void Init(void* origin_gd);

	//Raytracing�� �����Ǵ� ���� ���ʹ� ����̽�?���Ÿ� ó���ϴ� ���ø����̼��� �����θ� �׷��� �� �� �ִٰ� ������ �ʿ��ѵ�, �� �۾��� �Ѵ�.
	void CheckDeviceSelfRemovable();

	//Code From DirectX Raytracing HelloWorld Sample
	// Returns bool whether the device supports DirectX Raytracing tier.
	inline bool IsDirectXRaytracingSupported(IDXGIAdapter1* adapter);

	void SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ID3D12RootSignature** rootSig);

	bool InitDXC();

	SHADER_HANDLE* CreateShaderDXC(const wchar_t* shaderPath, const WCHAR* wchShaderFileName, const WCHAR* wchEntryPoint, const WCHAR* wchShaderModel, DWORD dwFlags, vector<LPWSTR>* macros = nullptr);

	void CreateSubRenderTarget();

	void CreateCameraCB();

	void SetRaytracingCamera(vec4 CameraPos, vec4 look, vec4 up = vec4(0, 1, 0, 0), float fov = -1);
};

#define SizeOfInUint32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)

//32 byte alignment require
struct LocalRootSigData {
	unsigned int VBOffset;
	unsigned int IBOffset;
	unsigned int MaterialStart;
	char padding[20];

	LocalRootSigData() {}
	LocalRootSigData(unsigned int VBOff, unsigned int IBOff, unsigned int MaterialSt, int ZoneID = -1);
	LocalRootSigData(const LocalRootSigData& ref) {
		VBOffset = ref.VBOffset;
		IBOffset = ref.IBOffset;
		MaterialStart = ref.MaterialStart;
		ZeroMemory(padding, sizeof(padding));
	}

	bool operator==(const LocalRootSigData& ref) const {
		return (ref.IBOffset == IBOffset && ref.VBOffset == VBOffset) && MaterialStart == ref.MaterialStart;
	}
};

// [float3 position] [float3 normal] [float2 uv] [float3 tangent] (�⺻������ BumpMesh��)
struct RayTracingMesh {
	//--------------- ���?�޽����� �����ϴ� ����----------------//
	// ���?�޽����� ���� �����ϴ� VB, IB (UploadBuffer)
	// ���ε� �����̹Ƿ� GPU ������ ���������?������. ������ ���� �ʴٸ�, ������ �ʴ� Mesh���� ���� 
	// DefaultHeap���� ������ �ʿ䰡 �ִ�.
	/*
	* commandList->CopyBufferRegion(
	defaultBuffer.Get(),   // ���?Default Heap)
	destOffset,            // ���?������
	uploadBuffer.Get(),    // ����(Upload Heap)
	srcOffset,             // ���� ������
	sizeInBytes            // ������ ũ��
	);
	*/

	//���?�޽����� ���� �����ϴ� VB, IB (UploadBuffer)
	inline static ID3D12Resource* vertexBuffer = nullptr; // SRV
	inline static ID3D12Resource* indexBuffer = nullptr; // SRV
	inline static ID3D12Resource* UAV_vertexBuffer = nullptr; // UAV, SRV

	// VB, IB�� �ִ�ũ��
	static constexpr int MB768 = 1048320; // MB�� ����� 768�� ������� ��. 21845���� ���ý��� ���� ����.
	static constexpr int VertexBufferCapacity = 700 * MB768; // 700MB
	static constexpr int IndexBufferCapacity = 300 * MB768; // 300MB
	static constexpr int UAV_VertexBufferCapacity = 300 * MB768; // 300MB

	//�� ���� �� �� �ִ� ���ý�/�ε��� ����ũ��
	static constexpr int VertexBufferCapacityPerZone = 70 * MB768; // 50MB
	static constexpr int IndexBufferCapacityPerZone = 30 * MB768; // 20MB

	/*
	* ���ý� ���ۿ� �ε��� ���۴� 10�������� ������ �ȴ�.
	* Seamless Zone�� ���ؼ�.
	* 0 : Global
	* 1~9 : Zone Local ��.
	* 
	* �ݸ� UAV_VertexBufferCapacity�� 
	* ��Ų�޽��� ���� ������, ��� �۷ι��̴�.
	*/

	// ���ε��� �޽��� VB, IB �� ���?������
	inline static ID3D12Resource* Upload_vertexBuffer = nullptr;
	inline static ID3D12Resource* Upload_indexBuffer = nullptr;
	inline static ID3D12Resource* UAV_Upload_vertexBuffer = nullptr;

	// ���ε� VB, IB�� �ִ�ũ��
	static constexpr int Upload_VertexBufferCapacity = 20 * MB768; // 20MB
	static constexpr int Upload_IndexBufferCapacity = 10 * MB768; // 10MB
	static constexpr int UAV_Upload_VertexBufferCapacity = 20 * MB768; // 20MB

	// ���ε� VB, IB�� ���ε� CPURAM �������� �ּ�
	inline static char* pVBMappedStart = nullptr;
	inline static char* pIBMappedStart = nullptr;
	inline static char* pUAV_VBMappedStart = nullptr;
	void MeshAddingMap();
	void MeshAddingUnMap();

	// VB, IB�� ���� ũ��
	inline static int VertexBufferByteSize[10] = {};
	inline static int IndexBufferByteSize[10] = {};
	inline static int UAV_VertexBufferByteSize = 0;

	static void ReleaseZone_FromAssetOffset(int AssetOffset);

	// VB, IB�� ����Ű�� SRV Desc Handle
	inline static DescIndex VBIB_DescIndex;
	inline static DescIndex UAV_VBIB_DescIndex;

	//--------------- �޽����� �������� ������ ����---------------//
	// �޽��� ���ý� ������, ������ �����Ͱ� ���ε� CPURAM �������� �ּ�
	char* pVBMapped = nullptr;
	char* pIBMapped = nullptr;
	char* pUAV_VBMapped = nullptr;

	// Ray�� �߻�ǰ�?BLAS�� �����?��ü�� ��������, Mesh�� VB, IB�� ������ �� �ֵ���
	// LocalRootSignature�� �ش� �޽��� VB, IB�� ���۵Ǵ� ����Ʈ�������� �־� �־��?�Ѵ�.
	UINT64 VBStartOffset;
	UINT64* IBStartOffset = nullptr;
	UINT64 UAV_VBStartOffset;

	int subMeshCount = 1;

	// �޽��� BLAS
	ID3D12Resource* BLAS;
	D3D12_RAYTRACING_GEOMETRY_DESC* GeometryDescs;
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS BLAS_Input;
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};

	// TLAS�� �ν��Ͻ��� �߰��Ҷ� �� ���� ����ؼ�?������ �ν��Ͻ��� ����.
	D3D12_RAYTRACING_INSTANCE_DESC MeshDefaultInstanceData = {};

	struct Vertex {
		XMFLOAT3 position;
		float u;
		float v;
		XMFLOAT3 normal;
		XMFLOAT3 tangent;
		int materialIndex; // 4����Ʈ �е��� ���͸����� �ε����� Ȱ���Ѵ�..

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

	//static std::set<D3D12_GPU_VIRTUAL_ADDRESS> BLASVA_Set;

	static void StaticInit();
	void AllocateRaytracingMesh(vector<Vertex> vbarr, vector<TriangleIndex> ibarr, int SubMeshNum = 1, int* SubMeshIndexes = nullptr, int ZoneID = -1);

	// ������ �ε����� �����Ͽ� �����ȴ�.
	void AllocateRaytracingUAVMesh(vector<Vertex> vbarr, UINT64* inIBStartOffset, int SubMeshNum = 1, int* SubMeshIndexes = nullptr);
	void AllocateRaytracingUAVMesh_OnlyIndex(vector<TriangleIndex> ibarr, int SubMeshNum = 1, int* SubMeshIndexes = nullptr);

	void UAV_BLAS_Refit();

	void Release();
};

struct RayTracingRenderInstance {
	RayTracingMesh* pMesh;
	matrix worldMat;
};

template<>
class hash<ShaderRecord> {
public:
	// ��Ȳ�� �´� �콬�� ������ �ʿ䰡 �ִ�.
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

	ID3D12Resource* TLAS = nullptr;
	ID3D12Resource* TLAS_InstanceDescs_Res; // UploadBuffer
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC TLASBuildDesc = {};

	// �����?Mesh ��ü�� ����
	static constexpr int TLAS_InstanceDescs_Capacity = 1048576;
	int TLAS_InstanceDescs_Size = 0;
	int TLAS_InstanceDescs_ImmortalSize = 0;
	D3D12_RAYTRACING_INSTANCE_DESC* TLAS_InstanceDescs_MappedData = nullptr;
	BitAllotter TLASAlloter;

	// immortal �� �ν��Ͻ��� clear�� �� �Ŀ� �߰��� ������. �ȱ׷� �߶׸��� �ν��Ͻ��� immortal �������� ����?
	// ��ȯ���� �ش� �ν��Ͻ��� �������?3x4)�� ����Ű�� �����͸� ��ȯ�Ѵ�.
	// LRSdata�� LocalRootSignature�� �迭�� ����? �迭�� ũ���?mesh.subMeshCount�̴�.
	//(����޽�����?LocalRoot �������� ���ε� �ȴ�.)
	float** push_rins_immortal(RayTracingMesh* mesh, matrix mat, LocalRootSigData* LRSdata, int hitGroupShaderIdentifyerIndex = 0);
	void clear_rins();
	float** push_rins(RayTracingMesh* mesh, matrix mat, LocalRootSigData* LRSdata, int hitGroupShaderIdentifyerIndex = 0);
	int GetOrCreateHitGroupShaderRecord(const LocalRootSigData& LRSdata, int hitGroupShaderIdentifyerIndex = 0, bool immortal = true);

	UINT shaderIdentifierSize;
	ComPtr<ID3D12Resource> RayGenShaderTable = nullptr;
	ComPtr<ID3D12Resource> MissShaderTable = nullptr;

	void** hitGroupShaderIdentifier = nullptr;
	//���� ������ �����޽��� ����
	static constexpr int HitGroupShaderTableCapavity = 1048576;
	int HitGroupShaderTableSize = 0;
	int HitGroupShaderTableImmortalSize = 0;
	ShaderTable hitGroupShaderTable;
	bool shaderTableInit = false;
	ComPtr<ID3D12Resource> HitGroupShaderTable = nullptr;

	// ���̴� ���̺��� Map.
	// ���̴� ���ڵ��� ������ �ش� ���ڵ尡 � ��ġ�� �ִ��� Ȯ�ΰ����ϰ�,
	// �ش� ���̴� ���ڵ尡 ���̺��� �����ϴ����� Ȯ�ΰ����ϴ�.
	unordered_map<ShaderRecord, int> HitGroupShaderTableToIndex;
	void InsertShaderRecord(ShaderRecord sr, int index);

	SkinMeshModifyShader* MySkinMeshModifyShader;
	vector<void*> RebuildBLASBuffer;

	void Init();
	void ReInit();
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

struct SDFTextSection {
	wchar_t c;
	ui16 pageindex;
	ui16 sx;
	ui16 sy;
	ui16 width;
	ui16 height;
};

struct SDFTextPageTextureBuffer {
	static constexpr int MaxWidth = 4096;
	static constexpr int MaxHeight = 4096;
	
	inline static unordered_map<wchar_t, SDFTextSection*> SDFSectionMap;

	ui8* data = nullptr; // PushSDFText ���� ���������?BakeSDF ���� ������.
	int present_StartX = 0;
	int present_StartY = 0;
	int present_height = 0;
	int pageindex = 0;
	//vector<SDFTextSection> sections;
	int sectionCount = 0;

	/*
	* false : �̸� ���� �ϼ��� �ؽ���
	* true : ���ο� ���ڿ��� ���ö����� ������Ʈ �� �� �ִ� �ؽ���
	*/
	bool isDynamicTexture = false;
	int uploadedSectionCount = 0;
	GPUResource UploadTextureBuffer;
	GPUResource DefaultTextureBuffer;
	char* mappedBuffer = nullptr;
	DescIndex SDFTextureSRV;

	

	SDFTextPageTextureBuffer(int page) :
		data{nullptr}, present_StartX {
		0
	}, present_StartY{ 0 }, present_height(0), pageindex{ page }
	{
		UploadTextureBuffer.resource = nullptr;
		DefaultTextureBuffer.resource = nullptr;
	}

	bool PushSDFText(wchar_t c, ui16 width, ui16 height, char* copybuffer);

	void BakeSDF();
};

/*
* ���� : DirectX 12 Ȱ���� ���� ������ ���� ������������ ��Ƴ���?����ü.
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
	bool isRaytracingRender = true;

	IDXGIAdapter1* pSelectedAdapter = nullptr;
	IDXGIAdapter1* pOutputAdapter = nullptr;
	int adapterVersion = 0;
	int adapterIndex = 0;
	WCHAR adapterDescription[128] = {};

	D3D_FEATURE_LEVEL minFeatureLevel;

	//DXGI ��ü�� �����?���� ���丮
	IDXGIFactory7* pFactory;// question 002 : why dxgi 6, but is type limit 7 ??

	//ȭ���� ����ü��
	IDXGISwapChain4* pSwapChain = nullptr;
	
	//DirectX 12 Device
	ID3D12Device* pDevice;

	// ����ü���� ���� ����
	static constexpr unsigned int SwapChainBufferCount = 2;
	// ���� �� ������ �ε���
	ui32 CurrentSwapChainBufferIndex;

	// RTV DESC�� ���� ������.
	ui32 RtvDescriptorIncrementSize;
	// ����Ÿ�� ���ҽ�
	ID3D12Resource* ppRenderTargetBuffers[SwapChainBufferCount];
	// RTV Desc Heap
	ID3D12DescriptorHeap* pRtvDescriptorHeap;
	// RenderTargets SRV GPU HANDLE
	D3D12_GPU_DESCRIPTOR_HANDLE RenderTargetSRV_pGPU[SwapChainBufferCount];
	// ���� �ػ� width
	ui32 ClientFrameWidth;
	// ���� �ػ� height
	ui32 ClientFrameHeight;

	// ���� ���ٽ� ���� ���ҽ�
	ID3D12Resource* pDepthStencilBuffer;
	// DSV DESC Heap
	ID3D12DescriptorHeap* pDsvDescriptorHeap;
	// DSV DESC ���� ������
	ui32 DsvDescriptorIncrementSize;
	// DS SRV
	DescIndex MainDS_SRV;

	GPUCmd gpucmd;

	GPUCmd CScmd;
	ID3D12CommandQueue* pComputeCommandQueue;
	ID3D12CommandAllocator* pComputeCommandAllocator;
	ID3D12GraphicsCommandList* pComputeCommandList;

	// �潺
	ID3D12Fence* pFence;
	// �潺��
	ui64 FenceValue;
	// �潺 �̺�Ʈ
	HANDLE hFenceEvent;

	// ����Ʈ�� ����
	static constexpr int ViewportCount = 2;
	// ����Ʈ���� ������
	ViewportData viewportArr[ViewportCount];

	// anti aliasing multi sampling
	//MSAA level
	ui32 m_nMsaa4xQualityLevels = 0;
	//active MSAA
	bool m_bMsaa4xEnable = false; 

	// Mouse �������� �����ϱ� ���� ����ü
	RAWINPUTDEVICE RawMouse;
	// �Է� ������ �����?���� ������ �迭
	BYTE InputTempBuffer[4096] = { };

	// ��Ʈ ����
	static constexpr int FontCount = 2;
	// ��Ʈ�̸� �迭
	string font_filename[FontCount];
	// ��Ʈ ������ �迭
	TTFFontParser::FontData font_data[FontCount];
	// ��Ʈ �ؽ��ĵ��� �����ϱ� ���� Map. wchar_t �ϳ��� �޴´�.
	unordered_map<wchar_t, GPUResource, hash<wchar_t>> font_sdftexture_map[FontCount];
	// �ؽ��ĸ� �߰��ؾ��� ���ڸ� ����.
	vector<wchar_t> addSDFTextureStack;

	// <��Ʈ�� �����ϴ� ��Ķ���?�޶����� ����.>

	/*
	* ���� : DXGI Factory, GPU Adaptor ���� �ʱ�ȭ�Ѵ�.
	* EnableFullScreenMode_Resolusions �� OS ���� �����ϴ� ��üȭ�� �ػ󵵵��� ���?�۾� ���� �����Ѵ�.
	* ������ �̴� �����찡 ���������?���� ����Ǹ�? ���⿡�� ũ�⸦ �޾� ������ ����� �ʱ⿡ �����Ѵ�.
	*/
	void Factory_Adaptor_Output_Init();

	/*
	* ���� : Global Device�� ��ü������ �ʱ�ȭ �Ѵ�. ���������� Factory_Adaptor_Output_Init �Լ��� ȣ��ǰ�? �����찡 �����Ǿ��?�Ѵ�.
	*/
	void Init();

	/*
	* ���� : Global Device�� �����Ѵ�.
	*/
	void Release();

	/*
	* ���� : ���� ���� ü���� �����ϰ�, ���ο� ����ü���� ������ 
	* ClientFrameWidth, ClientFrameHeight ��ŭ ���?�����?
	*/
	void NewSwapChain();

	/*
	* ���� : ���� ������ �ʴ´�. GBuffer�� ���� ���� ���� �̸��������� �Լ�.
	*/
	void NewGbuffer();

	/*
	* ���� : GPU�� �۾��� �� ���������� ��ٸ���? Fence�� ����Ѵ�?
	*/
	void WaitGPUComplete();

	//������ ���?
	RenderingMod RenderMod = ForwardRendering;
	
	//����Ÿ���� �ȼ�����
	DXGI_FORMAT MainRenderTarget_PixelFormat;
	
	//���۵� �������� ���? GBuffer�� ����
	static constexpr int GbufferCount = 1;
	// �����?������ GBuffer���� �ȼ�����
	DXGI_FORMAT GbufferPixelFormatArr[GbufferCount];
	//Gbuffer�� ����Ű�� DESC HEAP
	ID3D12DescriptorHeap* GbufferDescriptorHeap = nullptr;
	//Gbuffers
	ID3D12Resource* ppGBuffers[GbufferCount] = {};

	// Texutre�� DESC�� �Ҵ�/�����Ͽ� ������ ������ �ؽ��� ��¿� ���̰� �Ѵ�.
	DescriptorAllotter DynamicDescriptorAllotter;
	DescriptorAllotter DynamicDescriptorAllotterPerZone[9] = {};

	// CBV, SRV, UAV DESC�� ���� �������̴�.
	unsigned long long CBV_SRV_UAV_Desc_IncrementSiz = 0;

	// �������� ���������� ���̴� ShaderVisible�� DESC Heap.
	// Immortal�� Dynamic �������� ������.
	// Immortal�� �Լ� �����Ǵ� Res DESC, 
	// Dynamic�� TextureDescriptorAllotter���� �� �ٸ� Non-ShaderVisible Desc Heap�� �̹� �����ϸ鼭,
	// ���������� �ڸ��� ������ �� �ִ� �����̴�.
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

	// fix <���� Ǯ ��ũ�� ���?�ȵǴ� ������ �˰� �ִ�. �̰��� ���ľ� �Ѵ�.>
	/*
	* ���� : Ǯ ��ũ�� ���?/ â ���?��ȯ�Ѵ�.
	* �Ű����� : 
	* bool isFullScreen : true - Ǯ ��ũ�� ���� ��ȯ / false - â ���� ��ȯ
	*/
	void SetFullScreenMode(bool isFullScreen);

	/*
	* ���� : ������ �ػ󵵸� �ٸ� �ػ󵵷� �ٲ۴�.
	* �ٲ��?�ػ󵵴� OS���� ��üȭ������ �����ϴ� �ػ󵵸� �ٲ� �� �ִ�.
	* �װ��� resid �� ������ �Ǹ�, Ŭ ���� �� ������ �ػ󵵸� �����Ѵ�.
	* �Ű����� : 
	* int resid : ������ �ػ� id
	* bool ClientSizeUpdate : Window ����� ���� ������Ʈ�� �ϴ����� ���� ����
	*/
	void SetResolution(int resid, bool ClientSizeUpdate);

	// OS���� ��üȭ���� �����ϴ� �ػ� ����Ʈ
	vector<ResolutionStruct> EnableFullScreenMode_Resolusions;

	/*
	* AI Code Start : Microsoft Copilot
	* ����,��ȯ : DXGI_FORMAT�� �޾� �� �ȼ��� ����Ʈ ����� ��ȯ�Ѵ�.
	* �Ű����� : 
	* DXGI_FORMAT format : �ȼ�����
	*/
	static int PixelFormatToPixelSize(DXGI_FORMAT format);
	// AI Code End : Microsoft Copilot

	/*
	* ���� : GPU ���� ���ҽ��� �����?
	* �Ű����� : 
	* D3D12_HEAP_TYPE heapType : GPU Heap�� Ÿ��
	*	DEFAULT : GPU���� �а� ���� ������ ���� �޸�
	*	UPLOAD : CPU RAM���� GPU MEM���� ���ε��� �� �ִ� �޸�
	*	READBACK : GPU MEM ���� CPU RAM���� ���� �� �ִ� �޸�
	* D3D12_RESOURCE_STATES d3dResourceStates : �ʱ� ���ҽ� ����\
	*	// improve <common�� �ƴҶ� ������ ���� ���?�ִ��� ��� �׷��� ������ �ʿ���.>
	* D3D12_RESOURCE_DIMENSION dimension : ���ҽ��� � Ÿ���� ���ҽ����� ����.
	*	UNKNOWN : �� �� ����.
	*	BUFFER : ����
	*	TEXTURE 1D : 1���� �ȼ� ����
	*	TEXTURE 2D : 2���� �ȼ� �ؽ��� ����
	*	TEXTURE 3D : 3���� �ȼ� ���� ����
	* int Width : �ؽ����Ͻ� �ؽ����� ���α���, �����Ͻ� ����Ʈ ũ��
	* int Height : �ؽ����Ͻ� �ؽ����� ���α���, �����Ͻ� 1
	* DXGI_FORMAT BufferFormat : �� �ȼ� �����Ͱ� � �������� �̷����?�ִ���.
	*	�ȼ��� ���ٸ� DXGI_FORMAT_UNKNOWN
	* D3D12_RESOURCE_FLAGS flags : 
	*	���ҽ��� �÷��� ����. �پ��� enum �÷��׸� or �������� ���� ������?�� �ִ�.
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
	* ��ȯ : �ش� GPU ���ҽ� ������ �����?GPUResource���� ����, �װ��� ��ȯ�Ѵ�.
	*/
	GPUResource CreateCommitedGPUBuffer(D3D12_HEAP_TYPE heapType, 
		D3D12_RESOURCE_STATES d3dResourceStates, 
		D3D12_RESOURCE_DIMENSION dimension, 
		int Width, int Height, DXGI_FORMAT BufferFormat = DXGI_FORMAT_UNKNOWN, 
		UINT DepthOrArraySize = 1, 
		D3D12_RESOURCE_FLAGS AdditionalFlag = D3D12_RESOURCE_FLAG_NONE);

	/*
	* ���� : (DEFAULT)copydestBuffer�� (UPLOAD)uploadBuffer�� �����?ptr �κ��� CPU RAM �����͸� 
	*	�����Ѵ�. <�ش� �������� Ŀ�ǵ帮��Ʈ�� Ŀ�ǵ带 �߰��ϰ� GPU�� �����Ű��?������ �ʿ��ϱ� ������,
	*	Ŀ�ǵ帮��Ʈ�� Reset �Ǿ� ���� ����, Close �Ǿ����� �����?������ �ٸ��� ������, �̸� �����ؾ� �Ѵ�.
		�����?�̹� Reset �� �Ϸ�ǰ�?���ǰ� �ִ� Ŀ�ǵ帮��Ʈ�� ���� ���?�����ϰ� �ڵ尡 �ۼ��Ǿ� �ֵ�.>
		// fix

	* ID3D12GraphicsCommandList* commandList : ���� �������?Ŀ�ǵ� ����Ʈ
	* void* ptr : ������ RAM �޸��� �����ּ�
	* GPUResource* uploadBuffer : ���ε忡 ���� ���ε� ����
	* GPUResource* copydestBuffer : ���ε��� �������� �� DEFAULT HEAP
	* bool StateReturning : uploadBuffer�� copydestBuffer�� ���縦 �Ϸ��ϰ� ������ STATE�� �ǵ��ư� ������ ����.
	*/
	void UploadToCommitedGPUBuffer(void* ptr, GPUResource* uploadBuffer, 
		GPUResource* copydestBuffer = nullptr, bool StateReturning = true);
	
	/*
	 ���� : ??
	*/
	UINT64 GetRequiredIntermediateSize(
		_In_ ID3D12Resource* pDestinationResource,
		_In_range_(0, D3D12_REQ_SUBRESOURCES) UINT FirstSubresource,
		_In_range_(0, D3D12_REQ_SUBRESOURCES - FirstSubresource) UINT NumSubresources) noexcept;

	/*
	* ���� : bmp ������ dds�� �ٲٴ� �Լ�
	* �Ű����� : 
	* int mipmap_level : �Ӹ� ����
	* const char* Format : dds ���� ���� ����
	* const char* filename : bmp ���� ���?
	*/
	static void bmpTodds(int mipmap_level, const char* Format, const char* filename);

	/*
	* ���� : OBB �����͸� ���� AABB �����͸� �����Ѵ�.
	* OBB�� �����Ӱ� ȸ���Ͽ��� �װ��� ���?�����ϴ� �ּ��� AABB�� ���?�Ѵ�.
	* vec4* out : ������̾���?AABB�� ������, AABB�� �������� ����. vec4[2] ��ŭ�� ������ �Ҵ�Ǿ�?�־���Ѵ�?
	* BoundingOrientedBox obb : AABB�� ��ȯ�� OBB.
	* bool first :
	*	true�̸�, AABB�� ó������ ����ϴ�?���̴�. �׷��� obb�� AABB�� �ٲٴ� ������ ������ �����Ѵ�.
	*	false�̸�, ���� out�� �� AABB �����Ϳ� obb ������ ���?���Եǵ��� �ϴ� �ּ� AABB�� �ٽ� ����.
	*	�̷� �����?���� obb�� ���Խ�Ű�� �ϳ��� �ּ� AABB�� ���ϴµ� ���δ�.
	*/
	void GetAABBFromOBB(vec4* out, BoundingOrientedBox obb, bool first = false) {
		XMFLOAT3 corners[BoundingOrientedBox::CORNER_COUNT];
		obb.GetCorners(corners);

		//dbglog3_noline(L"AABB start : (%g, %g, %g) - ", out[0].x, out[0].y, out[0].z);
		//dbglog3_noline(L"(%g, %g, %g)\n", out[1].x, out[1].y, out[1].z);

		vec4 c[8];
		c[0].f3 = corners[0];
		//dbglog4_noline(L"OBB[%d] : (%g, %g, %g)\n", 0, c[0].x, c[0].y, c[0].z);
		vec4 minpos = (first) ? c[0] : vec4(_mm_min_ps(out[0], c[0]));
		vec4 maxpos = (first) ? c[0] : vec4(_mm_max_ps(out[1], c[0]));
		for (int i = 1; i < 8; ++i) {
			c[i].f3 = corners[i];
			//dbglog4_noline(L"OBB[%d] : (%g, %g, %g)\n", i, c[i].x, c[i].y, c[i].z);
			minpos = _mm_min_ps(c[i], minpos);
			maxpos = _mm_max_ps(c[i], maxpos);
		}
		out[0] = minpos;
		out[1] = maxpos;

		//dbglog3_noline(L"AABB result : (%g, %g, %g) - ", out[0].x, out[0].y, out[0].z);
		//dbglog3_noline(L"(%g, %g, %g)\n", out[1].x, out[1].y, out[1].z);

		return;

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

	//c++ ��������
	default_random_engine dre;
	//������ �����ϰ� ������ float ������ �������� ����.
	uniform_real_distribution<float> urd{ 0.0f, 1000000.0f };
	/*
	* ����,��ȯ : min~max������ float �� ������ ���� ���������� ���� ������ ��ȯ.
	* �Ű����� : 
	* float min : �ּҰ�
	* float max : �ִ밪
	*/
	float randomRangef(float min, float max) {
		float r = urd(dre);
		return min + r * (max - min) / 1000000.0f;
	}

	void CreateDefaultHeap_VB(void* ptr, GPUResource& VertexBuffer, GPUResource& VertexUploadBuffer, D3D12_VERTEX_BUFFER_VIEW& view, UINT VertexCount, UINT sizeofVertex);

	// IndexCount�� TriangleIndex�� size() * 3.
	template <int indexByteSize>
	void CreateDefaultHeap_IB(void* ptr, GPUResource& IndexBuffer, GPUResource& IndexUploadBuffer, D3D12_INDEX_BUFFER_VIEW& view, UINT IndexCount);

	GPUResource CreateShadowMap(int width, int height, int DSVoffset, SpotLight& spotLight);

	//this function cannot be executing while command list update.
	/*
	* ���� : �ؽ�Ʈ�� �ؽ��ĸ� �����?
	* �Ű����� :
	* wchar_t key : ���� �ؽ��ĸ� �߰��� ����.
	*/
	void AddTextSDFTexture(wchar_t key);
	
	/*
	* ���� : RAM�� �����?���ο� ���ΰ� width, height�� �ؽ��� texture��
	*	startpos ~ endpos �� ����Ǵ�?�ϳ��� ���� �ڴ´�.
	* �Ű����� :
	* float_v2 startpos : ���� ������
	* float_v2 endpos : ���� ����
	* BYTE* texture : RAM �ؽ���
	* int width : �����ȼ�����
	* int height : �����ȼ�����
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
	// ������ 2-pass distance transform (�ٻ�)
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

		// �ʱ�ȭ
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

		// sqrt�� �ٻ� ��Ŭ���� �Ÿ�
		for (float& v : d) v = std::sqrt(v);
		return d;
	}

	std::vector<uint8_t> makeSDF(char* raw, int width, int height, float distanceMul, float radius = -1.0f) {
		int N = width * height;
		std::vector<uint8_t> mask(N);
		vector<vec2f> outlines;

		// mask ���� (0 �� ����, 127 �� �ܺ�)
		for (int i = 0; i < N; ++i) {
			mask[i] = (raw[i] == 0) ? 1 : 0;
			if (raw[i] == 0) {
				outlines.push_back(vec2f(i % width, i / width));
			}
		}

		// ����/�ܺ� �Ÿ� ���?
		auto d_in = edt(mask, width, height);

		std::vector<uint8_t> inv(N);
		for (int i = 0; i < N; ++i) inv[i] = 1 - mask[i];
		auto d_out = edt(inv, width, height);

		// signed distance
		std::vector<float> sdf(N);
		for (int i = 0; i < N; ++i) sdf[i] = distanceMul * d_in[i] - distanceMul * d_out[i];

		// �ݰ� ���� (�ڵ�)
		if (radius <= 0.0f) radius = max(1.0f, 0.05f * min(width, height));

		// 0~255 ����
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
		DescIndex handle; // UAV
		DescIndex SRVIndex;
	};

	// width, height ũ���� �ؽ�ó�� UAV�� �����ϴ� �Լ�
	TextureWithUAV CreateTextureWithUAV(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format)
	{
		TextureWithUAV result{};

		// 1. �ؽ�ó ���ҽ� ����
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

		// 2. UAV ����
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
		// ���?��ȯ
		RenderTargetBundle bundle;

		UINT rtvDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		// ���ҽ� ����
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
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; // UAV�� ���?

		// RTV Ŭ���� ��
		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = format;
		clearValue.Color[0] = 0.0f;
		clearValue.Color[1] = 0.0f;
		clearValue.Color[2] = 0.0f;
		clearValue.Color[3] = 1.0f;

		// ���ҽ� ����
		ID3D12Resource* renderTarget;
		CD3DX12_HEAP_PROPERTIES hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		HRESULT hr = device->CreateCommittedResource(
			&hp,
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_RENDER_TARGET, // �ʱ� ���� RTV
			&clearValue,
			IID_PPV_ARGS(&renderTarget)
		);
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create render target texture.");
		}

		// RTV �ڵ� �Ҵ�
		DescHandle rtvHandle;
		rtvHandle.hcpu.ptr = rtvHeap->GetCPUDescriptorHandleForHeapStart().ptr + rtvIndex * rtvDescriptorSize;

		// RTV ����
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

	int SDFTextureArr_immortalSize = 0;
	vector<SDFTextPageTextureBuffer*> SDFTextureArr;
	int registerd_SDFTextSRVCount = 0;
	bool PushSDFText(wchar_t c, ui16 width, ui16 height, char* copybuffer);
	void GetBakedSDFs();

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
* ���� : �޽� ������
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
	// ���ý� ����
	GPUResource VertexBuffer;
	// ���ý� ���ε� ����
	GPUResource VertexUploadBuffer;
	// ���ý� ���� ��
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView;

	// �ε��� ����
	GPUResource IndexBuffer;
	// �ε��� ���ε� ����
	GPUResource IndexUploadBuffer;
	// �ε��� ���� ��
	D3D12_INDEX_BUFFER_VIEW IndexBufferView;

	// �ε��� ����
	ui32 IndexNum = 0;
	// ���ý� ����
	ui32 VertexNum = 0;
	// Mesh�� ��������
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
		_UVMesh = 1,
		_BumpMesh = 2,
		_SkinedBumpMesh = 3
	};
	MeshType type;

	// Mesh �� OBB�� Extends
	XMFLOAT3 OBB_Ext;
	// Mesh �� OBB�� Center
	XMFLOAT3 OBB_Tr;

	int subMeshNum = 0;
	int* SubMeshIndexStart = nullptr; // slotNum + 1 ���� int.
	// i ��° Mesh : (slotIndexStart[i] ~ slotIndexStart[i+1]-1)

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

		//���� �׸��� ������Ʈ�� ���� ���� �������� ������Ʈ�� ���?Ư�� ������Ʈ�� �������� ���ܽ�Ų��.
		void DestroyInstance(RenderInstanceData* instance);

		//�� �����Ӹ��� �ν��Ͻ� �׸��� ������Ʈ�� ���?�̰��� ����Ѵ�?
		void ClearInstancing();
		
		void Release();
	};
	static constexpr int MinInstancingStartSize = 0;
	InstancingStruct* InstanceData = nullptr; // ����޽���?������ŭ ����.
	void InstancingInit(unsigned int initialCapacity = 16);

	/*
	* ���� : AABB�� �����?Mesh�� OBB �����͸� ����.
	* �Ű����� :
	* XMFLOAT3 minpos : AABB�� �ּ� ��ġ
	* XMFLOAT3 maxpos : AABB�� �ִ� ��ġ
	*/
	void SetOBBDataWithAABB(XMFLOAT3 minpos, XMFLOAT3 maxpos);

	Mesh() {
		type = _Mesh;
	}
	virtual ~Mesh();

	/*
	* ���� : path ��ο�?�ִ� obj ������ Mesh�� �ҷ��´�. color �� ���? centering �� true�Ͻÿ� OBB Center�� 0�� �ȴ�.
	* const char* path : obj ���� ���?
	* vec4 color : Mesh�� ���� ����
	* bool centering : Mesh�� OBB Center�� �������� ���� (Mesh�� �� �߾��� ������ ��.)
	*/
	virtual void ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering = true);

	/*
	* ���� : Mesh�� ������ �Ѵ�.
	* ���⿡�� �����������̳� ���̴��� �������� �ʴ´�.
	* �Ű����� :
	* ID3D12GraphicsCommandList* pCommandList : ���� �������� ���Ǵ� Ŀ�ǵ� ����Ʈ
	* ui32 instanceNum : ������ �ϰ��� �ϴ� Mesh�� ����. (�ν��Ͻ��� ���?1�̻��� ���� �ʿ���.)
	*/
	virtual void Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum, ui32 slotIndex = 0);

	virtual void BatchRender(ID3D12GraphicsCommandList* pCommandList);

	/*
	* ����/��ȯ : Mesh�� OBB�� ��´�?
	*/
	BoundingOrientedBox GetOBB();

	/*
	* ���� : �� �𼭸� ���̰� width, height, depth�̰�, color ���� ���� ������ü Mesh�� �����?
	* float width : �ʺ�
	* float height : ����
	* float depth : ��
	* vec4 color : ����
	*/
	void CreateWallMesh(float width, float height, float depth, vec4 color);
	void CreateFlatDiskMesh(float outerRadius, float innerRadius, int segmentCount, vec4 color);

	/*
	* ���� : Mesh�� ������.
	*/
	virtual void Release();

	virtual void CreateSphereMesh(ID3D12GraphicsCommandList* pCommandList, float radius, int sliceCount, int stackCount, vec4 color);
};

/*
* ���� : Texture Mapping�� ���� UV�� �ִ� Mesh.
*/
class UVMesh : public Mesh {
public:
	/*
	* ���� : UVMesh�� ���ý��ϳ��� ������
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
	* ���� : path ��ο�?�ִ� obj ������ Mesh�� �ҷ��´�. color �� ���? centering �� true�Ͻÿ� OBB Center�� 0�� �ȴ�.
	* const char* path : obj ���� ���?
	* vec4 color : Mesh�� ���� ����
	* bool centering : Mesh�� OBB Center�� �������� ���� (Mesh�� �� �߾��� ������ ��.)
	*/
	virtual void ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering = true);

	/*
	* ���� : Mesh�� ������ �Ѵ�.
	* ���⿡�� �����������̳� ���̴��� �������� �ʴ´�.
	* �Ű����� :
	* ID3D12GraphicsCommandList* pCommandList : ���� �������� ���Ǵ� Ŀ�ǵ� ����Ʈ
	* ui32 instanceNum : ������ �ϰ��� �ϴ� Mesh�� ����. (�ν��Ͻ��� ���?1�̻��� ���� �ʿ���.)
	*/
	virtual void Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum);

	/*
	* ���� : �� �𼭸� ���̰� width, height, depth�̰�, color ���� ���� ������ü Mesh�� �����?
	* float width : �ʺ�
	* float height : ����
	* float depth : ��
	* vec4 color : ����
	*/
	void CreateWallMesh(float width, float height, float depth, vec4 color);
	void CreateSphereMesh(float radius, int sliceCount, int stackCount, vec4 color);
	void CreateBeamMesh(vec4 color);
	void CreateBeamMesh(vec4 color, float frameU, float frameV);
	void CreateMissileSpriteMesh(vec4 color, float frameU, float frameV);

	/*
	* ���� : Mesh�� ������.
	*/
	virtual void Release();

	/*
	* ���� : Text �������� ���� Plane�� �����?�Լ�.
	*/
	void CreateTextRectMesh();
};

/*
* UV�� �־� �ؽ��� ������ �� �� �����鼭, NormalMapping�� �����ϵ��� tangent ������ �ִ� Mesh.
*/
class BumpMesh : public Mesh {
public:
	RayTracingMesh rmesh;

	typedef RayTracingMesh::Vertex Vertex;
	std::vector<Vertex> sourceVertexData;
	std::vector<TriangleIndex> sourceIndexData;
	std::vector<int> sourceSubMeshIndexStart;
	int sourceZoneID = -1;
	bool sourceAutoLODReady = false;
	bool IsAutoLODGenerated = false;

	BumpMesh() {
		type = Mesh::MeshType::_BumpMesh;
	}
	virtual ~BumpMesh();
	/*
	* ���� : �� �𼭸� ���̰� width, height, depth�̰�, color ���� ���� ������ü Mesh�� �����?
	* float width : �ʺ�
	* float height : ����
	* float depth : ��
	* vec4 color : ����
	*/
	virtual void CreateWallMesh(float width, float height, float depth, vec4 color);

	void CreateMesh_FromVertexAndIndexData(vector<Vertex>& vert, vector<TriangleIndex>& inds, int SlotNum = 1, int* SlotArr = nullptr, bool include_DXR = true, int ZoneID = -1);

	/*
	* ���� : path ��ο�?�ִ� obj ������ Mesh�� �ҷ��´�. color �� ���? centering �� true�Ͻÿ� OBB Center�� 0�� �ȴ�.
	* const char* path : obj ���� ���?
	* vec4 color : Mesh�� ���� ����
	* bool centering : Mesh�� OBB Center�� �������� ���� (Mesh�� �� �߾��� ������ ��.)
	*/
	virtual void ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering = true, bool include_DXR = true, int ZoneID = -1);
	static BumpMesh* MakeTerrainMeshFromHeightMap(const char* HeightMapTexFilename, float bumpScale, float Unit, int& XN, int& ZN, byte8** Heightmap, bool include_DXR = true);
	void MakeTessTerrainMeshFromHeightMap(float EdgeLen, int xdiv, int zdiv);

	/*
	* ���� : Mesh�� ������ �Ѵ�.
	* ���⿡�� �����������̳� ���̴��� �������� �ʴ´�.
	* �Ű����� :
	* ID3D12GraphicsCommandList* pCommandList : ���� �������� ���Ǵ� Ŀ�ǵ� ����Ʈ
	* ui32 instanceNum : ������ �ϰ��� �ϴ� Mesh�� ����. (�ν��Ͻ��� ���?1�̻��� ���� �ʿ���.)
	*/
	virtual void Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum, ui32 slotIndex = 0);
	void MakeMeshFromWChar(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList
		* pd3dCommandList, const wchar_t wchar, float fontsiz);

	/*
	* ���� : Mesh�� ������.
	*/
	virtual void Release();

	virtual void BatchRender(ID3D12GraphicsCommandList* pCommandList);
};

class BumpSkinMesh : public Mesh {
public:
	matrix* OffsetMatrixs;
	int MatrixCount;
	GPUResource ToOffsetMatrixsCB;
	int* toNodeIndex;
	vector<matrix> BoneMatrixs;

	GPUResource BoneWeightBuffer;
	GPUResource BoneWeightUploadBuffer;
	D3D12_VERTEX_BUFFER_VIEW RenderVBufferView[2];

	// �� ���� ���ӵǾ� ����. DescTable�� �Ѵ� ���� ���� ����.
	DescIndex VertexSRV; // non shader visible desc heap�� ��ġ��.
	DescIndex BoneSRV; // non shader visible desc heap�� ��ġ��.
	RayTracingMesh rmesh;

	// raytracing�Ҷ� ���������� ���ý� �迭�� VB�� �ø��� ���� ���ȴ�.

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
		type = Mesh::MeshType::_SkinedBumpMesh;
	}
	virtual ~BumpSkinMesh();
	void CreateMesh_FromVertexAndIndexData(vector<Vertex>& vert, vector<BoneData>& bonedata, vector<TriangleIndex>& inds, int SubMeshNum = 1, int* SubMeshIndexArr = nullptr, matrix* NodeLocalMatrixs = nullptr, int matrixCount = 0);
	virtual void Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum, ui32 slotIndex = 0);
	virtual void Release();

	virtual void BatchRender(ID3D12GraphicsCommandList* pCommandList);
};

/*
* ���͸��� ����Ǵ�?�ؽ��İ� � �������� ��Ÿ���� enum
*/
enum eTextureSemantic {
	NONE = 0,
	DIFFUSE = 1, // ����
	SPECULAR = 2, // ���ݻ籤
	AMBIENT = 3, // �ں���?���?
	EMISSIVE = 4, // �߱�
	HEIGHT = 5, // ���̸�
	NORMALS = 6, // ��ָ�?
	SHININESS = 7, // ���δϽ�
	OPACITY = 8, // ��������
	DISPLACEMENT = 9, // ���÷��̽���Ʈ
	LIGHTMAP = 10, // ����Ʈ��
	REFLECTION = 11, // �ݻ� �� / ȯ���?
	BASE_COLOR = 12, // ����
	NORMAL_CAMERA = 13, // ī�޶���
	EMISSION_COLOR = 14, // ���߱�
	METALNESS = 15, // ��Ż��
	DIFFUSE_ROUGHNESS = 16, // (rgb)����+(a)�����Ͻ�
	AMBIENT_OCCLUSION = 17, // AO
	UNKNOWN = 18, // �� �� ����
	SHEEN = 19,
	CLEARCOAT = 20,
	TRANSMISSION = 21,
	MAYA_BASE = 22,
	MAYA_SPECULAR = 23,
	MAYA_SPECULAR_COLOR = 24,
	MAYA_SPECULAR_ROUGHNESS = 25,
	ANISOTROPY = 26, // ANISOTROPY - ���� ���� �Ӹ� ǥ��
	GLTF_METALLIC_ROUGHNESS = 27,
};

/*
* ���� : �⺻ �������� ���Ǵ� �ؽ����� ����
*/
enum eBaseTextureKind {
	Diffuse = 0,
	BaseColor = 1
};

/*
* ���ݻ� ������ ���Ǵ� �ؽ����� ����
*/
enum eSpecularTextureKind {
	Specular = 0,
	Specular_Color = 1,
	Specular_Roughness = 2,
	Metalic = 3,
	Metalic_Roughness = 4
};

/*
* �ں���?�⺻������ ���Ǵ� �ؽ����� ����
*/
enum eAmbientTextureKind {
	Ambient = 0,
	AmbientOcculsion = 1
};

/*
* �߱� �ؽ��ķ� ���Ǵ� �ؽ����� ����
*/
enum eEmissiveTextureKind {
	Emissive = 0,
	Emissive_Color = 1,
};

/*
* ���� ���ο� ���Ǵ� �ؽ���
*/
enum eBumpTextureKind {
	Normal = 0,
	Normal_Camera = 1,
	Height = 2,
	Displacement = 3
};

/*
* �ݻ翡 ���Ǵ� �ؽ���
*/
enum eShinenessTextureKind {
	Shineness = 0,
	Roughness = 1,
	DiffuseRoughness = 2,
};

/*
* ����/�������� ���Ǵ� �ؽ���
*/
enum eOpacityTextureKind {
	Opacity = 0,
	Transparency = 1,
};

/*
* ���� : ���͸����� ǥ���ϴ� ����ü.
*/
struct MaterialCB_Data {
	//����
	vec4 baseColor;
	//�ں���?�⺻��
	vec4 ambientColor;
	//�߱�
	vec4 emissiveColor;
	//��Ż��. �ݼ��� ����
	float metalicFactor;
	//�ݻ�����
	float smoothness;
	//���� �����ϸ�
	float bumpScaling;
	//�е�
	float extra;
	//[64]

	// Ÿ�ϸ�x
	float TilingX;
	// Ÿ�ϸ�y
	float TilingY;
	// Ÿ�ϸ� ������ x
	float TilingOffsetX;
	// Ÿ�ϸ� ������ y
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
* ���� : ���͸��� ������
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
	int ZoneOff = -1;

	inline static GPUResource MaterialStructuredBuffer;
	inline static MaterialST_Data* MappedMaterialStructuredBuffer = nullptr;
	inline static DescIndex MaterialStructuredBufferSRV;
	inline static int LastMaterialStructureBufferUp = 0;

	Material()
	{
		ZeroMemory(this, sizeof(Material));
		memset(&ti, 0xFF, sizeof(TextureIndex));
		clr.base = vec4(1, 1, 1, 1);
		TilingX = 1;
		TilingY = 1;
		TilingOffsetX = 1;
		TilingOffsetY = 1;
		ZoneOff = -1;
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
		ZoneOff = ref.ZoneOff;
	}

	void ShiftTextureIndexs(unsigned int TextureIndexStart);
	void SetDescTable(int zoneid);
	MaterialCB_Data GetMatCB();
	MaterialST_Data GetMatST();

	static void InitMaterialStructuredBuffer(bool reset = false);

	void Release();
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
* ���� : ���� ���?
*/
struct ModelNode {
	// �� �����?�̸�
	string name;
	// �� �����?���� transform
	XMMATRIX transform;
	// �θ� ���?
	ModelNode* parent = nullptr;
	// �ڽĳ��?����
	unsigned int numChildren = 0;
	// �ڽĳ���
	ModelNode** Childrens = nullptr;
	// �޽��� ����
	unsigned int numMesh = 0;
	// �޽��� �ε��� �迭
	unsigned int* Meshes; // array of int
	// �ش� �޽��� ����?��Ų�޽������� ���� �迭 Model�� ��Ų�޽��� ������, Node�� ��Ų�޽��� ������ �Ҵ��?
	int* Mesh_SkinMeshindex = nullptr;
	vector<BoundingBox> aabbArr;
	// �޽��� ���� ���͸��� ��ȣ �迭 (���� ���͸��� ���̺� ����.)
	int* materialIndex = nullptr;

	//metadata
	//???

	ModelNode() {
		parent = nullptr;
		numChildren = 0;
		Childrens = nullptr;
		numMesh = 0;
		Meshes = nullptr;
	}

	/*
	* ���� : ���� �����Ҷ� ���̴� ����ȣ��Ǵ�?ModelNode Render �Լ�.
	* �Ű����� :
	* void* model : ������ ������
	* ID3D12GraphicsCommandList* cmdlist : Ŀ�ǵ� ����Ʈ
	* const matrix& parentMat : �θ�κ���?��¹���?���� ���?
	*/
	template <bool isSkinMesh = false>
	void Render(void* model, GPUCmd& cmd, const matrix& parentMat, void* pGameobject = nullptr);

	void SkinMeshShadowRender(void* model, GPUCmd& cmd, const matrix& parentMat, void* pGameobject = nullptr);

	void PushRenderBatch(void* model, const matrix& parentMat, void* pGameobject = nullptr);

	/*
	* ���� : �� ���?�⺻�����϶�,
	* �ش� �� �����?�ڽŰ� ���?�ڽ��� ���Խ�Ű�� AABB�� �����Ͽ�
	* origin ���� AABB�� Ȯ���Ų��?
	* �Ű����� :
	* void* origin : Model�� �ν��Ͻ���, �ش� ModelNode�� ������ ���� Model�� void*
	* const matrix& parentMat : �θ��� �⺻ trasform���� ���� ��ȯ�� ���?
	*/
	void BakeAABB(void* origin, const matrix& parentMat);

	void RenderMeshOBBs(void* origin, const matrix& parentMat, void* gameobj = nullptr);

	void PushOBBs(void* origin, const matrix& parentMat, vector<BoundingOrientedBox>* obbArr, void* gameobj);

	void Release();
};

/*
* ���� : ��
* Sentinal Value :
* NULL = (nodeCount == 0 && RootNode == nullptr && Nodes == nullptr && mNumMeshes == 0 && mMeshes == nullptr)
*/
struct Model {
	// ���� �̸�
	std::string mName;
	// ���� ������ ���?������ ����
	int nodeCount = 0;
	// ���� �ֻ��� ���?
	ModelNode* RootNode;

	// Nodes�� �ε��� ���� ���̴� ������ (Ŭ���̾�Ʈ���� ������ ���� �ʿ䰡 ����.)
	vector<ModelNode*> NodeArr;
	unordered_map<void*, int> nodeindexmap;

	// �� ������ �迭
	ModelNode* Nodes;

	// ���� ���� �޽��� ����
	unsigned int mNumMeshes;

	// ���� ���� �޽����� ������ �迭
	Mesh** mMeshes;
	unsigned int mNumSkinMesh;
	BumpSkinMesh** mBumpSkinMeshs;

	matrix* DefaultNodelocalTr = nullptr;
	matrix* NodeOffsetMatrixArr = nullptr;

	unsigned int mNumTextures;
	unsigned int mNumMaterials;

	unsigned int TextureTableStart = 0;
	unsigned int MaterialTableStart = 0;

	/*unsigned int mNumAnimations;
	Animation** mAnimations;

	unsigned int mNumSkeletons;
	_Skeleton** mSkeletons;*/

	// ���� UnitScaleFactor
	float UnitScaleFactor = 1.0f;

	//void Rearrange1(ModelNode* node);
	//void Rearrange2();
	//void GetModelFromAssimp(string filename, float posmul = 0);
	//Mesh* Assimp_ReadMesh(aiMesh* mesh, const aiScene* scene, int meshindex);
	//Material* Assimp_ReadMaterial(aiMaterial* mat, const aiScene* scene);
	//Animation* Assimp_ReadAnimation(aiAnimation* anim, const aiScene* scene);

	// ���� �⺻���¿��� ���� ���?�����ϴ� ���� ���� AABB.
	vec4 AABB[2];
	// ���� OBB.Center
	vec4 OBB_Tr;
	// ���� OBB.Extends
	vec4 OBB_Ext;

	//?
	std::vector<matrix> BindPose;

	// nodeCount ��ŭ�� int �迭. �����?�ε����� ������ �޸ӳ��̵�ä���ε����� ����.
	int* Humanoid_retargeting = nullptr;

	/*
	* ���� : Unity���� �̾ƿ� �ʿ� �����ϴ� �� ���̳ʸ� ������ �ε���.
	* ���������� �浹�������� �����´�.
	* �Ű�����:
	* string filename : ���� ���?
	*/
	void LoadModelFile2(string filename, int ZoneId = -1, bool NoBone = false);

	/*
	* ���� : ���������� ����Ѵ�?
	*/
	void DebugPrintHierarchy(ModelNode* node, int depth = 0);

	/*
	* ���� : Model�� AABB�� �����Ѵ�.
	*/
	void BakeAABB();

	/*
	* ����/��ȯ : Model�� �⺻ OBB�� ��ȯ�Ѵ�.
	*/
	BoundingOrientedBox GetOBB();

	void Retargeting_Humanoid();

	/*
	* ���� : ���� ��������
	* �Ű����� :
	* ID3D12GraphicsCommandList* cmdlist : ���� �������� �����ϴ� Ŀ�ǵ� ����Ʈ
	* matrix worldMatrix : ���� �������� ���� ���?
	* ShaderType sre : � �������?������ �� ������
	*/
	template <bool isSkinMesh = false>
	void Render(GPUCmd& cmd, matrix worldMatrix, void* pGameobject = nullptr)
	{
		RootNode->Render<isSkinMesh>(this, cmd, worldMatrix, pGameobject);
	}

	void PushRenderBatch(matrix worldMatrix, void* pGameobject = nullptr);

	/*
	* ���� : �����?�̸����� ���?�ε����� ã�� �Լ�
	* �Ű����� :
	* const std::string& name : �̸�
	* ��ȯ : �����?�ε��� (ã�� ���ϸ� -1.)
	*/
	int FindNodeIndexByName(const std::string& name);

	void Release(bool releaseMat = true);
};

/*
* ���� : ���̴��� ��Ÿ���� Ŭ����.
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
	* ���� : ���̴��� �ʱ�ȭ�Ѵ�.
	*/
	virtual void InitShader();

	/*
	* ���� : RootSignature�� �����?
	*/
	virtual void CreateRootSignature();

	/*
	* ���� : ���������� ������Ʈ�� �����?
	*/
	virtual void CreatePipelineState();

	/*
	* ���� : ���̴��� ������ Ŀ�ǵ帮��Ʈ�� ���� Ŀ�ǵ带 ����Ѵ�?
	* �� �Լ��� �ɹ��Լ��̱� ������, this�� �����ϴ� ���̴��� �ǰ�,
	* reg�� ���� ���̴��� ������ ������ ������ �� �ִ�.
	* �Ű����� :
	* ID3D12GraphicsCommandList* commandList : ���� �������� ���̴� commandList
	* ShaderType reg : � ������ �������� ������ ������.
	*/
	virtual void Add_RegisterShaderCommand(GPUCmd& cmd, ShaderType reg = ShaderType::RenderNormal);

	/*
	* ���� : ���̴��� ����Ʈ�ڵ带 �����´�.
	* <�����?���Ϸκ��� GPU ����Ʈ �ڵ带 �������� �ִ�. ������ �������� �����?���ʿ� ����Ʈ�ڵ带>
	*/
	static D3D12_SHADER_BYTECODE GetShaderByteCode(const WCHAR* pszFileName, LPCSTR pszShaderName, LPCSTR pszShaderProfile, ID3DBlob** ppd3dShaderBlob, vector<D3D_SHADER_MACRO>* macros = nullptr);
};

/*
* ���� : ���� ������ ����ϴ�?���̴�
*/
class OnlyColorShader : public Shader {
public:
	ID3D12PipelineState* pUiPipelineState;
	ID3D12PipelineState* pPipelineState_OBBWireFrames;

	OnlyColorShader();
	virtual ~OnlyColorShader();

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();
	virtual void CreatePipelineState_OBBWireFrames();

	/*
	* ���� : ���̴��� ������ Ŀ�ǵ帮��Ʈ�� ���� Ŀ�ǵ带 ����Ѵ�?
	* �� �Լ��� �ɹ��Լ��̱� ������, this�� �����ϴ� ���̴��� �ǰ�,
	* reg�� ���� ���̴��� ������ ������ ������ �� �ִ�.
	* �Ű����� :
	* ID3D12GraphicsCommandList* commandList : ���� �������� ���̴� commandList
	* ShaderType reg : � ������ �������� ������ ������.
	*/
	virtual void Add_RegisterShaderCommand(GPUCmd& cmd, ShaderType reg = ShaderType::RenderNormal);

	enum RootParamId {
		Const_Camera = 0, // projview mat
		Const_Transform = 1, // world mat
		Normal_RootParamCapacity = 2,
	};
};

struct SDFInstance
{
	vec4 rect;
	vec4 uvrange;
	vec4 Color;
	float depth;
	float MinD;
	float MaxD;
	unsigned int pageId;
};

/*
* ���� : ȭ�鿡 ���ڸ� ����ϴ�?���̴� / � ȭ�� ������ �ؽ��ĸ� ������ �ϴ� ���̴�
*/
class ScreenShader : public Shader {
public:
	ID3D12PipelineState* pPipelineState_SDF = nullptr;
	ID3D12RootSignature* pRootSignature_SDF = nullptr;

	ScreenShader();
	virtual ~ScreenShader();

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();
	virtual void CreateRootSignature_SDF();
	virtual void CreatePipelineState_SDF();
	virtual void Release();

	virtual void Add_RegisterShaderCommand(GPUCmd& cmd, ShaderType reg = ShaderType::RenderNormal);

	void SetTextureCommand(GPUResource* texture);

	static constexpr int MaxInstance = 16384;
	inline static GPUResource SDFInstance_StructuredBuffer;
	inline static int SDFMappedCnt = 0;
	SDFInstance* MappedSDFInstance;
	static inline int SDFInstanceCount = 0;
	static inline DescIndex SDFInstance_SRV;
	__forceinline void PushSDFInstance(SDFInstance& ins) {
		if (SDFInstanceCount + 1 < MaxInstance) {
			MappedSDFInstance[SDFInstanceCount] = ins;
			SDFInstanceCount += 1;
		}
	}
	__forceinline void ClearSDFInstance() {
		SDFInstanceCount = 0;
	}
	void RenderAllSDFTexts();

	enum RootParamId {
		Const_BasicInfo = 0,
		SRVTable_SDFInstance = 1,
		SRVTable_Texture = 2,
		Normal_RootParamCapacity = 3,
	};
};

class WorldTextureShader : public Shader {
public:
	ID3D12PipelineState* pPipelineState = nullptr;
	ID3D12RootSignature* pRootSignature = nullptr;

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();
	virtual void Add_RegisterShaderCommand(GPUCmd& cmd, ShaderType reg = ShaderType::RenderNormal);
	virtual void Release();

	void SetTextureCommand(GPUResource* texture);

	enum RootParamId {
		Const_Camera = 0,
		Const_Transform = 1,
		Const_Tint = 2,
		Const_UVAnim = 3,
		SRVTable_Texture = 4,
		RootParamCapacity = 5,
	};
};

/*
* ���� : PBR�� �����?�������ϴ� ���̴�
*/
class PBRShader1 : public Shader {
public:
	ID3D12PipelineState* pPipelineState_TessTerrain = nullptr;
	ID3D12RootSignature* pRootSignature_TessTerrain = nullptr;
	ID3D12PipelineState* pPipelineState_SkinedMesh_withShadow = nullptr;
	ID3D12RootSignature* pRootSignature_SkinedMesh_withShadow = nullptr;
	ID3D12PipelineState* pPipelineState_Instancing_withShadow = nullptr;
	ID3D12RootSignature* pRootSignature_Instancing_withShadow = nullptr;

	ID3D12RootSignature* pRootSignature_SkinMesh_RenderShadow = nullptr;
	ID3D12PipelineState* pPipelineState_SkinMesh_RenderShadow = nullptr;

	PBRShader1() {}
	virtual ~PBRShader1() {}

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreateRootSignature_withShadow();
	virtual void CreateRootSignature_SkinedMesh();
	virtual void CreateRootSignature_Instancing_withShadow();
	virtual void CreateRootSignature_SkinMesh_RenderShadowMap();

	virtual void CreatePipelineState();
	virtual void CreatePipelineState_withShadow();
	virtual void CreatePipelineState_RenderShadowMap();
	virtual void CreatePipelineState_SkinMesh_RenderShadowMap();
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
		SRVTable_EnvionmentMap = 6,
		SRVTable_Chunck_StaticLightStructuredBuffer = 7,
		Const_ModelHitFlash = 8,
		withShaow_RootParamCapacity = 9,

		CBVTable_SkinMeshOffsetMatrix = 1,
		CBVTable_SkinMeshToWorldMatrix = 2,
		CBVTable_SkinMeshLightData = 3,
		SRVTable_SkinMeshMaterialTextures = 4,
		CBVTable_SkinMeshMaterial = 5,
		SRVTable_SkinMeshShadowMaps = 6,
		SRVTable_SKinMeshEnvironmentMap = 7,
		SRVTable_SKinMesh_Chunck_StaticLightStructuredBuffer = 8,
		Const_SkinMeshHitFlash = 9,
		withSkinMeshShaow_RootParamCapacity = 10,

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
* ���� : ��ī�̹ڽ��� �������ϴ� ���̴�
* // improve <��ī�̹ڽ��� �����?������?�� �־��?��. �ٵ� ������ 3D���� ���� �����͸� ����ϰ�?�ִ�. �����ؾ� ��.>
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
	void Dispatch(ID3D12GraphicsCommandList* cmd, GPUResource* buffer, UINT count, float dt, const ParticleEmitterCB& emitter);
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
	else if (type == 'n') {
		int zoneoff = ZoneOffIndex;
		if (zoneoff >= 0 && zoneoff < 10) {
			return DescHandle(gd.DynamicDescriptorAllotterPerZone[zoneoff].GetCPUHandle(index), D3D12_GPU_DESCRIPTOR_HANDLE(0));
		}
		else {
			return DescHandle(gd.DynamicDescriptorAllotter.GetCPUHandle(index), D3D12_GPU_DESCRIPTOR_HANDLE(0));
		}
	}
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

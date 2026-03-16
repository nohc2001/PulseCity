#include "stdafx.h"
#include "GraphicDefs.h"
#include "Render.h"
#include "Game.h"
#include "stb_image.h"

void GPUResource::AddResourceBarrierTransitoinToCommand(ID3D12GraphicsCommandList* cmd, D3D12_RESOURCE_STATES afterState)
{
	if (CurrentState_InCommandWriteLine != afterState) {
		D3D12_RESOURCE_BARRIER barrier = GetResourceBarrierTransition(afterState);
		cmd->ResourceBarrier(1, &barrier);
		CurrentState_InCommandWriteLine = afterState;
	}
}

D3D12_RESOURCE_BARRIER GPUResource::GetResourceBarrierTransition(D3D12_RESOURCE_STATES afterState)
{
	D3D12_RESOURCE_BARRIER d3dResourceBarrier;
	::ZeroMemory(&d3dResourceBarrier, sizeof(D3D12_RESOURCE_BARRIER));
	d3dResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	d3dResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	d3dResourceBarrier.Transition.pResource = resource;
	d3dResourceBarrier.Transition.StateBefore = CurrentState_InCommandWriteLine;
	d3dResourceBarrier.Transition.StateAfter = afterState;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	return d3dResourceBarrier;
}

BOOL GPUResource::CreateTexture_fromImageBuffer(UINT Width, UINT Height, const BYTE* pInitImage, DXGI_FORMAT Format, bool ImmortalShaderVisible)
{
	if (resource != nullptr) return FALSE;

	GPUResource uploadBuffer;

	//D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER - ??
	*this = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_DIMENSION_TEXTURE2D, Width, Height, Format);

	if (pInitImage)
	{
		D3D12_RESOURCE_DESC Desc = resource->GetDesc();
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint;
		UINT	Rows = 0;
		UINT64	RowSize = 0;
		UINT64	TotalBytes = 0;

		gd.pDevice->GetCopyableFootprints(&Desc, 0, 1, 0, &Footprint, &Rows, &RowSize, &TotalBytes);

		BYTE* pMappedPtr = nullptr;
		D3D12_RANGE writeRange;
		writeRange.Begin = 0;
		writeRange.End = 0;

		//return size of buffer for uploading data. (D3dx12.h)
		UINT64 uploadBufferSize = gd.GetRequiredIntermediateSize(resource, 0, 1);
		uploadBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, uploadBufferSize, 1, DXGI_FORMAT_UNKNOWN);

		HRESULT hr = uploadBuffer.resource->Map(0, &writeRange, reinterpret_cast<void**>(&pMappedPtr));
		if (FAILED(hr))
			return FALSE;

		const BYTE* pSrc = pInitImage;
		BYTE* pDest = pMappedPtr;
		int mul = 4;
		if (Format == DXGI_FORMAT_R8_SNORM) mul = 1;
		for (UINT y = 0; y < Height; y++)
		{
			memcpy(pDest, pSrc, Width * mul);
			pSrc += (Width * mul);
			pDest += Footprint.Footprint.RowPitch;
		}
		// Unmap
		uploadBuffer.resource->Unmap(0, nullptr);

		UpdateTextureForWrite(uploadBuffer.resource);

		uploadBuffer.resource->Release();
		uploadBuffer.resource = nullptr;
	}

	// success.
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = Format;
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MipLevels = 1;

	//pGPU = resource->GetGPUVirtualAddress();

	if (ImmortalShaderVisible == false) {
		int srvIndex = gd.TextureDescriptorAllotter.Alloc();
		if (srvIndex >= 0)
		{
			descindex = DescIndex(false, srvIndex);
			gd.pDevice->CreateShaderResourceView(resource, &SRVDesc, descindex.hCreation.hcpu);
		}
		else
		{
			resource->Release();
			resource = nullptr;
			return FALSE;
		}
	}
	else {
		bool b = gd.ShaderVisibleDescPool.ImmortalAlloc(&descindex, 1);
		if (b) {
			gd.pDevice->CreateShaderResourceView(resource, &SRVDesc, descindex.hCreation.hcpu);
		}
		else {
			resource->Release();
			resource = nullptr;
			return FALSE;
		}
	}

	AddResourceBarrierTransitoinToCommand(gd.gpucmd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	return TRUE;
}

void GPUResource::CreateTexture_fromFile(const wchar_t* filename, DXGI_FORMAT Format, int mipmapLevel, bool ImmortalShaderVisible)
{
	int len = wcslen(filename);
	wchar_t* DDSName = nullptr;
	//file is dds
	if (wcscmp(&filename[len - 3], L"dds") == 0) {
		DDSName = (wchar_t*)filename;
		goto TEXTURE_LOAD_START;
	}
	else {
		//if dds not exist
		wchar_t DDSFile[512] = {};
		wcscpy_s(DDSFile, filename);
		wchar_t* ddsstr = &DDSFile[len - 3];
		wcscpy_s(ddsstr, 4, L"dds");
		std::ifstream file(DDSFile);
		if (file.good()) {
			file.close();
			DDSName = DDSFile;
			// use dds file
			dbglog1(L"texture filename is exist in dds. %d\n", 0);
			goto TEXTURE_LOAD_START;
		}
		else {
			// create dds file

			// get filename as char[] (for stb_image function)
			int TexWidth, TexHeight, nrChannels = 4;
			char cfilename[512] = {};

			for (int i = 0; i < len; ++i) {
				cfilename[i] = filename[i];
			}
			cfilename[len] = 0;

			// raw image data load
			BYTE* pImageData = stbi_load(cfilename, &TexWidth, &TexHeight, &nrChannels, 4);

			// create bmp image file to convert dds
			char BMPFile[512] = {};
			strcpy_s(BMPFile, cfilename);
			strcpy_s(&BMPFile[len - 3], 4, "bmp");
			imgform::PixelImageObject pio;
			pio.data = (imgform::RGBA_pixel*)pImageData;
			pio.width = TexWidth;
			pio.height = TexHeight;
			pio.rawDataToBMP(BMPFile);

			//raw image data free
			stbi_image_free(pImageData);

			// block compression format identify
			switch (Format) {
			case DXGI_FORMAT_BC1_UNORM:
				gd.bmpTodds(mipmapLevel, "BC1_UNORM", BMPFile);
				break;
			case DXGI_FORMAT_BC2_UNORM:
				gd.bmpTodds(mipmapLevel, "BC2_UNORM", BMPFile);
				break;
			case DXGI_FORMAT_BC3_UNORM:
				gd.bmpTodds(mipmapLevel, "BC3_UNORM", BMPFile);
				break;
			case DXGI_FORMAT_BC7_UNORM: // normal map compression.
				gd.bmpTodds(mipmapLevel, "BC7_UNORM", BMPFile);
				break;
			default:
				gd.bmpTodds(mipmapLevel, "BC1_UNORM", BMPFile);
				break;
			}

			wchar_t* lastSlash = wcsrchr(DDSFile, L'/');
			lastSlash++;

			MoveFileW(lastSlash, DDSFile);

			DDSName = DDSFile;

			//delete original file, bmp file
			//DeleteFileW(filename);
			DeleteFileA(BMPFile);

			goto TEXTURE_LOAD_START;
		}
	}

	return;

TEXTURE_LOAD_START:
	//create texture resource
	ID3D12Resource* texture = nullptr, * uploadbuff = nullptr;
	std::unique_ptr<uint8_t[]> ddsData;
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	texture = CreateTextureResourceFromDDSFile(gd.pDevice, gd.gpucmd, DDSName, &uploadbuff, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	texture->QueryInterface<ID3D12Resource2>(&resource);
	this->CurrentState_InCommandWriteLine = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	// success.
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = Format;
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MipLevels = 1;

	//pGPU = resource->GetGPUVirtualAddress();

	if (ImmortalShaderVisible == false) {
		int srvIndex = gd.TextureDescriptorAllotter.Alloc();
		if (srvIndex >= 0)
		{
			descindex.Set(false, srvIndex);
			gd.pDevice->CreateShaderResourceView(resource, &SRVDesc, descindex.hCreation.hcpu);
		}
		else
		{
			resource->Release();
			resource = nullptr;
			return;
		}
	}
	else {
		bool b = gd.ShaderVisibleDescPool.ImmortalAlloc(&descindex, 1);
		if (b) {
			gd.pDevice->CreateShaderResourceView(resource, &SRVDesc, descindex.hCreation.hcpu);
		}
		else {
			resource->Release();
			resource = nullptr;
			return;
		}
	}
}

void GPUResource::UpdateTextureForWrite(ID3D12Resource* pSrcTexResource)
{
	const DWORD MAX_SUB_RESOURCE_NUM = 32;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint[MAX_SUB_RESOURCE_NUM] = {};
	UINT Rows[MAX_SUB_RESOURCE_NUM] = {};
	UINT64 RowSize[MAX_SUB_RESOURCE_NUM] = {};
	UINT64 TotalBytes = 0;

	D3D12_RESOURCE_DESC Desc = resource->GetDesc();
	if (Desc.MipLevels > (UINT)_countof(Footprint))
		__debugbreak();

	gd.pDevice->GetCopyableFootprints(&Desc, 0, Desc.MipLevels, 0, Footprint, Rows,
		RowSize, &TotalBytes);

	if (FAILED(gd.gpucmd.Reset()))
		__debugbreak();

	if (CurrentState_InCommandWriteLine != D3D12_RESOURCE_STATE_COPY_DEST) {
		this->AddResourceBarrierTransitoinToCommand(gd.gpucmd, D3D12_RESOURCE_STATE_COPY_DEST);
	}

	for (DWORD i = 0; i < Desc.MipLevels; ++i) {
		D3D12_TEXTURE_COPY_LOCATION destLocation = {};
		destLocation.PlacedFootprint = Footprint[i];
		destLocation.pResource = resource;
		destLocation.SubresourceIndex = i;
		destLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

		D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
		srcLocation.PlacedFootprint = Footprint[i];
		srcLocation.pResource = pSrcTexResource;
		srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

		gd.gpucmd->CopyTextureRegion(&destLocation, 0, 0, 0, &srcLocation, nullptr);
	}

	this->AddResourceBarrierTransitoinToCommand(gd.gpucmd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	gd.gpucmd.Close();
	gd.gpucmd.Execute();
	gd.gpucmd.WaitGPUComplete();
}

void GPUResource::Release()
{
	if (resource) {
		dbgc[0] += 1;
		if (dbgc[0] == 385) {
			__debugbreak();
		}
		dbglog2(L"GPUResource %llx released. dbgc0 = %d \n", resource->GetGPUVirtualAddress(), dbgc[0]);
		resource->Release();
	}
	CurrentState_InCommandWriteLine = D3D12_RESOURCE_STATE_COMMON;
	extraData = 0;
	pGPU = 0;
	descindex.Set(false, 0);
}

void DescriptorAllotter::Init(D3D12_DESCRIPTOR_HEAP_TYPE heapType, D3D12_DESCRIPTOR_HEAP_FLAGS Flags, int Capacity)
{
	D3D12_DESCRIPTOR_HEAP_DESC commonHeapDesc = {};
	commonHeapDesc.NumDescriptors = Capacity;
	commonHeapDesc.Type = heapType;
	commonHeapDesc.Flags = Flags;

	if (FAILED(gd.pDevice->CreateDescriptorHeap(&commonHeapDesc, IID_PPV_ARGS(&pDescHeap))))
	{
		__debugbreak();
	}

	DescriptorSize = gd.pDevice->GetDescriptorHandleIncrementSize(heapType);

	AllocFlagContainer.Init(Capacity);
}

int DescriptorAllotter::Alloc()
{
	int dwIndex = AllocFlagContainer.Alloc();
	return dwIndex;
}

void DescriptorAllotter::Free(int index)
{
	AllocFlagContainer.Free(index);
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorAllotter::GetGPUHandle(int index)
{
	//D3D12_GPU_DESCRIPTOR_HANDLE handle;
	//handle.ptr = pDescHeap->GetGPUDescriptorHandleForHeapStart().ptr + index * DescriptorSize;
	//return handle;
	D3D12_GPU_DESCRIPTOR_HANDLE hgpu;
	if (pDescHeap->GetDesc().Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) {
		hgpu.ptr = pDescHeap->GetGPUDescriptorHandleForHeapStart().ptr + index * DescriptorSize;
		return hgpu;
	}
	else {
		D3D12_GPU_DESCRIPTOR_HANDLE handle;
		handle.ptr = 0;
		return handle;
	}
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllotter::GetCPUHandle(int index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle;
	handle.ptr = pDescHeap->GetCPUDescriptorHandleForHeapStart().ptr + index * DescriptorSize;
	return handle;
}

void ShaderVisibleDescriptorPool::Release()
{
	if (m_pDescritorHeap)
	{
		m_pDescritorHeap->Release();
		m_pDescritorHeap = nullptr;
	}
}

BOOL ShaderVisibleDescriptorPool::Initialize(UINT MaxDescriptorCount)
{
	m_srvImmortalDescriptorSize = 0;
	BOOL bResult = FALSE;
	m_MaxDescriptorCount = MaxDescriptorCount;
	m_srvDescriptorSize = gd.pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// create descriptor heap
	D3D12_DESCRIPTOR_HEAP_DESC commonHeapDesc = {};
	commonHeapDesc.NumDescriptors = m_MaxDescriptorCount;
	commonHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	commonHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	if (FAILED(gd.pDevice->CreateDescriptorHeap(&commonHeapDesc, IID_PPV_ARGS(&m_pDescritorHeap))))
	{
		__debugbreak();
		goto lb_return;
	}
	m_cpuDescriptorHandle = m_pDescritorHeap->GetCPUDescriptorHandleForHeapStart();
	m_gpuDescriptorHandle = m_pDescritorHeap->GetGPUDescriptorHandleForHeapStart();
	bResult = TRUE;
lb_return:
	return bResult;
}

void SVDescPool2::Release()
{
	if (pSVDescHeapForRender)
	{
		pSVDescHeapForRender->Release();
		pSVDescHeapForRender = nullptr;
	}

	if (pNSVDescHeapForCreation)
	{
		pNSVDescHeapForCreation->Release();
		pNSVDescHeapForCreation = nullptr;
	}

	if (NSVDescHeapForCopy)
	{
		NSVDescHeapForCopy->Release();
		NSVDescHeapForCopy = nullptr;
	}
}

BOOL SVDescPool2::Initialize(UINT MaxDescriptorCount)
{
	InitDescArrSiz = 0;
	InitDescArrCap = TextureSRVStart = 64; // 64부터 Desc 배열관리가 시작됨.
	TextureSRVSiz = 0;
	TextureSRVCap = 64;
	MaterialCBVSiz = 0;
	MaterialCBVCap = 64;
	InstancingSRVSiz = 0;
	InstancingSRVCap = 64;

	BOOL bResult = FALSE;
	MaxDescCount = MaxDescriptorCount;

	// create SV descriptor heap
	D3D12_DESCRIPTOR_HEAP_DESC SVDescHeapDesc = {};
	D3D12_DESCRIPTOR_HEAP_DESC NSVDescHeapCreationDesc = {};
	D3D12_DESCRIPTOR_HEAP_DESC NSVDescHeapCopyDesc = {};

	SVDescHeapDesc.NumDescriptors = MaxDescCount;
	SVDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	SVDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	if (FAILED(gd.pDevice->CreateDescriptorHeap(&SVDescHeapDesc, IID_PPV_ARGS(&pSVDescHeapForRender))))
	{
		__debugbreak();
		goto lb_return;
	}
	SVDescHeapRenderHandle.hcpu = pSVDescHeapForRender->GetCPUDescriptorHandleForHeapStart();
	SVDescHeapRenderHandle.hgpu = pSVDescHeapForRender->GetGPUDescriptorHandleForHeapStart();

	// create NSV descriptor heap For Res Desc Creation
	NSVDescHeapCreationDesc.NumDescriptors = MaxDescCount;
	NSVDescHeapCreationDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	NSVDescHeapCreationDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	if (FAILED(gd.pDevice->CreateDescriptorHeap(&NSVDescHeapCreationDesc, IID_PPV_ARGS(&pNSVDescHeapForCreation))))
	{
		__debugbreak();
		goto lb_return;
	}
	NSVDescHeapCreationHandle.hcpu = pNSVDescHeapForCreation->GetCPUDescriptorHandleForHeapStart();

	// create NSV descriptor heap For Copy Desc
	NSVDescHeapCopyDesc.NumDescriptors = MaxDescCount;
	NSVDescHeapCopyDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	NSVDescHeapCopyDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	if (FAILED(gd.pDevice->CreateDescriptorHeap(&NSVDescHeapCopyDesc, IID_PPV_ARGS(&NSVDescHeapForCopy))))
	{
		__debugbreak();
		goto lb_return;
	}
	NSVDescHeapCopyHandle.hcpu = NSVDescHeapForCopy->GetCPUDescriptorHandleForHeapStart();

	bResult = TRUE;
lb_return:
	return bResult;
}

BOOL SVDescPool2::DynamicAlloc(DescHandle* pHandle, UINT DescriptorCount)
{
	BOOL bResult = FALSE;
	if (ImmortalSize + DynamicSize + DescriptorCount > MaxDescCount)
		return bResult;
	DescIndex index;
	index.Set(true, ImmortalSize + DynamicSize);
	*pHandle = index.hRender;
	DynamicSize += DescriptorCount;
	bResult = TRUE;
	return bResult;
}

BOOL SVDescPool2::ImmortalAlloc(DescIndex* pindex, UINT DescriptorCount)
{
	isImmortalChange = true;
	BOOL bResult = FALSE;
	if (InitDescArrSiz + DescriptorCount > InitDescArrCap)
	{
		ExpendDescStructure(2 * InitDescArrCap, TextureSRVCap, MaterialCBVCap, InstancingSRVCap);
	}
	pindex->Set(true, InitDescArrSiz);
	InitDescArrSiz += DescriptorCount;
	bResult = TRUE;
	return bResult;
}

BOOL SVDescPool2::ImmortalAlloc_TextureSRV(DescIndex* pindex, UINT DescriptorCount)
{
	isImmortalChange = true;
	BOOL bResult = FALSE;
	if (TextureSRVSiz + DescriptorCount > TextureSRVCap)
	{
		ExpendDescStructure(InitDescArrCap, 2 * TextureSRVCap, MaterialCBVCap, InstancingSRVCap);
	}
	pindex->Set(true, InitDescArrCap + TextureSRVSiz);
	TextureSRVSiz += DescriptorCount;
	bResult = TRUE;
	return bResult;
}

BOOL SVDescPool2::ImmortalAlloc_MaterialCBV(DescIndex* pindex, UINT DescriptorCount)
{
	isImmortalChange = true;
	BOOL bResult = FALSE;
	if (MaterialCBVSiz + DescriptorCount > MaterialCBVCap)
	{
		ExpendDescStructure(InitDescArrCap, TextureSRVCap, 2 * MaterialCBVCap, InstancingSRVCap);
	}
	pindex->Set(true, InitDescArrCap + TextureSRVCap + MaterialCBVSiz);
	MaterialCBVSiz += DescriptorCount;
	bResult = TRUE;
	return bResult;
}

BOOL SVDescPool2::ImmortalAlloc_InstancingSRV(DescIndex* pindex, UINT DescriptorCount)
{
	isImmortalChange = true;
	BOOL bResult = FALSE;
	if (InstancingSRVSiz + DescriptorCount > InstancingSRVCap)
	{
		ExpendDescStructure(InitDescArrCap, TextureSRVCap, MaterialCBVCap, 2 * InstancingSRVCap);
	}
	pindex->Set(true, InitDescArrCap + TextureSRVCap + MaterialCBVCap + InstancingSRVSiz);
	InstancingSRVSiz += DescriptorCount;
	bResult = TRUE;
	return bResult;
}

void SVDescPool2::ExpendDescStructure(ui32 newInitDescArrCap, ui32 newTextureSRVCap, ui32 newMaterialCBVCap, ui32 newInstancingSRVCap)
{
	D3D12_DESCRIPTOR_HEAP_TYPE descheaptype = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	//source meta
	D3D12_CPU_DESCRIPTOR_HANDLE SourceHandleArr[1];
	UINT SourceSizeArr[1];
	SourceHandleArr[0] = NSVDescHeapCreationHandle[0].hcpu;
	SourceSizeArr[0] = ImmortalSize;

	//dest meta
	D3D12_CPU_DESCRIPTOR_HANDLE DestHandleArr[5]; // init, tex, material, instancing, dynamic
	UINT DestSizeArr[5];
	DestHandleArr[0] = NSVDescHeapCopyHandle[0].hcpu;
	DestSizeArr[0] = InitDescArrCap;
	DestHandleArr[1] = NSVDescHeapCopyHandle[newInitDescArrCap].hcpu;
	DestSizeArr[1] = TextureSRVCap;
	DestHandleArr[2] = NSVDescHeapCopyHandle[newInitDescArrCap + newTextureSRVCap].hcpu;
	DestSizeArr[2] = MaterialCBVCap;
	DestHandleArr[3] = NSVDescHeapCopyHandle[newInitDescArrCap + newTextureSRVCap + newMaterialCBVCap].hcpu;
	DestSizeArr[3] = InstancingSRVCap;
	DestHandleArr[4] = NSVDescHeapCopyHandle[newInitDescArrCap + newTextureSRVCap + newMaterialCBVCap + newInstancingSRVCap].hcpu;
	DestSizeArr[4] = DynamicSize;
	//copy to NSVDescHeap
	gd.pDevice->CopyDescriptors(5, DestHandleArr, DestSizeArr, 1, SourceHandleArr, SourceSizeArr, descheaptype);

	ui32 beforeMatTexturesSRVStart = TextureSRVStart;
	ui32 afterMatTexturesSRVStart = newInitDescArrCap;
	ui32 TexturesSRVdelta = afterMatTexturesSRVStart - beforeMatTexturesSRVStart;

	ui32 beforeMatCBVStart = TextureSRVStart + TextureSRVCap;
	ui32 afterMatCBVStart = newInitDescArrCap + newTextureSRVCap;
	ui32 MatCBVdelta = afterMatCBVStart - beforeMatCBVStart;
	for (int i = 0; i < game.MaterialTable.size(); ++i) {
		Material& mat = game.MaterialTable[i];
		mat.TextureSRVTableIndex.index += TexturesSRVdelta;
		mat.CB_Resource.descindex.index += MatCBVdelta;
	}

	ui32 beforeInstancingSRVStart = TextureSRVStart + TextureSRVCap + MaterialCBVCap;
	ui32 afterInstancingSRVStart = newInitDescArrCap + newTextureSRVCap + newMaterialCBVCap;
	ui32 InstancingSRVdelta = afterInstancingSRVStart - beforeInstancingSRVStart;
	for (int i = 0; i < game.MeshTable.size(); ++i) {
		Mesh* mesh = game.MeshTable[i];
		for (int k = 0; k < mesh->subMeshNum; ++k) {
			mesh->InstanceData[k].InstancingSRVIndex.index += InstancingSRVdelta;
		}
	}

	InitDescArrCap = newInitDescArrCap;
	TextureSRVCap = newTextureSRVCap;
	MaterialCBVCap = newMaterialCBVCap;
	InstancingSRVCap = newInstancingSRVCap;

	SourceHandleArr[0] = NSVDescHeapCopyHandle[0].hcpu;
	SourceSizeArr[0] = ImmortalSize;
	DestHandleArr[0] = NSVDescHeapCreationHandle[0].hcpu;
	DestSizeArr[0] = ImmortalSize;
	gd.pDevice->CopyDescriptors(1, DestHandleArr, DestSizeArr, 1, SourceHandleArr, SourceSizeArr, descheaptype);

	DescIndex dummyTexSRV = DescIndex(true, TextureSRVStart + TextureSRVSiz);
	for (int i = 0; i < TextureSRVCap - TextureSRVSiz; ++i) {
		gd.pDevice->CopyDescriptorsSimple(1, dummyTexSRV.hCreation.hcpu, game.TextureTable[0]->descindex.hCreation.hcpu, descheaptype);
		dummyTexSRV.index += 1;
	}

	DescIndex dummyMatCBV = DescIndex(true, TextureSRVStart + TextureSRVCap + MaterialCBVSiz);
	for (int i = 0; i < MaterialCBVCap - MaterialCBVSiz; ++i) {
		gd.pDevice->CopyDescriptorsSimple(1, dummyMatCBV.hCreation.hcpu, game.MaterialTable[0].CB_Resource.descindex.hCreation.hcpu, descheaptype);
		dummyMatCBV.index += 1;
	}

	DescIndex dummyInstancingSRV = DescIndex(true, TextureSRVStart + TextureSRVCap + MaterialCBVCap + InstancingSRVSiz);
	for (int i = 0; i < InstancingSRVCap - InstancingSRVSiz; ++i) {
		gd.pDevice->CopyDescriptorsSimple(1, dummyInstancingSRV.hCreation.hcpu, game.MeshTable[0]->InstanceData[0].InstancingSRVIndex.hCreation.hcpu, descheaptype);
		dummyInstancingSRV.index += 1;
	}

	Material::InitMaterialStructuredBuffer(true);

	game.MyPBRShader1->ReBuild_Shader(ShaderType::InstancingWithShadow);
}

void SVDescPool2::BakeImmortalDesc()
{
	if (isImmortalChange) {
		D3D12_DESCRIPTOR_HEAP_TYPE descheaptype = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

		// copy to SV
		D3D12_CPU_DESCRIPTOR_HANDLE SourceHandleArr[1];
		UINT SourceSizeArr[1];
		SourceHandleArr[0] = NSVDescHeapCreationHandle[0].hcpu;
		SourceSizeArr[0] = ImmortalSize;
		D3D12_CPU_DESCRIPTOR_HANDLE DestHandleArr[1];
		UINT DestSizeArr[1];
		DestHandleArr[0] = SVDescHeapRenderHandle[0].hcpu;
		DestSizeArr[0] = ImmortalSize;
		gd.pDevice->CopyDescriptors(1, DestHandleArr, DestSizeArr, 1, SourceHandleArr, SourceSizeArr, descheaptype);
		Material::InitMaterialStructuredBuffer();

		isImmortalChange = false;
	}
}

bool SVDescPool2::IncludeHandle(D3D12_CPU_DESCRIPTOR_HANDLE hcpu)
{
	return (pSVDescHeapForRender->GetCPUDescriptorHandleForHeapStart().ptr <= hcpu.ptr) && (hcpu.ptr < pSVDescHeapForRender->GetCPUDescriptorHandleForHeapStart().ptr + gd.CBVSRVUAVSize * MaxDescCount);
}

void SVDescPool2::DynamicReset()
{
	DynamicSize = 0;
}

BOOL ShaderVisibleDescriptorPool::AllocDescriptorTable(D3D12_CPU_DESCRIPTOR_HANDLE* pOutCPUDescriptor, D3D12_GPU_DESCRIPTOR_HANDLE* pOutGPUDescriptor, UINT DescriptorCount)
{
	BOOL bResult = FALSE;
	if (m_AllocatedDescriptorCount + DescriptorCount > m_MaxDescriptorCount)
	{
		return bResult;
	}
	UINT offset = m_AllocatedDescriptorCount + DescriptorCount;

	pOutCPUDescriptor->ptr = m_cpuDescriptorHandle.ptr + m_AllocatedDescriptorCount * m_srvDescriptorSize;
	pOutGPUDescriptor->ptr = m_gpuDescriptorHandle.ptr + m_AllocatedDescriptorCount * m_srvDescriptorSize;
	m_AllocatedDescriptorCount += DescriptorCount;
	bResult = TRUE;
	return bResult;
}

BOOL ShaderVisibleDescriptorPool::ImmortalAllocDescriptorTable(D3D12_CPU_DESCRIPTOR_HANDLE* pOutCPUDescriptor, D3D12_GPU_DESCRIPTOR_HANDLE* pOutGPUDescriptor, UINT DescriptorCount)
{
	BOOL bResult = FALSE;
	if (m_AllocatedDescriptorCount + DescriptorCount > m_MaxDescriptorCount)
	{
		return bResult;
	}
	UINT offset = m_AllocatedDescriptorCount + DescriptorCount;

	pOutCPUDescriptor->ptr = m_cpuDescriptorHandle.ptr + m_AllocatedDescriptorCount * m_srvDescriptorSize;
	pOutGPUDescriptor->ptr = m_gpuDescriptorHandle.ptr + m_AllocatedDescriptorCount * m_srvDescriptorSize;
	m_AllocatedDescriptorCount += DescriptorCount;
	m_srvImmortalDescriptorSize = m_AllocatedDescriptorCount;
	bResult = TRUE;
	return bResult;
}

bool ShaderVisibleDescriptorPool::IncludeHandle(D3D12_CPU_DESCRIPTOR_HANDLE hcpu)
{
	return (m_pDescritorHeap->GetCPUDescriptorHandleForHeapStart().ptr <= hcpu.ptr) && (hcpu.ptr < m_pDescritorHeap->GetCPUDescriptorHandleForHeapStart().ptr + gd.CBV_SRV_UAV_Desc_IncrementSiz * m_MaxDescriptorCount);
}

void ShaderVisibleDescriptorPool::Reset()
{
	m_AllocatedDescriptorCount = m_srvImmortalDescriptorSize;
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
}

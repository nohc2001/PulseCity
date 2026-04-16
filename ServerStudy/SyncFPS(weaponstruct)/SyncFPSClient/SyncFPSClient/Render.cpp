#include "stdafx.h"
#include "Render.h"
#include "Game.h"
#include "main.h"
#include "stb_image.h"

GlobalDevice gd;
extern Game game;
extern int dbgc[128];

unordered_map<string, int> Shape::StrToShapeIndex;
vector<Shape> Shape::ShapeTable;
vector<string> Shape::ShapeStrTable;

#pragma region Global_InitCode

void GPUCmd::SetShader(Shader* shader, ShaderType reg) {
	shader->Add_RegisterShaderCommand(*this, reg);
}

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

bool SDFTextPageTextureBuffer::PushSDFText(wchar_t c, ui16 width, ui16 height, char* copybuffer) {
	SDFTextSection* sdftextSec = new SDFTextSection();
	auto f = SDFSectionMap.find(c);
	if (f == SDFSectionMap.end()) {
		int postSX = 0;
		int postSY = 0;
		int postHight = 0;
		if (present_StartX + width >= MaxWidth) {
			if (present_StartY + height >= MaxHeight) {
				return false;
			}
			postSX = 0;
			postSY = present_StartY + present_height;
			present_height = 0;
			postHight = max(height, present_height);

			sdftextSec->c = c;
			sdftextSec->sx = 0;
			sdftextSec->sy = postSY;
			sdftextSec->width = width;
			sdftextSec->height = height;

			present_StartX = postSX + width;
			present_StartY = postSY;
			present_height = postHight;
		}
		else {
			postSX = present_StartX + width;
			postSY = present_StartY;
			postHight = max(height, present_height);

			sdftextSec->c = c;
			sdftextSec->sx = present_StartX;
			sdftextSec->sy = present_StartY;
			sdftextSec->width = width;
			sdftextSec->height = height;

			present_StartX = postSX;
			present_StartY = postSY;
			present_height = postHight;
		}

		if (present_StartY + height >= MaxHeight) {
			return false;
		}

		sdftextSec->pageindex = pageindex;
		if (copybuffer != nullptr) {
			for (int iy = sdftextSec->sy; iy < sdftextSec->sy + sdftextSec->height;++iy) {
				for (int ix = sdftextSec->sx; ix < sdftextSec->sx + sdftextSec->width;++ix) {
					int iix = ix - sdftextSec->sx;
					int iiy = iy - sdftextSec->sy;
					data[iy][ix] = copybuffer[width * iiy + iix];
				}
			}
		}
		
		sectionCount += 1;
		SDFSectionMap.insert(pair<wchar_t, SDFTextSection*>(c, sdftextSec));
		return true;
	}
	return true; // 이미 텍스쳐 영역이 있을 경우
}

void SDFTextPageTextureBuffer::BakeSDF() {
	if (isDynamicTexture && uploadedSectionCount != sectionCount) {
		if (DefaultTextureBuffer.resource == nullptr) {
			DefaultTextureBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_DIMENSION_TEXTURE2D, MaxWidth, MaxHeight, DXGI_FORMAT_R8_UNORM);

			int index = gd.TextureDescriptorAllotter.Alloc();
			SDFTextureSRV.Set(false, index, 'n');
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Format = DXGI_FORMAT_R8_UNORM;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Texture2D.MipLevels = 1;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.PlaneSlice = 0;
			srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
			gd.pDevice->CreateShaderResourceView(DefaultTextureBuffer.resource, &srvDesc, SDFTextureSRV.hCreation.hcpu);
		}

		D3D12_RESOURCE_DESC Desc = DefaultTextureBuffer.resource->GetDesc();
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint;
		UINT	Rows = 0;
		UINT64	RowSize = 0;
		UINT64	TotalBytes = 0;

		gd.pDevice->GetCopyableFootprints(&Desc, 0, 1, 0, &Footprint, &Rows, &RowSize, &TotalBytes);

		D3D12_RANGE writeRange;
		writeRange.Begin = 0;
		writeRange.End = 0;

		if (UploadTextureBuffer.resource == nullptr) {
			//return size of buffer for uploading data. (D3dx12.h)
			UINT64 uploadBufferSize = gd.GetRequiredIntermediateSize(DefaultTextureBuffer.resource, 0, 1);
			UploadTextureBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, uploadBufferSize, 1, DXGI_FORMAT_UNKNOWN);
		}

		HRESULT hr = UploadTextureBuffer.resource->Map(0, &writeRange, reinterpret_cast<void**>(&mappedBuffer));

		const BYTE* pSrc = (BYTE*)data;
		BYTE* pDest = (BYTE*)mappedBuffer;
		int mul = 1;

		// fix : 가능하면 부분적으로만 업데이트 하도록도 하고 싶다. 현재 전체를 다 업데이트 한다.
		for (UINT y = 0; y < MaxHeight; y++)
		{
			memcpy(pDest, pSrc, MaxWidth * mul);
			pSrc += (MaxWidth * mul);
			pDest += Footprint.Footprint.RowPitch;
		}
		// Unmap
		UploadTextureBuffer.resource->Unmap(0, nullptr);

		DefaultTextureBuffer.UpdateTextureForWrite(UploadTextureBuffer.resource);
		uploadedSectionCount = sectionCount;
	}
}

#pragma region GPUResourceCode

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
	SRVDesc.Texture2D.MipLevels = mipmapLevel-1;
	SRVDesc.Texture2D.MostDetailedMip = 0;
	SRVDesc.Texture2D.PlaneSlice = 0;
	SRVDesc.Texture2D.ResourceMinLODClamp = 0.0f;
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

	if (gd.gpucmd.isClose) {
		gd.gpucmd.Reset();
	}

	/*if (FAILED(gd.gpucmd.Reset()))
		__debugbreak();*/

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
#ifdef DEVELOPMODE_DBG_GPURESOURCES
		dbgc[0] += 1;
		dbglog2(L"GPUResource %llx released. dbgc0 = %d \n", resource->GetGPUVirtualAddress(), dbgc[0]);
#endif
		resource->Release();
	}
	CurrentState_InCommandWriteLine = D3D12_RESOURCE_STATE_COMMON;
	extraData = 0;
	pGPU = 0;
	descindex.Set(false, 0);
}

#pragma endregion

#pragma region DescriptorAllotterCode

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

#pragma endregion

#pragma region ShaderVisibleDescHeapCode

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
	bool isTextureUpdate = false;
	bool isMaterialUpdate = false;

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
	for (int i = 0; i < game.RenderMaterialTable.size(); ++i) {
		Material* mat = game.RenderMaterialTable[i];
		mat->TextureSRVTableIndex.index += TexturesSRVdelta;
		mat->CB_Resource.descindex.index += MatCBVdelta;
	}

	ui32 beforeInstancingSRVStart = TextureSRVStart + TextureSRVCap + MaterialCBVCap;
	ui32 afterInstancingSRVStart = newInitDescArrCap + newTextureSRVCap + newMaterialCBVCap;
	ui32 InstancingSRVdelta = afterInstancingSRVStart - beforeInstancingSRVStart;
	for (int i = 0; i < game.RenderInstancingTable.size(); ++i) {
		game.RenderInstancingTable[i]->InstancingSRVIndex.index += InstancingSRVdelta;
	}

	InitDescArrCap = newInitDescArrCap;
	isTextureUpdate = (TextureSRVCap != newTextureSRVCap);
	TextureSRVCap = newTextureSRVCap;
	isMaterialUpdate = (MaterialCBVCap != newMaterialCBVCap);
	MaterialCBVCap = newMaterialCBVCap;
	InstancingSRVCap = newInstancingSRVCap;

	SourceHandleArr[0] = NSVDescHeapCopyHandle[0].hcpu;
	SourceSizeArr[0] = ImmortalSize;
	DestHandleArr[0] = NSVDescHeapCreationHandle[0].hcpu;
	DestSizeArr[0] = ImmortalSize;
	gd.pDevice->CopyDescriptors(1, DestHandleArr, DestSizeArr, 1, SourceHandleArr, SourceSizeArr, descheaptype);

	DescIndex dummyTexSRV = DescIndex(true, TextureSRVStart + TextureSRVSiz);
	if (game.RenderTextureTable.size() > 0) {
		for (int i = 0; i < TextureSRVCap - TextureSRVSiz; ++i) {
			gd.pDevice->CopyDescriptorsSimple(1, dummyTexSRV.hRender.hcpu, game.RenderTextureTable[0]->descindex.hCreation.hcpu, descheaptype);
			dummyTexSRV.index += 1;
		}
	}

	DescIndex dummyMatCBV = DescIndex(true, TextureSRVStart + TextureSRVCap + MaterialCBVSiz);
	if (game.RenderMaterialTable.size() > 0) {
		for (int i = 0; i < MaterialCBVCap - MaterialCBVSiz; ++i) {
			gd.pDevice->CopyDescriptorsSimple(1, dummyMatCBV.hRender.hcpu, game.RenderMaterialTable[0]->CB_Resource.descindex.hCreation.hcpu, descheaptype);
			dummyMatCBV.index += 1;
		}
	}

	DescIndex dummyInstancingSRV = DescIndex(true, TextureSRVStart + TextureSRVCap + MaterialCBVCap + InstancingSRVSiz);
	if (game.RenderInstancingTable.size() > 0) {
		for (int i = 0; i < InstancingSRVCap - InstancingSRVSiz; ++i) {
			gd.pDevice->CopyDescriptorsSimple(1, dummyInstancingSRV.hRender.hcpu, game.RenderInstancingTable[0]->InstancingSRVIndex.hCreation.hcpu, descheaptype);
			dummyInstancingSRV.index += 1;
		}
	}

	// 머터리얼의 용량이 달라지면 인스턴싱과 Raytracing에 쓰일 StructuredBuffer의 용량을 재조정한다.
	if (isMaterialUpdate) {
		Material::InitMaterialStructuredBuffer(true);
	}

	// 텍스쳐 용량이 달라짐에 따라 셰이더 코드의 매크로가 달라질 수 있도록 다시 빌드한다.
	if (isTextureUpdate) {
		game.MyPBRShader1->ReBuild_Shader(ShaderType::InstancingWithShadow);
		game.MyRayTracingShader->ReInit();
	}
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

void SVDescPool2::ImmortalReset_ExcludeInit() {
	TextureSRVSiz = 0;
	TextureSRVCap = 64;
	MaterialCBVSiz = 0;
	MaterialCBVCap = 64;
	InstancingSRVSiz = 0;
	InstancingSRVCap = 64;
}

#pragma endregion

#pragma region GlobalDeviceCode

void GlobalDevice::Factory_Adaptor_Output_Init()
{
	HRESULT hr;
	IDXGIInfoQueue* DebugInterface;
	debugDXGI = false;
#ifdef DEVELOPMODE_PIX_DEBUGING
	LoadLibrary(L"C:/Program Files/Microsoft PIX/2602.25/WinPixGpuCapturer.dll");
#endif

	UINT nDXGIFactoryFlags = 0;

#if defined(_DEBUG) || defined(RELEASE_GPUDEBUG)
	ID3D12Debug* pd3dDebugController = NULL;

	hr = D3D12GetDebugInterface(__uuidof(ID3D12Debug), (void
		**)&pd3dDebugController);

	if (pd3dDebugController)
	{
		pd3dDebugController->EnableDebugLayer();

		// GPU Validation 끄기 - Device가 문제 생기면 바로 삭제하도록 한다?
		ComPtr<ID3D12Debug1> debug1;
		if (SUCCEEDED(pd3dDebugController->QueryInterface<ID3D12Debug1>(&debug1)))
		{
			debug1->SetEnableGPUBasedValidation(FALSE);
		}

		pd3dDebugController->Release();
	}
	else
	{
		throw "WARNING: Direct3D Debug Device is not available";
	}

	// Debug Interface

	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&DebugInterface)))) {
		debugDXGI = true;
	}
	nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
	if (!debugDXGI)
	{
		hr = CreateDXGIFactory2(nDXGIFactoryFlags, IID_PPV_ARGS(&pFactory));
		if (SUCCEEDED(hr)) goto DXGI_FACTORY_INIT_END;

		hr = CreateDXGIFactory1(IID_PPV_ARGS(&pFactory));
		if (SUCCEEDED(hr)) goto DXGI_FACTORY_INIT_END;

		hr = CreateDXGIFactory(IID_PPV_ARGS(&pFactory));
		if (SUCCEEDED(hr)) goto DXGI_FACTORY_INIT_END;
		else {
			throw "CreateDXGIFactory Failed.";
		}
	}
	else {
		// sus 버전에 따라 안될 수 있으니 예외처리 필요.
		hr = ::CreateDXGIFactory2(nDXGIFactoryFlags, __uuidof(IDXGIFactory4), (void
			**)&pFactory);
	}

	if (FAILED(hr)) {
		OutputDebugStringA("[ERROR] : Create Factory Error.\n");
		return;
	}

DXGI_FACTORY_INIT_END:
#if defined(_DEBUG)
	if (debugDXGI && SUCCEEDED(hr)) {
		DebugInterface->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
		DebugInterface->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
	}
	else if (debugDXGI) {
		throw "DXGI_CREATE_FACTORY_DEBUG CreateDXGIFactory2 Fail.";
	}
#endif

	IDXGIOutput* output = nullptr;
	UINT numModes = 0;
	vector<DXGI_MODE_DESC> modeList;

	constexpr D3D_FEATURE_LEVEL FeatureLevelPriority[11] = {
		D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1, D3D_FEATURE_LEVEL_1_0_CORE
	};

	//Adapter Version Check
	IDXGIAdapter1* adapter = NULL;
	IDXGIAdapter4* pd3dAdapter4 = NULL;
	IDXGIAdapter3* pd3dAdapter3 = NULL;
	IDXGIAdapter2* pd3dAdapter2 = NULL;
	pFactory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter));
	DXGI_ADAPTER_DESC1 dxgiAdapterDesc;
	adapter->GetDesc1(&dxgiAdapterDesc);
	adapter->QueryInterface(__uuidof(IDXGIAdapter4), (void**)&pd3dAdapter4);
	if (pd3dAdapter4 != nullptr) {
		adapterVersion = 4;
		adapter->Release();
		adapter = NULL;
		goto DXGI_ADAPTER_VERSION_CHECK;
	}
	adapter->QueryInterface(__uuidof(IDXGIAdapter3), (void**)&pd3dAdapter3);
	if (pd3dAdapter3 != nullptr) {
		adapterVersion = 3;
		adapter->Release();
		adapter = NULL;
		goto DXGI_ADAPTER_VERSION_CHECK;
	}
	adapter->QueryInterface(__uuidof(IDXGIAdapter2), (void**)&pd3dAdapter2);
	if (pd3dAdapter2 != nullptr) {
		adapterVersion = 2;
		adapter->Release();
		adapter = NULL;
		goto DXGI_ADAPTER_VERSION_CHECK;
	}
	adapterVersion = 1;
	adapter->Release();
	adapter = NULL;

DXGI_ADAPTER_VERSION_CHECK:

	//실제 디바이스 생성은 안함.
	// 어디까지 지원되는지 테스트 & 어댑터 선택
	for (int i = 0; i < 11; ++i) {
		minFeatureLevel = FeatureLevelPriority[i];
		bool keepLoop = true;
		for (UINT adapterID = 0; keepLoop; ++adapterID) {
			keepLoop = DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapterByGpuPreference(adapterID, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter));
			if (keepLoop == false) break;

			DXGI_ADAPTER_DESC1 desc;
			hr = adapter->GetDesc1(&desc);
			if (FAILED(hr)) {
				throw "adapter GetDesc1() Failed.";
			}

			//Code From DirectX RaytracingHelloWorld Sample Start
			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				// Don't select the Basic Render Driver adapter.
				continue;
			}
			//Code From DirectX RaytracingHelloWorld Sample End

			hr = adapter->EnumOutputs(0, &output);
			if (SUCCEEDED(hr)) {
				pOutputAdapter = adapter;

				if (pSelectedAdapter) {
					goto DXGI_FINISH_SELECT_ADAPTER;
				}
			}

			hr = D3D12CreateDevice(adapter, minFeatureLevel, _uuidof(ID3D12Device), nullptr);
			if (SUCCEEDED(hr) && pSelectedAdapter == nullptr)
			{
				pSelectedAdapter = adapter;
				adapterIndex = adapterID;
				wcscpy_s(adapterDescription, desc.Description);
#ifdef _DEBUG
				wchar_t buff[256] = {};
				swprintf_s(buff, L"Direct3D Adapter (%u): VID:%04X, PID:%04X - %ls\n", adapterID, desc.VendorId, desc.DeviceId, desc.Description);
				OutputDebugStringW(buff);
#endif
				if (pOutputAdapter) {
					goto DXGI_FINISH_SELECT_ADAPTER;
				}
			}
			else {
				adapter->Release();
				adapter = NULL;
			}
		}
	}

	// if adapter is not selected Try WARP12(CPU Simulate) instead
	if (FAILED(pFactory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))))
	{
		throw "WARP12 not available. Enable the 'Graphics Tools' optional feature";
	}

DXGI_FINISH_SELECT_ADAPTER:

	// 전체화면 모드로 전환 가능한 해상도를 얻기 위한 작업

	//AI Code Start <Microsoft Copilot>
	if (output != nullptr) {
		hr = output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, nullptr);
		if (FAILED(hr)) throw "GetDisplayModeList Error";
		modeList.reserve(numModes);
		modeList.resize(numModes);
		hr = output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, modeList.data());
		if (FAILED(hr)) throw "GetDisplayModeList Error";
	}
	//AI Code End <Microsoft Copilot>

	for (int i = 0; i < numModes; ++i) {
		ResolutionStruct rs;
		rs.width = modeList[i].Width;
		rs.height = modeList[i].Height;
		bool isExist = false;
		for (int k = 0; k < EnableFullScreenMode_Resolusions.size(); ++k) {
			ResolutionStruct rs2 = EnableFullScreenMode_Resolusions[k];
			if (rs2.width == rs.width && rs2.height == rs.height) {
				isExist = true;
				break;
			}
		}
		if (!isExist) {
			EnableFullScreenMode_Resolusions.push_back(rs);
		}
	}

	hr = D3D12CreateDevice(pSelectedAdapter, minFeatureLevel, _uuidof(ID3D12Device), (void**)&pDevice);
	if (FAILED(hr)) throw "D3D12CreateDevice Failed.";

	Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
	if (SUCCEEDED(pDevice->QueryInterface(IID_PPV_ARGS(&infoQueue))))
	{
		// 에러 발생 시 자동 브레이크
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);

		//// 디바이스 리무브 관련 메시지에만 브레이크를 걸고 싶다면
		infoQueue->SetBreakOnID(D3D12_MESSAGE_ID_DEVICE_REMOVAL_PROCESS_AT_FAULT, TRUE);
	}

	/*ID3D12DeviceRemovedExtendedDataSettings* dredSettings = nullptr;
	pDevice->QueryInterface(IID_PPV_ARGS(&dredSettings));
	dredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
	dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
	dredSettings->Release();*/
}

void GlobalDevice::Init()
{
	TIMER_STATICINIT();

	HRESULT hr;

	font_filename[0] = "consola.ttf"; // english
	font_filename[1] = "malgunbd.ttf"; // korean
	for (int i = 0; i < FontCount; ++i) {
		uint8_t condition_variable = 0;
		int8_t error = TTFFontParser::parse_file(font_filename[i].c_str(), &font_data[i], &font_parsed, &condition_variable);
	}
	addSDFTextureStack.reserve(32);

	constexpr D3D_FEATURE_LEVEL FeatureLevelPriority[11] = {
		D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1, D3D_FEATURE_LEVEL_1_0_CORE
	};

	IDXGIAdapter1* pd3dAdapter1 = NULL;
	for (int k = 0; k < 11; ++k) {
		// Find GPU that support DirectX 12_2
		for (UINT i = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(i, &pd3dAdapter1); i++)
		{
			IDXGIAdapter4* pd3dAdapter4 = NULL;
			DXGI_ADAPTER_DESC1 dxgiAdapterDesc;
			pd3dAdapter1->GetDesc1(&dxgiAdapterDesc);
			pd3dAdapter1->QueryInterface(__uuidof(IDXGIAdapter4), (void**)&pd3dAdapter4);
			if (dxgiAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
			if (SUCCEEDED(D3D12CreateDevice(pd3dAdapter4, FeatureLevelPriority[k],
				_uuidof(ID3D12Device), (void**)&pDevice))) {

				pd3dAdapter4->Release();
				pd3dAdapter1->Release();
				goto GlobalDeviceInit_InitMultisamplingVariable;
				break;
			}
			pd3dAdapter4->Release();
			pd3dAdapter1->Release();
		}
	}

	if (!pd3dAdapter1)
	{
		OutputDebugStringA("[ERROR] : there is no GPU that support DirectX.\n");
		return;
	}

GlobalDeviceInit_InitMultisamplingVariable:

	//Get Desc Increment Siz
	RTVSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	DSVSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	CBVSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	SamplerDescSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS d3dMsaaQualityLevels;
	d3dMsaaQualityLevels.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	d3dMsaaQualityLevels.SampleCount = 4; //Msaa4x multi sampling.
	d3dMsaaQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	d3dMsaaQualityLevels.NumQualityLevels = 0;
	pDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&d3dMsaaQualityLevels, sizeof(D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS));
	m_nMsaa4xQualityLevels = d3dMsaaQualityLevels.NumQualityLevels;
	m_bMsaa4xEnable = (m_nMsaa4xQualityLevels > 1) ? true : false;
	//when multi sampling quality level is bigger than 1, active MSAA.

	hr = pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
		(void**)&pFence);
	FenceValue = 0;
	hFenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	// Create Event Object to Sync with Fence. Event's Init value is FALSE, when Signal call, Event Value change to FALSE

	// Graphic GPUCmd
	gpucmd = GPUCmd(pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);
	hr = gpucmd.Close(); // why?

	// compute GPUCmd
	CScmd = GPUCmd(pDevice, D3D12_COMMAND_LIST_TYPE_COMPUTE);
	hr = CScmd.Close();

	ClientFrameWidth = gd.EnableFullScreenMode_Resolusions[resolutionLevel].width;
	ClientFrameHeight = gd.EnableFullScreenMode_Resolusions[resolutionLevel].height;

	viewportArr[0].Viewport.TopLeftX = 0;
	viewportArr[0].Viewport.TopLeftY = 0;
	viewportArr[0].Viewport.Width = (float)ClientFrameWidth;
	viewportArr[0].Viewport.Height = (float)ClientFrameHeight;
	viewportArr[0].Viewport.MinDepth = 0.0f;
	viewportArr[0].Viewport.MaxDepth = 1.0f;
	viewportArr[0].ScissorRect = { 0, 0, (long)ClientFrameWidth, (long)ClientFrameHeight };

	MainRenderTarget_PixelFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	ShaderVisibleDescPool.Initialize(1024 * 32);

	NewSwapChain();
	Create_RTV_DSV_DescriptorHeaps();
	CreateRenderTargetViews();
	CreateDepthStencilView();
	//if (RenderMod == DeferedRendering) {
	//	//set gbuffer format ex>
	//	//GbufferPixelFormatArr[0] = DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM;

	//	CreateGbufferRenderTargetViews();
	//}

	TextureDescriptorAllotter.Init(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 16384);
	//why cannot be shader visible in Saver DescriptorHeap?
	//answer : CreateConstantBufferView, CreateShaderResourceView -> only CPU Descriptor Working. so Shader Invisible.

	try {
		raytracing.Init(this);
	}
	catch (string err) {
		dbglog1a("RayTracing ERROR : %s\n", err);
	}
}

void GlobalDevice::Release()
{
	WaitGPUComplete();

	if (RenderMod == DeferedRendering) {
		if (GbufferDescriptorHeap) GbufferDescriptorHeap->Release();
		for (int i = 0; i < GbufferCount; ++i) {
			if (ppGBuffers[i]) ppGBuffers[i]->Release();
		}
	}

	for (int i = 0; i < SwapChainBufferCount; ++i) {
		if (ppRenderTargetBuffers[i]) ppRenderTargetBuffers[i]->Release();
	}

	if (pRtvDescriptorHeap) pRtvDescriptorHeap->Release();
	if (pDepthStencilBuffer) pDepthStencilBuffer->Release();
	if (pDsvDescriptorHeap) pDsvDescriptorHeap->Release();

	if (gpucmd) gpucmd.Release();
	if (CScmd) CScmd.Release();

	CloseHandle(hFenceEvent);
	if (pFence) pFence->Release();

	pSwapChain->SetFullscreenState(FALSE, NULL);
	if (pSwapChain) pSwapChain->Release();

	if (pDevice) {
		pDevice->Release();
	}
	if (pFactory) pFactory->Release();

#if defined(_DEBUG)
	IDXGIDebug1* pdxgiDebug = NULL;
	DXGIGetDebugInterface1(0, __uuidof(IDXGIDebug1), (void**)&pdxgiDebug);
	HRESULT hResult = pdxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL,
		DXGI_DEBUG_RLO_DETAIL);
	pdxgiDebug->Release();
#endif
}

void GlobalDevice::WaitGPUComplete()
{
	gpucmd.WaitGPUComplete();
}

void GlobalDevice::NewSwapChain()
{
	if (pSwapChain) pSwapChain->Release();

	DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc;
	::ZeroMemory(&dxgiSwapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC1));
	dxgiSwapChainDesc.Width = ClientFrameWidth;
	dxgiSwapChainDesc.Height = ClientFrameHeight;
	dxgiSwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dxgiSwapChainDesc.SampleDesc.Count = (m_bMsaa4xEnable) ? 4 : 1;
	dxgiSwapChainDesc.SampleDesc.Quality = (m_bMsaa4xEnable) ? (m_nMsaa4xQualityLevels -
		1) : 0;
	dxgiSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	dxgiSwapChainDesc.BufferCount = SwapChainBufferCount;
	dxgiSwapChainDesc.Scaling = DXGI_SCALING_NONE;
	dxgiSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	dxgiSwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

	//must contain for fullscreen mode!!!!!
	dxgiSwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; // enable resize os resolusion..

	DXGI_SWAP_CHAIN_FULLSCREEN_DESC dxgiSwapChainFullScreenDesc;
	::ZeroMemory(&dxgiSwapChainFullScreenDesc, sizeof(DXGI_SWAP_CHAIN_FULLSCREEN_DESC));
	dxgiSwapChainFullScreenDesc.RefreshRate.Numerator = 60;
	dxgiSwapChainFullScreenDesc.RefreshRate.Denominator = 1;
	dxgiSwapChainFullScreenDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	dxgiSwapChainFullScreenDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	dxgiSwapChainFullScreenDesc.Windowed = TRUE;

	//Create SwapChain
	pFactory->CreateSwapChainForHwnd(gpucmd.pCommandQueue, hWnd,
		&dxgiSwapChainDesc, &dxgiSwapChainFullScreenDesc, NULL, (IDXGISwapChain1
			**)&pSwapChain); // is version 4 can executing without Query? sus..

	// Disable (Alt + Enter to Fullscreen Mode.)
	pFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);

	CurrentSwapChainBufferIndex = pSwapChain->GetCurrentBackBufferIndex();

	/*if (RenderMod == DeferedRendering) {
		NewGbuffer();
	}*/
}

void GlobalDevice::NewGbuffer()
{
	D3D12_HEAP_PROPERTIES heapProperty;
	heapProperty.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperty.Type = D3D12_HEAP_TYPE_DEFAULT; // type : DEFAULT HEAP
	heapProperty.VisibleNodeMask = 0xFFFFFFFF;
	heapProperty.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperty.CreationNodeMask = 0;

	D3D12_HEAP_FLAGS heapFlag;
	heapFlag = D3D12_HEAP_FLAG_NONE;

	D3D12_RESOURCE_DESC desc;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.MipLevels = 0;
	desc.Width = ClientFrameWidth;
	desc.Height = ClientFrameHeight;
	desc.Alignment = 0; // 64KB Alignment.
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	desc.DepthOrArraySize = 1; // ?
	desc.SampleDesc.Count = 0;
	desc.SampleDesc.Quality = 0; // no MSAA

	D3D12_RESOURCE_STATES initState;
	initState = D3D12_RESOURCE_STATE_RENDER_TARGET;

	D3D12_CLEAR_VALUE clearValue;
	clearValue.Color[0] = 0;
	clearValue.Color[1] = 0;
	clearValue.Color[2] = 0;
	clearValue.Color[3] = 0;

	for (int i = 0; i < GbufferCount; ++i) {
		desc.Format = GbufferPixelFormatArr[i];
		clearValue.Format = GbufferPixelFormatArr[i];

		pDevice->CreateCommittedResource(&heapProperty, heapFlag, &desc, initState, &clearValue, __uuidof(ID3D12Resource), (void**)&ppGBuffers[i]);
	}
}

void GlobalDevice::Create_RTV_DSV_DescriptorHeaps()
{
	//RTV heap
	D3D12_DESCRIPTOR_HEAP_DESC d3dDescriptorHeapDesc;
	::ZeroMemory(&d3dDescriptorHeapDesc, sizeof(D3D12_DESCRIPTOR_HEAP_DESC));
	d3dDescriptorHeapDesc.NumDescriptors = SwapChainBufferCount + GbufferCount + 1 + 6 * max_lightProbeCount;
	d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	d3dDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	d3dDescriptorHeapDesc.NodeMask = 0;
	HRESULT hResult = pDevice->CreateDescriptorHeap(&d3dDescriptorHeapDesc,
		__uuidof(ID3D12DescriptorHeap), (void**)&pRtvDescriptorHeap);

	//DSV heap
	d3dDescriptorHeapDesc.NumDescriptors = 2 + max_DirLightCascadingShadowCount + 6 * max_PointLightMaxCount;
	d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	hResult = pDevice->CreateDescriptorHeap(&d3dDescriptorHeapDesc,
		__uuidof(ID3D12DescriptorHeap), (void**)&pDsvDescriptorHeap);

	/*if (RenderMod == DeferedRendering) {
		D3D12_DESCRIPTOR_HEAP_DESC d3dDescriptorHeapDesc;
		::ZeroMemory(&d3dDescriptorHeapDesc, sizeof(D3D12_DESCRIPTOR_HEAP_DESC));
		d3dDescriptorHeapDesc.NumDescriptors = GbufferCount;
		d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		d3dDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		d3dDescriptorHeapDesc.NodeMask = 0;
		HRESULT hResult = pDevice->CreateDescriptorHeap(&d3dDescriptorHeapDesc,
			__uuidof(ID3D12DescriptorHeap), (void**)&GbufferDescriptorHeap);
	}*/
}

void GlobalDevice::CreateRenderTargetViews()
{
	D3D12_RENDER_TARGET_VIEW_DESC MainRenderTargetViewDesc;
	MainRenderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	MainRenderTargetViewDesc.Format = MainRenderTarget_PixelFormat;
	MainRenderTargetViewDesc.Texture2D.MipSlice = 0;
	MainRenderTargetViewDesc.Texture2D.PlaneSlice = 0;

	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle =
		pRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	for (UINT i = 0; i < SwapChainBufferCount; i++)
	{
		pSwapChain->GetBuffer(i, __uuidof(ID3D12Resource), (void
			**)&ppRenderTargetBuffers[i]);

		// renderTarget View
		pDevice->CreateRenderTargetView(ppRenderTargetBuffers[i], &MainRenderTargetViewDesc,
			d3dRtvCPUDescriptorHandle);

		// shader Resource View
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 1;
		D3D12_CPU_DESCRIPTOR_HANDLE pcpu;
		D3D12_GPU_DESCRIPTOR_HANDLE pgpu;
		DescIndex descI;
		gd.ShaderVisibleDescPool.ImmortalAlloc(&descI, 1);
		RenderTargetSRV_pGPU[i] = descI.hRender.hgpu;
		pDevice->CreateShaderResourceView(ppRenderTargetBuffers[i], &srvDesc, descI.hCreation.hcpu);

		d3dRtvCPUDescriptorHandle.ptr += gd.RTVSize;
	}

	//Bluring UAV
	gd.BlurTexture = CreateTextureWithUAV(pDevice, gd.ClientFrameWidth, gd.ClientFrameHeight, DXGI_FORMAT_R8G8B8A8_UNORM);

	SubRenderTarget = CreateRenderTargetTextureWithRTV(pDevice,
		pRtvDescriptorHeap, 2, gd.ClientFrameWidth, gd.ClientFrameHeight);
}

void GlobalDevice::CreateGbufferRenderTargetViews()
{
	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle =
		GbufferDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	if (RenderMod == DeferedRendering) {
		for (int i = 0; i < GbufferCount; ++i) {
			D3D12_RENDER_TARGET_VIEW_DESC GbufferRenderTargetViewDesc;
			GbufferRenderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			GbufferRenderTargetViewDesc.Format = GbufferPixelFormatArr[i];
			GbufferRenderTargetViewDesc.Texture2D.MipSlice = 0;
			GbufferRenderTargetViewDesc.Texture2D.PlaneSlice = 0;

			pDevice->CreateRenderTargetView(ppGBuffers[i], &GbufferRenderTargetViewDesc, d3dRtvCPUDescriptorHandle);
		}
	}
}

void GlobalDevice::CreateDepthStencilView()
{
	D3D12_RESOURCE_DESC d3dResourceDesc;
	d3dResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	d3dResourceDesc.Alignment = 0;
	d3dResourceDesc.Width = ClientFrameWidth;
	d3dResourceDesc.Height = ClientFrameHeight;
	d3dResourceDesc.DepthOrArraySize = 1;
	d3dResourceDesc.MipLevels = 1;
	d3dResourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	d3dResourceDesc.SampleDesc.Count = (m_bMsaa4xEnable) ? 4 : 1;
	d3dResourceDesc.SampleDesc.Quality = (m_bMsaa4xEnable) ? (m_nMsaa4xQualityLevels - 1)
		: 0;
	d3dResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	d3dResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	D3D12_HEAP_PROPERTIES d3dHeapProperties;
	::ZeroMemory(&d3dHeapProperties, sizeof(D3D12_HEAP_PROPERTIES));
	d3dHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	d3dHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	d3dHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	d3dHeapProperties.CreationNodeMask = 1;
	d3dHeapProperties.VisibleNodeMask = 1;
	D3D12_CLEAR_VALUE d3dClearValue;
	d3dClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	d3dClearValue.DepthStencil.Depth = 1.0f;
	d3dClearValue.DepthStencil.Stencil = 0;
	pDevice->CreateCommittedResource(&d3dHeapProperties, D3D12_HEAP_FLAG_NONE,
		&d3dResourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &d3dClearValue,
		__uuidof(ID3D12Resource), (void**)&pDepthStencilBuffer);

	D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle =
		pDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	pDevice->CreateDepthStencilView(pDepthStencilBuffer, NULL,
		d3dDsvCPUDescriptorHandle);

	gd.ShaderVisibleDescPool.ImmortalAlloc(&MainDS_SRV, 1);
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc2 = {};
	SRVDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	SRVDesc2.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	SRVDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc2.Texture2D.MipLevels = 1;
	SRVDesc2.Texture2D.MostDetailedMip = 0;
	SRVDesc2.Texture2D.PlaneSlice = 0;
	SRVDesc2.Texture2D.ResourceMinLODClamp = 0.0f;
	pDevice->CreateShaderResourceView(pDepthStencilBuffer, &SRVDesc2, MainDS_SRV.hCreation.hcpu);
}

//work in present device but.. if resolution change, it can be strange..
// why??
void GlobalDevice::SetFullScreenMode(bool isFullScreen)
{
	WaitGPUComplete();
	BOOL bFullScreenState = FALSE;
	pSwapChain->GetFullscreenState(&bFullScreenState, NULL);
	if (isFullScreen != bFullScreenState) {
		//Change FullscreenState
		pSwapChain->SetFullscreenState(isFullScreen, NULL);

		DXGI_MODE_DESC dxgiTargetParameters;
		dxgiTargetParameters.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		dxgiTargetParameters.Width = ClientFrameWidth;
		dxgiTargetParameters.Height = ClientFrameHeight;
		dxgiTargetParameters.RefreshRate.Numerator = 60;
		dxgiTargetParameters.RefreshRate.Denominator = 1;
		dxgiTargetParameters.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		dxgiTargetParameters.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		pSwapChain->ResizeTarget(&dxgiTargetParameters);

		for (int i = 0; i < SwapChainBufferCount; i++) if (ppRenderTargetBuffers[i]) ppRenderTargetBuffers[i]->Release();

		DXGI_SWAP_CHAIN_DESC dxgiSwapChainDesc;
		pSwapChain->GetDesc(&dxgiSwapChainDesc);
		pSwapChain->ResizeBuffers(SwapChainBufferCount, ClientFrameWidth,
			ClientFrameHeight, dxgiSwapChainDesc.BufferDesc.Format, dxgiSwapChainDesc.Flags);
		CurrentSwapChainBufferIndex = pSwapChain->GetCurrentBackBufferIndex();

		CreateRenderTargetViews();
	}
}

void GlobalDevice::SetResolution(int resid, bool ClientSizeUpdate)
{
	WaitGPUComplete();

	ClientFrameWidth = EnableFullScreenMode_Resolusions[resid].width;
	ClientFrameHeight = EnableFullScreenMode_Resolusions[resid].height;

	if (ClientSizeUpdate) {
		SetWindowPos(hWnd, NULL, 0, 0, ClientFrameWidth, ClientFrameHeight, SWP_NOMOVE | SWP_NOZORDER);
	}

	if (RenderMod == DeferedRendering) {
		for (int i = 0; i < GbufferCount; ++i) {
			ppGBuffers[i]->Release();
		}

		NewGbuffer();
		CreateGbufferRenderTargetViews();
	}

	pDepthStencilBuffer->Release();
	CreateDepthStencilView();
}

//(40 lines) outer code : microsoft copilot.
int GlobalDevice::PixelFormatToPixelSize(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R32G32B32A32_FLOAT: return 16;
	case DXGI_FORMAT_R32G32B32A32_UINT:  return 16;
	case DXGI_FORMAT_R32G32B32A32_SINT:  return 16;

	case DXGI_FORMAT_R32G32B32_FLOAT:    return 12;
	case DXGI_FORMAT_R32G32B32_UINT:     return 12;
	case DXGI_FORMAT_R32G32B32_SINT:     return 12;

	case DXGI_FORMAT_R16G16B16A16_FLOAT: return 8;
	case DXGI_FORMAT_R16G16B16A16_UNORM: return 8;
	case DXGI_FORMAT_R16G16B16A16_UINT:  return 8;
	case DXGI_FORMAT_R8G8B8A8_UNORM:     return 4;
	case DXGI_FORMAT_R8G8B8A8_UINT:      return 4;
	case DXGI_FORMAT_B8G8R8A8_UNORM:     return 4;
	case DXGI_FORMAT_R32_FLOAT:          return 4;
	case DXGI_FORMAT_R32_UINT:           return 4;
	case DXGI_FORMAT_R16_FLOAT:          return 2;
	case DXGI_FORMAT_R8_UNORM:           return 1;

	case DXGI_FORMAT_D32_FLOAT:          return 4;
	case DXGI_FORMAT_D24_UNORM_S8_UINT:  return 4;

	case DXGI_FORMAT_R11G11B10_FLOAT:    return 4;
	case DXGI_FORMAT_R10G10B10A2_UNORM:  return 4;

	case DXGI_FORMAT_UNKNOWN:			 return 1;
		// 압축 포맷은 픽셀당 크기가 고정되지 않음
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC3_UNORM:
	case DXGI_FORMAT_BC4_UNORM:
	case DXGI_FORMAT_BC5_UNORM:
	case DXGI_FORMAT_BC6H_UF16:
	case DXGI_FORMAT_BC7_UNORM:
		return -1; // 블록 기반 포맷: 직접 계산 필요
	}
}

GPUResource GlobalDevice::CreateCommitedGPUBuffer(D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES d3dResourceStates, D3D12_RESOURCE_DIMENSION dimension, int Width, int Height, DXGI_FORMAT BufferFormat, UINT DepthOrArraySize, D3D12_RESOURCE_FLAGS AdditionalFlag)
{
	ID3D12Resource* pBuffer = NULL;
	ID3D12Resource2* pBuffer2 = NULL;

	D3D12_HEAP_PROPERTIES d3dHeapPropertiesDesc;
	::ZeroMemory(&d3dHeapPropertiesDesc, sizeof(D3D12_HEAP_PROPERTIES));
	d3dHeapPropertiesDesc.Type = heapType; // default / upload / readback ...
	d3dHeapPropertiesDesc.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	d3dHeapPropertiesDesc.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	d3dHeapPropertiesDesc.CreationNodeMask = 1;
	d3dHeapPropertiesDesc.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC d3dResourceDesc;
	::ZeroMemory(&d3dResourceDesc, sizeof(D3D12_RESOURCE_DESC));
	d3dResourceDesc.Dimension = dimension;
	d3dResourceDesc.Alignment = 0;
	d3dResourceDesc.Width = Width;
	d3dResourceDesc.Height = Height;
	d3dResourceDesc.DepthOrArraySize = DepthOrArraySize;
	d3dResourceDesc.MipLevels = 1;
	d3dResourceDesc.Format = BufferFormat;
	d3dResourceDesc.SampleDesc.Count = 1;
	d3dResourceDesc.SampleDesc.Quality = 0;
	if (d3dResourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
		d3dResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	}
	else {
		d3dResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	}
	d3dResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;
	if (d3dResourceDesc.Layout == D3D12_TEXTURE_LAYOUT_ROW_MAJOR && (d3dResourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)) {
		heapFlags |= D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER;
	}

	if (heapFlags & D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER) {
		heapFlags |= D3D12_HEAP_FLAG_SHARED;
		d3dResourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
	}

	d3dResourceDesc.Flags |= AdditionalFlag;

	//D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER

	D3D12_RESOURCE_STATES d3dResourceInitialStates = D3D12_RESOURCE_STATE_COMMON;
	if (heapType == D3D12_HEAP_TYPE_UPLOAD)
		d3dResourceInitialStates = D3D12_RESOURCE_STATE_GENERIC_READ;
	else if (heapType == D3D12_HEAP_TYPE_READBACK)
		d3dResourceInitialStates = D3D12_RESOURCE_STATE_COPY_DEST;

	HRESULT hResult;
	D3D12_CLEAR_VALUE ClearValue;
	if (AdditionalFlag & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) {
		ClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		ClearValue.DepthStencil.Depth = 1.0f;
		ClearValue.DepthStencil.Stencil = 0;
		hResult = gd.pDevice->CreateCommittedResource(&d3dHeapPropertiesDesc, heapFlags, &d3dResourceDesc, d3dResourceInitialStates, &ClearValue, __uuidof(ID3D12Resource), (void**)&pBuffer);
	}
	else {
		hResult = gd.pDevice->CreateCommittedResource(&d3dHeapPropertiesDesc, heapFlags, &d3dResourceDesc, d3dResourceInitialStates, NULL, __uuidof(ID3D12Resource), (void**)&pBuffer);
	}

	dbgbreak(pBuffer == nullptr);

	pBuffer->QueryInterface<ID3D12Resource2>(&pBuffer2);
	pBuffer->Release();

	if (FAILED(hResult)) {
		GPUResource gr;
		gr.resource = nullptr;
		gr.extraData = 0;
		return gr;
	}
	else {
		GPUResource gr;
		gr.resource = pBuffer2;

#ifdef DEVELOPMODE_DBG_GPURESOURCES
		dbgc[0] += 1;
		dbglog2(L"GPUResource %llx Created. dbgc0 = %d \n", gr.resource->GetGPUVirtualAddress(), dbgc[0]);
#endif

		gr.CurrentState_InCommandWriteLine = d3dResourceInitialStates;
		gr.extraData = 0;
		return gr;
	}
}

void GlobalDevice::UploadToCommitedGPUBuffer(void* ptr, GPUResource* uploadBuffer, GPUResource* copydestBuffer, bool StateReturning)
{
	D3D12_RANGE d3dReadRange = { 0, 0 };

	D3D12_RESOURCE_DESC1 desc = uploadBuffer->resource->GetDesc1();

	int SizePerUnit = PixelFormatToPixelSize(desc.Format);
	ui32 BufferByteSize = desc.Width * desc.Height * SizePerUnit;
	UINT8* pBufferDataBegin = NULL;

	uploadBuffer->resource->Map(0, &d3dReadRange, (void**)&pBufferDataBegin);
	memcpy(pBufferDataBegin, ptr, BufferByteSize);
	uploadBuffer->resource->Unmap(0, NULL); // quest : i want know gpumem load excution shape

	if (copydestBuffer) {
		D3D12_RESOURCE_STATES uploadBufferOriginState = uploadBuffer->CurrentState_InCommandWriteLine;
		D3D12_RESOURCE_STATES copydestOriginState = copydestBuffer->CurrentState_InCommandWriteLine;

		gpucmd.ResBarrierTr(uploadBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE);
		gpucmd.ResBarrierTr(copydestBuffer, D3D12_RESOURCE_STATE_COPY_DEST);

		gpucmd->CopyResource(copydestBuffer->resource, uploadBuffer->resource);

		if (StateReturning) {
			gpucmd.ResBarrierTr(uploadBuffer, uploadBufferOriginState);
			gpucmd.ResBarrierTr(copydestBuffer, copydestOriginState);
		}
	}
}

UINT64 GlobalDevice::GetRequiredIntermediateSize(ID3D12Resource* pDestinationResource, UINT FirstSubresource, UINT NumSubresources) noexcept
{
#if defined(_MSC_VER) || !defined(_WIN32)
	const auto Desc = pDestinationResource->GetDesc();
#else
	D3D12_RESOURCE_DESC tmpDesc;
	const auto& Desc = *pDestinationResource->GetDesc(&tmpDesc);
#endif
	UINT64 RequiredSize = 0;
	pDevice->GetCopyableFootprints(&Desc, FirstSubresource, NumSubresources, 0, nullptr, nullptr, nullptr, &RequiredSize);
	return RequiredSize;
}

void GlobalDevice::bmpTodds(int mipmap_level, const char* Format, const char* filename)
{
	string cmd = "D3DTexConv\\texconv.exe -m ";
	cmd += to_string(mipmap_level);
	cmd += " -f ";
	cmd += Format;
	cmd += " ";
	cmd += filename;
	int result = system(cmd.c_str());
	cout << result << endl;
}

void GlobalDevice::CreateDefaultHeap_VB(void* ptr, GPUResource& VertexBuffer, GPUResource& VertexUploadBuffer, D3D12_VERTEX_BUFFER_VIEW& view, UINT VertexCount, UINT sizeofVertex)
{
	UINT capacity = VertexCount * sizeofVertex;
	VertexBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, capacity, 1);
	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, capacity, 1);
	gd.UploadToCommitedGPUBuffer(ptr, &VertexUploadBuffer, &VertexBuffer, true);
	gd.gpucmd.ResBarrierTr(&VertexBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	view.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
	view.StrideInBytes = sizeofVertex;
	view.SizeInBytes = sizeofVertex * VertexCount;
}

GPUResource GlobalDevice::CreateShadowMap(int width, int height, int DSVoffset, SpotLight& spotLight)
{
	GPUResource shadowMap;

	D3D12_RESOURCE_DESC shadowTextureDesc;
	shadowTextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	shadowTextureDesc.Alignment = 0;
	shadowTextureDesc.Width = width;
	shadowTextureDesc.Height = height;
	shadowTextureDesc.Format = DXGI_FORMAT_D32_FLOAT;
	shadowTextureDesc.DepthOrArraySize = 1;
	shadowTextureDesc.MipLevels = 1;
	shadowTextureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	shadowTextureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	shadowTextureDesc.SampleDesc.Count = 1;
	shadowTextureDesc.SampleDesc.Quality = 0;

	D3D12_CLEAR_VALUE clearValue;    // Performance tip: Tell the runtime at resource creation the desired clear value.
	clearValue.Format = DXGI_FORMAT_D32_FLOAT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	D3D12_HEAP_PROPERTIES heap_property;
	heap_property.Type = D3D12_HEAP_TYPE_DEFAULT;
	heap_property.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap_property.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heap_property.CreationNodeMask = 1;
	heap_property.VisibleNodeMask = 1;

	gd.pDevice->CreateCommittedResource(
		&heap_property,
		D3D12_HEAP_FLAG_NONE,
		&shadowTextureDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&clearValue,
		IID_PPV_ARGS(&shadowMap.resource));
	shadowMap.CurrentState_InCommandWriteLine = D3D12_RESOURCE_STATE_DEPTH_WRITE;

	// Create the depth stencil view.
	D3D12_CPU_DESCRIPTOR_HANDLE hcpu;
	hcpu.ptr = gd.pDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + DSVoffset * gd.DSVSize;
	gd.pDevice->CreateDepthStencilView(shadowMap.resource, nullptr, hcpu);
	shadowMap.descindex.Set(false, DSVoffset, 'd'); // DepthStencilView DescHeap의 CPU HANDLE

	D3D12_CPU_DESCRIPTOR_HANDLE srvCpuH;
	D3D12_GPU_DESCRIPTOR_HANDLE srvGpuH;

	gd.ShaderVisibleDescPool.ImmortalAlloc(&spotLight.descindex, 1);

	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
	srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
	srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv_desc.Texture2D.MostDetailedMip = 0;
	srv_desc.Texture2D.MipLevels = 1;
	srv_desc.Texture2D.ResourceMinLODClamp = 0;
	srv_desc.Texture2D.PlaneSlice = 0;
	gd.pDevice->CreateShaderResourceView(shadowMap.resource, &srv_desc, spotLight.descindex.hCreation.hcpu);
	//shadowMap.handle.hgpu = spotLight.descindex.hRender.hgpu; // CBV, SRV, UAV DescHeap 의 GPU HANDLE
	return shadowMap;
}

void GlobalDevice::AddTextSDFTexture(wchar_t key)
{
	char Zero = 0;
	// gpu texture byte to signed float normalize
	// : float f = max((float)b / 127.0f, -1.0f)
	// so 127 is 0. 0x8F;
	// signed float normalize to byte
	// : b = f * 127 + 128;

	constexpr int textureMipLevel = 16;
	constexpr int textureMipLevel_pow2 = (1 << textureMipLevel) - 1;

	constexpr int bezier_divide = 8;
	constexpr float bezier_delta = 1.0f / bezier_divide;
	int i = 0;
	for (; i < gd.FontCount; ++i) {
		if (font_data[i].glyphs.contains(key) == false) {
			continue;
		}

		break;
	}
	if (i < gd.FontCount) {
		if (font_sdftexture_map[i].contains(key)) return;

		Glyph g = font_data[i].glyphs[key];
		int width = g.bounding_box[2] - g.bounding_box[0] + 1;
		int height = g.bounding_box[3] - g.bounding_box[1] + 1;
		float xBase = g.bounding_box[0];
		float yBase = g.bounding_box[1];
		char* textureData = new char[width * height];
		memset(textureData, (char)127, width * height);

		vector<vector<XMFLOAT3>> polygons;
		//reserved??
		polygons.reserve(g.path_list.size());

		//outline
		for (int k = 0; k < g.path_list.size(); ++k) {
			polygons.push_back(vector<XMFLOAT3>());

			Path p = g.path_list[k];
			polygons.reserve(p.geometry.size());
			polygons[k].push_back({ p.geometry[0].p0.x, p.geometry[0].p0.y, 0.0f });
			for (int u = 0; u < p.geometry.size(); ++u) {
				Curve c = p.geometry[u];
				float_v2 startpos = c.p0;
				float_v2 endpos = c.p1;
				if (c.is_curve) {
					float_v2 controlpos = endpos;
					endpos = c.c;

					float t = 0;
					vec4 s = vec4(startpos.x, startpos.y, controlpos.x, controlpos.y);
					vec4 e = vec4(controlpos.x, controlpos.y, endpos.x, endpos.y);
					float_v2 prevpos = startpos;
					prevpos.x -= xBase;
					prevpos.y -= yBase;
					t += bezier_delta;
					for (; t < 1.0f + bezier_delta; t += bezier_delta) {
						vec4 r0 = s * (1.0f - t) + t * e;
						vec4 r1 = vec4(r0.z, r0.w, 0, 0);
						r1 = r0 * (1.0f - t) + t * r1;
						float_v2 newpos = { r1.x, r1.y };

						newpos.x -= xBase;
						newpos.y -= yBase;
						polygons[k].push_back({ newpos.x, newpos.y, 0.0f });
						AddLineInSDFTexture(prevpos, newpos, textureData, width, height);
						prevpos = newpos;
					}
				}
				else {
					startpos.x -= xBase;
					startpos.y -= yBase;
					endpos.x -= xBase;
					endpos.y -= yBase;

					polygons[k].push_back({ endpos.x, endpos.y, 0.0f });
					AddLineInSDFTexture(startpos, endpos, textureData, width, height);
				}
			}
		}

		for (int yi = 1; yi < height; ++yi) {

			bool isfill = false;

			bool returning = 0;
			int retcnt = 0;
		ROW_RETURN:

			int lastendxi = 0;
			int lastturncnt = 0;

			for (int xi = 0; xi < width; ++xi) {
				char* ptr = (char*)&textureData[yi * width + xi];
				//float distance = getSDF(polygons, vec2f(xi, yi), false);
				//if(distance >= 0) *ptr = SignedFloatNormalizeToByte(distance);

				int pxi = xi;
				if (*ptr <= Zero) {
					bool ret = false;

					char* beginpaint = ptr;
					while (*beginpaint <= Zero && xi < width) {
						xi += 1;
						beginpaint += 1;
					}

					char* endpaint = beginpaint;
					while (*endpaint > Zero && xi < width) {
						xi += 1;
						endpaint += 1;
					}

					for (; beginpaint < endpaint; beginpaint += 1) {
						if (*(beginpaint - width) <= Zero) {
							*beginpaint = Zero; // distance
						}
						else {
							ret = true;
						}
					}

					if (xi == width && (lastturncnt == 0 && beginpaint == endpaint)) {
						xi = lastendxi - 1;
						char* insptr = (char*)&textureData[yi * width + lastendxi];
						*insptr = Zero; // distance
						lastturncnt += 1;
						continue;
					}

					if (ret == false) {
						while (*endpaint <= Zero && xi < width) {
							xi += 1;
							endpaint += 1;
						}
						lastendxi = xi;
						continue;
					}
					else {
						xi = ptr - (char*)&textureData[yi * width];
						returning = true;
					}
				}
			}

			if (returning) {
				retcnt += 1;
				if (retcnt < 2) goto ROW_RETURN;
			}
			else retcnt = 0;
		}

		int mipW = width / textureMipLevel;
		int mipH = height / textureMipLevel;
		int realW = mipW * 2;
		int realH = mipH * 2;
		int startX = mipW / 2;
		int startY = mipH / 2;
		char* mipTex = new char[realW * realH];
		memset(mipTex, 127, realW * realH);
		for (int yi = 0; yi < mipH; ++yi) {
			for (int xi = 0; xi < mipW; ++xi) {
				*(char*)&mipTex[(startY + yi) * (realW)+startX + xi] = *(char*)&textureData[yi * textureMipLevel * width + xi * textureMipLevel];
			}
		}
		delete[] textureData;

		vector<uint8_t> sdfbuffer = makeSDF((char*)mipTex, realW, realH, 0.25f, -1.0f * realH * 0.5f);

		//텍스쳐영역을 여러 글자가 사용할때
		PushSDFText(key, realW, realH, (char*)sdfbuffer.data());

		//텍스쳐가 온전히 만들어지는지를 디버깅하기 위함.
		//imgform::PixelImageObject pio;
		//pio.width = realW;
		//pio.height = realH;
		//pio.data = (imgform::RGBA_pixel*)sdfbuffer.data();
		////pio.rawDataToBMP("SDFTestImage.bmp", DXGI_FORMAT_R8_SNORM);

		//단일 리소스로 제작시
		//GPUResource texture;
		//ZeroMemory(&texture, sizeof(GPUResource));
		//texture.CreateTexture_fromImageBuffer(realW, realH, sdfbuffer.data(), DXGI_FORMAT_R8_SNORM);
		//font_sdftexture_map[i].insert(pair<wchar_t, GPUResource>(key, texture));

		delete[] mipTex;
	}
}

void GlobalDevice::AddLineInSDFTexture(float_v2 startpos, float_v2 endpos, char* texture, int width, int height)
{
	char Zero = 0;
	//draw line
	float dy = (endpos.y - startpos.y);
	float dx = (endpos.x - startpos.x);

	if (fabsf(dy) > fabsf(dx)) {
		//y access
		float minY = min(startpos.y, endpos.y);
		float maxY = max(startpos.y, endpos.y);

		for (float di = minY; di < maxY + 1; di += 1.0f) {
			int x = dx / dy * (di - startpos.y) + startpos.x;

			if (x < 0) x = 0;
			if (x >= width) x = width - 1;
			int y = di;
			if (y < 0) y = 0;
			if (y >= height) y = height - 1;

			char* ptr = (char*)&texture[(int)y * width + x];
			*ptr = Zero;
		}
	}
	else {
		//x access

		float minX = min(startpos.x, endpos.x);
		float maxX = max(startpos.x, endpos.x);

		for (float di = minX; di < maxX + 1; di += 1.0f) {
			int y = dy / dx * (di - startpos.x) + startpos.y;

			int x = di;
			if (x < 0) x = 0;
			if (x >= width) x = width - 1;

			if (y < 0) y = 0;
			if (y >= height) y = height - 1;

			char* ptr = (char*)&texture[y * width + (int)x];
			*ptr = Zero;
		}
	}
}

char GlobalDevice::SignedFloatNormalizeToByte(float f)
{
	char b = (char)(f * 127.0f + 128.0f);
	return b;
}

template <int indexByteSize>
void GlobalDevice::CreateDefaultHeap_IB(void* ptr, GPUResource& IndexBuffer, GPUResource& IndexUploadBuffer, D3D12_INDEX_BUFFER_VIEW& view, UINT IndexCount)
{
	int capacity = IndexCount * indexByteSize;
	IndexBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, capacity, 1);
	IndexUploadBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, capacity, 1);
	gd.UploadToCommitedGPUBuffer(ptr, &IndexUploadBuffer, &IndexBuffer, true);
	gd.gpucmd.ResBarrierTr(&IndexBuffer, D3D12_RESOURCE_STATE_INDEX_BUFFER);
	view.BufferLocation = IndexBuffer.resource->GetGPUVirtualAddress();
	if constexpr (indexByteSize == 2) {
		view.Format = DXGI_FORMAT_R16_UINT;
	}
	else if constexpr (indexByteSize == 4) {
		view.Format = DXGI_FORMAT_R32_UINT;
	}
	view.SizeInBytes = indexByteSize * IndexCount;
}

bool GlobalDevice::PushSDFText(wchar_t c, ui16 width, ui16 height, char* copybuffer) {
	if (SDFTextureArr.size() == SDFTextureArr_immortalSize) {
		SDFTextureArr.push_back(new SDFTextPageTextureBuffer(SDFTextureArr.size()));
	}
	SDFTextPageTextureBuffer& texBuffer = *SDFTextureArr.at(SDFTextureArr.size() - 1);
	texBuffer.isDynamicTexture = true;
	bool b = texBuffer.PushSDFText(c, width, height, copybuffer);
	if (b == false) {
		SDFTextureArr.push_back(new SDFTextPageTextureBuffer(SDFTextureArr.size()));
		SDFTextPageTextureBuffer& texBuffer2 = *SDFTextureArr.at(SDFTextureArr.size() - 1);
		texBuffer2.isDynamicTexture = true;
		bool b = texBuffer2.PushSDFText(c, width, height, copybuffer);
	}
	return b;
}

void GlobalDevice::GetBakedSDFs() {
	wifstream ifs{ L"Resources/SDF/commonTextMeta.txt" };
	ifs.imbue(locale(""));
	wchar_t temp;
	int pageid;
	int sx, sy, w, h;
	while (ifs.eof() == false) {
		ifs >> temp;
		ifs >> pageid;
		ifs >> sx;
		ifs >> sy;
		ifs >> w;
		ifs >> h;
		SDFTextSection* sec = new SDFTextSection();
		sec->c = temp;
		sec->pageindex = pageid;
		sec->sx = sx;
		sec->sy = sy;
		sec->width = w;
		sec->height = h;

		if (SDFTextureArr.size() < pageid + 1) {
			SDFTextureArr.push_back(new SDFTextPageTextureBuffer(pageid));
		}
		SDFTextureArr[SDFTextureArr.size() - 1]->sectionCount += 1;
		SDFTextPageTextureBuffer::SDFSectionMap.insert(pair<wchar_t, SDFTextSection*>(temp, sec));
		dbglog1(L"%c\n", temp);
	}
	ifs.close();

	if (gd.gpucmd.isClose) {
		gd.gpucmd.Reset(true);
	}
	for (int i = 0;i < SDFTextureArr.size();++i) {
		SDFTextPageTextureBuffer* page = SDFTextureArr[i];
		wchar_t filename[256];
		wsprintfW(filename, L"Resources/SDF/SDFTextPage%d.dds", i);
		page->DefaultTextureBuffer.CreateTexture_fromFile(filename, DXGI_FORMAT_BC4_UNORM, 0);
		page->isDynamicTexture = false;
	}
	SDFTextureArr_immortalSize = SDFTextureArr.size();

	if (registerd_SDFTextSRVCount > 0) {
		for (int i = 0;i < registerd_SDFTextSRVCount;++i) {
			gd.TextureDescriptorAllotter.Free(SDFTextureArr[i]->SDFTextureSRV.index);
		}
	}

	for (int i = 0;i < SDFTextureArr.size(); ++i) {
		int index = gd.TextureDescriptorAllotter.Alloc();
		SDFTextureArr[i]->SDFTextureSRV.Set(false, index, 'n');
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		if (i < SDFTextureArr_immortalSize) {
			srvDesc.Format = DXGI_FORMAT_BC4_UNORM;
		}
		else {
			srvDesc.Format = DXGI_FORMAT_R8_SNORM;
		}
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 12;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		gd.pDevice->CreateShaderResourceView(SDFTextureArr[i]->DefaultTextureBuffer.resource, &srvDesc, SDFTextureArr[i]->SDFTextureSRV.hCreation.hcpu);
	}

	gd.gpucmd.Close(true);
	gd.gpucmd.Execute(true);
	gd.gpucmd.WaitGPUComplete();

	//game.MyScreenCharactorShader->SDFInstance_StructuredBuffer.resource->Map(0, nullptr, (void**)&game.MyScreenCharactorShader->MappedSDFInstance);
}

#pragma endregion

#pragma region RaytracingDeviceCode

void RayTracingDevice::Init(void* origin_gd)
{
	HRESULT hr;
	origin = (GlobalDevice*)origin_gd;
	GlobalDevice* sup = (GlobalDevice*)origin_gd;

	CheckDeviceSelfRemovable();

	if (IsDirectXRaytracingSupported(sup->pSelectedAdapter) == false) {
		throw "Your GPU is not support RayTracing.";
	}

	bool b = InitDXC();
	if (!b) throw "Cannot Init DXC.";

	//CreateRaytracingInterfaces
	hr = origin->pDevice->QueryInterface(IID_PPV_ARGS(&dxrDevice));
	if (FAILED(hr)) throw "Couldn't get DirectX Raytracing interface for the device.";
	hr = origin->gpucmd.GraphicsCmdList->QueryInterface(IID_PPV_ARGS(&dxrCommandList));
	if (FAILED(hr)) throw "Couldn't get DirectX Raytracing interface for the command list.";

	CreateSubRenderTarget();

	CreateCameraCB();
}

void RayTracingDevice::CheckDeviceSelfRemovable()
{
	__if_exists(DXGIDeclareAdapterRemovalSupport)
	{
		if (FAILED(DXGIDeclareAdapterRemovalSupport()))
		{
			OutputDebugString(L"Warning: application failed to declare adapter removal support.\n");
		}
	}
}

inline bool RayTracingDevice::IsDirectXRaytracingSupported(IDXGIAdapter1* adapter)
{
	ID3D12Device* testDevice;
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 featureSupportData = {};

	return SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&testDevice)))
		&& SUCCEEDED(testDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &featureSupportData, sizeof(featureSupportData)))
		&& featureSupportData.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
}

void RayTracingDevice::SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ID3D12RootSignature** rootSig)
{
	HRESULT hr;
	ID3DBlob* blob;
	ID3DBlob* error;

	hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
	if (FAILED(hr)) {
		if (error)
		{
			// 에러 메시지 출력
			OutputDebugStringA((char*)error->GetBufferPointer());
			error->Release();
		}

		//throw error ? static_cast<char*>(error->GetBufferPointer()) : nullptr;
	}

	hr = dxrDevice->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig)));
	if (FAILED(hr)) throw "dxrDevice->CreateRootSignature Failed.";
}

bool RayTracingDevice::InitDXC()
{
	bool bResult = FALSE;
	HRESULT hr;
	const WCHAR* wchDllPath = nullptr;
#if defined(_M_ARM64EC)
	wchDllPath = L"./Dxc/arm64";
#elif defined(_M_ARM64)
	wchDllPath = L"./Dxc/arm64";
#elif defined(_M_AMD64)
	wchDllPath = L"./Dxc/x64";
#elif defined(_M_IX86)
	wchDllPath = L"./Dxc/x86";
#endif
	WCHAR wchOldPath[_MAX_PATH];
	GetCurrentDirectoryW(_MAX_PATH, wchOldPath);
	SetCurrentDirectoryW(wchDllPath);
	DxcCreateInstanceT DxcCreateInstanceFunc = nullptr;

	hDXC = LoadLibrary(L"dxcompiler.dll");
	if (!hDXC) goto lb_return;

	DxcCreateInstanceFunc = (DxcCreateInstanceT)GetProcAddress(hDXC, "DxcCreateInstance");

	hr = DxcCreateInstanceFunc(CLSID_DxcLibrary, IID_PPV_ARGS(&pDXCLib));
	if (FAILED(hr))
		__debugbreak();

	hr = DxcCreateInstanceFunc(CLSID_DxcCompiler, IID_PPV_ARGS(&pDXCCompiler));
	if (FAILED(hr))
		__debugbreak();

	pDXCLib->CreateIncludeHandler(&pDSCIncHandle);
	bResult = TRUE;

lb_return:
	SetCurrentDirectoryW(wchOldPath);
	return bResult;
}

SHADER_HANDLE* RayTracingDevice::CreateShaderDXC(const wchar_t* shaderPath, const WCHAR* wchShaderFileName, const WCHAR* wchEntryPoint, const WCHAR* wchShaderModel, DWORD dwFlags, vector<LPWSTR>* macros)
{
	BOOL				bResult = FALSE;

	SYSTEMTIME	CreationTime = {};
	SHADER_HANDLE* pNewShaderHandle = nullptr;

	WCHAR wchOldPath[MAX_PATH];
	GetCurrentDirectory(_MAX_PATH, wchOldPath);

	IDxcBlob* pBlob = nullptr;

	// case DXIL::ShaderKind::Vertex:    entry = L"VSMain"; profile = L"vs_6_1"; break;
	// case DXIL::ShaderKind::Pixel:     entry = L"PSMain"; profile = L"ps_6_1"; break;
	// case DXIL::ShaderKind::Geometry:  entry = L"GSMain"; profile = L"gs_6_1"; break;
	// case DXIL::ShaderKind::Hull:      entry = L"HSMain"; profile = L"hs_6_1"; break;
	// case DXIL::ShaderKind::Domain:    entry = L"DSMain"; profile = L"ds_6_1"; break;
	// case DXIL::ShaderKind::Compute:   entry = L"CSMain"; profile = L"cs_6_1"; break;
	// case DXIL::ShaderKind::Mesh:      entry = L"MSMain"; profile = L"ms_6_5"; break;
	// case DXIL::ShaderKind::Amplification: entry = L"ASMain"; profile = L"as_6_5"; break;

	//"vs_6_0"
	//"ps_6_0"
	//"cs_6_0"
	//"gs_6_0"
	//"ms_6_5"
	//"as_6_5"
	//"hs_6_0"
	//"lib_6_3"

	bool bDisableOptimize = true;
#ifdef _DEBUG
	bDisableOptimize = false;
#endif

	SetCurrentDirectory(shaderPath);
	HRESULT	hr = CompileShaderFromFileWithDXC(pDXCLib, pDXCCompiler, pDSCIncHandle, wchShaderFileName, wchEntryPoint, wchShaderModel, &pBlob, bDisableOptimize, &CreationTime, 0, macros);

	if (FAILED(hr))
	{
		dbglog2(L"Failed to compile shader : %s-%s\n", wchShaderFileName, wchEntryPoint);
		throw "Failed to compile shader";
	}
	DWORD	dwCodeSize = (DWORD)pBlob->GetBufferSize();
	const char* pCodeBuffer = (const char*)pBlob->GetBufferPointer();

	DWORD	ShaderHandleSize = sizeof(SHADER_HANDLE) - sizeof(DWORD) + dwCodeSize;
	pNewShaderHandle = (SHADER_HANDLE*)malloc(ShaderHandleSize);

	if (pNewShaderHandle != nullptr) {
		memset(pNewShaderHandle, 0, ShaderHandleSize);
		memcpy(pNewShaderHandle->pCodeBuffer, pCodeBuffer, dwCodeSize);
		pNewShaderHandle->dwCodeSize = dwCodeSize;
		pNewShaderHandle->dwShaderNameLen = swprintf_s(pNewShaderHandle->wchShaderName, L"%s-%s", wchShaderFileName, wchEntryPoint);
		bResult = TRUE;
	}

lb_exit:
	if (pBlob)
	{
		pBlob->Release();
		pBlob = nullptr;
	}
	SetCurrentDirectory(wchOldPath);

	return pNewShaderHandle;
}

// 반드시 이전에 gd.SubRenderTarget 이 초기화 되어야 한다.
void RayTracingDevice::CreateSubRenderTarget()
{
	ID3D12Device5* device = dxrDevice;
	DXGI_FORMAT backbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	//// Create the output resource. The dimensions and format should match the swap-chain.
	//auto uavDesc = CD3DX12_RESOURCE_DESC::Tex2D(backbufferFormat, gd.ClientFrameWidth, gd.ClientFrameHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	//auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	//ThrowIfFailed(device->CreateCommittedResource(
	//	&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&RayTracingOutput)));
	//SetName(RayTracingOutput, L"gd.raytracing.RayTracingOutput");

	RayTracingOutput = gd.SubRenderTarget.resource;

	gd.ShaderVisibleDescPool.ImmortalAlloc(&RTO_UAV_index, 1);
	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
	UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	device->CreateUnorderedAccessView(RayTracingOutput, nullptr, &UAVDesc, RTO_UAV_index.hCreation.hcpu);

	DepthBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_DIMENSION_TEXTURE2D, gd.ClientFrameWidth, gd.ClientFrameHeight, DXGI_FORMAT_R32_FLOAT, 1, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	gd.ShaderVisibleDescPool.ImmortalAlloc(&MainDepth_UAV, 1);
	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc2 = {};
	UAVDesc2.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	UAVDesc2.Format = DXGI_FORMAT_R32_FLOAT;
	device->CreateUnorderedAccessView(DepthBuffer.resource, nullptr, &UAVDesc2, MainDepth_UAV.hCreation.hcpu);

	gd.ShaderVisibleDescPool.ImmortalAlloc(&MainDepth_SRV, 1);
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc2 = {};
	SRVDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	SRVDesc2.Format = DXGI_FORMAT_R32_FLOAT;
	SRVDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc2.Texture2D.MipLevels = 1;
	SRVDesc2.Texture2D.MostDetailedMip = 0;
	SRVDesc2.Texture2D.PlaneSlice = 0;
	SRVDesc2.Texture2D.ResourceMinLODClamp = 0.0f;
	device->CreateShaderResourceView(DepthBuffer.resource, &SRVDesc2, MainDepth_SRV.hCreation.hcpu);
}

void RayTracingDevice::CreateCameraCB()
{
	// Create the constant buffer memory and map the CPU and GPU addresses
	const D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	size_t ElementSize = sizeof(RayTracingDevice::CameraConstantBuffer);
	ElementSize = ((ElementSize + 255) & ~255);
	const D3D12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(ElementSize);

	ThrowIfFailed(dxrDevice->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&constantBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&CameraCB)));

	// Map the constant buffer and cache its heap pointers.
	// We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
	CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
	ThrowIfFailed(CameraCB->Map(0, nullptr, reinterpret_cast<void**>(&MappedCB)));

	gd.raytracing.m_eye = vec4(0.0f, 2.0f, -5.0f, 1.0f);
	gd.raytracing.m_at = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	XMVECTOR right = { 1.0f, 0.0f, 0.0f, 0.0f };

	XMVECTOR direction = XMVector4Normalize(gd.raytracing.m_at - gd.raytracing.m_eye);
	gd.raytracing.m_up = XMVector3Normalize(XMVector3Cross(direction, right));

	// Rotate camera around Y axis.
	XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(45.0f));
	gd.raytracing.m_eye = XMVector3Transform(gd.raytracing.m_eye, rotate);
	gd.raytracing.m_up = XMVector3Transform(gd.raytracing.m_up, rotate);

	MappedCB->cameraPosition = m_eye;
	float fovAngleY = 45.0f;
	XMMATRIX view = XMMatrixLookAtLH(gd.raytracing.m_eye, gd.raytracing.m_at, gd.raytracing.m_up);
	float m_aspectRatio = (float)gd.ClientFrameWidth / (float)gd.ClientFrameHeight;
	XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(fovAngleY), m_aspectRatio, 1.0f, 125.0f);
	XMMATRIX viewProj = view * proj;

	MappedCB->projectionToWorld = XMMatrixTranspose(XMMatrixInverse(nullptr, viewProj));
	//MappedCB->lightPosition = vec4(0.0f, 1.8f, -3.0f, 0.0f);
	MappedCB->DirLight_color = XMFLOAT3(1.0f, 1.0f, 1.0f);
	MappedCB->DirLight_intencity = 1.0f;
	vec4 dir = vec4(1, 1, 1, 0);
	dir /= dir.len3;
	MappedCB->DirLight_invDirection = dir.f3;
}

void RayTracingDevice::SetRaytracingCamera(vec4 CameraPos, vec4 look, vec4 up)
{
	vec4 at = CameraPos + look;

	gd.raytracing.MappedCB->cameraPosition = CameraPos;
	float fovAngleY = 60.0f;
	XMMATRIX view = XMMatrixLookAtLH(CameraPos, at, up);
	float m_aspectRatio = (float)gd.ClientFrameWidth / (float)gd.ClientFrameHeight;
	XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(fovAngleY), m_aspectRatio, 1.0f, 1000.0f);
	XMMATRIX viewProj = view * proj;
	gd.raytracing.MappedCB->projectionToWorld = XMMatrixTranspose(XMMatrixInverse(nullptr, viewProj));
}

#pragma endregion

#pragma endregion

#pragma region ShapeCode

#pragma region MeshCode

#pragma region RaytracingMeshCode

void RayTracingMesh::MeshAddingMap() {
	Upload_vertexBuffer->Map(0, nullptr, (void**)&pVBMappedStart);
	Upload_indexBuffer->Map(0, nullptr, (void**)&pIBMappedStart);
	UAV_Upload_vertexBuffer->Map(0, nullptr, (void**)&pUAV_VBMappedStart);
}
void RayTracingMesh::MeshAddingUnMap() {
	Upload_vertexBuffer->Unmap(0, nullptr);
	Upload_indexBuffer->Unmap(0, nullptr);
	UAV_Upload_vertexBuffer->Unmap(0, nullptr);
}
void RayTracingMesh::StaticInit()
{
	auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto VbufferDesc = CD3DX12_RESOURCE_DESC::Buffer(Upload_VertexBufferCapacity);
	ThrowIfFailed(gd.pDevice->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&VbufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&Upload_vertexBuffer)));
	Upload_vertexBuffer->SetName(L"UploadVertexBuffer");
	

	auto IbufferDesc = CD3DX12_RESOURCE_DESC::Buffer(Upload_IndexBufferCapacity);
	ThrowIfFailed(gd.pDevice->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&IbufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&Upload_indexBuffer)));
	Upload_indexBuffer->SetName(L"UploadSceneIndexBuffer");
	

	uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto UAV_VbufferDesc = CD3DX12_RESOURCE_DESC::Buffer(UAV_Upload_VertexBufferCapacity);
	ThrowIfFailed(gd.pDevice->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&UAV_VbufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&UAV_Upload_vertexBuffer)));
	UAV_Upload_vertexBuffer->SetName(L"UploadUAV_VertexBuffer");
	

	////////////////

	auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto DefaultVbufferDesc = CD3DX12_RESOURCE_DESC::Buffer(VertexBufferCapacity);
	ThrowIfFailed(gd.pDevice->CreateCommittedResource(
		&defaultHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&DefaultVbufferDesc,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&vertexBuffer)));
	vertexBuffer->SetName(L"SceneVertexBuffer");
	//vertexBuffer->Map(0, nullptr, (void**) & pVBMappedStart);

	auto DefaultIbufferDesc = CD3DX12_RESOURCE_DESC::Buffer(IndexBufferCapacity);
	ThrowIfFailed(gd.pDevice->CreateCommittedResource(
		&defaultHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&DefaultIbufferDesc,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&indexBuffer)));
	indexBuffer->SetName(L"SceneIndexBuffer");
	//indexBuffer->Map(0, nullptr, (void**)&pIBMappedStart);

	auto UAV_DefaultVbufferDesc = CD3DX12_RESOURCE_DESC::Buffer(UAV_VertexBufferCapacity);
	UAV_DefaultVbufferDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	ThrowIfFailed(gd.pDevice->CreateCommittedResource(
		&defaultHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&UAV_DefaultVbufferDesc,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&UAV_vertexBuffer)));
	UAV_vertexBuffer->SetName(L"UAV_SceneVertexBuffer");
	//vertexBuffer->Map(0, nullptr, (void**) & pVBMappedStart);

	//Make SRV Table Of Vertex And Index Buffer
	gd.ShaderVisibleDescPool.ImmortalAlloc(&VBIB_DescIndex, 2);
	DescHandle dh = VBIB_DescIndex.hCreation;
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc_VB = {};
	srvDesc_VB.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc_VB.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc_VB.Buffer.NumElements = VertexBufferCapacity / sizeof(Vertex);
	srvDesc_VB.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc_VB.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	srvDesc_VB.Buffer.StructureByteStride = sizeof(Vertex);
	gd.pDevice->CreateShaderResourceView(vertexBuffer, &srvDesc_VB, dh.hcpu);
	dh += gd.CBVSRVUAVSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc_IB = {};
	srvDesc_IB.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc_IB.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc_IB.Buffer.NumElements = IndexBufferCapacity / sizeof(unsigned int);
	srvDesc_IB.Format = DXGI_FORMAT_UNKNOWN;//DXGI_FORMAT_R32_TYPELESS;
	srvDesc_IB.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;//D3D12_BUFFER_SRV_FLAG_RAW;
	srvDesc_IB.Buffer.StructureByteStride = sizeof(unsigned int);//0;
	gd.pDevice->CreateShaderResourceView(indexBuffer, &srvDesc_IB, dh.hcpu);

	//////////////
	//UAV 버전
	gd.ShaderVisibleDescPool.ImmortalAlloc(&UAV_VBIB_DescIndex, 2);
	dh = UAV_VBIB_DescIndex.hCreation;
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc_UAV_VB = {};
	srvDesc_UAV_VB.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc_UAV_VB.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc_UAV_VB.Buffer.NumElements = UAV_VertexBufferCapacity / sizeof(Vertex);
	srvDesc_UAV_VB.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc_UAV_VB.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	srvDesc_UAV_VB.Buffer.StructureByteStride = sizeof(Vertex);
	gd.pDevice->CreateShaderResourceView(UAV_vertexBuffer, &srvDesc_UAV_VB, dh.hcpu);
	dh += gd.CBVSRVUAVSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc_UAV_IB = {};
	srvDesc_UAV_IB.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc_UAV_IB.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc_UAV_IB.Buffer.NumElements = IndexBufferCapacity / sizeof(unsigned int);
	srvDesc_UAV_IB.Format = DXGI_FORMAT_UNKNOWN;//DXGI_FORMAT_R32_TYPELESS;
	srvDesc_UAV_IB.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;//D3D12_BUFFER_SRV_FLAG_RAW;
	srvDesc_UAV_IB.Buffer.StructureByteStride = sizeof(unsigned int);//0;
	gd.pDevice->CreateShaderResourceView(indexBuffer, &srvDesc_UAV_IB, dh.hcpu);
}

void RayTracingMesh::AllocateRaytracingMesh(vector<Vertex> vbarr, vector<TriangleIndex> ibarr, int SubMeshNum, int* SubMeshIndexes)
{
	subMeshCount = SubMeshNum;
	int* SubMeshIndexArr = SubMeshIndexes;
	if (SubMeshIndexes == nullptr) {
		subMeshCount = 1;
		SubMeshIndexArr = new int[2];
		SubMeshIndexArr[0] = 0;
		SubMeshIndexArr[1] = ibarr.size();
	}

	static bool VBisFulling = false;
	static UINT RequireByteVB = 0;
	static bool IBisFulling = false;
	static UINT RequireByteIB = 0;

	if (vertexBuffer == nullptr) {
		RayTracingMesh::StaticInit();
	}

	if (gd.raytracing.ASBuild_ScratchResource == nullptr) {
		AllocateUAVBuffer(gd.raytracing.dxrDevice, gd.raytracing.ASBuild_ScratchResource_Maxsiz, &gd.raytracing.ASBuild_ScratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");
	}

	MeshAddingMap();

	constexpr UINT64 VBAlign = 768; //2816;
	constexpr UINT64 IBAlign = 768; //768; // 삼각형의 첫번째 인덱스로 나열되야함.

	int addtionalVB_Bytesiz = vbarr.size() * sizeof(RayTracingMesh::Vertex);
	int addtionalIB_Bytesiz = 0;
	/*vector<int> IBByteStart(subMeshCount);
	for (int i = 0; i < subMeshCount; ++i) {
		IBByteStart[i] = addtionalIB_Bytesiz;
		addtionalIB_Bytesiz += (SubMeshIndexArr[i + 1] - SubMeshIndexArr[i]) * sizeof(UINT);
		addtionalIB_Bytesiz = IBAlign * (1 + ((addtionalIB_Bytesiz - 1) / IBAlign));
	}*/
	addtionalIB_Bytesiz = ibarr.size() * sizeof(TriangleIndex);

	bool vb_con = VertexBufferByteSize + addtionalVB_Bytesiz <= VertexBufferCapacity;
	bool ib_con = IndexBufferByteSize + addtionalIB_Bytesiz <= IndexBufferCapacity;

	IBStartOffset = new UINT64[2]; /*new UINT64[subMeshCount + 1];*/
	if (vb_con && ib_con) {
		// Create New Mesh
		VBStartOffset = VertexBufferByteSize;
		/*for (int i = 0; i < subMeshCount; ++i) {
			IBStartOffset[i] = IndexBufferByteSize + IBByteStart[i];
		}*/
		//IBStartOffset[subMeshCount] = IndexBufferByteSize + addtionalIB_Bytesiz;
		IBStartOffset[0] = IndexBufferByteSize;
		IBStartOffset[1] = IndexBufferByteSize + addtionalIB_Bytesiz;

		pVBMapped = &pVBMappedStart[0];
		pIBMapped = &pIBMappedStart[0];

		// Copy Mesh Data
		memcpy(pVBMapped, vbarr.data(), addtionalVB_Bytesiz);
		memcpy(pIBMapped, ibarr.data(), addtionalIB_Bytesiz);
		/*for (int i = 0; i < subMeshCount; ++i) {
			memcpy(pIBMapped + IBByteStart[i], (char*)ibarr.data() + SubMeshIndexArr[i] * sizeof(UINT), (SubMeshIndexArr[i + 1] - SubMeshIndexArr[i]) * sizeof(UINT));
			UINT lastIndex = *((UINT*)ibarr.data() + SubMeshIndexArr[i + 1] - 1);
			for (int k = 0; k < IBAlign; ++k) {
				((UINT*)(pIBMapped + IBByteStart[i]))[SubMeshIndexArr[i + 1] - SubMeshIndexArr[i] + k] = lastIndex;
			}
		}*/

		//Build BLAS
		ID3D12Device5* device = gd.raytracing.dxrDevice;
		ID3D12GraphicsCommandList4* commandList = gd.raytracing.dxrCommandList;

		bool initialCmdStateClose = gd.gpucmd.isClose;
		if (gd.gpucmd.isClose) {
			gd.gpucmd.Reset(true);
		}

		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
		commandList->ResourceBarrier(1, &barrier);
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(Upload_vertexBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_SOURCE);
		commandList->ResourceBarrier(1, &barrier);

		//dbglog1(L"VBSiz : %d \n", addtionalVB_Bytesiz);
		commandList->CopyBufferRegion(vertexBuffer, VBStartOffset, Upload_vertexBuffer, 0, addtionalVB_Bytesiz);

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(1, &barrier);
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(Upload_vertexBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
		commandList->ResourceBarrier(1, &barrier);

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(indexBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
		commandList->ResourceBarrier(1, &barrier);
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(Upload_indexBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_SOURCE);
		commandList->ResourceBarrier(1, &barrier);

		//dbglog1(L"IBSiz : %d \n", addtionalIB_Bytesiz);
		commandList->CopyBufferRegion(indexBuffer, IBStartOffset[0], Upload_indexBuffer, 0, addtionalIB_Bytesiz);

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(indexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(1, &barrier);
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(Upload_indexBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
		commandList->ResourceBarrier(1, &barrier);

		gd.gpucmd.Close(true);
		gd.gpucmd.Execute(true);
		gd.WaitGPUComplete();

		////Geometry
		////느려서 폐기한 코드.
		///*
		//* 그럼 왜 느린가? 다양한 Geometry가 하나의 BLAS에 위치하면, BLAS는 Geometry마다 각자 다른 AABB를 할당한다.
		//* 하지만 AABB가 곂치게 되면(대부분의 서브메쉬의 AABB의 영역은 곂칠 수 밖에 없음.), 결국 Ray가 쏘아졌을때 두 AABB중 가장 가까운 삼각형이 항상 가까운 AABB에 있다고 보장을 하지 못하기 때문에, 결국 두 Geometry에 대한 AABB를 검사하게 되고, 그것이 프레임을 낮춘다.
		//*/
		//GeometryDescs = new D3D12_RAYTRACING_GEOMETRY_DESC[subMeshCount];
		//for (int i = 0; i < subMeshCount; ++i) {
		//	GeometryDescs[i].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		//	GeometryDescs[i].Triangles.IndexBuffer = indexBuffer->GetGPUVirtualAddress() + IBStartOffset[i];
		//	GeometryDescs[i].Triangles.IndexCount = (SubMeshIndexArr[i + 1] - SubMeshIndexArr[i]);
		//	GeometryDescs[i].Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
		//	GeometryDescs[i].Triangles.Transform3x4 = 0;
		//	GeometryDescs[i].Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		//	GeometryDescs[i].Triangles.VertexCount = vbarr.size();
		//	GeometryDescs[i].Triangles.VertexBuffer.StartAddress = vertexBuffer->GetGPUVirtualAddress() + VBStartOffset;
		//	GeometryDescs[i].Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
		//	GeometryDescs[i].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE; // Closest Hit Shader
		//}
		GeometryDescs = new D3D12_RAYTRACING_GEOMETRY_DESC();
		GeometryDescs->Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		GeometryDescs->Triangles.IndexBuffer = indexBuffer->GetGPUVirtualAddress() + IBStartOffset[0];
		GeometryDescs->Triangles.IndexCount = SubMeshIndexArr[subMeshCount];
		GeometryDescs->Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
		GeometryDescs->Triangles.Transform3x4 = 0;
		GeometryDescs->Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		GeometryDescs->Triangles.VertexCount = vbarr.size();
		GeometryDescs->Triangles.VertexBuffer.StartAddress = vertexBuffer->GetGPUVirtualAddress() + VBStartOffset;
		GeometryDescs->Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
		GeometryDescs->Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE; // Closest Hit Shader

		//BLAS Input
		BLAS_Input.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		BLAS_Input.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		BLAS_Input.NumDescs = 1;
		BLAS_Input.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		BLAS_Input.pGeometryDescs = GeometryDescs;

		//Prebuild Info
		gd.raytracing.dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&BLAS_Input, &bottomLevelPrebuildInfo);
		if (bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes <= 0) {
			throw "bottomLevelPrebuildInfo Create Failed.";
		}
		if (bottomLevelPrebuildInfo.ScratchDataSizeInBytes > gd.raytracing.ASBuild_ScratchResource_Maxsiz) {
			gd.raytracing.ASBuild_ScratchResource->Release();
			gd.raytracing.ASBuild_ScratchResource = nullptr;
			AllocateUAVBuffer(gd.raytracing.dxrDevice, bottomLevelPrebuildInfo.ScratchDataSizeInBytes, &gd.raytracing.ASBuild_ScratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");
			gd.raytracing.ASBuild_ScratchResource_Maxsiz = bottomLevelPrebuildInfo.ScratchDataSizeInBytes;
		}

		//Create BLAS Res
		D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		AllocateUAVBuffer(gd.raytracing.dxrDevice, bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &BLAS, initialResourceState, L"BottomLevelAccelerationStructure");

		// BLAS Build Desc
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
		bottomLevelBuildDesc.Inputs = BLAS_Input;
		bottomLevelBuildDesc.ScratchAccelerationStructureData = gd.raytracing.ASBuild_ScratchResource->GetGPUVirtualAddress();
		bottomLevelBuildDesc.DestAccelerationStructureData = BLAS->GetGPUVirtualAddress();

		if (gd.gpucmd.isClose) {
			gd.gpucmd.Reset(true);
		}

		CD3DX12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(BLAS);
		commandList->ResourceBarrier(1, &uavBarrier);

		commandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);

		/*gd.CmdClose(commandList);
		ID3D12CommandList* ppd3dCommandLists[] = { commandList };
		commandQueue->ExecuteCommandLists(1, ppd3dCommandLists);
		gd.WaitGPUComplete();*/

		MeshDefaultInstanceData.Transform[0][0] = MeshDefaultInstanceData.Transform[1][1] = MeshDefaultInstanceData.Transform[2][2] = 1;
		MeshDefaultInstanceData.InstanceMask = 1;
		MeshDefaultInstanceData.AccelerationStructure = BLAS->GetGPUVirtualAddress();
		MeshDefaultInstanceData.InstanceContributionToHitGroupIndex = 0;

		VertexBufferByteSize += addtionalVB_Bytesiz;
		VertexBufferByteSize = VBAlign * (1 + ((VertexBufferByteSize - 1) / VBAlign));
		//VertexBufferByteSize = ((VertexBufferByteSize + 255) & ~255);

		IndexBufferByteSize += addtionalIB_Bytesiz;
		IndexBufferByteSize = IBAlign * (1 + ((IndexBufferByteSize - 1) / IBAlign));
		//IndexBufferByteSize = ((IndexBufferByteSize + 255) & ~255);
		/*if (initialCmdStateClose == false) {
			gd.CmdReset(commandList, commandAllocator);
		}*/
	}
	else {
		if (vb_con == false) {
			if (VBisFulling) {
				RequireByteVB += addtionalVB_Bytesiz;
				dbglog1(L"Raytracing VB additional capacity require! %d \n", RequireByteVB);
			}
			else {
				dbglog1(L"Raytracing VB additional capacity require! %d \n", VertexBufferByteSize + addtionalVB_Bytesiz - VertexBufferCapacity);
				RequireByteVB += VertexBufferByteSize + addtionalVB_Bytesiz - VertexBufferCapacity;
				VBisFulling = true;
			}
		}
		if (RequireByteIB > 0) {
			if (IBisFulling) {
				RequireByteIB += addtionalIB_Bytesiz;
				dbglog1(L"Raytracing IB additional capacity require! %d \n", RequireByteIB);
			}
		}
		else {
			if (IBisFulling == false) {
				IBisFulling = true;
				RequireByteIB = IndexBufferByteSize + addtionalIB_Bytesiz - IndexBufferCapacity;
			}
			else {
				RequireByteIB += addtionalIB_Bytesiz;
			}
		}
	}

	MeshAddingUnMap();
}

void RayTracingMesh::AllocateRaytracingUAVMesh(vector<Vertex> vbarr, UINT64* inIBStartOffset, int SubMeshNum, int* SubMeshIndexes)
{
	static bool VBisFulling = false;
	static UINT RequireByteVB = 0;
	static bool IBisFulling = false;
	static UINT RequireByteIB = 0;

	if (UAV_vertexBuffer == nullptr) {
		RayTracingMesh::StaticInit();
	}

	if (gd.raytracing.ASBuild_ScratchResource == nullptr) {
		AllocateUAVBuffer(gd.raytracing.dxrDevice, gd.raytracing.ASBuild_ScratchResource_Maxsiz, &gd.raytracing.ASBuild_ScratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");
	}

	MeshAddingMap();

	constexpr UINT64 VBAlign = 768; //2816;
	constexpr UINT64 IBAlign = 256; //768;

	int addtionalVB_Bytesiz = vbarr.size() * sizeof(RayTracingMesh::Vertex);
	IBStartOffset = inIBStartOffset;
	bool vb_con = UAV_VertexBufferByteSize + addtionalVB_Bytesiz <= UAV_VertexBufferCapacity;
	if (vb_con) {
		// Create New Mesh
		UAV_VBStartOffset = UAV_VertexBufferByteSize;
		pUAV_VBMapped = &pVBMappedStart[0];

		// Copy Mesh Data
		memcpy(pUAV_VBMapped, vbarr.data(), addtionalVB_Bytesiz);

		//Build BLAS
		ID3D12Device5* device = gd.raytracing.dxrDevice;
		ID3D12GraphicsCommandList4* commandList = gd.raytracing.dxrCommandList;

		bool initialCmdStateClose = gd.gpucmd.isClose;
		if (gd.gpucmd.isClose) {
			gd.gpucmd.Reset(true);
		}

		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(UAV_vertexBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
		commandList->ResourceBarrier(1, &barrier);
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(UAV_Upload_vertexBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_SOURCE);
		commandList->ResourceBarrier(1, &barrier);

		//dbglog1(L"VBSiz : %d \n", addtionalVB_Bytesiz);
		commandList->CopyBufferRegion(UAV_vertexBuffer, UAV_VBStartOffset, UAV_Upload_vertexBuffer, 0, addtionalVB_Bytesiz);

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(UAV_vertexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(1, &barrier);
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(UAV_Upload_vertexBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
		commandList->ResourceBarrier(1, &barrier);

		gd.gpucmd.Close(true);
		gd.gpucmd.Execute(true);
		gd.WaitGPUComplete();

		//Geometry
		//GeometryDescs = new D3D12_RAYTRACING_GEOMETRY_DESC[SubMeshNum];
		//for (int i = 0; i < SubMeshNum; ++i) {
		//	GeometryDescs[i].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		//	GeometryDescs[i].Triangles.IndexBuffer = indexBuffer->GetGPUVirtualAddress() + inIBStartOffset[i];
		//	GeometryDescs[i].Triangles.IndexCount = (SubMeshIndexes[i + 1] - SubMeshIndexes[i]);
		//	GeometryDescs[i].Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
		//	GeometryDescs[i].Triangles.Transform3x4 = 0;
		//	GeometryDescs[i].Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		//	GeometryDescs[i].Triangles.VertexCount = vbarr.size();
		//	GeometryDescs[i].Triangles.VertexBuffer.StartAddress = UAV_vertexBuffer->GetGPUVirtualAddress() + UAV_VBStartOffset;
		//	GeometryDescs[i].Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
		//	GeometryDescs[i].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE; // Closest Hit Shader
		//}
		GeometryDescs = new D3D12_RAYTRACING_GEOMETRY_DESC();
		GeometryDescs->Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		GeometryDescs->Triangles.IndexBuffer = indexBuffer->GetGPUVirtualAddress() + inIBStartOffset[0];
		GeometryDescs->Triangles.IndexCount = SubMeshIndexes[subMeshCount];
		GeometryDescs->Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
		GeometryDescs->Triangles.Transform3x4 = 0;
		GeometryDescs->Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		GeometryDescs->Triangles.VertexCount = vbarr.size();
		GeometryDescs->Triangles.VertexBuffer.StartAddress = UAV_vertexBuffer->GetGPUVirtualAddress() + UAV_VBStartOffset;
		GeometryDescs->Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
		GeometryDescs->Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE; // Closest Hit Shader

		//BLAS Input
		BLAS_Input.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		BLAS_Input.Flags =
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE |
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
		BLAS_Input.NumDescs = 1;
		BLAS_Input.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		BLAS_Input.pGeometryDescs = GeometryDescs;

		//Prebuild Info
		gd.raytracing.dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&BLAS_Input, &bottomLevelPrebuildInfo);
		if (bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes <= 0) {
			throw "bottomLevelPrebuildInfo Create Failed.";
		}

		if (bottomLevelPrebuildInfo.ScratchDataSizeInBytes > gd.raytracing.ASBuild_ScratchResource_Maxsiz) {
			gd.raytracing.ASBuild_ScratchResource->Release();
			gd.raytracing.ASBuild_ScratchResource = nullptr;
			AllocateUAVBuffer(gd.raytracing.dxrDevice, bottomLevelPrebuildInfo.ScratchDataSizeInBytes, &gd.raytracing.ASBuild_ScratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");
			gd.raytracing.ASBuild_ScratchResource_Maxsiz = bottomLevelPrebuildInfo.ScratchDataSizeInBytes;
		}

		//Create BLAS Res
		D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		AllocateUAVBuffer(gd.raytracing.dxrDevice, bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &BLAS, initialResourceState, L"BottomLevelAccelerationStructure");

		// BLAS Build Desc
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
		bottomLevelBuildDesc.Inputs = BLAS_Input;
		bottomLevelBuildDesc.ScratchAccelerationStructureData = gd.raytracing.ASBuild_ScratchResource->GetGPUVirtualAddress();
		bottomLevelBuildDesc.DestAccelerationStructureData = BLAS->GetGPUVirtualAddress();

		if (gd.gpucmd.isClose) {
			gd.gpucmd.Reset(true);
		}

		CD3DX12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(BLAS);
		commandList->ResourceBarrier(1, &uavBarrier);

		commandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);

		/*gd.CmdClose(commandList);
		ID3D12CommandList* ppd3dCommandLists[] = { commandList };
		commandQueue->ExecuteCommandLists(1, ppd3dCommandLists);
		gd.WaitGPUComplete();*/
		// 이 주석은 애초에 Reset 상태에서 이 함수를 호출하기 때문에 주석을 처리함.
		// 만약 Reset이 아니라면 이걸 해주는 것이 맞다.
		// fix : 이 함수가 어떤 커맨드 상태로도 실행될 수 있도록 만드는 것.

		MeshDefaultInstanceData.Transform[0][0] = MeshDefaultInstanceData.Transform[1][1] = MeshDefaultInstanceData.Transform[2][2] = 1;
		MeshDefaultInstanceData.InstanceMask = 1;
		MeshDefaultInstanceData.AccelerationStructure = BLAS->GetGPUVirtualAddress();
		MeshDefaultInstanceData.InstanceContributionToHitGroupIndex = 0;

		UAV_VertexBufferByteSize += addtionalVB_Bytesiz;
		UAV_VertexBufferByteSize = VBAlign * (1 + ((UAV_VertexBufferByteSize - 1) / VBAlign));
	}
	else {
		if (vb_con == false) {
			if (VBisFulling) {
				RequireByteVB += addtionalVB_Bytesiz;
				dbglog1(L"Raytracing VB additional capacity require! %d \n", RequireByteVB);
			}
			else {
				dbglog1(L"Raytracing VB additional capacity require! %d \n", UAV_VertexBufferByteSize + addtionalVB_Bytesiz - UAV_VertexBufferCapacity);
				RequireByteVB += UAV_VertexBufferByteSize + addtionalVB_Bytesiz - UAV_VertexBufferCapacity;
				VBisFulling = true;
			}
		}
	}

	MeshAddingUnMap();
}

void RayTracingMesh::AllocateRaytracingUAVMesh_OnlyIndex(vector<TriangleIndex> ibarr, int SubMeshNum, int* SubMeshIndexes)
{
	subMeshCount = SubMeshNum;
	int* SubMeshIndexArr = SubMeshIndexes;
	if (SubMeshIndexes == nullptr) {
		subMeshCount = 1;
		SubMeshIndexArr = new int[2];
		SubMeshIndexArr[0] = 0;
		SubMeshIndexArr[1] = ibarr.size();
	}

	static bool VBisFulling = false;
	static UINT RequireByteVB = 0;
	static bool IBisFulling = false;
	static UINT RequireByteIB = 0;

	if (vertexBuffer == nullptr) {
		RayTracingMesh::StaticInit();
	}

	if (gd.raytracing.ASBuild_ScratchResource == nullptr) {
		AllocateUAVBuffer(gd.raytracing.dxrDevice, gd.raytracing.ASBuild_ScratchResource_Maxsiz, &gd.raytracing.ASBuild_ScratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");
	}

	MeshAddingMap();

	constexpr UINT64 VBAlign = 768; //2816;
	constexpr UINT64 IBAlign = 256;//768;

	int addtionalIB_Bytesiz = 0;
	/*vector<int> IBByteStart(subMeshCount);
	for (int i = 0; i < subMeshCount; ++i) {
		IBByteStart[i] = addtionalIB_Bytesiz;
		addtionalIB_Bytesiz += (SubMeshIndexArr[i + 1] - SubMeshIndexArr[i]) * sizeof(UINT);
		addtionalIB_Bytesiz = IBAlign * (1 + ((addtionalIB_Bytesiz - 1) / IBAlign));
	}*/
	addtionalIB_Bytesiz = ibarr.size() * sizeof(TriangleIndex);

	bool ib_con = IndexBufferByteSize + addtionalIB_Bytesiz <= IndexBufferCapacity;
	//IBStartOffset = new UINT64[subMeshCount + 1];
	IBStartOffset = new UINT64[2];
	if (ib_con) {
		// Create New Mesh
		/*for (int i = 0; i < subMeshCount; ++i) {
			IBStartOffset[i] = IndexBufferByteSize + IBByteStart[i];
		}
		IBStartOffset[subMeshCount] = IndexBufferByteSize + addtionalIB_Bytesiz;*/
		IBStartOffset[0] = IndexBufferByteSize;
		IBStartOffset[1] = IndexBufferByteSize + addtionalIB_Bytesiz;

		pIBMapped = &pIBMappedStart[0];

		// Copy Mesh Data
		/*for (int i = 0; i < subMeshCount; ++i) {
			memcpy(pIBMapped + IBByteStart[i], (char*)ibarr.data() + SubMeshIndexArr[i] * sizeof(UINT), (SubMeshIndexArr[i + 1] - SubMeshIndexArr[i]) * sizeof(UINT));
			UINT lastIndex = *((UINT*)ibarr.data() + SubMeshIndexArr[i + 1] - 1);
			for (int k = 0; k < IBAlign; ++k) {
				((UINT*)(pIBMapped + IBByteStart[i]))[SubMeshIndexArr[i + 1] - SubMeshIndexArr[i] + k] = lastIndex;
			}
		}*/
		memcpy(pIBMapped, ibarr.data(), addtionalIB_Bytesiz);

		//Build BLAS
		ID3D12Device5* device = gd.raytracing.dxrDevice;
		ID3D12GraphicsCommandList4* commandList = gd.raytracing.dxrCommandList;

		bool initialCmdStateClose = gd.gpucmd.isClose;
		if (gd.gpucmd.isClose) {
			gd.gpucmd.Reset(true);
		}

		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(indexBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
		commandList->ResourceBarrier(1, &barrier);
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(Upload_indexBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_SOURCE);
		commandList->ResourceBarrier(1, &barrier);

		//dbglog1(L"IBSiz : %d \n", addtionalIB_Bytesiz);
		commandList->CopyBufferRegion(indexBuffer, IBStartOffset[0], Upload_indexBuffer, 0, addtionalIB_Bytesiz);

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(indexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(1, &barrier);
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(Upload_indexBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);
		commandList->ResourceBarrier(1, &barrier);

		gd.gpucmd.Close(true);
		gd.gpucmd.Execute(true);
		gd.WaitGPUComplete();

		if (gd.gpucmd.isClose) {
			gd.gpucmd.Reset(true);
		}

		IndexBufferByteSize += addtionalIB_Bytesiz;
		IndexBufferByteSize = IBAlign * (1 + ((IndexBufferByteSize - 1) / IBAlign));
	}
	else {
		if (RequireByteIB > 0) {
			if (IBisFulling) {
				RequireByteIB += addtionalIB_Bytesiz;
				dbglog1(L"Raytracing IB additional capacity require! %d \n", RequireByteIB);
			}
		}
		else {
			if (IBisFulling == false) {
				IBisFulling = true;
				RequireByteIB = IndexBufferByteSize + addtionalIB_Bytesiz - IndexBufferCapacity;
			}
			else {
				RequireByteIB += addtionalIB_Bytesiz;
			}
		}
	}

	MeshAddingUnMap();
}

void RayTracingMesh::UAV_BLAS_Refit()
{
	//Build BLAS
	ID3D12Device5* device = gd.raytracing.dxrDevice;
	ID3D12GraphicsCommandList4* commandList = gd.raytracing.dxrCommandList;

	D3D12_GPU_VIRTUAL_ADDRESS UsingScratchBufferVA;

	//Prebuild Info
	BLAS_Input.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;

	gd.raytracing.dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&BLAS_Input, &bottomLevelPrebuildInfo);
	if (bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes <= 0) {
		throw "bottomLevelPrebuildInfo Create Failed.";
	}
	if (gd.raytracing.UsingScratchSize + bottomLevelPrebuildInfo.ScratchDataSizeInBytes > gd.raytracing.ASBuild_ScratchResource_Maxsiz) {
		// 이전의 Scratched Buffer 사용을 모두 끝낸다.
		gd.gpucmd.Close(true);
		gd.gpucmd.Execute(true);
		gd.gpucmd.WaitGPUComplete();
		gd.gpucmd.Reset(true);
		gd.raytracing.UsingScratchSize = 0;
	}

	UINT64 AllocSiz = bottomLevelPrebuildInfo.ScratchDataSizeInBytes;
	AllocSiz = 256 * (1 + ((AllocSiz - 1) / 256));
	UsingScratchBufferVA = gd.raytracing.ASBuild_ScratchResource->GetGPUVirtualAddress() + gd.raytracing.UsingScratchSize;
	gd.raytracing.UsingScratchSize += AllocSiz;

	// BLAS Build Desc
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
	bottomLevelBuildDesc.Inputs = BLAS_Input;
	bottomLevelBuildDesc.ScratchAccelerationStructureData = UsingScratchBufferVA;

	// 이둘을 같게하면 Refit함. SourceAccelerationStructureData 0이면 build.
	bottomLevelBuildDesc.SourceAccelerationStructureData = BLAS->GetGPUVirtualAddress();
	bottomLevelBuildDesc.DestAccelerationStructureData = BLAS->GetGPUVirtualAddress();

	CD3DX12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(BLAS);
	commandList->ResourceBarrier(1, &uavBarrier);

	commandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
}

void RayTracingMesh::Release() {
	pVBMapped = nullptr;
	pIBMapped = nullptr;
	pUAV_VBMapped = nullptr;
	VBStartOffset = 0;
	if (IBStartOffset) {
		delete[] IBStartOffset;
		IBStartOffset = nullptr;
	}
	UAV_VBStartOffset = 0;
	subMeshCount = 0;
	if (BLAS) {
		BLAS->Release();
		BLAS = nullptr;
	}
	if (GeometryDescs) {
		delete GeometryDescs;
		GeometryDescs = nullptr;
	}
}

#pragma endregion

#pragma region BasicMeshCode

Mesh::~Mesh()
{
}

void Mesh::InstancingInit()
{
	InstanceData = new InstancingStruct[subMeshNum];
	for (int i = 0; i < subMeshNum; ++i) {
		InstanceData[i].Init(16, this);
	}
}

void Mesh::SetOBBDataWithAABB(XMFLOAT3 minpos, XMFLOAT3 maxpos)
{
	vec4 max = maxpos;
	vec4 min = minpos;
	vec4 mid = 0.5f * (max + min);
	vec4 ext = max - mid;
	OBB_Ext = ext.f3;
	OBB_Tr = mid.f3;
}

void Mesh::ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering) {
	std::vector< Vertex > temp_vertices;
	std::vector< TriangleIndex > TrianglePool;
	XMFLOAT3 maxPos = { 0, 0, 0 };
	XMFLOAT3 minPos = { 0, 0, 0 };
	std::ifstream in{ path };

	std::vector< XMFLOAT3 > temp_pos;
	std::vector< XMFLOAT2 > temp_uv;
	std::vector< XMFLOAT3 > temp_normal;
	while (in.eof() == false && (in.is_open() && in.fail() == false)) {
		char rstr[128] = {};
		in >> rstr;
		if (strcmp(rstr, "v") == 0) {
			//좌표
			XMFLOAT3 pos;
			in >> pos.x;
			in >> pos.y;
			in >> pos.z;
			if (maxPos.x < pos.x) maxPos.x = pos.x;
			if (maxPos.y < pos.y) maxPos.y = pos.y;
			if (maxPos.z < pos.z) maxPos.z = pos.z;
			if (minPos.x > pos.x) minPos.x = pos.x;
			if (minPos.y > pos.y) minPos.y = pos.y;
			if (minPos.z > pos.z) minPos.z = pos.z;
			temp_pos.push_back(pos);
		}
		else if (strcmp(rstr, "vt") == 0) {
			// uv 좌표
			XMFLOAT3 uv;
			in >> uv.x;
			in >> uv.y;
			in >> uv.z;
			temp_uv.push_back(XMFLOAT2(uv.x, uv.y));
		}
		else if (strcmp(rstr, "vn") == 0) {
			// 노멀
			XMFLOAT3 normal;
			in >> normal.x;
			in >> normal.y;
			in >> normal.z;
			temp_normal.push_back(normal);
		}
		else if (strcmp(rstr, "f") == 0) {
			std::string vertex1, vertex2, vertex3;
			unsigned int vertexIndex[3], uvIndex[3], normalIndex[3];
			char blank;

			in >> vertexIndex[0];
			in >> blank;
			in >> uvIndex[0];
			in >> blank;
			in >> normalIndex[0];
			temp_vertices.push_back(Vertex(temp_pos[vertexIndex[0] - 1], color, temp_normal[normalIndex[0] - 1]));

			//in >> blank;
			in >> vertexIndex[1];
			in >> blank;
			in >> uvIndex[1];
			in >> blank;
			in >> normalIndex[1];
			temp_vertices.push_back(Vertex(temp_pos[vertexIndex[1] - 1], color, temp_normal[normalIndex[1] - 1]));

			//in >> blank;
			in >> vertexIndex[2];
			in >> blank;
			in >> uvIndex[2];
			in >> blank;
			in >> normalIndex[2];
			//in >> blank;
			temp_vertices.push_back(Vertex(temp_pos[vertexIndex[2] - 1], color, temp_normal[normalIndex[2] - 1]));

			int n = temp_vertices.size() - 1;
			TrianglePool.push_back(TriangleIndex(n - 2, n - 1, n));
		}
	}
	// For each vertex of each triangle

	in.close();

	XMFLOAT3 CenterPos;
	if (centering) {
		CenterPos.x = (maxPos.x + minPos.x) * 0.5f;
		CenterPos.y = (maxPos.y + minPos.y) * 0.5f;
		CenterPos.z = (maxPos.z + minPos.z) * 0.5f;
	}
	else {
		CenterPos.x = 0;
		CenterPos.y = 0;
		CenterPos.z = 0;
	}

	XMFLOAT3 AABB[2] = {};
	AABB[0] = { 0, 0, 0 };
	AABB[1] = { 0, 0, 0 };

	for (int i = 0; i < temp_vertices.size(); ++i) {
		temp_vertices[i].position.x -= CenterPos.x;
		temp_vertices[i].position.y -= CenterPos.y;
		temp_vertices[i].position.z -= CenterPos.z;
		temp_vertices[i].position.x /= 100.0f;
		temp_vertices[i].position.y /= 100.0f;
		temp_vertices[i].position.z /= 100.0f;

		if (temp_vertices[i].position.x < AABB[0].x) AABB[0].x = temp_vertices[i].position.x;
		if (temp_vertices[i].position.y < AABB[0].y) AABB[0].y = temp_vertices[i].position.y;
		if (temp_vertices[i].position.z < AABB[0].z) AABB[0].z = temp_vertices[i].position.z;

		if (temp_vertices[i].position.x > AABB[1].x) AABB[1].x = temp_vertices[i].position.x;
		if (temp_vertices[i].position.y > AABB[1].y) AABB[1].y = temp_vertices[i].position.y;
		if (temp_vertices[i].position.z > AABB[1].z) AABB[1].z = temp_vertices[i].position.z;
	}

	SetOBBDataWithAABB(AABB[0], AABB[1]);

	int m_nVertices = temp_vertices.size();
	int m_nStride = sizeof(Vertex);
	//m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// error.. why vertex buffer and index buffer do not input? 
	// maybe.. State Error.
	VertexBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, temp_vertices.size() * sizeof(Vertex), 1);
	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, temp_vertices.size() * sizeof(Vertex), 1);
	gd.UploadToCommitedGPUBuffer(&temp_vertices[0], &VertexUploadBuffer, &VertexBuffer, true);
	VertexBuffer.AddResourceBarrierTransitoinToCommand(gd.gpucmd, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	VertexBufferView.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
	VertexBufferView.StrideInBytes = m_nStride;
	VertexBufferView.SizeInBytes = m_nStride * m_nVertices;

	IndexBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(TriangleIndex) * TrianglePool.size(), 1);
	IndexUploadBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(TriangleIndex) * TrianglePool.size(), 1);
	gd.UploadToCommitedGPUBuffer(&TrianglePool[0], &IndexUploadBuffer, &IndexBuffer, true);
	IndexBuffer.AddResourceBarrierTransitoinToCommand(gd.gpucmd, D3D12_RESOURCE_STATE_INDEX_BUFFER);
	IndexBufferView.BufferLocation = IndexBuffer.resource->GetGPUVirtualAddress();
	IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	IndexBufferView.SizeInBytes = sizeof(TriangleIndex) * TrianglePool.size();

	IndexNum = TrianglePool.size() * 3;
	VertexNum = temp_vertices.size();
	topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	/*MeshOBB = BoundingOrientedBox(XMFLOAT3(0.0f, 0.0f, 0.0f), MAXpos, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));*/

	subMeshNum = 1;
	SubMeshIndexStart = new int[2];
	SubMeshIndexStart[0] = 0;
	SubMeshIndexStart[1] = IndexNum;
}

void Mesh::Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum, ui32 slotIndex)
{
	// question : what is StartSlot parameter??
	ui32 SlotNum = 0;
	pCommandList->IASetVertexBuffers(SlotNum, 1, &VertexBufferView);
	pCommandList->IASetPrimitiveTopology(topology);
	if (IndexNum > 0)
	{
		pCommandList->IASetIndexBuffer(&IndexBufferView);
		pCommandList->DrawIndexedInstanced(SubMeshIndexStart[slotIndex + 1] - SubMeshIndexStart[slotIndex], instanceNum, SubMeshIndexStart[slotIndex], 0, 0);
	}
	else
	{
		ui32 VertexOffset = 0;
		pCommandList->DrawInstanced(VertexNum, instanceNum, VertexOffset, 0);
	}
}

// 실제로 쓰이지는 않는 임시 함수.
void Mesh::BatchRender(ID3D12GraphicsCommandList* pCommandList)
{
}

BoundingOrientedBox Mesh::GetOBB()
{
	return BoundingOrientedBox(OBB_Tr, OBB_Ext, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
}

void Mesh::CreateWallMesh(float width, float height, float depth, vec4 color)
{
	std::vector<Vertex> vertices;
	std::vector<UINT> indices;
	// Front
	vertices.push_back(Vertex(XMFLOAT3(-width, -height, -depth), color, XMFLOAT3(0.0f, 0.0f, -1.0f)));
	vertices.push_back(Vertex(XMFLOAT3(width, -height, -depth), color, XMFLOAT3(0.0f, 0.0f, -1.0f)));
	vertices.push_back(Vertex(XMFLOAT3(width, height, -depth), color, XMFLOAT3(0.0f, 0.0f, -1.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-width, height, -depth), color, XMFLOAT3(0.0f, 0.0f, -1.0f)));
	// Back
	vertices.push_back(Vertex(XMFLOAT3(-width, -height, depth), color, XMFLOAT3(0.0f, 0.0f, 1.0f)));
	vertices.push_back(Vertex(XMFLOAT3(width, -height, depth), color, XMFLOAT3(0.0f, 0.0f, 1.0f)));
	vertices.push_back(Vertex(XMFLOAT3(width, height, depth), color, XMFLOAT3(0.0f, 0.0f, 1.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-width, height, depth), color, XMFLOAT3(0.0f, 0.0f, 1.0f)));
	// Top
	vertices.push_back(Vertex(XMFLOAT3(-width, height, -depth), color, XMFLOAT3(0.0f, 1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(width, height, -depth), color, XMFLOAT3(0.0f, 1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(width, height, depth), color, XMFLOAT3(0.0f, 1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-width, height, depth), color, XMFLOAT3(0.0f, 1.0f, 0.0f)));
	// Bottom
	vertices.push_back(Vertex(XMFLOAT3(-width, -height, -depth), color, XMFLOAT3(0.0f, -1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(width, -height, -depth), color, XMFLOAT3(0.0f, -1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(width, -height, depth), color, XMFLOAT3(0.0f, -1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-width, -height, depth), color, XMFLOAT3(0.0f, -1.0f, 0.0f)));
	// Left
	vertices.push_back(Vertex(XMFLOAT3(-width, -height, depth), color, XMFLOAT3(-1.0f, 0.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-width, -height, -depth), color, XMFLOAT3(-1.0f, 0.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-width, height, -depth), color, XMFLOAT3(-1.0f, 0.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-width, height, depth), color, XMFLOAT3(-1.0f, 0.0f, 0.0f)));
	// Right
	vertices.push_back(Vertex(XMFLOAT3(width, -height, -depth), color, XMFLOAT3(1.0f, 0.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(width, -height, depth), color, XMFLOAT3(1.0f, 0.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(width, height, depth), color, XMFLOAT3(1.0f, 0.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(width, height, -depth), color, XMFLOAT3(1.0f, 0.0f, 0.0f)));

	// index
	// front
	indices.push_back(0); indices.push_back(2); indices.push_back(1);
	indices.push_back(2); indices.push_back(0); indices.push_back(3);
	// back
	indices.push_back(4); indices.push_back(5); indices.push_back(6);
	indices.push_back(6); indices.push_back(7); indices.push_back(4);
	// top
	indices.push_back(8); indices.push_back(10); indices.push_back(9);
	indices.push_back(10); indices.push_back(8); indices.push_back(11);
	// bottom
	indices.push_back(12); indices.push_back(13); indices.push_back(14);
	indices.push_back(14); indices.push_back(15); indices.push_back(12);
	// left
	indices.push_back(16); indices.push_back(18); indices.push_back(17);
	indices.push_back(18); indices.push_back(16); indices.push_back(19);
	// right
	indices.push_back(20); indices.push_back(22); indices.push_back(21);
	indices.push_back(22); indices.push_back(20); indices.push_back(23);

	// OBB
	OBB_Tr = { 0, 0, 0 };
	OBB_Ext = XMFLOAT3(width, height, depth);

	int nVertices = vertices.size();
	int nStride = sizeof(Vertex);

	VertexBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	gd.UploadToCommitedGPUBuffer(&vertices[0], &VertexUploadBuffer, &VertexBuffer, true);

	VertexBuffer.AddResourceBarrierTransitoinToCommand(gd.gpucmd, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	VertexBufferView.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
	VertexBufferView.StrideInBytes = nStride;
	VertexBufferView.SizeInBytes = nStride * nVertices;

	IndexBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, indices.size() * sizeof(UINT), 1);
	IndexUploadBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, indices.size() * sizeof(UINT), 1);
	gd.UploadToCommitedGPUBuffer(&indices[0], &IndexUploadBuffer, &IndexBuffer, true);

	IndexBuffer.AddResourceBarrierTransitoinToCommand(gd.gpucmd, D3D12_RESOURCE_STATE_INDEX_BUFFER);

	IndexBufferView.BufferLocation = IndexBuffer.resource->GetGPUVirtualAddress();
	IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	IndexBufferView.SizeInBytes = indices.size() * sizeof(UINT);

	IndexNum = indices.size();
	VertexNum = vertices.size();

	subMeshNum = 1;
	SubMeshIndexStart = new int[2];
	SubMeshIndexStart[0] = 0;
	SubMeshIndexStart[1] = IndexNum;
	topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

void Mesh::InstancingStruct::Init(unsigned int capacity, Mesh* _mesh)
{
	Capacity = capacity;
	InstanceSize = 0;
	mesh = _mesh;
	InstanceDataArr = nullptr;
	StructuredBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(RenderInstanceData) * Capacity, 1);
	StructuredBuffer.resource->Map(0, NULL, (void**)&InstanceDataArr);
	
	if (game.isAssetAddingInGlobal) {
		int index = gd.TextureDescriptorAllotter.Alloc();
		InstancingSRVIndex.Set(false, index);
		//game.RenderInstancingTable.push_back(this);
	}
	else {
		gd.ShaderVisibleDescPool.ImmortalAlloc_InstancingSRV(&InstancingSRVIndex, 1);
		game.RenderInstancingTable.push_back(this);
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = Capacity;
	srvDesc.Buffer.StructureByteStride = sizeof(RenderInstanceData);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	gd.pDevice->CreateShaderResourceView(StructuredBuffer.resource, &srvDesc, InstancingSRVIndex.hCreation.hcpu);
}

int Mesh::InstancingStruct::PushInstance(RenderInstanceData instance)
{
	if (Capacity <= 0) {
		return -1;
	}
	int n = -1;
	if (InstanceSize < Capacity) {
		InstanceDataArr[InstanceSize] = instance;
		dbgbreak(sizeof(RenderInstanceData) != 80);
		n = InstanceSize;
		InstanceSize += 1;
	}
	else {
		int pastCap = Capacity;
		RenderInstanceData* newArr = new RenderInstanceData[Capacity * 2];
		memcpy_s(newArr, sizeof(RenderInstanceData) * Capacity, InstanceDataArr, sizeof(RenderInstanceData) * Capacity);
		Capacity *= 2;

		//release InstanceDataArr and StructuredBuffer
		GPUResource prevRes = StructuredBuffer;

		//Alloc new resource(StructuredBuffer) and cpu mem(InstanceDataArr)
		UINT ElementBytes = (((sizeof(RenderInstanceData) * Capacity) + 255) & ~255);
		StructuredBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, ElementBytes, 1);
		StructuredBuffer.resource->Map(0, NULL, (void**)&InstanceDataArr);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = Capacity;
		srvDesc.Buffer.StructureByteStride = sizeof(RenderInstanceData);
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		gd.pDevice->CreateShaderResourceView(StructuredBuffer.resource, &srvDesc, InstancingSRVIndex.hCreation.hcpu);

		//copy previous Buffer value;
		for (int i = 0; i < pastCap; ++i) {
			InstanceDataArr[i] = newArr[i];
		}

		delete[] newArr;

		InstanceDataArr[InstanceSize] = instance;
		n = InstanceSize;
		InstanceSize += 1;

		gd.ShaderVisibleDescPool.isImmortalChange = true;

		// 지금 릴리즈를 하니 오류가 생김. 그냥 릴리즈 하지 말고 어디에 따로 재활용하게 모아놔야 겠네.
		prevRes.resource->Unmap(0, NULL);
		prevRes.Release();
	}
	return n;
}

void Mesh::InstancingStruct::DestroyInstance(RenderInstanceData* instance)
{
	instance->worldMat.right = 0;
	instance->worldMat.up = 0;
	instance->worldMat.look = 0;
	instance->worldMat.pos = 0;
	instance->MaterialIndex = 0;

	int i = instance - InstanceDataArr;

	//delete logic
	for (int k = InstanceSize - 1; k >= 0; --k) {
		if (InstanceDataArr[k].worldMat.pos.w == 0) {
			InstanceSize -= 1;
		}
		else break;
	}
}

void Mesh::InstancingStruct::ClearInstancing()
{
	InstanceSize = 0;
}

void Mesh::InstancingStruct::Release() {
	if (InstanceDataArr) {
		StructuredBuffer.resource->Unmap(0, nullptr);
		StructuredBuffer.Release();
		InstanceDataArr = nullptr;
	}
	mesh = nullptr; // 참조만 할 뿐이다.
	Capacity = 0;
	InstanceSize = 0;
	InstancingSRVIndex.Set(false, 0, 0);
}

void Mesh::Release()
{
	VertexBuffer.Release();
	VertexUploadBuffer.Release();
	IndexBuffer.Release();
	IndexUploadBuffer.Release();
	for (int i = 0; i < subMeshNum; ++i) {
		InstanceData[i].Release();
	}
	delete[] InstanceData;
	delete[] SubMeshIndexStart;
}

// 구 메쉬 생성
void Mesh::CreateSphereMesh(ID3D12GraphicsCommandList* pCommandList, float radius, int sliceCount, int stackCount, vec4 color)
{
	std::vector<Vertex> vertices;
	std::vector<UINT> indices;

	// 로컬 OBB 
	OBB_Tr = { 0, 0, 0 };
	OBB_Ext = { radius, radius, radius };

	// Vertex 생성
	for (int i = 0; i <= stackCount; ++i)
	{
		float phi = XM_PI * (float)i / (float)stackCount;

		for (int j = 0; j <= sliceCount; ++j)
		{
			float theta = XM_2PI * (float)j / (float)sliceCount;

			XMFLOAT3 pos(
				radius * sinf(phi) * cosf(theta),
				radius * cosf(phi),
				radius * sinf(phi) * sinf(theta)
			);

			// normal = normalize(pos)
			XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&pos));
			XMFLOAT3 normal;
			XMStoreFloat3(&normal, n);

			vertices.push_back(Vertex(pos, color, normal));
		}
	}

	// Index 생성
	UINT ring = (UINT)sliceCount + 1;

	for (UINT i = 0; i < (UINT)stackCount; ++i)
	{
		for (UINT j = 0; j < (UINT)sliceCount; ++j)
		{
			indices.push_back(i * ring + j);
			indices.push_back((i + 1) * ring + j + 1);
			indices.push_back((i + 1) * ring + j);

			indices.push_back(i * ring + j);
			indices.push_back(i * ring + j + 1);
			indices.push_back((i + 1) * ring + j + 1);
		}
	}

	// GPU 버퍼 생성/업로드 
	int nVertices = (int)vertices.size();
	int nStride = sizeof(Vertex);

	VertexBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER,
		nVertices * nStride, 1);

	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER,
		nVertices * nStride, 1);

	gd.UploadToCommitedGPUBuffer(&vertices[0], &VertexUploadBuffer, &VertexBuffer, true);
	VertexBuffer.AddResourceBarrierTransitoinToCommand(pCommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	VertexBufferView.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
	VertexBufferView.StrideInBytes = nStride;
	VertexBufferView.SizeInBytes = nStride * nVertices;

	IndexBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER,
		(int)indices.size() * (int)sizeof(UINT), 1);

	IndexUploadBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER,
		(int)indices.size() * (int)sizeof(UINT), 1);

	gd.UploadToCommitedGPUBuffer(&indices[0], &IndexUploadBuffer, &IndexBuffer, true);
	IndexBuffer.AddResourceBarrierTransitoinToCommand(pCommandList, D3D12_RESOURCE_STATE_INDEX_BUFFER);

	IndexBufferView.BufferLocation = IndexBuffer.resource->GetGPUVirtualAddress();
	IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	IndexBufferView.SizeInBytes = (UINT)indices.size() * sizeof(UINT);

	IndexNum = (ui32)indices.size();
	VertexNum = (ui32)vertices.size();
	topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

#pragma endregion

#pragma region UVMeshCode

void UVMesh::ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering)
{
	std::vector< Vertex > temp_vertices;
	std::vector< TriangleIndex > TrianglePool;
	XMFLOAT3 maxPos = { 0, 0, 0 };
	XMFLOAT3 minPos = { 0, 0, 0 };
	std::ifstream in{ path };

	std::vector< XMFLOAT3 > temp_pos;
	std::vector< XMFLOAT2 > temp_uv;
	std::vector< XMFLOAT3 > temp_normal;
	char rstr[128] = {};
	while (in.eof() == false && in.fail() == false) {
		in >> rstr;
		if (strcmp(rstr, "v") == 0) {
			XMFLOAT3 pos;
			in >> pos.x;
			in >> pos.y;
			in >> pos.z;
			if (maxPos.x < pos.x) maxPos.x = pos.x;
			if (maxPos.y < pos.y) maxPos.y = pos.y;
			if (maxPos.z < pos.z) maxPos.z = pos.z;
			if (minPos.x > pos.x) minPos.x = pos.x;
			if (minPos.y > pos.y) minPos.y = pos.y;
			if (minPos.z > pos.z) minPos.z = pos.z;
			temp_pos.push_back(pos);
		}
		else if (strcmp(rstr, "vt") == 0) {
			XMFLOAT3 uv;
			in >> uv.x;
			in >> uv.y;
			uv.y = 1.0f - uv.y;
			temp_uv.push_back(XMFLOAT2(uv.x, uv.y));
		}
		else if (strcmp(rstr, "vn") == 0) {
			XMFLOAT3 normal;
			in >> normal.x;
			in >> normal.y;
			in >> normal.z;
			temp_normal.push_back(normal);
		}
		else {
			in.ignore(1024, '\n');
		}
	}

	in.close();


	in.open(path);

	if (!in.is_open()) {
		return;
	}

	while (in.eof() == false && in.fail() == false) {
		in >> rstr;
		if (strcmp(rstr, "f") == 0) {
			std::string vertex1, vertex2, vertex3;
			unsigned int vertexIndex[3], uvIndex[3], normalIndex[3];
			char blank;

			in >> vertexIndex[0]; in >> blank;
			in >> uvIndex[0];	  in >> blank;
			in >> normalIndex[0];
			temp_vertices.push_back(Vertex(
				temp_pos[vertexIndex[0] - 1],
				color,
				temp_normal[normalIndex[0] - 1],
				temp_uv[uvIndex[0] - 1]
			));

			in >> vertexIndex[1]; in >> blank;
			in >> uvIndex[1];	  in >> blank;
			in >> normalIndex[1];
			temp_vertices.push_back(Vertex(
				temp_pos[vertexIndex[1] - 1],
				color,
				temp_normal[normalIndex[1] - 1],
				temp_uv[uvIndex[1] - 1]
			));

			in >> vertexIndex[2]; in >> blank;
			in >> uvIndex[2];	  in >> blank;
			in >> normalIndex[2];
			temp_vertices.push_back(Vertex(
				temp_pos[vertexIndex[2] - 1],
				color,
				temp_normal[normalIndex[2] - 1],
				temp_uv[uvIndex[2] - 1]
			));

			int n = temp_vertices.size() - 1;
			TrianglePool.push_back(TriangleIndex(n - 2, n - 1, n));
		}
		else {
			in.ignore(1024, '\n');
		}
	}

	// Pass 2
	in.close();

	XMFLOAT3 CenterPos;
	if (centering) {
		CenterPos.x = (maxPos.x + minPos.x) * 0.5f;
		CenterPos.y = (maxPos.y + minPos.y) * 0.5f;
		CenterPos.z = (maxPos.z + minPos.z) * 0.5f;
	}
	else {
		CenterPos.x = 0;
		CenterPos.y = 0;
		CenterPos.z = 0;
	}

	XMFLOAT3 AABB[2] = {};
	AABB[0] = { 0, 0, 0 };
	AABB[1] = { 0, 0, 0 };

	for (int i = 0; i < temp_vertices.size(); ++i) {
		temp_vertices[i].position.x -= CenterPos.x;
		temp_vertices[i].position.y -= CenterPos.y;
		temp_vertices[i].position.z -= CenterPos.z;
		temp_vertices[i].position.x /= 100.0f;
		temp_vertices[i].position.y /= 100.0f;
		temp_vertices[i].position.z /= 100.0f;

		if (temp_vertices[i].position.x < AABB[0].x) AABB[0].x = temp_vertices[i].position.x;
		if (temp_vertices[i].position.y < AABB[0].y) AABB[0].y = temp_vertices[i].position.y;
		if (temp_vertices[i].position.z < AABB[0].z) AABB[0].z = temp_vertices[i].position.z;

		if (temp_vertices[i].position.x > AABB[1].x) AABB[1].x = temp_vertices[i].position.x;
		if (temp_vertices[i].position.y > AABB[1].y) AABB[1].y = temp_vertices[i].position.y;
		if (temp_vertices[i].position.z > AABB[1].z) AABB[1].z = temp_vertices[i].position.z;
	}

	SetOBBDataWithAABB(AABB[0], AABB[1]);

	int m_nVertices = temp_vertices.size();
	int m_nStride = sizeof(Vertex);
	//m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// error.. why vertex buffer and index buffer do not input? 
	// maybe.. State Error.
	VertexBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, temp_vertices.size() * sizeof(Vertex), 1);
	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, temp_vertices.size() * sizeof(Vertex), 1);
	gd.UploadToCommitedGPUBuffer(&temp_vertices[0], &VertexUploadBuffer, &VertexBuffer, true);
	VertexBuffer.AddResourceBarrierTransitoinToCommand(gd.gpucmd, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	VertexBufferView.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
	VertexBufferView.StrideInBytes = m_nStride;
	VertexBufferView.SizeInBytes = m_nStride * m_nVertices;

	IndexBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(TriangleIndex) * TrianglePool.size(), 1);
	IndexUploadBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(TriangleIndex) * TrianglePool.size(), 1);
	gd.UploadToCommitedGPUBuffer(&TrianglePool[0], &IndexUploadBuffer, &IndexBuffer, true);
	IndexBuffer.AddResourceBarrierTransitoinToCommand(gd.gpucmd, D3D12_RESOURCE_STATE_INDEX_BUFFER);
	IndexBufferView.BufferLocation = IndexBuffer.resource->GetGPUVirtualAddress();
	IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	IndexBufferView.SizeInBytes = sizeof(TriangleIndex) * TrianglePool.size();

	IndexNum = TrianglePool.size() * 3;
	VertexNum = temp_vertices.size();
	topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	/*MeshOBB = BoundingOrientedBox(XMFLOAT3(0.0f, 0.0f, 0.0f), MAXpos, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));*/
}

void UVMesh::Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum)
{
	Mesh::Render(pCommandList, instanceNum);
}

void UVMesh::Release()
{
	Mesh::Release();
}

void UVMesh::CreateWallMesh(float width, float height, float depth, vec4 color)
{
	std::vector<Vertex> vertices;
	std::vector<UINT> indices;
	// Front
	vertices.push_back(Vertex(XMFLOAT3(-width, -height, -depth), color, XMFLOAT3(0.0f, 0.0f, -1.0f), { 0, 0 }));
	vertices.push_back(Vertex(XMFLOAT3(width, -height, -depth), color, XMFLOAT3(0.0f, 0.0f, -1.0f), { 1, 0 }));
	vertices.push_back(Vertex(XMFLOAT3(width, height, -depth), color, XMFLOAT3(0.0f, 0.0f, -1.0f), { 1, 1 }));
	vertices.push_back(Vertex(XMFLOAT3(-width, height, -depth), color, XMFLOAT3(0.0f, 0.0f, -1.0f), { 0, 1 }));
	// Back
	vertices.push_back(Vertex(XMFLOAT3(-width, -height, depth), color, XMFLOAT3(0.0f, 0.0f, 1.0f), { 0, 0 }));
	vertices.push_back(Vertex(XMFLOAT3(width, -height, depth), color, XMFLOAT3(0.0f, 0.0f, 1.0f), { 1, 0 }));
	vertices.push_back(Vertex(XMFLOAT3(width, height, depth), color, XMFLOAT3(0.0f, 0.0f, 1.0f), { 1, 1 }));
	vertices.push_back(Vertex(XMFLOAT3(-width, height, depth), color, XMFLOAT3(0.0f, 0.0f, 1.0f), { 0, 1 }));
	// Top
	vertices.push_back(Vertex(XMFLOAT3(-width, height, -depth), color, XMFLOAT3(0.0f, 1.0f, 0.0f), { 0, 0 }));
	vertices.push_back(Vertex(XMFLOAT3(width, height, -depth), color, XMFLOAT3(0.0f, 1.0f, 0.0f), { 1, 0 }));
	vertices.push_back(Vertex(XMFLOAT3(width, height, depth), color, XMFLOAT3(0.0f, 1.0f, 0.0f), { 1, 1 }));
	vertices.push_back(Vertex(XMFLOAT3(-width, height, depth), color, XMFLOAT3(0.0f, 1.0f, 0.0f), { 0, 1 }));
	// Bottom
	vertices.push_back(Vertex(XMFLOAT3(-width, -height, -depth), color, XMFLOAT3(0.0f, -1.0f, 0.0f), { 0, 0 }));
	vertices.push_back(Vertex(XMFLOAT3(width, -height, -depth), color, XMFLOAT3(0.0f, -1.0f, 0.0f), { 1, 0 }));
	vertices.push_back(Vertex(XMFLOAT3(width, -height, depth), color, XMFLOAT3(0.0f, -1.0f, 0.0f), { 1, 1 }));
	vertices.push_back(Vertex(XMFLOAT3(-width, -height, depth), color, XMFLOAT3(0.0f, -1.0f, 0.0f), { 0, 1 }));
	// Left
	vertices.push_back(Vertex(XMFLOAT3(-width, -height, depth), color, XMFLOAT3(-1.0f, 0.0f, 0.0f), { 0, 0 }));
	vertices.push_back(Vertex(XMFLOAT3(-width, -height, -depth), color, XMFLOAT3(-1.0f, 0.0f, 0.0f), { 1, 0 }));
	vertices.push_back(Vertex(XMFLOAT3(-width, height, -depth), color, XMFLOAT3(-1.0f, 0.0f, 0.0f), { 1, 1 }));
	vertices.push_back(Vertex(XMFLOAT3(-width, height, depth), color, XMFLOAT3(-1.0f, 0.0f, 0.0f), { 0, 1 }));
	// Right
	vertices.push_back(Vertex(XMFLOAT3(width, -height, -depth), color, XMFLOAT3(1.0f, 0.0f, 0.0f), { 0, 0 }));
	vertices.push_back(Vertex(XMFLOAT3(width, -height, depth), color, XMFLOAT3(1.0f, 0.0f, 0.0f), { 1, 0 }));
	vertices.push_back(Vertex(XMFLOAT3(width, height, depth), color, XMFLOAT3(1.0f, 0.0f, 0.0f), { 1, 1 }));
	vertices.push_back(Vertex(XMFLOAT3(width, height, -depth), color, XMFLOAT3(1.0f, 0.0f, 0.0f), { 0, 1 }));

	// index
	// front
	indices.push_back(0); indices.push_back(2); indices.push_back(1);
	indices.push_back(2); indices.push_back(0); indices.push_back(3);
	// back
	indices.push_back(4); indices.push_back(5); indices.push_back(6);
	indices.push_back(6); indices.push_back(7); indices.push_back(4);
	// top
	indices.push_back(8); indices.push_back(10); indices.push_back(9);
	indices.push_back(10); indices.push_back(8); indices.push_back(11);
	// bottom
	indices.push_back(12); indices.push_back(13); indices.push_back(14);
	indices.push_back(14); indices.push_back(15); indices.push_back(12);
	// left
	indices.push_back(16); indices.push_back(18); indices.push_back(17);
	indices.push_back(18); indices.push_back(16); indices.push_back(19);
	// right
	indices.push_back(20); indices.push_back(22); indices.push_back(21);
	indices.push_back(22); indices.push_back(20); indices.push_back(23);

	// OBB
	this->OBB_Tr = XMFLOAT3(0, 0, 0);
	this->OBB_Ext = XMFLOAT3(width, height, depth);

	int nVertices = vertices.size();
	int nStride = sizeof(Vertex);

	VertexBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	gd.UploadToCommitedGPUBuffer(&vertices[0], &VertexUploadBuffer, &VertexBuffer, true);

	VertexBuffer.AddResourceBarrierTransitoinToCommand(gd.gpucmd, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	VertexBufferView.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
	VertexBufferView.StrideInBytes = nStride;
	VertexBufferView.SizeInBytes = nStride * nVertices;

	IndexBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, indices.size() * sizeof(UINT), 1);
	IndexUploadBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, indices.size() * sizeof(UINT), 1);
	gd.UploadToCommitedGPUBuffer(&indices[0], &IndexUploadBuffer, &IndexBuffer, true);

	IndexBuffer.AddResourceBarrierTransitoinToCommand(gd.gpucmd, D3D12_RESOURCE_STATE_INDEX_BUFFER);

	IndexBufferView.BufferLocation = IndexBuffer.resource->GetGPUVirtualAddress();
	IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	IndexBufferView.SizeInBytes = indices.size() * sizeof(UINT);

	IndexNum = indices.size();
	VertexNum = vertices.size();
	topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

void UVMesh::CreateTextRectMesh()
{
	std::vector<Vertex> vertices;
	std::vector<UINT> indices;

	// Front
	vec4 color = { 1, 1, 1, 1 };
	vertices.push_back(Vertex(XMFLOAT3(0, 0, 0), color, XMFLOAT3(0, 0, 0), XMFLOAT2(0.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(1, 0, 0), color, XMFLOAT3(0, 0, 0), XMFLOAT2(1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(1, 1, 0), color, XMFLOAT3(0, 0, 0), XMFLOAT2(1.0f, 1.0f)));
	vertices.push_back(Vertex(XMFLOAT3(0, 1, 0), color, XMFLOAT3(0, 0, 0), XMFLOAT2(0.0f, 1.0f)));

	// front
	indices.push_back(0); indices.push_back(2); indices.push_back(1);
	indices.push_back(2); indices.push_back(0); indices.push_back(3);

	// OBB
	OBB_Tr = { 0, 0, 0 };
	OBB_Ext = XMFLOAT3(1, 1, 0);

	int nVertices = vertices.size();
	int nStride = sizeof(Vertex);

	VertexBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	gd.UploadToCommitedGPUBuffer(&vertices[0], &VertexUploadBuffer, &VertexBuffer, true);

	VertexBuffer.AddResourceBarrierTransitoinToCommand(gd.gpucmd, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	VertexBufferView.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
	VertexBufferView.StrideInBytes = nStride;
	VertexBufferView.SizeInBytes = nStride * nVertices;

	IndexBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, indices.size() * sizeof(UINT), 1);
	IndexUploadBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, indices.size() * sizeof(UINT), 1);
	gd.UploadToCommitedGPUBuffer(&indices[0], &IndexUploadBuffer, &IndexBuffer, true);

	IndexBuffer.AddResourceBarrierTransitoinToCommand(gd.gpucmd, D3D12_RESOURCE_STATE_INDEX_BUFFER);

	IndexBufferView.BufferLocation = IndexBuffer.resource->GetGPUVirtualAddress();
	IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	IndexBufferView.SizeInBytes = indices.size() * sizeof(UINT);

	IndexNum = indices.size();
	VertexNum = vertices.size();
	topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	subMeshNum = 1;
	SubMeshIndexStart = new int[2];
	SubMeshIndexStart[0] = 0;
	SubMeshIndexStart[1] = IndexNum;
}

#pragma endregion

#pragma region BumpMeshCode

BumpMesh::~BumpMesh()
{
}

void BumpMesh::CreateWallMesh(float width, float height, float depth, vec4 color)
{
	std::vector<Vertex> vertices;
	std::vector<UINT> indices;
	// Front
	vertices.push_back(Vertex(XMFLOAT3(-width, -height, -depth), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0, 0), XMFLOAT3(1, 0, 0)));
	vertices.push_back(Vertex(XMFLOAT3(width, -height, -depth), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1, 0), XMFLOAT3(1, 0, 0)));
	vertices.push_back(Vertex(XMFLOAT3(width, height, -depth), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1, 1), XMFLOAT3(1, 0, 0)));
	vertices.push_back(Vertex(XMFLOAT3(-width, height, -depth), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0, 1), XMFLOAT3(1, 0, 0)));
	// Back
	vertices.push_back(Vertex(XMFLOAT3(-width, -height, depth), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0, 0), XMFLOAT3(1, 0, 0)));
	vertices.push_back(Vertex(XMFLOAT3(width, -height, depth), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(1, 0), XMFLOAT3(1, 0, 0)));
	vertices.push_back(Vertex(XMFLOAT3(width, height, depth), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(1, 1), XMFLOAT3(1, 0, 0)));
	vertices.push_back(Vertex(XMFLOAT3(-width, height, depth), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0, 1), XMFLOAT3(1, 0, 0)));
	// Top
	vertices.push_back(Vertex(XMFLOAT3(-width, height, -depth), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0, 0), XMFLOAT3(1, 0, 0)));
	vertices.push_back(Vertex(XMFLOAT3(width, height, -depth), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1, 0), XMFLOAT3(1, 0, 0)));
	vertices.push_back(Vertex(XMFLOAT3(width, height, depth), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1, 1), XMFLOAT3(1, 0, 0)));
	vertices.push_back(Vertex(XMFLOAT3(-width, height, depth), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0, 1), XMFLOAT3(1, 0, 0)));
	// Bottom
	vertices.push_back(Vertex(XMFLOAT3(-width, -height, -depth), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(0, 0), XMFLOAT3(1, 0, 0)));
	vertices.push_back(Vertex(XMFLOAT3(width, -height, -depth), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(1, 0), XMFLOAT3(1, 0, 0)));
	vertices.push_back(Vertex(XMFLOAT3(width, -height, depth), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(1, 1), XMFLOAT3(1, 0, 0)));
	vertices.push_back(Vertex(XMFLOAT3(-width, -height, depth), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(0, 1), XMFLOAT3(1, 0, 0)));
	// Left
	vertices.push_back(Vertex(XMFLOAT3(-width, -height, depth), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(0, 0), XMFLOAT3(0, 0, 1)));
	vertices.push_back(Vertex(XMFLOAT3(-width, -height, -depth), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(1, 0), XMFLOAT3(0, 0, 1)));
	vertices.push_back(Vertex(XMFLOAT3(-width, height, -depth), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(1, 1), XMFLOAT3(0, 0, 1)));
	vertices.push_back(Vertex(XMFLOAT3(-width, height, depth), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(0, 1), XMFLOAT3(0, 0, 1)));
	// Right
	vertices.push_back(Vertex(XMFLOAT3(width, -height, -depth), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(0, 0), XMFLOAT3(0, 0, 1)));
	vertices.push_back(Vertex(XMFLOAT3(width, -height, depth), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(1, 0), XMFLOAT3(0, 0, 1)));
	vertices.push_back(Vertex(XMFLOAT3(width, height, depth), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(1, 1), XMFLOAT3(0, 0, 1)));
	vertices.push_back(Vertex(XMFLOAT3(width, height, -depth), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(0, 1), XMFLOAT3(0, 0, 1)));

	// index
	// front
	indices.push_back(0); indices.push_back(2); indices.push_back(1);
	indices.push_back(2); indices.push_back(0); indices.push_back(3);
	// back
	indices.push_back(4); indices.push_back(5); indices.push_back(6);
	indices.push_back(6); indices.push_back(7); indices.push_back(4);
	// top
	indices.push_back(8); indices.push_back(10); indices.push_back(9);
	indices.push_back(10); indices.push_back(8); indices.push_back(11);
	// bottom
	indices.push_back(12); indices.push_back(13); indices.push_back(14);
	indices.push_back(14); indices.push_back(15); indices.push_back(12);
	// left
	indices.push_back(16); indices.push_back(18); indices.push_back(17);
	indices.push_back(18); indices.push_back(16); indices.push_back(19);
	// right
	indices.push_back(20); indices.push_back(22); indices.push_back(21);
	indices.push_back(22); indices.push_back(20); indices.push_back(23);

	// OBB
	OBB_Tr = { 0, 0, 0 };
	OBB_Ext = XMFLOAT3(width, height, depth);

	int nVertices = vertices.size();
	int nStride = sizeof(Vertex);

	VertexBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	gd.UploadToCommitedGPUBuffer(&vertices[0], &VertexUploadBuffer, &VertexBuffer, true);

	VertexBuffer.AddResourceBarrierTransitoinToCommand(gd.gpucmd, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	VertexBufferView.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
	VertexBufferView.StrideInBytes = nStride;
	VertexBufferView.SizeInBytes = nStride * nVertices;

	IndexBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, indices.size() * sizeof(UINT), 1);
	IndexUploadBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, indices.size() * sizeof(UINT), 1);
	gd.UploadToCommitedGPUBuffer(&indices[0], &IndexUploadBuffer, &IndexBuffer, true);

	IndexBuffer.AddResourceBarrierTransitoinToCommand(gd.gpucmd, D3D12_RESOURCE_STATE_INDEX_BUFFER);

	IndexBufferView.BufferLocation = IndexBuffer.resource->GetGPUVirtualAddress();
	IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	IndexBufferView.SizeInBytes = indices.size() * sizeof(UINT);

	IndexNum = indices.size();
	VertexNum = vertices.size();
	topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

void BumpMesh::CreateMesh_FromVertexAndIndexData(vector<Vertex>& vert, vector<TriangleIndex>& inds, int SubMeshNum, int* SubMeshIndexArr, bool include_DXR)
{
	if (SubMeshIndexArr == nullptr) {
		int* _SubMeshIndexArr = new int[2];
		_SubMeshIndexArr[0] = 0;
		_SubMeshIndexArr[1] = inds.size() * 3;
		SubMeshIndexStart = _SubMeshIndexArr;
		subMeshNum = 1;
	}
	else {
		subMeshNum = SubMeshNum;
		SubMeshIndexStart = SubMeshIndexArr;
	}

	if (gd.isSupportRaytracing) {
		rmesh.AllocateRaytracingMesh(vert, inds, SubMeshNum, SubMeshIndexStart);

		VertexBufferView.BufferLocation = RayTracingMesh::vertexBuffer->GetGPUVirtualAddress() + rmesh.VBStartOffset;
		VertexBufferView.StrideInBytes = sizeof(RayTracingMesh::Vertex);
		VertexBufferView.SizeInBytes = sizeof(RayTracingMesh::Vertex) * vert.size();

		//// update raster submesh index range
		//for (int i = 0; i < subMeshNum + 1; ++i) {
		//	SubMeshIndexStart[i] = (rmesh.IBStartOffset[i] - rmesh.IBStartOffset[0]) / sizeof(UINT);
		//}

		IndexBufferView.BufferLocation = RayTracingMesh::indexBuffer->GetGPUVirtualAddress() + rmesh.IBStartOffset[0];
		IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
		IndexBufferView.SizeInBytes = rmesh.IBStartOffset[1/*subMeshNum*/] - rmesh.IBStartOffset[0];
		IndexNum = (rmesh.IBStartOffset[1/*subMeshNum*/] - rmesh.IBStartOffset[0]) / sizeof(UINT);
		VertexNum = vert.size();
		topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}
	else {
		int m_nVertices = vert.size();
		int m_nStride = sizeof(Vertex);

		gd.CreateDefaultHeap_VB(&vert[0], VertexBuffer, VertexUploadBuffer, VertexBufferView, m_nVertices, m_nStride);
		gd.CreateDefaultHeap_IB<sizeof(UINT)>(inds.data(), IndexBuffer, IndexUploadBuffer, IndexBufferView, inds.size() * 3);

		IndexNum = inds.size() * 3;
		VertexNum = vert.size();
		topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}
	InstancingInit();
	game.AddMesh(this);
}

void BumpMesh::ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering, bool include_DXR)
{
	std::vector< Vertex > temp_vertices;
	std::vector< TriangleIndex > TrianglePool;
	XMFLOAT3 maxPos = { 0, 0, 0 };
	XMFLOAT3 minPos = { 0, 0, 0 };
	std::ifstream in{ path };

	std::vector< XMFLOAT3 > temp_pos;
	std::vector< XMFLOAT2 > temp_uv;
	std::vector< XMFLOAT3 > temp_normal;
	while (in.eof() == false && (in.is_open() && in.fail() == false)) {
		char rstr[128] = {};
		in >> rstr;
		if (strcmp(rstr, "v") == 0) {
			//좌표
			XMFLOAT3 pos;
			in >> pos.x;
			in >> pos.y;
			in >> pos.z;
			if (maxPos.x < pos.x) maxPos.x = pos.x;
			if (maxPos.y < pos.y) maxPos.y = pos.y;
			if (maxPos.z < pos.z) maxPos.z = pos.z;
			if (minPos.x > pos.x) minPos.x = pos.x;
			if (minPos.y > pos.y) minPos.y = pos.y;
			if (minPos.z > pos.z) minPos.z = pos.z;
			temp_pos.push_back(pos);
		}
		else if (strcmp(rstr, "vt") == 0) {
			// uv 좌표
			XMFLOAT3 uv;
			in >> uv.x;
			in >> uv.y;
			in >> uv.z;
			uv.y = 1.0f - uv.y;
			temp_uv.push_back(XMFLOAT2(uv.x, uv.y));
		}
		else if (strcmp(rstr, "vn") == 0) {
			// 노멀
			XMFLOAT3 normal;
			in >> normal.x;
			in >> normal.y;
			in >> normal.z;
			temp_normal.push_back(normal);
		}
		else if (strcmp(rstr, "f") == 0) {
			std::string vertex1, vertex2, vertex3;
			unsigned int vertexIndex[3], uvIndex[3], normalIndex[3];
			char blank;
			XMFLOAT3 v3 = { 0, 0, 0 };

			in >> vertexIndex[0];
			in >> blank;
			in >> uvIndex[0];
			in >> blank;
			in >> normalIndex[0];
			temp_vertices.push_back(Vertex(temp_pos[vertexIndex[0] - 1], temp_normal[normalIndex[0] - 1], temp_uv[uvIndex[0] - 1], v3));

			//in >> blank;
			in >> vertexIndex[1];
			in >> blank;
			in >> uvIndex[1];
			in >> blank;
			in >> normalIndex[1];
			temp_vertices.push_back(Vertex(temp_pos[vertexIndex[1] - 1], temp_normal[normalIndex[1] - 1], temp_uv[uvIndex[1] - 1], v3));

			//in >> blank;
			in >> vertexIndex[2];
			in >> blank;
			in >> uvIndex[2];
			in >> blank;
			in >> normalIndex[2];
			//in >> blank;
			temp_vertices.push_back(Vertex(temp_pos[vertexIndex[2] - 1], temp_normal[normalIndex[2] - 1], temp_uv[uvIndex[2] - 1], v3));

			int n = temp_vertices.size() - 1;
			TrianglePool.push_back(TriangleIndex(n - 2, n - 1, n));
		}
	}
	// For each vertex of each triangle

	in.close();

	XMFLOAT3 CenterPos;
	if (centering) {
		CenterPos.x = (maxPos.x + minPos.x) * 0.5f;
		CenterPos.y = (maxPos.y + minPos.y) * 0.5f;
		CenterPos.z = (maxPos.z + minPos.z) * 0.5f;
	}
	else {
		CenterPos.x = 0;
		CenterPos.y = 0;
		CenterPos.z = 0;
	}

	XMFLOAT3 AABB[2] = {};
	AABB[0] = { 0, 0, 0 };
	AABB[1] = { 0, 0, 0 };
	for (int i = 0; i < temp_vertices.size(); ++i) {
		temp_vertices[i].position.x -= CenterPos.x;
		temp_vertices[i].position.y -= CenterPos.y;
		temp_vertices[i].position.z -= CenterPos.z;
		temp_vertices[i].position.x /= 100.0f;
		temp_vertices[i].position.y /= 100.0f;
		temp_vertices[i].position.z /= 100.0f;

		if (temp_vertices[i].position.x < AABB[0].x) AABB[0].x = temp_vertices[i].position.x;
		if (temp_vertices[i].position.y < AABB[0].y) AABB[0].y = temp_vertices[i].position.y;
		if (temp_vertices[i].position.z < AABB[0].z) AABB[0].z = temp_vertices[i].position.z;

		if (temp_vertices[i].position.x > AABB[1].x) AABB[1].x = temp_vertices[i].position.x;
		if (temp_vertices[i].position.y > AABB[1].y) AABB[1].y = temp_vertices[i].position.y;
		if (temp_vertices[i].position.z > AABB[1].z) AABB[1].z = temp_vertices[i].position.z;
	}

	SetOBBDataWithAABB(AABB[0], AABB[1]);

	CreateMesh_FromVertexAndIndexData(temp_vertices, TrianglePool, 1, nullptr, gd.isSupportRaytracing);

	InstancingInit();
	game.AddMesh(this);

	/*MeshOBB = BoundingOrientedBox(XMFLOAT3(0.0f, 0.0f, 0.0f), MAXpos, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));*/
}

void BumpMesh::Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum, ui32 slotIndex)
{
	Mesh::Render(pCommandList, instanceNum, slotIndex);
}

void BumpMesh::MakeMeshFromWChar(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const wchar_t wchar, float fontsiz)
{
	vec4 averagePos;
	static constexpr float fontdiv = 500.0f;
	float fs = fontsiz / fontdiv;
	std::vector< std::vector<XMFLOAT3>> polys;
	std::vector<std::pair<int, int>> InvisibleRange;

	std::vector< Vertex > temp_vertices;
	std::vector< TriangleIndex > TrianglePool;
	int indexoffset = 0;

	for (int fontI = 0; fontI < gd.FontCount; ++fontI) {
		auto glyphmap = &gd.font_data[fontI].glyphs;

		if (glyphmap->find(wchar) != glyphmap->end()) {
			Glyph& g = (*glyphmap)[wchar];
			int polygonsiz = 0;
			for (int i = 0; i < g.path_list.size(); ++i) {
				polygonsiz += g.path_list.at(i).geometry.size();
			}

			subMeshNum = g.path_list.size();
			SubMeshIndexStart = new int[subMeshNum + 1];
			SubMeshIndexStart[0] = 0;
			for (int i = 0; i < g.path_list.size(); ++i) {
				std::vector<XMFLOAT3> poly;
				std::vector<TriangleIndex> polyindexes;
				constexpr int curve_vertex_count = 2;
				int s = 0;
				for (int k = 0; k < g.path_list.at(i).geometry.size(); ++k) {
					Curve& curve = g.path_list.at(i).geometry.at(k);
					if (curve.is_curve) {
						s += curve_vertex_count;
					}
					else s += 1;
				}
				poly.reserve(s);
				poly.resize(s);
				int polyindex = 0;
				for (int k = 0; k < g.path_list.at(i).geometry.size(); ++k) {
					Curve& curve = g.path_list.at(i).geometry.at(k);
					if (curve.is_curve == false) {
						averagePos.x += curve.p0.x * fs;
						averagePos.y += 0;
						averagePos.z += curve.p0.y * fs;
						poly[polyindex] = { curve.p0.x * fs, curve.p0.y * fs, 0 };
						polyindex += 1;
					}
					else {
						constexpr float dd = 1.0f / (float)curve_vertex_count;
						for (int d = 0; d < curve_vertex_count; d++) {
							float t = (float)d * dd;
							XMVECTOR pos0, pos1, cpos, rpos0, rpos1, rpos;
							pos0 = XMVectorSet(curve.p0.x, 0, curve.p0.y, 1);
							pos1 = XMVectorSet(curve.p1.x, 0, curve.p1.y, 1);
							cpos = XMVectorSet(curve.c.x, 0, curve.c.y, 1);
							rpos0 = XMVectorLerp(pos0, pos1, t);
							rpos1 = XMVectorLerp(pos1, cpos, t);
							rpos = XMVectorLerp(rpos0, rpos1, t);

							averagePos.x += rpos.m128_f32[0] * fs;
							averagePos.y += 0;
							averagePos.z += rpos.m128_f32[2] * fs;
							poly[polyindex] = { rpos.m128_f32[0] * fs, rpos.m128_f32[2] * fs, 0 };
							polyindex += 1;
						}
					}
				}

				// 만약 현재 poly가 과거의 poly들의 내부에 있을 경우.
				// 지우개처리.
				bool isEraserGeometry = false;
				for (int k = polys.size() - i; k < polys.size(); ++k) {
					int n = 0;
					for (int u = 0; u < poly.size(); ++u) {
						if (bPointInPolygonRange(XMVectorSet(poly[u].x, poly[u].y, 0, 0), polys[k])) {
							n += 1;
						}
					}

					if (n > poly.size() / 2) {
						isEraserGeometry = true;
					}

					if (isEraserGeometry) break;
				}

				if (isEraserGeometry) {
					InvisibleRange.push_back(std::pair<int, int>(temp_vertices.size(), temp_vertices.size() + poly.size()));
				}

				polys.push_back(poly);

				// 인덱스를 역순으로 삽입
				std::list<unsigned int> flist;
				int flistsize = 0;
				for (int i = 0; i < poly.size(); ++i) {
					flist.push_front(i); flistsize += 1;
				}

				int savesiz = 0;
				while (flistsize >= 3) {
					if (savesiz == flistsize) break;
					//ltlast->next = nullptr;
					savesiz = flistsize;
					std::list<unsigned int>::iterator lti = flist.begin();
					for (int index = 0; index < flistsize - 2; ++index) {
						//ltlast->next = nullptr;
						std::list<unsigned int>::iterator inslti0 = lti;
						std::list<unsigned int>::iterator inslti1 = ++lti;
						--lti;
						std::list<unsigned int>::iterator inslti2 = ++inslti1;
						--inslti1;
						if (lti == flist.end()) continue;
						bool bdraw = true; ///
						std::list<unsigned int>::iterator ltk = flist.begin();
						for (int kndex = 0; kndex < flistsize; ++kndex) {
							//ltlast->next = nullptr;
							bool b = false;
							unsigned int kv = *ltk;
							b = b || *inslti0 == kv;
							b = b || *inslti1 == kv;
							b = b || *inslti2 == kv;
							if (b) {
								ltk = ++ltk;
								continue;
							}

							bdraw = bdraw && !bPointInTriangleRange(poly.at(kv).x, poly.at(kv).y,
								poly.at(*inslti0).x, poly.at(*inslti0).y,
								poly.at(*inslti1).x, poly.at(*inslti1).y,
								poly.at(*inslti2).x, poly.at(*inslti2).y);

							ltk = ++ltk;
						}

						if (bdraw == true) {
							//fmlist_node<uint>* inslti = lti;
							unsigned int pi = *inslti0;
							unsigned int pi1 = *inslti1;
							unsigned int pi2 = *inslti2;

							if (bTriangleInPolygonRange(poly.at(pi).x, poly.at(pi).y,
								poly.at(pi1).x, poly.at(pi1).y,
								poly.at(pi2).x, poly.at(pi2).y, poly) || flistsize <= 3)
							{
								polyindexes.push_back(TriangleIndex(pi, pi1, pi2));
								//index_buf[nextchoice]->push_back(aindex(pi, pi1, pi2));
								flist.erase(inslti1); flistsize -= 1;
								//lti = inslti2;
								//여기에 도달하기 전에 lt의 first의 nest가 nullptr에서 쓰레기 값으로 덮어진다. 원인을 찾자
							}
						}

						lti = ++lti;
					}
				}

				for (int i = 0; i < polyindexes.size(); ++i) {
					XMVECTOR p0 = XMVectorSet(poly[polyindexes[i].v[0]].x, poly[polyindexes[i].v[0]].y, poly[polyindexes[i].v[0]].z, 0);
					XMVECTOR p1 = XMVectorSet(poly[polyindexes[i].v[1]].x, poly[polyindexes[i].v[1]].y, poly[polyindexes[i].v[1]].z, 0);
					XMVECTOR p2 = XMVectorSet(poly[polyindexes[i].v[2]].x, poly[polyindexes[i].v[2]].y, poly[polyindexes[i].v[2]].z, 0);

					XMVECTOR v0 = p1 - p0;
					XMVECTOR v1 = p2 - p1;
					v0 = XMVector3Cross(v0, v1);
					if (v0.m128_f32[2] < 0) {
						TrianglePool.push_back(TriangleIndex(polyindexes[i].v[0] + indexoffset,
							polyindexes[i].v[1] + indexoffset, polyindexes[i].v[2] + indexoffset));
					}
					else {
						TrianglePool.push_back(TriangleIndex(polyindexes[i].v[0] + indexoffset,
							polyindexes[i].v[2] + indexoffset, polyindexes[i].v[1] + indexoffset));
					}
				}

				SubMeshIndexStart[i + 1] = indexoffset;
				indexoffset += poly.size();

				for (int i = 0; i < poly.size(); ++i) {
					Vertex v;
					v.position = poly[i];
					v.normal = { 0, 0, 1 };
					v.tangent = { 0, -1, 0 };
					v.u = poly[i].x;
					v.v = poly[i].y;
					temp_vertices.push_back(v);
				}
			}
			//offsetX += (g.bounding_box[2] - g.bounding_box[0]) * fs * 1.2f;
		}
	}


	CreateMesh_FromVertexAndIndexData(temp_vertices, TrianglePool);
}

void BumpMesh::Release()
{
	rmesh.Release();
	Mesh::Release();
}

void BumpMesh::BatchRender(ID3D12GraphicsCommandList* pCommandList) {
	pCommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
	pCommandList->IASetPrimitiveTopology(topology);
	if (IndexNum > 0) {
		pCommandList->IASetIndexBuffer(&IndexBufferView);
	}
	int singleinstanceNum = 1;
	if (InstanceData[0].InstanceSize < MinInstancingStartSize) {
		// Root Constant Batch
		for (int k = 0; k < InstanceData[0].InstanceSize; ++k) {
			pCommandList->SetGraphicsRoot32BitConstants(1, 16, &InstanceData[0].InstanceDataArr[k].worldMat, 0);
			for (int i = 0; i < subMeshNum; ++i) {
				//material
				int materialIndex = InstanceData[i].InstanceDataArr[k].MaterialIndex;
				Material* mat = game.MaterialTable[materialIndex];
				using PBRRPI = PBRShader1::RootParamId;
				gd.gpucmd->SetGraphicsRootDescriptorTable(PBRRPI::CBVTable_Material, mat->CB_Resource.descindex.hRender.hgpu);
				gd.gpucmd->SetGraphicsRootDescriptorTable(PBRRPI::SRVTable_MaterialTextures, mat->TextureSRVTableIndex.hRender.hgpu);

				//draw
				pCommandList->DrawIndexedInstanced(SubMeshIndexStart[i + 1] - SubMeshIndexStart[i], singleinstanceNum, SubMeshIndexStart[i], 0, 0);
			}
		}
	}
	else {
		//StructuredBuffer Batch
		for (int i = 0; i < subMeshNum; ++i) {
			if (InstanceData[i].InstanceSize > 0) {
				using PBRRPI = PBRShader1::RootParamId;
				pCommandList->SetGraphicsRootDescriptorTable(PBRRPI::SRVTable_Instancing_RenderInstance, InstanceData[i].InstancingSRVIndex.hRender.hgpu);
				pCommandList->DrawIndexedInstanced(SubMeshIndexStart[i + 1] - SubMeshIndexStart[i], InstanceData[i].InstanceSize, SubMeshIndexStart[i], 0, 0);
			}
		}
	}
}

#pragma endregion

#pragma region BumpSkinMeshCode

BumpSkinMesh::~BumpSkinMesh()
{
}

void BumpSkinMesh::CreateMesh_FromVertexAndIndexData(vector<Vertex>& vert, vector<BoneData>& bonedata, vector<TriangleIndex>& inds, int SubMeshNum, int* SubMeshIndexArr, matrix* NodeLocalMatrixs, int matrixCount)
{
	if (SubMeshIndexArr == nullptr) {
		int* _SubMeshIndexArr = new int[2];
		_SubMeshIndexArr[0] = 0;
		_SubMeshIndexArr[1] = inds.size() * 3;
		SubMeshIndexStart = _SubMeshIndexArr;
		subMeshNum = 1;
	}
	else {
		subMeshNum = SubMeshNum;
		SubMeshIndexStart = SubMeshIndexArr;
	}

	if (gd.isSupportRaytracing) {
		vector<Vertex> dumy;
		dumy.reserve(0);
		dumy.resize(0);
		// 버택스 부분은 오브젝트 SetShape할때 해야 함. (인스턴스마다 따로 있어야 하니까.)
		rmesh.AllocateRaytracingUAVMesh_OnlyIndex(inds, SubMeshNum, SubMeshIndexStart);

		// Origin SRV VertexBuffer (non transform)
		int m_nVertices = vert.size();
		int m_nStride = sizeof(Vertex);
		VertexBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, vert.size() * sizeof(Vertex), 1);
		VertexUploadBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, vert.size() * sizeof(Vertex), 1);
		gd.UploadToCommitedGPUBuffer(&vert[0], &VertexUploadBuffer, &VertexBuffer, true);
		VertexBuffer.AddResourceBarrierTransitoinToCommand(gd.gpucmd, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		VertexBufferView.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
		VertexBufferView.StrideInBytes = m_nStride;
		VertexBufferView.SizeInBytes = m_nStride * m_nVertices;
		RenderVBufferView[0] = VertexBufferView; // 레스터를 위한 조치

		// update raster submesh index range
		//for (int i = 0; i < subMeshNum + 1; ++i) {
		//	SubMeshIndexStart[i] = (rmesh.IBStartOffset[i] - rmesh.IBStartOffset[0]) / sizeof(UINT);
		//}

		IndexBufferView.BufferLocation = RayTracingMesh::indexBuffer->GetGPUVirtualAddress() + rmesh.IBStartOffset[0];
		IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
		IndexBufferView.SizeInBytes = rmesh.IBStartOffset[1/*subMeshNum*/] - rmesh.IBStartOffset[0];
		IndexNum = (rmesh.IBStartOffset[1/*subMeshNum*/] - rmesh.IBStartOffset[0]) / sizeof(UINT);
		VertexNum = vert.size();
		topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		BoneWeightBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, vert.size() * sizeof(BoneData), 1);
		BoneWeightUploadBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, vert.size() * sizeof(BoneData), 1);
		gd.UploadToCommitedGPUBuffer(&bonedata[0], &BoneWeightUploadBuffer, &BoneWeightBuffer, true);
		BoneWeightBuffer.AddResourceBarrierTransitoinToCommand(gd.gpucmd, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		RenderVBufferView[1].BufferLocation = BoneWeightBuffer.resource->GetGPUVirtualAddress();
		RenderVBufferView[1].StrideInBytes = sizeof(BoneData);
		RenderVBufferView[1].SizeInBytes = sizeof(BoneData) * VertexNum;

		int index = gd.TextureDescriptorAllotter.Alloc();
		VertexSRV = DescIndex(false, index);
		D3D12_SHADER_RESOURCE_VIEW_DESC Vertex_SRVdesc = {};
		Vertex_SRVdesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		Vertex_SRVdesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		Vertex_SRVdesc.Buffer.NumElements = vert.size();
		Vertex_SRVdesc.Format = DXGI_FORMAT_UNKNOWN;
		Vertex_SRVdesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		Vertex_SRVdesc.Buffer.StructureByteStride = sizeof(Vertex);
		gd.pDevice->CreateShaderResourceView(VertexBuffer.resource, &Vertex_SRVdesc, VertexSRV.hCreation.hcpu);

		index = gd.TextureDescriptorAllotter.Alloc();
		BoneSRV = DescIndex(false, index);
		D3D12_SHADER_RESOURCE_VIEW_DESC Bone_SRVdesc = {};
		Bone_SRVdesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		Bone_SRVdesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		Bone_SRVdesc.Buffer.NumElements = vert.size();
		Bone_SRVdesc.Format = DXGI_FORMAT_UNKNOWN;
		Bone_SRVdesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		Bone_SRVdesc.Buffer.StructureByteStride = sizeof(BoneData);
		gd.pDevice->CreateShaderResourceView(BoneWeightBuffer.resource, &Bone_SRVdesc, BoneSRV.hCreation.hcpu);

		vertexData.reserve(vert.size());
		vertexData.resize(vert.size());
		std::copy(vert.begin(), vert.end(), vertexData.begin());
	}
	else {
		int m_nVertices = vert.size();
		int m_nStride = sizeof(Vertex);

		VertexBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, vert.size() * sizeof(Vertex), 1);
		VertexUploadBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, vert.size() * sizeof(Vertex), 1);
		gd.UploadToCommitedGPUBuffer(&vert[0], &VertexUploadBuffer, &VertexBuffer, true);
		VertexBuffer.AddResourceBarrierTransitoinToCommand(gd.gpucmd, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		VertexBufferView.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
		VertexBufferView.StrideInBytes = m_nStride;
		VertexBufferView.SizeInBytes = m_nStride * m_nVertices;
		RenderVBufferView[0] = VertexBufferView;

		BoneWeightBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, vert.size() * sizeof(BoneData), 1);
		BoneWeightUploadBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, vert.size() * sizeof(BoneData), 1);
		gd.UploadToCommitedGPUBuffer(&bonedata[0], &BoneWeightUploadBuffer, &BoneWeightBuffer, true);
		BoneWeightBuffer.AddResourceBarrierTransitoinToCommand(gd.gpucmd, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		RenderVBufferView[1].BufferLocation = BoneWeightBuffer.resource->GetGPUVirtualAddress();
		RenderVBufferView[1].StrideInBytes = sizeof(BoneData);
		RenderVBufferView[1].SizeInBytes = sizeof(BoneData) * m_nVertices;

		IndexBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(TriangleIndex) * inds.size(), 1);
		IndexUploadBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(TriangleIndex) * inds.size(), 1);
		gd.UploadToCommitedGPUBuffer(&inds[0], &IndexUploadBuffer, &IndexBuffer, true);
		IndexBuffer.AddResourceBarrierTransitoinToCommand(gd.gpucmd, D3D12_RESOURCE_STATE_INDEX_BUFFER);
		IndexBufferView.BufferLocation = IndexBuffer.resource->GetGPUVirtualAddress();
		IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
		IndexBufferView.SizeInBytes = sizeof(TriangleIndex) * inds.size();

		IndexNum = inds.size() * 3;
		VertexNum = vert.size();
		topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}

	MatrixCount = matrixCount;
	UINT ncbElementBytes = (((sizeof(matrix) * MatrixCount) + 255) & ~255); //256의 배수
	GPUResource ToOffsetMatrixsCB_Upload = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1);
	//ToOffsetMatrixsCB_Upload.resource->Map(0, NULL, (void**)&OffsetMatrixs);
	////make DefaultToWorldArr, ToLocalArr
	//for (int i = 0; i < MatrixCount; ++i) {
	//	OffsetMatrixs[i] = NodeLocalMatrixs[i];
	//}
	//ToOffsetMatrixsCB_Upload.resource->Unmap(0, nullptr);
	ToOffsetMatrixsCB = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1);
	gd.UploadToCommitedGPUBuffer(NodeLocalMatrixs, &ToOffsetMatrixsCB_Upload, &ToOffsetMatrixsCB);

	gd.gpucmd.Close(true);
	gd.gpucmd.Execute(true);
	gd.gpucmd.WaitGPUComplete();
	//ToOffsetMatrixsCB
	ToOffsetMatrixsCB_Upload.Release();
	gd.gpucmd.Reset(true);

	int n = gd.TextureDescriptorAllotter.Alloc();
	D3D12_CPU_DESCRIPTOR_HANDLE hCPU = gd.TextureDescriptorAllotter.GetCPUHandle(n);
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = ToOffsetMatrixsCB.resource->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = ncbElementBytes;
	gd.pDevice->CreateConstantBufferView(&cbvDesc, hCPU);
	ToOffsetMatrixsCB.descindex.Set(false, n);

	game.AddMesh(this);
}

void BumpSkinMesh::Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum, ui32 slotIndex)
{
	pCommandList->IASetVertexBuffers(0, 2, RenderVBufferView);
	pCommandList->IASetPrimitiveTopology(topology);
	if (IndexNum > 0)
	{
		pCommandList->IASetIndexBuffer(&IndexBufferView);
		pCommandList->DrawIndexedInstanced(SubMeshIndexStart[slotIndex + 1] - SubMeshIndexStart[slotIndex], instanceNum, SubMeshIndexStart[slotIndex], 0, 0);
	}
	else
	{
		ui32 VertexOffset = 0;
		pCommandList->DrawInstanced(VertexNum, instanceNum, VertexOffset, 0);
	}
}

void BumpSkinMesh::Release()
{
	if (OffsetMatrixs) {
		delete[] OffsetMatrixs;
		OffsetMatrixs = nullptr;
	}
	ToOffsetMatrixsCB.Release();
	if (toNodeIndex) {
		delete[] toNodeIndex;
		toNodeIndex = nullptr;
	}
	BoneMatrixs.clear();
	BoneWeightBuffer.Release();
	BoneWeightUploadBuffer.Release();
	if (VertexSRV.type == 'n') {
		gd.TextureDescriptorAllotter.Free(VertexSRV.index);
		VertexSRV.Set(false, 0, 0);
	}
	if (BoneSRV.type == 'n') {
		gd.TextureDescriptorAllotter.Free(BoneSRV.index);
		BoneSRV.Set(false, 0, 0);
	}
	rmesh.Release();
	vertexData.clear();
	Mesh::Release();
}

void BumpSkinMesh::BatchRender(ID3D12GraphicsCommandList* pCommandList)
{
}

#pragma endregion

#pragma endregion

#pragma region ModelCode

void ModelNode::PushRenderBatch(void* model, const matrix& parentMat, void* pGameobject)
{
	Model* pModel = (Model*)model;
	XMMATRIX sav;
	GameObject* obj = (GameObject*)pGameobject;
	if (obj == nullptr) sav = XMMatrixMultiply(transform, parentMat);
	else {
		int nodeindex = ((byte8*)this - (byte8*)pModel->Nodes) / sizeof(ModelNode);
		sav = XMMatrixMultiply(obj->transforms_innerModel[nodeindex], parentMat);
	}

	if (numMesh != 0 && Meshes != nullptr) {
		matrix m = sav;
		m.transpose();

		for (int i = 0; i < numMesh; ++i) {
			BumpMesh* Bmesh = (BumpMesh*)((BumpMesh*)pModel->mMeshes[Meshes[i]]);
			for (int k = 0; k < Bmesh->subMeshNum; ++k) {
				using PBRRPI = PBRShader1::RootParamId;
				Bmesh->InstanceData[k].PushInstance(RenderInstanceData(m, materialIndex[k]));
			}
		}
	}

	if (numChildren != 0 && Childrens != nullptr) {
		for (int i = 0; i < numChildren; ++i) {
			Childrens[i]->PushRenderBatch(model, sav, pGameobject);
		}
	}
}

void ModelNode::BakeAABB(void* origin, const matrix& parentMat)
{
	Model* model = (Model*)origin;
	XMMATRIX sav = XMMatrixMultiply(transform, parentMat);
	//if (this == model->RootNode) {
	//	sav = parentMat;
	//}
	if (numMesh != 0 && Meshes != nullptr) {
		matrix m = sav;
		vec4 AABB[2];

		BoundingOrientedBox obb = model->mMeshes[Meshes[0]]->GetOBB();
		BoundingOrientedBox obb_world;
		obb.Transform(obb_world, sav);

		gd.GetAABBFromOBB(model->AABB, obb_world);
		for (int i = 1; i < numMesh; ++i) {
			BoundingOrientedBox obb = model->mMeshes[Meshes[i]]->GetOBB();
			BoundingOrientedBox obb_world;
			obb.Transform(obb_world, sav);

			gd.GetAABBFromOBB(model->AABB, obb_world);
		}
	}

	for (int i = 0; i < numChildren; ++i) {
		ModelNode* node = Childrens[i];
		node->BakeAABB(origin, sav);
	}
}

void ModelNode::RenderMeshOBBs(void* origin, const matrix& parentMat, void* gameobj) {
	using OCSRP = OnlyColorShader::RootParamId;
	GameObject* obj = (GameObject*)gameobj;
	Model* model = (Model*)origin;
	XMMATRIX sav = XMMatrixMultiply(transform, parentMat);
	if (gameobj) {
		int nodeindex = ((char*)this - (char*)model->RootNode) / sizeof(ModelNode);
		sav = XMMatrixMultiply(obj->transforms_innerModel[nodeindex], parentMat);
	}
	
	if (numMesh != 0 && Meshes != nullptr) {
		matrix m = sav;
		vec4 AABB[2];

		BoundingOrientedBox obb = model->mMeshes[Meshes[0]]->GetOBB();
		BoundingOrientedBox obb_world;
		obb.Transform(obb_world, sav);
		if (obb_world.Extents.x > 0) {
			matrix worldMat = XMMatrixScaling(obb_world.Extents.x, obb_world.Extents.y, obb_world.Extents.z);
			worldMat.trQ(obb_world.Orientation);
			worldMat.pos.f3 = obb_world.Center;
			worldMat.pos.w = 1;
			worldMat.transpose();
			gd.gpucmd->SetGraphicsRoot32BitConstants(OCSRP::Const_Transform, 16, &worldMat, 0);
			game.OBBDebugMesh->Render(gd.gpucmd, 1, 0);
		}

		for (int i = 1; i < numMesh; ++i) {
			BoundingOrientedBox obb = model->mMeshes[Meshes[i]]->GetOBB();
			BoundingOrientedBox obb_world;
			obb.Transform(obb_world, sav);

			if (obb_world.Extents.x > 0) {
				matrix worldMat = XMMatrixScaling(obb_world.Extents.x, obb_world.Extents.y, obb_world.Extents.z);
				worldMat.trQ(obb_world.Orientation);
				worldMat.pos.f3 = obb_world.Center;
				worldMat.pos.w = 1;
				worldMat.transpose();
				gd.gpucmd->SetGraphicsRoot32BitConstants(OCSRP::Const_Transform, 16, &worldMat, 0);
				game.OBBDebugMesh->Render(gd.gpucmd, 1, 0);
			}
		}
	}

	for (int i = 0; i < numChildren; ++i) {
		ModelNode* node = Childrens[i];
		node->RenderMeshOBBs(origin, sav, gameobj);
	}
}

void ModelNode::PushOBBs(void* origin, const matrix& parentMat, vector<BoundingOrientedBox>* obbArr, void* gameobj)
{
	if (obbArr == nullptr) return;
	Model* model = (Model*)origin;
	GameObject* obj = (GameObject*)gameobj;
	XMMATRIX sav = XMMatrixMultiply(transform, parentMat);
	if (gameobj) {
		int nodeindex = ((char*)this - (char*)model->RootNode) / sizeof(ModelNode);
		sav = XMMatrixMultiply(obj->transforms_innerModel[nodeindex], parentMat);
	}
	if (this->aabbArr.size() > 0) {
		for (int i = 0; i < aabbArr.size(); ++i) {
			BoundingBox aabb = aabbArr[i];
			BoundingOrientedBox obb;
			obb = BoundingOrientedBox(aabb.Center, aabb.Extents, vec4(0, 0, 0, 1));
			BoundingOrientedBox obb_world;
			obb.Transform(obb_world, sav);
			obbArr->push_back(obb_world);
		}
	}

	for (int i = 0; i < numChildren; ++i) {
		ModelNode* node = Childrens[i];
		node->PushOBBs(origin, sav, obbArr, gameobj);
	}
}

void ModelNode::Release() {
	name.clear();
	parent = nullptr;
	if (Childrens) {
		delete[] Childrens;
		Childrens = nullptr;
	}
	if (Meshes) {
		delete[] Meshes;
		Meshes = nullptr;
	}

	if (Mesh_SkinMeshindex) {
		delete[] Mesh_SkinMeshindex;
		Mesh_SkinMeshindex = nullptr;
	}

	aabbArr.clear();

	if (materialIndex) {
		delete[] materialIndex;
		materialIndex = nullptr;
	}
}

void Model::LoadModelFile2(string filename)
{
	ifstream ifs{ filename, ios_base::binary };
	ifs.read((char*)&mNumMeshes, sizeof(unsigned int));
	mMeshes = new Mesh * [mNumMeshes];

	ifs.read((char*)&nodeCount, sizeof(unsigned int));
	Nodes = new ModelNode[nodeCount];

	//new0
	ifs.read((char*)&mNumTextures, sizeof(unsigned int));
	ifs.read((char*)&mNumMaterials, sizeof(unsigned int));

	vector<vector<BumpSkinMesh::BoneData>> skinboneWeights_Arr;

	int BSMCount = 0;
	MaterialTableStart = game.MaterialTable.size();
	for (int i = 0; i < mNumMeshes; ++i) {
		bool hasBone = false;
		ifs.read((char*)&hasBone, sizeof(bool));

		XMFLOAT3 AABB[2];
		ifs.read((char*)AABB, sizeof(XMFLOAT3) * 2);

		//new1
		//ifs.read((char*)&mesh->material_index, sizeof(int));

		XMFLOAT3 MAABB[2];
		unsigned int vertSiz = 0;
		unsigned int subMeshCount = 0;
		ifs.read((char*)&vertSiz, sizeof(unsigned int));
		ifs.read((char*)&subMeshCount, sizeof(unsigned int));

		vector<BumpMesh::Vertex> vertices;
		vector<BumpSkinMesh::Vertex> skinvertices;
		
		vector<TriangleIndex> indexs;
		int stackSiz = 0;
		int prevSiz = 0;
		int* SubMeshSlots = new int[subMeshCount + 1];
		SubMeshSlots[0] = 0;

		if (hasBone) {
			skinboneWeights_Arr.push_back(vector<BumpSkinMesh::BoneData>());
			vector<BumpSkinMesh::BoneData>& skinboneWeights = skinboneWeights_Arr[skinboneWeights_Arr.size() - 1];
			BSMCount += 1;
			BumpSkinMesh* mesh = new BumpSkinMesh();
			mesh->type = Mesh::MeshType::_SkinedBumpMesh;
			mesh->SetOBBDataWithAABB(AABB[0], AABB[1]);

			skinvertices.reserve(vertSiz); skinvertices.resize(vertSiz);
			skinboneWeights.reserve(vertSiz); skinboneWeights.resize(vertSiz);
			for (int k = 0; k < vertSiz; ++k) {
				ifs.read((char*)&skinvertices[k].position, sizeof(XMFLOAT3));
				if (k == 0) {
					MAABB[0] = skinvertices[k].position;
					MAABB[1] = skinvertices[k].position;
				}

				if (MAABB[0].x > skinvertices[k].position.x) MAABB[0].x = skinvertices[k].position.x;
				if (MAABB[0].y > skinvertices[k].position.y) MAABB[0].y = skinvertices[k].position.y;
				if (MAABB[0].z > skinvertices[k].position.z) MAABB[0].z = skinvertices[k].position.z;

				if (MAABB[1].x < skinvertices[k].position.x) MAABB[1].x = skinvertices[k].position.x;
				if (MAABB[1].y < skinvertices[k].position.y) MAABB[1].y = skinvertices[k].position.y;
				if (MAABB[1].z < skinvertices[k].position.z) MAABB[1].z = skinvertices[k].position.z;

				ifs.read((char*)&skinvertices[k].u, sizeof(float));
				ifs.read((char*)&skinvertices[k].v, sizeof(float));
				ifs.read((char*)&skinvertices[k].normal, sizeof(XMFLOAT3));
				ifs.read((char*)&skinvertices[k].tangent, sizeof(XMFLOAT3));

				// non use
				XMFLOAT3 bitangent;
				ifs.read((char*)&bitangent, sizeof(XMFLOAT3));

				//bonedata
				int boneindex = 0;
				float boneweight = 0;
				for (int u = 0; u < 4; ++u) {
					ifs.read((char*)&skinboneWeights[k].boneWeight[u].boneID, sizeof(int));
					ifs.read((char*)&skinboneWeights[k].boneWeight[u].weight, sizeof(float));
				}
			}

			for (int k = 0; k < subMeshCount; ++k) {
				int indCnt = 0;
				ifs.read((char*)&indCnt, sizeof(int));
				int tricnt = (indCnt / 3);
				stackSiz += tricnt;
				indexs.reserve(stackSiz);
				indexs.resize(stackSiz);
				for (int u = 0; u < tricnt; ++u) {
					ifs.read((char*)&indexs[prevSiz + u].v[0], sizeof(UINT));
					ifs.read((char*)&indexs[prevSiz + u].v[1], sizeof(UINT));
					ifs.read((char*)&indexs[prevSiz + u].v[2], sizeof(UINT));
					skinvertices[indexs[prevSiz + u].v[0]].materialIndex = k;
					skinvertices[indexs[prevSiz + u].v[1]].materialIndex = k;
					skinvertices[indexs[prevSiz + u].v[2]].materialIndex = k;
				}
				prevSiz = stackSiz;
				SubMeshSlots[k + 1] = 3 * prevSiz;
			}

			ifs.read((char*)&mesh->MatrixCount, sizeof(int));
			mesh->OffsetMatrixs = new matrix[mesh->MatrixCount];
			mesh->toNodeIndex = new int[mesh->MatrixCount];
			for (int k = 0; k < mesh->MatrixCount; ++k) {
				matrix offset;
				ifs.read((char*)&mesh->OffsetMatrixs[k], sizeof(matrix));
				//OffsetMatrixs[k].pos /= 100;
				//OffsetMatrixs[k].pos.w = 1;
				mesh->OffsetMatrixs[k].transpose();
			}
			for (int k = 0; k < mesh->MatrixCount; ++k) {
				ifs.read((char*)&mesh->toNodeIndex[k], sizeof(int));
			}

			float unitMulRate = 1 * AABB[0].x / MAABB[0].x;
			for (int k = 0; k < vertSiz; ++k) {
				skinvertices[k].position.x *= unitMulRate;
				skinvertices[k].position.y *= unitMulRate;
				skinvertices[k].position.z *= unitMulRate;
			}

			mesh->CreateMesh_FromVertexAndIndexData(skinvertices, skinboneWeights, indexs, subMeshCount, SubMeshSlots, mesh->OffsetMatrixs, mesh->MatrixCount);
			mMeshes[i] = mesh;
		}
		else {
			BumpMesh* mesh = new BumpMesh();
			mesh->type = Mesh::MeshType::_BumpMesh;
			
			vertices.reserve(vertSiz); vertices.resize(vertSiz);
			for (int k = 0; k < vertSiz; ++k) {
				ifs.read((char*)&vertices[k].position, sizeof(XMFLOAT3));
				if (k == 0) {
					MAABB[0] = vertices[k].position;
					MAABB[1] = vertices[k].position;
				}

				if (MAABB[0].x > vertices[k].position.x) MAABB[0].x = vertices[k].position.x;
				if (MAABB[0].y > vertices[k].position.y) MAABB[0].y = vertices[k].position.y;
				if (MAABB[0].z > vertices[k].position.z) MAABB[0].z = vertices[k].position.z;

				if (MAABB[1].x < vertices[k].position.x) MAABB[1].x = vertices[k].position.x;
				if (MAABB[1].y < vertices[k].position.y) MAABB[1].y = vertices[k].position.y;
				if (MAABB[1].z < vertices[k].position.z) MAABB[1].z = vertices[k].position.z;

				ifs.read((char*)&vertices[k].u, sizeof(float));
				ifs.read((char*)&vertices[k].v, sizeof(float));
				ifs.read((char*)&vertices[k].normal, sizeof(XMFLOAT3));
				ifs.read((char*)&vertices[k].tangent, sizeof(XMFLOAT3));

				// non use
				XMFLOAT3 bitangent;
				ifs.read((char*)&bitangent, sizeof(XMFLOAT3));
			}

			for (int k = 0; k < subMeshCount; ++k) {
				int indCnt = 0;
				ifs.read((char*)&indCnt, sizeof(int));
				int tricnt = (indCnt / 3);
				stackSiz += tricnt;
				indexs.reserve(stackSiz);
				indexs.resize(stackSiz);
				for (int u = 0; u < tricnt; ++u) {
					ifs.read((char*)&indexs[prevSiz + u].v[0], sizeof(UINT));
					ifs.read((char*)&indexs[prevSiz + u].v[1], sizeof(UINT));
					ifs.read((char*)&indexs[prevSiz + u].v[2], sizeof(UINT));
					vertices[indexs[prevSiz + u].v[0]].materialIndex = k;
					vertices[indexs[prevSiz + u].v[1]].materialIndex = k;
					vertices[indexs[prevSiz + u].v[2]].materialIndex = k;
				}
				prevSiz = stackSiz;
				SubMeshSlots[k + 1] = 3 * prevSiz;
			}

			float unitMulRate = 1 * AABB[0].x / MAABB[0].x;
			for (int k = 0; k < vertSiz; ++k) {
				vertices[k].position.x *= unitMulRate;
				vertices[k].position.y *= unitMulRate;
				vertices[k].position.z *= unitMulRate;
			}

			mesh->SetOBBDataWithAABB(MAABB[0], MAABB[1]);
			mesh->CreateMesh_FromVertexAndIndexData(vertices, indexs, subMeshCount, SubMeshSlots);
			mMeshes[i] = mesh;
		}
	}

	mNumSkinMesh = BSMCount;
	mBumpSkinMeshs = new BumpSkinMesh * [mNumSkinMesh];
	int cnt = 0;
	for (int i = 0; i < mNumMeshes; ++i) {
		Mesh* mesh = mMeshes[i];
		if (mesh->type == Mesh::_SkinedBumpMesh) {
			mBumpSkinMeshs[cnt] = (BumpSkinMesh*)mesh;
			cnt += 1;
		}
	}

	for (int i = 0; i < nodeCount; ++i) {
		int namelen = 0;
		ifs.read((char*)&namelen, sizeof(int));
		char name[256] = {};
		ifs.read((char*)name, namelen);
		name[namelen] = 0;
		Nodes[i].name = name;

		/*vec4 pos;
		ifs.read((char*)&pos, sizeof(float) * 3);
		vec4 rot = 0;
		ifs.read((char*)&rot, sizeof(float) * 3);
		vec4 scale = 0;
		ifs.read((char*)&scale, sizeof(float) * 3);*/
		matrix WorldMat;
		ifs.read((char*)&WorldMat, sizeof(matrix));

		if (i == 0) {
			matrix mat;
			mat.Id();
			Nodes[i].transform = mat;
		}
		else {
			/*matrix mat;
			mat.Id();
			rot *= 3.141592f / 180.0f;
			mat *= XMMatrixScaling(scale.x, scale.y, scale.z);
			mat *= XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z);
			mat.pos.f3 = pos.f3;
			mat.pos.w = 1;*/

			WorldMat.transpose();
			Nodes[i].transform = WorldMat;
		}

		int parent = 0;
		ifs.read((char*)&parent, sizeof(int));
		if (parent < 0) Nodes[i].parent = nullptr;
		else Nodes[i].parent = &Nodes[parent];

		ifs.read((char*)&Nodes[i].numChildren, sizeof(unsigned int));
		if (Nodes[i].numChildren != 0) Nodes[i].Childrens = new ModelNode * [Nodes[i].numChildren];

		ifs.read((char*)&Nodes[i].numMesh, sizeof(unsigned int));
		if (Nodes[i].numMesh != 0) Nodes[i].Meshes = new unsigned int[Nodes[i].numMesh];

		for (int k = 0; k < Nodes[i].numChildren; ++k) {
			int child = 0;
			ifs.read((char*)&child, sizeof(int));
			if (child < 0) Nodes[i].Childrens[k] = nullptr;
			else Nodes[i].Childrens[k] = &Nodes[child];
		}

		int skincap = 0;
		int tempMeshIndexArr[256] = {};
		for (int k = 0; k < Nodes[i].numMesh; ++k) {
			ifs.read((char*)&Nodes[i].Meshes[k], sizeof(int));

			if (mMeshes[Nodes[i].Meshes[k]]->type == Mesh::MeshType::_SkinedBumpMesh) {
				tempMeshIndexArr[skincap] = k;
				skincap += 1;
			}

			int num_materials = 0;
			ifs.read((char*)&num_materials, sizeof(int));
			Nodes[i].materialIndex = new int[num_materials];
			for (int u = 0; u < num_materials; ++u) {
				int material_id = 0;
				ifs.read((char*)&material_id, sizeof(int));
				material_id += MaterialTableStart;
				Nodes[i].materialIndex[u] = material_id;
			}
		}

		if (skincap > 0) {
			Nodes[i].Mesh_SkinMeshindex = new int[Nodes[i].numMesh];
			for (int k = 0; k < Nodes[i].numMesh; ++k) {
				Nodes[i].Mesh_SkinMeshindex[k] = -1;
			}
			for (int k = 0; k < skincap; ++k) {
				for (int u = 0; u < mNumSkinMesh; ++u) {
					if (mBumpSkinMeshs[u] == mMeshes[Nodes[i].Meshes[tempMeshIndexArr[k]]]) {
						Nodes[i].Mesh_SkinMeshindex[tempMeshIndexArr[k]] = u;
						break;
					}
				}
			}
		}

		//ifs.read((char*)&Nodes[i].Meshes[0], sizeof(int) * Nodes[i].numMesh);

		int ColliderCount = 0;
		ifs.read((char*)&ColliderCount, sizeof(int));
		Nodes[i].aabbArr.reserve(ColliderCount);
		Nodes[i].aabbArr.resize(ColliderCount);
		for (int k = 0; k < ColliderCount; ++k) {
			ifs.read((char*)&Nodes[i].aabbArr[k].Center, sizeof(XMFLOAT3));
			ifs.read((char*)&Nodes[i].aabbArr[k].Extents, sizeof(XMFLOAT3));
		}
	}
	//new2

	//mTextures = new GPUResource * [mNumTextures];
	TextureTableStart = game.TextureTable.size();
	for (int i = 0; i < mNumTextures; ++i) {
		GPUResource* Texture = new GPUResource();
		game.TextureTable.push_back(Texture);

		Texture->resource = nullptr;
		string DDSFilename = filename;
		for (int k = 0; k < 6; ++k) DDSFilename.pop_back();
		DDSFilename += to_string(i);
		DDSFilename += ".dds";

		ifstream ifs2{ DDSFilename , ios_base::binary };
		if (ifs2.good()) {
			ifs2.close();
			//load dds texture
			wstring wDDSFilename;
			wDDSFilename.reserve(DDSFilename.size());
			for (int k = 0; k < DDSFilename.size(); ++k) {
				wDDSFilename.push_back(DDSFilename[k]);
			}
			Texture->CreateTexture_fromFile(wDDSFilename.c_str(), game.basicTexFormat, game.basicTexMip);
		}
		else {
			string texfile = filename;
			for (int u = 0; u < 6; ++u) texfile.pop_back();
			texfile += to_string(i);
			texfile += ".tex";
			void* pdata = nullptr;
			int width = 0, height = 0;
			ifstream ifstex{ texfile, ios_base::binary };
			if (ifstex.good()) {
				ifstex.read((char*)&width, sizeof(int));
				ifstex.read((char*)&height, sizeof(int));
				int datasiz = 4 * width * height;
				pdata = malloc(4 * width * height);
				ifstex.read((char*)pdata, datasiz);
			}
			else {
				dbglog1(L"texture is not exist. %d\n", 0);
				return;
			}

			//make dds texture in DDSFilename path
			char BMPFile[512] = {};
			strcpy_s(BMPFile, DDSFilename.c_str());
			strcpy_s(&BMPFile[DDSFilename.size() - 3], 4, "bmp");

			imgform::PixelImageObject pio;
			pio.data = (imgform::RGBA_pixel*)pdata;
			pio.width = width;
			pio.height = height;
			pio.rawDataToBMP(BMPFile);

			gd.bmpTodds(game.basicTexMip, game.basicTexFormatStr, BMPFile);

			int pos = DDSFilename.find_last_of('/');
			char* onlyDDSfilename = &DDSFilename[pos + 1];
			MoveFileA(onlyDDSfilename, DDSFilename.c_str());

			wstring wDDSFilename;
			wDDSFilename.reserve(DDSFilename.size());
			for (int k = 0; k < DDSFilename.size(); ++k) {
				wDDSFilename.push_back(DDSFilename[k]);
			}
			Texture->CreateTexture_fromFile(wDDSFilename.c_str(), game.basicTexFormat, game.basicTexMip);

			DeleteFileA(BMPFile);
			//Texture->CreateTexture_fromImageBuffer(width, height, (BYTE*)pdata, DXGI_FORMAT_BC2_UNORM);

			free(pdata);
		}

		// copy pdata?
		//mTextures[i] = Texture;
	}

	//mMaterials = new Material * [mNumMaterials];
	for (int i = 0; i < mNumMaterials; ++i) {
		Material* mat = new Material();
		//ifs.read((char*)mat, sizeof(Material));

		int namelen = 0;
		ifs.read((char*)&namelen, sizeof(int));
		char name[512] = {};
		ifs.read((char*)name, namelen * sizeof(char));
		name[namelen] = 0;
		memcpy_s(mat->name, 40, name, 40);
		mat->name[39] = 0;

		ifs.read((char*)&mat->clr.diffuse, sizeof(float) * 4);

		ifs.read((char*)&mat->metallicFactor, sizeof(float));

		float smoothness = 0;
		ifs.read((char*)&smoothness, sizeof(float));
		mat->roughnessFactor = 1.0f - smoothness;

		ifs.read((char*)&mat->clr.bumpscaling, sizeof(float));

		vec4 tiling, offset = 0;
		vec4 tiling2, offset2 = 0;
		ifs.read((char*)&tiling, sizeof(float) * 2);
		ifs.read((char*)&offset, sizeof(float) * 2);
		ifs.read((char*)&tiling2, sizeof(float) * 2);
		ifs.read((char*)&offset2, sizeof(float) * 2);
		mat->TilingX = tiling.x;
		mat->TilingY = tiling.y;
		mat->TilingOffsetX = offset.x;
		mat->TilingOffsetY = offset.y;

		bool isTransparent = false;
		ifs.read((char*)&isTransparent, sizeof(bool));
		if (isTransparent) {
			mat->gltf_alphaMode = mat->Blend;
		}
		else mat->gltf_alphaMode = mat->Opaque;

		bool emissive = 0;
		ifs.read((char*)&emissive, sizeof(bool));
		if (emissive) {
			mat->clr.emissive = vec4(1, 1, 1, 1);
		}
		else {
			mat->clr.emissive = vec4(0, 0, 0, 0);
		}

		ifs.read((char*)&mat->ti.BaseColor, sizeof(int));
		ifs.read((char*)&mat->ti.Normal, sizeof(int));
		ifs.read((char*)&mat->ti.Metalic, sizeof(int));
		ifs.read((char*)&mat->ti.AmbientOcculsion, sizeof(int));
		ifs.read((char*)&mat->ti.Roughness, sizeof(int));
		ifs.read((char*)&mat->ti.Emissive, sizeof(int));

		int diffuse2, normal2 = 0;
		ifs.read((char*)&diffuse2, sizeof(int));
		ifs.read((char*)&normal2, sizeof(int));

		mat->ShiftTextureIndexs(TextureTableStart);
		mat->SetDescTable();
		//mMaterials[i] = mat;
		game.MaterialTable.push_back(mat);
		if (game.isAssetAddingInGlobal == false) {
			game.RenderMaterialTable.push_back(mat);
		}
	}

	RootNode = &Nodes[0];

	//calcul normal Node Local Tr Mats (WhenSkinMesh enabled)
	if (mNumSkinMesh > 0) {
		DefaultNodelocalTr = new matrix[nodeCount];
		for (int i = 0; i < nodeCount; ++i) {
			DefaultNodelocalTr[i].Id();
		}
	}

	NodeOffsetMatrixArr = new matrix[nodeCount];
	for (int i = 0; i < mNumSkinMesh; ++i) {
		BumpSkinMesh* bsm = mBumpSkinMeshs[i];
		for (int k = 0; k < bsm->MatrixCount; ++k) {
			matrix invBoneWorld = bsm->OffsetMatrixs[k];
			int nodeindex = bsm->toNodeIndex[k];
			NodeOffsetMatrixArr[nodeindex] = invBoneWorld;
		}
	}
	matrix IdMat;
	for (int i = 0; i < nodeCount; ++i) {
		if (NodeOffsetMatrixArr[i].pos == IdMat.pos && NodeOffsetMatrixArr[i].look == IdMat.look
			&& NodeOffsetMatrixArr[i].right == IdMat.right && NodeOffsetMatrixArr[i].up == IdMat.up) {
			// offset 행렬이 단위행렬일때
			ModelNode* node = Nodes[i].parent;
			if (node == nullptr) {
				continue;
			}
			int parentNodeindex = ((char*)node - (char*)Nodes) / sizeof(ModelNode);
			NodeOffsetMatrixArr[i] = NodeOffsetMatrixArr[parentNodeindex];
		}
	}

	for (int i = 0; i < mNumSkinMesh; ++i) {
		BumpSkinMesh* bsm = mBumpSkinMeshs[i];
		for (int k = 0; k < bsm->MatrixCount; ++k) {
			vec4 det;
			matrix invBoneWorld = bsm->OffsetMatrixs[k];
			invBoneWorld.transpose();
			invBoneWorld = XMMatrixInverse(&det.v4, invBoneWorld);
			int nodeindex = bsm->toNodeIndex[k];
			ModelNode* node = Nodes[nodeindex].parent;
			matrix parentBoneOffset;

			while (node != nullptr) {
				int parentindex = ((char*)node - (char*)Nodes) / sizeof(ModelNode);
				parentBoneOffset = NodeOffsetMatrixArr[parentindex];
				parentBoneOffset.transpose();
				goto SET_NODELOCALMAT;
				/*for (int u = 0; u < bsm->MatrixCount; ++u) {
					if (parentindex == bsm->toNodeIndex[u]) {
						parentBoneOffset = bsm->OffsetMatrixs[u];
						parentBoneOffset.transpose();
						goto SET_NODELOCALMAT;
					}
				}*/
				node = node->parent;
			}

		SET_NODELOCALMAT:
			DefaultNodelocalTr[nodeindex] = invBoneWorld * parentBoneOffset;
		}
	}

	Retargeting_Humanoid();

	BakeAABB();
}

void Model::DebugPrintHierarchy(ModelNode* node, int depth)
{
	if (node == nullptr) return;

	string logMsg = "";
	for (int i = 0; i < depth; ++i) logMsg += "  - ";

	logMsg += node->name + "\n";
	OutputDebugStringA(logMsg.c_str());

	for (int i = 0; i < node->numChildren; ++i) {
		DebugPrintHierarchy(node->Childrens[i], depth + 1);
	}
}

void Model::BakeAABB()
{
	AABB[0] = 0;
	AABB[1] = 0;
	RootNode->BakeAABB(this, XMMatrixIdentity());
	OBB_Tr = 0.5f * (AABB[0] + AABB[1]);
	OBB_Ext = AABB[1] - OBB_Tr;
}

BoundingOrientedBox Model::GetOBB()
{
	return BoundingOrientedBox(OBB_Tr.f3, OBB_Ext.f3, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
}

void Model::Retargeting_Humanoid()
{
	Humanoid_retargeting = new int[nodeCount];
	for (int i = 0; i < nodeCount; ++i) {
		ModelNode* node = &Nodes[i];
		Humanoid_retargeting[i] = -1;
		for (int k = 0; k < HumanoidAnimation::HumanBoneCount; ++k) {
			if (strcmp(node->name.c_str(), HumanoidAnimation::HumanBoneNames[k]) == 0) {
				Humanoid_retargeting[i] = k;
				break;
			}
		}
	}
}

void Model::PushRenderBatch(matrix worldMatrix, void* pGameobject)
{
	RootNode->PushRenderBatch(this, worldMatrix, pGameobject);
}

int Model::FindNodeIndexByName(const std::string& name)
{
	for (int i = 0; i < nodeCount; ++i) {
		if (Nodes[i].name == name) return i;
	}
	return -1;
}

void Model::Release() {
	mName.clear();
	RootNode = nullptr;
	NodeArr.clear();
	nodeindexmap.clear();
	if (Nodes) {
		for (int i = 0; i < nodeCount; ++i) {
			Nodes[i].Release();
		}
		delete[] Nodes;
		Nodes = nullptr;
		nodeCount = 0;
	}

	if (mMeshes) {
		for (int i = 0; i < mNumMeshes; ++i) {
			mMeshes[i]->Release();
			delete mMeshes[i];
		}
		delete[] mMeshes;
		mMeshes = nullptr;
		mNumMeshes = 0;
	}

	if (mBumpSkinMeshs) {
		delete[] mBumpSkinMeshs;
		mBumpSkinMeshs = nullptr;
		mNumSkinMesh = 0;
	}

	if (DefaultNodelocalTr) {
		delete[] DefaultNodelocalTr;
		DefaultNodelocalTr = nullptr;
	}

	if (NodeOffsetMatrixArr) {
		delete[] NodeOffsetMatrixArr;
		NodeOffsetMatrixArr = nullptr;
	}

	for (int i = TextureTableStart; i < mNumTextures + TextureTableStart; ++i) {
		game.TextureTable[i]->Release();
		delete game.TextureTable[i];
		game.TextureTable[i] = nullptr;
	}

	for (int i = MaterialTableStart; i < mNumMaterials + MaterialTableStart; ++i) {
		game.MaterialTable[i]->Release();
		delete game.MaterialTable[i];
	}

	AABB[0] = 0;
	AABB[1] = 0;
	OBB_Tr = 0;
	OBB_Ext = 0;
	BindPose.clear();
	
	if (Humanoid_retargeting) {
		delete[] Humanoid_retargeting;
		Humanoid_retargeting = nullptr;
	}
}

#pragma endregion

int Shape::GetShapeIndex(string meshName)
{
	auto f = Shape::StrToShapeIndex.find(meshName);
	if (f != Shape::StrToShapeIndex.end()) {
		return f->second;
	}
	return -1;
}

int Shape::AddMesh(string name, Mesh* ptr)
{
	int index = Shape::ShapeStrTable.size();
	Shape s;
	s.SetMesh(ptr);
	Shape::ShapeStrTable.push_back(name);
	Shape::ShapeTable.push_back(s);
	Shape::StrToShapeIndex.insert(pair<string, int>(name, index));
	return index;
}

int Shape::AddModel(string name, Model* ptr)
{
	int index = Shape::ShapeStrTable.size();
	Shape s;
	s.SetModel(ptr);
	Shape::ShapeStrTable.push_back(name);
	Shape::ShapeTable.push_back(s);
	Shape::StrToShapeIndex.insert(pair<string, int>(name, index));
	return index;
}

#pragma endregion

#pragma region MaterialCode

void Material::ShiftTextureIndexs(unsigned int TextureIndexStart)
{
	if (ti.Diffuse >= 0)ti.Diffuse += TextureIndexStart;
	if (ti.Specular >= 0)ti.Specular += TextureIndexStart;
	if (ti.Ambient >= 0)ti.Ambient += TextureIndexStart;
	if (ti.Emissive >= 0)ti.Emissive += TextureIndexStart;
	if (ti.Normal >= 0)ti.Normal += TextureIndexStart;
	if (ti.Roughness >= 0)ti.Roughness += TextureIndexStart;
	if (ti.Opacity >= 0)ti.Opacity += TextureIndexStart;
	if (ti.LightMap >= 0)ti.LightMap += TextureIndexStart;
	if (ti.Reflection >= 0)ti.Reflection += TextureIndexStart;
	if (ti.Sheen >= 0)ti.Sheen += TextureIndexStart;
	if (ti.ClearCoat >= 0)ti.ClearCoat += TextureIndexStart;
	if (ti.Transmission >= 0)ti.Transmission += TextureIndexStart;
	if (ti.Anisotropy >= 0)ti.Anisotropy += TextureIndexStart;
}

void Material::SetDescTable()
{
	DescIndex hDesc;
	DescIndex hOriginDesc;
	D3D12_CPU_DESCRIPTOR_HANDLE hcpu;

	// 텍스쳐 5개가 같은 머터리얼이 있는지 확인한다. (SRV Desc Heap 자리 재활용을 위해..)
	int findSame = -1;
	for (int i = 0; i < game.RenderMaterialTable.size(); ++i) {
		Material& mat = *game.RenderMaterialTable[i];
		bool b = mat.ti.Diffuse == ti.Diffuse;
		b = b && (mat.ti.Normal == ti.Normal);
		b = b && (mat.ti.AmbientOcculsion == ti.AmbientOcculsion);
		b = b && (mat.ti.Metalic == ti.Metalic);
		b = b && (mat.ti.Roughness == ti.Roughness);
		if (b) {
			findSame = i;
			break;
		}
	}

	if (findSame >= 0) {
		TextureSRVTableIndex = game.RenderMaterialTable[findSame]->TextureSRVTableIndex;
	}
	else {
		if (game.isAssetAddingInGlobal == false) {
			gd.ShaderVisibleDescPool.ImmortalAlloc_TextureSRV(&TextureSRVTableIndex, 5);

			hDesc = TextureSRVTableIndex;
			hOriginDesc = hDesc;

			GPUResource* diffuseTex = &game.DefaultTex;
			if (ti.Diffuse >= 0) diffuseTex = game.TextureTable[ti.Diffuse];
			GPUResource* normalTex = &game.DefaultNoramlTex;
			if (ti.Normal >= 0) normalTex = game.TextureTable[ti.Normal];
			GPUResource* ambientTex = &game.DefaultTex;
			if (ti.AmbientOcculsion >= 0) ambientTex = game.TextureTable[ti.AmbientOcculsion];
			GPUResource* MetalicTex = &game.DefaultAmbientTex;
			if (ti.Metalic >= 0) MetalicTex = game.TextureTable[ti.Metalic];
			GPUResource* roughnessTex = &game.DefaultAmbientTex;
			if (ti.Roughness >= 0) roughnessTex = game.TextureTable[ti.Roughness];

			const int inc = gd.CBVSRVUAVSize;
			gd.pDevice->CopyDescriptorsSimple(1, hDesc.hCreation.hcpu, diffuseTex->descindex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			hDesc.index += 1;
			gd.pDevice->CopyDescriptorsSimple(1, hDesc.hCreation.hcpu, normalTex->descindex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			hDesc.index += 1;
			gd.pDevice->CopyDescriptorsSimple(1, hDesc.hCreation.hcpu, ambientTex->descindex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			hDesc.index += 1;
			gd.pDevice->CopyDescriptorsSimple(1, hDesc.hCreation.hcpu, MetalicTex->descindex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			hDesc.index += 1;
			gd.pDevice->CopyDescriptorsSimple(1, hDesc.hCreation.hcpu, roughnessTex->descindex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			game.RenderTextureTable.push_back(diffuseTex);
			game.RenderTextureTable.push_back(normalTex);
			game.RenderTextureTable.push_back(ambientTex);
			game.RenderTextureTable.push_back(MetalicTex);
			game.RenderTextureTable.push_back(roughnessTex);
		}
	}

	UINT ncbElementBytes = ((sizeof(MaterialCB_Data) + 255) & ~255); //256의 배수
	if (CBData == nullptr) {
		CB_Resource = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1);
		CB_Resource.resource->Map(0, NULL, (void**)&CBData);
		*CBData = GetMatCB();
		CB_Resource.resource->Unmap(0, NULL);
	}

	if (game.isAssetAddingInGlobal == false) {
		gd.ShaderVisibleDescPool.ImmortalAlloc_MaterialCBV(&CB_Resource.descindex, 1);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc;
		cbv_desc.BufferLocation = CB_Resource.resource->GetGPUVirtualAddress();
		cbv_desc.SizeInBytes = ncbElementBytes;
		gd.pDevice->CreateConstantBufferView(&cbv_desc, CB_Resource.descindex.hCreation.hcpu);
	}//else 는 신경 안써도 된다. - 당장 ShaderVisible에 들어갈 수 없기 때문에.
}

MaterialCB_Data Material::GetMatCB()
{
	MaterialCB_Data CBData;
	CBData.baseColor = clr.base;
	CBData.ambientColor = clr.ambient;
	CBData.emissiveColor = clr.emissive;
	CBData.metalicFactor = metallicFactor;
	CBData.smoothness = roughnessFactor;
	CBData.bumpScaling = clr.bumpscaling;
	CBData.extra = 1.0f;
	CBData.TilingX = TilingX;
	CBData.TilingY = TilingY;
	CBData.TilingOffsetX = TilingOffsetX;
	CBData.TilingOffsetY = TilingOffsetY;
	return CBData;
}

MaterialST_Data Material::GetMatST()
{
	MaterialST_Data STData;
	STData.baseColor = clr.base;
	STData.ambientColor = clr.ambient;
	STData.emissiveColor = clr.emissive;
	STData.metalicFactor = metallicFactor;
	STData.smoothness = roughnessFactor;
	STData.bumpScaling = clr.bumpscaling;
	STData.TilingX = TilingX;
	STData.TilingY = TilingY;
	STData.TilingOffsetX = TilingOffsetX;
	STData.TilingOffsetY = TilingOffsetY;

	STData.diffuseTexId = TextureSRVTableIndex.index - gd.ShaderVisibleDescPool.TextureSRVStart;
	STData.normalTexId = STData.diffuseTexId + 1;
	STData.AOTexId = STData.normalTexId + 1;
	STData.metalicTexId = STData.AOTexId + 1;
	STData.roughnessTexId = STData.metalicTexId + 1;
	return STData;
}

void Material::Release() {
	CBData = nullptr;
	CB_Resource.Release();
	TextureSRVTableIndex.Set(false, 0, 0);
	
	ZeroMemory(this, sizeof(Material));
	memset(&ti, 0xFF, sizeof(TextureIndex));
	clr.base = vec4(1, 1, 1, 1);
	TilingX = 1;
	TilingY = 1;
	TilingOffsetX = 1;
	TilingOffsetY = 1;
}

void Material::InitMaterialStructuredBuffer(bool reset)
{
	if (MaterialStructuredBuffer.resource != nullptr) {
		if (reset) {
			MaterialStructuredBuffer.Release();
			MaterialStructuredBuffer.resource = nullptr;
			UINT ncbElementBytes = ((sizeof(MaterialST_Data) * gd.ShaderVisibleDescPool.MaterialCBVCap + 255) & ~255); //256의 배수
			MaterialStructuredBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1);
			MaterialStructuredBuffer.resource->Map(0, NULL, (void**)&MappedMaterialStructuredBuffer);
			for (int i = 0; i < game.RenderMaterialTable.size(); ++i) {
				MappedMaterialStructuredBuffer[i] = game.RenderMaterialTable[i]->GetMatST();
			}
			MaterialStructuredBuffer.resource->Unmap(0, NULL);
			LastMaterialStructureBufferUp = game.RenderMaterialTable.size();

			//MaterialStructuredBufferSRV를 재할당하지 않는다. (같은 자리를 차지한다.)
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.NumElements = gd.ShaderVisibleDescPool.MaterialCBVCap;
			srvDesc.Buffer.StructureByteStride = sizeof(MaterialST_Data);
			srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			gd.pDevice->CreateShaderResourceView(MaterialStructuredBuffer.resource, &srvDesc, MaterialStructuredBufferSRV.hCreation.hcpu);
		}
		else {
			MaterialStructuredBuffer.resource->Map(0, NULL, (void**)&MappedMaterialStructuredBuffer);
			for (int i = LastMaterialStructureBufferUp; i < game.RenderMaterialTable.size(); ++i) {
				MappedMaterialStructuredBuffer[i] = game.RenderMaterialTable[i]->GetMatST();
			}
			MaterialStructuredBuffer.resource->Unmap(0, NULL);
			LastMaterialStructureBufferUp = game.RenderMaterialTable.size();

			//MaterialStructuredBufferSRV를 재할당하지 않는다. (같은 자리를 차지한다.)
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.NumElements = gd.ShaderVisibleDescPool.MaterialCBVCap;
			srvDesc.Buffer.StructureByteStride = sizeof(MaterialST_Data);
			srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			gd.pDevice->CreateShaderResourceView(MaterialStructuredBuffer.resource, &srvDesc, MaterialStructuredBufferSRV.hCreation.hcpu);
		}
	}
	else {
		UINT ncbElementBytes = ((sizeof(MaterialST_Data) * gd.ShaderVisibleDescPool.MaterialCBVCap + 255) & ~255); //256의 배수
		MaterialStructuredBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1);
		MaterialStructuredBuffer.resource->Map(0, NULL, (void**)&MappedMaterialStructuredBuffer);
		for (int i = 0; i < game.RenderMaterialTable.size(); ++i) {
			MappedMaterialStructuredBuffer[i] = game.RenderMaterialTable[i]->GetMatST();
		}
		MaterialStructuredBuffer.resource->Unmap(0, NULL);
		LastMaterialStructureBufferUp = game.RenderMaterialTable.size();

		gd.ShaderVisibleDescPool.ImmortalAlloc(&MaterialStructuredBufferSRV, 1);
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = gd.ShaderVisibleDescPool.MaterialCBVCap;
		srvDesc.Buffer.StructureByteStride = sizeof(MaterialST_Data);
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		gd.pDevice->CreateShaderResourceView(MaterialStructuredBuffer.resource, &srvDesc, MaterialStructuredBufferSRV.hCreation.hcpu);
	}
}

#pragma endregion

#pragma region ShaderCode

#pragma region BasicShaderCode
Shader::Shader() {

}
Shader::~Shader() {

}
void Shader::InitShader() {
	CreateRootSignature();
	CreatePipelineState();
}

void Shader::CreateRootSignature()
{
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;

	D3D12_ROOT_PARAMETER1 rootParam[3] = {};

	rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParam[0].Constants.Num32BitValues = 20;
	rootParam[0].Constants.ShaderRegister = 0; // b0 : Camera Matrix (Projection, View) + Camera Positon
	rootParam[0].Constants.RegisterSpace = 0;

	rootParam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParam[1].Constants.Num32BitValues = 16;
	rootParam[1].Constants.ShaderRegister = 1; // b1 : Transform Matrix
	rootParam[1].Constants.RegisterSpace = 0;

	rootParam[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParam[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParam[2].Descriptor.ShaderRegister = 2; // b2
	rootParam[2].Descriptor.RegisterSpace = 0;

	D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	rootSigDesc1.NumParameters = 3;
	rootSigDesc1.pParameters = rootParam;
	rootSigDesc1.NumStaticSamplers = 0; // question002 : what is static samplers?
	rootSigDesc1.pStaticSamplers = NULL; //
	rootSigDesc1.Flags = d3dRootSignatureFlags;

	ID3DBlob* pd3dSignatureBlob = NULL;
	ID3DBlob* pd3dErrorBlob = NULL;
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_rootSigDesc;
	versioned_rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	versioned_rootSigDesc.Desc_1_1 = rootSigDesc1;
	D3D12SerializeVersionedRootSignature(&versioned_rootSigDesc, &pd3dSignatureBlob, &pd3dErrorBlob);
	gd.pDevice->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(),
		pd3dSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void
			**)&pRootSignature);
	if (pd3dErrorBlob) pd3dErrorBlob->Release();
	if (pd3dSignatureBlob) pd3dSignatureBlob->Release();
}

void Shader::CreatePipelineState()
{
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dVertexShaderBlob = NULL, * pd3dPixelShaderBlob = NULL;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature;

	gPipelineStateDesc.NodeMask = 0;

	//Input Asm
	gPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[3] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // color vec4
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // normal vec3
	};
	gPipelineStateDesc.InputLayout.NumElements = 3;
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/DefaultShader.hlsl", "VSMain", "vs_5_1", &pd3dVertexShaderBlob);

	//Hull Shader
	//gPipelineStateDesc.HS = NULLCODE;

	//Tessellation

	//Domain Shader
	//gPipelineStateDesc.DS = NULLCODE;

	//Geometry Shader
	//gPipelineStateDesc.GS = NULLCODE;

	//Stream Output
	//gPipelineStateDesc.StreamOutput

	//Rasterazer
	gPipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	gPipelineStateDesc.RasterizerState.FrontCounterClockwise = FALSE; // front = clock wise
	//Rasterazer-depth
	gPipelineStateDesc.RasterizerState.DepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthBiasClamp = 0;
	gPipelineStateDesc.RasterizerState.SlopeScaledDepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthClipEnable = TRUE; // if depth > 1, cliping vertex.
	//Rasterazer-MSAA
	gPipelineStateDesc.RasterizerState.MultisampleEnable = TRUE;
	gPipelineStateDesc.RasterizerState.AntialiasedLineEnable = TRUE;
	gPipelineStateDesc.RasterizerState.ForcedSampleCount = 0; // sample count of UAV rendering // question 003 : what is UAV rendering?
	//Rasterazer - Conservative Rendering On/Off - (bosujuk rendering)
	gPipelineStateDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	//Pixel Shader
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/DefaultShader.hlsl", "PSMain", "ps_5_1", &pd3dPixelShaderBlob);

	//Output Merger
	//Output Merger-Blend
	D3D12_BLEND_DESC d3dBlendDesc;
	::ZeroMemory(&d3dBlendDesc, sizeof(D3D12_BLEND_DESC));
	d3dBlendDesc.AlphaToCoverageEnable = FALSE;
	d3dBlendDesc.IndependentBlendEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	d3dBlendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	d3dBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	gPipelineStateDesc.BlendState = d3dBlendDesc;
	//Output Merger - MSAA
	gPipelineStateDesc.SampleDesc.Count = (gd.m_bMsaa4xEnable) ? 4 : 1;
	gPipelineStateDesc.SampleDesc.Quality = (gd.m_bMsaa4xEnable) ? (gd.m_nMsaa4xQualityLevels - 1) : 0;
	gPipelineStateDesc.SampleMask = 0xFFFFFFFF; // pass every sampling.
	//Output Merger - DepthStencil
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc; // D3D12_DEPTH_STENCIL_DESC1 is using for Stream Pipeline state.. -> what is that?
	//Output Merger - DepthStencil - depth
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	//depthStencilDesc.DepthBoundsTestEnable = FALSE; // question 004 : what is this??
	//Output Merger - DepthStencil - stencil
	depthStencilDesc.StencilEnable = FALSE;
	depthStencilDesc.StencilReadMask = 0x00;
	depthStencilDesc.StencilWriteMask = 0x00;
	depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	depthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	gPipelineStateDesc.DepthStencilState = depthStencilDesc;
	//Output Merger - RenderTagets / DepthStencil Buffer
	gPipelineStateDesc.NumRenderTargets = 1;
	gPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	gd.pDevice->CreateGraphicsPipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState);
	if (pd3dVertexShaderBlob) pd3dVertexShaderBlob->Release();
	if (pd3dPixelShaderBlob) pd3dPixelShaderBlob->Release();
}

void Shader::Add_RegisterShaderCommand(GPUCmd& cmd, ShaderType reg)
{
	switch (reg) {
	case ShaderType::RenderNormal:
		cmd->SetPipelineState(pPipelineState);
		cmd->SetGraphicsRootSignature(pRootSignature);
		return;
	case ShaderType::RenderWithShadow:
		cmd->SetPipelineState(pPipelineState_withShadow);
		cmd->SetGraphicsRootSignature(pRootSignature_withShadow);
		return;
	case ShaderType::RenderShadowMap:
		cmd->SetPipelineState(pPipelineState_RenderShadowMap);
		cmd->SetGraphicsRootSignature(pRootSignature);
		return;
	}
}

D3D12_SHADER_BYTECODE Shader::GetShaderByteCode(const WCHAR* pszFileName, LPCSTR pszShaderName, LPCSTR pszShaderProfile, ID3DBlob** ppd3dShaderBlob, vector<D3D_SHADER_MACRO>* macros)
{
	UINT nCompileFlags = 0;
#if defined(_DEBUG)
	nCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

#ifdef DEVELOPMODE_PIX_DEBUGING
	nCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif // !PIX_DEBUGING

	ID3DBlob* CompileOutput;
	D3D_SHADER_MACRO* macroset = nullptr;
	if (macros) {
		macroset = macros->data();
	}
	HRESULT hr = D3DCompileFromFile(pszFileName, macroset, NULL, pszShaderName, pszShaderProfile, nCompileFlags, 0, ppd3dShaderBlob, &CompileOutput);
	if (FAILED(hr)) {
		OutputDebugStringA((char*)CompileOutput->GetBufferPointer());
	}
	D3D12_SHADER_BYTECODE d3dShaderByteCode;
	d3dShaderByteCode.BytecodeLength = (*ppd3dShaderBlob)->GetBufferSize();
	d3dShaderByteCode.pShaderBytecode = (*ppd3dShaderBlob)->GetBufferPointer();

	constexpr bool ShaderReflection = true;
	if constexpr (ShaderReflection)
	{
		ID3D12ShaderReflection* pReflection = nullptr;
		D3DReflect((*ppd3dShaderBlob)->GetBufferPointer(), (*ppd3dShaderBlob)->GetBufferSize(), IID_ID3D12ShaderReflection, (void**)&pReflection);

		D3D12_SHADER_DESC shaderDesc;
		pReflection->GetDesc(&shaderDesc);

		for (UINT i = 0; i < shaderDesc.BoundResources; ++i) {
			D3D12_SHADER_INPUT_BIND_DESC bindDesc;
			pReflection->GetResourceBindingDesc(i, &bindDesc);
			dbglog2(L"Type:%d - register(%d)\n", bindDesc.Type, bindDesc.BindPoint);
			// bindDesc.Name, bindDesc.Type, bindDesc.BindPoint 등으로 RootParameter 구성 가능
		}
	}

	return(d3dShaderByteCode);
}
#pragma endregion

#pragma region OnlyColorShaderCode
OnlyColorShader::OnlyColorShader()
{

}

OnlyColorShader::~OnlyColorShader()
{
}

void OnlyColorShader::InitShader()
{
	CreateRootSignature();
	CreatePipelineState();
	CreatePipelineState_OBBWireFrames();
}

void OnlyColorShader::CreateRootSignature()
{
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;

	D3D12_ROOT_PARAMETER1 rootParam[Normal_RootParamCapacity] = {};

	rootParam[Const_Camera].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[Const_Camera].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParam[Const_Camera].Constants.Num32BitValues = 16;
	rootParam[Const_Camera].Constants.ShaderRegister = 0; // b0 : Camera Matrix (Projection, View)
	rootParam[Const_Camera].Constants.RegisterSpace = 0;

	rootParam[Const_Transform].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[Const_Transform].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParam[Const_Transform].Constants.Num32BitValues = 16;
	rootParam[Const_Transform].Constants.ShaderRegister = 1; // b1 : Transform Matrix
	rootParam[Const_Transform].Constants.RegisterSpace = 0;

	D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	rootSigDesc1.NumParameters = 2;
	rootSigDesc1.pParameters = rootParam;
	rootSigDesc1.NumStaticSamplers = 0; // question002 : what is static samplers?
	rootSigDesc1.pStaticSamplers = NULL; //
	rootSigDesc1.Flags = d3dRootSignatureFlags;

	ID3DBlob* pd3dSignatureBlob = NULL;
	ID3DBlob* pd3dErrorBlob = NULL;
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_rootSigDesc;
	versioned_rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	versioned_rootSigDesc.Desc_1_1 = rootSigDesc1;
	D3D12SerializeVersionedRootSignature(&versioned_rootSigDesc, &pd3dSignatureBlob, &pd3dErrorBlob);
	gd.pDevice->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(),
		pd3dSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void
			**)&pRootSignature);
	if (pd3dErrorBlob) pd3dErrorBlob->Release();
	if (pd3dSignatureBlob) pd3dSignatureBlob->Release();
}

void OnlyColorShader::CreatePipelineState()
{
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dVertexShaderBlob = NULL, * pd3dPixelShaderBlob = NULL;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature;

	gPipelineStateDesc.NodeMask = 0;

	//Input Asm
	gPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[3] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // color vec4
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // normal vec3
	};
	gPipelineStateDesc.InputLayout.NumElements = 3;
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/OnlyColorShader.hlsl", "VSMain", "vs_5_1", &pd3dVertexShaderBlob);

	//Hull Shader
	//gPipelineStateDesc.HS = NULLCODE;

	//Tessellation

	//Domain Shader
	//gPipelineStateDesc.DS = NULLCODE;

	//Geometry Shader
	//gPipelineStateDesc.GS = NULLCODE;

	//Stream Output
	//gPipelineStateDesc.StreamOutput

	//Rasterazer
	gPipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	gPipelineStateDesc.RasterizerState.FrontCounterClockwise = FALSE; // front = clock wise
	//Rasterazer-depth
	gPipelineStateDesc.RasterizerState.DepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthBiasClamp = 0;
	gPipelineStateDesc.RasterizerState.SlopeScaledDepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthClipEnable = TRUE; // if depth > 1, cliping vertex.
	//Rasterazer-MSAA
	gPipelineStateDesc.RasterizerState.MultisampleEnable = TRUE;
	gPipelineStateDesc.RasterizerState.AntialiasedLineEnable = TRUE;
	gPipelineStateDesc.RasterizerState.ForcedSampleCount = 0; // sample count of UAV rendering // question 003 : what is UAV rendering?
	//Rasterazer - Conservative Rendering On/Off - (bosujuk rendering)
	gPipelineStateDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	//Pixel Shader
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/OnlyColorShader.hlsl", "PSMain", "ps_5_1", &pd3dPixelShaderBlob);

	//Output Merger
	//Output Merger-Blend
	D3D12_BLEND_DESC d3dBlendDesc;
	::ZeroMemory(&d3dBlendDesc, sizeof(D3D12_BLEND_DESC));
	d3dBlendDesc.AlphaToCoverageEnable = FALSE;
	d3dBlendDesc.IndependentBlendEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	d3dBlendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	d3dBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	gPipelineStateDesc.BlendState = d3dBlendDesc;
	//Output Merger - MSAA
	gPipelineStateDesc.SampleDesc.Count = (gd.m_bMsaa4xEnable) ? 4 : 1;
	gPipelineStateDesc.SampleDesc.Quality = (gd.m_bMsaa4xEnable) ? (gd.m_nMsaa4xQualityLevels - 1) : 0;
	gPipelineStateDesc.SampleMask = 0xFFFFFFFF; // pass every sampling.
	//Output Merger - DepthStencil
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc; // D3D12_DEPTH_STENCIL_DESC1 is using for Stream Pipeline state.. -> what is that?
	//Output Merger - DepthStencil - depth
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	//depthStencilDesc.DepthBoundsTestEnable = FALSE; // question 004 : what is this??
	//Output Merger - DepthStencil - stencil
	depthStencilDesc.StencilEnable = FALSE;
	depthStencilDesc.StencilReadMask = 0x00;
	depthStencilDesc.StencilWriteMask = 0x00;
	depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	depthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	gPipelineStateDesc.DepthStencilState = depthStencilDesc;
	//Output Merger - RenderTagets / DepthStencil Buffer
	gPipelineStateDesc.NumRenderTargets = 1;
	gPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	gd.pDevice->CreateGraphicsPipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState);
	if (pd3dVertexShaderBlob) pd3dVertexShaderBlob->Release();
	if (pd3dPixelShaderBlob) pd3dPixelShaderBlob->Release();
}

void OnlyColorShader::CreatePipelineState_OBBWireFrames() {
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dVertexShaderBlob = NULL, * pd3dPixelShaderBlob = NULL;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature;

	gPipelineStateDesc.NodeMask = 0;

	//Input Asm
	gPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[3] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // color vec4
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // normal vec3
	};
	gPipelineStateDesc.InputLayout.NumElements = 3;
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/OnlyColorShader.hlsl", "VSMain", "vs_5_1", &pd3dVertexShaderBlob);

	//Hull Shader
	//gPipelineStateDesc.HS = NULLCODE;

	//Tessellation

	//Domain Shader
	//gPipelineStateDesc.DS = NULLCODE;

	//Geometry Shader
	//gPipelineStateDesc.GS = NULLCODE;

	//Stream Output
	//gPipelineStateDesc.StreamOutput

	//Rasterazer
	gPipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	gPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	gPipelineStateDesc.RasterizerState.FrontCounterClockwise = FALSE; // front = clock wise
	//Rasterazer-depth
	gPipelineStateDesc.RasterizerState.DepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthBiasClamp = 0;
	gPipelineStateDesc.RasterizerState.SlopeScaledDepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthClipEnable = TRUE; // if depth > 1, cliping vertex.
	//Rasterazer-MSAA
	gPipelineStateDesc.RasterizerState.MultisampleEnable = TRUE;
	gPipelineStateDesc.RasterizerState.AntialiasedLineEnable = TRUE;
	gPipelineStateDesc.RasterizerState.ForcedSampleCount = 0; // sample count of UAV rendering // question 003 : what is UAV rendering?
	//Rasterazer - Conservative Rendering On/Off - (bosujuk rendering)
	gPipelineStateDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	//Pixel Shader
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/OnlyColorShader.hlsl", "PSMain", "ps_5_1", &pd3dPixelShaderBlob);

	//Output Merger
	//Output Merger-Blend
	D3D12_BLEND_DESC d3dBlendDesc;
	::ZeroMemory(&d3dBlendDesc, sizeof(D3D12_BLEND_DESC));
	d3dBlendDesc.AlphaToCoverageEnable = FALSE;
	d3dBlendDesc.IndependentBlendEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	d3dBlendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	d3dBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	gPipelineStateDesc.BlendState = d3dBlendDesc;
	//Output Merger - MSAA
	gPipelineStateDesc.SampleDesc.Count = (gd.m_bMsaa4xEnable) ? 4 : 1;
	gPipelineStateDesc.SampleDesc.Quality = (gd.m_bMsaa4xEnable) ? (gd.m_nMsaa4xQualityLevels - 1) : 0;
	gPipelineStateDesc.SampleMask = 0xFFFFFFFF; // pass every sampling.
	//Output Merger - DepthStencil
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc; // D3D12_DEPTH_STENCIL_DESC1 is using for Stream Pipeline state.. -> what is that?
	//Output Merger - DepthStencil - depth
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	//depthStencilDesc.DepthBoundsTestEnable = FALSE; // question 004 : what is this??
	//Output Merger - DepthStencil - stencil
	depthStencilDesc.StencilEnable = FALSE;
	depthStencilDesc.StencilReadMask = 0x00;
	depthStencilDesc.StencilWriteMask = 0x00;
	depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	depthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	gPipelineStateDesc.DepthStencilState = depthStencilDesc;
	//Output Merger - RenderTagets / DepthStencil Buffer
	gPipelineStateDesc.NumRenderTargets = 1;
	gPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	gd.pDevice->CreateGraphicsPipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState_OBBWireFrames);
	if (pd3dVertexShaderBlob) pd3dVertexShaderBlob->Release();
	if (pd3dPixelShaderBlob) pd3dPixelShaderBlob->Release();
}

void OnlyColorShader::Add_RegisterShaderCommand(GPUCmd & cmd, ShaderType reg) {
	if (reg == ShaderType::Debug_OBB) {
		cmd->SetGraphicsRootSignature(pRootSignature);
		cmd->SetPipelineState(pPipelineState_OBBWireFrames);
	}
	else {
		cmd->SetGraphicsRootSignature(pRootSignature);
		cmd->SetPipelineState(pPipelineState);
	}
}
#pragma endregion

#pragma region ScreenShaderCode
ScreenCharactorShader::ScreenCharactorShader()
{
}

ScreenCharactorShader::~ScreenCharactorShader()
{
}

void ScreenCharactorShader::InitShader()
{
	CreateRootSignature();
	CreatePipelineState();
	CreateRootSignature_SDF();
	CreatePipelineState_SDF();

	UINT ncbElementBytes = (((sizeof(SDFInstance) * MaxInstance) + 255) & ~255); //256의 배수
	SDFInstance_StructuredBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1);
	
	gd.ShaderVisibleDescPool.ImmortalAlloc(&SDFInstance_SRV, 1);
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	srvDesc.Buffer.NumElements = 16384;
	srvDesc.Buffer.StructureByteStride = sizeof(SDFInstance);
	gd.pDevice->CreateShaderResourceView(SDFInstance_StructuredBuffer.resource, &srvDesc, SDFInstance_SRV.hCreation.hcpu);
}

void ScreenCharactorShader::CreateRootSignature()
{
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;

	D3D12_ROOT_PARAMETER1 rootParam[2] = {};

	rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParam[0].Constants.Num32BitValues = 11; // rect(4) + mulcolor(4) + screenWidht, screenHeight, depth
	rootParam[0].Constants.ShaderRegister = 0; // b0
	rootParam[0].Constants.RegisterSpace = 0;

	rootParam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParam[1].DescriptorTable.NumDescriptorRanges = 1;
	D3D12_DESCRIPTOR_RANGE1 ranges[1];
	ranges[0].BaseShaderRegister = 0; // t0 //Diffuse Texture
	ranges[0].RegisterSpace = 0;
	ranges[0].NumDescriptors = 1;
	ranges[0].OffsetInDescriptorsFromTableStart = 0;
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE |
		D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
	rootParam[1].DescriptorTable.pDescriptorRanges = &ranges[0];

	D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT/* |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS*/;

		/// default sampler
	D3D12_STATIC_SAMPLER_DESC sampler = {};
	// sampler register index.
	sampler.ShaderRegister = 0;
	// setting
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.MipLODBias = 0.0f;
	sampler.MaxAnisotropy = 16;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	sampler.MinLOD = -FLT_MAX;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

	rootSigDesc1.NumParameters = 2;
	rootSigDesc1.pParameters = rootParam;
	rootSigDesc1.NumStaticSamplers = 1; // question002 : what is static samplers?
	rootSigDesc1.pStaticSamplers = &sampler;
	rootSigDesc1.Flags = d3dRootSignatureFlags;

	ID3DBlob* pd3dSignatureBlob = NULL;
	ID3DBlob* pd3dErrorBlob = NULL;
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_rootSigDesc;
	versioned_rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	versioned_rootSigDesc.Desc_1_1 = rootSigDesc1;
	D3D12SerializeVersionedRootSignature(&versioned_rootSigDesc, &pd3dSignatureBlob, &pd3dErrorBlob);
	gd.pDevice->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(),
		pd3dSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void
			**)&pRootSignature);
	if (pd3dErrorBlob) pd3dErrorBlob->Release();
	if (pd3dSignatureBlob) pd3dSignatureBlob->Release();
}

void ScreenCharactorShader::CreatePipelineState()
{
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dVertexShaderBlob = NULL, * pd3dPixelShaderBlob = NULL;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature;

	gPipelineStateDesc.NodeMask = 0;

	//Input Asm
	gPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[4] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // color vec4
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // normal vec3
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0} // uv vec2
	};
	gPipelineStateDesc.InputLayout.NumElements = 4;
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/ScreenCharactorRenderShader.hlsl", "VSMain", "vs_5_1", &pd3dVertexShaderBlob);

	ID3D12ShaderReflection* pReflection = nullptr;
	D3DReflect(gPipelineStateDesc.VS.pShaderBytecode, gPipelineStateDesc.VS.BytecodeLength, IID_PPV_ARGS(&pReflection));

	D3D12_SHADER_DESC shaderDesc;
	pReflection->GetDesc(&shaderDesc);

	for (UINT i = 0; i < shaderDesc.ConstantBuffers; ++i)
	{
		ID3D12ShaderReflectionConstantBuffer* cb = pReflection->GetConstantBufferByIndex(i);
		D3D12_SHADER_BUFFER_DESC cbDesc;
		cb->GetDesc(&cbDesc);
		std::cout << "CB Name: " << cbDesc.Name << endl;
	}

	//Hull Shader
	//gPipelineStateDesc.HS = NULLCODE;

	//Tessellation

	//Domain Shader
	//gPipelineStateDesc.DS = NULLCODE;

	//Geometry Shader
	//gPipelineStateDesc.GS = NULLCODE;

	//Stream Output
	//gPipelineStateDesc.StreamOutput

	//Rasterazer
	gPipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	gPipelineStateDesc.RasterizerState.FrontCounterClockwise = FALSE; // front = clock wise
	//Rasterazer-depth
	gPipelineStateDesc.RasterizerState.DepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthBiasClamp = 0;
	gPipelineStateDesc.RasterizerState.SlopeScaledDepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthClipEnable = TRUE; // if depth > 1, cliping vertex.
	//Rasterazer-MSAA
	gPipelineStateDesc.RasterizerState.MultisampleEnable = TRUE;
	gPipelineStateDesc.RasterizerState.AntialiasedLineEnable = TRUE;
	gPipelineStateDesc.RasterizerState.ForcedSampleCount = 0; // sample count of UAV rendering // question 003 : what is UAV rendering?
	//Rasterazer - Conservative Rendering On/Off - (bosujuk rendering)
	gPipelineStateDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	//Pixel Shader
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/ScreenCharactorRenderShader.hlsl", "PSMain", "ps_5_1", &pd3dPixelShaderBlob);

	//Output Merger
	//Output Merger-Blend
	D3D12_BLEND_DESC d3dBlendDesc;
	::ZeroMemory(&d3dBlendDesc, sizeof(D3D12_BLEND_DESC));
	d3dBlendDesc.AlphaToCoverageEnable = 0;
	d3dBlendDesc.IndependentBlendEnable = 0;
	d3dBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	d3dBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	d3dBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	d3dBlendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	d3dBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	d3dBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	gPipelineStateDesc.BlendState = d3dBlendDesc;
	//Output Merger - MSAA
	gPipelineStateDesc.SampleDesc.Count = (gd.m_bMsaa4xEnable) ? 4 : 1;
	gPipelineStateDesc.SampleDesc.Quality = (gd.m_bMsaa4xEnable) ? (gd.m_nMsaa4xQualityLevels - 1) : 0;
	gPipelineStateDesc.SampleMask = 0xFFFFFFFF; // pass every sampling.
	//Output Merger - DepthStencil
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc; // D3D12_DEPTH_STENCIL_DESC1 is using for Stream Pipeline state.. -> what is that?
	//Output Merger - DepthStencil - depth
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	//depthStencilDesc.DepthBoundsTestEnable = FALSE; // question 004 : what is this??
	//Output Merger - DepthStencil - stencil
	depthStencilDesc.StencilEnable = FALSE;
	depthStencilDesc.StencilReadMask = 0x00;
	depthStencilDesc.StencilWriteMask = 0x00;
	depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	depthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	gPipelineStateDesc.DepthStencilState = depthStencilDesc;
	//Output Merger - RenderTagets / DepthStencil Buffer
	gPipelineStateDesc.NumRenderTargets = 1;
	gPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	gd.pDevice->CreateGraphicsPipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState);
	if (pd3dVertexShaderBlob) pd3dVertexShaderBlob->Release();
	if (pd3dPixelShaderBlob) pd3dPixelShaderBlob->Release();
}

void ScreenCharactorShader::CreateRootSignature_SDF() {
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;

	D3D12_ROOT_PARAMETER1 rootParam[RootParamId::Normal_RootParamCapacity] = {};

	// rect(4) + screenWidht, screenHeight  + depth + pad + Color4 + minD + maxD
	rootParam[RootParamId::Const_BasicInfo] = RootParam1::Const32s(GRegID('b', 0), 2, D3D12_SHADER_VISIBILITY_ALL);

	RootParam1 DescTable0;
	DescTable0.PushDescRange(GRegID('t', 0), "SRV", 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	DescTable0.DescTable(D3D12_SHADER_VISIBILITY_ALL);
	rootParam[RootParamId::SRVTable_SDFInstance] = DescTable0;

	RootParam1 DescTable1;
	DescTable1.PushDescRange(GRegID('t', 1), "SRV", 16, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	DescTable1.DescTable(D3D12_SHADER_VISIBILITY_PIXEL);
	rootParam[RootParamId::SRVTable_Texture] = DescTable1;

	D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT/* |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS*/;

		/// default sampler
	D3D12_STATIC_SAMPLER_DESC sampler = {};
	// sampler register index.
	sampler.ShaderRegister = 0;
	// setting
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.MipLODBias = 0.0f;
	sampler.MaxAnisotropy = 16;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	sampler.MinLOD = -FLT_MAX;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

	rootSigDesc1.NumParameters = Normal_RootParamCapacity;
	rootSigDesc1.pParameters = rootParam;
	rootSigDesc1.NumStaticSamplers = 1; // question002 : what is static samplers?
	rootSigDesc1.pStaticSamplers = &sampler;
	rootSigDesc1.Flags = d3dRootSignatureFlags;

	ID3DBlob* pd3dSignatureBlob = NULL;
	ID3DBlob* pd3dErrorBlob = NULL;
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_rootSigDesc;
	versioned_rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	versioned_rootSigDesc.Desc_1_1 = rootSigDesc1;
	D3D12SerializeVersionedRootSignature(&versioned_rootSigDesc, &pd3dSignatureBlob, &pd3dErrorBlob);
	gd.pDevice->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(),
		pd3dSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void
			**)&pRootSignature_SDF);
	if (pd3dErrorBlob) pd3dErrorBlob->Release();
	if (pd3dSignatureBlob) pd3dSignatureBlob->Release();
}
void ScreenCharactorShader::CreatePipelineState_SDF() {
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dVertexShaderBlob = NULL, * pd3dPixelShaderBlob = NULL;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature_SDF;

	gPipelineStateDesc.NodeMask = 0;

	//Input Asm
	gPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[4] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // color vec4
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // normal vec3
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0} // uv vec2
	};
	gPipelineStateDesc.InputLayout.NumElements = 4;
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/SDF/SDFTextShader.hlsl", "VSMain", "vs_5_1", &pd3dVertexShaderBlob);

	ID3D12ShaderReflection* pReflection = nullptr;
	D3DReflect(gPipelineStateDesc.VS.pShaderBytecode, gPipelineStateDesc.VS.BytecodeLength, IID_PPV_ARGS(&pReflection));

	D3D12_SHADER_DESC shaderDesc;
	pReflection->GetDesc(&shaderDesc);

	for (UINT i = 0; i < shaderDesc.ConstantBuffers; ++i)
	{
		ID3D12ShaderReflectionConstantBuffer* cb = pReflection->GetConstantBufferByIndex(i);
		D3D12_SHADER_BUFFER_DESC cbDesc;
		cb->GetDesc(&cbDesc);
		std::cout << "CB Name: " << cbDesc.Name << endl;
	}

	//Hull Shader
	//gPipelineStateDesc.HS = NULLCODE;

	//Tessellation

	//Domain Shader
	//gPipelineStateDesc.DS = NULLCODE;

	//Geometry Shader
	//gPipelineStateDesc.GS = NULLCODE;

	//Stream Output
	//gPipelineStateDesc.StreamOutput

	//Rasterazer
	gPipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	gPipelineStateDesc.RasterizerState.FrontCounterClockwise = FALSE; // front = clock wise
	//Rasterazer-depth
	gPipelineStateDesc.RasterizerState.DepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthBiasClamp = 0;
	gPipelineStateDesc.RasterizerState.SlopeScaledDepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthClipEnable = TRUE; // if depth > 1, cliping vertex.
	//Rasterazer-MSAA
	gPipelineStateDesc.RasterizerState.MultisampleEnable = TRUE;
	gPipelineStateDesc.RasterizerState.AntialiasedLineEnable = TRUE;
	gPipelineStateDesc.RasterizerState.ForcedSampleCount = 0; // sample count of UAV rendering // question 003 : what is UAV rendering?
	//Rasterazer - Conservative Rendering On/Off - (bosujuk rendering)
	gPipelineStateDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	//Pixel Shader
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/SDF/SDFTextShader.hlsl", "PSMain", "ps_5_1", &pd3dPixelShaderBlob);

	//Output Merger
	//Output Merger-Blend
	D3D12_BLEND_DESC d3dBlendDesc;
	::ZeroMemory(&d3dBlendDesc, sizeof(D3D12_BLEND_DESC));
	d3dBlendDesc.AlphaToCoverageEnable = 0;
	d3dBlendDesc.IndependentBlendEnable = 0;
	d3dBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	d3dBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	d3dBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	d3dBlendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	d3dBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	d3dBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	gPipelineStateDesc.BlendState = d3dBlendDesc;
	//Output Merger - MSAA
	gPipelineStateDesc.SampleDesc.Count = (gd.m_bMsaa4xEnable) ? 4 : 1;
	gPipelineStateDesc.SampleDesc.Quality = (gd.m_bMsaa4xEnable) ? (gd.m_nMsaa4xQualityLevels - 1) : 0;
	gPipelineStateDesc.SampleMask = 0xFFFFFFFF; // pass every sampling.
	//Output Merger - DepthStencil
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc; // D3D12_DEPTH_STENCIL_DESC1 is using for Stream Pipeline state.. -> what is that?
	//Output Merger - DepthStencil - depth
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	//depthStencilDesc.DepthBoundsTestEnable = FALSE; // question 004 : what is this??
	//Output Merger - DepthStencil - stencil
	depthStencilDesc.StencilEnable = FALSE;
	depthStencilDesc.StencilReadMask = 0x00;
	depthStencilDesc.StencilWriteMask = 0x00;
	depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	depthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	gPipelineStateDesc.DepthStencilState = depthStencilDesc;
	//Output Merger - RenderTagets / DepthStencil Buffer
	gPipelineStateDesc.NumRenderTargets = 1;
	gPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	gd.pDevice->CreateGraphicsPipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState_SDF);
	if (pd3dVertexShaderBlob) pd3dVertexShaderBlob->Release();
	if (pd3dPixelShaderBlob) pd3dPixelShaderBlob->Release();
}

void ScreenCharactorShader::Release()
{
	if (pPipelineState) pPipelineState->Release();
	if (pRootSignature) pRootSignature->Release();
}

void ScreenCharactorShader::SetTextureCommand(GPUResource* texture)
{
	DescHandle descH;
	gd.ShaderVisibleDescPool.DynamicAlloc(&descH, 1);

	gd.pDevice->CopyDescriptorsSimple(1, descH.hcpu, texture->descindex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	gd.gpucmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
	gd.gpucmd->SetGraphicsRootDescriptorTable(1, descH.hgpu);
}

void ScreenCharactorShader::Add_RegisterShaderCommand(GPUCmd& cmd, ShaderType reg) {
	if (reg == ShaderType::SDF) {
		cmd->SetGraphicsRootSignature(pRootSignature_SDF);
		cmd->SetPipelineState(pPipelineState_SDF);
	}
	else {
		cmd->SetGraphicsRootSignature(pRootSignature);
		cmd->SetPipelineState(pPipelineState);
	}
}

void ScreenCharactorShader::RenderAllSDFTexts() {
	game.MyScreenCharactorShader->SDFInstance_StructuredBuffer.resource->Unmap(0, nullptr);

	using SCSRP = ScreenCharactorShader::RootParamId;
	gd.gpucmd.SetShader(game.MyScreenCharactorShader, ShaderType::SDF);
	gd.gpucmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
	vec4 wh = vec4(gd.ClientFrameWidth, gd.ClientFrameHeight, 0, 0);
	gd.gpucmd->SetGraphicsRoot32BitConstants(SCSRP::Const_BasicInfo, 2, &wh, 0);
	gd.gpucmd->SetGraphicsRootDescriptorTable(SRVTable_SDFInstance, SDFInstance_SRV.hRender.hgpu);

	DescHandle descH;
	gd.ShaderVisibleDescPool.DynamicAlloc(&descH, 16);
	for (int i = 0;i < gd.SDFTextureArr.size();++i) {
		gd.pDevice->CopyDescriptorsSimple(1, descH[i].hcpu, gd.SDFTextureArr[i]->SDFTextureSRV.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
	for (int i = gd.SDFTextureArr.size();i < 16;++i) {
		gd.pDevice->CopyDescriptorsSimple(1, descH[i].hcpu, gd.SDFTextureArr[0]->SDFTextureSRV.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
	gd.gpucmd->SetGraphicsRootDescriptorTable(SRVTable_Texture, descH.hgpu);

	game.TextMesh->Render(gd.gpucmd, SDFInstanceCount);
}

#pragma endregion

#pragma region PBRShaderCode
void PBRShader1::InitShader()
{
	CreateRootSignature();
	CreateRootSignature_withShadow();
	CreateRootSignature_SkinedMesh();
	CreateRootSignature_Instancing_withShadow();

	CreatePipelineState();
	CreatePipelineState_withShadow();
	CreatePipelineState_RenderShadowMap();
	CreatePipelineState_RenderStencil();
	CreatePipelineState_InnerMirror();
	CreatePipelineState_SkinedMesh();
	CreatePipelineState_Instancing_withShadow();
}

void PBRShader1::CreateRootSignature()
{
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;
	D3D12_ROOT_PARAMETER1 rootParam[RootParamId::Normal_RootParamCapacity] = {};

	rootParam[RootParamId::Const_Camera] =
		RootParam1::Const32s(GRegID('b', 0), 20, D3D12_SHADER_VISIBILITY_VERTEX);
	rootParam[RootParamId::Const_Transform] =
		RootParam1::Const32s(GRegID('b', 1), 16, D3D12_SHADER_VISIBILITY_VERTEX);
	rootParam[RootParamId::CBV_StaticLight] =
		RootParam1::CBV(GRegID('b', 2), D3D12_SHADER_VISIBILITY_PIXEL, D3D12_ROOT_DESCRIPTOR_FLAG_NONE);
	RootParam1 DescTable1;
	DescTable1.PushDescRange(GRegID('t', 0), "SRV", 5, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	DescTable1.DescTable(D3D12_SHADER_VISIBILITY_PIXEL);
	rootParam[RootParamId::SRVTable_MaterialTextures] = DescTable1;
	RootParam1 DescTable2;
	DescTable2.PushDescRange(GRegID('b', 3), "CBV", 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	DescTable2.DescTable(D3D12_SHADER_VISIBILITY_ALL);
	rootParam[RootParamId::CBVTable_Material] = DescTable2;

	D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT/* |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS*/;

		/// default sampler
	D3D12_STATIC_SAMPLER_DESC sampler = {};
	// sampler register index.
	sampler.ShaderRegister = 0;
	// setting
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.MipLODBias = 0.0f;
	sampler.MaxAnisotropy = 16;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	sampler.MinLOD = -FLT_MAX;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

	rootSigDesc1.NumParameters = sizeof(rootParam) / sizeof(D3D12_ROOT_PARAMETER1);;
	rootSigDesc1.pParameters = rootParam;
	rootSigDesc1.NumStaticSamplers = 1; // question002 : what is static samplers?
	rootSigDesc1.pStaticSamplers = &sampler;
	rootSigDesc1.Flags = d3dRootSignatureFlags;

	ID3DBlob* pd3dSignatureBlob = NULL;
	ID3DBlob* pd3dErrorBlob = NULL;
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_rootSigDesc;
	versioned_rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	versioned_rootSigDesc.Desc_1_1 = rootSigDesc1;
	D3D12SerializeVersionedRootSignature(&versioned_rootSigDesc, &pd3dSignatureBlob, &pd3dErrorBlob);
	gd.pDevice->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(),
		pd3dSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void
			**)&pRootSignature);
	if (pd3dErrorBlob) pd3dErrorBlob->Release();
	if (pd3dSignatureBlob) pd3dSignatureBlob->Release();
}

void PBRShader1::CreateRootSignature_withShadow()
{
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;

	D3D12_ROOT_PARAMETER1 rootParam[RootParamId::withShaow_RootParamCapacity] = {};

	rootParam[RootParamId::Const_Camera] =
		RootParam1::Const32s(GRegID('b', 0), 20, D3D12_SHADER_VISIBILITY_ALL);
	rootParam[RootParamId::Const_Transform] =
		RootParam1::Const32s(GRegID('b', 1), 16, D3D12_SHADER_VISIBILITY_VERTEX);
	rootParam[RootParamId::CBV_StaticLight] =
		RootParam1::CBV(GRegID('b', 2), D3D12_SHADER_VISIBILITY_PIXEL, D3D12_ROOT_DESCRIPTOR_FLAG_NONE);
	RootParam1 DescTable1;
	DescTable1.PushDescRange(GRegID('t', 0), "SRV", 5, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	DescTable1.DescTable(D3D12_SHADER_VISIBILITY_PIXEL);
	rootParam[RootParamId::SRVTable_MaterialTextures] = DescTable1;
	RootParam1 DescTable2;
	DescTable2.PushDescRange(GRegID('b', 3), "CBV", 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	DescTable2.DescTable(D3D12_SHADER_VISIBILITY_ALL);
	rootParam[RootParamId::CBVTable_Material] = DescTable2;
	RootParam1 DescTable3;
	//direction Light ShadowMap
	DescTable3.PushDescRange(GRegID('t', 5), "SRV", 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	DescTable3.DescTable(D3D12_SHADER_VISIBILITY_PIXEL);
	rootParam[RootParamId::SRVTable_ShadowMap] = DescTable3;
	//PointLight/SpotLight ShadowMap


	D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT/* |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS*/;

		/// default sampler
	D3D12_STATIC_SAMPLER_DESC sampler = {};
	// sampler register index.
	sampler.ShaderRegister = 0;
	// setting
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.MipLODBias = 0.0f;
	sampler.MaxAnisotropy = 16;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	sampler.MinLOD = 0;
	sampler.MaxLOD = 20;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	rootSigDesc1.NumParameters = sizeof(rootParam) / sizeof(D3D12_ROOT_PARAMETER1);
	rootSigDesc1.pParameters = rootParam;
	rootSigDesc1.NumStaticSamplers = 1; // question002 : what is static samplers?
	rootSigDesc1.pStaticSamplers = &sampler;
	rootSigDesc1.Flags = d3dRootSignatureFlags;

	ID3DBlob* pd3dSignatureBlob = NULL;
	ID3DBlob* pd3dErrorBlob = NULL;
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_rootSigDesc;
	versioned_rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	versioned_rootSigDesc.Desc_1_1 = rootSigDesc1;
	D3D12SerializeVersionedRootSignature(&versioned_rootSigDesc, &pd3dSignatureBlob, &pd3dErrorBlob);
	gd.pDevice->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(),
		pd3dSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void
			**)&pRootSignature_withShadow);
	if (pd3dErrorBlob) pd3dErrorBlob->Release();
	if (pd3dSignatureBlob) pd3dSignatureBlob->Release();
}

void PBRShader1::CreateRootSignature_SkinedMesh()
{
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;

	D3D12_ROOT_PARAMETER1 rootParam[RootParamId::withSkinMeshShaow_RootParamCapacity] = {};

	rootParam[RootParamId::Const_Camera] =
		RootParam1::Const32s(GRegID('b', 0), 20, D3D12_SHADER_VISIBILITY_ALL);

	RootParam1 DescTableOffsetMatrix;
	DescTableOffsetMatrix.PushDescRange(GRegID('b', 1), "CBV", 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	DescTableOffsetMatrix.DescTable(D3D12_SHADER_VISIBILITY_VERTEX);
	rootParam[RootParamId::CBVTable_SkinMeshOffsetMatrix] = DescTableOffsetMatrix;

	RootParam1 DescTableToWorldMatrix;
	DescTableToWorldMatrix.PushDescRange(GRegID('b', 2), "CBV", 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
	DescTableToWorldMatrix.DescTable(D3D12_SHADER_VISIBILITY_VERTEX);
	rootParam[RootParamId::CBVTable_SkinMeshToWorldMatrix] = DescTableToWorldMatrix;

	RootParam1 DescTableLightData;
	DescTableLightData.PushDescRange(GRegID('b', 3), "CBV", 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
	DescTableLightData.DescTable(D3D12_SHADER_VISIBILITY_PIXEL);
	rootParam[RootParamId::CBVTable_SkinMeshLightData] = DescTableLightData;

	RootParam1 DescTableMaterialTextures;
	DescTableMaterialTextures.PushDescRange(GRegID('t', 0), "SRV", 5, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	DescTableMaterialTextures.DescTable(D3D12_SHADER_VISIBILITY_PIXEL);
	rootParam[RootParamId::SRVTable_SkinMeshMaterialTextures] = DescTableMaterialTextures;

	RootParam1 DescTableMaterial;
	DescTableMaterial.PushDescRange(GRegID('b', 4), "CBV", 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	DescTableMaterial.DescTable(D3D12_SHADER_VISIBILITY_ALL);
	rootParam[RootParamId::CBVTable_SkinMeshMaterial] = DescTableMaterial;

	RootParam1 DescTableShadowMaps;
	//direction Light ShadowMap
	DescTableShadowMaps.PushDescRange(GRegID('t', 5), "SRV", 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
	DescTableShadowMaps.DescTable(D3D12_SHADER_VISIBILITY_PIXEL);
	rootParam[RootParamId::SRVTable_SkinMeshShadowMaps] = DescTableShadowMaps;
	//PointLight/SpotLight ShadowMap


	D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT/* |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS*/;

		/// default sampler
	D3D12_STATIC_SAMPLER_DESC sampler = {};
	// sampler register index.
	sampler.ShaderRegister = 0;
	// setting
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.MipLODBias = 0.0f;
	sampler.MaxAnisotropy = 16;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	sampler.MinLOD = 0;
	sampler.MaxLOD = 20;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	rootSigDesc1.NumParameters = RootParamId::withSkinMeshShaow_RootParamCapacity;
	rootSigDesc1.pParameters = rootParam;
	rootSigDesc1.NumStaticSamplers = 1; // question002 : what is static samplers?
	rootSigDesc1.pStaticSamplers = &sampler;
	rootSigDesc1.Flags = d3dRootSignatureFlags;

	ID3DBlob* pd3dSignatureBlob = NULL;
	ID3DBlob* pd3dErrorBlob = NULL;
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_rootSigDesc;
	versioned_rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	versioned_rootSigDesc.Desc_1_1 = rootSigDesc1;
	D3D12SerializeVersionedRootSignature(&versioned_rootSigDesc, &pd3dSignatureBlob, &pd3dErrorBlob);
	gd.pDevice->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(),
		pd3dSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void
			**)&pRootSignature_SkinedMesh_withShadow);
	if (pd3dErrorBlob) pd3dErrorBlob->Release();
	if (pd3dSignatureBlob) pd3dSignatureBlob->Release();
}

void PBRShader1::CreateRootSignature_Instancing_withShadow()
{
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;

	D3D12_ROOT_PARAMETER1 rootParam[RootParamId::Instancing_Capacity] = {};

	rootParam[RootParamId::Const_Camera] =
		RootParam1::Const32s(GRegID('b', 0), 20, D3D12_SHADER_VISIBILITY_ALL);
	RootParam1 DescTable0;
	DescTable0.PushDescRange(GRegID('b', 1), "CBV", 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	DescTable0.DescTable(D3D12_SHADER_VISIBILITY_ALL);
	rootParam[RootParamId::CBVTable_Instancing_DirLightData] = DescTable0;
	RootParam1 DescTable1;
	DescTable1.PushDescRange(GRegID('t', 0), "SRV", 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	DescTable1.DescTable(D3D12_SHADER_VISIBILITY_ALL);
	rootParam[RootParamId::SRVTable_Instancing_RenderInstance] = DescTable1;

	RootParam1 DescTable2;
	DescTable2.PushDescRange(GRegID('t', 1), "SRV", 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	DescTable2.DescTable(D3D12_SHADER_VISIBILITY_ALL);
	rootParam[RootParamId::SRVTable_Instancing_MaterialPool] = DescTable2;
	RootParam1 DescTable3;
	DescTable3.PushDescRange(GRegID('t', 2), "SRV", 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	DescTable3.DescTable(D3D12_SHADER_VISIBILITY_PIXEL);
	rootParam[RootParamId::SRVTable_Instancing_ShadowMap] = DescTable3;
	RootParam1 DescTable4;
	DescTable4.PushDescRange(GRegID('t', 3), "SRV", gd.ShaderVisibleDescPool.TextureSRVCap, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	DescTable4.DescTable(D3D12_SHADER_VISIBILITY_PIXEL);
	rootParam[RootParamId::SRVTable_Instancing_MaterialTexturePool] = DescTable4;
	//PointLight/SpotLight ShadowMap


	D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT/* |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS*/;

		/// default sampler
	D3D12_STATIC_SAMPLER_DESC sampler = {};
	// sampler register index.
	sampler.ShaderRegister = 0;
	// setting
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.MipLODBias = 0.0f;
	sampler.MaxAnisotropy = 16;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	sampler.MinLOD = 0;
	sampler.MaxLOD = 20;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	rootSigDesc1.NumParameters = sizeof(rootParam) / sizeof(D3D12_ROOT_PARAMETER1);
	rootSigDesc1.pParameters = rootParam;
	rootSigDesc1.NumStaticSamplers = 1; // question002 : what is static samplers?
	rootSigDesc1.pStaticSamplers = &sampler;
	rootSigDesc1.Flags = d3dRootSignatureFlags;

	ID3DBlob* pd3dSignatureBlob = NULL;
	ID3DBlob* pd3dErrorBlob = NULL;
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_rootSigDesc;
	versioned_rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	versioned_rootSigDesc.Desc_1_1 = rootSigDesc1;
	D3D12SerializeVersionedRootSignature(&versioned_rootSigDesc, &pd3dSignatureBlob, &pd3dErrorBlob);
	gd.pDevice->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(),
		pd3dSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void
			**)&pRootSignature_Instancing_withShadow);
	if (pd3dErrorBlob) pd3dErrorBlob->Release();
	if (pd3dSignatureBlob) pd3dSignatureBlob->Release();
}

void PBRShader1::CreatePipelineState()
{
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dVertexShaderBlob = NULL, * pd3dPixelShaderBlob = NULL;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature;

	gPipelineStateDesc.NodeMask = 0;

	//Input Asm
	gPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[5] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},// uv vec2
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // normal vec3
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		// float3 tangent
		{ "EXTRA", 0, DXGI_FORMAT_R32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
		// float3 extra
	};
	gPipelineStateDesc.InputLayout.NumElements = sizeof(inputElementDesc) / sizeof(D3D12_INPUT_ELEMENT_DESC);
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01.hlsl", "VSMain", "vs_5_1", &pd3dVertexShaderBlob);

	//Hull Shader
	//gPipelineStateDesc.HS = NULLCODE;

	//Tessellation

	//Domain Shader
	//gPipelineStateDesc.DS = NULLCODE;

	//Geometry Shader
	//gPipelineStateDesc.GS = NULLCODE;

	//Stream Output
	//gPipelineStateDesc.StreamOutput

	//Rasterazer
	gPipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	gPipelineStateDesc.RasterizerState.FrontCounterClockwise = FALSE; // front = clock wise
	//Rasterazer-depth
	gPipelineStateDesc.RasterizerState.DepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthBiasClamp = 0;
	gPipelineStateDesc.RasterizerState.SlopeScaledDepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthClipEnable = TRUE; // if depth > 1, cliping vertex.
	//Rasterazer-MSAA
	gPipelineStateDesc.RasterizerState.MultisampleEnable = TRUE;
	gPipelineStateDesc.RasterizerState.AntialiasedLineEnable = TRUE;
	gPipelineStateDesc.RasterizerState.ForcedSampleCount = 0; // sample count of UAV rendering // question 003 : what is UAV rendering?
	//Rasterazer - Conservative Rendering On/Off - (bosujuk rendering)
	gPipelineStateDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	//Pixel Shader
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01.hlsl", "PSMain", "ps_5_1", &pd3dPixelShaderBlob);

	//Output Merger
	//Output Merger-Blend
	D3D12_BLEND_DESC d3dBlendDesc;
	::ZeroMemory(&d3dBlendDesc, sizeof(D3D12_BLEND_DESC));
	d3dBlendDesc.AlphaToCoverageEnable = FALSE;
	d3dBlendDesc.IndependentBlendEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	d3dBlendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	d3dBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	gPipelineStateDesc.BlendState = d3dBlendDesc;
	//Output Merger - MSAA
	gPipelineStateDesc.SampleDesc.Count = (gd.m_bMsaa4xEnable) ? 4 : 1;
	gPipelineStateDesc.SampleDesc.Quality = (gd.m_bMsaa4xEnable) ? (gd.m_nMsaa4xQualityLevels - 1) : 0;
	gPipelineStateDesc.SampleMask = 0xFFFFFFFF; // pass every sampling.
	//Output Merger - DepthStencil
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc; // D3D12_DEPTH_STENCIL_DESC1 is using for Stream Pipeline state.. -> what is that?
	//Output Merger - DepthStencil - depth
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	//depthStencilDesc.DepthBoundsTestEnable = FALSE; // question 004 : what is this??
	//Output Merger - DepthStencil - stencil
	depthStencilDesc.StencilEnable = FALSE;
	depthStencilDesc.StencilReadMask = 0x00;
	depthStencilDesc.StencilWriteMask = 0x00;
	depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	depthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	gPipelineStateDesc.DepthStencilState = depthStencilDesc;
	//Output Merger - RenderTagets / DepthStencil Buffer
	gPipelineStateDesc.NumRenderTargets = 1;
	gPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	gd.pDevice->CreateGraphicsPipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState);
	if (pd3dVertexShaderBlob) pd3dVertexShaderBlob->Release();
	if (pd3dPixelShaderBlob) pd3dPixelShaderBlob->Release();
}

void PBRShader1::CreatePipelineState_withShadow()
{
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dVertexShaderBlob = NULL, * pd3dPixelShaderBlob = NULL;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature_withShadow;

	gPipelineStateDesc.NodeMask = 0;

	//Input Asm
	gPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[5] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},// uv vec2
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // normal vec3
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		// float3 tangent
		{ "EXTRA", 0, DXGI_FORMAT_R32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
		// float3 extra
	};
	gPipelineStateDesc.InputLayout.NumElements = sizeof(inputElementDesc) / sizeof(D3D12_INPUT_ELEMENT_DESC);
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01_withShadow.hlsl", "VSMain", "vs_5_1", &pd3dVertexShaderBlob);

	//Hull Shader
	//gPipelineStateDesc.HS = NULLCODE;

	//Tessellation

	//Domain Shader
	//gPipelineStateDesc.DS = NULLCODE;

	//Geometry Shader
	//gPipelineStateDesc.GS = NULLCODE;

	//Stream Output
	//gPipelineStateDesc.StreamOutput

	//Rasterazer
	gPipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	gPipelineStateDesc.RasterizerState.FrontCounterClockwise = FALSE; // front = clock wise
	//Rasterazer-depth
	gPipelineStateDesc.RasterizerState.DepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthBiasClamp = 0;
	gPipelineStateDesc.RasterizerState.SlopeScaledDepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthClipEnable = TRUE; // if depth > 1, cliping vertex.
	//Rasterazer-MSAA
	gPipelineStateDesc.RasterizerState.MultisampleEnable = TRUE;
	gPipelineStateDesc.RasterizerState.AntialiasedLineEnable = TRUE;
	gPipelineStateDesc.RasterizerState.ForcedSampleCount = 0; // sample count of UAV rendering // question 003 : what is UAV rendering?
	//Rasterazer - Conservative Rendering On/Off - (bosujuk rendering)
	gPipelineStateDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	//Pixel Shader
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01_withShadow.hlsl", "PSMain", "ps_5_1", &pd3dPixelShaderBlob);

	//Output Merger
	//Output Merger-Blend
	D3D12_BLEND_DESC d3dBlendDesc;
	::ZeroMemory(&d3dBlendDesc, sizeof(D3D12_BLEND_DESC));
	d3dBlendDesc.AlphaToCoverageEnable = FALSE;
	d3dBlendDesc.IndependentBlendEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	d3dBlendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	d3dBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	gPipelineStateDesc.BlendState = d3dBlendDesc;
	//Output Merger - MSAA
	gPipelineStateDesc.SampleDesc.Count = (gd.m_bMsaa4xEnable) ? 4 : 1;
	gPipelineStateDesc.SampleDesc.Quality = (gd.m_bMsaa4xEnable) ? (gd.m_nMsaa4xQualityLevels - 1) : 0;
	gPipelineStateDesc.SampleMask = 0xFFFFFFFF; // pass every sampling.
	//Output Merger - DepthStencil
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc; // D3D12_DEPTH_STENCIL_DESC1 is using for Stream Pipeline state.. -> what is that?
	//Output Merger - DepthStencil - depth
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	//depthStencilDesc.DepthBoundsTestEnable = FALSE; // question 004 : what is this??
	//Output Merger - DepthStencil - stencil
	depthStencilDesc.StencilEnable = FALSE;
	depthStencilDesc.StencilReadMask = 0x00;
	depthStencilDesc.StencilWriteMask = 0x00;
	depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	depthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	gPipelineStateDesc.DepthStencilState = depthStencilDesc;
	//Output Merger - RenderTagets / DepthStencil Buffer
	gPipelineStateDesc.NumRenderTargets = 1;
	gPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	gd.pDevice->CreateGraphicsPipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState_withShadow);
	if (pd3dVertexShaderBlob) pd3dVertexShaderBlob->Release();
	if (pd3dPixelShaderBlob) pd3dPixelShaderBlob->Release();
}

void PBRShader1::CreatePipelineState_RenderShadowMap()
{
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dVertexShaderBlob = NULL, * pd3dPixelShaderBlob = NULL;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature;

	gPipelineStateDesc.NodeMask = 0;

	//Input Asm
	gPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[5] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},// uv vec2
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // normal vec3
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		// float3 tangent
		{ "EXTRA", 0, DXGI_FORMAT_R32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
		// float3 extra
	};
	gPipelineStateDesc.InputLayout.NumElements = 5;
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01.hlsl", "VSRenderShadow", "vs_5_1", &pd3dVertexShaderBlob);

	//Hull Shader
	//gPipelineStateDesc.HS = NULLCODE;

	//Tessellation

	//Domain Shader
	//gPipelineStateDesc.DS = NULLCODE;

	//Geometry Shader
	//gPipelineStateDesc.GS = NULLCODE;

	//Stream Output
	//gPipelineStateDesc.StreamOutput

	//Rasterazer
	gPipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	gPipelineStateDesc.RasterizerState.FrontCounterClockwise = FALSE; // front = clock wise
	//Rasterazer-depth
	gPipelineStateDesc.RasterizerState.DepthBias = 0; // Depth = Depth + DepthBias (pixel space?)
	// Depth = DepthBias * 2^k + SlopeScaledDepthBias * MaxDepthSlope


	gPipelineStateDesc.RasterizerState.DepthBiasClamp = 0; // maximun of depth bias

	gPipelineStateDesc.RasterizerState.SlopeScaledDepthBias = 0;
	// in gpu, according to slope of mesh, calculate dynamic bias.

	gPipelineStateDesc.RasterizerState.DepthClipEnable = TRUE; // if depth > 1, cliping vertex.
	//Rasterazer-MSAA
	gPipelineStateDesc.RasterizerState.MultisampleEnable = TRUE;
	gPipelineStateDesc.RasterizerState.AntialiasedLineEnable = TRUE;
	gPipelineStateDesc.RasterizerState.ForcedSampleCount = 0; // sample count of UAV rendering // question 003 : what is UAV rendering?
	//Rasterazer - Conservative Rendering On/Off - (bosujuk rendering)
	gPipelineStateDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	//Pixel Shader
	gPipelineStateDesc.PS = NULLCODE;

	//Output Merger
	//Output Merger-Blend
	D3D12_BLEND_DESC d3dBlendDesc;
	::ZeroMemory(&d3dBlendDesc, sizeof(D3D12_BLEND_DESC));
	d3dBlendDesc.AlphaToCoverageEnable = FALSE;
	d3dBlendDesc.IndependentBlendEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	d3dBlendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	d3dBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	gPipelineStateDesc.BlendState = d3dBlendDesc;
	//Output Merger - MSAA
	gPipelineStateDesc.SampleDesc.Count = (gd.m_bMsaa4xEnable) ? 4 : 1;
	gPipelineStateDesc.SampleDesc.Quality = (gd.m_bMsaa4xEnable) ? (gd.m_nMsaa4xQualityLevels - 1) : 0;
	gPipelineStateDesc.SampleMask = 0xFFFFFFFF; // pass every sampling.
	//Output Merger - DepthStencil
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc; // D3D12_DEPTH_STENCIL_DESC1 is using for Stream Pipeline state.. -> what is that?
	//Output Merger - DepthStencil - depth
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	//depthStencilDesc.DepthBoundsTestEnable = FALSE; // question 004 : what is this??
	//Output Merger - DepthStencil - stencil
	depthStencilDesc.StencilEnable = FALSE;
	depthStencilDesc.StencilReadMask = 0x00;
	depthStencilDesc.StencilWriteMask = 0x00;
	depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	depthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	gPipelineStateDesc.DepthStencilState = depthStencilDesc;
	//Output Merger - RenderTagets / DepthStencil Buffer
	//gPipelineStateDesc.NumRenderTargets = 1;
	//gPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	//gPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	gPipelineStateDesc.NumRenderTargets = 0;
	gPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	gPipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	gPipelineStateDesc.SampleDesc.Count = 1;

	gd.pDevice->CreateGraphicsPipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState_RenderShadowMap);
	if (pd3dVertexShaderBlob) pd3dVertexShaderBlob->Release();
	if (pd3dPixelShaderBlob) pd3dPixelShaderBlob->Release();
}

void PBRShader1::CreatePipelineState_RenderStencil()
{
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dVertexShaderBlob = NULL, * pd3dPixelShaderBlob = NULL;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature;

	gPipelineStateDesc.NodeMask = 0;

	//Input Asm
	gPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[5] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},// uv vec2
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // normal vec3
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		// float3 tangent
		{ "EXTRA", 0, DXGI_FORMAT_R32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
		// float3 extra
	};
	gPipelineStateDesc.InputLayout.NumElements = 5;
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01.hlsl", "VSMain", "vs_5_1", &pd3dVertexShaderBlob);

	//Hull Shader
	//gPipelineStateDesc.HS = NULLCODE;

	//Tessellation

	//Domain Shader
	//gPipelineStateDesc.DS = NULLCODE;

	//Geometry Shader
	//gPipelineStateDesc.GS = NULLCODE;

	//Stream Output
	//gPipelineStateDesc.StreamOutput

	//Rasterazer
	gPipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	gPipelineStateDesc.RasterizerState.FrontCounterClockwise = FALSE; // front = clock wise
	//Rasterazer-depth
	gPipelineStateDesc.RasterizerState.DepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthBiasClamp = 0;
	gPipelineStateDesc.RasterizerState.SlopeScaledDepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthClipEnable = TRUE; // if depth > 1, cliping vertex.
	//Rasterazer-MSAA
	gPipelineStateDesc.RasterizerState.MultisampleEnable = TRUE;
	gPipelineStateDesc.RasterizerState.AntialiasedLineEnable = TRUE;
	gPipelineStateDesc.RasterizerState.ForcedSampleCount = 0; // sample count of UAV rendering // question 003 : what is UAV rendering?
	//Rasterazer - Conservative Rendering On/Off - (bosujuk rendering)
	gPipelineStateDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	//Pixel Shader
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01.hlsl", "PSMain", "ps_5_1", &pd3dPixelShaderBlob);

	//Output Merger
	//Output Merger-Blend
	D3D12_BLEND_DESC d3dBlendDesc;
	::ZeroMemory(&d3dBlendDesc, sizeof(D3D12_BLEND_DESC));
	d3dBlendDesc.AlphaToCoverageEnable = FALSE;
	d3dBlendDesc.IndependentBlendEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].BlendEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;

	d3dBlendDesc.RenderTarget[0].RenderTargetWriteMask = 0;
	//do not update render target

	gPipelineStateDesc.BlendState = d3dBlendDesc;
	//Output Merger - MSAA
	gPipelineStateDesc.SampleDesc.Count = (gd.m_bMsaa4xEnable) ? 4 : 1;
	gPipelineStateDesc.SampleDesc.Quality = (gd.m_bMsaa4xEnable) ? (gd.m_nMsaa4xQualityLevels - 1) : 0;
	gPipelineStateDesc.SampleMask = 0xFFFFFFFF; // pass every sampling.
	//Output Merger - DepthStencil
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc; // D3D12_DEPTH_STENCIL_DESC1 is using for Stream Pipeline state.. -> what is that?
	//Output Merger - DepthStencil - depth
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	//depthStencilDesc.DepthBoundsTestEnable = FALSE; // question 004 : what is this??
	//Output Merger - DepthStencil - stencil
	depthStencilDesc.StencilEnable = TRUE;
	depthStencilDesc.StencilReadMask = 0x00;
	depthStencilDesc.StencilWriteMask = 0xFF;
	depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;

	depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	depthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;

	depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	gPipelineStateDesc.DepthStencilState = depthStencilDesc;
	//Output Merger - RenderTagets / DepthStencil Buffer
	gPipelineStateDesc.NumRenderTargets = 1;
	gPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	gd.pDevice->CreateGraphicsPipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState_RenderStencil);
	if (pd3dVertexShaderBlob) pd3dVertexShaderBlob->Release();
	if (pd3dPixelShaderBlob) pd3dPixelShaderBlob->Release();

	//
}

void PBRShader1::CreatePipelineState_InnerMirror()
{
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dVertexShaderBlob = NULL, * pd3dPixelShaderBlob = NULL;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature;

	gPipelineStateDesc.NodeMask = 0;

	//Input Asm
	gPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[5] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},// uv vec2
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // normal vec3
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		// float3 tangent
		{ "EXTRA", 0, DXGI_FORMAT_R32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
		// float3 extra
	};
	gPipelineStateDesc.InputLayout.NumElements = 5;
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01.hlsl", "VSMain", "vs_5_1", &pd3dVertexShaderBlob);

	//Hull Shader
	//gPipelineStateDesc.HS = NULLCODE;

	//Tessellation

	//Domain Shader
	//gPipelineStateDesc.DS = NULLCODE;

	//Geometry Shader
	//gPipelineStateDesc.GS = NULLCODE;

	//Stream Output
	//gPipelineStateDesc.StreamOutput

	//Rasterazer
	gPipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	gPipelineStateDesc.RasterizerState.FrontCounterClockwise = TRUE; // front = clock wise
	//Rasterazer-depth
	gPipelineStateDesc.RasterizerState.DepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthBiasClamp = 0;
	gPipelineStateDesc.RasterizerState.SlopeScaledDepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthClipEnable = TRUE; // if depth > 1, cliping vertex.
	//Rasterazer-MSAA
	gPipelineStateDesc.RasterizerState.MultisampleEnable = TRUE;
	gPipelineStateDesc.RasterizerState.AntialiasedLineEnable = TRUE;
	gPipelineStateDesc.RasterizerState.ForcedSampleCount = 0; // sample count of UAV rendering // question 003 : what is UAV rendering?
	//Rasterazer - Conservative Rendering On/Off - (bosujuk rendering)
	gPipelineStateDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	//Pixel Shader
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01.hlsl", "PSMain", "ps_5_1", &pd3dPixelShaderBlob);

	//Output Merger
	//Output Merger-Blend
	D3D12_BLEND_DESC d3dBlendDesc;
	::ZeroMemory(&d3dBlendDesc, sizeof(D3D12_BLEND_DESC));
	d3dBlendDesc.AlphaToCoverageEnable = FALSE;
	d3dBlendDesc.IndependentBlendEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	d3dBlendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	d3dBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	gPipelineStateDesc.BlendState = d3dBlendDesc;
	//Output Merger - MSAA
	gPipelineStateDesc.SampleDesc.Count = (gd.m_bMsaa4xEnable) ? 4 : 1;
	gPipelineStateDesc.SampleDesc.Quality = (gd.m_bMsaa4xEnable) ? (gd.m_nMsaa4xQualityLevels - 1) : 0;
	gPipelineStateDesc.SampleMask = 0xFFFFFFFF; // pass every sampling.
	//Output Merger - DepthStencil
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc; // D3D12_DEPTH_STENCIL_DESC1 is using for Stream Pipeline state.. -> what is that?
	//Output Merger - DepthStencil - depth
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	//depthStencilDesc.DepthBoundsTestEnable = FALSE; // question 004 : what is this??
	//Output Merger - DepthStencil - stencil
	depthStencilDesc.StencilEnable = TRUE;
	depthStencilDesc.StencilReadMask = 0xFF;
	depthStencilDesc.StencilWriteMask = 0xFF;
	depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
	depthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
	gPipelineStateDesc.DepthStencilState = depthStencilDesc;
	//Output Merger - RenderTagets / DepthStencil Buffer
	gPipelineStateDesc.NumRenderTargets = 1;
	gPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	gd.pDevice->CreateGraphicsPipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState_RenderInnerMirror);
	if (pd3dVertexShaderBlob) pd3dVertexShaderBlob->Release();
	if (pd3dPixelShaderBlob) pd3dPixelShaderBlob->Release();
}

void PBRShader1::CreatePipelineState_SkinedMesh()
{
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dVertexShaderBlob = NULL, * pd3dPixelShaderBlob = NULL;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature_SkinedMesh_withShadow;

	gPipelineStateDesc.NodeMask = 0;

	//Input Asm
	gPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	constexpr int inputElementCount = 13;
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[inputElementCount] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},// uv vec2
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // normal vec3
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		// float3 tangent
		{ "EXTRA", 0, DXGI_FORMAT_R32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		// float3 extra
		{ "BONEINDEX", 0, DXGI_FORMAT_R32_UINT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // uint boneindex0;
		{ "BONEWEIGHT", 0, DXGI_FORMAT_R32_FLOAT, 1, 4, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // float boneweight0;
		{ "BONEINDEX", 1, DXGI_FORMAT_R32_UINT, 1, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // uint boneindex1;
		{ "BONEWEIGHT", 1, DXGI_FORMAT_R32_FLOAT, 1, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // float boneweight1;
		{ "BONEINDEX", 2, DXGI_FORMAT_R32_UINT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // uint boneindex2;
		{ "BONEWEIGHT", 2, DXGI_FORMAT_R32_FLOAT, 1, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },  // float boneweight2;
		{ "BONEINDEX", 3, DXGI_FORMAT_R32_UINT, 1, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // uint boneindex3;
		{ "BONEWEIGHT", 3, DXGI_FORMAT_R32_FLOAT, 1, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // float boneweight3;
	};
	gPipelineStateDesc.InputLayout.NumElements = inputElementCount;
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01_SkinMeshwithShadow.hlsl", "VSMain", "vs_5_1", &pd3dVertexShaderBlob);

	//Hull Shader
	//gPipelineStateDesc.HS = NULLCODE;

	//Tessellation

	//Domain Shader
	//gPipelineStateDesc.DS = NULLCODE;

	//Geometry Shader
	//gPipelineStateDesc.GS = NULLCODE;

	//Stream Output
	//gPipelineStateDesc.StreamOutput

	//Rasterazer
	gPipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	gPipelineStateDesc.RasterizerState.FrontCounterClockwise = FALSE; // front = clock wise
	//Rasterazer-depth
	gPipelineStateDesc.RasterizerState.DepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthBiasClamp = 0;
	gPipelineStateDesc.RasterizerState.SlopeScaledDepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthClipEnable = TRUE; // if depth > 1, cliping vertex.
	//Rasterazer-MSAA
	gPipelineStateDesc.RasterizerState.MultisampleEnable = TRUE;
	gPipelineStateDesc.RasterizerState.AntialiasedLineEnable = TRUE;
	gPipelineStateDesc.RasterizerState.ForcedSampleCount = 0; // sample count of UAV rendering // question 003 : what is UAV rendering?
	//Rasterazer - Conservative Rendering On/Off - (bosujuk rendering)
	gPipelineStateDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	//Pixel Shader
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01_SkinMeshwithShadow.hlsl", "PSMain", "ps_5_1", &pd3dPixelShaderBlob);

	//Output Merger
	//Output Merger-Blend
	D3D12_BLEND_DESC d3dBlendDesc;
	::ZeroMemory(&d3dBlendDesc, sizeof(D3D12_BLEND_DESC));
	d3dBlendDesc.AlphaToCoverageEnable = FALSE;
	d3dBlendDesc.IndependentBlendEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	d3dBlendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	d3dBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	gPipelineStateDesc.BlendState = d3dBlendDesc;
	//Output Merger - MSAA
	gPipelineStateDesc.SampleDesc.Count = (gd.m_bMsaa4xEnable) ? 4 : 1;
	gPipelineStateDesc.SampleDesc.Quality = (gd.m_bMsaa4xEnable) ? (gd.m_nMsaa4xQualityLevels - 1) : 0;
	gPipelineStateDesc.SampleMask = 0xFFFFFFFF; // pass every sampling.
	//Output Merger - DepthStencil
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc; // D3D12_DEPTH_STENCIL_DESC1 is using for Stream Pipeline state.. -> what is that?
	//Output Merger - DepthStencil - depth
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	//depthStencilDesc.DepthBoundsTestEnable = FALSE; // question 004 : what is this??
	//Output Merger - DepthStencil - stencil
	depthStencilDesc.StencilEnable = FALSE;
	depthStencilDesc.StencilReadMask = 0x00;
	depthStencilDesc.StencilWriteMask = 0x00;
	depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	depthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	gPipelineStateDesc.DepthStencilState = depthStencilDesc;
	//Output Merger - RenderTagets / DepthStencil Buffer
	gPipelineStateDesc.NumRenderTargets = 1;
	gPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	gd.pDevice->CreateGraphicsPipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState_SkinedMesh_withShadow);
	if (pd3dVertexShaderBlob) pd3dVertexShaderBlob->Release();
	if (pd3dPixelShaderBlob) pd3dPixelShaderBlob->Release();
}

void PBRShader1::CreatePipelineState_Instancing_withShadow()
{
	vector<D3D_SHADER_MACRO> macroSet;
	string newSize = to_string(gd.ShaderVisibleDescPool.TextureSRVCap);
	macroSet.push_back({ "MAX_TEXTURES", newSize.c_str() });
	macroSet.push_back({ nullptr, nullptr });

	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dVertexShaderBlob = NULL, * pd3dPixelShaderBlob = NULL;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature_Instancing_withShadow;

	gPipelineStateDesc.NodeMask = 0;

	//Input Asm
	gPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[5] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},// uv vec2
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // normal vec3
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		// float3 tangent
		{ "EXTRA", 0, DXGI_FORMAT_R32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
		// float3 extra
	};
	gPipelineStateDesc.InputLayout.NumElements = sizeof(inputElementDesc) / sizeof(D3D12_INPUT_ELEMENT_DESC);
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01_InstancingwithShadow.hlsl", "VSInstancingMain", "vs_5_1", &pd3dVertexShaderBlob, &macroSet);

	//Hull Shader
	//gPipelineStateDesc.HS = NULLCODE;

	//Tessellation

	//Domain Shader
	//gPipelineStateDesc.DS = NULLCODE;

	//Geometry Shader
	//gPipelineStateDesc.GS = NULLCODE;

	//Stream Output
	//gPipelineStateDesc.StreamOutput

	//Rasterazer
	gPipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	gPipelineStateDesc.RasterizerState.FrontCounterClockwise = FALSE; // front = clock wise
	//Rasterazer-depth
	gPipelineStateDesc.RasterizerState.DepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthBiasClamp = 0;
	gPipelineStateDesc.RasterizerState.SlopeScaledDepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthClipEnable = TRUE; // if depth > 1, cliping vertex.
	//Rasterazer-MSAA
	gPipelineStateDesc.RasterizerState.MultisampleEnable = TRUE;
	gPipelineStateDesc.RasterizerState.AntialiasedLineEnable = TRUE;
	gPipelineStateDesc.RasterizerState.ForcedSampleCount = 0; // sample count of UAV rendering // question 003 : what is UAV rendering?
	//Rasterazer - Conservative Rendering On/Off - (bosujuk rendering)
	gPipelineStateDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	//Pixel Shader
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/PBRShader01_InstancingwithShadow.hlsl", "PSInstancingMain", "ps_5_1", &pd3dPixelShaderBlob, &macroSet);

	//Output Merger
	//Output Merger-Blend
	D3D12_BLEND_DESC d3dBlendDesc;
	::ZeroMemory(&d3dBlendDesc, sizeof(D3D12_BLEND_DESC));
	d3dBlendDesc.AlphaToCoverageEnable = FALSE;
	d3dBlendDesc.IndependentBlendEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	d3dBlendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	d3dBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	gPipelineStateDesc.BlendState = d3dBlendDesc;
	//Output Merger - MSAA
	gPipelineStateDesc.SampleDesc.Count = (gd.m_bMsaa4xEnable) ? 4 : 1;
	gPipelineStateDesc.SampleDesc.Quality = (gd.m_bMsaa4xEnable) ? (gd.m_nMsaa4xQualityLevels - 1) : 0;
	gPipelineStateDesc.SampleMask = 0xFFFFFFFF; // pass every sampling.
	//Output Merger - DepthStencil
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc; // D3D12_DEPTH_STENCIL_DESC1 is using for Stream Pipeline state.. -> what is that?
	//Output Merger - DepthStencil - depth
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	//depthStencilDesc.DepthBoundsTestEnable = FALSE; // question 004 : what is this??
	//Output Merger - DepthStencil - stencil
	depthStencilDesc.StencilEnable = FALSE;
	depthStencilDesc.StencilReadMask = 0x00;
	depthStencilDesc.StencilWriteMask = 0x00;
	depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	depthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	gPipelineStateDesc.DepthStencilState = depthStencilDesc;
	//Output Merger - RenderTagets / DepthStencil Buffer
	gPipelineStateDesc.NumRenderTargets = 1;
	gPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	gd.pDevice->CreateGraphicsPipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState_Instancing_withShadow);
	if (pd3dVertexShaderBlob) pd3dVertexShaderBlob->Release();
	if (pd3dPixelShaderBlob) pd3dPixelShaderBlob->Release();
}

void PBRShader1::ReBuild_Shader(ShaderType st)
{
	if (st == ShaderType::InstancingWithShadow) {
		if (pPipelineState_Instancing_withShadow != nullptr) {
			pPipelineState_Instancing_withShadow->Release();
			pRootSignature_Instancing_withShadow->Release();
			pPipelineState_Instancing_withShadow = nullptr;
			pRootSignature_Instancing_withShadow = nullptr;
		}
		CreateRootSignature_Instancing_withShadow();
		CreatePipelineState_Instancing_withShadow();
	}
}

void PBRShader1::Release()
{
	if (pPipelineState) pPipelineState->Release();
	if (pRootSignature) pRootSignature->Release();
}

void PBRShader1::Add_RegisterShaderCommand(GPUCmd& cmd, ShaderType reg)
{
	switch (reg) {
	case ShaderType::RenderNormal:
		cmd->SetPipelineState(pPipelineState);
		cmd->SetGraphicsRootSignature(pRootSignature);
		return;
	case ShaderType::RenderWithShadow:
		cmd->SetPipelineState(pPipelineState_withShadow);
		cmd->SetGraphicsRootSignature(pRootSignature_withShadow);
		return;
	case ShaderType::RenderShadowMap:
		cmd->SetPipelineState(pPipelineState_RenderShadowMap);
		cmd->SetGraphicsRootSignature(pRootSignature);
		return;
	case ShaderType::RenderStencil:
		cmd->SetPipelineState(pPipelineState_RenderStencil);
		cmd->SetGraphicsRootSignature(pRootSignature);
		return;
	case ShaderType::RenderInnerMirror:
		cmd->SetPipelineState(pPipelineState_RenderInnerMirror);
		cmd->SetGraphicsRootSignature(pRootSignature);
		return;
	case ShaderType::RenderTerrain:
		cmd->SetPipelineState(pPipelineState_Terrain);
		cmd->SetGraphicsRootSignature(pRootSignature_Terrain);
		return;
	case ShaderType::TessTerrain:
		cmd->SetPipelineState(pPipelineState_TessTerrain);
		cmd->SetGraphicsRootSignature(pRootSignature_TessTerrain);
		return;
	case ShaderType::SkinMeshRender:
		cmd->SetPipelineState(pPipelineState_SkinedMesh_withShadow);
		cmd->SetGraphicsRootSignature(pRootSignature_SkinedMesh_withShadow);
		return;
	case ShaderType::InstancingWithShadow:
		cmd->SetPipelineState(pPipelineState_Instancing_withShadow);
		cmd->SetGraphicsRootSignature(pRootSignature_Instancing_withShadow);
		return;
	}
}

void PBRShader1::SetTextureCommand(GPUResource* Color, GPUResource* Normal, GPUResource* AO, GPUResource* Metalic, GPUResource* Roughness)
{
	DescHandle hDesc;

	gd.ShaderVisibleDescPool.DynamicAlloc(&hDesc, 5);
	DescHandle hStart = hDesc;

	//if (gd.ShaderVisibleDescPool.IncludeHandle(Color->hCpu) == false) {
	//	
	//}

	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, Color->descindex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	hDesc += gd.SRVSize;
	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, Normal->descindex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	hDesc += gd.SRVSize;
	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, AO->descindex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	hDesc += gd.SRVSize;
	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, Metalic->descindex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	hDesc += gd.SRVSize;
	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, Roughness->descindex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	gd.gpucmd->SetGraphicsRootDescriptorTable(3, hStart.hgpu);
}

void PBRShader1::SetShadowMapCommand(DescHandle shadowMapDesc)
{
	gd.gpucmd->SetGraphicsRootDescriptorTable(5, shadowMapDesc.hgpu);
}

void PBRShader1::SetMaterialCBV(D3D12_GPU_DESCRIPTOR_HANDLE hgpu)
{
	gd.gpucmd->SetGraphicsRootDescriptorTable(4, hgpu);
}
#pragma endregion

#pragma region SkyBoxShaderCode
void SkyBoxShader::LoadSkyBox(const wchar_t* filename)
{
	ID3D12Resource* uploadbuffer = nullptr;
	ID3D12Resource* res = CurrentSkyBox.CreateTextureResourceFromDDSFile(gd.pDevice, gd.gpucmd, (wchar_t*)filename, &uploadbuffer, D3D12_RESOURCE_STATE_GENERIC_READ, true);
	res->QueryInterface<ID3D12Resource2>(&CurrentSkyBox.resource);
	CurrentSkyBox.CurrentState_InCommandWriteLine = D3D12_RESOURCE_STATE_GENERIC_READ;

	// success.
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = res->GetDesc().Format;
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	SRVDesc.TextureCube.MipLevels = 1;
	SRVDesc.TextureCube.MostDetailedMip = 0;
	SRVDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	gd.ShaderVisibleDescPool.ImmortalAlloc(&CurrentSkyBox.descindex, 1);
	gd.pDevice->CreateShaderResourceView(CurrentSkyBox.resource, &SRVDesc, CurrentSkyBox.descindex.hCreation.hcpu);

	constexpr int nVertices = 36;
	D3D_PRIMITIVE_TOPOLOGY d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	XMFLOAT3 m_pxmf3Positions[nVertices] = {};

	float fx = 10.0f, fy = 10.0f, fz = 10.0f;
	// Front Quad (quads point inward)
	m_pxmf3Positions[0] = XMFLOAT3(-fx, +fx, +fx);
	m_pxmf3Positions[1] = XMFLOAT3(+fx, +fx, +fx);
	m_pxmf3Positions[2] = XMFLOAT3(-fx, -fx, +fx);
	m_pxmf3Positions[3] = XMFLOAT3(-fx, -fx, +fx);
	m_pxmf3Positions[4] = XMFLOAT3(+fx, +fx, +fx);
	m_pxmf3Positions[5] = XMFLOAT3(+fx, -fx, +fx);
	// Back Quad										
	m_pxmf3Positions[6] = XMFLOAT3(+fx, +fx, -fx);
	m_pxmf3Positions[7] = XMFLOAT3(-fx, +fx, -fx);
	m_pxmf3Positions[8] = XMFLOAT3(+fx, -fx, -fx);
	m_pxmf3Positions[9] = XMFLOAT3(+fx, -fx, -fx);
	m_pxmf3Positions[10] = XMFLOAT3(-fx, +fx, -fx);
	m_pxmf3Positions[11] = XMFLOAT3(-fx, -fx, -fx);
	// Left Quad										
	m_pxmf3Positions[12] = XMFLOAT3(-fx, +fx, -fx);
	m_pxmf3Positions[13] = XMFLOAT3(-fx, +fx, +fx);
	m_pxmf3Positions[14] = XMFLOAT3(-fx, -fx, -fx);
	m_pxmf3Positions[15] = XMFLOAT3(-fx, -fx, -fx);
	m_pxmf3Positions[16] = XMFLOAT3(-fx, +fx, +fx);
	m_pxmf3Positions[17] = XMFLOAT3(-fx, -fx, +fx);
	// Right Quad										
	m_pxmf3Positions[18] = XMFLOAT3(+fx, +fx, +fx);
	m_pxmf3Positions[19] = XMFLOAT3(+fx, +fx, -fx);
	m_pxmf3Positions[20] = XMFLOAT3(+fx, -fx, +fx);
	m_pxmf3Positions[21] = XMFLOAT3(+fx, -fx, +fx);
	m_pxmf3Positions[22] = XMFLOAT3(+fx, +fx, -fx);
	m_pxmf3Positions[23] = XMFLOAT3(+fx, -fx, -fx);
	// Top Quad											
	m_pxmf3Positions[24] = XMFLOAT3(-fx, +fx, -fx);
	m_pxmf3Positions[25] = XMFLOAT3(+fx, +fx, -fx);
	m_pxmf3Positions[26] = XMFLOAT3(-fx, +fx, +fx);
	m_pxmf3Positions[27] = XMFLOAT3(-fx, +fx, +fx);
	m_pxmf3Positions[28] = XMFLOAT3(+fx, +fx, -fx);
	m_pxmf3Positions[29] = XMFLOAT3(+fx, +fx, +fx);
	// Bottom Quad										
	m_pxmf3Positions[30] = XMFLOAT3(-fx, -fx, +fx);
	m_pxmf3Positions[31] = XMFLOAT3(+fx, -fx, +fx);
	m_pxmf3Positions[32] = XMFLOAT3(-fx, -fx, -fx);
	m_pxmf3Positions[33] = XMFLOAT3(-fx, -fx, -fx);
	m_pxmf3Positions[34] = XMFLOAT3(+fx, -fx, +fx);
	m_pxmf3Positions[35] = XMFLOAT3(+fx, -fx, -fx);

	SkyBoxMesh = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * sizeof(XMFLOAT3), 1);
	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * sizeof(XMFLOAT3), 1);
	gd.UploadToCommitedGPUBuffer(&m_pxmf3Positions[0], &VertexUploadBuffer, &SkyBoxMesh, true);

	SkyBoxMeshVBView.BufferLocation = SkyBoxMesh.resource->GetGPUVirtualAddress();
	SkyBoxMeshVBView.StrideInBytes = sizeof(XMFLOAT3);
	SkyBoxMeshVBView.SizeInBytes = sizeof(XMFLOAT3) * nVertices;
}

SkyBoxShader::SkyBoxShader()
{
}

SkyBoxShader::~SkyBoxShader()
{
}

void SkyBoxShader::InitShader()
{
	CreateRootSignature();
	CreatePipelineState();
	CreatePipelineState_InnerMirror();
}

void SkyBoxShader::CreateRootSignature()
{
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;

	D3D12_ROOT_PARAMETER1 rootParam[3] = {};

	rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParam[0].Constants.Num32BitValues = 16;
	rootParam[0].Constants.ShaderRegister = 0; // b0 : Camera Matrix (View)
	rootParam[0].Constants.RegisterSpace = 0;

	rootParam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParam[1].Constants.Num32BitValues = 16;
	rootParam[1].Constants.ShaderRegister = 1; // b1 : Transform Matrix
	rootParam[1].Constants.RegisterSpace = 0;

	rootParam[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParam[2].DescriptorTable.NumDescriptorRanges = 1;
	D3D12_DESCRIPTOR_RANGE1 ranges[1];
	ranges[0].BaseShaderRegister = 0; // t0 //Diffuse Texture
	ranges[0].RegisterSpace = 0;
	ranges[0].NumDescriptors = 1;
	ranges[0].OffsetInDescriptorsFromTableStart = 0;
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
	rootParam[2].DescriptorTable.pDescriptorRanges = &ranges[0];

	D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT/* |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS*/;

		/// default sampler
	D3D12_STATIC_SAMPLER_DESC sampler = {};
	// sampler register index.
	sampler.ShaderRegister = 0;
	// setting
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.MipLODBias = 0.0f;
	sampler.MaxAnisotropy = 16;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	sampler.MinLOD = -FLT_MAX;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

	rootSigDesc1.NumParameters = sizeof(rootParam) / sizeof(D3D12_ROOT_PARAMETER1);
	rootSigDesc1.pParameters = rootParam;
	rootSigDesc1.NumStaticSamplers = 1; // question002 : what is static samplers?
	rootSigDesc1.pStaticSamplers = &sampler;
	rootSigDesc1.Flags = d3dRootSignatureFlags;

	ID3DBlob* pd3dSignatureBlob = NULL;
	ID3DBlob* pd3dErrorBlob = NULL;
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_rootSigDesc;
	versioned_rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	versioned_rootSigDesc.Desc_1_1 = rootSigDesc1;
	D3D12SerializeVersionedRootSignature(&versioned_rootSigDesc, &pd3dSignatureBlob, &pd3dErrorBlob);
	gd.pDevice->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(),
		pd3dSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void
			**)&pRootSignature);
	if (pd3dErrorBlob) pd3dErrorBlob->Release();
	if (pd3dSignatureBlob) pd3dSignatureBlob->Release();
}

void SkyBoxShader::CreatePipelineState()
{
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dVertexShaderBlob = NULL, * pd3dPixelShaderBlob = NULL;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature;

	gPipelineStateDesc.NodeMask = 0;

	//Input Asm
	gPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[1] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
	};
	gPipelineStateDesc.InputLayout.NumElements = 1;
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/SkyBox.hlsl", "VSSkyBox", "vs_5_1", &pd3dVertexShaderBlob);

	//Hull Shader
	//gPipelineStateDesc.HS = NULLCODE;

	//Tessellation

	//Domain Shader
	//gPipelineStateDesc.DS = NULLCODE;

	//Geometry Shader
	//gPipelineStateDesc.GS = NULLCODE;

	//Stream Output
	//gPipelineStateDesc.StreamOutput

	//Rasterazer
	gPipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	gPipelineStateDesc.RasterizerState.FrontCounterClockwise = FALSE; // front = clock wise
	//Rasterazer-depth
	gPipelineStateDesc.RasterizerState.DepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthBiasClamp = 0;
	gPipelineStateDesc.RasterizerState.SlopeScaledDepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthClipEnable = TRUE; // if depth > 1, cliping vertex.
	//Rasterazer-MSAA
	gPipelineStateDesc.RasterizerState.MultisampleEnable = TRUE;
	gPipelineStateDesc.RasterizerState.AntialiasedLineEnable = TRUE;
	gPipelineStateDesc.RasterizerState.ForcedSampleCount = 0; // sample count of UAV rendering // question 003 : what is UAV rendering?
	//Rasterazer - Conservative Rendering On/Off - (bosujuk rendering)
	gPipelineStateDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	//Pixel Shader
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/SkyBox.hlsl", "PSSkyBox", "ps_5_1", &pd3dPixelShaderBlob);

	//Output Merger
	//Output Merger-Blend
	D3D12_BLEND_DESC d3dBlendDesc;
	::ZeroMemory(&d3dBlendDesc, sizeof(D3D12_BLEND_DESC));
	d3dBlendDesc.AlphaToCoverageEnable = FALSE;
	d3dBlendDesc.IndependentBlendEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	d3dBlendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	d3dBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	gPipelineStateDesc.BlendState = d3dBlendDesc;
	//Output Merger - MSAA
	gPipelineStateDesc.SampleDesc.Count = (gd.m_bMsaa4xEnable) ? 4 : 1;
	gPipelineStateDesc.SampleDesc.Quality = (gd.m_bMsaa4xEnable) ? (gd.m_nMsaa4xQualityLevels - 1) : 0;
	gPipelineStateDesc.SampleMask = 0xFFFFFFFF; // pass every sampling.
	//Output Merger - DepthStencil
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc; // D3D12_DEPTH_STENCIL_DESC1 is using for Stream Pipeline state.. -> what is that?
	//Output Merger - DepthStencil - depth
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	//depthStencilDesc.DepthBoundsTestEnable = FALSE; // question 004 : what is this??
	//Output Merger - DepthStencil - stencil
	depthStencilDesc.StencilEnable = FALSE;
	depthStencilDesc.StencilReadMask = 0x00;
	depthStencilDesc.StencilWriteMask = 0x00;
	depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	depthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	gPipelineStateDesc.DepthStencilState = depthStencilDesc;
	//Output Merger - RenderTagets / DepthStencil Buffer
	gPipelineStateDesc.NumRenderTargets = 1;
	gPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	gd.pDevice->CreateGraphicsPipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState);
	if (pd3dVertexShaderBlob) pd3dVertexShaderBlob->Release();
	if (pd3dPixelShaderBlob) pd3dPixelShaderBlob->Release();
}

void SkyBoxShader::CreatePipelineState_InnerMirror()
{
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dVertexShaderBlob = NULL, * pd3dPixelShaderBlob = NULL;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature;

	gPipelineStateDesc.NodeMask = 0;

	//Input Asm
	gPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[1] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }, // pos vec3
	};
	gPipelineStateDesc.InputLayout.NumElements = 1;
	gPipelineStateDesc.InputLayout.pInputElementDescs = inputElementDesc;

	//Vertex Shader
	gPipelineStateDesc.VS = Shader::GetShaderByteCode(L"Resources/Shaders/SkyBox.hlsl", "VSSkyBox", "vs_5_1", &pd3dVertexShaderBlob);

	//Hull Shader
	//gPipelineStateDesc.HS = NULLCODE;

	//Tessellation

	//Domain Shader
	//gPipelineStateDesc.DS = NULLCODE;

	//Geometry Shader
	//gPipelineStateDesc.GS = NULLCODE;

	//Stream Output
	//gPipelineStateDesc.StreamOutput

	//Rasterazer
	gPipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
	gPipelineStateDesc.RasterizerState.FrontCounterClockwise = FALSE; // front = clock wise
	//Rasterazer-depth
	gPipelineStateDesc.RasterizerState.DepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthBiasClamp = 0;
	gPipelineStateDesc.RasterizerState.SlopeScaledDepthBias = 0;
	gPipelineStateDesc.RasterizerState.DepthClipEnable = TRUE; // if depth > 1, cliping vertex.
	//Rasterazer-MSAA
	gPipelineStateDesc.RasterizerState.MultisampleEnable = TRUE;
	gPipelineStateDesc.RasterizerState.AntialiasedLineEnable = TRUE;
	gPipelineStateDesc.RasterizerState.ForcedSampleCount = 0; // sample count of UAV rendering // question 003 : what is UAV rendering?
	//Rasterazer - Conservative Rendering On/Off - (bosujuk rendering)
	gPipelineStateDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	//Pixel Shader
	gPipelineStateDesc.PS = Shader::GetShaderByteCode(L"Resources/Shaders/SkyBox.hlsl", "PSSkyBox", "ps_5_1", &pd3dPixelShaderBlob);

	//Output Merger
	//Output Merger-Blend
	D3D12_BLEND_DESC d3dBlendDesc;
	::ZeroMemory(&d3dBlendDesc, sizeof(D3D12_BLEND_DESC));
	d3dBlendDesc.AlphaToCoverageEnable = FALSE;
	d3dBlendDesc.IndependentBlendEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	d3dBlendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	d3dBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	gPipelineStateDesc.BlendState = d3dBlendDesc;
	//Output Merger - MSAA
	gPipelineStateDesc.SampleDesc.Count = (gd.m_bMsaa4xEnable) ? 4 : 1;
	gPipelineStateDesc.SampleDesc.Quality = (gd.m_bMsaa4xEnable) ? (gd.m_nMsaa4xQualityLevels - 1) : 0;
	gPipelineStateDesc.SampleMask = 0xFFFFFFFF; // pass every sampling.
	//Output Merger - DepthStencil
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc; // D3D12_DEPTH_STENCIL_DESC1 is using for Stream Pipeline state.. -> what is that?
	//Output Merger - DepthStencil - depth
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	//depthStencilDesc.DepthBoundsTestEnable = FALSE; // question 004 : what is this??
	//Output Merger - DepthStencil - stencil
	depthStencilDesc.StencilEnable = TRUE;
	depthStencilDesc.StencilReadMask = 0xFF;
	depthStencilDesc.StencilWriteMask = 0xFF;
	depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
	depthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
	gPipelineStateDesc.DepthStencilState = depthStencilDesc;
	//Output Merger - RenderTagets / DepthStencil Buffer
	gPipelineStateDesc.NumRenderTargets = 1;
	gPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	gd.pDevice->CreateGraphicsPipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState_RenderInnerMirror);
	if (pd3dVertexShaderBlob) pd3dVertexShaderBlob->Release();
	if (pd3dPixelShaderBlob) pd3dPixelShaderBlob->Release();
}

void SkyBoxShader::Add_RegisterShaderCommand(GPUCmd& cmd, ShaderType reg)
{
	switch (reg) {
	case ShaderType::RenderInnerMirror:
		gd.gpucmd->SetPipelineState(pPipelineState_RenderInnerMirror);
		gd.gpucmd->SetGraphicsRootSignature(pRootSignature);
		break;
	default:
		gd.gpucmd->SetPipelineState(pPipelineState);
		gd.gpucmd->SetGraphicsRootSignature(pRootSignature);
		break;
	}
}

void SkyBoxShader::Release()
{
	if (pPipelineState) pPipelineState->Release();
	if (pRootSignature) pRootSignature->Release();
	CurrentSkyBox.Release();
	SkyBoxMesh.Release();
	VertexUploadBuffer.Release();
}

void SkyBoxShader::RenderSkyBox(matrix parentMat, ShaderType reg)
{
	gd.gpucmd.SetShader(this, reg);
	gd.gpucmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);

	XMFLOAT4X4 xmf4x4Projection;
	XMStoreFloat4x4(&xmf4x4Projection, XMMatrixTranspose(gd.viewportArr[0].ProjectMatrix));
	gd.gpucmd->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4Projection, 0);

	matrix mat = gd.viewportArr[0].ViewMatrix;
	//mat.transpose();
	mat.pos = 0;
	mat.pos.w = 1;
	//mat.transpose();
	mat *= gd.viewportArr[0].ProjectMatrix;

	XMFLOAT4X4 xmf4x4View;
	XMStoreFloat4x4(&xmf4x4View, XMMatrixTranspose(mat));
	gd.gpucmd->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4View, 0);

	vec4 pos = gd.viewportArr[0].Camera_Pos;
	pos.w = 1;
	pos *= parentMat;
	matrix mat2 = parentMat;
	//mat2.pos = pos;
	mat2.pos.w = 1;

	mat2.transpose();
	gd.gpucmd->SetGraphicsRoot32BitConstants(1, 16, &mat2, 0);

	gd.gpucmd->SetGraphicsRootDescriptorTable(2, CurrentSkyBox.descindex.hRender.hgpu);

	gd.gpucmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	gd.gpucmd->IASetVertexBuffers(0, 1, &SkyBoxMeshVBView);
	gd.gpucmd->DrawInstanced(36, 1, 0, 0);
}

#pragma endregion

#pragma region ParticleShaderCode
void ParticleShader::InitShader()
{
	CreateRootSignature();
	CreatePipelineState();
}

void ParticleShader::CreateRootSignature()
{
	CD3DX12_ROOT_PARAMETER rootParams[3];

	// t0 : StructuredBuffer<Particle> (VS)
	rootParams[0].InitAsShaderResourceView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

	// b0 : ViewProj + CamRight + CamUp (VS)
	rootParams[1].InitAsConstants(23, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

	// t1 : Fire Texture (PS)
	CD3DX12_DESCRIPTOR_RANGE srvRange;
	srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1); // t1

	rootParams[2].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);

	// s0 : Linear Sampler
	CD3DX12_STATIC_SAMPLER_DESC samplerDesc(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP
	);

	CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
	rsDesc.Init(_countof(rootParams), rootParams, 1, &samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ID3DBlob* sigBlob = nullptr;
	ID3DBlob* errBlob = nullptr;

	HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);

	if (FAILED(hr))
	{
		if (errBlob)
			OutputDebugStringA((char*)errBlob->GetBufferPointer());
		return;
	}

	gd.pDevice->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&ParticleRootSig));

	sigBlob->Release();
	if (errBlob) errBlob->Release();
}

void ParticleShader::CreatePipelineState()
{
	ID3DBlob* vsBlob = nullptr;
	ID3DBlob* psBlob = nullptr;

	Shader::GetShaderByteCode(L"Particle.hlsl", "VS", "vs_5_1", &vsBlob);
	Shader::GetShaderByteCode(L"Particle.hlsl", "PS", "ps_5_1", &psBlob);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = ParticleRootSig;

	// Input 없음 (SV_VertexID)
	psoDesc.InputLayout = { nullptr, 0 };

	psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
	psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	// Rasterizer
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	// Additive Blend 
	D3D12_BLEND_DESC blendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	psoDesc.BlendState = blendDesc;

	// Depth
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDesc.DepthStencilState.DepthEnable = TRUE;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	psoDesc.SampleMask = UINT_MAX;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = gd.MainRenderTarget_PixelFormat;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	psoDesc.SampleDesc.Count = (gd.m_bMsaa4xEnable) ? 4 : 1;
	psoDesc.SampleDesc.Quality = (gd.m_bMsaa4xEnable) ? (gd.m_nMsaa4xQualityLevels - 1) : 0;

	HRESULT hr = gd.pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&ParticlePSO));

	if (vsBlob) vsBlob->Release();
	if (psBlob) psBlob->Release();

	if (FAILED(hr))
		OutputDebugStringA("[ERROR] Particle PSO Create Failed\n");
}

void ParticleShader::Render(ID3D12GraphicsCommandList* cmd, GPUResource* particleBuffer, UINT particleCount)
{
	cmd->SetPipelineState(ParticlePSO);
	cmd->SetGraphicsRootSignature(ParticleRootSig);

	// t0 : Particle Buffer
	cmd->SetGraphicsRootShaderResourceView(0, particleBuffer->resource->GetGPUVirtualAddress());

	// t1 : Fire Texture SRV
	cmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
	cmd->SetGraphicsRootDescriptorTable(2, FireTexture->descindex.hRender.hgpu);

	// b0 : ViewProj + Cam vectors
	matrix viewProj = gd.viewportArr[0].ViewMatrix;
	viewProj *= gd.viewportArr[0].ProjectMatrix;
	viewProj.transpose();

	matrix view = gd.viewportArr[0].ViewMatrix;
	XMMATRIX xmInvView = XMMatrixInverse(nullptr, XMLoadFloat4x4((XMFLOAT4X4*)&view));

	matrix invView;
	XMStoreFloat4x4((XMFLOAT4X4*)&invView, xmInvView);

	XMFLOAT3 camRight = invView.right.f3;
	XMFLOAT3 camUp = invView.up.f3;
	float pad0 = 0.0f;

	cmd->SetGraphicsRoot32BitConstants(1, 16, &viewProj, 0);
	cmd->SetGraphicsRoot32BitConstants(1, 3, &camRight, 16);
	cmd->SetGraphicsRoot32BitConstants(1, 1, &pad0, 19);
	cmd->SetGraphicsRoot32BitConstants(1, 3, &camUp, 20);

	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->DrawInstanced(particleCount * 6, 1, 0, 0);
}

void ParticleCompute::Init(const wchar_t* hlslFile, const char* entry)
{
	// RootSignature 
	CD3DX12_ROOT_PARAMETER params[2];
	params[0].InitAsUnorderedAccessView(0); // u0
	params[1].InitAsConstants(1, 0);        // b0 (dt)

	CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
	rsDesc.Init(2, params);

	ID3DBlob* sig = nullptr;
	D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, nullptr);

	gd.pDevice->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&RootSig));

	sig->Release();

	ID3DBlob* csBlob = nullptr;
	Shader::GetShaderByteCode(hlslFile, entry, "cs_5_1", &csBlob);

	D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = RootSig;
	desc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };

	gd.pDevice->CreateComputePipelineState(&desc, IID_PPV_ARGS(&PSO));

	csBlob->Release();
}

void ParticleCompute::Dispatch(ID3D12GraphicsCommandList* cmd, GPUResource* buffer, UINT count, float dt)
{
	buffer->AddResourceBarrierTransitoinToCommand(cmd, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	cmd->SetPipelineState(PSO);
	cmd->SetComputeRootSignature(RootSig);
	cmd->SetComputeRootUnorderedAccessView(0, buffer->resource->GetGPUVirtualAddress());
	cmd->SetComputeRoot32BitConstants(1, 1, &dt, 0);

	cmd->Dispatch((count + 255) / 256, 1, 1);

	buffer->AddResourceBarrierTransitoinToCommand(cmd, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}
#pragma endregion

#pragma region RaytracingShaderCode
float** RayTracingShader::push_rins_immortal(RayTracingMesh* mesh, matrix mat, LocalRootSigData* LRSdata, int hitGroupShaderIdentifyerIndex)
{
	dbgc[1] += 1;
	//dbgbreak(dbgc[1] == 6167);
	std::unordered_map<ShaderRecord, int>::iterator f;
	
	// 현재는 LRS를 1로 고정해놔서 결국 단일 ShaderRecord를 만드는 작업을 할 것임. 하지만 
	// 언젠가 하나의 메쉬를 여러개의 Record로 나누어야 하는 일이 생긴다면 LRSCount를 조정하면 되겠다.
	int LRSCount = 1; // mesh->subMeshCount;
	static float* RaytracingInputWorldMatptr[1024] = {};
	int curindex[1024] = {};
	void* HGSI = hitGroupShaderIdentifier[hitGroupShaderIdentifyerIndex];
	if (HitGroupShaderTableToIndex.size() == 0) {
		// 셰이더 테이블이 없을때
		for (int i = 0; i < LRSCount; ++i) {
			//LRSdata[i].IBOffset = mesh->IBStartOffset[i] / sizeof(UINT);
			curindex[i] = hitGroupShaderTable.m_shaderRecords.size();
			ShaderRecord sr = ShaderRecord(HGSI, shaderIdentifierSize, &LRSdata[i], sizeof(LocalRootSigData));
			hitGroupShaderTable.push_back(sr);
			InsertShaderRecord(sr, curindex[i]);
			HitGroupShaderTableImmortalSize += 1;
		}
		goto PUSH_INSTACEDESC;
	}

	for (int i = 0; i < LRSCount; ++i) {
		//LRSdata[i].IBOffset = mesh->IBStartOffset[i] / sizeof(UINT);
		ShaderRecord sr = ShaderRecord(HGSI, shaderIdentifierSize, &LRSdata[i], sizeof(LocalRootSigData));
		f = HitGroupShaderTableToIndex.find(sr);

		if (f == HitGroupShaderTableToIndex.end()) {
			curindex[i] = hitGroupShaderTable.m_shaderRecords.size();
			hitGroupShaderTable.push_back(sr);
			InsertShaderRecord(sr, curindex[i]);
			HitGroupShaderTableImmortalSize += 1;
		}
		else {
			curindex[i] = f->second;
			if (curindex[i] > HitGroupShaderTableImmortalSize) {
				curindex[i] = hitGroupShaderTable.m_shaderRecords.size();
				ShaderRecord sr = ShaderRecord(HGSI, shaderIdentifierSize, &LRSdata[i], sizeof(LocalRootSigData));
				hitGroupShaderTable.push_back(sr);
				HitGroupShaderTableToIndex[sr] = curindex[i];
				HitGroupShaderTableImmortalSize += 1;
			}
			else {
				curindex[i] = HitGroupShaderTableToIndex[sr];
			}
		}
	}

PUSH_INSTACEDESC:
	HitGroupShaderTableSize = HitGroupShaderTableImmortalSize;

	D3D12_RAYTRACING_INSTANCE_DESC* insarr = &TLAS_InstanceDescs_MappedData[TLAS_InstanceDescs_ImmortalSize];
	mat.transpose();
	for (int i = 0; i < LRSCount; ++i) {
		insarr[i] = mesh->MeshDefaultInstanceData;
		memcpy(insarr[i].Transform, &mat, 12 * sizeof(float));
		insarr[i].InstanceContributionToHitGroupIndex = curindex[i];
		RaytracingInputWorldMatptr[i] = (float*)insarr[i].Transform;
	}
	TLAS_InstanceDescs_ImmortalSize += LRSCount;
	TLAS_InstanceDescs_Size = TLAS_InstanceDescs_ImmortalSize;
	//dbgbreak(TLAS_InstanceDescs_MappedData[0].AccelerationStructure == 0);

	return RaytracingInputWorldMatptr;
}

void RayTracingShader::clear_rins()
{
	hitGroupShaderTable.m_shaderRecords.resize(HitGroupShaderTableImmortalSize);
	HitGroupShaderTableSize = HitGroupShaderTableImmortalSize;
	TLAS_InstanceDescs_Size = TLAS_InstanceDescs_ImmortalSize;
}

float** RayTracingShader::push_rins(RayTracingMesh* mesh, matrix mat, LocalRootSigData* LRSdata, int hitGroupShaderIdentifyerIndex)
{
	std::unordered_map<ShaderRecord, int>::iterator f;
	int LRSCount = 1; //mesh->subMeshCount;
	static float* RaytracingInputWorldMatptr[1024] = {};
	int curindex[1024] = {};
	void* HGSI = hitGroupShaderIdentifier[hitGroupShaderIdentifyerIndex];
	if (HitGroupShaderTableToIndex.size() == 0) {
		// 셰이더 테이블이 없을때
		for (int i = 0; i < LRSCount; ++i) {
			//LRSdata[i].IBOffset = mesh->IBStartOffset[i] / sizeof(UINT);
			curindex[i] = hitGroupShaderTable.m_shaderRecords.size();
			ShaderRecord sr = ShaderRecord(HGSI, shaderIdentifierSize, &LRSdata[i], sizeof(LocalRootSigData));
			hitGroupShaderTable.push_back(sr);
			InsertShaderRecord(sr, curindex[i]);
			HitGroupShaderTableSize += 1;
		}
		goto PUSH_INSTACEDESC;
	}

	for (int i = 0; i < LRSCount; ++i) {
		//LRSdata[i].IBOffset = mesh->IBStartOffset[i] / sizeof(UINT);
		ShaderRecord sr = ShaderRecord(HGSI, shaderIdentifierSize, &LRSdata[i], sizeof(LocalRootSigData));
		f = HitGroupShaderTableToIndex.find(sr);

		if (f == HitGroupShaderTableToIndex.end()) {
			curindex[i] = hitGroupShaderTable.m_shaderRecords.size();
			ShaderRecord sr = ShaderRecord(HGSI, shaderIdentifierSize, &LRSdata[i], sizeof(LocalRootSigData));
			hitGroupShaderTable.push_back(sr);
			InsertShaderRecord(sr, curindex[i]);
			HitGroupShaderTableSize += 1;
		}
		else {
			curindex[i] = f->second;
			if (curindex[i] > HitGroupShaderTableSize) {
				curindex[i] = hitGroupShaderTable.m_shaderRecords.size();
				ShaderRecord sr = ShaderRecord(HGSI, shaderIdentifierSize, &LRSdata[i], sizeof(LocalRootSigData));
				hitGroupShaderTable.push_back(sr);
				HitGroupShaderTableToIndex[sr] = curindex[i];
				HitGroupShaderTableSize += 1;
			}
			else {
				curindex[i] = HitGroupShaderTableToIndex[sr];
			}
		}
	}

PUSH_INSTACEDESC:
	D3D12_RAYTRACING_INSTANCE_DESC* insarr = &TLAS_InstanceDescs_MappedData[TLAS_InstanceDescs_ImmortalSize];
	for (int i = 0; i < LRSCount; ++i) {
		insarr[i] = mesh->MeshDefaultInstanceData;
		mat.transpose();
		memcpy(insarr[i].Transform, &mat, 12 * sizeof(float));
		insarr[i].InstanceContributionToHitGroupIndex = curindex[i];
		RaytracingInputWorldMatptr[i] = (float*)insarr[i].Transform;
	}
	TLAS_InstanceDescs_Size += LRSCount;

	return RaytracingInputWorldMatptr;
}

void RayTracingShader::InsertShaderRecord(ShaderRecord sr, int index)
{
	LocalRootSigData* newLRS = new LocalRootSigData();
	memcpy(newLRS, sr.localRootArguments.ptr, sr.localRootArguments.size);
	sr.localRootArguments.ptr = newLRS;
	HitGroupShaderTableToIndex.insert(pair<ShaderRecord, int>(sr, index));
}

void RayTracingShader::Init()
{
	CreateGlobalRootSignature();
	CreateLocalRootSignatures();
	CreatePipelineState();
	InitAccelationStructure();
	InitShaderTable();

	MySkinMeshModifyShader = new SkinMeshModifyShader();
	MySkinMeshModifyShader->InitShader();
}

void RayTracingShader::ReInit() {
	CreateGlobalRootSignature();
	//CreateLocalRootSignatures();
	CreatePipelineState();
	//InitAccelationStructure();
	InitShaderTable();
}

void RayTracingShader::CreateGlobalRootSignature()
{
	//gd.ShaderVisibleDescPool.TextureSRVCap 에 따라 다른 SRV Range를 가지도록 만들어야 함.
	if (pGlobalRootSignature) {
		pGlobalRootSignature->Release();
		pGlobalRootSignature = nullptr;
	}

	//GRS
	CD3DX12_DESCRIPTOR_RANGE RT_UAVDescriptor;
	RT_UAVDescriptor.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0); // u0

	CD3DX12_DESCRIPTOR_RANGE DS_UAVDescriptor2;
	DS_UAVDescriptor2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1); // u1

	CD3DX12_DESCRIPTOR_RANGE srvVB;
	srvVB.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1); // t1
	CD3DX12_DESCRIPTOR_RANGE srvIB;
	srvIB.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2); // t2
	CD3DX12_DESCRIPTOR_RANGE ranges[2] = { srvVB, srvIB };

	CD3DX12_DESCRIPTOR_RANGE srvSkyBox;
	srvSkyBox.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3); // t3
	CD3DX12_DESCRIPTOR_RANGE ranges1[1] = { srvSkyBox };

	CD3DX12_DESCRIPTOR_RANGE srvSkinMeshVB;
	srvSkinMeshVB.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4); // t4
	CD3DX12_DESCRIPTOR_RANGE ranges2[1] = { srvSkinMeshVB };

	CD3DX12_DESCRIPTOR_RANGE srvStructuredBuffer_Material;
	srvStructuredBuffer_Material.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5); // t5
	CD3DX12_DESCRIPTOR_RANGE ranges3[1] = { srvStructuredBuffer_Material };

	CD3DX12_DESCRIPTOR_RANGE srvTextureArr;
	srvTextureArr.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, gd.ShaderVisibleDescPool.TextureSRVCap, 6); // t5
	CD3DX12_DESCRIPTOR_RANGE ranges4[1] = { srvTextureArr };

	constexpr int ParamCount = 9;
	CD3DX12_ROOT_PARAMETER rootParameters[ParamCount];
	rootParameters[0].InitAsDescriptorTable(1, &RT_UAVDescriptor); // RT OutputView u0
	rootParameters[1].InitAsDescriptorTable(1, &DS_UAVDescriptor2); // DS OutputView u1
	rootParameters[2].InitAsShaderResourceView(0); // AS t0
	rootParameters[3].InitAsConstantBufferView(0, 0); // Camera CBV b0
	rootParameters[4].InitAsDescriptorTable(2, ranges); // Vertex / IndexBuffers t1 t2
	rootParameters[5].InitAsDescriptorTable(1, ranges1); // SkyBoxCubeMap
	rootParameters[6].InitAsDescriptorTable(1, ranges2); // SkinMeshVertex
	rootParameters[7].InitAsDescriptorTable(1, ranges3); // srvStructuredBuffer_Material
	rootParameters[8].InitAsDescriptorTable(1, ranges4); // srvTextureArr

	/// default sampler
	D3D12_STATIC_SAMPLER_DESC sampler = {};
	// sampler register index.
	sampler.ShaderRegister = 0;
	// setting
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.MipLODBias = 0.0f;
	sampler.MaxAnisotropy = 16;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	sampler.MinLOD = -FLT_MAX;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

	CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
	globalRootSignatureDesc.NumStaticSamplers = 1;
	globalRootSignatureDesc.pStaticSamplers = &sampler;
	gd.raytracing.SerializeAndCreateRaytracingRootSignature(globalRootSignatureDesc, &pGlobalRootSignature);
}

void RayTracingShader::CreateLocalRootSignatures()
{
	//[0] LRS
	ID3D12RootSignature* rootsig = nullptr;
	CD3DX12_ROOT_PARAMETER rootParameters[1];
	int siz = SizeOfInUint32(LocalRootSigData);
	rootParameters[0].InitAsConstants(siz, 1, 0);
	CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
	localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
	gd.raytracing.SerializeAndCreateRaytracingRootSignature(localRootSignatureDesc, &rootsig);
	pLocalRootSignature.push_back(rootsig);
}

void RayTracingShader::CreatePipelineState()
{
	if (RTPSO) {
		RTPSO->Release();
		RTPSO = nullptr;
	}

	if (hitGroupShaderIdentifier == nullptr) {
		hitGroupShaderIdentifier = new void* [128];
	}
	
	HRESULT hr;
	CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

	// 1. DXIL library
	// 1-1. Load RayTracing Shader
	vector<LPWSTR> macros;
	wchar_t MacroStr[512];
	wsprintfW(MacroStr, L"MAX_TEXTURES=%d", gd.ShaderVisibleDescPool.TextureSRVCap);
	macros.push_back(MacroStr);

	SHADER_HANDLE* shaderHandle = gd.raytracing.CreateShaderDXC(L"Resources\\Shaders\\RayTracing", L"Raytracing.hlsl", L"", L"lib_6_3", 0, &macros);
	// 1-2. Set DXIL Lib
	CD3DX12_DXIL_LIBRARY_SUBOBJECT* lib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE((void*)shaderHandle->pCodeBuffer, shaderHandle->dwCodeSize);
	lib->SetDXILLibrary(&libdxil);

	const wchar_t* RayGenShaderName = L"MyRaygenShader";
	const wchar_t* ClosestHitShaderName = L"MyClosestHitShader";
	const wchar_t* SkinMeshClosestHitShaderName = L"MySkinMeshClosestHitShader";
	const wchar_t* MissShaderName = L"MyMissShader";

	lib->DefineExport(RayGenShaderName);
	lib->DefineExport(ClosestHitShaderName);
	lib->DefineExport(SkinMeshClosestHitShaderName);
	lib->DefineExport(MissShaderName);

	// 2. Hit Group
	CD3DX12_HIT_GROUP_SUBOBJECT* hitGroup[128];
	hitGroup[0] = raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
	hitGroup[0]->SetClosestHitShaderImport(ClosestHitShaderName);
	hitGroup[0]->SetHitGroupExport(L"MyHitGroup");
	hitGroup[0]->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

	hitGroup[1] = raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
	hitGroup[1]->SetClosestHitShaderImport(SkinMeshClosestHitShaderName);
	hitGroup[1]->SetHitGroupExport(L"MySkinMeshHitGroup");
	hitGroup[1]->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

	// 3. Shader config
	CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT* shaderConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	UINT payloadSize = 16 * sizeof(float);   // float4 color + float ShadowHit + padding + depth, stecil, extra2
	UINT attributeSize = 2 * sizeof(float); // float2 barycentrics
	shaderConfig->Config(payloadSize, attributeSize);

	// 4. Local root signature and shader association
	// 4-1. Local root signature (ray gen shader)
	{
		CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT* localRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
		localRootSignature->SetRootSignature(pLocalRootSignature[0]);
		// Shader association
		CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT* rootSignatureAssociation = raytracingPipeline.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
		rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
		rootSignatureAssociation->AddExport(ClosestHitShaderName);

		CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT* rootSignatureAssociation2 = raytracingPipeline.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
		rootSignatureAssociation2->SetSubobjectToAssociate(*localRootSignature);
		rootSignatureAssociation2->AddExport(SkinMeshClosestHitShaderName);
	}

	// Global root signature
	CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT* globalRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	globalRootSignature->SetRootSignature(pGlobalRootSignature);

	// Pipeline config
	CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT* pipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	UINT maxRecursionDepth = 2; // ~ primary rays only. (for shadow)
	pipelineConfig->Config(maxRecursionDepth);

#if _DEBUG
	PrintStateObjectDesc(raytracingPipeline);
#endif

	// Create the state object.
	hr = gd.raytracing.dxrDevice->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&RTPSO));
	if (FAILED(hr)) {
		throw "Couldn't create DirectX Raytracing state object.";
	}
}

void RayTracingShader::InitAccelationStructure()
{
	if (gd.raytracing.ASBuild_ScratchResource == nullptr) {
		AllocateUAVBuffer(gd.raytracing.dxrDevice, gd.raytracing.ASBuild_ScratchResource_Maxsiz, &gd.raytracing.ASBuild_ScratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");
	}
	// Reset the command list for the acceleration structure construction.

	ID3D12Device5* device = gd.raytracing.dxrDevice;
	//ID3D12GraphicsCommandList4* commandList = gd.raytracing.dxrCommandList;
	//ID3D12CommandQueue* commandQueue = gd.pCommandQueue;
	//ID3D12CommandAllocator* commandAllocator = gd.pCommandAllocator;

	//if (gd.isCmdClose == false)
	//{
	//	gd.CmdClose(commandList);
	//	ID3D12CommandList* ppd3dCommandLists[] = { commandList };
	//	commandQueue->ExecuteCommandLists(1, ppd3dCommandLists);
	//	gd.WaitGPUComplete();
	//}
	//gd.CmdReset(commandList, commandAllocator);

	//1. gpu memory allocation
	// Allocate TLAS UAV Buffer (in Max Capacity)
	D3D12_RAYTRACING_INSTANCE_DESC* copyData = nullptr;
	if (TLAS) {
		copyData = new D3D12_RAYTRACING_INSTANCE_DESC[TLAS_InstanceDescs_Capacity];
		memcpy_s(copyData, TLAS_InstanceDescs_Size * sizeof(D3D12_RAYTRACING_INSTANCE_DESC), TLAS_InstanceDescs_MappedData, TLAS_InstanceDescs_Size * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
		TLAS->Release();
		TLAS_InstanceDescs_Res->Unmap(0, nullptr);
		TLAS_InstanceDescs_Res->Release();
	}

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS topLevelInputs_MAX = {};
	topLevelInputs_MAX.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	topLevelInputs_MAX.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	topLevelInputs_MAX.NumDescs = TLAS_InstanceDescs_Capacity;
	topLevelInputs_MAX.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo_MAX = {};
	device->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs_MAX, &topLevelPrebuildInfo_MAX);
	if (topLevelPrebuildInfo_MAX.ResultDataMaxSizeInBytes <= 0) {
		throw "GetRaytracingAccelerationStructurePrebuildInfo Failed.";
	}
	// Allocate resources for acceleration structures.
	// Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
	// Default heap is OK since the application doesn't need CPU read/write access to them. 
	// The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
	// and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
	//  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
	//  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.
	D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	AllocateUAVBuffer(device, topLevelPrebuildInfo_MAX.ResultDataMaxSizeInBytes, &TLAS, initialResourceState, L"TopLevelAccelerationStructure");

	// Allocate TLAS Instance Desc Arr Buffer (UploadBuffer)
	// Create an instance desc for the bottom-level acceleration structure.
	//D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
	//instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1;
	//instanceDesc.InstanceMask = 1;
	//instanceDesc.AccelerationStructure = TestMesh->BLAS->GetGPUVirtualAddress();
	//instanceDesc.InstanceContributionToHitGroupIndex = 0;
	TLAS_InstanceDescs_MappedData = (D3D12_RAYTRACING_INSTANCE_DESC*)AllocateUploadBuffer(device, nullptr, sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * TLAS_InstanceDescs_Capacity, &TLAS_InstanceDescs_Res, L"InstanceDescs", false);
	if (copyData) {
		memcpy_s(TLAS_InstanceDescs_MappedData, TLAS_InstanceDescs_Size * sizeof(D3D12_RAYTRACING_INSTANCE_DESC), copyData, TLAS_InstanceDescs_Size * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
		delete[] copyData;
		copyData = nullptr;
	}
}

void RayTracingShader::BuildAccelationStructure()
{
	ID3D12Device5* device = gd.raytracing.dxrDevice;
	ID3D12GraphicsCommandList4* commandList = gd.raytracing.dxrCommandList;

	if (gd.gpucmd.isClose == false)
	{
		gd.gpucmd.Close(true);
		gd.gpucmd.Execute(true);
		gd.WaitGPUComplete();
	}
	gd.gpucmd.Reset(true);

	//2. real build start
	// Get required sizes for an acceleration structure.
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS topLevelInputs = {};
	topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	topLevelInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	topLevelInputs.NumDescs = TLAS_InstanceDescs_Size;
	topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
	device->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
	if (topLevelPrebuildInfo.ResultDataMaxSizeInBytes <= 0) {
		throw "GetRaytracingAccelerationStructurePrebuildInfo Failed.";
	}

	// Top Level Acceleration Structure desc
	{
		topLevelInputs.InstanceDescs = TLAS_InstanceDescs_Res->GetGPUVirtualAddress();
		TLASBuildDesc.Inputs = topLevelInputs;
		TLASBuildDesc.DestAccelerationStructureData = TLAS->GetGPUVirtualAddress();
		TLASBuildDesc.ScratchAccelerationStructureData = gd.raytracing.ASBuild_ScratchResource->GetGPUVirtualAddress();
	}

	// Build acceleration structure.
	commandList->BuildRaytracingAccelerationStructure(&TLASBuildDesc, 0, nullptr);

	gd.gpucmd.Close(true);

	// Kick off acceleration structure construction.
	gd.gpucmd.Execute(true);
	gd.gpucmd.WaitGPUComplete();
}

void RayTracingShader::InitShaderTable()
{
	HRESULT hr;

	ID3D12Device5* device = gd.raytracing.dxrDevice;

	const wchar_t* RayGenShaderName = L"MyRaygenShader";
	const wchar_t* ClosestHitShaderName = L"MyClosestHitShader";
	const wchar_t* MissShaderName = L"MyMissShader";
	const wchar_t* HitGroupName = L"MyHitGroup";

	void* rayGenShaderIdentifier;
	void* missShaderIdentifier;

	void** Past_hitGroupShaderIdentifier = new void* [128];
	for (int i = 0;i < 128;++i) {
		Past_hitGroupShaderIdentifier[i] = hitGroupShaderIdentifier[i];
	}

	auto GetShaderIdentifiers = [&](auto* stateObjectProperties)
		{
			rayGenShaderIdentifier = stateObjectProperties->GetShaderIdentifier(RayGenShaderName);
			missShaderIdentifier = stateObjectProperties->GetShaderIdentifier(MissShaderName);
			hitGroupShaderIdentifier[0] = stateObjectProperties->GetShaderIdentifier(L"MyHitGroup");
			hitGroupShaderIdentifier[1] = stateObjectProperties->GetShaderIdentifier(L"MySkinMeshHitGroup");
		};

	// Get shader identifiers.

	ID3D12StateObjectProperties* stateObjectProperties = nullptr;
	hr = RTPSO->QueryInterface<ID3D12StateObjectProperties>(&stateObjectProperties);
	if (FAILED(hr) || stateObjectProperties == nullptr) {
		throw "RTPSO QueryInterface to ID3D12StateObjectProperties Failed.";
	}
	GetShaderIdentifiers(stateObjectProperties);
	shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

	// Ray gen shader table
	{
		UINT numShaderRecords = 1;
		UINT shaderRecordSize = shaderIdentifierSize;
		ShaderTable rayGenShaderTable(device, numShaderRecords, shaderRecordSize, L"RayGenShaderTable");
		rayGenShaderTable.push_back(ShaderRecord(rayGenShaderIdentifier, shaderIdentifierSize));
		RayGenShaderTable = rayGenShaderTable.GetResource();
	}

	// Miss shader table
	{
		UINT numShaderRecords = 1;
		UINT shaderRecordSize = shaderIdentifierSize;
		ShaderTable missShaderTable(device, numShaderRecords, shaderRecordSize, L"MissShaderTable");
		missShaderTable.push_back(ShaderRecord(missShaderIdentifier, shaderIdentifierSize));
		MissShaderTable = missShaderTable.GetResource();
	}

	// Hit group shader table
	// RayTracing Shader가 Reinit 되면 다시 셰이더 테이블로 들어간 것들을 모조리 Shader Identifyer를 변경해야 함. 
	// 아예 다른 셰이더가 되었으니까.
	if(shaderTableInit == false)
	{
		LocalRootSigData lrsData{ 0, 0, 0 };

		UINT numShaderRecords = HitGroupShaderTableCapavity;
		UINT shaderRecordSize = shaderIdentifierSize + sizeof(LocalRootSigData);
		hitGroupShaderTable = ShaderTable(device, numShaderRecords, shaderRecordSize, L"HitGroupShaderTable");

		//hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderIdentifier, shaderIdentifierSize, &lrsData, sizeof(LocalRootSigData)));

		HitGroupShaderTable = hitGroupShaderTable.GetResource();
		shaderTableInit = true;
	}
	else {
		dbgc[20] += 1;
		vector<ShaderRecord> srvec;

		vector<char*> lrsvec;
		for (int i = 0;i < hitGroupShaderTable.m_shaderRecords.size();++i) {
			srvec.push_back(hitGroupShaderTable.m_shaderRecords[i]);
			char* lrs = new char[srvec[i].localRootArguments.size];
			memcpy_s(lrs, srvec[i].localRootArguments.size, srvec[i].localRootArguments.ptr, srvec[i].localRootArguments.size);
			lrsvec.push_back(lrs);
		}
		LocalRootSigData lrsData{ 0, 0, 0 };

		UINT numShaderRecords = HitGroupShaderTableCapavity;
		UINT shaderRecordSize = shaderIdentifierSize + sizeof(LocalRootSigData);
		hitGroupShaderTable = ShaderTable(device, numShaderRecords, shaderRecordSize, L"HitGroupShaderTable");
		HitGroupShaderTable = hitGroupShaderTable.GetResource();

		for (int i = 0;i < srvec.size();++i) {
			for (int k = 0;k < 128;++k) {
				if (srvec[i].shaderIdentifier.ptr == Past_hitGroupShaderIdentifier[k]) {
					srvec[i].shaderIdentifier.ptr = hitGroupShaderIdentifier[k];
					srvec[i].shaderIdentifier.size = shaderIdentifierSize;
					srvec[i].localRootArguments.ptr = lrsvec[i];
					break;
				}
			}
			
			hitGroupShaderTable.push_back(srvec[i]);
		}

		//해제
		for (int i = 0;i < lrsvec.size();++i) {
			delete[] lrsvec[i];
		}
		lrsvec.clear();

		//이전에 썼던 ShaderTable은 COM으로 이루어져 있어서 아마 자동으로 해제될 것임.
	}
}

void RayTracingShader::BuildShaderTable()
{
	HitGroupShaderTable = hitGroupShaderTable.GetResource();
}

void RayTracingShader::SkinMeshModify()
{
	RebuildBLASBuffer.clear();
	HRESULT hResult = gd.CScmd.Reset();
	
	gd.CScmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);

	game.renderViewPort = &gd.viewportArr[0];
	SkinMeshGameObject::collection.clear();
	SkinMeshGameObject::CurrentRenderFunc = &SkinMeshGameObject::CollectSkinMeshObject;
	game.RenderTour<true>();
	gd.CScmd.SetShader(game.MyAnimationBlendingShader);
	for (int i = 0;i < SkinMeshGameObject::collection.size();++i) {
		SkinMeshGameObject::collection[i]->BlendingAnimation();
	}
	gd.CScmd.SetShader(game.MyHBoneLocalToWorldShader);
	for (int i = 0;i < SkinMeshGameObject::collection.size();++i) {
		SkinMeshGameObject::collection[i]->ModifyLocalToWorld();
	}
	MySkinMeshModifyShader->Add_RegisterShaderCommand(gd.CScmd);
	for (int i = 0;i < SkinMeshGameObject::collection.size();++i) {
		SkinMeshGameObject::collection[i]->ModifyVertexs();
	}
	hResult = gd.CScmd.Close();
	gd.CScmd.Execute();
	gd.CScmd.WaitGPUComplete();

	//BLAS 빌드
	if (gd.gpucmd.isClose) {
		gd.gpucmd.Reset(true);
	}

	gd.raytracing.UsingScratchSize = 0;
	for (int i = 0; i < RebuildBLASBuffer.size(); ++i) {
		SkinMeshGameObject* smgo = (SkinMeshGameObject*)RebuildBLASBuffer[i];
		Model* model = smgo->shape.GetModel();
		for (int k = 0; k < model->mNumSkinMesh; ++k) {
			smgo->modifyMeshes[k].UAV_BLAS_Refit();
		}
	}
	gd.gpucmd.Close(true);
	gd.gpucmd.Execute(true);
	gd.WaitGPUComplete();
}

void RayTracingShader::PrepareRender()
{
	SkinMeshModify();
	BuildAccelationStructure();
	BuildShaderTable();
}

void SkinMeshModifyShader::InitShader()
{
	CreateRootSignature();
	CreatePipelineState();
}

void SkinMeshModifyShader::CreateRootSignature()
{
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;
	D3D12_ROOT_PARAMETER1 rootParam[RootParamId::Normal_RootParamCapacity] = {};

	rootParam[Const_OutputVertexBufferSize] = RootParam1::Const32s(GRegID('b', 0), 1, D3D12_SHADER_VISIBILITY_ALL, 0);
	RootParam1 DescTable1;
	DescTable1.PushDescRange(GRegID('b', 1), "CBV", 2, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	DescTable1.DescTable(D3D12_SHADER_VISIBILITY_ALL);
	rootParam[RootParamId::CBVTable_OffsetMatrixs_ToWorldMatrixs] = DescTable1;
	RootParam1 DescTable2;
	DescTable2.PushDescRange(GRegID('t', 0), "SRV", 2, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	DescTable2.DescTable(D3D12_SHADER_VISIBILITY_ALL);
	rootParam[RootParamId::SRVTable_SourceVertexAndBoneData] = DescTable2;
	RootParam1 DescTable3;
	DescTable3.PushDescRange(GRegID('u', 0), "UAV", 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
	DescTable3.DescTable(D3D12_SHADER_VISIBILITY_ALL);
	rootParam[RootParamId::UAVTable_OutputVertexData] = DescTable3;

	D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT/* |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS*/;

	rootSigDesc1.NumParameters = Normal_RootParamCapacity;
	rootSigDesc1.pParameters = rootParam;
	rootSigDesc1.NumStaticSamplers = 0;
	rootSigDesc1.Flags = d3dRootSignatureFlags;

	ID3DBlob* pd3dSignatureBlob = NULL;
	ID3DBlob* pd3dErrorBlob = NULL;
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_rootSigDesc;
	versioned_rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	versioned_rootSigDesc.Desc_1_1 = rootSigDesc1;
	D3D12SerializeVersionedRootSignature(&versioned_rootSigDesc, &pd3dSignatureBlob, &pd3dErrorBlob);
	gd.pDevice->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(),
		pd3dSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void
			**)&pRootSignature);
	if (pd3dErrorBlob) pd3dErrorBlob->Release();
	if (pd3dSignatureBlob) pd3dSignatureBlob->Release();
}

void SkinMeshModifyShader::CreatePipelineState()
{
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dComputeShaderBlob = NULL;
	D3D12_COMPUTE_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_COMPUTE_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature;
	gPipelineStateDesc.NodeMask = 0;

	//Compute Shader
	gPipelineStateDesc.CS = Shader::GetShaderByteCode(L"Resources/Shaders/AnimationCompute/ComputeSkinMeshGeometry.hlsl", "CSMain", "cs_5_1", &pd3dComputeShaderBlob);

	gd.pDevice->CreateComputePipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState);
	if (pd3dComputeShaderBlob) pd3dComputeShaderBlob->Release();
}

void SkinMeshModifyShader::Add_RegisterShaderCommand(ID3D12GraphicsCommandList* cmd)
{
	cmd->SetComputeRootSignature(pRootSignature);
	cmd->SetPipelineState(pPipelineState);
}

void SkinMeshModifyShader::Release()
{
	if (pRootSignature) pRootSignature->Release();
	if (pPipelineState) pPipelineState->Release();
}
#pragma endregion

#pragma region BluringShaderCode
void ComputeTestShader::InitShader() {
	CreateRootSignature();
	CreatePipelineState();
}
void ComputeTestShader::CreateRootSignature() {
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;

	constexpr int RootParamCount = 4;
	D3D12_ROOT_PARAMETER1 rootParam[RootParamCount] = {};

	rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParam[0].Constants.Num32BitValues = 2;
	rootParam[0].Constants.ShaderRegister = 0; // b0 : Camera Matrix (Projection, View) + Camera Positon
	rootParam[0].Constants.RegisterSpace = 0;

	rootParam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParam[1].DescriptorTable.NumDescriptorRanges = 1;
	D3D12_DESCRIPTOR_RANGE1 ranges[1];
	ranges[0].BaseShaderRegister = 0; // u0 prev renderTarget
	ranges[0].RegisterSpace = 0;
	ranges[0].NumDescriptors = 1;
	ranges[0].OffsetInDescriptorsFromTableStart = 0;
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
	rootParam[1].DescriptorTable.pDescriptorRanges = &ranges[0];

	rootParam[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParam[2].DescriptorTable.NumDescriptorRanges = 1;
	D3D12_DESCRIPTOR_RANGE1 ranges2[1];
	ranges2[0].BaseShaderRegister = 1; // t1 prev renderTarget
	ranges2[0].RegisterSpace = 0;
	ranges2[0].NumDescriptors = 1;
	ranges2[0].OffsetInDescriptorsFromTableStart = 0;
	ranges2[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges2[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
	rootParam[2].DescriptorTable.pDescriptorRanges = &ranges2[0];

	rootParam[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParam[3].DescriptorTable.NumDescriptorRanges = 1;
	D3D12_DESCRIPTOR_RANGE1 ranges3[1];
	ranges3[0].BaseShaderRegister = 1; // u1 blur output
	ranges3[0].RegisterSpace = 0;
	ranges3[0].NumDescriptors = 1;
	ranges3[0].OffsetInDescriptorsFromTableStart = 0;
	ranges3[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	ranges3[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
	rootParam[3].DescriptorTable.pDescriptorRanges = &ranges3[0];

	D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT/* |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS*/;

		/// default sampler
	D3D12_STATIC_SAMPLER_DESC sampler = {};
	// sampler register index.
	sampler.ShaderRegister = 0;
	// setting
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.MipLODBias = 0.0f;
	sampler.MaxAnisotropy = 16;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	sampler.MinLOD = -FLT_MAX;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

	rootSigDesc1.NumParameters = RootParamCount;
	rootSigDesc1.pParameters = rootParam;
	rootSigDesc1.NumStaticSamplers = 1; // question002 : what is static samplers?
	rootSigDesc1.pStaticSamplers = &sampler;
	rootSigDesc1.Flags = d3dRootSignatureFlags;

	ID3DBlob* pd3dSignatureBlob = NULL;
	ID3DBlob* pd3dErrorBlob = NULL;
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_rootSigDesc;
	versioned_rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	versioned_rootSigDesc.Desc_1_1 = rootSigDesc1;
	D3D12SerializeVersionedRootSignature(&versioned_rootSigDesc, &pd3dSignatureBlob, &pd3dErrorBlob);
	gd.pDevice->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(),
		pd3dSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void
			**)&pRootSignature);
	if (pd3dErrorBlob) pd3dErrorBlob->Release();
	if (pd3dSignatureBlob) pd3dSignatureBlob->Release();
}
void ComputeTestShader::CreatePipelineState() {
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dComputeShaderBlob = NULL;
	D3D12_COMPUTE_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_COMPUTE_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature;
	gPipelineStateDesc.NodeMask = 0;

	//Compute Shader
	gPipelineStateDesc.CS = Shader::GetShaderByteCode(L"Resources/Shaders/TestComputeShader.hlsl", "CSMain", "cs_5_1", &pd3dComputeShaderBlob);

	gd.pDevice->CreateComputePipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState);
	if (pd3dComputeShaderBlob) pd3dComputeShaderBlob->Release();
}
void ComputeTestShader::Add_RegisterShaderCommand(GPUCmd& cmd, ShaderType reg) {
	cmd->SetComputeRootSignature(pRootSignature);
	cmd->SetPipelineState(pPipelineState);
}
void ComputeTestShader::Release() {
	if (pRootSignature) pRootSignature->Release();
	if (pPipelineState) pPipelineState->Release();
}
#pragma endregion

#pragma region AnimationBlendingShaderCode
void AnimationBlendingShader::InitShader() {
	CreateRootSignature();
	CreatePipelineState();
}
void AnimationBlendingShader::CreateRootSignature() {
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;
	D3D12_ROOT_PARAMETER1 rootParam[RootParamId::Normal_RootParamCapacity] = {};

	RootParam1 DescTable0;
	DescTable0.PushDescRange(GRegID('b', 0), "CBV", 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	DescTable0.DescTable(D3D12_SHADER_VISIBILITY_ALL);
	rootParam[RootParamId::CBVTable_CBStruct] = DescTable0;
	
	RootParam1 DescTable1;
	DescTable1.PushDescRange(GRegID('t', 0), "SRV", 4, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	DescTable1.DescTable(D3D12_SHADER_VISIBILITY_ALL);
	rootParam[RootParamId::SRVTable_Animation1to4] = DescTable1;

	RootParam1 DescTable2;
	DescTable2.PushDescRange(GRegID('t', 4), "SRV", 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	DescTable2.DescTable(D3D12_SHADER_VISIBILITY_ALL);
	rootParam[RootParamId::SRVTable_HumanoidToNodeindex] = DescTable2;

	RootParam1 DescTable3;
	DescTable3.PushDescRange(GRegID('u', 0), "UAV", 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
	DescTable3.DescTable(D3D12_SHADER_VISIBILITY_ALL);
	rootParam[RootParamId::UAVTable_Out_LocalMatrix] = DescTable3;

	D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT/* |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS*/;

	rootSigDesc1.NumParameters = Normal_RootParamCapacity;
	rootSigDesc1.pParameters = rootParam;
	rootSigDesc1.NumStaticSamplers = 0; // question002 : what is static samplers?
	rootSigDesc1.pStaticSamplers = nullptr;
	rootSigDesc1.Flags = d3dRootSignatureFlags;

	ID3DBlob* pd3dSignatureBlob = NULL;
	ID3DBlob* pd3dErrorBlob = NULL;
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_rootSigDesc;
	versioned_rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	versioned_rootSigDesc.Desc_1_1 = rootSigDesc1;
	D3D12SerializeVersionedRootSignature(&versioned_rootSigDesc, &pd3dSignatureBlob, &pd3dErrorBlob);
	gd.pDevice->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(),
		pd3dSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void
			**)&pRootSignature);
	if (pd3dErrorBlob) pd3dErrorBlob->Release();
	if (pd3dSignatureBlob) pd3dSignatureBlob->Release();
}
void AnimationBlendingShader::CreatePipelineState() {
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dComputeShaderBlob = NULL;
	D3D12_COMPUTE_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_COMPUTE_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature;
	gPipelineStateDesc.NodeMask = 0;

	//Compute Shader
	gPipelineStateDesc.CS = Shader::GetShaderByteCode(L"Resources/Shaders/AnimationCompute/ModifyBoneAnimLocalMatrix.hlsl", "CSMain", "cs_5_1", &pd3dComputeShaderBlob);

	gd.pDevice->CreateComputePipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState);
	if (pd3dComputeShaderBlob) pd3dComputeShaderBlob->Release();
}

void AnimationBlendingShader::Add_RegisterShaderCommand(GPUCmd& cmd, ShaderType reg) {
	cmd->SetComputeRootSignature(pRootSignature);
	cmd->SetPipelineState(pPipelineState);
}
void AnimationBlendingShader::Release() {
	if (pRootSignature) pRootSignature->Release();
	if (pPipelineState) pPipelineState->Release();
}
#pragma endregion

#pragma region HBoneLocalToWorldShaderCode
void HBoneLocalToWorldShader::InitShader() {
	CreateRootSignature();
	CreatePipelineState();
}
void HBoneLocalToWorldShader::CreateRootSignature() {
	D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc1;
	D3D12_ROOT_PARAMETER1 rootParam[RootParamId::Normal_RootParamCapacity] = {};

	rootParam[Constant_WorldMat] = RootParam1::Const32s(GRegID('b', 0), 16, D3D12_SHADER_VISIBILITY_ALL, 0);

	RootParam1 DescTable1;
	DescTable1.PushDescRange(GRegID('t', 0), "SRV", 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	DescTable1.DescTable(D3D12_SHADER_VISIBILITY_ALL);
	rootParam[RootParamId::SRVTable_LocalMatrixs] = DescTable1;

	RootParam1 DescTable2;
	DescTable2.PushDescRange(GRegID('t', 1), "SRV", 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	DescTable2.DescTable(D3D12_SHADER_VISIBILITY_ALL);
	rootParam[RootParamId::SRVTable_TPOSLocalTr] = DescTable2;

	RootParam1 DescTable3;
	DescTable3.PushDescRange(GRegID('t', 2), "SRV", 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	DescTable3.DescTable(D3D12_SHADER_VISIBILITY_ALL);
	rootParam[RootParamId::SRVTable_toParent] = DescTable3;

	RootParam1 DescTable4;
	DescTable4.PushDescRange(GRegID('t', 3), "SRV", 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	DescTable4.DescTable(D3D12_SHADER_VISIBILITY_ALL);
	rootParam[RootParamId::SRVTable_NodeToBoneIndex] = DescTable4;

	RootParam1 DescTable5;
	DescTable5.PushDescRange(GRegID('u', 0), "UAV", 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
	DescTable5.DescTable(D3D12_SHADER_VISIBILITY_ALL);
	rootParam[RootParamId::UAVTable_Out_ToWorldMatrix] = DescTable5;

	D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT/* |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS*/;

	rootSigDesc1.NumParameters = Normal_RootParamCapacity;
	rootSigDesc1.pParameters = rootParam;
	rootSigDesc1.NumStaticSamplers = 0;
	rootSigDesc1.pStaticSamplers = nullptr;
	rootSigDesc1.Flags = d3dRootSignatureFlags;

	ID3DBlob* pd3dSignatureBlob = NULL;
	ID3DBlob* pd3dErrorBlob = NULL;
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_rootSigDesc;
	versioned_rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	versioned_rootSigDesc.Desc_1_1 = rootSigDesc1;
	D3D12SerializeVersionedRootSignature(&versioned_rootSigDesc, &pd3dSignatureBlob, &pd3dErrorBlob);
	gd.pDevice->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(),
		pd3dSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void
			**)&pRootSignature);
	if (pd3dErrorBlob) pd3dErrorBlob->Release();
	if (pd3dSignatureBlob) pd3dSignatureBlob->Release();
}
void HBoneLocalToWorldShader::CreatePipelineState() {
	D3D12_SHADER_BYTECODE NULLCODE;
	NULLCODE.BytecodeLength = 0;
	NULLCODE.pShaderBytecode = nullptr;

	ID3DBlob* pd3dComputeShaderBlob = NULL;
	D3D12_COMPUTE_PIPELINE_STATE_DESC gPipelineStateDesc;
	ZeroMemory(&gPipelineStateDesc, sizeof(D3D12_COMPUTE_PIPELINE_STATE_DESC));

	gPipelineStateDesc.pRootSignature = pRootSignature;
	gPipelineStateDesc.NodeMask = 0;

	//Compute Shader
	gPipelineStateDesc.CS = Shader::GetShaderByteCode(L"Resources/Shaders/AnimationCompute/ModifyLocalToWorldMatrix.hlsl", "CSMain", "cs_5_1", &pd3dComputeShaderBlob);

	gd.pDevice->CreateComputePipelineState(&gPipelineStateDesc,
		__uuidof(ID3D12PipelineState), (void**)&pPipelineState);
	if (pd3dComputeShaderBlob) pd3dComputeShaderBlob->Release();
}
void HBoneLocalToWorldShader::Add_RegisterShaderCommand(GPUCmd& cmd, ShaderType reg) {
	cmd->SetComputeRootSignature(pRootSignature);
	cmd->SetPipelineState(pPipelineState);
}
void HBoneLocalToWorldShader::Release() {
	if (pRootSignature) pRootSignature->Release();
	if (pPipelineState) pPipelineState->Release();
}
#pragma endregion

#pragma endregion

#pragma region AnimationCode

void HumanoidAnimation::LoadHumanoidAnimation(string filename)
{
	bool initialState = gd.gpucmd.isClose;

	ifstream ifs{ filename, ios_base::binary };
	Duration = 0;
	for (int i = 0; i < 55; ++i) {
		int posKeyNum = 0;
		int rotKeyNum = 0;
		int scaleKeyNum = 0;
		ifs.read((char*)&posKeyNum, sizeof(int));
		ifs.read((char*)&rotKeyNum, sizeof(int));
		ifs.read((char*)&scaleKeyNum, sizeof(int));
		channels[i].posKeys.reserve(posKeyNum);
		channels[i].posKeys.resize(posKeyNum);
		channels[i].rotKeys.reserve(rotKeyNum);
		channels[i].rotKeys.resize(rotKeyNum);
		channels[i].scaleKeys.reserve(scaleKeyNum);
		channels[i].scaleKeys.resize(scaleKeyNum);
		for (int k = 0; k < posKeyNum; ++k) {
			ifs.read((char*)&channels[i].posKeys[k].time, sizeof(double));
			ifs.read((char*)&channels[i].posKeys[k].value, sizeof(XMFLOAT3));
			Duration = max(channels[i].posKeys[k].time, Duration);
		}
		for (int k = 0; k < rotKeyNum; ++k) {
			ifs.read((char*)&channels[i].rotKeys[k].time, sizeof(double));
			ifs.read((char*)&channels[i].rotKeys[k].value, sizeof(XMFLOAT4));
			Duration = max(channels[i].rotKeys[k].time, Duration);
		}
		for (int k = 0; k < scaleKeyNum; ++k) {
			ifs.read((char*)&channels[i].scaleKeys[k].time, sizeof(double));
			ifs.read((char*)&channels[i].scaleKeys[k].value, sizeof(XMFLOAT3));
			Duration = max(channels[i].scaleKeys[k].time, Duration);
		}
	}

	if constexpr (gd.PlayAnimationByGPU) {
		frameRate = channels[0].posKeys[1].time - channels[0].posKeys[0].time;
		frameRate = 1.0 / frameRate;
		int fr = frameRate * Duration;

		UINT datasiz = fr * 64 * sizeof(AnimGPUKey);
		UINT ncbElementBytes = ((datasiz + 255) & ~255); //256의 배수
		AnimationRes = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1, DXGI_FORMAT_UNKNOWN, 1, D3D12_RESOURCE_FLAG_NONE);
		GPUResource AnimationRes_Upload = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1, DXGI_FORMAT_UNKNOWN, 1, D3D12_RESOURCE_FLAG_NONE);
		AnimGPUKey* animMapped = nullptr;
		AnimationRes_Upload.resource->Map(0, nullptr, (void**)&animMapped);
		for (int i = 0;i < fr;++i) {
			int keyStart = i << 6;
			for (int b = 0;b < 55;++b) {
				AnimGPUKey& key = animMapped[keyStart + b];
				if (channels[b].posKeys.size() > i) {
					key.pos = channels[b].posKeys[i].value;
				}
				else {
					key.pos = vec4(0, 0, 0, 0);
				}
				if (channels[b].rotKeys.size() > i) {
					key.rot = channels[b].rotKeys[i].value;
				}
				else {
					key.rot = vec4(0, 0, 0, 1);
				}
			}
		}
		AnimationRes_Upload.resource->Unmap(0, nullptr);

		if (gd.gpucmd.isClose) {
			gd.gpucmd.Reset();
		}

		gd.gpucmd.ResBarrierTr(&AnimationRes, D3D12_RESOURCE_STATE_COPY_DEST);
		gd.gpucmd.ResBarrierTr(&AnimationRes_Upload, D3D12_RESOURCE_STATE_COPY_SOURCE);
		gd.gpucmd->CopyResource(AnimationRes.resource, AnimationRes_Upload.resource);
		gd.gpucmd.ResBarrierTr(&AnimationRes, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

		gd.gpucmd.Close();
		gd.gpucmd.Execute();
		gd.gpucmd.WaitGPUComplete();

		if (initialState = false) {
			gd.gpucmd.Reset(true);
		}

		AnimationRes_Upload.Release();

		//create SRV
		int index = gd.TextureDescriptorAllotter.Alloc();
		AnimationDescIndex.Set(false, index, 'n');
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		srvDesc.Buffer.NumElements = fr * 64;
		srvDesc.Buffer.StructureByteStride = sizeof(vec4) * 2;
		gd.pDevice->CreateShaderResourceView(AnimationRes.resource, &srvDesc, AnimationDescIndex.hCreation.hcpu);
	}
}

matrix AnimChannel::GetLocalMatrixAtTime(float time, matrix original)
{
	vec4 pos = vec4(0, 0, 0, 1);;
	vec4 rotQ = vec4(0, 0, 0, 1);
	vec4 scale = vec4(1, 1, 1);
	//XMMatrixDecompose(&scale.v4, &rotQ.v4, &pos.v4, original);

	if (posKeys.size() > 0) {
		for (int i = 0; i < posKeys.size() - 1; ++i) {
			if (posKeys[i].time < time && time < posKeys[i + 1].time) {
				vec4 p0 = posKeys[i].value;
				vec4 p1 = posKeys[i + 1].value;
				float maxf = posKeys[i + 1].time - posKeys[i].time;
				float flow = time - posKeys[i].time;
				float rate = flow / maxf;
				pos = (1.0f - rate) * p0 + rate * p1;
				break;
			}
		}
	}

	if (rotKeys.size() > 0) {
		for (int i = 0; i < rotKeys.size() - 1; ++i) {
			if (rotKeys[i].time < time && time < rotKeys[i + 1].time) {
				vec4 p0 = rotKeys[i].value;
				vec4 p1 = rotKeys[i + 1].value;
				float maxf = rotKeys[i + 1].time - rotKeys[i].time;
				float flow = time - rotKeys[i].time;
				float rate = flow / maxf;
				rotQ = vec4::Qlerp(p0, p1, rate);

				break;
			}
		}
	}

	/*if (scaleKeys.size() > 0) {
		for (int i = 0; i < scaleKeys.size() - 1; ++i) {
			if (scaleKeys[i].time < time && time < scaleKeys[i + 1].time) {
				vec4 p0 = scaleKeys[i].value;
				vec4 p1 = scaleKeys[i + 1].value;
				float maxf = scaleKeys[i + 1].time - scaleKeys[i].time;
				float flow = time - scaleKeys[i].time;
				float rate = flow / maxf;
				scale = (1.0f - rate) * p0 + rate * p1;
				break;
			}
		}
	}
	*/

	//matrix rotM = XMMatrixRotationQuaternion(rotQ);
	//// Pitch (X) 
	//float pitch = atan2f(-rotM._32, sqrtf(rotM._11 * rotM._11 + rotM._21 * rotM._21));
	////pitch += 3.141592f/2;
	//// Yaw (Y)
	//float yaw = atan2f(rotM._31, rotM._33);
	////yaw += 3.141592f/2;
	////Roll (Z) 
	//float roll = atan2f(rotM._12, rotM._22);
	////roll += 3.141592f/2;

	matrix mat =
		XMMatrixScaling(scale.x, scale.y, scale.z) *
		XMMatrixRotationQuaternion(rotQ) *
		XMMatrixTranslation(pos.x, pos.y, pos.z);


	//mat.right *= -1;
	//mat.up.y *= -1;
	//matrix mat = XMMatrixScaling(1, 1, 1);
	//mat = mat * XMMatrixRotationQuaternion(rotQ);
	//mat.pos = pos;
	//mat.pos.w = 1;
	//mat.transpose();

	return mat;
}

#pragma endregion

#pragma region LightCode

void PointLight::CreatePointLight(PointLightCBData init, UINT resolution) {
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

void PointLight::RenderShadow() {
	gd.gpucmd.ResBarrierTr(&StaticShadowCubeMap, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	gd.gpucmd.SetShader(game.MyPBRShader1, ShaderType::RenderShadowMap);
	gd.gpucmd->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.pSVDescHeapForRender);
	game.PresentShaderType = ShaderType::RenderShadowMap;
	for (int i = 0; i < 6; ++i) {
		gd.gpucmd->RSSetViewports(1, &FaceViewPort[i].Viewport);
		gd.gpucmd->RSSetScissorRects(1, &FaceViewPort[i].ScissorRect);
		gd.gpucmd->OMSetRenderTargets(0, nullptr, TRUE, &StaticCubeShadowMapHandleDSV[i]);
		gd.gpucmd->ClearDepthStencilView(StaticCubeShadowMapHandleDSV[i],
			D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);
		game.renderViewPort = &FaceViewPort[i];
		game.renderViewPort->UpdateFrustum();

		matrix xmf4x4View = FaceViewPort[i].ViewMatrix;
		xmf4x4View *= FaceViewPort[i].ProjectMatrix;
		xmf4x4View.transpose();
		gd.gpucmd->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4View, 0);
		gd.gpucmd->SetGraphicsRoot32BitConstants(0, 4, &FaceViewPort[i].Camera_Pos, 16); // no matter

		//game.Map->MapObjects[0]->Render(XMMatrixIdentity());

		/*for (auto& gbj : game.m_gameObjects) {
			gbj->Render();
		}*/
	}
	gd.gpucmd.ResBarrierTr(&StaticShadowCubeMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

#pragma endregion
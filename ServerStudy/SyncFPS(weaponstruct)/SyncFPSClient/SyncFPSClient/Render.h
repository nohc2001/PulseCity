#pragma once
#include "stdafx.h"
#include "GraphicDefs.h"
#include "ttfParser.h"
#include "SpaceMath.h"
#include "Mesh.h"


using namespace TTFFontParser;

/*
* 설명 : DirectX 12 활용을 위한 렌더링 관련 전역변수들을 모아놓은 구조체.
*/
struct GlobalDevice {

	//DXGI 객체를 만들기 위한 팩토리
	IDXGIFactory7* pFactory;// question 002 : why dxgi 6, but is type limit 7 ??

	//화면의 스왑체인
	IDXGISwapChain4* pSwapChain = nullptr;
	
	//DirectX 12 Device
	ID3D12Device* pDevice;

	//스왑체인의 버퍼 개수
	static constexpr unsigned int SwapChainBufferCount = 2;
	// 현재 백 버퍼의 인덱스
	ui32 CurrentSwapChainBufferIndex;

	// RTV DESC의 증가 사이즈.
	ui32 RtvDescriptorIncrementSize;
	// 렌더타겟 리소스
	ID3D12Resource* ppRenderTargetBuffers[SwapChainBufferCount];
	// RTV Desc Heap
	ID3D12DescriptorHeap* pRtvDescriptorHeap;
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

	// 커맨드 큐
	ID3D12CommandQueue* pCommandQueue;
	// 커맨드 할당자
	ID3D12CommandAllocator* pCommandAllocator;
	// 커맨드 리스트
	ID3D12GraphicsCommandList* pCommandList;

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
	unordered_map<wchar_t, GPUResource, hash<wchar_t>> font_texture_map[FontCount];
	// 텍스쳐를 추가해야할 글자를 지정.
	vector<wchar_t> addTextureStack;
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

	// 텍스쳐들을 모아 놓은 것. 
	// fix <뭔가 렌더링에서 통합된 구조를 원한다. 이것도 수정해야 할 수도 있다.>
	// <주로 통신의 예전 버전때문에 어쩔 수 없이 넣은 느낌이다.>
	GPUResource GlobalTextureArr[64] = {};
	enum GlobalTexture {
		GT_TileTex = 0,
		GT_WallTex = 1,
		GT_Monster = 2,
	};

	//머터리얼을 모아 놓은 것.
	Material GlobalMaterialArr[64] = {};
	enum GlobalMaterial {
		GM_TileTex = 0,
		GM_WallTex = 1,
		GM_Monster = 2,
	};

	// CBV, SRV, UAV DESC의 증가 사이즈이다.
	unsigned long long CBV_SRV_UAV_Desc_IncrementSiz = 0;

	// 렌더링에 공통적으로 쓰이는 ShaderVisible한 DESC Heap.
	// Immortal과 Dynamic 영역으로 나뉜다.
	// Immortal은 게속 유지되는 Res DESC, 
	// Dynamic은 TextureDescriptorAllotter등의 또 다른 Non-ShaderVisible Desc Heap에 이미 존재하면서,
	// 유동적으로 자리를 차지할 수 있는 공간이다.
	ShaderVisibleDescriptorPool ShaderVisibleDescPool;

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
	GPUResource CreateCommitedGPUBuffer(D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES d3dResourceStates, D3D12_RESOURCE_DIMENSION dimension, int Width, int Height, DXGI_FORMAT BufferFormat = DXGI_FORMAT_UNKNOWN, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

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
	void UploadToCommitedGPUBuffer(ID3D12GraphicsCommandList* commandList, void* ptr, GPUResource* uploadBuffer, GPUResource* copydestBuffer = nullptr, bool StateReturning = true);

	/*
	 설명 : ??
	*/
	UINT64 GetRequiredIntermediateSize(
		_In_ ID3D12Resource* pDestinationResource,
		_In_range_(0, D3D12_REQ_SUBRESOURCES) UINT FirstSubresource,
		_In_range_(0, D3D12_REQ_SUBRESOURCES - FirstSubresource) UINT NumSubresources) noexcept;

	//this function cannot be executing while command list update.
	/*
	* 설명 : 텍스트의 텍스쳐를 만든다.
	* 매개변수 : 
	* wchar_t key : 새로 텍스쳐를 추가할 문자.
	*/
	void AddTextTexture(wchar_t key);

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
	void AddLineInTexture(float_v2 startpos, float_v2 endpos, BYTE* texture, int width, int height);

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
};

extern GlobalDevice gd;
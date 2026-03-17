#pragma once
#include "stdafx.h"
#include "GraphicDefs.h"

struct GPUResource;

/*
* 설명 : 셰이더를 나타내는 클래스.
* Sentinal Value : 
* NULL = pPipelineState == nullptr && pRootSignature == nullptr
*/
class Shader {
public:
	/*
	* 설명 : 같은 종류의 셰이더에서 다르게 렌더링을 하려하기 때문에, 
	* 어떤 렌더링을 사용할 것인지 선택할 수 있게 하는 enum.
	*/
	union RegisterEnum {
		enum RegisterEnum_memberenum {
			RenderNormal = 0, // 일반 렌더링
			RenderWithShadow = 1, // 그림자와 함께 렌더링
			RenderShadowMap = 2, // 쉐도우 맵을 렌더링
			RenderStencil = 3, // 스텐실을 렌더링
			RenderInnerMirror = 4, // 스텐실이 활성화된 부분을 렌더링 (거울 속 렌더링)
			RenderTerrain = 5, // 터레인을 렌더링
			StreamOut = 6, // 스트림아웃
		};
		int id;

		RegisterEnum(int i) : id{ i } {}
		RegisterEnum(RegisterEnum_memberenum e) { id = (int)e; }
		operator int() { return id; }
	};

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
	* Shader::RegisterEnum reg : 어떤 종류의 렌더링을 선택할 것인지.
	*/
	virtual void Add_RegisterShaderCommand(ID3D12GraphicsCommandList* commandList, Shader::RegisterEnum reg = Shader::RegisterEnum::RenderNormal);

	/*
	* 설명 : 셰이더의 바이트코드를 가져온다.
	* <현재는 파일로부터 GPU 바이트 코드를 가져오고 있다. 하지만 정석적인 방법은 애초에 바이트코드를>
	*/
	static D3D12_SHADER_BYTECODE GetShaderByteCode(const WCHAR* pszFileName, LPCSTR pszShaderName, LPCSTR pszShaderProfile, ID3DBlob** ppd3dShaderBlob);
};

/*
* 설명 : 단일 색상을 출력하는 셰이더
*/
class OnlyColorShader : Shader {
public:
	ID3D12PipelineState* pUiPipelineState;

	OnlyColorShader();
	virtual ~OnlyColorShader();

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();
};

/*
* 설명 : 단일 색상 텍스쳐를 매핑하는 셰이더
*/
class DiffuseTextureShader : Shader {
public:
	DiffuseTextureShader();
	virtual ~DiffuseTextureShader();

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();

	void SetTextureCommand(GPUResource* texture);
};

/*
* 설명 : 화면에 글자를 출력하는 셰이더 / 어떤 화면 영역에 텍스쳐를 렌더링 하는 셰이더
*/
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

/*
* 설명 : 
* <이것은 실제 쉐도우 매핑을 위한 셰이더가 아님. 그냥 쉐도우 맵 초기화등을 하는 오브젝트일 뿐이다. 이를 알 맞게 구조화해야 한다.>
* // fix
*/
class ShadowMappingShader : public Shader {
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

/*
* 설명 : PBR을 사용해 렌더링하는 셰이더
*/
class PBRShader1 : public Shader {
public:
	PBRShader1() {}
	virtual ~PBRShader1() {}

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreateRootSignature_withShadow();
	//virtual void CreateRootSignature_Terrain();

	virtual void CreatePipelineState();
	virtual void CreatePipelineState_withShadow();
	virtual void CreatePipelineState_RenderShadowMap();
	//virtual void CreatePipelineState_RenderStencil();
	//virtual void CreatePipelineState_InnerMirror();
	//virtual void CreatePipelineState_Terrain();

	virtual void Release();

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

	virtual void Add_RegisterShaderCommand(ID3D12GraphicsCommandList* commandList, Shader::RegisterEnum reg = Shader::RegisterEnum::RenderNormal);

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
	D3D12_VERTEX_BUFFER_VIEW SkyBoxMeshVBView;

	void LoadSkyBox(const wchar_t* filename);

	SkyBoxShader();
	virtual ~SkyBoxShader();

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();

	virtual void Add_RegisterShaderCommand(ID3D12GraphicsCommandList* commandList, Shader::RegisterEnum reg = Shader::RegisterEnum::RenderNormal);
	virtual void Release();

	void RenderSkyBox();
};

class ParticleShader : public Shader {
public:
	ID3D12RootSignature* ParticleRootSig = nullptr;
	ID3D12PipelineState* ParticlePSO = nullptr;
	ID3D12PipelineState* MuzzleFlashPSO = nullptr;

	GPUResource* FireTexture = nullptr;

	virtual void InitShader() override;
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();

	void Render(ID3D12GraphicsCommandList* cmd, GPUResource* particleBuffer, UINT particleCount, bool isMuzzleFlash = false);
};

class ParticleCompute
{
public:
	ID3D12RootSignature* RootSig = nullptr;
	ID3D12PipelineState* PSO = nullptr;

	void Init(const wchar_t* hlslFile, const char* entry, UINT constantSizeInBytes);
	void Dispatch(ID3D12GraphicsCommandList* cmd, GPUResource* buffer, UINT count, float dt);
	void DispatchMuzzle(ID3D12GraphicsCommandList* cmd, GPUResource* buffer, UINT count, const MuzzleCB& data, float dt);
	void DispatchTracer(ID3D12GraphicsCommandList* cmd, GPUResource* buffer, UINT count, const TracerCB& data, float dt);
};

#pragma once
#include "stdafx.h"
#include "GraphicDefs.h"

struct GPUResource;

class Shader {
public:
	union RegisterEnum {
		enum RegisterEnum_memberenum {
			RenderNormal = 0,
			RenderWithShadow = 1,
			RenderShadowMap = 2,
			RenderStencil = 3,
			RenderInnerMirror = 4,
			RenderTerrain = 5,
			StreamOut = 6,
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

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();
	virtual void Add_RegisterShaderCommand(ID3D12GraphicsCommandList* commandList, Shader::RegisterEnum reg = Shader::RegisterEnum::RenderNormal);

	static D3D12_SHADER_BYTECODE GetShaderByteCode(const WCHAR* pszFileName, LPCSTR pszShaderName, LPCSTR pszShaderProfile, ID3DBlob** ppd3dShaderBlob);
};

class OnlyColorShader : Shader {
public:
	ID3D12PipelineState* pUiPipelineState;

	OnlyColorShader();
	virtual ~OnlyColorShader();

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();
	void CreateUiPipelineState();
};

class DiffuseTextureShader : Shader {
public:
	DiffuseTextureShader();
	virtual ~DiffuseTextureShader();

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();

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
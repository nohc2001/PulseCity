#pragma once
#include "stdafx.h"

struct GPUResource;

class Shader {
public:
	ID3D12PipelineState* pPipelineState = nullptr;
	ID3D12RootSignature* pRootSignature = nullptr;
	Shader();
	virtual ~Shader();

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();
	void Add_RegisterShaderCommand(ID3D12GraphicsCommandList* commandList);

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
#include "pch.h"
#include "typedef.h"
#include <d3dcompiler.h>
#include <d3dx12.h>
#include <fstream>
#include "D3D_Util/D3DUtil.h"
#include "D3D12ResourceManager.h"
#include "D3D12Renderer.h"
#include "DescriptorPool.h"
#include "SimpleConstantBufferPool.h"
#include "BasicMeshObject.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "FBXCommon.h"

using namespace std;

ID3D12RootSignature* CBasicMeshObject::m_pRootSignature = nullptr;
ID3D12PipelineState* CBasicMeshObject::m_pPipelineState = nullptr;
DWORD CBasicMeshObject::m_dwInitRefCount = 0;
FbxManager* CBasicMeshObject::lSdkManager = nullptr;

CBasicMeshObject::CBasicMeshObject()
{
}

void CBasicMeshObject::STATIC_Init()
{
	lSdkManager = NULL;
	lSdkManager = FbxManager::Create();
	if (!lSdkManager)
	{
		FBXSDK_printf("Error: Unable to create FBX Manager!\n");
		exit(1);
	}
	else FBXSDK_printf("Autodesk FBX SDK version %s\n", lSdkManager->GetVersion());

	//Create an IOSettings object. This object holds all import/export settings.
	FbxIOSettings* ios = FbxIOSettings::Create(lSdkManager, IOSROOT);
	lSdkManager->SetIOSettings(ios);
}

BOOL CBasicMeshObject::Initialize(CD3D12Renderer* pRenderer)
{
	ZeroMemory(this, sizeof(CBasicMeshObject));
	m_pRenderer = pRenderer;

	BOOL bResult = InitCommonResources();
	return bResult;
}

BOOL CBasicMeshObject::InitCommonResources() {
	if (m_dwInitRefCount)
		goto lb_true;
	
	InitRootSinagture();
	InitPipelineState();

lb_true:
	m_dwInitRefCount++;
	return m_dwInitRefCount;
}

BOOL CBasicMeshObject::InitRootSinagture() {
	ID3D12Device5* pD3DDevice = m_pRenderer->INL_GetD3DDevice();
	ID3DBlob* pSignature = nullptr;
	ID3DBlob* pError = nullptr;

	CD3DX12_DESCRIPTOR_RANGE ranges[DESCRIPTOR_COUNT_FOR_DRAW] = {};
	ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // b0 : constant buffer
	ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0 : Diffuse texture
	ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1); // t1 : Specular texture
	ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2); // t2 : Normal texture

	CD3DX12_ROOT_PARAMETER rootParameters[1] = {};
	rootParameters[0].InitAsDescriptorTable(_countof(ranges), ranges, D3D12_SHADER_VISIBILITY_ALL);

	// default sampler
	D3D12_STATIC_SAMPLER_DESC sampler = {};
	SetDefaultSamplerDesc(&sampler, 0);
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

	// extra metadata for optimize of certain pipeline state
	// Allow input layout and deny uneccessary access to certain pipeline stages.
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = 
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	//create root signature
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
	//rootSigDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	rootSigDesc.Init(_countof(rootParameters), rootParameters, 1, &sampler,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	if (FAILED(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignature, &pError))) {
		__debugbreak();
	}

	if (FAILED(pD3DDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&m_pRootSignature)))) {
		__debugbreak();
	}

	if (pSignature) {
		pSignature->Release();
		pSignature = nullptr;
	}

	if (pError) {
		pError->Release();
		pError = nullptr;
	}

	return TRUE;
}

BOOL CBasicMeshObject::InitPipelineState() {
	ID3D12Device5* pD3DDeivce = m_pRenderer->INL_GetD3DDevice();

	ID3DBlob* pVertexShader = nullptr;
	ID3DBlob* pPixelShader = nullptr;

#if defined(_DEBUG)
	// Enable better shader debugging with the graphics debugging tools.
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif
	if (FAILED(D3DCompileFromFile(L"./Shaders/DSA_shader.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &pVertexShader, nullptr)))
	{
		__debugbreak();
	}
	if (FAILED(D3DCompileFromFile(L"./Shaders/DSA_shader.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pPixelShader, nullptr)))
	{
		__debugbreak();
	}


	// Define the vertex input layout.
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{ "TEXCOORD", 2, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};


	// Describe and create the graphics pipeline state object (PSO).
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	psoDesc.pRootSignature = m_pRootSignature;
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(pVertexShader->GetBufferPointer(), pVertexShader->GetBufferSize());
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pPixelShader->GetBufferPointer(), pPixelShader->GetBufferSize());
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	
	// depth buffer setting in pipeline state..
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	//psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;
	if (FAILED(pD3DDeivce->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pPipelineState))))
	{
		__debugbreak();
	}

	if (pVertexShader)
	{
		pVertexShader->Release();
		pVertexShader = nullptr;
	}
	if (pPixelShader)
	{
		pPixelShader->Release();
		pPixelShader = nullptr;
	}
	return TRUE;
}

BOOL CBasicMeshObject::LoadMesh_FromOBJ(const char* path, bool centerPivot, bool scaleUniform, float scaleFactor)
{
	int uvup = 0;
	int normalup = 0;
	int vertexup = 0;

	/*std::vector< unsigned int > vertexIndices, uvIndices, normalIndices;*/
	std::vector< XMFLOAT3 > temp_vertices;
	std::vector< XMFLOAT2 > temp_uvs;
	std::vector< XMFLOAT3 > temp_normals;
	std::vector< IndexX3 > VertexPool;
	std::vector< TriangleIndex > TrianglePool;
	XMFLOAT3 gCenter = {0, 0, 0};
	XMFLOAT3 minPos = { 0, 0, 0};
	XMFLOAT3 maxPos = { 0, 0, 0 };
	XMFLOAT3 ScaleV3 = { 0, 0, 0 };

	ifstream in;
	in.open(path);

	int insposup = 0;
	int insindexup = 0;
	int insUVup = 0;
	int insrealposup = 0;
	int insNormalUp = 0;

	while (in.eof() == false) {
		char rstr[128] = {};
		in >> rstr;
		if (strcmp(rstr, "v") == 0) {
			//좌표
			XMFLOAT3 pos;
			in >> pos.x;
			in >> pos.y;
			in >> pos.z;
			temp_vertices.push_back(pos);
			gCenter.x += pos.x;
			gCenter.y += pos.y;
			gCenter.z += pos.z;
		}
		else if (strcmp(rstr, "vt") == 0) {
			// uv 좌표
			XMFLOAT3 uv;
			in >> uv.x;
			in >> uv.y;
			in >> uv.z;
			temp_uvs.push_back(XMFLOAT2(uv.x, uv.y));
		}
		else if (strcmp(rstr, "vn") == 0) {
			// 노멀
			XMFLOAT3 normal;
			in >> normal.x;
			in >> normal.y;
			in >> normal.z;
			temp_normals.push_back(normal);
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
			//in >> blank;
			in >> vertexIndex[1];
			in >> blank;
			in >> uvIndex[1];
			in >> blank;
			in >> normalIndex[1];
			//in >> blank;
			in >> vertexIndex[2];
			in >> blank;
			in >> uvIndex[2];
			in >> blank;
			in >> normalIndex[2];
			//in >> blank;

			int originindex[3] = {};
			for (int k = 0; k < 3; ++k) {
				IndexX3 i3;
				i3.pos_index = vertexIndex[k];
				i3.uv_index = uvIndex[k];
				i3.normal_index = normalIndex[k];
				bool newVertex = true;
				originindex[k] = 0;
				for (int i = 0; i < VertexPool.size(); ++i) {
					if (i3 == VertexPool.at(i)) {
						newVertex = false;
						originindex[k] = i;
						break;
					}
				}
				if (newVertex) {
					originindex[k] = VertexPool.size();
					VertexPool.push_back(i3);
				}
			}
			TrianglePool.push_back(TriangleIndex(originindex[0], originindex[1], originindex[2]));
			
			/*vertexIndices.push_back(vertexIndex[0]);
			vertexIndices.push_back(vertexIndex[1]);
			vertexIndices.push_back(vertexIndex[2]);
			uvIndices.push_back(uvIndex[0]);
			uvIndices.push_back(uvIndex[1]);
			uvIndices.push_back(uvIndex[2]);
			normalIndices.push_back(normalIndex[0]);
			normalIndices.push_back(normalIndex[1]);
			normalIndices.push_back(normalIndex[2]);*/
		}
	}
	// For each vertex of each triangle

	in.close();

	gCenter.x /= (float)temp_vertices.size();
	gCenter.y /= (float)temp_vertices.size();
	gCenter.z /= (float)temp_vertices.size();
	if (centerPivot) {
		for (int i = 0; i < temp_vertices.size(); ++i) {
			temp_vertices.at(i).x -= gCenter.x;
			temp_vertices.at(i).y -= gCenter.y;
			temp_vertices.at(i).z -= gCenter.z;
		}
	}
	if (scaleUniform) {
		minPos.x = temp_vertices.at(0).x;
		minPos.y = temp_vertices.at(0).y;
		minPos.z = temp_vertices.at(0).z;
		maxPos.x = temp_vertices.at(0).x;
		maxPos.y = temp_vertices.at(0).y;
		maxPos.z = temp_vertices.at(0).z;
		for (int i = 0; i < temp_vertices.size(); ++i) {
			if (temp_vertices.at(i).x < minPos.x) minPos.x = temp_vertices.at(i).x;
			if (temp_vertices.at(i).y < minPos.y) minPos.y = temp_vertices.at(i).y;
			if (temp_vertices.at(i).z < minPos.z) minPos.z = temp_vertices.at(i).z;
			if (temp_vertices.at(i).x > maxPos.x) maxPos.x = temp_vertices.at(i).x;
			if (temp_vertices.at(i).y > maxPos.y) maxPos.y = temp_vertices.at(i).y;
			if (temp_vertices.at(i).z > maxPos.z) maxPos.z = temp_vertices.at(i).z;
		}
		ScaleV3.x = maxPos.x - minPos.x;
		ScaleV3.y = maxPos.y - minPos.y;
		ScaleV3.z = maxPos.z - minPos.z;
		float maxl = max(ScaleV3.x, ScaleV3.y);
		maxl = max(maxl, ScaleV3.z);
		float rate = scaleFactor / maxl;
		for (int i = 0; i < temp_vertices.size(); ++i) {
			temp_vertices.at(i).x *= rate;
			temp_vertices.at(i).y *= rate;
			temp_vertices.at(i).z *= rate;
		}
	}

	BasicVertex* temp_vertexbufff = (BasicVertex*)malloc(sizeof(BasicVertex) * VertexPool.size());
	vertexCount = VertexPool.size();
	for (unsigned int i = 0; i < vertexCount; i++) {
		unsigned int vertexIndex = VertexPool[i].pos_index;
		XMFLOAT3 vertex = temp_vertices[vertexIndex - 1];
		XMFLOAT2 uvs = temp_uvs[VertexPool[i].uv_index - 1];
		XMFLOAT3 normal = temp_normals[VertexPool[i].normal_index - 1];
		temp_vertexbufff[i].position = vertex;
		//temp_vertexbufff[i].color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		temp_vertexbufff[i].texcoord = uvs;
		temp_vertexbufff[i].normal = normal;
		temp_vertexbufff[i].tangent = XMFLOAT3(0, 0, 0);
		temp_vertexbufff[i].bitangent = XMFLOAT3(0, 0, 0);
	}

	for (int i = 0; i < TrianglePool.size(); ++i) {
		TriangleIndex index = TrianglePool.at(i);
		if (temp_vertexbufff[index.v[0]].HaveToSetTangent()) {
			BasicVertex::CreateTBN(temp_vertexbufff[index.v[0]], 
				temp_vertexbufff[index.v[1]], temp_vertexbufff[index.v[2]]);
		}

		if (temp_vertexbufff[index.v[1]].HaveToSetTangent()) {
			BasicVertex::CreateTBN(temp_vertexbufff[index.v[1]],
				temp_vertexbufff[index.v[0]], temp_vertexbufff[index.v[2]]);
		}

		if (temp_vertexbufff[index.v[2]].HaveToSetTangent()) {
			BasicVertex::CreateTBN(temp_vertexbufff[index.v[2]],
				temp_vertexbufff[index.v[0]], temp_vertexbufff[index.v[1]]);
		}
	}

	indexCount = TrianglePool.size() * 3;

	//index buffer -> TrianglePool
	BOOL bResult = FALSE;
	ID3D12Device5* pD3DDeivce = m_pRenderer->INL_GetD3DDevice();
	CD3D12ResourceManager* pResourceManager = m_pRenderer->INL_GetResourceManager();
	//buffer read by gpu memory
	if (FAILED(pResourceManager->CreateVertexBuffer(sizeof(BasicVertex), (DWORD)vertexCount, &m_VertexBufferView, &m_pVertexBuffer, temp_vertexbufff))) {
		_CrtDbgBreak();
		goto lb_return;
	}

	if (FAILED(pResourceManager->CreateIndexBuffer((DWORD)indexCount, &m_IndexBufferView, &m_pIndexBuffer, &TrianglePool[0]))) {
		_CrtDbgBreak();
		goto lb_return;
	}

	//CreateDescriptorTable();

	bResult = TRUE;

lb_return:
	free(temp_vertexbufff);
	return bResult;
}

BOOL CBasicMeshObject::LoadObject_FromFBX(const char* path, bool centerPivot, bool scaleUniform, float scaleFactor)
{
	FbxManager* lSdkManager = NULL;
	FbxScene* lScene = NULL;
	bool lResult;
	// Prepare the FBX SDK.
#ifndef FBXSDK_ENV_WINSTORE
//Load plugins from the executable directory (optional)
	FbxString lPath = FbxGetApplicationDirectory();
	lSdkManager->LoadPluginsDirectory(lPath.Buffer());
#endif

	//Create an FBX scene. This object holds most objects imported/exported from/to files.
	lScene = FbxScene::Create(lSdkManager, "My Scene");
	if (!lScene)
	{
		FBXSDK_printf("Error: Unable to create FBX scene!\n");
		exit(1);
	}

	FbxString lFilePath(path);

	FBXSDK_printf("\n\nFile: %s\n\n", lFilePath.Buffer());
	lResult = LoadScene(lSdkManager, lScene, lFilePath.Buffer());


	return 0;
}

void CBasicMeshObject::Draw(ID3D12GraphicsCommandList* pCommandList, const CONSTANT_BUFFER_DEFAULT& constantBuff,
	const TEXTURES_VECTOR& texvec)
{
	ID3D12Device5* pD3DDeivce = m_pRenderer->INL_GetD3DDevice();
	UINT srvDescriptorSize = m_pRenderer->INL_GetSrvDescriptorSize();
	CDescriptorPool* pDescriptorPool = m_pRenderer->INL_GetDescriptorPool();
	ID3D12DescriptorHeap* pDescriptorHeap = pDescriptorPool->INL_GetDescriptorHeap();
	CSimpleConstantBufferPool* pConstantBufferPool = m_pRenderer->INL_GetConstantBufferPool();

	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable = {};
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorTable = {};

	if (!pDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, DESCRIPTOR_COUNT_FOR_DRAW))
	{
		__debugbreak();
	}

	// 각각의 draw()에 대해 독립적인 constant buffer(내부적으로는 같은 resource의 다른 영역)를 사용한다.
	CB_CONTAINER* pCB = pConstantBufferPool->Alloc();
	if (!pCB)
	{
		__debugbreak();
	}
	CONSTANT_BUFFER_DEFAULT* pConstantBufferDefault = (CONSTANT_BUFFER_DEFAULT*)pCB->pSystemMemAddr;

	*pConstantBufferDefault = constantBuff;

	// set RootSignature
	pCommandList->SetGraphicsRootSignature(m_pRootSignature);

	pCommandList->SetDescriptorHeaps(1, &pDescriptorHeap);

	// 이번에 사용할 constant buffer의 descriptor를 렌더링용(shader visible) descriptor table에 카피
	CD3DX12_CPU_DESCRIPTOR_HANDLE cbvDest(cpuDescriptorTable, BASIC_MESH_DESCRIPTOR_INDEX_CBV, srvDescriptorSize);
	// cpu측 코드에서는 cpu descriptor handle에만 write가능
	pD3DDeivce->CopyDescriptorsSimple(1, cbvDest, pCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	int texture_start_desc_index = 1;
	for (int i = 0; i < 8; ++i) {
		// cpu측 코드에서는 cpu descriptor handle에만 write가능
		CD3DX12_CPU_DESCRIPTOR_HANDLE srvDest(cpuDescriptorTable, texture_start_desc_index+i, srvDescriptorSize);
		if (texvec.tex[i] != nullptr)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE srv = {};
			if (texvec.tex[i]) {
				srv = texvec.tex[i]->srv;
			}
			pD3DDeivce->CopyDescriptorsSimple(1, srvDest, srv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
		else break;
	}
	
	/*if (texvec.tex[1] != nullptr)
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE srvDest(cpuDescriptorTable, BASIC_MESH_DESCRIPTOR_INDEX_SPE, srvDescriptorSize);
		pD3DDeivce->CopyDescriptorsSimple(1, srvDest, texvec.tex[1]->srv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}*/

	/*CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable(
		m_pDescriptorHeap->GetGPUDescriptorHandleForHeapStart());*/

	/*fmhl.HeapDump();
	fm->RECORD_NonReleaseHeapData();
	ofstream ofs{ "fm1_lifecheck.txt" };
	fm->dbg_fm1_lifecheck(ofs);*/
	//outFAlloc();

	pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);

	pCommandList->SetPipelineState(m_pPipelineState);
	pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pCommandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
	pCommandList->IASetIndexBuffer(&m_IndexBufferView);
	//pCommandList->DrawInstanced(3, 1, 0, 0); // vertex data only drawing
	pCommandList->DrawIndexedInstanced(indexCount*3, 1, 0, 0, 0); // with indexdata drawing
}

void CBasicMeshObject::Cleanup()
{
	if (m_pVertexBuffer)
	{
		m_pVertexBuffer->Release();
		m_pVertexBuffer = nullptr;
	}
	if (m_pIndexBuffer) {
		m_pIndexBuffer->Release();
		m_pIndexBuffer = nullptr;
	}
	CleanupSharedResources();
}

void CBasicMeshObject::CleanupSharedResources()
{
	if (!m_dwInitRefCount)
		return;

	DWORD ref_count = --m_dwInitRefCount;
	if (!ref_count)
	{
		if (m_pRootSignature)
		{
			m_pRootSignature->Release();
			m_pRootSignature = nullptr;
		}
		if (m_pPipelineState)
		{
			m_pPipelineState->Release();
			m_pPipelineState = nullptr;
		}
	}
}

CBasicMeshObject::~CBasicMeshObject()
{
	Cleanup();
}
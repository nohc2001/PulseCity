#include "pch.h"
#include <d3dcompiler.h>
#include "D3D12Renderer.h"
#include "GameObject.h"

RenderState Mesh::renderstate[16] = {};
CD3D12Renderer* Mesh::Renderers[16] = {};
FbxManager* Mesh::lSdkManager;
void(*MeshComponent::UpdateFunc)(void*, float) = nullptr;
void(*ChildComponent::UpdateFunc)(void*, float) = nullptr;
void(*ChildComponent::EventFunc)(void*, WinEvent) = nullptr;

//Mesh
RenderState Mesh::InitRenderState01(CD3D12Renderer* Renderer)
{
	RenderState ret = {};
	ID3D12Device5* pD3DDevice = Renderer->INL_GetD3DDevice();
	ID3DBlob* pSignature = nullptr;
	ID3DBlob* pError = nullptr;

	constexpr int DESCRIPTOR_COUNT_FOR_DRAW = 4;
	CD3DX12_DESCRIPTOR_RANGE ranges[DESCRIPTOR_COUNT_FOR_DRAW] = {};
	ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // b0 : constant buffer
	ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0 : Diffuse texture
	ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1); // t1 : Specular texture
	ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2); // t2 : Normal texture

	CD3DX12_ROOT_PARAMETER rootParameters[1] = {};
	rootParameters[0].InitAsDescriptorTable(_countof(ranges), ranges, D3D12_SHADER_VISIBILITY_ALL);

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
		throw "D3D12SerializeRootSignature error.";
	}

	if (FAILED(pD3DDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&(ret.rs))))) {
		throw "CreateRootSignature error.";
	}

	if (pSignature) {
		pSignature->Release();
		pSignature = nullptr;
	}

	if (pError) {
		pError->Release();
		pError = nullptr;
	}

	ID3D12Device5* pD3DDeivce = Renderer->INL_GetD3DDevice();

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
		throw "Vertex Shader Compile Failed.";
	}
	if (FAILED(D3DCompileFromFile(L"./Shaders/DSA_shader.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pPixelShader, nullptr)))
	{
		throw "Pixel Shader Compile Failed.";
	}

	// Define the vertex input layout.
	constexpr int inputElementCount = 5;
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[inputElementCount] =
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
	psoDesc.pRootSignature = ret.rs;
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
	if (FAILED(pD3DDeivce->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&(ret.ps)))))
	{
		throw "CreateGraphicsPiplineState error";
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

	return ret;
}

void Mesh::Static_Init(CD3D12Renderer* Renderer)
{
	//Renderers Init
	for (int i = 0; i < 16; ++i) {
		Renderers[i] = nullptr;
	}
	Renderers[0] = Renderer;

	// FBX SDK Init
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

	//Init RenderState

	//id = 0 : NULL
	for (int i = 0; i < 16; ++i) {
		renderstate[i].ps = nullptr;
		renderstate[i].rs = nullptr;
	}
	
	//id = 1 : diffuse / specular / normal
	try {
		renderstate[1] = InitRenderState01(Renderer);
	}
	catch (const char* expStr) {
		cout << "Execption : " << expStr << endl;
	}

	//...
}

void Mesh::Init(int prtype, int rid)
{
	RendererID = rid;
	renderState_type = prtype;

}

void Mesh::LoadMesh_FromOBJ(const char* path, bool centerPivot, bool scaleUniform, float scaleFactor)
{
	fm->_tempPushLayer();
	CD3D12Renderer* renderer = Renderers[RendererID];

	int uvup = 0;
	int normalup = 0;
	int vertexup = 0;

	/*std::vector< unsigned int > vertexIndices, uvIndices, normalIndices;*/
	fmvecarr< XMFLOAT3 > temp_vertices;
	temp_vertices.NULLState();
	temp_vertices.Init(512, false);
	fmvecarr< XMFLOAT2 > temp_uvs;
	temp_uvs.NULLState();
	temp_uvs.Init(512, false);
	fmvecarr< XMFLOAT3 > temp_normals;
	temp_normals.NULLState();
	temp_normals.Init(512, false);
	fmvecarr< IndexX3 > VertexPool;
	VertexPool.NULLState();
	VertexPool.Init(512, false);
	fmvecarr< TriangleIndex > TrianglePool;
	TrianglePool.NULLState();
	TrianglePool.Init(512, false);
	XMFLOAT3 gCenter = { 0, 0, 0 };
	XMFLOAT3 minPos = { 0, 0, 0 };
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

	BasicVertex* temp_vertexbufff = (BasicVertex*)fm->_tempNew(sizeof(BasicVertex) * VertexPool.size());
	int vertexCount = VertexPool.size();
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
	ID3D12Device5* pD3DDeivce = renderer->INL_GetD3DDevice();
	CD3D12ResourceManager* pResourceManager = renderer->INL_GetResourceManager();
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
	fm->_tempPopLayer();
	//free(temp_vertexbufff);
}

void Mesh::Render(ID3D12GraphicsCommandList* pCommandList, const CONSTANT_BUFFER_DEFAULT& constantBuff, const pMaterial material)
{
	Pong_Material* pmat = material.GetMaterial<Pong_Material>();
	CD3D12Renderer* renderer = Renderers[RendererID];
	RenderState rs = renderstate[renderState_type];
	ID3D12Device5* pD3DDeivce = renderer->INL_GetD3DDevice();
	UINT srvDescriptorSize = renderer->INL_GetSrvDescriptorSize();
	CDescriptorPool* pDescriptorPool = renderer->INL_GetDescriptorPool();
	ID3D12DescriptorHeap* pDescriptorHeap = pDescriptorPool->INL_GetDescriptorHeap();
	CSimpleConstantBufferPool* pConstantBufferPool = renderer->INL_GetConstantBufferPool();

	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuDescriptorTable = {};
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorTable = {};

	if (!pDescriptorPool->AllocDescriptorTable(&cpuDescriptorTable, &gpuDescriptorTable, rs.DESCRIPTOR_COUNT_FOR_DRAW))
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
	pCommandList->SetGraphicsRootSignature(rs.rs);

	pCommandList->SetDescriptorHeaps(1, &pDescriptorHeap);

	// 이번에 사용할 constant buffer의 descriptor를 렌더링용(shader visible) descriptor table에 카피
	CD3DX12_CPU_DESCRIPTOR_HANDLE cbvDest(cpuDescriptorTable, BASIC_MESH_DESCRIPTOR_INDEX_CBV, srvDescriptorSize);
	// cpu측 코드에서는 cpu descriptor handle에만 write가능
	pD3DDeivce->CopyDescriptorsSimple(1, cbvDest, pCB->CBVHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// cpu측 코드에서는 cpu descriptor handle에만 write가능
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvDest0(cpuDescriptorTable, pmat->DiffuseIndex, srvDescriptorSize);
	if (pmat->DiffuseTexture != nullptr)
	{
		pD3DDeivce->CopyDescriptorsSimple(1, srvDest0, pmat->DiffuseTexture->srv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvDest1(cpuDescriptorTable, pmat->SpecularIndex, srvDescriptorSize);
	if (pmat->SpecularTexture != nullptr)
	{
		pD3DDeivce->CopyDescriptorsSimple(1, srvDest1, pmat->SpecularTexture->srv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvDest2(cpuDescriptorTable, pmat->NormalIndex, srvDescriptorSize);
	if (pmat->NormalTexture != nullptr)
	{
		pD3DDeivce->CopyDescriptorsSimple(1, srvDest2, pmat->NormalTexture->srv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	pCommandList->SetGraphicsRootDescriptorTable(0, gpuDescriptorTable);

	pCommandList->SetPipelineState(rs.ps);
	pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pCommandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
	pCommandList->IASetIndexBuffer(&m_IndexBufferView);
	pCommandList->DrawIndexedInstanced(indexCount * 3, 1, 0, 0, 0); // with indexdata drawing
}

void Mesh::Release()
{

}

void ChildComponent::Static_Init()
{
	UpdateFunc = ChildComp_Update;
}

void ChildComp_Update(void* comp, float delta)
{
	ChildComponent* childs = (ChildComponent*)comp;
	for (int i = 0; i < childs->up; ++i) {
		Object* obj = (Object*)childs->ObjArr[i];
		obj->Update(delta);
	}
}

void ChildComp_Event(void* comp, WinEvent evt)
{
	ChildComponent* childs = (ChildComponent*)comp;
	for (int i = 0; i < childs->up; ++i) {
		Object* obj = (Object*)childs->ObjArr[i];
		obj->Event(evt);
	}
}

void AllocNewMGOCLump()
{
	static constexpr ui32 SIZE_2MB = 4096 * 512;
	MGameObjectChunckLump* lptr = (MGameObjectChunckLump*)fmhl.AllocNewPages(SIZE_2MB);
	ZeroMemory(lptr, SIZE_2MB);
	MGameObjChunckLumps.push_back(lptr);
}

__forceinline MGameObjectChunck& GetMGOChunck(ui32 index)
{
	return MGameObjChunckLumps[index >> 9]->chuncks[index & 511];
}

void SpaceChunck::Init() {
	GeneralGameObjectArr.NULLState();
	GeneralGameObjectArr.Init(8, true);
	MOChunckArr.NULLState();
	MOChunckArr.Init(8, true);
	Collision_StaticTour.NULLState();
	Collision_StaticTour.Init(8, true);
	Collision_DynamicTour.NULLState();
	Collision_DynamicTour.Init(8, true);
	illuminate_StaticTour.NULLState();
	illuminate_StaticTour.Init(8, true);
	illuminate_DynamicTour.NULLState();
	illuminate_DynamicTour.Init(8, true);
	EventObject_Tour.NULLState();
	EventObject_Tour.Init(8, true);

	for (int x = -1; x < 2; ++x) {
		for (int y = -1; y < 2; ++y) {
			for (int z = -1; z < 2; ++z) {
				nearSpaceChunck[x + 1][y + 1][z + 1] = GameWorld.GetSpaceChunck(cx + x, cy + y, cz + z);
			}
		}
	}
}

void CubeColliderComponent::Static_Init()
{
}

void SphereColliderComponent::Static_Init()
{
}

void CilinderColliderComponent::Static_Init()
{
}

void MeshColliderComponent::Static_Init()
{
}

#ifndef BASICMESHOBJECT_H
#define BASICMESHOBJECT_H

#include <Windows.h>
#include "typedef.h"

#pragma comment (lib, "libfbxsdk.lib")
#include <fbxsdk.h>


XMVECTOR GetVec(XMFLOAT3 f3);
XMVECTOR GetVec(XMFLOAT2 f2);

enum BASIC_MESH_DESCRIPTOR_INDEX
{
	BASIC_MESH_DESCRIPTOR_INDEX_CBV = 0,
	BASIC_MESH_DESCRIPTOR_INDEX_TEX = 1,
	BASIC_MESH_DESCRIPTOR_INDEX_SPE = 2
};

class CD3D12Renderer;
class CBasicMeshObject
{
	// shared by all CBasicMeshObject instances.
	static ID3D12RootSignature* m_pRootSignature;
	static ID3D12PipelineState* m_pPipelineState;
	static DWORD	m_dwInitRefCount;
	static FbxManager* lSdkManager;

	CD3D12Renderer* m_pRenderer = nullptr;

	// vertex data
	int vertexCount = 0;
	ID3D12Resource* m_pVertexBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView = {};

	// index data
	int indexCount = 0;
	ID3D12Resource* m_pIndexBuffer = nullptr;
	D3D12_INDEX_BUFFER_VIEW m_IndexBufferView = {};

	BOOL	InitCommonResources();
	void	CleanupSharedResources();

	BOOL	InitRootSinagture();
	BOOL	InitPipelineState();

	//BOOL	CreateDescriptorTable();

	void	Cleanup();

public:
	static constexpr UINT DESCRIPTOR_COUNT_FOR_DRAW = 4; // {constantbuffer, diffuse, specular, normal}

	static void	STATIC_Init();
	BOOL	Initialize(CD3D12Renderer* pRenderer);
	void	Draw(ID3D12GraphicsCommandList* pCommandList, const CONSTANT_BUFFER_DEFAULT& constantBuff, const TEXTURES_VECTOR& texvec);
	BOOL	CreateMesh();

	//OBJ
	BOOL	LoadMesh_FromOBJ(const char* path, bool centerPivot = false, bool scaleUniform = false, float scaleFactor = 1);
	
	//FBX -> after instancing why? because node can hold multiple mesh, object, animations..
	BOOL	LoadObject_FromFBX(const char* path, bool centerPivot = false, bool scaleUniform = false, float scaleFactor = 1);
	//GLB

	CBasicMeshObject();
	~CBasicMeshObject();
};

//fbx load..
//https://help.autodesk.com/view/FBX/2020/ENU/?guid=FBX_Developer_Help_getting_started_installing_and_configuring_configuring_the_fbx_sdk_for_wind_html

#endif
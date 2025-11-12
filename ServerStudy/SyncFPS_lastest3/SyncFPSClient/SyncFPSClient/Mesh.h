#pragma once
#include "stdafx.h"
#include "GraphicDefs.h"

class Mesh {
public:
	GPUResource VertexBuffer;
	GPUResource VertexUploadBuffer;
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView;

	GPUResource IndexBuffer;
	GPUResource IndexUploadBuffer;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView;

	ui32 IndexNum = 0;
	ui32 VertexNum = 0;
	D3D12_PRIMITIVE_TOPOLOGY topology;

	XMFLOAT3 MAXpos = { 0, 0, 0 }; // it can replace Mesh OBB.

	static unordered_map<string, Mesh*> meshmap;

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

	Mesh();
	virtual ~Mesh();

	virtual void ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering = true);
	virtual void Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum);
	BoundingOrientedBox GetOBB();
	void CreateGroundMesh(ID3D12GraphicsCommandList* pCommandList);
	virtual void CreateWallMesh(float width, float height, float depth, vec4 color);
};

class UVMesh : public Mesh {
public:
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

	virtual void ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering = true);
	virtual void Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum);
	virtual void CreateWallMesh(float width, float height, float depth, vec4 color);

	void CreateTextRectMesh();
};
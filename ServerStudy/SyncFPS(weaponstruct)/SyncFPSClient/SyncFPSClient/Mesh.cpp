#include "stdafx.h"
#include "Render.h"
#include "Mesh.h"
#include "Game.h"

vector<string> Shape::ShapeNameArr;
unordered_map<string, Shape> Shape::StrToShapeMap;

Mesh::Mesh()
{
}

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
	VertexBuffer = gd.CreateCommitedGPUBuffer( D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, temp_vertices.size() * sizeof(Vertex), 1);
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
	topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

void Mesh::Release()
{
	VertexBuffer.Release();
	VertexUploadBuffer.Release();
	IndexBuffer.Release();
	IndexUploadBuffer.Release();
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
}

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
	vertices.push_back(Vertex(XMFLOAT3(width, -height, depth),  XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(1, 0), XMFLOAT3(1, 0, 0)));
	vertices.push_back(Vertex(XMFLOAT3(width, height, depth),XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(1, 1), XMFLOAT3(1, 0, 0)));
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

		// update raster submesh index range
		for (int i = 0; i < subMeshNum + 1; ++i) {
			SubMeshIndexStart[i] = (rmesh.IBStartOffset[i] - rmesh.IBStartOffset[0]) / sizeof(UINT);
		}

		IndexBufferView.BufferLocation = RayTracingMesh::indexBuffer->GetGPUVirtualAddress() + rmesh.IBStartOffset[0];
		IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
		IndexBufferView.SizeInBytes = rmesh.IBStartOffset[subMeshNum] - rmesh.IBStartOffset[0];
		IndexNum = (rmesh.IBStartOffset[subMeshNum] - rmesh.IBStartOffset[0]) / sizeof(UINT);
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
				Material* mat = &game.MaterialTable[materialIndex];
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
	for (int i = 0; i < game.MaterialTable.size(); ++i) {
		Material& mat = game.MaterialTable[i];
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
		TextureSRVTableIndex = game.MaterialTable[findSame].TextureSRVTableIndex;
	}
	else {
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
	}

	UINT ncbElementBytes = ((sizeof(MaterialCB_Data) + 255) & ~255); //256의 배수
	CB_Resource = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1);
	CB_Resource.resource->Map(0, NULL, (void**)&CBData);
	*CBData = GetMatCB();
	CB_Resource.resource->Unmap(0, NULL);
	gd.ShaderVisibleDescPool.ImmortalAlloc_MaterialCBV(&CB_Resource.descindex, 1);

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc;
	cbv_desc.BufferLocation = CB_Resource.resource->GetGPUVirtualAddress();
	cbv_desc.SizeInBytes = ncbElementBytes;
	gd.pDevice->CreateConstantBufferView(&cbv_desc, CB_Resource.descindex.hCreation.hcpu);
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

void Material::InitMaterialStructuredBuffer(bool reset)
{
	if (MaterialStructuredBuffer.resource != nullptr) {
		if (reset) {
			MaterialStructuredBuffer.Release();
			MaterialStructuredBuffer.resource = nullptr;
			UINT ncbElementBytes = ((sizeof(MaterialST_Data) * gd.ShaderVisibleDescPool.MaterialCBVCap + 255) & ~255); //256의 배수
			MaterialStructuredBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1);
			MaterialStructuredBuffer.resource->Map(0, NULL, (void**)&MappedMaterialStructuredBuffer);
			for (int i = 0; i < game.MaterialTable.size(); ++i) {
				MappedMaterialStructuredBuffer[i] = game.MaterialTable[i].GetMatST();
			}
			MaterialStructuredBuffer.resource->Unmap(0, NULL);

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
		for (int i = 0; i < game.MaterialTable.size(); ++i) {
			MappedMaterialStructuredBuffer[i] = game.MaterialTable[i].GetMatST();
		}
		MaterialStructuredBuffer.resource->Unmap(0, NULL);

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

template <bool isSkinMesh>
void ModelNode::Render(void* model, GPUCmd& cmd, const matrix& parentMat, void* pGameobject)
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
		if constexpr (isSkinMesh == false) {
			//bump mesh
			matrix m = sav;
			m.transpose();

			cmd->SetGraphicsRoot32BitConstants(1, 16, &m, 0);
			for (int i = 0; i < numMesh; ++i) {
				if (pModel->mMeshes[Meshes[i]]->type == Mesh::MeshType::BumpMesh) {
					BumpMesh* Bmesh = (BumpMesh*)((BumpMesh*)pModel->mMeshes[Meshes[i]]);
					for (int k = 0; k < Bmesh->subMeshNum; ++k) {
						using PBRRPI = PBRShader1::RootParamId;
						Material& mat = game.MaterialTable[materialIndex[k]];
						cmd->SetGraphicsRootDescriptorTable(PBRRPI::SRVTable_MaterialTextures, mat.TextureSRVTableIndex.hRender.hgpu);
						cmd->SetGraphicsRootDescriptorTable(PBRRPI::CBVTable_Material, mat.CB_Resource.descindex.hRender.hgpu);
						pModel->mMeshes[Meshes[i]]->Render(cmd, 1, k);
					}
				}
			}
		}
		else {
			//skin mesh
			SkinMeshGameObject* smgo = (SkinMeshGameObject*)pGameobject;
			for (int i = 0; i < numMesh; ++i) {
				if (pModel->mMeshes[Meshes[i]]->type == Mesh::MeshType::SkinedBumpMesh) {
					using PBRRPI = PBRShader1::RootParamId;
					BumpSkinMesh* bmesh = (BumpSkinMesh*)((BumpSkinMesh*)pModel->mMeshes[Meshes[i]]);

					//Set Offset
					DescHandle OffsetMatrixCBVHandle;
					gd.ShaderVisibleDescPool.DynamicAlloc(&OffsetMatrixCBVHandle, 1);
					gd.pDevice->CopyDescriptorsSimple(1, OffsetMatrixCBVHandle.hcpu, bmesh->ToOffsetMatrixsCB.descindex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					cmd->SetGraphicsRootDescriptorTable(PBRRPI::CBVTable_SkinMeshOffsetMatrix, OffsetMatrixCBVHandle.hgpu);

					//Set ToWorld
					DescHandle ToWorldMatrixCBVHandle;
					gd.ShaderVisibleDescPool.DynamicAlloc(&ToWorldMatrixCBVHandle, 1);
					gd.pDevice->CopyDescriptorsSimple(1, ToWorldMatrixCBVHandle.hcpu, smgo->BoneToWorldMatrixCB[Mesh_SkinMeshindex[i]].descindex.hCreation.hcpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					cmd->SetGraphicsRootDescriptorTable(PBRRPI::CBVTable_SkinMeshToWorldMatrix, ToWorldMatrixCBVHandle.hgpu);

					for (int k = 0; k < bmesh->subMeshNum; ++k) {
						Material& mat = game.MaterialTable[materialIndex[k]];
						cmd->SetGraphicsRootDescriptorTable(PBRRPI::SRVTable_SkinMeshMaterialTextures, mat.TextureSRVTableIndex.hRender.hgpu);
						cmd->SetGraphicsRootDescriptorTable(PBRRPI::CBVTable_SkinMeshMaterial, mat.CB_Resource.descindex.hRender.hgpu);

						pModel->mMeshes[Meshes[i]]->Render(cmd, 1, k);
					}
				}
			}
		}
	}

	if (numChildren != 0 && Childrens != nullptr) {
		for (int i = 0; i < numChildren; ++i) {
			Childrens[i]->Render<isSkinMesh>(model, cmd, sav, pGameobject);
		}
	}
}

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
	if (numMesh != 0 && Meshes != nullptr) {
		matrix m = sav;
		vec4 AABB[2];

		BoundingOrientedBox obb = model->mMeshes[0]->GetOBB();
		BoundingOrientedBox obb_world;
		obb_world.Transform(obb_world, sav);

		gd.GetAABBFromOBB(AABB, obb_world, true);
		for (int i = 1; i < numMesh; ++i) {
			BoundingOrientedBox obb = model->mMeshes[i]->GetOBB();
			BoundingOrientedBox obb_world;
			obb_world.Transform(obb_world, sav);

			gd.GetAABBFromOBB(AABB, obb_world);
		}

		if (model->AABB[0].x > AABB[0].x) model->AABB[0].x = AABB[0].x;
		if (model->AABB[0].y > AABB[0].y) model->AABB[0].y = AABB[0].y;
		if (model->AABB[0].z > AABB[0].z) model->AABB[0].z = AABB[0].z;

		if (model->AABB[1].x < AABB[1].x) model->AABB[1].x = AABB[1].x;
		if (model->AABB[1].y < AABB[1].y) model->AABB[1].y = AABB[1].y;
		if (model->AABB[1].z < AABB[1].z) model->AABB[1].z = AABB[1].z;
	}

	for (int i = 0; i < numChildren; ++i) {
		ModelNode* node = Childrens[i];
		node->BakeAABB(origin, sav);
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
		vector<BumpSkinMesh::BoneData> skinboneWeights;
		vector<TriangleIndex> indexs;
		int stackSiz = 0;
		int prevSiz = 0;
		int* SubMeshSlots = new int[subMeshCount + 1];
		SubMeshSlots[0] = 0;

		if (hasBone) {
			BSMCount += 1;
			BumpSkinMesh* mesh = new BumpSkinMesh();
			mesh->type = Mesh::MeshType::SkinedBumpMesh;
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
				for (int k = 0; k < tricnt; ++k) {
					ifs.read((char*)&indexs[prevSiz + k].v[0], sizeof(UINT));
					ifs.read((char*)&indexs[prevSiz + k].v[1], sizeof(UINT));
					ifs.read((char*)&indexs[prevSiz + k].v[2], sizeof(UINT));
				}
				prevSiz = stackSiz;
				SubMeshSlots[k + 1] = 3 * prevSiz;
			}

			ifs.read((char*)&mesh->MatrixCount, sizeof(int));
			matrix* OffsetMatrixs = new matrix[mesh->MatrixCount];
			mesh->toNodeIndex = new int[mesh->MatrixCount];
			for (int k = 0; k < mesh->MatrixCount; ++k) {
				matrix offset;
				ifs.read((char*)&OffsetMatrixs[k], sizeof(matrix));
				//OffsetMatrixs[k].pos /= 100;
				//OffsetMatrixs[k].pos.w = 1;
				OffsetMatrixs[k].transpose();
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

			mesh->CreateMesh_FromVertexAndIndexData(skinvertices, skinboneWeights, indexs, subMeshCount, SubMeshSlots, OffsetMatrixs, mesh->MatrixCount);
			mMeshes[i] = mesh;
		}
		else {
			BumpMesh* mesh = new BumpMesh();
			mesh->type = Mesh::MeshType::BumpMesh;
			mesh->SetOBBDataWithAABB(AABB[0], AABB[1]);

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
				for (int k = 0; k < tricnt; ++k) {
					ifs.read((char*)&indexs[prevSiz + k].v[0], sizeof(UINT));
					ifs.read((char*)&indexs[prevSiz + k].v[1], sizeof(UINT));
					ifs.read((char*)&indexs[prevSiz + k].v[2], sizeof(UINT));
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

			mesh->CreateMesh_FromVertexAndIndexData(vertices, indexs, subMeshCount, SubMeshSlots);
			mMeshes[i] = mesh;
		}
	}

	mNumSkinMesh = BSMCount;
	mBumpSkinMeshs = new BumpSkinMesh * [mNumSkinMesh];
	int cnt = 0;
	for (int i = 0; i < mNumMeshes; ++i) {
		Mesh* mesh = mMeshes[i];
		if (mesh->type == Mesh::SkinedBumpMesh) {
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

			if (mMeshes[Nodes[i].Meshes[k]]->type == Mesh::MeshType::SkinedBumpMesh) {
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
		Material mat;
		//ifs.read((char*)mat, sizeof(Material));

		int namelen = 0;
		ifs.read((char*)&namelen, sizeof(int));
		char name[512] = {};
		ifs.read((char*)name, namelen * sizeof(char));
		name[namelen] = 0;
		memcpy_s(mat.name, 40, name, 40);
		mat.name[39] = 0;

		ifs.read((char*)&mat.clr.diffuse, sizeof(float) * 4);

		ifs.read((char*)&mat.metallicFactor, sizeof(float));

		float smoothness = 0;
		ifs.read((char*)&smoothness, sizeof(float));
		mat.roughnessFactor = 1.0f - smoothness;

		ifs.read((char*)&mat.clr.bumpscaling, sizeof(float));

		vec4 tiling, offset = 0;
		vec4 tiling2, offset2 = 0;
		ifs.read((char*)&tiling, sizeof(float) * 2);
		ifs.read((char*)&offset, sizeof(float) * 2);
		ifs.read((char*)&tiling2, sizeof(float) * 2);
		ifs.read((char*)&offset2, sizeof(float) * 2);
		mat.TilingX = tiling.x;
		mat.TilingY = tiling.y;
		mat.TilingOffsetX = offset.x;
		mat.TilingOffsetY = offset.y;

		bool isTransparent = false;
		ifs.read((char*)&isTransparent, sizeof(bool));
		if (isTransparent) {
			mat.gltf_alphaMode = mat.Blend;
		}
		else mat.gltf_alphaMode = mat.Opaque;

		bool emissive = 0;
		ifs.read((char*)&emissive, sizeof(bool));
		if (emissive) {
			mat.clr.emissive = vec4(1, 1, 1, 1);
		}
		else {
			mat.clr.emissive = vec4(0, 0, 0, 0);
		}

		ifs.read((char*)&mat.ti.BaseColor, sizeof(int));
		ifs.read((char*)&mat.ti.Normal, sizeof(int));
		ifs.read((char*)&mat.ti.Metalic, sizeof(int));
		ifs.read((char*)&mat.ti.AmbientOcculsion, sizeof(int));
		ifs.read((char*)&mat.ti.Roughness, sizeof(int));
		ifs.read((char*)&mat.ti.Emissive, sizeof(int));

		int diffuse2, normal2 = 0;
		ifs.read((char*)&diffuse2, sizeof(int));
		ifs.read((char*)&normal2, sizeof(int));

		mat.ShiftTextureIndexs(TextureTableStart);
		mat.SetDescTable();
		//mMaterials[i] = mat;
		game.MaterialTable.push_back(mat);
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

template <bool isSkinMesh>
void Model::Render(GPUCmd& cmd, matrix worldMatrix, void* pGameobject)
{
	RootNode->Render<isSkinMesh>(this, cmd, worldMatrix, pGameobject);
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

int Shape::GetShapeIndex(string meshName)
{
	for (int i = 0; i < Shape::ShapeNameArr.size(); ++i) {
		if (Shape::ShapeNameArr[i] == meshName) {
			return i;
		}
	}
	return -1;
}

int Shape::AddShapeName(string meshName)
{
	int r = ShapeNameArr.size();
	ShapeNameArr.push_back(meshName);
	return r;
}

int Shape::AddMesh(string name, Mesh* ptr)
{
	int index = AddShapeName(name);
	Shape s;
	s.SetMesh(ptr);
	StrToShapeMap.insert(pair<string, Shape>(name, s));
	//Shape::IndexToShapeMap.insert(pair<int, Shape>(index, s));
	return index;
}

int Shape::AddModel(string name, Model* ptr)
{
	int index = AddShapeName(name);
	Shape s;
	s.SetModel(ptr);
	StrToShapeMap.insert(pair<string, Shape>(name, s));
	//IndexToShapeMap.insert(pair<int, Shape>(index, s));
	return index;
}

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
		for (int i = 0; i < subMeshNum + 1; ++i) {
			SubMeshIndexStart[i] = (rmesh.IBStartOffset[i] - rmesh.IBStartOffset[0]) / sizeof(UINT);
		}

		IndexBufferView.BufferLocation = RayTracingMesh::indexBuffer->GetGPUVirtualAddress() + rmesh.IBStartOffset[0];
		IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
		IndexBufferView.SizeInBytes = rmesh.IBStartOffset[subMeshNum] - rmesh.IBStartOffset[0];
		IndexNum = (rmesh.IBStartOffset[subMeshNum] - rmesh.IBStartOffset[0]) / sizeof(UINT);
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
	ToOffsetMatrixsCB = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1);
	ToOffsetMatrixsCB.resource->Map(0, NULL, (void**)&OffsetMatrixs);

	//make DefaultToWorldArr, ToLocalArr
	for (int i = 0; i < MatrixCount; ++i) {
		OffsetMatrixs[i] = NodeLocalMatrixs[i];
	}

	ToOffsetMatrixsCB.resource->Unmap(0, nullptr);

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
}

void BumpSkinMesh::BatchRender(ID3D12GraphicsCommandList* pCommandList)
{
}

void Mesh::InstancingStruct::Init(unsigned int capacity, Mesh* _mesh)
{
	Capacity = capacity;
	InstanceSize = 0;
	mesh = _mesh;
	InstanceDataArr = nullptr;
	StructuredBuffer = gd.CreateCommitedGPUBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(RenderInstanceData) * Capacity, 1);
	StructuredBuffer.resource->Map(0, NULL, (void**)&InstanceDataArr);
	gd.ShaderVisibleDescPool.ImmortalAlloc_InstancingSRV(&InstancingSRVIndex, 1);

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

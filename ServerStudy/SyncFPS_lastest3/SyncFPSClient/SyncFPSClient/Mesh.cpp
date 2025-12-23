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
			//ÁÂÇ¥
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
			// uv ÁÂÇ¥
			XMFLOAT3 uv;
			in >> uv.x;
			in >> uv.y;
			in >> uv.z;
			temp_uv.push_back(XMFLOAT2(uv.x, uv.y));
		}
		else if (strcmp(rstr, "vn") == 0) {
			// ³ë¸Ö
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
	VertexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, temp_vertices.size() * sizeof(Vertex), 1);
	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, temp_vertices.size() * sizeof(Vertex), 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &temp_vertices[0], &VertexUploadBuffer, &VertexBuffer, true);
	VertexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	VertexBufferView.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
	VertexBufferView.StrideInBytes = m_nStride;
	VertexBufferView.SizeInBytes = m_nStride * m_nVertices;

	IndexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(TriangleIndex) * TrianglePool.size(), 1);
	IndexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(TriangleIndex) * TrianglePool.size(), 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &TrianglePool[0], &IndexUploadBuffer, &IndexBuffer, true);
	IndexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_INDEX_BUFFER);
	IndexBufferView.BufferLocation = IndexBuffer.resource->GetGPUVirtualAddress();
	IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	IndexBufferView.SizeInBytes = sizeof(TriangleIndex) * TrianglePool.size();

	IndexNum = TrianglePool.size() * 3;
	VertexNum = temp_vertices.size();
	topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	/*MeshOBB = BoundingOrientedBox(XMFLOAT3(0.0f, 0.0f, 0.0f), MAXpos, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));*/
}

void Mesh::Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum)
{
	// question : what is StartSlot parameter??
	ui32 SlotNum = 0;
	pCommandList->IASetVertexBuffers(SlotNum, 1, &VertexBufferView);
	pCommandList->IASetPrimitiveTopology(topology);
	if (IndexBuffer.resource)
	{
		pCommandList->IASetIndexBuffer(&IndexBufferView);
		pCommandList->DrawIndexedInstanced(IndexNum, instanceNum, 0, 0, 0);
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

void Mesh::CreateGroundMesh(ID3D12GraphicsCommandList* pCommandList)
{
	std::vector<Vertex> vertices;
	vertices.push_back(Vertex(XMFLOAT3(-10.0f, 0.0f, -10.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(10.0f, 0.0f, -10.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(10.0f, 0.0f, 10.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)));
	vertices.push_back(Vertex(XMFLOAT3(-10.0f, 0.0f, 10.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)));

	std::vector<UINT> indices;
	indices.push_back(0);
	indices.push_back(1);
	indices.push_back(2);

	indices.push_back(2);
	indices.push_back(3);
	indices.push_back(0);

	int nVertices = vertices.size();
	int nStride = sizeof(Vertex);

	VertexBuffer = gd.CreateCommitedGPUBuffer(pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	gd.UploadToCommitedGPUBuffer(pCommandList, &vertices[0], &VertexUploadBuffer, &VertexBuffer, true);

	VertexBuffer.AddResourceBarrierTransitoinToCommand(pCommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	VertexBufferView.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
	VertexBufferView.StrideInBytes = nStride;
	VertexBufferView.SizeInBytes = nStride * nVertices;

	IndexBuffer = gd.CreateCommitedGPUBuffer(pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, indices.size() * sizeof(UINT), 1);
	IndexUploadBuffer = gd.CreateCommitedGPUBuffer(pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, indices.size() * sizeof(UINT), 1);
	gd.UploadToCommitedGPUBuffer(pCommandList, &indices[0], &IndexUploadBuffer, &IndexBuffer, true);

	IndexBuffer.AddResourceBarrierTransitoinToCommand(pCommandList, D3D12_RESOURCE_STATE_INDEX_BUFFER);

	IndexBufferView.BufferLocation = IndexBuffer.resource->GetGPUVirtualAddress();
	IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	IndexBufferView.SizeInBytes = indices.size() * sizeof(UINT);

	IndexNum = indices.size();
	VertexNum = vertices.size();
	topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
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

	VertexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &vertices[0], &VertexUploadBuffer, &VertexBuffer, true);

	VertexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	VertexBufferView.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
	VertexBufferView.StrideInBytes = nStride;
	VertexBufferView.SizeInBytes = nStride * nVertices;

	IndexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, indices.size() * sizeof(UINT), 1);
	IndexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, indices.size() * sizeof(UINT), 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &indices[0], &IndexUploadBuffer, &IndexBuffer, true);

	IndexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_INDEX_BUFFER);

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
	VertexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, temp_vertices.size() * sizeof(Vertex), 1);
	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, temp_vertices.size() * sizeof(Vertex), 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &temp_vertices[0], &VertexUploadBuffer, &VertexBuffer, true);
	VertexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	VertexBufferView.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
	VertexBufferView.StrideInBytes = m_nStride;
	VertexBufferView.SizeInBytes = m_nStride * m_nVertices;

	IndexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(TriangleIndex) * TrianglePool.size(), 1);
	IndexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(TriangleIndex) * TrianglePool.size(), 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &TrianglePool[0], &IndexUploadBuffer, &IndexBuffer, true);
	IndexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_INDEX_BUFFER);
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

	VertexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &vertices[0], &VertexUploadBuffer, &VertexBuffer, true);

	VertexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	VertexBufferView.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
	VertexBufferView.StrideInBytes = nStride;
	VertexBufferView.SizeInBytes = nStride * nVertices;

	IndexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, indices.size() * sizeof(UINT), 1);
	IndexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, indices.size() * sizeof(UINT), 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &indices[0], &IndexUploadBuffer, &IndexBuffer, true);

	IndexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_INDEX_BUFFER);

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

	VertexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &vertices[0], &VertexUploadBuffer, &VertexBuffer, true);

	VertexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	VertexBufferView.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
	VertexBufferView.StrideInBytes = nStride;
	VertexBufferView.SizeInBytes = nStride * nVertices;

	IndexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, indices.size() * sizeof(UINT), 1);
	IndexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, indices.size() * sizeof(UINT), 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &indices[0], &IndexUploadBuffer, &IndexBuffer, true);

	IndexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_INDEX_BUFFER);

	IndexBufferView.BufferLocation = IndexBuffer.resource->GetGPUVirtualAddress();
	IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	IndexBufferView.SizeInBytes = indices.size() * sizeof(UINT);

	IndexNum = indices.size();
	VertexNum = vertices.size();
	topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

BumpMesh::BumpMesh()
{
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

	VertexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, nVertices * nStride, 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &vertices[0], &VertexUploadBuffer, &VertexBuffer, true);

	VertexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	VertexBufferView.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
	VertexBufferView.StrideInBytes = nStride;
	VertexBufferView.SizeInBytes = nStride * nVertices;

	IndexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, indices.size() * sizeof(UINT), 1);
	IndexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, indices.size() * sizeof(UINT), 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &indices[0], &IndexUploadBuffer, &IndexBuffer, true);

	IndexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_INDEX_BUFFER);

	IndexBufferView.BufferLocation = IndexBuffer.resource->GetGPUVirtualAddress();
	IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	IndexBufferView.SizeInBytes = indices.size() * sizeof(UINT);

	IndexNum = indices.size();
	VertexNum = vertices.size();
	topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

void BumpMesh::CreateMesh_FromVertexAndIndexData(vector<Vertex>& vert, vector<TriangleIndex>& inds)
{
	int m_nVertices = vert.size();
	int m_nStride = sizeof(Vertex);

	VertexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, vert.size() * sizeof(Vertex), 1);
	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, vert.size() * sizeof(Vertex), 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &vert[0], &VertexUploadBuffer, &VertexBuffer, true);
	VertexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	VertexBufferView.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
	VertexBufferView.StrideInBytes = m_nStride;
	VertexBufferView.SizeInBytes = m_nStride * m_nVertices;

	IndexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(TriangleIndex) * inds.size(), 1);
	IndexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(TriangleIndex) * inds.size(), 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &inds[0], &IndexUploadBuffer, &IndexBuffer, true);
	IndexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_INDEX_BUFFER);
	IndexBufferView.BufferLocation = IndexBuffer.resource->GetGPUVirtualAddress();
	IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	IndexBufferView.SizeInBytes = sizeof(TriangleIndex) * inds.size();

	IndexNum = inds.size() * 3;
	VertexNum = vert.size();
	topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

void BumpMesh::ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering)
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
			//ÁÂÇ¥
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
			// uv ÁÂÇ¥
			XMFLOAT3 uv;
			in >> uv.x;
			in >> uv.y;
			in >> uv.z;
			uv.y = 1.0f - uv.y;
			temp_uv.push_back(XMFLOAT2(uv.x, uv.y));
		}
		else if (strcmp(rstr, "vn") == 0) {
			// ³ë¸Ö
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

	int m_nVertices = temp_vertices.size();
	int m_nStride = sizeof(Vertex);
	//m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// error.. why vertex buffer and index buffer do not input? 
	// maybe.. State Error.
	VertexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, temp_vertices.size() * sizeof(Vertex), 1);
	VertexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, temp_vertices.size() * sizeof(Vertex), 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &temp_vertices[0], &VertexUploadBuffer, &VertexBuffer, true);
	VertexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	VertexBufferView.BufferLocation = VertexBuffer.resource->GetGPUVirtualAddress();
	VertexBufferView.StrideInBytes = m_nStride;
	VertexBufferView.SizeInBytes = m_nStride * m_nVertices;

	IndexBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(TriangleIndex) * TrianglePool.size(), 1);
	IndexUploadBuffer = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_DIMENSION_BUFFER, sizeof(TriangleIndex) * TrianglePool.size(), 1);
	gd.UploadToCommitedGPUBuffer(gd.pCommandList, &TrianglePool[0], &IndexUploadBuffer, &IndexBuffer, true);
	IndexBuffer.AddResourceBarrierTransitoinToCommand(gd.pCommandList, D3D12_RESOURCE_STATE_INDEX_BUFFER);
	IndexBufferView.BufferLocation = IndexBuffer.resource->GetGPUVirtualAddress();
	IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	IndexBufferView.SizeInBytes = sizeof(TriangleIndex) * TrianglePool.size();

	IndexNum = TrianglePool.size() * 3;
	VertexNum = temp_vertices.size();
	topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	/*MeshOBB = BoundingOrientedBox(XMFLOAT3(0.0f, 0.0f, 0.0f), MAXpos, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));*/
}

void BumpMesh::Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum)
{
	Mesh::Render(pCommandList, instanceNum);
}

void BumpMesh::Release()
{
	Mesh::Release();
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
	DescHandle hDesc;
	DescHandle hOriginDesc;
	D3D12_CPU_DESCRIPTOR_HANDLE hcpu;
	gd.ShaderVisibleDescPool.ImmortalAllocDescriptorTable(&hDesc.hcpu, &hDesc.hgpu, 6);
	// [5 * tex] [1 * cbv]
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

	int inc = gd.CBV_SRV_UAV_Desc_IncrementSiz;

	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, diffuseTex->hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	hDesc += inc;
	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, normalTex->hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	hDesc += inc;
	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, ambientTex->hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	hDesc += inc;
	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, MetalicTex->hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	hDesc += inc;
	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, roughnessTex->hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	hGPU = hOriginDesc.hgpu;

	UINT ncbElementBytes = ((sizeof(MaterialCB_Data) + 255) & ~255); //256ÀÇ ¹è¼ö
	CB_Resource = gd.CreateCommitedGPUBuffer(gd.pCommandList, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_DIMENSION_BUFFER, ncbElementBytes, 1);
	CB_Resource.resource->Map(0, NULL, (void**)&CBData);
	*CBData = GetMatCB();

	hDesc += inc;

	CB_Resource.resource->Unmap(0, NULL);

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc;
	cbv_desc.BufferLocation = CB_Resource.resource->GetGPUVirtualAddress();
	cbv_desc.SizeInBytes = ncbElementBytes;
	gd.pDevice->CreateConstantBufferView(&cbv_desc, hDesc.hcpu);
	CB_Resource.hCpu = hDesc.hcpu;
	CB_Resource.hGpu = hDesc.hgpu;
}

void Material::SetDescTable_NOTCBV()
{
	DescHandle hDesc;
	DescHandle hOriginDesc;
	D3D12_CPU_DESCRIPTOR_HANDLE hcpu;
	gd.ShaderVisibleDescPool.ImmortalAllocDescriptorTable(&hDesc.hcpu, &hDesc.hgpu, 5);
	// [5 * tex] [1 * cbv]
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

	int inc = gd.CBV_SRV_UAV_Desc_IncrementSiz;

	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, diffuseTex->hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	hDesc += inc;
	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, normalTex->hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	hDesc += inc;
	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, ambientTex->hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	hDesc += inc;
	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, MetalicTex->hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	hDesc += inc;
	gd.pDevice->CopyDescriptorsSimple(1, hDesc.hcpu, roughnessTex->hCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	hGPU = hOriginDesc.hgpu;
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

void ModelNode::Render(void* model, ID3D12GraphicsCommandList* cmdlist, const matrix& parentMat)
{
	Model* pModel = (Model*)model;
	XMMATRIX sav = XMMatrixMultiply(transform, parentMat);
	if (numMesh != 0 && Meshes != nullptr) {
		matrix m = sav;
		m.transpose();

		//set world matrix in shader
		cmdlist->SetGraphicsRoot32BitConstants(1, 16, &m, 0);
		//set texture in shader
		//game.MyPBRShader1->SetTextureCommand()
		for (int i = 0; i < numMesh; ++i) {
			int materialIndex = ((BumpMesh*)pModel->mMeshes[Meshes[i]])->material_index;
			Material& mat = game.MaterialTable[materialIndex];
			//GPUResource* diffuseTex = &game.DefaultTex;
			//if (mat.ti.Diffuse >= 0) diffuseTex = game.TextureTable[mat.ti.Diffuse];
			//GPUResource* normalTex = &game.DefaultNoramlTex;
			//if (mat.ti.Normal >= 0) normalTex = game.TextureTable[mat.ti.Normal];
			//GPUResource* ambientTex = &game.DefaultTex;
			//if (mat.ti.AmbientOcculsion >= 0) ambientTex = game.TextureTable[mat.ti.AmbientOcculsion];
			//GPUResource* MetalicTex = &game.DefaultAmbientTex;
			//if (mat.ti.Metalic >= 0) MetalicTex = game.TextureTable[mat.ti.Metalic];
			//GPUResource* roughnessTex = &game.DefaultAmbientTex;
			//if (mat.ti.Roughness >= 0) roughnessTex = game.TextureTable[mat.ti.Roughness];
			//game.MyPBRShader1->SetTextureCommand(diffuseTex, normalTex, ambientTex, MetalicTex, roughnessTex);

			gd.pCommandList->SetGraphicsRootDescriptorTable(3, mat.hGPU);
			gd.pCommandList->SetGraphicsRootDescriptorTable(4, mat.CB_Resource.hGpu);

			pModel->mMeshes[Meshes[i]]->Render(cmdlist, 1);
		}
	}

	if (numChildren != 0 && Childrens != nullptr) {
		for (int i = 0; i < numChildren; ++i) {
			Childrens[i]->Render(model, cmdlist, sav);
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

void Model::LoadModelFile(string filename)
{
	ifstream ifs{ filename, ios_base::binary };
	ifs.read((char*)&mNumMeshes, sizeof(unsigned int));
	mMeshes = new Mesh * [mNumMeshes];
	vertice = new vector<BumpMesh::Vertex>[mNumMeshes];
	indice = new vector<TriangleIndex>[mNumMeshes];

	ifs.read((char*)&nodeCount, sizeof(unsigned int));
	Nodes = new ModelNode[nodeCount];

	//new0
	ifs.read((char*)&mNumTextures, sizeof(unsigned int));
	ifs.read((char*)&mNumMaterials, sizeof(unsigned int));

	MaterialTableStart = game.MaterialTable.size();
	for (int i = 0; i < mNumMeshes; ++i) {
		BumpMesh* mesh = new BumpMesh();
		XMFLOAT3 AABB[2];
		ifs.read((char*)AABB, sizeof(XMFLOAT3) * 2);
		mesh->SetOBBDataWithAABB(AABB[0], AABB[1]);

		//new1
		int material_subindex = 0;
		ifs.read((char*)&material_subindex, sizeof(int));
		mesh->material_index = MaterialTableStart + material_subindex;

		unsigned int vertSiz = 0;
		unsigned int indexSiz = 0;
		ifs.read((char*)&vertSiz, sizeof(unsigned int));
		ifs.read((char*)&indexSiz, sizeof(unsigned int));

		vector<BumpMesh::Vertex>& vertices = vertice[i];
		vector<TriangleIndex>& indices = indice[i];
		vertices.reserve(vertSiz); vertices.resize(vertSiz);
		indices.reserve(indexSiz); indices.resize(indexSiz);

		for (int k = 0; k < vertSiz; ++k) {
			ifs.read((char*)&vertice[i][k].position, sizeof(XMFLOAT3));
			ifs.read((char*)&vertice[i][k].uv, sizeof(XMFLOAT2));
			ifs.read((char*)&vertice[i][k].normal, sizeof(XMFLOAT3));
			ifs.read((char*)&vertice[i][k].tangent, sizeof(XMFLOAT3));

			// non use
			XMFLOAT3 bitangent;
			ifs.read((char*)&bitangent, sizeof(XMFLOAT3));
		}
		for (int k = 0; k < indexSiz; ++k) {
			ifs.read((char*)&indice[i][k], sizeof(TriangleIndex));
		}

		mesh->CreateMesh_FromVertexAndIndexData(vertices, indices);
		mMeshes[i] = mesh;
	}

	/*gd.pCommandList->Close();
	ID3D12CommandList* ppCommandLists[] = { gd.pCommandList };
	gd.pCommandQueue->ExecuteCommandLists(1, ppCommandLists);
	gd.WaitGPUComplete();*/

	for (int i = 0; i < nodeCount; ++i) {
		int namelen = 0;
		ifs.read((char*)&namelen, sizeof(int));
		char name[256] = {};
		ifs.read((char*)name, namelen);
		Nodes[i].name = name;

		ifs.read((char*)&Nodes[i].transform, sizeof(XMMATRIX));

		int parent = 0;
		ifs.read((char*)&parent, sizeof(int));
		if (parent < 0) Nodes[i].parent = nullptr;
		else Nodes[i].parent = &Nodes[parent];

		ifs.read((char*)&Nodes[i].numChildren, sizeof(unsigned int));
		Nodes[i].Childrens = new ModelNode * [Nodes[i].numChildren];
		ifs.read((char*)&Nodes[i].numMesh, sizeof(unsigned int));
		Nodes[i].Meshes = new unsigned int[Nodes[i].numMesh];
		for (int k = 0; k < Nodes[i].numChildren; ++k) {
			int child = 0;
			ifs.read((char*)&child, sizeof(int));
			if (child < 0) Nodes[i].Childrens[k] = nullptr;
			else Nodes[i].Childrens[k] = &Nodes[child];
		}

		ifs.read((char*)&Nodes[i].Meshes[0], sizeof(int) * Nodes[i].numMesh);
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
			//make dds texture in DDSFilename path
			int width = 0, height = 0;
			ifs.read((char*)&width, sizeof(int));
			ifs.read((char*)&height, sizeof(int));
			int datasiz = 4 * width * height;
			void* pdata = malloc(4 * width * height);
			ifs.read((char*)pdata, datasiz);

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

		/*GPUResource* Texture = new GPUResource();
		game.TextureTable.push_back(Texture);
		Texture->resource = nullptr;
		int width = 0, height = 0;
		ifs.read((char*)&width, sizeof(int));
		ifs.read((char*)&height, sizeof(int));
		int datasiz = 4 * width * height;
		void* pdata = malloc(4 * width * height);
		ifs.read((char*)pdata, datasiz);
		Texture->CreateTexture_fromImageBuffer(width, height, (BYTE*)pdata, DXGI_FORMAT_BC2_UNORM);
		free(pdata);*/
	}

	/*gd.pCommandAllocator->Reset();
	gd.pCommandList->Reset(gd.pCommandAllocator, NULL);*/

	//mMaterials = new Material * [mNumMaterials];
	for (int i = 0; i < mNumMaterials; ++i) {
		Material newmat;
		ifs.read((char*)&newmat, Material::MaterialSiz_withoutGPUResource);
		ZeroMemory(&newmat.name, sizeof(string));
		newmat.ShiftTextureIndexs(MaterialTableStart);

		newmat.SetDescTable();
		game.MaterialTable.push_back(newmat);
		//mMaterials[i] = mat;
	}

	RootNode = &Nodes[0];

	BindPose.resize(nodeCount);
	for (int i = 0; i < nodeCount; ++i) {
		BindPose[i] = Nodes[i].transform;
	}

	BakeAABB();

	delete[] vertice;
	delete[] indice;
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

	MaterialTableStart = game.MaterialTable.size();
	for (int i = 0; i < mNumMeshes; ++i) {
		BumpMesh* mesh = new BumpMesh();
		XMFLOAT3 AABB[2];
		ifs.read((char*)AABB, sizeof(XMFLOAT3) * 2);
		mesh->SetOBBDataWithAABB(AABB[0], AABB[1]);

		//new1
		//ifs.read((char*)&mesh->material_index, sizeof(int));

		XMFLOAT3 MAABB[2];
		unsigned int vertSiz = 0;
		unsigned int indexSiz = 0;
		ifs.read((char*)&vertSiz, sizeof(unsigned int));
		ifs.read((char*)&indexSiz, sizeof(unsigned int));

		vector<BumpMesh::Vertex> vertices;
		unsigned int triSiz = indexSiz / 3;
		vector<TriangleIndex> indices;
		vertices.reserve(vertSiz); vertices.resize(vertSiz);
		indices.reserve(triSiz); indices.resize(triSiz);

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

			ifs.read((char*)&vertices[k].uv, sizeof(XMFLOAT2));
			ifs.read((char*)&vertices[k].normal, sizeof(XMFLOAT3));
			ifs.read((char*)&vertices[k].tangent, sizeof(XMFLOAT3));

			// non use
			XMFLOAT3 bitangent;
			ifs.read((char*)&bitangent, sizeof(XMFLOAT3));
		}
		for (int k = 0; k < triSiz; ++k) {
			ifs.read((char*)&indices[k], sizeof(TriangleIndex));
		}

		float unitMulRate = 1 * AABB[0].x / MAABB[0].x;
		for (int k = 0; k < vertSiz; ++k) {
			vertices[k].position.x *= unitMulRate;
			vertices[k].position.y *= unitMulRate;
			vertices[k].position.z *= unitMulRate;
		}

		mesh->CreateMesh_FromVertexAndIndexData(vertices, indices);
		mMeshes[i] = mesh;
	}

	/*gd.pCommandList->Close();
	ID3D12CommandList* ppCommandLists[] = { gd.pCommandList };
	gd.pCommandQueue->ExecuteCommandLists(1, ppCommandLists);
	gd.WaitGPUComplete();*/

	for (int i = 0; i < nodeCount; ++i) {
		int namelen = 0;
		ifs.read((char*)&namelen, sizeof(int));
		char name[256] = {};
		ifs.read((char*)name, namelen);
		name[namelen] = 0;
		Nodes[i].name = name;

		vec4 pos;
		ifs.read((char*)&pos, sizeof(float) * 3);
		vec4 rot = 0;
		ifs.read((char*)&rot, sizeof(float) * 3);
		vec4 scale = 0;
		ifs.read((char*)&scale, sizeof(float) * 3);

		if (i == 0) {
			matrix mat;
			mat.Id();
			Nodes[i].transform = mat;
		}
		else {
			matrix mat;
			mat.Id();
			rot *= 3.141592f / 180.0f;
			mat *= XMMatrixScaling(scale.x, scale.y, scale.z);
			mat *= XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z);
			mat.pos.f3 = pos.f3;
			mat.pos.w = 1;

			Nodes[i].transform = mat;
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

		for (int k = 0; k < Nodes[i].numMesh; ++k) {
			ifs.read((char*)&Nodes[i].Meshes[k], sizeof(int));
			int num_materials = 0;
			ifs.read((char*)&num_materials, sizeof(int));
			for (int u = 0; u < num_materials; ++u) {
				int material_id = 0;
				ifs.read((char*)&material_id, sizeof(int));
				material_id += MaterialTableStart;

				((BumpMesh*)mMeshes[Nodes[i].Meshes[k]])->material_index = material_id;
			}
		}
		//ifs.read((char*)&Nodes[i].Meshes[0], sizeof(int) * Nodes[i].numMesh);

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

	/*gd.pCommandAllocator->Reset();
	gd.pCommandList->Reset(gd.pCommandAllocator, NULL);*/

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

void Model::Render(ID3D12GraphicsCommandList* cmdlist, matrix worldMatrix, Shader::RegisterEnum sre)
{
	if (sre == Shader::RegisterEnum::RenderShadowMap) {
		game.MyPBRShader1->Add_RegisterShaderCommand(cmdlist, Shader::RegisterEnum::RenderShadowMap);

		matrix xmf4x4Projection = game.MySpotLight.viewport.ProjectMatrix;
		matrix view = game.MySpotLight.View;
		view *= xmf4x4Projection;

		XMFLOAT4X4 xmf4x4View;
		XMStoreFloat4x4(&xmf4x4View, XMMatrixTranspose(view));
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4View, 0);
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16);
		gd.pCommandList->SetGraphicsRootConstantBufferView(2, game.LightCB_withShadowResource.resource->GetGPUVirtualAddress());
		gd.pCommandList->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.m_pDescritorHeap);

		RootNode->Render(this, cmdlist, worldMatrix);
	}
	else if (sre == Shader::RegisterEnum::RenderWithShadow) {
		game.MyPBRShader1->Add_RegisterShaderCommand(cmdlist, Shader::RegisterEnum::RenderWithShadow);

		matrix xmf4x4Projection = gd.viewportArr[0].ProjectMatrix;
		matrix xmf4x4View = gd.viewportArr[0].ViewMatrix;
		xmf4x4View *= xmf4x4Projection;
		xmf4x4View.transpose();
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4View, 0);
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16);
		gd.pCommandList->SetGraphicsRootConstantBufferView(2, game.LightCB_withShadowResource.resource->GetGPUVirtualAddress());
		gd.pCommandList->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.m_pDescritorHeap);
		game.MyPBRShader1->SetShadowMapCommand(game.MySpotLight.renderDesc);

		RootNode->Render(this, cmdlist, worldMatrix);
	}
	else if (sre == Shader::RegisterEnum::RenderNormal) {
		// no shadow render
		game.MyPBRShader1->Add_RegisterShaderCommand(cmdlist);

		matrix xmf4x4Projection = gd.viewportArr[0].ProjectMatrix;
		matrix xmf4x4View = gd.viewportArr[0].ViewMatrix;
		xmf4x4View *= xmf4x4Projection;
		xmf4x4View.transpose();
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4View, 0);
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16);
		gd.pCommandList->SetGraphicsRootConstantBufferView(2, game.LightCBResource.resource->GetGPUVirtualAddress());
		gd.pCommandList->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.m_pDescritorHeap);

		RootNode->Render(this, cmdlist, worldMatrix);
	}
	else if (sre == Shader::RegisterEnum::RenderInnerMirror) {
		// no shadow render
		game.MyPBRShader1->Add_RegisterShaderCommand(cmdlist, sre);

		matrix xmf4x4Projection = gd.viewportArr[0].ProjectMatrix;
		matrix xmf4x4View = gd.viewportArr[0].ViewMatrix;
		xmf4x4View *= xmf4x4Projection;
		xmf4x4View.transpose();
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4View, 0);
		gd.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &gd.viewportArr[0].Camera_Pos, 16);
		gd.pCommandList->SetGraphicsRootConstantBufferView(2, game.LightCBResource.resource->GetGPUVirtualAddress());
		gd.pCommandList->SetDescriptorHeaps(1, &gd.ShaderVisibleDescPool.m_pDescritorHeap);

		RootNode->Render(this, cmdlist, worldMatrix);
	}
	//game.MyShader->Add_RegisterShaderCommand(cmdlist);
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

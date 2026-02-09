#include "stdafx.h"
#include "Render.h"
#include "Game.h"
#include "GameObject.h"
#include "Mesh.h"

Mesh* BulletRay::mesh = nullptr;
ViewportData* Game::renderViewPort = nullptr;

GameObject::GameObject()
{
	isExist = true;
	tickLVelocity = vec4(0, 0, 0, 0);
}

GameObject::~GameObject()
{
}

void GameObject::Update(float delatTime)
{
	PositionInterpolation(delatTime);
}

void GameObject::Render()
{
	if (!m_pMesh || !m_pShader) {
		static bool warned = false;
		if (!warned) {
			char dbg[256];
			snprintf(dbg, sizeof(dbg),
				"[RENDER][SKIP] obj=%p mesh=%p shader=%p (null -> not drawing)\n",
				(void*)this, (void*)m_pMesh, (void*)m_pShader);
			OutputDebugStringA(dbg);
			warned = true;
		}
		return;
	}

	if (rmod == eRenderMeshMod::single_Mesh) {
		XMFLOAT4X4 xmf4x4World;
		XMStoreFloat4x4(&xmf4x4World, DirectX::XMMatrixTranspose(m_worldMatrix));

		gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &xmf4x4World, 0);

		//game.MyDiffuseTextureShader->SetTextureCommand(&gd.GlobalTextureArr[diffuseTextureIndex]);
		int materialIndex = MaterialIndex;
		Material& mat = game.MaterialTable[materialIndex];
		gd.pCommandList->SetGraphicsRootDescriptorTable(3, mat.hGPU);
		gd.pCommandList->SetGraphicsRootDescriptorTable(4, mat.CB_Resource.hGpu);

		m_pMesh->Render(gd.pCommandList, 1);
	}
	else {
		//??
	}
}

void GameObject::SetMesh(Mesh* pMesh)
{
	m_pMesh = pMesh;
}

void GameObject::SetShader(Shader* pShader)
{
	m_pShader = pShader;
}

void GameObject::LookAt(vec4 look, vec4 up)
{
	m_worldMatrix.LookAt(look, up);
}

void GameObject::PositionInterpolation(float deltaTime)
{
	float pow = deltaTime * 10;
	Destpos.w = 1;
	m_worldMatrix.pos = (1.0f - pow) * m_worldMatrix.pos + pow * Destpos;
}

BulletRay::BulletRay()
{
}

BulletRay::BulletRay(vec4 s, vec4 dir, float dist) {
	start = s;
	direction = dir;
	distance = dist;
	t = 0;
}

matrix BulletRay::GetRayMatrix() {
	matrix mat;
	matrix smat;
	mat.LookAt(direction);
	float rate = t / remainTime;
	mat.right *= 1 - rate;
	mat.up *= 1 - rate;
	mat.look *= distance;
	mat.pos = start;
	mat.pos.w = 1;
	return mat;
}

void BulletRay::Update() {
	if (direction.fast_square_of_len3 > 0) {
		t += game.DeltaTime;
		if (t > remainTime) {
			direction = 0;
		}
	}
}

void BulletRay::Render() {
	if (direction.fast_square_of_len3 > 0) {
		matrix mat = GetRayMatrix();
		mat.transpose();
		gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &mat, 0);
		mesh->Render(gd.pCommandList, 1);
	}
}

void Hierarchy_Object::Render_Inherit(matrix parent_world, Shader::RegisterEnum sre)
{
	XMMATRIX sav = XMMatrixMultiply(m_worldMatrix, parent_world);
	BoundingOrientedBox obb = GetOBBw(sav);
	if (obb.Extents.x > 0 && Game::renderViewPort->m_xmFrustumWorld.Intersects(obb)) {
		if (rmod == GameObject::eRenderMeshMod::single_Mesh && m_pMesh != nullptr) {
			matrix m = sav;
			m.transpose();

			//set world matrix in shader
			gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &m, 0);
			//set texture in shader
			//game.MyPBRShader1->SetTextureCommand()
			int materialIndex = MaterialIndex;
			Material& mat = game.MaterialTable[materialIndex];
			/*GPUResource* diffuseTex = &game.DefaultTex;
			if (mat.ti.Diffuse >= 0) diffuseTex = game.TextureTable[mat.ti.Diffuse];
			GPUResource* normalTex = &game.DefaultNoramlTex;
			if (mat.ti.Normal >= 0) normalTex = game.TextureTable[mat.ti.Normal];
			GPUResource* ambientTex = &game.DefaultTex;
			if (mat.ti.AmbientOcculsion >= 0) ambientTex = game.TextureTable[mat.ti.AmbientOcculsion];
			GPUResource* MetalicTex = &game.DefaultAmbientTex;
			if (mat.ti.Metalic >= 0) MetalicTex = game.TextureTable[mat.ti.Metalic];
			GPUResource* roughnessTex = &game.DefaultAmbientTex;
			if (mat.ti.Roughness >= 0) roughnessTex = game.TextureTable[mat.ti.Roughness];
			game.MyPBRShader1->SetTextureCommand(diffuseTex, normalTex, ambientTex, MetalicTex, roughnessTex);*/

			gd.pCommandList->SetGraphicsRootDescriptorTable(3, mat.hGPU);
			gd.pCommandList->SetGraphicsRootDescriptorTable(4, mat.CB_Resource.hGpu);
			m_pMesh->Render(gd.pCommandList, 1);
		}
		else if (rmod == GameObject::eRenderMeshMod::Model && pModel != nullptr) {
			pModel->Render(gd.pCommandList, sav, sre);
		}
	}

	for (int i = 0; i < childCount; ++i) {
		childs[i]->Render_Inherit(sav, sre);
	}
}

void Hierarchy_Object::Render_Inherit_CullingOrtho(matrix parent_world, Shader::RegisterEnum sre)
{
	XMMATRIX sav = XMMatrixMultiply(m_worldMatrix, parent_world);
	BoundingOrientedBox obb = GetOBBw(sav);
	if (obb.Extents.x > 0 && Game::renderViewPort->OrthoFrustum.Intersects(obb)) {
		if (rmod == GameObject::eRenderMeshMod::single_Mesh && m_pMesh != nullptr) {
			matrix m = sav;
			m.transpose();

			//set world matrix in shader
			gd.pCommandList->SetGraphicsRoot32BitConstants(1, 16, &m, 0);
			//set texture in shader
			//game.MyPBRShader1->SetTextureCommand()
			int materialIndex = MaterialIndex;
			Material& mat = game.MaterialTable[materialIndex];
			/*GPUResource* diffuseTex = &game.DefaultTex;
			if (mat.ti.Diffuse >= 0) diffuseTex = game.TextureTable[mat.ti.Diffuse];
			GPUResource* normalTex = &game.DefaultNoramlTex;
			if (mat.ti.Normal >= 0) normalTex = game.TextureTable[mat.ti.Normal];
			GPUResource* ambientTex = &game.DefaultTex;
			if (mat.ti.AmbientOcculsion >= 0) ambientTex = game.TextureTable[mat.ti.AmbientOcculsion];
			GPUResource* MetalicTex = &game.DefaultAmbientTex;
			if (mat.ti.Metalic >= 0) MetalicTex = game.TextureTable[mat.ti.Metalic];
			GPUResource* roughnessTex = &game.DefaultAmbientTex;
			if (mat.ti.Roughness >= 0) roughnessTex = game.TextureTable[mat.ti.Roughness];
			game.MyPBRShader1->SetTextureCommand(diffuseTex, normalTex, ambientTex, MetalicTex, roughnessTex);*/

			gd.pCommandList->SetGraphicsRootDescriptorTable(3, mat.hGPU);
			gd.pCommandList->SetGraphicsRootDescriptorTable(4, mat.CB_Resource.hGpu);
			m_pMesh->Render(gd.pCommandList, 1);
		}
		else if (rmod == GameObject::eRenderMeshMod::Model && pModel != nullptr) {
			pModel->Render(gd.pCommandList, sav, sre);
		}
	}

	for (int i = 0; i < childCount; ++i) {
		childs[i]->Render_Inherit_CullingOrtho(sav, sre);
	}
}

//bool Hierarchy_Object::Collision_Inherit(matrix parent_world, BoundingBox bb)
//{
//	XMMATRIX sav = XMMatrixMultiply(m_worldMatrix, parent_world);
//	BoundingOrientedBox obb = GetOBBw(sav);
//	if (obb.Extents.x > 0 && obb.Intersects(bb)) {
//		return true;
//	}
//
//	for (int i = 0; i < childCount; ++i) {
//		if (childs[i]->Collision_Inherit(sav, bb)) return true;
//	}
//
//	return false;
//}
//
//void Hierarchy_Object::InitMapAABB_Inherit(void* origin, matrix parent_world)
//{
//	GameMap* map = (GameMap*)origin;
//
//	XMMATRIX sav = XMMatrixMultiply(m_worldMatrix, parent_world);
//	BoundingOrientedBox obb = GetOBBw(sav);
//	map->ExtendMapAABB(obb);
//
//	for (int i = 0; i < childCount; ++i) {
//		childs[i]->InitMapAABB_Inherit(origin, sav);
//	}
//}

BoundingOrientedBox Hierarchy_Object::GetOBBw(matrix worldMat)
{
	BoundingOrientedBox obb_local;
	if (m_pMesh == nullptr) {
		obb_local.Extents.x = -1;
		return obb_local;
	}
	switch (rmod) {
	case GameObject::eRenderMeshMod::single_Mesh:
		obb_local = m_pMesh->GetOBB();
		break;
	case GameObject::eRenderMeshMod::Model:
		obb_local = pModel->GetOBB();
		break;
	}
	BoundingOrientedBox obb_world;
	obb_local.Transform(obb_world, worldMat);
	return obb_world;
}

void GameMap::ExtendMapAABB(BoundingOrientedBox obb)
{
	vec4 aabb[2];
	gd.GetAABBFromOBB(aabb, obb, true);

	if (AABB[0].x > aabb[0].x) AABB[0].x = aabb[0].x;
	if (AABB[0].y > aabb[0].y) AABB[0].y = aabb[0].y;
	if (AABB[0].z > aabb[0].z) AABB[0].z = aabb[0].z;

	if (AABB[1].x < aabb[1].x) AABB[1].x = aabb[1].x;
	if (AABB[1].y < aabb[1].y) AABB[1].y = aabb[1].y;
	if (AABB[1].z < aabb[1].z) AABB[1].z = aabb[1].z;
}

void GameMap::LoadMap(const char* MapName)
{
	GameMap* map = this;

	string dirName = "Resources/Map/";
	dirName += MapName;

	string MapFilePath = dirName;
	MapFilePath += "/";
	MapFilePath += MapName;
	MapFilePath += ".map";

	ifstream ifs{ MapFilePath, ios_base::binary };
	int nameCount;
	int MeshCount;
	int TextureCount;
	int MaterialCount;
	int ModelCount;
	int gameObjectCount;
	ifs.read((char*)&nameCount, sizeof(int));
	ifs.read((char*)&MeshCount, sizeof(int));
	ifs.read((char*)&TextureCount, sizeof(int));
	ifs.read((char*)&MaterialCount, sizeof(int));
	ifs.read((char*)&ModelCount, sizeof(int));
	ifs.read((char*)&gameObjectCount, sizeof(int));
	map->name.reserve(nameCount);
	map->name.resize(nameCount);
	map->meshes.reserve(MeshCount);
	map->meshes.resize(MeshCount);
	map->models.reserve(ModelCount);
	map->models.resize(ModelCount);
	map->MapObjects.reserve(gameObjectCount);
	map->MapObjects.resize(gameObjectCount);

	for (int i = 0; i < nameCount; ++i) {
		char TempBuffer[1024] = {};
		string name;
		int namelen = 0;
		ifs.read((char*)&namelen, sizeof(int));
		ifs.read((char*)TempBuffer, sizeof(char) * (namelen));
		TempBuffer[namelen] = 0;
		name = TempBuffer;
		map->name[i] = name;
	}

	constexpr char MeshDir[] = "Mesh/";
	constexpr char TextureDir[] = "Texture/";
	constexpr char ModelDir[] = "Model/";
	string MeshDirPath = dirName;
	MeshDirPath += "/Mesh/";

	string TextureDirPath = dirName;
	TextureDirPath += "/Texture/";

	string ModelDirPath = dirName;
	ModelDirPath += "/Model/";

	for (int i = 0; i < MeshCount; ++i) {
		int nameid = 0;
		ifs.read((char*)&nameid, sizeof(int));
		string& name = map->name[nameid];

		string filename = MeshDirPath;
		// .map (확장자)제거
		filename += name;
		filename += ".mesh";

		BumpMesh* mesh = new BumpMesh();
		ifstream ifs2{ filename, ios_base::binary };
		int vertCnt = 0;
		int indCnt = 0;
		ifs2.read((char*)&vertCnt, sizeof(int));
		ifs2.read((char*)&indCnt, sizeof(int));

		vector<BumpMesh::Vertex> verts;
		verts.reserve(vertCnt);
		verts.resize(vertCnt);
		vector<TriangleIndex> indexs;
		int tricnt = (indCnt / 3);
		indexs.reserve(tricnt);
		indexs.resize(tricnt);
		for (int k = 0; k < vertCnt; ++k) {
			ifs2.read((char*)&verts[k].position, sizeof(float) * 3);
			ifs2.read((char*)&verts[k].uv, sizeof(float) * 2);
			ifs2.read((char*)&verts[k].normal, sizeof(float) * 3);
			ifs2.read((char*)&verts[k].tangent, sizeof(float) * 3);
			float w = 0; // bitangent direction??
			ifs2.read((char*)&w, sizeof(float));
		}

		for (int k = 0; k < tricnt; ++k) {
			ifs2.read((char*)&indexs[k].v[0], sizeof(UINT));
			ifs2.read((char*)&indexs[k].v[1], sizeof(UINT));
			ifs2.read((char*)&indexs[k].v[2], sizeof(UINT));
		}

		ifs2.close();
		mesh->CreateMesh_FromVertexAndIndexData(verts, indexs);
		//mesh->ReadMeshFromFile_OBJ(filename.c_str(), vec4(1, 1, 1, 1), false);

		ifs.read((char*)&mesh->OBB_Tr, sizeof(float) * 3);
		ifs.read((char*)&mesh->OBB_Ext, sizeof(float) * 3);
		map->meshes[i] = mesh;
	}
	/*gd.pCommandList->Close();
	ID3D12CommandList* ppCommandLists[] = { gd.pCommandList };
	gd.pCommandQueue->ExecuteCommandLists(1, ppCommandLists);
	gd.WaitGPUComplete();*/

	TextureTableStart = game.TextureTable.size();
	MaterialTableStart = game.MaterialTable.size();
	for (int i = 0; i < TextureCount; ++i) {
		int namelen = 0;
		ifs.read((char*)&namelen, sizeof(int));
		char TempBuff[512] = {};
		ifs.read((char*)&TempBuff, namelen);
		TempBuff[namelen] = 0;
		string textureName = TempBuff;

		int width = 0;
		int height = 0;
		int format = 0; // Unity GraphicsFormat

		string filename_dds = TextureDirPath;
		filename_dds += textureName;
		filename_dds += ".dds";
		ifstream ifs2{ filename_dds, ios_base::binary };
		GPUResource* texture = new GPUResource();

		if (ifs2.good()) {
			wchar_t ddsFile[512] = {};
			int len = filename_dds.size();
			for (int i = 0; i < len; ++i) ddsFile[i] = filename_dds[i];
			ddsFile[len] = 0;
			texture->CreateTexture_fromFile(ddsFile, DXGI_FORMAT_BC3_UNORM, 3);
		}
		else {
			string filename = TextureDirPath;
			// .map (확장자)제거
			filename += textureName;
			filename += ".tex";

			ifstream ifs2{ filename, ios_base::binary };

			ifs2.read((char*)&width, sizeof(int));
			ifs2.read((char*)&height, sizeof(int));

			BYTE* pixels = new BYTE[width * height * 4];
			ifs2.read((char*)pixels, width * height * 4);
			ifs2.close();

			char* isnormal = &filename[filename.size() - 10];
			char* isnormalmap = &filename[filename.size() - 13];
			char* isnormalDX = &filename[filename.size() - 12];
			if ((strcmp(isnormal, "Normal.tex") == 0 ||
				strcmp(isnormalDX, "NormalDX.tex") == 0) ||
				strcmp(isnormalmap, "normalmap.tex") == 0) {
				float xtotal = 0;
				float ytotal = 0;
				float ztotal = 0;
				for (int ix = 0; ix < width; ++ix) {
					int h = rand() % height;
					BYTE* p = &pixels[h * width * 4 + ix * 4];
					xtotal += p[0];
					ytotal += p[1];
					ztotal += p[2];
				}
				if (ztotal < ytotal) {
					// x = z
					// y = -x
					// z = y
					for (int ix = 0; ix < width; ++ix) {
						for (int iy = 0; iy < height; ++iy) {
							BYTE* p = &pixels[iy * width * 4 + ix * 4];
							BYTE z = p[2];
							BYTE y = p[1];
							BYTE x = p[0];
							p[0] = z;
							p[1] = 255 - x;
							p[2] = y;
						}
					}
				}
				if (ztotal < xtotal) {
					// x = z
					// y = y
					// z = x
					for (int ix = 0; ix < width; ++ix) {
						for (int iy = 0; iy < height; ++iy) {
							BYTE* p = &pixels[iy * width * 4 + ix * 4];
							BYTE k = p[2];
							p[2] = p[0];
							p[0] = k;
						}
					}
				}
			}
			imgform::PixelImageObject pio;
			pio.data = (imgform::RGBA_pixel*)pixels;
			pio.width = width;
			pio.height = height;
			string filename_bmp = TextureDirPath;
			filename_bmp += textureName;
			filename_bmp += ".bmp";
			pio.rawDataToBMP(filename_bmp);

			const char* lastSlash = strrchr(filename_dds.c_str(), L'/');
			lastSlash += 1;

			gd.bmpTodds(game.basicTexMip, game.basicTexFormatStr, filename_bmp.c_str());

			MoveFileA(lastSlash, filename_dds.c_str());

			wchar_t ddsFile[512] = {};
			int len = filename_dds.size();
			for (int i = 0; i < len; ++i) ddsFile[i] = filename_dds[i];
			ddsFile[len] = 0;
			texture->CreateTexture_fromFile(ddsFile, game.basicTexFormat, game.basicTexMip);

			DeleteFileA(filename_bmp.c_str());

			//texture->CreateTexture_fromImageBuffer(width, height, pixels, DXGI_FORMAT_BC2_UNORM);

			delete[] pixels;
		}

		ifs.read((char*)&width, sizeof(int));
		ifs.read((char*)&height, sizeof(int));
		ifs.read((char*)&format, sizeof(int));

		game.TextureTable.push_back(texture);
	}
	/*gd.pCommandAllocator->Reset();
	gd.pCommandList->Reset(gd.pCommandAllocator, NULL);*/

	for (int i = 0; i < MaterialCount; ++i) {
		Material mat;

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

		game.MaterialTable.push_back(mat);
	}

	for (int i = 0; i < ModelCount; ++i) {
		int namelen = 0;
		ifs.read((char*)&namelen, sizeof(int));
		char TempBuff[512] = {};
		ifs.read((char*)&TempBuff, namelen);
		TempBuff[namelen] = 0;

		string modelName = TempBuff;
		string filename = ModelDirPath;
		// .map (확장자)제거
		filename += modelName;
		filename += ".model";

		Model* pModel = new Model();
		pModel->LoadModelFile2(filename);
		map->models[i] = pModel;
	}

	for (int i = 0; i < gameObjectCount; ++i) {
		Hierarchy_Object* go = new Hierarchy_Object();
		map->MapObjects[i] = go;
	}
	for (int i = 0; i < gameObjectCount; ++i) {
		Hierarchy_Object* go = map->MapObjects[i];
		int nameId = 0;
		ifs.read((char*)&nameId, sizeof(int));

		vec4 pos;
		ifs.read((char*)&pos, sizeof(float) * 3);
		vec4 rot = 0;
		ifs.read((char*)&rot, sizeof(float) * 3);
		vec4 scale = 0;
		ifs.read((char*)&scale, sizeof(float) * 3);

		rot *= 3.141592f / 180.0f;
		go->m_worldMatrix *= XMMatrixScaling(scale.x, scale.y, scale.z);
		go->m_worldMatrix *= XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z);
		go->m_worldMatrix.pos.f3 = pos.f3;
		go->m_worldMatrix.pos.w = 1;

		bool isMeshes = false;
		ifs.read((char*)&isMeshes, sizeof(bool));
		if (isMeshes) {
			go->rmod = GameObject::eRenderMeshMod::single_Mesh;
			int meshid = 0;
			int materialNum = 0;
			int materialId = 0;
			ifs.read((char*)&meshid, sizeof(int));
			if (meshid == 75) {
				cout << endl;
			}
			if (meshid < 0) {
				go->m_pMesh = nullptr;
				ifs.read((char*)&materialNum, sizeof(int));
			}
			else {
				go->m_pMesh = map->meshes[meshid];
				ifs.read((char*)&materialNum, sizeof(int));
				for (int k = 0; k < materialNum; ++k) {
					ifs.read((char*)&materialId, sizeof(int));
					go->MaterialIndex = MaterialTableStart + materialId;
				}
			}
		}
		else {
			go->rmod = GameObject::eRenderMeshMod::Model;
			//Model
			int ModelID;
			ifs.read((char*)&ModelID, sizeof(int));
			if (ModelID < 0) {
				go->pModel = nullptr;
			}
			else {
				go->pModel = map->models[ModelID];
			}
		}

		if (isMeshes) {
			ifs.read((char*)&go->childCount, sizeof(int));
			go->childs.reserve(go->childCount);
			go->childs.resize(go->childCount);
			for (int k = 0; k < go->childCount; ++k) {
				int childIndex = 0;
				ifs.read((char*)&childIndex, sizeof(int));
				go->childs[k] = map->MapObjects[childIndex];
			}
		}
		else {
			go->childCount = 0;
		}

		go->m_pShader = game.MyPBRShader1;

		map->MapObjects[i] = go;
	}

	//BakeStaticCollision();
}

void SphereLODObject::Update(float deltaTime)
{
	m_worldMatrix.pos = FixedPos;
}

void SphereLODObject::Render()
{
	if (MeshNear && MeshFar)
	{
		vec4 cam = gd.viewportArr[0].Camera_Pos;
		vec4 pos = m_worldMatrix.pos;

		vec4 d = cam - pos;
		float dist = d.len3;

		m_pMesh = (dist < SwitchDistance) ? MeshNear : MeshFar;
	}

	GameObject::Render();
}

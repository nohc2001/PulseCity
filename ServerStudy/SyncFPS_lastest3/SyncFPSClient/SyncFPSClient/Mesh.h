#pragma once
#include "stdafx.h"
#include "GraphicDefs.h"
#include "Shader.h"

class Mesh {
protected:
	GPUResource VertexBuffer;
	GPUResource VertexUploadBuffer;
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView;

	GPUResource IndexBuffer;
	GPUResource IndexUploadBuffer;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView;

	ui32 IndexNum = 0;
	ui32 VertexNum = 0;
	D3D12_PRIMITIVE_TOPOLOGY topology;
public:
	//static unordered_map<string, Mesh*> meshmap;

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

	XMFLOAT3 OBB_Ext;
	XMFLOAT3 OBB_Tr;
	void SetOBBDataWithAABB(XMFLOAT3 minpos, XMFLOAT3 maxpos);

	Mesh();
	virtual ~Mesh();

	virtual void ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering = true);
	virtual void Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum);
	BoundingOrientedBox GetOBB();
	void CreateGroundMesh(ID3D12GraphicsCommandList* pCommandList);
	virtual void CreateWallMesh(float width, float height, float depth, vec4 color);

	virtual void Release();

	virtual void CreateSphereMesh(ID3D12GraphicsCommandList* pCommandList, float radius, int sliceCount, int stackCount, vec4 color);
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
	void CreateWallMesh(float width, float height, float depth, vec4 color);

	virtual void Release();

	void CreateTextRectMesh();

	//void CreatePointMesh_FromVertexData(vector<Vertex>& vert);
};

class BumpMesh : public Mesh {
public:
	int material_index;

	struct Vertex {
		XMFLOAT3 position;
		XMFLOAT3 normal;
		XMFLOAT2 uv;
		XMFLOAT3 tangent;

		Vertex() {}
		Vertex(XMFLOAT3 p, XMFLOAT3 nor, XMFLOAT2 _uv, XMFLOAT3 tan)
			: position{ p }, normal{ nor }, uv{ _uv }, tangent{ tan }
		{
		}

		bool HaveToSetTangent() {
			bool b = ((tangent.x == 0 && tangent.y == 0) && tangent.z == 0);
			b |= ((isnan(tangent.x) || isnan(tangent.y)) || isnan(tangent.z));
			return b;
		}

		static void CreateTBN(Vertex& P0, Vertex& P1, Vertex& P2) {
			vec4 N1, N2, M1, M2;
			//XMFLOAT3 N0, N1, M0, M1;
			N1 = vec4(P1.uv) - vec4(P0.uv);
			N2 = vec4(P2.uv) - vec4(P0.uv);
			M1 = vec4(P1.position) - vec4(P0.position);
			M2 = vec4(P2.position) - vec4(P0.position);

			float f = 1.0f / (N1.x * N2.y - N2.x * N1.y);
			vec4 tan = vec4(P0.tangent);
			tan = f * (M1 * N2.y - M2 * N1.y);
			vec4 v = XMVector3Normalize(tan);
			P0.tangent = v.f3;

			if (P0.HaveToSetTangent()) {
				// why tangent is nan???
				XMVECTOR NorV = XMVectorSet(P0.normal.x, P0.normal.y, P0.normal.z, 1.0f);
				XMVECTOR TanV = XMVectorSet(0, 0, 0, 0);
				XMVECTOR BinV = XMVectorSet(0, 0, 0, 0);
				BinV = XMVector3Cross(NorV, M1);
				TanV = XMVector3Cross(NorV, BinV);
				P0.tangent = { 1, 0, 0 };
			}
		}
	};

	BumpMesh();
	virtual ~BumpMesh();
	virtual void CreateWallMesh(float width, float height, float depth, vec4 color);
	void CreateMesh_FromVertexAndIndexData(vector<Vertex>& vert, vector<TriangleIndex>& inds);
	virtual void ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering = true);
	virtual void Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum);
	virtual void Release();
};

enum eTextureSemantic {
	NONE = 0,
	DIFFUSE = 1,
	SPECULAR = 2,
	AMBIENT = 3,
	EMISSIVE = 4,
	HEIGHT = 5,
	NORMALS = 6,
	SHININESS = 7,
	OPACITY = 8,
	DISPLACEMENT = 9,
	LIGHTMAP = 10,
	REFLECTION = 11,
	BASE_COLOR = 12,
	NORMAL_CAMERA = 13,
	EMISSION_COLOR = 14,
	METALNESS = 15,
	DIFFUSE_ROUGHNESS = 16,
	AMBIENT_OCCLUSION = 17,
	UNKNOWN = 18,
	SHEEN = 19,
	CLEARCOAT = 20,
	TRANSMISSION = 21,
	MAYA_BASE = 22,
	MAYA_SPECULAR = 23,
	MAYA_SPECULAR_COLOR = 24,
	MAYA_SPECULAR_ROUGHNESS = 25,
	ANISOTROPY = 26,
	GLTF_METALLIC_ROUGHNESS = 27,
};

enum eBaseTextureKind {
	Diffuse = 0,
	BaseColor = 1
};

enum eSpecularTextureKind {
	Specular = 0,
	Specular_Color = 1,
	Specular_Roughness = 2,
	Metalic = 3,
	Metalic_Roughness = 4
};

enum eAmbientTextureKind {
	Ambient = 0,
	AmbientOcculsion = 1
};

enum eEmissiveTextureKind {
	Emissive = 0,
	Emissive_Color = 1,
};

enum eBumpTextureKind {
	Normal = 0,
	Normal_Camera = 1,
	Height = 2,
	Displacement = 3
};

enum eShinenessTextureKind {
	Shineness = 0,
	Roughness = 1,
	DiffuseRoughness = 2,
};

enum eOpacityTextureKind {
	Opacity = 0,
	Transparency = 1,
};

struct MaterialCB_Data {
	vec4 baseColor;
	vec4 ambientColor;
	vec4 emissiveColor;
	float metalicFactor;
	float smoothness;
	float bumpScaling;
	float extra;
	//[64]

	float TilingX;
	float TilingY;
	float TilingOffsetX;
	float TilingOffsetY;
	//[64+16 = 80]
};

struct Material {
	struct CLR {
		bool blending;
		float bumpscaling;
		vec4 ambient;
		union {
			vec4 diffuse;
			vec4 base;
		};
		vec4 emissive;
		vec4 reflective;
		vec4 specular;
		float transparent;

		CLR() {}
		CLR(const CLR& ref) {
			memcpy_s(this, sizeof(CLR), &ref, sizeof(CLR));
		}
	};
	struct TextureIndex {
		union { // 1
			int Diffuse;
			int BaseColor;
		};
		union { // 3
			int Specular; // 
			int Specular_Color; // 
			int Specular_Roughness;
			int Metalic; // 0 : non metal / 1 : metal (binary texture)
			int Metalic_Roughness; // (r:metalic, g: roughness, g:none, a:ambientocculsion)
		};
		union { // 1
			int Ambient;
			int AmbientOcculsion;
		};
		union { // 1
			int Emissive;
			int EmissionColor;
		};
		union { // 2
			int Normal; // in TBN Coord
			int Normal_inCameraCoord; // in Camera Coord
			int Height;
			int Displacement;
		};
		union { // 2
			int Shineness;
			int Roughness;
			int DiffuseRoughness;
		};
		union { // 1 
			int Opacity;
			int Transparency;
		};
		int LightMap;
		int Reflection;
		int Sheen;
		int ClearCoat;
		int Transmission;
		int Anisotropy;

		TextureIndex() {
			for (int i = 0; i < (sizeof(TextureIndex) >> 2); ++i) {
				((int*)this)[i] = -1;
			}
		}
	};
	enum GLTF_alphaMod {
		Opaque = 0,
		Blend = 1,
	};
	struct PipelineChanger {
		bool twosided = false;
		bool wireframe = false;
		unsigned short TextureSementicSelector;

		enum eTextureKind {
			Base,
			Specular_Metalic,
			Ambient,
			Emissive,
			Bump,
			Shineness,
			Opacity
		};

		void SelectTextureFlag(eTextureKind kind, int selection) {
			switch (kind) {
			case eTextureKind::Base:
				TextureSementicSelector &= 0xFFFFFFFE;
				TextureSementicSelector |= selection & 0x00000001;
				break;
			case eTextureKind::Specular_Metalic:
				TextureSementicSelector &= 0xFFFFFFF1;
				TextureSementicSelector |= (selection << 1) & 0x0000000E;
				break;
			case eTextureKind::Ambient:
				TextureSementicSelector &= 0xFFFFFFEF;
				TextureSementicSelector |= (selection << 4) & 0x00000010;
				break;
			case eTextureKind::Emissive:
				TextureSementicSelector &= 0xFFFFFFDF;
				TextureSementicSelector |= (selection << 5) & 0x00000020;
				break;
			case eTextureKind::Bump:
				TextureSementicSelector &= 0xFFFFFF3F;
				TextureSementicSelector |= (selection << 6) & 0x000000C0;
				break;
			case eTextureKind::Shineness:
				TextureSementicSelector &= 0xFFFFFCFF;
				TextureSementicSelector |= (selection << 8) & 0x00000300;
				break;
			case eTextureKind::Opacity:
				TextureSementicSelector &= 0xFFFFFBFF;
				TextureSementicSelector |= (selection << 9) & 0x00000400;
				break;
			}
		}
	};
	char name[40] = {};
	D3D12_GPU_DESCRIPTOR_HANDLE hGPU; // texture desc table handle
	CLR clr;
	TextureIndex ti;
	PipelineChanger pipelineC; // this change pipeline of materials.

	//[1bit:base][3bit:Specular][1bit:Ambient][1bit:Emissive][2bit:Normal][2bit:Shineness][1bit:Opacity]
	//https://docs.omniverse.nvidia.com/materials-and-rendering/latest/templates/OmniPBR.html#

	float metallicFactor;
	float roughnessFactor;
	float shininess;
	float opacity;
	float specularFactor;
	float clearcoat_factor;
	float clearcoat_roughnessFactor;
	GLTF_alphaMod gltf_alphaMode;
	float gltf_alpha_cutoff;

	static constexpr int MaterialSiz_withoutGPUResource = 256;
	GPUResource CB_Resource;
	MaterialCB_Data* CBData = nullptr;

	float TilingX;
	float TilingY;
	float TilingOffsetX;
	float TilingOffsetY;

	Material()
	{
		ZeroMemory(this, sizeof(Material));
		TilingX = 1;
		TilingY = 1;
		TilingOffsetX = 0;
		TilingOffsetY = 0;
		clr.bumpscaling = 1;
		clr.base = vec4(1, 1, 1, 1);
		memset(&ti, -1, sizeof(TextureIndex));
	}

	~Material()
	{
	}

	Material(const Material& ref)
	{
		memcpy_s(this, MaterialSiz_withoutGPUResource, &ref, MaterialSiz_withoutGPUResource);
		CB_Resource = ref.CB_Resource;
		CBData = ref.CBData;
		TilingX = ref.TilingX;
		TilingY = ref.TilingY;
		TilingOffsetX = ref.TilingOffsetX;
		TilingOffsetY = ref.TilingOffsetY;
	}

	__forceinline void ShiftTextureIndexs(unsigned int TextureIndexStart);
	void SetDescTable();
	void SetDescTable_NOTCBV();
	MaterialCB_Data GetMatCB();
};

struct ModelNode {
	string name;
	XMMATRIX transform;
	ModelNode* parent;
	unsigned int numChildren;
	ModelNode** Childrens;
	unsigned int numMesh;
	unsigned int* Meshes; // array of int
	unsigned int* Mesh_Materials;

	//metadata
	//???

	ModelNode() {
		parent = nullptr;
		numChildren = 0;
		Childrens = nullptr;
		numMesh = 0;
		Meshes = nullptr;
		Mesh_Materials = nullptr;
	}

	void Render(void* model, ID3D12GraphicsCommandList* cmdlist, const matrix& parentMat);
	void BakeAABB(void* origin, const matrix& parentMat);
};

struct Model {
	std::string mName;
	int nodeCount = 0;
	ModelNode* RootNode;
	vector<ModelNode*> NodeArr;
	unordered_map<void*, int> nodeindexmap;
	ModelNode* Nodes;

	unsigned int mNumMeshes;
	Mesh** mMeshes;
	vector<BumpMesh::Vertex>* vertice = nullptr;
	vector<TriangleIndex>* indice = nullptr;

	unsigned int mNumTextures;
	//GPUResource** mTextures;

	unsigned int mNumMaterials;
	//Material** mMaterials;

	unsigned int TextureTableStart = 0;
	unsigned int MaterialTableStart = 0;

	/*unsigned int mNumAnimations;
	Animation** mAnimations;

	unsigned int mNumSkeletons;
	_Skeleton** mSkeletons;*/

	//metadata
	float UnitScaleFactor = 1.0f;

	//void Rearrange1(ModelNode* node);
	//void Rearrange2();
	//void GetModelFromAssimp(string filename, float posmul = 0);
	//Mesh* Assimp_ReadMesh(aiMesh* mesh, const aiScene* scene, int meshindex);
	//Material* Assimp_ReadMaterial(aiMaterial* mat, const aiScene* scene);
	//Animation* Assimp_ReadAnimation(aiAnimation* anim, const aiScene* scene);

	vec4 AABB[2];
	vec4 OBB_Tr;
	vec4 OBB_Ext;

	std::vector<matrix> BindPose;

	//void SaveModelFile(string filename);
	void LoadModelFile(string filename);
	void LoadModelFile2(string filename);

	void DebugPrintHierarchy(ModelNode* node, int depth = 0);

	void BakeAABB();
	BoundingOrientedBox GetOBB();

	void Render(ID3D12GraphicsCommandList* cmdlist, matrix worldMatrix, Shader::RegisterEnum sre = Shader::RegisterEnum::RenderNormal);

	int FindNodeIndexByName(const std::string& name);

};

struct Shape {
	ui64 FlagPtr;

	__forceinline bool isMesh() {
		return FlagPtr & 0x8000000000000000;

		// highest bit == 1 -> Mesh
		// else -> Model
	}

	__forceinline Mesh* GetMesh() {
		if (isMesh()) {
			return reinterpret_cast<Mesh*>(FlagPtr & 0x7FFFFFFFFFFFFFFF);
		}
		else return nullptr;
	}

	__forceinline void SetMesh(Mesh* ptr) {
		FlagPtr = reinterpret_cast<ui64>(ptr);
		FlagPtr |= 0x8000000000000000;
	}

	__forceinline Model* GetModel() {
		if (isMesh()) return nullptr;
		else {
			return reinterpret_cast<Model*>(FlagPtr);
		}
	}

	__forceinline void SetModel(Model* ptr) {
		FlagPtr = reinterpret_cast<ui64>(ptr);
	}

	static int GetShapeIndex(string meshName);
	static int AddShapeName(string meshName);

	static vector<string> ShapeNameArr;
	static unordered_map<string, Shape> StrToShapeMap;

	static int AddMesh(string name, Mesh* ptr);
	static int AddModel(string name, Model* ptr);
};
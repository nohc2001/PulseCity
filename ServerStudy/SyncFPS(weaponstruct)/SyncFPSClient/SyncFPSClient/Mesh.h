#pragma once
#include "stdafx.h"
#include "GraphicDefs.h"
#include "Shader.h"

/*
* 설명 : 메쉬 데이터
*/
class Mesh {
protected:
	// 버택스 버퍼
	GPUResource VertexBuffer;
	// 버택스 업로드 버퍼
	GPUResource VertexUploadBuffer;
	// 버택스 버퍼 뷰
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView;

	// 인덱스 버퍼
	GPUResource IndexBuffer;
	// 인덱스 업로드 버퍼
	GPUResource IndexUploadBuffer;
	// 인덱스 버퍼 뷰
	D3D12_INDEX_BUFFER_VIEW IndexBufferView;

	// 인덱스 개수
	ui32 IndexNum = 0;
	// 버택스 개수
	ui32 VertexNum = 0;
	// Mesh의 토폴로지
	D3D12_PRIMITIVE_TOPOLOGY topology;
public:

	/*
	* 설명 : 일반 Mesh의 버택스하나의 데이터
	*/
	struct Vertex {
		//위치
		XMFLOAT3 position;
		//색상
		XMFLOAT4 color;
		//법선
		XMFLOAT3 normal;

		Vertex() {}
		Vertex(XMFLOAT3 p, XMFLOAT4 col, XMFLOAT3 nor)
			: position{ p }, color{ col }, normal{ nor }
		{
		}
	};

	// Mesh 의 OBB의 Extends
	XMFLOAT3 OBB_Ext;
	// Mesh 의 OBB의 Center
	XMFLOAT3 OBB_Tr;
	/*
	* 설명 : AABB를 사용해 Mesh의 OBB 데이터를 구성.
	* 매개변수 : 
	* XMFLOAT3 minpos : AABB의 최소 위치
	* XMFLOAT3 maxpos : AABB의 최대 위치
	*/
	void SetOBBDataWithAABB(XMFLOAT3 minpos, XMFLOAT3 maxpos);

	Mesh();
	virtual ~Mesh();

	/*
	* 설명 : path 경로에 있는 obj 파일의 Mesh를 불러온다. color 색 대로, centering 이 true일시에 OBB Center가 0이 된다.
	* const char* path : obj 파일 경로
	* vec4 color : Mesh에 입힐 색상
	* bool centering : Mesh의 OBB Center가 원점인지 여부 (Mesh의 정 중앙이 원점이 됨.)
	*/
	virtual void ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering = true);

	/*
	* 설명 : Mesh를 렌더링 한다.
	* 여기에서 파이프라인이나 셰이더를 설정하지 않는다.
	* 매개변수 : 
	* ID3D12GraphicsCommandList* pCommandList : 현재 렌더링에 사용되는 커맨드 리스트
	* ui32 instanceNum : 렌더링 하고자 하는 Mesh의 개수. (인스턴싱의 경우 1이상의 값이 필요함.)
	*/
	virtual void Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum);

	/*
	* 설명/반환 : Mesh의 OBB를 얻는다.
	*/
	BoundingOrientedBox GetOBB();
	
	/*
	* 설명 : 각 모서리 길이가 width, height, depth이고, color 색을 가진 직육면체 Mesh를 만든다.
	* float width : 너비
	* float height : 높이
	* float depth : 폭
	* vec4 color : 색상
	*/
	virtual void CreateWallMesh(float width, float height, float depth, vec4 color);

	/*
	* 설명 : Mesh를 해제함.
	*/
	virtual void Release();


	virtual void CreateSphereMesh(ID3D12GraphicsCommandList* pCommandList, float radius, int sliceCount, int stackCount, vec4 color);
};

/*
* 설명 : Texture Mapping을 위한 UV가 있는 Mesh.
*/
class UVMesh : public Mesh {
public:
	/*
	* 설명 : UVMesh의 버택스하나의 데이터
	*/
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

	/*
	* 설명 : path 경로에 있는 obj 파일의 Mesh를 불러온다. color 색 대로, centering 이 true일시에 OBB Center가 0이 된다.
	* const char* path : obj 파일 경로
	* vec4 color : Mesh에 입힐 색상
	* bool centering : Mesh의 OBB Center가 원점인지 여부 (Mesh의 정 중앙이 원점이 됨.)
	*/
	virtual void ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering = true);

	/*
	* 설명 : Mesh를 렌더링 한다.
	* 여기에서 파이프라인이나 셰이더를 설정하지 않는다.
	* 매개변수 :
	* ID3D12GraphicsCommandList* pCommandList : 현재 렌더링에 사용되는 커맨드 리스트
	* ui32 instanceNum : 렌더링 하고자 하는 Mesh의 개수. (인스턴싱의 경우 1이상의 값이 필요함.)
	*/
	virtual void Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum);

	/*
	* 설명 : 각 모서리 길이가 width, height, depth이고, color 색을 가진 직육면체 Mesh를 만든다.
	* float width : 너비
	* float height : 높이
	* float depth : 폭
	* vec4 color : 색상
	*/
	void CreateWallMesh(float width, float height, float depth, vec4 color);

	/*
	* 설명 : Mesh를 해제함.
	*/
	virtual void Release();

	/*
	* 설명 : Text 렌더링을 위한 Plane을 만드는 함수.
	*/
	void CreateTextRectMesh();
};

/*
* UV가 있어 텍스쳐 매핑을 할 수 있으면서, NormalMapping이 가능하도록 tangent 정보가 있는 Mesh.
*/
class BumpMesh : public Mesh {
public:
	// 해당 메쉬에 적용될 수 있는 머터리얼의 인덱스
	// fix <이건 게임 오브젝트에 있어야 한다. 수정이 필요함.>
	int material_index;

	/*
	* 설명 : BumpMesh의 버택스하나의 데이터
	*/
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

		/*
		* 설명 : 해당 버택스의 tangent를 초기화할 필요가 있는지검사한다.
		*/
		__forceinline bool HaveToSetTangent() {
			bool b = ((tangent.x == 0 && tangent.y == 0) && tangent.z == 0);
			b |= ((isnan(tangent.x) || isnan(tangent.y)) || isnan(tangent.z));
			return b;
		}

		/*
		* 설명 : 삼각형의 세 점 P0, P1, P2가 있을때
		* P0의 tangent를 계산하고 설정한다.
		* 매개변수 : Vertex& P0, Vertex& P1, Vertex& P2 : 삼각형의 세 버택스
		*/
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
	
	/*
	* 설명 : 각 모서리 길이가 width, height, depth이고, color 색을 가진 직육면체 Mesh를 만든다.
	* float width : 너비
	* float height : 높이
	* float depth : 폭
	* vec4 color : 색상
	*/
	virtual void CreateWallMesh(float width, float height, float depth, vec4 color);

	/*
	*/
	void CreateMesh_FromVertexAndIndexData(vector<Vertex>& vert, vector<TriangleIndex>& inds);
	
	/*
	* 설명 : path 경로에 있는 obj 파일의 Mesh를 불러온다. color 색 대로, centering 이 true일시에 OBB Center가 0이 된다.
	* const char* path : obj 파일 경로
	* vec4 color : Mesh에 입힐 색상
	* bool centering : Mesh의 OBB Center가 원점인지 여부 (Mesh의 정 중앙이 원점이 됨.)
	*/
	virtual void ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering = true);

	/*
	* 설명 : Mesh를 렌더링 한다.
	* 여기에서 파이프라인이나 셰이더를 설정하지 않는다.
	* 매개변수 :
	* ID3D12GraphicsCommandList* pCommandList : 현재 렌더링에 사용되는 커맨드 리스트
	* ui32 instanceNum : 렌더링 하고자 하는 Mesh의 개수. (인스턴싱의 경우 1이상의 값이 필요함.)
	*/
	virtual void Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum);

	/*
	* 설명 : Mesh를 해제함.
	*/
	virtual void Release();
};

/*
* 머터리얼에 적용되는 텍스쳐가 어떤 유형인지 나타내는 enum
*/
enum eTextureSemantic {
	NONE = 0,
	DIFFUSE = 1, // 색상
	SPECULAR = 2, // 정반사광
	AMBIENT = 3, // 앰비언트광
	EMISSIVE = 4, // 발광
	HEIGHT = 5, // 높이맵
	NORMALS = 6, // 노멀맵
	SHININESS = 7, // 샤인니스
	OPACITY = 8, // 불투명도
	DISPLACEMENT = 9, // 디스플레이스먼트
	LIGHTMAP = 10, // 라이트맵
	REFLECTION = 11, // 반사 맵 / 환경맵
	BASE_COLOR = 12, // 색상
	NORMAL_CAMERA = 13, // 카메라노멀
	EMISSION_COLOR = 14, // 색발광
	METALNESS = 15, // 메탈릭
	DIFFUSE_ROUGHNESS = 16, // (rgb)색상+(a)러프니스
	AMBIENT_OCCLUSION = 17, // AO
	UNKNOWN = 18, // 알 수 없음
	SHEEN = 19,
	CLEARCOAT = 20,
	TRANSMISSION = 21,
	MAYA_BASE = 22,
	MAYA_SPECULAR = 23,
	MAYA_SPECULAR_COLOR = 24,
	MAYA_SPECULAR_ROUGHNESS = 25,
	ANISOTROPY = 26, // ANISOTROPY - 넓은 경사면 밉맵 표현
	GLTF_METALLIC_ROUGHNESS = 27,
};

/*
* 설명 : 기본 색상으로 사용되는 텍스쳐의 종류
*/
enum eBaseTextureKind {
	Diffuse = 0,
	BaseColor = 1
};

/*
* 정반사 광으로 사용되는 텍스쳐의 종류
*/
enum eSpecularTextureKind {
	Specular = 0,
	Specular_Color = 1,
	Specular_Roughness = 2,
	Metalic = 3,
	Metalic_Roughness = 4
};

/*
* 앰비언트 기본광으로 사용되는 텍스쳐의 종류
*/
enum eAmbientTextureKind {
	Ambient = 0,
	AmbientOcculsion = 1
};

/*
* 발광 텍스쳐로 사용되는 텍스쳐의 종류
*/
enum eEmissiveTextureKind {
	Emissive = 0,
	Emissive_Color = 1,
};

/*
* 범프 매핑에 사용되는 텍스쳐
*/
enum eBumpTextureKind {
	Normal = 0,
	Normal_Camera = 1,
	Height = 2,
	Displacement = 3
};

/*
* 반사에 사용되는 텍스쳐
*/
enum eShinenessTextureKind {
	Shineness = 0,
	Roughness = 1,
	DiffuseRoughness = 2,
};

/*
* 투명/불투명에 사용되는 텍스쳐
*/
enum eOpacityTextureKind {
	Opacity = 0,
	Transparency = 1,
};

/*
* 설명 : 머터리얼을 표현하는 구조체.
*/
struct MaterialCB_Data {
	//색상
	vec4 baseColor;
	//앰비언트 기본광
	vec4 ambientColor;
	//발광
	vec4 emissiveColor;
	//메탈릭. 금속인 정도
	float metalicFactor;
	//반사정도
	float smoothness;
	//범프 스케일링
	float bumpScaling;
	//패딩
	float extra;
	//[64]

	// 타일링x
	float TilingX;
	// 타일링y
	float TilingY;
	// 타일링 오프셋 x
	float TilingOffsetX;
	// 타일링 오프셋 y
	float TilingOffsetY;
	//[64+16 = 80]
};

/*
* 설명 : 머터리얼 테이터
*/
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

/*
* 설명 : 모델의 노드.
*/
struct ModelNode {
	// 모델 노드의 이름
	string name;
	// 모델 노드의 원본 transform
	XMMATRIX transform;
	// 부모 노드
	ModelNode* parent;
	// 자식노드 개수
	unsigned int numChildren;
	// 자식노드들
	ModelNode** Childrens;
	// 메쉬의 개수
	unsigned int numMesh;
	// 메쉬의 인덱스 배열
	unsigned int* Meshes; // array of int
	// 메쉬가 가진 머터리얼 번호 배열
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

	/*
	* 설명 : 모델을 렌더할때 쓰이는 제귀호출되는 ModelNode Render 함수.
	* 매개변수 : 
	* void* model : 원본모델 포인터
	* ID3D12GraphicsCommandList* cmdlist : 커맨드 리스트
	* const matrix& parentMat : 부모로부터 계승받은 월드 행렬
	*/
	void Render(void* model, ID3D12GraphicsCommandList* cmdlist, const matrix& parentMat);

	/*
	* 설명 : 모델 노드가 기본상태일때,
	* 해당 모델 노드의 자신과 모든 자식을 포함시키는 AABB를 구성하여
	* origin 모델의 AABB를 확장시킨다.
	* 매개변수 :
	* void* origin : Model의 인스턴스로, 해당 ModelNode를 소유한 원본 Model의 void*
	* const matrix& parentMat : 부모의 기본 trasform으로 부터 변환된 행렬
	*/
	void BakeAABB(void* origin, const matrix& parentMat);
};

/*
* 설명 : 모델
* Sentinal Value :
* NULL = (nodeCount == 0 && RootNode == nullptr && Nodes == nullptr && mNumMeshes == 0 && mMeshes == nullptr)
*/
struct Model {
	// 모델의 이름
	std::string mName;
	// 모델이 포함한 모든 노드들의 개수
	int nodeCount = 0;
	// 모델의 최상위 노드
	ModelNode* RootNode;

	// Nodes를 로드할 때만 쓰이는 변수들 (클라이언트에서 밖으로 빼낼 필요가 있음.)
	vector<ModelNode*> NodeArr;
	unordered_map<void*, int> nodeindexmap;

	// 모델 노드들의 배열
	ModelNode* Nodes;

	// 모델이 가진 메쉬의 개수
	unsigned int mNumMeshes;

	// 모델이 가진 메쉬들의 포인터 배열
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

	// 모델의 UnitScaleFactor
	float UnitScaleFactor = 1.0f;

	//void Rearrange1(ModelNode* node);
	//void Rearrange2();
	//void GetModelFromAssimp(string filename, float posmul = 0);
	//Mesh* Assimp_ReadMesh(aiMesh* mesh, const aiScene* scene, int meshindex);
	//Material* Assimp_ReadMaterial(aiMaterial* mat, const aiScene* scene);
	//Animation* Assimp_ReadAnimation(aiAnimation* anim, const aiScene* scene);

	// 모델의 기본상태에서 모델을 모두 포함하는 가장 작은 AABB.
	vec4 AABB[2];
	// 모델의 OBB.Center
	vec4 OBB_Tr;
	// 모델의 OBB.Extends
	vec4 OBB_Ext;

	std::vector<matrix> BindPose;

	//void SaveModelFile(string filename);

	/*
	* 설명 : MyModelExporter에서 뽑아온 모델 바이너리 정보를 로드함.
	* 서버에서는 충돌정보만을 가져온다.
	* 매개변수:
	* string filename : 모델의 경로
	*/
	void LoadModelFile(string filename);

	/*
	* 설명 : Unity에서 뽑아온 맵에 존재하는 모델 바이너리 정보를 로드함.
	* 서버에서는 충돌정보만을 가져온다.
	* 매개변수:
	* string filename : 모델의 경로
	*/
	void LoadModelFile2(string filename);

	/*
	* 설명 : 계층구조를 출력한다.
	*/
	void DebugPrintHierarchy(ModelNode* node, int depth = 0);

	/*
	* 설명 : Model의 AABB를 구성한다.
	*/
	void BakeAABB();

	/*
	* 설명/반환 : Model의 기본 OBB를 반환한다.
	*/
	BoundingOrientedBox GetOBB();

	/*
	* 설명 : 모델을 렌더링함
	* 매개변수 : 
	* ID3D12GraphicsCommandList* cmdlist : 현재 렌더링을 수행하는 커맨드 리스트
	* matrix worldMatrix : 모델이 렌더링될 월드 행렬
	* Shader::RegisterEnum sre : 어떤 방식으로 렌더링 할 것인지
	*/
	void Render(ID3D12GraphicsCommandList* cmdlist, matrix worldMatrix, Shader::RegisterEnum sre = Shader::RegisterEnum::RenderNormal);

	/*
	* 설명 : 노드의 이름으로 노드 인덱스를 찾는 함수
	* 매개변수 : 
	* const std::string& name : 이름
	* 반환 : 노드의 인덱스 (찾지 못하면 -1.)
	*/
	int FindNodeIndexByName(const std::string& name);

};

/*
* 설명 : Shape은 Mesh와 Model을 포함할 수 있는 모양을 나타낸 구조체.
* highest bit == 1 -> Mesh
* else -> Model
*
* Sentinal Value :
* NULL : (FlagPtr = 0);
* isMesh : (FlagPtr & 0x8000000000000000);
* isModel : !isMesh;
*/
struct Shape {
	ui64 FlagPtr;

	/*
	* 설명/반환 : Shape가 Mesh인지 여부를 반환
	*/
	__forceinline bool isMesh() {
		return FlagPtr & 0x8000000000000000;

		// highest bit == 1 -> Mesh
		// else -> Model
	}

	/*
	* 설명/반환 : Shape가 가진 Mesh 포인터를 반환
	*/
	__forceinline Mesh* GetMesh() {
		if (isMesh()) {
			return reinterpret_cast<Mesh*>(FlagPtr & 0x7FFFFFFFFFFFFFFF);
		}
		else return nullptr;
	}

	/*
	* 설명 : Shape에 Mesh를 넣는다.
	*/
	__forceinline void SetMesh(Mesh* ptr) {
		FlagPtr = reinterpret_cast<ui64>(ptr);
		FlagPtr |= 0x8000000000000000;
	}

	/*
	* 설명/반환 : Shape가 가진 Model 포인터를 반환
	*/
	__forceinline Model* GetModel() {
		if (isMesh()) return nullptr;
		else {
			return reinterpret_cast<Model*>(FlagPtr);
		}
	}

	/*
	* 설명 : Shape에 Model를 넣는다.
	*/
	__forceinline void SetModel(Model* ptr) {
		FlagPtr = reinterpret_cast<ui64>(ptr);
	}

	/*
	* 설명/반환 : Shape의 이름을 받아서 해당 Shape의 인덱스를 반환한다.
	*/
	static int GetShapeIndex(string meshName);
	/*
	* 설명/반환 : Shape의 이름을 받아서 ShapeNameArr에 저장후 해당 Shape의 인덱스를 반환한다.
	*/
	static int AddShapeName(string meshName);

	// Shape의 이름 배열
	static vector<string> ShapeNameArr;
	// 이름에서 Shape를 얻는 map
	static unordered_map<string, Shape> StrToShapeMap;

	/*
	* 설명 : Mesh의 이름과 Mesh 포인터를 받아 Mesh를 추가하는 함수
	* 매개변수 :
	* string name : Mesh의 이름
	* Mesh* ptr : Mesh의 포인터
	*/
	static int AddMesh(string name, Mesh* ptr);

	/*
	* 설명 : Model의 이름과 Model 포인터를 받아 Model를 추가하는 함수
	* 매개변수 :
	* string name : Model의 이름
	* Model* ptr : Model의 포인터
	*/
	static int AddModel(string name, Model* ptr);
};
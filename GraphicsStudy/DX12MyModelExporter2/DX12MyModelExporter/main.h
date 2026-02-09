#pragma once
#include "GlobalDevice.h"
using namespace TTFFontParser;

using namespace std;
using namespace DirectX;
using namespace DirectX::PackedVector;
//using Microsoft::WRL::ComPtr; -> question 001

#include <assimp/types.h>
#include <assimp/version.h>
#include "ASSIMP/ModelLoader.h"
#include "Utill_ImageFormating.h"

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

//#define PIX_DEBUGING

#ifdef PIX_DEBUGING
#include "C:\\Users\\nohc2\\OneDrive\\Desktop\\WorkRoom\\Dev\\CollegeProjects\\Graphics\\PixEvents-main\\include\\pix3.h"
#endif

#pragma region dbglogDefines
#define dbglog1(format, A) \
	{\
		wchar_t str[512] = {}; \
		wchar_t format0[128] = L"%s line %d : "; \
		wcscpy_s(format0+13, 128-13, format);\
		swprintf_s(str, 512, format0, _T(__FUNCTION__), __LINE__, A); \
		OutputDebugString(str);\
	}
#define dbglog2(format, A, B) \
	{\
		wchar_t str[512] = {}; \
		wchar_t format0[128] = L"%s line %d : "; \
		wcscpy_s(format0+13,  128-13, format);\
		swprintf_s(str, 512, format0, _T(__FUNCTION__), __LINE__, A, B); \
		OutputDebugString(str);\
	}
#define dbglog3(format, A, B, C) \
	{\
		wchar_t str[512] = {}; \
		wchar_t format0[128] = L"%s line %d : "; \
		wcscpy_s(format0+13,  128-13, format);\
		swprintf_s(str, 512, format0, _T(__FUNCTION__), __LINE__, A, B, C); \
		OutputDebugString(str);\
	}
#define dbglog4(format, A, B, C, D) \
	{\
		wchar_t str[512] = {}; \
		wchar_t format0[128] = L"%s line %d : "; \
		wcscpy_s(format0+13,  128-13, format);\
		swprintf_s(str, 512, format, _T(__FUNCTION__), __LINE__, A, B, C, D); \
		OutputDebugString(str);\
	}

#define dbglog1a(format, A) \
	{\
		char str[512] = {}; \
		char format0[128] = "%s line %d : "; \
		strcpy_s(format0+13, 128-13, format);\
		sprintf_s(str, 512, format0, __FUNCTION__, __LINE__, A); \
		OutputDebugStringA(str);\
	}
#define dbglog2a(format, A, B) \
	{\
		char str[512] = {}; \
		char format0[128] = "%s line %d : "; \
		strcpy_s(format0+13, 128-13, format);\
		sprintf_s(str, 512, format0, __FUNCTION__, __LINE__, A, B); \
		OutputDebugStringA(str);\
	}
#pragma endregion

ofstream dbg;
void MatrixDbg(matrix mat) {
	dbg << mat._11 << ", " << mat._12 << ", " << mat._13 << ", " << mat._14 << endl;
	dbg << mat._21 << ", " << mat._22 << ", " << mat._23 << ", " << mat._24 << endl;
	dbg << mat._31 << ", " << mat._32 << ", " << mat._33 << ", " << mat._34 << endl;
	dbg << mat._41 << ", " << mat._42 << ", " << mat._43 << ", " << mat._44 << endl;
}

struct WinEvent {
	HWND hWnd;
	UINT Message;
	WPARAM wParam;
	LPARAM lParam;

	WinEvent(HWND hwnd, UINT msg, WPARAM wP, LPARAM lP) {
		hWnd = hwnd;
		Message = msg;
		wParam = wP;
		lParam = lP;
	}
};

#define FRAME_BUFFER_WIDTH 1280
#define FRAME_BUFFER_HEIGHT 720

HINSTANCE g_hInst;
HWND hWnd;
LPCTSTR lpszClass = L"Client 001";
LPCTSTR lpszWindowName = L"Client 001";
int resolutionLevel = 3;

#define QUERYPERFORMANCE_HZ 10000000//Hz
static inline ui64 GetTicks()
{
	LARGE_INTEGER ticks;
	QueryPerformanceCounter(&ticks);
	return ticks.QuadPart;
}

void font_parsed(void* args, void* _font_data, int error)
{
	if (error)
	{
		*(uint8_t*)args = error;
	}
	else
	{
		TTFFontParser::FontData* font_data = (TTFFontParser::FontData*)_font_data;

		{
			for (const auto& glyph_iterator : font_data->glyphs)
			{
				uint32_t num_curves = 0, num_lines = 0;
				for (const auto& path_list : glyph_iterator.second.path_list)
				{
					for (const auto& geometry : path_list.geometry)
					{
						if (geometry.is_curve)
							num_curves++;
						else
							num_lines++;
					}
				}
			}
		}

		*(uint8_t*)args = 1;
	}
}

//ratio = 4 : 3
constexpr ui32 ResolutionArr_GA[10][2] = {
	{640, 480}, {800, 600}, {1024, 768}, {1152, 864},
	{1400, 1050}, {1600, 1200}, {2048, 1536}, {3200, 2400},
	{4096, 3072}, {6400, 4800}
};
enum class ResolutionName_GA {
	VGA = 0,
	SVGA = 1,
	XGA = 2,
	XGAplus = 3,
	SXGAplus = 4,
	UXGA = 5,
	QXGA = 6,
	QUXGA = 7,
	HXGA = 8,
	HUXGA = 9
};

//ratio = 16 : 9
constexpr ui32 ResolutionArr_HD[12][2] = {
	{640, 360}, {854, 480}, {960, 540}, {1280, 720}, {1600, 900},
	{1920, 1080}, {2560, 1440}, {2880, 1620}, {3200, 1800}, {3840, 2160}, {5120, 2880}, {7680, 4320}
};
enum class ResolutionName_HD {
	nHD = 0,
	SD = 1,
	qHD = 2,
	HD = 3,
	HDplus = 4,
	FHD = 5,
	QHD = 6,
	QHDplus0 = 7,
	QHDplus1 = 8,
	UHD = 9,
	UHDplus = 10,
	FUHD = 11
};

//ratio = 16 : 10
constexpr ui32 ResolutionArr_WGA[10][2] = {
	{1280, 800}, {1440, 900}, {1680, 1050}, {1920, 1200},
	{2560, 1600}, {2880, 1800}, {3072, 1920}, {3840, 2400},
	{5120, 3200}, {7680, 4800}
};
enum class ResolutionName_WGA {
	WXGA = 0,
	WXGAplus = 1,
	WSXGA = 2,
	WUXGA = 3,
	WQXGA0 = 4,
	WQXGA1 = 5,
	WQXGA2 = 6,
	WQUXGA = 7,
	WHXGA = 8,
	WHUXGA = 9
};

struct DescHandle {
	D3D12_CPU_DESCRIPTOR_HANDLE hcpu;
	D3D12_GPU_DESCRIPTOR_HANDLE hgpu;

	DescHandle(D3D12_CPU_DESCRIPTOR_HANDLE _hcpu, D3D12_GPU_DESCRIPTOR_HANDLE _hgpu) {
		hcpu = _hcpu;
		hgpu = _hgpu;
	}
	DescHandle(){}

	__forceinline void operator+=(unsigned long long inc) {
		hcpu.ptr += inc;
		hgpu.ptr += inc;
	}
};

struct TestStruct {
	bool UseBundle;

	void SetTestValue();
};

struct TextTextureKey {
	wchar_t charactor;
	unsigned int fontSize;

	TextTextureKey(wchar_t _charactor, unsigned int _fontSize) {
		charactor = _charactor;
		fontSize = _fontSize;
	}
};

template<>
class hash<TextTextureKey> {
public:
	size_t operator()(const TextTextureKey& s) const {
		size_t d0 = s.charactor;
		d0 = _pdep_u64(d0, 0x5555555555555555);
		size_t d1 = s.fontSize;
		d1 = _pdep_u64(d1, 0xAAAAAAAAAAAAAAAA);
		return d0 | d1;
	}
};

GlobalDevice gd;

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
	virtual void Release();

	static D3D12_SHADER_BYTECODE GetShaderByteCode(const WCHAR* pszFileName, LPCSTR pszShaderName, LPCSTR pszShaderProfile, ID3DBlob** ppd3dShaderBlob);
};

class OnlyColorShader : Shader {
public:
	OnlyColorShader();
	virtual ~OnlyColorShader();

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();
	virtual void Release();
};

class DiffuseTextureShader : Shader {
public:
	DiffuseTextureShader();
	virtual ~DiffuseTextureShader();

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreatePipelineState();
	virtual void Release();

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

class PBRShader1 : public Shader{
public:
	ID3D12PipelineState* pPipelineState_SkinedMesh;
	ID3D12RootSignature* pRootSignature_SkinedMesh;

	PBRShader1() {}
	virtual ~PBRShader1() {}

	virtual void InitShader();
	virtual void CreateRootSignature();
	virtual void CreateRootSignature_SkinedMesh();
	virtual void CreatePipelineState();
	virtual void CreatePipelineState_SkinedMesh();
	virtual void Release();

	void Add_RegisterShaderCommand2(ID3D12GraphicsCommandList* commandList, bool skined);

	union eTextureID {
		enum texid {
			Color = 0,
			Normal = 1,
			AO = 2,
			Matalic = 3,
			Roughness = 4,
		};
		unsigned int num;
		eTextureID(texid tid) { num = (unsigned int)tid; }
		eTextureID(unsigned int n) { num = n; }
		operator texid() { return (texid)num; }
		operator unsigned int() { return num; }
	};

	void SetTextureCommand(GPUResource* Color, GPUResource* Normal, GPUResource* AO, GPUResource* Metalic, GPUResource* Roughness);
};

struct TriangleIndex {
	UINT v[3];

	bool operator==(TriangleIndex ti) {
		v[0] = ti.v[0];
		v[1] = ti.v[1];
		v[2] = ti.v[2];
	}

	TriangleIndex() {}
	TriangleIndex(UINT a0, UINT a1, UINT a2) {
		v[0] = a0;
		v[1] = a1;
		v[2] = a2;
	}
};

struct Color {
	XMFLOAT4 v;
	operator XMFLOAT4() const { return v; }

	static Color RandomColor() {
		Color c;
		c.v.x = (float)(rand() & 255) / 255.0f;
		c.v.y = (float)(rand() & 255) / 255.0f;
		c.v.z = (float)(rand() & 255) / 255.0f;
		c.v.w = 0.5f + (float)(rand() & 127) / 127.0f;
		return c;
	}
};

class ColorMesh {
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
	//XMFLOAT3 MAXpos = { 0, 0, 0 }; // it can replace Mesh OBB.
	XMFLOAT3 OBB_Ext;
	XMFLOAT3 OBB_Tr;

	enum MeshType {
		Mesh = 0,
		UVMesh = 1,
		BumpMesh = 2,
		SkinedBumpMesh = 3
	};
	MeshType type;

	struct Vertex {
		XMFLOAT3 position;
		XMFLOAT4 color;
		XMFLOAT3 normal;

		Vertex() {}
		Vertex(XMFLOAT3 p, XMFLOAT4 col, XMFLOAT3 nor) 
			: position{ p }, color{ col }, normal{ nor }
		{}
	};

	ColorMesh();
	virtual ~ColorMesh();

	void SetOBBDataWithAABB(XMFLOAT3 minpos, XMFLOAT3 maxpos);

	void CreateMesh_FromVertexAndIndexData(vector<Vertex>& vert, vector<TriangleIndex>& inds);
	virtual void ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering = true);
	virtual void Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum);
	BoundingOrientedBox GetOBB();
	void CreateGroundMesh(ID3D12GraphicsCommandList* pCommandList);
	void CreateWallMesh(float width, float height, float depth, vec4 color);

	virtual void Release();
};

class UVMesh : ColorMesh {
public:
	struct Vertex {
		XMFLOAT3 position;
		XMFLOAT4 color;
		XMFLOAT3 normal;
		XMFLOAT2 uv;

		Vertex() {}
		Vertex(XMFLOAT3 p, XMFLOAT4 col, XMFLOAT3 nor, XMFLOAT2 texcoord)
			: position{ p }, color{ col }, normal{ nor }, uv{texcoord}
		{
		}
	};

	void CreateMesh_FromVertexAndIndexData(vector<Vertex>& vert, vector<TriangleIndex>& inds);
	virtual void ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering = true);
	virtual void Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum);

	virtual void Release();

	void CreateTextRectMesh();
};

class BumpMesh : public ColorMesh {
public:
	int material_index;

	struct Vertex {
		XMFLOAT3 position;
		XMFLOAT3 normal;
		XMFLOAT2 uv;
		XMFLOAT3 tangent;
		XMFLOAT3 bitangent;

		Vertex() {}
		Vertex(XMFLOAT3 p, XMFLOAT3 nor, XMFLOAT2 _uv, XMFLOAT3 tan, XMFLOAT3 bit)
			: position{ p }, normal{ nor }, uv{ _uv }, tangent{ tan }, bitangent{bit}
		{
		}

		bool HaveToSetTangent() {
			bool b = ((tangent.x == 0 && tangent.y == 0) && tangent.z == 0);
			b |= ((isnan(tangent.x) || isnan(tangent.y)) || isnan(tangent.z));

			b |= ((bitangent.x == 0 && bitangent.y == 0) && bitangent.z == 0);
			b |= ((isnan(bitangent.x) || isnan(bitangent.y)) || isnan(bitangent.z));
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

			XMVECTOR bit = vec4(P0.bitangent);
			bit = f * (M2 * N1.x - M1 * N2.x);
			v = XMVector3Normalize(bit);
			P0.bitangent = v.f3;

			if (P0.HaveToSetTangent()) {
				// why tangent is nan???
				XMVECTOR NorV = XMVectorSet(P0.normal.x, P0.normal.y, P0.normal.z, 1.0f);
				XMVECTOR TanV = XMVectorSet(0, 0, 0, 0);
				XMVECTOR BinV = XMVectorSet(0, 0, 0, 0);
				BinV = XMVector3Cross(NorV, M1);
				TanV = XMVector3Cross(NorV, BinV);
				P0.tangent = { 1, 0, 0 };
				P0.bitangent = { 0, 0, 1 };
			}
		}
	};

	BumpMesh();
	virtual ~BumpMesh();
	void CreateMesh_FromVertexAndIndexData(vector<Vertex>& vert, vector<TriangleIndex>& inds);
	virtual void ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering = true);
	virtual void Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum);
	virtual void Release();
};

class BumpSkinMesh : public ColorMesh {
public:
	int material_index;
	matrix* OffsetMatrixs;
	int MatrixCount;
	GPUResource ToOffsetMatrixsCB;
	int* toNodeIndex;

	struct BoneWeight {
		int boneID;
		float weight;

		BoneWeight(){
			boneID = 0;
			weight = 0;
		}

		BoneWeight(int bID, float w) :
			boneID{ bID }, weight{w}
		{

		}
	};

	struct BoneWeight4 {
		BoneWeight bones[4];
		BoneWeight4() {

		}
		BoneWeight4(BoneWeight b0, BoneWeight b1, BoneWeight b2, BoneWeight b3) {
			bones[0] = b0;
			bones[1] = b1;
			bones[2] = b2;
			bones[3] = b3;
		}

		void PushBones(BoneWeight p) {
			for (int i = 0; i < 4; ++i) {
				if (bones[i].boneID < 0) {
					bones[i] = p;
					return;
				}
			}

			for (int i = 0; i < 4; ++i) {
				if (bones[i].weight < p.weight) {
					bones[i] = p;
					return;
				}
			}
		}
	};

	struct MatNode {
		matrix mat;
		int parent; // NULL = -1
	};

	struct Vertex {
		XMFLOAT3 position;
		XMFLOAT3 normal;
		XMFLOAT2 uv;
		XMFLOAT3 tangent;
		XMFLOAT3 bitangent;
		BoneWeight boneWeight[4];

		Vertex() {}
		Vertex(XMFLOAT3 p, XMFLOAT3 nor, XMFLOAT2 _uv, XMFLOAT3 tan, XMFLOAT3 bit, BoneWeight b0, BoneWeight b1, BoneWeight b2, BoneWeight b3)
			: position{ p }, normal{ nor }, uv{ _uv }, tangent{ tan }, bitangent{ bit }
		{
			boneWeight[0] = b0;
			boneWeight[1] = b1;
			boneWeight[2] = b2;
			boneWeight[3] = b3;
		}

		bool HaveToSetTangent() {
			bool b = ((tangent.x == 0 && tangent.y == 0) && tangent.z == 0);
			b |= ((isnan(tangent.x) || isnan(tangent.y)) || isnan(tangent.z));

			b |= ((bitangent.x == 0 && bitangent.y == 0) && bitangent.z == 0);
			b |= ((isnan(bitangent.x) || isnan(bitangent.y)) || isnan(bitangent.z));
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

			XMVECTOR bit = vec4(P0.bitangent);
			bit = f * (M2 * N1.x - M1 * N2.x);
			v = XMVector3Normalize(bit);
			P0.bitangent = v.f3;

			if (P0.HaveToSetTangent()) {
				// why tangent is nan???
				XMVECTOR NorV = XMVectorSet(P0.normal.x, P0.normal.y, P0.normal.z, 1.0f);
				XMVECTOR TanV = XMVectorSet(0, 0, 0, 0);
				XMVECTOR BinV = XMVectorSet(0, 0, 0, 0);
				BinV = XMVector3Cross(NorV, M1);
				TanV = XMVector3Cross(NorV, BinV);
				P0.tangent = { 1, 0, 0 };
				P0.bitangent = { 0, 0, 1 };
			}
		}
	};

	BumpSkinMesh();
	virtual ~BumpSkinMesh();
	void CreateMesh_FromVertexAndIndexData(vector<Vertex>& vert, vector<TriangleIndex>& inds, matrix* NodeLocalMatrixs, int matrixCount);
	virtual void Render(ID3D12GraphicsCommandList* pCommandList, ui32 instanceNum);
	virtual void Release();
};

const std::wstring TextureSemanticNames[32] = {
	L"NONE",         // 0
	L"DIFFUSE",      // 1 - 0
	L"SPECULAR",     // 2 - 1
	L"AMBIENT",      // 3 - 2
	L"EMISSIVE",     // 4 - 3
	L"HEIGHT",       // 5 - 4
	L"NORMALS",      // 6 - 5
	L"SHININESS",    // 7 - 6
	L"OPACITY",      // 8 - 7
	L"DISPLACEMENT", // 9 - 8
	L"LIGHTMAP",     // 10 - 9
	L"REFLECTION",   // 11 - 10
	L"BASE_COLOR",      // 12 - 0
	L"NORMAL_CAMERA",      // 13 - 11
	L"EMISSION_COLOR",      // 14 - 3
	L"METALNESS",      // 15 - 12
	L"DIFFUSE_ROUGHNESS",      // 16 - 0
	L"AMBIENT_OCCLUSION",      // 17 - 2
	L"UNKNOWN",      // 18 - 13
	L"SHEEN",      // 19 - 14
	L"CLEARCOAT",      // 20 - 15
	L"TRANSMISSION",      // 21
	L"MAYA_BASE",      // 22
	L"MAYA_SPECULAR",      // 23
	L"MAYA_SPECULAR_COLOR",      // 24
	L"MAYA_SPECULAR_ROUGHNESS",      // 25
	L"ANISOTROPY",      // 26
	L"GLTF_METALLIC_ROUGHNESS",      // 27
	L"UNKNOWN",      // 28
	L"UNKNOWN",      // 29
	L"UNKNOWN",      // 30
	L"UNKNOWN"       // 31
};

//Roughness Texture Sementic is not exist!!
//question : is diffuse roughness is same with roughness texture??
//+ Transparent Map is also not exist.
// question : transparent map is not exist normal rendering??
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

struct ModelMetaDataEntry {
	aiMetadataType mType;
	/*
	AI_BOOL = 0,
	AI_INT32 = 1,
	AI_UINT64 = 2,
	AI_FLOAT = 3,
	AI_DOUBLE = 4,
	AI_AISTRING = 5,
	AI_AIVECTOR3D = 6,
	AI_AIMETADATA = 7,
	AI_INT64 = 8,
	AI_UINT32 = 9,
	AI_META_MAX = 10,
	*/
	void* mData;

	operator bool() { return *(bool*)mData; }
	operator unsigned long long() { return *(unsigned long long*)mData; }
	operator int() { return *(int*)mData; }
	operator float() { return *(float*)mData; }
	operator double() { return *(double*)mData; }
	operator const char* () { return ((aiString*)mData)->C_Str(); }
	operator XMFLOAT3() { return *(XMFLOAT3*)mData; }
	operator unsigned int() { return *(bool*)mData; }
	operator long long() { return *(long long*)mData; }

	void LogMetaDataEntry() {
		wchar_t str[512] = {};
		constexpr wchar_t TypeWstr[11][64] = {
			L"bool",
			L"int",
			L"uint64",
			L"float",
			L"double",
			L"string",
			L"vector3",
			L"MetaData",
			L"int64",
			L"uint",
			L"META_MAX",
		};
		switch (mType) {
		case aiMetadataType::AI_BOOL:
			swprintf_s(str, 512, L"[MetaEntry] (%s) %s\n", TypeWstr[(int)mType], (*this) ? L"true" : L"false");
			OutputDebugStringW(str);
			break;
		case aiMetadataType::AI_INT32:
			swprintf_s(str, 512, L"[MetaEntry] (%s) %d\n", TypeWstr[(int)mType], (*this).operator int());
			OutputDebugStringW(str);
			break;
		case aiMetadataType::AI_UINT64:
			swprintf_s(str, 512, L"[MetaEntry] (%s) %d\n", TypeWstr[(int)mType], (*this).operator unsigned long long());
			OutputDebugStringW(str);
			break;
		case aiMetadataType::AI_FLOAT:
			swprintf_s(str, 512, L"[MetaEntry] (%s) %g\n", TypeWstr[(int)mType], (*this).operator float());
			OutputDebugStringW(str);
			break;
		case aiMetadataType::AI_DOUBLE:
			swprintf_s(str, 512, L"[MetaEntry] (%s) %g\n", TypeWstr[(int)mType], (*this).operator double());
			OutputDebugStringW(str);
			break;
		case aiMetadataType::AI_AISTRING:
		{
			wchar_t wstr[512] = {};
			aiString* astr = (aiString*)mData;
			for (int i = 0; i < astr->length; ++i) {
				wstr[i] = astr->data[i];
			}
			wstr[astr->length] = L'\0';

			swprintf_s(str, 512, L"[MetaEntry] (%s) \"%s\"\n", TypeWstr[(int)mType], wstr);
			OutputDebugStringW(str);
		}
		break;
		case aiMetadataType::AI_AIVECTOR3D:
			swprintf_s(str, 512, L"[MetaEntry] (%s) (%g, %g, %g)\n", TypeWstr[(int)mType], (*this).operator DirectX::XMFLOAT3().x,
				(*this).operator DirectX::XMFLOAT3().y, (*this).operator DirectX::XMFLOAT3().z);
			OutputDebugStringW(str);
			break;
		case aiMetadataType::AI_AIMETADATA:
			break;
		case aiMetadataType::AI_INT64:
			swprintf_s(str, 512, L"[MetaEntry] (%s) %d\n", TypeWstr[(int)mType], (*this).operator long long());
			OutputDebugStringW(str);
			break;
		case aiMetadataType::AI_UINT32:
			swprintf_s(str, 512, L"[MetaEntry] (%s) %d\n", TypeWstr[(int)mType], (*this).operator unsigned int());
			OutputDebugStringW(str);
			break;
		case aiMetadataType::AI_META_MAX:
			break;
		}
	}
};

struct ModelMetaData {
	unsigned int mNumProperties;
	std::string* mKeys;
	ModelMetaDataEntry* mValues;

	void copyMetaData(aiMetadata* src) {
		mNumProperties = src->mNumProperties;
		mKeys = new string[mNumProperties];
		mValues = new ModelMetaDataEntry[mNumProperties];
		for (int i = 0; i < mNumProperties; ++i) {
			mKeys[i] = src->mKeys[i].C_Str();
			mValues[i].mType = src->mValues[i].mType;
			mValues[i].mData = src->mValues[i].mData;
		}
	}

	void LogMetaData() {
		wchar_t str[512] = {};
		swprintf_s(str, 512, L"[MetaData] property siz : %d\n", mNumProperties);
		OutputDebugStringW(str);

		for (int i = 0; i < mNumProperties; ++i) {
			std::wstring wstr;
			wstr.reserve(mKeys[i].size() + 1);
			wstr.resize(mKeys[i].size());
			for (int k = 0; k < mKeys[i].size(); ++k) {
				wstr[k] = mKeys[i][k];
			}
			swprintf_s(str, 512, L"[%d] %s : ", i, wstr.c_str());
			OutputDebugStringW(str);
			mValues[i].LogMetaDataEntry();
		}
	}
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
	vec4 BoneOrientation;
	ModelMetaData* mMetaData; // later..

	void Render(void* model, ID3D12GraphicsCommandList* cmdlist, const matrix& parentMat, void* goptr);
	void DBGNodes();
};

/*
[MetaData] Material Count : 9
[0] Material Name : thunderbolt96_body
string mat.name = (datalen = 23) data = thunderbolt96_body : NONE [0]

float clr.diffuse = (datalen = 16) data = 1 : NONE [0]
float clr.base = (datalen = 16) data = 1 : NONE [0]
float $clr.specular = (datalen = 16) data = 1 : NONE [0]
float $clr.emissive = (datalen = 16) data = 0 : NONE [0]

//???
string tex.file = (datalen = 7) data = *0 : DIFFUSE [0]
int tex.uvwsrc = (datalen = 4) data = 0 : DIFFUSE [0]
string tex.mappingname = (datalen = 5) data =  : DIFFUSE [0]
string tex.mappingid = (datalen = 16) data = samplers[0] : DIFFUSE [0]
buffer tex.mapmodeu = (datalen = 4) data = 000001E795944EE0 : DIFFUSE [0]
buffer tex.mapmodev = (datalen = 4) data = 000001E7986C4260 : DIFFUSE [0]
buffer tex.mappingfiltermag = (datalen = 4) data = 000001E7986C42E0 : DIFFUSE [0]
buffer tex.mappingfiltermin = (datalen = 4) data = 000001E7986C4140 : DIFFUSE [0]
float tex.uvtrafo = (datalen = 20) data = 0 : GLTF_METALLIC_ROUGHNESS [0]
float $tex.file.strength = (datalen = 4) data = 1 : LIGHTMAP [0]

float $mat.metallicFactor = (datalen = 4) data = 1 : NONE [0]
float $mat.roughnessFactor = (datalen = 4) data = 0.181887 : NONE [0]
float $mat.shininess = (datalen = 4) data = 669.31 : NONE [0]


buffer $mat.twosided = (datalen = 1) data = 000001E79FFF3ED0 : NONE [0]
float $mat.opacity = (datalen = 4) data = 1 : NONE [0]
string $mat.gltf.alphaMode = (datalen = 11) data = OPAQUE : NONE [0]
float $mat.gltf.alphaCutoff = (datalen = 4) data = 0.5 : NONE [0] ??

float $mat.specularFactor = (datalen = 4) data = 1 : NONE [0]
buffer $mat.shadingm = (datalen = 4) data = 000001E79FFF3D00 : NONE [0]
float $mat.clearcoat.factor = (datalen = 4) data = 0.576632 : NONE [0]
float $mat.clearcoat.roughnessFactor = (datalen = 4) data = 0.04 : NONE [0]
*/

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

		CLR(){}
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
					TextureSementicSelector |= (selection<<1) & 0x0000000E;
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
	string name;
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

	Material()
	{
		ZeroMemory(this, sizeof(Material));
		clr.base = vec4(1, 1, 1, 1);
		memset(&ti, 0xFF, sizeof(TextureIndex));
	}
};

enum AnimInterpolation {
	AnimInterpolation_Step,
	AnimInterpolation_Linear,
	AnimInterpolation_Spherical_Linear,
	AnimInterpolation_Cubic_Spline,
#ifndef SWIG
	_AnimInterpolation_Force32Bit = INT_MAX
#endif
};

enum AnimBehaviour {
	AnimBehaviour_DEFAULT = 0x0,
	AnimBehaviour_CONSTANT = 0x1,
	AnimBehaviour_LINEAR = 0x2,
	AnimBehaviour_REPEAT = 0x3,
#ifndef SWIG
	_AnimBehaviour_Force32Bit = INT_MAX
#endif
};

struct VectorKey {
	double mTime;
	XMFLOAT3 mValue;
	AnimInterpolation mInterpolation;
};

struct QuatKey {
	double mTime;
	vec4 mValue;
	AnimInterpolation mInterpolation;
};

struct NodeAnim {
	int nodeIndex; // bone's index
	unsigned int mNumPositionKeys;
	VectorKey* mPositionKeys;
	unsigned int mNumRotationKeys;
	QuatKey* mRotationKeys;
	unsigned int mNumScalingKeys;
	VectorKey* mScalingKeys;
	AnimBehaviour mPreState;
	AnimBehaviour mPostState;

	matrix GetLocalMatrixAtTime(float time, matrix original);
};

struct MeshAnim {

};

struct MeshMorphAnim {

};

struct Animation {
	string name;
	double mDuration;
	double mTicksPerSecond;
	
	unsigned int mNumChannels;
	NodeAnim* mChannels;
	
	unsigned int mNumMeshChannels;
	MeshAnim* mMeshChannels;
	
	unsigned int mNumMorphMeshChannels;
	MeshMorphAnim* mMorphMeshChannels;

	void GetBoneLocalMatrixAtTime(matrix* out, float time);
};

struct _Texture {

};

struct _Skeleton {

};

struct Model {
	std::string mName;
	int nodeCount = 0;
	ModelNode* RootNode;
	vector<ModelNode*> NodeArr;
	unordered_map<void*, int> nodeindexmap;
	ModelNode* Nodes;

	unsigned int mNumMeshes;
	ColorMesh** mMeshes;
	vector<BumpMesh::Vertex>* vertice = nullptr;
	vector<TriangleIndex>* indice = nullptr;

	unsigned int mNumTextures;
	GPUResource** mTextures;

	unsigned int mNumMaterials;
	Material** mMaterials;

	unsigned int mNumAnimations;
	Animation** mAnimations;

	unsigned int mNumSkeletons;
	_Skeleton** mSkeletons;

	//metadata
	float UnitScaleFactor = 1.0f;
	int MaxBoneCount;
	unsigned int mNumSkinMesh;
	BumpSkinMesh** mBumpSkinMeshs;
	matrix* DefaultNodelocalTr;
	matrix* NodeOffsetMatrixArr;

	void Rearrange1(ModelNode* node);
	void Rearrange2();
	void GetModelFromAssimp(string filename, float posmul = 0);
	ColorMesh* Assimp_ReadMesh(aiMesh* mesh, const aiScene* scene, int meshindex);
	Material* Assimp_ReadMaterial(aiMaterial* mat, const aiScene* scene);
	Animation* Assimp_ReadAnimation(aiAnimation* anim, const aiScene* scene);

	void SaveModelFile(string filename);
	void LoadModelFile(string filename);

	void Render(ID3D12GraphicsCommandList* cmdlist, matrix worldMatrix, void* goptr);

	int GetNodeIndex(char* NodeName);
	int BoneStartIndex = 0; // Node중 뼈대가 시작되는 index.
	int BoneIndexToNodeIndex(int boneIndex);
	int NodeIndexToBoneIndex(int nodeIndex);

	void DBGNodes();

	int GetSkinMeshIndexByPtr(BumpSkinMesh* ptr);
};

struct OBB_vertexVector {
	vec4 vertex[2][2][2] = { {{}} };
};

struct BulletRay {
	static constexpr float remainTime = 1.0f;
	static ColorMesh* mesh;
	vec4 start;
	vec4 direction;
	float t;
	float distance;
	float extra[2];

	BulletRay();

	BulletRay(vec4 s, vec4 dir, float dist);
	
	matrix GetRayMatrix(float distance);

	void Update();

	void Render();
};

struct GameObject {
	matrix m_worldMatrix;
	union {
		ColorMesh* m_pMesh;
		Model* pModel = nullptr;
	};
	
	Shader* m_pShader = nullptr;
	GPUResource* m_pTexture = nullptr;

	enum eRenderMeshMod {
		single_Mesh = 0,
		_Model = 1
	};

	eRenderMeshMod rmod = eRenderMeshMod::single_Mesh;

	// node? bone?
	matrix* NodeLocalMatrixs;
	vector<matrix*> RootBoneMatrixs_PerSkinMesh;
	// [bone 0] [...] [bone N]
	vector<GPUResource> BoneToWorldMatrixCB;
	void InitRootBoneMatrixs();
	void SetRootMatrixs();
	void SetRootMatrixsInherit(matrix parentMat, ModelNode* node);

	vec4 LVelocity;
	vec4 tickLVelocity;
	vec4 tickAVelocity;
	vec4 LastQuerternion;

	GameObject();
	virtual ~GameObject();

	virtual void Update(float delatTime);
	virtual void Render();
	virtual void Event(WinEvent evt);
	virtual void Release();

	void SetMesh(ColorMesh* pMesh);
	void SetModel(Model* model);
	void SetShader(Shader* pShader);
	void SetWorldMatrix(const XMMATRIX& worldMatrix);

	const XMMATRIX& GetWorldMatrix() const;

	virtual BoundingOrientedBox GetOBB();
	OBB_vertexVector GetOBBVertexs();

	void LookAt(vec4 look, vec4 up = { 0, 1, 0, 0 });

	static void CollisionMove(GameObject* obj1, GameObject* obj2);
	virtual void OnCollision(GameObject* other);
};

class Player : public GameObject {
	UCHAR* m_pKeyBuffer;
	vec4 velocity;
	vec4 accel;
public:
	bool isGround = false;
	int collideCount = 0;
	float JumpVelocity = 5;

	ColorMesh* Gun;
	ColorMesh* ShootPointMesh;
	matrix gunMatrix_thirdPersonView;
	matrix gunMatrix_firstPersonView;
	static constexpr float ShootDelay = 0.5f;
	float ShootFlow = 0;
	static constexpr float recoilVelocity = 20.0f;
	float recoilFlow = 0;
	static constexpr float recoilDelay = 0.3f;

	ColorMesh* HPBarMesh;
	float HP = 100;
	static constexpr float MaxHP = 100;
	matrix HPMatrix;

	Player(UCHAR* pKeyBuffer) : m_pKeyBuffer(pKeyBuffer) {}

	virtual void Update(float deltaTime) override;

	virtual void Render();
	void Render_AfterDepthClear();

	virtual void Event(WinEvent evt);

	virtual void OnCollision(GameObject* other) override;
	
	virtual void Release();

	virtual BoundingOrientedBox GetOBB();
};

struct DirectionLight
{
	XMFLOAT3 gLightDirection; // Light Direction
	float pad0;
	XMFLOAT3 gLightColor; // Light_Color
	float pad1;
};

struct PointLight
{
	XMFLOAT3 LightPos;
	float LightIntencity;
	XMFLOAT3 LightColor; // Light_Color
	float LightRange;
};

struct LightCB_DATA {
	DirectionLight dirlight;
	PointLight pointLights[8];
};

class Game {
public:
	Shader* MyShader;
	OnlyColorShader* MyOnlyColorShader;
	DiffuseTextureShader* MyDiffuseTextureShader;
	ScreenCharactorShader* MyScreenCharactorShader;
	PBRShader1* MyPBRShader1;
	UVMesh* TextMesh;

	std::vector<GameObject*> m_gameObjects; // GameObjets
	Player* player;
	GPUResource TestTexture;
	GPUResource DefaultTex;
	GPUResource DefaultAmbientTex;
	GPUResource DefaultNoramlTex;
	Model* WalkingAnim;

	vecset<BulletRay> bulletRays;

	//inputs
	bool isMouseReturn;
	vec4 DeltaMousePos;
	vec4 LastMousePos;

	bool bFirstPersonVision = true;

	float DeltaTime;
	UCHAR pKeyBuffer[256];

	GPUResource LightCBResource;
	LightCB_DATA* LightCBData;
	void SetLight();
	void UpdateLight(float deltaTime);

	Game(){}
	~Game() {}
	void Init();
	void Render();
	void Update();
	void Event(WinEvent evt);



	// why I make Release function seperatly? : it is multi useable form.
	// 1. when I want show object release on code lines or if object is global, just call Release();
	// 2. when I want convinient of Destructor ~Class(), just put Release function on it.
	void Release();

	void FireRaycast(vec4 rayStart, vec4 rayDirection, float rayDistance);

	void RenderText(const wchar_t* wstr, int length, vec4 Rect, float fontsiz);
};
Game game;

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
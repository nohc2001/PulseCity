#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

#include "pch.h"
#include "typedef.h"
#pragma comment (lib, "libfbxsdk.lib")
#include <fbxsdk.h>
#include "FBXCommon.h"

constexpr ui64 TypeMask = 0xFF00000000000000;
constexpr ui64 PtrMask = 0x00FFFFFFFFFFFFFF;
constexpr float rotcvt = (2*3.141592f / (float)65535);

extern ui8 ctable[256][256]; // 64KB // value 255 is null
extern ui32 ctableup;

enum struct ComponentType {
	MeshComponent = 0,
	ChildComponent = 1,
	CubeColliderComponent = 2,
	SphereColliderComponent = 3,
	CilinderColliderComponent = 4,
	MeshColliderComponent = 5,
};

struct RenderState {
	ID3D12PipelineState* ps;
	ID3D12RootSignature* rs;
	int DESCRIPTOR_COUNT_FOR_DRAW = 4; // {constantbuffer, diffuse, specular, normal}
};

enum struct RenderStateType {
	None = 0,
	Pong = 1,
};

//Material
struct Pong_Material {
	static constexpr int type = (int)RenderStateType::Pong;
	static constexpr ui32 DiffuseIndex = 1;
	static constexpr ui32 SpecularIndex = 2;
	static constexpr ui32 NormalIndex = 3;
	TEXTURE_HANDLE* DiffuseTexture;
	TEXTURE_HANDLE* SpecularTexture;
	TEXTURE_HANDLE* NormalTexture;
	float ambientRate;
	float MetalicRate;
};

// Material은 RenderState와 쌍대를 이뤄야 함.
// RenderState[1] - MaterialType[1]
// 때문에 pMaterial 상위 1바이트에 RenderState의 인덱스를 넣는다.

union pMaterial {
	void* data;

	template<typename T> __forceinline T* GetMaterial() const {
		return reinterpret_cast<T*>(reinterpret_cast<ui64>(data) & PtrMask);
	}

	template<typename T> __forceinline void SetMaterial(T* vptr) {
		ui64 type = vptr->type;
		data = vptr;
		data = reinterpret_cast<void*>(reinterpret_cast<ui64>(data) & PtrMask);
		type <<= 56;
		data = reinterpret_cast<void*>(reinterpret_cast<ui64>(data) | type);
	}

	__forceinline RenderStateType gettype() {
		return (RenderStateType)((reinterpret_cast<ui64>(data) & TypeMask) >> 56);
	}
};

enum BASIC_MESH_DESCRIPTOR_INDEX
{
	BASIC_MESH_DESCRIPTOR_INDEX_CBV = 0,
	BASIC_MESH_DESCRIPTOR_INDEX_TEX = 1,
	BASIC_MESH_DESCRIPTOR_INDEX_SPE = 2
};

//Mesh
struct Mesh {
	static RenderState renderstate[16];
	static CD3D12Renderer* Renderers[16];
	static FbxManager* lSdkManager;
	static RenderState InitRenderState01(CD3D12Renderer* Renderer);
	static void Static_Init(CD3D12Renderer* Renderer);

	// view
	D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView = {};
	D3D12_INDEX_BUFFER_VIEW m_IndexBufferView = {}; // 32

	// buffer resource
	ID3D12Resource* m_pVertexBuffer = nullptr;
	ID3D12Resource* m_pIndexBuffer = nullptr; // 16

	// count
	//int vertexCount = 0;
	int indexCount = 0;  // 8
	//Mesh_PipelineAndRootSignatureType pipeline_rootsignature_type;
	ui16 renderState_type; // renderstate[renderState_type] (= pipelinestate, rootsignature)
	ui16 RendererID = 0;
	
	ui64 extradata = 0;

	void Init(int prtype, int rid);
	void LoadMesh_FromOBJ(const char* path, bool centerPivot, bool scaleUniform, float scaleFactor);
	void Render(ID3D12GraphicsCommandList* pCommandList, const CONSTANT_BUFFER_DEFAULT& constantBuff, const pMaterial material);
	void Release();
};

extern fmvecarr<Mesh*> MeshVec;
extern CD3D12Renderer* g_pRenderer;
struct MeshComponent {
	static constexpr ui8 type = (ui8)ComponentType::MeshComponent; // evey component have this global constant.
	static void(*UpdateFunc)(void*, float);
	static Pong_Material Default_Material;

	Mesh* mesh;
	pMaterial material;

	static void Static_Init() {
		Default_Material.DiffuseTexture = (TEXTURE_HANDLE*)g_pRenderer->AllocTextureDesc("Resources/material/Default_Material_Diffuse.png");
		Default_Material.SpecularTexture = (TEXTURE_HANDLE*)g_pRenderer->AllocTextureDesc("Resources/material/Default_Material_Specular.png");
		Default_Material.NormalTexture = (TEXTURE_HANDLE*)g_pRenderer->AllocTextureDesc("Resources/material/Default_Material_Normal.png");
		Default_Material.ambientRate = 0.1f;
		Default_Material.MetalicRate = 0.2f;
	}

	void Init() {
		material.SetMaterial<Pong_Material>(&Default_Material);
	}
};

struct ChildComponent {
	static constexpr ui8 type = (ui8)ComponentType::ChildComponent; // evey component have this global constant.
	static void(*UpdateFunc)(void*, float);
	static void(*EventFunc)(void*, WinEvent);
	static void Static_Init();

	void** ObjArr;
	ui32 Capacity = 0;
	ui32 up = 0;

	void Init(const ui32 childCapacity) {
		ObjArr = (void**)fm->_New(sizeof(void*) * childCapacity, true);
		ZeroMemory(ObjArr, sizeof(void*) * childCapacity);
		Capacity = childCapacity;
		up = 0;
	}

	void push_child(void* childptr) {
		if (up < Capacity) {
			ObjArr[up] = childptr;
			up += 1;
		}
	}
};
void ChildComp_Update(void* comp, float delta);
void ChildComp_Event(void* comp, WinEvent evt);

//64byte
struct CubeColliderComponent {
	static constexpr ui8 type = (ui8)ComponentType::CubeColliderComponent; // evey component have this global constant.
	static void(*UpdateFunc)(void*, float);
	static void Static_Init();

	XMFLOAT3 FPOS;
	XMFLOAT3 LPOS;
	XMFLOAT3 velocity;
	ui32 extraData[5];
	void* parent; // parent Object
};
void CubeColliderComp_Update(void* comp, float delta);

//16byte
struct SphereColliderComponent {
	static constexpr ui8 type = (ui8)ComponentType::SphereColliderComponent; // evey component have this global constant.
	static void(*UpdateFunc)(void*, float);
	static void Static_Init();

	float radius;
	XMFLOAT3 velocity;
	ui32 extraData[2];
	void* parent;
};
void SphereColliderComp_Update(void* comp, float delta);

//32 byte
struct CilinderColliderComponent {
	static constexpr ui8 type = (ui8)ComponentType::CilinderColliderComponent; // evey component have this global constant.
	static void(*UpdateFunc)(void*, float);
	static void Static_Init();

	float radius;
	float fy;
	float ly;
	XMFLOAT3 velocity;
	void* parent;
};

struct MeshColliderComponent {
	static constexpr ui8 type = (ui8)ComponentType::MeshColliderComponent; // evey component have this global constant.
	static void(*UpdateFunc)(void*, float);
	static void Static_Init();

	void Init();
	XMFLOAT3 velocity;
	ui32 extradata[3];
	void* parent;
};

//8byte
union Component {
	void* data; // 8byte  [1byte type][7byte addr]

	template<typename T> __forceinline T* GetComponent() {
		return reinterpret_cast<T*>(reinterpret_cast<ui64>(data) & PtrMask);
	}

	template<typename T> __forceinline void SetComponent(T* vptr) {
		ui64 type = rptr->type;
		data = rptr;
		data = data & PtrMask;
		type <<= 56;
		data = data & type;
	}

	__forceinline ComponentType gettype() {
		return (ComponentType)((reinterpret_cast<ui64>(data) & TypeMask) >> 56);
	}
};

extern CONSTANT_BUFFER_DEFAULT g_constBuff;
// 32byte General Object - not fixed. can add and delete multiple components..
// operate with palleral for loop
struct Object
{
	Component* ComponentArr; // 28 - mesh, childobjs, colider, physic... // also aligin with type.
	// [(a)component align type 1byte] [ptr 7 byte] // a : table of 
	XMFLOAT3 pos; // 12
	ui8 Capacity; // 30
	ui8 Up; // 32
	ui16 extraData; // visible & enable & tag on/off values [2bit enable / visible] [14 bit tag data]
	ui16 rot[3]; // 18 xR = rot[0] * rotcvt;
	hfloat scale; // 20

	template<typename T> __forceinline T* GetComponent() {
		int index = ctable[get_ComponentAlign_type()][T::type];
		if (index == 255) return nullptr;
		return mcvt(Component*)(mcvt(ui64)(ComponentArr) & PtrMask)[index].GetComponent<T>();
	}

	__forceinline ui8 get_ComponentAlign_type() {
		return (ui8)((reinterpret_cast<ui64>(ComponentArr) & TypeMask) >> 56);
	}

	void Init(int compCount) {
		ComponentArr = (Component*)fm->_New(sizeof(Component) * compCount, true);
		ZeroMemory(ComponentArr, sizeof(Component) * compCount);
		Up = 0;
		Capacity = compCount;
	}

	void Update(float delta) {
		for (int i = 0; i < Up; ++i) {
			Component comp_ptr = ComponentArr[i];
			switch ((ComponentType)comp_ptr.gettype()) {
			case ComponentType::MeshComponent:
			{
				MeshComponent* comp = comp_ptr.GetComponent<MeshComponent>();
				comp->UpdateFunc(comp, delta);
				break;
			}
			case ComponentType::ChildComponent:
			{
				ChildComponent* comp = comp_ptr.GetComponent<ChildComponent>();
				comp->UpdateFunc(comp, delta);
				break;
			}
			}
		}
	}

	void Render(ID3D12GraphicsCommandList* cmd, XMMATRIX ParentWorldMatrix = XMMatrixIdentity()) {
		MeshComponent* meshcomp = GetComponent<MeshComponent>();
		ChildComponent* childcomp = GetComponent<ChildComponent>();
		bool mb = meshcomp != nullptr;
		bool cb = childcomp != nullptr;
		if (mb || cb) {
			CONSTANT_BUFFER_DEFAULT cbd;
			constexpr float rotcvt = 6.283185307179f / 65536.0f;
			float scalef = scale.toFloat();
			cbd.GetCamModelCB(pos, { rotcvt * rot[0], rotcvt * rot[1], rotcvt * rot[2] }, { scalef , scalef , scalef });
			//cbd.model = XMMatrixMultiply(ParentWorldMatrix, cbd.model);
			cbd.pointLights = g_constBuff.pointLights;

			if (mb) {
				//cbd.SetCamera(); // required..
				meshcomp->mesh->Render(cmd, cbd, meshcomp->material);
			}

			if (cb) {
				for (int i = 0; i < childcomp->up; ++i) {
					Object* obj = (Object*)childcomp->ObjArr[i];
					obj->Render(cmd, cbd.model);
				}
			}
		}
	}

	void Event(WinEvent evt) {
		for (int i = 0; i < Up; ++i) {
			Component comp_ptr = ComponentArr[i];
			switch ((ComponentType)comp_ptr.gettype()) {
			case ComponentType::ChildComponent:
			{
				ChildComponent* comp = comp_ptr.GetComponent<ChildComponent>();
				comp->EventFunc(comp, evt);
				break;
			}
			}
		}
	}

	template<typename T> void PushComponent(T* compptr) {
		if (Up + 1 <= Capacity) {
			ui64 t = T::type;
			ui64 r = reinterpret_cast<ui64>(compptr) | (t << 56);
			ComponentArr[Up] = *reinterpret_cast<Component*>(&r);
			Up += 1;
		}
	}

	static Object* LoadObjectFromFBX(const char* path, bool centerPivot = false, bool scaleUniform = false, float scaleFactor = 1.0f) {
		bool lResult;
		// Load the scene.
		// The example can take a FBX file as an argument.
		FbxString lFilePath(path);

		FbxScene* lScene = FbxScene::Create(Mesh::lSdkManager, "myScene");

		Object* returnObj = nullptr;
		lResult = LoadScene(Mesh::lSdkManager, lScene, lFilePath.Buffer());
		if (lResult) {
			int i;
			FbxNode* lNode = lScene->GetRootNode();
			if (lNode)
			{
				int childCount = lNode->GetChildCount();
				if (childCount >= 2) {
					Object* pobj = (Object*)fm->_New(sizeof(Object), true);
					pobj->Init(1);
					ChildComponent* childcomp = (ChildComponent*)fm->_New(sizeof(ChildComponent), true);
					childcomp->Init(childCount);
					for (i = 0; i < childCount; i++)
					{
						Object* obj = LoadObjectFromFBX_Node(lNode->GetChild(i));
						childcomp->push_child(obj);
					}
					pobj->PushComponent<ChildComponent>(childcomp);

					returnObj = pobj;
				}
				else {
					Object* obj = LoadObjectFromFBX_Node(lNode->GetChild(0));
					returnObj = obj;
				}
			}
		}

	FBX_RETURN:
		returnObj->pos = { 0, 0, 0 };
		returnObj->rot[0] = 0;
		returnObj->rot[1] = 0;
		returnObj->rot[2] = 0;
		returnObj->scale = 1;

		returnObj->PushObjectTypeToCTable();
		return returnObj;
	}

	static Object* LoadObjectFromFBX_Node(FbxNode* pNode) {
		Object* robj = (Object*)fm->_New(sizeof(Object), true);

		int compnum = 0;
		if (pNode->GetChildCount() > 0) compnum += 1;
		if (pNode->GetNodeAttribute() != nullptr) compnum += 1;
		robj->Init(compnum);

		//Node Local Transform Setting
		FbxObject* pObject = pNode;
		FbxProperty lProperty = pObject->GetFirstProperty();
		FbxDouble3 vec3;
		XMVECTOR LCLscale = XMVectorSet(1, 1, 1, 1);
		robj->pos = { 0, 0, 0 };
		robj->rot[0] = 0;
		robj->rot[1] = 0;
		robj->rot[2] = 0;
		robj->scale = 1;
		XMMATRIX PreRotationMatrix = {};
		XMMATRIX PostRotationMatrix = {};
		XMVECTOR PreRotation = XMVectorSet(0, 0, 0, 0);
		XMVECTOR PostRotation = XMVectorSet(0, 0, 0, 0);

		XMMATRIX GeometricWorldMatrix = {};
		XMVECTOR GTranslate = XMVectorSet(0, 0, 0, 0);
		XMVECTOR GRotate = XMVectorSet(0, 0, 0, 0);
		XMVECTOR GScale = XMVectorSet(1, 1, 1, 0);

		constexpr double roticvt = 65536.0 / 360.0;

		while (lProperty.IsValid())
		{
			FbxString lString;
			lString = lProperty.GetLabel();
			if (lString.Compare("Lcl Translation") == 0) {
				vec3 = lProperty.Get<FbxDouble3>();
				robj->pos = { (float)vec3.mData[0], (float)vec3.mData[2], (float)vec3.mData[1] };
			}
			else if (lString.Compare("Lcl Rotation") == 0) {
				vec3 = lProperty.Get<FbxDouble3>();
				robj->rot[0] = -(ui16)(vec3.mData[0] * roticvt);
				robj->rot[1] = (ui16)(vec3.mData[2] * roticvt);
				robj->rot[2] = -(ui16)(vec3.mData[1] * roticvt);
			}
			else if (lString.Compare("Lcl Scaling") == 0) {
				vec3 = lProperty.Get<FbxDouble3>();
				LCLscale = XMVectorSet(vec3.mData[0], vec3.mData[1], vec3.mData[2], 1.0f);
			}
			else if (lString.Compare("GeometricTranslation") == 0) {
				vec3 = lProperty.Get<FbxDouble3>();
				GTranslate = XMVectorSet(vec3.mData[0], vec3.mData[1], vec3.mData[2], 0);
			}
			else if (lString.Compare("GeometricRotation") == 0) {
				vec3 = lProperty.Get<FbxDouble3>();
				GRotate = XMVectorSet(3.141592 * vec3.mData[0] / 180.0, 3.141592 * vec3.mData[1] / 180.0, 3.141592 * vec3.mData[2] / 180.0, 0);
			}
			else if (lString.Compare("GeometricScaling") == 0) {
				vec3 = lProperty.Get<FbxDouble3>();
				GScale = XMVectorSet(vec3.mData[0], vec3.mData[1], vec3.mData[2], 0);
			}
			else if (lString.Compare("PreRotation") == 0) {
				vec3 = lProperty.Get<FbxDouble3>();
				PreRotation = XMVectorSet(3.141592 * vec3.mData[0] / 180.0, 3.141592 * vec3.mData[2] / 180.0, 3.141592 * vec3.mData[1] / 180.0, 0);
			}
			else if (lString.Compare("PostRotation") == 0) {
				vec3 = lProperty.Get<FbxDouble3>();
				PostRotation = XMVectorSet(3.141592 * vec3.mData[0]/180.0, 3.141592 * vec3.mData[2] / 180.0, 3.141592 * vec3.mData[1] / 180.0, 0);
			}
			lProperty = pObject->GetNextProperty(lProperty);
		}
		// why rotation dvide with pre / post / geometry??
		
		PreRotationMatrix = XMMatrixIdentity();
		PreRotationMatrix = XMMatrixMultiply(PreRotationMatrix, XMMatrixRotationX(PreRotation.m128_f32[0]));
		PreRotationMatrix = XMMatrixMultiply(PreRotationMatrix, XMMatrixRotationY(PreRotation.m128_f32[1]));
		PreRotationMatrix = XMMatrixMultiply(PreRotationMatrix, XMMatrixRotationZ(PreRotation.m128_f32[2]));

		GeometricWorldMatrix = XMMatrixScaling(GScale.m128_f32[0], GScale.m128_f32[1], GScale.m128_f32[2]);
		GeometricWorldMatrix = XMMatrixMultiply(GeometricWorldMatrix, XMMatrixRotationX(GRotate.m128_f32[0]));
		GeometricWorldMatrix = XMMatrixMultiply(GeometricWorldMatrix, XMMatrixRotationY(GRotate.m128_f32[1]));
		GeometricWorldMatrix = XMMatrixMultiply(GeometricWorldMatrix, XMMatrixRotationZ(GRotate.m128_f32[2]));
		GeometricWorldMatrix = XMMatrixMultiply(GeometricWorldMatrix, 
			XMMatrixTranslation(GTranslate.m128_f32[0], GTranslate.m128_f32[1], GTranslate.m128_f32[2]));

		PostRotationMatrix = XMMatrixIdentity();
		PostRotationMatrix = XMMatrixMultiply(PostRotationMatrix, XMMatrixRotationX(PostRotation.m128_f32[0]));
		PostRotationMatrix = XMMatrixMultiply(PostRotationMatrix, XMMatrixRotationY(PostRotation.m128_f32[1]));
		PostRotationMatrix = XMMatrixMultiply(PostRotationMatrix, XMMatrixRotationZ(PostRotation.m128_f32[2]));
		//GeometricWorldMatrix = XMMatrixTranspose(GeometricWorldMatrix);
		//robj->pos = { robj->pos.x / LCLscale.m128_f32[0], robj->pos.y / LCLscale.m128_f32[1], robj->pos.z / LCLscale.m128_f32[2] };

		//what is preRotation??
		XMVECTOR pvec = XMVectorSet(robj->pos.x, robj->pos.y, robj->pos.z, 1.0f);
		pvec = XMVector4Transform(pvec, PreRotationMatrix);
		robj->pos = { pvec.m128_f32[0], pvec.m128_f32[1], pvec.m128_f32[2] };

		//attribute (mesh .. etc)
		FbxNodeAttribute::EType lAttributeType;
		int i;
		if (pNode->GetNodeAttribute()) {
			lAttributeType = (pNode->GetNodeAttribute()->GetAttributeType());
			switch (lAttributeType)
			{
			case FbxNodeAttribute::eMarker:
			case FbxNodeAttribute::eSkeleton:
				break;
			case FbxNodeAttribute::eMesh:
			{
				// load Mesh
				MeshComponent* mesh = (MeshComponent*)fm->_New(sizeof(MeshComponent), true);
				mesh->Init();
				mesh->mesh = (Mesh*)fm->_New(sizeof(Mesh), true);
				mesh->mesh->Init((int)RenderStateType::Pong, 0);

				FbxMesh* pMesh = pNode->GetMesh();

				// scaling with lcl scale
				FbxVector4* lControlPoints = pMesh->GetControlPoints();
				int ctrlPos_count = pMesh->GetControlPointsCount();
				for (int k = 0; k < ctrlPos_count; ++k) {
					XMVECTOR v = XMVectorSet(lControlPoints[k].mData[0], lControlPoints[k].mData[1],
						lControlPoints[k].mData[2], 1.0f);
					
					v = XMVector4Transform(v, GeometricWorldMatrix);
					
					v *= LCLscale;

					//v = XMVector4Transform(v, PostRotationMatrix);

					lControlPoints[k].mData[0] = v.m128_f32[0];
					lControlPoints[k].mData[1] = v.m128_f32[1];
					lControlPoints[k].mData[2] = v.m128_f32[2];
				}

				int lPolygonCount = pMesh->GetPolygonCount();
				int vertexCount = 0;
				for (int i = 0; i < lPolygonCount; ++i) {
					int lPolygonSize = pMesh->GetPolygonSize(i);
					vertexCount += lPolygonSize;
				}

				fm->_tempPushLayer();
				BasicVertex* vertexArr = (BasicVertex*)fm->_tempNew(sizeof(BasicVertex) * vertexCount, true);
				fmvecarr<TriangleIndex> TrianglePool;
				TrianglePool.NULLState();
				TrianglePool.Init(lPolygonCount * 3, false);
				int vi = 0;
				for (int i = 0; i < lPolygonCount; ++i) {
					int lPolygonSize = pMesh->GetPolygonSize(i);
					int bvi[32] = {};
					for (int k = 0; k < lPolygonSize; ++k) {
						int lControlPointIndex = pMesh->GetPolygonVertex(i, k);
						FbxVector4 pos = lControlPoints[lControlPointIndex];
						FbxGeometryElementUV* leUV = pMesh->GetElementUV(0);
						FbxGeometryElementNormal* leNormal = pMesh->GetElementNormal(0);
						FbxGeometryElementTangent* leTangent = pMesh->GetElementTangent(0);
						FbxGeometryElementBinormal* leBinormal = pMesh->GetElementBinormal(0);
						FbxVector2 uv;
						FbxVector4 normal, tangent, binorminal;
						switch (leUV->GetMappingMode())
						{
						default:
							break;
						case FbxGeometryElement::eByControlPoint:
							switch (leUV->GetReferenceMode())
							{
							case FbxGeometryElement::eDirect:
								uv = leUV->GetDirectArray().GetAt(lControlPointIndex);
								break;
							case FbxGeometryElement::eIndexToDirect:
							{
								int id = leUV->GetIndexArray().GetAt(lControlPointIndex);
								uv = leUV->GetDirectArray().GetAt(id);
							}
							break;
							default:
								break; // other reference modes not shown here!
							}
							break;

						case FbxGeometryElement::eByPolygonVertex:
						{
							int lTextureUVIndex = pMesh->GetTextureUVIndex(i, k);
							switch (leUV->GetReferenceMode())
							{
							case FbxGeometryElement::eDirect:
							case FbxGeometryElement::eIndexToDirect:
							{
								uv = leUV->GetDirectArray().GetAt(lTextureUVIndex);
							}
							break;
							default:
								break; // other reference modes not shown here!
							}
						}
						break;
						case FbxGeometryElement::eByPolygon: // doesn't make much sense for UVs
						case FbxGeometryElement::eAllSame:   // doesn't make much sense for UVs
						case FbxGeometryElement::eNone:       // doesn't make much sense for UVs
							break;
						}

						if (leNormal->GetMappingMode() == FbxGeometryElement::eByPolygonVertex)
						{
							switch (leNormal->GetReferenceMode())
							{
							case FbxGeometryElement::eDirect:
								normal = leNormal->GetDirectArray().GetAt(vi);
								break;
							case FbxGeometryElement::eIndexToDirect:
							{
								int id = leNormal->GetIndexArray().GetAt(vi);
								normal = leNormal->GetDirectArray().GetAt(id);
							}
							break;
							default:
								break; // other reference modes not shown here!
							}
						}
						
						if (leTangent != nullptr && leTangent->GetMappingMode() == FbxGeometryElement::eByPolygonVertex)
						{
							switch (leTangent->GetReferenceMode())
							{
							case FbxGeometryElement::eDirect:
								tangent = leTangent->GetDirectArray().GetAt(vi);
								break;
							case FbxGeometryElement::eIndexToDirect:
							{
								int id = leTangent->GetIndexArray().GetAt(vi);
								tangent = leTangent->GetDirectArray().GetAt(id);
							}
							break;
							default:
								break; // other reference modes not shown here!
							}
						}
						
						if (leBinormal != nullptr && leBinormal->GetMappingMode() == FbxGeometryElement::eByPolygonVertex)
						{
							switch (leBinormal->GetReferenceMode())
							{
							case FbxGeometryElement::eDirect:
								binorminal = leBinormal->GetDirectArray().GetAt(vi);
								break;
							case FbxGeometryElement::eIndexToDirect:
							{
								int id = leBinormal->GetIndexArray().GetAt(vi);
								binorminal = leBinormal->GetDirectArray().GetAt(id);
							}
							break;
							default:
								break; // other reference modes not shown here!
							}
						}
						
						BasicVertex bv;
						bv.position = {(float)pos.mData[0], (float)pos.mData[2], (float)pos.mData[1]};
						bv.texcoord = { (float)uv.mData[0], (float)uv.mData[1]};
						bv.normal = { (float)normal.mData[0], (float)normal.mData[1], (float)normal.mData[2]};
						bv.tangent = { (float)tangent.mData[0], (float)tangent.mData[1], (float)tangent.mData[2]};
						bv.bitangent = { (float)binorminal.mData[0], (float)binorminal.mData[1], (float)binorminal.mData[2]};
						
						vertexArr[vi] = bv;
						bvi[k] = vi;
						vi += 1;
					}

					for (int k = 1; k + 1 < lPolygonSize; ++k) {
						TriangleIndex ti;
						ti.v[0] = bvi[k+1];
						ti.v[1] = bvi[k];
						ti.v[2] = bvi[0];

						//why index are inverse order?
						// >> this is because yz swap. 
						// 3dx max model to left hand yup space model..

						if (vertexArr[ti.v[0]].HaveToSetTangent()) {
							BasicVertex::CreateTBN(vertexArr[ti.v[0]],
								vertexArr[ti.v[1]], vertexArr[ti.v[2]]);
						}

						if (vertexArr[ti.v[1]].HaveToSetTangent()) {
							BasicVertex::CreateTBN(vertexArr[ti.v[1]],
								vertexArr[ti.v[0]], vertexArr[ti.v[2]]);
						}

						if (vertexArr[ti.v[2]].HaveToSetTangent()) {
							BasicVertex::CreateTBN(vertexArr[ti.v[2]],
								vertexArr[ti.v[0]], vertexArr[ti.v[1]]);
						}

						TrianglePool.push_back(ti);
					}
				}

				mesh->mesh->indexCount = TrianglePool.size() * 3;
				
				//index buffer -> TrianglePool
				BOOL bResult = FALSE;
				CD3D12Renderer* renderer = Mesh::Renderers[mesh->mesh->RendererID];
				ID3D12Device5* pD3DDeivce = renderer->INL_GetD3DDevice();
				CD3D12ResourceManager* pResourceManager = renderer->INL_GetResourceManager();
				//buffer read by gpu memory
				if (FAILED(pResourceManager->CreateVertexBuffer(sizeof(BasicVertex), (DWORD)vi, &mesh->mesh->m_VertexBufferView, &mesh->mesh->m_pVertexBuffer, vertexArr))) {
					_CrtDbgBreak();
				}

				if (FAILED(pResourceManager->CreateIndexBuffer((DWORD)(TrianglePool.size() * 3), &mesh->mesh->m_IndexBufferView, &mesh->mesh->m_pIndexBuffer, &TrianglePool[0]))) {
					_CrtDbgBreak();
				}

				robj->PushComponent<MeshComponent>(mesh);

				fm->_tempPopLayer();
			}
				break;
			case FbxNodeAttribute::eNurbs:
			case FbxNodeAttribute::ePatch:
			case FbxNodeAttribute::eCamera:
			case FbxNodeAttribute::eLight:
			case FbxNodeAttribute::eLODGroup:
				break;
			}
		}

		int childCount = pNode->GetChildCount();
		if (childCount > 0) {
			ChildComponent* childcomp = (ChildComponent*)fm->_New(sizeof(ChildComponent), true);
			childcomp->Init(childCount);
			for (i = 0; i < childCount; i++)
			{
				Object* obj = LoadObjectFromFBX_Node(pNode->GetChild(i));
				childcomp->push_child(obj);
			}
			robj->PushComponent<ChildComponent>(childcomp);
		}

		robj->PushObjectTypeToCTable();
		return robj;
	}

	void PushObjectTypeToCTable() {
		//when
		ComponentArr = mcvt(Component*)(mcvt(ui64)(ComponentArr) & PtrMask);

		bool typeexist = true;
		ui64 t = 255;
		for (int i = 0; i < ctableup; ++i) {
			for (int k = 0; k < Up; ++k) {
				if (ctable[i][(int)ComponentArr[k].gettype()] != k) {
					typeexist = false;
					break;
				}
			}

			if (typeexist) {
				t = i;
				break;
			}
		}
		if (t != 255) {
			t << 56;
			ComponentArr = mcvt(Component*)(mcvt(ui64)(ComponentArr) | t);
			return;
		}

		if (ctableup + 1 < 256) {
			for (int i = 0; i < Up; ++i) {
				ctable[ctableup][(int)ComponentArr[i].gettype()] = i;
			}
			ComponentArr = mcvt(Component*)(mcvt(ui64)(ComponentArr) & PtrMask);
			t = ctableup;
			t = t << 56;
			ComponentArr = mcvt(Component*)(mcvt(ui64)(ComponentArr) | t);
			ctableup += 1;
		}
	}
};

// Fixed Object - Fixed. cannot add and delete components.
// multiple structure.
// operate with ECS framework..

struct MObject32def {
	constexpr static ui16 type = 0x5000; // 8[32] - 12[0]
	constexpr static ui64 coliderFlag = 0;
	ui16 rot[3];
	hfloat scale;
	XMFLOAT3 pos; 
	ui32 extradata;
	Component ComponentArr[1];
};

union MObject32 {
	MObject32def def;
};

struct MObject64def {
	constexpr static ui16 type = 0x6000; // 8[64] - 12[0]
	constexpr static ui64 coliderFlag = 0;
	ui16 rot[3];
	hfloat scale;
	XMFLOAT3 pos;
	ui32 extradata;
	Component ComponentArr[5];
};

union MObject64 {
	MObject64def def;
};

struct MObject128def {
	constexpr static ui16 type = 0x7000; // 8[128] - 12[0]
	constexpr static ui64 coliderFlag = 0;
	ui16 rot[3];
	hfloat scale;
	XMFLOAT3 pos;
	ui32 extradata;
	Component ComponentArr[13];
};

union MObject128 {
	MObject128def def;
};

struct MObject256def {
	constexpr static ui16 type = 0x8000; // 8[256] - 12[0]
	constexpr static ui64 coliderFlag = 0;
	ui16 rot[3];
	hfloat scale;
	XMFLOAT3 pos;
	ui32 extradata;
	Component ComponentArr[29];
};

union MObject256 {
	MObject256def def;
};

struct MObject512def {
	constexpr static ui16 type = 0x9000; // 8[512] - 12[0]
	constexpr static ui64 coliderFlag = 0;
	ui16 rot[3];
	hfloat scale;
	XMFLOAT3 pos;
	ui32 extradata;
	Component ComponentArr[61];
};

union MObject512 {
	MObject512def def;
};

struct MObject1024def {
	constexpr static ui16 type = 0xA000; // 8[1024] - 12[0]
	constexpr static ui64 coliderFlag = 0;
	ui16 rot[3];
	hfloat scale;
	XMFLOAT3 pos;
	ui32 extradata;
	Component ComponentArr[125];
};

union MObject1024 {
	MObject1024def def;
};

// 1 page 4096 byte / 4KB
union MGameObjectChunck {
	MObject32 obj32[128];
	MObject64 obj64[64];
	MObject128 obj128[32];
	MObject256 obj256[16];
	MObject512 obj512[8];
	MObject1024 obj1024[4];
};
//fmvecarr<MGameObjectChunck*> MObjectArr;

struct MOChunckType {
	ui16 objtype; //[ 4bit obj size (log2) ][ 12bit objecttype ]
	ui16 objcount; // count of object
	si32 x;
	si32 y;
	si32 z; //[ 48bit chunck location (si16, si16, si16)]
};

//fm.AllocNewPages
struct MGameObjectChunckLump{
	MGameObjectChunck chuncks[512] = {};
};
extern fmvecarr<MGameObjectChunckLump*> MGameObjChunckLumps;
extern fmvecarr<MOChunckType> MChunckTypeVector;
void AllocNewMGOCLump();
__forceinline MGameObjectChunck& GetMGOChunck(ui32 index);

// chunck length = x64m / y64m / z64m - 1 : 1m
// (0, 0, 0) -> {-32~32, -32~32, -32~32}
extern fmvecarr<void*> SeekChunckArr; 
extern fmvecarr<void*> TempSeekChunckArr;
extern ui16 SeekFlag;
struct SpaceChunck {
	static constexpr float ChunckRange = 64.0f;

	SpaceChunck* nearSpaceChunck[3][3][3] = { {{}} };
	void* parentChunck[8] = {};
	fmvecarr<Object*> GeneralGameObjectArr;
	fmvecarr<MGameObjectChunck*> MOChunckArr;

	fmvecarr<ui16> Collision_StaticTour;
	fmvecarr<ui16> Collision_DynamicTour;
	fmvecarr<ui16> illuminate_StaticTour;
	fmvecarr<ui16> illuminate_DynamicTour;
	fmvecarr<ui16> EventObject_Tour;
	si16 cx;
	si16 cy;
	si16 cz;
	ui16 seekFlags;
	// TOUR : index of array<MGameObjectChunck>
	// list 로 해도 되지 않을까? >
	// 사유 : 1. 수정이 용이함.
	//		  2. random access 필요 없음
	//        3. capacity가 다 차도 새로 할당 안해도 됨.

	static void Static_Init() {
		SeekChunckArr.NULLState();
		SeekChunckArr.Init(8, true);
		TempSeekChunckArr.NULLState();
		TempSeekChunckArr.Init(8, true);
	}

	void Init();

	void AddObject(Object* obj) {
		GeneralGameObjectArr.push_back(obj);
	}

	//Game Loops
	void Update(float delta) {
		// Move & Collision Update
		for (int i = 0; i < GeneralGameObjectArr.size(); ++i) {
			Object* obj = GeneralGameObjectArr.at(i);
			obj->Update(delta);
		}
		for (int i = 0; i < Collision_DynamicTour.size(); ++i) {
			ui32 n = Collision_DynamicTour[i];
		}
	}

	// Object Renders
	void Render(ID3D12GraphicsCommandList* pCommandList) {
		for (int i = 0; i < GeneralGameObjectArr.size(); ++i) {
			Object* obj = GeneralGameObjectArr[i];
			obj->Render(pCommandList, XMMatrixIdentity());
		}
	}

	void ChunckSeeking() {
		SeekChunckArr.push_back(this);
		seekFlags = SeekFlag;

		for (int ix = 0; ix < 3; ++ix) {
			for (int iy = 0; iy < 3; ++iy) {
				for (int iz = 0; iz < 3; ++iz) {
					SpaceChunck* scptr = nearSpaceChunck[ix][iy][iz];
					if (scptr != nullptr && (SeekFlag != scptr->seekFlags && isSpaceChunckVisibleInCamera())) {
						TempSeekChunckArr.push_back(scptr);
					}
				}
			}
		}
	}

	void Event(WinEvent evt) {
		// event loop
		for (int i = 0; i < GeneralGameObjectArr.size(); ++i) {
			Object* obj = GeneralGameObjectArr[i];
		}

		for (int i = 0; i < EventObject_Tour.size(); ++i) {
			MGameObjectChunck& gmoc = GetMGOChunck(EventObject_Tour[i]);
			MOChunckType& gmoct = MChunckTypeVector[EventObject_Tour[i]];
			//if ((cx != gmoct.x || cy != gmoct.y) || cz != gmoct.z) {
			//	EventObject_Tour.erase(i);
			//	break;
			//}
			ui16 objtype = MChunckTypeVector[EventObject_Tour[i]].objtype;
			ui16 objdatasize = (objtype >> 12);
			ui16 objcount = MChunckTypeVector[EventObject_Tour[i]].objcount;
			for (int i = 0; i < objcount; ++i) {
				switch (objdatasize) {
				case 5: // 32
					break;
				case 6: // 64
					break;
				case 7: // 128
					break;
					//...
				}
			}
		}
	}

	static XMFLOAT2 GetRange(si16 x) {
		XMFLOAT2 v2;
		v2.x = ChunckRange * (float)x + ChunckRange * 0.5f;
		v2.y = ChunckRange * (float)x + ChunckRange * 0.5f;
		return v2;
	}

	bool isSpaceChunckVisibleInCamera() {
		XMVECTOR v[2][2][2] = { {{}} };
		bool b = false;
		for (int xi = 0; xi < 2; ++xi) {
			for (int yi = 0; yi < 2; ++yi) {
				for (int zi = 0; zi < 2; ++zi) {
					float xv[2];
					xv[0] = GetRange(cx).x;
					xv[1] = GetRange(cx).y;
					float yv[2];
					yv[0] = GetRange(cy).x;
					yv[1] = GetRange(cy).y;
					float zv[2];
					zv[0] = GetRange(cz).x;
					zv[1] = GetRange(cz).y;
					v[xi][yi][zi] = XMVectorSet(xv[xi], yv[xi], zv[zi], 0.0f);

					XMVECTOR vv = v[xi][yi][zi];
					b |= CONSTANT_BUFFER_DEFAULT::isPosVisibleInCamera(vv.m128_f32[0], vv.m128_f32[1], vv.m128_f32[2]);
					if (b) return true;
				}
			}
		}
		return false;
	}
};

struct Octree {
	void* nodeArr[2][2][2] = { {{}} };

	void Init() {
		for (int x = 0; x < 2; ++x) {
			for (int y = 0; y < 2; ++y) {
				for (int z = 0; z < 2; ++z) {
					nodeArr[x][y][z] = nullptr;
				}
			}
		}
	}

	SpaceChunck* AddSpaceChunck(si16 x, si16 y, si16 z) {
		Octree* ptree = this;
		Octree** pref = &ptree;
		ui16 mask = 1 << 15;
		for (int i = 0; i < 15; ++i) {
			pref = (Octree**)(*pref)->GetChild(x & mask, y & mask, z & mask);
			if (*pref == nullptr) {
				*pref = (Octree*)fm->_New(sizeof(Octree), true);
				(*pref)->Init();
			}
			mask = mask >> 1;
		}

		SpaceChunck** spaceref = (SpaceChunck**)(*pref)->GetChild(x & mask, y & mask, z & mask);
		if (*spaceref == nullptr) {
			*spaceref = (SpaceChunck*)fm->_New(sizeof(SpaceChunck), true);
			(*spaceref)->Init();
		}

		return *spaceref;
	}

	SpaceChunck* GetSpaceChunck(si16 x, si16 y, si16 z) {
		Octree* ptree = this;
		Octree** pref = &ptree;
		ui16 mask = 1 << 15;
		for (int i = 0; i < 15; ++i) {
			pref = (Octree**)(*pref)->GetChild(x & mask, y & mask, z & mask);
			if (*pref == nullptr) {
				return nullptr;
			}
			mask = mask >> 1;
		}

		SpaceChunck** spaceref = (SpaceChunck**)(*pref)->GetChild(x & mask, y & mask, z & mask);
		if (*spaceref == nullptr) {
			return nullptr;
		}
		return *spaceref;
	}

	SpaceChunck* GetSpaceChunckWithWorldPosition(float x, float y, float z) {
		static constexpr float div64 = 1.0f / 64.0f;
		si16 v[3] = {};
		v[0] = (si64)((x + 32.0f) * div64 - 0.5f);
		v[1] = (si64)((y + 32.0f) * div64 - 0.5f);
		v[2] = (si64)((z + 32.0f) * div64 - 0.5f);
		return GetSpaceChunck(v[0], v[1], v[2]);
	}

	void** GetChild(ui8 x, ui8 y, ui8 z) {
		return &nodeArr[!!x][!!y][!!z];
	}

	__forceinline void SeekChuncks() {
		SeekChunckArr.up = 0;
		SeekFlag += 1;
		XMFLOAT3 f3 = CONSTANT_BUFFER_DEFAULT::SCamPos;
		SpaceChunck* sc = GetSpaceChunckWithWorldPosition(f3.x, f3.y, f3.z);

		sc->ChunckSeeking();

		int pastN = 0;
		int postN = 0;
	Octree_ChunckSeeking_Loop:
		for (int i = pastN; i < postN; ++i) {
			SpaceChunck* scptr = (SpaceChunck*)SeekChunckArr[i];
			scptr->ChunckSeeking();
		}

		pastN = SeekChunckArr.size();
		for (int i = 0; i < TempSeekChunckArr.size(); ++i) {
			SeekChunckArr.push_back(TempSeekChunckArr[i]);
		}
		TempSeekChunckArr.up = 0;
		postN = SeekChunckArr.size();

		if (postN > pastN) {
			goto Octree_ChunckSeeking_Loop;
		}
	}

	void GameWorldRender(ID3D12GraphicsCommandList* pCommandList) {
		SeekChuncks();
		
		for (int i = 0; i < SeekChunckArr.size(); ++i) {
			SpaceChunck* scptr = (SpaceChunck*)SeekChunckArr[i];
			scptr->Render(pCommandList);
		}
	}
};

extern Octree GameWorld;

#endif
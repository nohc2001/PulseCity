#pragma once
#include "stdafx.h"
#include "Render.h"
#include "Game.h"
#include "main.h"

class Mesh;
class Shader;
struct GPUResource;

/*
* ���� : Shape�� Mesh�� Model�� ������ �� �ִ� ����� ��Ÿ�� ����ü.
* highest bit == 1 -> Mesh
* else -> Model
*
* Sentinal Value :
* NULL : (FlagPtr = 0);
* isMesh : (FlagPtr & 0x8000000000000000);
* isModel : !isMesh;
*/
struct Shape {
	ui64 FlagPtr = 0;

	Shape() : FlagPtr{ 0 } {}
	Shape(Mesh* mesh) {
		SetMesh(mesh);
	}
	Shape(Model* model) {
		SetModel(model);
	}

	/*
	* ����/��ȯ : Shape�� Mesh���� ���θ� ��ȯ
	*/
	__forceinline bool isMesh() {
		return FlagPtr & 0x8000000000000000;
	}

	/*
	* ����/��ȯ : Shape�� ���� Mesh �����͸� ��ȯ
	*/
	__forceinline Mesh* GetMesh() {
		if (isMesh()) {
			return reinterpret_cast<Mesh*>(FlagPtr & 0x7FFFFFFFFFFFFFFF);
		}
		else return nullptr;
	}

	/*
	* ���� : Shape�� Mesh�� �ִ´�.
	*/
	__forceinline void SetMesh(Mesh* ptr) {
		FlagPtr = reinterpret_cast<ui64>(ptr);
		FlagPtr |= 0x8000000000000000;
	}

	/*
	* ����/��ȯ : Shape�� ���� Model �����͸� ��ȯ
	*/
	__forceinline Model* GetModel() {
		if (isMesh()) return nullptr;
		else {
			return reinterpret_cast<Model*>(FlagPtr);
		}
	}

	/*
	* ���� : Shape�� Model�� �ִ´�.
	*/
	__forceinline void SetModel(Model* ptr) {
		FlagPtr = reinterpret_cast<ui64>(ptr);
	}

	/*
	* ����/��ȯ : Shape�� �̸��� �޾Ƽ� �ش� Shape�� �ε����� ��ȯ�Ѵ�.
	*/
	static int GetShapeIndex(string meshName);
	/*
	* ����/��ȯ : Shape�� �̸��� �޾Ƽ� ShapeNameArr�� ������ �ش� Shape�� �ε����� ��ȯ�Ѵ�.
	*/
	static int AddShapeName(string meshName);

	// �̸����� ShapeIndex�� ��� map
	static unordered_map<string, int> StrToShapeIndex;
	static vector<Shape> ShapeTable;
	static vector<string> ShapeStrTable;

	/*
	* ���� : Mesh�� �̸��� Mesh �����͸� �޾� Mesh�� �߰��ϴ� �Լ�
	* �Ű����� :
	* string name : Mesh�� �̸�
	* Mesh* ptr : Mesh�� ������
	*/
	static int AddMesh(string name, Mesh* ptr);

	/*
	* ���� : Model�� �̸��� Model �����͸� �޾� Model�� �߰��ϴ� �Լ�
	* �Ű����� :
	* string name : Model�� �̸�
	* Model* ptr : Model�� ������
	*/
	static int AddModel(string name, Model* ptr);

	void GetRealShape(Mesh*& out0, Model*& out1) {
		if (isMesh()) out0 = reinterpret_cast<Mesh*>(FlagPtr & 0x7FFFFFFFFFFFFFFF);
		else out1 = reinterpret_cast<Model*>(FlagPtr & 0x7FFFFFFFFFFFFFFF);
	}
};

union Tag {
	UINT tag = 0;
	operator UINT() { return tag; }
	operator bool() { return tag; }

	Tag() {}
	Tag(UINT n) : tag{ n } {}

	struct TagSetter {
		Tag* t;
		int index;

		operator bool() { return t->tag; }

		void operator=(bool b) {
			if (b) {
				t->tag |= index;
			}
			else {
				t->tag &= ~index;
			}
		}
	};

	// bool�ε� �� �� ����.
	TagSetter operator[](UINT MaskIndex) {
		TagSetter ts;
		ts.t = this;
		ts.index = MaskIndex;
		return ts;
	}
};

enum GameObjectTag {
	Tag_Enable = 1, // ���ӿ�����Ʈ Ȱ��ȭ ����
	Tag_Dynamic = 2, // ���ӿ�����Ʈ�� ������ �� �ִ��� ����
	// ���� Tag_Dynamic == true ���.
	Tag_SkinMeshObject = 3,
	// ���� Tag_Dynamic == false ���.
};

struct GameObject {
#define STC_CurrentStruct GameObject
	STC_STATICINIT_innerStruct;

	inline static void* StaticVptr = nullptr;
	inline static void* DynamicVptr = nullptr;
	inline static void* SkinMeshVptr = nullptr;
	template <typename T> inline static void* Vptr = nullptr;
	static void StaticInit();

	template <typename T> __forceinline static bool IsType(GameObject* go) {
		return Vptr<T> == *(void**)go;
	}

	/////////////////
	// ûũ�� ���� �� ƽ�� ���ӿ�����Ʈ �� �ѹ��� �ؾ��ϴ� �۾��� �ִٸ� �� ���� �������.
	UINT TourID = 0;

	/*
	* ���ӿ�����Ʈ�� �����ϰ� Ž���ϱ� ���� tag. 32���� tag�� ������ �� �ִ�.
	* �׻� tag�� ù��° ��Ʈ�� enable�̴�. (���ӿ�����Ʈ�� Ȱ��ȭ �Ǿ��ִ��� ����)
	*/
	STCDef(Tag, tag);

	// appearance
	Shape shape;
	union {
		int* material = nullptr; // mesh �� ��쿡�� Ȱ��ȭ��. slotNum��ŭ. game.MaterialTable���� ����.
		matrix* transforms_innerModel; // model �� ��츸 Ȱ��ȭ��. nodeCount ��ŭ.
	};

	// ��������
	GameObject* parent = nullptr;
	GameObject* childs = nullptr;
	GameObject* sibling = nullptr;
	union {
		char extra[8];
		float* RaytracingWorldMatInput = nullptr;
		float** RaytracingWorldMatInput_Model; // float*�� 2���� �迭�� ������.
		// [nodeIndex][subMeshIndex] ���µ�.. ���� subMesh�� Raytracing ��ü���� ���������� ������..
		// �̰� �׳� 1���� �迭�� �ٲ��� ��.
	};

	// transform
	matrix worldMat;

	//ó�� ���� �ÿ��� enable�� false. ������Ʈ�� ���� �ʴ´�. 
	// ������� �ʱ�ȭ�� �Ϸ�Ǿ� ���ӷ����� �� �غ� ��ġ�� �Ǹ� �׶� Enable�� �Ѵ�.
	GameObject();
	virtual ~GameObject();

	virtual matrix GetWorld();
	virtual void SetWorld(matrix localWorldMat);

	// render instance�� �̿��� ��ġó���� ���� �ʴ� ������ �������ÿ� ���
	virtual void Render(matrix parent = XMMatrixIdentity());
	virtual void PushRenderBatch(matrix parent = XMMatrixIdentity());
	// ���� ����ϴ� �����Լ��� ����Ŵ.
	using RenderFuncType = void (GameObject::*)(matrix parent);
	inline static RenderFuncType CurrentRenderFunc = &GameObject::Render;

	virtual void Release();
	virtual BoundingOrientedBox GetOBB();

	virtual void SetShape(Shape _shape);
	virtual void SetShape(Model* _shape);
	virtual void SetShape(Mesh* _shape);

	virtual void OnRayHit(GameObject* rayFrom);

	void RaytracingUpdateTransform();
	void RaytracingUpdateTransform(Model* model, ModelNode* node, matrix parent);

	void DbgHieraky();

#pragma pack(push, 1)
	struct STC_SyncObjData {
		Tag tag;
		int shapeindex;
		int parent;
		int childs;
		int sibling;
		matrix worldMatrix;
	};
#pragma pack(pop)
	
	// ����� ����ó���� �ϼ�.
	virtual void RecvSTC_SyncObj(char* data);
	static void STATICINIT(int typeindex = GameObjectType::_GameObject);

#undef STC_CurrentStruct
};

struct StaticGameObject : public GameObject {
	StaticGameObject();
	virtual ~StaticGameObject();
	vector<BoundingOrientedBox> obbArr;

	virtual matrix GetWorld();
	virtual void SetWorld(matrix localWorldMat);

	virtual void Render(matrix parent = XMMatrixIdentity());
	virtual void PushRenderBatch(matrix parent = XMMatrixIdentity());
	// ���� ����ϴ� �����Լ��� ����Ŵ.
	using RenderFuncType = void (StaticGameObject::*)(matrix parent);
	inline static RenderFuncType CurrentRenderFunc = &StaticGameObject::Render;

	virtual void Release();
	virtual BoundingOrientedBox GetOBB();

	bool Collision_Inherit(matrix parent_world, BoundingBox bb);
	void InitMapAABB_Inherit(void* origin, matrix parent_world);
	BoundingOrientedBox GetOBBw(matrix worldMat);

	virtual void SetShape(Shape _shape);

#pragma pack(push, 1)
	struct STC_SyncObjData {
		Tag tag;
		int shapeindex;
		int parent;
		int childs;
		matrix worldMatrix;
		int sibling;
	};
#pragma pack(pop)

	// ����� ����ó���� �ϼ�.
	__forceinline void RecvSTC_SyncObj(char* data);
	static void STATICINIT(int typeindex = GameObjectType::_StaticGameObject);
};

/*
* ���� : ûũ�� ã�ư��� ���� �ε���
* Sentinal Value :
* NULL = (x == -2,147,483,648 || y == -2,147,483,648 || z == -2,147,483,648)
*/
struct ChunkIndex {
	int x = 0;
	int y = 0;
	int z = 0;
	int extra = 0;

	ChunkIndex() {}
	~ChunkIndex() {}

	ChunkIndex(int X, int Y, int Z) {
		x = X;
		y = Y;
		z = Z;
	}
	ChunkIndex(const ChunkIndex& ref) {
		x = ref.x;
		y = ref.y;
		z = ref.z;
	}

	__forceinline bool operator==(const ChunkIndex& ci) const {
		return (x == ci.x && y == ci.y) && z == ci.z;
	}
	__forceinline bool operator!=(const ChunkIndex& ci) const {
		return x != ci.x || y != ci.y || z != ci.z;
	}

	BoundingBox GetAABB();
};

//�� ������ simd�� ����ȭ �����ϰڴ� ������ ���.
struct GameObjectIncludeChunks {
	int xmin;
	int ymin;
	int zmin;
	char xlen;
	char ylen;
	char zlen;
	unsigned char extraByte;

	void operator+=(const GameObjectIncludeChunks& range) {
		int xmax = xmin + xlen;
		int ymax = ymin + ylen;
		int zmax = zmin + zlen;
		int rxmax = range.xmin + range.xlen;
		int rymax = range.ymin + range.ylen;
		int rzmax = range.zmin + range.zlen;
		xmax = max(xmax, rxmax);
		ymax = max(ymax, rymax);
		zmax = max(zmax, rzmax);
		xmin = min(xmin, range.xmin);
		ymin = min(ymin, range.ymin);
		zmin = min(zmin, range.zmin);
		xlen = max(xmax - xmin, 0);
		ylen = max(ymax - ymin, 0);
		zlen = max(zmax - zmin, 0);
	}

	void operator&=(const GameObjectIncludeChunks& range) {
		int xmax = xmin + xlen;
		int ymax = ymin + ylen;
		int zmax = zmin + zlen;
		int rxmax = range.xmin + range.xlen;
		int rymax = range.ymin + range.ylen;
		int rzmax = range.zmin + range.zlen;
		xmax = min(xmax, rxmax);
		ymax = min(ymax, rymax);
		zmax = min(zmax, rzmax);
		xmin = max(xmin, range.xmin);
		ymin = max(ymin, range.ymin);
		zmin = max(zmin, range.zmin);
		xlen = xmax - xmin;
		ylen = ymax - ymin;
		zlen = zmax - zmin;
		extraByte = 0;
	}

	__forceinline int GetChunckSize() const {
		if (xlen < 0 || (ylen < 0 || zlen < 0)) return 0;
		return (int)(xlen + 1) * (int)(ylen + 1) * (int)(zlen + 1);
	}

	__forceinline ChunkIndex& Inc(ChunkIndex& ref) const {
		if (ref.z + 1 <= zmin + zlen) {
			ref.z = ref.z + 1;
			ref.extra += 1;
			return ref;
		}
		else {
			ref.z = zmin;
			if (ref.y + 1 <= ymin + ylen) {
				ref.y = ref.y + 1;
				ref.extra += 1;
				return ref;
			}
			else {
				ref.y = ymin;
				if (ref.x + 1 <= xmin + xlen) {
					ref.x = ref.x + 1;
					ref.extra += 1;
					return ref;
				}
			}
		}
		ref.extra += 1;
		return ref;
	}

	bool operator==(GameObjectIncludeChunks range) {
		bool b = (xmin == range.xmin);
		b = b && (ymin == range.ymin);
		b = b && (zmin == range.zmin);
		b = b && (xlen == range.xlen);
		b = b && (ylen == range.ylen);
		b = b && (zlen == range.zlen);
		return b;
	}

	__forceinline bool isInclude(const ChunkIndex& ci) const {
		return ((xmin <= ci.x && ci.x <= xmin + xlen) &&
			(ymin <= ci.y && ci.y <= ymin + ylen)) &&
			(zmin <= ci.z && ci.z <= zmin + zlen);
	}
};

struct DynamicGameObject : public GameObject {
#define STC_CurrentStruct DynamicGameObject
	STC_STATICINIT_innerStruct;

	DynamicGameObject();
	virtual ~DynamicGameObject();

	virtual matrix GetWorld();
	virtual void SetWorld(matrix localWorldMat);

	STCDef(vec4, LVelocity);
	//dest world
	STCDef(vec4, DestPos);
	STCDef(vec4, DestRot);
	STCDef(vec4, DestScale);

	GameObjectIncludeChunks IncludeChunks;
	int* chunkAllocIndexs = nullptr;
	int chunkAllocIndexsCapacity = 8;

	void InitialChunkSetting();
	void Move(vec4 velocity, vec4 Q);
	void Move(vec4 velocity, vec4 Q, GameObjectIncludeChunks afterChunkInc);
	virtual void Update(float delatTime);
	virtual void Render(matrix parent = XMMatrixIdentity());
	virtual void PushRenderBatch(matrix parent = XMMatrixIdentity());
	using RenderFuncType = void (DynamicGameObject::*)(matrix parent);
	inline static RenderFuncType CurrentRenderFunc = &DynamicGameObject::Render;

	virtual void Event(WinEvent evt);
	virtual void Release();
	virtual BoundingOrientedBox GetOBB();

	void LookAt(vec4 look, vec4 up = { 0, 1, 0, 0 });

	static void CollisionMove(DynamicGameObject* obj1, DynamicGameObject* obj2);
	virtual void OnCollision(GameObject* other);
	virtual void OnRayHit(GameObject* rayFrom);

	virtual void SetShape(Shape _shape);

	void PositionInterpolation(float deltaTime);
	virtual void MoveChunck(const matrix& afterMat, const GameObjectIncludeChunks& beforeChunckInc, const GameObjectIncludeChunks& afterChunkInc);

#pragma pack(push, 1)
	struct STC_SyncObjData {
		Tag tag;
		int shapeindex;
		int parent;
		int childs;
		int sibling;
		matrix DestWorld;
		vec4 LVelocity;
	};
#pragma pack(pop)

	virtual void RecvSTC_SyncObj(char* data);
	static void STATICINIT(int typeindex = GameObjectType::_DynamicGameObject);

	__forceinline static void SyncDestWolrd(GameObject* go, char* data, int len) {
		matrix& destWorld = *(matrix*)data;
		DynamicGameObject* dgo = (DynamicGameObject*)go;
		XMMatrixDecompose((XMVECTOR*) & dgo->DestScale, (XMVECTOR*)&dgo->DestRot, (XMVECTOR*)&dgo->DestPos, destWorld);
	}
#undef STC_CurrentStruct
};

//skinMesh �� ��� shader ��ü�� �ٸ��� ������ ��ġó���� �� �ÿ� ���� �������� �����ؾ� �Ѵ�.
struct SkinMeshGameObject : public DynamicGameObject {
#define STC_CurrentStruct SkinMeshGameObject
	STC_STATICINIT_innerStruct;
	SkinMeshGameObject();
	virtual ~SkinMeshGameObject();

	// temp data. later combine one upload CBV
	vector<matrix*> RootBoneMatrixs_PerSkinMesh;
	// [bone 0] [...] [bone N]
	vector<GPUResource> BoneToWorldMatrixCB;
	vector<GPUResource> BoneToWorldMatrixCB_Default;
	vector<DescIndex> BoneToWorldMatrix_UAVDescIndex;
	vector<GPUResource> NodeToBone;
	vector<DescIndex> NodeToBone_SRVDescIndex;

	// Ư�� Node�� ���� ToWorldMatrix�� �������� ���� ReadBackBuffer
	GPUResource NodeWorldMatrixReadBack;
	// ���° Node�� ������ ������ ���� �迭.
	vector<int> GetWorldMat_NodeIndexArr;
	// CPU ���� ������ 
	matrix* Mapped_NodeWorldMatrixReadBack;

#pragma region GPUAnimation
	struct AnimationBlendingCBStruct
	{
		// �������� 4���� �ִϸ��̼� ������ �ð�
		float animTime[4];

		// �������� 4���� �ִϸ��̼� ������ �ִ�ð�
		float MAXTime[4];

		// �������� 4���� �ִϸ��̼� ������ ������ ����ġ
		float animWeight[4];

		// Root�� ������ 64���� Humanoid Bone�� ����ŷ.
		ui64 animMask[4];

		// �ִϸ��̼� ������ ����Ʈ
		float frameRate;
	};

	// �� ���� ���� ����.
	GPUResource NodeLocalMatrixs;
	DescIndex NodeLocalMatrixs_UAVDescIndex;
	DescIndex NodeLocalMatrixs_SRVDescIndex;

	GPUResource NodeTposMatrixs;
	DescIndex NodeTposMatrixs_SRVDescIndex;

	GPUResource Node_ToParentRes;
	DescIndex NodeToParentSRVIndex;

	GPUResource HumanoidToNodeIndexRes;
	DescIndex HumanoidToNodeIndexSRVIndex;

	GPUResource AnimationBlendConstantUploadBuffer;
	AnimationBlendingCBStruct* AnimBlendingCB_Mapped;
	DescIndex AnimBlendingCBVDescIndex;
#pragma endregion
	
	// non shader visible desc heap�� ��ġ��. 
	// model->mNumSkinMesh ��ŭ ������.
	DescIndex* OutVertexUAV;

	// ����Ʈ���̽��� �Ҷ� ������ �����Ǵ� �޽�.
	// model�� skinmeshcount ��ŭ ������.
	RayTracingMesh* modifyMeshes = nullptr;

	float AnimationFlowTime[4] = {};
	STCDefArr(float, DestAnimationFlowTime, 4);
	STCDefArr(int, PlayingAnimationIndex, 4);

	ui64 m_animMask[4] = { 0xFFFFFFFFFFFFFFFFULL, 0ULL, 0ULL, 0ULL };

	void InitRootBoneMatrixs();
	void SetRootMatrixs();

	void GetBoneLocalMatrixAtTime(HumanoidAnimation* hanim, matrix* out, float time);
	virtual void RaytracingUpdateTransform();

	virtual void Render(matrix parent = XMMatrixIdentity());
	void RenderShadow(matrix parent = XMMatrixIdentity());
	virtual void PushRenderBatch(matrix parent = XMMatrixIdentity());
	void ModifyVertexs(matrix parent = XMMatrixIdentity());
	
	// �ִϸ��̼� ������ ����� �� ������ Local Matrix�� ����ϴ� ���� GPU���� �±��.
	void BlendingAnimation();
	
	// �ִϸ��̼� ������ ����� Local Matrix �� ��Ų�޽��� �������� �� �ֵ��� ToWorldMatrix�� ��ȯ�ϴ� ������ 
	// GPU���� �±��.
	void ModifyLocalToWorld();

	// �� �ΰ��� �Լ��� ���ʷ� �����ϰԲ� Ŀ�ǵ带 �ִ´�.
	void AnimationComputeDispatch(matrix parent = XMMatrixIdentity());

	using RenderFuncType = void (SkinMeshGameObject::*)(matrix parent);
	inline static RenderFuncType CurrentRenderFunc = &SkinMeshGameObject::Render;

	virtual void SetShape(Shape _shape);

	virtual void Update(float delatTime);
	virtual void AnimationUpdate(float deltaTime);

	void CollectSkinMeshObject(matrix parent);
	inline static vector<SkinMeshGameObject*> collection;

	virtual void MoveChunck(const matrix& afterMat, const GameObjectIncludeChunks& beforeChunckInc, const GameObjectIncludeChunks& afterChunkInc);

	virtual void Release();

#pragma pack(push, 1)
	struct STC_SyncObjData {
		Tag tag;
		int shapeindex;
		int parent;
		int childs;
		int sibling;
		matrix DestWorld;
		vec4 LVelocity;
		float AnimationFlowTime;
		int PlayingAnimationIndex;
	};
#pragma pack(pop)

	virtual void RecvSTC_SyncObj(char* data);
	static void STATICINIT(int typeindex = GameObjectType::_SkinMeshGameObject);
#undef STC_CurrentStruct
};

/*
* ���� : �Ѿ��� �߻�� ������ ǥ���ϴ� ����ü
*/
struct BulletRay {
	// ������ �߻�ǰ� ������������ �ð�
	static constexpr float remainTime = 0.2f;
	
	// ������ ǥ���ϴ� �޽�
	static Mesh* mesh;

	// ������ ������
	vec4 start;
	// ������ ����
	vec4 direction;
	// ���� Ÿ�̸�
	float t;
	// ������ ����
	float distance;
	// �е�
	float extra[2];

	BulletRay();

	BulletRay(vec4 s, vec4 dir, float dist);

	/*
	* ����/��ȯ : BulletRay::mesh�� �������� �� ������ �Ǵ� worldMatrix�� ��ȯ�Ѵ�.
	*/
	matrix GetRayMatrix();

	/*
	* ���� : ������ Ÿ���� ������Ʈ
	*/ 
	void Update();

	/*
	* ������ ������ �Ѵ�.
	*/
	void Render();
};

enum ItemType {
	_NULL = 0, // �������� �ƴ�.
	_Consumable = 1, // �Һ������
	_Weapon = 2, // ����
	_Equipment = 3, // ���
	_Material = 4, // �ٸ� �������� ����� ���
	_Quest = 5, // ����Ʈ�� ���� Ư���� ������. �ŷ� �Ұ�.
	_Extra = 5, // ��Ÿ ������.
};

/*
* ���� : ������ ����
* Sentinal Value :
* NULL =
*/
struct Item
{
	// ������ id
	int id;
	// �������� ����
	ItemType itemtype;
	// ������ ����
	vec4 color;
	// �κ��丮�� ��Ÿ�� �޽�/��
	Shape ShapeInInventory;
	// Mesh�� �׸��� ������ �ؽ��� - ���̸� ����.
	GPUResource* tex;
	// Mesh�� �׸��� ������ �ؽ���
	GPUResource* icon;
	// ������ ����
	const wchar_t* description;

	Item(int i, ItemType type, vec4 c, Shape s, GPUResource* t, GPUResource* icontex, const wchar_t* d) :
		id{ i }, itemtype{type}, color { c }, ShapeInInventory{ s }, tex{ t }, icon{ icontex }, description{ d }
	{

	}
};

// �������� ���� ������ ��� �����۵��� ���̺�
extern vector<Item> ItemTable;

enum LightType {
	LT_SpotLight = 0,
	LT_DirectionLight = 1,
	LT_PointLight = 2,
};

struct Light {
	// ����Ʈ�� ��ġ
	union {
		vec4 pos;
		struct {
			float posx;
			float posy;
			float posz;
			int MaxLightCount;
		};
	};

	// ����Ʈ�� ����
	vec4 dir;

	// ����Ʈ�� ����
	LightType lightType;

	// ����Ʈ ����Ʈ�� ���, �󸶳� �а� �������̳ĸ� ������.
	float spot_angle;

	// ����Ʈ�� �ݿ��Ǵ� �Ÿ��� ��Ÿ��
	float range;

	// ����Ʈ�� �󸶳� ������
	float intencity;

	// ���� ����
	vec4 LightColor;

	Light(){}
	~Light(){}

	// �ʱ�ȭ�Լ�
	void GenerateLight();
	BoundingOrientedBox GetOBB();
};

// �� �������� ������. �ִ� 32���� ����Ʈ ���� ����.
struct ChunckLightData {
	Light lights[32];
};
// �� ûũ�� 2048 ����Ʈ�� GPU ���ҽ� �Ҵ�.
// �ױ��� - 1.5km * 2km * height2 chuck = 5MB ������ �Ҹ�.
// Map���� GPUResource�� ����.
// ��ü ���� AABB�� ���� �ε����� ������.


/*
* ���� : ���������� ������ ������Ʈ
* Sentinal Value : 
* NULL = (isExist == false)
*/
class Hierarchy_Object : public GameObject {
public:
	//�ڽ� ������Ʈ�� ����
	int childCount = 0;
	//�ڽ� ������Ʈ���� �迭
	vector<Hierarchy_Object*> childs;

	Hierarchy_Object() {
		childCount = 0;
	}
	~Hierarchy_Object() {}

	/*
	* ���� : parent_world �� ��ȯ�� ��������, �������� ������Ʈ�� �ڽŰ� �ڽ��� ��� �����Ѵ�.
	* ���� ����������Ʈ���� ��� ������ �� �� ���δ�.
	* *���� ���õ� Game::renderViewPort���� <�������� �������� �ø�>�� ����Ͽ� 
	* �׸��� �ʴ� ��ü�� ���ܽ�Ų��.
	* �Ű����� : 
	* matrix parent_world : ��µ� ���
	* ShaderType sre : � ������� �������� �����Ұ��� �����Ѵ�.
	*/
	void Render_Inherit(matrix parent_world, ShaderType sre = ShaderType::RenderWithShadow);

	/*
	* ���� : parent_world �� ��ȯ�� ��������, �������� ������Ʈ�� �ڽŰ� �ڽ��� ��� �����Ѵ�.
	* ���� ����������Ʈ���� ��� ������ �� �� ���δ�. 
	* ���� ���õ� Game::renderViewPort���� <�������� �������� �ø�>�� ����Ͽ� 
	* �׸��� �ʴ� ��ü�� ���ܽ�Ų��.
	* �Ű����� : 
	* matrix parent_world : ��µ� ���
	* ShaderType sre : � ������� �������� �����Ұ��� �����Ѵ�.
	*/
	void Render_Inherit_CullingOrtho(matrix parent_world, ShaderType sre = ShaderType::RenderWithShadow);

	/*
	* ���� :
	* Hierarchy_Object�� Shape(Model/Mesh)�� OBB�� worldMat ��ȯ�� ������ ��ȯ�Ѵ�.
	* �Ű����� :
	* matrix worldMat : OBB�� ��ȯ�� ���庯ȯ���
	* ��ȯ :
	* if ShapeIndex == -1 (����� ���� ���) >>> Extents.x == -1 �� OBB (OBB���� NULL��)
	* if (����� �޽��� ���) >>> MeshOBB�� worldMat�� ��ȯ�� OBB
	* if (����� ���� ���) >>> ModelOBB�� worldMat�� ��ȯ�� OBB
	*/
	BoundingOrientedBox GetOBBw(matrix worldMat);
};

/*
* ���� : ûũ.
* �������� OBB�� �����ϰ� �ִ�.
* Sentinal Value :
* NULL = (obbs.size() == 0)
*/
struct GameChunk {
	vecset<StaticGameObject*> Static_gameobjects;
	vecset<DynamicGameObject*> Dynamic_gameobjects;
	vecset<SkinMeshGameObject*> SkinMesh_gameobjects;
	
	// ������=
	vector<Light*> Lights; // �ش� ûũ�� ������ ��ġ�� ��� ������

	int StaticLightCount = 0; // ûũ�� ������ ��� �������� ����
	GPUResource GameChunckRefLightArr; // ûũ�� ������ ��� �������� Structured Buffer�� ��� ���� ��.

	ChunkIndex cindex;
	BoundingBox AABB;
	UINT TourID = 0;

	GameChunk() {
		ZeroMemory(&Static_gameobjects, sizeof(vecset<StaticGameObject*>));
		ZeroMemory(&Dynamic_gameobjects, sizeof(vecset<DynamicGameObject*>));
		ZeroMemory(&SkinMesh_gameobjects, sizeof(vecset<SkinMeshGameObject*>));
		Static_gameobjects.Init(32);
		Dynamic_gameobjects.Init(32);
		SkinMesh_gameobjects.Init(32);
	}
	void SetChunkIndex(ChunkIndex ci);
	void Release();
};

/*
* ���� : ChunckIndex�� �ؽ�
* x, y, z�� �� ��Ʈ�� ���ư��鼭 �����ϴ� ���.
* pdep�� �̿��� �װ��� ������ �����Ѵ�.
*/
template<>
struct hash<ChunkIndex> {
	size_t operator()(const ChunkIndex& p) const noexcept {
		size_t h = _pdep_u64((size_t)p.x, 0x9249249249249249);
		h |= _pdep_u64((size_t)p.y, 0x2492492492492492);
		h |= _pdep_u64((size_t)p.z, 0x4924924924924924);
		return h;
	}
};

/*
* ���� : ���� �� ������
*/
struct GameMap {
	GameMap() {}
	~GameMap() {}
	vector<string> name;
	vector<Mesh*> meshes;
	vector<Model*> models;
	vector<StaticGameObject*> MapObjects;
	vec4 AABB[2] = { 0, 0 };
	int ZoneID = 0;
	void ExtendMapAABB(BoundingOrientedBox obb);

	int StartShapeIndex = 0;
	int StartDesc_Init = 0;
	int StartDesc_Texture = 0;
	int StartDesc_Material = 0;
	int StartDesc_Instancing = 0;
	int LastDesc_Init = 0;
	int LastDesc_Texture = 0;
	int LastDesc_Material = 0;
	int LastDesc_Instancing = 0;
	void GetStartDescIndexs();
	void GetLastDescIndexs();

	/*ui64 pdep_src2[48] = {};
	int pdepcnt = 0;
	ui64 GetSpaceHash(int x, int y, int z);*/

	struct Posindex {
		int x;
		int y;
		int z;
	};
	/*ui64 inv_pdep_src2[48] = {};
	int inv_pdepcnt = 0;
	Posindex GetInvSpaceHash(ui64 hash);*/

	//static collision..
	void BakeStaticCollision();

	unsigned int TextureTableStart = 0;
	unsigned int MaterialTableStart = 0;

	void LoadMap(const char* MapName, int ZoneID);

	void Release();
};

struct SphereLODObject : public DynamicGameObject {
	Mesh* MeshNear;
	Mesh* MeshFar;
	float SwitchDistance;

	vec4 FixedPos;

	virtual void Update(float deltaTime);
	virtual void Render(matrix parent = XMMatrixIdentity());
};

/*
* ���� : ���� Ÿ�� enum
*/
enum class WeaponType { MachineGun, Sniper, Shotgun, Rifle, Pistol, Max };

/*
* ���� : ���� Ÿ�� ����ü
*/
struct WeaponData {
	WeaponType type;
	float shootDelay;     // ���� �ӵ�
	float recoilVelocity; // �ݵ� ����
	float recoilDelay;    // �ݵ� ȸ�� �ð�
	float damage;         // �⺻ ������
	int maxBullets;       // źâ �뷮
	float reloadTime;     // ���� �ð�
};

static WeaponData GWeaponTable[] = {
	{ WeaponType::MachineGun, 0.1f, 12.0f, 0.2f, 10.0f, 100, 4.0f },
	{ WeaponType::Sniper, 1.5f, 10.0f, 1.0f, 100.0f, 5, 2.0f },
	{ WeaponType::Shotgun, 0.7f, 7.0f, 0.6f, 12.0f, 8, 3.0f },
	{ WeaponType::Rifle, 0.12f, 10.0f, 0.3f, 15.0f, 30, 2.5f },
	{ WeaponType::Pistol, 0.4f, 5.0f, 0.2f, 15.0f, 12, 1.5f },
	// 
};

class Weapon {
public:
	WeaponData m_info;      // GWeaponTable���� ������ ��ġ
	float m_shootFlow = 0;  // ���� �߻���� ���� �ð� ���
	float m_recoilFlow = 0; // �ݵ� �ִϸ��̼�/���� ��� �����

	Weapon(){}

	Weapon(WeaponType type) : m_info(GWeaponTable[(int)type]) {
		m_shootFlow = m_info.shootDelay;
		m_recoilFlow = m_info.recoilDelay;
	}

	virtual void Update(float deltaTime) {
		if (m_shootFlow < m_info.shootDelay) m_shootFlow += deltaTime;
		if (m_recoilFlow < m_info.recoilDelay) m_recoilFlow += deltaTime;
	}

	virtual void OnFire() {
		m_shootFlow = 0.0f;
		m_recoilFlow = 0.0f;
	}

	/*
	* ���� 
	* ���� �ݵ��� �󸶳� ����Ǿ����� 0~1 ���� ������ ��ȯ
	*/
	float GetRecoilAlpha() const {
		float alpha = 1.0f - (m_recoilFlow / m_info.recoilDelay);
		return (alpha < 0) ? 0 : alpha;
	}
};

/*
* ���� : ���� ���� Ŭ����
*/
class Monster : public SkinMeshGameObject {
#define STC_CurrentStruct Monster
	STC_STATICINIT_innerStruct;
public:
	enum class State {
		IDLE,
		WALK,
		RUN,
		ATTACK,
		DEATH,
	};

	State m_currentState = State::IDLE;

	void ChangeState(State newState);

	// ���� HP
	STCDef(float, HP);// = 30;
	// ���� �ִ� ä��
	STCDef(float, MaxHP);// = 30;
	// ���� ��� ����
	STCDef(bool, isDead);

	// ���� HP ���� ���° HP ������. (������ �Ҷ� �߿�.)
	int HPBarIndex;
	// HP �ٸ� ǥ���ϴ� ���� ���
	matrix HPMatrix;

	Monster();
	virtual ~Monster() {}

	/*
	* ���� : ���ӿ�����Ʈ�� ������Ʈ�� ������.
	* �Ű����� :
	* float deltaTime : ���� ������Ʈ ���� ���� ��������� �ð� ����.
	*/
	virtual void Update(float deltaTime);

	/*
	* ���� : ���ӿ�����Ʈ�� �������Ѵ�.
	*/
	virtual void Render(matrix parent = XMMatrixIdentity());

	virtual void Release();

	/*
	* ���͸� �ʱ�ȭ �Ѵ�.
	* const XMMATRIX& initialWorldMatrix : �ʱ� ���� ���
	*/
	void Init(const XMMATRIX& initialWorldMatrix);

#pragma pack(push, 1)
	struct STC_SyncObjData {
		Tag tag;
		int shapeindex;
		int parent;
		int childs;
		int sibling;
		matrix DestWorld;
		vec4 LVelocity;
		float AnimationFlowTime;
		int PlayingAnimationIndex;

		//STC ü��
		float HP = 30;
		//STC �ִ�ü��
		float MaxHP = 30;
		//STC ���� �׾����� ����
		bool isDead = false;
	};
#pragma pack(pop)

	virtual void RecvSTC_SyncObj(char* data);
	static void STATICINIT(int typeindex = GameObjectType::_Monster);
#undef STC_CurrentStruct
};

/*
* ���� : �÷��̾� ���� ������Ʈ ����ü
*/
class Player : public SkinMeshGameObject {
public:
	enum class State {
		IDLE,
		WALK,
		RUN,
		JUMP,
		SHOOT,
		RELOAD,
	};

	State m_currentState = State::IDLE;

	void ChangeState(State newState);

	enum class UpperState {
		NONE,
		AIM,
		SHOOT,
	};

	UpperState m_currentUpperState = UpperState::NONE;
	void ChangeUpperState(UpperState newState);

#define STC_CurrentStruct Player
	STC_STATICINIT_innerStruct

	//STC HP
	STCDef(float, HP);
	//STC �ִ�HP
	STCDef(float, MaxHP);// = 100;
	//STC ������ź����
	STCDef(int, bullets);
	//STC ���͸� ų�� ī��Ʈ (�ӽ� ����)
	STCDef(int, KillCount);// = 0;
	//STC ���� ī��Ʈ
	STCDef(int, DeathCount);// = 0;
	//STC �̱״Ͻ� ���� ������
	STCDef(float, HeatGauge);// = 0;
	//STC �ִ� ���� ������
	STCDef(float, MaxHeatGauge);// = 100;
	//STC player job
	STCDef(int, m_currentJob);
	//STC skill cooldown duration by slot
	STCDefArr(float, SkillCooldown, (int)SkillSlot::Max);
	//STC skill cooldown remaining by slot
	STCDefArr(float, SkillCooldownFlow, (int)SkillSlot::Max);
	//STC ���� ���� Ÿ��
	STCDef(int, m_currentWeaponType);// = 0;
	//STC �κ��丮 ������
	//static constexpr int maxItem = 36;
	//STCDefArr(ItemStack, Inventory, maxItem);
	//STC ���� ��� �ִ� ����
	Weapon weapon;

	//ClientOnly ���콺�� �󸶳� ������������ ��Ÿ����.
	vec4 DeltaMousePos;
	//ClientOnly
	//Mesh* Gun;
	//ClientOnly ���� ��� �ִ� ���� ��
	::Model* GunModel;
	//ClientOnly �������� ��Ÿ���� �޽�
	Mesh* ShootPointMesh;
	//ClientOnly 3��Ī�ϴ� ���� ���� ���
	matrix gunMatrix_thirdPersonView;
	//ClientOnly 1��Ī�϶� ���� ���� ���
	matrix gunMatrix_firstPersonView;
	//ClientOnly HPBar�� ǥ���ϴ� Mesh
	Mesh* HPBarMesh;
	//ClientOnly HPBar�� ǥ���ϴ� ���
	matrix HPMatrix;
	//ClientOnly HeatBar�� ǥ���ϴ� Mesh
	Mesh* HeatBarMesh;
	//ClientOnly HeatBar�� ǥ���ϴ� ���
	matrix HeatBarMatrix;
	//ClientOnly
	std::vector<int> gunBarrelNodeIndices;
	float gunBarrelAngle;
	float gunBarrelSpeed;
	bool  m_isZooming = false;
	float m_currentFov = 60.0f;  // ���� FOV
	float m_targetFov = 60.0f;   // ��ǥ FOV

	//CTS
	float m_yaw = 0.0f;
	float m_pitch = 0.0f;

	Player();
	virtual ~Player();

	/*
	* ���� : ���ӿ�����Ʈ�� ������Ʈ�� ������.
	* �Ű����� :
	* float deltaTime : ���� ������Ʈ ���� ���� ��������� �ð� ����.
	*/
	virtual void Update(float deltaTime);
	void ClientUpdate(float deltaTime);

	/*
	* ���� :
	* <�޽��� ���>
	* WorldMatrix�� Root Param 1�� 0~16�� Set�ϰ�,
	* Material�� �ؽ��Ĵ� Root Param 3�� DescTable�� Set�ϰ�,
	* Material�� Constant Buffer ���� Root Param 4�� DescTable�� Set�ϰ�,
	* �޽��� �����
	* <���� ���>
	* //fix <ó�� ����>
	*/
	virtual void Render(matrix parent = XMMatrixIdentity()) override;

	/*
	* ���� : DepthClear �ǰ� �������Ǵ� ��ҵ��� ������
	* ���� DepthClear�� �Ǹ� ���� UI, �׻� �տ� �����߸� �ϴ� �͵��� ������ �Ѵ�.
	*/
	void Render_AfterDepthClear();
	void Render_ThirdPersonWeapon();

	virtual void Release();

	// idk
	void UpdateGunBarrelNodes();

#pragma pack(push, 1)
	struct STC_SyncObjData {
		Tag tag;
		int shapeindex;
		int parent;
		int childs;
		int sibling;
		matrix DestWorld;
		vec4 LVelocity;
		float AnimationFlowTime;
		int PlayingAnimationIndex;

		//STC HP
		float HP;
		//STC �ִ�HP
		float MaxHP = 100;
		//STC ������ź����
		int bullets = 100;
		//STC ���͸� ų�� ī��Ʈ (�ӽ� ����)
		int KillCount = 0;
		//STC ���� ī��Ʈ
		int DeathCount = 0;
		//STC ���� ������
		float HeatGauge = 0;
		//STC �ִ� ���� ������
		float MaxHeatGauge = 100;
		//STC player job
		int m_currentJob = (int)PlayerJob::Healer;
		//STC skill cooldown duration by slot
		float SkillCooldown[(int)SkillSlot::Max] = {};
		//STC skill cooldown remaining by slot
		float SkillCooldownFlow[(int)SkillSlot::Max] = {};
		//STC ���� ���� Ÿ��?
		int m_currentWeaponType = 0;
		////STC �÷��̾��� �κ��丮 ����
		//static constexpr int maxItem = 36;
		//ItemStack Inventory[maxItem];
		//STC ����ִ� ����
		Weapon weapon;
	};
#pragma pack(pop)
	
	virtual void RecvSTC_SyncObj(char* data);
	static void STATICINIT(int typeindex = GameObjectType::_Player);
#undef STC_CurrentStruct
};

struct Portal : public StaticGameObject {
	float spawnX = 0;
	float spawnY = 0;
	float spawnZ = 0;
	float radius = 1.0f;
	int zoneId = 0;
	int dstzoneId = 0;

	STC_STATICINIT_innerStruct;

#pragma pack(push, 1)
	struct STC_SyncObjData {
		Tag tag;
		int shapeindex;
		int parent;
		int childs;
		int sibling;
		matrix DestWorld;
		float spawnX;
		float spawnY;
		float spawnZ;
		float radius;
		int zoneId;
		int dstzoneId;
	};
#pragma pack(pop)

	Portal() {
		tag[GameObjectTag::Tag_Enable] = false;
	}

	virtual void RecvSTC_SyncObj(char* data) override;
	virtual void Render(matrix parent) override {
		//// shape�� ��ȿ�� ���� ����
		//Mesh* mesh = nullptr;
		//Model* model = nullptr;
		//shape.GetRealShape(mesh, model);
		//if (mesh == nullptr && model == nullptr) return;
		//GameObject::Render(parent);
	}
};

#pragma region UIDef
// �ִϸ��̼� �ϴ� ��ƿ Ŭ����
class AnimOperUtil {
public:
	AnimOperUtil() {

	}

	virtual ~AnimOperUtil() {

	}

	//slow->fast
	static __forceinline float EaseIn(const float& trate, const float& power) {
		float f = powf(trate, power);
		return f;
	}

	//fast->slow
	static __forceinline float EaseOut(const float& trate, const float& power) {
		return (1 - powf(1 - trate, power));
	}

	//slow->fast->slow
	static __forceinline float EaseIO(const float& trate, const float& power) {
		if (trate < 0.5) {
			return 0.5f * EaseIn(trate * 2, power);
		}
		else {
			return 0.5f + 0.5f * EaseOut(2 * (trate - 0.5f), power);
		}
	}
};

// DXEvent�� ��¥�� �������� ��������.
/*
* �׸��� ���⿡ ���� ���콺 ��ġ, ���� ���콺 ����, 
* ���� Ű �Է� ���µ��� ��� ������ �� �־�� ��. (�����̶�..)
*/
struct DXEvent {
	HWND hWnd;
	UINT uMsg;
	WPARAM wParam;
	LPARAM lParam;

	DXEvent(){}
	~DXEvent(){}
};

enum DXUI_TYPE {
	DXUI_NULL = 0,
	DXUI_Btn = 1,
	DXUI_Edit = 2,
	DXUI_Text = 3,
	DXUI_Slider = 4,
	DXUI_Window = 5,
	DXUI_Slot = 6,
};

struct DXUI {	
	DXUI_TYPE type = DXUI_TYPE::DXUI_NULL;
	int ParameterData_Capacity = 0;

	// ��� �ִ��Ŀ� ���� ��� ��ǥ�� ���.
	// �����ǥ�� �������� ��ġ�� ���� �޶�����, game.CurrentUICenter�� ��ǥ��ŭ �̵��� ��ǥ�� �ȴ�.
	// ������ loc�� �߽����� ���ϸ� �ȴ�.
	vec4 location = 0;
	
	void* pParamterData = nullptr;
	bool isFocus = false;
	bool enable = false;
	float depth = 0;

	void(*RenderFunc)(DXUI*) = nullptr;
	void(*UpdateFunc)(DXUI*, float) = nullptr;
	void(*EventFunc)(DXUI*) = nullptr;

	DXUI(DXUI_TYPE t, int PCapacity, vec4 loc, float d, void* pPData = nullptr);
	void SetFunctions(void(*rf)(DXUI*), void(*uf)(DXUI*, float), void(*ef)(DXUI*)) {
		RenderFunc = rf;
		UpdateFunc = uf;
		EventFunc = ef;
	}
};

struct DXBtnParam {
	// ���������� ������ ���� ����
	float flow;
	float maxtime;
	int Base_UITextureIndex = 0;

	// ��ư�� ������ �ִ� �ؽ�Ʈ
	wchar_t text[64] = {};

	// �߰� ����
	union {
		float addtionalParams[16];
		void* addtionalPtr[8];
		int addtionalParams_int[16];
	};
	

	void Set(float f, float max, int texid, const wchar_t* uitext) {
		flow = f;
		maxtime = max;
		Base_UITextureIndex = texid;
		wcscpy_s(text, 64, uitext);
		ZeroMemory(addtionalParams, sizeof(float) * 16);
	}
};

struct DXEditParam {
	// ���������� ������ ���� ����
	float flow;
	float maxtime;
	int Base_UITextureIndex = 0;
	// ������ ���Ϲ����� �����ϴ� ���� / 0 - ���� / 1 - ���� / 2 - �Ǽ�
	int ReturnMode;

	// �ƹ��͵� ������ ���� ������ ������ �ؽ�Ʈ
	wchar_t text[64] = {};

	// �� �ؽ�Ʈ
	wstring wstr;
	int editCursor = 0;

	// �߰� ����
	union {
		float addtionalParams[16];
		void* addtionalPtr[8];
		int addtionalParams_int[16];
	};

	void Set(float f, float max, int rtmode, int texid, const wchar_t* cleartext) {
		ZeroMemory(this, sizeof(DXEditParam));
		flow = f;
		maxtime = max;
		ReturnMode = rtmode;
		Base_UITextureIndex = texid;
		wcscpy_s(text, 64, cleartext);
		wstr.reserve(64);
		wstr.clear();
	}
};

struct DXSliderParam {
	// �����̴� ����
	// �ּҰ�
	float min = 0;
	// �ִ밪
	float max = 1;
	// ���� ����
	float setter = 0;
	//�߽��� �ؽ���
	int Base_UITextureIndex;
	// �ǽð����� ���� ����� �ּ�
	void* obj = 0;

	// ������ ���Ϲ����� �����ϴ� ���� / n : int / f : float /
	char mod = 'n';

	// �����̴��� ���ι��������� ����
	bool horizontal = true;

	// ũ������ ������ ���������� �� (������ �����ʰ� ���� +)
	bool inverse_direction = false;
	
	// �����̴��� ���� ���� ���� �����ִ� ����
	char ShowValueMode = 0; // f : front, b : back, \0 : null, q : present-front, p : present-back

	//�߽����� �������� �ϴ� �����̴� ���� �� ��ġ�� ǥ���ϴ� ����
	vec4 NotchLoc;

	// �߰� ����
	union {
		float addtionalParams[16];
		void* addtionalPtr[8];
		int addtionalParams_int[16];
	};

	void Set(bool ishori, char valuemode, bool inv_dir, float minv, float maxv, char showValueMode, void* object, int Notch_texid, vec4 notchLoc) {
		ZeroMemory(this, sizeof(DXSliderParam));
		horizontal = ishori;
		mod = valuemode;
		inverse_direction = inv_dir;
		min = minv;
		max = maxv;
		if (object == nullptr) {
			obj = new float;
		}
		else {
			obj = object;
		}
		ShowValueMode = showValueMode;
		Base_UITextureIndex = Notch_texid;
		NotchLoc = notchLoc;
	}
};

enum SlotKind {
	_Item = 0,
	_Skill = 1,
};

struct SlotData {
	int objid = 0;
	int itemCnt = 0;
	DXUI* selectedSlot = nullptr;
};

// ������ �۵� ���
/*
* 1. ���� Ŭ��
*		- �ƹ��͵� ��������� : ��� ����
*		- ���� ������ �������� ���� ���Կ� ���� ��� : ������ ���� ��ġ��
*		- ���ƾ� �� ���Կ� �̹� �ٸ� �������� ���� ��� : ������ ���Կ� �ִ� ������ �ٽ� ��´�.
*			- ������ ���� ���� ������ �Ϻθ� ��� ���� ��� : �ƹ� ���۵� ���Ѵ�. (��ũ�� �ٸ���.)
* 2. ������ Ŭ��
*		- �ƹ��͵� ���������� : ���ݸ� ����
*		- �̹� ������ ��������, �� ���Կ� ������ Ŭ�� : �ϳ��� ����
*		- ���� ������ �������� ���� ���Կ� ���� ��� : ������ ������ �ϳ��� ����
*		- �ٸ� ������ �������� ���� ���Կ� ���� ��� : swap
*			- ������ ���� ���� ������ �Ϻθ� ��� ���� ��� : �ƹ� ���۵� ���Ѵ�. (��ũ�� �ٸ���.)
* 3. �� : ������ ���� ����
* 
* ����� ������ ����� ���� �ʴ´�.
* ���⸸ ������ ����� �Ѵ�.
* ���⿡�� 
* 1. ������ ���� ��ġ��
* 2. ������ ���� �ϳ��� ����
* 2. ������ ���Կ� �ִ� �����۰� swap
* 3. �� ������ ������ ��ü ����
* 4. �� ������ ������ �ϳ��� ����
* 
* �۵������ ����ũ����Ʈ�� ���� �۵� ����� �����Դ�.
* �⺻������ Ŭ���̾�Ʈ���� ���¸� �ٲٰ�, �������� �ֱ������� ������ ���־�� �Ѵ�.
*/

struct DXSlotParam {
	float flow;
	float maxtime;
	SlotKind slotObType;
	int Base_UITextureIndex = 0; // ���� �ؽ���
	int objid; // ���Կ� ����� � ���� ����Ű�� id.
	int itemCount = 0; // ���Կ� �������� ����Ǿ��� ���, �� �������� ����

	// �߰� ����
	union {
		float addtionalParams[16];
		void* addtionalPtr[8];
		int addtionalParams_int[16];
	};

	void Set(float f, float m, SlotKind type, int texid) {
		flow = f;
		maxtime = m;
		slotObType = type;
		Base_UITextureIndex = texid;
	}
};

struct DXPage {
	vec4 location;
	vec4 BackGroundColor = vec4(0, 0, 0, 0.5f);
	
	float depth_min = 0;
	float depth_max = 0;
	int depthlevel_Count = 0;
	float GetDepth(int level) {
		float rate = (float)level / (float)depthlevel_Count;
		rate = clamp<float>(rate, 0, 1);
		return depth_min + (depth_max - depth_min) * rate;
	}
	void AlignUIDepth();

	vector<DXUI*> temp_WindowArr;
	vector<vec4> temp_LocStack;
	vector<DXUI*> uiArr;

	void Render();

	void Update(float deltaTime) {
		for (int i = 0; i < uiArr.size(); ++i) {
			DXUI* ui = uiArr[i];
			if (ui->enable == false) continue;
			if (ui->UpdateFunc) ui->UpdateFunc(ui, deltaTime);
		}
	}

	void Event() {
		for (int i = 0; i < uiArr.size(); ++i) {
			DXUI* ui = uiArr[i];
			if (ui->enable == false) continue;
			if (ui->EventFunc) ui->EventFunc(ui);
		}
	}

	DXUI* GetSlotUIFromPos(vec4 pos);
};

struct DXWindowParam {
	vector<DXPage*> page_stack; // PageStack ������ 0�̸� �����찡 ����.
	vector<DXPage*> page_table;
	DXUI* origin = nullptr;
	bool Focus_Move = false;
	int WindowImageIndex = 0;
	vec4 MovePivot = 0;
	chrono::system_clock::time_point LastFocusedTime;

	float depth_min = 0;
	float depth_max = 0;
	int depthlevel_Count = 0;
	float GetDepth(int level) {
		float rate = (float)level / (float)depthlevel_Count;
		rate = clamp<float>(rate, 0, 1);
		return depth_min + (depth_max - depth_min) * rate;
	}
	void AlignUIDepth();

	// �߰� ����
	union {
		float addtionalParams[16];
		void* addtionalPtr[8];
		int addtionalParams_int[16];
	};

	DXWindowParam(){}
	~DXWindowParam(){}

	// ������ ������ UI���� loc�� ��ǥ�� �����ǥ�� �⺻ ������ ��ǥ�� �ƾ�ٸ���.
	void NormalizeCoordToWindowCoord_vec4(vec4& loc);

	void Set(DXUI* o, int texid) {
		ZeroMemory(this, sizeof(DXWindowParam));
		origin = o;
		WindowImageIndex = texid;
	}

	//pos�� window�� �߽����� 0, 0 ���� �ϴ� �����ǥ�̴�.
	// �ش� ��� ��ǥ�� �������� ������ ��ȸ�Ѵ�.
	DXUI* GetSlotUIFromPos(vec4 pos);
};

#pragma endregion
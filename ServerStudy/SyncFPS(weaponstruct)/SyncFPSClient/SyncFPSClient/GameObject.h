#pragma once
#include "stdafx.h"
#include "Render.h"
#include "Game.h"
#include "main.h"

class Mesh;
class Shader;
struct GPUResource;

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
	ui64 FlagPtr = 0;

	Shape() : FlagPtr{ 0 } {}
	Shape(Mesh* mesh) {
		SetMesh(mesh);
	}
	Shape(Model* model) {
		SetModel(model);
	}

	/*
	* 설명/반환 : Shape가 Mesh인지 여부를 반환
	*/
	__forceinline bool isMesh() {
		return FlagPtr & 0x8000000000000000;
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

	// 이름에서 ShapeIndex를 얻는 map
	static unordered_map<string, int> StrToShapeIndex;
	static vector<Shape> ShapeTable;
	static vector<string> ShapeStrTable;

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

		operator bool() const {
			return t != nullptr && index >= 0 && index < 32 &&
				(t->tag & (UINT(1) << index)) != 0;
		}

		void operator=(bool b) {
			if (t == nullptr || index < 0 || index >= 32) return;
			if (b) {
				t->tag |= (UINT(1) << index);
			}
			else {
				t->tag &= ~(UINT(1) << index);
			}
		}
	};

	bool Test(UINT MaskIndex) const {
		return MaskIndex < 32 && (tag & (UINT(1) << MaskIndex)) != 0;
	}

	// OBB.Center
	TagSetter operator[](UINT MaskIndex) {
		return TagSetter{ this, static_cast<int>(MaskIndex) };
	}
};

enum GameObjectTag {
	Tag_Enable = 0, // 게임오브젝트 활성화 여부
	Tag_Dynamic = 1, // 게임오브젝트가 움직일 수 있는지 여부
	// 이름에서 ShapeIndex를 얻는 map
	Tag_SkinMeshObject = 2,
	// 이름에서 ShapeIndex를 얻는 map
	Tag_Player = 3,
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
	// 이름에서 ShapeIndex를 얻는 map
	UINT TourID = 0;

	/*
	* 게임오브젝트를 구분하고 탐색하기 위한 tag. 32개의 tag를 보유할 수 있다.
	* 항상 tag의 첫번째 비트는 enable이다. (게임오브젝트가 활성화 되어있는지 여부)
	*/
	STCDef(Tag, tag);

	// appearance
	Shape shape;
	union {
		int* material = nullptr; // mesh 일 경우에만 활성화됨. slotNum만큼. game.MaterialTable에서 접근.
		matrix* transforms_innerModel; // model 일 경우만 활성화됨. nodeCount 만큼.
	};

	// 이름에서 ShapeIndex를 얻는 map
	GameObject* parent = nullptr;
	GameObject* childs = nullptr;
	GameObject* sibling = nullptr;
	union {
		char extra[8];
		float* RaytracingWorldMatInput = nullptr;
		float** RaytracingWorldMatInput_Model; // float*의 2차원 배열의 형태임.
	// 이름에서 ShapeIndex를 얻는 map
	// 이름에서 ShapeIndex를 얻는 map
	};

	// transform
	int zoneId = 0;
	matrix worldMat;

	// 이름에서 ShapeIndex를 얻는 map
	// 이름에서 ShapeIndex를 얻는 map
	GameObject();
	virtual ~GameObject();

	virtual matrix GetWorld();
	virtual void SetWorld(matrix localWorldMat);

	// 이름에서 ShapeIndex를 얻는 map
	virtual void Render(matrix parent = XMMatrixIdentity());
	virtual void PushRenderBatch(matrix parent = XMMatrixIdentity());
	// 이름에서 ShapeIndex를 얻는 map
	using RenderFuncType = void (GameObject::*)(matrix parent);
	inline static RenderFuncType CurrentRenderFunc = &GameObject::Render;

	virtual void Release();
	virtual BoundingOrientedBox GetOBB();

	virtual void SetShape(Shape _shape, int ZoneID = -1);
	virtual void SetShape(Model* _shape, int ZoneID = -1);
	virtual void SetShape(Mesh* _shape, int ZoneID = -1);

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

	// 이름에서 ShapeIndex를 얻는 map
	virtual void RecvSTC_SyncObj(char* data);
	static void STATICINIT(int typeindex = GameObjectType::_GameObject);

#undef STC_CurrentStruct
};

struct StaticGameObject : public GameObject {
	StaticGameObject();
	virtual ~StaticGameObject();
	vector<BoundingOrientedBox> obbArr;
	int zoneid = -1;

	virtual matrix GetWorld();
	virtual void SetWorld(matrix localWorldMat);

	virtual void Render(matrix parent = XMMatrixIdentity());
	virtual void PushRenderBatch(matrix parent = XMMatrixIdentity());
	// 이름에서 ShapeIndex를 얻는 map
	using RenderFuncType = void (StaticGameObject::*)(matrix parent);
	inline static RenderFuncType CurrentRenderFunc = &StaticGameObject::Render;

	virtual void Release();
	virtual BoundingOrientedBox GetOBB();

	bool Collision_Inherit(matrix parent_world, BoundingBox bb);
	void InitMapAABB_Inherit(void* origin, matrix parent_world);
	BoundingOrientedBox GetOBBw(matrix worldMat);

	virtual void SetShape(Shape _shape, int ZoneID = -1);

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

	// 이름에서 ShapeIndex를 얻는 map
	__forceinline void RecvSTC_SyncObj(char* data);
	static void STATICINIT(int typeindex = GameObjectType::_StaticGameObject);
};

/*
* 설명 : 청크를 찾아가기 위한 인덱스
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
	int zoneid = -1;

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

	virtual void SetShape(Shape _shape, int ZoneID = -1);

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

// 이름에서 ShapeIndex를 얻는 map
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
	float HitFlashTimer = 0.0f;
	float HitFlashDuration = 0.18f;
	vec4 StatusTint = vec4(0.0f, 1.0f, 1.0f, 1.0f);

	__forceinline void TriggerHitFlash(float duration = 0.18f) {
		HitFlashDuration = duration;
		HitFlashTimer = duration;
	}

	__forceinline float GetHitFlashRate() const {
		if (HitFlashDuration <= 0.0f) return 0.0f;
		float rate = HitFlashTimer / HitFlashDuration;
		if (rate < 0.0f) return 0.0f;
		if (rate > 1.0f) return 1.0f;
		return rate;
	}

	__forceinline void SetStatusTint(vec4 tint) {
		StatusTint = tint;
	}

	__forceinline vec4 GetRenderTint() const {
		float hitFlashRate = GetHitFlashRate();
		if (hitFlashRate > 0.0f) return vec4(hitFlashRate * 1.35f, 1.0f, 0.0f, 0.0f);
		return StatusTint;
	}

// 이름에서 ShapeIndex를 얻는 map
	GPUResource NodeWorldMatrixReadBack;
// 이름에서 ShapeIndex를 얻는 map
	vector<int> GetWorldMat_NodeIndexArr;
// 이름에서 ShapeIndex를 얻는 map
	matrix* Mapped_NodeWorldMatrixReadBack;

#pragma region GPUAnimation
	struct AnimationBlendingCBStruct
	{
// 이름에서 ShapeIndex를 얻는 map
		float animTime[4];

// 이름에서 ShapeIndex를 얻는 map
		float MAXTime[4];

// 이름에서 ShapeIndex를 얻는 map
		float animWeight[4];

// 이름에서 ShapeIndex를 얻는 map
		ui64 animMask[4];

// 이름에서 ShapeIndex를 얻는 map
		float frameRate;
		float upperBodyYawCorrection;
		float upperBodyPitchCorrection;
	};

// 이름에서 ShapeIndex를 얻는 map
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
	AnimationBlendingCBStruct* AnimBlendingCB_Mapped = nullptr;   // [crash-fix] must start null; uninitialized garbage passed the (cbData != nullptr) guard and crashed in BlendingAnimation on zone re-entry
	DescIndex AnimBlendingCBVDescIndex;
#pragma endregion

// 이름에서 ShapeIndex를 얻는 map
// 이름에서 ShapeIndex를 얻는 map
	DescIndex* OutVertexUAV;

// 이름에서 ShapeIndex를 얻는 map
// 이름에서 ShapeIndex를 얻는 map
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

// 이름에서 ShapeIndex를 얻는 map
	void BlendingAnimation();

	virtual void ModifyLocalToWorld();

// 이름에서 ShapeIndex를 얻는 map
	void AnimationComputeDispatch(matrix parent = XMMatrixIdentity());

	using RenderFuncType = void (SkinMeshGameObject::*)(matrix parent);
	inline static RenderFuncType CurrentRenderFunc = &SkinMeshGameObject::Render;

	virtual void SetShape(Shape _shape, int ZoneID = -1);

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
* 설명 : 총알이 발사된 광선을 표현하는 구조체
*/
struct BulletRay {
		// 이름에서 ShapeIndex를 얻는 map
	static constexpr float remainTime = 0.2f;

		// 이름에서 ShapeIndex를 얻는 map
	static Mesh* mesh;

	// 이름에서 ShapeIndex를 얻는 map
	vec4 start;
	// 이름에서 ShapeIndex를 얻는 map
	vec4 direction;
	// 이름에서 ShapeIndex를 얻는 map
	float t;
	// 이름에서 ShapeIndex를 얻는 map
	float distance;
	// 이름에서 ShapeIndex를 얻는 map
	float extra[2];

	BulletRay();

	BulletRay(vec4 s, vec4 dir, float dist);

	/*
	* 설명/반환 : BulletRay::mesh가 렌더링될 때 가지게 되는 worldMatrix를 반환한다.
	*/
	matrix GetRayMatrix();

	/*
* 설명 : 총알이 발사된 광선을 표현하는 구조체
	*/
	void Update();

	/*
	* 광선을 렌더링 한다.
	*/
	void Render();
};

enum ItemType {
	_NULL = 0, // 아이템이 아님.
	_Consumable = 1, // 소비아이템
	_Weapon = 2, // 무기
	_Equipment = 3, // 장비
	_Material = 4, // 다른 아이템을 만드는 재료
	_Quest = 5, // 퀘스트에 쓰일 특별한 아이템. 거래 불가.
	_Extra = 5, // 기타 아이템.
};

/*
* 설명 : 총알이 발사된 광선을 표현하는 구조체
* Sentinal Value :
* NULL =
*/
struct Item
{
	// 이름에서 ShapeIndex를 얻는 map
	int id;
	// 이름에서 ShapeIndex를 얻는 map
	ItemType itemtype;
	// 이름에서 ShapeIndex를 얻는 map
	vec4 color;
	// 이름에서 ShapeIndex를 얻는 map
	Shape ShapeInInventory;
	// 이름에서 ShapeIndex를 얻는 map
	GPUResource* tex;
	// 이름에서 ShapeIndex를 얻는 map
	GPUResource* icon;
	// 이름에서 ShapeIndex를 얻는 map
	const wchar_t* description;

	// ������ ������
	void* pItemData = nullptr;

	Item(int i, ItemType type, vec4 c, Shape s, GPUResource* t, GPUResource* icontex, const wchar_t* d, void* ptrData = nullptr) :
		id{ i }, itemtype{type}, color { c }, ShapeInInventory{ s }, tex{ t }, icon{ icontex }, description{ d }
	{
		pItemData = ptrData;
	}
};

	// 이름에서 ShapeIndex를 얻는 map
extern vector<Item> ItemTable;

enum LightType {
	LT_SpotLight = 0,
	LT_DirectionLight = 1,
	LT_PointLight = 2,
};

struct Light {
	// 이름에서 ShapeIndex를 얻는 map
	union {
		vec4 pos;
		struct {
			float posx;
			float posy;
			float posz;
			int MaxLightCount;
		};
	};

	// 이름에서 ShapeIndex를 얻는 map
	vec4 dir;

	// 이름에서 ShapeIndex를 얻는 map
	LightType lightType;

	// 이름에서 ShapeIndex를 얻는 map
	float spot_angle;

	// 이름에서 ShapeIndex를 얻는 map
	float range;

	// 이름에서 ShapeIndex를 얻는 map
	float intencity;

	// 이름에서 ShapeIndex를 얻는 map
	vec4 LightColor;

	Light(){}
	~Light(){}

	// 이름에서 ShapeIndex를 얻는 map
	void GenerateLight();
	BoundingOrientedBox GetOBB();
};

	// 이름에서 ShapeIndex를 얻는 map
struct ChunckLightData {
	Light lights[32];
};
	// 이름에서 ShapeIndex를 얻는 map
	// 이름에서 ShapeIndex를 얻는 map
	// 이름에서 ShapeIndex를 얻는 map
	// 이름에서 ShapeIndex를 얻는 map


/*
* 설명 : 아이템 정보
* Sentinal Value :
* NULL = (isExist == false)
*/
class Hierarchy_Object : public GameObject {
public:
	// 이름에서 ShapeIndex를 얻는 map
	int childCount = 0;
	// 이름에서 ShapeIndex를 얻는 map
	vector<Hierarchy_Object*> childs;

	Hierarchy_Object() {
		childCount = 0;
	}
	~Hierarchy_Object() {}

	/*
	* 설명 : parent_world 로 변환을 수행한후, 계층구조 오브젝트의 자신과 자식을 모두 렌더한다.
	* 계층 구조오브젝트들을 모두 렌더링 할 때 쓰인다.
	* *현재 선택된 Game::renderViewPort에서 <원근투영 프러스텀 컬링>을 사용하여
	* 그리지 않는 물체를 제외시킨다.
* 설명 : 계층구조를 가지는 오브젝트
	* matrix parent_world : 계승될 행렬
	* ShaderType sre : 어떤 방식으로 렌더링을 진행할건지 선택한다.
	*/
	void Render_Inherit(matrix parent_world, ShaderType sre = ShaderType::RenderWithShadow);

	/*
	* 설명 : parent_world 로 변환을 수행한후, 계층구조 오브젝트의 자신과 자식을 모두 렌더한다.
	* 계층 구조오브젝트들을 모두 렌더링 할 때 쓰인다.
	* 현재 선택된 Game::renderViewPort에서 <직교투영 프러스텀 컬링>을 사용하여
	* 그리지 않는 물체를 제외시킨다.
* 설명 : 계층구조를 가지는 오브젝트
	* matrix parent_world : 계승될 행렬
	* ShaderType sre : 어떤 방식으로 렌더링을 진행할건지 선택한다.
	*/
	void Render_Inherit_CullingOrtho(matrix parent_world, ShaderType sre = ShaderType::RenderWithShadow);

	/*
* 설명 : 계층구조를 가지는 오브젝트
	* Hierarchy_Object의 Shape(Model/Mesh)의 OBB에 worldMat 변환을 적용해 반환한다.
* 설명 : 계층구조를 가지는 오브젝트
	* matrix worldMat : OBB를 변환할 월드변환행렬
* 설명 : 계층구조를 가지는 오브젝트
	* if ShapeIndex == -1 (모양이 없을 경우) >>> Extents.x == -1 인 OBB (OBB계의 NULL임)
	* if (모양이 메쉬인 경우) >>> MeshOBB를 worldMat로 변환한 OBB
	* if (모양이 모델인 경우) >>> ModelOBB를 worldMat로 변환한 OBB
	*/
	BoundingOrientedBox GetOBBw(matrix worldMat);
};

/*
* 설명 : 청크.
* 여러개의 OBB를 소유하고 있다.
* Sentinal Value :
* NULL = (obbs.size() == 0)
*/
struct GameChunk {
	vecset<StaticGameObject*> Static_gameobjects;
	vecset<DynamicGameObject*> Dynamic_gameobjects;
	vecset<SkinMeshGameObject*> SkinMesh_gameobjects;

	// 이름에서 ShapeIndex를 얻는 map
	vector<Light*> Lights; // 해당 청크에 범위가 걸치는 모든 라이팅
	int StaticLightCount = 0; // 청크에 참조된 모든 라이팅의 개수

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
	void Release_Asset();
};

/*
* 설명 : ChunckIndex의 해쉬
* x, y, z의 각 비트를 돌아가면서 적제하는 방식.
* pdep를 이용해 그것을 빠르게 구현한다.
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
* 설명 : 계층구조를 가지는 오브젝트
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

	bool LoadMap(const char* MapName, int ZoneID);

	enum LoadMap_Context_WorkKind {
		LMCWK_
	};
	struct LoadMap_Context {
		ifstream* ifs;
		ifstream* ifs2;
		LoadMap_Context_WorkKind workKind;
		int work_iterator;
	};
	bool LoadMap_WithContext(const char* MapName, int ZoneID, LoadMap_Context* context);

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
* 설명 : 게임 맵 데이터
*/

/*
* 설명 : 무기 타입 구조체
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

	// 이름에서 ShapeIndex를 얻는 map
	STCDef(float, HP);// = 30;
	// 이름에서 ShapeIndex를 얻는 map
	STCDef(float, MaxHP);// = 30;
	STCDef(float, Defense);// = 0;
	// 이름에서 ShapeIndex를 얻는 map
	STCDef(bool, isDead);

	// 이름에서 ShapeIndex를 얻는 map
	int HPBarIndex;
	// 이름에서 ShapeIndex를 얻는 map
	matrix HPMatrix;

	Monster();
	virtual ~Monster() {}

	/*
	* 설명 : 게임오브젝트의 업데이트를 실행함.
* 설명 : 무기 타입 구조체
	* float deltaTime : 이전 업데이트 실행 부터 현재까지의 시간 차이.
	*/
	virtual void Update(float deltaTime);

	/*
	* 설명 : 게임오브젝트를 렌더링한다.
	*/
	virtual void Render(matrix parent = XMMatrixIdentity());

	virtual void Release();

	/*
	* 몬스터를 초기화 한다.
	* const XMMATRIX& initialWorldMatrix : 초기 월드 행렬
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

	// 이름에서 ShapeIndex를 얻는 map
		float HP = 30;
	// 이름에서 ShapeIndex를 얻는 map
		float MaxHP = 30;
	// 이름에서 ShapeIndex를 얻는 map
		bool isDead = false;
		float Defense = 0;
	};
#pragma pack(pop)

	virtual void RecvSTC_SyncObj(char* data);
	static void SyncHP(GameObject* go, char* data, int len);
	static void STATICINIT(int typeindex = GameObjectType::_Monster);
#undef STC_CurrentStruct
};

enum QuestType {
	CollectItem,
	KillMonster
};

struct QuestRequirement {
	QuestType type;
	int ObjID = -1; // ������ �����̸� �������� ���̵�, ���� óġ�� ������ ���̵�
	int Cnt = 0; // �ؾ��ϴ� ��ǥ�� ����
	int PresentCnt = 0; // ���� ����� ����
	int PastCnt = 0; // ������ ����� ���� - ���� ���̿� ���.

	QuestRequirement() {}
	QuestRequirement(QuestType t, int objid, int count) {
		type = t;
		ObjID = objid;
		Cnt = count;
		PresentCnt = 0;
		PastCnt = 0;
	}

	void Copy(QuestRequirement* dest) {
		dest->type = type;
		dest->ObjID = ObjID;
		dest->Cnt = Cnt;
		dest->PresentCnt = PresentCnt;
		dest->PastCnt = PastCnt;
	}

	bool isComplete() {
		return PresentCnt >= Cnt;
	}
};

enum QuestRewardType {
	QRT_Money = 0,
	QRT_Item = 1,
	QRT_Exp = 2,
};

struct QuestReward {
	QuestRewardType type;
	int objid = 0;
	int count = 0;

	QuestReward() {}
	QuestReward(QuestRewardType t, int obj, int cnt) {
		type = t;
		objid = obj;
		count = cnt;
	}

	void Copy(QuestReward* dest) {
		dest->type = type;
		dest->objid = objid;
		dest->count = count;
	}
};

struct Quest {
	const wchar_t* QuestName;
	const wchar_t* QuestDesc;
	static constexpr int MaxRequire = 16;
	int requp = 0;
	QuestRequirement ReqArr[MaxRequire];

	static constexpr int MaxReward = 16;
	int rewardUp = 0;
	QuestReward RewardArr[MaxReward];

	Quest() {}
	Quest(const wchar_t* qName, const wchar_t* qDesc) {
		QuestName = qName;
		QuestDesc = qDesc;
	}

	void PushReq(QuestType type, int objid, int Cnt) {
		if (requp < MaxRequire) {
			ReqArr[requp] = QuestRequirement(type, objid, Cnt);
			requp += 1;
		}
	}

	void PushReward(QuestRewardType t, int obj, int cnt) {
		if (rewardUp < MaxReward) {
			RewardArr[rewardUp] = QuestReward(t, obj, cnt);
			rewardUp += 1;
		}
	}

	void Copy(Quest* dest) {
		dest->QuestName = QuestName;
		dest->QuestDesc = QuestDesc;
		dest->requp = requp;
		for (int i = 0; i < dest->requp; ++i) {
			ReqArr[i].Copy(&dest->ReqArr[i]);
		}
		for (int i = 0; i < dest->rewardUp; ++i) {
			RewardArr[i].Copy(&dest->RewardArr[i]);
		}
	}

	bool isSameQuest(Quest* q) {
		bool b = q->QuestName == QuestName;
		b = b && q->QuestDesc == QuestDesc;
		b = b && q->requp == requp;
		b = b && q->rewardUp == rewardUp;
		return b;
	}

	bool isComplete() {
		bool b = true;
		for (int i = 0; i < requp; ++i) {
			b = b && ReqArr[i].isComplete();
		}
		return b;
	}
};

/*
* 설명 : 간이 몬스터 클래스
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

	enum class JumpPhase {
		NONE,
		TAKEOFF,
		AIRBORNE,
		LANDING,
	};

	JumpPhase m_jumpPhase = JumpPhase::NONE;
	float m_jumpElapsed = 0.0f;
	float m_jumpGroundStableTime = 0.0f;
	float m_jumpReentryCooldown = 0.0f;
	bool m_jumpSawDescending = false;

	enum class UpperState {
		NONE,
		AIM,
		SHOOT,
		RELOAD,
	};

	UpperState m_currentUpperState = UpperState::AIM;
	float m_upperShootHoldTimer = 0.0f;
	float m_upperAimPoseTime = 0.0f;
	float m_dualBladeVisualTimer = 0.0f;
	float m_dualBladeAttackVisualTimer = 0.0f;
	bool m_dualBladeAttackAlternate = false;
	float m_dualLeftRecoilTimer = 0.0f;
	float m_dualRightRecoilTimer = 0.0f;
	float m_droneAssaultVisualTimer = 0.0f;
	float m_droneFlightVisualTimer = 0.0f;
	void ChangeUpperState(UpperState newState, bool restart = false);
	void TriggerUpperShoot();
	void TriggerDualPistolRecoil(bool leftHand);

#define STC_CurrentStruct Player
	STC_STATICINIT_innerStruct

	//STC HP
	STCDef(float, HP);
	// 이름에서 ShapeIndex를 얻는 map
	STCDef(float, MaxHP);// = 100;
	STCDef(float, Attack); // = 0
	STCDef(float, Defense);// = 0;
	// 이름에서 ShapeIndex를 얻는 map
	STCDef(int, bullets);
	// 이름에서 ShapeIndex를 얻는 map
	STCDef(int, KillCount);// = 0;
	// 이름에서 ShapeIndex를 얻는 map
	STCDef(int, DeathCount);// = 0;
	// 이름에서 ShapeIndex를 얻는 map
	STCDef(float, HeatGauge);// = 0;
	// 이름에서 ShapeIndex를 얻는 map
	STCDef(float, MaxHeatGauge);// = 100;
	//STC aegis shield durability
	STCDef(float, ShieldDurability);
	//STC aegis max shield durability
	STCDef(float, MaxShieldDurability);
	//STC player job
	STCDef(int, m_currentJob);
	//STC skill cooldown duration by slot
	STCDefArr(float, SkillCooldown, (int)SkillSlot::Max);
	//STC skill cooldown remaining by slot
	STCDefArr(float, SkillCooldownFlow, (int)SkillSlot::Max);
		// 이름에서 ShapeIndex를 얻는 map
	STCDef(int, m_currentWeaponType);// = 0;
	STCDef(bool, m_weaponHolstered);
	STCDef(float, ReloadRemain);
	STCDef(bool, m_sniperDmrMode);
	STCDef(bool, m_bomberHealAmmoMode);
	STCDef(bool, isGround);
		// 이름에서 ShapeIndex를 얻는 map
	//static constexpr int maxItem = 36;
	//STCDefArr(ItemStack, Inventory, maxItem);

	//STC ���� ��� �ִ� ����
	Weapon weapon[3];
	int SelectedWeapon = 0;

	//STC �÷��̾��� ��
	STCDef(int, Gold);

	//STC �÷��̾��� ����ġ
	STCDef(int, Exp);

	//STC �÷��̾��� ����
	STCDef(int, Level);
	
	//ClientOnly ���콺�� �󸶳� ������������ ��Ÿ����.
	vec4 DeltaMousePos;
	//ClientOnly
	//Mesh* Gun;
		// 이름에서 ShapeIndex를 얻는 map
	::Model* GunModel;
		// 이름에서 ShapeIndex를 얻는 map
	Mesh* ShootPointMesh;
		// 이름에서 ShapeIndex를 얻는 map
	matrix gunMatrix_thirdPersonView;
	matrix cachedThirdPersonRightHandMatrix;
	bool cachedThirdPersonRightHandMatrixValid = false;
	matrix cachedThirdPersonLeftHandMatrix;
	bool cachedThirdPersonLeftHandMatrixValid = false;
	int cachedRightHandForwardAxisIndex = -1;
	int cachedRightHandRightAxisIndex = -1;
	float cachedRightHandForwardAxisSign = 1.0f;
	float cachedRightHandRightAxisSign = 1.0f;
	matrix cachedThirdPersonWeaponMatrix;
	bool cachedThirdPersonWeaponMatrixValid = false;
	matrix cachedThirdPersonLeftWeaponMatrix;
	bool cachedThirdPersonLeftWeaponMatrixValid = false;
	int cachedThirdPersonWeaponType = -1;
		// 이름에서 ShapeIndex를 얻는 map
	matrix gunMatrix_firstPersonView;
		// 이름에서 ShapeIndex를 얻는 map
	Mesh* HPBarMesh;
		// 이름에서 ShapeIndex를 얻는 map
	matrix HPMatrix;
		// 이름에서 ShapeIndex를 얻는 map
	Mesh* HeatBarMesh;
		// 이름에서 ShapeIndex를 얻는 map
	matrix HeatBarMatrix;
	//ClientOnly
	std::vector<int> gunBarrelNodeIndices;
	float gunBarrelAngle;
	float gunBarrelSpeed;
	bool  m_isZooming = false;
	float m_currentFov = 60.0f;  // 현재 FOV
	float m_targetFov = 60.0f;   // 목표 FOV

	//CTS
	float m_yaw = 0.0f;
	float m_pitch = 0.0f;

	Player();
	virtual ~Player();

	/*
	* float deltaTime : 이전 업데이트부터 현재까지의 시간 차이.
	*/
	virtual void Update(float deltaTime);
	void ClientUpdate(float deltaTime);

	/*
	* 월드 행렬과 머티리얼 정보를 설정하고 플레이어 모델을 렌더링한다.
	*/
	virtual void Render(matrix parent = XMMatrixIdentity()) override;

	/*
	* 깊이 버퍼를 비운 뒤 1인칭 무기와 UI용 오브젝트를 렌더링한다.
	*/
	void Render_AfterDepthClear();
	void Render_ThirdPersonWeapon();
	void UpdateThirdPersonWeaponAttachmentCache();
	void ClearThirdPersonWeaponVisuals();

	virtual void Release();

	// idk
	void UpdateGunBarrelNodes();

	virtual void ModifyLocalToWorld();

	static constexpr int MaxWeapon = (int)WeaponType::Max;
	GameObject* PlayerWeaponObj[MaxWeapon] = {};
	GameObject* LeftHand = {};
	GameObject* DronObj[2] = {};
	GameObject* Knife[2] = {};
	void PlayerWeaponObjectInit();

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
	// 이름에서 ShapeIndex를 얻는 map
		float MaxHP = 100;
	// 이름에서 ShapeIndex를 얻는 map
		float Attack = 0;
		float Defense = 0;
		int bullets = 100;
	// 이름에서 ShapeIndex를 얻는 map
		int KillCount = 0;
	// 이름에서 ShapeIndex를 얻는 map
		int DeathCount = 0;
	// 이름에서 ShapeIndex를 얻는 map
		float HeatGauge = 0;
	// 이름에서 ShapeIndex를 얻는 map
		float MaxHeatGauge = 100;

		float ShieldDurability = 0;
		float MaxShieldDurability = 100;
		//STC player job
		int m_currentJob = (int)PlayerJob::Healer;
		//STC skill cooldown duration by slot
		float SkillCooldown[(int)SkillSlot::Max] = {};
		//STC skill cooldown remaining by slot
		float SkillCooldownFlow[(int)SkillSlot::Max] = {};
		int m_currentWeaponType = 0;
		bool m_weaponHolstered = false;
		float ReloadRemain = 0.0f;
		bool m_sniperDmrMode = false;
		bool m_bomberHealAmmoMode = false;
		bool isGround = false;
		float m_yaw = 0.0f;
		float m_pitch = 0.0f;
		Weapon weapon[3];
		int SelectedWeapon = 0;
		int Gold = 0;
		int Exp = 0;
		int Level = 0;
	};
#pragma pack(pop)

	virtual void RecvSTC_SyncObj(char* data);
	static void STATICINIT(int typeindex = GameObjectType::_Player);
#undef STC_CurrentStruct
};

class PeacefulNPC : public SkinMeshGameObject {
#define STC_CurrentStruct PeacefulNPC
	STC_STATICINIT_innerStruct;
public:
	// �� Ŭ���̾�Ʈ���� �� NPC�� ���̴��� ����
	STCDef(PeacefulNPCType, NPCType);
	STCDef(int, NPCID);

	PeacefulNPC();
	virtual ~PeacefulNPC() {}

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

		PeacefulNPCType npctype;
		int NPCID;
	};
#pragma pack(pop)

	virtual void RecvSTC_SyncObj(char* data);
	static void STATICINIT(int typeindex = GameObjectType::_PeacefulNPC);
#undef STC_CurrentStruct
};

struct TalkSelection {
	const wchar_t* volatile selectionText;
	bool gotoOtherTalk = true;
	void (*Function)() = nullptr; // gotoOtherTalk == false �̸�, ���ý� �� �Լ��� �����Ѵ�. (Ŭ�󿡼��� �ƹ��͵� ���Ѵ�.)
	size_t goto_otherTalkID; // gotoOtherTalk == true �̸�, ���ý� �ش� Id�� ��ȭ�� ��ȯ�ȴ�. (Ŭ�󿡼��� �۵��ȴ�.)

	TalkSelection() {
		selectionText = nullptr;
		gotoOtherTalk = true;
		Function = nullptr;
		goto_otherTalkID = -1;
	}
	TalkSelection(const wchar_t* txt, bool gotoOther_talk, ui64 gotoData) {
		selectionText = txt;
		gotoOtherTalk = gotoOther_talk;
		if (gotoOther_talk) {
			goto_otherTalkID = gotoData;
		}
		else {
			Function = reinterpret_cast<void(*)()>(gotoData);
		}
	}
	TalkSelection(const TalkSelection& ref) {
		selectionText = ref.selectionText;
		gotoOtherTalk = ref.gotoOtherTalk;
		goto_otherTalkID = ref.goto_otherTalkID;
		Function = ref.Function;
	}
};

struct NPCTalkData {
	// ����� �ؽ�Ʈ
	const wchar_t* volatile SpeakerName;
	const wchar_t* volatile text;
	int selectCnt = 0;
	bool NextEscape = false;
	TalkSelection sel[4];
	NPCTalkData(const wchar_t* speaker, const wchar_t* txt, bool nxtEsc = false, TalkSelection sel1 = TalkSelection(), TalkSelection sel2 = TalkSelection(), TalkSelection sel3 = TalkSelection(), TalkSelection sel4 = TalkSelection());
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
		// 이름에서 ShapeIndex를 얻는 map
		//Mesh* mesh = nullptr;
		//Model* model = nullptr;
		//shape.GetRealShape(mesh, model);
		//if (mesh == nullptr && model == nullptr) return;
		//GameObject::Render(parent);
	}
};

#pragma region UIDef
		// 이름에서 ShapeIndex를 얻는 map
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

		// 이름에서 ShapeIndex를 얻는 map
/*
* 윈도우 이벤트를 큐에 저장하기 위한 데이터 구조체.
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

		// 이름에서 ShapeIndex를 얻는 map
		// 이름에서 ShapeIndex를 얻는 map
		// 이름에서 ShapeIndex를 얻는 map
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
	float flow;
	float maxtime;
	int Base_UITextureIndex = 0;
	wchar_t text[256] = {};

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
// 이름에서 ShapeIndex를 얻는 map
	float flow;
	float maxtime;
	int Base_UITextureIndex = 0;
// 이름에서 ShapeIndex를 얻는 map
	int ReturnMode;

// 이름에서 ShapeIndex를 얻는 map
	wchar_t text[64] = {};

// 이름에서 ShapeIndex를 얻는 map
	wstring wstr;
	int editCursor = 0;

// 이름에서 ShapeIndex를 얻는 map
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
	// 이름에서 ShapeIndex를 얻는 map
	// 이름에서 ShapeIndex를 얻는 map
	float min = 0;
	// 이름에서 ShapeIndex를 얻는 map
	float max = 1;
	// 이름에서 ShapeIndex를 얻는 map
	float setter = 0;
	// 이름에서 ShapeIndex를 얻는 map
	int Base_UITextureIndex;
	// 이름에서 ShapeIndex를 얻는 map
	void* obj = 0;

	// 이름에서 ShapeIndex를 얻는 map
	char mod = 'n';

	// 이름에서 ShapeIndex를 얻는 map
	bool horizontal = true;

	// 이름에서 ShapeIndex를 얻는 map
	bool inverse_direction = false;

	// 이름에서 ShapeIndex를 얻는 map
	char ShowValueMode = 0; // f : front, b : back, \0 : null, q : present-front, p : present-back

	// 이름에서 ShapeIndex를 얻는 map
	vec4 NotchLoc;

	// 이름에서 ShapeIndex를 얻는 map
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
	_Item_Equipment = 2,
	_Item_Weapone = 4,
	_Item_Consumable = 6,
	// ���� ���� ¦���� ��������

	_Skill = 1,
	//������ ���� Ȧ���� ��ų��.
};

struct SlotData {
	int objid = 0;
	int itemCnt = 0;
	DXUI* selectedSlot = nullptr;
};

/*
*			- 하지만 지금 기존 슬롯의 일부만 잡고 있을 경우 : 아무 동작도 안한다. (마크와 다른점.)
* 2. 오른쪽 클릭
*		- 아무것도 안집었을때 : 절반만 집기
*		- 이미 뭔가를 집었을때, 빈 슬롯에 오른쪽 클릭 : 하나씩 놓기
*		- 같은 종류의 아이템을 가진 슬롯에 놓는 경우 : 아이템 수량을 하나씩 전달
*		- 다른 종류의 아이템을 가진 슬롯에 놓는 경우 : swap
*			- 하지만 지금 기존 슬롯의 일부만 잡고 있을 경우 : 아무 동작도 안한다. (마크와 다른점.)
* 3. 휠 : 아이템 수량 조절
*
* 집기는 서버와 통신을 하지 않는다.
* 놓기만 서버와 통신을 한다.
* 놓기에는
*
* 2. 오른쪽 클릭
* 2. 기존에 슬롯에 있던 아이템과 swap
* 3. 빈 공간에 아이템 전체 놓기
* 4. 빈 공간에 아이템 하나씩 놓기
*
* 작동방식은 마인크래프트의 슬롯 작동 방식을 가져왔다.
* 기본적으로 클라이언트에서 상태를 바꾸고, 서버에서 주기적으로 검증을 해주어야 한다.
*/

struct DXSlotParam {
	float flow;
	float maxtime;
	SlotKind slotObType;
	int Base_UITextureIndex = 0; // 렌더 텍스쳐
	int objid; // 슬롯에 저장된 어떤 것을 가리키는 id.
	int itemCount = 0; // 슬롯에 아이템이 저장되었을 경우, 그 아이템의 개수

	// 이름에서 ShapeIndex를 얻는 map
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
		objid = 0;
		itemCount = 0;
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
	vector<DXPage*> page_stack; // PageStack 개수가 0이면 윈도우가 닫힘.
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

	// 이름에서 ShapeIndex를 얻는 map
	union {
		float addtionalParams[16];
		void* addtionalPtr[8];
		int addtionalParams_int[16];
	};

	DXWindowParam(){}
	~DXWindowParam(){}

	// 이름에서 ShapeIndex를 얻는 map
	void NormalizeCoordToWindowCoord_vec4(vec4& loc);

	void Set(DXUI* o, int texid) {
		ZeroMemory(this, sizeof(DXWindowParam));
		origin = o;
		WindowImageIndex = texid;
	}

	// 이름에서 ShapeIndex를 얻는 map
	// 이름에서 ShapeIndex를 얻는 map
	DXUI* GetSlotUIFromPos(vec4 pos);
};

#pragma endregion

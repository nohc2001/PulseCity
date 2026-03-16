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

	// bool로도 쓸 수 있음.
	TagSetter operator[](UINT MaskIndex) {
		TagSetter ts;
		ts.t = this;
		ts.index = MaskIndex;
		return ts;
	}
};

enum GameObjectTag {
	Tag_Enable = 1, // 게임오브젝트 활성화 여부
	Tag_Dynamic = 2, // 게임오브젝트가 움직일 수 있는지 여부
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
	// 청크를 통해 한 틱에 게임오브젝트 당 한번씩 해야하는 작업이 있다면 이 값을 사용하자.
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

	// 계층구조
	GameObject* parent = nullptr;
	GameObject* childs = nullptr;
	GameObject* sibling = nullptr;
	union {
		char extra[8];
		float** RaytracingWorldMatInput = nullptr;
		float*** RaytracingWorldMatInput_Model;
	};

	// transform
	matrix worldMat;

	//처음 생성 시에는 enable이 false. 업데이트를 하지 않는다. 
	// 어느정도 초기화가 완료되어 게임루프에 들어갈 준비를 마치면 되면 그때 Enable을 한다.
	GameObject();
	virtual ~GameObject();

	virtual matrix GetWorld();
	virtual void SetWorld(matrix localWorldMat);

	// render instance를 이용한 배치처리를 하지 않는 순수한 렌더링시에 사용
	virtual void Render(matrix parent = XMMatrixIdentity());
	virtual void PushRenderBatch(matrix parent = XMMatrixIdentity());
	// 현재 사용하는 렌더함수를 가리킴.
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
	
	// 헤더는 선행처리를 하셈.
	virtual void RecvSTC_SyncObj(char* data);
	static void STATICINIT(int typeindex = GameObjectType::_GameObject);

#undef STC_CurrentStruct
};

struct StaticGameObject : public GameObject {
	StaticGameObject();
	virtual ~StaticGameObject();
	vector<BoundingBox> aabbArr;

	virtual matrix GetWorld();
	virtual void SetWorld(matrix localWorldMat);

	virtual void Render(matrix parent = XMMatrixIdentity());
	virtual void PushRenderBatch(matrix parent = XMMatrixIdentity());
	// 현재 사용하는 렌더함수를 가리킴.
	using RenderFuncType = void (StaticGameObject::*)(matrix parent);
	inline static RenderFuncType CurrentRenderFunc = &StaticGameObject::Render;

	virtual void Release();
	virtual BoundingOrientedBox GetOBB();

	bool Collision_Inherit(matrix parent_world, BoundingBox bb);
	void InitMapAABB_Inherit(void* origin, matrix parent_world);
	BoundingOrientedBox GetOBBw(matrix worldMat);

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

	// 헤더는 선행처리를 하셈.
	__forceinline void RecvSTC_SyncObj(char* data);
	static void STATICINIT(int typeindex = GameObjectType::_StaticGameObject);
};

struct GameObjectIncludeChunks {
	int xmin;
	int ymin;
	int zmin;
	unsigned char xlen;
	unsigned char ylen;
	unsigned char zlen;
	unsigned char extraByte;
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

//skinMesh 의 경우 shader 자체가 다르기 때문에 배치처리를 할 시에 따로 렌더링을 수행해야 한다.
struct SkinMeshGameObject : public DynamicGameObject {
#define STC_CurrentStruct SkinMeshGameObject
	STC_STATICINIT_innerStruct;
	SkinMeshGameObject();
	virtual ~SkinMeshGameObject();

	// temp data. later combine one upload CBV
	vector<matrix*> RootBoneMatrixs_PerSkinMesh;
	// [bone 0] [...] [bone N]
	vector<GPUResource> BoneToWorldMatrixCB;
	vector<HumanoidAnimation*> HumanoidAnimationArr;
	
	// non shader visible desc heap에 위치함. 
	// model->mNumSkinMesh 만큼 존재함.
	DescIndex* OutVertexUAV;

	// 레이트레이싱을 할때 실제로 변형되는 메쉬.
	// model의 skinmeshcount 만큼 존재함.
	RayTracingMesh* modifyMeshes = nullptr;

	float AnimationFlowTime = 0;
	STCDef(float, DestAnimationFlowTime);
	STCDef(int, PlayingAnimationIndex);

	void InitRootBoneMatrixs();
	void SetRootMatrixs();

	void PushHumanoidAnimation(HumanoidAnimation* hanim);
	void GetBoneLocalMatrixAtTime(HumanoidAnimation* hanim, matrix* out, float time);

	virtual void Render(matrix parent = XMMatrixIdentity());
	virtual void PushRenderBatch(matrix parent = XMMatrixIdentity());
	void ModifyVertexs(matrix parent = XMMatrixIdentity());

	using RenderFuncType = void (SkinMeshGameObject::*)(matrix parent);
	inline static RenderFuncType CurrentRenderFunc = &SkinMeshGameObject::Render;

	virtual void SetShape(Shape _shape);

	virtual void Update(float delatTime);

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
	// 광선이 발사되고 사라지기까지의 시간
	static constexpr float remainTime = 0.2f;
	
	// 광선을 표현하는 메쉬
	static Mesh* mesh;

	// 광선의 시작점
	vec4 start;
	// 광선의 방향
	vec4 direction;
	// 광선 타이머
	float t;
	// 광선의 길이
	float distance;
	// 패딩
	float extra[2];

	BulletRay();

	BulletRay(vec4 s, vec4 dir, float dist);

	/*
	* 설명/반환 : BulletRay::mesh가 렌더링될 때 가지게 되는 worldMatrix를 반환한다.
	*/
	matrix GetRayMatrix();

	/*
	* 설명 : 광선의 타임을 업데이트
	*/ 
	void Update();

	/*
	* 광선을 렌더링 한다.
	*/
	void Render();
};

/*
* 설명 : 아이템 정보
* Sentinal Value :
* NULL =
*/
struct Item
{
	// 아이템 id
	int id;
	// 아이템 색상
	vec4 color;
	// 인벤토리에 나타날 메쉬
	Mesh* MeshInInventory;
	// Mesh를 그릴때 입히는 텍스쳐
	GPUResource* tex;
	// 아이템 설명
	const wchar_t* description;

	Item(int i, vec4 c, Mesh* m, GPUResource* t, const wchar_t* d) :
		id{ i }, color{ c }, MeshInInventory{ m }, tex{ t }, description{
		d
		}
	{

	}
};

// 아이템의 원본 정보가 담긴 아이템들의 테이블
extern vector<Item> ItemTable;

enum LightType {
	LT_SpotLight = 0,
	LT_DirectionLight = 1,
	LT_PointLight = 2,
};

struct Light {
	matrix transform;
	LightType lightType;
	float spot_angle;
	float range;
	float intencity;
	void* data;

	void GenerateLight();
};

/*
* 설명 : 계층구조를 가지는 오브젝트
* Sentinal Value : 
* NULL = (isExist == false)
*/
class Hierarchy_Object : public GameObject {
public:
	//자식 오브젝트의 개수
	int childCount = 0;
	//자식 오브젝트들의 배열
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
	* 매개변수 : 
	* matrix parent_world : 계승될 행렬
	* ShaderType sre : 어떤 방식으로 렌더링을 진행할건지 선택한다.
	*/
	void Render_Inherit(matrix parent_world, ShaderType sre = ShaderType::RenderWithShadow);

	/*
	* 설명 : parent_world 로 변환을 수행한후, 계층구조 오브젝트의 자신과 자식을 모두 렌더한다.
	* 계층 구조오브젝트들을 모두 렌더링 할 때 쓰인다. 
	* 현재 선택된 Game::renderViewPort에서 <직교투영 프러스텀 컬링>을 사용하여 
	* 그리지 않는 물체를 제외시킨다.
	* 매개변수 : 
	* matrix parent_world : 계승될 행렬
	* ShaderType sre : 어떤 방식으로 렌더링을 진행할건지 선택한다.
	*/
	void Render_Inherit_CullingOrtho(matrix parent_world, ShaderType sre = ShaderType::RenderWithShadow);

	/*
	* 설명 :
	* Hierarchy_Object의 Shape(Model/Mesh)의 OBB에 worldMat 변환을 적용해 반환한다.
	* 매개변수 :
	* matrix worldMat : OBB를 변환할 월드변환행렬
	* 반환 :
	* if ShapeIndex == -1 (모양이 없을 경우) >>> Extents.x == -1 인 OBB (OBB계의 NULL임)
	* if (모양이 메쉬인 경우) >>> MeshOBB를 worldMat로 변환한 OBB
	* if (모양이 모델인 경우) >>> ModelOBB를 worldMat로 변환한 OBB
	*/
	BoundingOrientedBox GetOBBw(matrix worldMat);
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

	bool operator==(const ChunkIndex& ci) const {
		return (x == ci.x && y == ci.y) && z == ci.z;
	}

	BoundingBox GetAABB();
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

	ChunkIndex cindex;
	BoundingBox AABB;
	UINT TourID = 0;

	GameChunk() {
		Static_gameobjects.Init(32);
		Dynamic_gameobjects.Init(32);
		SkinMesh_gameobjects.Init(32);
	}
	void SetChunkIndex(ChunkIndex ci);
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
* 설명 : 게임 맵 데이터
*/
struct GameMap {
	GameMap() {}
	~GameMap() {}
	vector<string> name;
	vector<Mesh*> meshes;
	vector<Model*> models;
	vector<StaticGameObject*> MapObjects;
	vec4 AABB[2] = { 0, 0 };
	void ExtendMapAABB(BoundingOrientedBox obb);

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

	void LoadMap(const char* MapName);
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
* 설명 : 무기 타입 enum
*/
enum class WeaponType { MachineGun, Sniper, Shotgun, Rifle, Pistol, Max };

/*
* 설명 : 무기 타입 구조체
*/
struct WeaponData {
	WeaponType type;
	float shootDelay;     // 연사 속도
	float recoilVelocity; // 반동 세기
	float recoilDelay;    // 반동 회복 시간
	float damage;         // 기본 데미지
	int maxBullets;       // 탄창 용량
	float reloadTime;     // 장전 시간
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
	WeaponData m_info;      // GWeaponTable에서 가져온 수치
	float m_shootFlow = 0;  // 다음 발사까지 남은 시간 계산
	float m_recoilFlow = 0; // 반동 애니메이션/에임 상승 진행률

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
	* 설명 
	* 현재 반동이 얼마나 진행되었는지 0~1 사이 값으로 반환
	*/
	float GetRecoilAlpha() const {
		float alpha = 1.0f - (m_recoilFlow / m_info.recoilDelay);
		return (alpha < 0) ? 0 : alpha;
	}
};

/*
* 설명 : 간이 몬스터 클래스
*/
class Monster : public SkinMeshGameObject {
#define STC_CurrentStruct Monster
	STC_STATICINIT_innerStruct;
private:
	/*float m_speed = 2.0f;
	float m_patrolRange = 20.0f;
	float m_chaseRange = 10.0f;
	vec4 m_homePos;
	vec4 m_targetPos;

	bool m_isMove = false;
	float m_patrolTimer = 0.0f;
	float m_fireDelay = 1.0f;
	float m_fireTimer = 0.0f;

	int collideCount = 0;
	bool isGround = false;

	float HP = 30;
	const float MAXHP = 30;*/

public:
	// 몬스터 HP
	STCDef(float, HP);// = 30;
	// 몬스터 최대 채력
	STCDef(float, MaxHP);// = 30;
	// 몬스터 사망 여부
	STCDef(bool, isDead);

	// 여러 HP 바중 몇번째 HP 바인지. (렌더링 할때 중요.)
	int HPBarIndex;
	// HP 바를 표현하는 월드 행렬
	matrix HPMatrix;

	Monster() {}
	virtual ~Monster() {}

	/*
	* 설명 : 게임오브젝트의 업데이트를 실행함.
	* 매개변수 :
	* float deltaTime : 이전 업데이트 실행 부터 현재까지의 시간 차이.
	*/
	virtual void Update(float deltaTime);

	/*
	* 설명 : 게임오브젝트를 렌더링한다.
	*/
	virtual void Render(matrix parent = XMMatrixIdentity());

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

		//STC 체력
		float HP = 30;
		//STC 최대체력
		float MaxHP = 30;
		//STC 현재 죽었는지 여부
		bool isDead = false;
	};
#pragma pack(pop)

	virtual void RecvSTC_SyncObj(char* data);
	static void STATICINIT(int typeindex = GameObjectType::_Monster);
#undef STC_CurrentStruct
};

/*
* 설명 : 플래이어 게임 오브젝트 구조체
*/
class Player : public SkinMeshGameObject {
public:
#define STC_CurrentStruct Player
	inline static vector<STCMemberInfo> g_members;

	//STC HP
	STCDef(float, HP);
	//STC 최대HP
	STCDef(float, MaxHP);// = 100;
	//STC 현재총탄개수
	STCDef(int, bullets);
	//STC 몬스터를 킬한 카운트 (임시 점수)
	STCDef(int, KillCount);// = 0;
	//STC 죽음 카운트
	STCDef(int, DeathCount);// = 0;
	//STC 이그니스 열기 게이지
	STCDef(float, HeatGauge);// = 0;
	//STC 최대 열기 게이지
	STCDef(float, MaxHeatGauge);// = 100;
	//STC 스킬 쿨타임
	STCDef(float, HealSkillCooldown);// = 10.0f;
	//STC 쿨타임 타이머
	STCDef(float, HealSkillCooldownFlow);// = 0.0f;
	//STC 현재 무기 타입
	STCDef(int, m_currentWeaponType);// = 0;
	//STC 인벤토리 데이터
	static constexpr int maxItem = 36;
	STCDefArr(ItemStack, Inventory, maxItem);
	//STC 현재 들고 있는 무기
	Weapon weapon;

	//ClientOnly 마우스가 얼마나 움직였는지를 나타낸다.
	vec4 DeltaMousePos;
	//ClientOnly
	//Mesh* Gun;
	//ClientOnly 현재 들고 있는 총의 모델
	::Model* GunModel;
	//ClientOnly 조준점을 나타내는 메쉬
	Mesh* ShootPointMesh;
	//ClientOnly 3인칭일대 총의 월드 행렬
	matrix gunMatrix_thirdPersonView;
	//ClientOnly 1인칭일때 총의 월드 행렬
	matrix gunMatrix_firstPersonView;
	//ClientOnly HPBar를 표현하는 Mesh
	Mesh* HPBarMesh;
	//ClientOnly HPBar를 표현하는 행렬
	matrix HPMatrix;
	//ClientOnly HeatBar를 표현하는 Mesh
	Mesh* HeatBarMesh;
	//ClientOnly HeatBar를 표현하는 행렬
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
	* 설명 : 게임오브젝트의 업데이트를 실행함.
	* 매개변수 :
	* float deltaTime : 이전 업데이트 실행 부터 현재까지의 시간 차이.
	*/
	virtual void Update(float deltaTime);
	void ClientUpdate(float deltaTime);

	/*
	* 설명 :
	* <메쉬일 경우>
	* WorldMatrix를 Root Param 1에 0~16에 Set하고,
	* Material의 텍스쳐는 Root Param 3에 DescTable로 Set하고,
	* Material의 Constant Buffer 값은 Root Param 4에 DescTable로 Set하고,
	* 메쉬를 사용해
	* <모델일 경우>
	* //fix <처리 안함>
	*/
	virtual void Render();

	/*
	* 설명 : DepthClear 되고 렌더링되는 요소들을 렌더링
	* 보통 DepthClear가 되면 이후 UI, 항상 앞에 보여야만 하는 것들을 렌더링 한다.
	*/
	void Render_AfterDepthClear();

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
		//STC 최대HP
		float MaxHP = 100;
		//STC 현재총탄개수
		int bullets = 100;
		//STC 몬스터를 킬한 카운트 (임시 점수)
		int KillCount = 0;
		//STC 죽음 카운트
		int DeathCount = 0;
		//STC 열기 게이지
		float HeatGauge = 0;
		//STC 최대 열기 게이지
		float MaxHeatGauge = 100;
		//STC 스킬 쿨타임
		float HealSkillCooldown = 10.0f;
		//STC 쿨타임 타이머
		float HealSkillCooldownFlow = 0.0f;
		//STC 현재 무기 타입?
		int m_currentWeaponType = 0;
		//STC 플레이어의 인벤토리 정보
		static constexpr int maxItem = 36;
		ItemStack Inventory[maxItem];
		//STC 들고있는 무기
		Weapon weapon;
	};
#pragma pack(pop)
	
	virtual void RecvSTC_SyncObj(char* data);
	static void STATICINIT(int typeindex = GameObjectType::_Player);
#undef STC_CurrentStruct
};
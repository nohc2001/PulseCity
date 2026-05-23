#pragma once
#include "stdafx.h"
struct Zone;

/*
* 설명 : Mesh의 충돌처리 OBB 정보를 저장.
* Sentinal Value :
* NULL = (MAXpos.x < 0 || MAXpos.y < 0 || MAXpos.z < 0);
*/
struct Mesh {
	// OBB.Center
	vec4 Center;
	// OBB.Extends
	vec4 MAXpos;
	// submeshCount
	int subMeshNum = 0;

	/*
	* 설명 : obj 파일로 부터 Mesh의 OBB 데이터를 읽어온다.
	* <현재 UnitScale이 100인 fbx에서 편집된 결과를 기준으로 Mesh 데이터를 불러온다.>
	* 때문에 현재 [100.0f = 1m 인 Mesh]만을 올바르게 불러올 수 있는 상태이다.
	* !!!조치가 필요하다.!!!
	* 매개변수 :
	* const char* path : obj 파일의 경로
	* bool centering : OBB의 Center가 (0, 0, 0)이 되도록 데이터를 조정.
	*/
	void ReadMeshFromFile_OBJ(const char* path, bool centering = true);

	/*
	* 설명/반환 : Mesh의 OBB 충돌데이터를 반환한다.
	*/
	BoundingOrientedBox GetOBB();

	/*
	* 설명 : 가로 width, 높이 height, 세로 depth인 직육면체 벽 Mesh 정보를 생성한다.
	* 매개변수 :
	* float width : 너비
	* float height : 높이
	* float depth : 폭
	*/
	void CreateWallMesh(float width, float height, float depth);

	/*
	* 설명 : AABB 데이터를 통해 Mesh의 OBB 데이터를 구성한다.
	* XMFLOAT3 minpos : AABB의 최소지점
	* XMFLOAT3 maxpos : AABB의 최대지점
	*/
	void SetOBBDataWithAABB(XMFLOAT3 minpos, XMFLOAT3 maxpos);

	/*
	* 설명 : OBB 데이터를 통해 AABB 데이터를 구성한다.
	* OBB가 자유롭게 회전하여도 그것을 모두 포함하는 최소의 AABB를 얻게 한다.
	* vec4* out : 계산중이었던 AABB가 들어오고, AABB가 내보내질 공간. vec4[2] 만큼의 공간이 할당되어 있어야한다.
	* BoundingOrientedBox obb : AABB로 변환될 OBB.
	* bool first :
	*	true이면, AABB를 처음으로 계산하는 것이다. 그래서 obb를 AABB로 바꾸는 과정을 순수히 수행한다.
	*	false이면, 기존 out에 든 AABB 데이터와 obb 영역이 모두 포함되도록 하는 최소 AABB를 다시 쓴다.
	*	이런 기능은 여러 obb를 포함시키는 하나의 최소 AABB를 구하는데 쓰인다.
	*/
	static void GetAABBFromOBB(vec4* out, BoundingOrientedBox obb, bool first = false);
};

/*
	* 매개변수 :
* Sentinal Value :
* NULL = (parent == nullptr && numChildren == 0 && Childrens == nullptr && numMesh == 0 && Meshes == nullptr)
*/
struct ModelNode {
	// OBB.Center
	XMMATRIX transform;
	// OBB.Center
	ModelNode* parent;
	// OBB.Center
	unsigned int numChildren;
	// OBB.Center
	ModelNode** Childrens;
	// OBB.Center
	unsigned int numMesh;
	// OBB.Center
	// OBB.Center
	unsigned int* Meshes;

	// OBB.Center
	int* materialIndex;

	vector<BoundingBox> aabbArr;

	ModelNode() {
		parent = nullptr;
		numChildren = 0;
		Childrens = nullptr;
		numMesh = 0;
		Meshes = nullptr;
	}

	/*
	* 설명 : 모델 노드가 기본상태일때,
	* 해당 모델 노드의 자신과 모든 자식을 포함시키는 AABB를 구성하여
	* origin 모델의 AABB를 확장시킨다.
	* 매개변수 :
	* void* origin : Model의 인스턴스로, 해당 ModelNode를 소유한 원본 Model의 void*
	* const matrix& parentMat : 부모의 기본 trasform으로 부터 변환된 행렬
	*/
	void BakeAABB(void* origin, const matrix& parentMat);

	void PushOBBs(void* origin, const matrix& parentMat, vector<BoundingOrientedBox>* obbArr, void* gameobj);
};

/*
	* 매개변수 :
* Sentinal Value :
* NULL = (nodeCount == 0 && RootNode == nullptr && Nodes == nullptr && mNumMeshes == 0 && mMeshes == nullptr)
*/
struct Model {
	// OBB.Center
	// OBB.Center
	vec4 OBB_Tr;
	// OBB.Center
	vec4 OBB_Ext;
	// OBB.Center
	std::string mName;
	// OBB.Center
	int nodeCount = 0;
	// OBB.Center
	ModelNode* RootNode;

	// OBB.Center
	static vector<ModelNode*> NodeArr;
	static unordered_map<void*, int> nodeindexmap;

	// OBB.Center
	ModelNode* Nodes;

	// OBB.Center
	unsigned int mNumMeshes;

	// OBB.Center
	Mesh** mMeshes;

	// OBB.Center
	float UnitScaleFactor = 1.0f;

	// OBB.Center
	vec4 AABB[2];

	/*
	* 설명 : MyModelExporter에서 뽑아온 모델 바이너리 정보를 로드함.
	* 서버에서는 충돌정보만을 가져온다.
	* 매개변수:
	* string filename : 모델의 경로
	*/
	//void LoadModelFile(string filename);

	/*
	* 설명 : Unity에서 뽑아온 맵에 존재하는 모델 바이너리 정보를 로드함.
	* 서버에서는 충돌정보만을 가져온다.
	* 매개변수:
	* string filename : 모델의 경로
	*/
	void LoadModelFile2(string filename, Zone* zone = nullptr);

	/*
	* 설명 : Model의 AABB를 구성한다.
	*/
	void BakeAABB();

	/*
	* 설명/반환 : Model의 기본 OBB를 반환한다.
	*/
	BoundingOrientedBox GetOBB();
};

struct HumanoidAnimation {
	double Duration = 0;
	void LoadHumanoidAnimation(string filename);
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

	// OBB.Center
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

	/*
	* 설명 : Mesh의 이름과 Mesh 포인터를 받아 Mesh를 추가하는 함수
	* 매개변수 :
	* string name : Mesh의 이름
	* Mesh* ptr : Mesh의 포인터
	* zoneid : Zone의 id
	*/
	static int AddMeshInZone(string name, Mesh* ptr, int zoneid);

	/*
	* 설명 : Model의 이름과 Model 포인터를 받아 Model를 추가하는 함수
	* 매개변수 :
	* string name : Model의 이름
	* Model* ptr : Model의 포인터
	*/
	static int AddModelInZone(string name, Model* ptr, int zoneid);

	void GetRealShape(Mesh*& out0, Model*& out1) {
		if (isMesh()) out0 = reinterpret_cast<Mesh*>(FlagPtr & 0x7FFFFFFFFFFFFFFF);
		else out1 = reinterpret_cast<Model*>(FlagPtr & 0x7FFFFFFFFFFFFFFF);
	}
};

/*
	* 매개변수 :
*/
extern vector<Item> ItemTable;

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

	// OBB.Center
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
	// OBB.Center
	Tag_SkinMeshObject = 3,
	// OBB.Center
};

struct GameObject {
#define STC_CurrentStruct GameObject
	STC_STATICINIT_innerStruct;

	/////////////////
	// OBB.Center
	UINT TourID = 0;

	/*
	* 게임오브젝트를 구분하고 탐색하기 위한 tag. 32개의 tag를 보유할 수 있다.
	* 항상 tag의 첫번째 비트는 enable이다. (게임오브젝트가 활성화 되어있는지 여부)
	*/
	STCDef(Tag, tag);

	// appearance
	STCDef(int, shapeindex);

	// OBB.Center
	STCDef(int, parent);
	STCDef(int, childs);
	STCDef(int, sibling);

	// OBB.Center
	//STC
	union {
		int* material = nullptr; // mesh 일 경우에만 활성화됨. slotNum만큼. game.MaterialTable에서 접근.
		matrix* transforms_innerModel; // model 일 경우만 활성화됨. nodeCount 만큼.
	};

	// OBB.Center
	int zoneId = 0;

	static unsigned int _offset_fn_material() {
		GameObject obj{}; char* base = reinterpret_cast<char*>(&obj); char* mem = reinterpret_cast<char*>(&obj.material); return (mem - base);
	} inline static MemberInfo _reg_material{ "material", _offset_fn_material, sizeof(int*) };;
	static unsigned int _offset_fn_transforms_innerModel() {
		GameObject obj{}; char* base = reinterpret_cast<char*>(&obj); char* mem = reinterpret_cast<char*>(&obj.transforms_innerModel); return (mem - base);
	} inline static MemberInfo _reg_transforms_innerModel{ "transforms_innerModel", _offset_fn_transforms_innerModel, sizeof(matrix*) };;

	// OBB.Center
	STCDef(matrix, worldMat);

	// OBB.Center
	// OBB.Center
	GameObject();
	virtual ~GameObject();

	virtual matrix GetWorld();
	virtual void SetWorld(matrix localWorldMat);

	virtual void Release();
	virtual BoundingOrientedBox GetOBB();

	virtual void SetShape(int shapeindex);

	virtual void OnRayHit(GameObject* rayFrom);

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

	virtual void SendGameObject(int objindex, SendDataSaver& sds);

	static void PrintOffset(ofstream& ofs) {
		for (int i = 0;i < g_member.size();++i) {
			g_member[i].offset = g_member[i].get_offset();
			ofs << g_member[i].name << " " << g_member[i].offset << " " << g_member[i].size << endl;
		}
	}
#undef STC_CurrentStruct
};

struct StaticGameObject : public GameObject {
	StaticGameObject();
	virtual ~StaticGameObject();
	vector<BoundingOrientedBox> obbArr;

	virtual matrix GetWorld();
	virtual void SetWorld(matrix localWorldMat);

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

	virtual void SendGameObject(int objindex, SendDataSaver& sds);

	static void PrintOffset(ofstream& ofs) {
		for (int i = 0;i < g_member.size();++i) {
			g_member[i].offset = g_member[i].get_offset();
			ofs << g_member[i].name << " " << g_member[i].offset << " " << g_member[i].size << endl;
		}
	}
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

	BoundingBox GetAABB(Zone* zone);
};

// OBB.Center
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

	/*
	* 설명 : 현재 포함 청크에서 정보를 해제하고, local world matrix 를 변경한다.
	* 그 후, 다시 바뀐 위치의 청크 정보를 기입한다.
	* 단순히 변수를 바꾸는 것 이상의 연산을 하기 때문에 자주 사용하는건 좋지 않다.
	* 포탈과 같은 특수 상황에서나 사용이 가능하다.
	* 위치를 바꾸려면 그냥 Velocity를 조정하는 것이 좋다.
	*/
	virtual void SetWorld(matrix local);

	//ServerOnly
	vec4 tickLVelocity;
	vec4 tickAVelocity;
	vec4 LastQuerternion;
	GameObjectIncludeChunks IncludeChunks;
	int* chunkAllocIndexs = nullptr;
	int chunkAllocIndexsCapacity = 8;
	Zone* zone = nullptr;

	//STC
	STCDef(vec4, LVelocity);

	void InitialChunkSetting();
	//void Move(vec4 velocity, vec4 Q);
	virtual void MoveChunck(const vec4& velocity, const vec4& Q, const GameObjectIncludeChunks& beforeChunckInc, const GameObjectIncludeChunks& afterChunkInc);
	virtual void Update(float delatTime);

	//virtual void Event(WinEvent evt);
	virtual void Release();
	virtual BoundingOrientedBox GetOBB();

	void LookAt(vec4 look, vec4 up = { 0, 1, 0, 0 });

	virtual void OnRayHit(GameObject* rayFrom);

	void PositionInterpolation(float deltaTime);

	/*
	* 설명 : 두 게임 오브젝트간의 움직임에 대한 충돌을 처리한다.
	* 매개변수 :
	* GameObject* gbj1 : 첫번째 게임오브젝트
	* GameObject* gbj2 : 두번째 게임오브젝트
	*/
	static void CollisionMove(DynamicGameObject* gbj1, DynamicGameObject* gbj2);

	/*
	* 설명 : movObj가 움직이고, colObj 가 정지해 있을때, 서로 충돌할 경우,
	* 보다 자연스럽게 이동하기 위해서 colObj의 기저를 기준으로 이동을 시행한다.
	* 매개변수 :
	* GameObject* movObj : 움직이는 게임오브젝트
	* GameObject* colObj : 정지해있는 게임오브젝트
	*
	* <CollisionMove_DivideBaseline_StaticOBB 가 있는데 왜 이 함수가 있는가?>
	* >> 그것은 GameObject를 받으면, 기저를 계산함에 있어 유리함이 있기 때문에 함수를 나누어야 함.
	*/
	static void CollisionMove_DivideBaseline(DynamicGameObject* movObj, DynamicGameObject* colObj);

	/*
	* 설명 : movObj가 움직이고, colOBB와 서로 충돌할 경우,
	* 보다 자연스럽게 이동하기 위해서 colOBB의 기저를 기준으로 이동을 시행한다.
	* 매개변수 :
	* GameObject* movObj : 움직이는 게임오브젝트
	* BoundingOrientedBox colOBB : 정지해있는 게임오브젝트
	*/
	static void CollisionMove_DivideBaseline_StaticOBB(DynamicGameObject* movObj, BoundingOrientedBox colOBB);

	/*
	* 설명 : movObj가 preMove만큼 움직인후, 게속 움직이고, colOBB와 서로 충돌할 경우,
	* 보다 자연스럽게 이동하기 위해서 colOBB의 기저를 기준으로 이동을 시행한다.
	* 이때 colObj는 오직 기저를 계산하기 위해 사용된다.
	* 매개변수 :
	* GameObject* movObj : 움직이는 게임오브젝트
	* BoundingOrientedBox colOBB : 정지해있는 게임오브젝트
	*/
	static void CollisionMove_DivideBaseline_rest(DynamicGameObject* movObj, DynamicGameObject* colObj, BoundingOrientedBox colOBB, vec4 preMove);

	/*
	* 설명 : 게임오브젝트가 충돌했을때에 호출되는 함수.
	* GameObject* other : 충돌한 오브젝트
	*/
	virtual void OnCollision(GameObject* other);

	/*
	* 설명 : 게임오브젝트가 움직이지 않는 Static 충돌체와 충돌했을때에 호출되는 함수.
	* BoundingOrientedBox other : 충돌한 OBB.
	*/
	virtual void OnStaticCollision(BoundingOrientedBox other);

	/*
	* 설명 : 게임오브젝트가 Ray와 충돌했을때 호출되는 함수
	* GameObject* shooter : Ray를 쏜 게임 오브젝트
	*/
	virtual void OnCollisionRayWithBullet(GameObject* shooter, float damage);

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

	virtual void SendGameObject(int objindex, SendDataSaver& sds);

	static void PrintOffset(ofstream& ofs) {
		GameObject::PrintOffset(ofs);
		for (int i = 0;i < g_member.size();++i) {
			g_member[i].offset = g_member[i].get_offset();
			ofs << g_member[i].name << " " << g_member[i].offset << " " << g_member[i].size << endl;
		}
	}

#undef STC_CurrentStruct
};

struct SkinMeshGameObject : public DynamicGameObject {
#define STC_CurrentStruct SkinMeshGameObject
	STC_STATICINIT_innerStruct;
	SkinMeshGameObject();
	virtual ~SkinMeshGameObject();

	STCDef(float, AnimationFlowTime);
	STCDef(int, PlayingAnimationIndex);

	/*
	* 설명 : 현재 포함 청크에서 정보를 해제하고, local world matrix 를 변경한다.
	* 그 후, 다시 바뀐 위치의 청크 정보를 기입한다.
	* 단순히 변수를 바꾸는 것 이상의 연산을 하기 때문에 자주 사용하는건 좋지 않다.
	* 포탈과 같은 특수 상황에서나 사용이 가능하다.
	* 위치를 바꾸려면 그냥 Velocity를 조정하는 것이 좋다.
	*/
	virtual void SetWorld(matrix local);

	virtual void Update(float delatTime);

	virtual void MoveChunck(const vec4& velocity, const vec4& Q, const GameObjectIncludeChunks& beforeChunckInc, const GameObjectIncludeChunks& afterChunkInc);

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

	virtual void SendGameObject(int objindex, SendDataSaver& sds);

	static void PrintOffset(ofstream& ofs) {
		DynamicGameObject::PrintOffset(ofs);
		for (int i = 0;i < g_member.size();++i) {
			g_member[i].offset = g_member[i].get_offset();
			ofs << g_member[i].name << " " << g_member[i].offset << " " << g_member[i].size << endl;
		}
	}
#undef STC_CurrentStruct
};

enum class WeaponType { MachineGun, Sniper, Shotgun, Rifle, Pistol, Max };

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

	Weapon() {

	}

	Weapon(WeaponType type) : m_info(GWeaponTable[(int)type]) {
		m_shootFlow = m_info.shootDelay;
		m_recoilFlow = m_info.recoilDelay;
	}

	Weapon(const Weapon& ref) {
		m_info = ref.m_info;
		m_shootFlow = ref.m_shootFlow;
		m_recoilFlow = ref.m_recoilFlow;
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
	* 설명 : obb를 맵 청크 내에 담는다.
	* 현재 반동이 얼마나 진행되었는지 0~1 사이 값으로 반환
	*/
	float GetRecoilAlpha() const {
		float alpha = 1.0f - (m_recoilFlow / m_info.recoilDelay);
		return (alpha < 0) ? 0 : alpha;
	}
};

/*
* 설명 : 플래이어 게임 오브젝트 구조체
*/
struct SkillData {
	SkillEffectType effectType;
	float cooldown;
	float heatCost;
	float range;
	float radius;
	float power;
	float duration;
};

struct JobData {
	PlayerJob job;
	WeaponType defaultWeapon;
	SkillData skills[(int)SkillSlot::Max];
};

static JobData GJobTable[] = {
	{ PlayerJob::Juggernaut, WeaponType::MachineGun, {
		{ SkillEffectType::Juggernaut_FireProjectile, 5.0f, 35.0f, 35.0f, 1.2f, 40.0f, 1.0f },
		{ SkillEffectType::Juggernaut_Taunt, 10.0f, 30.0f, 0.0f, 6.0f, 0.0f, 1.5f },
		{ SkillEffectType::Juggernaut_UltimateFire, 32.0f, 80.0f, 30.0f, 5.0f, 75.0f, 5.0f },
	} },
	{ PlayerJob::Frost, WeaponType::Shotgun, {
		{ SkillEffectType::Frost_Cone, 7.0f, 25.0f, 12.0f, 5.0f, 20.0f, 1.0f },
		{ SkillEffectType::Frost_IceBlock, 12.0f, 20.0f, 0.0f, 3.0f, 35.0f, 1.5f },
		{ SkillEffectType::Frost_Blizzard, 34.0f, 90.0f, 0.0f, 8.0f, 45.0f, 4.0f },
	} },
	{ PlayerJob::Aegis, WeaponType::Pistol, {
		{ SkillEffectType::Aegis_ShieldCharge, 7.0f, 20.0f, 8.0f, 2.0f, 25.0f, 0.8f },
		{ SkillEffectType::Aegis_Barrier, 14.0f, 35.0f, 8.0f, 4.0f, 0.0f, 3.0f },
		{ SkillEffectType::Aegis_ShieldAura, 32.0f, 80.0f, 0.0f, 7.0f, 50.0f, 6.0f },
	} },
	{ PlayerJob::Mage, WeaponType::Pistol, {
		{ SkillEffectType::Mage_FireBall, 4.0f, 20.0f, 30.0f, 1.0f, 35.0f, 1.0f },
		{ SkillEffectType::Fire_Ring, 8.0f, 35.0f, 8.0f, 4.0f, 20.0f, 1.5f },
		{ SkillEffectType::Fire_Pillar, 25.0f, 100.0f, 20.0f, 5.0f, 80.0f, 2.5f },
	} },
	{ PlayerJob::Healer, WeaponType::Pistol, {
		{ SkillEffectType::Healer_HealAura, 10.0f, 0.0f, 0.0f, 3.5f, 0.0f, 1.5f },
		{ SkillEffectType::Electric_Arc, 8.0f, 25.0f, 18.0f, 1.0f, 15.0f, 1.0f },
		{ SkillEffectType::Healer_HealAura, 30.0f, 100.0f, 0.0f, 7.0f, 100.0f, 3.0f },
	} },
	{ PlayerJob::Gunner, WeaponType::Rifle, {
		{ SkillEffectType::Gunner_Muzzle, 5.0f, 20.0f, 30.0f, 1.0f, 20.0f, 0.5f },
		{ SkillEffectType::Electric_Burst, 9.0f, 35.0f, 12.0f, 3.0f, 25.0f, 1.0f },
		{ SkillEffectType::Ember_Shower, 28.0f, 100.0f, 25.0f, 5.0f, 60.0f, 3.0f },
	} },
	{ PlayerJob::Tank, WeaponType::Shotgun, {
		{ SkillEffectType::Tank_ShockWave, 7.0f, 25.0f, 0.0f, 5.0f, 20.0f, 1.0f },
		{ SkillEffectType::Electric_Burst, 11.0f, 35.0f, 0.0f, 4.0f, 25.0f, 1.0f },
		{ SkillEffectType::Tank_ShockWave, 32.0f, 100.0f, 0.0f, 8.0f, 70.0f, 2.0f },
	} },
};

inline const JobData& GetJobData(PlayerJob job) {
	int index = (int)job;
	if (index < 0 || index >= (int)PlayerJob::Max) index = (int)PlayerJob::Healer;
	return GJobTable[index];
}
/*
* 설명 : 플래이어 게임 오브젝트 구조체
*/
struct Player : public SkinMeshGameObject {
#define STC_CurrentStruct Player
	STC_STATICINIT_innerStruct;
	//STC HP
	STCDef(float, HP);
	// OBB.Center
	STCDef(float, MaxHP);// = 100;
	STCDef(float, Attack); // = 0;
	STCDef(float, Defense) // = 0;
	// OBB.Center
	STCDef(int, bullets);// = 100;
	// OBB.Center
	STCDef(int, KillCount);// = 0;
	// OBB.Center
	STCDef(int, DeathCount);// = 0;
	// OBB.Center
	STCDef(float, HeatGauge);// = 0;
	// OBB.Center
	STCDef(float, MaxHeatGauge);// = 100;
	//STC player job
	STCDef(int, m_currentJob);
	//STC skill cooldown duration by slot
	STCDefArr(float, SkillCooldown, (int)SkillSlot::Max);
	//STC skill cooldown remaining by slot
	STCDefArr(float, SkillCooldownFlow, (int)SkillSlot::Max);
	// OBB.Center
	STCDef(int, m_currentWeaponType);// = 0;
	// OBB.Center
	static constexpr int maxItem = 49;
	STCDefArr(ItemStack, Inventory, maxItem);
	// OBB.Center
	STCDef(Weapon, weapon);
	// OBB.Center
	STCDef(float, ZoneMoveCooldown);
	bool m_frostPassiveUsed = false;
	float m_tempMaxHpBonus = 0.0f;
	float m_tempMaxHpTimer = 0.0f;
	float m_iceBlockTimer = 0.0f;

	// OBB.Center
	float zoneMoveCooldownRemain = 0.0f;
	int lastBoundaryIndex = -1;
	bool wasInsideBoundary = false;

	// OBB.Center
	float JumpVelocity = 5;
	// OBB.Center
	bool isGround = false;
	// OBB.Center
	int collideCount = 0;
	// OBB.Center
	int clientIndex = 0;

	// OBB.Center
	BitBoolArr<2> InputBuffer;
	// OBB.Center
	bool bFirstPersonVision = true;
	// OBB.Center

	float m_yaw;
	float m_pitch;

	Player();

	virtual ~Player() { }

	/*
	* 설명 : 플레이어 상태를 한 프레임 갱신한다.
	* float deltaTime : 이전 업데이트부터 현재까지의 시간 차이
	*/
	virtual void Update(float deltaTime) override;
	void ApplyJob(PlayerJob job);
	void SyncJobState(Zone* zones);
	bool TryUseSkill(SkillSlot slot);
	void UpdateSkillCooldowns(float deltaTime, Zone* zones);
	void UpdateJobTimers(float deltaTime, Zone* zones);

	/*
		////STC 플레이어의 인벤토리 정보
		//static constexpr int maxItem = 36;
		//ItemStack Inventory[maxItem];
	*/
	virtual void OnCollision(GameObject* other) override;

	/*
	* 설명 : 플레이어가 맵의 Static 충돌체와 충돌했을 때 호출된다.
	* BoundingOrientedBox obb : 충돌한 OBB
	*/
	virtual void OnStaticCollision(BoundingOrientedBox obb) override;

	/*
	* 설명/반환 : 플레이어의 충돌 OBB 정보를 반환한다.
	*/
	virtual BoundingOrientedBox GetOBB();

	/*
	* 설명 : damage 만큼 플레이어에게 피해를 적용한다.
	* float damage : 적용할 피해량
	*/
	void TakeDamage(float damage);

	/*
	* 설명 : 플레이어를 리스폰 상태로 되돌린다.
	*/
	void Respawn();

	/*
	* 설명 : 총알 Ray와 플레이어가 충돌했을 때 호출된다.
	* GameObject* shooter : Ray를 발사한 오브젝트
	*/
	virtual void OnCollisionRayWithBullet(GameObject* shooter, float damage);

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
		// OBB.Center
		float MaxHP = 100;
		float Attack = 0;
		float Defense = 0;
		// OBB.Center
		int bullets = 100;
		// OBB.Center
		int KillCount = 0;
		// OBB.Center
		int DeathCount = 0;
		// OBB.Center
		float HeatGauge = 0;
		// OBB.Center
		float MaxHeatGauge = 100;
		//STC player job
		int m_currentJob = (int)PlayerJob::Healer;
		//STC skill cooldown duration by slot
		float SkillCooldown[(int)SkillSlot::Max] = {};
		//STC skill cooldown remaining by slot
		float SkillCooldownFlow[(int)SkillSlot::Max] = {};
		// OBB.Center
		int m_currentWeaponType = 0;

		// OBB.Center
		//static constexpr int maxItem = 36;
		//ItemStack Inventory[maxItem];
		// OBB.Center
		Weapon weapon;
	};
#pragma pack(pop)

	virtual void SendGameObject(int objindex, SendDataSaver& sds);

	static void PrintOffset(ofstream& ofs) {
		SkinMeshGameObject::PrintOffset(ofs);
		for (int i = 0;i < g_member.size();++i) {
			g_member[i].offset = g_member[i].get_offset();
			ofs << g_member[i].name << " " << g_member[i].offset << " " << g_member[i].size << endl;
		}
	}
#undef STC_CurrentStruct
};

//astar pathfinding
struct AstarNode {
	int xIndex, zIndex;
	float worldx, worldz;
	bool cango;
	float gCost, hCost, fCost;
	AstarNode* parent;
};

struct AstarNode2 {
	BoundingOrientedBox obb;
	float distance = 0;
	float cost = 0;
	AstarNode2* parent = nullptr;

	bool operator<(const AstarNode2& other) const {
		return distance + cost < other.distance + other.cost;
	}
};

AstarNode* FindClosestNode(float wx, float wz, const std::vector<AstarNode*>& allNodes);

struct StatusEffect {
	StatusEffectType type = StatusEffectType::None;
	float duration = 0.0f;
	float remainTime = 0.0f;
	float power = 0.0f;
	int sourceObjIndex = -1;
	bool active = false;
};

/*
* 설명 : 몬스터 게임 오브젝트.
*/
struct Monster : public SkinMeshGameObject {
#define STC_CurrentStruct Monster
	STC_STATICINIT_innerStruct;

	// OBB.Center
	STCDef(float, HP); // = 30;
	// OBB.Center
	STCDef(float, MaxHP); // = 30;
	STCDef(float, Defense) // = 0;
	// OBB.Center
	STCDef(bool, isDead);// = false;

	// OBB.Center
	vec4 m_homePos;
	// OBB.Center
	vec4 m_targetPos;
	// OBB.Center
	float m_speed = 2.0f;
	// OBB.Center
	float m_patrolRange = 20.0f;
	// OBB.Center
	float m_chaseRange = 10.0f;
	// ServerOnly 상태
	float m_patrolTimer = 0.0f;
	// OBB.Center
	float m_fireDelay = 1.0f;
	// OBB.Center
	float m_fireTimer = 0.0f;
	// OBB.Center
	int collideCount = 0;
	// 여러 개의 OBB를 소유하고 있다.
	int targetSeekIndex = 0;
	// OBB.Center
	Player** Target = nullptr;
	// OBB.Center
	bool m_isMove = false;
	// OBB.Center
	bool isGround = false;
	// OBB.Center
	float respawntimer = 0;
	// ServerOnly 상태
	float pathfindTimer = 0.0f;

	static constexpr int MaxStatusEffectCount = 8;
	StatusEffect StatusEffects[MaxStatusEffectCount];

	Monster();
	virtual ~Monster() {}

	virtual void Update(float deltaTime) override;
	//virtual void Render();
	virtual void OnCollision(GameObject* other) override;

	virtual void OnStaticCollision(BoundingOrientedBox obb) override;

	virtual void OnCollisionRayWithBullet(GameObject* shooter, float damage);
	void ApplyDamage(GameObject* attacker, float damage);
	void UpdateStatusEffects(float deltaTime);
	bool AddStatusEffect(StatusEffectType type, float duration, float power, int sourceObjIndex);
	void RemoveStatusEffect(StatusEffectType type);
	bool HasStatusEffect(StatusEffectType type) const;
	float GetStatusPower(StatusEffectType type) const;

	void Init(const XMMATRIX& initialWorldMatrix);

	void Respawn();

	virtual BoundingOrientedBox GetOBB();

	//astar pathfinding
	vector<AstarNode*> AstarSearch(AstarNode* start, AstarNode* destination, std::vector<AstarNode*>& allNodes);
	AstarNode* FindClosestNode(float wx, float wz, const std::vector<AstarNode*>& allNodes);
	void MoveByAstar(float deltaTime);
	std::vector<AstarNode*> path; // 현재 따라가야 할 경로
	size_t currentPathIndex = 0;

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

		// OBB.Center
		float HP = 30;
		// OBB.Center
		float MaxHP = 30;
		// OBB.Center
		bool isDead = false;
		float Defense = 0;
	};
#pragma pack(pop)

	virtual void SendGameObject(int objindex, SendDataSaver& sds);

	static void PrintOffset(ofstream& ofs) {
		SkinMeshGameObject::PrintOffset(ofs);
		for (int i = 0;i < g_member.size();++i) {
			g_member[i].offset = g_member[i].get_offset();
			ofs << g_member[i].name << " " << g_member[i].offset << " " << g_member[i].size << endl;
		}
	}
#undef STC_CurrentStruct
};

/*
* 맵 전체 영역의 AABB와 StaticCollision OBB를 저장한다.
* Sentinel Value:
* NULL = (obbs.size() == 0)
*/
struct GameChunk {
	vecset<StaticGameObject*> Static_gameobjects;
	vecset<DynamicGameObject*> Dynamic_gameobjects;
	vecset<SkinMeshGameObject*> SkinMesh_gameobjects;
	vector<indexRange> IR_Dynamic;
	vector<indexRange> IR_SkinMesh;
	int dynamicIRSiz = 0;
	int SkinMeshIRSiz = 0;

	ChunkIndex cindex;
	BoundingBox AABB;
	UINT TourID = 0;

	GameChunk() {
		Static_gameobjects.Init(32);
		Dynamic_gameobjects.Init(32);
		SkinMesh_gameobjects.Init(32);
	}
	void SetChunkIndex(ChunkIndex ci, Zone* zone);

	void RenderChunkDbg();
};

/*
	/*
	* 설명 : 어떤 obb가 Map에 StaticCollision에 대하여 충돌하는지 계산하는 함수
	* 매개변수 :
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
* 설명 : 게임 맵 데이터.
* Sentinel Value:
* NULL = 초기화되지 않은 맵
* (name.size() == 0 && meshes.size() == 0 &&
* mesh_shapeindexes == 0 && models.size() == 0 &&
* MapObjects.size() == 0)
*/
struct GameMap {
	GameMap() {}
	~GameMap() {}
	// OBB.Center
	vector<string> name;

	// OBB.Center
	vector<Mesh> meshes;

	// OBB.Center
	vector<int> mesh_shapeindexes;

	// OBB.Center
	vector<Model*> models;

	// OBB.Center
	vector<int> model_shapeindexes;

	// OBB.Center
	vector<StaticGameObject*> MapObjects;

	// OBB.Center
	vec4 AABB[2] = { 0, 0 };

	// OBB.Center
	Zone* ownerzone = nullptr;

	unsigned int TextureTableStart = 0;
	unsigned int MaterialTableStart = 0;

	/*
	* 설명 : OBB를 포함하도록 맵 전체 AABB를 확장한다.
	* BoundingOrientedBox obb : 추가할 OBB
	*/
	void ExtendMapAABB(BoundingOrientedBox obb);

	/*
	* 설명 : 맵의 StaticCollision 정보를 Chunk 단위로 굽고 전체 AABB를 계산한다.
	*/
	void BakeStaticCollision();

	/*
	* 설명 : 동적 오브젝트가 맵 StaticCollision과 충돌했는지 검사하고 보정한다.
	* DynamicGameObject* obj : 충돌 검사를 수행할 오브젝트
	*/
	void StaticCollisionMove(DynamicGameObject* obj);

	/*
	* 설명 : OBB가 맵 StaticCollision과 충돌하는지 검사한다.
	* BoundingOrientedBox obb : 추가할 OBB
	* 반환 : 충돌하면 true, 아니면 false
	*/
	bool isStaticCollision(BoundingOrientedBox obb);

	/*
	* 설명 : 맵을 로드한다.
	* const char* MapName : 로드할 맵 이름. .map 확장자를 제외한 이름을 사용한다.
	*/
	void LoadMap(const char* MapName);
};

/*
설명 : PACK 연산에 쓰일 한번에 send에 보내질 최대 데이터 사이즈.
*/
struct twoPage {
	char data[8196] = {};
};

/*
설명 : 접속한 Client마다 가지고 있는 클라이언트의 데이터.
*/
struct ClientData {
	// OBB.Center
	SOCKET socket;

	// OBB.Center
	static constexpr int rbufcap = 8192 - sizeof(int);
	char rbuf[rbufcap + sizeof(int)] = {};
	int rbufoffset = 0;

	// OBB.Center
	NWAddr addr;

	// OBB.Center
	Player* pObjData;

	// OBB.Center
	int objindex = 0;

	// OBB.Center
	int zoneId = 0;

	// OBB.Center
	SendDataSaver PersonalSDS;

	__forceinline int recv(char* data, int len, DWORD flag) {
		WSABUF buf;
		buf.buf = data;
		buf.len = len;
		DWORD retval = 0;
		int err = WSARecv(socket, &buf, 1, &retval, &flag, NULL, NULL);
		if (err == SOCKET_ERROR) return -1;
		return (int)retval;
	}

	void SetNonBlocking() {
		u_long val = 1;
		int ret = ioctlsocket(socket, FIONBIO, &val);
		if (ret != 0)
		{
			stringstream ss;
			ss << "bind failed:" << WSAGetLastError();
			throw ss.str().c_str();
		}
	}

	void GetClientAddr() {
		socklen_t retLength = sizeof(addr.addr);
		if (::getpeername(socket, (sockaddr*)&addr.addr, &retLength) < 0)
		{
			stringstream ss;
			ss << "getPeerAddr failed:" << WSAGetLastError();
			throw ss.str().c_str();
		}
		if (retLength > sizeof(addr.addr))
		{
			stringstream ss;
			ss << "getPeerAddr buffer overrun: " << retLength;
			throw ss.str().c_str();
		}

		inet_ntop(AF_INET, &addr.addr.sin_addr, addr.IPString, sizeof(addr.IPString) - 1);
	}

	static void DisconnectToServer(int index);
};

struct collisionchecksphere {
	vec4 center;
	float radius;
};

// OBB.Center
struct World;
struct Zone;
struct Portal;

/*
* 설명 : Zone 사이를 연결하는 경계와 이동 정보를 저장한다.
*/
struct Zoneboundary {
	int basezoneId;
	int dstzoneId;
	int dstServerId = 0;

	vec4 minPos = {};
	vec4 maxPos = {};

	vec4 spawnPos = {};
    float spawnYaw = 0.0f;
	float cooldownSec = 2.0f;

	bool enabled = true;
};

/*
* 설명 : 하나의 게임 Zone을 관리한다.
* 동적/정적 오브젝트, 맵, 청크, Astar 데이터와 포탈 정보를 가진다.
*/
struct Zone {
	// OBB.Center
	World* world = nullptr;

	// OBB.Center
	int zoneId = 0;

	// OBB.Center
	vecset<DynamicGameObject*> Dynamic_gameObjects;
	vecset<StaticGameObject*> Static_gameObjects;

	// OBB.Center
	vector<Portal*> portals;

	// OBB.Center
	vecset<ItemLoot> DropedItems;

	// OBB.Center
	SendDataSaver CommonSDS;

	// OBB.Center
	GameMap map;

	// OBB.Center
	vector<Zoneboundary> boundaries;

	// Astar pathfinding
	vector<AstarNode*> allnodes;
	static constexpr float AstarStartX = -40.0f;
	static constexpr float AstarStartZ = -40.0f;
	static constexpr float AstarEndX = 40.0f;
	static constexpr float AstarEndZ = 40.0f;

	// OBB.Center
	int currentIndex = 0;

	// OBB.Center
	static constexpr float lowFrequencyDelay = 0.2f;
	float lowFrequencyFlow = 0.0f;
	__forceinline bool lowHit() {
		return lowFrequencyFlow > lowFrequencyDelay;
	}

	static constexpr float midFrequencyDelay = 0.05f;
	float midFrequencyFlow = 0.0f;
	__forceinline bool midHit() {
		return midFrequencyFlow > midFrequencyDelay;
	}

	static constexpr float highFrequencyDelay = 0.01f;
	float highFrequencyFlow = 0.0f;
	__forceinline bool highHit() {
		return highFrequencyFlow > highFrequencyDelay;
	}

	UINT TourID = 0;

	// OBB.Center
	static constexpr float chunck_divide_Width = 50.0f;

	// OBB.Center
	unordered_map<ChunkIndex, GameChunk*> chunck;

	// OBB.Center
	// OBB.Center
	vector<Shape> ZoneShapeTable;
	unordered_map<string, int> ZoneStrToShapeIndex;
	vector<string> ZoneShapeStrTable;
	Shape& GetShape(int shapeindex);

	int ZoneTextureCount = 0;
	int ZoneMaterialCount = 0;

	// OBB.Center
	Zone() : world(nullptr), zoneId(0) {}
	Zone(World* w, int id) : world(w), zoneId(id) {}

	/*
	* 설명 : Zone을 초기화한다.
	* Astar 데이터, 오브젝트 배열, 맵 로드, 포탈 정보 등을 준비한다.
	*/
	void Init();

	/*
	* 설명 : Zone을 DeltaTime만큼 갱신한다.
	* 오브젝트 업데이트, 이동, 충돌 검사 등을 수행한다.
	*/
	void Update(float deltaTime);

	// OBB.Center

	/*
	* 설명 : 새로운 Dynamic 오브젝트를 Zone에 추가한다.
	*/
	int NewObject(DynamicGameObject* obj, GameObjectType gotype);

	/*
	* 설명 : 새로운 플레이어를 Zone에 추가한다.
	*/
	int NewPlayer(SendDataSaver& sds, Player* obj, int clientIndex);

	/*
	* 설명 : 플레이어를 Zone에서 제거한다. 객체 자체는 삭제하지 않는다.
	*/
	void RemovePlayer(int clientIndex);

	/*
	* 설명 : 기존 Zone에 플레이어를 배치한다.
	*/
	int AddPlayer(int clientIndex, Player* player, vec4 spawnPos, bool update_Map = true);

	// OBB.Center

	void SpawnPortal();
	void Spawnboundary();
	void CheckPortalCollision(Player* p);
	void CheckBoundaryCrossing(Player* p, float deltaTime);

	// OBB.Center

	/*
	// 글로벌 머터리얼 카운트
	*/
	void FlushSendToClients();

	/*
	// 글로벌 머터리얼 카운트
	*/
	void SendingAllObjectForNewClient(SendDataSaver& sds);

	// OBB.Center

	void Sending_NewGameObject(SendDataSaver& sds, int newindex, GameObject* newobj);

	template <typename memberType>
	void Sending_ChangeGameObjectMember(SendDataSaver& sds, int objindex, GameObject* ptrobj, GameObjectType gotype, void* memberAddr) {
		sds.postpush_start();
		constexpr int memberSize = sizeof(memberType);
		constexpr int reqsiz = sizeof(STC_ChangeMemberOfGameObject_Header) + memberSize;
		sds.postpush_reserve(reqsiz);
		STC_ChangeMemberOfGameObject_Header& header = *(STC_ChangeMemberOfGameObject_Header*)sds.ofbuff;
		header.size = reqsiz;
		header.st = STC_Protocol::ChangeMemberOfGameObject;
		header.type = gotype;
		header.objindex = objindex;
		header.serveroffset = ((char*)memberAddr) - (char*)ptrobj;
		header.datasize = memberSize;
		sds.postpush_senddata<sizeof(STC_ChangeMemberOfGameObject_Header), memberSize>(memberAddr);
		sds.postpush_end();
	}

	void Sending_NewRay(SendDataSaver& sds, vec4 rayStart, vec4 rayDirection, float rayDistance);
	//void Sending_SetMeshInGameObject(SendDataSaver& sds, int objindex, string str);
	void Sending_DeleteGameObject(SendDataSaver& sds, int objindex);
	void Sending_ItemDrop(SendDataSaver& sds, int dropindex, ItemLoot lootdata);
	void Sending_ItemRemove(SendDataSaver& sds, int dropindex);
	void Sending_InventoryItemSync(SendDataSaver& sds, ItemStack lootdata, int inventoryIndex);
	void Sending_PlayerFire(SendDataSaver& sds, int objIndex);
	void Sending_SkillCast(SendDataSaver& sds, int ownerObjIndex, PlayerJob job, SkillSlot slot, SkillEffectType effectType, vec4 position, vec4 direction, float radius, float power, float duration);
	void Sending_StatusEffect(SendDataSaver& sds, int targetObjIndex, int sourceObjIndex, StatusEffectType statusType, bool active, float duration, float remainTime, float power, vec4 position, vec4 extents);


	// OBB.Center

	void FireRaycast(GameObject* shooter, vec4 rayStart, vec4 rayDirection, float rayDistance, float damage);
	int ApplySkillDamage(GameObject* caster, SkillEffectType effectType, vec4 position, vec4 direction, float range, float radius, float damage);

	void GridCollisionCheck();

	// OBB.Center

	GameObjectIncludeChunks GetChunks_Include_OBB(BoundingOrientedBox obb);
	GameChunk* GetChunkFromPos(vec4 pos);
	void PushGameObject(GameObject* go);

	// OBB.Center

	void LoadMapForZone(int zoneId);
	void SpawnObjects();
	void PrintCangoGrid(const std::vector<AstarNode*>& all, int gridWidth, int gridHeight);
	bool CheckAABBSphereCollision(const vec4& boxCenter, const vec4& boxHalfSize, const collisionchecksphere& sphere);
};

/*
* 설명 : 서버의 전체 게임 월드를 관리한다.
* Zone 배열과 클라이언트, 플레이어 입장/퇴장 흐름을 담당한다.
*/
struct World {
	// OBB.Center
	vecset<ClientData> clients;

	// OBB.Center
	twoPage tempbuffer;

	// OBB.Center
	unsigned int GlobalTextureCount = 0;
	// OBB.Center
	unsigned int GlobalMaterialSiz = 0;
	unsigned int GlobalMaterialCount = 0;
	// OBB.Center
	unsigned int GlobalMeshCount = 0;
	// OBB.Center
	unsigned int GlobalHumanoidAnimaionCount = 0;
	// OBB.Center
	unsigned int GlobalShapeTableSyncSiz = 0;

	// OBB.Center
	vector<HumanoidAnimation> HumanoidAnimationTable;

	// OBB.Center
	static constexpr int zoneCount = 2;
	const char* ZoneMapName[zoneCount] = {
		"The_Port",
		"OfficeDungeon_1floor",
	};
	vector<Zone> zones;
	int serverId = 0;
	unsigned short listenPort = 9000;
	int ownedZoneId = 0;
    unordered_map<int, PlayerTransferData> pendingTransfers;
    int nextTransferToken = 1;

	bool IsZoneOwned(int zoneId) const {
		return zoneId == ownedZoneId;
	}



	/*
	* 설명 : 게임 월드를 초기화한다.
	* Zone 생성과 기본 오브젝트 구성을 준비한다.
	*/
	void Init();

	/*
	* 설명 : 월드를 한 프레임 갱신한다.
	*/
	void Update();

	/*
	* 설명 : 게임 월드를 초기화한다.
	*/
	__forceinline int Receiving(int clientIndex, char* rBuffer, int totallen);

	/*
* 설명 : 서버의 전체 게임 월드를 관리한다.
	*/
	__forceinline void Sending_AllocPlayerIndex(SendDataSaver& sds, int clientindex, int objindex) {
		sds.postpush_start();
		constexpr int reqsiz = sizeof(STC_AllocPlayerIndexes_Header);
		sds.postpush_reserve(reqsiz);
		STC_AllocPlayerIndexes_Header& header = *(STC_AllocPlayerIndexes_Header*)sds.ofbuff;
		header.size = reqsiz;
		header.st = STC_Protocol::AllocPlayerIndexes;
		header.clientindex = clientindex;
		header.server_obj_index = objindex;
		sds.postpush_end();
	}

	__forceinline void Sending_PlayerMoveZone(SendDataSaver& sds, int clientindex, int zoneId){
		sds.postpush_start();
		constexpr int reqsiz = sizeof(STC_PlayerMoveZone_Header);
		sds.postpush_reserve(reqsiz);
		STC_PlayerMoveZone_Header& header = *(STC_PlayerMoveZone_Header*)sds.ofbuff;
		header.size = reqsiz;
		header.st = STC_Protocol::SyncPlayerMoveZone;
		header.clientIndex = clientindex;
		header.zoneId = zoneId;
		sds.postpush_end();
	}

    __forceinline void Sending_ServerTransfer(SendDataSaver& sds, int dstZoneId, const char* ip, unsigned short port, int transferToken) {
		sds.postpush_start();
		constexpr int reqsiz = sizeof(STC_ServerTransfer_Header);
		sds.postpush_reserve(reqsiz);
		STC_ServerTransfer_Header& header = *(STC_ServerTransfer_Header*)sds.ofbuff;
		header.size = reqsiz;
		header.st = STC_Protocol::ServerTransfer;
		header.dstZoneId = dstZoneId;
		header.port = port;
		header.transferToken = transferToken;
		strncpy_s(header.ip, ip, _TRUNCATE);
		sds.postpush_end();
	}


	/*
	* 설명 : 월드를 한 프레임 갱신한다.
	*/
	__forceinline void Sending_SyncGameState(SendDataSaver& sds) {
		sds.postpush_start();
		constexpr int reqsiz = sizeof(STC_SyncGameState_Header);
		sds.postpush_reserve(reqsiz);
		STC_SyncGameState_Header& header = *(STC_SyncGameState_Header*)sds.ofbuff;
		header.size = reqsiz;
		header.st = STC_Protocol::SyncGameState;
		header.DynamicGameObjectCapacity = zones[ownedZoneId].Dynamic_gameObjects.Capacity;
		header.StaticGameObjectCapacity = zones[ownedZoneId].Static_gameObjects.Capacity;
		sds.postpush_end();
	}

	/*
	* 설명 : 플레이어를 srcZone에서 dstZone으로 이동시킨다.
	*/
	void MovePlayerToZone(int clientIndex, int dstZoneId, vec4 spawnPos);

	/*
	* 설명 : 플레이어가 속한 Zone을 얻는다.
	*/
	Zone* GetClientZone(int clientIndex);

	/*
	* 설명 : zoneId에 해당하는 Zone을 얻는다.
	*/
	Zone* GetZone(int zoneId) {
		if (zoneId < 0 || zoneId >= zoneCount) return nullptr;
		if (IsZoneOwned(zoneId) == false) return nullptr;
		return &zones[zoneId];
	}

	unsigned short GetZonePort(int zoneId) const { return (unsigned short)(9000 + zoneId); }
	const char* GetZoneIP(int zoneId) const { return "127.0.0.1"; }
	int IssueTransferToken() { return nextTransferToken++; }
	bool SendPlayerTransferToServer(const PlayerTransferData& data);
	void AcceptClientHello(int clientIndex);
	bool AcceptTransferConnect(int clientIndex, int transferToken);
	void StoreIncomingPlayerTransfer(const PlayerTransferData& data);

	void PrintOffset();
};

struct Portal : public GameObject {
#define STC_CurrentStruct Portal
	STC_STATICINIT_innerStruct;

	STCDef(float, spawnX);
	STCDef(float, spawnY);
	STCDef(float, spawnZ);
	STCDef(float, radius);
	STCDef(int, zoneId);
	STCDef(int, dstzoneId);

	Portal() {}
	virtual ~Portal() {}

	virtual BoundingOrientedBox GetOBB();

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
	virtual void SendGameObject(int objindex, SendDataSaver& sds);

	static void PrintOffset(ofstream& ofs) {
		GameObject::PrintOffset(ofs);
		for (int i = 0; i < g_member.size(); ++i) {
			g_member[i].offset = g_member[i].get_offset();
			ofs << g_member[i].name << " " << g_member[i].offset << " " << g_member[i].size << endl;
		}
	}
#undef STC_CurrentStruct
};


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
* 설명 : 모델의 노드
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
* 설명 : 모델
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
* 설명 : 게임에 나타날 모든 아이템을 저장해놓은 테이블
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

		operator bool() {
			UINT tb = (t->tag & (1 << index));
			return (tb != 0);
		}

		void operator=(bool b) {
			if (b) {
				t->tag |= (1 << index);
			}
			else {
				t->tag &= ~(1 << index);
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
	Tag_Enable = 0, 
	Tag_Dynamic = 1, 
	Tag_SkinMeshObject = 2,
	Tag_Player = 3,
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
	void ApplyDamage(GameObject* source, float damage);

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

	QuestReward(){}
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

	Quest(){}
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

	float MaxHP;
	float Attack;
	float Defense;

	WeaponType defaultWeapon;
	SkillData skills[(int)SkillSlot::Max];
};

static JobData GJobTable[] = {
	{ PlayerJob::Juggernaut, 150.0f, 10.0f, 15.0f, WeaponType::MachineGun, {
		{ SkillEffectType::Juggernaut_FireProjectile, 5.0f, 35.0f, 35.0f, 1.2f, 40.0f, 1.0f },
		{ SkillEffectType::Juggernaut_Taunt, 10.0f, 30.0f, 0.0f, 6.0f, 100.0f, 10.0f },
		{ SkillEffectType::Juggernaut_UltimateFire, 32.0f, 80.0f, 30.0f, 5.0f, 75.0f, 5.0f },
	} },
	{ PlayerJob::Frost, 120.0f, 15.0f, 10.0f, WeaponType::Shotgun, {
		{ SkillEffectType::Frost_Cone, 7.0f, 0.0f, 12.0f, 5.0f, 20.0f, 1.0f },
		{ SkillEffectType::Frost_IceBlock, 12.0f, 0.0f, 0.0f, 3.0f, 100.0f, 2.0f },
		{ SkillEffectType::Frost_Blizzard, 34.0f, 0.0f, 0.0f, 22.0f, 45.0f, 4.0f },
	} },
	{ PlayerJob::Aegis, 100.0f, 20.0f, 9.0f, WeaponType::Pistol, {
		{ SkillEffectType::Aegis_ShieldCharge, 7.0f, 0.0f, 8.0f, 3.2f, 50.0f, 0.32f },
		{ SkillEffectType::Aegis_Barrier, 10.0f, 0.0f, 0.0f, 1.5f, 100.0f, 5.0f },
		{ SkillEffectType::Aegis_ShieldAura, 32.0f, 0.0f, 0.0f, 10.0f, 100.0f, 6.0f },
	} },
	{ PlayerJob::Mage, 105.0f, 18.0f, 8.0f, WeaponType::Rifle, {
		{ SkillEffectType::Rifle_TacticalGrenade, 7.0f, 0.0f, 16.0f, 8.0f, 50.0f, 3.0f },
		{ SkillEffectType::Rifle_StimPack, 14.0f, 0.0f, 0.0f, 1.0f, 10.0f, 7.0f },
		{ SkillEffectType::Rifle_MissileBarrage, 32.0f, 0.0f, 25.0f, 8.0f, 45.0f, 3.0f },
	} },
	{ PlayerJob::Healer, 90.0f, 24.0f, 5.0f, WeaponType::Sniper, {
		{ SkillEffectType::Sniper_GrappleHook, 7.0f, 0.0f, 18.0f, 1.0f, 0.0f, 0.35f },
		{ SkillEffectType::Sniper_ModeSwitch, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
		{ SkillEffectType::Sniper_Railgun, 32.0f, 0.0f, 70.0f, 0.8f, 120.0f, 14.0f },
	} },
	{ PlayerJob::Gunner, 100.0f, 20.0f, 7.0f, WeaponType::DualPistol, {
		{ SkillEffectType::DualPistol_DeathDash, 3.5f, 0.0f, 0.0f, 5.0f, 20.0f, 0.45f },
		{ SkillEffectType::DualPistol_BladeMode, 20.0f, 0.0f, 4.0f, 2.6f, 50.0f, 10.0f },
		{ SkillEffectType::DualPistol_Awaken, 32.0f, 0.0f, 0.0f, 1.0f, 0.0f, 10.0f },
	} },
	{ PlayerJob::DroneOperator, 100.0f, 20.0f, 7.0f, WeaponType::DronePistol, {
		{ SkillEffectType::Drone_Heal, 4.0f, 0.0f, 35.0f, 1.8f, 15.0f, 0.25f },
		{ SkillEffectType::Drone_Assault, 15.0f, 0.0f, 0.0f, 1.0f, 0.0f, 10.0f },
		{ SkillEffectType::Drone_Flight, 32.0f, 0.0f, 0.0f, 30.0f, 20.0f, 10.0f },
	} },
	{ PlayerJob::Hacker, 100.0f, 20.0f, 7.0f, WeaponType::SMG, {
		{ SkillEffectType::Hacker_Hack, 7.0f, 0.0f, 20.0f, 1.0f, 0.0f, 0.0f },
		{ SkillEffectType::Hacker_EMPField, 20.0f, 0.0f, 0.0f, 12.0f, 0.0f, 10.0f },
		{ SkillEffectType::Hacker_EMPBurst, 32.0f, 0.0f, 0.0f, 16.0f, 30.0f, 1.0f },
	} },
	{ PlayerJob::Bomber, 110.0f, 18.0f, 8.0f, WeaponType::GrenadeGun, {
		{ SkillEffectType::Bomber_SpeedBurst, 12.0f, 0.0f, 0.0f, 8.0f, 0.0f, 5.0f },
		{ SkillEffectType::Bomber_AmmoSwitch, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
		{ SkillEffectType::Bomber_Meteor, 32.0f, 0.0f, 0.0f, 36.0f, 90.0f, 1.0f },
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
	// OBB.Center
	STCDef(int, m_currentWeaponType);// = 0;
	STCDef(bool, m_weaponHolstered);
	STCDef(float, ReloadRemain);
	STCDef(bool, m_sniperDmrMode);
	STCDef(bool, m_bomberHealAmmoMode);
	STCDef(bool, isGround);
	// OBB.Center
	static constexpr int maxItem = 49;
	STCDefArr(ItemStack, Inventory, maxItem);
	//STC ����ִ� ����
	STCDefArr(Weapon, weapon, 3);
	STCDef(int, SelectedWeapon);

	//STC �÷��̾��� �� �� �̵� ��ٿ�
	STCDef(float, ZoneMoveCooldown);
	// Death respawn destination. Open-world deaths may replace this with the shared main-map point.
	vec4 RespawnPosition = vec4(0, 0, 0, 1);
	// Independent safety point for invalid coordinates / falling below the map. Only Zone::AddPlayer
	// updates it, so changing the death respawn destination never redirects error recovery.
	vec4 RecoveryPosition = vec4(0, 0, 0, 1);
	// Debug aid for authoring dungeon monster spawn positions.
	float m_dungeonPositionLogFlow = 0.0f;
	bool m_frostPassiveUsed = false;
	float m_tempMaxHpBonus = 0.0f;
	float m_tempMaxHpTimer = 0.0f;
	float m_iceBlockTimer = 0.0f;
	float m_iceBlockEffectFlow = 0.0f;
	float m_frostBlizzardTimer = 0.0f;
	float m_frostBlizzardEffectFlow = 0.0f;
	float m_frostBlizzardRadius = 0.0f;
	float m_frostBlizzardPower = 0.0f;
	float m_juggernautFlameTimer = 0.0f;
	float m_juggernautFlameTickFlow = 0.0f;
	float m_juggernautFlameEffectFlow = 0.0f;
	float m_juggernautFlameRange = 0.0f;
	float m_juggernautFlameRadius = 0.0f;
	float m_juggernautFlameDps = 0.0f;
	float m_juggernautRegenTimer = 0.0f;
	float m_juggernautRegenFlow = 0.0f;
	float m_rifleGrenadeTimer = 0.0f;
	float m_rifleGrenadeDuration = 0.0f;
	float m_rifleGrenadeEffectFlow = 0.0f;
	float m_rifleGrenadeRadius = 0.0f;
	float m_rifleGrenadeDamage = 0.0f;
	vec4 m_rifleGrenadeOrigin = vec4(0, 0, 0, 1);
	vec4 m_rifleGrenadePosition = vec4(0, 0, 0, 1);
	vec4 m_rifleGrenadeVelocity = vec4(0, 0, 0, 0);
	float m_rifleGrenadeArmTimer = 0.0f;
	float m_rifleStimTimer = 0.0f;
	float m_rifleStimRegenFlow = 0.0f;
	float m_rifleStimEffectFlow = 0.0f;
	float m_rifleMissileTimer = 0.0f;
	float m_rifleMissileTickFlow = 0.0f;
	int m_rifleMissileCount = 0;
	vec4 m_rifleMissileOrigin = vec4(0, 0, 0, 1);
	vec4 m_rifleMissileDirection = vec4(0, 0, 1, 0);
	float m_rifleMissileRadius = 0.0f;
	float m_rifleMissileDamage = 0.0f;
	struct RifleMissileImpact {
		vec4 Position = vec4(0, 0, 0, 1);
		float Timer = 0.0f;
		float Radius = 0.0f;
		float Damage = 0.0f;
	};
	std::vector<RifleMissileImpact> m_rifleMissileImpacts;
	int m_sniperSrBullets = 5;
	int m_sniperDmrBullets = 15;
	bool m_railgunSavedDmrMode = false;
	float m_sniperNextShotDamageMultiplier = 1.0f;
	float m_railgunTimer = 0.0f;
	float m_railgunEffectFlow = 0.0f;
	int m_railgunShots = 0;
	float m_dualBladeTimer = 0.0f;
	float m_dualBladeEffectFlow = 0.0f;
	float m_dualAwakenTimer = 0.0f;
	float m_hackerProjectileTimer = 0.0f;
	float m_hackerProjectileEffectFlow = 0.0f;
	vec4 m_hackerProjectilePosition = vec4(0, 0, 0, 1);
	vec4 m_hackerProjectileVelocity = vec4(0, 0, 0, 0);
	float m_hackerEmpFieldTimer = 0.0f;
	float m_hackerEmpHealFlow = 0.0f;
	float m_hackerEmpEffectFlow = 0.0f;
	float m_droneAssaultTimer = 0.0f;
	float m_droneFlightTimer = 0.0f;
	float m_droneFlightHealFlow = 0.0f;
	float m_droneFlightEffectFlow = 0.0f;
	float m_bomberSpeedBuffTimer = 0.0f;
	float m_bomberMeteorTimer = 0.0f;
	float m_bomberMeteorEffectFlow = 0.0f;
	vec4 m_bomberMeteorPosition = vec4(0, 0, 0, 1);
	vec4 m_bomberMeteorTarget = vec4(0, 0, 0, 1);
	struct BomberProjectile {
		vec4 Position = vec4(0, 0, 0, 1);
		vec4 Velocity = vec4(0, 0, 0, 0);
		float Timer = 0.0f;
		float ArmTimer = 0.0f;
		float EffectFlow = 0.0f;
		bool HealMode = false;
	};
	std::vector<BomberProjectile> m_bomberProjectiles;
	bool m_aegisShieldActive = false;
	bool m_weaponTogglePrevInput = false;
	bool m_reloadPrevInput = false;
	bool m_aegisShieldPrevInput = false;
	float m_aegisShieldCooldownTimer = 0.0f;
	float m_aegisShieldInactiveTimer = 0.0f;
	float m_aegisShieldEffectFlow = 0.0f;
	float m_aegisRepairTimer = 0.0f;
	float m_aegisInvincibleTimer = 0.0f;
	float m_aegisAuraEffectFlow = 0.0f;
	float m_aegisAuraDamageTimer = 0.0f;
	float m_aegisAuraDamageFlow = 0.0f;
	float m_aegisAuraElectricFlow = 0.0f;
	float m_aegisAuraRadius = 0.0f;
	float m_aegisChargeTimer = 0.0f;
	float m_aegisChargeEffectFlow = 0.0f;
	float m_sniperBackdashTimer = 0.0f;
	float m_dualDashTimer = 0.0f;
	float m_dualDashDamageFlow = 0.0f;
	std::vector<GameObject*> m_dualDashHitTargets;
	
	// 플레이어가 지나온 경로 표시. NPC는 이 것들중 가장 가깝고, 갈 수 있는 방향으로 전진하도록 만든다.
	vec4 PosTail[32] = {};
	int tailOffset = 0;

	//STC �÷��̾��� ��
	STCDef(int, Gold);

	//STC �÷��̾��� ����ġ
	STCDef(int, Exp);

	//STC �÷��̾��� ����
	STCDef(int, Level);

	STCDef(int, StatPoint);
	STCDef(int, StatHP);
	STCDef(int, StatDefense);
	STCDef(int, StatMoveSpeed);
	STCDef(int, StatAttack);

	static constexpr int ExpLimit[100] = {
		100, 105, 110, 116, 122, 128, 134, 141, 148, 155, 163, 171, 180, 189, 198, 208, 218, 229, 241, 253, 265, 279, 293, 307, 323, 339, 356, 373, 392, 412, 432, 454, 476, 500, 525, 552, 579, 608, 639, 670, 704, 739, 776, 815, 856, 899, 943, 991, 1040, 1092, 1147, 1204, 1264, 1327, 1394, 1464, 1537, 1614, 1694, 1779, 1868, 1961, 2059, 2162, 2270, 2384, 2503, 2628, 2760, 2898, 3043, 3195, 3355, 3522, 3698, 3883, 4077, 4281, 4495, 4720, 4956, 5204, 5464, 5737, 6024, 6325, 6642, 6974, 7322, 7689, 8073, 8477, 8901, 9346, 9813, 10303, 10819, 11360, 11928, 12524
	};

	static constexpr float StatHpPercentPerPoint = 0.05f;
	static constexpr float StatDefensePerPoint = 5.0f;
	static constexpr float StatMoveSpeedPercentPerPoint = 0.001f;
	static constexpr float StatAttackPercentPerPoint = 0.05f;

	//STC �÷��̾ Ư�� ����Ʈ�� �ϼ��ߴ��� ��Ÿ���� ��Ʈ�迭 (�ִ� 4096���� ����Ʈ ���밡��.)
	BitBoolArr<64> CompleteQuestBitArr;
	//STC �÷��̾ Ư�� ����Ʈ�� ���������� ��Ÿ���� ��Ʈ�迭 (�ִ� 4096���� ����Ʈ ���밡��.)
	BitBoolArr<64> PrograssQuestBitArr;

	//STC Quest List - Index�� ���� (���� ����Ʈ)
	vector<int> QuestArr;
	// ���� ����ǰ� �ִ� ����Ʈ ���� (��������)
	vector<Quest*> QuestPrograss;
	void EraseQuest(int index);

	//STC
	int PresentTalkID = -1;

	//ServerOnly �÷��̾ �� �� �̵��� �� ��, �ٽ� �̵��ϱ� ���� ���� ��ٿ� �ð��� ��Ÿ����. �̵��ϰ� ��� ���� ���������� ��� �̵��ϴ� ������ �߻��� �� ������.
	float zoneMoveCooldownRemain = 0.0f;
	int lastBoundaryIndex = -1;
	bool wasInsideBoundary = false;
	// Stable combat-team identity. Unlike clientIndex, this survives server/zone transfers.
	// -1 means solo; solo players are enemies to one another, while self is always allied.
	int partyId = -1;
	// Open-world portal entrance used when this dungeon run ends.
	int dungeonReturnZoneId = -1;
	vec4 dungeonReturnPosition = vec4(0, 0, 0, 1);

	// OBB.Center
	float JumpVelocity = 5;
	// OBB.Center
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
	void StartReload(Zone* zones);
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
	void ApplyDamage(GameObject* source, float damage);

	bool LootItem(ItemStack is);

	bool AddGold(int delta);

	void AddExp(int delta);
	void GrantLevel(int count = 1);
	bool TrySpendStatPoint(PlayerStatType stat);
	void RecalculateStatsFromJob(bool preserveHpRate = true);
	float GetAttackDamageMultiplier() const;
	float GetMoveSpeedMultiplier() const;
	void SyncStatState(Zone* zones);
	bool LoadPersistentData(const char* key);
	void SavePersistentData(const char* key) const;

	void RewardQuest(Quest* completeQuest);

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

		float ShieldDurability = 0;
		float MaxShieldDurability = 100;
		//STC player job
		int m_currentJob = (int)PlayerJob::Healer;
		//STC skill cooldown duration by slot
		float SkillCooldown[(int)SkillSlot::Max] = {};
		//STC skill cooldown remaining by slot
		float SkillCooldownFlow[(int)SkillSlot::Max] = {};
		// OBB.Center
		int m_currentWeaponType = 0;
		bool m_weaponHolstered = false;
		float ReloadRemain = 0.0f;
		bool m_sniperDmrMode = false;
		bool m_bomberHealAmmoMode = false;
		bool isGround = false;
		float m_yaw = 0.0f;
		float m_pitch = 0.0f;

		Weapon weapon[3];
		int SelectedWeapone;

		int Gold = 0;
		int Exp = 0;
		int Level = 0;
		int StatPoint = 0;
		int StatHP = 0;
		int StatDefense = 0;
		int StatMoveSpeed = 0;
		int StatAttack = 0;

		int clientIndex = 0;
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

enum class MonsterType : int {
	Walker,
	Dron,
	Tower,
	Max
};

struct MonsterData {
	MonsterType type;
	const char* name;
	const char* shapeName;
	float MaxHP;
	float Attack;
	float Defense;
	float MoveSpeed;
	float FireDelay;
	float DetectionRange;
	float ChaseRange;
	int ExpReward;
};

static MonsterData GMonsterTable[] = {
	{ MonsterType::Walker, "Walker", "Monster001", 40.0f, 10.0f, 5.0f, 2.0f, 1.0f, 40.0f, 8.0f, 50 },
	{ MonsterType::Dron, "Dron", "MonsterDrone", 25.0f, 8.0f, 0.0f, 3.5f, 0.8f, 40.0f, 14.0f, 70 },
	{ MonsterType::Tower, "Tower", "MonsterTurret", 90.0f, 18.0f, 25.0f, 0.0f, 1.6f, 40.0f, 22.0f, 100 },
};

inline const MonsterData& GetMonsterData(MonsterType type) {
	for (int i = 0; i < (int)MonsterType::Max; ++i) {
		if (GMonsterTable[i].type == type) return GMonsterTable[i];
	}
	return GMonsterTable[0];
}

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

	MonsterType m_monsterType = MonsterType::Walker;
	float Attack = 10.0f;

	// OBB.Center
	vec4 m_homePos;
	// OBB.Center
	vec4 m_targetPos;
	// OBB.Center
	float m_speed = 2.0f;
	// OBB.Center
	float m_patrolRange = 20.0f;
	// OBB.Center
	float m_detectionRange = 12.0f;
	float m_chaseRange = 10.0f;
	float handoffCooldown = 0.0f;
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
	// Keep the player object itself, never the address of a vecset slot. The slot array can
	// reallocate when another client joins and would leave a Player** dangling.
	Player* Target = nullptr;
	int TargetClientIndex = -1;
	Player* CurrentTarget = nullptr;
	// OBB.Center
	bool m_isMove = false;
	// OBB.Center
	bool isGround = false;
	// OBB.Center
	float respawntimer = 0;
	// ServerOnly 상태
	float pathfindTimer = 0.0f;
	float StatusRemain[(int)StatusEffectType::Max] = {};
	float StatusDuration[(int)StatusEffectType::Max] = {};
	float StatusPower[(int)StatusEffectType::Max] = {};
	float StatusTickFlow[(int)StatusEffectType::Max] = {};
	GameObject* StatusSource[(int)StatusEffectType::Max] = {};

	int tempId = 0;
	float dronYMover = -1;

	Monster();
	virtual ~Monster() {}

	virtual void Update(float deltaTime) override;
	//virtual void Render();
	virtual void OnCollision(GameObject* other) override;

	virtual void OnStaticCollision(BoundingOrientedBox obb) override;

	virtual void OnCollisionRayWithBullet(GameObject* shooter, float damage);
	void ApplyDamage(GameObject* source, float damage);
	void ApplyStatusEffect(GameObject* source, StatusEffectType type, float duration, float power);
	void UpdateStatusEffects(float deltaTime);
	bool HasStatusEffect(StatusEffectType type) const;
	void ApplyMonsterData(MonsterType type);

	void Init(const XMMATRIX& initialWorldMatrix);

	void Respawn();

	virtual BoundingOrientedBox GetOBB();

	//astar pathfinding
	vector<AstarNode*> AstarSearch(AstarNode* start, AstarNode* destination, std::vector<AstarNode*>& allNodes);
	AstarNode* FindClosestNode(float wx, float wz, const std::vector<AstarNode*>& allNodes);
	void MoveByAstar(float deltaTime);
	bool TryChaseGhost(float deltaTime, vec4 monsterPos);
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
	* BoundingOrientedBox obb : 검사할 OBB
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
	static constexpr int rbufcap = 8192;
	char rbuf[rbufcap] = {};
	int rbufoffset = 0;

	// OBB.Center
	NWAddr addr;

	// OBB.Center
	Player* pObjData;

	// OBB.Center
	int objindex = 0;

	// OBB.Center
	int zoneId = 0;

	int pendingTransferToken = 0;

	// [4단계-STEP1] 이 소켓이 게임 클라이언트가 아니라 '이웃 서버와의 상시 복제 링크'인지 표시.
	// true면 플레이어 로직(이동/전송/게임데이터 송신)에서 제외하고, 서버 간 메시지만 주고받는다.
	bool isServerPeer = false;
	int peerServerId = -1;

	// OBB.Center
	SendDataSaver PersonalSDS;
	// Per-client output queue. CommonSDS/PersonalSDS are copied here before non-blocking send,
	// so a partial WSASend cannot truncate the packet stream for only one client.
	SendDataSaver PendingSDS;
	int pendingSendOffset = 0;

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
		BOOL noDelay = TRUE;
		setsockopt(socket, IPPROTO_TCP, TCP_NODELAY,
			reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));

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


struct TalkSelection {
	const wchar_t* selectionText;
	char mod = 't'; // t : goto talk, q : quest, f : function
	void (*Function)(Player*) = nullptr; // gotoOtherTalk == false �̸�, ���ý� �� �Լ��� �����Ѵ�. (����������)
	int AddQuest = 0;
	size_t goto_otherTalkID; // gotoOtherTalk == true �̸�, ���ý� �ش� Id�� ��ȭ�� ��ȯ�ȴ�.

	TalkSelection() {
		selectionText = nullptr;
		mod = 't';
		Function = nullptr;
		goto_otherTalkID = -1;
		AddQuest = 0;
	}
	TalkSelection(const wchar_t* txt, char _mod, ui64 gotoData) {
		selectionText = txt;
		mod = _mod;
		if (mod == 't') {
			goto_otherTalkID = gotoData;
		}
		else if (mod == 'f') {
			Function = reinterpret_cast<void(*)(Player*)>(gotoData);
		}
		else if (mod == 'q') {
			AddQuest = (int)gotoData;
		}
	}
	TalkSelection(const TalkSelection& ref) {
		selectionText = ref.selectionText;
		mod = ref.mod;
		goto_otherTalkID = ref.goto_otherTalkID;
		Function = ref.Function;
		AddQuest = ref.AddQuest;
	}
};

struct NPCTalkData {
	// ����� �ؽ�Ʈ
	const wchar_t* text;
	int selectCnt = 0;
	bool NextEscape = false;
	TalkSelection sel[4];
	NPCTalkData();
	NPCTalkData(const wchar_t* txt, bool nxtEsc = false, TalkSelection sel1 = TalkSelection(), TalkSelection sel2 = TalkSelection(), TalkSelection sel3 = TalkSelection(), TalkSelection sel4 = TalkSelection());
};

class PeacefulNPC : public SkinMeshGameObject {
#define STC_CurrentStruct PeacefulNPC
	STC_STATICINIT_innerStruct;
public:
	// �� Ŭ���̾�Ʈ���� �� NPC�� ���̴��� ����
	STCDef(PeacefulNPCType, NPCType);

	// �����̸� ������ ID
	// ����Ʈ�� ��ȭâ�̸� ���۴�ȭ�� ID�� �����Ѵ�.
	STCDef(int, NPCID);

	//ServerOnly ���� ���� ����ִ����� ��Ÿ����.
	bool isGround = false;
	//ServerOnly �󸶳� ���� ���� ������Ʈ�� �浹�Ǿ������� ��Ÿ����.
	int collideCount = 0;

	vector<int> NPCQuestList;

	PeacefulNPC();
	virtual ~PeacefulNPC() {}

	/*
	* ���� : ���ӿ�����Ʈ�� ������Ʈ�� ������.
	* �Ű����� :
	* float deltaTime : ���� ������Ʈ ���� ���� ��������� �ð� ����.
	*/
	virtual void Update(float deltaTime);

	virtual void Release();

	/*
	* ���� : ���ӿ�����Ʈ�� �浹�������� ȣ��Ǵ� �Լ�.
	* �Ű����� :
	* GameObject* other : �浹�� ������Ʈ
	*/
	virtual void OnCollision(GameObject* other) override;

	/*
	* ���� : ���ӿ�����Ʈ�� �������� �ʴ� Static �浹ü�� �浹�������� ȣ��Ǵ� �Լ�.
	* �Ű����� :
	* BoundingOrientedBox other : �浹�� OBB.
	*/
	virtual void OnStaticCollision(BoundingOrientedBox obb) override;

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

	virtual void SendGameObject(int objindex, SendDataSaver& sds);

	static void PrintOffset(ofstream& ofs) {
		SkinMeshGameObject::PrintOffset(ofs);
		for (int i = 0; i < g_member.size(); ++i) {
			g_member[i].offset = g_member[i].get_offset();
			ofs << g_member[i].name << " " << g_member[i].offset << " " << g_member[i].size << endl;
		}
	}
#undef STC_CurrentStruct
};

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
	static constexpr int OffsetMulArr[3][3] = {
		{ 5, 1, 6 },
		{ 2, 0, 3 },
		{ 7, 4, 8 } };

	// OBB.Center
	World* world = nullptr;

	// 존의 이름
	char zoneName[128] = {};
	
	//존 좌표
	int x;
	int y;

	Zone(int id, const char* name, int _x, int _y) {
		zoneId = id;
		strcpy_s(zoneName, 128, name);
		x = _x;
		y = _y;
	}

	// OBB.Center
	int zoneId = 0;
	Zone* nearZones[9] = {};
	Zone* AssetOffsetToNearZone[9] = {};
	int Asset_OffsetMul = 0;

	// 주변 존 관련 데이터를 구성함.
	void BakeNear() {
		for (int i = 0; i < 9; ++i) {
			AssetOffsetToNearZone[i] = 0;
		}

		for (int i = 0; i < 9; ++i) {
			Zone* zone = nearZones[i];
			if (zone != nullptr) {
				int key = zone->Asset_OffsetMul;
				AssetOffsetToNearZone[key] = zone;
			}
		}
	}

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

	enum class BossPrototypePhase {
		FindBoss,
		MachineGun,
		MissileLock,
		RailgunCharge,
		Bombardment,
		SummonTurret,
		RotatingLaser,
		Rest,
	};

	struct BossPrototypeCore {
		vec4 Position = vec4(0, 0, 0, 1);
		float HP = 1200.0f;
		float MaxHP = 1200.0f;
		bool Active = true;
	};

	struct BossPrototypeWarning {
		int Shape = 0;
		vec4 Position = vec4(0, 0, 0, 1);
		vec4 Direction = vec4(0, 0, 1, 0);
		float Radius = 2.0f;
		float Width = 2.0f;
		float Length = 6.0f;
		float WarningTime = 1.0f;
		float Age = 0.0f;
		float Damage = 8.0f;
		float InnerDamage = 0.0f;
		float CoreDamage = 0.0f;
		float ResidualDamagePerSecond = 0.0f;
		float ResidualDuration = 0.0f;
		float ResidualTickFlow = 0.0f;
		float FollowTime = 0.0f;
		float LockTime = 0.0f;
		bool DamageApplied = false;
		bool ResidualActive = false;
		bool Active = true;
		bool FollowPlayer = false;
		bool DarkenOnLock = false;
		bool VisualSpawned = false;
	};

	bool BossPrototypeEnabled = false;
	bool BossPrototypeConfigured = false;
	bool BossPrototypeCoresInitialized = false;
	bool BossPrototypeShieldActive = true;
	int BossPrototypeIndex = -1;
	int BossPrototypePatternStep = 0;
	int BossPrototypeMachineGunShots = 0;
	int BossPrototypeSummonCount = 0;
	BossPrototypePhase BossPrototypePhaseState = BossPrototypePhase::FindBoss;
	float BossPrototypePhaseTime = 0.0f;
	float BossPrototypePatternCooldown = 0.0f;
	float BossPrototypeShieldDownTime = 0.0f;
	float BossPrototypeGroggyTime = 0.0f;
	float BossPrototypeMachineGunCooldown = 0.0f;
	float BossPrototypeMissileCooldown = 0.0f;
	float BossPrototypeRailgunCooldown = 0.0f;
	float BossPrototypeBombardmentCooldown = 0.0f;
	float BossPrototypeSummonCooldown = 0.0f;
	float BossPrototypeRotatingLaserCooldown = 0.0f;
	float BossPrototypeMachineGunShotFlow = 0.0f;
	float BossPrototypeRotatingLaserHitFlow = 0.0f;
	float BossPrototypeRotatingLaserBaseAngle = 0.0f;
	int BossPrototypeRotatingLaserStep = 0;
	int BossPrototypeRotatingLaserMode = 0;
	float BossPrototypeRotatingLaserDirectionSign = 1.0f;
	vec4 BossPrototypeCenter = vec4(0, 0, 0, 1);
	vec4 BossPrototypeAimDirection = vec4(0, 0, 1, 0);
	vec4 BossPrototypeRailgunDirection = vec4(0, 0, 1, 0);
	vector<BossPrototypeCore> BossPrototypeCores;
	vector<BossPrototypeWarning> BossPrototypeWarnings;

	// �� ����
	static constexpr float lowFrequencyDelay = 3.0f;
	float lowFrequencyFlow = 0.0f;
	__forceinline bool lowHit() {
		return lowFrequencyFlow > lowFrequencyDelay;
	}

	static constexpr float midFrequencyDelay = 0.2f;
	float midFrequencyFlow = 0.0f;
	__forceinline bool midHit() {
		return midFrequencyFlow > midFrequencyDelay;
	}

	static constexpr float highFrequencyDelay = 0.05f;
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

	vec4 BasicAABB_onlyXZ = 0;

	static constexpr float MinimumCenter = -1575;
	static constexpr float ZoneWidth = 350;
	static constexpr float ZoneHalfWidth = ZoneWidth / 2;

	Zone() : world(nullptr), zoneId(0) {}
	Zone(World* w, int id, int _x, int _y) : world(w), zoneId(id), x{ _x }, y{_y} {
		BasicAABB_onlyXZ = vec4(
			MinimumCenter + ZoneWidth * _x - ZoneHalfWidth,
			MinimumCenter + ZoneWidth * _y - ZoneHalfWidth,
			MinimumCenter + ZoneWidth * _x + ZoneHalfWidth,
			MinimumCenter + ZoneWidth * _y + ZoneHalfWidth);
	}

	void Set_world_id_pos(World* w, int id, int _x, int _y);

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
	// fullWorldSnapshot: true=최초 입장/서버전송(클라가 빈 상태 → 전체 객체 전송)
	//                    false=심리스 로컬 이동(클라가 이미 인접 존 객체 보유 → 무거운 동적객체 재전송 생략)
	int AddPlayer(int clientIndex, Player* player, vec4 spawnPos, bool update_Map = true, bool fullWorldSnapshot = true);

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
	// 글로벌 휴머노이드 애니메이션 카운트
	*/
	// includeDynamicObjects: false면 동적 객체(몬스터/플레이어 등) 재전송을 생략한다.
	// (심리스 이동 시 클라가 이미 보유 → 끊김 유발하는 스킨메시 버스트를 막음. 아이템/포탈은 가벼우니 항상 전송)
	void SendingAllObjectForNewClient(SendDataSaver& sds, bool includeDynamicObjects = true, int excludeDynamicIndex = -1);

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
		header.zoneId = zoneId;
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
	void Sending_PlayerFire(SendDataSaver& sds, int objIndex, unsigned char fireHand = 0);
	void Sending_SkillCast(SendDataSaver& sds, int ownerObjIndex, PlayerJob job, SkillSlot slot, SkillEffectType effectType, vec4 position, vec4 direction, float radius, float power, float duration);
	void Sending_StatusEffect(SendDataSaver& sds, int targetObjIndex, int sourceObjIndex, StatusEffectType statusType, bool active, float duration, float remainTime, float power, vec4 position, vec4 extents);
	void Sending_BossState(SendDataSaver& sds);
	void Sending_NPCStartTalk(SendDataSaver& sds, PeacefulNPCType npctype, int StartID);
	void Sending_AddQuest(SendDataSaver& sds, int questID);
	void Sending_DeleteQuest(SendDataSaver& sds, int questID);
	void Sending_SyncQuestPrograss(SendDataSaver& sds, int questID, Quest* prograss);

	// OBB.Center

	bool HasStaticLineOfSight(vec4 rayStart, vec4 rayEnd);
	void FireRaycast(GameObject* shooter, vec4 rayStart, vec4 rayDirection, float rayDistance, float damage);
	void FirePiercingRaycast(GameObject* shooter, vec4 rayStart, vec4 rayDirection, float rayDistance, float damage);
	int ApplySkillDamage(GameObject* caster, SkillEffectType effectType, vec4 position, vec4 direction, float range, float radius, float damage, std::vector<GameObject*>* hitTargets = nullptr);
	void UpdateBossPrototype(float deltaTime);
	bool DamageBossCoreByRay(vec4 rayStart, vec4 rayDirection, float maxDistance, float damage, float& hitDistance);

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
	//static constexpr int zoneCount = 2;
	//const char* ZoneMapName[zoneCount] = {
	//	"Zone_4_8",
	//	"OfficeDungeon_1floor",
	//};
	vector<Zone*> ZoneTable;
	GameMap commonMap;
	int serverId = 0;
	unsigned short listenPort = 9000;
	int ownedZoneId = 0;

	bool singleProcessAllZones = false;
	bool NotMakePeer = false;
    
	unordered_map<int, PlayerTransferData> pendingTransfers;
    int nextTransferToken = 1;

	// [4단계-STEP1] 이웃 존 서버와의 상시 복제 링크 상태. 인덱스 = 이웃 zoneId(=serverId).
	// true면 그 이웃과 링크가 연결돼 있다는 뜻. (멀티 프로세스에서만 사용)
	bool peerLinkUp[256] = {};
	SOCKET pendingPeerSocket[256] = {};
	bool pendingPeerConnect[256] = {};
	ULONGLONG pendingPeerStartMs[256] = {};
	ULONGLONG peerRetryAfterMs[256] = {};

	// 아직 연결 안 된 이웃에 주기적으로 재접속 시도. (이웃 서버가 늦게 떠도 따라잡음)
	void TryConnectPeers();
	void PumpPeerConnections();
	bool CompleteOutboundPeerConnection(int zoneId, SOCKET socket);
	// 이웃이 보낸 ServerLink 핸드셰이크를 받았을 때 그 소켓을 peer 링크로 확정.
	void OnPeerLinkEstablished(int clientIndex, int fromServerId);

	// Ghost object indexes are local to each zone. Keep the source zone in the key so equal indexes
	// from different peers cannot overwrite or delete one another.
	unordered_map<unsigned long long, DynamicGameObject*> ghostPlayers;
	static unsigned long long MakeGhostKey(int zoneId, int objindex) {
		return (static_cast<unsigned long long>(static_cast<unsigned int>(zoneId)) << 32) |
			static_cast<unsigned int>(objindex);
	}
	static int GhostKeyZone(unsigned long long key) { return static_cast<int>(key >> 32); }
	static int GhostKeyObject(unsigned long long key) { return static_cast<int>(key & 0xffffffffULL); }
	static bool ArePlayersAllies(const Player* source, const Player* target);
	// 내 존 플레이어들의 상태를 peer 링크로 이웃에 전송(매 틱).
	void SendGhostToPeers();
	bool QueuePeerPacket(int peerClientIndex, const void* data, int size);
	void FlushPeerSends();
	// 이웃이 보낸 GhostSync 를 받아 고스트를 갱신/생성/제거하고, 그 변화를 내 클라들에게 중계.
	void OnGhostSync(char* data);
	// 고스트를 맞혔을 때, 그 객체를 소유한 서버로 데미지 적용 요청을 보낸다.
	void SendGhostDamageToOwner(int targetZoneId, int targetObjIndex, float damage, int sourcePartyId = -1);
	// 몬스터가 경계를 넘었을 때 소유권을 이웃 서버로 넘긴다.
	bool SendMonsterHandoff(int dstZoneId, Monster* m);
	// 이웃이 넘긴 몬스터를 내 존에 진짜 몬스터로 생성한다.
	void SpawnHandoffMonster(int monsterType, vec4 pos, float hp, float maxhp);

	// 서버에서 돌아갈 Zone의 포함영역. singleProcessAllZones == true 일때 만 적용됨.
	static constexpr int minx = 3, miny = 7;
	static constexpr int maxx = 4, maxy = 8;
	// 존이 서버에서 돌아가고 있는지 체크
	bool IsZoneOwned(int zoneId) const {
		if (zoneId < 0 || zoneId >= ZoneTable.size()) return false;
		if (singleProcessAllZones) {
			Zone* zptr = ZoneTable[zoneId];
			bool b = (minx <= zptr->x && zptr->x <= maxx);
			b = b && (miny <= zptr->y && zptr->y <= maxy);
			return b;
		}
		// [party/dungeon] the dungeon server owns EVERY instance's floor zones (100..100+DungeonZoneSpan-1),
		// so floor-to-floor moves AND cross-instance re-homing are same-server (local) zone moves.
		if (IsDungeonZone(ownedZoneId)) {
			return IsDungeonZone(zoneId);
		}
		return zoneId == ownedZoneId;
	}

	// A, B의 ID를 가진 두 존이 인접해 있는지를 체크
	bool IsAdjacentZone(int zoneA, int zoneB) const {
		if (zoneA < 0 || zoneA >= ZoneTable.size()) return false;
		if (zoneB < 0 || zoneB >= ZoneTable.size()) return false;
		if (zoneA == zoneB) return true;
		bool b = abs(ZoneTable[zoneA]->x - ZoneTable[zoneB]->x) <= 1;
		b = b && (abs(ZoneTable[zoneA]->y - ZoneTable[zoneB]->y) <= 1);
		return b;
	}

	Zone* GetStaticChunkZone() {
		if (ZoneTable.empty()) return nullptr;
		int staticZoneId = singleProcessAllZones ? 0 : ownedZoneId;
		if (staticZoneId < 0 || staticZoneId >= (int)ZoneTable.size()) return nullptr;
		return ZoneTable[staticZoneId];
	}

	GameMap& GetCommonMap() {
		return commonMap;
	}

	void InitCommonMap();

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
	* 설명 : 클라이언트 패킷을 처리한다.
	*/
	__forceinline int Receiving(int clientIndex, char* rBuffer, int totallen);

	/*
	* 설명 : 클라이언트에게 할당된 플레이어 인덱스를 전송한다.
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

	__forceinline void Sending_InitialSyncComplete(SendDataSaver& sds, int zoneId, int objindex) {
		sds.postpush_start();
		constexpr int reqsiz = sizeof(STC_InitialSyncComplete_Header);
		sds.postpush_reserve(reqsiz);
		STC_InitialSyncComplete_Header& header = *(STC_InitialSyncComplete_Header*)sds.ofbuff;
		header.size = reqsiz;
		header.st = STC_Protocol::InitialSyncComplete;
		header.zoneId = zoneId;
		header.playerObjIndex = objindex;
		sds.postpush_end();
	}

	__forceinline void Sending_JobChangeAck(SendDataSaver& sds, PlayerJob job, int weaponType) {
		sds.postpush_start();
		constexpr int reqsiz = sizeof(STC_JobChangeAck_Header);
		sds.postpush_reserve(reqsiz);
		STC_JobChangeAck_Header& header = *(STC_JobChangeAck_Header*)sds.ofbuff;
		header.size = reqsiz;
		header.st = STC_Protocol::JobChangeAck;
		header.job = (int)job;
		header.weaponType = weaponType;
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

	// [party/dungeon] Write a party/queue roster snapshot (per-member objindex, HP, MaxHP, job) into a
	// client's send buffer. Pass the member list to send: the waiting queue (count<=DungeonPartyMax).
	__forceinline void Sending_DungeonQueueUpdate(SendDataSaver& sds, const int* members, int count,
		int leaderClientIndex = -1, int partyId = -1, int number = 0,
		int dungeonDeathCount = -1, int dungeonDeathLimit = 3) {
		sds.postpush_start();
		constexpr int reqsiz = sizeof(STC_DungeonQueueUpdate_Header);
		sds.postpush_reserve(reqsiz);
		STC_DungeonQueueUpdate_Header& header = *(STC_DungeonQueueUpdate_Header*)sds.ofbuff;
		header.size = reqsiz;
		header.st = STC_Protocol::DungeonQueueUpdate;
		header.count = count;
		header.maxCount = DungeonPartyMax;
		header.leaderClientIndex = leaderClientIndex;
		header.partyId = partyId;
		header.number = number;
		header.dungeonDeathCount = dungeonDeathCount;
		header.dungeonDeathLimit = dungeonDeathLimit;
		for (int i = 0; i < DungeonPartyMax; ++i) {
			int ci = (members != nullptr && i < count) ? members[i] : -1;
			if (ci >= 0 && ci < clients.size && clients.isnull(ci) == false && clients[ci].pObjData != nullptr) {
				header.objindex[i] = clients[ci].objindex;
				header.zoneId[i] = clients[ci].zoneId;
				header.hp[i] = clients[ci].pObjData->HP;
				header.maxhp[i] = clients[ci].pObjData->MaxHP;
				header.m_currentJob[i] = clients[ci].pObjData->m_currentJob;
			} else {
				header.objindex[i] = -1;
				header.zoneId[i] = -1;
				header.hp[i] = 0;
				header.maxhp[i] = 0;
				header.m_currentJob[i] = -1;
			}
		}
		sds.postpush_end();
	}

	__forceinline void Sending_DungeonResult(SendDataSaver& sds, DungeonResultCode result,
		int deathCount, int deathLimit) {
		sds.postpush_start();
		constexpr int reqsiz = sizeof(STC_DungeonResult_Header);
		sds.postpush_reserve(reqsiz);
		STC_DungeonResult_Header& header = *(STC_DungeonResult_Header*)sds.ofbuff;
		header.size = reqsiz;
		header.st = STC_Protocol::DungeonResult;
		header.result = result;
		header.deathCount = deathCount;
		header.deathLimit = deathLimit;
		sds.postpush_end();
	}

	// [party] write the current open-party list into a client's buffer (the "join party" window renders it).
	void Sending_PartyList(SendDataSaver& sds);

	// [party] tell a leader their start request was refused (every dungeon instance is busy).
	__forceinline void Sending_DungeonReject(SendDataSaver& sds, int reason) {
		sds.postpush_start();
		constexpr int reqsiz = sizeof(STC_DungeonReject_Header);
		sds.postpush_reserve(reqsiz);
		STC_DungeonReject_Header& header = *(STC_DungeonReject_Header*)sds.ofbuff;
		header.size = reqsiz;
		header.st = STC_Protocol::DungeonReject;
		header.reason = reason;
		sds.postpush_end();
	}

	// [party/dungeon][phase2] Tell a client to portal-teleport (fresh connect) to the dungeon server at ip:port.
	__forceinline void Sending_DungeonEnter(SendDataSaver& sds, const char* ip, unsigned short port, int dstZoneId) {
		sds.postpush_start();
		constexpr int reqsiz = sizeof(STC_DungeonEnter_Header);
		sds.postpush_reserve(reqsiz);
		STC_DungeonEnter_Header& header = *(STC_DungeonEnter_Header*)sds.ofbuff;
		header.size = reqsiz;
		header.st = STC_Protocol::DungeonEnter;
		header.port = port;
		header.dstZoneId = dstZoneId;
		strncpy_s(header.ip, ip, _TRUNCATE);
		sds.postpush_end();
	}


	/*
	* 설명 : 현재 게임 상태를 전송한다.
	*/
	__forceinline void Sending_SyncGameState(SendDataSaver& sds) {
		sds.postpush_start();
		constexpr int reqsiz = sizeof(STC_SyncGameState_Header);
		sds.postpush_reserve(reqsiz);
		STC_SyncGameState_Header& header = *(STC_SyncGameState_Header*)sds.ofbuff;
		header.size = reqsiz;
		header.st = STC_Protocol::SyncGameState;
		header.DynamicGameObjectCapacityPerZone = NetworkDynamicObjectCapacityPerZone;
		header.ZoneCount = ZoneTable.size();
		header.DynamicGameObjectCapacity = header.DynamicGameObjectCapacityPerZone * header.ZoneCount;
		Zone* staticZone = GetStaticChunkZone();
		header.StaticGameObjectCapacity = staticZone != nullptr ? staticZone->Static_gameObjects.Capacity : 0;
		sds.postpush_end();
	}

	/*
	* 설명 : 플레이어를 srcZone에서 dstZone으로 이동시킨다.
	*/
	// partyId/originZoneId are only meaningful for dungeon-entry moves (carried into the transfer so the
	// dungeon server can group party members into one instance and bounce them back if it is full).
	bool MovePlayerToZone(int clientIndex, int dstZoneId, vec4 spawnPos, int partyId = -1, int originZoneId = -1);
	// Open-world deaths use one code-configured world position. The X/Z coordinate decides which
	// of the four main-map zone servers owns the respawn point.
	int ResolveMainMapRespawnZone(const vec4& respawnPosition) const;
	void RespawnPlayerAtMainMapPoint(int clientIndex);

	// [party/dungeon] portal waiting-queue. Up to DungeonPartyMax players gather here.
	static constexpr int DungeonPartyMax = 3;
	// [party/dungeon][phase2] the dungeon is a special zone OUTSIDE the open-world grid (ids 0..99),
	// served by its own process on port 9000+DungeonZoneId. Coords are far away so it is never adjacent.
	static constexpr int DungeonZoneId = 100;
	// [party/dungeon] dungeon floors: 1F + 2F + boss room. One dungeon server owns ALL instances' floors.
	static constexpr int DungeonFloorCount = 3;
	// [party] number of independent dungeon copies (instances). Each instance = a block of DungeonFloorCount
	// consecutive zone ids. Instance i occupies zones [DungeonZoneId + i*DungeonFloorCount, +DungeonFloorCount).
	// With DungeonInstanceCount=2 the dungeon process owns zones 100..105 (inst0:100-102, inst1:103-105).
	static constexpr int DungeonInstanceCount = 2;
	static constexpr int DungeonZoneSpan = DungeonInstanceCount * DungeonFloorCount; // total dungeon zones

	// --- dungeon zone-id helpers (instance math) ---
	static bool IsDungeonZone(int zoneId) {
		return zoneId >= DungeonZoneId && zoneId < DungeonZoneId + DungeonZoneSpan;
	}
	static int DungeonInstanceBase(int instanceIndex) {
		return DungeonZoneId + instanceIndex * DungeonFloorCount;
	}
	static int DungeonInstanceIndexOf(int zoneId) {
		return IsDungeonZone(zoneId) ? (zoneId - DungeonZoneId) / DungeonFloorCount : -1;
	}
	static int DungeonInstanceBaseOf(int zoneId) {
		int idx = DungeonInstanceIndexOf(zoneId);
		return idx < 0 ? -1 : DungeonInstanceBase(idx);
	}
	static int DungeonFloorOf(int zoneId) {
		return IsDungeonZone(zoneId) ? (zoneId - DungeonZoneId) % DungeonFloorCount : -1;
	}

	// --- [party] lobby-side party formation (lives on the open-world server that owns the portal) ---
	struct LobbyParty {
		int id = -1;                                  // unique party id (serverId-scoped)
		int number = 0;                               // display number -> "파티N"
		int leaderClientIndex = -1;
		int members[DungeonPartyMax] = { -1, -1, -1 };
		int memberCount = 0;
		bool started = false;                         // true once routed into a dungeon instance
		bool active = false;
	};
	static constexpr int MaxParties = MaxPartyListEntries;
	LobbyParty parties[MaxParties];
	int nextPartyNumber = 1;
	int nextPartyId = 1;

	int PartyFindSlotByClient(int clientIndex);       // party array index containing this client, or -1
	int PartyFindSlotById(int partyId);               // party array index with this id, or -1
	int PartyAllocSlot();                             // first inactive party slot, or -1 if full
	void PartyCreate(int clientIndex);                // make a party (caller = leader + first member)
	void PartyJoin(int clientIndex, int partyId);     // join an open party
	void PartyLeave(int clientIndex);                 // leave current party (disband/promote as needed)
	void PartyDisband(int clientIndex);               // leader-only: destroy the whole party (kick all members)
	void PartyLeaderStart(int clientIndex);           // leader pressed start -> route members to a free instance
	void BroadcastPartyRoom(int partySlot);           // push STC_DungeonQueueUpdate (HP/job/leader) to members
	void BroadcastPartyListToMembers();               // push STC_PartyList to everyone currently in a party

	// --- [party] dungeon-side instance pool (lives on the dungeon server, ownedZoneId in dungeon range) ---
	struct DungeonInstanceSlot {
		bool busy = false;
		int partyId = -1;       // which lobby party currently occupies this instance
		int number = 0;         // that party's display number (logging only)
		int deathCount = 0;
		int deathLimit = 3;
		DungeonResultCode result = DungeonResultCode::None;
		bool exitPending = false;
		float exitRetryTimer = 0.0f;
	};
	DungeonInstanceSlot dungeonInstances[DungeonInstanceCount];
	int DungeonAllocInstanceForParty(int partyId, int number); // returns instance index, or -1 if all busy
	int DungeonInstanceForPartyId(int partyId);                 // existing instance index for a party, or -1
	int CountPlayersInInstance(int instanceIndex);              // live players across that instance's floors
	void DungeonUpdateInstances();                              // free + reset instances whose floors emptied
	void ResetDungeonInstance(int instanceIndex);               // respawn monsters / clear drops on its floors
	void RegisterDungeonDeath(int clientIndex);
	void RequestDungeonAbort(int clientIndex);
	void CompleteDungeonForZone(int zoneId);
	void BeginDungeonExit(int instanceIndex, DungeonResultCode result);
	void ProcessDungeonExits();

	// legacy single-queue API kept for the in-dungeon HP/job share; portal no longer auto-queues.
	void BroadcastDungeonParty();              // [party] in-dungeon: share in-dungeon players' HP/job with each other

	/*
	* 설명 : 플레이어가 속한 Zone을 얻는다.
	*/
	Zone* GetClientZone(int clientIndex);

	/*
	* 설명 : zoneId에 해당하는 Zone을 얻는다.
	*/
	Zone* GetZone(int zoneId) {
		if (zoneId < 0 || zoneId >= ZoneTable.size()) return nullptr;
		if (IsZoneOwned(zoneId) == false) return nullptr;
		return ZoneTable[zoneId];
	}

	unsigned short GetZonePort(int zoneId) const { return (unsigned short)(9000 + zoneId); }
	string zoneServerIP = "127.0.0.1";
	const char* GetZoneIP(int zoneId) const { return zoneServerIP.c_str(); }
	int IssueTransferToken() {
		// A transfer token crosses server boundaries, so a per-process sequence alone can collide.
		// Keep the sign bit clear and reserve the upper 7 bits for the issuing server id.
		const unsigned int issuer = (unsigned int)(serverId & 0x7f);
		const unsigned int sequence = (unsigned int)(nextTransferToken++ & 0x00ffffff);
		return (int)((issuer << 24) | (sequence == 0 ? 1u : sequence));
	}
	bool SendPlayerTransferToServer(const PlayerTransferData& data);
	void AcceptClientHello(int clientIndex);
	bool AcceptTransferConnect(int clientIndex, int transferToken);
	void StoreIncomingPlayerTransfer(const PlayerTransferData& data);

	void PrintOffset();

	//NPC Talk Data
	vector<NPCTalkData> NPCTalkTable;

	//Quest Table
	vector<Quest*> QuestTable;

	int GetStartTalk(Player* p, PeacefulNPC* npc);

#pragma region TimeMeasureCode
	static constexpr int TimeMeasureSamepleCount = 32;
	static constexpr int MeasureCount = 1024;
	ui64 tickStack[MeasureCount] = {};
	ui64 tickMin[MeasureCount] = {};
	ui64 tickMax[MeasureCount] = {};
	ui64 ft[MeasureCount] = {};
	ui32 cnt[MeasureCount] = {};
	__forceinline void TIMER_STATICINIT() {
		for (int i = 0; i < MeasureCount; ++i) {
			tickMin[i] = numeric_limits<unsigned long long>::lowest() - 1;
		}
	}
	__forceinline void AverageClockStart(int id = 0) {
		unsigned int aux = 0;
		ft[id] = __rdtscp(&aux);
	}
	__forceinline void AverageClockEnd(int id = 0) {
		unsigned int aux = 0;
		ui64 et = __rdtscp(&aux) - 33;
		cnt[id] += 1;
		ui64 del = et - ft[id];
		tickMin[id] = min(tickMin[id], del);
		tickMax[id] = max(tickMax[id], del);
		tickStack[id] += del;
		if (cnt[id] > TimeMeasureSamepleCount) {
			ui64 avrtick = tickStack[id] / TimeMeasureSamepleCount;
			cout << "Clock Interval [" << id << "] : Avg : " << avrtick << "clock \t Min : " << tickMin[id] << " clock \n Max : " << tickMax[id] << " clock \n" << endl;
			tickStack[id] = 0;
			cnt[id] = 0;
		}
	}

	__forceinline void AverageTickStart(int id = 0) {
		ft[id] = GetTicks();
	}
	__forceinline void AverageTickEnd(int id = 0) {
		ui64 et = GetTicks();
		cnt[id] += 1;
		ui64 del = et - ft[id];
		tickMin[id] = min(tickMin[id], del);
		tickMax[id] = max(tickMax[id], del);
		tickStack[id] += del;
		if (cnt[id] > TimeMeasureSamepleCount) {
			ui64 avrtick = tickStack[id] / TimeMeasureSamepleCount;
			cout << "Tick Interval [" << id << "] : Avg : " << avrtick << "tick \t Min : " << tickMin[id] << " tick \n Max : " << tickMax[id] << " tick \n" << endl;
			tickStack[id] = 0;
			cnt[id] = 0;
		}
	}

	__forceinline void AverageSecPer60Start(int id = 0) {
		ft[id] = GetTicks();
	}
	__forceinline void AverageSecPer60End(int id = 0) {
		ui64 et = GetTicks();
		cnt[id] += 1;
		ui64 del = et - ft[id];
		tickMin[id] = min(tickMin[id], del);
		tickMax[id] = max(tickMax[id], del);
		tickStack[id] += del;
		if (cnt[id] > TimeMeasureSamepleCount) {
			double average = 60.0 * (double)(tickStack[id] / TimeMeasureSamepleCount) / (double)QUERYPERFORMANCE_HZ;
			double Min = 60.0 * ((double)tickMin[id] / (double)QUERYPERFORMANCE_HZ);
			double Max = 60.0 * ((double)tickMax[id] / (double)QUERYPERFORMANCE_HZ);
			cout << "Tick Interval [" << id << "] : Avg : " << average << "step \t Min : " << Min << " step \n Max : " << Max << " step \n" << endl;
			tickStack[id] = 0;
			cnt[id] = 0;
		}
	}
#pragma endregion
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

#pragma once
#include "stdafx.h"

/*
* 설명 : Mesh의 충돌처리 OBB 정보를 저장.
* Sentinal Value : 
* NULL = (MAXpos.x < 0 || MAXpos.y < 0 || MAXpos.z < 0);
*/
struct Mesh {
	// OBB.Extends
	vec4 MAXpos;
	// OBB.Center
	vec4 Center;
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
	// 모델노드의 기본 변환행렬
	XMMATRIX transform;
	// 부모노드
	ModelNode* parent;
	// 자식노드의 개수
	unsigned int numChildren;
	// 자식노드 포인터들의 배열
	ModelNode** Childrens;
	// 해당 노드가 가진 Mesh의 개수
	unsigned int numMesh;
	// 해당 노드가 가진 Mesh index들의 배열
	// 해당 index는 Model::mMeshes 배열을 통해 Mesh를 얻을 수 있다.
	unsigned int* Meshes;

	// 메쉬에 입힐 머터리얼의 인덱스 배열
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
	static vector<ModelNode*> NodeArr;
	static unordered_map<void*, int> nodeindexmap;
	
	// 모델 노드들의 배열
	ModelNode* Nodes;

	// 모델이 가진 메쉬의 개수
	unsigned int mNumMeshes;
	
	// 모델이 가진 메쉬들의 포인터 배열
	Mesh** mMeshes;

	// 모델의 UnitScaleFactor
	float UnitScaleFactor = 1.0f;

	// 모델의 기본상태에서 모델을 모두 포함하는 가장 작은 AABB.
	vec4 AABB[2];
	// 모델의 OBB.Center
	vec4 OBB_Tr;
	// 모델의 OBB.Extends
	vec4 OBB_Ext;

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
	// 만약 Tag_Dynamic == true 라면.
	Tag_SkinMeshObject = 3,
	// 만약 Tag_Dynamic == false 라면.
};

struct GameObject {
#define STC_CurrentStruct GameObject
	STC_STATICINIT_innerStruct;

	/////////////////
	// 청크를 통해 한 틱에 게임오브젝트 당 한번씩 해야하는 작업이 있다면 이 값을 사용하자.
	UINT TourID = 0;

	/*
	* 게임오브젝트를 구분하고 탐색하기 위한 tag. 32개의 tag를 보유할 수 있다.
	* 항상 tag의 첫번째 비트는 enable이다. (게임오브젝트가 활성화 되어있는지 여부)
	*/
	STCDef(Tag , tag);

	// appearance
	STCDef(int, shapeindex);
	
	// 계층구조 - GameObject의 인덱스를 사용한다. vector<GameObject*> ObjectTable;
	STCDef(int, parent);
	STCDef(int, childs);
	STCDef(int, sibling);

	// 이것도 서버에 필요할 것 같다.
	//STC
	union {
		int* material = nullptr; // mesh 일 경우에만 활성화됨. slotNum만큼. game.MaterialTable에서 접근.
		matrix* transforms_innerModel; // model 일 경우만 활성화됨. nodeCount 만큼.
	};
	inline static OffsetRegister reg_material{ "material", offset_of(&GameObject::material), sizeof(int*) };
	inline static OffsetRegister reg_transforms_innerModel{ "transforms_innerModel", offset_of(&GameObject::transforms_innerModel), sizeof(matrix*) };

	// transform
	STCDef(matrix, worldMat);

	//처음 생성 시에는 enable이 false. 업데이트를 하지 않는다. 
	// 어느정도 초기화가 완료되어 게임루프에 들어갈 준비를 마치면 되면 그때 Enable을 한다.
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

	virtual void SendGameObject(int objindex, SendDataSaver& sds) {
		sds.postpush_start();

		//calculate app packet siz.
		int reqsiz = sizeof(STC_SyncGameObject_Header) + sizeof(STC_SyncObjData);
		Shape& s = Shape::ShapeTable[shapeindex];
		Mesh* mesh = nullptr; 
		Model* model = nullptr; 
		s.GetRealShape(mesh, model);
		if (mesh) reqsiz += sizeof(int) + sizeof(int) * mesh->subMeshNum;
		else reqsiz += sizeof(int) + sizeof(matrix) * model->nodeCount;

		sds.postpush_reserve(reqsiz);
		int offset = 0;
		
		//static pushv
		STC_SyncGameObject_Header& header = *(STC_SyncGameObject_Header*)sds.ofbuff;
		header.size = reqsiz;
		header.st = SendingType::SyncGameObject;
		header.objindex = objindex;
		header.type = GameObjectType::_GameObject;
		offset += sizeof(STC_SyncGameObject_Header);

		STC_SyncObjData& static_data = *(STC_SyncObjData*)(sds.ofbuff + offset);
		static_data.tag = tag;
		static_data.shapeindex = shapeindex;
		static_data.parent = parent;
		static_data.childs = childs;
		static_data.sibling = sibling;
		static_data.worldMatrix = worldMat;
		offset += sizeof(STC_SyncObjData);

		//dynamic push
		if (mesh) {
			int& submeshNum = *(int*)(sds.ofbuff + offset);
			submeshNum = mesh->subMeshNum; 
			offset += sizeof(int);

			int*& MaterialArr = *(int**)(sds.ofbuff + offset);
			memcpy(MaterialArr, material, sizeof(int) * mesh->subMeshNum);
			offset += sizeof(int) * mesh->subMeshNum;
		}
		else {
			int& nodecount = *(int*)(sds.ofbuff + offset);
			nodecount = model->nodeCount;
			offset += sizeof(int);

			matrix* InnerModelTransformArr = (matrix*)(sds.ofbuff + offset);
			memcpy(InnerModelTransformArr, transforms_innerModel, sizeof(matrix) * model->nodeCount);
			offset += sizeof(matrix) * model->nodeCount;
		}

		sds.postpush_end();
	}

	static void PrintOffset(ofstream& ofs) {
		for (int i = 0;i < g_members.size();++i) {
			ofs << g_members[i].name << " " << g_members[i].offset << " " << g_members[i].size << endl;
		}
	}
#undef STC_CurrentStruct
};

struct StaticGameObject : public GameObject {
	StaticGameObject();
	virtual ~StaticGameObject();
	vector<BoundingBox> aabbArr;

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

	virtual void SendGameObject(int objindex, SendDataSaver& sds) {
		sds.postpush_start();

		//calculate app packet siz.
		int reqsiz = sizeof(STC_SyncGameObject_Header) + sizeof(STC_SyncObjData);
		Shape& s = Shape::ShapeTable[shapeindex];
		Mesh* mesh = nullptr; 
		Model* model = nullptr; 
		s.GetRealShape(mesh, model);
		if (mesh) reqsiz += sizeof(int) + sizeof(int) * mesh->subMeshNum;
		else reqsiz += sizeof(int) + sizeof(matrix) * model->nodeCount;
		reqsiz += sizeof(int) + aabbArr.size() * sizeof(XMFLOAT3) * 2;

		sds.postpush_reserve(reqsiz);
		int offset = 0;

		//static pushv
		STC_SyncGameObject_Header& header = *(STC_SyncGameObject_Header*)sds.ofbuff;
		header.size = reqsiz;
		header.st = SendingType::SyncGameObject;
		header.objindex = objindex;
		header.type = GameObjectType::_StaticGameObject;
		offset += sizeof(STC_SyncGameObject_Header);

		STC_SyncObjData& static_data = *(STC_SyncObjData*)(sds.ofbuff + offset);
		static_data.tag = tag;
		static_data.shapeindex = shapeindex;
		static_data.parent = parent;
		static_data.childs = childs;
		static_data.sibling = sibling;
		static_data.worldMatrix = worldMat;
		offset += sizeof(STC_SyncObjData);

		//dynamic push
		if (mesh) {
			int& submeshNum = *(int*)(sds.ofbuff + offset);
			submeshNum = mesh->subMeshNum;
			offset += sizeof(int);

			int*& MaterialArr = *(int**)(sds.ofbuff + offset);
			memcpy(MaterialArr, material, sizeof(int) * mesh->subMeshNum);
			offset += sizeof(int) * mesh->subMeshNum;
		}
		else {
			int& nodecount = *(int*)(sds.ofbuff + offset);
			nodecount = model->nodeCount;
			offset += sizeof(int);

			matrix* InnerModelTransformArr = (matrix*)(sds.ofbuff + offset);
			memcpy(InnerModelTransformArr, transforms_innerModel, sizeof(matrix) * model->nodeCount);
			offset += sizeof(matrix) * model->nodeCount;
		}

		int& aabbCount = *(int*)(sds.ofbuff + offset);
		aabbCount = aabbArr.size();
		for (int i = 0;i < aabbArr.size();++i) {
			XMFLOAT3& Center = *(XMFLOAT3*)(sds.ofbuff + offset);
			Center = aabbArr[i].Center;
			offset += sizeof(XMFLOAT3);

			XMFLOAT3& Extents = *(XMFLOAT3*)(sds.ofbuff + offset);
			Extents = aabbArr[i].Extents;
			offset += sizeof(XMFLOAT3);
		}
		sds.postpush_end();
	}

	static void PrintOffset(ofstream& ofs) {
		GameObject::PrintOffset(ofs);
		for (int i = 0;i < g_members.size();++i) {
			ofs << g_members[i].name << " " << g_members[i].offset << " " << g_members[i].size << endl;
		}
	}
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

	//ServerOnly
	vec4 tickLVelocity;
	vec4 tickAVelocity;
	vec4 LastQuerternion;
	GameObjectIncludeChunks IncludeChunks;
	int* chunkAllocIndexs = nullptr;
	int chunkAllocIndexsCapacity = 8;

	//STC
	STCDef(vec4, LVelocity);

	void InitialChunkSetting();
	void Move(vec4 velocity, vec4 Q);
	void Move(vec4 velocity, vec4 Q, GameObjectIncludeChunks afterChunkInc);
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

	virtual void SendGameObject(int objindex, SendDataSaver& sds) {
		sds.postpush_start();

		//calculate app packet siz.
		int reqsiz = sizeof(STC_SyncGameObject_Header) + sizeof(STC_SyncObjData);
		Shape& s = Shape::ShapeTable[shapeindex];
		Mesh* mesh = nullptr; 
		Model* model = nullptr; 
		s.GetRealShape(mesh, model);
		if (mesh) reqsiz += sizeof(int) + sizeof(int) * mesh->subMeshNum;
		else reqsiz += sizeof(int) + sizeof(matrix) * model->nodeCount;

		sds.postpush_reserve(reqsiz);
		int offset = 0;

		//static pushv
		STC_SyncGameObject_Header& header = *(STC_SyncGameObject_Header*)sds.ofbuff;
		header.size = reqsiz;
		header.st = SendingType::SyncGameObject;
		header.objindex = objindex;
		header.type = GameObjectType::_DynamicGameObject;
		offset += sizeof(STC_SyncGameObject_Header);

		STC_SyncObjData& static_data = *(STC_SyncObjData*)(sds.ofbuff + offset);
		static_data.tag = tag;
		static_data.shapeindex = shapeindex;
		static_data.parent = parent;
		static_data.childs = childs;
		static_data.sibling = sibling;
		static_data.DestWorld = worldMat;
		static_data.LVelocity = LVelocity;
		offset += sizeof(STC_SyncObjData);

		//dynamic push
		if (mesh) {
			int& submeshNum = *(int*)(sds.ofbuff + offset);
			submeshNum = mesh->subMeshNum;
			offset += sizeof(int);

			int*& MaterialArr = *(int**)(sds.ofbuff + offset);
			memcpy(MaterialArr, material, sizeof(int) * mesh->subMeshNum);
			offset += sizeof(int) * mesh->subMeshNum;
		}
		else {
			int& nodecount = *(int*)(sds.ofbuff + offset);
			nodecount = model->nodeCount;
			offset += sizeof(int);

			matrix* InnerModelTransformArr = (matrix*)(sds.ofbuff + offset);
			memcpy(InnerModelTransformArr, transforms_innerModel, sizeof(matrix) * model->nodeCount);
			offset += sizeof(matrix) * model->nodeCount;
		}
		sds.postpush_end();
	}

	static void PrintOffset(ofstream& ofs) {
		GameObject::PrintOffset(ofs);
		for (int i = 0;i < g_members.size();++i) {
			ofs << g_members[i].name << " " << g_members[i].offset << " " << g_members[i].size << endl;
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

	virtual void SendGameObject(int objindex, SendDataSaver& sds) {
		sds.postpush_start();

		//calculate app packet siz.
		int reqsiz = sizeof(STC_SyncGameObject_Header) + sizeof(STC_SyncObjData);
		Shape& s = Shape::ShapeTable[shapeindex];
		Mesh* mesh = nullptr; 
		Model* model = nullptr; 
		s.GetRealShape(mesh, model);
		if (mesh) reqsiz += sizeof(int) + sizeof(int) * mesh->subMeshNum;
		else reqsiz += sizeof(int) + sizeof(matrix) * model->nodeCount;

		sds.postpush_reserve(reqsiz);
		int offset = 0;

		//static pushv
		STC_SyncGameObject_Header& header = *(STC_SyncGameObject_Header*)sds.ofbuff;
		header.size = reqsiz;
		header.st = SendingType::SyncGameObject;
		header.objindex = objindex;
		header.type = GameObjectType::_DynamicGameObject;
		offset += sizeof(STC_SyncGameObject_Header);

		STC_SyncObjData& static_data = *(STC_SyncObjData*)(sds.ofbuff + offset);
		static_data.tag = tag;
		static_data.shapeindex = shapeindex;
		static_data.parent = parent;
		static_data.childs = childs;
		static_data.sibling = sibling;
		static_data.DestWorld = worldMat;
		static_data.LVelocity = LVelocity;
		static_data.AnimationFlowTime = AnimationFlowTime;
		static_data.PlayingAnimationIndex = PlayingAnimationIndex;
		offset += sizeof(STC_SyncObjData);

		//dynamic push

		//shape
		if (mesh) {
			int& submeshNum = *(int*)(sds.ofbuff + offset);
			submeshNum = mesh->subMeshNum;
			offset += sizeof(int);

			int*& MaterialArr = *(int**)(sds.ofbuff + offset);
			memcpy(MaterialArr, material, sizeof(int) * mesh->subMeshNum);
			offset += sizeof(int) * mesh->subMeshNum;
		}
		else {
			int& nodecount = *(int*)(sds.ofbuff + offset);
			nodecount = model->nodeCount;
			offset += sizeof(int);

			matrix* InnerModelTransformArr = (matrix*)(sds.ofbuff + offset);
			memcpy(InnerModelTransformArr, transforms_innerModel, sizeof(matrix) * model->nodeCount);
			offset += sizeof(matrix) * model->nodeCount;
		}

		sds.postpush_end();
	}

	static void PrintOffset(ofstream& ofs) {
		DynamicGameObject::PrintOffset(ofs);
		for (int i = 0;i < g_members.size();++i) {
			ofs << g_members[i].name << " " << g_members[i].offset << " " << g_members[i].size << endl;
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
struct Player : public SkinMeshGameObject {
#define STC_CurrentStruct Player
	STC_STATICINIT_innerStruct;
	//STC HP
	STCDef(float, HP);
	//STC 최대HP
	STCDef(float, MaxHP);// = 100;
	//STC 현재총탄개수
	STCDef(int, bullets);// = 100;
	//STC 몬스터를 킬한 카운트 (임시 점수)
	STCDef(int, KillCount);// = 0;
	//STC 죽음 카운트
	STCDef(int, DeathCount);// = 0;
	//STC 열기 게이지
	STCDef(float, HeatGauge);// = 0;
	//STC 최대 열기 게이지
	STCDef(float, MaxHeatGauge);// = 100;
	//STC 스킬 쿨타임
	STCDef(float, HealSkillCooldown);// = 10.0f;
	//STC 쿨타임 타이머
	STCDef(float, HealSkillCooldownFlow);// = 0.0f;
	//STC 현재 무기 타입?
	STCDef(int, m_currentWeaponType);// = 0;
	//STC 플레이어의 인벤토리 정보
	static constexpr int maxItem = 36;
	STCDefArr(ItemStack, Inventory, maxItem);
	//STC 들고있는 무기
	STCDef(Weapon, weapon);

	//ServerOnly 점프력
	float JumpVelocity = 20;
	//ServerOnly 현재 땅에 닿아있는지를 나타낸다.
	bool isGround = false;
	//ServerOnly 얼마나 많은 게임 오브젝트와 충돌되었는지를 나타낸다.
	int collideCount = 0;
	//ServerOnly 해당 플레이어는 몇번째 클라이언트가 가지고 있는지 나타낸다.
	int clientIndex = 0;

	//CTS 플레이어가 어떤 키를 누르고 있는지를 나타내는 BoolBit 배열.
	BitBoolArr<2> InputBuffer;
	//CTS 현재 플레이어가 1인칭 시점인지 여부 (이게 동기화가 되는 중인가??)
	bool bFirstPersonVision = true;
	//CTS 플레이어가 바라보는 방향

	float m_yaw;
	float m_pitch;

	Player() : HP(100.0f), HeatGauge(0), MaxHeatGauge(200) {
		m_currentWeaponType = (int)WeaponType::Sniper;
		weapon = Weapon(WeaponType::Sniper);
	}

	virtual ~Player() { }

	/*
	* 설명 : 게임오브젝트의 업데이트를 실행함.
	* 매개변수 :
	* float deltaTime : 이전 업데이트 실행 부터 현재까지의 시간 차이.
	*/
	virtual void Update(float deltaTime) override;

	/*
	* 설명 : 게임오브젝트가 충돌했을때에 호출되는 함수.
	* 매개변수 :
	* GameObject* other : 충돌한 오브젝트
	*/
	virtual void OnCollision(GameObject* other) override;

	/*
	* 설명 : 게임오브젝트가 움직이지 않는 Static 충돌체와 충돌했을때에 호출되는 함수.
	* 매개변수 :
	* BoundingOrientedBox other : 충돌한 OBB.
	*/
	virtual void OnStaticCollision(BoundingOrientedBox obb) override;

	/*
	* 설명/반환 : 게임오브젝트의 충돌 OBB 정보를 반환한다.
	*/
	virtual BoundingOrientedBox GetOBB();

	/*
	* 설명 : damage 만큼 플레이어에게 데미지를 준다.
	* 매개변수 :
	* float damage : 줄 데미지 양.
	*/
	void TakeDamage(float damage);

	/*
	* 설명 : 플레이어를 리스폰 시킨다.
	*/
	void Respawn();

	/*
	* 설명 : Ray와 플레이어가 충돌했을때 호출되는 함수
	* 매개변수 :
	* GameObject* shooter : Ray를 쏜 게임오브젝트
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

	virtual void SendGameObject(int objindex, SendDataSaver& sds) {
		sds.postpush_start();

		//calculate app packet siz.
		int reqsiz = sizeof(STC_SyncGameObject_Header) + sizeof(STC_SyncObjData);
		Shape& s = Shape::ShapeTable[shapeindex];
		Mesh* mesh = nullptr; 
		Model* model = nullptr; 
		s.GetRealShape(mesh, model);
		if (mesh) reqsiz += sizeof(int) + sizeof(int) * mesh->subMeshNum;
		else reqsiz += sizeof(int) + sizeof(matrix) * model->nodeCount;

		sds.postpush_reserve(reqsiz);
		int offset = 0;

		//static pushv
		STC_SyncGameObject_Header& header = *(STC_SyncGameObject_Header*)sds.ofbuff;
		header.size = reqsiz;
		header.st = SendingType::SyncGameObject;
		header.objindex = objindex;
		header.type = GameObjectType::_Player;
		offset += sizeof(STC_SyncGameObject_Header);

		STC_SyncObjData& static_data = *(STC_SyncObjData*)(sds.ofbuff + offset);
		static_data.tag = tag;
		static_data.shapeindex = shapeindex;
		static_data.parent = parent;
		static_data.childs = childs;
		static_data.sibling = sibling;
		static_data.DestWorld = worldMat;
		static_data.LVelocity = LVelocity;
		static_data.AnimationFlowTime = AnimationFlowTime;
		static_data.PlayingAnimationIndex = PlayingAnimationIndex;

		static_data.HP = HP;
		static_data.MaxHP = MaxHP;
		static_data.bullets = bullets;
		static_data.KillCount = KillCount;
		static_data.DeathCount = DeathCount;
		static_data.HeatGauge = HeatGauge;
		static_data.MaxHeatGauge = MaxHeatGauge;
		static_data.HealSkillCooldown = HealSkillCooldown;
		static_data.HealSkillCooldownFlow = HealSkillCooldownFlow;
		static_data.m_currentWeaponType = m_currentWeaponType;
		memcpy(static_data.Inventory, Inventory, sizeof(ItemStack) * maxItem);
		static_data.weapon = weapon;
		offset += sizeof(STC_SyncObjData);

		//dynamic push

		//shape
		if (mesh) {
			int& submeshNum = *(int*)(sds.ofbuff + offset);
			submeshNum = mesh->subMeshNum;
			offset += sizeof(int);

			int*& MaterialArr = *(int**)(sds.ofbuff + offset);
			memcpy(MaterialArr, material, sizeof(int) * mesh->subMeshNum);
			offset += sizeof(int) * mesh->subMeshNum;
		}
		else {
			int& nodecount = *(int*)(sds.ofbuff + offset);
			nodecount = model->nodeCount;
			offset += sizeof(int);

			matrix* InnerModelTransformArr = (matrix*)(sds.ofbuff + offset);
			memcpy(InnerModelTransformArr, transforms_innerModel, sizeof(matrix) * model->nodeCount);
			offset += sizeof(matrix) * model->nodeCount;
		}

		sds.postpush_end();
	}

	static void PrintOffset(ofstream& ofs) {
		SkinMeshGameObject::PrintOffset(ofs);
		for (int i = 0;i < g_members.size();++i) {
			ofs << g_members[i].name << " " << g_members[i].offset << " " << g_members[i].size << endl;
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

AstarNode* FindClosestNode(float wx, float wz, const std::vector<AstarNode*>& allNodes);

/*
* 설명 : 간이 몬스터 클래스
*/
struct Monster : public SkinMeshGameObject {
#define STC_CurrentStruct Monster
	STC_STATICINIT_innerStruct;

	//STC 체력
	STCDef(float, HP); // = 30;
	//STC 최대체력
	STCDef(float, MaxHP); // = 30;
	//STC 현재 죽었는지 여부
	STCDef(bool, isDead);// = false;
	
	//ServerOnly 처음 스폰되었던 좌표
	vec4 m_homePos;
	//ServerOnly 공격대상의 좌표
	vec4 m_targetPos;
	//STC 걸어가는 속도 
	float m_speed = 2.0f;
	//ServerOnly 관찰가능한 영역반지름
	float m_patrolRange = 20.0f;
	//ServerOnly 쫒아가기 시작하는 영역의 반지름
	float m_chaseRange = 10.0f;
	//ServerOnly ??
	float m_patrolTimer = 0.0f;
	//ServerOnly 총을 발사하는 간격
	float m_fireDelay = 1.0f;
	//ServerOnly 총을 발사하고 나서 시간기록을 위한 타이머
	float m_fireTimer = 0.0f;
	//ServerOnly 현재 충돌한 지점의 개수
	int collideCount = 0;
	//ServerOnly ??
	int targetSeekIndex = 0;
	//ServerOnly 공격대상 
	Player** Target = nullptr;
	//ServerOnly 현재 움지이고 있는지 여부
	bool m_isMove = false;
	//ServerOnly 현재 땅에 붙어있는지 여부
	bool isGround = false;
	//ServerOnly 리스폰에 사용되는 타이머
	float respawntimer = 0;
	//ServerOnly ??
	float pathfindTimer = 0.0f;

	Monster() {}
	virtual ~Monster() {}

	virtual void Update(float deltaTime) override;
	//virtual void Render();
	virtual void OnCollision(GameObject* other) override;

	virtual void OnStaticCollision(BoundingOrientedBox obb) override;

	virtual void OnCollisionRayWithBullet(GameObject* shooter, float damage);

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

		//STC 체력
		float HP = 30;
		//STC 최대체력
		float MaxHP = 30;
		//STC 현재 죽었는지 여부
		bool isDead = false;
	};
#pragma pack(pop)

	virtual void SendGameObject(int objindex, SendDataSaver& sds) {
		sds.postpush_start();

		//calculate app packet siz.
		int reqsiz = sizeof(STC_SyncGameObject_Header) + sizeof(STC_SyncObjData);
		Shape& s = Shape::ShapeTable[shapeindex];
		Mesh* mesh = nullptr; 
		Model* model = nullptr; 
		s.GetRealShape(mesh, model);
		if (mesh) reqsiz += sizeof(int) + sizeof(int) * mesh->subMeshNum;
		else reqsiz += sizeof(int) + sizeof(matrix) * model->nodeCount;

		sds.postpush_reserve(reqsiz);
		int offset = 0;

		//static pushv
		STC_SyncGameObject_Header& header = *(STC_SyncGameObject_Header*)sds.ofbuff;
		header.size = reqsiz;
		header.st = SendingType::SyncGameObject;
		header.objindex = objindex;
		header.type = GameObjectType::_DynamicGameObject;
		offset += sizeof(STC_SyncGameObject_Header);

		STC_SyncObjData& static_data = *(STC_SyncObjData*)(sds.ofbuff + offset);
		static_data.tag = tag;
		static_data.shapeindex = shapeindex;
		static_data.parent = parent;
		static_data.childs = childs;
		static_data.sibling = sibling;
		static_data.DestWorld = worldMat;
		static_data.LVelocity = LVelocity;
		static_data.AnimationFlowTime = AnimationFlowTime;
		static_data.PlayingAnimationIndex = PlayingAnimationIndex;
		static_data.HP = HP;
		static_data.MaxHP = MaxHP;
		static_data.isDead = isDead;
		offset += sizeof(STC_SyncObjData);

		//dynamic push

		//shape
		if (mesh) {
			int& submeshNum = *(int*)(sds.ofbuff + offset);
			submeshNum = mesh->subMeshNum;
			offset += sizeof(int);

			int*& MaterialArr = *(int**)(sds.ofbuff + offset);
			memcpy(MaterialArr, material, sizeof(int) * mesh->subMeshNum);
			offset += sizeof(int) * mesh->subMeshNum;
		}
		else {
			int& nodecount = *(int*)(sds.ofbuff + offset);
			nodecount = model->nodeCount;
			offset += sizeof(int);

			matrix* InnerModelTransformArr = (matrix*)(sds.ofbuff + offset);
			memcpy_s(InnerModelTransformArr, sizeof(matrix) * model->nodeCount, transforms_innerModel, sizeof(matrix) * model->nodeCount);
			offset += sizeof(matrix) * model->nodeCount;
		}

		sds.postpush_end();
	}

	static void PrintOffset(ofstream& ofs) {
		SkinMeshGameObject::PrintOffset(ofs);
		for (int i = 0;i < g_members.size();++i) {
			ofs << g_members[i].name << " " << g_members[i].offset << " " << g_members[i].size << endl;
		}
	}
#undef STC_CurrentStruct
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

	void RenderChunkDbg();
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
* 설명 : 게임의 맵.
* Sentinal Value :
* NULL = 
* (name.size() == 0 && meshes.size() == 0 && 
* mesh_shapeindexes == 0 && models.size() == 0 && 
* MapObjects.size() == 0)
*/
struct GameMap {
	GameMap() {}
	~GameMap() {}
	//맵의 다양한 객체 (오브젝트, 메쉬, 애니메이션, 텍스쳐, 머터리얼)등을 읽을 때 사용되는 중복가능한 이름을 저장함.
	vector<string> name;

	//맵의 메쉬데이터 배열
	vector<Mesh> meshes;

	//Mesh와 MeshName와 Shape::AddMesh 함수로부터 로드되는 Shape들의 인덱스 배열
	vector<int> mesh_shapeindexes;
	
	//모델의 배열
	vector<Model*> models;

	//Shape::AddMesh 함수로부터 로드된 모델의 Shape들의 인덱스 배열
	vector<int> model_shapeindexes;

	//맵에 놓여져 있는 충돌처리를 할 계층구조 오브젝트
	vector<StaticGameObject*> MapObjects;

	// 맵 전체 영역의 AABB
	vec4 AABB[2] = { 0, 0 };

	unsigned int TextureTableStart = 0;
	unsigned int MaterialTableStart = 0;

	/*
	* 설명 : OBB를 받고, 그것을 통해 맵 전체의 AABB를 확장한다.
	* 매개변수 : 
	* BoundingOrientedBox obb : 받은 OBB
	*/
	void ExtendMapAABB(BoundingOrientedBox obb);
	
	/*
	* 설명 : Map의 전체 StaticCollision을 계산하여 Chunk들을 구성하고,
	* 맵 전체 영역의 AABB를 구하는 함수
	*/
	void BakeStaticCollision();

	/*
	* 설명 : 어떤 obj가 움직일때, Map에 StaticCollision에 대하여 충돌을 계산하는 함수.
	* 매개변수 : 
	* GameObject* obj : 충돌 감지를 할 움직이는 오브젝트
	*/
	void StaticCollisionMove(DynamicGameObject* obj);

	/*
	* 설명 : 어떤 obb가 Map에 StaticCollision에 대하여 충돌하는지 계산하는 함수
	* 매개변수 : 
	* BoundingOrientedBox obb : 충돌을 검사할 obb.
	* 반환 : 
	* 충돌시 true, 아니면 false
	*/
	bool isStaticCollision(BoundingOrientedBox obb);

	/*
	* 설명 : 전체 맵을 로드한다.
	* 매개변수 : 
	* const char* MapName : 맵 파일의 이름. 경로가 아니니 확장자 또한 붙이지 않고, 오직 파일의 이름만 적는다.
	*	해당 이름을 가진 .map 파일이 Resource/Map 경로에 있어야 한다.
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
	//접속한 Client의 소켓
	SOCKET socket;
	
	// 클라이언트로부터 받는 데이터를 저장하는 버퍼
	static constexpr int rbufcap = 8192 - sizeof(int);
	char rbuf[rbufcap + sizeof(int)] = {};
	int rbufoffset = 0;

	// 클라이언트 접속주소
	NWAddr addr;

	//Client가 조작하는 서버내 게임오브젝트(Player)
	Player* pObjData;

	//pObjData 가 서버 gameworld GameObject 배열에서 몇번째 인덱스에 위치하는지 나타낸다.
	int objindex = 0;

	//Send하는 데이터를 쌓아놓는 곳.
	SendDataSaver PersonalSDS;

	__forceinline DWORD recv(char* data, int len, DWORD flag) {
		WSABUF buf;
		buf.buf = data;
		buf.len = len;
		DWORD retval = 0;
		WSARecv(socket, &buf, 1, &retval, &flag, NULL, NULL);
		return retval;
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

/*
* 설명 : 게임이 돌아가는 월드 구조체.
*/
struct World {
	// 클라이언트 배열
	vecset<ClientData> clients;

	// 모든 클라이언트에게 전달될 공통의 데이터
	SendDataSaver CommonSDS;
	// 특정 클라이언트는 공통의 데이터를 전달해주지 않으려고 할때 쓰는 비트 마스크.
	BitAllotter CommonSDS_EnableClients;

	// 게임 오브젝트 배열
	vecset<DynamicGameObject*> Dynamic_gameObjects;
	vecset<StaticGameObject*> Static_gameObjects;

	// 드롭된 아이템들의 배열
	vecset<ItemLoot> DropedItems;

	// Astar pathfinding?
	vector<AstarNode*> allnodes;
	// Astar적용 가능한 최대/최소 영역
	static constexpr float AstarStartX = -40.0f;
	static constexpr float AstarStartZ = -40.0f;
	static constexpr float AstarEndX = 40.0f;
	static constexpr float AstarEndZ = 40.0f;

	// TODO : <지워야 할 것. PACK을 지워야 함.>
	// PACK 프로토콜에 쓰이는 변수들이다.
	twoPage tempbuffer;

	// 현재 실행하고 있는 게임 오브젝트가 gameObjects 배열에서 몇번째 인덱스에 존재하는지를 가리킨다.
	int currentIndex = 0;

	static constexpr float lowFrequencyDelay = 0.2f;
	float lowFrequencyFlow = 0.0f;
	/*
	lowFrequencyDelay 시간 간격마다 true가 되는 함수.
	빈도가 낮은 업데이트 계산을 시작하는데 쓰일 수 있다.
	*/
	__forceinline bool lowHit() {
		return lowFrequencyFlow > lowFrequencyDelay;
	}

	static constexpr float midFrequencyDelay = 0.05f;
	float midFrequencyFlow = 0.0f;
	/*
	midFrequencyDelay 시간 간격마다 true가 되는 함수.
	빈도가 낮은 업데이트 계산을 시작하는데 쓰일 수 있다.
	*/
	__forceinline bool midHit() {
		return midFrequencyFlow > midFrequencyDelay;
	}

	static constexpr float highFrequencyDelay = 0.01f;
	float highFrequencyFlow = 0.0f;
	/*
	highFrequencyDelay 시간 간격마다 true가 되는 함수.
	빈도가 낮은 업데이트 계산을 시작하는데 쓰일 수 있다.
	*/
	__forceinline bool highHit() {
		return highFrequencyFlow > midFrequencyDelay;
	}

	// 게임 맵 데이터
	GameMap map;
	unsigned int MaterialCount = 0;
	vector<HumanoidAnimation> HumanoidAnimationTable;

	UINT TourID = 0;

	/*
	* 설명 : 게임서버를 초기화한다.
	* 실질적으로 하는 일은 다음과 같다.
	* 1. Astar 길찾기를 위한 초기화 진행
	* 2. 아이템을 ItemTable에 생성
	* 3. 클라이언트와 서버간의 동기화를 위한 게임오브젝트 맴버변수의 Offset 동기화 설정.
	* 4. 각 Mesh의 충돌범위 계산
	* 5. 맵 충돌 정보를 Load
	* 6. 게임에서 작동시킬 게임 오브젝트들을 만들고 배열에 저장, 모든 클라이언트에게 해당 정보를 Send.
	* 7. 모든 작업이 끝난 후 "Game Init end" 출력.
	*/
	void Init();

	/*
	* 설명 : 게임을 DeltaTime 만큼 업데이트 한다.
	* 실질적으로 하는 일은 다음과 같다.
	* 1. lowHit, midHit, highHit 함수 작동을 위한 처리
	* 2. 모든 활성화된 게임 오브젝트에 대하여 Update 함수를 실행.
	* 3. 모든 게임 오브젝트에 대하여 tickVelocity 움직임과 충돌 계산.
	*	-> 충돌시 OnCollision 호출됨.
	*/
	void Update();


	void gridcollisioncheck();

	/*
	* 설명 : clientIndex 번째 클라이언트가 rBuffer 데이터를 서버로 보냈을때, 서버의 처리.
	* 클라이언트의 키보드 입력과 마우스 움직임을 처리한다.
	* 매개변수 :
	** int clientIndex : 입력을 보낸 클라이언트 번호
	** char* rBuffer : 클라이언트가 실제 보낸 데이터의 주소
	* 반환 :
	* 실제로 클라이언트가 보낸 패킷의 크기를 반환한다.
	* 키보드 입력(2byte), 마우스 움직임 입력(9byte)
	*/
	__forceinline int Receiving(int clientIndex, char* rBuffer, int totallen);

	/*
	* 설명 : 새로운 게임오브젝트를 추가하는 함수
	* 게임오브젝트 배열 내의 공간을 할당하여 게임오브젝트 포인터를 넣고 활성화 한다.
	* Sending_NewGameObject 를 호출해 데이터를 구성하고,
	* SendToAllClient 를 호출해 모든 클라이언트에게 데이터를 전송한다.
	* 매개변수 :
	* GameObject* obj : 추가할 오브젝트 포인터
	* GameObjectType gotype : 추가할 오브젝트의 타입
	* 반환 :
	* 반환값은 추가된 오브젝트가 게임오브젝트 배열에 몆번째 위치에 있는지에 대한 인덱스.
	*/
	int NewObject(DynamicGameObject* obj, GameObjectType gotype);

	/*
	* 설명 : 새로운 플레이어를 추가하는 함수
	* 게임오브젝트 배열 내의 공간을 할당하여 게임오브젝트 포인터를 넣고 활성화 한다.
	* <PACK> 프로토콜을 사용한다. 수정이 필요해 보인다.
	* Sending_NewGameObject 를 호출해 데이터를 구성하고,
	* SendToAllClient_execept 를 호출해 [clientIndex번째 클라이언트]를 제외한 모든 클라이언트에게 데이터를 전송한다.
	* 나중에 클라이언트가 초기화되면 보내줄 데이터를 pack_factory에 push한다.
	* 그리고 push한 데이터들을 보내지는 않는다.
	*
	* 매개변수 :
	* Player* obj : 추가할 플레이어 오브젝트 포인터
	* int clientIndex : 새로 들어온 클라이언트의 번호
	* 반환 :
	* 반환값은 추가된 오브젝트가 게임오브젝트 배열에 몆번째 위치에 있는지에 대한 인덱스.
	*/
	int NewPlayer(SendDataSaver& sds, Player* obj, int clientIndex);

	/*
	* 설명 : 새로운 오브젝트가 만들어졌단 정보를 클라이언트에게 전달하기 위해
	* 패킷 데이터를 tempbuffer에 구성한다.
	* 매개변수 :
	* int newindex : 새로운 오브젝트의 인덱스
	* GameObject* newobj : 새로운 오브젝트
	* GameObjectType gotype : 새로운 오브젝트의 타입
	* 반환 :
	* 구성된 패킷의 사이즈를 반환
	*/
	__forceinline void Sending_NewGameObject(SendDataSaver& sds, int newindex, GameObject* newobj);

	/*
	* 설명 : 기존 오브젝트의 맴버변수가 수정됨을 클라이언트에게 전달하기 위해
	* 패킷 데이터를 tempbuffer에 구성한다.
	* 매개변수 :
	* int objindex : 변경된 오브젝트의 인덱스
	* GameObject* ptrobj : 변경된 오브젝트
	* GameObjectType gotype : 변경된 오브젝트의 타입
	* void* memberAddr : 변경된 맴버변수의 주소
	* int memberSize : 변경된 맴버변수의 타입 사이즈
	* 반환 :
	* 구성된 패킷의 사이즈를 반환
	*/
	template <typename memberType>
	__forceinline void Sending_ChangeGameObjectMember(SendDataSaver& sds, int objindex, GameObject* ptrobj, GameObjectType gotype, void* memberAddr) {
		sds.postpush_start();
		constexpr int memberSize = sizeof(memberType);
		constexpr int reqsiz = sizeof(STC_ChangeMemberOfGameObject_Header) + memberSize;
		sds.postpush_reserve(reqsiz);
		STC_ChangeMemberOfGameObject_Header& header = *(STC_ChangeMemberOfGameObject_Header*)sds.ofbuff;
		header.size = reqsiz;
		header.st = SendingType::ChangeMemberOfGameObject;
		header.type = gotype;
		header.objindex = objindex;
		header.serveroffset = ((char*)memberAddr) - (char*)ptrobj;
		header.datasize = memberSize;
		sds.postpush_senddata<sizeof(STC_ChangeMemberOfGameObject_Header), memberSize>(memberAddr);
		sds.postpush_end();
	}

	/*
	* 설명 : 총알발사를 나타내는 Ray가 만들어졌다는 것을 클라이언트에게 보내기 위해
	* 패킷 데이터를 tempbuffer에 구성한다.
	* 매개변수 :
	* vec4 rayStart : Ray의 시작점
	* vec4 rayDirection : Ray의 진행방향
	* float rayDistance : Ray의 길이
	* 반환 :
	* 구성된 패킷의 사이즈를 반환
	*/
	__forceinline void Sending_NewRay(SendDataSaver& sds, vec4 rayStart, vec4 rayDirection, float rayDistance);

	/*
	* 설명 : 클라이언트 오브젝트의 Mesh 데이터를 수정하기 위해
	* 패킷 데이터를 tempbuffer에 구성한다.
	* 매개변수 :
	* int objindex : 오브젝트의 인덱스
	* string str : Mesh에 접근할 수 있는 문자열 key.
	* 반환 :
	* 구성된 패킷의 사이즈를 반환
	*/
	__forceinline void Sending_SetMeshInGameObject(SendDataSaver& sds, int objindex, string str);

	/*
	* 설명 : Ray를 발사하여 충돌지점을 찾는다.
	* 충돌지점은 게임오브젝트 배열을 돌아가면서 검사하며 찾는다.
	* 충돌시 gameObjects[i]->OnCollisionRayWithBullet(shooter); 를 호출하고,
	* 충돌판정작업이 끝나면, Sending_NewRay와 SendToAllClient 를 호출해
	* 모든 클라이언트에게 Ray 정보를 보내준다.
	* 매개변수 :
	* GameObject* shooter : 사격을 한 게임오브젝트
	* vec4 rayStart : Ray의 시작점
	* vec4 rayDirection : Ray의 진행방향
	* float rayDistance : Ray의 최대 길이
	*/
	__forceinline void FireRaycast(GameObject* shooter, vec4 rayStart, vec4 rayDirection, float rayDistance, float damage);

	/*
	* 설명 : 새 클라이언트가 접속하여 새 플레이어가 만들어진 상황에서,
	* 새로운 클라이언트에게 클라이언트가 가진 플레이어의 오브젝트 인덱스를 전달한다.
	* 이는 새로운 플레이어를 등록하는 역할을 하고, 등록이 되지 않은 클라이언트는 게임 실행을 하지 못한다.
	* 해당 데이터를 구성하기 위해
	* 패킷 데이터를 tempbuffer에 구성한다.
	* 매개변수 :
	* int clientindex : 클라이언트 번호
	* int objindex : 클라이언트가 가지는 플레이어의 게임 오브젝트 배열의 인덱스
	* 반환 :
	* 구성된 패킷의 사이즈를 반환
	*/
	__forceinline void Sending_AllocPlayerIndex(SendDataSaver& sds, int clientindex, int objindex);

	/*
	* 설명 : 클라이언트 오브젝트를 삭제하기 위해
	* 패킷 데이터를 tempbuffer에 구성한다.
	* 매개변수 :
	* int objindex : 삭제될 오브젝트의 인덱스
	* 반환 :
	* 구성된 패킷의 사이즈를 반환
	*/
	__forceinline void Sending_DeleteGameObject(SendDataSaver& sds, int objindex);

	/*
	* 설명 : 아이템이 드롭되는 것을 클라이언트에게 전달하기 위해
	* 패킷 데이터를 tempbuffer에 구성한다.
	* 매개변수 :
	* int dropindex : 드롭된 아이템이 DropedItems 배열에 어떤 인덱스에 위치하는지
	* ItemLoot lootdata : 드롭된 아이템 데이터
	* 반환 :
	* 구성된 패킷의 사이즈를 반환
	*/
	__forceinline void Sending_ItemDrop(SendDataSaver& sds, int dropindex, ItemLoot lootdata);

	/*
	* 설명 : 드롭된 아이템이 삭제되는 것을 클라이언트에게 전달하기 위해
	* 패킷 데이터를 tempbuffer에 구성한다.
	* 매개변수 :
	* int dropindex : 삭제될 드롭된 아이템이 DropedItems 배열에 어떤 인덱스에 위치하는지
	* 반환 :
	* 구성된 패킷의 사이즈를 반환
	*/
	__forceinline void Sending_ItemRemove(SendDataSaver& sds, int dropindex);

	/*
	* 설명 : 인벤토리의 아이템 데이터를 동기하기 위해
	* 패킷 데이터를 tempbuffer에 구성한다.
	* 매개변수 :
	* ItemStack lootdata : 인벤토리에 들어갈 아이템스택의 정보
	* int inventoryIndex : 인벤토리 몇번째 칸인지 결정
	* 반환 :
	* 구성된 패킷의 사이즈를 반환
	*/
	__forceinline void Sending_InventoryItemSync(SendDataSaver& sds, ItemStack lootdata, int inventoryIndex);

	__forceinline void Sending_PlayerFire(SendDataSaver& sds, int objIndex);

	__forceinline void Sending_SyncGameState(SendDataSaver& sds);

	/*
	* 설명 : tempbuffer에 저장된 패킷 데이터를 datacap 만큼
	* [execept 번째 클라이언트를 제외한]
	* 모든 클라이언트에게 전송한다.
	* 매개변수 :
	* int datacap : 보낼 데이터의 크기
	* int execept : 제외할 클라이언트 번호
	*/
	//__forceinline void SendToAllClient_execept(int datacap, int execept) {
	//	for (int i = 0; i < execept; ++i) {
	//		if (clients.isnull(i)) continue;
	//		clients[i].sendSaver.push_senddata(tempbuffer.data, datacap);
	//	}
	//	for (int i = execept + 1; i < clients.size; ++i) {
	//		if (clients.isnull(i)) continue;
	//		clients[i].sendSaver.push_senddata(tempbuffer.data, datacap);
	//	}
	//}

	////temp 2025.9.9 <??> 
	//void DestroyObject(int index);

	/*
	* 설명 : 새로운 클라이언트를 위해 현재 클라이언트와 공유할 모든 서버의 정보를
	* 보내기 위한 패킷을 pack_factory에 구성한다.
	* <PACK 프로토콜 >이 쓰여서 수정할 필요가 있다.
	* 매개변수 :
	* int new_client_index : 새로운 클라이언트의 번호
	*/
	void SendingAllObjectForNewClient(SendDataSaver& sds) {
		// STATIC 은 아직 보낼 필요는 없는듯? 하는게 충돌처리 밖에 없는데 충돌처리도 서버에서 함.
		// 그리고 클라이언트쪽에서 맵 불러올때 StaticObject들도 같이 불러온다.
		for (int i = 0; i < Dynamic_gameObjects.size; ++i) {
			if (Dynamic_gameObjects.isnull(i)) continue;
			void* vptr = *(void**)Dynamic_gameObjects[i];
			Sending_NewGameObject(sds, i, (GameObject*)Dynamic_gameObjects[i]);
		}

		for (int i = 0; i < DropedItems.size; ++i) {
			if (DropedItems.isnull(i)) continue;
			Sending_ItemDrop(sds, i, DropedItems[i]);
		}
	}

	////temp 2025.09.08 <PACK이 쓰이지 않음에 따라 나중에 쓸 일이 있지 않을까?>
	/*void SendingAllObjectForNewClient(int new_client_index) {
		for (int i = 0; i < gameObjects.size; ++i) {
			if (gameObjects.isnull(i)) continue;
			void* vptr = *(void**)gameObjects[i];
			int datacap = Sending_NewGameObject(i, gameObjects[i], GameObjectType::VptrToTypeTable[vptr]);
			clients[new_client_index].socket.Send((char*)tempbuffer.data, datacap);

			if (gameObjects[i]->MeshIndex >= 0) {
				datacap = Sending_SetMeshInGameObject(i, Mesh::MeshNameArr[gameObjects[i]->MeshIndex]);
				clients[new_client_index].socket.Send((char*)tempbuffer.data, datacap);
			}
		}
	}*/


	//하나의 청크의 정육면체의 한 변의 길이를 결정한다.
	static constexpr float chunck_divide_Width = 10.0f;

	//게임내의 Chunck들의 모임.
	unordered_map<ChunkIndex, GameChunk*> chunck;

	/*
	* 설명 : obb가 걸치는 모든 청크영역을 찾아낸다.
	* 매개변수 :
	* BoundingOrientedBox obb
	* 반환 : 찾아낸 청크영역
	*/
	GameObjectIncludeChunks GetChunks_Include_OBB(BoundingOrientedBox obb);

	/*
	* 설명 : 위치로 해당 위치를 포함하는 청크를 찾아낸다.
	* 매개변수 :
	* vec4 pos : 위치
	* 반환 : 찾아낸 청크
	*/
	GameChunk* GetChunkFromPos(vec4 pos);

	/*
	* 설명 : 게임오브젝트를 알맞은 청크에 넣는다.
	* 매개변수 :
	* GameObject* go : 넣을 오브젝트
	*/
	void PushGameObject(GameObject* go);

	void PrintCangoGrid(const std::vector<AstarNode*>& all, int gridWidth, int gridHeight);
	void PrintOffset();
	bool CheckAABBSphereCollision(const vec4& boxCenter, const vec4& boxHalfSize, const collisionchecksphere& sphere);
};
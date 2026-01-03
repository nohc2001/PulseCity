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
	// 인덱스에서 Shape를 얻는 map
	static unordered_map<int, Shape> IndexToShapeMap;

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

/*
* 설명 : Item의 데이터
*/
struct Item
{
	// server, client
	int id;
	// only client
	//vec4 color;
	//Mesh* MeshInInventory;

	Item(int i) : id{ i } {}
};


typedef int ItemID;

/*
* 설명 : 인벤토리 칸에 들어갈 데이터.
* Sentinal Value :
* NULL : (ItemCount == 0)
*/
struct ItemStack {
	ItemID id;
	int ItemCount;
};

/*
* 설명 : 드롭된 아이템 데이터.
* Sentinal Value :
* NULL : ItemStack=NULL
*/
struct ItemLoot {
	ItemStack itemDrop;
	vec4 pos;
};

/*
* 설명 : 게임에 나타날 모든 아이템을 저장해놓은 테이블
*/
extern vector<Item> ItemTable;

/*
* 설명 : 서버 내의 게임오브젝트
* Sentinal Value : 
* NULL = (isExist == false)
*/
struct GameObject {
	// 게임오브젝트의 활성화 여부
	bool isExist = true;
	// 게임오브젝트가 가진 shape의 index
	int ShapeIndex;
	// 월드 행렬
	matrix m_worldMatrix;
	// 현재 속도
	vec4 LVelocity;
	// 업데이트할 위치를
	vec4 tickLVelocity;

	GameObject();
	virtual ~GameObject();

	/*
	* 설명 : 게임오브젝트의 업데이트를 실행함.
	* 매개변수 :
	* float deltaTime : 이전 업데이트 실행 부터 현재까지의 시간 차이.
	*/
	virtual void Update(float deltaTime);

	/*
	* 설명/반환 : 게임오브젝트의 충돌 OBB 정보를 반환한다.
	*/
	virtual BoundingOrientedBox GetOBB();

	/*
	* 설명 : 두 게임 오브젝트간의 움직임에 대한 충돌을 처리한다.
	* 매개변수 : 
	* GameObject* gbj1 : 첫번째 게임오브젝트
	* GameObject* gbj2 : 두번째 게임오브젝트
	*/
	static void CollisionMove(GameObject* gbj1, GameObject* gbj2);

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
	static void CollisionMove_DivideBaseline(GameObject* movObj, GameObject* colObj);

	/*
	* 설명 : movObj가 움직이고, colOBB와 서로 충돌할 경우,
	* 보다 자연스럽게 이동하기 위해서 colOBB의 기저를 기준으로 이동을 시행한다.
	* 매개변수 :
	* GameObject* movObj : 움직이는 게임오브젝트
	* BoundingOrientedBox colOBB : 정지해있는 게임오브젝트
	*/
	static void CollisionMove_DivideBaseline_StaticOBB(GameObject* movObj, BoundingOrientedBox colOBB);

	/*
	* 설명 : movObj가 preMove만큼 움직인후, 게속 움직이고, colOBB와 서로 충돌할 경우,
	* 보다 자연스럽게 이동하기 위해서 colOBB의 기저를 기준으로 이동을 시행한다.
	* 이때 colObj는 오직 기저를 계산하기 위해 사용된다.
	* 매개변수 :
	* GameObject* movObj : 움직이는 게임오브젝트
	* BoundingOrientedBox colOBB : 정지해있는 게임오브젝트
	*/
	static void CollisionMove_DivideBaseline_rest(GameObject* movObj, GameObject* colObj, BoundingOrientedBox colOBB, vec4 preMove);

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
	virtual void OnCollisionRayWithBullet(GameObject* shooter);
};

class Hierarchy_Object : public GameObject {
public:
	// 오브젝트가 가지는 자식오브젝트의 개수
	int childCount = 0;

	// 해당 오브젝트의 자식 오브젝트
	vector<Hierarchy_Object*> childs;
	
	// 해당 오브젝트가 가진 Material의 index 
	// >> 다만 서버의 경우 이것이 제대로 로드하지 않아 유의미한 값은 아님.
	int Mesh_materialIndex = 0;

	// AABB
	vec4 AA, BB; // min, max xyz

	Hierarchy_Object() {
		childCount = 0;
		Mesh_materialIndex = 0;
	}
	~Hierarchy_Object() {}

	/*
	* 설명 : 하이라키 오브젝트의 OBB에 현재 worldMatrix와 계층구조로부터 받은 parent_world를 곱하고, 해당 OBB와 bb를 충돌체크 한다.
	* 매개변수 : 
	* matrix parent_world : 계층구조로 쌓아진 행렬
	* BoundingBox bb : 충돌 검사할 AABB
	*/
	bool Collision_Inherit(matrix parent_world, BoundingBox bb);

	/*
	* 설명 : Map의 계층구조를 탐방하며 Map의 Static Collision Chunck와 
	* Map 전체의 AABB를 계산하기 위한 함수.
	* 매개변수 : 
	* void* origin : 가장 근본 부모가 되는 Map 오브젝트
	* matrix parent_world : 제귀호출이 일어나며 상속받은 부모들의 월드행렬
	*/
	void InitMapAABB_Inherit(void* origin, matrix parent_world);

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
	BoundingOrientedBox GetOBBw( matrix worldMat);
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

	ChunkIndex(){}
	~ChunkIndex(){}

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
};

/*
* 설명 : Static Collision 에 사용되는 청크.
* 여러개의 OBB를 소유하고 있다.
* Sentinal Value :
* NULL = (obbs.size() == 0)
*/
struct StaticCollisions {
	vector<BoundingOrientedBox> obbs;
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
	vector<Hierarchy_Object*> MapObjects;

	// 맵 전체 영역의 AABB
	vec4 AABB[2] = { 0, 0 };

	/*
	* 설명 : OBB를 받고, 그것을 통해 맵 전체의 AABB를 확장한다.
	* 매개변수 : 
	* BoundingOrientedBox obb : 받은 OBB
	*/
	void ExtendMapAABB(BoundingOrientedBox obb);

	//하나의 청크의 정육면체의 한 변의 길이를 결정한다.
	static constexpr float chunck_divide_Width = 10.0f;

	//chunkIndex로 StaticCollision을 위한 청크를 찾기 위한 Map
	unordered_map<ChunkIndex, StaticCollisions*> static_collision_chunck;

	/*
	* 설명 : obb를 맵 청크 내에 담는다. obb와 조금이라도 걸치는 청크에 모두 담는다.
	* 매개변수 : 
	* BoundingOrientedBox obb : 담을 obb.
	*/
	void PushOBB_ToStaticCollisions(BoundingOrientedBox obb);

	/*
	* 설명 : obb를 맵 청크 내에 담는다.
	* 매개변수 : 
	* ChunkIndex ci : 청크를 선택할 인덱스
	* BoundingOrientedBox obb : 담을 obb.
	*/
	void PushOBB_ToStaticCollisions_WithChunkIndex(ChunkIndex ci, BoundingOrientedBox obb);
	
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
	void StaticCollisionMove(GameObject* obj);

	/*
	* 설명 : 어떤 obb가 Map에 StaticCollision에 대하여 충돌하는지 계산하는 함수
	* 매개변수 : 
	* BoundingOrientedBox obb : 충돌을 검사할 obb.
	* 반환 : 
	* 충돌시 true, 아니면 false
	*/
	bool isStaticCollision(BoundingOrientedBox obb);

	// 서버에서는 쓸데 없는 값. 
	unsigned int TextureTableStart = 0;
	unsigned int MaterialTableStart = 0;

	/*
	* 설명 : 전체 맵을 로드한다.
	* 매개변수 : 
	* const char* MapName : 맵 파일의 이름. 경로가 아니니 확장자 또한 붙이지 않고, 오직 파일의 이름만 적는다.
	*	해당 이름을 가진 .map 파일이 Resource/Map 경로에 있어야 한다.
	*/
	void LoadMap(const char* MapName);
};
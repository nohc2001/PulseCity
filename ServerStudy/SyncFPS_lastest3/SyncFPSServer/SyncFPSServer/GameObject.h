#pragma once
#include "stdafx.h"

struct Mesh {
	vec4 MAXpos;
	vec4 Center;

	void ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering = true);
	BoundingOrientedBox GetOBB();
	void CreateWallMesh(float width, float height, float depth);

	void SetOBBDataWithAABB(XMFLOAT3 minpos, XMFLOAT3 maxpos);
	static void GetAABBFromOBB(vec4* out, BoundingOrientedBox obb, bool first = false);
};

struct ModelNode {
	XMMATRIX transform;
	ModelNode* parent;
	unsigned int numChildren;
	ModelNode** Childrens;
	unsigned int numMesh;
	unsigned int* Meshes; // array of int

	ModelNode() {
		parent = nullptr;
		numChildren = 0;
		Childrens = nullptr;
		numMesh = 0;
		Meshes = nullptr;
	}

	void BakeAABB(void* origin, const matrix& parentMat);
};

struct Model {
	std::string mName;
	int nodeCount = 0;
	ModelNode* RootNode;
	vector<ModelNode*> NodeArr;
	unordered_map<void*, int> nodeindexmap;
	ModelNode* Nodes;

	unsigned int mNumMeshes;
	Mesh** mMeshes;

	//metadata
	float UnitScaleFactor = 1.0f;

	vec4 AABB[2];
	vec4 OBB_Tr;
	vec4 OBB_Ext;

	void LoadModelFile(string filename);
	void LoadModelFile2(string filename);

	void BakeAABB();
	BoundingOrientedBox GetOBB();
};

struct Shape {
	ui64 FlagPtr;

	__forceinline bool isMesh() {
		return FlagPtr & 0x8000000000000000;

		// highest bit == 1 -> Mesh
		// else -> Model
	}

	__forceinline Mesh* GetMesh() {
		if (isMesh()) {
			return reinterpret_cast<Mesh*>(FlagPtr & 0x7FFFFFFFFFFFFFFF);
		}
		else return nullptr;
	}

	__forceinline void SetMesh(Mesh* ptr) {
		FlagPtr = reinterpret_cast<ui64>(ptr);
		FlagPtr |= 0x8000000000000000;
	}

	__forceinline Model* GetModel() {
		if (isMesh()) return nullptr;
		else {
			return reinterpret_cast<Model*>(FlagPtr);
		}
	}

	__forceinline void SetModel(Model* ptr) {
		FlagPtr = reinterpret_cast<ui64>(ptr);
	}

	static int GetShapeIndex(string meshName);
	static int AddShapeName(string meshName);

	static vector<string> ShapeNameArr;
	static unordered_map<string, Shape> StrToShapeMap;
	static unordered_map<int, Shape> IndexToShapeMap;

	static int AddMesh(string name, Mesh* ptr);
	static int AddModel(string name, Model* ptr);
};

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
struct ItemStack {
	ItemID id;
	int ItemCount;
};

struct ItemLoot {
	ItemStack itemDrop;
	vec4 pos;
};

extern vector<Item> ItemTable;

struct GameObject {
	bool isExist = true;
	int ShapeIndex;
	matrix m_worldMatrix;
	vec4 LVelocity;
	vec4 tickLVelocity;

	GameObject();
	virtual ~GameObject();

	virtual void Update(float deltaTime);
	virtual void Event();

	virtual BoundingOrientedBox GetOBB();

	static void CollisionMove(GameObject* gbj1, GameObject* gbj2);
	static void CollisionMove_DivideBaseline(GameObject* movObj, GameObject* colObj, BoundingOrientedBox colOBB);
	static void CollisionMove_DivideBaseline_StaticOBB(GameObject* movObj, BoundingOrientedBox colOBB);
	static void CollisionMove_DivideBaseline_rest(GameObject* movObj, GameObject* colObj, BoundingOrientedBox colOBB, vec4 preMove);
	virtual void OnCollision(GameObject* other);
	virtual void OnStaticCollision(BoundingOrientedBox other);

	virtual void OnCollisionRayWithBullet(GameObject* shooter);
};

class Hierarchy_Object : public GameObject {
public:
	int childCount = 0;
	vector<Hierarchy_Object*> childs;
	int Mesh_materialIndex = 0;
	vec4 AA, BB; // min, max xyz

	Hierarchy_Object() {
		childCount = 0;
		Mesh_materialIndex = 0;
	}
	~Hierarchy_Object() {}

	bool Collision_Inherit(matrix parent_world, BoundingBox bb);
	void InitMapAABB_Inherit(void* origin, matrix parent_world);
	BoundingOrientedBox GetOBBw(matrix worldMat);
};

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

struct StaticCollisions {
	vector<BoundingOrientedBox> obbs;
};

template<>
struct hash<ChunkIndex> {
	size_t operator()(const ChunkIndex& p) const noexcept {
		size_t h = _pdep_u64((size_t)p.x, 0x9249249249249249);
		h |= _pdep_u64((size_t)p.y, 0x2492492492492492);
		h |= _pdep_u64((size_t)p.z, 0x4924924924924924);
		return h;
	}
};

struct GameMap {
	GameMap() {}
	~GameMap() {}
	vector<string> name;
	vector<Mesh> meshes;
	vector<int> mesh_shapeindexes;
	vector<Model*> models;
	vector<int> model_shapeindexes;
	vector<Hierarchy_Object*> MapObjects;
	vec4 AABB[2] = { 0, 0 };
	void ExtendMapAABB(BoundingOrientedBox obb);

	static constexpr float chunck_divide_Width = 10.0f;
	StaticCollisions WholeStaticCollisions;
	unordered_map<ChunkIndex, StaticCollisions*> static_collision_chunck;
	void PushOBB_ToStaticCollisions(BoundingOrientedBox obb);
	void PushOBB_ToStaticCollisions_WithChunkIndex(ChunkIndex ci, BoundingOrientedBox obb);
	//static collision..
	void BakeStaticCollision();

	void StaticCollisionMove(GameObject* obj);
	bool isStaticCollision(BoundingOrientedBox obb);


	unsigned int TextureTableStart = 0;
	unsigned int MaterialTableStart = 0;

	void LoadMap(const char* MapName);
};
#pragma once
#include "stdafx.h"
#include "main.h"
#include "Mesh.h"

class Mesh;
class Shader;
struct GPUResource;

struct GameObject {
	bool isExist = true;
	int diffuseTextureIndex = 0;
	matrix m_worldMatrix;
	vec4 LVelocity;
	vec4 tickLVelocity;
	union {
		Mesh* m_pMesh;
		Model* pModel = nullptr;
	};
	Shader* m_pShader = nullptr;
	vec4 Destpos;

	enum eRenderMeshMod {
		single_Mesh = 0,
		Model = 1
	};
	eRenderMeshMod rmod = eRenderMeshMod::single_Mesh;

	GameObject();
	virtual ~GameObject();

	virtual void Update(float deltaTime);
	virtual void Render();
	virtual void RenderShadowMap();
	virtual void Event(WinEvent evt);

	void SetMesh(Mesh* pMesh);
	void SetShader(Shader* pShader);
	void SetWorldMatrix(const XMMATRIX& worldMatrix);

	const XMMATRIX& GetWorldMatrix() const;

	virtual BoundingOrientedBox GetOBB();
	OBB_vertexVector GetOBBVertexs();

	void LookAt(vec4 look, vec4 up = { 0, 1, 0, 0 });

	static void CollisionMove(GameObject* obj1, GameObject* obj2);
	static void CollisionMove_DivideBaseline(GameObject* movObj, GameObject* colObj, BoundingOrientedBox colOBB);
	static void CollisionMove_DivideBaseline_rest(GameObject* movObj, GameObject* colObj, BoundingOrientedBox colOBB, vec4 preMove);
	virtual void OnCollision(GameObject* other);

	virtual void OnCollisionRayWithBullet();

	void PositionInterpolation(float deltaTime);
};

struct BulletRay {
	static constexpr float remainTime = 0.2f;
	static Mesh* mesh;
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

struct Item
{
	// server, client
	int id;
	// only client
	vec4 color;
	Mesh* MeshInInventory;
	GPUResource* tex;
	const wchar_t* description;

	Item(int i, vec4 c, Mesh* m, GPUResource* t, const wchar_t* d) :
		id{ i }, color{ c }, MeshInInventory{ m }, tex{ t }, description{
		d
		}
	{

	}
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

class LMoveObject_CollisionTest : public GameObject {
public:
	LMoveObject_CollisionTest();
	virtual ~LMoveObject_CollisionTest();

	virtual void Update(float deltaTime);
	virtual void Render();
	virtual void Event(WinEvent evt);

	virtual void OnCollision(GameObject* other) override;
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

	void Render_Inherit(matrix parent_world, Shader::RegisterEnum sre = Shader::RegisterEnum::RenderWithShadow);
	bool Collision_Inherit(matrix parent_world, BoundingBox bb);
	void InitMapAABB_Inherit(void* origin, matrix parent_world);
	BoundingOrientedBox GetOBBw(matrix worldMat);
};

struct GameMap {
	GameMap() {}
	~GameMap() {}
	vector<string> name;
	vector<Mesh*> meshes;
	vector<Model*> models;
	vector<Hierarchy_Object*> MapObjects;
	vec4 AABB[2] = { 0, 0 };
	void ExtendMapAABB(BoundingOrientedBox obb);

	ui64 pdep_src2[48] = {};
	int pdepcnt = 0;
	ui64 GetSpaceHash(int x, int y, int z);

	struct Posindex {
		int x;
		int y;
		int z;
	};
	ui64 inv_pdep_src2[48] = {};
	int inv_pdepcnt = 0;
	Posindex GetInvSpaceHash(ui64 hash);

	RangeArr<ui32, bool> IsCollision;

	//static collision..
	void BakeStaticCollision();

	unsigned int TextureTableStart = 0;
	unsigned int MaterialTableStart = 0;

	void LoadMap(const char* MapName);
};
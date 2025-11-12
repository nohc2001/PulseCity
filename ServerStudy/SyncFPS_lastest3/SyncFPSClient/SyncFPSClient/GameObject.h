#pragma once
#include "stdafx.h"
#include "main.h"

class Mesh;
class Shader;
struct GPUResource;

struct GameObject {
	bool isExist = true;
	int diffuseTextureIndex = 0;
	matrix m_worldMatrix;
	vec4 LVelocity;
	vec4 tickLVelocity;
	Mesh* m_pMesh = nullptr;
	Shader* m_pShader = nullptr;
	vec4 Destpos;

	GameObject();
	virtual ~GameObject();

	virtual void Update(float deltaTime);
	virtual void Render();
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
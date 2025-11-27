#pragma once
#include "stdafx.h"

struct Mesh {
	vec4 MAXpos;

	struct Vertex {
		XMFLOAT3 position;
		Vertex() {}
		Vertex(XMFLOAT3 p)
			: position{ p }
		{
		}
	};

	void ReadMeshFromFile_OBJ(const char* path, vec4 color, bool centering = true);
	BoundingOrientedBox GetOBB();
	void CreateWallMesh(float width, float height, float depth);

	static vector<string> MeshNameArr;
	static unordered_map<string, Mesh*> meshmap;
	static int GetMeshIndex(string meshName);
	static int AddMeshName(string meshName);
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
	int MeshIndex;
	matrix m_worldMatrix;
	vec4 LVelocity;
	vec4 tickLVelocity;
	Mesh mesh;

	GameObject();
	virtual ~GameObject();

	virtual void Update(float deltaTime);
	virtual void Event();

	virtual BoundingOrientedBox GetOBB();

	static void CollisionMove(GameObject* gbj1, GameObject* gbj2);
	static void CollisionMove_DivideBaseline(GameObject* movObj, GameObject* colObj, BoundingOrientedBox colOBB);
	static void CollisionMove_DivideBaseline_rest(GameObject* movObj, GameObject* colObj, BoundingOrientedBox colOBB, vec4 preMove);
	virtual void OnCollision(GameObject* other);

	virtual void OnCollisionRayWithBullet(GameObject* shooter);
};
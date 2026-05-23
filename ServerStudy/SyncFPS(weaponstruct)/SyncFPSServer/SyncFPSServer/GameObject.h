#pragma once
#include "stdafx.h"
struct Zone;

/*
* ���� : Mesh�� �浹ó�� OBB ������ ����.
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
	* ���� : obj ���Ϸ� ���� Mesh�� OBB �����͸� �о�´�.
	* <���� UnitScale�� 100�� fbx���� ������ ����� �������� Mesh �����͸� �ҷ��´�.>
	* ������ ���� [100.0f = 1m �� Mesh]���� �ùٸ��� �ҷ��� �� �ִ� �����̴�.
	* !!!��ġ�� �ʿ��ϴ�.!!!
	* �Ű����� : 
	* const char* path : obj ������ ���
	* bool centering : OBB�� Center�� (0, 0, 0)�� �ǵ��� �����͸� ����.
	*/
	void ReadMeshFromFile_OBJ(const char* path, bool centering = true);

	/*
	* ����/��ȯ : Mesh�� OBB �浹�����͸� ��ȯ�Ѵ�.
	*/
	BoundingOrientedBox GetOBB();
	
	/*
	* ���� : ���� width, ���� height, ���� depth�� ������ü �� Mesh ������ �����Ѵ�.
	* �Ű����� : 
	* float width : �ʺ�
	* float height : ����
	* float depth : ��
	*/
	void CreateWallMesh(float width, float height, float depth);

	/*
	* ���� : AABB �����͸� ���� Mesh�� OBB �����͸� �����Ѵ�.
	* XMFLOAT3 minpos : AABB�� �ּ�����
	* XMFLOAT3 maxpos : AABB�� �ִ�����
	*/
	void SetOBBDataWithAABB(XMFLOAT3 minpos, XMFLOAT3 maxpos);

	/*
	* ���� : OBB �����͸� ���� AABB �����͸� �����Ѵ�.
	* OBB�� �����Ӱ� ȸ���Ͽ��� �װ��� ��� �����ϴ� �ּ��� AABB�� ��� �Ѵ�.
	* vec4* out : ������̾��� AABB�� ������, AABB�� �������� ����. vec4[2] ��ŭ�� ������ �Ҵ�Ǿ� �־���Ѵ�.
	* BoundingOrientedBox obb : AABB�� ��ȯ�� OBB.
	* bool first : 
	*	true�̸�, AABB�� ó������ ����ϴ� ���̴�. �׷��� obb�� AABB�� �ٲٴ� ������ ������ �����Ѵ�.
	*	false�̸�, ���� out�� �� AABB �����Ϳ� obb ������ ��� ���Եǵ��� �ϴ� �ּ� AABB�� �ٽ� ����.
	*	�̷� ����� ���� obb�� ���Խ�Ű�� �ϳ��� �ּ� AABB�� ���ϴµ� ���δ�.
	*/
	static void GetAABBFromOBB(vec4* out, BoundingOrientedBox obb, bool first = false);
};

/*
* ���� : ���� ���
* Sentinal Value :
* NULL = (parent == nullptr && numChildren == 0 && Childrens == nullptr && numMesh == 0 && Meshes == nullptr)
*/
struct ModelNode {
	// �𵨳���� �⺻ ��ȯ���
	XMMATRIX transform;
	// �θ���
	ModelNode* parent;
	// �ڽĳ���� ����
	unsigned int numChildren;
	// �ڽĳ�� �����͵��� �迭
	ModelNode** Childrens;
	// �ش� ��尡 ���� Mesh�� ����
	unsigned int numMesh;
	// �ش� ��尡 ���� Mesh index���� �迭
	// �ش� index�� Model::mMeshes �迭�� ���� Mesh�� ���� �� �ִ�.
	unsigned int* Meshes;

	// �޽��� ���� ���͸����� �ε��� �迭
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
	* ���� : �� ��尡 �⺻�����϶�,
	* �ش� �� ����� �ڽŰ� ��� �ڽ��� ���Խ�Ű�� AABB�� �����Ͽ�
	* origin ���� AABB�� Ȯ���Ų��.
	* �Ű����� :
	* void* origin : Model�� �ν��Ͻ���, �ش� ModelNode�� ������ ���� Model�� void*
	* const matrix& parentMat : �θ��� �⺻ trasform���� ���� ��ȯ�� ���
	*/
	void BakeAABB(void* origin, const matrix& parentMat);

	void PushOBBs(void* origin, const matrix& parentMat, vector<BoundingOrientedBox>* obbArr, void* gameobj);
};

/*
* ���� : ��
* Sentinal Value :
* NULL = (nodeCount == 0 && RootNode == nullptr && Nodes == nullptr && mNumMeshes == 0 && mMeshes == nullptr)
*/
struct Model {
	// �浹ó���� ������ �ϱ� ���� OBB ������ �� �տ� ��ġ��Ų��.
	// ���� OBB.Center
	vec4 OBB_Tr;
	// ���� OBB.Extends
	vec4 OBB_Ext;
	// ���� �̸�
	std::string mName;
	// ���� ������ ��� ������ ����
	int nodeCount = 0;
	// ���� �ֻ��� ���
	ModelNode* RootNode;

	// Nodes�� �ε��� ���� ���̴� ������ (Ŭ���̾�Ʈ���� ������ ���� �ʿ䰡 ����.)
	static vector<ModelNode*> NodeArr;
	static unordered_map<void*, int> nodeindexmap;
	
	// �� ������ �迭
	ModelNode* Nodes;

	// ���� ���� �޽��� ����
	unsigned int mNumMeshes;
	
	// ���� ���� �޽����� ������ �迭
	Mesh** mMeshes;

	// ���� UnitScaleFactor
	float UnitScaleFactor = 1.0f;

	// ���� �⺻���¿��� ���� ��� �����ϴ� ���� ���� AABB.
	vec4 AABB[2];

	/*
	* ���� : MyModelExporter���� �̾ƿ� �� ���̳ʸ� ������ �ε���.
	* ���������� �浹�������� �����´�.
	* �Ű�����:
	* string filename : ���� ���
	*/
	//void LoadModelFile(string filename);

	/*
	* ���� : Unity���� �̾ƿ� �ʿ� �����ϴ� �� ���̳ʸ� ������ �ε���.
	* ���������� �浹�������� �����´�.
	* �Ű�����:
	* string filename : ���� ���
	*/
	void LoadModelFile2(string filename, Zone* zone = nullptr);

	/*
	* ���� : Model�� AABB�� �����Ѵ�.
	*/
	void BakeAABB();

	/*
	* ����/��ȯ : Model�� �⺻ OBB�� ��ȯ�Ѵ�.
	*/
	BoundingOrientedBox GetOBB();
};

struct HumanoidAnimation {
	double Duration = 0;
	void LoadHumanoidAnimation(string filename);
};

/*
* ���� : Shape�� Mesh�� Model�� ������ �� �ִ� ����� ��Ÿ�� ����ü.
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
	* ����/��ȯ : Shape�� Mesh���� ���θ� ��ȯ
	*/
	__forceinline bool isMesh() {
		return FlagPtr & 0x8000000000000000;
	}

	/*
	* ����/��ȯ : Shape�� ���� Mesh �����͸� ��ȯ
	*/
	__forceinline Mesh* GetMesh() {
		if (isMesh()) {
			return reinterpret_cast<Mesh*>(FlagPtr & 0x7FFFFFFFFFFFFFFF);
		}
		else return nullptr;
	}

	/*
	* ���� : Shape�� Mesh�� �ִ´�.
	*/
	__forceinline void SetMesh(Mesh* ptr) {
		FlagPtr = reinterpret_cast<ui64>(ptr);
		FlagPtr |= 0x8000000000000000;
	}

	/*
	* ����/��ȯ : Shape�� ���� Model �����͸� ��ȯ
	*/
	__forceinline Model* GetModel() {
		if (isMesh()) return nullptr;
		else {
			return reinterpret_cast<Model*>(FlagPtr);
		}
	}

	/*
	* ���� : Shape�� Model�� �ִ´�.
	*/
	__forceinline void SetModel(Model* ptr) {
		FlagPtr = reinterpret_cast<ui64>(ptr);
	}

	// �̸����� ShapeIndex�� ��� map
	static unordered_map<string, int> StrToShapeIndex;
	static vector<Shape> ShapeTable;
	static vector<string> ShapeStrTable;

	/*
	* ���� : Mesh�� �̸��� Mesh �����͸� �޾� Mesh�� �߰��ϴ� �Լ�
	* �Ű����� : 
	* string name : Mesh�� �̸�
	* Mesh* ptr : Mesh�� ������
	*/
	static int AddMesh(string name, Mesh* ptr);

	/*
	* ���� : Model�� �̸��� Model �����͸� �޾� Model�� �߰��ϴ� �Լ�
	* �Ű����� :
	* string name : Model�� �̸�
	* Model* ptr : Model�� ������
	*/
	static int AddModel(string name, Model* ptr);

	/*
	* ���� : Mesh�� �̸��� Mesh �����͸� �޾� Mesh�� �߰��ϴ� �Լ�
	* �Ű����� :
	* string name : Mesh�� �̸�
	* Mesh* ptr : Mesh�� ������
	* zoneid : Zone�� id
	*/
	static int AddMeshInZone(string name, Mesh* ptr, int zoneid);

	/*
	* ���� : Model�� �̸��� Model �����͸� �޾� Model�� �߰��ϴ� �Լ�
	* �Ű����� :
	* string name : Model�� �̸�
	* Model* ptr : Model�� ������
	*/
	static int AddModelInZone(string name, Model* ptr, int zoneid);

	void GetRealShape(Mesh*& out0, Model*& out1) {
		if (isMesh()) out0 = reinterpret_cast<Mesh*>(FlagPtr & 0x7FFFFFFFFFFFFFFF);
		else out1 = reinterpret_cast<Model*>(FlagPtr & 0x7FFFFFFFFFFFFFFF);
	}
};

/*
* ���� : ���ӿ� ��Ÿ�� ��� �������� �����س��� ���̺�
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

	// bool�ε� �� �� ����.
	TagSetter operator[](UINT MaskIndex) {
		TagSetter ts;
		ts.t = this;
		ts.index = MaskIndex;
		return ts;
	}
};

enum GameObjectTag {
	Tag_Enable = 1, // ���ӿ�����Ʈ Ȱ��ȭ ����
	Tag_Dynamic = 2, // ���ӿ�����Ʈ�� ������ �� �ִ��� ����
	// ���� Tag_Dynamic == true ���.
	Tag_SkinMeshObject = 3,
	// ���� Tag_Dynamic == false ���.
};

struct GameObject {
#define STC_CurrentStruct GameObject
	STC_STATICINIT_innerStruct;

	/////////////////
	// ûũ�� ���� �� ƽ�� ���ӿ�����Ʈ �� �ѹ��� �ؾ��ϴ� �۾��� �ִٸ� �� ���� �������.
	UINT TourID = 0;

	/*
	* ���ӿ�����Ʈ�� �����ϰ� Ž���ϱ� ���� tag. 32���� tag�� ������ �� �ִ�.
	* �׻� tag�� ù��° ��Ʈ�� enable�̴�. (���ӿ�����Ʈ�� Ȱ��ȭ �Ǿ��ִ��� ����)
	*/
	STCDef(Tag, tag);

	// appearance
	STCDef(int, shapeindex);
	
	// �������� - GameObject�� �ε����� ����Ѵ�. vector<GameObject*> ObjectTable;
	STCDef(int, parent);
	STCDef(int, childs);
	STCDef(int, sibling);

	// �̰͵� ������ �ʿ��� �� ����.
	//STC
	union {
		int* material = nullptr; // mesh �� ��쿡�� Ȱ��ȭ��. slotNum��ŭ. game.MaterialTable���� ����.
		matrix* transforms_innerModel; // model �� ��츸 Ȱ��ȭ��. nodeCount ��ŭ.
	};

	// ���� �����ִ� Zone�� id.
	int zoneId = 0;

	static unsigned int _offset_fn_material() {
		GameObject obj{}; char* base = reinterpret_cast<char*>(&obj); char* mem = reinterpret_cast<char*>(&obj.material); return (mem - base);
	} inline static MemberInfo _reg_material{ "material", _offset_fn_material, sizeof(int*) };;
	static unsigned int _offset_fn_transforms_innerModel() {
		GameObject obj{}; char* base = reinterpret_cast<char*>(&obj); char* mem = reinterpret_cast<char*>(&obj.transforms_innerModel); return (mem - base);
	} inline static MemberInfo _reg_transforms_innerModel{ "transforms_innerModel", _offset_fn_transforms_innerModel, sizeof(matrix*) };;

	// transform (���� �÷��� ���������� ���� �����ϴ� ���� �ݱ��. - ûũ ������ ��Ʋ����.)
	STCDef(matrix, worldMat);

	//ó�� ���� �ÿ��� enable�� false. ������Ʈ�� ���� �ʴ´�. 
	// ������� �ʱ�ȭ�� �Ϸ�Ǿ� ���ӷ����� �� �غ� ��ġ�� �Ǹ� �׶� Enable�� �Ѵ�.
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
* ���� : ûũ�� ã�ư��� ���� �ε���
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

//�� ������ simd�� ����ȭ �����ϰڴ� ������ ���.
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
	* ���� : ���� ���� ûũ���� ������ �����ϰ�, local world matrix �� �����Ѵ�.
	* �� ��, �ٽ� �ٲ� ��ġ�� ûũ ������ �����Ѵ�.
	* �ܼ��� ������ �ٲٴ� �� �̻��� ������ �ϱ� ������ ���� ����ϴ°� ���� �ʴ�.
	* ��Ż�� ���� Ư�� ��Ȳ������ ����� �����ϴ�.
	* ��ġ�� �ٲٷ��� �׳� Velocity�� �����ϴ� ���� ����.
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
	* ���� : �� ���� ������Ʈ���� �����ӿ� ���� �浹�� ó���Ѵ�.
	* �Ű����� :
	* GameObject* gbj1 : ù��° ���ӿ�����Ʈ
	* GameObject* gbj2 : �ι�° ���ӿ�����Ʈ
	*/
	static void CollisionMove(DynamicGameObject* gbj1, DynamicGameObject* gbj2);

	/*
	* ���� : movObj�� �����̰�, colObj �� ������ ������, ���� �浹�� ���,
	* ���� �ڿ������� �̵��ϱ� ���ؼ� colObj�� ������ �������� �̵��� �����Ѵ�.
	* �Ű����� :
	* GameObject* movObj : �����̴� ���ӿ�����Ʈ
	* GameObject* colObj : �������ִ� ���ӿ�����Ʈ
	*
	* <CollisionMove_DivideBaseline_StaticOBB �� �ִµ� �� �� �Լ��� �ִ°�?>
	* >> �װ��� GameObject�� ������, ������ ����Կ� �־� �������� �ֱ� ������ �Լ��� ������� ��.
	*/
	static void CollisionMove_DivideBaseline(DynamicGameObject* movObj, DynamicGameObject* colObj);

	/*
	* ���� : movObj�� �����̰�, colOBB�� ���� �浹�� ���,
	* ���� �ڿ������� �̵��ϱ� ���ؼ� colOBB�� ������ �������� �̵��� �����Ѵ�.
	* �Ű����� :
	* GameObject* movObj : �����̴� ���ӿ�����Ʈ
	* BoundingOrientedBox colOBB : �������ִ� ���ӿ�����Ʈ
	*/
	static void CollisionMove_DivideBaseline_StaticOBB(DynamicGameObject* movObj, BoundingOrientedBox colOBB);

	/*
	* ���� : movObj�� preMove��ŭ ��������, �Լ� �����̰�, colOBB�� ���� �浹�� ���,
	* ���� �ڿ������� �̵��ϱ� ���ؼ� colOBB�� ������ �������� �̵��� �����Ѵ�.
	* �̶� colObj�� ���� ������ ����ϱ� ���� ���ȴ�.
	* �Ű����� :
	* GameObject* movObj : �����̴� ���ӿ�����Ʈ
	* BoundingOrientedBox colOBB : �������ִ� ���ӿ�����Ʈ
	*/
	static void CollisionMove_DivideBaseline_rest(DynamicGameObject* movObj, DynamicGameObject* colObj, BoundingOrientedBox colOBB, vec4 preMove);

	/*
	* ���� : ���ӿ�����Ʈ�� �浹�������� ȣ��Ǵ� �Լ�.
	* GameObject* other : �浹�� ������Ʈ
	*/
	virtual void OnCollision(GameObject* other);

	/*
	* ���� : ���ӿ�����Ʈ�� �������� �ʴ� Static �浹ü�� �浹�������� ȣ��Ǵ� �Լ�.
	* BoundingOrientedBox other : �浹�� OBB.
	*/
	virtual void OnStaticCollision(BoundingOrientedBox other);

	/*
	* ���� : ���ӿ�����Ʈ�� Ray�� �浹������ ȣ��Ǵ� �Լ�
	* GameObject* shooter : Ray�� �� ���� ������Ʈ
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
	* ���� : ���� ���� ûũ���� ������ �����ϰ�, local world matrix �� �����Ѵ�.
	* �� ��, �ٽ� �ٲ� ��ġ�� ûũ ������ �����Ѵ�.
	* �ܼ��� ������ �ٲٴ� �� �̻��� ������ �ϱ� ������ ���� ����ϴ°� ���� �ʴ�.
	* ��Ż�� ���� Ư�� ��Ȳ������ ����� �����ϴ�.
	* ��ġ�� �ٲٷ��� �׳� Velocity�� �����ϴ� ���� ����.
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
	float shootDelay;     // ���� �ӵ�
	float recoilVelocity; // �ݵ� ����
	float recoilDelay;    // �ݵ� ȸ�� �ð�
	float damage;         // �⺻ ������
	int maxBullets;       // źâ �뷮
	float reloadTime;     // ���� �ð�
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
	WeaponData m_info;      // GWeaponTable���� ������ ��ġ
	float m_shootFlow = 0;  // ���� �߻���� ���� �ð� ���
	float m_recoilFlow = 0; // �ݵ� �ִϸ��̼�/���� ��� �����

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
	* ���� : obb�� �� ûũ ���� ��´�.
	* ���� �ݵ��� �󸶳� ����Ǿ����� 0~1 ���� ������ ��ȯ
	*/
	float GetRecoilAlpha() const {
		float alpha = 1.0f - (m_recoilFlow / m_info.recoilDelay);
		return (alpha < 0) ? 0 : alpha;
	}
};

/*
* ���� : �÷��̾� ���� ������Ʈ ����ü
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
struct Player : public SkinMeshGameObject {
#define STC_CurrentStruct Player
	STC_STATICINIT_innerStruct;
	//STC HP
	STCDef(float, HP);
	//STC �ִ�HP
	STCDef(float, MaxHP);// = 100;
	//STC ������ź����
	STCDef(int, bullets);// = 100;
	//STC ���͸� ų�� ī��Ʈ (�ӽ� ����)
	STCDef(int, KillCount);// = 0;
	//STC ���� ī��Ʈ
	STCDef(int, DeathCount);// = 0;
	//STC ���� ������
	STCDef(float, HeatGauge);// = 0;
	//STC �ִ� ���� ������
	STCDef(float, MaxHeatGauge);// = 100;
	//STC player job
	STCDef(int, m_currentJob);
	//STC skill cooldown duration by slot
	STCDefArr(float, SkillCooldown, (int)SkillSlot::Max);
	//STC skill cooldown remaining by slot
	STCDefArr(float, SkillCooldownFlow, (int)SkillSlot::Max);
	//STC ���� ���� Ÿ��?
	STCDef(int, m_currentWeaponType);// = 0;
	//STC �÷��̾��� �κ��丮 ����
	static constexpr int maxItem = 49;
	STCDefArr(ItemStack, Inventory, maxItem);
	//STC ����ִ� ����
	STCDef(Weapon, weapon);
	//STC �÷��̾��� �� �� �̵� ��ٿ�
	STCDef(float, ZoneMoveCooldown);
	bool m_frostPassiveUsed = false;
	float m_tempMaxHpBonus = 0.0f;
	float m_tempMaxHpTimer = 0.0f;
	float m_iceBlockTimer = 0.0f;

	//ServerOnly �÷��̾ �� �� �̵��� �� ��, �ٽ� �̵��ϱ� ���� ���� ��ٿ� �ð��� ��Ÿ����. �̵��ϰ� ��� ���� ���������� ��� �̵��ϴ� ������ �߻��� �� ������.
	float zoneMoveCooldownRemain = 0.0f;
	int lastBoundaryIndex = -1;
	bool wasInsideBoundary = false;

	//ServerOnly ������
	float JumpVelocity = 5;
	//ServerOnly ���� ���� ����ִ����� ��Ÿ����.
	bool isGround = false;
	//ServerOnly �󸶳� ���� ���� ������Ʈ�� �浹�Ǿ������� ��Ÿ����.
	int collideCount = 0;
	//ServerOnly �ش� �÷��̾�� ���° Ŭ���̾�Ʈ�� ������ �ִ��� ��Ÿ����.
	int clientIndex = 0;

	//CTS �÷��̾ � Ű�� ������ �ִ����� ��Ÿ���� BoolBit �迭.
	BitBoolArr<2> InputBuffer;
	//CTS ���� �÷��̾ 1��Ī �������� ���� (�̰� ����ȭ�� �Ǵ� ���ΰ�??)
	bool bFirstPersonVision = true;
	//CTS �÷��̾ �ٶ󺸴� ����

	float m_yaw;
	float m_pitch;

	Player();

	virtual ~Player() { }

	/*
	* ���� : ���ӿ�����Ʈ�� ������Ʈ�� ������.
	* �Ű����� :
	* float deltaTime : ���� ������Ʈ ���� ���� ��������� �ð� ����.
	*/
	virtual void Update(float deltaTime) override;
	void ApplyJob(PlayerJob job);
	void SyncJobState(Zone* zones);
	bool TryUseSkill(SkillSlot slot);
	void UpdateSkillCooldowns(float deltaTime, Zone* zones);
	void UpdateJobTimers(float deltaTime, Zone* zones);

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

	/*
	* ����/��ȯ : ���ӿ�����Ʈ�� �浹 OBB ������ ��ȯ�Ѵ�.
	*/
	virtual BoundingOrientedBox GetOBB();

	/*
	* ���� : damage ��ŭ �÷��̾�� �������� �ش�.
	* �Ű����� :
	* float damage : �� ������ ��.
	*/
	void TakeDamage(float damage);

	/*
	* ���� : �÷��̾ ������ ��Ų��.
	*/
	void Respawn();

	/*
	* ���� : Ray�� �÷��̾ �浹������ ȣ��Ǵ� �Լ�
	* �Ű����� :
	* GameObject* shooter : Ray�� �� ���ӿ�����Ʈ
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
		//STC �ִ�HP
		float MaxHP = 100;
		//STC ������ź����
		int bullets = 100;
		//STC ���͸� ų�� ī��Ʈ (�ӽ� ����)
		int KillCount = 0;
		//STC ���� ī��Ʈ
		int DeathCount = 0;
		//STC ���� ������
		float HeatGauge = 0;
		//STC �ִ� ���� ������
		float MaxHeatGauge = 100;
		//STC player job
		int m_currentJob = (int)PlayerJob::Healer;
		//STC skill cooldown duration by slot
		float SkillCooldown[(int)SkillSlot::Max] = {};
		//STC skill cooldown remaining by slot
		float SkillCooldownFlow[(int)SkillSlot::Max] = {};
		//STC ���� ���� Ÿ��?
		int m_currentWeaponType = 0;

		////STC �÷��̾��� �κ��丮 ����
		//static constexpr int maxItem = 36;
		//ItemStack Inventory[maxItem];
		//STC ����ִ� ����
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

/*
* ���� : ���� ���� Ŭ����
*/
struct Monster : public SkinMeshGameObject {
#define STC_CurrentStruct Monster
	STC_STATICINIT_innerStruct;

	//STC ü��
	STCDef(float, HP); // = 30;
	//STC �ִ�ü��
	STCDef(float, MaxHP); // = 30;
	//STC ���� �׾����� ����
	STCDef(bool, isDead);// = false;
	
	//ServerOnly ó�� �����Ǿ��� ��ǥ
	vec4 m_homePos;
	//ServerOnly ���ݴ���� ��ǥ
	vec4 m_targetPos;
	//STC �ɾ�� �ӵ� 
	float m_speed = 2.0f;
	//ServerOnly ���������� ����������
	float m_patrolRange = 20.0f;
	//ServerOnly �i�ư��� �����ϴ� ������ ������
	float m_chaseRange = 10.0f;
	//ServerOnly ??
	float m_patrolTimer = 0.0f;
	//ServerOnly ���� �߻��ϴ� ����
	float m_fireDelay = 1.0f;
	//ServerOnly ���� �߻��ϰ� ���� �ð������ ���� Ÿ�̸�
	float m_fireTimer = 0.0f;
	//ServerOnly ���� �浹�� ������ ����
	int collideCount = 0;
	//ServerOnly ??
	int targetSeekIndex = 0;
	//ServerOnly ���ݴ�� 
	Player** Target = nullptr;
	//ServerOnly ���� �����̰� �ִ��� ����
	bool m_isMove = false;
	//ServerOnly ���� ���� �پ��ִ��� ����
	bool isGround = false;
	//ServerOnly �������� ���Ǵ� Ÿ�̸�
	float respawntimer = 0;
	//ServerOnly ??
	float pathfindTimer = 0.0f;

	Monster();
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
	std::vector<AstarNode*> path; // ���� ���󰡾� �� ���
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

		//STC ü��
		float HP = 30;
		//STC �ִ�ü��
		float MaxHP = 30;
		//STC ���� �׾����� ����
		bool isDead = false;
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
* ���� : ûũ.
* �������� OBB�� �����ϰ� �ִ�.
* Sentinal Value :
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
* ���� : ChunckIndex�� �ؽ�
* x, y, z�� �� ��Ʈ�� ���ư��鼭 �����ϴ� ���.
* pdep�� �̿��� �װ��� ������ �����Ѵ�.
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
* ���� : ������ ��.
* Sentinal Value :
* NULL = 
* (name.size() == 0 && meshes.size() == 0 && 
* mesh_shapeindexes == 0 && models.size() == 0 && 
* MapObjects.size() == 0)
*/
struct GameMap {
	GameMap() {}
	~GameMap() {}
	//���� �پ��� ��ü (������Ʈ, �޽�, �ִϸ��̼�, �ؽ���, ���͸���)���� ���� �� ���Ǵ� �ߺ������� �̸��� ������.
	vector<string> name;

	//���� �޽������� �迭
	vector<Mesh> meshes;

	//Mesh�� MeshName�� Shape::AddMesh �Լ��κ��� �ε�Ǵ� Shape���� �ε��� �迭
	vector<int> mesh_shapeindexes;
	
	//���� �迭
	vector<Model*> models;

	//Shape::AddMesh �Լ��κ��� �ε�� ���� Shape���� �ε��� �迭
	vector<int> model_shapeindexes;

	//�ʿ� ������ �ִ� �浹ó���� �� �������� ������Ʈ
	vector<StaticGameObject*> MapObjects;

	// �� ��ü ������ AABB
	vec4 AABB[2] = { 0, 0 };

	//���� ���� ��
	Zone* ownerzone = nullptr;

	unsigned int TextureTableStart = 0;
	unsigned int MaterialTableStart = 0;

	/*
	* ���� : OBB�� �ް�, �װ��� ���� �� ��ü�� AABB�� Ȯ���Ѵ�.
	* �Ű����� : 
	* BoundingOrientedBox obb : ���� OBB
	*/
	void ExtendMapAABB(BoundingOrientedBox obb);
	
	/*
	* ���� : Map�� ��ü StaticCollision�� ����Ͽ� Chunk���� �����ϰ�,
	* �� ��ü ������ AABB�� ���ϴ� �Լ�
	*/
	void BakeStaticCollision();

	/*
	* ���� : � obj�� �����϶�, Map�� StaticCollision�� ���Ͽ� �浹�� ����ϴ� �Լ�.
	* �Ű����� : 
	* GameObject* obj : �浹 ������ �� �����̴� ������Ʈ
	*/
	void StaticCollisionMove(DynamicGameObject* obj);

	/*
	* ���� : � obb�� Map�� StaticCollision�� ���Ͽ� �浹�ϴ��� ����ϴ� �Լ�
	* �Ű����� : 
	* BoundingOrientedBox obb : �浹�� �˻��� obb.
	* ��ȯ : 
	* �浹�� true, �ƴϸ� false
	*/
	bool isStaticCollision(BoundingOrientedBox obb);

	/*
	* ���� : ��ü ���� �ε��Ѵ�.
	* �Ű����� : 
	* const char* MapName : �� ������ �̸�. ��ΰ� �ƴϴ� Ȯ���� ���� ������ �ʰ�, ���� ������ �̸��� ���´�.
	*	�ش� �̸��� ���� .map ������ Resource/Map ��ο� �־�� �Ѵ�.
	*/
	void LoadMap(const char* MapName);
};

/*
���� : PACK ���꿡 ���� �ѹ��� send�� ������ �ִ� ������ ������.
*/
struct twoPage {
	char data[8196] = {};
};

/*
���� : ������ Client���� ������ �ִ� Ŭ���̾�Ʈ�� ������.
*/
struct ClientData {
	//������ Client�� ����
	SOCKET socket;
	
	// Ŭ���̾�Ʈ�κ��� �޴� �����͸� �����ϴ� ����
	static constexpr int rbufcap = 8192 - sizeof(int);
	char rbuf[rbufcap + sizeof(int)] = {};
	int rbufoffset = 0;

	// Ŭ���̾�Ʈ �����ּ�
	NWAddr addr;

	//Client�� �����ϴ� ������ ���ӿ�����Ʈ(Player)
	Player* pObjData;

	//pObjData �� ���� gameworld GameObject �迭���� ���° �ε����� ��ġ�ϴ��� ��Ÿ����.
	int objindex = 0;

	//�� Ŭ���̾�Ʈ�� ���� zoneId
	int zoneId = 0;

	//Send�ϴ� �����͸� �׾Ƴ��� ��.
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

// ���漱��
struct World;
struct Zone;
struct Portal;

/*
* ���� :���� ��� ����. ���� ��迡 ������ �� ���̸� �̵��ϱ� ����.
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
* ���� : ���� ���� ���� �������� ����.
* �� Zone�� ��ü���� ������Ʈ �迭, ��, ûũ, Astar �׸��带 �����Ѵ�.
* Zone �� �̵��� Portal�� ���� �̷������.
*/
struct Zone {
	// �Ҽ� World ������
	World* world = nullptr;

	// �� ID
	int zoneId = 0;

	// ���� ������Ʈ �迭
	vecset<DynamicGameObject*> Dynamic_gameObjects;
	vecset<StaticGameObject*> Static_gameObjects;

	// ��Ż �迭 (���� ����)
	vector<Portal*> portals;

	// ��ӵ� �����۵��� �迭
	vecset<ItemLoot> DropedItems;

	// ��� Ŭ���̾�Ʈ���� ���޵� �� ���� ���� ������
	SendDataSaver CommonSDS;

	// ���� �� ������
	GameMap map;

	// ���� ��� ���� (���� ��迡 ������ �� ���̸� �̵��ϱ� ����)
	vector<Zoneboundary> boundaries;

	// Astar pathfinding
	vector<AstarNode*> allnodes;
	static constexpr float AstarStartX = -40.0f;
	static constexpr float AstarStartZ = -40.0f;
	static constexpr float AstarEndX = 40.0f;
	static constexpr float AstarEndZ = 40.0f;

	// ���� �������� ������Ʈ �ε���
	int currentIndex = 0;

	// �� ����
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

	// �ϳ��� ûũ�� ������ü�� �� ���� ���̸� �����Ѵ�.
	static constexpr float chunck_divide_Width = 50.0f;

	// ���ӳ��� Chunck���� ����.
	unordered_map<ChunkIndex, GameChunk*> chunck;

	// Zone���� ShapeTable (�Ʒ� 3���� �ڷᱸ���� ������ zone������ �ε����θ� �����Ѵ�.)
	// �ܺο��� ����ϱ� ���� �Ű��� �� ����� �Ѵ�.
	vector<Shape> ZoneShapeTable;
	unordered_map<string, int> ZoneStrToShapeIndex;
	vector<string> ZoneShapeStrTable;
	Shape& GetShape(int shapeindex);

	int ZoneTextureCount = 0;
	int ZoneMaterialCount = 0;

	// ������
	Zone() : world(nullptr), zoneId(0) {}
	Zone(World* w, int id) : world(w), zoneId(id) {}

	/*
	* ���� : Zone�� �ʱ�ȭ�Ѵ�.
	* Astar �׸���, ������Ʈ �迭, �� �ε�, ���� ����, ��Ż ���� ���� ����.
	*/
	void Init();

	/*
	* ���� : Zone�� DeltaTime��ŭ ������Ʈ�Ѵ�.
	* ������Ʈ ������Ʈ, ûũ ��� �浹 ó��, ��Ż �˻縦 ����.
	*/
	void Update(float deltaTime);

	// ===== ������Ʈ ���� =====

	/*
	* ���� : ���ο� Dynamic ������Ʈ�� �� Zone�� �߰��Ѵ�.
	*/
	int NewObject(DynamicGameObject* obj, GameObjectType gotype);

	/*
	* ���� : ���ο� �÷��̾ �� Zone�� �߰��Ѵ�.
	*/
	int NewPlayer(SendDataSaver& sds, Player* obj, int clientIndex);

	/*
	* ���� : �÷��̾ �� Zone���� �����Ѵ�. (�޸� ���� X)
	*/
	void RemovePlayer(int clientIndex);

	/*
	* ���� : �ٸ� Zone���� �� �÷��̾ �� Zone�� �߰��Ѵ�.
	*/
	int AddPlayer(int clientIndex, Player* player, vec4 spawnPos, bool update_Map = true);

	// ===== ��Ż ���� =====

	void SpawnPortal();
	void Spawnboundary();
	void CheckPortalCollision(Player* p);
	void CheckBoundaryCrossing(Player* p, float deltaTime);

	// ===== ���� ���� =====

	/*
	* ���� : �� Zone�� ��� Ŭ���̾�Ʈ���� CommonSDS + PersonalSDS�� �����Ѵ�.
	*/
	void FlushSendToClients();

	/*
	* ���� : �� Ŭ���̾�Ʈ���� �� Zone�� ��� ������Ʈ ������ �����Ѵ�.
	*/
	void SendingAllObjectForNewClient(SendDataSaver& sds);

	// ===== Sending ���� =====

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


	// ===== �浹/����ĳ��Ʈ =====

	void FireRaycast(GameObject* shooter, vec4 rayStart, vec4 rayDirection, float rayDistance, float damage);

	void GridCollisionCheck();

	// ===== ûũ ���� =====

	GameObjectIncludeChunks GetChunks_Include_OBB(BoundingOrientedBox obb);
	GameChunk* GetChunkFromPos(vec4 pos);
	void PushGameObject(GameObject* go);

	// ===== ��Ÿ =====

	void LoadMapForZone(int zoneId);
	void SpawnObjects();
	void PrintCangoGrid(const std::vector<AstarNode*>& all, int gridWidth, int gridHeight);
	bool CheckAABBSphereCollision(const vec4& boxCenter, const vec4& boxHalfSize, const collisionchecksphere& sphere);
};

/*
* ���� : ������ ���ư��� ���� ����ü.
* Zone���� �����ϰ�, Ŭ���̾�Ʈ ����/�Է� ó���� ����Ѵ�.
*/
struct World {
	// Ŭ���̾�Ʈ �迭
	vecset<ClientData> clients;

	// TODO : <������ �� ��. PACK�� ������ ��.>
	twoPage tempbuffer;

	// �۷ι� �ؽ��� ī��Ʈ
	unsigned int GlobalTextureCount = 0;
	// �۷ι� ���͸��� ī��Ʈ
	unsigned int GlobalMaterialSiz = 0;
	unsigned int GlobalMaterialCount = 0;
	// �۷ι� �޽� ī��Ʈ
	unsigned int GlobalMeshCount = 0;
	// �۷ι� �޸ӳ��̵� �ִϸ��̼� ī��Ʈ
	unsigned int GlobalHumanoidAnimaionCount = 0;
	// Sync�� ShapeTable �׸� ����
	unsigned int GlobalShapeTableSyncSiz = 0;

	// �޸ӳ��̵� �ִϸ��̼� ���̺�
	vector<HumanoidAnimation> HumanoidAnimationTable;

	// ===== Zone ���� =====
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
	* ���� : ���Ӽ����� �ʱ�ȭ�Ѵ�.
	* 1. ������ ���̺� ����
	* 2. Ŭ���̾�Ʈ/������ ����ȭ�� ���� Offset ����
	* 3. ��/�޽� �ε� (���� ���ҽ�)
	* 4. �� Zone �ʱ�ȭ
	*/
	void Init();

	/*
	* ���� : ������ DeltaTime ��ŭ ������Ʈ �Ѵ�.
	* �� Zone�� Update�� ȣ���Ѵ�.
	*/
	void Update();

	/*
	* ���� : clientIndex ��° Ŭ���̾�Ʈ�� rBuffer �����͸� ������ �������� ó��.
	*/
	__forceinline int Receiving(int clientIndex, char* rBuffer, int totallen);

	/*
	* ���� : Ŭ���̾�Ʈ���� �ڽ��� �÷��̾� �ε����� �����Ѵ�.
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
	* ���� : ���� ���¸� ����ȭ�Ѵ�.
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
	* ���� : �÷��̾ srcZone���� dstZone���� �̵���Ų��.
	*/
	void MovePlayerToZone(int clientIndex, int dstZoneId, vec4 spawnPos);

	/*
	* ���� : �÷��̾ ���� zone�� ã�´�.
	*/
	Zone* GetClientZone(int clientIndex);

	/*
	* ���� : ���� �ִ� ��id�� ã�´�
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


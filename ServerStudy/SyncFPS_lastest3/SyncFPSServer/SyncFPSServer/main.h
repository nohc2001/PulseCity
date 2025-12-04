// Including SDKDDKVer.h defines the highest available Windows platform.

// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.

#include "stdafx.h"
#include "GameObjectTypes.h"
#include "Monster.h"
#include "Player.h"

enum InputID {
	KeyboardW = 'W',
	KeyboardA = 'A',
	KeyboardS = 'S',
	KeyboardD = 'D',
	KeyboardSpace = VK_SPACE,
	MouseLbutton = 5,
	MouseRbutton = 6,
	MouseMove = 7,
};

union SendingType {
	enum {
		NullType = 0,
		NewGameObject = 1, // [st] [new obj index] [type of gameobject] [gameobject raw data]
		ChangeMemberOfGameObject = 2,
		// [st(2)] [obj index(int) (4)] [type of gameobject(2)] [client member offset(short)] [memberSize (2)] [rawData (member size)]
		NewRay = 3, // [st] [Ray raw data (include distance which determined by raycast)]
		SetMeshInGameObject = 4, // [st] [obj index] [strlen] [string data]
		AllocPlayerIndexes = 5, // [st] [client Index] [Object Index]
		DeleteGameObject = 6, // [st] [obj index]
		PACK = 7, PACK_END = 8, // [st] [pack order index] [~ data ~] (siz = 8196)
		ItemDrop = 9, // [st] [dropindex] [lootdata]
		ItemDropRemove = 10, //[st] [dropindex]
		InventoryItemSync = 11, //[st] [lootdata] [inventory index]
	};
	short n;
	char two_byte[2];

	SendingType(short id) { n = id; }
	operator short() { return n; }
};

struct twoPage {
	char data[8196] = {};
};

//socket memory size is too big -> because recieve buffer..
struct ClientData {
	Socket socket;
	XMVECTOR pos;

	bool isPosUpdate = false;
	bool Connected = false;
	Player* pObjData;
	int objindex = 0;
	static constexpr float MoveSpeed = 3.0f;
};

struct DataPackFactory {
	struct DataPack {
		twoPage data;
		int up = 0;

		void Clear(int data_order_id) {
			up = 0;
			ZeroMemory(&data, sizeof(twoPage));
			*(short*)&data.data[up] = SendingType::PACK;
			up += sizeof(short);
			*(int*)&data.data[up] = data_order_id;
			up += sizeof(int);
			int dataSiz = 0;
			*(int*)&data.data[up] = dataSiz;
			up += sizeof(int);
		}

		bool push_data(int datacap, void* dataptr) {
			if (up + datacap <= sizeof(twoPage)) {
				memcpy_s(&data.data[up], datacap, dataptr, datacap);
				up += datacap;
				return true;
			}
			else return false;
		}
	};

private:
	vector<DataPack> packs;
	int pack_up = 0;

	void push_pack() {
		packs.push_back(DataPack());
		packs[pack_up].Clear(pack_up);
		pack_up += 1;
	}

public:
	operator vector<DataPack>() { return packs; }

	void Clear() {
		packs.clear();
		pack_up = 0;
		push_pack();
	}

	void push_data(int datacap, void* dataptr) {
		bool b = packs[pack_up - 1].push_data(datacap, dataptr);
		if (b == false) {
			*(int*)&packs[pack_up - 1].data.data[6] = packs[pack_up - 1].up;
			push_pack();
			packs[pack_up - 1].push_data(datacap, dataptr);
		}
	}

	void send(Socket* sock) {
		*(short*)packs[packs.size() - 1].data.data = SendingType::PACK_END;
		*(int*)&packs[pack_up - 1].data.data[6] = packs[pack_up - 1].up;
		for (int i = 0; i < pack_up; ++i) {
			sock->Send((char*)&packs[i].data, packs[i].up);
		}
		Clear();
	}
};

struct collisionchecksphere {
	vec4 center;
	float radius;
};

struct World {
	vecset<ClientData> clients; // players
	vecset<GameObject*> gameObjects;
	vecset<ItemLoot> DropedItems;
	vector<AstarNode*> allnodes; //Astar pathfinding

	twoPage tempbuffer;
	DataPackFactory pack_factory;

	int currentIndex = 0; // vector stack??

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
		return highFrequencyFlow > midFrequencyDelay;
	}

	GameMap map;

	void Init();
	void Update();
	void gridcollisioncheck();
	__forceinline int Receiving(int clientIndex, char* rBuffer);

	int NewObject(GameObject* obj, GameObjectType gotype);
	int NewPlayer(Player* obj, int clientIndex);

	__forceinline int Sending_NewGameObject(int newindex, GameObject* newobj, GameObjectType gotype);
	__forceinline int Sending_ChangeGameObjectMember(int objindex, GameObject* newobj, GameObjectType gotype, void* memberAddr, int memberSize);
	__forceinline int Sending_NewRay(vec4 rayStart, vec4 rayDirection, float rayDistance);
	__forceinline int Sending_SetMeshInGameObject(int objindex, string str);
	void FireRaycast(GameObject* shooter, vec4 rayStart, vec4 rayDirection, float rayDistance);
	int Sending_AllocPlayerIndex(int clientindex, int objindex);
	int Sending_DeleteGameObject(int objindex);
	int Sending_ItemDrop(int dropindex, ItemLoot lootdata);
	int Sending_ItemRemove(int dropindex);
	int Sending_InventoryItemSync(ItemStack lootdata, int inventoryIndex);

	__forceinline void SendToAllClient(int datacap) {
		for (int i = 0; i < clients.size; ++i) {
			if (clients.isnull(i)) continue;
			clients[i].socket.Send(tempbuffer.data, datacap);
		}
	}

	__forceinline void SendToAllClient_execept(int datacap, int execept) {
		for (int i = 0; i < execept; ++i) {
			if (clients.isnull(i)) continue;
			clients[i].socket.Send(tempbuffer.data, datacap);
		}
		for (int i = execept + 1; i < clients.size; ++i) {
			if (clients.isnull(i)) continue;
			clients[i].socket.Send(tempbuffer.data, datacap);
		}
	}
	//9월 9일 
	//void DestroyObject(int index);

	void SendingAllObjectForNewClient(int new_client_index) {
		pack_factory.Clear();
		for (int i = 0; i < gameObjects.size; ++i) {
			if (gameObjects.isnull(i)) continue;
			void* vptr = *(void**)gameObjects[i];
			int datacap = Sending_NewGameObject(i, gameObjects[i], GameObjectType::VptrToTypeTable[vptr]);
			pack_factory.push_data(datacap, tempbuffer.data);
			//clients[new_client_index].socket.Send((char*)tempbuffer.data, datacap);

			if (gameObjects[i]->ShapeIndex >= 0) {
				datacap = Sending_SetMeshInGameObject(i, Shape::ShapeNameArr[gameObjects[i]->ShapeIndex]);
				pack_factory.push_data(datacap, tempbuffer.data);
				//clients[new_client_index].socket.Send((char*)tempbuffer.data, datacap);
			}

			if (GameObjectType::VptrToTypeTable[vptr] == GameObjectType::_Monster) {
				int datacap = Sending_ChangeGameObjectMember(i, gameObjects[i], GameObjectType::_Monster, &((Monster*)gameObjects[i])->isDead, sizeof(bool));
				pack_factory.push_data(datacap, tempbuffer.data);
				//clients[new_client_index].socket.Send((char*)tempbuffer.data, datacap);

				datacap = Sending_ChangeGameObjectMember(i, gameObjects[i], GameObjectType::_Monster, &((Monster*)gameObjects[i])->HP, sizeof(float));
				pack_factory.push_data(datacap, tempbuffer.data);
				//clients[new_client_index].socket.Send((char*)tempbuffer.data, datacap);

				datacap = Sending_ChangeGameObjectMember(i, gameObjects[i], GameObjectType::_Monster, &((Monster*)gameObjects[i])->MaxHP, sizeof(float));
				pack_factory.push_data(datacap, tempbuffer.data);
				//clients[new_client_index].socket.Send((char*)tempbuffer.data, datacap);
			}
			else if (GameObjectType::VptrToTypeTable[vptr] == GameObjectType::_Player) {
				int datacap = Sending_ChangeGameObjectMember(i, gameObjects[i], GameObjectType::_Player, &((Player*)gameObjects[i])->KillCount, sizeof(int));
				pack_factory.push_data(datacap, tempbuffer.data);
				//clients[new_client_index].socket.Send((char*)tempbuffer.data, datacap);

				datacap = Sending_ChangeGameObjectMember(i, gameObjects[i], GameObjectType::_Player, &((Player*)gameObjects[i])->DeathCount, sizeof(int));
				pack_factory.push_data(datacap, tempbuffer.data);
				//clients[new_client_index].socket.Send((char*)tempbuffer.data, datacap);

				datacap = Sending_ChangeGameObjectMember(i, gameObjects[i], GameObjectType::_Player, &((Player*)gameObjects[i])->bullets, sizeof(int));
				pack_factory.push_data(datacap, tempbuffer.data);
				//clients[new_client_index].socket.Send((char*)tempbuffer.data, datacap);
			}
		}

		for (int i = 0; i < DropedItems.size; ++i) {
			if (DropedItems.isnull(i)) continue;
			int datacap = Sending_ItemDrop(i, DropedItems[i]);
			pack_factory.push_data(datacap, tempbuffer.data);
		}
	}

	//09.08 수정

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
};

extern World gameworld;
extern float DeltaTime;

void PrintOffset();
bool CheckAABBSphereCollision(const vec4& boxCenter, const vec4& boxHalfSize, const collisionchecksphere& sphere);
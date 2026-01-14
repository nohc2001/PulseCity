#include "stdafx.h"
#include "GameObjectTypes.h"
#include "Monster.h"
#include "Player.h"

/*
설명 : Client의 입력을 구분하는 enum
*/
enum InputID {
	KeyboardW = 'W',
	KeyboardA = 'A',
	KeyboardS = 'S',
	KeyboardD = 'D',
	KeyboardQ = 'Q',
	KeyboardSpace = VK_SPACE,
	MouseLbutton = 5,
	MouseRbutton = 6,
	MouseMove = 7,
};

/*
설명 : Server 에서 Client로 보내는 데이터 프로토콜의 타입
Sentinal Value :
NULL = (short)0
*/
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

	// enum을 숫자로 나타낸 것.
	short n;
	char two_byte[2];

	SendingType(short id) { n = id; }
	operator short() { return n; }
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
	Socket socket;

	//Client가 조작하는 서버내 게임오브젝트(Player)
	Player* pObjData;

	//pObjData 가 서버 gameworld GameObject 배열에서 몇번째 인덱스에 위치하는지 나타낸다.
	int objindex = 0;
};

/*
설명 : <TCP IP에 대한 오해로부터 만들어진 구조체>
때문에 이후 수정이 필요. PACK이라는 프로토콜은 쓸모가 없기 때문.
*/
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
		cout << datacap << endl;
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

/*
* 설명 : 게임이 돌아가는 월드 구조체.
*/
struct World {
	// 클라이언트 배열
	vecset<ClientData> clients;

	// 게임 오브젝트 배열
	vecset<GameObject*> gameObjects;

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
	DataPackFactory pack_factory;

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
	__forceinline int Receiving(int clientIndex, char* rBuffer);

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
	int NewObject(GameObject* obj, GameObjectType gotype);

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
	int NewPlayer(Player* obj, int clientIndex);

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
	__forceinline int Sending_NewGameObject(int newindex, GameObject* newobj, GameObjectType gotype);

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
	__forceinline int Sending_ChangeGameObjectMember(int objindex, GameObject* ptrobj, GameObjectType gotype, void* memberAddr, int memberSize);

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
	__forceinline int Sending_NewRay(vec4 rayStart, vec4 rayDirection, float rayDistance);

	/*
	* 설명 : 클라이언트 오브젝트의 Mesh 데이터를 수정하기 위해
	* 패킷 데이터를 tempbuffer에 구성한다.
	* 매개변수 :
	* int objindex : 오브젝트의 인덱스
	* string str : Mesh에 접근할 수 있는 문자열 key.
	* 반환 :
	* 구성된 패킷의 사이즈를 반환
	*/
	__forceinline int Sending_SetMeshInGameObject(int objindex, string str);

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
	void FireRaycast(GameObject* shooter, vec4 rayStart, vec4 rayDirection, float rayDistance);

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
	int Sending_AllocPlayerIndex(int clientindex, int objindex);

	/*
	* 설명 : 클라이언트 오브젝트를 삭제하기 위해
	* 패킷 데이터를 tempbuffer에 구성한다.
	* 매개변수 :
	* int objindex : 삭제될 오브젝트의 인덱스
	* 반환 :
	* 구성된 패킷의 사이즈를 반환
	*/
	int Sending_DeleteGameObject(int objindex);

	/*
	* 설명 : 아이템이 드롭되는 것을 클라이언트에게 전달하기 위해
	* 패킷 데이터를 tempbuffer에 구성한다.
	* 매개변수 :
	* int dropindex : 드롭된 아이템이 DropedItems 배열에 어떤 인덱스에 위치하는지
	* ItemLoot lootdata : 드롭된 아이템 데이터
	* 반환 : 
	* 구성된 패킷의 사이즈를 반환
	*/
	int Sending_ItemDrop(int dropindex, ItemLoot lootdata);

	/*
	* 설명 : 드롭된 아이템이 삭제되는 것을 클라이언트에게 전달하기 위해
	* 패킷 데이터를 tempbuffer에 구성한다.
	* 매개변수 :
	* int dropindex : 삭제될 드롭된 아이템이 DropedItems 배열에 어떤 인덱스에 위치하는지
	* 반환 :
	* 구성된 패킷의 사이즈를 반환
	*/
	int Sending_ItemRemove(int dropindex);

	/*
	* 설명 : 인벤토리의 아이템 데이터를 동기하기 위해
	* 패킷 데이터를 tempbuffer에 구성한다.
	* 매개변수 :
	* ItemStack lootdata : 인벤토리에 들어갈 아이템스택의 정보
	* int inventoryIndex : 인벤토리 몇번째 칸인지 결정
	* 반환 :
	* 구성된 패킷의 사이즈를 반환
	*/
	int Sending_InventoryItemSync(ItemStack lootdata, int inventoryIndex);

	/*
	* 설명 : tempbuffer에 저장된 패킷 데이터를 datacap 만큼 모든 클라이언트에게 전송한다.
	* 매개변수 : 
	* int datacap : 보낼 데이터의 크기
	*/
	__forceinline void SendToAllClient(int datacap) {
		for (int i = 0; i < clients.size; ++i) {
			if (clients.isnull(i)) continue;
			clients[i].socket.Send(tempbuffer.data, datacap);
		}
	}

	/*
	* 설명 : tempbuffer에 저장된 패킷 데이터를 datacap 만큼 
	* [execept 번째 클라이언트를 제외한]
	* 모든 클라이언트에게 전송한다.
	* 매개변수 :
	* int datacap : 보낼 데이터의 크기
	* int execept : 제외할 클라이언트 번호
	*/
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

	////temp 2025.9.9 <??> 
	//void DestroyObject(int index);

	/*
	* 설명 : 새로운 클라이언트를 위해 현재 클라이언트와 공유할 모든 서버의 정보를 
	* 보내기 위한 패킷을 pack_factory에 구성한다.
	* <PACK 프로토콜 >이 쓰여서 수정할 필요가 있다.
	* 매개변수 : 
	* int new_client_index : 새로운 클라이언트의 번호
	*/
	void SendingAllObjectForNewClient() {
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
};

extern World gameworld;
extern float DeltaTime;

/*
* 설명 : 서버의 GameObject를 상속한 구조체들의 
* Offset을 볼 수 있도록 출력하는 함수
*/
void PrintOffset();

bool CheckAABBSphereCollision(const vec4& boxCenter, const vec4& boxHalfSize, const collisionchecksphere& sphere);
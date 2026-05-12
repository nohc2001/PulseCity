#pragma once
#include "stdafx.h"

struct Client {
	static constexpr int rbufMax = 8192 - sizeof(int);
	SOCKET sock;
	char rBuf[rbufMax + sizeof(int)] = {};
	int rbufOffset = 0;

	bool Init(const char* ServerIP, unsigned short ServerPort) {
		sock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (sock == INVALID_SOCKET) {
			int wsaErr = WSAGetLastError();
			char _dbg[160] = {};
			sprintf_s(_dbg, "[Client::Init] WSASocket FAILED err=%d ip=\"%s\" port=%u\n", wsaErr, ServerIP ? ServerIP : "(null)", (unsigned)ServerPort);
			OutputDebugStringA(_dbg); printf("%s", _dbg); fflush(stdout);
			return false;
		}

		struct sockaddr_in serveraddr;
		memset(&serveraddr, 0, sizeof(serveraddr));
		serveraddr.sin_family = AF_INET;
		int ptonResult = inet_pton(AF_INET, ServerIP, &serveraddr.sin_addr);
		serveraddr.sin_port = htons(ServerPort);
		{
			char _dbg[160] = {};
			sprintf_s(_dbg, "[Client::Init] try connect ip=\"%s\" port=%u pton=%d\n", ServerIP ? ServerIP : "(null)", (unsigned)ServerPort, ptonResult);
			OutputDebugStringA(_dbg); printf("%s", _dbg); fflush(stdout);
		}
		int retval = connect(sock, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
		if (retval == SOCKET_ERROR) {
			int err = WSAGetLastError();
			char _dbg[160] = {};
			sprintf_s(_dbg, "[Client::Init] connect SOCKET_ERROR err=%d\n", err);
			OutputDebugStringA(_dbg); printf("%s", _dbg); fflush(stdout);
			if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS && err != WSAEINVAL) {
				closesocket(sock);
				sock = INVALID_SOCKET;
				return false;
			}
		}
		else {
			char _dbg[80] = {};
			sprintf_s(_dbg, "[Client::Init] connect OK (retval=0)\n");
			OutputDebugStringA(_dbg); printf("%s", _dbg); fflush(stdout);
		}

		u_long val = 1;
		int ret = ioctlsocket(sock, FIONBIO, &val);
		if (ret != 0)
		{
			int wsaErr = WSAGetLastError();
			char _dbg[160] = {};
			sprintf_s(_dbg, "[Client::Init] ioctlsocket FAILED err=%d\n", wsaErr);
			OutputDebugStringA(_dbg); printf("%s", _dbg); fflush(stdout);
			closesocket(sock);
			sock = INVALID_SOCKET;
			return false;
		}

		return true;
	}

	void ResetRecvBuffer() {
		memset(rBuf, 0, sizeof(rBuf));
		rbufOffset = 0;
	}

	void Disconnect() {
		if (sock != INVALID_SOCKET) {
			closesocket(sock);
			sock = INVALID_SOCKET;
		}
		ResetRecvBuffer();
	}

	__forceinline DWORD send(char* data, int len, DWORD flag) {
		WSABUF buf;
		buf.buf = data;
		buf.len = len;
		DWORD retval = 0;
		int err = WSASend(sock, &buf, 1, &retval, flag, NULL, NULL);
		if (err == SOCKET_ERROR) {
			int wsaErr = WSAGetLastError();
			char dbg[128] = {};
			sprintf_s(dbg, "[ClientSend] WSASend failed err=%d len=%d\n", wsaErr, len);
			OutputDebugStringA(dbg);
		}
		return retval;
	}

	__forceinline int recv(char* data, int len) {
		WSABUF buf;
		buf.buf = data;
		buf.len = len;
		DWORD retval = 0;
		DWORD flag = 0;
		int err = WSARecv(sock, &buf, 1, &retval, &flag, NULL, NULL);
		if (err == SOCKET_ERROR) {
			return -1;
		}
		return (int)retval;
	}
};

typedef int ItemID;

/*
* ���� : �κ��丮�� �� ������ ���� ����
* Sentinal Value :
* NULL : (ItemCount == 0)
*/
struct ItemStack {
	//������ id
	ItemID id;
	//�������� ����
	int ItemCount;
};

/*
* ���� : ��ӵ� �������� ��Ÿ���� ����ü
* Sentinal Value :
* NULL = (itemDrop.ItemCount == 0)
*/
struct ItemLoot {
	// ������ ���� ����
	ItemStack itemDrop;
	// ������ ��� ��ġ
	// improve : <�������� �ٴ����� �߷��� �ۿ�Ǿ����� ������.>
	vec4 pos;
};

/*
* ������ 2��. 8196 ����Ʈ
*/
struct twoPage {
	char data[8196] = {};
};

//Protocol

/*
* ���� : �����κ��� �޴� ���������� Ÿ���� �з��ϴ� enum
* Sentinal Value : 
* NULL = 0;
*/
/*
���� : Server ���� Client�� ������ ������ ���������� Ÿ��
Sentinal Value :
NULL = (short)0
*/
//
//#pragma pack(push, 1)
//union STCProtocol {
//	enum {
//		NullType = 0,
//		//there is no format
//
//		SyncGameObject = 1,
//		// [int size] [int sendttype] [int newobj_index] [type of gameobject] [gameobject raw data]
//
//		ChangeMemberOfGameObject = 2,
//		// [int size] [st(2)] [obj index(int) (4)] [type of gameobject(2)] [client member offset(short)] [memberSize (2)] [rawData (member size)]
//
//		NewRay = 3,
//		// [int size] [st] [Ray raw data (include distance which determined by raycast)]
//
//		AllocPlayerIndexes = 4,
//		// [int size] [st] [client Index] [Object Index]
//
//		DeleteGameObject = 5,
//		// [int size] [st] [obj index]
//
//		ItemDrop = 6,
//		// [int size] [st] [dropindex] [lootdata]
//
//		ItemDropRemove = 7,
//		// [int size] [st] [dropindex]
//
//		InventoryItemSync = 8,
//		// [int size] [st] [lootdata] [inventory index]
//
//		PlayerFire = 9,
//		// [int size] [st(2)] [obj index(4)]
//
//		SyncGameState = 10,
//		// [int size] [st] [int DynamicGameObjectCapacity] [int StaticGameObjectCapacity]
//	};
//
//	// enum�� ���ڷ� ��Ÿ�� ��.
//	short n;
//	char two_byte[2];
//
//	STCProtocol(short id) { n = id; }
//	operator short() { return n; }
//};
//
//
///*
//* ���� : ��ü ������Ʈ �ϳ��� ��ü�� ����ȭ �ϰ� ������ ���ȴ�.
//* �� ���ӿ�����Ʈ���� �ɹ������� �ش� �����͸� ����� SendDataSaver�� ���� �� �ִ�.
//*/
//struct STC_SyncGameObject_Header {
//	unsigned int size = 0;
//	STCProtocol st = STCProtocol::SyncGameObject;
//	GameObjectType type;
//	int objindex;
//};
//
///*
//* ���� : ���ӿ�����Ʈ���� � ������Ʈ�� � �ɹ��� �����ϰ� ������ ���ȴ�.
//*/
//struct STC_ChangeMemberOfGameObject_Header {
//	unsigned int size = 0;
//	STCProtocol st = STCProtocol::ChangeMemberOfGameObject;
//	GameObjectType type;
//	int objindex;
//	// ������ �������� ����Ѵ�. (Ŭ�� �˾Ƽ� �ؼ��Ѵ�.)
//	int serveroffset;
//	int datasize;
//	// �� ���ķ� ���� ����ȭ�� �����Ͱ� �ٴ´�.
//};
//
///*
//* ���� : �׳� Ŭ���̾�Ʈ���� � �Ѿ� ������ �׸��� �����ϴ� ��.
//	��� �浹�� �������� ��������.
//*/
//struct STC_NewRay_Header {
//	unsigned int size = 34; // ũ�����
//	STCProtocol st = STCProtocol::NewRay;
//	XMFLOAT3 raystart;
//	XMFLOAT3 rayDir;
//	float distance;
//};
//
///*
//* ���� : Ŭ���̾�Ʈ���� �������� �ڽŰ� �ڽ��� ������Ʈ�� ��� �����ǰ� �ִ��� �˷��ش�.
//*/
//struct STC_AllocPlayerIndexes_Header {
//	unsigned int size = 14; // ũ�����
//	STCProtocol st = STCProtocol::AllocPlayerIndexes;
//
//	// �����͸� ���� Ŭ���̾�Ʈ�� ���������� ���° Ŭ���̾�Ʈ����
//	int clientindex;
//	// �� Ŭ���̾�Ʈ�� �����ϴ� ������Ʈ�� �������� ���° Dynamic������Ʈ����.
//	int server_obj_index;
//};
//
///*
//* ���� : Ư�� ������Ʈ�� �����Ǿ��ٴ� ����� Ŭ���̾�Ʈ���� �����Ѵ�.
//*/
//struct STC_DeleteGameObject_Header {
//	unsigned int size = 10; // ũ�����
//	STCProtocol st = STCProtocol::DeleteGameObject;
//	int obj_index; // ������ ������ dynamic ������Ʈ�� �ε���
//};
//
///*
//* ���� : �������� ��ӵǾ��ٴ� �� Ŭ���̾�Ʈ���� �˸��� ����.
//*/
//struct STC_ItemDrop_Header {
//	unsigned int size = 48; // ũ�����
//	STCProtocol st = STCProtocol::ItemDrop;
//	int dropindex; // ��Ӿ����� �ε���
//	ItemLoot lootData; // ���õ� �������� ������
//};
//
///*
//* ���� : ��� �������� �����Ǿ��ٴ°� Ŭ���̾�Ʈ���� �˸��� ����
//*/
//struct STC_ItemDropRemove_Header {
//	unsigned int size = 10; // ũ�����
//	STCProtocol st = STCProtocol::ItemDropRemove;
//	int dropindex; // ������ ��Ӿ������� �ε���
//};
//
///*
//* ���� : �κ��丮�� Ư�� ĭ�� ����ȭ �ϴ� ����
//*/
//struct STC_InventoryItemSync_Header {
//	unsigned int size = 18; // ũ�����
//	STCProtocol st = STCProtocol::InventoryItemSync;
//	// �κ��丮�� �� ������
//	ItemStack Iteminfo;
//	// �κ��丮 ���° ĭ����.
//	int inventoryIndex;
//};
//
///*
//* ���� : ???
//*/
//struct STC_PlayerFire_Header {
//	unsigned int size = 10; // ũ�����
//	STCProtocol st = STCProtocol::PlayerFire;
//	int objindex;
//};
//
///*
//* ���� : �������� ���Ӱ� ���õ� ���µ��� �����Ѵ�.
//*/
//struct STC_SyncGameState_Header {
//	unsigned int size = 10; // ũ�����
//	STCProtocol st = STCProtocol::SyncGameState;
//	int DynamicGameObjectCapacity;
//	int StaticGameObjectCapacity;
//};
//
//union CTS_Protocol {
//	enum {
//		KeyInput = 0,
//		SyncRotation = 1
//	};
//	short n;
//	char two_byte[2];
//
//	CTS_Protocol(short id) { n = id; }
//	operator short() { return n; }
//};
//
//struct CTS_KeyInput_Header {
//	unsigned int size = 8; // ũ�����
//	CTS_Protocol st = CTS_Protocol::KeyInput;
//	char Key;
//	bool isdown;
//};
//
//struct CTS_SyncRotation_Header {
//	unsigned int size = 14;
//	CTS_Protocol st = CTS_Protocol::KeyInput;
//	float yaw;
//	float pitch;
//};

#include "../../SyncFPSServer/SyncFPSServer/Protocol.h"
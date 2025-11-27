#pragma once
#include "stdafx.h"

union ServerInfoType {
	enum {
		NullType = 0,
		NewGameObject = 1, // [st] [new obj index] [type of gameobject] [gameobject raw data]
		ChangeMemberOfGameObject = 2,
		// [st(2)] [obj index(int) (4)] [type of gameobject(2)] [client member offset(short)] [memberSize (2)] [rawData (member size)]
		NewRay = 3, // [st] [Ray raw data (include distance which determined by raycast)]
		SetMeshInGameObject = 4, // [st] [obj index] [strlen] [string data]
		AllocPlayerIndexes = 5, // [st] [client Index] [Object Index]
		DeleteGameObject = 6, // [st] [obj index] // yet error..
		PACK = 7, PACK_END = 8, // [st] [pack order index] [~ data ~] (siz = 8196)
		ItemDrop = 9, // [st] [dropindex] [lootdata]
		ItemDropRemove = 10, //[st] [dropindex]
		InventoryItemSync = 11, //[st] [item stack data] [inventory index]
	};
	short n;
	char two_byte[2];

	ServerInfoType(short id) { n = id; }
	operator short() { return n; }
};

struct twoPage {
	char data[8196] = {};
};

struct DataPackFactory {
	struct DataPack {
		twoPage data;
		int up = 0;

		void Clear(int data_order_id) {
			up = 0;
			ZeroMemory(&data, sizeof(twoPage));
			*(short*)&data.data[up] = ServerInfoType::PACK;
			up += sizeof(short);
			*(int*)&data.data[up] = data_order_id;
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

	vector<DataPack> packs;
	int pack_up = 0;
	bool meetend = false;

	void push_pack() {
		packs.push_back(DataPack());
		packs[pack_up].Clear(pack_up);
		pack_up += 1;
	}

public:
	operator vector<DataPack>() { return packs; }

	void Clear() {
		packs.clear();
		push_pack();
	}

	void push_data(int datacap, void* dataptr) {
		bool b = packs[pack_up - 1].push_data(datacap, dataptr);
		if (b == false) {
			push_pack();
			packs[pack_up - 1].push_data(datacap, dataptr);
		}
	}

	//return true if ready to read fully.
	bool Recieve(char* ptr, int len) {
		int offset = 0;
		ServerInfoType type = *(ServerInfoType*)ptr;
		if (type == ServerInfoType::PACK_END) {
			meetend = true;
		}
		offset += 2;
		int order_id = *(int*)&ptr[offset];
		if (order_id == pack_up) {
			packs.push_back(DataPack());
			memcpy_s(packs[order_id].data.data, len, ptr, len);
			packs[order_id].up = len;
		}
		else if (order_id < pack_up) {
			memcpy_s(packs[order_id].data.data, len, ptr, len);
			packs[order_id].up = len;
		}
		else if (pack_up < order_id) {
			packs.resize(order_id + 1);
			memcpy_s(packs[order_id].data.data, len, ptr, len);
			packs[order_id].up = len;
			for (int i = pack_up; i < order_id; ++i) {
				packs[i].up = 0;
			}
			pack_up = order_id + 1;
		}

		if (meetend == true) {
			bool b = true;
			for (int i = 0; i < pack_up; ++i) {
				b &= (packs[i].up != 0);
			}
			if (b) {
				return true;
			}
		}
		return false;
	}

	void send(Socket* sock) {
		for (int i = 0; i < pack_up; ++i) {
			sock->Send((char*)&packs[i].data, sizeof(twoPage));
		}
		Clear();
	}
};
#pragma once
#include "stdafx.h"
#include "GameObject.h"
#include "Player.h"
#include "Monster.h"

struct type_offset_pair {
	short type;
	short offset;

	type_offset_pair(short t, short o) :
		type{ t }, offset{ o }
	{

	}

	type_offset_pair(const type_offset_pair& ref) {
		type = ref.type;
		offset = ref.offset;
	}

	bool operator==(const type_offset_pair& ref) const {
		return type == ref.type && offset == ref.offset;
	}
};

template<>
class hash<type_offset_pair> {
public:
	size_t operator()(const type_offset_pair& s) const {
		ui32 offsethash = s.offset;
		offsethash = (offsethash & 177600) | ((offsethash & 7) << 3) | ((offsethash & 70) >> 3);
		ui32 typehash = s.type;
		offsethash = _pdep_u32(offsethash, 0xAAAAAAAA);
		typehash = _pdep_u32(typehash, 0x55555555);
		return offsethash | typehash;
	}
};

template <typename T> void* GetVptr() {
	T a;
	return *(void**)&a;
}

// virtual function pointer table <-> GameObjectType
// pair <GameObjectType, offset> <-> Client Offset
union GameObjectType {
	static constexpr int ObjectTypeCount = 3;

	short id;
	enum {
		_GameObject = 0,
		_Player = 1,
		_Monster = 2,
	};

	GameObjectType() {}
	GameObjectType(short s) { id = s; }

	operator short() { return id; }
	static constexpr int ClientSizeof[ObjectTypeCount] = {
#ifdef _DEBUG
		160,
		416,
		160,
#else
		144,
		400,
		144,
#endif
	};

	static constexpr int ServerSizeof[ObjectTypeCount] =
	{
		sizeof(GameObject),
		sizeof(Player),
		sizeof(Monster),
	};

	static unordered_map<type_offset_pair, short, hash<type_offset_pair>> GetClientOffset;
	static unordered_map<void*, GameObjectType> VptrToTypeTable;
	static void STATICINIT();

	static void AddClientOffset_ptr(GameObjectType gotype, char* obj, char* member, int clientOffset);
};

#define AddClientOffset(a, b, c) GameObjectType::GetClientOffset.insert(std::pair<type_offset_pair, short>(type_offset_pair( \
	a, b), c));
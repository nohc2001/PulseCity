#pragma once
#include "stdafx.h"
#include "GameObject.h"
#include "Player.h"
#include "Monster.h"

/*
* 설명 : 어떤 서버의 클래스에서 해당 클래스의 어떤 맴버변수가 어떤 오프셋을 가지는지에 대한 정보
*/
struct type_offset_pair {
	//클래스의 타입
	short type;
	//서버 GameObject내 어떤 맴버변수의 offset
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

/*
* 설명 : type_offset_pair의 hash.
* offset의 제일 하위 3바이트와 그다음 하위 3바이트를 뒤바꾼후, 
* type과 교차한다.
*/
template<>
class hash<type_offset_pair> {
public:
	size_t operator()(const type_offset_pair& s) const {
		ui32 offsethash = s.offset;
		offsethash = (offsethash & 177700) | ((offsethash & 7) << 3) | ((offsethash & 38) >> 3);
		
		//temp 2026.1.3 <아마 의도와는 다른 코드가 아닐까 생각중.>
		//offsethash = (offsethash & 177600) | ((offsethash & 7) << 3) | ((offsethash & 70) >> 3);
		
		ui32 typehash = s.type;
		offsethash = _pdep_u32(offsethash, 0xAAAAAAAA);
		typehash = _pdep_u32(typehash, 0x55555555);
		return offsethash | typehash;
	}
};

/*
* 설명 : 어떤 타입 T의 가상함수 테이블을 가리키는 vptr을 얻는다.
*/
template <typename T> void* GetVptr() {
	T a;
	return *(void**)&a;
}

/*
* 설명 : 게임오브젝트들의 타입을 가리키는 구조체.
* 
*/
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

	// pair <GameObjectType, offset> -> Client Offset
	static unordered_map<type_offset_pair, short, hash<type_offset_pair>> GetClientOffset;

	// virtual function pointer table -> GameObjectType
	static unordered_map<void*, GameObjectType> VptrToTypeTable;

	/*
	* 설명 : 전체 클래스의 초기화.클래스 사용전 반드시 호출해야함.
	*/
	static void STATICINIT();
};

/*
* 설명 : GetClientOffset Map에 ClientOffset 정보를 추가한다.
* 매개변수 : 
* a : 게임오브젝트 타입
* b : 서버 오프셋
* c : 클라이언트 오프셋
*/
#define AddClientOffset(a, b, c) GameObjectType::GetClientOffset.insert(std::pair<type_offset_pair, short>(type_offset_pair( \
	a, b), c));
#include "stdafx.h"
#include "GameObjectTypes.h"

unordered_map<type_offset_pair, short, hash<type_offset_pair>> GameObjectType::GetClientOffset;
unordered_map<void*, GameObjectType> GameObjectType::VptrToTypeTable;

void GameObjectType::STATICINIT()
{
	GameObjectType::VptrToTypeTable.insert(pair<void*, GameObjectType>(GetVptr<GameObject>(), GameObjectType::_GameObject));
	GameObjectType::VptrToTypeTable.insert(pair<void*, GameObjectType>(GetVptr<Player>(), GameObjectType::_Player));
	GameObjectType::VptrToTypeTable.insert(pair<void*, GameObjectType>(GetVptr<Monster>(), GameObjectType::_Monster));
}

void GameObjectType::AddClientOffset_ptr(GameObjectType gotype, char* obj, char* member, int clientOffset)
{
	AddClientOffset(gotype, (short)(member - obj), (short)clientOffset);
}
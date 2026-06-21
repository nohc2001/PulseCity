#define NOMINMAX
#include "stdafx.h"
#include "GameObject.h"
#include "main.h"
#include <set>

using namespace std;

World gameworld;
float DeltaTime = 0;
Server server;
vector<Item> ItemTable;
int dbgc[128] = {};

void dbgbreak(bool condition) {
	if (condition) __debugbreak();
}

int main() {
	// 개발시 존 아이디 지정.
	constexpr int testZoneID = 74; // Zone_4_7
	int serverId = testZoneID;
	unsigned short listenPort = 9000 + testZoneID;
	int ownedZoneId = testZoneID;

	if (__argc >= 2) serverId = atoi(__argv[1]);
	if (__argc >= 3) listenPort = (unsigned short)atoi(__argv[2]);
	if (__argc >= 4) ownedZoneId = atoi(__argv[3]);

	// 오류등이 한글로 표시되도록 한다.
	wcout.imbue(locale("korean"));
	//WSA 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	if (serverId == 0) {
		gameworld.PrintOffset();
		cout << "PrintOffset" << endl;
	}

	//gameworld.CommonSDS.Init(4096);

	gameworld.serverId = serverId;
	gameworld.listenPort = listenPort;
	gameworld.ownedZoneId = ownedZoneId;
	if (__argc >= 4) {
		gameworld.singleProcessAllZones = false;
	}

	server.Init("127.0.0.1", listenPort);

	cout << "Server Start"
		<< " | serverId=" << serverId
		<< " | port=" << listenPort
		<< " | ownedZoneId=" << ownedZoneId
		<< endl;

	vector<WSAPOLLFD> readFds;
	//??
	server.Listen();
	double DeltaFlow = 0;
	constexpr double InvHZ = 1.0 / (double)QUERYPERFORMANCE_HZ;

	gameworld.Init();
	cout << "Server Init Complete" << " | serverId=" << serverId << " | ownedZoneId=" << ownedZoneId << endl;

	// [4단계-STEP1] 기동 직후 이웃 서버 상시 링크 1차 시도(아직 안 떠 있으면 루프에서 재시도).
	gameworld.TryConnectPeers();

	ui64 ft, st;
	ui64 TimeStack = 0;
	st = GetTicks();

	while (true) {
		ft = GetTicks();
		DeltaFlow += (double)(ft - st) * InvHZ;
		if (DeltaFlow >= 0.008) { // [지연 개선] 틱 주기 16ms -> 8ms (이동은 DeltaTime 기반이라 속도 불변, 플러시만 빨라짐)
			DeltaTime = (float)DeltaFlow;
			gameworld.Update();
			DeltaFlow = 0;

			// [4단계-STEP2] 매 틱 내 존 플레이어 상태를 이웃 서버로 복제.
			gameworld.SendGhostToPeers();

			// [4단계-STEP1] 약 1초마다 아직 연결 안 된 이웃에 재접속 시도.
			static int peerRetryCounter = 0;
			if (++peerRetryCounter >= 60) {
				peerRetryCounter = 0;
				gameworld.TryConnectPeers();
			}
		}

		// fix : vector 의 공간 할당이 매 프레임마다 있다. 개느릴듯.
		readFds.reserve(gameworld.clients.size + 1);
		readFds.clear();

		WSAPOLLFD item2;
		item2.events = POLLRDNORM;
		item2.fd = server.listen_sock;
		item2.revents = 0;
		readFds.push_back(item2); // 0번째에 리슨소켓
		for (int i = 0; i < gameworld.clients.size; ++i)
		{
			WSAPOLLFD item;
			item.events = POLLRDNORM;
			item.fd = gameworld.clients[i].socket;
			item.revents = 0;
			readFds.push_back(item);
		}

		// I/O 가능 이벤트가 있을 때까지 기다립니다.
		WSAPoll(readFds.data(), (int)readFds.size(), 4);

		int num = 0;
		constexpr int listenSockIndex = 0;
		for (auto readFd : readFds)
		{
			if (readFd.revents != 0)
			{
				if (num == listenSockIndex) {
					// 4
					//SOCKET tcpConnection = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
					string errorMessage;

					int newindex = gameworld.clients.Alloc();
					if (newindex >= 0) {
						server.Accept(gameworld.clients[newindex].socket, errorMessage);
						gameworld.clients[newindex].GetClientAddr();
						auto a = gameworld.clients[newindex].addr.ToString();
						gameworld.clients[newindex].SetNonBlocking();
						gameworld.clients[newindex].PersonalSDS.Init(4096);
						gameworld.clients[newindex].pObjData = nullptr;
						gameworld.clients[newindex].objindex = -1;
						gameworld.clients[newindex].zoneId = gameworld.ownedZoneId;
						gameworld.clients[newindex].pendingTransferToken = 0;

						cout << "Socket from " << a << " is accepted.\n";
						char newParticipantstr[128] = {};
						sprintf_s(newParticipantstr, "New client - %d\n", newindex);
						cout << newParticipantstr << endl;
					}
					else {
						cout << "too many client!!" << endl;
					}
				}
				else {
					int index = num - 1;
					if (gameworld.clients.isnull(index)) continue;
					ClientData& client = gameworld.clients[index];
					while (1) {
						int result = client.recv(client.rbuf + client.rbufoffset, client.rbufcap - client.rbufoffset, 0);
						if (result > 0) {
							char* cptr = client.rbuf;
							result += client.rbufoffset;
							int offset = gameworld.Receiving(index, cptr, result);
							memmove(client.rbuf, client.rbuf + offset, result - offset);
							client.rbufoffset = result - offset;
						}
						if (result == -1) {
							if (WSAGetLastError() == WSAEWOULDBLOCK) {
								// 아직 읽을 수 없다면 다음 루프로 넘긴다.
								break;
							}
							else {
								// 네트워크 오류가 발생되었거나 클라이언트가 죽은 상황
								// TODO : 서버와의 연결 끊을때의 처리
								ClientData::DisconnectToServer(index);
								break;
							}
						}
						else if (result == 0) {
							// 서버가 종료됨.
							// TODO : 서버와의 연결 끊을때의 처리
							ClientData::DisconnectToServer(index);
							break;
						}
						else break; // ??
					}
				}
			}
			num += 1;
		}

		st = ft;
	}

	WSACleanup();
	return 0;
}

int World::Receiving(int clientIndex, char* rBuffer, int totallen) {
	Zone* zone = GetClientZone(clientIndex);
	ClientData& client = clients[clientIndex];
	Player* p = (Player*)client.pObjData;

	char* currentPivot = rBuffer;
	int offset = 0;
	int size;
	CTS_Protocol type = CTS_Protocol::KeyInput;
READ_START:
	if (offset + sizeof(int) > totallen) return offset; // 이거 살리는게 좋지 않나? 흠. // fix
	size = *(int*)currentPivot;
	if (offset + size > totallen) return offset;
	type = *(CTS_Protocol*)(currentPivot + sizeof(int));

	if (p == nullptr
		&& type.n != CTS_Protocol::ClientHello
		&& type.n != CTS_Protocol::TransferConnect
		&& type.n != CTS_Protocol::ServerPlayerTransfer
		&& type.n != CTS_Protocol::ServerLink
		&& type.n != CTS_Protocol::GhostSync
		&& type.n != CTS_Protocol::GhostDamage
		&& type.n != CTS_Protocol::MonsterHandoff) {   // [4단계] peer 링크 메시지는 pObjData 없이도 통과
		currentPivot += size;
		offset += size;
		goto READ_START;
	}

	switch (type) {
	case CTS_Protocol::KeyInput:
	{
		CTS_KeyInput_Header& header = *(CTS_KeyInput_Header*)currentPivot;
		p->InputBuffer[header.Key] = header.isdown;
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case CTS_Protocol::SyncRotation:
	{
		CTS_SyncRotation_Header& header = *(CTS_SyncRotation_Header*)currentPivot;
		p->m_yaw = header.yaw;
		p->m_pitch = header.pitch;
		p->bFirstPersonVision = header.bFirstPersonVision;
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case CTS_Protocol::ClientHello: {
		//cout << "[Receiving] ClientHello" << endl;
		CTS_ClientHello_Header& header = *(CTS_ClientHello_Header*)currentPivot;
		AcceptClientHello(clientIndex);
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case CTS_Protocol::TransferConnect:
	{
		CTS_TransferConnect_Header& header = *(CTS_TransferConnect_Header*)currentPivot;
		AcceptTransferConnect(clientIndex, header.transferToken);
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case CTS_Protocol::ServerPlayerTransfer:
	{
		CTS_ServerPlayerTransfer_Header& header = *(CTS_ServerPlayerTransfer_Header*)currentPivot;
		StoreIncomingPlayerTransfer(header.data);
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case CTS_Protocol::ServerLink:
	{
		// [4단계-STEP1] 이웃 서버가 connect 후 보낸 핸드셰이크 -> 이 소켓을 peer 링크로 확정.
		CTS_ServerLink_Header& header = *(CTS_ServerLink_Header*)currentPivot;
		OnPeerLinkEstablished(clientIndex, header.fromServerId);
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case CTS_Protocol::GhostSync:
	{
		// [4단계-STEP2] 이웃 서버가 보낸 플레이어 목록 -> 고스트 갱신 + 내 클라에 중계.
		CTS_GhostSync_Header& header = *(CTS_GhostSync_Header*)currentPivot;
		OnGhostSync(currentPivot);
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case CTS_Protocol::GhostDamage:
	{
		// [4단계-STEP4] 이웃이 내 객체(고스트)를 맞혔다 -> 진짜 객체에 데미지 적용.
		CTS_GhostDamage_Header& header = *(CTS_GhostDamage_Header*)currentPivot;
		int tz = header.targetZoneId;
		if (tz >= 0 && tz < ZoneTable.size() && IsZoneOwned(tz)) {
			Zone* z = ZoneTable[tz];
			int ti = header.targetObjIndex;
			if (ti >= 0 && ti < z->Dynamic_gameObjects.size && z->Dynamic_gameObjects.isnull(ti) == false) {
				DynamicGameObject* obj = z->Dynamic_gameObjects[ti];
				if (obj != nullptr) {
					short ot = (short)GameObjectType::VptrToTypeTable[*(void**)obj];
					if (ot == GameObjectType::_Monster) {
						z->currentIndex = ti;
						((Monster*)obj)->ApplyDamage(nullptr, header.damage);
					}
				}
			}
		}
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case CTS_Protocol::MonsterHandoff:
	{
		// [4단계-STEP5] 이웃이 넘긴 몬스터를 내 존에 진짜 몬스터로 생성(소유권 인수).
		CTS_MonsterHandoff_Header& header = *(CTS_MonsterHandoff_Header*)currentPivot;
		SpawnHandoffMonster(header.monsterType, header.pos, header.HP, header.MaxHP);
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case CTS_Protocol::UseSkill:
	{
		CTS_UseSkill_Header& header = *(CTS_UseSkill_Header*)currentPivot;
		p->TryUseSkill(header.slot);
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case CTS_Protocol::DungeonStart:
	{
		// [party/dungeon] F-key = START the dungeon, but ONLY if already in the queue (joined via the portal).
		// Pressing F when not queued does nothing -> entry is gated behind walking into the portal.
		CTS_DungeonStart_Header& header = *(CTS_DungeonStart_Header*)currentPivot;
		if (gameworld.DungeonQueueContains(clientIndex)) {
			gameworld.DungeonTryStart(true);
		}
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case CTS_Protocol::ChangeJob:
	{
		CTS_ChangeJob_Header& header = *(CTS_ChangeJob_Header*)currentPivot;
		p->ApplyJob(header.job);
		p->SyncJobState(gameworld.GetClientZone(clientIndex));
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case CTS_Protocol::ChangeInventoryItemSlot:
	{
		CTS_ChangeInventoryItemSlot_Header& header = *(CTS_ChangeInventoryItemSlot_Header*)currentPivot;
		switch (header.ciitType) {
		case _ChangeInventoryItemSlot_Type::CIIT_ItemCountCombine:
		{
			ItemStack& dest_slot = p->Inventory[header.destIndex];
			ItemStack& src_slot = p->Inventory[header.srcIndex];
			if (dest_slot.id == src_slot.id && src_slot.ItemCount >= header.srcCount) {
				dest_slot.ItemCount += header.srcCount;
				src_slot.ItemCount -= header.srcCount;

				//dest, src 를 STC로 Sync Protocol을 보내기
				p->zone->Sending_InventoryItemSync(gameworld.clients[p->clientIndex].PersonalSDS, dest_slot, header.destIndex);
				p->zone->Sending_InventoryItemSync(gameworld.clients[p->clientIndex].PersonalSDS, src_slot, header.srcIndex);
			}
		}
		break;
		case _ChangeInventoryItemSlot_Type::CIIT_ItemMoveToBlankSlot:
		{
			ItemStack& dest_slot = p->Inventory[header.destIndex];
			ItemStack& src_slot = p->Inventory[header.srcIndex];
			if (dest_slot.ItemCount == 0 && src_slot.ItemCount >= header.srcCount) {
				dest_slot.id = src_slot.id;
				dest_slot.ItemCount += header.srcCount;
				src_slot.ItemCount -= header.srcCount;
				//dest, src 를 STC로 Sync Protocol을 보내기
				p->zone->Sending_InventoryItemSync(gameworld.clients[p->clientIndex].PersonalSDS, dest_slot, header.destIndex);
				p->zone->Sending_InventoryItemSync(gameworld.clients[p->clientIndex].PersonalSDS, src_slot, header.srcIndex);
			}
		}
		break;
		case _ChangeInventoryItemSlot_Type::CIIT_Swap:
		{
			ItemStack& dest_slot = p->Inventory[header.destIndex];
			ItemStack& src_slot = p->Inventory[header.srcIndex];
			if (dest_slot.id != src_slot.id && src_slot.ItemCount == header.srcCount) {
				swap(dest_slot.id, src_slot.id);
				swap(dest_slot.ItemCount, src_slot.ItemCount);
				//dest, src 를 STC로 Sync Protocol을 보내기
				p->zone->Sending_InventoryItemSync(gameworld.clients[p->clientIndex].PersonalSDS, dest_slot, header.destIndex);
				p->zone->Sending_InventoryItemSync(gameworld.clients[p->clientIndex].PersonalSDS, src_slot, header.srcIndex);
			}
		}
		break;
		}
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case CTS_Protocol::CTS_ChangeEquipSlotWithInventorySlot:
	{
		CTS_ChangeEquipSlotWithInventorySlot_Header& header = *(CTS_ChangeEquipSlotWithInventorySlot_Header*)currentPivot;
		header.size = sizeof(CTS_ChangeEquipSlotWithInventorySlot_Header);
		header.st = CTS_Protocol::CTS_ChangeEquipSlotWithInventorySlot;
		if (header.EquipIndex >= 4) {
			int weaponeindex = header.EquipIndex - 4;
			if (weaponeindex >= 3) weaponeindex = 2;
			if (weaponeindex < 0) weaponeindex = 0;
			dbgbreak(header.InventoryIndex >= 49);
			Item& item = ItemTable[p->Inventory[header.InventoryIndex].id];
			p->weapon[weaponeindex] = *(Weapon*)item.ItemData;
		}
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case CTS_Protocol::Client_NPCTalkSelection: 
	{
		CTS_Client_NPCTalkSelection_Header& header = *(CTS_Client_NPCTalkSelection_Header*)currentPivot;
		header.size = sizeof(CTS_Client_NPCTalkSelection_Header);
		header.st = CTS_Protocol::Client_NPCTalkSelection;
		
		if (p->PresentTalkID >= 0) {
			// 플레이어가 선택지를 선택했을때.
			TalkSelection& ts = gameworld.NPCTalkTable[p->PresentTalkID].sel[header.selectionID];
			if (ts.mod == 't') {
				// 다음 대화로 넘어가는 경우
				p->PresentTalkID = ts.goto_otherTalkID;
				zone->Sending_NPCStartTalk(gameworld.clients[p->clientIndex].PersonalSDS, PeacefulNPCType::PNT_Quest, ts.goto_otherTalkID);
			}
			else if(ts.mod == 'f') {
				// 선택지의 결과 동작을 실행하는 경우
				ts.Function(p);
				p->PresentTalkID = -1;
			}
			else if (ts.mod == 'q') {
				p->QuestArr.push_back(ts.AddQuest);
				zone->Sending_AddQuest(gameworld.clients[p->clientIndex].PersonalSDS, ts.AddQuest);
				p->PrograssQuestBitArr[ts.AddQuest] = true;
				p->QuestPrograss.push_back(gameworld.QuestTable[ts.AddQuest]);
				p->PresentTalkID = -1;
			}
		}

		currentPivot += header.size;
		offset += header.size;
	}
	break;
	}

	goto READ_START;
}

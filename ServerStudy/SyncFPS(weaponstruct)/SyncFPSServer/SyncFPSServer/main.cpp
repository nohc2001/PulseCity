#define NOMINMAX
#include "stdafx.h"
#include "GameObject.h"
#include "main.h"
#include <set>

World gameworld;
float DeltaTime = 0;
Server server;
vector<Item> ItemTable;

void dbgbreak(bool condition) {
	if (condition) __debugbreak();
}

int main() {
	// 오류등이 한글로 표시되도록 한다.
	wcout.imbue(locale("korean"));
	//WSA 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	gameworld.PrintOffset();
	cout << "PrintOffset" << endl;

	//gameworld.CommonSDS.Init(4096);

	server.Init("127.0.0.1", 9000);

	cout << "Server Start" << endl;

	ui64 ft, st;
	ui64 TimeStack = 0;
	st = GetTicks();
	vector<WSAPOLLFD> readFds;
	//??
	server.Listen();
	double DeltaFlow = 0;
	constexpr double InvHZ = 1.0 / (double)QUERYPERFORMANCE_HZ;

	gameworld.Init();

	while (true) {
		ft = GetTicks();
		DeltaFlow += (double)(ft - st) * InvHZ;
		if (DeltaFlow >= 0.016) { // limiting fps.
			DeltaTime = (float)DeltaFlow;
			gameworld.Update();
			
			//indexRange ir[2048] = {};
			//int irlen = 0;
			//gameworld.clients.GetTourPairs(ir, &irlen);
			//WSABUF sendbuf[2];
			////sendbuf[1].buf = gameworld.CommonSDS.buffer;
			////sendbuf[1].len = gameworld.CommonSDS.size;
			//for (int i = 0;i < irlen;++i) {
			//	for (int k = ir[i].start;k <= ir[i].end;++k) {
			//		sendbuf[0].buf = gameworld.clients[k].PersonalSDS.buffer;
			//		sendbuf[0].len = gameworld.clients[k].PersonalSDS.size;
			//		DWORD retval = 0;

			//		bool sendSuccess = true;
			//		STARTSEND:
			//		int err = WSASend(gameworld.clients[k].socket, sendbuf, 1, &retval, 0, NULL, NULL);
			//		if (err == SOCKET_ERROR) {
			//			int error = WSAGetLastError();
			//			ClientData::DisconnectToServer(k);
			//			continue;
			//		}
			//		if (retval != sendbuf[0].len + sendbuf[1].len) {
			//			int sav = sendbuf[0].len;
			//			sendbuf[0].len -= retval;
			//			sendbuf[0].buf += retval;
			//			if (sendbuf[0].len < 0) sendbuf[0].len = 0;
			//			retval -= sav;
			//			sendbuf[1].len -= retval;
			//			sendbuf[1].buf += retval;
			//			if (sendbuf[1].len < 0) sendbuf[0].len = 0;
			//			sendSuccess = false;
			//		}
			//		if (sendSuccess == false) goto STARTSEND;

			//		//sendbuf[1].buf = gameworld.CommonSDS.buffer;
			//		//sendbuf[1].len = gameworld.CommonSDS.size;
			//		gameworld.clients[k].PersonalSDS.Clear();
			//	}
			//}
			////gameworld.CommonSDS.Clear();

			DeltaFlow = 0;
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
		WSAPoll(readFds.data(), (int)readFds.size(), 50);

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
						auto a = gameworld.clients[newindex].addr.ToString();
						gameworld.clients[newindex].SetNonBlocking();

						// player index sending
						int indexes[2] = {};
						int clientindex = newindex;
						gameworld.clients[newindex].PersonalSDS.Init(4096);
						
						gameworld.Sending_SyncGameState(gameworld.clients[newindex].PersonalSDS);
						//gameworld.SendingAllObjectForNewClient(gameworld.clients[newindex].PersonalSDS);

						int StartzoneId = 0;

						Player* p = new Player();
						p->clientIndex = clientindex;
						p->worldMat.Id();
						p->worldMat.pos.f3.y = 100;
						p->SetShape(Shape::StrToShapeIndex["Player"]);
						//p->mesh = Mesh::StrToShapeMap["Player"];
						for (int i = 0; i < 36; ++i) {
							p->Inventory[i].id = 0;
							p->Inventory[i].ItemCount = 0;
						}

						gameworld.clients[clientindex].pObjData = p;
						gameworld.clients[clientindex].zoneId = StartzoneId;

						vec4 spawnPos(0.0f, 10.0f, 0.0f, 1.0f);
						int objindex = gameworld.zones[StartzoneId].AddPlayer(clientindex, p, spawnPos);
						gameworld.clients[clientindex].objindex = objindex;

						//gameworld.Sending_NewGameObject(gameworld.clients[newindex].PersonalSDS, objindex, p);
						//gameworld.zones[StartzoneId].PushGameObject(p);

						//int datacap = gameworld.Sending_AllocPlayerIndex(newindex, objindex);

						//gameworld.Sending_AllocPlayerIndex(gameworld.clients[newindex].PersonalSDS, clientindex, objindex);
						//gameworld.clients[clientindex].socket.Send((char*)gameworld.tempbuffer.data, dcap);
						//gameworld.clients[clientindex].objindex = objindex;

						//print log
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

//void World::Sending_AllocPlayerIndex(SendDataSaver& sds, int clientindex, int objindex)
//{
//	sds.postpush_start();
//	constexpr int reqsiz = sizeof(STC_AllocPlayerIndexes_Header);
//	sds.postpush_reserve(reqsiz);
//	STC_AllocPlayerIndexes_Header& header = *(STC_AllocPlayerIndexes_Header*)sds.ofbuff;
//	header.size = reqsiz;
//	header.st = SendingType::AllocPlayerIndexes;
//	header.clientindex = clientindex;
//	header.server_obj_index = objindex;
//	sds.postpush_end();
//}

void World::Sending_SyncGameState(SendDataSaver& sds) {
	sds.postpush_start();
	constexpr int reqsiz = sizeof(STC_SyncGameState_Header);
	sds.postpush_reserve(reqsiz);
	STC_SyncGameState_Header& header = *(STC_SyncGameState_Header*)sds.ofbuff;
	header.size = reqsiz;
	header.st = SendingType::SyncGameState;
	//나중에 클라이언트 존 받는걸로 바꿔야함
	header.DynamicGameObjectCapacity = zones[0].Dynamic_gameObjects.Capacity;
	header.StaticGameObjectCapacity = zones[0].Static_gameObjects.Capacity;

	sds.postpush_end();
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
	//if (offset + sizeof(int) > totallen) return offset; // 이거 살리는게 좋지 않나? 흠. // fix
	size = *(int*)currentPivot;
	if (offset + size >= totallen) return offset;
	type = *(CTS_Protocol*)(currentPivot + sizeof(int));

	switch (type) {
	case CTS_Protocol::KeyInput:
	{
		CTS_KeyInput_Header& header = *(CTS_KeyInput_Header*)currentPivot;
		p->InputBuffer[header.Key] = header.isdown;
		cout << "client" << clientIndex << " : " << header.Key << " isdown : " << header.isdown << endl;
		currentPivot += header.size;
		offset += header.size;
	}
	break;
	case CTS_Protocol::SyncRotation:
	{
		CTS_SyncRotation_Header& header = *(CTS_SyncRotation_Header*)currentPivot;
		p->m_yaw = header.yaw;
		p->m_pitch = header.pitch;
		currentPivot += header.size;
		offset += header.size;
	}
		break;
	}

	goto READ_START;

	// fix 이 아래 코드는 이전 코드임.
	// 사실 무기 바꾸는 코드는 여기 있는게 아니라 Update에 가야 하는데 여기에 있는 이유는
	// 아마 Player 구조체에 이전 상태가 없어서 그런것 같다. 이 코드가 Update에 위치할 수 있도록 고쳐야 한다.
	// 당장 생각나는 방법은 이중 버퍼링을 하는것?
	if (rBuffer[0] == InputID::RotationSync) {
		RotationPacket* pkt = (RotationPacket*)rBuffer;
		p->m_yaw = pkt->yaw;
		p->m_pitch = pkt->pitch;
		return sizeof(RotationPacket);
	}
	else {
		if (rBuffer[1] == 'D') {
			char key = rBuffer[0];
			if (key == '1' || key == '2' || key == '3' || key == '4' || key == '5') {

				WeaponType targetType;
				if (key == '1') targetType = WeaponType::MachineGun;
				else if (key == '2') targetType = WeaponType::Sniper;
				else if (key == '3') targetType = WeaponType::Shotgun;
				else if (key == '4') targetType = WeaponType::Rifle;
				else targetType = WeaponType::Pistol;

				if (p->m_currentWeaponType != (int)targetType) {
					p->m_currentWeaponType = (int)targetType;

					p->weapon = Weapon(targetType);

					zone->Sending_ChangeGameObjectMember<int>(zone->CommonSDS, client.objindex, p, GameObjectType::_Player, &p->m_currentWeaponType);

					std::cout << "[Weapon Change] Key: " << key
						<< " -> Type: " << (int)targetType
						<< " Damage: " << p->weapon.m_info.damage << std::endl;
				}
				return 2;
			}
		}

		ui64 putv = (rBuffer[1] == 'D') ? 1 : 0;
		BitBoolArr_ui64* bit = &p->InputBuffer[rBuffer[0]];
		*bit = putv;

		return 2;
	}
}
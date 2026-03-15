#define NOMINMAX
#include "stdafx.h"
#include "GameObject.h"
#include "main.h"
#include <set>

World gameworld;
float DeltaTime = 0;
Server server;
vector<Item> ItemTable;

int main() {
	gameworld.PrintOffset();
	gameworld.CommonSDS.Init(4096);

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
					SOCKET tcpConnection = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
					string errorMessage;

					int newindex = gameworld.clients.Alloc();
					if (newindex >= 0) {
						server.Accept(tcpConnection, errorMessage);
						auto a = gameworld.clients[newindex].addr.ToString();
						gameworld.clients[newindex].SetNonBlocking();

						// player index sending
						int indexes[2] = {};
						int clientindex = newindex;
						gameworld.clients[newindex].PersonalSDS.Init(4096);
						
						gameworld.Sending_SyncGameState(gameworld.clients[newindex].PersonalSDS);
						gameworld.SendingAllObjectForNewClient(gameworld.clients[newindex].PersonalSDS);

						Player* p = new Player();
						p->clientIndex = clientindex;
						p->worldMat.Id();
						p->worldMat.pos.f3.y = 10;
						p->shapeindex = Shape::StrToShapeIndex["Player"];
						//p->mesh = Mesh::StrToShapeMap["Player"];
						for (int i = 0; i < 36; ++i) {
							p->Inventory[i].id = 0;
							p->Inventory[i].ItemCount = 0;
						}

						int objindex = gameworld.NewPlayer(gameworld.CommonSDS, p, clientindex);
						gameworld.clients[clientindex].pObjData = p;

						//int datacap = gameworld.Sending_AllocPlayerIndex(newindex, objindex);

						gameworld.Sending_AllocPlayerIndex(gameworld.clients[newindex].PersonalSDS, clientindex, objindex);
						//gameworld.clients[clientindex].socket.Send((char*)gameworld.tempbuffer.data, dcap);
						gameworld.clients[clientindex].objindex = objindex;

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
					int result = gameworld.clients[index].recv(0);
					if (result > 0) {
						char* cptr = gameworld.clients[index].rbuf;

					RECEIVE_LOOP:
						int offset = gameworld.Receiving(index, cptr);
						cptr += offset;
						result -= offset;
						if (result > 1) {
							goto RECEIVE_LOOP;
						}
					}
					else if (result <= 0) {
						cout << "client " << index << " Left the Game." << endl;
						int objindex = gameworld.clients[index].objindex;
						delete gameworld.clients[index].pObjData;
						gameworld.clients[index].pObjData = nullptr;
						gameworld.Dynamic_gameObjects[objindex] = nullptr;
						gameworld.Dynamic_gameObjects.Free(objindex);
						gameworld.clients.Free(index);
					}
				}
			}
			num += 1;
		}

		st = ft;
	}
	return 0;
}

void World::Sending_AllocPlayerIndex(SendDataSaver& sds, int clientindex, int objindex)
{
	sds.postpush_start();
	constexpr int reqsiz = sizeof(STC_AllocPlayerIndexes_Header);
	sds.postpush_reserve(reqsiz);
	STC_AllocPlayerIndexes_Header& header = *(STC_AllocPlayerIndexes_Header*)sds.ofbuff;
	header.size = reqsiz;
	header.st = SendingType::AllocPlayerIndexes;
	header.clientindex = clientindex;
	header.server_obj_index = objindex;
	sds.postpush_end();
}

void World::Sending_SyncGameState(SendDataSaver& sds) {
	sds.postpush_start();
	constexpr int reqsiz = sizeof(STC_SyncGameState_Header);
	sds.postpush_reserve(reqsiz);
	STC_SyncGameState_Header& header = *(STC_SyncGameState_Header*)sds.ofbuff;
	header.size = reqsiz;
	header.st = SendingType::SyncGameState;
	header.DynamicGameObjectCapacity = Dynamic_gameObjects.Capacity;
	header.StaticGameObjectCapacity = Static_gameObjects.Capacity;
	sds.postpush_end();
}

int World::Receiving(int clientIndex, char* rBuffer) {
	ClientData& client = clients[clientIndex];
	Player* p = (Player*)client.pObjData;

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

					Sending_ChangeGameObjectMember<int>(gameworld.CommonSDS, client.objindex, p, GameObjectType::_Player, &p->m_currentWeaponType);

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
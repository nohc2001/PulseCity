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
	int serverId = 0;
	unsigned short listenPort = 9000;
	int ownedZoneId = 0;
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

	ui64 ft, st;
	ui64 TimeStack = 0;
	st = GetTicks();

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
						gameworld.clients[newindex].GetClientAddr();
						auto a = gameworld.clients[newindex].addr.ToString();
						gameworld.clients[newindex].SetNonBlocking();
						gameworld.clients[newindex].PersonalSDS.Init(4096);
						gameworld.clients[newindex].pObjData = nullptr;
						gameworld.clients[newindex].objindex = -1;
						gameworld.clients[newindex].zoneId = gameworld.ownedZoneId;

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
                                cout << "[ServerRecv] socket error client=" << index << " err=" << WSAGetLastError() << endl;
								ClientData::DisconnectToServer(index);
								break;
							}
						}
						else if (result == 0) {
							// 서버가 종료됨.
							// TODO : 서버와의 연결 끊을때의 처리
                            cout << "[ServerRecv] client closed connection client=" << index << endl;
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
	//cout << "[Receiving] client=" << clientIndex << " bytes=" << totallen << endl;
	ClientData& client = clients[clientIndex];
	Player* p = (Player*)client.pObjData;
	char* currentPivot = rBuffer;
	int offset = 0;

	while (true) {
		if (offset + (int)sizeof(int) > totallen) return offset;
		int size = *(int*)currentPivot;
		if (size <= 0) return offset;
		if (offset + size > totallen) return offset;

		CTS_Protocol type = *(CTS_Protocol*)(currentPivot + sizeof(int));
		//cout << "[Receiving] size=" << size << " type=" << (short)type << endl;
		switch (type) {
		case CTS_Protocol::ClientHello:
			//cout << "[Receiving] ClientHello" << endl;
			AcceptClientHello(clientIndex);
			break;
		case CTS_Protocol::TransferConnect:
		{
			CTS_TransferConnect_Header& header = *(CTS_TransferConnect_Header*)currentPivot;
			AcceptTransferConnect(clientIndex, header.transferToken);
		}
		break;
		case CTS_Protocol::ServerPlayerTransfer:
		{
			CTS_ServerPlayerTransfer_Header& header = *(CTS_ServerPlayerTransfer_Header*)currentPivot;
			StoreIncomingPlayerTransfer(header.data);
		}
		break;
		case CTS_Protocol::KeyInput:
		{
			CTS_KeyInput_Header& header = *(CTS_KeyInput_Header*)currentPivot;
			if (p != nullptr) {
				p->InputBuffer[header.Key] = header.isdown;
				cout << "client" << clientIndex << " : " << header.Key << " isdown : " << header.isdown << endl;
			}
		}
		break;
		case CTS_Protocol::SyncRotation:
		{
			CTS_SyncRotation_Header& header = *(CTS_SyncRotation_Header*)currentPivot;
			if (p != nullptr) {
				p->m_yaw = header.yaw;
				p->m_pitch = header.pitch;
			}
		}
		break;
		default:
			return offset;
		}

		offset += size;
		currentPivot += size;
		p = (Player*)client.pObjData;
	}
}

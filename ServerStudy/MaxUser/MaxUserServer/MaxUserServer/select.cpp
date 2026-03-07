#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <mutex>
#include <conio.h>
#include <sstream>
#include <format> // C++20 필수
#include "vecset.h"

#pragma comment(lib, "ws2_32")

using namespace std;

vector<char> Commonly_send_data;

#define QUERYPERFORMANCE_HZ 10000000//Hz
static inline int64_t GetTicks()
{
	LARGE_INTEGER ticks;
	QueryPerformanceCounter(&ticks);
	return ticks.QuadPart;
}

constexpr bool isdebug = true;
int dbgc[128] = {};
__forceinline void dbgbreak(bool condition) {
	if constexpr (isdebug) {
		if (condition) {
			__debugbreak();
		}
	}
}
enum edbg {
	SelectError = 0,
	AcceptError_InvalidSocket = 1,
	IDLE_Recv = 2,
	IDLE_Send = 3,
	ClientForcedDisconnet = 4,
};

// 2의 거듭제곱 - 1 형태면 좋다. (64의 배수 - 1)
constexpr int clientCount = 2047;
struct Client {
	float x = 0;
	float y = 0;

	bool W;
	bool S;
	bool A;
	bool D;

	vector<char> wbuf;

	void Send(char* data, int len) {
		int prevSiz = wbuf.size();
		char* startptr = wbuf.data() + prevSiz;
		if (prevSiz + len > wbuf.capacity()) {
			wbuf.reserve(prevSiz + len);
			wbuf.resize(prevSiz + len);
			startptr = &wbuf[prevSiz];
		}
		else wbuf.resize(prevSiz + len);
		memcpy(startptr, data, len);
	}

	static void CommonSend(char* data, int len) {
		int prevSiz = Commonly_send_data.size();
		char* startptr = Commonly_send_data.data() + prevSiz;
		if (prevSiz + len > Commonly_send_data.capacity()) {
			Commonly_send_data.reserve(prevSiz + len);
			Commonly_send_data.resize(prevSiz + len);
			startptr = &Commonly_send_data[prevSiz];
		}
		else Commonly_send_data.resize(prevSiz + len);
		memcpy(startptr, data, len);
	}

	static constexpr float speed = 1.0f;
	void Update(float deltaTime) {
		if (W) y += speed * deltaTime;
		if (S) y -= speed * deltaTime;
		if (A) x += speed * deltaTime;
		if (D) x -= speed * deltaTime;
	}
};

struct SOCKETINFO {
	SOCKET sock;
	char buf[BUFSIZ + 1];
	int recvbytes;
	int sendbytes;
	Client clientdata;
	int rbuf_offset = 0;
};
int nTotalSockets = 0;
vecset<SOCKETINFO> SocketInfoArray;//[clientCount + 1];

stringstream  serverLog;

void RemoveSocketInfo(int nindex);

bool AddSocketInfo(SOCKET sock) {
	if (nTotalSockets >= clientCount) {
		serverLog << "cannot add client socket cause sock max number in selectmodel" << "\n";
		return false;
	}
	int newindex = SocketInfoArray.Alloc();
	SOCKETINFO* ptr = &SocketInfoArray[newindex];
	if (ptr == NULL) {
		serverLog << "memeory starvation." << endl;
		return false;
	}

	ptr->sock = sock;
	ptr->recvbytes = 0;
	ptr->sendbytes = 0;

	struct sockaddr_in clientaddr;
	int addrlen = sizeof(clientaddr);
	getpeername(ptr->sock, (sockaddr*)&clientaddr, &addrlen);

	char addr[INET_ADDRSTRLEN] = {};
	inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));
	serverLog << "[TCP 서버] 클라이언트 접속 : IP주소 = " << addr << ", 포트번호 = " << ntohs(clientaddr.sin_port) << "\n";
	
	//서버vecset 내의 클라이언트 데이터의 인덱스를 클라이언트도 알 수 있도록 보내준다.
	int sendsize = sizeof(int);
	char sbuf[4];
	char* sptr = sbuf;
	*(int*)sptr = newindex;
	RESEND:
	int retval = send(ptr->sock, sptr, sendsize, 0);
	if (retval == SOCKET_ERROR) {
		if (WSAGetLastError() == WSAEWOULDBLOCK) {
			goto RESEND;
		}
		else {
			RemoveSocketInfo(newindex);
			return false;
		}
	}
	else if (retval != sendsize) {
		sendsize -= retval;
		sptr += retval;
		goto RESEND;
	}
	return true;
}

void RemoveSocketInfo(int nindex) {
	SocketInfoArray.Free(nindex);
	SOCKETINFO* ptr = &SocketInfoArray[nindex];
	ptr->clientdata.wbuf.clear(); ptr->rbuf_offset = 0;
	ptr->recvbytes = 0;
	ptr->sendbytes = 0;
	struct sockaddr_in clientaddr;
	int addrlen = sizeof(clientaddr);
	getpeername(ptr->sock, (sockaddr*)&clientaddr, &addrlen);

	char addr[INET_ADDRSTRLEN] = {};
	inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));
	serverLog << "[TCP 서버] 클라이언트 종료 : IP주소 = " << addr << ", 포트번호 = " << ntohs(clientaddr.sin_port) << "\n";

	closesocket(ptr->sock);

	//if (nindex != nTotalSockets - 1) {
	//	SocketInfoArray[nindex] = SocketInfoArray[nTotalSockets - 1];
	//	--nTotalSockets;
	//}
}

#define VAR_INITSET

constexpr int SelectSetCount = ((clientCount) >> 6) + 1;
fd_set rset[SelectSetCount], wset[SelectSetCount];
ui64 lastTimeRecord = 0;

int main() {
	SocketInfoArray.Init(clientCount + 1);
	Commonly_send_data.reserve(BUFSIZ);

	cout << "Initialize WinSock...";
	WSADATA wsa;

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;
	cout << "WSAStartup() Success. " << "\n";

	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET) {
		cout << "listen socket init error" << "\n";
		return 0;
	}

	u_long on = 1;
	int retval = ioctlsocket(listen_sock, FIONBIO, &on);
	if (retval == SOCKET_ERROR) {
		cout << "listen socket non blocking mode error" << "\n";
		return 0;
	}

	int nready = 0;

	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	constexpr int SERVERPORT = 9000;
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(listen_sock, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		closesocket(listen_sock);
		WSACleanup();
		return 0;
	}

	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR) {
		cout << "listen Error" << "\n";
		return 0;
	}

	SOCKET client_sock;
	struct sockaddr_in clientaddr;
	int addrlen;
	char buf[BUFSIZ + 1];

	int stack = 0;
	serverLog << "Server Start" << endl;
	double AverageSelectTime = 0;
	double StackTime = 0;
	int64_t DeltaTick = 0;

	int errorCount = 0;

	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	constexpr double fixedDelta = 0.05f;
	while (1) {
		int64_t deltaft = GetTicks();
		double deltaTime = (double)(deltaft - DeltaTick) / (double)(QUERYPERFORMANCE_HZ);
		char data[12];
		DeltaTick = deltaft;
		StackTime += deltaTime;

		indexRange irange[(clientCount + 1) / 2];
		int outlen = 0;
		SocketInfoArray.GetTourPairs(irange, &outlen);

		bool isUpdated = false;
		if (StackTime > fixedDelta) {
			isUpdated = true;
			for (int i = 0; i < outlen; ++i) {
				for (int k = irange[i].start; k <= irange[i].end; ++k) {
					SocketInfoArray[k].clientdata.Update(fixedDelta);
				}
			}
			StackTime -= fixedDelta;
		}
		
		if (serverLog.str().size() > 0) {
			COORD pos = { 0, 0 }; // (x=0, y=0) 좌표
			SetConsoleCursorPosition(hConsole, pos);
			cout << serverLog.str();
			cout << errorCount << "\n";
			serverLog.str("");
			serverLog.clear();
		}

		//sock set init
		for (int i = 0; i < SelectSetCount; ++i) {
			FD_ZERO(&rset[i]);
			FD_ZERO(&wset[i]);
		}
		FD_SET(listen_sock, &rset[0]);

		stack += 1;
		stack = stack & 1;

		for (int k = 0; k < outlen; ++k) {
			for (int i = irange[k].start; i <= irange[k].end; ++i) {
				int setindex = ((i + 1) >> 6);
				if (isUpdated) {
					*(int*)&data = i;
					*(float*)&data[sizeof(int)] = SocketInfoArray[i].clientdata.x;
					*(float*)&data[sizeof(int) + sizeof(float)] = SocketInfoArray[i].clientdata.y;
					Client::CommonSend(data, 12);
#ifdef VAR_INITSET
					FD_SET(SocketInfoArray[i].sock, &wset[setindex]);
#endif
				}
#ifdef VAR_INITSET
				FD_SET(SocketInfoArray[i].sock, &rset[setindex]);
#endif
			}
		}

		ui64 ft = GetTicks();

		timeval t;
		t.tv_sec = 0;
		t.tv_usec = 0; // 60Hz
		for (int i = 0; i < SelectSetCount; ++i) {
			retval = select(0, &rset[i], &wset[i], NULL, &t);
			if (retval == SOCKET_ERROR) {
				dbgc[edbg::SelectError] += 1;
			}
		}
		serverLog << "select error : " << dbgc[edbg::SelectError] << "\n";

		//accept
		if (FD_ISSET(listen_sock, &rset[0])) {
			addrlen = sizeof(clientaddr);
			client_sock = accept(listen_sock, (struct sockaddr*)&clientaddr, &addrlen);
			if (client_sock == INVALID_SOCKET) {
				dbgc[edbg::AcceptError_InvalidSocket] += 1;
			}
			else {
				if (AddSocketInfo(client_sock)) {
				} 
				else {
					dbgc[edbg::AcceptError_InvalidSocket] += 1;
					closesocket(client_sock);
				}
			}

			if (--nready <= 0) continue;
		}
		serverLog << "accept error. invalid socket. : " << dbgc[edbg::AcceptError_InvalidSocket] << "\n";
		serverLog << "IDLE_Recv : " << dbgc[edbg::IDLE_Recv] << "\n";
		serverLog << "IDLE_Send : " << dbgc[edbg::IDLE_Send] << "\n";
		serverLog << "ClientForcedDisconnet : " << dbgc[edbg::ClientForcedDisconnet] << "\n";
		
		for (int k = 0; k < outlen; ++k) {
			for (int i = irange[k].start; i <= irange[k].end; ++i) {
				int setindex = ((i + 1) >> 6);
				bool isContinue = false;
				SOCKETINFO* ptr = &SocketInfoArray[i];
				if (FD_ISSET(ptr->sock, &rset[setindex])) {
					//recv data
				Recv_Again:
					int retval = recv(ptr->sock, &ptr->buf[ptr->rbuf_offset], BUFSIZ - (ptr->rbuf_offset), 0);

					if (retval == SOCKET_ERROR) {
						if (WSAGetLastError() == WSAEWOULDBLOCK) {
							dbgc[edbg::IDLE_Recv] += 1;
							goto END_RECV;
						}
						else {
							RemoveSocketInfo(i);
							goto END_RECV;
							//goto Recv_Again;
						}
					}
					else if (retval == 0) { // normal exit.
						RemoveSocketInfo(i);
						continue;
					}
					else {
						retval += ptr->rbuf_offset;
						ptr->recvbytes = retval;
						addrlen = sizeof(clientaddr);
						getpeername(ptr->sock, (sockaddr*)&clientaddr, &addrlen);
						ptr->buf[ptr->recvbytes] = 0;
						char addr[INET_ADDRSTRLEN] = {};
						inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));
						serverLog << "[TCP/" << addr << ":" << ntohs(clientaddr.sin_port) << "] : ";

						if (retval >= 10) errorCount += 1;

						int sro = 0;
						int ro = 0;
						for (; ro + 2 < retval; ro += 2) {
							char key = *(char*)&ptr->buf[ro];
							bool isdown = ('D' == *(char*)&ptr->buf[ro + sizeof(char)]);
							char isdownC = *(char*)&ptr->buf[ro + sizeof(char)];
							serverLog << key << isdownC;
							switch (key) {
							case 'W':
								ptr->clientdata.W = isdown;
								break;
							case 'S':
								ptr->clientdata.S = isdown;
								break;
							case 'A':
								ptr->clientdata.A = isdown;
								break;
							case 'D':
								ptr->clientdata.D = isdown;
								break;
							}
						}
						sro = ro;
						if (sro != retval) {
							int remain_len = retval - sro;
							for (int copyi = 0; copyi < remain_len; ++copyi) {
								ptr->buf[copyi] = ptr->buf[sro + copyi];
							}
							ptr->rbuf_offset = remain_len;
						}
						else ptr->rbuf_offset = 0;
						serverLog << "\n";
					}
					goto Recv_Again;
				}

			END_RECV:

				if (FD_ISSET(ptr->sock, &wset[setindex])) {
					//Commonly Send Data
					if (Commonly_send_data.size() != 0) {
						int k = 0;
						for (; k + BUFSIZ < Commonly_send_data.size(); k += BUFSIZ) {
							char* buf = Commonly_send_data.data() + k;
						common_Send_Again_inloop:
							retval = send(ptr->sock, buf, BUFSIZ, 0);
							if (retval == SOCKET_ERROR) {
								if (WSAGetLastError() == WSAEWOULDBLOCK) {
									dbgc[edbg::IDLE_Send] += 1;
									goto common_Send_END;
								}
								else {
									// 클라이언트가 강제종료를 한 상황.
									dbgc[edbg::ClientForcedDisconnet] += 1;
									RemoveSocketInfo(i);
									isContinue = true;
									goto common_Send_END;
								}
							}
							else if (retval != BUFSIZ) {
								__debugbreak();
							}
						}
						int len = Commonly_send_data.size() & (BUFSIZ - 1);
						char* buf = Commonly_send_data.data() + k;
					common_Send_Again:
						retval = send(ptr->sock, buf, len, 0);
						if (retval == SOCKET_ERROR) {
							if (WSAGetLastError() == WSAEWOULDBLOCK) {
								dbgc[edbg::IDLE_Send] += 1;
								goto common_Send_END;
							}
							else {
								// 클라이언트가 강제종료를 한 상황.
								dbgc[edbg::ClientForcedDisconnet] += 1;
								RemoveSocketInfo(i);
								isContinue = true;
								continue;
							}
							goto common_Send_Again;
						}
						else if (retval != len) {
							__debugbreak();
						}
					}
				common_Send_END:
					if (isContinue)
						continue;

					//personal Send Data
					if (ptr->clientdata.wbuf.size() != 0)
					{
						int k = 0;
						for (; k + BUFSIZ < ptr->clientdata.wbuf.size(); k += BUFSIZ) {
							char* buf = ptr->clientdata.wbuf.data() + k;
						Send_Again_inloop:
							retval = send(ptr->sock, buf, BUFSIZ, 0);
							if (retval == SOCKET_ERROR) {
								if (WSAGetLastError() == WSAEWOULDBLOCK) {
									dbgc[edbg::IDLE_Send] += 1;
									goto Send_END;
								}
								else {
									// 클라이언트가 강제종료를 한 상황.
									dbgc[edbg::ClientForcedDisconnet] += 1;
									RemoveSocketInfo(i);
									isContinue = true;
									goto Send_END;
								}
								//goto Send_Again_inloop;
							}
							else if (retval != BUFSIZ) {
								__debugbreak();
							}
						}
						int len2 = ptr->clientdata.wbuf.size() & (BUFSIZ - 1);
						char* buf2 = ptr->clientdata.wbuf.data() + k;
					Send_Again:
						retval = send(ptr->sock, buf2, len2, 0);
						if (retval == SOCKET_ERROR) {
							if (WSAGetLastError() == WSAEWOULDBLOCK) {
								dbgc[edbg::IDLE_Send] += 1;
								goto Send_END;
							}
							else {
								// 클라이언트가 강제종료를 한 상황.
								dbgc[edbg::ClientForcedDisconnet] += 1;
								RemoveSocketInfo(i);
								isContinue = true;
								continue;
							}
							goto Send_Again;
						}
						else if (retval != len2) {
							__debugbreak();
						}
						ptr->clientdata.wbuf.clear();
					}
				Send_END:
					if (isContinue)
						continue;
				}
			}
		}
		Commonly_send_data.clear();
	}

	closesocket(listen_sock);
	WSACleanup();
}
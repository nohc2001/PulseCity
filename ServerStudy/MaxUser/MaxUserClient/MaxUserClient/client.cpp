#define _CRT_SECURE_NO_WARNINGS // 구형 C 함수 사용 시 경고 끄기
#define _WINSOCK_DEPRECATED_NO_WARNINGS // 구형 소켓 API 사용 시 경고 끄기

#include <winsock2.h> // 윈속2 메인 헤더
#include <ws2tcpip.h> // 윈속2 확장 헤더
#include <iostream>
#include <random>
#include <iomanip>
#include <conio.h>
#include <format> // C++20 필수
#include "vecset.h"

#pragma comment(lib, "ws2_32") // ws2_32.lib 링크

using namespace std;

char* SERVERIP = (char*)"127.0.0.1";
#define SERVERPORT 9000
#define BUFSIZE    512

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
	ConnectFailedCount = 0,
	NWErrorWhenRecvFromServer = 1,
	ConnectedCountImmediate = 2,
	UserShutDown = 3,
	ConnectedCountDelayed = 4,
	DataCombineError = 5, // 지금 이 에러가 일어나는 원인 : 서버에서 특정 주기마다 일어나며, 클라가 많던 적던 상관없이 일어남.
};

void err_display(const char* msg) {
	//cout << "last window error code : " << WSAGetLastError() << endl;
	char* errorMessageBuffer = nullptr;
	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, WSAGetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errorMessageBuffer, 0, NULL);
	MessageBoxA(NULL, (const char*)errorMessageBuffer, msg, MB_ICONERROR);
	LocalFree(errorMessageBuffer);
}

void err_quit(const char* msg) {
	err_display(msg);
	exit(1);
}

#define QUERYPERFORMANCE_HZ 10000000//Hz
static inline int64_t GetTicks()
{
	LARGE_INTEGER ticks;
	QueryPerformanceCounter(&ticks);
	return ticks.QuadPart;
}

//#define VAR_RECVSKIP

// 2의 거듭제곱 - 1 형태면 좋다. (64의 배수 - 1)
// 시뮬레이션할 클라이언트 개수
constexpr int clientCount = 1023;
// 해당 클라이언트 만큼의 소켓 영역 할당.
vecset<SOCKET> sock = {};

//c++ 렌덤엔진
default_random_engine dre;
//렌덤이 균일하고 일정한 float 분포를 가지도록 설정.
uniform_real_distribution<float> urd{ 0.0f, 1000000.0f };

/*
* 설명,반환 : min~max까지의 float 중 렌덤한 것을 독립적으로 렌덤 선택후 반환.
* 매개변수 :
* float min : 최소값
* float max : 최대값
*/
float randomRangef(float min, float max) {
	float r = urd(dre);
	return min + r * (max - min) / 1000000.0f;
}

int ConnectedClientCount = 0;
enum ConnectState {
	Trying = 0,
	WaitForDecideServerIndex = 1,
	Connected = 2,
};

// 클라이언트의 정보를 나타내는 상태
// 그냥 컴퓨터 하나라고 보면 됨.
struct ClientState {
	//위치
	float x;
	float y;

	// 데이터 통신에 사용할 변수
	char rbuf[BUFSIZE];
	vector<char> wbuf;
	// 잘린 데이터 복원에 사용되기 위해 사용됨.
	int rbuf_offset = 0;

	int W; // 0 : non push // 1 : push moment / 2 : pushing / 3 : pop
	int A;
	int S;
	int D;

	// 각 유저들은 0.1초마다 행동을 바꿀 수 있다.
	// 그 행동은 WASD를 누르거나 때는것이다.
	static constexpr int64_t delay = QUERYPERFORMANCE_HZ / 10;
	int64_t flow = delay;
	int64_t lastTick = 0;

	// 현재 클라이언트가 서버와 연결되었는지의 여부
	int sockindex = 0;
	int serverindex = 0;
	float playtime = 0;
	ConnectState connectState = Trying;
	bool isConnectToServer = true;

	__forceinline void ConnetToServer() {
		ConnectedClientCount += 1;
		rbuf_offset = 0;
		ZeroMemory(rbuf, BUFSIZ);
		wbuf.clear();
		sockindex  = sock.Alloc();
		playtime = 0;

		// 소켓 생성
		u_long on = 1;
		sock[sockindex] = socket(AF_INET, SOCK_STREAM, 0);
		if (sock[sockindex] == INVALID_SOCKET) err_quit("socket()");

		// 넌블럭킹 모드 설정
		u_long mode = 1;  // 0 = blocking, 1 = non-blocking
		int result = ioctlsocket(sock[sockindex], FIONBIO, &mode);
		if (result != NO_ERROR) {
			std::cerr << "ioctlsocket() failed\n";
			DisConnectToServer();
			return;
		}

		// connect()
		struct sockaddr_in serveraddr;
		memset(&serveraddr, 0, sizeof(serveraddr));
		serveraddr.sin_family = AF_INET;
		inet_pton(AF_INET, SERVERIP, &serveraddr.sin_addr);
		serveraddr.sin_port = htons(SERVERPORT);
		int retval = connect(sock[sockindex], (struct sockaddr*)&serveraddr, sizeof(serveraddr));
		int error = WSAGetLastError();
		if (retval < 0 && error != WSAEWOULDBLOCK) {
			//perror("connect failed");
			DisConnectToServer();
			dbgc[edbg::ConnectFailedCount] += 1;
			return;
		}
		else if (retval < 0 && errno == EINPROGRESS) {
			connectState = Trying;
		}
		else {
			connectState = WaitForDecideServerIndex;
		}
		dbgc[edbg::ConnectedCountImmediate] += 1;
		isConnectToServer = true;
	}

	__forceinline void DisConnectToServer() {
		ConnectedClientCount -= 1;
		rbuf_offset = 0;
		wbuf.clear();
		ZeroMemory(rbuf, BUFSIZ);
		isConnectToServer = false;
		sock.Free(sockindex);
		closesocket(sock[sockindex]);
		sock[sockindex] = 0;
		sockindex = -1;
	}

	// 결론적으로 wbuf에다 data를 집어넣는데, 기존 데이터 뒤에 위치시킴.
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

	//플레이어 판단 로직
	void Decide() {
		
		if ((W & 1) == 1) W += 1;
		W = W & 3;
		if ((A & 1) == 1) A += 1;
		A = A & 3;
		if ((S & 1) == 1) S += 1;
		S = S & 3;
		if ((D & 1) == 1) D += 1;
		D = D & 3;

		int64_t ft = GetTicks();
		flow -= (ft - lastTick);
		unsigned int del = (unsigned int)(ft - lastTick);
		float deltaTime = (float)((double)del / (double)QUERYPERFORMANCE_HZ);
		playtime += deltaTime;
		if (flow <= 0) {
			flow = (float)delay * randomRangef(0.5f, 2.0f);
			if (randomRangef(0, 100.0f) < 10.0f) {
				if (W == 0) W = 1;
				if (W == 2) W = 3;
			}
			if (randomRangef(0, 100.0f) < 10.0f) {
				if (A == 0) A = 1;
				if (A == 2) A = 3;
			}
			if (randomRangef(0, 100.0f) < 10.0f) {
				if (S == 0) S = 1;
				if (S == 2) S = 3;
			}
			if (randomRangef(0, 100.0f) < 10.0f) {
				if (D == 0) D = 1;
				if (D == 2) D = 3;
			}

			if (W == 1) Send((char*)"WD", 2);
			if (W == 3) Send((char*)"WU", 2);
			if (S == 1) Send((char*)"SD", 2);
			if (S == 3) Send((char*)"SU", 2);
			if (A == 1) Send((char*)"AD", 2);
			if (A == 3) Send((char*)"AU", 2);
			if (D == 1) Send((char*)"DD", 2);
			if (D == 3) Send((char*)"DU", 2);
		}
		lastTick = ft;
	}
};
// 이 인덱스는 sock의 인덱스와 같지 않다.
ClientState clientStateArr[clientCount] = {};

//Server To Client
// [client Id] [float x] [float y] (12byte)

//Client to Server
// [WASD] [down/up] (2byte)

// 실제 클라이언트들이 clientCount 만큼 있다고 가정하고 fd_set 등을 클라이언트 개수만큼 준비.
fd_set readfds[clientCount], writefds[clientCount];

int main(int argc, char* argv[])
{
	sock.Init(clientCount);

	int retval = 0;

	// 명령행 인수가 있으면 IP 주소로 사용
	if (argc > 1) SERVERIP = argv[1];

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// 클라이언트 들이 차례대로 접속.
	for (int i = 0; i < clientCount; ++i) {
		clientStateArr[i].ConnetToServer();
	}

	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	int coutPage = 0;
	double SetIndexd = 0;
	double SetIndexInc = 1.0f / 64.0f;
	while (1) {
		// 보여지는 페이지 넘기기
		if (_kbhit()) {
			char c = _getch();
			if (c == 'E' || c == 'e') {
				coutPage += 1;
				if (coutPage * 64 > clientCount) coutPage = 0;
			}
			if (c == 'Q' || c == 'q') {
				coutPage -= 1;
				if (coutPage < 0) coutPage = clientCount >> 6;
			}
			else if (c == 'C' || c == 'c') {
				system("cls");
			}
		}

		SetIndexd = -SetIndexInc;
		COORD pos = { 0, 0 }; // (x=0, y=0) 좌표
		SetConsoleCursorPosition(hConsole, pos);
		cout << fixed << setprecision(2);
		for (int i = 0; i < clientCount; ++i) {
			ClientState& client = clientStateArr[i];
			//SetIndexd += SetIndexInc;
			
			if (client.isConnectToServer == true) {
				if (client.connectState == Connected) {
					// 클라이언트에서 입력
					client.Decide();

					// 클라이언트 정보 적제 syscall 아님.
					//int setIndex = (int)SetIndexd;
					if (i>>6 == coutPage) {
						char inputdata[5] = "____";
						inputdata[0] = (client.W == 2) ? 'W' : '_';
						inputdata[1] = (client.A == 2) ? 'A' : '_';
						inputdata[2] = (client.S == 2) ? 'S' : '_';
						inputdata[3] = (client.D == 2) ? 'D' : '_';
						cout << format("client{:<5} Input{:<5} Pos({:<7.2},{:<7.2})", i, inputdata, client.x, client.y);
						//cout << "pos " << i << " : \t(" << client.x << ", " << client.y;
						if ((i & 1) == 1) cout << "\n";
						else cout << "\t";
					}

					if (rand() % 1000 == 0 && client.playtime > 30.0f) {
						client.DisConnectToServer();
						dbgc[edbg::UserShutDown] += 1;
						//dbgbreak(dbgc[edbg::UserShutDown] > 100);
						continue;
					}
				}
				else if (client.connectState == WaitForDecideServerIndex) {
					// 클라이언트 정보 적제 syscall 아님.
					//int setIndex = (int)SetIndexd;
					if (i>>6 == coutPage) {
						cout << format("client{:<5} WaitForDecideServerIndex       ", i);
						//cout << "pos " << i << " : \tWaitAllocIndex";
						if ((i & 1) == 1) cout << "\n";
						else cout << "\t";
					}
				}
			}
			else {
				//int setIndex = (int)SetIndexd;
				if (i>>6 == coutPage) {
					cout << format("client{:<5} DisConnect                     ", i);
					//cout << "pos " << i << " : \tdisconnected";
					if ((i & 1) == 1) cout << "\n";
					else cout << "\t";
				}
			}

			// 2퍼센트로 클라이언트가 접속상태를 바꾼다.
			if (client.isConnectToServer == false ||
				client.isConnectToServer == true && client.connectState == Trying)
			{
				if (client.isConnectToServer == false) {
					if (rand() % 1000 == 0) {
						client.ConnetToServer();
					}
					continue;
				}
			}

			int sockindex = client.sockindex;

			//select
			FD_ZERO(&readfds[i]);
			FD_ZERO(&writefds[i]);
			FD_SET(sock[sockindex], &readfds[i]);
			FD_SET(sock[sockindex], &writefds[i]);

			timeval t;
			t.tv_sec = 0;
			t.tv_usec = 0;
			select(sock[sockindex] + 1, &readfds[i], &writefds[i], NULL, &t);
			if (retval == SOCKET_ERROR) {
				//cout << "error\n";
			}

			// read
			if (FD_ISSET(sock[sockindex], &readfds[i])) {
			Recv_Again :
				// recvBuffer의 rbufferOffset 번째부터 읽은 데이터를 넣는다.
				retval = recv(sock[sockindex], &client.rbuf[client.rbuf_offset], BUFSIZE - client.rbuf_offset, 0);
				if (retval == -1) {
					if (WSAGetLastError() == WSAEWOULDBLOCK) {
						// 아직 읽을 수 없다면 다음 루프로 넘긴다.
						goto END_RECV;
					}
					else {
						// 네트워크 오류가 발생되었거나 서버가 죽은 상황
						//goto Recv_Again;
						if (client.connectState == Connected) {
							client.DisConnectToServer();
							dbgc[edbg::NWErrorWhenRecvFromServer] += 1;
							//dbgbreak(dbgc[edbg::NWErrorWhenRecvFromServer] > 100);
							continue;
						}
						goto END_RECV;
					}
				}
				else if (retval == 0) {
					// 서버가 종료됨.
					client.DisConnectToServer();
					continue;
				}

				retval += client.rbuf_offset;

				if (client.connectState == WaitForDecideServerIndex) {
					client.serverindex = *(int*)&client.rbuf[0];
					int sro = 4;
					if (sro != retval) {
						int remain_len = retval - sro;
						for (int copyi = 0; copyi < remain_len; ++copyi) {
							client.rbuf[copyi] = client.rbuf[sro + copyi];
						}
						client.rbuf_offset = remain_len;
					}
					else client.rbuf_offset = 0;
					client.connectState = Connected;
					goto Recv_Again;
				}
				else if (client.connectState == Connected) {
					// 읽은 데이터를 처리 (클라이언트의 움직임.)
					int sro = 0;
					int ro = 0;
					for (; ro + 12 < retval; ro += 12) {
						int clientId = *(int*)&client.rbuf[ro];
						// 왜 잘못되는가?
						if (clientId > clientCount || clientId < 0) {
							dbgc[edbg::DataCombineError] += 1;
							ro += 4;
							continue;
						}
						else {
							if (clientId == client.serverindex) {
								client.x = *(float*)&client.rbuf[ro + sizeof(int)];
								client.y = *(float*)&client.rbuf[ro + sizeof(int) + sizeof(float)];
							}
							else {
								// 본래는 클라이언트는 자신의 RAM 안의 버퍼에 주변 모든 플레이어 데이터를 저장해야 한다.
								// 하지만 이 프로그램에서는 여러 클라이언트를 돌리기 때문에 생략한다.
							}
						}
					}
					sro = ro;
					if (sro != retval) {
						int remain_len = retval - sro;
						for (int copyi = 0; copyi < remain_len; ++copyi) {
							client.rbuf[copyi] = client.rbuf[sro + copyi];
						}
						client.rbuf_offset = remain_len;
					}
					else client.rbuf_offset = 0;
					goto Recv_Again;
				}
			}

		END_RECV:

			if (FD_ISSET(sock[sockindex], &writefds[i])) {
				//connect 되어있는지 확인? getsockopt??
				if (client.connectState == Trying) {
					// [중요] 접속 중이었던 소켓이 쓰기 가능해짐 -> 접속 완료 시도
					int error = 0;
					socklen_t len = sizeof(error);

					// 실제로 접속이 성공했는지, 에러가 났는지 확인
					if (getsockopt(sock[sockindex], SOL_SOCKET, SO_ERROR, (char*)&error, &len) < 0) {
						// getsockopt 자체 실패
						client.DisConnectToServer();
					}
					else if (error != 0) {
						// 접속 실패 (서버 거부 등)
						client.DisConnectToServer();
					}
					else {
						// 접속 성공!
						client.connectState = Connected;
						dbgc[edbg::ConnectedCountDelayed] += 1;
						//dbgbreak(dbgc[edbg::ConnectedCountDelayed] > 100);
					}
				}

				if (client.wbuf.size() != 0) {
					// 쓰기 가능한 소켓의 상태.
					int k = 0;
					for (; k + BUFSIZE < client.wbuf.size(); k += BUFSIZE) {
						char* buf = client.wbuf.data() + k;
					Send_Again_inloop:
						retval = send(sock[sockindex], buf, BUFSIZE, 0);
						if (retval == SOCKET_ERROR) {
							goto Send_Again_inloop;
						}
					}
					int len = client.wbuf.size() & (BUFSIZE - 1);
					char* buf = client.wbuf.data() + k;
				Send_Again:
					retval = send(sock[sockindex], buf, len, 0);
					if (retval == SOCKET_ERROR) {
						goto Send_Again;
					}
					client.wbuf.clear();
				}
			}
			else {
				// 만약 쓸 수 없는데 wbuf가 있을 경우 (접속종료)
				client.DisConnectToServer();
			}
		}

		cout << "ERROR : ConnectFailedCount : " << dbgc[edbg::ConnectFailedCount] << " \n";
		cout << "ERROR : NWErrorWhenRecvFromServer : " << dbgc[edbg::NWErrorWhenRecvFromServer] << " \n";
		cout << "CRITICAL ERROR : DataCombineError : " << dbgc[edbg::DataCombineError] << " \n";
		cout << "동시접속자 : " << ConnectedClientCount << "\n";
		cout << endl;
	}

	for (int i = 0; i < clientCount; ++i) {
		// 소켓 닫기
		ClientState& client = clientStateArr[i];
		if (client.sockindex > 0 && client.isConnectToServer) {
			client.DisConnectToServer();
		}
	}

	// 윈속 종료
	WSACleanup();
	return 0;
}
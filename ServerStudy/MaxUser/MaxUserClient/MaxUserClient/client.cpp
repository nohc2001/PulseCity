#define _CRT_SECURE_NO_WARNINGS // 구형 C 함수 사용 시 경고 끄기
#define _WINSOCK_DEPRECATED_NO_WARNINGS // 구형 소켓 API 사용 시 경고 끄기

#include <winsock2.h> // 윈속2 메인 헤더
#include <ws2tcpip.h> // 윈속2 확장 헤더
#include <iostream>
#include <random>
#include <iomanip>
#include <conio.h>

#pragma comment(lib, "ws2_32") // ws2_32.lib 링크

using namespace std;

char* SERVERIP = (char*)"127.0.0.1";
#define SERVERPORT 9000
#define BUFSIZE    512

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
constexpr int clientCount = 511;
SOCKET sock[clientCount] = {};

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

struct ClientState {
	float x;
	float y;

	// 데이터 통신에 사용할 변수
	char rbuf[BUFSIZE + 1];
	vector<char> wbuf;
	int rbuf_offset = 0;

	int W; // 0 : non push // 1 : push moment / 2 : pushing / 3 : pop
	int A;
	int S;
	int D;

	static constexpr int64_t delay = QUERYPERFORMANCE_HZ / 10;
	int64_t flow = delay;
	int64_t lastTick = 0;

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

ClientState clientStateArr[clientCount];

//2MB
struct TempVec2MB {
	int up = 0;
	char RecvTempMemeory[2097152] = {};

	__forceinline void push(char* data, int len) {
		if (up + len < 2097152) {
			memcpy(&RecvTempMemeory[up], data, len);
			up += len;
		}
	}

	__forceinline void clear() {
		up = 0;
	}
};


//Server To Client
// [client Id] [float x] [float y] (12byte)

//Client to Server
// [WASD] [down/up] (2byte)

TempVec2MB tempBuff;
fd_set readfds[clientCount], writefds[clientCount];

int main(int argc, char* argv[])
{
	int retval;

	// 명령행 인수가 있으면 IP 주소로 사용
	if (argc > 1) SERVERIP = argv[1];

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	for (int i = 0; i < clientCount; ++i) {
		// 소켓 생성
		u_long on = 1;
		sock[i] = socket(AF_INET, SOCK_STREAM, 0);
		if (sock[i] == INVALID_SOCKET) err_quit("socket()");

		// 넌블럭킹 모드 설정
		u_long mode = 1;  // 0 = blocking, 1 = non-blocking
		int result = ioctlsocket(sock[i], FIONBIO, &mode);
		if (result != NO_ERROR) {
			std::cerr << "ioctlsocket() failed\n";
			return 1;
		}

		// connect()
		struct sockaddr_in serveraddr;
		memset(&serveraddr, 0, sizeof(serveraddr));
		serveraddr.sin_family = AF_INET;
		inet_pton(AF_INET, SERVERIP, &serveraddr.sin_addr);
		serveraddr.sin_port = htons(SERVERPORT);
		retval = connect(sock[i], (struct sockaddr*)&serveraddr, sizeof(serveraddr));
		if (retval < 0 && errno != EINPROGRESS) {
			perror("connect failed");
		}
	}

	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	int coutPage = 0;
	double SetIndexd = 0;
	double SetIndexInc = 1.0f / 64.0f;
	while (1) {
		if (_kbhit()) {
			coutPage += 1;
			if (coutPage * 64 > clientCount + 1) coutPage = 0;
			_getch();
		}

		SetIndexd = 0;
		COORD pos = { 0, 0 }; // (x=0, y=0) 좌표
		SetConsoleCursorPosition(hConsole, pos);
		cout << fixed << setprecision(2);

		for (int i = 0; i < clientCount; ++i) {

			/*int err = 0;
			int len = sizeof(err);
			if (getsockopt(sock[i], SOL_SOCKET, SO_ERROR, (char*)&err, &len) == SOCKET_ERROR)
			{
				printf("connecting : %d, error : %d\n", i, WSAGetLastError());
				continue;
			}*/
			ClientState& client = clientStateArr[i];

			int setIndex = (int)SetIndexd;
			if (setIndex == coutPage) {
				cout << "pos " << i << " : \t(" << client.x << ", " << client.y;
				if ((i & 3) == 3) cout << ")\n";
				else cout << ")\t";
			}
			SetIndexd += SetIndexInc;

			client.Decide();

			FD_ZERO(&readfds[i]);
			FD_ZERO(&writefds[i]);
			FD_SET(sock[i], &readfds[i]);
			FD_SET(sock[i], &writefds[i]);

			timeval t;
			t.tv_sec = 0;
			t.tv_usec = 0;
			select(sock[i] + 1, &readfds[i], &writefds[i], NULL, &t);
			if (retval == SOCKET_ERROR) {
				//cout << "error\n";
			}

			if (FD_ISSET(sock[i], &readfds[i])) {
#ifndef VAR_RECVSKIP
				Recv_Again :
				retval = recv(sock[i], &client.rbuf[client.rbuf_offset], BUFSIZE - client.rbuf_offset, 0);
				if (retval == -1) {
					if (WSAGetLastError() == WSAEWOULDBLOCK) {
						goto END_RECV;
					}
					else {
						goto Recv_Again;
					}
				}

				retval += client.rbuf_offset;

				int sro = 0;
				int ro = 0;
				for (; ro + 12 <= retval; ro += 12) {
					int clientId = *(int*)&client.rbuf[ro];
					clientStateArr[clientId].x = *(float*)&client.rbuf[ro + sizeof(int)];
					clientStateArr[clientId].y = *(float*)&client.rbuf[ro + sizeof(int) + sizeof(float)];
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
#else
			Recv_Again:
				retval = recv(sock[i], &tempBuff.RecvTempMemeory[tempBuff.up], BUFSIZE - client.rbuf_offset, 0);
				if (retval == -1) {
					if (WSAGetLastError() == WSAEWOULDBLOCK) {
						goto END_RECV;
					}
					else {
						goto Recv_Again;
					}
				}
				tempBuff.up += retval;

				int ro = retval - (retval % 12);
				client.rbuf_offset = ro;
				ro -= 12;



				goto Recv_Again;
#endif
			}

		END_RECV:

			if (FD_ISSET(sock[i], &writefds[i])) {
				//connect 되어있는지 확인?

				if (client.wbuf.size() != 0) {
					// 쓰기 가능한 소켓의 상태.
					int k = 0;
					for (; k + BUFSIZE < client.wbuf.size(); k += BUFSIZE) {
						char* buf = client.wbuf.data() + k;
					Send_Again_inloop:
						retval = send(sock[i], buf, BUFSIZE, 0);
						if (retval == SOCKET_ERROR) {
							goto Send_Again_inloop;
						}
					}
					int len = client.wbuf.size() & (BUFSIZE - 1);
					char* buf = client.wbuf.data() + k;
				Send_Again:
					retval = send(sock[i], buf, len, 0);
					if (retval == SOCKET_ERROR) {
						goto Send_Again;
					}
					client.wbuf.clear();
				}
			}
		}
		cout << endl;
	}

	for (int i = 0; i < clientCount; ++i) {
		// 소켓 닫기
		closesocket(sock[i]);
	}

	// 윈속 종료
	WSACleanup();
	return 0;
}
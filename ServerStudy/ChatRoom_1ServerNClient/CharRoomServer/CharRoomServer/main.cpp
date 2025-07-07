// Including SDKDDKVer.h defines the highest available Windows platform.

// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.

#include <SDKDDKVer.h>

#include <Ws2tcpip.h>
#include <assert.h>
#include <exception>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <tchar.h>
#include <winsock2.h>
#include <cstdint>

#include "ImaysNet.h"
#include <chrono>
#include <thread>
#include <vector>
using namespace std;

typedef unsigned long long ui64;
#define QUERYPERFORMANCE_HZ 10000000.0//Hz
static inline ui64 GetTicks()
{
	LARGE_INTEGER ticks;
	QueryPerformanceCounter(&ticks);
	return ticks.QuadPart;
}

constexpr unsigned int MAXCHAR_DATA = 4096;
struct ChatRoomDataStruct {
	char ChatRoomData[MAXCHAR_DATA] = {};
	int pivot = 0;
};
ChatRoomDataStruct chatdata;

bool MessageUpdated = false;
void PushMessage(char* str, int len) {
	for (int i = 0; i < len; ++i) {
		chatdata.ChatRoomData[chatdata.pivot] = str[i];
		chatdata.pivot = (chatdata.pivot + 1) & (MAXCHAR_DATA-1);
	}

	MessageUpdated = true;
}

Socket ParticipantSockets[16] = {};
int participantCount = 0;

int main() {
	for (int i = 0; i < 4096; ++i) {
		chatdata.ChatRoomData[i] = ' ';
	}
	chatdata.pivot = 0;

	Socket listenSocket(SocketType::Tcp); // 1
	listenSocket.Bind(Endpoint("0.0.0.0", 5959)); // 2

	cout << "Server Start" << endl;

	ui64 ft, st;
	ui64 TimeStack = 0;
	st = GetTicks();
	listenSocket.SetNonblocking();
	vector<PollFD> readFds;
	//??
	listenSocket.Listen();	// 3

	while (true) {
		readFds.reserve(participantCount + 1);
		readFds.clear();
		PollFD item2;
		item2.m_pollfd.events = POLLRDNORM;
		item2.m_pollfd.fd = listenSocket.m_fd;
		item2.m_pollfd.revents = 0;
		readFds.push_back(item2); // first is listen socket.
		for (int i=0;i< participantCount;++i)
		{
			PollFD item;
			item.m_pollfd.events = POLLRDNORM;
			item.m_pollfd.fd = ParticipantSockets[i].m_fd;
			item.m_pollfd.revents = 0;
			readFds.push_back(item);
		}

		// I/O 가능 이벤트가 있을 때까지 기다립니다.
		Poll(readFds.data(), (int)readFds.size(), 100);
		
		int num = 0;
		for (auto readFd : readFds)
		{
			if (readFd.m_pollfd.revents != 0)
			{
				if (num == 0) {
					// 4
					Socket tcpConnection;
					string ignore;
					listenSocket.Accept(ParticipantSockets[participantCount], ignore);
					auto a = ParticipantSockets[participantCount].GetPeerAddr().ToString();
					ParticipantSockets[participantCount].SetNonblocking();
					cout << "Socket from " << a << " is accepted.\n";
					char newParticipantstr[128] = {};
					sprintf_s(newParticipantstr, "New Participant - %d\n", participantCount);
					PushMessage(newParticipantstr, strlen(newParticipantstr));

					participantCount += 1;
					if (participantCount >= 16) {
						break;
					}
				}
				else {
					int index = num - 1;
					int result = ParticipantSockets[index].Receive();
					if (result > 0) {
						int len = strlen(ParticipantSockets[index].m_receiveBuffer);
						cout << "participant " << index << " sending message (Len : " << len << ")" << endl;
						PushMessage(ParticipantSockets[index].m_receiveBuffer, len);
					}
					else if (result <= 0) {
						ParticipantSockets[index].Close();
						for (int k = index; k < participantCount; ++k) {
							ParticipantSockets[k] = ParticipantSockets[k + 1];
						}
						participantCount -= 1;
						cout << "Participant " << index << "Left the Chatroom." << endl;
					}
				}
			}
			num += 1;
		}

		ft = GetTicks();
		TimeStack += ft - st;
		st = ft;
		if (TimeStack > QUERYPERFORMANCE_HZ && MessageUpdated) {
			for (int i = 0; i < participantCount; ++i) {
				ParticipantSockets[i].Send((char*) & chatdata, MAXCHAR_DATA + 4);
			}
			MessageUpdated = false;
			TimeStack = 0;
		}
	}
	return 0;
}
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

#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>

#include "NWLib/ImaysNet.h"
#include <chrono>
#include <thread>
#include <vector>

using namespace std;
using namespace DirectX;
using namespace DirectX::PackedVector;

typedef unsigned long long ui64;
#define QUERYPERFORMANCE_HZ 10000000.0//Hz
static inline ui64 GetTicks()
{
	LARGE_INTEGER ticks;
	QueryPerformanceCounter(&ticks);
	return ticks.QuadPart;
}

enum InputID {
	KeyboardW = 0,
	KeyboardA = 1,
	KeyboardS = 2,
	KeyboardD = 3,
};

//socket memory size is too big -> because recieve buffer..
struct ParticipantData {
	Socket socket;
	
	XMVECTOR pos;
	bool Connected = false;

	bool InputBuffer[4];
	static constexpr float MoveSpeed = 3.0f;
};

struct World {
	static constexpr int max_participant = 16;
	ParticipantData Participants[max_participant] = {};
	int participantCount = 0;

	int RecycleArr[max_participant] = {};
	int RecycleCount = 0;
	bool WorldUpdate = false;

	void Init() {
		RecycleCount = 0;
		WorldUpdate = false;
		participantCount = 0;
		for (int i = 0; i < max_participant; ++i) {
			RecycleArr[0] = 0;
			Participants[i].Connected = false;
			Participants[i].pos = { 0, 0, 0 };
		}
	}

	int AllocNewParticipantIndex(){
		int index = 0;
		if (RecycleCount > 0) {
			index = RecycleArr[RecycleCount - 1];
			RecycleCount -= 1;
		}
		else {
			index = participantCount;

			participantCount += 1;
			if (participantCount >= max_participant) {
				return -1;
			}
		}

		ZeroMemory(&Participants[index], sizeof(ParticipantData));

		Participants[index].Connected = true;
		return index;
	}

	void LeaveParticipant(int index) {
		//Participants[index].socket.Close();
		Participants[index].Connected = false;
		RecycleArr[RecycleCount] = index;
		RecycleCount += 1;
	}
};

World gameworld;
float DeltaTime = 0;
float StackTime = 0;
constexpr float SendDelay = 0.017f;

void GameUpdate() {
	StackTime += DeltaTime;
	for (int i = 0; i < gameworld.participantCount; ++i) {
		if (gameworld.Participants[i].Connected == false) continue;
		if (gameworld.Participants[i].InputBuffer[(int)InputID::KeyboardW]) {
			gameworld.Participants[i].pos += XMVectorSet(0, 0, ParticipantData::MoveSpeed * DeltaTime, 0);
			gameworld.WorldUpdate = true;
		}
		if (gameworld.Participants[i].InputBuffer[(int)InputID::KeyboardA]) {
			gameworld.Participants[i].pos += XMVectorSet(-ParticipantData::MoveSpeed * DeltaTime, 0, 0, 0);
			gameworld.WorldUpdate = true;
		}
		if (gameworld.Participants[i].InputBuffer[(int)InputID::KeyboardS]) {
			gameworld.Participants[i].pos += XMVectorSet(0, 0, -ParticipantData::MoveSpeed * DeltaTime, 0);
			gameworld.WorldUpdate = true;
		}
		if (gameworld.Participants[i].InputBuffer[(int)InputID::KeyboardD]) {
			gameworld.Participants[i].pos += XMVectorSet(ParticipantData::MoveSpeed * DeltaTime, 0, 0, 0);
			gameworld.WorldUpdate = true;
		}
	}

	if (StackTime > SendDelay) {
		float SendData[World::max_participant][4] = { {} };
		for (int i = 0; i < gameworld.participantCount; ++i) {
			if (gameworld.Participants[i].Connected == false) {
				SendData[i][0] = 0;
				SendData[i][1] = 0;
				SendData[i][2] = 0;
				SendData[i][3] = 0;
			}
			else {
				*(XMVECTOR*)&SendData[i] = gameworld.Participants[i].pos;
				SendData[i][3] = 1;
			}
		}
		if (gameworld.WorldUpdate) {
			for (int i = 0; i < gameworld.participantCount; ++i) {
				if (gameworld.Participants[i].Connected == false) continue;
				gameworld.Participants[i].socket.Send((char*)SendData, sizeof(float) * 64);
			}
		}
		StackTime = 0;
	}

	gameworld.WorldUpdate = false;
}

int main() {
	gameworld.Init();

	Socket listenSocket(SocketType::Tcp); // 1
	listenSocket.Bind(Endpoint("0.0.0.0", 1000)); // 2

	cout << "Server Start" << endl;

	ui64 ft, st;
	ui64 TimeStack = 0;
	st = GetTicks();
	listenSocket.SetNonblocking();
	vector<PollFD> readFds;
	//??
	listenSocket.Listen();	// 3
	double DeltaFlow = 0;
	constexpr double InvHZ = 1.0 / (double)QUERYPERFORMANCE_HZ;
	while (true) {
		ft = GetTicks();
		DeltaFlow += (double)(ft - st) * InvHZ;
		if (DeltaFlow >= 0.016) { // limiting fps.
			DeltaTime = (float)DeltaFlow;
			GameUpdate();
			DeltaFlow = 0;
		}

		readFds.reserve(gameworld.participantCount + 1);
		readFds.clear();

		PollFD item2;
		item2.m_pollfd.events = POLLRDNORM;
		item2.m_pollfd.fd = listenSocket.m_fd;
		item2.m_pollfd.revents = 0;
		readFds.push_back(item2); // first is listen socket.
		for (int i=0;i< gameworld.participantCount;++i)
		{
			PollFD item;
			item.m_pollfd.events = POLLRDNORM;
			item.m_pollfd.fd = gameworld.Participants[i].socket.m_fd;
			item.m_pollfd.revents = 0;
			readFds.push_back(item);
		}

		// I/O 가능 이벤트가 있을 때까지 기다립니다.
		Poll(readFds.data(), (int)readFds.size(), 100);
		
		vector<int> newPart;

		int num = 0;
		for (auto readFd : readFds)
		{
			if (readFd.m_pollfd.revents != 0)
			{
				if (num == 0) {
					// 4
					Socket tcpConnection;
					string ignore;

					int newindex = gameworld.AllocNewParticipantIndex();
					if (newindex >= 0) {
						newPart.push_back(newindex);
						listenSocket.Accept(gameworld.Participants[newindex].socket, ignore);
						auto a = gameworld.Participants[newindex].socket.GetPeerAddr().ToString();
						gameworld.Participants[newindex].socket.SetNonblocking();
						cout << "Socket from " << a << " is accepted.\n";
						char newParticipantstr[128] = {};
						sprintf_s(newParticipantstr, "New Participant - %d\n", newindex);
						cout << newParticipantstr << endl;
					}
					else {
						cout << "too many participant!!" << endl;
					}
				}
				else {
					int index = num - 1;
					if (gameworld.Participants[index].Connected == false) continue;
					int result = gameworld.Participants[index].socket.Receive();
					if (result > 0) {
						char* cptr = gameworld.Participants[index].socket.m_receiveBuffer;
						bool putv = false;
						if (cptr[1] == 'D') {
							putv = true;
						}
						else if (cptr[1] == 'U') {
							putv = false;
						}

						switch (cptr[0]) {
						case 'W':
							gameworld.Participants[index].InputBuffer[(int)InputID::KeyboardW] = putv;
							break;
						case 'A':
							gameworld.Participants[index].InputBuffer[(int)InputID::KeyboardA] = putv;
							break;
						case 'S':
							gameworld.Participants[index].InputBuffer[(int)InputID::KeyboardS] = putv;
							break;
						case 'D':
							gameworld.Participants[index].InputBuffer[(int)InputID::KeyboardD] = putv;
							break;
						}

						cout << "Participant " << index << " Input : " << cptr[0] << cptr[1] << endl;
					}
					else if (result <= 0) {
						bool isPart = false;
						for (int n : newPart) {
							if (index == n)isPart = true;
						}
						if (isPart == false) {
							gameworld.LeaveParticipant(index);
							cout << "Participant " << index << "Left the Game." << endl;
						}
					}
				}
			}
			num += 1;
		}

		for (int n : newPart) {
			gameworld.Participants[n].Connected = true;
		}

		st = ft;
	}
	return 0;
}
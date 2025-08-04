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
#include <signal.h>

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

// true가 되면 프로그램을 종료합니다.
volatile bool stopWorking = false;
void ProcessSignalAction(int sig_number)
{
	if (sig_number == SIGINT)
		stopWorking = true;
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
		HANDLE FinishEvent[gameworld.max_participant] = {};
		DWORD bytesSent[gameworld.max_participant] = {};
		for (int i = 0; i < gameworld.participantCount; ++i) {
			if (gameworld.Participants[i].Connected == false) continue;

			//gameworld.Participants[i].socket.m_isReadOverlapped = false;
			//WSABUF wsaBuf;
			//wsaBuf.buf = (char*)SendData;
			//wsaBuf.len = sizeof(float) * 64;
			//WSAOVERLAPPED overlapped;
			//memset(&overlapped, 0, sizeof(overlapped));
			//FinishEvent[i] = WSACreateEvent();
			//overlapped.hEvent = FinishEvent[i];  // 이벤트 기반 확인용
			//int result = WSASend(*(SOCKET*)&gameworld.Participants[i].socket, &wsaBuf, 1, &bytesSent[i], 0, &overlapped, NULL);
			//WSAWaitForMultipleEvents(1, &FinishEvent[i], TRUE, WSA_INFINITE, FALSE);
			//DWORD Flag = 0;
			//WSAGetOverlappedResult(*(SOCKET*)&gameworld.Participants[i].socket, &overlapped, &bytesSent[i], FALSE, &Flag);
			//gameworld.Participants[i].socket.m_isReadOverlapped = true;
			gameworld.Participants[i].socket.Send((char*)SendData, sizeof(float) * 64);
		}

		//for (int i = 0; i < gameworld.participantCount; ++i) {
		//	if (FinishEvent[i] == 0) continue;
		//	WSAOVERLAPPED overlapped;
		//	memset(&overlapped, 0, sizeof(overlapped));
		//	overlapped.hEvent = FinishEvent[i];
		//	WSAWaitForMultipleEvents(1, &FinishEvent[i], TRUE, WSA_INFINITE, FALSE);
		//	DWORD Flag = 0;
		//	WSAGetOverlappedResult(*(SOCKET*)&gameworld.Participants[i].socket, &overlapped, &bytesSent[i], FALSE, &Flag);
		//}
	}

	gameworld.WorldUpdate = false;
}

int main() {
	// 사용자가 ctl-c를 누르면 메인루프를 종료하게 만듭니다.
	gameworld.Init();
	
	signal(SIGINT, ProcessSignalAction);
	Iocp iocp(1);

	Socket listenSocket(SocketType::Tcp); // 1
	listenSocket.Bind(Endpoint("0.0.0.0", 1000)); // 2

	cout << "Server Start" << endl;

	ui64 ft, st;
	ui64 TimeStack = 0;
	st = GetTicks();
	listenSocket.Listen();	// 3

	iocp.Add(listenSocket, nullptr);

	Socket clientAcceptSock(SocketType::Tcp);
	string errorText;
	if (!listenSocket.AcceptOverlapped(clientAcceptSock, errorText)
		&& WSAGetLastError() != ERROR_IO_PENDING)
	{
		throw Exception("Overlapped AcceptEx failed.");
	}
	
	listenSocket.m_isReadOverlapped = true;

	cout << "서버가 시작되었습니다.\n";
	cout << "CTL-C키를 누르면 프로그램을 종료합니다.\n";

	double DeltaFlow = 0;
	constexpr double InvHZ = 1.0 / (double)QUERYPERFORMANCE_HZ;

	while (!stopWorking)
	{
		ft = GetTicks();
		DeltaFlow += (double)(ft - st) * InvHZ;
		st = ft;
		if (DeltaFlow >= 0.016) { // limiting fps.
			DeltaTime = (float)DeltaFlow;
			GameUpdate();
			DeltaFlow = 0;
		}

		// I/O 완료 이벤트가 있을 때까지 기다립니다.
		IocpEvents readEvents;
		iocp.Wait(readEvents, 0);

		for (int i = 0; i < readEvents.m_eventCount; i++) {
			auto& readEvent = readEvents.m_events[i];
			if (readEvent.lpCompletionKey == 0) { // is listen socket.
				if (clientAcceptSock.UpdateAcceptContext(listenSocket) != 0)
				{
					//리슨소켓을 닫았던지 하면 여기서 에러날거다. 그러면 리슨소켓 불능상태로 만들자.
					listenSocket.Close();
				}
				else {
					int newindex = gameworld.AllocNewParticipantIndex();
					if (newindex >= 0) {
						gameworld.Participants[newindex].socket = clientAcceptSock;

						listenSocket.AcceptOverlapped(gameworld.Participants[newindex].socket, errorText);

						void* completionKey = reinterpret_cast<void*>((ui64)newindex + 1);
						iocp.Add(gameworld.Participants[newindex].socket, completionKey);
						if (gameworld.Participants[newindex].socket.ReceiveOverlapped() != 0
							&& WSAGetLastError() != ERROR_IO_PENDING) {
							gameworld.LeaveParticipant(newindex);
							gameworld.Participants[newindex].socket.Close();
						}
						else {
							gameworld.Participants[newindex].socket.m_isReadOverlapped = true;
							cout << "Client joined. clientIndex : " << newindex << ".\n";
						}

						ZeroMemory(&clientAcceptSock, sizeof(Socket));
						if (!listenSocket.AcceptOverlapped(clientAcceptSock, errorText)
							&& WSAGetLastError() != ERROR_IO_PENDING)
						{
							listenSocket.Close();
						}
						else {
							listenSocket.m_isReadOverlapped = true;
						}
					}
				}
			}
			else {
				int index = reinterpret_cast<ui64>((void*)readEvent.lpCompletionKey) - 1;

				if (gameworld.Participants[index].Connected == false) continue;

				if (readEvent.dwNumberOfBytesTransferred <= 0)
				{
					int a = 0;
				}

				if (index >= 0) {
					gameworld.Participants[index].socket.m_isReadOverlapped = false;
					int ec = readEvent.dwNumberOfBytesTransferred;

					if (ec <= 0)
					{
						gameworld.LeaveParticipant(index);
					}
					else {
						//입력처리
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

						//로그출력
						cout << "Participant " << index << " Input : " << cptr[0] << cptr[1] << endl;

						// 다시 수신을 받으려면 overlapped I/O를 걸어야 한다.
						if (gameworld.Participants[index].socket.ReceiveOverlapped() != 0
							&& WSAGetLastError() != ERROR_IO_PENDING)
						{
							gameworld.LeaveParticipant(index);
						}
						else
						{
							// I/O를 걸었다. 완료를 대기하는 중 상태로 바꾸자.
							gameworld.Participants[index].socket.m_isReadOverlapped = true;
						}
					}
				}
			}
		}
	}
	return 0;
}
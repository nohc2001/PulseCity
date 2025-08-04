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

// true�� �Ǹ� ���α׷��� �����մϴ�.
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
			//overlapped.hEvent = FinishEvent[i];  // �̺�Ʈ ��� Ȯ�ο�
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
	// ����ڰ� ctl-c�� ������ ���η����� �����ϰ� ����ϴ�.
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

	cout << "������ ���۵Ǿ����ϴ�.\n";
	cout << "CTL-CŰ�� ������ ���α׷��� �����մϴ�.\n";

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

		// I/O �Ϸ� �̺�Ʈ�� ���� ������ ��ٸ��ϴ�.
		IocpEvents readEvents;
		iocp.Wait(readEvents, 0);

		for (int i = 0; i < readEvents.m_eventCount; i++) {
			auto& readEvent = readEvents.m_events[i];
			if (readEvent.lpCompletionKey == 0) { // is listen socket.
				if (clientAcceptSock.UpdateAcceptContext(listenSocket) != 0)
				{
					//���������� �ݾҴ��� �ϸ� ���⼭ �������Ŵ�. �׷��� �������� �Ҵɻ��·� ������.
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
						//�Է�ó��
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

						//�α����
						cout << "Participant " << index << " Input : " << cptr[0] << cptr[1] << endl;

						// �ٽ� ������ �������� overlapped I/O�� �ɾ�� �Ѵ�.
						if (gameworld.Participants[index].socket.ReceiveOverlapped() != 0
							&& WSAGetLastError() != ERROR_IO_PENDING)
						{
							gameworld.LeaveParticipant(index);
						}
						else
						{
							// I/O�� �ɾ���. �ϷḦ ����ϴ� �� ���·� �ٲ���.
							gameworld.Participants[index].socket.m_isReadOverlapped = true;
						}
					}
				}
			}
		}
	}
	return 0;
}
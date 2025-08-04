//2025.07.29
//multiple access
//time-based movement

#include "stdafx.h"
#include "ImaysNet.h"
#include <iostream>
#include <string>
#include <SDKDDKVer.h>

#include <Ws2tcpip.h>
#include <assert.h>
#include <exception>
#include <sstream>
#include <stdio.h>
#include <tchar.h>
#include <winsock2.h>
#include <cstdint>

#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>

#include <chrono>
#include <thread>
#include <vector>
#include <unordered_map>

using namespace std;
using namespace DirectX;
using namespace DirectX::PackedVector;

using namespace std;

//why used large integer?
//more precise time measurement
LARGE_INTEGER frequency;
LARGE_INTEGER lasttime;

//make player's data structure
struct PlayerData {
	Socket socket;
	XMVECTOR position = XMVectorZero();
	bool connected;
	int speed = 2.0f;
	int MaxP = 16;
	XMVECTOR inputdir = XMVectorZero(); //input direction vector
};

// reduce socket data size
// 플레이어 데이터 저장방식

//used for delta time measurement
//QueryPerformanceFrequency? => calculating tick count per second // tick's speed
//QueryPerformanceCounter? => calculating tick count from start of program // tick's location now
void InitializeTime() {
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&lasttime);
}

float GetDeltaTime() {
	LARGE_INTEGER currentTime;
	QueryPerformanceCounter(&currentTime);
	float deltaTime = (float)(currentTime.QuadPart - lasttime.QuadPart) / frequency.QuadPart;
	lasttime = currentTime;
	return deltaTime;
}


//participants = unordered_map<PlayerData*, shared_ptr<PlayerData>>;
//participantsList = vector<PlayerData*>;
//Participant = participantsList[participantsCount]


int main()
{
	InitializeTime();
	try
	{
		//unordered_map<PlayerData*, shared_ptr<PlayerData>> participants;
		//why change to vector?

		vector<shared_ptr<PlayerData>> participants; //vector to put in valiable participants 

		vector<PollFD> readfd; // vector to identify which socket is i/o possible socket

		Socket listenSocket(SocketType::Tcp);		// 1
		listenSocket.Bind(Endpoint("0.0.0.0", 1000));		// 2

		listenSocket.Listen();					// 3
		listenSocket.SetNonblocking();

		cout << "Server started.\n";
		cout << "Waiting for connection...\n";
		

		while (true)
		{
			float Deltatime = GetDeltaTime();

			readfd.reserve(participants.size() + 1);
			readfd.clear();

			for (size_t i = 0; i < participants.size(); ++i)
			{
				PollFD item;
				item.m_pollfd.events = POLLRDNORM;
				item.m_pollfd.fd = participants[i]->socket.m_fd;
				item.m_pollfd.revents = 0;
				readfd.push_back(item);
			}

			//last item is listen socket
			PollFD item2;
			item2.m_pollfd.events = POLLRDNORM;
			item2.m_pollfd.fd = listenSocket.m_fd;
			item2.m_pollfd.revents = 0;
			readfd.push_back(item2);

			//wait until there is i/o-able socket 
			Poll(readfd.data(), (int)readfd.size(), 100);

			int participantsCount = 0;

			for (auto readfd : readfd) {
				// if this socket is the listen socket
				if (readfd.m_pollfd.revents != 0) {
					//is this a listen socket?
					if (readfd.m_pollfd.fd == listenSocket.m_fd) {
						//accept
						auto newparticipant = make_shared<PlayerData>();

						//already "client connected" status
						string ignore;
						listenSocket.Accept(newparticipant->socket, ignore);
						newparticipant->socket.SetNonblocking();

						auto a = newparticipant->socket.GetPeerAddr().ToString();
						cout << "Socket from " << a << " is accepted.\n";

						//add new participant
						participants.push_back(newparticipant);
						newparticipant->connected = true;
						cout << "Client joined. " << participants.size() << " connections.\n";
					}
					else //if this socket is tcp socket
					{
						//return received data
						auto& Participant = participants[participantsCount];
						int ec = Participant->socket.Receive();
						if (ec <= 0) {
							//error or disconnected socket
							Participant->socket.Close();
							participants.erase(
								remove_if(participants.begin(), participants.end(),
									[](auto& p) { return !p->connected; }),
								participants.end());

							cout << "Client disconnected. " << participants.size() << " connections.\n";
						}
						else // vvvvv location sync handling vvvv
						{
							string receivedData;
							cout << "Receiving data...\n";

							//result for participant

							if (ec == 0)
							{
								cout << "Connection closed.\n";
								Participant->socket.Close();
								break;
							}
							else if (ec < 0)
							{
								cout << "Connect lost: " << GetLastErrorAsString() << endl;
							}

							cout << "Received: " << Participant->socket.m_receiveBuffer << endl;

							char key = Participant->socket.m_receiveBuffer[0];
							char action = Participant->socket.m_receiveBuffer[1];

							switch (key)
							{
							case 'W': Participant->inputdir = XMVectorSet(0, 0, 1, 0);
								break;
							case 'S': Participant->inputdir = XMVectorSet(0, 0, -1, 0);
								break;
							case 'A': Participant->inputdir = XMVectorSet(-1, 0, 0, 0);
								break;
							case 'D': Participant->inputdir = XMVectorSet(1, 0, 0, 0);
								break;
							default:
								break;
							}

							switch (action) {
								//if 'U' ,participant is stop
							case 'U': Participant->inputdir = XMVectorZero();
								break;
								//if 'D' ,participant is moving
							case 'D':
								break;

							}

							//need to update position for every frame

							XMVECTOR deltaMove = XMVectorScale(Participant->inputdir, Participant->speed * Deltatime);

							Participant->position = XMVectorAdd(Participant->position, deltaMove);

						}


					}
				}
				participantsCount++;
			}

			float dt = GetDeltaTime();
			for (auto& P : participants) {
				XMVECTOR delta = XMVectorScale(P->inputdir, P->speed * dt);
				P->position = XMVectorAdd(P->position, delta);
			}

			float worldBuf[16][4] = {};
			for (size_t i = 0; i < participants.size(); ++i) {
				auto& P = participants[i];
				worldBuf[i][0] = XMVectorGetX(P->position);
				worldBuf[i][1] = XMVectorGetY(P->position);
				worldBuf[i][2] = XMVectorGetZ(P->position);
				worldBuf[i][3] = P->connected ? 1.0f : 0.0f;
			}
			for (auto& P : participants) {
				if (P->connected)
					P->socket.Send((char*)worldBuf, sizeof(worldBuf));
			}
		}
	}
	catch (Exception& e)
	{
		cout << "Exception! " << e.what() << endl;
	}
	return 0;
}
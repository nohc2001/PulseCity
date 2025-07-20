#include "stdafx.h"
#include "ImaysNet.h"
#include <iostream>
#include <string>

using namespace std;

int main()
{
	try
	{
		Socket listenSocket(SocketType::Tcp);		// 1
		listenSocket.Bind(Endpoint("0.0.0.0", 5959));		// 2
		listenSocket.Listen();					// 3
		cout << "Server started.\n";
		cout << "Waiting for connection...\n";

		// 4
		Socket tcpConnection;
		string ignore;
		listenSocket.Accept(tcpConnection, ignore);

		// 5
		auto a = tcpConnection.GetPeerAddr().ToString();
		cout << "Socket from " << a << " is accepted.\n";
		
		float x = 0.0f;
		float z = 0.0f;
		const float speed = 1.0f;

		while (true)
		{
			string receivedData;
			cout << "Receiving data...\n";
			int result = tcpConnection.Receive();

			if (result == 0)
			{
				cout << "Connection closed.\n";
				break;
			}
			else if (result < 0)
			{
				cout << "Connect lost: " << GetLastErrorAsString() << endl;
			}

			cout << "Received: " << tcpConnection.m_receiveBuffer << endl;

			char key = tcpConnection.m_receiveBuffer[0];

			switch (key)
			{
			case 'W': z += speed; 
				break;
			case 'S': z -= speed; 
				break;
			case 'A': x -= speed; 
				break;
			case 'D': x += speed; 
				break;
			default: 
				break;
			}

			// x, z 좌표를 클라이언트로 전송
			char buffer[sizeof(float) * 2];
			memcpy(buffer, &x, sizeof(float));
			memcpy(buffer + sizeof(float), &z, sizeof(float));

			tcpConnection.Send(buffer, sizeof(buffer));
		}

		tcpConnection.Close();
	}
	catch (Exception& e)
	{
		cout << "Exception! " << e.what() << endl;
	}
	return 0;
}
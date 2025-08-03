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


			tcpConnection.Send(tcpConnection.m_receiveBuffer, sizeof(tcpConnection.m_receiveBuffer));
		}

		tcpConnection.Close();
	}
	catch (Exception& e)
	{
		cout << "Exception! " << e.what() << endl;
	}
	return 0;
}
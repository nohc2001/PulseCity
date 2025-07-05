#include "stdafx.h"
#include "ImaysNet.h"
#include <iostream>
#include <string>

using namespace std;

void Receiver(Socket& recvmsg) {
	while (true)
	{
		string receivedData;
		cout << "Receiving data...\n";
		int result = recvmsg.Receive();

		if (result == 0)
		{
			cout << "Connection closed.\n";
			break;
		}
		else if (result < 0)
		{
			cout << "Connect lost: " << GetLastErrorAsString() << endl;
		}

		cout << "Received: " << recvmsg.m_receiveBuffer << endl;
	}
}

void Sender(Socket& sendmsg){
	while (true) {
		sendmsg.Send(sendmsg.m_receiveBuffer, strlen(sendmsg.m_receiveBuffer) + 1);
	}
}

int main()
{
	try
	{
		Socket listenSocket(SocketType::Tcp);		// 1
		listenSocket.Bind(Endpoint("0.0.0.0", 5959));		// 2
		listenSocket.Listen();					// 3
		cout << "Server started.\n";

		// 4
		Socket tcpConnection;
		string ignore;
		listenSocket.Accept(tcpConnection, ignore);

		// 5
		auto a = tcpConnection.GetPeerAddr().ToString();
		cout << "Socket from " << a << " is accepted.\n";
		
		thread Receiverthrd(Receiver, ref(tcpConnection));
		thread Senderthrd(Sender, ref(tcpConnection));

		Receiverthrd.join();
		Senderthrd.join();

		tcpConnection.Close();
	}
	catch (Exception& e)
	{
		cout << "Exception! " << e.what() << endl;
	}
	return 0;
}
#include "stdafx.h"
#include "ImaysNet.h"
#include <iostream>
#include <string>
#include <chrono>
#include <thread>

using namespace std;


void Reciever(Socket& recvmsg){
	while (true) {

		string receivedData;
		cout << "Receiving data...\n";
		int result = recvmsg.Receive();

		if (result == 0)
		{
			cout << "Connection closed.\n";
		}
		else if (result < 0)
		{
			cout << "Connect lost: " << GetLastErrorAsString() << endl;
		}
		cout << "Received: " << recvmsg.m_receiveBuffer << endl;
	}
}

void Sender(Socket& sndmsg){

	while(true){
		// �ؽ�Ʈ �Է�
		cout << "���� ���� �Է�\n";
		char* text = new char[100];
		cin >> text;

		sndmsg.Send(text, strlen(text) + 1);
		if (strcmp(sndmsg.m_receiveBuffer, "close") == 0) {
			sndmsg.Close();
			break;
		}
		delete[] text;
	}

}


int main()
{
	try
	{
		Socket s(SocketType::Tcp);	// 1
		s.Bind(Endpoint::Any);		// 2
		cout << "Connecting to server...\n";
		s.Connect(Endpoint("127.0.0.1", 5959)); // 3 <===����ٰ� ���� �����е��� ���ϴ� �ּҸ� �����ʽÿ�. 127.0.0.1�� ���� ��⸦ ���մϴ�.


		std::this_thread::sleep_for(1s);
		
		thread SndThread(Sender, ref(s));
		thread RcvThread(Reciever, ref(s));
		
		SndThread.join();
		RcvThread.join();
		


	}
	catch (Exception& e)
	{
		cout << "Exception! " << e.what() << endl;
	}
	return 0;
}

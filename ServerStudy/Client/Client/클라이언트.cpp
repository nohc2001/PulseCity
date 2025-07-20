#include "ImaysNet.h"
#include <iostream>
#include <string>
#include <chrono>
#include <thread>

using namespace std;

thread_local int a = 0;

int main()
{
	try
	{
		Socket s(SocketType::Tcp);	// 1
		s.Bind(Endpoint::Any);		// 2
		cout << "Connecting to server...\n";
		//s.Connect(Endpoint("112.150.15.111", 1000)); // 3 <===여기다가 실제 여러분들이 원하는 주소를 넣으십시오. 127.0.0.1은 로컬 기기를 뜻합니다.
		s.Connect(Endpoint("127.0.0.1", 5959));

		// 텍스트 입력
		while (true) {
			cout << "보낼 문자 입력\n";
			char* text = new char[100];
			cin >> text;

			s.Send(text, strlen(text) + 1);

			std::this_thread::sleep_for(1s);
			delete[] text;

			string receivedData;
			cout << "Receiving data...\n";
			int result = s.Receive();

			// 7
			if (result == 0)
			{
				cout << "Connection closed.\n";
			}
			else if (result < 0)
			{
				cout << "Connect lost: " << GetLastErrorAsString() << endl;
			}

			// 수신된 데이터를 꺼내서 출력합니다. 송신자는 "hello\0"을 보냈으므로 sz가 있음을 그냥 믿고 출력합니다.
			// (실전에서는 클라이언트가 보낸 데이터는 그냥 믿으면 안됩니다. 그러나 여기는 독자의 이해를 위해 예외로 두겠습니다.)
			cout << "Received: " << s.m_receiveBuffer << endl;

			if (strcmp(s.m_receiveBuffer, "close") == 0) {
				s.Close();
				break;
			}

		}
	}
	catch (Exception& e)
	{
		cout << "Exception! " << e.what() << endl;
	}
	return 0;
}

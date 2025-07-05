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
		s.Connect(Endpoint("127.0.0.1", 5959)); // 3 <===����ٰ� ���� �����е��� ���ϴ� �ּҸ� �����ʽÿ�. 127.0.0.1�� ���� ��⸦ ���մϴ�.

		// �ؽ�Ʈ �Է�
		while (true) {
			cout << "���� ���� �Է�\n";
			char* text = new char[100];
			cin >> text;
			// ���� ���ڿ��� sz�� �����ؼ� �����ؾ� �ϹǷ� +1 �մϴ�.
			s.Send(text, strlen(text) + 1);

			// �� �۽� �Լ��� �����ϴ��� �̰��� ���Ͽ� �۽� �����͸� ���� ���� ��,
			// ���� ������ �����ߴٴ� ���� �ƴմϴ�.
			// �� ���¿��� �ٷ� ���� �ݱ⸦ �� ������ ������ �����͸� ���� ���ϴ� ���ɼ��� �ֽ��ϴ�.
			// ���� �ǵ������� ������ ���� �� �ִ� ����� �ð��� ��ٸ��ϴ�.
			// �� ���� ���α׷��� ������ ���ظ� ���� ���� ��, �������� �������� �̷��� ����� ���� ���� ���� ���Դϴ�.
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

			// ���ŵ� �����͸� ������ ����մϴ�. �۽��ڴ� "hello\0"�� �������Ƿ� sz�� ������ �׳� �ϰ� ����մϴ�.
			// (���������� Ŭ���̾�Ʈ�� ���� �����ʹ� �׳� ������ �ȵ˴ϴ�. �׷��� ����� ������ ���ظ� ���� ���ܷ� �ΰڽ��ϴ�.)
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

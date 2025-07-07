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

#include "ImaysNet.h"
#include <chrono>
#include <thread>
#include <conio.h>
using namespace std;

constexpr unsigned int MAXCHAR_DATA = 4096;
struct ChatRoomDataStruct {
	char ChatRoomData[MAXCHAR_DATA] = {};
	int pivot = 0;
};
ChatRoomDataStruct chatdata;

Socket ParticipantSockets[16] = {};
int participantCount = 0;

char MyName[64] = {};
string Message;
int messageComplete;

void MessageInit() {
	ZeroMemory(&Message[0], MAXCHAR_DATA);
	Message.clear();
	for (int i = 0; i < strlen(MyName); ++i) {
		Message.push_back(MyName[i]);
	}
	Message.push_back(':');
}

int main() {
	Message.reserve(MAXCHAR_DATA);
	cout << "참여할 이름을 입력하세요 : ";
	cin >> MyName;

	Socket s(SocketType::Tcp);	// 1
	
	s.Bind(Endpoint::Any);		// 2
	cout << "Connecting to server...\n";
	s.Connect(Endpoint("127.0.0.1", 5959));
	cout << MyName << "님! 서버에 입장합니다!" << endl;
	s.SetNonblocking();

	MessageInit();
	cout << "Message Input : " << Message << endl;
	while (true) {
		bool isChatUpdate = false;

		int result = s.Receive();
		if (result == 0) {
			break;
		}
		else if (result > 0) {
			isChatUpdate = true;
			memcpy_s(&chatdata, MAXCHAR_DATA+4, s.m_receiveBuffer, MAXCHAR_DATA+4);
		}

		if (isChatUpdate) {
			string str;
			for (int i = chatdata.pivot; i < MAXCHAR_DATA; ++i) {
				str.push_back(chatdata.ChatRoomData[i]);
			}
			for (int i = 0; i < chatdata.pivot; ++i) {
				str.push_back(chatdata.ChatRoomData[i]);
			}
			cout << str << endl;
			cout << "Message Input : " << Message;
		}

		if (_kbhit()) {
			char c = _getch();
			if (c == VK_RETURN) {
				Message.push_back('\n');
				Message.push_back(0);
				s.Send(Message.c_str(), Message.size()+1);
				MessageInit();
				cout << endl;
				isChatUpdate = true;
			}
			else {
				Message.push_back(c);
				cout << c;
			}
		}
	}
	s.Close();
	return 0;
}
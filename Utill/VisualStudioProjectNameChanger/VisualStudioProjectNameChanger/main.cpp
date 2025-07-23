#include <windows.h>
#include <tchar.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <locale>
#include <codecvt>

using namespace std;

HINSTANCE g_hInst;
LPCTSTR lpszClass = L"Window Class Name";
LPCTSTR lpszWindowName = L"Window Programming 1";
HWND hWnd;

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdParam, int nCmdShow) {
	HWND hWnd;
	MSG Message;
	WNDCLASSEX WndClass;
	g_hInst = hInstance;

	WndClass.cbSize = sizeof(WndClass);
	WndClass.style = CS_HREDRAW | CS_VREDRAW;
	WndClass.lpfnWndProc = (WNDPROC)WndProc;
	WndClass.cbClsExtra = 0;
	WndClass.cbWndExtra = 0;
	WndClass.hInstance = hInstance;
	WndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	WndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	WndClass.lpszMenuName = NULL;
	WndClass.lpszClassName = lpszClass;
	WndClass.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	RegisterClassEx(&WndClass);

	hWnd = CreateWindow(lpszClass, lpszWindowName, WS_OVERLAPPEDWINDOW, 0, 0, 600, 400, NULL, (HMENU)NULL, hInstance, NULL);
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	while (GetMessage(&Message, 0, 0, 0)) {
		TranslateMessage(&Message);
		DispatchMessage(&Message);
	}

	return Message.wParam;
}

void func();
void ChangeProjectName(wchar_t* str);

TCHAR str[10][30] = { {} };
int line = 0;
int cnt = 0;
RECT rt;
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	PAINTSTRUCT ps;
	SIZE size;
	HDC hdc;

	switch (uMsg) {
	case WM_CREATE:
		CreateCaret(hWnd, NULL, 5, 15);

		func();
		break;
	case WM_CHAR:
		if (wParam == VK_RETURN || wParam
			 == VK_BACK) {
			break;
		}

		if (cnt + 1 < 30) {
			if (str[line][cnt] == 0) {
				str[line][cnt] = wParam; //--- 문자 저장
				cnt++;
				str[line][cnt] = '\0';
			}
			else {
				str[line][cnt] = wParam; //--- 문자 저장
				cnt++;
			}
		}
		else {
			if (line + 1 < 10) {
				line++;
				cnt = 0;
			}
			else {
				line = 0;
				cnt = 0;
			}
		}

		InvalidateRect(hWnd, NULL, TRUE); //--- 직접 출력하지 않고 WM_PAINT 메시지 발생
		break;
	case WM_KEYDOWN:
		if (wParam == VK_RETURN) {
			ChangeProjectName(str[line]);
			if (line + 1 < 10) {
				line++;
				cnt = 0;
			}
			else {
				line = 0;
				cnt = 0;
			}
		}
		else if (wParam == VK_BACK) {
			cnt -= 1;
			str[line][cnt] = '\0';
		}
		InvalidateRect(hWnd, NULL, TRUE); //--- 직접 출력하지 않고 WM_PAINT 메시지 발생
		break;
	case WM_PAINT:
		GetClientRect(hWnd, &rt);
		hdc = BeginPaint(hWnd, &ps);

		for (int i = 0; i < 10; ++i) {
			TCHAR* tstr = str[i];
			TextOut(hdc, 0, i * 20, tstr, lstrlen(tstr)); //--- 문자 출력
			if (line == i) {
				GetTextExtentPoint32(hdc, tstr, lstrlen(tstr), &size);
			}
		}

		for (int i = 0; i < 10; ++i) {
			TCHAR* tstr = new TCHAR[80];
			lstrcpy(tstr, str[i]);
			tstr[cnt] = L'\0';
			if (line == i) {
				GetTextExtentPoint32(hdc, tstr, lstrlen(tstr), &size);
			}
		}

		SetCaretPos(size.cx, line * 20);
		ShowCaret(hWnd);

		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		DestroyCaret();
		PostQuitMessage(0);
		break;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void RenameFolder(const std::wstring& oldPath, const std::wstring& newPath) {
	// MoveFile 함수를 사용하여 폴더 이름 변경
	bool b = MoveFile(oldPath.c_str(), newPath.c_str());
	if (b == false) {
		DWORD error = GetLastError();
		HRESULT hr = HRESULT_FROM_WIN32(error);
		if (hr == 0x80070020) {
			wchar_t* dest = (wchar_t*)str[line + 1];
			wcscpy_s(dest, 18, L"sharing violation");
		}
		else {
			wchar_t* dest = (wchar_t*)str[line + 1];
			wcscpy_s(dest, 7, L"error");
		}
		line += 1;
		cnt = 0;
	}
}

wstring files[10] = { };
wstring outfiles[10] = { };
wstring projName;
wstring projDirectory;
wstring _vcxproj = L".vcxproj";
wstring _vcxproj_filter = L".vcxproj.filters";
wstring _vcxproj_user = L".vcxproj.user";
void func() {
	OPENFILENAME OFN;
	TCHAR filePathName[256] = L"";
	TCHAR lpstrFile[256] = L"";
	static TCHAR filter[] = L"모든 파일\0*.*";

	memset(&OFN, 0, sizeof(OPENFILENAME));
	OFN.lStructSize = sizeof(OPENFILENAME);
	OFN.hwndOwner = hWnd;
	OFN.lpstrFilter = filter;
	OFN.lpstrFile = lpstrFile;
	OFN.nMaxFile = 256;
	OFN.lpstrInitialDir = L".";

	wchar_t* rstr = nullptr;

	wchar_t text[512] = {};
	GetCurrentDirectoryW(512, text);
	
	if (GetOpenFileName(&OFN) != 0) {
		wsprintf(filePathName, L"%s 파일을 열겠습니까?", OFN.lpstrFile);
		MessageBox(hWnd, filePathName, L"열기 선택", MB_OK);

		wchar_t* str = OFN.lpstrFile;
		int len = (wcslen(str) + 1);
		files[0] = str;

		int dotin = -1;
		int sluchin = -1;
		for (int i = files[0].size() - 1; i >= 0; --i) {
			if (files[0][i] == L'.') dotin = i;
			if (files[0][i] == L'\\') sluchin = i;
			if (dotin >= 0 && sluchin >= 0) {
				projName.clear();
				for (int i = sluchin + 1; i < dotin; ++i) {
					projName.push_back(files[0][i]);
				}
				for (int i = 0; i < sluchin; ++i) {
					projDirectory.push_back(files[0][i]);
				}

				files[1] = (wchar_t*)projDirectory.c_str();
				swap(files[0], files[1]);
				files[2] = projDirectory + L"\\" + projName;
				files[3] = files[2] + L"\\" + projName + _vcxproj;
				files[4] = files[2] + L"\\" + projName + _vcxproj_filter;
				files[5] = files[2] + L"\\" + projName + _vcxproj_user;
				break;
			}
		}

		//rstr = (wchar_t*)fm->_New(sizeof(wchar_t) * len, true);
		//wcscpy_s(rstr, len, str);
		//return rstr;
	}

	SetCurrentDirectoryW(text);
	return;
}

int FindVector(vector<char> str, string f, int offset) {
	int k = 0;
	int r = -1;
	for (int i = offset; i < str.size() - f.size(); ++i) {
		if (str[i] == f[k]) {
			if (k == 0) {
				r = i;
			}

			k++;
			
			if (k == f.size()) {
				return r;
			}
		}
		else {
			if (k != 0) {
				i = r + 1;
			}
			k = 0;
		}
	}
	return -1;
}

void ReplaceVector(vector<char>& str, int index, int size, string f) {
	for (int i = 0; i < size; ++i) {
		str.erase(str.begin() + index);
	}
	for (int i = f.size() - 1; i >= 0; --i) {
		str.insert(str.begin() + index, f[i]);
	}
}

void ChangeProjectName(wchar_t* _str) {
	wstring cfiles[10] = { };
	wstring ccfiles[10] = { };
	std::wstring cproj(_str);
	std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
	std::string cprojstr = conv.to_bytes(cproj);
	std::string projstr = conv.to_bytes(projName);

	int indexArr[5] = { 0, 3, 4, 5 };
	for (int i = 0; i < 6; ++i) {
		cfiles[i] = files[i];
		ccfiles[i] = files[i];
		if(i != 0 && i != 2) {
			ifstream ifs{ files[i] , ios::binary};
			vector<char> FileD;
			if (ifs.is_open()) {
				while (!ifs.eof()) {
					char c = 0;
					ifs.read(&c, 1);
					FileD.push_back(c);
				}
			}

			while (true) {
				int index = FindVector(FileD, projstr.c_str(), 0);
				if (index != -1) {
					ReplaceVector(FileD, index, projstr.size(), cprojstr.c_str());
				}
				else {
					break;
				}
			}
			
			ofstream ofs{ files[i], std::ios::trunc | std::ios::binary };
			if (ofs.is_open()) {
				ofs.write(&FileD[0], FileD.size());
				ofs.close();
			}
		}
	}

	for (int i = 0; i < 6; ++i) {
		int index = ccfiles[i].find(projName.c_str(), 0);
		if (index != wstring::npos) {
			ccfiles[i].replace(index, projName.size(), cproj.c_str());
		}
	}

	//bool b = RenameFolder(files[0], cfiles[0]);
	RenameFolder(cfiles[0], ccfiles[0]);
	for (int i = 1; i < 6; ++i) {
		int index = ccfiles[i].find(projName.c_str(), 0);
		if (index != wstring::npos) {
			ccfiles[i].replace(index, projName.size(), cproj.c_str());
		}

		index = cfiles[i].find(projName.c_str(), 0);
		if (index != wstring::npos) {
			cfiles[i].replace(index, projName.size(), cproj.c_str());
		}
	}

	RenameFolder(cfiles[1], ccfiles[1]);

	RenameFolder(cfiles[2], ccfiles[2]);

	for (int i = 3; i < 6; ++i) {
		int index = ccfiles[i].find(projName.c_str(), 0);
		if (index != wstring::npos) {
			ccfiles[i].replace(index, projName.size(), cproj.c_str());
		}

		index = cfiles[i].find(projName.c_str(), 0);
		if (index != wstring::npos) {
			cfiles[i].replace(index, projName.size(), cproj.c_str());
		}
	}

	RenameFolder(cfiles[3], ccfiles[3]);

	RenameFolder(cfiles[4], ccfiles[4]);

	RenameFolder(cfiles[5], ccfiles[5]);
}

#include <windows.h>
#include <tchar.h>
#include <vector>
#include <time.h>
#include <cmath>   
#include <stack>   
#include <set>     
#include <algorithm> 

using namespace std;

HBRUSH whiteB = CreateSolidBrush(RGB(255, 255, 255));
HBRUSH blackB = CreateSolidBrush(RGB(0, 0, 0));
HBRUSH blueBrush = CreateSolidBrush(RGB(0, 128, 200));
HBRUSH redBrush = CreateSolidBrush(RGB(200, 50, 50));
HBRUSH greenBrush = CreateSolidBrush(RGB(50, 200, 100));

constexpr int gridNum = 50;
HWND hWnd;
float DeltaTime;

// [수정] Pos 구조체를 A*에서도 활용
struct Pos {
    int x, y;
    bool operator<(const Pos& other) const {
        if (x != other.x) return x < other.x;
        return y < other.y;
    }
};

char Grid[gridNum][gridNum] = { {} };

Pos startPos = { 10, 10 }; 
Pos destPos;               
vector<Pos> g_path;       

struct AstarNode {
    int x = -1, y = -1;
    float f = FLT_MAX, g = FLT_MAX, h = FLT_MAX;
    AstarNode* parent = nullptr;
};

// [추가] 8방향 탐색을 위한 배열
const int dx[] = { -1, -1, -1, 0, 0, 1, 1, 1 };
const int dy[] = { -1, 0, 1, -1, 1, -1, 0, 1 };

// [추가] 랜덤 목적지 설정 함수
void SetRandomDestination() {
    do {
        destPos.x = rand() % gridNum;
        destPos.y = rand() % gridNum;
    } while (Grid[destPos.x][destPos.y] == 1 || (destPos.x == startPos.x && destPos.y == startPos.y));
    g_path.clear(); // 목적지가 바뀌면 기존 경로 초기화
}

float Heuristic(int x, int y, int targetX, int targetY) {
    return sqrt(pow(x - targetX, 2) + pow(y - targetY, 2));
}

vector<Pos> TracePath(AstarNode* cellDetails[gridNum][gridNum], Pos dst) {
    stack<Pos> pathStack;
    AstarNode* current = cellDetails[dst.x][dst.y];
    while (current != nullptr) {
        pathStack.push({ current->x, current->y });
        current = current->parent;
    }

    vector<Pos> finalPath;
    while (!pathStack.empty()) {
        finalPath.push_back(pathStack.top());
        pathStack.pop();
    }
    return finalPath;
}

vector<Pos> AstarSearch(Pos src, Pos dst) {
    if (Grid[src.x][src.y] == 1 || Grid[dst.x][dst.y] == 1) {
        return {}; // 시작 또는 끝이 벽이면 빈 경로 반환
    }

    bool closedList[gridNum][gridNum];
    memset(closedList, false, sizeof(closedList));

    AstarNode* cellDetails[gridNum][gridNum];
    for (int i = 0; i < gridNum; ++i) {
        for (int j = 0; j < gridNum; ++j) {
            cellDetails[i][j] = nullptr;
        }
    }

    // 시작 노드 초기화
    cellDetails[src.x][src.y] = new AstarNode();
    cellDetails[src.x][src.y]->x = src.x;
    cellDetails[src.x][src.y]->y = src.y;
    cellDetails[src.x][src.y]->f = 0.0f;
    cellDetails[src.x][src.y]->g = 0.0f;
    cellDetails[src.x][src.y]->h = 0.0f;

    set<pair<float, Pos>> openList;
    openList.insert({ 0.0f, src });

    while (!openList.empty()) {
        auto p = *openList.begin();
        openList.erase(openList.begin());

        int x = p.second.x;
        int y = p.second.y;
        closedList[x][y] = true;

        if (x == dst.x && y == dst.y) {
            vector<Pos> path = TracePath(cellDetails, dst);
            for (int i = 0; i < gridNum; ++i) for (int j = 0; j < gridNum; ++j) if (cellDetails[i][j] != nullptr) delete cellDetails[i][j];
            return path;
        }

        for (int i = 0; i < 8; ++i) {
            int newX = x + dx[i];
            int newY = y + dy[i];

            if (newX >= 0 && newX < gridNum && newY >= 0 && newY < gridNum && Grid[newX][newY] == 0 && !closedList[newX][newY]) {
                float newG = cellDetails[x][y]->g + 1.0f;
                float newH = Heuristic(newX, newY, dst.x, dst.y);
                float newF = newG + newH;

                if (cellDetails[newX][newY] == nullptr || cellDetails[newX][newY]->f > newF) {
                    if (cellDetails[newX][newY] == nullptr) {
                        cellDetails[newX][newY] = new AstarNode();
                    }

                    openList.insert({ newF, {newX, newY} });
                    cellDetails[newX][newY]->x = newX;
                    cellDetails[newX][newY]->y = newY;
                    cellDetails[newX][newY]->f = newF;
                    cellDetails[newX][newY]->g = newG;
                    cellDetails[newX][newY]->h = newH;
                    cellDetails[newX][newY]->parent = cellDetails[x][y];
                }
            }
        }
    }

    // 경로를 찾지 못한 경우
    for (int i = 0; i < gridNum; ++i) for (int j = 0; j < gridNum; ++j) if (cellDetails[i][j] != nullptr) delete cellDetails[i][j];
    return {};
}

void Init() {
    for (int i = 0; i < gridNum; ++i) {
        for (int k = 0; k < gridNum; ++k) {
            Grid[i][k] = 0;
        }
    }
    SetRandomDestination();
}

void Render(HDC hdc) {
    float margin = 700.0f / (float)gridNum;
    for (int xi = 0; xi < gridNum; ++xi) {
        MoveToEx(hdc, (float)xi * margin, 0, NULL);
        LineTo(hdc, (float)xi * margin, 700);
    }
    for (int yi = 0; yi < gridNum; ++yi) {
        MoveToEx(hdc, 0, (float)yi * margin, NULL);
        LineTo(hdc, 700, (float)yi * margin);
    }

    for (const auto& p : g_path) {
        if ((p.x == startPos.x && p.y == startPos.y) || (p.x == destPos.x && p.y == destPos.y)) {
            continue;
        }
        RECT brt;
        brt.left = p.x * margin;
        brt.top = p.y * margin;
        brt.right = p.x * margin + margin;
        brt.bottom = p.y * margin + margin;
        FillRect(hdc, &brt, greenBrush);
    }

    // 시작 유닛 렌더링 (파란색)
    RECT brt;
    brt.left = startPos.x * margin;
    brt.top = startPos.y * margin;
    brt.right = startPos.x * margin + margin;
    brt.bottom = startPos.y * margin + margin;
    FillRect(hdc, &brt, blueBrush);

    // 목적지 유닛 렌더링 (빨간색)
    brt.left = destPos.x * margin;
    brt.top = destPos.y * margin;
    brt.right = destPos.x * margin + margin;
    brt.bottom = destPos.y * margin + margin;
    FillRect(hdc, &brt, redBrush);

    // 벽 렌더링
    for (int ix = 0; ix < gridNum; ++ix) {
        for (int iy = 0; iy < gridNum; ++iy) {
            if (Grid[ix][iy] == 1) {
                brt.left = ix * margin;
                brt.top = iy * margin;
                brt.right = ix * margin + margin;
                brt.bottom = iy * margin + margin;
                FillRect(hdc, &brt, blackB);
            }
        }
    }

    TCHAR scoreLabel[13] = L"A* Algorithm";
    brt = { 0, 350, 100, 400 };


    DrawText(hdc, scoreLabel, 12, &brt, DT_CENTER);
}

void Update() {
    // Update 함수는 현재 사용하지 않음
}

HINSTANCE g_hInst;
LPCTSTR lpszClass = L"BrainTrainer";
LPCTSTR lpszWindowName = L"BrainTrainer";

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);

typedef unsigned long long ui64;
#define QUERYPERFORMANCE_HZ 10000000//Hz
static inline ui64 GetTicks()
{
    LARGE_INTEGER ticks;
    QueryPerformanceCounter(&ticks);
    return ticks.QuadPart;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdParam, int nCmdShow) {
    srand((unsigned int)time(0));
    Init();

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

    hWnd = CreateWindow(lpszClass, lpszWindowName, WS_OVERLAPPEDWINDOW, 0, 0, 700, 700, NULL, (HMENU)NULL, hInstance, NULL);
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    constexpr double MaxFPSFlow = 10000;
    constexpr double InvHZ = 1.0 / (double)QUERYPERFORMANCE_HZ;
    double FPSflow = 0;
    double DeltaFlow = 0;
    ui64 ft = GetTicks();
    while (1) {
        ui64 et = GetTicks();
        double f = (double)(et - ft) * InvHZ;
        FPSflow += f;
        DeltaFlow += f;
        if (::PeekMessage(&Message, NULL, 0, 0, PM_REMOVE))
        {
            if (Message.message == WM_QUIT) break;
            ::TranslateMessage(&Message);
            ::DispatchMessage(&Message);
        }
        else
        {
            if (DeltaFlow >= 0.016) { // limiting fps.
                DeltaTime = (float)DeltaFlow;
                Update();
                InvalidateRect(hWnd, 0, FALSE);
                DeltaFlow = 0;
            }
        }
        ft = et;
    }

    return Message.wParam;
}

RECT rt;
HBITMAP hBit = NULL;
bool lDown = false;
bool rDown = false;
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    PAINTSTRUCT ps;
    HDC hdc;
    HDC memdc;
    switch (uMsg) {
    case WM_CREATE:
        GetClientRect(hWnd, &rt);
        break;
    case WM_PAINT:
    {
        GetClientRect(hWnd, &rt);
        hdc = BeginPaint(hWnd, &ps);
        if (hBit == NULL) {
            hBit = CreateCompatibleBitmap(hdc, rt.right, rt.bottom);
        }
        HDC insDC = CreateCompatibleDC(hdc);
        HBITMAP OldBit = (HBITMAP)SelectObject(insDC, hBit);

        FillRect(insDC, &rt, whiteB);
        Render(insDC);

        SelectObject(insDC, OldBit);
        DeleteDC(insDC);

        memdc = CreateCompatibleDC(hdc);
        OldBit = (HBITMAP)SelectObject(memdc, hBit);
        GetClientRect(hWnd, &rt);
        BitBlt(hdc, 0, 0, rt.right, rt.bottom, memdc, 0, 0, SRCCOPY);
        SelectObject(memdc, OldBit);
        DeleteDC(memdc);
        EndPaint(hWnd, &ps);
    }
    break;
    case WM_CHAR:
        break;
    case WM_KEYDOWN:
        if (wParam == VK_SPACE) {
            g_path = AstarSearch(startPos, destPos);
            InvalidateRect(hWnd, NULL, FALSE);
        }
        if (wParam == 'R') {
            SetRandomDestination();
            InvalidateRect(hWnd, NULL, FALSE);
        }
        break;
    case WM_LBUTTONDOWN:
        lDown = true;
        break;
    case WM_LBUTTONUP:
        lDown = false;
        break;
    case WM_RBUTTONDOWN:
        rDown = true;
        break;
    case WM_RBUTTONUP:
        rDown = false;
        break;
    case WM_MOUSEMOVE:
    {
        float margin = 700.0f / (float)gridNum; 
        if (lDown) {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            int gridX = x / margin;
            int gridY = y / margin;
            if (gridX >= 0 && gridX < gridNum && gridY >= 0 && gridY < gridNum)
                Grid[gridX][gridY] = 1;
        }
        else if (rDown) {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            int gridX = x / margin;
            int gridY = y / margin;
            if (gridX >= 0 && gridX < gridNum && gridY >= 0 && gridY < gridNum)
                Grid[gridX][gridY] = 0;
        }
    }
    break;
    case WM_DESTROY:
        DeleteObject(whiteB);
        DeleteObject(blackB);
        DeleteObject(blueBrush);
        DeleteObject(redBrush);
        DeleteObject(greenBrush);
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
#pragma region AI_Inf_Code

#include <winsock2.h>
#include <mswsock.h>
#include <windows.h>
#include <iostream>
#include <vector>
#include <process.h> // _beginthreadex

// 1. Overlapped 구조체 확장 (중요!)
// 단순 OVERLAPPED는 데이터가 없으므로, 이를 포함하는 사용자 정의 구조체를 만듭니다.
struct OverlappedEx {
    OVERLAPPED overlapped;
    WSABUF wsaBuf;           // 데이터 버퍼 포인터와 길이
    char buffer[1024];       // 실제 데이터가 담길 공간
    int operationType;       // 0: RECV, 1: SEND 등 구분
};

// ai 워커 스레드 함수
unsigned __stdcall WorkerThread(void* pParam) {
    int coreIndex = static_cast<int>(reinterpret_cast<INT_PTR>(pParam));

    // ai 1. 스레드 Affinity 설정 (특정 코어에 고정)
    // ai 1 << coreIndex는 해당 인덱스의 비트만 1로 만듭니다 (예: 0번 코어 -> 0x01, 1번 코어 -> 0x02)
    DWORD_PTR affinityMask = (static_cast<DWORD_PTR>(1) << coreIndex);
    if (SetThreadAffinityMask(GetCurrentThread(), affinityMask) == 0) {
        std::cerr << "Affinity 설정 실패: " << GetLastError() << std::endl;
    }

    // ai 2. 스레드 우선순위 설정 (THREAD_PRIORITY_ABOVE_NORMAL 또는 HIGHEST)
    // ai 게임 로직의 중요도에 따라 설정합니다.
    if (SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL) == 0) {
        std::cerr << "Priority 설정 실패: " << GetLastError() << std::endl;
    }

    std::cout << coreIndex << "번 코어에서 워커 스레드 구동 시작..." << std::endl;
    
    // my 스레드의 설정값을 적용하기 위해 대기상태 잠시 진입.
    Sleep(0);

    // 여기서 GetQueuedCompletionStatus() 호출 루프 진행
    // ...
    // 앞서 논의한 Affinity & Priority 설정
    HANDLE hIOCP = (HANDLE)pParam; // 메인에서 넘겨준 IOCP 핸들

    DWORD bytesTransferred = 0;
    ULONG_PTR completionKey = 0;   // 우리가 등록한 기기/세션 식별자 (보통 Session 객체 포인터)
    LPOVERLAPPED pOverlapped = nullptr;

    while (true) {
        // 2. 완료 큐에서 작업 꺼내기 (핵심 시스템 콜)
        // 작업이 없으면 여기서 스레드가 커널에 의해 잠들고(Sleep), 
        // 데이터가 오면 LIFO 방식으로 가장 효율적인 스레드가 깨어납니다.
        BOOL result = GetQueuedCompletionStatus(
            hIOCP,
            &bytesTransferred,   // 전송된 바이트 수
            &completionKey,      // CreateIoCompletionPort 시 등록한 Key
            &pOverlapped,        // 완료된 I/O의 Overlapped 구조체 주소
            INFINITE             // 작업 올 때까지 무한 대기
        );

        // 3. 예외 상황 처리
        if (result == FALSE) {
            DWORD error = GetLastError();
            if (pOverlapped == nullptr) {
                // IOCP 핸들 자체가 잘못되었거나 심각한 커널 에러
                break;
            }
            else {
                // 클라이언트 강제 종료 (Connection Closed/Aborted)
                // 여기서 세션 정리 로직 실행
                std::cout << "클라이언트 접속 종료 (에러: " << error << ")" << std::endl;
                continue;
            }
        }

        // 정상적인 접속 종료 (Graceful Close)
        if (bytesTransferred == 0) {
            std::cout << "클라이언트가 연결을 정상적으로 닫았습니다." << std::endl;
            // 세션 정리 로직
            continue;
        }

        // 4. 데이터 처리 (비즈니스 로직)
        // pOverlapped 주소를 우리가 만든 확장 구조체 주소로 캐스팅합니다.
        OverlappedEx* pEx = reinterpret_cast<OverlappedEx*>(pOverlapped);

        if (pEx->operationType == 0) { // RECV 완료인 경우
            // pEx->buffer 에 이미 데이터가 들어와 있음 (Zero-copy)
            std::cout << "수신 데이터: " << pEx->buffer << " (" << bytesTransferred << " bytes)" << std::endl;

            // 로직 처리 후 다시 다음 RECV를 걸어줘야 함 (WSARecv 호출)
        }
        else if (pEx->operationType == 1) { // SEND 완료인 경우
            // 보냈다는 확인만 하면 됨 (보통 보낼 데이터 버퍼 해제 등)
        }
    }

    return 0;
}

int main() {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    int numCores = sysInfo.dwNumberOfProcessors;
    std::vector<HANDLE> workerThreads;

    for (int i = 0; i < numCores; ++i) {
        // 스레드 생성 (매개변수로 코어 인덱스 전달)
        HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread,
            reinterpret_cast<void*>(static_cast<INT_PTR>(i)),
            0, NULL);
        if (hThread) {
            workerThreads.push_back(hThread);
        }
    }

    // 스레드 종료 대기 (실제 서버는 여기서 무한 루프)
    WaitForMultipleObjects(static_cast<DWORD>(workerThreads.size()), workerThreads.data(), TRUE, INFINITE);

    for (auto h : workerThreads) CloseHandle(h);
    return 0;
}
#pragma endregion
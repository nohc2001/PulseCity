#pragma once
#include "stdafx.h"

struct Client {
	static constexpr int rbufMax = 8192 - sizeof(int);
	SOCKET sock;
	char rBuf[rbufMax + sizeof(int)] = {};
	int rbufOffset = 0;

	bool Init(const char* ServerIP, unsigned short ServerPort) {
		sock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (sock == INVALID_SOCKET) {
			int wsaErr = WSAGetLastError();
			char _dbg[160] = {};
			sprintf_s(_dbg, "[Client::Init] WSASocket FAILED err=%d ip=\"%s\" port=%u\n", wsaErr, ServerIP ? ServerIP : "(null)", (unsigned)ServerPort);
			OutputDebugStringA(_dbg); printf("%s", _dbg); fflush(stdout);
			return false;
		}

		// Gameplay input packets are tiny and latency-sensitive. Do not let TCP's
		// small-packet coalescing delay key/skill commands behind other traffic.
		BOOL noDelay = TRUE;
		setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
			reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));

		struct sockaddr_in serveraddr;
		memset(&serveraddr, 0, sizeof(serveraddr));
		serveraddr.sin_family = AF_INET;
		int ptonResult = inet_pton(AF_INET, ServerIP, &serveraddr.sin_addr);
		serveraddr.sin_port = htons(ServerPort);
		{
			char _dbg[160] = {};
			sprintf_s(_dbg, "[Client::Init] try connect ip=\"%s\" port=%u pton=%d\n", ServerIP ? ServerIP : "(null)", (unsigned)ServerPort, ptonResult);
			OutputDebugStringA(_dbg); printf("%s", _dbg); fflush(stdout);
		}
		int retval = connect(sock, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
		if (retval == SOCKET_ERROR) {
			int err = WSAGetLastError();
			char _dbg[160] = {};
			sprintf_s(_dbg, "[Client::Init] connect SOCKET_ERROR err=%d\n", err);
			OutputDebugStringA(_dbg); printf("%s", _dbg); fflush(stdout);
			if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS && err != WSAEINVAL) {
				closesocket(sock);
				sock = INVALID_SOCKET;
				return false;
			}
		}
		else {
			char _dbg[80] = {};
			sprintf_s(_dbg, "[Client::Init] connect OK (retval=0)\n");
			OutputDebugStringA(_dbg); printf("%s", _dbg); fflush(stdout);
		}

		u_long val = 1;
		int ret = ioctlsocket(sock, FIONBIO, &val);
		if (ret != 0)
		{
			int wsaErr = WSAGetLastError();
			char _dbg[160] = {};
			sprintf_s(_dbg, "[Client::Init] ioctlsocket FAILED err=%d\n", wsaErr);
			OutputDebugStringA(_dbg); printf("%s", _dbg); fflush(stdout);
			closesocket(sock);
			sock = INVALID_SOCKET;
			return false;
		}

		return true;
	}

	void ResetRecvBuffer() {
		memset(rBuf, 0, sizeof(rBuf));
		rbufOffset = 0;
	}

	void Disconnect() {
		if (sock != INVALID_SOCKET) {
			closesocket(sock);
			sock = INVALID_SOCKET;
		}
		ResetRecvBuffer();
	}

	__forceinline DWORD send(char* data, int len, DWORD flag) {
		WSABUF buf;
		buf.buf = data;
		buf.len = len;
		DWORD retval = 0;
		int err = WSASend(sock, &buf, 1, &retval, flag, NULL, NULL);
		if (err == SOCKET_ERROR) {
			int wsaErr = WSAGetLastError();
			char dbg[128] = {};
			sprintf_s(dbg, "[ClientSend] WSASend failed err=%d len=%d\n", wsaErr, len);
			OutputDebugStringA(dbg);
		}
		return retval;
	}

	DWORD send_all(const char* data, int len, DWORD flag, DWORD timeoutMs = 30) {
		if (sock == INVALID_SOCKET || data == nullptr || len <= 0) return 0;
		DWORD total = 0;
		const ULONGLONG deadline = GetTickCount64() + timeoutMs;
		while (total < (DWORD)len) {
			WSABUF buf;
			buf.buf = const_cast<char*>(data) + total;
			buf.len = (ULONG)(len - total);
			DWORD sent = 0;
			const int result = WSASend(sock, &buf, 1, &sent, flag, NULL, NULL);
			if (result == 0 && sent > 0) {
				total += sent;
				continue;
			}
			const int error = WSAGetLastError();
			if (result != SOCKET_ERROR || error != WSAEWOULDBLOCK || GetTickCount64() >= deadline) break;
			fd_set writeSet;
			FD_ZERO(&writeSet);
			FD_SET(sock, &writeSet);
			timeval wait = {};
			wait.tv_usec = 10000;
			select(0, nullptr, &writeSet, nullptr, &wait);
		}
		return total;
	}

	__forceinline int recv(char* data, int len) {
		WSABUF buf;
		buf.buf = data;
		buf.len = len;
		DWORD retval = 0;
		DWORD flag = 0;
		int err = WSARecv(sock, &buf, 1, &retval, &flag, NULL, NULL);
		if (err == SOCKET_ERROR) {
			return -1;
		}
		return (int)retval;
	}
};

typedef int ItemID;

/*
* ���� : �κ��丮�� �� ������ ���� ����
* Sentinal Value :
* NULL : (ItemCount == 0)
*/
struct ItemStack {
	//������ id
	ItemID id;
	//�������� ����
	int ItemCount;
};

/*
* ���� : ��ӵ� �������� ��Ÿ���� ����ü
* Sentinal Value :
* NULL = (itemDrop.ItemCount == 0)
*/
struct ItemLoot {
	// ������ ���� ����
	ItemStack itemDrop;
	// ������ ��� ��ġ
	// improve : <�������� �ٴ����� �߷��� �ۿ�Ǿ����� ������.>
	vec4 pos;
};

/*
* ������ 2��. 8196 ����Ʈ
*/
struct twoPage {
	char data[8196] = {};
};


/*
* 설명 : 무기 타입 enum
*/
enum class WeaponType { MachineGun, Sniper, Shotgun, Rifle, Pistol, DualPistol, DronePistol, SMG, GrenadeGun, Max };

/*
* 설명 : 무기 타입 구조체
*/
struct WeaponData {
	WeaponType type;
	float shootDelay;     // 연사 속도
	float recoilVelocity; // 반동 세기
	float recoilDelay;    // 반동 회복 시간
	float damage;         // 기본 데미지
	int maxBullets;       // 탄창 용량
	float reloadTime;     // 장전 시간
};

static WeaponData GWeaponTable[] = {
	{ WeaponType::MachineGun, 0.1f, 12.0f, 0.2f, 10.0f, 100, 4.0f },
	{ WeaponType::Sniper, 1.5f, 10.0f, 1.0f, 100.0f, 5, 2.0f },
	{ WeaponType::Shotgun, 0.7f, 7.0f, 0.6f, 12.0f, 8, 3.0f },
	{ WeaponType::Rifle, 0.12f, 10.0f, 0.3f, 15.0f, 30, 2.5f },
	{ WeaponType::Pistol, 0.14f, 7.0f, 0.16f, 15.0f, 30, 1.5f },
	{ WeaponType::DualPistol, 0.50f, 6.0f, 0.22f, 12.0f, 30, 2.2f },
	{ WeaponType::DronePistol, 0.35f, 5.0f, 0.2f, 15.0f, 12, 1.5f },
	{ WeaponType::SMG, 0.09f, 8.0f, 0.15f, 13.0f, 25, 2.0f },
	{ WeaponType::GrenadeGun, 0.80f, 9.0f, 0.5f, 10.0f, 5, 2.4f },
	// 
};

class Weapon {
public:
	WeaponData m_info;      // GWeaponTable에서 가져온 수치
	float m_shootFlow = 0;  // 다음 발사까지 남은 시간 계산
	float m_recoilFlow = 0; // 반동 애니메이션/에임 상승 진행률

	Weapon() {}

	Weapon(WeaponType type) : m_info(GWeaponTable[(int)type]) {
		m_shootFlow = m_info.shootDelay;
		m_recoilFlow = m_info.recoilDelay;
	}

	virtual void Update(float deltaTime) {
		if (m_shootFlow < m_info.shootDelay) m_shootFlow += deltaTime;
		if (m_recoilFlow < m_info.recoilDelay) m_recoilFlow += deltaTime;
	}

	virtual void OnFire() {
		m_shootFlow = 0.0f;
		m_recoilFlow = 0.0f;
	}

	/*
	* 설명
	* 현재 반동이 얼마나 진행되었는지 0~1 사이 값으로 반환
	*/
	float GetRecoilAlpha() const {
		float alpha = 1.0f - (m_recoilFlow / m_info.recoilDelay);
		return (alpha < 0) ? 0 : alpha;
	}
};

#include "../../SyncFPSServer/SyncFPSServer/Protocol.h"

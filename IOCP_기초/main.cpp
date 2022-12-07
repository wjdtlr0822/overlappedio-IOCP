#include <stdio.h>
#include<stdlib.h>
#include <process.h>
#include<algorithm>
#include <WinSock2.h>
#include <list>

#pragma comment(lib,"ws2_32.lib")

#define BUF_SIZE 1024
#define NAME_LEN 20
#define maxClnt 1024

enum class RW_MODE {
	READ,
	WRITE
};

typedef struct {
	SOCKET hClntSocket;
	SOCKADDR_IN clntAddr;
}PER_HANDLE_DATA,*LPPER_HANDLE_DATA;

typedef struct {
	OVERLAPPED overlapped;
	WSABUF wsaBuf;
	char buf[BUF_SIZE];
	RW_MODE rwMode;
	int refCount;
}PER_IO_DATA,*LPPER_IO_DATA;

unsigned int __stdcall IOThreadFunc(void* CompletionIO);
void ErrorHandling(const char* message);

CRITICAL_SECTION crt;
std::list<SOCKET> clientList;

int main() {
	WSADATA wsa;
	HANDLE hComport;
	SYSTEM_INFO sysInfo;
	LPPER_IO_DATA ioInfo;
	LPPER_HANDLE_DATA handleInfo;

	SOCKET hServSock;
	SOCKADDR_IN servAddr;
	DWORD recvBytes, flags = 0;

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		ErrorHandling("WSAStartup() Error");
	}

	InitializeCriticalSection(&crt);

	hComport = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	GetSystemInfo(&sysInfo);
	for (int i = 0; i < sysInfo.dwNumberOfProcessors; i++) {
		_beginthreadex(nullptr, 0, IOThreadFunc, static_cast<void*>(hComport), 0, nullptr);
	}

	hServSock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(8888);
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	bind(hServSock, (SOCKADDR*)&servAddr, sizeof(servAddr));
	listen(hServSock, 5);
	printf("Chatting Server Started\n");

	while (1) {
		SOCKET hClntSock;
		SOCKADDR_IN hClntAddr;
		int addrLen = sizeof(hClntAddr);

		//accept대기
		hClntSock = accept(hServSock, (SOCKADDR*)&hClntAddr, &addrLen);
		handleInfo = (LPPER_HANDLE_DATA)malloc(sizeof(PER_HANDLE_DATA));
		handleInfo->hClntSocket = hClntSock;
		memcpy(&(handleInfo->clntAddr), &hClntAddr, addrLen);

		//Send할 때 client list를 순회하므로, 동기화
		EnterCriticalSection(&crt);
		clientList.push_back(hClntSock);
		printf("Connect Client : %d\n", clientList.size());
		LeaveCriticalSection(&crt);

		//CompletionPort를 소켓과 연결, 핸들 정보가 전달 된다
		CreateIoCompletionPort((HANDLE)hClntSock, hComport, (DWORD)handleInfo, 0);

		//io정보 생성하고 recvice
		ioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
		memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
		ioInfo->wsaBuf.len = BUF_SIZE;
		ioInfo->wsaBuf.buf = ioInfo->buf;
		ioInfo->rwMode = RW_MODE::READ;
		ioInfo->refCount = 0;

		WSARecv(handleInfo->hClntSocket, &(ioInfo->wsaBuf), 1, &recvBytes, &flags, &(ioInfo->overlapped), NULL);
	}

	DeleteCriticalSection(&crt);
	return 0;
}


unsigned int __stdcall IOThreadFunc(void* CompletionIO) {
	HANDLE hComPort = (HANDLE)CompletionIO;
	SOCKET sock;
	DWORD byteTrans;
	LPPER_HANDLE_DATA handleInfo;
	LPPER_IO_DATA ioInfo;
	DWORD flags = 0;

	while (1) {
		GetQueuedCompletionStatus(hComPort, &byteTrans, (LPDWORD)&handleInfo, (LPOVERLAPPED*)&ioInfo, INFINITE);

		sock = handleInfo->hClntSocket;

		if (ioInfo->rwMode == RW_MODE::READ) {
			printf("Message Received\n");

			if (byteTrans == 0) {
				//연결이 끊긴 경우 client list에 접근해야 하기 때문에 동기화
				EnterCriticalSection(&crt);
				clientList.erase(std::find(clientList.begin(), clientList.end(), sock));
				printf("EXIT Client : Remain Client :%d \n", clientList.size());
				free(handleInfo);
				free(ioInfo);
				LeaveCriticalSection(&crt);
				continue;
			}

//			memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
			ZeroMemory(&(ioInfo->overlapped), sizeof(OVERLAPPED));
			ioInfo->wsaBuf.len = byteTrans;

			//client list 순회하는 도중 list의 원소가 삭제되어서는 안되기 때문에 동기화
			EnterCriticalSection(&crt);
			ioInfo->rwMode = RW_MODE::WRITE;
			for (auto& e : clientList) {
				++ioInfo->refCount;
				WSASend(e, (&ioInfo->wsaBuf), 1, NULL, 0, &(ioInfo->overlapped), NULL);
			}
			LeaveCriticalSection(&crt);

			ioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
			memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
			ioInfo->wsaBuf.len = BUF_SIZE;
			ioInfo->wsaBuf.buf = ioInfo->buf;
			ioInfo->rwMode = RW_MODE::READ;
			ioInfo->refCount = 0;
			WSARecv(sock, &(ioInfo->wsaBuf), 1, NULL, &flags, &(ioInfo->overlapped), NULL);
		}
		else {
			EnterCriticalSection(&crt);
			--ioInfo->refCount;
			LeaveCriticalSection(&crt);

			if (ioInfo->refCount <= 0) {
				printf("Message Sent to %d Players\n", clientList.size());
				free(ioInfo);
			}
		}
	}
}

void ErrorHandling(const char* message) {
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}

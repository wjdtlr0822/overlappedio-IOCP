#include <WinSock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <process.h>

#define BUF_SIZE 1024
#pragma comment(lib,"ws2_32.lib")

using namespace std;

typedef struct {
	SOCKET hClntSock;
	SOCKADDR_IN clntAddr;
}PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

typedef struct {
	OVERLAPPED overlapped;
	char buffer[BUF_SIZE];
	WSABUF wsabuf;
}PER_IO_DATA,*LPPER_IO_DATA;

unsigned int __stdcall CompletionThread(LPVOID pComport);
void ErrorHandling(const char* message);


int main() {
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		ErrorHandling("WSAStartup() Error");
	}

	HANDLE hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);

	for (int i = 0; i < systemInfo.dwNumberOfProcessors; i++) {
		_beginthreadex(NULL, 0, CompletionThread,(LPVOID)hCompletionPort, 0, NULL);
	}

	SOCKET hServSock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	SOCKADDR_IN servAddr;
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(8888);
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	bind(hServSock, (SOCKADDR*)&servAddr, sizeof(servAddr));
	listen(hServSock, 5);

	LPPER_IO_DATA PerIoData;
	LPPER_HANDLE_DATA PerHandleData;

	int RecvBytes;
	int i, Flags;

	while (1) {
		SOCKADDR_IN clntAddr;
		int addrLen = sizeof(clntAddr);

		SOCKET hClntSock = accept(hServSock, (SOCKADDR*)&clntAddr, &addrLen);

		PerHandleData = (LPPER_HANDLE_DATA)malloc(sizeof(PER_HANDLE_DATA));
		memset(&PerIoData, 0, sizeof(PER_IO_DATA));
		PerIoData->wsabuf.len = BUF_SIZE;
		PerIoData->wsabuf.buf = PerIoData->buffer;
		Flags = 0;

		WSARecv(PerHandleData->hClntSock, &(PerIoData->wsabuf), 1, (LPDWORD)&RecvBytes, (LPDWORD) & Flags, &(PerIoData->overlapped), NULL);
	}
	return 0;
}

unsigned int __stdcall CompletionThread(LPVOID pComPort) {
	HANDLE hCompletionPort = (HANDLE)pComPort;
	DWORD BytesTransferred;
	LPPER_HANDLE_DATA PerHandleData;
	LPPER_IO_DATA PerIoData;
	DWORD flags;

	while (1) {
		GetQueuedCompletionStatus(hCompletionPort, &BytesTransferred, (PULONG_PTR) & PerHandleData, (LPOVERLAPPED*) & PerIoData, INFINITE);

		if (BytesTransferred == 0) {
			closesocket(PerHandleData->hClntSock);
			free(PerHandleData);
			free(PerIoData);
			continue;
		}

		PerIoData->wsabuf.buf[BytesTransferred] = '\0';
		printf("Recv[%s]\n", PerIoData->wsabuf.buf);

		PerIoData->wsabuf.len = BytesTransferred;
		WSASend(PerHandleData->hClntSock, &(PerIoData->wsabuf), 1, NULL, 0, NULL, NULL);
		
		memset(&(PerIoData->overlapped), 0, sizeof(OVERLAPPED));
		PerIoData->wsabuf.len = BUF_SIZE;
		PerIoData->wsabuf.buf = PerIoData->buffer;

		flags = 0;

		WSARecv(PerHandleData->hClntSock, &(PerIoData->wsabuf), 1, NULL, &flags, &(PerIoData->overlapped), NULL);
	}
	return 0;
}

void ErrorHandling(const char* message) {
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}
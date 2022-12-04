#include <stdio.h>
#include<stdlib.h>
#include <WinSock2.h>

void ErrorHandling(const char* message);

#pragma comment(lib,"ws2_32.lib")

int main() {
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		ErrorHandling("WSAStartup() Error");
	}

	SOCKET hSocket = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	if (hSocket == INVALID_SOCKET) {
		ErrorHandling("SOCKET() Error");
	}

	SOCKADDR_IN recvAddr;
	recvAddr.sin_family = AF_INET;
	recvAddr.sin_port = htons(8888);
	recvAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	if (connect(hSocket, (SOCKADDR*)&recvAddr, sizeof(recvAddr)) == SOCKET_ERROR) {
		ErrorHandling("Connect() Error");
	}

	WSAEVENT event = WSACreateEvent();

	WSAOVERLAPPED overlapped;
	memset(&overlapped, 0, sizeof(overlapped));
	overlapped.hEvent = event;

	WSABUF dataBuf;
	char message[1024] = { 0, };
	int sendBytes = 0;
	int recvBytes = 0;
	int flags = 0;

	while (1) {
		flags = 0;

		printf("send :");
		scanf_s("%s", message);

		dataBuf.len = strlen(message);
		dataBuf.buf = message;

		if (WSASend(hSocket, &dataBuf, 1, (LPDWORD)&sendBytes, flags, &overlapped, NULL) == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				ErrorHandling("WSASend() Error");
			}
		}

		WSAWaitForMultipleEvents(1, &event, TRUE, WSA_INFINITE, FALSE);
		WSAGetOverlappedResult(hSocket, &overlapped, (LPDWORD)&sendBytes, FALSE, NULL);

		printf("send byte : %d \n", sendBytes);

		if (WSARecv(hSocket, &dataBuf, 1, (LPDWORD)&recvBytes, (LPDWORD)&flags, &overlapped, NULL) == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				ErrorHandling("Recv() Error");
			}
		}
		printf("Recv[%s]\n", dataBuf.buf);
	}
	closesocket(hSocket);
	WSACleanup();
	return 0;
}


void ErrorHandling(const char* message) {
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}
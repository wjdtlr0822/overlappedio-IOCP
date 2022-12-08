#include <stdio.h>
#include <process.h>
#include <WinSock2.h>
#include <stdio.h>
#pragma comment(lib,"ws2_32.lib")

using namespace std;

void ErrorHandling(const char* message);
unsigned WINAPI SendMSG(void* arg);
unsigned WINAPI RecvMSG(void* arg);

int main() {
	WSADATA wsa;
	SOCKET servSock;
	SOCKADDR_IN servAddr;
	HANDLE _send, _recv;

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		ErrorHandling("WSAStartup Error");
	}


	servSock = socket(PF_INET, SOCK_STREAM, 0);

	if (servSock == INVALID_SOCKET) ErrorHandling("socket() Error");

	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(8888);
	servAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	if (SOCKET_ERROR == connect(servSock, (SOCKADDR*)&servAddr, sizeof(servAddr))) {
		ErrorHandling("connect() Error");
	}

	printf("connect Success\n");

	_send = (HANDLE)_beginthreadex(NULL, 0, SendMSG, (void*)&servSock, 0, NULL);
	_recv = (HANDLE)_beginthreadex(NULL, 0, RecvMSG, (void*)&servSock, 0, NULL);

	WaitForSingleObject(_send, INFINITE);
	WaitForSingleObject(_recv, INFINITE);

	closesocket(servSock);
	WSACleanup();
	return 0;
}


unsigned WINAPI SendMSG(void* arg) {
	SOCKET hSock = *((SOCKET*)arg);
	char buf[1024];

	while (1) {
		fgets(buf, 1024, stdin);
		send(hSock, buf, sizeof(buf), 0);
		printf("size : %d\n", sizeof(buf));
	}
}

unsigned WINAPI RecvMSG(void* arg) {
	SOCKET hSock = *((SOCKET*)arg);
	char buf[1024];
	int len;

	while (1) {
		len = recv(hSock, buf, 1024, 0);
		if (len == -1) return -1;
		printf("%s\n", buf);
	}
}

void ErrorHandling(const char* message) {
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}
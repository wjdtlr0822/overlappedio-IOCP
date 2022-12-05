#include <stdio.h>
#include<stdlib.h>
#include<cstring>
#include<process.h>

#include<WinSock2.h>
#include <iostream>
#pragma comment(lib,"ws2_32.lib")

#define BUF_SIZE 1024
#define NAME_LEN 20

void ErrorHandling(const char* message);

unsigned WINAPI SendMsg(void* arg);
unsigned WINAPI RecvMsg(void* arg);

char message[BUF_SIZE];
char name[NAME_LEN]="CLIENT";

int main() {
	WSADATA wsa;
	SOCKET hSock;
	SOCKADDR_IN servAddr;
	int strLen, readLen;
	HANDLE hSendThread, hRecvThread;

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		ErrorHandling("WSAStartup() Error");
	}
	hSock = socket(PF_INET, SOCK_STREAM, 0);

	if (INVALID_SOCKET == hSock) {
		ErrorHandling("SOCKET() Error");
	}

	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(8888);
	servAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	if (SOCKET_ERROR == connect(hSock, (SOCKADDR*)&servAddr, sizeof(servAddr))) {
		ErrorHandling("Connect() Error");
	}
	else {
		std::cin.getline(name, NAME_LEN);
		printf("Connected as CLIENT...\n");
	}

	hSendThread = (HANDLE)_beginthreadex(NULL, 0, SendMsg, (void*)&hSock, 0, NULL);
	hRecvThread = (HANDLE)_beginthreadex(NULL, 0, RecvMsg, (void*)&hSock, 0, NULL);

	WaitForSingleObject(hSendThread, INFINITE);
	WaitForSingleObject(hRecvThread, INFINITE);

	closesocket(hSock);
	WSACleanup();
	return 0;

}

unsigned WINAPI SendMsg(void* arg) {
	SOCKET hSock = *((SOCKET*)arg);
	char fullMsg[NAME_LEN + BUF_SIZE];

	while (1) {
		fgets(message, BUF_SIZE, stdin);

		if (!strcmp(message, "q\n") || !strcmp(message, "quit\n")) {

			closesocket(hSock);
			return 0;
		}
		sprintf_s(fullMsg, "%s : %s ",name,message);
		send(hSock, fullMsg, strlen(fullMsg), 0);
	}
	return 0;
}

unsigned WINAPI RecvMsg(void* arg) {
	SOCKET hSock = *((SOCKET*)arg);
	char fullMsg[NAME_LEN + BUF_SIZE];
	int len;

	while (1) {
		len = recv(hSock, fullMsg, NAME_LEN + BUF_SIZE, 0);

		if (-1 == len) {
			return -1;
		}
		fullMsg[len] = '\0';
		fputs(fullMsg, stdout);
	}
	return 0;
}


void ErrorHandling(const char* message) {
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}
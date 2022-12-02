

//�����۾� �ʼ�!!!!
// IOCP(I/O Completion Port)

/*-IOCP������ �Ϸ�� IO�� ������ CP������Ʈ��� Ŀ�� ������Ʈ�� ��ϵȴ�.

- ������ CP������Ʈ�� ����� �س����� CP������Ʈ���� IO �Ϸῡ ���� ������ ���ش�.



CP ������Ʈ�� IO�� �Ϸᰡ ��ϵ� �� �ֵ��� ���� �� ������ ����Ǿ�� �Ѵ�.



1. Completion Port ������Ʈ�� ����

2. Completion Port ������Ʈ�� IO�� �ϷḦ ����� ���ϰ��� ����





/****************************************************************************/
/*���� �� ���� �۾��� ���ؼ� ���ǵ� �Լ��� CreationIOCompletionPort�̴�.* /

//CreationIOCompletionPort�� CP������Ʈ ������ CP������Ʈ�� ���ϰ��� ������ �������� ���

//cp object ����
/*
HANDLE hCpObject;
hCpObject = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0); */

//���� ����
/*SOCKET sock;
CreateIoCompletionPort((HANDLE)sock, hCpObject, (DWORD)ioInfo, 0);*/

/****************************************************************************/

/*
GetQueuedCompletionStatus() // ������ TRUE ���н� FALSE;
GetQueuedCompletionStatus( HANDLE, ipnumberOfBytes, 3��° ����, overlapped, timeout
*/

#include <iostream>
#include <WinSock2.h>
#include <process.h>
#include <stdlib.h>

#pragma comment(lib,"ws2_32.lib")

using namespace std;

#define BUF_SIZE 1024
#define READ 3
#define WRITE 5

typedef struct {
	SOCKET clntsock;
	SOCKADDR_IN clntAddr;
}PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

typedef struct {
	OVERLAPPED overlapped;
	WSABUF wsaBuf;
	char buffer[BUF_SIZE];
	int rwMode;
}PER_IO_DATA, *LPPER_IO_DATA;

unsigned int WINAPI EchoThreadMain(void* CompletionPortIO);
void ErrorHandling(const char* message);

int main() {
	WSADATA   		  wsaData;
	HANDLE			  hComPort;
	SYSTEM_INFO		  sysInfo;
	LPPER_IO_DATA	  ioInfo;
	LPPER_HANDLE_DATA handleInfo;

	SOCKET			  servSock;
	SOCKADDR_IN		  servAddr;
	DWORD			  recvBytes, i, flags = 0;


	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		ErrorHandling("WSAStartup Error");
	}
	hComPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	GetSystemInfo(&sysInfo);


	// dwNumberOfProcessors : �ھ��
// ��Ŀ ������ ����
// �ھ�� * 2�� IOCP���� ���� �̻����� Woker thread ����
	for (i = 0; i < sysInfo.dwNumberOfProcessors; i++) {
		_beginthreadex(NULL, 0, EchoThreadMain, (LPVOID)hComPort, 0,NULL);
	}

	servSock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(8888);
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	bind(servSock, (SOCKADDR*)&servAddr, sizeof(servAddr));
	listen(servSock, 5);

	while (1) {
		SOCKET clntSock;
		SOCKADDR_IN clntAddr;

		int addrLen = sizeof(clntAddr);

		clntSock = accept(servSock, (SOCKADDR*)&clntAddr, &addrLen);

		//����� ����
		handleInfo = (LPPER_HANDLE_DATA)malloc(sizeof(PER_HANDLE_DATA));
		handleInfo->clntsock = clntSock;

		memcpy(&(handleInfo->clntAddr), &clntAddr, addrLen);

		ioInfo->wsaBuf.len = BUF_SIZE;
		ioInfo->wsaBuf.buf = ioInfo->buffer;
		ioInfo->rwMode = READ;

		WSARecv(handleInfo->clntsock, &(ioInfo->wsaBuf), 1, &recvBytes, &flags, &(ioInfo->overlapped), NULL);
	}
	return 0;
}


unsigned int WINAPI EchoThreadMain(void* pComPort) {
	HANDLE				hComPort = (HANDLE)pComPort;
	SOCKET				sock;
	DWORD				bytesTrans;
	LPPER_HANDLE_DATA	handleInfo;
	LPPER_IO_DATA		ioInfo;
	DWORD				flags = 0;


	while (1) {
		// IOCP ť���� �Ϸ� ���¸� �޾ƿ´�
		// 1. �޾ƿ� IOCP �ڵ�
		// 2. �ۼ��� ����Ʈ ��
		// 3. IOCP ��Ͻ� ����Ű��
		// 4. OVERLAPPED ����ü
		// 5. ���ð�
		GetQueuedCompletionStatus(hComPort, &bytesTrans, (LPDWORD)&handleInfo, (LPOVERLAPPED*)&ioInfo, INFINITE);
		sock = handleInfo->clntsock;

		if (ioInfo->rwMode == READ) //���ſϷ�
		{
			puts("message Received!!");
			if (bytesTrans == 0) {
				closesocket(sock);
				free(handleInfo);
				free(ioInfo);
				continue;
			}

			//���� ����Ʈ��ŭ ����
			// ���� ���� overlapped �ʱ�ȭ
			memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
			ioInfo->wsaBuf.len = bytesTrans; //���� ����Ʈ ��ŭ ���� ���� ����
			ioInfo->rwMode = WRITE; // send���·� �ٲ�

			WSASend(sock, &(ioInfo->wsaBuf), 1, NULL, 0, &(ioInfo->overlapped), NULL);


			//����� ������ ���� ���·�
			ioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
			memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
			ioInfo->wsaBuf.len = BUF_SIZE;
			ioInfo->wsaBuf.buf = ioInfo->buffer;
			ioInfo->rwMode = READ;

			WSARecv(sock, &(ioInfo->wsaBuf), 1, NULL, &flags, &(ioInfo->overlapped), NULL);
		}
		else {//�۽ſϷ�
			puts("Message sent!!");
			free(ioInfo);
		}
		return 0;
	}

}
void ErrorHandling(const char* message) {
	cout << message << "-" << WSAGetLastError() << endl;
}
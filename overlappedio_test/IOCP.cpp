

//선행작업 필수!!!!
// IOCP(I/O Completion Port)

/*-IOCP에서는 완료된 IO의 정보가 CP오브젝트라는 커널 오브젝트에 등록된다.

- 소켓을 CP오브젝트에 등록을 해놓으면 CP오브젝트에서 IO 완료에 대한 관리를 해준다.



CP 오브젝트에 IO의 완료가 등록될 수 있도록 다음 두 가지가 선행되어야 한다.



1. Completion Port 오브젝트의 생성

2. Completion Port 오브젝트와 IO의 완료를 등록할 소켓과의 연결





/****************************************************************************/
/*위의 두 가지 작업을 위해서 정의된 함수가 CreationIOCompletionPort이다.* /

//CreationIOCompletionPort는 CP오브젝트 생성과 CP오브젝트와 소켓과의 연결을 목적으로 사용

//cp object 생성
/*
HANDLE hCpObject;
hCpObject = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0); */

//소켓 지정
/*SOCKET sock;
CreateIoCompletionPort((HANDLE)sock, hCpObject, (DWORD)ioInfo, 0);*/

/****************************************************************************/

/*
GetQueuedCompletionStatus() // 성공시 TRUE 실패시 FALSE;
GetQueuedCompletionStatus( HANDLE, ipnumberOfBytes, 3번째 인자, overlapped, timeout
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


	// dwNumberOfProcessors : 코어갯수
// 워커 쓰레드 생성
// 코어갯수 * 2가 IOCP에서 가장 이상적인 Woker thread 갯수
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

		//사용자 정보
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
		// IOCP 큐에서 완료 상태를 받아온다
		// 1. 받아올 IOCP 핸들
		// 2. 송수신 바이트 수
		// 3. IOCP 등록시 고유키값
		// 4. OVERLAPPED 구조체
		// 5. 대기시간
		GetQueuedCompletionStatus(hComPort, &bytesTrans, (LPDWORD)&handleInfo, (LPOVERLAPPED*)&ioInfo, INFINITE);
		sock = handleInfo->clntsock;

		if (ioInfo->rwMode == READ) //수신완료
		{
			puts("message Received!!");
			if (bytesTrans == 0) {
				closesocket(sock);
				free(handleInfo);
				free(ioInfo);
				continue;
			}

			//받은 바이트만큼 전송
			// 재사용 위해 overlapped 초기화
			memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
			ioInfo->wsaBuf.len = bytesTrans; //받은 바이트 만큼 버퍼 길이 지정
			ioInfo->rwMode = WRITE; // send상태로 바꿈

			WSASend(sock, &(ioInfo->wsaBuf), 1, NULL, 0, &(ioInfo->overlapped), NULL);


			//사용자 소켓을 수신 상태로
			ioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
			memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
			ioInfo->wsaBuf.len = BUF_SIZE;
			ioInfo->wsaBuf.buf = ioInfo->buffer;
			ioInfo->rwMode = READ;

			WSARecv(sock, &(ioInfo->wsaBuf), 1, NULL, &flags, &(ioInfo->overlapped), NULL);
		}
		else {//송신완료
			puts("Message sent!!");
			free(ioInfo);
		}
		return 0;
	}

}
void ErrorHandling(const char* message) {
	cout << message << "-" << WSAGetLastError() << endl;
}
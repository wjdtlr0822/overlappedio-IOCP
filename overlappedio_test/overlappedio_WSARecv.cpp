#include <iostream>
#include <WinSock2.h>

#define BUF_SIZE 1024
using namespace std;

int main() {
	WSADATA wsa;
	SOCKET serverSock, clientSock;
	SOCKADDR_IN servAddr, clntAddr;
	int recvAdrSz;
	WSABUF dataBuf;
	WSAEVENT evObj;
	WSAOVERLAPPED overlapped;

	char buf[BUF_SIZE];
	DWORD recvBytes = 0, flag = 0;


	WSAStartup(MAKEWORD(2, 2), &wsa);
	serverSock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(8888);
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	bind(serverSock, (SOCKADDR*)&servAddr, sizeof(servAddr));
	listen(serverSock, SOMAXCONN);


	int clntSz = sizeof(clntAddr);
	clientSock = accept(serverSock, (SOCKADDR*)&clntAddr, &clntSz);

	evObj = WSACreateEvent();
	memset(&overlapped, 0, sizeof(overlapped));
	overlapped.hEvent = evObj;
	dataBuf.len = BUF_SIZE;
	dataBuf.buf = buf;

	if (WSARecv(clientSock, &dataBuf, 1, &recvBytes, &flag, &overlapped, NULL) == SOCKET_ERROR) {
		if (WSAGetLastError() == WSA_IO_PENDING) {
			puts("background data recv");
			//WSA이벤트를 기다린다. 전송 완료를 기다림
			//overlapped 구조체에 등록된 이벤트 오브젝트가 signaled가 되면 반환
			WSAWaitForMultipleEvents(1, &evObj, TRUE, WSA_INFINITE, FALSE);
			
			//결과값을 받아온다. 완료된 바이트도 확인할 수 있음.
			WSAGetOverlappedResult(clientSock, &overlapped, &recvBytes, FALSE, NULL);
			// Event를 사용하지 않고 4번쨰 인자를 TRUE로 하면 Event를 사용하지 않고 기다릴 수 있다.
		}
		else {
			cout << "WSARecv() Error" << endl;
		}
	}

	cout << "받은 값  : " << buf << endl;

	WSACloseEvent(evObj);
	closesocket(clientSock);
	closesocket(serverSock);
	WSACleanup();

	return 0;
}
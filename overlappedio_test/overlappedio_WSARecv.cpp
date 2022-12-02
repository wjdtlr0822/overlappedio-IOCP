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
			//WSA�̺�Ʈ�� ��ٸ���. ���� �ϷḦ ��ٸ�
			//overlapped ����ü�� ��ϵ� �̺�Ʈ ������Ʈ�� signaled�� �Ǹ� ��ȯ
			WSAWaitForMultipleEvents(1, &evObj, TRUE, WSA_INFINITE, FALSE);
			
			//������� �޾ƿ´�. �Ϸ�� ����Ʈ�� Ȯ���� �� ����.
			WSAGetOverlappedResult(clientSock, &overlapped, &recvBytes, FALSE, NULL);
			// Event�� ������� �ʰ� 4���� ���ڸ� TRUE�� �ϸ� Event�� ������� �ʰ� ��ٸ� �� �ִ�.
		}
		else {
			cout << "WSARecv() Error" << endl;
		}
	}

	cout << "���� ��  : " << buf << endl;

	WSACloseEvent(evObj);
	closesocket(clientSock);
	closesocket(serverSock);
	WSACleanup();

	return 0;
}
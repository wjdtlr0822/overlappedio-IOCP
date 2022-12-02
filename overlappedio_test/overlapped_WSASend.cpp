#include <WinSock2.h>
#include <iostream>

#define BUF_SIZE 1024

#pragma comment(lib,"ws2_32.lib")


using namespace std;

int main() {
	WSADATA wsa;
	SOCKET clnt_sock;
	SOCKADDR_IN clnt_addr;
	WSAEVENT evObj;
	WSAOVERLAPPED overlapped;
	WSABUF dataBuf;

	char buf[BUF_SIZE];
	DWORD sendBytes = 0;

	WSAStartup(MAKEWORD(2, 2), &wsa);
	clnt_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	memset(&clnt_addr, 0, sizeof(clnt_addr));
	clnt_addr.sin_family = AF_INET;
	clnt_addr.sin_port = htons(8888);
	clnt_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (connect(clnt_sock, (SOCKADDR*)&clnt_addr, sizeof(clnt_addr)) == SOCKET_ERROR) {
		cout << "connect error" << endl;
	}

	memset(&overlapped, 0, sizeof(overlapped));
	evObj = WSACreateEvent();
	overlapped.hEvent = evObj;
	dataBuf.len = BUF_SIZE;
	dataBuf.buf = buf;

	if (WSASend(clnt_sock, &dataBuf, 1, &sendBytes, 0, &overlapped, NULL) == SOCKET_ERROR) {
		if (WSAGetLastError() == WSA_IO_PENDING) {
			puts("background data send");
			WSAWaitForMultipleEvents(1, &evObj, TRUE, WSA_INFINITE, FALSE);
			WSAGetOverlappedResult(clnt_sock, &overlapped, &sendBytes, FALSE, NULL);
		}
		else {
			cout << "WSASend() error" << endl;
		}
	}

	cout << "send data size : " << sendBytes << endl;

	WSACloseEvent(evObj);
	closesocket(clnt_sock);
	WSACleanup();
}
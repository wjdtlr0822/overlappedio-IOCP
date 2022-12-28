#pragma once
#include <WinSock2.h>
#include <WS2tcpip.h>

const UINT32 MAX_SOCKBUF = 256;
const UINT32 MAX_SOCK_SENDBUF = 4096;
const UINT32 MAX_WORKERTHREAD = 4;

enum class IOOperation {
	RECV,
	SEND
};

struct stOverlappedEx {
	WSAOVERLAPPED m_wsaOverlapped;
	SOCKET		  m_socketClient;
	WSABUF		  m_wsaBuf;
	IOOperation	  m_eOperation;
};
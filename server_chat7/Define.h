#pragma once

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <MSWSock.h>

const UINT32 MAX_SOCK_RECVBUF = 256;
const UINT32 MAX_SOCK_SENDBUF = 4096;
const UINT32 RE_USE_SESSION_WAIT_TIMESEC = 3;


enum class IOOPeration {
	RECV,
	SEND
};

struct stOverlappedEx {
	WSAOVERLAPPED m_wsaOverlapped;
	WSABUF m_wsaBuf;
	IOOPeration m_eOperation;
	UINT32 SessionIndex = 0;
};
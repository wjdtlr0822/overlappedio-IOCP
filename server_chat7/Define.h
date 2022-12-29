#pragma once

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <MSWSock.h>

const UINT32 MAX_SOCK_RECVBUF = 256;
const UINT32 MAX_SOCK_SENDBUF = 4096;
const UINT64 RE_USE_SESSION_WAIT_TIMESEc = 3;

enum class IOOperation
{
	ACCEPT,
	RECV,
	SEND
};

struct stOverlappedEx {
	WSAOVERLAPPED m_wsaOverlapped;
	WSABUF m_wsaBuf;
	IOOperation m_eOperation;
	UINT32 SessionIndex = 0;
};
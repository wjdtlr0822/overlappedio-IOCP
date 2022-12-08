#pragma once

#include <WinSock2.h>
#include <WS2tcpip.h>

const UINT32 MAX_SOCKBUF = 1024;
const UINT32 MAX_WORKERTHREAD = 4;

enum class IOOPeration {
	RECV,
	SEND
};

struct stOverlappedEx {
	WSAOVERLAPPED m_wsaOverlapped;
	SOCKET					 m_socketClient;
	WSABUF					 m_wsaBuf;
	IOOPeration			 m_eOperation;
};

struct stClientInfo {
	INT32					mIndex = 0;
	SOCKET				m_socketClient;
	stOverlappedEx	m_stRecvOverlappedEx;
	stOverlappedEx	m_stSendOverlappedEx;

	char						mRecvBuf[MAX_SOCKBUF];
	char						mSendBuf[MAX_SOCKBUF];

	stClientInfo() {
		ZeroMemory(&m_stRecvOverlappedEx, sizeof(m_stRecvOverlappedEx));
		ZeroMemory(&m_stSendOverlappedEx, sizeof(m_stSendOverlappedEx));
		m_socketClient = INVALID_SOCKET;
	}
};
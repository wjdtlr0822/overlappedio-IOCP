#pragma once


#include<WinSock2.h>
#include <WS2tcpip.h>
const UINT32 MAX_SOCKBUF = 256;
const UINT32 MAX_WORKERTHREAD = 4;

enum class IOOPeration {
	RECV,SEND
};

struct stOverlappedEx {
	WSAOVERLAPPED   m_wsaOverlapped;
	SOCKET						m_socketClient;
	WSABUF						m_wsaBuf;
	IOOPeration				m_eOperation;
};


struct stClientInfo {
	SOCKET						m_socketClient;
	stOverlappedEx			m_stRecvOverlappedEx;
	stOverlappedEx			m_stSendOverlappedEx;
	char								mRecvBuf[MAX_SOCKBUF];
	char								mSendBuf[MAX_SOCKBUF];

	stClientInfo() {
		ZeroMemory(&m_stRecvOverlappedEx, sizeof(stOverlappedEx));
		ZeroMemory(&m_stSendOverlappedEx, sizeof(stOverlappedEx));
		m_socketClient = INVALID_SOCKET;
	}
};
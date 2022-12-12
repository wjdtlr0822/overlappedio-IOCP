#pragma once

#include "Define.h"
#include <stdio.h>


//클라이언트 정보를 담기 위한 구조체
class stClientInfo
{
public:
	stClientInfo() {
		ZeroMemory(&mRecvOverlappedEx, sizeof(mRecvOverlappedEx));
		mSock = INVALID_SOCKET;
	}

	void Init(const UINT32 index) {
		mIndex = index;
	}

	UINT32 GetIndex() { return mIndex; }
	bool IsConnectd() { return mSock != INVALID_SOCKET; }
	SOCKET GetSock() { return mSock; }
	char* RecvBuffer() { return mRecvBuf; }

	bool onConnect(HANDLE iocpHandle_, SOCKET socket_) {
		mSock = socket_;
		Clear();

		if (BindIOCompletionPort(iocpHandle_) == false) {
			return false;
		}
		return BindRecv();
	}

	void Close(bool bIsForce = false) {
		struct linger stLinger = { 0,0 };
		if (true == bIsForce) {
			stLinger.l_onoff = 1;
		}
		shutdown(mSock, SD_BOTH);
		setsockopt(mSock, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));
		closesocket(mSock);
		mSock = INVALID_SOCKET;
	}

	void Clear() {}

	//IOCP와 소켓 연결
	bool BindIOCompletionPort(HANDLE iocpHandle_)
	{
		auto hIOCP = CreateIoCompletionPort((HANDLE)GetSock(), iocpHandle_, (ULONG_PTR)(this), 0);

		if (hIOCP == INVALID_HANDLE_VALUE) {
			printf("[에러] CreateIoCompletionPort() 함수 실패 : %d\n", GetLastError());
			return false;
		}
		return true;
	}

	bool BindRecv() {
		DWORD dwFlag = 0;
		DWORD dwRecvNumBytes = 0;

		mRecvOverlappedEx.m_wsaBuf.len = MAX_SOCKBUF;
		mRecvOverlappedEx.m_wsaBuf.buf = mRecvBuf;
		mRecvOverlappedEx.m_eOperation = IOOperation::RECV;

		int nRet = WSARecv(mSock,
			&(mRecvOverlappedEx.m_wsaBuf),
			1,
			&dwRecvNumBytes,
			&dwFlag,
			(LPWSAOVERLAPPED) & (mRecvOverlappedEx),
			NULL);

		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING)) {
			printf("[ERROR] WSARecv() 함수 실패 : %d\n", WSAGetLastError());
			return false;
		}
		return true;
	}


	//1개의 스레드에서만 호출해야한다..
	bool SendMsg(const UINT32 dataSize_, char* pMsg_) {
		auto sendOverlappedEx = new stOverlappedEx;
		ZeroMemory(sendOverlappedEx, sizeof(stOverlappedEx));
		sendOverlappedEx->m_wsaBuf.len = dataSize_;
		sendOverlappedEx->m_wsaBuf.buf = new char[dataSize_];
		CopyMemory(sendOverlappedEx->m_wsaBuf.buf, pMsg_, dataSize_);
		sendOverlappedEx->m_eOperation = IOOperation::SEND;

		DWORD dwRecvNumBytes = 0;

		int nRet = WSASend(mSock,
			&(sendOverlappedEx->m_wsaBuf),
			1,
			&dwRecvNumBytes,
			0,
			(LPWSAOVERLAPPED)sendOverlappedEx,
			NULL);

		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING)) {
			printf("[ERROR] WSASend()함수 실패 : %d\n", WSAGetLastError());
			return false;
		}

		return true;
	}

	void SendCompleted(const UINT32 dataSize_) {
		printf("[송신 완료] Bytes : %d\n", dataSize_);
	}

private:
	INT32 mIndex = 0;
	SOCKET mSock;
	stOverlappedEx mRecvOverlappedEx;

	char mRecvBuf[MAX_SOCKBUF];
};
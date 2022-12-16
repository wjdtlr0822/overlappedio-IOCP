#pragma once

#include "Define.h"
#include <stdio.h>
#include <mutex>

class stClientInfo {
public:
	stClientInfo() {
		ZeroMemory(&mRecvOverlappedEx, sizeof(stOverlappedEx));
		ZeroMemory(&mSendOverlappedEx, sizeof(stOverlappedEx));
		mSock = INVALID_SOCKET;
	}

	~stClientInfo() = default;

	void Init(const UINT32 index) { mIndex = index; }
	UINT32 GetIndex() { return mIndex; }
	bool IsConnectd() { return mSock != INVALID_SOCKET; }
	SOCKET GetSock() { return mSock; }
	char* RecvBuffer() { return mRecvBuf; }

	bool OnConnect(HANDLE iocpHandle_, SOCKET socket_) {
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

		//소켓 연결을 종료 시킨다. 
		closesocket(mSock);
		mSock = INVALID_SOCKET;
	}

	void Clear() {
		mSendPos = 0;
		mIsSending = false;
	}

	bool BindIOCompletionPort(HANDLE iocpHandle_) {
		auto hIOCP = CreateIoCompletionPort((HANDLE)GetSock(), iocpHandle_, (ULONG_PTR)(this), 0);
		if (hIOCP == INVALID_HANDLE_VALUE) return false;
		return true;
	}

	bool BindRecv() {
		DWORD dwFlag = 0;
		DWORD dwRecvNumBytes = 0;

		mRecvOverlappedEx.m_wsaBuf.len = MAX_SOCKBUF;
		mRecvOverlappedEx.m_wsaBuf.buf = mRecvBuf;
		mRecvOverlappedEx.m_eOperation = IOOperation::RECV;

		if (WSARecv(mSock, &(mRecvOverlappedEx.m_wsaBuf), 1, &dwRecvNumBytes, &dwFlag, (LPWSAOVERLAPPED) & (mRecvOverlappedEx), NULL) == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			printf("[Error] WSARecv() %d\n", WSAGetLastError());
				return false;
		}
		return true;
	}

	bool SendMsg(const UINT32 dataSize_, char* pMsg_) {
		std::lock_guard<std::mutex> guard(mSendLock);
		if ((mSendPos + dataSize_) > MAX_SOCK_SENDBUF) {
			mSendPos = 0;
		}
		auto pSendBuf = &mSendBuf[mSendPos];

		CopyMemory(pSendBuf, pMsg_, dataSize_);
		mSendPos += dataSize_;

		return true;
	}

	bool SendIO() {
		if (mSendPos <= 0 || mIsSending) {
			return true;
		}
		std::lock_guard<std::mutex> guard(mSendLock);

		mIsSending = true;
		CopyMemory(mSendingBuf, &mSendBuf[0], mSendPos);

		mSendOverlappedEx.m_wsaBuf.len = mSendPos;
		mSendOverlappedEx.m_wsaBuf.buf = &mSendingBuf[0];
		mSendOverlappedEx.m_eOperation = IOOperation::SEND;

		DWORD dwRecvNumBytes = 0;

		if (WSASend(mSock, &(mSendOverlappedEx.m_wsaBuf), 1, &dwRecvNumBytes, 0, (LPWSAOVERLAPPED) & (mSendOverlappedEx), NULL) == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING)) {
			printf("[ERROR] WSASend() %d\n", WSAGetLastError());
			return false;
		}
		mSendPos = 0;
		return true;
	}


	void SendCompleted(const UINT32 dataSize_) {
		mIsSending = false;
		printf("[송신 완료] bytes : %d\n", dataSize_);
	}

private:
	INT32 mIndex = 0;
	SOCKET mSock; // client와 연결되는 소켓
	stOverlappedEx mRecvOverlappedEx;
	stOverlappedEx mSendOverlappedEx;

	char		 mRecvBuf[MAX_SOCKBUF]; // 데이터 버퍼
	std::mutex mSendLock;
	bool mIsSending = false;
	UINT64 mSendPos = 0;
	char	mSendBuf[MAX_SOCK_SENDBUF]; //데이터 버퍼
	char	mSendingBuf[MAX_SOCK_SENDBUF]; //데이터 버퍼
};
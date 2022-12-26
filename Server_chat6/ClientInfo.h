#pragma once

#include "Define.h"
#include <stdint.h>
#include <mutex>
#include <queue>

class stClientInfo {
public:
	stClientInfo() {
		ZeroMemory(&mRecvOverlappedEx, sizeof(stOverlappedEx));
		mSock = INVALID_SOCKET;
	}

	void Init(const UINT32 index) {
		mIndex = index;
	}

	UINT32 GetIndex() { return mIndex; }
	bool IsConnected() { return mSock != INVALID_SOCKET; }
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
		closesocket(mSock);
		mSock = INVALID_SOCKET;
	}

	void Clear() {}

	bool BindIOCompletionPort(HANDLE iocphandle_) {
		auto hIOCP = CreateIoCompletionPort((HANDLE)GetSock(), iocphandle_, (ULONG_PTR)(this), 0);
		if (hIOCP == INVALID_HANDLE_VALUE) {
			printf("[ERROR] CreateIoCompletionPort()함수 실패 : %d\n", GetLastError());
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
			&(mRecvOverlappedEx.m_wsaOverlapped),
			NULL);

		if (nRet == SOCKET_ERROR && (WSAGetLastError() != WSA_IO_PENDING)) {
			printf("[ERROR] WSARecv() Error : %d\n", WSAGetLastError());
			return false;
		}
		return true;
	}

	bool SendMsg(const UINT32 dataSize_, char* pMsg_) {
		auto sendOverlappedEx = new stOverlappedEx;
		ZeroMemory(sendOverlappedEx, sizeof(stOverlappedEx));

		sendOverlappedEx->m_wsaBuf.len = dataSize_;
		sendOverlappedEx->m_wsaBuf.buf = new char[dataSize_];
		CopyMemory(sendOverlappedEx->m_wsaBuf.buf, pMsg_, dataSize_);
		sendOverlappedEx->m_eOperation = IOOperation::SEND;

		std::lock_guard<std::mutex> guard(mSendLock);

		mSendDataqueue.push(sendOverlappedEx);
		if (mSendDataqueue.size() == 1) {
			SendIO();
		}
		return true;
	}


	void SendCompleted(const UINT32 dataSize_) {
		printf("[송신 완료]bytes : %d\n", dataSize_);
		std::lock_guard<std::mutex> guard(mSendLock);

		delete[] mSendDataqueue.front()->m_wsaBuf.buf;
		delete mSendDataqueue.front();

		mSendDataqueue.pop();

		if (mSendDataqueue.empty() == false) {
			SendIO();
		}
	}



private:
	bool SendIO() {
		auto sendOverlappedEx = mSendDataqueue.front();

		DWORD dwRecvNumBytes = 0;
		int nRet = WSASend(mSock,
			&(sendOverlappedEx->m_wsaBuf),
			1,
			&dwRecvNumBytes,
			0,
			&(sendOverlappedEx->m_wsaOverlapped),
			NULL);

		if (nRet == SOCKET_ERROR && (WSAGetLastError() != WSA_IO_PENDING)) {
			printf("[ERROR] WSASend() Error : %d\n", WSAGetLastError());
			return false;
		}
		return true;
	}


	INT32 mIndex = 0;
	SOCKET	mSock;
	stOverlappedEx	mRecvOverlappedEx;
	char	mRecvBuf[MAX_SOCKBUF];
	std::mutex mSendLock;
	std::queue<stOverlappedEx*> mSendDataqueue;
};
#pragma once
#include "Define.h"

#include <stdio.h>
#include <mutex>
#include <queue>

class stClientInfo
{
public:

private:

	bool SendIO() {
		auto sendOverlappedEx = mSendDataqueue.front();

		DWORD dwRecvNumBytes = 0;

	}

	INT32 mIndex = 0;
	HANDLE mIOCPHandle = INVALID_HANDLE_VALUE;
	INT64 mIsConnect = 0;
	UINT64 mLatestClosedTimeSec = 0;
	SOCKET mSocket;

	stOverlappedEx mAcceptContext;
	char mAcceptBuf[64];

	stOverlappedEx mRecvOverlappedEx;
	char mRecvBuf[MAX_SOCK_RECVBUF];

	std::mutex mSendLock;
	std::queue<stOverlappedEx*> mSendDataqueue;
};
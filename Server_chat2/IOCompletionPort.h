#pragma once
#pragma comment(lib,"ws2_32.lib")

#include "Define.h"
#include <thread>
#include <vector>
#include <WS2tcpip.h>


class IOCompletionPort {
public:
	IOCompletionPort(void) {}

	~IOCompletionPort(void) {
		WSACleanup();
	}

	bool InitSocket() {
		WSADATA wsa;
		if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
			printf("[에러] WSAStartup() Error %d\n", WSAGetLastError());
			return false;
		}
		mListenSocket = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (mListenSocket == INVALID_SOCKET) {
			printf("[에러] SOCKET() Error %d\n", WSAGetLastError());
			return false;
		}
		printf("소켓 초기화 성공....\n");
		return true;
	}


	bool BindandListen() {
		SOCKADDR_IN			servAddr;
		servAddr.sin_family = AF_INET;
		servAddr.sin_port = htons(8888);
		servAddr.sin_addr.s_addr = htonl(INADDR_ANY);

		if (bind(mListenSocket, (SOCKADDR*)&servAddr, sizeof(servAddr)) != 0) {
			printf("[에러] Bind() Error %d\n", WSAGetLastError());
			return false;
		}
		if (listen(mListenSocket, 5) != 0) {
			printf("[에러] Listen() Error %d\n", WSAGetLastError());
			return false;
		}

		printf("서버 등록 성공....\n");
		return true;
	}

	bool StartServer(const UINT32 maxClientCount) {
		CreateClient(maxClientCount);

		mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MAX_WORKERTHREAD);
		if (mIOCPHandle == NULL) {
			printf("[에러] CreateIOCompletionPort() Error %d\n", GetLastError());
			return false;
		}
		bool bRet = CreateWorkerThread();
		if (false == bRet) {
			return false;
		}
		bRet = CreateAccepterThread();
		if (false == bRet) {
			return false;
		}

		printf("서버 시작...!\n");
		return true;
	}


	void DestroyThread() {
		mIsWorkerRun = false;
		CloseHandle(mIOCPHandle);
		for (auto& th : mIOWorkerThreads) {
			if (th.joinable()) {
				th.join();
			}
		}

		mIsAccepterRun = false;
		closesocket(mListenSocket);
		if (mAccepterThread.joinable()) {
			mAccepterThread.join();
		}
	}


private:

	void CreateClient(const UINT32 maxClientCount) {
		for (int i = 0; i < maxClientCount; i++) {
			mClientInfos.emplace_back();
		}
	}


	bool CreateWorkerThread() {
		unsigned int uiThreadId = 0;
		for (int i = 0; i < MAX_WORKERTHREAD; i++) {
			mIOWorkerThreads.emplace_back([this]() {WorkerThread(); });
		}
		return true;
	}

	bool CreateAccepterThread() {
		mAccepterThread = std::thread([this]() {AccepterThread(); });
		printf("AccepterThread Start...\n");
		return true;
	}

	stClientInfo* GetEmptyClientInfo() {
		for (auto& client : mClientInfos) {
			if (INVALID_SOCKET == client.m_socketClient) {
				return &client;
			}
		}
		return nullptr;
	}


	bool BindIOCompletionPort(stClientInfo* pClientInfo) {
		auto HIOCP = CreateIoCompletionPort((HANDLE)pClientInfo->m_socketClient, mIOCPHandle, (ULONG_PTR)(pClientInfo), 0);
		
		if (NULL == HIOCP || mIOCPHandle != HIOCP) {
			printf("[ERROR] CreateIOCompletionPort() Error %d\n", GetLastError());
			return false;
		}
		return true;
	}

	bool SendMsg(stClientInfo* pClientInfo, char* msg, int nLen) {
		DWORD dwRecvNumBytes = 0;

		CopyMemory(pClientInfo->mSendBuf, msg, nLen);
		pClientInfo->mSendBuf[nLen] = '\0';

		pClientInfo->m_stSendOverlappedEx.m_wsaBuf.len = nLen;
		pClientInfo->m_stSendOverlappedEx.m_wsaBuf.buf = pClientInfo->mSendBuf;
		pClientInfo->m_stSendOverlappedEx.m_eOperation = IOOPeration::SEND;

		int nRet = WSASend(pClientInfo->m_socketClient, 
			&(pClientInfo->m_stSendOverlappedEx.m_wsaBuf), 
			1, 
			&dwRecvNumBytes, 
			0, 
			(LPWSAOVERLAPPED) & (pClientInfo->m_stSendOverlappedEx), 
			NULL);

		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING)) {
			printf("[Error] WSASend() Error : %d\n", WSAGetLastError());
			return false;
		}
		return true;
	}


	bool BindRecv(stClientInfo* pClientInfo) {
		DWORD dwFlag = 0;
		DWORD dwRecvNumBytes = 0;

		pClientInfo->m_stRecvOverlappedEx.m_wsaBuf.len = MAX_SOCKBUF;
		pClientInfo->m_stRecvOverlappedEx.m_wsaBuf.buf = pClientInfo->mRecvBuf;
		pClientInfo->m_stRecvOverlappedEx.m_eOperation = IOOPeration::RECV;

		int nRet = WSARecv(pClientInfo->m_socketClient, 
			&(pClientInfo->m_stRecvOverlappedEx.m_wsaBuf),
			1,
			&dwRecvNumBytes,
			&dwFlag,
			(LPWSAOVERLAPPED) & (pClientInfo->m_stRecvOverlappedEx),
			NULL);

		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING)) {
			printf("[Error] WSARecv() Error %d\n", WSAGetLastError());
			return false;
		}
		//printf("BindRecv : %s \n", pClientInfo->m_stRecvOverlappedEx.m_wsaBuf.buf);
		return true;
	}


	void WorkerThread() {
		stClientInfo* pClientInfo = nullptr;
		BOOL bSuccess = TRUE;
		DWORD dwIoSize = 0;
		LPOVERLAPPED lpOverlapped = NULL;

		while (mIsWorkerRun) {
			bSuccess = GetQueuedCompletionStatus(mIOCPHandle, &dwIoSize, (PULONG_PTR)&pClientInfo, &lpOverlapped, INFINITE);

			if (bSuccess == TRUE && 0 == dwIoSize && NULL == lpOverlapped) {
				mIsWorkerRun = false;
				continue;
			}
			if (NULL == lpOverlapped) {
				continue;
			}

			if (FALSE == bSuccess || (bSuccess == TRUE && dwIoSize == 0)) {
				printf("socket(%d) 접속 종료\n", int(pClientInfo->m_socketClient));
				continue;
			}

			auto pOverlapped = (stOverlappedEx*)lpOverlapped;

			if (IOOPeration::RECV == pOverlapped->m_eOperation) {
				pClientInfo->mRecvBuf[dwIoSize] = '\0';
				printf("[수신] bytes : %d, msg : %s \n", dwIoSize, pClientInfo->mRecvBuf);

				SendMsg(pClientInfo, pClientInfo->mRecvBuf, dwIoSize);
				BindRecv(pClientInfo);
			}
			else if (IOOPeration::SEND == pOverlapped->m_eOperation) {
				printf("[송신] bytes: %d, msg : %s\n", dwIoSize, pClientInfo->mSendBuf);
			}
			else {
				printf("ERROR (%d) 에서 예외사항 발생!!!", (int)pClientInfo->m_socketClient);
			}
		}
	}

	void AccepterThread() {
		SOCKADDR_IN		stClientAddr;
		int addrLen = sizeof(stClientAddr);
		while (mIsAccepterRun) {
			stClientInfo* pClientInfo = GetEmptyClientInfo();

			if (pClientInfo == NULL) {
				printf("ERROR Client\n");
				return;
			}

			pClientInfo->m_socketClient = accept(mListenSocket, (SOCKADDR*)&stClientAddr, &addrLen);
			if (INVALID_SOCKET == pClientInfo->m_socketClient) {
				continue;
			}

			bool bRet = BindIOCompletionPort(pClientInfo);
			if (false == bRet) {
				return;
			}
			printf("Accepter() Thread : \n");
			bRet = BindRecv(pClientInfo);

			if (false == bRet) {
				return;
			}

			char clientIP[32] = { 0, };
			inet_ntop(AF_INET, &(stClientAddr.sin_addr), clientIP, 31);
			printf("클라이언트 접속 : IP(%s) SOCKET(%d)\n", clientIP, (int)pClientInfo->m_socketClient);
			++mClientCnt;
		}
	}

	void CloseSocket(stClientInfo* pClientInfo, bool bIsForce = false) {
		struct linger stLinger = { 0,0 };

		if (true == bIsForce) {
			stLinger.l_onoff = 1;
		}
		shutdown(pClientInfo->m_socketClient, SD_BOTH);
		setsockopt(pClientInfo->m_socketClient, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));
		closesocket(pClientInfo->m_socketClient);
		pClientInfo->m_socketClient = INVALID_SOCKET;
	}

	std::vector<stClientInfo> mClientInfos;
	SOCKET						mListenSocket = INVALID_SOCKET;
	int							mClientCnt = 0;
	std::vector<std::thread>	mIOWorkerThreads;
	std::thread					mAccepterThread;
	HANDLE						mIOCPHandle = INVALID_HANDLE_VALUE;
	bool						mIsWorkerRun = true;
	bool						mIsAccepterRun = true;
	char						mSocketBuf[1024] = { 0, };
};
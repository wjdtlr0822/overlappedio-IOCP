#pragma once
#pragma comment(lib,"ws2_32.lib")

#include "Define.h"
#include <thread>
#include <vector>

class IOCPServer {
public:
	IOCPServer() {}
	~IOCPServer() { WSACleanup(); }

	bool InitSocket() {
		WSADATA wsa;
		if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
			printf("[Error] WSAStartup() ERROR %d\n", WSAGetLastError());
			return false;
		}

		m_listenSocket = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

		if (m_listenSocket == INVALID_SOCKET) {
			printf("SOCKET()  ERROR\n");
			return false;
		}

		printf("소켓 초기화 성공./...\n");
		return true;
	}

	bool BindandListen() {
		SOCKADDR_IN servAddr;
		servAddr.sin_family = AF_INET;
		servAddr.sin_port = htons(8888);
		servAddr.sin_addr.s_addr = htonl(INADDR_ANY);

		if (bind(m_listenSocket, (SOCKADDR*)&servAddr, sizeof(servAddr)) != 0) {
			printf("Bind() ERROR\n");
			return false;
		}
		if (listen(m_listenSocket, 5) != 0) {
			printf("listen() ERROR\n");
			return false;
		}

		printf("서버 등록 성공...\n");
		return true;
	}


	bool StartServer(const UINT32 maxClientCount) {
		CreateClient(maxClientCount);

		mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MAX_WORKERTHREAD);
		if (NULL == mIOCPHandle) {
			printf("CreateIOCompletionPort() ERROR\n");
			return false;
		}

		bool bRet = CreateWorkerThread();
		if (bRet == false) {
			return false;
		}
		bRet = CreateAccepterThread();
		if (bRet == false) {
			return false;
		}
		printf("서버 시작\n");
		return true;
	}

	void DestoryThread() {
		mIsWorkerRun = false;
		CloseHandle(mIOCPHandle);

		for (auto& x : mIOWorkerThreads) {
			if (x.joinable()) {
				x.join();
			}
		}
		mIsAccepterRun = false;
		closesocket(m_listenSocket);

		if (mAccepterThread.joinable()) {
			mAccepterThread.join();
		}
	}

	virtual void OnConnect(const UINT32 clientIndex_) {}
	virtual void OnClose(const UINT32 clientInext_) {}
	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pData_) {}


private:

	void CreateClient(const UINT32 maxClientCount) {
		for (int i = 0; i < maxClientCount; i++) {
			mClientInfos.emplace_back();
		}
	}

	bool CreateWorkerThread() {
		unsigned uiThreadId = 0;
		for (int i = 0; i < MAX_WORKERTHREAD; i++) {
			mIOWorkerThreads.emplace_back([this]() {WorkerThread(); });
		}
		return true;
	}


	bool CreateAccepterThread() {
		mAccepterThread = std::thread([this]() {AccepterThread(); });
		printf("AccepterThread 시작\n");
		return true;
	}

	stClientInfo* GetEmptyClientInfo() {
		for (auto& x : mClientInfos) {
			if (INVALID_SOCKET == x.m_socketClient) {
				return &x;
			}
		}
		return nullptr;
	}


	bool BindIOCompletionPort(stClientInfo* pClientInfo) {
		auto hIOCP = CreateIoCompletionPort((HANDLE)pClientInfo->m_socketClient,
			mIOCPHandle,
			(ULONG_PTR)(pClientInfo), 0);

		if (NULL == hIOCP || mIOCPHandle != hIOCP) {
			printf("ERROR CreateIOCompletionPort\n");
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

		int nRet = WSARecv(pClientInfo->m_socketClient,&(pClientInfo->m_stRecvOverlappedEx.m_wsaBuf),
			1,
			&dwRecvNumBytes,
			&dwFlag,
			(LPWSAOVERLAPPED)&(pClientInfo->m_stRecvOverlappedEx),
			NULL);

		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING)) {
			printf("WSARecv ERROR\n");
			return false;
		}

		return true;
	}


	bool SendMsg(stClientInfo* pClientInfo, char* pMsg, int nLen) {
		DWORD dwRecvNumBytes = 0;

		CopyMemory(pClientInfo->mSendBuf, pMsg, nLen);
		pClientInfo->mSendBuf[nLen] = '\n';

		pClientInfo->m_stSendOverlappedEx.m_wsaBuf.len = nLen;
		pClientInfo->m_stSendOverlappedEx.m_wsaBuf.buf = pClientInfo->mSendBuf;
		pClientInfo->m_stSendOverlappedEx.m_eOperation = IOOPeration::SEND;

		int nRet = WSASend(pClientInfo->m_socketClient, &(pClientInfo->m_stSendOverlappedEx.m_wsaBuf),
			1,
			&dwRecvNumBytes,
			0,
			(LPWSAOVERLAPPED) & (pClientInfo->m_stSendOverlappedEx), NULL);

		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING)) {
			printf("WSASend() ERROR\n");
			return false;
		}
		return true;
	}

	void WorkerThread() {
		stClientInfo* pClientInfo = nullptr;
		BOOL bSuccess = TRUE;
		DWORD dwIoSize = 0;
		LPOVERLAPPED lpOverlapped = NULL;

		while (mIsWorkerRun) {
			bSuccess = GetQueuedCompletionStatus(mIOCPHandle, &dwIoSize, (PULONG_PTR)pClientInfo, &lpOverlapped, INFINITE);

			if (TRUE == bSuccess && 0 == dwIoSize && NULL == lpOverlapped) {
				mIsWorkerRun = false;
				continue;
			}

			if (NULL == lpOverlapped) {
				continue;
			}

			if (FALSE == bSuccess || (TRUE == bSuccess && dwIoSize == 0)) {
				CloseSocket(pClientInfo);
				continue;
			}

			auto pOverlappedEx = (stOverlappedEx*)lpOverlapped;

			if (IOOPeration::RECV == pOverlappedEx->m_eOperation) {
				OnReceive(pClientInfo->mIndex, dwIoSize, pClientInfo->mRecvBuf);
				//pClientInfo->mRecvBuf[dwIoSize] = '\n';
				//printf("[수신] bytes : %d , msg : %s\n",dwIoSize,pClientInfo->mRecvBuf);

				//클라이언트에 메세지를 에코한다.
				SendMsg(pClientInfo, pClientInfo->mRecvBuf, dwIoSize);
				BindRecv(pClientInfo);
			}

			else if (IOOPeration::SEND == pOverlappedEx->m_eOperation) {
				printf("[송신] bytes : %d, msg : %s\n", dwIoSize, pClientInfo->mSendBuf);
			}
			else {
				printf("socket(%d)에서 예외사항 \n", (int)pClientInfo->m_socketClient);
			}
		}
	}

	void AccepterThread() {
		SOCKADDR_IN					stClientAddr;
		int											addrLen = sizeof(stClientAddr);

		while (mIsAccepterRun) {
			stClientInfo* pClientInfo = GetEmptyClientInfo();
			if (pClientInfo == NULL) {
				printf("Client Full\n");
				return;
			}
			pClientInfo->m_socketClient = accept(m_listenSocket, (SOCKADDR*)&stClientAddr, &addrLen);
			if (INVALID_SOCKET == pClientInfo->m_socketClient) {
				continue;
			}

			bool bRet = BindIOCompletionPort(pClientInfo);
			if (bRet == false) {
				return;
			}
			bRet = BindRecv(pClientInfo);
			if (bRet == false) {
				return;
			}

			OnConnect(pClientInfo->mIndex);
			++mClientCnt;
		}
	}



	//소켓의 연결을 종료 시킨다.
	void CloseSocket(stClientInfo* pClientInfo, bool bIsForce = false)
	{
		auto clientIndex = pClientInfo->mIndex;

		struct linger stLinger = { 0, 0 };	// SO_DONTLINGER로 설정

		// bIsForce가 true이면 SO_LINGER, timeout = 0으로 설정하여 강제 종료 시킨다. 주의 : 데이터 손실이 있을수 있음 
		if (true == bIsForce)
		{
			stLinger.l_onoff = 1;
		}

		//socketClose소켓의 데이터 송수신을 모두 중단 시킨다.
		shutdown(pClientInfo->m_socketClient, SD_BOTH);

		//소켓 옵션을 설정한다.
		setsockopt(pClientInfo->m_socketClient, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));

		//소켓 연결을 종료 시킨다. 
		closesocket(pClientInfo->m_socketClient);

		pClientInfo->m_socketClient = INVALID_SOCKET;

		OnClose(clientIndex);
	}

	std::vector<stClientInfo> mClientInfos;
	SOCKET							m_listenSocket = INVALID_SOCKET;
	int										mClientCnt = 0;
	std::vector<std::thread>	mIOWorkerThreads;
	std::thread						mAccepterThread;

	HANDLE							mIOCPHandle = INVALID_HANDLE_VALUE;
	bool									mIsWorkerRun = true;
	bool									mIsAccepterRun = true;
	char									mSocketBuf[1024] = { 0, };
};
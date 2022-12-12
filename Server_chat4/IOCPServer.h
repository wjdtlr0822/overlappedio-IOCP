#pragma once
#pragma comment(lib,"ws2_32.lib")

#include "ClientInfo.h"
#include "Define.h"
#include <thread>
#include <vector>


class IOCPServer
{
public:
	IOCPServer(void) {}
	virtual ~IOCPServer(void) { WSACleanup(); }

	bool InitSocket() {
		WSADATA wsa;
		if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
			printf("[Error] WSAStartup()함수 실패 : %d \n", WSAGetLastError());
			return false;
		}

		mListenSocket = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, NULL, WSA_FLAG_OVERLAPPED);

		if (INVALID_SOCKET == mListenSocket) {
			printf("[ERROR] socket()함수 실패 : %d\n", WSAGetLastError());
			return false;
		}
		printf("소켓 초기화 성공\n");
		return true;
	}

	bool BindandListen() {
		SOCKADDR_IN ServAddr;
		ServAddr.sin_family = AF_INET;
		ServAddr.sin_port = htons(8888);
		ServAddr.sin_addr.s_addr = htonl(INADDR_ANY);

		if (bind(mListenSocket, (SOCKADDR*)&ServAddr, sizeof(ServAddr)) != 0) {
			printf("[ERROR] bind()함수 실패 :  %d\n", WSAGetLastError());
			return false;
		}

		if (listen(mListenSocket, 5) != 0) {
			printf("[ERROR] listen() 함수 실패 : %d\n", WSAGetLastError());
			return false;
		}
		printf("서버 등록 성공....\n");
		return true;
	}


	bool StartServer(const UINT32 maxClientCount) {
		CreateClient(maxClientCount);

		mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MAX_WORKERTHREAD);
		if (NULL == mIOCPHandle) {
			printf("[ERROR] CreateIoCompletionPort() 함수 실패 : %d\n", GetLastError());
			return false;
		}
		if (CreateWorkerThread() == false) {
			return false;
		}
		if (CreateAccepterThread() == false) {
			return false;
		}

		printf("서버 시작\n");
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

	bool SendMsg(const UINT32 sessionIndex_, const UINT32 dataSize_, char* pMsg) {
		auto pClient = GetClientInfo(sessionIndex_);
		return pClient->SendMsg(dataSize_, pMsg);
	}


	virtual void OnConnect(const UINT32 clientIndex_) {}
	virtual void OnClose(const UINT32 clientIndex_) {}
	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pMsg) {}

private:

	void CreateClient(int maxClientCount) {
		for (int i = 0; i < maxClientCount; i++) {
			mClientInfos.emplace_back();
			mClientInfos[i].Init(i);
		}
	}

	bool CreateWorkerThread() {
		unsigned int uiThreadId = 0;
		for (int i = 0; i < MAX_WORKERTHREAD; i++) {
			mIOWorkerThreads.emplace_back([this]() { WorkerThread(); });
		}
		printf("WorkerThread시작!!!!");
		return true;
	}

	stClientInfo* GetEmptyClientInfo() {
		for (auto& client : mClientInfos) {
			if (client.IsConnectd() == false) {
				return &client;
			}
		}
		return nullptr;
	}

	stClientInfo* GetClientInfo(const UINT32 sessionIndex) {
		return &mClientInfos[sessionIndex];
	}

	bool CreateAccepterThread() {
		mAccepterThread = std::thread([this]() { AccepterThread(); });
		printf("AccepterThread 시작...!\n");
		return true;
	}

	void WorkerThread() {
		stClientInfo* pClientInfo = nullptr;
		BOOL bSuccess = TRUE;
		DWORD dwIoSize = 0;
		LPOVERLAPPED lpOverlapped = NULL;

		while (mIsWorkerRun) {
			bSuccess = GetQueuedCompletionStatus(mIOCPHandle,
				&dwIoSize,
				(PULONG_PTR) & (pClientInfo),
				&lpOverlapped,
				INFINITE);

			if (TRUE == bSuccess && dwIoSize == 0 && lpOverlapped == NULL) {
				mIsWorkerRun = false;
				continue;
			}
			if (lpOverlapped == NULL) {
				continue;
			}
			if (FALSE == bSuccess || (dwIoSize == 0 && bSuccess == TRUE)) {
				CloseSocket(pClientInfo);
				continue;
			}

			auto pOverlappedEx = (stOverlappedEx*)lpOverlapped;

			if (IOOperation::RECV == pOverlappedEx->m_eOperation) {
				OnReceive(pClientInfo->GetIndex(), dwIoSize, pClientInfo->RecvBuffer());
				pClientInfo->BindRecv();
			}
			else if (IOOperation::SEND == pOverlappedEx->m_eOperation) {
				delete[] pOverlappedEx->m_wsaBuf.buf;
				delete pOverlappedEx;
				pClientInfo->SendCompleted(dwIoSize);
			}
			else {
				printf("client Index(%d)에서 예외사황\n", pClientInfo->GetIndex());
			}
		}
	}

	void AccepterThread() {
		SOCKADDR_IN stClientAddr;
		int AddrLen = sizeof(stClientAddr);

		while (mIsAccepterRun) {
			stClientInfo* pClientInfo = GetEmptyClientInfo();
			if (NULL == pClientInfo) {
				printf("ERROR Client FULL\n");
				return;
			}

			auto newSocket = accept(mListenSocket, (SOCKADDR*)&stClientAddr, &AddrLen);
			if (newSocket == INVALID_SOCKET) {
				continue;
			}
			if (pClientInfo->onConnect(mIOCPHandle, newSocket) == false) {
				pClientInfo->Close(true);
				return;
			}

			OnConnect(pClientInfo->GetIndex());
			++mClientCnt;
		}
	}

	void CloseSocket(stClientInfo* pClientInfo, bool bIsForce = false) {
		auto clientInex = pClientInfo->GetIndex();
		pClientInfo->Close(bIsForce);
		OnClose(clientInex);
	}

	std::vector<stClientInfo> mClientInfos;
	SOCKET					 mListenSocket = INVALID_SOCKET;
	int						 mClientCnt = 0;
	std::vector<std::thread> mIOWorkerThreads;
	std::thread				 mAccepterThread;
	HANDLE					 mIOCPHandle = INVALID_HANDLE_VALUE;
	bool					 mIsWorkerRun = true;
	bool					 mIsAccepterRun = true;
};
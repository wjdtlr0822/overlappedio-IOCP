#pragma once
#pragma comment(lib,"ws2_32.lib")

#include "ClientInfo.h"
#include "Define.h"
#include <thread>
#include <vector>

class IOCPServer {
public:

	IOCPServer(void) {}
	virtual ~IOCPServer(void) { WSACleanup(); }

	bool InitSocket() {
		WSADATA wsaData;

		int nRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (0 != nRet) {
			printf("[ERROR] WSAStartup()함수 실패 : %d\n", WSAGetLastError());
			return false;
		}

		mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);

		if (INVALID_SOCKET == mListenSocket)
		{
			printf("[ERROR] socket()함수 실패 :%d\n", WSAGetLastError());
			return false;
		}
		printf("소켓 초기화 성공\n");
		return true;
	}


	bool BindandListen()
	{
		SOCKADDR_IN stServerAddr;
		stServerAddr.sin_family = AF_INET;
		stServerAddr.sin_port = htons(8888);
		stServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

		int nRet = bind(mListenSocket, (SOCKADDR*)&stServerAddr, sizeof(SOCKADDR_IN));
		if (0 != nRet) {
			printf("[ERROR] bind()함수 실패 : %d\n", WSAGetLastError());
			return false;
		}
		nRet = listen(mListenSocket, 5);
		if (0 != nRet) {
			printf("[ERROR] listen()함수 실패 :%d\n", WSAGetLastError());
			return false;
		}
		printf("서버 등록 성공\n");
		return true;
	}

	bool StartServer(const UINT32 maxClientCount) 
	{
		CreateClient(maxClientCount);
		mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MAX_WORKERTHREAD);
		if (NULL == mIOCPHandle)
		{
			printf("[ERROR] CreateIoCompletionPort()함수 실패 : %d\n", GetLastError());
			return false;
		}

		bool bRet = CreateWorkerThread();
		if (false == bRet) return false;
		bRet = CreateAccepterThread();
		if (false == bRet) return false;

		printf("server start!!!\n");
		return true;
	}

	void DestroyThrad() {
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

	bool SendMsg(const UINT32 sessionIndex_, const UINT32 dataSize_, char* pData) {
		auto pClient = GetClientInfo(sessionIndex_);
		return pClient->SendMsg(dataSize_, pData);
	}

	virtual void OnConnect(const UINT32 clientIndex_) {}
	virtual void OnClose(const UINT32 clientIndex_) {}
	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pData_) {}

private:
	void CreateClient(const UINT32 maxClientCount) {
		for (int i = 0; i < maxClientCount; i++) {
			auto client = new stClientInfo;
			client->Init(i);
			mClientInfos.push_back(client);
		}
	}

	bool CreateWorkerThread() {
		unsigned int uiThreadId = 0;
		for (int i = 0; i < MAX_WORKERTHREAD; i++) {
			mIOWorkerThreads.emplace_back([this]() {WorkerThread(); });
		}
		printf("WorkerThread Start!!!\n");
		return true;
	}

	stClientInfo* GetEmptyClientInfo() {
		for (auto client : mClientInfos) {
			if (client->IsConnected() == false) {
				return client;
			}
			return nullptr;
		}
	}

	stClientInfo* GetClientInfo(const UINT32 sessionIndex) {
		return mClientInfos[sessionIndex];
	}

	bool CreateAccepterThread() {
		mAccepterThread = std::thread([this]() {AccepterThread(); });
		printf("AccepterThread Start!!!\n");
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
				(PULONG_PTR)&pClientInfo,
				&lpOverlapped,
				INFINITE);

			if (TRUE == bSuccess && 0 == dwIoSize && NULL == lpOverlapped) {
				mIsWorkerRun = false;
				continue;
			}
			if (NULL == lpOverlapped) {
				continue;
			}

			if (FALSE == bSuccess || (bSuccess == TRUE && dwIoSize == 0)) {
				CloseSocket(pClientInfo);
				continue;
			}

			auto pOverlappedEx = (stOverlappedEx*)lpOverlapped;

			if (IOOperation::RECV == pOverlappedEx->m_eOperation) {
				OnReceive(pClientInfo->GetIndex(), dwIoSize, pClientInfo->RecvBuffer());
				pClientInfo->BindRecv();
			}
			else if (IOOperation::SEND == pOverlappedEx->m_eOperation) {
				pClientInfo->SendCompleted(dwIoSize);
			}
			else {
				printf("Client Index(%d)에서 예외사항\n", pClientInfo->GetIndex());
			}
		}
	}

	void AccepterThread() {
		SOCKADDR_IN stClientAddr;
		int nAddrLen = sizeof(SOCKADDR_IN);

		while (mIsAccepterRun) {
			stClientInfo* pClientInfo = GetEmptyClientInfo();
			if (NULL == pClientInfo) {
				printf("[ERROR] Client FULL\n");
				return;
			}

			auto newSocket = accept(mListenSocket, (SOCKADDR*)&stClientAddr, &nAddrLen);
			if (INVALID_SOCKET == newSocket) {
				continue;
			}
			if (pClientInfo->OnConnect(mIOCPHandle, newSocket) == false) {
				pClientInfo->Close(true);
				return;
			}

			OnConnect(pClientInfo->GetIndex());
			mClientCnt++;
		}
	}

	void CloseSocket(stClientInfo* pClientInfo, bool bIsForce = false) {
		auto clientIndex = pClientInfo->GetIndex();
		pClientInfo->Close(bIsForce);
		OnClose(clientIndex);
	}

	std::vector<stClientInfo*> mClientInfos;
	SOCKET mListenSocket = INVALID_SOCKET;
	int mClientCnt = 0;
	std::vector<std::thread> mIOWorkerThreads;
	std::thread mAccepterThread;

	HANDLE mIOCPHandle = INVALID_HANDLE_VALUE;
	bool mIsWorkerRun = true;
	bool mIsAccepterRun = true;

};
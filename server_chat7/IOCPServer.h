#pragma once
#pragma comment(lib, "ws2_32")
#pragma comment(lib,"mssock.lib")

#include "ClientInfo.h"
#include "Define.h"
#include <thread>
#include <vector>


class IOCPServer {
public:
	IOCPServer() {}

	virtual ~IOCPServer() { WSACleanup(); }

	bool Init(const UINT32 maxIOWorkerThreadCount_) {
		WSADATA wsa;

		if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
			printf("[ERROR] WSAStartup() ERROR : %d\n", WSAGetLastError());
			return false;
		}
		mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);

		if (INVALID_SOCKET == mListenSocket) {
			printf("[ERROR] WSASocket Error : %d\n", WSAGetLastError());
			return false;
		}

		MaxIOWorkerThreadCount = maxIOWorkerThreadCount_;
		printf("SOCKET INIT SUCCESS !!! \n");
		return true;
	}

	bool BindandListen() {
		SOCKADDR_IN stServerAddr;
		stServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
		stServerAddr.sin_port = htons(8888);
		stServerAddr.sin_family = AF_INET;

		int nRet = bind(mListenSocket, (SOCKADDR*)&stServerAddr, sizeof(stServerAddr));
		if (nRet != 0) {
			printf("[ERROR] bind ERROR : %d\n", WSAGetLastError());
			return false;
		}

		nRet = listen(mListenSocket, 5);
		if (nRet != 0) {
			printf("[Error] listen() ERROR : %d\n", WSAGetLastError());
			return false;
		}
		mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MaxIOWorkerThreadCount);
		if (NULL == mIOCPHandle) {
			printf("[ERROR] CreateIoCompletionPort() ERROR : %d\n", GetLastError());
			return false;
		}

		auto hIOCPHandle = CreateIoCompletionPort((HANDLE)mListenSocket, mIOCPHandle, (UINT32)0, 0);
		if (nullptr == hIOCPHandle) {
			printf("[ERROR] listen socket IOCP bind 실패 %d\n", WSAGetLastError());
			return false;
		}
		printf("서버 등록 성공....!\n");
		return true;
	}

	bool StartServer(const UINT32 maxClientCount) {
		CreateClient(maxClientCount);

		bool bRet = CreateWorkerThread();
		if (false == bRet) { return false; }
		bRet = CreateAccepterThread();
		if (false == bRet) return false;

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

	bool SendMsg(const UINT32 sessionIndex_, const UINT32 dataSize_, char* pData) {
		auto pClient = GetClientInfo(sessionIndex_);
		return pClient->SendMsg(dataSize_, pData);
	}

	virtual void OnConnect(const UINT32 clientIndex_) {}
	virtual void OnClose(const UINT32 clientIndex_) {}
	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pData_);

private:
	void CreateClient(const UINT32 maxClientCount) {
		for (int i = 0; i < maxClientCount; i++) {
			auto client = new stClientInfo;
			client->Init(i, mIOCPHandle);
			mClientInfos.push_back(client);
		}
	}


	bool CreateWorkerThread() {
		unsigned int uiThreadId = 0;
		for (int i = 0; i < MaxIOWorkerThreadCount; i++) {
			mIOWorkerThreads.emplace_back([this]() {WorkerThread(); });
		}
		printf("WorkerThread Start....!\n");
		return true;
	}

	stClientInfo* GetEmptyClientInfo() {
		for (auto& client : mClientInfos) {
			if (client->IsConnectd() == false) {
				return client;
			}
		}
		return nullptr;
	}

	stClientInfo* GetClientInfo(const UINT32 sessionIndex) {
		return mClientInfos[sessionIndex];
	}

	bool CreateAccepterThread() {
		mAccepterThread = std::thread([this]() {AccepterThread(); });
		printf("AccepterThread Start...!\n");
		return true;
	}

	void WorkerThread() {
		stClientInfo* pClientInfo = nullptr;
		BOOL bSuccess = TRUE;
		DWORD dwIoSize = 0;
		LPOVERLAPPED lpOverlapped = NULL;


		while (mIsWorkerRun) {
			bSuccess = GetQueuedCompletionStatus(mIOCPHandle, &dwIoSize,
				(PULONG_PTR) & (pClientInfo),
				&lpOverlapped,
				INFINITE);

			if (bSuccess == TRUE && dwIoSize == 0 && lpOverlapped == NULL) {
				mIsWorkerRun = false;
				continue;
			}

			if (lpOverlapped == NULL) {
				continue;
			}
			auto pOverlappedEx = (stOverlappedEx*)lpOverlapped;
			if (bSuccess == FALSE || (0 == dwIoSize && IOOperation::ACCEPT != pOverlappedEx->m_eOperation)) {
				CloseSocket(pClientInfo);
				continue;
			}

			if (IOOperation::ACCEPT == pOverlappedEx->m_eOperation) {
				pClientInfo = GetClientInfo(pOverlappedEx->SessionIndex);
				if (pClientInfo->Acceptcompletion()) {
					++mClientCnt;
					OnConnect(pClientInfo->GetIndex());
				}
				else {
					CloseSocket(pClientInfo, true);
				}
			}

			else if (IOOperation::RECV == pOverlappedEx->m_eOperation) {
				OnReceive(pClientInfo->GetIndex(), dwIoSize, pClientInfo->RecvBuffer());
				pClientInfo->BindRecv();
			}
			else if (IOOperation::SEND == pOverlappedEx->m_eOperation) {
				pClientInfo->SendCompleted(dwIoSize);
			}
			else {
				printf("Client index(%d) ERROR\n", pClientInfo->GetIndex());
			}
		}
	}

	void AccepterThread() {
		while (mIsAccepterRun) {
			auto curTimeSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

			for (auto client : mClientInfos) {
				if (client->IsConnectd()) {
					continue;
				}
				if ((UINT64)curTimeSec < client->GetLatestClosedTimeSec()) {
					continue;
				}
				auto diff = curTimeSec - client->GetLatestClosedTimeSec();
				if (diff <= RE_USE_SESSION_WAIT_TIMESEc) {
					continue;
				}
				client->PostAccept(mListenSocket, curTimeSec);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
	}

	void CloseSocket(stClientInfo* pClientInfo, bool bIsForce = false) {
		auto clientIndex = pClientInfo->GetIndex();

		pClientInfo->Close(bIsForce);
		OnClose(clientIndex);
	}

	UINT32 MaxIOWorkerThreadCount = 0;
	std::vector<stClientInfo*> mClientInfos;
	SOCKET mListenSocket = INVALID_SOCKET;
	int mClientCnt = 0;
	std::vector<std::thread> mIOWorkerThreads;
	std::thread mAccepterThread;
	HANDLE mIOCPHandle = INVALID_HANDLE_VALUE;
	bool mIsWorkerRun = true;
	bool mIsAccepterRun = true;
};
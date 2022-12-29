#pragma once
#pragma comment(lib, "ws2_32")
#pragma comment(lib,"mssock.lib")

#include "ClientInfo.h"
#include "Define.h"
#include <thread>
#include <vector>


class IOCPServer {
public:

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
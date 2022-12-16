#pragma once
#pragma comment(lib,"ws2_32")

#include "ClientInfo.h"
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
			printf("[ERROR] WSAStartup() %d\n", WSAGetLastError());
			return false;
		}
		mListenSocket = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (mListenSocket == INVALID_SOCKET) {
			printf("[ERROR] WSASocket() %d\n", WSAGetLastError());
			return false;
		}
		printf("socket init success\n");
		return true;
	}

	bool BindandListen()
	{
		SOCKADDR_IN servAddr;
		servAddr.sin_family = AF_INET;
		servAddr.sin_port = htons(8888);
		servAddr.sin_addr.s_addr = htonl(INADDR_ANY);

		if (bind(mListenSocket, (SOCKADDR*)&servAddr, sizeof(servAddr)) != 0) {
			printf("[ERROR] BInd() %d\n", WSAGetLastError());
			return false;
		}
		if (listen(mListenSocket, 5) != 0)
		{
			printf("[ERROR] Listen() %d\n", WSAGetLastError());
			return false;
		}
		printf("���� ��� ����...!\n");
		return true;
	}

	bool StartServer(const UINT32 maxClientCount) {
		CreateClient(maxClientCount);

		mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MAX_WORKERTHREAD);
		if (NULL == mIOCPHandle)
		{
			printf("[ERROR] CreateIoCompletionPort() �Լ� ���� %d\n", WSAGetLastError());
			return false;
		}
		if (CreateWorkerThread() == false) {
			return false;
		}
		if (CreateAccepterThread() == false) {
			return false;
		}

		CreateSendThread();

		printf("���� ����\n");
		return true;
	}

	void DestroyThread()
	{
		mIsSenderRun = false;

		if (mSendThread.joinable()) {
			mSendThread.join();
		}

		mIsAccepterRun = false;
		closesocket(mListenSocket);

		if (mAccepterThread.joinable()) {
			mAccepterThread.join();
		}

		mIsWorkerRun = false;
		CloseHandle(mIOCPHandle);

		for (auto& th : mIOWorkerThreads) {
			if (th.joinable()) {
				th.join();
			}
		}
	}

	bool SendMsg(const UINT32 sessionIndex_, const UINT32 dataSize_, char* pData) {
		auto pClient = GetClientInfo(sessionIndex_);
		return pClient->SendMsg(dataSize_, pData);
	}

	virtual void OnConnect(const UINT32 clientIndex_) {}
	virtual void OnClose(const UINT32 clientIndex_) { }
	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pData) {}

private:
	void CreateClient(const UINT32 maxClientCount) {
		for (int i = 0; i < maxClientCount; i++) {
			auto client = new stClientInfo();
			client->Init(i);
			mClientInfos.push_back(client);
		}
	}

	bool CreateWorkerThread() {
		mIsWorkerRun = true;
		for (int i = 0; i < MAX_WORKERTHREAD; i++) {
			mIOWorkerThreads.emplace_back([this]() { WokerThread(); })
		}
		printf("WORKERThread ����....\n");
		return true;
	}

	bool CreateAccepterThread() {
		mIsAccepterRun = true;
		mAccepterThread = std::thread([this]() { AccepterThread(); });
		printf("AccepterThread ����....\n");
		return true;
	}

	void CreateSendThread() {
		mIsSenderRun = true;
		mSendThread = std::thread([this]() {SendThread(); });
		printf("SendThread����\n");
	}

	stClientInfo* GetEmptyClientInfo() {
		for (auto client : mClientInfos) {
			if (client->IsConnectd() == false) {
				return client;
			}
		}
		return nullptr;
	}

	stClientInfo* GetclientInfo(const UINT32 sessionIndex) {
		return mClientInfos[sessionIndex];
	}

	void CloseSocket(stClientInfo* pClientInfo,bool bIsForce =false)
	{
		auto clientIndex = pClientInfo->GetIndex();
		pClientInfo->Close(bIsForce);
		OnClose(clientIndex);
	}
	//Ŭ���̾�Ʈ ���� ���� ����ü
	std::vector<stClientInfo*> mClientInfos;

	//Ŭ���̾�Ʈ�� ������ �ޱ����� ���� ����
	SOCKET		mListenSocket = INVALID_SOCKET;

	//���� �Ǿ��ִ� Ŭ���̾�Ʈ ��
	int			mClientCnt = 0;

	//�۾� ������ ���� �÷���
	bool		mIsWorkerRun = false;
	//IO Worker ������
	std::vector<std::thread> mIOWorkerThreads;

	//���� ������ ���� �÷���
	bool		mIsAccepterRun = false;
	//Accept ������
	std::thread	mAccepterThread;

	//���� ������ ���� �÷���
	bool		mIsSenderRun = false;
	//Accept ������
	std::thread	mSendThread;

	//CompletionPort��ü �ڵ�
	HANDLE		mIOCPHandle = INVALID_HANDLE_VALUE;
};
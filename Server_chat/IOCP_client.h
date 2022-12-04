#pragma once
#pragma comment(lib,"ws2_32.lib")
#include <WinSock2.h>
#include <WS2tcpip.h>

#include <thread>
#include <vector>

#define MAX_SOCKBUF 1024
#define MAX_WORKRTHREAD 4 //������ Ǯ�� ���� ������ ����

enum class IOOperation {
	RECV,
	SEND
};

//WSAOVERLAPPED����ü�� Ȯ�� ���Ѽ� �ʿ��� ������ �� �־���.
struct stOVerlappedEx {
	WSAOVERLAPPED	m_wsaOverlapped; // overlapped I/O ����ü
	SOCKET			m_socketClientl; // Ŭ���̾�Ʈ ����
	WSABUF			m_swaBuf; // overlapped I/O�۾� ����
	char			m_szBuf[MAX_SOCKBUF]; // �������� ����
	IOOperation		m_eOperation; //�۾� ���� ����
};


//Ŭ���̾�Ʈ ������ ������� ����ü
struct stClientInfo {
	SOCKET				m_socketClient; //client�� ����Ǵ� ����
	stOVerlappedEx		m_stRecvOverlappedEx; // RECV overlapped
	stOVerlappedEx		m_stSendOverlappedEx; // SEND overlapped

	stClientInfo() {
		ZeroMemory(&m_stRecvOverlappedEx, sizeof(stOVerlappedEx));
		ZeroMemory(&m_stSendOverlappedEx, sizeof(stOVerlappedEx));
		m_socketClient = INVALID_SOCKET;
	}
};

class IOCompletionPort {
public:
	IOCompletionPort(void) {}

	~IOCompletionPort(void) {
		WSACleanup();
	}

	bool InitSocket() {
		WSADATA wsaData;
		int nRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (nRet != 0) {
			printf("[error] WSAstartup() Error!!");
			return false;
		}

		//���������� TCP, Overlapped I/O���� ����
		mListenSocket = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

		if (INVALID_SOCKET == mListenSocket) {
			printf("[Error] : Socket()�Լ� ���� %d\n", WSAGetLastError());
			return false;
		}
		printf("���� �ʱ�ȭ ����\n");
		return true;
	}


	//������ �ּ������� ���ϰ� �����Ű�� ���� ��û�� �ޱ� ���� // ������ ����ϴ� �Լ�
	bool BindandListen(int nBindPort) {
		SOCKADDR_IN			stServerAddr;
		stServerAddr.sin_family = AF_INET;
		stServerAddr.sin_port = htons(8888);
		stServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

		int nRet = bind(mListenSocket, (SOCKADDR*)&stServerAddr, sizeof(stServerAddr));
		if (nRet != 0) {
			printf("[ERROR] : bind() Error");
			return false;
		}

		nRet = listen(mListenSocket, 5);
		if (nRet != 0) {
			printf("[Error] : listen() Error");
			return false;
		}
		printf("���� ��� ����...\n");
		return true;
	}

	//���� ��û�� �����ϰ� �޼����� �޾Ƽ� ó���ϴ� �Լ�
	bool startServer(const UINT32 maxClientCount) {
		CreateClient(maxClientCount);

		//CompletionPort��ü ���� ��û�� �Ѵ�.
		mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MAX_WORKRTHREAD);//?
		if (mIOCPHandle == NULL) {
			printf("[ERROR] CreateIoCompletionPort() ERROR %d\n", GetLastError());
			return false;
		}

		//���ӵ� Ŭ���̾�Ʈ �ּ� ������ ������ ����ü
		bool bRet = CreateWorkerThread();
		if (false == bRet) {
			return false;
		}
		bRet = CreateAccepterThread();
		if (false == bRet) {
			return false;
		}

		printf("���� ����\n");
		return true;
	}

	//�����Ǿ��ִ� �����带 �ı��Ѵ�.
	void DestroyThread() {
		mIsWorkerRun = false;
		CloseHandle(mIOCPHandle);

		for (auto& th : mIOWorkerThreads) {
			if (th.joinable()) {
				th.join();
			}
		}
		//Accepter �����带 �����Ѵ�.
		mIsAcceptRun = false;
		closesocket(mListenSocket);
		if (mAccepterThread.joinable()) {
			mAccepterThread.join();
		}
	}

private:
	void CreateClient(const UINT32 maxClientCount) {
		for (UINT i = 0; i < maxClientCount; i++) {
			mClientInfos.emplace_back();
		}
	}

	//WaitingThread Queue���� ����� ��������� ����
	bool CreateWorkerThread() {
		unsigned int uiThreadId = 0;
		//WaingThread Queue�� ��� ���·� ���� ������� ���� ����Ǵ� ���� : (cpu���� * 2) + 1 
		for (int i = 0; i < MAX_WORKRTHREAD; i++) {
			mIOWorkerThreads.emplace_back([this]() {WorkerThread(); });
		}
		printf("WorkerThread Start....\n");
		return true;
	}

	//accept��û�� ó���ϴ� ������ ����
	bool CreateAccepterThread() {
		mAccepterThread = std::thread([this]() {AccepterThread(); });
		printf("AccepterThread Start...\n");
		return true;
	}


	//������� �ʴ� Ŭ���̾�Ʈ ���� ����ü�� ��ȯ�Ѵ�.
	stClientInfo* GetEmptyClientInfo() {
		for (auto& client : mClientInfos) {
			if (INVALID_SOCKET == client.m_socketClient) {
				return &client;
			}
		}
		return nullptr;
	}


	//CompletionPort��ü�� ���ϰ� CompletionKey��
	//�����Ű�� ������ �Ѵ�.
	bool BindIOCompletionPort(stClientInfo* pClientInfo) {
		//socket�� pClientInfo�� CompletionPort��ü�� �����Ų��.
		auto HIOCP = CreateIoCompletionPort((HANDLE)pClientInfo->m_socketClient,
			mIOCPHandle, (ULONG_PTR)(pClientInfo), 0);

		if (NULL == HIOCP || mIOCPHandle != HIOCP) {
			printf("[ERROR] CreateIoCompletionPort() ERROR %d\n", GetLastError());
			return false;
		}
		return true;
	}

	bool BindRecv(stClientInfo* pClientInfo) {
		DWORD dwflag = 0;
		DWORD dwRecvNumBytes = 0;

		pClientInfo->m_stRecvOverlappedEx.m_swaBuf.len = MAX_SOCKBUF;
		pClientInfo->m_stRecvOverlappedEx.m_swaBuf.buf = pClientInfo->m_stRecvOverlappedEx.m_szBuf;
		pClientInfo->m_stRecvOverlappedEx.m_eOperation = IOOperation::RECV;

		int nRet = WSARecv(pClientInfo->m_socketClient
			, &(pClientInfo->m_stRecvOverlappedEx.m_swaBuf), 1, &dwRecvNumBytes, &dwflag
			, (LPWSAOVERLAPPED) & (pClientInfo->m_stRecvOverlappedEx.m_wsaOverlapped), NULL);

		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING)) {
			printf("[ERROR] : WSARecv�Լ� ���� %d\n", WSAGetLastError());
			return false;
		}
		return true;
	}

	bool SendMsg(stClientInfo* pClientInfo, char* msg, int nLen) {
		DWORD dwRecvNumBytes = 0;

		CopyMemory(pClientInfo->m_stSendOverlappedEx.m_szBuf, pMsg, nLen);

		pClientInfo->m_stSendOverlappedEx.m_swaBuf.len = nLen;
		pClientInfo->m_stSendOverlappedEx.m_swaBuf.buf = msg;
		pClientInfo->m_stSendOverlappedEx.m_eOperation = IOOperation::SEND;

		int nRet = WSASend(pClientInfo->m_socketClient,
			&(pClientInfo->m_stSendOverlappedEx.m_swaBuf),
			1,
			&dwRecvNumBytes,
			0,
			(LPWSAOVERLAPPED) & (pClientInfo->m_stSendOverlappedEx.m_wsaOverlapped),
			NULL);

		if (nRet == SOCKET_ERROR && (WSAGetLastError() != WSA_IO_PENDING)) {
			printf("[ERROR] : WSASend() Error");
			return false;
		}
		return true;
	}


	void WorkerThread() {
		stClientInfo* pClientInfo = NULL;

		BOOL bSuccess = TRUE;

		DWORD dwIoSize = 0;
		LPOVERLAPPED IpOverLapped = NULL;

		while (mIsWorkerRun) {
			//////////////////////////////////////////////////////
			//�� �Լ��� ���� ��������� WaitingThread Queue��
			//��� ���·� ���� �ȴ�.
			//�Ϸ�� Overlapped I/O�۾��� �߻��ϸ� IOCP Queue����
			//�Ϸ�� �۾��� ������ �� ó���� �Ѵ�.
			//�׸��� PostQueuedCompletionStatus()�Լ������� �����
			//�޼����� �����Ǹ� �����带 �����Ѵ�.
			//////////////////////////////////////////////////////
			bSuccess = GetQueuedCompletionStatus(mIOCPHandle,
				&dwIoSize,
				(PULONG_PTR)&pClientInfo,
				&IpOverLapped,
				INFINITE);

			if (TRUE == bSuccess && 0 == dwIoSize && NULL == IpOverLapped) {
				mIsWorkerRun = false;
				continue;
			}
			if (NULL == IpOverLapped) {
				continue;
			}


			//Ŭ���̾�Ʈ�� ������ ������ ��
			if (FALSE == bSuccess || (0 == dwIoSize && TRUE == bSuccess)) {
				printf("socket(%d) close", (int)pClientInfo->m_socketClient);
				CloseSocket(pClientInfo);
				continue;
			}

			stOVerlappedEx* pOverlappedEx = (stOVerlappedEx*)IpOverLapped;//??

			if (IOOperation::RECV == pOverlappedEx->m_eOperation) {
				pOverlappedEx->m_szBuf[dwIoSize] = NULL;
				printf("[����] bytes : %d , msg : %s\n", dwIoSize, pOverlappedEx->m_szBuf);

				SendMsg(pClientInfo, pOverlappedEx->m_szBuf, dwIoSize);
				BindRecv(pClientInfo);
			}
			else if (IOOperation::SEND == pOverlappedEx->m_eOperation) {
				printf("[�۽�] byte : %d, msg :%s\n", dwIoSize, pOverlappedEx->m_szBuf);
			}

			else {
				printf("socket(%d)���� ���ܻ��� �߻� \n", (int)pClientInfo->m_socketClient);
			}
		}
	}

	void AccepterThread() {
		SOCKADDR_IN				stClientAddr;
		int						nAddrLen = sizeof(SOCKADDR_IN);

		while (mIsAcceptRun) {
			stClientInfo* pClientInfo = GetEmptyClientInfo();
			if (NULL == pClientInfo) {
				printf("[Error] : client FULL");
				return;
			}

			pClientInfo->m_socketClient = accept(mListenSocket, (SOCKADDR*)&stClientAddr, &nAddrLen);

			if (INVALID_SOCKET == pClientInfo->m_socketClient) {
				continue;
			}
			bool bRet = BindIOCompletionPort(pClientInfo);
			if (false == bRet) {
				return;
			}

			bRet = BindRecv(pClientInfo);
			if (false == bRet) {
				return;
			}

			char clientIP[32] = { 0, };
			inet_ntop(AF_INET, &(stClientAddr.sin_addr), clientIP, 32 - 1);
			printf("Ŭ���̾�Ʈ ���� : IP(%s) SOCKET(%d)\n", clientIP, (int)pClientInfo->m_socketClient);
			++mClinetCnt;
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

	//Ŭ���̾�Ʈ ���� ���� ����ü
	std::vector<stClientInfo>			mClientInfos;

	//Ŭ���̾�Ʈ�� ������ �ޱ����� ���� ����
	SOCKET								mListenSocket = INVALID_SOCKET;

	//���ӵǾ��ִ� Ŭ���̾�Ʈ ��
	int									mClinetCnt = 0;

	//IO WorKer ������
	std::vector<std::thread>			mIOWorkerThreads;

	// Accept������
	std::thread							mAccepterThread;

	// CompletionPort ��ü �ڵ�
	HANDLE								mIOCPHandle = INVALID_HANDLE_VALUE;

	//�۾� ������ ���� �÷���
	bool								mIsWorkerRun = true;

	//���� ������ ���� �÷���
	bool								mIsAcceptRun = true;

	//���� ����
	char								mSocketBuf[1024] = { 0, };
};
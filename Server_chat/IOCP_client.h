#pragma once
#pragma comment(lib,"ws2_32.lib")
#include <WinSock2.h>
#include <WS2tcpip.h>

#include <thread>
#include <vector>

#define MAX_SOCKBUF 1024
#define MAX_WORKRTHREAD 4 //쓰레드 풀에 넣을 쓰레드 개수

enum class IOOperation {
	RECV,
	SEND
};

//WSAOVERLAPPED구조체를 확장 시켜서 필요한 정보를 더 넣었다.
struct stOVerlappedEx {
	WSAOVERLAPPED	m_wsaOverlapped; // overlapped I/O 구조체
	SOCKET			m_socketClientl; // 클라이언트 소켓
	WSABUF			m_swaBuf; // overlapped I/O작업 버퍼
	char			m_szBuf[MAX_SOCKBUF]; // 데이터의 버퍼
	IOOperation		m_eOperation; //작업 동작 종류
};


//클라이언트 정보를 담기위한 구조체
struct stClientInfo {
	SOCKET				m_socketClient; //client와 연결되는 소켓
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

		//연결지향형 TCP, Overlapped I/O소켓 생성
		mListenSocket = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

		if (INVALID_SOCKET == mListenSocket) {
			printf("[Error] : Socket()함수 실패 %d\n", WSAGetLastError());
			return false;
		}
		printf("소켓 초기화 성공\n");
		return true;
	}


	//서버의 주소정보를 소켓과 연결시키고 접속 요청을 받기 위해 // 소켓을 등록하는 함수
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
		printf("서버 등록 성공...\n");
		return true;
	}

	//접속 요청을 수락하고 메세지를 받아서 처리하는 함수
	bool startServer(const UINT32 maxClientCount) {
		CreateClient(maxClientCount);

		//CompletionPort객체 생성 요청을 한다.
		mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MAX_WORKRTHREAD);//?
		if (mIOCPHandle == NULL) {
			printf("[ERROR] CreateIoCompletionPort() ERROR %d\n", GetLastError());
			return false;
		}

		//접속된 클라이언트 주소 정보를 저장할 구조체
		bool bRet = CreateWorkerThread();
		if (false == bRet) {
			return false;
		}
		bRet = CreateAccepterThread();
		if (false == bRet) {
			return false;
		}

		printf("서버 시작\n");
		return true;
	}

	//생성되어있는 쓰레드를 파괴한다.
	void DestroyThread() {
		mIsWorkerRun = false;
		CloseHandle(mIOCPHandle);

		for (auto& th : mIOWorkerThreads) {
			if (th.joinable()) {
				th.join();
			}
		}
		//Accepter 쓰레드를 종요한다.
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

	//WaitingThread Queue에서 대기할 쓰레드들을 생성
	bool CreateWorkerThread() {
		unsigned int uiThreadId = 0;
		//WaingThread Queue에 대기 상태로 넣을 쓰레드들 생성 권장되는 개수 : (cpu개수 * 2) + 1 
		for (int i = 0; i < MAX_WORKRTHREAD; i++) {
			mIOWorkerThreads.emplace_back([this]() {WorkerThread(); });
		}
		printf("WorkerThread Start....\n");
		return true;
	}

	//accept요청을 처리하는 쓰레드 생성
	bool CreateAccepterThread() {
		mAccepterThread = std::thread([this]() {AccepterThread(); });
		printf("AccepterThread Start...\n");
		return true;
	}


	//사용하지 않는 클라이언트 정보 구조체를 반환한다.
	stClientInfo* GetEmptyClientInfo() {
		for (auto& client : mClientInfos) {
			if (INVALID_SOCKET == client.m_socketClient) {
				return &client;
			}
		}
		return nullptr;
	}


	//CompletionPort객체와 소켓과 CompletionKey를
	//연결시키는 역할을 한다.
	bool BindIOCompletionPort(stClientInfo* pClientInfo) {
		//socket과 pClientInfo를 CompletionPort객체와 연결시킨다.
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
			printf("[ERROR] : WSARecv함수 실패 %d\n", WSAGetLastError());
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
			//이 함수로 인해 쓰레드들은 WaitingThread Queue에
			//대기 상태로 들어가게 된다.
			//완료된 Overlapped I/O작업이 발생하면 IOCP Queue에서
			//완료된 작업을 가져와 뒤 처리를 한다.
			//그리고 PostQueuedCompletionStatus()함수에의해 사용자
			//메세지가 도착되면 쓰레드를 종료한다.
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


			//클라이언트가 접속을 끊었을 떄
			if (FALSE == bSuccess || (0 == dwIoSize && TRUE == bSuccess)) {
				printf("socket(%d) close", (int)pClientInfo->m_socketClient);
				CloseSocket(pClientInfo);
				continue;
			}

			stOVerlappedEx* pOverlappedEx = (stOVerlappedEx*)IpOverLapped;//??

			if (IOOperation::RECV == pOverlappedEx->m_eOperation) {
				pOverlappedEx->m_szBuf[dwIoSize] = NULL;
				printf("[수신] bytes : %d , msg : %s\n", dwIoSize, pOverlappedEx->m_szBuf);

				SendMsg(pClientInfo, pOverlappedEx->m_szBuf, dwIoSize);
				BindRecv(pClientInfo);
			}
			else if (IOOperation::SEND == pOverlappedEx->m_eOperation) {
				printf("[송신] byte : %d, msg :%s\n", dwIoSize, pOverlappedEx->m_szBuf);
			}

			else {
				printf("socket(%d)에서 예외사항 발생 \n", (int)pClientInfo->m_socketClient);
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
			printf("클라이언트 접속 : IP(%s) SOCKET(%d)\n", clientIP, (int)pClientInfo->m_socketClient);
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

	//클라이언트 정보 저장 구조체
	std::vector<stClientInfo>			mClientInfos;

	//클라이언트의 접속을 받기위한 리슨 소켓
	SOCKET								mListenSocket = INVALID_SOCKET;

	//접속되어있는 클라이언트 수
	int									mClinetCnt = 0;

	//IO WorKer 스레드
	std::vector<std::thread>			mIOWorkerThreads;

	// Accept스레드
	std::thread							mAccepterThread;

	// CompletionPort 객체 핸들
	HANDLE								mIOCPHandle = INVALID_HANDLE_VALUE;

	//작업 쓰레드 동작 플레그
	bool								mIsWorkerRun = true;

	//접속 쓰레드 동작 플래그
	bool								mIsAcceptRun = true;

	//소켓 버퍼
	char								mSocketBuf[1024] = { 0, };
};
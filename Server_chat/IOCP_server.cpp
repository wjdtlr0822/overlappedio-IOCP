#include "IOCP_client.h";

const UINT16 SERVER_PORT = 11021;
const UINT16 MAX_CLIENT = 100;


int main() {
	IOCompletionPort ioCompletionport;

	ioCompletionport.InitSocket();
	ioCompletionport.BindandListen(SERVER_PORT);
	ioCompletionport.startServer(MAX_CLIENT);

	printf("�ƹ� Ű�� Ŭ��");
	getchar();

	ioCompletionport.DestroyThread();
	return 0;
}


//ioCompletionport ����

//vector<stClientInfo> mClientInfos;
// SOCKET mListenSocket
// int mClientCnt = 0;
// vector<thread> mIOWorkerThreads;
//thread mAccepterThread;
//HANDLE mIOCPHANDLE;
//bool mlsWorkerRun = true;
//bool mlsAcceptRun = true;
//char mSocketBuf[1024] = {0,};
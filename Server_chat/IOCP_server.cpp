#include "IOCP_client.h";

const UINT16 SERVER_PORT = 11021;
const UINT16 MAX_CLIENT = 100;


int main() {
	IOCompletionPort ioCompletionport;

	ioCompletionport.InitSocket();
	ioCompletionport.BindandListen(SERVER_PORT);
	ioCompletionport.startServer(MAX_CLIENT);

	printf("아무 키나 클릭");
	getchar();

	ioCompletionport.DestroyThread();
	return 0;
}
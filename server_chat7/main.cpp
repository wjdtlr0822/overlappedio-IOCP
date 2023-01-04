#include "EchoServer.h"
#include <string>
#include <iostream>


const UINT16 MAX_CLIENT = 3; //�� �����Ҽ� �ִ� Ŭ���̾�Ʈ ��
const UINT32 MAX_IO_WORKER_THREAD = 4;


int main() {
	EchoServer server;

	server.Init(MAX_IO_WORKER_THREAD);

	server.BindandListen();

	server.Run(MAX_CLIENT);

	printf("CLICK\n");
	while (true) {
		std::string inputCmd;
		std::getline(std::cin, inputCmd);

		if (inputCmd == "quit") {
			break;
		}
	}


	server.End();
	return 0;
}
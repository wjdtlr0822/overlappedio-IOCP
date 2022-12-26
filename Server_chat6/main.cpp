#include "EchoServer.h"
#include <string>
#include <iostream>

const UINT16 MAX_CLIENT = 100;

int main() {
	EchoServer server;

	server.InitSocket();

	server.BindandListen();

	server.Run(MAX_CLIENT);

	printf("button Click\n");
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
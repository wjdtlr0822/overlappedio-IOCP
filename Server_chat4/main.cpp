#include "EchoServer.h"

#include <string>
#include <iostream>

const UINT16 MAX_CLIENT = 100;

//4. 네트워크와 로직(패킷 or 요청) 처리 각각의 스레드로 분리하기
// - Send를 recv와 다른 스레드에서 하기
// - send를 연속으로 보낼 수 있는 구조가 되어야 한다.


int main() {
	EchoServer server;

	server.InitSocket();

	server.BindandListen();
	server.Run(MAX_CLIENT);

	printf("아무 키나 누를 때까지 대기합니다\n");
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
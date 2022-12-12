#include "EchoServer.h"

#include <string>
#include <iostream>

const UINT16 MAX_CLIENT = 100;

//4. ��Ʈ��ũ�� ����(��Ŷ or ��û) ó�� ������ ������� �и��ϱ�
// - Send�� recv�� �ٸ� �����忡�� �ϱ�
// - send�� �������� ���� �� �ִ� ������ �Ǿ�� �Ѵ�.


int main() {
	EchoServer server;

	server.InitSocket();

	server.BindandListen();
	server.Run(MAX_CLIENT);

	printf("�ƹ� Ű�� ���� ������ ����մϴ�\n");
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
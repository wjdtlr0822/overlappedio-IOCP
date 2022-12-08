#include "IOCompletionPort.h";
#include <string>
#include <iostream>

const UINT16 MAX_CLIENT = 100;

int main() {
	IOCompletionPort iocompletionport;

	iocompletionport.InitSocket();
	iocompletionport.BindandListen();
	iocompletionport.StartServer(MAX_CLIENT);

	printf("click key\n");
	while (true) {
		std::string inputCmd;
		std::getline(std::cin, inputCmd);
		if (inputCmd == "quit") {
			break;
		}
	}
	iocompletionport.DestroyThread();
	return 0;
}

//tutorial2 1�ܰ���� ������ : 
//overlappedEx ����ü�� �ִ� char m_szBuf[MAX_SOCKBUF]�� stClientInfo ����ü�� �̵� �� �ڵ� �и�
//�� �ܰ迡�� overlappedEx����ü�� m_szBuf�� �־ ���ʿ��� �޸� ���� �߻�

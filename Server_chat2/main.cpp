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

//tutorial2 1단계와의 차이점 : 
//overlappedEx 구조체에 있는 char m_szBuf[MAX_SOCKBUF]를 stClientInfo 구조체로 이동 및 코드 분리
//앞 단계에는 overlappedEx구조체에 m_szBuf가 있어서 불필요한 메모리 낭비가 발생

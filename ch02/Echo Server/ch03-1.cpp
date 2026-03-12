#include <iostream>
#include <WS2tcpip.h> // 윈도우 소켓 라이브러리 헤더
#pragma comment (lib, "WS2_32.LIB") // 라이브러리 링크

const char* SERVER_ADDR = "127.0.0.1"; // 내 컴퓨터의 주소
constexpr short SERVER_PORT = 4000;
constexpr int BUFSIZE = 256;

int main()
{
	std::wcout.imbue(std::locale("korean")); // 에러 메세지 한글로 표시
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), nullptr);
	SOCKET s_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, 0);
	SOCKADDR_IN server_addr{}; // {} 0 으로 초기화
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_ADDR, &server_addr.sin_addr);
	WSAConnect(s_socket, reinterpret_cast<SOCKADDR*>(&server_addr), sizeof(server_addr), 0, 0, 0, 0);
	for (;;)
	{
		char buffer[BUFSIZE];
		std::string input;
		std::cout << "enter message to send: ";
		std::cin.getline(buffer, BUFSIZE);

		WSABUF wsa_buf{strlen(buffer) + 1, buffer}; // 버퍼에 써져있는 길이 만큼
		DWORD sent_size = 0;
		WSASend(s_socket, &wsa_buf, 1, &sent_size, 0, nullptr, nullptr);

		char recv_buffer[BUFSIZE]{};
		WSABUF recv_wsa_buf{ BUFSIZE, recv_buffer };
		DWORD recv_size = 0;
		DWORD recv_flag = 0;
		WSARecv(s_socket, &recv_wsa_buf, 1, &recv_size, &recv_flag, nullptr, nullptr);

		std::cout << "recv from server: " << recv_buffer << std::endl;
		std::cout << ", SIZE: " << recv_size << std::endl;
	}
}
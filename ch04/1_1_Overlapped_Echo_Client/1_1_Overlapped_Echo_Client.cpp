#include <iostream>
#include <WS2tcpip.h> // 윈도우 소켓 라이브러리 헤더
#pragma comment (lib, "WS2_32.LIB") // 라이브러리 링크

const char* SERVER_ADDR = "127.0.0.1"; // 내 컴퓨터의 주소
constexpr short SERVER_PORT = 4000;
constexpr int BUFSIZE = 256;

char g_recv_buffer[BUFSIZE];
char g_send_buffer[BUFSIZE];
SOCKET g_s_socket;
WSABUF g_recv_wsa_buf{ BUFSIZE, g_recv_buffer };
WSABUF g_send_wsa_buf{ BUFSIZE, g_send_buffer };
WSAOVERLAPPED g_send_overlapped{}, g_recv_overlapped{};
void send_to_server();
void send_callback();
void recv_callback();

void error_display(const wchar_t* msg, int err_no)
{
	WCHAR* lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err_no,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	std::wcout << msg << L" === 에러 " << lpMsgBuf << std::endl;
	while (true); // 디버깅 용
	LocalFree(lpMsgBuf);
}

void CALLBACK recv_callback(DWORD error, DWORD bytes_transferred, LPWSAOVERLAPPED overlapped, DWORD flags)
{
	if (error != 0)
	{
		error_display(L"데이터 수신 실패", WSAGetLastError());
		exit(1);
	}
	std::cout << "Received from server: SIZE: " << bytes_transferred << ", MESSAGE: " << g_recv_buffer << std::endl;
	send_to_server();
}
void CALLBACK send_callback(DWORD error, DWORD bytes_transferred, LPWSAOVERLAPPED overlapped, DWORD flags)
{
	if (error != 0)
	{
		error_display(L"데이터 전송 실패", WSAGetLastError());
		exit(1);
	}
	std::cout << "Sent to Server: SIZE: " << bytes_transferred << std::endl;

	DWORD recv_flag = 0;
	ZeroMemory(&g_recv_overlapped, sizeof(g_recv_overlapped));
	// overlapped를 클리어 해줘야함 why, 이상한값이 들어갈수있어 제대로 동작하지 않을 수 있음
	int result = WSARecv(g_s_socket, &g_recv_wsa_buf, 1, nullptr, &recv_flag, &g_recv_overlapped, recv_callback);
	if (result == SOCKET_ERROR)
	{
		int err_no = WSAGetLastError();
		if (err_no != WSA_IO_PENDING) // WSA_IO_PENDING이 아닐 경우에만 에러임
		{
			error_display(L"Recv 실패", WSAGetLastError());
			exit(1);
		}
	}
	// size를 nullptr로 해주면 리턴사이즈할 수 없어 즉시받지 않고 callback으로만 받음

}
void send_to_server()
{
	std::cout << "enter message to send: ";
	std::cin.getline(g_send_buffer, BUFSIZE);

	g_send_wsa_buf.len = static_cast<ULONG>(strlen(g_send_buffer) + 1); // 버퍼에 써져있는 길이 만큼
	ZeroMemory(&g_send_overlapped, sizeof(g_send_overlapped));
	DWORD sent_size = 0;
	int result = WSASend(g_s_socket, &g_send_wsa_buf, 1, &sent_size, 0, &g_send_overlapped, send_callback);
	if (result == SOCKET_ERROR)
	{
		error_display(L"데이터 전송 실패", WSAGetLastError());
		return;
	}
}

int main()
{
	std::wcout.imbue(std::locale("korean")); // 에러 메세지 한글로 표시
	WSADATA wsa_data{};
	WSAStartup(MAKEWORD(2, 2), &wsa_data);
	g_s_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	SOCKADDR_IN server_addr{}; // {} 0 으로 초기화
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_ADDR, &server_addr.sin_addr);
	int result = WSAConnect(g_s_socket, reinterpret_cast<SOCKADDR*>(&server_addr), sizeof(server_addr), 0, 0, 0, 0);
	if (result == SOCKET_ERROR)
	{
		error_display(L"서버 연결 실패", WSAGetLastError());
		return 1;
	}
	
	send_to_server();

	for (;;)
	{
		SleepEx(0, TRUE);
	}
	WSACleanup();
}
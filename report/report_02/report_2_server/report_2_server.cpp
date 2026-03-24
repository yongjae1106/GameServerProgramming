#include <iostream>
#include <WS2tcpip.h>
#pragma comment (lib, "WS2_32.LIB")
#define SIZE 8
#define EMPTY 0
#define P1 1

const short SERVER_PORT = 4000;
const int BUFSIZE = 256;
int board[SIZE][SIZE];

struct Pos
{
	int y;
	int x;
};

void resetboard()
{
	for (int y = 0; y < SIZE; y++)
	{
		for (int x = 0; x < SIZE; x++)
		{
			board[y][x] = EMPTY;
		}
	}
	board[SIZE / 2][SIZE / 2] = P1;
}
Pos find_location()
{
	for (int y = 0; y < SIZE; y++)
	{
		for (int x = 0; x < SIZE; x++)
		{
			if (board[y][x] == P1)
			{
				return { y, x };
			}
		}
	}
	return { -1, -1 };
}
void updateboard(int key, Pos p)
{
	int nx = p.x;
	int ny = p.y;

	switch (key)
	{
	case 72: // 상
	{
		ny--;
		break;
	}
	case 80: // 하
	{
		ny++;
		break;
	}
	case 75: // 좌
	{
		nx--;
		break;
	}
	case 77: // 우
	{
		nx++;
		break;
	}
	}

	// 이동 체크
	if ((nx >= 0 && nx < SIZE && ny >= 0 && ny < SIZE) && board[ny][nx] == EMPTY)
	{
		board[p.y][p.x] = EMPTY;
		board[ny][nx] = P1;
	}
}
int main()
{
	std::wcout.imbue(std::locale("korean"));
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 0), &WSAData);
	SOCKET s_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, 0);
	SOCKADDR_IN server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(s_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(s_socket, SOMAXCONN);
	INT addr_size = sizeof(server_addr);

	resetboard();
	std::cout << "board created!" << std::endl;

	std::cout << "Client Connecting..." << std::endl;
	SOCKET client = WSAAccept(s_socket, reinterpret_cast<sockaddr*>(&server_addr), &addr_size, 0, 0);
	if (client == INVALID_SOCKET)
	{
		std::cerr << "accept failed: " << WSAGetLastError() << std::endl;
		WSACleanup();
		return 1;
	}
	std::cout << "Client Connect!" << std::endl;

	int temp[SIZE * SIZE];

	for (int y = 0; y < SIZE; y++)
	{
		for (int x = 0; x < SIZE; x++)
		{
			temp[y * SIZE + x] = htonl(board[y][x]);
		}
	}
	WSABUF send_wsa_buf{ sizeof(temp), reinterpret_cast<char*>(temp) };
	DWORD sent;

	WSASend(client, &send_wsa_buf, 1, &sent, 0, 0, 0);

	Pos p;
	while (true)
	{
		int recv_buf;
		WSABUF recv_wsa_buf{ sizeof(int), reinterpret_cast<char*>(&recv_buf) };

		DWORD recv_size = 0;
		DWORD recv_flag = 0;
		WSARecv(client, &recv_wsa_buf, 1, &recv_size, &recv_flag, nullptr, nullptr);
		if (recv_size == 0)
		{
			std::cout << "Client Exit" << std::endl;
			closesocket(client);
			return 1;
		}
		else if (recv_size != sizeof(int)) continue;
		std::cout << "recieve " << recv_size << " byte!" << std::endl;

		p = find_location(); 
		if (p.x == -1) continue;
		std::cout << "P1 pos: " << p.x << " " << p.y << std::endl;

		int key = ntohl(recv_buf);
		if (key != 72 && key != 80 && key != 75 && key != 77) continue;
		updateboard(key, p);

		int temp[SIZE * SIZE];

		for (int y = 0; y < SIZE; y++)
		{
			for (int x = 0; x < SIZE; x++)
			{
				temp[y * SIZE + x] = htonl(board[y][x]);
			}
		}
		WSABUF send_wsa_buf{ sizeof(temp), reinterpret_cast<char*>(temp) };
		DWORD sent;

		WSASend(client, &send_wsa_buf, 1, &sent, 0, 0, 0);
	}
	WSACleanup();
}
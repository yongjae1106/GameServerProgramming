#include <iostream>
#include <WS2tcpip.h>
#pragma comment (lib, "WS2_32.LIB")
#define EMPTY 0
#define P1 1
#define P2 2

const int SIZE = 8;
const short SERVER_PORT = 4000;
const int BUFSIZE = 256;
int board[SIZE][SIZE];
int turn = 0;

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
	board[SIZE / 2 + 1][SIZE / 2] = P2;
}
Pos find_location(int player_id)
{
	for (int y = 0; y < SIZE; y++)
	{
		for (int x = 0; x < SIZE; x++)
		{
			if (board[y][x] == player_id)
			{
				return { y, x };
			}
		}
	}
	return { -1, -1 };
}
void updateboard(int key, int player_id, Pos p)
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
		board[ny][nx] = player_id;
		turn = (turn == 0) ? 1 : 0;
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

	SOCKET client[2];
	for(int i = 0; i < 2; i++)
	{
		client[i] = WSAAccept(s_socket, reinterpret_cast<sockaddr*>(&server_addr), &addr_size, 0, 0);

		int id = i + 1;
		int send_id = htonl(id);

		WSABUF buf{ sizeof(int), reinterpret_cast<char*>(&send_id) };
		DWORD sent;

		WSASend(client[i], &buf, 1, &sent, 0, 0, 0);

		std::cout << "클라이언트 " << id << "연결 됨" << std::endl;
	}

	fd_set rfds;
	Pos p[2];
	while (true)
	{
		FD_ZERO(&rfds);
		FD_SET(client[0], &rfds);
		FD_SET(client[1], &rfds);

		select(0, &rfds, nullptr, nullptr, nullptr);

		for (int i = 0; i < 2; i++)
		{
			if (i != turn) continue;

			if (FD_ISSET(client[i], &rfds))
			{
				int recv_buf;
				WSABUF recv_wsa_buf{ sizeof(int), reinterpret_cast<char*>(&recv_buf) };

				DWORD recv_size = 0;
				DWORD recv_flag = 0;
				WSARecv(client[i], &recv_wsa_buf, 1, &recv_size, &recv_flag, nullptr, nullptr);
				if (recv_size == 0)
				{
					std::cout << "클라이언트 종료" << std::endl;
					closesocket(client[i]);
					continue;
				}
				else if (recv_size != sizeof(int)) continue;

				int player_id = i + 1;
				p[i] = find_location(player_id);
				if (p[i].x == -1) continue;

				int key = ntohl(recv_buf);
				updateboard(key, player_id, p[i]);

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

				WSASend(client[0], &send_wsa_buf, 1, &sent, 0, 0, 0);
				WSASend(client[1], &send_wsa_buf, 1, &sent, 0, 0, 0);
			}
		}
	}
	WSACleanup();
}
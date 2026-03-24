#include <iostream>
#include <WS2tcpip.h> // 윈도우 소켓 라이브러리 헤더
#pragma comment (lib, "WS2_32.LIB") // 라이브러리 링크
#include <conio.h>
#define SIZE 8
#define EMPTY 0
#define P1 1

constexpr short SERVER_PORT = 4000;
constexpr int BUFSIZE = 256;
int board[SIZE][SIZE];

void drawboard()
{
    system("cls");

    for (int x = 0; x < SIZE; x++)
        std::cout << "+---";
    std::cout << "+\n";

    for (int y = 0; y < SIZE; y++)
    {
        for (int x = 0; x < SIZE; x++)
        {
            std::cout << "| ";
            if (board[y][x] == 1)
            {
                std::cout << "@";
            }
            else if (board[y][x] == 2)
            {
                std::cout << "#";
            }
            else
            {
                std::cout << " ";
            }
            std::cout << " ";
        }
        std::cout << "|\n";

        for (int x = 0; x < SIZE; x++)
            std::cout << "+---";
        std::cout << "+\n";
    }
    std::cout << "\n press to move (ESC: exit)" << std::endl;
}

int main()
{
    char SERVER_ADDR[32];
    std::cout << "Enter IP: ";
    std::cin >> SERVER_ADDR;

    std::wcout.imbue(std::locale("korean")); // 에러 메세지 한글로 표시
    WSADATA wsa_data{};
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
    SOCKET s_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, 0);
    SOCKADDR_IN server_addr{}; // {} 0 으로 초기화
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_ADDR, &server_addr.sin_addr);
    int result = WSAConnect(s_socket, reinterpret_cast<SOCKADDR*>(&server_addr), sizeof(server_addr), 0, 0, 0, 0);

    int temp[SIZE * SIZE];
    WSABUF recv_wsa_buf{ sizeof(temp), reinterpret_cast<char*>(temp) };
    DWORD recv_size = 0;
    DWORD recv_flag = 0;
    WSARecv(s_socket, &recv_wsa_buf, 1, &recv_size, &recv_flag, nullptr, nullptr);

    for (int y = 0; y < SIZE; y++)
    {
        for (int x = 0; x < SIZE; x++)
        {
            board[y][x] = ntohl(temp[y * SIZE + x]);
        }
    }

    drawboard();

    for (;;)
    {
        int key = 0;
        key = _getch();

        // ESC : 27
        if (key == 27)
        {
            break;
        }
        else if (key == 224)
        {
            key = _getch();
        }
        int send_key = htonl(key);
        WSABUF send_wsa_buf{ sizeof(int), reinterpret_cast<char*>(&send_key) };
        DWORD sent_size = 0;
        WSASend(s_socket, &send_wsa_buf, 1, &sent_size, 0, nullptr, nullptr);

        WSARecv(s_socket, &recv_wsa_buf, 1, &recv_size, &recv_flag, nullptr, nullptr);
        if (recv_size != sizeof(temp)) continue;

        for (int y = 0; y < SIZE; y++)
        {
            for (int x = 0; x < SIZE; x++)
            {
                board[y][x] = ntohl(temp[y * SIZE + x]);
            }
        }
        drawboard();
    }
    WSACleanup();


    return 0;
}
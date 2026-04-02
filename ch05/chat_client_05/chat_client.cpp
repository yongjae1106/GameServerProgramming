#include <iostream>
#include <string>
#include <thread>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

const char* SERVER_IP = "127.0.0.1";
constexpr short SERVER_PORT = 3500;
constexpr int BUFFER_SIZE = 200;

char g_recv_buffer[BUFFER_SIZE];
char g_send_buffer[BUFFER_SIZE];
WSABUF g_recv_wsa_buf{ BUFFER_SIZE, g_recv_buffer };
WSABUF g_send_wsa_buf{ BUFFER_SIZE, g_send_buffer };
WSAOVERLAPPED g_recv_overlapped{}, g_send_overlapped{};
SOCKET g_s_socket;

class PACKET {
public:
    unsigned char m_size;
    unsigned char m_sender_id;
    char m_buf[BUFFER_SIZE];
    PACKET(int sender, char* mess) : m_sender_id(sender)
    {
        m_size = static_cast<int>(strlen(mess) + 3);
        strcpy_s(m_buf, mess);
    }
};

void error_display(const wchar_t* msg, int err_no)
{
    WCHAR* lpMsgBuf;
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL, err_no, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf, 0, NULL);
    std::wcout << msg << L" === 에러 " << lpMsgBuf << std::endl;
    LocalFree(lpMsgBuf);
}

void recv_from_server();

void CALLBACK recv_callback(DWORD error, DWORD bytes_transferred, LPWSAOVERLAPPED overlapped, DWORD flags)
{
    if (error != 0) {
        error_display(L"데이터 수신 실패", WSAGetLastError());
        exit(1);
    }

    PACKET* packet = reinterpret_cast<PACKET*>(g_recv_buffer);
    int remain = bytes_transferred;
    while (remain > 0) {
        int id = packet->m_sender_id;
        std::cout << "Client [" << id << "] " << packet->m_buf << std::endl;
        remain -= packet->m_size;
        packet = packet + packet->m_size;
    }
    recv_from_server();
}

void CALLBACK send_callback(DWORD error, DWORD bytes_transferred, LPWSAOVERLAPPED overlapped, DWORD flags)
{
    if (error != 0) {
        error_display(L"데이터 전송 실패", WSAGetLastError());
        return;
    }
    std::cout << "Sent to server: SIZE: " << bytes_transferred << std::endl;
}

void recv_from_server()
{
    DWORD recv_flag = 0;
    ZeroMemory(&g_recv_overlapped, sizeof(g_recv_overlapped));
    int result = WSARecv(g_s_socket, &g_recv_wsa_buf, 1, nullptr, &recv_flag, &g_recv_overlapped, recv_callback);
    if (result == SOCKET_ERROR) {
        int err_no = WSAGetLastError();
        if (err_no != WSA_IO_PENDING) {
            error_display(L"데이터 수신 실패", err_no);
            exit(1);
        }
    }
}

int main()
{
    std::wcout.imbue(std::locale("korean"));

    WSADATA wsa_data{};
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
    g_s_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);

    SOCKADDR_IN server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    int result = WSAConnect(g_s_socket, reinterpret_cast<SOCKADDR*>(&server_addr), sizeof(server_addr), nullptr, nullptr, nullptr, nullptr);
    if (result == SOCKET_ERROR) {
        error_display(L"서버 연결 실패", WSAGetLastError());
        return 1;
    }

    recv_from_server();

    std::cout << "서버에 연결되었습니다. 메시지를 입력하세요 (종료: quit)\n";

    // 입력 루프 - 메인 스레드에서 콘솔 입력 처리
    while (true) {
        std::string message;
        std::getline(std::cin, message);

        if (message == "quit") break;
        if (message.empty()) continue;

        g_send_wsa_buf.len = static_cast<ULONG>(message.size() + 1);
        memcpy(g_send_wsa_buf.buf, message.c_str(), g_send_wsa_buf.len);
        ZeroMemory(&g_send_overlapped, sizeof(g_send_overlapped));

        DWORD sent_size = 0;
        int send_result = WSASend(g_s_socket, &g_send_wsa_buf, 1, &sent_size, 0, &g_send_overlapped, send_callback);
        if (send_result == SOCKET_ERROR) {
            int err_no = WSAGetLastError();
            if (err_no != WSA_IO_PENDING) {
                error_display(L"데이터 전송 실패", err_no);
                break;
            }
        }

        SleepEx(0, TRUE); // 콜백 처리
    }

    closesocket(g_s_socket);
    WSACleanup();
    return 0;
}
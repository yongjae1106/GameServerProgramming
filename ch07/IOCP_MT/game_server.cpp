#include <iostream>
#include <WS2tcpip.h>
#include <array>
#include <MSWSock.h>
#include <thread>
#include <vector>
#include <time.h>
#include "protocol.h"
#include <tbb/concurrent_unordered_map.h>

#pragma comment(lib, "MSWSock.lib")
#pragma comment(lib, "WS2_32.lib")
using namespace std;
constexpr int BUF_SIZE = 200;

std::atomic<int> player_index = 0;

enum IOType { IO_SEND, IO_RECV, IO_ACCEPT };

class EXP_OVER {
public:
	WSAOVERLAPPED m_over;
	IOType  m_iotype;
	WSABUF	m_wsa;
	SOCKET  m_client_socket;
	char  m_buff[BUF_SIZE];
	EXP_OVER()
	{
		ZeroMemory(&m_over, sizeof(m_over));
		m_wsa.buf = m_buff;
		m_wsa.len = BUF_SIZE;
	}
	EXP_OVER(IOType iot) : m_iotype(iot)
	{
		ZeroMemory(&m_over, sizeof(m_over));
		m_wsa.buf = m_buff;
		m_wsa.len = BUF_SIZE;
	}
};

HANDLE h_iocp;
SOCKET server;
SOCKET client_socket;
EXP_OVER accept_over(IO_ACCEPT);

void error_display(const wchar_t* msg, int err_no)
{
	WCHAR* lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err_no,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	std::wcout << msg;
	std::wcout << L" === 에러 " << lpMsgBuf << std::endl;
	while (true);   // 디버깅 용
	LocalFree(lpMsgBuf);
}

enum CL_STATE { CS_CONNECT, CS_PLAYING, CS_LOGOUT };

class SESSION {
public:
	SOCKET m_client;
	int m_id;
	CL_STATE m_state;
	EXP_OVER m_recv_over;
	int m_prev_recv;
	char m_username[MAX_NAME_LEN];
	short m_x, m_y; 
	int m_move_time;

	SESSION() {
		std::cout << "SESSION Creation Error!\n";	
		exit(-1);
	}
	SESSION(SOCKET s, int id) : m_client(s), m_id(id) {
		m_state = CS_CONNECT;
		m_recv_over.m_iotype = IO_RECV;
		m_x = 0; 		m_y = 0;
		m_prev_recv = 0;
	}
	~SESSION()
	{
		if (m_state != CS_LOGOUT)
			closesocket(m_client);
	}
	void do_recv()
	{
		DWORD recv_flag = 0;
		memset(&m_recv_over.m_over, 0, sizeof(m_recv_over.m_over));
		m_recv_over.m_wsa.buf = m_recv_over.m_buff + m_prev_recv;		// 남은 패킷을 위해 버퍼를 덮어씌우지 않고 이어서 작동
		m_recv_over.m_wsa.len = BUF_SIZE - m_prev_recv;					// 길이도 남은 만큼으로 조정
		WSARecv(m_client, &m_recv_over.m_wsa, 1, 0, &recv_flag, &m_recv_over.m_over, nullptr);
	}
	void do_send(int num_bytes, char* mess)
	{
		EXP_OVER* o = new EXP_OVER(IO_SEND);
		o->m_wsa.len = num_bytes;
		memcpy(o->m_buff, mess, num_bytes);
		WSASend(m_client, &o->m_wsa, 1, 0, 0, &o->m_over, nullptr);
	}
	void send_avatar_info()
	{
		S2C_AvatarInfo packet;
		packet.size = sizeof(S2C_AvatarInfo);
		packet.type = S2C_AVATAR_INFO;
		packet.playerId = m_id;
		packet.x = m_x;
		packet.y = m_y;
		do_send(packet.size, reinterpret_cast<char*>(&packet));
	}
	void send_move_packet(int mover);
	void send_add_player(int player_id);
	void send_login_success()
	{
		S2C_LoginResult packet;
		packet.size = sizeof(S2C_LoginResult);
		packet.type = S2C_LOGIN_RESULT;
		packet.success = true;
		strncpy_s(packet.message, "Login successful.", sizeof(packet.message));
		do_send(packet.size, reinterpret_cast<char*>(&packet));
	}
	void send_remove_player(int player_id)
	{
		S2C_RemovePlayer packet;
		packet.size = sizeof(S2C_RemovePlayer);
		packet.type = S2C_REMOVE_PLAYER;
		packet.playerId = player_id;
		do_send(packet.size, reinterpret_cast<char*>(&packet));
	}
	bool process_packet(unsigned char* p);
	void do_move(DIRECTION dir);
};

tbb::concurrent_unordered_map<int, 
	std::atomic<std::shared_ptr<SESSION>>> clients;

void SESSION::send_add_player(int player_id)
{
	S2C_AddPlayer packet;
	packet.size = sizeof(S2C_AddPlayer);
	packet.type = S2C_ADD_PLAYER;
	packet.playerId = player_id;
	std::shared_ptr<SESSION> pl = clients[player_id];
	if (pl == nullptr) return;
	memcpy(packet.username, pl->m_username, sizeof(packet.username));
	packet.x = pl->m_x;
	packet.y = pl->m_y;
	do_send(packet.size, reinterpret_cast<char*>(&packet));
}

bool SESSION::process_packet(unsigned char* p)
{
	PACKET_TYPE type = *reinterpret_cast<PACKET_TYPE*>(&p[1]);
	switch (type) {
	case C2S_LOGIN: {
		C2S_Login* packet = reinterpret_cast<C2S_Login*>(p);
		strncpy_s(m_username, packet->username, MAX_NAME_LEN);
		m_state = CS_PLAYING;
		cout << "Player[" << m_id << "] logged in as " << m_username << endl;
		send_avatar_info();
		for (auto& cl : clients) {
			std::shared_ptr<SESSION> fair = cl.second.load();
			if (fair == nullptr) continue;
			if (fair->m_state == CS_LOGOUT) continue;
			if (fair->m_id == m_id) continue;
			fair->send_add_player(m_id);
		}
		break;
	}
	case C2S_MOVE: 
	{
		C2S_Move* packet = reinterpret_cast<C2S_Move*>(p);
		DIRECTION dir = packet->dir;
		m_move_time = packet->move_time;
		do_move(dir);
		break;
	}
	default:
		cout << "Unknown packet type received from player[" << m_id << "].\n";
		return false;
		break;
	}
	return true;
}

void SESSION::do_move(DIRECTION dir)
{
	bool block;
	short new_x = m_x, new_y = m_y;
	switch (dir) {
	case UP: new_y = max(0, m_y - 1); break;
	case DOWN: new_y = min(WORLD_HEIGHT - 1, m_y + 1); break;
	case LEFT: new_x = max(0, m_x - 1); break;
	case RIGHT: new_x = min(WORLD_WIDTH - 1, m_x + 1); break;
	}
	//cout << "Player[" << m_id << "] moved to (" << m_x << ", " << m_y << ")\n";
	for (auto& pair : clients) // 충돌체크
	{
		int id = pair.first;
		std::shared_ptr<SESSION> cl = pair.second.load();
		if (cl == nullptr) continue;
		if (cl->m_state == CS_LOGOUT) continue;
		if (cl->m_x == new_x && cl->m_y == new_y)
		{
			block = true;
			break;
		}
	}
	if (!block)
	{
		m_x = new_x;
		m_y = new_y;

		for (auto& pair : clients)
		{
			int id = pair.first;
			std::shared_ptr<SESSION> cl = pair.second.load();
			if (cl == nullptr) continue;
			if (cl->m_state != CS_LOGOUT)
				cl->send_move_packet(m_id);
		}
	}

}
void SESSION::send_move_packet(int mover)
{
	S2C_MovePlayer packet;
	packet.size = sizeof(S2C_MovePlayer);
	packet.type = S2C_MOVE_PLAYER;
	packet.playerId = mover;
	packet.x = clients[mover].load()->m_x;
	packet.y = clients[mover].load()->m_y;
	packet.move_time = clients[mover].load()->m_move_time;
	do_send(packet.size, reinterpret_cast<char*>(&packet));
}

void send_login_fail(SOCKET client, const char* message)
{
	S2C_LoginResult packet;
	packet.size = sizeof(S2C_LoginResult);
	packet.type = S2C_LOGIN_RESULT;
	packet.success = false;
	strncpy_s(packet.message, message, sizeof(packet.message));
	WSABUF wsa_buf;
	wsa_buf.buf = reinterpret_cast<char*>(&packet);
	wsa_buf.len = packet.size;
	WSASend(client, &wsa_buf, 1, 0, 0, nullptr, nullptr);
}

void disconnect(int key)
{
	std::shared_ptr<SESSION> cl = clients[key].load();
	if (nullptr != cl)
	{
		cl->m_state = CS_LOGOUT;
		for (auto& other : clients)
		{
			std::shared_ptr<SESSION> o = other.second.load();
			if (nullptr == o) continue;
			if (CS_PLAYING == o->m_state)
				o->send_remove_player(key);
		}
		closesocket(cl->m_client);
		cl->m_client = INVALID_SOCKET;
	}
	clients[key].store(nullptr);
}
void worker_thread()
{
	for (;;) 
	{
		DWORD num_bytes;
		ULONG_PTR key;
		LPOVERLAPPED over;
		bool result = GetQueuedCompletionStatus(h_iocp, &num_bytes, &key, &over, INFINITE);
		if (over == nullptr || (over != nullptr && result == false))
		{
			std::cout << "client[" << key << "] Disconnected.\n";
			disconnect(key);
			continue;
		}	// 접속 종료 처리
		EXP_OVER* exp_over = reinterpret_cast<EXP_OVER*>(over);

		switch (exp_over->m_iotype) {
		case IO_ACCEPT:
		{
			cout << "Client connected." << endl;
			int my_id = player_index++;
			if (MAX_PLAYERS <= clients.size()) {
				cout << "No more player can be accepted." << endl;
				send_login_fail(exp_over->m_client_socket, "Server is full.");
				closesocket(exp_over->m_client_socket);
			}
			else 
			{
				auto new_session = std::make_shared<SESSION>(exp_over->m_client_socket, my_id);
				for (;;) // 랜덤 위치 설정
				{
					short rx = rand() % WORLD_WIDTH;
					short ry = rand() % WORLD_HEIGHT;
					bool overlap = false;
					for (auto& fair : clients)
					{
						std::shared_ptr<SESSION> cl = fair.second.load();
						if (cl == nullptr) continue;
						if (cl->m_state == CS_LOGOUT) continue;
						if (cl->m_x == rx && cl->m_y == ry)
						{
							overlap = true;
							break;
						}
					}
					if (!overlap)
					{
						new_session->m_x = rx;
						new_session->m_y = ry;
						break;
					}
				}
				clients[my_id].store(new_session); // atomic선언이라 직접 멤버 접근 불가 -> new_session을 통해 입력
				CreateIoCompletionPort((HANDLE)client_socket, h_iocp, my_id, 0);
				new_session->send_login_success();

				for (auto& other : clients)
				{
					std::shared_ptr<SESSION> o = other.second.load();
					if (o == nullptr) continue;
					if (o->m_state == CS_LOGOUT) continue;
					if (o->m_id == my_id) continue;
					o->send_add_player(my_id);
					new_session->send_add_player(o->m_id);
				}
				new_session->do_recv();
			}
			client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			accept_over.m_client_socket = client_socket;
			AcceptEx(server, client_socket, &accept_over.m_buff, 0,
				sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
				NULL, &accept_over.m_over);
			break;
		}
		case IO_RECV:
		{
			int client_index = static_cast<int>(key);
			if (num_bytes == 0)
			{
				std::cout << "client[" << key << "] Disconnected.\n";
				disconnect(key);
				break;
			}

			//cout << "Client[" << client_index << "] sent a message." << endl;
			std::shared_ptr<SESSION> cl = clients[client_index];
			if (cl == nullptr) break;
			unsigned char* p = reinterpret_cast<unsigned char*>(exp_over->m_buff);
			int data_size = num_bytes + cl->m_prev_recv;
			while (data_size > 0) 
			{
				int packet_size = p[0];
				if (packet_size > data_size) break;
				if (false == cl->process_packet(p))
				{
					disconnect(key);
					break;
				}
				cl->process_packet(p);
				p += packet_size;
				data_size -= packet_size;
			}
			if (data_size >= 0) {
				memmove(cl->m_recv_over.m_buff, p, data_size);
				cl->m_prev_recv = data_size;
			}
			cl->do_recv();
			
			break;
		}
		case IO_SEND: {
			//cout << "Message sent. to client[" << key << "]\n";
			EXP_OVER* o = reinterpret_cast<EXP_OVER*>(over);
			delete o;
			break;
		}
		default:
			cout << "Unknown IO type." << endl;
			exit(-1);
			break;
		}
	}
}

int main()
{
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);
	server = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;
	int result = ::bind(server, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	if (SOCKET_ERROR == result)
	{
		cout << "bind 실패: " << WSAGetLastError() << endl;
	}
	result = ::listen(server, SOMAXCONN);
	if (SOCKET_ERROR == result)
	{
		cout << "listen 실패: " << WSAGetLastError() << endl;
	}
	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	CreateIoCompletionPort((HANDLE)server, h_iocp, -1, 0);

	client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	accept_over.m_client_socket = client_socket;
	AcceptEx(server, client_socket, &accept_over.m_buff, 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
		NULL, &accept_over.m_over);

	vector <thread> worker_threads;
	int num_threads = thread::hardware_concurrency();

	for (int i = 0; i < num_threads; ++i)
		worker_threads.emplace_back(worker_thread);
	for (auto &th : worker_threads)
		th.join();

	closesocket(server);
	WSACleanup();
}

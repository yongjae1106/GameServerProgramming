#include <iostream>
#include <WS2tcpip.h>
#include <unordered_map> // 세션 여러개를 저장해야함
#include <time.h>
#pragma comment(lib, "WS2_32.lib")
#define BOARD_SIZE 9
#define EMPTY 0

using namespace std;

constexpr int PORT_NUM = 3500;
constexpr int BUF_SIZE = 350;

int board[BOARD_SIZE][BOARD_SIZE];

void CALLBACK recv_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED over, DWORD flags);
void CALLBACK send_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED over, DWORD flags);

class EXP_OVER {
public:
	WSAOVERLAPPED m_over;
	WSABUF	m_wsa[1];
	char  m_buff[BUF_SIZE];	
	EXP_OVER(int num_bytes)
	{
		ZeroMemory(&m_over, sizeof(m_over));
		m_wsa[0].buf = m_buff;
		m_wsa[0].len = num_bytes;
		memcpy(m_buff, board, sizeof(board));  // board를 그대로 복사
	}
};

class SESSION;
unordered_map<long long, SESSION> clients;
class SESSION {
	SOCKET client;
	WSAOVERLAPPED c_over;
	WSABUF c_wsabuf[1];
	int m_id;
	int last_key;
	int x, y;
public:
	CHAR c_mess[BUF_SIZE]; // 실제 데이터가 들어갈 버퍼
	SESSION() { exit(-1); }
	SESSION(int id, SOCKET so) : m_id(id), client(so)
	{
		c_wsabuf[0].buf = c_mess;
		for (;;)
		{
			int randx = rand() % BOARD_SIZE;
			int randy = rand() % BOARD_SIZE;
			if (board[randy][randx] == EMPTY)
			{
				board[randy][randx] = m_id; 
				x = randx;                   
				y = randy;
				break;
			}
		}
		send(client, reinterpret_cast<char*>(&m_id), sizeof(m_id), 0); // id 전송용 (일회용)
	}
	~SESSION()
	{
		closesocket(client);
	}
	void do_recv()
	{
		c_wsabuf[0].len = BUF_SIZE;
		DWORD recv_flag = 0;
		memset(&c_over, 0, sizeof(c_over));
		c_over.hEvent = reinterpret_cast<HANDLE>(m_id);
		WSARecv(client, c_wsabuf, 1, 0, &recv_flag, &c_over, recv_callback);
	}
	void do_send()
	{
		EXP_OVER* o = new EXP_OVER(sizeof(board));
		WSASend(client, o->m_wsa, 1, 0, 0, &o->m_over, send_callback);
	}
	int get_id()
	{
		return m_id;
	}
	int get_x()
	{
		return x;
	}
	int get_y()
	{
		return y;
	}
	int get_last_key()
	{
		return last_key;
	}
	void set_last_key(int _last_key)
	{
		last_key = _last_key;
	}
	void updateboard(int key)
	{
		int nx = x;
		int ny = y;

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
		if ((nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE) && board[ny][nx] == EMPTY)
		{
			board[y][x] = EMPTY;
			board[ny][nx] = m_id;
			y = ny;
			x = nx;
		}
	}

};

void resetboard()
{
	for (int y = 0; y < BOARD_SIZE; y++)
	{
		for (int x = 0; x < BOARD_SIZE; x++)
		{
			board[y][x] = EMPTY;
		}
	}
}

void CALLBACK recv_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED over, DWORD flags)
{
	int client_id = static_cast<int>(reinterpret_cast<long long>(over->hEvent));
	if (err != 0 || num_bytes == 0)
	{
		board[clients[client_id].get_y()][clients[client_id].get_x()] = EMPTY; // 보드에서 지움

		clients.erase(client_id); // unordered_map 에서 삭제
		
		cout << "Client[" << client_id << "] exit" << endl;

		for (auto& cl : clients)
			cl.second.do_send();

		return;
	}
	cout << "Client[" << client_id << "] sent: " << num_bytes << " bytes" << endl;

	int key = *reinterpret_cast<int*>(clients[client_id].c_mess); // 키 int로 파싱
	clients[client_id].set_last_key(key);
	clients[client_id].updateboard(key);

	for (auto& cl : clients) // 모든 클라이언트에게
		cl.second.do_send();
	clients[client_id].do_recv(); // 다시 리시브 해줌
}

void CALLBACK send_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED over, DWORD flags)
{
	EXP_OVER* o = reinterpret_cast<EXP_OVER*>(over); // delete할 over의 주소를 얻음
	delete over;  // send가 종료되었으니 delete해줌
}

int main()
{
	srand(time(nullptr));
	resetboard();
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);
	SOCKET server = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM);
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;
	bind(server, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(server, SOMAXCONN);
	SOCKADDR_IN cl_addr;
	int addr_size = sizeof(cl_addr);
	for (int i = 1; ; ++i) {
		SOCKET client = WSAAccept(server,
			reinterpret_cast<sockaddr*>(&cl_addr), &addr_size, NULL, NULL);
		clients.try_emplace(i, i, client);
		clients[i].do_recv();
	}
	closesocket(server);
	WSACleanup();
}

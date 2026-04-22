#include <iostream>
#include <WS2tcpip.h>
#include <unordered_map> // 세션 여러개를 저장해야함
#include <MSWSock.h>
#pragma comment(lib, "MSWSock.lib")
#pragma comment(lib, "WS2_32.lib")
using namespace std;
constexpr int PORT_NUM = 3500;
constexpr int BUF_SIZE = 200;

enum IOType { IO_SEND, IO_RECV, IO_ACCEPT };

class EXP_OVER {
public:
	WSAOVERLAPPED m_over;
	IOType m_iotype;
	WSABUF	m_wsa;
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

class SESSION;
unordered_map<long long, SESSION> clients;
class SESSION {
	SOCKET client;
	long long m_id;
public:
	EXP_OVER recv_over;
	SESSION() { exit(-1); }
	SESSION(int id, SOCKET so) : m_id(id), client(so)
	{
		recv_over.m_iotype = IO_RECV; // IO_RECV로 리시브용 iotype을 생성해줘야함
	}
	~SESSION()
	{
		closesocket(client);
	}
	void do_recv()
	{
		DWORD recv_flag = 0;
		memset(&recv_over.m_over, 0, sizeof(recv_over.m_over));
		WSARecv(client, &recv_over.m_wsa, 1, 0, &recv_flag, &recv_over.m_over, nullptr);
	}
	void do_send(int sender_id, int num_bytes, char* mess)
	{
		EXP_OVER* o = new EXP_OVER(IO_SEND);
		o->m_buff[0] = num_bytes + 2;
		o->m_buff[1] = sender_id;
		memcpy(o->m_buff + 2, mess, num_bytes);
		o->m_wsa.len = o->m_buff[0];
		WSASend(client, &o->m_wsa, 1, 0, 0, &o->m_over, nullptr);
	}
};
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

int main()
{
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

	HANDLE h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, NULL);
	CreateIoCompletionPort((HANDLE)server, h_iocp, 0, 0);
	
	SOCKET client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	EXP_OVER accept_over(IO_ACCEPT);
	AcceptEx(server, client_socket, accept_over.m_buff, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, NULL, &accept_over.m_over);
	// 서버 주소 sizeof(SOCKADDR_IN) + 16 / accept 할때 컴퓨터가 여러개의 주소를 가지고있기 때문에 어떤 네트워크카드를 통해 접속하였는지에 따라 다르기때문에

	for (int i = 0;;)
	{
		DWORD num_bytes;
		ULONG_PTR key;
		LPOVERLAPPED over;
		GetQueuedCompletionStatus(h_iocp, &num_bytes, &key, &over, INFINITE);
		if (over == nullptr)
		{
			error_display(L"GQCS Error: ", WSAGetLastError());
			continue;
		}
		EXP_OVER* exp_over = reinterpret_cast<EXP_OVER*>(over);
		switch (exp_over->m_iotype)
		{
		case IO_ACCEPT:
		{
			cout << "" << endl;
			CreateIoCompletionPort((HANDLE)client_socket, h_iocp, i, 0);
			clients.try_emplace(i, i, client_socket);
			clients[i].do_recv();
			i++; // ACCEPT를 받았을 경우에만 증감식 적용
			client_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			AcceptEx(server, client_socket, accept_over.m_buff, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, NULL, &accept_over.m_over);

			break;
		}
		case IO_RECV:
		{
			cout << "Receive" << endl;
			int client_id = static_cast<int>(key);
			cout << "Client[" << client_id << "] sent: " << clients[client_id].recv_over.m_buff << endl;
			for (auto& cl : clients) // 모든 클라이언트에게
				cl.second.do_send(client_id, num_bytes, clients[client_id].recv_over.m_buff);
			clients[client_id].do_recv(); // 다시 리시브 해줌
			break;
		}
		case IO_SEND:
		{
			cout << "Message Send" << endl;
			EXP_OVER* o = reinterpret_cast<EXP_OVER*>(over); // delete할 over의 주소를 얻음
			delete over;  // send가 종료되었으니 delete해줌
			break;
		}
		}
	}
	SOCKADDR_IN cl_addr;
	int addr_size = sizeof(cl_addr);
	closesocket(server);
	WSACleanup();
}

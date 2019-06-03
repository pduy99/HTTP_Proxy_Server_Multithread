// Proxy_Server.cpp : Defines the entry point for the console application.
//

//Khai bao thu vien
#include "stdafx.h"
#include "Proxy_Server.h"
#include "afxsock.h"
#include <string>
#include <vector>
#include <sstream>



#ifdef _DEBUG
#define new DEBUG_NEW
#endif


/* Define mot so tu khoa */
#define HTTP_PORT 80
#define Proxy_PORT 8888
#define BSIZE 10000
#define USER_AGENT "HTMLGET 1.0"
#define PAGE "/"
#define HTTP "http://"
#define BlacklistFile "blacklist.conf" 




using namespace std;


struct Socket_Param
{
	string hostAddr;
	string page;
	HANDLE handle;
	SOCKET client;
	SOCKET server;
};

/*Khai bao cac ham */
//Ham khoi tao Server
void KhoiTaoServer();
//Ham giao tiep giua Client va Proxy
UINT ClientToProxy(LPVOID pParam);
//Ham giao tiep giua Proxy va remote Server
UINT ProxytoRemoteServer(LPVOID pParam);
//Ham dong ket noi
void CloseServer();
//Ham lay host va page tu truy van cua Client
void getHostNPage(string buff, string &host, string &page);
//Ham tao querry tu truy van Client
char* build_GET_querry(const char* host, const char* page);
//Ham lay IP tu host
char *get_ip(const char *host);
//Ham Check Blacklist
bool isInBlacklist(string host);
//Ham chuyen doi char* sang LPCWSTR
wchar_t *convertCharArrayToLPCWSTR(const char* charArray);


string ForbiddenRequest =  "HTTP/1.0 403 Forbidden\r\nContent-Type: text/html\r\n\r\n<!doctype html><html><body>You cannot access this website !</body></html>";

// The one and only application object

CWinApp theApp;


int main()
{
    int nRetCode = 0;

    HMODULE hModule = ::GetModuleHandle(nullptr);

    if (hModule != nullptr)
    {
        // initialize MFC and print and error on failure
        if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
        {
            // TODO: change error code to suit your needs
            wprintf(L"Fatal Error: MFC initialization failed\n");
            nRetCode = 1;
        }
        else
        {
            // TODO: code your application's behavior here.
			
			KhoiTaoServer();
			while (true)
			{
				Sleep(2000);
			}

			
        }
    }
    else
    {
        // TODO: change error code to suit your needs
        wprintf(L"Fatal Error: GetModuleHandle failed\n");
        nRetCode = 1;
    }

    return nRetCode;
}

void KhoiTaoServer()
{
	sockaddr_in address;
	SOCKET serverfd;
	WSADATA wsaData;
	
	if (WSAStartup(0x202, &wsaData) != 0)
	{
		cout << "WSAStartup Failed with error "<<WSAGetLastError();
		WSACleanup();
		exit(0);
	}
	
	address.sin_family = AF_INET; //IPv4
	address.sin_port = htons(Proxy_PORT); //Port
	address.sin_addr.s_addr = INADDR_ANY; //Local IP

	
	//Tạo Socket descriptor
	if ((serverfd = socket(AF_INET, SOCK_STREAM, 0)) == 0) 
	{
		cout << "Socket failed";
		exit(0);
	}

	//Sử dụng lại Port 
	char opt = '1';
	int iRes = setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (iRes == SOCKET_ERROR)
	{
		cout << "setsockopt error" << endl;
		exit(0);
	}

	//bind Socket 
	if (bind(serverfd, (sockaddr*)&address, sizeof(address)) == SOCKET_ERROR)
	{
		cout << "Bind failed with error " << WSAGetLastError();
		exit(0);
	}
	cout << "Waiting for incoming connections..." << endl;
	//Lắng nghe các kết nối
	if (listen(serverfd, 5) < 0) //Lắng nghe tối đa 5 kết nối
	{
		cout << "Listen failed";
		exit(0);
	}

	//Khởi tạo Thread lắng nghe kết nối từ Client đến Proxy Server
	AfxBeginThread(ClientToProxy, (LPVOID)serverfd);
}

UINT ClientToProxy(LPVOID pParam)
{
	SOCKET server = (SOCKET)pParam;
	if (server == NULL)
	{
		return -1; //Failed 
	}

	SOCKET new_socket;
	sockaddr_in addr;
	int addrlen = sizeof(addr);
	
	//Truy cap moi
	new_socket = accept(server, (sockaddr*)&addr, &addrlen);
	cout << "Connect accepted" << endl;

	//Khởi tạo một thread để tiếp tục lắng nghe các kết nối khác
	AfxBeginThread(ClientToProxy, pParam);

	//Nhận truy vấn từ Client
	char client_sent[BSIZE];
	int valnum; //so byte nhan duoc tu Client
	memset(client_sent, 0, BSIZE);
	valnum = recv(new_socket, client_sent, BSIZE, 0);
	if (valnum < 0)
	{
		cout << "Received error";
		exit(0);
	}
	if (valnum == 0)
	{
		cout << "Client dong ket noi...";
		exit(0);
	}

	//Xuat cac truy van
	cout << "Client sent: " << endl;
	cout << client_sent;

	//Lay host va kiem tra host co trong black list khong
	bool check = FALSE;
	string host, page;
	getHostNPage(client_sent, host, page);
	if (isInBlacklist(host) && host !="")
	{
		cout << "403 Error - Forbiden" << endl;
		valnum = send(new_socket, ForbiddenRequest.c_str(), ForbiddenRequest.size(),0); // Gui Code 403 Forbidden den Client
		check = TRUE;
		Sleep(2000);
	}
	
	//Gán dữ liệu cho Socket_Param
	Socket_Param S;
	S.server = server;
	S.client = new_socket;
	S.hostAddr = host;
	S.page = page;
	S.handle = CreateEvent(NULL, TRUE, FALSE, NULL);
	
	//Bắt đầu thread giao tiếp giữa Proxy và Web
	if (check == FALSE)
	{
		AfxBeginThread(ProxytoRemoteServer, (LPVOID)&S);
		//Đợi cho Proxy kết nối đến Web 
		WaitForSingleObject(S.handle, 5000);
		CloseHandle(S.handle);


	}
	return 0;
}

UINT ProxytoRemoteServer(LPVOID pParam)
{
	Socket_Param* S = (Socket_Param*)pParam;
	string host = S->hostAddr;
	string page = S->page;
	SOCKET client = S->client;
	SOCKET server = S->server;
	int port = HTTP_PORT;
	
	char* IP;
	char * get_query;
	int valnum;
	char* buf=NULL;

	IP = get_ip(host.c_str());
	get_query = build_GET_querry(host.c_str(), page.c_str());

	//Khoi tao socket client
	CSocket s_client;
	AfxSocketInit(NULL);
	s_client.Create();
	//Ket noi den Remote Server
	if (s_client.Connect(convertCharArrayToLPCWSTR(IP), HTTP_PORT) < 0)
	{
		perror("Could not connect");
		return 1;
	}
	else
	{
		cout << "Ket noi thanh cong" << endl;
		valnum = s_client.Send(get_query, strlen(get_query), 0);
		if (valnum == -1)
		{
			cout << "Khong the gui truy van den Server" << endl;
			return 1;
		}
		//Nhan HTTP respond tu Server
		memset(buf, 0, BSIZE);
		valnum = s_client.Receive(buf, BSIZE, 0);
		if (valnum == -1)
		{
			cout << "Khong the nhan respond tu Server";
			return -1;
		}
		else
		{
			cout << buf;
			//Gui HTTP respond den Client
			valnum = send(client, buf, strlen(buf), 0);
			{
				if (valnum == -1)
				{
					cout << "Khong the gui den Client";
					return 1;
				}
			}
		}

	}
	s_client.Close();
	
	return 0;


}



/*
CACHING
  Kiểm tra nếu host không có trong catch
	build_querry(host) --> buff2
	lưu vào file buff2 --> gửi buff2 về chrome
  Kiểm tra có
     gửi request HTTP if_modified lên Server
	  if not_modified --> lấy từ file gửi lại về chrome
	  else cập nhật lại rồi sau đó gửi về chrome
*/

// Return 1 if host is in Blacklist, return 0 if not
bool isInBlacklist(string host)
{
	
	freopen(BlacklistFile, "rt", stdin);
	string hostname;
	while (cin >> hostname)
	{
		if (hostname.find(host) != string::npos)
		{
			return 1;
		}
	}
	return 0;
}

void getHostNPage(string buff, string &host, string &page)
{
	stringstream buffer(buff);
	vector<string> res;
	string token;
	//res[0]: method, res[1]: host, res[2]: protocol, i.e:  GET http://weevil.info/abc HTML/1.0
	for (int i = 0; i < 3; i++)
	{
		getline(buffer, token, ' ');
		res.push_back(token); 
	}
	int pos = res[1].find(HTTP); // Tim vi tri cua chuoi "http://"
	if (pos != string::npos)
	{
		//Chuoi substring la chuoi res[1] da bo chuoi "http://"
		string substring = res[1].substr(pos+7, res[1].length()-7); 
		//Lay host
		host = substring.substr(0, substring.find(PAGE));
		//Lay Page
		page = substring.substr(substring.find(PAGE)+1, substring.length() - host.length());
	}
}


char* build_GET_querry(const char *host, const char *page)
{
	char *query;
	const char *getpage = page;
	char *tpl = "GET /%s HTTP/1.0\r\nHost: %s\r\nUser-Agent: %s\r\n\r\n";
	if (getpage[0] == '/') {
		getpage = getpage + 1;
		fprintf(stderr, "Removing leading \"/\", converting %s to %s\n", page, getpage);
	}
	// -5 is to consider the %s %s %s in tpl and the ending \0
	query = (char *)malloc(strlen(host) + strlen(getpage) + strlen(USER_AGENT) + strlen(tpl) - 5);
	sprintf_s(query, strlen(query) + 1, tpl, getpage, host, USER_AGENT);
	return query;
}

char *get_ip(const char *host)
{
	struct hostent *hent;
	int iplen = 15; //XXX.XXX.XXX.XXX
	char *ip = (char *)malloc(iplen + 1);
	memset(ip, 0, iplen + 1);
	if ((hent = gethostbyname(host)) == NULL)
	{
		perror("Can't get IP");
		exit(1);
	}
	if (inet_ntop(AF_INET, (void *)hent->h_addr_list[0], ip, iplen) == NULL)
	{
		perror("Can't resolve host");
		exit(1);
	}
	return ip;
}

wchar_t *convertCharArrayToLPCWSTR(const char* charArray)
{
	wchar_t* wString = new wchar_t[4096];
	MultiByteToWideChar(CP_ACP, 0, charArray, -1, wString, 4096);
	return wString;
}
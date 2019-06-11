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
#define BSIZE 200000
#define USER_AGENT "Mozilla/5.0"
#define PAGE "/"
#define HTTP "http://"
#define BlacklistFile "blacklist.conf" 

using namespace std;


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
string build_get_query(string host, string page);
//Ham lay IP tu host
char *get_ip(const char *host);
//Ham loaf blacklist
void loadBlackList();
//Ham Check Blacklist
bool isInBlacklist(string host);
//Ham chuyen doi char* sang LPCWSTR
wchar_t *convertCharArrayToLPCWSTR(const char* charArray);



/* Khai bao bien toan cuc*/
string ForbiddenRequest =  "HTTP/1.1 403 Forbidden\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n<!doctype html><html><body> <h1> <strong> 403 Forbidden </strong> </h1> <h3> You cannot access this website ! </h3></body></html>";
vector<string> blacklist;
string website_HTML;

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
				Sleep(10000);
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

	//Load blacklist
	loadBlackList();

	//Tạo Socket descriptor
	if ((serverfd = socket(AF_INET, SOCK_STREAM, 0)) == 0) 
	{
		cout << "Socket failed";
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
	if (listen(serverfd, 10) < 0) //Lắng nghe tối đa 10 kết nối
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
	cout << "socket id: " << new_socket << " connected" << endl;
	//Khởi tạo một thread để tiếp tục lắng nghe các kết nối khác
	AfxBeginThread(ClientToProxy, pParam);

	//Nhận truy vấn từ Client
	char client_sent[BSIZE];
	int valnum; //so byte nhan duoc tu Client
	memset(client_sent, 0, BSIZE);
	valnum = recv(new_socket, client_sent, BSIZE, 0);
	if (valnum == SOCKET_ERROR)
	{
		cout << "Received error with code: " << WSAGetLastError() << endl;
		return -1;
	}
	if (valnum == 0)
	{
		cout << "Client closed...";
		return -1;
	}

	//Xuat cac truy van
	cout << "Client sent: " << endl;
	cout << client_sent;
	
	
	//Lay host va kiem tra host co trong black list khong
	string host, page;
	getHostNPage(client_sent, host, page);
	
	if (isInBlacklist(host) && host != "")
	{
		cout << "403 Error - Forbiden" << endl;
		valnum = send(new_socket, ForbiddenRequest.c_str(), ForbiddenRequest.size(),0); // Gui Code 403 Forbidden den Client
		Sleep(2000);
		closesocket(new_socket);
		return 0;
	}
	else
	{
		Sleep(2000);
		string get_query = build_get_query(host, page);
		cout << "QUERY IS: " << endl << get_query << endl;
		char *IP = get_ip(host.c_str());
		if (IP == NULL)
		{
			return -1;
		}
		cout << IP << endl;
		char buf[BSIZE];

		sockaddr_in server_info;
		server_info.sin_family = AF_INET; //IPv4
		server_info.sin_port = htons(HTTP_PORT); //Port
		server_info.sin_addr.s_addr = inet_addr(IP); //The IP of Server got above

		SOCKET remote_server = socket(AF_INET, SOCK_STREAM, 0);
		if (connect(remote_server, (sockaddr*)&server_info, sizeof(server_info)) == SOCKET_ERROR)
		{
			cout << "Cannot connect to " << host << " error code: " << WSAGetLastError() << endl;
			send(new_socket, ForbiddenRequest.c_str(), sizeof(ForbiddenRequest), 0);
			return -1;
		}
		//Send query to server
		valnum = send(remote_server, get_query.c_str(), get_query.size(), 0);
		if (valnum == SOCKET_ERROR)
		{
			cout << "Send to sever error with code: " << WSAGetLastError() << endl;
			send(new_socket, ForbiddenRequest.c_str(), sizeof(ForbiddenRequest), 0);

			return -1;
		}
		//Get response from server and send them to browser
		while (true)
		{
			valnum = recv(remote_server, buf, BSIZE, 0);
			if (valnum <= 0)
			{
				cout << "Can't get data from server, error code: " << WSAGetLastError() << endl;
				break;
			}
			valnum >= BSIZE ? buf[valnum - 1] = '\0' : (valnum > 0 ? buf[valnum] = '\0' : buf[0] = '\0');
			cout << "--------------------------------------------------------" << endl;
			cout << "Received: " << valnum << " data from server" << endl;
			//Send response to browser
			valnum = send(new_socket, buf, strlen(buf), 0);
			if (valnum <= 0)
			{
				cout << "Cannot send to browser: " << WSAGetLastError() << endl;
				send(new_socket, ForbiddenRequest.c_str(), sizeof(ForbiddenRequest), 0);
				break;
			}
			else
			{
				cout << host << " sent " << valnum << " data to browser" << endl;
			}
		}
		closesocket(new_socket);
	}
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
void loadBlackList()
{
	if (freopen(BlacklistFile, "rt", stdin) == NULL)
	{
		cout << "No Blacklist available" << endl;
		return;
	}
	string hostname;
	while (cin >> hostname)
	{
		blacklist.push_back(hostname);
	}
}


// Return 1 if host is in Blacklist, return 0 if not
bool isInBlacklist(string host)
{
	if (blacklist.size() <= 0)
	{
		return 0;
	}
	for (int i = 0; i < blacklist.size(); i++)
	{
		if (blacklist[i].find(host) != string::npos)
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


string build_get_query(string host, string page)
{
	if (page[0] != '/')
	{
		page = "/" + page;
	}
	string query = "GET " + page + " HTTP/1.1\r\nHost: " + host + "\r\nUser-Agent: "+USER_AGENT+"\r\n\r\n";
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
		cout<<"Can't get IP";
		return NULL;
	}
	if (inet_ntop(AF_INET, (void *)hent->h_addr_list[0], ip, iplen) == NULL)
	{
		cout<<"Can't resolve host";
		return NULL;
	}
	return ip;
}

wchar_t *convertCharArrayToLPCWSTR(const char* charArray)
{
	wchar_t* wString = new wchar_t[4096];
	MultiByteToWideChar(CP_ACP, 0, charArray, -1, wString, 4096);
	return wString;
}

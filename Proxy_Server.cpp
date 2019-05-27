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
#define PORT 8888
#define BSIZE 10000
#define USER_AGENT "HTMLGET 1.0"
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
char* build_GET_querry(char buff[]);
//Ham lay IP tu host
char *get_ip(char *host);
//Ham Check Blacklist
bool isInBlacklist(string host);


string ForbiddenRequest =  "HTTP/1.0 403 Forbidden\r\n\Cache-Control: no-cache\r\n\Connection: close\r\n ";

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
				Sleep(1000);
			}
			/*
			string host, page;
			string buf = "GET http://weevil.info HTML/1.0 ";
			getHostNPage(buf, host, page);
			if (isInBlacklist(host))
			{
				cout << "403 Error - Forbiden";
			}
			*/
			
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
	address.sin_port = htons(PORT); //Port
	address.sin_addr.s_addr = INADDR_ANY; //Local IP, for example: 172.0.0.1

	
	//Tao Socket descriptor
	if ((serverfd = socket(AF_INET, SOCK_STREAM, 0)) == 0) 
	{
		cout << "Socket failed";
		exit(0);
	}

	char opt = '1';
	int iRes = setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (iRes == SOCKET_ERROR)
	{
		cout << "setsockopt error" << endl;
		exit(0);
	}

	//bind socket server to PORT
	if (bind(serverfd, (sockaddr*)&address, sizeof(address)) == SOCKET_ERROR)
	{
		cout << "Bind failed with error " << WSAGetLastError();
		exit(0);
	}
	//Lang nghe cac ket noi
	if (listen(serverfd, 5) < 0) //Lang nghe toi da 5 ket noi
	{
		cout << "Listen failed";
		exit(0);
	}

	SOCKET gListen = serverfd;
	//Bat dau Thread giao tiep giua Client va ProxyServer
	AfxBeginThread(ClientToProxy, (LPVOID)serverfd);
}

UINT ClientToProxy(LPVOID pParam)
{
	SOCKET server = (SOCKET)pParam;
	if (server == NULL)
	{
		return 1; //Failed 
	}

	SOCKET client;
	sockaddr_in addr;
	int addrlen = sizeof(addr);
	
	//Truy cap moi
	client = accept(server, (sockaddr*)&addr, &addrlen);
	//Tao mot thread moi de nghe cac truy cap khac
	AfxBeginThread(ClientToProxy, pParam);

	//Nhan truy van tu Client
	char buffer[BSIZE];
	int valnum; //so byte nhan duoc tu Client
	valnum = recv(client, buffer, BSIZE, 0);
	if (valnum == SOCKET_ERROR)
	{
		cout << "Nhan truy van bi loi";
		exit(0);
	}
	if (valnum == 0)
	{
		cout << "Client ngung gui truy van";
		exit(0);
	}
	if (valnum >= BSIZE)
	{
		buffer[valnum - 1] = '\0';
	}
	else if (valnum > 0)
	{
		buffer[valnum] = '\0';
	}
	else
		buffer[0] = '\0';

	//Xuat cac truy van
	cout << "Nhan tu Client: " << endl;
	cout << buffer;

	//Lay host va kiem tra host co trong black list khong
	bool check = FALSE;
	string host, page;
	getHostNPage(buffer, host, page);
	if (isInBlacklist(host) && host !="")
	{
		cout << "403 Error - Forbiden";
		valnum = send(client, ForbiddenRequest.c_str(), ForbiddenRequest.length(),0); // Gui Code 403 Forbidden den Client
		cout << valnum;
		check = TRUE;
		Sleep(5000);
	}
	if (check == FALSE)
	{

	}


	
	return 0;
}

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


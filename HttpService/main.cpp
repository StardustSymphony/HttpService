#include<stdio.h>
#include<iostream>
#include<stdlib.h>	//exit()
#include<string.h>	//memset()
#include<sys/types.h>
#include<sys/stat.h>

//网络通信的头文件和需要加载的库文件
#include<WinSock2.h>
#include<windows.h>
#pragma comment(lib,"WS2_32.lib")

#define PRINTF(str) printf("[%s-%d]"#str"=%s\n", __func__, __LINE__, buff)

//打印系统调用失败的错误信息
void error_die(const char* str)
{
	perror(str);
	exit(1);
}

//网络的初始化，返回服务器端的套接字
//port为端口号，为0时会自动分配一个可用的端口
SOCKET startup(unsigned short* port)
{
	//1、网络通信初始化
	WSADATA data;
	int ret=WSAStartup(MAKEWORD(1, 1),	//1.1版本的协议
		&data);

	if (ret)
	{
		error_die("WSAStartup");
	}

	//创建套服务器接字
	SOCKET severt_socket=socket(AF_INET,	//使用的地址族协议，也就是ip地址的格式（ipv4/ipv6）
		SOCK_STREAM,	//使用流式的传输协议
		IPPROTO_TCP	//流式传输默认使用的是tcp
	);

	if (severt_socket == INVALID_SOCKET)
	{
		//打印错误提示，并结束程序
		error_die("socket create failed");
	}

	//设置端口可复用
	int opt = 1;
	ret=setsockopt(severt_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
	if (ret == -1)
	{
		error_die("setsockopt");
	}

	//配置服务器的网络地址
	struct sockaddr_in sever_addr;  //定义一个存储服务器 IP + 端口的 IPv4 地址结构体变量
	memset(&sever_addr, 0, sizeof(sever_addr));
	sever_addr.sin_family = AF_INET;	//地址协议族
	sever_addr.sin_port = htons(*port);	//主机字节序->网络字节序
	sever_addr.sin_addr.s_addr = htonl(INADDR_ANY);  //所有地址都可以访问

	//绑定套接字,将创建好的 socket 和 指定 IP + 端口绑定在一起
	if (bind(severt_socket, (struct sockaddr*)&sever_addr, sizeof(sever_addr)) < 0)
	{//参数 2 强制转成通用地址结构体struct sockaddr*
		error_die("bind");
	}

	//动态获取一个端口
	int nameLen = sizeof(sever_addr);
	if (*port == 0)
	{
		if (getsockname(severt_socket, (struct sockaddr*)&sever_addr, &nameLen) < 0)
		{
			error_die("getsockname");
		}

		*port = sever_addr.sin_port;
	}

	//创建监听队列
	if (listen(severt_socket, 5) < 0)
	{
		error_die("listen");
	}

	return severt_socket;
}

//从指定的客户端套接字中读取一行数据，保存到buff中，返回实际读取的字节数
int get_line(int sock, char* buff, int size)
{
	char c = 0;	 //当前读取的字符
	int i = 0;	//索引

	while (i < size - 1 && c != '\n')
	{
		int n = recv(sock, &c, 1, 0);
		if (n > 0)	//读取成功
		{
			if (c == '\r')
			{
				n = recv(sock, &c, 1, MSG_PEEK);	//先检查下一个字符是什么
				if (n > 0 && c == '\n')
				{
					recv(sock, &c, 1, 0);
				}
				else
				{
					c = '\n';
				}
			}
			buff[i++] = c;
		}
		else
		{
			break;	//recv 返回 0(对端关闭) 或 -1(出错)：必须跳出，否则 c、i 都不变会死循环 100% CPU
		}
	}
	buff[i] = 0;
	return i;
}

void unimplement(int client)
{

}

void not_found(int client)
{
	
}

void headers(int client)
{
	//发送响应包的头信息
	char buff[1024];

	strcpy(buff, "HTTP/1.0 200 OK\r\n");
	send(client, buff, strlen(buff), 0);

	strcpy(buff, "Server: HaleHttp/0.1\r\n");
	send(client, buff, strlen(buff), 0);

	strcpy(buff, "Content-type:text/html\r\n");
	send(client, buff, strlen(buff), 0);

	strcpy(buff, "\r\n");
	send(client, buff, strlen(buff), 0);
}

void cat(int client, FILE* resource)
{
	char buff[4096];
	int count = 0;
	while (1)
	{
		int ret = fread(buff, sizeof(char), sizeof(buff), resource);
		if (ret <= 0) break;
		send(client, buff, ret, 0);
		count += ret;
	}
	std::cout << "一共发送[" << count << "]字节给浏览器\n";
}

void server_file(int client, const char* fileName)
{
	char numchars = 1;
	char buff[1024];

	//把请求数据包的剩余数据行读完
	while (numchars > 0 && strcmp(buff, "\n"))
	{
		numchars = get_line(client, buff, sizeof(buff));
		PRINTF(buff);
	}

	FILE* resource = fopen(fileName, "r");
	if (resource == nullptr)
	{
		not_found(client);
	}
	else
	{
		//正式发送资源给浏览器
		headers(client);

		//发送请求的资源信息
		cat(client, resource);

		printf("资源发送完毕！\n");
	}

	fclose(resource);
}

//处理用户请求的线程函数
DWORD WINAPI accept_request(LPVOID arg)
{
	char buff[1024];

	int client = (SOCKET)arg;	//客户端套接字

	int numchars = get_line(client, buff, sizeof(buff));
	PRINTF(buff);

	char method[255];
	int i = 0, j = 0;
	while (!isspace(buff[j]) && i < sizeof(method) - 1)
	{
		method[i++] = buff[j++];
	}
	method[i] = 0;
	PRINTF(method);

	//检查请求方法，判断本服务器是否支持
	if (stricmp(method, "GET") && stricmp(method, "POST"))
	{
		//向浏览器返回一个错误提示页面
		unimplement(client);
		return 0;
	}

	//解析资源文件路径
	char url[255];	//存放完整的资源文件路径
	i = 0;
	while (isspace(buff[j]) && j < sizeof(buff))
	{
		j++;
	}

	while (!isspace(buff[j]) && i < sizeof(url) - 1 && j < sizeof(buff))
	{
		url[i++] = buff[j++];
	}

	url[i] = 0;
	PRINTF(url);

	char path[512] = "";
	sprintf(path, "htdocs%s", url);
	if (path[strlen(path) - 1] == '/')
	{
		strcat(path, "index.html");
	}
	PRINTF(path);

	struct stat status;
	if (stat(path, &status) == -1)
	{
		//请求包的剩余数据读取完毕
		while (numchars > 0 && strcmp(buff, "\n"))
		{
			numchars = get_line(client, buff, sizeof(buff));
		}
		not_found(client);
	}
	else
	{
		if ((status.st_mode & S_IFMT) == S_IFDIR)
		{
			strcat(path, "/index.html");
		}

		server_file(client, path);
	}

	closesocket(client);

	return 0;
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);	//禁用 stdout 缓冲：VS 调试/管道下 stdout 可能是全缓冲，光靠 \n 不会刷新
	unsigned short port = 80;
	SOCKET server_sock=startup(&port);
	printf("http服务器已经启动，正在监听 %d 端口...\n", port);

	//先创建一个客户端的网络地址
	struct sockaddr_in client_addr;
	int client_addr_len = sizeof(client_addr);

	// to do
	while (1)	//接受客户端发来的请求
	{
		//阻塞式等待用户发来请求,同意的话服务器创建一个新的对应的套接字与客户端进行通信
		SOCKET client_sock=accept(server_sock, (struct sockaddr*)&client_addr, &client_addr_len);

		if (client_sock == INVALID_SOCKET)
		{
			error_die("accept");
		}

		//创建一个新的线程来接受其他用户发来的请求
		DWORD threadId = 0;
		HANDLE hThread = CreateThread(0, 0, accept_request, (void*)client_sock, 0, &threadId);
		if (hThread)
			CloseHandle(hThread);
	}

	closesocket(server_sock);
	return 0;
}
#include<stdio.h>
#include<stdlib.h>	//exit()
#include<string.h>	//memset()

//网络通信的头文件和需要加载的库文件
#include<WinSock2.h>
#pragma comment(lib,"WS2_32.lib")

//打印系统调用失败的错误信息
void error_die(const char* str)
{
	perror(str);
	exit(1);
}

//网络的初始化，返回服务器端的套接字
//port为端口号，为0时会自动分配一个可用的端口
int startup(unsigned short* port)
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
	int severt_socket=socket(AF_INET,	//使用的地址族协议，也就是ip地址的格式（ipv4/ipv6）
		SOCK_STREAM,	//使用流式的传输协议
		IPPROTO_TCP	//流式传输默认使用的是tcp
	);

	if (severt_socket == -1)
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

//处理用户请求的线程函数
DWORD WINAPI accept_request(LPVOID arg)
{

	return 0;
}

int main()
{
	unsigned short port = 0;
	int server_sock=startup(&port);
	printf("http服务器已经启动，正在监听 %d 端口...", port);

	struct sockaddr_in client_addr;
	int client_addr_len = sizeof(client_addr);

	// to do
	while (1)	//接受客户端发来的请求
	{
		//阻塞式等待用户发来请求,同意的话服务器创建一个新的对应的套接字与客户端进行通信
		int client_sock=accept(server_sock, (struct sockaddr*)&client_addr, &client_addr_len);

		if (client_sock == -1)
		{
			error_die("accept");
		}

		//创建一个新的线程来接受其他用户发来的请求
		DWORD threadId = 0;
		CreateThread(0, 0, accept_request, (void*)client_sock, 0, &threadId);
	}

	closesocket(server_sock);
	return 0;
}
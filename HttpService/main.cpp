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

//定义一个宏函数
#define PRINTF(str) printf("[%s-%d]"#str"=%s\n", __func__, __LINE__, buff)

//打印系统调用失败的错误信息（Winsock 错误需用 WSAGetLastError 获取准确错误码）
void error_die(const char* str)
{
	printf("%s failed, WSA error: %d\n", str, WSAGetLastError());
	exit(1);
}

//循环发送，直到全部字节发出或出错（send 可能只发送部分数据，忽略返回值会截断响应）
int send_all(int sock, const char* buf, int len)
{
	int sent = 0;
	while (sent < len)
	{
		int n = send(sock, buf + sent, len - sent, 0);
		if (n <= 0)
			break;
		sent += n;
	}
	return sent;
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

		*port = ntohs(sever_addr.sin_port);	//sin_port 为网络字节序，必须转回主机序才能正确打印端口
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
	//向浏览器返回 501 Not Implemented（不支持的请求方法）
	const char* resp =
		"HTTP/1.0 501 Not Implemented\r\n"
		"Server: HaleHttp/0.1\r\n"
		"Content-type: text/html\r\n"
		"\r\n"
		"<html><head><title>501</title></head>"
		"<body><h1>501 Not Implemented</h1>"
		"<p>The requested method is not supported.</p></body></html>";
	send_all(client, resp, (int)strlen(resp));
}

void not_found(int client)
{
	//404 响应体（此前把头缓冲 "\r\n" 当 body 重复发送，这里改为发送真正的错误页）
	static const char* body =
		"<html><head><title>404</title></head>"
		"<body><h1>404 Not Found</h1>"
		"<p>The requested resource was not found.</p></body></html>";

	char head[1024];
	int n = snprintf(head, sizeof(head),
		"HTTP/1.0 404 NOT FOUND\r\n"
		"Server: HaleHttp/0.1\r\n"
		"Content-type: text/html\r\n"
		"Content-Length: %zu\r\n"
		"Connection: close\r\n"
		"\r\n"
		,
		strlen(body));

	send_all(client, head, n);
	send_all(client, body, (int)strlen(body));
}

void headers(int client, long content_length)
{
	//发送响应包的头信息
	char buff[1024];

	snprintf(buff, sizeof(buff), "HTTP/1.0 200 OK\r\n");
	send_all(client, buff, strlen(buff));

	snprintf(buff, sizeof(buff), "Server: HaleHttp/0.1\r\n");
	send_all(client, buff, strlen(buff));

	snprintf(buff, sizeof(buff), "Content-type: text/html\r\n");
	send_all(client, buff, strlen(buff));

	snprintf(buff, sizeof(buff), "Content-Length: %ld\r\n", content_length);
	send_all(client, buff, strlen(buff));

	snprintf(buff, sizeof(buff), "Connection: close\r\n");
	send_all(client, buff, strlen(buff));

	snprintf(buff, sizeof(buff), "\r\n");
	send_all(client, buff, strlen(buff));
}

void cat(int client, FILE* resource)
{
	char buff[4096];
	int count = 0;
	while (1)
	{
		int ret = fread(buff, sizeof(char), sizeof(buff), resource);
		if (ret <= 0) break;
		send_all(client, buff, ret);
		count += ret;
	}
	std::cout << "一共发送[" << count << "]字节给浏览器\n";
}

void server_file(int client, const char* fileName)
{
	char buff[1024];

	//读取剩余的请求头，直到遇到空行（先读入再判断，避免对未初始化 buff 做 strcmp）
	while (get_line(client, buff, sizeof(buff)) > 0 && strcmp(buff, "\n") != 0)
	{
		PRINTF(buff);
	}

	FILE* resource = NULL;
	if (strcmp(fileName, "htdocs/index.html") == 0)
	{
		resource = fopen(fileName, "r");
	}
	else
	{
		resource = fopen(fileName, "rb");
	}

	if (resource == nullptr)
	{
		not_found(client);
		return;	//打开失败：已发送404，禁止继续 fclose(nullptr)
	}

	//取得文件大小，用于 Content-Length 响应头
	fseek(resource, 0, SEEK_END);
	long size = ftell(resource);
	fseek(resource, 0, SEEK_SET);

	//正式发送资源给浏览器
	headers(client, size);

	//发送请求的资源信息
	cat(client, resource);

	printf("资源发送完毕！\n");

	fclose(resource);	//仅成功打开时才会执行到此
}

//CGI 动态请求处理：把请求交给 cgi-bin/ 下的 CGI 程序，通过匿名管道转发其 stdout 作为响应体。
//父进程管道范式（修复 C1/C2）：关闭父端写句柄以收到 EOF、读取到 EOF 即退出、初始化读取字节数、等待并关闭子进程句柄。
void execute_cgi(int client, const char* method, const char* url)
{
    // 目录穿越防护：拒绝 URL 中含 ".." 的 CGI 请求
    if (strstr(url, "..") != nullptr)
    {
        not_found(client);
        return;
    }

    // 拆分 path 与 query string（"?" 之后为 QUERY_STRING）
    char path_part[512];
    const char* q = strchr(url, '?');
    size_t path_len = q ? (size_t)(q - url) : strlen(url);
    if (path_len >= sizeof(path_part)) path_len = sizeof(path_part) - 1;
    memcpy(path_part, url, path_len);
    path_part[path_len] = 0;
    const char* query_string = (q && *(q + 1)) ? q + 1 : "";

    // 定位 CGI 程序：约定放在 cgi-bin/ 目录下，按请求路径命名
    // 例如请求 /cgi-bin/hello -> 启动 cgi-bin/hello.exe
    const char* prog = path_part + 9;   // 跳过前缀 "/cgi-bin/"
    if (*prog == 0) prog = "index";
    char exe_path[512];
    snprintf(exe_path, sizeof(exe_path), "cgi-bin/%s.exe", prog);

    struct stat st;
    if (stat(exe_path, &st) == -1)
    {
        not_found(client);
        return;
    }

    // 读取剩余请求头，捕获 Content-Length / Content-Type，并丢弃其它头
    char header[1024];
    int content_length = 0;
    char content_type[256] = "text/plain";
    while (get_line(client, header, sizeof(header)) > 0 && strcmp(header, "\n") != 0)
    {
        if (_strnicmp(header, "Content-Length:", 15) == 0)
        {
            content_length = atoi(header + 15);
        }
        else if (_strnicmp(header, "Content-Type:", 13) == 0)
        {
            const char* p = header + 13;
            while (*p == ' ' || *p == '\t') p++;
            strncpy(content_type, p, sizeof(content_type) - 1);
            content_type[sizeof(content_type) - 1] = 0;
            size_t L = strlen(content_type);
            while (L > 0 && (content_type[L - 1] == '\n' || content_type[L - 1] == '\r'))
                content_type[--L] = 0;
        }
    }

    // 设置 CGI 环境变量（通过继承的环境块传给子进程）
    char e_method[64], e_query[1024], e_clen[64], e_ctype[256];
    snprintf(e_method, sizeof(e_method), "REQUEST_METHOD=%s", method);
    snprintf(e_query,  sizeof(e_query),  "QUERY_STRING=%s", query_string);
    snprintf(e_clen,   sizeof(e_clen),   "CONTENT_LENGTH=%d", content_length);
    snprintf(e_ctype,  sizeof(e_ctype),  "CONTENT_TYPE=%s", content_type);
    _putenv(e_method);
    _putenv(e_query);
    _putenv(e_clen);
    _putenv(e_ctype);
    // 注意：多线程下直接修改进程环境存在竞态，教学项目可接受；生产环境应改用 CreateProcess 的自定义环境块

    // 创建两条匿名管道：子进程 stdout -> 服务器读；服务器写 -> 子进程 stdin
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hChildStdout_R = NULL, hChildStdout_W = NULL;
    HANDLE hChildStdin_R  = NULL, hChildStdin_W  = NULL;
    if (!CreatePipe(&hChildStdout_R, &hChildStdout_W, &sa, 0) ||
        !CreatePipe(&hChildStdin_R,  &hChildStdin_W,  &sa, 0))
    {
        error_die("CreatePipe");
    }

    STARTUPINFO si = { 0 };
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput  = hChildStdin_R;     // 子进程从管道读请求体
    si.hStdOutput = hChildStdout_W;    // 子进程向管道写响应
    si.hStdError  = hChildStdout_W;    // 子进程错误也写进响应
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = { 0 };
    char cmdline[640];
    snprintf(cmdline, sizeof(cmdline), "\"%s\"", exe_path);
    if (!CreateProcess(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    {
        printf("CreateProcess failed, error: %d\n", GetLastError());
        not_found(client);
        CloseHandle(hChildStdout_R); CloseHandle(hChildStdout_W);
        CloseHandle(hChildStdin_R);  CloseHandle(hChildStdin_W);
        return;
    }

    // 关键（修复 C1）：关闭父进程持有的子进程 stdout 写端 与 stdin 读端，
    // 否则子进程退出后 ReadFile 永远收不到 EOF，会死循环
    CloseHandle(hChildStdout_W);
    CloseHandle(hChildStdin_R);

    // 若是 POST，把请求体写入子进程 stdin，关闭写端后子进程读到 EOF
    if (strcmp(method, "POST") == 0 && content_length > 0)
    {
        char buf[4096];
        int remaining = content_length;
        while (remaining > 0)
        {
            int to_read = remaining < (int)sizeof(buf) ? remaining : (int)sizeof(buf);
            int n = recv(client, buf, to_read, 0);
            if (n <= 0) break;
            DWORD written = 0;
            WriteFile(hChildStdin_W, buf, (DWORD)n, &written, NULL);
            remaining -= n;
        }
    }
    CloseHandle(hChildStdin_W);   // 关闭后子进程 stdin 到达 EOF

    // 先给客户端发送 HTTP 状态行（CGI 程序自己会输出 Content-type 等头，这里只发状态行）
    const char* status_line = "HTTP/1.0 200 OK\r\n";
    send_all(client, status_line, (int)strlen(status_line));

    // 读取子进程 stdout 并流式转发给客户端；读到 EOF（dwRead==0）即结束（修复 C1 死循环 / C2 未初始化）
    char out_buf[4096];
    DWORD dwRead = 0;
    while (ReadFile(hChildStdout_R, out_buf, sizeof(out_buf), &dwRead, NULL) && dwRead > 0)
    {
        send_all(client, out_buf, (int)dwRead);
    }

    // 等待子进程结束并关闭所有句柄
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hChildStdout_R);
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
	while (!isspace((unsigned char)buff[j]) && i < sizeof(method) - 1)
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
		closesocket(client);	//避免该分支提前 return 导致套接字泄漏
		return 0;
	}

	//解析资源文件路径
	char url[255];	//存放完整的资源文件路径
	i = 0;
	while (isspace((unsigned char)buff[j]) && j < sizeof(buff))
	{
		j++;
	}

	while (!isspace((unsigned char)buff[j]) && i < sizeof(url) - 1 && j < sizeof(buff))
	{
		url[i++] = buff[j++];
	}

	url[i] = 0;
	PRINTF(url);

	//安全检查：拒绝包含 ".." 的路径，防止目录穿越读取 htdocs 之外的文件
	if (strstr(url, "..") != nullptr)
	{
		not_found(client);
		closesocket(client);
		return 0;
	}

	//CGI 分支：URL 命中 /cgi-bin/ 时交给 CGI 程序动态处理
	//（C3：让孤儿模块接入服务器；execute_cgi 内含正确的父进程管道范式，对应 C1/C2 修复）
	if (strncmp(url, "/cgi-bin/", 9) == 0)
	{
		execute_cgi(client, method, url);
		closesocket(client);
		return 0;
	}

	char path[512] = "";
	snprintf(path, sizeof(path), "htdocs%s", url);
	if (path[strlen(path) - 1] == '/')
	{
		strcat(path, "index.html");
	}
	PRINTF(path);

	struct stat status;
	if (stat(path, &status) == -1)
	{
		//读取剩余的请求头，直到空行
		while (get_line(client, buff, sizeof(buff)) > 0 && strcmp(buff, "\n") != 0)
		{
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
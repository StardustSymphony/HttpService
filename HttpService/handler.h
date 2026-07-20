#pragma once
#include <WinSock2.h>

// 静态文件服务：读取文件并作为 HTTP 响应发送
void server_file(int client, const char* fileName);

// CGI 动态请求处理：通过匿名管道启动 CGI 程序并转发其输出
void execute_cgi(int client, const char* method, const char* url);

// 处理单个客户端请求的入口函数（线程池工作函数）
DWORD WINAPI accept_request(LPVOID arg);

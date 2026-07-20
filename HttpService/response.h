#pragma once
#include <stdio.h>

// 发送 501 Not Implemented
void unimplement(int client);

// 发送 404 Not Found（含 Content-Length 和 Connection: close）
void not_found(int client);

// 发送 200 OK 响应头（含 Content-Length 和 Connection: close）
void headers(int client, long content_length);

// 以 4KB 块循环读取文件并发送给客户端
void cat(int client, FILE* resource);

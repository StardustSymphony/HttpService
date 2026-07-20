#pragma once
#include <WinSock2.h>

// 网络初始化：WSAStartup → socket → bind → listen，返回监听套接字
// port: 端口号，传入 0 时自动分配并写回实际端口
SOCKET startup(unsigned short* port);

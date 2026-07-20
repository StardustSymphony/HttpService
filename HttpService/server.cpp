#include "server.h"
#include "utils.h"
#include <WinSock2.h>
#include <string.h>

SOCKET startup(unsigned short* port)
{
    // 1、网络通信初始化
    WSADATA data;
    int ret = WSAStartup(MAKEWORD(1, 1), &data);
    if (ret)
        error_die("WSAStartup");

    // 2、创建服务器套接字（IPv4, TCP）
    SOCKET severt_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (severt_socket == INVALID_SOCKET)
        error_die("socket create failed");

    // 3、设置端口可复用，避免 TIME_WAIT 状态下端口被占用
    int opt = 1;
    ret = setsockopt(severt_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    if (ret == -1)
        error_die("setsockopt");

    // 4、配置服务器地址并绑定
    struct sockaddr_in sever_addr;
    memset(&sever_addr, 0, sizeof(sever_addr));
    sever_addr.sin_family = AF_INET;
    sever_addr.sin_port = htons(*port);
    sever_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(severt_socket, (struct sockaddr*)&sever_addr, sizeof(sever_addr)) < 0)
        error_die("bind");

    // 5、port == 0 时获取系统自动分配的端口
    int nameLen = sizeof(sever_addr);
    if (*port == 0)
    {
        if (getsockname(severt_socket, (struct sockaddr*)&sever_addr, &nameLen) < 0)
            error_die("getsockname");
        *port = ntohs(sever_addr.sin_port);
    }

    // 6、开始监听
    if (listen(severt_socket, 5) < 0)
        error_die("listen");

    return severt_socket;
}

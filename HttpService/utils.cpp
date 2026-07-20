#include "utils.h"
#include <WinSock2.h>
#include <stdio.h>
#include <stdlib.h>

// 打印系统调用失败的错误信息，退出进程
void error_die(const char* str)
{
    printf("%s failed, WSA error: %d\n", str, WSAGetLastError());
    exit(1);
}

// 循环发送，直到全部字节发出或出错
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

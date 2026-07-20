#include "http_parser.h"
#include <WinSock2.h>

// 从客户端套接字逐字节读取一行，处理 \r\n 和 \r 两种行尾
int get_line(int sock, char* buff, int size)
{
    char c = 0;
    int i = 0;

    while (i < size - 1 && c != '\n')
    {
        int n = recv(sock, &c, 1, 0);
        if (n > 0)
        {
            if (c == '\r')
            {
                // 偷看下一个字节，若为 \n 则读走；否则将 \r 当作 \n
                n = recv(sock, &c, 1, MSG_PEEK);
                if (n > 0 && c == '\n')
                    recv(sock, &c, 1, 0);  // 读走 \n
                else
                    c = '\n';               // \r 替换为 \n
            }
            buff[i++] = c;
        }
        else
        {
            break;  // 连接关闭或出错：必须跳出，否则死循环
        }
    }
    buff[i] = 0;
    return i;
}

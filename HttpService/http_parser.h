#pragma once

// 从客户端套接字逐字节读取一行，处理 \r\n 和 \r 两种行尾，返回实际字节数
int get_line(int sock, char* buff, int size);

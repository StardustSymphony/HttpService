#pragma once

// 宏：打印 buff 变量名和值，方便调试
#define PRINTF(str) printf("[%s-%d]"#str"=%s\n", __func__, __LINE__, buff)

// 打印 WinSock 错误并退出进程
void error_die(const char* str);

// 循环发送，直到全部发出或出错（TCP send 可能部分发送）
int send_all(int sock, const char* buf, int len);

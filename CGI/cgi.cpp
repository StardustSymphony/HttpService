#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// CGI 子程序（由 HttpService 服务器通过匿名管道启动）
// 从环境变量读取请求信息，向 stdout 输出 "响应头 + 空行 + 响应体"。
// 说明：原 cgi.cpp 是“父进程读管道”的死循环 demo（C1 死循环 / C2 未初始化 size），
// 现已改造为符合 CGI 规范的子程序；正确的“父进程管道范式”由服务器 execute_cgi 负责。
int main()
{
    const char* method = getenv("REQUEST_METHOD");
    const char* query  = getenv("QUERY_STRING");
    const char* clen   = getenv("CONTENT_LENGTH");

    // 若是 POST，从 stdin 读取请求体（长度由 CONTENT_LENGTH 给出）
    char body[4096] = { 0 };
    if (method && strcmp(method, "POST") == 0 && clen)
    {
        int n = atoi(clen);
        if (n > 0 && n < (int)sizeof(body))
        {
            int got = (int)fread(body, 1, (size_t)n, stdin);
            body[got] = '\0';
        }
    }

    // 输出 CGI 响应：先输出响应头，再输出一个空行，最后输出响应体
    printf("Content-type: text/html; charset=utf-8\r\n");
    printf("\r\n");
    printf("<html><head><meta charset=\"utf-8\"><title>CGI Demo</title></head>\r\n");
    printf("<body>\r\n");
    printf("  <h1>Hello from CGI program</h1>\r\n");
    printf("  <p>REQUEST_METHOD = %s</p>\r\n", method ? method : "(null)");
    printf("  <p>QUERY_STRING = %s</p>\r\n", query ? query : "(null)");
    if (body[0])
    {
        printf("  <p>POST body = %s</p>\r\n", body);
    }
    printf("</body></html>\r\n");
    fflush(stdout);
    return 0;
}

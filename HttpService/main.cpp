#include <WinSock2.h>
#include <windows.h>
#include <stdio.h>

// 链接 WinSock 库
#pragma comment(lib, "WS2_32.lib")

#include "server.h"
#include "handler.h"
#include "thread_pool.h"
#include "utils.h"

int main()
{
    SetConsoleOutputCP(65001);              // 控制台输出设为 UTF-8，避免中文乱码
    setvbuf(stdout, NULL, _IONBF, 0);       // 禁用 stdout 缓冲，确保实时输出

    unsigned short port = 8080;
    SOCKET server_sock = startup(&port);
    printf("HttpServer 已启动，监听端口 %d...\n", port);

    // 创建线程池：线程数 = 硬件并发数 * 2（I/O 密集型可适当超配）
    unsigned int num_threads = std::thread::hardware_concurrency() * 2;
    if (num_threads == 0) num_threads = 4;   // 硬件信息不可用时的回退值
    ThreadPool pool(num_threads);
    printf("线程池已创建，工作线程: %u\n", num_threads);

    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);

    while (1)
    {
        SOCKET client_sock = accept(server_sock,
            (struct sockaddr*)&client_addr, &client_addr_len);

        if (client_sock == INVALID_SOCKET)
            error_die("accept");

        // 将请求处理任务提交到线程池（生产-消费模型）
        pool.enqueue([client_sock]() {
            accept_request((LPVOID)client_sock);
        });
    }

    closesocket(server_sock);
    return 0;
}

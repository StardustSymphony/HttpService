# HttpService

一个用 C++ 和 WinSock2 从零实现的 HTTP/1.0 服务器，支持静态文件服务和 CGI 动态请求处理。
tinyhttpd 的 Windows 移植版，用于学习 HTTP 协议底层、TCP Socket 编程和操作系统进程间通信。

## 项目结构

```
HttpService/
├── main.cpp              # 入口：初始化服务器 + 线程池 + accept 循环
├── server.h / .cpp       # 网络初始化：WSAStartup → socket → bind → listen
├── http_parser.h / .cpp  # HTTP 请求行解析：逐字节读取，处理 \r\n
├── response.h / .cpp     # HTTP 响应构造：200/404/501，send_all 循环发送
├── handler.h / .cpp      # 请求处理：静态文件服务 + CGI 动态处理
├── utils.h / .cpp        # 工具函数：error_die、send_all
├── thread_pool.h         # 线程池：固定大小，生产者-消费者模型
├── htdocs/               # 静态网站根目录
│   └── index.html
└── cgi-bin/              # CGI 可执行文件目录
    └── index.exe
```

## 架构图

```
浏览器 ──TCP──> accept() ──> ThreadPool
                                │
                    ┌───────────┴───────────┐
                    ▼                       ▼
              GET /index.html         GET /cgi-bin/xxx
                    │                       │
                    ▼                       ▼
            server_file()             execute_cgi()
                    │                       │
            fopen + fread            CreatePipe() × 2
                    │                       │
            headers() + cat()        CreateProcess()
                    │                       │
                    ▼                       ▼
              200 OK + HTML          子进程 stdout → 转发
```

## 功能特性

- **HTTP/1.0**：支持 GET 和 POST 方法
- **静态文件服务**：从 htdocs/ 目录读取并返回文件，自动补全 index.html
- **CGI 动态处理**：通过匿名管道 + CreateProcess 启动外部程序，转发其 stdout 作为 HTTP 响应
- **多线程并发**：线程池模型（固定工作线程 + 任务队列），每连接复用线程
- **安全防护**：URL 穿越检测（拒绝 `..`）、端口快速复用（SO_REUSEADDR）、可靠发送（send_all）
- **错误处理**：404 Not Found、501 Not Implemented

## 构建 & 运行

**环境要求：**
- Windows 10+
- Visual Studio 2022（MSVC v145 工具集）
- C++20

```bash
# 用 VS 打开解决方案文件
start HttpService.slnx
# F5 编译运行（Debug x64）

# 或直接运行编译产物
x64\Debug\HttpService.exe
# 浏览器打开 http://localhost:8080/
```

## 设计决策

| 决策 | 选择 | 理由 |
|------|------|------|
| 协议版本 | HTTP/1.0 | 协议最简，无需处理持久连接和分块传输 |
| 并发模型 | 线程池 | 比裸 CreateThread 节省线程创建开销，控制并发上限 |
| 线程数 | CPU 核心数 × 2 | I/O 密集型负载适当超配，平衡资源与吞吐 |
| 进程通信 | 匿名管道 | Windows 上与 CGI 标准最匹配的 IPC 方式 |
| 文件读取 | 4KB 分块循环 | 避免为大文件分配大内存缓冲区 |
| 平台 | 仅 Windows | WinSock2 + CreateProcess，不依赖跨平台库 |

## 模块职责

| 模块 | 文件 | 核心函数 | 依赖 |
|------|------|----------|------|
| 入口 | main.cpp | main() | server, handler, thread_pool |
| 网络 | server.h/cpp | startup() | utils |
| 解析 | http_parser.h/cpp | get_line() | — |
| 响应 | response.h/cpp | headers(), cat(), not_found(), unimplement() | utils |
| 处理 | handler.h/cpp | server_file(), execute_cgi(), accept_request() | utils, http_parser, response |
| 工具 | utils.h/cpp | error_die(), send_all() | — |
| 线程池 | thread_pool.h | ThreadPool 类 | std::thread |

## 已知限制

- MIME 类型硬编码为 text/html，CSS/JS/图片资源无法独立引用（前端采用内联方案绕开）
- _putenv 修改全局环境块，多线程 CGI 请求存在竞态（教学可接受）
- 无连接超时机制，恶意客户端可导致工作线程永久阻塞
- 仅支持 HTTP/1.0，不支持 keep-alive、chunked 传输编码、HTTPS

## 参考

- [tinyhttpd](http://tinyhttpd.sourceforge.net/) — 本项目的 Unix 原版参考
- [CGI 1.1 RFC 3875](https://www.rfc-editor.org/rfc/rfc3875)
- [Windows Sockets 2](https://learn.microsoft.com/en-us/windows/win32/winsock/)

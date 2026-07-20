#include "handler.h"
#include "utils.h"
#include "http_parser.h"
#include "response.h"
#include <WinSock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

void server_file(int client, const char* fileName)
{
    char buff[1024];

    // 读取剩余的请求头，直到遇到空行
    while (get_line(client, buff, sizeof(buff)) > 0 && strcmp(buff, "\n") != 0)
    {
        PRINTF(buff);
    }

    FILE* resource = fopen(fileName, "rb");
    if (resource == nullptr)
    {
        not_found(client);
        return;
    }

    // 获取文件大小，用于 Content-Length
    fseek(resource, 0, SEEK_END);
    long size = ftell(resource);
    fseek(resource, 0, SEEK_SET);

    headers(client, size);
    cat(client, resource);

    printf("资源发送完毕！\n");
    fclose(resource);
}

void execute_cgi(int client, const char* method, const char* url)
{
    // 目录穿越防护
    if (strstr(url, "..") != nullptr)
    {
        not_found(client);
        return;
    }

    // 拆分路径与查询字符串
    char path_part[512];
    const char* q = strchr(url, '?');
    size_t path_len = q ? (size_t)(q - url) : strlen(url);
    if (path_len >= sizeof(path_part)) path_len = sizeof(path_part) - 1;
    memcpy(path_part, url, path_len);
    path_part[path_len] = 0;
    const char* query_string = (q && *(q + 1)) ? q + 1 : "";

    // 定位 CGI 程序：/cgi-bin/<name> -> cgi-bin/<name>.exe
    const char* prog = path_part + 9;   // 跳过 "/cgi-bin/"
    if (*prog == 0) prog = "index";
    char exe_path[512];
    snprintf(exe_path, sizeof(exe_path), "cgi-bin\\%s.exe", prog);

    struct stat st;
    if (stat(exe_path, &st) == -1)
    {
        not_found(client);
        return;
    }

    // 读取剩余请求头，捕获 Content-Length 和 Content-Type
    char header[1024];
    int content_length = 0;
    char content_type[256] = "text/plain";
    while (get_line(client, header, sizeof(header)) > 0 && strcmp(header, "\n") != 0)
    {
        if (_strnicmp(header, "Content-Length:", 15) == 0)
            content_length = atoi(header + 15);
        else if (_strnicmp(header, "Content-Type:", 13) == 0)
        {
            const char* p = header + 13;
            while (*p == ' ' || *p == '\t') p++;
            strncpy(content_type, p, sizeof(content_type) - 1);
            content_type[sizeof(content_type) - 1] = 0;
            size_t L = strlen(content_type);
            while (L > 0 && (content_type[L - 1] == '\n' || content_type[L - 1] == '\r'))
                content_type[--L] = 0;
        }
    }

    // 设置 CGI 环境变量
    char e_method[64], e_query[1024], e_clen[64], e_ctype[256];
    snprintf(e_method, sizeof(e_method), "REQUEST_METHOD=%s", method);
    snprintf(e_query,  sizeof(e_query),  "QUERY_STRING=%s", query_string);
    snprintf(e_clen,   sizeof(e_clen),   "CONTENT_LENGTH=%d", content_length);
    snprintf(e_ctype,  sizeof(e_ctype),  "CONTENT_TYPE=%s", content_type);
    _putenv(e_method);
    _putenv(e_query);
    _putenv(e_clen);
    _putenv(e_ctype);
    // 注意：多线程下 _putenv 存在竞态，教学可接受

    // 创建两条匿名管道：子进程 stdout -> 服务器读；服务器写 -> 子进程 stdin
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hChildStdout_R = NULL, hChildStdout_W = NULL;
    HANDLE hChildStdin_R  = NULL, hChildStdin_W  = NULL;
    if (!CreatePipe(&hChildStdout_R, &hChildStdout_W, &sa, 0) ||
        !CreatePipe(&hChildStdin_R,  &hChildStdin_W,  &sa, 0))
    {
        error_die("CreatePipe");
    }

    // 配置子进程启动信息，将管道绑定到 stdin/stdout
    STARTUPINFOA si = { 0 };
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput  = hChildStdin_R;
    si.hStdOutput = hChildStdout_W;
    si.hStdError  = hChildStdout_W;
    si.wShowWindow = SW_HIDE;

    // 转为绝对路径后启动子进程
    char abs_exe[512];
    DWORD abs_len = GetFullPathNameA(exe_path, sizeof(abs_exe), abs_exe, NULL);
    if (abs_len == 0 || abs_len >= sizeof(abs_exe))
    {
        printf("GetFullPathName failed, error: %d\n", GetLastError());
        not_found(client);
        CloseHandle(hChildStdout_R); CloseHandle(hChildStdout_W);
        CloseHandle(hChildStdin_R);  CloseHandle(hChildStdin_W);
        return;
    }

    PROCESS_INFORMATION pi = { 0 };
    char cmdline[640];
    snprintf(cmdline, sizeof(cmdline), "\"%s\"", abs_exe);

    if (!CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    {
        printf("CreateProcess failed, error: %d\n", GetLastError());
        not_found(client);
        CloseHandle(hChildStdout_R); CloseHandle(hChildStdout_W);
        CloseHandle(hChildStdin_R);  CloseHandle(hChildStdin_W);
        return;
    }

    // 关闭父端写句柄，确保子进程退出后 ReadFile 能收到 EOF
    CloseHandle(hChildStdout_W);
    CloseHandle(hChildStdin_R);

    // POST 时写入请求体到子进程 stdin
    if (strcmp(method, "POST") == 0 && content_length > 0)
    {
        char buf[4096];
        int remaining = content_length;
        while (remaining > 0)
        {
            int to_read = remaining < (int)sizeof(buf) ? remaining : (int)sizeof(buf);
            int n = recv(client, buf, to_read, 0);
            if (n <= 0) break;
            DWORD written = 0;
            WriteFile(hChildStdin_W, buf, (DWORD)n, &written, NULL);
            remaining -= n;
        }
    }
    CloseHandle(hChildStdin_W);

    // 发送 HTTP 状态行
    const char* status_line = "HTTP/1.0 200 OK\r\n";
    send_all(client, status_line, (int)strlen(status_line));

    // 流式转发子进程 stdout 到客户端
    char out_buf[4096];
    DWORD dwRead = 0;
    while (ReadFile(hChildStdout_R, out_buf, sizeof(out_buf), &dwRead, NULL) && dwRead > 0)
    {
        send_all(client, out_buf, (int)dwRead);
    }

    // 等待子进程结束并清理
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hChildStdout_R);
}

DWORD WINAPI accept_request(LPVOID arg)
{
    char buff[1024];
    int client = (SOCKET)arg;

    int numchars = get_line(client, buff, sizeof(buff));
    PRINTF(buff);

    // 解析请求方法
    char method[255];
    int i = 0, j = 0;
    while (!isspace((unsigned char)buff[j]) && i < sizeof(method) - 1)
        method[i++] = buff[j++];
    method[i] = 0;
    PRINTF(method);

    // 检查方法是否支持
    if (stricmp(method, "GET") && stricmp(method, "POST"))
    {
        unimplement(client);
        closesocket(client);
        return 0;
    }

    // 解析 URL
    char url[255];
    i = 0;
    while (isspace((unsigned char)buff[j]) && j < sizeof(buff))
        j++;
    while (!isspace((unsigned char)buff[j]) && i < sizeof(url) - 1 && j < sizeof(buff))
        url[i++] = buff[j++];
    url[i] = 0;
    PRINTF(url);

    // 目录穿越防护
    if (strstr(url, "..") != nullptr)
    {
        not_found(client);
        closesocket(client);
        return 0;
    }

    // CGI 分支
    if (strncmp(url, "/cgi-bin/", 9) == 0)
    {
        execute_cgi(client, method, url);
        closesocket(client);
        return 0;
    }

    // 静态文件分支
    char path[512] = "";
    snprintf(path, sizeof(path), "htdocs%s", url);
    if (path[strlen(path) - 1] == '/')
        strcat(path, "index.html");
    PRINTF(path);

    struct stat status;
    if (stat(path, &status) == -1)
    {
        while (get_line(client, buff, sizeof(buff)) > 0 && strcmp(buff, "\n") != 0)
        {
            // 消费剩余请求头
        }
        not_found(client);
    }
    else
    {
        if ((status.st_mode & S_IFMT) == S_IFDIR)
            strcat(path, "/index.html");
        server_file(client, path);
    }

    closesocket(client);
    return 0;
}

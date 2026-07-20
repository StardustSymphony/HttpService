#include "response.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <iostream>

void unimplement(int client)
{
    const char* resp =
        "HTTP/1.0 501 Not Implemented\r\n"
        "Server: HaleHttp/0.1\r\n"
        "Content-type: text/html\r\n"
        "\r\n"
        "<html><head><title>501</title></head>"
        "<body><h1>501 Not Implemented</h1>"
        "<p>The requested method is not supported.</p></body></html>";
    send_all(client, resp, (int)strlen(resp));
}

void not_found(int client)
{
    static const char* body =
        "<html><head><title>404</title></head>"
        "<body><h1>404 Not Found</h1>"
        "<p>The requested resource was not found.</p></body></html>";

    char head[1024];
    int n = snprintf(head, sizeof(head),
        "HTTP/1.0 404 NOT FOUND\r\n"
        "Server: HaleHttp/0.1\r\n"
        "Content-type: text/html\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        strlen(body));

    send_all(client, head, n);
    send_all(client, body, (int)strlen(body));
}

void headers(int client, long content_length)
{
    char buff[1024];

    snprintf(buff, sizeof(buff), "HTTP/1.0 200 OK\r\n");
    send_all(client, buff, strlen(buff));

    snprintf(buff, sizeof(buff), "Server: HaleHttp/0.1\r\n");
    send_all(client, buff, strlen(buff));

    snprintf(buff, sizeof(buff), "Content-type: text/html\r\n");
    send_all(client, buff, strlen(buff));

    snprintf(buff, sizeof(buff), "Content-Length: %ld\r\n", content_length);
    send_all(client, buff, strlen(buff));

    snprintf(buff, sizeof(buff), "Connection: close\r\n");
    send_all(client, buff, strlen(buff));

    snprintf(buff, sizeof(buff), "\r\n");
    send_all(client, buff, strlen(buff));
}

void cat(int client, FILE* resource)
{
    char buff[4096];
    int count = 0;
    while (1)
    {
        int ret = fread(buff, sizeof(char), sizeof(buff), resource);
        if (ret <= 0) break;
        send_all(client, buff, ret);
        count += ret;
    }
    std::cout << "一共发送[" << count << "]字节给浏览器\n";
}

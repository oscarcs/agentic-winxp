#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int send_all(SOCKET s, const char *buf, int len)
{
    int sent;

    while (len > 0) {
        sent = send(s, buf, len, 0);
        if (sent == SOCKET_ERROR) {
            return -1;
        }
        buf += sent;
        len -= sent;
    }

    return 0;
}

int main(void)
{
    WSADATA wsa;
    SOCKET s;
    struct sockaddr_in addr;
    const char *request =
        "GET /health HTTP/1.0\r\n"
        "Host: 10.0.2.2\r\n"
        "User-Agent: winxp-tcc-netcheck\r\n"
        "\r\n";
    char buf[512];
    int n;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        printf("socket failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(7777);
    addr.sin_addr.s_addr = inet_addr("10.0.2.2");

    if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("connect failed: %d\n", WSAGetLastError());
        closesocket(s);
        WSACleanup();
        return 1;
    }

    if (send_all(s, request, (int)strlen(request)) != 0) {
        printf("send failed: %d\n", WSAGetLastError());
        closesocket(s);
        WSACleanup();
        return 1;
    }

    while ((n = recv(s, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[n] = '\0';
        printf("%s", buf);
    }

    if (n == SOCKET_ERROR) {
        printf("\nrecv failed: %d\n", WSAGetLastError());
    }

    closesocket(s);
    WSACleanup();
    return n == SOCKET_ERROR ? 1 : 0;
}


#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../portable/agent_core.h"

#ifndef CP_UTF8
#define CP_UTF8 65001
#endif

typedef struct socket_transport {
    SOCKET socket;
} socket_transport;

static int socket_write_all(void *ctx, const char *data, int len)
{
    socket_transport *transport = (socket_transport *)ctx;
    int written = 0;

    while (written < len) {
        int n = send(transport->socket, data + written, len - written, 0);
        if (n == SOCKET_ERROR || n == 0) {
            return -1;
        }
        written += n;
    }
    return 0;
}

static int socket_read_line(void *ctx, char *buf, int cap)
{
    socket_transport *transport = (socket_transport *)ctx;
    int i = 0;

    while (i < cap - 1) {
        char ch;
        int n = recv(transport->socket, &ch, 1, 0);
        if (n <= 0) {
            return -1;
        }
        buf[i++] = ch;
        if (ch == '\n') {
            buf[i] = '\0';
            return 0;
        }
    }
    buf[cap - 1] = '\0';
    return -1;
}

static int socket_read_exact(void *ctx, char *buf, int len)
{
    socket_transport *transport = (socket_transport *)ctx;
    int got = 0;

    while (got < len) {
        int n = recv(transport->socket, buf + got, len - got, 0);
        if (n <= 0) {
            return -1;
        }
        got += n;
    }
    return 0;
}

static SOCKET connect_tcp(const char *host, int port)
{
    SOCKET s;
    struct sockaddr_in addr;

    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        return INVALID_SOCKET;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    addr.sin_addr.s_addr = inet_addr(host);

    if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(s);
        return INVALID_SOCKET;
    }

    return s;
}

static void trim_newline(char *s)
{
    int n = (int)strlen(s);

    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = '\0';
        n--;
    }
}

static int wide_to_utf8(const WCHAR *wide, char *out, int out_cap)
{
    int n;

    n = WideCharToMultiByte(CP_UTF8, 0, wide, -1, out, out_cap, NULL, NULL);
    if (n <= 0) {
        return -1;
    }
    return n - 1;
}

static int console_to_utf8(const char *input, char *out, int out_cap)
{
    WCHAR wide[AG_MAX_MESSAGE + 1];
    int n;

    n = MultiByteToWideChar(CP_OEMCP, 0, input, -1, wide,
                            sizeof(wide) / sizeof(wide[0]));
    if (n <= 0) {
        lstrcpyn(out, input, out_cap);
        return (int)strlen(out);
    }
    n = wide_to_utf8(wide, out, out_cap);
    if (n < 0) {
        lstrcpyn(out, input, out_cap);
        return (int)strlen(out);
    }
    return n;
}

static void normalize_for_console(WCHAR *wide)
{
    int i;

    for (i = 0; wide[i]; i++) {
        switch (wide[i]) {
        case 0x2018:
        case 0x2019:
        case 0x201A:
        case 0x201B:
            wide[i] = '\'';
            break;
        case 0x201C:
        case 0x201D:
        case 0x201E:
        case 0x201F:
            wide[i] = '"';
            break;
        case 0x2013:
        case 0x2014:
        case 0x2212:
            wide[i] = '-';
            break;
        case 0x2026:
            wide[i] = '.';
            break;
        case 0x00A0:
            wide[i] = ' ';
            break;
        default:
            break;
        }
    }
}

static void print_utf8_console(const char *prefix, const char *utf8)
{
    WCHAR wide[AG_MAX_MESSAGE + 1];
    char console_text[(AG_MAX_MESSAGE * 2) + 1];
    int n;

    if (prefix) {
        printf("%s", prefix);
    }

    n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide,
                            sizeof(wide) / sizeof(wide[0]));
    if (n <= 0) {
        printf("%s\n", utf8);
        return;
    }

    normalize_for_console(wide);
    n = WideCharToMultiByte(CP_OEMCP, 0, wide, -1, console_text,
                            sizeof(console_text), NULL, NULL);
    if (n <= 0) {
        printf("%s\n", utf8);
        return;
    }
    printf("%s\n", console_text);
}

int main(int argc, char **argv)
{
    const char *host = "10.0.2.2";
    int port = 7790;
    int id = 1;
    char input[AG_MAX_MESSAGE + 1];
    char wire_input[AG_MAX_MESSAGE + 1];
    WSADATA wsa;
    SOCKET s;
    socket_transport socket_ctx;
    ag_transport transport;

    if (argc >= 2) {
        host = argv[1];
    }
    if (argc >= 3) {
        port = atoi(argv[2]);
    }

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

    s = connect_tcp(host, port);
    if (s == INVALID_SOCKET) {
        printf("connect failed: %s:%d error=%d\n", host, port, WSAGetLastError());
        WSACleanup();
        return 1;
    }

    socket_ctx.socket = s;
    transport.ctx = &socket_ctx;
    transport.write_all = socket_write_all;
    transport.read_line = socket_read_line;
    transport.read_exact = socket_read_exact;

    printf("xpagent connected to %s:%d\n", host, port);
    printf("type /quit to exit\n\n");

    for (;;) {
        ag_message reply;

        printf("xpagent> ");
        fflush(stdout);
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }
        trim_newline(input);
        if (strcmp(input, "/quit") == 0) {
            ag_send(&transport, "BYE", id++, "bye", 3);
            break;
        }
        if (input[0] == '\0') {
            continue;
        }

        console_to_utf8(input, wire_input, sizeof(wire_input));
        if (ag_exchange(&transport, id++, wire_input, &reply) != 0) {
            printf("agent exchange failed\n");
            closesocket(s);
            WSACleanup();
            return 1;
        }

        printf("%s: ", reply.type);
        print_utf8_console(NULL, reply.body);
    }

    closesocket(s);
    WSACleanup();
    return 0;
}

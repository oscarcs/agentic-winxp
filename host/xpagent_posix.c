#include "../portable/agent_core.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct socket_transport {
    int fd;
} socket_transport;

static int socket_write_all(void *ctx, const char *data, int len)
{
    socket_transport *transport = (socket_transport *)ctx;
    int written = 0;

    while (written < len) {
        ssize_t n = write(transport->fd, data + written, len - written);
        if (n <= 0) {
            return -1;
        }
        written += (int)n;
    }
    return 0;
}

static int socket_read_line(void *ctx, char *buf, int cap)
{
    socket_transport *transport = (socket_transport *)ctx;
    int i = 0;

    while (i < cap - 1) {
        char ch;
        ssize_t n = read(transport->fd, &ch, 1);
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
        ssize_t n = read(transport->fd, buf + got, len - got);
        if (n <= 0) {
            return -1;
        }
        got += (int)n;
    }
    return 0;
}

static int connect_tcp(const char *host, int port)
{
    int fd;
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static void trim_newline(char *s)
{
    int n = (int)strlen(s);

    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = '\0';
        n--;
    }
}

int main(int argc, char **argv)
{
    const char *host = "127.0.0.1";
    int port = 7790;
    int fd;
    int id = 1;
    char input[AG_MAX_MESSAGE + 1];
    socket_transport socket_ctx;
    ag_transport transport;

    if (argc >= 2) {
        host = argv[1];
    }
    if (argc >= 3) {
        port = atoi(argv[2]);
    }

    fd = connect_tcp(host, port);
    if (fd < 0) {
        fprintf(stderr, "connect failed: %s:%d (%s)\n", host, port,
                strerror(errno));
        return 1;
    }

    socket_ctx.fd = fd;
    transport.ctx = &socket_ctx;
    transport.write_all = socket_write_all;
    transport.read_line = socket_read_line;
    transport.read_exact = socket_read_exact;

    printf("xpagent local test connected to %s:%d\n", host, port);
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

        if (ag_exchange(&transport, id++, input, &reply) != 0) {
            fprintf(stderr, "agent exchange failed\n");
            close(fd);
            return 1;
        }

        printf("%s: %s\n", reply.type, reply.body);
    }

    close(fd);
    return 0;
}

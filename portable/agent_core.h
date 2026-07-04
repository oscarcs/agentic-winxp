#ifndef AGENT_CORE_H
#define AGENT_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#define AG_MAX_MESSAGE 8192

typedef struct ag_transport {
    void *ctx;
    int (*write_all)(void *ctx, const char *data, int len);
    int (*read_line)(void *ctx, char *buf, int cap);
    int (*read_exact)(void *ctx, char *buf, int len);
} ag_transport;

typedef struct ag_message {
    char type[16];
    int id;
    int len;
    char body[AG_MAX_MESSAGE + 1];
} ag_message;

int ag_send(ag_transport *transport, const char *type, int id,
            const char *body, int len);
int ag_recv(ag_transport *transport, ag_message *message);
int ag_exchange(ag_transport *transport, int id, const char *user_text,
                ag_message *reply);

#ifdef __cplusplus
}
#endif

#endif

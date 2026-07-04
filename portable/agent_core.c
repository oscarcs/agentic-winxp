#include "agent_core.h"

#include <stdio.h>
#include <string.h>

static int ag_strlen(const char *s)
{
    int n = 0;

    if (!s) {
        return 0;
    }
    while (s[n]) {
        n++;
    }
    return n;
}

int ag_send(ag_transport *transport, const char *type, int id,
            const char *body, int len)
{
    char header[64];
    int header_len;

    if (!transport || !transport->write_all || !type) {
        return -1;
    }
    if (!body) {
        body = "";
    }
    if (len < 0) {
        len = ag_strlen(body);
    }

    header_len = sprintf(header, "AG1 %s %d %d\n", type, id, len);
    if (transport->write_all(transport->ctx, header, header_len) != 0) {
        return -1;
    }
    if (len > 0 && transport->write_all(transport->ctx, body, len) != 0) {
        return -1;
    }
    return 0;
}

int ag_recv(ag_transport *transport, ag_message *message)
{
    char line[128];
    char magic[8];
    int fields;

    if (!transport || !transport->read_line || !transport->read_exact ||
        !message) {
        return -1;
    }

    memset(message, 0, sizeof(*message));
    if (transport->read_line(transport->ctx, line, sizeof(line)) != 0) {
        return -1;
    }

    fields = sscanf(line, "%7s %15s %d %d", magic, message->type, &message->id,
                    &message->len);
    if (fields != 4 || strcmp(magic, "AG1") != 0) {
        return -1;
    }
    if (message->len < 0 || message->len > AG_MAX_MESSAGE) {
        return -1;
    }
    if (message->len > 0 &&
        transport->read_exact(transport->ctx, message->body, message->len) !=
            0) {
        return -1;
    }
    message->body[message->len] = '\0';
    return 0;
}

int ag_exchange(ag_transport *transport, int id, const char *user_text,
                ag_message *reply)
{
    if (ag_send(transport, "USER", id, user_text, -1) != 0) {
        return -1;
    }
    return ag_recv(transport, reply);
}

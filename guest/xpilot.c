#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifndef CREATE_NO_WINDOW
#define CREATE_NO_WINDOW 0x08000000
#endif

#define XPILOT_VERSION "0.3"
#define DEFAULT_TIMEOUT_MS 30000
#define DEFAULT_MAX_OUTPUT (1024 * 1024)
#define HARD_MAX_OUTPUT (4 * 1024 * 1024)
#define MAX_FILE_SIZE (16 * 1024 * 1024)
#define MAX_COMMAND_SIZE 16384

struct buffer {
    char *data;
    long len;
    long cap;
    long limit;
    int truncated;
};

static char current_dir[MAX_PATH];

static void log_line(const char *fmt, ...)
{
    SYSTEMTIME st;
    va_list ap;

    GetLocalTime(&st);
    printf("[%02u:%02u:%02u] ", st.wHour, st.wMinute, st.wSecond);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
    fflush(stdout);
}

static void log_preview(const char *label, long id, const char *value)
{
    char tmp[81];
    int i;

    if (!value) {
        value = "";
    }
    for (i = 0; i < 77 && value[i]; i++) {
        tmp[i] = value[i];
    }
    tmp[i] = '\0';
    if (value[i]) {
        strcat(tmp, "...");
    }
    log_line("#%ld %s %s", id, label, tmp);
}

static int send_all(SOCKET s, const char *buf, long len)
{
    int sent;

    while (len > 0) {
        sent = send(s, buf, len > 32767 ? 32767 : (int)len, 0);
        if (sent == SOCKET_ERROR) {
            return -1;
        }
        buf += sent;
        len -= sent;
    }

    return 0;
}

static int recv_exact(SOCKET s, char *buf, long len)
{
    int got;

    while (len > 0) {
        got = recv(s, buf, len > 32767 ? 32767 : (int)len, 0);
        if (got <= 0) {
            return -1;
        }
        buf += got;
        len -= got;
    }

    return 0;
}

static int read_line(SOCKET s, char *buf, int cap)
{
    int n;
    int i = 0;
    char ch;

    while (i < cap - 1) {
        n = recv(s, &ch, 1, 0);
        if (n <= 0) {
            return -1;
        }
        buf[i++] = ch;
        if (ch == '\n') {
            buf[i] = '\0';
            return i;
        }
    }

    buf[cap - 1] = '\0';
    return -1;
}

static char *recv_part(SOCKET s, long len)
{
    char *data;

    if (len < 0 || len > MAX_FILE_SIZE) {
        return NULL;
    }

    data = (char *)malloc(len + 1);
    if (!data) {
        return NULL;
    }
    if (recv_exact(s, data, len) != 0) {
        free(data);
        return NULL;
    }
    data[len] = '\0';
    return data;
}

static int send_response(SOCKET s, const char *status, long id, long code,
                         const char *body, long body_len)
{
    char header[128];
    int header_len;

    if (!body) {
        body = "";
        body_len = 0;
    }

    header_len = sprintf(header, "%s %ld %ld %ld\n", status, id, code, body_len);
    if (send_all(s, header, header_len) != 0) {
        return -1;
    }
    return send_all(s, body, body_len);
}

static void buffer_init(struct buffer *buf, long limit)
{
    memset(buf, 0, sizeof(*buf));
    if (limit <= 0) {
        limit = DEFAULT_MAX_OUTPUT;
    }
    if (limit > HARD_MAX_OUTPUT) {
        limit = HARD_MAX_OUTPUT;
    }
    buf->limit = limit;
}

static int buffer_append(struct buffer *buf, const char *data, long len)
{
    long wanted;
    long new_cap;
    char *new_data;
    long room;

    if (len <= 0) {
        return 0;
    }

    if (buf->len >= buf->limit) {
        buf->truncated = 1;
        return 0;
    }

    room = buf->limit - buf->len;
    if (len > room) {
        len = room;
        buf->truncated = 1;
    }

    wanted = buf->len + len + 1;
    if (wanted > buf->cap) {
        new_cap = buf->cap ? buf->cap * 2 : 4096;
        while (new_cap < wanted) {
            new_cap *= 2;
        }
        new_data = (char *)realloc(buf->data, new_cap);
        if (!new_data) {
            return -1;
        }
        buf->data = new_data;
        buf->cap = new_cap;
    }

    memcpy(buf->data + buf->len, data, len);
    buf->len += len;
    buf->data[buf->len] = '\0';
    return 0;
}

static int buffer_append_note(struct buffer *buf, const char *data, long len)
{
    long old_limit;
    int rc;

    old_limit = buf->limit;
    buf->limit = buf->len + len + 1;
    rc = buffer_append(buf, data, len);
    buf->limit = old_limit;
    return rc;
}

static int buffer_printf(struct buffer *buf, const char *fmt, ...)
{
    va_list ap;
    char tmp[1024];
    int n;

    va_start(ap, fmt);
    n = vsprintf(tmp, fmt, ap);
    va_end(ap);
    if (n < 0) {
        return -1;
    }
    return buffer_append(buf, tmp, n);
}

static void buffer_free(struct buffer *buf)
{
    if (buf->data) {
        free(buf->data);
    }
    memset(buf, 0, sizeof(*buf));
}

static void init_current_dir(void)
{
    DWORD n;

    n = GetCurrentDirectory(sizeof(current_dir), current_dir);
    if (n == 0 || n >= sizeof(current_dir)) {
        strcpy(current_dir, "C:\\");
        SetCurrentDirectory(current_dir);
    }
}

static int set_current_dir(const char *path, struct buffer *out)
{
    DWORD n;

    if (path && path[0]) {
        log_preview("cwd ->", 0, path);
        if (!SetCurrentDirectory(path)) {
            log_line("cwd failed: %s error=%lu", path, GetLastError());
            buffer_printf(out, "cannot cd to %s\r\n", path);
            return 1;
        }
    }

    n = GetCurrentDirectory(sizeof(current_dir), current_dir);
    if (n == 0 || n >= sizeof(current_dir)) {
        buffer_append(out, "cannot read current directory\r\n", 31);
        return 1;
    }

    buffer_printf(out, "%s\r\n", current_dir);
    log_line("cwd is %s", current_dir);
    return 0;
}

static long run_command(const char *command, const char *cwd, long timeout_ms,
                        long max_output, struct buffer *out)
{
    SECURITY_ATTRIBUTES sa;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    HANDLE read_pipe = NULL;
    HANDLE write_pipe = NULL;
    HANDLE nul = NULL;
    char *cmdline;
    DWORD avail;
    DWORD got;
    DWORD exit_code = 1;
    DWORD wait_result;
    DWORD start_tick;
    char tmp[4096];
    int done = 0;
    int timed_out = 0;
    const char *run_dir;

    buffer_init(out, max_output);
    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    if (!command || !command[0]) {
        buffer_append(out, "empty command\r\n", 15);
        return 1;
    }

    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
        buffer_append(out, "CreatePipe failed\r\n", 19);
        return 1;
    }
    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

    nul = CreateFile("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                     &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    cmdline = (char *)malloc(strlen(command) + 12);
    if (!cmdline) {
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);
        if (nul != INVALID_HANDLE_VALUE && nul != NULL) {
            CloseHandle(nul);
        }
        buffer_append(out, "out of memory\r\n", 15);
        return 1;
    }
    strcpy(cmdline, "cmd.exe /C ");
    strcat(cmdline, command);

    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = write_pipe;
    si.hStdError = write_pipe;
    si.hStdInput = (nul != INVALID_HANDLE_VALUE && nul != NULL)
                       ? nul
                       : GetStdHandle(STD_INPUT_HANDLE);

    run_dir = (cwd && cwd[0]) ? cwd : current_dir;
    log_line("run start cwd=%s timeout=%ld max=%ld", run_dir, timeout_ms,
             max_output);
    if (!CreateProcess(NULL, cmdline, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL,
                       run_dir, &si, &pi)) {
        log_line("run createprocess failed error=%lu", GetLastError());
        buffer_printf(out, "CreateProcess failed in %s\r\n", run_dir);
        free(cmdline);
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);
        if (nul != INVALID_HANDLE_VALUE && nul != NULL) {
            CloseHandle(nul);
        }
        return 1;
    }

    free(cmdline);
    CloseHandle(write_pipe);
    if (nul != INVALID_HANDLE_VALUE && nul != NULL) {
        CloseHandle(nul);
    }

    start_tick = GetTickCount();
    while (!done) {
        avail = 0;
        if (PeekNamedPipe(read_pipe, NULL, 0, NULL, &avail, NULL) && avail > 0) {
            if (ReadFile(read_pipe, tmp,
                         avail > sizeof(tmp) ? sizeof(tmp) : avail,
                         &got, NULL) &&
                got > 0) {
                buffer_append(out, tmp, (long)got);
            }
            continue;
        }

        wait_result = WaitForSingleObject(pi.hProcess, 50);
        if (wait_result == WAIT_OBJECT_0) {
            done = 1;
        } else if (timeout_ms > 0 &&
                   (long)(GetTickCount() - start_tick) > timeout_ms) {
            timed_out = 1;
            log_line("run timeout after %ld ms", timeout_ms);
            TerminateProcess(pi.hProcess, 124);
            WaitForSingleObject(pi.hProcess, 1000);
            done = 1;
        }
    }

    while (PeekNamedPipe(read_pipe, NULL, 0, NULL, &avail, NULL) && avail > 0) {
        if (!ReadFile(read_pipe, tmp, avail > sizeof(tmp) ? sizeof(tmp) : avail,
                      &got, NULL) ||
            got == 0) {
            break;
        }
        buffer_append(out, tmp, (long)got);
    }

    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(read_pipe);

    if (timed_out) {
        buffer_printf(out, "\r\n[xpilot: timed out after %ld ms]\r\n", timeout_ms);
        exit_code = 124;
    }
    if (out->truncated) {
        buffer_append_note(out, "\r\n[xpilot: output truncated]\r\n", 29);
    }

    log_line("run done exit=%lu bytes=%ld%s", exit_code, out->len,
             out->truncated ? " truncated" : "");
    return (long)exit_code;
}

static int read_file(const char *path, char **data, long *len)
{
    FILE *f;
    long size;

    *data = NULL;
    *len = 0;

    f = fopen(path, "rb");
    if (!f) {
        log_line("get failed path=%s", path);
        return 1;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return 1;
    }
    size = ftell(f);
    if (size < 0 || size > MAX_FILE_SIZE) {
        log_line("get too large path=%s size=%ld", path, size);
        fclose(f);
        return 2;
    }
    rewind(f);

    *data = (char *)malloc(size ? size : 1);
    if (!*data) {
        fclose(f);
        return 1;
    }
    if (size > 0 && fread(*data, 1, size, f) != (size_t)size) {
        free(*data);
        *data = NULL;
        fclose(f);
        return 1;
    }
    fclose(f);
    *len = size;
    log_line("get path=%s bytes=%ld", path, size);
    return 0;
}

static int write_file_mode(const char *path, const char *data, long len,
                           const char *mode)
{
    FILE *f;

    f = fopen(path, mode);
    if (!f) {
        log_line("write failed path=%s mode=%s", path, mode);
        return 1;
    }
    if (len > 0 && fwrite(data, 1, len, f) != (size_t)len) {
        fclose(f);
        return 1;
    }
    fclose(f);
    log_line("write path=%s mode=%s bytes=%ld", path, mode, len);
    return 0;
}

static int stat_path(const char *path, struct buffer *out)
{
    WIN32_FIND_DATA fd;
    HANDLE h;
    SYSTEMTIME st;
    const char *type;

    buffer_init(out, 4096);
    h = FindFirstFile(path, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        log_line("stat not found path=%s", path);
        buffer_printf(out, "not found: %s\r\n", path);
        return 1;
    }
    FindClose(h);

    type = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? "directory" : "file";
    buffer_printf(out, "path: %s\r\n", path);
    buffer_printf(out, "name: %s\r\n", fd.cFileName);
    buffer_printf(out, "type: %s\r\n", type);
    buffer_printf(out, "attrs: 0x%08lx\r\n", fd.dwFileAttributes);
    buffer_printf(out, "size_low: %lu\r\n", fd.nFileSizeLow);
    buffer_printf(out, "size_high: %lu\r\n", fd.nFileSizeHigh);
    if (FileTimeToSystemTime(&fd.ftLastWriteTime, &st)) {
        buffer_printf(out, "mtime_utc: %04u-%02u-%02u %02u:%02u:%02u\r\n",
                      st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
                      st.wSecond);
    }
    log_line("stat path=%s type=%s size=%lu", path, type, fd.nFileSizeLow);
    return 0;
}

static void build_info(struct buffer *out)
{
    char win_dir[MAX_PATH];
    char sys_dir[MAX_PATH];
    char *value;
    DWORD version;

    buffer_init(out, 4096);
    version = GetVersion();
    GetWindowsDirectory(win_dir, sizeof(win_dir));
    GetSystemDirectory(sys_dir, sizeof(sys_dir));

    buffer_printf(out, "xpilot_version: %s\r\n", XPILOT_VERSION);
    buffer_printf(out, "cwd: %s\r\n", current_dir);
    buffer_printf(out, "pid: %lu\r\n", GetCurrentProcessId());
    buffer_printf(out, "windows_version_raw: 0x%08lx\r\n", version);
    buffer_printf(out, "windows_directory: %s\r\n", win_dir);
    buffer_printf(out, "system_directory: %s\r\n", sys_dir);

    value = getenv("COMPUTERNAME");
    if (value) {
        buffer_printf(out, "computername: %s\r\n", value);
    }
    value = getenv("USERNAME");
    if (value) {
        buffer_printf(out, "username: %s\r\n", value);
    }
    value = getenv("PROCESSOR_ARCHITECTURE");
    if (value) {
        buffer_printf(out, "processor_architecture: %s\r\n", value);
    }
}

static int handle_connection(SOCKET s)
{
    char line[512];
    char verb[16];
    long id;
    long a;
    long b;
    long c;
    long d;
    int fields;
    char *body1 = NULL;
    char *body2 = NULL;
    char *file_data = NULL;
    long file_len = 0;
    long exit_code;
    int rc;
    struct buffer output;

    for (;;) {
        if (read_line(s, line, sizeof(line)) < 0) {
            return 0;
        }

        verb[0] = '\0';
        id = 0;
        a = 0;
        b = 0;
        c = 0;
        d = 0;
        fields = sscanf(line, "%15s %ld %ld %ld %ld %ld", verb, &id, &a, &b,
                        &c, &d);

        if (fields < 2) {
            send_response(s, "ERR", id, 1, "bad request\r\n", 13);
            continue;
        }

        if (strcmp(verb, "PING") == 0) {
            log_line("#%ld ping", id);
            send_response(s, "OK", id, 0, "pong from Windows XP\r\n", 22);
            continue;
        }

        if (strcmp(verb, "INFO") == 0) {
            log_line("#%ld info", id);
            build_info(&output);
            send_response(s, "OK", id, 0, output.data, output.len);
            buffer_free(&output);
            continue;
        }

        if (strcmp(verb, "QUIT") == 0) {
            log_line("#%ld quit", id);
            send_response(s, "OK", id, 0, "bye\r\n", 5);
            return 1;
        }

        if (strcmp(verb, "CWD") == 0) {
            log_line("#%ld cwd", id);
            buffer_init(&output, 4096);
            if (fields >= 3 && a > 0 && a <= MAX_PATH) {
                body1 = recv_part(s, a);
                if (!body1) {
                    buffer_append(&output, "bad cwd payload\r\n", 17);
                    send_response(s, "ERR", id, 1, output.data, output.len);
                    buffer_free(&output);
                    continue;
                }
                rc = set_current_dir(body1, &output);
                free(body1);
                body1 = NULL;
            } else {
                rc = set_current_dir(NULL, &output);
            }
            send_response(s, rc == 0 ? "OK" : "ERR", id, rc, output.data,
                          output.len);
            buffer_free(&output);
            continue;
        }

        if (strcmp(verb, "RUN") == 0) {
            if (fields < 3 || a < 0 || a > MAX_COMMAND_SIZE) {
                send_response(s, "ERR", id, 1, "bad RUN length\r\n", 16);
                continue;
            }
            body1 = recv_part(s, a);
            if (!body1) {
                send_response(s, "ERR", id, 1, "bad RUN payload\r\n", 17);
                continue;
            }

            log_preview("run", id, body1);
            exit_code = run_command(body1, current_dir, DEFAULT_TIMEOUT_MS,
                                    DEFAULT_MAX_OUTPUT, &output);
            free(body1);
            body1 = NULL;
            send_response(s, "OK", id, exit_code, output.data, output.len);
            buffer_free(&output);
            continue;
        }

        if (strcmp(verb, "RUNX") == 0) {
            if (fields < 6 || c < 0 || c > MAX_COMMAND_SIZE || d < 0 ||
                d > MAX_PATH) {
                send_response(s, "ERR", id, 1, "bad RUNX header\r\n", 18);
                continue;
            }
            body1 = recv_part(s, c);
            body2 = recv_part(s, d);
            if (!body1 || !body2) {
                if (body1) free(body1);
                if (body2) free(body2);
                send_response(s, "ERR", id, 1, "bad RUNX payload\r\n", 18);
                continue;
            }
            log_preview("run", id, body1);
            exit_code = run_command(body1, body2[0] ? body2 : current_dir, a, b,
                                    &output);
            free(body1);
            free(body2);
            body1 = NULL;
            body2 = NULL;
            send_response(s, "OK", id, exit_code, output.data, output.len);
            buffer_free(&output);
            continue;
        }

        if (strcmp(verb, "GET") == 0 || strcmp(verb, "STAT") == 0) {
            if (fields < 3 || a < 0 || a > MAX_PATH) {
                send_response(s, "ERR", id, 1, "bad path length\r\n", 17);
                continue;
            }
            body1 = recv_part(s, a);
            if (!body1) {
                send_response(s, "ERR", id, 1, "bad path payload\r\n", 18);
                continue;
            }

            if (strcmp(verb, "STAT") == 0) {
                log_preview("stat", id, body1);
                rc = stat_path(body1, &output);
                free(body1);
                body1 = NULL;
                send_response(s, rc == 0 ? "OK" : "ERR", id, rc, output.data,
                              output.len);
                buffer_free(&output);
                continue;
            }

            log_preview("get", id, body1);
            rc = read_file(body1, &file_data, &file_len);
            free(body1);
            body1 = NULL;
            if (rc == 0) {
                send_response(s, "OK", id, 0, file_data, file_len);
                free(file_data);
                file_data = NULL;
            } else if (rc == 2) {
                send_response(s, "ERR", id, rc, "file too large\r\n", 16);
            } else {
                send_response(s, "ERR", id, rc, "cannot read file\r\n", 18);
            }
            continue;
        }

        if (strcmp(verb, "PUT") == 0 || strcmp(verb, "APPEND") == 0) {
            if (fields < 4 || a < 0 || a > MAX_PATH || b < 0 ||
                b > MAX_FILE_SIZE) {
                send_response(s, "ERR", id, 1, "bad write length\r\n", 18);
                continue;
            }
            body1 = recv_part(s, a);
            body2 = recv_part(s, b);
            if (!body1 || !body2) {
                if (body1) free(body1);
                if (body2) free(body2);
                send_response(s, "ERR", id, 1, "bad write payload\r\n", 19);
                continue;
            }
            log_preview(strcmp(verb, "APPEND") == 0 ? "append" : "put", id,
                        body1);
            rc = write_file_mode(body1, body2, b,
                                 strcmp(verb, "APPEND") == 0 ? "ab" : "wb");
            free(body1);
            free(body2);
            body1 = NULL;
            body2 = NULL;
            if (rc == 0) {
                send_response(s, "OK", id, 0, "wrote file\r\n", 12);
            } else {
                send_response(s, "ERR", id, rc, "cannot write file\r\n", 19);
            }
            continue;
        }

        log_line("#%ld unknown verb=%s", id, verb);
        send_response(s, "ERR", id, 1, "unknown verb\r\n", 14);
    }
}

static SOCKET connect_to_host(const char *host, int port)
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
        log_line("connect failed error=%d", WSAGetLastError());
        closesocket(s);
        return INVALID_SOCKET;
    }

    return s;
}

int main(int argc, char **argv)
{
    const char *host = "10.0.2.2";
    int port = 7778;
    WSADATA wsa;
    SOCKET s;
    int should_exit;

    if (argc >= 2) {
        host = argv[1];
    }
    if (argc >= 3) {
        port = atoi(argv[2]);
    }

    init_current_dir();

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

    log_line("xpilot %s starting cwd=%s", XPILOT_VERSION, current_dir);
    log_line("connecting to %s:%d", host, port);
    for (;;) {
        s = connect_to_host(host, port);
        if (s == INVALID_SOCKET) {
            log_line("retrying in 1000 ms");
            Sleep(1000);
            continue;
        }

        log_line("connected; cwd=%s", current_dir);
        should_exit = handle_connection(s);
        closesocket(s);
        if (should_exit) {
            log_line("exiting by request");
            break;
        }

        log_line("disconnected; retrying in 1000 ms");
        Sleep(1000);
    }

    WSACleanup();
    return 0;
}

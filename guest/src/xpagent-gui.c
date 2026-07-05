#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <string.h>

#include "../../portable/agent_core.h"

#ifndef BS_FLAT
#define BS_FLAT 0x8000
#endif

#ifndef CP_UTF8
#define CP_UTF8 65001
#endif

#ifndef WM_APP
#define WM_APP 0x8000
#endif

#define WM_XPAGENT_STATUS (WM_APP + 1)
#define WM_XPAGENT_REPLY (WM_APP + 2)
#define WM_XPAGENT_ERROR (WM_APP + 3)
#define WM_XPAGENT_PROJECTS (WM_APP + 4)

#define GATEWAY_HOST "10.0.2.2"
#define GATEWAY_PORT 7790
#define EDIT_TEXT_LIMIT 60000
#define MAX_PROJECTS 16
#define MAX_THREADS 32
#define MAX_PROJECT_ROWS 64
#define PROJECT_ROW_PROJECT 1
#define PROJECT_ROW_THREAD 2
#define DEFAULT_TRANSCRIPT \
    "Ready for a new XPAgent conversation.\r\n\r\n" \
    "Type a message below and click Send to talk to the host gateway."

#define ID_NEW_CHAT 1001
#define ID_SEARCH 1002
#define ID_SCHEDULED 1003
#define ID_PLUGINS 1004
#define ID_OPEN_IN 1005
#define ID_THREAD_TITLE 1006
#define ID_STATUS 1007
#define ID_PROJECTS_LABEL 1010
#define ID_PROJECTS_LIST 1011
#define ID_USER_BADGE 1012
#define ID_USER_TEXT 1013
#define ID_TRANSCRIPT 1014
#define ID_CHANGES_LABEL 1015
#define ID_CHANGES_LIST 1016
#define ID_PROMPT_LABEL 1017
#define ID_PROMPT 1018
#define ID_ADD 1019
#define ID_ACCESS 1020
#define ID_MODEL 1021
#define ID_MIC 1022
#define ID_SEND 1023
#define ID_ENV_LABEL 1024
#define ID_ENV_LIST 1025
#define ID_TASKS_LABEL 1026
#define ID_TASKS_LIST 1027
#define ID_USAGE_TEXT 1029
#define ID_STATUS_BAR 1030
#define ID_TOOLSTRIP 1031

static const char *CLASS_NAME = "XPAgentGuiShell";

static HINSTANCE g_hinst;
static HFONT g_font;
static HFONT g_bold_font;
static HFONT g_mono_font;
static HFONT g_small_font;
static HBRUSH g_bg_brush;
static HBRUSH g_header_brush;
static HBRUSH g_white_brush;
static HICON g_app_icon_big;
static HICON g_app_icon_small;
static HICON g_icon_new_chat;
static HICON g_icon_search;
static HICON g_icon_scheduled;
static HICON g_icon_plugins;
static HICON g_icon_add;
static HICON g_icon_mic;
static HICON g_icon_send;
static HICON g_icon_file;
static HICON g_icon_project_open;
static HICON g_icon_project_closed;
static HICON g_icon_badge;
static HBITMAP g_bmp_new_chat;
static HBITMAP g_bmp_search;
static HBITMAP g_bmp_scheduled;
static HBITMAP g_bmp_plugins;
static HBITMAP g_bmp_add;
static HBITMAP g_bmp_mic;
static HBITMAP g_bmp_send;
static HBITMAP g_bmp_file;
static HBITMAP g_bmp_project_open;
static HBITMAP g_bmp_project_closed;
static HBITMAP g_bmp_badge;

static HWND g_new_chat;
static HWND g_search;
static HWND g_scheduled;
static HWND g_plugins;
static HWND g_open_in;
static HWND g_toolstrip;
static HWND g_thread_title;
static HWND g_status;
static HWND g_projects_label;
static HWND g_projects_list;
static HWND g_user_badge;
static HWND g_user_text;
static HWND g_usage_text;
static HWND g_transcript;
static HWND g_changes_label;
static HWND g_changes_list;
static HWND g_prompt_label;
static HWND g_prompt;
static HWND g_add;
static HWND g_access;
static HWND g_model;
static HWND g_mic;
static HWND g_send;
static HWND g_env_label;
static HWND g_env_list;
static HWND g_tasks_label;
static HWND g_tasks_list;
static HWND g_status_bar;

typedef struct socket_transport {
    SOCKET socket;
} socket_transport;

typedef struct gui_request {
    HWND hwnd;
    int id;
    DWORD started_at;
    char body[AG_MAX_MESSAGE + 1];
} gui_request;

typedef struct project_model {
    char name[64];
    int expanded;
} project_model;

typedef struct conversation_model {
    int project_index;
    char title[128];
    char age[32];
    char transcript[EDIT_TEXT_LIMIT + 1];
    char prompt[AG_MAX_MESSAGE + 1];
    int message_count;
} conversation_model;

static SOCKET g_gateway_socket = INVALID_SOCKET;
static CRITICAL_SECTION g_socket_lock;
static int g_socket_lock_ready;
static int g_gateway_connected;
static int g_winsock_ready;
static int g_request_in_flight;
static int g_next_request_id = 1;
static int g_message_count;
static char g_status_bar_state[64] = "Ready";
static WNDPROC g_prompt_proc;
static project_model g_projects[MAX_PROJECTS];
static conversation_model g_threads[MAX_THREADS];
static int g_project_count;
static int g_thread_count;
static int g_active_project = -1;
static int g_active_thread = -1;
static int g_next_new_chat = 1;

static void refresh_runtime_lists(void);
static void handle_send(HWND hwnd);
static void reset_conversation(HWND hwnd);
static void handle_reconnect(HWND hwnd);
static void copy_focused_control(void);
static void select_all_focused_control(void);
static void populate_projects_list(void);
static void save_active_conversation(void);
static void request_project_metadata(HWND hwnd);
static void retitle_active_conversation_from_prompt(const char *prompt);

static char *heap_dup_text(const char *text)
{
    char *copy;
    int len;

    if (!text) {
        text = "";
    }
    len = lstrlen(text) + 1;
    copy = (char *)HeapAlloc(GetProcessHeap(), 0, len);
    if (copy) {
        lstrcpy(copy, text);
    }
    return copy;
}

static void heap_free_text(LPARAM lparam)
{
    if (lparam) {
        HeapFree(GetProcessHeap(), 0, (void *)lparam);
    }
}

static void post_text_message(HWND hwnd, UINT msg, const char *text)
{
    char *copy;

    copy = heap_dup_text(text);
    if (!copy) {
        return;
    }
    if (!PostMessage(hwnd, msg, 0, (LPARAM)copy)) {
        HeapFree(GetProcessHeap(), 0, copy);
    }
}

static int append_limited(char *dst, int dst_size, const char *src)
{
    int dst_len;
    int src_len;
    int room;

    if (!dst || dst_size <= 0) {
        return -1;
    }
    if (!src) {
        src = "";
    }

    dst_len = lstrlen(dst);
    if (dst_len >= dst_size - 1) {
        return -1;
    }
    src_len = lstrlen(src);
    room = dst_size - dst_len - 1;
    if (src_len > room) {
        src_len = room;
    }
    memcpy(dst + dst_len, src, src_len);
    dst[dst_len + src_len] = '\0';
    return src[src_len] ? -1 : 0;
}

static int ui_to_utf8(const char *input, char *out, int out_cap)
{
    WCHAR wide[AG_MAX_MESSAGE + 1];
    int n;

    if (!out || out_cap <= 0) {
        return -1;
    }
    if (!input) {
        input = "";
    }

    n = MultiByteToWideChar(CP_ACP, 0, input, -1, wide,
                            sizeof(wide) / sizeof(wide[0]));
    if (n <= 0) {
        lstrcpyn(out, input, out_cap);
        return lstrlen(out);
    }

    n = WideCharToMultiByte(CP_UTF8, 0, wide, -1, out, out_cap, NULL, NULL);
    if (n <= 0) {
        lstrcpyn(out, input, out_cap);
        return lstrlen(out);
    }
    return n - 1;
}

static int utf8_to_ui(const char *input, char *out, int out_cap)
{
    WCHAR wide[AG_MAX_MESSAGE + 1];
    int n;

    if (!out || out_cap <= 0) {
        return -1;
    }
    if (!input) {
        input = "";
    }

    n = MultiByteToWideChar(CP_UTF8, 0, input, -1, wide,
                            sizeof(wide) / sizeof(wide[0]));
    if (n <= 0) {
        lstrcpyn(out, input, out_cap);
        return lstrlen(out);
    }

    n = WideCharToMultiByte(CP_ACP, 0, wide, -1, out, out_cap, NULL, NULL);
    if (n <= 0) {
        lstrcpyn(out, input, out_cap);
        return lstrlen(out);
    }
    return n - 1;
}

static int socket_write_all(void *ctx, const char *data, int len)
{
    socket_transport *transport;
    int written;

    transport = (socket_transport *)ctx;
    written = 0;
    while (written < len) {
        int n;

        n = send(transport->socket, data + written, len - written, 0);
        if (n == SOCKET_ERROR || n == 0) {
            return -1;
        }
        written += n;
    }
    return 0;
}

static int socket_read_line(void *ctx, char *buf, int cap)
{
    socket_transport *transport;
    int i;

    transport = (socket_transport *)ctx;
    i = 0;
    while (i < cap - 1) {
        char ch;
        int n;

        n = recv(transport->socket, &ch, 1, 0);
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
    socket_transport *transport;
    int got;

    transport = (socket_transport *)ctx;
    got = 0;
    while (got < len) {
        int n;

        n = recv(transport->socket, buf + got, len - got, 0);
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

static void close_gateway_socket(void)
{
    SOCKET s;

    s = INVALID_SOCKET;
    if (g_socket_lock_ready) {
        EnterCriticalSection(&g_socket_lock);
    }
    if (g_gateway_socket != INVALID_SOCKET) {
        s = g_gateway_socket;
        g_gateway_socket = INVALID_SOCKET;
    }
    g_gateway_connected = 0;
    if (g_socket_lock_ready) {
        LeaveCriticalSection(&g_socket_lock);
    }
    if (s != INVALID_SOCKET) {
        closesocket(s);
    }
}

static int ensure_gateway_connection(char *error, int error_cap)
{
    SOCKET s;

    if (g_socket_lock_ready) {
        EnterCriticalSection(&g_socket_lock);
    }
    if (g_gateway_socket != INVALID_SOCKET) {
        if (g_socket_lock_ready) {
            LeaveCriticalSection(&g_socket_lock);
        }
        return 0;
    }
    if (g_socket_lock_ready) {
        LeaveCriticalSection(&g_socket_lock);
    }

    s = connect_tcp(GATEWAY_HOST, GATEWAY_PORT);
    if (s == INVALID_SOCKET) {
        wsprintf(error, "Could not connect to %s:%d (Winsock error %d).",
                 GATEWAY_HOST, GATEWAY_PORT, WSAGetLastError());
        return -1;
    }

    if (g_socket_lock_ready) {
        EnterCriticalSection(&g_socket_lock);
    }
    g_gateway_socket = s;
    g_gateway_connected = 1;
    if (g_socket_lock_ready) {
        LeaveCriticalSection(&g_socket_lock);
    }
    return 0;
}

static void set_request_in_flight(int in_flight)
{
    g_request_in_flight = in_flight;
    if (g_send) {
        EnableWindow(g_send, !in_flight);
        InvalidateRect(g_send, NULL, TRUE);
    }
    refresh_runtime_lists();
}

static void set_runtime_status(const char *text)
{
    if (!text || !text[0]) {
        text = "Ready";
    }
    lstrcpyn(g_status_bar_state, text, sizeof(g_status_bar_state));
    if (g_status) {
        SetWindowText(g_status, text);
        InvalidateRect(g_status, NULL, TRUE);
    }
    if (g_status_bar) {
        InvalidateRect(g_status_bar, NULL, TRUE);
    }
    refresh_runtime_lists();
}

static void edit_append_raw(HWND edit, const char *text)
{
    int len;

    if (!edit || !text) {
        return;
    }
    len = GetWindowTextLength(edit);
    SendMessage(edit, EM_SETSEL, len, len);
    SendMessage(edit, EM_REPLACESEL, FALSE, (LPARAM)text);
    SendMessage(edit, EM_SCROLLCARET, 0, 0);
}

static void edit_append_normalized(HWND edit, const char *text)
{
    char buf[512];
    int used;

    if (!text) {
        return;
    }
    used = 0;
    while (*text) {
        if (*text == '\r') {
            text++;
            continue;
        }
        if (*text == '\n') {
            if (used > 0) {
                buf[used] = '\0';
                edit_append_raw(edit, buf);
                used = 0;
            }
            edit_append_raw(edit, "\r\n");
            text++;
            continue;
        }
        buf[used++] = *text++;
        if (used >= (int)sizeof(buf) - 1) {
            buf[used] = '\0';
            edit_append_raw(edit, buf);
            used = 0;
        }
    }
    if (used > 0) {
        buf[used] = '\0';
        edit_append_raw(edit, buf);
    }
}

static void append_transcript_message_ui(const char *role, const char *text)
{
    if (GetWindowTextLength(g_transcript) > 0) {
        edit_append_raw(g_transcript, "\r\n\r\n");
    }
    edit_append_raw(g_transcript, role);
    edit_append_raw(g_transcript, ":\r\n");
    edit_append_normalized(g_transcript, text);
    g_message_count++;
    if (g_active_thread >= 0 && g_active_thread < g_thread_count) {
        lstrcpyn(g_threads[g_active_thread].age, "now",
                 sizeof(g_threads[g_active_thread].age));
    }
    save_active_conversation();
    populate_projects_list();
    refresh_runtime_lists();
}

static void append_transcript_message_utf8(const char *role, const char *utf8)
{
    char ui_text[(AG_MAX_MESSAGE * 2) + 1];

    utf8_to_ui(utf8, ui_text, sizeof(ui_text));
    append_transcript_message_ui(role, ui_text);
}

static void make_request_body(char *out, int out_cap, const char *message_utf8,
                              const char *permission_ui, const char *model_ui)
{
    char permission_utf8[128];
    char model_utf8[128];

    ui_to_utf8(permission_ui, permission_utf8, sizeof(permission_utf8));
    ui_to_utf8(model_ui, model_utf8, sizeof(model_utf8));

    out[0] = '\0';
    append_limited(out, out_cap, "XPAGENT-META permission: ");
    append_limited(out, out_cap, permission_utf8);
    append_limited(out, out_cap, "\nXPAGENT-META model: ");
    append_limited(out, out_cap, model_utf8);
    append_limited(out, out_cap, "\n\n");
    append_limited(out, out_cap, message_utf8);
}

static DWORD WINAPI agent_worker(LPVOID param)
{
    gui_request *request;
    socket_transport socket_ctx;
    ag_transport transport;
    ag_message reply;
    SOCKET s;
    char error[256];
    char status[80];
    DWORD elapsed_ms;

    request = (gui_request *)param;
    error[0] = '\0';

    if (!g_winsock_ready) {
        post_text_message(request->hwnd, WM_XPAGENT_ERROR,
                          "Winsock startup failed.");
        HeapFree(GetProcessHeap(), 0, request);
        return 0;
    }

    post_text_message(request->hwnd, WM_XPAGENT_STATUS,
                      "Connecting to gateway...");
    if (ensure_gateway_connection(error, sizeof(error)) != 0) {
        post_text_message(request->hwnd, WM_XPAGENT_ERROR, error);
        HeapFree(GetProcessHeap(), 0, request);
        return 0;
    }

    post_text_message(request->hwnd, WM_XPAGENT_STATUS, "Thinking...");

    if (g_socket_lock_ready) {
        EnterCriticalSection(&g_socket_lock);
    }
    s = g_gateway_socket;
    if (g_socket_lock_ready) {
        LeaveCriticalSection(&g_socket_lock);
    }

    socket_ctx.socket = s;
    transport.ctx = &socket_ctx;
    transport.write_all = socket_write_all;
    transport.read_line = socket_read_line;
    transport.read_exact = socket_read_exact;

    if (ag_exchange(&transport, request->id, request->body, &reply) != 0) {
        wsprintf(error, "Gateway exchange failed (Winsock error %d).",
                 WSAGetLastError());
        close_gateway_socket();
        post_text_message(request->hwnd, WM_XPAGENT_ERROR, error);
        HeapFree(GetProcessHeap(), 0, request);
        return 0;
    }

    if (lstrcmp(reply.type, "ERROR") == 0) {
        post_text_message(request->hwnd, WM_XPAGENT_ERROR, reply.body);
        HeapFree(GetProcessHeap(), 0, request);
        return 0;
    }

    post_text_message(request->hwnd, WM_XPAGENT_REPLY, reply.body);
    elapsed_ms = GetTickCount() - request->started_at;
    wsprintf(status, "Done in %lus", (elapsed_ms + 999) / 1000);
    post_text_message(request->hwnd, WM_XPAGENT_STATUS, status);
    HeapFree(GetProcessHeap(), 0, request);
    return 0;
}

static DWORD WINAPI reconnect_worker(LPVOID param)
{
    HWND hwnd;
    char error[256];

    hwnd = (HWND)param;
    error[0] = '\0';

    if (!g_winsock_ready) {
        post_text_message(hwnd, WM_XPAGENT_ERROR, "Winsock startup failed.");
        return 0;
    }

    post_text_message(hwnd, WM_XPAGENT_STATUS, "Reconnecting...");
    close_gateway_socket();
    if (ensure_gateway_connection(error, sizeof(error)) != 0) {
        post_text_message(hwnd, WM_XPAGENT_ERROR, error);
        return 0;
    }

    post_text_message(hwnd, WM_XPAGENT_STATUS, "Gateway connected");
    return 0;
}

static DWORD WINAPI projects_worker(LPVOID param)
{
    HWND hwnd;
    SOCKET s;
    socket_transport socket_ctx;
    ag_transport transport;
    ag_message reply;

    hwnd = (HWND)param;
    if (!g_winsock_ready) {
        post_text_message(hwnd, WM_XPAGENT_STATUS, "Projects unavailable");
        return 0;
    }

    s = connect_tcp(GATEWAY_HOST, GATEWAY_PORT);
    if (s == INVALID_SOCKET) {
        post_text_message(hwnd, WM_XPAGENT_STATUS, "Projects unavailable");
        return 0;
    }

    socket_ctx.socket = s;
    transport.ctx = &socket_ctx;
    transport.write_all = socket_write_all;
    transport.read_line = socket_read_line;
    transport.read_exact = socket_read_exact;

    if (ag_send(&transport, "PROJECTS", g_next_request_id++, "", 0) != 0 ||
        ag_recv(&transport, &reply) != 0 ||
        lstrcmp(reply.type, "PROJECTS") != 0) {
        closesocket(s);
        post_text_message(hwnd, WM_XPAGENT_STATUS, "Projects unavailable");
        return 0;
    }

    closesocket(s);
    post_text_message(hwnd, WM_XPAGENT_PROJECTS, reply.body);
    return 0;
}

static void handle_send(HWND hwnd)
{
    gui_request *request;
    HANDLE thread;
    DWORD thread_id;
    char *prompt_ui;
    char prompt_utf8[AG_MAX_MESSAGE + 1];
    char permission_ui[80];
    char model_ui[80];
    int len;

    if (g_request_in_flight) {
        return;
    }

    len = GetWindowTextLength(g_prompt);
    if (len <= 0) {
        return;
    }

    prompt_ui = (char *)HeapAlloc(GetProcessHeap(), 0, len + 1);
    if (!prompt_ui) {
        return;
    }
    GetWindowText(g_prompt, prompt_ui, len + 1);
    if (prompt_ui[0] == '\0') {
        HeapFree(GetProcessHeap(), 0, prompt_ui);
        return;
    }

    request = (gui_request *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                       sizeof(gui_request));
    if (!request) {
        HeapFree(GetProcessHeap(), 0, prompt_ui);
        return;
    }

    GetWindowText(g_access, permission_ui, sizeof(permission_ui));
    GetWindowText(g_model, model_ui, sizeof(model_ui));
    ui_to_utf8(prompt_ui, prompt_utf8, sizeof(prompt_utf8));

    request->hwnd = hwnd;
    request->id = g_next_request_id++;
    request->started_at = GetTickCount();
    make_request_body(request->body, sizeof(request->body), prompt_utf8,
                      permission_ui, model_ui);

    retitle_active_conversation_from_prompt(prompt_ui);
    append_transcript_message_ui("You", prompt_ui);
    SetWindowText(g_prompt, "");
    save_active_conversation();
    set_request_in_flight(1);
    set_runtime_status("Connecting...");

    thread = CreateThread(NULL, 0, agent_worker, request, 0, &thread_id);
    if (!thread) {
        append_transcript_message_ui("Error", "Could not start request worker.");
        set_runtime_status("Error");
        set_request_in_flight(0);
        HeapFree(GetProcessHeap(), 0, request);
    } else {
        CloseHandle(thread);
    }

    HeapFree(GetProcessHeap(), 0, prompt_ui);
}

static LRESULT CALLBACK prompt_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                    LPARAM lparam)
{
    if (msg == WM_KEYDOWN && wparam == VK_RETURN &&
        (GetKeyState(VK_CONTROL) & 0x8000)) {
        handle_send(GetParent(hwnd));
        return 0;
    }
    return CallWindowProc(g_prompt_proc, hwnd, msg, wparam, lparam);
}

static void set_font(HWND hwnd, HFONT font)
{
    SendMessage(hwnd, WM_SETFONT, (WPARAM)font, TRUE);
}

static HWND make_control(HWND parent, const char *class_name, const char *text,
                         DWORD style, DWORD ex_style, int id)
{
    HWND hwnd;

    hwnd = CreateWindowEx(ex_style, class_name, text,
                          WS_CHILD | WS_VISIBLE | style,
                          0, 0, 10, 10, parent, (HMENU)id, g_hinst, NULL);
    set_font(hwnd, g_font);
    return hwnd;
}

static HWND make_section_label(HWND parent, const char *text, int id)
{
    HWND hwnd;

    hwnd = make_control(parent, "STATIC", text, SS_OWNERDRAW, 0, id);
    set_font(hwnd, g_bold_font);
    return hwnd;
}

static void make_asset_path(char *out, const char *rel)
{
    char *slash;
    char *p;

    GetModuleFileName(NULL, out, MAX_PATH);
    slash = NULL;
    for (p = out; *p; p++) {
        if (*p == '\\' || *p == '/') {
            slash = p;
        }
    }
    if (slash) {
        slash[1] = 0;
    } else {
        out[0] = 0;
    }
    lstrcat(out, rel);
}

static HBITMAP load_bitmap_asset(const char *rel)
{
    char path[MAX_PATH];

    make_asset_path(path, rel);
    return (HBITMAP)LoadImage(NULL, path, IMAGE_BITMAP, 0, 0,
                              LR_LOADFROMFILE | LR_CREATEDIBSECTION);
}

static HICON load_icon_asset(const char *rel, int size)
{
    char path[MAX_PATH];

    make_asset_path(path, rel);
    return (HICON)LoadImage(NULL, path, IMAGE_ICON, size, size,
                            LR_LOADFROMFILE | LR_DEFAULTCOLOR);
}

static void load_assets(void)
{
    g_bmp_new_chat = load_bitmap_asset("assets\\icons\\bmp\\new-chat.bmp");
    g_bmp_search = load_bitmap_asset("assets\\icons\\bmp\\search.bmp");
    g_bmp_scheduled = load_bitmap_asset("assets\\icons\\bmp\\scheduled.bmp");
    g_bmp_plugins = load_bitmap_asset("assets\\icons\\bmp\\plugins.bmp");
    g_bmp_add = load_bitmap_asset("assets\\icons\\bmp\\add.bmp");
    g_bmp_mic = load_bitmap_asset("assets\\icons\\bmp\\mic.bmp");
    g_bmp_send = load_bitmap_asset("assets\\icons\\bmp\\send.bmp");
    g_bmp_file = load_bitmap_asset("assets\\icons\\bmp\\file.bmp");
    g_bmp_project_open = load_bitmap_asset("assets\\icons\\bmp\\project-open.bmp");
    g_bmp_project_closed = load_bitmap_asset("assets\\icons\\bmp\\project-closed.bmp");
    g_bmp_badge = load_bitmap_asset("assets\\icons\\bmp\\xpagent-badge.bmp");
    g_app_icon_big = load_icon_asset("assets\\icons\\ico\\xpagent.ico", 32);
    g_app_icon_small = load_icon_asset("assets\\icons\\ico\\xpagent.ico", 16);
    g_icon_new_chat = load_icon_asset("assets\\icons\\ico\\new-chat.ico", 16);
    g_icon_search = load_icon_asset("assets\\icons\\ico\\search.ico", 16);
    g_icon_scheduled = load_icon_asset("assets\\icons\\ico\\scheduled.ico", 16);
    g_icon_plugins = load_icon_asset("assets\\icons\\ico\\plugins.ico", 16);
    g_icon_add = load_icon_asset("assets\\icons\\ico\\add.ico", 16);
    g_icon_mic = load_icon_asset("assets\\icons\\ico\\mic.ico", 16);
    g_icon_send = load_icon_asset("assets\\icons\\ico\\send.ico", 16);
    g_icon_file = load_icon_asset("assets\\icons\\ico\\file.ico", 16);
    g_icon_project_open = load_icon_asset("assets\\icons\\ico\\project-open.ico", 16);
    g_icon_project_closed = load_icon_asset("assets\\icons\\ico\\project-closed.ico", 16);
    g_icon_badge = load_icon_asset("assets\\icons\\ico\\xpagent-badge.ico", 32);
}

static void delete_bitmap(HBITMAP bitmap)
{
    if (bitmap) {
        DeleteObject(bitmap);
    }
}

static void cleanup_assets(void)
{
    delete_bitmap(g_bmp_new_chat);
    delete_bitmap(g_bmp_search);
    delete_bitmap(g_bmp_scheduled);
    delete_bitmap(g_bmp_plugins);
    delete_bitmap(g_bmp_add);
    delete_bitmap(g_bmp_mic);
    delete_bitmap(g_bmp_send);
    delete_bitmap(g_bmp_file);
    delete_bitmap(g_bmp_project_open);
    delete_bitmap(g_bmp_project_closed);
    delete_bitmap(g_bmp_badge);
    if (g_app_icon_big) {
        DestroyIcon(g_app_icon_big);
    }
    if (g_app_icon_small) {
        DestroyIcon(g_app_icon_small);
    }
    if (g_icon_new_chat) {
        DestroyIcon(g_icon_new_chat);
    }
    if (g_icon_search) {
        DestroyIcon(g_icon_search);
    }
    if (g_icon_scheduled) {
        DestroyIcon(g_icon_scheduled);
    }
    if (g_icon_plugins) {
        DestroyIcon(g_icon_plugins);
    }
    if (g_icon_add) {
        DestroyIcon(g_icon_add);
    }
    if (g_icon_mic) {
        DestroyIcon(g_icon_mic);
    }
    if (g_icon_send) {
        DestroyIcon(g_icon_send);
    }
    if (g_icon_file) {
        DestroyIcon(g_icon_file);
    }
    if (g_icon_project_open) {
        DestroyIcon(g_icon_project_open);
    }
    if (g_icon_project_closed) {
        DestroyIcon(g_icon_project_closed);
    }
    if (g_icon_badge) {
        DestroyIcon(g_icon_badge);
    }
}

static int is_section_label(int id)
{
    return id == ID_PROJECTS_LABEL ||
           id == ID_THREAD_TITLE ||
           id == ID_CHANGES_LABEL ||
           id == ID_PROMPT_LABEL ||
           id == ID_ENV_LABEL ||
           id == ID_TASKS_LABEL;
}

static int is_owner_button(int id)
{
    return id == ID_NEW_CHAT ||
           id == ID_SEARCH ||
           id == ID_SCHEDULED ||
           id == ID_PLUGINS ||
           id == ID_ADD ||
           id == ID_MIC ||
           id == ID_SEND;
}

static int is_dropdown_button(int id)
{
    return id == ID_OPEN_IN ||
           id == ID_ACCESS ||
           id == ID_MODEL;
}

static int is_owner_list(int id)
{
    return id == ID_PROJECTS_LIST ||
           id == ID_CHANGES_LIST ||
           id == ID_ENV_LIST ||
           id == ID_TASKS_LIST;
}

static int blend_channel(int a, int b, int step, int steps)
{
    return a + ((b - a) * step) / steps;
}

static COLORREF blend_color(COLORREF a, COLORREF b, int step, int steps)
{
    int r;
    int g;
    int bl;

    if (steps <= 0) {
        return a;
    }

    r = blend_channel(GetRValue(a), GetRValue(b), step, steps);
    g = blend_channel(GetGValue(a), GetGValue(b), step, steps);
    bl = blend_channel(GetBValue(a), GetBValue(b), step, steps);
    return RGB(r, g, bl);
}

static void fill_gradient(HDC hdc, RECT *rc, COLORREF top, COLORREF bottom)
{
    RECT line;
    HBRUSH brush;
    int height;
    int y;

    height = rc->bottom - rc->top;
    if (height <= 1) {
        FillRect(hdc, rc, g_header_brush);
        return;
    }

    line.left = rc->left;
    line.right = rc->right;
    for (y = 0; y < height; y++) {
        line.top = rc->top + y;
        line.bottom = line.top + 1;
        brush = CreateSolidBrush(blend_color(top, bottom, y, height - 1));
        FillRect(hdc, &line, brush);
        DeleteObject(brush);
    }
}

static void draw_bitmap_icon(HDC hdc, HBITMAP bitmap, int x, int y,
                             int w, int h)
{
    BLENDFUNCTION blend;
    HDC mem_dc;
    HGDIOBJ old_bitmap;

    if (!bitmap) {
        return;
    }

    mem_dc = CreateCompatibleDC(hdc);
    if (!mem_dc) {
        return;
    }

    old_bitmap = SelectObject(mem_dc, bitmap);
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    AlphaBlend(hdc, x, y, w, h, mem_dc, 0, 0, w, h, blend);
    SelectObject(mem_dc, old_bitmap);
    DeleteDC(mem_dc);
}

static void draw_icon_handle(HDC hdc, HICON icon, int x, int y, int w, int h)
{
    if (icon) {
        DrawIconEx(hdc, x, y, icon, w, h, 0, NULL, DI_NORMAL);
    }
}

static HICON toolbar_icon_for_id(int id)
{
    switch (id) {
    case ID_NEW_CHAT:
        return g_icon_new_chat;
    case ID_SEARCH:
        return g_icon_search;
    case ID_SCHEDULED:
        return g_icon_scheduled;
    case ID_PLUGINS:
        return g_icon_plugins;
    case ID_ADD:
        return g_icon_add;
    case ID_MIC:
        return g_icon_mic;
    case ID_SEND:
        return g_icon_send;
    default:
        return NULL;
    }
}

static HBITMAP toolbar_bitmap_for_id(int id)
{
    switch (id) {
    case ID_NEW_CHAT:
        return g_bmp_new_chat;
    case ID_SEARCH:
        return g_bmp_search;
    case ID_SCHEDULED:
        return g_bmp_scheduled;
    case ID_PLUGINS:
        return g_bmp_plugins;
    case ID_ADD:
        return g_bmp_add;
    case ID_MIC:
        return g_bmp_mic;
    case ID_SEND:
        return g_bmp_send;
    default:
        return NULL;
    }
}

static void draw_checkbox_icon(HDC hdc, int x, int y, COLORREF color)
{
    HPEN pen;
    HPEN old_pen;
    HBRUSH old_brush;

    pen = CreatePen(PS_SOLID, 1, color);
    old_pen = (HPEN)SelectObject(hdc, pen);
    old_brush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, x, y, x + 12, y + 12);
    MoveToEx(hdc, x + 3, y + 6, NULL);
    LineTo(hdc, x + 5, y + 9);
    LineTo(hdc, x + 10, y + 3);
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

static void draw_project_box(HDC hdc, int x, int y, int expanded,
                             COLORREF color)
{
    HPEN pen;
    HPEN old_pen;
    HBRUSH old_brush;

    pen = CreatePen(PS_SOLID, 1, color);
    old_pen = (HPEN)SelectObject(hdc, pen);
    old_brush = (HBRUSH)SelectObject(hdc, GetStockObject(WHITE_BRUSH));
    Rectangle(hdc, x, y, x + 10, y + 10);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    MoveToEx(hdc, x + 2, y + 5, NULL);
    LineTo(hdc, x + 8, y + 5);
    if (!expanded) {
        MoveToEx(hdc, x + 5, y + 2, NULL);
        LineTo(hdc, x + 5, y + 8);
    }
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

static void draw_file_icon(HDC hdc, int x, int y, COLORREF color)
{
    HPEN pen;
    HPEN old_pen;
    HBRUSH old_brush;

    pen = CreatePen(PS_SOLID, 1, color);
    old_pen = (HPEN)SelectObject(hdc, pen);
    old_brush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, x + 2, y + 1, x + 11, y + 13);
    MoveToEx(hdc, x + 8, y + 1, NULL);
    LineTo(hdc, x + 11, y + 4);
    MoveToEx(hdc, x + 4, y + 6, NULL);
    LineTo(hdc, x + 9, y + 6);
    MoveToEx(hdc, x + 4, y + 9, NULL);
    LineTo(hdc, x + 9, y + 9);
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

static void copy_trimmed(char *dst, int dst_size, const char *src, int len)
{
    int i;

    while (len > 0 && src[len - 1] == ' ') {
        len--;
    }
    if (len >= dst_size) {
        len = dst_size - 1;
    }
    for (i = 0; i < len; i++) {
        dst[i] = src[i];
    }
    dst[len] = 0;
}

static void split_project_child(const char *text, char *label, int label_size,
                                char *age, int age_size)
{
    const char *start;
    int len;
    int end;
    int split;

    start = text;
    while (*start == ' ') {
        start++;
    }

    len = lstrlen(start);
    end = len - 1;
    while (end >= 0 && start[end] == ' ') {
        end--;
    }

    split = end;
    while (split >= 0 && start[split] != ' ') {
        split--;
    }

    if (split > 0) {
        copy_trimmed(label, label_size, start, split);
        copy_trimmed(age, age_size, start + split + 1, end - split);
    } else {
        copy_trimmed(label, label_size, start, end + 1);
        age[0] = 0;
    }
}

static void draw_toolbar_icon(HDC hdc, int id, RECT *rc, COLORREF color)
{
    HPEN pen;
    HPEN old_pen;
    HBRUSH brush;
    HBRUSH old_brush;
    int x;
    int y;
    HBITMAP bitmap;
    HICON icon;

    icon = toolbar_icon_for_id(id);
    if (icon) {
        draw_icon_handle(hdc, icon, rc->left, rc->top, 16, 16);
        return;
    }

    bitmap = toolbar_bitmap_for_id(id);
    if (bitmap) {
        draw_bitmap_icon(hdc, bitmap, rc->left, rc->top, 16, 16);
        return;
    }

    x = rc->left;
    y = rc->top;
    pen = CreatePen(PS_SOLID, 1, color);
    brush = CreateSolidBrush(color);
    old_pen = (HPEN)SelectObject(hdc, pen);
    old_brush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

    if (id == ID_NEW_CHAT) {
        Rectangle(hdc, x + 1, y + 1, x + 12, y + 14);
        MoveToEx(hdc, x + 4, y + 5, NULL);
        LineTo(hdc, x + 10, y + 5);
        MoveToEx(hdc, x + 7, y + 2, NULL);
        LineTo(hdc, x + 7, y + 8);
    } else if (id == ID_SEARCH) {
        Ellipse(hdc, x + 1, y + 1, x + 10, y + 10);
        MoveToEx(hdc, x + 8, y + 8, NULL);
        LineTo(hdc, x + 14, y + 14);
    } else if (id == ID_SCHEDULED) {
        Ellipse(hdc, x + 1, y + 1, x + 14, y + 14);
        MoveToEx(hdc, x + 7, y + 3, NULL);
        LineTo(hdc, x + 7, y + 7);
        LineTo(hdc, x + 11, y + 7);
    } else if (id == ID_PLUGINS) {
        Rectangle(hdc, x + 2, y + 4, x + 12, y + 12);
        SelectObject(hdc, brush);
        Rectangle(hdc, x + 5, y + 1, x + 9, y + 5);
        Rectangle(hdc, x + 5, y + 11, x + 9, y + 15);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
    } else if (id == ID_ADD) {
        MoveToEx(hdc, x + 7, y + 2, NULL);
        LineTo(hdc, x + 7, y + 13);
        MoveToEx(hdc, x + 2, y + 7, NULL);
        LineTo(hdc, x + 13, y + 7);
    } else if (id == ID_MIC) {
        RoundRect(hdc, x + 4, y + 1, x + 11, y + 10, 4, 4);
        MoveToEx(hdc, x + 3, y + 8, NULL);
        LineTo(hdc, x + 3, y + 11);
        LineTo(hdc, x + 12, y + 11);
        LineTo(hdc, x + 12, y + 8);
        MoveToEx(hdc, x + 7, y + 11, NULL);
        LineTo(hdc, x + 7, y + 15);
    } else if (id == ID_SEND) {
        SelectObject(hdc, brush);
        MoveToEx(hdc, x + 2, y + 2, NULL);
        LineTo(hdc, x + 14, y + 7);
        LineTo(hdc, x + 2, y + 12);
        LineTo(hdc, x + 5, y + 7);
        LineTo(hdc, x + 2, y + 2);
    }

    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(brush);
    DeleteObject(pen);
}

static void draw_section_label(DRAWITEMSTRUCT *item)
{
    char text[128];
    RECT rc;
    RECT icon_rc;
    HPEN top_pen;
    HPEN bottom_pen;
    HPEN old_pen;
    HGDIOBJ old_font;
    int old_mode;

    rc = item->rcItem;
    fill_gradient(item->hDC, &rc, RGB(255, 255, 255), RGB(224, 235, 252));

    top_pen = CreatePen(PS_SOLID, 1, RGB(122, 161, 220));
    bottom_pen = CreatePen(PS_SOLID, 1, RGB(180, 198, 225));
    old_pen = (HPEN)SelectObject(item->hDC, top_pen);
    MoveToEx(item->hDC, rc.left, rc.top, NULL);
    LineTo(item->hDC, rc.right, rc.top);
    SelectObject(item->hDC, bottom_pen);
    MoveToEx(item->hDC, rc.left, rc.bottom - 1, NULL);
    LineTo(item->hDC, rc.right, rc.bottom - 1);
    SelectObject(item->hDC, top_pen);
    MoveToEx(item->hDC, rc.left, rc.top, NULL);
    LineTo(item->hDC, rc.left, rc.bottom);
    SelectObject(item->hDC, old_pen);
    DeleteObject(top_pen);
    DeleteObject(bottom_pen);

    GetWindowText(item->hwndItem, text, sizeof(text));
    rc.left += 7;
    if (item->CtlID == ID_THREAD_TITLE) {
        icon_rc.left = rc.left;
        icon_rc.top = rc.top + 5;
        draw_checkbox_icon(item->hDC, icon_rc.left, icon_rc.top,
                           RGB(49, 106, 197));
        rc.left += 18;
    }
    old_font = SelectObject(item->hDC, g_bold_font);
    old_mode = SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, RGB(0, 51, 153));
    DrawText(item->hDC, text, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SetBkMode(item->hDC, old_mode);
    SelectObject(item->hDC, old_font);
}

static void draw_agent_badge(DRAWITEMSTRUCT *item)
{
    RECT rc;
    HPEN border;
    HPEN old_pen;
    HGDIOBJ old_font;
    int old_mode;

    rc = item->rcItem;
    if (g_icon_badge) {
        FillRect(item->hDC, &rc, g_bg_brush);
        draw_icon_handle(item->hDC, g_icon_badge,
                         rc.left + ((rc.right - rc.left) - 32) / 2,
                         rc.top + ((rc.bottom - rc.top) - 32) / 2,
                         32, 32);
        return;
    }

    if (g_bmp_badge) {
        FillRect(item->hDC, &rc, g_bg_brush);
        draw_bitmap_icon(item->hDC, g_bmp_badge,
                         rc.left + ((rc.right - rc.left) - 32) / 2,
                         rc.top + ((rc.bottom - rc.top) - 32) / 2,
                         32, 32);
        return;
    }

    fill_gradient(item->hDC, &rc, RGB(77, 144, 230), RGB(0, 62, 174));

    border = CreatePen(PS_SOLID, 1, RGB(0, 45, 130));
    old_pen = (HPEN)SelectObject(item->hDC, border);
    MoveToEx(item->hDC, rc.left, rc.top, NULL);
    LineTo(item->hDC, rc.right - 1, rc.top);
    LineTo(item->hDC, rc.right - 1, rc.bottom - 1);
    LineTo(item->hDC, rc.left, rc.bottom - 1);
    LineTo(item->hDC, rc.left, rc.top);
    SelectObject(item->hDC, old_pen);
    DeleteObject(border);

    old_font = SelectObject(item->hDC, g_bold_font);
    old_mode = SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, RGB(255, 255, 255));
    DrawText(item->hDC, "XP", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SetBkMode(item->hDC, old_mode);
    SelectObject(item->hDC, old_font);
}

static void draw_user_info(DRAWITEMSTRUCT *item)
{
    RECT rc;
    RECT left_rc;
    HGDIOBJ old_font;
    int old_mode;

    rc = item->rcItem;
    FillRect(item->hDC, &rc, g_bg_brush);

    old_mode = SetBkMode(item->hDC, TRANSPARENT);

    left_rc = rc;
    left_rc.top += 1;
    left_rc.bottom = left_rc.top + 13;
    old_font = SelectObject(item->hDC, g_bold_font);
    SetTextColor(item->hDC, RGB(0, 0, 0));
    DrawText(item->hDC, "Oscar Sims", -1, &left_rc,
             DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

    left_rc.top += 14;
    left_rc.bottom = left_rc.top + 13;
    SelectObject(item->hDC, g_font);
    DrawText(item->hDC, "Pro", -1, &left_rc,
             DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

    SetBkMode(item->hDC, old_mode);
    SelectObject(item->hDC, old_font);
}

static void draw_usage_info(DRAWITEMSTRUCT *item)
{
    RECT rc;
    RECT left_rc;
    RECT right_rc;
    HGDIOBJ old_font;
    int old_mode;

    rc = item->rcItem;
    FillRect(item->hDC, &rc, g_bg_brush);

    old_mode = SetBkMode(item->hDC, TRANSPARENT);
    SelectObject(item->hDC, g_small_font);
    SetTextColor(item->hDC, RGB(70, 70, 70));

    left_rc = rc;
    left_rc.top += 1;
    left_rc.bottom = left_rc.top + 12;
    right_rc = left_rc;
    right_rc.left = rc.left + 72;
    DrawText(item->hDC, "5h", -1, &left_rc, DT_LEFT | DT_SINGLELINE);
    DrawText(item->hDC, "8%  03:40", -1, &right_rc,
             DT_RIGHT | DT_SINGLELINE);

    left_rc.top += 13;
    left_rc.bottom = left_rc.top + 12;
    right_rc = left_rc;
    right_rc.left = rc.left + 58;
    DrawText(item->hDC, "Weekly", -1, &left_rc, DT_LEFT | DT_SINGLELINE);
    DrawText(item->hDC, "54%  7 Jul", -1, &right_rc,
             DT_RIGHT | DT_SINGLELINE);

    left_rc.top += 14;
    left_rc.bottom = left_rc.top + 14;
    SetTextColor(item->hDC, RGB(0, 51, 153));
    DrawText(item->hDC, "3 resets available", -1, &left_rc,
             DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

    SetBkMode(item->hDC, old_mode);
    SelectObject(item->hDC, old_font);
}

static void draw_dropdown_button(DRAWITEMSTRUCT *item)
{
    char text[80];
    RECT rc;
    RECT text_rc;
    RECT arrow_rc;
    POINT tri[3];
    HPEN border;
    HPEN sep;
    HPEN highlight;
    HPEN old_pen;
    HBRUSH arrow_brush;
    HBRUSH old_brush;
    HGDIOBJ old_font;
    int old_mode;
    int selected;

    selected = (item->itemState & ODS_SELECTED) != 0;
    rc = item->rcItem;

    fill_gradient(item->hDC, &rc,
                  selected ? RGB(211, 226, 249) : RGB(255, 255, 255),
                  selected ? RGB(188, 211, 244) : RGB(229, 239, 252));

    border = CreatePen(PS_SOLID, 1, RGB(120, 151, 205));
    highlight = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    old_pen = (HPEN)SelectObject(item->hDC, border);
    MoveToEx(item->hDC, rc.left, rc.top, NULL);
    LineTo(item->hDC, rc.right - 1, rc.top);
    LineTo(item->hDC, rc.right - 1, rc.bottom - 1);
    LineTo(item->hDC, rc.left, rc.bottom - 1);
    LineTo(item->hDC, rc.left, rc.top);
    SelectObject(item->hDC, highlight);
    MoveToEx(item->hDC, rc.left + 1, rc.top + 1, NULL);
    LineTo(item->hDC, rc.right - 2, rc.top + 1);
    MoveToEx(item->hDC, rc.left + 1, rc.top + 1, NULL);
    LineTo(item->hDC, rc.left + 1, rc.bottom - 2);

    arrow_rc = rc;
    arrow_rc.left = rc.right - 22;
    fill_gradient(item->hDC, &arrow_rc,
                  selected ? RGB(195, 216, 247) : RGB(242, 247, 255),
                  selected ? RGB(165, 196, 239) : RGB(206, 225, 250));
    sep = CreatePen(PS_SOLID, 1, RGB(155, 178, 215));
    SelectObject(item->hDC, sep);
    MoveToEx(item->hDC, arrow_rc.left, arrow_rc.top + 2, NULL);
    LineTo(item->hDC, arrow_rc.left, arrow_rc.bottom - 2);

    arrow_brush = CreateSolidBrush(RGB(36, 78, 160));
    old_brush = (HBRUSH)SelectObject(item->hDC, arrow_brush);
    SelectObject(item->hDC, GetStockObject(NULL_PEN));
    tri[0].x = arrow_rc.left + 7;
    tri[0].y = arrow_rc.top + ((arrow_rc.bottom - arrow_rc.top) / 2) - 2;
    tri[1].x = tri[0].x + 8;
    tri[1].y = tri[0].y;
    tri[2].x = tri[0].x + 4;
    tri[2].y = tri[0].y + 5;
    Polygon(item->hDC, tri, 3);

    SelectObject(item->hDC, old_brush);
    SelectObject(item->hDC, old_pen);
    DeleteObject(arrow_brush);
    DeleteObject(sep);
    DeleteObject(highlight);
    DeleteObject(border);

    GetWindowText(item->hwndItem, text, sizeof(text));
    text_rc = rc;
    text_rc.left += 9;
    text_rc.right = arrow_rc.left - 6;
    if (selected) {
        OffsetRect(&text_rc, 1, 1);
    }
    old_font = SelectObject(item->hDC, g_font);
    old_mode = SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, RGB(0, 0, 0));
    DrawText(item->hDC, text, -1, &text_rc,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SetBkMode(item->hDC, old_mode);
    SelectObject(item->hDC, old_font);

    if (item->itemState & ODS_FOCUS) {
        rc.left += 3;
        rc.top += 3;
        rc.right -= 3;
        rc.bottom -= 3;
        DrawFocusRect(item->hDC, &rc);
    }
}

static void draw_button(DRAWITEMSTRUCT *item)
{
    char text[80];
    RECT rc;
    RECT text_rc;
    HPEN border;
    HPEN highlight;
    HPEN old_pen;
    HGDIOBJ old_font;
    int old_mode;
    int selected;
    COLORREF fill_color;
    COLORREF fill_bottom;
    COLORREF border_color;
    COLORREF text_color;
    COLORREF icon_color;
    RECT icon_rc;
    int has_icon;

    selected = (item->itemState & ODS_SELECTED) != 0;
    rc = item->rcItem;
    text_rc = rc;
    text_rc.left += 4;
    text_rc.right -= 4;

    if (item->CtlID == ID_SEND) {
        fill_color = selected ? RGB(0, 70, 170) : RGB(80, 148, 238);
        fill_bottom = selected ? RGB(0, 52, 140) : RGB(34, 96, 200);
        border_color = RGB(0, 45, 130);
        text_color = RGB(255, 255, 255);
        icon_color = RGB(255, 255, 255);
    } else {
        fill_color = selected ? RGB(211, 226, 249) : RGB(255, 255, 255);
        fill_bottom = selected ? RGB(188, 211, 244) : RGB(229, 239, 252);
        border_color = RGB(142, 161, 191);
        text_color = RGB(0, 0, 0);
        icon_color = RGB(36, 78, 160);
    }

    fill_gradient(item->hDC, &rc, fill_color, fill_bottom);

    border = CreatePen(PS_SOLID, 1, border_color);
    old_pen = (HPEN)SelectObject(item->hDC, border);
    MoveToEx(item->hDC, rc.left, rc.top, NULL);
    LineTo(item->hDC, rc.right - 1, rc.top);
    LineTo(item->hDC, rc.right - 1, rc.bottom - 1);
    LineTo(item->hDC, rc.left, rc.bottom - 1);
    LineTo(item->hDC, rc.left, rc.top);
    highlight = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    SelectObject(item->hDC, highlight);
    MoveToEx(item->hDC, rc.left + 1, rc.top + 1, NULL);
    LineTo(item->hDC, rc.right - 2, rc.top + 1);
    MoveToEx(item->hDC, rc.left + 1, rc.top + 1, NULL);
    LineTo(item->hDC, rc.left + 1, rc.bottom - 2);
    SelectObject(item->hDC, old_pen);
    DeleteObject(border);
    DeleteObject(highlight);

    GetWindowText(item->hwndItem, text, sizeof(text));
    if (selected) {
        OffsetRect(&text_rc, 1, 1);
    }

    has_icon = item->CtlID == ID_NEW_CHAT ||
               item->CtlID == ID_SEARCH ||
               item->CtlID == ID_SCHEDULED ||
               item->CtlID == ID_PLUGINS ||
               item->CtlID == ID_ADD ||
               item->CtlID == ID_MIC ||
               item->CtlID == ID_SEND;
    if (has_icon) {
        icon_rc.left = rc.left + 8;
        icon_rc.top = rc.top + ((rc.bottom - rc.top) - 16) / 2;
        icon_rc.right = icon_rc.left + 16;
        icon_rc.bottom = icon_rc.top + 16;
        if (item->CtlID == ID_ADD) {
            icon_rc.left = rc.left + ((rc.right - rc.left) - 16) / 2;
        }
        if (item->CtlID == ID_MIC && text[0] == 0) {
            icon_rc.left = rc.left + ((rc.right - rc.left) - 16) / 2;
        }
        if (selected) {
            OffsetRect(&icon_rc, 1, 1);
        }
        draw_toolbar_icon(item->hDC, (int)item->CtlID, &icon_rc, icon_color);
        if (item->CtlID != ID_ADD && text[0] != 0) {
            text_rc.left += 17;
        }
    }

    old_font = SelectObject(item->hDC, g_font);
    old_mode = SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, text_color);
    if (item->CtlID != ID_ADD && text[0] != 0) {
        DrawText(item->hDC, text, -1, &text_rc,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    SetBkMode(item->hDC, old_mode);
    SelectObject(item->hDC, old_font);

    if (item->itemState & ODS_FOCUS) {
        rc.left += 3;
        rc.top += 3;
        rc.right -= 3;
        rc.bottom -= 3;
        DrawFocusRect(item->hDC, &rc);
    }
}

static void draw_status_line(DRAWITEMSTRUCT *item)
{
    RECT rc;
    RECT text_rc;
    RECT icon_rc;
    HPEN line_pen;
    HPEN old_pen;
    HGDIOBJ old_font;
    int old_mode;
    char text[128];

    rc = item->rcItem;
    fill_gradient(item->hDC, &rc, RGB(250, 252, 255), RGB(238, 244, 253));

    line_pen = CreatePen(PS_SOLID, 1, RGB(202, 215, 235));
    old_pen = (HPEN)SelectObject(item->hDC, line_pen);
    MoveToEx(item->hDC, rc.left, rc.bottom - 1, NULL);
    LineTo(item->hDC, rc.right, rc.bottom - 1);
    SelectObject(item->hDC, old_pen);
    DeleteObject(line_pen);

    icon_rc.left = rc.left + 7;
    icon_rc.top = rc.top + 5;
    icon_rc.right = icon_rc.left + 16;
    icon_rc.bottom = icon_rc.top + 16;
    draw_toolbar_icon(item->hDC, ID_SCHEDULED, &icon_rc, RGB(49, 106, 197));

    GetWindowText(item->hwndItem, text, sizeof(text));
    text_rc = rc;
    text_rc.left += 27;
    text_rc.right -= 8;
    old_font = SelectObject(item->hDC, g_font);
    old_mode = SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, RGB(64, 64, 64));
    DrawText(item->hDC, text, -1, &text_rc,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SetBkMode(item->hDC, old_mode);
    SelectObject(item->hDC, old_font);
}

static void draw_toolstrip(DRAWITEMSTRUCT *item)
{
    RECT rc;
    HPEN line_pen;
    HPEN old_pen;

    rc = item->rcItem;
    fill_gradient(item->hDC, &rc, RGB(255, 255, 255), RGB(220, 232, 250));

    line_pen = CreatePen(PS_SOLID, 1, RGB(150, 175, 215));
    old_pen = (HPEN)SelectObject(item->hDC, line_pen);
    MoveToEx(item->hDC, rc.left, rc.bottom - 1, NULL);
    LineTo(item->hDC, rc.right, rc.bottom - 1);
    SelectObject(item->hDC, old_pen);
    DeleteObject(line_pen);
}

static void draw_status_segment(HDC hdc, RECT *bar_rc, int left, int right,
                                const char *text)
{
    RECT seg_rc;
    RECT text_rc;

    seg_rc = *bar_rc;
    seg_rc.left = bar_rc->left + left;
    seg_rc.right = bar_rc->left + right;
    if (seg_rc.right > bar_rc->right) {
        seg_rc.right = bar_rc->right;
    }

    if (seg_rc.right < bar_rc->right) {
        DrawEdge(hdc, &seg_rc, EDGE_ETCHED, BF_RIGHT);
    }

    text_rc = seg_rc;
    text_rc.left += 7;
    text_rc.right -= 6;
    DrawText(hdc, text, -1, &text_rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

static void draw_status_bar(DRAWITEMSTRUCT *item)
{
    RECT rc;
    HPEN line_pen;
    HPEN old_pen;
    HGDIOBJ old_font;
    int old_mode;

    rc = item->rcItem;
    fill_gradient(item->hDC, &rc, RGB(255, 255, 255), RGB(224, 235, 252));

    line_pen = CreatePen(PS_SOLID, 1, RGB(165, 184, 215));
    old_pen = (HPEN)SelectObject(item->hDC, line_pen);
    MoveToEx(item->hDC, rc.left, rc.top, NULL);
    LineTo(item->hDC, rc.right, rc.top);
    SelectObject(item->hDC, old_pen);
    DeleteObject(line_pen);

    old_font = SelectObject(item->hDC, g_font);
    old_mode = SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, RGB(64, 64, 64));
    draw_status_segment(item->hDC, &rc, 0, 150, g_status_bar_state);
    draw_status_segment(item->hDC, &rc, 150, 274, "xpilot connected");
    draw_status_segment(item->hDC, &rc, 274, rc.right,
                        g_gateway_connected ? "gateway connected" :
                        "gateway 10.0.2.2:7790");
    SetBkMode(item->hDC, old_mode);
    SelectObject(item->hDC, old_font);
}

static void draw_owner_list(DRAWITEMSTRUCT *item)
{
    char text[256];
    char label[160];
    char age[32];
    RECT rc;
    RECT text_rc;
    RECT age_rc;
    HGDIOBJ old_font;
    int old_mode;
    int selected;
    int expanded;
    COLORREF text_color;
    COLORREF icon_color;

    if (item->itemID == (UINT)-1) {
        return;
    }

    text[0] = 0;
    SendMessage(item->hwndItem, LB_GETTEXT, item->itemID, (LPARAM)text);

    rc = item->rcItem;
    selected = (item->itemState & ODS_SELECTED) != 0;
    if (selected) {
        fill_gradient(item->hDC, &rc, RGB(91, 147, 226), RGB(49, 106, 197));
        text_color = RGB(255, 255, 255);
        icon_color = RGB(255, 255, 255);
    } else {
        FillRect(item->hDC, &rc, g_white_brush);
        text_color = RGB(0, 0, 0);
        icon_color = RGB(49, 106, 197);
    }

    old_font = SelectObject(item->hDC, g_font);
    old_mode = SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, text_color);

    text_rc = rc;
    text_rc.left += 4;
    text_rc.right -= 4;

    if (item->CtlID == ID_PROJECTS_LIST) {
        if (text[0] == '[' && text[2] == ']') {
            expanded = text[1] == '-';
            if (expanded && g_icon_project_open) {
                draw_icon_handle(item->hDC, g_icon_project_open,
                                 rc.left + 3, rc.top, 16, 16);
            } else if (!expanded && g_icon_project_closed) {
                draw_icon_handle(item->hDC, g_icon_project_closed,
                                 rc.left + 3, rc.top, 16, 16);
            } else if (expanded && g_bmp_project_open) {
                draw_bitmap_icon(item->hDC, g_bmp_project_open,
                                 rc.left + 3, rc.top, 16, 16);
            } else if (!expanded && g_bmp_project_closed) {
                draw_bitmap_icon(item->hDC, g_bmp_project_closed,
                                 rc.left + 3, rc.top, 16, 16);
            } else {
                draw_project_box(item->hDC, rc.left + 4, rc.top + 3, expanded,
                                 icon_color);
            }
            text_rc.left = rc.left + 22;
            DrawText(item->hDC, text + 4, -1, &text_rc,
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        } else if (text[0] == ' ') {
            split_project_child(text, label, sizeof(label), age, sizeof(age));
            if (g_icon_file) {
                draw_icon_handle(item->hDC, g_icon_file, rc.left + 18, rc.top,
                                 16, 16);
            } else if (g_bmp_file) {
                draw_bitmap_icon(item->hDC, g_bmp_file, rc.left + 18, rc.top,
                                 16, 16);
            } else {
                draw_file_icon(item->hDC, rc.left + 18, rc.top + 2,
                               icon_color);
            }
            text_rc.left = rc.left + 34;
            age_rc = rc;
            age_rc.left = rc.right - 34;
            age_rc.right = rc.right - 6;
            text_rc.right = age_rc.left - 4;
            DrawText(item->hDC, label, -1, &text_rc,
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            if (age[0]) {
                DrawText(item->hDC, age, -1, &age_rc,
                         DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            }
        } else {
            DrawText(item->hDC, text, -1, &text_rc,
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
    } else {
        DrawText(item->hDC, text, -1, &text_rc,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    SetBkMode(item->hDC, old_mode);
    SelectObject(item->hDC, old_font);

    if (item->itemState & ODS_FOCUS) {
        DrawFocusRect(item->hDC, &rc);
    }
}

static void add_list_item(HWND list, const char *text)
{
    SendMessage(list, LB_ADDSTRING, 0, (LPARAM)text);
}

static void clear_list(HWND list)
{
    if (list) {
        SendMessage(list, LB_RESETCONTENT, 0, 0);
    }
}

static LPARAM encode_project_row(int type, int index)
{
    return (LPARAM)(((type & 0xff) << 16) | (index & 0xffff));
}

static int row_type(LPARAM data)
{
    return ((int)data >> 16) & 0xff;
}

static int row_index(LPARAM data)
{
    return (int)data & 0xffff;
}

static int add_project_row(const char *text, int type, int index)
{
    int row;

    row = (int)SendMessage(g_projects_list, LB_ADDSTRING, 0, (LPARAM)text);
    if (row >= 0) {
        SendMessage(g_projects_list, LB_SETITEMDATA, row,
                    encode_project_row(type, index));
    }
    return row;
}

static void set_default_conversation_text(char *out, int out_cap)
{
    lstrcpyn(out, DEFAULT_TRANSCRIPT, out_cap);
}

static int find_project_index(const char *name)
{
    int i;

    for (i = 0; i < g_project_count; i++) {
        if (lstrcmpi(g_projects[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int add_project_model(const char *name, int expanded)
{
    int index;

    index = find_project_index(name);
    if (index >= 0) {
        g_projects[index].expanded = expanded || g_projects[index].expanded;
        return index;
    }
    if (g_project_count >= MAX_PROJECTS) {
        return -1;
    }
    index = g_project_count++;
    lstrcpyn(g_projects[index].name, name, sizeof(g_projects[index].name));
    g_projects[index].expanded = expanded;
    return index;
}

static int find_thread_index(int project_index, const char *title)
{
    int i;

    for (i = 0; i < g_thread_count; i++) {
        if (g_threads[i].project_index == project_index &&
            lstrcmpi(g_threads[i].title, title) == 0) {
            return i;
        }
    }
    return -1;
}

static int add_thread_model(int project_index, const char *title,
                            const char *age)
{
    int index;

    if (project_index < 0 || project_index >= g_project_count) {
        return -1;
    }
    index = find_thread_index(project_index, title);
    if (index >= 0) {
        if (age && age[0]) {
            lstrcpyn(g_threads[index].age, age, sizeof(g_threads[index].age));
        }
        return index;
    }
    if (g_thread_count >= MAX_THREADS) {
        return -1;
    }
    index = g_thread_count++;
    ZeroMemory(&g_threads[index], sizeof(g_threads[index]));
    g_threads[index].project_index = project_index;
    lstrcpyn(g_threads[index].title, title, sizeof(g_threads[index].title));
    lstrcpyn(g_threads[index].age, age && age[0] ? age : "now",
             sizeof(g_threads[index].age));
    set_default_conversation_text(g_threads[index].transcript,
                                  sizeof(g_threads[index].transcript));
    return index;
}

static void make_thread_row_text(int thread_index, char *out, int out_cap)
{
    char text[220];

    wsprintf(text, "    %s        %s", g_threads[thread_index].title,
             g_threads[thread_index].age);
    lstrcpyn(out, text, out_cap);
}

static void populate_projects_list(void)
{
    int i;
    int j;
    int selected_row;
    char text[256];

    if (!g_projects_list) {
        return;
    }

    clear_list(g_projects_list);
    selected_row = -1;
    for (i = 0; i < g_project_count; i++) {
        wsprintf(text, "%s %s", g_projects[i].expanded ? "[-]" : "[+]",
                 g_projects[i].name);
        add_project_row(text, PROJECT_ROW_PROJECT, i);
        if (!g_projects[i].expanded) {
            continue;
        }
        for (j = 0; j < g_thread_count; j++) {
            if (g_threads[j].project_index != i) {
                continue;
            }
            make_thread_row_text(j, text, sizeof(text));
            if (add_project_row(text, PROJECT_ROW_THREAD, j) >= 0 &&
                j == g_active_thread) {
                selected_row = (int)SendMessage(g_projects_list, LB_GETCOUNT,
                                                0, 0) - 1;
            }
        }
    }
    if (selected_row >= 0) {
        SendMessage(g_projects_list, LB_SETCURSEL, selected_row, 0);
    }
}

static void save_active_conversation(void)
{
    conversation_model *thread;

    if (g_active_thread < 0 || g_active_thread >= g_thread_count) {
        return;
    }
    thread = &g_threads[g_active_thread];
    if (g_transcript) {
        GetWindowText(g_transcript, thread->transcript,
                      sizeof(thread->transcript));
    }
    if (g_prompt) {
        GetWindowText(g_prompt, thread->prompt, sizeof(thread->prompt));
    }
    thread->message_count = g_message_count;
}

static void load_conversation(HWND hwnd, int thread_index)
{
    conversation_model *thread;
    char title[180];

    if (thread_index < 0 || thread_index >= g_thread_count) {
        return;
    }

    thread = &g_threads[thread_index];
    g_active_thread = thread_index;
    g_active_project = thread->project_index;
    if (g_active_project >= 0 && g_active_project < g_project_count) {
        g_projects[g_active_project].expanded = 1;
    }
    g_message_count = thread->message_count;
    SetWindowText(g_transcript, thread->transcript);
    SetWindowText(g_prompt, thread->prompt);
    SetWindowText(g_thread_title, thread->title);
    wsprintf(title, "XPAgent - %s", thread->title);
    SetWindowText(hwnd, title);
    populate_projects_list();
    set_runtime_status("Ready");
    refresh_runtime_lists();
}

static void retitle_active_conversation_from_prompt(const char *prompt)
{
    conversation_model *thread;
    int len;
    int i;

    if (g_active_thread < 0 || g_active_thread >= g_thread_count || !prompt ||
        !prompt[0]) {
        return;
    }
    thread = &g_threads[g_active_thread];
    if (lstrcmp(thread->title, "New XPAgent chat") != 0 &&
        strncmp(thread->title, "New XPAgent chat ", 17) != 0) {
        return;
    }

    len = lstrlen(prompt);
    if (len > 52) {
        len = 52;
    }
    for (i = 0; i < len; i++) {
        if (prompt[i] == '\r' || prompt[i] == '\n') {
            break;
        }
        thread->title[i] = prompt[i];
    }
    thread->title[i] = '\0';
    if (thread->title[0] == '\0') {
        lstrcpyn(thread->title, "New XPAgent chat", sizeof(thread->title));
    }
    SetWindowText(g_thread_title, thread->title);
    if (g_thread_title) {
        char title[180];
        HWND parent;

        parent = GetParent(g_thread_title);
        wsprintf(title, "XPAgent - %s", thread->title);
        if (parent) {
            SetWindowText(parent, title);
        }
    }
    populate_projects_list();
}

static void init_project_model(HWND hwnd)
{
    int project;
    int thread;

    g_project_count = 0;
    g_thread_count = 0;
    g_active_project = -1;
    g_active_thread = -1;

    project = add_project_model("winxp", 1);
    thread = add_thread_model(project, "Set up Windows XP ISO", "2m");
    add_project_model("win2k", 0);
    project = add_project_model("powerstreet", 1);
    add_thread_model(project, "Fix road junction markings", "1h");
    add_thread_model(project, "Update project dependencies", "2d");
    add_thread_model(project, "Refactor road modules", "2d");
    add_project_model("firefox-ios", 0);
    add_project_model("law-agent", 0);
    add_project_model("tycoon", 0);

    if (thread >= 0) {
        load_conversation(hwnd, thread);
    } else {
        populate_projects_list();
    }
}

static char *next_project_field(char **cursor)
{
    char *start;
    char *tab;

    start = *cursor;
    tab = strchr(start, '\t');
    if (tab) {
        *tab = '\0';
        *cursor = tab + 1;
    } else {
        *cursor = start + lstrlen(start);
    }
    return start;
}

static void apply_project_metadata(HWND hwnd, char *body)
{
    char *line;
    char *next;
    int loaded_active;

    loaded_active = g_active_thread >= 0;
    line = body;
    while (line && *line) {
        char *cursor;
        char *kind;
        char *project_name;
        char *title;
        char *age;
        int project_index;

        next = strchr(line, '\n');
        if (next) {
            *next = '\0';
            next++;
        }
        if (line[0]) {
            int n;

            n = lstrlen(line);
            while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == ' ')) {
                line[n - 1] = '\0';
                n--;
            }
        }

        cursor = line;
        kind = next_project_field(&cursor);
        if (lstrcmp(kind, "PROJECT") == 0) {
            project_name = next_project_field(&cursor);
            add_project_model(project_name, cursor[0] == '1');
        } else if (lstrcmp(kind, "THREAD") == 0) {
            project_name = next_project_field(&cursor);
            title = next_project_field(&cursor);
            age = cursor;
            project_index = add_project_model(project_name, 1);
            if (project_index >= 0) {
                add_thread_model(project_index, title, age);
            }
        }

        line = next;
    }

    if (!loaded_active && g_thread_count > 0) {
        load_conversation(hwnd, 0);
    } else {
        populate_projects_list();
    }
    refresh_runtime_lists();
}

static void handle_project_selection(HWND hwnd)
{
    int row;
    LPARAM data;
    int type;
    int index;

    row = (int)SendMessage(g_projects_list, LB_GETCURSEL, 0, 0);
    if (row < 0) {
        return;
    }
    data = SendMessage(g_projects_list, LB_GETITEMDATA, row, 0);
    type = row_type(data);
    index = row_index(data);

    if (g_request_in_flight) {
        populate_projects_list();
        set_runtime_status("Busy");
        return;
    }

    if (type == PROJECT_ROW_PROJECT && index >= 0 && index < g_project_count) {
        save_active_conversation();
        g_projects[index].expanded = !g_projects[index].expanded;
        g_active_project = index;
        populate_projects_list();
        return;
    }

    if (type == PROJECT_ROW_THREAD && index >= 0 && index < g_thread_count) {
        if (index != g_active_thread) {
            save_active_conversation();
            load_conversation(hwnd, index);
        }
    }
}

static void request_project_metadata(HWND hwnd)
{
    HANDLE thread;
    DWORD thread_id;

    thread = CreateThread(NULL, 0, projects_worker, hwnd, 0, &thread_id);
    if (thread) {
        CloseHandle(thread);
    }
}

static void refresh_runtime_lists(void)
{
    char text[256];
    char cwd[MAX_PATH];
    char permission[80];
    char model[80];

    if (!g_env_list || !g_tasks_list || !g_changes_list) {
        return;
    }

    cwd[0] = '\0';
    GetCurrentDirectory(sizeof(cwd), cwd);
    GetWindowText(g_access, permission, sizeof(permission));
    GetWindowText(g_model, model, sizeof(model));

    clear_list(g_env_list);
    add_list_item(g_env_list,
                  g_gateway_connected ? "Gateway connected" :
                  "Gateway disconnected");
    add_list_item(g_env_list, "xpilot connected via host");
    if (g_active_project >= 0 && g_active_project < g_project_count) {
        wsprintf(text, "Project %s", g_projects[g_active_project].name);
        add_list_item(g_env_list, text);
    }
    if (g_active_thread >= 0 && g_active_thread < g_thread_count) {
        wsprintf(text, "Thread %s", g_threads[g_active_thread].title);
        add_list_item(g_env_list, text);
    }
    wsprintf(text, "CWD %s", cwd[0] ? cwd : "(unknown)");
    add_list_item(g_env_list, text);
    wsprintf(text, "Permission %s", permission);
    add_list_item(g_env_list, text);
    wsprintf(text, "Model %s", model);
    add_list_item(g_env_list, text);
    wsprintf(text, "Messages %d", g_message_count);
    add_list_item(g_env_list, text);

    clear_list(g_tasks_list);
    add_list_item(g_tasks_list,
                  g_request_in_flight ? "Waiting for gateway reply" : "Idle");
    add_list_item(g_tasks_list,
                  g_gateway_connected ? "AG1 socket open" :
                  "AG1 socket will connect on send");

    clear_list(g_changes_list);
    add_list_item(g_changes_list, "No edited files yet");
    SetWindowText(g_changes_label, "Edited files (0)");
    InvalidateRect(g_changes_label, NULL, TRUE);
}

static void reset_conversation(HWND hwnd)
{
    int project;
    int thread;
    char title[128];

    if (g_request_in_flight) {
        set_runtime_status("Busy");
        return;
    }

    save_active_conversation();
    close_gateway_socket();
    g_next_request_id = 1;
    g_gateway_connected = 0;

    project = g_active_project;
    if (project < 0 || project >= g_project_count) {
        project = 0;
    }
    if (project < 0 || project >= g_project_count) {
        project = add_project_model("winxp", 1);
    }

    if (g_next_new_chat == 1) {
        lstrcpyn(title, "New XPAgent chat", sizeof(title));
    } else {
        wsprintf(title, "New XPAgent chat %d", g_next_new_chat);
    }
    g_next_new_chat++;

    thread = add_thread_model(project, title, "now");
    if (thread >= 0) {
        g_threads[thread].message_count = 0;
        g_threads[thread].prompt[0] = '\0';
        set_default_conversation_text(g_threads[thread].transcript,
                                      sizeof(g_threads[thread].transcript));
        load_conversation(hwnd, thread);
    }
}

static void handle_reconnect(HWND hwnd)
{
    HANDLE thread;
    DWORD thread_id;

    if (g_request_in_flight) {
        set_runtime_status("Busy");
        return;
    }

    thread = CreateThread(NULL, 0, reconnect_worker, hwnd, 0, &thread_id);
    if (!thread) {
        append_transcript_message_ui("Error", "Could not start reconnect worker.");
        set_runtime_status("Error");
    } else {
        CloseHandle(thread);
    }
}

static void copy_focused_control(void)
{
    HWND focus;

    focus = GetFocus();
    if (focus) {
        SendMessage(focus, WM_COPY, 0, 0);
    }
}

static void select_all_focused_control(void)
{
    HWND focus;
    char class_name[32];

    focus = GetFocus();
    if (!focus) {
        return;
    }

    class_name[0] = '\0';
    GetClassName(focus, class_name, sizeof(class_name));
    if (lstrcmpi(class_name, "Edit") == 0) {
        SendMessage(focus, EM_SETSEL, 0, -1);
    }
}

static void show_choice_menu(HWND hwnd, HWND control, const char **items,
                             int count)
{
    HMENU menu;
    RECT rc;
    int i;
    int command;

    menu = CreatePopupMenu();
    if (!menu) {
        return;
    }

    for (i = 0; i < count; i++) {
        AppendMenu(menu, MF_STRING, 4000 + i, items[i]);
    }

    GetWindowRect(control, &rc);
    command = TrackPopupMenu(menu,
                             TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
                             rc.left, rc.bottom, 0, hwnd, NULL);
    if (command >= 4000 && command < 4000 + count) {
        SetWindowText(control, items[command - 4000]);
        InvalidateRect(control, NULL, TRUE);
        refresh_runtime_lists();
    }
    DestroyMenu(menu);
}

static void setup_fonts(void)
{
    LOGFONT lf;

    ZeroMemory(&lf, sizeof(lf));
    lstrcpy(lf.lfFaceName, "Tahoma");
    lf.lfHeight = -11;
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    g_font = CreateFontIndirect(&lf);

    if (!g_font) {
        g_font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        ZeroMemory(&lf, sizeof(lf));
        GetObject(g_font, sizeof(lf), &lf);
    }

    lf.lfWeight = FW_BOLD;
    g_bold_font = CreateFontIndirect(&lf);

    ZeroMemory(&lf, sizeof(lf));
    lstrcpy(lf.lfFaceName, "Courier New");
    lf.lfHeight = -13;
    lf.lfWeight = FW_NORMAL;
    g_mono_font = CreateFontIndirect(&lf);

    ZeroMemory(&lf, sizeof(lf));
    lstrcpy(lf.lfFaceName, "Tahoma");
    lf.lfHeight = -10;
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    g_small_font = CreateFontIndirect(&lf);
}

static void setup_brushes(void)
{
    g_bg_brush = CreateSolidBrush(RGB(239, 243, 250));
    g_header_brush = CreateSolidBrush(RGB(224, 235, 252));
    g_white_brush = CreateSolidBrush(RGB(255, 255, 255));
}

static void setup_menu(HWND hwnd)
{
    HMENU menu;
    HMENU file_menu;
    HMENU edit_menu;
    HMENU view_menu;
    HMENU tools_menu;
    HMENU help_menu;

    menu = CreateMenu();
    file_menu = CreatePopupMenu();
    edit_menu = CreatePopupMenu();
    view_menu = CreatePopupMenu();
    tools_menu = CreatePopupMenu();
    help_menu = CreatePopupMenu();

    AppendMenu(file_menu, MF_STRING, 2001, "New Chat");
    AppendMenu(file_menu, MF_SEPARATOR, 0, NULL);
    AppendMenu(file_menu, MF_STRING, 2002, "Exit");
    AppendMenu(edit_menu, MF_STRING, 2003, "Copy");
    AppendMenu(edit_menu, MF_STRING, 2004, "Select All");
    AppendMenu(view_menu, MF_STRING, 2005, "Refresh");
    AppendMenu(tools_menu, MF_STRING, 2006, "Reconnect");
    AppendMenu(help_menu, MF_STRING, 2007, "About");

    AppendMenu(menu, MF_POPUP, (UINT_PTR)file_menu, "File");
    AppendMenu(menu, MF_POPUP, (UINT_PTR)edit_menu, "Edit");
    AppendMenu(menu, MF_POPUP, (UINT_PTR)view_menu, "View");
    AppendMenu(menu, MF_POPUP, (UINT_PTR)tools_menu, "Tools");
    AppendMenu(menu, MF_POPUP, (UINT_PTR)help_menu, "Help");

    SetMenu(hwnd, menu);
}

static void fill_initial_content(HWND hwnd)
{
    init_project_model(hwnd);
}

static void create_children(HWND hwnd)
{
    DWORD list_style;
    DWORD edit_style;

    list_style = LBS_NOTIFY | WS_VSCROLL | LBS_OWNERDRAWFIXED |
                 LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT;
    edit_style = ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL;

    g_new_chat = make_control(hwnd, "BUTTON", "New Chat", BS_OWNERDRAW, 0,
                              ID_NEW_CHAT);
    g_search = make_control(hwnd, "BUTTON", "Search", BS_OWNERDRAW, 0,
                            ID_SEARCH);
    g_scheduled = make_control(hwnd, "BUTTON", "Scheduled", BS_OWNERDRAW, 0,
                               ID_SCHEDULED);
    g_plugins = make_control(hwnd, "BUTTON", "Plugins", BS_OWNERDRAW, 0,
                             ID_PLUGINS);
    g_open_in = make_control(hwnd, "BUTTON", "Open in...", BS_OWNERDRAW, 0,
                             ID_OPEN_IN);
    g_toolstrip = make_control(hwnd, "STATIC", "", SS_OWNERDRAW, 0,
                               ID_TOOLSTRIP);

    g_projects_label = make_section_label(hwnd, "Projects", ID_PROJECTS_LABEL);
    g_projects_list = make_control(hwnd, "LISTBOX", "", list_style,
                                   WS_EX_CLIENTEDGE,
                                   ID_PROJECTS_LIST);
    g_user_badge = make_control(hwnd, "STATIC", "XP", SS_OWNERDRAW, 0,
                                ID_USER_BADGE);
    g_user_text = make_control(hwnd, "STATIC", "", SS_OWNERDRAW, 0,
                               ID_USER_TEXT);
    g_usage_text = make_control(hwnd, "STATIC", "", SS_OWNERDRAW, 0,
                                ID_USAGE_TEXT);

    g_thread_title = make_section_label(hwnd, "Set up Windows XP ISO",
                                        ID_THREAD_TITLE);
    g_status = make_control(hwnd, "STATIC", "Worked for 2m 3s",
                            SS_OWNERDRAW, 0, ID_STATUS);
    g_transcript = make_control(hwnd, "EDIT", "", edit_style | ES_READONLY,
                                WS_EX_CLIENTEDGE, ID_TRANSCRIPT);
    g_changes_label = make_section_label(hwnd, "Edited files (3)",
                                         ID_CHANGES_LABEL);
    g_changes_list = make_control(hwnd, "LISTBOX", "", list_style,
                                  WS_EX_CLIENTEDGE,
                                  ID_CHANGES_LIST);
    g_prompt_label = make_section_label(hwnd, "Follow-up",
                                        ID_PROMPT_LABEL);
    g_prompt = make_control(hwnd, "EDIT", "",
                            ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL |
                                0,
                            WS_EX_CLIENTEDGE, ID_PROMPT);
    g_add = make_control(hwnd, "BUTTON", "+", BS_OWNERDRAW, 0, ID_ADD);
    g_access = make_control(hwnd, "BUTTON", "Full access", BS_OWNERDRAW, 0,
                            ID_ACCESS);
    g_model = make_control(hwnd, "BUTTON", "5.5 Extra High", BS_OWNERDRAW, 0,
                           ID_MODEL);
    g_mic = make_control(hwnd, "BUTTON", "", BS_OWNERDRAW, 0, ID_MIC);
    g_send = make_control(hwnd, "BUTTON", "Send", BS_OWNERDRAW, 0, ID_SEND);

    g_env_label = make_section_label(hwnd, "Environment", ID_ENV_LABEL);
    g_env_list = make_control(hwnd, "LISTBOX", "", list_style,
                              WS_EX_CLIENTEDGE, ID_ENV_LIST);
    g_tasks_label = make_section_label(hwnd, "Tasks", ID_TASKS_LABEL);
    g_tasks_list = make_control(hwnd, "LISTBOX", "", list_style,
                                WS_EX_CLIENTEDGE,
                                ID_TASKS_LIST);
    g_status_bar = make_control(hwnd, "STATIC",
                                "",
                                SS_OWNERDRAW, 0, ID_STATUS_BAR);

    set_font(g_transcript, g_font);
    set_font(g_prompt, g_font);
    set_font(g_user_badge, g_bold_font);
    SendMessage(g_transcript, EM_LIMITTEXT, EDIT_TEXT_LIMIT, 0);
    SendMessage(g_prompt, EM_LIMITTEXT, AG_MAX_MESSAGE - 512, 0);
    g_prompt_proc = (WNDPROC)SetWindowLong(g_prompt, GWL_WNDPROC,
                                           (LONG)prompt_proc);

    fill_initial_content(hwnd);
}

static void move(HWND hwnd, int x, int y, int w, int h)
{
    MoveWindow(hwnd, x, y, w, h, TRUE);
}

static void layout_children(HWND hwnd)
{
    RECT rc;
    int margin;
    int gap;
    int toolbar_y;
    int toolbar_h;
    int content_y;
    int content_h;
    int left_w;
    int right_w;
    int center_w;
    int left_x;
    int center_x;
    int right_x;
    int row_y;
    int footer_h;
    int label_h;
    int user_h;
    int composer_h;
    int header_h;
    int status_h;
    int transcript_h;
    int prompt_h;
    int bottom_row_h;
    int control_x;
    int right_section_h;
    int statusbar_h;
    int footer_top;
    int access_w;
    int model_w;
    int model_max_right;
    int mic_x;

    GetClientRect(hwnd, &rc);

    margin = 8;
    gap = 6;
    toolbar_y = 6;
    toolbar_h = 36;
    content_y = toolbar_y + toolbar_h + 6;
    statusbar_h = 20;
    content_h = rc.bottom - content_y - margin - statusbar_h - 4;
    left_w = 238;
    right_w = 245;

    if (rc.right < 900) {
        left_w = 190;
        right_w = 205;
    }
    center_w = rc.right - (margin * 2) - left_w - right_w - (gap * 2);
    if (center_w < 340) {
        center_w = 340;
    }

    left_x = margin;
    center_x = left_x + left_w + gap;
    right_x = center_x + center_w + gap;

    move(g_new_chat, margin, toolbar_y, 82, 28);
    move(g_search, margin + 88, toolbar_y, 76, 28);
    move(g_scheduled, margin + 170, toolbar_y, 92, 28);
    move(g_plugins, margin + 268, toolbar_y, 78, 28);
    move(g_open_in, rc.right - margin - 150, toolbar_y, 150, 28);
    move(g_toolstrip, 0, toolbar_y + toolbar_h, rc.right, 6);

    label_h = 24;
    footer_h = 94;
    user_h = 86;
    row_y = content_y;

    move(g_projects_label, left_x, row_y, left_w, label_h);
    row_y += label_h;
    move(g_projects_list, left_x, row_y, left_w,
         content_y + content_h - row_y - footer_h - gap);
    footer_top = content_y + content_h - user_h;
    move(g_user_badge, left_x, footer_top, 38, 38);
    move(g_user_text, left_x + 48, footer_top + 2, left_w - 48, 38);
    move(g_usage_text, left_x, footer_top + 44, left_w, 42);

    header_h = 26;
    status_h = 26;
    composer_h = 104;
    transcript_h = content_h - header_h - status_h - composer_h - (gap * 2);

    row_y = content_y;
    move(g_thread_title, center_x, row_y, center_w, header_h);
    row_y += header_h;
    move(g_status, center_x, row_y, center_w, status_h);
    row_y += status_h + gap;
    move(g_transcript, center_x, row_y, center_w, transcript_h);
    row_y += transcript_h + gap;
    move(g_prompt_label, center_x, row_y, center_w, label_h);
    row_y += label_h;
    prompt_h = 44;
    move(g_prompt, center_x, row_y, center_w, prompt_h);
    row_y += prompt_h + 6;
    bottom_row_h = 28;
    control_x = center_x;
    move(g_add, control_x, row_y, 30, bottom_row_h);
    control_x += 36;
    access_w = 104;
    move(g_access, control_x, row_y, access_w, bottom_row_h);
    control_x += access_w + 6;
    mic_x = center_x + center_w - 104;
    model_max_right = mic_x - 8;
    model_w = model_max_right - control_x;
    if (model_w > 150) {
        model_w = 150;
    }
    if (model_w < 116) {
        model_w = 116;
    }
    move(g_model, control_x, row_y, model_w, bottom_row_h);
    move(g_mic, mic_x, row_y, 34, bottom_row_h);
    move(g_send, mic_x + 40, row_y, 64, bottom_row_h);

    right_section_h = (content_h - (label_h * 3) - (gap * 2)) / 3;
    row_y = content_y;
    move(g_env_label, right_x, row_y, right_w, label_h);
    row_y += label_h;
    move(g_env_list, right_x, row_y, right_w, right_section_h);
    row_y += right_section_h + gap;
    move(g_tasks_label, right_x, row_y, right_w, label_h);
    row_y += label_h;
    move(g_tasks_list, right_x, row_y, right_w, right_section_h);
    row_y += right_section_h + gap;
    move(g_changes_label, right_x, row_y, right_w, label_h);
    row_y += label_h;
    move(g_changes_list, right_x, row_y, right_w,
         content_y + content_h - row_y);
    move(g_status_bar, 0, rc.bottom - statusbar_h, rc.right, statusbar_h);
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                    LPARAM lparam)
{
    DRAWITEMSTRUCT *draw_item;
    MEASUREITEMSTRUCT *measure_item;
    HDC ctl_dc;
    HWND ctl_hwnd;

    switch (msg) {
    case WM_CREATE:
        setup_menu(hwnd);
        create_children(hwnd);
        layout_children(hwnd);
        set_runtime_status("Ready");
        request_project_metadata(hwnd);
        return 0;

    case WM_SIZE:
        layout_children(hwnd);
        return 0;

    case WM_XPAGENT_STATUS:
        set_runtime_status((const char *)lparam);
        heap_free_text(lparam);
        return 0;

    case WM_XPAGENT_REPLY:
        append_transcript_message_utf8("Assistant", (const char *)lparam);
        set_request_in_flight(0);
        heap_free_text(lparam);
        return 0;

    case WM_XPAGENT_ERROR:
        append_transcript_message_utf8("Error", (const char *)lparam);
        set_runtime_status("Error");
        set_request_in_flight(0);
        heap_free_text(lparam);
        return 0;

    case WM_XPAGENT_PROJECTS:
        apply_project_metadata(hwnd, (char *)lparam);
        heap_free_text(lparam);
        return 0;

    case WM_DRAWITEM:
        draw_item = (DRAWITEMSTRUCT *)lparam;
        if (draw_item && is_section_label((int)draw_item->CtlID)) {
            draw_section_label(draw_item);
            return TRUE;
        }
        if (draw_item && draw_item->CtlID == ID_USER_BADGE) {
            draw_agent_badge(draw_item);
            return TRUE;
        }
        if (draw_item && draw_item->CtlID == ID_USER_TEXT) {
            draw_user_info(draw_item);
            return TRUE;
        }
        if (draw_item && draw_item->CtlID == ID_USAGE_TEXT) {
            draw_usage_info(draw_item);
            return TRUE;
        }
        if (draw_item && is_dropdown_button((int)draw_item->CtlID)) {
            draw_dropdown_button(draw_item);
            return TRUE;
        }
        if (draw_item && is_owner_button((int)draw_item->CtlID)) {
            draw_button(draw_item);
            return TRUE;
        }
        if (draw_item && draw_item->CtlID == ID_STATUS) {
            draw_status_line(draw_item);
            return TRUE;
        }
        if (draw_item && draw_item->CtlID == ID_TOOLSTRIP) {
            draw_toolstrip(draw_item);
            return TRUE;
        }
        if (draw_item && draw_item->CtlID == ID_STATUS_BAR) {
            draw_status_bar(draw_item);
            return TRUE;
        }
        if (draw_item && is_owner_list((int)draw_item->CtlID)) {
            draw_owner_list(draw_item);
            return TRUE;
        }
        break;

    case WM_MEASUREITEM:
        measure_item = (MEASUREITEMSTRUCT *)lparam;
        if (measure_item && is_owner_list((int)measure_item->CtlID)) {
            measure_item->itemHeight = 16;
            return TRUE;
        }
        break;

    case WM_CTLCOLORSTATIC:
        ctl_dc = (HDC)wparam;
        ctl_hwnd = (HWND)lparam;
        SetBkMode(ctl_dc, TRANSPARENT);
        if (ctl_hwnd == g_status) {
            SetTextColor(ctl_dc, RGB(72, 72, 72));
            return (LRESULT)g_bg_brush;
        }
        if (ctl_hwnd == g_user_text) {
            SetTextColor(ctl_dc, RGB(0, 0, 0));
            return (LRESULT)g_bg_brush;
        }
        return (LRESULT)g_bg_brush;

    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case ID_PROJECTS_LIST:
            if (HIWORD(wparam) == LBN_SELCHANGE) {
                handle_project_selection(hwnd);
            }
            return 0;
        case ID_NEW_CHAT:
        case 2001:
            reset_conversation(hwnd);
            return 0;
        case ID_SEND:
            handle_send(hwnd);
            return 0;
        case ID_OPEN_IN: {
            const char *items[] = {"Open in...", "Explorer", "Command Prompt"};
            show_choice_menu(hwnd, g_open_in, items, 3);
            return 0;
        }
        case ID_ACCESS: {
            const char *items[] = {"Full access", "Read only", "Ask first"};
            show_choice_menu(hwnd, g_access, items, 3);
            return 0;
        }
        case ID_MODEL: {
            const char *items[] = {"5.5 Extra High", "5.5 High", "5.5 Fast"};
            show_choice_menu(hwnd, g_model, items, 3);
            return 0;
        }
        case 2003:
            copy_focused_control();
            return 0;
        case 2004:
            select_all_focused_control();
            return 0;
        case 2005:
            refresh_runtime_lists();
            request_project_metadata(hwnd);
            set_runtime_status("Ready");
            return 0;
        case 2006:
            handle_reconnect(hwnd);
            return 0;
        case 2002:
            DestroyWindow(hwnd);
            return 0;
        default:
            break;
        }
        break;

    case WM_DESTROY:
        close_gateway_socket();
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE hprev, LPSTR cmdline,
                   int show)
{
    WNDCLASS wc;
    HWND hwnd;
    MSG msg;
    WSADATA wsa;

    (void)hprev;
    (void)cmdline;

    g_hinst = hinstance;
    setup_fonts();
    setup_brushes();
    load_assets();
    InitializeCriticalSection(&g_socket_lock);
    g_socket_lock_ready = 1;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) {
        g_winsock_ready = 1;
    }

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = window_proc;
    wc.hInstance = hinstance;
    wc.hIcon = g_app_icon_big ? g_app_icon_big : LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = g_bg_brush;
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, "RegisterClass failed.", "XPAgent", MB_ICONERROR);
        return 1;
    }

    hwnd = CreateWindowEx(
        WS_EX_APPWINDOW,
        CLASS_NAME,
        "XPAgent - Set up Windows XP ISO",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        0,
        0,
        800,
        570,
        NULL,
        NULL,
        hinstance,
        NULL);
    if (!hwnd) {
        MessageBox(NULL, "CreateWindowEx failed.", "XPAgent", MB_ICONERROR);
        return 1;
    }

    if (g_app_icon_big) {
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)g_app_icon_big);
    }
    if (g_app_icon_small) {
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)g_app_icon_small);
    }

    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (g_bold_font) {
        DeleteObject(g_bold_font);
    }
    if (g_mono_font) {
        DeleteObject(g_mono_font);
    }
    if (g_small_font) {
        DeleteObject(g_small_font);
    }
    if (g_bg_brush) {
        DeleteObject(g_bg_brush);
    }
    if (g_header_brush) {
        DeleteObject(g_header_brush);
    }
    if (g_white_brush) {
        DeleteObject(g_white_brush);
    }
    cleanup_assets();
    if (g_winsock_ready) {
        WSACleanup();
    }
    if (g_socket_lock_ready) {
        DeleteCriticalSection(&g_socket_lock);
    }

    return (int)msg.wParam;
}

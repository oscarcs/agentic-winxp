#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifndef BS_FLAT
#define BS_FLAT 0x8000
#endif

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
#define ID_SOURCES_LABEL 1028
#define ID_SOURCES_LIST 1029

static const char *CLASS_NAME = "XPAgentGuiShell";

static HINSTANCE g_hinst;
static HFONT g_font;
static HFONT g_bold_font;
static HFONT g_mono_font;
static HBRUSH g_bg_brush;
static HBRUSH g_panel_brush;
static HBRUSH g_header_brush;
static HBRUSH g_white_brush;

static HWND g_new_chat;
static HWND g_search;
static HWND g_scheduled;
static HWND g_plugins;
static HWND g_open_in;
static HWND g_thread_title;
static HWND g_status;
static HWND g_projects_label;
static HWND g_projects_list;
static HWND g_user_badge;
static HWND g_user_text;
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
static HWND g_sources_label;
static HWND g_sources_list;

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

static HWND make_label(HWND parent, const char *text, int id)
{
    HWND hwnd;

    hwnd = make_control(parent, "STATIC", text, SS_LEFT | SS_CENTERIMAGE, 0, id);
    set_font(hwnd, g_bold_font);
    return hwnd;
}

static void add_list_item(HWND list, const char *text)
{
    SendMessage(list, LB_ADDSTRING, 0, (LPARAM)text);
}

static void add_combo_item(HWND combo, const char *text)
{
    SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)text);
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
}

static void setup_brushes(void)
{
    g_bg_brush = CreateSolidBrush(RGB(246, 248, 252));
    g_panel_brush = CreateSolidBrush(RGB(255, 255, 255));
    g_header_brush = CreateSolidBrush(RGB(239, 243, 250));
    g_white_brush = CreateSolidBrush(RGB(255, 255, 255));
}

static void draw_rect(HDC hdc, int x, int y, int w, int h, HBRUSH fill)
{
    RECT rc;

    rc.left = x;
    rc.top = y;
    rc.right = x + w;
    rc.bottom = y + h;
    FillRect(hdc, &rc, fill);
}

static void draw_panel(HDC hdc, int x, int y, int w, int h)
{
    RECT rc;

    rc.left = x;
    rc.top = y;
    rc.right = x + w;
    rc.bottom = y + h;
    FillRect(hdc, &rc, g_panel_brush);
    DrawEdge(hdc, &rc, EDGE_ETCHED, BF_RECT);
}

static void paint_chrome(HWND hwnd, HDC hdc)
{
    RECT rc;
    HPEN line_pen;
    HPEN old_pen;
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
    int label_h;
    int footer_h;
    int header_h;
    int status_h;
    int changes_h;
    int composer_h;
    int transcript_h;
    int prompt_y;

    GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, g_bg_brush);

    margin = 8;
    gap = 6;
    toolbar_y = 6;
    toolbar_h = 36;
    content_y = toolbar_y + toolbar_h + 6;
    content_h = rc.bottom - content_y - margin;
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
    label_h = 24;
    footer_h = 64;

    draw_rect(hdc, 0, 0, rc.right, toolbar_h + 12, g_header_brush);
    line_pen = CreatePen(PS_SOLID, 1, RGB(190, 198, 214));
    old_pen = (HPEN)SelectObject(hdc, line_pen);
    MoveToEx(hdc, 0, toolbar_h + 11, NULL);
    LineTo(hdc, rc.right, toolbar_h + 11);
    SelectObject(hdc, old_pen);
    DeleteObject(line_pen);

    draw_panel(hdc, left_x - 1, content_y - 1, left_w + 2, content_h + 2);
    draw_panel(hdc, right_x - 1, content_y - 1, right_w + 2, content_h + 2);

    draw_rect(hdc, left_x, content_y, left_w, label_h, g_header_brush);
    draw_rect(hdc, right_x, content_y, right_w, label_h, g_header_brush);

    header_h = 26;
    status_h = 26;
    changes_h = 118;
    composer_h = 104;
    transcript_h = content_h - header_h - status_h - changes_h - composer_h -
                   (gap * 4);
    prompt_y = content_y + header_h + status_h + gap + transcript_h + gap +
               changes_h + gap;

    draw_panel(hdc, center_x - 1, content_y - 1, center_w + 2,
               header_h + status_h + gap + transcript_h + 2);
    draw_rect(hdc, center_x, content_y, center_w, header_h, g_header_brush);
    draw_panel(hdc, center_x - 1,
               content_y + header_h + status_h + gap + transcript_h + gap - 1,
               center_w + 2, changes_h + 2);
    draw_panel(hdc, center_x - 1, prompt_y - 1, center_w + 2,
               composer_h + 2);
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

static void fill_static_content(void)
{
    const char *sample;

    add_list_item(g_projects_list, "[-] winxp");
    add_list_item(g_projects_list, "    Set up Windows XP ISO        2m");
    add_list_item(g_projects_list, "[+] win2k");
    add_list_item(g_projects_list, "[-] powerstreet");
    add_list_item(g_projects_list, "    Fix road junction markings  1h");
    add_list_item(g_projects_list, "    Update project dependencies 2d");
    add_list_item(g_projects_list, "    Refactor road modules       2d");
    add_list_item(g_projects_list, "[+] firefox-ios");
    add_list_item(g_projects_list, "[+] law-agent");
    add_list_item(g_projects_list, "[+] tycoon");
    SendMessage(g_projects_list, LB_SETCURSEL, 1, 0);

    sample =
        "Worked for 2m 3s\r\n\r\n"
        "Yep, XPAgent now has a tidier native Win32 shell: project navigation "
        "on the left, the active thread in the middle, and environment context "
        "on the right.\r\n\r\n"
        "Added:\r\n"
        "  * guest/xpagent-gui.c\r\n"
        "  * guest/build-xpagent-gui.bat\r\n\r\n"
        "The real version can reuse AG1 networking while this window owns "
        "the transcript, prompt, task list, and environment panes.\r\n\r\n"
        "Next natural step: wire Send to the host gateway and append replies "
        "to this transcript.";
    SetWindowText(g_transcript, sample);

    add_list_item(g_changes_list, "Edited 3 files");
    add_list_item(g_changes_list, "guest/README-XP.txt                 +7 -0");
    add_list_item(g_changes_list, "guest/build-gui-hello.bat          +25 -0");
    add_list_item(g_changes_list, "guest/gui-hello.c                 +155 -0");

    add_list_item(g_env_list, "Changes                         +187 -0");
    add_list_item(g_env_list, "Local");
    add_list_item(g_env_list, "main");
    add_list_item(g_env_list, "Commit or push");
    add_list_item(g_env_list, "Pull request status unavailable");

    add_list_item(g_tasks_list, "./host/agent_gateway.py --backend codex");
    add_list_item(g_tasks_list, "./xpilot_host.py");
    add_list_item(g_sources_list, "web");
    add_list_item(g_sources_list, "filesystem");
    add_list_item(g_sources_list, "xpilot");

    add_combo_item(g_open_in, "Open in");
    add_combo_item(g_open_in, "Explorer");
    add_combo_item(g_open_in, "Command Prompt");
    SendMessage(g_open_in, CB_SETCURSEL, 0, 0);

    add_combo_item(g_access, "Full access");
    add_combo_item(g_access, "Read only");
    add_combo_item(g_access, "Ask first");
    SendMessage(g_access, CB_SETCURSEL, 0, 0);

    add_combo_item(g_model, "5.5 Extra High");
    add_combo_item(g_model, "5.5 High");
    add_combo_item(g_model, "5.5 Fast");
    SendMessage(g_model, CB_SETCURSEL, 0, 0);

    SetWindowText(g_prompt, "Ask for follow-up changes");
}

static void create_children(HWND hwnd)
{
    DWORD list_style;
    DWORD edit_style;

    list_style = LBS_NOTIFY | WS_VSCROLL;
    edit_style = ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL;

    g_new_chat = make_control(hwnd, "BUTTON", "New Chat",
                              BS_PUSHBUTTON | BS_FLAT, 0,
                              ID_NEW_CHAT);
    g_search = make_control(hwnd, "BUTTON", "Search", BS_PUSHBUTTON | BS_FLAT, 0,
                            ID_SEARCH);
    g_scheduled = make_control(hwnd, "BUTTON", "Scheduled",
                               BS_PUSHBUTTON | BS_FLAT, 0,
                               ID_SCHEDULED);
    g_plugins = make_control(hwnd, "BUTTON", "Plugins",
                             BS_PUSHBUTTON | BS_FLAT, 0,
                             ID_PLUGINS);
    g_open_in = make_control(hwnd, "COMBOBOX", "", CBS_DROPDOWNLIST, 0,
                             ID_OPEN_IN);

    g_projects_label = make_label(hwnd, "Projects", ID_PROJECTS_LABEL);
    g_projects_list = make_control(hwnd, "LISTBOX", "", list_style,
                                   WS_EX_CLIENTEDGE,
                                   ID_PROJECTS_LIST);
    g_user_badge = make_control(hwnd, "STATIC", "OS", SS_CENTER | WS_BORDER, 0,
                                ID_USER_BADGE);
    g_user_text = make_control(hwnd, "STATIC", "Oscar Sims\r\nPro",
                               SS_LEFT, 0, ID_USER_TEXT);

    g_thread_title = make_label(hwnd, "Set up Windows XP ISO", ID_THREAD_TITLE);
    g_status = make_control(hwnd, "STATIC", "Worked for 2m 3s",
                            SS_LEFT | SS_CENTERIMAGE, 0, ID_STATUS);
    g_transcript = make_control(hwnd, "EDIT", "", edit_style | ES_READONLY,
                                WS_EX_CLIENTEDGE, ID_TRANSCRIPT);
    g_changes_label = make_label(hwnd, "Edited files", ID_CHANGES_LABEL);
    g_changes_list = make_control(hwnd, "LISTBOX", "", list_style,
                                  WS_EX_CLIENTEDGE,
                                  ID_CHANGES_LIST);
    g_prompt_label = make_label(hwnd, "Ask for follow-up changes",
                                ID_PROMPT_LABEL);
    g_prompt = make_control(hwnd, "EDIT", "",
                            ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL |
                                0,
                            WS_EX_CLIENTEDGE, ID_PROMPT);
    g_add = make_control(hwnd, "BUTTON", "+", BS_PUSHBUTTON | BS_FLAT, 0,
                         ID_ADD);
    g_access = make_control(hwnd, "COMBOBOX", "", CBS_DROPDOWNLIST, 0,
                            ID_ACCESS);
    g_model = make_control(hwnd, "COMBOBOX", "", CBS_DROPDOWNLIST, 0, ID_MODEL);
    g_mic = make_control(hwnd, "BUTTON", "Mic", BS_PUSHBUTTON | BS_FLAT, 0,
                         ID_MIC);
    g_send = make_control(hwnd, "BUTTON", "Send", BS_DEFPUSHBUTTON, 0, ID_SEND);

    g_env_label = make_label(hwnd, "Environment", ID_ENV_LABEL);
    g_env_list = make_control(hwnd, "LISTBOX", "", list_style,
                              WS_EX_CLIENTEDGE, ID_ENV_LIST);
    g_tasks_label = make_label(hwnd, "Tasks", ID_TASKS_LABEL);
    g_tasks_list = make_control(hwnd, "LISTBOX", "", list_style,
                                WS_EX_CLIENTEDGE,
                                ID_TASKS_LIST);
    g_sources_label = make_label(hwnd, "Sources", ID_SOURCES_LABEL);
    g_sources_list = make_control(hwnd, "LISTBOX", "", list_style,
                                  WS_EX_CLIENTEDGE,
                                  ID_SOURCES_LIST);

    set_font(g_transcript, g_font);
    set_font(g_prompt, g_font);
    set_font(g_user_badge, g_bold_font);

    fill_static_content();
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
    int changes_h;
    int header_h;
    int status_h;
    int transcript_h;
    int prompt_h;
    int bottom_row_h;
    int control_x;
    int right_section_h;

    GetClientRect(hwnd, &rc);

    margin = 8;
    gap = 6;
    toolbar_y = 6;
    toolbar_h = 36;
    content_y = toolbar_y + toolbar_h + 6;
    content_h = rc.bottom - content_y - margin;
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
    move(g_open_in, rc.right - margin - 150, toolbar_y, 150, 220);

    label_h = 24;
    footer_h = 64;
    user_h = 44;
    row_y = content_y;

    move(g_projects_label, left_x, row_y, left_w, label_h);
    row_y += label_h;
    move(g_projects_list, left_x, row_y, left_w,
         content_y + content_h - row_y - footer_h - gap);
    move(g_user_badge, left_x, content_y + content_h - user_h, 38, 38);
    move(g_user_text, left_x + 48, content_y + content_h - user_h + 2,
         left_w - 48, 38);

    header_h = 26;
    status_h = 26;
    changes_h = 118;
    composer_h = 104;
    transcript_h = content_h - header_h - status_h - changes_h - composer_h -
                   (gap * 4);

    row_y = content_y;
    move(g_thread_title, center_x, row_y, center_w, header_h);
    row_y += header_h;
    move(g_status, center_x, row_y, center_w, status_h);
    row_y += status_h + gap;
    move(g_transcript, center_x, row_y, center_w, transcript_h);
    row_y += transcript_h + gap;
    move(g_changes_label, center_x, row_y, center_w, label_h);
    row_y += label_h;
    move(g_changes_list, center_x, row_y, center_w, changes_h - label_h);
    row_y += changes_h - label_h + gap;
    move(g_prompt_label, center_x, row_y, center_w, label_h);
    row_y += label_h;
    prompt_h = 44;
    move(g_prompt, center_x, row_y, center_w, prompt_h);
    row_y += prompt_h + 6;
    bottom_row_h = 26;
    control_x = center_x;
    move(g_add, control_x, row_y, 30, bottom_row_h);
    control_x += 36;
    move(g_access, control_x, row_y, 110, 160);
    control_x += 116;
    move(g_model, control_x, row_y, 115, 160);
    control_x = center_x + center_w - 92;
    move(g_mic, control_x, row_y, 40, bottom_row_h);
    move(g_send, control_x + 46, row_y, 46, bottom_row_h);

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
    move(g_sources_label, right_x, row_y, right_w, label_h);
    row_y += label_h;
    move(g_sources_list, right_x, row_y, right_w,
         content_y + content_h - row_y);
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                    LPARAM lparam)
{
    switch (msg) {
    case WM_CREATE:
        setup_menu(hwnd);
        create_children(hwnd);
        layout_children(hwnd);
        return 0;

    case WM_SIZE:
        layout_children(hwnd);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case ID_SEND:
            MessageBox(hwnd,
                       "Networking comes next. This pass blocks out the UI.",
                       "XPAgent",
                       MB_OK | MB_ICONINFORMATION);
            return 0;
        case 2002:
            DestroyWindow(hwnd);
            return 0;
        default:
            break;
        }
        break;

    case WM_DESTROY:
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

    (void)hprev;
    (void)cmdline;

    g_hinst = hinstance;
    setup_fonts();
    setup_brushes();

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = window_proc;
    wc.hInstance = hinstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, "RegisterClass failed.", "XPAgent", MB_ICONERROR);
        return 1;
    }

    hwnd = CreateWindowEx(
        WS_EX_APPWINDOW,
        CLASS_NAME,
        "XPAgent - Set up Windows XP ISO",
        WS_OVERLAPPEDWINDOW,
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
    if (g_bg_brush) {
        DeleteObject(g_bg_brush);
    }
    if (g_panel_brush) {
        DeleteObject(g_panel_brush);
    }
    if (g_header_brush) {
        DeleteObject(g_header_brush);
    }
    if (g_white_brush) {
        DeleteObject(g_white_brush);
    }

    return (int)msg.wParam;
}

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
#define ID_STATUS_BAR 1030
#define ID_TOOLSTRIP 1031

static const char *CLASS_NAME = "XPAgentGuiShell";

static HINSTANCE g_hinst;
static HFONT g_font;
static HFONT g_bold_font;
static HFONT g_mono_font;
static HBRUSH g_bg_brush;
static HBRUSH g_header_brush;
static HBRUSH g_white_brush;

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
static HWND g_status_bar;

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

static int is_section_label(int id)
{
    return id == ID_PROJECTS_LABEL ||
           id == ID_THREAD_TITLE ||
           id == ID_CHANGES_LABEL ||
           id == ID_PROMPT_LABEL ||
           id == ID_ENV_LABEL ||
           id == ID_TASKS_LABEL ||
           id == ID_SOURCES_LABEL;
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

static int is_owner_list(int id)
{
    return id == ID_PROJECTS_LIST ||
           id == ID_CHANGES_LIST ||
           id == ID_ENV_LIST ||
           id == ID_TASKS_LIST ||
           id == ID_SOURCES_LIST;
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
               item->CtlID == ID_MIC;
    if (has_icon) {
        icon_rc.left = rc.left + 8;
        icon_rc.top = rc.top + ((rc.bottom - rc.top) - 16) / 2;
        icon_rc.right = icon_rc.left + 16;
        icon_rc.bottom = icon_rc.top + 16;
        if (item->CtlID == ID_ADD) {
            icon_rc.left = rc.left + ((rc.right - rc.left) - 16) / 2;
        }
        if (selected) {
            OffsetRect(&icon_rc, 1, 1);
        }
        draw_toolbar_icon(item->hDC, (int)item->CtlID, &icon_rc, icon_color);
        if (item->CtlID != ID_ADD) {
            text_rc.left += 17;
        }
    }

    old_font = SelectObject(item->hDC, g_font);
    old_mode = SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, text_color);
    if (item->CtlID != ID_ADD) {
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
    draw_status_segment(item->hDC, &rc, 0, 64, "Ready");
    draw_status_segment(item->hDC, &rc, 64, 188, "xpilot connected");
    draw_status_segment(item->hDC, &rc, 188, 360, "gateway 10.0.2.2:7790");
    draw_status_segment(item->hDC, &rc, 360, rc.right, "portable C core");
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
            draw_project_box(item->hDC, rc.left + 4, rc.top + 3, expanded,
                             icon_color);
            text_rc.left = rc.left + 20;
            DrawText(item->hDC, text + 4, -1, &text_rc,
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        } else if (text[0] == ' ') {
            split_project_child(text, label, sizeof(label), age, sizeof(age));
            draw_file_icon(item->hDC, rc.left + 18, rc.top + 2, icon_color);
            text_rc.left = rc.left + 34;
            age_rc = rc;
            age_rc.left = rc.right - 34;
            age_rc.right = rc.right - 6;
            DrawText(item->hDC, label, -1, &text_rc,
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE);
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
    g_open_in = make_control(hwnd, "COMBOBOX", "", CBS_DROPDOWNLIST, 0,
                             ID_OPEN_IN);
    g_toolstrip = make_control(hwnd, "STATIC", "", SS_OWNERDRAW, 0,
                               ID_TOOLSTRIP);

    g_projects_label = make_section_label(hwnd, "Projects", ID_PROJECTS_LABEL);
    g_projects_list = make_control(hwnd, "LISTBOX", "", list_style,
                                   WS_EX_CLIENTEDGE,
                                   ID_PROJECTS_LIST);
    g_user_badge = make_control(hwnd, "STATIC", "XP", SS_OWNERDRAW, 0,
                                ID_USER_BADGE);
    g_user_text = make_control(hwnd, "STATIC", "Oscar Sims\r\nPro",
                               SS_LEFT, 0, ID_USER_TEXT);

    g_thread_title = make_section_label(hwnd, "Set up Windows XP ISO",
                                        ID_THREAD_TITLE);
    g_status = make_control(hwnd, "STATIC", "Worked for 2m 3s",
                            SS_OWNERDRAW, 0, ID_STATUS);
    g_transcript = make_control(hwnd, "EDIT", "", edit_style | ES_READONLY,
                                WS_EX_CLIENTEDGE, ID_TRANSCRIPT);
    g_changes_label = make_section_label(hwnd, "Edited files", ID_CHANGES_LABEL);
    g_changes_list = make_control(hwnd, "LISTBOX", "", list_style,
                                  WS_EX_CLIENTEDGE,
                                  ID_CHANGES_LIST);
    g_prompt_label = make_section_label(hwnd, "Ask for follow-up changes",
                                        ID_PROMPT_LABEL);
    g_prompt = make_control(hwnd, "EDIT", "",
                            ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL |
                                0,
                            WS_EX_CLIENTEDGE, ID_PROMPT);
    g_add = make_control(hwnd, "BUTTON", "+", BS_OWNERDRAW, 0, ID_ADD);
    g_access = make_control(hwnd, "COMBOBOX", "", CBS_DROPDOWNLIST, 0,
                            ID_ACCESS);
    g_model = make_control(hwnd, "COMBOBOX", "", CBS_DROPDOWNLIST, 0, ID_MODEL);
    g_mic = make_control(hwnd, "BUTTON", "Mic", BS_OWNERDRAW, 0, ID_MIC);
    g_send = make_control(hwnd, "BUTTON", "Send", BS_OWNERDRAW, 0, ID_SEND);

    g_env_label = make_section_label(hwnd, "Environment", ID_ENV_LABEL);
    g_env_list = make_control(hwnd, "LISTBOX", "", list_style,
                              WS_EX_CLIENTEDGE, ID_ENV_LIST);
    g_tasks_label = make_section_label(hwnd, "Tasks", ID_TASKS_LABEL);
    g_tasks_list = make_control(hwnd, "LISTBOX", "", list_style,
                                WS_EX_CLIENTEDGE,
                                ID_TASKS_LIST);
    g_sources_label = make_section_label(hwnd, "Sources", ID_SOURCES_LABEL);
    g_sources_list = make_control(hwnd, "LISTBOX", "", list_style,
                                  WS_EX_CLIENTEDGE,
                                  ID_SOURCES_LIST);
    g_status_bar = make_control(hwnd, "STATIC",
                                "Ready | xpilot connected | gateway 10.0.2.2:7790 | portable C core",
                                SS_OWNERDRAW, 0, ID_STATUS_BAR);

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
    int statusbar_h;

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
    move(g_open_in, rc.right - margin - 150, toolbar_y, 150, 220);
    move(g_toolstrip, 0, toolbar_y + toolbar_h, rc.right, 6);

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
    control_x = center_x + center_w - 104;
    move(g_mic, control_x, row_y, 42, bottom_row_h);
    move(g_send, control_x + 48, row_y, 56, bottom_row_h);

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
        return 0;

    case WM_SIZE:
        layout_children(hwnd);
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
    if (g_header_brush) {
        DeleteObject(g_header_brush);
    }
    if (g_white_brush) {
        DeleteObject(g_white_brush);
    }

    return (int)msg.wParam;
}

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define IDC_STATUS 1001
#define IDC_HELLO 1002
#define IDC_CLOSE 1003

static const char *CLASS_NAME = "AgenticWinXPHelloWindow";
static HWND g_status;

static void set_default_font(HWND hwnd)
{
    SendMessage(hwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                    LPARAM lparam)
{
    HWND child;

    switch (msg) {
    case WM_CREATE:
        child = CreateWindow(
            "STATIC",
            "Hello from Windows XP + TinyCC.",
            WS_CHILD | WS_VISIBLE,
            18,
            18,
            300,
            20,
            hwnd,
            NULL,
            ((LPCREATESTRUCT)lparam)->hInstance,
            NULL);
        set_default_font(child);

        g_status = CreateWindow(
            "STATIC",
            "This is a native Win32 GUI program built inside the XP guest.",
            WS_CHILD | WS_VISIBLE,
            18,
            48,
            320,
            40,
            hwnd,
            (HMENU)IDC_STATUS,
            ((LPCREATESTRUCT)lparam)->hInstance,
            NULL);
        set_default_font(g_status);

        child = CreateWindow(
            "BUTTON",
            "Say hello",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            18,
            104,
            96,
            28,
            hwnd,
            (HMENU)IDC_HELLO,
            ((LPCREATESTRUCT)lparam)->hInstance,
            NULL);
        set_default_font(child);

        child = CreateWindow(
            "BUTTON",
            "Close",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            126,
            104,
            80,
            28,
            hwnd,
            (HMENU)IDC_CLOSE,
            ((LPCREATESTRUCT)lparam)->hInstance,
            NULL);
        set_default_font(child);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case IDC_HELLO:
            SetWindowText(g_status, "Hello, agent. The GUI event loop works.");
            MessageBeep(MB_OK);
            return 0;
        case IDC_CLOSE:
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

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = window_proc;
    wc.hInstance = hinstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, "RegisterClass failed.", "GUI hello", MB_ICONERROR);
        return 1;
    }

    hwnd = CreateWindowEx(
        WS_EX_APPWINDOW,
        CLASS_NAME,
        "Agentic WinXP GUI Hello",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        380,
        190,
        NULL,
        NULL,
        hinstance,
        NULL);
    if (!hwnd) {
        MessageBox(NULL, "CreateWindowEx failed.", "GUI hello", MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

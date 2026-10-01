#include <windows.h>
#include <commctrl.h>
#include "ui/main_window.h"

#pragma comment(lib, "comctl32.lib")

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // 1. Enable modern Common Controls v6
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    // 2. Enable Per-Monitor V2 DPI awareness if possible
    typedef BOOL(WINAPI* PFN_SetProcessDpiAwarenessContext)(DPI_AWARENESS_CONTEXT);
    auto pfnSetDpi = (PFN_SetProcessDpiAwarenessContext)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext");
    if (pfnSetDpi) {
        pfnSetDpi(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }

    // 3. Register and create Main Window
    if (!MainWindow::Register(hInstance)) {
        MessageBoxW(NULL, L"Failed to register Bootelware window class.", L"Bootelware - Fatal Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    HWND hWnd = MainWindow::Create(hInstance);
    if (!hWnd) {
        MessageBoxW(NULL, L"Failed to create Bootelware main window.", L"Bootelware - Fatal Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // 4. Message Pump
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}

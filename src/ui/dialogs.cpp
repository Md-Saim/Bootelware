#include "dialogs.h"
#include "dark_theme.h"
#include "../core/checksum.h"
#include <commctrl.h>
#include <shellapi.h>
#include <thread>
#include <vector>

#pragma comment(lib, "comctl32.lib")

// --- Win11 Options Dialog Implementation ---
static Win11BypassOptions s_tempWin11Opts;
static bool s_win11Confirmed = false;

static LRESULT CALLBACK Win11DlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hChk1, hChk2, hChk3, hEditUser, hChk4, hChk5, hBtnOk, hBtnCancel;

    switch (msg) {
    case WM_CREATE: {
        int y = 20;
        CreateWindowW(L"STATIC", L"Windows 11 Setup Customization", WS_CHILD | WS_VISIBLE, 20, y, 420, 24, hWnd, NULL, NULL, NULL);
        y += 35;

        hChk1 = CreateWindowW(L"BUTTON", L"Remove requirement for 4GB+ RAM, Secure Boot and TPM 2.0", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, y, 430, 24, hWnd, (HMENU)1001, NULL, NULL);
        SendMessageW(hChk1, BM_SETCHECK, s_tempWin11Opts.bypassTpmSecureBootRam ? BST_CHECKED : BST_UNCHECKED, 0);
        y += 30;

        hChk2 = CreateWindowW(L"BUTTON", L"Remove requirement for an online Microsoft account (BypassNRO)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, y, 430, 24, hWnd, (HMENU)1002, NULL, NULL);
        SendMessageW(hChk2, BM_SETCHECK, s_tempWin11Opts.bypassOnlineAccount ? BST_CHECKED : BST_UNCHECKED, 0);
        y += 30;

        hChk3 = CreateWindowW(L"BUTTON", L"Create a local account with username:", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, y, 260, 24, hWnd, (HMENU)1003, NULL, NULL);
        SendMessageW(hChk3, BM_SETCHECK, s_tempWin11Opts.createLocalAccount ? BST_CHECKED : BST_UNCHECKED, 0);

        hEditUser = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", s_tempWin11Opts.localAccountUsername.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 285, y + 2, 140, 22, hWnd, (HMENU)1004, NULL, NULL);
        y += 35;

        hChk4 = CreateWindowW(L"BUTTON", L"Disable BitLocker automatic device encryption", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, y, 430, 24, hWnd, (HMENU)1005, NULL, NULL);
        SendMessageW(hChk4, BM_SETCHECK, s_tempWin11Opts.disableBitLocker ? BST_CHECKED : BST_UNCHECKED, 0);
        y += 30;

        hChk5 = CreateWindowW(L"BUTTON", L"Disable diagnostic data collection (Telemetry)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, y, 430, 24, hWnd, (HMENU)1006, NULL, NULL);
        SendMessageW(hChk5, BM_SETCHECK, s_tempWin11Opts.disableTelemetry ? BST_CHECKED : BST_UNCHECKED, 0);
        y += 45;

        hBtnOk = CreateWindowW(L"BUTTON", L"Save Options", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 230, y, 110, 32, hWnd, (HMENU)IDOK, NULL, NULL);
        hBtnCancel = CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 350, y, 90, 32, hWnd, (HMENU)IDCANCEL, NULL, NULL);

        EnumChildWindows(hWnd, [](HWND hChild, LPARAM) -> BOOL {
            SendMessageW(hChild, WM_SETFONT, (WPARAM)Theme::ThemeManager::hFontRegular, TRUE);
            return TRUE;
        }, 0);
        break;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == IDOK) {
            s_tempWin11Opts.bypassTpmSecureBootRam = (SendMessageW(hChk1, BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_tempWin11Opts.bypassOnlineAccount = (SendMessageW(hChk2, BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_tempWin11Opts.createLocalAccount = (SendMessageW(hChk3, BM_GETCHECK, 0, 0) == BST_CHECKED);

            wchar_t userBuf[64] = { 0 };
            GetWindowTextW(hEditUser, userBuf, 64);
            s_tempWin11Opts.localAccountUsername = userBuf;
            if (s_tempWin11Opts.localAccountUsername.empty()) s_tempWin11Opts.localAccountUsername = L"User";

            s_tempWin11Opts.disableBitLocker = (SendMessageW(hChk4, BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_tempWin11Opts.disableTelemetry = (SendMessageW(hChk5, BM_GETCHECK, 0, 0) == BST_CHECKED);

            s_win11Confirmed = true;
            DestroyWindow(hWnd);
        } else if (id == IDCANCEL) {
            s_win11Confirmed = false;
            DestroyWindow(hWnd);
        }
        break;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, Theme::CLR_DARK);
        SetBkColor(hdc, Theme::CLR_WHITE);
        return (LRESULT)Theme::ThemeManager::hbrWhite;
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, Theme::CLR_DARK);
        SetBkColor(hdc, Theme::CLR_WHITE);
        return (LRESULT)Theme::ThemeManager::hbrWhite;
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hWnd, &rc);
        FillRect(hdc, &rc, Theme::ThemeManager::hbrCanvas);
        return 1;
    }
    case WM_CLOSE:
        s_win11Confirmed = false;
        DestroyWindow(hWnd);
        break;
    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

bool Dialogs::ShowWin11OptionsDialog(HWND hParent, Win11BypassOptions& inOutOpts) {
    s_tempWin11Opts = inOutOpts;
    s_win11Confirmed = false;

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = Win11DlgProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"BootelwareWin11Dlg";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = Theme::ThemeManager::hbrCanvas;
    RegisterClassW(&wc);

    RECT parentRect;
    GetWindowRect(hParent, &parentRect);
    int w = 480, h = 310;
    int x = parentRect.left + (parentRect.right - parentRect.left - w) / 2;
    int y = parentRect.top + (parentRect.bottom - parentRect.top - h) / 2;

    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"BootelwareWin11Dlg", 
                                L"Windows 11 Customization Options", 
                                WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, 
                                x, y, w, h, hParent, NULL, GetModuleHandleW(NULL), NULL);

    EnableWindow(hParent, FALSE);
    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);

    if (s_win11Confirmed) {
        inOutOpts = s_tempWin11Opts;
        return true;
    }
    return false;
}

// --- Checksums Dialog Implementation ---
static std::wstring s_isoPathToCheck;
static ChecksumResult s_checksumRes;
static HWND s_hProgBar, s_hEditMd5, s_hEditSha1, s_hEditSha256, s_hEditSha512, s_hEditVerify, s_hLblMatch;

static void ComputeChecksumsThread(HWND hWnd) {
    ChecksumProgressCallback cb = [hWnd](uint64_t done, uint64_t total, int pct) {
        PostMessageW(s_hProgBar, PBM_SETPOS, (WPARAM)pct, 0);
    };
    s_checksumRes = ChecksumEngine::ComputeFileHashes(s_isoPathToCheck, cb);
    PostMessageW(hWnd, WM_USER + 101, 0, 0);
}

static LRESULT CALLBACK ChecksumDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        int y = 15;
        CreateWindowW(L"STATIC", L"Computing cryptographic checksums for selected ISO...", WS_CHILD | WS_VISIBLE, 20, y, 480, 20, hWnd, NULL, NULL, NULL);
        y += 25;

        s_hProgBar = CreateWindowExW(0, PROGRESS_CLASSW, NULL, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 20, y, 480, 16, hWnd, NULL, NULL, NULL);
        SendMessageW(s_hProgBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        y += 28;

        CreateWindowW(L"STATIC", L"MD5:", WS_CHILD | WS_VISIBLE, 20, y, 70, 20, hWnd, NULL, NULL, NULL);
        s_hEditMd5 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Calculating...", WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL, 95, y, 405, 22, hWnd, NULL, NULL, NULL);
        y += 30;

        CreateWindowW(L"STATIC", L"SHA-1:", WS_CHILD | WS_VISIBLE, 20, y, 70, 20, hWnd, NULL, NULL, NULL);
        s_hEditSha1 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Calculating...", WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL, 95, y, 405, 22, hWnd, NULL, NULL, NULL);
        y += 30;

        CreateWindowW(L"STATIC", L"SHA-256:", WS_CHILD | WS_VISIBLE, 20, y, 70, 20, hWnd, NULL, NULL, NULL);
        s_hEditSha256 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Calculating...", WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL, 95, y, 405, 22, hWnd, NULL, NULL, NULL);
        y += 30;

        CreateWindowW(L"STATIC", L"SHA-512:", WS_CHILD | WS_VISIBLE, 20, y, 70, 20, hWnd, NULL, NULL, NULL);
        s_hEditSha512 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Calculating...", WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL, 95, y, 405, 22, hWnd, NULL, NULL, NULL);
        y += 35;

        CreateWindowW(L"STATIC", L"Verify Hash (Paste expected hash below to compare):", WS_CHILD | WS_VISIBLE, 20, y, 480, 20, hWnd, NULL, NULL, NULL);
        y += 24;

        s_hEditVerify = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 20, y, 350, 24, hWnd, (HMENU)2001, NULL, NULL);
        s_hLblMatch = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 380, y + 2, 120, 22, hWnd, NULL, NULL, NULL);
        y += 38;

        CreateWindowW(L"BUTTON", L"Copy Hashes", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 290, y, 110, 30, hWnd, (HMENU)2002, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 410, y, 90, 30, hWnd, (HMENU)IDCANCEL, NULL, NULL);

        EnumChildWindows(hWnd, [](HWND hChild, LPARAM) -> BOOL {
            SendMessageW(hChild, WM_SETFONT, (WPARAM)Theme::ThemeManager::hFontRegular, TRUE);
            return TRUE;
        }, 0);

        std::thread(ComputeChecksumsThread, hWnd).detach();
        break;
    }
    case WM_USER + 101: {
        SetWindowTextW(s_hEditMd5, s_checksumRes.md5.c_str());
        SetWindowTextW(s_hEditSha1, s_checksumRes.sha1.c_str());
        SetWindowTextW(s_hEditSha256, s_checksumRes.sha256.c_str());
        SetWindowTextW(s_hEditSha512, s_checksumRes.sha512.c_str());
        break;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == 2001 && HIWORD(wParam) == EN_CHANGE) {
            wchar_t testBuf[256] = { 0 };
            GetWindowTextW(s_hEditVerify, testBuf, 256);
            std::wstring input = testBuf;
            if (input.empty()) {
                SetWindowTextW(s_hLblMatch, L"");
            } else if (ChecksumEngine::VerifyHash(s_checksumRes.sha256, input)) {
                SetWindowTextW(s_hLblMatch, L"✓ Match (SHA-256)");
            } else if (ChecksumEngine::VerifyHash(s_checksumRes.md5, input)) {
                SetWindowTextW(s_hLblMatch, L"✓ Match (MD5)");
            } else if (ChecksumEngine::VerifyHash(s_checksumRes.sha1, input)) {
                SetWindowTextW(s_hLblMatch, L"✓ Match (SHA-1)");
            } else if (ChecksumEngine::VerifyHash(s_checksumRes.sha512, input)) {
                SetWindowTextW(s_hLblMatch, L"✓ Match (SHA-512)");
            } else {
                SetWindowTextW(s_hLblMatch, L"✗ No Match");
            }
        } else if (id == 2002) {
            std::wstring all = L"MD5: " + s_checksumRes.md5 + L"\r\nSHA1: " + s_checksumRes.sha1 + 
                               L"\r\nSHA256: " + s_checksumRes.sha256 + L"\r\nSHA512: " + s_checksumRes.sha512;
            if (OpenClipboard(hWnd)) {
                EmptyClipboard();
                HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, (all.size() + 1) * sizeof(wchar_t));
                if (hGlob) {
                    memcpy(GlobalLock(hGlob), all.c_str(), (all.size() + 1) * sizeof(wchar_t));
                    GlobalUnlock(hGlob);
                    SetClipboardData(CF_UNICODETEXT, hGlob);
                }
                CloseClipboard();
            }
        } else if (id == IDCANCEL) {
            DestroyWindow(hWnd);
        }
        break;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, Theme::CLR_DARK);
        SetBkColor(hdc, Theme::CLR_CANVAS);
        return (LRESULT)Theme::ThemeManager::hbrCanvas;
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, Theme::CLR_DARK);
        SetBkColor(hdc, Theme::CLR_WHITE);
        return (LRESULT)Theme::ThemeManager::hbrWhite;
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hWnd, &rc);
        FillRect(hdc, &rc, Theme::ThemeManager::hbrCanvas);
        return 1;
    }
    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;
    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

void Dialogs::ShowChecksumDialog(HWND hParent, const std::wstring& isoPath) {
    s_isoPathToCheck = isoPath;

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = ChecksumDlgProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"BootelwareChecksumDlg";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = Theme::ThemeManager::hbrCanvas;
    RegisterClassW(&wc);

    RECT parentRect;
    GetWindowRect(hParent, &parentRect);
    int w = 535, h = 335;
    int x = parentRect.left + (parentRect.right - parentRect.left - w) / 2;
    int y = parentRect.top + (parentRect.bottom - parentRect.top - h) / 2;

    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"BootelwareChecksumDlg", 
                                L"Image Checksums (MD5, SHA-1, SHA-256, SHA-512)", 
                                WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, 
                                x, y, w, h, hParent, NULL, GetModuleHandleW(NULL), NULL);

    EnableWindow(hParent, FALSE);
    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);
}

// --- Download Official ISOs Dialog Implementation ---
struct DownloadSource {
    std::wstring title;
    std::wstring description;
    std::wstring url;
};

static const std::vector<DownloadSource> s_sources = {
    { L"Windows 11 (Microsoft Official)", L"Official Windows 11 multi-edition ISO direct from Microsoft", L"https://www.microsoft.com/software-download/windows11" },
    { L"Windows 10 (Microsoft Official)", L"Official Windows 10 multi-edition ISO direct from Microsoft", L"https://www.microsoft.com/software-download/windows10" },
    { L"Ubuntu 24.04 LTS Desktop", L"The popular open-source Linux operating system", L"https://ubuntu.com/download/desktop" },
    { L"Debian GNU/Linux (Netinst)", L"Universal and rock-solid Linux distribution", L"https://www.debian.org/distrib/" },
    { L"Fedora Workstation 40", L"Leading Linux distribution with latest open source technologies", L"https://fedoraproject.org/workstation/download" },
    { L"FreeDOS 1.3", L"Complete open source DOS-compatible operating system", L"https://www.freedos.org/download/" },
    { L"MemTest86+ Bootable ISO", L"Advanced memory diagnostics and testing tool", L"https://www.memtest.org/" }
};

static HWND s_hListSources;

static LRESULT CALLBACK DownloadDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        CreateWindowW(L"STATIC", L"Select an official operating system distribution to download:", WS_CHILD | WS_VISIBLE, 20, 15, 480, 20, hWnd, NULL, NULL, NULL);

        s_hListSources = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY, 20, 40, 480, 160, hWnd, (HMENU)3001, NULL, NULL);
        for (const auto& src : s_sources) {
            SendMessageW(s_hListSources, LB_ADDSTRING, 0, (LPARAM)src.title.c_str());
        }
        SendMessageW(s_hListSources, LB_SETCURSEL, 0, 0);

        CreateWindowW(L"BUTTON", L"Open Official Download Page", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 230, 215, 180, 32, hWnd, (HMENU)3002, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 420, 215, 80, 32, hWnd, (HMENU)IDCANCEL, NULL, NULL);

        EnumChildWindows(hWnd, [](HWND hChild, LPARAM) -> BOOL {
            SendMessageW(hChild, WM_SETFONT, (WPARAM)Theme::ThemeManager::hFontRegular, TRUE);
            return TRUE;
        }, 0);
        break;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == 3002 || (id == 3001 && HIWORD(wParam) == LBN_DBLCLK)) {
            int sel = (int)SendMessageW(s_hListSources, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)s_sources.size()) {
                ShellExecuteW(NULL, L"open", s_sources[sel].url.c_str(), NULL, NULL, SW_SHOWNORMAL);
            }
        } else if (id == IDCANCEL) {
            DestroyWindow(hWnd);
        }
        break;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, Theme::CLR_DARK);
        SetBkColor(hdc, Theme::CLR_CANVAS);
        return (LRESULT)Theme::ThemeManager::hbrCanvas;
    }
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, Theme::CLR_DARK);
        SetBkColor(hdc, Theme::CLR_WHITE);
        return (LRESULT)Theme::ThemeManager::hbrWhite;
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hWnd, &rc);
        FillRect(hdc, &rc, Theme::ThemeManager::hbrCanvas);
        return 1;
    }
    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;
    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

void Dialogs::ShowDownloadDialog(HWND hParent) {
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = DownloadDlgProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"BootelwareDownloadDlg";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = Theme::ThemeManager::hbrCanvas;
    RegisterClassW(&wc);

    RECT parentRect;
    GetWindowRect(hParent, &parentRect);
    int w = 535, h = 300;
    int x = parentRect.left + (parentRect.right - parentRect.left - w) / 2;
    int y = parentRect.top + (parentRect.bottom - parentRect.top - h) / 2;

    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"BootelwareDownloadDlg", 
                                L"Download Official Operating System Images", 
                                WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, 
                                x, y, w, h, hParent, NULL, GetModuleHandleW(NULL), NULL);

    EnableWindow(hParent, FALSE);
    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);
}

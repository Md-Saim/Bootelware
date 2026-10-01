#include "main_window.h"
#include "dark_theme.h"
#include "dialogs.h"
#include "../core/disk_writer.h"
#include "../../resources/resource.h"
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <thread>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "msimg32.lib")

// ─── Control Identifiers ───────────────────────────────────────────────────────
enum ControlIds {
    IDC_BTN_MENU = 5001,
    IDC_BTN_REFRESH_SYS,
    IDC_BTN_SELECT_ISO,
    IDC_BTN_DOWNLOAD_ISO,
    IDC_BTN_WIN11_OPTS,
    IDC_BTN_CHECKSUMS,
    IDC_CMB_DEVICE,
    IDC_BTN_REFRESH_DEV,
    IDC_EDIT_LABEL,
    IDC_BTN_ADV_TOGGLE,
    IDC_CMB_SCHEME,
    IDC_CMB_TARGET,
    IDC_CMB_FS,
    IDC_CMB_CLUSTER,
    IDC_CHK_BADBLOCKS,
    IDC_BTN_START,
    IDC_PROGRESS,
    IDC_LBL_STATUS,
    IDC_LBL_SPEED,
    IDC_BTN_TOGGLE_LOG,
    IDC_BTN_COPY_LOG,
    IDC_EDIT_LOG,

    // Drawer Controls
    IDC_DRAWER_CLOSE = 6001,
    IDC_DRAWER_SEC1_BTN,
    IDC_DRAWER_SEC1_BODY,
    IDC_DRAWER_DISC_BTN,
    IDC_DRAWER_DISC_BODY,
    IDC_DRAWER_SEC2_BTN,
    IDC_DRAWER_SEC2_BODY,
    IDC_DRAWER_GITHUB_BTN
};

// ─── Layout Constants ──────────────────────────────────────────────────────────
const int WINDOW_W           = 700;
const int WINDOW_H_BASE      = 780;
const int MARGIN             = 20;
const int INNER_W            = WINDOW_W - 2 * MARGIN - 16; // 644px
const int STAGE_W            = (INNER_W - 14) / 2;         // 315px
const int DRAWER_WIDTH       = 360;
const int EXPANDED_WIDTH     = WINDOW_W + DRAWER_WIDTH;

// ─── Static UI State ───────────────────────────────────────────────────────────
static HWND s_hWndMain           = NULL;
static HWND s_hBtnMenu           = NULL;
static HWND s_hBtnRefreshSys     = NULL;
static HWND s_hBtnSelectIso      = NULL;
static HWND s_hBtnDownloadIso    = NULL;
static HWND s_hBtnWin11Opts      = NULL;
static HWND s_hBtnChecksums      = NULL;
static HWND s_hCmbDevice         = NULL;
static HWND s_hBtnRefreshDev     = NULL;
static HWND s_hEditLabel         = NULL;
static HWND s_hBtnAdvToggle      = NULL;
static HWND s_hCmbScheme         = NULL;
static HWND s_hCmbTarget         = NULL;
static HWND s_hCmbFs             = NULL;
static HWND s_hCmbCluster        = NULL;
static HWND s_hChkBadBlocks      = NULL;
static HWND s_hBtnStart          = NULL;
static HWND s_hProgressBar       = NULL;
static HWND s_hLblStatus         = NULL;
static HWND s_hLblSpeed          = NULL;
static HWND s_hBtnToggleLog      = NULL;
static HWND s_hBtnCopyLog        = NULL;
static HWND s_hEditLog           = NULL;

// Drawer Controls
static HWND s_hDrawer            = NULL;
static HWND s_hDrawerClose       = NULL;
static HWND s_hDrawerSec1Btn     = NULL;
static HWND s_hDrawerSec1Body    = NULL;
static HWND s_hDrawerDiscBtn     = NULL;
static HWND s_hDrawerDiscBody    = NULL;
static HWND s_hDrawerSec2Btn     = NULL;
static HWND s_hDrawerSec2Body    = NULL;
static HWND s_hDrawerGithubBtn   = NULL;

// State Flags
static bool s_isDrawerOpen       = false;
static int  s_currentWindowWidth = WINDOW_W;
static int  s_targetWindowWidth  = WINDOW_W;
static bool s_sec1Expanded       = true;
static bool s_disclaimerExpanded = true;
static bool s_sec2Expanded       = true;
static bool s_advancedExpanded   = false;
static bool s_isLogExpanded      = true;

// Core Engine Data
static SystemDriveInfo           s_sysInfo;
static std::vector<TargetDeviceInfo> s_targetDevices;
static IsoMetadata               s_currentIso;
static Win11BypassOptions        s_win11Options;
static bool                      s_isWriting = false;
static std::atomic<bool>         s_cancelWrite{ false };
static std::wstring              s_overrideWarningText;

// ─── Window Registration & Creation ────────────────────────────────────────────
bool MainWindow::Register(HINSTANCE hInstance) {
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWindow::WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hIconSm = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL; // WM_PAINT handles all background painting
    wc.lpszClassName = L"BootelwareMainWindow";
    if (!RegisterClassExW(&wc)) return false;

    WNDCLASSEXW dwc = { 0 };
    dwc.cbSize = sizeof(WNDCLASSEXW);
    dwc.style = CS_HREDRAW | CS_VREDRAW;
    dwc.lpfnWndProc = MainWindow::DrawerWndProc;
    dwc.hInstance = hInstance;
    dwc.hCursor = LoadCursor(NULL, IDC_ARROW);
    dwc.hbrBackground = NULL;
    dwc.lpszClassName = L"BootelwareDrawerClass";
    RegisterClassExW(&dwc);

    return true;
}

HWND MainWindow::Create(HINSTANCE hInstance) {
    int x = (GetSystemMetrics(SM_CXSCREEN) - WINDOW_W) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - WINDOW_H_BASE) / 2;
    if (y < 20) y = 20;

    HWND hWnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"BootelwareMainWindow",
        L"Bootelware - Smart Bootable USB Creator",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
        x, y, WINDOW_W, WINDOW_H_BASE,
        NULL, NULL, hInstance, NULL
    );

    if (hWnd) {
        HICON hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
        SendMessageW(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessageW(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    }
    return hWnd;
}

// ─── Logging & Telemetry ───────────────────────────────────────────────────────
void MainWindow::AppendLog(const std::wstring& text) {
    if (!s_hEditLog || !IsWindow(s_hEditLog)) return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t timeBuf[32];
    swprintf_s(timeBuf, L"[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
    std::wstring line = timeBuf + text + L"\r\n";
    int len = GetWindowTextLengthW(s_hEditLog);
    SendMessageW(s_hEditLog, EM_SETSEL, len, len);
    SendMessageW(s_hEditLog, EM_REPLACESEL, FALSE, (LPARAM)line.c_str());
    SendMessageW(s_hEditLog, EM_SCROLLCARET, 0, 0);
}

void MainWindow::UpdateProgress(int percent, double speedMb, const std::wstring& taskDesc) {
    if (!s_hWndMain || !IsWindow(s_hWndMain)) return;
    PostMessageW(s_hProgressBar, PBM_SETPOS, (WPARAM)percent, 0);
    SetWindowTextW(s_hLblStatus, taskDesc.c_str());

    std::wstringstream ss;
    if (speedMb > 0.0)
        ss << std::fixed << std::setprecision(1) << speedMb << L" MB/s  ·  " << percent << L"%";
    else
        ss << percent << L"%";
    SetWindowTextW(s_hLblSpeed, ss.str().c_str());
}

// ─── System & Device Detection ─────────────────────────────────────────────────
void MainWindow::RefreshSystemDetection(HWND hWnd) {
    s_sysInfo = SystemDetector::DetectSystemDrive();

    // Auto-select based on system partition style
    if (s_sysInfo.isGpt) {
        SendMessageW(s_hCmbScheme, CB_SETCURSEL, 0, 0); // GPT
        SendMessageW(s_hCmbTarget, CB_SETCURSEL, 0, 0); // UEFI (non-CSM)
    } else {
        SendMessageW(s_hCmbScheme, CB_SETCURSEL, 1, 0); // MBR
        SendMessageW(s_hCmbTarget, CB_SETCURSEL, 1, 0); // BIOS / UEFI-CSM
    }
    s_overrideWarningText.clear();

    AppendLog(L"Host PC Detected: " + s_sysInfo.systemDriveLetter + L" (" + s_sysInfo.diskModel + L")");
    AppendLog(L"Smart Profile: " + s_sysInfo.partitionStyleName + L" partition style · " + s_sysInfo.firmwareBootType);

    InvalidateRect(hWnd, NULL, FALSE);
}

void MainWindow::RefreshDeviceList(HWND hWnd) {
    SendMessageW(s_hCmbDevice, CB_RESETCONTENT, 0, 0);
    s_targetDevices = DeviceManager::EnumerateTargetDrives(s_sysInfo.physicalDriveIndex, true);

    if (s_targetDevices.empty()) {
        SendMessageW(s_hCmbDevice, CB_ADDSTRING, 0, (LPARAM)L"No USB drive detected");
        SendMessageW(s_hCmbDevice, CB_SETCURSEL, 0, 0);
        EnableWindow(s_hBtnStart, FALSE);
    } else {
        for (const auto& dev : s_targetDevices)
            SendMessageW(s_hCmbDevice, CB_ADDSTRING, 0, (LPARAM)dev.displayName.c_str());
        SendMessageW(s_hCmbDevice, CB_SETCURSEL, 0, 0);

        if (s_currentIso.isValid && !s_isWriting)
            EnableWindow(s_hBtnStart, TRUE);
    }
    AppendLog(L"Target drives scanned: " + std::to_wstring(s_targetDevices.size()) + L" device(s) available.");
    InvalidateRect(hWnd, NULL, FALSE);
}

void MainWindow::OnSelectIso(HWND hWnd) {
    wchar_t szFile[MAX_PATH] = { 0 };
    OPENFILENAMEW ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"Bootable Disk Images (*.iso;*.img;*.vhd)\0*.iso;*.img;*.vhd\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn)) {
        s_currentIso = IsoReader::AnalyzeIso(szFile);
        if (s_currentIso.isValid) {
            AppendLog(L"Image loaded: " + s_currentIso.fileName);
            AppendLog(L"Detected OS: " + s_currentIso.osName);
            SetWindowTextW(s_hEditLabel, s_currentIso.volumeLabel.c_str());

            if (s_currentIso.detectedType == IsoType::Windows11 || s_currentIso.detectedType == IsoType::Windows10) {
                ShowWindow(s_hBtnWin11Opts, SW_SHOW);
                AppendLog(L"Windows installation media recognized. Win11 bypasses enabled.");
            } else {
                ShowWindow(s_hBtnWin11Opts, SW_HIDE);
            }

            ShowWindow(s_hBtnChecksums, SW_SHOW);

            if (s_currentIso.requiresWimSplit)
                AppendLog(L"Split-WIM Engine active: install.wim > 4 GB will be split automatically for FAT32 compatibility.");

            if (!s_targetDevices.empty() && !s_isWriting)
                EnableWindow(s_hBtnStart, TRUE);
        } else {
            AppendLog(L"Failed to read ISO: " + s_currentIso.errorDescription);
            ShowWindow(s_hBtnWin11Opts, SW_HIDE);
            ShowWindow(s_hBtnChecksums, SW_HIDE);
        }
        InvalidateRect(hWnd, NULL, FALSE);
    }
}

void MainWindow::OnSchemeChanged(HWND hWnd) {
    int sel = (int)SendMessageW(s_hCmbScheme, CB_GETCURSEL, 0, 0);
    bool selectedGpt = (sel == 0);
    SendMessageW(s_hCmbTarget, CB_SETCURSEL, selectedGpt ? 0 : 1, 0);

    if (selectedGpt != s_sysInfo.isGpt) {
        s_overrideWarningText = L"Custom Override: " + std::wstring(selectedGpt ? L"GPT" : L"MBR") +
                                L" differs from host PC (" + s_sysInfo.partitionStyleName + L")";
        AppendLog(L"Notice: " + s_overrideWarningText);
    } else {
        s_overrideWarningText.clear();
    }
    InvalidateRect(hWnd, NULL, FALSE);
}

void MainWindow::OnToggleAdvanced(HWND hWnd) {
    s_advancedExpanded = !s_advancedExpanded;
    LayoutMainWindow(hWnd);
    InvalidateRect(hWnd, NULL, FALSE);
}

void MainWindow::OnToggleLog(HWND hWnd) {
    s_isLogExpanded = !s_isLogExpanded;
    LayoutMainWindow(hWnd);
    InvalidateRect(hWnd, NULL, FALSE);
}

void MainWindow::OnCopyLog(HWND hWnd) {
    int len = GetWindowTextLengthW(s_hEditLog);
    if (len > 0) {
        std::vector<wchar_t> buf(len + 1);
        GetWindowTextW(s_hEditLog, buf.data(), len + 1);
        if (OpenClipboard(hWnd)) {
            EmptyClipboard();
            HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(wchar_t));
            if (hGlob) {
                memcpy(GlobalLock(hGlob), buf.data(), (len + 1) * sizeof(wchar_t));
                GlobalUnlock(hGlob);
                SetClipboardData(CF_UNICODETEXT, hGlob);
            }
            CloseClipboard();
            AppendLog(L"Log copied to clipboard.");
        }
    }
}

// ─── Drawer Logic ──────────────────────────────────────────────────────────────
void MainWindow::ToggleDrawer(bool open) {
    s_isDrawerOpen = open;
    s_targetWindowWidth = open ? EXPANDED_WIDTH : WINDOW_W;

    if (open) {
        RECT rc;
        GetWindowRect(s_hWndMain, &rc);
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        if (rc.left + EXPANDED_WIDTH > screenW) {
            int newLeft = screenW - EXPANDED_WIDTH - 20;
            if (newLeft < 0) newLeft = 0;
            SetWindowPos(s_hWndMain, NULL, newLeft, rc.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        ShowWindow(s_hDrawer, SW_SHOW);
    }
    SetTimer(s_hWndMain, 9999, 10, NULL);
}

void MainWindow::LayoutDrawerContent() {
    if (!s_hDrawer) return;
    const int P = 18;
    const int DW = DRAWER_WIDTH - 2 * P;
    int y = 74;

    // 1. About Bootelware
    SetWindowPos(s_hDrawerSec1Btn, NULL, P, y, DW, 36, SWP_NOZORDER);
    y += 40;
    if (s_sec1Expanded) {
        SetWindowPos(s_hDrawerSec1Body, NULL, P, y, DW, 136, SWP_NOZORDER);
        ShowWindow(s_hDrawerSec1Body, SW_SHOW);
        y += 144;
    } else {
        ShowWindow(s_hDrawerSec1Body, SW_HIDE);
    }

    // 2. Disclaimer & Safety Notice (Separate card!)
    SetWindowPos(s_hDrawerDiscBtn, NULL, P, y, DW, 36, SWP_NOZORDER);
    y += 40;
    if (s_disclaimerExpanded) {
        SetWindowPos(s_hDrawerDiscBody, NULL, P, y, DW, 126, SWP_NOZORDER);
        ShowWindow(s_hDrawerDiscBody, SW_SHOW);
        y += 134;
    } else {
        ShowWindow(s_hDrawerDiscBody, SW_HIDE);
    }

    // 3. About Developer
    SetWindowPos(s_hDrawerSec2Btn, NULL, P, y, DW, 36, SWP_NOZORDER);
    y += 40;
    if (s_sec2Expanded) {
        SetWindowPos(s_hDrawerSec2Body, NULL, P, y, DW, 108, SWP_NOZORDER);
        ShowWindow(s_hDrawerSec2Body, SW_SHOW);
        y += 116;
    } else {
        ShowWindow(s_hDrawerSec2Body, SW_HIDE);
    }

    // 4. GitHub Action Button
    SetWindowPos(s_hDrawerGithubBtn, NULL, P, y, DW, 42, SWP_NOZORDER);
    ShowWindow(s_hDrawerGithubBtn, SW_SHOW);
}

LRESULT CALLBACK MainWindow::DrawerWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);

        FillRect(hdc, &rc, Theme::ThemeManager::hbrDrawer);

        // Left accent stripe
        HPEN hPen = CreatePen(PS_SOLID, 2, Theme::ACCENT_BLUE);
        HGDIOBJ oldPen = SelectObject(hdc, hPen);
        MoveToEx(hdc, 1, 0, NULL);
        LineTo(hdc, 1, rc.bottom);
        SelectObject(hdc, oldPen);
        DeleteObject(hPen);

        SetBkMode(hdc, TRANSPARENT);

        // Header Title
        SetTextColor(hdc, Theme::ACCENT_CYAN);
        SelectObject(hdc, Theme::ThemeManager::hFontLargeTitle);
        TextOutW(hdc, 20, 16, L"BOOTELWARE", 10);

        SetTextColor(hdc, Theme::TEXT_SECONDARY);
        SelectObject(hdc, Theme::ThemeManager::hFontSmall);
        TextOutW(hdc, 20, 42, L"Smart Boot Engine  ·  v1.0  ·  Information", 42);

        // Subtle divider
        HPEN hDiv = CreatePen(PS_SOLID, 1, Theme::BORDER_COLOR);
        oldPen = SelectObject(hdc, hDiv);
        MoveToEx(hdc, 20, 66, NULL);
        LineTo(hdc, rc.right - 20, 66);
        SelectObject(hdc, oldPen);
        DeleteObject(hDiv);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_DRAWITEM: {
        auto dis = (LPDRAWITEMSTRUCT)lParam;
        if (dis->CtlID == IDC_DRAWER_CLOSE) {
            Theme::ThemeManager::RenderModernButton(dis, L"\u2715", false, true, true);
            return TRUE;
        } else if (dis->CtlID == IDC_DRAWER_SEC1_BTN) {
            std::wstring t = s_sec1Expanded ? L"\u25BE  About Bootelware" : L"\u25B8  About Bootelware";
            HDC hdc = dis->hDC;
            RECT rc = dis->rcItem;
            Theme::ThemeManager::FillRoundedRect(hdc, rc, 6, Theme::BG_SECTION, Theme::BORDER_COLOR);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, Theme::ACCENT_CYAN);
            SelectObject(hdc, Theme::ThemeManager::hFontBold);
            RECT tr = { rc.left + 12, rc.top, rc.right, rc.bottom };
            DrawTextW(hdc, t.c_str(), -1, &tr, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
            return TRUE;
        } else if (dis->CtlID == IDC_DRAWER_SEC1_BODY) {
            HDC hdc = dis->hDC;
            RECT rc = dis->rcItem;
            Theme::ThemeManager::FillRoundedRect(hdc, rc, 8, Theme::BG_CARD, Theme::BORDER_COLOR);
            SetBkMode(hdc, TRANSPARENT);

            SetTextColor(hdc, Theme::TEXT_PRIMARY);
            SelectObject(hdc, Theme::ThemeManager::hFontRegular);
            RECT ra = { rc.left + 14, rc.top + 10, rc.right - 14, rc.bottom - 10 };
            DrawTextW(hdc,
                L"Bootelware is an intelligent, high-speed bootable USB creator designed for Windows & Linux.\r\n\r\n"
                L"\u2022 Smart System Detection: Auto-matches host partition style (GPT/MBR).\r\n"
                L"\u2022 Dual-Engine: ISO file extractor & raw DD byte writer.\r\n"
                L"\u2022 Windows 11 Bypasses: Removes TPM 2.0, Secure Boot & RAM checks.\r\n"
                L"\u2022 Large File Support: Automatically splits >4GB WIMs for FAT32.",
                -1, &ra, DT_WORDBREAK | DT_NOPREFIX);
            return TRUE;
        } else if (dis->CtlID == IDC_DRAWER_DISC_BTN) {
            std::wstring t = s_disclaimerExpanded ? L"\u25BE  Disclaimer & Safety Notice" : L"\u25B8  Disclaimer & Safety Notice";
            HDC hdc = dis->hDC;
            RECT rc = dis->rcItem;
            Theme::ThemeManager::FillRoundedRect(hdc, rc, 6, Theme::BG_SECTION, Theme::BORDER_COLOR);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, Theme::WARNING_TEXT);
            SelectObject(hdc, Theme::ThemeManager::hFontBold);
            RECT tr = { rc.left + 12, rc.top, rc.right, rc.bottom };
            DrawTextW(hdc, t.c_str(), -1, &tr, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
            return TRUE;
        } else if (dis->CtlID == IDC_DRAWER_DISC_BODY) {
            HDC hdc = dis->hDC;
            RECT rc = dis->rcItem;
            Theme::ThemeManager::FillRoundedRect(hdc, rc, 8, Theme::WARNING_BG, Theme::WARNING_BORDER);
            SetBkMode(hdc, TRANSPARENT);

            SetTextColor(hdc, Theme::WARNING_TEXT);
            SelectObject(hdc, Theme::ThemeManager::hFontBold);
            TextOutW(hdc, rc.left + 14, rc.top + 10, L"\u26A0  CRITICAL DATA WARNING", 24);

            SetTextColor(hdc, Theme::TEXT_PRIMARY);
            SelectObject(hdc, Theme::ThemeManager::hFontSmall);
            RECT rd = { rc.left + 14, rc.top + 32, rc.right - 14, rc.bottom - 10 };
            DrawTextW(hdc,
                L"Bootelware writes raw partitions and formats the selected target USB drive.\r\n\r\n"
                L"ALL EXISTING DATA ON THE TARGET MEDIA WILL BE PERMANENTLY ERASED.\r\n\r\n"
                L"The developer (Md Saim) and contributors assume no liability for data loss. Always verify your drive letter before flashing.",
                -1, &rd, DT_WORDBREAK | DT_NOPREFIX);
            return TRUE;
        } else if (dis->CtlID == IDC_DRAWER_SEC2_BTN) {
            std::wstring t = s_sec2Expanded ? L"\u25BE  About Developer" : L"\u25B8  About Developer";
            HDC hdc = dis->hDC;
            RECT rc = dis->rcItem;
            Theme::ThemeManager::FillRoundedRect(hdc, rc, 6, Theme::BG_SECTION, Theme::BORDER_COLOR);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, Theme::ACCENT_PURPLE);
            SelectObject(hdc, Theme::ThemeManager::hFontBold);
            RECT tr = { rc.left + 12, rc.top, rc.right, rc.bottom };
            DrawTextW(hdc, t.c_str(), -1, &tr, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
            return TRUE;
        } else if (dis->CtlID == IDC_DRAWER_SEC2_BODY) {
            HDC hdc = dis->hDC;
            RECT rc = dis->rcItem;
            Theme::ThemeManager::FillRoundedRect(hdc, rc, 8, Theme::BG_CARD, Theme::BORDER_COLOR);
            SetBkMode(hdc, TRANSPARENT);

            SetTextColor(hdc, Theme::TEXT_PRIMARY);
            SelectObject(hdc, Theme::ThemeManager::hFontTitle);
            TextOutW(hdc, rc.left + 14, rc.top + 10, L"Md Saim", 7);

            SetTextColor(hdc, Theme::ACCENT_PURPLE);
            SelectObject(hdc, Theme::ThemeManager::hFontSmall);
            TextOutW(hdc, rc.left + 14, rc.top + 32, L"Systems Programmer & Software Engineer", 39);

            SetTextColor(hdc, Theme::TEXT_SECONDARY);
            SelectObject(hdc, Theme::ThemeManager::hFontRegular);
            RECT rb = { rc.left + 14, rc.top + 52, rc.right - 14, rc.bottom - 8 };
            DrawTextW(hdc, L"Creator of Bootelware. Building high-performance native tools, OS utilities, and modern developer experiences.", -1, &rb, DT_WORDBREAK | DT_NOPREFIX);
            return TRUE;
        } else if (dis->CtlID == IDC_DRAWER_GITHUB_BTN) {
            HDC hdc = dis->hDC;
            RECT rc = dis->rcItem;
            bool pressed = (dis->itemState & ODS_SELECTED) != 0;
            Theme::ThemeManager::FillRoundedRect(hdc, rc, 8,
                pressed ? Theme::ACCENT_PRESSED : Theme::ACCENT_BLUE, Theme::ACCENT_HOVER);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            SelectObject(hdc, Theme::ThemeManager::hFontBold);
            RECT tr = rc;
            if (pressed) OffsetRect(&tr, 0, 1);
            DrawTextW(hdc, L"\u2605  Visit GitHub  \u2197  github.com/Md-Saim", -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            return TRUE;
        }
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == IDC_DRAWER_CLOSE) {
            ToggleDrawer(false);
        } else if (id == IDC_DRAWER_SEC1_BTN) {
            s_sec1Expanded = !s_sec1Expanded;
            InvalidateRect(s_hDrawerSec1Btn, NULL, TRUE);
            LayoutDrawerContent();
        } else if (id == IDC_DRAWER_DISC_BTN) {
            s_disclaimerExpanded = !s_disclaimerExpanded;
            InvalidateRect(s_hDrawerDiscBtn, NULL, TRUE);
            LayoutDrawerContent();
        } else if (id == IDC_DRAWER_SEC2_BTN) {
            s_sec2Expanded = !s_sec2Expanded;
            InvalidateRect(s_hDrawerSec2Btn, NULL, TRUE);
            LayoutDrawerContent();
        } else if (id == IDC_DRAWER_GITHUB_BTN) {
            ShellExecuteW(NULL, L"open", L"https://github.com/Md-Saim", NULL, NULL, SW_SHOWNORMAL);
        }
        break;
    }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void MainWindow::InitDrawer(HWND hWnd) {
    s_hDrawer = CreateWindowExW(0, L"BootelwareDrawerClass", NULL,
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        WINDOW_W, 0, DRAWER_WIDTH, WINDOW_H_BASE,
        hWnd, NULL, GetModuleHandleW(NULL), NULL);

    s_hDrawerClose = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        DRAWER_WIDTH - 44, 16, 28, 28, s_hDrawer, (HMENU)IDC_DRAWER_CLOSE, NULL, NULL);

    const int P = 18, DW = DRAWER_WIDTH - 2 * P;

    s_hDrawerSec1Btn = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        P, 74, DW, 36, s_hDrawer, (HMENU)IDC_DRAWER_SEC1_BTN, NULL, NULL);
    s_hDrawerSec1Body = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        P, 114, DW, 136, s_hDrawer, (HMENU)IDC_DRAWER_SEC1_BODY, NULL, NULL);

    s_hDrawerDiscBtn = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        P, 258, DW, 36, s_hDrawer, (HMENU)IDC_DRAWER_DISC_BTN, NULL, NULL);
    s_hDrawerDiscBody = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        P, 298, DW, 126, s_hDrawer, (HMENU)IDC_DRAWER_DISC_BODY, NULL, NULL);

    s_hDrawerSec2Btn = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        P, 432, DW, 36, s_hDrawer, (HMENU)IDC_DRAWER_SEC2_BTN, NULL, NULL);
    s_hDrawerSec2Body = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        P, 472, DW, 108, s_hDrawer, (HMENU)IDC_DRAWER_SEC2_BODY, NULL, NULL);

    s_hDrawerGithubBtn = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        P, 590, DW, 42, s_hDrawer, (HMENU)IDC_DRAWER_GITHUB_BTN, NULL, NULL);

    LayoutDrawerContent();
}

// ─── Main Window Dynamic Layout ────────────────────────────────────────────────
void MainWindow::LayoutMainWindow(HWND hWnd) {
    if (!hWnd || !IsWindow(hWnd)) return;

    // Header buttons
    SetWindowPos(s_hBtnMenu, NULL, MARGIN + INNER_W - 110, 16, 110, 34, SWP_NOZORDER);
    SetWindowPos(s_hBtnRefreshSys, NULL, MARGIN + INNER_W - 100, 78, 90, 32, SWP_NOZORDER);

    // Stage 1 (Source Image) Buttons
    int s1BtnW = (STAGE_W - 28) / 2;
    SetWindowPos(s_hBtnSelectIso, NULL, MARGIN + 14, 218, s1BtnW, 32, SWP_NOZORDER);
    SetWindowPos(s_hBtnDownloadIso, NULL, MARGIN + 14 + s1BtnW + 6, 218, s1BtnW, 32, SWP_NOZORDER);

    SetWindowPos(s_hBtnWin11Opts, NULL, MARGIN + 14, 256, s1BtnW, 28, SWP_NOZORDER);
    SetWindowPos(s_hBtnChecksums, NULL, MARGIN + 14 + s1BtnW + 6, 256, s1BtnW, 28, SWP_NOZORDER);

    // Stage 2 (Target USB) Controls
    int s2X = MARGIN + STAGE_W + 14;
    SetWindowPos(s_hCmbDevice, NULL, s2X + 14, 182, STAGE_W - 28, 200, SWP_NOZORDER);
    SetWindowPos(s_hBtnRefreshDev, NULL, s2X + 14, 246, STAGE_W - 28, 34, SWP_NOZORDER);

    // Stage 3 (Boot & Format Profile)
    int profY = 306;
    SetWindowPos(s_hEditLabel, NULL, MARGIN + 110, profY + 44, 156, 24, SWP_NOZORDER);
    SetWindowPos(s_hBtnAdvToggle, NULL, MARGIN + INNER_W - 164, profY + 42, 154, 30, SWP_NOZORDER);

    // Advanced controls
    int advY = profY + 82;
    int colW = (INNER_W - 28 - 18) / 4;
    SetWindowPos(s_hCmbScheme, NULL, MARGIN + 14, advY + 16, colW, 150, SWP_NOZORDER);
    SetWindowPos(s_hCmbTarget, NULL, MARGIN + 14 + colW + 6, advY + 16, colW, 150, SWP_NOZORDER);
    SetWindowPos(s_hCmbFs, NULL, MARGIN + 14 + 2 * (colW + 6), advY + 16, colW, 150, SWP_NOZORDER);
    SetWindowPos(s_hCmbCluster, NULL, MARGIN + 14 + 3 * (colW + 6), advY + 16, colW, 150, SWP_NOZORDER);

    int advY2 = advY + 52;
    SetWindowPos(s_hChkBadBlocks, NULL, MARGIN + 14, advY2, 220, 22, SWP_NOZORDER);

    int showAdv = s_advancedExpanded ? SW_SHOW : SW_HIDE;
    ShowWindow(s_hCmbScheme, showAdv);
    ShowWindow(s_hCmbTarget, showAdv);
    ShowWindow(s_hCmbFs, showAdv);
    ShowWindow(s_hCmbCluster, showAdv);
    ShowWindow(s_hChkBadBlocks, showAdv);

    // Stage 4: Hero Action Button & Progress
    int profileCardH = s_advancedExpanded ? 180 : 86;
    int actionY = profY + profileCardH + 16;

    SetWindowPos(s_hBtnStart, NULL, MARGIN, actionY, INNER_W, 46, SWP_NOZORDER);

    int progY = actionY + 54;
    SetWindowPos(s_hLblStatus, NULL, MARGIN, progY, INNER_W - 160, 18, SWP_NOZORDER);
    SetWindowPos(s_hLblSpeed, NULL, MARGIN + INNER_W - 160, progY, 160, 18, SWP_NOZORDER);

    SetWindowPos(s_hProgressBar, NULL, MARGIN, progY + 22, INNER_W, 14, SWP_NOZORDER);

    // Stage 5: Log Console
    int logControlsY = progY + 44;
    SetWindowPos(s_hBtnToggleLog, NULL, MARGIN, logControlsY, 94, 28, SWP_NOZORDER);
    SetWindowPos(s_hBtnCopyLog, NULL, MARGIN + 102, logControlsY, 84, 28, SWP_NOZORDER);

    int logEditH = s_isLogExpanded ? 110 : 0;
    SetWindowPos(s_hEditLog, NULL, MARGIN, logControlsY + 34, INNER_W, logEditH, SWP_NOZORDER);
    ShowWindow(s_hEditLog, s_isLogExpanded ? SW_SHOW : SW_HIDE);
}

// ─── Main Window Initialization ────────────────────────────────────────────────
void MainWindow::OnInit(HWND hWnd) {
    Theme::ThemeManager::InitGDI();
    Theme::ThemeManager::EnableDarkMode(hWnd);

    // Header Controls
    s_hBtnMenu = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_MENU, NULL, NULL);

    s_hBtnRefreshSys = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_REFRESH_SYS, NULL, NULL);

    // Stage 1 (Source Image) Controls
    s_hBtnSelectIso = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_SELECT_ISO, NULL, NULL);

    s_hBtnDownloadIso = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_DOWNLOAD_ISO, NULL, NULL);

    s_hBtnWin11Opts = CreateWindowW(L"BUTTON", L"", WS_CHILD | BS_OWNERDRAW,
        0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_WIN11_OPTS, NULL, NULL);

    s_hBtnChecksums = CreateWindowW(L"BUTTON", L"", WS_CHILD | BS_OWNERDRAW,
        0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_CHECKSUMS, NULL, NULL);

    // Stage 2 (Target USB) Controls
    s_hCmbDevice = CreateWindowExW(0, L"COMBOBOX", NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 0, 0, hWnd, (HMENU)IDC_CMB_DEVICE, NULL, NULL);

    s_hBtnRefreshDev = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_REFRESH_DEV, NULL, NULL);

    // Stage 3 (Boot & Format Profile) Controls
    s_hEditLabel = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"BOOTELWARE",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, 0, 0, 0, hWnd, (HMENU)IDC_EDIT_LABEL, NULL, NULL);

    s_hBtnAdvToggle = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_ADV_TOGGLE, NULL, NULL);

    // Advanced Dropdowns
    s_hCmbScheme = CreateWindowExW(0, L"COMBOBOX", NULL,
        WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 0, 0, hWnd, (HMENU)IDC_CMB_SCHEME, NULL, NULL);
    SendMessageW(s_hCmbScheme, CB_ADDSTRING, 0, (LPARAM)L"GPT");
    SendMessageW(s_hCmbScheme, CB_ADDSTRING, 0, (LPARAM)L"MBR");
    SendMessageW(s_hCmbScheme, CB_SETCURSEL, 0, 0);

    s_hCmbTarget = CreateWindowExW(0, L"COMBOBOX", NULL,
        WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 0, 0, hWnd, (HMENU)IDC_CMB_TARGET, NULL, NULL);
    SendMessageW(s_hCmbTarget, CB_ADDSTRING, 0, (LPARAM)L"UEFI (non-CSM)");
    SendMessageW(s_hCmbTarget, CB_ADDSTRING, 0, (LPARAM)L"BIOS / UEFI-CSM");
    SendMessageW(s_hCmbTarget, CB_SETCURSEL, 0, 0);

    s_hCmbFs = CreateWindowExW(0, L"COMBOBOX", NULL,
        WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 0, 0, hWnd, (HMENU)IDC_CMB_FS, NULL, NULL);
    SendMessageW(s_hCmbFs, CB_ADDSTRING, 0, (LPARAM)L"Large FAT32");
    SendMessageW(s_hCmbFs, CB_ADDSTRING, 0, (LPARAM)L"NTFS");
    SendMessageW(s_hCmbFs, CB_ADDSTRING, 0, (LPARAM)L"exFAT");
    SendMessageW(s_hCmbFs, CB_SETCURSEL, 0, 0);

    s_hCmbCluster = CreateWindowExW(0, L"COMBOBOX", NULL,
        WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 0, 0, hWnd, (HMENU)IDC_CMB_CLUSTER, NULL, NULL);
    SendMessageW(s_hCmbCluster, CB_ADDSTRING, 0, (LPARAM)L"4096 (Default)");
    SendMessageW(s_hCmbCluster, CB_ADDSTRING, 0, (LPARAM)L"8192 bytes");
    SendMessageW(s_hCmbCluster, CB_ADDSTRING, 0, (LPARAM)L"16 KB");
    SendMessageW(s_hCmbCluster, CB_ADDSTRING, 0, (LPARAM)L"32 KB");
    SendMessageW(s_hCmbCluster, CB_SETCURSEL, 0, 0);

    s_hChkBadBlocks = CreateWindowW(L"BUTTON", L"Check drive for bad blocks",
        WS_CHILD | BS_AUTOCHECKBOX,
        0, 0, 0, 0, hWnd, (HMENU)IDC_CHK_BADBLOCKS, NULL, NULL);

    // Stage 4: Start Button & Progress
    s_hBtnStart = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_START, NULL, NULL);
    EnableWindow(s_hBtnStart, FALSE);

    s_hLblStatus = CreateWindowW(L"STATIC", L"Ready to create bootable media", WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, hWnd, (HMENU)IDC_LBL_STATUS, NULL, NULL);

    s_hLblSpeed = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_RIGHT,
        0, 0, 0, 0, hWnd, (HMENU)IDC_LBL_SPEED, NULL, NULL);

    s_hProgressBar = CreateWindowExW(0, PROGRESS_CLASSW, NULL, WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        0, 0, 0, 0, hWnd, (HMENU)IDC_PROGRESS, NULL, NULL);
    SendMessageW(s_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageW(s_hProgressBar, PBM_SETPOS, 0, 0);
    SendMessageW(s_hProgressBar, PBM_SETBARCOLOR, 0, (LPARAM)Theme::ACCENT_CYAN);
    SendMessageW(s_hProgressBar, PBM_SETBKCOLOR, 0, (LPARAM)Theme::PROGRESS_BG);

    // Stage 5: Log Controls
    s_hBtnToggleLog = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_TOGGLE_LOG, NULL, NULL);

    s_hBtnCopyLog = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_COPY_LOG, NULL, NULL);

    s_hEditLog = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
        0, 0, 0, 0, hWnd, (HMENU)IDC_EDIT_LOG, NULL, NULL);

    // Apply Fonts
    EnumChildWindows(hWnd, [](HWND hChild, LPARAM) -> BOOL {
        if (hChild == s_hEditLog)
            SendMessageW(hChild, WM_SETFONT, (WPARAM)Theme::ThemeManager::hFontMonoSmall, TRUE);
        else
            SendMessageW(hChild, WM_SETFONT, (WPARAM)Theme::ThemeManager::hFontRegular, TRUE);
        return TRUE;
    }, 0);

    // Initialize Drawer
    InitDrawer(hWnd);

    // Arrange all controls
    LayoutMainWindow(hWnd);

    AppendLog(L"Bootelware v1.0 engine loaded.");
    RefreshSystemDetection(hWnd);
    RefreshDeviceList(hWnd);
}

// ─── Write Execution Handler ───────────────────────────────────────────────────
void MainWindow::OnStartClicked(HWND hWnd) {
    if (s_targetDevices.empty()) return;
    int devIdx = (int)SendMessageW(s_hCmbDevice, CB_GETCURSEL, 0, 0);
    if (devIdx < 0 || devIdx >= (int)s_targetDevices.size()) return;
    const auto& targetDev = s_targetDevices[devIdx];

    std::wstring warnMsg = L"CRITICAL WARNING:\n\nAll existing data on:\n" + targetDev.displayName +
                           L"\n\nWILL BE PERMANENTLY DESTROYED!\n\nDo you want to continue?";
    if (MessageBoxW(hWnd, warnMsg.c_str(), L"Bootelware - Erase Confirmation",
                    MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2) != IDOK) {
        AppendLog(L"Operation cancelled by user.");
        return;
    }

    WriteJobConfig cfg;
    cfg.physicalDriveIndex = targetDev.physicalDriveIndex;
    cfg.assignedDriveLetter = targetDev.assignedDriveLetters;
    cfg.isoPath = s_currentIso.filePath;
    cfg.isoMeta = s_currentIso;

    int ss = (int)SendMessageW(s_hCmbScheme, CB_GETCURSEL, 0, 0);
    cfg.partitionScheme = (ss == 0) ? L"GPT" : L"MBR";

    int ts = (int)SendMessageW(s_hCmbTarget, CB_GETCURSEL, 0, 0);
    cfg.targetSystem = (ts == 0) ? L"UEFI (non-CSM)" : L"BIOS or UEFI-CSM";

    int fs = (int)SendMessageW(s_hCmbFs, CB_GETCURSEL, 0, 0);
    if (fs == 1) cfg.fileSystem = L"NTFS";
    else if (fs == 2) cfg.fileSystem = L"exFAT";
    else cfg.fileSystem = L"FAT32";

    cfg.clusterSize = 4096;
    wchar_t lblBuf[64] = { 0 };
    GetWindowTextW(s_hEditLabel, lblBuf, 64);
    cfg.volumeLabel = lblBuf[0] ? lblBuf : L"BOOTELWARE";

    cfg.checkBadBlocks = (SendMessageW(s_hChkBadBlocks, BM_GETCHECK, 0, 0) == BST_CHECKED);
    cfg.badBlockPasses = 1;
    cfg.enableWin11Bypasses = (s_currentIso.detectedType == IsoType::Windows11 || s_currentIso.detectedType == IsoType::Windows10);
    cfg.win11Options = s_win11Options;
    cfg.ddMode = false;

    s_isWriting = true;
    s_cancelWrite.store(false);
    EnableWindow(s_hBtnStart, FALSE);
    InvalidateRect(s_hBtnStart, NULL, TRUE);

    std::thread([cfg, hWnd]() {
        WriteProgressCallback progCb = [](int pct, double speed, const std::wstring& desc) {
            UpdateProgress(pct, speed, desc);
        };
        WriteLogCallback logCb = [](const std::wstring& line) {
            AppendLog(line);
        };
        std::wstring errMsg;
        bool success = DiskWriter::ExecuteWriteJob(cfg, progCb, logCb, s_cancelWrite, errMsg);
        PostMessageW(hWnd, WM_USER + 201, success ? 1 : 0, 0);
    }).detach();
}

// ─── Main Window Message Procedure ─────────────────────────────────────────────
LRESULT CALLBACK MainWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        s_hWndMain = hWnd;
        OnInit(hWnd);
        break;

    case WM_TIMER:
        if (wParam == 9999) {
            int step = 45;
            if (s_isDrawerOpen) {
                s_currentWindowWidth += step;
                if (s_currentWindowWidth >= s_targetWindowWidth) {
                    s_currentWindowWidth = s_targetWindowWidth;
                    KillTimer(hWnd, 9999);
                }
            } else {
                s_currentWindowWidth -= step;
                if (s_currentWindowWidth <= s_targetWindowWidth) {
                    s_currentWindowWidth = s_targetWindowWidth;
                    KillTimer(hWnd, 9999);
                    ShowWindow(s_hDrawer, SW_HIDE);
                }
            }
            RECT rc;
            GetWindowRect(hWnd, &rc);
            SetWindowPos(hWnd, NULL, rc.left, rc.top, s_currentWindowWidth, rc.bottom - rc.top, SWP_NOZORDER);
        }
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdcScreen = BeginPaint(hWnd, &ps);
        RECT rcClient;
        GetClientRect(hWnd, &rcClient);

        // Double-buffered rendering to eliminate all tearing & UI leaking
        HDC hdc = CreateCompatibleDC(hdcScreen);
        HBITMAP hbm = CreateCompatibleBitmap(hdcScreen, rcClient.right, rcClient.bottom);
        HGDIOBJ oldBm = SelectObject(hdc, hbm);

        // 1. Fill background
        FillRect(hdc, &rcClient, Theme::ThemeManager::hbrBackground);

        // 2. Draw Header Brand & Icon
        HICON hIcon = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 36, 36, LR_DEFAULTCOLOR);
        if (hIcon) {
            DrawIconEx(hdc, MARGIN, 15, hIcon, 36, 36, 0, NULL, DI_NORMAL);
            DestroyIcon(hIcon);
        }
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, Theme::TEXT_PRIMARY);
        SelectObject(hdc, Theme::ThemeManager::hFontHero);
        TextOutW(hdc, MARGIN + 46, 12, L"BOOTELWARE", 10);

        SetTextColor(hdc, Theme::ACCENT_CYAN);
        SelectObject(hdc, Theme::ThemeManager::hFontSmall);
        TextOutW(hdc, MARGIN + 48, 42, L"SMART BOOT ENGINE  ·  v1.0  ·  PORTABLE", 39);

        // 3. Smart System Auto-Match Banner
        RECT bannerRc = { MARGIN, 66, MARGIN + INNER_W, 132 };
        bool isOverridden = !s_overrideWarningText.empty();
        Theme::ThemeManager::FillRoundedRect(hdc, bannerRc, 8,
            isOverridden ? Theme::WARNING_BG : Theme::BANNER_BG,
            isOverridden ? Theme::WARNING_BORDER : Theme::BANNER_BORDER);

        SetTextColor(hdc, isOverridden ? Theme::WARNING_TEXT : Theme::ACCENT_GREEN);
        SelectObject(hdc, Theme::ThemeManager::hFontBold);
        TextOutW(hdc, bannerRc.left + 14, bannerRc.top + 10,
            isOverridden ? L"\u26A0  SYSTEM OVERRIDE DETECTED" : L"\u25CF  SMART SYSTEM AUTO-MATCH ACTIVE",
            isOverridden ? 28 : 31);

        SetTextColor(hdc, Theme::TEXT_PRIMARY);
        SelectObject(hdc, Theme::ThemeManager::hFontRegular);
        std::wstring sysDesc = L"Host: Drive " + s_sysInfo.systemDriveLetter + L" (" + s_sysInfo.diskModel + L")  \u00B7  " +
                               s_sysInfo.partitionStyleName + L"  \u00B7  " + s_sysInfo.firmwareBootType;
        TextOutW(hdc, bannerRc.left + 14, bannerRc.top + 30, sysDesc.c_str(), (int)sysDesc.size());

        SetTextColor(hdc, isOverridden ? Theme::WARNING_TEXT : Theme::TEXT_SECONDARY);
        SelectObject(hdc, Theme::ThemeManager::hFontSmall);
        std::wstring matchDesc = isOverridden ?
            (L"Warning: USB target differs from this computer (" + s_sysInfo.partitionStyleName + L").") :
            L"\u2728 Optimal partition scheme and filesystem automatically pre-selected for this PC.";
        TextOutW(hdc, bannerRc.left + 14, bannerRc.top + 48, matchDesc.c_str(), (int)matchDesc.size());

        // 4. Stage 1 Card (Source Image)
        RECT srcRc = { MARGIN, 140, MARGIN + STAGE_W, 296 };
        Theme::ThemeManager::DrawCard(hdc, srcRc, L"1. SOURCE IMAGE",
            s_currentIso.isValid ? L"LOADED" : L"ISO / IMG", Theme::ACCENT_CYAN);

        if (s_currentIso.isValid) {
            SetTextColor(hdc, Theme::TEXT_PRIMARY);
            SelectObject(hdc, Theme::ThemeManager::hFontBold);
            RECT osRc = { srcRc.left + 14, srcRc.top + 38, srcRc.right - 14, srcRc.top + 56 };
            DrawTextW(hdc, s_currentIso.osName.c_str(), -1, &osRc, DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

            SetTextColor(hdc, Theme::TEXT_SECONDARY);
            SelectObject(hdc, Theme::ThemeManager::hFontSmall);
            std::wstring isoDetails = s_currentIso.fileName + L"  (" + std::to_wstring(s_currentIso.fileSizeBytes / (1024 * 1024)) + L" MB)";
            RECT detRc = { srcRc.left + 14, srcRc.top + 56, srcRc.right - 14, srcRc.top + 74 };
            DrawTextW(hdc, isoDetails.c_str(), -1, &detRc, DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        } else {
            SetTextColor(hdc, Theme::TEXT_MUTED);
            SelectObject(hdc, Theme::ThemeManager::hFontRegular);
            RECT emptyRc = { srcRc.left + 14, srcRc.top + 42, srcRc.right - 14, srcRc.top + 72 };
            DrawTextW(hdc, L"Click 'Browse ISO' or choose a preset to load a bootable disk image.", -1, &emptyRc, DT_WORDBREAK | DT_NOPREFIX);
        }

        // 5. Stage 2 Card (Target USB Drive)
        RECT tgtRc = { MARGIN + STAGE_W + 14, 140, MARGIN + INNER_W, 296 };
        Theme::ThemeManager::DrawCard(hdc, tgtRc, L"2. TARGET USB DRIVE",
            s_targetDevices.empty() ? L"NO MEDIA" : L"READY", Theme::ACCENT_ORANGE);

        SetTextColor(hdc, s_targetDevices.empty() ? Theme::WARNING_TEXT : Theme::ACCENT_GREEN);
        SelectObject(hdc, Theme::ThemeManager::hFontSmall);
        std::wstring tgtStatus = s_targetDevices.empty() ?
            L"\u26A0 No USB flash drive detected. Insert a drive." :
            L"\U0001F6E1 Safe to write \u2014 Host drive " + s_sysInfo.systemDriveLetter + L" is protected.";
        TextOutW(hdc, tgtRc.left + 14, tgtRc.top + 76, tgtStatus.c_str(), (int)tgtStatus.size());

        // 6. Stage 3 Card (Boot & Format Profile)
        int profileCardH = s_advancedExpanded ? 180 : 86;
        RECT profRc = { MARGIN, 306, MARGIN + INNER_W, 306 + profileCardH };
        Theme::ThemeManager::DrawCard(hdc, profRc, L"3. BOOT & FORMAT PROFILE",
            s_advancedExpanded ? L"MANUAL OVERRIDE" : L"OPTIMIZED", Theme::ACCENT_PURPLE);

        // Volume Label Text
        SetTextColor(hdc, Theme::TEXT_PRIMARY);
        SelectObject(hdc, Theme::ThemeManager::hFontRegular);
        TextOutW(hdc, profRc.left + 14, profRc.top + 48, L"Volume Label:", 13);

        // In normal mode, draw summary pills
        if (!s_advancedExpanded) {
            int pillX = profRc.left + 280;
            int pillY = profRc.top + 44;

            std::wstring schemePill = L"Scheme: " + std::wstring(SendMessageW(s_hCmbScheme, CB_GETCURSEL, 0, 0) == 0 ? L"GPT" : L"MBR");
            Theme::ThemeManager::DrawBadge(hdc, pillX, pillY, schemePill.c_str(), Theme::BG_SURFACE, Theme::BORDER_COLOR, Theme::TEXT_PRIMARY);
            pillX += 88;

            std::wstring targetPill = L"Target: " + std::wstring(SendMessageW(s_hCmbTarget, CB_GETCURSEL, 0, 0) == 0 ? L"UEFI" : L"BIOS");
            Theme::ThemeManager::DrawBadge(hdc, pillX, pillY, targetPill.c_str(), Theme::BG_SURFACE, Theme::BORDER_COLOR, Theme::TEXT_PRIMARY);
        } else {
            // Advanced column labels
            int advY = profRc.top + 80;
            int colW = (INNER_W - 28 - 18) / 4;
            SetTextColor(hdc, Theme::TEXT_LABEL);
            SelectObject(hdc, Theme::ThemeManager::hFontSmall);
            TextOutW(hdc, MARGIN + 14, advY, L"Partition Scheme", 16);
            TextOutW(hdc, MARGIN + 14 + colW + 6, advY, L"Target System", 13);
            TextOutW(hdc, MARGIN + 14 + 2 * (colW + 6), advY, L"File System", 11);
            TextOutW(hdc, MARGIN + 14 + 3 * (colW + 6), advY, L"Cluster Size", 12);
        }

        // Blit backbuffer to screen
        BitBlt(hdcScreen, 0, 0, rcClient.right, rcClient.bottom, hdc, 0, 0, SRCCOPY);

        SelectObject(hdc, oldBm);
        DeleteObject(hbm);
        DeleteDC(hdc);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        if (id == IDC_BTN_MENU) {
            ToggleDrawer(!s_isDrawerOpen);
        } else if (id == IDC_BTN_REFRESH_SYS) {
            RefreshSystemDetection(hWnd);
        } else if (id == IDC_BTN_SELECT_ISO) {
            OnSelectIso(hWnd);
        } else if (id == IDC_BTN_DOWNLOAD_ISO) {
            Dialogs::ShowDownloadDialog(hWnd);
        } else if (id == IDC_BTN_WIN11_OPTS) {
            Dialogs::ShowWin11OptionsDialog(hWnd, s_win11Options);
        } else if (id == IDC_BTN_CHECKSUMS) {
            if (!s_currentIso.filePath.empty())
                Dialogs::ShowChecksumDialog(hWnd, s_currentIso.filePath);
            else
                MessageBoxW(hWnd, L"Please select an ISO image first.", L"Bootelware", MB_OK | MB_ICONINFORMATION);
        } else if (id == IDC_BTN_REFRESH_DEV) {
            RefreshDeviceList(hWnd);
        } else if (id == IDC_BTN_ADV_TOGGLE) {
            OnToggleAdvanced(hWnd);
        } else if (id == IDC_CMB_SCHEME && code == CBN_SELCHANGE) {
            OnSchemeChanged(hWnd);
        } else if (id == IDC_BTN_TOGGLE_LOG) {
            OnToggleLog(hWnd);
        } else if (id == IDC_BTN_COPY_LOG) {
            OnCopyLog(hWnd);
        } else if (id == IDC_BTN_START) {
            OnStartClicked(hWnd);
        }
        break;
    }

    case WM_USER + 201: {
        s_isWriting = false;
        EnableWindow(s_hBtnStart, TRUE);
        InvalidateRect(s_hBtnStart, NULL, TRUE);

        if (wParam == 1) {
            MessageBoxW(hWnd, L"Bootable USB media created successfully!\n\nYour drive is ready to boot.",
                        L"Bootelware - Complete", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBoxW(hWnd, L"An error occurred while creating bootable media.\n\nPlease check the Activity Log for details.",
                        L"Bootelware - Failed", MB_OK | MB_ICONERROR);
        }
        break;
    }

    case WM_DRAWITEM: {
        auto dis = (LPDRAWITEMSTRUCT)lParam;

        if (dis->CtlID == IDC_BTN_MENU) {
            HDC hdc = dis->hDC;
            RECT rc = dis->rcItem;
            bool pressed = (dis->itemState & ODS_SELECTED) != 0;
            Theme::ThemeManager::FillRoundedRect(hdc, rc, 6,
                pressed ? Theme::BTN_PRESSED : Theme::BTN_NORMAL,
                s_isDrawerOpen ? Theme::ACCENT_CYAN : Theme::BORDER_COLOR);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, s_isDrawerOpen ? Theme::ACCENT_CYAN : Theme::TEXT_PRIMARY);
            SelectObject(hdc, Theme::ThemeManager::hFontBold);
            RECT tr = rc;
            if (pressed) OffsetRect(&tr, 0, 1);
            DrawTextW(hdc, L"\u2630  Menu & Info", -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            return TRUE;
        } else if (dis->CtlID == IDC_BTN_REFRESH_SYS) {
            Theme::ThemeManager::RenderModernButton(dis, L"\u27F3  Re-Detect", false, false, true);
            return TRUE;
        } else if (dis->CtlID == IDC_BTN_SELECT_ISO) {
            Theme::ThemeManager::RenderModernButton(dis, L"\U0001F4C1  Browse ISO...", true, false, false);
            return TRUE;
        } else if (dis->CtlID == IDC_BTN_DOWNLOAD_ISO) {
            Theme::ThemeManager::RenderModernButton(dis, L"\u2B07  Presets", false, false, false);
            return TRUE;
        } else if (dis->CtlID == IDC_BTN_WIN11_OPTS) {
            Theme::ThemeManager::RenderModernButton(dis, L"\u2699  Win11 Bypasses", false, false, true);
            return TRUE;
        } else if (dis->CtlID == IDC_BTN_CHECKSUMS) {
            Theme::ThemeManager::RenderModernButton(dis, L"#  Verify SHA256", false, false, true);
            return TRUE;
        } else if (dis->CtlID == IDC_BTN_REFRESH_DEV) {
            Theme::ThemeManager::RenderModernButton(dis, L"\u27F3  Refresh USB Drives", false, false, false);
            return TRUE;
        } else if (dis->CtlID == IDC_BTN_ADV_TOGGLE) {
            std::wstring advText = s_advancedExpanded ? L"\u25B2  Hide Advanced" : L"\u26A1  Advanced Settings \u25BE";
            Theme::ThemeManager::RenderModernButton(dis, advText, false, false, true);
            return TRUE;
        } else if (dis->CtlID == IDC_BTN_TOGGLE_LOG) {
            std::wstring logText = s_isLogExpanded ? L"\u25BC  Hide Log" : L"\u25B2  Show Log";
            Theme::ThemeManager::RenderModernButton(dis, logText, false, false, true);
            return TRUE;
        } else if (dis->CtlID == IDC_BTN_COPY_LOG) {
            Theme::ThemeManager::RenderModernButton(dis, L"\U0001F4CB  Copy Log", false, false, true);
            return TRUE;
        } else if (dis->CtlID == IDC_BTN_START) {
            bool disabled = (dis->itemState & ODS_DISABLED) != 0;
            std::wstring btnLabel = s_isWriting ? L"\u23F3  Writing Media..." : L"\U0001F680  CREATE BOOTABLE USB";
            Theme::ThemeManager::RenderHeroButton(dis, btnLabel, disabled);
            return TRUE;
        }
        break;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        HWND hCtl = (HWND)lParam;
        SetBkMode(hdc, TRANSPARENT);

        if (hCtl == s_hChkBadBlocks) {
            SetTextColor(hdc, Theme::TEXT_PRIMARY);
            return (LRESULT)Theme::ThemeManager::hbrCard;
        } else if (hCtl == s_hLblStatus) {
            SetTextColor(hdc, Theme::TEXT_SECONDARY);
            return (LRESULT)Theme::ThemeManager::hbrBackground;
        } else if (hCtl == s_hLblSpeed) {
            SetTextColor(hdc, Theme::ACCENT_CYAN);
            return (LRESULT)Theme::ThemeManager::hbrBackground;
        }
        SetTextColor(hdc, Theme::TEXT_LABEL);
        return (LRESULT)Theme::ThemeManager::hbrBackground;
    }

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, Theme::TEXT_PRIMARY);
        SetBkColor(hdc, Theme::BG_INPUT);
        return (LRESULT)Theme::ThemeManager::hbrInput;
    }

    case WM_ERASEBKGND:
        return 1; // Handled in double-buffered WM_PAINT

    case WM_DEVICECHANGE:
        RefreshDeviceList(hWnd);
        break;

    case WM_DESTROY:
        Theme::ThemeManager::CleanupGDI();
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

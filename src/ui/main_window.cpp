#include "main_window.h"
#include "dark_theme.h"
#include "dialogs.h"
#include "../core/disk_writer.h"
#include "../core/device_manager.h"
#include "../core/system_detector.h"
#include "../core/iso_reader.h"
#include "../../resources/resource.h"
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <thread>
#include <sstream>
#include <iomanip>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

// ─── App Views matching PDF ───────────────────────────────────────────────────
enum AppView {
    VIEW_SOURCE = 0,     // Page 1: Let's make a bootable USB!
    VIEW_DRIVE  = 1,     // Page 2: Which USB should we use?
    VIEW_TUNE   = 2,     // Page 3: Pick your Windows extras
    VIEW_WRITE  = 3,     // Page 4: Hang tight, working on it!
    VIEW_DONE   = 4,     // Page 5: All done! Your USB is ready
    VIEW_FORMAT = 5,     // Format USB Tool (Rufus Mode, Large FAT32)
    VIEW_ABOUT_APP = 6,  // Page 7: About Bootelware
    VIEW_ABOUT_DEV = 7   // Page 8: About the developer
};

// ─── Interactive Hit Area ─────────────────────────────────────────────────────
enum ButtonAction {
    ACT_NONE = 0,
    ACT_LOGO,
    ACT_STEP_1,
    ACT_STEP_2,
    ACT_STEP_3,
    ACT_STEP_4,
    ACT_MENU_BTN,

    // Step 1: Source
    ACT_BROWSE_ISO,
    ACT_CHANGE_ISO,
    ACT_CONTINUE_ISO,
    ACT_GET_WIN11,
    ACT_GET_WIN10,
    ACT_GET_UBUNTU,
    ACT_GET_OTHER,

    // Step 2: Drive
    ACT_SELECT_DRIVE_CARD,
    ACT_RESCAN_DRIVES,
    ACT_GOTO_FORMAT_USB,
    ACT_DRIVE_BACK,
    ACT_DRIVE_NEXT,

    // Step 3: Tune
    ACT_OPT_OLDER_PCS,
    ACT_OPT_NO_MS_ACCOUNT,
    ACT_OPT_LOCAL_USER,
    ACT_OPT_SKIP_ENCRYPTION,
    ACT_OPT_PAUSE_TRACKING,
    ACT_OPT_USB_NAME,
    ACT_TUNE_SCHEME_GPT,
    ACT_TUNE_SCHEME_MBR,
    ACT_TUNE_FIX_MISMATCH,
    ACT_TUNE_BACK,
    ACT_CREATE_USB,

    // Step 4: Write
    ACT_TOGGLE_DETAILS,
    ACT_CANCEL_WRITE,

    // Step 5: Done
    ACT_MAKE_ANOTHER,
    ACT_EJECT_USB,

    // Menu Popup
    ACT_MENU_ABOUT_APP,
    ACT_MENU_ABOUT_DEV,
    ACT_MENU_FORMAT_TOOL,
    ACT_MENU_CHECKSUMS,
    ACT_MENU_CLOSE,

    // About App
    ACT_ABOUT_APP_BACK,

    // About Dev
    ACT_ABOUT_DEV_BACK,
    ACT_ABOUT_DEV_THANKS,
    ACT_DEV_LINK_WEB,
    ACT_DEV_LINK_GH,
    ACT_DEV_LINK_EMAIL,

    // Format Tool
    ACT_FORMAT_FS_FAT32,
    ACT_FORMAT_FS_NTFS,
    ACT_FORMAT_FS_EXFAT,
    ACT_FORMAT_CHANGE_DRIVE,
    ACT_FORMAT_EXECUTE,
    ACT_FORMAT_BACK
};

struct HitArea {
    RectF rect;
    ButtonAction action;
    int indexData = 0;
};

// ─── Application State ────────────────────────────────────────────────────────
static HWND s_hWnd = NULL;
static AppView s_currentView = VIEW_SOURCE;
static AppView s_previousView = VIEW_SOURCE;
static bool s_showMenu = false;
static ButtonAction s_hoveredAction = ACT_NONE;
static ButtonAction s_pressedAction = ACT_NONE;
static int s_animTick = 0;
static std::vector<HitArea> s_hitAreas;

// System Detection & Drive Data
static SystemDriveInfo s_hostInfo;
static std::vector<TargetDeviceInfo> s_drives;
static int s_selectedDriveIndex = -1;

// ISO Data
static std::wstring s_isoPath;
static IsoMetadata s_isoMeta;
static bool s_isoLoaded = false;

// Step 3 (Tune) Options
static bool s_optOlderPCs = true;       // Works on older PCs (Skips RAM, Secure Boot, TPM checks)
static bool s_optNoMsAccount = true;    // No Microsoft account (Sign in offline during setup)
static bool s_optLocalUser = false;     // Make a local user
static std::wstring s_localUserName = L"User";
static bool s_optSkipEncryption = false;// Skip drive encryption (Turns off BitLocker)
static bool s_optPauseTracking = false; // Pause tracking (Turns off telemetry)
static std::wstring s_usbVolumeName = L"BOOTELWARE";
static bool s_isGpt = true;             // Partition scheme: GPT vs MBR
static std::wstring s_partitionScheme = L"GPT";
static std::wstring s_targetSystem = L"UEFI (auto)";

// Step 4 (Write) State
static std::atomic<bool> s_isWriting(false);
static std::atomic<bool> s_cancelWrite(false);
static int s_progressPercent = 0;
static double s_progressSpeed = 0.0;
static std::wstring s_currentTaskDesc = L"Preparing...";
static int s_chkCleaning = 0;       // 0: pending, 1: running, 2: done
static int s_chkCopying = 0;
static int s_chkExtras = 0;
static int s_chkDoubleChecking = 0;
static bool s_showDetails = false;
static std::vector<std::wstring> s_logLines;

// Format Tool State (Rufus Mode)
static std::wstring s_formatFs = L"FAT32"; // FAT32, NTFS, exFAT
static std::wstring s_formatLabel = L"BOOTELWARE";
static DWORD s_formatCluster = 0;          // 0 = Default auto
static bool s_formatQuick = true;
static bool s_isFormatting = false;
static std::wstring s_formatStatusText = L"Ready to format.";

// ─── Forward Declarations ────────────────────────────────────────────────────
static void RefreshDevices();
static void SelectIsoFile(const std::wstring& path);
static void BrowseForIso();
static void StartWriteJob();
static void StartFormatJob();
static void AppendLog(const std::wstring& line);
static void DrawAll(Graphics& g, int width, int height);

// ─── Simple In-Place Input Prompt ────────────────────────────────────────────
static bool PromptTextInput(HWND hParent, const std::wstring& title, const std::wstring& prompt, std::wstring& inOutVal) {
    struct InputDlgData {
        std::wstring title;
        std::wstring prompt;
        std::wstring result;
        bool ok = false;
    } data;
    data.title = title;
    data.prompt = prompt;
    data.result = inOutVal;

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = [](HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
        static InputDlgData* pData = nullptr;
        static HWND hEdit = NULL;
        switch (msg) {
        case WM_CREATE: {
            pData = (InputDlgData*)((LPCREATESTRUCT)lParam)->lpCreateParams;
            CreateWindowW(L"STATIC", pData->prompt.c_str(), WS_CHILD | WS_VISIBLE, 20, 15, 310, 20, hWnd, NULL, NULL, NULL);
            hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", pData->result.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 20, 42, 310, 24, hWnd, (HMENU)101, NULL, NULL);
            CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 160, 80, 80, 28, hWnd, (HMENU)IDOK, NULL, NULL);
            CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 250, 80, 80, 28, hWnd, (HMENU)IDCANCEL, NULL, NULL);
            SendMessageW(hEdit, EM_SETSEL, 0, -1);
            SetFocus(hEdit);
            EnumChildWindows(hWnd, [](HWND hChild, LPARAM) -> BOOL {
                SendMessageW(hChild, WM_SETFONT, (WPARAM)Theme::ThemeManager::hFontRegular, TRUE);
                return TRUE;
            }, 0);
            break;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == IDOK) {
                wchar_t buf[256] = { 0 };
                GetWindowTextW(hEdit, buf, 256);
                pData->result = buf;
                pData->ok = true;
                DestroyWindow(hWnd);
            } else if (id == IDCANCEL) {
                DestroyWindow(hWnd);
            }
            break;
        }
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, Theme::CLR_DARK);
            SetBkColor(hdc, Theme::CLR_WHITE);
            return (LRESULT)Theme::ThemeManager::hbrWhite;
        }
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hWnd, &rc);
            FillRect(hdc, &rc, Theme::ThemeManager::hbrWhite);
            return 1;
        }
        case WM_CLOSE:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProcW(hWnd, msg, wParam, lParam);
        }
        return 0;
    };
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"BootelwarePromptDlg";
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    RegisterClassW(&wc);

    RECT parentRect;
    GetWindowRect(hParent, &parentRect);
    int w = 360, h = 155;
    int x = parentRect.left + (parentRect.right - parentRect.left - w) / 2;
    int y = parentRect.top + (parentRect.bottom - parentRect.top - h) / 2;

    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"BootelwarePromptDlg", data.title.c_str(),
                                WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                                x, y, w, h, hParent, NULL, GetModuleHandleW(NULL), &data);
    EnableWindow(hParent, FALSE);
    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);

    if (data.ok) {
        inOutVal = data.result;
        return true;
    }
    return false;
}

// ─── Logging Helper ──────────────────────────────────────────────────────────
static void AppendLog(const std::wstring& line) {
    s_logLines.push_back(line);
    if (s_logLines.size() > 200) {
        s_logLines.erase(s_logLines.begin());
    }
    if (s_hWnd) {
        InvalidateRect(s_hWnd, NULL, FALSE);
    }
}

// ─── Device Enumeration ──────────────────────────────────────────────────────
static void RefreshDevices() {
    s_drives = DeviceManager::EnumerateTargetDrives(s_hostInfo.physicalDriveIndex, true);
    if (s_drives.empty()) {
        s_selectedDriveIndex = -1;
    } else {
        if (s_selectedDriveIndex < 0 || s_selectedDriveIndex >= (int)s_drives.size()) {
            s_selectedDriveIndex = 0;
        }
    }
    if (s_hWnd) InvalidateRect(s_hWnd, NULL, FALSE);
}

// ─── ISO Selection ───────────────────────────────────────────────────────────
static void SelectIsoFile(const std::wstring& path) {
    if (path.empty()) return;
    s_isoPath = path;
    s_isoMeta = IsoReader::AnalyzeIso(path);
    s_isoLoaded = s_isoMeta.isValid;

    if (s_isoLoaded) {
        // Auto-configure recommended options based on image
        if (s_isoMeta.detectedType == IsoType::Windows11) {
            s_optOlderPCs = true;
            s_optNoMsAccount = true;
        }
        if (!s_isoMeta.volumeLabel.empty()) {
            s_usbVolumeName = s_isoMeta.volumeLabel;
        }
    }
    if (s_hWnd) InvalidateRect(s_hWnd, NULL, FALSE);
}

static void BrowseForIso() {
    wchar_t szFile[MAX_PATH] = { 0 };
    OPENFILENAMEW ofn = { 0 };
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = s_hWnd;
    ofn.lpstrFilter = L"Disk Images (*.iso;*.img;*.vhd)\0*.iso;*.img;*.vhd\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;

    if (GetOpenFileNameW(&ofn)) {
        SelectIsoFile(szFile);
    }
}

// ─── Background Job: Create Bootable USB ─────────────────────────────────────
static void StartWriteJob() {
    if (s_selectedDriveIndex < 0 || s_selectedDriveIndex >= (int)s_drives.size()) {
        MessageBoxW(s_hWnd, L"Please select a target USB drive first.", L"No USB Selected", MB_OK | MB_ICONWARNING);
        return;
    }
    if (!s_isoLoaded) {
        MessageBoxW(s_hWnd, L"Please select an ISO image first.", L"No ISO Selected", MB_OK | MB_ICONWARNING);
        return;
    }

    const auto& drv = s_drives[s_selectedDriveIndex];
    std::wstring warnMsg = L"WARNING: ALL DATA ON THE TARGET USB DRIVE\n[" + drv.assignedDriveLetters + L" " + drv.model + 
                          L"] WILL BE PERMANENTLY ERASED.\n\nAre you sure you want to proceed?";
    if (MessageBoxW(s_hWnd, warnMsg.c_str(), L"Confirm USB Creation", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
        return;
    }

    s_currentView = VIEW_WRITE;
    s_isWriting = true;
    s_cancelWrite = false;
    s_progressPercent = 0;
    s_progressSpeed = 0.0;
    s_currentTaskDesc = L"Cleaning the drive...";
    s_chkCleaning = 1;
    s_chkCopying = 0;
    s_chkExtras = 0;
    s_chkDoubleChecking = 0;
    s_logLines.clear();

    WriteJobConfig cfg;
    cfg.physicalDriveIndex = drv.physicalDriveIndex;
    cfg.assignedDriveLetter = drv.assignedDriveLetters;
    cfg.isoPath = s_isoPath;
    cfg.isoMeta = s_isoMeta;
    cfg.partitionScheme = s_partitionScheme;
    cfg.targetSystem = s_targetSystem;
    cfg.fileSystem = L"FAT32"; // Large FAT32
    cfg.volumeLabel = s_usbVolumeName.empty() ? L"BOOTELWARE" : s_usbVolumeName;
    cfg.enableWin11Bypasses = (s_isoMeta.detectedType == IsoType::Windows11);
    cfg.win11Options.bypassTpmSecureBootRam = s_optOlderPCs;
    cfg.win11Options.bypassOnlineAccount = s_optNoMsAccount;
    cfg.win11Options.createLocalAccount = s_optLocalUser;
    cfg.win11Options.localAccountUsername = s_localUserName;
    cfg.win11Options.disableBitLocker = s_optSkipEncryption;
    cfg.win11Options.disableTelemetry = s_optPauseTracking;

    std::thread([cfg]() {
        std::wstring outErr;
        auto progressCb = [](int pct, double speed, const std::wstring& task) {
            s_progressPercent = pct;
            s_progressSpeed = speed;
            s_currentTaskDesc = task;

            if (pct < 10) {
                s_chkCleaning = 1;
            } else if (pct < 90) {
                s_chkCleaning = 2;
                s_chkCopying = 1;
            } else if (pct < 98) {
                s_chkCleaning = 2;
                s_chkCopying = 2;
                s_chkExtras = 1;
            } else {
                s_chkCleaning = 2;
                s_chkCopying = 2;
                s_chkExtras = 2;
                s_chkDoubleChecking = 1;
            }

            if (s_hWnd) PostMessageW(s_hWnd, WM_USER + 200, 0, 0);
        };

        auto logCb = [](const std::wstring& line) {
            AppendLog(line);
        };

        bool ok = DiskWriter::ExecuteWriteJob(cfg, progressCb, logCb, s_cancelWrite, outErr);
        s_isWriting = false;

        if (ok) {
            s_chkCleaning = 2;
            s_chkCopying = 2;
            s_chkExtras = 2;
            s_chkDoubleChecking = 2;
            s_currentView = VIEW_DONE;
        } else {
            if (!s_cancelWrite.load()) {
                MessageBoxW(s_hWnd, (L"Failed to create bootable USB:\n" + outErr).c_str(), L"Creation Error", MB_OK | MB_ICONERROR);
            }
            s_currentView = VIEW_TUNE;
        }
        if (s_hWnd) PostMessageW(s_hWnd, WM_USER + 200, 0, 0);
    }).detach();

    InvalidateRect(s_hWnd, NULL, FALSE);
}

// ─── Background Job: Standalone Format USB (Rufus Mode) ───────────────────────
static void StartFormatJob() {
    if (s_selectedDriveIndex < 0 || s_selectedDriveIndex >= (int)s_drives.size()) {
        MessageBoxW(s_hWnd, L"Please select a target USB drive to format.", L"No Drive Selected", MB_OK | MB_ICONWARNING);
        return;
    }

    const auto& drv = s_drives[s_selectedDriveIndex];
    std::wstring warnMsg = L"WARNING: ALL DATA ON [" + drv.assignedDriveLetters + L" " + drv.model + 
                          L"] WILL BE COMPLETELY ERASED.\n\nFile System: " + s_formatFs + 
                          (s_formatFs == L"FAT32" ? L" (Large FAT32 unlocked > 32 GB)\n" : L"\n") +
                          L"Volume Label: " + s_formatLabel + L"\n\nFormat drive now?";
    if (MessageBoxW(s_hWnd, warnMsg.c_str(), L"Confirm Format", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
        return;
    }

    s_isFormatting = true;
    s_formatStatusText = L"Formatting drive in progress...";
    s_logLines.clear();

    std::wstring driveLetter = drv.assignedDriveLetters;
    std::wstring fsType = s_formatFs;
    std::wstring label = s_formatLabel.empty() ? L"BOOTELWARE" : s_formatLabel;
    DWORD cluster = s_formatCluster;
    bool quick = s_formatQuick;

    std::thread([driveLetter, fsType, label, cluster, quick]() {
        std::atomic<bool> cancelFlag(false);
        std::wstring outErr;

        auto progressCb = [](int pct, double speed, const std::wstring& task) {
            s_formatStatusText = task;
            if (s_hWnd) PostMessageW(s_hWnd, WM_USER + 200, 0, 0);
        };

        auto logCb = [](const std::wstring& line) {
            AppendLog(line);
        };

        bool ok = DiskWriter::FormatStandaloneDrive(driveLetter, fsType, label, cluster, quick, progressCb, logCb, cancelFlag, outErr);
        s_isFormatting = false;

        if (ok) {
            s_formatStatusText = L"Format completed successfully!";
            MessageBoxW(s_hWnd, (L"Drive " + driveLetter + L" formatted successfully as " + fsType + L"!").c_str(), L"Format Success", MB_OK | MB_ICONINFORMATION);
            RefreshDevices();
        } else {
            s_formatStatusText = L"Format failed: " + outErr;
            MessageBoxW(s_hWnd, (L"Formatting failed:\n" + outErr).c_str(), L"Format Error", MB_OK | MB_ICONERROR);
        }
        if (s_hWnd) PostMessageW(s_hWnd, WM_USER + 200, 0, 0);
    }).detach();

    InvalidateRect(s_hWnd, NULL, FALSE);
}

// ─── UI Rendering Implementation ─────────────────────────────────────────────
static void AddHitArea(const RectF& rc, ButtonAction act, int indexData = 0) {
    s_hitAreas.push_back({ rc, act, indexData });
}

static void DrawAll(Graphics& g, int w, int h) {
    s_hitAreas.clear();

    // 1. Draw Canvas Background
    SolidBrush canvasBrush(Color(255, 236, 238, 248));
    g.FillRectangle(&canvasBrush, 0, 0, w, h);

    // 2. Draw Top Header Bar
    // Logo & Title with gentle idle animation
    Theme::ThemeManager::DrawMascot(g, 35, 16, 42, false, s_animTick);
    AddHitArea(RectF(35, 16, 175, 42), ACT_LOGO);

    SolidBrush darkBrush(Color(255, 24, 24, 36));
    g.DrawString(L"Bootelware", -1, Theme::ThemeManager::fontLarge, PointF(86, 26), &darkBrush);

    StringFormat sfCenter;
    sfCenter.SetAlignment(StringAlignmentCenter);
    sfCenter.SetLineAlignment(StringAlignmentCenter);

    StringFormat sfLeft;
    sfLeft.SetAlignment(StringAlignmentNear);
    sfLeft.SetLineAlignment(StringAlignmentNear);

    // Stepper Pills (Center) - Only on Steps 1, 2, 3, 4
    if (s_currentView <= VIEW_WRITE) {
        float stepW = 86.0f, stepH = 34.0f, gap = 12.0f;
        float totalStepW = 4 * stepW + 3 * gap;
        float startX = (w - totalStepW) / 2.0f;
        float stepY = 20.0f;

        const wchar_t* stepNames[] = { L"Source", L"Drive", L"Tune", L"Write" };

        for (int i = 0; i < 4; ++i) {
            RectF pillRc(startX + i * (stepW + gap), stepY, stepW, stepH);
            AddHitArea(pillRc, (ButtonAction)(ACT_STEP_1 + i));

            if (i == (int)s_currentView) {
                // Active: Sunny Yellow with tactile shadow
                Theme::ThemeManager::DrawNeoButton(g, pillRc, (std::to_wstring(i + 1) + L" " + stepNames[i]).c_str(), Color(255, 254, 210, 50), Color(255, 24, 24, 36), s_hoveredAction == (ACT_STEP_1 + i), s_pressedAction == (ACT_STEP_1 + i), Theme::ThemeManager::fontBodyBold, 2.0f);
            } else if (i < (int)s_currentView) {
                // Completed: Mint Green with checkmark
                Theme::ThemeManager::DrawNeoButton(g, pillRc, (L"✓ " + std::wstring(stepNames[i])).c_str(), Color(255, 111, 227, 180), Color(255, 24, 24, 36), s_hoveredAction == (ACT_STEP_1 + i), s_pressedAction == (ACT_STEP_1 + i), Theme::ThemeManager::fontBodyBold, 1.5f);
            } else {
                // Pending: White
                Theme::ThemeManager::DrawNeoButton(g, pillRc, (std::to_wstring(i + 1) + L" " + stepNames[i]).c_str(), Color(255, 255, 255, 255), Color(255, 85, 88, 112), s_hoveredAction == (ACT_STEP_1 + i), s_pressedAction == (ACT_STEP_1 + i), Theme::ThemeManager::fontBody, 1.0f);
            }
        }
    }

    // Menu Button (Top Right)
    RectF menuRc(w - 135.0f, 20.0f, 95.0f, 34.0f);
    AddHitArea(menuRc, ACT_MENU_BTN);
    Color menuFill = (s_hoveredAction == ACT_MENU_BTN) ? Color(255, 245, 246, 253) : Color(255, 255, 255, 255);
    Theme::ThemeManager::DrawNeoButton(g, menuRc, L"☰  Menu", menuFill, Color(255, 24, 24, 36), s_hoveredAction == ACT_MENU_BTN, s_pressedAction == ACT_MENU_BTN, Theme::ThemeManager::fontBodyBold, 1.8f);

    // ─── Render Current Page View ─────────────────────────────────────────────
    SolidBrush subBrush(Color(255, 85, 88, 112));

    switch (s_currentView) {
    case VIEW_SOURCE: { // ─── Page 1: Source ───
        g.DrawString(L"Let's make a bootable USB!", -1, Theme::ThemeManager::fontTitle, PointF(45, 88), &darkBrush);
        g.DrawString(L"First, choose the Windows or Linux file (ISO) you downloaded.", -1, Theme::ThemeManager::fontSubtitle, PointF(45, 126), &subBrush);

        // Dashed Drop Box
        RectF dropRc(45, 155, w - 90.0f, 310.0f);
        Theme::ThemeManager::DrawDashedCard(g, dropRc, Color(255, 255, 255, 255), Color(255, 24, 24, 36), 18.0f, 2.0f);

        float cx = dropRc.X + dropRc.Width / 2.0f;

        if (!s_isoLoaded) {
            // Bullseye
            Theme::ThemeManager::DrawTargetBullseye(g, cx, 235.0f, 24.0f);

            // Texts
            g.DrawString(L"Drop your ISO file here", -1, Theme::ThemeManager::fontLarge, RectF(45, 275, w - 90.0f, 30), &sfCenter, &darkBrush);
            g.DrawString(L"or", -1, Theme::ThemeManager::fontSubtitle, RectF(45, 305, w - 90.0f, 25), &sfCenter, &subBrush);

            // Browse Button
            RectF browseRc(cx - 95.0f, 335.0f, 190.0f, 44.0f);
            AddHitArea(browseRc, ACT_BROWSE_ISO);
            Color bFill = (s_hoveredAction == ACT_BROWSE_ISO) ? Color(255, 255, 224, 88) : Color(255, 254, 210, 50);
            Theme::ThemeManager::DrawNeoButton(g, browseRc, L"Browse my files", bFill, Color(255, 24, 24, 36), s_hoveredAction == ACT_BROWSE_ISO, s_pressedAction == ACT_BROWSE_ISO, Theme::ThemeManager::fontBodyBold, 2.5f);
        } else {
            // Selected ISO state
            Theme::ThemeManager::DrawTargetBullseye(g, cx, 215.0f, 22.0f);

            std::wstring fileName = s_isoPath;
            size_t slash = fileName.find_last_of(L"\\/");
            if (slash != std::wstring::npos) fileName = fileName.substr(slash + 1);

            g.DrawString(fileName.c_str(), -1, Theme::ThemeManager::fontLarge, RectF(45, 250, w - 90.0f, 30), &sfCenter, &darkBrush);

            std::wstringstream ss;
            ss << std::fixed << std::setprecision(1) << s_isoMeta.fileSizeGb << L" GB · "
               << (!s_isoMeta.osName.empty() ? s_isoMeta.osName : L"Disk Image")
               << L" · " << s_partitionScheme << L" · " << s_targetSystem;
            g.DrawString(ss.str().c_str(), -1, Theme::ThemeManager::fontSubtitle, RectF(45, 282, w - 90.0f, 25), &sfCenter, &subBrush);

            // Change ISO & Continue Buttons
            RectF changeRc(cx - 150.0f, 335.0f, 135.0f, 42.0f);
            AddHitArea(changeRc, ACT_CHANGE_ISO);
            Color chFill = (s_hoveredAction == ACT_CHANGE_ISO) ? Color(255, 245, 246, 253) : Color(255, 255, 255, 255);
            Theme::ThemeManager::DrawNeoButton(g, changeRc, L"Change file", chFill, Color(255, 24, 24, 36), s_hoveredAction == ACT_CHANGE_ISO, s_pressedAction == ACT_CHANGE_ISO, Theme::ThemeManager::fontBodyBold, 2.0f);

            RectF contRc(cx + 15.0f, 335.0f, 135.0f, 42.0f);
            AddHitArea(contRc, ACT_CONTINUE_ISO);
            Color contFill = (s_hoveredAction == ACT_CONTINUE_ISO) ? Color(255, 90, 75, 215) : Color(255, 107, 92, 231);
            Theme::ThemeManager::DrawNeoButton(g, contRc, L"Next  →", contFill, Color(255, 255, 255, 255), s_hoveredAction == ACT_CONTINUE_ISO, s_pressedAction == ACT_CONTINUE_ISO, Theme::ThemeManager::fontBodyBold, 2.5f);
        }

        // Bottom Row: No file yet? Get one:
        g.DrawString(L"No file yet? Get one:", -1, Theme::ThemeManager::fontBody, PointF(45, 498), &subBrush);

        float btnX = 185.0f, btnY = 490.0f, btnH = 34.0f;
        auto drawDlPill = [&](const wchar_t* title, float wBtn, ButtonAction act) {
            RectF rc(btnX, btnY, wBtn, btnH);
            AddHitArea(rc, act);
            Color fill = (s_hoveredAction == act) ? Color(255, 245, 246, 253) : Color(255, 255, 255, 255);
            Theme::ThemeManager::DrawNeoButton(g, rc, title, fill, Color(255, 24, 24, 36), s_hoveredAction == act, s_pressedAction == act, Theme::ThemeManager::fontBodyBold, 1.8f);
            btnX += wBtn + 10.0f;
        };

        drawDlPill(L"Windows 11", 110.0f, ACT_GET_WIN11);
        drawDlPill(L"Windows 10", 110.0f, ACT_GET_WIN10);
        drawDlPill(L"Ubuntu", 85.0f, ACT_GET_UBUNTU);
        drawDlPill(L"Other", 75.0f, ACT_GET_OTHER);
        break;
    }

    case VIEW_DRIVE: { // ─── Page 2: Drive ───
        g.DrawString(L"Which USB should we use?", -1, Theme::ThemeManager::fontTitle, PointF(45, 88), &darkBrush);
        std::wstring driveSub = L"We will put " + (!s_isoMeta.osName.empty() ? s_isoMeta.osName : (s_isoLoaded ? L"your OS image" : L"system installer")) + L" on it.";
        g.DrawString(driveSub.c_str(), -1, Theme::ThemeManager::fontSubtitle, PointF(45, 126), &subBrush);

        // Drive Card
        RectF driveCardRc(45, 160, w - 90.0f, 78.0f);
        AddHitArea(driveCardRc, ACT_SELECT_DRIVE_CARD);
        Color dFill = (s_selectedDriveIndex >= 0) ? Color(255, 230, 250, 242) : Color(255, 255, 255, 255);
        Theme::ThemeManager::DrawNeoCard(g, driveCardRc, dFill);

        // USB Icon
        Theme::ThemeManager::DrawUsbIcon(g, 70, 182, 34);

        if (s_selectedDriveIndex >= 0 && s_selectedDriveIndex < (int)s_drives.size()) {
            const auto& drv = s_drives[s_selectedDriveIndex];
            g.DrawString(drv.model.empty() ? L"USB Flash Drive" : drv.model.c_str(), -1, Theme::ThemeManager::fontLarge, PointF(125, 175), &darkBrush);

            std::wstringstream ss;
            ss << L"Drive " << drv.assignedDriveLetters << L" · " << std::fixed << std::setprecision(1) << drv.totalSizeGb << L" GB"
               << (drv.currentFileSystem.empty() ? L"" : L" (" + drv.currentFileSystem + L")");
            g.DrawString(ss.str().c_str(), -1, Theme::ThemeManager::fontSubtitle, PointF(125, 203), &subBrush);

            // Selected Badge
            RectF badgeRc(w - 185.0f, 180.0f, 105.0f, 36.0f);
            Theme::ThemeManager::DrawNeoPill(g, badgeRc, Color(255, 111, 227, 180));
            g.DrawString(L"Selected", -1, Theme::ThemeManager::fontBodyBold, badgeRc, &sfCenter, &darkBrush);
        } else {
            g.DrawString(L"No USB drive detected", -1, Theme::ThemeManager::fontLarge, PointF(125, 175), &darkBrush);
            g.DrawString(L"Please connect your USB flash drive and click Rescan.", -1, Theme::ThemeManager::fontSubtitle, PointF(125, 203), &subBrush);
        }

        // Warning Box
        RectF warnRc(45, 255, w - 90.0f, 52.0f);
        Theme::ThemeManager::DrawNeoCard(g, warnRc, Color(255, 255, 245, 192), Color(255, 24, 24, 36), 14.0f);
        Theme::ThemeManager::DrawWarningTriangle(g, 75, 281, 20);
        g.DrawString(L"Everything on this USB will be erased. Back up your files first!", -1, Theme::ThemeManager::fontBodyBold, PointF(98, 272), &darkBrush);

        // Action Buttons Row
        RectF rescanRc(45, 330, 165.0f, 38.0f);
        AddHitArea(rescanRc, ACT_RESCAN_DRIVES);
        Color rFill = (s_hoveredAction == ACT_RESCAN_DRIVES) ? Color(255, 245, 246, 253) : Color(255, 255, 255, 255);
        Theme::ThemeManager::DrawNeoButton(g, rescanRc, L"Rescan USB drives", rFill, Color(255, 24, 24, 36), s_hoveredAction == ACT_RESCAN_DRIVES, s_pressedAction == ACT_RESCAN_DRIVES, Theme::ThemeManager::fontBodyBold, 1.8f);

        // Rufus format mode button
        RectF formatRc(225, 330, 195.0f, 38.0f);
        AddHitArea(formatRc, ACT_GOTO_FORMAT_USB);
        Color fFill = (s_hoveredAction == ACT_GOTO_FORMAT_USB) ? Color(255, 255, 224, 88) : Color(255, 254, 210, 50);
        Theme::ThemeManager::DrawNeoButton(g, formatRc, L"⚡ Format USB Drive", fFill, Color(255, 24, 24, 36), s_hoveredAction == ACT_GOTO_FORMAT_USB, s_pressedAction == ACT_GOTO_FORMAT_USB, Theme::ThemeManager::fontBodyBold, 2.0f);

        // Bottom Navigation
        RectF backRc(45, h - 75.0f, 95.0f, 42.0f);
        AddHitArea(backRc, ACT_DRIVE_BACK);
        Color bFill = (s_hoveredAction == ACT_DRIVE_BACK) ? Color(255, 245, 246, 253) : Color(255, 255, 255, 255);
        Theme::ThemeManager::DrawNeoButton(g, backRc, L"Back", bFill, Color(255, 24, 24, 36), s_hoveredAction == ACT_DRIVE_BACK, s_pressedAction == ACT_DRIVE_BACK, Theme::ThemeManager::fontBodyBold, 2.0f);

        RectF nextRc(w - 140.0f, h - 75.0f, 95.0f, 42.0f);
        AddHitArea(nextRc, ACT_DRIVE_NEXT);
        Color nFill = (s_hoveredAction == ACT_DRIVE_NEXT) ? Color(255, 90, 75, 215) : Color(255, 107, 92, 231);
        Theme::ThemeManager::DrawNeoButton(g, nextRc, L"Next", nFill, Color(255, 255, 255, 255), s_hoveredAction == ACT_DRIVE_NEXT, s_pressedAction == ACT_DRIVE_NEXT, Theme::ThemeManager::fontBodyBold, 2.5f);
        break;
    }

    case VIEW_TUNE: { // ─── Page 3: Tune (Pick your Windows extras) ───
        g.DrawString(L"Pick your Windows extras", -1, Theme::ThemeManager::fontTitle, PointF(45, 88), &darkBrush);
        g.DrawString(L"Not sure? The recommended ones are a safe start.", -1, Theme::ThemeManager::fontSubtitle, PointF(45, 126), &subBrush);

        // 2x3 Grid
        float colW = (w - 110.0f) / 2.0f;
        float cardH = 76.0f;
        float x1 = 45.0f, x2 = 45.0f + colW + 20.0f;
        float y1 = 160.0f, y2 = 248.0f, y3 = 336.0f;

        auto drawOptionCard = [&](float x, float y, const wchar_t* title, const wchar_t* desc, bool isChecked, bool showBadge, ButtonAction act) {
            RectF rc(x, y, colW, cardH);
            AddHitArea(rc, act);
            Color fill = (s_hoveredAction == act) ? Color(255, 248, 249, 254) : Color(255, 255, 255, 255);
            Theme::ThemeManager::DrawNeoCard(g, rc, fill, Color(255, 24, 24, 36), 18.0f);

            // Checkbox
            float boxX = x + 20.0f, boxY = y + (cardH - 24.0f) / 2.0f;
            RectF boxRc(boxX, boxY, 24.0f, 24.0f);
            Color chkFill = isChecked ? Color(255, 107, 92, 231) : Color(255, 255, 255, 255);
            Theme::ThemeManager::DrawNeoCard(g, boxRc, chkFill, Color(255, 24, 24, 36), 6.0f, 1.8f);
            if (isChecked) {
                Theme::ThemeManager::DrawCheckmark(g, boxX + 4.0f, boxY + 4.0f, 16.0f, Color(255, 255, 255, 255), 2.2f);
            }

            // Title & Badge
            float textX = boxX + 38.0f;
            g.DrawString(title, -1, Theme::ThemeManager::fontBodyBold, PointF(textX, y + 16.0f), &darkBrush);

            if (showBadge) {
                RectF badgeRc(textX + 170.0f, y + 16.0f, 95.0f, 22.0f);
                Theme::ThemeManager::DrawNeoPill(g, badgeRc, Color(255, 111, 227, 180), Color(255, 24, 24, 36), 1.5f);
                g.DrawString(L"Recommended", -1, Theme::ThemeManager::fontSmallBold, badgeRc, &sfCenter, &darkBrush);
            }

            g.DrawString(desc, -1, Theme::ThemeManager::fontSmall, PointF(textX, y + 42.0f), &subBrush);
        };

        // Card 1: Works on older PCs
        drawOptionCard(x1, y1, L"Works on older PCs", L"Skips RAM, Secure Boot and TPM checks", s_optOlderPCs, true, ACT_OPT_OLDER_PCS);
        // Card 2: No Microsoft account
        drawOptionCard(x2, y1, L"No Microsoft account", L"Sign in offline during setup", s_optNoMsAccount, true, ACT_OPT_NO_MS_ACCOUNT);
        // Card 3: Make a local user
        std::wstring userDesc = L"Name: " + s_localUserName + L" (Click to customize)";
        drawOptionCard(x1, y2, L"Make a local user", userDesc.c_str(), s_optLocalUser, false, ACT_OPT_LOCAL_USER);
        // Card 4: Skip drive encryption
        drawOptionCard(x2, y2, L"Skip drive encryption", L"Turns off BitLocker auto-encrypt", s_optSkipEncryption, false, ACT_OPT_SKIP_ENCRYPTION);
        // Card 5: Pause tracking
        drawOptionCard(x1, y3, L"Pause tracking", L"Turns off diagnostic data", s_optPauseTracking, false, ACT_OPT_PAUSE_TRACKING);

        // Card 6: Partition Scheme Toggle & Host Format Indicator
        {
            RectF rc(x2, y3, colW, cardH);
            Theme::ThemeManager::DrawNeoCard(g, rc, Color(255, 255, 255, 255), Color(255, 24, 24, 36), 18.0f);

            float textX = x2 + 20.0f;
            g.DrawString(L"Partition scheme", -1, Theme::ThemeManager::fontBodyBold, PointF(textX, y3 + 14.0f), &darkBrush);

            std::wstring hostDetected = L"Host (C:) is " + (s_hostInfo.isGpt ? std::wstring(L"GPT (UEFI)") : std::wstring(L"MBR (BIOS)"));
            g.DrawString(hostDetected.c_str(), -1, Theme::ThemeManager::fontSmall, PointF(textX, y3 + 40.0f), &subBrush);

            // Toggle Pills for GPT vs MBR
            float btnW = 86.0f, btnH = 34.0f;
            float btnY = y3 + (cardH - btnH) / 2.0f;
            RectF gptRc(x2 + colW - 195.0f, btnY, btnW, btnH);
            RectF mbrRc(x2 + colW - 98.0f, btnY, btnW, btnH);

            AddHitArea(gptRc, ACT_TUNE_SCHEME_GPT);
            AddHitArea(mbrRc, ACT_TUNE_SCHEME_MBR);

            Color gptFill = s_isGpt ? Color(255, 107, 92, 231) : Color(255, 255, 255, 255);
            Color gptText = s_isGpt ? Color(255, 255, 255, 255) : Color(255, 24, 24, 36);
            Theme::ThemeManager::DrawNeoButton(g, gptRc, L"GPT", gptFill, gptText, s_hoveredAction == ACT_TUNE_SCHEME_GPT, s_pressedAction == ACT_TUNE_SCHEME_GPT, Theme::ThemeManager::fontBodyBold, 1.8f);

            Color mbrFill = (!s_isGpt) ? Color(255, 107, 92, 231) : Color(255, 255, 255, 255);
            Color mbrText = (!s_isGpt) ? Color(255, 255, 255, 255) : Color(255, 24, 24, 36);
            Theme::ThemeManager::DrawNeoButton(g, mbrRc, L"MBR", mbrFill, mbrText, s_hoveredAction == ACT_TUNE_SCHEME_MBR, s_pressedAction == ACT_TUNE_SCHEME_MBR, Theme::ThemeManager::fontBodyBold, 1.8f);
        }

        // Check if there is a mismatch with Host C:
        bool isMismatch = (s_isGpt != s_hostInfo.isGpt);
        if (isMismatch) {
            // Warning Pop Banner beneath the cards
            RectF popRc(45.0f, 424.0f, w - 90.0f, 54.0f);
            Theme::ThemeManager::DrawNeoCard(g, popRc, Color(255, 255, 245, 192), Color(255, 24, 24, 36), 14.0f, 2.0f, 2.0f);
            Theme::ThemeManager::DrawWarningTriangle(g, 68.0f, 451.0f, 22.0f);

            std::wstring warnText = s_hostInfo.isGpt 
                ? L"Partition Mismatch: Host C: is GPT (UEFI). Selecting MBR may make this USB unbootable on this PC!"
                : L"Partition Mismatch: Host C: is MBR (BIOS). Selecting GPT requires UEFI motherboard to boot!";

            StringFormat sfWarn;
            sfWarn.SetAlignment(StringAlignmentNear);
            sfWarn.SetLineAlignment(StringAlignmentCenter);
            sfWarn.SetTrimming(StringTrimmingEllipsisCharacter);
            g.DrawString(warnText.c_str(), -1, Theme::ThemeManager::fontSmallBold, RectF(90.0f, 426.0f, popRc.Width - 310.0f, 50.0f), &sfWarn, &darkBrush);

            // Quick Fix Button on the Pop
            RectF fixRc(w - 245.0f, 432.0f, 185.0f, 38.0f);
            AddHitArea(fixRc, ACT_TUNE_FIX_MISMATCH);
            std::wstring fixLabel = s_hostInfo.isGpt ? L"Switch to GPT" : L"Switch to MBR";
            Theme::ThemeManager::DrawNeoButton(g, fixRc, fixLabel.c_str(), Color(255, 254, 210, 50), Color(255, 24, 24, 36), s_hoveredAction == ACT_TUNE_FIX_MISMATCH, s_pressedAction == ACT_TUNE_FIX_MISMATCH, Theme::ThemeManager::fontSmallBold, 1.8f);
        }

        // Bottom Navigation
        RectF backRc(45, h - 75.0f, 95.0f, 42.0f);
        AddHitArea(backRc, ACT_TUNE_BACK);
        Color bFill = (s_hoveredAction == ACT_TUNE_BACK) ? Color(255, 245, 246, 253) : Color(255, 255, 255, 255);
        Theme::ThemeManager::DrawNeoButton(g, backRc, L"Back", bFill, Color(255, 24, 24, 36), s_hoveredAction == ACT_TUNE_BACK, s_pressedAction == ACT_TUNE_BACK, Theme::ThemeManager::fontBodyBold, 2.0f);

        RectF createRc(w - 205.0f, h - 75.0f, 160.0f, 42.0f);
        AddHitArea(createRc, ACT_CREATE_USB);
        Color cFill = (s_hoveredAction == ACT_CREATE_USB) ? Color(255, 90, 75, 215) : Color(255, 107, 92, 231);
        Theme::ThemeManager::DrawNeoButton(g, createRc, L"Create my USB", cFill, Color(255, 255, 255, 255), s_hoveredAction == ACT_CREATE_USB, s_pressedAction == ACT_CREATE_USB, Theme::ThemeManager::fontBodyBold, 2.5f);
        break;
    }

    case VIEW_WRITE: { // ─── Page 4: Write (Hang tight, working on it!) ───
        g.DrawString(L"Hang tight, working on it!", -1, Theme::ThemeManager::fontTitle, PointF(45, 88), &darkBrush);
        g.DrawString(L"This usually takes a few minutes. Please don't unplug the USB.", -1, Theme::ThemeManager::fontSubtitle, PointF(45, 126), &subBrush);

        // Main Progress Card
        RectF mainRc(45, 160, w - 90.0f, 320.0f);
        Theme::ThemeManager::DrawNeoCard(g, mainRc, Color(255, 255, 255, 255), Color(255, 24, 24, 36), 18.0f);

        // Left Side: Status & Progress
        // Truncated with ellipsis so it NEVER overlaps with percentage
        StringFormat sfTask;
        sfTask.SetAlignment(StringAlignmentNear);
        sfTask.SetLineAlignment(StringAlignmentCenter);
        sfTask.SetTrimming(StringTrimmingEllipsisCharacter);
        sfTask.SetFormatFlags(StringFormatFlagsNoWrap);

        RectF taskRc(75.0f, 206.0f, 375.0f, 32.0f);
        g.DrawString(s_currentTaskDesc.c_str(), -1, Theme::ThemeManager::fontLarge, taskRc, &sfTask, &darkBrush);

        StringFormat sfPct;
        sfPct.SetAlignment(StringAlignmentFar);
        sfPct.SetLineAlignment(StringAlignmentCenter);
        sfPct.SetFormatFlags(StringFormatFlagsNoWrap);

        std::wstringstream pctSs;
        pctSs << s_progressPercent << L"%";
        RectF pctRc(455.0f, 206.0f, 70.0f, 32.0f);
        g.DrawString(pctSs.str().c_str(), -1, Theme::ThemeManager::fontLarge, pctRc, &sfPct, &darkBrush);

        // Progress Bar with animated shimmer
        RectF trackRc(75, 245, 450.0f, 26.0f);
        Theme::ThemeManager::DrawNeoPill(g, trackRc, Color(255, 255, 255, 255), Color(255, 24, 24, 36), 2.0f);

        if (s_progressPercent > 0) {
            float fillW = (s_progressPercent / 100.0f) * trackRc.Width;
            if (fillW < 26.0f) fillW = 26.0f;
            RectF fillRc(trackRc.X, trackRc.Y, fillW, trackRc.Height);
            Theme::ThemeManager::DrawNeoPill(g, fillRc, Color(255, 254, 210, 50), Color(255, 24, 24, 36), 2.0f);

            // Shimmer animation
            Region oldClip;
            g.GetClip(&oldClip);
            g.SetClip(fillRc);
            float shimPos = trackRc.X - 50.0f + fmodf((float)s_animTick * 3.8f, fillW + 80.0f);
            RectF shimRc(shimPos, trackRc.Y + 3.0f, 34.0f, trackRc.Height - 6.0f);
            SolidBrush shimBrush(Color(80, 255, 255, 255));
            g.FillEllipse(&shimBrush, shimRc);
            g.SetClip(&oldClip);
        }

        // Speed Display
        if (s_progressSpeed > 0.05) {
            std::wstringstream speedSs;
            speedSs << std::fixed << std::setprecision(1) << s_progressSpeed << L" MB/s";
            g.DrawString(speedSs.str().c_str(), -1, Theme::ThemeManager::fontSubtitle, PointF(75, 280), &subBrush);
        }

        // Show details button
        RectF detailsRc(75, 310, 120.0f, 34.0f);
        AddHitArea(detailsRc, ACT_TOGGLE_DETAILS);
        Color detFill = (s_hoveredAction == ACT_TOGGLE_DETAILS) ? Color(255, 245, 246, 253) : Color(255, 255, 255, 255);
        Theme::ThemeManager::DrawNeoButton(g, detailsRc, s_showDetails ? L"Hide details" : L"Show details", detFill, Color(255, 24, 24, 36), s_hoveredAction == ACT_TOGGLE_DETAILS, s_pressedAction == ACT_TOGGLE_DETAILS, Theme::ThemeManager::fontBodyBold, 1.8f);

        // If details expanded, draw strictly clipped log box inside left panel
        if (s_showDetails) {
            RectF logRc(75, 352, 450.0f, 110.0f);
            Theme::ThemeManager::DrawNeoCard(g, logRc, Color(255, 245, 246, 253), Color(255, 24, 24, 36), 10.0f, 1.2f);

            Region oldClipLog;
            g.GetClip(&oldClipLog);
            RectF innerClip(logRc.X + 8.0f, logRc.Y + 6.0f, logRc.Width - 16.0f, logRc.Height - 12.0f);
            g.SetClip(innerClip);

            StringFormat sfMono;
            sfMono.SetAlignment(StringAlignmentNear);
            sfMono.SetLineAlignment(StringAlignmentCenter);
            sfMono.SetTrimming(StringTrimmingEllipsisCharacter);
            sfMono.SetFormatFlags(StringFormatFlagsNoWrap);

            int startIdx = s_logLines.size() > 5 ? (int)s_logLines.size() - 5 : 0;
            float lineY = logRc.Y + 8.0f;
            for (size_t i = startIdx; i < s_logLines.size(); ++i) {
                RectF lineRc(logRc.X + 10.0f, lineY, logRc.Width - 20.0f, 18.0f);
                g.DrawString(s_logLines[i].c_str(), -1, Theme::ThemeManager::fontMono, lineRc, &sfMono, &darkBrush);
                lineY += 19.0f;
            }

            g.SetClip(&oldClipLog);
        }

        // Right Side: Animated Mascot & Step Checklist
        Theme::ThemeManager::DrawMascot(g, w - 240.0f, 195.0f, 82.0f, false, s_animTick);

        float checkX = w - 300.0f, checkY = 285.0f;
        auto drawCheckItem = [&](const wchar_t* label, int state) {
            RectF circleRc(checkX, checkY, 20.0f, 20.0f);
            if (state == 2) { // Done: Mint green filled with checkmark
                Theme::ThemeManager::DrawNeoPill(g, circleRc, Color(255, 111, 227, 180), Color(255, 24, 24, 36), 1.8f);
                Theme::ThemeManager::DrawCheckmark(g, checkX + 4.0f, checkY + 4.0f, 12.0f, Color(255, 24, 24, 36), 2.0f);
            } else if (state == 1) { // Running: Pulsing animated yellow
                float pulse = fabsf(sinf((float)s_animTick * 0.15f)) * 5.0f;
                RectF haloRc(checkX - pulse / 2.0f, checkY - pulse / 2.0f, 20.0f + pulse, 20.0f + pulse);
                SolidBrush haloBrush(Color(70, 254, 210, 50));
                g.FillEllipse(&haloBrush, haloRc);

                Theme::ThemeManager::DrawNeoPill(g, circleRc, Color(255, 254, 210, 50), Color(255, 24, 24, 36), 1.8f);
                SolidBrush centerDot(Color(255, 24, 24, 36));
                g.FillEllipse(&centerDot, RectF(checkX + 6.0f, checkY + 6.0f, 8.0f, 8.0f));
            } else { // Pending: White outline
                Theme::ThemeManager::DrawNeoPill(g, circleRc, Color(255, 255, 255, 255), Color(255, 24, 24, 36), 1.8f);
            }
            g.DrawString(label, -1, Theme::ThemeManager::fontBody, PointF(checkX + 30.0f, checkY + 1.0f), &darkBrush);
            checkY += 30.0f;
        };

        drawCheckItem(L"Cleaning the drive", s_chkCleaning);
        drawCheckItem(L"Copying files", s_chkCopying);
        drawCheckItem(L"Adding your extras", s_chkExtras);
        drawCheckItem(L"Double-checking", s_chkDoubleChecking);

        // Bottom Row: Tip & Cancel
        g.DrawString(L"Tip: you can grab a coffee.", -1, Theme::ThemeManager::fontBody, PointF(45, h - 65.0f), &subBrush);

        RectF cancelRc(w - 155.0f, h - 75.0f, 110.0f, 42.0f);
        AddHitArea(cancelRc, ACT_CANCEL_WRITE);
        Color canFill = (s_hoveredAction == ACT_CANCEL_WRITE) ? Color(255, 245, 246, 253) : Color(255, 255, 255, 255);
        Theme::ThemeManager::DrawNeoButton(g, cancelRc, L"Cancel", canFill, Color(255, 24, 24, 36), s_hoveredAction == ACT_CANCEL_WRITE, s_pressedAction == ACT_CANCEL_WRITE, Theme::ThemeManager::fontBodyBold, 2.0f);
        break;
    }

    case VIEW_DONE: { // ─── Page 5: All done! ───
        // Joyful Mint Green Mascot in center with celebration bounce animation
        float cx = w / 2.0f;
        Theme::ThemeManager::DrawMascot(g, cx - 44.0f, 90.0f, 88.0f, true, s_animTick);

        g.DrawString(L"All done! Your USB is ready", -1, Theme::ThemeManager::fontTitle, RectF(0, 195, (float)w, 35), &sfCenter, &darkBrush);
        g.DrawString(L"Here is how to install from it:", -1, Theme::ThemeManager::fontSubtitle, RectF(0, 235, (float)w, 25), &sfCenter, &subBrush);

        // 3 Step Instruction Cards
        float cardW = (w - 130.0f) / 3.0f;
        float cardY = 280.0f, cHeight = 135.0f;

        auto drawDoneStep = [&](float x, const wchar_t* num, const wchar_t* title, const wchar_t* subtext) {
            RectF rc(x, cardY, cardW, cHeight);
            Theme::ThemeManager::DrawNeoCard(g, rc, Color(255, 255, 255, 255), Color(255, 24, 24, 36), 18.0f);

            // Number Pill
            float cMid = x + cardW / 2.0f;
            RectF numRc(cMid - 16.0f, cardY + 22.0f, 32.0f, 32.0f);
            Theme::ThemeManager::DrawNeoPill(g, numRc, Color(255, 254, 210, 50));
            g.DrawString(num, -1, Theme::ThemeManager::fontBodyBold, numRc, &sfCenter, &darkBrush);

            // Title & Subtext
            g.DrawString(title, -1, Theme::ThemeManager::fontBodyBold, RectF(x + 10, cardY + 68.0f, cardW - 20, 24), &sfCenter, &darkBrush);
            if (subtext) {
                g.DrawString(subtext, -1, Theme::ThemeManager::fontSmall, RectF(x + 10, cardY + 92.0f, cardW - 20, 20), &sfCenter, &subBrush);
            }
        };

        drawDoneStep(45.0f, L"1", L"Safely eject the USB", nullptr);
        drawDoneStep(45.0f + cardW + 20.0f, L"2", L"Plug it into the PC", nullptr);
        drawDoneStep(45.0f + (cardW + 20.0f) * 2.0f, L"3", L"Restart and press boot key", L"(F12 or Esc)");

        // Bottom Actions
        RectF anotherRc(45, h - 75.0f, 155.0f, 44.0f);
        AddHitArea(anotherRc, ACT_MAKE_ANOTHER);
        Color aFill = (s_hoveredAction == ACT_MAKE_ANOTHER) ? Color(255, 245, 246, 253) : Color(255, 255, 255, 255);
        Theme::ThemeManager::DrawNeoButton(g, anotherRc, L"Make another", aFill, Color(255, 24, 24, 36), s_hoveredAction == ACT_MAKE_ANOTHER, s_pressedAction == ACT_MAKE_ANOTHER, Theme::ThemeManager::fontBodyBold, 2.0f);

        RectF ejectRc(w - 205.0f, h - 75.0f, 160.0f, 44.0f);
        AddHitArea(ejectRc, ACT_EJECT_USB);
        Color ejFill = (s_hoveredAction == ACT_EJECT_USB) ? Color(255, 255, 224, 88) : Color(255, 254, 210, 50);
        Theme::ThemeManager::DrawNeoButton(g, ejectRc, L"Eject USB safely", ejFill, Color(255, 24, 24, 36), s_hoveredAction == ACT_EJECT_USB, s_pressedAction == ACT_EJECT_USB, Theme::ThemeManager::fontBodyBold, 2.5f);
        break;
    }

    case VIEW_FORMAT: { // ─── Format USB Screen (Rufus Mode: Large FAT32) ───
        g.DrawString(L"Format USB Drive", -1, Theme::ThemeManager::fontTitle, PointF(45, 88), &darkBrush);
        g.DrawString(L"Rufus-compatible USB formatter — unlocks FAT32 for drives larger than 32 GB!", -1, Theme::ThemeManager::fontSubtitle, PointF(45, 126), &subBrush);

        // Selected Drive Card
        RectF driveRc(45, 155, w - 90.0f, 65.0f);
        AddHitArea(driveRc, ACT_FORMAT_CHANGE_DRIVE);
        Theme::ThemeManager::DrawNeoCard(g, driveRc, Color(255, 255, 255, 255));
        Theme::ThemeManager::DrawUsbIcon(g, 65, 172, 30);

        if (s_selectedDriveIndex >= 0 && s_selectedDriveIndex < (int)s_drives.size()) {
            const auto& drv = s_drives[s_selectedDriveIndex];
            std::wstring dTitle = drv.assignedDriveLetters + L" " + drv.model + L" (" + std::to_wstring((int)drv.totalSizeGb) + L" GB)";
            g.DrawString(dTitle.c_str(), -1, Theme::ThemeManager::fontLarge, PointF(110, 167), &darkBrush);
            g.DrawString(L"Click to switch USB drive", -1, Theme::ThemeManager::fontSmall, PointF(110, 192), &subBrush);
        } else {
            g.DrawString(L"No USB drive selected. Plug in a drive and click Rescan.", -1, Theme::ThemeManager::fontLarge, PointF(110, 175), &darkBrush);
        }

        // File System Selector Card
        RectF fsCardRc(45, 235, w - 90.0f, 65.0f);
        Theme::ThemeManager::DrawNeoCard(g, fsCardRc, Color(255, 255, 255, 255));
        g.DrawString(L"File system:", -1, Theme::ThemeManager::fontBodyBold, PointF(65, 257), &darkBrush);

        auto drawFsPill = [&](float x, const wchar_t* label, float pillW, bool active, ButtonAction act) {
            RectF pill(x, 248.0f, pillW, 36.0f);
            AddHitArea(pill, act);
            Color fill = active ? Color(255, 254, 210, 50) : ((s_hoveredAction == act) ? Color(255, 245, 246, 253) : Color(255, 255, 255, 255));
            Theme::ThemeManager::DrawNeoButton(g, pill, label, fill, Color(255, 24, 24, 36), s_hoveredAction == act, s_pressedAction == act, Theme::ThemeManager::fontBodyBold, 1.8f);
        };

        drawFsPill(165.0f, L"FAT32 (Large FAT32 unlocked > 32 GB)", 320.0f, s_formatFs == L"FAT32", ACT_FORMAT_FS_FAT32);
        drawFsPill(500.0f, L"NTFS", 80.0f, s_formatFs == L"NTFS", ACT_FORMAT_FS_NTFS);
        drawFsPill(595.0f, L"exFAT", 85.0f, s_formatFs == L"exFAT", ACT_FORMAT_FS_EXFAT);

        // Options: Label & Cluster size
        RectF optRc(45, 315, w - 90.0f, 65.0f);
        Theme::ThemeManager::DrawNeoCard(g, optRc, Color(255, 255, 255, 255));
        std::wstring lblTxt = L"Volume label: " + s_formatLabel;
        g.DrawString(lblTxt.c_str(), -1, Theme::ThemeManager::fontBodyBold, PointF(65, 337), &darkBrush);
        g.DrawString(L"Cluster size: Default (Optimized for size) · Quick Format enabled", -1, Theme::ThemeManager::fontSubtitle, PointF(380, 337), &subBrush);

        // Yellow Explanatory Box
        RectF calloutRc(45, 395, w - 90.0f, 55.0f);
        Theme::ThemeManager::DrawNeoCard(g, calloutRc, Color(255, 255, 245, 192), Color(255, 24, 24, 36), 14.0f);
        Theme::ThemeManager::DrawWarningTriangle(g, 75, 422, 18);
        g.DrawString(L"Overcoming Windows 32 GB Limit: Windows natively disables FAT32 for drives > 32 GB.", -1, Theme::ThemeManager::fontSmallBold, PointF(98, 407), &darkBrush);
        g.DrawString(L"Bootelware calculates custom cluster geometry so 64GB, 128GB+ sticks format cleanly for UEFI and car media!", -1, Theme::ThemeManager::fontSmall, PointF(98, 426), &subBrush);

        // Bottom Actions
        RectF backRc(45, h - 75.0f, 95.0f, 42.0f);
        AddHitArea(backRc, ACT_FORMAT_BACK);
        Color bFill = (s_hoveredAction == ACT_FORMAT_BACK) ? Color(255, 245, 246, 253) : Color(255, 255, 255, 255);
        Theme::ThemeManager::DrawNeoButton(g, backRc, L"Back", bFill, Color(255, 24, 24, 36), s_hoveredAction == ACT_FORMAT_BACK, s_pressedAction == ACT_FORMAT_BACK, Theme::ThemeManager::fontBodyBold, 2.0f);

        RectF fmtExecRc(w - 225.0f, h - 75.0f, 180.0f, 42.0f);
        AddHitArea(fmtExecRc, ACT_FORMAT_EXECUTE);
        Color feFill = (s_hoveredAction == ACT_FORMAT_EXECUTE) ? Color(255, 90, 75, 215) : Color(255, 107, 92, 231);
        Theme::ThemeManager::DrawNeoButton(g, fmtExecRc, s_isFormatting ? L"Formatting..." : L"Format Drive Now", feFill, Color(255, 255, 255, 255), s_hoveredAction == ACT_FORMAT_EXECUTE, s_pressedAction == ACT_FORMAT_EXECUTE, Theme::ThemeManager::fontBodyBold, 2.5f);
        break;
    }

    case VIEW_ABOUT_APP: { // ─── Page 7: About Bootelware ───
        Theme::ThemeManager::DrawMascot(g, 45, 85, 68, false, s_animTick);
        g.DrawString(L"About Bootelware", -1, Theme::ThemeManager::fontTitle, PointF(125, 95), &darkBrush);
        g.DrawString(L"Version 1.0 · portable, no install needed", -1, Theme::ThemeManager::fontSubtitle, PointF(125, 130), &subBrush);

        g.DrawString(L"Bootelware turns a USB stick into an installer for Windows or Linux, in four easy steps.", -1, Theme::ThemeManager::fontLarge, PointF(45, 175), &darkBrush);

        // 3 Cards
        float cW = (w - 130.0f) / 3.0f;
        float cY = 220.0f, cH = 150.0f;

        auto drawFeatureCard = [&](float x, const wchar_t* title, const wchar_t* desc) {
            RectF rc(x, cY, cW, cH);
            Theme::ThemeManager::DrawNeoCard(g, rc, Color(255, 255, 255, 255));
            g.DrawString(title, -1, Theme::ThemeManager::fontLarge, PointF(x + 20, cY + 22), &darkBrush);
            RectF descRc(x + 20, cY + 60, cW - 40, 75);
            g.DrawString(desc, -1, Theme::ThemeManager::fontBody, descRc, &sfLeft, &subBrush);
        };

        drawFeatureCard(45.0f, L"Smart setup", L"Picks GPT or MBR and the right file system automatically for your PC.");
        drawFeatureCard(45.0f + cW + 20.0f, L"Windows extras", L"Skip hardware checks (TPM, Secure Boot, RAM) and the mandatory online account.");
        drawFeatureCard(45.0f + (cW + 20.0f) * 2.0f, L"Drive safety", L"Your primary Windows system disk (C:) is strictly locked from selection.");

        RectF backRc(45, h - 75.0f, 95.0f, 42.0f);
        AddHitArea(backRc, ACT_ABOUT_APP_BACK);
        Color bFill = (s_hoveredAction == ACT_ABOUT_APP_BACK) ? Color(255, 245, 246, 253) : Color(255, 255, 255, 255);
        Theme::ThemeManager::DrawNeoButton(g, backRc, L"Back", bFill, Color(255, 24, 24, 36), s_hoveredAction == ACT_ABOUT_APP_BACK, s_pressedAction == ACT_ABOUT_APP_BACK, Theme::ThemeManager::fontBodyBold, 2.0f);
        break;
    }

    case VIEW_ABOUT_DEV: { // ─── Page 8: About Developer ───
        Theme::ThemeManager::DrawDevAvatar(g, 45, 85, 68);
        g.DrawString(L"About the developer", -1, Theme::ThemeManager::fontTitle, PointF(125, 95), &darkBrush);
        g.DrawString(L"Md Saim · Software Engineer & Creator", -1, Theme::ThemeManager::fontSubtitle, PointF(125, 130), &subBrush);

        RectF storyRc(45, 175, w - 90.0f, 40);
        g.DrawString(L"I built Bootelware to make creating bootable USBs fast, fun, and foolproof — without the complex clutter, confusing options, or partition headaches.", -1, Theme::ThemeManager::fontBody, storyRc, &sfLeft, &darkBrush);

        // 3 Contact Cards
        float cW = (w - 130.0f) / 3.0f;
        float cY = 235.0f, cH = 54.0f;

        auto drawContactCard = [&](float x, const wchar_t* title, ButtonAction act) {
            RectF rc(x, cY, cW, cH);
            AddHitArea(rc, act);
            Color fill = (s_hoveredAction == act) ? Color(255, 245, 246, 253) : Color(255, 255, 255, 255);
            Theme::ThemeManager::DrawNeoButton(g, rc, title, fill, Color(255, 24, 24, 36), s_hoveredAction == act, s_pressedAction == act, Theme::ThemeManager::fontBodyBold, 1.8f);
        };

        drawContactCard(45.0f, L"Website: github.com/Md-Saim", ACT_DEV_LINK_WEB);
        drawContactCard(45.0f + cW + 20.0f, L"GitHub: @Md-Saim", ACT_DEV_LINK_GH);
        drawContactCard(45.0f + (cW + 20.0f) * 2.0f, L"Email: lakhvisaim@gmail.com", ACT_DEV_LINK_EMAIL);

        RectF backRc(45, h - 75.0f, 95.0f, 42.0f);
        AddHitArea(backRc, ACT_ABOUT_DEV_BACK);
        Color bFill = (s_hoveredAction == ACT_ABOUT_DEV_BACK) ? Color(255, 245, 246, 253) : Color(255, 255, 255, 255);
        Theme::ThemeManager::DrawNeoButton(g, backRc, L"Back", bFill, Color(255, 24, 24, 36), s_hoveredAction == ACT_ABOUT_DEV_BACK, s_pressedAction == ACT_ABOUT_DEV_BACK, Theme::ThemeManager::fontBodyBold, 2.0f);

        RectF thanksRc(w - 185.0f, h - 75.0f, 140.0f, 42.0f);
        AddHitArea(thanksRc, ACT_ABOUT_DEV_THANKS);
        Color tFill = (s_hoveredAction == ACT_ABOUT_DEV_THANKS) ? Color(255, 255, 224, 88) : Color(255, 254, 210, 50);
        Theme::ThemeManager::DrawNeoButton(g, thanksRc, L"Say thanks ★", tFill, Color(255, 24, 24, 36), s_hoveredAction == ACT_ABOUT_DEV_THANKS, s_pressedAction == ACT_ABOUT_DEV_THANKS, Theme::ThemeManager::fontBodyBold, 2.5f);
        break;
    }
    }

    // ─── Render Menu Popup (Page 6) ──────────────────────────────────────────
    if (s_showMenu) {
        float popW = 310.0f, popH = 220.0f;
        float popX = w - popW - 40.0f, popY = 62.0f;
        RectF menuCardRc(popX, popY, popW, popH);

        // Soft offset shadow
        SolidBrush shadowBrush(Color(25, 24, 24, 36));
        g.FillRectangle(&shadowBrush, popX + 4, popY + 4, popW, popH);

        // Menu background
        Theme::ThemeManager::DrawNeoCard(g, menuCardRc, Color(255, 255, 255, 255), Color(255, 24, 24, 36), 18.0f);

        auto drawMenuItem = [&](float y, const wchar_t* title, const wchar_t* subtitle, ButtonAction act) {
            RectF itemRc(popX + 12.0f, y, popW - 24.0f, 44.0f);
            AddHitArea(itemRc, act);
            Color fill = (s_hoveredAction == act) ? Color(255, 245, 246, 253) : Color(255, 255, 255, 255);
            Theme::ThemeManager::DrawNeoCard(g, itemRc, fill, Color(255, 24, 24, 36), 12.0f, 1.5f);

            g.DrawString(title, -1, Theme::ThemeManager::fontBodyBold, PointF(popX + 26.0f, y + 6.0f), &darkBrush);
            g.DrawString(subtitle, -1, Theme::ThemeManager::fontSmall, PointF(popX + 26.0f, y + 24.0f), &subBrush);
        };

        drawMenuItem(popY + 14.0f, L"About Bootelware", L"What it is and what it does", ACT_MENU_ABOUT_APP);
        drawMenuItem(popY + 64.0f, L"About the developer", L"Who made it", ACT_MENU_ABOUT_DEV);
        drawMenuItem(popY + 114.0f, L"⚡ Format USB Drive", L"Large FAT32 (>32GB), NTFS, exFAT", ACT_MENU_FORMAT_TOOL);
        drawMenuItem(popY + 164.0f, L"Calculate Checksums", L"Verify ISO cryptographic hashes", ACT_MENU_CHECKSUMS);
    }
}

// ─── Button Click Router ─────────────────────────────────────────────────────
static void HandleActionClick(ButtonAction act, HWND hWnd) {
    // If menu is open and clicked outside, close menu
    if (s_showMenu && act != ACT_MENU_BTN && act != ACT_MENU_ABOUT_APP && act != ACT_MENU_ABOUT_DEV && act != ACT_MENU_FORMAT_TOOL && act != ACT_MENU_CHECKSUMS) {
        s_showMenu = false;
        InvalidateRect(hWnd, NULL, FALSE);
        return;
    }

    switch (act) {
    case ACT_LOGO:
        s_currentView = VIEW_SOURCE;
        break;

    case ACT_STEP_1:
        s_currentView = VIEW_SOURCE;
        break;
    case ACT_STEP_2:
        if (s_isoLoaded) s_currentView = VIEW_DRIVE;
        break;
    case ACT_STEP_3:
        if (s_isoLoaded && s_selectedDriveIndex >= 0) s_currentView = VIEW_TUNE;
        break;
    case ACT_STEP_4:
        if (s_isWriting) s_currentView = VIEW_WRITE;
        break;

    case ACT_MENU_BTN:
        s_showMenu = !s_showMenu;
        break;

    case ACT_MENU_ABOUT_APP:
        s_showMenu = false;
        s_previousView = s_currentView;
        s_currentView = VIEW_ABOUT_APP;
        break;

    case ACT_MENU_ABOUT_DEV:
        s_showMenu = false;
        s_previousView = s_currentView;
        s_currentView = VIEW_ABOUT_DEV;
        break;

    case ACT_MENU_FORMAT_TOOL:
        s_showMenu = false;
        s_previousView = s_currentView;
        s_currentView = VIEW_FORMAT;
        break;

    case ACT_MENU_CHECKSUMS:
        s_showMenu = false;
        Dialogs::ShowChecksumDialog(hWnd, s_isoPath);
        break;

    // Step 1: Source
    case ACT_BROWSE_ISO:
    case ACT_CHANGE_ISO:
        BrowseForIso();
        break;

    case ACT_CONTINUE_ISO:
        if (s_isoLoaded) {
            RefreshDevices();
            s_currentView = VIEW_DRIVE;
        }
        break;

    case ACT_GET_WIN11:
        ShellExecuteW(NULL, L"open", L"https://www.microsoft.com/software-download/windows11", NULL, NULL, SW_SHOWNORMAL);
        break;
    case ACT_GET_WIN10:
        ShellExecuteW(NULL, L"open", L"https://www.microsoft.com/software-download/windows10", NULL, NULL, SW_SHOWNORMAL);
        break;
    case ACT_GET_UBUNTU:
        ShellExecuteW(NULL, L"open", L"https://ubuntu.com/download/desktop", NULL, NULL, SW_SHOWNORMAL);
        break;
    case ACT_GET_OTHER:
        Dialogs::ShowDownloadDialog(hWnd);
        break;

    // Step 2: Drive
    case ACT_SELECT_DRIVE_CARD:
        // Cycle drives if multiple are available
        if (s_drives.size() > 1) {
            s_selectedDriveIndex = (s_selectedDriveIndex + 1) % (int)s_drives.size();
        }
        break;

    case ACT_RESCAN_DRIVES:
        RefreshDevices();
        break;

    case ACT_GOTO_FORMAT_USB:
        s_previousView = s_currentView;
        s_currentView = VIEW_FORMAT;
        break;

    case ACT_DRIVE_BACK:
        s_currentView = VIEW_SOURCE;
        break;

    case ACT_DRIVE_NEXT:
        if (s_selectedDriveIndex >= 0 && s_selectedDriveIndex < (int)s_drives.size()) {
            s_currentView = VIEW_TUNE;
        } else {
            MessageBoxW(hWnd, L"Please connect and select a USB drive to continue.", L"No Drive Selected", MB_OK | MB_ICONWARNING);
        }
        break;

    // Step 3: Tune
    case ACT_OPT_OLDER_PCS:
        s_optOlderPCs = !s_optOlderPCs;
        break;
    case ACT_OPT_NO_MS_ACCOUNT:
        s_optNoMsAccount = !s_optNoMsAccount;
        break;
    case ACT_OPT_LOCAL_USER:
        s_optLocalUser = !s_optLocalUser;
        if (s_optLocalUser) {
            PromptTextInput(hWnd, L"Local Username", L"Enter custom username for offline setup:", s_localUserName);
        }
        break;
    case ACT_OPT_SKIP_ENCRYPTION:
        s_optSkipEncryption = !s_optSkipEncryption;
        break;
    case ACT_OPT_PAUSE_TRACKING:
        s_optPauseTracking = !s_optPauseTracking;
        break;
    case ACT_OPT_USB_NAME:
        PromptTextInput(hWnd, L"USB Volume Name", L"Enter drive volume label:", s_usbVolumeName);
        break;

    case ACT_TUNE_SCHEME_GPT:
        s_isGpt = true;
        s_partitionScheme = L"GPT";
        s_targetSystem = L"UEFI (non-CSM)";
        break;

    case ACT_TUNE_SCHEME_MBR:
        s_isGpt = false;
        s_partitionScheme = L"MBR";
        s_targetSystem = L"BIOS or UEFI-CSM";
        break;

    case ACT_TUNE_FIX_MISMATCH:
        s_isGpt = s_hostInfo.isGpt;
        s_partitionScheme = s_hostInfo.recommendedScheme;
        s_targetSystem = s_hostInfo.recommendedTarget;
        break;

    case ACT_TUNE_BACK:
        s_currentView = VIEW_DRIVE;
        break;

    case ACT_CREATE_USB:
        StartWriteJob();
        break;

    // Step 4: Write
    case ACT_TOGGLE_DETAILS:
        s_showDetails = !s_showDetails;
        break;

    case ACT_CANCEL_WRITE:
        if (MessageBoxW(hWnd, L"Are you sure you want to cancel the USB creation?", L"Cancel Operation", MB_YESNO | MB_ICONQUESTION) == IDYES) {
            s_cancelWrite = true;
        }
        break;

    // Step 5: Done
    case ACT_MAKE_ANOTHER:
        s_currentView = VIEW_SOURCE;
        break;

    case ACT_EJECT_USB:
        if (s_selectedDriveIndex >= 0 && s_selectedDriveIndex < (int)s_drives.size()) {
            std::wstring err;
            if (DiskWriter::EjectDrive(s_drives[s_selectedDriveIndex].assignedDriveLetters, err)) {
                MessageBoxW(hWnd, L"The USB drive has been safely ejected and is ready to be unplugged.", L"Ejection Complete", MB_OK | MB_ICONINFORMATION);
            } else {
                MessageBoxW(hWnd, err.c_str(), L"Ejection Notice", MB_OK | MB_ICONWARNING);
            }
        }
        break;

    // About App
    case ACT_ABOUT_APP_BACK:
        s_currentView = s_previousView;
        break;

    // About Dev
    case ACT_ABOUT_DEV_BACK:
        s_currentView = s_previousView;
        break;
    case ACT_ABOUT_DEV_THANKS:
    case ACT_DEV_LINK_GH:
        ShellExecuteW(NULL, L"open", L"https://github.com/Md-Saim/Bootelware", NULL, NULL, SW_SHOWNORMAL);
        break;
    case ACT_DEV_LINK_WEB:
        ShellExecuteW(NULL, L"open", L"https://github.com/Md-Saim", NULL, NULL, SW_SHOWNORMAL);
        break;
    case ACT_DEV_LINK_EMAIL:
        ShellExecuteW(NULL, L"open", L"mailto:lakhvisaim@gmail.com", NULL, NULL, SW_SHOWNORMAL);
        break;

    // Format Tool
    case ACT_FORMAT_FS_FAT32:
        s_formatFs = L"FAT32";
        break;
    case ACT_FORMAT_FS_NTFS:
        s_formatFs = L"NTFS";
        break;
    case ACT_FORMAT_FS_EXFAT:
        s_formatFs = L"exFAT";
        break;
    case ACT_FORMAT_CHANGE_DRIVE:
        RefreshDevices();
        if (s_drives.size() > 1) {
            s_selectedDriveIndex = (s_selectedDriveIndex + 1) % (int)s_drives.size();
        }
        break;
    case ACT_FORMAT_EXECUTE:
        StartFormatJob();
        break;
    case ACT_FORMAT_BACK:
        s_currentView = s_previousView;
        break;

    default:
        break;
    }

    InvalidateRect(hWnd, NULL, FALSE);
}

// ─── WndProc Implementation ──────────────────────────────────────────────────
LRESULT CALLBACK MainWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        s_hWnd = hWnd;
        Theme::ThemeManager::Init();

        // Query Host System detection
        s_hostInfo = SystemDetector::DetectSystemDrive();
        s_isGpt = s_hostInfo.isGpt;
        s_partitionScheme = s_hostInfo.recommendedScheme;
        s_targetSystem = s_hostInfo.recommendedTarget;

        RefreshDevices();
        DragAcceptFiles(hWnd, TRUE);

        // 30 FPS animation timer for mascot, progress shimmer, and pulsing dots
        SetTimer(hWnd, 999, 33, NULL);
        return 0;
    }

    case WM_TIMER: {
        if (wParam == 999) {
            s_animTick++;
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT clientRc;
        GetClientRect(hWnd, &clientRc);
        int w = clientRc.right - clientRc.left;
        int h = clientRc.bottom - clientRc.top;

        // Double buffer to eliminate flicker
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
        HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

        {
            Graphics g(memDC);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

            DrawAll(g, w, h);
        }

        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE: {
        float mx = (float)LOWORD(lParam);
        float my = (float)HIWORD(lParam);

        ButtonAction hitAct = ACT_NONE;
        for (const auto& ha : s_hitAreas) {
            if (ha.rect.Contains(PointF(mx, my))) {
                hitAct = ha.action;
                break;
            }
        }

        if (hitAct != s_hoveredAction) {
            s_hoveredAction = hitAct;
            InvalidateRect(hWnd, NULL, FALSE);
        }

        if (hitAct != ACT_NONE) {
            SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_HAND));
        } else {
            SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_ARROW));
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        float mx = (float)LOWORD(lParam);
        float my = (float)HIWORD(lParam);

        ButtonAction clickedAct = ACT_NONE;
        for (const auto& ha : s_hitAreas) {
            if (ha.rect.Contains(PointF(mx, my))) {
                clickedAct = ha.action;
                break;
            }
        }

        s_pressedAction = clickedAct;
        InvalidateRect(hWnd, NULL, FALSE);
        return 0;
    }

    case WM_LBUTTONUP: {
        float mx = (float)LOWORD(lParam);
        float my = (float)HIWORD(lParam);

        ButtonAction releasedAct = ACT_NONE;
        for (const auto& ha : s_hitAreas) {
            if (ha.rect.Contains(PointF(mx, my))) {
                releasedAct = ha.action;
                break;
            }
        }

        ButtonAction actToTrigger = (releasedAct == s_pressedAction) ? releasedAct : ACT_NONE;
        s_pressedAction = ACT_NONE;

        if (actToTrigger != ACT_NONE) {
            HandleActionClick(actToTrigger, hWnd);
        } else if (s_showMenu) {
            s_showMenu = false;
            InvalidateRect(hWnd, NULL, FALSE);
        } else {
            InvalidateRect(hWnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_DROPFILES: {
        HDROP hDrop = (HDROP)wParam;
        wchar_t droppedPath[MAX_PATH] = { 0 };
        if (DragQueryFileW(hDrop, 0, droppedPath, MAX_PATH)) {
            SelectIsoFile(droppedPath);
            s_currentView = VIEW_SOURCE;
        }
        DragFinish(hDrop);
        InvalidateRect(hWnd, NULL, FALSE);
        return 0;
    }

    case WM_USER + 200: {
        InvalidateRect(hWnd, NULL, FALSE);
        return 0;
    }

    case WM_DEVICECHANGE: {
        // Automatically detect USB insertion/removal
        RefreshDevices();
        return 0;
    }

    case WM_DESTROY: {
        Theme::ThemeManager::Shutdown();
        PostQuitMessage(0);
        return 0;
    }
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

bool MainWindow::Register(HINSTANCE hInstance) {
    WNDCLASSEXW wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = MainWindow::WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wcex.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wcex.lpszClassName = L"BootelwareMainWindow";
    wcex.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));

    return RegisterClassExW(&wcex) != 0;
}

HWND MainWindow::Create(HINSTANCE hInstance) {
    const int W = 940;
    const int H = 600;

    RECT rc = { 0, 0, W, H };
    AdjustWindowRectEx(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, 0);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - (rc.right - rc.left)) / 2;
    int y = (screenH - (rc.bottom - rc.top)) / 2;

    HWND hWnd = CreateWindowExW(
        0,
        L"BootelwareMainWindow",
        L"Bootelware",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y,
        rc.right - rc.left,
        rc.bottom - rc.top,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    return hWnd;
}

static int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0;
    UINT size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL) return -1;
    GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }
    free(pImageCodecInfo);
    return -1;
}

bool MainWindow::ExportScreenshots(const std::wstring& outputDir) {
    Theme::ThemeManager::Init();

    CreateDirectoryW(outputDir.c_str(), NULL);

    CLSID pngClsid;
    int encRes = GetEncoderClsid(L"image/png", &pngClsid);
    if (encRes < 0) {
        return false;
    }

    const int W = 940, H = 600;

    // Ensure mock drive data for realistic screen rendering
    if (s_drives.empty()) {
        TargetDeviceInfo dummy;
        dummy.assignedDriveLetters = L"E:";
        dummy.model = L"ProductCode";
        dummy.totalSizeGb = 58.6;
        dummy.currentFileSystem = L"FAT32";
        s_drives.push_back(dummy);
        s_selectedDriveIndex = 0;
    }
    s_isoMeta.detectedType = IsoType::Windows11;
    s_isoMeta.osName = L"Windows 11 (Setup)";
    s_isoMeta.fileSizeGb = 5.4;
    s_isoPath = L"C:\\Downloads\\Win11_23H2_English_x64v2.iso";

    auto renderAndSave = [&](AppView view, bool menuOpen, const std::wstring& filename, int progress = 0) {
        s_currentView = view;
        s_showMenu = menuOpen;
        if (view == VIEW_WRITE && s_currentTaskDesc.empty()) {
            s_progressPercent = progress > 0 ? progress : 64;
            s_progressSpeed = 24.5;
            s_currentTaskDesc = L"Copying files";
            s_chkCleaning = 2;
            s_chkCopying = 1;
            s_chkExtras = 0;
            s_chkDoubleChecking = 0;
        }

        Bitmap bmp(W, H, PixelFormat32bppARGB);
        {
            Graphics g(&bmp);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
            DrawAll(g, W, H);
        }
        std::wstring outPath = outputDir;
        if (outPath.back() != L'\\') outPath += L'\\';
        outPath += filename;
        bmp.Save(outPath.c_str(), &pngClsid, NULL);
    };

    // Screen 1: Source (Blank dropzone)
    s_isoLoaded = false;
    renderAndSave(VIEW_SOURCE, false, L"screen1_source.png");

    // Screen 2: Drive
    s_isoLoaded = true;
    renderAndSave(VIEW_DRIVE, false, L"screen2_drive.png");

    // Screen 3: Tune (Windows extras - matched GPT)
    s_isGpt = s_hostInfo.isGpt;
    renderAndSave(VIEW_TUNE, false, L"screen3_tune.png");

    // Screen 3b: Tune with Mismatch Warning Pop
    s_isGpt = !s_hostInfo.isGpt;
    renderAndSave(VIEW_TUNE, false, L"screen3_tune_mismatch.png");
    s_isGpt = s_hostInfo.isGpt;

    // Screen 4: Write (In progress 64%)
    s_showDetails = false;
    s_currentTaskDesc = L"Copying files";
    s_progressPercent = 64;
    s_progressSpeed = 24.5;
    s_chkCleaning = 2;
    s_chkCopying = 1;
    s_chkExtras = 0;
    s_chkDoubleChecking = 0;
    renderAndSave(VIEW_WRITE, false, L"screen4_write.png", 64);

    // Screen 4b: Write with Details & long GUID task description (verifying no text leaking)
    s_showDetails = true;
    s_currentTaskDesc = L"Writing {cdd5cb55-db68-4d71-aa38-3df2b6473a52}.iso";
    s_progressPercent = 0;
    s_progressSpeed = 1.3;
    s_chkCleaning = 1;
    s_chkCopying = 0;
    s_chkExtras = 0;
    s_chkDoubleChecking = 0;
    s_logLines.clear();
    s_logLines.push_back(L"Custom FAT32 Geometry: Size=59998 MB, Total Sectors=122877888, Cluster Size=4096 bytes (8 sectors/cluster)");
    s_logLines.push_back(L"✓ FAT32 format complete! Drive E: is now formatted as FAT32.");
    s_logLines.push_back(L"Mounting ISO image via Windows Virtual Disk Service...");
    s_logLines.push_back(L"ISO mounted successfully at virtual drive F:\\");
    s_logLines.push_back(L"Extracting and copying files to USB target drive...");
    renderAndSave(VIEW_WRITE, false, L"screen4_write_details.png", 0);
    s_showDetails = false;
    s_currentTaskDesc = L"";

    // Screen 5: Done (USB ready)
    renderAndSave(VIEW_DONE, false, L"screen5_done.png");

    // Screen 6: Menu Popup (Page 6)
    s_isoLoaded = false;
    renderAndSave(VIEW_SOURCE, true, L"screen6_menu.png");

    // Screen 7: About Bootelware (Page 7)
    renderAndSave(VIEW_ABOUT_APP, false, L"screen7_about.png");

    // Screen 8: About Developer (Page 8)
    renderAndSave(VIEW_ABOUT_DEV, false, L"screen8_developer.png");

    // Feature Screen: Standalone Rufus Format USB
    renderAndSave(VIEW_FORMAT, false, L"screen_format_usb.png");

    Theme::ThemeManager::Shutdown();
    return true;
}

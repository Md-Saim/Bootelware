#pragma once
#include <windows.h>
#include <string>
#include "../core/win11_bypass.h"

class Dialogs {
public:
    static bool ShowWin11OptionsDialog(HWND hParent, Win11BypassOptions& inOutOpts);
    static void ShowChecksumDialog(HWND hParent, const std::wstring& isoPath);
    static void ShowDownloadDialog(HWND hParent);
};

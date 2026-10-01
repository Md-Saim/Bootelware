#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <atomic>
#include "../core/system_detector.h"
#include "../core/device_manager.h"
#include "../core/iso_reader.h"
#include "../core/win11_bypass.h"

class MainWindow {
public:
    static bool Register(HINSTANCE hInstance);
    static HWND Create(HINSTANCE hInstance);

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK DrawerWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Initialization & Event Handlers
    static void OnInit(HWND hWnd);
    static void InitDrawer(HWND hWnd);
    static void ToggleDrawer(bool open);
    static void LayoutDrawerContent();
    static void RefreshSystemDetection(HWND hWnd);
    static void RefreshDeviceList(HWND hWnd);
    static void OnSelectIso(HWND hWnd);
    static void OnSchemeChanged(HWND hWnd);
    static void OnStartClicked(HWND hWnd);
    static void LayoutMainWindow(HWND hWnd);
    static void OnToggleAdvanced(HWND hWnd);
    static void OnToggleLog(HWND hWnd);
    static void OnCopyLog(HWND hWnd);

    // Logging & Progress
    static void AppendLog(const std::wstring& text);
    static void UpdateProgress(int percent, double speedMb, const std::wstring& taskDesc);
};

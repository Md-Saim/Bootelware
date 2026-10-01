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
    static bool ExportScreenshots(const std::wstring& outputDir);

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

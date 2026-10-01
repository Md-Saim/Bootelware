#pragma once
#include <windows.h>
#include <dwmapi.h>
#include <string>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

namespace Theme {
    // ─── Premium Dark Color Palette ───
    // Base backgrounds (deep blue-black tones, not pure gray)
    const COLORREF BG_DARK         = RGB(13, 17, 23);       // GitHub-dark inspired base
    const COLORREF BG_SURFACE      = RGB(22, 27, 34);       // Elevated surface
    const COLORREF BG_DRAWER       = RGB(18, 22, 28);       // Drawer background
    const COLORREF BG_CARD         = RGB(30, 36, 46);       // Card / Panel background
    const COLORREF BG_SECTION      = RGB(26, 32, 40);       // Section header bg
    const COLORREF BG_INPUT        = RGB(18, 22, 30);       // Input field bg
    const COLORREF BG_HOVER        = RGB(36, 42, 54);       // Hover state bg

    // Text
    const COLORREF TEXT_PRIMARY    = RGB(240, 246, 252);     // High-contrast white
    const COLORREF TEXT_SECONDARY  = RGB(139, 148, 158);     // Subdued gray-blue
    const COLORREF TEXT_MUTED      = RGB(88, 96, 105);       // Dim muted
    const COLORREF TEXT_LABEL      = RGB(201, 209, 217);     // Label text

    // Accent colors
    const COLORREF ACCENT_BLUE     = RGB(31, 111, 235);      // Vivid action blue
    const COLORREF ACCENT_HOVER    = RGB(56, 132, 244);      // Blue hover
    const COLORREF ACCENT_PRESSED  = RGB(23, 85, 190);       // Blue pressed
    const COLORREF ACCENT_GREEN    = RGB(35, 197, 94);       // Emerald success
    const COLORREF ACCENT_GREEN_DIM= RGB(22, 60, 42);        // Emerald background
    const COLORREF ACCENT_CYAN     = RGB(56, 189, 248);      // Sky cyan for highlights
    const COLORREF ACCENT_PURPLE   = RGB(163, 113, 247);     // Subtle purple accent
    const COLORREF ACCENT_ORANGE   = RGB(255, 166, 87);      // Warm orange

    // Banner
    const COLORREF BANNER_BG       = RGB(14, 56, 42);        // Deep emerald
    const COLORREF BANNER_BORDER   = RGB(35, 134, 92);       // Emerald border

    // Warning / Danger
    const COLORREF WARNING_BG      = RGB(66, 38, 12);        // Deep amber
    const COLORREF WARNING_TEXT    = RGB(255, 202, 40);       // Amber text
    const COLORREF WARNING_BORDER = RGB(180, 120, 30);       // Amber border
    const COLORREF DANGER_BG       = RGB(56, 18, 18);        // Deep red
    const COLORREF DANGER_TEXT     = RGB(248, 81, 73);        // Red text

    // Borders & Dividers
    const COLORREF BORDER_COLOR    = RGB(48, 54, 61);        // Standard border
    const COLORREF BORDER_FOCUS    = RGB(31, 111, 235);      // Focus ring
    const COLORREF BORDER_SUBTLE   = RGB(33, 38, 45);        // Very subtle divider

    // Buttons
    const COLORREF BTN_NORMAL      = RGB(33, 38, 48);        // Normal button
    const COLORREF BTN_HOVER       = RGB(42, 48, 60);        // Hovered button
    const COLORREF BTN_PRESSED     = RGB(24, 28, 36);        // Pressed button
    const COLORREF BTN_START_BG    = RGB(21, 128, 61);       // Green START button
    const COLORREF BTN_START_HOVER = RGB(34, 160, 80);       // START hover
    const COLORREF BTN_DANGER_BG   = RGB(153, 27, 27);       // Destructive button

    // Scrollbar / Progress
    const COLORREF PROGRESS_BG     = RGB(22, 27, 34);        // Progress track
    const COLORREF PROGRESS_FILL   = RGB(31, 111, 235);      // Progress bar fill

    class ThemeManager {
    public:
        static void EnableDarkMode(HWND hWnd);
        static void InitGDI();
        static void CleanupGDI();

        // Rendering helpers
        static void RenderModernButton(
            LPDRAWITEMSTRUCT dis,
            const std::wstring& text,
            bool isAccent = false,
            bool isDestructive = false,
            bool isSmall = false
        );

        static void RenderStartButton(
            LPDRAWITEMSTRUCT dis,
            const std::wstring& text,
            bool isDisabled = false
        );

        static void FillRoundedRect(HDC hdc, const RECT& rc, int radius, COLORREF fill, COLORREF border);
        static void DrawGradientRect(HDC hdc, const RECT& rc, COLORREF top, COLORREF bottom);
        static void DrawSectionHeader(HDC hdc, int x, int y, int w, const wchar_t* text);
        static void DrawBadge(HDC hdc, int x, int y, const wchar_t* text, COLORREF bg, COLORREF border, COLORREF textCol, HFONT hFont = nullptr);
        static void DrawCard(HDC hdc, const RECT& rc, const wchar_t* title, const wchar_t* badge = nullptr, COLORREF accentColor = ACCENT_BLUE);
        static void RenderHeroButton(LPDRAWITEMSTRUCT dis, const std::wstring& text, bool isDisabled = false);

        // GDI Objects
        static HBRUSH hbrBackground;
        static HBRUSH hbrSurface;
        static HBRUSH hbrDrawer;
        static HBRUSH hbrCard;
        static HBRUSH hbrSection;
        static HBRUSH hbrInput;
        static HBRUSH hbrBanner;
        static HBRUSH hbrWarning;
        static HBRUSH hbrAccent;

        // Fonts (expanded set)
        static HFONT hFontTiny;
        static HFONT hFontSmall;
        static HFONT hFontRegular;
        static HFONT hFontMedium;
        static HFONT hFontBold;
        static HFONT hFontTitle;
        static HFONT hFontLargeTitle;
        static HFONT hFontHero;
        static HFONT hFontMono;
        static HFONT hFontMonoSmall;
    };
}

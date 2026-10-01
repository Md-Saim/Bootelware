#include "dark_theme.h"

#pragma comment(lib, "dwmapi.lib")

namespace Theme {
    HBRUSH ThemeManager::hbrBackground = NULL;
    HBRUSH ThemeManager::hbrSurface    = NULL;
    HBRUSH ThemeManager::hbrDrawer     = NULL;
    HBRUSH ThemeManager::hbrCard       = NULL;
    HBRUSH ThemeManager::hbrSection    = NULL;
    HBRUSH ThemeManager::hbrInput      = NULL;
    HBRUSH ThemeManager::hbrBanner     = NULL;
    HBRUSH ThemeManager::hbrWarning    = NULL;
    HBRUSH ThemeManager::hbrAccent     = NULL;

    HFONT ThemeManager::hFontTiny       = NULL;
    HFONT ThemeManager::hFontSmall      = NULL;
    HFONT ThemeManager::hFontRegular    = NULL;
    HFONT ThemeManager::hFontMedium     = NULL;
    HFONT ThemeManager::hFontBold       = NULL;
    HFONT ThemeManager::hFontTitle      = NULL;
    HFONT ThemeManager::hFontLargeTitle = NULL;
    HFONT ThemeManager::hFontHero       = NULL;
    HFONT ThemeManager::hFontMono       = NULL;
    HFONT ThemeManager::hFontMonoSmall  = NULL;

    void ThemeManager::EnableDarkMode(HWND hWnd) {
        BOOL darkMode = TRUE;
        DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
    }

    void ThemeManager::InitGDI() {
        if (!hbrBackground) {
            hbrBackground = CreateSolidBrush(BG_DARK);
            hbrSurface    = CreateSolidBrush(BG_SURFACE);
            hbrDrawer     = CreateSolidBrush(BG_DRAWER);
            hbrCard       = CreateSolidBrush(BG_CARD);
            hbrSection    = CreateSolidBrush(BG_SECTION);
            hbrInput      = CreateSolidBrush(BG_INPUT);
            hbrBanner     = CreateSolidBrush(BANNER_BG);
            hbrWarning    = CreateSolidBrush(WARNING_BG);
            hbrAccent     = CreateSolidBrush(ACCENT_BLUE);

            // Segoe UI Variable is available on Win11, fallback to Segoe UI
            const wchar_t* uiFont = L"Segoe UI";
            const wchar_t* monoFont = L"Cascadia Mono";

            hFontTiny       = CreateFontW(-10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uiFont);
            hFontSmall      = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uiFont);
            hFontRegular    = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uiFont);
            hFontMedium     = CreateFontW(-13, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uiFont);
            hFontBold       = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uiFont);
            hFontTitle      = CreateFontW(-16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uiFont);
            hFontLargeTitle = CreateFontW(-20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uiFont);
            hFontHero       = CreateFontW(-26, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uiFont);
            hFontMono       = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, monoFont);
            hFontMonoSmall  = CreateFontW(-10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, monoFont);
        }
    }

    void ThemeManager::CleanupGDI() {
        HGDIOBJ* objs[] = {
            (HGDIOBJ*)&hbrBackground, (HGDIOBJ*)&hbrSurface, (HGDIOBJ*)&hbrDrawer,
            (HGDIOBJ*)&hbrCard, (HGDIOBJ*)&hbrSection, (HGDIOBJ*)&hbrInput,
            (HGDIOBJ*)&hbrBanner, (HGDIOBJ*)&hbrWarning, (HGDIOBJ*)&hbrAccent,
            (HGDIOBJ*)&hFontTiny, (HGDIOBJ*)&hFontSmall, (HGDIOBJ*)&hFontRegular,
            (HGDIOBJ*)&hFontMedium, (HGDIOBJ*)&hFontBold, (HGDIOBJ*)&hFontTitle,
            (HGDIOBJ*)&hFontLargeTitle, (HGDIOBJ*)&hFontHero,
            (HGDIOBJ*)&hFontMono, (HGDIOBJ*)&hFontMonoSmall
        };
        for (auto* p : objs) {
            if (*p) { DeleteObject(*p); *p = NULL; }
        }
    }

    void ThemeManager::FillRoundedRect(HDC hdc, const RECT& rc, int radius, COLORREF fill, COLORREF border) {
        HBRUSH hBrush = CreateSolidBrush(fill);
        HPEN hPen = CreatePen(PS_SOLID, 1, border);
        HGDIOBJ oldBrush = SelectObject(hdc, hBrush);
        HGDIOBJ oldPen = SelectObject(hdc, hPen);
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(hBrush);
        DeleteObject(hPen);
    }

    void ThemeManager::DrawGradientRect(HDC hdc, const RECT& rc, COLORREF top, COLORREF bottom) {
        TRIVERTEX vertices[2];
        vertices[0].x = rc.left;
        vertices[0].y = rc.top;
        vertices[0].Red   = (COLOR16)(GetRValue(top) << 8);
        vertices[0].Green = (COLOR16)(GetGValue(top) << 8);
        vertices[0].Blue  = (COLOR16)(GetBValue(top) << 8);
        vertices[0].Alpha = 0;

        vertices[1].x = rc.right;
        vertices[1].y = rc.bottom;
        vertices[1].Red   = (COLOR16)(GetRValue(bottom) << 8);
        vertices[1].Green = (COLOR16)(GetGValue(bottom) << 8);
        vertices[1].Blue  = (COLOR16)(GetBValue(bottom) << 8);
        vertices[1].Alpha = 0;

        GRADIENT_RECT gRect = { 0, 1 };
        GradientFill(hdc, vertices, 2, &gRect, 1, GRADIENT_FILL_RECT_V);
    }

    void ThemeManager::DrawSectionHeader(HDC hdc, int x, int y, int w, const wchar_t* text) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, TEXT_LABEL);
        SelectObject(hdc, hFontMedium);
        TextOutW(hdc, x, y, text, (int)wcslen(text));

        // Draw a subtle line after the text
        SIZE sz;
        GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &sz);
        HPEN hPen = CreatePen(PS_SOLID, 1, BORDER_SUBTLE);
        HGDIOBJ oldPen = SelectObject(hdc, hPen);
        MoveToEx(hdc, x + sz.cx + 8, y + sz.cy / 2, NULL);
        LineTo(hdc, x + w, y + sz.cy / 2);
        SelectObject(hdc, oldPen);
        DeleteObject(hPen);
    }

    void ThemeManager::RenderModernButton(
        LPDRAWITEMSTRUCT dis,
        const std::wstring& text,
        bool isAccent,
        bool isDestructive,
        bool isSmall
    ) {
        HDC hdc = dis->hDC;
        RECT rc = dis->rcItem;
        bool isPressed = (dis->itemState & ODS_SELECTED) != 0;
        bool isFocused = (dis->itemState & ODS_FOCUS) != 0;
        bool isDisabled = (dis->itemState & ODS_DISABLED) != 0;

        COLORREF fillColor = BTN_NORMAL;
        COLORREF borderColor = BORDER_COLOR;
        COLORREF textColor = TEXT_PRIMARY;

        if (isDisabled) {
            fillColor = BG_CARD;
            borderColor = BORDER_SUBTLE;
            textColor = TEXT_MUTED;
        } else if (isAccent) {
            fillColor = isPressed ? ACCENT_PRESSED : ACCENT_BLUE;
            borderColor = isPressed ? ACCENT_BLUE : ACCENT_HOVER;
            textColor = RGB(255, 255, 255);
        } else if (isDestructive) {
            fillColor = isPressed ? RGB(120, 20, 20) : BTN_NORMAL;
            borderColor = isPressed ? DANGER_TEXT : BORDER_COLOR;
            textColor = isPressed ? RGB(255, 255, 255) : DANGER_TEXT;
        } else if (isPressed) {
            fillColor = BTN_PRESSED;
            borderColor = ACCENT_BLUE;
            textColor = TEXT_PRIMARY;
        } else if (isFocused) {
            borderColor = ACCENT_BLUE;
        }

        // Draw rounded button background
        FillRoundedRect(hdc, rc, 6, fillColor, borderColor);

        // Draw button text
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, textColor);
        SelectObject(hdc, isSmall ? hFontSmall : hFontMedium);

        RECT textRc = rc;
        if (isPressed) {
            OffsetRect(&textRc, 0, 1);
        }

        DrawTextW(hdc, text.c_str(), -1, &textRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    void ThemeManager::RenderStartButton(
        LPDRAWITEMSTRUCT dis,
        const std::wstring& text,
        bool isDisabled
    ) {
        HDC hdc = dis->hDC;
        RECT rc = dis->rcItem;
        bool isPressed = (dis->itemState & ODS_SELECTED) != 0;

        COLORREF fillColor, borderColor, textColor;

        if (isDisabled) {
            fillColor = BG_CARD;
            borderColor = BORDER_SUBTLE;
            textColor = TEXT_MUTED;
        } else {
            fillColor = isPressed ? RGB(16, 100, 48) : BTN_START_BG;
            borderColor = isPressed ? ACCENT_GREEN : BTN_START_HOVER;
            textColor = RGB(255, 255, 255);
        }

        FillRoundedRect(hdc, rc, 8, fillColor, borderColor);

        // Subtle glow effect for enabled start button
        if (!isDisabled && !isPressed) {
            HPEN hGlow = CreatePen(PS_SOLID, 1, ACCENT_GREEN);
            HGDIOBJ oldPen = SelectObject(hdc, hGlow);
            SelectObject(hdc, GetStockObject(NULL_BRUSH));
            RECT inner = { rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1 };
            RoundRect(hdc, inner.left, inner.top, inner.right, inner.bottom, 7, 7);
            SelectObject(hdc, oldPen);
            DeleteObject(hGlow);
        }

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, textColor);
        SelectObject(hdc, hFontBold);

        RECT textRc = rc;
        if (isPressed) OffsetRect(&textRc, 0, 1);
        DrawTextW(hdc, text.c_str(), -1, &textRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    void ThemeManager::DrawBadge(HDC hdc, int x, int y, const wchar_t* text, COLORREF bg, COLORREF border, COLORREF textCol, HFONT hFont) {
        HFONT fontToUse = hFont ? hFont : hFontSmall;
        HGDIOBJ oldFont = SelectObject(hdc, fontToUse);
        SIZE sz;
        GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &sz);
        RECT br = { x, y, x + sz.cx + 12, y + sz.cy + 4 };
        FillRoundedRect(hdc, br, 4, bg, border);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, textCol);
        RECT tr = br;
        DrawTextW(hdc, text, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, oldFont);
    }

    void ThemeManager::DrawCard(HDC hdc, const RECT& rc, const wchar_t* title, const wchar_t* badge, COLORREF accentColor) {
        // Fill card background
        FillRoundedRect(hdc, rc, 10, BG_CARD, BORDER_COLOR);

        // Accent indicator bar on left edge
        HBRUSH hAccent = CreateSolidBrush(accentColor);
        RECT accentBar = { rc.left + 2, rc.top + 10, rc.left + 5, rc.top + 28 };
        FillRect(hdc, &accentBar, hAccent);
        DeleteObject(hAccent);

        // Title
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, accentColor);
        SelectObject(hdc, hFontBold);
        TextOutW(hdc, rc.left + 12, rc.top + 10, title, (int)wcslen(title));

        // Optional badge on top-right
        if (badge && badge[0]) {
            SIZE bsz;
            SelectObject(hdc, hFontSmall);
            GetTextExtentPoint32W(hdc, badge, (int)wcslen(badge), &bsz);
            int bx = rc.right - bsz.cx - 22;
            DrawBadge(hdc, bx, rc.top + 8, badge, BG_SURFACE, accentColor, accentColor, hFontSmall);
        }
    }

    void ThemeManager::RenderHeroButton(LPDRAWITEMSTRUCT dis, const std::wstring& text, bool isDisabled) {
        HDC hdc = dis->hDC;
        RECT rc = dis->rcItem;
        bool isPressed = (dis->itemState & ODS_SELECTED) != 0;

        if (isDisabled) {
            FillRoundedRect(hdc, rc, 8, BG_CARD, BORDER_SUBTLE);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, TEXT_MUTED);
            SelectObject(hdc, hFontTitle);
            RECT tr = rc;
            DrawTextW(hdc, text.c_str(), -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            return;
        }

        COLORREF topCol, botCol, borderCol;
        if (isPressed) {
            topCol = RGB(16, 120, 56);
            botCol = RGB(12, 85, 40);
            borderCol = ACCENT_GREEN;
        } else {
            topCol = RGB(34, 180, 88);
            botCol = RGB(20, 130, 62);
            borderCol = RGB(52, 211, 120);
        }

        DrawGradientRect(hdc, rc, topCol, botCol);

        // Outer border
        HPEN hPen = CreatePen(PS_SOLID, 1, borderCol);
        HGDIOBJ oldPen = SelectObject(hdc, hPen);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);
        SelectObject(hdc, oldPen);
        DeleteObject(hPen);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        SelectObject(hdc, hFontTitle);

        RECT textRc = rc;
        if (isPressed) OffsetRect(&textRc, 0, 1);
        DrawTextW(hdc, text.c_str(), -1, &textRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
}


#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <string>

namespace Theme {
    // ─── Playful Neo-Brutalist Color Palette ───
    const COLORREF CLR_CANVAS       = RGB(236, 238, 248);     // Soft periwinkle canvas (#ECEEF8)
    const COLORREF CLR_WHITE        = RGB(255, 255, 255);     // Card white
    const COLORREF CLR_DARK         = RGB(24, 24, 36);        // Deep charcoal borders & text (#181824)
    const COLORREF CLR_YELLOW       = RGB(254, 210, 50);      // Sunny yellow (#FED232)
    const COLORREF CLR_YELLOW_HOVER = RGB(255, 224, 88);      // Light yellow hover
    const COLORREF CLR_MINT         = RGB(111, 227, 180);     // Mint green (#6FE3B4)
    const COLORREF CLR_MINT_HOVER   = RGB(135, 238, 196);     // Light mint hover
    const COLORREF CLR_PURPLE       = RGB(107, 92, 231);      // Action purple (#6B5CE7)
    const COLORREF CLR_PURPLE_HOVER = RGB(90, 75, 215);       // Deep action purple
    const COLORREF CLR_MASCOT_PURPLE= RGB(124, 110, 230);     // Mascot purple (#7C6EE6)
    const COLORREF CLR_MASCOT_GREEN = RGB(92, 225, 166);      // Mascot success green (#5CE1A6)
    const COLORREF CLR_WARNING_BG   = RGB(255, 245, 192);     // Warning card yellow (#FFF5C0)
    const COLORREF CLR_TEXT_SUBTITLE= RGB(85, 88, 112);       // Subtitle slate (#555870)
    const COLORREF CLR_TEXT_MUTED   = RGB(142, 146, 168);     // Muted caption (#8E92A8)
    const COLORREF CLR_CORAL        = RGB(255, 184, 184);     // Avatar coral (#FFB8B8)
    const COLORREF CLR_HOVER_TINT   = RGB(245, 246, 253);     // Subtle white card hover

    class ThemeManager {
    public:
        static void Init();
        static void Shutdown();

        // GDI+ Drawing Helpers
        static void DrawNeoPill(
            Gdiplus::Graphics& g,
            const Gdiplus::RectF& rc,
            Gdiplus::Color fill,
            Gdiplus::Color border = Gdiplus::Color(255, 24, 24, 36),
            float borderWidth = 2.0f
        );

        static void DrawNeoButton(
            Gdiplus::Graphics& g,
            const Gdiplus::RectF& rc,
            const wchar_t* text,
            Gdiplus::Color fill,
            Gdiplus::Color textCol = Gdiplus::Color(255, 24, 24, 36),
            bool isHovered = false,
            bool isPressed = false,
            Gdiplus::Font* pFont = nullptr,
            float shadowOffset = 2.5f
        );

        static void DrawNeoCard(
            Gdiplus::Graphics& g,
            const Gdiplus::RectF& rc,
            Gdiplus::Color fill,
            Gdiplus::Color border = Gdiplus::Color(255, 24, 24, 36),
            float radius = 18.0f,
            float borderWidth = 2.0f,
            float shadowOffset = 0.0f
        );

        static void DrawDashedCard(
            Gdiplus::Graphics& g,
            const Gdiplus::RectF& rc,
            Gdiplus::Color fill,
            Gdiplus::Color border = Gdiplus::Color(255, 24, 24, 36),
            float radius = 18.0f,
            float borderWidth = 2.0f
        );

        static void DrawMascot(
            Gdiplus::Graphics& g,
            float x,
            float y,
            float size,
            bool isGreen = false,
            int animTick = 0
        );

        static void DrawTargetBullseye(
            Gdiplus::Graphics& g,
            float cx,
            float cy,
            float radius
        );

        static void DrawUsbIcon(
            Gdiplus::Graphics& g,
            float x,
            float y,
            float size
        );

        static void DrawWarningTriangle(
            Gdiplus::Graphics& g,
            float cx,
            float cy,
            float size
        );

        static void DrawDevAvatar(
            Gdiplus::Graphics& g,
            float x,
            float y,
            float size
        );

        static void DrawCheckmark(
            Gdiplus::Graphics& g,
            float x,
            float y,
            float size,
            Gdiplus::Color col,
            float strokeWidth = 2.5f
        );

        // GDI+ Fonts
        static Gdiplus::Font* fontTitle;
        static Gdiplus::Font* fontLarge;
        static Gdiplus::Font* fontHeading;
        static Gdiplus::Font* fontSubtitle;
        static Gdiplus::Font* fontBodyBold;
        static Gdiplus::Font* fontBody;
        static Gdiplus::Font* fontSmallBold;
        static Gdiplus::Font* fontSmall;
        static Gdiplus::Font* fontMono;

        // GDI Brushes & Fonts for Windows standard controls
        static HBRUSH hbrCanvas;
        static HBRUSH hbrWhite;
        static HBRUSH hbrDark;
        static HBRUSH hbrYellow;
        static HBRUSH hbrMint;
        static HBRUSH hbrWarning;

        static HFONT hFontRegular;
        static HFONT hFontBold;
        static HFONT hFontTitle;
        static HFONT hFontMono;

    private:
        static ULONG_PTR s_gdiplusToken;
    };
}

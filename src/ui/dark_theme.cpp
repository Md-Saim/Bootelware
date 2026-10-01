#include "dark_theme.h"
#include <dwmapi.h>
#include <math.h>
#include "../resources/resource.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

namespace Theme {
    ULONG_PTR ThemeManager::s_gdiplusToken = 0;
    static PrivateFontCollection s_fontCollection;
    static HANDLE s_hFontMem = NULL;
    static FontFamily* s_pFredokaFamily = nullptr;

    Font* ThemeManager::fontTitle     = nullptr;
    Font* ThemeManager::fontLarge     = nullptr;
    Font* ThemeManager::fontHeading   = nullptr;
    Font* ThemeManager::fontSubtitle  = nullptr;
    Font* ThemeManager::fontBodyBold  = nullptr;
    Font* ThemeManager::fontBody      = nullptr;
    Font* ThemeManager::fontSmallBold = nullptr;
    Font* ThemeManager::fontSmall     = nullptr;
    Font* ThemeManager::fontMono      = nullptr;

    HBRUSH ThemeManager::hbrCanvas    = NULL;
    HBRUSH ThemeManager::hbrWhite     = NULL;
    HBRUSH ThemeManager::hbrDark      = NULL;
    HBRUSH ThemeManager::hbrYellow    = NULL;
    HBRUSH ThemeManager::hbrMint      = NULL;
    HBRUSH ThemeManager::hbrWarning   = NULL;

    HFONT ThemeManager::hFontRegular  = NULL;
    HFONT ThemeManager::hFontBold     = NULL;
    HFONT ThemeManager::hFontTitle    = NULL;
    HFONT ThemeManager::hFontMono     = NULL;

    void ThemeManager::Init() {
        GdiplusStartupInput gdiplusStartupInput;
        GdiplusStartup(&s_gdiplusToken, &gdiplusStartupInput, NULL);

        // 1. Try loading Fredoka from embedded resource IDR_FONT_FREDOKA
        HRSRC hRes = FindResourceW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDR_FONT_FREDOKA), MAKEINTRESOURCEW(10));
        if (hRes) {
            HGLOBAL hMem = LoadResource(GetModuleHandleW(NULL), hRes);
            void* pData = LockResource(hMem);
            DWORD len = SizeofResource(GetModuleHandleW(NULL), hRes);
            if (pData && len > 0) {
                DWORD numFonts = 0;
                s_hFontMem = AddFontMemResourceEx(pData, len, NULL, &numFonts);
                s_fontCollection.AddMemoryFont(pData, len);
            }
        }

        // 2. Also try loading from disk if not yet loaded
        if (s_fontCollection.GetFamilyCount() == 0) {
            if (GetFileAttributesW(L"assets\\fonts\\Fredoka.ttf") != INVALID_FILE_ATTRIBUTES) {
                AddFontResourceExW(L"assets\\fonts\\Fredoka.ttf", FR_PRIVATE, 0);
                s_fontCollection.AddFontFile(L"assets\\fonts\\Fredoka.ttf");
            } else if (GetFileAttributesW(L"resources\\Fredoka.ttf") != INVALID_FILE_ATTRIBUTES) {
                AddFontResourceExW(L"resources\\Fredoka.ttf", FR_PRIVATE, 0);
                s_fontCollection.AddFontFile(L"resources\\Fredoka.ttf");
            }
        }

        // 3. Resolve FontFamily (Fredoka or system fallback)
        const FontFamily* pFam = nullptr;
        if (s_fontCollection.GetFamilyCount() > 0) {
            int count = s_fontCollection.GetFamilyCount();
            FontFamily* families = new FontFamily[count];
            int found = 0;
            s_fontCollection.GetFamilies(count, families, &found);
            if (found > 0) {
                s_pFredokaFamily = families[0].Clone();
                pFam = s_pFredokaFamily;
            }
            delete[] families;
        }

        if (!pFam || pFam->GetLastStatus() != Ok) {
            static FontFamily tryFredoka(L"Fredoka");
            if (tryFredoka.GetLastStatus() == Ok) {
                pFam = &tryFredoka;
            } else {
                static FontFamily fallbackFont(L"Segoe UI");
                pFam = (fallbackFont.GetLastStatus() == Ok) ? &fallbackFont : FontFamily::GenericSansSerif();
            }
        }

        const FontFamily* pMono = FontFamily::GenericMonospace();

        // Fredoka Font Hierarchy (Weights 400 to 700)
        fontTitle     = new Font(pFam, 22.0f, FontStyleBold, UnitPixel);
        fontLarge     = new Font(pFam, 16.5f, FontStyleBold, UnitPixel);
        fontHeading   = new Font(pFam, 14.5f, FontStyleBold, UnitPixel);
        fontSubtitle  = new Font(pFam, 12.5f, FontStyleRegular, UnitPixel);
        fontBodyBold  = new Font(pFam, 12.0f, FontStyleBold, UnitPixel);
        fontBody      = new Font(pFam, 11.5f, FontStyleRegular, UnitPixel);
        fontSmallBold = new Font(pFam, 10.0f, FontStyleBold, UnitPixel);
        fontSmall     = new Font(pFam, 9.5f,  FontStyleRegular, UnitPixel);
        fontMono      = new Font(pMono, 10.0f, FontStyleRegular, UnitPixel);

        // GDI objects
        hbrCanvas  = CreateSolidBrush(CLR_CANVAS);
        hbrWhite   = CreateSolidBrush(CLR_WHITE);
        hbrDark    = CreateSolidBrush(CLR_DARK);
        hbrYellow  = CreateSolidBrush(CLR_YELLOW);
        hbrMint    = CreateSolidBrush(CLR_MINT);
        hbrWarning = CreateSolidBrush(CLR_WARNING_BG);

        const wchar_t* primaryFont = L"Fredoka";
        hFontRegular = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, primaryFont);
        hFontBold    = CreateFontW(-14, 0, 0, 0, FW_BOLD,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, primaryFont);
        hFontTitle   = CreateFontW(-24, 0, 0, 0, FW_BOLD,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, primaryFont);
        hFontMono    = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    }

    void ThemeManager::Shutdown() {
        delete fontTitle;
        delete fontLarge;
        delete fontHeading;
        delete fontSubtitle;
        delete fontBodyBold;
        delete fontBody;
        delete fontSmallBold;
        delete fontSmall;
        delete fontMono;

        if (s_pFredokaFamily) {
            delete s_pFredokaFamily;
            s_pFredokaFamily = nullptr;
        }

        if (hbrCanvas)  DeleteObject(hbrCanvas);
        if (hbrWhite)   DeleteObject(hbrWhite);
        if (hbrDark)    DeleteObject(hbrDark);
        if (hbrYellow)  DeleteObject(hbrYellow);
        if (hbrMint)    DeleteObject(hbrMint);
        if (hbrWarning) DeleteObject(hbrWarning);

        if (hFontRegular) DeleteObject(hFontRegular);
        if (hFontBold)    DeleteObject(hFontBold);
        if (hFontTitle)   DeleteObject(hFontTitle);
        if (hFontMono)    DeleteObject(hFontMono);

        if (s_hFontMem) {
            RemoveFontMemResourceEx(s_hFontMem);
            s_hFontMem = NULL;
        }

        if (s_gdiplusToken) {
            GdiplusShutdown(s_gdiplusToken);
            s_gdiplusToken = 0;
        }
    }

    void ThemeManager::DrawNeoPill(
        Graphics& g,
        const RectF& rc,
        Color fill,
        Color border,
        float borderWidth
    ) {
        float r = rc.Height / 2.0f;
        if (r > rc.Width / 2.0f) r = rc.Width / 2.0f;
        float d = r * 2.0f;

        GraphicsPath path;
        path.AddArc(rc.X, rc.Y, d, d, 90.0f, 180.0f);
        path.AddArc(rc.X + rc.Width - d, rc.Y, d, d, 270.0f, 180.0f);
        path.CloseFigure();

        SolidBrush brush(fill);
        g.FillPath(&brush, &path);

        if (borderWidth > 0.0f) {
            Pen pen(border, borderWidth);
            g.DrawPath(&pen, &path);
        }
    }

    void ThemeManager::DrawNeoButton(
        Graphics& g,
        const RectF& rc,
        const wchar_t* text,
        Color fill,
        Color textCol,
        bool isHovered,
        bool isPressed,
        Font* pFont,
        float shadowOffset
    ) {
        if (!pFont) pFont = fontBodyBold;
        Color darkLine(255, 24, 24, 36);

        float off = shadowOffset;
        if (isPressed) {
            off = 0.5f;
        } else if (isHovered) {
            off = shadowOffset + 0.8f;
        }

        // Draw shadow pill underneath if offset > 0
        if (off > 0.6f) {
            RectF shadowRc(rc.X, rc.Y + off, rc.Width, rc.Height);
            DrawNeoPill(g, shadowRc, darkLine, darkLine, 1.0f);
        }

        // Button face position (shifts down on press)
        float shiftY = isPressed ? off : 0.0f;
        RectF btnRc(rc.X, rc.Y + shiftY, rc.Width, rc.Height);

        DrawNeoPill(g, btnRc, fill, darkLine, 2.0f);

        StringFormat sf;
        sf.SetAlignment(StringAlignmentCenter);
        sf.SetLineAlignment(StringAlignmentCenter);
        sf.SetFormatFlags(StringFormatFlagsNoWrap);
        SolidBrush tBrush(textCol);
        g.DrawString(text, -1, pFont, btnRc, &sf, &tBrush);
    }

    void ThemeManager::DrawNeoCard(
        Graphics& g,
        const RectF& rc,
        Color fill,
        Color border,
        float radius,
        float borderWidth,
        float shadowOffset
    ) {
        if (shadowOffset > 0.5f) {
            RectF shadowRc(rc.X, rc.Y + shadowOffset, rc.Width, rc.Height);
            DrawNeoCard(g, shadowRc, border, border, radius, 1.0f, 0.0f);
        }

        float d = radius * 2.0f;
        if (d > rc.Width) d = rc.Width;
        if (d > rc.Height) d = rc.Height;

        GraphicsPath path;
        path.AddArc(rc.X, rc.Y, d, d, 180.0f, 90.0f);
        path.AddArc(rc.X + rc.Width - d, rc.Y, d, d, 270.0f, 90.0f);
        path.AddArc(rc.X + rc.Width - d, rc.Y + rc.Height - d, d, d, 0.0f, 90.0f);
        path.AddArc(rc.X, rc.Y + rc.Height - d, d, d, 90.0f, 90.0f);
        path.CloseFigure();

        SolidBrush brush(fill);
        g.FillPath(&brush, &path);

        if (borderWidth > 0.0f) {
            Pen pen(border, borderWidth);
            g.DrawPath(&pen, &path);
        }
    }

    void ThemeManager::DrawDashedCard(
        Graphics& g,
        const RectF& rc,
        Color fill,
        Color border,
        float radius,
        float borderWidth
    ) {
        float d = radius * 2.0f;
        if (d > rc.Width) d = rc.Width;
        if (d > rc.Height) d = rc.Height;

        GraphicsPath path;
        path.AddArc(rc.X, rc.Y, d, d, 180.0f, 90.0f);
        path.AddArc(rc.X + rc.Width - d, rc.Y, d, d, 270.0f, 90.0f);
        path.AddArc(rc.X + rc.Width - d, rc.Y + rc.Height - d, d, d, 0.0f, 90.0f);
        path.AddArc(rc.X, rc.Y + rc.Height - d, d, d, 90.0f, 90.0f);
        path.CloseFigure();

        SolidBrush brush(fill);
        g.FillPath(&brush, &path);

        if (borderWidth > 0.0f) {
            Pen pen(border, borderWidth);
            pen.SetDashStyle(DashStyleDash);
            REAL dashPattern[2] = { 4.0f, 3.0f };
            pen.SetDashPattern(dashPattern, 2);
            g.DrawPath(&pen, &path);
        }
    }

    void ThemeManager::DrawMascot(
        Graphics& g,
        float x,
        float y,
        float size,
        bool isGreen,
        int animTick
    ) {
        float scale = size / 100.0f;
        Color darkLine(255, 24, 24, 36);
        Pen darkPen(darkLine, 2.0f * scale);
        SolidBrush darkBrush(darkLine);

        // Animation dynamics
        float bobY = 0.0f;
        float capTilt = 0.0f;
        bool isBlinking = false;
        if (animTick > 0) {
            if (isGreen) {
                // Celebration bounce
                bobY = -fabsf(sinf(animTick * 0.12f)) * (5.5f * scale);
            } else {
                // Gentle floating hover
                bobY = sinf(animTick * 0.09f) * (3.5f * scale);
            }
            capTilt = sinf(animTick * 0.12f) * (1.8f * scale);

            // Natural eye blinking cycle
            int blinkCycle = animTick % 110;
            if (blinkCycle >= 92 && blinkCycle <= 100) {
                isBlinking = true;
            }
        }
        y += bobY;

        // Cap (Yellow USB plug on top)
        float capW = 34.0f * scale;
        float capH = 17.0f * scale;
        float capX = x + (size - capW) / 2.0f + capTilt;
        float capY = y + 8.0f * scale;
        float capR = 5.0f * scale;

        GraphicsPath capPath;
        capPath.AddArc(capX, capY, capR * 2, capR * 2, 180, 90);
        capPath.AddArc(capX + capW - capR * 2, capY, capR * 2, capR * 2, 270, 90);
        capPath.AddLine(capX + capW, capY + capH, capX, capY + capH);
        capPath.CloseFigure();

        SolidBrush yellowBrush(Color(255, 254, 210, 50));
        g.FillPath(&yellowBrush, &capPath);
        g.DrawPath(&darkPen, &capPath);

        // Cap inner notches
        float notchW = 4.5f * scale;
        float notchH = 5.5f * scale;
        g.FillRectangle(&darkBrush, RectF(capX + 6.0f * scale, capY + 4.5f * scale, notchW, notchH));
        g.FillRectangle(&darkBrush, RectF(capX + capW - 6.0f * scale - notchW, capY + 4.5f * scale, notchW, notchH));

        // Head (Purple or Mint Green)
        float headW = 76.0f * scale;
        float headH = 68.0f * scale;
        float headX = x + (size - headW) / 2.0f;
        float headY = y + 21.0f * scale;
        float headR = 18.0f * scale;

        GraphicsPath headPath;
        headPath.AddArc(headX, headY, headR * 2, headR * 2, 180, 90);
        headPath.AddArc(headX + headW - headR * 2, headY, headR * 2, headR * 2, 270, 90);
        headPath.AddArc(headX + headW - headR * 2, headY + headH - headR * 2, headR * 2, headR * 2, 0, 90);
        headPath.AddArc(headX, headY + headH - headR * 2, headR * 2, headR * 2, 90, 90);
        headPath.CloseFigure();

        Color headColor = isGreen ? Color(255, 92, 225, 166) : Color(255, 124, 110, 230);
        SolidBrush headBrush(headColor);
        g.FillPath(&headBrush, &headPath);
        g.DrawPath(&darkPen, &headPath);

        // Cheeks blush
        SolidBrush blushBrush(Color(65, 255, 100, 150));
        g.FillEllipse(&blushBrush, RectF(headX + 5.0f * scale, headY + headH * 0.58f, 10.0f * scale, 6.0f * scale));
        g.FillEllipse(&blushBrush, RectF(headX + headW - 15.0f * scale, headY + headH * 0.58f, 10.0f * scale, 6.0f * scale));

        // Eyes
        float eyeY = headY + 30.0f * scale;
        float eyeLX = headX + 22.0f * scale;
        float eyeRX = headX + headW - 22.0f * scale;
        float eyeR = 5.0f * scale;

        if (isGreen || isBlinking) {
            // Happy curved smiling eyes ^ ^
            Pen eyeArcPen(darkLine, 2.4f * scale);
            eyeArcPen.SetStartCap(LineCapRound);
            eyeArcPen.SetEndCap(LineCapRound);
            g.DrawArc(&eyeArcPen, RectF(eyeLX - eyeR, eyeY - eyeR * 0.6f, eyeR * 2, eyeR * 1.6f), 200, 140);
            g.DrawArc(&eyeArcPen, RectF(eyeRX - eyeR, eyeY - eyeR * 0.6f, eyeR * 2, eyeR * 1.6f), 200, 140);
        } else {
            // Cute round dark eyes with specular shine
            g.FillEllipse(&darkBrush, RectF(eyeLX - eyeR, eyeY - eyeR, eyeR * 2, eyeR * 2));
            g.FillEllipse(&darkBrush, RectF(eyeRX - eyeR, eyeY - eyeR, eyeR * 2, eyeR * 2));

            SolidBrush whiteBrush(Color(255, 255, 255, 255));
            float shineR = 1.6f * scale;
            g.FillEllipse(&whiteBrush, RectF(eyeLX - shineR + 1.2f * scale, eyeY - shineR - 1.2f * scale, shineR * 2, shineR * 2));
            g.FillEllipse(&whiteBrush, RectF(eyeRX - shineR + 1.2f * scale, eyeY - shineR - 1.2f * scale, shineR * 2, shineR * 2));
        }

        // Smile
        Pen smilePen(darkLine, 2.2f * scale);
        smilePen.SetStartCap(LineCapRound);
        smilePen.SetEndCap(LineCapRound);
        float smileW = 16.0f * scale;
        float smileH = 13.0f * scale;
        g.DrawArc(&smilePen, RectF(headX + (headW - smileW) / 2.0f, eyeY + 4.0f * scale, smileW, smileH), 20, 140);
    }

    void ThemeManager::DrawTargetBullseye(
        Graphics& g,
        float cx,
        float cy,
        float radius
    ) {
        Color darkLine(255, 24, 24, 36);
        Pen darkPen(darkLine, 2.2f);
        SolidBrush darkBrush(darkLine);

        // Outer circle
        g.DrawEllipse(&darkPen, RectF(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f));

        // Yellow middle disc
        float midR = radius * 0.65f;
        SolidBrush yellowBrush(Color(255, 254, 210, 50));
        g.FillEllipse(&yellowBrush, RectF(cx - midR, cy - midR, midR * 2.0f, midR * 2.0f));
        g.DrawEllipse(&darkPen, RectF(cx - midR, cy - midR, midR * 2.0f, midR * 2.0f));

        // Center dot
        float dotR = radius * 0.22f;
        g.FillEllipse(&darkBrush, RectF(cx - dotR, cy - dotR, dotR * 2.0f, dotR * 2.0f));
    }

    void ThemeManager::DrawUsbIcon(
        Graphics& g,
        float x,
        float y,
        float size
    ) {
        float scale = size / 32.0f;
        Color darkLine(255, 24, 24, 36);
        Pen darkPen(darkLine, 2.0f * scale);
        SolidBrush whiteBrush(Color(255, 255, 255, 255));
        SolidBrush darkBrush(darkLine);

        // Body of USB stick
        float bodyW = 16.0f * scale;
        float bodyH = 22.0f * scale;
        float bodyX = x + (size - bodyW) / 2.0f;
        float bodyY = y + 8.0f * scale;

        GraphicsPath bodyPath;
        float r = 3.0f * scale;
        bodyPath.AddArc(bodyX, bodyY, r*2, r*2, 180, 90);
        bodyPath.AddArc(bodyX + bodyW - r*2, bodyY, r*2, r*2, 270, 90);
        bodyPath.AddArc(bodyX + bodyW - r*2, bodyY + bodyH - r*2, r*2, r*2, 0, 90);
        bodyPath.AddArc(bodyX, bodyY + bodyH - r*2, r*2, r*2, 90, 90);
        bodyPath.CloseFigure();
        g.FillPath(&whiteBrush, &bodyPath);
        g.DrawPath(&darkPen, &bodyPath);

        // Connector on top
        float connW = 10.0f * scale;
        float connH = 7.0f * scale;
        float connX = x + (size - connW) / 2.0f;
        float connY = y + 2.0f * scale;
        g.DrawRectangle(&darkPen, RectF(connX, connY, connW, connH));

        // Connector pins
        g.FillRectangle(&darkBrush, RectF(connX + 2.0f * scale, connY + 2.0f * scale, 2.0f * scale, 3.0f * scale));
        g.FillRectangle(&darkBrush, RectF(connX + connW - 4.0f * scale, connY + 2.0f * scale, 2.0f * scale, 3.0f * scale));

        // Body center notch
        g.DrawLine(&darkPen, PointF(bodyX + 4.0f * scale, bodyY + 8.0f * scale), PointF(bodyX + bodyW - 4.0f * scale, bodyY + 8.0f * scale));
    }

    void ThemeManager::DrawWarningTriangle(
        Graphics& g,
        float cx,
        float cy,
        float size
    ) {
        Color darkLine(255, 24, 24, 36);
        Pen darkPen(darkLine, 2.0f);
        SolidBrush darkBrush(darkLine);

        float h = size * 0.866f;
        PointF pts[3] = {
            PointF(cx, cy - h / 2.0f),
            PointF(cx - size / 2.0f, cy + h / 2.0f),
            PointF(cx + size / 2.0f, cy + h / 2.0f)
        };

        g.DrawPolygon(&darkPen, pts, 3);

        // Exclamation mark
        Pen exPen(darkLine, 2.0f);
        g.DrawLine(&exPen, PointF(cx, cy - h / 5.0f), PointF(cx, cy + h / 6.0f));
        g.FillEllipse(&darkBrush, RectF(cx - 1.2f, cy + h / 3.0f - 1.2f, 2.4f, 2.4f));
    }

    void ThemeManager::DrawDevAvatar(
        Graphics& g,
        float x,
        float y,
        float size
    ) {
        Color darkLine(255, 24, 24, 36);
        Pen darkPen(darkLine, 2.0f);
        SolidBrush coralBrush(Color(255, 255, 184, 184));
        SolidBrush darkBrush(darkLine);

        // Circular background
        g.FillEllipse(&coralBrush, RectF(x, y, size, size));
        g.DrawEllipse(&darkPen, RectF(x, y, size, size));

        // Face & hair sketch
        float scale = size / 64.0f;
        SolidBrush skinBrush(Color(255, 255, 224, 189));
        g.FillEllipse(&skinBrush, RectF(x + 19.0f * scale, y + 21.0f * scale, 26.0f * scale, 28.0f * scale));
        g.DrawEllipse(&darkPen, RectF(x + 19.0f * scale, y + 21.0f * scale, 26.0f * scale, 28.0f * scale));

        // Hair arc
        Pen hairPen(darkLine, 4.0f * scale);
        g.DrawArc(&hairPen, RectF(x + 17.0f * scale, y + 13.0f * scale, 30.0f * scale, 20.0f * scale), 160, 220);

        // Eyes
        g.FillEllipse(&darkBrush, RectF(x + 24.0f * scale, y + 30.0f * scale, 3.0f * scale, 3.0f * scale));
        g.FillEllipse(&darkBrush, RectF(x + 37.0f * scale, y + 30.0f * scale, 3.0f * scale, 3.0f * scale));

        // Smile
        Pen smilePen(darkLine, 1.8f * scale);
        g.DrawArc(&smilePen, RectF(x + 27.0f * scale, y + 36.0f * scale, 10.0f * scale, 6.0f * scale), 20, 140);
    }

    void ThemeManager::DrawCheckmark(
        Graphics& g,
        float x,
        float y,
        float size,
        Color col,
        float strokeWidth
    ) {
        Pen pen(col, strokeWidth);
        pen.SetStartCap(LineCapRound);
        pen.SetEndCap(LineCapRound);

        PointF p1(x, y + size * 0.5f);
        PointF p2(x + size * 0.38f, y + size * 0.88f);
        PointF p3(x + size, y + size * 0.12f);

        g.DrawLine(&pen, p1, p2);
        g.DrawLine(&pen, p2, p3);
    }
}

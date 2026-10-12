// media-player / blur-probe.cpp
// Finds the recipe for a frosted panel that looks like the taskbar's own pills,
// and answers the corner question.
//
// The taskbar's pills (theme FrostyGlass) are
//   <WindhawkBlur BlurAmount="20" TintColor="SystemChromeDarkColor" TintOpacity="0.15"/>
// that is: a real blur of what is behind the taskbar, plus a 15 % tint. The
// widget has to end up looking like that, and the first attempt (accent acrylic
// plus the same tint painted again) came out about twice as dark.
//
// The probe puts four wide colour bands behind the rows (red, green, blue,
// black) and photographs the screen. The rows paint NOTHING themselves (their
// layered bitmap is fully transparent), so whatever a row shows is exactly what
// its glass recipe produces - and it can be compared with the raw bands in
// row 1. A recipe that matches the taskbar should keep the band colours close to
// the reference and only slightly darkened by the 15 % tint.
//
// Rows (label: state / flags / tint / corners):
//   1 reference: no glass at all, the raw bands
//   2 acrylic, modern recipe, theme tint       (0x26 1F1F1F)
//   3 acrylic, modern recipe, white tint       (0x33 FFFFFF)
//   4 acrylic, old recipe (flags 0), theme tint
//   5 host backdrop (state 5), theme tint
//   6 blur behind (state 3), no tint
//   7 acrylic + window region 10        - corner test
//   8 acrylic + DWM rounding            - corner test
//   9 acrylic on a separate non-layered backdrop window + region 10
//
// Build with build-blur-probe.ps1, run with two output PNG paths.

#include <windows.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <stdio.h>

namespace {

constexpr int kPanelWidth = 466;
constexpr int kPanelHeight = 42;
constexpr int kRowGap = 10;
constexpr int kOriginX = 200;
constexpr int kOriginY = 150;
constexpr int kMargin = 24;

// FrostyGlass: CornerRadius 10.
constexpr float kRadius = 10.0f;

constexpr DWORD kDwmwaUseImmersiveDarkMode = 20;
constexpr DWORD kDwmwaWindowCornerPreference = 33;
constexpr int kCornerRound = 2;  // DWMWCP_ROUND (8 px at 96 DPI)

// SetWindowCompositionAttribute is undocumented but stable since Windows 10
// 1803; this is what the taskbar-blur tools and dynamic-island-for-windows use.
struct AccentPolicy {
    DWORD accentState;
    DWORD accentFlags;
    DWORD gradientColor;  // ABGR
    DWORD animationId;
};

struct WindowCompositionAttributeData {
    DWORD attribute;
    void* data;
    SIZE_T size;
};

constexpr DWORD kWcaAccentPolicy = 19;
constexpr DWORD kAccentDisabled = 0;
constexpr DWORD kAccentEnableTransparentGradient = 2;
constexpr DWORD kAccentEnableBlurBehind = 3;
constexpr DWORD kAccentEnableAcrylicBlurBehind = 4;
constexpr DWORD kAccentEnableHostBackdrop = 5;

using SetWindowCompositionAttributeFn = BOOL(WINAPI*)(HWND,
                                                      WindowCompositionAttributeData*);

SetWindowCompositionAttributeFn SetWindowCompositionAttributeProc() {
    static SetWindowCompositionAttributeFn proc = []() {
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        if (!user32) {
            return (SetWindowCompositionAttributeFn) nullptr;
        }
        return (SetWindowCompositionAttributeFn)GetProcAddress(
            user32, "SetWindowCompositionAttribute");
    }();
    return proc;
}

bool ApplyAccent(HWND hwnd, const wchar_t* what, DWORD state, DWORD flags,
                 DWORD gradientColor) {
    SetWindowCompositionAttributeFn proc = SetWindowCompositionAttributeProc();
    if (!proc) {
        wprintf(L"   %s: SetWindowCompositionAttribute is not exported\n", what);
        return false;
    }
    AccentPolicy policy{};
    policy.accentState = state;
    policy.accentFlags = flags;
    policy.gradientColor = gradientColor;
    WindowCompositionAttributeData data{};
    data.attribute = kWcaAccentPolicy;
    data.data = &policy;
    data.size = sizeof(policy);
    BOOL ok = proc(hwnd, &data);
    wprintf(L"   %s: state=%lu flags=%lu tint=%08lX -> %s\n", what, state, flags,
            gradientColor, ok ? L"ok" : L"FAILED");
    return ok != FALSE;
}

// ---------------------------------------------------------------------------
// Windows
// ---------------------------------------------------------------------------

constexpr PCWSTR kBackdropClass = L"BlurProbeBackdrop";
constexpr PCWSTR kPanelClass = L"BlurProbePanel";
constexpr PCWSTR kPatternClass = L"BlurProbePattern";

LRESULT CALLBACK PassThroughProc(HWND hwnd, UINT message, WPARAM wParam,
                                 LPARAM lParam) {
    switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_NCHITTEST:
            return HTTRANSPARENT;
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_SETCURSOR:
            return TRUE;
        default:
            return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

void RegisterProbeClasses() {
    const PCWSTR names[3] = {kBackdropClass, kPanelClass, kPatternClass};
    for (PCWSTR name : names) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = PassThroughProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = name;
        // No background brush anywhere: nothing may paint over the glass.
        RegisterClassExW(&wc);
    }
}

int CaptureX() { return kOriginX - kMargin; }
int CaptureY() { return kOriginY - kMargin; }
int CaptureWidth() { return kPanelWidth + 2 * kMargin; }

// ---------------------------------------------------------------------------
// The rows
// ---------------------------------------------------------------------------

struct RowSpec {
    const wchar_t* label;
    DWORD state;        // 0 = no glass at all
    DWORD flags;
    DWORD tint;         // ABGR
    bool region;        // SetWindowRgn to the theme radius
    bool dwmRounding;   // DWMWCP_ROUND
    bool separateWindow;// glass on a non-layered window instead of the panel
};

RowSpec g_rows[] = {
    {L"1 reference: no glass, raw bands", 0, 0, 0, false, false, false},
    {L"2 acrylic modern, theme tint", kAccentEnableAcrylicBlurBehind, 2, 0x261F1F1F,
     false, false, false},
    {L"3 acrylic modern, white tint", kAccentEnableAcrylicBlurBehind, 2, 0x33FFFFFF,
     false, false, false},
    {L"4 acrylic old recipe, theme tint", kAccentEnableAcrylicBlurBehind, 0,
     0x261F1F1F, false, false, false},
    {L"5 host backdrop, theme tint", kAccentEnableHostBackdrop, 2, 0x261F1F1F, false,
     false, false},
    {L"6 blur behind, no tint", kAccentEnableBlurBehind, 0, 0x00000000, false, false,
     false},
    {L"7 acrylic + window region 10", kAccentEnableAcrylicBlurBehind, 2, 0x261F1F1F,
     true, false, false},
    {L"8 acrylic + DWM rounding", kAccentEnableAcrylicBlurBehind, 2, 0x261F1F1F, false,
     true, false},
    {L"9 acrylic on a separate backdrop + region", kAccentEnableAcrylicBlurBehind, 2,
     0x261F1F1F, true, false, true},
};
constexpr int kRows = (int)(sizeof(g_rows) / sizeof(g_rows[0]));

int CaptureHeight() {
    return kRows * kPanelHeight + (kRows - 1) * kRowGap + 2 * kMargin;
}

// The bottom layer: wide, saturated colour bands. Glass keeps the colour of each
// band and softens the seams; a darker recipe shows as darker band colours.
void PaintPattern(HWND hwnd) {
    RECT client{};
    GetClientRect(hwnd, &client);
    int width = client.right;
    int height = client.bottom;

    HDC windowDc = GetDC(hwnd);
    HDC memDc = CreateCompatibleDC(windowDc);
    HBITMAP dib = CreateCompatibleBitmap(windowDc, width, height);
    HBITMAP old = (HBITMAP)SelectObject(memDc, dib);
    {
        Gdiplus::Graphics g(memDc);
        Gdiplus::SolidBrush white(Gdiplus::Color(255, 255, 255, 255));
        g.FillRectangle(&white, 0, 0, width, height);

        struct Band {
            int from;
            int to;
            Gdiplus::Color colour;
        };
        Band bands[4] = {
            {0, width / 4, Gdiplus::Color(255, 220, 20, 20)},
            {width / 4, width / 2, Gdiplus::Color(255, 20, 190, 40)},
            {width / 2, 3 * width / 4, Gdiplus::Color(255, 20, 60, 230)},
            {3 * width / 4, width, Gdiplus::Color(255, 0, 0, 0)},
        };
        for (const Band& band : bands) {
            Gdiplus::SolidBrush brush(band.colour);
            g.FillRectangle(&brush, band.from, 0, band.to - band.from, height);
        }
    }
    BitBlt(windowDc, 0, 0, width, height, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, old);
    DeleteObject(dib);
    DeleteDC(memDc);
    ReleaseDC(hwnd, windowDc);
}

HWND CreateWindowOfClass(PCWSTR cls, DWORD exStyle, int x, int y) {
    HWND hwnd = CreateWindowExW(exStyle, cls, L"", WS_POPUP, x, y, kPanelWidth,
                                kPanelHeight, nullptr, nullptr,
                                GetModuleHandleW(nullptr), nullptr);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    SetWindowPos(hwnd, HWND_TOPMOST, x, y, kPanelWidth, kPanelHeight,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    return hwnd;
}

HWND CreateBackdrop(int x, int y) {
    return CreateWindowOfClass(kBackdropClass,
                               WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
                                   WS_EX_TRANSPARENT,
                               x, y);
}

HWND CreatePanel(int x, int y) {
    return CreateWindowOfClass(kPanelClass,
                               WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
                                   WS_EX_LAYERED,
                               x, y);
}

// Pushes a fully transparent bitmap: the row paints nothing of its own, so only
// the glass recipe is visible.
void PaintNothing(HWND hwnd) {
    RECT client{};
    GetClientRect(hwnd, &client);
    int width = client.right;
    int height = client.bottom;

    Gdiplus::Bitmap bitmap(width, height, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(&bitmap);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));
    }

    HDC screenDc = GetDC(nullptr);
    HDC memDc = CreateCompatibleDC(screenDc);
    HBITMAP dib = nullptr;
    if (bitmap.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &dib) == Gdiplus::Ok && dib) {
        HBITMAP old = (HBITMAP)SelectObject(memDc, dib);
        RECT windowRect{};
        GetWindowRect(hwnd, &windowRect);
        POINT dest{windowRect.left, windowRect.top};
        SIZE size{width, height};
        POINT source{0, 0};
        BLENDFUNCTION blend{};
        blend.BlendOp = AC_SRC_OVER;
        blend.SourceConstantAlpha = 255;
        blend.AlphaFormat = AC_SRC_ALPHA;
        UpdateLayeredWindow(hwnd, screenDc, &dest, &size, memDc, &source, 0, &blend,
                            ULW_ALPHA);
        SelectObject(memDc, old);
        DeleteObject(dib);
    }
    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);
}

void ClipRounded(HWND hwnd, int width, int height) {
    int diameter = (int)(kRadius * 2.0f + 0.5f);
    HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, diameter, diameter);
    SetWindowRgn(hwnd, region, TRUE);
}

// ---------------------------------------------------------------------------
// Screen capture
// ---------------------------------------------------------------------------

int GetEncoderClsid(const WCHAR* format, CLSID* clsid) {
    UINT count = 0;
    UINT size = 0;
    if (Gdiplus::GetImageEncodersSize(&count, &size) != Gdiplus::Ok || size == 0) {
        return -1;
    }
    auto* info = (Gdiplus::ImageCodecInfo*)malloc(size);
    if (!info) {
        return -1;
    }
    int found = -1;
    if (Gdiplus::GetImageEncoders(count, size, info) == Gdiplus::Ok) {
        for (UINT i = 0; i < count; ++i) {
            if (wcscmp(info[i].MimeType, format) == 0) {
                *clsid = info[i].Clsid;
                found = (int)i;
                break;
            }
        }
    }
    free(info);
    return found;
}

bool CaptureRect(int x, int y, int width, int height, const wchar_t* path) {
    HDC screenDc = GetDC(nullptr);
    HDC memDc = CreateCompatibleDC(screenDc);
    void* bits = nullptr;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;  // top down
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    HBITMAP dib = CreateDIBSection(memDc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    bool saved = false;
    if (dib && bits) {
        HBITMAP old = (HBITMAP)SelectObject(memDc, dib);
        BitBlt(memDc, 0, 0, width, height, screenDc, x, y, SRCCOPY);
        Gdiplus::Bitmap bitmap(dib, nullptr);
        CLSID png{};
        if (GetEncoderClsid(L"image/png", &png) >= 0) {
            saved = bitmap.Save(path, &png, nullptr) == Gdiplus::Ok;
        }
        SelectObject(memDc, old);
    }
    if (dib) {
        DeleteObject(dib);
    }
    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);
    return saved;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    const wchar_t* early = argc > 1 ? argv[1] : L"blur-probe.png";
    const wchar_t* late = argc > 2 ? argv[2] : L"blur-probe-late.png";

    ULONG_PTR token = 0;
    Gdiplus::GdiplusStartupInput input;
    if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok) {
        wprintf(L"GdiplusStartup failed\n");
        return 1;
    }
    RegisterProbeClasses();

    const int rowPitch = kPanelHeight + kRowGap;
    auto rowY = [rowPitch](int row) { return kOriginY + row * rowPitch; };

    wprintf(L"backdrop pattern window (4 colour bands)\n");
    HWND pattern = CreateWindowOfClass(
        kPatternClass,
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
        CaptureX(), CaptureY());
    SetWindowPos(pattern, HWND_TOPMOST, CaptureX(), CaptureY(), CaptureWidth(),
                 CaptureHeight(), SWP_NOACTIVATE | SWP_SHOWWINDOW);
    PaintPattern(pattern);
    UpdateWindow(pattern);

    for (int row = 0; row < kRows; ++row) {
        const RowSpec& spec = g_rows[row];
        wprintf(L"row %d: %s\n", row + 1, spec.label);

        // The glass lives either on the layered panel itself or on a separate
        // non-layered window underneath it.
        HWND panel = CreatePanel(kOriginX, rowY(row));
        HWND glassHost = panel;
        if (spec.separateWindow) {
            HWND backdrop = CreateBackdrop(kOriginX, rowY(row));
            glassHost = backdrop;
            // Keep the backdrop directly below its panel.
            SetWindowPos(backdrop, panel, kOriginX, rowY(row), kPanelWidth,
                         kPanelHeight, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }

        if (spec.dwmRounding) {
            int corner = kCornerRound;
            HRESULT hr = DwmSetWindowAttribute(glassHost,
                                               kDwmwaWindowCornerPreference, &corner,
                                               sizeof(corner));
            wprintf(L"   DWM rounding -> hr=0x%08lX\n", (unsigned)hr);
            BOOL dark = TRUE;
            DwmSetWindowAttribute(glassHost, kDwmwaUseImmersiveDarkMode, &dark,
                                  sizeof(dark));
        }
        if (spec.region) {
            ClipRounded(glassHost, kPanelWidth, kPanelHeight);
        }
        if (spec.state != 0) {
            ApplyAccent(glassHost, L"glass", spec.state, spec.flags, spec.tint);
        }
        PaintNothing(panel);
    }

    Sleep(1800);
    bool savedEarly = CaptureRect(CaptureX(), CaptureY(), CaptureWidth(),
                                  CaptureHeight(), early);
    wprintf(L"capture: %s\n", savedEarly ? early : L"FAILED");

    Sleep(5000);
    bool savedLate = CaptureRect(CaptureX(), CaptureY(), CaptureWidth(),
                                 CaptureHeight(), late);
    wprintf(L"capture: %s\n", savedLate ? late : L"FAILED");

    Gdiplus::GdiplusShutdown(token);
    return (savedEarly && savedLate) ? 0 : 1;
}

// media-player / widget-diag.cpp
// Reports the widget window's real geometry and takes a picture of it, so the
// drawing can be checked without relying on a screenshot of the whole taskbar.
//
// Read-only: it never moves, shows or hides any window.
//
// Build with Windhawk's clang++ (same flags as run-probe.ps1), run widget-diag.exe <out-dir>.

#include <windows.h>
#include <dwmapi.h>

#include <gdiplus.h>

#include <cstdio>
#include <string>

#pragma comment(lib, "gdiplus.lib")

static const wchar_t* kWidgetClass = L"WindhawkTaskbarMediaWidget";

static const char* Narrow(const wchar_t* wide) {
    static char buffers[8][512];
    static int index = 0;
    index = (index + 1) % 8;
    char* out = buffers[index];
    out[0] = '\0';
    if (wide) {
        WideCharToMultiByte(CP_UTF8, 0, wide, -1, out, 511, nullptr, nullptr);
        out[511] = '\0';
    }
    return out;
}

static HWND found = nullptr;
static HWND taskbar = nullptr;

static BOOL CALLBACK EnumProc(HWND hwnd, LPARAM) {
    wchar_t className[256] = L"";
    GetClassNameW(hwnd, className, ARRAYSIZE(className));

    if (_wcsicmp(className, kWidgetClass) == 0) {
        found = hwnd;
    }
    if (_wcsicmp(className, L"Shell_TrayWnd") == 0) {
        taskbar = hwnd;
    }
    if (taskbar && found) {
        return FALSE;
    }
    return TRUE;
}

static void ReportWindow(const char* label, HWND hwnd) {
    if (!hwnd) {
        printf("%-18s (not found)\n", label);
        return;
    }
    RECT r{};
    GetWindowRect(hwnd, &r);
    RECT c{};
    GetClientRect(hwnd, &c);

    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    HWND parent = GetParent(hwnd);

    int cloaked = -1;
    DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));

    printf("%-18s hwnd=%p\n", label, (void*)hwnd);
    printf("%-18s   screen rect : %ld,%ld - %ld,%ld  (%ld x %ld)\n", "",
           r.left, r.top, r.right, r.bottom, r.right - r.left, r.bottom - r.top);
    printf("%-18s   client size : %ld x %ld\n", "", c.right, c.bottom);
    printf("%-18s   visible=%d  parent=%p  cloaked=%d\n", "",
           (int)IsWindowVisible(hwnd), (void*)parent, cloaked);
    printf("%-18s   style=0x%08lX  exStyle=0x%08lX\n", "", (unsigned long)style,
           (unsigned long)exStyle);
    printf("%-18s   has WS_EX_LAYERED=%d WS_EX_TOPMOST=%d WS_EX_TOOLWINDOW=%d\n",
           "", (exStyle & WS_EX_LAYERED) ? 1 : 0,
           (exStyle & WS_EX_TOPMOST) ? 1 : 0,
           (exStyle & WS_EX_TOOLWINDOW) ? 1 : 0);

    // Layered window attributes, if any.
    COLORREF key = 0;
    BYTE alpha = 0;
    DWORD flags = 0;
    if (GetLayeredWindowAttributes(hwnd, &key, &alpha, &flags)) {
        printf("%-18s   layered: alpha=%u flags=0x%08lX\n", "", alpha,
               (unsigned long)flags);
    }

    // Does the widget overlap the taskbar rectangle at all?
    if (taskbar) {
        RECT t{};
        GetWindowRect(taskbar, &t);
        bool overlaps = !(r.right <= t.left || r.left >= t.right ||
                          r.bottom <= t.top || r.top >= t.bottom);
        printf("%-18s   overlaps taskbar: %s\n", "", overlaps ? "yes" : "NO");
        printf("%-18s   taskbar rect: %ld,%ld - %ld,%ld\n", "", t.left, t.top,
               t.right, t.bottom);
    }
}

// Captures the widget's client area two ways:
//  * WM_PRINT into a memory DC reproduces what the widget would draw;
//  * a screen copy shows what is actually visible at that position.
static void CaptureToPng(HWND hwnd, const wchar_t* path, bool fromScreen) {
    RECT c{};
    GetClientRect(hwnd, &c);
    int w = c.right, h = c.bottom;
    if (w <= 0 || h <= 0) {
        printf("capture skipped: empty client area\n");
        return;
    }

    HDC screenDc = GetDC(nullptr);
    HDC memDc = CreateCompatibleDC(screenDc);
    HBITMAP bmp = CreateCompatibleBitmap(screenDc, w, h);
    HGDIOBJ old = SelectObject(memDc, bmp);

    if (fromScreen) {
        RECT r{};
        GetWindowRect(hwnd, &r);
        BitBlt(memDc, 0, 0, w, h, screenDc, r.left, r.top, SRCCOPY);
    } else {
        // Give the widget a real chance to paint itself.
        SendMessageW(hwnd, WM_PRINT, (WPARAM)memDc, PRF_CLIENT | PRF_ERASEBKGND);
    }

    Gdiplus::Bitmap out(bmp, nullptr);
    CLSID pngClsid{};
    CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}", &pngClsid);
    Gdiplus::Status status = out.Save(path, &pngClsid, nullptr);
    printf("capture (%s) -> %s : %s\n", fromScreen ? "screen" : "wm_print",
           Narrow(path),
           status == Gdiplus::Ok ? "saved" : "FAILED");

    SelectObject(memDc, old);
    DeleteObject(bmp);
    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);
}

int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // Where to write the captures. Defaults to the current directory.
    std::wstring dir = L".";
    if (argc > 1) {
        dir = argv[1];
    }

    Gdiplus::GdiplusStartupInput gdiInput;
    ULONG_PTR gdiToken = 0;
    Gdiplus::GdiplusStartup(&gdiToken, &gdiInput, nullptr);

    printf("=== Taskbar media widget diagnostic ===\n\n");

    EnumWindows(EnumProc, 0);

    ReportWindow("widget", found);
    printf("\n");
    ReportWindow("taskbar", taskbar);

    if (!found) {
        printf("\nThe widget window does not exist.\n");
        printf("That means the mod is not running, or it failed to create it.\n");
        printf("Check Windhawk: is the mod enabled, and does its log show\n");
        printf("'Widget created'?\n");
        Gdiplus::GdiplusShutdown(gdiToken);
        return 1;
    }

    printf("\n=== Captures ===\n");
    std::wstring wmPrintPath = dir + L"\\widget-wmprint.png";
    std::wstring screenPath = dir + L"\\widget-screen.png";
    CaptureToPng(found, wmPrintPath.c_str(), false);
    CaptureToPng(found, screenPath.c_str(), true);

    printf("\nSend both PNG files back.\n");

    Gdiplus::GdiplusShutdown(gdiToken);
    CoUninitialize();
    return 0;
}

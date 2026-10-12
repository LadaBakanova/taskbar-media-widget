// media-player / preview/orientation-probe.cpp
// Two measurements the visualizer's partial repaint depends on, neither of
// which is safe to assume:
//
//  1. how GDI+ stores a 32bpp ARGB bitmap in memory - whether the first row of
//     Scan0 is the top row of the picture or the bottom one. The partial
//     repaint copies the region under the bars pixel by pixel, and a wrong
//     answer would paste it back upside down;
//  2. whether a 32bpp DIB section pushed into a layered window with
//     UpdateLayeredWindow keeps its per-pixel alpha, which is what makes a
//     small patch blend into the surface instead of covering it.
//
// Build with the same compiler and flags as build-preview.ps1.

#include <windows.h>
#include <gdiplus.h>
#include <stdio.h>

int wmain() {
    ULONG_PTR token = 0;
    Gdiplus::GdiplusStartupInput input;
    if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok) {
        wprintf(L"GdiplusStartup failed\n");
        return 1;
    }

    // ---------------------------------------------------------------- stride
    {
        Gdiplus::Bitmap bitmap(4, 3, PixelFormat32bppARGB);
        Gdiplus::Graphics g(&bitmap);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));
        // Green on the top row, red on the bottom row, both opaque.
        Gdiplus::SolidBrush green(Gdiplus::Color(255, 0, 255, 0));
        Gdiplus::SolidBrush red(Gdiplus::Color(255, 255, 0, 0));
        g.FillRectangle(&green, Gdiplus::Rect(0, 0, 4, 1));
        g.FillRectangle(&red, Gdiplus::Rect(0, 2, 4, 1));

        Gdiplus::Rect rect(0, 0, 4, 3);
        Gdiplus::BitmapData data{};
        if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead,
                            PixelFormat32bppARGB, &data) != Gdiplus::Ok) {
            wprintf(L"LockBits failed\n");
            return 1;
        }
        const BYTE* row0 = (const BYTE*)data.Scan0;
        const BYTE* row2 = row0 + (size_t)2 * data.Stride;
        wprintf(L"stride          : %d\n", data.Stride);
        wprintf(L"Scan0 first px  : B=%u G=%u R=%u A=%u\n", row0[0], row0[1],
                row0[2], row0[3]);
        wprintf(L"row2 first px   : B=%u G=%u R=%u A=%u\n", row2[0], row2[1],
                row2[2], row2[3]);
        wprintf(L"Scan0 is the    : %s row\n",
                (row0[1] > row0[2]) ? L"TOP (G>R)" : L"BOTTOM (R>G)");
        bitmap.UnlockBits(&data);
    }

    // -------------------------------------------------------- layered window
    {
        const int kWidth = 20;
        const int kHeight = 10;
        HDC screen = GetDC(nullptr);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = kWidth;
        info.bmiHeader.biHeight = -kHeight;  // top-down, as GDI+ gives it
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HBITMAP dib = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits,
                                       nullptr, 0);
        if (!dib || !bits) {
            wprintf(L"CreateDIBSection failed\n");
            return 1;
        }
        // Opaque green in the top-left quarter, fully transparent elsewhere.
        BYTE* pixels = (BYTE*)bits;
        for (int y = 0; y < kHeight; ++y) {
            for (int x = 0; x < kWidth; ++x) {
                BYTE* p = pixels + ((size_t)y * kWidth + x) * 4;
                bool inside = x < kWidth / 2 && y < kHeight / 2;
                p[0] = 0;                          // blue
                p[1] = inside ? 255 : 0;           // green
                p[2] = 0;                          // red
                p[3] = inside ? 255 : 0;           // alpha
            }
        }

        HWND window = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW,
                                      L"STATIC", L"", WS_POPUP, 0, 0, kWidth,
                                      kHeight, nullptr, nullptr, nullptr, nullptr);
        if (!window) {
            wprintf(L"CreateWindowExW failed: %u\n", GetLastError());
            return 1;
        }
        HDC memDc = CreateCompatibleDC(screen);
        HBITMAP old = (HBITMAP)SelectObject(memDc, dib);
        POINT dest{100, 100};
        SIZE size{kWidth, kHeight};
        POINT source{0, 0};
        BLENDFUNCTION blend{};
        blend.BlendOp = AC_SRC_OVER;
        blend.SourceConstantAlpha = 255;
        blend.AlphaFormat = AC_SRC_ALPHA;
        BOOL ok = UpdateLayeredWindow(window, screen, &dest, &size, memDc, &source,
                                      0, &blend, ULW_ALPHA);
        wprintf(L"UpdateLayeredWindow: %s\n", ok ? L"ok" : L"failed");
        SelectObject(memDc, old);

        // Read the screen back. The window is never shown, but a layered window
        // is composited as soon as it is updated, so the pixels are there.
        HDC screenCopy = CreateCompatibleDC(screen);
        HBITMAP capture = CreateCompatibleBitmap(screen, kWidth, kHeight);
        HBITMAP oldCapture = (HBITMAP)SelectObject(screenCopy, capture);
        BitBlt(screenCopy, 0, 0, kWidth, kHeight, screen, 100, 100, SRCCOPY);
        auto pixel = [&](int x, int y) {
            COLORREF c = GetPixel(screenCopy, x, y);
            return (int)((GetRValue(c) << 16) | (GetGValue(c) << 8) | GetBValue(c));
        };
        wprintf(L"top-left quarter    : 0x%06X (expect 0x00FF00)\n", pixel(2, 2));
        wprintf(L"bottom-right quarter: 0x%06X (expect the desktop, not black)\n",
                pixel(kWidth - 3, kHeight - 3));
        SelectObject(screenCopy, oldCapture);
        DeleteObject(capture);
        DeleteDC(screenCopy);
        DeleteDC(memDc);
        DestroyWindow(window);
        DeleteObject(dib);
        ReleaseDC(nullptr, screen);
    }

    Gdiplus::GdiplusShutdown(token);
    return 0;
}

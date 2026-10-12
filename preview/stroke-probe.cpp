// stroke-probe.cpp
// What GDI+ actually does with a 1.5 unit stroke, with and without the pixel
// offset mode the widget uses. Printed as one row of pixel values per case.

#include <windows.h>
#include <gdiplus.h>
#include <stdio.h>

int wmain() {
    ULONG_PTR token = 0;
    Gdiplus::GdiplusStartupInput input;
    Gdiplus::GdiplusStartup(&token, &input, nullptr);

    struct Case {
        const wchar_t* name;
        Gdiplus::REAL width;
        Gdiplus::REAL x;
        Gdiplus::PixelOffsetMode offset;
    } cases[] = {
        {L"w=1.5 x=406.5 half", 1.5f, 406.5f, Gdiplus::PixelOffsetModeHalf},
        {L"w=1.5 x=406.5 none", 1.5f, 406.5f, Gdiplus::PixelOffsetModeNone},
        {L"w=1.5 x=406.0 half", 1.5f, 406.0f, Gdiplus::PixelOffsetModeHalf},
        {L"w=2.0 x=406.5 half", 2.0f, 406.5f, Gdiplus::PixelOffsetModeHalf},
        {L"w=1.0 x=406.5 half", 1.0f, 406.5f, Gdiplus::PixelOffsetModeHalf},
        {L"w=1.5 rect 405.5..407 half", 0.0f, 0.0f, Gdiplus::PixelOffsetModeHalf},
    };

    for (auto const& c : cases) {
        Gdiplus::Bitmap bitmap(420, 20, PixelFormat32bppARGB);
        Gdiplus::Graphics g(&bitmap);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(c.offset);
        g.Clear(Gdiplus::Color(255, 0x1a, 0x1a, 0x1a));

        Gdiplus::SolidBrush brush(Gdiplus::Color(255, 255, 255, 255));
        if (c.name[0] == L'w' && c.width > 0) {
            Gdiplus::Pen pen(Gdiplus::Color(255, 255, 255, 255), c.width);
            g.DrawLine(&pen, c.x, 3.0f, c.x, 17.0f);
        } else {
            g.FillRectangle(&brush, Gdiplus::RectF(405.5f, 3.0f, 1.5f, 14.0f));
        }

        wprintf(L"%-30s :", c.name);
        for (int x = 403; x <= 410; ++x) {
            Gdiplus::Color pixel;
            bitmap.GetPixel(x, 10, &pixel);
            wprintf(L" %3d", (int)pixel.GetRed());
        }
        wprintf(L"\n");
    }

    Gdiplus::GdiplusShutdown(token);
    return 0;
}

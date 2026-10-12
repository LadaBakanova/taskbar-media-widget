// text-probe.cpp
// Renders the design's three text strings with the two GDI+ text paths, so the
// one that matches the design's own metrics can be picked for the widget.
//
//   A: Graphics::DrawString with an explicit baseline (hinted advances)
//   B: Graphics::DrawDriverString with realized advances
//
// Build: clang++ ... text-probe.cpp -o text-probe.exe -lgdiplus -l:libc++.a ...

#include <windows.h>
#include <gdiplus.h>
#include <stdio.h>
#include <string>

namespace {

int GetEncoderClsid(const WCHAR* format, CLSID* clsid) {
    UINT count = 0, size = 0;
    if (Gdiplus::GetImageEncodersSize(&count, &size) != Gdiplus::Ok || !size) return -1;
    auto* info = (Gdiplus::ImageCodecInfo*)malloc(size);
    int found = -1;
    if (info && Gdiplus::GetImageEncoders(count, size, info) == Gdiplus::Ok) {
        for (UINT i = 0; i < count; ++i) {
            if (!wcscmp(info[i].MimeType, format)) {
                *clsid = info[i].Clsid;
                found = (int)i;
                break;
            }
        }
    }
    free(info);
    return found;
}

std::wstring Dir() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring file(path);
    size_t slash = file.find_last_of(L'\\');
    return slash == std::wstring::npos ? std::wstring() : file.substr(0, slash);
}

void FillBackground(Gdiplus::Graphics& g, int width, int height) {
    Gdiplus::SolidBrush panel(Gdiplus::Color(255, 0x1a, 0x1a, 0x1a));
    g.FillRectangle(&panel, 0, 0, width, height);
}

Gdiplus::REAL DrawStringBaseline(Gdiplus::Graphics& g, std::wstring const& text,
                                 Gdiplus::REAL left, Gdiplus::REAL baseline,
                                 Gdiplus::FontFamily* family, Gdiplus::REAL size) {
    Gdiplus::Font font(family, size, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    INT em = family->GetEmHeight(Gdiplus::FontStyleRegular);
    UINT16 ascent = family->GetCellAscent(Gdiplus::FontStyleRegular);
    Gdiplus::REAL ascentPixels = size * (Gdiplus::REAL)ascent / (Gdiplus::REAL)em;

    Gdiplus::StringFormat format;
    format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
    Gdiplus::RectF rect(left, baseline - ascentPixels, 400.0f, size * 3.0f);
    Gdiplus::SolidBrush brush(Gdiplus::Color(255, 255, 255, 255));
    g.DrawString(text.c_str(), -1, &font, rect, &format, &brush);

    Gdiplus::RectF bounds;
    g.MeasureString(text.c_str(), -1, &font, rect, &format, &bounds);
    return bounds.Width;
}

Gdiplus::REAL DriverString(Gdiplus::Graphics& g, std::wstring const& text,
                           Gdiplus::REAL left, Gdiplus::REAL baseline,
                           Gdiplus::FontFamily* family, Gdiplus::REAL size,
                           bool realized) {
    Gdiplus::Font font(family, size, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    INT flags = Gdiplus::DriverStringOptionsCmapLookup;
    if (realized) {
        flags |= Gdiplus::DriverStringOptionsRealizedAdvance;
    }

    Gdiplus::PointF origin(left, baseline);
    Gdiplus::RectF bounds;
    Gdiplus::Status measure = g.MeasureDriverString(
        (const UINT16*)text.c_str(), (INT)text.size(), &font, &origin, flags,
        nullptr, &bounds);

    Gdiplus::SolidBrush brush(Gdiplus::Color(255, 255, 255, 255));
    Gdiplus::Status draw = g.DrawDriverString((const UINT16*)text.c_str(),
                                              (INT)text.size(), &font, &brush,
                                              &origin, flags, nullptr);
    if (measure != Gdiplus::Ok || draw != Gdiplus::Ok) {
        wprintf(L"driver: measure=%d draw=%d\n", (int)measure, (int)draw);
    }
    return bounds.Width;
}

}  // namespace

int wmain() {
    ULONG_PTR token = 0;
    Gdiplus::GdiplusStartupInput input;
    Gdiplus::GdiplusStartup(&token, &input, nullptr);

    Gdiplus::PrivateFontCollection collection;
    std::wstring dir = Dir();
    collection.AddFontFile((dir + L"\\Inter-Regular.ttf").c_str());
    collection.AddFontFile((dir + L"\\Inter-Medium.ttf").c_str());
    INT count = collection.GetFamilyCount();
    auto* families = new Gdiplus::FontFamily[count];
    INT found = 0;
    collection.GetFamilies(count, families, &found);
    Gdiplus::FontFamily* regular = nullptr;
    Gdiplus::FontFamily* medium = nullptr;
    for (INT i = 0; i < found; ++i) {
        WCHAR name[LF_FACESIZE]{};
        families[i].GetFamilyName(name);
        if (!wcscmp(name, L"Inter")) regular = &families[i];
        if (!wcscmp(name, L"Inter Medium")) medium = &families[i];
    }
    wprintf(L"families: %d, regular=%p medium=%p\n", (int)found, (void*)regular,
            (void*)medium);

    const int width = 466;
    const int height = 42;
    const wchar_t* out = L"text-probe.png";

    struct Case {
        const wchar_t* name;
        Gdiplus::FontFamily* family;
        Gdiplus::REAL size;
        Gdiplus::REAL baseline;
        const wchar_t* text;
    } cases[] = {
        {L"title", medium, 12.0f, 17.87f, L"Спектакль окончен"},
        {L"artist", regular, 11.0f, 32.50f, L"Полина Гагарина"},
        {L"time", regular, 11.0f, 32.50f, L"1:15 / 4:00"},
    };

    for (int variant = 0; variant < 3; ++variant) {
        Gdiplus::Bitmap bitmap(width, height, PixelFormat32bppARGB);
        Gdiplus::Graphics g(&bitmap);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        FillBackground(g, width, height);

        Gdiplus::REAL widths[3] = {0, 0, 0};
        for (int i = 0; i < 3; ++i) {
            Gdiplus::REAL left = 44.5f;
            switch (variant) {
                case 0:
                    widths[i] = DrawStringBaseline(g, cases[i].text, left,
                                                   cases[i].baseline,
                                                   cases[i].family, cases[i].size);
                    break;
                case 1:
                    widths[i] = DriverString(g, cases[i].text, left,
                                             cases[i].baseline, cases[i].family,
                                             cases[i].size, false);
                    break;
                default:
                    widths[i] = DriverString(g, cases[i].text, left,
                                             cases[i].baseline, cases[i].family,
                                             cases[i].size, true);
                    break;
            }
        }

        wchar_t name[64];
        swprintf(name, 64, L"%s-variant%d.png", out, variant);
        CLSID png;
        GetEncoderClsid(L"image/png", &png);
        bitmap.Save(name, &png, nullptr);
        wprintf(L"variant %d: title %.2f  artist %.2f  time %.2f  -> %s\n", variant,
                widths[0], widths[1], widths[2], name);
    }

    Gdiplus::GdiplusShutdown(token);
    return 0;
}

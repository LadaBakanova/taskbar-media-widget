// media-player / preview.cpp
// Renders the widget's painting off screen, without Windhawk and without
// touching the installed mod, so it can be compared with the design pixel by
// pixel. The mod source is included as is: the drawing code under test is the
// drawing code that ships.
//
// Build with build-preview.ps1, run with an output directory as its argument.
//
// The frames are rendered at the width the layout is drawn for, which is the
// mod's own design::kPanelWidth: the design's 466 less the two shifts the
// layout carries (35 for the right-hand group, 40 more for the time and the bar
// with it). Reading it from the mod source instead of repeating the number is
// what keeps this file from drawing a frame the widget no longer paints.
// Compared with a frame from before 2.8.0 the picture reads the other way round
// than the screen does - the widget itself is placed by its right edge, so on
// the taskbar the two texts are what moves.

#include <windows.h>
#include <gdiplus.h>
#include <stdio.h>

#include "../taskbar-media-widget.wh.cpp"

namespace {

constexpr int kWidth = (int)design::kPanelWidth;
constexpr int kHeight = (int)design::kPanelHeight;

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

MediaState MakeState(bool playing, long long positionMs, long long durationMs) {
    MediaState state;
    state.title = L"Спектакль окончен";
    state.artist = L"Полина Гагарина";
    state.playing = playing;
    state.hasSession = true;
    state.canSeek = true;
    state.anchorPositionMs = positionMs;
    state.anchorTick = 0;  // no interpolation: draw exactly this position
    state.durationMs = durationMs;
    state.hasArt = false;
    return state;
}

// The design's cover slot holds an image the document no longer has, so the
// widget's own slot is checked with a flat colour instead: the position, the size
// and the rounded clip are what matter.
MediaState MakeArtState() {
    MediaState state = MakeState(true, 75000, 240000);
    state.artSize = 64;
    state.artPixels.assign((size_t)state.artSize * state.artSize * 4, 0);
    for (int y = 0; y < state.artSize; ++y) {
        for (int x = 0; x < state.artSize; ++x) {
            BYTE* pixel = &state.artPixels[((size_t)y * state.artSize + x) * 4];
            pixel[0] = 0x40;  // blue
            pixel[1] = 0x80;  // green
            pixel[2] = 0xff;  // red
            pixel[3] = 0xff;  // alpha
        }
    }
    state.hasArt = true;
    return state;
}

bool Render(const wchar_t* path, int width, int height, MediaState const& state,
            long long positionMs, bool followTaskbarTheme,
            AnimationFrame const& frame) {
    Settings settings;
    settings.widgetWidth = width;
    settings.widgetHeight = height;
    settings.opacity = 255;
    settings.showProgressBar = true;
    settings.followTaskbarTheme = followTaskbarTheme;
    settings.visualizer = false;
    {
        std::lock_guard<std::mutex> lock(g_settingsMutex);
        g_settings = settings;
    }
    RefreshSurface(true);

    Gdiplus::Bitmap bitmap(width, height, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics graphics(&bitmap);
        PaintWidget(graphics, width, height, 96, settings, state, positionMs,
                    HoverTarget::None, frame);
    }

    CLSID png;
    if (GetEncoderClsid(L"image/png", &png) < 0) {
        wprintf(L"no PNG encoder\n");
        return false;
    }
    Gdiplus::Status status = bitmap.Save(path, &png, nullptr);
    wprintf(L"%s: %s\n", path, status == Gdiplus::Ok ? L"saved" : L"FAILED");
    return status == Gdiplus::Ok;
}

// The heights the design gives its bars, which is what is drawn when the
// visualizer is off.
AnimationFrame DesignFrame() {
    AnimationFrame frame;
    for (int i = 0; i < design::kMeterCount; ++i) {
        frame.heightUnits[i] = (float)design::kMeterLevels[i];
    }
    return frame;
}

// A frame caught mid-movement, for the picture of the visualizer doing
// something. The heights follow the same decibel scale the widget uses - bass
// near the top, mids about six dB down, highs about fifteen - so the picture
// shows the strip as it actually looks rather than as an extreme.
AnimationFrame LiveFrame() {
    const double dbDown[kScopeBands] = {3.0, 9.0, 18.0};
    AnimationFrame frame;
    for (int i = 0; i < design::kMeterCount; ++i) {
        int channel = 0;
        int band = 0;
        MeterBandFor(i, channel, band);
        float level = (float)std::sqrt(1.0 - dbDown[band] / kScopeRangeDb);
        frame.heightUnits[i] =
            design::kMeterMinHeight +
            (design::kMeterMaxHeight - design::kMeterMinHeight) * level;
    }
    frame.moving = true;
    return frame;
}

// The visualizer's fast path, checked against the slow one.
//
// A frame on screen is the base - everything that does not move with the music
// - with the current bars drawn over it. The animation does not repaint the
// base; it copies it and draws the bars. If those two ever disagree, the bars
// leave ghosts of their old heights, or the panel goes missing under them.
//
// So this builds a frame the cheap way and a frame the expensive way, and
// compares every pixel of the widget. `from` and `to` are the heights the bars
// have before and after, which covers growing and shrinking - where a stale
// pixel would show.
bool CheckPartialRepaint(const wchar_t* label, MediaState const& state,
                         long long positionMs, AnimationFrame const& from,
                         AnimationFrame const& to) {
    Settings settings;
    settings.widgetWidth = kWidth;
    settings.widgetHeight = kHeight;
    settings.opacity = 255;
    settings.showProgressBar = true;
    settings.followTaskbarTheme = false;
    settings.visualizer = true;
    {
        std::lock_guard<std::mutex> lock(g_settingsMutex);
        g_settings = settings;
    }
    RefreshSurface(true);

    // The expensive way: paint everything, including the new bars.
    Gdiplus::Bitmap expected(kWidth, kHeight, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(&expected);
        PaintWidget(g, kWidth, kHeight, 96, settings, state, positionMs, HoverTarget::None,
                    to);
    }

    // The cheap way: the same base the widget builds, then the frames composed
    // over it exactly as the animation composes them - the previous bars first,
    // then this frame's, so the early-out that skips an unchanged frame is
    // exercised the same way it is in the widget.
    {
        std::lock_guard<std::mutex> lock(g_baseMutex);
        RenderBaseLocked(kWidth, kHeight, 96, settings, state, positionMs);
        RenderFrameLocked(nullptr, kWidth, kHeight, 96, settings, state, positionMs, from,
                          false);
        Gdiplus::Bitmap* got =
            RenderFrameLocked(nullptr, kWidth, kHeight, 96, settings, state, positionMs,
                              to, false);
        if (!got || got != g_frameBitmap) {
            wprintf(L"partial repaint %s: no frame was composed\n", label);
            return false;
        }
    }

    int mismatches = 0;
    int worst = 0;
    {
        std::lock_guard<std::mutex> lock(g_baseMutex);
        for (int y = 0; y < kHeight; ++y) {
            for (int x = 0; x < kWidth; ++x) {
                Gdiplus::Color got;
                Gdiplus::Color want;
                g_frameBitmap->GetPixel(x, y, &got);
                expected.GetPixel(x, y, &want);
                int diff = abs((int)got.GetA() - (int)want.GetA()) +
                           abs((int)got.GetR() - (int)want.GetR()) +
                           abs((int)got.GetG() - (int)want.GetG()) +
                           abs((int)got.GetB() - (int)want.GetB());
                if (diff > 0) {
                    if (mismatches < 6) {
                        wprintf(L"   frame(%d,%d)=%08X  full(%d,%d)=%08X\n", x, y,
                                (unsigned)got.GetValue(), x, y,
                                (unsigned)want.GetValue());
                    }
                    ++mismatches;
                    if (diff > worst) {
                        worst = diff;
                    }
                }
            }
        }
    }
    wprintf(L"partial repaint %s: %d of %d pixels differ (worst %d of 1020)\n",
            label, mismatches, kWidth * kHeight, worst);
    return mismatches == 0;
}

// The box the bars occupy on a surface, clipped to it. The widget itself does
// not need this any more - it pushes the whole surface - but cutting the strip
// out of a frame is how the visualizer gets looked at on its own.
Gdiplus::Rect MeterRegion(Gdiplus::Bitmap& surface, int dpi) {
    Gdiplus::RectF box = MeterBounds(dpi);
    int left = (int)std::floor(box.X);
    int top = (int)std::floor(box.Y);
    int right = (int)std::ceil(box.X + box.Width);
    int bottom = (int)std::ceil(box.Y + box.Height);
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right > (int)surface.GetWidth()) right = (int)surface.GetWidth();
    if (bottom > (int)surface.GetHeight()) bottom = (int)surface.GetHeight();
    if (right <= left || bottom <= top) {
        return Gdiplus::Rect(0, 0, 0, 0);
    }
    return Gdiplus::Rect(left, top, right - left, bottom - top);
}

// The strip on its own, saved so it can be looked at without the rest of the
// widget around it: the animated frame with its region cut out of it.
void SaveStrip(const wchar_t* path, AnimationFrame const& frame) {
    Settings settings;
    settings.widgetWidth = kWidth;
    settings.widgetHeight = kHeight;
    settings.opacity = 255;
    settings.showProgressBar = true;
    settings.followTaskbarTheme = false;
    settings.visualizer = true;
    {
        std::lock_guard<std::mutex> lock(g_settingsMutex);
        g_settings = settings;
    }
    RefreshSurface(true);
    MediaState state = MakeState(true, 75000, 240000);
    std::lock_guard<std::mutex> lock(g_baseMutex);
    RenderBaseLocked(kWidth, kHeight, 96, settings, state, 75000);
    Gdiplus::Bitmap* composed =
        RenderFrameLocked(nullptr, kWidth, kHeight, 96, settings, state, 75000, frame,
                          false);
    if (!composed) {
        return;
    }
    Gdiplus::Rect region = MeterRegion(*composed, 96);
    Gdiplus::Bitmap strip(region.Width, region.Height, PixelFormat32bppARGB);
    if (!CopyRegion(*composed, strip, region)) {
        return;
    }
    CLSID png;
    if (GetEncoderClsid(L"image/png", &png) >= 0) {
        strip.Save(path, &png, nullptr);
    }
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        wprintf(L"usage: preview.exe <output directory>\n");
        return 2;
    }

    ULONG_PTR token = 0;
    Gdiplus::GdiplusStartupInput input;
    if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok) {
        wprintf(L"GdiplusStartup failed\n");
        return 1;
    }

    FontSet& fonts = Fonts();
    wchar_t regular[LF_FACESIZE]{};
    wchar_t medium[LF_FACESIZE]{};
    if (fonts.regular) fonts.regular->GetFamilyName(regular);
    if (fonts.medium) fonts.medium->GetFamilyName(medium);
    wprintf(L"library   : %s\n", LibraryDirectory().c_str());
    wprintf(L"inter     : %s\n", fonts.inter ? L"yes" : L"no");
    wprintf(L"regular   : %s\n", regular);
    wprintf(L"medium    : %s\n", medium);

    std::wstring dir = argv[1];
    if (!dir.empty() && dir.back() != L'\\') {
        dir += L'\\';
    }

    // 1:15 into a 4:00 track is the design's own time text.
    const long long duration = 240000;
    const long long designPosition = 75000;
    // 42.34% is the design's own progress fill (58 of 137).
    const long long fillPosition = 101606;

    // The design's own panel first, then the same widget wearing the theme that
    // the Windows 11 Taskbar Styler mod has selected, then the visualizer with
    // the bars caught mid-movement.
    Render((dir + L"widget-playing.png").c_str(), kWidth, kHeight,
           MakeState(true, designPosition, duration), designPosition, false,
           DesignFrame());
    Render((dir + L"widget-paused.png").c_str(), kWidth, kHeight,
           MakeState(false, designPosition, duration), designPosition, false,
           DesignFrame());
    Render((dir + L"widget-fill58.png").c_str(), kWidth, kHeight,
           MakeState(true, fillPosition, duration), fillPosition, false,
           DesignFrame());
    Render((dir + L"widget-art.png").c_str(), kWidth, kHeight, MakeArtState(), 75000, false,
           DesignFrame());
    Render((dir + L"widget-theme.png").c_str(), kWidth, kHeight,
           MakeState(true, fillPosition, duration), fillPosition, true,
           DesignFrame());
    Render((dir + L"widget-visualizer.png").c_str(), kWidth, kHeight,
           MakeState(true, designPosition, duration), designPosition, false,
           LiveFrame());
    Render((dir + L"widget-visualizer-theme.png").c_str(), kWidth, kHeight,
           MakeState(true, fillPosition, duration), fillPosition, true,
           LiveFrame());

    // The frame that shows the cost of the second compaction (design::kTimeShift):
    // a track past ten minutes needs a wider time block than the room left of it,
    // so the elapsed time's first digits land under the meter's strokes. Rendered
    // so the overlap can be measured instead of guessed at: the sample is
    // "61:53 / 65:00", two digits wider than the design's own "1:15 / 4:00".
    Render((dir + L"widget-longtime.png").c_str(), kWidth, kHeight,
           MakeState(true, 3713000, 3900000), 3713000, false, DesignFrame());

    // The widest time the layout keeps room for (2.8.8): eight characters on
    // each side of the slash. "20:00:00 / 20:00:00" is within a pixel of the
    // "00:00:00 / 00:00:00" the room is sized by, and a real track can show it.
    Render((dir + L"widget-maxtime.png").c_str(), kWidth, kHeight,
           MakeState(true, 72000000, 72000000), 72000000, false, DesignFrame());

    // The animation's fast path has to agree with a whole repaint, both while
    // the bars grow and while they shrink: anything else leaves ghosts of the
    // old heights on the strip.
    AnimationFrame loud = LiveFrame();
    AnimationFrame quiet = DesignFrame();
    MediaState playing = MakeState(true, designPosition, duration);
    bool quietToLoud =
        CheckPartialRepaint(L"rising ", playing, designPosition, quiet, loud);
    bool loudToQuiet =
        CheckPartialRepaint(L"falling", playing, designPosition, loud, quiet);
    wprintf(L"partial repaint : %s\n",
            (quietToLoud && loudToQuiet) ? L"identical to a whole repaint"
                                         : L"MISMATCH, the strip would ghost");
    SaveStrip((dir + L"meter-strip.png").c_str(), loud);

    styler::Surface surface = GetSurface();
    wprintf(L"styler      : theme \"%s\", %s\n", surface.themeName.c_str(),
            surface.fromTheme ? L"applied" : L"not found");    if (surface.fromTheme) {
        wprintf(L"   radius   : %.1f, border %.2f %s\n", surface.cornerRadius,
                surface.borderThickness,
                surface.border.gradient ? L"(gradient)" : L"(solid)");
        wprintf(L"   panel    : %s alpha %d (%d,%d,%d), %d stops\n",
                surface.background.gradient ? L"gradient" : L"solid",
                surface.background.a, surface.background.r, surface.background.g,
                surface.background.b, surface.background.stopCount);
        wprintf(L"   border   : %s alpha %d (%d,%d,%d), %d stops\n",
                surface.border.gradient ? L"gradient" : L"solid", surface.border.a,
                surface.border.r, surface.border.g, surface.border.b,
                surface.border.stopCount);
        for (int i = 0; i < surface.border.stopCount; ++i) {
            wprintf(L"      stop %d: offset %.2f alpha %d (%d,%d,%d)\n", i,
                    surface.border.stops[i].offset, surface.border.stops[i].a,
                    surface.border.stops[i].r, surface.border.stops[i].g,
                    surface.border.stops[i].b);
        }
    }

    Gdiplus::GdiplusShutdown(token);
    return 0;
}

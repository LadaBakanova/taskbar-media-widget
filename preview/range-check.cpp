// media-player / preview/range-check.cpp
// Checks the visualizer's automatic range against the two things it has to do
// at once: leave the progress bar alone even at the top of a bar, and keep the
// strip alive on material that is quiet or heavily compressed.
//
// It drives AnimationFrameNow - the same function the widget draws from - with
// a scripted level history, one frame at a time, and reports the bar heights it
// produces. Nothing here is part of the mod.
//
// Build like build-preview.ps1, then run: range-check.exe

#include <windows.h>
#include <stdio.h>

#include "../taskbar-media-widget.wh.cpp"

namespace {

int g_failures = 0;

void Check(bool condition, const wchar_t* what) {
    wprintf(L"   %s %s\n", condition ? L"ok  " : L"FAIL", what);
    if (!condition) {
        ++g_failures;
    }
}

// The level the harness is feeding in, republished as the analyser would.
float g_current[2][kScopeBands] = {};

void SetFrame(const float rms[2][kScopeBands]) {
    for (int c = 0; c < 2; ++c) {
        for (int b = 0; b < kScopeBands; ++b) {
            g_current[c][b] = rms[c][b];
        }
    }
}

void PublishFrame() {
    g_scope.sequence.fetch_add(1, std::memory_order_acq_rel);
    for (int c = 0; c < 2; ++c) {
        for (int b = 0; b < kScopeBands; ++b) {
            g_scope.rms[c][b] = g_current[c][b];
        }
    }
    g_scope.tick.store(GetTickCount(), std::memory_order_relaxed);
    g_scope.sequence.fetch_add(1, std::memory_order_acq_rel);
}

// What the widget would draw right now, at a steady frame rate.
//
// The frame is republished every 10 ms while the frames are drawn, which is
// what the real analyser does: a test that published once and then drew a
// hundred frames would be measuring a widget whose scope had gone stale, not
// the behaviour of the range.
AnimationFrame Step(double seconds, bool playing) {
    MediaState state;
    state.playing = playing;
    state.hasSession = true;
    Settings settings;
    settings.visualizer = true;
    settings.visualizerGain = 100;

    AnimationFrame frame;
    int frames = (int)(seconds / 0.033);
    for (int i = 0; i < frames; ++i) {
        Sleep(33);
        PublishFrame();
        frame = AnimationFrameNow(state, settings);
    }
    return frame;
}

float MeterSpan(const AnimationFrame& frame) {
    float lowest = frame.heightUnits[0];
    float highest = frame.heightUnits[0];
    for (int i = 0; i < design::kMeterCount; ++i) {
        if (frame.heightUnits[i] < lowest) lowest = frame.heightUnits[i];
        if (frame.heightUnits[i] > highest) highest = frame.heightUnits[i];
    }
    return highest - lowest;
}

float Tallest(const AnimationFrame& frame) {
    float highest = frame.heightUnits[0];
    for (int i = 0; i < design::kMeterCount; ++i) {
        if (frame.heightUnits[i] > highest) highest = frame.heightUnits[i];
    }
    return highest;
}

void Report(const wchar_t* label, const AnimationFrame& frame) {
    wprintf(L"   %-22s bars:", label);
    for (int i = 0; i < design::kMeterCount; ++i) {
        wprintf(L" %.1f", frame.heightUnits[i]);
    }
    wprintf(L"\n");
}

const double kGapTop = design::kMeterBottom - design::kMeterMaxHeight;
const double kBarBottomEdge = design::kBarTop + design::kBarHeight;

}  // namespace

int wmain() {
    wprintf(L"=== visualizer range check ===\n\n");
    wprintf(L"progress bar occupies y %.1f..%.1f; a bar may reach y %.1f, "
            L"leaving %.1f units of panel\n\n",
            (double)design::kBarTop, kBarBottomEdge, (double)kGapTop,
            kGapTop - kBarBottomEdge);

    ResetScopeReference();

    // ---------------------------------------------------------------------
    // 1. A loud track: the reference may not run away, and the tallest bar
    //    may never touch the progress bar.
    wprintf(L"loud track (bass 0.016, mids 0.008, highs 0.004):\n");
    {
        const float rms[2][kScopeBands] = {{0.016f, 0.008f, 0.004f},
                                           {0.016f, 0.008f, 0.004f}};
        SetFrame(rms);
        AnimationFrame frame = Step(4.0, true);
        Report(L"loud", frame);
        Check(Tallest(frame) <= design::kMeterMaxHeight + 0.01f,
              L"no bar passes the maximum height");
        Check(Tallest(frame) > design::kMeterMaxHeight * 0.7f,
              L"the loud track fills the strip");
    }

    // ---------------------------------------------------------------------
    // 2. The same track turning quiet: the request was that this must not
    //    leave the strip empty.
    wprintf(L"\nthe same track, now four times quieter (bass 0.004):\n");
    {
        const float rms[2][kScopeBands] = {{0.004f, 0.002f, 0.001f},
                                           {0.004f, 0.002f, 0.001f}};
        SetFrame(rms);
        AnimationFrame frame = Step(3.0, true);
        Report(L"quiet passage", frame);
        Check(Tallest(frame) > 8.0f,
              L"a quiet passage still reaches a third of the strip");
        Check(MeterSpan(frame) > 2.0f,
              L"the strip still has shape, not one flat row");
    }

    // ---------------------------------------------------------------------
    // 3. A quiet master - the case a fixed scale is worst at.
    wprintf(L"\nquiet master (bass 0.0015, mids 0.0008, highs 0.0004):\n");
    {
        const float rms[2][kScopeBands] = {{0.0015f, 0.0008f, 0.0004f},
                                           {0.0015f, 0.0008f, 0.0004f}};
        SetFrame(rms);
        AnimationFrame frame = Step(6.0, true);
        Report(L"quiet master", frame);
        Check(Tallest(frame) > 7.0f, L"a quiet master still fills the strip");
    }

    // ---------------------------------------------------------------------
    // 4. Silence: the strip has to come down and stay down. The automatic
    //    range must not amplify nothing.
    wprintf(L"\nsilence, player still says it is playing:\n");
    {
        const float rms[2][kScopeBands] = {};
        SetFrame(rms);
        AnimationFrame frame = Step(4.0, true);
        Report(L"silence", frame);
        Check(Tallest(frame) <= design::kMeterMinHeight + 0.6f,
              L"silence leaves the bars at their minimum");
    }

    // ---------------------------------------------------------------------
    // 5. And the cap: a signal far below the floor must not be amplified into
    //    a full strip by the automatic range.
    wprintf(L"\nbackground noise (bass 0.0002, below the floor):\n");
    {
        const float rms[2][kScopeBands] = {{0.0002f, 0.0001f, 0.00005f},
                                           {0.0002f, 0.0001f, 0.00005f}};
        SetFrame(rms);
        AnimationFrame frame = Step(4.0, true);
        Report(L"noise", frame);
        Check(Tallest(frame) <= design::kMeterMinHeight + 1.5f,
              L"noise stays at the bottom of the strip");
    }

    // ---------------------------------------------------------------------
    // 6. The reference must come back up when the music does, and it must not
    //    be capped at the calibrated scale.
    wprintf(L"\nback to a very loud passage (bass 0.06):\n");
    {
        const float rms[2][kScopeBands] = {{0.06f, 0.03f, 0.015f},
                                           {0.06f, 0.03f, 0.015f}};
        SetFrame(rms);
        AnimationFrame frame = Step(3.0, true);
        Report(L"very loud", frame);
        Check(Tallest(frame) <= design::kMeterMaxHeight + 0.01f,
              L"the loudest passage still respects the maximum");
    }

    wprintf(L"\n%s\n", g_failures == 0 ? L"all checks passed"
                                       : L"there were failures");
    return g_failures == 0 ? 0 : 1;
}

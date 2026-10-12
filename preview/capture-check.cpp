// media-player / preview/capture-check.cpp
// Drives the mod's own loopback capture from a plain console program.
//
// The unit test for the visualizer is not in the widget: the widget can only be
// observed through the taskbar, and Windhawk's log is off by default. This
// includes the mod source as it is - the same CaptureThreadProc, the same
// ScopeAnalyser - starts the capture exactly as Wh_ModInit does, and reports
// what the drawing thread would see, once a second.
//
// It is also the check that the band split moves the bars the way it is meant
// to: bass, mids and highs should not report the same numbers on real music.
//
// Build with the same compiler and flags as build-preview.ps1, then:
//   capture-check.exe            (10 seconds)
//   capture-check.exe 30         (30 seconds)
//
// Start some music first: the capture is of the playback device, so silence is
// what it reports when nothing is playing.

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "../taskbar-media-widget.wh.cpp"

int wmain(int argc, wchar_t** argv) {
    int seconds = argc > 1 ? _wtoi(argv[1]) : 10;
    if (seconds <= 0) {
        seconds = 10;
    }

    g_captureStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_captureExitedEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_captureStopEvent || !g_captureExitedEvent) {
        wprintf(L"CreateEvent failed\n");
        return 1;
    }

    StartCapture();
    wprintf(L"capture started, watching for %d seconds\n", seconds);
    wprintf(L"  (the first second is silent: the analyser has no block yet)\n\n");

    for (int second = 0; second < seconds; ++second) {
        Sleep(1000);
        ScopeFrame frame = ReadScopeFrame();
        DWORD age = frame.tick ? (GetTickCount() - frame.tick) : 0;
        wprintf(L"%2d s  L bass %7.4f  mids %7.4f  highs %7.4f"
                L"   R bass %7.4f  mids %7.4f  highs %7.4f   (block %u ms ago)\n",
                second + 1, frame.rms[0][0], frame.rms[0][1], frame.rms[0][2],
                frame.rms[1][0], frame.rms[1][1], frame.rms[1][2], age);
    }

    wprintf(L"\ncapture alive : %s\n", g_captureAlive.load() ? L"yes" : L"no");
    wprintf(L"capture works : %s\n", g_captureWorks.load() ? L"yes" : L"no");

    // What the bars would do with these numbers.
    ScopeFrame last = ReadScopeFrame();
    wprintf(L"\nbar heights with this frame (design units, 2..21):\n  ");
    for (int i = 0; i < design::kMeterCount; ++i) {
        double reference[kScopeBands];
        for (int b = 0; b < kScopeBands; ++b) {
            reference[b] = ScopeReferenceFor(b);
        }
        float level = ScopeTargetFor(last.rms, i, 1.0, reference);
        float height = design::kMeterMinHeight +
                       (design::kMeterMaxHeight - design::kMeterMinHeight) * level;
        wprintf(L"%.0f ", height);
    }
    wprintf(L"\n");

    StopCapture();
    CloseHandle(g_captureStopEvent);
    CloseHandle(g_captureExitedEvent);
    return 0;
}

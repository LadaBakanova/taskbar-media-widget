// media-player / preview/band-check.cpp
// Plays a known signal into the default playback device and reads it back
// through the widget's own capture and band split, so the visualizer can be
// checked without a music player: the numbers below are exactly what the bars
// would be drawn from.
//
// The signal is pink-ish noise built from three bands with known levels -
// loud at 60 Hz, medium at 800 Hz, quiet at 6 kHz - so each of the widget's
// three bands has a level it should find, and they are far enough apart that a
// broken split shows up as the wrong band being the loudest.
//
// Nothing here is part of the mod: it only drives the mod's own code.
//
// Build like build-preview.ps1, then run:
//   band-check.exe

#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <cmath>
#include <cstdio>
#include <vector>

#include "../taskbar-media-widget.wh.cpp"

namespace {

const double kPi = 3.14159265358979;

// One oscillator per band, at the level the band should come back with.
struct Tone {
    double frequency;
    double amplitude;
    double phase = 0.0;

    double Next(double rate) {
        phase += 2.0 * kPi * frequency / rate;
        if (phase > 2.0 * kPi) {
            phase -= 2.0 * kPi;
        }
        return amplitude * std::sin(phase);
    }
};

}  // namespace

int wmain() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // ------------------------------------------------------------- playback
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioClient* renderClient = nullptr;
    IAudioRenderClient* render = nullptr;
    WAVEFORMATEX* mix = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                  CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                  (void**)&enumerator);
    if (SUCCEEDED(hr)) {
        hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    }
    if (SUCCEEDED(hr)) {
        hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                              (void**)&renderClient);
    }
    if (SUCCEEDED(hr)) {
        hr = renderClient->GetMixFormat(&mix);
    }
    if (SUCCEEDED(hr)) {
        hr = renderClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 1000000, 0, mix,
                                      nullptr);
    }
    if (SUCCEEDED(hr)) {
        hr = renderClient->GetService(__uuidof(IAudioRenderClient), (void**)&render);
    }
    if (FAILED(hr) || !render) {
        wprintf(L"the playback device could not be opened (0x%08X)\n", (unsigned)hr);
        return 1;
    }

    const int channels = mix->nChannels < 2 ? mix->nChannels : 2;
    const double rate = (double)mix->nSamplesPerSec;
    UINT32 bufferFrames = 0;
    renderClient->GetBufferSize(&bufferFrames);

    // One tone per band, at a known level, so the widget's own analyser can be
    // calibrated against the mix: the level that reaches the loopback is lower
    // than the level played, and that ratio is what decides how far the bars
    // travel on real music.
    const double kBassLevel = 0.50;    // 60 Hz
    const double kMidLevel = 0.25;     // 1 kHz
    const double kHighLevel = 0.05;    // 6 kHz
    std::vector<Tone> left = {{60.0, kBassLevel}, {1000.0, kMidLevel},
                              {6000.0, kHighLevel}};
    std::vector<Tone> right = left;

    // The root mean square each tone has in the mix, which is what it "should"
    // read as before the capture's own gain.
    const double sourceRms[kScopeBands] = {
        kBassLevel / std::sqrt(2.0), kMidLevel / std::sqrt(2.0),
        kHighLevel / std::sqrt(2.0)};

    // -------------------------------------------------------------- capture
    g_captureStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_captureExitedEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    StartCapture();
    Sleep(500);  // let the capture thread open its own client

    renderClient->Start();

    wprintf(L"playing 60 Hz at %.2f, 1 kHz at %.2f, 6 kHz at %.2f\n", kBassLevel,
            kMidLevel, kHighLevel);
    wprintf(L"the widget's bands should come back in that order\n\n");

    double seen[2][kScopeBands] = {};
    // The first part of the playback is a step from silence to a steady tone,
    // and a filter that has been sitting on silence rings on that step: a few
    // frames of nonsense at the start. They are skipped, and what is measured
    // is the steady state, which is what a track sounds like nearly all the
    // time.
    const DWORD settleMs = 1200;
    DWORD start = GetTickCount();
    while (GetTickCount() - start < 4000) {
        UINT32 padding = 0;
        if (FAILED(renderClient->GetCurrentPadding(&padding))) {
            break;
        }
        UINT32 free = bufferFrames - padding;
        if (free > 0) {
            BYTE* buffer = nullptr;
            if (SUCCEEDED(render->GetBuffer(free, &buffer))) {
                float* samples = (float*)buffer;
                for (UINT32 i = 0; i < free; ++i) {
                    for (int c = 0; c < channels; ++c) {
                        double value = c == 0
                                           ? left[0].Next(rate) + left[1].Next(rate) +
                                                 left[2].Next(rate)
                                           : right[0].Next(rate) + right[1].Next(rate) +
                                                 right[2].Next(rate);
                        samples[(size_t)i * channels + c] = (float)value;
                    }
                }
                render->ReleaseBuffer(free, 0);
            }
        }

        // The loudest block once the signal is steady is what the bars would
        // show on a track at this level.
        if (GetTickCount() - start > settleMs) {
            ScopeFrame frame = ReadScopeFrame();
            if (frame.tick && GetTickCount() - frame.tick < 320) {
                for (int c = 0; c < 2; ++c) {
                    for (int b = 0; b < kScopeBands; ++b) {
                        if (frame.rms[c][b] > seen[c][b]) {
                            seen[c][b] = frame.rms[c][b];
                        }
                    }
                }
            }
        }
        Sleep(20);
    }

    renderClient->Stop();

    const wchar_t* names[kScopeBands] = {L"bass (<200 Hz)", L"mids (200 Hz-2 kHz)",
                                         L"highs (>2 kHz)"};
    double attenuation[kScopeBands] = {};
    for (int c = 0; c < 2; ++c) {
        wprintf(L"%s channel:\n", c == 0 ? L"left " : L"right");
        for (int b = 0; b < kScopeBands; ++b) {
            double ratio = seen[c][b] > 0 ? sourceRms[b] / seen[c][b] : 0.0;
            if (c == 0) {
                attenuation[b] = ratio;
            }
            wprintf(L"   %-20s played %.4f -> read %.4f rms (.1f dB apart)\n",
                    names[b], sourceRms[b], seen[c][b],
                    seen[c][b] > 0
                        ? 20.0 * log10(sourceRms[b] / seen[c][b])
                        : 0.0);
        }
    }

    // What a bar would do, if the frame were exactly this.
    float rms[2][kScopeBands];
    for (int c = 0; c < 2; ++c) {
        for (int b = 0; b < kScopeBands; ++b) {
            rms[c][b] = (float)seen[c][b];
        }
    }
    wprintf(L"\nbars from that frame (design units, 2..21):\n  ");
    for (int i = 0; i < design::kMeterCount; ++i) {
        float level = ScopeTargetFor(rms, i, 1.0);
        wprintf(L"%.1f ", (double)(design::kMeterMinHeight +
                                   (design::kMeterMaxHeight -
                                    design::kMeterMinHeight) * level));
    }
    wprintf(L"\n  (a 60 Hz tone at 0.50 amplitude in the mix: a loud bass "
            L"line)\n");

    // What a typical track would do. -18 dBFS rms in the mix is the level the
    // bar scale is calibrated against, and the attenuation is the one measured
    // above for the band in question.
    const double typicalRms = 0.126;  // rms of a -18 dBFS sine
    wprintf(L"\ntypical track: -18 dBFS rms in the mix, at the measured "
            L"attenuation\n");
    for (int b = 0; b < kScopeBands; ++b) {
        float reading[2][kScopeBands] = {};
        reading[0][b] = (float)(typicalRms / attenuation[b]);
        reading[1][b] = reading[0][b];
        // Bar 0 is the highs, 4 the mids and 9 the bass, so the band under test
        // is reported through its own bar.
        int bar = b == 0 ? 9 : (b == 1 ? 4 : 0);
        float level = ScopeTargetFor(reading, bar, 1.0);
        wprintf(L"   %-20s reads %.4f -> bar %.1f of 21\n", names[b],
                (double)reading[0][b],
                (double)(design::kMeterMinHeight +
                         (design::kMeterMaxHeight - design::kMeterMinHeight) *
                             level));
    }

    bool bassLoudest = seen[0][0] > seen[0][1] && seen[0][1] > seen[0][2];
    wprintf(L"\nband split     : %s\n",
            bassLoudest ? L"bass > mids > highs, as played"
                        : L"WRONG ORDER, the split is not separating the bands");
    wprintf(L"capture works  : %s\n", g_captureWorks.load() ? L"yes" : L"no");
    wprintf(L"capture gain   : bass x%.1f  mids x%.1f  highs x%.1f\n",
            attenuation[0], attenuation[1], attenuation[2]);
    wprintf(L"               (a tone's own rms in the mix divided by what the "
            L"loopback reads)\n");

    StopCapture();
    renderClient->Release();
    render->Release();
    if (mix) {
        CoTaskMemFree(mix);
    }
    device->Release();
    enumerator->Release();
    CoUninitialize();
    return bassLoudest ? 0 : 1;
}

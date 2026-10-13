// media-player / probe-level.cpp
// Standalone check that a WASAPI loopback capture of the default render device
// really delivers the samples that are playing, and that the level maths the
// widget will use (block RMS -> dB -> a 0..1 bar height) produces sane numbers.
//
// It plays a known stereo test signal itself (a 440 Hz tone on the left channel
// and a 1 kHz tone on the right, 1 s long with 0.25 s of silence at each end)
// through the default render device, captures the same device in loopback mode
// and reports what came back. Nothing here is part of the mod: the mod only ever
// captures.
//
// Build (same compiler as verify-build.ps1):
//   clang++ -x c++ -std=c++23 -target x86_64-w64-mingw32 -O1 probe-level.cpp \
//           -o probe-level.exe -lole32 -lmmdevapi -luuid
// Run:
//   probe-level.exe

#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <cmath>
#include <cstdio>
#include <vector>

// ksmedia.h declares these as extern const GUIDs that only the static ksuser
// library defines, which the mod does not link. The values are the documented
// ones; the widget only ever compares against them.
static const GUID kSubtypeIeeeFloat = {
    0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

namespace {

// The same numbers the widget uses: a window is 1/25 s of samples, silence is
// -60 dB, full scale is +6 dB, and the bar height is the normalised level.
constexpr double kSilenceDb = -60.0;
constexpr double kFullDb = 6.0;

double RmsToLevel(double rms) {
    if (rms <= 1e-7) {
        return 0.0;
    }
    double db = 20.0 * log10(rms);
    double t = (db - kSilenceDb) / (kFullDb - kSilenceDb);
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    return t;
}

// One biquad, used as the widget's band splitter.
struct Biquad {
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;

    double Process(double x) {
        double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// RBJ low pass / high pass at the given cutoff.
Biquad LowPass(double freq, double sampleRate) {
    double w0 = 2.0 * 3.14159265358979 * freq / sampleRate;
    double cosw = cos(w0), sinw = sin(w0);
    double q = 0.7071;
    double alpha = sinw / (2.0 * q);
    double a0 = 1.0 + alpha;
    Biquad f;
    f.b0 = (1.0 - cosw) / 2.0 / a0;
    f.b1 = (1.0 - cosw) / a0;
    f.b2 = f.b0;
    f.a1 = -2.0 * cosw / a0;
    f.a2 = (1.0 - alpha) / a0;
    return f;
}

Biquad HighPass(double freq, double sampleRate) {
    double w0 = 2.0 * 3.14159265358979 * freq / sampleRate;
    double cosw = cos(w0), sinw = sin(w0);
    double q = 0.7071;
    double alpha = sinw / (2.0 * q);
    double a0 = 1.0 + alpha;
    Biquad f;
    f.b0 = (1.0 + cosw) / 2.0 / a0;
    f.b1 = -(1.0 + cosw) / a0;
    f.b2 = f.b0;
    f.a1 = -2.0 * cosw / a0;
    f.a2 = (1.0 - alpha) / a0;
    return f;
}

const char* HrText(HRESULT hr) {
    static char buf[32];
    sprintf_s(buf, "0x%08lX", (unsigned long)hr);
    return buf;
}

}  // namespace

int wmain() {
    // MTA, exactly as the mod's capture thread will use it.
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    printf("CoInitializeEx (MTA)          : %s\n", HrText(hr));

    IMMDeviceEnumerator* enumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
    printf("CoCreateInstance enumerator   : %s\n", HrText(hr));
    if (FAILED(hr) || !enumerator) {
        return 1;
    }

    IMMDevice* device = nullptr;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    printf("GetDefaultAudioEndpoint       : %s\n", HrText(hr));
    if (FAILED(hr) || !device) {
        return 1;
    }

    IAudioClient* client = nullptr;
    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&client);
    printf("Activate IAudioClient         : %s\n", HrText(hr));
    if (FAILED(hr) || !client) {
        return 1;
    }

    WAVEFORMATEX* mix = nullptr;
    hr = client->GetMixFormat(&mix);
    printf("GetMixFormat                  : %s", HrText(hr));
    if (FAILED(hr) || !mix) {
        return 1;
    }
    printf("  %u Hz, %u ch, %u bits, tag %u, block %u\n", mix->nSamplesPerSec,
           mix->nChannels, mix->wBitsPerSample, mix->wFormatTag,
           mix->nBlockAlign);

    bool isFloat = (mix->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
                   (mix->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                    IsEqualGUID(
                        reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mix)->SubFormat,
                        kSubtypeIeeeFloat));
    printf("sample format                 : %s\n", isFloat ? "float32" : "PCM");

    hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
                            1000000, 0, mix, nullptr);
    printf("Initialize (loopback)         : %s\n", HrText(hr));
    if (FAILED(hr)) {
        return 1;
    }

    IAudioCaptureClient* capture = nullptr;
    hr = client->GetService(__uuidof(IAudioCaptureClient), (void**)&capture);
    printf("GetService IAudioCaptureClient: %s\n", HrText(hr));
    if (FAILED(hr) || !capture) {
        return 1;
    }

    UINT32 bufferFrames = 0;
    client->GetBufferSize(&bufferFrames);
    printf("buffer frames                 : %u (%.1f ms)\n", bufferFrames,
           1000.0 * bufferFrames / mix->nSamplesPerSec);

    // ---------------------------------------------------------------- play
    IAudioClient* renderClient = nullptr;
    IAudioRenderClient* render = nullptr;
    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                          (void**)&renderClient);
    if (SUCCEEDED(hr)) {
        hr = renderClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 1000000, 0, mix,
                                      nullptr);
    }
    if (SUCCEEDED(hr)) {
        hr = renderClient->GetService(__uuidof(IAudioRenderClient), (void**)&render);
    }
    printf("render client ready           : %s\n", SUCCEEDED(hr) ? "yes" : "no");

    const int channels = mix->nChannels < 2 ? mix->nChannels : 2;
    const double rate = (double)mix->nSamplesPerSec;
    std::vector<float> tone;
    if (SUCCEEDED(hr)) {
        // 1.5 s: 0.25 s silence, 1 s tone, 0.25 s silence. Left 440 Hz, right 1 kHz.
        const int total = (int)(rate * 1.5);
        tone.assign((size_t)total * channels, 0.0f);
        for (int i = 0; i < total; ++i) {
            double t = (double)i / rate;
            if (t < 0.25 || t > 1.25) {
                continue;
            }
            float left = (float)(0.5 * sin(2.0 * 3.14159265358979 * 440.0 * t));
            float right = (float)(0.5 * sin(2.0 * 3.14159265358979 * 1000.0 * t));
            tone[(size_t)i * channels + 0] = left;
            if (channels > 1) {
                tone[(size_t)i * channels + 1] = right;
            }
        }
        hr = renderClient->Start();
        printf("render Start                  : %s\n", HrText(hr));
    }

    hr = client->Start();
    printf("capture Start                 : %s\n", HrText(hr));

    // ------------------------------------------------------------ pump both
    int written = 0;
    int packets = 0;
    int framesSeen = 0;
    int nonZeroPackets = 0;
    double peakLevel[2] = {0.0, 0.0};
    double maxRms[2] = {0.0, 0.0};
    // Per-block reports, so the tone start/stop is visible in the output.
    const int blockSamples = (int)(rate / 25.0);  // the widget's 1/25 s window
    int blockFill = 0;
    double blockSum[2] = {0.0, 0.0};
    int blocksReported = 0;

    Biquad lp[2] = {LowPass(200.0, rate), LowPass(200.0, rate)};
    Biquad hp[2] = {HighPass(2000.0, rate), HighPass(2000.0, rate)};

    DWORD start = GetTickCount();
    while (GetTickCount() - start < 3000) {
        // Feed the render side.
        if (render && SUCCEEDED(hr) && written < (int)tone.size()) {
            UINT32 padding = 0;
            if (SUCCEEDED(renderClient->GetCurrentPadding(&padding))) {
                UINT32 free = bufferFrames - padding;
                int left = (int)tone.size() - written;
                UINT32 give = (UINT32)(left < (int)free ? left : (int)free);
                if (give > 0) {
                    BYTE* dst = nullptr;
                    if (SUCCEEDED(render->GetBuffer(give, &dst))) {
                        memcpy(dst, tone.data() + written, give * mix->nBlockAlign);
                        render->ReleaseBuffer(give, 0);
                        written += (int)give;
                    }
                }
            }
        }

        UINT32 packet = 0;
        if (SUCCEEDED(capture->GetNextPacketSize(&packet)) && packet == 0) {
            Sleep(4);
            continue;
        }
        while (packet > 0) {
            BYTE* data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;
            if (FAILED(capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) {
                break;
            }
            packets++;
            framesSeen += (int)frames;
            bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
            if (!silent && data) {
                nonZeroPackets++;
                for (UINT32 i = 0; i < frames; ++i) {
                    for (int c = 0; c < 2; ++c) {
                        double sample = 0.0;
                        if (c < channels) {
                            if (isFloat) {
                                sample = ((float*)data)[(size_t)i * channels + c];
                            } else if (mix->wBitsPerSample == 16) {
                                sample = ((short*)data)[(size_t)i * channels + c] /
                                         32768.0;
                            }
                        }
                        blockSum[c] += sample * sample;
                        if (blockFill == 0) {
                            // reset the band filters' state per block
                            (void)lp[c].Process(0.0);
                        }
                        (void)lp[c].Process(sample);
                        (void)hp[c].Process(sample);
                    }
                    blockFill++;
                    if (blockFill >= blockSamples) {
                        double rms[2];
                        for (int c = 0; c < 2; ++c) {
                            rms[c] = sqrt(blockSum[c] / (double)blockFill);
                            if (rms[c] > maxRms[c]) maxRms[c] = rms[c];
                            double lvl = RmsToLevel(rms[c]);
                            if (lvl > peakLevel[c]) peakLevel[c] = lvl;
                            blockSum[c] = 0.0;
                        }
                        if (blocksReported < 40) {
                            printf("  block %2d  L rms %.4f -> %.2f   R rms %.4f -> %.2f\n",
                                   blocksReported, rms[0], RmsToLevel(rms[0]),
                                   rms[1], RmsToLevel(rms[1]));
                            blocksReported++;
                        }
                        blockFill = 0;
                    }
                }
            } else {
                blockFill += (int)frames;
            }
            capture->ReleaseBuffer(frames);
            if (FAILED(capture->GetNextPacketSize(&packet))) {
                break;
            }
        }
    }

    printf("\npackets %d, frames %d (%.2f s of audio), non-silent packets %d\n",
           packets, framesSeen, framesSeen / rate, nonZeroPackets);
    printf("loudest block level: L %.2f  R %.2f\n", peakLevel[0], peakLevel[1]);
    printf("loudest block rms  : L %.4f  R %.4f\n", maxRms[0], maxRms[1]);

    client->Stop();
    if (renderClient) {
        renderClient->Stop();
        renderClient->Release();
    }
    if (render) render->Release();
    capture->Release();
    client->Release();
    device->Release();
    enumerator->Release();
    CoUninitialize();
    return 0;
}

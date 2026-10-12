// media-player / preview/endpoint-check.cpp
// Lists the playback endpoints and reads the level of each one, so it is clear
// which device a player's audio is actually on.
//
// Written after the widget's visualizer read near-silence on a machine where
// music was playing: either the player is on an endpoint that is not the
// default one, or the default endpoint is not where the sound goes.
//
// Build like capture-check.cpp. Run it while music is playing.

#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

const GUID kSubtypeIeeeFloat = {
    0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

bool IsFloat(const WAVEFORMATEX* format) {
    if (!format) return false;
    if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) return true;
    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE && format->cbSize >= 22) {
        return IsEqualGUID(
            reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format)->SubFormat,
            kSubtypeIeeeFloat);
    }
    return false;
}

// Peak and rms of what this endpoint's loopback reports over the given time.
bool Measure(IMMDevice* device, int milliseconds, double& rms, double& peak,
             bool& silentFlag) {
    IAudioClient* client = nullptr;
    if (FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                (void**)&client))) {
        return false;
    }
    WAVEFORMATEX* mix = nullptr;
    if (FAILED(client->GetMixFormat(&mix)) || !IsFloat(mix)) {
        if (mix) CoTaskMemFree(mix);
        client->Release();
        return false;
    }
    HRESULT hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                    AUDCLNT_STREAMFLAGS_LOOPBACK, 1000000, 0, mix,
                                    nullptr);
    if (FAILED(hr)) {
        CoTaskMemFree(mix);
        client->Release();
        return false;
    }
    IAudioCaptureClient* capture = nullptr;
    if (FAILED(client->GetService(__uuidof(IAudioCaptureClient), (void**)&capture))) {
        CoTaskMemFree(mix);
        client->Release();
        return false;
    }
    client->Start();

    double sum = 0.0;
    long long count = 0;
    peak = 0.0;
    silentFlag = false;
    DWORD start = GetTickCount();
    while (GetTickCount() - start < (DWORD)milliseconds) {
        UINT32 packet = 0;
        if (FAILED(capture->GetNextPacketSize(&packet)) || packet == 0) {
            Sleep(5);
            continue;
        }
        while (packet > 0) {
            BYTE* data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;
            if (FAILED(capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) {
                break;
            }
            if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                silentFlag = true;
            } else if (data) {
                const float* samples = (const float*)data;
                for (UINT32 i = 0; i < frames; ++i) {
                    for (UINT32 c = 0; c < mix->nChannels; ++c) {
                        double v = samples[(size_t)i * mix->nChannels + c];
                        sum += v * v;
                        if (fabs(v) > peak) peak = fabs(v);
                        ++count;
                    }
                }
            }
            capture->ReleaseBuffer(frames);
            if (FAILED(capture->GetNextPacketSize(&packet))) break;
        }
    }

    rms = count > 0 ? sqrt(sum / (double)count) : 0.0;
    client->Stop();
    capture->Release();
    CoTaskMemFree(mix);
    client->Release();
    return true;
}

}  // namespace

int wmain() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IMMDeviceEnumerator* enumerator = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator),
                                (void**)&enumerator))) {
        wprintf(L"no device enumerator\n");
        return 1;
    }

    IMMDevice* defaultDevice = nullptr;
    enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice);
    LPWSTR defaultId = nullptr;
    if (defaultDevice) {
        defaultDevice->GetId(&defaultId);
    }

    IMMDeviceCollection* collection = nullptr;
    if (FAILED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE,
                                              &collection))) {
        wprintf(L"could not enumerate the playback endpoints\n");
        return 1;
    }

    UINT count = 0;
    collection->GetCount(&count);
    wprintf(L"%u active playback endpoint(s); reading each for 1.5 s\n\n", count);

    for (UINT i = 0; i < count; ++i) {
        IMMDevice* device = nullptr;
        if (FAILED(collection->Item(i, &device))) continue;

        LPWSTR id = nullptr;
        device->GetId(&id);
        IPropertyStore* properties = nullptr;
        std::wstring name = L"(no name)";
        if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &properties))) {
            PROPVARIANT value;
            PropVariantInit(&value);
            if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &value)) &&
                value.vt == VT_LPWSTR) {
                name = value.pwszVal;
            }
            PropVariantClear(&value);
            properties->Release();
        }

        bool isDefault = defaultId && id && wcscmp(defaultId, id) == 0;
        double rms = 0.0;
        double peak = 0.0;
        bool silentFlag = false;
        bool measured = Measure(device, 1500, rms, peak, silentFlag);

        wprintf(L"%s %s\n", isDefault ? L"[default]" : L"         ", name.c_str());
        if (measured) {
            wprintf(L"            rms %.5f (%.1f dBFS)  peak %.5f  %s\n", rms,
                    rms > 0 ? 20.0 * log10(rms) : -120.0, peak,
                    silentFlag ? L"(some packets flagged silent)" : L"");
        } else {
            wprintf(L"            no loopback (in exclusive use, or no float "
                    L"mix format)\n");
        }

        CoTaskMemFree(id);
        device->Release();
    }

    if (defaultId) CoTaskMemFree(defaultId);
    if (defaultDevice) defaultDevice->Release();
    collection->Release();
    enumerator->Release();
    CoUninitialize();
    return 0;
}

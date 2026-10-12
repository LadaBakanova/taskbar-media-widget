// media-player / probe-media.cpp
// Measures what the currently playing player actually publishes to the Windows
// media session API (GSMTC), and prints a verdict about whether a real progress
// bar is possible.
//
// This is a read-only diagnostic: it never sends a command to the player.
//
// Build:
//   "C:\Program Files\Windhawk\Compiler\bin\clang++.exe" -x c++ -std=c++23 ^
//     -target x86_64-w64-mingw32 -municode -O1 ^
//     -I"C:\Program Files\Windhawk\Compiler\include" ^
//     -I"C:\Program Files\Windhawk\Compiler\include\winrt" ^
//     probe-media.cpp -o probe-media.exe ^
//     -lwindowsapp -lruntimeobject -lole32 -loleaut32 -luser32 -lshlwapi
//
// Or simply run build-probe.ps1 in this folder.

#include <windows.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Storage.Streams.h>

#include <cstdio>
#include <set>
#include <string>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Media::Control;

// WinRT TimeSpan is in 100-nanosecond ticks.
static double ToSeconds(int64_t ticks) {
    return static_cast<double>(ticks) / 10000000.0;
}

static std::wstring FmtSeconds(int64_t ticks) {
    double seconds = ToSeconds(ticks);
    int total = static_cast<int>(seconds);
    wchar_t buf[64];
    swprintf_s(buf, L"%d:%02d (%d.%02d s)", total / 60, total % 60, total,
               static_cast<int>((seconds - total) * 100));
    return buf;
}

// DateTime is projected as a chrono time_point with 100-ns ticks since 1601.
static std::wstring FmtDateTime(DateTime const& dt) {
    int64_t ticks = dt.time_since_epoch().count();
    if (ticks == 0) {
        return L"(never)";
    }
    ULARGE_INTEGER u;
    u.QuadPart = static_cast<unsigned long long>(ticks);
    FILETIME ft;
    ft.dwLowDateTime = u.LowPart;
    ft.dwHighDateTime = u.HighPart;
    FILETIME local{};
    FileTimeToLocalFileTime(&ft, &local);
    SYSTEMTIME st{};
    FileTimeToSystemTime(&local, &st);
    wchar_t buf[64];
    swprintf_s(buf, L"%02d:%02d:%02d.%03d", st.wHour, st.wMinute, st.wSecond,
               st.wMilliseconds);
    return buf;
}

// Narrow UTF-8 copy of a wide string, valid until the next few calls. Used with
// printf so that Cyrillic text survives redirection to a file.
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

// Reports whether the artwork can actually be decoded, using the SAME calls the
// mod makes in DecodeThumbnail(). Availability is not the same as decodability:
// the mod only ever gets one attempt per track, so if this fails, the widget
// shows no cover for the whole track even though the player did publish one.
static void ReportThumbnail(
    Windows::Storage::Streams::IRandomAccessStreamReference const& reference) {
    if (!reference) {
        wprintf(L"Thumbnail  : none published by the player\n");
        return;
    }

    wprintf(L"Thumbnail  : available\n");

    using namespace Windows::Graphics::Imaging;

    constexpr int kDecode = 64;  // kArtDecodeSize in the mod
    try {
        auto stream = reference.OpenReadAsync().get();
        auto decoder = BitmapDecoder::CreateAsync(stream).get();
        wprintf(L"  source   : %u x %u  pixelFormat=%d alphaMode=%d\n",
                decoder.PixelWidth(), decoder.PixelHeight(),
                (int)decoder.BitmapPixelFormat(), (int)decoder.BitmapAlphaMode());

        BitmapTransform transform;
        transform.ScaledWidth(kDecode);
        transform.ScaledHeight(kDecode);
        transform.InterpolationMode(BitmapInterpolationMode::Linear);

        auto pixelData =
            decoder
                .GetPixelDataAsync(BitmapPixelFormat::Bgra8,
                                   BitmapAlphaMode::Premultiplied, transform,
                                   ExifOrientationMode::IgnoreExifOrientation,
                                   ColorManagementMode::DoNotColorManage)
                .get();
        auto buffer = pixelData.DetachPixelData();

        const size_t needed = (size_t)kDecode * kDecode * 4;
        wprintf(L"  decoded  : %u bytes (mod needs %u)\n",
                (unsigned)buffer.size(), (unsigned)needed);

        if (buffer.size() < needed) {
            wprintf(L"  RESULT   : the mod would REJECT this image\n");
            return;
        }

        // Enough to tell a real picture from a flat or corrupted one.
        unsigned minAlpha = 255, maxAlpha = 0;
        int distinct = 0;
        {
            std::set<unsigned> colours;
            for (size_t i = 0; i + 3 < needed; i += 4) {
                unsigned b = buffer[i], g = buffer[i + 1], r = buffer[i + 2],
                         a = buffer[i + 3];
                if (a < minAlpha) minAlpha = a;
                if (a > maxAlpha) maxAlpha = a;
                colours.insert((r << 16) | (g << 8) | b);
            }
            distinct = (int)colours.size();
        }
        wprintf(L"  content  : %d distinct colours, alpha %u..%u\n", distinct,
                minAlpha, maxAlpha);
        wprintf(L"  RESULT   : the mod would ACCEPT this image\n");
    } catch (hresult_error const& e) {
        wprintf(L"  RESULT   : DECODE FAILED 0x%08X: %ls\n",
                (unsigned)e.code().value, e.message().c_str());
    } catch (...) {
        wprintf(L"  RESULT   : DECODE FAILED (unknown exception)\n");
    }
}

int wmain(int argc, wchar_t** argv) {
    // Make Windows console and redirected output handle UTF-8, otherwise
    // Cyrillic track and artist names come out as garbage.
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    init_apartment(apartment_type::multi_threaded);

    // Watch mode: print every media session once a second, so it is visible
    // whether a session appears at all, and what state it is in.
    bool watch = false;
    int watchSeconds = 30;
    int watchIntervalMs = 250;
    for (int i = 1; i < argc; i++) {
        if (wcscmp(argv[i], L"--watch") == 0) {
            watch = true;
        } else if (wcscmp(argv[i], L"--secs") == 0 && i + 1 < argc) {
            watchSeconds = _wtoi(argv[++i]);
            if (watchSeconds < 1) watchSeconds = 1;
            if (watchSeconds > 300) watchSeconds = 300;
        } else if (wcscmp(argv[i], L"--interval") == 0 && i + 1 < argc) {
            watchIntervalMs = _wtoi(argv[++i]);
            if (watchIntervalMs < 50) watchIntervalMs = 50;
            if (watchIntervalMs > 5000) watchIntervalMs = 5000;
        }
    }

    if (watch) {
        printf("=== Watching the published timeline for %d s ===\n", watchSeconds);
        printf("Interval %d ms. Only changes are printed after the first samples.\n\n",
               watchIntervalMs);
        printf("     time   position      end    lastUpdate   changed?\n");

        double prevPos = -1;
        std::wstring prevUpdate;
        int changeCount = 0;
        const int samples = watchSeconds * 1000 / watchIntervalMs;

        for (int i = 0; i < samples; i++) {
            try {
                auto manager = GlobalSystemMediaTransportControlsSessionManager::
                                   RequestAsync()
                                       .get();
                auto session = manager.GetCurrentSession();
                if (!session) {
                    auto sessions = manager.GetSessions();
                    if (sessions.Size() > 0) {
                        session = sessions.GetAt(0);
                    }
                }
                if (session) {
                    auto info = session.GetPlaybackInfo();
                    auto tl = session.GetTimelineProperties();
                    double pos = ToSeconds(tl.Position().count());
                    std::wstring lastUpdate = FmtDateTime(tl.LastUpdatedTime());
                    double elapsed = i * watchIntervalMs / 1000.0;

                    bool changed =
                        (i == 0) || (lastUpdate != prevUpdate) ||
                        (prevPos >= 0 && pos - prevPos > 0.5) ||
                        (prevPos >= 0 && prevPos - pos > 0.5);
                    if (i > 0 && changed) {
                        changeCount++;
                    }

                    if (i < 6 || changed) {
                        printf("  %6.2fs  %9.2f s  %7.2f s   %s   %s\n", elapsed,
                               pos, ToSeconds(tl.EndTime().count()),
                               Narrow(lastUpdate.c_str()),
                               changed ? "<-- UPDATED" : "");
                        fflush(stdout);
                    }
                    prevPos = pos;
                    prevUpdate = lastUpdate;

                    if (i > 0 && i % 8 == 0) {
                        printf("      [t=%5.1fs status=%d pos=%8.2f]\n", elapsed,
                               (int)info.PlaybackStatus(), pos);
                        fflush(stdout);
                    }
                } else {
                    printf("  %6.2fs  no session\n",
                           i * watchIntervalMs / 1000.0);
                    fflush(stdout);
                }
            } catch (hresult_error const& e) {
                printf("  %6.2fs  ERROR 0x%08X\n", i * watchIntervalMs / 1000.0,
                       (unsigned)e.code().value);
                fflush(stdout);
            }
            Sleep(watchIntervalMs);
        }

        wprintf(L"\n================ WATCH VERDICT ================\n");
        wprintf(L"Timeline updates observed: %d\n", changeCount);
        if (changeCount == 0) {
            wprintf(L"Nothing was ever published. Either the audio is not playing at\n");
            wprintf(L"all, or the player publishes no timeline whatsoever.\n");
        } else if (changeCount < 10) {
            wprintf(L"The timeline updates ONLY on your actions, not continuously.\n");
            wprintf(L"=> The player does not publish a live position. A progress bar\n");
            wprintf(L"   must be estimated by counting time, and resynchronised each\n");
            wprintf(L"   time the player does publish an update.\n");
        } else {
            wprintf(L"The timeline updates repeatedly on its own.\n");
            wprintf(L"=> A real, exact progress bar is possible.\n");
        }
        wprintf(L"==============================================\n");
        return 0;
    }

    wprintf(L"=== Windows media session probe ===\n\n");

    try {
        auto manager =
            GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
        auto sessions = manager.GetSessions();

        wprintf(L"Sessions found: %u\n", (unsigned)sessions.Size());
        for (auto const& s : sessions) {
            auto i = s.GetPlaybackInfo();
            printf("  %-40s status=%d\n", Narrow(s.SourceAppUserModelId().c_str()),
                    (int)i.PlaybackStatus());
        }

        if (sessions.Size() == 0) {
            wprintf(L"\n[!] No player is publishing a media session.\n");
            wprintf(L"    Start playback in Yandex Music and run this again.\n");
            return 2;
        }

        // Prefer the session Windows considers active, then any playing one.
        GlobalSystemMediaTransportControlsSession session{nullptr};
        if (auto current = manager.GetCurrentSession()) {
            session = current;
        } else {
            for (auto const& s : sessions) {
                if (s.GetPlaybackInfo().PlaybackStatus() ==
                    GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing) {
                    session = s;
                    break;
                }
            }
            if (!session) {
                session = sessions.GetAt(0);
            }
        }

        printf("\n--- analysing: %s ---\n", Narrow(session.SourceAppUserModelId().c_str()));

        auto props = session.TryGetMediaPropertiesAsync().get();
        printf("Title      : %s\n", Narrow(props.Title().c_str()));
        printf("Artist     : %s\n", Narrow(props.Artist().c_str()));
        printf("Album      : %s\n", Narrow(props.AlbumTitle().c_str()));
        ReportThumbnail(props.Thumbnail());

        auto info = session.GetPlaybackInfo();
        auto tl = session.GetTimelineProperties();
        auto controls = info.Controls();

        wprintf(L"\nStatus     : %d  (4 = Playing)\n", (int)info.PlaybackStatus());
        wprintf(L"Position   : %s\n", Narrow(FmtSeconds(tl.Position().count()).c_str()));
        wprintf(L"Start      : %s\n", Narrow(FmtSeconds(tl.StartTime().count()).c_str()));
        wprintf(L"End        : %s\n", Narrow(FmtSeconds(tl.EndTime().count()).c_str()));
        wprintf(L"MinSeek    : %s\n", Narrow(FmtSeconds(tl.MinSeekTime().count()).c_str()));
        wprintf(L"MaxSeek    : %s\n", Narrow(FmtSeconds(tl.MaxSeekTime().count()).c_str()));
        wprintf(L"LastUpdate : %s\n", Narrow(FmtDateTime(tl.LastUpdatedTime()).c_str()));

        wprintf(L"\nControls   : playPause=%d next=%d prev=%d seek=%d\n",
                (int)controls.IsPlayPauseToggleEnabled(),
                (int)controls.IsNextEnabled(),
                (int)controls.IsPreviousEnabled(),
                (int)controls.IsPlaybackPositionEnabled());

        // Sample four times a second. That shows not only whether the position
        // advances, but how often the player republishes it, which decides how
        // accurate an interpolated progress bar can be.
        wprintf(L"\nSampling for 20 s, 4 times a second...\n");
        wprintf(L"  elapsed    position      end    lastUpdate\n");

        double firstPos = -1, lastPos = -1;
        bool endSeen = false;
        int updates = 0;
        std::wstring shownLastUpdate;

        const int kSamples = 80;  // 80 * 250 ms = 20 s
        for (int i = 0; i < kSamples; i++) {
            Sleep(250);
            auto t = session.GetTimelineProperties();
            double pos = ToSeconds(t.Position().count());
            double end = ToSeconds(t.EndTime().count());
            std::wstring lastUpdate = FmtDateTime(t.LastUpdatedTime());

            if (i == 0) {
                firstPos = pos;
                shownLastUpdate = lastUpdate;
            }
            if (end > 0) {
                endSeen = true;
            }
            if (i > 0 && lastUpdate != shownLastUpdate) {
                updates++;
            }

            bool changed = (lastUpdate != shownLastUpdate);
            if (i < 8 || changed) {
                printf("  %6.2fs  %9.2f s  %7.2f s   %s\n", i * 0.25, pos, end,
                       Narrow(lastUpdate.c_str()));
                fflush(stdout);
                shownLastUpdate = lastUpdate;
            }
            lastPos = pos;
        }

        // ---------------- verdict ----------------
        double advanced = lastPos - firstPos;
        auto status = info.PlaybackStatus();
        bool playing =
            status == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;

        wprintf(L"\n================ VERDICT ================\n");
        wprintf(L"Position advanced by %.2f s over the sample window.\n", advanced);
        wprintf(L"Timeline republished %d time(s) in 20 s.\n", updates);
        wprintf(L"Duration published : %ls\n", endSeen ? L"yes" : L"NO (EndTime is 0)");
        wprintf(L"Player was playing : %ls\n", playing ? L"yes" : L"no");
        wprintf(L"\n");

        if (!playing) {
            wprintf(L"[?] Nothing was playing during the sample, so this proves\n");
            wprintf(L"    nothing. Start a track and run the probe again.\n");
        } else if (advanced > 15.0) {
            wprintf(L"[OK] The player publishes a live playback position.\n");
            wprintf(L"     A real progress bar with exact elapsed/total time works.\n");
        } else if (advanced > 1.0) {
            wprintf(L"[PARTIAL] The position advances, but the timeline is updated\n");
            wprintf(L"     rarely. The progress bar must interpolate between updates\n");
            wprintf(L"     and resynchronise on each one.\n");
        } else if (endSeen) {
            wprintf(L"[STALLED] The track is marked as playing, yet the position did\n");
            wprintf(L"     not move at all in 20 s. That normally means the audio never\n");
            wprintf(L"     actually started: make sure you can HEAR the track, then run\n");
            wprintf(L"     the probe again.\n");
            wprintf(L"     If sound IS audible and this still appears, the player does not\n");
            wprintf(L"     publish its position, and the bar would have to be estimated by\n");
            wprintf(L"     counting time while audio plays.\n");
        } else {
            wprintf(L"[NO] The player publishes neither a position nor a duration.\n");
        }
        wprintf(L"=========================================\n");
    } catch (hresult_error const& e) {
        wprintf(L"\nERROR 0x%08X: %s\n", (unsigned)e.code().value,
                e.message().c_str());
        return 1;
    }

    return 0;
}

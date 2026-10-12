// ==WindhawkMod==
// @id              taskbar-media-widget
// @name            Taskbar media widget
// @description     Shows what is playing (Yandex Music and any other player) on the taskbar, between the app buttons and the system tray, with a live stereo visualizer, previous / play-pause / next buttons and a live progress bar
// @version         2.8.9
// @author          you
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -lole32 -loleaut32 -lruntimeobject -lcomctl32 -lgdi32 -luser32 -lshell32 -lmsimg32 -lgdiplus -ldwmapi -lmmdevapi
// @license         MIT
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Taskbar media widget

Draws a media widget on the Windows 11 taskbar, in the free space between the
application buttons and the system tray:

* the cover, the track title and the artist on the left
* a progress bar with the elapsed and the total time
* a stereo visualizer under the progress bar, driven by what is playing
* previous / play-pause / next buttons on the right
* click the progress bar to seek

The widget is a pixel copy of the Pencil document `design.pen` (frames `qBuu4`
and `HLQsH`): the panel, the cover slot, both text lines, the progress bar, the
level-meter bars and the three buttons are drawn from the design's own
coordinates. The widget is 47 px narrower than the document: the whole
right-hand group - the progress bar, the meter, the time and the buttons - was
pulled 35 px towards the title and the artist, the buttons 10 px towards the
time, and the time itself ends 2 px earlier than the design's own. The time
sits on the bar's right edge, so the two move together, and since 2.8.8 that
edge is placed by the room the time needs: the widest it gets,
"00:00:00 / 00:00:00", plus 6 px of clean panel after the visualizer.
The text block is 5 px shorter than the design's own as well - the bar does not
move, so that 5 px goes into the gap between the text and the bar - which is why
a long title is clipped a little earlier than the design's own text box would
clip it. The two Inter weights the design uses are installed next to the library
by the install script; without them the widget falls back to a system font.

Data comes from the Windows media session API
(`Windows.Media.Control`), the same subsystem behind the volume/media popup, so
any player that registers there works: the Yandex Music desktop app, Spotify,
browsers, VLC. The widget follows whichever player is playing.

## The visualizer

The bars under the progress bar follow the music. The widget opens a WASAPI
loopback capture of the default playback device - the same stream the speakers
get, whichever player produces it - and measures the energy of each channel per
frequency band, twenty-five times a second. Bars rise with the signal and fall
back with a short release, so the meter moves with the track instead of sitting
still.

The bars are shorter than the design gives them, as a deliberate change to the
design: a strip 26.5 units long instead of 69.5, and 18 bars instead of 24, which
is what leaves the time block room to breathe next to it.

If the playback device cannot be captured (no device, or a format the widget
cannot read), the bars fall back to a synthetic signal whenever the player says
it is playing, so the widget never looks frozen. `visualizerGain` scales the
captured signal, and `visualizerFps` sets the animation rate.

## How it is placed, and why it is safe

The taskbar in Windows 11 is a XAML island. Version 1.0.0 of this mod subclassed
the taskbar window and made the widget a child of it; that froze the whole
taskbar, because the island does not tolerate an outside child window or a
patched window procedure.

This version never touches the taskbar's own window. It only ever *reads* the
taskbar rectangle and the tray position, and draws its own independent topmost
window on top of the taskbar surface. Nothing is injected into the taskbar's
message flow, so a failure here can only make the widget itself disappear, never
the taskbar.

## Settings

* `widgetWidth` / `widgetHeight` - widget size in pixels.
* `taskbarMonitor` - the screen the widget sits on: 1 is the primary display,
  2 to 4 are the additional displays, counted left to right.
* `rightMargin` - gap between the widget and the system tray.
* `fallbackRightOffset` - distance from the right edge of the taskbar, used only
  when the tray cannot be located on any taskbar. A taskbar whose tray has no
  window of its own (the additional displays draw it inside the XAML island)
  gets the tray width measured on another taskbar instead.
* `pollIntervalMs` - how often the player state is refreshed.
* `backgroundBlur` - blur the desktop behind the widget (frosted glass). On by
  default; turn it off to get the plain translucent panel back.
* `showProgressBar` - draw the progress bar.
* `seekOnClick` - allow seeking by clicking the progress bar.
* `forceTopmostEveryTick` - re-assert the topmost position on every refresh.
  Leave this on if the widget gets hidden behind the taskbar; turn it off to
  save a little CPU.

## Controls answer at once

Since 2.6.0 the buttons and the progress bar are not tied to the refresh
interval. A click only queues the command and wakes a separate command thread,
which hands it to the player immediately, so the button or the playhead answers
as soon as the click is handled instead of up to one `pollIntervalMs` later. The
widget also draws the click's own effect right away - the icon flips, the seek
moves the time and the playhead - while the player's confirmation is on its way.
The mod logs how long the command took to reach the player
(`command N reached the player after M ms`) when Windhawk's logging is enabled.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- widgetWidth: "419"
  $name: Widget width
  $name:ru-RU: Ширина виджета
  $description: Width of the widget in pixels. 419 is what the layout is drawn for - the design is 466 wide, with the right-hand group pulled 35 px closer to the title and the artist, the buttons 10 px closer to the time, and the progress bar as long as the widest time ("00:00:00 / 00:00:00") needs next to the visualizer. Narrower values are raised to it.
  $description:ru-RU: Ширина виджета в пикселях. 419 — ширина, под которую нарисована раскладка — макет 466, но правая группа придвинута на 35 px к названию и исполнителю, кнопки придвинуты на 10 px ко времени, а длина прогресс-бара взята под самое длинное время («00:00:00 / 00:00:00») рядом с визуализатором. Меньшие значения поднимаются до неё.
- widgetHeight: "42"
  $name: Widget height
  $name:ru-RU: Высота виджета
  $description: Height of the widget in pixels. 42 is the height of the design; the taskbar is 48 px tall, so a small margin is left.
  $description:ru-RU: Высота виджета в пикселях. 42 — высота макета; панель задач по умолчанию 48 px, поэтому остаётся небольшой отступ.
- taskbarMonitor: "screen1"
  $name: Screen
  $name:ru-RU: Экран
  $description: >-
    Which screen shows the widget. Screen 1 is the primary display, 2-4 are the
    additional displays, ordered left to right and then top to bottom. A screen
    that does not exist falls back to the primary display, so the widget is
    never left without a home.
  $description:ru-RU: >-
    На каком экране показывать виджет. Экран 1 — основной, 2–4 — дополнительные,
    слева направо и затем сверху вниз. Если такого экрана нет, виджет вернётся на
    основной, поэтому без виджета он не останется.
  $options:
  - screen1: Screen 1 (primary)
  - screen2: Screen 2
  - screen3: Screen 3
  - screen4: Screen 4
  $options:ru-RU:
  - screen1: Экран 1 (основной)
  - screen2: Экран 2
  - screen3: Экран 3
  - screen4: Экран 4
- rightMargin: "-2"
  $name: Gap to the system tray
  $name:ru-RU: Отступ до системного трея
  $description: Space in pixels between the right edge of the widget and the system tray's window. Themes pad the tray inside that window (FrostyGlass by 12 px), so -2 leaves 10 px to the visible tray; the value may be negative.
  $description:ru-RU: Расстояние в пикселях между правым краем виджета и окном системного трея. Темы оставляют внутри этого окна поле (FrostyGlass — 12 px), поэтому −2 даёт 10 px до видимого трея; значение может быть отрицательным.
- fallbackRightOffset: "220"
  $name: Fallback offset from the right edge
  $name:ru-RU: Отступ от правого края, если трей не найден
  $description: Last resort only, used when the system tray cannot be found on any taskbar. Distance in pixels from the right edge of the taskbar.
  $description:ru-RU: "Крайний случай: используется, только если системный трей не удалось найти нигде, в том числе на другой панели задач. Расстояние в пикселях от правого края панели задач."
- pollIntervalMs: "400"
  $name: Refresh interval (ms)
  $name:ru-RU: Период обновления (мс)
  $description: How often the player state is read and the widget is repositioned. Lower values make the widget react faster but use more CPU.
  $description:ru-RU: Как часто читается состояние плеера и переставляется виджет. Меньшие значения — быстрее реакция, но больше нагрузка.
- opacity: "255"
  $name: Opacity (0-255)
  $name:ru-RU: Прозрачность (0–255)
  $description: Transparency of the widget panel. 255 is the design's own opaque #1a1a1a; lower values let the taskbar show through. Minimum 40.
  $description:ru-RU: Прозрачность панели виджета. 255 — непрозрачный #1a1a1a, как в макете; меньшие значения показывают панель задач сквозь виджет. Минимум 40.
- followTaskbarTheme: true
  $name: Match the Windows 11 Taskbar Styler theme
  $name:ru-RU: Подстраиваться под тему Windows 11 Taskbar Styler
  $description: >-
    Take the widget's surface - the corner radius, the background brush and the
    border - from the theme selected in the Windows 11 Taskbar Styler mod, so the
    widget looks like part of the styled taskbar. With this off, the widget keeps
    the panel of the design. The opacity setting below applies to that panel.
  $description:ru-RU: >-
    Брать поверхность виджета — скругление, фон и рамку — из темы, выбранной в
    моде Windows 11 Taskbar Styler, чтобы виджет выглядел частью оформленной
    панели задач. Если выключить, виджет остаётся с панелью из макета, и на неё
    действует настройка прозрачности ниже.
- backgroundBlur: true
  $name: Blur the desktop behind the widget
  $name:ru-RU: Размывать рабочий стол за виджетом
  $description: >-
    Give the widget the same frosted glass as the taskbar's own pills: the
    wallpaper, the windows and the taskbar under it are blurred instead of
    showing through sharply, and the tint comes from the selected Taskbar Styler
    theme. The corners are rounded by the system, at its own radius of 8 pixels.
    With this off the widget keeps the plain translucent panel it had before.
  $description:ru-RU: >-
    Дать виджету такое же матовое стекло, как у пилюль панели задач: обои, окна и
    панель под ним размываются, а тон берётся из выбранной темы Taskbar Styler.
    Углы скругляет система, своим радиусом 8 пикселей. Если выключить, останется
    прежняя полупрозрачная панель.
- showProgressBar: true
  $name: Show the progress bar
  $name:ru-RU: Показывать прогресс-бар
  $description: Draw the progress bar with the elapsed and the total time.
  $description:ru-RU: Рисовать прогресс-бар с прошедшим и общим временем.
- visualizer: true
  $name: Live visualizer
  $name:ru-RU: Визуализатор музыки
  $description: >-
    Move the bars under the progress bar with what is playing. The widget
    captures the default playback device in loopback mode - the stream the
    speakers get - and measures the energy of the left and the right channel in
    three frequency bands, twenty-five times a second. With this off the bars
    stay at the heights the design gives them.
  $description:ru-RU: >-
    Двигать штрихи под прогресс-баром под музыку. Виджет захватывает устройство
    воспроизведения по умолчанию в режиме loopback — тот же поток, что идёт в
    колонки, — и 25 раз в секунду измеряет энергию левого и правого канала в трёх
    полосах частот. Если выключить, штрихи останутся на высотах из макета.
- visualizerGain: "100"
  $name: Visualizer sensitivity, %
  $name:ru-RU: Чувствительность визуализатора, %
  $description: >-
    Scales the captured signal before the bars are drawn. 100 is calibrated on a
    full scale tone; raise it if the bars stay low, lower it if they sit at the
    top all the time.
  $description:ru-RU: >-
    Множитель сигнала перед отрисовкой штрихов. 100 откалибровано по сигналу
    полной шкалы; увеличивайте, если штрихи низкие, уменьшайте, если они всё
    время упираются в верх.
- visualizerFps: "33"
  $name: Visualizer frame rate
  $name:ru-RU: Частота кадров визуализатора
  $description: >-
    How often the bars are redrawn while the visualizer is moving, in frames per
    second. Higher is smoother and costs a little more CPU; the widget drops back
    to two redraws a second on its own when nothing moves.
  $description:ru-RU: >-
    Как часто перерисовываются штрихи, пока визуализатор движется, в кадрах в
    секунду. Больше — плавнее и чуть дороже по процессору; когда движения нет,
    виджет сам возвращается к двум перерисовкам в секунду.
- seekOnClick: true
  $name: Seek by clicking the progress bar
  $name:ru-RU: Перемотка по клику на прогресс-баре
  $description: Clicking a position on the progress bar jumps to that point in the track.
  $description:ru-RU: Клик по позиции прогресс-бара перематывает трек на это место.
- forceTopmostEveryTick: true
  $name: Keep the widget on top
  $name:ru-RU: Держать виджет поверх панели
  $description: Re-assert the topmost position on every refresh. Turn off only if the widget is reliably visible without it.
  $description:ru-RU: Подтверждать верхний слой при каждом обновлении. Отключайте, только если виджет и так стабильно виден.
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <audioclient.h>
#include <mmdeviceapi.h>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>

#include <windhawk_api.h>
#include <gdiplus.h>

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Media::Control;

namespace {

constexpr PCWSTR kWidgetClass = L"WindhawkTaskbarMediaWidget";
constexpr UINT_PTR kPollTimerId = 0x4D57;   // "MW"
// The visualizer's own timer, which redraws only the strip of bars while they
// move. It is re-armed with a different interval as the widget starts and stops
// moving, so nothing is redrawn thirty times a second while the music is off.
constexpr UINT_PTR kVisualTimerId = 0x4D59;
constexpr UINT kVisualIdleIntervalMs = 500;
constexpr int kVisualMinIntervalMs = 1000 / 120;
// How long the strip keeps its fast repaint after the signal stops: the bars
// have to be able to fall all the way down at the same rate they rose.
constexpr DWORD kVisualFallMs = 900;
// Sent by the polling thread to the widget window to request a repaint.
constexpr UINT kRefreshMessage = WM_APP + 1;
constexpr UINT kCloseMessage = WM_APP + 2;
// Sent by the command thread right after it hands a command to the player, so
// the button or the progress bar answers at once instead of at the next poll.
constexpr UINT kForceRefreshMessage = WM_APP + 3;

// One taskbar per display. Eight additional displays is the practical limit
// Windows itself works with, and it keeps the scan free of allocation.
constexpr int kMaxSecondaryTaskbars = 8;
constexpr int kMaxTaskbars = 1 + kMaxSecondaryTaskbars;

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

struct Settings {
    // 419, not the design's own 466: the layout's right-hand group is 35 units
    // closer to the text (design::kCompactShift), the time with the bar's right
    // end is another 2 units closer to the visualizer (kTimeShift) and the
    // buttons are 10 units closer to the time (kButtonShift).
    int widgetWidth = 419;
    int widgetHeight = 42;
    int taskbarMonitor = 1;
    int rightMargin = -2;
    int fallbackRightOffset = 220;
    int pollIntervalMs = 400;
    int opacity = 255;
    bool followTaskbarTheme = true;
    bool backgroundBlur = true;
    bool showProgressBar = true;
    bool seekOnClick = true;
    bool forceTopmostEveryTick = true;
    bool visualizer = true;
    // Percent. 100 is calibrated so that a full scale sine reads as the top.
    int visualizerGain = 100;
    int visualizerFps = 33;
};

Settings g_settings;
std::mutex g_settingsMutex;

Settings GetSettings() {
    std::lock_guard<std::mutex> lock(g_settingsMutex);
    return g_settings;
}

// Where Windhawk keeps a mod's settings. The path comes from the engine's own
// engine.ini, which this installation runs with:
//   [Storage]
//   RegistryKey=HKLM\SOFTWARE\Windhawk\Engine
constexpr PCWSTR kStoredSettingsKey =
    L"SOFTWARE\\Windhawk\\Engine\\Mods\\taskbar-media-widget\\Settings";

// Every setting reaches the mod as a string: numbers as digits ("390"), the
// screen choice as a word ("screen2"). A value that starts with a number is read
// whole; otherwise the first digit inside the word is the number. That also
// makes a plain "2" work, which is what an older install or a hand-edited
// registry entry holds.
int ParseSettingNumber(PCWSTR value, int fallback) {
    if (!value || !*value) {
        return fallback;
    }
    if ((*value >= L'0' && *value <= L'9') || *value == L'-') {
        return static_cast<int>(wcstol(value, nullptr, 10));
    }
    for (PCWSTR ch = value; *ch; ++ch) {
        if (*ch >= L'0' && *ch <= L'9') {
            return *ch - L'0';
        }
    }
    return fallback;
}

// Reads one stored setting. Windhawk writes numbers and booleans as DWORDs and a
// choice from a dropdown as a string.
int ReadStoredSetting(PCWSTR name, int fallback) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, kStoredSettingsKey, 0,
                      KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS) {
        return fallback;
    }

    BYTE data[64] = {};
    DWORD size = sizeof(data);
    DWORD type = 0;
    int result = fallback;
    if (RegQueryValueExW(key, name, nullptr, &type, data, &size) == ERROR_SUCCESS) {
        if (type == REG_DWORD && size >= sizeof(DWORD)) {
            result = static_cast<int>(*reinterpret_cast<const DWORD*>(data));
        } else if ((type == REG_SZ || type == REG_EXPAND_SZ) &&
                   size >= sizeof(WCHAR)) {
            result = ParseSettingNumber(reinterpret_cast<const WCHAR*>(data),
                                        fallback);
        }
    }
    RegCloseKey(key);
    return result;
}

void LoadSettings() {
    Settings s;

    // Windhawk usually hands the mod the settings it has stored, and the mod's
    // first version trusted it completely. When the engine has no snapshot for a
    // mod - which is the state this installation has always been in - it passes
    // nothing at all: every Wh_GetStringSetting comes back empty and every
    // Wh_GetIntSetting comes back 0, so the widget silently used its built-in
    // defaults and even taskbarMonitor = 2 in the registry did nothing.
    //
    // The stored values are therefore read directly, and the engine's value is
    // only the fallback for a name that is not stored. Both come from the same
    // settings page, so they agree whenever both exist.
    PCWSTR raw = Wh_GetStringSetting(L"widgetWidth");
    bool engineHasSettings = (raw != nullptr) && (*raw != L'\0');
    if (raw) {
        Wh_FreeStringSetting(raw);
    }
    Wh_Log(L"settings from the engine: %s",
           engineHasSettings ? L"yes" : L"no, reading the stored values");

    // The screen is a choice in the settings page, so Windhawk hands it over as
    // a string; the numbers arrive as strings as well. Both are parsed the same
    // way, and Wh_GetIntSetting stays as a second attempt.
    auto setting = [engineHasSettings](PCWSTR name, int fallback) -> int {
        int engineValue = fallback;
        if (engineHasSettings) {
            PCWSTR rawValue = Wh_GetStringSetting(name);
            if (rawValue && *rawValue) {
                engineValue = ParseSettingNumber(rawValue, fallback);
            } else {
                engineValue = Wh_GetIntSetting(name);
            }
            if (rawValue) {
                Wh_FreeStringSetting(rawValue);
            }
        }
        return ReadStoredSetting(name, engineValue);
    };

    s.widgetWidth = setting(L"widgetWidth", s.widgetWidth);
    s.widgetHeight = setting(L"widgetHeight", s.widgetHeight);
    s.taskbarMonitor = setting(L"taskbarMonitor", s.taskbarMonitor);
    s.rightMargin = setting(L"rightMargin", s.rightMargin);
    s.fallbackRightOffset = setting(L"fallbackRightOffset", s.fallbackRightOffset);
    s.pollIntervalMs = setting(L"pollIntervalMs", s.pollIntervalMs);
    s.opacity = setting(L"opacity", s.opacity);
    s.followTaskbarTheme =
        setting(L"followTaskbarTheme", s.followTaskbarTheme ? 1 : 0) != 0;
    s.backgroundBlur = setting(L"backgroundBlur", s.backgroundBlur ? 1 : 0) != 0;
    s.showProgressBar =
        setting(L"showProgressBar", s.showProgressBar ? 1 : 0) != 0;
    s.seekOnClick = setting(L"seekOnClick", s.seekOnClick ? 1 : 0) != 0;
    s.forceTopmostEveryTick =
        setting(L"forceTopmostEveryTick", s.forceTopmostEveryTick ? 1 : 0) != 0;
    s.visualizer = setting(L"visualizer", s.visualizer ? 1 : 0) != 0;
    s.visualizerGain = setting(L"visualizerGain", s.visualizerGain);
    s.visualizerFps = setting(L"visualizerFps", s.visualizerFps);

    // Also guard individual values that were never populated.
    if (s.widgetWidth <= 0) s.widgetWidth = 419;
    if (s.widgetHeight <= 0) s.widgetHeight = 42;
    if (s.taskbarMonitor <= 0) s.taskbarMonitor = 1;
    if (s.opacity <= 0) s.opacity = 255;
    if (s.pollIntervalMs <= 0) s.pollIntervalMs = 400;

    if (s.taskbarMonitor > kMaxTaskbars) s.taskbarMonitor = kMaxTaskbars;

    // The layout needs 419 units - the design's own 466 less the 35 the
    // right-hand group was pulled in by (design::kCompactShift), the 2 the
    // time and the bar's right end moved on by (design::kTimeShift) and the 10
    // the buttons were pulled in by (design::kButtonShift): a narrower
    // window would push the buttons on top of the progress bar. Wider is fine -
    // the buttons hang off the right edge - so only the minimum is enforced.
    if (s.widgetWidth < 419) s.widgetWidth = 419;
    if (s.widgetHeight < 20) s.widgetHeight = 20;
    if (s.pollIntervalMs < 100) s.pollIntervalMs = 100;

    if (s.visualizerGain < 10) s.visualizerGain = 10;
    if (s.visualizerGain > 800) s.visualizerGain = 800;
    if (s.visualizerFps < 10) s.visualizerFps = 10;
    if (s.visualizerFps > 120) s.visualizerFps = 120;

    std::lock_guard<std::mutex> lock(g_settingsMutex);
    g_settings = s;
}

// ---------------------------------------------------------------------------
// Shared state: written by the polling thread, read by the widget's thread
// ---------------------------------------------------------------------------

// How the progress bar is computed.
//
// Yandex Music does not publish a continuously advancing playback position: it
// publishes one only in response to an action (seek, pause, track change). That
// was measured directly, see probe-watch.txt. The position is therefore
// estimated as:
//
//     position = anchorPosition + (now - anchorTime)   while playing
//
// and every message from the player overwrites the anchor, so the bar becomes
// exact again after every seek, pause or track change.
struct MediaState {
    std::wstring title;
    std::wstring artist;
    bool playing = false;
    bool hasSession = false;
    bool canSeek = false;
    long long anchorPositionMs = 0;
    long long durationMs = 0;
    // GetTickCount64() when anchorPositionMs was captured.
    unsigned long long anchorTick = 0;
    // Album art (or the YouTube video still), decoded when the track changes and
    // retried until the player publishes it. Empty when it publishes none.
    std::vector<BYTE> artPixels;  // BGRA, artSize x artSize
    int artSize = 0;
    bool hasArt = false;
};

std::mutex g_stateMutex;
MediaState g_state;

MediaState GetState() {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return g_state;
}

// Position to draw right now, interpolated from the anchor. Runs on the drawing
// thread, which is also the polling thread, but the state is copied under the
// mutex first.
long long CurrentPositionMs(MediaState const& state, unsigned long long now) {
    long long position = state.anchorPositionMs;
    if (state.playing && state.anchorTick) {
        unsigned long long elapsed = now - state.anchorTick;
        position += (long long)elapsed;
        if (state.durationMs > 0 && position > state.durationMs) {
            position = state.durationMs;
        }
    }
    if (position < 0) {
        position = 0;
    }
    return position;
}

// ---------------------------------------------------------------------------
// Command queue: the widget thread posts, the command thread executes
//
// A click is queued and the command thread is woken through g_pollWakeEvent.
// Nothing here waits for the poll timer: a play/pause or a seek reaches the
// player as soon as the click is handled, and the poll timer only refreshes the
// state and keeps the widget in place.
// ---------------------------------------------------------------------------

enum class Command { PlayPause, Next, Prev, Seek };

struct CommandRequest {
    Command command = Command::PlayPause;
    long long seekMs = 0;
    // GetTickCount64() when the click was queued. Only used to measure how long
    // the command took to reach the player, which is worth knowing precisely
    // because the whole point of the command thread is that it is immediate.
    unsigned long long queuedTick = 0;
};

// Wakes the command thread the moment something is queued.
HANDLE g_pollWakeEvent = nullptr;

std::mutex g_commandMutex;
CommandRequest g_pendingCommand{};
bool g_hasPendingCommand = false;

void QueueCommand(Command cmd, long long seekMs) {
    {
        std::lock_guard<std::mutex> lock(g_commandMutex);
        g_pendingCommand = CommandRequest{cmd, seekMs, GetTickCount64()};
        g_hasPendingCommand = true;
    }
    if (g_pollWakeEvent) {
        SetEvent(g_pollWakeEvent);
    }
}

// ---------------------------------------------------------------------------
// The visualizer: what is really coming out of the speakers
//
// The bars under the progress bar used to be the design's static heights. They
// now follow the music, and to do that the widget has to see the audio, which
// the media session API does not expose in any form.
//
// What it uses instead is a WASAPI *loopback* capture of the default playback
// device: opening the render endpoint for capture delivers the very mix the
// speakers are fed, whichever player produced it. That is the same mechanism
// behind the "listen to this device" checkbox and behind every system-wide
// visualizer, and it needs no hook, no injection and no audio driver.
//
// The capture runs on its own thread, because a loopback client is a pull
// source: GetNextPacketSize has to be polled, and polling it from the widget's
// own timer would tie the audio to the paint rate. The thread publishes one
// small frame (six numbers and a tick) that the drawing thread reads without a
// lock: there is one writer, and a reader must never be able to block a
// real-time audio path.
//
// Each channel is split into three bands by two one-pole filters and the energy
// of every band is measured over a 1/25 s block, which is what makes the bars
// move with the music instead of all rising and falling together.
// ---------------------------------------------------------------------------

constexpr int kScopeBands = 3;   // [0] bass, [1] mids, [2] highs

struct ScopeFrame {
    // Root mean square of each band, as a fraction of full scale.
    float rms[2][kScopeBands] = {};
    // GetTickCount() of the block this frame came from; 0 means the capture
    // thread has not finished a block yet.
    DWORD tick = 0;
};

struct ScopeShared {
    std::atomic<unsigned> sequence{0};
    float rms[2][kScopeBands] = {};
    std::atomic<DWORD> tick{0};
};

ScopeShared g_scope;

// True once a loopback client has been running, whether or not the track is
// audible. This is what tells "the track is silent" - the bars fall and stay
// down - apart from "this machine cannot be captured", where the bars are
// animated instead so the widget never looks frozen.
std::atomic<bool> g_captureWorks{false};
std::atomic<bool> g_captureAlive{false};

std::atomic<HANDLE> g_captureThread{nullptr};
HANDLE g_captureStopEvent = nullptr;
HANDLE g_captureExitedEvent = nullptr;

// The last block the capture thread published. The sequence number makes a torn
// read impossible: a reader that sees it change reads again.
ScopeFrame ReadScopeFrame() {
    ScopeFrame frame;
    for (int attempt = 0; attempt < 4; ++attempt) {
        unsigned before = g_scope.sequence.load(std::memory_order_acquire);
        if (before & 1u) {
            continue;  // a write is in progress
        }
        float copy[2][kScopeBands];
        std::memcpy(copy, g_scope.rms, sizeof(copy));
        DWORD tick = g_scope.tick.load(std::memory_order_relaxed);
        unsigned after = g_scope.sequence.load(std::memory_order_acquire);
        if (before != after) {
            continue;
        }
        std::memcpy(frame.rms, copy, sizeof(copy));
        frame.tick = tick;
        return frame;
    }
    return frame;  // torn every time: silence for this frame
}

// A one-pole filter: cheap, stable, and enough to decide which bars move.
struct OnePole {
    double state = 0.0;
    double coefficient = 0.0;

    void Configure(double cutoff, double rate) {
        const double pi = 3.14159265358979;
        coefficient = 1.0 - std::exp(-2.0 * pi * cutoff / rate);
        if (coefficient < 0.0) coefficient = 0.0;
        if (coefficient > 1.0) coefficient = 1.0;
    }

    double Low(double x) {
        state += coefficient * (x - state);
        return state;
    }
};

// The band split, per channel. Bass is what a 200 Hz low pass keeps, highs is
// what a 2 kHz low pass leaves, and mids is the band between the two, taken
// from the difference of those two low passes.
struct ScopeAnalyser {
    OnePole bass[2];
    OnePole highs[2];
    double sumSquares[2][kScopeBands] = {};
    int blockFrames = 0;
    int blockFramesTarget = 1920;  // 1/25 s at 48 kHz, set from the mix rate
    double peakRms = 0.0;

    void Configure(double rate) {
        for (int c = 0; c < 2; ++c) {
            bass[c].Configure(200.0, rate);
            highs[c].Configure(2000.0, rate);
        }
        blockFramesTarget = (int)(rate / 25.0);
        if (blockFramesTarget < 1) {
            blockFramesTarget = 1;
        }
    }

    // One packet, straight from the loopback client. `silent` is the flag the
    // client sets for a packet it knows holds nothing, which is the normal case
    // between tracks.
    void Process(const float* samples, UINT32 frames, int channelCount,
                 bool silent) {
        for (UINT32 i = 0; i < frames; ++i) {
            for (int c = 0; c < 2; ++c) {
                double sample = 0.0;
                if (!silent && c < channelCount) {
                    sample = samples[(size_t)i * channelCount + c];
                }
                double below200 = bass[c].Low(sample);
                double below2000 = highs[c].Low(sample);
                double mid = below2000 - below200;
                double high = sample - below2000;

                sumSquares[c][0] += below200 * below200;
                sumSquares[c][1] += mid * mid;
                sumSquares[c][2] += high * high;
            }
            blockFrames++;
            if (blockFrames >= blockFramesTarget) {
                Publish();
            }
        }
    }

    // Turns the accumulated energy into the six numbers the drawing thread
    // waits for, and starts a new block.
    void Publish() {
        float levels[2][kScopeBands];
        for (int c = 0; c < 2; ++c) {
            for (int b = 0; b < kScopeBands; ++b) {
                double rms =
                    std::sqrt(sumSquares[c][b] / (double)blockFrames);
                // A band cannot carry more energy than a full scale signal, so
                // anything above that is a transient in the filter, not sound.
                // Clamping here keeps one such frame from throwing a bar to the
                // top of the strip for no reason.
                if (rms > 1.0) {
                    rms = 1.0;
                }
                levels[c][b] = (float)rms;
                sumSquares[c][b] = 0.0;
            }
        }
        // The loudest block of the session, for the log: it is how the
        // sensitivity gets chosen without guessing.
        for (int c = 0; c < 2; ++c) {
            for (int b = 0; b < kScopeBands; ++b) {
                if (levels[c][b] > peakRms) {
                    peakRms = levels[c][b];
                }
            }
        }

        g_scope.sequence.fetch_add(1, std::memory_order_acq_rel);
        std::memcpy(g_scope.rms, levels, sizeof(levels));
        g_scope.tick.store(GetTickCount(), std::memory_order_relaxed);
        g_scope.sequence.fetch_add(1, std::memory_order_acq_rel);

        blockFrames = 0;
    }
};

// Loopback packets arrive as whatever the shared mix format is, which on every
// Windows 10 and 11 machine is 32 bit float. A format that is not float is
// reported once and turns the visualizer off, rather than being guessed at.
bool IsFloatFormat(const WAVEFORMATEX* format) {
    if (!format) {
        return false;
    }
    if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        return true;
    }
    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE && format->cbSize >= 22) {
        // KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, written out rather than taken from
        // ksmedia.h, which would drag in a library for one constant.
        static const GUID kFloatSubtype = {
            0x00000003, 0x0000, 0x0010,
            {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
        const WAVEFORMATEXTENSIBLE* ext =
            reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
        return IsEqualGUID(ext->SubFormat, kFloatSubtype);
    }
    return false;
}

// True when the default playback device is no longer the one `boundId` names.
// Called from the capture thread, which owns the enumerator.
bool DefaultEndpointChanged(IMMDeviceEnumerator* enumerator, PCWSTR boundId) {
    if (!boundId) {
        return false;
    }
    IMMDevice* current = nullptr;
    if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &current)) ||
        !current) {
        return false;
    }
    bool changed = false;
    LPWSTR currentId = nullptr;
    if (SUCCEEDED(current->GetId(&currentId)) && currentId) {
        changed = lstrcmpW(currentId, boundId) != 0;
        CoTaskMemFree(currentId);
    }
    current->Release();
    return changed;
}

// The capture thread's own COM apartment. Loopback capture is a plain WASAPI
// client, so it neither needs nor wants the widget's WinRT apartment.
DWORD WINAPI CaptureThreadProc(LPVOID) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool comReady = SUCCEEDED(hr);

    auto fail = [](PCWSTR what, HRESULT code) {
        Wh_Log(L"visualizer: %s failed: 0x%08X", what, (unsigned)code);
    };

    if (comReady) {
        IMMDeviceEnumerator* enumerator = nullptr;
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                              __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
        if (FAILED(hr) || !enumerator) {
            fail(L"the device enumerator", hr);
        } else {
            // The default *console* device is what a player's audio goes to.
            // The communications and multimedia endpoints can differ from it,
            // and capturing the wrong one would show silence.
            IMMDevice* device = nullptr;
            hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
            if (FAILED(hr) || !device) {
                fail(L"opening the playback device", hr);
            } else {
                // The endpoint this client is bound to. A loopback client stays
                // on the device it was opened on: when the default output is
                // switched (headphones, Bluetooth) the old endpoint keeps
                // running and delivers silence, so the thread has to notice the
                // change itself - it does not end on its own.
                LPWSTR boundId = nullptr;
                device->GetId(&boundId);

                IAudioClient* client = nullptr;
                hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                      (void**)&client);
                if (FAILED(hr) || !client) {
                    fail(L"activating the audio client", hr);
                } else {
                    WAVEFORMATEX* mix = nullptr;
                    hr = client->GetMixFormat(&mix);
                    if (FAILED(hr) || !mix) {
                        fail(L"reading the mix format", hr);
                    } else if (!IsFloatFormat(mix)) {
                        Wh_Log(L"visualizer: the mix format is not 32 bit float "
                               L"(tag %u, %u bits); the bars stay synthetic",
                               mix->wFormatTag, mix->wBitsPerSample);
                    } else {
                        hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                                AUDCLNT_STREAMFLAGS_LOOPBACK,
                                                1000000, 0, mix, nullptr);
                        if (FAILED(hr)) {
                            // The device can be in exclusive use by a player
                            // that asked for it; that is the player's own choice
                            // and there is nothing to fix here.
                            fail(L"starting the loopback capture", hr);
                        } else {
                            IAudioCaptureClient* capture = nullptr;
                            hr = client->GetService(
                                __uuidof(IAudioCaptureClient), (void**)&capture);
                            if (FAILED(hr) || !capture) {
                                fail(L"getting the capture client", hr);
                            } else {
                                UINT32 bufferFrames = 0;
                                client->GetBufferSize(&bufferFrames);
                                ScopeAnalyser analyser;
                                analyser.Configure((double)mix->nSamplesPerSec);
                                const int channelCount = mix->nChannels;

                                hr = client->Start();
                                if (FAILED(hr)) {
                                    fail(L"starting the audio client", hr);
                                } else {
                                    g_captureWorks.store(true);
                                    g_captureAlive.store(true);
                                    Wh_Log(L"visualizer: capturing %u Hz, %u "
                                           L"channel(s), %u frame buffer",
                                           mix->nSamplesPerSec, mix->nChannels,
                                           bufferFrames);

                                    // One line per second is plenty, and it
                                    // keeps the peak visible while the
                                    // sensitivity is being chosen.
                                    DWORD lastReport = GetTickCount();
                                    DWORD lastEndpointCheck = lastReport;
                                    while (WaitForSingleObject(
                                               g_captureStopEvent, 5) ==
                                           WAIT_TIMEOUT) {
                                        // Once a second is plenty. Ending the
                                        // loop lets EnsureCaptureRunning start a
                                        // fresh capture, which binds to the new
                                        // default device.
                                        if (GetTickCount() - lastEndpointCheck >=
                                            1000) {
                                            lastEndpointCheck = GetTickCount();
                                            if (DefaultEndpointChanged(
                                                    enumerator, boundId)) {
                                                Wh_Log(L"visualizer: the default "
                                                       L"playback device changed; "
                                                       L"capture moves to it");
                                                break;
                                            }
                                        }
                                        UINT32 packet = 0;
                                        if (FAILED(capture->GetNextPacketSize(
                                                &packet))) {
                                            break;
                                        }
                                        while (packet > 0) {
                                            BYTE* data = nullptr;
                                            UINT32 frames = 0;
                                            DWORD flags = 0;
                                            if (FAILED(capture->GetBuffer(
                                                    &data, &frames, &flags,
                                                    nullptr, nullptr))) {
                                                break;
                                            }
                                            analyser.Process(
                                                reinterpret_cast<const float*>(
                                                    data),
                                                frames, channelCount,
                                                (flags &
                                                 AUDCLNT_BUFFERFLAGS_SILENT) != 0);
                                            capture->ReleaseBuffer(frames);
                                            if (FAILED(capture->GetNextPacketSize(
                                                    &packet))) {
                                                break;
                                            }
                                        }

                                        DWORD now = GetTickCount();
                                        if (now - lastReport >= 1000) {
                                            lastReport = now;
                                            if (analyser.peakRms > 0.0) {
                                                Wh_Log(L"visualizer: loudest "
                                                       L"block so far %.4f rms "
                                                       L"(%.1f dBFS)",
                                                       analyser.peakRms,
                                                       20.0 * log10(
                                                           analyser.peakRms));
                                            }
                                        }
                                    }
                                    client->Stop();
                                    g_captureAlive.store(false);
                                    Wh_Log(L"visualizer: capture stopped");
                                }
                            }
                            if (capture) {
                                capture->Release();
                            }
                        }
                    }
                    if (mix) {
                        CoTaskMemFree(mix);
                    }
                    client->Release();
                }
                CoTaskMemFree(boundId);
                device->Release();
            }
            enumerator->Release();
        }
    } else {
        fail(L"CoInitializeEx", hr);
    }

    if (comReady) {
        CoUninitialize();
    }
    if (g_captureExitedEvent) {
        SetEvent(g_captureExitedEvent);
    }
    return 0;
}

// Brings the capture thread up. Called from the mod's own init; it is also safe
// to call again after the thread has ended, which is how a device change - new
// headphones, another default output - gets picked up.
void StartCapture() {
    if (g_captureThread.load()) {
        return;
    }
    if (!g_captureStopEvent || !g_captureExitedEvent) {
        return;
    }
    ResetEvent(g_captureStopEvent);
    ResetEvent(g_captureExitedEvent);
    HANDLE thread =
        CreateThread(nullptr, 0, CaptureThreadProc, nullptr, 0, nullptr);
    if (!thread) {
        Wh_Log(L"visualizer: CreateThread failed: %u", GetLastError());
        return;
    }
    g_captureThread.store(thread);
}

// Ends the capture thread and waits for it, so nothing of this library can
// still be running after the mod unloads.
void StopCapture() {
    if (g_captureStopEvent) {
        SetEvent(g_captureStopEvent);
    }
    HANDLE thread = g_captureThread.exchange(nullptr);
    if (!thread) {
        return;
    }
    if (g_captureExitedEvent) {
        DWORD wait = WaitForSingleObject(g_captureExitedEvent, 3000);
        if (wait != WAIT_OBJECT_0) {
            Wh_Log(L"visualizer: capture thread did not exit in time (%u)", wait);
        }
    }
    CloseHandle(thread);
    g_captureAlive.store(false);
}

// Called from the poll timer. A capture thread that has ended - the device was
// removed, or it was busy when the widget started - is brought back, so
// switching the default output does not leave the bars synthetic for good.
void EnsureCaptureRunning() {
    HANDLE thread = g_captureThread.load();
    if (thread) {
        // The thread ends on its own when the loopback client fails - after
        // sleep the audio device is often invalidated - and then the handle is
        // still set. Reap it here, or the capture is never brought back.
        if (WaitForSingleObject(thread, 0) != WAIT_OBJECT_0) {
            return;
        }
        g_captureThread.store(nullptr);
        CloseHandle(thread);
        g_captureAlive.store(false);
        Wh_Log(L"visualizer: capture thread had ended; restarting it");
    }
    static DWORD lastAttempt = 0;
    DWORD now = GetTickCount();
    if (now - lastAttempt < 2000) {
        return;
    }
    lastAttempt = now;
    StartCapture();
}

// ---------------------------------------------------------------------------
// Album art
//
// The player publishes the track's artwork as a stream reference. It is decoded
// when the track changes - with retries, because the player needs a moment to
// download it - and kept as raw BGRA pixels, which the renderer wraps in a
// GDI+ bitmap. On YouTube the same thumbnail is the video still, so the widget
// shows a preview there as well.
// ---------------------------------------------------------------------------

// Decoded a little larger than it is drawn, so the downscale stays crisp on
// high-DPI displays.
constexpr int kArtDecodeSize = 64;

bool DecodeThumbnail(
    winrt::Windows::Storage::Streams::IRandomAccessStreamReference const& reference,
    std::vector<BYTE>& pixels, int& size) {
    if (!reference) {
        return false;
    }

    using namespace winrt::Windows::Graphics::Imaging;

    try {
        auto stream = reference.OpenReadAsync().get();
        auto decoder = BitmapDecoder::CreateAsync(stream).get();

        BitmapTransform transform;
        transform.ScaledWidth(kArtDecodeSize);
        transform.ScaledHeight(kArtDecodeSize);
        transform.InterpolationMode(BitmapInterpolationMode::Linear);

        auto pixelData =
            decoder
                .GetPixelDataAsync(BitmapPixelFormat::Bgra8,
                                   BitmapAlphaMode::Premultiplied, transform,
                                   ExifOrientationMode::IgnoreExifOrientation,
                                   ColorManagementMode::DoNotColorManage)
                .get();

        auto buffer = pixelData.DetachPixelData();
        if (buffer.size() <
            (uint32_t)kArtDecodeSize * kArtDecodeSize * 4) {
            return false;
        }

        pixels.assign(buffer.begin(),
                      buffer.begin() + (size_t)kArtDecodeSize * kArtDecodeSize * 4);
        size = kArtDecodeSize;
        return true;
    } catch (...) {
        return false;
    }
}

// ---------------------------------------------------------------------------
// Media session
// ---------------------------------------------------------------------------

std::atomic<bool> g_stopThread{false};
// Signalled by the polling thread right before it returns. Wh_ModUninit waits
// on it, because returning from unload while this thread still runs would let
// Explorer execute code from an unloaded module.
HANDLE g_threadExitedEvent = nullptr;
// The same handshake for the command thread, which Wh_ModUninit also has to
// outlive: it can be woken by a click at any moment.
HANDLE g_commandThreadExitedEvent = nullptr;
// Owned by the polling thread: it both creates the window and runs its loop.
// Atomic because the unload path reads it from another thread.
std::atomic<HWND> g_widgetWindow{nullptr};

// The frosted backdrop of the widget window, shared between the code that talks
// to the compositor (ConfigureWidgetBackdrop) and the drawing code (PaintWidget).
// Both run on the polling thread that owns the window, so no lock is needed.
struct GlassState {
    bool on = false;
    // True when the glass already carries the panel's tint, so painting it again
    // in the bitmap would darken the panel twice.
    bool skipPanelFill = false;
    // The radius the panel is drawn with, in design units: DWM's own rounding
    // while the glass is on, the theme's radius otherwise.
    int cornerRadius = 0;
};
GlassState g_glass;
GlobalSystemMediaTransportControlsSession g_session{nullptr};
std::mutex g_sessionMutex;

long long ToMs(TimeSpan const& t) {
    return t.count() / 10000;  // 100-ns ticks -> ms
}

// The taskbar can sit on a monitor with a different scale factor, so every
// measurement is taken through this helper instead of assuming 96 dpi.
int GetWindowDpi(HWND hwnd) {
    static auto getDpiForWindow = (UINT(WINAPI*)(HWND))GetProcAddress(
        GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    if (getDpiForWindow) {
        UINT value = getDpiForWindow(hwnd);
        if (value >= 72 && value <= 480) {
            return (int)value;
        }
    }
    return 96;
}

// Prefers whatever Windows treats as the active session, then any playing one,
// then the first session, so a paused track keeps showing.
GlobalSystemMediaTransportControlsSession PickSession(
    GlobalSystemMediaTransportControlsSessionManager const& manager) {
    GlobalSystemMediaTransportControlsSession first{nullptr};
    try {
        if (auto current = manager.GetCurrentSession()) {
            return current;
        }
    } catch (...) {
    }

    for (auto const& s : manager.GetSessions()) {
        if (!first) {
            first = s;
        }
        try {
            if (s.GetPlaybackInfo().PlaybackStatus() ==
                GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing) {
                return s;
            }
        } catch (...) {
        }
    }
    return first;
}

// ---------------------------------------------------------------------------
// Optimistic feedback: what to draw while the player is still answering
//
// A command reaches the player immediately, but the player's own answer comes a
// round trip later. Until then the widget draws what the click meant: the button
// flips, and a seek moves the playhead. This is kept apart from the polled state
// on purpose - the state the polling thread anchors and counts from must stay
// exactly what the player reported, or a stale value it publishes afterwards
// would be mistaken for a real event and the progress bar would jump back.
// ---------------------------------------------------------------------------

struct OptimisticState {
    // False once the overlay is spent or replaced by a command that has none.
    bool valid = false;
    // The play state the click asked for, not a toggle of anything: this is the
    // answer to "what should the button show", so a repeated click cannot flip
    // the icon twice.
    bool playing = false;
    long long anchorPositionMs = 0;
    unsigned long long anchorTick = 0;
};

std::mutex g_optimisticMutex;
OptimisticState g_optimistic;

// clear = true for a track change: no overlay can describe the new track.
void SetOptimisticState(bool playing, long long anchorPositionMs, bool hasAnchor,
                        bool clear = false) {
    std::lock_guard<std::mutex> lock(g_optimisticMutex);
    if (clear) {
        g_optimistic = OptimisticState{};
        return;
    }
    g_optimistic.valid = true;
    g_optimistic.playing = playing;
    // Set as a pair, never carried over: a stored anchorTick from an earlier
    // click used to survive, and StateForRendering then kept the old anchor
    // alive through a play/pause that had nothing to do with the position.
    g_optimistic.anchorPositionMs = hasAnchor ? anchorPositionMs : 0;
    g_optimistic.anchorTick = hasAnchor ? GetTickCount64() : 0;
}

// Whether the overlay is allowed to override the play state. It never expires
// on its own: a command's outcome is confirmed by the player's own event, not by
// a clock.
void ClearOptimisticState() {
    SetOptimisticState(false, 0, false, true);
}

// The polled state with the overlay applied, ready to be drawn.
MediaState StateForRendering() {
    MediaState state = GetState();
    OptimisticState overlay;
    {
        std::lock_guard<std::mutex> lock(g_optimisticMutex);
        overlay = g_optimistic;
    }
    if (!overlay.valid) {
        return state;
    }
    state.playing = overlay.playing;
    if (overlay.anchorTick) {
        state.anchorPositionMs = overlay.anchorPositionMs;
        state.anchorTick = overlay.anchorTick;
    }
    return state;
}

// Runs on the command thread, which is woken by QueueCommand.
//
// The session reference is copied out under the lock and the player call is made
// after it is released, so a command never waits for the poll thread's own
// session requests - and the poll thread is never made to wait for the player's
// answer to this one.
void RunPendingCommand() {
    CommandRequest req{};
    {
        std::lock_guard<std::mutex> lock(g_commandMutex);
        if (!g_hasPendingCommand) {
            return;
        }
        req = g_pendingCommand;
        g_hasPendingCommand = false;
        g_pendingCommand.queuedTick = 0;
    }

    // What the click costs before the player is asked to do anything, and how
    // long the player then took to accept it. The first number is the one this
    // mod is responsible for; the second one is the player's own.
    unsigned long long sentTick = GetTickCount64();
    if (req.queuedTick) {
        Wh_Log(L"command %d reached the player after %llu ms", (int)req.command,
               sentTick - req.queuedTick);
    }

    GlobalSystemMediaTransportControlsSession session{nullptr};
    {
        std::lock_guard<std::mutex> lock(g_sessionMutex);
        session = g_session;
    }
    if (!session) {
        return;
    }

    try {
        switch (req.command) {
            case Command::PlayPause:
                session.TryTogglePlayPauseAsync().get();
                break;
            case Command::Next:
                session.TrySkipNextAsync().get();
                break;
            case Command::Prev:
                session.TrySkipPreviousAsync().get();
                break;
            case Command::Seek:
                session.TryChangePlaybackPositionAsync(req.seekMs * 10000).get();
                break;
        }
    } catch (...) {
    }

    if (req.queuedTick) {
        Wh_Log(L"command %d accepted by the player after %llu ms",
               (int)req.command, GetTickCount64() - sentTick);
    }

    // The widget window belongs to the message thread, so the repaint is asked
    // for with a message rather than by drawing from here.
    if (HWND widget = g_widgetWindow.load()) {
        PostMessageW(widget, kForceRefreshMessage, 0, 0);
    }
}

// ---------------------------------------------------------------------------
// Session tracking
// ---------------------------------------------------------------------------

// Explicit event tokens are used instead of the auto-revoke overloads, because
// the revoker type names are only emitted by the header in some SDK versions.
winrt::event_token g_timelineToken{};
winrt::event_token g_mediaToken{};
winrt::event_token g_playbackToken{};
bool g_subscribed = false;

// Anchors on the session passed in. Taking the session as an argument avoids
// re-locking the session mutex from inside an event callback, which could
// deadlock when the callback fires synchronously during subscription.
//
// The player republishes the same stale value continuously while playing, so a
// blind re-anchor here would reset the counted time several times a second and
// freeze the progress bar at the track start. Only a genuinely new value counts
// as a real event.
void AnchorFromSession(
    GlobalSystemMediaTransportControlsSession const& session) {
    auto timeline = session.GetTimelineProperties();
    long long timelinePositionMs = ToMs(timeline.Position());
    unsigned long long now = GetTickCount64();

    static bool havePublished = false;
    static long long publishedMs = 0;

    if (havePublished && timelinePositionMs == publishedMs) {
        return;  // the same stale value the player keeps repeating
    }

    havePublished = true;
    publishedMs = timelinePositionMs;

    std::lock_guard<std::mutex> stateLock(g_stateMutex);
    g_state.anchorPositionMs = timelinePositionMs;
    g_state.anchorTick = now;
    g_state.durationMs = ToMs(timeline.EndTime());
    Wh_Log(L"anchor <- event: %lld ms (dur %lld ms)", timelinePositionMs,
           g_state.durationMs);
}

// The state a command asks for; the same numbers the API reports.
constexpr int kStatusClosed = 0;
constexpr int kStatusPlaying = 4;

// Whether this player has ever raised a playback event. The poll timer reads it
// to decide who owns the play state: while the widget is subscribed and the
// player has proven it raises events, the event owns it and polling keeps its
// hands off (see RefreshState).
std::atomic<bool> g_playbackEventSeen{false};

// Applies what the player just reported, without waiting for the next poll.
//
// While this player keeps raising events, what is written here IS the state -
// the poll timer deliberately does not overwrite the play state any more (see
// RefreshState) - so the icon follows the player, not the polling clock.
//
// Two things are deliberately NOT done here:
//  * the position is not touched - that is AnchorFromSession's job, and a second
//    writer of the anchor would fight it;
//  * the work is skipped rather than waited for if the polling thread is inside
//    RefreshState right now - the next tick is at most one interval away, and
//    blocking the player's own event thread is how deadlocks start.
void ReportPlaybackEvent(
    GlobalSystemMediaTransportControlsSession const& session) {
    static std::timed_mutex stateLock;

    int status = -1;
    long long positionMs = 0;
    try {
        auto info = session.GetPlaybackInfo();
        status = (int)info.PlaybackStatus();
        positionMs = ToMs(session.GetTimelineProperties().Position());
    } catch (...) {
        return;
    }

    {
        std::unique_lock<std::timed_mutex> lock(stateLock, std::try_to_lock);
        if (!lock.owns_lock()) {
            return;  // a poll is already reading; it will publish this itself
        }

        static bool haveStatus = false;
        static int lastStatus = -1;
        static long long lastPositionMs = -1;

        bool statusChanged = (!haveStatus || status != lastStatus);
        bool positionChanged = (!haveStatus || positionMs != lastPositionMs);

        if (statusChanged) {
            Wh_Log(L"player reported status %d", status);
            g_playbackEventSeen.store(true);

            // The player has spoken: the guess a click left behind is spent.
            // Without this the overlay would keep the icon on the state the
            // click asked for even after the player changed its mind.
            SetOptimisticState(false, 0, false, true);

            if (status == kStatusClosed) {
                // A real stop: the position is not anchored to anything
                // meaningful any more, so the bar goes back to the start.
                g_state.anchorPositionMs = 0;
                g_state.anchorTick = GetTickCount64();
            }
            g_state.playing = (status == kStatusPlaying);
        }

        if (!statusChanged && !positionChanged) {
            return;  // the same stale pair the player keeps repeating
        }

        haveStatus = true;
        lastStatus = status;
        lastPositionMs = positionMs;
    }

    // The widget window belongs to the message thread, so the redraw is asked
    // for with a message rather than done from here.
    if (HWND widget = g_widgetWindow.load()) {
        PostMessageW(widget, kForceRefreshMessage, 0, 0);
    }
}

// Attaches change handlers so a seek, a pause or a track change updates the
// widget at once, instead of waiting for the next poll.
//
// A pause is the case this exists for: the polling thread anchors the position
// from the timeline event, and the playback state - the icon - comes from the
// playback event.
void SubscribeToSession(
    GlobalSystemMediaTransportControlsSession const& session) {
    try {
        g_timelineToken = session.TimelinePropertiesChanged(
            [](auto const& sender, auto const&) {
                auto session =
                    sender.template as<GlobalSystemMediaTransportControlsSession>();
                AnchorFromSession(session);
                ReportPlaybackEvent(session);
            });
        g_mediaToken = session.MediaPropertiesChanged(
            [](auto const& sender, auto const&) {
                auto session =
                    sender.template as<GlobalSystemMediaTransportControlsSession>();
                AnchorFromSession(session);
                ReportPlaybackEvent(session);
            });
        g_playbackToken = session.PlaybackInfoChanged(
            [](auto const& sender, auto const&) {
                auto session =
                    sender.template as<GlobalSystemMediaTransportControlsSession>();
                AnchorFromSession(session);
                ReportPlaybackEvent(session);
            });
        g_subscribed = true;
    } catch (...) {
        // Subscriptions are an optimisation; polling still keeps things right.
    }
}

// Detaching matters even though a dropped session reference also drops its
// handlers: the widget would otherwise leave its own callbacks on a session it
// no longer reads, and a player with two sessions would fire the widget twice.
void UnsubscribeFromSession(
    GlobalSystemMediaTransportControlsSession const& session) {
    if (!g_subscribed) {
        return;
    }
    try {
        session.TimelinePropertiesChanged(g_timelineToken);
        session.MediaPropertiesChanged(g_mediaToken);
        session.PlaybackInfoChanged(g_playbackToken);
    } catch (...) {
    }
    g_subscribed = false;
    // The next session starts from scratch: whether it raises events at all is
    // not known until it does.
    g_playbackEventSeen.store(false);
}

void RefreshState() {
    MediaState next;
    bool gotSession = false;
    static std::wstring lastSessionId;
    // The session the handlers are attached to, so it can be detached by
    // reference rather than by name.
    static GlobalSystemMediaTransportControlsSession subscribedSession{nullptr};
    const unsigned long long now = GetTickCount64();

    try {
        auto manager =
            GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
        auto session = PickSession(manager);

        if (session) {
            gotSession = true;
            next.hasSession = true;

            std::wstring id = session.SourceAppUserModelId().c_str();
            bool sessionChanged = (id != lastSessionId);

            {
                std::lock_guard<std::mutex> lock(g_sessionMutex);
                g_session = session;
            }

            // Subscribe when the player changed AND when there is simply no
            // subscription yet. The second case is not hypothetical: the widget
            // usually attaches after the player has already been running for a
            // while, and the first poll then saw no change of id and subscribed
            // to nothing at all. Everything worked - but only at poll speed, so
            // a pause or a track change started from the player itself looked
            // late, while the widget's own buttons (which do not need events)
            // were already immediate.
            // The same app can come back as a new session object (Yandex Music
            // after it was closed or the music was stopped), with the same id.
            // The old object is dead and raises nothing, so the object itself is
            // compared too - otherwise the play state freezes on the stale one.
            if (sessionChanged || !g_subscribed || subscribedSession != session) {
                UnsubscribeFromSession(subscribedSession);
                lastSessionId = id;
                subscribedSession = session;
                SubscribeToSession(session);
            }

            auto props = session.TryGetMediaPropertiesAsync().get();
            next.title = props.Title().c_str();
            next.artist = props.Artist().c_str();

            // The artwork is decoded when the track changes - but NOT only once.
            //
            // Measured: right after a track change the player usually has not
            // published the cover yet, because it downloads it in the
            // background. A single attempt at that moment fails, and because the
            // attempt was remembered as done, the cover stayed missing for the
            // whole track. So retry for a while, then give up on this track.
            //
            // The key is title + artist on purpose. It used to include the
            // duration, but the duration is not assigned until later in this
            // function, so the key changed twice per track: a second, pointless
            // decode, and a second chance for the cover to be lost.
            constexpr unsigned long long kArtRetryMs = 1500;
            constexpr int kArtMaxAttempts = 8;
            {
                static std::wstring artTrack;
                static bool artLoaded = false;
                static int artAttempts = 0;
                static unsigned long long artRetryAt = 0;

                std::wstring track = next.title + L"\x1f" + next.artist;
                if (track != artTrack) {
                    artTrack = track;
                    artLoaded = false;
                    artAttempts = 0;
                    artRetryAt = 0;
                }

                // Reserve the slot as soon as the player says a cover exists,
                // so the layout does not jump sideways when the image arrives.
                bool thumbPublished = static_cast<bool>(props.Thumbnail());

                unsigned long long now = GetTickCount64();
                bool retryDue = artAttempts > 0 && !artLoaded &&
                                artAttempts < kArtMaxAttempts &&
                                now >= artRetryAt;

                if (artAttempts == 0 || retryDue) {
                    artRetryAt = now + kArtRetryMs;
                    artAttempts++;

                    std::vector<BYTE> artPixels;
                    int artSize = 0;
                    if (DecodeThumbnail(props.Thumbnail(), artPixels, artSize)) {
                        next.artPixels = std::move(artPixels);
                        next.artSize = artSize;
                        next.hasArt = true;
                        artLoaded = true;
                        Wh_Log(L"artwork decoded on attempt %d: %d x %d",
                               artAttempts, artSize, artSize);
                    } else {
                        next.hasArt = thumbPublished;
                        Wh_Log(L"artwork not available yet (attempt %d of %d)",
                               artAttempts, kArtMaxAttempts);
                    }
                } else if (artLoaded) {
                    // Same track: carry the art that is already loaded.
                    std::lock_guard<std::mutex> stateLock(g_stateMutex);
                    next.artPixels = g_state.artPixels;
                    next.artSize = g_state.artSize;
                    next.hasArt = true;
                } else {
                    // Waiting for the next attempt: no image for this track yet.
                    next.hasArt = thumbPublished;
                }
            }

            auto info = session.GetPlaybackInfo();
            next.playing = info.PlaybackStatus() ==
                           GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
            next.canSeek = info.Controls().IsPlaybackPositionEnabled();

            // A poll can only read the state the player had when it was asked,
            // and the player needs a moment to apply a command - so for a short
            // while after a click this value is the state *before* the click.
            // Publishing it would flip the icon back to what the user just
            // changed, which is exactly the "music paused, but the icon is
            // late" report: the icon flipped, flipped back, and only settled
            // when the player's own event arrived.
            //
            // So while the widget is subscribed to this player, the player's own
            // event is the authority on the play state and polling never touches
            // it. The one exception is a player that accepts the subscription and
            // then never raises an event - for that one the fallback below hands
            // the field back to polling, which is then the only source there is.
            //
            // Note that the test is "has this player ever raised an event", not
            // "did one arrive recently": Yandex Music publishes nothing at all
            // during ordinary playback, so a recent-event test would quietly
            // hand the icon back to the polling clock a couple of seconds into
            // every track - which is the delay this code exists to remove.
            if (g_subscribed && g_playbackEventSeen.load()) {
                next.playing = g_state.playing;
            }

            auto timeline = session.GetTimelineProperties();
            long long publishedPositionMs = ToMs(timeline.Position());
            next.durationMs = ToMs(timeline.EndTime());

            // How the position is tracked.
            //
            // Some players (Yandex Music among them) stop updating the timeline
            // while playing: the value stays frozen at whatever it was, and
            // LastUpdatedTime never moves. Measured directly - see
            // probe-watch.txt. Trusting such a value would pin the progress bar
            // at the track start forever.
            //
            // So the player is trusted only while it keeps producing new values.
            // Once it goes quiet for kPublishTimeoutMs, the widget stops
            // believing it and simply counts time. Any new value (a seek, a
            // track change, a pause) restores trust immediately.
            constexpr unsigned long long kPublishTimeoutMs = 12000;

            static long long lastDurationMs = -1;
            static bool wasPlaying = false;
            static bool havePublished = false;
            static long long publishedMs = 0;
            static unsigned long long lastPublishChangeTick = 0;
            static bool playerTrusted = true;

            bool trackChanged = (next.durationMs != lastDurationMs);
            bool startedPlaying = (next.playing && !wasPlaying);
            bool valueChanged = (!havePublished || publishedPositionMs != publishedMs);

            if (valueChanged) {
                lastPublishChangeTick = now;
                if (!playerTrusted) {
                    Wh_Log(L"player spoke again, trusting it");
                }
                playerTrusted = true;
            } else if (lastPublishChangeTick &&
                       now - lastPublishChangeTick > kPublishTimeoutMs) {
                if (playerTrusted) {
                    Wh_Log(L"player went quiet for %llu ms, counting on our own",
                           now - lastPublishChangeTick);
                }
                playerTrusted = false;
            }

            bool anchor = false;
            if (!havePublished || trackChanged || startedPlaying) {
                anchor = true;
            } else if (playerTrusted && valueChanged) {
                anchor = true;
            }

            if (anchor) {
                next.anchorPositionMs = publishedPositionMs;
                next.anchorTick = now;
                Wh_Log(L"anchor <- player: %lld ms (dur %lld ms, playing %d)",
                       publishedPositionMs, next.durationMs, (int)next.playing);
            } else {
                // Keep counting from the anchor we already hold.
                next.anchorPositionMs = g_state.anchorPositionMs;
                next.anchorTick = g_state.anchorTick;
                if (g_state.anchorTick == 0) {
                    next.anchorPositionMs = publishedPositionMs;
                    next.anchorTick = now;
                }
            }

            havePublished = true;
            publishedMs = publishedPositionMs;
            lastDurationMs = next.durationMs;
            // Must be updated, otherwise "started playing" stays true forever
            // and every poll re-anchors, which freezes the progress bar.
            wasPlaying = next.playing;
        }
    } catch (...) {
        gotSession = false;
    }

    if (!gotSession) {
        // No session: drop the handlers on the old one now, so a session that
        // comes back later is subscribed to as a new session, not skipped.
        UnsubscribeFromSession(subscribedSession);
        subscribedSession = nullptr;
    }

    bool noSession = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        if (!gotSession) {
            g_state = MediaState{};
            noSession = true;
        } else {
            g_state = next;
        }
    }

    if (noSession) {
        // No player at all: an overlay could only describe a track that does not
        // exist, and keeping one would leave the last click's icon on an empty
        // widget. Cleared outside the state lock so the two locks are never held
        // in two different orders.
        ClearOptimisticState();
    }
}

// ---------------------------------------------------------------------------
// The taskbar's own theme
//
// The widget sits on the taskbar, so it can wear the taskbar's own surface: the
// corner radius, the background brush and the border come from the theme selected
// in the Windows 11 Taskbar Styler mod, and the user's own controlStyles entries
// for the taskbar frame win over the theme.
//
// That mod keeps every theme in its own source file, so this reads it from there
// - the same text the styler itself applies to the taskbar. Anything that cannot
// be resolved (the styler is not installed, the source is missing, the syntax is
// not one of the brushes below) leaves the widget on the design's own panel, the
// one design.pen specifies.
//
// What cannot be copied is the blur: WindhawkBlur and AcrylicBrush ask the
// compositor to blur whatever is behind the taskbar. The widget is translucent
// instead, so the taskbar's own blurred surface shows through it - which looks
// the same from the outside and needs no compositor tricks.
// ---------------------------------------------------------------------------

namespace styler {

constexpr PCWSTR kSettingsKey =
    L"SOFTWARE\\Windhawk\\Engine\\Mods\\windows-11-taskbar-styler\\Settings";
constexpr PCWSTR kSourcePath =
    L"C:\\ProgramData\\Windhawk\\ModsSource\\windows-11-taskbar-styler.wh.cpp";

struct Stop {
    Gdiplus::REAL offset = 0.0f;
    BYTE r = 0, g = 0, b = 0, a = 255;
};

// One brush from the theme: a solid colour, a vertical gradient, or nothing.
struct Brush {
    bool present = false;
    bool gradient = false;
    // True when the theme asked for a blur (WindhawkBlur / AcrylicBrush) and the
    // fields below are only that brush's tint. Such a background is not painted
    // flat: the widget's own glass reproduces it, see ConfigureWidgetBackdrop.
    bool blur = false;
    BYTE r = 0, g = 0, b = 0, a = 255;
    Stop stops[4]{};
    int stopCount = 0;
};

// The taskbar surface the widget should wear.
struct Surface {
    bool fromTheme = false;  // false: the design's own panel and no border
    bool lightSystem = false;
    std::wstring themeName;
    Gdiplus::REAL cornerRadius = 8.0f;
    // The theme's own Margin for the frame, bottom part. FrostyGlass asks for
    // `Margin=0,0,0,4`, so its pills are a 44 px box at the top of the 48 px
    // taskbar - 2 px higher than the middle. The widget follows the same box.
    int bottomMargin = 0;
    Brush background;
    Brush border;
    Gdiplus::REAL borderThickness = 0.0f;
};

std::wstring ReadStylerString(PCWSTR name) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, kSettingsKey, 0,
                      KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS) {
        return L"";
    }
    wchar_t buffer[1024]{};
    DWORD size = sizeof(buffer);
    DWORD type = 0;
    std::wstring result;
    if (RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<LPBYTE>(buffer),
                         &size) == ERROR_SUCCESS &&
        (type == REG_SZ || type == REG_EXPAND_SZ)) {
        result.assign(buffer);
    }
    RegCloseKey(key);
    return result;
}

bool ReadWholeFile(PCWSTR path, std::string& out) {
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER size{};
    bool ok = GetFileSizeEx(file, &size) && size.QuadPart > 0 &&
              size.QuadPart < (32LL << 20);
    if (ok) {
        out.resize((size_t)size.QuadPart);
        DWORD read = 0;
        ok = ReadFile(file, out.data(), (DWORD)out.size(), &read, nullptr) &&
             read == out.size();
    }
    CloseHandle(file);
    if (!ok) {
        out.clear();
    }
    return ok;
}

std::string Trim(std::string const& text) {
    size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return std::string();
    }
    size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

std::string ToAsciiIdentifier(std::wstring const& name) {
    std::string out;
    out.reserve(name.size());
    for (wchar_t ch : name) {
        out.push_back(ch > 0 && ch < 128 ? (char)ch : '_');
    }
    return out;
}

// Index just past the brace that closes the one at `open`, ignoring braces inside
// string literals and comments - themes are full of XAML with braces in it.
size_t MatchBrace(std::string const& text, size_t open) {
    int depth = 0;
    for (size_t i = open; i < text.size(); ++i) {
        char c = text[i];
        if (c == '"') {
            for (++i; i < text.size(); ++i) {
                if (text[i] == '\\') {
                    ++i;
                    continue;
                }
                if (text[i] == '"') {
                    break;
                }
            }
            continue;
        }
        if (c == '/' && i + 1 < text.size() && text[i + 1] == '/') {
            while (i < text.size() && text[i] != '\n') {
                ++i;
            }
            continue;
        }
        if (c == '/' && i + 1 < text.size() && text[i + 1] == '*') {
            i += 2;
            while (i + 1 < text.size() && !(text[i] == '*' && text[i + 1] == '/')) {
                ++i;
            }
            ++i;
            continue;
        }
        if (c == '{') {
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0) {
                return i + 1;
            }
        }
    }
    return std::string::npos;
}

// Copies one character of a C++ string literal, resolving its escapes: the themes
// are full of XAML with \" in it.
void AppendUnescaped(std::string const& text, size_t& i, std::string& out) {
    if (text[i] != '\\' || i + 1 >= text.size()) {
        out.push_back(text[i++]);
        return;
    }
    switch (text[i + 1]) {
        case 'n':
            out.push_back('\n');
            break;
        case 't':
            out.push_back('\t');
            break;
        case 'r':
            out.push_back('\r');
            break;
        case '"':
            out.push_back('"');
            break;
        case '\\':
            out.push_back('\\');
            break;
        default:
            // \uXXXX and anything else stays as written; none of it is needed to
            // read a brush.
            out.push_back('\\');
            out.push_back(text[i + 1]);
            break;
    }
    i += 2;
}

// Every `L"Name=Value"` in a group of the theme, with a trailing ':' on the name
// removed (`CornerRadius:=10` and `CornerRadius=10` mean the same to us).
std::vector<std::pair<std::string, std::string>> ParsePairs(std::string const& group) {
    std::vector<std::pair<std::string, std::string>> out;
    size_t i = 0;
    while ((i = group.find("L\"", i)) != std::string::npos) {
        i += 2;
        std::string value;
        while (i < group.size() && group[i] != '"') {
            AppendUnescaped(group, i, value);
        }
        ++i;
        size_t equals = value.find('=');
        if (equals != std::string::npos && equals > 0) {
            std::string name = value.substr(0, equals);
            if (!name.empty() && name.back() == ':') {
                name.pop_back();
            }
            out.emplace_back(name, value.substr(equals + 1));
        }
    }
    return out;
}

// The styles of every entry in the theme whose target names the taskbar frame.
std::vector<std::pair<std::string, std::string>> FindFrameStyles(
    std::string const& group) {
    std::vector<std::pair<std::string, std::string>> out;
    size_t i = 0;
    while ((i = group.find("L\"", i)) != std::string::npos) {
        size_t begin = i + 2;
        std::string target;
        size_t p = begin;
        while (p < group.size() && group[p] != '"') {
            AppendUnescaped(group, p, target);
        }
        size_t after = p + 1;
        size_t next = after;
        auto isSpace = [](char c) {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n';
        };
        while (next < group.size() && isSpace(group[next])) {
            ++next;
        }
        if (next >= group.size() || group[next] != ',') {
            i = after;  // a style, not a target
            continue;
        }
        size_t brace = group.find('{', next);
        if (brace == std::string::npos) {
            break;
        }
        size_t end = MatchBrace(group, brace);
        if (end == std::string::npos) {
            break;
        }
        bool isFrame = target.find("TaskbarFrame") != std::string::npos &&
                       target.find("RootGrid") != std::string::npos;
        if (isFrame) {
            std::string styles(group, brace + 1, end - 1 - (brace + 1));
            for (auto& pair : ParsePairs(styles)) {
                out.push_back(pair);
            }
        }
        i = end;
    }
    return out;
}

bool NamedColour(std::string const& name, Brush& out) {
    struct Entry {
        char const* name;
        BYTE r, g, b;
    };
    static Entry const entries[] = {
        {"Black", 0, 0, 0},       {"White", 255, 255, 255},
        {"Gray", 128, 128, 128},  {"Grey", 128, 128, 128},
        {"Red", 255, 0, 0},       {"Green", 0, 128, 0},
        {"Blue", 0, 0, 255},      {"Transparent", 0, 0, 0},
    };
    for (auto const& entry : entries) {
        if (name == entry.name) {
            out.present = name != "Transparent";
            out.gradient = false;
            out.r = entry.r;
            out.g = entry.g;
            out.b = entry.b;
            out.a = name == "Transparent" ? 0 : 255;
            return true;
        }
    }
    return false;
}

bool HexColour(std::string const& text, Brush& out) {
    std::string digits = text.substr(1);
    auto isHexDigit = [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
               (c >= 'A' && c <= 'F');
    };
    for (char c : digits) {
        if (!isHexDigit(c)) {
            return false;
        }
    }
    auto hex = [&](size_t at) {
        return (BYTE)strtoul(digits.substr(at, 2).c_str(), nullptr, 16);
    };
    out.gradient = false;
    out.present = true;
    if (digits.size() == 8) {
        out.a = hex(0);
        out.r = hex(2);
        out.g = hex(4);
        out.b = hex(6);
    } else if (digits.size() == 6) {
        out.a = 255;
        out.r = hex(0);
        out.g = hex(2);
        out.b = hex(4);
    } else if (digits.size() == 3) {
        out.a = 255;
        out.r = (BYTE)(strtoul(digits.substr(0, 1).c_str(), nullptr, 16) * 17);
        out.g = (BYTE)(strtoul(digits.substr(1, 1).c_str(), nullptr, 16) * 17);
        out.b = (BYTE)(strtoul(digits.substr(2, 1).c_str(), nullptr, 16) * 17);
    } else {
        return false;
    }
    return true;
}

bool Attribute(std::string const& tag, char const* name, std::string& value) {
    std::string needle = std::string(name) + "=\"";
    size_t at = tag.find(needle);
    if (at == std::string::npos) {
        return false;
    }
    size_t begin = at + needle.size();
    size_t end = tag.find('"', begin);
    if (end == std::string::npos) {
        return false;
    }
    value = tag.substr(begin, end - begin);
    return true;
}

// The system colours themes refer to by name. XAML keeps them to itself, so the
// few that matter are approximated from the system's light/dark preference and
// the user's accent palette.
bool SystemResource(std::string const& name, bool lightSystem, Brush& out) {
    auto solid = [&](BYTE r, BYTE g, BYTE b) {
        out.present = true;
        out.gradient = false;
        out.r = r;
        out.g = g;
        out.b = b;
        out.a = 255;
        return true;
    };

    if (name == "SystemAccentColor" || name.rfind("SystemAccentColor", 0) == 0) {
        DWORD colorization = 0;
        DWORD size = sizeof(colorization);
        DWORD type = 0;
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\DWM", 0,
                          KEY_READ, &key) == ERROR_SUCCESS) {
            if (RegQueryValueExW(key, L"ColorizationColor", nullptr, &type,
                                 reinterpret_cast<LPBYTE>(&colorization),
                                 &size) == ERROR_SUCCESS &&
                type == REG_DWORD) {
                RegCloseKey(key);
                return solid((BYTE)((colorization >> 16) & 0xFF),
                             (BYTE)((colorization >> 8) & 0xFF),
                             (BYTE)(colorization & 0xFF));
            }
            RegCloseKey(key);
        }
        return solid(0x00, 0x78, 0xD4);
    }

    if (name == "SystemChromeDarkColor" || name == "SystemChromeAltHighColor" ||
        name == "SystemAltHighColor" || name == "SystemChromeMediumColor") {
        return lightSystem ? solid(0xF3, 0xF3, 0xF3) : solid(0x1F, 0x1F, 0x1F);
    }
    if (name == "SystemAltLowColor" || name == "SystemAltMediumLowColor" ||
        name == "SystemChromeLowColor") {
        return lightSystem ? solid(0xFF, 0xFF, 0xFF) : solid(0x0F, 0x0F, 0x0F);
    }
    if (name == "SystemChromeHighColor" || name == "SystemChromeMediumHighColor" ||
        name == "SystemChromeAltMediumHighColor") {
        return solid(0x80, 0x80, 0x80);
    }
    return false;
}

bool ParseBrush(std::string const& raw, bool lightSystem, Brush& out) {
    std::string value = Trim(raw);
    if (value.empty()) {
        return false;
    }
    if (value[0] == '#') {
        return HexColour(value, out);
    }
    if (value[0] == '{') {
        if (value.compare(0, 15, "{ThemeResource ") != 0 || value.back() != '}') {
            return false;
        }
        std::string name = Trim(value.substr(15, value.size() - 16));
        return SystemResource(name, lightSystem, out);
    }
    if (value[0] != '<') {
        return NamedColour(value, out);
    }

    std::string tag = value.substr(1, value.find_first_of(" />") - 1);
    std::string tint;
    std::string opacity;
    if (tag == "WindhawkBlur" || tag == "AcrylicBrush") {
        if (!Attribute(value, "TintColor", tint)) {
            return false;
        }
        Brush colour;
        if (!ParseBrush(tint, lightSystem, colour)) {
            return false;
        }
        double alpha = 1.0;
        if (Attribute(value, "TintOpacity", opacity)) {
            alpha = strtod(opacity.c_str(), nullptr);
        } else if (tag == "AcrylicBrush" && Attribute(value, "Opacity", opacity)) {
            alpha = strtod(opacity.c_str(), nullptr);
        }
        if (alpha < 0.0) alpha = 0.0;
        if (alpha > 1.0) alpha = 1.0;
        out = colour;
        out.a = (BYTE)(alpha * colour.a + 0.5);
        out.present = out.a > 0;
        // A blur brush: what the widget's own glass is matched against.
        out.blur = true;
        return true;
    }
    if (tag == "SolidColorBrush") {
        std::string colour;
        if (!Attribute(value, "Color", colour)) {
            return false;
        }
        Brush solid;
        if (!ParseBrush(colour, lightSystem, solid)) {
            return false;
        }
        double alpha = 1.0;
        if (Attribute(value, "Opacity", opacity)) {
            alpha = strtod(opacity.c_str(), nullptr);
        }
        out = solid;
        out.a = (BYTE)(alpha * solid.a + 0.5);
        out.present = out.a > 0;
        return true;
    }
    if (tag == "LinearGradientBrush") {
        out.present = true;
        out.gradient = true;
        out.stopCount = 0;
        size_t at = 0;
        while (out.stopCount < 4 &&
               (at = value.find("<GradientStop", at)) != std::string::npos) {
            size_t end = value.find("/>", at);
            if (end == std::string::npos) {
                break;
            }
            std::string stop = value.substr(at, end - at);
            std::string colour;
            std::string offset;
            if (Attribute(stop, "Color", colour) && Attribute(stop, "Offset", offset)) {
                Brush parsed;
                if (ParseBrush(colour, lightSystem, parsed)) {
                    Stop& entry = out.stops[out.stopCount++];
                    entry.offset = (Gdiplus::REAL)strtod(offset.c_str(), nullptr);
                    entry.r = parsed.r;
                    entry.g = parsed.g;
                    entry.b = parsed.b;
                    entry.a = parsed.a;
                }
            }
            at = end;
        }
        return out.stopCount > 0;
    }
    return false;
}

// Loads the surface of the theme selected in the styler, or an empty surface when
// there is nothing to follow.
Surface Load() {
    Surface surface;
    std::wstring themeName = ReadStylerString(L"theme");
    if (themeName.empty()) {
        return surface;
    }
    surface.themeName = themeName;

    DWORD light = 0;
    DWORD size = sizeof(light);
    DWORD type = 0;
    HKEY key = nullptr;
    bool lightSystem = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\"
                      L"Personalize",
                      0, KEY_READ, &key) == ERROR_SUCCESS) {
        if (RegQueryValueExW(key, L"SystemUsesLightTheme", nullptr, &type,
                             reinterpret_cast<LPBYTE>(&light), &size) == ERROR_SUCCESS) {
            lightSystem = light != 0;
        }
        RegCloseKey(key);
    }
    surface.lightSystem = lightSystem;

    std::string source;
    if (!ReadWholeFile(kSourcePath, source)) {
        return surface;
    }

    std::string needle = "const Theme g_theme" + ToAsciiIdentifier(themeName) + " = {";
    size_t at = source.find(needle);
    if (at == std::string::npos) {
        return surface;
    }
    size_t open = at + needle.size() - 1;
    size_t close = MatchBrace(source, open);
    if (close == std::string::npos) {
        return surface;
    }

    // The body holds two groups: the target styles and the theme's variables.
    std::string body = source.substr(open + 1, close - 1 - (open + 1));
    std::vector<std::string> groups;
    size_t i = 0;
    while (i < body.size()) {
        if (body[i] == '{') {
            size_t end = MatchBrace(body, i);
            if (end == std::string::npos) {
                break;
            }
            groups.push_back(body.substr(i + 1, end - 1 - (i + 1)));
            i = end;
        } else {
            ++i;
        }
    }
    if (groups.empty()) {
        return surface;
    }

    std::vector<std::pair<std::string, std::string>> variables;
    if (groups.size() > 1) {
        variables = ParsePairs(groups.back());
    }

    auto styles = FindFrameStyles(groups.front());
    // The user's own controlStyles entries for the taskbar frame come last and win.
    for (int index = 0; index < 16; ++index) {
        wchar_t targetName[64];
        swprintf_s(targetName, L"controlStyles[%d].target", index);
        std::wstring target = ReadStylerString(targetName);
        if (target.empty()) {
            continue;
        }
        std::string narrow = ToAsciiIdentifier(target);
        if (narrow.find("TaskbarFrame") == std::string::npos ||
            narrow.find("RootGrid") == std::string::npos) {
            continue;
        }
        for (int style = 0; style < 16; ++style) {
            wchar_t styleName[64];
            swprintf_s(styleName, L"controlStyles[%d].styles[%d]", index, style);
            std::wstring entry = ReadStylerString(styleName);
            if (entry.empty()) {
                continue;
            }
            std::string text = ToAsciiIdentifier(entry);
            size_t equals = text.find('=');
            if (equals == std::string::npos || equals == 0) {
                continue;
            }
            std::string name = text.substr(0, equals);
            if (!name.empty() && name.back() == ':') {
                name.pop_back();
            }
            styles.emplace_back(name, text.substr(equals + 1));
        }
    }

    auto value = [&](char const* name, std::string& out) {
        for (auto const& pair : styles) {
            if (pair.first == name) {
                out = pair.second;
                if (!out.empty() && out[0] == '$') {
                    std::string variable = out.substr(1);
                    out.clear();
                    for (auto const& entry : variables) {
                        if (entry.first == variable) {
                            out = entry.second;
                            break;
                        }
                    }
                }
            }
        }
        return !out.empty();
    };

    std::string text;
    if (value("CornerRadius", text)) {
        surface.cornerRadius = (Gdiplus::REAL)strtod(text.c_str(), nullptr);
        if (surface.cornerRadius < 0.0f) {
            surface.cornerRadius = 0.0f;
        }
    }
    text.clear();
    if (value("Background", text)) {
        styler::Brush brush;
        if (ParseBrush(text, lightSystem, brush)) {
            surface.background = brush;
        }
    }
    // XAML Margin: one number for all sides, two for horizontal and vertical, or
    // four in the order left, top, right, bottom. Only the bottom one matters
    // here, because it is what lifts the pills off the bottom edge.
    text.clear();
    if (value("Margin", text)) {
        std::vector<double> parts;
        size_t at = 0;
        while (at <= text.size()) {
            size_t comma = text.find(',', at);
            std::string piece =
                text.substr(at, comma == std::string::npos ? std::string::npos
                                                           : comma - at);
            if (!Trim(piece).empty()) {
                parts.push_back(strtod(piece.c_str(), nullptr));
            }
            if (comma == std::string::npos) {
                break;
            }
            at = comma + 1;
        }
        double bottom = 0.0;
        if (parts.size() >= 4) {
            bottom = parts[3];
        } else if (parts.size() == 2) {
            bottom = parts[1];
        } else if (parts.size() == 1) {
            bottom = parts[0];
        }
        if (bottom > 0.0) {
            surface.bottomMargin = (int)(bottom + 0.5);
        }
    }
    text.clear();
    if (value("BorderThickness", text)) {
        surface.borderThickness = (Gdiplus::REAL)strtod(text.c_str(), nullptr);
        if (surface.borderThickness < 0.0f) {
            surface.borderThickness = 0.0f;
        }
    }
    text.clear();
    if (value("BorderBrush", text)) {
        styler::Brush brush;
        if (ParseBrush(text, lightSystem, brush)) {
            surface.border = brush;
        }
    }

    surface.fromTheme = true;
    return surface;
}

}  // namespace styler

// The surface the widget currently wears. Re-read when the styler's theme, its
// source file or the setting changes; the paint timer only ever reads it.
styler::Surface g_surface;
std::mutex g_surfaceMutex;
std::wstring g_surfaceStamp;

styler::Surface GetSurface() {
    std::lock_guard<std::mutex> lock(g_surfaceMutex);
    return g_surface;
}

// The stamp is the theme name plus the styler source's write time, so a theme
// change and a styler update are both noticed without parsing 1 MB every tick.
std::wstring SurfaceStamp() {
    std::wstring name = styler::ReadStylerString(L"theme");
    WIN32_FILE_ATTRIBUTE_DATA info{};
    if (!GetFileAttributesExW(styler::kSourcePath, GetFileExInfoStandard, &info)) {
        return name + L"|none";
    }
    wchar_t stamp[64];
    swprintf_s(stamp, L"|%08X%08X", info.ftLastWriteTime.dwHighDateTime,
               info.ftLastWriteTime.dwLowDateTime);
    return name + stamp;
}

void RefreshSurface(bool force) {
    Settings settings = GetSettings();
    std::wstring stamp = settings.followTaskbarTheme ? SurfaceStamp() : L"off";
    {
        std::lock_guard<std::mutex> lock(g_surfaceMutex);
        if (!force && stamp == g_surfaceStamp) {
            return;
        }
        g_surfaceStamp = stamp;
    }

    styler::Surface surface;
    if (settings.followTaskbarTheme) {
        surface = styler::Load();
    }
    Wh_Log(L"taskbar theme: %s (%s), radius %.1f, border %.1f",
           surface.themeName.c_str(), surface.fromTheme ? L"applied" : L"not found",
           surface.cornerRadius, surface.borderThickness);

    std::lock_guard<std::mutex> lock(g_surfaceMutex);
    g_surface = surface;
}

// ---------------------------------------------------------------------------
// Layout and painting (GDI+)
//
// The widget is a copy of the Pencil document design.pen: frame qBuu4 (the
// paused state, 465 units wide) and frame HLQsH (the playing state, 466 units
// wide). Every number below is one design unit read from those frames - not an
// estimate - and one unit is one pixel at 96 DPI, scaled by the display DPI
// when drawing.
//
// The coordinates stay fractional on purpose: the design puts its progress bar
// and its level meter on half pixels, and the canvas draws them the same way, so
// the widget does too.
//
// The widget is a layered window updated with UpdateLayeredWindow, which is what
// gives it true per-pixel alpha: the rounded corners are genuinely transparent
// and the taskbar shows through them. GDI+ is used for anti-aliased shapes and
// text, so the icons are smooth rather than jagged.
// ---------------------------------------------------------------------------

namespace design {

// GDI+'s floating point type, so the design numbers below read as numbers.
using Gdiplus::REAL;

// How far the design's right-hand group is pulled towards its left one, at the
// user's request. The left part of the widget is the cover with the title and
// the artist, the middle is the progress bar with the meter and the time, the
// right part is the three buttons; the middle and the right part move 35 units
// towards the text, and the panel is 35 units narrower with them.
constexpr REAL kCompactShift = 35.0f;

// How far the time block's right edge is pulled on again, towards the
// visualizer. The visualizer does not move: the time is what travels, and the
// progress bar's right edge travels with it (the time's right edge is the bar's
// own right edge, see kTimeRight), so the bar and the panel are this much
// shorter than kCompactShift alone leaves them.
//
// It was 40 from 2.8.2 to 2.8.7, which left the time 69.5 units of room. Since
// 2.8.8 the number is derived, at the user's request, from the room the widest
// time needs - "00:00:00 / 00:00:00" - with a small gap after the strip:
//
//    198     the strip's right edge (kMeterLeft + 17 * kMeterPitch + kMeterStroke)
//  +   6     clean panel between the strip and the widest time
//  + 101.5   the widest time: two eight character parts (44.8 units each as
//            "00:00:00", 45.4 at the very widest, "44:44:44"), the slash (3.7)
//            and two kTimeGap; measured with MeasureTextLine in Inter Medium 10
//  = 305.5   kTimeRight, which is the design's 342.5 less kCompactShift less 2
constexpr REAL kTimeShift = 2.0f;

// How far the three buttons are pulled towards the time, at the user's request
// (2.8.4). They hang off the right edge, so nothing but the panel's width has to
// change: the panel is 10 units narrower, everything measured from the left stays
// where it was, and the buttons end up 10 units closer to the time. The gap
// between the time's right edge and the previous button, which was 24.5 units,
// is 14.5 now.
constexpr REAL kButtonShift = 10.0f;

// Panel - the frame fill of both frames: #1a1a1a with an 8 unit corner radius.
//
// Every coordinate below is still written in the design's own space, which is
// 466 x 42 (frame HLQsH; frame qBuu4 is 465 units wide); the constants that sit
// to the right of the text carry the shifts above. The bar's left edge and the
// meter carry only kCompactShift, so kTimeShift comes off the bar's length and
// the gap between the meter and the time.
//
// The window itself is placed by its *right* edge (see PositionWidget), so on
// screen the two sides swap roles: what carries a shift keeps the screen
// position it had and what does not is what travels. A longer bar therefore
// shows on the taskbar as the left part - the cover, the two text lines, the
// meter and the bar's left end - moving left, while the time and the buttons
// stay where the tray is.
constexpr REAL kPanelWidth = 466.0f - kCompactShift - kTimeShift - kButtonShift;   // 419
constexpr REAL kPanelHeight = 42.0f;
constexpr REAL kPanelRadius = 8.0f;
constexpr BYTE kPanelR = 0x1a, kPanelG = 0x1a, kPanelB = 0x1a;

// Cover - "Frame 413": 32 x 32 at (5.5, 5), corner radius 5, clipped. The left
// edge is 1 px further in than the design's own 4.5, at the user's request.
constexpr REAL kArtLeft = 5.5f;
constexpr REAL kArtTop = 5.0f;
constexpr REAL kArtSize = 32.0f;
constexpr REAL kArtRadius = 5.0f;

// Track text - Inter 12 / 500 (title) and Inter 11 / 500 (artist) at x = 44.5.
// The design gives each text a box 15 and 13 units tall, which is the font's own
// line box; the glyph baseline therefore sits at half leading + ascent below the
// top of that box. Inter's ascent is 0.96875 em and its descent 0.24121 em, which
// puts the title's baseline at 17.87 and the artist's at 32.50 - and the canvas'
// own render puts both one unit lower than that, so the drawing is calibrated
// against the exported design rather than against the arithmetic.
//
// The artist keeps the design's 11 unit size but is one weight step heavier than
// the design's 400, by request, so it stays on the design's own baseline (33.50).
// The time is the one that is smaller - 10 units - and that is why its baseline
// is the lower 33.33: the smaller em returns half a unit of leading to its 13
// unit box, and following the arithmetic above (33.74) put the descender of
// "1:15 / 4:00" one row lower than the export does. 33.33 keeps the ink in the
// rows the export uses, y = 25..33.
constexpr REAL kTextLeft = 44.5f;
constexpr REAL kTitleBaseline = 18.87f;
constexpr REAL kTitleSize = 12.0f;
constexpr REAL kArtistBaseline = 33.50f;
constexpr REAL kArtistSize = 11.0f;
// Text stops this far before the bar, and the two parts of that distance are
// deliberate:
//
//  * 8 units are the design's own (its text boxes end on 197.5 and its bar
//    starts on 205.5), and they moved with the bar when the panel narrowed, so
//    the text is never overrun by the bar;
//  * 5 more were given up at the user's request: the gap between the text block
//    and the middle of the widget looked too tight, and the middle stays where it
//    is, so the 5 units come off the title and the artist and go into the gap
//    (8 -> 13). The price is that a long title loses its last letter a little
//    earlier - on the track that prompted this, "Шёлковая простынь" was only just
//    clipped before, and is clipped by a bit more now. That is the trade the
//    request asks for, so it is not compensated anywhere else.
constexpr REAL kTextInset = 5.0f;
constexpr REAL kTextRight = 197.5f - kCompactShift - kTextInset;   // 157.5

// Progress bar - "Rectangle 81" (track, #ffffff30) and "Rectangle 82" (fill,
// #ffffff): 137 x 4 at (205.5, 12), corner radius clamped from 10 to half the
// height. Its place moved with kCompactShift, to 170.5, and its length is the
// design's own less kTimeShift: the right edge is the time's own right edge, so
// the bar is as long as the time's room makes it, and its left edge stays where
// the design's own 8 unit gap before it does not let it move.
constexpr REAL kBarLeft = 205.5f - kCompactShift;
constexpr REAL kBarTop = 12.0f;
constexpr REAL kBarWidth = 137.0f - kTimeShift;   // 135
constexpr REAL kBarHeight = 4.0f;
constexpr BYTE kTrackAlpha = 0x30;

// Time - the design reads "1:15 / 4:00" in Inter 11 at #939393, ending exactly
// on the right edge of the bar. It is drawn one unit smaller and one weight step
// heavier than the design, which is where the 33.33 above comes from; the artist
// next to it stayed at the design's size. Its right edge moves with both shifts,
// and kTimeShift is chosen so that the room between the strip and this edge
// holds the widest time with 6 units to spare (see kTimeShift).
constexpr REAL kTimeRight = 342.5f - kCompactShift - kTimeShift;   // 305.5
constexpr REAL kTimeBaseline = 33.33f;
constexpr REAL kTimeSize = 10.0f;
// "1:15", "/" and "4:00" are three nodes in the design with 4 units between
// them; the widget keeps 3, at the user's request (2.8.8) - the slash read as
// standing too far from the digits.
constexpr REAL kTimeGap = 3.0f;

// Stereo visualizer - 24 one unit wide strokes growing upwards from y = 34 and
// painted in #6d6d6d, on the half pixels the design uses.
//
// This is the one place the widget deliberately departs from design.pen, at the
// user's request: the design spreads its 24 strokes over 69.5 units, which is
// half the panel, and the strip is drawn at half that length instead - a 1.5
// unit step rather than 3. The heights are no longer the design's: every stroke
// now follows the music, see ScopeBands. Its place carries kCompactShift only:
// in 2.8.2 the time is what moved towards the strip (kTimeShift) and the strip
// stayed, which is what "towards the visualizer" means.
//
// In 2.8.3 the strip lost six of its strokes, at the user's request, and with
// them 9 units of length: the time block, which had come that close, needs the
// room for the two extra digits a track past ten minutes has. 18 strokes on the
// same 1.5 unit step end on 197, so 26.5 units instead of 35.5, and the room
// left of the time grew from 60.5 units to 69.5 - enough for "61:53 / 65:00"
// (64 units of ink; since 2.8.8 the room is 107.5, see kTimeShift). Six in
// place of eight strokes per band is the one obvious
// alternative and it is not taken: the strokes are 1 unit wide on a 1.5 unit
// step, so the step cannot shrink any further without the strokes touching.
constexpr int kMeterCount = 18;
constexpr REAL kMeterLeft = 206.5f - kCompactShift;   // 171.5
constexpr REAL kMeterPitch = 1.5f;
constexpr REAL kMeterStroke = 1.0f;
constexpr REAL kMeterBottom = 34.0f;
// The halves are the two channels: strokes 0..8 are the left one, 9..17 the
// right one, and within a half the band rises towards the middle of the widget -
// bass at the ends, the highs in the middle of each half. That is the usual
// stereo spectrum layout, and it reads as one picture rather than as two
// disconnected meters.
constexpr int kMeterHalf = kMeterCount / 2;
// A stroke is never shorter than this, so the strip keeps the look of a row of
// bars even in total silence.
constexpr REAL kMeterMinHeight = 2.0f;
// The tallest a stroke may be. The design's own 21 units let the tips reach the
// progress bar - the bar occupies y = 12..16, and 21 units above a bottom of 34
// lands on y = 13 - while the request was an explicit gap, so the maximum stops
// at 14 and leaves 4 units of clean panel under the bar at all times.
constexpr REAL kMeterMaxHeight = 14.0f;
// The design's own heights, drawn when the visualizer is switched off. The
// motif is the design's, read as the nine strokes of one channel - 5, 7, 5, 7,
// 10 coming in from the edge of the widget and mirrored back out - so the strip
// keeps the shape the design gives it even though it is six strokes shorter.
constexpr int kMeterLevels[kMeterCount] = {
    5, 7, 5, 7, 10, 7, 5, 7, 5,
    5, 7, 5, 7, 10, 7, 5, 7, 5,
};

// Buttons - "Frame 416", "Frame 417" and "Frame 418": 28 x 28, vertically centred
// in the widget (see ComputeLayout), corner
// radius 5, fill #ffffff17, hung off the right edge. At the design's own width
// they land on x = 367, 398.5 and 431, so they are 99, 67.5 and 35 units from
// the right edge. Being offsets from that edge, they do not move when the panel
// narrows or widens: at 419 units they land on x = 320, 351.5 and 384, keeping the design's
// own 7 units between the last button and the right edge. On screen that is the
// position they have always been in - the widget is placed by its right edge.
constexpr REAL kButtonSize = 28.0f;
constexpr REAL kButtonRadius = 5.0f;
constexpr REAL kButtonRight = 7.0f;
constexpr REAL kPrevButtonOffset = 99.0f;
constexpr REAL kPlayButtonOffset = 67.5f;
constexpr REAL kNextButtonOffset = 35.0f;
constexpr BYTE kButtonAlpha = 0x17;
// Hover is not in the design; a hovered button only brightens.
constexpr BYTE kButtonHoverAlpha = 0x26;

// Icons - vuesax/linear paths drawn as 1.5 unit strokes with round caps and
// joins, exactly like the design. Each glyph carries the node box the design
// gives it, relative to the button it sits in.
//
// The paths are the ones in the icons/ folder, verbatim: the widget's own small
// SVG reader accepts exactly the commands they use (M, V, H, L, C, Z), so no
// conversion step sits between the artwork and the drawing code. The icons are
// authored on an 18 x 18 grid; the node boxes below carry the size and the
// position the design gives each glyph inside its 28 x 28 button, and the path
// is fitted to its box, so the grid size itself does not matter.
constexpr REAL kGlyphStroke = 1.5f;
constexpr BYTE kGlyphDim = 153;    // 0.6, the opacity of the skip icons
constexpr BYTE kGlyphSolid = 255;  // play and pause are not dimmed

// back.svg: the bar on the left, then the triangle pointing left.
constexpr char kBackBar[] = "M2.82031 13.6372V4.36719";
constexpr char kBackTriangle[] =
    "M15.1807 5.41663V12.5941C15.1807 14.0641 13.5832 14.9866 12.3082 14.2516"
    "L9.19572 12.4591L6.0832 10.6591C4.8082 9.92412 4.8082 8.08662 6.0832 7.3"
    "5162L9.19572 5.55162L12.3082 3.75913C13.5832 3.02413 15.1807 3.93913 15."
    "1807 5.41663Z";
// next.svg: the triangle pointing right, then the bar on the right.
constexpr char kNextTriangle[] =
    "M2.82031 5.41663V12.5941C2.82031 14.0641 4.4178 14.9866 5.6928 14.2516L8"
    ".8053 12.4591L11.9178 10.6591C13.1928 9.92412 13.1928 8.08662 11.9178 7."
    "35162L8.8053 5.55162L5.6928 3.75913C4.4178 3.02413 2.82031 3.93913 2.820"
    "31 5.41663Z";
constexpr char kNextBar[] = "M15.1807 13.6372V4.36719";
// play.svg: the triangle pointing right.
constexpr char kPlayTriangle[] =
    "M4.41597 5.41272V12.5902C4.41597 14.0602 6.01346 14.9827 7.28846 14.2477"
    "L10.401 12.4552L13.5135 10.6552C14.7885 9.92021 14.7885 8.08271 13.5135 "
    "7.34771L10.401 5.54771L7.28846 3.75523C6.01346 3.02023 4.41597 3.93522 4"
    ".41597 5.41272Z";
// pause.svg: two rounded bars.
constexpr char kPauseBarLeft[] =
    "M8 13.74V4.26C8 3.36 7.62745 3 6.68627 3H4.31373C3.37255 3 3 3.36 3 4.26"
    "V13.74C3 14.64 3.37255 15 4.31373 15H6.68627C7.62745 15 8 14.64 8 13.74Z";
constexpr char kPauseBarRight[] =
    "M15 13.74V4.26C15 3.36 14.6275 3 13.6863 3H11.3137C10.3791 3 10 3.36 10 "
    "4.26V13.74C10 14.64 10.3725 15 11.3137 15H13.6863C14.6275 15 15 14.64 15"
    " 13.74Z";

struct Glyph {
    const char* path;
    REAL left, top, width, height;  // node box, relative to the button
};

// Frame 416: bar on the left, triangle pointing left - back.svg, placed as the
// mirror image of frame 418 about the middle of the button (x = 14), so the bar
// stands 2.2 units to the left of the triangle just as next.svg's stands to the
// right of its own.
constexpr Glyph kPrevGlyphs[] = {
    {kBackBar, 7.81f, 9.37f, 0.0f, 9.27f},
    {kBackTriangle, 10.0062f, 8.47f, 10.0538f, 11.5925f},
};
// Frame 417 while the track is playing: two rounded bars, undimmed. A bar is the
// icon's own 5 x 12 block; the 7 units the artwork puts between the two bar
// centres are kept, so they straddle the button's middle by 3.5 units each - the
// same 0.93 unit gap the design's own 5 wide bars have.
constexpr Glyph kPauseGlyphs[] = {
    {kPauseBarLeft, 9.074f, 8.0f, 5.0f, 12.0f},
    {kPauseBarRight, 13.926f, 8.0f, 5.0f, 12.0f},
};
// Frame 417 while the track is paused: the triangle points right, and sits where
// frame qBuu4 puts it.
constexpr Glyph kPlayGlyphs[] = {
    {kPlayTriangle, 9.40f, 8.20f, 10.3725f, 11.9625f},
};
// Frame 418: triangle pointing right, bar on the right. The skip triangles and
// the bars land on exactly the same pixels as frame 416's, mirrored about the
// middle of the play button.
constexpr Glyph kNextGlyphs[] = {
    {kNextTriangle, 7.94f, 8.47f, 10.0538f, 11.5925f},
    {kNextBar, 20.19f, 9.37f, 0.0f, 9.27f},
};

}  // namespace design

// One design unit in device pixels.
Gdiplus::REAL Scaled(Gdiplus::REAL units, int dpi) {
    return units * (Gdiplus::REAL)dpi / 96.0f;
}

Gdiplus::RectF DesignRect(Gdiplus::REAL left, Gdiplus::REAL top,
                          Gdiplus::REAL width, Gdiplus::REAL height, int dpi) {
    return Gdiplus::RectF(Scaled(left, dpi), Scaled(top, dpi),
                          Scaled(width, dpi), Scaled(height, dpi));
}

// Everything the mouse can land on, in device pixels.
struct Layout {
    Gdiplus::RectF artArea;
    Gdiplus::RectF progressArea;
    Gdiplus::RectF prevButton;
    Gdiplus::RectF playButton;
    Gdiplus::RectF nextButton;
};

Layout ComputeLayout(int width, int height, int dpi) {
    Layout l{};
    l.artArea = DesignRect(design::kArtLeft, design::kArtTop, design::kArtSize,
                           design::kArtSize, dpi);
    l.progressArea = DesignRect(design::kBarLeft, design::kBarTop,
                                design::kBarWidth, design::kBarHeight, dpi);

    // The buttons sit in the middle of the height, so the gap above and below
    // them is the same whatever height the widget is set to. (The design's own
    // top of 8 units left 6 below it at its 42-unit height.)
    Gdiplus::REAL button = Scaled(design::kButtonSize, dpi);
    Gdiplus::REAL top = ((Gdiplus::REAL)height - button) / 2.0f;
    auto place = [&](Gdiplus::REAL offsetFromRight) {
        return Gdiplus::RectF((Gdiplus::REAL)width - Scaled(offsetFromRight, dpi),
                              top, button, button);
    };
    l.prevButton = place(design::kPrevButtonOffset);
    l.playButton = place(design::kPlayButtonOffset);
    l.nextButton = place(design::kNextButtonOffset);
    return l;
}

enum class HoverTarget { None, Prev, Play, Next, Progress };
HoverTarget g_hover = HoverTarget::None;
bool g_trackingMouse = false;

bool PointIn(POINT const& p, Gdiplus::RectF const& r) {
    Gdiplus::REAL x = (Gdiplus::REAL)p.x;
    Gdiplus::REAL y = (Gdiplus::REAL)p.y;
    return x >= r.X && x < r.X + r.Width && y >= r.Y && y < r.Y + r.Height;
}

// Returns which control a client point falls on.
HoverTarget HitTest(Layout const& layout, POINT const& p) {
    if (PointIn(p, layout.prevButton)) return HoverTarget::Prev;
    if (PointIn(p, layout.playButton)) return HoverTarget::Play;
    if (PointIn(p, layout.nextButton)) return HoverTarget::Next;
    if (PointIn(p, layout.progressArea)) return HoverTarget::Progress;
    return HoverTarget::None;
}

std::wstring FormatTime(long long ms) {
    if (ms < 0) {
        ms = 0;
    }
    long long totalSeconds = ms / 1000;
    wchar_t buf[32];
    // Past an hour the minutes would keep growing ("61:53"); with the hours
    // split off, a part is never longer than the eight characters of
    // "00:00:00" the layout keeps room for (see design::kTimeShift).
    if (totalSeconds >= 3600) {
        swprintf_s(buf, L"%lld:%02lld:%02lld", totalSeconds / 3600,
                   (totalSeconds / 60) % 60, totalSeconds % 60);
    } else {
        swprintf_s(buf, L"%lld:%02lld", totalSeconds / 60, totalSeconds % 60);
    }
    return buf;
}

// ---------------------------------------------------------------------------
// Fonts
//
// The design is set in Inter, which Windows does not ship. The two weights it
// uses are loaded from the mod's own directory when they are there - the install
// script copies them next to the library - and a system family is used
// otherwise, so the widget always paints something.
// ---------------------------------------------------------------------------

// Any address inside this module; used to find out where the module lives.
const int kModuleAnchor = 0;

std::wstring LibraryDirectory() {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&kModuleAnchor), &module)) {
        return L"";
    }
    wchar_t path[MAX_PATH]{};
    if (!GetModuleFileNameW(module, path, MAX_PATH)) {
        return L"";
    }
    std::wstring file(path);
    size_t slash = file.find_last_of(L'\\');
    return slash == std::wstring::npos ? std::wstring() : file.substr(0, slash);
}

// Owns the private fonts for the lifetime of the mod. GDI+ hands out families
// that belong to the collection, so the whole array is kept alive rather than
// copied, and never freed - the widget lives as long as Explorer does.
struct FontSet {
    Gdiplus::PrivateFontCollection privateFonts;
    Gdiplus::FontFamily* families = nullptr;
    Gdiplus::FontFamily* regular = nullptr;
    Gdiplus::FontFamily* medium = nullptr;
    bool inter = false;
};

FontSet& Fonts() {
    static FontSet set = [] {
        FontSet s;
        std::wstring dir = LibraryDirectory();
        bool loaded = false;
        if (!dir.empty()) {
            std::wstring regularFile = dir + L"\\Inter-Regular.ttf";
            std::wstring mediumFile = dir + L"\\Inter-Medium.ttf";
            if (GetFileAttributesW(regularFile.c_str()) != INVALID_FILE_ATTRIBUTES &&
                GetFileAttributesW(mediumFile.c_str()) != INVALID_FILE_ATTRIBUTES) {
                loaded = s.privateFonts.AddFontFile(regularFile.c_str()) ==
                             Gdiplus::Ok &&
                         s.privateFonts.AddFontFile(mediumFile.c_str()) ==
                             Gdiplus::Ok;
            }
        }

        if (loaded) {
            INT count = s.privateFonts.GetFamilyCount();
            if (count > 0) {
                s.families = new Gdiplus::FontFamily[count];
                INT found = 0;
                if (s.privateFonts.GetFamilies(count, s.families, &found) ==
                    Gdiplus::Ok) {
                    for (INT i = 0; i < found; ++i) {
                        WCHAR name[LF_FACESIZE]{};
                        if (s.families[i].GetFamilyName(name) != Gdiplus::Ok) {
                            continue;
                        }
                        std::wstring familyName(name);
                        // Inter's static files register one family per weight, so
                        // the medium weight arrives as its own family.
                        if (familyName == L"Inter") {
                            s.regular = &s.families[i];
                        } else if (familyName == L"Inter Medium") {
                            s.medium = &s.families[i];
                        }
                    }
                }
            }
            s.inter = s.regular != nullptr;
        }

        if (!s.regular) {
            // No Inter next to the library. Fall back to a family that is always
            // there, preferring the one Windows 11's own shell uses.
            Gdiplus::FontFamily probe(L"Segoe UI Variable Text");
            const wchar_t* fallback =
                probe.IsAvailable() ? L"Segoe UI Variable Text" : L"Segoe UI";
            s.regular = new Gdiplus::FontFamily(fallback);
        }
        if (!s.medium) {
            s.medium = s.regular;
        }
        return s;
    }();
    return set;
}

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------

void AddRoundRect(Gdiplus::GraphicsPath& path, Gdiplus::RectF const& r,
                  Gdiplus::REAL radius) {    Gdiplus::REAL d = radius * 2.0f;
    if (d > r.Width) d = r.Width;
    if (d > r.Height) d = r.Height;
    path.AddArc(Gdiplus::RectF(r.X, r.Y, d, d), 180.0f, 90.0f);
    path.AddArc(Gdiplus::RectF(r.X + r.Width - d, r.Y, d, d), 270.0f, 90.0f);
    path.AddArc(Gdiplus::RectF(r.X + r.Width - d, r.Y + r.Height - d, d, d), 0.0f,
                90.0f);
    path.AddArc(Gdiplus::RectF(r.X, r.Y + r.Height - d, d, d), 90.0f, 90.0f);
    path.CloseFigure();
}

// A theme brush resolved for one area: the themes' gradients run top to bottom,
// which is GDI+'s 90 degrees.
struct ThemeBrush {
    Gdiplus::LinearGradientBrush gradient;
    bool isGradient = false;

    ThemeBrush(styler::Brush const& source, Gdiplus::RectF const& area)
        : gradient(area, Gdiplus::Color(255, 0, 0, 0), Gdiplus::Color(255, 0, 0, 0),
                   90.0f) {
        int count = source.stopCount > 4 ? 4 : source.stopCount;
        if (count < 2) {
            return;
        }
        Gdiplus::Color colors[4];
        Gdiplus::REAL positions[4];
        for (int i = 0; i < count; ++i) {
            colors[i] = Gdiplus::Color(source.stops[i].a, source.stops[i].r,
                                       source.stops[i].g, source.stops[i].b);
            positions[i] = source.stops[i].offset;
        }
        isGradient = gradient.SetInterpolationColors(colors, positions, count) ==
                     Gdiplus::Ok;
    }
};

void FillWithThemeBrush(Gdiplus::Graphics& g, Gdiplus::GraphicsPath const& path,
                        styler::Brush const& brush, Gdiplus::RectF const& area) {
    if (!brush.present || brush.a == 0) {
        return;
    }
    if (!brush.gradient || brush.stopCount < 2) {
        Gdiplus::SolidBrush fill(Gdiplus::Color(brush.a, brush.r, brush.g, brush.b));
        g.FillPath(&fill, &path);
        return;
    }
    ThemeBrush theme(brush, area);
    if (theme.isGradient) {
        g.FillPath(&theme.gradient, &path);
    }
}

void StrokeWithThemeBrush(Gdiplus::Graphics& g, Gdiplus::GraphicsPath const& path,
                          styler::Brush const& brush, Gdiplus::RectF const& area,
                          Gdiplus::REAL thickness) {
    if (!brush.present || brush.a == 0 || thickness <= 0.0f) {
        return;
    }
    if (!brush.gradient || brush.stopCount < 2) {
        Gdiplus::SolidBrush colour(Gdiplus::Color(brush.a, brush.r, brush.g, brush.b));
        Gdiplus::Pen pen(&colour, thickness);
        g.DrawPath(&pen, &path);
        return;
    }
    ThemeBrush theme(brush, area);
    if (theme.isGradient) {
        Gdiplus::Pen pen(&theme.gradient, thickness);
        g.DrawPath(&pen, &path);
    }
}

// The design's palette, and the same palette inverted for a light taskbar theme.
struct Palette {
    Gdiplus::Color title;
    Gdiplus::Color dim;
    Gdiplus::Color track;
    Gdiplus::Color fill;
    Gdiplus::Color buttonFill;
    Gdiplus::Color buttonHover;
    Gdiplus::Color glyph;
    Gdiplus::Color glyphDim;
    Gdiplus::Color meter;
    // The same grey at 55 % towards white, used for the strokes that are near
    // the top of their travel: a spectrum that only changes height is hard to
    // read on a 42 pixel panel, and the brighter tip makes the movement obvious.
    Gdiplus::Color meterPeak;
};

Palette MakePalette(bool light) {
    Palette p;
    if (!light) {
        p.title = Gdiplus::Color(255, 255, 255, 255);
        p.dim = Gdiplus::Color(255, 0x93, 0x93, 0x93);
        p.track = Gdiplus::Color(design::kTrackAlpha, 255, 255, 255);
        p.fill = Gdiplus::Color(255, 255, 255, 255);
        p.buttonFill = Gdiplus::Color(design::kButtonAlpha, 255, 255, 255);
        p.buttonHover = Gdiplus::Color(design::kButtonHoverAlpha, 255, 255, 255);
        p.glyph = Gdiplus::Color(design::kGlyphSolid, 255, 255, 255);
        p.glyphDim = Gdiplus::Color(design::kGlyphDim, 255, 255, 255);
        p.meter = Gdiplus::Color(255, 0x6d, 0x6d, 0x6d);
        p.meterPeak = Gdiplus::Color(255, 0xb0, 0xb0, 0xb0);
    } else {
        p.title = Gdiplus::Color(255, 20, 20, 20);
        p.dim = Gdiplus::Color(255, 96, 96, 96);
        p.track = Gdiplus::Color(design::kTrackAlpha, 0, 0, 0);
        p.fill = Gdiplus::Color(255, 20, 20, 20);
        p.buttonFill = Gdiplus::Color(design::kButtonAlpha, 0, 0, 0);
        p.buttonHover = Gdiplus::Color(design::kButtonHoverAlpha, 0, 0, 0);
        p.glyph = Gdiplus::Color(design::kGlyphSolid, 20, 20, 20);
        p.glyphDim = Gdiplus::Color(design::kGlyphDim, 20, 20, 20);
        p.meter = Gdiplus::Color(255, 0x6d, 0x6d, 0x6d);
        p.meterPeak = Gdiplus::Color(255, 0xb0, 0xb0, 0xb0);
    }
    return p;
}

// Measures one line the same way DrawTextLine draws it, so that a right aligned
// line ends where the design's own line ends.
Gdiplus::REAL MeasureTextLine(Gdiplus::Graphics& g, std::wstring const& text,
                              Gdiplus::Font* font) {
    if (text.empty()) {
        return 0.0f;
    }
    const INT flags = Gdiplus::DriverStringOptionsCmapLookup |
                      Gdiplus::DriverStringOptionsRealizedAdvance;
    Gdiplus::PointF origin(0.0f, 0.0f);
    Gdiplus::RectF measured;
    if (g.MeasureDriverString(reinterpret_cast<const UINT16*>(text.c_str()),
                              (INT)text.size(), font, &origin, flags, nullptr,
                              &measured) != Gdiplus::Ok) {
        return 0.0f;
    }
    return measured.Width;
}

// Draws one line at an explicit baseline origin, clipped to the room it has.
void DrawTextAt(Gdiplus::Graphics& g, std::wstring const& text,
                Gdiplus::PointF origin, Gdiplus::Font* font,
                Gdiplus::Color const& colour, Gdiplus::REAL clipLeft,
                Gdiplus::REAL clipRight, Gdiplus::REAL size) {
    if (text.empty()) {
        return;
    }
    const INT flags = Gdiplus::DriverStringOptionsCmapLookup |
                      Gdiplus::DriverStringOptionsRealizedAdvance;
    Gdiplus::SolidBrush brush(colour);
    Gdiplus::GraphicsState saved = g.Save();
    g.SetClip(Gdiplus::RectF(clipLeft, origin.Y - size * 2.0f, clipRight - clipLeft,
                             size * 4.0f));
    g.DrawDriverString(reinterpret_cast<const UINT16*>(text.c_str()),
                       (INT)text.size(), font, &brush, &origin, flags, nullptr);
    g.Restore(saved);
}

// Draws one line of text with its baseline exactly on `baseline`, which is how
// the design positions text: the canvas lays the font's own line box out in a box
// of a known height and the glyphs sit on the baseline inside it.
//
// The glyphs are drawn with DrawDriverString and a realized advance rather than
// with DrawString, because DrawString snaps every advance to a whole pixel: the
// design's own title came out 8 units too wide that way, and with the font's real
// advances it matches the canvas to the pixel.
void DrawTextLine(Gdiplus::Graphics& g, std::wstring const& text,
                  Gdiplus::REAL left, Gdiplus::REAL right, Gdiplus::REAL baseline,
                  Gdiplus::REAL size, Gdiplus::FontFamily* family,
                  Gdiplus::Color const& colour, bool rightAlign) {
    if (text.empty() || right <= left || !family) {
        return;
    }

    Gdiplus::Font font(family, size, Gdiplus::FontStyleRegular,
                       Gdiplus::UnitPixel);
    if (font.GetLastStatus() != Gdiplus::Ok) {
        return;
    }

    Gdiplus::REAL x = left;
    if (rightAlign) {
        x = right - MeasureTextLine(g, text, &font);
    }
    DrawTextAt(g, text, Gdiplus::PointF(x, baseline), &font, colour, left, right,
               size);
}

// The subset of the SVG path syntax the design's icons use: M/m, L/l, H/h, V/v,
// C/c and Z/z. A full SVG engine is not needed for four glyphs, but the curves
// have to be real curves - the rounded triangle is the whole shape.
class SvgPathReader {
   public:
    explicit SvgPathReader(char const* text) : text_(text) {}

    bool Build(Gdiplus::GraphicsPath& path) {
        Gdiplus::PointF current(0.0f, 0.0f);
        Gdiplus::PointF figureStart(0.0f, 0.0f);
        while (true) {
            if (!SkipSpace()) {
                return path.GetPointCount() > 0;
            }
            char command = *text_;
            if (!IsCommand(command)) {
                return false;
            }
            text_++;

            if (command == 'Z' || command == 'z') {
                path.CloseFigure();
                current = figureStart;
                continue;
            }

            if (command == 'M' || command == 'm') {
                Gdiplus::REAL ex = 0, ey = 0;
                if (!Number(&ex) || !Number(&ey)) {
                    return false;
                }
                current = Gdiplus::PointF(
                    command == 'm' ? current.X + ex : ex,
                    command == 'm' ? current.Y + ey : ey);
                if (path.GetPointCount() > 0) {
                    path.StartFigure();
                }
                // The first segment of a figure starts here; a path that begins
                // with a single point still needs that point to exist.
                path.AddLine(current, current);
                figureStart = current;
                continue;
            }

            // Everything else repeats while numbers keep coming.
            while (true) {
                if (!StartsNumber()) {
                    break;
                }
                if (command == 'L' || command == 'l' || command == 'H' ||
                    command == 'h' || command == 'V' || command == 'v') {
                    Gdiplus::PointF from = current;
                    Gdiplus::REAL ex = 0, ey = 0;
                    if (command == 'H' || command == 'h') {
                        if (!Number(&ex)) return false;
                        current.X = command == 'h' ? current.X + ex : ex;
                    } else if (command == 'V' || command == 'v') {
                        if (!Number(&ey)) return false;
                        current.Y = command == 'v' ? current.Y + ey : ey;
                    } else {
                        if (!Number(&ex) || !Number(&ey)) return false;
                        current = Gdiplus::PointF(
                            command == 'l' ? current.X + ex : ex,
                            command == 'l' ? current.Y + ey : ey);
                    }
                    path.AddLine(from, current);
                    continue;
                }
                if (command == 'C' || command == 'c') {
                    Gdiplus::REAL cx1 = 0, cy1 = 0, cx2 = 0, cy2 = 0, ex = 0, ey = 0;
                    if (!Number(&cx1) || !Number(&cy1) || !Number(&cx2) ||
                        !Number(&cy2) || !Number(&ex) || !Number(&ey)) {
                        return false;
                    }
                    bool relative = command == 'c';
                    Gdiplus::PointF from = current;
                    Gdiplus::PointF c1(relative ? current.X + cx1 : cx1,
                                       relative ? current.Y + cy1 : cy1);
                    Gdiplus::PointF c2(relative ? current.X + cx2 : cx2,
                                       relative ? current.Y + cy2 : cy2);
                    current = Gdiplus::PointF(relative ? current.X + ex : ex,
                                              relative ? current.Y + ey : ey);
                    path.AddBezier(from, c1, c2, current);
                    continue;
                }
                return false;
            }
        }
    }

   private:
    static bool IsCommand(char c) {
        return c == 'M' || c == 'm' || c == 'L' || c == 'l' || c == 'H' ||
               c == 'h' || c == 'V' || c == 'v' || c == 'C' || c == 'c' ||
               c == 'Z' || c == 'z';
    }

    // Returns false at the end of the string.
    bool SkipSpace() {
        while (*text_ == ' ' || *text_ == ',' || *text_ == '\t' ||
               *text_ == '\n' || *text_ == '\r') {
            text_++;
        }
        return *text_ != '\0';
    }

    static bool IsNumberStart(char c) {
        return (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.';
    }

    bool StartsNumber() {
        SkipSpace();
        return IsNumberStart(*text_);
    }

    bool Number(Gdiplus::REAL* out) {
        if (!SkipSpace() || !IsNumberStart(*text_)) {
            return false;
        }
        char* end = nullptr;
        double value = strtod(text_, &end);
        if (end == text_) {
            return false;
        }
        text_ = end;
        *out = (Gdiplus::REAL)value;
        return true;
    }

    char const* text_;
};

// Maps one glyph's path onto the node box the design gives it, in device pixels.
bool BuildGlyph(Gdiplus::GraphicsPath& path, design::Glyph const& glyph,
                Gdiplus::REAL originX, Gdiplus::REAL originY, int dpi) {
    SvgPathReader reader(glyph.path);
    if (!reader.Build(path)) {
        return false;
    }

    // The path's own bounding box, measured on the flattened curve so that the
    // control points outside the curve do not widen it.
    Gdiplus::GraphicsPath flattened;
    if (flattened.AddPath(&path, FALSE) != Gdiplus::Ok) {
        return false;
    }
    flattened.Flatten(nullptr, 0.05f);
    Gdiplus::RectF bounds;
    if (flattened.GetBounds(&bounds) != Gdiplus::Ok) {
        return false;
    }

    Gdiplus::REAL targetX = originX + Scaled(glyph.left, dpi);
    Gdiplus::REAL targetY = originY + Scaled(glyph.top, dpi);
    Gdiplus::REAL targetW = Scaled(glyph.width, dpi);
    Gdiplus::REAL targetH = Scaled(glyph.height, dpi);

    Gdiplus::REAL sx = bounds.Width > 0.001f ? targetW / bounds.Width : 0.0f;
    Gdiplus::REAL sy = bounds.Height > 0.001f ? targetH / bounds.Height : 0.0f;
    Gdiplus::Matrix transform(sx, 0.0f, 0.0f, sy, targetX - bounds.X * sx,
                              targetY - bounds.Y * sy);
    return path.Transform(&transform) == Gdiplus::Ok;
}

void DrawGlyphs(Gdiplus::Graphics& g, design::Glyph const* glyphs, int count,
                Gdiplus::REAL originX, Gdiplus::REAL originY, int dpi,
                Gdiplus::Color const& colour) {
    Gdiplus::Pen pen(colour, Scaled(design::kGlyphStroke, dpi));
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    pen.SetDashCap(Gdiplus::DashCapRound);
    Gdiplus::SolidBrush brush(colour);
    for (int i = 0; i < count; ++i) {
        Gdiplus::GraphicsPath path;
        if (!BuildGlyph(path, glyphs[i], originX, originY, dpi)) {
            continue;
        }
        // The stroke is widened into an outline and filled instead of being
        // drawn with DrawPath: for a 1.5 unit stroke GDI+ snaps DrawPath to a
        // single hard pixel, while the design keeps the anti-aliased edges the
        // canvas draws. A filled outline has the real coverage.
        path.Widen(&pen, nullptr, 0.05f);
        g.FillPath(&brush, &path);
    }
}

void DrawButton(Gdiplus::Graphics& g, Gdiplus::RectF const& button,
                design::Glyph const* glyphs, int glyphCount, int dpi,
                Gdiplus::Color const& fillColour, Gdiplus::Color const& glyphColour) {
    Gdiplus::GraphicsPath face;
    AddRoundRect(face, button, Scaled(design::kButtonRadius, dpi));
    Gdiplus::SolidBrush fill(fillColour);
    g.FillPath(&fill, &face);

    DrawGlyphs(g, glyphs, glyphCount, button.X, button.Y, dpi, glyphColour);
}

// ---------------------------------------------------------------------------
// The widget itself
//
// The animation of the visualizer
//
// The capture thread publishes what the audio *is*; this turns that into what
// the bars *look like*. The two are separate on purpose: the analyser runs at
// the audio block rate, while the bars are drawn at the frame rate, and a bar
// has to rise quickly and fall slowly whatever the rate is - hence the two
// release constants, applied per frame against the real elapsed time.
//
// The levels are the only state the drawing code keeps between frames, and it
// lives on the thread that owns the window, so no lock is needed for it.
// ---------------------------------------------------------------------------

// Root mean square that counts as silence, and the value that fills a bar.
//
// The second number is calibrated by measurement, not by taste.
// `preview/band-check.cpp` plays a tone per band into the default device, reads
// it back through this very analyser and prints the ratio between what was
// played and what came back. Two findings from running it:
//
//  * the loopback does not hand back the levels the player produced. On this
//    machine a 60 Hz tone leaves the capture about 22 times weaker than it went
//    into the mix - 27 dB, which is the device's own processing plus the shared
//    mode mixer. Without a correction the bars would barely leave the floor;
//  * the bass ratio is the stable one (22.0, 22.1, 22.2 over three runs, and
//    the same on both channels); the mids and the highs move around with what
//    the device is doing to them. So the scale is anchored on the bass, which
//    is also where most of a track's energy is.
//
// Full scale is then the reading a typical mix leaves behind: a track around
// -18 dBFS rms arrives at roughly 0.008 in the bass band, so 0.012 puts that
// near the top of a bar with a little room above it. `visualizerGain` scales
// the whole thing, so a system whose playback level is far from this one can be
// brought back without a rebuild.
constexpr double kScopeFullRms = 0.012;

// How far below its own reference a band may be and still show anything.
// Twenty-six dB is wider than the natural distance between the bass and the
// highs of real music - about 18 dB on this machine's own measurements - so the
// whole spectrum fits inside the strip with room for a quiet passage, and
// narrow enough that the strip does not become a noise meter.
//
// The scale is in decibels rather than in fractions of full scale on purpose: a
// quiet master and a loud one differ by roughly a constant factor, and a decibel
// window keeps the *shape* of the strip the same for both - the distance
// between the bands does not change with the overall loudness. A linear window
// against a fixed floor cannot do that: a quiet master falls entirely below the
// floor, and every bar comes out at the same height.
constexpr double kScopeRangeDb = 26.0;
// Below this there is nothing to show, which is what keeps the background noise
// of a silent room from twitching the strip.
constexpr double kScopeSilenceRms = kScopeFullRms * 0.03;

// How far the automatic range may lift the scale: a band that has been four
// times louder than the calibrated full scale may keep that as its new full
// bar, and no further. Six dB of headroom is enough to keep a quiet track
// alive; more than that starts to make every track look equally loud, and the
// strip loses the difference between a chorus and a verse.
constexpr double kScopeAutoRangeMax = 4.0;
// How long the reference takes to fall back after the music stops being that
// loud. Long enough to hold the strip lively across a whole quiet passage,
// short enough to follow a track change.
constexpr double kScopeReferenceDecaySeconds = 8.0;

ScopeFrame g_scopeLast;          // the last frame the capture published
unsigned long long g_visualizerFrameTick = 0;
// GetTickCount() of the last block that actually carried a signal. It decides
// how long the strip keeps its fast repaint after the music stops, so the bars
// can fall at the same rate they rose instead of dropping in one step.
DWORD g_lastSignalTick = 0;
UINT g_visualTimerInterval = 0;
// GetTickCount64() of the last whole-surface render. The animation is drawn over
// that surface, so it has to be rebuilt now and then; this is what paces it.
unsigned long long g_lastWholeSurfaceTick = 0;
float g_meterLevels[design::kMeterCount] = {};
// The bar heights the framebuffer currently holds, so a frame that has not
// changed is not pushed again.
float g_framebufferHeights[design::kMeterCount] = {};
bool g_framebufferValid = false;
bool g_meterPrimed = false;

// The band a bar shows, and the channel it belongs to: the left half is the
// left channel, the right half the right one, and within a half the bands run
// from the bass at its ends to the highs in its middle.
void MeterBandFor(int bar, int& channel, int& band) {
    channel = (bar < design::kMeterHalf) ? 0 : 1;
    int local = bar % design::kMeterHalf;
    // Distance from the middle of the half: with nine strokes the distances run
    // 0..4 and back, 0, 1, 2, 3, 4, 3, 2, 1, 0, so the halves mirror each other
    // and the strip is symmetric about its own centre.
    int distance = local < design::kMeterHalf / 2
                       ? local
                       : design::kMeterHalf - 1 - local;
    if (distance < 2) {
        band = 0;  // bass, at the ends
    } else if (distance < 3) {
        band = 1;  // mids
    } else {
        band = 2;  // highs, in the middle of the half
    }
    // Nine strokes per channel therefore land as four on the bass (the two
    // distances 0 and 1, one on each side of the half), two on the mids and
    // three on the highs in the middle. The bass keeps the widest share, as it
    // had with twelve strokes, so the strip still reads as a spectrum.
}

// The same, as the height a bar should reach right now, 0..1.
//
// The bar is drawn on a decibel scale against its band's own reference: full
// when the band is at the reference, nothing at `kScopeRangeDb` below it, and a
// square root shape in between so that the loud end of the range takes up most
// of the travel. The reference is the calibrated full scale, or a louder level
// if the band has been louder recently (see ScopeReferenceFor). `gain` is the
// sensitivity setting as a multiplier.
float ScopeTargetFor(const float rms[2][kScopeBands], int bar, double gain,
                     const double reference[kScopeBands]) {
    int channel = 0;
    int band = 0;
    MeterBandFor(bar, channel, band);

    double value = rms[channel][band] * gain;
    double full = reference[band] > kScopeFullRms ? reference[band]
                                                  : kScopeFullRms;
    if (value <= 0.0) {
        return 0.0f;
    }
    // Decibels below the reference: 0 at the top of a bar, kScopeRangeDb at the
    // bottom. Nothing in music is exactly zero, so the clamp is on the low side
    // only - a value above the reference simply fills the bar.
    double db = 20.0 * std::log10(full / value);
    double t = 1.0 - db / kScopeRangeDb;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    return (float)std::sqrt(t);
}

// ---------------------------------------------------------------------------
// The automatic range
//
// A fixed full scale is right for a track that is mastered at the level it was
// calibrated for and wrong for everything else: a quiet or compressed track
// leaves the strip nearly flat. So the widget remembers how loud each band has
// been lately and lets *that* mean a full bar.
//
// The follower rises at once - a level that turns up has to be the new
// reference immediately, or the bars would be pinned at the top until it decays
// - and sinks slowly, over about eight seconds. The slow fall is what keeps it
// from reacting to the beat: a kick drum pushes the bass reference up, and the
// bars stay lively for the next few seconds instead of dropping back the moment
// the kick ends. It also means a quiet passage inside a loud track does not
// rescale the strip, and a whole quiet track still fills it.
//
// The floor is what stops the automatic range from chasing its own tail: at
// most a modest boost over the calibrated scale, never amplification of silence.
// ---------------------------------------------------------------------------

double g_scopeReference[kScopeBands] = {};

double ScopeReferenceFor(int band) {
    double value = g_scopeReference[band];
    if (value < kScopeFullRms) {
        value = kScopeFullRms;
    }
    if (value > kScopeFullRms * kScopeAutoRangeMax) {
        value = kScopeFullRms * kScopeAutoRangeMax;
    }
    return value;
}

// Feeds one captured frame into the follower. `playing` freezes the decay when
// the player is paused: the reference describes the track, not the silence in
// between.
void UpdateScopeReference(const ScopeFrame& frame, bool playing, double dt) {
    for (int b = 0; b < kScopeBands; ++b) {
        double loudest = frame.rms[0][b] > frame.rms[1][b] ? frame.rms[0][b]
                                                          : frame.rms[1][b];
        if (loudest > g_scopeReference[b]) {
            // A level that turns up is the new reference at once.
            g_scopeReference[b] = loudest;
        } else if (playing && loudest > kScopeSilenceRms) {
            // Otherwise it sinks slowly, and only while there is something to
            // hear: silence must not make the range creep up over the noise
            // floor and amplify the room.
            g_scopeReference[b] *= 1.0 - dt / kScopeReferenceDecaySeconds;
        }
        if (g_scopeReference[b] < kScopeFullRms) {
            g_scopeReference[b] = kScopeFullRms;
        }
        if (g_scopeReference[b] > kScopeFullRms * kScopeAutoRangeMax) {
            g_scopeReference[b] = kScopeFullRms * kScopeAutoRangeMax;
        }
    }
}

void ResetScopeReference() {
    for (int b = 0; b < kScopeBands; ++b) {
        g_scopeReference[b] = kScopeFullRms;
    }
}

// What the bars show when the machine cannot be captured at all: a signal that
// moves with the clock, so the widget never looks frozen instead of following
// something that is not there. Only used while the player says it is playing,
// and only when the loopback client has never come up.
float SyntheticLevelFor(int bar, double seconds) {
    double fast = std::sin(seconds * (7.0 + 0.61 * bar) + bar * 1.7);
    double slow = std::sin(seconds * (2.3 + 0.13 * bar) + bar * 0.9);
    double value = 0.52 + 0.30 * fast + 0.18 * slow;
    // The middle of the strip (the bass) is the liveliest, as it is in music.
    int channel = 0;
    int band = 0;
    MeterBandFor(bar, channel, band);
    value *= (band == 0) ? 1.0 : (band == 1 ? 0.8 : 0.6);
    if (value < 0.0) value = 0.0;
    if (value > 1.0) value = 1.0;
    return (float)value;
}

struct AnimationFrame {
    // Bar heights in design units, ready to draw.
    float heightUnits[design::kMeterCount] = {};
    // True while the bars still move, which is when the fast repaint is worth
    // its cost. It stays true for the whole fall after the music stops.
    bool moving = false;
};

// Advances the bars to this moment and returns what to draw. Called once per
// widget message, so a frame is never advanced twice.
AnimationFrame AnimationFrameNow(MediaState const& state, Settings const& s) {
    AnimationFrame frame;
    unsigned long long now = GetTickCount64();
    double dt = 0.033;
    if (g_visualizerFrameTick) {
        dt = (double)(now - g_visualizerFrameTick) / 1000.0;
    }
    g_visualizerFrameTick = now;
    if (dt < 0.001) dt = 0.001;
    if (dt > 0.5) dt = 0.5;

    ScopeFrame scope = ReadScopeFrame();
    DWORD nowTick = GetTickCount();
    if (scope.tick && scope.tick != g_scopeLast.tick) {
        g_scopeLast = scope;
        // Every captured block feeds the automatic range, not just the ones a
        // frame happens to be drawn on: the reference has to see the peaks
        // between frames, or the loudest moment of a track would be missed.
        UpdateScopeReference(g_scopeLast, state.playing, dt);
    }
    bool fresh = g_scopeLast.tick && (nowTick - g_scopeLast.tick) < 320;
    // The strip keeps moving for a moment after the last block with a signal in
    // it, which is what lets the bars fall smoothly when the music stops.
    if (fresh) {
        for (int c = 0; c < 2; ++c) {
            for (int b = 0; b < kScopeBands; ++b) {
                if (g_scopeLast.rms[c][b] > (float)kScopeSilenceRms) {
                    g_lastSignalTick = nowTick;
                }
            }
        }
    }

    if (!s.visualizer) {
        ResetScopeReference();
        for (int i = 0; i < design::kMeterCount; ++i) {
            frame.heightUnits[i] = (float)design::kMeterLevels[i];
        }
        return frame;
    }

    // How far each bar should reach right now: the captured bands while there
    // is a capture to read, and a signal that moves with the clock when the
    // machine cannot be captured at all.
    double gain = (double)s.visualizerGain / 100.0;
    double reference[kScopeBands];
    for (int b = 0; b < kScopeBands; ++b) {
        reference[b] = ScopeReferenceFor(b);
    }
    float target[design::kMeterCount];
    for (int i = 0; i < design::kMeterCount; ++i) {
        float bandTarget = 0.0f;
        if (state.playing) {
            if (fresh) {
                bandTarget = ScopeTargetFor(g_scopeLast.rms, i, gain, reference);
            } else if (!g_captureWorks.load()) {
                bandTarget = SyntheticLevelFor(i, (double)now / 1000.0);
            }
        }
        target[i] = bandTarget;
    }

    // Attack quickly, release slowly, both against the real frame time so the
    // look does not change with the frame rate setting.
    if (!g_meterPrimed) {
        for (int i = 0; i < design::kMeterCount; ++i) {
            g_meterLevels[i] = target[i];
        }
        g_meterPrimed = true;
    }
    const double attack = 1.0 - std::exp(-dt / 0.045);
    const double release = 1.0 - std::exp(-dt / 0.170);
    bool moving = false;
    for (int i = 0; i < design::kMeterCount; ++i) {
        double coefficient = (target[i] > g_meterLevels[i]) ? attack : release;
        g_meterLevels[i] +=
            (float)(coefficient * (target[i] - g_meterLevels[i]));
        if (g_meterLevels[i] < 0.0005f) {
            g_meterLevels[i] = 0.0f;
        }
        if (g_meterLevels[i] > 0.0005f || target[i] > 0.0005f) {
            moving = true;
        }
    }
    frame.moving = moving;

    for (int i = 0; i < design::kMeterCount; ++i) {
        frame.heightUnits[i] =
            design::kMeterMinHeight +
            (design::kMeterMaxHeight - design::kMeterMinHeight) *
                g_meterLevels[i];
    }
    return frame;
}

// Draws the strip of bars. Runs both for a whole-widget paint and for a meter
// only update, which is why it takes nothing but the frame and the palette.
void DrawMeter(Gdiplus::Graphics& g, int dpi, Palette const& palette,
               AnimationFrame const& frame) {
    Gdiplus::REAL bottom = Scaled(design::kMeterBottom, dpi);
    Gdiplus::REAL stroke = Scaled(design::kMeterStroke, dpi);
    Gdiplus::SolidBrush meter(palette.meter);
    Gdiplus::SolidBrush peak(palette.meterPeak);
    for (int i = 0; i < design::kMeterCount; ++i) {
        Gdiplus::REAL left =
            Scaled(design::kMeterLeft + design::kMeterPitch * i, dpi);
        Gdiplus::REAL barHeight = Scaled(frame.heightUnits[i], dpi);
        // A bar that has travelled most of the way up is drawn in the lighter
        // grey: on a strip this short the height alone is easy to miss.
        Gdiplus::Brush* brush =
            frame.heightUnits[i] > design::kMeterMaxHeight * 0.72f
                ? (Gdiplus::Brush*)&peak
                : (Gdiplus::Brush*)&meter;
        g.FillRectangle(brush, Gdiplus::RectF(left, bottom - barHeight, stroke,
                                              barHeight));
    }
}

// The box the bars are drawn in, in device pixels, for the partial repaint and
// for the preview's comparison. The stroke is drawn from its left edge, so the
// box ends one stroke before the next bar's start.
Gdiplus::RectF MeterBounds(int dpi) {
    Gdiplus::REAL left = Scaled(design::kMeterLeft, dpi);
    Gdiplus::REAL right =
        Scaled(design::kMeterLeft + design::kMeterPitch * (design::kMeterCount - 1) +
                   design::kMeterStroke,
               dpi);
    Gdiplus::REAL bottom = Scaled(design::kMeterBottom, dpi);
    Gdiplus::REAL top = bottom - Scaled(design::kMeterMaxHeight, dpi);
    return Gdiplus::RectF(left, top, right - left, bottom - top);
}

// Copies one rectangle of one ARGB bitmap onto the same rectangle of another.
//
// Pixel by pixel on purpose. Drawing it with DrawImage would be a drawing
// operation - free to interpolate on the way in, and free to blend where the
// surface is translucent - while the only thing wanted here is the panel the
// bars are drawn over, exactly as it is, because the bars themselves are
// opaque and it is the pixels around them that have to survive.
bool CopyRegion(Gdiplus::Bitmap& from, Gdiplus::Bitmap& to,
                Gdiplus::Rect const& region) {
    if (region.Width <= 0 || region.Height <= 0) {
        return false;
    }
    Gdiplus::BitmapData source{};
    Gdiplus::Rect sourceRect = region;
    if (from.LockBits(&sourceRect, Gdiplus::ImageLockModeRead,
                      PixelFormat32bppARGB, &source) != Gdiplus::Ok) {
        return false;
    }
    bool copied = false;
    Gdiplus::Rect targetRect(region.X, region.Y, region.Width, region.Height);
    Gdiplus::BitmapData target{};
    if (to.LockBits(&targetRect, Gdiplus::ImageLockModeWrite,
                    PixelFormat32bppARGB, &target) == Gdiplus::Ok) {
        for (int y = 0; y < region.Height; ++y) {
            const BYTE* line =
                (const BYTE*)source.Scan0 + (size_t)y * source.Stride;
            BYTE* out = (BYTE*)target.Scan0 + (size_t)y * target.Stride;
            std::memcpy(out, line, (size_t)region.Width * 4);
        }
        to.UnlockBits(&target);
        copied = true;
    }
    from.UnlockBits(&source);
    return copied;
}

// The palette the bars are drawn in, resolved the same way PaintWidget resolves
// it, so an animation frame cannot come out a different colour from the base it
// is drawn over.
Palette CurrentPalette(Settings const& s) {
    styler::Surface surface = GetSurface();
    bool themed = s.followTaskbarTheme && surface.fromTheme;
    return MakePalette(themed && surface.lightSystem);
}

// The strip of bars alone. Both the whole-widget paint and the animation's own
// frame call this, so there is one definition of what the bars look like.
void PaintMeter(Gdiplus::Graphics& g, int dpi, Palette const& palette,
                AnimationFrame const& frame) {
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    DrawMeter(g, dpi, palette, frame);
}

// Paints the whole widget into `g`. Kept free of windows and timers so the same
// code can render off screen for a pixel comparison against the design.
//
// `skipMeter` is what makes the animation's two-part frame possible: the base
// is painted without the bars, and the bars are drawn over it on every frame.
void PaintWidget(Gdiplus::Graphics& g, int width, int height, int dpi,
                 Settings const& s, MediaState const& state, long long positionMs,
                 HoverTarget hover, AnimationFrame const& frame,
                 bool skipMeter = false) {
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);

    // With per-pixel alpha the pixels left untouched stay fully transparent.
    g.Clear(Gdiplus::Color(0, 0, 0, 0));

    // The surface: the theme of the Windows 11 Taskbar Styler mod when the user
    // asked for it and it could be read, the design's own panel otherwise. The
    // opacity setting is the design panel's own alpha, so the shipped value (255)
    // is exactly the design.
    styler::Surface surface = GetSurface();
    bool themed = s.followTaskbarTheme && surface.fromTheme;
    Palette palette = CurrentPalette(s);

    BYTE panelAlpha =
        (BYTE)(s.opacity < 40 ? 40 : (s.opacity > 255 ? 255 : s.opacity));
    Gdiplus::GraphicsPath panel;
    Gdiplus::RectF panelRect(0.0f, 0.0f, (Gdiplus::REAL)width, (Gdiplus::REAL)height);
    // While the glass is on, the panel's shape follows DWM's rounding, because
    // that is what clips the glass at the corners.
    Gdiplus::REAL panelRadius =
        Scaled(g_glass.on && g_glass.cornerRadius > 0
                   ? (Gdiplus::REAL)g_glass.cornerRadius
                   : (themed ? surface.cornerRadius : design::kPanelRadius),
               dpi);
    AddRoundRect(panel, panelRect, panelRadius);
    if (g_glass.on && g_glass.skipPanelFill) {
        // The glass carries this tint itself (see ConfigureWidgetBackdrop);
        // painting it here as well would darken the panel twice.
    } else if (themed) {
        FillWithThemeBrush(g, panel, surface.background, panelRect);
    } else {
        Gdiplus::SolidBrush panelBrush(Gdiplus::Color(panelAlpha, design::kPanelR,
                                                      design::kPanelG, design::kPanelB));
        g.FillPath(&panelBrush, &panel);
    }

    // The theme's border, stroked inside the panel's own edge so the widget keeps
    // its size. The taskbar's is a vertical gradient in most themes.
    if (themed && surface.borderThickness > 0.0f) {
        Gdiplus::REAL thickness = Scaled(surface.borderThickness, dpi);
        if (thickness > 0.0f && thickness * 2.0f < panelRadius * 2.0f + panelRect.Height) {
            Gdiplus::RectF inset(panelRect.X + thickness / 2.0f,
                                 panelRect.Y + thickness / 2.0f,
                                 panelRect.Width - thickness,
                                 panelRect.Height - thickness);
            Gdiplus::REAL insetRadius = panelRadius - thickness / 2.0f;
            Gdiplus::GraphicsPath border;
            AddRoundRect(border, inset, insetRadius > 0.0f ? insetRadius : 0.0f);
            StrokeWithThemeBrush(g, border, surface.border, panelRect, thickness);
        }
    }

    Layout layout = ComputeLayout(width, height, dpi);
    FontSet& fonts = Fonts();

    // Artwork: the album cover, or the YouTube video still, clipped to the
    // rounded square the design uses.
    if (state.hasArt && !state.artPixels.empty() && state.artSize > 0) {
        // GDI+ wants a writable scan pointer; it is only ever read from here.
        BYTE* artPixels = const_cast<BYTE*>(state.artPixels.data());
        Gdiplus::Bitmap art(state.artSize, state.artSize, state.artSize * 4,
                            PixelFormat32bppPARGB, artPixels);
        if (art.GetLastStatus() == Gdiplus::Ok) {
            Gdiplus::GraphicsPath clip;
            AddRoundRect(clip, layout.artArea, Scaled(design::kArtRadius, dpi));
            Gdiplus::GraphicsState saved = g.Save();
            g.SetClip(&clip);
            g.DrawImage(&art, layout.artArea);
            g.Restore(saved);
        }
    }

    // Track text.
    if (!state.hasSession) {
        DrawTextLine(g, L"Nothing is playing", Scaled(design::kTextLeft, dpi),
                     Scaled(design::kTextRight, dpi),
                     Scaled(design::kTitleBaseline, dpi),
                     Scaled(design::kTitleSize, dpi), fonts.medium, palette.dim,
                     false);
    } else {
        DrawTextLine(g, state.title, Scaled(design::kTextLeft, dpi),
                     Scaled(design::kTextRight, dpi),
                     Scaled(design::kTitleBaseline, dpi),
                     Scaled(design::kTitleSize, dpi), fonts.medium, palette.title,
                     false);
        // The artist is drawn in the medium weight, not the design's regular:
        // one weight step heavier at the request of the user. Its size and
        // baseline are the design's own.
        DrawTextLine(g, state.artist, Scaled(design::kTextLeft, dpi),
                     Scaled(design::kTextRight, dpi),
                     Scaled(design::kArtistBaseline, dpi),
                     Scaled(design::kArtistSize, dpi), fonts.medium, palette.dim,
                     false);
    }

    // Progress: the design's translucent track with the fill on top.
    if (s.showProgressBar) {
        Gdiplus::GraphicsPath track;
        AddRoundRect(track, layout.progressArea,
                     Scaled(design::kBarHeight / 2.0f, dpi));
        Gdiplus::SolidBrush trackBrush(palette.track);
        g.FillPath(&trackBrush, &track);

        if (state.durationMs > 0) {
            double fraction = (double)positionMs / (double)state.durationMs;
            if (fraction < 0) fraction = 0;
            if (fraction > 1) fraction = 1;
            Gdiplus::REAL full = layout.progressArea.Width;
            Gdiplus::REAL filled = (Gdiplus::REAL)(full * fraction);
            // A fill narrower than the cap would look broken, so it keeps the
            // track's own height as a minimum, as before.
            if (filled < layout.progressArea.Height) {
                filled = layout.progressArea.Height;
            }
            if (filled > full) filled = full;

            Gdiplus::RectF fillRect(layout.progressArea.X, layout.progressArea.Y,
                                    filled, layout.progressArea.Height);
            Gdiplus::GraphicsPath fill;
            AddRoundRect(fill, fillRect, Scaled(design::kBarHeight / 2.0f, dpi));
            Gdiplus::SolidBrush fillBrush(palette.fill);
            g.FillPath(&fillBrush, &fill);
        }

        // Time, laid out the way the design lays it out: the elapsed time, a
        // slash and the total, separated by the design's own 4 unit gaps
        // (the canvas has three text nodes there, not one string, so a plain
        // "1:15 / 4:00" would come out two units too narrow).
        std::wstring parts[3] = {FormatTime(positionMs), L"/",
                                 FormatTime(state.durationMs)};
        Gdiplus::REAL gap = Scaled(design::kTimeGap, dpi);
        Gdiplus::REAL baseline = Scaled(design::kTimeBaseline, dpi);
        Gdiplus::REAL size = Scaled(design::kTimeSize, dpi);
        // Same one step heavier weight as the artist line.
        Gdiplus::Font timeFont(fonts.medium, size, Gdiplus::FontStyleRegular,
                               Gdiplus::UnitPixel);
        if (timeFont.GetLastStatus() == Gdiplus::Ok) {
            Gdiplus::REAL widths[3];
            Gdiplus::REAL total = gap * 2.0f;
            for (int i = 0; i < 3; ++i) {
                widths[i] = MeasureTextLine(g, parts[i], &timeFont);
                total += widths[i];
            }
            Gdiplus::REAL x = Scaled(design::kTimeRight, dpi) - total;
            for (int i = 0; i < 3; ++i) {
                DrawTextAt(g, parts[i], Gdiplus::PointF(x, baseline), &timeFont,
                           palette.dim, Scaled(design::kTextLeft, dpi),
                           Scaled(design::kTimeRight, dpi), size);
                x += widths[i] + gap;
            }
        }
    }

    if (!skipMeter) {
        PaintMeter(g, dpi, palette, frame);
    }

    // Controls.
    DrawButton(g, layout.prevButton, design::kPrevGlyphs,
               (int)(sizeof(design::kPrevGlyphs) / sizeof(design::kPrevGlyphs[0])),
               dpi, hover == HoverTarget::Prev ? palette.buttonHover : palette.buttonFill,
               palette.glyphDim);
    DrawButton(g, layout.playButton,
               state.playing ? design::kPauseGlyphs : design::kPlayGlyphs,
               (int)(state.playing
                         ? sizeof(design::kPauseGlyphs) /
                               sizeof(design::kPauseGlyphs[0])
                         : sizeof(design::kPlayGlyphs) /
                               sizeof(design::kPlayGlyphs[0])),
               dpi, hover == HoverTarget::Play ? palette.buttonHover : palette.buttonFill,
               palette.glyph);
    DrawButton(g, layout.nextButton, design::kNextGlyphs,
               (int)(sizeof(design::kNextGlyphs) / sizeof(design::kNextGlyphs[0])),
               dpi, hover == HoverTarget::Next ? palette.buttonHover : palette.buttonFill,
               palette.glyphDim);
}

// ---------------------------------------------------------------------------
// Pushing pixels to the layered window
//
// There is one way to get pixels on screen here - UpdateLayeredWindow, which
// replaces the whole surface and is what makes the rounded corners genuinely
// transparent - and it has two properties that shape everything below:
//
//  * it takes the window's position and size as arguments, so the window does
//    not move or resize on its own. Pushing a small patch at an offset does not
//    blend the patch in: it *shrinks the window* to the patch and moves it
//    there. That was a real bug in the first version of this code - the widget
//    collapsed to the size of the meter box and jumped sideways;
//  * it always carries a whole surface, so a frame costs a bitmap of the
//    widget's own size no matter how little of it changed.
//
// So the animation keeps its own copy of the surface. The expensive part - the
// panel, the cover, the two text lines, the progress bar, the buttons - is
// painted into the base once per state change, and a frame only copies the base
// and draws the new bars over it. The bar heights are remembered, so a frame
// that would be identical to the one already on screen is not pushed at all.
// ---------------------------------------------------------------------------

Gdiplus::Bitmap* g_baseBitmap = nullptr;
Gdiplus::Bitmap* g_frameBitmap = nullptr;
std::mutex g_baseMutex;

// Gives a bitmap of exactly this size, replacing whatever was in the slot. GDI+
// bitmaps cannot be assigned, so this is how one is kept across frames.
Gdiplus::Bitmap* ResizeBitmap(Gdiplus::Bitmap*& slot, int width, int height) {
    if (slot && ((int)slot->GetWidth() != width ||
                 (int)slot->GetHeight() != height)) {
        delete slot;
        slot = nullptr;
    }
    if (!slot) {
        slot = new Gdiplus::Bitmap(width, height, PixelFormat32bppARGB);
    }
    if (slot->GetLastStatus() != Gdiplus::Ok) {
        delete slot;
        slot = nullptr;
    }
    return slot;
}

// Does `slot` already hold a frame drawn with these bar heights?
bool FrameIsCurrent(Gdiplus::Bitmap* slot, int width, int height,
                    AnimationFrame const& frame) {
    if (!slot || !g_framebufferValid) {
        return false;
    }
    if ((int)slot->GetWidth() != width || (int)slot->GetHeight() != height) {
        return false;
    }
    for (int i = 0; i < design::kMeterCount; ++i) {
        if (g_framebufferHeights[i] != frame.heightUnits[i]) {
            return false;
        }
    }
    return true;
}

void RememberFrame(AnimationFrame const& frame) {
    for (int i = 0; i < design::kMeterCount; ++i) {
        g_framebufferHeights[i] = frame.heightUnits[i];
    }
    g_framebufferValid = true;
}

void InvalidateBase() {
    std::lock_guard<std::mutex> lock(g_baseMutex);
    delete g_baseBitmap;
    g_baseBitmap = nullptr;
    delete g_frameBitmap;
    g_frameBitmap = nullptr;
    g_framebufferValid = false;
}

// Takes the whole surface to the window. `bitmap` must be the size of the
// widget: this is the call that positions it, so the size passed here is the
// size the window ends up with.
void PushSurface(HWND hwnd, Gdiplus::Bitmap& bitmap, int width, int height) {
    HDC screenDc = GetDC(nullptr);
    if (!screenDc) {
        return;
    }
    HDC memDc = CreateCompatibleDC(screenDc);
    HBITMAP dib = nullptr;
    Gdiplus::Status status = bitmap.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &dib);
    if (status == Gdiplus::Ok && dib) {
        HBITMAP oldBitmap = (HBITMAP)SelectObject(memDc, dib);

        RECT windowRect{};
        GetWindowRect(hwnd, &windowRect);
        POINT dest{windowRect.left, windowRect.top};
        SIZE size{width, height};
        POINT source{0, 0};
        BLENDFUNCTION blend{};
        blend.BlendOp = AC_SRC_OVER;
        blend.SourceConstantAlpha = 255;
        blend.AlphaFormat = AC_SRC_ALPHA;

        UpdateLayeredWindow(hwnd, screenDc, &dest, &size, memDc, &source, 0,
                            &blend, ULW_ALPHA);

        SelectObject(memDc, oldBitmap);
        DeleteObject(dib);
    }
    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);
}

// The three functions below all work on the base and the framebuffer, and they
// all run on the thread that owns the window. The lock is taken in exactly one
// place - RenderFrame - and the helpers underneath assume it is already held.
// That is deliberate: RenderMeterOnly repaints the base when there is none, and
// a helper that locked again would deadlock against its own caller.

// Paints everything that does not move with the music: the panel, the cover,
// both text lines, the progress bar and the buttons, with the bars left out.
// This is the expensive half of a frame, and it is only paid when the state
// changes rather than on every animation tick.
void RenderBaseLocked(int width, int height, int dpi, Settings const& s,
                      MediaState const& state, long long positionMs) {
    Gdiplus::Bitmap* base = ResizeBitmap(g_baseBitmap, width, height);
    if (!base) {
        return;
    }
    Gdiplus::Graphics g(base);
    AnimationFrame none;
    PaintWidget(g, width, height, dpi, s, state, positionMs, g_hover, none,
                true /*bars are drawn per frame*/);
    // The framebuffer no longer describes the surface: the base just changed.
    g_framebufferValid = false;
}

// Draws the whole widget: the base plus the bars for this frame. Returns the
// bitmap to push, or null when that frame is already on screen.
Gdiplus::Bitmap* ComposeFrameLocked(int width, int height, int dpi,
                                    AnimationFrame const& frame,
                                    bool forceRepaint) {
    Gdiplus::Bitmap* composed = ResizeBitmap(g_frameBitmap, width, height);
    if (!composed) {
        return nullptr;
    }
    if (!forceRepaint && FrameIsCurrent(composed, width, height, frame)) {
        return nullptr;
    }
    Gdiplus::Graphics g(composed);
    if (g_baseBitmap) {
        // An exact copy, not a draw: the base is being *moved* into the frame,
        // and drawing it would composite it with the framebuffer's own
        // transparent pixels. That changes the alpha of the panel's rounded
        // corners by a step and rounds one channel of a few edge pixels, which
        // is enough to make the frame differ from a whole repaint.
        if (!CopyRegion(*g_baseBitmap, *composed,
                        Gdiplus::Rect(0, 0, width, height))) {
            return nullptr;
        }
    } else {
        g.Clear(Gdiplus::Color(0, 0, 0, 0));
    }
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    PaintMeter(g, dpi, CurrentPalette(GetSettings()), frame);
    RememberFrame(frame);
    return composed;
}

// Paints the base if it is stale and then draws the bars over it, all under the
// one lock, and hands back the frame to push.
Gdiplus::Bitmap* RenderFrameLocked(HWND hwnd, int width, int height, int dpi,
                                   Settings const& s, MediaState const& state,
                                   long long positionMs, AnimationFrame const& frame,
                                   bool repaintBase) {
    if (repaintBase || !g_baseBitmap) {
        RenderBaseLocked(width, height, dpi, s, state, positionMs);
    }
    return ComposeFrameLocked(width, height, dpi, frame, repaintBase);
}

// Paints the base and pushes a whole frame: what a state change needs.
void RenderWidget(HWND hwnd, AnimationFrame const& frame) {
    RECT client{};
    GetClientRect(hwnd, &client);
    int width = client.right;
    int height = client.bottom;
    if (width <= 0 || height <= 0) {
        return;
    }
    Settings s = GetSettings();
    MediaState state = StateForRendering();
    long long positionMs = CurrentPositionMs(state, GetTickCount64());
    int dpi = GetWindowDpi(hwnd);

    Gdiplus::Bitmap* composed = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_baseMutex);
        composed = RenderFrameLocked(hwnd, width, height, dpi, s, state,
                                     positionMs, frame, true);
        if (composed) {
            // Pushed under the lock, so the bitmap cannot be replaced while the
            // window is being updated with it.
            PushSurface(hwnd, *composed, width, height);
        }
    }
    g_lastWholeSurfaceTick = GetTickCount64();
}

// Pushes a frame made of the base and the new bars, without repainting the
// base. Returns false when there is no base to draw on, in which case the
// caller does a whole repaint.
bool RenderMeterOnly(HWND hwnd, AnimationFrame const& frame, int dpi) {
    RECT client{};
    GetClientRect(hwnd, &client);
    int width = client.right;
    int height = client.bottom;
    if (width <= 0 || height <= 0) {
        return false;
    }
    Settings s = GetSettings();
    MediaState state = StateForRendering();
    long long positionMs = CurrentPositionMs(state, GetTickCount64());

    std::lock_guard<std::mutex> lock(g_baseMutex);
    if (!g_baseBitmap) {
        return false;
    }
    Gdiplus::Bitmap* composed =
        RenderFrameLocked(hwnd, width, height, dpi, s, state, positionMs, frame,
                          false);
    if (composed) {
        PushSurface(hwnd, *composed, width, height);
    }
    return true;
}


// ---------------------------------------------------------------------------
// The frosted backdrop
//
// The panel of the widget is translucent on purpose, and what sits behind it is
// the desktop: with nothing else, the wallpaper shows through it sharply. The
// widget also wears the taskbar's own surface, so it should get the taskbar's own
// glass as well.
//
// The theme on this machine (FrostyGlass) asks the taskbar for
//
//   <WindhawkBlur BlurAmount="20" TintColor="SystemChromeDarkColor" TintOpacity="0.15"/>
//
// that is a real blur of what is behind the taskbar plus a 15 % tint - which is
// exactly what SetWindowCompositionAttribute with ACCENT_ENABLE_ACRYLICBLURBEHIND
// gives a top-level window, as long as the *classic* recipe is asked for and the
// tint is the theme's own.
//
// Everything below was measured on this machine before it was written
// (preview/blur-probe.cpp runs the recipes over a four-band colour backdrop and
// photographs the screen, preview/measure-blur.py reads the pictures):
//
//  * the modern acrylic recipe (AccentFlags = 2) makes Windows apply its own dark
//    base: what is behind the window keeps only 42 % of its brightness, about
//    twice as dark as the taskbar's own pills. The classic recipe (flags 0) keeps
//    84 %, which is the pill's blur-plus-15 %-tint recipe;
//  * the tint has to be the theme's (0x261F1F1F here, that is #1F1F1F at 15 %).
//    A white tint washes the surface out (262 %), alpha 0 turns it almost black;
//  * ACCENT_ENABLE_HOSTBACKDROP (5) does nothing at all on this build, and
//    ACCENT_ENABLE_BLURBEHIND (3) is far too dark;
//  * SetWindowRgn does NOT clip the glass, the corners stay square. DWM's own
//    rounding does, at a fixed 8 px (4 px for the small setting), which is why the
//    widget uses it and why the panel is drawn at 8 px while the glass is on, even
//    though the theme asks for 10;
//  * DWMWA_SYSTEMBACKDROP_TYPE = DWMSBT_TRANSIENTWINDOW is accepted (hr = S_OK)
//    for a frameless topmost popup and then paints the system's flat fallback
//    colour: every trace of what is behind it is lost. It is not used.
// ---------------------------------------------------------------------------

namespace blur {

// SetWindowCompositionAttribute is undocumented, but it has been stable since
// Windows 10 1803 and is what the taskbar-blur tools use.
struct AccentPolicy {
    DWORD state;
    DWORD flags;
    DWORD gradientColor;  // ABGR
    DWORD animationId;
};

struct WindowCompositionAttributeData {
    DWORD attribute;
    void* data;
    SIZE_T size;
};

constexpr DWORD kWcaAccentPolicy = 19;
constexpr DWORD kAccentDisabled = 0;
constexpr DWORD kAccentEnableAcrylicBlurBehind = 4;
// The classic recipe: bit 1 would ask for the modern one, which measured about
// twice as dark as the taskbar's pills.
constexpr DWORD kAccentFlags = 0;

// Documented DWM attributes, spelled as numbers so the mod builds against any
// SDK version.
constexpr DWORD kDwmwaWindowCornerPreference = 33;
constexpr int kCornerDoNotRound = 1;  // DWMWCP_DONOTROUND
constexpr int kCornerRound = 2;       // DWMWCP_ROUND, 8 px at 96 DPI
constexpr int kCornerRoundSmall = 3;  // DWMWCP_ROUNDSMALL, 4 px at 96 DPI
constexpr int kRadiusRound = 8;
constexpr int kRadiusRoundSmall = 4;

// What was last applied, so the compositor is only called when something really
// changed. A tint of 0 means "no glass".
DWORD g_appliedTint = 0;
int g_cornerPreference = -1;
// Cleared when this Windows build has no accent acrylic at all, so the attempt is
// not repeated several times a second.
bool g_available = true;

DWORD Abgr(BYTE a, BYTE r, BYTE g, BYTE b) {
    return ((DWORD)a << 24) | ((DWORD)b << 16) | ((DWORD)g << 8) | (DWORD)r;
}

bool SetAccent(HWND hwnd, DWORD state, DWORD tint) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) {
        return false;
    }
    using Proc = BOOL(WINAPI*)(HWND, WindowCompositionAttributeData*);
    auto proc = (Proc)GetProcAddress(user32, "SetWindowCompositionAttribute");
    if (!proc) {
        return false;
    }
    AccentPolicy policy{};
    policy.state = state;
    policy.flags = kAccentFlags;
    policy.gradientColor = tint;
    WindowCompositionAttributeData data{};
    data.attribute = kWcaAccentPolicy;
    data.data = &policy;
    data.size = sizeof(policy);
    return proc(hwnd, &data) != FALSE;
}

// DWM rounds the window - and with it the glass - at a fixed size. Returns the
// radius in design units the panel should be drawn with, or 0 when this build has
// no rounding at all.
int RoundCorners(HWND hwnd, Gdiplus::REAL wantedRadius) {
    int preference = wantedRadius <= (kRadiusRound + kRadiusRoundSmall) / 2.0f
                         ? kCornerRoundSmall
                         : kCornerRound;
    if (g_cornerPreference != preference) {
        if (FAILED(DwmSetWindowAttribute(hwnd, kDwmwaWindowCornerPreference,
                                         &preference, sizeof(preference)))) {
            return 0;
        }
        g_cornerPreference = preference;
    }
    return preference == kCornerRoundSmall ? kRadiusRoundSmall : kRadiusRound;
}

void ClearCorners(HWND hwnd) {
    if (g_cornerPreference < 0) {
        return;
    }
    int preference = kCornerDoNotRound;
    DwmSetWindowAttribute(hwnd, kDwmwaWindowCornerPreference, &preference,
                          sizeof(preference));
    g_cornerPreference = -1;
}

// The compositor is only asked while the window is on screen, so the applied
// state is dropped when the widget is shown again.
void ForceReapply() { g_appliedTint = 0; }

}  // namespace blur

// Gives the widget window the taskbar's own glass: the theme's blur tint over a
// real blur of the desktop, with DWM's rounding for the corners. Called from
// PositionWidget, so it runs on the widget's own thread, follows the theme and
// only does work when something changed.
void ConfigureWidgetBackdrop(HWND widget, int width, int height) {
    Settings s = GetSettings();
    styler::Surface surface = GetSurface();
    bool themed = s.followTaskbarTheme && surface.fromTheme;

    // Which tint the glass is made of. A theme that asked for a blur, and the
    // design's own panel, are solid tints and the glass carries them exactly, so
    // the panel must not paint them a second time - that is what made the first
    // attempt twice as dark as the taskbar.
    BYTE tintA = 0;
    BYTE tintR = 0, tintG = 0, tintB = 0;
    bool skipFill = false;
    if (themed && surface.background.present && !surface.background.gradient) {
        tintA = surface.background.a;
        tintR = surface.background.r;
        tintG = surface.background.g;
        tintB = surface.background.b;
        skipFill = true;
    } else if (!themed) {
        tintA = (BYTE)(s.opacity < 40 ? 40 : (s.opacity > 255 ? 255 : s.opacity));
        tintR = design::kPanelR;
        tintG = design::kPanelG;
        tintB = design::kPanelB;
        skipFill = true;
    }

    // A gradient theme paints its own surface, and a fully transparent theme asks
    // for no surface at all: neither gets glass, and the panel is drawn as before.
    DWORD tint = blur::Abgr(tintA, tintR, tintG, tintB);
    bool wanted = s.backgroundBlur && blur::g_available && tintA > 0;

    g_glass.on = false;
    g_glass.skipPanelFill = false;
    g_glass.cornerRadius = 0;

    if (!wanted) {
        blur::ClearCorners(widget);
        if (blur::g_appliedTint != 0) {
            if (blur::SetAccent(widget, blur::kAccentDisabled, 0)) {
                blur::g_appliedTint = 0;
                Wh_Log(L"blur: glass removed");
            } else {
                blur::g_available = false;
            }
        }
        return;
    }

    if (blur::g_appliedTint != tint) {
        if (!blur::SetAccent(widget, blur::kAccentEnableAcrylicBlurBehind, tint)) {
            // No accent acrylic on this Windows build: the widget keeps the plain
            // translucent panel it had before this setting existed.
            blur::g_available = false;
            blur::g_appliedTint = 0;
            Wh_Log(L"blur: no accent acrylic on this build, the panel stays "
                   L"translucent");
            return;
        }
        blur::g_appliedTint = tint;
        Wh_Log(L"blur: glass applied, tint %08lX", (unsigned long)tint);
    }

    Gdiplus::REAL wantedRadius =
        Scaled(themed ? surface.cornerRadius : design::kPanelRadius,
               GetWindowDpi(widget));
    int radius = blur::RoundCorners(widget, wantedRadius);
    if (radius == 0) {
        // No rounding on this build: the glass would have square corners, so the
        // panel keeps its own shape and the glass stays behind it.
        radius = (int)(wantedRadius + 0.5f);
    }
    g_glass.on = true;
    g_glass.skipPanelFill = skipFill;
    g_glass.cornerRadius = radius;
}

// ---------------------------------------------------------------------------
// Placement: read-only inspection of the taskbar
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Placement: read-only inspection of the taskbar
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Placement: read-only inspection of the taskbar
// ---------------------------------------------------------------------------

// A taskbar window and the display it belongs to.
struct TaskbarEntry {
    HWND hwnd = nullptr;
    int left = 0;
    int top = 0;
};

struct TaskbarScan {
    TaskbarEntry primary;
    TaskbarEntry secondary[kMaxSecondaryTaskbars];
    int secondaryCount = 0;
};

// Collects the taskbars owned by this Explorer process: the primary one first,
// then the taskbars of the additional displays ordered left to right and then
// top to bottom, which is the order the user sees on the desk. Returns how many
// taskbars were written into `out`.
int CollectTaskbars(HWND* out, int capacity) {
    TaskbarScan scan;
    EnumWindows(
        [](HWND hWnd, LPARAM lParam) -> BOOL {
            auto* scan = reinterpret_cast<TaskbarScan*>(lParam);
            DWORD pid = 0;
            GetWindowThreadProcessId(hWnd, &pid);
            if (pid != GetCurrentProcessId()) {
                return TRUE;
            }
            WCHAR className[64];
            if (!GetClassNameW(hWnd, className, ARRAYSIZE(className))) {
                return TRUE;
            }
            bool primary = _wcsicmp(className, L"Shell_TrayWnd") == 0;
            bool secondary = _wcsicmp(className, L"Shell_SecondaryTrayWnd") == 0;
            if (!primary && !secondary) {
                return TRUE;
            }

            TaskbarEntry entry;
            entry.hwnd = hWnd;
            HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO info{};
            info.cbSize = sizeof(info);
            if (monitor && GetMonitorInfoW(monitor, &info)) {
                entry.left = info.rcMonitor.left;
                entry.top = info.rcMonitor.top;
            }

            if (primary) {
                scan->primary = entry;
            } else if (scan->secondaryCount < kMaxSecondaryTaskbars) {
                scan->secondary[scan->secondaryCount++] = entry;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&scan));

    // Insertion sort: at most a handful of entries, and it keeps the scan free
    // of any allocation or further system calls.
    for (int i = 1; i < scan.secondaryCount; ++i) {
        TaskbarEntry key = scan.secondary[i];
        int j = i - 1;
        while (j >= 0 && (scan.secondary[j].left > key.left ||
                          (scan.secondary[j].left == key.left &&
                           scan.secondary[j].top > key.top))) {
            scan.secondary[j + 1] = scan.secondary[j];
            --j;
        }
        scan.secondary[j + 1] = key;
    }

    int count = 0;
    if (scan.primary.hwnd && count < capacity) {
        out[count++] = scan.primary.hwnd;
    }
    for (int i = 0; i < scan.secondaryCount && count < capacity; ++i) {
        out[count++] = scan.secondary[i].hwnd;
    }
    return count;
}

// The taskbar the widget should sit on, and how many exist.
//
// The setting is 1-based: 1 is the primary display, 2 and up are the additional
// displays. A number that does not exist falls back to the primary taskbar, so a
// stale setting can move the widget but never lose it. `nullptr` comes back only
// when this process owns no taskbar at all - which is the case in an Explorer
// process that is not the shell.
HWND FindTaskbarWindow(int* chosenIndex = nullptr, int* taskbarCount = nullptr) {
    HWND taskbars[kMaxTaskbars] = {};
    int count = CollectTaskbars(taskbars, kMaxTaskbars);
    if (chosenIndex) {
        *chosenIndex = 0;
    }
    if (taskbarCount) {
        *taskbarCount = count;
    }
    if (count == 0) {
        return nullptr;
    }

    int wanted = GetSettings().taskbarMonitor;
    if (wanted < 1 || wanted > count) {
        wanted = 1;
    }
    if (chosenIndex) {
        *chosenIndex = wanted;
    }
    return taskbars[wanted - 1];
}

// Windhawk's log is off by default and the poll timer runs every 400 ms, so the
// placement is reported only when it actually changes.
void LogTaskbarChoice(HWND taskbar, int index, int count) {
    static HWND lastTaskbar = nullptr;
    static int lastIndex = -1;
    static int lastCount = -1;
    if (taskbar == lastTaskbar && index == lastIndex && count == lastCount) {
        return;
    }
    lastTaskbar = taskbar;
    lastIndex = index;
    lastCount = count;
    if (!taskbar) {
        Wh_Log(L"no taskbar in this Explorer process, the widget stays hidden");
    } else {
        Wh_Log(L"widget placed on taskbar %d of %d", index, count);
    }
}

int FindSystemTrayLeft(HWND taskbar) {
    static const PCWSTR kCandidates[] = {
        L"TrayNotifyWnd",
        L"TrayButton",
        L"OverflowButton",
    };
    for (PCWSTR name : kCandidates) {
        HWND h = FindWindowExW(taskbar, nullptr, name, nullptr);
        if (!h) {
            continue;
        }
        RECT r{};
        if (GetWindowRect(h, &r) && r.right > r.left) {
            return r.left;
        }
    }
    return 0;
}

// How much room the system tray needs, measured on another taskbar of this
// process. The clock and tray of the additional displays are drawn inside the
// taskbar's XAML island, so those taskbars have no tray window at all and a
// fixed offset left the widget flush against the tray, covering its left part.
// The primary taskbar's tray window is the best stand-in: it already accounts
// for the display scale, the clock format and the number of tray icons.
int MeasureTrayRoom(HWND taskbar) {
    HWND taskbars[kMaxTaskbars] = {};
    int count = CollectTaskbars(taskbars, kMaxTaskbars);
    for (int i = 0; i < count; ++i) {
        if (taskbars[i] == taskbar) {
            continue;
        }
        RECT rect{};
        if (!GetWindowRect(taskbars[i], &rect)) {
            continue;
        }
        int trayLeft = FindSystemTrayLeft(taskbars[i]);
        if (trayLeft > 0) {
            return rect.right - trayLeft;
        }
    }
    return 0;
}

// Keeps the widget on the taskbar's own level. Two things follow the taskbar:
//  * its topmost flag. Windows drops that flag from a display's taskbar while a
//    window that fills the display is up (a Telegram media viewer does this), so
//    a widget that stayed topmost would float above that window. The widget
//    takes the same flag as the taskbar.
//  * its slot in the z-order: directly beneath the window that sits right above
//    the taskbar, so anything covering the taskbar covers the widget too.
// Only reads the taskbar; the taskbar window itself is never modified.
void RestackWidget(HWND widget, HWND taskbar) {
    bool taskbarTopmost =
        (GetWindowLongW(taskbar, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    bool widgetTopmost =
        (GetWindowLongW(widget, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    const UINT noMove = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE;
    if (taskbarTopmost != widgetTopmost) {
        SetWindowPos(widget, taskbarTopmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0,
                     0, noMove);
    }
    HWND above = GetWindow(taskbar, GW_HWNDPREV);
    if (above == widget) {
        return;
    }
    HWND slot = above ? above : (taskbarTopmost ? HWND_TOPMOST : HWND_TOP);
    SetWindowPos(widget, slot, 0, 0, 0, 0, noMove);
}

// The taskbar the poll last found, so the faster visualizer tick can keep the
// widget in its slot without a search through every window.
HWND g_taskbarWindow = nullptr;

// Restacks the widget only when something has come between it and the taskbar.
// It has a timer of its own, faster than the poll (400 ms) and the visualizer
// (which slows to about 2 Hz when nothing plays): a window that opens over the
// taskbar would otherwise show the widget on top of it for that long.
constexpr UINT_PTR kZOrderTimerId = 0x4D5C;  // "MZ", after the poll and startup ids
constexpr UINT kZOrderIntervalMs = 100;

void CALLBACK ZOrderTimerProc(HWND hwnd, UINT, UINT_PTR, DWORD) {
    if (!IsWindow(hwnd) || !IsWindowVisible(hwnd) || !g_taskbarWindow ||
        !IsWindow(g_taskbarWindow) || !GetSettings().forceTopmostEveryTick) {
        return;
    }
    RestackWidget(hwnd, g_taskbarWindow);
}

// Repositions the widget. Only ever reads the taskbar; never modifies it.
void PositionWidget(HWND widget, HWND taskbar, bool forceTopmost) {
    if (!widget || !taskbar) {
        return;
    }

    Settings s = GetSettings();

    RECT taskbarRect{};
    if (!GetWindowRect(taskbar, &taskbarRect)) {
        return;
    }

    bool horizontal = (taskbarRect.right - taskbarRect.left) >=
                      (taskbarRect.bottom - taskbarRect.top);

    int x, y;
    if (horizontal) {
        int trayLeft = FindSystemTrayLeft(taskbar);
        int rightLimit = 0;
        if (trayLeft > 0) {
            rightLimit = trayLeft - s.rightMargin;
        } else {
            int trayRoom = MeasureTrayRoom(taskbar);
            if (trayRoom > 0) {
                // The mirrored room stands in for the tray window's left edge,
                // so the margin applies to it the same way: otherwise the gap
                // to the tray differs between the displays by `rightMargin`.
                rightLimit = taskbarRect.right - trayRoom - s.rightMargin;
                // The timer runs every 400 ms, so this is logged once.
                static bool loggedMirroredRoom = false;
                if (!loggedMirroredRoom) {
                    loggedMirroredRoom = true;
                    Wh_Log(L"no tray window on this taskbar, keeping %d px free "
                           L"for the tray, measured on another taskbar",
                           trayRoom);
                }
            } else {
                rightLimit = taskbarRect.right - s.fallbackRightOffset;
            }
        }
        x = rightLimit - s.widgetWidth;
        if (x < taskbarRect.left) {
            x = taskbarRect.left;
        }
        // The taskbar's own pills do not fill the taskbar: the theme gives the
        // frame a bottom margin (FrostyGlass asks for 4 px), so the pills sit in
        // the box above it. Centring the widget in the whole taskbar would leave
        // it visibly lower than its neighbours, so the same box is centred here.
        styler::Surface surface = GetSurface();
        int bottomMargin =
            (s.followTaskbarTheme && surface.fromTheme) ? surface.bottomMargin : 0;
        int box = (taskbarRect.bottom - taskbarRect.top) - bottomMargin;
        y = taskbarRect.top + (box - s.widgetHeight) / 2;
        if (y < taskbarRect.top) {
            y = taskbarRect.top;
        }
    } else {
        x = taskbarRect.left +
            ((taskbarRect.right - taskbarRect.left) - s.widgetWidth) / 2;
        y = taskbarRect.top + s.rightMargin;
    }

    UINT flags = SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_NOZORDER;
    if (forceTopmost) {
        RestackWidget(widget, taskbar);
    }
    SetWindowPos(widget, nullptr, x, y, s.widgetWidth, s.widgetHeight, flags);

    // The frosted backdrop follows the same numbers and the same schedule as the
    // widget itself: it is a property of this window, not a second one.
    ConfigureWidgetBackdrop(widget, s.widgetWidth, s.widgetHeight);
}

// ---------------------------------------------------------------------------
// Widget window
// ---------------------------------------------------------------------------

// Declared here because the message handler below re-arms the visualizer timer:
// a settings change reaches the window as a message, and the frame rate it asks
// for is only applied by the timer that draws the frames.
void RenderNow(HWND hwnd);
void UpdateVisualTimer(HWND hwnd, Settings const& s, AnimationFrame const& frame);
void CALLBACK VisualTimerProc(HWND hwnd, UINT message, UINT_PTR timerId,
                              DWORD elapsed);

LRESULT HandleWidgetMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        // The visualizer's own timer. It advances the bars and redraws the
        // strip; the second of the two calls decides whether that is cheap
        // enough to be done this often.
        case WM_TIMER:
            if (wParam == kVisualTimerId) {
                VisualTimerProc(hwnd, message, wParam, (DWORD)lParam);
                return 0;
            }
            break;

        case WM_PAINT: {
            Settings s = GetSettings();
            MediaState state = StateForRendering();
            AnimationFrame frame = AnimationFrameNow(state, s);
            RenderWidget(hwnd, frame);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case kRefreshMessage: {
            Settings s = GetSettings();
            MediaState state = StateForRendering();
            AnimationFrame frame = AnimationFrameNow(state, s);
            RenderWidget(hwnd, frame);
            UpdateVisualTimer(hwnd, s, frame);
            return 0;
        }

        // The command thread sent the command to the player; redrawing here
        // shows its effect - the button, the position - before the next poll.
        case kForceRefreshMessage: {
            Settings s = GetSettings();
            MediaState state = StateForRendering();
            AnimationFrame frame = AnimationFrameNow(state, s);
            RenderWidget(hwnd, frame);
            return 0;
        }

        case kCloseMessage:
            DestroyWindow(hwnd);
            return 0;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_LBUTTONDOWN: {
            POINT p{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            RECT client{};
            GetClientRect(hwnd, &client);
            int dpi = GetWindowDpi(hwnd);
            Settings s = GetSettings();
            // Same state the widget is drawn from, so a click toggles what the
            // user actually sees. Reading the polled state instead would let two
            // quick clicks compute the same target and appear to do nothing.
            MediaState state = StateForRendering();
            Layout layout = ComputeLayout(client.right, client.bottom, dpi);

            if (PointIn(p, layout.prevButton)) {
                QueueCommand(Command::Prev, 0);
                // Another track is coming; nothing here can describe it yet.
                SetOptimisticState(false, 0, false, true);
            } else if (PointIn(p, layout.playButton)) {
                QueueCommand(Command::PlayPause, 0);
                SetOptimisticState(!state.playing, 0, false);
                RenderNow(hwnd);
            } else if (PointIn(p, layout.nextButton)) {
                QueueCommand(Command::Next, 0);
                SetOptimisticState(false, 0, false, true);
            } else if (s.showProgressBar && s.seekOnClick && state.canSeek &&
                       state.durationMs > 0 && PointIn(p, layout.progressArea)) {
                double trackWidth = (double)layout.progressArea.Width;
                if (trackWidth > 0) {
                    double fraction =
                        (double)(p.x - layout.progressArea.X) / trackWidth;
                    if (fraction < 0) fraction = 0;
                    if (fraction > 1) fraction = 1;
                    long long seekMs =
                        (long long)(fraction * (double)state.durationMs);
                    QueueCommand(Command::Seek, seekMs);
                    // The playhead lands where it was clicked, and keeps counting
                    // from there, so the bar does not lag a round trip behind.
                    SetOptimisticState(state.playing, seekMs, true);
                    RenderNow(hwnd);
                }
            }
            return 0;
        }

        case WM_MOUSEMOVE: {
            if (!g_trackingMouse) {
                TRACKMOUSEEVENT track{};
                track.cbSize = sizeof(track);
                track.dwFlags = TME_LEAVE;
                track.hwndTrack = hwnd;
                if (TrackMouseEvent(&track)) {
                    g_trackingMouse = true;
                }
            }

            POINT p{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            RECT client{};
            GetClientRect(hwnd, &client);
            Layout layout = ComputeLayout(client.right, client.bottom,
                                          GetWindowDpi(hwnd));
            HoverTarget target = HitTest(layout, p);
            if (target != g_hover) {
                g_hover = target;
                RenderNow(hwnd);
            }
            return 0;
        }

        case WM_MOUSELEAVE: {
            g_trackingMouse = false;
            if (g_hover != HoverTarget::None) {
                g_hover = HoverTarget::None;
                RenderNow(hwnd);
            }
            return 0;
        }

        case WM_SETCURSOR: {
            POINT p{};
            GetCursorPos(&p);
            ScreenToClient(hwnd, &p);
            RECT client{};
            GetClientRect(hwnd, &client);
            Layout layout = ComputeLayout(client.right, client.bottom,
                                          GetWindowDpi(hwnd));
            if (HitTest(layout, p) != HoverTarget::None) {
                SetCursor(LoadCursorW(nullptr, IDC_HAND));
            } else {
                SetCursor(LoadCursorW(nullptr, IDC_ARROW));
            }
            return TRUE;
        }

        case WM_DESTROY:
            KillTimer(hwnd, kVisualTimerId);
            g_visualTimerInterval = 0;
            InvalidateBase();
            g_widgetWindow.store(nullptr);
            return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK WidgetProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    // Anything unexpected is swallowed here. Explorer's process must never be
    // taken down by a drawing or input bug in this widget.
    try {
        return HandleWidgetMessage(hwnd, message, wParam, lParam);
    } catch (...) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

// Two timers, both on the polling thread that owns the widget window:
//  * the poll timer reads the player state and keeps the widget in place;
//  * the visualizer timer redraws the bars while they move, and drops back to
//    its idle interval when they do not.
// Queued commands are deliberately NOT run here: this callback is busy for a
// good part of every interval, and a click must not wait for it. See
// CommandThreadProc.
void CALLBACK PollTimerProc(HWND hwnd, UINT, UINT_PTR, DWORD) {
    try {
        Settings s = GetSettings();
        RefreshState();
        // Cheap when nothing changed: two registry values and a file timestamp.
        RefreshSurface(false);
        // A capture thread that has ended - the device was removed, or it was
        // busy when the widget started - is brought back here.
        if (s.visualizer) {
            EnsureCaptureRunning();
        }

        if (!IsWindow(hwnd)) {
            return;
        }
        int index = 0;
        int count = 0;
        HWND taskbar = FindTaskbarWindow(&index, &count);
        LogTaskbarChoice(taskbar, index, count);
        g_taskbarWindow = taskbar;
        if (!taskbar) {
            // An Explorer process that is not the shell owns no taskbar. There is
            // nothing to sit on, so the widget hides instead of hanging in the
            // top-left corner of the screen.
            ShowWindow(hwnd, SW_HIDE);
            return;
        }
        if (!GetState().hasSession) {
            // No player has a session, so there is nothing to show. The widget
            // hides until one appears; the poll brings it back then.
            ShowWindow(hwnd, SW_HIDE);
            return;
        }
        PositionWidget(hwnd, taskbar, s.forceTopmostEveryTick);
        if (!IsWindowVisible(hwnd)) {
            ShowWindow(hwnd, SW_SHOWNOACTIVATE);
            // The accent is (re)applied once the window is really on screen: the
            // compositor is not asked to blur a window nobody can see.
            blur::ForceReapply();
            ConfigureWidgetBackdrop(hwnd, s.widgetWidth, s.widgetHeight);
        }
        RenderNow(hwnd);
    } catch (...) {
    }
}

// Draws the widget with the bars advanced to this moment.
void RenderNow(HWND hwnd) {
    Settings s = GetSettings();
    MediaState state = StateForRendering();
    AnimationFrame frame = AnimationFrameNow(state, s);
    RenderWidget(hwnd, frame);
    UpdateVisualTimer(hwnd, s, frame);
}

// The animation tick. It only reaches the window while the bars are moving, and
// it is what makes the motion smooth rather than one step per poll interval.
//
// The whole surface is only rebuilt now and then: the bars are drawn over the
// last one, so a frame costs a few hundred pixels instead of a repaint of the
// panel, the cover, both text lines and the buttons. The rebuild also happens
// when the restore fails, which is the case before the first whole paint.
void CALLBACK VisualTimerProc(HWND hwnd, UINT, UINT_PTR, DWORD) {
    if (!IsWindow(hwnd) || !IsWindowVisible(hwnd)) {
        return;
    }
    Settings s = GetSettings();
    MediaState state = StateForRendering();
    AnimationFrame frame = AnimationFrameNow(state, s);
    int dpi = GetWindowDpi(hwnd);
    bool wholeSurface = (GetTickCount64() - g_lastWholeSurfaceTick) > 200;
    if (wholeSurface) {
        RenderWidget(hwnd, frame);
    } else if (!RenderMeterOnly(hwnd, frame, dpi)) {
        RenderWidget(hwnd, frame);
    }
    UpdateVisualTimer(hwnd, s, frame);
}

// Arms the visualizer timer for the rate the current state deserves: fast while
// the bars move, slow when they are at rest. Re-arming with the same interval is
// skipped, so the timer is not touched on every frame.
void UpdateVisualTimer(HWND hwnd, Settings const& s, AnimationFrame const& frame) {
    if (!IsWindow(hwnd)) {
        return;
    }
    UINT interval = kVisualIdleIntervalMs;
    DWORD signalAge = GetTickCount() - g_lastSignalTick;
    bool live = !s.visualizer || frame.moving || signalAge < kVisualFallMs;
    if (live) {
        int fps = s.visualizerFps < 10 ? 10 : (s.visualizerFps > 120 ? 120 : s.visualizerFps);
        interval = (UINT)(1000 / fps);
        if (interval < (UINT)kVisualMinIntervalMs) {
            interval = (UINT)kVisualMinIntervalMs;
        }
    }
    if (interval == g_visualTimerInterval) {
        return;
    }
    if (SetTimer(hwnd, kVisualTimerId, interval, VisualTimerProc)) {
        g_visualTimerInterval = interval;
    }
}

bool CreateWidget() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WidgetProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWidgetClass;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        Wh_Log(L"RegisterClassExW failed: %u", GetLastError());
        return false;
    }

    Settings s = GetSettings();

    // An independent, layered, topmost tool window:
    //  * no parent, so the taskbar's XAML island is never disturbed;
    //  * WS_EX_TOPMOST plus DWM compositing puts it above the taskbar surface,
    //    which is how TranslucentTB and similar tools guarantee visibility;
    //  * WS_EX_NOACTIVATE keeps clicks from stealing focus from the user's app.
    // WS_EX_LAYERED is required by UpdateLayeredWindow. SetLayeredWindowAttributes
    // is deliberately NOT called: it would apply a single global alpha to the
    // whole window, which painted the area outside the rounded corners as a
    // black rectangle. Per-pixel alpha comes from the bitmap instead, which is
    // what makes the corners genuinely transparent.
    HWND widget = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
        kWidgetClass, L"", WS_POPUP, 0, 0, s.widgetWidth, s.widgetHeight, nullptr,
        nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!widget) {
        Wh_Log(L"CreateWindowExW failed: %u", GetLastError());
        return false;
    }

    g_widgetWindow = widget;

    // The widget is created hidden and is only shown once there is a taskbar to
    // sit on. In an Explorer process that is not the shell there is none, and an
    // unplaced layered window would otherwise be visible at 0,0.
    HWND taskbar = FindTaskbarWindow();
    if (taskbar) {
        PositionWidget(widget, taskbar, true);
        ShowWindow(widget, SW_SHOWNOACTIVATE);
    } else {
        Wh_Log(L"no taskbar in this Explorer process, the widget stays hidden");
    }

    if (!SetTimer(widget, kPollTimerId, (UINT)s.pollIntervalMs, PollTimerProc)) {
        Wh_Log(L"poll SetTimer failed: %u", GetLastError());
    }
    if (!SetTimer(widget, kZOrderTimerId, kZOrderIntervalMs, ZOrderTimerProc)) {
        Wh_Log(L"z-order SetTimer failed: %u", GetLastError());
    }
    // The visualizer's timer starts idle: the polling tick arms it for the fast
    // rate as soon as the bars have something to show.
    {
        Settings current = GetSettings();
        int fps = current.visualizerFps < 10
                      ? 10
                      : (current.visualizerFps > 120 ? 120 : current.visualizerFps);
        UINT interval = (UINT)(1000 / fps);
        if (interval < (UINT)kVisualMinIntervalMs) {
            interval = (UINT)kVisualMinIntervalMs;
        }
        g_visualTimerInterval = interval;
        if (!SetTimer(widget, kVisualTimerId, interval, VisualTimerProc)) {
            Wh_Log(L"visual SetTimer failed: %u", GetLastError());
            g_visualTimerInterval = 0;
        }
    }
    Wh_Log(L"Widget created: %p", widget);
    return true;
}

// Creates the widget shortly after the mod loads, on a one-shot timer.
//
// Creating the window immediately made the whole desktop flicker when the mod
// was toggled: Explorer was still busy applying the injection, and a new
// topmost window appearing at that exact moment forced a visible repaint of the
// taskbar. Waiting a moment makes the appearance smooth.
constexpr UINT_PTR kStartupTimerId = 0x4D5A;
constexpr UINT kStartupDelayMs = 1500;

void CALLBACK StartupTimerProc(HWND hwnd, UINT, UINT_PTR, DWORD) {
    KillTimer(hwnd, kStartupTimerId);
    if (g_stopThread.load()) {
        return;
    }
    try {
        CreateWidget();
    } catch (...) {
    }
}

// Commands are executed here, not on the message thread and not on the poll
// timer. Both of those are busy with other work for a noticeable part of every
// interval - reading the player, decoding the artwork, scanning the taskbars -
// and a click that waits for their current round to finish is exactly the delay
// this thread exists to remove. The thread is woken by its event, so a command
// normally reaches the player within a millisecond of the click.
//
// No window messages and no polling state are touched here, which is why this
// can run beside the polling thread without disturbing it. The apartment is
// multi-threaded, so the session object can be used from this thread too.
DWORD WINAPI CommandThreadProc(LPVOID) {
    try {
        init_apartment(apartment_type::multi_threaded);
    } catch (...) {
    }

    while (!g_stopThread.load()) {
        if (g_pollWakeEvent) {
            WaitForSingleObject(g_pollWakeEvent, 1000);
        } else {
            Sleep(50);
        }
        if (g_stopThread.load()) {
            break;
        }
        try {
            RunPendingCommand();
        } catch (...) {
        }
    }

    if (g_commandThreadExitedEvent) {
        SetEvent(g_commandThreadExitedEvent);
    }
    return 0;
}

DWORD WINAPI PollThreadProc(LPVOID) {
    // This thread owns the media session objects and the widget window, so it
    // initializes its own apartment. The taskbar's thread is never blocked on
    // WinRT calls.
    bool apartmentReady = false;
    try {
        init_apartment(apartment_type::multi_threaded);
        apartmentReady = true;
    } catch (...) {
    }

    if (apartmentReady) {
        // A message-only window carries the startup timer, so the widget itself
        // is only created once Explorer has settled.
        try {
            HWND host = CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0,
                                        HWND_MESSAGE, nullptr,
                                        GetModuleHandleW(nullptr), nullptr);
            if (host) {
                if (!SetTimer(host, kStartupTimerId, kStartupDelayMs,
                              StartupTimerProc)) {
                    // Fall back to creating the widget at once.
                    CreateWidget();
                }
            } else {
                CreateWidget();
            }
        } catch (...) {
        }
    }

    // A message loop is required for the widget window and its timer. It ends
    // when WM_QUIT arrives or when the stop flag is set and no messages are
    // pending.
    MSG msg;
    while (!g_stopThread.load()) {
        BOOL result = GetMessageW(&msg, nullptr, 0, 0);
        if (result == 0 || result == -1) {
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    try {
        std::lock_guard<std::mutex> lock(g_sessionMutex);
        g_session = nullptr;
    } catch (...) {
    }

    if (g_threadExitedEvent) {
        SetEvent(g_threadExitedEvent);
    }
    return 0;
}

// Applies new settings to the live widget. Safe to call from any thread:
// PostMessage does not touch the window and the size is applied by the window's
// own timer, which runs on the owning thread.
void ApplySettingsToWidget() {
    HWND widget = g_widgetWindow.load();
    if (widget) {
        PostMessageW(widget, kRefreshMessage, 0, 0);
    }
}

}  // namespace

// GDI+ is initialised once when the mod loads and shut down when it unloads.
ULONG_PTR g_gdiplusToken = 0;

BOOL Wh_ModInit() {
    Wh_Log(L">");
    LoadSettings();
    RefreshSurface(true);

    Gdiplus::GdiplusStartupInput gdiplusInput;
    if (Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusInput, nullptr) !=
        Gdiplus::Ok) {
        Wh_Log(L"GdiplusStartup failed");
        g_gdiplusToken = 0;
    }

    g_pollWakeEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    g_threadExitedEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_commandThreadExitedEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_captureStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_captureExitedEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    // The visualizer's capture is independent of the widget window, so it can
    // start here and be ready by the time the first frame is drawn.
    if (GetSettings().visualizer) {
        StartCapture();
    }

    HANDLE thread = CreateThread(nullptr, 0, PollThreadProc, nullptr, 0, nullptr);
    if (thread) {
        CloseHandle(thread);
    } else {
        Wh_Log(L"CreateThread failed: %u", GetLastError());
    }

    // Commands run on their own thread. Without it they would wait for the poll
    // timer, which is what made a click feel late.
    HANDLE commandThread =
        CreateThread(nullptr, 0, CommandThreadProc, nullptr, 0, nullptr);
    if (commandThread) {
        CloseHandle(commandThread);
    } else {
        Wh_Log(L"command CreateThread failed: %u", GetLastError());
    }

    return TRUE;
}

void Wh_ModAfterInit() {
    Wh_Log(L">");
    // Intentionally empty: every piece of work happens on the polling and
    // command threads, so nothing ever runs on the taskbar's thread.
}

void Wh_ModUninit() {
    Wh_Log(L">");

    // The capture thread is stopped and joined first, before the widget window
    // and GDI+ go away: it is the only thread that can still touch this library
    // without being asked to.
    StopCapture();

    g_stopThread.store(true);

    // Ask the widget to close on its own thread, and wake its message loop so
    // GetMessage returns and the loop can observe the stop flag.
    HWND widget = g_widgetWindow.load();
    if (widget) {
        PostMessageW(widget, kCloseMessage, 0, 0);
        PostMessageW(widget, WM_NULL, 0, 0);
    }
    // Wake the command thread too: it is sitting on this event, and the flag it
    // re-reads afterwards is what ends it.
    if (g_pollWakeEvent) {
        SetEvent(g_pollWakeEvent);
    }

    // Wait for the polling thread to finish. Returning from unload while it
    // still runs would let Explorer call into unloaded code, so this wait is
    // bounded but generous, and a timeout is logged rather than ignored.
    if (g_threadExitedEvent) {
        DWORD wait = WaitForSingleObject(g_threadExitedEvent, 5000);
        if (wait != WAIT_OBJECT_0) {
            Wh_Log(L"Polling thread did not exit in time (wait=%u)", wait);
        }
        CloseHandle(g_threadExitedEvent);
        g_threadExitedEvent = nullptr;
    }

    if (g_commandThreadExitedEvent) {
        DWORD wait = WaitForSingleObject(g_commandThreadExitedEvent, 5000);
        if (wait != WAIT_OBJECT_0) {
            Wh_Log(L"Command thread did not exit in time (wait=%u)", wait);
        }
        CloseHandle(g_commandThreadExitedEvent);
        g_commandThreadExitedEvent = nullptr;
    }

    if (g_pollWakeEvent) {
        CloseHandle(g_pollWakeEvent);
        g_pollWakeEvent = nullptr;
    }

    if (g_captureStopEvent) {
        CloseHandle(g_captureStopEvent);
        g_captureStopEvent = nullptr;
    }
    if (g_captureExitedEvent) {
        CloseHandle(g_captureExitedEvent);
        g_captureExitedEvent = nullptr;
    }
    InvalidateBase();

    // GDI+ goes last, after the widget window and its drawing are gone.
    if (g_gdiplusToken) {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
}

void Wh_ModSettingsChanged() {
    Wh_Log(L">");
    LoadSettings();
    // The styler's theme may have changed while the settings page was open.
    RefreshSurface(true);

    // The widget re-reads the settings on its next tick; this only makes the
    // change visible immediately instead of up to one interval later, and it is
    // also where the visualizer timer is re-armed for a new frame rate.
    HWND widget = g_widgetWindow.load();
    if (widget) {
        PostMessageW(widget, kRefreshMessage, 0, 0);
    }
}

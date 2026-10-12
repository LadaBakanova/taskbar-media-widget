# Windows 11 desktop blur APIs — source-backed findings

Tooling note: in this session `web_fetch` was **blocked** (every hostname resolved to a non-public
IP). All content below was retrieved with `Invoke-WebRequest` from the shell and HTML-stripped
locally. Nothing here relies on `web_fetch`.

## 1. ACCENT_STATE / ACCENT_FLAG values (undocumented API)

`SetWindowCompositionAttribute` + `WCA_ACCENT_POLICY` (19) is **undocumented**; the enum values are
reverse-engineered. Two independent reverse-engineered headers agree:

| Value | Name | Meaning |
|---|---|---|
| 0 | `ACCENT_DISABLED` | background black |
| 1 | `ACCENT_ENABLE_GRADIENT` | GradientColor, alpha ignored |
| 2 | `ACCENT_ENABLE_TRANSPARENTGRADIENT` | GradientColor, alpha honoured |
| 3 | `ACCENT_ENABLE_BLURBEHIND` | GradientColor + blur |
| 4 | `ACCENT_ENABLE_ACRYLICBLURBEHIND` | GradientColor + acrylic |
| 5 | `ACCENT_ENABLE_HOSTBACKDROP` | enables `Compositor.CreateHostBackdropBrush` |
| 6 | `ACCENT_INVALID_STATE` | "unknown, seems to draw fully transparent" |

Sources:
- https://raw.githubusercontent.com/TranslucentTB/TranslucentTB/release/Common/undoc/user32.hpp
- https://raw.githubusercontent.com/ramensoftware/windhawk-mods/main/mods/translucent-windows.wh.cpp

`ACCENT_POLICY` layout: `{ AccentState; AccentFlags; GradientColor /* COLORREF */; AnimationId; }`.
TranslucentTB notes `AccentFlags = 2` means "GradientColor is used". `translucent-windows` (v1.8.2)
adds `ACCENT_FLAG_ENABLE_MODERN_ACRYLIC_RECIPE = 1 << 1` commented "Windows 11 22H2+" —
i.e. on 22H2+ acrylic uses a different recipe selected by that flag.

`ACCENT_ENABLE_BLURBEHIND` (3) is annotated in `translucent-windows.wh.cpp` as
`// Removed in Windows 11 22H2+`.

## 2. Does accent blur still work on Windows 11?

**Yes — verified for Windows 11 22H2..24H2+ by shipping code that still calls it.**

- `taskbar-background-helper` (Windhawk mods, **v1.2, committed 2026-01-01**) sets
  `ACCENT_ENABLE_BLURBEHIND` / `ACCENT_ENABLE_ACRYLICBLURBEHIND` / `ACCENT_ENABLE_TRANSPARENTGRADIENT`
  on `Shell_TrayWnd` and `Shell_SecondaryTrayWnd` from inside explorer.exe.
  https://raw.githubusercontent.com/ramensoftware/windhawk-mods/main/mods/taskbar-background-helper.wh.cpp
- `dynamic-island-for-windows` (**v1.3.1, committed 2026-09-26**) uses 3 and 4 on a custom top-level
  window. https://raw.githubusercontent.com/ramensoftware/windhawk-mods/main/mods/dynamic-island-for-windows.wh.cpp
- FluentFlyout (WPF) uses `ACCENT_ENABLE_ACRYLICBLURBEHIND`:
  https://github.com/unchihugo/FluentFlyout/blob/e5da6514/FluentFlyoutWPF/Classes/WindowBlurHelper.cs

Known limitations, with evidence quality:

- **Needs transparency effects / falls back on Battery Saver.** VERIFIED in Microsoft docs (about
  WinUI, but the fallback is a platform behaviour): "Windows disables acrylic
  (`DesktopAcrylicBackdrop` and `AcrylicBrush`) when Battery Saver is on. **Mica is not affected.**"
  and "if the user turns off Transparency effects ... all SystemBackdrop materials and AcrylicBrush
  fall back to their solid-color alternatives."
  https://learn.microsoft.com/en-us/windows/apps/develop/ui/materials
- **Plain blur (`ACCENT_ENABLE_BLURBEHIND`) is the fragile one on 22H2+.** Community report (Windhawk
  mod source comment). Use acrylic instead.
- **Blur loss on resize / flicker / lag.** Community reports, mainly Windows 10:
  - dotnet/wpf#3608: blur disappears on resize since Win10 2004, only with ≥2 blurred windows on one
    monitor and after clicking around; reporter found "The acrylic blur does not disappear when
    resizing the window" (i.e. acrylic tolerated it, plain blur-behind did not). Also: **"if my window
    is a child window of the desktop (WorkerW-SHELLDLL_DefView), it is impossible to create the blur
    effect."** https://github.com/dotnet/wpf/issues/3608
  - Windows.UI.Composition-Win32-Samples#66: acrylic flicker + resize lag.
    https://github.com/microsoft/Windows.UI.Composition-Win32-Samples/issues/66
- **Edge bleeding when maximized/snapped** is stated by the mod author: "The blur effect may show a
  bleeding effect at the edges of a window when maximized or snapped to the edge of the screen. This
  is caused by default by the AccentBlur API." (translucent-windows readme, in-source)
- **Does `WS_EX_LAYERED` break it? No** — see §3.
- I found **no** authoritative source documenting a 24H2/25H2 removal of
  `ACCENT_ENABLE_ACRYLICBLURBEHIND`. Treat "broken on 24H2" claims as unverified.

## 3. WS_EX_LAYERED + UpdateLayeredWindow + blur — the key finding

**The common claim "layered + UpdateLayeredWindow ⇒ black instead of glass" is FALSE as a general
statement. It works, and there is shipping code proving it.**

`dynamic-island-for-windows` v1.3.1 creates exactly your window and stacks acrylic under per-pixel
alpha:

```cpp
HWND hwnd = CreateWindowExW(
    WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
    kWindowClass, L"Dynamic Island for Windows", WS_POPUP, 0, 0, 520, 140,
    nullptr, nullptr, wc.hInstance, nullptr);
```

It renders content with:
```cpp
blend.BlendOp = AC_SRC_OVER;
blend.SourceConstantAlpha = ...;
blend.AlphaFormat = AC_SRC_ALPHA;
return UpdateLayeredWindow(hwnd_, nullptr, &dst, &size, memDc_, &src, 0, &blend, ULW_ALPHA) != FALSE;
```

and its own comment states the architecture explicitly:

> It composes with UpdateLayeredWindow: DWM blurs whatever is behind the window, and the per-pixel
> alpha we paint decides how much of that blur shows through. That is why enabling a backdrop also
> caps the pill's own fill alpha further down -- an opaque fill would hide the very effect being
> switched on.

It then deliberately lowers the pill fill alpha when a backdrop is on, and drops the shadow/tint scrim
which would otherwise hide the blur:
```cpp
if (settings.backdropMaterial != BackdropMaterial::None) { bg.a = std::min(bg.a, settings.backdropFillAlpha); }
```
Its readme: "**Real Blur & Acrylic Backdrop:** Optionally paint genuine Windows blur or frosted acrylic
behind the island ... Requires Windows 10 1803 or newer; on unsupported builds the island simply stays
opaque."
https://raw.githubusercontent.com/ramensoftware/windhawk-mods/main/mods/dynamic-island-for-windows.wh.cpp

**So: your current architecture (WS_EX_LAYERED + UpdateLayeredWindow) is compatible with acrylic.**

What *is* documented and relevant:
- `DwmEnableBlurBehindWindow` — "**Beginning with Windows 8, calling this function doesn't result in
  the blur effect, due to a style change in the way windows are rendered.**" Also: only top-level
  windows. https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/nf-dwmapi-dwmenableblurbehindwindow
  (superseded by `SetWindowCompositionAttribute`, which is a different mechanism.)
- DWM Blur Behind Overview repeats the Windows 8 note; `DwmExtendFrameIntoClientArea` with
  `MARGINS{-1}` ("sheet of glass") likewise no longer frosts.
  https://learn.microsoft.com/en-us/windows/win32/dwm/blur-ovw
- The genuine caveat is **parenting**, not layering: blur cannot be created on a window parented into
  the desktop `WorkerW`/`SHELLDLL_DefView` hierarchy (dotnet/wpf#3608 above). Since you inject into
  explorer.exe and sit above the taskbar, make sure your popup stays unowned/top-level.

Caveats of the "works" result, because the evidence is one mod:
- It is a community implementation, not a spec. It resolves `SetWindowCompositionAttribute`
  dynamically and silently falls back to opaque if the export is missing.
- Note it also uses `WS_EX_TRANSPARENT` (click-through); that is independent of blur.
- Because you use per-pixel alpha via the layer, **your own content must stay translucent where you
  want the blur visible.** Fully opaque pixels hide the acrylic. That is inherent, not a bug.
- Acrylic will not be drawn when the desktop-backdrop sampling is unavailable (battery saver,
  transparency effects off, RDP). Plan an opaque fallback.

## 4. DWMWA_SYSTEMBACKDROP_TYPE

VERIFIED from Microsoft docs:

- Enum: `DWMSBT_AUTO`(0), `DWMSBT_NONE`(1), `DWMSBT_MAINWINDOW`(2), `DWMSBT_TRANSIENTWINDOW`(3),
  `DWMSBT_TABBEDWINDOW`(4). **Minimum supported client: Windows 11 Build 22621** (22H2).
  https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwm_systembackdrop_type
- Semantics from the same page: `DWMSBT_MAINWINDOW` = "Mica in its default variant";
  `DWMSBT_TRANSIENTWINDOW` = "**Desktop Acrylic, also known as Background Acrylic**";
  `DWMSBT_TABBEDWINDOW` = "Mica Alt".
- **Acrylic blurs what is behind the window; Mica does not.** VERIFIED:
  - "Desktop Acrylic shows a live, blurred view of the desktop and content behind the app window,
    creating a frosted-glass appearance." https://learn.microsoft.com/en-us/windows/apps/develop/ui/materials
  - "Mica is specifically designed for app performance as it only samples the desktop wallpaper once
    to create its visualization." https://learn.microsoft.com/en-us/windows/apps/design/style/mica
  - "Background acrylic reveals the desktop wallpaper and other windows that are behind the currently
    active app". https://learn.microsoft.com/en-us/windows/apps/design/style/acrylic
  => `DWMSBT_TRANSIENTWINDOW` blurs **windows behind it**, not just wallpaper. `DWMSBT_MAINWINDOW`
  (Mica) only uses the wallpaper.
- `DWMWINDOWATTRIBUTE` build gates (VERIFIED):
  https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute
  - `DWMWA_USE_HOSTBACKDROPBRUSH` = 23, Windows 11 Build 22000
  - `DWMWA_USE_IMMERSIVE_DARK_MODE` = 20, Build 22000
  - `DWMWA_WINDOW_CORNER_PREFERENCE` = 33, Build 22000
  - `DWMWA_BORDER_COLOR` = 34, Build 22000
  - `DWMWA_SYSTEMBACKDROP_TYPE` = 38, **Build 22621**
  - `DWMWA_REDIRECTIONBITMAP_ALPHA`, **Build 26100** (24H2)
  - `DWMWA_BORDER_MARGINS`, "upcoming update to Windows 11 build 26100"

**Frameless `WS_POPUP` support.** No doc states "requires WS_CAPTION/WS_THICKFRAME", and there is
evidence it is not required, but a *different* precondition is real. Qt's Windows platform plugin
(`win32windoweffects_module.cpp`, used by VLC) refuses the 22H2 native acrylic backdrop unless the
window is either frameless **or** has `WS_EX_NOREDIRECTIONBITMAP`:

```cpp
if (!window->flags().testFlag(Qt::FramelessWindowHint)) {
    const auto extendedStyle = GetWindowLong((HWND)window->winId(), GWL_EXSTYLE);
    if (!(extendedStyle & 0x00200000L /* WS_EX_NOREDIRECTIONBITMAP */)) {
        qDebug("Target window is not frameless and does not use WS_EX_NOREDIRECTIONBITMAP, "
               "window can not be translucent for the Windows 11 22H2 native acrylic backdrop effect.");
        return false;
    }
}
```
and then it simply calls `DwmSetWindowAttribute(hwnd, 38, &DWMSBT_TRANSIENTWINDOW, sizeof(int))` —
**no** `DwmExtendFrameIntoClientArea`, no `DWMWA_NCRENDERING_POLICY`, no dark-mode attribute.
https://raw.githubusercontent.com/videolan/vlc/master/modules/gui/qt/maininterface/win32windoweffects_module.cpp
and the commit: https://mailman.videolan.org/pipermail/vlc-commits/2025-January/070907.html

Extra attribute: `DWMWA_USE_HOSTBACKDROPBRUSH` (TRUE) is needed when you use
`Compositor.CreateHostBackdropBrush` instead of the accent policy; per Win32Acrylic, without the host
backdrop brush "During the maximized animation, the effect will temporarily unavailable".
https://github.com/ALTaleX531/Win32Acrylic

The mod `translucent-windows` gates its SystemBackdrop options with "Windows 11 version >= 22621.xxx
(22H2) is required for SystemBackdrop effects."

**Important for you:** `DWMWA_SYSTEMBACKDROP_TYPE` and `UpdateLayeredWindow` are, on the evidence I
have, **not** demonstrated to work together. All the working SystemBackdrop examples
(VLC/Qt, WinUI, `translucent-windows`) go through the redirection-surface-free /
composition path, and Qt explicitly requires frameless-or-`WS_EX_NOREDIRECTIONBITMAP`. Treat
"SystemBackdrop on a layered UpdateLayeredWindow window" as **unverified / unlikely**. The proven
combination for your window shape is `SetWindowCompositionAttribute` (§3).

## 5. DWMWA_WINDOW_CORNER_PREFERENCE

VERIFIED:
- Enum: `DWMWCP_DEFAULT`=0, `DWMWCP_DONOTROUND`=1, `DWMWCP_ROUND`=2, `DWMWCP_ROUNDSMALL`=3.
  Minimum supported client: **Windows Build 22000**. Docs do **not** state a pixel radius.
  https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwm_window_corner_preference
- Radius values, VERIFIED from Microsoft design docs: "**8px** — Top-level containers such as app
  windows, flyouts and dialogs are rounded using an 8px corner radius." / "**4px** — In-page
  elements..."; flyouts/overlays = 8px, ToolTip = 4px (exception "due to its small size").
  "Window corners are not rounded when windows are snapped or maximized."
  Also: "The default corner radii are controlled by two global resources: `ControlCornerRadius`
  (default 4px) and `OverlayCornerRadius` (default 8px). You can override these values in your
  App.xaml" — i.e. **that override is a XAML/WinUI mechanism, not available to a raw Win32 popup.**
  https://learn.microsoft.com/en-us/windows/apps/design/style/rounded-corner
  => `DWMWCP_ROUND` ≈ **8 px** at 96 DPI, `DWMWCP_ROUNDSMALL` ≈ 4 px. (The docs give the design
  values, not a formal DWM guarantee of exactly 8.0 device-independent px — treat the exact number as
  strong-but-inferred.)
- **Arbitrary radius: not supported.** `DwmSetWindowAttribute` only accepts the four enum values.
  Community projects have to work around it: DWMBlurGlass issue #131 ("Rounded corners don't work
  anymore as of 2.0.0") describes a config `extendRound=10` that stopped doing anything, and confirms
  "There is a default radius (8px) when you enable the effect to extend into borders".
  https://github.com/Maplespe/DWMBlurGlass/issues/131
  To get 10 px you must either accept 8 px, or stop using DWM rounding and (a) apply your own
  `SetWindowRgn`/`CreateRoundRectRgn` mask, or (b) draw the rounded corners yourself in your layered
  bitmap (which for a `WS_EX_LAYERED` + `UpdateLayeredWindow` window is natural — your alpha channel
  already defines the silhouette). Note DWMBlurGlass itself hooks DWM to get arbitrary radii, which is
  not something an injected mod should rely on.
- Whether DWM rounds a **`WS_EX_LAYERED`** window: the docs do not restrict it by layering, and
  `taskbar-background-helper`/`translucent-windows` apply `DWMWA_WINDOW_CORNER_PREFERENCE` to shell
  windows. But for a per-pixel-alpha `UpdateLayeredWindow` window you already control the silhouette,
  so DWM rounding is largely redundant there. Treat "DWM rounds my layered popup" as **unverified**;
  the self-drawn rounded alpha is the reliable route.

## 6. WS_EX_NOREDIRECTIONBITMAP and DirectComposition

VERIFIED / well-sourced:

- `WS_EX_NOREDIRECTIONBITMAP` removes the window's redirection bitmap entirely, leaving only
  composition layers. It is the **recommended** way to make a window transparent for
  `Windows.UI.Composition` backdrop effects:
  "For a Win32 application, the easiest way to see the effect is to give the window a
  `WS_EX_NOREDIRECTIONBITMAP` extended style, which will completely remove a redirection bitmap from
  the window, only leaving composition layers." https://notes.yvt.jp/Desktop-Apps/Enabling-Backdrop-Blur/
- Same page: with `Windows.UI.Composition`, "there are no layers behind the target window. This means
  that to achieve the backdrop blur effect, you have to draw the foreground contents in a child window
  or as a top composition layer." And: **"DirectComposition ... can't be used to achieve the backdrop
  blur effect"** — a Microsoft employee (msftdavid, 2017) said they "do not have any immediate plans
  to add `HostBackdropBrush` to the DirectComposition API set." So plain DirectComposition is a dead
  end for real backdrop blur; you need `Windows.UI.Composition` (`Compositor.CreateBackdropBrush` /
  `CreateHostBackdropBrush`).
- `DWMWA_USE_HOSTBACKDROPBRUSH` docs: "Enables a non-UWP window to use host backdrop brushes. If this
  flag is set, then a Win32 app that calls `Windows::UI::Composition` APIs can build transparency
  effects using the host backdrop brush". Build 22000.
  https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute
- `WS_EX_NOREDIRECTIONBITMAP` is **only compatible with the flip swapchain model and D3D backend**
  (VLC/Qt comment), and it means the window's background brush is not painted — which is precisely why
  acrylic becomes visible instead of a white artifact.
  https://mailman.videolan.org/pipermail/vlc-commits/2025-January/070907.html
- **Is `SetWindowCompositionAttribute` supported/needed on a `WS_EX_NOREDIRECTIONBITMAP` window?**
  Not needed, and mixing it with a redirection-bitmap-free DComp window is not a documented
  combination. Win32Acrylic uses `SetWindowCompositionAttribute` with `ACCENT_ENABLE_HOSTBACKDROP` (5)
  for **Windows 10** and `DWMWA_USE_HOSTBACKDROPBRUSH` for Windows 11 when using host backdrop
  brushes. https://github.com/ALTaleX531/Win32Acrylic
- **Crucially: `WS_EX_NOREDIRECTIONBITMAP` and `UpdateLayeredWindow` are mutually exclusive in
  practice** — `UpdateLayeredWindow` feeds a redirection/layered surface. Do not add
  `WS_EX_NOREDIRECTIONBITMAP` to your existing window.

## 7. What the well-known projects actually do

| Project | Approach | Blurs a standalone layered window? |
|---|---|---|
| **TranslucentTB** | ExplorerTAP injects into explorer.exe and manipulates the live taskbar XAML. `XamlBlurBrush` = `XamlCompositionBrushBase` whose brush is `Compositor.CreateBackdropBrush()` → `GaussianBlurEffect` → `FloodEffect` tint, composited `SOURCE_OVER`; applied as the taskbar background `Shape.Fill`. `SetTaskbarAppearance` uses a real `xaml::Media::AcrylicBrush` with `BackgroundSource::Backdrop`, with the comment that on the taskbar **Backdrop (not HostBackdrop) still shows what's behind and does not disable itself when the window isn't active**. | **No** — no standalone window; it styles existing XAML in another process. |
| **Windows 11 Taskbar Styler (Windhawk)** | Its `WindhawkBlur` object is explicitly "based on XamlBlurBrush from the TranslucentTB project", exposed as an XAML brush with `BlurAmount`/`TintColor`/`TintOpacity`/`NoiseOpacity`/`FallbackColor`; `FallbackColor` "is used in place of the blur effect when battery saver is on or when transparency effects are disabled". Also warns it and TranslucentTB/ExplorerBlurMica conflict, since "there can only be one XAML diagnostics consumer at a time". Requires `>= 22621` for SystemBackdrop effects. | **No** — XAML brush inside explorer's taskbar tree. |
| **ExplorerBlurMica** | Explorer-only. Config `effect=0 Blur / 1 Acrylic / 2 Mica / 3 Blur(Clear) / 4 MicaAlt`, and documents: "**Blur is only available until win11 22h2**, Blur (Clear) is available in both win10 and win11, Mica is win11 only." Companion projects: DWMBlurGlass (global title bars) and it points to DWMBlurGlass for global effects. | **No** — explorer windows. |
| **DWMBlurGlass / AcrylicEverywhere / OpenGlass** | Reverse-engineer and hook **DWM itself** to implement a custom blur. "We reverse-analyzed DWM and created a custom blur method"; three methods `CustomBlur`, `AccentBlur`, `SystemBackdrop`; `SystemBackdrop` "uses the system's publicly available interfaces and has the same effect as MicaForEveryone". Supports Win10 2004 → latest Win11. OpenGlass lists Win11 21H2–25H2 (22000–26200) **Stable**, plus 26H2 (26300) stable and 26H1 (28000) experimental. | N/A — it changes the compositor, not an app. |
| **`dynamic-island-for-windows` (Windhawk)** | **The one that does exactly your thing.** Standalone `WS_POPUP` + `WS_EX_LAYERED\|TOPMOST\|TOOLWINDOW\|NOACTIVATE\|TRANSPARENT` window rendered with `UpdateLayeredWindow(..., ULW_ALPHA)` (D2D onto a DC-bound target), plus `SetWindowCompositionAttribute`/`WCA_ACCENT_POLICY` with `ACCENT_ENABLE_BLURBEHIND` (Blur) or `ACCENT_ENABLE_ACRYLICBLURBEHIND` (Acrylic, ABGR tint in the policy). Caps its own fill alpha and skips shadow/scrim when a backdrop is on. Clips the backdrop to the pill silhouette with `SetWindowRgn`. | **Yes — verified working design.** |
| **`translucent-windows` (Windhawk)** | `SetWindowCompositionAttribute` accent blur + optional `DWMWA_SYSTEMBACKDROP_TYPE` `MAINWINDOW`/`TRANSIENTWINDOW`/`TABBEDWINDOW`; notes `ACCENT_ENABLE_BLURBEHIND` removed on 22H2+, and adds `ACCENT_FLAG_ENABLE_MODERN_ACRYLIC_RECIPE` for 22H2+. | Applies to other apps' windows (injected broadly). |
| **`taskbar-background-helper` (Windhawk, v1.2, 2026-01-01)** | Hooks `SetWindowCompositionAttribute` in explorer.exe and sets accent policy on `Shell_TrayWnd`/`Shell_SecondaryTrayWnd`. Direct proof accent blur still functions on current Windows 11. | Applies to the taskbar HWND. |
| **Win32Acrylic / Qt / FluentFlyout** | `Windows.UI.Composition` effect graph + `CreateBackdropBrush`/`HostBackdropBrush` + `DWMWA_USE_HOSTBACKDROPBRUSH` (Win11) / `ACCENT_ENABLE_HOSTBACKDROP` (Win10); FluentFlyout uses the accent API from WPF. | Qt requires frameless or `WS_EX_NOREDIRECTIONBITMAP`. |

**No Windhawk mod other than `dynamic-island-for-windows` blurs a standalone layered top-level
window.** That is the reference implementation to copy.

## 8. Practical architecture answer (Q5)

Yes to both halves, with the qualifications from §3:

1. **Window A (layered, per-pixel alpha) over window B (acrylic, no per-pixel alpha):** A's
   translucent pixels will reveal B, because DWM composites A over B and A's alpha is honoured
   (`ULW_ALPHA`/`AC_SRC_ALPHA`). Technically true, but this two-window design is *unnecessary* —
   `dynamic-island-for-windows` proves you can get blur on window A directly.
2. **Will B's blur sample the desktop/taskbar under it?** Yes, if B's backdrop target really is the
   desktop. That requires the cross-window sampling path: `ACCENT_ENABLE_ACRYLICBLURBEHIND`, or
   `DWMWA_USE_HOSTBACKDROPBRUSH` + `CreateHostBackdropBrush`. With plain `CreateBackdropBrush` inside
   a window, the sampled "backdrop" is behind the *visual*, and per the yvt.jp finding, if the window
   is opaque/redirection-surface-based there is nothing valid behind it. Also note `HostBackdropBrush`
   "samples the area behind the window ... and rendered it to the redirection surface" and "for
   security reasons ... has a certain blur amount itself" (Win32Acrylic) — you cannot get a zero-blur
   host backdrop.
3. **Z-order / stacking pitfalls (the real risks):**
   - Two independent topmost windows have no reliable relative order; `WS_EX_TOPMOST` is a single
     band. `dynamic-island-for-windows` contains an entire careful z-order reclamation routine
     (`IsShellOwnedWindow`, "Everything above a topmost window is itself topmost...") precisely to
     fight this — including giving up rather than raising itself above Start, notification centre,
     volume OSD, Task View, menus and tooltips. **Expect to write that code.**
   - Do not parent your popup into the desktop `WorkerW`/`SHELLDLL_DefView` hierarchy — blur cannot be
     created on such a window (dotnet/wpf#3608). `dynamic-island-for-windows` only reparents to
     `Progman` when `alwaysOnTop == false`, and uses `GWLP_HWNDPARENT` with the HWND as *owner*.
   - Blurred windows flicker/refresh oddly on resize and when ≥2 blurred windows share a monitor
     (dotnet/wpf#3608, Composition-Win32-Samples#66). Since you re-blit via `UpdateLayeredWindow`,
     keep the window size stable and avoid per-frame resizes.
   - Blur can be dropped when the window loses focus (this is by design for Mica/background acrylic:
     "only background acrylic will replace its translucency and texture with a solid color: When an
     app window on desktop deactivates"). A `WS_EX_NOACTIVATE` popup may therefore be *permanently*
     in the deactivated state — **test this specifically**; it is the single biggest risk to your
     design that I could not fully resolve from sources.
     https://learn.microsoft.com/en-us/windows/apps/design/style/acrylic

## 9. Recommendation

Keep `WS_EX_LAYERED` + `UpdateLayeredWindow`; drop the notion of a second window B; add
`SetWindowCompositionAttribute` with `WCA_ACCENT_POLICY`:
- `ACCENT_ENABLE_ACRYLICBLURBEHIND` (4) with an ABGR `GradientColor` for frosted glass, or
- `ACCENT_ENABLE_BLURBEHIND` (3) for plain blur (avoid on 22H2+ — it is the value reported as removed).

Resolve the export dynamically via `GetProcAddress(GetModuleHandle(L"user32.dll"),
"SetWindowCompositionAttribute")` and fall back to an opaque fill. Cap your own panel fill alpha
(give alpha < 1 to the pixels where you want glass) and skip any scrim/shadow that would sit over the
backdrop. Clip with `SetWindowRgn` to the rounded silhouette if you want the blur confined to a
rounded pill. Handle the battery-saver / transparency-off / RDP fallback explicitly.

Do **not** reach for `DWMWA_SYSTEMBACKDROP_TYPE` on this window: it is for redirection-surface-free
(DComp) windows, and combining it with `UpdateLayeredWindow` is unverified.

For arbitrary corner radii, draw the rounded corners in your own alpha mask rather than trying to make
DWM do 10 px.

# HdrScreenshotSyncer

[![Windows build](https://github.com/mangokingTW/HdrScreenshotSyncer/actions/workflows/windows-build.yml/badge.svg)](https://github.com/mangokingTW/HdrScreenshotSyncer/actions/workflows/windows-build.yml)
[![CodeQL](https://github.com/mangokingTW/HdrScreenshotSyncer/actions/workflows/codeql.yml/badge.svg)](https://github.com/mangokingTW/HdrScreenshotSyncer/actions/workflows/codeql.yml)
[![OpenSSF Scorecard](https://api.scorecard.dev/projects/github.com/mangokingTW/HdrScreenshotSyncer/badge)](https://scorecard.dev/viewer/?uri=github.com/mangokingTW/HdrScreenshotSyncer)
[![Latest release](https://img.shields.io/github/v/release/mangokingTW/HdrScreenshotSyncer?sort=semver)](https://github.com/mangokingTW/HdrScreenshotSyncer/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/mangokingTW/HdrScreenshotSyncer/total)](https://github.com/mangokingTW/HdrScreenshotSyncer/releases)
[![License: MIT](https://img.shields.io/github/license/mangokingTW/HdrScreenshotSyncer)](LICENSE)

A small Windows tray tool that keeps Snipping Tool's **HDR screenshot color
corrector** (`IsHDRToneMappingEnabled`) in sync with whether the display is
currently in HDR — so native screenshots come out right without toggling the
setting by hand. Windows switches HDR on and off per app, so you can't tell the
current state; this follows it automatically.

Usage guide (install, tray menu, App overrides) is in the
**[wiki](https://github.com/mangokingTW/HdrScreenshotSyncer/wiki)**.

## How it works

The app sits in the system tray and re-evaluates on events rather than a fixed
poll: a worker thread blocks on the next desktop frame via Desktop Duplication
(so a changing screen wakes it and a static one sleeps in the kernel), and a
wake event nudges it on app switches (`EVENT_SYSTEM_FOREGROUND`) and display
changes (`WM_DISPLAYCHANGE`). When the decision changes it writes
`IsHDRToneMappingEnabled` to match, through `RegLoadAppKey` (no admin), retrying
if Snipping Tool is holding the hive. Scans are throttled so a playing video
doesn't cause constant work.

Deciding "is this HDR" is the interesting part. Windows exposes **no** per-window
or per-app HDR API, and on a display left in HDR permanently the display-level
flag (`DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO`, `advancedColorEnabled`) never
changes — so it can't tell an SDR app from an HDR one. The only externally
observable signal is the pixels themselves: with HDR on, Windows composites the
desktop into an scRGB FP16 framebuffer where SDR content tops out at SDR white
and HDR highlights go brighter. So the app captures the foreground window's
region via Desktop Duplication (`IDXGIOutput5::DuplicateOutput1` at
`R16G16B16A16_FLOAT`) and checks whether enough pixels exceed SDR white
(read from `DISPLAYCONFIG_SDR_WHITE_LEVEL`). If the display isn't in HDR at all,
the corrector is simply off.

This is content-based, not app-based: it detects HDR pixels in the foreground
window, so an SDR window showing an embedded HDR video reads as HDR. When a frame
can't be captured (a fullscreen-exclusive game, protected content), it keeps the
last decision rather than guess. It also freezes the decision while Snipping Tool
itself is foreground, so its translucent capture overlay can't flip the setting
just as the shot is taken.

The tray menu has Enabled, Sync now, Start at logon, App overrides, and Exit —
localized in English and Traditional Chinese. No elevation, no network, no data
collection.

**App overrides.** A few apps present an HDR swapchain whose need for the
corrector isn't visible in the captured pixels (it depends on the app's own GPU
presentation, which no API exposes). "App overrides…" opens a small dialog where
you force a process by name to HDR or SDR — skipping pixel detection.

One caveat: some apps (e.g. **Discord with hardware acceleration on**) look wrong
with *both* corrector states — too bright with it off, too dark with it on — so
no override can fix them. Turning off that app's hardware acceleration makes it
plain SDR and correct.

## Install

| Tool | Command |
|---|---|
| **Scoop** | `scoop bucket add mango https://github.com/mangokingTW/scoop-bucket`<br>`scoop install mango/HdrScreenshotSyncer` |
| **winget** | `winget install mangokingTW.HdrScreenshotSyncer` |
| **Chocolatey** | `choco install hdrscreenshotsyncer` |
| **Microsoft Store** | search for *HdrScreenshotSyncer* |

Or download the installer / portable zip from
[Releases](https://github.com/mangokingTW/HdrScreenshotSyncer/releases). Scoop is
available now; winget, Chocolatey, and the Microsoft Store listing are pending
review.

## Build

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

Produces `build/Release/HdrScreenshotSyncer.exe`.

The release, packaging, and Microsoft Store publishing flow is documented in
**[docs/PACKAGING.md](docs/PACKAGING.md)**.

## How the setting is stored

Reverse-engineered from Process Monitor:

- It's a registry value in Snipping Tool's `settings.dat`
  (`%LocalAppData%\Packages\Microsoft.ScreenSketch_8wekyb3d8bbwe\Settings\settings.dat`),
  under `LocalState\IsHDRToneMappingEnabled`.
- Registry type `100000011` (`0x5F5E10B`) = UWP LocalSettings Boolean.
- Data is 9 bytes: 1 value byte (`00`/`01`) + an 8-byte FILETIME timestamp.
- The live path is a dynamic mount, so it's written by loading `settings.dat`
  with `RegLoadAppKey` (no admin). `tools/toggle-hdr-tonemap.ps1` is the original
  proof-of-concept that toggles it from the command line.

## Privacy

No data collection, no network connections — see [PRIVACY.md](PRIVACY.md).

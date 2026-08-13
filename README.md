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

## How it works

The app sits in the system tray and, on a light timer and on every
`WM_DISPLAYCHANGE`, decides whether the corrector should be on, then writes
`IsHDRToneMappingEnabled` to match when it differs. The write goes through
`RegLoadAppKey` (no admin) and retries next cycle if Snipping Tool is holding the
hive.

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

The tray menu has Enabled, Sync now, Start at logon, and Exit. No elevation, no
network, no data collection.

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

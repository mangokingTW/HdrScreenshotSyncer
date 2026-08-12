# HdrScreenshotSyncer

A small Windows tray tool that keeps Snipping Tool's **HDR screenshot color
corrector** (`IsHDRToneMappingEnabled`) in sync with whether the display is
currently in HDR — so native screenshots come out right without toggling the
setting by hand. Windows switches HDR on and off per app, so you can't tell the
current state; this follows it automatically.

Separate from ImeModePersistence on purpose (unrelated concern, and that app is
already published / in Store review).

## Status: proving feasibility first

The whole idea depends on one unknown: **does Snipping Tool honor an external
write to its `IsHDRToneMappingEnabled` setting?** Until that's confirmed, nothing
else is worth building.

What we know (from Process Monitor):
- It's a registry value in Snipping Tool's `settings.dat`
  (`%LocalAppData%\Packages\Microsoft.ScreenSketch_8wekyb3d8bbwe\Settings\settings.dat`),
  under `LocalState\IsHDRToneMappingEnabled`.
- Registry type `100000011` (`0x5F5E10B`) = UWP LocalSettings Boolean.
- Data is 9 bytes: 1 value byte (`00`/`01`) + an 8-byte FILETIME timestamp.
- The live path is a dynamic mount, so it's written by loading `settings.dat`
  with `RegLoadAppKey` (no admin) — see `tools/toggle-hdr-tonemap.ps1`.

### The test

Close Snipping Tool (end any background `SnippingTool.exe`), then:

```powershell
powershell -ExecutionPolicy Bypass -File tools\toggle-hdr-tonemap.ps1 on
```

Open Snipping Tool > Settings and check whether the corrector reflects it, and
whether a screenshot of HDR content is corrected. If yes → the tray tool is
viable. If no → the setting is cached in-process and this approach won't work.

## Planned design (only if the test passes)

- Detect the display's HDR state via the CCD APIs
  (`DisplayConfigGetDeviceInfo` + `DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO`),
  re-checking on `WM_DISPLAYCHANGE` and a light poll.
- On change, write `IsHDRToneMappingEnabled` to match (true in HDR, false in SDR)
  via `RegLoadAppKey`, when Snipping Tool isn't holding the hive.
- Tray toggle + optional autostart; same MIT license, CI and packaging approach
  as ImeModePersistence.

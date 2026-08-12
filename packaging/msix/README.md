# Microsoft Store (MSIX) build

Packages HdrScreenshotSyncer as a full-trust desktop MSIX for the Microsoft
Store. The tray app is identical to the portable build; only how it starts at
logon differs (a package StartupTask instead of an HKCU Run entry).

## One-time setup

1. Reserve the app name in Partner Center and open **Product > Product identity**.
2. Copy the three values into `AppxManifest.xml`:

   | Partner Center             | AppxManifest.xml                          |
   |----------------------------|-------------------------------------------|
   | Name                       | `Package/Identity/@Name`                  |
   | Publisher (`CN=...`)       | `Package/Identity/@Publisher`             |
   | Publisher display name     | `Package/Properties/PublisherDisplayName` |

   Keep the version's fourth field `0` (the Store rejects a non-zero revision).

## Build

Run each line on its own in Windows PowerShell (5.1 has no `&&`):

```powershell
cmake -S . -B build-x64 -A x64
cmake --build build-x64 --config Release
powershell -ExecutionPolicy Bypass -File packaging\msix\build.ps1
```

This writes `dist\HdrScreenshotSyncer.msix`. Upload that in Partner Center — the
Store signs it, so no certificate is needed for submission.

## Sideload test before submitting

The Store package is signed by Microsoft, but to run it locally first, sign the
`.msix` with a self-signed cert whose subject matches `Identity/@Publisher`
exactly, then `Add-AppxPackage`. Confirm the one behaviour that differs under a
package: the tray app writes Snipping Tool's `IsHDRToneMappingEnabled` via
`RegLoadAppKey` on `settings.dat`. That is an ordinary per-user file operation
with no elevation, so a full-trust package should be able to do it — verify it
does before submitting: toggle the display's HDR and confirm Snipping Tool's
"HDR screenshot color corrector" follows.

## Autostart

The MSIX build carries a `windows.startupTask` (disabled by default). The user
turns it on under **Settings > Apps > Startup**. The tray menu's "Start at logon"
item is hidden in this build, since the Run key it would write is virtualized to
no effect inside a package.

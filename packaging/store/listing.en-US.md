# Microsoft Store listing copy — English (en-US)

> **Purpose:** This file is the `en-us` Store listing copy for **HDR Screenshot Syncer**, kept under version control to mirror the sibling `listing.zh-TW.md`. The app already has a live `en-us` listing; this records the canonical English text.
>
> **Note:** The **Description** below is a **static listing field** and is maintained by hand in Partner Center. The per-version **"What's new"** (release notes) is set automatically by the release workflow from each version's CHANGELOG section — it is not written by hand here.

---

## 1. Title

**HDR Screenshot Syncer**

Keep the English brand name un-localized. It is the existing product / package name (package id, Store ID, and install commands all use it), so keeping it as-is preserves cross-language consistency and search recognition. The app's own UI already switches to Traditional Chinese based on your Windows display language, so the title needs no separate translation.

---

## 2. Description

A tiny system-tray tool that automatically keeps Windows Snipping Tool's "HDR screenshot color corrector" (`IsHDRToneMappingEnabled`) in sync with whether the **foreground content is actually HDR** — so native screenshots come out with correct colors on an HDR display, without toggling that setting by hand every time.

Windows enables HDR **per app**, and it exposes no API to tell you whether the current window is HDR — so you can't know the current state, and on a display left permanently in HDR the display-level flag never changes at all. So this tool looks at the **actual pixels** instead: it captures the foreground window through Desktop Duplication in scRGB FP16 and checks whether enough pixels exceed SDR white (bright highlights / wide gamut), which distinguishes SDR from HDR content, and writes the corrector setting to match. This is **content-based (pixel-based), not the display's HDR flag** — for example, an SDR window playing an embedded HDR video is correctly detected as HDR. If the display isn't in HDR at all, the corrector simply stays off.

It is **event-driven** — it re-evaluates when you switch windows or the display state changes, rather than polling continuously, and pure-black frames are ignored. The decision is **remembered per window**, and is **held while Snipping Tool is in the foreground**, so its translucent capture overlay can't flip the setting at the very moment you take the shot. When a frame can't be captured (a fullscreen-exclusive game, protected content), it keeps the last decision rather than guessing.

**Per-app overrides.** A few apps present HDR in a way the captured pixels can't reveal (it depends on the app's own GPU presentation, which no API exposes). For those, open the "App overrides" dialog from the tray menu and **force a specific process by name to HDR or SDR**, skipping pixel detection.

**Known caveat.** Some apps (for example **Discord with hardware acceleration on**) look wrong with the corrector either way — too bright when it's off, too dark when it's on — so no override can fix them. Instead, **turn off that app's hardware acceleration**; it then falls back to plain SDR and displays correctly.

The interface is available in English and Traditional Chinese, chosen automatically by your Windows display language. **No administrator rights, no network connections, no data collection.**

**Highlights**

- Automatically keeps Snipping Tool's "HDR screenshot color corrector" in sync with whether the foreground content is HDR, so screenshots come out with correct colors.
- Decides from the **actual pixels** (bright highlights / wide gamut), not the display's HDR flag; ignores pure-black frames.
- Remembers the HDR decision per window; holds it while Snipping Tool is in the foreground; keeps the last decision when it can't capture a frame.
- **Event-driven** — reacts to focus / HDR changes instead of polling.
- **Per-app overrides:** when detection can't tell, force a specific app to HDR or SDR by process name.
- Lives in the system tray; English and Traditional Chinese UI, chosen by your Windows display language.
- **No administrator rights, no network, no data collection.**

---

## 3. (Optional) Short description / search keywords

**Short description (one line):**
Automatically keeps Snipping Tool's HDR screenshot color corrector in sync with the foreground content, so screenshots come out with correct colors on an HDR display.

**Suggested search keywords:**
HDR, screenshot, screen capture, Snipping Tool, color correction, HDR screenshot, tone mapping, tray tool

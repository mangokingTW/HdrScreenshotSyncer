#pragma once

#include <optional>

namespace hdr {

// Filled in by foreground_has_hdr_content when a diagnostics sink is passed, so
// the tray tool can log why it decided as it did while tuning the thresholds.
struct ScanDiag {
    const wchar_t* status = L"";  // "ok", "sdr-output", "acquire-timeout", ...
    long hr = 0;                  // HRESULT for the failing step, when relevant
    float sdrWhite = 0.0f;        // SDR white in scRGB (the reference the scan compares against)
    float threshold = 0.0f;       // brightness a pixel must exceed to count as HDR
    float maxChannel = 0.0f;      // brightest channel seen in the scanned region
    float minChannel = 0.0f;      // most-negative channel seen (below 0 => outside sRGB / wide gamut)
    double hotFraction = 0.0;     // fraction of sampled pixels above the threshold
};

// Whether the foreground window's on-screen content is actually HDR, decided by
// capturing the composited framebuffer and scanning for pixels brighter than
// SDR white (in scRGB). This is the only externally observable HDR signal:
// Windows exposes no per-window/per-app HDR API, and on a display left in HDR
// permanently the display-level flag never changes, so `any_display_on()` alone
// can't tell an SDR app from an HDR one.
//
// Returns nullopt when it can't tell this cycle -- no foreground window, or the
// frame can't be captured (a fullscreen-exclusive game, protected content, a
// transient access loss). The caller should keep its previous decision then
// rather than flip the setting on missing data.
//
// If diag is non-null it is filled with the measured values for logging.
// acquireTimeoutMs is how long to block waiting for the next desktop frame; a
// larger value lets a worker thread sleep on screen updates instead of polling.
std::optional<bool> foreground_has_hdr_content(ScanDiag* diag = nullptr,
                                               unsigned long acquireTimeoutMs = 100);

} // namespace hdr

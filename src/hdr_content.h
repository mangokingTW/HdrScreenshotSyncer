#pragma once

#include <optional>
#include <string>
#include <vector>

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

// A per-app override rule: force `exe` (process image name, e.g. "Discord.exe")
// to HDR (corrector on) or SDR, skipping pixel detection -- the only reliable
// signal for apps whose HDR presentation doesn't show up in the captured pixels.
struct AppRule {
    std::wstring exe;
    bool hdr;
};

// The override file's rules (read fresh). The settings dialog uses these; the
// detection worker keeps its own copy and re-reads the file when it changes.
std::vector<AppRule> list_overrides();

// Add or update (case-insensitive on exe), then write the file.
void set_override(const std::wstring& exe, bool hdr);

// Remove the rule for exe (if any), then write the file.
void remove_override(const std::wstring& exe);

// Path to the override file (%LOCALAPPDATA%\HdrScreenshotSyncer\overrides.txt).
std::wstring overrides_file_path();

} // namespace hdr

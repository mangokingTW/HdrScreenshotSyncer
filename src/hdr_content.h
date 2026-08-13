#pragma once

#include <optional>

namespace hdr {

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
std::optional<bool> foreground_has_hdr_content();

} // namespace hdr

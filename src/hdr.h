#pragma once

// Whether any currently-active display has HDR (advanced color) turned on.
// Windows flips this per app, so it is polled and also re-checked on
// WM_DISPLAYCHANGE.
namespace hdr {

bool any_display_on();

} // namespace hdr

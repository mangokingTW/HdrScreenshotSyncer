#pragma once

#include <windows.h>

namespace overrides {

// Modal per-app override editor (list + add/remove, localized). Reads and writes
// the same overrides file the detection worker watches.
void show(HINSTANCE instance, HWND owner);

}  // namespace overrides

#pragma once

#include <windows.h>

#include <string>

namespace overrides {

// Modal per-app override editor (list + add/remove, localized). Reads and writes
// the same overrides file the detection worker watches. lastApp is the executable
// name of the last external foreground window, offered by the "Use last app"
// button (empty disables it).
void show(HINSTANCE instance, HWND owner, const std::wstring& lastApp);

}  // namespace overrides

#include "snip_setting.h"

#include <windows.h>

#include <cstring>
#include <string>

namespace snip {
namespace {

// UWP LocalSettings store a Boolean as registry type 0x5F5E10B, with 9 bytes of
// data: one value byte followed by an 8-byte FILETIME timestamp. (Confirmed with
// Process Monitor against Snipping Tool.)
constexpr DWORD kUwpBooleanType = 0x5F5E10B;
constexpr wchar_t kValueName[] = L"IsHDRToneMappingEnabled";

std::wstring settings_path() {
    wchar_t base[MAX_PATH]{};
    const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", base, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return {};
    }
    return std::wstring(base) +
           L"\\Packages\\Microsoft.ScreenSketch_8wekyb3d8bbwe\\Settings\\settings.dat";
}

} // namespace

std::optional<bool> read() {
    const std::wstring dat = settings_path();
    if (dat.empty()) {
        return std::nullopt;
    }

    HKEY root{};
    // A private, per-process load of the hive file -- no administrator rights,
    // unlike RegLoadKey. Fails if Snipping Tool holds the file exclusively.
    if (RegLoadAppKeyW(dat.c_str(), &root, KEY_READ, 0, 0) != ERROR_SUCCESS) {
        return std::nullopt;
    }

    std::optional<bool> result;
    HKEY local{};
    if (RegOpenKeyExW(root, L"LocalState", 0, KEY_READ, &local) == ERROR_SUCCESS) {
        BYTE buffer[16]{};
        DWORD size = sizeof(buffer);
        DWORD type = 0;
        if (RegQueryValueExW(local, kValueName, nullptr, &type, buffer, &size) == ERROR_SUCCESS &&
            size >= 1) {
            result = buffer[0] != 0;
        }
        RegCloseKey(local);
    }
    RegCloseKey(root);
    return result;
}

bool write(bool enabled) {
    const std::wstring dat = settings_path();
    if (dat.empty()) {
        return false;
    }

    HKEY root{};
    if (RegLoadAppKeyW(dat.c_str(), &root, KEY_ALL_ACCESS, 0, 0) != ERROR_SUCCESS) {
        return false; // Snipping Tool is holding the hive; try again next cycle.
    }

    bool ok = false;
    HKEY local{};
    if (RegOpenKeyExW(root, L"LocalState", 0, KEY_SET_VALUE, &local) == ERROR_SUCCESS) {
        BYTE data[9]{};
        data[0] = enabled ? 1 : 0;
        FILETIME now{};
        GetSystemTimeAsFileTime(&now);
        std::memcpy(data + 1, &now, sizeof(now)); // 8-byte timestamp

        ok = RegSetValueExW(local, kValueName, 0, kUwpBooleanType, data,
                            static_cast<DWORD>(sizeof(data))) == ERROR_SUCCESS;
        RegCloseKey(local);
    }
    RegCloseKey(root);
    return ok;
}

} // namespace snip

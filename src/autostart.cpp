#include "autostart.h"

#include <windows.h>

#include <appmodel.h>
#include <shellapi.h>

#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.h>

#include <string>
#include <thread>

namespace autostart {
namespace {

constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kValueName[] = L"HdrScreenshotSyncer";

std::wstring module_path() {
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        const DWORD written =
            GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (written == 0) {
            return {};
        }
        if (written < path.size()) {
            path.resize(written);
            return path;
        }
        if (path.size() >= 32768) {
            return {};
        }
        path.resize(path.size() * 2);
    }
}

// Quoted so a path with spaces survives the shell's command-line parsing.
std::wstring launch_command() {
    const std::wstring path = module_path();
    return path.empty() ? std::wstring{} : L'"' + path + L'"';
}

std::wstring read_value() {
    DWORD bytes = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, kRunKey, kValueName, RRF_RT_REG_SZ, nullptr, nullptr,
                     &bytes) != ERROR_SUCCESS ||
        bytes < sizeof(wchar_t)) {
        return {};
    }
    std::wstring value(bytes / sizeof(wchar_t), L'\0');
    if (RegGetValueW(HKEY_CURRENT_USER, kRunKey, kValueName, RRF_RT_REG_SZ, nullptr, value.data(),
                     &bytes) != ERROR_SUCCESS) {
        return {};
    }
    value.resize(bytes / sizeof(wchar_t));
    while (!value.empty() && value.back() == L'\0') {
        value.pop_back();
    }
    return value;
}

// Runs `work` on a dedicated multithreaded-apartment thread and waits for it.
// The package StartupTask's WinRT calls must not run on the tray's
// single-threaded apartment -- doing so faults in combase.dll -- so a
// short-lived MTA thread runs them instead; the UI STA is left untouched.
template <typename F>
void run_on_mta(F&& work) {
    std::thread worker([&] {
        if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
            return;
        }
        try {
            work();
        } catch (...) {
        }
        CoUninitialize();
    });
    worker.join();
}

// Matches the uap5:StartupTask TaskId in packaging/msix/AppxManifest.xml.
constexpr wchar_t kStartupTaskId[] = L"HdrScreenshotSyncerStartup";

// Reads the package StartupTask state (MSIX build): enabled if on, false on any
// failure so it never throws out of a menu handler.
bool startup_task_enabled() {
    bool result = false;
    run_on_mta([&result] {
        using namespace winrt::Windows::ApplicationModel;
        const StartupTask task = StartupTask::GetAsync(kStartupTaskId).get();
        const StartupTaskState state = task.State();
        result = state == StartupTaskState::Enabled || state == StartupTaskState::EnabledByPolicy;
    });
    return result;
}

} // namespace

bool packaged() {
    static const bool result = [] {
        UINT32 length = 0;
        return GetCurrentPackageFullName(&length, nullptr) != APPMODEL_ERROR_NO_PACKAGE;
    }();
    return result;
}

bool enabled() {
    if (packaged()) {
        return startup_task_enabled(); // MSIX build: read the package StartupTask.
    }
    const std::wstring expected = launch_command();
    const std::wstring actual = read_value();
    if (expected.empty() || actual.empty()) {
        return false;
    }
    return CompareStringOrdinal(actual.c_str(), -1, expected.c_str(), -1, TRUE) == CSTR_EQUAL;
}

bool set_enabled(bool on) {
    if (packaged()) {
        // MSIX build: the StartupTask is toggled in Windows Settings (the tray
        // menu opens it), not from code. Reached only defensively.
        return false;
    }
    if (!on) {
        const LSTATUS status = RegDeleteKeyValueW(HKEY_CURRENT_USER, kRunKey, kValueName);
        return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
    }

    const std::wstring command = launch_command();
    if (command.empty()) {
        return false;
    }

    HKEY key{};
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key,
                        nullptr) != ERROR_SUCCESS) {
        return false;
    }
    const DWORD bytes = static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t));
    const LSTATUS status = RegSetValueExW(key, kValueName, 0, REG_SZ,
                                          reinterpret_cast<const BYTE*>(command.c_str()), bytes);
    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

void open_startup_settings() {
    ShellExecuteW(nullptr, L"open", L"ms-settings:startupapps", nullptr, nullptr, SW_SHOWNORMAL);
}

} // namespace autostart

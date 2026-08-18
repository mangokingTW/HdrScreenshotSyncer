#include <windows.h>

#include <shellapi.h>

#include <atomic>
#include <optional>
#include <string>

#include "autostart.h"
#include "diagnostic.h"
#include "hdr.h"
#include "hdr_content.h"
#include "overrides_dialog.h"
#include "resource.h"
#include "snip_setting.h"
#include "strings.h"

namespace {

constexpr UINT WMAPP_TRAY = WM_APP + 1;
constexpr UINT ID_ENABLED = 1001;
constexpr UINT ID_SYNC_NOW = 1002;
constexpr UINT ID_AUTOSTART = 1003;
constexpr UINT ID_EXIT = 1004;
constexpr UINT ID_OPENLOG = 1005;
constexpr UINT ID_OVERRIDES = 1006;
constexpr UINT ID_STATE_AUTO = 1007;
constexpr UINT ID_STATE_HDR = 1008;
constexpr UINT ID_STATE_SDR = 1009;
// How long a scan blocks waiting for the next desktop frame (so the worker
// sleeps on screen updates), the minimum gap between scans while content keeps
// changing (throttle for video), and how long to idle when there's nothing to
// watch (disabled, or no HDR display).
constexpr unsigned long kAcquireMs = 700;
// Minimum gap between scans while the screen keeps updating. Kept fairly high so
// incidental updates (a blinking caret, a clock) don't cause constant scanning;
// app switches still react instantly via the foreground hook's wake.
constexpr DWORD kThrottleMs = 2000;
constexpr DWORD kIdleWaitMs = 1000;
constexpr wchar_t kClassName[] = L"HdrScreenshotSyncerWindow";
constexpr wchar_t kSingleInstanceMutex[] = L"Local\\HdrScreenshotSyncer.SingleInstance";

HWND g_hwnd{};
HINSTANCE g_instance{};
NOTIFYICONDATAW g_tray{};
std::atomic<bool> g_enabled{true};
// Correction-state override: 0 = automatic detection, 1 = force HDR (corrector
// on), 2 = force SDR (corrector off).
std::atomic<int> g_force{0};
std::atomic<bool> g_running{true};
HANDLE g_singleInstance{};
HANDLE g_wake{};              // signalled to nudge the worker out of its wait
HANDLE g_worker{};
HWINEVENTHOOK g_foregroundHook{};

// Executable base name of the last external foreground window, offered to the
// overrides dialog's "Use last app" button. Touched only on the UI thread (the
// OUTOFCONTEXT hook callback and the menu handler), so it needs no lock.
std::wstring g_lastForegroundExe;

// Nudge the worker to re-evaluate now. Safe before the event exists / if it
// failed to create.
void wake() {
    if (g_wake) {
        SetEvent(g_wake);
    }
}

// Base executable name (e.g. "notepad.exe") of a window's process, or empty for
// our own process, the shell, or on failure. Used to record the last external
// foreground app. The shell (explorer.exe -- taskbar, desktop, notification
// area) is excluded because clicking the tray icon makes it the foreground for an
// instant; without this the "last app" would always be explorer.exe.
std::wstring foreground_exe(HWND hwnd) {
    if (!hwnd) {
        return {};
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0 || pid == GetCurrentProcessId()) {
        return {};
    }
    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!proc) {
        return {};
    }
    wchar_t path[MAX_PATH]{};
    DWORD size = ARRAYSIZE(path);
    const bool ok = QueryFullProcessImageNameW(proc, 0, path, &size) != FALSE;
    CloseHandle(proc);
    if (!ok) {
        return {};
    }
    const wchar_t* base = path;
    for (const wchar_t* p = path; *p; ++p) {
        if (*p == L'\\' || *p == L'/') {
            base = p + 1;
        }
    }
    if (lstrcmpiW(base, L"explorer.exe") == 0) {
        return {};  // the shell; see the comment above
    }
    return base;
}

void set_tray_icon(bool add) {
    g_tray.cbSize = sizeof(g_tray);
    g_tray.hWnd = g_hwnd;
    g_tray.uID = 1;
    if (add) {
        g_tray.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        g_tray.uCallbackMessage = WMAPP_TRAY;
        g_tray.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APPICON));
        if (!lstrcpynW(g_tray.szTip, text::s().trayTip, ARRAYSIZE(g_tray.szTip))) {
            g_tray.szTip[0] = L'\0';
        }
        Shell_NotifyIconW(NIM_ADD, &g_tray);
    } else {
        Shell_NotifyIconW(NIM_DELETE, &g_tray);
    }
}

// Match Snipping Tool's HDR screenshot corrector to whether the content being
// looked at is actually HDR. If no display is in HDR at all, the corrector isn't
// needed (SDR capture). Otherwise decide by scanning the foreground window's
// pixels -- the display-level flag is useless when HDR is left on permanently,
// since it never changes while apps switch between SDR and HDR content.
//
// Only writes when the setting differs, and a write may fail if Snipping Tool
// holds the hive -- the next cycle retries. When the content can't be read this
// cycle, keep the current setting rather than flip it on missing data. Runs on
// the worker thread; returns whether a display is in HDR so the worker can pace
// itself. The scan blocks up to kAcquireMs waiting for a desktop frame, so a
// static screen costs nothing beyond that kernel wait.
bool evaluate_and_apply() {
    const int force = g_force.load();
    const bool displayHdr = hdr::any_display_on();

    // Manual override: apply the forced state and skip detection entirely. Still
    // reports whether a display is in HDR so the worker paces itself the same way.
    if (force != 0) {
        const bool want = (force == 1);
        bool wrote = false;
        const std::optional<bool> current = snip::read();
        if (!current.has_value() || current.value() != want) {
            snip::write(want);
            wrote = true;
        }
        if (wrote) {
            diag::write(L"forced=%s | display_hdr=%d | wrote=1", want ? L"HDR" : L"SDR",
                        displayHdr ? 1 : 0);
        } else {
            // Steady state under a manual override: log once per distinct state.
            diag::write_once(L"forced=%s | display_hdr=%d | steady", want ? L"HDR" : L"SDR",
                             displayHdr ? 1 : 0);
        }
        return displayHdr;
    }

    bool want = false;
    bool decided = true;
    hdr::ScanDiag diag;
    if (displayHdr) {
        const std::optional<bool> content =
            hdr::foreground_has_hdr_content(&diag, kAcquireMs);
        if (content.has_value()) {
            want = content.value();
        } else {
            decided = false;
        }
    }

    bool wrote = false;
    if (decided) {
        const std::optional<bool> current = snip::read();
        if (!current.has_value() || current.value() != want) {
            snip::write(want);
            wrote = true;
        }
    }

    // Event-driven logging, like the IME tool: a real transition is logged in
    // full every time (the interesting moment, with the measurements that explain
    // it), while a steady state is logged once per distinct (app, outcome) via
    // write_once. The scan measurements fluctuate every frame, so they are left
    // out of the steady-state line -- included, write_once would never dedupe and
    // the log would fill with near-identical repeats. diag:: prepends timestamps.
    wchar_t title[128]{};
    GetWindowTextW(GetForegroundWindow(), title, ARRAYSIZE(title));
    if (wrote) {
        wchar_t line[600];
        wsprintfW(line,
                  L"fg=\"%s\" | display_hdr=%d | status=%s hr=0x%08x "
                  L"white=%d.%02d thr=%d.%02d max=%d.%02d min=-%d.%03d hot=%d.%03d%% | "
                  L"decided=%d want=%s wrote=1",
                  title, displayHdr ? 1 : 0, (diag.status && *diag.status) ? diag.status : L"-",
                  static_cast<unsigned int>(diag.hr),
                  static_cast<int>(diag.sdrWhite), static_cast<int>(diag.sdrWhite * 100) % 100,
                  static_cast<int>(diag.threshold), static_cast<int>(diag.threshold * 100) % 100,
                  static_cast<int>(diag.maxChannel), static_cast<int>(diag.maxChannel * 100) % 100,
                  static_cast<int>(-diag.minChannel), static_cast<int>(-diag.minChannel * 1000) % 1000,
                  static_cast<int>(diag.hotFraction * 100), static_cast<int>(diag.hotFraction * 100000) % 1000,
                  decided ? 1 : 0, want ? L"HDR" : L"SDR");
        diag::write(L"%s", line);
    } else {
        // The scan measurements are kept here too (not just on transitions): they
        // are what a "why is this SDR?" tuning question needs, and on a steady
        // window they are near-constant so write_once still dedupes to a line or
        // two rather than one per scan.
        diag::write_once(L"fg=\"%s\" | display_hdr=%d | status=%s | "
                         L"max=%d.%02d thr=%d.%02d min=-%d.%03d hot=%d.%03d%% | decided=%d want=%s | no change",
                         title, displayHdr ? 1 : 0,
                         (diag.status && *diag.status) ? diag.status : L"-",
                         static_cast<int>(diag.maxChannel), static_cast<int>(diag.maxChannel * 100) % 100,
                         static_cast<int>(diag.threshold), static_cast<int>(diag.threshold * 100) % 100,
                         static_cast<int>(-diag.minChannel), static_cast<int>(-diag.minChannel * 1000) % 1000,
                         static_cast<int>(diag.hotFraction * 100), static_cast<int>(diag.hotFraction * 100000) % 1000,
                         decided ? 1 : 0, want ? L"HDR" : L"SDR");
    }
    return displayHdr;
}

// Event-driven loop: the scan itself blocks on the next desktop frame (so a
// changing screen wakes it and a static one sleeps in the kernel), and the wake
// event lets foreground/display/menu changes nudge it between scans. No fixed
// polling tick.
DWORD WINAPI worker_proc(LPVOID) {
    while (g_running.load()) {
        DWORD waitMs = kIdleWaitMs;
        if (g_enabled.load()) {
            waitMs = evaluate_and_apply() ? kThrottleMs : kIdleWaitMs;
        }
        if (g_wake) {
            WaitForSingleObject(g_wake, waitMs);
        } else {
            Sleep(waitMs);
        }
    }
    return 0;
}

void CALLBACK on_foreground_changed(HWINEVENTHOOK, DWORD, HWND hwnd, LONG, LONG, DWORD, DWORD) {
    // Remember the last external app for the overrides dialog's "Use last app".
    // This OUTOFCONTEXT callback runs on the UI thread, so no lock is needed.
    std::wstring exe = foreground_exe(hwnd);
    if (!exe.empty()) {
        g_lastForegroundExe = std::move(exe);
    }
    wake();  // re-evaluate promptly when the user switches apps
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DISPLAYCHANGE:
        wake();
        return 0;

    case WMAPP_TRAY:
        if (lParam == WM_RBUTTONUP) {
            HMENU menu = CreatePopupMenu();
            if (menu) {
                const text::Strings& t = text::s();
                // A non-clickable header showing the running version, so the
                // build is identifiable straight from the tray.
                const std::wstring versionLabel =
                    std::wstring(t.menuVersion) + L" " APP_VERSION_STRING;
                AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, versionLabel.c_str());
                AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(menu, MF_STRING | (g_enabled ? MF_CHECKED : MF_UNCHECKED),
                            ID_ENABLED, t.menuEnabled);
                AppendMenuW(menu, MF_STRING, ID_SYNC_NOW, t.menuSyncNow);

                // Correction-state submenu: follow automatic detection, or force
                // the corrector on (HDR) / off (SDR).
                const int force = g_force.load();
                HMENU stateMenu = CreatePopupMenu();
                if (stateMenu) {
                    AppendMenuW(stateMenu, MF_STRING | (force == 0 ? MF_CHECKED : 0u),
                                ID_STATE_AUTO, t.stateAuto);
                    AppendMenuW(stateMenu, MF_STRING | (force == 1 ? MF_CHECKED : 0u),
                                ID_STATE_HDR, t.stateHdr);
                    AppendMenuW(stateMenu, MF_STRING | (force == 2 ? MF_CHECKED : 0u),
                                ID_STATE_SDR, t.stateSdr);
                    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(stateMenu),
                                t.menuState);
                }

                AppendMenuW(menu, MF_STRING | (autostart::enabled() ? MF_CHECKED : MF_UNCHECKED),
                            ID_AUTOSTART, t.menuStartLogon);
                AppendMenuW(menu, MF_STRING, ID_OVERRIDES, t.menuOverrides);
                if (!diag::path().empty()) {
                    AppendMenuW(menu, MF_STRING, ID_OPENLOG, t.menuOpenLog);
                }
                AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(menu, MF_STRING, ID_EXIT, t.menuExit);
                POINT pt{};
                GetCursorPos(&pt);
                SetForegroundWindow(hwnd);
                TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
                DestroyMenu(menu);
            }
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_ENABLED:
            g_enabled = !g_enabled.load();
            wake();
            return 0;
        case ID_SYNC_NOW:
            wake();
            return 0;
        case ID_AUTOSTART:
            if (autostart::packaged()) {
                // MSIX build: the StartupTask is toggled in Windows Settings.
                autostart::open_startup_settings();
            } else {
                autostart::set_enabled(!autostart::enabled());
            }
            return 0;
        case ID_OPENLOG: {
            const std::wstring log = diag::path();
            if (!log.empty()) {
                ShellExecuteW(nullptr, L"open", log.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
            return 0;
        }
        case ID_STATE_AUTO:
            g_force = 0;
            wake();
            return 0;
        case ID_STATE_HDR:
            g_force = 1;
            wake();
            return 0;
        case ID_STATE_SDR:
            g_force = 2;
            wake();
            return 0;
        case ID_OVERRIDES:
            overrides::show(g_instance, hwnd, g_lastForegroundExe);
            return 0;
        case ID_EXIT:
            DestroyWindow(hwnd);
            return 0;
        default:
            return 0;
        }

    case WM_DESTROY:
        set_tray_icon(false);
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

} // namespace

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int) {
    // Desktop Duplication's DuplicateOutput1 requires the process to be
    // DPI-aware, and per-monitor awareness keeps window rects in the same
    // physical pixels as the captured framebuffer on scaled displays.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    g_instance = instance;

    g_singleInstance = CreateMutexW(nullptr, TRUE, kSingleInstanceMutex);
    if (g_singleInstance && GetLastError() == ERROR_ALREADY_EXISTS) {
        return 0;
    }

    diag::initialise();
    diag::write(L"---- started, version %hs", APP_VERSION_STRING);

    WNDCLASSW wc{};
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = instance;
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APPICON));
    wc.lpszClassName = kClassName;
    if (!RegisterClassW(&wc)) {
        return 1;
    }

    g_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, kClassName, L"HdrScreenshotSyncer", WS_POPUP,
                             0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
    if (!g_hwnd) {
        return 1;
    }

    set_tray_icon(true);

    // Seed the "last app" with whatever is already in front at startup.
    g_lastForegroundExe = foreground_exe(GetForegroundWindow());

    // Drive the sync from a worker thread that sleeps on desktop-frame updates,
    // nudged by the wake event on app switches / display changes / menu actions.
    g_wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    g_worker = CreateThread(nullptr, 0, worker_proc, nullptr, 0, nullptr);
    g_foregroundHook =
        SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
                        on_foreground_changed, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_foregroundHook) {
        UnhookWinEvent(g_foregroundHook);
    }
    g_running = false;
    wake();
    if (g_worker) {
        WaitForSingleObject(g_worker, 2000);
        CloseHandle(g_worker);
    }
    if (g_wake) {
        CloseHandle(g_wake);
    }
    diag::shutdown();
    return 0;
}

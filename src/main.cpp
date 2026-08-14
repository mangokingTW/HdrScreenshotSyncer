#include <windows.h>

#include <shellapi.h>

#include <atomic>
#include <optional>
#include <string>

#include "autostart.h"
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
constexpr UINT ID_DIAGLOG = 1005;
constexpr UINT ID_OVERRIDES = 1006;
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
std::atomic<bool> g_diagLog{false};
std::atomic<bool> g_running{true};
HANDLE g_singleInstance{};
HANDLE g_wake{};              // signalled to nudge the worker out of its wait
HANDLE g_worker{};
HWINEVENTHOOK g_foregroundHook{};

// Nudge the worker to re-evaluate now. Safe before the event exists / if it
// failed to create.
void wake() {
    if (g_wake) {
        SetEvent(g_wake);
    }
}

// Appends one UTF-8 line to %TEMP%\HdrScreenshotSyncer-diag.log for tuning the
// content scan. Only called while the diagnostic toggle is on.
void diag_log(const wchar_t* line) {
    wchar_t path[MAX_PATH]{};
    const DWORD n = GetTempPathW(MAX_PATH, path);
    if (n == 0 || n > MAX_PATH) {
        return;
    }
    lstrcatW(path, L"HdrScreenshotSyncer-diag.log");
    HANDLE h = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }
    const int len = WideCharToMultiByte(CP_UTF8, 0, line, -1, nullptr, 0, nullptr, nullptr);
    if (len > 1) {
        std::string buf(static_cast<size_t>(len - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, line, -1, buf.data(), len, nullptr, nullptr);
        buf += "\r\n";
        DWORD written = 0;
        WriteFile(h, buf.data(), static_cast<DWORD>(buf.size()), &written, nullptr);
    }
    CloseHandle(h);
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
    bool want = false;
    bool decided = true;
    const bool displayHdr = hdr::any_display_on();
    hdr::ScanDiag diag;
    if (displayHdr) {
        const std::optional<bool> content =
            hdr::foreground_has_hdr_content(g_diagLog.load() ? &diag : nullptr, kAcquireMs);
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

    if (g_diagLog.load()) {
        wchar_t title[128]{};
        GetWindowTextW(GetForegroundWindow(), title, ARRAYSIZE(title));
        SYSTEMTIME st{};
        GetLocalTime(&st);
        wchar_t line[600];
        wsprintfW(line,
                  L"%04d-%02d-%02d %02d:%02d:%02d | fg=\"%s\" | display_hdr=%d | status=%s hr=0x%08x "
                  L"white=%d.%02d thr=%d.%02d max=%d.%02d min=-%d.%03d hot=%d.%03d%% | "
                  L"decided=%d want=%s wrote=%d",
                  st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, title,
                  displayHdr ? 1 : 0, (diag.status && *diag.status) ? diag.status : L"-",
                  static_cast<unsigned int>(diag.hr),
                  static_cast<int>(diag.sdrWhite),
                  static_cast<int>(diag.sdrWhite * 100) % 100,
                  static_cast<int>(diag.threshold),
                  static_cast<int>(diag.threshold * 100) % 100,
                  static_cast<int>(diag.maxChannel),
                  static_cast<int>(diag.maxChannel * 100) % 100,
                  static_cast<int>(-diag.minChannel),
                  static_cast<int>(-diag.minChannel * 1000) % 1000,
                  static_cast<int>(diag.hotFraction * 100),
                  static_cast<int>(diag.hotFraction * 100000) % 1000, decided ? 1 : 0,
                  want ? L"HDR" : L"SDR", wrote ? 1 : 0);
        diag_log(line);
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

void CALLBACK on_foreground_changed(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD) {
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
                AppendMenuW(menu, MF_STRING | (g_enabled ? MF_CHECKED : MF_UNCHECKED),
                            ID_ENABLED, t.menuEnabled);
                AppendMenuW(menu, MF_STRING, ID_SYNC_NOW, t.menuSyncNow);
                if (!autostart::packaged()) {
                    // Hidden in the MSIX build: autostart there is the package's
                    // StartupTask, managed in Windows Settings > Startup.
                    AppendMenuW(menu, MF_STRING | (autostart::enabled() ? MF_CHECKED : MF_UNCHECKED),
                                ID_AUTOSTART, t.menuStartLogon);
                }
                AppendMenuW(menu, MF_STRING, ID_OVERRIDES, t.menuOverrides);
                AppendMenuW(menu, MF_STRING | (g_diagLog ? MF_CHECKED : MF_UNCHECKED),
                            ID_DIAGLOG, t.menuDiagLog);
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
            autostart::set_enabled(!autostart::enabled());
            return 0;
        case ID_DIAGLOG:
            g_diagLog = !g_diagLog.load();
            wake();  // log a line promptly
            return 0;
        case ID_OVERRIDES:
            overrides::show(g_instance, hwnd);
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
    return 0;
}

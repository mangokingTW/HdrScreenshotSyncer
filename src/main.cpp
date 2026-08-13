#include <windows.h>

#include <shellapi.h>

#include <optional>
#include <string>

#include "autostart.h"
#include "hdr.h"
#include "hdr_content.h"
#include "resource.h"
#include "snip_setting.h"

namespace {

constexpr UINT WMAPP_TRAY = WM_APP + 1;
constexpr UINT_PTR TIMER_SYNC = 1;
constexpr UINT ID_ENABLED = 1001;
constexpr UINT ID_SYNC_NOW = 1002;
constexpr UINT ID_AUTOSTART = 1003;
constexpr UINT ID_EXIT = 1004;
constexpr UINT ID_DIAGLOG = 1005;
constexpr UINT kSyncIntervalMs = 3000;
constexpr wchar_t kClassName[] = L"HdrScreenshotSyncerWindow";
constexpr wchar_t kSingleInstanceMutex[] = L"Local\\HdrScreenshotSyncer.SingleInstance";

HWND g_hwnd{};
NOTIFYICONDATAW g_tray{};
bool g_enabled = true;
bool g_diagLog = false;
HANDLE g_singleInstance{};

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
        lstrcpynW(g_tray.szTip, L"HDR Screenshot Syncer", ARRAYSIZE(g_tray.szTip));
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
// cycle, keep the current setting rather than flip it on missing data.
void sync() {
    if (!g_enabled) {
        return;
    }
    bool want = false;
    bool decided = true;
    const bool displayHdr = hdr::any_display_on();
    hdr::ScanDiag diag;
    if (displayHdr) {
        const std::optional<bool> content =
            hdr::foreground_has_hdr_content(g_diagLog ? &diag : nullptr);
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

    if (g_diagLog) {
        wchar_t title[128]{};
        GetWindowTextW(GetForegroundWindow(), title, ARRAYSIZE(title));
        SYSTEMTIME st{};
        GetLocalTime(&st);
        wchar_t line[600];
        wsprintfW(line,
                  L"%04d-%02d-%02d %02d:%02d:%02d | fg=\"%s\" | display_hdr=%d | status=%s hr=0x%08x "
                  L"white=%d.%02d thr=%d.%02d max=%d.%02d hot=%d.%03d%% | decided=%d want=%s wrote=%d",
                  st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, title,
                  displayHdr ? 1 : 0, (diag.status && *diag.status) ? diag.status : L"-",
                  static_cast<unsigned int>(diag.hr),
                  static_cast<int>(diag.sdrWhite),
                  static_cast<int>(diag.sdrWhite * 100) % 100,
                  static_cast<int>(diag.threshold),
                  static_cast<int>(diag.threshold * 100) % 100,
                  static_cast<int>(diag.maxChannel),
                  static_cast<int>(diag.maxChannel * 100) % 100,
                  static_cast<int>(diag.hotFraction * 100),
                  static_cast<int>(diag.hotFraction * 100000) % 1000, decided ? 1 : 0,
                  want ? L"HDR" : L"SDR", wrote ? 1 : 0);
        diag_log(line);
    }
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TIMER:
        if (wParam == TIMER_SYNC) {
            sync();
        }
        return 0;

    case WM_DISPLAYCHANGE:
        sync();
        return 0;

    case WMAPP_TRAY:
        if (lParam == WM_RBUTTONUP) {
            HMENU menu = CreatePopupMenu();
            if (menu) {
                AppendMenuW(menu, MF_STRING | (g_enabled ? MF_CHECKED : MF_UNCHECKED),
                            ID_ENABLED, L"Enabled");
                AppendMenuW(menu, MF_STRING, ID_SYNC_NOW, L"Sync now");
                if (!autostart::packaged()) {
                    // Hidden in the MSIX build: autostart there is the package's
                    // StartupTask, managed in Windows Settings > Startup.
                    AppendMenuW(menu, MF_STRING | (autostart::enabled() ? MF_CHECKED : MF_UNCHECKED),
                                ID_AUTOSTART, L"Start at logon");
                }
                AppendMenuW(menu, MF_STRING | (g_diagLog ? MF_CHECKED : MF_UNCHECKED),
                            ID_DIAGLOG, L"Write diagnostic log");
                AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(menu, MF_STRING, ID_EXIT, L"Exit");
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
            g_enabled = !g_enabled;
            if (g_enabled) {
                sync();
            }
            return 0;
        case ID_SYNC_NOW:
            sync();
            return 0;
        case ID_AUTOSTART:
            autostart::set_enabled(!autostart::enabled());
            return 0;
        case ID_DIAGLOG:
            g_diagLog = !g_diagLog;
            if (g_diagLog) {
                sync();  // write a first line immediately so the log appears
            }
            return 0;
        case ID_EXIT:
            DestroyWindow(hwnd);
            return 0;
        default:
            return 0;
        }

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_SYNC);
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
    sync();
    SetTimer(g_hwnd, TIMER_SYNC, kSyncIntervalMs, nullptr);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}

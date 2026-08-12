#include <windows.h>

#include <shellapi.h>

#include <optional>

#include "autostart.h"
#include "hdr.h"
#include "resource.h"
#include "snip_setting.h"

namespace {

constexpr UINT WMAPP_TRAY = WM_APP + 1;
constexpr UINT_PTR TIMER_SYNC = 1;
constexpr UINT ID_ENABLED = 1001;
constexpr UINT ID_SYNC_NOW = 1002;
constexpr UINT ID_AUTOSTART = 1003;
constexpr UINT ID_EXIT = 1004;
constexpr UINT kSyncIntervalMs = 3000;
constexpr wchar_t kClassName[] = L"HdrScreenshotSyncerWindow";
constexpr wchar_t kSingleInstanceMutex[] = L"Local\\HdrScreenshotSyncer.SingleInstance";

HWND g_hwnd{};
NOTIFYICONDATAW g_tray{};
bool g_enabled = true;
HANDLE g_singleInstance{};

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

// Match Snipping Tool's HDR screenshot corrector to whether a display is in HDR.
// Only writes when it differs, and a write may fail if Snipping Tool holds the
// hive -- the next cycle retries.
void sync() {
    if (!g_enabled) {
        return;
    }
    const bool want = hdr::any_display_on();
    const std::optional<bool> current = snip::read();
    if (!current.has_value() || current.value() != want) {
        snip::write(want);
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

#include "strings.h"

#include <windows.h>

namespace text {
namespace {

const Strings kEn = {
    .trayTip = L"HDR Screenshot Syncer",
    .menuEnabled = L"Enabled",
    .menuSyncNow = L"Sync now",
    .menuState = L"Correction state",
    .stateAuto = L"Automatic",
    .stateHdr = L"Force HDR",
    .stateSdr = L"Force SDR",
    .menuStartLogon = L"Start at logon",
    .menuOverrides = L"App overrides…",
    .menuOpenLog = L"Open diagnostic log",
    .menuExit = L"Exit",
    .menuVersion = L"Version",

    .ovrCaption = L"App overrides",
    .ovrIntro = L"Force Snipping Tool's HDR corrector for specific apps, by process name:",
    .ovrProcess = L"Process:",
    .ovrUseLast = L"Use last app",
    .ovrBrowse = L"Browse…",
    .ovrHdr = L"HDR (corrector on)",
    .ovrSdr = L"SDR (corrector off)",
    .ovrAdd = L"Add / Update",
    .ovrRemove = L"Remove selected",
    .ovrClose = L"Close",
    .ovrTip = L"Tip: if an app looks wrong either way (e.g. Discord with hardware "
              L"acceleration on), turn off that app's hardware acceleration instead.",
    .browseTitle = L"Choose an application",
    .filterPrograms = L"Programs (*.exe)",
    .filterAll = L"All files (*.*)",
};

const Strings kZhTw = {
    .trayTip = L"HDR 螢幕擷取同步器",
    .menuEnabled = L"啟用",
    .menuSyncNow = L"立即同步",
    .menuState = L"校正狀態",
    .stateAuto = L"自動",
    .stateHdr = L"強制 HDR",
    .stateSdr = L"強制 SDR",
    .menuStartLogon = L"開機時啟動",
    .menuOverrides = L"應用程式覆寫…",
    .menuOpenLog = L"開啟診斷記錄",
    .menuExit = L"結束",
    .menuVersion = L"版本",

    .ovrCaption = L"應用程式覆寫",
    .ovrIntro = L"依程式名稱，為特定應用程式"
                L"強制剪取工具的 HDR 校正器：",
    .ovrProcess = L"程式：",
    .ovrUseLast = L"用剛才的程式",
    .ovrBrowse = L"瀏覽…",
    .ovrHdr = L"HDR（開啟校正器）",
    .ovrSdr = L"SDR（關閉校正器）",
    .ovrAdd = L"新增 / 更新",
    .ovrRemove = L"移除選取",
    .ovrClose = L"關閉",
    .ovrTip = L"提示：若某程式開關校正器"
              L"都不對（例如開了硬體加速的"
              L" Discord），請改為關閉該程式的"
              L"硬體加速。",
    .browseTitle = L"選擇應用程式",
    .filterPrograms = L"程式 (*.exe)",
    .filterAll = L"所有檔案 (*.*)",
};

// Traditional Chinese (Taiwan / Hong Kong / Macau) users get the zh-Hant table.
bool is_zh_hant() {
    const LANGID lang = GetUserDefaultUILanguage();
    if (PRIMARYLANGID(lang) != LANG_CHINESE) {
        return false;
    }
    switch (SUBLANGID(lang)) {
    case SUBLANG_CHINESE_TRADITIONAL:
    case SUBLANG_CHINESE_HONGKONG:
    case SUBLANG_CHINESE_MACAU:
        return true;
    default:
        return false;
    }
}

}  // namespace

const Strings& s() {
    static const Strings& chosen = is_zh_hant() ? kZhTw : kEn;
    return chosen;
}

}  // namespace text

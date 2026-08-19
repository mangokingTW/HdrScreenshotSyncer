# Changelog

Per-version **highlights** live under a `## <tag>` heading (e.g. `## v0.2.4`),
bilingual (English + 繁體中文). At release time the workflow takes that section
and appends a consistent install / verify section, so the published GitHub
release note reads as a finished, formatted note rather than a raw changelog.
Keep each section to what changed; the boilerplate is added automatically. If a
tag has no section here, the workflow falls back to auto-generated notes.

## v1.1.1

The tray icon now reflects your display's HDR state at a glance: it shows an
**HDR** mark when any display has HDR (advanced color) turned on, and an **SDR**
mark when none does. It switches automatically when you toggle Windows HDR.

繁體中文:托盤圖示現在一眼就能看出螢幕目前的 HDR 狀態:有任一顯示器開啟 HDR(進階色彩)時顯示 **HDR** 圖案,都沒開時顯示 **SDR** 圖案,切換 Windows HDR 時會自動更換。

## v1.1.0

Three features brought over from the sibling IME tool:

- The diagnostic log is now always on, size-capped at 1 MB (it rotates), and kept
  at `%LocalAppData%\HdrScreenshotSyncer\log.txt`; the tray menu's **Open
  diagnostic log** opens it directly. It records each SDR/HDR switch and each
  distinct decision, so an odd result can be diagnosed after the fact.
- App overrides gained a **Use last app** button that fills in the program you
  were just in, so you don't have to type or browse for it.
- A new **Correction state** tray submenu forces the corrector to HDR or SDR, or
  leaves it on Automatic (the default).

繁體中文:從姊妹工具 IME 帶過來的三項功能:

- 診斷記錄改為永遠開啟、上限 1 MB(會自動輪替),存於 `%LocalAppData%\HdrScreenshotSyncer\log.txt`;托盤選單「**開啟診斷記錄**」可直接開檔。它會記錄每次 SDR/HDR 切換與每個不同的判斷,事後能追查異常。
- App 覆寫新增「**用剛才的程式**」按鈕,一鍵填入你剛才所在的程式,不必手打或瀏覽。
- 托盤新增「**校正狀態**」子選單,可強制校正器為 HDR 或 SDR,或維持自動(預設)。

## v1.0.1

The tray right-click menu now shows the app name and version at the top, so you
can tell which build is running at a glance.

繁體中文:托盤右鍵選單頂端現在會顯示 App 名稱與版本號,一眼就能看出目前執行的是哪個版本。

## v1.0.0

First stable 1.0 release. The Microsoft Store build now shows the "Start at logon" tray option — it opens
Windows Settings > Startup apps to turn it on or off (the check mark reflects the
current state). The app is also named "HDR Screenshot Syncer" consistently (Store,
Start menu, Task Manager), and a crash when reading the autostart state is fixed.

繁體中文:首個 1.0 正式版。Microsoft Store 版現在會顯示「開機時啟動」托盤選項——點了會開「Windows 設定 → 啟動應用程式」讓你開關(打勾會反映目前狀態)。App 在商店、開始功能表、工作管理員的名稱統一為「HDR Screenshot Syncer」,並修正讀取開機啟動狀態時可能 crash 的問題。

## v0.2.6

Microsoft Store: the app package now advertises its Traditional Chinese
(Taiwan) support, so the Store can present the listing with Traditional Chinese
store info. The in-app UI was already bilingual — this just lets the Store show
a Chinese listing too.

繁體中文:App 封裝現在會宣告支援繁體中文(台灣),讓 Microsoft Store 能以繁體中文呈現商店資訊。App 介面原本就已中英雙語,這次是讓商店也能顯示中文的商店列表。

## v0.2.5

Overview of everything HdrScreenshotSyncer does — it keeps Windows Snipping
Tool's HDR screenshot color corrector in sync with whether the foreground
content is actually HDR, so screenshots come out with the right colors on an HDR
display, automatically.

- Detects HDR from the actual pixels (wide-gamut / bright highlights), not the
  display's HDR flag; ignores black frames.
- Remembers the HDR decision per window, and holds it while Snipping Tool is in
  the foreground.
- Event-driven — reacts to focus / HDR changes instead of polling.
- Per-app overrides: force a specific app (by process name) to HDR or SDR when
  pixel detection can't tell it apart.
- English and Traditional Chinese (Taiwan) UI, chosen by your Windows display
  language.
- Opt-in diagnostics for the content scan; DPI-aware capture.

Note: some apps look wrong with the corrector both ways (e.g. Discord with
hardware acceleration on) — turn off that app's hardware acceleration instead.

繁體中文:HdrScreenshotSyncer 功能總覽——自動讓 Windows 剪取工具的「HDR 螢幕擷取色彩校正器」跟著前景內容是不是 HDR 同步,HDR 螢幕上截圖顏色就正確,不用手動切。

- 依實際像素判斷 HDR(廣色域/高亮),不看顯示器的 HDR 旗標;忽略純黑畫面。
- 每個視窗記住 HDR 判斷,剪取工具在前景時維持不變。
- 事件驅動——依焦點/HDR 變化反應,而非輪詢。
- 每 App 覆寫:判斷不出來時,可依程式名把特定 app 強制設為 HDR 或 SDR。
- 英文與繁體中文(台灣)介面,依 Windows 顯示語言自動選擇。
- 內容掃描可選開啟診斷;支援 DPI 感知擷取。

注意:有些 app 開關校正器都不對(例如開了硬體加速的 Discord)——請改為關閉該 app 的硬體加速。

## v0.2.4

Overview of everything HdrScreenshotSyncer does — it keeps Windows Snipping
Tool's HDR screenshot color corrector in sync with whether the foreground
content is actually HDR, so screenshots come out with the right colors on an HDR
display, automatically.

- Detects HDR from the actual pixels (wide-gamut / bright highlights), not the
  display's HDR flag; ignores black frames.
- Remembers the HDR decision per window, and holds it while Snipping Tool is in
  the foreground.
- Event-driven — reacts to focus / HDR changes instead of polling.
- Per-app overrides: force a specific app (by process name) to HDR or SDR when
  pixel detection can't tell it apart.
- English and Traditional Chinese (Taiwan) UI, chosen by your Windows display
  language.
- Opt-in diagnostics for the content scan; DPI-aware capture.

Note: some apps look wrong with the corrector both ways (e.g. Discord with
hardware acceleration on) — turn off that app's hardware acceleration instead.

繁體中文:HdrScreenshotSyncer 功能總覽——自動讓 Windows 剪取工具的「HDR 螢幕擷取色彩校正器」跟著前景內容是不是 HDR 同步,HDR 螢幕上截圖顏色就正確,不用手動切。

- 依實際像素判斷 HDR(廣色域/高亮),不看顯示器的 HDR 旗標;忽略純黑畫面。
- 每個視窗記住 HDR 判斷,剪取工具在前景時維持不變。
- 事件驅動——依焦點/HDR 變化反應,而非輪詢。
- 每 App 覆寫:判斷不出來時,可依程式名把特定 app 強制設為 HDR 或 SDR。
- 英文與繁體中文(台灣)介面,依 Windows 顯示語言自動選擇。
- 內容掃描可選開啟診斷;支援 DPI 感知擷取。

注意:有些 app 開關校正器都不對(例如開了硬體加速的 Discord)——請改為關閉該 app 的硬體加速。

## v0.2.3

Per-app overrides dialog — force a specific app (by process name) to HDR or SDR
when pixel detection can't tell it apart (list + Browse + Add/Remove). Traditional
Chinese (Taiwan) UI alongside English, chosen by the Windows display language.

繁體中文：新增「應用程式覆寫」對話框——當像素判斷不出來時,可依程式名把特定 app 強制設為 HDR 或 SDR(清單 + 瀏覽 + 新增/移除)。新增繁體中文(台灣)介面,依 Windows 顯示語言自動選擇。

Note: some apps look wrong with the corrector both ways (e.g. Discord with
hardware acceleration on) — no override fixes that; turn off that app's hardware
acceleration instead.

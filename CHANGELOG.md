# Changelog

Per-version **highlights** live under a `## <tag>` heading (e.g. `## v0.2.4`),
bilingual (English + 繁體中文). At release time the workflow takes that section
and appends a consistent install / verify section, so the published GitHub
release note reads as a finished, formatted note rather than a raw changelog.
Keep each section to what changed; the boilerplate is added automatically. If a
tag has no section here, the workflow falls back to auto-generated notes.

## v0.2.3

Per-app overrides dialog — force a specific app (by process name) to HDR or SDR
when pixel detection can't tell it apart (list + Browse + Add/Remove). Traditional
Chinese (Taiwan) UI alongside English, chosen by the Windows display language.

繁體中文：新增「應用程式覆寫」對話框——當像素判斷不出來時,可依程式名把特定 app 強制設為 HDR 或 SDR(清單 + 瀏覽 + 新增/移除)。新增繁體中文(台灣)介面,依 Windows 顯示語言自動選擇。

Note: some apps look wrong with the corrector both ways (e.g. Discord with
hardware acceleration on) — no override fixes that; turn off that app's hardware
acceleration instead.

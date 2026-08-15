#pragma once

// UI strings, English or Traditional Chinese depending on the user's Windows
// display language. text::s() returns the chosen table for the session.
namespace text {

struct Strings {
    const wchar_t* trayTip;
    const wchar_t* menuEnabled;
    const wchar_t* menuSyncNow;
    const wchar_t* menuStartLogon;
    const wchar_t* menuOverrides;
    const wchar_t* menuDiagLog;
    const wchar_t* menuExit;
    const wchar_t* menuVersion;       // "Version" / "版本"; the number is appended

    const wchar_t* ovrCaption;
    const wchar_t* ovrIntro;
    const wchar_t* ovrProcess;
    const wchar_t* ovrBrowse;
    const wchar_t* ovrHdr;
    const wchar_t* ovrSdr;
    const wchar_t* ovrAdd;
    const wchar_t* ovrRemove;
    const wchar_t* ovrClose;
    const wchar_t* ovrTip;
    const wchar_t* browseTitle;
    const wchar_t* filterPrograms;
    const wchar_t* filterAll;
};

const Strings& s();

}  // namespace text

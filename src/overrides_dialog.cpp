#include "overrides_dialog.h"

#include <windows.h>

#include <commdlg.h>

#include <string>
#include <vector>

#include "hdr_content.h"
#include "resource.h"
#include "strings.h"

namespace overrides {
namespace {

// Mirrors the list box so Remove can map a selection back to a rule.
std::vector<hdr::AppRule> g_rules;

// The last external foreground app, offered by the "Use last app" button. Set by
// show() before the modal dialog opens; the dialog runs on one thread.
std::wstring g_lastApp;

void populate(HWND dlg) {
    g_rules = hdr::list_overrides();
    HWND list = GetDlgItem(dlg, IDC_OVR_LIST);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    for (const hdr::AppRule& r : g_rules) {
        const std::wstring line = r.exe + (r.hdr ? L"  =  HDR" : L"  =  SDR");
        SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line.c_str()));
    }
}

// Basename of a full path into a std::wstring.
std::wstring basename_of(const wchar_t* path) {
    const wchar_t* base = path;
    for (const wchar_t* p = path; *p; ++p) {
        if (*p == L'\\' || *p == L'/') {
            base = p + 1;
        }
    }
    return base;
}

void browse(HWND dlg) {
    wchar_t file[MAX_PATH] = {};

    // Double-null-terminated filter, built from the localized descriptions.
    std::wstring filter;
    filter.append(text::s().filterPrograms).push_back(L'\0');
    filter.append(L"*.exe").push_back(L'\0');
    filter.append(text::s().filterAll).push_back(L'\0');
    filter.append(L"*.*").push_back(L'\0');
    filter.push_back(L'\0');

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = dlg;
    ofn.lpstrFile = file;
    ofn.nMaxFile = ARRAYSIZE(file);
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrTitle = text::s().browseTitle;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&ofn)) {
        SetDlgItemTextW(dlg, IDC_OVR_EXE, basename_of(file).c_str());
    }
}

void add_or_update(HWND dlg) {
    wchar_t exe[MAX_PATH] = {};
    GetDlgItemTextW(dlg, IDC_OVR_EXE, exe, ARRAYSIZE(exe));
    std::wstring name = exe;
    // Trim surrounding whitespace.
    const size_t a = name.find_first_not_of(L" \t");
    const size_t b = name.find_last_not_of(L" \t");
    name = a == std::wstring::npos ? std::wstring{} : name.substr(a, b - a + 1);
    if (name.empty()) {
        return;
    }
    const bool hdr = IsDlgButtonChecked(dlg, IDC_OVR_HDR) == BST_CHECKED;
    hdr::set_override(name, hdr);
    SetDlgItemTextW(dlg, IDC_OVR_EXE, L"");
    populate(dlg);
}

void remove_selected(HWND dlg) {
    const LRESULT sel = SendMessageW(GetDlgItem(dlg, IDC_OVR_LIST), LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR || static_cast<size_t>(sel) >= g_rules.size()) {
        return;
    }
    hdr::remove_override(g_rules[static_cast<size_t>(sel)].exe);
    populate(dlg);
}

INT_PTR CALLBACK dlg_proc(HWND dlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        const text::Strings& t = text::s();
        SetWindowTextW(dlg, t.ovrCaption);
        SetDlgItemTextW(dlg, IDC_OVR_INTRO, t.ovrIntro);
        SetDlgItemTextW(dlg, IDC_OVR_PROCESS, t.ovrProcess);
        SetDlgItemTextW(dlg, IDC_OVR_BROWSE, t.ovrBrowse);
        SetDlgItemTextW(dlg, IDC_OVR_HDR, t.ovrHdr);
        SetDlgItemTextW(dlg, IDC_OVR_SDR, t.ovrSdr);
        SetDlgItemTextW(dlg, IDC_OVR_ADD, t.ovrAdd);
        SetDlgItemTextW(dlg, IDC_OVR_REMOVE, t.ovrRemove);
        SetDlgItemTextW(dlg, IDC_OVR_USELAST, t.ovrUseLast);
        SetDlgItemTextW(dlg, IDCANCEL, t.ovrClose);
        SetDlgItemTextW(dlg, IDC_OVR_TIP, t.ovrTip);
        // No last app seen yet: nothing to fill in.
        EnableWindow(GetDlgItem(dlg, IDC_OVR_USELAST), !g_lastApp.empty());
        CheckRadioButton(dlg, IDC_OVR_HDR, IDC_OVR_SDR, IDC_OVR_HDR);
        populate(dlg);
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_OVR_BROWSE:
            browse(dlg);
            return TRUE;
        case IDC_OVR_USELAST:
            if (!g_lastApp.empty()) {
                SetDlgItemTextW(dlg, IDC_OVR_EXE, g_lastApp.c_str());
            }
            return TRUE;
        case IDC_OVR_ADD:
            add_or_update(dlg);
            return TRUE;
        case IDC_OVR_REMOVE:
            remove_selected(dlg);
            return TRUE;
        case IDCANCEL:
            EndDialog(dlg, 0);
            return TRUE;
        default:
            return FALSE;
        }

    case WM_CLOSE:
        EndDialog(dlg, 0);
        return TRUE;

    default:
        return FALSE;
    }
}

}  // namespace

void show(HINSTANCE instance, HWND owner, const std::wstring& lastApp) {
    g_lastApp = lastApp;
    DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_OVERRIDES), owner, dlg_proc, 0);
}

}  // namespace overrides

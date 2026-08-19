#pragma once

#define IDI_APPICON 101
// Tray state icons: green "HDR" when the corrector is on, grey "SDR" when off.
// The window, installer and Store keep IDI_APPICON (the plain brand mark).
#define IDI_TRAY_HDR 102
#define IDI_TRAY_SDR 103

// App-overrides dialog. Control text is set at runtime from the strings table
// (English / Traditional Chinese), so the template below carries only layout.
#define IDD_OVERRIDES 200
#define IDC_OVR_INTRO 201
#define IDC_OVR_LIST 202
#define IDC_OVR_PROCESS 203
#define IDC_OVR_EXE 204
#define IDC_OVR_BROWSE 205
#define IDC_OVR_HDR 206
#define IDC_OVR_SDR 207
#define IDC_OVR_ADD 208
#define IDC_OVR_REMOVE 209
#define IDC_OVR_TIP 210
#define IDC_OVR_USELAST 211

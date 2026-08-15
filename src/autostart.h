#pragma once

// Start at logon via the unelevated HKCU Run key. This tool never needs
// elevation (RegLoadAppKey works as a normal user), so a Run entry is enough --
// no scheduled task.
namespace autostart {

// Whether the Run entry exists and points at this executable.
bool enabled();

// Adds or removes the Run entry.
bool set_enabled(bool on);

// Whether this process runs from an MSIX package (the Store build). There the Run
// key is virtualized to no effect; autostart is the package's StartupTask.
// enabled() reads its state; the tray "Start at logon" item opens Windows
// Settings (below) to toggle it, since the WinRT enable/disable API faults.
bool packaged();

// Opens Windows Settings > Startup apps. The MSIX build's tray "Start at logon"
// opens this rather than toggling the StartupTask from code.
void open_startup_settings();

} // namespace autostart

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
// key is virtualized to no effect; autostart is the package's StartupTask, managed
// in Windows Settings > Startup, so the tray toggle is hidden and this module's
// Run-key calls are inert.
bool packaged();

} // namespace autostart

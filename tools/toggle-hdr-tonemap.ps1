# Proof of concept: flip Snipping Tool's IsHDRToneMappingEnabled from OUTSIDE the
# app, to check whether Snipping Tool honors an external write. If it does, an
# auto-sync tray tool is worth building; if not, the approach is dead.
#
# Close Snipping Tool first (and end any background SnippingTool.exe in Task
# Manager) so settings.dat is not locked. No administrator rights needed.
#
#   powershell -ExecutionPolicy Bypass -File tools\toggle-hdr-tonemap.ps1 on
#   powershell -ExecutionPolicy Bypass -File tools\toggle-hdr-tonemap.ps1 off
#   powershell -ExecutionPolicy Bypass -File tools\toggle-hdr-tonemap.ps1        # toggle
param([ValidateSet('on','off','toggle')][string]$State = 'toggle')

$ErrorActionPreference = 'Stop'

$dat = Join-Path $env:LOCALAPPDATA 'Packages\Microsoft.ScreenSketch_8wekyb3d8bbwe\Settings\settings.dat'
if (-not (Test-Path $dat)) { throw "settings.dat not found: $dat" }

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class AppKey {
  [DllImport("advapi32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  public static extern int RegLoadAppKey(string file, out IntPtr hk, int sam, int opt, int res);
  [DllImport("advapi32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  public static extern int RegOpenKeyEx(IntPtr hk, string sub, int opt, int sam, out IntPtr res);
  [DllImport("advapi32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  public static extern int RegQueryValueEx(IntPtr hk, string name, IntPtr res, out int type, byte[] data, ref int len);
  [DllImport("advapi32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  public static extern int RegSetValueEx(IntPtr hk, string name, int res, int type, byte[] data, int len);
  [DllImport("advapi32.dll", SetLastError=true)]
  public static extern int RegCloseKey(IntPtr hk);
}
"@

$KEY_ALL = 0xF003F
$TYPE = 100000011   # 0x5F5E10B: UWP LocalSettings Boolean, exactly as ProcMon reported

$root = [IntPtr]::Zero
$rc = [AppKey]::RegLoadAppKey($dat, [ref]$root, $KEY_ALL, 0, 0)
if ($rc -ne 0) { throw "RegLoadAppKey failed ($rc). Close Snipping Tool / end SnippingTool.exe, then retry." }
try {
  $sub = [IntPtr]::Zero
  $rc = [AppKey]::RegOpenKeyEx($root, 'LocalState', 0, $KEY_ALL, [ref]$sub)
  if ($rc -ne 0) { throw "open LocalState failed ($rc)" }
  try {
    $type = 0; $len = 0
    [AppKey]::RegQueryValueEx($sub, 'IsHDRToneMappingEnabled', [IntPtr]::Zero, [ref]$type, $null, [ref]$len) | Out-Null
    $cur = $null
    if ($len -gt 0) {
      $buf = New-Object byte[] $len
      [AppKey]::RegQueryValueEx($sub, 'IsHDRToneMappingEnabled', [IntPtr]::Zero, [ref]$type, $buf, [ref]$len) | Out-Null
      $cur = $buf[0]
    }
    Write-Host "current IsHDRToneMappingEnabled = $cur (regtype=$type, len=$len)"

    switch ($State) {
      'on'  { $val = 1 }
      'off' { $val = 0 }
      default { if ($cur -eq 1) { $val = 0 } else { $val = 1 } }
    }

    $ft = [BitConverter]::GetBytes([DateTime]::UtcNow.ToFileTimeUtc())  # 8 bytes, little-endian
    $data = New-Object byte[] 9
    $data[0] = [byte]$val
    [Array]::Copy($ft, 0, $data, 1, 8)

    $rc = [AppKey]::RegSetValueEx($sub, 'IsHDRToneMappingEnabled', 0, $TYPE, $data, 9)
    if ($rc -ne 0) { throw "RegSetValueEx failed ($rc)" }
    Write-Host "wrote IsHDRToneMappingEnabled = $val"
  } finally { [AppKey]::RegCloseKey($sub) | Out-Null }
} finally { [AppKey]::RegCloseKey($root) | Out-Null }

Write-Host ""
Write-Host "Now open Snipping Tool > Settings: does the 'HDR screenshot color corrector' match?"
Write-Host "Then take a screenshot of HDR content and check the colors. Report back whether it was honored."

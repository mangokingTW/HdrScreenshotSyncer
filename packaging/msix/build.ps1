# Packs the Store MSIX from a built executable, the manifest and the assets.
# Fill in the real Identity in AppxManifest.xml first (see README.md). Run each
# line separately in Windows PowerShell (5.1 has no '&&'):
#
#   cmake -S . -B build-x64 -A x64
#   cmake --build build-x64 --config Release
#   powershell -ExecutionPolicy Bypass -File packaging\msix\build.ps1
#
# The Store signs the package on upload, so the .msix produced here needs no
# signing for submission; sign it with a self-signed cert only to sideload-test.
param(
  [string]$ExePath = "build-x64/Release/HdrScreenshotSyncer.exe",
  [string]$OutDir  = "dist"
)
$ErrorActionPreference = 'Stop'

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not (Test-Path $ExePath)) { throw "executable not found: $ExePath (build Release first)" }

$stage = Join-Path ([System.IO.Path]::GetTempPath()) 'hdrscreenshotsyncer-msix'
Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $stage | Out-Null

Copy-Item $ExePath                             (Join-Path $stage 'HdrScreenshotSyncer.exe')
Copy-Item (Join-Path $here 'AppxManifest.xml') (Join-Path $stage 'AppxManifest.xml')
Copy-Item (Join-Path $here 'assets')           (Join-Path $stage 'assets') -Recurse

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$msix = Join-Path $OutDir 'HdrScreenshotSyncer.msix'

function Find-MakeAppx {
  $cmd = Get-Command makeappx.exe -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Source }
  foreach ($root in @("${env:ProgramFiles(x86)}\Windows Kits\10\bin", "${env:ProgramFiles}\Windows Kits\10\bin")) {
    if (Test-Path $root) {
      $exe = Get-ChildItem $root -Recurse -Filter makeappx.exe -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\x64\\' } |
        Sort-Object FullName -Descending | Select-Object -First 1
      if ($exe) { return $exe.FullName }
    }
  }
  throw "makeappx.exe not found. Install the Windows SDK, or use the 'Developer PowerShell for VS'."
}
$makeappx = Find-MakeAppx
& $makeappx pack /d $stage /p $msix /o
if ($LASTEXITCODE) { throw "makeappx failed" }

Write-Host "Packed: $msix"
Write-Host "Store: upload this .msix in Partner Center (Microsoft signs it)."

$ErrorActionPreference = 'Stop'

# Chocolatey runs elevated, so the per-user installer is a poor fit; install the
# portable build instead and let Chocolatey shim the executable. The package
# version drives the URLs (choco sets $env:ChocolateyPackageVersion), so one
# script serves every release; the release workflow bumps the version and the
# checksums below.
$version = $env:ChocolateyPackageVersion
$base = "https://github.com/mangokingTW/HdrScreenshotSyncer/releases/download/v$version"

$packageArgs = @{
  packageName    = 'hdrscreenshotsyncer'
  unzipLocation  = Split-Path -Parent $MyInvocation.MyCommand.Definition
  url            = "$base/HdrScreenshotSyncer-$version-x86.zip"
  url64bit       = "$base/HdrScreenshotSyncer-$version-x64.zip"
  checksum       = 'C0E7D8EF567463B6AEDAE3B075C7922634ADB565C6A5A10774E748D62FA19A7D'
  checksumType   = 'sha256'
  checksum64     = 'DAB513E473FD532C3F10CE74B735B5E0BAC5A45EDA0DEDD4FFAAD1A7CB7A824A'
  checksumType64 = 'sha256'
}

Install-ChocolateyZipPackage @packageArgs

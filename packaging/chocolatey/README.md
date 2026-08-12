# Chocolatey package

Community-repo package for [Chocolatey](https://community.chocolatey.org). It
downloads the official portable build from GitHub Releases and shims the
executable (the per-user installer is a poor fit for Chocolatey's elevated
context). Use the tray menu's **Start at logon** to run it automatically.

## Automated publishing

`.github/workflows/chocolatey.yml` packs and pushes automatically when a
**stable** release is published: version from the tag, checksums from the
release's `SHA256SUMS.txt`. It is a no-op until the **`CHOCO_API_KEY`** repo
secret is set, and skips pre-releases.

## Manual publish

```powershell
# bump <version> + the two checksums in tools/chocolateyInstall.ps1 first
choco pack packaging\chocolatey\hdrscreenshotsyncer.nuspec
choco apikey --key <API_KEY> --source https://push.chocolatey.org/
choco push hdrscreenshotsyncer.<version>.nupkg --source https://push.chocolatey.org/
```

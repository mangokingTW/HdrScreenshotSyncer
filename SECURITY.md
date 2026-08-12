# Security Policy

## Reporting a vulnerability

Please report security issues privately via GitHub's **Report a vulnerability**
button on the [Security tab](https://github.com/mangokingTW/HdrScreenshotSyncer/security/advisories/new),
rather than opening a public issue. I'll respond as soon as I can.

## Supported versions

The latest release is supported; fixes ship in a new release.

## Scope

HdrScreenshotSyncer runs entirely locally: it reads the display's HDR state and
writes a single Snipping Tool setting. It makes no network connections, collects
no data, and needs no elevation. Every release carries build provenance,
verifiable with:

```
gh attestation verify <file> --repo mangokingTW/HdrScreenshotSyncer
```

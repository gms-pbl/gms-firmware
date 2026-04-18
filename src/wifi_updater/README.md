# Portenta WiFi Updater Utility

Utility firmware used to test or update WiFi/TLS connectivity behavior on Portenta.

This project is separate from the main zone-control firmware (`src/portenta`) and is useful for network/bootstrap checks.

## What It Is For

- validating WiFi connectivity quickly
- validating TLS certificate bundle handling
- isolating connectivity issues from application logic

## Project Layout

- `src/main.cpp` - updater logic entry point
- `src/certificates.h` - embedded CA bundle header
- `src/Readme.md` - certificate conversion helpers
- `scripts/build.sh` - build helper
- `scripts/upload.sh` - upload helper

## Build and Upload

From `firmware/src/wifi_updater`:

```bash
./scripts/build.sh
./scripts/upload.sh
```

## PlatformIO Environment

Defined in `platformio.ini`:

- environment: `portenta_h7_m7`
- framework: Arduino
- upload protocol: DFU

## Certificate Maintenance Workflow

Detailed commands are documented in `src/Readme.md`.

Typical flow:

1. Convert PEM CA file into C header bytes (`certificates.h`)
2. Build and flash updater image
3. Verify TLS connectivity
4. Update/add root CAs when required by broker/endpoint chain

## When To Use This Utility

- onboarding new network environments
- verifying certificate trust chain changes
- debugging transport-level issues before testing full firmware stack

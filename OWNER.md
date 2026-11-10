# Where QOR OS stands

For Andrew, 10 November 2026. This checkpoint on `integration/owner-brief`,
based on `3a5ddc3`, contains Q Pi preparation, project separation and the
resource checks below. It establishes no image or hardware acceptance.

## QOR OS and Q Pi resource checks

The shared memory guard rejects thresholds below its existing 3 GiB floor;
a caller cannot disable it with zero, a boolean or a floating-point value.
Higher requirements remain possible. Q Pi's read-only probe now accepts a
measured component budget and an existing scratch directory, then checks
local disk capacity. Missing or invalid estimates, duplicate fields, overflow,
insufficient space and unqualified virtual/remote storage remain blockers.
The probe reserves no space and starts no build or service operation.

Final selected tests pass on Windows: 38 executed, four Linux mount tests
skipped. All 42 pass under Linux through Windows Subsystem for Linux (WSL),
including refusal of virtual mounts hiding a local disk. These are tooling
and synthetic-input checks, not image or hardware acceptance. The 9 November Windows
Python regression passes 585 tests with 68 platform/tool skips; repository hooks
pass. The final mount checks were also refreshed in the focused suites. The detailed
record is build/resource-preflight-2026-11-09/result.json.

## Q Pi preparation

QOR-0025 scopes a Raspberry Pi 5 development edition for ARM64, its processor
architecture, using a maintained base login and desktop before native QOR
applications. The probe still exits 2 with unresolved source, host, builder,
ARM64 Qt/dependency and measured-budget qualification. A small synthetic
capacity control passes on the existing Windows volume; it is no real build
estimate. No ARM64 application, Pi image or hardware test ran in this session.

## Existing device work

The private amd64 installation/reboot summary and its 24 retained evidence
checksums still match. Prism's current build contract reports authentication
unavailable. The last recorded compiler run has twelve Wayland type-metadata
warnings; this session ran no fresh CMake, product or sanitizer build. Memory
fell below the build floor during investigation and later recovered; current
headroom must be checked again before each heavy phase. Real sign-in, device
services, hardware, recovery, rollback and release gates remain open.

QOR uses its own resource checks and heavy-job lock. Retired cross-project
helpers remain removed, and unrelated project files and services were untouched.

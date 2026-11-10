# Where QOR OS stands

For Andrew, 10 November 2026. Preparation and project separation were pushed
as `b55c7a4`; native Linux memory checks followed as `e97f57b`, both on
`integration/owner-brief`. This checkpoint records the subsequent compiler stop.

## Validated QOR OS and Q Pi preparation

The shared resource guard preserves its 3 GiB memory floor. Q Pi can check
an explicit scratch budget on a local disk and can now measure native Linux
memory, bounded by visible cgroup v2 ancestry (Linux resource limits). Missing
or ambiguous counters and migration during observation are refused. Windows
keeps its host guard; WSL, Windows Subsystem for Linux, cannot substitute its
guest memory. No settings, limits or unrelated services are changed by the probe.

The complete Windows Python regression passes 608 tests with 72 platform/tool
skips. Focused suites pass 57 Windows cases with four Linux-only skips and all
61 Linux cases under WSL. Repository hooks pass. Native Linux counter tests use
fixtures; they do not qualify an ARM64 builder. Source hashes, commands and
results are in build/native-memory-2026-11-10/.

## Prism compiler work

The corrected Qt type-registrar baseline reproduces all twelve metadata
warnings. A private candidate uses Qt's public extension base and adds a
protocol-global regression, but its Release build was stopped when available
host RAM fell to 748 MiB, below the unchanged 3,072-MiB floor. Only this session's
QOR build scope was stopped; its native exit is 15 and the heavy-job lock is free.

The candidate patch and partial logs remain in build/prism-metadata-2026-11-10/.
Its source changes were restored to the validated checkpoint. No candidate
compiler, runtime or sanitizer pass is claimed; the twelve warnings remain open.

## Device and image qualification

The private amd64 installer/reboot summary retains 24 matching evidence hashes.
Prism still reports authentication unavailable. No image, VM, physical-device,
provisioning or media operation ran. Q Pi still needs a supported native ARM64
builder, entitled Linux ARM64 Qt 6.12, compatible dependencies, verified inputs
and a measured disk budget before application/image/Pi acceptance. Full QOR OS
sign-in, services, hardware, recovery, rollback, CI isolation and release remain
open. Maintained frameworks, primary documentation and pinned, checked inputs
are now an explicit repository working agreement.

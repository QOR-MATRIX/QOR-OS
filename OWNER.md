# Where QOR OS stands

For Andrew, 10 November 2026. This session began at clean `8670e39` on
`integration/owner-brief`. Existing history, candidate patch and logs are retained.
No unrelated project's files or services were inspected or managed.

## Prism warning candidate

Qt's type registrar, which generates QML's C++ type descriptions, reproduces
**twelve warnings on committed headers and zero on the candidate** with warnings
explicitly enabled. Ten came from five template/protocol base pairs; two from
Qt shell templates pulled in by a method argument. The candidate uses Qt's
public concrete extension base, checks that QObject argument before accessing
protocol state, and adds a protocol-global regression. It is reapplied but
uncommitted: Linux compilation, runtime and sanitizer acceptance remain pending.
The metadata comparison used Windows Qt 6.12 tools, not a Linux compiler.
Evidence is in build/prism-resume-2026-11-10/; the original patch/logs remain
in build/prism-metadata-2026-11-10/.

## Resource blocker

No active QOR compiler or running QOR user unit was available to reclaim.
The original heavy-job lock was used. The monitored resume stopped only its
own scope before compilation when Windows available RAM fell from 3,201 MiB
to 1,189 MiB. A separate read-only WSL (Windows Subsystem for Linux) startup
fell from 3,577 MiB to 1,419 MiB without compiling. Admit work only with WSL
already resident and at least the unchanged 3,072-MiB floor. No host settings,
other services or physical media changed. More host headroom or another
qualified builder is needed; WSL's virtual free space cannot clear disk checks.

## Q Pi source build path

QQ now has native ARM64 release/sanitizer configure, build and test presets.
They retain hardening, warnings as errors, single-job builds and 16-KiB linker
pages. The existing Linux product harness selects them and tests the same
cache, with its display/keyring isolation. Actual CMake rejects the x86-64 host
before compiler discovery. Portable resource/ELF/preset suites pass 59 Windows
cases with four platform skips and 63 WSL cases; all 17 harness fixture tests
pass. These fixtures do not run actual products. Repository hooks pass;
no full product or sanitizer pass is claimed.

The exact candidate image-generator source was checked out and its dependency,
private-tool and Pi 5 boot recipes inspected. No generator was run or installed.
Only x86-64 WSL Qt and unsuitable Windows/Android ARM64 kits are installed here.
A native Debian/Raspberry Pi OS Trixie ARM64 builder and entitled Qt 6.12 Linux
ARM64 kit are still needed, with measured physical scratch and dependency checks.
**No ARM64 QOR app or bootable Pi image was built or tested.** Physical boot,
graphics, login, development, recovery, full QOR services and release remain open.

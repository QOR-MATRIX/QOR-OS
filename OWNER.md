# Where QOR OS stands

For Andrew, 16 November 2026. Authorized private builder dependencies are
provisioned and their tool checks pass. The Pi's host packages and running
kernel are unchanged. Kernel regression and product acceptance remain open.

The private 64-bit ARM tool root contains 218 pinned Debian packages. Signed
metadata and every archive hash verify before payload extraction. No package
installation script runs. The root is mounted read-only for work; it is not
a configured Debian system. Its contents, permissions and links are recorded.

GCC 14.2.0 compiles, links and runs a small program with OpenSSL. bison/flex
(parser generators) produce working compiled programs. QEMU 10.0.13 (the
emulator) starts a paused ARM64 machine and exits normally. No kernel is
provided to that emulator; this is not a successful kernel boot or test.
The original guard records 158 samples, at least 7,214 MiB (mebibytes)
available, above the unchanged 3,072-MiB floor. A separate content seal passes.

The reviewed two-line V3D (Pi graphics driver) backport still targets the
running 6.18.50 source/configuration. It keeps zero as an immediate poll while
preserving positive waits and errors. Its six KUnit (Linux kernel unit) cases
are unchanged and remain unrun on both kernels. Earlier object compilation
is separate evidence. Complete source/config preparation, kernel link,
regression execution and kernel sanitizer checks remain to be done.

Recovery inventory retains the stock kernel, initial-memory filesystem,
modules and device trees. Normal boot files select 6.18.60 while 6.18.50 runs.
Physical media access, backup readback and a stock recovery boot remain
unverified. Nothing authorizes candidate installation, module loading or reboot.

All 1,414 original source hashes, Agent/Studio binaries and prior image are
preserved at admission. The original gameplay result remains three passes,
one failure and zero skips: `times > 5` is three; later notice/cursor checks
were unreached. The software-renderer failure and 97 image outliers remain.
No new QOR Release, sanitizer, product or image acceptance is claimed.

The first hosted security scan flags a verified public archive filename.
An exact path/line exception has five passing local scanner checks; hosted
verification remains separate. Windows refuses a history scan at 2,398 MiB,
below the resource floor. No services are stopped to free memory.

Next: verify the complete pinned kernel source/config, build the isolated
test kernel, then run the unchanged six-case control and corrected suite.
PR #10 stays draft while required acceptance and isolated CI (automated
integration checks) remain incomplete. Player frame rates, the earlier
Wayland leak and ARQADE diagnostics remain open. Review the dependency lock
and runbook in `image/README.md`; receipts:
`build/development-2026-11-16/session-08/`. Session-07 remains sealed.
Consolidation planning is separate.

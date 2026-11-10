# Where QOR OS stands

For Andrew, 10 November 2026. Q Pi preparation, project separation and the
shared resource checks were committed and pushed as `b55c7a4` on
`integration/owner-brief`. This follow-up adds native Linux memory observation.

## QOR OS and Q Pi resource checks

Q Pi's read-only preparation probe can now measure memory on a native Linux
builder. It takes the smaller of the kernel's available-memory estimate and
the unused allowance of every visible cgroup v2 ancestor (Linux's resource
limits). Invalid/missing counters, unsafe paths, hidden mount roots and a
membership change during observation are refused. Swap adds no headroom.
Windows keeps its existing host check; WSL, Windows Subsystem for Linux,
still cannot substitute guest memory for the Windows host's 3 GiB floor.

The final focused suites pass 57 Windows cases, with four Linux mount cases
skipped, and all 61 cases on Linux through WSL. These use synthetic counter
fixtures; no native ARM64 builder was tested. Live Windows and WSL probes
still refuse image acceptance for the remaining prerequisites. Detailed
commands, source hashes and results are in build/native-memory-2026-11-10/.
The complete Windows Python regression passes 608 tests with 72 platform/tool
skips; repository hooks pass. No full compiler or image gate is cleared.

## Compiler and device work

The attempted Prism metadata investigation stopped before its corrected
reproduction: available RAM fell to 2,014 MiB against the unchanged 3,072-MiB
minimum. The initial diagnostic selected the wrong module and was rejected;
its failed record is preserved. No C++ source changed and no compiler pass is
claimed. The last recorded twelve Wayland metadata warnings remain unresolved.

The private amd64 installer/reboot summary retains 24 matching evidence
checksums. Prism's current contract still reports authentication unavailable.
This work starts no image, VM, device service, provisioning or media operation.

## Remaining qualification

Q Pi still needs a supported native ARM64 builder, entitled Linux ARM64 Qt 6.12
and compatible dependencies, verified inputs, a measured disk budget, then
actual application, image and Pi tests. Full QOR OS sign-in, device services,
hardware, recovery, rollback, CI isolation and release gates remain open.
QOR uses its original heavy-job lock and independent resource checks; unrelated
projects and their services remain untouched.

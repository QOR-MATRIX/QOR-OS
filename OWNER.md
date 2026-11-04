# Where QOR OS stands

For Andrew, 4 November 2026. Focused validation passed; 435 QQ lint warnings keep the zero-warning bar open.

QOR OS joins Prism (desktop and compositor), Lumen (design language), QQ, ARQADE and QOR Media. This session remains sole
implementation owner. It began clean at `17ef113`; all nine recovery fingerprints matched. The worktrees and unfinished work
remain. The remote integration branch is still `f65c90d`; nothing was pushed.

Linux product validation now requires complete matching CTest (test runner) evidence on every attempt. Old records are
invalidated before configuring, building and testing; cleanup failure stops the run. Whole skipped suites cannot pass.
Three false-pass cases were reproduced before the final fix; **88 wrapper/report tests pass**.

QQ Studio's marker and swatch keep their sizes through the layout. Fresh QML lint (Qt's interface-language checker):
**439 → 435 warnings, four removed, none added** against `17ef113`. Built lint agrees; the validation wrapper correctly exits 1.
Recovery, transaction tags, trust, checked thumbnails, changed-copy protection and listing provenance (where a pack came from)
were rechecked against code and hostile-input tests. The 8 MiB lock limit stays.

Fresh runs, hidden under WSL (Linux inside Windows), drawn in software and fenced from the graphics processor:

- Release (optimized): **25 Studio and 89 asset-library cases passed**, none failed or skipped.
- AddressSanitizer/UBSan (memory and undefined-behavior checks): **89 asset-library cases passed**, none failed or skipped,
  in the unchanged recovery code's preserved build. Leak checking was off.
- Prism's simulator/device command-line checks: **21 cases passed under sanitizers**, including script restrictions.
- Hardening (exploit-mitigation checks): **32 programs passed**. Both QQ runs measured the largest pack.

The shared lock kept jobs serial; products used one compiler/rendering worker, two processors and a 1.25 GiB memory cap.
Kintrek's handoff held its full build. Evidence: `build/checkpoint-2026-11-04/`. Full OS/core, all products, Windows, native
hardware and root-wide lint were not rerun. Listing and planning still occupy the window thread: Release measured 257/402 ms
for the largest pack, local observations without a latency guarantee. Continue lint in small batches.

VM (virtual machine) Create/Prove commands are prepared from current `docs/CI.md`; neither ran. Your separate go-ahead, a clean
reviewed revision and about 7 GB free are needed; the final sample had roughly 4 GB free. Qt's licensing answer and successful isolation
proof precede a kit/runner in the guest. Runner settings and isolation flags remain unchanged. The roadmap's other owner decisions stand.

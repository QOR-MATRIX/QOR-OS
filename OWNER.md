# Where QOR OS stands

For Andrew, 4 November 2026. The runtime batch passed; the warning gate remains open.

QOR OS joins Prism (desktop and compositor), Lumen (design language), QQ, ARQADE and QOR Media. This session remains sole
implementation owner. It began clean at `8eca6ea`; all 13 QOR worktrees were inspected and preserved. Nothing was pushed.

Work follows QOR's workers and verified build lock (one heavy job at a time). Cross-project prerequisites are removed from
OWNER and local handoff/VM notes. The development guide records this procedure. No other project was changed.

QQ's Player material and TileMap root have private identifiers distinct from Qt properties. Public fields and bindings stay
the same. Two new checks verify body-part colours follow edits and tile geometry follows map, layer and scale changes.

Fresh QML lint (Qt's interface-language checker): **435 → 431 diagnostics**, two shadowing warnings and their two declaration
notes removed, none added against `8eca6ea`. Targeted and full built lint agree. No warnings were suppressed; the wrapper
correctly exits 1 for the remaining gate.

Checks used commercial Qt 6.12 under WSL (Linux inside Windows), hidden, drawn in software and fenced from the graphics processor:

- Release (optimized): **43 scene cases passed before and after the rename**, none failed or skipped; no compiler warnings.
- AddressSanitizer/UBSan (memory and undefined-behavior checks): **43 scene cases passed**, none failed or skipped, rebuilt
  from current source with documented sanitizer settings. Leak checking was off.
- Hardening (exploit-mitigation checks): **16 Release programs and the sanitizer scene program passed**.

Runs used the QOR-local lock sequentially, one compiler/rendering worker, two processors and a 1.25 GiB memory cap.
Evidence: `build/checkpoint-2026-11-04/lint-runtime/`.

Recovery code is unchanged from `8eca6ea`: trust, transaction tags (change identifiers), checked thumbnails, changed-copy
protection, listing provenance (origin) and the 8 MiB lock limit remain. That checkpoint passed
25 Studio and 89 asset-library Release cases, 89 asset-library and 21 Prism option sanitizer cases, 32 hardening checks and
88 wrapper/report tests. Those suites were not rerun here. CTest (test runner) evidence must remain complete and fresh.

Continue lint in small batches. Listing/planning still occupy the window thread; earlier largest-pack measurements were
257/402 ms, without a latency guarantee. Full OS/core, full products, Windows, native hardware and root-wide lint remain unrun.

VM (virtual machine) Create/Prove remains prepared in current `docs/CI.md`, requiring your separate go-ahead, a clean reviewed
revision and documented prerequisites. Neither ran. Qt's licensing answer and successful isolation proof precede a guest
kit/runner. Runner settings, isolation flags and the roadmap's other owner decisions remain unchanged.

# Where QOR OS stands

For Andrew, 5 November 2026. Local branch: `integration/owner-brief`.

All 14 worktrees (Git checkouts) remain. This session owns implementation. The QOR-local lock serializes heavy jobs;
no cross-project prerequisite exists. Nothing was pushed.

## Verified checks

Commercial Qt 6.12 runs hidden in WSL (Linux inside Windows), fenced from the graphics processor, with two processors,
one worker and a 1.75 GiB hard cap. ASan/UBSan check memory errors and undefined behavior; leak checking is off.
Windows: two processors, one worker; no memory-cap claim.

- QQ at `39b0768`: **91 asset-library/195 game-build/3 Store-to-Player cases per product build** pass, zero failures/skips.
  Recovery, transaction tags, trust, checked previews and provenance retain regression checks.
- Linux Store format 2/legacy/Library/pages/front-end: **621/11/23/18/39 cases per main profile** pass, zero failures/skips.
  Shape's **52 Scene/10 physics cases** and Inspector's **5 cases/3 functions** pass per product build.
- The complete journey at `f99e007` passes per product build, including actual Player playback after project removal.
  Prism (compositor and shell) records a stand-in; actual Player playback runs separately. Prism is not under sanitizers here.
- Failed scripts omit the complete request object while keeping **exit 4**. Benchmark output failures return **exit 1**;
  **11 collector cases per Linux profile/9 on Windows** pass, zero failures/skips.
- Windows Store: **614 cases/all 149 Windows functions pass** in standard private Windows Temp, zero failures/skips.
  Four Unix-only functions are not compiled there.
- Hardening (exploit mitigations): **33 normal/31 sanitizer Linux main programs, 16/13 products, 21 Windows program headers** pass.
- Windows services: **17 cases/15 functions pass**, zero failures/skips; private-pipe permissions verified.
- Readiness wording: **16 Python tests per platform pass**; workflow **Windows 14 pass/1 skip; Linux 15 pass**.

## Remaining and next

Checkout-local Windows Store still fails: **613 pass/1 fail**, zero skips. Diagnostics read **Access denied (5)** after the
staging rename failed. Two checkout-root controls fail; two outside-root controls pass. Cause unresolved; temporary
diagnostics were restored. No permissions or security settings changed, and no other process was interrupted.

QQ lint (interface-language checking) fails at **149 console/174 structured warnings**, four Inspector findings removed,
none added. Windows root lint: **seven modules, zero warnings**. **12 Wayland metadata warnings** match baseline. A pinned Rust dependency fails its warning gate; repair belongs
upstream. Listing/planning remain on the window thread without a latency budget.

Next: independent Windows Store/service checks. Exact commands and resume point: `build/autonomous-2026-11-04/HANDOFF.md`.
Full OS/Rust suites, Windows products, native Linux graphics/sound/controllers and image/install proof remain unrun.
Software observations clear no hardware budget. Zero-warning gates remain unmet.

VM (virtual machine) Create/Prove commands are prepared in `docs/CI.md`; neither ran. Next owner decision: Create go-ahead.
Licensing prerequisites and actual proof gate the guest Qt kit and runner. No runner settings, isolation flags, licences
or accounts changed; nothing was merged, deployed or published.

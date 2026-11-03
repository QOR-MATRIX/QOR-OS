# Where QOR OS stands

For Andrew, 3 November 2026. Recovery continued; the zero-warning gate remains open.

QOR OS joins Prism (desktop and compositor), Lumen (design language), QQ, ARQADE and QOR Media. This session owns implementation;
you confirmed the other recovery sessions are idle. Integration was clean at `f65c90d` on `integration/owner-brief`.

## What this change carries

W10's recovery commit `6e495d2` and its six unfinished files were preserved and continued in its existing worktree,
`.claude/worktrees/agent-a3d662eca31450d64`. The original patch is saved. Git cannot inspect unsaved editor buffers.

Recovery checks the project folder and placed bytes, leaves files alone when staging is gone, and gives each lock a distinct tag.
Update passes the project's trust check; outside titles stay plain; thumbnails use checked bytes. Copy and Update run in the
background; Save, Build and changing project wait. Signed-listing provenance (where a pack came from) is kept within the existing
8 MiB lock limit. Listing and planning still run on the window's thread.

## Evidence and limits

All runs used commercial Qt 6.12 under WSL (Linux inside Windows), hidden, drawn in software and fenced from the graphics
processor. The existing shared lock kept builds serial, with four compiler and Mesa rendering workers.

- Release (optimized) asset-library suite: **88 passed, none failed, one opt-in benchmark skipped**.
- AddressSanitizer and UBSan (memory and undefined-behavior checks): **195 game-build and 88 asset-library cases passed**;
  only that benchmark skipped. An earlier Release game-build run passed 195 cases before the final Build guard.
- Prism's product-host suite passed under sanitizers. Hardening (exploit-mitigation checks) passed 16 programs in each QQ build.
- Root QML lint (Qt's interface-language checker): no QML diagnostics; its build emitted **12 Wayland type-registration warnings**.
  QQ lint: **439 warnings**, the same texts and counts as untouched `f65c90d`. **The zero-warning bar is not met.**
- Pre-commit (change checks) passed. GitHub's `main` remains `4c620c2`, the merge of pull request #3, matching reviewed `a31154c`'s
  tree. Lint and Security passed; Build failed at readiness because both Qt jobs skipped. Nothing was built by those jobs.

Commands, source fingerprints, failures and logs: `build/recovery-2026-11-03/` in the integration checkout, also
`~/qor-work/recovery-2026-11-03/` in WSL. The stale Store helper and outdated text assertion were corrected and rechecked.
Full OS/core and all-product suites, Windows, native Linux hardware and new before/after shell benchmarks were not run.

## Next and owner decisions

Resolve the warning gate, then the roadmap's work that needs no graphics card. Nothing was pushed or published; no VM (virtual
machine) was started and no runner setting changed. The decisions in `docs/ROADMAP.md` and `docs/CI.md` stand: your separate go-ahead
and about 7 GB free for the VM's first Create and Prove; Qt's licensing answer before any kit enters it; the software frame-rate
floor and eased checks; a Windows run while you are away; and the release, signing-key, outside-app access, engine-licence and naming decisions.

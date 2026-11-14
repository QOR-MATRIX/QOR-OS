# Where QOR OS stands

For Andrew, 14 November 2026. Draft pull request #10 continues QQ, the game
engine, on `fix/native-play-clock-waits`. All 14 worktrees were clean at this
session's admission; local and remote heads matched `57d9cab`. The Pi had no
active build or test worker. Existing work and evidence are preserved.

The current Store-installed game still fails its unchanged 60-second startup
test on the Raspberry Pi 5. A separate diagnostic reaches the first frame in
64.204 seconds, then reports 19.9 frames per second and exits cleanly. It uses
llvmpipe, the software renderer, inside private device and network namespaces.
That longer diagnostic is not a passing acceptance test. A new regression
also reproduces duplicate initial sky construction; its repair is being tested.
Receipts are in `build/development-2026-11-14/session-06/`.

The previous 2D Player change has 83 passing scene results in both optimized
native and instrumented memory/undefined-behavior runs, plus four selected
gameplay tests, recorded in session-05. Those are dated, focused results.
Whole-product, Windows, hardware and bootable Q Pi acceptance remain open.
The last runtime lint check reports 53 warnings and fails its zero-warning gate.

Both registered Qt runners remain offline and their isolation-enabling
variables are absent. CI, automated build and test, remains paused for Qt.
The proposed isolated virtual machine has no local provisioning/proof record;
this terminal has no administrator or Hyper-V administrator access. The original
build coordinators and 3,072-MiB memory floor remain mandatory.

The long-term direction is one canonical development repository for QOR OS,
Q Pi, Demiurge Chain, ARQADE and associated systems. QOR-0028 records planning
only: reconcile current ownership, history, licences, security boundaries,
consumers and deployments in a separate proposal after this milestone is
reviewable. No code moves, restructuring or visibility changes occur here.

Next: validate the sky repair and report PR #10's exact passing and remaining
gates. The Store/Orb timing, display-library leak, broader product checks,
upstream dependency findings and isolated-runner proof remain separate work.

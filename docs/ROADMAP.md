# Roadmap

The only roadmap for QOR OS. Each phase ends at its **exit**, shown working,
not on a date; durations are rough guides for one founder working with agents.
What 1.0 contains is [QOR-0007](decisions/QOR-0007-what-1-0-ships.md). The
phases are tracked as GitHub milestones.

Dated completed items below preserve the result at that revision and environment;
they are not a rerun on today's branch. Qt CI jobs remain paused pending isolation
([CI.md](CI.md#what-ci-trusts)). Private x86-64 VM evidence and the still-unqualified
Q Pi route are distinguished in [the image deliverable](#release-deliverable--the-image).

QOR OS began on 26 October 2026 from QTOS's Phase 0 and most of its Phase 1
([history below](#history-qtos-phases)) and from Demiurge's device-side
products ([UPSTREAM.md](UPSTREAM.md)).

## The order of work

The owner's 14 November direction is one canonical development repository for
QOR OS, Q Pi, Demiurge Chain, ARQADE and associated systems
([QOR-0028](decisions/QOR-0028-plan-one-canonical-development-repository.md)).
First make the current milestone reviewable in PR #10, retaining explicit
failed and untested gates. Then propose a separate consolidation plan with
verified history/licence and access boundaries, active work, consumers,
deployments, cutover checks and rollback. Destination and visibility remain
undecided; this milestone authorizes no migration or restructuring. QOR-0002's
network split, QOR-0003's upstream core, QOR-0015/0017's product and shared-rule
contracts and QOR-0027's homepage deployment all require reconciliation in that
later plan. Current contracts remain in force.

The owner's 13 November hosting correction moves the QORSYNC.DEV homepage into
`site/` here, targeting the existing Astra Matrix / qor project from QOR OS `main`
([QOR-0027](decisions/QOR-0027-the-homepage-lives-in-qor-os.md)). Homepage build and
privacy checks are separate from device qualification; [HOSTING.md](HOSTING.md)
records connection and live-deployment verification. Network/core ownership and
the device work below are unchanged.
PRs #8 and #9 are merged; remote readback confirms that connection, and QORSYNC.DEV
serves QOR OS main `f4c9863`. The original live check found proxy-injected analytics;
the deployed `no-transform` response header now guards the reviewed HTML privacy
boundary. All 47 public files, including both HTML pages, match reviewed source
or locked-package bytes. Security headers, canonical links, redirect, private-file
exclusion and Analytics script delivery pass; dashboard collection was not exercised.
Current hosting receipts are in `build/development-2026-11-13/session-09/`.

The Git reconciliation verified all 14 QOR worktrees clean at admission, fetched
and pruned eight stale remote-tracking refs, and retained every local checkpoint.
The two apparently unmerged histories are already integrated by identical tree
or patch. Upstream's pending homepage ownership handoff is committed and pushed
through `846d9ba` on draft PR #118; duplicate homepage Analytics PR #119 is closed.
The core pin stays unchanged. The service audit still fails: stable SQLx 0.9.0
excludes the SQLite binding versions patched for RUSTSEC-2026-0459. No exception
or dependency-source override was added. Receipts: session-10 beside the above.

The owner's milestones ([QOR-0021](decisions/QOR-0021-projects-engines-and-two-stores.md)). The phases and sections below say
what each holds. A step is done when it is integrated and its checks have evidence, kept to the configuration they ran on.

1. **The real baseline.** Checked on 1 November against an outside review of `a3a2fd4`: the repository was public against its
   documents, and is now private ([SECURITY.md](../SECURITY.md#known-limits)); Linux CI skipped the products; `image/` is a plan
   without an accepted boot/install path (startup/installer branding has a checked data layer and first-run app;
   a confined Fedora/QEMU VM lab is in progress under QOR-0024); Unreal is a decision, not code;
   and Epic changed its clause on AI, which QOR-0020 missed
   ([QOR-0022](decisions/QOR-0022-assistants-and-unreal-engine-after-epics-change.md), proposed).
2. **The first whole journey, on native Linux:** open a QQ project workspace, import an authorised asset, edit, save, build a Linux
   game, and launch the packaged result from the player library. Switching between two projects and restarting restore the right
   state, lose no unsaved work and mix nothing. Editor play and running a packaged game are different results. Real input, sound
   and a real graphics card need the owner's Linux machine.
3. **One authorised asset pack in the Creator Store and one finished game in the Player Store,** installed and imported or launched,
   delivered free. Paid commerce is a separate goal: it needs sandbox checkout, entitlement, download, and refund or revocation
   before anyone says sales work. No live charge and no publication without the owner's word.
4. **The shared graphics, input and display work, then selected Unreal and Unity workflows,** with the image, install, update and
   rollback alongside. Unity's operations stay gated ([QOR-0021](decisions/QOR-0021-projects-engines-and-two-stores.md)).
5. **The release gates below,** and then paid publishing in its phase, and console capture when its hardware can be tested.
   Publishing remains the owner's act.

## Phase 0 · Clear the ground

**Complete, 27 October 2026.** *A few days.* Exit: `qor-os` builds and tests in CI on Windows and Linux, the carried
products included on Windows.

- [x] Disk space for the work: 204.7 GB free on the build drive (was 9.6 GB), by cleaning Demiurge's Rust build output
- [x] `QOR-MATRIX/qor-os`, private, seeded with QTOS's full history; QTOS frozen at `37b621e`, with a note pointing here (`3ecdded`) ([QOR-0002](decisions/QOR-0002-one-repository-from-qtos-and-demiurge.md))
- [x] QQ, ARQADE's app, games and fixtures, and QOR Media carried from demiurge-chain with their history, at upstream paths; `tools/upstream/demiurge.sh` pulls and patches ([UPSTREAM.md](UPSTREAM.md))
- [x] The QOR core built from demiurge-chain at a pinned commit and tested through its C bridge (`core/`, `tst_qorcore`; [QOR-0003](decisions/QOR-0003-the-qor-core-at-a-pinned-commit.md))
- [x] One rulebook ([AGENTS.md](../AGENTS.md)) and one decision log, QOR-0001 to QOR-0009 ([decisions/](decisions/README.md))
- [x] The carried products build and pass their suites here (`tools/dev/products.ps1`)
- [x] One name: QOR OS in the interface, the documents and the code, QTOS only for the predecessor (27 October, [QOR-0009](decisions/QOR-0009-the-name-is-qor-os.md))
- [x] The CI runners serve QOR OS: from the organisation since 27 October, from the repository since 29 October, when GitHub stopped giving the organisation's runners jobs ([CI.md](CI.md#where-the-runners-are-registered))
- [x] CI green on Windows (with the products) and Linux: the first run on 26 October passed every job, the products on their first attempt
- [x] Demiurge's L13.2 merged upstream (#107, `e99c7ce`, 27 October: QOR ID's client, the chain client, Social's client, the Library's index, the Mesh and Qontrol in the core), and the core's pin moved to it with the `desktop` feature ([UPSTREAM.md](UPSTREAM.md#moves-so-far))

## Phase 1 · One stack

*About 2–4 weeks.* Exit: Orb Run plays inside Prism on Linux with a DualSense, and Linux CI runs
QQ's, ARQADE's and QOR Media's suites.

**Current local priority, 13 November: native products, the creator/player journey and upstream dependency repair.**
Upstream's workflow hardening passes the required build/test jobs at `97dcb13`:
213 chain, 245 core and 109 Launcher host tests pass, with 10/five ignored
core/host tests. The service audits remain failed. The separate two-validator
recovery check passes all 13 checks on a hosted local test network; its optional Rust 1.100 report retains the SDK macro
lint blocker (66 generated DRC-369 diagnostics). The required compiler stays
1.98.1 and QOR OS's core pin is unchanged; [UPSTREAM.md](UPSTREAM.md) owns the
repair and integration procedure.

The later native Pi continuation completed the added Prism boundary regressions
in both Release and ASan/UBSan: all seven selected suites pass without skips,
including 43 pointer-capture and six window-management Qt Test results. The
test-only correction activates a second client through Shelf after asserting
focus-stealing prevention. All 11 programs in each configuration pass hardening
and ARM64/16-KiB checks; Release QML lint has zero warnings. These focused builds
exclude the core and use software rendering and product stand-ins. On 13 November,
all 66 saved Prism/QQ evidence files and 1,404 source files were rechecked; local
differences from the built snapshot were documentation only. Native receipts:
`build/q-pi-connect-2026-11-12/session-01/`. Earlier Windows/WSL attempts below
remain historical evidence for that host, not a blocker on the completed Pi runs.

The 13 November core-enabled Windows configure/build completes, all 22 selected
software suites pass without failed/skipped suites, hardening passes 33 programs
and QML lint has zero warnings. The pinned `proc-macro-error2` 2.0.1 dependency
still emits Rust's future-compatibility warning; the warning-free gate remains
unmet and requires an upstream fix followed by a reviewed core-pin update.
These tests exclude GPU-labelled suites and do not cover full products, native
Linux OS acceptance or isolated CI. On the Pi, the unchanged pinned core builds
with the `desktop` features and Rust 1.98.1; its C bridge suite passes nine Qt
Test results without failures/skips, and the program passes hardening and
ARM64/16-KiB checks. Its dependency warning remains too. This focused target
does not complete the full native OS build/test gate. Receipts:
`build/development-2026-11-13/session-01/`.

The later continuation after pushing `9d67ab2` completes the full core-enabled
native development build and all 27 suites selected without the `gpu` label.
The first run exposed four Store cases failing in two suites because signed
demonstration metadata omitted `linux-arm64`. The fixture generator and its
regression now include ARM64; only demonstration signatures/metadata changed,
with all packaged content preserved. The final 27-suite run has no failures or
skips; 42 programs pass hardening, 44 linked files pass ARM64/16-KiB layout checks,
and QML lint is warning-free. All 15 generator tests and three affected Windows
Store/Library suites pass. QQ's full scene-data and Player sandbox suites pass
56 and three Qt Test results, including setup/cleanup, without failures/skips.
The Rust warning still prevents warning-free acceptance; full product/sanitizer,
whole-journey, GPU and isolated-CI gates remain. Receipts, including initial
failures: `build/development-2026-11-13/session-02/`.

The next continuation passes seven non-windowed QQ launcher-protocol cases
(nine Qt Test results including setup/cleanup), without failures/skips. These
use a local stand-in and cover handshake, ordering, refusal, framing and loss of
connection. The cached binary and all 1,404 snapshot/overlay source hashes were
verified before execution; the initial stale-manifest refusal is preserved in
`build/development-2026-11-13/session-03/`. The two windowed publication cases were subsequently completed in the full
publishing-suite run recorded in the Q Pi track below. The complete creator/player
journey subsequently passes once in 201.292 seconds with its unchanged 300-second
deadline. Real Studio and Player programs run; Prism records its Library launch
through a stand-in, followed by separate real Player playback. This is software
evidence, not a real Player window inside Prism or hardware acceptance. The broad
QQ run reports four `tst_play` failures (54 passes) and hits its 7,200-second
aggregate deadline during `tst_player`; remaining suites did not complete.
Receipts and preserved failures: `build/development-2026-11-13/session-07/`.

The next native continuation reproduces all four failures with both threaded and
basic rendering, using the same binary and deadlines. Diagnostics show only
325–409 ms of physics during the three 5–6-second contact/trigger waits. Orb Run
advances no frame before its startup wait expires. A separate six-wait proposal
uses the existing software physics clock for contact, passage and settling; all
three cases pass (five results including setup/cleanup), with no failures/skips.
The owner approved that exact six-wait patch and it is now applied. GPU deadlines, exact outcomes, tolerances,
the Player frame-rate floor and release gates remain unchanged. Orb Run startup
passes in a separate first-frame probe, but that full case still fails its later
lantern-idle timer wait. The startup probe is not a fix or a passing full case.
The remaining product suites are incomplete. Evidence and the reviewable patch:
`build/development-2026-11-13/session-10/`.

The approved source is rebuilt in the native snapshot; its binary hash matches
the passing three-case proposal exactly, and all 1,404 source hashes are checked.
Hardening passes all 16 QQ programs. Native QQ `all_qmllint` fails with 103
warnings. The unchanged Store-installed Player case exceeds its internal
60-second deadline under both render loops. `tst_playerwall` passes all three
results with no skips; `tst_lockdown` is incomplete at this probe's 300-second
suite budget, which is shorter than the normal product-harness budget. No
completed-suite pass is claimed for it. The cache is current, no owned workers
remain, and the original native lock is free.

The session-11 follow-up fixes QQ's loaded-scene property shadowing Qt's viewport
root and migrates the Player and ARQADE callers. Its regression reproduces the
old collision; the full scene suite passes 57 results after the fix. Native
runtime lint falls from 103 to 102 warnings. Store playback still exceeds its
unchanged 60-second deadline. A separate affected-play probe passes model startup
and two contact cases, then reaches its diagnostic budget during the prefab case;
it is not a completed play-suite pass. The original failures and scope corrections
remain in `build/development-2026-11-13/session-11/`.

Its instrumented scene suite now passes all 57 results offscreen with leak
checking enabled. Test-owned viewports have hidden window owners and queued
destruction is drained. The Wayland run retains a 96-byte allocation, reproduced
even by a numeric-only case, so that platform's sanitizer gate remains failed.
The final full QQ rebuild is free of compiler and license-client diagnostics.
All 57 scene results pass, and all four affected playback functions complete
with six Qt results and no failures/skips in 305.333 seconds. This completes the
earlier bounded batch without changing any assertion or internal duration.
Four ARQADE game/arena caller functions also pass six results without failures
or skips. Its build and context warnings remain. Hardening passes all checked
programs; the final source/cache/lock audit passes independently of the documented
outer-wrapper child-status error. Whole-product acceptance is still incomplete.

The 14 November Shape continuation binds private collision/body factories to
their owning instance and keeps playing/view reads valid across structural
parent types. A new regression checks updates, reparenting, detachment, strict
boolean activation and body identity. The final optimized native build passes
58 scene results and three selected physics functions (seven results including
setup/cleanup), without failures or skips. Runtime lint falls from 102 to 88
warnings, with 14 removed and none added; that gate remains failed. Hardening
passes all 16 QQ programs. The full 58-result scene suite also passes offscreen
under ASan/UBSan with leak detection enabled, and its program passes hardening.
Evidence: `build/development-2026-11-14/session-01/`.
The earlier Store/Orb timing, Wayland leak, ARQADE diagnostics and full-product
gates remain unresolved. Store, Orb Run and ARQADE suites and the Wayland
sanitizer run were not repeated for this increment.

The next 14 November continuation checks Shape's actual physics type before
calling motion methods. A plain node or mismatched static body now returns false
without a script error; public properties and every existing test are unchanged.
The new regression reproduces both runtime failures before the fix. One separate
null-assignment error in its initial fixture is preserved and corrected by
constructing a non-playing Shape. The full optimized native QQ build has zero
compiler/license-client diagnostics. All 65 scene results and seven selected
physics results pass without failures/skips, including dynamic/kinematic motion,
contact reporting and all three plane-lock rows. Hardening passes 16 programs.
All 65 scene results also pass offscreen under ASan/UBSan with leak detection
enabled, and the sanitizer program passes hardening. The separate Wayland leak
finding and broader product suites were not rechecked for this increment.
Runtime lint falls from 88 to 83 warnings: all five remaining Shape findings are
removed and none added; the whole lint gate remains failed. Current evidence:
`build/development-2026-11-14/session-02/`. The next Git continuation follows the
preserved recovery procedure: after two idle-worker checks it restores the exact
Windows helper to its original path and proves independent lock exclusion and
child exit-status propagation. All-file hooks and the complete working-tree
secret scan pass. Windows product builds remain unrun. Recovery and Git receipts
are in session-03 beside the native evidence. The original native coordinator and
3,072-MiB floor remain in force. Full products, hardware and isolated CI remain
unaccepted.

The next native continuation reads Trigger's collision count only through an
actual TriggerBody while keeping the public Node property. The new regression
reproduces a false count of 17 from an ordinary node before the fix. Structural
parent state and optional entity callbacks keep their behavior; four new results
cover both collider forms, live dimensions, reparenting, detachment, strict
activation, missing volumes and entity/null callbacks. Existing tests are
unchanged. The complete optimized QQ build has zero compiler/license-client
diagnostics. All 69 scene results and six selected trigger/group physics results
pass without failures/skips; hardening passes 16 programs. Runtime lint falls
from 83 to 76 warnings: all seven Trigger findings are removed and none added.
All 69 scene results also pass offscreen under ASan/UBSan with leak detection
enabled, and the sanitizer test program passes hardening. The older Wayland
leak, Store/Orb timing and ARQADE findings were not rerun for this increment;
full products and hardware acceptance remain open.
The whole lint gate still fails. Receipts, including the failing original
regression: `build/development-2026-11-14/session-03/`.

The 3D Player continuation requires an actual CharacterController for its frame
updates and movement commands, while keeping the public Node property and the
structural scene-parent interface. The original runtime writes movement to an
ordinary node and leaves updates running for static and trigger bodies; three
new rows reproduce those failures after two fixture import corrections. Seven
new rows also check the valid and missing bodies, strict activation, pause
changes, selection ownership, reparenting and detachment. Existing assertions,
tolerances, internal durations and movement calculations are unchanged.
The full optimized native build has zero compiler/license-client diagnostics;
all 76 scene results and six selected player/controller/trigger results pass
without failures/skips. Hardening passes 16 programs. All 76 scene results pass
offscreen under ASan/UBSan with leak detection enabled, and that test program
passes hardening. Runtime lint falls from 76 to 67 warnings: all nine Player
findings are removed and none added. The whole lint gate still fails.
The final native audit verifies all 1,404 snapshot fingerprints, the current
build cache, tested executable identities and sanitizer instrumentation. The
original coordinator and 3,072-MiB floor remain in force. Broader product,
Store/Orb, Wayland leak, ARQADE, hardware and upstream-audit gates remain open;
they were not rerun here. Receipts: `build/development-2026-11-14/session-04/`.

The Player2D continuation checks an actual QQ Sprite before applying visual
position and facing updates. A lookalike node stays untouched and an unrelated
entity no longer raises a script error; both failures are reproduced before the
fix. Named lookup and public property types are preserved. Seven new rows also
check real/missing visuals, strict activation, carrier ownership and dimensions,
player selection, paused interpolation, replay state, reparenting and detachment.
The full optimized native build has zero compiler/license-client diagnostics;
all 83 scene results pass normally and offscreen under ASan/UBSan with leak
detection enabled. The same four existing tiled-level, top-down, recorded-replay
and 2D-trigger tests pass before and after the change, six results per run with
no failures/skips. Their source, assertions, tolerances and durations are
unchanged. Hardening passes all 16 QQ programs and the sanitizer program.
All 14 Player2D lint findings are removed without adding any; 53 runtime warnings
remain and the whole lint gate still fails. The final native audit verifies
1,404 snapshot fingerprints, current build output, tested binaries and sanitizer
instrumentation. The original coordinators and 3,072-MiB floor remain in force.
Broader product, Store/Orb, Wayland leak, ARQADE, hardware and upstream-audit
gates were not rerun here. Receipts: `build/development-2026-11-14/session-05/`.

The next continuation reproduces the Store startup failure on the current code
at its unchanged 60-second deadline. A separate diagnostic reaches a frame at
64.204 seconds, draws at 19.9 frames per second and exits normally; software
rendering threads account for nearly all its startup CPU time. Initial sky
construction also runs twice for a batch of colour properties. A new regression
reproduces that duplication; coalescing it passes all 84 scene results normally
and offscreen under ASan/UBSan with leak detection enabled. All 11 unfiltered
world-rendering results pass in 1,229.256 seconds, with no failures/skips. The
full optimized native build has no compiler/license-client diagnostics;
hardening passes 16 QQ programs and the sanitizer program. Runtime lint remains
failed at the same 53 findings. The Store test reaps its own timed-out Player
before teardown, eliminating the running-process warning, but still fails the
same startup deadline. The final diagnostic reaches a frame at 63.827 seconds
and exits normally at 20 frames per second; this is not a demonstrated speedup
or a passing Store gate. Assertions, tolerances, deadlines and rendering settings
are retained. Receipts: `build/development-2026-11-14/session-06/`.

The next 14 November continuation builds all QQ targets on Windows with Qt
6.12.0/MSVC 2022, serially under the original coordinator and unchanged memory
floor. Compilation has no compiler/license-client diagnostics; all 84 scene
results pass without failures/skips, and all 15 programs pass hardening. The
offscreen test emits a missing-font warning and a hardware-audio buffer warning
despite `QOR_AUDIO_OUTPUT=null`. Its assertions are local diagnostic evidence;
Windows audio isolation and the full product gate remain open. Runtime lint
fails with 53 warnings. The final evidence collector wrongly assumed Linux's
`tests/` executable directory after those checks; its failure is preserved and
the actual CTest command is reconciled separately without repeating the tests.

The owner confirms the Pi is online while this workstation is on a different
network. Its local hostname does not resolve and no native job starts. A
timestamped Player diagnostic is prepared under the same native lock, private
software display and source checks. Installed Qt source identifies environment-map
filtering as a profiling hypothesis; no speedup, visual change or Store pass is
claimed. No network/access configuration changed. Current local receipts:
`build/development-2026-11-14/session-07/`.

The owner subsequently runs that trace through the Pi's existing Raspberry Pi
Connect remote shell. The supplied terminal report records 1,404 source hashes
checked, separate isolation namespaces, a clean diagnostic exit, and the original
lock available afterward. First frame is 63.838 seconds; Qt reports a 63.132-second
rendered frame with 62.791 seconds in its swap stage. That timing includes graphics
completion/presentation and does not by itself isolate the cause. A minimal
comparison with and without sky lighting is prepared under the same native
coordinator and private software display; its subsequent result is below. Full trace artifacts
remain on the Pi; the owner-provided report and preparation are preserved in
`build/development-2026-11-14/session-08/`. Production sources, rendering quality,
test deadlines and the unresolved Store gate remain unchanged. Both prepared
scenes lint without warnings. Local all-file hooks stop at 2,897 MiB available,
below the unchanged 3,072-MiB floor; the incomplete run is preserved without a
retry or unrelated service changes.

The owner's next Connect report isolates the sky-light path in that minimal
comparison: first frame is 0.474 seconds without the light probe and 61.054
seconds with it, a difference of 60.580 seconds. Both report llvmpipe, four
completed frames, no recorded diagnostics and exit zero. Scene hashes match the
prepared inputs; the outer report records 1,404 source hashes, separate isolation
namespaces and an available coordinator afterward. This is a synthetic diagnosis,
not an improved Player or passing Store case. `session-09/` preserves the
owner-provided report under `build/development-2026-11-14/`.

The later direct SSH continuation through Tailscale verifies the existing key
and saved Pi host identity and completes the precomputed-lighting diagnostic.
Qt's public texture-data API exports the exact default procedural texture;
an EXR image preserves its half-float pixels for the installed Qt baker. In the
same private software display, the procedural first frame takes 61.529 seconds
and the precomputed first frame 0.774 seconds. Both 1280-by-720 captures have the
same hash and zero differing pixels across the sky and four differently shaded
spheres. The helper builds without diagnostics, passes hardening, and both
scenes lint without warnings. The first helper attempt stopped because its QML
component was not ready; a local base URL and bounded loading wait corrected it.
Both attempts and the retrieved earlier traces are preserved in
`build/development-2026-11-14/session-11/`. The original coordinator verifies
1,404 source hashes, separate namespaces, sustained and continuous resource
headroom, and clean completion. That diagnostic left production sources and test
deadlines unchanged; integration and the Store case followed below. A
2,491-MiB Windows snapshot prevents local heavy checks and leaves all-file hooks
incomplete. No unrelated service was changed.

The completed integration packages both palettes authored in the bundled scenes,
including the World default. Matching uses four colour values, including alpha,
without consulting the game name or path. Other palettes remain procedural and
explicit sky images retain precedence; exposure, rotation and rendering quality
are unchanged. Both baked palettes have pixel-identical diagnostic captures;
Ember Climb's first frame takes 0.768 seconds versus 61.294 seconds procedurally.
The native `q-pi-release` preset builds all QQ targets without compiler/license
diagnostics. All 85 scene results pass, including selection, resource hashes,
colour/alpha edits and image precedence. The unchanged Store-installed Player
suite now passes three Qt Test results in 8.371 seconds with its original
60-second internal deadline. All 11 world-rendering results pass in 340.938 seconds;
all 85 scene results also pass offscreen under ASan/UBSan with leak checking
enabled. Hardening accepts all 16 QQ programs and the sanitizer program. Runtime
lint still fails with exactly the previous 53 findings, none added or removed.
`build/development-2026-11-14/session-12/` preserves
the first configure refusal for a missing kit environment variable, the corrected
preset invocation and subsequent native results. Windows heavy checks remain
stopped at a 2,501-MiB snapshot, below the unchanged 3,072-MiB floor; all-file
hooks remain incomplete. General edited-palette startup cost and platform
validation remain open.

The following Orb Run investigation retains the unchanged case's startup
failure. A separate trace observes zero ticks at that failure, the first eight
ticks after 10.071 seconds of waiting, and only 24 logic steps (0.4 seconds) before the lantern
wait expires. The lantern subsequently resets; the next original wall wait
fails at orb replacement. The first proposal still fails at that replacement.
The revised proposal adds a software-only first-tick admission and measures
lantern reset, orb replacement and growth on their logic clock, keeping their
three/four/three-second game-time limits and bounded wall limits. The complete
case passes three Qt Test results in 184.714 seconds with no failures/skips;
the native build has no compiler/license diagnostics and hardening accepts all
16 programs. GPU branches, predicates, tolerances and unrelated waits retain
their code. The runtime and game logic are unchanged. This software acceptance
change awaited the owner's decision under `AGENTS.md`; the original test was
restored on the Pi before approval. Evidence and the
reviewable `orb-proposal-v2.patch` are in `build/development-2026-11-14/session-13/`;
the original failure, diagnostic, first proposal and text-decoding correction
are preserved. Windows heavy checks remain stopped at a 2,536-MiB snapshot.

The owner then approved that exact four-change Orb proposal. It is applied
byte-for-byte, and the native acceptance rebuild passes without compiler/license
diagnostics; hardening accepts all 16 QQ programs. All 1,409 native source hashes
and the rebuilt executable match the previously passing proposal, so its complete
184.714-second case supplies the software functional result without a duplicate
run. Approval, current source/binary proof and free-lock verification are in
`build/development-2026-11-14/session-14/`. Windows measured 2,791 MiB, below the
unchanged floor. Full products, hardware and real-time performance remain open.

The following session-15 continuation completes the native QQ suite attempts.
Gameplay passes 58 results; Studio 29, workspace 241, game builds 195, lockdown
24, publishing 11, network confinement three and Store launch three. The asset
library passes 91 with its optional benchmark skipped. The complete Studio
creator journey passes, and `first_journey` passes through Prism's Library and
the packaged Player in 202.406 seconds under its original 300-second limit.
The Library step uses the test launcher before running the real Player on its
private display; real Player rendering inside Prism remains a hardware gate.
Scene/world passes are retained by matching current executable fingerprints.

The bundled playground's HDR lighting is now precomputed with identical pixels,
fixing two Player launch timeouts. Full QQ qualification remains open: the agent
suite has one timing failure; the Player still fails its unchanged 20-FPS floor
at 0.5 and 18.9 FPS and skips unavailable Landlock confinement. Runtime lint
retains exactly 53 findings. No assertion, timing boundary or rendering quality
is relaxed. Native builds and hardening pass, all 1,412 source files and 15
executables match receipts, and the original lock is free. All complete suites
stay above the 3,072-MiB floor, with a 4,923-MiB minimum. Windows heavy checks
remain stopped below that floor. Evidence is in
`build/development-2026-11-14/session-15/`.

The 15 November follow-up removes Game's 24 runtime lint findings while keeping
its public object handles and compatible scene/follower interfaces. The new
preservation cases pass before and after the change; all 87 scene results pass
normally and offscreen with address/undefined-behavior sanitizers and leak
checking. Complete native gameplay passes all 58 results under its approved
timing boundaries. Store launch passes three and the packaged-game creator
journey passes in 201.985 seconds under its unchanged 300-second limit. Player
retains both FPS failures (0.5 and 18.9 against 20) and the Landlock skip.
The other 29 lint warning lines remain unchanged. Separate traces of
the original Player and agent concentrate cost in software rendering and its
synchronization; these diagnostic timings do not replace acceptance results.
The coordinator, memory floor, rendering quality and approved timing boundaries
are unchanged. Evidence: `build/development-2026-11-15/session-01/`.
The ARQADE caller rebuild still emits the existing Qt `GuiPrivate` compatibility
warning; compilation exits zero, but the build gate fails before caller tests.

The next 15 November increment removes Behaviour's 11 lint findings through
direct dynamic accessors, preserving compatible logic objects and parent state.
Focused tests pass before and after the change; all 89 scene results pass
normally and under offscreen address/undefined-behavior sanitizers with leak
checking. Callback payloads, prefab target lookup, frame guards, parent changes
and logic replacement remain covered. Complete native gameplay passes all 58
results with its approved timing boundaries and source unchanged. The other
18 lint findings are unchanged. Store launch passes three and the packaged-game
journey passes in 201.377 seconds under its 300-second limit. The full agent
suite reproduces its prior failure exactly (21 pass, one fail, one optional
skip); Player retains two FPS failures at 0.6 and 19.5 against 20, with 13
passes and the Landlock skip. All runs preserve the original guard and
isolation; 1,966 resource samples stay at or above 5,427 MiB available.
Evidence: `build/development-2026-11-15/session-02/`.

The subsequent parent-state and TileMap increment removes 12 more lint findings.
Six runtime kinds retain their structural parent interfaces, while TileMap's
private factory result has its concrete collision-body type. Eleven focused
results pass before and after the change; the complete scene suite passes 97
normally and offscreen under address/undefined-behavior sanitizers with leak checking.
Complete native gameplay passes all 58 results with its approved timing
boundaries and source unchanged.
Six spatial-audio metadata findings remain, with no new findings. Inspection
shows the installed SpatialAudio metadata inherits QQuick3DNode but omits the
QtQuick3D module dependency; this is a candidate cause, not a verified fix.
The SDK and lint policy remain unchanged. Store launch passes three; the packaged-
game creator journey passes in 201.631 seconds under its original 300-second
limit. Agent retains its identical log-timing failure (21 pass, one fail, one
optional skip). Player retains two 20-FPS-floor failures at 0.5 and 19.7 FPS,
with 13 passes and the Landlock skip. All 1,997 resource samples remain at or
above 5,329 MiB available; the original guard, isolation, assertions and limits
are unchanged. Evidence:
`build/development-2026-11-15/session-03/`.

The next audit uses structured lint output and corrects the preceding console-only
remaining count: Runtime had 20 findings, with 14 in Music, Sprite and TileMap
omitted from its console output. Studio independently has 18 AgentPanel findings;
Player has none. The three runtime kinds now retain structural parent reads,
and Sprite motion requires an actual dynamic body. New regressions reproduce
script errors for ordinary and static replacement handles; all 14 focused
results pass after the fix. Fourteen structured runtime findings are removed
with none added, leaving six runtime and 18 Studio findings. A native temporary
metadata copy confirms the missing SpatialAudio dependency accounts for those
six runtime findings; after the source fixes, complete Runtime lint passes
with that copy. Unknown-property and incompatible-type controls still fail;
normal lint remains failed with the installed metadata. The complete scene
suite passes 109 normally and offscreen under address/
undefined-behavior sanitizers with leak checking; complete gameplay passes 58
with the approved timing and source unchanged. Store launch passes three, and
the packaged-game creator journey passes in 201.365 seconds under its original
300-second limit. Agent retains its identical log-timing failure and optional
provider skip; Player retains its two 20-FPS-floor failures at 0.5 and 19.5 FPS
and its Landlock skip. The installed SDK and lint policy are unchanged. Evidence:
`build/development-2026-11-15/session-04/`.

The subsequent integration connects that correction to the generated QML tools
through a build-local metadata copy. The installed Qt SDK and application
import environment remain unchanged. Six helper tests pass, including refusal
of unexpected/incomplete metadata and unreviewed broken versions; fixed kits
need no override. Both normal runtime lint targets pass with zero warnings,
and invalid-property/type controls still fail. Player remains clean, Studio
retains 18 warnings and the aggregate lint gate remains failed. The complete
native scene suite passes all 109 results normally and offscreen under ASan/
UBSan with leak checking; eight audio results pass. The creator journey passes
in 199.848 seconds under its original 300-second limit. All eight audited
executables match the preceding validation byte for byte. All 862 samples stay
at or above 6,588 MiB against the unchanged 3,072-MiB floor. Evidence:
`build/development-2026-11-15/session-06/`.

The following AgentPanel correction removes the remaining 18 Studio findings.
An explicit accessor retains its required structural controller and reads the
current object in bindings and actions. Native QQ `all_qmllint` now exits zero;
Runtime, Player and Studio structured commands each report zero warnings.
No diagnostic category, severity or source coverage changed. Six focused
results pass against the unchanged panel before the correction and offscreen
under ASan/UBSan afterward, with leak checking. Complete Agent passes 22 with
the identical existing log-timing failure and optional provider skip; complete
Studio passes 29. The packaged-game creator journey passes in 131.581 seconds
under its original 300-second limit. All 1,351 validation samples stay at
or above 5,835 MiB against the unchanged 3,072-MiB floor. Native compilation
and hardening pass. Existing assertions, approved timing, the original guard
and private namespaces are unchanged. Receipts: `build/development-2026-11-15/session-07/`.

The 15 November Windows continuation at source `5d1359e` passes all 22 selected
core suites, retaining the expected desktop-entry case skip and excluding nine
GPU-labelled suites. Core/QQ builds and hardening pass for 33/15 programs;
aggregate and all three QQ structured lint reports have zero warnings. The
known Rust dependency warning remains.
Six metadata helper tests and six inert AgentPanel results pass. The original
Windows coordinator and continuous 3,072-MiB guard stay in force. The owner
selected qualified checks on this PC; full product runtime checks still need
an audio/keyring boundary, and GPU checks were not run. Current Windows receipts:
`build/development-2026-11-15/session-08/`.

The session-13 V3D qualification at source `f0893ea` verifies the Pi graphics
processor inside a separate render-node-only sandbox, but does not qualify it
for Studio. `/dev/dri/renderD128` is character device 226:128, mode 0660, bound
to `v3d`; access works with existing account permissions. The direct probe and
private headless compositor use Broadcom V3D 7.1.10.2 / OpenGL ES 3.1. The original
Studio client instead selects desktop OpenGL 3.1, which cannot consume several
baked GLSL 150 shaders. No system graphics, service, permission or desktop change
is made; the original software configuration remains intact.

The original focused Agent cases report **3 passes, 1 failure, 0 skips in
28.341 seconds**. Three intentional QML errors are split into two entries by
intervening shader warnings: `typeErrors == 1` fails before `times > 5` is reached.
Wall-clock waiting passes. First surface creation to first Wayland frame callback
is 24.064 seconds, including startup; the first frame request-to-callback is
13.152 ms. These are wire timestamps, not the earlier instrumented play interval.
The changed private compositor has no physical vsync, so no direct speedup over
the sealed software baseline is claimed. An initial attempt using the original
offscreen compositor fell back to llvmpipe and correctly aborted on the existing
renderer assertion (1 pass / 1 failure / 0 skips; second case not run).

The existing Studio button/focus image check passes on V3D and on a matched
llvmpipe client control (3/0/0 each), but the V3D capture loses the sky and
environment lighting. The control uses the same headless compositor and original
binary and renders them correctly. Narrow UI passes do not establish viewport
correctness. Software timing remains open: the preserved original baseline is
3/1/0, `times > 5` fails with 1. Recommend supplemental deterministic real-error
counting/cursor coverage while retaining the unchanged natural-frame requirement;
no test contract or production code is changed.

All 1,414 original source hashes and the original Agent binary remain intact.
Native preset builds pass without diagnostics (RelWithDebInfo, not CMake Release).
All 216 samples exceed the 3,072-MiB guard, minimum 6,653 MiB; the native lock is
free and workers have exited. Qualification failed, so no new sanitizer, Release
acceptance or broader-suite pass is claimed. Next, use a bounded reversible
OpenGL ES 3.1 client diagnostic to test the shader/API mismatch, preserving image
and original timing requirements. Software support still needs attribution of
the dominant texture sampler to its Studio material/pass; hardware is no substitute.
See the [renderer evidence](PERFORMANCE.md#qq-agent-callback-timing-2026-11-15-isolated-pi-software-rendering).
Receipts: `build/development-2026-11-15/session-13/`; session-12 evidence is sealed
and preserved. Draft PR #10 and required isolated CI gates remain open.

PR #10's next product gates include Agent callback timing, Player failures and remaining
full-product and Windows runtime runs. The Wayland sanitizer leak, ARQADE diagnostics,
upstream dependency audit and isolated Qt CI remain open. Focused scene/rendering
checks do not qualify the full OS, platform, Q Pi image or physical hardware.

Windows/Linux Qt runners remain offline and unqualified for isolation. The local
session lacks Hyper-V administration rights and has no verified isolated VM;
observed free memory cannot fit the planned 6-GiB VM while retaining the 3-GiB
host floor. No runner flag, guard or unrelated service was changed. These are
separate blockers from the native software tests.

**Earlier 12 November Windows/WSL continuation.**
The warning candidate at the pushed `01ba206` checkpoint compiled without warnings in the focused Qt 6.12
Linux Release and ASan/UBSan builds. Each passes all 42 pointer-capture results
with zero failures/skips, including unique announcements and bind/destroy/rebind
for all five custom managers, cursor-shape v1/v2 and last-binding destruction.
Each also passes `tst_prismoptions`, `tst_prismoptions_device`, `prism_products`
and `prism_untrusted`. Root `all_qmllint` reports zero warnings in both builds;
hardening passes 15 Release and 36 sanitizer programs. Eighteen exercised program
identities were verified, with both sanitizer runtimes linked by all nine
sanitizer programs. Leak detection was off. The production/test sources remained
unchanged during this validation; the prior twelve-to-zero Windows metadata
differential is retained as separate historical evidence.

The owner explicitly requested optional host-service cleanup after the memory
refusals. One build then stopped at 3,040 MiB; additional cleanup allowed the
complete rerun under the original coordinator and unchanged 3,072-MiB guard.
Tests remained hidden, GPU-fenced and limited to software/virtual devices.
Startup settings were unchanged. This owner-authorised maintenance creates no
QOR service dependency or automatic cleanup step. The core library was disabled
in these focused caches; full-OS, product, hardware and isolated-CI acceptance
remain separate. Current evidence: `build/prism-completion-2026-11-12/session-02/`.
The original candidate and metadata logs remain under
`build/prism-metadata-2026-11-10/` and `build/prism-resume-2026-11-10/`.

The local continuation adds a real delayed-acknowledgement regression: normal
focus changes supply two configure serials; geometry cannot resize the shell
before acknowledgement or after only the older reply. A commit of unchanged
geometry after the latest reply must adopt it. `WindowManagement` moved unchanged
to dedicated production files shared by Prism and a new in-process suite covering
null/wrong objects through direct and invokable calls. No production hook or
copied implementation was added. The initial 2,384-MiB admission refusal remains
in `build/prism-boundary-2026-11-12/session-01/`. The measured-memory continuation
fixed the test target's directory-scoped Qt imports; Release configuration and
generation then succeeded, and the extracted production source compiled. The
build remains incomplete and the new tests and sanitizers are unrun. Clean WSL
page-cache reclamation admitted compilation, which stopped at 2,956 MiB. A fresh
incremental run with a stricter temporary 256-MiB job-memory pressure threshold
stopped at 3,060 MiB against the unchanged 3,072-MiB floor. Only owned scopes were
stopped. Subsequent idle Search/Widgets cache trimming produced a modest observed
headroom increase, without a further compile attempt. ChatGPT, the voice call and
active work stayed untouched; no service/security/startup changes or app closures
occurred. Evidence: `build/prism-boundary-2026-11-12/session-02/`.

The next continuation reverified all 16 saved source hashes, 78 evidence files
and both incremental caches. After one measured clean guest-cache reclamation,
30 seconds of resident host headroom passed admission. Release resumed at about
3,519 MiB, then stopped at 3,008 MiB against the same 3,072-MiB floor. No new
compilation unit or test completed; no unchanged retry followed. Current evidence:
`build/prism-boundary-2026-11-12/session-03/`. Source and active work remain intact.

The later explicitly authorised resource review preserved all 23 incoming file
fingerprints and 57 sealed research evidence files. No service/application was
stopped: the reviewed Google Play Games service's normal stop request could not
open the service from this non-elevated session. A single clean guest-cache
reclamation passed an observer-resident check, but the actual validation scope
was stopped before admission at 2,869 MiB, 203 MiB below the unchanged floor.
No build/test ran and no further validation attempt followed. Current receipt:
`build/prism-resource-2026-11-12/session-01/handoff.json`. Additional sustained
worker-resident headroom or a qualified builder remains the dependency.

An independently authorised research implementation improves the existing
benchmark gate's evidence checks, using portable tests and preserved reports;
it does not change Prism's runtime sources or advance a blocked milestone.
The [performance assessment](PERFORMANCE.md#implementation-assessment-against-the-current-tree-2026-11-12)
distinguishes that implemented improvement, existing mechanisms and deferred
runtime/hardware experiments.

On 13 November all 51 benchmark-validator and Linux launcher/bundle fixture tests
pass on the Pi, with zero failures/errors/skips. This closes the 25 cases skipped
by the earlier Windows run. The fixtures use inert programs and hardware metadata;
they establish tooling behavior, not compositor timing or physical-device results.
Receipts: `build/development-2026-11-13/session-01/native-gate-20261113/`.

Continue in the owner's order: run the full core-enabled OS and appropriate
product checks, then prove runner isolation before Qt CI resumes. Native focused
Prism validation above completes that earlier prerequisite. Recheck workers,
the original OS lock and resident headroom before each job. The owner authorised
committing and pushing the reviewed increment on 13 November. The existing Lumen tokens, accessibility,
golden-image comparisons and same-machine performance gates continue to govern
visual changes; this validation work does not claim a new visual or speed gain.

- [x] QQ, ARQADE's app and games, and QOR Media are developed here from 29 October
  ([QOR-0015](decisions/QOR-0015-the-products-are-developed-here.md)); demiurge-chain freezes its copies (its ADR-126),
  which its Windows launcher keeps shipping. ARQADE's rules and recorded runs stay shared with ARQADE's server there
  ([UPSTREAM.md](UPSTREAM.md#changing-a-games-rules))
- [ ] QQ, ARQADE and QOR Media build and pass their suites on Linux. Their Windows-only parts get platform seams:
  - XInput becomes QOR.Input (done 30 October: QQ's `Input` reads QOR.Input's backends);
  - the DXGI GPU choice is handled per platform;
  - the agent's key is kept in the Secret Service on Linux ([QOR-0011](decisions/QOR-0011-the-creators-provider-key-in-the-secret-service.md));
  - on Wayland Prism places the windows the Windows launcher docks ([QOR-0010](decisions/QOR-0010-on-wayland-prism-places-the-products-windows.md)).

  **Begun 28 October:** all three build on Linux with `tools/dev/products.sh`, on a display nobody sees, and the Linux
  kit has Qt Quick 3D Physics and Qt PDF. Under WSL, runs on the GPU hung the workstation three times on 28 October,
  so since 29 October the script fences them from it and they draw in software. Fenced, in software, on 1 November, every
  suite runs to its end:
  - QOR Media passes all 9 suites at `6dd9e43`; docking is skipped on Wayland, and the video case skips, needing a Windows sample file;
  - ARQADE passes all 4 at `ff6946f` on the first attempt: 112 cases, none skipped, `games` 67 of 67 in 52 minutes. The stall at
    Orbital's ten locks was Qt's threaded render loop starving the event loop on Wayland at these frame rates; in software ARQADE
    draws on the basic loop;
  - QQ passes 9 of 10 at `ff6946f`: 353 cases passed, 1 failed, 1 skipped, in 22 minutes. The one failure is the QQ Player's floor of
    20 frames a second, reading 6.2 to 6.3 in software over four runs, with the Player still drawing to the end of each. Whether Linux
    CI stays red on it or lets it stand is the owner's decision (`--accept-software-floor`). The live-provider case skips, needing
    `QQ_LIVE_ENDPOINT=1`.

  In software some checks are eased, and the owner has not been asked to accept that: [CI.md](CI.md#what-linux-runs) lists what
  software alone no longer shows. A pass in software says nothing of a real GPU, real sound or real controllers. Linux CI can run them
  ([CI.md](CI.md#what-linux-runs)), but `QOR_RUN_LINUX_PRODUCTS` is not set, so it does not yet.
- [x] Each runs as a Wayland client of Prism, and Prism places it (the launcher's `place` request becomes a Prism layout request)

  **ARQADE and QOR Media, 29 October:** Prism hosts them.
  - `ProductHost` starts each as a Wayland client of Prism. It speaks the launcher's side of the host protocol, and
    places and names their windows by its own plan and records ([ARCHITECTURE.md](ARCHITECTURE.md#hosted-by-prism)).
  - `tst_producthost` tests the protocol on Windows and Linux. `prism_products` tests the hosting with stand-ins,
    hidden, in Linux CI.
  - The real ARQADE and QOR Media ran in a hidden, GPU-fenced Prism: their windows landed at the planned frames, and
    closing them ended them.

  **QQ Studio and the QQ Player, 30 October:** Prism starts them with no channel (QQ Studio's publishing waits for
  the vault service, Phase 2), and every client's window, theirs included, opens at the size it asks for, fitted to
  the work area, where Prism used to open it at 960 × 640. QQ Studio is in Beam; the QQ Player plays the scene the
  shell gives it (`Desktop.playQQGame()`), one game at a time. The real QQ Studio and QQ Player ran in a hidden,
  GPU-fenced Prism: the Studio at its own 1600 × 960, the Player playing Orb Run at its own 1280 × 720, and closing
  each ended it.
- [ ] The controller service drives QQ through `Input`'s gamepad seam; ARQADE and QOR Media gain controller focus navigation

  **QQ, 30 October:** QOR.Input's backends are `qor_gamepads`, which QQ's runtime links, and `Input` takes every
  controller they report as its gamepad, on Windows (DualShock 4 and DualSense too) and Linux, heard only by the
  application in front ([CONTROLLERS.md](CONTROLLERS.md#games)). ARQADE's games, on QQ's runtime, get it too. In
  Prism the controller interface steps aside while the QQ Player has the focus, and Guide brings Overview. `tst_play`
  plays a virtual controller through QOR.Input; `prism_products` checks the shell's side.

  **ARQADE and QOR Media, 30 October** ([CONTROLLERS.md](CONTROLLERS.md#games)):
  - **ARQADE takes the controller in Prism**, as the QQ Player does. Its games read it themselves, and the shell
    answering as well turned each d-pad press into a second, arrow-key press, and East into Esc, which put the game
    away. Its home answers the d-pad and the arrows with the focus ring on the nearest card, button or tab, the page
    following; A or Return presses as it comes up; and a controller's Back puts a game away, the focus back on its card.
  - **QOR Media keeps the controller interface**, which gives it the d-pad as the arrows, A as Return and East as Esc.
    Return takes the keyboard from what is open to the controls, where the arrows go from one to the next as Tab does,
    and Esc comes back.
  - `tst_games`, `tst_playback` and `tst_reader` drive them by the keys and a virtual controller; `prism_products`
    checks that ARQADE in front takes the controller and QOR Media does not.

  **Left:** Orb Run with a real DualSense on a Linux machine, since WSL has no controllers.
- [x] Lumen's tokens theme every app: Demiurge's one look (`docs/design/tokens.json`) mapped onto Lumen roles, with one source

  **30 October** ([QOR-0016](decisions/QOR-0016-the-products-wear-lumen.md)):
  - The mapping is `lumen.tokens.json`'s `products.colors`: each of the look's 21 colours, a Lumen role. Its inks are
    tested to read at 4.5:1 on its grounds at every accent and contrast, the faintest at 4.0:1 on the raised one, as
    Lumen's own tertiary text.
  - Prism resolves the roles dark, with the person's accent, tint and contrast. It sends them to ARQADE and QOR Media as
    they connect and whenever they change (`theme`), and passes them to every product as it starts
    (`QOR_THEME_COLORS`), which QQ's `Look` takes.
  - **Left**, should either be wanted: Lumen's faces and type scale for the products, and its light appearance.
- [x] A controller-first **Home** and **Library** in the shell: QQ games, ARQADE, media

  **30 October** ([SHELL.md](design/SHELL.md#home-and-the-library-homeqml-homeshelfqml-homecardqml-libraryh)):
  - **Home** (Guide, or Beam) shows what was played last, every game and the newest media, a row at a time. The
    **Library** shows everything, chosen by what it is or who made it.
  - The d-pad goes card to card, South plays or opens, LB and RB go between the two, and the hints show the
    controller's own glyphs.
  - **The Library** (`library.h`) finds, until the core's index (Phase 2):
    - the QQ games QOR OS carries (`QOR_GAMES`, else `share/qor-os/games`);
    - the person's own QQ projects in Documents;
    - ARQADE's games (nine since Bellwick);
    - video, music and pictures.
  - It remembers what was played, and where QOR Media last stopped, which Prism now takes from QOR Media's reports.
    A game plays in the QQ Player or in ARQADE (`play`), and a film resumes there.
  - `tst_library`, `tst_producthost` and `prism_products` test it, the last by a virtual controller.
  - **Left** for later phases: friends online and what's new in ARQADE (Phase 2, with Social and QOR ID), Steam's
    games, and the core's index.
- [ ] From QTOS's Phase 1: evaluate Qt Interface Framework for service code generation and simulation (a QOR record)
- [ ] From QTOS's Phase 1: a Qt Design Studio project for Lumen components

## Alongside the phases · ARQADE's new games

Seven original games built with QQ, each rewarded from the start: its rules in one module that ARQADE's server
replays, shared with demiurge-chain ([QOR-0017](decisions/QOR-0017-arqades-new-games.md)). Each lands whole (its
package, ARQADE's card, the Library, `tst_games`, a fixture and a Node test) before the next begins. Working titles
until the owner names them, as [QOR-0019](decisions/QOR-0019-ever-rift-and-darkveil.md) did the first two; none is
published before.

- [x] Ever Rift, the dungeon hack-and-slash

  **30 October** ([games/README.md](../products/arqade/games/README.md#ever-rift)): three floors carved from the
  seed, a sword, whirl, dash and potions, five kinds of enemy and the warden, gold, relics and levels; its rules in
  ARQADE's replay-safe arithmetic. ARQADE's fifth card and Prism's Library carry it. Its Node test (14 tests) runs in
  the lint workflow; `tst_games` plays it through its first floor and to a fall on the second, by the keys, the pad and
  the screen's buttons, and its recorded run replays to the same outcome in QQ and in Node.
- [x] Darkveil, the 3D action RPG, single-player, its rules built so a server can run them for many players

  **31 October** ([games/README.md](../products/arqade/games/README.md#darkveil)): one hand-laid valley, a village
  with four people and their errands, three wardens over three beacons, a gate and the Hollow King behind it; a blade in
  a chain of three, a roll, a bolt, a nova and potions; camps of five kinds of enemy, levels, gold and what it buys; rigged
  models with clips, in QQ's props. ARQADE's sixth card and Prism's Library carry it. Its Node test (19 tests) runs in the
  lint workflow, and steps two players in one valley; `tst_games` plays it to its first beacon and to a third fall, and its
  recorded run replays to the same outcome in QQ and in Node.
- [x] Bellwick (a working title), the platformer

  **31 October** ([games/README.md](../products/arqade/games/README.md#bellwick)): eight hand-laid levels in four regions, run,
  jump, stomp, slam and a hand bell that stuns and wakes hush blocks; blocks, two powers, six kinds of enemy, platforms, secret
  rooms, and Old Muffle twice. ARQADE's seventh card and Prism's Library carry it. Its Node test (29 tests) runs in the lint
  workflow and plays a run that a solver found through the whole game by the rules themselves, so every level is shown to be
  finishable; `tst_games` plays it through its first level and to its last life, and its recorded run replays to the same
  outcome in QQ and in Node.
- [ ] A racer, a pixel-art game and an arcade game. These wait behind the second and third steps of the order of work.
- [ ] The online world grown from the RPG, its server in demiurge-chain, after Phase 2's sign-in (its own record)
- [ ] Each named game's rules, Node test and fixture in demiurge-chain, by a pull request there

  Ever Rift's and Darkveil's, with its ADR-129: demiurge-chain
  [#117](https://github.com/QOR-MATRIX/demiurge-chain/pull/117), merged by the owner on 1 November (`74a1014`). Bellwick's
  wait for its name.

## Phase 2 · Identity and keys

*About 3–6 weeks.* Exit: sign in once, chat in World, and publish Orb Run on the devnet from QQ
Studio running inside Prism.

- [ ] The vault as a system service built from the QOR core, outside Prism and every app; Prism draws its approvals in the trusted layer
- [ ] The core's bridge grows (upstream, with tests) from four commands to what the shell needs, with events
- [ ] One QOR ID sign-in at the shell, with offline login
- [ ] Social in the shell: chat, friends and World
- [ ] The Library's index as a system service (the role QTOS planned for its indexing daemon, now `qor-indexd`)
- [ ] Publishing from QQ Studio through the vault service, to the devnet, in test CGT only ([QOR-0006](decisions/QOR-0006-qor-os-1-0-moves-no-money.md))

## Phase 3 · Boots and plays PC games

*About 2–4 months.* Exit: the reference laptop boots QOR OS with its panel at 360 Hz and the
television at 60 Hz, and plays a Steam game and Orb Run from the
controller-first Home.

- [ ] The image on Fedora Atomic with bootc: NVIDIA's open kernel modules for every RTX card, Mesa, the QOR OS layer ([QOR-0004](decisions/QOR-0004-the-base-image-is-fedora-atomic.md), [image/](../image/README.md))
- [ ] Secure Boot with QOR OS's own key; image updates with rollback (`bootc upgrade`, a forced rollback tested)
- [ ] **Prism's own KMS backend** ([QOR-0005](decisions/QOR-0005-every-display-at-its-own-refresh.md)):
  - atomic modesetting and scheduling per output;
  - several GPUs: rendering on one and scanning out on another;
  - each display at its own refresh rate from 60 to 360 Hz;
  - VRR, hotplug and direct scan-out.

  This backend also fixes both outputs appearing on one display.
- [ ] The display acceptance test on the reference laptop and on RTX and Intel desktops: under 1% missed frames per output at its own rate, and input to swap within two refresh periods at p95
- [ ] Steam and Proton inside gamescope, as a Wayland client of Prism; a game mode
- [ ] greetd with a Prism greeter, flicker-free from the firmware logo
  The full device-layer guard now queries Prism's compiled capabilities instead of
  accepting its filename. Current device builds report authentication unavailable
  and are refused; the real sign-in backend and device-session proof remain open.
- [ ] Touchpad gestures from libinput for the KMS backend
- [ ] Real daemons for every service contract (`qor-netd`, `-btd`, `-powerd`, `-audiod`, `-notifyd`, `-settingsd`) and an xdg-desktop-portal backend with prompts in the trusted layer
- [ ] Games and apps sandboxed (bubblewrap, seccomp, Landlock): the operating-system half of Demiurge's ADR-087

## Phase 4 · The ecosystem

*Ongoing.* Exit: an outside developer ships a QQ game to QOR OS using only the public
SDK.

- [x] Record QORSYNC.DEV and approved service subdomains as the public domain
  policy ([QOR-0026](decisions/QOR-0026-qorsync-is-the-public-domain.md)); remove
  retired-host references from checked QOR OS text, enforce the check in hooks/CI,
  and verify GitHub's homepage field. The initial 30 focused domain/GitHub tests
  pass locally; the expanded checks pass in hosted Lint at `0cdae3d`.
- [ ] Complete the corresponding upstream domain work and live verification.
  Source maintenance is in [upstream PR #118](https://github.com/QOR-MATRIX/demiurge-chain/pull/118): Launcher guidance,
  disabled frozen destinations, historical redactions and inert obsolete deployment
  helpers. Vercel confirms apex/`www` assignments and Cloudflare nameservers, but
  the connected account cannot access the assigned project. Social/ARQADE service
  hosts, mail, installed settings and released clients need separate acceptance.
  Automated Vercel preview checks pass; production was not promoted. Upstream
  dependency audits fail, so the PR remains a draft with its gates intact.
  Read-only live checks at 22:33-22:34 UTC confirm the homepage, QOR ID health and
  database/cache readiness, and the RPC's runtime 11 with advancing finalized
  headers. The returned homepage HTML still lacks the exact QOR OS repository
  link; project management access and deployment/client acceptance remain open.


The 13 November owner direction makes developer access across every system an
explicit workstream alongside the native validation gates. The main repository
homepage links the canonical [developer entry point](../sdk/README.md), current
progress and this masterplan. The source-verified routes cover apps, assets,
services, QQ's agent tools and the upstream QOR Launcher/DRC-369 boundary.

- [x] Offline machine-readable discovery of developer entry points, prerequisites
  and action boundaries, with JSON output for agents and hostile-input tests.
  `tools/dev/developer.py` provides `list`, `show` and `check` for 14 reviewed
  integration areas using Python only. It validates local path existence and
  pinned upstream URL syntax, not installed tools or upstream content. It grants
  no execution rights; the [developer guide](../sdk/README.md#offline-discovery)
  describes the contract. This increment is integrated on main through `0cdae3d`;
  the installable SDK and runtime integrations remain open.
  Hosted Lint at `0cdae3d` passes 714 tooling tests with 124 platform skips and
  all 63 ARQADE rule tests. All 14 discovery tests also pass on Windows. The full
  local tooling run and all-file hooks were then resource-blocked. The later
  Windows run at `ed3526e` completes all 844 tooling results: 746 pass and 98
  platform-specific cases skip, with at least 3,314 MiB available under the
  unchanged guard. All-file hooks passed before PR #5 merged. These checks do not
  replace native product, sanitizer or isolated-runner acceptance.
- [ ] Launcher/DRC-369 and agent integration: first specify and test the required
  read-only core bridge capabilities upstream; then add trusted approval flows for
  authorised mutations. The current bridge has four version/amount commands only.
  Do not use the frozen upstream root SDK or expose keys to an agent.
- [ ] Reproducible app/asset onboarding and end-to-end developer examples, with
  accessible Lumen controls, clear trust prompts and validation for each supported
  host. The installable SDK and device services remain separate deliverables.

- [ ] The public SDK: QOR.Kit, Lumen, the service interfaces and QQ, with a Qt Creator wizard and the simulator
- [ ] The Store and the Market as two stores on shared services ([QOR-0021](decisions/QOR-0021-projects-engines-and-two-stores.md)): the Store's signature decides what installs; whether the chain records who holds a licence is left to the record that chooses the paid model
- [ ] Store stage 2: the developer program and portal, review, privacy-preserving analytics, with no payments ([STORE.md](STORE.md#roadmap))
- [ ] QQ's agent proven: a scene built from a written description with no human edit, recorded with its frames, on
  the creator's own provider key (Demiurge's P3.3, otherwise built before the cut-over)
- [ ] Published QQ games played on ARQADE's website through the WebAssembly QQ Player, each in a sandboxed frame from a
  site of its own (Demiurge's P3.5; the website's part is made in demiurge-chain)
- [ ] QOR Media: Office documents through LibreOffice's engine, its own record first; and pictures beyond Qt's plugins
  (HEIC, AVIF, JPEG XL and camera RAW) (Demiurge's L9.7 and L9.8)
- [ ] The Mesh for distributing games and creations, off until turned on
- [ ] From QTOS's Phases 2 and 4: a browser (Qt WebEngine), HDR, fractional scaling, Quick Look, Stacks and Projects, hero transitions, a Qt-native screen reader, Increase Contrast, translations
- [ ] Paid items, only once [QOR-0006](decisions/QOR-0006-qor-os-1-0-moves-no-money.md)'s conditions are met and a record chooses the model; QQ's side of Demiurge's P3.6 (collectibles and in-game purchases) with them
- [ ] Later than 1.0 ([QOR-0007](decisions/QOR-0007-what-1-0-ships.md)): GNOSIS, Stream, Spatial mode (Qt Quick 3D Xr), on-device intelligence, Continuity

## Alongside the phases · Unreal Engine

[QOR-0020](decisions/QOR-0020-unreal-engine-on-qor-os.md): Epic's own Linux builds, unchanged. The OS meets them and the
ecosystem joins from outside the engine. Each stage ends when its exit is shown working. Nothing of the engine enters this
repository, the image or an assistant's context, and nobody here downloads it until the owner has accepted Epic's licence.

- [ ] **The licence:** the owner, with counsel, reads the current text, chooses the licensee and records the version
  accepted; Epic answers in writing on plugins, stores, QOR's chain and CGT. Epic changed its clause on AI on 29 October, and
  [QOR-0022](decisions/QOR-0022-assistants-and-unreal-engine-after-epics-change.md) proposes what follows; until it is accepted,
  the rule above stands
- [ ] **Stage 1 · A game plays in Prism.** Exit: Epic's Third Person template, packaged for Linux from the Windows editor,
  plays in gamescope in Prism on the owner's machine, with mouse look, a DualSense and sound, at the display's rate.
  - [x] `zwp_relative_pointer_manager_v1` and `zwp_pointer_constraints_v1` in Prism, tested from a hostile client's side
    (`tst_pointercapture`, offscreen; holding the real pointer and unaccelerated motion wait for the KMS backend; Phase 3's
    Steam needs these too)
  - [ ] `zwp_linux_dmabuf_v1` on a real graphics card
  - [ ] gamescope running a Vulkan program in Prism on the RTX 4060: the first test of NVIDIA nested
- [ ] **Stage 2 · A native game is in the Library, in a sandbox.** Exit: a game installed by hand appears from its manifest,
  starts from the controller-first Home, and reaches only what it declared. Tested first with a stand-in, under `linux-asan`.
- [ ] **Stage 3 · The plugin.** Exit: a packaged game takes the person's look, hands the controller over and pauses when
  Prism asks, through the plugin alone. The Unreal-free client is tested in CI, against Prism's side of the host protocol.
- [ ] **Stage 4 · The editor on QOR OS.** Exit: on QOR OS alone, a person downloads the engine from Epic with their own
  account, opens a template, packages it and plays it from the Library. Through xwayland-satellite, in a container of a
  distribution Epic names. Until this passes, the Windows editor with Epic's cross-compile toolchain is the supported way.
- [ ] **Stage 5 · The Store.** Exit: a maker's packaged game installs from a signed catalogue, and a package holding Engine
  Tools or source is refused. Waits for Store stage 2.
- [ ] Rewarded Unreal games: not decided; a later record, if wanted

## Alongside the phases · Project workspaces

[QOR-0021](decisions/QOR-0021-projects-engines-and-two-stores.md), decision 2. Exit: with two small QQ projects, a person switches,
gets the right tools and files back, imports an authorised asset, edits, saves, builds and tests, and reopens after a restart, with
no mix-ups between them, on native Linux.

- [x] QQ Studio: a workspace for each project, one way to switch, unsaved work kept, a list of recent projects. Shown by `tst_workspace`
  in software on Linux, on 1 November 2026, and not yet on native Linux, a GPU or Windows. Unsaved work is kept in a recovery copy and
  offered back after a crash or a launcher's close (`tst_workspace`, the real program killed by SIGKILL), one copy for each Studio, never another project's
- [x] QQ Studio builds a game of a project, and Prism's Library plays that copy: QML in QQ's runtime, not a native program. Shown by
  `tst_gamebuild` and `first_journey` in software under WSL at `72945fe`, after an independent review and its recheck, with Prism starting a stand-in for the Player and the
  real Player playing what it asked for on a display of its own. Not yet on native Linux, a GPU or Windows; the real Player in a Prism
  window needs a real graphics card, and an import records no permission for what it brings in
- [ ] What is pinned to a project: its engine and version, and its asset dependencies, apart from the person's own library
- [ ] Prism: a project as a place on the desktop, with its files, tools, window layout, build targets and test history
- [ ] Budgets, proposed and not yet measured, on the reference laptop: back in a small project within 1 second at p95, no
  frames or wakeups in an idle workspace, and no heavy editor started in the background

## Alongside the phases · Engines

[QOR-0021](decisions/QOR-0021-projects-engines-and-two-stores.md), decisions 3 and 4. Open, Build and Test through small adapters,
each version on its own evidence. Unreal Engine 6 gets a validation stage when its requirements are known.

- [ ] QQ's adapter
- [ ] Unreal's adapter, after the owner's licence step and its stage 1 above: `.uproject` found, the person's own engine opened,
  packaged with Epic's tool, played from the Library. Art comes to QQ through Unreal's own glTF export, the rest rewritten
- [ ] Unity's adapter: each operation stays gated until it has a grant in Unity's terms, its documentation or its written answer,
  or a legal reading says none is needed. A game built with Unity is an ordinary Linux program and needs none of this

**What has actually been run.** Each cell is one of: **passed**, **failed**, **blocked** (with why), or **untested**. A suite is
named as one: it is not a person at real hardware. What a vendor lists is not what QOR OS has tested, and QOR OS being tested is
not a vendor's support. The commit and date go in the row.

| Configuration | Editor launch | Menus, focus | Pointer capture | Graphics | Audio | Controller | Project save | Packaged build | Game launch |
|---------------|---------------|--------------|-----------------|----------|-------|------------|--------------|----------------|-------------|
| QQ, Windows 11, in CI, at `3bf213a` | passed, suites | passed, suites | untested | passed, suites | suites, deaf to the machine | virtual only | passed, suites | untested: the build step has not run on Windows | project in the Player: passed, suites; packaged: untested |
| QQ, Ubuntu in WSL2, software, hidden display, at `72945fe` | passed, suites | passed, suites | untested | software only | deaf to the machine | virtual only | passed, suites | passed, suites (`tst_gamebuild`, `first_journey`) | what was built, in the real Player: passed, suites; from the Library: Prism starts a stand-in there, and the real Player plays what it asked for |
| QQ, native ARM64 Linux (Q Pi), software, 13 November 2026, product source unchanged from `ed3526e` | passed, Studio suite (29 Qt Test results) | untested: full acceptance | untested | software only; World suite passes | untested: physical devices hidden | untested: physical devices hidden | passed, Studio save/reopen cases; workspace suite pending | passed, complete first-journey check | real Player playback passes in the journey; Prism records a stand-in launch. Broad play suite: 54 pass, four fail; aggregate run times out during Player |
| Unreal 5.8 | blocked: licence, not installed | blocked | blocked: Prism's interfaces | blocked | blocked | blocked | blocked | blocked | blocked |
| Unity 6.3 | blocked: operation gated | blocked | blocked | blocked | blocked | blocked | blocked | blocked | blocked |

Vendors list, for Linux: Unreal 5.8, Rocky or RHEL 8 and newer, developing on Ubuntu 22.04 or Rocky 8, NVIDIA 570+ or RADV 24.2.8+. Unity
6.3, Ubuntu 22.04 and 24.04 on GNOME, NVIDIA and AMD, with Wayland experimental and no container. QOR OS is neither vendor's list. That
is a fact to report, not proof it cannot work.

## Alongside the phases · Two stores

[QOR-0021](decisions/QOR-0021-projects-engines-and-two-stores.md), decisions 5 to 8. No money ([QOR-0006](decisions/QOR-0006-qor-os-1-0-moves-no-money.md)).
Exit of the first stage: on a demonstration catalogue, a redistributable pack is found, installed, used, updated and reopened in a
real QQ project; one finished game appears for players, installs and launches; player-only navigation, search, recommendations,
featured collections and counts hold no creator-only listing, and a creator-only item is refused as well as hidden; private source and
unpublished builds stay out of reach.

- [x] Catalogue format 2, typed per item: kind, store placement and category, stable package identity, entitlement, platform, engine
  and base-game dependencies, licence, provenance, previews; sequence and expiry; `price` refused, and format 1's unchecked `price`
  closed. Unclassified or contradictory listings stay drafts. Hostile cases under `linux-asan`: archive bombs, duplicate and device
  names, executable files in a pack, climbing glTF paths, an older signed copy swapped in, dependency cycles, markup in a licence.
  Installs and removals all or nothing, across a crash. `tst_store_format2` 604 cases under `linux-asan` and 597 under MSVC at
  `811d192`, after an independent review and two rechecks, each check proven by a planted fault
- [x] Explicit views in the query layer, with no fallback between stores; a utility in both stores is one package, installed once;
  turning creator features off keeps what is installed
- [x] A separate demonstration catalogue with a throwaway key, never in a release; today's fixtures not re-signed
- [x] The Player Store's pages: find, licence and provenance as plain text, install, update and remove, with refusals said
  plainly, in a demonstration build only (`1892e30`, after an independent review)
- [x] Play, and Store games in the Library, checked rather than scanned: listed from the engine's record, checked whole as each is
  played, started on the scene its `game.json` names; in a demonstration build (`214ca91`). The real QQ Player plays a Store install
  in QQ's own `tst_storegame` (`2fbfc2a`), walled, from the scene its `game.json` names
- [x] The creator-features switch in Settings, off by default and enforced by the engine, and the Creator Store's pages; turning
  it off closes them and keeps what is installed
- [x] QQ Studio's asset library: browse, preview checked bytes, copy into a trusted project with `assets.lock.json`, and update
  without writing over a changed copy; the built game says what it uses. Recovery checks the project folder and each placed file,
  and every lock has its own change tag. Use and Update run off the window's thread; Save, Build and changing project wait.
  Delayed replacement/update answers recheck trust before writing. The replacement gap reproduced in the real window, then
  all 91 asset-library cases pass in Release and under sanitizers after the guard, with no failures/skips; fresh requests still work.
  The Store's listing provenance reaches the lock whole within its existing 8 MiB cap (`8b77c01`). On 4 November, Qt inputs at
  `6046e08` passed Release `tst_assetlibrary` 89 cases, `tst_gamebuild` 195 and `tst_studio` 25, none failed or skipped, including
  the opt-in largest-pack measurement and the real StoreEngine's demonstration pack. The matching current-source sanitizer group
  also passes all 89/195/25 cases, none failed or skipped, with leak checking off. The later trust guard has the 91-case asset checks
  above; those game-build/Studio runs precede it. Hardening at that checkpoint passes 16 Release/8 sanitizer programs.
  Earlier sanitizer runs passed 89 asset-library cases on 4 November and 195 game-build cases on 3 November;
  the unchanged recovery code's earlier build is preserved at `850a639`. These use WSL software rendering. Listing and planning
  still run on the window's thread: that Release largest-pack run measured first listing 269 ms, repeated listing 39 ms,
  new-version listing 345 ms and planning 396 ms. These are individual observations without a latency budget or an improvement claim.
  Native Linux, a GPU and Windows still need
  their own evidence. Runtime ids and the Qt Quick 3D C++ dependency declaration reduced QQ's console lint messages to 429,
  none added. Binding all five screen drawings to their owning context and typing image readiness then left 347, none added.
  The project-file panel's bound delegates, actual Studio window type and preferred marker sizes then left 340 console
  messages, none added. Its structured findings fall from 14 to 5 with none added; full QQ JSON retains 375 warning records,
  including repeats collapsed by the console. Seven selected Studio/workspace Qt cases pass in each of Release and
  ASan/UBSan, none failed/skipped, covering file click/filter, saved-state summary/Forget and two-project isolation.
  A real Studio probe then reproduced automatic parsing of supplied logic problem text. Its label is now plain text;
  actual window typing and bound editor delegates leave 338 console messages/363 structured warning records, none added.
  The editor's 12 structured findings are gone. Three focused and 18 related Qt cases pass in each build without failures/skips,
  including exact save filenames, unsaved prompts, restore/discard, project switching and build interlocks/packaged logic.
  Typing the statistics overlay as `World`/`RenderStats` then removes five more findings, none added: that checkpoint has
  333 console messages/358 structured warning records. Its existing F3 statistics/play/collider-toggle check passes in
  Release and under sanitizers (three Qt cases each), none failed/skipped. All three native lint commands still fail.
  The inspector's source filename also reproduced automatic markup detection in the real window. Its labels now use plain
  text; bound components, typed loaders and explicit axis roles remove 76 more findings with none added, leaving 257 console
  messages/282 structured warning records. Seven selected Qt cases per build pass without failures/skips: literal filenames,
  actual loaded field edits and entity switches, undo, groups/prefabs and 2D editing. The checks caught and fixed a transient
  text-row assignment warning. Fresh product hardening passes 16 Release/9 sanitizer programs. Native lint still fails.
  The agent panel's bound step rows then remove ten findings with none added: 247 console messages/272 structured warnings.
  Six selected Qt cases per Release/sanitizer build pass, none failed/skipped, including inert row replacement/style/plain text,
  literal status/key problems and the existing key/description flow in a private test keyring. Native lint still fails.
  At integrated product revision `b2b084c`, the complete Studio suite at that revision passes in each build: all 26 functions plus
  setup/cleanup, 28 Qt cases with no failures/skips or function filter. Fresh native records/source matches are verified;
  incremental build logs have no warnings, and hardening passes 16 Release/9 sanitizer programs. These remain hidden WSL
  software checks with leak checking off; full OS/core and native hardware are unverified.
  A later real-popup test reproduced AutoText for creator group names. An explicit public MenuItem/plain-label delegate
  fixes it; actual delegate activation still reparents the entity and updates the selected index. Seven selected Qt cases per
  build pass without failures/skips, including group/prefab, undo, duplication and Studio-look checks. Hardening passes 16/9;
  lint remains 247 console messages/272 structured warnings, none added. The full 28-case runs above precede this popup fix.
  Ground's bound slab context and explicit texture owner then remove four findings, none added: 243 console messages/268
  structured warnings. Existing physics checks pass five Qt cases per build; its checker/extent pixel check passes three per
  build, all without failures/skips. Both Studio/Player variants are rebuilt and fingerprinted. Hardening passes 16 Release/11
  sanitizer programs, including the newly built play/world suites. The full 28-case runs at `b2b084c` precede this batch too.
  Current-program journeys at `6f14fe2` pass three existing functions in each build (five Qt cases, zero failures/skips):
  Studio edit/save/build and project switch/reopen; actual Player built-copy independence; actual headless Studio trust and
  command-line protocol. Fresh records/product-source matches and hardening 16/11 pass. The CLI reports `compiled:false`;
  these remain copy/rendering checks under private WSL, with native hardware/audio unverified.
  A Player report-write failure then reproduced exit 0 after measurement generation. Checked open/full-write/flush/close
  now returns exit 1 on failure, with the exact-file kernel permissions unchanged. Failed output preserves its directory
  sentinel; valid output replaces stale JSON; a later copy-playback run uses a fresh report filename. Four Player and three
  build-copy Qt cases per Release/sanitizer build pass, zero failed/skipped; fresh records/source hashes and unchanged
  sandbox sources are verified. Hardening passes 16 Release/12 sanitizer programs. No native audio/hardware claim follows.
  Music’s bound private voice factories remove four more lint findings, none added: 239 console messages/264 structured
  warnings. Ambient and WebVoice factory crossfades pass four Qt cases per Release/sanitizer build, zero failures/skips,
  with unchanged gain/fade/release/stop assertions. Hardening passes 16/12; physical sound/browser playback remains unproved.
  Sprite’s bound collider/body factories remove 19 findings, none added: 220 console messages/245 structured warnings.
  Seven Qt cases per Release/sanitizer build pass, zero failed/skipped: crisp/animated pixels and dynamic sprites landing
  on static/kinematic sprite floors with contact reports and plane locks. Both programs are relinked; hardening passes 16/12.
  Trigger’s bound volume/collider factories remove nine findings, none added: 211 console messages/236 structured warnings.
  Six Qt cases per Release/sanitizer build pass, zero failed/skipped: box entry/exit, spherical pickup, moving groups and
  headless 2D detection. Both programs are relinked; hardening passes 16/12. Shape’s separate experiment exposed two
  parent-view type warnings and was left out; resolving its shared-parent contract remains before that binding change.
  Sound/one-shot factories then bind to their owner and use actual View3D/Camera types through the viewport’s camera.
  They remove 26 findings, none added: 185 console messages/210 structured warnings. Eight Qt cases per Release/sanitizer
  build pass, zero failed/skipped: placed/one-shot sounds, both music paths, projection switching and silent replay. Both
  programs are relinked; hardening passes 16/12. Physical sound/browser playback remains unproved.
  At integrated runtime `89877cf`, complete unfiltered play then passes in both builds: 56 Qt cases, zero failed/skipped,
  all 51 functions with 54 expanded rows plus setup/cleanup. Fresh native records, exact coverage and unchanged source/tree
  hashes are verified. Release takes 1,878.98 seconds; ASan/UBSan 1,923.71 seconds, with leaks off. Post-run hardening passes
  16 Release/12 sanitizer programs. All remaining 210 structured warnings occur in the first retained 381-warning snapshot:
  171 removed, none added against that snapshot. The native lint gate still fails, and this is not native hardware/OS proof.
  The saved-game store’s existing C++ instance API is then declared as an uncreatable QML type; the project panel uses it.
  Storage algorithms, formats, paths, limits and permissions are unchanged. All 49 scene cases and three actual Studio
  save/Forget cases pass per Release/sanitizer build, zero failed/skipped, including a memory-only typed/creation-refusal
  regression. Fresh records and hardening 16/12 are verified; both programs are relinked. Five lint findings are removed,
  none added: 180 console messages/205 structured warnings remain. The 56-case play runs above precede this metadata batch.
  Player2D’s bound private carrier then removes six findings, none added: 174 console messages/199 structured warnings.
  Five Qt cases per Release/sanitizer build pass, zero failed/skipped, for tiled collision/rendering, top-down movement
  with its carried look and headless 2D trigger replay. Both programs are relinked; fresh hardening passes 16/12.
  Bound private physics factories in Scene/Player/TileMap remove six findings, none added: 168 console messages/193
  structured warnings. Six gameplay and three actual Studio Qt cases per Release/sanitizer build pass, zero failed/skipped,
  for gravity, pause, player pickup, tiled collision and collider-debug toggling. Fresh records/exact functions and hardening
  16/12 pass; both programs are relinked. Physics values, movement and signal handling are unchanged.
  World’s explicit light-probe texture owners remove two findings, none added: 166 console messages/191 structured warnings.
  Four Qt pixel cases per Release/sanitizer build pass, zero failed/skipped, for HDR lighting/rotation/bloom and pictured
  versus plain procedural lighting. Fresh records/source hashes and hardening 16/12 pass; both programs are relinked.
  A later startup fix rejects non-3D scene roots and discards incomplete 3D roots with a reported problem and a valid retry.
  The actual Player retains one terminal result: startup failure 3, failed report write 1; ordinary play stays open after
  its console report and capture waits for its image. All 51 Scene and 11 selected Player Qt cases pass per Release and
  ASan/UBSan build, zero failures/skips, including capture and record/replay. Fresh native records/source hashes and
  hardening 16/12 are verified. The final local bound is two CPUs and 1.75 GiB; earlier tighter-cap startup failures and
  observed paging remain recorded, without a conclusive cause claim. Lint stays at 166 console/191 structured warnings,
  none added, with its native failure preserved. Full OS/hardware readiness remains unproved.
  The Player’s startup-error label is then made plain text, preserving its binding, font, wrapping and color. A real-QML
  regression reproduces automatic parsing, then verifies missing filenames, inert supplied markup and later changes.
  All 52 Scene and five actual Player startup Qt cases pass per Release/sanitizer build, zero failures/skips. Fresh records,
  source hashes and hardening 16/12 pass; production C++, Game and SceneIO are unchanged from `ad4aa3f`. Lint remains
  166 console/191 structured warnings with none added; the zero-warning gate still fails.
  Integrated product code at `39b0768` then passes fresh unfiltered Release and ASan/UBSan refreshes: 91 asset-library,
  195 game-build and three Store-to-Player Qt cases per build, zero failures/skips, all 44/48/1 ordinary functions.
  Opt-in largest-pack measurements and the real StoreEngine demonstration-pack path run; native records, 18 source
  hashes and unchanged product inputs are verified. Fresh hardening passes 16 Release/13 sanitizer programs. The installer
  helper is from its separately recorded earlier build; it proves neither current compositor nor hardware readiness.
  Release first-list/planning observations are 246/386 ms on the window thread, without a latency budget or improvement
  claim. All completed suites fall below the documented 5,400-second ceiling; the picked runner's missing aggregate-timeout
  argument is disclosed and corrected in the future local display clone. QQ lint stays 166 console/191 structured warnings,
  none added, and its native failures/overall exit 1 remain. No release gate is cleared by these software checks.
  The complete first journey at `3421d35` then passes once with Release QQ and once with ASan/UBSan QQ, zero
  failures/skips, in 113.07/113.31 seconds under its unchanged 300-second deadline. Each checks two Studio builds,
  three real Player launches, current Prism Library discovery/request and playback after the temporary project is gone.
  Fresh native records, source hashes and nine program identities are verified. Prism uses a recording stand-in and the
  real Player runs separately; Root Prism itself is not sanitizer-instrumented. The selected strict Root build uses
  the core-enabled profile, but Prism itself does not link the core. Hardening passes 25 programs; the wrapper exits 1
  for the same 12 inherited Wayland metadata
  warnings, none added/removed. This is functional software evidence, with the warning/full-OS/hardware gates still unmet.
  The 5 November Shape batch then qualifies private factories through their actual owner, preserving public interfaces
  and 2D axis locks. Full Scene (52 cases) and six physics functions (10 cases) pass per Release/sanitizer build, zero
  failures/skips; fresh records/source hashes and hardening 16/13 are verified. Group waits use existing software clocks
  after the original raising wait failed with both old and candidate Shape sources. GPU waits/tolerances and the Player's
  frame-rate floor remain; earlier failures are retained. Lint drops to 153 console/178 structured warnings, 13 findings
  removed, none added; native failures/overall exit 1 remain. At `d111bdc`, the complete journey is refreshed after this
  batch: one CTest per product build passes, zero failures/skips, in 112.977/112.875 seconds, keeping its 300-second limit.
  Two Studio builds, three real Player launches, Prism Library discovery/request and playback after project removal run;
  fresh records, 22 source fingerprints and nine program identities are verified. At `f99e007`, this journey is refreshed
  after Inspector: one CTest per product build passes in 126.656/114.105 seconds, zero failures/skips, with the same
  300-second limit. Current app rebuilds, 26 sources, nine program identities and sanitizer flags/libraries are verified;
  lint remains failed at 149 console/174 structured warnings, with none added or removed against Inspector. Prism remains a non-instrumented
  simulator using a recording stand-in, with real Player playback separate. This clears no warning/hardware/isolation gate.
  Earlier, 48 scene cases passed in Release and under sanitizers, including malformed-image readiness; six selected screen-play
  functions pass (8 Qt cases including setup/cleanup). The earlier broader play run and a clean detached baseline each had
  the same 11 failures. Test clock waits and guaranteed fake-controller cleanup subsequently bring all 53 play cases to a pass,
  none skipped; that full run predates the latest panel/bar/image batch. Spatial-audio type metadata findings still reproduce
  in a Qt-only probe. Native CMake lint enforces zero
  warnings through `.qmllint.ini`; all three QQ module commands fail. The zero-warning requirement remains unmet.
- [ ] Publishing in stages: private draft, preview, package checks, a declaration of rights, submission, review, publication
- [ ] Editable projects in the Creator Store as editions linked to a game, with their engine, files, outside dependencies and permitted
  reuse; playable mods, per game, only where that game allows them
- [ ] Paid commerce, a separate goal, in its own record: sandbox checkout, entitlement, download, refund and revocation, then seller
  onboarding, payouts, tax and takedowns. Nothing here is claimed until those run

## Alongside the phases · Optional apps

[QOR-0021](decisions/QOR-0021-projects-engines-and-two-stores.md), decision 11: native Qt, reusing what exists first, offline with local
data, and a backlog rather than fourteen projects. None is started.

| App | Store and category | First useful scope |
|-----|--------------------|--------------------|
| Game Design Document Studio | Creator: Design & Planning | Guided one-page and full templates, project-linked editing, recoverable saves, Markdown and PDF export |
| Calculator | Player: Utilities; Creator: Development Math | Basic calculations and history, plus units, aspect ratios, frame-time conversion and hexadecimal modes |
| Build & Playtest Launcher | Creator: Build & Test | QQ project launch and build actions, exit status and logs; engine adapters after verification |
| Controller & Input Workbench | Player: Controllers; Creator: Input Testing | Device, button and axis diagnostics; remapping follows supported-stack evidence |
| Project Notebook & Playtest Journal | Creator: Planning & Playtesting | Project- and build-linked notes, checklists and attached screenshots |
| Asset Inventory & License Manifest | Creator: Asset Management | Asset sources, acquisition and licence evidence, versions and project usage |
| GPU Frame-Capture Helper | Creator: Debugging & Performance | One verified existing capture tool, with capture metadata kept |
| Mod Packager & Validator | Creator: Mod Development | One supported manifest, dependencies and validation; playable mods get separate Player listings |
| Save-Game Backup Manager | Player: Save Management; Creator: Test Save States | Explicit folders, versioned backups and confirmed restoration |
| Reference Board & Palette Tool | Creator: Art & Visual Design | Local references, notes, swatches and project links |
| Audio Trim & Loop Utility | Creator: Audio Design | Waveform, trim, loop preview and supported export, reusing QOR Media where it can |
| Project Health Checker | Creator: Build Diagnostics | Known project, version, path and dependency checks, with explained results |
| Performance Session Dashboard | Player: Performance; Creator: Performance Testing | Supported metrics only, with hardware and build context, and session timelines |
| Console Capture Viewer | Player: Capture & Accessories; Creator: Capture & Media | Compatible HDMI-to-USB hardware, sound and picture preview, fullscreen |

**The Game Design Document Studio comes first,** once it fits without disturbing the Linux milestone. It covers pitch, audience and
platform, core loop, mechanics and controls, progression, narrative and levels where relevant, art, interface and audio direction, scope and
milestones, and playtest criteria, with editable and reorderable sections, images and captions. It tracks revisions and tells proposed
ideas from what is implemented or verified. It is checked on: two-project switching, restart recovery, interrupted writes, conflicts with
outside edits, offline use, keyboard and scaling, and rendered Markdown and PDF with long text and non-ASCII. Documents survive
updates. It claims no universal certification. A calculator can follow as a small independent piece.

## Alongside the phases · Console capture

Proposed ([QOR-0021](decisions/QOR-0021-projects-engines-and-two-stores.md), decision 9), not in 1.0, and after native-Linux
reliability. A PS4's HDMI goes to one USB capture device that works on Linux, aiming for 1080p60. Exit, on real hardware: device choice,
fullscreen preview, sound, hot-plug, loss of signal, suspend and resume, a protected source named as such, frame drops and resource
use measured, and the laptop's actual preview delay measured. A latency budget is proposed from that measurement, not from a
vendor's claim. No firmware turns an output-only HDMI port into an input, so this needs the capture device.

## Alongside the phases · The security bar

[QOR-0018](decisions/QOR-0018-the-security-and-quality-bar.md), from two independent reviews of the trust boundaries on
30 October 2026. What is left of them, with the known limits in [SECURITY.md](../SECURITY.md#known-limits):

- [x] Exploit mitigations on every program, read back in CI; the suites under sanitizers; CI pinned by content; secrets
  and advisories looked for
- [ ] Resolve the pinned core's current dependency advisories upstream before
  expanding Launcher/DRC-369 bridge capabilities. The 13 November on-demand audit
  at `6a4822d` fails with four vulnerability advisories and 13 warnings; upstream
  `74a1014` retains the affected versions. [SECURITY.md](../SECURITY.md#known-limits)
  records the findings and their scope. Review feature/target reachability and
  compatible updates, run upstream tests and a clean audit, then review the QOR
  pin. Do not suppress findings or copy the core into this repository.
  Repair commit `042941c` is pushed in upstream PR #118: local core/host audits
  now report zero vulnerability advisories, retaining 13/23 warnings. Compatible
  chain dependency patches initially remove nine findings. The certificate and
  compression follow-up at `3d71009` passes all 213 hosted chain tests with
  WebAssembly built and reduces the audit to 38 findings and 26 warnings.
  QOR ID and Social each retain one SQLite advisory; stable SQLx also rejects the
  fixed version. The Launcher test helper now uses Cargo's reported executable
  path. Hosted build/test acceptance and the QOR OS pin review remain open.
  At that repair commit, hosted chain format/lints and all 213 workspace tests
  pass with WebAssembly built; QOR ID, Social and ARQADE build/test jobs pass.
  Launcher validation also passes: 245 core tests pass with 10 ignored, and
  109 host tests pass with five ignored, including all four repaired real-spawn
  cases and the new artifact-discovery regression. The two service audits still
  fail, so the upstream repair remains in draft PR #118.
- [x] The repository private, as its documents always said (1 November; it had been public since 27 October, while CI ran on
  the owner's machine)
- On 13 November, GitHub Actions settings were tightened and read back: six reviewed
  action repositories, full commit pins, read-only default tokens and no review
  approval by jobs. Offline regression checks also caught and removed persisted
  credentials in the two platform checkouts. Contribution guidance now records
  exact commands/outcomes and keeps the carried Apache notices. The prepared
  ruleset requires Lint, history scanning and readiness without an administrator
  bypass; the API still refuses installation on this private repository's plan.
  Both registered runners report offline. These controls do not close the isolation
  or release gates below; [CI.md](CI.md#repository-settings-and-recovery) owns setup and recovery.
  Hosted Lint subsequently passes at `3ad8954`, including 691 tooling passes and
  124 platform-specific skips. Qt jobs remain paused; no isolation gate is closed.
- [ ] CI that cannot reach the owner's keys or credentials ([CI.md](CI.md#what-ci-trusts)). Build's workstation jobs fail closed
  until their runner is isolated (pull request #2, merged 1 November 2026), both runners are stopped, and Release readiness fails
  while a required build or test did not run
  - [ ] The Store's keys and their backup off the workstation (the owner's act)
  - [ ] Linux jobs in a distribution of their own, with no Windows drive, no Windows programs and no credentials. `qor-ci` is
    built and its boundary tried ([CI.md](CI.md#an-isolated-place-for-linux-jobs)), but a distribution shares WSL's kernel, and
    any process in one reaches `C:\` through `AF_VSOCK`, so it is not the replacement
  - [ ] Linux jobs in a VM of their own: `tools/ci/isolated-vm.ps1` is written, reviewed and hardened, and tested against
    stand-ins (138 tests, 125 planted faults; [CI.md](CI.md#a-clean-vm-for-linux-jobs-not-yet-made)). Still to do: the owner runs
    Create and Prove at `c9513c6`; a way to start it without elevating each time, by a decision of its own; then Qt's answer, a
    kit, a runner and a clean disk per job
  - [ ] Windows jobs under an account or a virtual machine that cannot reach the owner's profile
  - [ ] The GitHub Apps with workflow access to every repository limited, so that they cannot reach the runners (the owner's act)
- [x] Prism: the bus, the keyboard, the lock screen, what a client's name and title can do, the Store's widgets
  Fresh local command-line checks at `af8fc43` pass 10 simulator/11 device-option Qt cases in each main profile,
  zero failures/skips, verifying device refusal of script/unlock/screenshot/benchmark-only options. Native records,
  unchanged sources/settings and sanitizer instrumentation are verified. The selected core C interface separately passes
  9 normal-profile cases; the sanitizer profile intentionally has no core. Hardening passes 27/25 main-build programs,
  while the pinned Rust dependency warning still fails its guard/overall wrapper. Full OS/Rust and hardware proof remain unrun.
- [x] QQ and QOR Media: what a package can reach, the Studio's trust in a project, the agent's logic; the QQ Player
  walled in by the kernel on Linux
- [ ] The Store's own catalogue in format 2. Format 2 carries a sequence and an expiry, and its checks are done. The first-party
  catalogue of themes and widgets is still format 1, which has neither (needs signing, with Store stage 2)
- [ ] The Player's wall on the owner's Linux machine with a real graphics card: tested so far in software only
- [ ] Prism's clipboard: a client sets the selection only when it has the keyboard (needs Prism's own data device, or a
  patch to Qt)
- [ ] Report Qt Remote Objects' decoder (it trusts packet contents) to The Qt Company, under the owner's licence

## Release gates

Nothing is published until all of these hold
([QOR-0008](decisions/QOR-0008-before-a-public-release.md)). They are the
owner's to obtain. This list is their status. [release/](release/README.md)
holds, for each one, who must act, the next step, and what is prepared for
the owner's advisers.

- [ ] The Qt Company's written confirmation of the licence to distribute Qt in a QOR OS image and SDK ([release/QT.md](release/QT.md))
- [ ] Trademark clearance for "QOR OS", QQ and ARQADE ([release/TRADEMARKS.md](release/TRADEMARKS.md))
- [ ] Terms of Service, a privacy policy, and a legal opinion on rewards, prizes, identity checks, sanctions and age limits ([release/LEGAL.md](release/LEGAL.md))
- [ ] Code-signing certificates and the Secure Boot key ([release/SIGNING.md](release/SIGNING.md))
- [ ] An external security review of the QOR core and the vault service ([release/SECURITY-REVIEW.md](release/SECURITY-REVIEW.md))
- [ ] QOR ID in a second region with offline login; the key exposed on 5 October 2026 replaced ([release/SIGN-IN.md](release/SIGN-IN.md))
- [ ] A public way to report a QOR OS security problem: this private repository's
  form reaches only its members. Demiurge-chain's public private-reporting form
  is enabled; its scope and routing for private QOR OS reports still need an
  explicit published policy before this release gate closes.
- [ ] Every required build and test run and passed on the commit released, as Build's Release readiness says
  (`tools/ci/release_readiness.py`). That includes the products' suites on a graphics card with nothing eased, on Windows and on
  a Linux machine, since what software eases ([CI.md](CI.md#what-linux-runs)) proves nothing of what it leaves out. Not yet: Build
  is paused, and no job runs the products on a Linux graphics card

## Release deliverable · The image

The owner's 12 November installer request is **one downloadable QOR OS image
containing full QOR OS and Q Pi**, booting supported hardware and selecting a
compatible edition, not two independent images placed on a card. Assess
feasibility only after the preceding Prism, full-validation and runner-isolation
milestones. Profiles must name architecture, firmware and supported hardware;
ARM alone must not select Q Pi. Ambiguity requires a safe manual choice or an
unsupported notice. No combined layout/backend has been adopted or validated.
The owner will supply storage when ready; no physical media is to be written in
this increment. [Image guidance](../image/README.md) holds the requirements.

### Q Pi development edition

**Reopened by the owner on 13 November alongside QOR OS.** Continue from the
native application evidence under the existing qualification gates. The earlier
hold is superseded; image composition, hardware acceptance and publication still
have their own prerequisites.

[QOR-0025](decisions/QOR-0025-q-pi-development-edition.md) scopes Pi 5 ARM64,
8 GB RAM and 128 GB nominal storage. The reviewed preparation and read-only
probe are local source work. Native Prism and QQ builds now have focused software
evidence; no Q Pi image has been built or accepted.

- [x] Review the supplied preparation against the current architecture and
  workers; preserve unrelated projects, existing proof and worktrees; add bounded
  ELF/Qt metadata checks and the [first-boot plan](../image/README.md#q-pi-first-boot-and-bluetooth-bootstrap).
- [x] Document the stock Pi 5 Desktop Trixie bootstrap separately from Q Pi,
  with normal local login, generic remote access disabled, single-card/off-card recovery
  boundaries, exact package-delta review and commercial Qt module/SDK questions.
  A read-only `--bootstrap-only` probe checks native/board/OS/page/RAM metadata
  before Qt exists; passing it does not qualify a builder. On 11 November, the
  separate stock Pi builder was observed over authorised restricted-key SSH:
  Pi 5/ARM64/Trixie/16-KiB metadata and bootstrap RAM checks pass. Initial SSH trust
  remains owner-approved first use, not independent identity verification. The
  earlier Windows media identity is historical. No package installation or Q Pi
  media write ran in that bootstrap review. A later hostname/access failure was
  resolved on 12 November; restricted key-based access was reverified on
  13 November with strict host checking unchanged. Prior observations
  and the 80 native tooling tests below are dated evidence.
  Qt's Ubuntu ARM reference is an unadopted fallback. `tests/pi/` now prepares a
  smaller Core/Test build of the existing Prism parser suites with native gating,
  hardening and release/sanitizer presets. Both native parser configurations
  subsequently passed 23 Qt Test results each, with four hardened programs;
  the older Windows configuration refusal remains separate.
- [x] Prepare the headless Wi-Fi/key-only SSH design against pinned current Imager
  `cloudinit-rpi` sources, preserving both autologin modes off. Add bounded public
  host-identity receipt/retry checks and strict offline Windows SSH argument plans.
  Portable fixtures and existing Windows OpenSSH configuration/fingerprint checks
  pass; they prove no Pi boot, network association or authenticated assistant access.
- [x] Implement a [native Linux job coordinator](../image/README.md#native-linux-heavy-job-coordination)
  with a permanent per-account OS lock, durable incomplete-run marker, bounded
  child supervision and unchanged 3,072-MiB RAM admission. The Windows/WSL
  coordinator is preserved. Windows validation: 28 portable passes, 11 Linux
  syscall/lifecycle skips; actual Windows CLI refuses native use. On 11 November,
  all **80 resource/coordinator cases pass on the real aarch64 Pi with zero
  failures/skips**, including real Linux lock/descendant cases, under its permanent
  production lock; post-run CLI check passes. Correct the resource reader for
  positively observed absent memory controllers without lowering its floor or
  changing kernel settings. Full application/image budgets and complete Qt
  dependency qualification remain open.
- [ ] Integrate and qualify the actual headless first-boot hook, account/login
  ordering, stable host identity across a completed bootstrap/reboot, public receipt
  publication/shutdown and approved access-marker/expiry policy. Real credentials,
  Wi-Fi/SSH activation and ongoing access remain separately gated. USB/HDMI is
  optional recovery, not required by the proposed physical-receipt headless path.
- [ ] Qualify a physical scratch budget, supported native ARM64 builder, pinned
  base/package inputs and entitled Linux ARM64 Qt 6.12 kit with 16-KiB-compatible
  dependencies. Preserve the shared heavy lock and 3-GiB host-RAM floor.
  The read-only probe now accepts a measured component budget and checks the
  selected scratch volume. Windows requires a fixed drive; Linux checks filesystem
  and mount metadata, not physical backing or removability. WSL/overlay/tmpfs and
  ordinary network mounts are refused; layered storage still needs independent
  backing-volume review ([exact scope](../image/README.md#reviewed-environment-and-preparation-checks)).
  Its native Linux RAM observation also accounts for visible parent cgroup limits;
  WSL continues to require the Windows guard. Synthetic counter tests are not
  qualification of a native ARM64 builder.
  Native free RAM/storage snapshots now exist, but no complete application/image
  peak budget has been measured; this qualification item stays open. CMake 3.31.6,
  the owner's Qt 6.12.0 Linux ARM64 kit, reviewed distro development packages and
  user-local Rust 1.98.1 are installed. The three additional D-Bus development
  packages were reverified at their exact approved versions on 13 November,
  with a clean package audit and no repeated installation. The earlier install
  receipt's architecture-qualified package-name verification failure is preserved.
  The probe now rejects Windows/mobile Qt target-platform metadata by reading
  its literal mkspec without executing the toolchain. Installed kit inspection
  now verifies the installed Linux ARM64 Core layout; the reported Enterprise
  licence and selected binary checks do not establish redistribution terms or
  the complete deployment dependency closure.
  Native preset configuration now enforces the existing bounded version,
  target-platform and Qt Core ELF checks before normal Qt discovery. A separate
  explicit payload audit checks hashes, ARM64/16-KiB layouts, interpreter entries
  and transitive dependency-name candidates without executing the target files.
  It now checks bounded GNU version-label tables/hashes and exact label agreement
  against the unique explicit dependency candidate, including weak requirements;
  local validation status is in [OWNER](../OWNER.md). Complete deployment
  selection, target loader/symbol ABI and native execution
  still require the actual builder and payload; fixture passes clear none of them.
- [x] Add native QQ Studio/Player configure/build/test presets and connect them
  to the existing isolated Linux product harness, preserving the same cache,
  sanitizer selection, warnings-as-errors, hardening and 16-KiB link layout.
  Actual CMake rejects this unsupported host; harness fixtures pass. The pinned
  generator source and boot/tool recipes have been inspected locally.
- [ ] Compile and test those actual applications on the qualified ARM64 builder,
  including hostile inputs under sanitizers, before staging image content.
  Native QQ Studio, Player and test programs compile without warnings; all 16
  programs pass hardening/layout checks and six selected non-rendering cases
  pass. The full product and sanitizer suites remain required, so this item
  stays open.
  On 13 November, trusted native test workers verified separate process, mount,
  network and user namespaces, hidden hardware-device paths, loopback-only
  networking and a private software display under the original heavy lock.
  Initial single-worker diagnostics time out at 180 seconds in the second
  window case with both render loops. The follow-up uses the Linux harness's
  two software-render workers and a 900-second outer budget, preserving Qt's
  watchdog and every assertion: **all 11 publishing-suite results pass in
  133.315 seconds**, with no failures or skips and at least 6,956 MiB available.
  The original 3,072-MiB floor is unchanged. Earlier incomplete diagnostics stay
  in the evidence; the basic-loop trial was not adopted. This passes the complete
  publishing suite, not all products, hardware or CI-worker acceptance. The
  [building guide](BUILDING.md#q-pi-preparation) records the source-match boundary.
  The Linux harness's separate short-runtime regression fails
  against the old script and all 18 fixture tests pass with the fix on ARM64.
  The next full-harness attempt stopped before tests because its filtered native
  snapshot omitted the tracked in-memory keyring fixture. A private tools overlay
  now includes that fixture; the broader software run is in progress. The harness
  gains an earlier missing-fixture diagnostic and a regression that checks no bus,
  display, configure or suite starts. Hosted Linux Lint at `148c91f` passes the
  new regression and all 19 harness fixtures; its 840 Python results include
  124 platform skips, and all 63 ARQADE Node tests pass.
  The QOR Media helper additionally selects a pinned native ARM64 PDFium package
  with a separate cache. Its digest and library layout pass static inspection;
  the Pi kit lacks Qt PDF/PDF Quick, so native Media compilation and runtime
  acceptance remain open. Hosted Linux Lint at `c1902f2` passes all 23 harness
  fixtures: 720 Python tests pass with 124 platform skips and all 63 ARQADE
  tests pass. The follow-up at `74d22e9` passes the same checks, including a stale
  extraction marker being removed before retrying an incomplete package.
  These tooling changes are merged through PR #5 (`ed3526e`), with the checkpoint
  merged through PR #6 (`b79c40c`). The queued first journey now passes once in
  201.292 seconds under its unchanged deadline, after verifying 1,404 source
  hashes and five Prism programs. It uses the original native lock and isolated
  software display/device/network views; available memory stays above 6,954 MiB.
  Studio builds and the real Player plays; Prism's Library launch uses a recording
  stand-in. The broad product run retains four play failures and its aggregate
  timeout during Player, with later suites incomplete. No full product or hardware
  acceptance is claimed. Receipts: `build/development-2026-11-13/session-07/`.
- [ ] Build and statically validate a regular-file image with a real maintained
  Pi login/desktop, native QOR apps and local development tools; no simulator.
- [ ] After the [action-time write gates](../image/README.md#authorised-q-pi-target-and-action-time-write-gates), prove Pi boot, hardware graphics,
  Wi-Fi/Bluetooth/input, secure provisioning, local project save/build, offline
  behavior and persistence across three reboots. The owner authorises eventual
  Q Pi image writing to the identified Samsung EVO 128-GB card, including its old
  Kali/data erasure. Fresh device/image checks, erasure confirmation and write
  verification remain required; stock bootstrap and access/provisioning are separate.
  Other devices/installations remain untouched. No write has occurred.
- [ ] Later: split QQ's 2D editing/runtime dependencies from Quick 3D/Physics and
  measure the Pi path; qualify optional user-controlled remote heavy builds.

The first milestone requires usable QOR apps as well as the base desktop.
Image composition, emulation and physical-device acceptance are separate results.
The Fedora full-device/release deliverable below keeps its existing scope.

### Full device image

[QOR-0021](decisions/QOR-0021-projects-engines-and-two-stores.md), decision 12. **No accepted release image yet.**
[image/](../image/README.md) assembles the branding layer and explicit device recipe.
The earlier private Fedora bootc/QEMU proof passes stock installation,
media-detached first run and persistence through two installed reboots. Its image
and compiler are clean `feed626`; the bounded host launcher has separate checksum
provenance. All five actual VM phases exit 0 across preserved resource interruptions,
with four distinct installation/installed boot IDs and no reset of the installed disk.
Review/Cancel and a separate corrupt-payload refusal preserve their complete test disks.
The agent permits four operations and disables 39 others. Agent-triggered reboots
are proved. Actual frontend reboot tests on 8 November failed after installation.
Diagnostics identify bootc's retained storage mount; targeted cleanup makes the
unchanged guard and actual button succeed. The local backend now confines mounts
to its supervised process and checks target inactivity before completion. A separate
loop-node correction retains refusal of writable backing. The fresh clean `66df3f7`
image and same-revision compiler now pass stock installation and the actual UI reboot
on a new 8-GiB disk without manual cleanup or debug overrides. Two installed boots
without media prove welcome changes from pending to complete, then retains that
record before the next welcome; all three native VM phases exit 0 with three distinct
boot IDs. The final verifier accepts the evidence in
`build/vm-ui-reboot-2026-11-08/isolated-ui-proof/`; the earlier proof and failures
remain separate and unchanged. Both new image branches pass
14 bootc checks with zero warnings and one skipped. The lab excludes the full
desktop, products and authentication, and clears no hardware or release gate.
Calamares is an inactive native Qt framework candidate under
[QOR-0023](decisions/QOR-0023-installation-uses-proven-backends.md); its disk/deployment backend is not integrated.
QOR OS is not ready to ship because its desktop applications compile.

- **Who:** the lead engineer builds it. The owner gives the hardware and says when anything is published.
- **Depends on:** the native product workflow proven first; Phase 3's KMS backend and sandbox; an isolated build path; a key for Secure
  Boot; The Qt Company's letter before anything is distributed.
- **Accepted when:** a reproducible build with its inputs recorded; it boots on the declared reference hardware; it installs; an update
  applies and a deliberately broken one rolls back; recovery works from a failed boot; and the core journeys pass on that image.

## History: QTOS phases

QOR OS carries QTOS's work. QTOS's own phases ended where QOR OS began:

- **Phase 0 · Foundations: complete (v0.1.0, 23 October 2026).**
  - Lumen tokens, `Theme`, `Spring`, SDF `Surface`, `Glass` and `Backdrop`.
  - The Lumen controls style and `QStyle`, and the platform theme.
  - Service contracts, the simulator, `qor-simd` and `qorctl`.
  - The whole shell in simulation, and six apps.
  - The benchmark, the tests and CI.
- **Phase 1 · Nested Prism: complete except three items.**
  - **Done:**
    - Prism nested on Linux and WSLg with real clients;
    - client windows (configure, decorations, moves and resizes, popups,
      activation) and Spaces for client windows;
    - the controller interface and real audio;
    - Wayland extensions (fractional-scale, cursor-shape, presentation-time,
      idle-inhibit);
    - three-finger gestures and several displays;
    - the Store's stage 1.
  - **Carried into QOR OS:**
    - the decision gate, deferred by QOR-0005;
    - the Qt Interface Framework evaluation and the Design Studio project,
      now in Phase 1 above.
- **Phases 2 to 5** are folded into the phases above:
  - the bootable image and the KMS backend into Phase 3;
  - the platform daemons and sandbox into Phase 3;
  - the premium and ecosystem items into Phase 4.

  The Yocto plan was replaced by QOR-0004.

# Changelog

All notable changes to QOR OS, and before it QTOS, are recorded here. The
format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the
project uses [Semantic Versioning](https://semver.org/) from 1.0 onward. The
products (QQ, ARQADE, QOR Media) kept their own history in demiurge-chain until
29 October 2026 ([QOR-0015](docs/decisions/QOR-0015-the-products-are-developed-here.md));
their changes since, and pulls of what is still shared, are recorded here.

## [Unreleased]

- **Complete native test fixtures, 13 November.** The Linux product harness
  refuses a missing fake-keyring fixture before starting its private bus, display,
  build or suites. The diagnostic names the missing tracked helper; a regression
  checks that no test services or build commands start. Native contribution
  guidance includes test-support files when preparing a reviewed source snapshot.

- **Native contribution continuation, 13 November.** The next development branch
  documents offline Q Pi/QQ agent-tool discovery, source/binary matching, original
  resource coordination, private software-test boundaries and reproducible handoffs.
  It distinguishes diagnostic time limits, selected cases, complete suites and
  hardware acceptance without introducing a second roadmap.

- **Dependency repair and Linux test isolation, 13 November.** Upstream PR #118
  now carries patched core/Launcher dependencies and Cargo artifact discovery
  for the real-spawn tests, plus compatible blockchain dependency patches.
  Local core/host audits clear the four vulnerability findings; maintenance,
  service SQLite, chain and hosted acceptance gates remain recorded. The QOR OS
  pin is unchanged. Linux product tests now use a private short runtime directory
  in hidden-display modes, with a regression for long inherited runtime/temp
  paths and cleanup that preserves the desktop's directory. Q Pi development is
  reopened; native software-display qualification continues under its original
  resource lock and unchanged memory floor.
  All 18 Linux harness fixtures pass on the Pi, including a regression shown to
  fail against the old script. Initial native publication diagnostics time out
  with one software-render worker. With the harness's two-worker default, all
  11 results in the full publishing suite pass in 133.315 seconds, with hardware
  devices hidden, no external networking and no assertion changes. The earlier
  diagnostics remain preserved. QOR OS PR #4 integrates the harness fix.
  Upstream's patched chain passes all 213 hosted workspace tests with WebAssembly.
  The core passes 245 tests with 10 ignored; the Launcher host passes 109 with
  five ignored, including the four repaired spawn cases and the new regression.

- **Core audit continuation prepared, 13 November.** The new
  `feat/core-audit-remediation` branch adds an upstream repair procedure tied to
  the official advisory records, actual desktop/bundled-SQLite features and the
  existing resource/test gates. It separates patched-version candidates from a
  tested dependency selection and requires synchronized source references for a
  future pin update. No dependency, advisory exception or core pin changed.

- **Developer/domain integration, 13 November.** Main includes the new offline
  discovery and canonical-domain policy through `0cdae3d`, after hosted Lint
  passed 714 Python tests with 124 platform skips and all 63 ARQADE rule tests.
  GitHub's homepage field is verified as QORSYNC.DEV. Upstream domain maintenance
  is draft PR #118; source checks, automatic previews, dependency-audit failures
  and the still-open live service migration are recorded separately. The final
  local all-file validation remained resource-blocked; normal commit hooks pass.

- **Canonical public domain, 13 November.** QORSYNC.DEV and its service
  subdomains are the approved first-party addresses (QOR-0026). GitHub setup
  retains that homepage; README navigation keeps direct developer/progress links.
  Retired contact-domain references were removed from release guidance without
  inventing replacement mailboxes. A bounded tracked-text check and hostile-input
  tests join commit hooks and hosted Lint. Upstream source, hosting configuration,
  mail verification and installed clients remain separately verified work.

- **Pinned core audit, 13 November.** The on-demand hosted dependency check
  reports four vulnerability advisories and 13 warnings at core pin `6a4822d`.
  The same affected versions remain in upstream `74a1014`. The security limits
  and masterplan now record upstream remediation, target/feature review and
  tested pin movement as the next priority; no audit exception or pin change was
  made. This is distinct from the passing history scan and developer-tool Lint.

- **Offline developer discovery, 13 November.** A Python-only `list`, `show`
  and `check` command exposes reviewed source, guidance, prerequisites and action
  boundaries for apps, Lumen, services, Prism, Q Pi, products, assets and upstream
  Launcher/DRC-369/agent work. Versioned JSON supports agent clients. Bounded
  metadata validation refuses unsafe paths, links and malformed records without
  echoing rejected values. The tool never executes an integration, reads linked
  source content or connects to a service. Hostile-input and fresh-checkout CLI
  tests join the existing hosted tooling suite; public SDK and runtime bridge
  capabilities remain separate Phase 4 tasks.
  Hosted Lint at `5a853c2` passes 706 Python tests with 124 platform skips and
  63 ARQADE rule tests. Fourteen focused Windows tests and commit hooks pass;
  the full local tooling run and all-file hooks remain resource-blocked under
  the unchanged 3,072-MiB floor.

- **GitHub setup on Windows, 13 November.** Decode GitHub CLI responses as
  UTF-8 so milestone titles with punctuation match their existing records instead
  of causing duplicate-creation attempts. The regression reproduces a legacy
  Windows locale with an inert child process. Repository setup also retains the
  homepage link to the canonical documentation index.

- **Main integration and developer entry points, 13 November.** The owner requested
  merging the validated integration branch into main. The older readiness change
  on main was reconciled with its tested successor, preserving the fail-closed
  build gates and upstream ancestry. The homepage now directly links progress,
  the masterplan, developer entry points and security. Source-verified guidance
  distinguishes QQ's existing MCP tools from the upstream Launcher/DRC-369
  work and the four-command core bridge. Phase 4 now records offline discovery,
  upstream bridge capabilities and reproducible onboarding as explicit tasks.
  Qt CI remains paused; this integration does not accept a release or hardware.

- **Native QQ protocol continuation, 13 November.** All seven non-windowed
  launcher-protocol cases pass on ARM64 (nine Qt Test results including setup and
  cleanup, no failures/skips). They exercise ordering, refusal and hostile framing
  against a local stand-in; no live publication occurs. The source snapshot plus
  committed Store overlay and binary layout were verified. The initial stale
  snapshot refusal remains in session-03 evidence. Windowed publication, complete
  products and the whole creator/player journey remain open.

- **Contribution workflow and Actions policy, 13 November.** Corrected the unsafe
  release-folder recommendation, carried-code licensing wording and commit-message
  guidance; added explicit validation outcomes and an assistant handoff checklist.
  GitHub now requires full action commit pins and selects six reviewed action
  repositories, verified by API readback. The offline hook/Lint check exposed two
  platform checkouts with persisted credentials; both now disable persistence.
  Setup verifies Actions writes and fails on rejected rulesets/security settings.
  Regression tests cover malformed inputs, privilege changes and API failures.
  Merge commits are available for required upstream fixture ancestry. The prepared
  ruleset requires Lint, history scanning and readiness, with no administrator
  bypass, but remains unapplied because the private-repository plan rejects it.
  The repository stays private and both Qt runners remain offline. Evidence and
  settings recovery snapshots: `build/development-2026-11-13/session-03/`.
  Hosted Lint run `43461849136` subsequently passes at implementation commit
  `3ad8954`: 691 tooling passes, 124 platform-specific skips, and all remaining
  configured Lint checks pass. The Qt build pause remains in force.

- **Native OS software gate and ARM64 Store fixtures, 13 November.** After the
  reviewed `9d67ab2` checkpoint was pushed, the full core-enabled Pi development
  build completed. Its first test run passed 25 of 27 suites: four cases failed
  because the signed demonstration omitted `linux-arm64`. The generator now
  includes it in catalogue and package metadata; regenerated demonstration
  signatures use throwaway keys, with all packaged content and the first-party
  catalogue unchanged. The regression fails before the fix; all 15 generator
  tests and three affected Windows Store/Library suites pass afterward. The full
  native rerun passes all 27 software suites without failures/skips, hardening
  passes 42 programs, 44 linked files pass ARM64/16-KiB layout checks, and QML lint
  has zero warnings. QQ's complete scene and Player sandbox suites pass 56 and
  three Qt Test results respectively, including setup/cleanup. Rust's pinned
  future-compatibility warning remains; full products, hardware and isolated CI
  are not accepted. Original failures, the corrected artifact-selection harness,
  source overlays and receipts remain in `build/development-2026-11-13/session-02/`.

- **Reviewed development checkpoint authorised for Git, 13 November.** The owner
  requested committing and pushing all pending QOR OS work. This checkpoint
  includes the validated Prism regressions, benchmark evidence checks and
  reconciled native/Windows documentation. Build receipts stay under ignored
  `build/`; the Rust warning, full native/product checks and release gates retain
  their recorded limits.

- **Native core bridge and Windows validation, 13 November.**
  The owner reopened Q Pi alongside QOR OS. The existing `ci-windows` preset
  configures and builds; all 22 selected software suites pass without failed or
  skipped suites, 33 programs pass hardening, and QML lint has zero warnings.
  Tests use private settings, offscreen software rendering, silent audio and
  virtual controllers. Rust still reports the pinned `proc-macro-error2` 2.0.1
  future-compatibility warning, so the warning-free gate remains unmet. The
  native core also builds through the existing pinned FetchContent/Corrosion
  integration with Rust 1.98.1 and unchanged Cargo.lock (1,385.79 seconds).
  Its bridge suite passes nine Qt Test results, zero failures/skips; the program
  passes hardening and ARM64/16-KiB layout checks. The same dependency warning
  remains on ARM64. Temporary Qt helpers exit naturally before the original
  coordinator releases its lock. Complete native OS, products, GPU suites and
  isolated CI remain separate.
  Reverified 66 saved native Prism/QQ evidence files and the 1,404-file source
  snapshot, corrected stale roadmap/guide claims, and confirmed the approved
  D-Bus package versions without repeating installation. Original failures,
  source work and locks are preserved. Receipts:
  `build/development-2026-11-13/session-01/`.

- **Linux performance-gate fixtures validated, 13 November.** All 51 validator,
  launcher and bundle tests pass natively on the Pi with zero failures/errors/skips,
  including the 25 Linux process cases previously skipped on Windows. The
  original coordinator and resource floor are unchanged. Sources are unchanged;
  fixtures use inert programs and hardware metadata, so this is tooling evidence,
  not a measured graphics/input improvement or hardware acceptance.

- **Native QQ compile and selected logic checks, 12 November.** Studio, Player
  and test programs compile without warnings under the native preset and original
  serial guard (883.16 seconds). All 16 programs pass hardening and ARM64/16-KiB
  layout checks. Three scene cases and three fake-launcher protocol cases pass
  without failures/skips: ten Qt Test results including setup/cleanup. They use
  private folders and disconnected desktop/audio/keyring services, without
  overriding renderer expectations or publishing anything. This is not full QQ,
  physical graphics, other products, x86 or OS acceptance. Official Rust 1.98.1
  ARM64 components are checksum-verified and installed only in a separate user
  prefix, with no shell/system settings changed; core compilation is still pending.
  Receipts: `build/q-pi-connect-2026-11-12/session-01/native-qq-20261112/`.

- **Native ARM64 Prism validation and acknowledgement-fixture correction, 12 November.**
  The Pi's Qt 6.12.0 kit and 18 reviewed distro development packages enabled a
  warning-free serial Prism build in 1,678.5 seconds, with no package upgrades
  or removals. The initial new regression failed reproducibly because it expected
  another client to steal focus. Its test-only fix asserts that refusal, then
  explicitly activates the second window through the existing Shelf path;
  production focus behavior, timeouts and all acknowledgement/geometry checks
  remain unchanged. Corrected Release passes seven selected suites, zero skipped:
  pointer capture has 43 Qt Test results and window management six. All 11 built
  programs pass hardening and ARM64/16-KiB layout checks; QML lint has zero warnings.
  The native ASan/UBSan build also completes without warnings (3,005.93 seconds),
  and its same seven suites pass without skips or sanitizer diagnostics. Actual
  compiler flags and both sanitizer libraries are verified on Prism and the tests;
  all 11 sanitizer programs pass hardening/layout checks. Leak checking remains
  disabled by the existing preset. Temporary licensing helpers exited naturally
  before reviewed incomplete-marker recovery; the original
  lock inode, memory guard and license behavior remain unchanged. Source, initial
  failures, exact overlay and receipts: `build/q-pi-connect-2026-11-12/session-01/`.
  These core-disabled, software-rendered tests use product stand-ins and are not
  full OS/product/hardware, leak-check or release acceptance.

- **Explicitly authorised Prism resource review, 12 November.** Reverified 23
  incoming file fingerprints, 57 sealed research evidence files and both
  incremental caches. The only confirmed service candidate, Google Play Games
  Services (about 26 MiB), could not be opened for a graceful stop by this
  non-elevated session; no service or application stopped. One measured clean
  guest-cache reclamation produced 30 seconds of observer-resident headroom at
  3,860–4,067 MiB, but the actual validation scope was stopped before admission
  at 2,869 MiB, 203 MiB below the unchanged floor. No compiler/test ran and no
  further validation attempt followed. Protected work and all source candidates
  remain intact. Receipts: `build/prism-resource-2026-11-12/session-01/`.

- **Performance research applied to benchmark evidence quality.** Prism's
  diagnostic gate now requires all 40 posted input samples for timing acceptance,
  validates complete distributions and count/rate consistency, and bounds report
  parsing. The bundle carries the same production validator. Valid partial trials
  remain diagnostics; malformed reports fail. Three reproduced false passes are
  fixed, 26 portable tests pass, and six preserved real software reports validate
  without being rerun. Linux launcher/bundle process tests remain platform-skipped.
  Existing timing targets and the Prism milestone order are unchanged; no speed,
  hardware or release-readiness gain is claimed. The performance guide records
  what is already implemented, what can follow runtime validation and what needs
  hardware evidence, and removes advice to select only faster runs.

- **Saved Prism continuation reverified, 12 November.** All 16 incoming source
  hashes and 78 saved evidence files match; both focused caches and Release's
  extracted production object remain present. No source fix was needed during
  this continuation. One measured clean WSL-cache reclamation allowed 30 seconds
  of resident host headroom above the unchanged 3,072-MiB guard. The preserved
  incremental Release build resumed at about 3,519 MiB, then the guard stopped
  its owned scope at 3,008 MiB. No additional compilation unit completed and no
  new regression or sanitizer ran. There was no unchanged retry, app closure or
  service/security/startup change. Evidence: `build/prism-boundary-2026-11-12/session-03/`.

- **Prism boundary build advanced; runtime remains memory-blocked, 12 November.**
  Fixed the new test target's Qt Wayland import scope: the sibling tests directory
  now resolves the same installed modules. Release configuration/generation then
  succeeded, and the unchanged extracted production source compiled. Compilation
  did not finish; the new tests and ASan/UBSan have not run. Measured clean WSL
  page-cache reclamation admitted the job, but the unchanged host guard stopped
  it at 2,956 MiB. A fresh incremental continuation with a stricter, temporary
  256-MiB memory-pressure threshold stopped at 3,060 MiB, below the 3,072-MiB floor.
  An identity/visibility/idle-checked Search/Widgets working-set trim followed;
  no further compile was attempted without stable resident headroom. ChatGPT,
  the voice call and active work were untouched; no app was closed, no service,
  security or startup setting changed. Evidence: `build/prism-boundary-2026-11-12/session-02/`.

- **Linux/platform performance research documented, 12 November.** Added a
  source-linked research section to `docs/PERFORMANCE.md`, distinguishing
  historical observations from unmeasured candidates, hardware-specific
  tradeoffs and future decision gates. Recommends maintained Linux foundations
  while leaving evidence-led configuration or kernel changes open. No behavior,
  kernel/configuration, runtime test, resource guard or milestone order changed.

- **Prism acknowledgement-boundary regressions prepared, 12 November.** The raw
  client withholds real focus-change configure acknowledgements, checks that
  changed geometry cannot replace the configured frame before the latest reply,
  and requires unchanged geometry to be adopted on a later acknowledged commit.
  `tst_windowmanagement` exercises null and wrong-object inputs directly and via
  Qt's invokable interface. The unchanged production class now has its own files
  shared by Prism and the test, without a production test hook. In the initial
  session compilation and runtime were unrun: the resident Windows guard stopped this owned Linux
  scope at 2,384 MiB against its unchanged 3,072-MiB floor, before build admission.
  Prior passing evidence remains separate. See OWNER for the ordered continuation.

- **Owner-authorised source checkpoint, 12 November.** Rechecked all 52 incoming
  file fingerprints and 150 saved evidence files before commit preparation.
  Windows verification passes 203 portable tooling tests, with 15 Linux-only
  skips; the initial missing-Ninja failure and corrected-environment rerun are
  preserved under `build/commit-checkpoint-2026-11-12/session-01/`. No production
  or test source changed during this check. Ignored build evidence remains local,
  and Q Pi remains on hold.

- **Prism manager lifecycle regressions pass in Linux software; Q Pi remains deferred.**
  Extended the saved warning candidate's existing raw-client test to reject
  duplicate custom globals and bind, destroy and rebind all five managers,
  including cursor-shape v1/v2. Client stubs use the existing vendored protocol
  XML, including the tablet interface referenced by cursor-shape. The original
  patch, all uncommitted work and evidence are preserved; production candidate
  files are unchanged, retaining the prior 12-to-zero metadata warning result.
  On 12 November, focused Release and ASan/UBSan builds completed without warnings;
  each passed 42 pointer-capture results with no failures/skips and all four
  affected suites. Root QML lint is warning-free in both builds; hardening passes
  15 Release and 36 sanitizer programs. All 18 exercised program identities and
  all nine sanitizer programs' actual runtime linkage were verified. Leak
  detection was off. Execution stayed hidden, GPU-fenced and under the unchanged
  memory guard and original QOR lock; previous refused/interrupted attempts are
  preserved. The core library was disabled in these focused caches. Direct
  null/wrong-object and deferred-acknowledgement regressions remain pending;
  hardware, full-OS and isolated-CI acceptance are not established by this run.
  See OWNER for the continuation; no Pi work is a core prerequisite.

- **Small native Q Pi component build prepared.** `tests/pi/` reuses Prism's
  actual command-line parser and its existing development/device-option suites
  with Qt 6.12 Core/Test. Release and sanitizer presets retain the native/static
  kit gate, hardening, 16-KiB linker settings, single-job execution and bounded
  tests. Added preset/wrong-host regressions, including configuration without a
  preset. Source review found no blocking issue; execution was not started at
  2,741 MiB against the unchanged 3,072-MiB guard. A bounded authorised connection
  diagnostic found no hostname/last-address access or mDNS address. Standard
  prerequisites are now authorised subject to concrete transaction review, but
  Pi access and actual commercial ARM64 entitlement remain blocked. No native
  application, package install or image result is claimed.

- **Q Pi GNU version-label audit prepared locally.** The existing bounded ELF
  reader now checks version requirements/definitions, relative chains, indices
  and label hashes. The payload audit requires exact label agreement with each
  unique explicit dependency candidate, including weak requirements; inherited
  labels and unversioned providers cannot fill gaps. Added hostile binary
  fixtures; 59 focused Windows Python tests pass with zero failures/skips after
  authorised RAM recovery, under the original lock and unchanged 3,072-MiB floor.
  Symbol binding, loader resolution and full ABI compatibility remain
  unqualified. See OWNER for current execution evidence; these fixtures are not
  native Qt/QOR or image proof. During that increment the Pi was reported
  unreachable and the separate connection investigation was left alone.

- **Real Pi coordination/resource qualification, 11 November.** The authorised
  stock Pi 5 ARM64/Trixie builder ran 80 focused tests with zero failures/skips,
  including Linux process/lock cases. Corrected native RAM observation to consult
  advertised cgroup controllers before requiring their counters, while retaining
  all applicable ancestor limits, metadata rechecks and the 3,072-MiB floor.
  No kernel/resource settings changed. Bootstrap and the production lock check
  pass; this is not Qt/QOR/image acceptance. Recorded restricted-key access with
  approved first-use host trust, the missing CMake/Qt kit, four pinned CMake
  package candidates and concrete commercial ARM64 entitlement/module questions.
  No package install, media write, credential creation or service change occurred
  in this repository session; key provisioning was separately authorised/performed.

- **Native Linux heavy-job coordination prepared.** A per-account permanent OS
  lock and durable incomplete-run marker serialize cooperative native QOR jobs;
  the launcher checks the existing 3,072-MiB RAM floor while holding the lock,
  supervises orphan descendants and stops only its own unreaped process group.
  Unsafe paths, busy locks and uncertain cleanup refuse further work without
  automatic recovery. Original Windows/WSL coordination and resource settings
  remain unchanged. Windows validation records 28 portable passes and 11 Linux
  skips; actual native locking, ARM64 application and Pi/image acceptance remain
  unrun. Canonical commands now use the launcher after native qualification.

- **Documentation reconciled with the local tree and dated evidence.** The
  canonical index now includes subsystem guides and explains the authority of
  current instructions, historical decisions, drafts and ignored handoffs.
  Corrected stale CI/runner and device-service claims, unsafe default build
  examples, carried-product links and unmeasured performance assertions. Q Pi
  docs now state the scratch probe's actual mount-check limits and the public
  receipt helper's hostname/comment handling; original evidence and decision
  history remain unchanged. No runtime behavior or native/hardware acceptance
  changed in this audit.

- **Headless Q Pi preparation with pinned SSH identity.** The first-boot design
  now uses the reviewed Trixie `cloudinit-rpi` interface and per-device credentials,
  with normal console/desktop login retained. A public-only helper validates and
  preserves identity receipts, refuses malformed/changed/partial input and prepares
  bounded Windows OpenSSH argument plans without connecting or reading private
  keys. Tests cover hostile metadata, retry/interruption, SSH option injection and
  probe-output failures. The existing Windows client parses the plan offline and
  agrees on the public fixture fingerprint. Actual first-boot hook/marker integration,
  shutdown, Wi-Fi, SSH and authenticated access remain unqualified; no real credentials,
  services, security settings or media were changed.

- **Scoped future Q Pi media authorisation recorded.** The owner selects the
  connected Samsung EVO 128-GB card, including old Kali/data erasure, for the
  eventual ready Q Pi image only. Canonical planning retains fresh capacity/USB
  identity and system/boot-disk checks, image provenance/hash/compatibility/size
  validation, action-time erasure confirmation and completed-write verification.
  Stock bootstrap, credentials and access changes remain separate. No write ran.

- **Pi 5 bootstrap qualification before Qt.** A read-only `--bootstrap-only`
  preflight checks native ARM64/64-bit userspace, bounded Pi 5/Trixie metadata,
  the running kernel's 16-KiB pages and the unchanged RAM guard without executing
  Qt, Git or package tools. A pass explicitly leaves the builder unqualified.
  The canonical image guide separates stock bootstrap from Q Pi and records
  local first boot, package-delta simulation, Qt modules/entitlement, one-card
  recovery, native lock prerequisites and application/image acceptance. No Pi
  provisioning, installation, media write or native application build occurred.

QOR OS begins: QTOS and Demiurge's device-side products in one repository
([QOR-0002](docs/decisions/QOR-0002-one-repository-from-qtos-and-demiurge.md)).

### Added

- **Q Pi kit enforcement and explicit payload audits.** The native QQ presets
  check Qt 6.12 Linux target metadata and ARM64/16-KiB Qt Core layout before
  normal package discovery. A separate read-only audit checks a bounded,
  hash-pinned manifest of applications, libraries, plugins, helpers and loaders,
  records their ELF dependencies and rejects missing or ambiguous dependency-name
  candidates. It executes no target code or kit toolchain. Hostile fixture tests
  cover malformed ELF metadata, path escapes, changed hashes and input/output
  bounds. Static name coverage does not certify symbol ABI, actual loader search,
  complete QML/plugin selection, commercial entitlement or native execution.

- **Qt target-platform metadata checks for Q Pi.** The read-only probe now
  distinguishes literal Linux desktop mkspec metadata from Windows and mobile
  ARM64 kits without executing a kit's toolchain. Ambiguous, dynamic, commented
  and oversized inputs have refusal tests; this does not certify target ABI,
  commercial entitlement, complete dependencies or an image.
- **Explicit CMake discovery in Windows IDE presets.** Windows presets point
  CMake Tools at the already installed Qt-bundled executable, while Linux keeps
  its own lookup. VS Code's automatic configure/build triggers are disabled and
  build/lint tasks use the separate `ci-windows` folder. Host settings and tools
  are unchanged.

- **Native ARM64 QQ build entry points for Q Pi.** Configure/build/test presets
  now select QQ Studio, Player and tests with single-job Ninja builds, warnings
  as errors, existing exploit mitigations and 16-KiB linker page sizes. A native
  host gate rejects x86-64, WSL and cross compilation before compiler discovery.
  The existing Linux product harness accepts these presets and builds/tests the
  same cache without replacing sanitizer mode or permitting parallel builds.
  The image guide records the pinned generator source/boot-tool inspection and
  actual build commands. Portable CMake refusal tests pass; ARM64 compilation,
  dependency/licence qualification, image composition and Pi testing remain open.

- **Native Linux RAM checks for Q Pi preparation.** The probe now measures
  `MemAvailable` and the remaining memory allowance of its visible cgroup v2
  ancestry instead of always reporting Linux RAM as unmeasured. Parent limits
  constrain the result; missing counters, unsafe paths, ambiguous metadata and
  a membership change during observation fail closed. Swap cannot increase the
  estimate. WSL still requires the unchanged Windows host check. Synthetic
  counter tests establish neither ARM64 builder qualification nor hardware proof.

- **Measured scratch budgets for Q Pi preparation.** The read-only probe can
  check an explicit component budget against an existing local scratch volume.
  It rejects duplicate or unknown fields, missing estimates, imprecise byte
  counts, overflow, insufficient capacity and unsupported mount/drive types.
  Linux filesystem/mount checks do not attest physical backing or removability.
  WSL's virtual free space cannot clear the backing-volume check. The shared
  QOR memory guard also refuses caller-supplied thresholds below its existing
  3-GiB floor. These checks neither reserve resources nor qualify an image,
  builder, dependency closure or Pi hardware.

- **Independent host-resource checks.** QOR OS and Q Pi use only their existing
  resource guard and heavy-job lock, with no service-management dependency on
  unrelated projects. Retired cross-project service wrappers, plans and lifecycle
  records are removed from the local build folders. Mixed QOR records omit the
  unrelated service bookkeeping and identify the metadata cleanup; dependent
  checksums are updated and retained QOR evidence is rechecked. No runtime code
  changed, and no other project's files or services were accessed.

- **Q Pi development preparation.** QOR-0025 scopes a Pi 5 ARM64 edition using
  a supported base login/desktop before native QOR applications, with no simulator
  or change to the full-device guard. A read-only Python probe checks Qt version
  metadata, source identity and bounded ARM64 ELF load layouts, including hostile
  input tests. The canonical image guide covers physical backing-space sizing,
  Bluetooth bootstrap, Wi-Fi/key-only SSH provisioning and actual Pi acceptance.
  No ARM64 build, image, device boot, tool installation or provisioning is claimed.
- **The standard SELinux context for the private installer.** The live recipe
  pairs Fedora's existing `install_exec_t` executable label with the unit's
  `install_t` process domain and checks its label lookup during image building.
  A guest diagnostic verifies the required label capability and actual Qt target
  inventory in that domain while enforcement stays enabled. No policy permission
  or permissive fallback is added. The clean `fa17674` image passes normal startup,
  review/Cancel and corrupt-payload refusal, but its first stock installation exits
  1 from the generic service domain and leaves the full disk unchanged. The later
  clean `feed626` image now passes stock installation, media-free first run and
  persistence through two installed reboots in the private VM lab.
- **Port-ready startup in the private installation lab.** The restricted guest
  agent and installed welcome now start from their assigned virtual-port events;
  the live installer selects its own evidence-port event. Early boot-target links
  no longer race slow device discovery. Existing device, disk and RPC guards stay
  in place, and copied service files explicitly use non-executable permissions.
  The `f132dc1` candidate built an installer ISO and passed warning-fatal image
  checks, but its normal startup exposed that race. A separate diagnostic boot
  displayed the real installer, exercised target review and Cancel, and preserved
  the entire test disk's hash. The rebuilt `fa17674` image subsequently passes
  normal automatic startup and review/Cancel. Successful installation is now
  separately proved on `feed626`; diagnostic overrides keep their original scope.
- **Guarded offline installation candidate for the private VM lab.** The opt-in
  image path connects the Qt installer to stock bootc using a complete,
  checksum-checked OCI payload on read-only media. The installed profile adds
  a locked local test account, a limited welcome that makes no desktop or
  authentication claim, and bounded public setup/log evidence. A restricted
  guest agent permits policy inspection and shutdown/reboot only; execution,
  file access and time changes are refused in the actual agent probe.
  The launcher revalidates the ISO after acquiring its ancestry/file handles.
  End-to-end installation and persistence remain pending until VM evidence exists.
  The first deployment-image attempt exposed a missing root-home target in the
  shared recipe: dracut returned success while reporting an install error.
  The diagnostic gate rejected that image; the shared stage now creates the
  directory before either initramfs build.
  Bootc warnings now fail both image branches, with an additional wrapper
  diagnostic check. The recipe removes the reported generated package caches
  and logs, and declares the required runtime directories. A read-only Windows
  memory preflight blocks expensive image phases and VM launch below 3 GiB
  available RAM. The second composition was stopped for observed memory pressure;
  no installer ISO or VM installation is accepted from that attempt.
- **Stable gamepad enum names for QML.** The existing Gamepads singleton names
  its QML export explicitly, retaining its public name while addressing a
  qualified-name registration warning exposed by the fresh Linux build.
  A focused test imports the actual module and reads its button enums.
- **Private disposable-VM image and installation development.** A digest-pinned
  Fedora 44 bootc lab prepares a UEFI live-display ISO with the official mark,
  GRUB diagnostics, Plymouth files and a host-built Qt runtime. It is explicitly
  a probe with installation unavailable, not a release or installed-system pass.
  A separate opt-in Qt installer and stock-bootc adapter are under development;
  encryption and QOR ID authentication remain unavailable. Fresh regular-file
  targets/private firmware variables, pinned Windows ancestry and open QEMU file
  descriptors prevent host-device/path substitution. Guest UUID, serial,
  capacity, VirtIO/PCI ancestry and inactive-use checks precede each transaction.
  Inert regression tests exercise hostile targets, output/exit propagation and
  process-group cleanup. Actual install, media-free boot, first run and second
  reboot evidence remain required by QOR-0024. Build attempts and resource failures
  are retained; no readiness gate is cleared by synthetic tests.
  The first real ISO reached the live console after manual GRUB recovery. The
  corrected `c8ccc9b` ISO boots automatically through the correctly sized menu,
  shows the OS-loading mark and exposes readable output with Escape. Its Qt
  service still aborts on absent default DRM `card0`; new source selects the
  connected display by the sealed VM's VirtIO graphics ancestry. The offline
  source now requires read-only ISO mount/file identity as well as checksum,
  preventing in-place edits that an open descriptor alone would not prevent.
  The display selector, disk collector and read-only ISO checks pass in the real
  diagnostic guest. Its live welcome renders, survives interruption without
  completion, then completes through three deliberate Return presses with a
  checked public state record and exit zero. Those RAM-only overrides are not
  rebuilt automatic startup, installed boot or reboot-persistence proof.
  Source fixes retain embedded modules,
  bound the official menu mark and use software DRM; a missing PulseAudio client
  library was reproduced and corrected in the target dependency closure. The new
  audit checks 62 ELF files and rejects a missing dependency even on loader exit
  zero. Runtime receipts retain the actual compiler revision and fail on changed
  files; future staging builds and checks frozen source inputs before publication.
  Windows passes 13 installer and 20 first-run cases, module lint and hardening of
  30 programs. Updated guard/provenance/display checks pass 35 cases per Windows/Linux,
  plus seven Linux process/source checks and two Windows asset cases, without
  failures/skips in those targeted suites. Refused process supervision now leaves
  the signal handler intact. The earlier Linux harness correctly hit the parent
  guard; the supervised harness passes without weakening it. Ubuntu connectivity
  recovered without a restart; fresh Linux installer/sanitizer compilation remains
  pending. Installation, media-free boot and persistence remain
  untested; encryption stays disabled and no QOR ID flow is offered.
- **Installation groundwork with explicit capability limits.** The private `qor-setup` app
  introduces the desktop with the official QorMark, Lumen, keyboard navigation and finite
  reduced-motion-aware page transitions. It preserves unknown/changed completion records,
  publishes its own small record without overwriting, and leaves welcome pending on close
  or failure. Prism requests it once after device-session unlock, excluding simulation and
  live media; an already-unlocked launch still reports failure to the host.
  Motion/contrast preferences reach the welcome at launch; unknown motion hints keep it still.
  The image target stages 25 checked files: startup branding, QOR identity, original
  Calamares theme data and deliberately inactive installer settings. A saved data-only
  Docker image is checked byte-for-byte against current source. The separate device recipe
  requires an approved existing immutable base and rejects development apps, missing device
  components and active unproved installer settings. The header gate includes `setup/`.
  [QOR-0023](docs/decisions/QOR-0023-installation-uses-proven-backends.md) and the
  [capability map](image/README.md#installation-capability-map) record the boundary:
  no working live session, partitioning/deployment adapter, bootloader installer or installed
  OS has been demonstrated. No host partitions, boot settings, firmware, VM, licences or runner
  configuration changed. This is development progress, not bare-metal readiness.
  Final first-run checks pass 20 Qt cases per normal/sanitizer Linux and Windows, with no
  failures/skips. Fifteen image and three archive tests pass per host, and the saved data
  image contains 25 current source-matching files. Hardening passes 37/35/28/5 programs in
  normal Linux/sanitizer/Windows/device caches. Three real app captures are inspected.
  The new layout warning and Windows completion failures were fixed; the separate warning
  guard still fails on 12 existing Wayland type-generation diagnostics. Incremental passes
  do not clear that baseline, product failures, full OS suites or hardware/VM evidence.
- **The existing QorMark now introduces the shared Prism session.** A dark, responsive startup surface releases on
  the first valid output frame with Lumen's 280 ms cosmetic fade, reduced-motion handling, Escape dismissal and a
  bounded watchdog. Zero-size surfaces stay hidden until layout is ready; a new regression first fails on the
  earlier implementation. The lock screen and authentication remain underneath. A host-only renderer exports the
  original mark for software rendering and the planned OS splash; source/recipe hashes, bounded PNG decoding and
  explicit write failures protect the reviewed asset. The optional `qor_boot_payload` target stages seven reviewed
  Plymouth/dracut files in the build directory and verifies them without overwriting existing work. Firmware stays
  vendor-controlled; there is no packaged bootloader or bootable image builder. The OS-loading theme, initramfs
  inclusion and the complete boot handoff still require actual image/VM/hardware evidence. No host boot setting,
  disk, firmware, VM, licence or runner configuration was changed.
  Normal/sanitizer Linux startup checks pass 12 Qt cases each, zero failures/skips; the existing Kit passes ten
  normal cases and the real exporter passes three tests per build. Sixteen hostile asset/payload tests pass per
  Windows/Linux host, zero failures/skips. Actual private host/two-output simulated Prism captures are inspected,
  and three before/three after software benchmarks are recorded separately from boot/hardware evidence.
  Native Windows startup also passes 12 cases, zero failures/skips; its offscreen fixture explicitly uses the
  repository fonts, retaining the warning gate after an initial missing-default-font-directory failure. Exploit
  mitigation checks pass 34 normal/32 sanitizer Linux and 25 native Windows programs. Early failed harness/capture
  attempts remain archived; these focused results do not clear the existing product, warning, image or CI gates.
  Incremental root `all_qmllint` returns zero on both hosts; cached Wayland type generation was not rerun and its
  twelve earlier warnings are not claimed fixed.
  The first history scan flags two public source digests whose file names contain “tokens.” Both hashes recompute
  from the checked-in sources. Their exact lines are classified as nonsecrets using the existing rule/file/line
  policy; controls still flag a different digest, another field and another path. Existing commits are preserved.

- **Agent step fonts use the existing typed Application singleton.** The body startup font, tool mono face, literal
  text and model replacement pass five inert panel cases per QQ Release/ASan/UBSan/native Windows build, zero
  failures/skips. The same additive startup checks pass before and after the one-expression qualifier change;
  every pre-existing assertion remains. Native/Linux lint remove one old finding and add none, leaving 121 console/
  136 structured warnings and aggregate exit 1. Actual objects/programs/compiler flags and hardening 16/13 Linux/
  five native product headers are verified. An exploratory live-font-update expectation failed in both forms and
  was discarded because Qt declares this property constant; original tests were restored before adding the valid
  startup coverage. Those failed experiments remain recorded, and no live-update feature or full product pass is claimed.

- **The authorised push checkpoint refreshes its failures as well as passes.** Recovery data passes 69 Qt cases/
  32 functions per QQ Release/ASan/UBSan build, zero failures/skips. The actual Studio crash case still fails at its
  initialization helper before any crash/restore (2 passes/1 failure, exit 8); its sanitizer counterpart remains unrun.
  The current checkout-local Windows Store full suite still reports 613 passes/1 failure, zero skips, exit 8, although
  the same-program isolated many-file case passes. Earlier Access-denied diagnostics remain a lead, with no cause
  or source fix established. Windows Python tools pass 376 tests/56 skips and Linux 305/127, zero failures; skips
  remain skips. Signed catalogue, token, header and tracked-file credential checks pass. This is development progress,
  with required warning/build/hardware/isolation gates still unmet.

- **Group and Prefab state reads preserve mixed parent types.** Private structural readers retain strict boolean
  checks, null/undefined guards, numeric/view defaults and live nested bindings. New compatibility rows first pass on
  the unchanged implementation and then pass with identical test source. Fourteen Qt cases across seven functions
  pass per QQ Release/ASan/UBSan build, zero failures/skips, covering state edits/reparenting, serialization, group
  kinematics, gameplay and Studio editing. Seven native Windows node/data contract cases also pass, zero failures/skips.
  An initial native harness guard stopped before testing because it expected `/WX`; the actual `-WX` spelling is
  verified and accepted without changing the compiler or cache. Its failed wrapper result remains recorded.
  Windows/Linux lint remove eighteen old findings and add none, leaving 122 console/137 structured warnings and
  aggregate exit 1. Source/copied-QML/program records, freshly compiled Group/Prefab objects, actual sanitizer flags
  and hardening 16/13 Linux product/three native product headers pass. No full product, hardware or CI gate is cleared.

- **Scene's saved-state snapshot describes the existing GameStore.** One local cast exposes its `contents` method,
  preserving SceneIO's return interface, ownership, null guard and snapshot value. Ten Qt cases across four functions
  pass per QQ Release/ASan/UBSan build, zero failures/skips, including actual Player state retained across starts and
  a replay that leaves it unchanged. Native Windows/Linux lint remove one structured finding and add none, leaving
  140 console/155 structured warnings and aggregate exit 1. Fresh source/program/copied-QML records, actual sanitizer
  flags/libraries and hardening 16/13 pass; no full product, hardware or CI gate is cleared.

- **A Scene enforces its documented RunRecord slot.** An unrelated object is refused while the previous recorder
  remains; real RunRecord objects passed through the existing object interface and invalid-variant clearing still
  work. The new regression first fails on the previous implementation, then passes unchanged with the typed slot.
  Fifteen Qt cases across nine functions pass per QQ Release/ASan/UBSan build, zero failures/skips: serialization,
  stored-state/host-message/animation/silent/screen-input replay and the actual Player's record/replay/error exits.
  Game and Player public object interfaces remain unchanged. Native Windows and Linux lint remove exactly eleven
  Scene recorder findings and add none, leaving 140 console/156 structured warnings; the aggregate returns 1.
  Fresh records, 152 source/public-input hashes, four program identities per build, actual sanitizer flags/libraries
  and hardening 16/13 are verified. These focused software checks clear no full product, hardware or CI gate.

- **Six QQ lint findings are isolated to existing Qt metadata.** Minimal AudioListener/SpatialSound inputs fail
  against the installed Qt 6.12 SpatialAudio dependency description and byte-identical diagnostic copies. Adding
  only its missing QtQuick3D dependency in a temporary copy clears those inputs; an invalid property still fails.
  The actual Runtime response file loses exactly six known import/type findings and adds none, with both full-module
  commands still failing. SDK/response-file hashes are unchanged, and no diagnostic overlay is connected to a
  production check. That checkpoint's 142 console/167 structured warning gate stayed failed. No kit installation, licence
  action, runtime/hardware check or runner change occurred.

- **The Studio recovery refresh has an explicit partial result.** At `229f930`, six existing interface functions,
  including 30 hostile-copy rows, trust, two Studios and folder replacement, pass 37 Qt cases under ASan/UBSan, zero
  failures/skips. The original Release selection reports 37 passes/1 failure, zero skips: the real Studio's initialization
  reply is missing before the crash/restore exercise. An isolated same-program run reproduces it; that crash function
  remains unrun under sanitizers. Synthetic private clients connect but receive no reply with either software render
  loop; parent-debugger frames show a render wait, without proving its cause. Source/program/compiler/archive checks
  and 13 existing sanitizer program headers pass. The original failed selection stays failed, and no timeout or gate
  was loosened. Tests use private HOME/data/keyring and a hidden display with the GPU device masked.

- **The remaining Store layers are checked on native Windows.** Full ordinary-build legacy/Library/front-end/page
  suites pass 11/21/39/18 Qt cases and all 9/9/28/16 functions, zero failures/skips. The two Unix-only Library symlink
  rows are not compiled on Windows; the demonstration startup/engine branch and Linux compositor binary scan are
  outside the ordinary Windows build. Fresh records, 130 source/public-input hashes, actual compiler flags and five
  program identities are verified; hardening reads 24 program headers. The Library checks scene-path requests without
  launching the Player or scanning personal folders; the shell dependency is built and read, not started. Pages draw
  offscreen in software over private engines and nonpersistent simulated services. The separate checkout-local
  many-file Store rename failure remains unresolved, and no hardware/full-OS/isolation gate is cleared.

- **Windows root lint is checked at zero warnings.** The configured `all_qmllint` target runs seven root modules
  successfully with `MaxWarnings=0`, unchanged categories and severities. Fresh invocations, 112 source/config hashes
  and response files are verified. Six direct/generated-launcher controls preserve clean exit 0 and the raw Windows
  warning/syntax failure status 4294967295. These synthetic failures are expected controls. Products and the Linux-only
  compositor are outside this result: QQ remains at 149 console/174 structured warnings and Linux retains its
  12 Wayland metadata warnings; neither failed gate is cleared by the Windows root pass.

- **The Windows service boundary is checked natively.** The full existing suite passes 17 Qt cases and all 15
  functions, zero failures/skips, in 4.77 seconds. Its Windows branch reads the actual private named pipe's access-control
  list and requires every allowance to name this test process's user; identity values stay in memory. The nonpersistent
  simulator supplies synthetic network, media and settings with null audio and virtual controllers. Fresh records,
  53 source hashes, compiler commands and the rebuilt program are verified; hardening reads 21 program headers.
  This is neither another-user attack nor runner isolation/hardware proof. The separate checkout-local Store rename
  failure remains unresolved.

- **Native Windows Store checks retain a location-specific failure.** In a standard private Windows Temp root, the
  unfiltered format-2 suite passes 614 Qt cases and all 149 Windows functions, zero failures/skips/blacklists. The
  original checkout-local root passes 613 and fails the many-file install. The same unchanged case fails in two
  checkout roots and passes in private roots on C: and X: outside the checkout. Temporary diagnostics read Windows
  error 5 after the staging rename failed; the cause remains unresolved. That logging was restored byte-for-byte,
  with no retry, deadline, predicate, process or security-policy change. Fresh rebuilt-program records, 115 source/public
  input hashes, Windows CNG compiler commands and hardening (21 existing program headers) are verified. Four Unix-only
  functions are not compiled on Windows. The standard-temp pass does not clear the original failed gate or prove
  native hardware, full-build or isolation readiness.

- **The collector is checked on native Windows.** Existing commercial Qt 6.12/MSVC 19.44 builds the selected
  `ci-windows` target cleanly. Its hidden software suite passes all nine Qt cases/seven functions, zero failures/skips,
  covering directory-output exit 1, complete stale-report replacement, stalls and idle/thread behavior. Fresh records,
  ten source hashes, actual compiler flags and unchanged program identity are verified; hardening reads 21 existing
  program headers with none falling short. Two Linux-only failure cases are not compiled on Windows. The first
  dashboard invocation selected the source folder, exited 8 and ran no tests; its artifacts remain preserved. A
  test-only follow-up with the explicit build folder passes without rebuilding. No hardware or full-build gate is
  cleared, and no installation, licence, account or runner changes were needed.

- **The complete journey is refreshed after the Inspector batch.** At `f99e007`, one complete CTest per product build
  passes, zero failures/skips, in 126.656/114.105 seconds with its unchanged 300-second limit. Two Studio builds, three
  real Player launches, Prism Library discovery/request and playback after temporary-project removal run. Both apps
  rebuild cleanly after restoring an unsuccessful Shape context experiment. Fresh records, 26 source fingerprints,
  nine program identities, sanitizer commands/libraries and hardening (16/13 products) are verified. Native lint still
  fails at 149 console/174 structured warnings, unchanged against the retained Inspector snapshot; the combined wrapper
  exits 1. Prism remains an ordinary simulator with a recording stand-in, with separate actual Player playback;
  software results clear no native hardware or release gate.

- **Current Prism client-identity and lock-screen validation.** At `f92a6d2`, the existing `prism_untrusted` scenario
  passes once per main profile, zero failures/skips: all 50 steps and 17 expectations, in 19.902/19.457 seconds.
  Claimed application identities, an undrawn window, virtual-controller input and a private simulated lock screen retain
  their boundaries. Fresh records, 343 source fingerprints, six program identities per profile and unchanged settings
  are verified; the sanitizer compositor's actual compiler flags/libraries and hardening (31/29 programs) pass.
  Stand-in-only builds retain the existing compositors and their earlier 12-warning metadata gate failure. The actual
  registered command, 120-second CTest and 100-second driver guards are unchanged. Test HOME/runtime/data/cache folders
  are private; no host lock, account or real device is exercised. This remains software functional evidence.

- **Current Linux service boundary validation.** At `d52bd58`, the unfiltered Qt Remote Objects suite passes 17 Qt
  cases and all 15 functions per main profile, zero failures/skips/blacklists, in 4.600/4.708 seconds. It covers private
  local-socket permissions, source/replica updates, settings and simulated network/media behavior. Fresh records,
  52 unchanged source inputs, actual service/simulator/audio sanitizer commands and profile settings are verified;
  hardening remains 31/29 main-build programs. Tests use a nonpersistent simulator, private HOME/runtime directories,
  null audio and virtual controllers. The Windows permissions branch did not compile here. The wrapper exits 0 without
  build/licence diagnostics; existing QQ/Wayland/Rust, hardware and isolated-runner gates remain unmet.

- **Current C++ Storefronts boundary validation.** At `6a4d6a4`, the unfiltered ordinary-build suite passes 39 Qt cases
  and all 28 active functions per main profile, zero failures/skips/blacklists. It checks view separation, checked
  previews/licences, creator-switch races and absence of the demonstration key from the actual programs. Fresh records,
  118 unchanged source/public inputs, reader/frontend sanitizer commands and profile settings are verified; hardening
  passes 31/29 main-build programs. The sanitizer build's 12 Wayland metadata warnings exactly match the retained baseline,
  so its strict wrapper exits 1 before runtime. A separate test-only run uses the successful build, keeps those diagnostics
  and also exits 1 to retain the failed warning gate. Nothing is rebuilt to erase warnings. Demonstration-enabled startup
  is a different compile branch and did not run here; no owner signing keys, fixture regeneration or hardware proof is involved.

- **The complete journey is refreshed after the Shape batch.** At `d111bdc`, one complete native CTest passes per
  Release/ASan product build, zero failures/skips, in 112.977/112.875 seconds under the unchanged 300-second deadline.
  Two Studio builds, three real Player launches, Prism Library discovery/request and playback after temporary-project
  removal all run. Fresh native records, 22 unchanged source fingerprints and nine program identities are verified.
  Prism remains a non-instrumented simulator using a recording stand-in; real Player playback runs separately.
  Memory high/maximum/OOM events are zero for this job. No hardware, isolated-runner or zero-warning gate is cleared.

- **Current Player/Creator Store pages validation.** At `e5a67a4`, the complete Storefronts QML file passes all
  18 Qt cases and 16 functions per main profile, zero failures/skips/blacklists. It covers view separation, literal licence
  and provenance text, preview recovery/decode sizing, creator-toggle retention and updates. Fresh records, 116 unchanged
  source/public inputs, Storefronts/parser sanitizer commands and profile settings are verified; hardening passes 30/28
  main-build programs. Runs use documented offscreen software graphics, silent nonpersistent simulated services, temporary
  Store data and test-generated signing keys. No production change, owner private-key access or fixture regeneration
  is involved; full OS/hardware and warning gates remain unmet.
- **Current Store-backed Library boundary validation.** At `2d290af`, all 23 Qt cases and 9 ordinary functions pass
  unfiltered in each main profile, zero failures/skips/blacklists. Changed installed files, escaping/cross-game scene
  records, Creator visibility and non-game entries retain their refusals. Fresh records, 116 unchanged source/public-input
  fingerprints and actual Store-reader/Library sanitizer commands are verified; hardening passes 29/27 main-build programs.
  These check listing/playable-path metadata over temporary signed test data, not a Player launch. No production change,
  owner private-key access or fixture regeneration is involved; full OS/hardware and warning gates remain unmet.
- **Current legacy Store compatibility validation.** At `6dc80cf`, the unfiltered legacy suite passes all 11 Qt cases
  and 9 ordinary functions per main profile, zero failures/skips/blacklists, in 0.79/0.99 seconds. Fresh records and
  113 unchanged source/public-input fingerprints are verified; sanitizer reader flags/linkage and hardening (28/26
  main-build programs) pass. The suite uses pre-signed public fixtures and temporary install data. No fixture regeneration,
  owner signing-key access or production change is involved; full OS/hardware and warning gates remain unmet.
- **Current Store format-2 security validation.** At `80de855`, all 621 Qt cases and 153 ordinary test functions pass
  unfiltered in each main profile, zero failures/skips/blacklists. Fresh records, 112 unchanged source/public-fixture
  fingerprints, sanitizer flags on all six parser/engine readers, library linkage and unchanged profile settings are verified.
  Normal/sanitizer runs take 33.59/78.32 seconds; hardening passes 28/26 main-build programs. The job exits 0 without
  build/licence-failure diagnostics. Application builds use GCC 15.2/CMake 4.2.3; Qt's GCC 11.5 string describes its library.
  Test-generated keys and localhost data remain separate from owner keys. No fixture regeneration, core/OS/hardware
  readiness or zero-warning claim is made; the separate pinned-core Rust warning and known lint gates remain failed.
- **QQ imported-model coverage.** Two focused real-window tests cover Game startup and a pick box across empty,
  valid, cleared, failed and repeated loads, with the reported undefined-vector diagnostic made fatal. Alongside existing
  animation and Studio selection controls, 5 Play/3 Studio Qt cases pass per Release/ASan build, zero failures/skips.
  Fresh records/source fingerprints and hardening (16/13 product programs) are verified. The reported warning did not
  reproduce; production QML is unchanged. This adds coverage and does not claim a production fix or clear the lint gate.
- **Fresh simulator/device restrictions and selected core bridge.** At `af8fc43`, the unfiltered command-line tests pass
  10 simulator/11 device-option Qt cases in each existing main profile; the pinned core's C interface passes 9 cases in
  `ci-linux`, all zero failures/skips. Fresh records/source fingerprints, unchanged settings, sanitizer instrumentation
  and hardening (27 normal/25 sanitizer programs) are verified. The sanitizer profile intentionally keeps the core off.
  The wrapper still exits 1 for the same `proc-macro-error2` 2.0.1 future-compatibility warning in the pinned core.
  An earlier private-home build's Qt licence lookup failure is retained; recompilation with the normal tool environment
  has no licence-failure diagnostics, without bypass/registration/settings changes. Full OS/Rust and hardware gates remain unmet.
- **Current first-journey validation.** At `3421d35`, one complete journey passes with Release QQ and one with
  ASan/UBSan QQ, zero failures/skips, in 113.07/113.31 seconds within the unchanged 300-second deadline. Both check
  two builds, three real Player launches, current Prism Library discovery/request and playback after removing the
  temporary project. Fresh records, source hashes and nine program identities are verified. Prism's recording stand-in
  and the separately run real Player remain distinct; Root Prism is not sanitizer-instrumented. Its selected strict build
  uses the core-enabled profile; Prism itself does not link the core. Hardening passes 25 programs, but the wrapper
  correctly exits 1 for 12 unchanged
  Wayland metadata warnings. Zero-warning, full-OS and native hardware readiness remain unproved.
- **Prism takes what only tests need only in a build with the simulator.** `--script`, `--unlocked`, `--screenshot`, `--delay`, `--benchmark` and `--benchmark-out` are no options in a Prism built without `QOR_BUILD_SIMULATOR`, as a device's image is: asking for one is an error before anything is read, and the code that would run them is not built in. `--unlocked` and `--benchmark` had started a device's Prism past its lock screen with nothing asked. `--surface`, `--size`, `--outputs`, `--socket` and `--spread-clients` stay. `tst_prismoptions` and `tst_prismoptions_device` build the command line both ways, and SECURITY.md no longer lists `--script` among its known limits.
- **A game can capture the mouse in Prism.** `zwp_pointer_constraints_v1` and `zwp_relative_pointer_manager_v1`, as QOR-0020 decided: only the active window with no shell surface open, with the keyboard, the host's focus and the pointer on the very surface that asks, after a person's action on it. `Super+Esc` (whatever else is held), the lock screen and any shell surface end it. A persistent capture comes back only on a mouse button pressed on it, never on a key. No client hears the pointer while a shell surface is open, nor over a notification, toast, the volume or anything else of the shell's lying over its window. Nested, the host's pointer is not held and there is one kind of delta; the rest waits for the KMS backend ([SHELL.md](docs/design/SHELL.md#capturing-the-pointer)). `tst_pointercapture` tests it from a hostile client's side: 40 cases under `linux-asan`, offscreen, in software, after an independent review; not yet on real hardware or with gamescope.
- **The QOR OS Store's catalogue format 2** ([STORE.md](docs/STORE.md#format-2-the-two-storefronts), [QOR-0021](docs/decisions/QOR-0021-projects-engines-and-two-stores.md), decisions 5 to 8). Each item is typed: its kind, where it is placed in the Player Store or the Creator Store and under which category, `entitlement` (free is the only kind), its platforms, engines, dependencies, licence, provenance and previews, and an optional edition. The catalogue carries a sequence and an expiry. `price` is refused anywhere, even when it is zero, and format 1's unchecked `price` is closed. An item that is unclassified or contradictory stays a draft: it is in no listing, and install refuses it with a reason a tool can read, while the rest of the catalogue loads. At this stage only `game` and `asset-pack` install. Every query names its view (Player or Creator), with no default and no fallback. With creator features off, the Creator view answers nothing and install refuses its items from C++, and turning them off removes nothing. There are no pages for it yet.
- **The Store's package checks.** The Store reads archives itself, stored entries only, in place of Qt's reader: no compression, no links or folders, every entry's header must agree with the directory, and at most 32 MiB unpacked. Each kind has an allowlist checked by extension and by first bytes. A model may name only files beside it, and may carry only pictures and buffers as data. A text file or licence may hold no markup: no tag that Qt or a browser knows, and no tag that carries a value, whatever its name. Angle brackets around plain words, an address or a link, as licences have them, still pass, and the GPL, LGPL, MPL, Apache, MIT and BSD texts are tested. It is one linear pass. A format 2 manifest is closed and mirrors its listing.
- **A Store install is all or nothing**, with its dependencies, and so is a removal. Every package is downloaded and verified first. Then, under the store's lock, the engine reads what is on disk again, checks again, writes a journal, swaps the new versions in and writes `installed.json` last. A crash at any moment recovers to before or after. A listing older than what is installed is refused before anything moves. Something already broken blocks only what depends on it. Another program sharing the folder keeps its progress and its records, and replay memory is kept separately for each store key. Putting back or clearing up may itself be cut short and run again over the same journal, and comes to the same end. On Linux and macOS each step is made durable before the next. What installs under way hold in memory is bounded at 128 MB for all of them together. The store's lock is never taken from a holder that is alive. A journal is believed only for names of the engine's own shapes inside `items/`, and clearing up never deletes through a link. If the record of what is installed is unreadable or gone, what each installed package's manifest claims stands in for it, so an id keeps its version, developer and kind. A second recheck's findings are fixed, each with a test proven by a planted fault. A rollback or removal whose delete fails partway (a file held by a scanner) now stops and keeps its journal, and the next engine finishes the job. An old version set aside is deleted only once what stands in its place holds exactly what its manifest lists, so the only good copy is never lost. A record is written before a first install's journal, so a record found missing beside a journal was lost, and what is on disk then decides; a first install that had been committed is no longer deleted. Text is read for markup as Qt's own parser reads a tag, where a quoted word holding `>` does not end it, still in one pass. A package's download counts against the 128 MB that installs under way may hold together, from its first byte. Entries rebuilt from manifests are marked `"claimed"` in `installed.json` until the item is installed again. Clearing up walks folders without recursion. There is still no power-cut test, and Windows junctions are untested.
- **A demonstration Store catalogue** (`tests/store/fixtures-demo/`, built by `build_store.py demo`), signed with a throwaway key that is dropped once used, and never in a release. The fixtures in use today are not re-signed. `tst_store_format2` runs 604 cases under `linux-asan` at `811d192`, and 597 of them built with MSVC (the seven others are Unix-only). Each check is proven by a fault planted against it when it was written; where two faults hide each other they are caught together.
- **The Store's two storefronts, in a demonstration build.** A second Store engine takes the format 2 catalogue, with its own folder and key, only in a build that asks for it with `QOR_STORE_DEMONSTRATION`, from the catalogue address in `QOR_STORE_DEMONSTRATION_URL`. An ordinary build looks as it did.
  - **It cannot reach a release.** Every preset other than the two demonstrations turns the option off, so a folder once configured with it by hand is turned back off. It is refused without the simulator, nothing built with it installs, and `tools/ci/check_hardening.py` fails a build folder whose cache has it, or whose programs hold its key.
  - **The pages.** The Player Store's pages find, show and install games, with their licence and where they came from as plain text. Settings › Store has a creator-features switch, off until set. Turned on, it opens the Creator Store's pages, where a pack installs and updates. Turned off, it closes them, what is installed stays, and nothing is told what only the Creator Store lists. Every page asks of one named view.
  - **What it holds.** Previews come only as checked bytes, four at a time, decoded at the size they are drawn, and held within a bound that counts them as they are held. Reading a licence checks its package as an install would (about 0.3 s on the window's thread for 32 MB), one download at a time per item, stopped when it is no longer wanted, and counted against the 128 MB every install under way shares. What was fetched is kept through Check for Updates when the catalogue is the same.
  - **Tests.** `tst_storefronts`, `tst_storefronts_qml`, `shell_e2e_store`, `shell_e2e_quiet`, `demonstration_does_not_install`, `test_check_hardening.py`, and `tst_store_format2` at 606 cases, each check proven by a planted fault, under `linux-asan` with the demonstration on and off, at `1892e30`, after an independent review, in software. Playing a Store game, and Store games in the Library, are not done yet.
- **Games from the Store play from the Library, in a demonstration build.** An installed Store game is in Home's Library, from the Store, by the name its developer signed. It is listed from the Store engine's own record, never by reading a folder for games, and only while the Player Store lists it (or the Creator Store with creator features on). As it is played it is checked again, whole: every file as signed, nothing beside them, no links. The QQ Player then starts on the scene its own `game.json` names, as a QQ Studio build writes it. A changed game, one with no `game.json`, or one naming a scene outside its `scenes/` does not play, and says why in plain words. It plays behind the Player's usual walls, trusted no more than a game built here. The demonstration's Ember Climb now carries `game.json`, and the demonstration was signed again for it, with new throwaway keys. An ordinary build is unchanged. Tests: `tst_library_store` from the hostile side under `linux-asan`, `prism_store_play` (Prism, with a stand-in for the Player), and `shell_e2e_store`, at `214ca91`, in software. QQ's own `tst_storegame` lays the demonstration's Ember Climb out from its signed package as the Store installs it, and the real QQ Player plays it behind its walls, from the scene its `game.json` names to its first frame, and quits cleanly (`2fbfc2a`, in software).
- **QQ Studio's asset library.** In a demonstration build the Studio browses installed asset packs, copies chosen files with their licence and notice into a trusted project, and records their origin in `assets.lock.json`; a built game carries what it uses. The review fixes keep crash recovery from moving files it cannot account for: journals name the project folder and placed digests, missing staging moves nothing, and each lock has its own change tag. Changed copies are checked again just before replacement; a build and an asset change in Studios sharing the person's own store exclude each other. Update asks for trust, external text stays plain, and thumbnails decode checked bytes with bounded dimensions. Use and Update run off the window's thread; Save, Build and changing project wait. Listing and planning still run on the window's thread. Listing provenance is kept only when the Store marks it as coming from its signed listing, each field capped at 8,192 characters; the total lock cap stays 8 MiB, with an oversized change refused before moving files. The Studio says it has not checked the developer's signature. Latest Release and matching AddressSanitizer/UBSan checks at Qt inputs `6046e08` each pass 89 asset-library, 195 game-build and 25 Studio cases without failures/skips, including the opt-in pack measurement and real StoreEngine demonstration pack. Leak checking is off. Fresh hardening passes 16 Release/8 sanitizer programs. These run hidden under WSL in software. At that checkpoint QQ lint reported 347 diagnostics and failed its native zero-warning gate. Native Linux, a GPU and Windows remain unverified for these changes.
- **The Store keeps where an item came from.** At install and at each update, a format 2 listing's provenance (origin, author, source, permission) is kept in its `installed.json` entry, marked `"provenanceFrom": "listing"`: the catalogue's word, signed by the store's key. It is believed on reading only so marked and shaped, and capped as the catalogue caps it. A record rebuilt from the folders says `"provenanceFrom": "none"`. QQ Studio's asset library reads it into a project's `assets.lock.json`, so a pack from the Store shows and records the provenance its listing gives (`tst_store_format2` 621 cases under `linux-asan`, and `tst_assetlibrary`'s walk with the real Store, at `8b77c01`).
- **A clean VM for Linux CI jobs, written and not yet run.** `tools/ci/isolated-vm.ps1` makes `qor-ci-vm`, a generation 2 Hyper-V VM with its own kernel, from Ubuntu 24.04's cloud image pinned by SHA-256. It has Secure Boot, every integration service off, no checkpoints, and its own switch and NAT behind a port ACL and two firewall rules. `-Action Prove` attacks it from inside and exits 0 only if every line passed. After an independent review its elevated run trusts nothing an ordinary process can edit: `-Root` is locked to administrators, what Remove deletes comes from an administrator-only record in `%ProgramData%`, no path is followed through a link, the image is hashed on the same handle it is converted from, and Create and Remove require `pwsh -NoProfile`. A half-made VM is recoverable, Stop no longer calls a silent guest stopped, and Prove reports lines that test nothing as notes. After a second review its elevated runs check who made a folder before trusting it: the record's folder and the root are made already owned by Administrators and locked, one that is there already is used only if it is owned by Administrators or SYSTEM, writable by nobody else and cannot be moved from under it, the record is read only after that check, and Remove deletes only what passes it. The test stand-ins refuse any folder outside the test's own. Nothing in it has run against Hyper-V. Its first run is the owner's, as administrator ([CI.md](docs/CI.md#a-clean-vm-for-linux-jobs-not-yet-made)).
- **QQ Studio builds a game, and the Library plays it.** **Build** brings in what the scene names from outside the project, saves, checks that the project's scenes, logic and prefabs build (at most 2,000 QML files of 16 MB each, since the window waits for it), and copies them with its assets into a folder of the game's own, with a manifest. Play runs the project as it stands; the Library lists the copy beside the project and plays the copy. The build refuses, with the file and the reason, what would leave a game incomplete that it can see: a link or a pipe, an unsafe name, an unreadable folder, compiled QML, too much, or anything the game would not hold that a scene, a logic file (read from its own folder and the scene's, as `play()` and `spawn()` read it), a glTF or OBJ model, an OBJ material library, or a Tiled map or tileset names. That is a scan of the text, so words that look like a path may be taken for one, and the reason says so. It does not see a path a game puts together as it runs, a tileset saved as `.json`, or what other model formats name: a game that needs one finds it missing only when it plays. One build of a game runs at a time, and a build killed at any step loses no game, though a power cut can still leave files short behind a whole `game.json`. On Linux a file swapped after the build looked is not followed. While a build runs, the Studio and its agent change nothing and the Studio stays in its project. `qq-studio --project <folder> --build <scene>` builds only a project already trusted, asks that of the real folder it copies, answers on standard output that it compiled nothing, and accepts `--build=`. The Player keeps Qt's QML disk cache off whatever the environment asks, and reports what Qt does; the Studio uses only the cache compiled into it. Shown in software under WSL at `72945fe`, after an independent review and its recheck; not yet on native Linux, a GPU or Windows.
- **Unsaved work in QQ Studio survives a crash and a launcher's close.** Once editing pauses it is copied to the person's own store, never into the project, and offered back as Restore or Discard on arrival, past the trust check; the question says if a file changed since the copy was made. Each Studio keeps its own copy, locked while it runs, and keeps writing it while an older copy waits for an answer, so nothing typed meanwhile goes unkept and two Studios on one project never write over or remove each other's; a copy that cannot be kept is said. A copy records which folder it was made for, so another project later put in the same place is not offered it; a project moved or renamed finds its copies again only when moved back. Shown with the real program killed by SIGKILL and with a stand-in launcher closing it. The camera is remembered per project.
- **Project workspaces in QQ Studio.** Each project comes back as it was left, across a switch and a restart: its open scene, its open logic file, its panels and the window. What is kept is the person's, outside every project (`<AppDataLocation>/workspaces/<SHA-256 of the project's real path>.json`, written whole or not at all), and is read back as a stranger's file would be: size capped, version and folder checked, paths that climb out or leave through a link dropped, and windows fitted to the screens there are. `switchProject()` is the one way to change project, from **Change…** and the new **Recent** list: it asks Save, Discard or Cancel about unsaved work, and Cancel leaves everything as it was, a playing scene included; nothing of the old project stays; and its files open only past the existing trust check. `--project` always wins; with nothing on the command line the Studio goes back to the last project; a capture keeps nothing. `tst_workspace` drives the real Studio and the real program, from the hostile side too, and passes under AddressSanitizer and UBSan, in software on Linux; it has not run on Windows or a GPU.
- **QOR OS**, the private repository `QOR-MATRIX/qor-os`, seeded with QTOS's full history (36 commits and its tags). QTOS is frozen: its code at `37b621e`, with a note pointing here (`3ecdded`).
- **QQ, ARQADE and QOR Media**, carried from demiurge-chain (`28d9e97`) with their history by `git subtree`, at the same paths as upstream: `products/qq/`, `products/arqade/qt/`, `products/arqade/games/`, `products/arqade/tests/fixtures/` and `tools/qor-media/`, with `docs/design/tokens.json` copied at the same commit. All three build here and pass their suites: QQ 8 suites, ARQADE 4, QOR Media 9.
  - `tools/dev/products.ps1` builds and tests them with their own scripts, and tries a failing product once more.
  - `tools/upstream/demiurge.sh` shows their upstream state, pulls from demiurge-chain, and writes patches for upstream ([docs/UPSTREAM.md](docs/UPSTREAM.md)).
- **The QOR core**, built from demiurge-chain at a pinned commit through Corrosion (`core/`, `qor::core`, `QOR_BUILD_CORE`, `QOR_CORE_SOURCE_DIR`), never copied ([QOR-0003](docs/decisions/QOR-0003-the-qor-core-at-a-pinned-commit.md)). `tst_qorcore` calls it through its C bridge: the core's version, CGT's units, amounts read exactly, the largest amount never rounded, refusals as errors, and no command that reads a key.
  - The pin moved from `28d9e97` to `e99c7ce`, the merge of demiurge-chain's L13.2 (#107), with the `desktop` feature: QOR ID's and Social's clients, the chain client, the Library's index, the Mesh and Qontrol are now in the core QOR OS builds.
  - On 28 October it moved to `6a4822d`, the merge of demiurge-chain #109: the same code, with the core's manifest declaring Apache-2.0 instead of MIT (demiurge-chain's ADR-122).
  - The pin (`QOR_CORE_COMMIT`) and features (`QOR_CORE_FEATURES`) are written on every configure instead of cached, so a build folder, CI's included, follows a moved pin.
- **One rulebook and one decision log.** `AGENTS.md` (with `CLAUDE.md` pointing to it) merges Demiurge's rules on checking claims, money, language and secrets with QTOS's build and CI practice. `docs/decisions/` holds QOR-0001 to QOR-0009, and QTOS's records as QTOS-0001 to QTOS-0010.
- `OWNER.md`: where QOR OS stands, on one page, rewritten every session.
- `tools/ci/runners.ps1` moves the workstation's two CI runners to the organisation or a repository and starts them. It leaves a runner already in place alone and never stops one that is running a job. Both runners serve the organisation since 27 October.
- `LICENSES/Apache-2.0.txt` and the carried paths in `THIRD_PARTY_NOTICES.md` and `LICENSE`.
- **The carried products build and test on Linux** (Phase 1, begun 28 October). `tools/dev/products.sh` is the Linux counterpart of `products.ps1`: it configures QQ, ARQADE and QOR Media with CMake and Ninja against the `gcc_64` kit, builds them, and runs the suites each registers with CTest, running failed suites once more. It fetches PDFium's pinned release for Linux and checks its SHA-256. **Under WSL it fences the run from the GPU**: three runs there that drew on it through Direct3D 12 hung the workstation on 28 October, each with a suite stuck in the driver where ctest's timeout could not reach it. The script runs itself again in a mount namespace where `/dev/dxg`, WSL's GPU device, is `/dev/null`, so Mesa draws in software and nothing reaches the GPU, and with systemd's user manager in a scope held to half the processors and three quarters of WSL's memory. In software QQ's walk and the QQ Player's frame-rate floor fail; `--gpu` draws on the GPU as before. The suites run on a display of the script's own, `tools/dev/products-display.qml`: a compositor on Qt's offscreen platform that keeps each window at its own size and gives the newest the keyboard, with no X display and no sound. No window or sound reaches the desktop, and nothing there can take a test window's keyboard. `--display prism` runs them inside Prism, as hidden; `--display session` on the desktop. Linux CI runs it when the carried code changes, as Windows CI runs `products.ps1`, but only where `QOR_RUN_LINUX_PRODUCTS` is set, which it is not: the Linux runner shares that workstation.
  - The Linux kit gains Qt Quick 3D Physics, which every QQ scene imports, and Qt PDF, which QOR Media links; both are in `.github/qt-packages.json`.
  - **QQ Studio keeps the creator's provider key in the Secret Service on Linux** ([QOR-0011](docs/decisions/QOR-0011-the-creators-provider-key-in-the-secret-service.md)), over Qt D-Bus, as it keeps it in Credential Manager on Windows: an item in the default collection, unlocked through the keyring's prompt. `AGENTS.md` §6 says a creator's own key for an outside provider is not a QOR key. In the hidden modes `products.sh` gives the run a session bus of its own with `tools/dev/secret-service.py`, an in-memory Secret Service (tested by `tests/tools/test_secret_service.py`), so the suites never reach a real keyring. `--tests <regex>` runs only the matching suites.
  - The run's display gives the keyboard to the window below one that closes only once the closed window's surface is gone. Moving it while the surface was being destroyed crashed Qt Wayland Compositor, whenever a window closed with another still open, as in QOR Media's reader suite, and left the suites after it with no display.
  - On Wayland the products' windows stay toplevels and Prism places them ([QOR-0010](docs/decisions/QOR-0010-on-wayland-prism-places-the-products-windows.md)); QOR Media's docking case is skipped there and still runs on Windows.
- **`docs/release/`**, the owner's pack for the six release gates, linked from ROADMAP.md's gates, which stay the only status: the letter to The Qt Company, a trademark knockout brief for an attorney, a brief for counsel on the terms, the privacy policy and the legal opinion, code signing and the Secure Boot key, an RFP for the external security review, and a runbook for replacing the 5 October key. It is research, not legal advice, and ticks no gate. `THIRD_PARTY_NOTICES.md` now says what licence the core's crate declares at the pin.
- **[QOR-0014](docs/decisions/QOR-0014-qor-os-does-not-use-entire.md): QOR OS does not use Entire.** On 29 October the owner first chose to mirror on Entire, the Git forge launched on 26 October 2026 (QOR-0012), then allowed its app on every repository (QOR-0013), then took the assistant's recommendation to drop it: it duplicated GitHub, offered no CI the licensed kit could use, and had write access to this private repository. GitHub alone, as before; the app is removed from the organisation. demiurge-chain's record is its ADR-125.
- **Prism hosts ARQADE and QOR Media** (Phase 1, [QOR-0010](docs/decisions/QOR-0010-on-wayland-prism-places-the-products-windows.md)). `ProductHost` (`prism/shell/producthost.h`) is Prism's side of the QOR host protocol, version 1, to the contract of demiurge-chain's QOR Launcher.
  - **Starting a product.** Prism runs it as a Wayland client of its own, with `--launcher-channel`, the person's Motion setting, a trusted xdg-activation token, and a one-time token on its stdin only.
  - **Its channel.** A local socket in a folder only this user can enter, which takes one connection, from the process Prism started as the kernel reports it. The hello must carry the token within 30 seconds, compared in constant time and used once.
  - **What Prism offers.** Only the product's long poll, which it answers with `place` (the content rectangle Prism planned, no parent), `open`, `prefs` or `quit`.
  - **Its window.** Prism knows a product's window by the client's process, names it and gives it its icon from its own record, and opens it at the planned frame.
  - **In the shell.** ARQADE is in Beam where it is installed (`QOR_ARQADE`, else beside Prism). QOR Media opens a file through `Desktop.openInMedia()`, for the Library. Closing a product's window ends it, and a failed product shows a toast. A product's channel closes when Prism ends, and the product quits; on Linux it is also sent SIGTERM if Prism dies first.
  - **Tests.** `tst_producthost` covers the protocol: the handshake, who may connect, the long poll, a real child process with its token on stdin only, and no token or session id at any log level. It passes on Windows and Linux. `prism_products` runs Prism offscreen, in software, with stand-ins for both products (`tools/dev/prism-products.py`), so Linux CI runs it with no display.
- **Prism hosts QQ Studio and the QQ Player, and every window opens at the size its client asks for** (Phase 1, 30 October).
  - **Started, with no channel.** `ProductHost` has records for QQ Studio (`qq-studio`, found through `QOR_QQ_STUDIO`, offered in Beam) and the QQ Player (`qq-player`, `QOR_QQ_PLAYER`), and starts them with `launch()`: no `--launcher-channel`, an empty stdin, and the file as an argument (`--project <folder>`, `-- <scene>`). QQ Studio's channel is for publishing, which waits for the vault service (Phase 2); started without one, the Studio says publishing needs the launcher. `Desktop.playQQGame(scene)` plays a game in the Player, one at a time: asked for another, it brings the one playing forward and says it is already open. Stopping a product, or Prism ending, asks one with no channel to end at once, and an end Prism asked for is not reported as a failure.
  - **Windows at their own size.** Prism used to configure every client it did not place to a 960 × 640 frame. Its first configure now has no size, so the client chooses; the window stays unseen until the client's first commit says what it chose, then Prism plans the frame around it, cascaded and within the work area (`WindowLayer.planFrame()`), configures the client to fit, and the window flies in. A client that answers configures only when it redraws, as weston-terminal does, is not waited on, since an unseen window gets no frame callbacks.
  - **Fixed: a window closed while minimised was never removed.** Nothing was left to animate, so its `closed` signal never came, and the window and its surface item stayed in the window list. A window awaiting its size would have done the same. Such a window now goes at once.
  - **`products.sh --display prism`.** QQ's suites, run inside Prism on 30 October, failed only the cases that time movement in software.
  - **Benchmarked** before and after (`docs/PERFORMANCE.md`): `qor-shell --benchmark 8`, three runs of each build, medians 2.78 and 2.79 ms a frame, p95 3.03 and 3.02 ms, the same within noise.
  - **Tests.** `tst_producthost` gains five cases for products with no channel. `prism_products` adds stand-ins for QQ Studio, which asks for more than the screen holds and is fitted to the work area, and the QQ Player, which opens at its own 1280 × 720, plays one game at a time, and is closed while minimised. The script's `expect` paths can now index an array or take its `length`.
- **QQ plays with QOR OS's controllers** (Phase 1, 30 October; [CONTROLLERS.md](docs/CONTROLLERS.md#games)).
  - **`qor_gamepads`** (`input/gamepads/`) is QOR.Input's backends and `GamepadManager` on their own, with Qt Core and Gui only; `qor_input` links it, and so does QQ's runtime, which adds it when built on its own.
  - **QQ's `Input` reads it** in place of XInput: every controller it reports, combined, with its buttons by QQ's names (Guide is never a game's). So QQ, and ARQADE's games on QQ's runtime, take DualShock 4 and DualSense on Windows too, and evdev controllers on Linux. Its sticks arrive with QOR.Input's dead zone out, and QQ no longer takes out its own on top. A controller is heard only by the application in front.
  - **In Prism**, a product whose record `takesController` (the QQ Player) has the controller while its window has the focus and nothing of the shell is open: the controller interface steps aside with its cursor and hints, Guide brings Overview, and a controller connected then is not asked about.
  - **Tests.** `tst_play` plays a virtual DualSense through QOR.Input: walking and a jump, one dead zone, nothing heard while another application is in front, the buttons by name, Guide never a game's; it uses only virtual controllers now. `prism_products` connects a virtual DualSense: asked about with QQ Studio in front, the interface stepping aside for the QQ Player and coming back with Guide, and not asked about while the Player is in front.
- **ARQADE and QOR Media answer the controller** (Phase 1, 30 October; [CONTROLLERS.md](docs/CONTROLLERS.md#games)).
  - **ARQADE takes the controller in Prism** (its record's `takesController`), as the QQ Player does. Its games read the controller themselves, so with the controller interface answering as well, each d-pad press also arrived as an arrow key, and East as Esc, which put the game away: in Rift Survivor, B, the third perk, would also have left the game.
  - **ARQADE's home** answers the d-pad and the arrow keys with QQ's `Input`, in actions defined only while the home shows, so no game hears them or keeps them in its run. Each press takes the focus ring to the nearest card, button or tab that way (one level with it first), held it repeats, and the page scrolls to keep the focus in view. A or Return presses as it comes up, so the game a card starts never takes the press as its own first. A controller's Back puts a game away, read from the controller service by `ControllerBack` (`products/arqade/qt/src/controllerback.h`), not as an action the game would hear; back on the home the focus is on that game's card.
  - **QOR Media** keeps the controller interface, which gives a window the d-pad as the arrows, A as Return and East as Esc. Return on what is open takes the keyboard to its controls (the player's Play, or the top bar's way back), where the arrows go from one control to the next as Tab does, the volume keeping its own Left and Right, and Esc comes back to what is open; the Player's `release()` is now `rest()`, as the Reader's is. In the Reader, Return counts only on the pages: a text field passes on the Return it took, and the page's number would otherwise have sent the keyboard to the bar.
  - **Tests.** `tst_games` chooses a game by the keys and a virtual pad, plays it by A and by Return, and puts it away by a virtual controller's Back and by Esc, the focus back on its card each time, with no warning from the game. `tst_playback` and `tst_reader` reach the controls by Return, go along them by the arrows, and come back by Esc; the Reader's page number keeps the keyboard on the pages. Two reader cases that ended with Esc on a bar's button Tab had reached now see it come back to the pages, and close with a second Esc. A button the mouse focused is not one Esc comes back from. `prism_products` checks that ARQADE in front takes the controller and QOR Media does not.
  - **Not benchmarked:** the shell's only change is ARQADE's record, read when a product's window has the focus, and the benchmark's tour opens none.
- **The products wear Lumen** (Phase 1, 30 October; [QOR-0016](docs/decisions/QOR-0016-the-products-wear-lumen.md)).
  - **One mapping, in Lumen's source.** `kit/tokens/lumen.tokens.json` gains `products.colors`: each of the 21 colours of Demiurge's one look and the Lumen role it takes. The generator writes it into `Tokens.products` and a table in `docs/design/tokens.md`.
  - **Prism gives the colours.** `Theme.productColors` resolves the roles dark, whatever the shell's appearance, with the person's accent, tint and contrast as set. ProductHost passes them to every product as it starts (`QOR_THEME_COLORS`), and sends them to ARQADE and QOR Media as they connect and whenever they change (`theme`, which Prism had listed but never sent).
  - **The products take them.** ARQADE takes every colour, the gilt and the status colours included; QOR Media takes its on-accent ink too; QQ's `Look` takes them as the runtime starts, for QQ Studio, the QQ Player and ARQADE's games' screens. Outside QOR OS they keep Demiurge's look.
  - **Readable in both looks.** A lit primary button in ARQADE and QOR Media keeps its fill and lights its edge, since Lumen's ink on the accent is white. ARQADE's core halo is drawn white (`make_light.py`) and tinted by the accent, exactly the ember it was in Demiurge's look.
  - **Tests.** `test_generate_tokens.py`: the mapping is every colour of Demiurge's look, and its inks read on its grounds at every accent and contrast. `tst_producthost`: the colours reach a product as it starts and as they change, and one with no channel. `tst_interface` (ARQADE): every colour sent is worn, and the lit primary's ink reads. `tst_playback` (QOR Media) and `tst_scene` (QQ): the on-accent ink, and a host's colours at start.
  - **Benchmarked** before and after, hidden on Linux ([PERFORMANCE.md](docs/PERFORMANCE.md)): frame times the same, 7.35 and 7.36 ms average.
- **Home and the Library** (Phase 1, 30 October; [SHELL.md](docs/design/SHELL.md#home-and-the-library-homeqml-homeshelfqml-homecardqml-libraryh)).
  - **Home** (`Home.qml`, `HomeShelf.qml`, `HomeCard.qml`) is a new surface, opened by Guide (and closed by it again) or Beam's Home. It has three rows: Continue (what was played or opened last, a film's progress on its card), Games, and the newest Media. **The Library**, its second page, shows everything by Everything, Games, ARQADE, Made by you or Media. Cards take their art from their hue (Kit's `Artwork`) until they have their own.
  - **By controller:** the d-pad goes card to card, Up and Down a row at a time; South plays a game or opens a file; LB and RB go between Home and the Library; East closes; the hints show those buttons. The keyboard does the same with the arrows, Return and Esc. Every arrow is taken, so the controller interface never falls back to Tab there.
  - **The Library** (`qor::Library`, `prism/shell/library.h`) finds, on a worker thread:
    - the QQ games QOR OS carries, in `QOR_GAMES`, else `share/qor-os/games` beside Prism and in the data folder;
    - the person's own QQ projects in Documents;
    - ARQADE's six games while ARQADE can start;
    - video, music and pictures in the person's folders.

    It keeps what was played or opened, and where QOR Media last stopped, in `library-history.json`.
  - **ProductHost:** `play()` asks ARQADE for one of its games (the `play` command, which Prism had listed but never sent); `open()` carries where a film or song resumes; and QOR Media's reports, passed over until now, reach the Library (`reported`), checked for their type and size.
  - **Beam** lists every game of the Library and the actions Home and Library. `--surface home` or `library` opens them at start.
  - **Tests.** `tst_library` (new): games found by their scenes, media newest first and by kind, ARQADE's six while it can start, what was played remembered across runs, malformed reports kept out, and a refresh on the worker. `tst_producthost`: a game played, a file opened where it resumes, a poll's reports passed on. `prism_products`: the Library knows a QQ game, ARQADE's six and a video; a virtual controller opens Home by Guide, goes to the Library and back by RB and LB, and plays ARQADE's first game by the d-pad and South; Beam's action plays another. The script's `expect` gains `contains`.
  - **Benchmarked** before and after, hidden on Linux ([PERFORMANCE.md](docs/PERFORMANCE.md)): 7.36 and 7.30 ms a frame, the same.
- **ARQADE's new games** ([QOR-0017](docs/decisions/QOR-0017-arqades-new-games.md)), 30 October. The owner asked for platformers, 3D RPGs, an MMORPG, racing, pixel-art, dungeon hack-and-slash and arcade games built with QQ, and chose: rewarded from the start, the big ones first, and the RPG first with the online world later. Seven original games by genre; each lands whole before the next; its rules, Node test and fixture are shared with demiurge-chain under Apache-2.0 once the owner names it, and nothing about it is published before. A roadmap section tracks them.
- **Ever Rift**, the first: a dungeon hack-and-slash, three floors down into the rift ([games/README.md](products/arqade/games/README.md#ever-rift)).
  - **Its rules** (`products/arqade/games/ever-rift/logic/rules.mjs`, Apache-2.0): floors carved from the seed, rooms joined by passages, the stairs in the room furthest by walking and the warden on the third floor; the delver's swing, whirl, dash and potions; husks, wisps, brutes, seers and the warden, which wake on sight, find their way by the floor's tiles, and wind up blows that miss a delver who steps away; gold, potions, relics and levels. Only the arithmetic ARQADE's rules allow, so a run replays the same everywhere. A step blocked by a corner slips round it, as far as it would have gone.
  - **Its scene and logic**: a tilted board that moves under the eye, flagstones and rock with relief, the rock built as blocks per floor, a torch with the rock's shadows, a red ring for the warden's slam, sound from where it happens, and touch buttons for every action. `make_assets.py` writes its pictures, sky and nineteen sounds, the same bytes each run.
  - **In ARQADE** its fifth card, an emblem, the game view and the launcher's list; **in Prism** the Library's seventh ARQADE game.
  - **Tests.** `products/arqade/tests/ever-rift.test.mjs` (Node, 14 tests, in the lint workflow); `tst_games` plays it by the keys, the pad's stick and buttons and the screen's buttons through a pilot in the game's own engine (`tests/ever-rift-pilot.js`), down the first floor's stairs and then to a fall, and records the fixture; `tst_library` and `prism_products` count seven ARQADE games.
- **Checked.** The Node test passes, 14 of 14, and a fault planted in the rules (a wound's hold one step shorter) fails its fixture and its pinned state. Hidden on Linux and fenced from the GPU: `tst_games`' Ever Rift cases, the home's d-pad case with the fifth card and the launcher's ask; QQ's two controller cases; `tst_library` and `prism_products`. The run `tst_games` recorded replays to the same outcome in QQ and in Node (`products/arqade/tests/fixtures/ever-rift-run.json`: floor 2, 19 fallen, 722). QOR OS builds without warnings on Windows (`ci-windows`) and Linux; ARQADE and QQ build on Windows; their changed QML lints clean. The whole games suite runs on Windows CI.
  - **Drawn for less.** In the first hidden run the game stepped at 7 hundredths of real time in software: every idle piece of the pools was drawn, in the torch's six shadow passes too, and a dim sun drew one more. Idle pieces are now hidden and the floors have no sun, only the torch and the cavern's faint light; the same cases then ran faster than real time.
- **QQ: the left stick as four pad buttons.** An `InputAction` binds `LS Up`, `LS Down`, `LS Left` and `LS Right`, held while the stick leans past half way, so games that move a way at a time take the stick as well as the d-pad ([CONTROLLERS.md](docs/CONTROLLERS.md#games)). `tst_play` checks each way, a diagonal and a stick near rest.
- **A Linux machine of its own** ([BUILDING.md](docs/BUILDING.md#linux-on-a-machine-of-its-own-fedora-on-an-external-ssd)): Fedora Workstation on an external SSD, from the laptop's boot menu, leaving Windows untouched. `tools/dev/setup-fedora.sh` sets it up step by step (the packages by what pkg-config looks for; RPM Fusion's NVIDIA driver signed for Secure Boot, the key enrolled at the next boot; Rust; the commercial Qt kit; QOR OS and the products) and starts Prism with the products and the QQ samples. `tools/dev/products.sh --build-only` builds the products without their suites.
- **The CI runners are registered to `qor-os`, not the organisation.** On 29 October GitHub stopped giving the organisation's runners jobs: online, they took none for hours, even after `tools/ci/runners.ps1 -Reregister` (new: it removes and registers both again where they are, deleting the settings GitHub migrated for the removed registrations). Registered to the repository, they took the waiting jobs within 15 seconds. That is now the script's default ([CI.md](docs/CI.md#where-the-runners-are-registered)).

- **The security and quality bar ([QOR-0018](docs/decisions/QOR-0018-the-security-and-quality-bar.md)), from two independent reviews of the trust boundaries on 30 October 2026.** The holes they found are closed, each with a test from the side that would use it; what is left is on the roadmap and in [SECURITY.md](SECURITY.md#known-limits).
  - **Checked by machines.** Every program is built with exploit mitigations (`cmake/QorHardening.cmake`) and read back in CI (`tools/ci/check_hardening.py`). The suites run under AddressSanitizer and UBSan (the `linux-asan` preset). Actions, tools, Corrosion and Python's tooling are pinned by content, and Dependabot's pull requests stay off the workstation. `tools/ci/check_secrets.py` looks for secrets at the door, `gitleaks` through the history and `cargo audit` at the core's pin, weekly (`security.yml`).
  - **The service bus** is a socket in the person's 0700 runtime folder, no longer an abstract socket any user could reach. `QOR_BUILD_SIMULATOR=OFF` leaves the simulator out of a device's Prism altogether.
  - **Prism treats every client as hostile.** A client takes the keyboard only once it can be seen, from no other client, and never on the lock screen. An id it claims never stands for Prism's or a product's. Its strings are plain text, and the shell's engine has no network. A window's size stops at the work area, and a client at 32 windows. An app id is no path, and only plain files of a sensible size are read for it.
  - **One way off the lock screen.** `Desktop.tryUnlock` is the only one. A controller types the password and never unlocks, and nothing a device says appears over the lock screen. A session with nothing to check a password against stays locked.
  - **The Store** verifies what it installed each time it loads it, keeps a package's manifest and signature beside it, loads only QOR OS's own widgets into the shell, and refuses a version older than the one installed. What was installed before this needs installing again.
  - **The QQ Player** refuses a `qmldir` in a package, which could load a native plugin; follows symbolic links to where they really go; holds `WebVoice`, `LogicFile` and `Mixer` to the package; refuses `Qt.openUrlExternally` from modules and workers; refuses a package as wide as a home or Downloads folder; and interrupts logic that never returns. On Linux it **walls itself in with the kernel** before it makes a thread (`player/sandbox.h`): Landlock for files and programs, seccomp for the network.
  - **QQ Studio** asks before a project made elsewhere runs anything, and remembers what was made or trusted here. Its logic reads only the project, reaches no network and opens nothing outside, and a `qmldir` in a project brings in nothing. Logic an AI agent wrote runs only once the creator allows it, and an agent's file fields stay in the project. The provider key goes to no redirect, and a looping answer stops at the limit on answers. The agent's pipe is in the runtime folder on Linux.
  - **Prism's Library** calls a game the person's own only if QQ Studio made it or they trusted it: a download unpacked in Documents is no longer listed as "Made by you".
  - **QOR Media** follows only web and mail links from Markdown, as the Reader does; fetches nothing from the network or another computer for what it shows; shows a file's metadata as plain text; opens no program "elsewhere"; and keeps copies being saved in the person's own cache, not a fixed folder in the shared temporary one.
- **The service bus's pipe on Windows is this person's alone** (`QRemoteObjectHost::setLocalServerOptions`), where Windows' default let everyone on the machine read it, and the socket on Linux takes the same. **A game's store refuses an object nested far too deep** at once, where checking it first walked the whole object and hung the game. What the two reviews found and this does not close is now in [SECURITY.md](SECURITY.md#known-limits), as `d7689ad` said it was.
- **How Unreal Engine comes to QOR OS** ([QOR-0020](docs/decisions/QOR-0020-unreal-engine-on-qor-os.md)): Epic's own Linux builds, unchanged, which QOR OS never carries. Games run in gamescope and the sandbox, the editor is the person's own download through xwayland-satellite, and a plugin joins them to the ecosystem. A decision and its stages in the roadmap; nothing of it is built yet. `AGENTS.md` keeps the engine out of this repository and out of an AI assistant's context.
- **The owner's brief and next-actions direction** ([QOR-0021](docs/decisions/QOR-0021-projects-engines-and-two-stores.md)): a workspace for every project, engines through small adapters, "supported" meaning run, two stores on shared services with typed listings, starting free and curated, optional Qt apps as a backlog, the image a release deliverable, and console capture proposed. Decisions and the roadmap's order of work; each part is built on its own evidence.
- **An isolated place for Linux CI jobs, not yet in use.** `tools/ci/isolated-linux-runner.ps1` makes `qor-ci`, a separate WSL distribution where a job runs in a kernel-enforced sandbox with no route to the owner's files or Windows programs, and `tools/ci/check_runner_boundary.sh` tries each door from inside it. `wsl.conf` alone was measured not to be a boundary. QOR OS's 15 non-GPU suites pass there. No runner is registered to it, and whether it may build without Qt's licence service is a question for Qt ([docs/CI.md](docs/CI.md#an-isolated-place-for-linux-jobs)). The Linux job's first step now also stops on `AF_VSOCK`.
- **What assistants may do with Unreal Engine** ([QOR-0022](docs/decisions/QOR-0022-assistants-and-unreal-engine-after-epics-change.md)), proposed, not in force: a rule for each operation, after Epic's update 22 of 30 October, which QOR-0020 missed. `AGENTS.md`'s rule stands until the owner accepts it. `AGENTS.md` also gains a rule that no assistant operates Unity's editor, whose terms name AI agents, and that an owner's approval cannot override a third party's terms.
- **Ever Rift and Darkveil** are the owner's names for Rift Delve and Gloamreach ([QOR-0019](docs/decisions/QOR-0019-ever-rift-and-darkveil.md)), in every path and identifier, and earlier entries here use them too. Darkveil's story takes "the veil" from its name. The rules decide what they decided: the digests their Node tests pin did not move.
- **Bellwick** (a working title), the third of ARQADE's new games ([QOR-0017](docs/decisions/QOR-0017-arqades-new-games.md)): a platformer on islands in the sky. Tam runs, jumps, stomps, slams and rings a hand bell across eight hand-laid levels in four regions, each with its own ground, far country, sky, light and music, to ring the Great Bell at the end of each; Old Muffle waits at the top of the Belfry and in the Last Belfry. Note, rich, power and life blocks; brass and the kite; hush blocks that stand only when rung; six kinds of thing that walks, flies or falls; platforms, fallers, springs, mid-way bells, three silver bells a level and secret rooms. Its rules keep to ARQADE's replay-safe arithmetic. ARQADE's seventh card and Prism's Library carry it. `tests/bellwick-solver.mjs` finds a way through the whole game by the rules themselves, and the Node test plays what it found, so a level no one could finish cannot be committed ([games/README.md](products/arqade/games/README.md#bellwick)). Its rules do not cross to demiurge-chain until the owner names it.
- **Ever Rift and Darkveil face the eye.** QQ's eye looks from a quarter of the way round, and the four rebuilt games turn their stages to meet it; these two did not, so "up" went diagonally across the screen. Their rules did not change.
- **Darkveil**, the second of ARQADE's new games ([QOR-0017](docs/decisions/QOR-0017-arqades-new-games.md)): a 3D action RPG in one valley. A village with four people and their errands, three wardens over three beacons, a gate and the Hollow King behind it; a chain of three swings, a roll, a bolt, a nova and potions; camps of five kinds of enemy, levels, gold and what it buys. Its rules keep the world's state apart from each player's, in ARQADE's replay-safe arithmetic. Its land, scenery and rigged figures are written by `make_assets.py` with the standard library. ARQADE's sixth card and Prism's Library carry it; its rules, Node test, pilot and fixture go upstream once the owner names it ([games/README.md](products/arqade/games/README.md#darkveil)).
- **A Build clears what a stopped machine left**: the crash left empty objects in CI's Linux folder, dated as built, so its next link failed; empty objects are now deleted before building.
- **CI runs one job at a time on the workstation**, at 8 compilers each, not two at 22: it reset itself on 31 October 15 minutes into two full rebuilds at once ([CI.md](docs/CI.md#linux-runner-in-wsl2)). The cause is not proven; the load is what could be changed. A secret scan's findings now say rule, file, line and commit, and never the value or who committed it. Every finding in the history was a developer's public key in a Store catalogue, each shown to be a point on P-256: no secret.
- **Every product suite hears only virtual controllers** (`products/qq/tests/virtualpads.cpp`, built in by CMake, so a new suite cannot forget). `tst_studio` and `tst_agent` read the real controller on the machine they ran on, whose stick walked and turned the player out of their cases on the Windows runner. `docs/CONTROLLERS.md` said every suite was fenced; now it is so. A HID scan's list of every device on the machine is off unless asked for. QQ's music test looks at a fade by how far it has come, so a slow frame on a busy machine cannot carry it past the middle unseen, and it now checks both levels exactly.

### Changed

- **QQ Studio asks before unsaved work is replaced:** New, another scene or logic file, Close in the logic editor and closing the window ask too.
- **The products' suites run to their end on Linux in software, and what that leaves out is listed.** On a GPU they check what they did before, and one thing more: a long wait by QQ's agent tool lasts what it was asked for. In software alone, chosen by the renderer's own name, QQ's cases that time movement wait on the game's clocks, ARQADE draws on Qt's basic render loop, sounds are checked as they begin, Darkveil's pilot pays its own ticks, and the wall's limits are longer. So software no longer shows that the world keeps up with real time, that frames pay logic's ticks at the right rate, that ARQADE's threaded loop works, or that a sound goes on sounding ([CI.md](docs/CI.md#what-linux-runs)). The stall at Orbital's ten locks was no stuck frame: on the threaded loop, on Wayland, at these frame rates, each turn of the event loop ran one frame more than the last. `tools/dev/products.sh` names every case failed or skipped, with why, in the log and the job's summary. `--accept-software-floor` lets the QQ Player's floor stand, only for a Player still drawing at least a frame a second that ended as it said, and `build.yml` does not pass it. `build.yml` can run them with `QOR_RUN_LINUX_PRODUCTS` alone, which is still unset.
- **The products' suites on Linux say what drew them.** `tools/dev/products.sh` always sets `QOR_EXPECTED_RENDERER` from the machine's devices, and `tst_play`, `tst_studio`, `tst_agent`, `tst_player` and `tst_games` fail if the renderer disagrees, whichever way round. So a machine with a GPU that falls back on software fails, where before it took the eased path untold.
- **`tst_player` checks on every machine that the QQ Player was still drawing at the end of its measured time** (`lastFrameAgoMs` in `--measure`'s report, which also names the renderer). The floor of 20 frames a second is unchanged. `qq-player` now links `Qt6::GuiPrivate`.
- **`tools/dev/products-report.py` reads CTest's log and record together.** A suite that isn't Qt Test reads passed, failed or stopped. One that could not start, or skipped itself by its `SKIP_RETURN_CODE`, reads NOT RUN, never a pass. Any other suite that failed, stopped or could not start refuses `--accept-software-floor`; one that skipped itself does not.
- **`products.sh` counts a retry only if the record shows it ran each failed suite and that suite passed.** It names at the end any suite that passed only on a later attempt, removes the last record before each attempt, and fails a filter that matches nothing.
- **Build's jobs on the workstation are paused until their runners are isolated.** Both runners could reach the Store's signing keys and the owner's credentials, and their jobs run third-party code. A job on a kit's runner now needs `QOR_WINDOWS_RUNNER_ISOLATED` or `QOR_LINUX_RUNNER_ISOLATED`, unset for now, and its first step checks the claim before anything from the repository runs. Both runners are stopped and do not start at sign-in, and neither variable is set ([docs/CI.md](docs/CI.md#what-ci-trusts)).
- **A release needs the products run unrelaxed on a graphics card.** Release readiness requires the products' suites on a Linux machine with a graphics card as well as in software, since in software some of their checks are eased. No job runs that yet, so it reads "not run" and no commit is ready for a release.
- **A run that built nothing is no longer green.** Build's last job, Release readiness, tells a runner being allowed from a build having passed, and fails unless every step a release needs ran and passed. So Build's result is red while its jobs are paused, by design (`tools/ci/release_readiness.py`). Unchanged products may stand down outside a release only when their step was skipped and the run says, exactly, that it is not a release; "unchanged" counts every file they are built, hardened or checked with.
- **[QOR-0015](docs/decisions/QOR-0015-the-products-are-developed-here.md): QQ, ARQADE's app and games, and QOR Media are developed here** from 29 October 2026, no longer pulled from demiurge-chain, whose copies its ADR-126 freezes and its Windows launcher keeps shipping. Nothing had changed upstream in their paths since the carry (`28d9e97`, identical to `db98ac0`). The owner chose each part of it: the products move and the core and both launchers stay; new work on QQ is closed until the public SDK's record; the launcher ships frozen copies.
  - **Licence:** the 374 files that came from demiurge-chain are listed in `LICENSES/from-demiurge-chain.txt` and stay Apache-2.0; work on the products since is QOR OS's (`LICENSE`, `THIRD_PARTY_NOTICES.md`). `tools/ci/check_headers.py` now checks the products: a file not on the list needs QOR OS's SPDX header, and the list may name only files that exist (`tests/tools/test_check_headers.py`, 5 cases).
  - **Still shared:** ARQADE's five rule modules (each game's `logic/rules.mjs`, the arenas' `logic/arena.mjs`), which ARQADE's Node tests upstream import and its service is to judge runs by, stay identical there and Apache-2.0, and change in both at once with their fixtures. `tools/upstream/demiurge.sh` now pulls only the fixtures and `docs/design/tokens.json`, says whether the rules match upstream, and writes patches of all three.
  - `AGENTS.md` §3 and §7, UPSTREAM, ARCHITECTURE §11, BUILDING §6, DEVELOPMENT, CI, CONTRIBUTING, README, VISION and the roadmap say so.
- The product is **QOR OS**. README, VISION, ROADMAP, ARCHITECTURE, BUILDING, DEVELOPMENT, CI, PERFORMANCE, QT-MODULES, STORE, CONTRIBUTING, SECURITY, issue templates, labels and milestones are rewritten for it.
- **The name is QOR OS in the code too** ([QOR-0009](docs/decisions/QOR-0009-the-name-is-qor-os.md), superseding QOR-0008's wait for clearance). QTOS now names only the predecessor.
  - Code: the `qor::` namespace, `qor_*` targets, `qor.*` logging categories, `QOR.*` QML modules (`QOR.Kit`, `QOR.Shell`, `QOR.Services`, `QOR.Store`, `QOR.Input`, `QOR.Apps`, `QOR.Prism`), `QOR_*` options and environment variables, `LicenseRef-QOR-OS-Proprietary`, and `project(qor-os)`.
  - Programs: `qor-shell`, `qorctl`, `qor-simd`, `qor-gamepads`; the SDK template `sdk/templates/qor-app`; `tools/dev/Enter-QorShell.ps1`.
  - The interface says QOR OS, and the generated music is **QOR Sessions**. The shell and simulator carry their settings and data (installed Store items among them) over from "QTOS Shell" and "QTOS Simulator" on first run.
  - The Store: the **QOR OS Store**, first-party items as `dev.qorsync.*` in `.qorpkg` packages, re-signed with the same store key (`store/keys/store.pub` unchanged). `build_store.py` uses `~/.qor-os/store-signing/`, where the keys moved on 27 October (verified byte for byte, with a dated backup), and falls back to `~/.qtos/store-signing/` on a machine that has only that.
  - CI variables are `QOR_*` on `qor-os`; the organisation domain is `qorsync.dev`.
- The roadmap is QOR OS's phases 0 to 4, with release gates. QTOS's phases are kept as history, and the phase numbers cited across the documents follow the new ones.
- The base image is **Fedora Atomic** with bootc, NVIDIA's open kernel modules and Mesa ([QOR-0004](docs/decisions/QOR-0004-the-base-image-is-fedora-atomic.md)).
- **Displays:** every display at its own refresh rate on every GPU is the acceptance test, and Prism gets its own KMS backend. QTOS's bare-metal decision gate is **deferred, not failed**, and the gate kit becomes a diagnostic ([QOR-0005](docs/decisions/QOR-0005-every-display-at-its-own-refresh.md)).
- **Money:** QOR OS 1.0 moves no money. It has free items and test CGT only, and the Store's paid stage waits for written decisions ([QOR-0006](docs/decisions/QOR-0006-qor-os-1-0-moves-no-money.md)).
- CI:
  - the presets require the QOR core;
  - the Windows job builds and tests the carried products when they change;
  - Lint skips the carried code's headers and formatting;
  - Linux CI installs `libdbus-1-dev`.

### Removed

- QTOS's Yocto skeleton (`image/meta-qtos`, `image/kas`), replaced by the Fedora Atomic plan in `image/README.md`.

### Fixed

- **The full device layer checks Prism's compiled capabilities.** Previously the
  structural guard only required a file named `prism` and checked the welcome app's
  build mode. It could accept simulated or unauthenticated Prism, including when
  capability probing was omitted. A headless query now reports Prism's actual
  simulator mode and unavailable authentication before graphics, audio, services or
  user state initialise. The image guard requires a non-development authenticated
  contract, with exact fields and boolean types. Current real device builds remain
  refused until their authentication backend exists. The prior private installation
  image and evidence retain their separate, limited scope.
  All 18 Windows image cases and 21 Linux image/archive cases pass; both command-line
  modes pass 11/12 Qt cases on Windows, Linux Release and ASan/UBSan. Actual binary
  probes work with graphics unavailable and write no user state; the image guard
  refuses real device Prism's missing authentication. Hardening and all ten existing
  QML lint commands pass. The three full Linux Prism builds exit 0 but retain failed
  warning gates on 12 pre-existing Wayland type-registration warnings; no full device
  session or newer image is accepted. RAM and harness failures are preserved.

- **Stock installation mounts are confined to the backend's lifetime.** Two actual
  normal UI reboot tests failed after successful installation. A diagnostic captures
  bootc's storage bind mount still active after exit 0; removing only that verified
  mount makes the unchanged strict guard and real button succeed. The backend now
  runs stock bootc in a private mount namespace and rechecks the strict target guard
  before announcing completion. Namespace launch failure and a still-active target
  cannot produce completion; the new regressions fail on the old code. All 67
  portable cases pass on Windows/Linux, plus eight Linux process/file cases.
  Real-kernel controls verify namespace teardown in the existing SELinux context
  with enforcement enabled. The fresh clean `66df3f7` image and same-revision compiler
  now pass stock installation and the actual UI reboot on a new 8-GiB target without
  manual cleanup or debug overrides. Two media-free installed boots prove welcome
  changes from pending to complete, then retains the same record before welcome.
  All three native VM phases exit 0 with three distinct boot IDs; the final verifier
  accepts the complete evidence. Prior proofs and failures are unchanged. Both image
  branches pass 14 fatal-warning checks with zero warnings and one skipped check.
  The approved service pause is restored and PostgreSQL is healthy. Full device,
  hardware and release gates remain open.

- **Unconfigured kernel loop nodes are distinguished from backed disks in the
  private VM inventory.** An actual UI reboot fails after a successful fresh
  installation. A separate kernel diagnostic reproduces the collector's refusal
  after requesting an unused loop node. The correction excludes only an inactive,
  zero-size kernel loop node with major number 7 and absent backing attributes.
  The guest preview still refuses attached writable backing. All 65 portable
  cases pass on Windows/Linux, plus eight Linux process/file cases. The first
  Linux harness omitted its read-only image inputs; that failure is preserved.
  The clean 5b7d321 image includes this correction and installs successfully; its
  normal UI reboot still encounters the separate storage-mount issue above.
  Prior installation and guest-agent persistence proof retain their original scope.

- **VM deployment keeps its I/O workers inside the container budget.** QEMU's
  main-loop pool is bounded at four workers within the unchanged 64-task limit,
  addressing a thread-creation failure during stock deployment. The regression
  fails on the old launcher, then all 62 portable cases pass on Windows/Linux,
  eight Linux process/file cases pass, and native QEMU confirms the pool setting.
  The unchanged clean `feed626` image subsequently installs with backend exit 0,
  boots without media, completes the real welcome and retains its record across
  two installed reboots. Five actual VM phases exit 0 across preserved resource
  interruptions; the installed disk is never reset. That sequence uses the restricted
  guest agent. A later actual frontend reboot test fails, as recorded above. Current
  service states are verified after container-identity replacements, with failed
  lookups preserved. Full OS, hardware and release gates remain open.

- **Pre-1970 Windows recovery records keep the same protection.** The legacy matcher recognizes the optional minus
  sign emitted by the old timestamp identity format. Negative journals now stay for review and block a new transaction;
  negative workspace copies keep the correct review explanation. Two synthetic negative rows first failed with native
  and wrapper exit 2 while positive/zero/missing identities passed. The same tests now pass within 78 native data
  cases/35 functions, zero failures/skips; shared Linux Release/ASan/UBSan passes 69/32 per build. The three Windows
  functions are not compiled on Linux. The two production changes only add the optional sign; automatic migration,
  file-ID checks and trust rules remain unchanged. Fresh source/program/ASan/hardening records pass, without reading
  any personal recovery records.

- **Player labels retain their scene and follower interfaces.** Dynamic property reads keep titles and movement hints
  bound to different scene/player types, preserving the existing guards, text and fallback branches. New compatibility
  coverage checks name/style edits, absent followers, the 3D hint and release fallback; existing plain-text/error and
  actual Player startup/report checks remain intact. Six Scene/eight Player Qt cases pass per Release/ASan/UBSan build,
  zero failures/skips. Fresh source/program/copied-Main/sanitizer/hardening records pass. Two old lint findings disappear
  and none are added: Windows/Linux both report 142 console/167 structured warnings. The Player leaf passes; whole-QQ
  warning gates still fail and the wrapper returns 1. The separate Studio startup failure remains unresolved.

- **Gizmo keeps the selected entity's structural interface.** Kind, field metadata and scene position use dynamic
  property lookup across the different selected types, retaining the `QtObject` target, null/field guards and existing
  visibility/transform behavior. Fresh Windows and Linux lint removes exactly five old findings and adds none:
  149 console/174 structured warnings become 144/169. Every warning gate still fails. Six existing Studio functions
  pass eight Qt cases per Release/ASan/UBSan build, zero failures/skips, covering picking, translation, scaling, flat
  editing, snapping and groups/prefabs. Source/program/copied-QML records, actual sanitizer flags/libraries and
  hardening 16/13 pass; the aggregate wrapper keeps exit 1 for remaining lint. The separate actual-Studio
  initialization failure is unresolved.

- **Recovery keeps records it cannot safely place.** A temporarily missing project retains its asset journal until
  the original folder returns. Windows records volume/file identifiers instead of creation timestamps, so a replacement
  with a cloned timestamp gets no asset rollback or unsaved workspace copy. Unavailable identities and older Windows
  timestamp-only records are kept for review; a pending journal blocks the next asset transaction instead of being overwritten.
  Three new cases first failed on unchanged Windows code, returning native/wrapper exit 3. The corrected native data
  subset passes 72 cases/35 functions, and Linux Release/ASan/UBSan passes 69/32 per build, zero failures/skips. The fourth
  workspace-copy check is additional coverage; three Windows-only functions are not compiled on Linux. Fresh source,
  program, compiler, sanitizer and hardening records are checked. No personal recovery files were inspected or migrated;
  full UI/product/hardware gates and the checkout-local Store rename failure remain separate.

- **QQ's native Windows asset data build is warning-clean.** The Studio keeps the native string-length type when
  recognizing asset options, and a provenance field loop no longer shadows the public fixture-key variable. Both
  pre-existing warnings failed the strict MSVC build before these local fixes; warnings remain errors. The selected
  native data subset passes all 68 Qt cases/31 functions, zero failures/skips, including crash recovery and link checks.
  Fresh source/program/XML records and two hardened program headers are verified. This runs no Studio/Player window,
  3D scene or audio device, and does not establish the full Windows product gate or resolve the timestamp-identity lead.

- **An allowed product skip no longer claims release readiness.** The summary says non-release checks passed and
  release readiness is unproven when unchanged product suites stand down. All required outcomes and the existing
  non-release exit 0/release-skip exit 1 policy stay intact. A synthetic-status probe and four failing regression
  subcases reproduced the old claim; 16 readiness tests now pass on each platform, including actual child exits and
  private summary files. Workflow checks pass 14 on Windows with one explicit Bash-only skip, and all 15 on Linux.
  The policy instructions and 1,920 bounded before/after status comparisons agree; no actual CI or runner action ran.

- **Inspector metadata keeps its structural entity interface.** The selected kind, name and field list use the existing
  dynamic metadata lookup, as the optional editor dictionaries already do. Public target typing, guards, edits and
  undo keys stay intact. Fresh Release/ASan Studio controls pass five Qt cases and all three selected functions per
  build, zero failures/skips; source hashes, actual sanitizer commands, app/test relinks and hardening (16/13) are
  verified. Four existing lint findings disappear with none added: 149 console/174 structured warnings remain. Native
  lint exits 255 and its wrapper exits 1; the zero-warning gate remains failed. Hidden software tests prove no hardware
  or release readiness.

- **Benchmark output failures fail the command.** The shared shell/Prism collector checks requested-file opening,
  the complete write, flush and close, plus stdout writes/flushes. Failure emits a generic diagnostic and exits 1;
  successful output keeps its schema and replacement behavior. An actual compositor reproduced exit 0 for a directory
  destination before the fix; three process regressions reproduced the directory, virtual-full-sink report and stdout
  failures. The full existing collector suite passes all 11 Qt cases/8 functions per normal/ASan profile, zero failures/
  skips, including stall/idle/thread controls and complete stale-output replacement. Actual compositor checks verify
  failed-report exit 1 with an intact sentinel and fresh-report exit 0 with matching JSON and 40 input samples.
  Source fingerprints, sanitizer commands/libraries, native records and hardening (33/31 programs) are verified.
  Same-machine software observations are in PERFORMANCE.md; original failures remain recorded and no native hardware,
  full-build or zero-warning gate is cleared. Windows-specific execution remains unrun.

- **Failed script steps omit their complete request objects.** The shared shell/Prism runner retains the step number,
  existing reason and native exit 4, while dropping the automatic JSON dump that exposed arbitrary nested arguments.
  A new hardened process-level QtTest reproduces the disclosure before the fix and keeps captured child diagnostics in
  memory even when its assertions fail. Its three Qt cases plus the existing 50-step client scenario and item-text
  privacy control pass per normal/ASan profile, zero failures/skips. The first post-fix run exposed a nested temporary
  socket path that was too long; controlled runs and WSL socket binds prove it, and a short private test root fixes it
  without changing assertions or timeouts. Fresh records, 347 source fingerprints, actual sanitizer flags/libraries and
  hardening (32/30 programs) are verified. Three before/after software benchmark reports each retain all 40 input samples;
  their observations and limits are in PERFORMANCE.md. Original failed runs remain recorded; warning/hardware/CI gates stay unmet.

- **Shape's private collision factories use their actual owner.** Dimensions, contact callbacks, mass and kinematic
  transforms are qualified through the owning Shape; public interfaces, materials and 2D axis locks stay unchanged.
  Native QQ lint removes 13 existing findings with none added: 153 console/178 structured warnings remain, all three
  commands exit 255 and the wrapper exits 1. Full Scene (52 cases) and six physics functions (10 cases) pass per
  Release/ASan build, zero failures/skips, with fresh records/source fingerprints and hardening 16/13 verified.
  An existing group test's wall/frame-clock mismatch reproduced with both original and candidate Shape sources; its
  software waits now use the existing bounded physics/frame helpers. GPU waits, tolerances and the Player's frame-rate
  floor remain. Earlier failed runs are retained. Production QML/apps were built in the preceding check; the final
  test-only revision rebuilt the test and reused those verified apps. Software checks do not prove hardware readiness.

- **Asset replacement confirmation rechecks trust at the write entry.** A real-window regression reproduced copying and locking files after the window's trust state changed to false; Update already refused that case. Copy now checks trust and current build/asset work before starting, including delayed confirmations. Two rows cover refusal without file changes and successful fresh requests after trust is restored. All 91 asset-library cases pass in Release and under sanitizers without skips; hardening passes 16/8 programs. QQ lint stays at 347 diagnostics, none added, with its native gate failing.
- **Long benchmark stalls reach the report.** Continuous shell benchmarks now retain intervals of 200 ms or more; the interactive Monitor keeps its idle filtering and the sampler's cap is unchanged. A hidden synthetic-frame regression reproduced a missing 500 ms pause, then passed under ASan/UBSan with four other cases and no skips. Same-machine synthetic probe-cost measurements establish no speed change or rendering/hardware readiness.
- **The shell's window manager uses an unambiguous private ID.** Renaming `layer` to `windowLayer` removes two pre-existing shadowing diagnostics that appeared only in Qt's JSON output, despite its failing native status. Native shell lint now checks all 28 files with zero warnings/errors and one unchanged informational note; aggregate root lint passes on the incremental build. The existing shell UI script passes before and after under sanitizers, with fresh records. Same-machine software benchmarks are recorded without a speed claim. Twelve Wayland metadata warnings seen during dependency generation remain unresolved.
- **The frame probe stops publishing while idle.** A regression counted three unnecessary timer wakes in 750 ms after idle. The publisher now stops and queues its restart on the GUI thread when frames resume, retaining the cadence, idle filter, sample cap and benchmark stall capture. Seven sanitizer cases pass, including another-thread resume, queued detach and idle-status redraw feedback; the shell UI script and five-program hardening check pass too. An initially slower timing series is retained; controlled alternating comparisons do not reproduce its large gap. No speed or hardware-readiness claim is made.
- **The Studio's project-file list binds to its own panel.** Its delegates keep their intended context, its Studio reference uses the actual window type, and kind marks declare their preferred layout size. Seven selected Qt cases pass in each of Release and ASan/UBSan, without failures/skips: file click/filter, saved-state summary/Forget, and two-project asset/filter isolation. Console lint falls from 347 to 340 messages with none added; the panel's structured findings fall from 14 to 5 with none added. Full QQ JSON reports retain 375 warning records, including repeats collapsed by the console; all three native lint commands still fail.
- **Logic problem descriptions are plain text.** A real Studio regression reproduced automatic text parsing of an inert supplied description. The problem label now preserves the supplied characters with its existing font, error color and ellipsis; the editor also uses its actual Studio window type and binds its delegates to that context. Three focused and 18 related Qt cases pass in each of Release and ASan/UBSan, none failed/skipped, covering editing, exact save filenames, unsaved prompts, recovery, project switching, build interlocks and packaged logic. Console lint falls from 340 to 338 messages; structured output falls from 375 to 363 warning records, with the editor's 12 findings removed and none added. QQ's native lint gate still fails.
- **A Player measurement that cannot be written fails the command.** A real-process regression reached measurement generation but reproduced exit 0 when its requested output was a directory. `--measure` now checks opening, the complete write, flushing and close status, emits a generic error and exits 1 on failure. The exact-file kernel permissions are unchanged; no directory grant or atomic-write claim is added. Tests verify failed output leaves its directory sentinel intact, a valid report replaces stale JSON, and a later built-copy Player launch produces its own report path. Four Player cases and three build-copy cases per Release/sanitizer build pass with zero failures/skips. Fresh records/source hashes and unchanged sandbox sources are verified; hardening passes 16 Release/12 sanitizer programs.
- **Current-program build and playback journeys pass locally.** At `6f14fe2`, three existing scenarios pass in each of Release and ASan/UBSan: five Qt cases, zero failures/skips. They cover Studio edit/save/build, project switching/reopening, actual Player playback independent of the source project, and the real Studio's headless trust/command-line protocol. Fresh records and matching product sources are verified; hardening passes 16 Release/11 sanitizer programs. The CLI explicitly reports `compiled:false` for its copy-only operation. Private WSL rendering and retained no-audio-device warnings provide no native hardware/audio proof.
- **The integrated asset/build/Store fixes are rechecked at `39b0768`.** Fresh unfiltered Release and ASan/UBSan runs each pass 91 asset-library, 195 game-build and three real Store-to-Player Qt cases, zero failures/skips: all 44/48/1 ordinary functions, with the largest-pack measurement and real StoreEngine demonstration-pack path included. Native records, all 18 source hashes and unchanged product sources are verified. Post-run hardening passes 16 Release/13 sanitizer programs. The prior StoreEngine installer helper is identified separately; this is not a current compositor/hardware proof. Release first listing/planning measure 246/386 ms on the window thread; no latency budget or improvement is claimed. Each completed suite is below the documented 5,400-second software ceiling; the picked CTest command omitted the aggregate timeout argument, recorded explicitly, and a corrected local display clone is prepared. QQ lint remains 166 console/191 structured warnings, none added; all native lint commands fail and the wrapper retains exit 1.
- **The first-journey check retains child failures and requires fresh reports.** A success-looking `QQ-BUILD` can no longer hide a failed Studio exit; duplicate, malformed or non-object results fail explicitly. Player output paths must be unoccupied before launch and become regular, non-link UTF-8 JSON-object files afterward; an earlier report is preserved and refused. Real inert subprocess regressions reproduce the false-success/stale-output paths, then pass all 15 tests on Windows and Linux, zero skips. Missing, negative, boolean/string and non-finite first-frame markers also fail. These are wrapper tests, not Qt/Prism integration proof. Missing Prism stages still return 77 as not run. The building guide distinguishes the simulator stand-in launch from the real Player's separate test-display playback.
- **Player startup problems are plain text.** The error label now shows filenames and supplied problem text literally while retaining its live binding, font, wrapping and color. A real-QML regression first reproduced automatic parsing, then verifies inert markup and a later filename change. All 52 Scene and five real Player startup Qt cases pass per Release/sanitizer build, zero failures/skips, with fresh records/source hashes and hardening 16/12. Production C++, Game and SceneIO are unchanged from `ad4aa3f`; lint remains 166 console/191 structured warnings, none added, and the native zero-warning gate still fails.
- **Incomplete scenes report a startup failure and can be retried.** SceneIO refuses a non-3D scene root; Game discards an incomplete 3D root and reports a generic problem before a valid retry. A measured Player reports that problem with exit 3, or exit 1 when the report cannot be written. A single final-result guard keeps later callbacks from replacing that status; ordinary play stays open after its console report and capture finishes after saving its image. At `ad4aa3f`, Release/ASan/UBSan runs pass all 51 Scene and 11 selected Player Qt cases per build, zero failures/skips, including capture and record/replay. Hardening passes 16/12 programs; sandbox/confinement/watchdog sources are unchanged. Final runs use two CPUs and a 1.75 GiB hard memory cap after observed paging at the earlier tighter cap; failed experiments remain recorded. QQ lint stays at 166 console/191 structured warnings, none added, and its failing native status is preserved.
- **World’s environment names its own light-probe textures explicitly.** Qualifying `photographed` and `made` removes two lint findings without changing lighting or material values. Four Qt pixel cases per Release/sanitizer build pass, zero failed/skipped, covering HDR sky/lighting/rotation/bloom and pictured-versus-plain procedural lighting. Fresh records/source hashes and hardening 16/12 are verified; both programs are relinked. QQ lint still fails at 166 console messages/191 structured warnings, with none added.
- **Private physics factories bind to their owning Scene, Player and TileMap.** Bound component declarations remove six lint findings without changing physics values, movement or signals. Six gameplay and three actual Studio Qt cases per Release/sanitizer build pass, zero failed/skipped: per-scene gravity, pause/resume, player pickup, tiled collision and the collider-debug toggle. Fresh records/exact function coverage and hardening 16/12 are verified; both programs are relinked. QQ lint still fails at 168 console messages/193 structured warnings, with none added.
- **Player2D’s private carrier body binds to its owning player.** A bound component declaration removes six lint findings without changing movement, dimensions, controller handling or physics values. Five Qt cases per Release/sanitizer build pass, zero failed/skipped: tiled-level rendering/collision, top-down movement/carried look and headless 2D trigger replay. Both programs are relinked; fresh hardening passes 16/12 programs. QQ lint still fails at 174 console messages/199 structured warnings, with none added.
- **The saved-game store’s existing instance API has a concrete QML type.** `GameStore` is declared within QQ and stays uncreatable from QML; the project panel uses it and receives a checked cast from the runtime. Storage algorithms, formats, paths, limits and permissions are unchanged. A memory-only regression first reproduced the unknown type, then verifies instance fields/state/clear and refusal of direct construction. All 49 scene cases and three actual Studio save/Forget cases pass per Release/sanitizer build, zero failed/skipped, with fresh records and hardening 16/12. Both programs are relinked. Five panel lint findings are removed with none added: 180 console messages/205 structured warnings remain, and native lint still fails. The earlier complete 56-case play runs at `89877cf` precede this metadata batch.
- **The integrated QQ runtime passes its complete play suite at `89877cf`.** Release passes 56 Qt cases in 1,878.98 seconds and ASan/UBSan passes 56 in 1,923.71 seconds, with zero failures/skips. Both runs are unfiltered: all 51 functions, 54 data-expanded rows plus setup/cleanup. Fresh native records, exact function/row coverage and unchanged product-tree/source hashes are verified; incremental builds emit no warnings and post-run hardening passes 16 Release/12 sanitizer programs. These remain hidden WSL software checks with leak checking off, not hardware or full OS proof. Lint still fails at 185 console messages/210 structured warnings. A semantic comparison against the first retained 381-warning structured snapshot verifies 171 removed and none added; it does not reconstruct the earlier reported 439-warning revision.
- **Sound factories bind to their owner and use the viewport’s actual camera types.** Sound and one-shot voices now declare their private viewport as `View3D` and eye as `Camera`, following `view.camera` (which World binds to its active camera). Bound factories and these concrete types remove 26 lint findings, without changing gain, panning, reach, loops or expiry. A pre-change baseline verifies live perspective/orthographic switching and one-shot camera identity. Eight Qt cases per Release/sanitizer build pass with zero failures/skips, including placed/one-shot sounds, both music paths and silent replay. Both programs are relinked; hardening passes 16/12 programs. QQ lint still fails at 185 console messages/210 structured warnings, with none added. These native logic checks do not demonstrate physical sound or browser playback.
- **Trigger keeps its volume and collider factories in its owning context.** A bound component declaration removes nine lint findings without changing dimensions, positioning or signal behavior. Six Qt cases per Release/sanitizer build pass, with zero failures/skips: box entry/exit, spherical player pickup, moving groups and headless 2D detection. Both programs are relinked; hardening passes 16/12 programs. QQ lint still fails at 211 console messages/236 structured warnings, with none added. A separate Shape experiment exposed two parent-view type findings and was left out.
- **Sprite keeps its collider and body factories in its owning context.** A bound component declaration removes 19 lint findings without changing rendering, frame sizing or physics values. The plane-lock regression now includes dynamic sprites landing on static and kinematic sprite floors, with contact reports and the original axis checks. Seven Qt cases per Release/sanitizer build pass, including crisp pixels and pause-held sheet animation, with zero failures/skips. Studio and Player are relinked in both builds; hardening passes 16/12 programs. QQ lint still fails at 220 console messages/245 structured warnings, with none added.
- **Music keeps its two private voice factories in its owning context.** A bound component declaration removes four lint findings without changing gain, fading, source selection or lifecycle. The crossfade regression now checks both ambient and WebVoice factories, with the same volume, fade, release and stop assertions. Four Qt cases per Release/sanitizer build pass with zero failures/skips; hardening passes 16/12 programs. QQ lint remains failing at 239 console messages/264 structured warnings, with none added. These are hidden logic checks, not physical sound or browser playback proof.
- **Ground keeps its slab and texture bindings in its owning context.** Bound components and an explicit texture owner remove four lint findings without changing geometry, material values or physics. Existing collision/gravity checks pass five Qt cases per build, and the checker/extent pixel check passes three per build, in Release and ASan/UBSan with zero failures/skips. Studio and Player executables are rebuilt in both variants; their fingerprints and timestamps match the current input scope. Fresh hardening passes 16 Release/11 sanitizer programs (the sanitizer inventory now includes the play and world suites). QQ remains at 243 console messages/268 structured warnings with none added, and all three native lint targets still fail.
- **Group names stay literal in the Studio's selector popup.** A real-window regression reproduced AutoText in the default popup label. The selector now uses a public MenuItem delegate with a plain-text label and explicit model roles, retaining its font/material bindings, highlighting, hover, mirroring and width. The new test also activates the actual delegate and verifies reparenting and the selected index. Seven selected Qt cases per Release/sanitizer build pass with no failures/skips, covering grouping/prefabs, undo, duplication and the Studio look. Hardening passes 16 Release/9 sanitizer programs; lint stays at 247 console messages/272 structured warnings, with none added and its gate still failing. The complete 28-case runs at `b2b084c` precede this focused popup fix.
- **The integrated Studio passes a fresh complete local run.** At product revision `b2b084c`, all 26 test functions plus setup/cleanup pass in each of Release and ASan/UBSan: 28 Qt cases, zero failures/skips, without a function filter. Fresh native records and matching product sources are verified; incremental build logs have no warnings, and hardening passes 16 Release/9 sanitizer programs. The runs use hidden WSL software graphics with the graphics-processor fence and leak checking off; no native hardware or full-OS claim follows. QQ's older phase counts are explicitly historical. VM first-run notes now use the current safely checkpointed, clean checkout; the script is verified byte-identical to reviewed `c9513c6`, with separate Create authorization still required. No VM, runner or licensing action ran.
- **Agent-panel step rows keep their own model bindings.** Bound components and an explicit row ID remove ten lint findings without changing the designer interface, controls or row style. An inert two-row/replacement regression passes before and after; six selected Qt cases per Release/sanitizer build pass without failures/skips, including literal status/key problems and the existing key/description flow in the private test keyring. Hardening passes 16 Release/9 sanitizer programs. QQ remains at 247 console messages/272 structured warnings, with none added; its three native lint targets still fail.
- **The Studio inspector treats source filenames as text and keeps each loaded field editor bound to its owner.** A real-window test reproduced automatic markup detection in a filename. Filename and field labels now use plain text; typed loaders and explicit axis roles retain their inspector context. A transient text-row assignment warning found by the editing checks is fixed without weakening the warning gate. Seven selected Qt cases pass in each of Release and ASan/UBSan with no failures/skips: filename display, loaded field edits and selection changes, undo, grouping/prefabs and 2D editing. Fresh hardening passes 16 Release/9 sanitizer programs. QQ lint removes 76 console/structured findings with none added, leaving 257 console messages/282 structured warning records; all three native targets still fail. QQ's README now describes the current paused CI and VM/licensing prerequisites.
- **The Studio statistics overlay uses its real data types.** `World` and Qt's `RenderStats` replace generic object references; its labels are plain text. The existing F3 display/play/collider-toggle check passes in Release and under ASan/UBSan (three Qt cases per build, none failed/skipped), with the sampling timer retained. Five findings are removed with none added: QQ remains at 333 console messages/358 structured warning records, and all three native lint targets still fail.
- **Malformed benchmark metrics cannot pass the compositor gate.** Reports must cover the running outputs and carry valid finite counts, refresh rates, intervals, percentages and input percentiles. Negative timings, non-finite intervals and boolean counts reproduced false passes with inert producers. Valid zero-sample trials remain diagnostic; no hardware readiness is inferred.
- **An interrupted gate-bundle write preserves the earlier archive.** Packaging writes a new sibling file, verifies it is readable as an archive, and replaces the destination only on success. Regressions first reproduced a failed writer truncating older evidence and a no-op writer falsely reporting success; both now fail without changing it. Successful replacement remains covered with inert fixtures.
- **Gate bundles stop on failed dependency inventories.** File discovery, `ldd` and dependency filters now propagate failures, including partial output, before an archive can replace earlier evidence. Regressions use only a fake Qt tree and command stand-ins. Bundle metadata identifies the actual executable digests and leaves their build revision unverified; the launcher checkout and packaging date are labelled separately.
- **The compositor benchmark checks its animating load.** A client that exits during measurement fails the run, even with status zero; measuring before the clients start also fails. The finite benchmark monitors its own child processes only. Inert regressions cover startup and later failures, with normal post-measurement disconnects, valid runs and trials still passing; no hardware gate was run.
- **The compositor benchmark gate requires evidence from a successful fresh run.** Existing output folders are refused intact, and Prism/watchdog failures propagate even when a report exists, including trials. The gate's exit status now enforces its documented different-refresh requirement. Inert-program regressions reproduce the false successes without using a compositor, display or graphics device; trials remain labelled as trials. Its output points to the current decision process.
- **Panels, bars and images retain their own screen-part bindings.** Their drawing Components now bind to their creation context, completing the five screen kinds. Image readiness uses the actual Image type; a malformed local image becomes not ready without affecting a second instance. All 48 scene cases pass in Release and under sanitizers, and six selected screen-play functions pass (8 Qt cases). Full QQ lint falls from 382 to 347 diagnostics with none added; the zero-warning gate still fails.
- **Software gameplay checks wait for the game's time and clean up failed controller cases.** The remaining physics, controller, trigger, impulse and Orb Run movement waits use the existing game-clock helpers in software; native waits, conditions and tolerances are retained. The fake controller is removed even after an early assertion failure. A clean baseline reproduced all 11 failures; the corrected full play suite passes all 53 cases without skips. Scene drawing coverage now exercises panels, bars and project images as separate instances, including edits and scaling; all 48 scene cases pass in Release and under sanitizers.
- **Agent status and key-storage problems are literal text.** Both labels explicitly use plain text, closing the gap left beside the already-literal message list. Two real-panel regression rows with inert state first reproduced automatic rich-text mode, then pass with supplied markup and later status edits; they use no provider account, network request or keychain operation.
- **Linux product suites stay sequential on first attempts and retries.** Both CTest commands explicitly use `--parallel 1`, overriding an inherited `CTEST_PARALLEL_LEVEL`. Real-CTest stand-in regressions first reproduced overlapping suites on each path, without running Qt or touching real hardware.
- **Game-screen drawings retain their owning component context.** ScreenText and ScreenButton bind their nested drawing Components explicitly; button clicks use the owning Scene's queue. Independent instances, edits and overlay scaling pass in all 45 scene cases in Release and under sanitizers, and six selected screen-play functions pass (8 Qt cases including setup/cleanup). Full QQ lint falls from 429 to 382 diagnostics with none added. The broader play suite's 11 failures reproduced on a clean baseline and were subsequently resolved by the test clock/cleanup corrections above.
- **QQ's README points to QOR OS's current documents.** Broken decision/blueprint links and the obsolete dated status summary are replaced with the current Qt rule, QOR-0015, architecture, roadmap and owner checkpoint. No deleted upstream document is restored or used.
- **Product CI change detection fails on Git errors and handles large diffs.** Windows checks fetch and diff exit codes immediately; Linux separates diff collection from matching and consumes the whole output. Regression cases reproduce a masked Windows fetch failure, a masked Linux diff failure and a large Linux diff incorrectly reported as unchanged. The decision step now says only whether products were requested, instead of claiming tests already ran. No runner boundary or enablement setting changes.
- **Native QML lint now fails on warnings.** The root `.qmllint.ini` sets Qt's `MaxWarnings=0` without changing diagnostic categories or severity. Both CI platforms explicitly lint changed products sequentially and stop on native failure; changing the policy triggers the products. Real Qt tests cover warnings, clean input and syntax errors, and extracted workflow commands exercise failure propagation. QQ's unchanged 429 diagnostics now make its native lint commands fail; they have not been hidden or fixed by this gate change.
- **Game screen labels show supplied text literally.** ScreenText and ScreenButton explicitly use plain text instead of Qt's automatic rich-text detection. Two real-QML regression rows reproduced the old format, then verify literal markup, independent instances and later edits; button capitalization stays as before. The scene suite passes 45 cases in Release and under AddressSanitizer/UBSan. No private file, image fetch or real network request is used by the tests.
- **QQ declares the QML module that owns its C++ base types.** `DEPENDENCIES QtQuick3D` lets tooling resolve the base types of SoftDot and TileGeometry without adding QML auto-imports. Full QQ lint falls from 431 to 429 diagnostics with none added; the spatial-audio metadata findings remain separate.
- **Windows product builds run the registered suites and require evidence.** Scanning `tst_*.exe` could report success with no executable found and could not run registered tests that are not executables. All three products now use CTest through `products-tests.ps1`; empty runs, failures, whole-suite skips and missing/mismatched results fail. Old test records are invalidated before building/testing, and a cleanup failure stops the run. Suites stay sequential even with inherited CTest parallelism. Real CTest with stand-in builds covers these paths without opening product windows. Qt's Windows setup puts executables in the build root; no claim is made that earlier Windows runs missed all QQ suites.
- **The Windows product wrapper no longer reports a failed child as successful.** It checks nonzero script/native exit codes as well as exceptions, retries them, and clears an inherited exit code before each attempt. Six PowerShell regression cases use stand-ins, including a later success and continuing the other products after failure; no real product window is opened by these checks.
- **QQ runtime identifiers no longer shadow Qt properties.** The Player's body material and the TileMap root have distinct private ids, preserving their public properties and bindings. Two scene checks verify every body part follows colour changes and tile geometry follows the map, layer and scale. All 43 scene cases pass before and after the rename and under AddressSanitizer/UBSan; full QQ lint falls from 435 to 431 diagnostics with none added.
- **Linux product validation requires evidence from the current attempt.** A zero CTest exit without a complete matching log and record no longer passes, including on the first attempt. Old test records are invalidated before configuration, building and each attempt; a cleanup failure stops the run. A whole suite skipped by CTest remains unrun and cannot pass the product. Regression tests exercise no-op CTest, failed configuration/builds, inaccessible records and whole-suite skips with real CTest.
- **QQ Studio's layout owns its marker and colour-swatch sizes.** Their existing dimensions are expressed as preferred layout sizes, removing four warnings without changing the requested sizes. The 4 November comparison against `17ef113` has no new QQ lint findings; 435 baseline warnings remain.
- **Prism no longer starts a product into Qt Multimedia's PipeWire crash.** Where no PipeWire daemon runs (as under WSL) and nothing chose an audio backend, Prism starts every product with `QT_AUDIO_BACKEND=pulseaudio`, as `tools/dev/products.sh` does. Qt Multimedia's PipeWire backend crashes there, and QQ Studio and the QQ Player both reach it through `QtQuick3D.SpatialAudio`. Tested in `tst_producthost` each way, with planted faults.
- **Wayland clients in Prism heard no pointer motion until a button was down.** A hover handler above a window's content kept every hover from it. An unfocused window's controls light up over a client's content again.
- **A window opening while another closed was lost in Prism.** Counting a client's windows threw on the one closing.
- **QQ Studio's 3D view shows each sky it is given.** Qt Quick 3D keeps a light probe by its texture data and works out its lighting only once, so the Studio's long-lived world kept its first sky on any machine; it now makes new sky data once after a colour changes, and draws nothing while nothing changes (`tst_gamebuild`, by the Studio's pixels, in software).
- **A QQ Studio closed by its launcher with unsaved work no longer stops to ask with nobody there:** Qt 6's quit closed the window first. The work stays in its recovery copy.
- **QQ Studio saves where it opened.** A scene opened from `scenes/Level.qml` saves back to it; logic saves to the file that is open and over no other; nothing is saved through a link that leads away.
- **A project whose only code is in `prefabs/` is asked about** before a prefab of it runs.
- **`tst_agent` and `tst_publish` no longer add temporary folders** to a person's recent projects.
- **Ever Rift's test pilot no longer paces for ever.** It chose what to walk to by eye, so something near but behind a wall led it off until something else looked better and led it back. About one run in 300 stood still and failed `tst_games` after four minutes. It now chooses by the length of the walk, and `ever-rift.test.mjs` plays each seed where it paced.
- **QQ Studio: saving in, or importing into, a project not trusted now waits for the trust question.** A scene saved there was built among the stranger's files from then on, so Undo could run what stood beside it.
- **The QQ Player's kernel wall left three routes to a network socket open.** Its filter let every call from another architecture through, so a 32-bit `int 0x80` was not looked at, and it missed x32's numbers and `io_uring`, which makes a socket with no call to filter. Reaching them takes native code, which a game cannot bring, so this mattered only to what got past QQ's own checks, which is what the wall is for. All three are refused now, and `tst_playerwall` tries each from behind the wall, `AF_VSOCK` too. Found by reading the wall against what the runner work measured.
- **What sits on an accent fill was black, not Lumen's light ink, and in Prism the products never had the person's colours.** QML reads a binding named `onAccent` as a signal handler, since `accent` is a property too, and keeps no value. So `Tokens.color.onAccent` was undefined and `Theme.onAccent` an invalid colour, drawn black: the labels of primary buttons, check marks, radio dots, highlighted menu items, Control Center's tiles, the lock screen's button. And `Theme.productColors` threw on it, so Prism handed `ProductHost` nothing and [QOR-0016](docs/decisions/QOR-0016-the-products-wear-lumen.md)'s accent, tint and contrast stopped short of the products inside Prism. Both are aliases now, which are no bindings, and `Theme.onAccent` keeps its name for the Store's signed calendar widget.
  - It lasted because an invalid colour has an alpha of 1, which was all `tst_kit_qml` asked of a role; because a warning in a suite that passes is not shown; and because the black was 0.21% of the golden images, inside the 0.3% they allow, and their references had it too. `tst_kit_qml` now asks that every role be valid and fails on any warning, `prism_products` checks the look Prism hands over, and the five `controls` references are drawn again.
- **A Build no longer fails everywhere because the laptop's display timed out.** The Windows steps that draw go through `tools/ci/with_display.py`, which holds the display on, wakes the machine if it is asleep, and if it cannot draw says so and runs nothing ([docs/CI.md](docs/CI.md)).
- **`tst_games` crashed where it should have failed.** `a_rematchs_lobby_closed_before_anyone_joins` read the match the cases before it played, and without one read through a null pointer, which took with it the cases after it.
- **QQ Studio's Agent panel did not import `QQ`,** so `Look`, where its colours and faces come from, was undefined: the key's problem and the run's steps were drawn uncoloured and the tool steps not in the mono face, and the Studio printed `ReferenceError: Look is not defined`. Of the names qmllint flags as unqualified in the Studio's QML, it was the only one that does not exist when the Studio runs; the others are ids and delegates' roles. QQ's Studio and agent suites now fail a case on any warning from QQ's own QML (`QTest::failOnWarning`); without the import, the agent suite's two Agent panel cases fail.
- **QQ Studio's F3 overlay, Group and Duplicate cases failed on Linux.** They pressed their shortcuts as soon as the Studio's scene existed, before its window had the keyboard, which a compositor may give only once the first frame is drawn: in software, seconds later. A window shortcut answers only in the window that has the keyboard, so the keys did nothing. The suite's Studio now waits for the keyboard (up to 15 seconds) before a case begins, and all three pass on the run's own display. On Windows and in Prism, which gives the keyboard when the window is created, they passed already.

## QTOS after 0.1.0 (unreleased, carried into QOR OS on 26 October 2026)

### Added

- **Sound, everywhere.** QTOS plays real audio through the machine's outputs ([docs/AUDIO.md](docs/AUDIO.md)).
  - The new audio engine (`audio/`, `qtos_audio`) uses Qt Multimedia's real-time output callback, with lock-free hand-off to the audio thread, click-free fades, a perceptual volume taper and a soft limiter.
  - **QTOS Sessions**: original music generated live and deterministically by a small synthesiser (band-limited oscillators, state-variable filters, synthesised drums, ping-pong delay, reverb) in four styles. The five scenario tracks are sessions.
  - **Your music**: files in the Music folder and `QTOS_MUSIC_DIR` (MP3, FLAC, Ogg, Opus, WAV, AAC, WMA, AIFF) are decoded with `QAudioDecoder` and queued after the sessions.
  - **Five-band equaliser** with seven presets, and a response curve drawn over the live spectrum in Settings.
  - **Output devices** follow the system: real devices are listed, hot-plugging is tracked, and the chosen output is remembered.
  - **Interface sounds** (`Sounds` in `QTOS.Kit`): notification, lock, unlock, volume and connect cues, synthesised by `tools/sounds/generate_sounds.py`, with their own switch and volume.
- **Audio-reactive interface.** The wallpaper's light swells with the music and flares on beats, the Live Activity shows the real spectrum, the Music artwork breathes on the beat, and Music and Control Center gain visualisers.
  - Settings › Sound adds **React to sound**, an **Intensity** slider, and the frequencies it **Reacts to**: presets (Bass, Vocals, Highs, Full range) or any range on a logarithmic range slider over the live spectrum. A response meter shows the energy and beats.
  - The analysis (48-band spectrum, loudness, range energy, beat detection) runs in the engine and reaches the interface as the `Audio` service's `analysis` signal; `Reactive` (`QTOS.Services`) smooths it at the display's frame rate. Reduce Motion stills it.
- Service contracts: `Audio` gains `outputAvailable`, the equaliser, interface-sound settings, `active` and `analysis`; `Media` gains `buffering` and queue entries with `kind`, `session` and `path`; `Settings` gains `audioReactive`, `reactiveIntensity`, `reactiveLowHz` and `reactiveHighHz`.
- Kit `Spectrum` and Lumen `RangeSlider`.
- `qtosctl media seek`, `eq`, `sounds`, `reactive` and `spectrum` (a live spectrum in the terminal); `status` shows the output, EQ, reactive settings and play position.
- `qtos-simd --silent`. Automated runs (`--screenshot`, `--benchmark`, `--script`) and every test use the engine's null output, which renders in real time to nowhere.
- `qtos-shell --open settings/sound` opens an app at a page.
- **Controller interface.** QTOS can be driven entirely with a game controller ([docs/CONTROLLERS.md](docs/CONTROLLERS.md)).
  - When a controller connects, QTOS asks whether to switch (South: switch; East: not now; "always switch automatically" remembers). When the last controller disconnects, QTOS returns to keyboard and mouse.
  - The left stick drives a Lumen cursor; South clicks and West right-clicks; East goes back; the D-pad moves focus. North opens Beam, Start Control Center, Back Overview, Guide home or unlock. The shoulders switch Spaces, the triggers set the volume, and the right stick scrolls.
  - An on-screen keyboard opens for text fields. A hint bar shows each button's job with the glyphs of the controller in use (A B X Y, or Cross Circle Square Triangle). PlayStation light bars follow the accent colour. Selections give a light vibration.
  - Native backends with no third-party libraries:
    - XInput, including the Guide button: Xbox controllers, SteelSeries Stratus Duo and other XInput pads.
    - Raw HID: DualShock 4 and DualSense over USB and Bluetooth, with rumble, light bar and battery; generic HID pads.
    - Linux evdev, with hotplug.
  - The same controller seen twice (Steam Input) never acts twice.
  - Settings › Controllers: the interface, auto-switch, cursor speed, vibration and light bar; connected controllers with battery and Identify; a live tester of every button, stick and trigger; and the button map.
  - `qtos-gamepads` lists controllers and streams their state. `tools/dev/uinput-gamepad.py` creates a kernel game pad on Linux.
  - Shell scripts gain a virtual controller (`gamepad` steps). `tst_input` covers reports, CRCs, the mappings, dead zones, the manager and the navigator, and `shell_e2e` the whole controller flow.
- **Music library.** Songs, Albums and Artists views, and QTOS Sessions and Music Folder collections with real counts (the placeholder playlists are gone). Play and Shuffle buttons start a view; the playing song shows a live spectrum.
- **Shuffle and repeat** (`Media.shuffle`, `Media.repeat`): shuffle plays every track once per pass; repeat cycles through all, one and off. Both persist, and `qtosctl media shuffle|repeat` sets them.
- **Media and volume keys** everywhere, including the lock screen, with a glass volume indicator (`VolumeHud`).
- Lucide icons `repeat-1`, `disc-3`, `mic-vocal`, `list-music`, `library`, `audio-lines` and `gamepad-2` (113 in all).
- On Linux without a running PipeWire daemon (WSLg), QTOS asks Qt Multimedia for its PulseAudio backend, avoiding a crash in Qt 6.12's PipeWire backend.
- Shell scripts: a `scroll` step; the state includes `audio`, `media` and the reactive settings; item targets are found among `Repeater` delegates and must not be clipped or scrolled out of view.
- Tests: `tst_audio` (24 cases), playback and analysis over Qt Remote Objects in `tst_services`, and the Sound page in `shell_e2e`.
- **Increase Contrast**, a new accessibility setting.
  - It is available through the `increaseContrast` setting, in Settings → Accessibility, as a Beam action, as `qtosctl contrast on|off`, and as `qtos-shell --contrast high`.
  - Roles can define `highContrast` token values, which `Theme.contrast` blends in OKLab on a spring.
  - Targets: 7:1 for primary, secondary and accent text; 4.5:1 for tertiary text and status colors; 3:1 for focus rings.
  - Separators, strokes and control fills strengthen, and glass becomes solid (`Theme.solidMaterials`).
  - The Widgets palette follows `QTOS_CONTRAST=high`.
- **Luminance-capped tokens.** `maxLuminance` lowers lightness until a role's WCAG luminance fits under the cap. White text on `accent` and `accentHover` therefore stays at least 4.5:1 at *every* accent hue, which OKLCH lightness alone cannot guarantee. Implemented in `color::capLuminance`, mirrored in the token generator.
- **Golden-image tests (`tst_golden`, label `gpu`).**
  - Covers Lumen controls, Kit components, and wallpaper with backdrop and glass, each in light, dark, high-contrast and RTL variants.
  - Comparison is a perceptual per-pixel OKLab diff with failure heatmaps, plus an `update_golden` target. CI uploads the diffs when a case fails.
  - One reference set serves Windows (Direct3D 11) and Linux (OpenGL), which differ on under 0.01% of pixels. Per-OS overrides are optional.
- Token tests that enforce the WCAG contrast guarantees.
- `qtosctl transparency on|off`.
- **Prism runs real Wayland clients** (verified nested in WSLg with `weston-terminal` and a Qt Widgets app).
  - Server-side decorations for `xdg-decoration` clients; none for clients that draw their own.
  - Client window geometry is aligned with the frame (shadows clipped, never scaled), and the frame adopts the client's chosen size.
  - Activated and maximized states are sent; client maximize and minimize requests are honoured.
  - Moves started from a client's own title bar use Prism's springs and edge snapping.
  - Clients are configured for the target size, not every frame of a resize animation.
  - New `prism` options: `--socket`, `--size`, `--surface`, `--screenshot` and `--delay`. Prism prints the socket clients should use.
- **Wayland protocols in Prism**, verified on the wire by `tools/dev/prism-protocol-check.sh`.
  - Prism implements `wp_fractional_scale_v1` and `wp_cursor_shape_v1` (v2) itself, with protocol XML vendored from wayland-protocols 1.47.
  - Qt's `wp_presentation` is enabled, fed with swap timestamps, and works around a Qt 6.12 initialisation bug for the QML element.
  - Qt's idle-inhibit and text-input-v3 are enabled.
- Linux on WSL2 build and run guide (docs/BUILDING.md).
- **Shell scripts and end-to-end tests.**
  - `qtos-shell --script` and `prism --script` drive the shell with synthetic input through `QWindowSystemInterface` (clicks, drags, keys, shell calls), and assert with `expect` steps.
  - `shell_e2e` covers first-party apps on every platform. `prism_e2e` covers real Wayland clients (52 checks), with the new `prism-test-client`.
  - Documented in docs/DEVELOPMENT.md.
- **Wayland app identity.** The Shelf names and draws client apps from their desktop entry (`DesktopEntries`, with XDG icon-theme lookup), falling back to the window title.
- **Resizing from every edge and corner** (eight grips; left and top edges keep the opposite side in place). Client-initiated moves and resizes (`xdg_toplevel.move`/`resize`) go through the same springs, snapping and size limits. `WindowManagement.takeOver()` detaches Qt's built-in xdg-shell handlers, which otherwise fought Prism over the window size.
- **xdg-activation with focus-stealing prevention.** Tokens issued while the requesting client has keyboard focus (or by Prism, for launches) activate the window. Any other request only marks it as wanting attention: a Shelf hop and an accent dot, cleared on focus.
- **Client size limits and the resizing state.** Interactive resizing respects a client's minimum and maximum size, and clients receive `xdg_toplevel` `resizing` while it lasts.
- **Benchmark v2**, shared by `qtos-shell` and `prism` (`prism/shell/shellbenchmark.cpp`): `prism --benchmark` reports per-output frame pacing, missed frames at each output's refresh rate, and input-to-swap latency. Top-level JSON fields are unchanged. Baselines: input to swap 5.9 ms p50 on the RTX 4060 (360 Hz); nested Prism under WSLg holds 60 Hz. This is the tooling for the ADR-0003 decision gate, which needs a bare-metal run.
- **Several displays in Prism** ([SHELL.md](docs/design/SHELL.md#displays)).
  - One Wayland output per display: `prism --outputs N` nested; on bare metal (eglfs/KMS) every connected screen. The main display keeps the whole shell; the others show the wallpaper and their windows (`DisplayDesktop`).
  - Move windows between displays with the window menu, <kbd>Meta</kbd>+<kbd>Shift</kbd>+<kbd>←</kbd>/<kbd>→</kbd>, or by dragging the title bar past a display's edge. Spaces, Overview and the lock screen span all displays, and one window is active across them all.
  - Clients are on the output of the display that shows them, so frame callbacks follow them. In-window frost samples each display's own wallpaper (`Materials.desktopFor`).
  - `--spread-clients` opens new clients on the emptiest display. Keyboard shortcuts work from every display.
  - **The decision-gate kit**: `tools/dev/prism-gate.sh` runs Prism on eglfs/KMS with an animating client per output, benchmarks it and judges the result against the gate's criteria. It finds the GPU that has both displays, offers a probe (`--probe`), a trial inside a desktop session (`--try`) and per-display modes (`--mode`), and packs the results for upload. `tools/dev/prism-gate-bundle.sh` packs Prism, the test client and their Qt runtime (about 100 MB) for a stock Ubuntu desktop, so a live USB session can run the gate without a Qt kit or a build. The benchmark keeps every output drawing while it measures (single-output numbers unchanged).
  - `prism_e2e` adds a two-display run (menu, shortcut, edge drag, Spaces across displays) and a two-output benchmark: 52 checks. Shell scripts reach windows on any display and report each window's `display` and the number of `displays`; screenshots take a `display`.
- **The QTOS Store, stage 1** ([docs/STORE.md](docs/STORE.md)).
  - A new **Store** app: Discover, Themes and Widgets, search, item pages with live previews, and a Library with updates. Apps lists what comes with QTOS. Games, Tools and Music arrive with the developer program, and the app says so.
  - Eight **themes** (Ember, Tidepool, Moss, Dusk, Paper, Graphite, Saffron, Glacier). A theme sets the accent, a preferred appearance, a tint for every neutral surface and the Aurora wallpaper's character. Choose one in the Store, Settings › Appearance or Beam.
  - **Desktop widgets** on a grid over the wallpaper: Clock, Calendar, Now Playing, System, Focus and Visualizer. Add them from the Store or the gallery (right-click the desktop › Edit Widgets, or Beam), drag them between cells and remove them. The layout persists.
  - **Every install is verified.** The store signs the catalogue, the developer signs each package's manifest, and every file is hashed. Signatures are ECDSA P-256, checked by CNG on Windows and OpenSSL elsewhere. Installs are staged and swapped in whole.
  - `QTOS.Store` (`StoreEngine`, `Store`, `Themes`, `ThemePreview`); `tools/store/build_store.py` builds, signs and checks the catalogue (CI checks it); `tst_store`, `store_widgets_qmllint` and a Store section in `shell_e2e`.
  - `Settings` gains `theme` and `widgets`; `Intents` gain `widgets.add` and `widgets.edit`; `Theme` gains `neutralHue`, `neutralTint`, `wallpaperSeed` and `wallpaperIntensity`, and `Oklch.themed` optional neutral arguments (Aurora's colours are unchanged). Shell scripts report `store`, `editingWidgets` and `desktopWidgets`.
  - Lucide icons `shopping-bag`, `badge-check`, `wrench` and `timer`.
- **Three-finger gestures** ([SHELL.md](docs/design/SHELL.md#gestures)). Swipe sideways between Spaces, and up and down for Overview, on a touchpad or a touchscreen. The desktop follows the fingers, and the springs carry on with their velocity.
  - Overview opens progressively: windows sit between their places and the Overview grid as far as the fingers go.
  - `SystemGestures` (`QTOS.Input`) takes swipes of three or more fingers for the whole window, before apps and Wayland clients. Touchpad swipes arrive as Qt's native gestures (the host's pointer gestures under nested Prism). Touchscreen swipes come from touch points, and the apps under the fingers get a touch cancel.
  - Windows reports no touchpad gestures to apps, so QTOS reads precision touchpads through Raw Input (`QTOS_TOUCHPAD=off` turns this off; automated runs do).
  - Shell scripts gain `gesture` and `touch` steps on a virtual clock. The state includes `spaceOffset`, `overviewProgress` and `gesture`, windows their `overviewMix`, and `expect` compares single numbers with `near`. `tst_input` covers frame assembly, swipe tracking and recognition; `shell_e2e` and `prism_e2e` cover the shell.
- **Spaces for every window, Wayland clients included** ([SHELL.md](docs/design/SHELL.md#spaces)).
  - Windows ride their Space along the strip, and windows on Spaces out of view are not drawn.
  - Take the active window along with <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>Shift</kbd>+<kbd>←</kbd>/<kbd>→</kbd>, <kbd>Alt</kbd>+<kbd>Shift</kbd>+<kbd>1</kbd>…<kbd>3</kbd> or Beam's "Move Window to Space".
  - Send any window away from its new title-bar menu (right-click: Minimize, Maximize, Move to Space, Close), or drag it onto a Space chip in Overview.
  - Clients on Spaces out of view leave the output and get no frame callbacks, so they stop drawing. `prism_e2e` checks this with an animating client (`prism-test-client --animate`).
  - Shell scripts gain `rightClick` steps. The window state includes `visible`, `commits` and the surface's `onOutput`, and the state includes the shell's `frames`. `prism-e2e.py` keeps each client's output next to Prism's log.

### Fixed

- **Clicking an icon button in a window that overlapped another raised the window underneath.** `IconButton`'s `TapHandler` takes only a passive grab, so the press reached the activation watcher of every window below. Only the topmost window at the press now activates. `shell_e2e` checks it.
- **Automated runs could talk to a QTOS session in use.** On Windows a named pipe accepts several servers, so a second simulator "hosted" the default endpoint alongside a running session, and clients could reach either. The simulator now probes the endpoint first, and `--script`, `--screenshot` and `--benchmark` runs use a private one.
- On Linux the keyboard's play/pause key (delivered as Media Play) only ever started playback; it now toggles.
- **A Wayland client that kept drawing during a resize could pull its window back to an earlier size.** Prism took any committed size other than the latest configured one as the client's own choice, including frames drawn for an earlier configure. It now adopts a client's size only once the client has acknowledged the latest configure (`WindowManagement.configureAcked`). Found by `prism_e2e` with an animating client.
- **Scripted tests could be disturbed by real input.** A real pointer motion with no button held (a person's mouse, or WSLg's pointer crossing a nested Prism) cancels Qt Quick drags, the likely reason `prism_e2e`'s edge resize occasionally stopped part-way. While a script runs, only its own input and the controller interface's (`qtos::input::InjectedInput`) now reach the window.
- **Synthetic keys lost their modifiers inside Prism.** QtWaylandCompositor rebuilds every key event from its XKB state, which only real modifier key events change. Shell scripts' <kbd>Ctrl</kbd>+<kbd>Alt</kbd> shortcuts, the controller's <kbd>Shift</kbd>+<kbd>Tab</kbd> and capitals from the on-screen keyboard therefore arrived without modifiers. Synthetic key strokes (`qtos::input::sendKeyStroke`) now press the modifier keys around the key, as a keyboard does, and carry evdev codes for US punctuation and F1–F12.

- `tools/dev/run.ps1 -- <shell options>` passed the first shell option as `-Preset` (so `run.ps1 -- --unlocked` failed with "not built for preset"). `-Preset` is now named-only.

- **Mouse presses never reached Wayland clients, nor item-based controls inside app windows** (Qt Quick Controls, text fields). AppWindow's activation overlay used a `TapHandler`. In Qt 6.12 that leaves a mouse press accepted (QTBUG-145896), so Qt Quick stopped offering it to items underneath. Activation now uses the overlay's `PointHandler`. Found by the new end-to-end tests.
- A minimised Wayland client could not be restored: `WindowLayer.open()` consulted the first-party app registry before looking for an existing window.
- Wayland clients' app ids were lost: they are set just after the toplevel is created, and only the title was followed.
- New windows could cascade above the work area, under the Rail.
- Default dark-appearance accent: white text on it was 2.9:1. The accent is now a deeper indigo (OKLCH L 0.575) and meets AA. Accent hover darkens instead of lightening, so text keeps its contrast while hovered.
- CI Qt manifest: Qt 6.8+ add-on IDs are architecture-independent. The old per-kit IDs are virtual, and the installer skipped them while still reporting success. `install_qt.py` now verifies the expected CMake packages afterwards.
- `install_qt.py` installs additional packages through the existing installation's MaintenanceTool, since the online installer refuses a non-empty root. It also accepts an existing Qt Account sign-in (`qtaccount.ini`).
- Removed three unused QML imports flagged by qmllint.
- `prism` failed to load on Qt 6 (`ShellSurfaceItem.sizeFollowsSurface` no longer exists). It was never built before: this is its first run.

### Changed

- The scenario no longer starts playing on its own: press play.

- CI: the Linux job runs on a self-hosted WSL2 runner with the commercial kit. With warnings as errors (as on Windows), it runs the GPU suites on WSLg and the Prism protocol check with real Wayland clients; about 40 s incremental.
- CI: a newer push cancels an older Build run on the same branch, so a run queued for an offline runner no longer blocks later builds. GPU suites check for a display, time out after 120 s and retry once.
- CI: per-platform Qt sources (`QTOS_WINDOWS_QT_DIR`, `QTOS_LINUX_QT_DIR`), so a Windows self-hosted kit no longer affects the Linux job. Self-hosted builds are incremental. Qt's bundled CMake and Ninja are put on `PATH` automatically. The optional `QTOS_RUN_GPU_TESTS` runs GPU suites. Build also runs on `v*` tags.
- CI: the installable Windows artifact is uploaded only for tags and manual runs (7-day retention), to stay within the Free-plan storage quota.
- pre-commit: vendored fonts and icons and generated token files are excluded from all hooks. Multi-document YAML (Qt Application Manager manifests) is allowed. clang-format skips the SDK template, matching CI.
- Developer setup uses a repo-local `.venv` for the pinned tooling.
- `QT_QML_GENERATE_QMLLS_INI` is a cache option, so a second build of the same checkout (for example a WSL build of a Windows working tree) can turn it off without repointing the editor.

## [0.1.0] - 2026-10-23

The Phase 0 foundation: the complete Prism shell running in simulation on
Windows, with the Lumen design system, simulated system services over Qt
Remote Objects, six first-party apps, platform integration for Qt Widgets, and
a full development and CI setup.

### Added

- **Lumen design system (QTOS.Kit).**
  - Single-source OKLCH design tokens (`kit/tokens/lumen.tokens.json`), generated into QML, C++ and docs.
  - Live `Theme` with OKLab-interpolated light/dark and accent-hue springs.
  - `Spring`: a closed-form damped-harmonic animation driver.
  - SDF `Surface` (continuous-curvature shape, gradient, hairline, sheen, analytic shadow).
  - `Glass` and `Backdrop`, with one shared blur per frame for every glass panel.
  - `RoundMask`, the procedural Aurora `Wallpaper`, and GPU-tinted `Icon`.
  - `AppIcon`, `Artwork`, `QtosMark`, and app building blocks: `SidebarItem`, `Card`, `SegmentedControl`, `SearchField`, `Kbd`, `Divider`, `IconButton`.
  - `FrameStats`, a passive frame-pacing probe.
- **Lumen Qt Quick Controls style** (`Lumen` module): 23 controls on Qt Quick Templates, with Basic fallback.
- **Prism shell (QTOS.Shell).**
  - Lock screen with a focus-pull unlock.
  - Rail with the Live Activity capsule and status cluster.
  - Shelf with magnification and launch hops.
  - Beam command bar: fuzzy search, safe calculator, preview pane.
  - Control Center, notification banners and history, and the Media popover.
  - Spring-driven window manager: 1:1 drag, magnetic snapping, resize, minimise-to-Shelf, maximise.
  - Three Spaces with parallax, and an Overview grid with Space switcher.
  - Keyboard shortcuts throughout.
- **System services.**
  - Nine Qt Remote Objects contracts (`services/interfaces/*.rep`).
  - Replicas typed for QML (`QTOS.Services`).
  - The `Intents` app→system API.
- **Simulator (`qtos-sim`, `qtos-simd`)**: scenario-driven Wi-Fi, Bluetooth, battery, audio, display, media, notifications and persisted settings.
- **Apps (QTOS.Apps)**: Files, Music, Notes, Terminal, Monitor (Qt Graphs) and Settings (8 pages).
- **Tools.**
  - `qtosctl` CLI.
  - `qtos-shell` options: `--benchmark` (frame pacing as JSON), `--screenshot`, `--gpu`.
  - Token generator, Lucide icon importer, screenshot capture, developer PowerShell scripts.
- **Platform**: Lumen `QStyle` plugin for Qt Widgets, the `qtos` QPA platform theme plugin, and `lumen-widgets-gallery`.
- **Prism compositor** (`prism`, Linux, experimental): Qt Wayland Compositor host for the same shell.
- **Image**: `meta-qtos` Yocto layer skeleton and kas configuration (Phase 2).
- **Quality.**
  - 6 test suites: springs, OKLCH, the QtRO round trip, Beam search, Kit QML, and the token generator.
  - Zero-warning `qmllint`, pinned clang-format, SPDX header check, and pre-commit.
- **CI**: lint workflow, plus Windows and Linux build workflows using commercial Qt via secrets or a self-hosted kit.
- **Docs**: vision, architecture, building, development, CI, performance, roadmap, Lumen and shell specifications, service reference, Qt module map, and ADRs 0001–0010.

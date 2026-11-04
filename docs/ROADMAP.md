# Roadmap

The only roadmap for QOR OS. Each phase ends at its **exit**, shown working,
not on a date; durations are rough guides for one founder working with agents.
What 1.0 contains is [QOR-0007](decisions/QOR-0007-what-1-0-ships.md). The
phases are tracked as GitHub milestones.

QOR OS began on 26 October 2026 from QTOS's Phase 0 and most of its Phase 1
([history below](#history-qtos-phases)) and from Demiurge's device-side
products ([UPSTREAM.md](UPSTREAM.md)).

## The order of work

The owner's milestones ([QOR-0021](decisions/QOR-0021-projects-engines-and-two-stores.md)). The phases and sections below say
what each holds. A step is done when it is integrated and its checks have evidence, kept to the configuration they ran on.

1. **The real baseline.** Checked on 1 November against an outside review of `a3a2fd4`: the repository was public against its
   documents, and is now private ([SECURITY.md](../SECURITY.md#known-limits)); Linux CI skipped the products; `image/` is a plan
   with nothing that builds; Unreal is a decision, not code; and Epic changed its clause on AI, which QOR-0020 missed
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
- [ ] Touchpad gestures from libinput for the KMS backend
- [ ] Real daemons for every service contract (`qor-netd`, `-btd`, `-powerd`, `-audiod`, `-notifyd`, `-settingsd`) and an xdg-desktop-portal backend with prompts in the trusted layer
- [ ] Games and apps sandboxed (bubblewrap, seccomp, Landlock): the operating-system half of Demiurge's ADR-087

## Phase 4 · The ecosystem

*Ongoing.* Exit: an outside developer ships a QQ game to QOR OS using only the public
SDK.

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
| QQ, native Linux | blocked: machine | blocked: machine | blocked: machine | blocked: machine | blocked: machine | blocked: machine | blocked: machine | blocked: machine | blocked: machine |
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
  The Store's listing provenance reaches the lock whole within its existing 8 MiB cap (`8b77c01`). On 4 November, Release and
  AddressSanitizer/UBSan `tst_assetlibrary` each passed 89 cases, none failed or skipped, including the opt-in largest-pack
  measurement; Release `tst_studio` passed all 25. The unchanged recovery code's sanitizer build is preserved at `850a639`.
  The 3 November sanitizer `tst_gamebuild` passed all 195 cases; it was not rerun on 4 November. These use WSL software rendering
  and the real StoreEngine's demonstration pack. Listing and planning still run on the window's thread: the Release largest-pack
  run measured 257 ms and 402 ms respectively, without establishing a latency budget. Native Linux, a GPU and Windows still need
  their own evidence. QQ's full QML lint has 429 diagnostics after the runtime-id batch and the Qt Quick 3D C++ dependency
  declaration; that comparison removed two with none added against 431. Plain-text game labels bring the scene suite to 45
  passing cases in Release and under sanitizers, without changing lint. The remaining spatial-audio type metadata warnings
  reproduce in a Qt-only lint probe. Native CMake lint now enforces zero warnings through `.qmllint.ini`; all three QQ module
  commands fail with the same 429 diagnostics. The zero-warning requirement remains unmet.
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
- [x] The repository private, as its documents always said (1 November; it had been public since 27 October, while CI ran on
  the owner's machine)
- [ ] CI that cannot reach the owner's keys or credentials ([CI.md](CI.md#what-ci-trusts)). Build's workstation jobs fail closed
  until their runner is isolated (pull request #2, merged 1 November 2026), both runners are stopped, and Release readiness fails
  while a required build did not run (pull request #3, for the owner to merge)
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
- [ ] A public way to report a security problem: this repository's form reaches only its members, and demiurge-chain's private vulnerability reporting is off
- [ ] Every required build and test run and passed on the commit released, as Build's Release readiness says
  (`tools/ci/release_readiness.py`). That includes the products' suites on a graphics card with nothing eased, on Windows and on
  a Linux machine, since what software eases ([CI.md](CI.md#what-linux-runs)) proves nothing of what it leaves out. Not yet: Build
  is paused, and no job runs the products on a Linux graphics card

## Release deliverable · The image

[QOR-0021](decisions/QOR-0021-projects-engines-and-two-stores.md), decision 12. **Nothing builds yet:** [image/](../image/README.md) is a plan.
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

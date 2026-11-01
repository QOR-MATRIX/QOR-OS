# QOR-0021: Projects, engines and two stores

- **Status:** Accepted
- **Date:** 2026-11-01
- **Deciders:** the owner, in their engineering brief of 1 November 2026 and their next-actions direction of the same day,
  which revised this record before it landed. Where they leave the way to the assistant as lead engineer, the way is the
  assistant's.
- **Amends:** [QOR-0007](QOR-0007-what-1-0-ships.md) (1.0 gains what decision 10 says) and
  [QOR-0020](QOR-0020-unreal-engine-on-qor-os.md) (its order, and its Store check, by decisions 1 and 5)
- **Relates to:** [QOR-0006](QOR-0006-qor-os-1-0-moves-no-money.md), which stands;
  [QOR-0017](QOR-0017-arqades-new-games.md); [QOR-0018](QOR-0018-the-security-and-quality-bar.md);
  [QOR-0022](QOR-0022-assistants-and-unreal-engine-after-epics-change.md) (proposed); [STORE.md](../STORE.md)

## Context

The owner's brief sets what QOR OS is for: playing, designing, developing, building and distributing games, for players,
game developers and game designers. It adds what the plan did not have: a desktop organised around projects, Open, Build and
Test for Unreal and Unity beside QQ, two stores, optional apps, and console capture. It asks for these in dependency order,
each shown working, reconciled with what exists. A custom kernel, custom drivers and a boot-time edition choice are not part of
this work; the Linux foundation and its reliability stay the milestone.

**What the code shows, at `a3a2fd4`:**

- **QQ Studio remembers nothing of a project between runs.** It takes `--project` and guards unsaved work in places, but
  has no list of recent projects and restores no state (`products/qq/studio/`).
- **The Store has one signed catalogue,** format 1, with two kinds, theme and widget (`store/src/catalog.cpp`). The
  Library's games come from folder scans, not from the Store (`prism/shell/library.cpp`).
- **A QQ game is readable.** Its scenes and logic are QML files in its package (`products/qq/runtime/sceneio.cpp`).
- **Nothing enforces a free Store.** `price` is read as floating point and never checked, and the Store app's button
  installs whatever the catalogue lists (`apps/StoreApp.qml`). QOR-0006 says it offers Install only for free items. Every
  item today is free, so this has not mattered yet.

**Unity's terms** were read directly on 1 November 2026 ([Terms of Service](https://unity.com/legal/terms-of-service) and
[Software Terms](https://unity.com/legal/editor-terms-of-service/software), both last updated 30 June 2026). This is the
assistant's reading, not legal advice. Unless authorised by the terms, policies, documentation or in writing, §17.2 bars:

- (ff) interacting with an Offering "by means of any AI agent, autonomous or semi-autonomous software system, large
  language model, command line interface, model-context-protocol (MCP) client or server, agentic framework, or any other
  automated, machine or non-human caller", except through Unity's gateway and allowlist;
- (gg) "any plugin, extension, add-on, module, or third-party integration that interfaces with, accesses, or integrates
  into any Offering" without "prior written authorization";
- (p) any Project "in connection with any product or service that is similar to or competes with, any Offering";
- (e) trying to study its "data structures or other externals", and (q) benchmarking to improve a product that replicates it.

A breach "shall immediately and without further action terminate the rights": the maker's licence is what is at risk. The
Software Terms add that "User Modifications" which build, package or optimise a project for a platform Unity does not list
need "a separate grant of rights". A Platform counts an operating system's "fork" and a store, and those terms say a
Platform may need its own grant. Two things soften this. Unity has long documented command-line builds, and a Unity advocate
[wrote on 6 July 2026](https://discussions.unity.com/t/new-terms-of-service-is-unity-restricting-local-ai-tools-and-ai-training/1724661/21)
that Unity "will not be enforcing this restriction until we have vetted these 3rd parties". A forum post does not change
a contract. [Unity 6.3 lists](https://docs.unity.com/en-us/engine/6000.3/manual/get-started/install-and-upgrade/getting-started-installing-unity/system-requirements)
Ubuntu 22.04 and 24.04 on GNOME, an editor "running without emulation, container or compatibility layer", and Wayland on
Linux as experimental.

## Decision

1. **The order is the brief's milestones,** in [ROADMAP.md](../ROADMAP.md): the real baseline; QQ creation and play on native
   Linux, with two project workspaces; one asset pack and one game in separate demonstration stores; the shared platform
   prerequisites, then selected Unreal and Unity workflows, with the image alongside; then the release gates. QOR-0017's
   remaining games (the racer, the pixel-art game, the arcade game) wait behind the second and third.

2. **Every project has a workspace.** Personal state (open files, panels, window layout, test history) lives in the person's
   own data, never in the project folder, because projects are shared and published and a stranger's is hostile. What is pinned
   to the project (its engine and version, its asset dependencies) travels with it. A workspace is not a security boundary: a
   switch goes through the same trust check as opening. It keeps unsaved work, starts no heavy editor in the background and
   upgrades nothing silently. It starts in QQ Studio, and Prism's desktop-level workspace follows.

3. **Engines are reached through small adapters.** Open, Build and Test, with discovery, launching, packaging, process
   management and useful diagnostics, version by version. Nothing specific to an engine enters Prism's core. QQ keeps its
   own project and runtime model. People use their own engine under their own licence, and QOR-0020's rules stand.
   - **QQ's comes first,** then **Unreal's,** once the owner has the licence and the shared prerequisites pass.
   - **Unity's stays gated, one operation at a time.** The goal stands. An owner's approval cannot override Unity's terms, so
     each operation waits until it has a grant: in additional terms, in documentation, or from Unity in writing, or a legal
     reading says none is needed. Which agreement a given person accepted is a fact to establish, and an installed version does
     not establish it. While gated, no code of ours touches Unity's editor. A game built with Unity is an ordinary Linux
     program, plays by the same path as any other and needs nothing from us. **Assistants do not operate Unity's editor**
     (`AGENTS.md` §3).
   - **Unreal Engine 6 means replaceable seams and a later validation stage,** not a compatibility promise.

4. **"Supported" means run.** For a configuration to be supported, these are shown on it: opening or creating a project,
   importing an asset, editing a scene, menus and dialogs, the viewport with correct mouse capture, saving and reopening,
   building a small game, and launching what was built, with focus, clipboard, scaling, fullscreen, sound, controllers and
   several displays where they matter. The roadmap's matrix records engine, host, drivers, card and display path. It keeps
   what a vendor supports apart from what QOR OS has tested, and editor, packaged Linux and compatibility-layer runs apart
   from each other.

5. **Two stores, on shared services.** This replaces Phase 4's "one catalogue".
   - The **Player Store** carries finished games, demos, expansions, listening soundtracks, supported playable mods and selected
     player utilities. Themes and widgets stay where they are, for everyone.
   - The **Creator Store** carries development assets, editable projects and templates, design resources, editor tools and
     plugins, mod-making tools and creator applications. Creator features are opt-in, one account covers both, and there is no
     third store.
   - **Every listing is typed:** product kind, store placement and category, stable package identity, entitlement, platform,
     and engine or base-game dependencies. An item that is unclassified or contradictory stays a draft: listed nowhere and
     refused by `install`.
   - **Discovery is deliberate.** Player-only navigation, search, recommendations, featured collections and category counts
     exclude creator-only listings, each asking for an explicit view with no fallback to the other store, and that is tested.
   - **A utility in both stores is one package,** installed once, with one entitlement. Turning creator features off keeps
     installed apps, purchases and project data.
   - **Hiding is not access control.** The switch is a preference. Authorisation in the engine, and later on a server, protects
     installs, private downloads and project archives independently, tested from C++.
   - QOR-0020's refusal of source in a package applies to the Player Store. Nothing holding Engine Tools goes in either.
   - Whether the chain records who holds a licence is left to the record that chooses the paid model.

6. **The first stage is free, curated and small.**
   - A new catalogue format, 2, adds those typed fields with licence, provenance, previews, a sequence and an expiry. It refuses
     `price`. Format 1 stays valid, so today's fixtures are not re-signed.
   - It installs only `game` and `asset-pack`, and only passive files in a pack. Other kinds parse and stay drafts until they
     have their own stage, since executable plugins are not passive assets.
   - The demonstration uses a separate catalogue and a throwaway key, in tests and development builds, never in a release.
   - The first pack is content QOR-MATRIX owns or may redistribute. **The owner's Fab and Epic collection is never store
     inventory:** a purchase or a conversion gives no right to resell.

7. **What buying a game gives.** A finished game and its editable project are distinct linked editions, each with its own
   entitlement. A game purchase does not unlock the project's archive or its reuse licence. That is a question of rights and
   delivery. **Readable QML at runtime is a separate question:** no packaging can promise perfect secrecy of source. Sealed
   packaging is judged against realistic copying and tampering, and waits unless it blocks a release requirement.

8. **Money is unchanged.** QOR-0006 stands: a free catalogue or simulated sale is a prototype, not commerce. Seller onboarding,
   purchases, payouts, refunds, tax, updates and takedowns are specified with the record that chooses the paid model, before
   any of it is built.

9. **Two things wait for what they need.**
   - **A playable scene from the owner's collection** needs an inventory of what the account holds and each item's licence,
     QOR-0022's answer, and real access to an editor. One scene, not several new games.
   - **Console capture** is proposed: PS4 over HDMI to a USB capture device, with device choice, a fullscreen preview, sound and
     honest messages for no signal or a protected source. It takes capture hardware and measured delay, and it does not displace
     native-Linux reliability.

10. **What 1.0 gains.** QQ's project workspaces, and free games and asset packs from QOR-MATRIX through the two stores. An
    engine's adapter ships only where its workflow has passed, and is not a gate on 1.0. Paid items and console capture are not in
    1.0. Third parties' games still wait for Store stage 2.

11. **Optional apps are native Qt, and a backlog, not fourteen projects.** They reuse QQ Studio, Prism, ARQADE and QOR Media before
    adding a standalone app or a framework, work offline on local data, and need neither AI nor cloud sync to open local work. Each
    has its store placement in [ROADMAP.md](../ROADMAP.md). Nothing is uploaded silently. The Game Design Document Studio comes first,
    when it fits without disturbing the Linux milestone. Apps that need hardware wait for it.

12. **The image is a release deliverable,** with its acceptance in [ROADMAP.md](../ROADMAP.md). QOR OS is not ready to ship because
    its desktop applications compile.

## Consequences

- **The Store's document and its stages change.** [STORE.md](../STORE.md) describes one store today and points here.
- **Evidence stays bounded.** A suite passing under WSL in software says nothing of a real card, real sound or a real controller.
  Those need the owner's Linux machine, as Phase 1's exit already does.
- **A real-shell demonstration needs a key.** The shell's Store key is compiled in, and `QOR_STORE_URL` changes only the address,
  so the demonstration needs a demonstration build, or the owner's key for a real catalogue.
- **A free Store gets enforced.** Format 2 refuses `price`, and format 1's gap is closed in the same work.
- **A sign this is wrong:** Unity answers a question in writing (then that operation's gate changes); people want both stores
  together in use (then the navigation changes, and the enforcement does not); or workspaces cost more than they give on the
  reference hardware (then the budget in the roadmap says so).

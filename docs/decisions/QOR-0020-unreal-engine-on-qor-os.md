# QOR-0020: Unreal Engine on QOR OS, with Epic's own builds

- **Status:** Accepted
- **Date:** 2026-11-01
- **Deciders:** the owner, who asked to "find the best way to implement Unreal
  Engine 5 into our ecosystem", to support developing with it fully within QOR OS.
  The way is the assistant's recommendation, under the owner's standing word to
  take the recommended path. Accepting Epic's licence is the owner's (decision 7).
- **Amends:** [QOR-0007](QOR-0007-what-1-0-ships.md), whose Games row gains native
  Linux games (decision 9)
- **Relates to:** [QOR-0005](QOR-0005-every-display-at-its-own-refresh.md) (gamescope
  as a client of Prism), [QOR-0006](QOR-0006-qor-os-1-0-moves-no-money.md) (no money),
  [QOR-0008](QOR-0008-before-a-public-release.md) (release gates),
  [QOR-0017](QOR-0017-arqades-new-games.md) (rewarded games),
  [QOR-0018](QOR-0018-the-security-and-quality-bar.md) (the bar)

## Context

**What was asked comes to three things.** Players run Unreal games on QOR OS. Makers
build them there, or for it. And those games belong in the ecosystem: the Library, the
Store, the controller, the person's look.

**QOR OS is already a target.** It is Linux, and Unreal builds for Linux with nothing
from us. For [5.8](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-release-notes?lang=en-US),
the current release (June 2026), [Epic documents](https://dev.epicgames.com/documentation/unreal-engine/linux-development-requirements-for-unreal-engine):

| | |
|---|---|
| Runs on | Rocky Linux 8 or Red Hat 8 or newer. Fedora is not named |
| Developed on | Ubuntu 22.04, Rocky Linux 8 |
| Graphics | Vulkan: NVIDIA 570 or newer; AMD RADV 24.2.8 or newer. Intel is not named |
| Recommended | 32 GB of memory, a GeForce 2080, 8 GB of video memory |
| Toolchain | v26, Clang 20.1.8, fetched by `Setup.sh` |
| From another machine | Cross-compiling for Linux is supported from Windows only |

**In practice it runs on X11.** [Epic's page on SDL3](https://dev.epicgames.com/documentation/unreal-engine/updating-unreal-engine-on-linux-to-sdl3)
says the editor on pure Wayland is unusable, on XWayland it is fine, and game targets on
Wayland are "still untested".

**The Linux editor is unsettled, whatever we do.** Reports from people, not from Epic:
[5.7's interface was broken on several desktops](https://forums.unrealengine.com/t/unreal-editor-5-7-ui-interaction-is-broken-on-linux-for-3-month-already/2652325?page=4),
Fedora 43 among them, with a fix said to be in 5.8; and
[5.8's preview crashed](https://forums.unrealengine.com/t/my-experience-unreal-engine-5-8-0-preview-on-linux/2721855)
on NVIDIA and AMD alike. No report on the final 5.8 was found.

**Unreal Engine 6 is coming.** [Epic targets Early Access at the end of 2027](https://www.therookies.co/blog/headlines/state-of-unreal-2026-top-news-about-unreal-engine).
Whatever is built here should not depend on the engine's insides.

**The licence decides the shape.** Epic's pages refuse automated reading, so no one here
has read the current text. This is from
[Epic's own PDF of May 2023](https://cdn2.unrealengine.com/unreal-engine-end-user-license-agreement-18-1-a8e14000e547.pdf),
with [a third party's capture of May 2026](https://conductatlas.com/platform/unreal-engine/unreal-engine-eula/)
and [a summary of October 2026](https://wiki.licenseware.io/wiki/unreal-engine-eula-royalties-and-seats/) for what has moved:

- **No shared engine.** A distributed product may carry the engine "only in object code
  and only as an inseparable part of the Product".
- **No editor from us.** A product with Engine Tools (the editor, anything in the Developer
  or Editor folders, what can build standalone products) goes only through Fab or a fork of
  Epic's GitHub network. [Images with them may not go to end users or public registries](https://unrealcontainers.com/docs/obtaining-images/eula-restrictions).
- **A store may carry a game.** Publishers and distributors may distribute a product on the
  maker's behalf, under an end-user licence that disclaims Epic's technology. Credits, where
  a product has them, carry Epic's notice.
- **Copyleft stays out.** No GPL, ShareAlike, or LGPL other than dynamic linking.
- **A language beside C++** may only be distributed free and as source, to every licensee.
- **Generative AI.** Licensed Technology may not be "a training input to any Generative AI
  Program or ... prompt-based input where the Generative AI Program trains on input data".
  Earlier wording left out that last condition. This project is built largely by an assistant.
- **Royalties are the product's:** 5% of revenue past $1,000,000, "regardless of who receives"
  it. The 2023 text counts redemption of virtual currency. Neither names cryptocurrency.
- **Terms change.** A new download may need the amended licence, so the version accepted matters.

**What Prism lacks, as measured.** A running Prism, offscreen and in software, offers 19
globals. [gamescope's Wayland backend](https://github.com/ValveSoftware/gamescope/blob/master/src/Backends/WaylandBackend.cpp)
will not start without nine, and Prism has six: `wl_compositor`, `wl_subcompositor`, `wl_shm`,
`xdg_wm_base`, `wp_viewporter`, `wp_presentation`. It lacks `zwp_relative_pointer_manager_v1`,
`zwp_pointer_constraints_v1` and `zwp_linux_dmabuf_v1`. Qt provides the last, nothing here turns
it on, and only a real graphics card can show it working. So Phase 3's Steam is blocked by the
same three. Prism has no X11 at all, which QOR-0005 left for later.

**X11 need not mean writing a window manager.** [xwayland-satellite](https://github.com/Supreeeme/xwayland-satellite)
(MPL-2.0, [packaged by Fedora](https://packages.fedoraproject.org/pkgs/xwayland-satellite/xwayland-satellite))
gives any compositor with `xdg_wm_base` and `wp_viewporter` rootless XWayland, as a client of
it. One person reports it fixing most of the editor's trouble on Sway.

**Options:**

| Option | For | Against |
|--------|-----|---------|
| A. The engine in the image or the Store | One click | The licence forbids it |
| B. A QOR platform in a fork of the engine | A target with our name | QOR OS is Linux, so it adds nothing; makers must build from source; a rebase at every release |
| C. Unreal inside QQ, or in its place | One engine | QQ's confinement and replayed runs are why QQ exists (QOR-0017); nothing of QQ's would survive it |
| D. Windows builds through Proton alone | Comes with Phase 3 anyway | Nothing for makers; every game second-hand |
| **E. Epic's builds as they are; the OS meets them; the ecosystem joins from outside the engine** | Within the licence; shares its hardest work with Steam; should outlast 5.x | Rests on X11, and on an editor Epic is still settling |

## Decision

Option E. D comes with it, for what people already own.

1. **Epic's own builds, unchanged.** Nothing of the engine, its editor or its tools enters
   this repository, the image or the Store's own content. Each game carries its own engine.
   QQ stays QOR OS's engine and QOR-0017 stands: this is support for another's, beside it.
2. **Games run in gamescope, as Steam's do.** Prism gains the three interfaces, as hostile
   input (QOR-0018): only the surface with the keyboard may lock the pointer or hear relative
   motion, after a person's action; Prism always keeps a way out; the lock screen ends it. A
   game's launch sets `SDL_VIDEODRIVER=x11`. Each game has gamescope's X server to itself,
   because X clients sharing a server can read each other's keys. When Epic stands behind
   Wayland for games, they move to it.
3. **The editor runs through xwayland-satellite,** one instance for it, started by Prism on
   demand. It lives in a container of a distribution Epic names, made from a file here with
   nothing of Epic's in it. The person downloads the engine from Epic, with their own account.
   Until stage 4 below passes, the supported way to make a game for QOR OS is the Windows
   editor with Epic's cross-compile toolchain.
4. **A native game is a Library item in a sandbox.** It declares itself in a manifest and runs
   under Phase 3's bubblewrap, seccomp and Landlock, with its own save folder, the graphics card
   and sound, and the network only if it says so. What a game hands Prism is hostile.
5. **The ecosystem joins through a plugin, from outside.** A small C++ client for the host
   protocol lives here, with no Unreal in it, and is tested here. A thin plugin wraps it, for
   controller hand-over, the person's look, pausing and quitting, and later sign-in. No Qt, no
   QOR core and no keys enter the game's process (`AGENTS.md` §6). It is distributed through Fab
   or a fork of Epic's network until Epic says otherwise. Its public name waits for the owner
   (`AGENTS.md` §8), and "Unreal" in a name or logo needs Epic's approval.
6. **No rewards and no money, for now.** Unreal's physics and floating point give no replayable
   run, so an Unreal game is not rewarded by QOR-0017's method. Embedding `rules.mjs` would touch
   the clause on languages beside C++. A later record decides this. QOR-0006 stands, and a maker's
   royalty is between them and Epic.
7. **The licence is the owner's to accept, and nobody here downloads the engine before that.**
   With counsel, read the current text; choose the licensee, since the licence does not transfer
   and the legal entity is still open; record the version accepted; and put to Epic in writing:
   may a plugin go elsewhere than Fab and GitHub, may a store carry games as this one would, how do
   their terms read on QOR's chain and CGT, and what does the clause on AI allow.
8. **Assistants do not read the engine.** Until Epic or counsel says otherwise, nothing under an
   engine's folders, nor a log quoting it, goes into an assistant's context, and the engine is
   never in this repository. An assistant works from Epic's public documents and our own code, and
   QQ Studio's agent never opens an Unreal project. A person compiles the plugin against the engine.
9. **In stages, each ending when its exit is shown working.** [ROADMAP.md](../ROADMAP.md) carries
   them. 1.0 gains native Linux games in the Library, installed by hand (stages 1 and 2), since they
   share Steam's path. The plugin, the editor and the Store wait for Phase 4, with the public SDK.

## Consequences

- **The first work needs no engine and helps twice.** Stage 1's interfaces and a Vulkan program
  in gamescope in Prism are what Phase 3's Steam needs regardless.
- **"Fully" has limits we do not set.** QOR OS cannot put the editor in a person's hands, only make
  it one download and a working desktop away. Epic's Linux editor may stay rough, so the Windows
  route stays open. Intel is not in Epic's list, so QOR OS promises Unreal on NVIDIA and AMD only.
- **gamescope on NVIDIA, nested in Prism, is unproven** and comes first on the real machine. Nested,
  gamescope may also copy each frame, which Prism's direct scan-out cannot avoid; that gets measured.
- **The engine stays off CI.** It is too big for this laptop and its images may not be shared, so CI
  tests the Unreal-free client and a person compiles the thin plugin.
- **The owner's laptop has 16 GB,** half of Epic's recommendation. It suits packaging and playing;
  the editor there will be tight.
- **The Store gets a check** that refuses Engine Tools and source in a package, and asks for the maker's
  disclaimer and notices. Third-party games wait for Store stage 2 in any case.
- **A sign this is wrong:** gamescope cannot run nested on NVIDIA (then games use the satellite too,
  or Prism gets its own XWayland); Epic rules out a part of this in writing (then that part changes);
  or Unreal 6 drops the Linux path this rests on (then a later record decides again).

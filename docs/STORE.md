# The QOR OS Store

The Store is where people get themes, widgets and, later, apps, games, tools
and music for QOR OS. Stage 1, in this repository today, is a complete,
verifying client with QOR OS's own themes and widgets. There are no payments
yet. Third-party publishing arrives with the developer program in stages 2
and 3 ([Roadmap](#roadmap)).

[QOR-0021](decisions/QOR-0021-projects-engines-and-two-stores.md) makes this two storefronts on shared services, a player
store and a creator store, starting free and curated. The engine under them exists ([Format 2](#format-2-the-two-storefronts)),
and so do their pages, in a build that asks for them, on a catalogue signed with a throwaway key
([The demonstration build](#the-demonstration-build)). What people see in any other build is still the one store of themes and
widgets below.

## What people see

- **Store app** (`apps/StoreApp.qml`): Discover, Themes, Widgets, Apps, Games,
  Tools and Music, search, item pages, and a Library with updates. Apps lists
  what comes with QOR OS. Games, Tools and Music say plainly that they arrive
  with the developer program.
- **Themes** apply from the Store, from Settings › Appearance (Aurora plus
  installed themes) or from Beam ("Theme: Ember"). A theme sets the accent, a
  preferred appearance, a tint for every neutral surface and the Aurora
  wallpaper's character. The accent and appearance stay the user's to change
  afterwards.
- **Widgets** live on the desktop, over the wallpaper and under the windows,
  on a grid of 152 px cells: small (1×1), medium (2×1) and large (2×2). Add
  them from the Store ("Add to Desktop") or from the gallery: right-click the
  desktop and choose Edit Widgets, or use Beam. While editing, the windows
  step aside, widgets drag to a free cell, and × removes one. Escape or Done
  finishes. The layout persists in `Settings.widgets`.

First-party content: eight themes (Ember, Tidepool, Moss, Dusk, Paper,
Graphite, Saffron, Glacier) and six widgets (Clock, Calendar, Now Playing,
System, Focus, Visualizer), all by QOR-MATRIX.

## How it works

`QOR.Store` (`store/`) is the client:

| Part | Role |
|------|------|
| `StoreEngine` | Fetches the catalogue; downloads, verifies, installs, updates and removes packages per user |
| `Store` (QML singleton) | The catalogue and the user's library as plain maps for QML; installed themes and widgets |
| `Storefronts` (QML singleton) | The Player Store and the Creator Store for QML, each asked for by name, in a demonstration build. Empty in any other |
| `Themes` (QML singleton) | Aurora plus installed themes; the active one; `apply(id)` |
| `ThemePreview` | A theme in miniature: its wallpaper and a window in its colours |

The catalogue comes from `QOR_STORE_URL` (the URL of a `catalog.json`; its
signature is the same URL with `.sig` appended) or, by default, the signed
first-party catalogue compiled into QOR OS. `qrc:` and `file:` URLs are read
directly; anything else goes through Qt Network (redirects only to equally
safe URLs, a 30 s transfer timeout), so a store server needs no client
change. Stage 1 has no server, so that path has not yet met one. Packages install under
`QOR_STORE_DIR`, by default the app data folder's `store/`
(`items/<id>/` and `installed.json`). Automated runs (`--script`,
`--screenshot`, `--benchmark`) and the tests use a temporary folder.

The shell binds the active theme into `Theme` (`neutralHue`, `neutralTint`,
`wallpaperSeed`, `wallpaperIntensity`). Neutral roles resolve through
`Oklch.themed(token, …, neutralHue, neutralChroma)`. With Aurora's values the
colours are identical to Lumen's own, which the golden-image tests guard.

## Trust

Nothing installs unless all of these hold:

1. The catalogue carries the **store's signature** over its exact bytes. The
   store's public key is compiled into QOR OS (`store/keys/store.pub`) and is the
   root of trust. Nothing in an unsigned or altered catalogue is believed, not
   even its developer keys.
2. The downloaded package's **SHA-256** matches the catalogue entry.
3. The package's manifest carries its **developer's signature**, made with
   the key the signed catalogue lists for that developer.
4. Every file matches the **manifest's SHA-256**, and the package holds
   exactly the listed files: no extra or missing files, no symbolic links, no
   paths that climb out of the install folder, and at most 32 MB unpacked,
   counted from the bytes themselves.
5. The manifest's id, version, kind and developer match the catalogue's. In format 2 so do the stores it is placed in, its
   edition, its licence, its dependencies, its platforms and engines, and what it unpacks to.
6. The catalogue lists it free. Format 1 refuses a `price` that is not zero, and format 2 refuses any at all
   ([QOR-0006](decisions/QOR-0006-qor-os-1-0-moves-no-money.md)).

Signatures are ECDSA P-256 over SHA-256 (r ‖ s, 64 bytes), checked by the
platform's own crypto: CNG on Windows, OpenSSL's libcrypto elsewhere.

**An install is all or nothing, as seen from outside,** with whatever it depends on. Every package it needs is downloaded and
verified first, and held in memory, up to 128 MB for every install under way together, a package counting from its first
byte on the way, so a download that would not fit stops there. Only then does anything move, under a
lock in the store's folder and against what is on disk at that moment, since another program of this person's may share the
folder: what is recorded is read again, and the plan is checked again against it. A journal is written, then each new version
is swapped in beside the old one, then `installed.json` is written. That last write is the moment it happens, and it is atomic.
A failure before it puts everything back. So does a crash, the next time an engine starts. A crash after it finishes clearing
up, and an old version goes only once the new one stands whole, every file as its manifest lists it. Putting back or
clearing up may itself be cut short and run again over the same journal, and comes to the same end: it
decides from what is on disk, by whether the old version is still set aside, and never from an absence it made itself. A step of
it that fails, as when a file being scanned cannot be deleted yet, stops it there: the journal stays, the old version stays set
aside, and the next try starts again. A record is written before any journal, an empty one before a first install, so
one missing beside a journal was lost. With no record left to say whether a plan happened, what is on disk says: a plan whose every
new version is in place and whole stands, and anything less goes back. On Linux
and macOS each step is made durable before the next (`fsync` on the folders): what was staged before the journal says it is
there, the journal before anything moves, the swap before the record. A removal goes the same way. The lock is never taken from
a holder that is alive, however long it has held it; one whose holder has died is. An engine that goes away while installs are
on their way ends each of them, and says so for each.

**Widgets are code.** A widget is QML that runs in the shell, so stage 1
installs only QOR-MATRIX widgets. Third-party widgets and apps wait for the
Phase 3 sandbox, which runs them out of process. Themes are data and can open
to third parties sooner.

## Formats

**Catalogue** (`catalog.json`, signed as `catalog.json.sig`):

```json
{
  "format": 1,
  "store": "QOR OS Store",
  "developers": { "dev.qorsync": { "name": "QOR-MATRIX", "verified": true, "key": "<base64 P-256 point>" } },
  "items": [{
    "id": "dev.qorsync.theme.ember", "kind": "theme", "name": "Ember", "developer": "dev.qorsync",
    "version": "1.0.0", "summary": "…", "description": "…", "whatsNew": "…", "released": "2026-10-26",
    "tags": ["warm"], "featured": true, "price": 0,
    "preview": { "accentHue": 38, "appearance": "dark", "neutralHue": 40, "neutralTint": 1.5,
                 "wallpaperSeed": 11.7, "wallpaperIntensity": 1.15 },
    "package": { "file": "packages/dev.qorsync.theme.ember-1.0.0.qorpkg", "size": 806, "sha256": "…" }
  }]
}
```

`price`, if a format 1 listing has one, must be the number zero, and nothing keeps it after that: a price of anything else
refuses the whole catalogue. `package.file` must be a plain relative path: no drive, scheme, absolute path or climbing.

**Package** (`.qorpkg`): a zip archive of `manifest.json` (`format`, `id`,
`kind`, `version`, `name`, `developer`, and `files`: path → SHA-256),
`manifest.sig` (the developer's signature over `manifest.json`) and the
files.

The archive is read by a reader of its own (`store/src/archive.cpp`), which takes the plainest kind of zip, the kind
`build_store.py` writes, and has no way to inflate anything, so nothing can grow past the archive that holds it:

- entries stored, never compressed or encrypted, with both of an entry's headers agreeing on its name and size;
- one after another from the first byte, then the central directory and nothing after it: no overlap, no gap, no comment;
- 4096 entries at most, and 32 MB in all, counted from where the bytes lie and not from what a header says;
- names that are plain relative paths on every system a package may reach: no climbing, drive, backslash or line break, no name
  Windows keeps for a device (`CON`, `NUL`, `COM1`..., with or without an extension), no trailing dot or space, and no two
  names, or a name and a folder, that a system ignoring case would land on one file.

**Theme** (`theme.json`):

```json
{ "accentHue": 38, "appearance": "dark",
  "neutral": { "hue": 40, "tint": 1.5 },
  "wallpaper": { "style": "aurora", "seed": 11.7, "intensity": 1.15 } }
```

`appearance` is `dark`, `light` or `any`. `tint` scales the neutrals' chroma
(0 to 2.5; 1 is Lumen's). Seeds run 0 to 1000 and intensity 0.3 to 1.6.

**Widget** (`widget.json` and the QML it names):

```json
{ "entry": "Clock.qml", "size": "medium", "glyph": "clock", "hue": 228 }
```

The entry's root `Item` fills a glass card the desktop draws. It may import
`QtQuick`, `QtQuick.Shapes`, `QOR.Kit` and `QOR.Services`, reads the system
through `Services` and asks for actions through `Intents`. It never needs
input while the desktop is being edited: the card disables it then.

### Format 2: the two storefronts

One catalogue feeds both storefronts, and each listing says where it goes.

```json
{
  "format": 2, "store": "…", "sequence": 2, "expires": "2099-12-31",
  "developers": { "org.example.demo": { "name": "…", "verified": false, "key": "<base64 P-256 point>" } },
  "items": [{
    "id": "org.example.demo.game.ember-climb", "kind": "game",
    "placements": [{ "store": "player", "category": "Platformers" }],
    "entitlement": { "type": "free" }, "platforms": ["linux-x86_64"],
    "edition": { "group": "org.example.demo.ember-climb", "role": "game" },
    "executable": false, "name": "…", "developer": "org.example.demo", "version": "1.0.0", "summary": "…",
    "description": "…", "whatsNew": "…", "released": "2026-11-01", "tags": ["platformer"], "featured": false,
    "licence": { "spdx": "Apache-2.0", "file": "LICENSE.txt" },
    "provenance": { "origin": "…", "author": "…", "source": "…", "permission": "…" },
    "engines": [{ "id": "qq", "min": "0.1.0", "below": "1.0.0" }],
    "dependencies": [],
    "previews": [{ "file": "previews/hero.png", "sha256": "…" }],
    "package": { "file": "packages/….qorpkg", "size": 18000, "sha256": "…", "unpacked": 26000 }
  }]
}
```

An older client refuses this cleanly: its check is that `format` is 1.

**What a catalogue may be refused for, whole.** A bad signature, an unknown format, an `expires` that has passed or is not a
date, or a `sequence` that is not a positive integer. Then a `sequence` lower than the highest this engine has taken, or the
same one with different bytes: it remembers both in its folder (`catalog-state.json`) for each store key, which is what tells
an older copy, still signed, from a new one. It rereads that under the store's lock, so another program's progress counts, and
another store key starts its own memory, so a demonstration build cannot lock out a store that works. Once an engine has taken a
format 2 catalogue under a key it refuses format 1 under that key, which has no sequence to compare and would otherwise be the
way round this. A catalogue with `items` or `developers` missing or of the wrong type is refused in both formats: it is not an
empty catalogue, and would drop the keys what is installed is checked against. Also anything malformed or hostile in a listing
that says clearly what it is, since a store that signed that is not to be half believed: a `price` anywhere, on any listing,
under any field, even zero; a field this format does not know, at the top of the catalogue, a developer or a listing, or inside
a listing that is not a draft, so that a paid field cannot arrive under another name; an id that is missing, not valid or used
twice; an unsafe package file; a licence, preview or range that does not hold what it should; a list past its limit; a dependency
cycle, or a chain deeper than 32, in whatever order the catalogue lists it.

**What makes a listing a draft.** A listing that is unclassified or contradictory stays out of every view and `install`
refuses it, and the rest of the catalogue loads. `Catalog::drafts` holds its id, its kind, a `code` for a tool to read and a
`reason` for a person. Three things are asked of a draft before it is one, since they refuse the whole catalogue: that its id is
good, that no price is anywhere in it, and that its top level holds no field this format does not know. Its edition group, if it
gives one, is kept for one use: nothing may depend on another edition of itself through it. Nothing else of a draft is read.

| `code` | The listing |
|--------|-------------|
| `unknown-kind` | names no kind, or one no store knows (`theme` and `widget` are format 1's) |
| `no-placements` | is placed nowhere |
| `bad-placement` | has a placement that is not a store and a category, or more than 16 of them |
| `unknown-store` | is placed in a store other than `player` or `creator` |
| `empty-category` | has a placement with no category |
| `placement-contradicts-kind` | is in a store its kind does not belong in |
| `duplicate-placement` | is placed in one category twice |
| `entitlement-not-free` | has an entitlement other than exactly `{"type": "free"}`, which replaces any notion of price |
| `no-platforms` | names no platform, or something that is not the name of one |
| `executable-not-said`, `executable-not-taken` | does not say whether it is executable, or says it is |
| `edition-contradicts-kind` | has an edition with no group, or a role that does not fit its kind |
| `edition-dependency` | reaches another edition of itself through what it depends on, however indirectly |
| `kind-not-taken-yet` | is of a known kind this stage does not take |

**Kinds and where they go.** This stage installs `game` and `asset-pack`. Every other kind parses and stays a draft until it
has a stage of its own, with rules for what its packages may hold.

| | Player store | Creator store |
|---|---|---|
| For the player store alone | `game`, `demo`, `expansion`, `soundtrack`, `mod` | |
| For the creator store alone | | `asset-pack`, `template`, `project`, `plugin`, `tool` |
| Either or both, as its placements say | `utility`, `creator-app` | `utility`, `creator-app` |

**One engine, one install, two views.** `id` is the package's identity: one id, one install, in one folder, whichever store it
was found in, so a utility placed in both is listed in each and installed once. Every query names its view, and there is no
default and no fallback: `listings(View)`, `featured(View)`, `search(View, query)` and `categoryCounts(View)` answer from that
view alone, and an empty answer stays empty. A catalogue that has expired since it arrived answers nothing. A person with creator
features on still uses the player view as players do. A format 1 catalogue is in neither view: `Store` lists its themes and widgets
as it always has, and shows no format 2 listing, in its list or by id, since that page knows no view.

**Creator features are a state the engine is given, off until it is.** Off, the creator view answers nothing, and `install`
refuses from C++ what only the creator store carries, including as another item's dependency or if they go off while it is on
its way, with no page involved. Its previews are not handed over either. Turning
them off removes nothing: what is installed stays installed, intact and usable, and only its updates wait. The preference itself
is kept elsewhere, and the engine does not remember it.

**A game and its editable project are separate.** They may share an `edition` group, and are separate packages with separate
entitlements. Holding one gives nothing of the other. A listing that reaches another edition of its group, directly or through
something else, is a draft, and the resolver refuses it as well.

**An id is one thing, and only moves forward.** A listing older than what is installed is refused, as is one from another developer
or of another kind. Remove it first if that is what is meant.

**Where it runs.** `install` refuses a listing whose `platforms` do not name this system (`linux-x86_64`, `windows-x86_64`).
`StoreEngine::setEngines` tells the engine which engines this system has and at what versions, and then a listing that works with
none of them is refused. Told nothing, it cannot make that check and does not guess. The views list by placement alone, so a page
may show what will not install here, and say why. Both are mirrored in the manifest, because they are the developer's claims about
what the package works with, and a store may not list a package for a system its developer never named.

**Dependencies** are `{id, min, below}`, where `min <= version < below`, and carry an expansion's or a mod's base game. They are
fetched first, in order, and one that fails stops all of it. The resolver refuses rather than guesses: a dependency this catalogue
does not list and that is not installed in range, a version outside its range, moving a dependency back, and an update that would
take something installed out of a range that holds now. This is asked as a plan is made and again when it lands, so a dependency
removed or moved while it was on its way stops it, and so does a catalogue that changes under it, though not one that is the same
bytes again. What is already out of range and is not touched stops nothing else: a block is about what it affects. A pack something
needs cannot be removed, while the range it is needed by holds. Updating a pack touches only its own folder.

**`executable`** says whether a package holds what the system would start by itself: native code or a script. It must be false
at this stage, on every kind. A QQ game's QML is not that, and is not passive either: it runs inside QQ's runtime
([QOR-0021](decisions/QOR-0021-projects-engines-and-two-stores.md), decision 7), which is why a game has rules of its own.

**What a package may hold** (`store/src/filerules.cpp`) is an allowlist of extension and first bytes, so a program named as a
picture is refused, and anything not on the list is too.

- An `asset-pack`, and any kind without rules of its own yet, holds only passive files, because a QML file copied into a
  project's `logic/` runs: `png`, `jpg`, `jpeg`, `webp`, `hdr`, `ktx`, `ktx2`, `wav`, `ogg`, `flac`, `mp3`, `glb`, `gltf`, a `bin`
  that a model names, and `txt`. No QML, JavaScript, shader, SVG, level, JSON, native code or script.
- A `game` may also hold `qml`, `mjs`, `js`, `json`, `tmj` and `tsj`. It holds no native code, no script for the system to
  start, and no `qmldir`, which could name a plugin.
- A `gltf` or `glb` may name only files beside it in the package, or carry pictures and buffers as `data:` in base64 (PNG, JPEG,
  WebP, KTX2, or octet-stream and glTF buffers). No climbing, scheme, missing file or other kind of data, under any field called
  `uri`, however deep, and no such field twice in one object, because two readers would not agree on which counts.
- A `txt` is UTF-8 with no control characters but tab, line and page breaks, and no markup, since a Qt `Text` guesses at rich text
  and rich text can fetch a picture. `Qt::mightBeRichText` is asked whether a `Text` would go rich at all. Anywhere in the text, a
  tag is read as Qt's parser reads one, and is markup if Qt's own table of elements or a browser knows its name, and whatever its
  name if it carries a value, an `=` before the `>` that ends it: Qt applies `style` to a tag it does not know, and a browser runs
  an event handler on any tag. Where a tag ends is read both ways: at any `>`, as a browser ends a tag whose name holds a quote,
  and as Qt's parser reads it, where a quoted word, `>` and all, is one word. What is left is what licences hold: plain words in angle brackets, as the GPL has them, an address
  or a link. It is one pass over the text, whatever the text is, and cannot fail open, and the GPL, LGPL, MPL, Apache, MIT and BSD
  texts pass it. The page that shows it still sets `textFormat: Text.PlainText`: this is the second wall, not the first.

A passive file still reaches a decoder when it is used, so these rules do not make a hostile picture safe to decode.

**The manifest** of a format 2 package has `format: 2` and mirrors what identifies it in the catalogue: `id`, `kind`, `version`,
`developer`, `stores` (the stores it is placed in, sorted), `edition`, `licence`, `dependencies`, `platforms` and `engines`. It knows
those fields and refuses any other, a price among them. A placement's category is the store's to change, and is not mirrored. Themes
and widgets keep format 1 manifests, so their packages are never signed again.

**Previews** are files beside the catalogue. `StoreEngine::fetchPreview` hands one over only after checking it against the
catalogue's digest and that its name and first bytes are a picture's. That is no promise about the rest of it.

**The demonstration** is `tests/store/fixtures-demo/`, signed with a throwaway key that is used once and dropped, so no private
key exists to keep out of the repository. It is in tests and in a build that asks for it, never in a release: nothing of it is
in any other binary, and its key opens nothing else. It has two catalogues in turn, `v1` and `v2` (sequences 1 and 2). Each holds
Ember Climb as a `game`, its art as an `asset-pack` (1.0.0, then 1.1.0), and Ember Climb's editable project as a draft. Everything
in it is the Apache-2.0 sample in `products/qq/samples/ember-climb/`, with its licence and notice in each package. It expires in
2099, because tests read it for years.

### The demonstration build

`QOR_STORE_DEMONSTRATION`, off by default, is what shows the two storefronts on the shell. It is a choice of a build and never a
release:

- With it off there is no second engine. `Storefronts` answers nothing, no folder is made, its URL is read by nothing, and the
  Store's Games, Tools and Music pages and Settings are as they were.
- With it on, `tests/store/fixtures-demo/store.pub` is compiled in, and `QOR_STORE_DEMONSTRATION_URL` names a format 2 catalogue, or
  several separated by `|` to be taken in turn.
- Every preset says. The hidden `base` sets it off and only the two `-demonstration` presets turn it on, so configuring a folder
  with any other preset turns off what was once set there by hand. `tst_storefronts` fails if a preset leaves it to the cache, if
  one turns it on whose name does not end in `-demonstration` or says CI or a release, if what CI, a release or the image runs
  names it (but for the check below), or if a default build's `qor-shell` or `prism` holds the key.
- Whatever a build says of itself, `tools/ci/check_hardening.py <build folder>` fails one whose `CMakeCache.txt` has it on, or in
  which a program or a shared library holds the key, as its base64 text or its bytes.
- Nothing of a demonstration build installs: its first install rule refuses, which `demonstration_does_not_install` checks.
  CMake refuses the option without the simulator, which is how a device's image is built.

```bash
cmake --preset linux-demonstration      # or windows-demonstration
cmake --build --preset linux-demonstration --target qor-shell
QOR_STORE_DEMONSTRATION_URL="$PWD/tests/store/fixtures-demo/v1/catalog.json|$PWD/tests/store/fixtures-demo/v2/catalog.json" \
    build/linux-demonstration/prism/qor-shell
```

**A person's walk.** Open the Store. Games is the Player Store: Ember Climb is there, with its licence, where it came from, and
Get. Settings › Store turns creator features on, and the sidebar gains the Creator Store, with the art pack. Library has a
**Next Catalogue** button, which takes the next URL as a real store's catalogue would move on at one address, and the pack then
offers its update to 1.1.0. Settings › Store turns them off: the Creator Store is gone, and the pack stays, its next update
waiting until they are on again. Ember Climb, once installed, is in Home's Library, and plays from there.

**Two engines, because trust differs.** Themes and widgets stay on `Store`'s engine, folder and key, in format 1, since the
first-party catalogue cannot be signed again without the owner's key. The second `StoreEngine` has its own key and its own folder,
`demonstration/` in the Store's data location, which the first looks past. Each refuses the other's catalogue by its signature.
Once the second has taken a format 2 catalogue, it refuses format 1, as any engine does. It is told creator features from Settings
as they change, and that this system has QQ at the version in this tree. QQ keeps its own build, so that says which QQ this is, and
not that it has been built.

**What a page gets.** Every list asks of a view named `"player"` or `"creator"`, and any other name answers nothing. No page merges
them, and the Library keeps what each engine installed on a card of its own. A map holds no path a catalogue chose: a page asks for a
preview by number and gets a `data:` URL of the checked bytes, once their digest has matched, their first bytes have said they are
a picture, and their header has said they are at most 4096 pixels a side, since a small file can claim a very large picture.
A card decodes its picture at the size it draws it, both ways. Four previews are on their way at a time and the rest wait their
turn. What is held is bounded at 32 MB, counted as it is held, as base64 text: past that the oldest go first, and a card that lost
its picture shows its glyph until its page is built again and asks again, as it may of one that was refused. What was fetched of a
catalogue, previews and licences, is kept when "Check for Updates" finds the same catalogue, and forgotten when one that differs
arrives or creator features go off. With them off, asking by id after something only the creator store carries gives its id and no
version, and an installed one keeps its own name. An update is said to wait for creator features only when nothing else would
refuse it. Everything from a catalogue is someone else's text, in a `Text` that sets `textFormat: Text.PlainText`.

**A licence's text is only in its package.** "Read the Licence" fetches the whole package and checks it as an install would, signature
and all, and installs nothing. So reading a licence before getting a large item costs what getting it costs: a catalogue that wants
better would carry the text beside itself, as it does a preview. The package on its way counts against the 128 MB every install
under way shares, read from a local file as much as downloaded, and may stop with "Too much is on its way at once" as an install's
would. One is on its way for an item however often it is asked for. A download stops, and says why, when creator features go off
and only the creator store carries the item, or when a catalogue that differs arrives; one read from a local file is refused as
it arrives instead. Its checks, the package's digest, signature and every file's, run on the thread that draws, as an install's
do. Longer than 256 KB the licence is not shown, and the page says it comes with the package.

**Starting again over the same folder.** A person who has reached `v2` and starts over finds `v1` refused as older, in plain words,
with Next Catalogue beside the refusal. That is the sequence working. A fresh `QOR_STORE_DIR` starts the walk again.

**Playing a game from the Store.** An installed game is in the shell's Library (`prism/shell/library.h`) and Home, from the
Store, by the name its developer signed: listed from the engine's own record of what is installed, never by reading a folder for
games, and only while the Player Store lists it, or the Creator Store with creator features on. As it is played, it is checked
again, whole: every file as its developer signed it, nothing beside them, and no link (`StoreEngine::isIntact`). Then the QQ
Player starts on the scene its own `game.json` names, the file a QQ Studio build writes (`products/qq/studio/gamebuild.cpp`), held
to what that writes: a plain file directly in the game's `scenes/`. Nothing is guessed from its files, so a game with no
`game.json` does not play. A game that has changed, or names a scene anywhere else, does not play, and says why in plain words; it
stays installed, for the Store to remove. The Player plays it behind the walls every QQ game has (`tst_playerwall`), and nothing of
it is trusted more than a game built here. The demonstration's Ember Climb carries the `game.json` a build would.
QQ's own `tst_storegame` lays its signed package out as the Store installs it and has the real QQ Player play it, walled, from
that scene to its first frame.

## Building the catalogue

The sources are in `store/content/`: `developers.json`, then one folder per
item under `themes/` or `widgets/`, each with a `listing.json` (the
catalogue text) and the files its package carries.

```bash
python tools/store/build_store.py build      # sign changed packages and the catalogue
python tools/store/build_store.py check      # verify store/catalog against store/content (CI runs this)
python tools/store/build_store.py fixtures   # regenerate tests/store/fixtures (throwaway keys)
python tools/store/build_store.py demo       # regenerate tests/store/fixtures-demo (a throwaway key, format 2)
```

Unchanged items keep their signed packages, so rebuilding changes nothing. `fixtures` and `demo` are not like that: each
re-signs everything in its own folder every time, so run one only when that folder must change.
`git` treats packages and signatures as binary and leaves the catalogue's
bytes alone (`.gitattributes`).

**Signing keys never enter the repository.** `build` keeps them in
`~/.qor-os/store-signing/` (moved there from QTOS's `~/.qtos/store-signing/`
on 27 October 2026). If a machine still has only the old folder, `build`
uses it, so a missing folder never creates a new store key:
`store.pem` signs the catalogue and
`developer-<id>.pem` signs that developer's packages. The first build creates
them and writes the store's public key to `store/keys/store.pub`. Back these
keys up: whoever holds `store.pem` decides what every QOR OS installation
trusts. Before the Store opens to the public, generate the production store
key offline or in a hardware security module, sign catalogues on a machine
that holds nothing else, and ship its public key in a release. Rotating the
store key needs a QOR OS update.

Tests: `tst_store` (signatures, the first-party catalogue, refused tampering, forgery and unsigned catalogues, install,
update and remove), `tst_store_format2` (format 2, every package check and the engine behind the two storefronts, from
the hostile side, signing what it needs with keys it makes and drops, and reading the licence texts in
`tests/store/licences/`, SPDX's as Qt carries them), `tst_storefronts` (the build choice, and what the layer hands a
page), `tst_storefronts_qml` (the real pages, over a catalogue of markup), `shell_e2e_store` (in a demonstration build a
person's walk, and in any other that there is nothing to walk), `tst_library_store` (the Library's games from the
Store, from the hostile side), `prism_store_play` (in a demonstration build, Ember Climb from the Player Store to the QQ
Player's stand-in, through Prism), a qmllint pass over every widget
(`store_widgets_qmllint`), the Store section of `shell_e2e`, and
`tests/tools/test_build_store.py` for the builder.

## What runs, and when it is checked

A package is verified as it is installed (the catalogue's digest, the developer's signature, every file). Its manifest
and signature are kept beside its files, and `Store` verifies them again each time it loads what is installed: every
listed file against the manifest and nothing added, not a file, a folder or a link, since anything running as the person can
write there. What fails is
not loaded. A widget's QML runs inside the shell with all it can do, so only QOR OS's own (`dev.qorsync`) are loaded,
whoever a catalogue lists. A version older than the one installed is refused ([QOR-0018](decisions/QOR-0018-the-security-and-quality-bar.md)). What is
installed must also be the very package recorded as installed: an older copy, signed in its day and put back by hand, is not
intact. A crash during an install or a removal is put right the next time an engine starts, from the journal, unless another
program holds the store's lock. A journal is believed only for names inside `items/` of exactly the shapes the engine writes: an
id, `.staging-<hex>` and `<id>.old-<hex>`. Clearing up deletes nothing through a link: a link where a folder would be goes
itself, and it walks one folder at a time, however deep a tree it finds. What was left with no journal to say otherwise, by an
older version of this code, is cleared too: an old version set aside goes back if nothing replaced it, and goes only once what
stands in its place is whole, since it may be the only good copy. A record that cannot be read is set aside and not believed.
Then, as when there is no record at all, what each installed folder's manifest claims stands in for it, so an id keeps its
version, developer and kind against a catalogue that would move it back or hand it to someone else. Those entries are marked
`"claimed": true` in `installed.json` until the item is installed again, since no signature was checked for them.

## Roadmap

**QOR OS 1.0 moves no money**
([QOR-0006](decisions/QOR-0006-qor-os-1-0-moves-no-money.md)). The Store
ships free items only.

- Stage 2 may proceed without its fee, which is a payment.
- Stage 3 below is QTOS's proposal, kept as the starting point. It is not
  decided. It waits for:
  - Demiurge's U-15 decided in writing;
  - a QOR record choosing the model: dollars, CGT, or both kept apart, and
    the platform's share;
  - the legal list and The Qt Company's confirmation.

  Until then no price, balance, commission or payout code reaches a build.

**Stage 2: the developer program.** A developer portal and an SDK preview
(Phase 4's installable SDK). Developers register once (the recommendation is
a one-time $13 per developer account rather than per item, with free items
always free to publish, waived for a founding group), sign their packages
with their own keys, and see privacy-preserving analytics: aggregated,
opt-in, with no tracking of individual users. Every submission is reviewed,
automatically and by people, before the store signs a catalogue that lists
it. Third-party themes first; widgets and apps once the Phase 3 sandbox
exists.

**Stage 3: paid items.** An 8% commission on paid purchases, made to work by
a prepaid QOR OS balance: people top up once (cards and Cash App Pay through
Stripe; Bitcoin only for top-ups and fees, through a processor that settles
in dollars) and purchases come out of the balance, so card fees do not eat
small sales. Stripe Connect onboards and pays developers. Stripe Tax collects
sales tax and VAT. Refunds follow local law (the EU's 14-day right on digital
content included).

**Before either stage.** Confirm with The Qt Company how Qt's commercial
licence applies to shipping QOR OS on devices and to an SDK that third parties
build on ([QTOS-0004](decisions/qtos/QTOS-0004-commercial-qt-and-proprietary-repository.md),
[QOR-0008](decisions/QOR-0008-before-a-public-release.md)). Write the developer agreement, terms, privacy policy and
content policy, including age ratings for games. Keep installs from outside
the Store (Flatpak, sideloading) open: that is what lets people trust the
Store.

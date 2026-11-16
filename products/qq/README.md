# QQ, the QOR Engine

<!-- Changed for QOR OS: precomputed shipped sky palettes, generation inputs and editable-colour fallback. -->

<!-- Changed for QOR OS: current project-document links; on Linux the Studio keeps the provider's key in the Secret Service (QOR OS's QOR-0011); gamepads come from QOR OS's controller service, QOR.Input, in place of XInput; each project's workspace in the Studio; the asset library; literal game-screen labels and their owning component context; literal Agent status/key problems; project-file list bindings and layout; literal logic problem descriptions; typed statistics overlay; bound inspector field loaders and literal source filenames; bound Agent step rows; literal group-selector names; bound ground/slab and texture context; checked Player measurement output. -->

An engine for virtual worlds and experiences, built on **Qt 6.12** under the owner's commercial licence
([the Qt rule](../../docs/decisions/qtos/QTOS-0004-commercial-qt-and-proprietary-repository.md)). QQ is developed in QOR OS
([QOR-0015](../../docs/decisions/QOR-0015-the-products-are-developed-here.md)). Use the current
[architecture](../../docs/ARCHITECTURE.md), [roadmap](../../docs/ROADMAP.md) and [verified checkpoint](../../OWNER.md)
for its place in the project, remaining work and validation limits.

QQ Studio edits a 3D world, keeps it in a Qontrol project, and plays
it; the QQ Player plays a scene on its own, natively and in a browser.

- **The world:** a procedural sky that also lights the scene, a sun with soft shadows, lamps, filmic tonemapping, bloom,
  ambient occlusion and multisampled edges; shapes with physically based surfaces; glTF 2.0 models loaded at runtime;
  and since P3.14 pictures on shapes and the ground, spot lights and HDR skies (below).
- **Play (P3.2):** shapes with bodies (`body`: none, static or dynamic; mass, bounce, friction) that fall, collide as
  their shapes and come to rest, under the scene's gravity; a **Player**, a character controller that walks, runs, turns
  and jumps, is stopped by walls and climbs low steps, with a camera that follows; **input** from the keyboard (WASD or
  the arrows, Space, Shift), the mouse (a drag looks around) and a gamepad (QOR OS's QOR.Input: sticks past a dead
  zone, A, the shoulders), read as one set of intentions (`Input`: move, look, jump, run) so every game works with every device and
  the agent can play one too; **particles** (`Emitter`: soft points of light that rise, spread and fade); **sound**
  (`Sound`: placed in the world, louder nearer, panned, silent beyond its reach, heard through the camera, in a browser
  too); and **logic** (`Behaviour` names a `logic/<name>.qml` file whose root is a `Logic`; it runs only
  in play and is rebuilt each time the file is saved, without stopping the world).
- **2D (P3.10, built 19 October 2026; ADR-105, ADR-106):** a scene with `view: "2d"` is the same world seen flat,
  through an orthographic camera on the plane z = 0, over a backdrop of its sky's colours. **Pixel art** (`pixelArt`, on
  by default) is scaled by whole numbers of screen pixels to an art pixel, as many as fit `viewHeight` units, with the
  camera on whole device pixels, and shown in exactly the colours it was painted (no bloom, tonemapping or dithering);
  drawn art fills its view exactly and keeps the 3D look. **Sprite**: an image or a frame of a sheet, played from
  `first` for `count` frames at `fps`, crisp or smooth, lit or not, flipped, and a body if it needs one. **TileMap**: a
  level drawn in [Tiled](https://www.mapeditor.org) and saved as JSON (`.tmj`, its tileset beside it or inside it, tiles
  as CSV or zlib-compressed Base64, flips kept), one layer each, drawn as one mesh; a solid one collides through the
  fewest rectangles that cover its tiles, and the one marked `bounds` is where a following eye stops. **Player2D**: a
  platformer's hero or a top-down one, under the same input as everything else, carrying a Sprite as its look and
  saying what it is doing (`activity`) so logic plays the right frames; `respawn()` puts it back. Since P3.11 it moves
  by QQ's own kinematics, not PhysX. Shapes and sprites with bodies stay on the plane and turn only about its depth
  axis. Shape's private collider and body factories bind to its owning instance. Its immediate parent's playing and view
  state can come from a Scene, Group or Prefab; changes and reparenting update the live state and plane locks. A missing
  parent or state does not make it live or lock it to the plane. Sprite’s private collider and body factories bind to its
  owning instance; tests cover dynamic sprites landing on
  static and kinematic sprite floors, contact reports, plane locks and crisp/animated pixels.
- **Steps, seeds and replay (P3.11, built 19 October 2026; ADR-105 decision 6):** play moves in ticks of 1/60 s,
  whatever the frame rate. Each frame's time is paid in whole ticks, at most eight a frame. Each tick takes the player's
  hands and the buttons clicked, and then, unless the world is paused, steps.
  - **Logic** has `onStep` for gameplay, timers in steps, `random()` and `randomInt()` from the scene's `seed`,
    `overlaps()` by QQ's own boxes, and an `outcome`. `onFrame` stays, for what is only looked at.
  - **The 2D player** moves by QQ's own kinematics: boxes against boxes, in doubles, the same on every machine.
    Triggers hear it by QQ's own measure, and it is drawn between steps so it moves smoothly at any frame rate.
  - **A run is kept** (`RunRecord`: the seed and what each tick took) **and replayed with no window and no physics
    world** to the same outcome (`replayRun`; the Player's `--record` and `--replay`). Because a replay never runs
    PhysX, an outcome it reproduces cannot depend on PhysX.
- **Animation (P3.12, built 19 October 2026; ADR-105 decision 7):**
  - **A model's glTF clips** (`Prop.clip`, `clipSpeed`, `clipLoops`; `clips` lists them, `part(name)` finds a joint or
    a mesh) play on the game's clock, skeletal and morph alike. Each step poses the model exactly, so logic reads the
    same joints in play and in a replay; frames draw it between steps. Changing clips puts what the last one moved
    back first.
  - **Tweens** (`tween(entity, field, to, seconds, easing, done)`) ease any number, vector or colour field, in steps,
    with nine easings.
  - **State machines** (`machine({ start, <state>: { enter, step, exit, on } })`) keep what something is doing, how
    long it has been at it, and the events that move it on.
- **Sound (P3.13, built 19 October 2026; ADR-105):**
  - **One-shots from logic** (`play(source, { at, volume, group, reach })`): a coin or a hit heard once, placed in the
    world where logic says or heard everywhere alike, and put away when it has played through.
  - **Music** (`Music`): a track heard everywhere from when play starts, and on through a pause, as a menu's would be.
    Logic changes it by setting `source`, and the old track fades out as the new one fades in, over `fade` seconds.
    Both private voice factories bind to the owning Music. The crossfade check covers ambient and WebVoice creation,
    gain, release and stop; passing it under private software tests does not demonstrate physical sound/browser playback.
  - **Volume groups** (`Mixer`): every sound is in one (`effects` unless named otherwise, `music` for music), and what
    is heard is its own volume times its group's level and the master level. Logic sets and reads them (`setVolume`,
    `volumeOf`), so a game's options screen turns the music down and leaves the effects alone. The Studio's Stop puts
    them back.
  - **In a browser** every sound is QQ's own voice on the page's Web Audio, each file decoded once. A placed one is
    quieter with distance from the eye and panned to the side of it it is on. The page's first touch or key starts its
    audio, as browsers require.
    The private sound factories bind to their owner and read the viewport’s `Camera`; tests check live perspective/orthographic
    switching and one-shot camera identity. Native logic checks do not demonstrate physical sound or browser playback.
  - **A replay makes no sound.** There `play()` returns nothing, so what it returns must not decide an outcome.
- **Materials (P3.14, built 19 October 2026; ADR-105):**
  - **Pictures on shapes** (`image`, `imageSize`): one copy every `imageSize` units on every face, whatever the shape's
    size, so a wall 22 units long has 22 units of bricks rather than a few stretched ones. The picture is projected
    along the shape's own three axes (`runtime/shaders/surface.frag`), so it moves and turns with the shape, and is
    tinted by `colour` (white shows it as painted). A `normalMap` gives the surface its fine relief for the light to
    pick out, and `crisp` keeps a pixel-art picture's pixels sharp. The lighting, shadows and sky light are Qt's own,
    exactly as on a plain shape. The Ground takes a picture too.
  - **Spot lights** (`Spotlight`): a cone of light (`cone` degrees, softening over `softness` of it toward its edge)
    shining down unless turned (`eulerRotation`), falling off as a Lamp does, with shadows. Logic sweeps one with a
    tween.
  - **HDR skies** (the Scene's `sky`, `skyTurn`): an equirectangular `.hdr` image seen behind everything and lighting
    it, turned about the vertical to bring its sun round. Its middle column faces +x and a quarter of the way across
    faces -z, as Qt lays it. Empty, the sky is made from the scene's four colours as before.
  - **Saving** brings pictures and skies into the project's `assets/`, the scene's own sky among them.
  - **In the Player**, every file a scene names, pictures and skies among them, comes only from the game's package
    (ADR-087): Qt Quick 3D reads pictures and models itself, past the engine's interceptor, so `SceneIO.resolvedUrl`
    gives nothing outside the package.
- **Saving (P3.16, built 20 October 2026; ADR-105 decision 7):**
  - **One small store per game** (`runtime/store.h`): logic keeps a value with `save(key, value)`, reads it back with
    `saved(key, fallback)` and lets it go with `forget(key)`. A store holds numbers, text, true and false, null, and
    lists and objects of them, 64 KiB in all, under keys of 1 to 100 characters. Anything else is refused with its
    reason in the log, and `save` returns false.
  - **The runtime keeps it, never the game.** It is written a moment after a change and as the Player closes: as a JSON
    file named for the game in QQ's saves folder (`Demiurge/QQ/saves/` in the system's data folder), or in the page's
    local storage in a browser. A game is its folder (the folder's name and a hash of where it is), so moving a project
    starts its store afresh. A store that cannot be read is kept aside, never written over, and the game starts
    without it.
  - **It is the one store the lockdown allows** (ADR-087). A game names no file, and its keys are only keys. Confined,
    every way it asks leads to its own store, so no game reads another's.
  - **A replay neither reads nor writes it.** A run's record carries the store as the run found it, and the replay
    plays from that copy, in memory, so a server checking a run needs nothing from the player's machine. The store is
    the player's, on their machine, where they can change it, so nothing rewarded rests on it.
  - **The Studio** says under the project's files what the game keeps between plays, and offers **Forget**, to play
    it as someone new would.
- **The Studio's look (ADR-108, 21 October 2026):** the tool register of Demiurge's one look, every value from `Look`:
  obsidian grounds, a gilt Cinzel wordmark, Barlow and Barlow Semi Condensed, IBM Plex Mono for code and figures. Its
  actions are `StudioButton`s: Play the one ember primary, Save a ghost with an ember edge while there is work to save,
  tools quiet until pointed at, all with the shared focus ring and still under reduced motion. A game's screen buttons
  answer the keyboard too (Tab, Enter, Space), and a replay presses them as it presses a click.
- **The Studio:** the scene's entities in a list, the selected one's fields in an inspector (built from what each type
  declares). Source filenames and field labels are shown as plain text; loaded editors keep their selected field when
  entity kinds change. Click to select in the viewport, handles to move along an axis, turn about the vertical or scale evenly,
  orbit and zoom, add (cube, sphere, cylinder, cone, lamp, model, player, emitter, sound, behaviour, the screen kinds,
  actions, and in 2D a sprite, a tile map and a 2D player) and delete. A 2D scene is edited flat: a drag pans, the wheel
  zooms, and the handles move things on the plane and turn them about its depth axis.
  **Play** (F5) plays a copy built from the scene's canonical text; **Stop** (Esc) throws the copy away, so the scene,
  its unsaved state and the selection are exactly as they were. **New behaviour** writes a starter logic file into the
  project, never over one that is there.
  **Editing (P3.15, first part, built 20 October 2026):** **Undo** (Ctrl+Z) and **Redo** (Ctrl+Y or Ctrl+Shift+Z) step
  through every edit, whatever made it (a field, a handle, an entity added, duplicated or deleted, a tool the agent
  used), since each step is the scene's canonical text (`studio/History.qml`); quick edits of one field (a slider
  dragged, a name typed) are one step, saving is not one, and the view and the selection stay where they were.
  **Duplicate** (Ctrl+D) puts a copy beside the selection, every field the same but its name. **Snapping** (the
  toolbar's choice: off, one art pixel, 0.1, 0.25, 0.5 or 1 unit) moves a handle along that grid, turns by 15 degrees
  and scales by tenths.
  **The scene's tree (P3.15, second part, built 20 October 2026):** a **Group** holds entities under one place, turn
  and scale, moved as one and written inside it in the file; the list is the tree, indented; **Ctrl+G** puts the
  selection in a new group where it stands, **Ctrl+Shift+G** takes a group's contents out, and the inspector's "In
  group" moves the selection between groups, keeping where it is in the world. **Keep as prefab** writes a group to
  `prefabs/<name>.qml` and places a **Prefab** of it where the group stood; **Add → Prefab…** places another. A
  prefab's parts come from its file, so the scene keeps only where each prefab stands and which file it is, a change to
  the file reaches every one the next time the scene opens or plays, and a click on a part picks the prefab.
  **Unpack** turns a prefab back into a group of its parts to change them; keeping it as a prefab again writes over its
  own file, and the scene's other prefabs of it are made again at once.
  **The Studio's tools (P3.15, third part, built 20 October 2026):** the **project's files** under the scene list
  (`studio/ProjectPanel.qml`): its scenes, prefabs and logic, and the assets a scene can use by kind (models, pictures
  shown as themselves, sounds, skies, maps), with a filter; a click uses one as its kind says (a model added, a
  picture put on the selection, a sky made the scene's, a prefab placed, a logic file opened). Its delegates bind to their
  own panel and list, the Studio reference has the actual window type, and the layout owns each kind mark's preferred size.
  The **logic editor**
  under the world (`studio/LogicEditor.qml`): a logic file with its lines numbered and coloured, built as you pause
  (`SceneIO.checkLogic`), what does not build listed by line as plain text, and saved with its button or Ctrl+S while it has the
  keys, so a scene that is playing picks it up at once; a Behaviour's **Edit logic** opens its file there. **F3**
  (or Stats) shows what the world costs (`studio/StatsOverlay.qml`): frames a second and a frame's time, draw calls
  and the graphics API, entities and bodies, and in play every body drawn as its collider. Its overlay reads the actual
  World's Qt `RenderStats`; its labels are plain text and sampling stays twice a second while it is visible.
  **Workspaces (QOR OS, 1 November 2026):** each project comes back as it was left, across a switch and across a restart:
  its open scene, its open logic file, its panels, the camera and the window (`studio/projectworkspace.h`). What is kept is the
  person's, never in the project's folder: `<AppDataLocation>/workspaces/<SHA-256 of the project's real path>.json`, read
  back as a stranger's file would be. **Change…** and **Recent** both go through `switchProject()`: unsaved work is asked
  about first (Save, Discard or Cancel, and Cancel leaves everything, a playing scene included, as it was); then nothing
  of the old project stays, not undo, the selection, the logic editor, the agent's run or what it was allowed; then the
  project goes past the same trust check as anything else that runs its files, and only then are they opened. New, another
  scene or logic file, Close in the editor and closing the window ask about unsaved work too. `--project` always wins;
  with nothing on the command line the Studio goes back to the last project; a capture keeps nothing. Launched by the
  launcher, a Studio that the launcher closes exits without asking (ADR-046).
  **Unsaved work survives a crash (QOR OS, 1 November 2026).** Once editing pauses for a second, what is unsaved (the scene's
  text and name, the logic file's text) is copied to `<AppDataLocation>/workspaces/<SHA-256 of the project's real
  path>.recovery-<session>.json`, beside the workspace and never in the project, written whole or not at all; only an edit starts
  that pause, so an idle Studio has no timer running. Each Studio keeps its own copy, named by a session of its own and locked
  while it runs, and keeps it whatever is still to be answered, so two Studios on one project never write over or remove each
  other's. It goes once the work is saved or discarded, and a launcher's close keeps it as it stands. Arriving in the project,
  past the trust check, the person is asked to **Restore** or **Discard** a copy that a Studio no longer running left, newest
  first, and told if a file has changed or gone since the copy was made; Restore changes no file, and what comes back is in
  this Studio's own copy before the old one goes. It is read back as a stranger's, as the workspace is, and a project's copy
  reaches no other. A copy also records which folder it was made for (its device and inode on Linux and macOS, its volume and
  128-bit file identifier on Windows), so another project put later where that one was is not offered it, even with the same
  creation time. An unavailable folder identity, or an older Windows copy recording only a creation time, leaves the copy's
  bytes alone and offers nothing for automatic restore; the older copy needs manual review. A project moved or renamed
  has a new real path, so its copies are not found there; moved back, they are offered again. A scene in no project has none.
- **Building a game (QOR OS, 1 November 2026):** **Build** brings in what the scene names from outside the project, saves what
  is on the screen, and checks that the project's scenes, logic and prefabs build: at most 2,000 QML files of 16 MB each, since
  the window waits for it, and past that it says so and builds nothing. Then it copies the project's `scenes/`,
  `logic/`, `prefabs/` and `assets/` into a folder of the game's own, `<GenericDataLocation>/qor-os/games/<name>-<hash of the
  project's real path>/`, with a `game.json` of what it holds and the digest of each file (`studio/gamebuild.h`). Play here runs the
  project as it stands, and the Library plays this copy, which changes only when built again and plays with the project gone. What a
  game cannot carry refuses the build, with the file and the reason: a link, a pipe, a name a system would read as more, a folder or
  file it cannot read, compiled QML, more than 8 GB or 50,000 files, and anything a scene, logic file, glTF or OBJ model, OBJ
  material library or Tiled map or tileset names that the game would not hold. That last is a scan of the text, which reads a
  path in logic from the logic's own folder and from the scene's, as `play()` and `spawn()` do, so a string in logic that only
  looks like a path can be taken for one, and the reason says so. It does not see a path a game puts together as it runs
  (strings joined, or what a template's `${...}` holds), a tileset saved as `.json`, or what any other model format names: a
  game that needs one of those finds it missing only when it plays. Dot-files and anything else in the project stay behind. A new build replaces the
  old only once it is whole, one build of a game runs at a time, and a build that dies at any step loses no game; a power cut
  can still leave files short behind a whole `game.json`, since only that is synced. While one runs, the Studio saves nothing
  and stays in its project. A project just opened, showing a new scene nobody has touched, asks for the scene the game starts in.
  `qq-studio --project <folder> --build <scenes/x.qml>` makes the same copy with no window and no display. It builds only a
  project already on the person's trusted list, and refuses any other, since nobody is there to be asked; the build asks that of
  the real folder it goes on to copy, so a link changed meanwhile changes nothing. It compiles nothing,
  as its answer says (`"compiled": false`). It prints `QQ-BUILD` and its result as JSON on standard output and exits 0 built, 3
  refused; on Windows the Studio is a windowed program, so a shell sees that only with its output piped or redirected.
  Importing a file still records no permission for it. What comes from the asset library does, in `assets.lock.json` (below),
  and the build puts it in `game.json`. A game is QML and JavaScript that QQ's runtime
  runs behind the Player's walls, with Qt's disk cache off (the Studio uses only the cache compiled into it). No native program
  is made, and none runs: a native library among the assets (an `.so`, say) is carried as a file like any other, and the Player
  refuses a `qmldir` in a game, which is how QML would load one, and imports only Qt's and QQ's own modules. Each project folder
  has one game: moved or renamed, it builds a second, and the first stays in the Library as it was.
- **The asset library (QOR OS, 3 November 2026):** packs the Store installed, used in a project
  ([QOR-0021](../../docs/decisions/QOR-0021-projects-engines-and-two-stores.md), decisions 5 and 6; `studio/assetlibrary.h`). The Store
  installs and the Studio only reads. Prism names the Store's folder of installed items when it starts the Studio (`QOR_ASSET_PACKS`), and
  only where the Store's format 2 engine exists, which is a demonstration build. `--asset-packs <folder>` does the same. With neither,
  there is no library and the Studio is as it was. The **Library** panel under the project's files lists each pack: its name, version,
  developer (said to be not checked here, which it is not), licence (its SPDX id) and where it came from, and on asking its files, a
  picture by a thumbnail drawn from the bytes whose digest was just checked, as PNG, JPEG or WebP by its name and only up to 4096
  pixels a side. **Use** copies the whole pack into
  `assets/<pack id>/`, and a click on a file copies only that file. The pack's licence and notice go with it either way. This needs the
  same trust as saving, as Update does, and happens neither while a build runs nor over a file already there that differs: that is asked first, and
  **Leave them** changes nothing. It happens whole or not at all, and writes **`assets.lock.json`** at the project's root. That file
  holds, for each pack, its id, name, version, developer, licence and provenance (the SHA-256 of the manifest the developer signed, that
  signature, and where it came from when the Store's record says that came from the signed listing), and each file copied: its path in the project and in the pack, its
  SHA-256 and the version it came from. It is the permission an import does not have. **Update** shows when the installed version is
  newer than the lock's, and says what it would do before it does any of it. A copy the person changed (its digest differs from the
  lock's) is never written over: that is looked at when the update is planned and again just before each file is set aside, and a copy
  changed in between stops the update with nothing changed. It is kept and listed, and the new version is put beside it
  (`tiles-1.1.0.png`, under a name neither the project nor the new version has) or not, as they choose. A beside copy is the person's
  from then on: no later update replaces or removes it.
  A copy they deleted is not brought back. Every other copy is replaced, a file the new version drops goes only if unchanged, and new
  files come too if the whole pack was used. Cancel changes nothing. A pack the Store has removed leaves the project's copies and lock
  as they are, and the library says it is no longer installed. A build carries the copies like any other asset, and its `game.json`
  gains `uses`: each pack with its version, developer, licence, provenance and the copies the game carries.
  **What is read as a stranger's.** The Store's folder: its `installed.json` and every pack's folder are size-capped, opened a name at a
  time with no link followed, and only `asset-pack` kinds are taken. A pack counts only if its folder holds exactly what its manifest
  lists, every file matching its digest, of a kind a pack may hold (pictures, skies, models, sounds, a model's buffers, plain text) and
  starting as one, every model naming only files beside it in the pack. Names, developers and paths must be NFC with no control or
  format characters (no right-to-left override, no zero-width joiner), and `items/` itself may not be a link. The Studio checks this
  itself rather than trust the Store's check. On Linux a pack checked whole is listed again without being read again while none of its files has
  changed on disk (device, inode, size, modification and change times); what is copied, or drawn as a thumbnail, is read and its digest
  checked again first. It holds no developer's key, so it cannot check the signature: the lock keeps it, so that can be checked later. Each
  project's `assets.lock.json` is read the same way on every open: too large, not JSON, of another format, or with a path that is not
  plainly a file under `assets/<its pack>/`, that climbs or goes through a link, and none of it is used. Then the project opens, the
  library says why, nothing it names is changed, and a build is refused until the lock is put right or removed. A project with no lock
  is as before.
  **Across a crash.** What is new is written beside the project first, in `.qq-assets-<tag>/`, a folder named with a dot that a build
  leaves behind. A journal in the person's own store, never the project's, says what is to move, the SHA-256 of each file it places, and
  which folder it is for (its device and inode on Linux and macOS, its volume and 128-bit file identifier on Windows). The new lock is checked as it would be
  read before anything moves. Then the old files are set aside, the new ones are moved into place, and the lock is written, which is
  the moment it happens; it carries the change's own tag, so no lock is mistaken for the one before it. The next time the project is
  looked at, a change whose lock was written is finished and one whose lock was not is put back. Putting back moves a placed file back
  only while it still holds what was placed, and a set-aside file only where nothing now is. A file the person edited since stays as it
  is, and what was set aside and could not go back stays in the staging folder, and both are said. A journal for another folder now in
  that place, or whose staging folder has gone (`git clean`, a restore), moves nothing and is dropped, and that is said. If the
  project folder cannot be identified, its journal and staged files stay; bringing the original folder back permits recovery.
  Older timestamp-only Windows journals also stay, including negative values for dates before 1970, for manual review rather
  than automatic migration. While a journal remains,
  another asset transaction is refused, so it cannot overwrite the unresolved record. New transactions require a readable folder
  identity; Windows uses the volume and file ID from [the Windows API](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_id_info),
  refusing unavailable or all-zero identifiers. On Linux each
  step is made durable before the next by one `syncfs` of the project's file system; elsewhere each file and folder is synced. One
  change to a project's assets, or one build of it, runs at a time, by a lock in the person's own store: that keeps apart the Studios
  sharing that store, and not another person, a Studio with another store, another computer on a shared folder, or any other program.
  `qq-studio --project <folder> --asset-packs <folder> --use-pack <id> [--pack-file <path>]... [--replace]`, and `--update-pack <id>
  [--beside]`, do the same with no window. They take only a project on the person's trusted list, print `QQ-ASSETS` and their result as
  JSON on standard output, and exit 0 done or 3 refused. In the window Use and Update run on a thread of their own, so the window
  goes on drawing, and while one runs nothing is saved or built and the project stays, each saying why. A delayed Replace or Update
  answer checks trust and the current build/asset work again before writing; a fresh request after restoring trust works normally.
  Listing the packs and planning
  an update stay on the window's thread. Provenance fields may contain up to 8,192 characters each; the whole lock must still fit
  its 8 MiB limit, or the change is refused before any project file moves. **Not yet:** a pack's own pages in the Studio (its description,
  its previews). A moved project leaves a change cut short in
  its old place's journal.
- **The files:** a scene is `scenes/<name>.qml` in the project, in one canonical layout: read and written back it is
  the same bytes, and one changed value is one changed line in Projects. Models and sounds are copied into `assets/`
  and logic into `logic/`, so the project is whole on its own; a Tiled map brings its tileset and image, and one whose
  tileset is outside its folder is refused with the reason. 2D scenes from the launcher's preview (`.qq.json`) import
  as 2D scenes of drawn art (100 pixels to a unit, framed as they were made, their background as the backdrop),
  emitters included; motion and the pointer follow are named as not brought in.
- **The QQ Player:** plays one scene as a game, full window, natively (`qq-player [scene.qml]`, the bundled playground
  without one) and as a WebAssembly build in a browser, and measures how long its first frame takes.
- **The launcher:** **Open in QQ Studio** on the launcher's QQ surface starts the Studio on the project open there.
- **Publishing (P3.4, built 10 October 2026):** **Publish** saves the scene, then asks the launcher that started the
  Studio to commit the project through Qontrol and publish it as its **Cartridge** (ADR-088): the first time a DRC-369
  asset is minted, holding the manifest of every file at that commit; each time after, the same Cartridge is revised
  to the new commit; with nothing new, nothing is asked. The launcher's own window asks first and names QQ Studio; the
  Studio holds no key and never sees a transaction. It asks over a channel only the process the launcher started can
  use (ADR-046: a one-time token on its stdin and the operating system's word on who connected), and exits when the
  launcher closes it. A Studio started any other way works as before but cannot publish, and its Publish button says
  why.

**P3.3 under way (10 October 2026): the Studio's tools for agents.** A running QQ Studio offers fourteen tools to
language models over MCP (the Model Context Protocol): read the scene and what can be added, add, change and remove
entities, write logic, play, stop, hold the controls, wait, place the eye, capture a frame, read the log. Any MCP client
reaches them through `qq-mcp` (below); what an agent changes shows in the Studio as unsaved, and only the creator saves
and commits. **And the design loop**, in the Studio's **Agent** panel: describe a game, and Claude (Opus 5.5, ADR-085)
writes a brief, builds it with the same tools, plays it, looks at captured frames, judges them and revises, every step
shown as it happens; supplied text stays literal and each row uses its own model data. The creator then saves and commits it, or undoes the run. The provider's key is kept in Windows
Credential Manager (on Linux, the Secret Service) and never logged. **Generation is paid for by whoever uses it** (ADR-086): the key is the creator's
own, Anthropic bills it directly, the panel says so before a key is given and shows each run's token use, and Demiurge
holds no provider key and pays for no one's generation. Still to come in P3.3: its proof, a scene built from a
description with no human edit, made by whoever runs it with their own key.

The panel's required controller remains structurally compatible with DesignLoop and inert test controllers.
Its accessor reads the current object for bindings and actions, including after replacement. A refused key submission
still clears the password field and displays the problem as plain text; no new key storage or provider path is added.

## What is here

| Path | What it is |
| --- | --- |
| `runtime/` | The QML module `QQ`: `World` (the viewport, its sky, light, look and hearing), `Scene` (the root a file holds), `Game` (a world playing one scene file: what the Player shows), the entities `Sun`, `Lamp`, `Ground`, `Shape`, `Prop`, `Player`, `Emitter`, `Sound`, `Behaviour`, `Trigger` (a volume that hears what enters and leaves it, P3.7), the screen kinds `ScreenText`, `ScreenBar`, `ScreenButton`, `ScreenImage` and `ScreenPanel` (the game's interface, drawn over the world on the World's overlay, each in a named screen; built on `ScreenPart`, P3.8), `Music` (a scene's music, fading from track to track, P3.13), `Spotlight` (a cone of light, P3.14), `Group` and `Prefab` (entities kept together, and a group kept in a file of its own and placed as often as wanted, P3.15), `InputAction` (an action a player can take, with its keys, mouse buttons and pad buttons, P3.9), the 2D kinds `Sprite`, `TileMap` and `Player2D` (P3.10), and `Logic` (what a logic file is, with the gameplay API of P3.7: `onStarted`, `onContact`, `onEntered`, `onExited`, `ray`, `spawn`, `remove`, `after`, `every`, `cancel`; and for the interface, P3.8, `onClicked`, `showScreen`, `hideScreen`, `onlyScreen`, `pause` and `resume`; and for actions, P3.9, `onPressed`, `onReleased`, `held`, `rebind`, `bindingOf` and `pointing`; and for steps and runs, P3.11, `onStep`, `steps`, `random`, `randomInt`, `overlaps` and `outcome`; and for animation, P3.12, `tween` and `machine`; and for sound, P3.13, `play`, `setVolume` and `volumeOf`; and since P3.15 `spawn` places a prefab, or puts what it makes in a group, and `remove` takes a group or a prefab away whole; and for saving, P3.16, `saved`, `save` and `forget`; and for a game played inside another application, ADR-117, `askHost` and `onTold`, the host hearing `asked` and answering with `tell` on the `Game` or `Scene`, what it tells heard at the next tick and kept in the run). `Look` is the style of the Studio and of a game's screens, Demiurge's one look (ADR-108): every colour, face, size, corner and duration from `runtime/tokens.h`, which `docs/design/tokens.mjs` generates from `docs/design/tokens.json`; the faces Cinzel and Barlow Semi Condensed carried in `runtime/fonts/` (and Barlow and IBM Plex Mono in the Studio's own `studio/fonts/`), under the SIL Open Font License; and reduced motion as the launcher passes it on (`QOR_REDUCED_MOTION`) or the system says. In C++: `SceneIO` (canonical writing, loading, saving, playing a copy, adopting files, importing 2D scenes, and the editing operations the Studio uses and the agent will, P3.3), `Input` (keyboard, mouse and gamepad as intentions), `LogicFile` (builds a logic file and rebuilds it on each save), `tilemap` (Tiled's maps read, their solid tiles merged into colliders, and `TileGeometry`, a layer as one mesh), `kinematics` (QQ's own: boxes swept against boxes, P3.11), `run` (`Random`, seeded and the same everywhere; `RunRecord`, a run kept; `replayRun`, a run played again with no window), `clips` (a glTF model's animations, taken from Qt's own wall-clock playing and posed on the game's steps, P3.12), `Mixer` (the volume groups, and a sound file's length, P3.13), `GameStore` (a game's one store, kept between plays, P3.16), `shaders/surface.vert` and `surface.frag` (a Shape's pictured surface, P3.14), `SoftDot` (the particles' sprite, made in memory) `gpu` (draw with the faster of two GPUs), `Confinement` (what a game may reach in the Player, ADR-087) and `WebVoice` (a sound in a browser, on the page's Web Audio and panned, so a game never needs Qt Multimedia's QML) |
| `studio/` | QQ Studio. `qq-studio [model.gltf] [--project <folder>] [--capture <frame.png>] [--launcher-channel <name>]`; `LauncherLink`, its side of the QOR host protocol (publishing through the launcher that started it, ADR-088); `History.qml` (undo and redo), `ProjectPanel.qml` (the project's files), `LogicEditor.qml` with `LogicHighlighter` (the logic editor) and `StatsOverlay.qml` (F3), P3.15; `ProjectWorkspace`, what is kept of each project, outside it; `AssetLibrary` with `LibraryPanel.qml`, the asset library (`--asset-packs <folder>`) |
| `agent/` | The agent's side of the Studio (P3.3): `McpServer` (MCP's JSON-RPC, served on a local pipe only this user can open), `StudioTools` (the fourteen tools, working the Studio's own window functions, and `StudioLog`, what the Studio has said), `DesignLoop` (a description designed, built, played and judged by Claude through those tools; ADR-085), `keychain` (the provider's key in Windows Credential Manager, or the Secret Service on Linux) and `qq-mcp` (the program an MCP client starts, relaying its standard input and output to the running Studio) |
| `player/` | The QQ Player. `qq-player [scene.qml] [--capture <frame.png>] [--measure <result.json>] [--seed <n>] [--record <run.json> [--ticks <n>]] [--replay <run.json>]` (a replay has no window and exits 0 when it reaches the outcome kept, 4 when it does not); its playground (a small game with everything P3.2 plays, which counts its visits in its store) bundled inside it, and its measurement lists any of the scene's logic that did not build; `measure-web.mjs`, which plays the browser build in a headless browser, measures it, and loads it again to find the playground's store kept |
| `tests/` | `tst_world` (rendering, judged by pixels), `tst_scene` (scene files), `tst_studio` (the real Studio window, worked by clicks, drags and keys), `tst_play` (physics, the player, input, particles, sound, logic and gameplay in logic, 2D pixel for pixel, in real frames, and the Orb Run and Ember Climb samples played), `tst_player` (the real Player executable, its runs and its stores across starts), `tst_lockdown` (what a game's logic may reach in the Player, its one store among it, ADR-087), `tst_publish` (publishing through a stand-in launcher, P3.4), `tst_agent` (the tools over MCP, in the real Studio and through the real `qq-mcp`), `tst_workspace` (projects switched and reopened in the real Studio, with nothing mixed up, unsaved work through a crash and a launcher's close, and what is kept of them read back from the hostile side), `tst_gamebuild` (a project built into a game by the real Studio and the command line, from the hostile side too, a build stopped dead by a real process at each step, and what was built played by the real Player), `tst_assetlibrary` (the asset library from the hostile side, a change cut short at each step and the real `qq-studio` killed mid-update, the window, and the demonstration pack installed by the real StoreEngine where `QOR_PRISM_BUILD` names a build of Prism), and `first_journey` (`tools/dev/first-journey.py`: the build, Prism's Library and the Player, the real programs, run where `QOR_PRISM_BUILD` names a build of Prism) |
| `tests/fixtures/` | `orb.gltf` (written by `make_orb.py`), `chime.wav` (written by `make_chime.py`), `logic/spin.qml`, `scenes/sample.qml` (every entity type, canonical), `first-light.qq.json` (the 2D preview's starter scene), and the 2D fixtures `make_2d.py` writes: `tiles.png`, `tiles.tsj`, `level.tmj` (a Tiled map with a mirrored tile and two layers) and `sheet.png` (eight frames), and `rig.gltf` (written by `make_rig.py`: a bar skinned to two joints that "bend" turns, and a box whose morph target "puff" doubles) |
| `samples/` | Games made only from kinds and logic files, each proving a step of P3 and played by the suites (ADR-105): `orb-run/` for P3.7 to P3.9 and P3.12 to P3.16, and `ember-climb/`, a pixel-art platformer, for P3.10 and P3.11 ([`samples/README.md`](samples/README.md)) |
| `build.ps1` | Builds with the owner's Qt, MSVC 2022 and Ninja and runs every test; `-Deploy` makes a self-contained Studio; `-Web` builds the Player for the browser and measures it |

ScreenText and ScreenButton labels render game-supplied text literally. Markup is shown as characters rather than parsed as
rich text; changing one screen part updates its own drawing. ScreenButton keeps its existing uppercase label formatting.
All five screen kinds bind their drawing Components to their own screen part's context, including when ScreenPart creates
them on the World's overlay. Text, sizing, fill, images and actions continue to refer to that instance; these drawings use
no delegate model context. ScreenImage is ready only when its actual Image has finished loading; a malformed file is not ready.

`Game.playingScene` is the loaded QQ scene, or null before loading and after release or failed startup.
`Game.scene` is inherited from Qt's `View3D`: it is the viewport's own scene root and remains separate when a game
loads, restarts or releases. The Player and ARQADE read `playingScene` for game state; hosts using the former
loaded-scene property must use that name too.

The public `playingScene` and `follow` handles remain `QtObject`s. SceneIO accepts compatible 3D scene nodes,
and a follower can implement either the 2D or 3D interface. Game accesses their fields through private dynamic
accessors, preserving object identity, live scene edits, follower replacement, camera tracking and host messages.
It does not require the loaded object to have one generated QML class. The scene suite covers both a QQ Scene
and a compatible ordinary 3D node, including startup seed/recorder forwarding and release. Script errors and binding
loops fail this coverage; the accessors read the current handles directly without a separate cached binding.

Shape's `push`, `applyForce`, `setVelocity` and `setSpin` require both the `dynamic` body mode and an actual
`DynamicRigidBody`. A missing body, ordinary node or mismatched static body returns false without a script error.
The public `physics` property remains a `Node`; the private body factory checks its result as `PhysicsBody` before
assigning collision shapes. Static, kinematic and scenery modes still refuse these motion commands.

Sprite's `push`, `applyForce` and `setVelocity` use the same actual-dynamic-body check while retaining the public
`Node` handle and existing body-mode guard. Its private factory result is a `PhysicsBody`. Music, Sprite and TileMap
read the immediate parent's compatible state directly, preserving strict play activation, truthy pause/replay flags,
pixel-scale updates, reparenting and detachment. Sprite body identity, collider ownership and plane locks stay intact.

Trigger's `inside` count reads an actual `TriggerBody`; a missing volume or another kind of node gives zero.
Its public `physics` property remains a `Node`. Playing state still follows the immediate structural parent, including
groups and prefabs. Enter/exit callbacks forward a body's optional QQ entity, or null when no entity is present.
The private box and sphere colliders remain bound to their owning trigger as its scale changes.

The 3D `Player` moves only an actual `CharacterController`. Its public `physics` property remains a `Node`, but
missing or incompatible bodies stop its frame animation and receive no motion or teleport commands. Playing,
pause, gravity and player selection still use the immediate parent's structural scene interface. The factory
registers its owning player only when that parent has no selected player.

Player2D keeps its named-entity lookup and public `QtObject` sprite property. Visual updates change position and
facing only for an actual QQ `Sprite`; a lookalike node or another entity stays untouched. Missing names remain
harmless. Its kinematic carrier, structural scene clock, player selection and deterministic movement are preserved.

The Agent panel also shows supplied status, key-storage problems and messages literally. Markup in those strings stays
text, including when the status changes during a run.

## Build, test, deploy

QQ packages precomputed lighting for the two palettes authored in its bundled scenes: the World default (also used
by Orb Run) and Ember Climb's colours. Selection compares all four colour values, including alpha; it does not depend
on a game name or path. Other colour combinations keep the procedural sky and its batched updates. An explicit sky
image still takes precedence, and exposure, orientation, skybox blur and all other rendering settings are unchanged.
The runtime selects these same textures in the Studio, Player and ARQADE. Cross-platform validation is separate from
their native ARM64 proof; this optimization does not make arbitrary edited palettes fast on a software renderer.

`runtime/skies/manifest.json` records the inputs, Qt version, hashes and pixel comparisons for both baked textures.
Their checked-in EXR source images preserve the procedural texture's half-float pixels; the KTX files are the runtime
resources. The two KTX files total 33,546,472 bytes before Qt resource compression. No downloaded image is used.
To rebuild an asset, use the installed commercial Qt 6.12 `balsam <source.exr> --outputPath <separate-output-folder>`
inside the existing guarded software-rendering workflow. Review the output hash and compare rendered pixels against
the procedural palette before replacing the KTX and updating its manifest and scene test. Normal application builds
embed the reviewed KTX bytes and do not launch a baker or render on the build host. The EXR inputs and manifest are
development inputs, not runtime resources. A future Qt or renderer change needs its own comparison.

The 14 November integration builds all QQ targets through the native `q-pi-release` preset without compiler/license
diagnostics. All 85 scene results pass, including palette selection, embedded texture hashes, colour and alpha edits,
exposure, rotation and explicit-image precedence. The unchanged Store-installed Player suite passes three results in
8.371 seconds under its original 60-second deadline; all 11 world-rendering results pass in 340.938 seconds. Hardening
accepts all 16 QQ programs and the sanitizer program. All 85 scene results also pass offscreen under ASan/UBSan with
leak checking enabled. Runtime lint still fails with exactly the previous 53 findings, none added or removed.
Evidence: `build/development-2026-11-14/session-12/`. Windows heavy checks remain stopped below the 3,072-MiB floor;
all-file hooks, full products and platform acceptance remain incomplete.

The bundled Player playground also carries precomputed lighting for its explicit HDR sky. Its original `dusk.hdr`
remains in `player/playground/assets/`; `dusk-lighting.json` records the original image's source licence, generator,
hashes and pixel comparison. The scene uses `dusk.ktx`, embedded by the ordinary Player build. Other HDR images keep
their existing loading path. The native comparison is pixel-identical and reaches its first frame in 0.762 seconds
instead of 61.384 seconds. The real bundled Player reaches its first frame in 5.279 seconds and its first-frame
capture passes. The complete Player suite still fails its unchanged 20-FPS floor at 0.5 FPS for the playground and
18.9 FPS for Ember Climb; the kernel's unavailable Landlock check is skipped. These results do not accept real-time
performance. Receipts are in `build/development-2026-11-14/session-15/`.

The complete native suite continuation passes gameplay (58 results), lockdown
(24), publishing (11), Player network confinement (three), Store launch (three),
Studio (29), workspace (241) and game builds (195). The asset library passes 91
with its optional benchmark skipped. The real Studio open/edit/save/build/launch
journey passes. `first_journey` also passes in 202.406 seconds under its original
300-second limit, including Prism's Library, rebuild and playing after removal
of the source project. Prism uses a test launcher to record arguments; the real
Player then runs those arguments on a private display. This does not prove the
Player rendering inside Prism on hardware.

The agent suite retains one short log-timing failure and its live-provider skip.
The complete Player rerun retains two frame-rate failures and its unavailable
Landlock skip. All 53 prior runtime lint warning lines remain. Assertions,
rendering quality, the approved gameplay timing boundaries, private software
isolation and the 3,072-MiB floor are unchanged. Scene/world results are retained
through matching current binaries. Full QQ qualification is still open; exact
commands, failures, skips and source/binary reconciliation are in session-15.

The 15 November Game follow-up replaces implicit generic-object member access with dynamic accessors while keeping
the public object handles. All 24 Game lint findings are removed; the other 29 warning lines are unchanged. Both
scene variants pass live-edit, follower, camera, startup, host-message and release coverage. An initial candidate's
intermediate bindings emitted transient null-property warnings despite passing value assertions; it was rejected.
The strengthened test fails that candidate and passes the direct accessors without script warnings. All 87 scene
results then pass normally and offscreen under ASan/UBSan with leak checking, and complete gameplay passes all 58.
Store launch passes three results; the packaged-game creator journey passes in 201.985 seconds under its unchanged
300-second limit, using the same separate Prism test-launcher and private-display Player steps described above.
The full Player suite retains 13 passes, two FPS failures (0.5 and 18.9 against 20) and the unavailable-Landlock skip.
Original assertions, rendering quality, approved timing boundaries and isolation remain unchanged. Evidence:
`build/development-2026-11-15/session-01/`; full QQ and hardware qualification remain open.

The next 15 November increment gives Behaviour direct accessors for its current parent and logic object. LogicFile's
compatible-object contract, public handles, callback receivers, frame guards and prefab lookup remain intact. New
coverage passes against both the original and changed Behaviour: QQ Logic and a compatible QtObject receive the
same nested messages, step/frame arguments and target events, follow target/parent changes, and retire replaced
logic. The test also rejects script errors and binding loops. All 89 scene results pass normally and offscreen
under ASan/UBSan with leak checking. Exactly 11 Behaviour lint findings are removed; the remaining 18 lines are
unchanged, so lint still fails. Complete native gameplay passes all 58 results with its approved timing boundaries
and source unchanged. Store launch passes three, and the packaged-game journey passes in 201.377 seconds under
its original 300-second limit, with the same separate Prism test-launcher and real Player steps. Agent records
21 passes, the identical prior log-timing failure and one optional live-provider skip. Player records 13 passes,
two FPS failures (0.6 and 19.5 against 20), and the unavailable-Landlock skip. Hardening passes 16 QQ programs
and the sanitizer program. All 1,966 resource samples remain at or above 5,427 MiB available; isolation and the
3,072-MiB floor are unchanged. Evidence: `build/development-2026-11-15/session-02/`.

The next parent-state increment covers Emitter, Ground, InputAction, Prop, Sound and Spotlight with direct reads of
their current parent's compatible interface. Strict boolean activation, truthy pause/replay guards, input declarations,
Ground body identity, Prop's between-step pose and sound replay suppression are preserved across reparenting. TileMap's
private factory result is typed as StaticRigidBody; tests retain collision-box geometry, identity and ownership.
Eleven focused results pass before and after the change, and the complete scene suite passes 97 normally and
offscreen under ASan/UBSan with leak checking. Twelve lint findings
are removed with none added. The six remaining spatial-audio findings retain their messages, with one Sound location
shifted by the edit; lint still fails. Evidence: `build/development-2026-11-15/session-03/`.
Complete native gameplay also passes all 58 results with its approved timing boundaries and source unchanged,
including particles, model animation, sound replay suppression, input rebinding and tiled collisions.
Store launch passes three; the packaged-game creator journey passes in 201.631 seconds under its original
300-second limit, including edit/rebuild and launch after removing the source project. Agent retains its identical
log-timing failure (21 pass, one fail, one optional skip); Player retains the two 20-FPS-floor failures at 0.5 and
19.7 FPS (13 pass, two fail, one Landlock skip). Hardening passes 16 QQ programs and the sanitizer program.
All 1,997 resource samples stay at or above 5,329 MiB available; the original guard, isolation and limits are unchanged.

The following structured-lint audit corrects the console-only remaining count: Runtime had 20 findings, including
14 in Music, Sprite and TileMap omitted from its console output. Studio has 18 independent AgentPanel findings;
Player has none. The 14 source findings are now removed, with six runtime and 18 Studio findings remaining.
The new Sprite regressions reproduce script errors for ordinary and static physics handles; all 14 focused results
pass after the fix. Existing scene assertions and approved gameplay source are unchanged. A temporary metadata copy
with the missing QtQuick3D dependency clears only the six SpatialAudio findings while invalid properties and inherited
property types still fail. No SDK or lint policy was changed. Evidence: `build/development-2026-11-15/session-04/`.
After the source fixes, complete Runtime lint passes with the corrected copy. Normal lint remains failed against
the unchanged installed metadata at that diagnostic checkpoint.
The complete scene suite passes all 109 results normally and offscreen under ASan/UBSan with leak checking.
Complete native gameplay passes all 58 results with no failures or skips, including music, sprite animation,
tile-map rendering and collision coverage under the unchanged approved timing boundaries.
Store launch passes three results; the packaged-game creator journey passes in 201.365 seconds under its original
300-second limit, including edit/rebuild and playback after removing the source project. Agent retains the identical
log-timing failure (21 pass, one fail, one optional provider skip). Player retains the two 20-FPS-floor failures at
0.5 and 19.5 FPS (13 pass, two fail, one Landlock skip). No assertion, timing boundary or rendering setting changed.

The following build integration supplies the proven SpatialAudio dependency through a local tooling-metadata copy,
using `cmake/QorSpatialAudioMetadata.cmake`. It leaves the installed SDK and application import environment unchanged.
Fixed SDK metadata needs no override; unexpected/incomplete metadata and unreviewed broken versions stop configuration.
All six helper tests pass. The normal runtime console and JSON lint targets now pass with zero warnings; Player has
none, while Studio's 18 findings retain their failing status. Valid spatial types resolve and invalid properties/types
remain rejected. The complete native scene suite passes 109 results normally and offscreen under ASan/UBSan with
leak checking; eight targeted audio results pass. The creator journey passes in 199.848 seconds under its original
300-second limit. All eight audited executables match the preceding validation byte for byte; the full gameplay,
Agent, Player and Store suites were not all rerun for this metadata increment. All 862 memory samples stay at or above
6,588 MiB against the unchanged 3,072-MiB floor. Evidence: `build/development-2026-11-15/session-06/`.

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

The 15 November Windows continuation at source `5d1359e` builds all QQ targets
in `build/ci-products/qq`, commercial Qt 6.12.0/MSVC 2022, `Release`, serially
with warnings as errors and no compiler/licence diagnostics. All 15 programs
pass hardening; aggregate lint and all three structured module reports have
zero warnings. Six metadata helper
tests and six focused AgentPanel results pass. Those panel checks use inert
controllers, private settings and offscreen software rendering, with no audio
engine, keychain or provider calls. Full Windows product runtime suites still
need a qualified audio/keyring boundary; `QOR_AUDIO_OUTPUT=null` does not isolate
Qt Multimedia on Windows. The original coordinator, continuous 3,072-MiB guard,
test assertions and deadlines remain unchanged. Exact selection, environment,
output and corrected wrapper failures: `build/development-2026-11-15/session-08/`.

The 16 November read-only kernel audit at source `d79c6ab` explains why the
captured buffer-cache probe sleeps. The running Pi kernel is
`6.18.50+rpt-rpi-2712`; the V3D and DRM module files match their loaded build IDs.
The actual V3D instructions convert timeout zero to one jiffy, a kernel timer
tick, before the buffer fence wait. With `CONFIG_HZ=250`, that is a nominal
4 ms timeout; timer rounding and scheduling can extend elapsed time. The exact
package-declared kernel source corroborates the path into `schedule_timeout`.
This identifies a kernel polling defect, not a measured breakdown of each wait
or proof that the GPU finished. The unique fence and Studio pass remain unknown.

The installed but unbooted `6.18.60+rpt-rpi-2712` module has identical wait-function
instructions and retains the defect. Upstream Linux commit `226666556a9b`
preserves zero as a poll through `drm_timeout_rel_to_jiffies`; no fixed Pi package
has been installed or qualified. Next: prepare a reviewable Pi-kernel backport
candidate and busy-buffer regression plan using that correction, before any
kernel installation or reboot. Do not change QOR's assertions or bypass waits.

No graphics/test run, production source, executable, image, library, kernel,
service or system setting changes. The audit uses the original resource guard;
41 and 34 samples stay above the 3,072-MiB floor. The first job's module checks
succeed but its header-only helper search has no match; that whole job is not
reported as passing. The source lookup resolves the helper in the driver header.
Sealed reference/observer results remain 3 passed, 1 failed, 0 skipped
(`times > 5` with 3), with later notice/cursor assertions unreached. The independent
llvmpipe failure and 97 unresolved image outliers stay open. Release, sanitizer,
broader products and isolated CI acceptance are unrun in this continuation.
PR #10 stays draft. Evidence: `build/development-2026-11-16/session-05/`;
session-04 remains sealed.

Earlier 14 November sky/startup follow-up: World queues its first procedural sky with the same colour-update batch used
for later edits, so initial properties do not create the same texture twice. The regression checks construction
count, final colours, ownership, replacement and disposal; it fails against the original World. All 84 scene results
pass normally and offscreen under ASan/UBSan with leak detection enabled. All 11 world-rendering results pass, without
failures/skips, in 1,229.256 seconds. The full optimized native build has no compiler/license-client diagnostics;
hardening passes 16 QQ programs and the sanitizer program. Runtime lint still fails at the same 53 warnings.
The Store test now waits for its own timed-out Player to exit before teardown. Its unchanged 60-second deadline
failed before and after that change; separate diagnostics reached the first frame at 64.204/63.827 seconds and exited normally.
Those longer diagnostics demonstrate neither a speedup nor Store acceptance. The original assertions, tolerances,
deadlines and rendering settings remain. Evidence: `build/development-2026-11-14/session-06/`. Whole-product, Windows,
hardware and isolated-CI acceptance, plus the older Orb/Wayland/ARQADE and upstream-audit gates, remain open.

14 November Player2D follow-up: seven new rows cover valid and mismatched visuals, missing names, carrier ownership
and dimensions, selection, strict activation, paused interpolation, replay state and parent changes. Two rows reproduce
the original wrong-object mutation and script error. The full optimized native build has no compiler/license-client
diagnostics. All 83 scene results pass normally and offscreen under ASan/UBSan with leak detection enabled. Four existing
tiled-level, top-down, recorded-replay and 2D-trigger tests pass before and after the fix, six results per run with no
failures/skips; their source, assertions, tolerances and durations are unchanged. Hardening passes all 16 QQ programs
and the sanitizer program. All 14 Player2D lint findings are removed and none added; 53 runtime findings remain, so the
whole lint gate still fails. The final audit verifies 1,404 native fingerprints, current build output, tested binaries
and sanitizer instrumentation. Evidence: `build/development-2026-11-14/session-05/`. Broader product, hardware, isolated
CI and the older Store/Orb, Wayland leak and ARQADE gates remain open and were not rerun for this increment.

14 November 3D Player follow-up: seven new regression rows check actual, missing and incompatible controllers,
strict activation, pause changes, selected-player ownership, reparenting and detachment. The original runtime
fails the ordinary-node, static-body and trigger-body rows; two earlier test-fixture import errors are preserved
separately. Existing tests and movement calculations remain unchanged. The complete native optimized build has
no compiler/license-client diagnostics. All 76 scene results and six selected keyboard/controller/trigger results
pass without failures/skips, and hardening passes 16 programs. The same 76 scene results pass offscreen under
ASan/UBSan with leak detection enabled; its program passes hardening. All nine Player lint findings are removed
without adding any; 67 runtime warnings remain, so the whole lint gate still fails. The final audit checks the
1,404 native source fingerprints, current build cache, tested binaries and sanitizer instrumentation. The original
coordinator and 3,072-MiB floor are unchanged. Evidence: `build/development-2026-11-14/session-04/`.
Full product, hardware and isolated CI acceptance and the older Store/Orb, Wayland leak and ARQADE findings remain open.

14 November Trigger follow-up: an ordinary node can no longer supply a false collision count. Four new scene results
check both collider forms and their owning instance, live dimensions, parent changes, detachment, strict activation,
missing volumes and optional entity callbacks. Existing tests remain unchanged. The full optimized native QQ build
has no compiler/license-client diagnostics; all 69 scene results and six selected trigger/group physics results pass
without failures/skips. Hardening passes 16 programs. Seven Trigger lint findings are removed and none added; 76 runtime
warnings remain and the whole lint gate still fails. Original failures and passing results are preserved under
`build/development-2026-11-14/session-03/`.
All 69 scene results also pass offscreen under ASan/UBSan with leak detection enabled; its test program passes
hardening. The prior Wayland leak and broader product/Store/Orb/ARQADE findings remain separate, unrerun gates.

14 November motion-command follow-up: seven new rows check all four Shape motion commands against dynamic, static,
kinematic, absent and mismatched bodies. The original runtime raises script errors for the two mismatched types;
the corrected runtime passes all 65 scene results and seven selected physics results. The full native optimized build
has no compiler/license-client diagnostics, and hardening passes all 16 QQ programs. The same 65 scene results pass
offscreen under ASan/UBSan with leak detection enabled; its test program passes hardening. Five Shape lint findings
are removed without adding any; 83 runtime warnings remain, so the lint gate still fails. The following Git continuation
recovers the missing Windows coordinator byte-for-byte at its original path after idle-worker checks. Lock exclusion,
child exit propagation, all-file hooks and the complete working-tree secret scan pass. Windows product builds remain
unrun. The native coordinator and memory floor are unchanged. These are software-only checks; full product and hardware
acceptance remain open. Recovery and Git receipts are in session-03 beside the native evidence.
Receipts, including the original runtime failures and a corrected null-assignment test-fixture error:
`build/development-2026-11-14/session-02/`.

14 November Shape follow-up: private factories keep their owning context, and structural parent-state reads retain
live updates and plane locks across reparenting and detachment. The final optimized native build passes all 58 scene
results and seven selected physics results, with no failures or skips. Runtime lint removes 14 findings without adding
any; 88 warnings remain and the gate still fails. Hardening passes all 16 QQ programs. The cache's actual CMake mode is
`RelWithDebInfo`, despite `release` in its directory name. All 58 scene results also pass offscreen under ASan/UBSan with
leak detection enabled; that test program passes hardening. These are isolated software checks, not full product or
hardware acceptance. Receipts: `build/development-2026-11-14/session-01/`.

13–14 November native scene follow-up: `Game.playingScene` separates the loaded game from the viewport root. The regression
reproduces the old collision; the complete scene suite passes 57 results. Test-owned viewports have hidden window owners
and drain queued destruction. All 57 results pass offscreen under ASan/UBSan with leak detection enabled; Wayland retains
a 96-byte allocation also reproduced without a viewport, and still exits with failure. Runtime lint retains 102 warnings
after removing the property override, and Store playback still exceeds its unchanged 60-second deadline. These are
focused software results, not full product or graphics acceptance. Receipts: `build/development-2026-11-13/session-11/`.

Latest Inspector batch, 5 November: selected kind/name/fields use the same dynamic metadata lookup as the optional
editor dictionaries, preserving the public target interface, guards, writes and undo keys. Three existing real Studio
controls (five Qt cases) pass per Release/ASan build, zero failures/skips, with fresh records, source hashes, actual
sanitizer compiler commands and app/test relinks; hardening passes 16/13. Four lint findings are removed, none added:
149 console/174 structured warnings remain. All native commands still exit 255 and the wrapper exits 1.

At `f99e007`, the complete journey is refreshed after Inspector: one CTest per product build passes, zero failures/skips,
in 126.656/114.105 seconds with its 300-second limit unchanged. Fresh records, 26 sources and nine program identities
are verified, including rebuilt current apps and sanitizer flags/libraries; hardening remains 16/13. Native lint stays
at 149 console/174 structured findings with no additions/removals, and the combined wrapper exits 1. Prism is the
ordinary simulator with a recording stand-in; real Player playback runs separately. An attempted Shape context change
added two parent-view findings and was restored byte-for-byte; its runtime checks were not run.

The preceding Shape batch, 5 November: private factories use their actual owner, preserving public interfaces and 2D axis locks.
Full Scene (52 Qt cases) and six physics functions (10 cases) pass per Release/ASan build, zero failures/skips, with fresh
records/source fingerprints and hardening 16/13. Group waits use existing software physics/frame clocks; GPU waits,
tolerances and the Player's frame-rate floor remain. Earlier failures are retained. Native QQ lint removes 13 findings,
none added: 153 console/178 structured warnings remain, and native commands/the wrapper still fail. The results below
describe earlier checkpoints. The complete journey at `d111bdc` subsequently passes once per product build, zero failures/
skips, in 112.977/112.875 seconds: two Studio builds, three real Player launches, Prism Library discovery/request and
playback after temporary-project removal. Fresh records, 22 source fingerprints and nine program identities are verified.
Prism is an uninstrumented simulator using a recording stand-in; actual Player playback runs separately.

Asset/build/Store checkpoint at `39b0768`: unfiltered Release and ASan/UBSan each pass 91 asset-library,
195 game-build and three real Store-to-Player Qt cases, zero failures/skips, all 44/48/1 ordinary functions. This includes
the opt-in largest-pack measurement and real StoreEngine demonstration-pack install/import path. Fresh native records,
18 source hashes and unchanged product inputs are verified; post-run hardening passes 16 Release/13 sanitizer programs.
The StoreEngine helper is from its separately recorded earlier build, not a current compositor/hardware proof.

These are hidden WSL software runs, two CPUs, one worker, a 1.75 GiB hard cap and leak checking off. Every completed suite
stays below the documented 5,400-second software ceiling. The picked CTest command omitted the aggregate timeout flag;
this limitation is recorded, and future local checks use the corrected display clone. Release first-list/planning
observations are 246/386 ms; sanitizer observations are 807/1,003 ms. Both operations remain on the window thread;
no latency budget or speed improvement is established. QQ lint stays at 166 console/191 structured warnings, none added,
with native failures preserved. Whole-OS, Windows and native hardware readiness remain separate and unproved.

At integrated runtime `89877cf`, the complete play suite passes unfiltered in Release and ASan/UBSan: 56 Qt cases per
build, zero failures/skips, all 51 functions with 54 expanded rows plus setup/cleanup. Fresh native records/source hashes
and hardening (16/12 programs) are verified. These are hidden WSL software checks with leak checking off; physical
graphics, sound/controllers, browser playback and full OS readiness remain separate.

A later saved-game store metadata batch declares the existing `GameStore` instance API as an uncreatable QML type.
The project panel receives that concrete type; save algorithms, formats, paths, limits and permissions are unchanged.
All 49 scene cases and three Studio save/Forget cases pass per Release/sanitizer build, zero failures/skips, including
a memory-only typed-instance and direct-construction-refusal check. Fresh records and hardening 16/12 pass. The full
56-case play runs above precede this batch.

Player2D’s later bound-carrier batch passes five Qt cases per Release/sanitizer build with zero failures/skips: tiled
collision/rendering, top-down movement/carried look and headless 2D trigger replay. Hardening passes 16/12 programs.

Scene/Player/TileMap’s later bound-factory batch passes six gameplay and three Studio Qt cases per Release/sanitizer
build, zero failures/skips: gravity, pause, pickup, tiled collision and collider-debug toggling. Fresh records/exact
functions and hardening 16/12 pass.

World’s later light-probe qualification passes four Qt pixel cases per Release/sanitizer build with zero failures/skips,
covering HDR lighting/rotation/bloom and procedural lighting. Fresh records/source hashes and hardening 16/12 pass.
QQ lint still fails at 166 console messages/191 structured warnings, with two removed and none added.

Malformed scene startup now reports a problem, discards the rejected object and allows a valid file to be tried again.
A scene root must be a 3D object, matching the existing play/discard contract; complete custom 3D scene interfaces remain
supported. A measured startup failure exits 3, with exit 1 taking precedence when its report cannot be written. The first
terminal result is retained. Ordinary play stays open after its console report; capture-only runs finish after saving.
At `ad4aa3f`, all 51 Scene and 11 selected Player Qt cases pass per Release/ASan/UBSan build, zero failures/skips, including both rejected
root kinds, retry, failed/stale output, ordinary play, capture and record/replay. Fresh records/source hashes and hardening
16/12 are verified; sandbox, confinement and watchdog sources are unchanged. Final runs use two CPUs and a 1.75 GiB hard
memory cap. Earlier tighter-cap sanitizer failures and observed paging are retained; their precise cause is not established.
QQ lint remains 166 console messages/191 structured warnings, none added; its native failure propagates.

The Player’s error label now shows filenames and supplied problems as plain text, with its live binding, font,
wrapping and color preserved. The regression drives the actual Player QML through a real missing-file problem,
inert supplied markup and a later filename change. All 52 Scene and five real Player startup Qt cases pass per
Release/sanitizer build, zero failures/skips, with fresh records/source hashes and hardening 16/12. Production C++,
Game and SceneIO are unchanged from `ad4aa3f`; lint remains 166 console/191 structured warnings, none added.

`qq-player --measure <result.json>` returns exit code 1 if its report cannot be opened, fully written, flushed or closed.
Automation must check the process status and use a fresh report path for each run. The Player retains its exact-file
kernel permissions for the requested output.

Needs Qt 6.12 (MSVC 2022 64-bit kit, with Qt Quick 3D, Quick 3D Physics, Spatial Audio and Multimedia) and Visual
Studio Build Tools 2022 with the C++ workload. The browser build also needs Qt's WebAssembly kit (multithreaded), the
Emscripten SDK that kit names (5.0.5 for Qt 6.12, in `%LOCALAPPDATA%\emsdk` by default, `-Emsdk` to change it), Node.js
and a Chromium-family browser.

```powershell
pwsh products/qq/build.ps1           # configure, build, run every test; non-zero if anything fails
pwsh products/qq/build.ps1 -Run      # build, then open QQ Studio
pwsh products/qq/build.ps1 -Deploy   # build, test, then put a self-contained QQ Studio where the launcher looks
pwsh products/qq/build.ps1 -Web      # build the QQ Player for the browser, play it in one and measure it
```

The build goes to `%LOCALAPPDATA%\qq-build` (`-Build` to change it); the deployed Studio to `%LOCALAPPDATA%\qq-studio`
(`-DeployTo`), with the Qt libraries, plugins and QML modules it uses and the Microsoft C++ runtime installer: about
1,520 files and 182 MB (113 MB before P3.2 brought physics, audio and Qt Multimedia), runnable on a machine with no
Qt. The launcher's **Open in QQ Studio** starts that copy, or the one the `QQ_STUDIO` environment variable names.

The browser build goes to `%LOCALAPPDATA%\qq-wasm` (`-WebBuild`): the page, `player/qq-player.html`, with
`qtloader.js`, `qq-player.js` and `qq-player.wasm` beside it. **Whatever serves it must send
`Cross-Origin-Opener-Policy: same-origin` and `Cross-Origin-Embedder-Policy: require-corp`**: the build uses threads,
and a page gets the shared memory threads need only when it is cross-origin isolated (ADR-084).

The tests open real windows and render on the GPU, so they need a desktop session. `QQ_SAVE_FRAMES=<folder>` keeps
every frame `tst_world` judges.

Native Windows asset data now has a separate, limited check: the existing commercial Qt 6.12/MSVC kit builds
`tst_assetlibrary` and its Studio dependency in an isolated cache with warnings as errors and hardening enabled.
The 35 selected file/JSON/image functions pass 78 Qt cases, zero failures/skips, including all selected link checks.
Two pre-existing compiler warnings were corrected without changing option matching or provenance assertions.
The tests use temporary projects and Qt's test-mode storage, an offscreen/software configuration, and
`QT_D3D_ADAPTER_INDEX=0` to bypass QQ's static adapter enumeration. They do not open Studio/Player windows, load
3D scenes, use audio hardware or establish the full Windows product gate. Exact selections, source/program hashes,
fresh XML and the original failed builds are in `build/autonomous-2026-11-04/HANDOFF.md`.
The shared data subset passes 69 cases/32 functions per Linux Release/ASan/UBSan build. Three Windows-only functions
are not compiled there. The new missing-folder, cloned-timestamp and older-journal cases first failed on unchanged
Windows production code; a fourth workspace-copy check also verifies that newer records restore only to their own folder.

The workspace interface refresh at `229f930` is partial: the original Release selection reports 37 passes/1 failure,
zero skips. The real Studio accepts a private agent-pipe connection but does not reply to initialization before the
crash test can proceed; the same binaries fail in an isolated case. Six independent interface functions pass 37 Qt
cases under ASan/UBSan, zero failures/skips, covering 30 hostile-copy rows, trust/restore, two Studios and folder
replacement. The actual-program crash case is **not run under sanitizers** and the original failed selection remains
failed. Private-pipe controls and a parent-debugger trace narrow the failure to a software-render wait without proving
its cause. No production workaround or longer timeout was applied. Records and source/program/compiler hashes are
in `build/autonomous-2026-11-04/workspace-recovery-ui-verified.json`.

Gizmo's selected-object lookups now follow the same structural entity contract without changing the `QtObject`
target or its visibility/field guards. Six existing picking/translation/scaling/flat-editing/snapping/group-prefab
functions pass eight Qt cases per Release/ASan/UBSan build, zero failures/skips. Fresh native Windows and Linux lint
both move from 149 console/174 structured warnings to 144/169: exactly five old Gizmo findings removed and none
added. The remaining warning gates still fail, with the aggregate wrapper returning 1. Source/program/copied-QML
hashes, actual sanitizer flags/libraries and hardening 16/13 are verified in
`build/autonomous-2026-11-04/gizmo-entity-verified.json`; this does not clear the separate real Studio startup failure.

Player window labels read the scene name and follower style through their structural object interface, with the same
guards and fallback strings. Compatibility coverage checks live name/style edits, absent followers, the 3D hint and
release fallback, alongside the existing literal-error/startup checks and actual Player launches. Six Scene/eight
Player Qt cases pass per Release/ASan/UBSan build, zero failures/skips. Windows and Linux Player lint now pass with
zero warnings; whole QQ still fails at 142 console/167 structured warnings, two old findings removed and none added.
Fresh records and the exact scope, including the unrun full 2D frame-rate gate, are in
`build/autonomous-2026-11-04/player-labels-verified.json`.

The legacy Windows matcher now includes pre-1970 timestamps. Positive, zero, negative and missing identities have
native preservation/review checks for both journals and workspace copies. The two negative rows first failed on the
digits-only matcher; adding an optional minus sign makes all 78 native data cases pass, with zero failures/skips.
Common Linux Release/ASan/UBSan still passes 69 cases per build; the Windows-only methods are not compiled there.
The same test source was used red/green, and neither trust nor automatic recovery was loosened. No personal records
were inspected. Exact proof is `build/autonomous-2026-11-04/legacy-negative-verified.json`.

Scene's recorder property now names its documented `RunRecord` type. The regression rejects an unrelated object
without replacing the previous recorder, accepts the real run through the existing QObject interface, and clears
through an invalid QVariant. It first failed on the previous slot and passed unchanged after the type refinement.
Fifteen Qt cases across nine functions pass per Release/ASan/UBSan build, zero failures/skips, including actual Player
record/replay and its altered-run/malformed-input error exits. Game and Player public object interfaces stay unchanged.
Windows and Linux lint remove eleven existing recorder findings and add none, leaving 140 console/156 structured
warnings and aggregate exit 1. Source/program/copied-Scene hashes, actual sanitizer flags/libraries and hardening
16/13 are checked in `build/autonomous-2026-11-04/recorder-type-verified.json`; no full product or hardware pass is claimed.

The Scene store snapshot also names the existing `GameStore` returned by SceneIO, without changing the C++ interface
or object ownership. Ten Qt cases/four functions pass per Release/ASan/UBSan build, zero failures/skips: the supplied
store type, serialized runs, replay without writes, and two actual Player starts retaining saved state followed by
replay. Native Windows/Linux lint remove one structured `contents` finding and add none, leaving 140 console/155
structured warnings and aggregate exit 1. Fresh source/program/copied-QML records, actual sanitizer flags/libraries
and hardening 16/13 are verified in `build/autonomous-2026-11-04/store-snapshot-verified.json`.

Group/Prefab state reads keep their mixed parent contract through private structural readers. New compatibility
rows pass unchanged before and after the cleanup: strict booleans, live values, nested propagation, absent fields,
null parents and reparenting. Fourteen Qt cases/seven functions pass per Release/ASan/UBSan build, zero failures/skips,
including group/prefab serialization, kinematics, gameplay and Studio editing. Seven native Windows node/data cases
also pass; the initial pre-test flag-guard failure is retained, and accepting the observed `-WX` spelling changes no
build setting. Native Windows/Linux lint remove eighteen old findings and add none, leaving 122 console/137 structured
warnings and aggregate exit 1. Exact scope, source/copied-QML/program/fresh-object records, actual sanitizer flags and
hardening 16/13 Linux/three native product headers are in `build/autonomous-2026-11-04/group-state-verified.json`.

Agent body rows now qualify the startup font through the existing QtQuick `Application` singleton. Five inert panel
cases pass per Release/ASan/UBSan/native Windows build, zero failures/skips, retaining startup body/mono choices,
literal text and replacement rows. Additive startup coverage passes unchanged before/after; no old assertion is removed.
An exploratory notification expectation failed in both forms; Qt's property is constant, so no live-update feature is
claimed. That experiment was retained and the original tests restored before the startup-only additions. Native/Linux
lint remove one old font finding and add none, leaving 121 console/136 structured warnings and aggregate exit 1.
Source/copied-QML/program/object records, actual compiler flags and hardening 16/13 Linux/five native product headers
are verified in `build/autonomous-2026-11-04/application-font-startup-verified.json`. No provider or keychain flow runs.

At the authorised push checkpoint, recovery data is refreshed at 69 cases/32 functions per Release/ASan/UBSan build,
zero failures/skips. The current real Studio crash case still fails before crash/restore at the initialization helper
(2 passes/1 failure, CTest/wrapper 8); its sanitizer counterpart remains unrun. Passing data tests do not clear that
interface failure. Exact fresh records use `build/autonomous-2026-11-04/authorized-push-*`.

## The Player, measured (10 October 2026)

On the owner's laptop (RTX 4060 Laptop GPU), on mains power, playing the bundled playground at 1280 × 720, locked down
(ADR-087):

| | Native | In a browser (Chrome, headless) |
| --- | --- | --- |
| First frame proper (the scene playing, every model loaded) | about 1.7 s from the process starting (five runs: 2,023, 1,709, 1,729, 1,791 and 1,711 ms) | 5.5 to 5.7 s from the page starting to load, cold, served from this machine (two runs: 5,683 and 5,537 ms; `main()` begins at about 0.3 to 0.4 s) |
| Frames a second over the next three seconds | 265 to 346 | 313 |
| Size | the executable, 0.55 MB, with Qt's libraries beside it | 44.1 MB as served (the `.wasm` 43.8 MB); 16.0 MB gzip, 11.7 MB brotli |

The lockdown costs nothing measurable: its parts switched off, one at a time or all together, give the same first
frame. The 1.4 s first measured (P3.2) was for a game missing its one model, which had failed to load; the Player now
names any model that fails (`unloaded` in its measurement), and its checks fail on one.

The browser's first frame includes compiling 43.8 MB of WebAssembly; across a network it is longer by the download.
Chrome must be given the faster GPU (`measure-web.mjs` asks for it): on the laptop's integrated Intel GPU the same build
drew about 12 frames a second.

## The tests, and how they were proven

The phase-by-phase case counts below preserve the product's earlier progression. For QOR OS's verified run scope,
source revisions, failures and skips, use [OWNER.md](../../OWNER.md) and [CI.md](../../docs/CI.md). A historical count
or planted-fault record is not a fresh passing suite.

- **`tst_world`** (3 cases): a glTF model loads at runtime and is drawn lit in its own colour; bloom lights pixels past
  the glowing band; an unreadable model is an error, not a crash. Proven to fail by two planted faults (bloom ignored;
  a prop that never loads). **Since P3.14, 9 cases** (fixtures by `make_looks.py`):
  - a checker on a wall 4 units by 2, a copy a unit: eight squares across and four up, square, not stretched, with
    sharp edges when `crisp` and blended ones when not, and half as many at twice the size;
  - a white box with a mid-grey picture drawn as a grey box is, face by face, so a picture is lit as a plain shape is
    and its colours converted to linear light alike;
  - a normal map leaning toward +x turns a face to a sun on its right and away from one on its left, where a flat
    face is lit alike from both;
  - a spot light's pool under it and darkness two units away, the pool moving two units when the light is tipped;
  - an HDR sky half red and half blue: the half ahead seen, a white ball lit by the half behind the eye, and both
    swapped when the sky is turned half round; and a sky dimmer than white left as it is by bloom;
  - a ground 4 units across drawn as wide as its slab, with its picture, and the sky beyond its edge.

  Eleven planted faults were each caught: pictures laid by the mesh rather than the shape's size, crisp ignored, a
  picture's colours not made linear, the relief never reaching the light, the relief turned by Qt's normal matrix, a
  spot light's cone ignored, the sky never turned, the ground drawn a hundred times its size, bloom washing a sky image
  out, a confined game reading pictures from outside its package (by `tst_lockdown`), and saving bringing in only the
  entities' files (by `tst_studio`).
- **`tst_scene`** (14 cases): numbers are written one way only; a scene file reads and writes back byte for byte; one
  changed value is one changed line; a saved scene keeps its model relative and loads again; a scene saved elsewhere
  still points at its model (including across drives, where no relative path exists); adopting a model brings its
  files and never overwrites; sounds are adopted into `assets/` and logic into `logic/`; playing is a copy, and the
  scene is not touched; what is not a scene is refused with a reason; entities are added and removed and the file
  follows; a project lists its scenes; the bundled sample can be adopted; a 2D scene imports as a canonical one; a
  file of another format is not imported. **Since P3.10, 19 cases (24 checks):** a Tiled map is read with its external
  tileset, its mirrored tile and its solids merged, and another layer by name; a map of zlib-compressed Base64 with its
  tileset inside reads the same; six maps that cannot be read are refused, each with its reason; solid tiles become the
  fewest rectangles; and a map adopted into a project brings its tileset and image, while one whose tileset is outside
  its folder is refused with nothing copied. **Since P3.11, 22 cases (27 checks):** the seeded numbers pinned against an independent
  implementation, a box stopped by what stands in its way (touching is not overlapping), and a run kept and read back.
  **Since P3.13, 23 cases (28 checks):** a sound file's length read from its header, and read again when the file is
  replaced; the Mixer's levels held to 0 to 1, multiplied by the master's, and reset; a browser voice's pan held to all
  left and all right. **Since P3.15, 26 cases (31 checks):** groups nested two deep written inside each other and read
  back the same bytes, listed each after its group, added into, moved out and in keeping their place in the world,
  refused for a player, never into themselves, and removed with what is in them; QQ's kinematics measuring a wall
  through a moved group turned a quarter about the depth axis, in doubles; and a prefab made of its file where it
  stands, never written into the scene, its part found by name, refused when its file's root is not a Group, a group
  written as one at the origin and never over a file, and a prefab brought into a project's `prefabs/`. **Since its
  third part, 27 cases (32 checks):** a project's assets listed by kind, in folders too, a model's buffer not among
  them; logic built where its file is, an error by its line, an unknown type where it is written; and a text file read,
  or said not to be.
- **`tst_studio`** (10 cases, the real window): a new Studio opens a lit scene in its project; a click on the model
  selects it and a click on the sky selects nothing; dragging the X handle moves along X only; dragging the centre
  handle scales evenly and leaves the position alone; saving writes a canonical scene and carries the model into the
  project, and one edit saved again is one changed line; a saved scene opens again as it was; entities are added,
  named uniquely and deleted; a 2D scene is imported with what cannot come yet said; **Play then Stop**: Play from its
  button, a crate falls and rests while the scene's crate stays put, W walks the player and the eye follows, a drag
  turns the player and tilts the eye, Esc leaves the scene, its unsaved state and the selection exactly as they were,
  and Play starts again from the same place; a new behaviour is written into the project and never over a file.
  **Since P3.10, 11 cases:** a 2D scene opens framed as it plays, its handles see what the world sees, its X handle moves
  along X on the plane, a drag on nothing pans with the pointer, and the wheel shows fewer units, larger. **Since
  P3.13:** Stop puts back the volume levels a play set. **Since P3.14:** saving brings a shape's picture and the scene's
  own sky in from outside the project, and the file names both relative to itself. **Since P3.15, 14 cases:** a handle
  drag, an entity added and three quick colour changes undone one step each with Ctrl+Z, back to the saved scene and
  no further, and done again with Ctrl+Y, Ctrl+Shift+Z and the Redo button, the selection kept by name and an edit after
  an undo leaving nothing to redo; a duplicate beside its original, every field the same and apart from it, a copy of
  a copy the third, a model's file carried, and undone; and with snapping, a handle landing on the grid, turning by 15
  degrees and scaling by tenths. **Since its second part, 15 cases:** a selection grouped with Ctrl+G where it stood,
  moved with its group, out and back in by the inspector's choice, ungrouped with Ctrl+Shift+G and that undone; a group
  kept as a prefab, placed where it stood, its part not in the list and picked as the prefab, saved without its parts,
  and placed again; and unpacked, changed and kept back over its own file, the other prefab of it made again with the
  change. **Since its third part, 18 cases:** the project's files listed by kind, a model added by a click in the list,
  a picture put on the selection, a sky made the scene's, a sound added, and the filter; a Behaviour's logic opened in
  the editor, a brace taken away said to be wrong by line, mended and saved with Ctrl+S (the file written, the scene
  not); and F3's frames, draw calls and bodies, every body drawn as its collider in play, and gone with F3 again.

  **Since QOR OS:** literal logic-problem descriptions, inspector source filenames and group-selector popup names (with
  actual delegate activation/reparenting), plus edits through loaded field controls and entity-kind switches; undo, groups/prefabs and 2D editing remain covered.

  Eight planted faults were each caught: quick edits of one field never merged, an edit after an undo keeping what
  could be redone, undo forgetting what was last saved, undo losing the selection, saving made a step to undo, a copy
  of a copy named after the copy, turning not snapped, and redo made ambiguous by naming a key the platform's redo
  already has.
- **`tst_play`** (11 cases, real frames): a dynamic shape falls and comes to rest on the ground; a static one holds
  what lands on it and one with no body lets it through; a cylinder and a cone collide as their shapes; a fall takes
  the time the scene's gravity says; the player walks, runs, turns and jumps at the keyboard, with the camera behind
  and above; a gamepad moves and turns it past its dead zone and is let go when unplugged (the pad's state is given,
  as the agent will give it: no pad is attached in the run); typing into a field is not playing; an emitter fills the
  air with its colour, counted in the frame; a sound is set up where it is, with its reach, only in play, and no sound
  device is opened while editing (what is heard is not judged); logic runs, is rebuilt when its file is saved without
  the world stopping, keeps running through a save that does not build and says why, and does not run while editing.
  **Since 19 October 2026, 18 cases (P3.7, ADR-105):**
  - logic hears its target's body meet another entity, once as they meet;
  - a Trigger hears a falling body enter and leave, and lets it through;
  - a Trigger hears the player walk in;
    its private volume and box/sphere collider factories bind to that Trigger, including when its group moves;
  - a ray meets the first entity in its way, passing through triggers and starting inside its own target;
  - logic makes three bodies, refuses an unknown kind, removes one with its body, and keeps timers on its own clock,
    `every` and `after` and a cancel;
  - an impulse moves a dynamic body, a kinematic one is not pushed, and a kinematic lift rises with its rider;
  - the Orb Run sample is canonical and plays: an orb gathered, removed with its glow and replaced, and the boulder
    rolled.

  Five planted faults were each caught: the contact relay cut, a removed body left behind, timers never run, the
  player unheard by triggers, and rays with no view.
  **Since 19 October 2026, evening, 23 cases (P3.8):**
  - a screen draws what logic sets, over the world, where its anchor and insets put it;
  - screens show and hide, and a pause holds the world, logic's clock and its timers while screens still answer;
  - a button answers only while its screen shows and the world plays, and never while editing;
  - with reduced motion a screen appears at once;
  - an image from the project is drawn crisp;
  - Orb Run opens on its title, Play starts it, the score is on its HUD, and Pause and Resume hold and release it.

  Five planted faults were each caught: a button that answers on any screen, physics or logic that ignores a pause,
  reduced motion ignored, and a part drawn whatever its screen.
  **Since 19 October 2026, night, 26 cases (P3.9):**
  - an action is held from its key, a mouse button, a pad button and a finger on a button;
  - jump and run are actions with their old bindings;
  - a player rebinds a key, a pad button and the built-in jump by pressing what they want, and that press does
    nothing else;
  - the pointer picks a shape, the sky and the ground;
  - Orb Run pauses and goes on with P.

  Five planted faults were each caught: a press never announced, the naming press also firing, a finger holding
  nothing, picking ignoring the pointer, and every event read twice.
  **Since 19 October 2026, late night, 32 cases (P3.10):**
  - a crisp sprite is drawn pixel for pixel at a whole zoom: every art pixel a square of whole device pixels, sharp
    edges, the painted colours, the sheet's second row the right way up, and flipped;
  - a sheet plays its frames, from the first of them, and holds them while the world is paused;
  - a Tiled level is drawn tile for tile with its mirrored tile and only its first layer, and the hero stands on its
    floor to within half an art pixel, runs to its wall, jumps a unit and a half and turns;
  - a top-down player moves across the plane with no gravity and carries its look;
  - a dynamic body in a 2D scene, pushed into the depth, stays on the plane and turns only about its depth axis;
  - Ember Climb is canonical and plays: its title, an ember gathered, a pit and back to the start, pause, the forge lit
    and Again; and its eye stops at the level's edges.

  Seven planted faults were each caught: a sheet read upside down, pixel art scaled to fit, Tiled's mirror flag
  ignored, PhysX's skin not allowed for, a body free to drift in depth, a tileset read from outside the package (by
  `tst_lockdown`), and the eye shown past the level's edges.
  **Since P3.11, 36 cases:**
  - two seconds of play drawn at 30, 60, 144 and uneven frame rates are always 120 steps, with the timer firing eight
    times; a long frame is at most eight steps, and a paused world ticks without steps;
  - the same seed gives the same dice, another seed others, and seed 0 a new seed each play;
  - a run played with the keyboard on a Tiled level is kept and replayed with no window to the same outcome, the
    hero's place to the last bit, and not with another seed or with hands that never moved;
  - Ember Climb's first part is kept and replayed;
  - a trigger hears a 2D player in a replay, with no physics world made.

  Seven planted faults were each caught: steps that follow frames, the seed ignored, clicks or hands not kept, a
  physics world in a replay, triggers deaf to 2D players, and touching counted as overlapping (by `tst_scene`).
  **Since P3.12, 40 cases:**
  - a rig's clips step by step: its joint at 45 and 90 degrees, its morph weight at a half and one, and the pixels
    drawn; looped, paused, sped up, and put back at rest when the clip changes;
  - a joint read by logic in a replay with no window, where the clip put it on that step;
  - tweens of a vector, a colour and a number at their exact steps, a take-over and a cancel;
  - a machine's enters, steps, exits and events;
  - Orb Run's lanterns flaring and its orbs growing in.

  Six planted faults were each caught: a clip's joints not put back, a clip running on while paused, a tween's start
  following its field, a tween in the wrong steps, a machine never leaving a state, and steps that do not pose the
  model.
  **Since P3.13, 44 cases:**
  - a one-shot placed where logic said and one heard everywhere, at their groups' levels as they change, both put
    away once played through;
  - music fading from one track to the next, the first let go when it has, and both when play stops;
  - as a browser hears it, a sound on the eye's right panned right and one on its left panned left, quieter with
    distance, one ahead in the middle, and a one-shot the same;
  - no sound in a replay;
  - Orb Run's music changing when play starts, its chime and rumble, and its music switch.

  **Since P3.14,** Orb Run's case also checks its looks: stone walls with their relief, a slate floor, two spot lights
  and a dusk sky, each from its own `assets/`.

  **Since P3.15, 46 cases:** a group carrying a kinematic deck, the crate on it and a trigger, raised by logic a
  little each frame; and a prefab of a trigger and its logic placed twice and once more by logic, each counting only
  what passes through its own trigger, its parts not the scene's own, and the third removed whole. Orb Run's orbs are
  a prefab placed by logic and its walls a group.

  Ten planted faults were each caught, by these and by `tst_scene` and `tst_studio`: the entity list not walking into
  groups, a prefab's parts written into the scene, a group not passing on that the scene plays, a body made inside its
  group, kinematics measuring only the entity's own place, a prefab's logic finding its target in the scene first,
  moving between groups not keeping the world place, a click on a prefab's part picking the part, logic unable to
  remove what is in a group or a prefab, and an unpacked prefab kept as a new file.

  Eight planted faults were each caught: one-shots deaf to their group's level, a one-shot never put away, music
  never letting its old track go, music deaf to its group's level, logic sounding in a replay, a browser panning to the
  wrong side, the Studio's Stop keeping a play's levels (by `tst_studio`), and a replaced file keeping its old length
  (by `tst_scene`).
- **`tst_lockdown`** (8 cases, 13 checks with the imports' five; ADR-087): a game that keeps to its own files (logic
  importing Quick 3D Physics, a model from its `assets/`) plays with nothing refused; imports of `QtQuick.LocalStorage`,
  `QtCore`, `QtQuick.Dialogs` and `Qt.labs.settings` refused with the module named (and `QtMultimedia` an expected
  failure, the known limit); `SceneIO`'s writing, saving, copying, loading and listing outside the package refused and
  nothing on disk; a `Loader` and an `Image` aimed outside the package, by whole URL and by a relative path climbing
  out, both errors; a network request to a listener on this computer never arriving; `Qt.openUrlExternally` refused for
  a file and a web address, with nothing handed to the system; and (P3.10) a picture measured, a map read, or a
  map's tileset read from outside the package, each refused, while a game's own map and sheet play; and (P3.14) a
  shape's picture and relief, a sky, a model and a sound named from outside the package are nothing, while a picture
  inside comes as named; and (P3.15) a prefab from outside is made of nothing. **Proven to fail** by four planted faults, each caught by
  its own check: no URL interceptor; no network policy; `SceneIO` left unconfined; every module allowed.
- **`tst_publish`** (9 cases; P3.4, ADR-088), against a stand-in launcher speaking the QOR host protocol: the hello
  carries the token and the Studio's id; Publish asks for the commit and then the publish, with the project, and
  reports the receipt; a decline is said and nothing more is asked; a refused handshake leaves publishing unavailable
  with the reason; frames a byte at a time and two in one write read alike; a frame over 1 MiB is not read; the
  launcher closing the channel is told (and the Studio exits); and the real Studio window, not started by the launcher,
  cannot publish, then, linked, saves an unsaved scene first and says what was published, or that it was declined,
  and a refusal too long for the toolbar is shown whole when the notice is hovered;
  and, linked but on a folder that is not versioned, Publish can still be pressed, says what is missing and asks the
  launcher nothing. **Proven to fail** by four planted faults: publishing without the commit (three cases);
  publishing without saving first; Publish greyed out on a folder that is not versioned (the owner's first run, 10
  October 2026); the whole notice not shown on hover (15 October 2026). The launcher's side, the channel's proofs and a run against a development node, is in the launcher's tests.
- **`tst_player`** (the real executable): the playground plays and its start is measured; its first frame is
  the game (hundreds of colours, not an empty window); a scene file from disk plays too, with every model loaded in it
  and in the playground; a 2D game (Ember Climb) plays, measured and captured; a run is kept with `--record`,
  `--seed` and `--ticks`, replayed with `--replay` to the same outcome (exit 0), not with another seed (exit 4), and
  refused when it is not a run or the game's logic does not build (exit 3); a scene that is not there is said, not
  hidden. Later cases cover incomplete roots, retained report failures and ordinary play staying open after its report. **Proven to fail** (10 October 2026) by putting
  back `Prop`'s old loader line: both playing cases fail with `Orb: IO Error: File not found`.
- **Proven to fail** (10 October 2026) by five planted faults, each caught by its own check: no gamepad dead zone (the
  gamepad case); a logic file not watched (the rebuild); Play on the scene itself, not a copy (`tst_scene` and
  `tst_studio`); a sound active while editing (the sound case); a player that ignores the mouse (the drag in
  `tst_studio`, "turned 0").
- **In a browser:** `build.ps1 -Web` (`player/measure-web.mjs`) serves the build cross-origin isolated, plays it in
  headless Chrome, reads the Player's measurement, captures the first frame and fails unless it is the game, with every
  model loaded and no error on the page or in any of its workers; and (P3.13) unless the playground's sound is on the
  page's Web Audio, decoded and through a stereo panner.
- **`tst_agent`** (15 cases, one of them run only when asked): the protocol alone (a version agreed, or the server's newest; instructions given; not
  JSON, a batch, an unknown method or tool each answered with its JSON-RPC error; a notification not answered; a
  tool's failure a result with its id); in the real Studio, through the protocol: every tool listed with a name and
  schema clients accept; the scene read as its canonical text and fields, and every kind with its types, defaults,
  ranges and choices; entities added (shown in the Studio, unsaved, the file untouched), changed (one value, one
  line), refused with nothing of the call kept, named uniquely and removed; logic written, attached once, and code
  that does not build written but reported; play, a crate fallen to the ground as `wait` reports, the agent's
  controls walking the player, a captured frame that is the world, Stop restoring the scene and letting go of the
  controls, edits refused while playing; logic that throws every frame in the log once, with a count; and the real
  programs: `qq-mcp` with no Studio says so, finds one opened later, relays requests and results, and a second Studio
  on the same pipe stays out and says why. The design loop, against a stand-in for Anthropic's API on this computer: a
  description built, played, looked at and left for the creator (every request carrying the key in its header and
  nowhere else, the API version, the fallback opt-in, the model, its effort and thinking, and the fourteen tools; the
  conversation append-only; a turn's three results sent back together in order, the frame as a PNG; the crate built,
  unsaved, play stopped, the key in no log; the run undone back to the scene before it); a busy provider waited out (a
  429 and a 529, as `retry-after` says), a refused key said and not retried, a refusal ending the run, an answer cut
  off not run, a stop dropping the request in flight and anything after it; what a fallback replaced neither run nor
  sent back; the Agent panel asking for a key, never showing it again, then for a description; each run's token use
  counted from the provider's answers; and a key in the environment (`ANTHROPIC_API_KEY`, as a server might set) never
  used: with no key of the creator's own nothing is sent, the panel saying first who pays. **Proven to fail** by nine
  planted faults, each caught by its own check: a refused change half kept; edits allowed in play; a bridge that
  never tries the Studio again; a log that keeps every repeat; tool calls a fallback replaced run anyway; a busy
  provider not waited out; a cut-off answer's tool call run; the key put in the request's body; a key taken from the
  environment when the creator has given none. With
  `QQ_LIVE_ENDPOINT=1`, one more case reaches Anthropic's real endpoint over TLS with a key that is not one and is
  refused (10 October 2026: refused, as it should be).
- **With an independent client** (10 October 2026, once): the official MCP Python SDK (`mcp` 2.3.0) started `qq-mcp`,
  agreed protocol 2025-06-18, listed the fourteen tools, built a crate, an emitter and spinning logic, played, captured
  a frame of the result, stopped, and was refused a mass out of range.
- In the launcher: `check-qq-view.mjs` checks that **Open in QQ Studio** asks the host to start the Studio on the open
  project and nothing more; the host's `qq::` tests check where the Studio is found, that the project is one argument,
  and that a folder that is not a project is refused; `qq_studio_starts_on_a_project` (ignored by default: it opens a
  window) starts the deployed Studio on a real repository and sees it in the process list.

Learned on the way, and recorded in `HANDOFF.md` §5: Qt Quick 3D's `Material` type shadows the Material style's attached
`Material.*` when it is imported after it; Qt 6 keeps a URL property as written, so relative model URLs are resolved
against the entity's own file; across drives there is no relative path, and a bare `X:/...` is read back as a URL with
scheme `x`; a lambda counting `frameSwapped` must not take the window as its context while capturing a local (the
signal is queued from the render thread, and a late one writes into a stack frame that has gone); Qt for WebAssembly's
single-threaded kit has no Qt Multimedia, and in the threaded one Qt's spatial audio makes its sound on a worker, where
the web has no audio, so in a browser sound is QQ's own, on the page's Web Audio (`WebVoice`).

## Known limits

- **A project made elsewhere (QOR OS's QOR-0018).** Opening a scene or a logic file runs it, with the person's own reach,
  so the Studio asks once before it does and remembers (`qor-os/own-projects.json` in the data folder). The Studio's logic
  reads only the project, reaches no network, and opens nothing outside; a `qmldir` in a project brings in nothing. What it
  can still do, once trusted, is what a person's own code can do.
- **What an agent writes** runs only once the creator allows it in the Agent panel.
- **The Player's walls (QOR-0018).** QQ's own checks (`runtime/confinement.h`) everywhere, and on Linux the kernel's
  (`player/sandbox.h`): Landlock for files and programs, seccomp for the network, by any route to a socket
  (`tests/tst_playerwall.cpp`). Natively on Windows, Qt Quick's `Canvas`
  and `grabToImage` and Qt Multimedia's types get past the first, which is why a stranger's game plays only in a browser.
- **2D, as built in P3.10:** a Tiled map is read one tile layer per TileMap, from its first tileset, which must be one
  image (a collection of images is refused, with the reason); object layers, tile animations, one-way platforms and
  slopes are not read yet. A Player2D is stopped by solid tile maps, static and kinematic shapes and sprites, and the
  ground; it passes through dynamic bodies (pushing them aside in play) and does not ride moving platforms yet.
- **What a replay reaches (P3.11).** A replay runs logic and QQ's own kinematics, never PhysX, so these are not part
  of one, and a rewarded outcome must not depend on them:
  - the 3D Player, which is still PhysX's character controller (QQ's own 3D kinematics are later work);
  - contacts and trigger entries between PhysX bodies;
  - `pointing()` and `ray()`, which ask the drawn view.

  Logic's arithmetic is the same on every machine, but `Math.sin` and its kin come from each system's library and may
  differ in their last bits. An outcome that must replay elsewhere keeps to plain arithmetic, `random()` and
  QQ's own kinematics.

- **Sound, as built in P3.13:** natively Qt's spatial audio places a sound around the listener; in a browser it is
  panned between left and right only, with its volume following the distance from the eye (full within a metre,
  halving with each doubling, none beyond `reach`). A browser decodes each file whole before it plays, so a long track
  costs its decoded size in memory. Volume levels last while the game runs; keeping a player's settings between plays
  comes with saving (P3.16). A one-shot's length is read only from a WAV file's header, so another format is put away
  after ten seconds, whatever its length.
- **Groups and prefabs, as built in P3.15:** a static body in a group is made where its shape stands when play
  starts, and stays there if the group moves: a platform that moves is kinematic, and moved a little each step or frame
  (a kinematic body moved in jumps pushes what rests on it violently, as PhysX does). A group's turn about anything but
  the depth axis is not part of QQ's kinematics. A prefab's parts are found by its `part(name)`; logic's `entity(name)`
  looks at the scene's own first. A prefab brought in from another project comes alone, without the files it names. A
  prefab is changed by unpacking it in a scene and keeping it back; the Studio does not open a prefab's file on its
  own.
- **The Studio's tools, as built in P3.15:** the logic editor builds what is written without running it, so an
  error that shows only in play (a name logic reaches that is not there) is said in play, as before; it has no
  completion or search yet. The statistics' frames a second count frames drawn, and the Studio draws only when
  something changes, so an idle Studio shows few. A picture from the list goes on the selection only if it takes one
  (a shape or the ground), or becomes a sprite in a 2D scene.
- **Undo, as built in P3.15:** it covers the scene that is open, back to when it was opened; opening another starts
  afresh. A logic file's text and the files saving brought into the project are not taken back with it.
- **Materials, as built in P3.14:** a shape's picture is projected along its three axes, so on a sphere, cylinder or
  cone it blends where two projections meet, and cannot be wrapped round it as a glTF model's texture is (a Prop is for
  that). A shape takes one picture and one relief; roughness and metal come from its fields, not from pictures. A sky is
  a Radiance `.hdr`, run-length encoded as tools write them: Qt misreads one written uncompressed, which shows as a dark
  seam where the image's edges meet. A 2D scene keeps its backdrop of colours, not its sky image. Under a sky image,
  bloom glows only from what is brighter than white, since bleeding the whole frame would wash a bright sky out. A sky
  image's lighting and a pictured shape's shaders are prepared before the first frame: in a browser the playground's
  first frame came 1.5 s later with them (7.2 s, from 5.7 s).
- **Logic is code, and the Player locks it down** (ADR-087). In the Studio a creator's logic has the engine's full
  reach, as it should. In the QQ Player, natively and in a browser, every game is confined (`runtime/confinement.h`):
  it loads QML and files only from its own package and Qt's and QQ's module folders, imports only an allowed list of
  modules (no storage, settings, dialogs, sockets or devices), makes no network request, writes and copies no file
  (`SceneIO::confineTo`), and opens nothing outside QQ. It is a second wall: a stranger's game plays only in the
  browser (P3.5), whose sandbox is the boundary. **Two limits, both shown by the checks:** Qt refuses URLs to the
  operating system only scheme by scheme, so a scheme off the list still reaches it; and Qt Multimedia (its camera and
  microphone types among them) cannot be refused natively, because Qt's spatial audio library registers it when it
  loads. Natively only a person's own games play; in a browser the browser asks before any camera or microphone.
- **Gamepads come from QOR OS's controller service, QOR.Input** (`input/gamepads` in QOR OS): XInput and raw HID on
  Windows (so DualShock 4 and DualSense too), evdev on Linux, and none in a browser yet. A pad is heard only by the
  application in front. The checks give the pad's state, or a virtual controller's, rather than reading a real one.
- **One Studio at a time offers its tools.** A second Studio on the same pipe says so and stays out; `QQ_MCP_PIPE`
  gives one another name (for `qq-mcp` too).
- **An agent works with the creator's trust.** The pipe is open only to the user running the Studio, and the agent can
  write logic, which is code; it cannot save the scene or commit. The creator reviews what it did (the scene shows as
  unsaved, each change is said in the notice line) and saves and commits it.

## The Agent panel

**Agent**, in the Studio's header, opens it. Type an Anthropic API key once (it goes into Windows Credential Manager for
your account, or the desktop's keyring on Linux, and is not shown again), describe the game, and press **Design and build**. The panel shows what the agent
says, each tool it uses and each frame it captures; **Stop** ends the run at once. When it is done, look the scene over:
save and commit it as any other, or press **Undo the run** to put the scene back as it was (logic files it wrote stay in
`logic/`).

**You pay for what it uses, and Demiurge pays nothing** (ADR-086). The agent runs on your own Anthropic account: Anthropic
bills that account for each run (Claude Opus 5.5: $4 per million input tokens, $20 per million output tokens, cached
input $0.20; every captured frame is an image it reads). Demiurge does not pay for it, does not charge for it and does
not see the bill. Without your key the agent is simply off: none is built in, and none is taken from the environment.
After a run the panel shows the tokens it used, as Anthropic counted them.

## Connect an agent

Open QQ Studio (from the launcher's QQ screen, or `qq-studio`), then point any MCP client at `qq-mcp.exe`, which
`build.ps1 -Deploy` puts beside the Studio in `%LOCALAPPDATA%\qq-studio`. In Claude Code:

```powershell
claude mcp add qq -- "$env:LOCALAPPDATA\qq-studio\qq-mcp.exe"
```

In Claude Desktop's `claude_desktop_config.json`, with the path written out:

```json
{ "mcpServers": { "qq": { "command": "C:\\Users\\<you>\\AppData\\Local\\qq-studio\\qq-mcp.exe" } } }
```

The Studio says in its notice line when an agent connects, and each change it makes. The client's model runs on the
client's own account, as everything generative does here (ADR-086).

## Local checks and paused CI

QOR OS's Qt jobs and self-hosted runners remain paused pending actual isolation proof; see
[CI.md](../../docs/CI.md#what-ci-trusts). Local results state their source revision, commands, environment,
failures and skips. They do not establish native graphics, a complete operating-system build or release readiness.
The first VM Create/Prove steps need the owner's separate go-ahead; a guest Qt kit also needs the documented
licensing prerequisite. An online runner or a skipped job is no passing build.

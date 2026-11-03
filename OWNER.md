# Where QOR OS stands

For Andrew, 3 November 2026. One page, rewritten every session (`AGENTS.md` §2). Checkpoint, mid-session.

## What QOR OS is

The operating system for players, game developers and game designers: the Prism shell (*the compositor and desktop in
one*) and Lumen design, with QQ, ARQADE and QOR Media, on Linux. Code: `QOR-MATRIX/qor-os`, private, at `X:\Repoz\QOR-OS`.
The work is on the branch `integration/owner-brief`; `main` has your merges of pull requests #2 and #3.

## Shown working, and where

Everything here ran hidden, under WSL (*Linux inside Windows*), drawn in software. **Not yet:** native Linux, a graphics card,
Windows.

1. **The first journey.** Open a QQ project, bring in a model, edit, save, build a game, and play that copy from Prism's
   Library; two projects kept apart; a restart; unsaved work kept through a crash. Reviewed and rechecked; at `72945fe`.
2. **The second journey, the two stores, in a demonstration build** (*one made with a throwaway key, never a release*):
   - Ember Climb found in the Player Store, installed, and played from the Library by the real QQ Player, checked whole first.
   - Creator features turned on in Settings, the art pack installed from the Creator Store, then copied into a QQ project by
     its new asset library with `assets.lock.json` (*a record in the project of what came from where, and under which
     licence*), updated without touching what you changed, reopened, and named in the built game. A broken lock refuses a build,
     as you chose.
   - The Store's engine and pages reviewed and fixed. The asset library's review found two ways crash recovery can lose files,
     a pack's name shown as rich text, and Update skipping the trust check: being fixed.
3. **Prism lets a game capture the mouse** (*two of the three interfaces gamescope needs*), after a review; real hardware waits.
4. **CI** (*the automatic build machines, both this laptop*): both runners offline, no isolation setting made. You merged pull
   request #3 on 3 November as `4c620c2` (its tree is exactly the reviewed `a31154c`). On it, Lint and Security passed on GitHub's
   machines; Build failed only at Release readiness, as it should: both kit jobs were skipped as not isolated, so every required
   step reads "not run". Nothing was built, and nothing failed to build. A replacement VM (*a separate machine inside this one*) is
   written, reviewed twice, and has never run.
5. **The suites in software:** QQ passes all but the Player's floor of 20 frames a second (5.6 to 6.3 here).

## Needs you

1. **Run the VM's first Create and Prove** (`docs/CI.md`, "The owner's first run"; the script is unchanged since `c9513c6`).
   Tell me first: it needs about 7 GB free, so I hold the build lock and the workers' builds wait.
2. **Readiness is a signal, not a lock:** a pull request can edit the workflow that judges it, and the Free plan has no branch
   protection.
3. **Decide the floor and the eased software checks** (`docs/CI.md`, "What Linux runs"). I recommend letting them stand in
   software only, with a graphics-card run required before a release, and holding the drawing suites to that too.
4. **A Windows test run** of `tools/dev/products.ps1` while you are away from the screen: the only graphics-card evidence here.
5. **Send the Qt letter** (`docs/release/QT.md`), question 15 on CI. No Qt kit goes on the VM before the answer.
6. **Move the Store's signing keys and their backup off this PC**, and **limit the Vercel and Render GitHub Apps.**
7. **The Linux machine:** a USB-C SSD of 500 GB or more and a USB stick of 8 GB or more, then `docs/BUILDING.md`.
8. **Later:** your accepted Unreal licence version, a Fab export and your Claude data setting, the Unity question, Bellwick's
   name, and whether a catalogue should carry a licence's text so reading one doesn't download its whole package.

## Next

The asset library's review fixes (W10). Done today besides: a pack's provenance kept by the Store and recorded in a project's lock,
and Prism's test-only options (`--script`, `--unlocked` and more) gone from a build without the simulator, as an image is; they had
let a device's Prism start past its lock screen. After that, milestone 4's groundwork that needs no graphics card.

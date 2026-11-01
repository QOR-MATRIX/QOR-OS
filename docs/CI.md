# Continuous integration

Workflows live in [`.github/workflows/`](../.github/workflows/).

| Workflow | Runs on | Needs Qt | Checks |
|----------|---------|----------|--------|
| **Lint** (`lint.yml`) | `ubuntu-24.04` | No | Generated tokens are current, token generator tests, the Store's signed catalogue, SPDX headers, no secret in the tracked files, the rules of QOR OS's own ARQADE games in Node (`products/arqade/tests/*.test.mjs`, [QOR-0017](decisions/QOR-0017-arqades-new-games.md)), clang-format 20.1.8, JSON validity. The products keep their own form, so the format checks skip them; the header check skips only their files from demiurge-chain (`LICENSES/from-demiurge-chain.txt`, [UPSTREAM.md](UPSTREAM.md)) |
| **Build** (`build.yml`) | Windows and Linux, self-hosted (or hosted with the online installer) | Yes (commercial) | Configure (`ci-windows`/`ci-linux`, warnings as errors, the QOR core required), build, every program's exploit mitigations read back (`tools/ci/check_hardening.py`), `all_qmllint`, `ctest -LE gpu` (plus `-L gpu` when enabled), and on Linux the same suites again under AddressSanitizer and UBSan (the `linux-asan` preset). On each where its variable says so, the carried products (QQ, ARQADE, QOR Media) when they changed. Tags and manual runs also upload an installable Windows artifact (kept 7 days) |
| **Security** (`security.yml`) | `ubuntu-24.04`, never the workstation | No | `gitleaks` over the whole history, redacted, its download checked by SHA-256. Weekly and on demand, `cargo audit` of the QOR core's lock file at the pin ([QOR-0018](decisions/QOR-0018-the-security-and-quality-bar.md)) |

All three run on pushes to `main`, on pull requests and on demand; Build also runs on `v*` tags and Security every
Monday. Pull-request runs cancel superseded runs. Each OS job runs only when that OS has a Qt source.

## What CI trusts

The workstation's runners run as its user, on the machine that holds the Store's signing keys and the Qt account, so a
job there can read them. So ([QOR-0018](decisions/QOR-0018-the-security-and-quality-bar.md)):

- **Every action is pinned by its commit**, with its version in a comment for Dependabot, which proposes the moves. A
  tag can be moved by whoever owns it; a commit cannot. Downloads are checked by SHA-256, and Python's tooling is
  installed by hash from `requirements-dev.txt`, generated from `requirements-dev.in`.
- **Dependabot's pull requests do not run on the workstation.** Build's Windows and Linux jobs skip them, so a person
  reads the update before its code runs there; a merge runs them as any push does. Lint and Security run on hosted
  runners, which hold nothing.
- **Anything that downloads a tool to run it goes in Security**, on a hosted runner.
- **Left to the owner:** the Store's keys off this machine (`OWNER.md`). Until then, a compromised job here can read them.

## What the Build job needs on a runner

- **The commercial Qt 6.12 kit** (below).
- **Rust 1.91 or later** (`cargo`) for the QOR core, which `core/` builds from
  demiurge-chain at a pinned commit through Corrosion
  ([QOR-0003](decisions/QOR-0003-the-qor-core-at-a-pinned-commit.md)). The CI
  presets set `QOR_BUILD_CORE=ON`, so a runner without Rust fails at configure
  instead of skipping the core. The workstation's runners use Rust 1.98.1.
  Linux also needs `libdbus-1-dev` and `pkg-config`, because the core's
  keychain uses the Secret Service.
- **Network access to GitHub** at configure time. It fetches demiurge-chain at
  the pin, and Corrosion v0.6.1.

### The carried products

Both jobs can also build and test QQ, ARQADE and QOR Media: Windows with
`QOR_RUN_GPU_TESTS`, through `tools/dev/products.ps1`, which calls each
product's own script, and Linux with `QOR_RUN_LINUX_PRODUCTS`, through
`tools/dev/products.sh`, QOR OS's Linux build of them. QOR Media needs PDFium,
which both scripts fetch at a pinned release and check by SHA-256.

The suites drive real windows, on Windows on the runner's desktop, so they run
only when:

- the push or pull request changes `products/`, `tools/qor-media/`,
  `docs/design/tokens.json`, `tools/dev/products.ps1` (`products.sh`,
  `products-display.qml`, `products-report.py`, `secret-service.py` and
  `.github/qt-packages.json` on Linux) or `build.yml`;
- the run is for a tag;
- the run was started by hand.

A product whose suites fail is run once more (on Linux, its failed suites).
On a Windows desktop someone is using, a test window can lose the keyboard to
another application, or a synthetic click to the real pointer, wherever it rests,
and a genuine failure fails both attempts. The step summary says whether the
products ran.

#### What Linux runs

**`QOR_RUN_LINUX_PRODUCTS` is not set, so today it runs none of this.** With it
set, the job runs every suite each product registers with CTest, **all of them
in software**, hidden:

- **Fenced from the GPU.** Under WSL the script hides `/dev/dxg` in a namespace of
  its own, and Mesa draws with llvmpipe. A machine with no door to a GPU at all
  draws in software the same way. ([Why the fence](#why-linux-is-fenced), and
  [BUILDING.md](BUILDING.md#on-linux).)
- **Out of sight.** A display of the script's own, a compositor on Qt's offscreen
  platform; a session bus and an in-memory keyring of its own; no sound device.
- **Within the machine's limits.** One job at a time and 8 compilers, as for the
  rest of the job. The tests are held to half the processors and three quarters of
  WSL's memory where systemd's user manager runs.

**A pass there says nothing of a real GPU, real sound or a real controller.** It
says what the games do, by their own clocks, and that their windows behave.
Nothing here replaces playing them on the Linux machine.

**The log and the step's summary name every case failed or skipped, with why**, in
each product's `what its suites said` (`tools/dev/products-report.py`, from CTest's own
log, the only place a passing suite's output is kept). A suite that stops before its
end says `STOPPED`. Nothing is excluded. These are the cases that do not simply pass:

| Case | In software, under WSL | Why |
|------|------------------------|-----|
| QQ's `tst_player`: `thePlaygroundPlaysAndItsStartIsMeasured` | **Fails, and so fails the step**, until the owner says otherwise | Its floor of 20 frames a second, a real performance check, reads 5 to 6 here. It stays a check on Windows. See below |
| QQ's `tst_agent`: `theRealProviderIsReachedOverTls` | Skipped, as everywhere | Needs `QQ_LIVE_ENDPOINT=1`, to reach the real provider |
| QOR Media's `playback`: `video_decodes_through_ffmpeg` | Skipped | Windows' sample video is not on this machine |
| QOR Media's `playback`: `it_docks_inside_the_launchers_window_and_lifts_out_to_full_screen` | Skipped | Wayland has no window handles to dock by; Prism places the window ([QOR-0010](decisions/QOR-0010-on-wayland-prism-places-the-products-windows.md)) |

**The QQ Player's floor** asks for 20 frames a second of the bundled playground. Software
cannot give that, and it says nothing of the game, so the floor is neither lowered nor skipped:
the case fails and says what it read. Whether Linux CI stays red on it or lets it stand is the
owner's decision. `tools/dev/products.sh --accept-software-floor` is the second. In software only, it
reports the failure and lets it stand when all of this holds: that check in that case is the product's
only failure, `tst_player` ended normally with that one failure as its exit status (CTest's record says
so; its log says only "Test Failed.", even of a crash), and the Player still drew **at least 1 frame a
second**, since below that it has stopped drawing, which is a fault anywhere. `build.yml` does not pass
it, so the step is red on this alone.

**How long.** On this laptop, with the builds incremental, about 70 minutes: QQ 17, ARQADE 50
(`games`, which is `tst_games`, 49), QOR Media under one. So a cold build and a second go at a
failed `games` fit the step's limit of 180 minutes, and the job has 270 with the variable set.
What the products built then has its exploit mitigations read back, as on Windows.

**On a GPU these suites check what they always did, and one thing more:** `tst_agent`'s
`aLongWaitLastsWhatItWasAskedFor`, on every machine. What follows is eased **in software alone**. The
suites choose that by the renderer's own name (`llvmpipe`, in `products/qq/tests/drawnin.h` and `tst_games`'
`drawnInSoftware`) and the script by the machine's devices. The owner has not been asked to accept these,
and they do not count as a pass of what they leave out.

**What software alone no longer checks:**

- **That the world keeps up with real time.** On a GPU the walks, the falls and Orb Run's first orb in
  `tst_play`, `tst_studio` and `tst_agent` wait on the wall's clock, so that a walk of over 3 metres in a
  second fails a world below two thirds of real time. In software nothing compares the world with the
  wall. A game has three clocks, which agree at 30 frames a second or more: a body moves by the physics
  world's (at most 1/30 s a frame), the Player's turn, fall and jump speeds go by the frame clock (at most
  1/20 s), and logic by the scene's ticks (at most 8/60 s). In software these cases wait on the clock the
  thing goes by, with the same numbers, and a limit on the wall's time that stops a hung game.
- **That frames pay logic's ticks at the right rate, against what moves bodies.** On a GPU `tst_play`'s
  `logicMakesAndRemovesEntitiesOnItsOwnClock` is paid by frames, so logic running ahead of bodies fails it. In
  software logic's clock runs up to four times ahead of the physics world's, so there it pays its ticks from the
  physics world's time, and `root.advance(frameTime * 3)` in `Scene.qml` passes it. Darkveil's two pilot cases are
  paid by frames on a GPU too, as a person's game is. In software the pilot pays them itself, one to each decision,
  because deciding once in eight ticks plays a rougher game: of two such runs of the first case, one went home to the
  healer before it met the Elder, which fails it, and one passed.
- **ARQADE's threaded render loop.** The script sets `QSG_RENDER_LOOP=basic` for it. On the threaded loop,
  on Wayland, with frames this slow, each turn of the event loop took one frame more than the last: at the
  13th turn of Orbital's first lock, 13 frames and 15 seconds. ARQADE keeps one window through all its cases,
  so in time nothing else ever ran. That was the stall at Orbital's ten locks. No frame was stuck, and the game
  was not looping. Why Qt does this is not known. Whether it happens at a GPU's frame rates, which would
  matter to a game in Prism, has not been looked at.
- **That a sound goes on sounding.** On a GPU `tst_games` finds each placed sound still playing, and Orbital's lock
  within a metre of the satellite as it is then. In software a timer puts a short sound away before a frame's
  drawing lets the test look, so it keeps each sound as it begins (`PlacedVoices`), which shows it began where it
  should, and compares the lock with where the satellite was as the lock was asked for.
- **Limits on the wall's time that stop a slow or hung game sooner:** `QTEST_FUNCTION_TIMEOUT` is 30 minutes and
  a suite's limit 90, not 5 and 15; Rift Survivor's hull has that case limit, less 20 seconds, not 2 minutes, since it
  takes 13½ to break; the drops case has 60 and 30 seconds, not 8 and 4.
- **Anything of a real GPU, real sound or a real controller**, as above.

**The GPU's side of each of these has run here only by forcing it on in software**, since no GPU may be used here. Forced, the
walks, the falls, the drops, Orb Run's first orb, the lock's sound and the hull's two minutes each failed, as they should
where software cannot keep up. The live check of a kill's sound and Darkveil's frame-paid first case passed. The other sounds and
Darkveil's second case were not run forced. Windows or Linux CI on a GPU must still confirm all of it.

**What the suites need of the machine.** Loopback: `tst_agent`, `tst_games` (its arenas and
sign-in) and `tst_arqadeserver` talk to stand-ins for the provider and ARQADE's server on 127.0.0.1,
so a job that closes it cannot pass them. `tst_lockdown` listens there too, but what it checks
is the refusal, which stands either way. The rest use Unix sockets at a path. The keyring needs
`dbus-daemon`, `python3-dbus` and `python3-gi`, and the display the kit's Qt Wayland Compositor.

#### Why Linux is fenced

On 28 October, three runs under WSL that drew on the GPU through Mesa's Direct3D
12 driver hung the workstation the Linux runner shares, at 1:04, 3:08 and 7:20 PM; each
took the power button, and Windows logged no GPU reset and no crash. Each run's
log stops where a QQ suite starts: tst_agent at 12:24, the Player's suite at 3:05,
tst_world at 3:51. ctest stops a suite after 900 seconds and says so, yet the first and
third logs stayed silent for 40 minutes and three and a half hours before Windows
hung, which fits a suite stuck inside the GPU driver, where no signal reaches it.
Before the second, QQ's play suite had crashed after Direct3D 12 removed its device
and Mesa failed to make a context (`EGL_BAD_ALLOC`). The script now fences runs under
WSL from the GPU ([BUILDING.md](BUILDING.md#on-linux)). The owner decides when to set
the variable.

## Providing Qt to CI

QOR OS uses the commercial Qt 6.12 kit, which CI cannot download
anonymously. Configure **one** of the following in
*Settings → Secrets and variables → Actions*.

### Option A: self-hosted runners (current)

Hosted Windows minutes are billed at 2× on private repositories, and a cold
Qt install takes several minutes. A self-hosted runner on a machine that
already has Qt is faster, free, builds incrementally and has a real GPU.

**Current setup.** The owner's workstation (the reference laptop) runs two
runners:

| Runner | Where | Labels |
|--------|-------|--------|
| `<COMPUTERNAME>-qtos` | `%USERPROFILE%\ghr\qtos`, work folder `w` | `self-hosted, Windows, X64, qt, msvc, gpu` |
| `<COMPUTERNAME>-qtos-linux` | WSL2 Ubuntu, `~/ghr/qtos-linux` | `self-hosted, Linux, X64, qt, gcc, wsl, gpu` |

- Both run as the signed-in user, so no admin rights are needed.
- A full Windows build of the OS with its tests takes about 2½ minutes once
  incremental. The first build with the QOR core compiles its Rust
  dependencies, which takes several more minutes.

### Where the runners are registered

A runner serves either one repository or the organisation. **Both runners are
registered to `QOR-MATRIX/qor-os`, since 29 October 2026.** From 27 October they
served the organisation, in its Default runner group, which does not allow
public repositories. On 29 October GitHub stopped giving jobs to the
organisation's runners (below); moved to the repository, they took the waiting
jobs within 15 seconds. The frozen QTOS needs none. The public demiurge-chain
must never run on this workstation: a self-hosted runner on a public
repository runs code from anyone's pull request.

`tools/ci/runners.ps1` moves both runners in one command:

1. It leaves a runner already registered at the scope asked for as it is, so
   running it twice is harmless.
2. It refuses to touch a runner GitHub reports busy. Stopping one mid-job
   orphans the job, and GitHub then refuses to remove the runner until the
   job times out.
3. It removes each other runner from where it is registered now, and
   registers it at the new scope with the same name and labels.
4. It pins the Linux runner's PATH (with `~/.cargo/bin`) and display
   variables.
5. It starts both, minimised.

Tokens are piped from `gh`, never printed.

```powershell
.\tools\ci\runners.ps1                       # both runners, for qor-os (the default, -Scope repo -Repo qor-os)
.\tools\ci\runners.ps1 -StartOnly            # just start them (after a reboot, without Startup entries)
.\tools\ci\runners.ps1 -Reregister           # remove and register both again where they are
gh auth refresh -h github.com -s admin:org   # only for -Scope org, which took no jobs on 29 October
```

QTOS is frozen, so its CI runs only if someone pushes to it.

Repository variables (*Settings → Secrets and variables → Actions → Variables*,
or `gh variable set <NAME> --body <VALUE> --repo QOR-MATRIX/qor-os`). The
private repositories of a Free organisation cannot read organisation
variables, so each repository sets its own:

| Variable | Value | Effect |
|----------|-------|--------|
| `QOR_WINDOWS_RUNNER` | `["self-hosted","windows","qt"]` | Windows job runs on the workstation |
| `QOR_WINDOWS_QT_DIR` | `C:/Qt/6.12.0/msvc2022_64` | Use the installed kit; skip the installer |
| `QOR_RUN_GPU_TESTS` | `true` (optional) | Also run GPU-labelled suites, and the products on Windows (opens brief test windows on the runner's desktop) |
| `QOR_RUN_LINUX_PRODUCTS` | `true` (optional, unset) | Also run the products on Linux, in software and out of sight ([what it runs](#what-linux-runs), [why it was off](#why-linux-is-fenced)), and lets the job take 270 minutes, its Products step 180 |
| `QOR_LINUX_RUNNER` | `["self-hosted","linux","qt"]` | Linux job runs in WSL on the workstation |
| `QOR_LINUX_QT_DIR` | `/home/<user>/Qt/6.12.0/gcc_64` | Use the installed kit |

The variables took their `QOR_` names on 27 October 2026
([QOR-0009](decisions/QOR-0009-the-name-is-qor-os.md)); the frozen QTOS keeps its own `QTOS_` set. Until they are set,
a job is skipped; once they are set, jobs wait in the queue whenever the runner
is not running.

**Starting the runners at sign-in** is a Startup-folder entry for each, which
the owner adds once. Assistants do not write Startup entries.

```powershell
Set-Content "$([Environment]::GetFolderPath('Startup'))\qor-github-runner.cmd" `
  "@start `"QOR OS Windows runner`" /min cmd /c `"%USERPROFILE%\ghr\qtos\run.cmd`""
Set-Content "$([Environment]::GetFolderPath('Startup'))\qor-github-runner-linux.cmd" `
  "@start `"QOR OS Linux runner`" /min wsl.exe -d Ubuntu --cd ~ --exec /home/$($env:USERNAME.ToLower())/ghr/qtos-linux/run.sh"
```

If CI does not run, first check that both runners are online:
`gh api repos/QOR-MATRIX/qor-os/actions/runners` (or
`orgs/QOR-MATRIX/actions/runners` once they belong to the organisation).

**Online and idle while jobs wait queued** means GitHub is giving a runner no
work, though it shows it online. On 29 October 2026 both runners lost their
connection to GitHub's job queue at the same second (10:36 UTC). Their logs
(`_diag\Runner_*.log`, `~/ghr/qtos-linux/_diag/` in WSL) end in `BrokerServer`
errors followed only by token requests. Restarting the Windows runner (its
`Runner.Listener` stopped while GitHub reported it idle, then
`tools/ci/runners.ps1 -StartOnly`) reconnected it: for two minutes it reported
"A session for this runner already exists", then "Listening for Jobs". Yet
neither the jobs queued before nor a new run's reached it in ten minutes. Both
runners have run on "migrated settings" (`.runner_migrated`) since GitHub
refreshed their configuration at 05:48 UTC that day, which points to a change
on GitHub's side. At the owner's word, `tools/ci/runners.ps1 -Reregister`
removed both and registered them again at 17:41 UTC, as runners 236 and 237,
their migrated settings deleted. Both connected and listened, and no job
reached them in 45 minutes. The labels, the runner group (open to every
repository in the organisation), the Actions permissions, the bill (the private
repositories had used 112 of their 2,000 free minutes in October, with no charge
for self-hosted runners) and GitHub's status page (Actions operational) were all
in order. **Registered to the repository instead
(`tools/ci/runners.ps1 -Scope repo -Repo qor-os`), both took the waiting
Build's jobs within 15 seconds**, so GitHub's routing to the organisation's
runners was what failed. They stay registered to the repository, which is now
the script's default. A newer run supersedes queued ones (`concurrency` in
`build.yml`), so a push, or a run started by hand, tries again.

#### Setting up a runner from scratch

```powershell
# Windows, in an empty folder holding the extracted actions-runner-win-x64 zip:
$token = gh api -X POST repos/QOR-MATRIX/qor-os/actions/runners/registration-token --jq .token
.\config.cmd --unattended --url https://github.com/QOR-MATRIX/qor-os --token $token `
  --name "$env:COMPUTERNAME-qtos" --labels qt,msvc,gpu --work w --replace
.\run.cmd
```

For the organisation, use `orgs/QOR-MATRIX/actions/runners/registration-token`
and `--url https://github.com/QOR-MATRIX`.

#### Linux runner in WSL2

The Linux job runs inside WSL2 Ubuntu, using the commercial kit in `~/Qt`
([BUILDING.md](BUILDING.md#linux-on-wsl2-windows-workstation)) and Rust from
rustup in `~/.cargo` (1.98.1).

With `QOR_RUN_GPU_TESTS`, the Linux job also runs two things on WSLg's
display: the GPU suites, and `tools/dev/prism-protocol-check.sh`, which
connects real clients to the built compositor.

`prism_products` runs in the plain test step, without it. Prism runs offscreen
and in software, in a runtime folder of its own, with the session's display
variables removed. Nothing reaches WSLg's display or the GPU.

Two runner files are pinned rather than captured from the shell that
configured the runner. `tools/ci/runners.ps1` writes both:

- **`.path`** holds a Linux-only PATH, starting with `~/.cargo/bin`. WSL
  appends the whole Windows PATH, so a job could otherwise run a Windows
  `.exe` for a tool missing on Linux.
- **`.env`** holds WSLg's display variables. `WAYLAND_DISPLAY` is the absolute
  path of WSLg's socket, `/mnt/wslg/runtime-dir/wayland-0`. With systemd
  enabled, `/run/user/<uid>` is recreated whenever the distribution starts,
  and WSLg's socket is not always linked into it, so a relative `wayland-0`
  can point at nothing and GPU tests would wait for a window forever. The
  GPU step checks for the socket and fails with this pointer instead.

GPU suites run with a 120-second timeout and one retry
(`ctest --repeat until-pass:2`). Under WSLg a window is occasionally never
reported as exposed. A genuine failure still fails both attempts.

```bash
# Set up (in WSL). Rust and the core's system package first:
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --profile minimal --default-toolchain 1.98.1
sudo apt-get install libdbus-1-dev pkg-config
# Download the actions-runner-linux-x64 tarball, check its SHA-256, extract it to ~/ghr/qtos-linux, then:
./config.sh --unattended --url https://github.com/QOR-MATRIX/qor-os --token <registration token> \
  --name "$(hostname)-qor-linux" --labels qt,gcc,wsl,gpu --work _work --replace
printf '%s\n' "$HOME/.cargo/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/lib/wsl/lib" > .path
printf '%s\n' LANG=C.UTF-8 DISPLAY=:0 WAYLAND_DISPLAY=/mnt/wslg/runtime-dir/wayland-0 XDG_RUNTIME_DIR=/run/user/$(id -u) > .env
```

WSL keeps the distribution running while the runner's `wsl.exe` process is
alive, so start the runner from Windows (`tools/ci/runners.ps1 -StartOnly`, or
the Startup entry above).

**One machine, one job at a time.** Both runners are this laptop: 20 threads, 16 GB, and someone using it. So the Linux job
waits for the Windows one, each build runs at most 8 compilers (`CMAKE_BUILD_PARALLEL_LEVEL`) and Cargo 8 jobs. Left to
itself each build starts 22. On 31 October 2026, 15 minutes into two full rebuilds at once (the exploit mitigations had
changed every object), with the laptop in Modern Standby, it reset itself: Kernel-Power 41, with no bugcheck, dump or
press of the power button (`ConnectedStandbyInProgress` true). Nothing it logged says why, so the load is the part that
could be changed, and was. The Windows suites also have the machine to themselves now, which their timing wants.

A build that **stops partway with no failure in its log**, both jobs ending together, means the machine stopped. Look at
the System log first: on 30 October a Build "failed" that way at Windows' critical-battery shutdown (Kernel-Power 524).

**Windows barely draw once the display has timed out.** This laptop's only sleep is Modern Standby, which starts when the
display times out: after two hours on mains, ten minutes on battery. The machine goes on working and the runner goes on taking
jobs, but animations hold still, a game's clock stops and the clipboard cannot be opened. On 1 November 2026 a Build failed this
way twice over: `tst_kit_qml` in one standby, then in the next 40 cases of ARQADE's `tst_games` and others in QOR Media, at each
of a product's two attempts, for an hour. QQ's suites passed through it, so one product passing proves nothing. Failures across
unrelated suites, with `it_draws_on_the_graphics_card` among them (fewer than 30 frames swapped), mean this; the System log says
so, in Kernel-Power's events 506 and 507 and their reasons.

So on Windows the steps that draw go through `tools/ci/with_display.py`. It holds the display on while they run, by a request
that ends with its process and changes no setting. If the machine is already asleep it wakes it, and if it cannot, or the
desktop is locked, it says so and runs nothing. If the machine sleeps anyway (a lid, the power button), the step says that too.
Measured on this laptop: a request alone does not wake it, a mouse move of no distance does, and woken with nothing holding it
it was asleep again in 32 seconds. The next Build's Products step ran 24 minutes through it with no standby, and ARQADE, which had
failed 40 cases, passed all of them first time. Linux's suites draw in software out of sight, and passed through both standbys.

**Something can hold the clipboard.** `tst_reader`'s `a_pages_text_is_selected_and_copied` is the one case that uses the real
clipboard, and it clears what was on it. On 1 November 2026 it failed on both attempts, with `OleSetClipboard` reporting
`0x800401d0: OpenClipboard Failed` and Qt "Unable to obtain clipboard", in a step otherwise clean. Every program on the machine
was refused (`OpenClipboard`, error 5) with no QOR program running and no window named as the holder, so a process held it
without a window. It let go after at least an hour and three quarters, in the minute a mouse woke the machine, with no restart,
and the job then passed when run again. What held it is not known: WSLg's viewer `msrdc.exe` was suspected, and was still running
when it let go. Check by opening the clipboard from another process, and run the job again once that succeeds.

**A stopped machine leaves damage behind.** The next Build on the Linux runner failed to link, with undefined references to
code that had not changed: five objects written as the machine stopped were empty, dated a minute before it, so Ninja took
them as built. A Build now deletes empty objects first. Anything stranger in a folder that used to build (a partly written
file has no such mark) means deleting the runner's `build` folder while it is idle and running the failed job again.

**WSL's clock drifts.** After the laptop sleeps, WSL's clock can be hours away
from Windows'. It was 6 h 25 min ahead on 26 October 2026. The runner's session
is a signed token with a time window, so GitHub refuses it, and the runner logs
"The runner registration has been deleted from the server" and exits, even
though the registration exists. `tools/ci/runners.ps1` sets WSL's clock from
Windows before it starts the runners. To fix it by hand:
`wsl -u root date -u -s "<Windows' UTC time>"`, then start the runner again.

### Option B: hosted runners with the Qt Online Installer

Add repository **secrets**:

- `QT_INSTALLER_JWT_TOKEN`: a Qt Account JWT for unattended installs (preferred), **or**
- `QT_INSTALLER_EMAIL` and `QT_INSTALLER_PASSWORD`: the Qt Account login.

`tools/ci/install_qt.py` downloads the official online installer and installs
the kit and add-ons in [`.github/qt-packages.json`](../.github/qt-packages.json)
in unattended mode. It then checks that the CMake packages listed under
`cmake` exist, because the installer exits successfully even when it skips a
package it cannot resolve. The result is cached with `actions/cache` (keyed on
OS, Qt version and the package list). Credentials are passed through the
environment and are redacted from logs. Hosted images include Rust.

The same script sets up a workstation or a self-hosted runner. Without
credential variables it uses an existing Qt Account sign-in (`qtaccount.ini`,
written by any interactive Qt installer run), and it adds packages to an
existing installation through that installation's MaintenanceTool:

```bash
python3 tools/ci/install_qt.py --platform linux --root ~/Qt   # e.g. WSL2 Ubuntu
```

To change modules, edit `qt-packages.json`. Since Qt 6.8, add-on IDs are
architecture-independent (`qt.qt6.6120.addons.qtgraphs`); the per-kit IDs are
virtual and cannot be selected. List available IDs with the installer's
`search` command, for example
`qt-online-installer-windows-x64-online.exe --root C:\tmp search qt.qt6.6120`.

Without either option, the Build workflow's preflight job succeeds and writes
a summary explaining how to enable Qt builds, so pull requests are not
blocked.

## Branch protection

[`.github/rulesets/main.json`](../.github/rulesets/main.json) defines the
`main` ruleset:

- no deletion and no force-push
- linear history
- pull requests required, with stale approvals dismissed and conversations resolved
- the Lint job required
- repository admins can bypass

Apply it with `python tools/github/bootstrap.py --ruleset`. GitHub only
enforces rulesets on **private** repositories on the Team or Enterprise
plans (or on public repositories). On the Free plan the command reports that
the ruleset could not be applied.

## Dependabot

`.github/dependabot.yml` updates GitHub Actions weekly (grouped) and the
pinned Python tooling monthly.

## Local parity

Everything CI runs can be run locally:

```powershell
python tools/tokens/generate_tokens.py --check
python -m unittest discover -s tests/tools -v
python tools/store/build_store.py check
python tools/ci/check_headers.py
.\.venv\Scripts\pre-commit run --all-files
.\tools\dev\build.ps1 -Preset ci-windows -Test
.\tools\dev\build.ps1 -Preset ci-windows -Target all_qmllint
.\tools\dev\products.ps1
```

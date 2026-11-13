# Security policy

## Reporting a vulnerability

Please **do not** open a public issue for security problems.

Report privately through GitHub's
[security advisory form](https://github.com/QOR-MATRIX/qor-os/security/advisories/new),
which, while this repository is private, only people with access to it can reach. There is
no public route yet, and one is a release gate ([ROADMAP](docs/ROADMAP.md#release-gates)).
Include:

- the affected component: the compositor, a service, an app, QQ, ARQADE, QOR
  Media, the QOR core and vault, the image or updates;
- the commit SHA or version;
- steps to reproduce, or a proof of concept;
- the impact you expect: for example privilege escalation, sandbox escape,
  a key or token exposed, or a spoofed trusted-UI prompt.

Please do not test against anyone else's device or account.

A problem in the QOR core, or in the network services (the chain, QOR ID,
Social, ARQADE's website), belongs to demiurge-chain, where that code lives. Report it as
[its SECURITY.md](https://github.com/QOR-MATRIX/demiurge-chain/blob/main/SECURITY.md)
says, or here, and we will move it.

You will receive an acknowledgement within **3 business days**. We aim to
deliver a fix or mitigation within **30 days** for high-severity issues and
will coordinate disclosure with you.

## Supported versions

QOR OS is pre-release. Only the latest commit on `main` receives security
fixes.

| Version | Supported |
|---------|-----------|
| `main`  | Yes |
| older   | No |

## Security model

The intended device security model is described in
[`VISION.md` §3](VISION.md#3-foundation-image-boot-updates-security). In
summary:

- a signed boot chain, and whole-image updates with rollback;
- sandboxed games and apps that reach the system only through portals and
  service contracts;
- a vault service, built from the QOR core, that keeps keys outside the
  compositor and every app, and never answers with a key;
- system prompts and key approvals drawn by Prism in a trusted layer that
  clients can never cover.

Secrets never reach a log, test output, screenshot or commit
([`AGENTS.md`](AGENTS.md) §6).

The **simulator** (`qor-shell`, `qor-simd`, and Prism's embedded services) is a
development tool. It exposes simulation controls (`SimControl`) on a local socket
with no authentication, its lock screen takes any password, and it decodes media
inside the shell. A device's image is built without it
(`-DQOR_BUILD_SIMULATOR=OFF`), which removes it from Prism altogether. With it go the options only tests and a
developer's runs need (`prism/compositor/commandline.h`): `--script`, which reads input steps from a file and can call
the shell's own methods, `--unlocked`, which starts past the lock screen, `--screenshot` and `--delay`, and
`--benchmark` and `--benchmark-out`, which unlock and drive the shell too. In such a build they are no options at all:
asking for one is an error before anything is read (`tst_prismoptions_device`).

## What is checked by machines

[QOR-0018](docs/decisions/QOR-0018-the-security-and-quality-bar.md): exploit
mitigations on every program, read back in CI; the suites under AddressSanitizer
and UBSan; everything CI runs pinned by content; secrets looked for at the door
and through the whole history; the QOR core's dependencies audited weekly.
These are configured gates, not a claim that every job ran on the current commit.
Qt jobs remain paused pending runner isolation. Local results and omissions are
recorded in `OWNER.md`; [CI.md](docs/CI.md) distinguishes configuration from execution.
The Actions policy is checked offline and its server settings were read back on
13 November: selected action repositories, full commit pins and read-only default
tokens. Branch protection remains unavailable on the current private-repository
plan. The prepared ruleset is not an enforced control.

## What a game or an app can and cannot do

- **Prism treats every Wayland client as hostile.** Its name is a claim: a client
  calling itself Settings or ARQADE is kept apart from them. It takes the keyboard
  only when it can be seen and it steals it from nobody, and never on the lock
  screen. Its title is text, never markup. It captures the pointer only as the active
  window with no shell surface open, after a person's action on it, and
  <kbd>Super</kbd>+<kbd>Esc</kbd>, the lock screen and any shell surface end that. No client
  hears the pointer while a shell surface is open, nor over anything of the shell's lying
  over its window ([SHELL.md](docs/design/SHELL.md#capturing-the-pointer)).
- **The QQ Player keeps a game to its package** by two walls: QQ's own checks
  (`products/qq/runtime/confinement.h`), and on Linux the kernel's
  (`products/qq/player/sandbox.h`): Landlock for files and programs, seccomp for
  the network.
- **QQ Studio runs a project's code with the person's own reach**, since making a
  game means running it. So a project made elsewhere runs nothing until they say
  they trust it, nothing in it can bring in native code, and logic an AI agent
  wrote runs only once the creator allows it.

## Known limits

Say so if you find a way past one; these are the walls we know are low.

- **The pinned core's dependency audit fails.** The 13 November
  [hosted audit](https://github.com/QOR-MATRIX/qor-os/actions/runs/43474225093)
  found four vulnerability advisories and 13 additional warnings in the lock file
  at `6a4822d`: `libsqlite3-sys 0.38.2` (RUSTSEC-2026-0459),
  `quinn-proto 0.11.18` (RUSTSEC-2026-0523 and RUSTSEC-2026-0524), and
  `quinn-udp 0.5.15` (RUSTSEC-2026-0538). Current upstream `74a1014` still locks
  those versions. The SQLite issue depends on opening an untrusted database/WAL
  pair; the Quinn UDP advisory excludes Linux and Windows. Application feature
  selection and reachability need review; a lock-file finding alone does not prove
  exploitation. Fix and test dependencies upstream, rerun the audit, then review
  the QOR OS pin. No advisory was ignored or security gate weakened.
- **This repository was public from 27 October to 1 November 2026,** against its own
  documents, while CI ran on the owner's machine. It was recorded as made private on 1 November;
  private visibility was reconfirmed through the repository API on 13 November. Treat the code as
  copied up to `a3a2fd4`. GitHub counted 239 clones (85 unique) and does not say by whom. CI's
  jobs explain at most about 163 of them, and the gap is widest on 31 October, at 133 clones
  against 45 jobs. The history's secret scan passed at that commit, and no artifact was
  kept, so a copy should hold no secret, though that scan is not proof. A copy holds the code,
  the plans and the Store's public key. CI's logs were readable as well. A scan of all 120 runs'
  logs on 1 November, by the patterns `tools/ci/check_secrets.py` uses, found no key, token or
  email address, and neither the signing folder's name nor the Qt account file's. They do show
  the workstation's name and its Windows user folder. **Nothing found shows that outside code ran on that
  machine:** of 117 runs, all came from this repository, 110 triggered by the owner and 7 by
  Dependabot; there were no forks and no pull requests. The signing keys' last-access times, and
  their backup's, have not moved since 27 October, which is consistent with no later read and
  does not prove it.
- **A CI job on the workstation can reach what its user can:** the Store's keys and backup,
  and SSH, cloud, GitHub CLI and Qt credentials. Those jobs are off until their runner is isolated
  ([docs/CI.md](docs/CI.md#what-ci-trusts)). The "runners stay online" observation belonged to
  1 November; later repository policy requires them stopped pending isolation. Current workflow
  configuration remains paused. GitHub reported both registered runners offline and not busy on
  13 November; local runner services and GitHub Apps were not inspected. Any registered online runner remains addressable by workflows
  allowed to target it; a paused job alone does not establish host isolation.
- **Under WSL, any Linux process can reach the Windows drive with the Windows user's rights.** Measured on 1 November 2026: with
  drives unmounted and interop off, a process with no privilege opened `AF_VSOCK` to Windows' 9P file server and attached to
  `C:\`. Only a filter on the process closes that, as in `qor-ci`'s job sandbox and the QQ Player's wall. So nothing run in WSL
  without such a filter is contained, whatever `wsl.conf` says. A device has no WSL.
- **A game past QQ's own checks can still use local sockets on Linux,** which the display and sound need. The kernel's wall
  refuses it the network and its files, not what listens on a local socket.
- **On Windows the Player has only QQ's own checks.** Windows is a development
  host, not the device. Qt Quick's `Canvas` (reads any picture) and `grabToImage`
  (writes over any picture) and Qt Multimedia's camera, microphone, screen capture
  and recorder types cannot be refused from inside the process. A stranger's game
  plays only in the browser's sandbox (Demiurge's ADR-087).
- **The bus trusts anyone who reaches it**, which is the person's own processes.
  Until games and apps are sandboxed (Phase 3), a process running as the person can
  crash whatever hosts the bus: Prism, with the simulator. Qt Remote Objects'
  decoder trusts what it is sent. So: nothing sandboxed is ever given the bus.
- **Prism's clipboard is Qt's**: any client may set the selection. Reading it takes
  the keyboard, which a client no longer gets unseen.
- **A format 1 Store catalogue has no sequence or expiry.** Replaying an older one can
  hide updates, but it cannot move anything installed back to an older version. Once a
  store key has had a format 2 catalogue accepted, format 1 is refused under that key.
  Format 2 carries both. The memory of what was last accepted is a file the person's own
  processes can delete; if it cannot be read it is treated as absent, with a warning,
  since the two cannot be told apart, and an older catalogue under that key can then be
  accepted again. The clock is trusted, and nothing limits how far ahead an expiry may
  be. Both wait for Store stage 2.
- **A Store package is checked on the thread that draws the shell.** At the 32 MB limit
  the slowest text measured takes about six seconds under the sanitizers, and the
  slowest JSON about two. Only a catalogue the store has signed can list such a package.
  A later stage moves the check off that thread.
- **A same-user process can rewrite a widget** after it is checked and before Qt
  reads it, in a small window. A read-only image closes that (Phase 3).
- **A process running as the person can drive QQ Studio.** Its tools' pipe admits
  only that person, and has no token of its own, so any of their processes can use
  an open Studio's tools. `QQ_ANTHROPIC_URL` and `QQ_KEYCHAIN_TARGET`, which the
  checks use, are honoured too, so whoever starts Studio can send its agent's key to
  another https server, or have it use another credential.
- **Letting the agent play its logic is one switch for the session.** Off by
  default, and it says logic runs with everything the person can reach. Once on, it
  covers all the logic the agent writes in that session, unread.
- **A process running as the person can spoil a product's start** by connecting
  first to its channel: the connection is refused as not the product Prism started,
  and the channel closes, so the product does not open. It cannot pose as the product.
- **In the simulator, a music file is decoded inside Prism**, by Qt Multimedia, so
  a hostile file reaches the compositor's process. No device build has the simulator.

Before anything holds value, the QOR core and the vault service get an
external security review, and the key Demiurge's records list as exposed on
5 October 2026 is replaced
([QOR-0008](docs/decisions/QOR-0008-before-a-public-release.md)).

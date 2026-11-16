# Continuous integration

Hosted Lint and commit hooks also run `tools/ci/check_domains.py`, enforcing
[QOR-0026](decisions/QOR-0026-qorsync-is-the-public-domain.md) for supported tracked
text formats. It checks source references, not live DNS, hosting or installed
client settings. Restricted secret files are not read, and findings print only
file/line locations. Its hostile-input tests run in the Python tooling suite.

Workflows live in [`.github/workflows/`](../.github/workflows/).

| Workflow | Runs on | Needs Qt | Checks |
|----------|---------|----------|--------|
| **Lint** (`lint.yml`) | `ubuntu-24.04` | No | Generated tokens are current, Python tooling suites under `tests/tools` (including portable Q Pi fixtures), the Store's signed catalogue, SPDX headers, no secret in the tracked files, the rules of QOR OS's own ARQADE games in Node (`products/arqade/tests/*.test.mjs`, [QOR-0017](decisions/QOR-0017-arqades-new-games.md)), clang-format 20.1.8, JSON validity. The products keep their own form, so the format checks skip them; the header check skips only their files from demiurge-chain (`LICENSES/from-demiurge-chain.txt`, [UPSTREAM.md](UPSTREAM.md)) |
| **Build** (`build.yml`) | Windows and Linux, self-hosted (or hosted with the online installer), subject to isolation gates | Yes (commercial) | Configure (`ci-windows`/`ci-linux`, warnings as errors, the QOR core required), build, every program's exploit mitigations read back, and no Store demonstration in the folder (`tools/ci/check_hardening.py`), `all_qmllint`, `ctest -LE gpu` (plus `-L gpu` when enabled). Linux also builds/tests the `linux-asan` non-GPU subset with AddressSanitizer and UBSan; that preset disables the core and platform plugins and does not run product sanitizer suites. On each where its variable says so, the products (QQ, ARQADE, QOR Media) when requested. Tags and manual runs also upload a Windows application bundle (kept 7 days) |
| **Security** (`security.yml`) | `ubuntu-24.04`, never the workstation | No | `gitleaks` over the whole history, redacted, its download checked by SHA-256. Weekly and on demand, `cargo audit` of the QOR core's lock file at the pin ([QOR-0018](decisions/QOR-0018-the-security-and-quality-bar.md)) |
| **Homepage** (`homepage.yml`) | `ubuntu-24.04` | No | Node 24 locked install with scripts disabled; static public output, content/ownership checks, SDK/privacy tests and npm audit. No deployment credentials are used |

All four run on pushes to `main`, on pull requests and on demand; Build also runs on `v*` tags and Security every
Monday. Pull-request runs cancel superseded runs. Each OS job runs only when that OS has a Qt source.

The main suites register `tst_scriptprivacy` when Prism and Python are built. It drives an actual private simulated
compositor with public synthetic nested arguments, checks native exit 4 and absence of those arguments from diagnostics,
and keeps captured output in memory even on failure. Failed script steps report their number and existing reason without
serializing the complete request object. The test uses a short private Unix temporary root to keep nested socket names
within the operating system's limit. Local normal/ASan checks pass its three Qt cases plus the existing client-identity
and item-text privacy controls, zero failures/skips; this does not mean paused CI or native hardware checks ran.

## What CI trusts

**The workstation Qt-job pause remains in force pending runner isolation.** GitHub
reported both registered runners offline and not busy on 14 November 2026. Both
isolation-enabling repository variables remain absent. That
API observation does not inspect local services or prove isolation. Lint and Security are configured on hosted
runners, including portable tooling tests. Qt application/build evidence must name
the actual machine, revision, commands, failures and skips; registration or a fixture
pass does not establish that a Qt CI job ran.

The same session found no provisioning or proof folder for the proposed
`qor-ci-vm` at its documented local locations. Its terminal has neither elevated
administrator nor Hyper-V administrator access. This establishes missing local
access/evidence, not an inventory of every VM. Qt CI stays disabled until a
replacement passes the documented isolation proof. Receipts:
`build/development-2026-11-14/session-06/`.

No workflow builds or tests native ARM64 QOR applications. Portable tests may
invoke `q-pi-release` to verify refusal before compiler/Qt execution; the Linux Qt
package configuration is still x86-64 (`gcc_64`). Those fixtures therefore establish
neither native ARM64 CI nor a qualified Pi builder, target kit, image or hardware.

**A runner being there is not a build having passed.** Runners online, runners allowed, a job that ran and a test that passed are
four different things:

- Whether a runner is online is the owner's to read: `gh api repos/QOR-MATRIX/qor-os/actions/runners`. A job's token cannot.
- Whether a job may use one is **Qt availability**, the first job.
- Whether a build and its tests ran and passed is **Release readiness**, the last job, on a machine of GitHub's. It fails unless
  every step a release needs ran and passed: each platform's job, the suites that draw, Prism's protocol check, and the products
  (which may stand down when a push or pull request did not change them, and never at a tag or a manual run). It also requires
  the products run unrelaxed on a Linux machine with a graphics card, since in software some of their checks are eased; no job
   supplies `LINUX_PRODUCTS_GPU` to readiness yet, so that line reads "not run" and no commit is ready for a release until one does. So a run that built
  nothing is red, as the Build workflow's result, by design. Its summary tells a step that failed from a step that did not run.
  An allowed unchanged-product skip retains exit 0 for that non-release check, while its summary explicitly says
  **Non-release checks passed** and **Not ready for a release**. Only every required step actually succeeding produces
  **Ready**; skips at a release still fail. Synthetic-status tests exercise real child exits and private summary files:
  16 readiness tests pass per Windows/Linux; workflow checks pass 14 with one Bash-only skip on Windows and all 15 on
  Linux. This is tool validation, not a real CI run or isolation proof.
  `tests/tools/test_release_readiness.py` and `test_build_workflow.py` hold it to this.

The recorded workstation Windows runner configuration used the signed-in person. Its Linux runner used WSL in the `sudo` and `docker`
groups, with the Windows drives mounted and Windows programs a call away. That machine holds the Store's signing keys and
their backup, and SSH, cloud, GitHub CLI and Qt credentials, so a job there can reach them all, and these jobs run other
people's code: pinned actions, and the Rust crates the QOR core builds, with their build scripts. Running a job hidden, one
at a time or away from the GPU changes none of this. So:

- **A workstation runner runs nothing until a person says it is isolated:** `QOR_WINDOWS_RUNNER_ISOLATED` or
  `QOR_LINUX_RUNNER_ISOLATED`, set to `true`, and neither is set until its replacement has been shown to hold. A second account
  on the same machine does not qualify until a job under it has been shown unable to reach the first's files and privileges.
  Without it nothing is built or tested, and Release readiness fails.
- **The job's first step checks that claim** before anything from the repository runs, and a job that can still reach Windows
  or a credential stops there.
- **On Linux it stops if `AF_VSOCK` opens at all.** Under WSL that reaches Windows' file server whatever `wsl.conf` says
  (measured 1 November 2026). Elsewhere such a socket can open with nothing behind it, and the job still stops: a Linux job must
  run where `AF_VSOCK` is refused.
- **What the recorded pause leaves open:** stopped runners remain registered. Started again, they would be reached by any workflow
  naming their labels, so
  whatever can write workflows could run code there. That is the owner, and the GitHub Apps installed on the organisation with
  workflow access, until the owner limits them. The check covers the places it names, not everything a job could do.

The 1 November 2026 repository/API review recorded the Free plan, no forks or fork pull requests, a read-only
default token and no secrets for a job to take. Branch protection and rulesets were refused by the API,
which required a paid plan; secret scanning was unavailable. The checks at commit time are local and can be skipped, and the secret
scan in Security comes after the push; neither equals a control on the server. Hosted minutes come from the plan's allowance,
2,000 a month in the plan documentation reviewed then. The billing API recorded 129 minutes from 27 October to
1 November, at no charge. These are dated observations, not current billing or account guarantees;
recheck the actual repository/plan before changing CI. Exhausted hosted minutes would also prevent Qt availability and Build.

As for the rest ([QOR-0018](decisions/QOR-0018-the-security-and-quality-bar.md)):

- **Every action is pinned by its commit**, with its version in a comment for Dependabot, which proposes the moves. A
  tag can be moved by whoever owns it; a commit cannot. Downloads are checked by SHA-256, and Python's tooling is
  installed by hash from `requirements-dev.txt`, generated from `requirements-dev.in`.
- **Dependabot's pull requests do not run on the workstation.** Build's Windows and Linux jobs skip them, so a person
  reads the update before its code runs there; a merge runs them as any push does. Lint and Security run on hosted
  runners, which hold nothing. So Release readiness never reads ready on a Dependabot pull request, by design.
- **Anything that downloads a tool to run it goes in Security**, on a hosted runner.
- **Left to the owner:** the Store's keys off this machine (`OWNER.md`). Until then, a compromised job here can read them.

### An isolated place for Linux jobs

`tools/ci/isolated-linux-runner.ps1` makes `qor-ci`, a fresh Ubuntu 24.04 from Microsoft's catalogue, needing no administrator
and holding no credential. The Qt kit is streamed in from `~/Qt/6.12.0/gcc_64`, and nothing else from that home. **It serves no
job yet:** no runner is registered to it and Build does not use it.

**`wsl.conf` is not the boundary.** With automount and interop off, a process with no privilege still reached Windows' 9P file
server over `AF_VSOCK` and attached to `C:\` (measured on 1 November 2026, WSL 2.7.11). So a job runs through `qor-ci-job`, a
sandbox whose system-call filter refuses `AF_VSOCK` and `io_uring`, with Landlock scoping and a per-unit network filter. A plain
`wsl -d qor-ci -u runner` is outside it, and `check_runner_boundary.sh` fails there on purpose. Anyone already on Windows as the
owner can be root in `qor-ci`.

**What the distributions share.** One kernel, one user namespace, one network namespace (so one loopback), one `/dev` and one
`binfmt_misc` table. A second distribution's start can empty that table, and Windows programs then answer `Exec format error` in
the other. The script masks the units that write to shared things and unmounts the table, and repairs what a first start cannot
be stopped from doing. Never `chmod` or `rm` a device node here, and never stop `systemd-binfmt`.

**The Qt licence is an open question.** The kit's `moc` is a licence wrapper. In `qor-ci` it says it cannot find Qt's licence
service and cannot acquire a licence, then works and exits 0. Neither the service nor an account file is copied in, and the bypass
variable its message names is not set. Whether building in that state is allowed is for the owner and Qt, so **`qor-ci` takes no
runner and no further builds until that is answered.** The clauses, and the question, are in
[release/QT.md](release/QT.md#ci-machines-that-hold-no-account).

**What remains.** One kernel and VM with the other distributions, so a kernel exploit gets everything; this is not a VM per job. A
job has the internet, so it can send out the source and its token. It can see the other distribution's sockets in `/proc/net`, and
it shares CPU and memory. Only private address ranges are refused, so a public address of this machine would not be. A job's DNS
goes to public resolvers, because WSL's DNS address is local and any listener on all addresses answers there.

**Before Build can move here.** The GPU and Prism-protocol steps need a display, which it lacks. Whatever listens on loopback
needs another answer, since the sandbox closes it. Each job starts from an empty home, so a cold build each time, and there is no
Docker. Its labels include `qt`, so point `QOR_LINUX_RUNNER` at `isolated` and `QOR_LINUX_QT_DIR` at `/opt/qt/6.12.0/gcc_64` before
registering, or a job could land on either runner. The Windows job needs its own answer.

**Second stage, not run.** `-Action Register` serves one `--ephemeral` job at a time from an empty home, with the token piped from
`gh` and never printed. It holds no credential between jobs, and nothing starts it at sign-in.

### A clean VM for Linux jobs, not yet made

`tools/ci/isolated-vm.ps1` makes `qor-ci-vm`, a generation 2 Hyper-V VM with its own kernel, and `-Action Prove` attacks it from
inside with `tools/ci/isolated-vm-guest.py`. This is the replacement [release/QT.md](release/QT.md#ci-machines-that-hold-no-account)
recommends for the Linux job. **Nothing in it has run against Hyper-V, and no such VM exists:** this account cannot call Hyper-V,
so `-Action Create` first runs when the owner runs it once, as administrator, after reading it. Until `-Action Prove` has run there, it
says nothing about any boundary. It holds no Qt, no kit, no Qt login, no runner, no GitHub token and no repository variable. Those wait
for Qt's answer and for the proof.

**What it makes,** and `-Action Remove` takes away, by these exact names and each only if it carries the script's mark: the VM and an
internal virtual switch `qor-ci-vm`, that switch's adapter with one address, a NAT `qor-ci-vm`, the firewall rules
`qor-ci-vm-block-by-adapter` and `qor-ci-vm-block-by-address`, a folder (`X:\qor-ci-vm` unless told otherwise, locked to
administrators and SYSTEM so no ordinary process can place or change the image, disk or seed inside it), `%LOCALAPPDATA%\qor-ci-vm`
(the SSH key that opens this guest and nothing else, and Prove's reports), and `%ProgramData%\qor-ci-vm` (a record of what was made,
which only an elevated run can write). It changes no other VM, switch, rule or setting, and it stops rather than touch one of those
names that it did not make. Windows supports one NAT per host, so another NAT stops it too. An elevated run refuses to start unless it
was launched with `-NoProfile`, so that no profile script or user module runs first.

**Who can have made its folders.** `X:\` and `C:\ProgramData` both let any process make a folder in them, so a folder with the right
name proves nothing. The two locked folders are made already owned by Administrators and locked, in one step (an ordinary account
cannot make one so, and a test shows Windows refusing it). One that is there already, or that appears between the look and the
making, is used only if it is owned by Administrators or SYSTEM, writable by nobody else, has no link on its path, and no folder above
it lets an ordinary process move it out of the way; otherwise the run stops and changes nothing. An owner can always rewrite a file's
permissions, unelevated too, so what the script writes in them is given to Administrators, and the folders carry an `OWNER RIGHTS`
entry that takes that right away from whoever owns a file inside (a test shows that too). The record is read only after the check:
Create, Check and Start stop at one that fails it, and Prove, Stop and Remove go on without it, with the defaults, and say so. A disk is
trusted only if the record says this script built it and the disk passes the same check, and the seed is checked before it is
attached. `-Action Remove` empties the root only if it has the script's mark and passes the check, deletes from `%LOCALAPPDATA%` only
the files it made there, by name, and deletes the record only if it passes the check. No path it deletes along may be a link.

**The VM.** Generation 2, Secure Boot with the UEFI CA template, no checkpoints, no shared folders, no GPU or device passthrough, no
nested virtualisation, and every integration service off. Enhanced Session has no per-VM off switch, and the host's is left alone: this
VM keeps the VMBus transport and has no xrdp, which by Microsoft's archived `linux-vm-tools` and the guides is what a Linux guest would
need changed. 8 processors. 6 GB of static memory on this 15.7 GB
laptop, with under 1 GB free during a build: it stays stopped when idle, one heavy job runs at a time, and a start that will not fit is
refused. Each choice has its reason beside the line that makes it.

**The image** is Ubuntu 24.04's generic cloud image, pinned to `release-20261017` by URL and SHA-256. The digest comes from Ubuntu's
`SHA256SUMS`, whose signature was checked against Ubuntu's image key, and the script checks it before each use. It takes its first-boot
settings from a NoCloud seed, an ISO labelled `cidata` on a DVD drive, holding one public key and the network settings, which is taken
out after the first boot. Canonical's Multipass does the same on generation 2 Hyper-V (read in its source), with Secure Boot off, so
Secure Boot on is unproven. It is QCOW2 and Hyper-V wants VHDX, so the script reads it itself: not with `qemu-img` from WSL, where the
jobs ran, nor with a tool downloaded and run as administrator. That reader has only read files written from the format's description.
Not the Azure VHD (its manifest carries `walinuxagent`), Quick Create's desktop image (used with Enhanced Session), or the server ISO
(unattended, it stops at "Continue with autoinstall? (yes|no)" on the console).

**What blocks what,** read on 1 November 2026, none of it yet seen working on this machine:

| Layer | What it covers | What it does not |
|-------|----------------|------------------|
| **Hyper-V firewall** | "Containers hosted by Windows, including WSL", by VM creator. `Get-NetFirewallHyperVVMCreator` here lists WSL alone | A classic VM. The script makes no such rule |
| **Windows Firewall** | A packet that arrives for this machine. Block rules beat allow rules, so two rules stop anything starting a connection in from the VM's adapter or range | Its rules are inbound and outbound, with none for forwarded traffic, and Microsoft's documents do not say they stop what WinNAT sends on. So the LAN does not rest on them |
| **Port ACL on the VM's adapter** (`Add-VMNetworkAdapterAcl`) | Below the guest's kernel and before NAT, so root in the guest cannot change it. Denies private, carrier NAT, link-local, loopback, multicast and reserved ranges and all of IPv6; longest match decides | It has no state, so the one address SSH's replies go to must be allowed, and only the firewall rules stand in front of that address. Microsoft's page is written for Windows Server: whether the switch enforces it on Windows 11 Pro is for Prove to show |
| **The adapter's bindings** | SMB, IPv6 and link-layer protocols, which an IP ACL never sees, are off on this one adapter | NetBIOS over TCP/IP stays with IPv4, behind the firewall rules |

A stateless ACL cannot tell a reply from a new connection, so `-StatefulSshAcl` (off by default) would add a stateful extended ACL
(`Add-VMNetworkAdapterExtendedAcl -Stateful`) for the host's SSH, which, if Windows 11 honours extended ACLs beside basic ones, would
let only SSH and its replies reach the host. It is off for the first proof, because mixing the two ACL kinds on a client Hyper-V is
undocumented and getting it wrong would block the guest's internet (Prove's control would catch that) or its SSH; turn it on once the
stateless boundary is proven, and read what Prove's `host-tcp` to `10.213.57.1` on a non-SSH port, and its `host-spoof` lines, then say.
Each Create makes it match its own switch: a Create without `-StatefulSshAcl` takes away one an earlier Create added, and Remove
takes it before the VM.

Also unknown until it runs: whether WinNAT agrees to a NAT beside WSL's and the Default Switch's, and whether it still forwards with the
rule on the adapter in place. Create would stop at the first, with Windows' reason, and Prove's outbound control would show the second.

**Starting and stopping.** Stopping needs no privilege while the guest cooperates: `-Action Stop` powers it off from inside, over SSH.
An ordinary account cannot read Hyper-V's state, so it reports only that the guest stopped answering SSH, and a guest that never
answered (stuck at a Secure Boot refusal, say) it reports as not stopped, with the administrator command to force it, rather than
calling it stopped. An administrator confirms against Hyper-V's own state and turns it off if it did not go. **Starting has no way
round.** Hyper-V has no per-VM right to start one
(`Grant-VMConnectAccess` gives the console and nothing more), and this account is neither an elevated administrator nor in Hyper-V
Administrators. None of these is made by the script. The choices:

1. The owner elevates for each start: `-Action Start`. Changes nothing, and a person sees each one. **Recommended while nothing uses
   this VM.**
2. A task the owner makes once, as administrator, running `Start-VM -Name qor-ci-vm` and nothing else, which this account may run and
   not edit. **Recommended when a job needs it, as a decision of its own.**
3. This account in Hyper-V Administrators: "complete and unrestricted access to all the features in Hyper-V", for everything that runs
   as this account, and a VM can be pointed at any disk. Not recommended.
4. Leave it running, holding its memory all the time. Not recommended.

**`-Action Prove`** runs from an ordinary terminal and refuses to run elevated. It attacks as `runner`, with no privilege, and then as
root. Each attempt prints what it tried and what happened:

- `AF_VSOCK`, which is how a Linux guest reaches Hyper-V sockets, to the 9P port that reached `C:\` from WSL, to every service registered
  for Hyper-V sockets on this machine, and to a list of well-known ports.
- This machine's addresses on SMB, RDP, WinRM and every port found listening, and every TCP port on its address on this switch.
- The LAN (the router, DNS and whatever this machine has seen nearby), WSL's addresses, and Windows' `127.0.0.1` by a raw frame.
- As root: SYNs from source ports 22 and 53, ping, IPv6 on the link, and mounts of the host's filesystems.
- What shows on the guest's mounts and devices, and every VMBus device the host offers, against a list of what should be there.

A failed attack is a pass. A TCP refusal is a failure, because it means a packet got there and something answered. A check that could
not run is a failure, never a pass. Outbound DNS and HTTPS must work, before and after the attacks, so that "everything refused" cannot
mean "the network is down". Its exit status is 0 only if every line of both levels passed, the two levels' lines are all there, and the
firewall rules read back as they should. **All of these mean something only in a real VM.** The check that this is one fails anywhere
else, including under WSL, and the script on Windows refuses a run on stand-ins.

**What Prove does not try.** A public address that leads back to this machine or its router, which is not a private range. Root in the
guest speaking VMBus directly with code in its own kernel. Bugs in the hypervisor, or in `ssh.exe`, which talks to the guest. A guest
that is already compromised can also lie: Prove's lines come from inside it, so it shows something about a fresh VM and nothing about
one that has run jobs. A job here would have the internet, on any port, so it could send out whatever it was given.

**Before Build can move here,** beyond the proof: Qt's answer and a kit, a way to start the VM, a runner, and a clean disk for each job,
which this does not do. 6 GB will not carry 8 compilers without swap, and the suites that draw need a display it lacks. The Windows job
needs its own answer.

#### The owner's first run

Use the current checkout after its pending work is safely checkpointed and `git status` is clean. Record the current
`git log -1` and the last script change (`git log -1 --format=%h -- tools/ci/isolated-vm.ps1`). On 4 November the script in the
current checkout is byte-identical to `c9513c6`, the commit with the second review's fixes. A later script change needs its own
review before elevation. The owner's separate Create go-ahead is still required.
The first run is likely to stop somewhere, because nothing here has met Hyper-V; each stop prints its reason, and most say to run the
same command again.

**Before elevating:** read the top of `tools/ci/isolated-vm.ps1` down to `param(`, and this section. Confirm the tree: `git status`
clean, `git log -1`. Pick a time with no build running, since it needs about 7 GB of memory free.

**Confirm its three folders are not there yet,** whatever the script would make of them. This should print `False` three times:

```powershell
Test-Path -LiteralPath X:\qor-ci-vm, "$env:ProgramData\qor-ci-vm", "$env:LOCALAPPDATA\qor-ci-vm"
```

If one prints `True`, nothing of a real Create is there yet, so it is a leftover or someone else's: look at it and remove it as below
before Create. On the morning of 2 November 2026 an `X:\qor-ci-vm` was there, left by a run of the script against its test stand-ins
(its disk and image were the suite's test files), which then had nothing to keep them inside the test's own folder; they refuse
anything outside it now. It held five items, all this account's: `.made-by-qor-ci-vm` (90 bytes), `disk\`, `disk\qor-ci-vm.vhdx`
(2,097,664 bytes, a 2 MB test disk), `image\`, and `image\ubuntu-24.04-server-cloudimg-amd64-20261017.img` (199,605 bytes, a test
image, not Ubuntu's). Create refuses a folder like that, because it is not owned by Administrators. The owner removed it that morning,
and on checking it was gone; the check above says whether anything has come back.

**To remove a leftover `X:\qor-ci-vm` that holds no VM,** in PowerShell 7, one command at a time, reading each answer before the next:

```powershell
Get-VM -Name qor-ci-vm -ErrorAction SilentlyContinue   # as administrator: nothing printed means there is no such VM
Get-ChildItem -LiteralPath X:\qor-ci-vm -Recurse -Force -Attributes ReparsePoint   # must print nothing: no link inside to follow
Get-ChildItem -LiteralPath X:\qor-ci-vm -Recurse -Force | Format-Table FullName, Length, LastWriteTime   # what will go
Remove-Item -LiteralPath X:\qor-ci-vm -Recurse -Force
```

If `Get-VM` prints a VM, stop: that is a real one, and `-Action Remove` is the way to take it and its folder. Leftovers that belong to
this account, as the stand-ins' did, an ordinary terminal can remove; one that belongs to Administrators needs an administrator's.
The same four steps, with the path changed, do for a leftover `%ProgramData%\qor-ci-vm` (it should hold at most `record.json`) or
`%LOCALAPPDATA%\qor-ci-vm` (at most `ssh\` and `prove-*.txt`).

**Create.** Open PowerShell 7 as administrator from the Start menu, then run `pwsh -NoProfile` (the script refuses an elevated run that
was not). Then:

```powershell
cd X:\REPOZ\QOR-OS
.\tools\ci\isolated-vm.ps1 -Action Create
```

Pass no `-Root`; a later `-Root` spelled even slightly differently is refused. Don't press Ctrl+C during the VM steps. Expect a download
of about 600 MB, a conversion of a minute or more, up to 15 minutes for the first boot, then the VM left running on 6 GB. It ends with
the line telling you to run Prove from an ordinary terminal.

**Prove.** Start WSL (so Prove has something to attack on that side), then, in an **ordinary** PowerShell 7 (Prove refuses to run
elevated):

```powershell
cd X:\REPOZ\QOR-OS
.\tools\ci\isolated-vm.ps1 -Action Prove; "exit status: $LASTEXITCODE"
```

It prints one line per attempt and ends with a per-level summary and either `Every line passed...` or `PROVE FAILED:` with reasons, then
writes `%LOCALAPPDATA%\qor-ci-vm\prove-<when>.txt`. It powers the VM off at the end.

**Send back:** all of Create's output (or its `STOPPED` block), and the `prove-...txt` file with the exit status. That file holds this
machine's addresses and port numbers, and no credential.

**If it stops halfway:**

- **Most steps:** run the same command again; it does what is left.
- **It says a folder or the record cannot be trusted:** it changed nothing. Look at what it names, and remove a leftover as above.
  `-Action Remove` stops the same way at a root it cannot trust; remove that by hand, then run Remove again for the key and record.
- **It stops waiting for SSH:** likely Secure Boot refusing the image. The VM keeps running; as administrator,
  `Stop-VM -Name qor-ci-vm -TurnOff -Force`, then `-Action Remove`. The script will not turn Secure Boot off; that is the owner's call.
- **An unmarked `qor-ci-vm` is left (a stop between making and marking):** run `-Action Create` again, which finishes it, or
  `-Action Remove`, which takes it when its files are under this script's root.
- **WSL or the Default Switch lose networking:** `-Action Remove`, then `wsl --shutdown`, then reboot.

**To undo everything, as administrator:** `pwsh -NoProfile`, then `.\tools\ci\isolated-vm.ps1 -Action Remove`.

## What the Build job needs on a runner

- **The commercial Qt 6.12 kit** (below).
- **Rust 1.91 or later** (`cargo`) for the QOR core, which `core/` builds from
  demiurge-chain at a pinned commit through Corrosion
  ([QOR-0003](decisions/QOR-0003-the-qor-core-at-a-pinned-commit.md)). The CI
  presets set `QOR_BUILD_CORE=ON`, so a runner without Rust fails at configure
  instead of skipping the core. The recorded workstation toolchains use Rust 1.98.1.
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

- the push or pull request changes `products/`, `tools/qor-media/`, `.qmllint.ini`,
  `docs/design/tokens.json`, `cmake/QorHardening.cmake`, `tools/dev/products.ps1`,
  `products.sh`, `products-tests.ps1`, `products-display.qml`, `products-report.py`,
  `secret-service.py`, `tools/ci/check_hardening.py`, `tools/ci/with_display.py`,
  `.github/qt-packages.json` or `.github/workflows/build.yml`. Windows and Linux
  use the same change-path filter;
- the run is for a tag;
- the run was started by hand.

Change detection stops on a failed Git fetch or diff. On Linux it reads the whole diff before matching and does not close
the producer's pipe early, so a large change list cannot turn a match into an "unchanged" decision. The step reports only
whether products were requested; it cannot claim they were built or tested. Regression checks execute each platform's
actual workflow script with a stand-in Git, including failures, a large diff, unchanged files, tags and manual runs.

A product whose suites fail is run once more (on Linux, its failed suites).
On a Windows desktop someone is using, a test window can lose the keyboard to
another application, or a synthetic click to the real pointer, wherever it rests,
and a genuine failure fails both attempts. The step summary says whether the
products ran. The Windows wrapper rejects both an exception and a nonzero child/native exit code; it clears a stale exit code
before each attempt. Its PowerShell regression tests use stand-in product scripts without opening Qt windows.

The Windows product scripts run CTest's registered suites through `products-tests.ps1`, rather than scanning for executables
in the build root. Qt's Windows setup puts executables there. The old scan accepted an empty list and could not execute
registered tests that are not executables. Suites run sequentially even if CTEST_PARALLEL_LEVEL is inherited. Old records are invalidated
before configuring/building and before testing; a failed cleanup stops the run. No registered suites, a whole-suite skip,
or missing/mismatched log/XML evidence fails the product. These paths are checked with real CTest and stand-in builds,
without proving Windows Qt runtime behavior. An empty executable scan is not evidence that suites ran; no such diagnosis is
asserted for earlier Windows runs without their logs.

Both platform jobs also run `all_qmllint` for each changed product, one at a time, and stop at the first native failure.
The root `.qmllint.ini` sets `MaxWarnings=0` without changing any category or severity, so the root and product CMake lint
commands fail on warnings. Previously, native qmllint could exit zero while printing warnings. Local commercial-Qt tests
verify clean input, warnings and syntax errors; extracted Windows/Linux workflow commands are tested with a failing native
stand-in. The 5 November Inspector batch in WSL retains 149 console messages/174 structured warnings, with four removed and
none added against the preceding Shape snapshot (153/178). All three native QQ commands exit 255 and the wrapper exits 1. The zero-warning
gate remains failed; the paused CI jobs did not run.

A separate native Windows check of the configured root `all_qmllint` target passes all seven modules with zero
warnings: Kit, Lumen style, Input, Services, Store, Apps and Shell. Fresh commands, 112 source/config hashes and
response files are verified. `MaxWarnings=0` and all diagnostic categories/severities are unchanged. Direct and
generated-tool-launcher controls both return 0 for clean input and raw Windows status 4294967295 for planted
warnings/syntax errors. The first control driver assumed the WSL status 255; its failed assertion is retained, and the
corrected comparison preserves the exact platform-native status. Products and the Linux-only compositor are outside
this Windows root result; the QQ and Linux metadata gates above remain failed.

#### What Linux runs

The product wrapper runs suites sequentially on first attempts and retries, overriding inherited CTest parallelism.
This is checked with real CTest and stand-in suites that fail if they overlap; it does not prove product runtime behavior.

Gameplay checks measure body movement against the existing game-clock helpers when rendering in software. The
physics/controller, trigger, impulse and Orb Run movement waits changed in that earlier snapshot use those helpers,
retaining their durations, conditions and tolerances. GPU waits retain their wall-clock behavior; UI opacity, music and pause controls keep their existing
waits. A virtual controller is removed even when its case returns early, so a failure cannot hold input in later cases.
Earlier clean and component baselines each failed the same 11 play cases; that corrected full-suite snapshot passed
53 cases. The 5 November Shape check runs full Scene (52 cases) and six physics functions (10 cases) per Release/sanitizer
build, zero failures/skips. Its group test uses existing physics helpers for landing, stability, contact and settling,
and the frame helper for frame-driven raising. The original raising wait failed with both pre-change and candidate Shape
sources; height and elapsed frame time agreed. GPU waits and all tolerances remain unchanged. This is functional evidence,
not native graphics, sound or controller proof.

The 13 November native ARM64 software recheck finds three contact/falling-trigger
cases still waiting on wall time: only 325–409 ms of physics passed before their
5–6-second deadlines. Orb Run's startup wait expires before its first frame.
Both render loops reproduce all four failures. A six-wait physics-clock proposal
passes the three contact cases in a separate overlay. The owner approved that
exact six-wait patch, now applied; its source bytes match the tested overlay.
This qualifies those cases in software and does not prove their real-time speed.
GPU timing and the Player's frame-rate floor
are unchanged. A separate first-frame probe advances Orb Run past startup but
still fails its later lantern-idle timer wait; it is not a complete fix. Logs,
source hashes and the patch are in
`build/development-2026-11-13/session-10/`.

The 14 November session-13 investigation still fails the original Orb startup
assertion. Its diagnostic sees no ticks at the deadline and the first eight at
10.071 seconds of waiting. Only 24 logic steps (0.4 seconds) pass before the later lantern
wait expires. A separate four-change proposal passes the complete Orb case:
three Qt Test results, no failures/skips, in 184.714 seconds. Software startup
first admits a scene tick within 120 seconds, then retains the existing
five-second assertion. The lantern, replacement-orb and growth checks use
three/four/three seconds on the logic clock, bounded by 120/150/120 seconds on
the wall. GPU branches retain the original waits; predicates, tolerances, music,
sound cleanup, input and pause controls are unchanged. Build and hardening pass.
The owner separately approved these exact four software changes on 14 November,
as required by `AGENTS.md` section 7. Their applied source is byte-identical to
the passing proposal. The acceptance rebuild and hardening pass; all 1,409 native
source hashes and the rebuilt executable match the tested proposal. The case
was not rerun for that identical rebuild. This accepts the selected software
functional case; real-time, hardware and full-product acceptance remain open.
The patch, original failure, diagnostics and pre-approval restoration evidence
remain under `build/development-2026-11-14/session-13/`; approval and integration
proof are in `session-14/` beside it. Other software exceptions retain their
existing scope and status.

The session-15 continuation runs complete, unfiltered QQ suites under the same
native coordinator and private software display. Gameplay passes 58 results;
lockdown 24, publishing 11, network confinement three, Store launch three,
Studio 29, workspace 241 and game builds 195. Asset-library checks pass 91,
with only their optional measurement skipped. The Studio creator journey passes.
The separate `first_journey` passes in 202.406 seconds, including Prism's Library,
rebuild and playing after the source project is removed. Its test launcher
records Prism's arguments; the real Player then runs them on its private display,
not inside a hardware-rendered Prism window. Its registered 300-second limit is
retained; ordinary suites use 5,400 seconds and Qt functions retain 1,800,000 ms.

The agent suite records 21 passed, one timing failure and one live-provider skip.
A separate diagnostic demonstrates the error log working but not reaching its
unchanged repetition threshold; its test changes were then restored. The basic
render-loop comparison also fails.
The Player's separate bundled HDR sky now uses pixel-identical precomputed
lighting, fixing both launch timeouts. Its full rerun records 13 passed, two
20-FPS-floor failures (0.5 and 18.9 FPS), and one unavailable-Landlock skip.
No software-floor acceptance option or assertion change is used. Runtime lint
still fails with exactly the same 53 warning lines. Build and hardening pass;
all 16 QQ programs meet the hardening checks. Full QQ acceptance remains open.

Final readback verifies all 1,412 native source files and 15 executables, including
the unchanged scene/world binaries whose earlier complete passes are retained.
Across the complete suites, 5,268 resource samples stay at or above 4,923 MiB.
The original lock is free and no test worker remains. Windows stays below the
3,072-MiB floor, so all-file hooks and local heavy checks remain incomplete.
`build/development-2026-11-14/session-15/` retains commands, environments, sources,
JUnit reports, failures, diagnostics, provenance and the final reconciliation.
This is native software evidence; it does not qualify hardware, the Q Pi image,
the other products or an isolated CI runner.

The 15 November Game interface increment reruns complete scene (87), gameplay
(58), Player and Store-launch (three) suites. Scene also passes all 87 offscreen
under ASan/UBSan with leak checking. The packaged-game creator journey passes
in 201.985 seconds under the registered 300-second limit. Player retains 13
passes, two FPS failures (0.5 and 18.9 against 20), and the Landlock skip.
All 24 Game lint findings are removed; the other 29 warning lines remain.
The first candidate was rejected after log review found transient script errors;
new coverage reproduces them and passes the corrected direct accessors. Fresh
corrected-suite logs contain no Game script errors. No existing assertion,
rendering quality, approved timing boundary, coordinator or isolation setting
changes. Receipts: `build/development-2026-11-15/session-01/`. The agent timing,
remaining lint, other-product, Windows, hardware and isolated-CI gates stay open.

The subsequent Behaviour increment passes six focused results on both the
original and changed runtime, then all 89 scene results normally and offscreen
under ASan/UBSan with leak checking. Its 11 lint findings are removed, leaving
18 unchanged warning lines and a failing lint gate. Existing scene assertions,
approved gameplay timing, coordinator and isolation remain intact. Complete
gameplay passes 58 results, Store launch three, and the packaged-game journey
passes in 201.377 seconds under its original 300-second limit. Agent retains
21 passes, its identical prior failure and one optional live-provider skip;
Player retains 13 passes, two FPS failures (0.6 and 19.5 against 20) and the
Landlock skip. Hardening passes all 16 QQ programs and the sanitizer program.
The separate ARQADE diagnostic gate and all-file Windows checks remain open;
neither was rerun for this increment. Receipts:
`build/development-2026-11-15/session-02/`.

The following parent-state and 2D runtime increments retain the approved
gameplay source and all pre-existing scene assertions. The latest complete
scene run passes 109 results normally and offscreen under ASan/UBSan with leak
checking; gameplay passes 58 and Store launch three. The packaged-game creator
journey passes in 201.365 seconds under its registered 300-second limit. Agent
retains its identical log-timing failure (21 pass, one fail, one optional
provider skip); Player retains its two 20-FPS-floor failures at 0.5 and 19.5
(13 pass, two fail, one Landlock skip). All 16 QQ programs and the sanitizer
program pass hardening; native builds emit no compiler/licence diagnostics.

Structured output corrects the earlier console-only lint counts: Runtime had
20 warnings despite printing six; Studio independently had 18. Fourteen runtime
source findings are removed with none added, leaving six Runtime and 18 Studio
warnings, and zero Player warnings. The six Runtime findings disappear with
the missing SpatialAudio dependency added in a temporary metadata copy;
complete Runtime lint then passes while invalid-property and invalid-type
controls still fail. The installed SDK and normal lint gate are unchanged.
No severity, source coverage, assertion or timing limit was relaxed.
All 2,305 resource samples stay at or above 5,353 MiB available against the
3,072-MiB floor. Receipts: `build/development-2026-11-15/session-03/` and
`session-04/`; the LF-format patch export is in `session-04-export/`.
These are native software results, not hosted Qt CI or hardware acceptance.
The subsequent repository reconciliation runs all configured pre-commit hooks
and the tracked-file credential scan successfully on a separate complete Pi
checkout with the hash-pinned development requirements, the original native
coordinator and the unchanged memory floor. Windows remains below its floor;
this completes the repository-hook check without claiming a Windows product
build. Exact tree and publication receipts are in
`build/development-2026-11-15/session-05/`.

The next increment integrates the SpatialAudio dependency correction through
QQ's generated QML tooling import path, using only a build-local metadata
copy. The normal runtime console and JSON lint targets pass with zero warnings;
Player has none, while Studio's same 18 findings still fail the aggregate gate.
Six helper tests and the real valid/invalid-property/type controls pass, as do
all 109 native scene results normally and offscreen under ASan/UBSan with leak
checking. Eight audio results pass; the creator journey passes in 199.848
seconds under its original 300-second limit. All eight audited executables
are byte-identical to the preceding validation, whose complete gameplay,
Agent, Player and Store results remain scoped to that earlier run. All 862
samples stay at or above 6,588 MiB against the unchanged 3,072-MiB floor.
The installed SDK and application import environment remain unchanged. Receipts:
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

On 15 November, the qualified Windows continuation at source `5d1359e` passes
all 22 selected core suites with one expected desktop-entry case skip; nine
GPU-labelled suites are excluded. Core and QQ builds pass, with the core's
known Rust dependency warning retained. Hardening accepts 33/15 programs and
aggregate QML lint reports zero warnings, confirmed by all three QQ structured
module reports. Six metadata helper tests and six
inert AgentPanel results pass under the original Windows coordinator and
continuous 3,072-MiB guard. Full Windows product runtime suites remain unrun:
offscreen rendering/private profiles do not isolate their audio devices or
Windows Credential Manager. These local checks do not qualify hosted runners
or clear skipped Qt jobs/core audit and failed Release readiness. Receipts:
`build/development-2026-11-15/session-08/`.

The 16 November client-only graphics trace at source `489ddfe` narrows first
Play to repeated V3D buffer waits inside Qt's `QRhi::endFrame`. In the targeted
ioctl observer, 417 `WAIT_BO` calls occupy 491.838 of the 510.661-ms
after-render-to-swap interval, with 0.969 ms of render-thread CPU. Buffer handle
291 accounts for 383.545 ms and is present in captured submission 946. This
identifies a dominant buffer reservation wait, not a unique fence sequence,
shader or Studio pass. Matching Mesa/Qt debug symbols are absent; raw offsets
are preserved. No production change follows from this evidence alone.

The same original Agent cases report 3 passed, 1 failed and 0 skipped in all
three executions. Reference/observer fail `times > 5` with 3; `strace` fails
with 1 and measurably perturbs scheduling. Error capture/coalescing and the
wall-clock case pass; later notice/cursor assertions are unreached. Actual
V3D/OpenGL ES 3.1/Mesa/Qt identities and private compositor are checked;
client caches start empty. There are no shader/pipeline errors. The independent
llvmpipe failure remains open, and no new image equivalence is claimed.

Original sources, binaries, screenshot, assertions and suite limits are intact.
All 215 diagnostic/setup resource samples exceed 3,072 MiB, minimum 6,736 MiB.
One observer compilation refusal is preserved and corrected before its test
ran. No application Release, sanitizer, broader product suite or isolated CI
acceptance was run anew. Prior 97 unexplained image outliers, deterministic
logging coverage and an early ES initializer remain unresolved/proposed.
Next: resolve the captured Mesa/Qt call sites with matching debug information
to identify the GL operation and pass that reuse the dominant buffer. Keep
PR #10 draft; skipped hosted Qt/core-audit jobs are not passes and required
Release readiness remains unmet. Receipts:
`build/development-2026-11-16/session-03/`; session-02 remains sealed.

The earlier accepted test rebuild reproduces its passing proposal's binary hash, with
all 1,404 snapshot source hashes verified. Hardening reads 16 QQ programs and
passes all of them; native QQ `all_qmllint` fails with 103 warnings. The Store
Player case still exceeds its internal 60-second deadline with either render
loop. The network-confinement suite passes three results without skips. The
lockdown suite does not complete within a separate 300-second diagnostic budget;
that bounded probe is not full-suite acceptance or a new standard timeout.

The session-11 follow-up separates `Game.playingScene` from Qt's inherited
viewport `scene`. The new regression fails on the original implementation and
passes after the rename; the full scene suite passes 57 results without failures
or skips. Runtime QML lint removes the property-override finding, retaining the
other 102 warnings. The Store Player still exceeds 60 seconds. A separate
300-second affected-play probe passes model startup and two contact cases before
its aggregate budget expires during the prefab case; that batch is incomplete.
Early isolated compilation logs also contain Qt license-client registration
diagnostics: their zero compiler-warning count does not establish a clean build.
Subsequent checks compile using the host's existing Qt license setup and keep
test execution in private namespaces. Raw logs and scope corrections are in
`build/development-2026-11-13/session-11/`.

The sanitizer build verifies actual instrumented compiler commands and linked
ASan/UBSan libraries. Its first scene run passes all 57 assertions but exits 1
with 3,744 leaked bytes. Giving standalone viewport tests hidden window owners
and draining queued destruction removes the viewport allocations. The complete
offscreen scene run then passes 57 results and exits 0 with leak detection left
enabled. Wayland still exits 1 with one 96-byte allocation in its hardware
integration client; the same allocation appears in a numeric-only case that
creates no viewport. No leak suppression or disabled leak checking was added.

The ARQADE caller build completes compilation but retains two diagnostics: the
configuration's GuiPrivate version warning and a QML generator warning about
the namespaced server enum. Its generated metadata still exports
`ARQADE/ArqadeServer 1.0`. Functional checks against that binary do not clear its
warning gate; these diagnostics were not suppressed.

The final full QQ build exits 0 without compiler or license-client diagnostics.
Its current scene suite passes all 57 results. The four affected playback cases
(imported-model startup and the three contact/trigger cases) complete in
305.333 seconds: six Qt results including setup/cleanup, no failures or skips.
Their assertions and internal durations are unchanged; the preceding 300-second
diagnostic probe was too short for that combined batch.

The four ARQADE caller functions also pass six Qt results in 255.637 seconds,
including opening/leaving a game and match creation/opponent-join polling.
A context warning during return to home remains in the log. Hardening passes
16 QQ Release programs, the ARQADE test program and the sanitizer test program.
The independent final audit verifies 1,404 source hashes, a current Release
cache and the original lock available. Some outer-wrapper receipts retain exit
1 because their temporary drain helper counted exited adopted children awaiting
their parent's reap as active. That helper was corrected; command results and
the independent audit are recorded separately, with the raw failures preserved.

The 14 November Shape continuation verifies a 102-warning baseline. Binding its
private factories to their owner removes 13 findings but exposes two parent-view
type findings, reproducing the reason an earlier experiment was excluded. That
intermediate candidate passes seven physics functions (11 Qt results) and all
57 scene results. The final change uses Group/Prefab's existing structural
parent-property pattern for playing/view state and adds a parent-state regression.
It passes all 58 scene results, plus contact reporting, grouped motion and all
three 2D plane-lock rows: seven physics results including setup/cleanup, no
failures or skips, in 385.566 seconds. The new regression emits no QML warnings.

The final full QQ build has zero compiler/license-client diagnostics, and
hardening passes 16 programs. Runtime lint reports 88 warnings: 14 baseline
findings removed, none added, with its gate still returning 1. Remaining findings
are 82 missing properties, three imports and three unresolved types. The actual
CMake configuration is `RelWithDebInfo` (optimized with debug information);
earlier prose calling the `q-pi-qq-release/qq` cache "Release" is not evidence of
a CMake `Release` configuration. No existing assertion, duration or tolerance
changed. Commands, source comparisons, intermediate results and raw failures
remain in `build/development-2026-11-14/session-01/`.

The final sanitizer build also has zero compiler/license-client diagnostics.
Its complete scene suite passes 58 results offscreen and exits 0, with leak
detection enabled and no suppressions. The sanitizer test program passes
hardening. The compiler commands for Shape's generated code and the scene test
carry address/undefined-behavior instrumentation, and both sanitizer libraries
are linked. The final audit verifies the native snapshot's 1,404 source hashes,
a current QQ cache and the original lock available; both changed local code
files match their tested native bytes. This does not clear the separately
recorded Wayland leak or the remaining full-product and isolated-CI gates.

**The variable was recorded unset and the Qt-job pause remains in force.** Once
a runner is qualified and its product step is enabled, the harness runs each
product's registered CTest suites hidden: **in software when fenced or without a
render node, and expecting GPU rendering when a render node is available.**
That selection is not itself proof that the observed renderer or hardware passed.

- **Fenced from the GPU.** Under WSL the script hides `/dev/dxg` in a namespace of
  its own, and Mesa draws with llvmpipe. A machine with no door to a GPU at all
  draws in software the same way. ([Why the fence](#why-linux-is-fenced), and
  [BUILDING.md](BUILDING.md#on-linux).)
- **Out of sight.** A display of the script's own, a compositor on Qt's offscreen
  platform; a session bus and an in-memory keyring of its own; no sound device.
- **Within the machine's limits.** One job at a time and 8 compilers, as for the
  rest of the job. The tests are held to half the processors and three quarters of
  WSL's memory where systemd's user manager runs. The script prints the commit, the
  kernel, the processors, the limits and the display it ran with.

**A pass in software says nothing of a real GPU, real sound or a real controller.** It
says what the games do, by their own clocks, and that their windows behave.
Nothing here replaces playing them on the Linux machine.

**What draws is checked, not assumed.** From the machine's devices the script decides what should draw, and always sets
`QOR_EXPECTED_RENDERER` to it, whatever the environment said. The suites whose checks differ in software (`tst_play`,
`tst_studio`, `tst_agent`, `tst_player` and `games`) print what drew them, and stop if it disagrees, whichever way round. So
`GALLIUM_DRIVER=llvmpipe`, `LIBGL_ALWAYS_SOFTWARE=1` or a render node Mesa cannot use is a failure on a machine with a GPU, not
an easier run, and a machine with no render node that draws with some other renderer fails too. The other suites check the same
either way, and neither print nor stop. Run by hand, or by `products.ps1`, nothing is expected and a suite takes its strict path.
Only someone who sets both by hand, outside the script, is eased on a machine with a GPU, and the suite says what drew it.

**The log and the step's summary name every case failed or skipped, with why**, in each product's `what its suites said`
(`tools/dev/products-report.py`). It reads two files of CTest's: its log, the only place a passing suite's output is kept, and its
record (`ctest -T Test`), the only place that says how a suite ended. A Qt Test suite with no totals says `STOPPED`. A suite that
could not start, or skipped itself by its `SKIP_RETURN_CODE`, says `NOT RUN`, never a pass. A suite that is not Qt Test has no counts,
and reads passed, failed or stopped by the record. On the first attempt and every retry, success needs a nonempty log and record
that name the same suites, with every suite passed and every Qt Test suite's totals agreeing. A whole suite that skipped itself
keeps the step from passing; individual skipped cases stay named and counted separately. Before configuring or building, and before
each attempt, the script invalidates old test records. If that cannot be done, it stops: a failed build or an interrupted test
must not leave earlier passing evidence looking current. At the end the script names any suite that passed only on a later attempt,
having checked that the retry ran it. Nothing is excluded. These are the cases that do not simply pass:

| Case | In software, under WSL | Why |
|------|------------------------|-----|
| QQ's `tst_player`: `thePlaygroundPlaysAndItsStartIsMeasured` | **Fails, and so fails the step**, until the owner says otherwise | Its floor of 20 frames a second, a real performance check, reads about 6 here. It stays a check on Windows. See below |
| QQ's `tst_agent`: `theRealProviderIsReachedOverTls` | Skipped, as everywhere | Needs `QQ_LIVE_ENDPOINT=1`, to reach the real provider |
| QOR Media's `playback`: `video_decodes_through_ffmpeg` | Skipped | Windows' sample video is not on this machine |
| QOR Media's `playback`: `it_docks_inside_the_launchers_window_and_lifts_out_to_full_screen` | Skipped | Wayland has no window handles to dock by; Prism places the window ([QOR-0010](decisions/QOR-0010-on-wayland-prism-places-the-products-windows.md)) |

**The QQ Player's floor** asks for 20 frames a second of the bundled playground. Software
cannot give that, and it says nothing of the game, so the floor is neither lowered nor skipped:
the case fails and says what it read. Whether Linux CI stays red on it or lets it stand is the
owner's decision. `tools/dev/products.sh --accept-software-floor` is the second. It reports the failure and lets it stand only when
all of this holds: the script expected software from the machine's devices, and the Player says `llvmpipe` drew it; that check in that
case is the product's only failure; `tst_player` ended normally with that one failure as its exit status (CTest's record says so; its
log says only "Test Failed.", even of a crash); and the Player read **at least 1 frame a second** (`STILL_DRAWING_FPS`). Any other
suite that failed, stopped or could not start refuses it, Qt Test or not. One that skipped itself does not, and still reads `NOT RUN`.
`build.yml` does not pass the flag, so the step is red on this alone.

**Whether the Player kept drawing is a check of its own, on every machine,** neither part of the floor nor eased with it. An average
cannot say it: a Player that draws four frames and stops reads 1.3 over its three measured seconds. So `tst_player` fails if the last
frame came more than a second before those three seconds ended (`lastFrameAgoMs` in `--measure`'s report).

**How long.** On this laptop, with the builds incremental, about 77 minutes, QQ and ARQADE measured on 1 November 2026 with
nothing else holding the machine's one heavy job: QQ 22 (`tst_play` 7, `tst_workspace` 4), ARQADE 55 (`games`, which is
`tst_games`, 52), and QOR Media under one, from an earlier run. So a cold build and a second go at a failed `games` fit
the step's limit of 180 minutes, and the job has 270 with the variable set.
What the products built then has its exploit mitigations read back, as on Windows.

**On a GPU these suites check what they did at `a3a2fd4`, with these differences.** Added, on every machine: `tst_agent`'s
`aLongWaitLastsWhatItWasAskedFor`; `tst_player`'s check that the Player kept drawing; and the renderer's check above, where the script
expects something. Changed, and no easier: the gamepad's jump in `tst_play` waits 500 ms on the wall, where its 50 short waits took at
least that, so it has less time to reach the same height; each of Orbital's ten locks looks the satellite up and checks it is there,
where only the first did. What follows is eased **in software alone**: where the script expects it and the renderer says `llvmpipe`
(`products/qq/tests/drawnin.h`). The owner has not been asked to accept these, and they do not count as a pass of what they leave out.

**What software alone no longer checks:**

- **That the world keeps up with real time, in the cases that were failing for it.** On a GPU the walks, the Studio's and the agent's
  falls and Orb Run's first orb in `tst_play`, `tst_studio` and `tst_agent` wait on the wall's clock, so that a walk of over 3 metres in a
  second fails a world below two thirds of real time. In software these wait instead on the clock the thing goes by, with the same numbers,
  and a limit on the wall's time that stops a hung game. A game has three clocks, which agree at 30 frames a second or more: a body moves
  by the physics world's (at most 1/30 s a frame), the Player's turn, fall and jump speeds go by the frame clock (at most 1/20 s), and
  logic by the scene's ticks (at most 8/60 s). Falling-shape and group-body waits now use those helpers too. UI opacity,
  music and pause controls retain their separate wall-clock waits; these do not measure body movement.
- **That frames pay logic's ticks at the right rate, against what moves bodies.** On a GPU `tst_play`'s
  `logicMakesAndRemovesEntitiesOnItsOwnClock` is paid by frames, so logic running more than about 1.6 times ahead of bodies fails it: the
  drops have not fallen 2 metres when logic's clock says they should have. In software logic's clock runs up to four times ahead of the physics
  world's, so there it pays its ticks from the physics world's time, and `root.advance(frameTime * 3)` in `Scene.qml` passes it. Darkveil's two
  pilot cases are paid by frames on a GPU too, as a person's game is. In software the pilot pays them itself, one to each decision, because
  deciding once in eight ticks plays a rougher game: of two such runs of the first case, one went home to the healer before it met the Elder,
  which fails it, and one passed.
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
  a suite's limit 90, not 5 and 15. What follows that limit, less 20 seconds, goes from 280 seconds to 1780: Darkveil's two pilot
  cases and Bellwick's two. Rift Survivor's hull has it too, not 2 minutes, since it takes 13½ to break. The drops case has 60 and 30
  seconds, not 8 and 4.
- **Anything of a real GPU, real sound or a real controller**, as above.

**The GPU's side of each of these has run here only by forcing it on in software**, since no GPU may be used here. Forced, the
walks, the falls, the drops, Orb Run's first orb, the lock's sound and the hull's two minutes each failed, as they should
where software cannot keep up. The live check of a kill's sound and Darkveil's frame-paid first case passed. The other sounds and
Darkveil's second case were not run forced. Since Build is paused and Linux here is fenced, a stated run of `tools/dev/products.ps1` on
Windows is what must still confirm them, with the `Qt6::GuiPrivate` that `tst_studio`, `tst_play`, `tst_agent` and `qq-player` now link.

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

### Option A: self-hosted runners (recorded setup)

A self-hosted runner can reuse its installed Qt kit and incremental build cache.
Its credential/resource isolation and actual GPU access still need verification;
neither the labels nor the presence of a kit supplies that evidence.

**Recorded workstation layout (October 2026; paused 1 November).** The two
runner registrations used these folders and labels. This is not evidence that
either runner is currently online or approved to run:

| Runner | Where | Labels |
|--------|-------|--------|
| `<COMPUTERNAME>-qtos` | `%USERPROFILE%\ghr\qtos`, work folder `w` | `self-hosted, Windows, X64, qt, msvc, gpu` |
| `<COMPUTERNAME>-qtos-linux` | WSL2 Ubuntu, `~/ghr/qtos-linux` | `self-hosted, Linux, X64, qt, gcc, wsl, gpu` |

- Both were configured as the signed-in user; this did not provide isolation.
- An incremental Windows build with its tests was observed at about 2½ minutes then.
  The first build also compiled the QOR core's Rust dependencies. These timings
  are historical, not a budget for the current tree or an ARM64 builder.

### Where the runners are registered

A runner serves either one repository or the organisation. **Both registrations
were moved to `QOR-MATRIX/qor-os` on 29 October 2026.** From 27 October they
served the organisation, in its Default runner group, which does not allow
public repositories. On 29 October GitHub stopped giving jobs to the
organisation's runners (below); moved to the repository, they took the waiting
jobs within 15 seconds. The frozen QTOS needs none. The public demiurge-chain
must never run on this workstation: a self-hosted runner on a public
repository runs code from anyone's pull request.

**This repository must stay private for the same reason.** It was public from its
creation on 27 October until 1 November 2026, against its own documents, until an
outside review noticed. No pull request was ever opened on it. Being public may also be
why GitHub stopped giving it the organisation's runners, whose group refuses public
repositories; nobody has tested that. If it is ever made public, the runners
come off it first.

`tools/ci/runners.ps1` is the historical registration/start helper. Do not run the
following setup commands while the isolation pause remains in force; a replacement
and its activation need the owner's approval. Its implementation:

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

Recorded repository variables (*Settings → Secrets and variables → Actions → Variables*,
or `gh variable set <NAME> --body <VALUE> --repo QOR-MATRIX/qor-os`). The
review found that the private repository could not read its Free organisation's
variables, so it used repository variables. Names below match the workflow;
values and unset states are recorded configuration, not a fresh API observation:

| Variable | Value | Effect |
|----------|-------|--------|
| `QOR_WINDOWS_RUNNER` | `["self-hosted","windows","qt"]` | Windows job runs on the workstation |
| `QOR_WINDOWS_QT_DIR` | `C:/Qt/6.12.0/msvc2022_64` | Use the installed kit; skip the installer |
| `QOR_WINDOWS_RUNNER_ISOLATED` | `true` (unset) | Lets the job run on a kit's runner, once that runner cannot reach the owner's keys or credentials ([why](#what-ci-trusts)) |
| `QOR_LINUX_RUNNER_ISOLATED` | `true` (unset) | The same for Linux |
| `QOR_RUN_GPU_TESTS` | `true` (optional) | Also run GPU-labelled suites, and the products on Windows (opens brief test windows on the runner's desktop) |
| `QOR_RUN_LINUX_PRODUCTS` | `true` (optional; recorded unset) | Also run the products on a hidden Linux display; the harness expects software when fenced or without a render node, GPU otherwise ([what it runs](#what-linux-runs)). Job timeout 270 minutes, Products step 180; this flag does not supply the separate native product GPU readiness result |
| `QOR_LINUX_RUNNER` | `["self-hosted","linux","qt"]` | Linux job runs in WSL on the workstation |
| `QOR_LINUX_QT_DIR` | `/home/<user>/Qt/6.12.0/gcc_64` | Use the installed kit |

The variables took their `QOR_` names on 27 October 2026
([QOR-0009](decisions/QOR-0009-the-name-is-qor-os.md)); the frozen QTOS keeps its own `QTOS_` set. Until they are set,
a job is skipped; once they are set, jobs wait in the queue whenever the runner
is not running.

**Both runners were stopped on 1 November 2026, at the owner's word, and do not start at sign-in.** They stay registered, as
`DESKTOP-Q38RMM0-qtos` and `DESKTOP-Q38RMM0-qtos-linux`, with their folders untouched. Their two Startup entries were moved to
`%USERPROFILE%\ghr\startup-held\`, so putting them back is moving them back. Neither goes back until the place it runs in has been
shown to hold. Stopping the Linux one also let go of what had kept `Ubuntu` running, and `/tmp` there is in memory, so a
distribution left with no client loses what is in `/tmp` once WSL stops it.

**Starting the runners at sign-in**, when they run again, is a Startup-folder entry for each, which
the owner adds once. Assistants do not write Startup entries.

```powershell
Set-Content "$([Environment]::GetFolderPath('Startup'))\qor-github-runner.cmd" `
  "@start `"QOR OS Windows runner`" /min cmd /c `"%USERPROFILE%\ghr\qtos\run.cmd`""
Set-Content "$([Environment]::GetFolderPath('Startup'))\qor-github-runner-linux.cmd" `
  "@start `"QOR OS Linux runner`" /min wsl.exe -d Ubuntu --cd ~ --exec /home/$($env:USERNAME.ToLower())/ghr/qtos-linux/run.sh"
```

Once a runner is isolated and started again, if CI does not run, first check that it is online:
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

The historical workstation Linux runner used WSL2 Ubuntu and the commercial kit in `~/Qt`
([BUILDING.md](BUILDING.md#linux-on-wsl2-windows-workstation)) and Rust from
rustup in `~/.cargo` (recorded 1.98.1). This setup is paused and fails the current
isolation requirement; it is not a qualified replacement runner or Pi builder.

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

Without either Qt source, the availability job succeeds and explains the missing
prerequisite, but the platform jobs do not run. Build's final readiness job still
runs unless cancelled and fails missing required outcomes. Availability success
therefore does not make the Build workflow green or establish release readiness;
branch/merge enforcement is a separate repository setting.

## Branch protection

[`.github/rulesets/main.json`](../.github/rulesets/main.json) prepares the
default-branch ruleset. **It is not installed.** The 13 November API audit again
refused both rulesets and branch protection because the private repository's plan
does not support them. Do not make the repository public to work around this.
The prepared rules require:

- no deletion and no force-push
- pull requests required, with stale approvals dismissed and conversations resolved
- Lint, history secret scanning and Release readiness to pass
- no configured administrator bypass

Squash and rebase remain available; merge commits are also allowed to preserve
the upstream fixture history required by [UPSTREAM.md](UPSTREAM.md). A blanket
linear-history rule would conflict with those imports. The template retains zero
required approving reviews; CODEOWNERS names the responsible owner but does not
by itself enforce review. GitHub reported no CODEOWNERS syntax errors on
13 November. When the plan supports rulesets, review and apply with
`python tools/github/bootstrap.py --ruleset`; this also synchronises repository
settings, labels and milestones. A refused request now exits nonzero. Read back
the installed ruleset before claiming enforcement. Its readiness requirement
will deliberately block merges while required Qt jobs are paused.

GitHub documents [ruleset availability](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-rulesets/about-rulesets)
by plan; the API response is the observation for this repository.

## Repository settings and recovery

On 13 November 2026, repository API readback confirms private visibility, disabled
forking and automatic merge, deletion of merged branches, and a read-only default
workflow token that cannot approve pull requests. Dependabot vulnerability alerts
and automated security fixes are enabled. Both registered runners are offline;
no runner service or isolation variable was changed.

[`.github/actions-policy.json`](../.github/actions-policy.json) is the reviewed
Actions policy. It selects six explicit public action repositories, disables broad
GitHub-owned/verified-publisher allowances and requires full commit pins. The `@*`
allowlist permits reviewed Dependabot pin updates; the separate full-SHA rule
rejects mutable version tags. Both current `main` and the integration branch use
those same six action repositories. These settings were applied and read back
successfully; this is configuration evidence, not a new workflow run.

The subsequent hosted [Lint run 43461849136](https://github.com/QOR-MATRIX/qor-os/actions/runs/43461849136)
passes at `3ad895449ad3205e38ea9ca88865e3a4df8427a1`. Its Ubuntu run checks all
three workflows and 16 pinned action uses, then completes 815 tooling tests
(691 passed, 124 platform-specific skips), catalogue validation, headers, tracked
file secret scanning, game rules, formatting and JSON validation. This validates
the contribution controls on GitHub; it does not run the paused Qt jobs or establish
release readiness. The later documentation-only receipt update does not change
that tested implementation.

```powershell
python tools/github/check_actions_policy.py               # offline; no mutation
python tools/github/bootstrap.py --actions-only           # authenticated admin; changes only Actions policy
gh api repos/QOR-MATRIX/qor-os/actions/permissions
gh api repos/QOR-MATRIX/qor-os/actions/permissions/selected-actions
gh api repos/QOR-MATRIX/qor-os/actions/permissions/workflow
```

The offline check runs in the local hook and hosted Lint. It rejects unreviewed
actions, incomplete pins, write-token grants, persisted checkout credentials,
privileged follow-up events, duplicate keys and YAML aliases. Local actions,
container actions and reusable workflows are unsupported until separately
reviewed. It does not inspect external action implementations, their transitive
dependencies or prove a runner boundary. PyYAML is already pinned and hashed in
`requirements-dev.txt` and is now an explicit input in `requirements-dev.in`;
no dependency version or artifact hash changed. The two external pre-commit
hook repositories also use full commit pins at their existing reviewed versions.

Before applying a change, save the three readbacks above under a new ignored
`build/` evidence directory and review the policy against all active branches.
The setup tool validates local workflows first, writes the three endpoints in
order and verifies their returned values. It exits nonzero on rejection or drift;
earlier writes can remain applied. Inspect those same endpoints before retrying.
For an accidentally omitted action, correct the reviewed allowlist and reapply.
For an incorrect value, restore only that value from the saved, reviewed snapshot
through the matching GitHub settings page or API endpoint, then read it back.
Do not respond to an error by granting all actions, enabling write tokens or
unpausing a runner. This session's before/after receipts are in
`build/development-2026-11-13/session-03/`.

The implementation follows GitHub's [Actions security guidance](https://docs.github.com/en/actions/reference/security/secure-use)
and [permissions API](https://docs.github.com/en/rest/actions/permissions).

## Dependabot

`.github/dependabot.yml` updates GitHub Actions weekly (grouped) and the
pinned Python tooling monthly.

## Local parity

Everything CI runs can be run locally:

```powershell
python tools/github/check_actions_policy.py
python tools/tokens/generate_tokens.py --check
python -m unittest discover -s tests/tools -v
python tools/store/build_store.py check
python tools/ci/check_headers.py
.\.venv\Scripts\pre-commit run --all-files
.\tools\dev\build.ps1 -Preset ci-windows -Test
.\tools\dev\build.ps1 -Preset ci-windows -Target all_qmllint
.\tools\dev\products.ps1
```

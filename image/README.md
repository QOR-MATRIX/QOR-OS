# image — the QOR OS image (Phase 3)

> **Status:** a private ISO boots automatically through the branded UEFI menu into the live OS.
> Private stock installation, media-free first run and two installed reboots pass. The base is **Fedora Atomic with bootc**
> ([QOR-0004](../docs/decisions/QOR-0004-the-base-image-is-fedora-atomic.md)).
> The checked branding filesystem layer builds into a data-only container. The first-run
> app runs in the installed private VM and its completion persists across two reboots.
> Full device and release acceptance remain open.
> The first image is Phase 3 work
> ([ROADMAP](../docs/ROADMAP.md#phase-3--boots-and-plays-pc-games)).
>
> QTOS's Yocto skeleton (`meta-qtos`, `kas`) was retired by QOR-0004. It is in
> the history before commit "Retire the Yocto skeleton for Fedora Atomic"
> (`git log -- image/meta-qtos`).

## Private disposable VM lab

[QOR-0024](../docs/decisions/QOR-0024-disposable-vm-installation-lab.md) defines a
limited Fedora bootc/QEMU development path. Its normal installer startup,
cancellation, corrupt-payload refusal, stock installation and persisted first run
now pass in actual guests. The full device session,
authentication, native drivers, updates, recovery and rollback remain unfinished.

The selected base is Fedora 44 bootc **amd64**, manifest
`sha256:0cd45c0431f553a1f40a4717fb53f59f24a7f93f0d2962d581ac9b9b8860008e`.
`vm/tooling-input.json` and `vm/tooling-rpms.txt` pin the resolved local tooling
image, filesystem layers and RPM manifest hash. The private saved tooling archive,
its checksum, all staged input hashes and runtime/compiler receipts are retained.
The bootstrap still uses mutable signed RPM repositories. Snapshot restoration,
timestamp normalization and repeated-build determinism remain unproved.
These tools publish no image.

The Windows memory preflight is `python tools/image/vm_resources.py`. Run it
before the Linux runtime build. The image driver checks before expensive phases;
the VM launcher checks before ISO hashing and again before launch. Below 3 GiB of
available host RAM they report **not started** and return failure. The guard only
reads memory availability; it changes no setting, stops no process and uses no
retry loop. Owner-authorized temporary service stops and cache/page reclamation
in the recorded local run are separate orchestration, with restoration receipts.

`stage_vm_runtime.py` builds the requested non-simulator targets with one worker,
checks source snapshots, warnings, kit identity and native build results, then
stages required Qt libraries, QML imports and plugins with their original notices.
No Qt kit, tools, account or credentials enter the guest. This is private development
packaging, with distribution approval still separate. Installation candidates require
a clean revision and runtime built from that same revision. Older compiler evidence
keeps its original identity and is never relabelled as a fresh build.

`vm/Probe.Containerfile` prepares the kernel and initramfs. The default ISO remains
**live display probe; installation unavailable**. `build_vm_probe.py --installation`
adds a separate deployment image and validated offline OCI directory outside the
live SquashFS. Native dracut errors reject acceptance even if its command returns
zero. Both image branches run `bootc container lint --fatal-warnings`; wrappers
also reject emitted warnings. At clean `feed626`, each branch passes 14 checks with
zero warnings; one alternate-root check is skipped in running-root mode and is
reported as not run. The 1,437-file runtime has that same clean compiler revision;
the target-image audit resolves all 63 bundled ELF programs, libraries and plugins.
These limited builds exclude the compositor and clear no full-project gate.

ISO compression uses one CPU, 768 MiB and no network or host devices. Its only
added capability is `DAC_READ_SEARCH`, to preserve mode-000 image files. The
builder's writable layer is disposable; SELinux labels and extended attributes
are packed from the image's policy contexts. Docker-generated identity/DNS data
is replaced with neutral image data. GRUB supplies the static UEFI menu; Plymouth
and Qt use the existing official startup assets. BIOS, USB hybrid boot and Secure
Boot are unsupported in this lab.

The clean `feed626671949399c42b8ff32aa9697ff6fc6bfa` installation ISO is
**3,601,666,048 bytes**, SHA-256
`b23e86336461e59284385d56efb38c99fef80878ac42d376885d00704ee2936b`.
Its 1,437 runtime files have that same clean compiler revision. The host launcher
has separate source-checksum provenance in every launch record.
It boots automatically into the real Qt installer without guest overrides.
Port-ready udev events start the restricted agent and assigned frontend; the
existing device dependencies remain, and service files use mode `0644`.
The display helper selects the connected device at fixed VirtIO PCI `00:01.0`,
not a presumed `card0`. A reserved guest console prevents a login console from
stopping the frontend. Earlier GRUB, display, image-lint and device-wait failures
are preserved in their original private attempt records.

The earlier `fa17674` image's first actual stock installation exits 1: its generic
`unconfined_service_t` context cannot enter bootc's required installer context.
The entire 16-GiB raw disk remains blank. The suggested enforcement-disable
fallback was not used. A separate diagnostic establishes the required pair:
`install_t` process context and an executable carrying `install_exec_t`.
With enforcement enabled, the labelled entrypoint passes bootc's unknown-label
capability test, and the actual Qt frontend shows the assigned target in that
context. A context-only shell probe is rejected at execution, so both parts matter.
The recipe now pairs the unit context with a local file-context rule for
`/usr/bin/qor-install` and checks the label lookup during image building. A small
native SquashFS fixture verifies that the local rule is honored when packing.
No new SELinux permission or permissive fallback is added. The actual packed
`feed626` executable and unit pass inspection, and normal stock installation now
exits 0 with a complete 2,051-byte log, checked by its byte count and checksum.

An earlier deployment run stops when QEMU cannot create another thread. Its
default I/O pool can grow to 64 workers ([QEMU reference](https://www.qemu.org/docs/master/interop/qemu-storage-daemon-qmp-ref.html)), competing with other tasks inside the
64-task container. The host launcher now bounds its main-loop pool at four;
the container limit, disk guard, memory minimum and other confinement stay unchanged.
Native QEMU reports that limit, and the successful installation records a peak of
17 tasks with no task-limit or out-of-memory events. The source image is unchanged.
All 62 portable cases pass on Windows and Linux, plus eight Linux process/file cases.

`vmguard.py create` requires a **new** directory under this checkout's `build/`,
a verified private firmware template, exact source revision and ISO checksum.
It records a regular raw file without backing, file IDs, capacity, parent ancestry,
VM UUID and unique serial. `vmrun.py` pins those Windows files and their ancestry
for the VM lifetime, rechecks ISO identity/hash after pinning, and passes held
regular-file descriptors to QEMU. The restricted, network-free TCG container gets
private OVMF variables, no host block device, EFI, KVM, GPU or Docker socket.
Installed phases have no CD drive. Host boot and security settings stay unchanged.

The guest validates sealed firmware metadata **before enumerating disks**. Before
every stock bootc transaction it must match UUID, exact serial/capacity, VirtIO
driver and PCI `00:04.0` ancestry, node/kernel identity and inactive use. Additional,
busy, changed or ambiguous disks are refused. Explicit selection, an initially
unchecked erase acknowledgement and complete serial confirmation precede writing.
A device name or merely detecting a VM never authorises erasure.

The OCI directory and receipt must be on the read-only ISO at
`/run/initramfs/live/qor/install-source` and `install-source.json`. One manifest,
amd64/Linux identity, installed-profile/source labels, every blob size/hash and
exact file inventory are required. External URLs, multiple or unknown images,
missing/extra/linked files and duplicate metadata keys are refused. Mount type,
read-only flags, filesystem identity, descriptors and unlinked ancestry are checked.
There is no custom partitioner, automatic transaction retry or estimated percentage.
A failed installation request disables retry and preserves the disk for inspection.

Actual normal review/Cancel on `feed626` preserves its full 16-GiB disk hash.
A separate fault-injection ISO changes one OCI config-blob byte while retaining
its manifest and receipt. The real UI refuses it, keeps retry disabled, permits
Cancel and shuts down cleanly; that complete test disk hash is also unchanged.
The corrupt bytes and unchanged metadata were extracted and checked from the
actual fault ISO. This derivative is never accepted as an installation candidate.

Only private virtual serial ports are added at PCI `00:05.0`. The guest agent
permits synchronization, ping, policy inspection and shutdown/reboot; actual normal
VM probes show those four operations enabled and 39 others disabled. Execution,
file access and clock changes remain unavailable. The client checks the QMP VM
UUID before opening the separate agent channel. No endpoint is published.
A write-only evidence port exports source/image/boot IDs, known public completion
status/hash and a fixed bounded offline installer log. It accepts no arbitrary
paths or commands. Unknown state bytes are preserved and never exported; Linux
walks state parents through no-follow directory descriptors.

The installed profile uses a locked local `qortest` account. Its limited welcome
states that Prism, Library, games and QOR ID authentication are unavailable.
Encryption remains disabled until a separate encrypted install/unlock/reboot is
proved. The evidence checker requires complete successful installer logs,
source/image/VM identity, media-free installed boots, successful welcome exits and
distinct boot IDs. It checks persisted completion before the next welcome process.
Synthetic fixtures establish none of those actual installation stages.

Private acceptance now records five actual VM phases with native exit 0: normal
review/Cancel, stock installation and three media-free installed boots. The real
welcome starts pending, completes through its three visible pages and saves the
expected record. Both subsequent boots find that same completed record before
welcome runs, with successful process results and four distinct boot IDs including
installation. That sequence uses the restricted agent.

A fresh 8 November installation also exits 0 with a complete 2,051-byte log, but
the actual frontend reboot button returns an error. Its disk, screenshot and native
failure are preserved under `build/vm-ui-reboot-2026-11-08/attempt-2/`.
A separate diagnostic boot shows the packaged reboot command can run in the existing
SELinux domain. Another real-kernel diagnostic reproduces an inventory defect:
requesting an unused loop node makes the original collector count storage that has
no backing file. The local correction requires kernel loop ancestry, major number 7,
zero capacity, no active use and absent loop attributes before excluding a node.
Its guest preview accepts the unconfigured node, refuses attached writable backing
and accepts detachment. Diagnostics are separate from normal packaged-image proof;
a rebuilt image and successful normal UI reboot remain pending.

RAM preflights and a bounded reclamation-helper interruption are retained as failed
orchestration records. The accepted continuation uses the same installed disk,
without a reset. Pause/restore receipts retain the failures and service-identity
changes: containers were replaced during an idle interval, then current identities
were revalidated. Pause batches exclude PostgreSQL and preserve volumes. Final
lookups of removed identities fail and remain recorded; all current services are
running or restarting and the database is healthy. Final checked proof is
`v12b-installation-proof/full-sequence-result.json`. Actual screenshots, immutable
input records, native failures and the exact resume history live in
`build/vm-install-2026-11-06/`. These ignored artifacts are private development
evidence, not release assets.

## Intended full device image

An OCI container image built `FROM` a Fedora bootc base and booted as a whole
operating system. It contains:

| Layer | Contents |
|-------|----------|
| Base | Fedora Atomic: kernel, systemd, PipeWire, NetworkManager, BlueZ, UPower, fwupd, Mesa, libinput |
| Graphics | NVIDIA's open kernel modules for every RTX card, built for the image's kernel and signed with the QOR OS Secure Boot key; Mesa for Intel and AMD |
| Games | Steam and its 32-bit runtime, gamescope |
| QOR OS | Prism and the shell, QOR.Kit and Lumen, the service daemons, QQ, ARQADE, QOR Media, the vault service from the QOR core, the `qor` platform theme and Lumen QStyle |
| Session | A systemd unit that starts Prism on the first virtual terminal through a greeter (below) |

Updates are whole images: `bootc upgrade` stages the next one, and a boot that
fails returns to the last good image. Apps update separately.

## Phase 3 checklist

- [ ] Complete `image/Containerfile` from a Fedora bootc base, with NVIDIA's open
      modules and Mesa; built and pushed by CI as an OCI image
- [ ] Secure Boot: a QOR OS key that signs the kernel modules, and enrolment
      on first boot
- [ ] The QOR OS layer installed from a CI build against the commercial Qt kit
      (Qt's distribution terms: [QOR-0008](../docs/decisions/QOR-0008-before-a-public-release.md))
- [ ] Prism on its own KMS backend as the session ([QOR-0005](../docs/decisions/QOR-0005-every-display-at-its-own-refresh.md)),
      started by greetd with a Prism greeter, with a measured transition from the OS splash
- [ ] Steam and Proton inside gamescope as a Wayland client of Prism
- [ ] Installable disk images and a live installer ISO using a version-pinned, supported image builder
- [ ] Boots the reference laptop with its panel at 360 Hz and a television at
      60 Hz, and rolls back from a deliberately broken update

## The session, as QTOS sketched it

The retired Yocto layer started Prism directly on tty1. The image keeps the
same environment until Prism's own backend replaces Qt's KMS integration:

```ini
[Service]
User=qor
PAMName=login
TTYPath=/dev/tty1
Environment=QT_QPA_PLATFORM=eglfs
Environment=QT_QPA_EGLFS_INTEGRATION=eglfs_kms
Environment=QT_QPA_EGLFS_KMS_ATOMIC=1
Environment=QT_QPA_PLATFORMTHEME=qor
Environment=XDG_SESSION_TYPE=wayland
ExecStart=/usr/bin/prism
```

**The image is built with `-DQOR_BUILD_SIMULATOR=OFF`**, so Prism has no simulated
services to fall back on, no `qor-shell`, and no unlock that accepts any password:
it runs on the real daemons (Phase 3) and stays locked until there is a sign-in to
check against ([QOR-0018](../docs/decisions/QOR-0018-the-security-and-quality-bar.md)).

## Branding the startup stages

| Stage | Control and implementation |
|-------|----------------------------|
| Firmware | The manufacturer's logo remains under firmware control. The test VM uses unchanged OVMF code and private variable files; no host firmware/EFI is touched. |
| Boot menu | The corrected private VM ISO automatically boots through static GRUB EFI, a QOR theme with the correct logo size and a readable diagnostics entry. No bootloader animation is assumed. |
| OS loading | `branding/qor-os.plymouth` selects Plymouth's script plugin, with the existing QorMark on Lumen's dark background. One 267 ms fade, then a still mark; no fake progress, repeating animation or end-animation delay. |
| Session | Prism's existing `Desktop` and `DisplayDesktop` instantiate `StartupSplash`: the same image/background, released on the first swapped frame, Escape dismissal, reduced-motion handling and a 1.5-second watchdog. Hotplug after readiness skips the intro. |

The approved source is [`kit/QorMark.qml`](../kit/QorMark.qml), documented in
[Lumen](../docs/design/LUMEN.md). `kit/branding/` contains its reviewed 1024-square rendering,
licence sidecar and source/recipe/hash manifest. It also renders on Qt's software backend,
where the original shader-based mark cannot. `tools/branding/check_assets.py` validates
source hashes, complete bounded PNG pixels, aspect ratio and dark background before shell builds.
`qor-brand-assets` renders the existing mark with commercial Qt; it is a build-host tool,
never installed on a device. Generate into a **new empty directory**, inspect the result,
then review/copy its PNG and manifest into `kit/branding/` together. Under WSL use the
documented GPU fence and a private shader-capable software display; plain Qt Quick software
cannot render the source shaders, and the generator refuses that blank result.

```text
cmake --build --preset <existing-preset> --target qor-brand-assets qor_brand_assets_qmllint
<build-folder>/tools/branding/qor-brand-assets --output <new-empty-directory>
python tools/branding/check_assets.py
cmake --build --preset <existing-preset> --target qor_boot_payload
python tools/branding/stage_image.py --output build/boot-branding-payload --verify
python -m unittest discover -s tests/tools -p test_boot_branding.py -v
```

Executable locations differ on Windows. Normal builds validate the checked-in rendering;
they do not need to regenerate it or acquire a second Qt kit. A stale rendering fails closed.

`qor_boot_payload` produces **seven files** beneath `build/boot-branding-payload`:
the theme, script, mark, provenance and licence in `/usr/share/plymouth/themes/qor-os/`,
the theme selection at `/etc/plymouth/plymouthd.conf`, and an inclusion rule at
`/usr/lib/dracut/dracut.conf.d/50-qor-branding.conf`. It refuses paths outside this checkout's
`build/`, links, incomplete outputs and different existing content. Repeating it verifies
matching files; it never replaces unfinished work. Changed content needs a fresh staging directory.

The device Containerfile copies those paths into the **image filesystem**, but requires
a separately supplied approved device base. That base must contain Fedora's Plymouth script
and text plugins, build that image's initramfs with the
Plymouth dracut module, then verify both the root filesystem and extracted initramfs against
this payload. Do not run `plymouth-set-default-theme`, `dracut`, or an installation command
on the workstation. No hook here changes host boot arguments or adds `quiet`/`rhgb`.
The selected image base and bootloader must supply their actual initramfs commands;
the full device recipe remains incomplete; the separate private VM recipe above
does produce a bootable ISO. Missing plugins/assets,
failed initramfs generation or absent inclusion must fail that image build.

Plymouth retains Escape/text mode and keyboard handling. Messages and password prompts
replace the mark; only bullet counts reach the password drawing, with no file/log output.
The ordinary text interface remains the recovery path for full output, long messages,
questions, missing graphics and a boot that stalls. The theme does not own or delay boot
completion, change service deadlines, or capture input. Its API was checked against the
[maintained Plymouth source](https://github.com/deepin-community/plymouth/blob/master/src/plugins/splash/script/script-lib-plymouth.c);
that review is **not a Plymouth runtime pass**.

Only UEFI Secure Boot is planned by the current architecture. No legacy BIOS/CSM or
Secure Boot signing claim is added by artwork. A payload test/asset preview is not a bootable
image or boot sequence. The corrected private VM ISO boots automatically and shows
the mark during OS loading, with a failed Qt display service; it does not prove
Plymouth animation or installation.
Windows Docker's Linux engine builds both the data-only layer and the separate VM probe.
Actual firmware/menu/OS splash, graphics fallback, prompts, multiple displays,
initramfs inclusion, rollback and the splash-to-greeter handoff still need real image/VM and
hardware evidence. The CI VM Create/Prove and guest Qt licensing gates remain unchanged.

## Installation capability map

Refreshed on 7 November 2026 using the `f132dc1` installer ISO and subsequent source fixes,
retaining the earlier starting-state column. Application deployment through
CMake or a Windows package is not installation of an operating system.

| Stage | Before this change | Implemented here | Still required |
|---|---|---|---|
| Image generation | Plan and seven-file boot-branding payload | 25-file data layer and a private installer ISO from clean `f132dc1`, with validated deployment/OCI payload | Port-ready startup rebuild, frozen-input reconstruction/repeated-build proof and full device payload |
| USB boot/live session | No image or live session | Automatic UEFI optical-media boot and Linux startup; new installer startup hits a serial-device discovery race | Automatic GUI startup in the rebuilt candidate; actual USB/hardware/mode tests |
| Installer framework | Calamares mentioned in planning only | Calamares theme stays inactive; Qt/stock-bootc prototype builds; the real installer renders after a diagnostic late start | Normal automatic startup and backend transaction |
| Choices/disk/encryption | No implementation | Recorded raw-file guard and real guest target review; diagnostic Cancel preserves the full disk hash; encryption disabled | Normal-startup cancellation, actual install and representative failure checks |
| Deployment/progress | No implementation | Checked read-only OCI source, bounded exit/log gate, full payload conversion/validation and warning-fatal image checks pass | Actual guarded transaction, progress and interrupted-install recovery |
| Bootloader | No installation backend | Static GRUB EFI is in the ISO; stock bootc is selected for target-disk installation | Actual bootloader deployment and media-free UEFI boot; no BIOS/Secure Boot claim |
| Completion/reboot | No installer | Process-exit-gated completion tested with inert fixtures; diagnostic live VM shuts down normally | Genuine install success, media detached, installed first run and second reboot proving persistence |
| First run | No app | Real `qor-setup`: three-page introduction, keyboard controls, checked state and once-per-session hook; live diagnostic VM proves interruption/restart and three-Return completion in RAM | Packaged automatic startup, real authentication/session service, installed-image launch/reboot proof, screen-reader and hardware testing |

Calamares is the candidate because its native Qt interface can be branded without
rewriting partitioning. Its KPMcore partition module is **not integrated here**, and
there is no verified Calamares-to-bootc adapter. `settings.conf.in` has unresolved
`@VERIFIED_DISK_REVIEW_MODULE@` and `@VERIFIED_BOOTC_DEPLOYMENT_SEQUENCE@` slots.
The checked device layer refuses active installer settings and live-media activation
for the full device recipe. The separate VM prototype exposes an install button
only after target review and exact serial/erase acknowledgement. It is not in the
current probe, and its real guest transaction is not yet exercised; synthetic
tests cannot authorise claims of a working installer.

The required final disk review must show physical model, stable identifier, capacity,
partitions/data affected and encryption action. Re-enumeration or a changed target must
invalidate confirmation. Do not activate the candidate by merely substituting module
names. Disk removal, read/write/space failures, invalid credentials, interrupted install,
boot failure and recovery need disposable-disk tests. No destructive choice or reboot is
preselected. The original branding retains upstream attribution in
[`installer/ATTRIBUTION.md`](installer/ATTRIBUTION.md); it contains no vendored installer code.

The first-run app is an introduction, not account provisioning, a hardware validator or
an installer. It writes only `{"schema":1,"completed":true}` under its own per-user state
location. Invalid, truncated, oversized, linked or changed records are preserved with a
plain-text error; completion is blocked until resolved. Closing leaves welcome pending.
No preferences, credentials, personal data, disk choices or sign-in results are recorded.
The Windows regression first failed both completion and keyboard completion. Publication
now uses `QTemporaryFile::rename()` directly, preserving atomic no-overwrite behavior:
wrapping that still-owned temporary path in another writer failed on Windows. Qt's
[temporary-file implementation](https://raw.githubusercontent.com/qt/qtbase/v6.12.0/src/corelib/io/qtemporaryfile.cpp)
retains the handle across `close()` and closes it during its own rename. Tests verify the
record persists, repeat completion is harmless, and foreign records remain unchanged.
This does not claim power-loss durability or resolve the separate Windows Store rename issue.
Prism excludes simulation and `/usr/lib/qor-os/live-session`, and requests welcome only
after `Desktop.locked` becomes false. The actual device authentication backend is still
Phase 3 work. Launch failure produces one generic diagnostic, without a retry loop.
Prism passes a snapshot of Lumen's reduced-motion and increased-contrast preferences.
The welcome applies those public scalar hints to its own Theme; it changes no system
setting. Manual launches and unknown motion hints default to no motion. Preference
changes while welcome is already open are not streamed into it yet.

## Building the installation data layer

These commands use an **existing** toolchain and Docker engine. They do not install
software, download a base, start a VM/container, or change host boot/disks. Choose a new
output folder when inputs change; assembly never overwrites a different existing tree.

```text
cmake --build --preset <existing-preset> --target qor-setup tst_setup qor_setup_qmllint
cmake --build --preset <existing-preset> --target qor_image_payload
python tools/image/assemble.py verify --output build/image-payload
python -m unittest discover -s tests/tools -p test_install_image.py -v
python -m unittest discover -s tests/tools -p test_install_archive.py -v
docker build --network=none --pull=false -f image/Payload.Containerfile -t qor-os/install-payload:development build/image-payload
docker save --output build/install-payload.tar qor-os/install-payload:development
python tools/image/verify_archive.py build/install-payload.tar
```

The saved-image checker reads bounded archives without extracting or running them. It
compares every payload byte to current source and rejects missing, altered, duplicate,
linked or unexpected files. **FROM scratch + COPY produces data, not a Linux OS.** The
first-run QML source in this layer is audit data; the actual app is compiled/installed by
CMake into the separately supplied device base, with its Qt runtime.

Only after that base exists and is approved locally, the device-layer command is:

```text
python tools/image/assemble.py device-build --output build/image-payload --base <approved-existing-reference>@sha256:<digest>
```

It requires a locally present immutable reference, disables network/pull during build,
then runs `check-image.py` inside the build. That read-only check requires Prism, bootc,
`qor-session.service`, QOR identity and a non-development `qor-setup --build-capabilities`.
It is a structural guard, **not service, boot, installation or recovery proof**. No such
base was supplied or built in this session. The existing device CMake cache excludes
simulation but also excludes the QOR core; it cannot establish a complete device build.

## Evidence and next prerequisites

Local Qt tests exercise actual first-run QML, keyboard completion, close-without-complete,
small/HD/UHD layouts, malformed/changed/linked records and once-after-unlock launch/error
handling. Private app captures show this app; Calamares has not been run. Image tests
and an inspected saved Docker layer demonstrate packaging only. Exact session results,
failures and ignored evidence paths are recorded in `OWNER.md` and the local handoff.

No disposable-disk install, reboot into an installed system, real first login, recovery,
rollback, encrypted install or USB boot has run. UEFI is intended; no storage or firmware
mode is certified. WSL/software rendering is not native hardware evidence. Disposable
QEMU installation work is now authorised and live boot tests have run using fresh
regular-file disks and private firmware variables. The separate CI isolation VM
Create/Prove and guest Qt-kit licensing gates remain unchanged; they are not a
prerequisite for this confined runtime-only QEMU lab. Ubuntu connectivity and its
existing commercial kit are available again; fresh installer/sanitizer builds are
pending. Next: prove the display
selection in the actual startup path, package the guarded offline deployment image,
then install, detach media, complete first run and reboot a second time. No runner,
host boot/EFI settings or physical disks have been changed.

Primary framework contracts reviewed: [Calamares](https://github.com/calamares/calamares),
[partition options](https://github.com/calamares/calamares/blob/calamares/src/modules/partition/partition.conf),
[bootc installation](https://bootc.dev/bootc/bootc-install.html) and
[generic bootc ISOs](https://osbuild.org/docs/developer-guide/projects/image-builder/advanced/bootc/isos/).
Those documents are design inputs, not runtime passes. The older Anaconda ISO path is
being replaced upstream; no legacy builder invocation is presented as a verified command.

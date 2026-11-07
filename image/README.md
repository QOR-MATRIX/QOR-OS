# image — the QOR OS image (Phase 3)

> **Status:** a private ISO boots automatically through the branded UEFI menu into the live OS.
> Normal installer startup still needs a rebuilt port-ready activation fix; installation is not yet proved. The base is **Fedora Atomic with bootc**
> ([QOR-0004](../docs/decisions/QOR-0004-the-base-image-is-fedora-atomic.md)).
> The checked branding filesystem layer builds into a data-only container. The first-run
> app builds and runs. A separate private VM lab is being developed below; its boot,
> installation and second-reboot acceptance checks remain incomplete.
> The first image is Phase 3 work
> ([ROADMAP](../docs/ROADMAP.md#phase-3--boots-and-plays-pc-games)).
>
> QTOS's Yocto skeleton (`meta-qtos`, `kas`) was retired by QOR-0004. It is in
> the history before commit "Retire the Yocto skeleton for Fedora Atomic"
> (`git log -- image/meta-qtos`).

## Private disposable VM lab

[QOR-0024](../docs/decisions/QOR-0024-disposable-vm-installation-lab.md) adds an
explicitly limited development path, separate from the full device recipe and
inactive Calamares candidate. The first inspected source was `9d6c7c9` on
6 November 2026. The lab does not yet demonstrate installation. The full device
session, authentication, native drivers, updates and rollback remain unfinished.

The selected development base is Fedora 44 bootc **amd64**, manifest
`sha256:0cd45c0431f553a1f40a4717fb53f59f24a7f93f0d2962d581ac9b9b8860008e`.
`vm/Tooling.bootstrap.Containerfile` describes dependency acquisition. Its signed
RPM repositories are mutable: bootstrap is **not** a reproducible release build.
The lab driver enforces `vm/tooling-input.json` and `vm/tooling-rpms.txt`, pinning
the resolved local tooling image, root filesystem layers and RPM manifest hash,
and records every staged input hash. Preserve that tooling image, its RPM record,
runtime record and exact source revision. The exact v8 tooling image is also
preserved in a private saved-image archive; its checksum is in the input record.
Restoration and deterministic output still need testing, together with a
locked package source/snapshot, timestamp normalization and repeated-build proof.
No image is published by these tools.

The Windows lab preflight is `python tools/image/vm_resources.py`. Run it before
the Linux runtime build as well. The image driver rechecks before expensive phases;
the VM launcher checks before ISO hashing and again before launch. Below 3 GiB of
available host RAM they report **not started** and return failure. They read only
memory availability, never change host settings, stop other processes or wait in
a retry loop. This guard follows an observed low-memory composition stop; it is
not a hardware-readiness check.

`vm/Probe.Containerfile` prepares a live filesystem and its own kernel/initramfs.
`tools/image/build_vm_probe.py` composes a regular-file UEFI El Torito ISO with
GRUB, a SquashFS live root, the existing startup assets and the existing first-run
runtime. The default remains **display probe; installation unavailable**.
The new opt-in installation candidate builds a separate deployment image and
packages its content-addressed OCI directory on the read-only ISO. This source
connection awaits a real installation run. GRUB is static; Plymouth animation
and the transition to a framebuffer application require actual boot evidence.
BIOS, USB hybrid boot and Secure Boot are unsupported by this probe.

The first ISO (`4f3ecebc802da583fe056b12ecd5837542263ee607d23c01c51709a193a1538b`)
actually reached the branded live login console after manual GRUB recovery.
Its clean boot failed because an early prefix change hid embedded modules; the
logo was oversized, and the live Qt app failed on a missing PulseAudio client
library and unavailable legacy framebuffer. Source fixes keep GRUB's embedded
prefix, derive a bounded 112-pixel official mark with provenance, include the
missing runtime library and use Qt's software DRM mode. The corrected ISO from clean
recipe revision `c8ccc9b05b486bb20c03be2a73fa414ce2c81a0e` is **1,932,019,712 bytes**, SHA-256
`a3d3ea7518b57053ffb2c2199a51719b5e8d86222033a3331814db2719efaa2c`.
It actually boots automatically through the correctly sized branded menu and
shows the official mark during OS loading. Escape exposes readable diagnostics.
The first-run app still aborts: Qt defaults to absent `/dev/dri/card0`, while this
VM's VirtIO display is `card1`. The new `vm_display.py` selects the connected
display at the launcher's fixed VirtIO PCI slot and writes a private KMS
configuration. That helper now works in a temporary diagnostic boot, including
the real PCI parent and its VirtIO child. Reserving guest tty2 also prevents an
automatic login console from stopping the app. The live welcome renders,
remains pending after interruption, and completes with three Return presses.
Its public state record and successful exit were checked in RAM. The packaged
automatic startup still needs a new image and actual boot proof.
Neither the brief Plymouth fade nor installed-system first run is proved by
these captures. The target-image audit now
checks all 62 bundled ELF files, including dynamically loaded QML plugins, and
fails on missing libraries even when `ldd` returns zero.

ISO compression runs with one CPU, a 768-MiB memory limit and no network or host
devices. The builder has only `DAC_READ_SEARCH` to preserve mode-000 system files
from the prepared image; its writable layer is disposable. SELinux contexts and
extended attributes are retained. It removes Docker's generated detector and
uses neutral image hostname/hosts/DNS data. The first whole-root BuildKit copy
exhausted available Windows memory and was cancelled; compression no longer
duplicates that root. Initramfs errors fail acceptance even if dracut exits zero.

`stage_vm_runtime.py` first builds the requested non-simulator targets with one
worker and checks unchanged source snapshots, warnings, kit identity and build
exit status before staging the Qt runtime
import closure and plugins. It installs no Qt kit, tools, account or credentials
in the VM. It is private-test packaging, not distribution approval. Original
runtime notices stay intact. `QOR_BUILD_VM_INSTALLER=ON` is opt-in, Linux-only
and incompatible with simulator builds. Its Qt frontend and guarded Python
backend are under development, not an exercised installation path. Encryption
is disabled: no separate encrypted install/unlock/reboot has been proved.
The installed profile defines a locked local `qortest` account and starts
welcome as that user on a reserved guest console. It is a private installation
test, not a login/authentication service. Its limited introduction explicitly
states that Prism, Library, games and QOR ID authentication are unavailable.

The probe requires `--runtime-record`: all runtime files must match that receipt.
An older compiler revision is recorded separately from the image recipe revision;
it is never relabelled as a fresh build. The current diagnostic binary is from
`9d6c7c9`. Ubuntu WSL previously failed with `Wsl/Service/0x8007274c`; after the
disposable VM shut down, a fresh connectivity check passed and the existing
commercial Qt kit was verified. No distro or host service was restarted. A new
installer-only Linux build at `9e2e7b9` subsequently passed with zero warnings,
module lint and hardening for two programs. It excludes the unused compositor
and clears no full-project gate. The expanded welcome and Linux sanitizer checks
are pending.

`vmguard.py create` requires a **new** directory under this checkout's `build/`,
an extracted private firmware-variable template, exact source revision and ISO
checksum. It creates a regular raw file without backing and records file IDs,
capacity, parent ancestry, UUID and unique serial. `vmrun.py` pins Windows
ancestry/files for the VM lifetime, rechecks ISO identity/checksum after pinning and uses a restricted,
network-free QEMU TCG container. QEMU receives open regular-file descriptors,
private OVMF variables and no host block devices, EFI, KVM or Docker socket.
The installed phase has no CD drive. No host boot or security setting is changed.

The guest reads sealed firmware metadata **before enumerating disks**. Immediately
before each stock bootc transaction it must match UUID, exact serial/capacity,
VirtIO driver and PCI `00:04.0` ancestry, node/kernel identity and inactive use.
Additional, busy, changed or ambiguous disks are refused. A device name or merely
detecting a VM never authorises erasure. Explicit selection, unchecked erase
acknowledgement and complete serial confirmation precede a whole-disk install.
The actual guest collector matched the new 16-GiB VirtIO disk and sealed UUID;
unloading its guest-only firmware-metadata module caused refusal, and reloading
restored the exact match. This is a real read-only guard check, not an install.
An offline source checksum and real backend exit status are also required. The
OCI directory and receipt must reside on the read-only ISO at
`/run/initramfs/live/qor/install-source` and `install-source.json`. A single
manifest, amd64/Linux identity, installed-profile/source labels, all referenced
blob sizes/checksums and exact file inventory are required. External URLs,
unknown/multiple images, missing/extra/linked files and duplicate metadata keys
are refused. Mount type, read-only flags,
file descriptors, filesystem identity and unlinked ancestry are checked; a held
descriptor alone would not prevent in-place modification on a writable overlay. No
automatic transaction retry, custom partitioner or estimated progress is used.

The optional build command adds `--installation` to
`tools/image/build_vm_probe.py` with the same explicit runtime, branding, output
and pinned tooling arguments. It requires a fresh non-simulator installer runtime
from the exact clean source revision, builds the deployment target, converts its
saved image with network-free Skopeo, and grafts the validated payload outside
the live SquashFS. It produces `qor-os-vm-install.iso` only after those gates.
Offline conversion and complete payload validation pass at `f132dc1`; actual VM
installation remains unproved. On 7 November its private installer ISO was built:
3,601,666,048 bytes, with its exact checksum in the private build record below.
The first deployment build was rejected when dracut reported a missing root-home
target despite exiting zero. The common stage now creates that target before
either initramfs build; the failed attempt and its outer failure status are retained.
The next attempt passed initramfs generation and OCI conversion/validation, but
stock bootc reported four warnings: non-empty boot/runtime directories, package
logs and undeclared mutable directories. Most came from the tooling bootstrap;
the retained dracut log was new in this recipe. The recipe now removes generated
caches/logs and declares the required runtime directories. Both image branches
use `bootc container lint --fatal-warnings`; the wrapper also rejects emitted
lint warnings even with a zero command exit. The corrected images pass 14 checks
with zero warnings. The one alternate-root lint is skipped in running-root mode and is
reported as not run, never added to the pass count.

The new ISO boots automatically through its installer menu into Linux, but its
installer and guest agent lost their initial device-wait jobs before the virtual
serial ports became active. A separate guest-only diagnostic boot confirmed the
correct ports and PCI ancestry. Starting the services after readiness displayed
the actual installer, reported the exact assigned target and source image, and
exposed only the four allowed guest-agent operations. Review and Cancel left the
complete 16-GiB disk hash unchanged. This is diagnostic UI/cancellation evidence,
not automatic startup or an installation. The recipe now uses per-port
`SYSTEMD_WANTS` activation with the existing device guards, keeps the generic
agent masked, and copies service units with mode `0644`. A corrected normal boot
still needs a fresh image.

The evidence checker requires source/image/VM identity, complete checked installer logs,
media-free installed phases, successful welcome process exits and distinct boot
identifiers. It checks completion before welcome on the second installed boot.
Its synthetic negative controls do not establish a VM installation.

Only private virtual serial ports are added at PCI `00:05.0`. The stock guest
agent permits synchronization, ping, policy inspection and shutdown/reboot.
Its actual 10.2.2 process refuses execution, file access and clock changes;
the container probe does not prove a VM reboot. The client validates the QMP
VM identifier before opening the agent channel. No endpoint is published.
A separate write-only evidence port emits source/image/boot identifiers, known
public welcome-state status/hash and the fixed bounded offline installer log.
It accepts no arbitrary paths or commands. Unknown state bytes are preserved and
never exported; Linux walks state parents with no-follow directory descriptors.

Acceptance still requires a clean committed source and checksum-bound image,
VM configuration, target identity shown before erasure, real menu/splash/live/
installer screenshots, cancellation and failure evidence, successful deployment,
media detached, installed boot, first-run completion and a **second reboot**
showing persisted completion. Synthetic fixtures and asset previews do not prove
those stages. Private attempt logs and the exact resume point currently live in
`build/vm-install-2026-11-06/`; they are ignored build evidence, not release assets.

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

# image — the QOR OS image (Phase 3)

> **Status:** no bootable OS image or working installer. The selected base is **Fedora Atomic with bootc**
> ([QOR-0004](../docs/decisions/QOR-0004-the-base-image-is-fedora-atomic.md)).
> The checked branding filesystem layer builds into a data-only container. The first-run
> app builds and runs; the device base, live environment and installation backend are missing.
> The first image is Phase 3 work
> ([ROADMAP](../docs/ROADMAP.md#phase-3--boots-and-plays-pc-games)).
>
> QTOS's Yocto skeleton (`meta-qtos`, `kas`) was retired by QOR-0004. It is in
> the history before commit "Retire the Yocto skeleton for Fedora Atomic"
> (`git log -- image/meta-qtos`).

## What the image is

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
| Firmware | The manufacturer's UEFI logo stays under firmware control. No firmware/EFI changes are made here. |
| Boot menu | No bootloader is packaged yet. Preserve recovery entries, kernel editing and readable diagnostics when choosing it; no bootloader animation is assumed. |
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
there is no claimed bootable image recipe until those exist. Missing plugins/assets,
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
image or boot sequence: no Plymouth/bootc runtime, bootable image or proven VM was available in
this session. Windows Docker's Linux engine is available and builds the data-only layer.
Actual firmware/menu/OS splash, graphics fallback, prompts, multiple displays,
initramfs inclusion, rollback and the splash-to-greeter handoff still need real image/VM and
hardware evidence. The CI VM Create/Prove and guest Qt licensing gates remain unchanged.

## Installation capability map

Inspected on 5 November 2026, starting at `dad071d`. Application deployment through
CMake or a Windows package is not installation of an operating system.

| Stage | Before this change | Implemented here | Still required |
|---|---|---|---|
| Image generation | Plan and seven-file boot-branding payload | 25-file checked filesystem layer; data-only Docker image; explicit device-layer Containerfile and structural check | Approved digest-pinned Fedora device base, real services, licensed runtime, initramfs generation/inspection, complete OS image |
| USB boot/live session | No image or live session | Inactive generic-ISO input with a QOR OS menu label | Version-pinned image builder, actual USB/live bootstrap, recovery/diagnostics and tested firmware paths |
| Installer framework | Calamares mentioned in planning only | Original QOR theme using official mark and generated Lumen roles; inactive settings | Packaged version and licence review, actual Calamares run, hardware checks and live launch integration |
| Choices/disk/encryption | No implementation | Candidate partition config chooses nothing initially and retains labels/warnings | Integrated KPMcore backend; explicit physical-disk/data-loss review, stale-disk refusal, encryption and existing-data tests |
| Deployment/progress | No implementation | Unresolved deployment/review slots deliberately kept outside `/etc/calamares/` | Reviewed bootc adapter; truthful backend progress, errors, cancellation and interrupted-install recovery |
| Bootloader | No installation backend | No custom bootloader logic | Maintained backend, UEFI boot and recovery proof; no BIOS/Secure Boot support claim |
| Completion/reboot | No installer | Inactive completion config defaults to no reboot | Genuine success detection, user-selected reboot, installed-system boot and failure checks |
| First run | No app | Real `qor-setup`: three-page introduction, keyboard controls, reduced-motion page fade, checked completion state; once-per-device-session unlock hook | Real authentication/session service, installed-image launch/reboot proof, screen-reader and hardware testing |

Calamares is the candidate because its native Qt interface can be branded without
rewriting partitioning. Its KPMcore partition module is **not integrated here**, and
there is no verified Calamares-to-bootc adapter. `settings.conf.in` has unresolved
`@VERIFIED_DISK_REVIEW_MODULE@` and `@VERIFIED_BOOTC_DEPLOYMENT_SEQUENCE@` slots.
The checked device layer refuses active installer settings and live-media activation
at this checkpoint. There is no install button that can erase anything.

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
mode is certified. WSL/software rendering is not native hardware evidence. The next
external prerequisite is the owner's existing CI VM Create go-ahead, followed by actual
Prove results under [CURRENT CI instructions](../docs/CI.md#what-ci-trusts). Guest Qt needs
its documented licence prerequisite. A proven VM alone still needs the approved device
base and reviewed installer adapter before disk-install testing. The CI isolation VM is
not an installer target: a separately authorised disposable QOR boot-test VM and empty
virtual disks are also required. No VM was created or
started, no runner changed and no physical installation or host EFI modification performed.

Primary framework contracts reviewed: [Calamares](https://github.com/calamares/calamares),
[partition options](https://github.com/calamares/calamares/blob/calamares/src/modules/partition/partition.conf),
[bootc installation](https://bootc.dev/bootc/bootc-install.html) and
[generic bootc ISOs](https://osbuild.org/docs/developer-guide/projects/image-builder/advanced/bootc/isos/).
Those documents are design inputs, not runtime passes. The older Anaconda ISO path is
being replaced upstream; no legacy builder invocation is presented as a verified command.

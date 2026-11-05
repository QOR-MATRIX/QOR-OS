# image — the QOR OS image (Phase 3)

> **Status:** planned. The image is built on **Fedora Atomic with bootc**
> ([QOR-0004](../docs/decisions/QOR-0004-the-base-image-is-fedora-atomic.md)).
> The branding filesystem payload builds; there is still no bootable image builder.
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

- [ ] `image/Containerfile` from a Fedora bootc base, with NVIDIA's open
      modules and Mesa; built and pushed by CI as an OCI image
- [ ] Secure Boot: a QOR OS key that signs the kernel modules, and enrolment
      on first boot
- [ ] The QOR OS layer installed from a CI build against the commercial Qt kit
      (Qt's distribution terms: [QOR-0008](../docs/decisions/QOR-0008-before-a-public-release.md))
- [ ] Prism on its own KMS backend as the session ([QOR-0005](../docs/decisions/QOR-0005-every-display-at-its-own-refresh.md)),
      started by greetd with a Prism greeter, with a measured transition from the OS splash
- [ ] Steam and Proton inside gamescope as a Wayland client of Prism
- [ ] Installable disk images and an installer ISO (`bootc-image-builder`)
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

The eventual Fedora Containerfile must copy those paths into the **image filesystem**,
install Fedora's Plymouth script and text plugins, build that image's initramfs with the
Plymouth dracut module, then verify both the root filesystem and extracted initramfs against
this payload. Do not run `plymouth-set-default-theme`, `dracut`, or an installation command
on the workstation. No hook here changes host boot arguments or adds `quiet`/`rhgb`.
The selected image base and bootloader must supply their actual initramfs commands;
there is no claimed executable image recipe until those exist. Missing plugins/assets,
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
image or boot sequence: Plymouth, bootc, an image builder and a proven VM are unavailable in
this session. Actual firmware/menu/OS splash, graphics fallback, prompts, multiple displays,
initramfs inclusion, rollback and the splash-to-greeter handoff still need real image/VM and
hardware evidence. The CI VM Create/Prove and guest Qt licensing gates remain unchanged.

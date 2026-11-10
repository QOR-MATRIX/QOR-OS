# image — the QOR OS image (Phase 3)

> **Status:** a private ISO boots automatically through the branded UEFI menu into the live OS.
> Private stock installation, actual installer UI reboot and media-free welcome persistence pass. The base is **Fedora Atomic with bootc**
> ([QOR-0004](../docs/decisions/QOR-0004-the-base-image-is-fedora-atomic.md)).
> The checked branding filesystem layer builds into a data-only container. The first-run
> app runs in the installed private VM. The earlier image retains completion through two
> installed reboots; the repaired image passes actual UI reboot and one installed reboot.
> Full device and release acceptance remain open.
> The first image is Phase 3 work
> ([ROADMAP](../docs/ROADMAP.md#phase-3--boots-and-plays-pc-games)).
>
> QTOS's Yocto skeleton (`meta-qtos`, `kas`) was retired by QOR-0004. It is in
> the history before commit "Retire the Yocto skeleton for Fedora Atomic"
> (`git log -- image/meta-qtos`).

## Q Pi development edition

[QOR-0025](../docs/decisions/QOR-0025-q-pi-development-edition.md) scopes **Q Pi**:
Pi 5 ARM64, 8 GB RAM and nominal 128 GB media, a real maintained Pi login and
desktop, native QOR applications and usable local development tools. Its initial
base candidate is [Raspberry Pi OS 64-bit desktop, Debian Trixie](https://www.raspberrypi.com/software/operating-systems/),
the basic desktop edition rather than Full. This is a development-edition
exception to the Fedora/Qt-only host desktop. Require local login and explicitly
disable base autologin. Neither Prism's simulator nor its unfinished device
authentication becomes that login. The full device guard remains unchanged.

`pi/preparation.json` is planning data, **not a generator configuration**. Its
base checksum is upstream-advertised metadata only: no base artifact was
downloaded or hashed locally. The direct metadata request returned HTTP 403;
the official published page was reviewed separately. A candidate generator commit
is recorded from a read-only Git ref lookup. On 10 November that exact source
was checked out and its dependency/tool/boot recipes inspected; its runtime is
still unqualified. Scratch budget, artifact paths and acceptance receipts remain
unset. The application presets below provide an actual build entry point, but
no image recipe is accepted. A stock desktop alone does not meet the Q Pi
milestone without tested QOR applications and a small native development build.

### Reviewed environment and preparation checks

The 9 November review began at clean `3a5ddc3`; all fourteen checkouts were
clean and the original `heavy.py` OS lock was free. No active QOR compiler/VM
worker was identified. Evidence is in `build/q-pi-2026-11-09/review-01/`, including
the untouched supplied bundle's six matching SHA-256 entries and its twelve
passing synthetic parser tests. The seventh file, `SHA256SUMS` itself, is not
covered by that manifest. Self-consistent hashes do not authenticate its origin.
Instructions inside that bundle remain proposals; the owner's current request
sets the scope. Existing amd64 installer proof is not Pi boot proof.

| Observation at initial inventory | Result and implication |
|---|---|
| Windows available RAM | 1,668 MiB; below the unchanged 3,072-MiB heavy-job floor |
| C: physical free space | 34.18 GiB; Ubuntu WSL's registered backing directory is on C: |
| X: physical free space | 4.43 GiB; preserve existing output, no cleanup authorised |
| WSL free space | About 884 GiB virtual; not physical backing capacity |
| Existing Linux host | x86-64 Ubuntu 26.04 under WSL2; not a supported native generator host |
| Existing Linux Qt | Commercial 6.12.0 `gcc_64`, x86-64; no Linux ARM64 target kit identified |
| ARM64 tools | No GNU AArch64 compiler, QEMU AArch64 or Pi generator identified |
| Packaging tools on WSL PATH | No podman, mmdebstrap, bdebstrap or genimage identified |
| Namespace capability | Current WSL user has no effective capabilities; no namespace permission test or privilege change performed |

These are dated observations, not permanent resource promises. The 11:17 local
inventory records **1,007 MiB
RAM, C: 27.17 GiB free and X: 4.43 GiB free**; resources changed during review.
`build/q-pi-2026-11-09/review-01/review-result.json` retains the exact byte counts.
Before any heavy phase, recheck workers, the original lock and `python tools/image/vm_resources.py`
on Windows. It refuses to start below 3 GiB and never frees RAM itself. Do not
inspect or manage unrelated projects' services to meet this budget. If it fails,
stop the QOR attempt and report the shortfall. Retired cross-project service
wrappers and bookkeeping are removed from `build/`; preserve the QOR evidence
and do not restore or rerun those wrappers. Size source,
Qt/sysroot, package caches, expanded filesystems, raw output and compressed output
on the actual backing volume before selecting a build route. No measured builder
disk minimum is established yet. A compact image can later expand on its own
booted media; 128 GB capacity need not mean a 128 GB raw download.

The repository probe accepts optional `--scratch-root <existing-directory>` and
`--scratch-budget <measured-budget.json>` arguments together. The budget is a
bounded schema-1 JSON object with exactly `schema` and `components`. Components
must contain exactly `source`, `qt_sysroot`, `package_cache`, `expanded_root`,
`raw_image`, `compressed_output` and `reserve`, each a positive integer byte
count derived from the selected build route's measured peak needs. Their sum,
including reserve, must fit the selected volume's current available space.
There is no built-in estimate; null, zero, floating-point, missing, duplicate,
unknown and overflowing values fail. Budgets are at most 64 KiB.

This read-only capacity observation creates no directory, allocates no file and
reserves no space. It permits fixed local Windows volumes or native Linux local
ext4/XFS/Btrfs/F2FS disk mounts. It refuses WSL observations, overlay/tmpfs and
remote or removable storage rather than treating virtual free space as backing
capacity. A Windows check can observe a local scratch volume but does not size a
WSL virtual disk's required growth. Omitted or invalid budgets remain blockers.
Passing this check establishes only available capacity against the supplied
numbers; the builder, realistic budget sizing and every other input still need
qualification. The separate 3-GiB memory floor cannot be reduced by a caller.

Native Linux RAM can now be observed by the repository probe. It takes the
minimum of the kernel's `MemAvailable` estimate and `memory.max` minus
`memory.current` for every visible non-root cgroup v2 ancestor. It counts no
swap, assumes no automatic cache reclamation within a capped group, and refuses
missing/ambiguous counters, unsafe paths or cgroup migration during observation.
This route requires the standard `/sys/fs/cgroup` unified mount with its root
visible; unsupported layouts remain unmeasured. WSL remains blocked on this
route because guest memory cannot establish Windows host headroom. The check
reads only kernel resource metadata and changes no setting or service.
The [Linux proc](https://docs.kernel.org/filesystems/proc.html) and
[cgroup v2](https://docs.kernel.org/admin-guide/cgroup-v2.html) interfaces define
these observations; they are capacity snapshots, not builder or image acceptance.

The included preflight ran read-only under the original heavy lock and exited 2:
wrong host architecture/distribution, missing tools, source query timeout, failed
Qt version detection and wrong target ELF architecture. Follow-up identified two
probe defects: the Git status query took about 17 seconds against its 15-second
limit, and Qt's version file delegates to `Qt6CoreConfigVersionImpl.cmake`.
The repository probe allows a bounded 60-second Git query and reads those fixed
metadata files without executing CMake. Its ARM64 ELF layout checks additionally
refuse invalid header/type/version, overlapping/truncated program tables,
out-of-file segments, address overflow and incongruence at the declared segment
alignment. The probe uses a conservative qualification floor for declared load
alignment and excludes empty load segments; these are not universal ELF validity
rules ([ELF program-loading specification](https://gabi.xinuos.com/elf/07-pheader.html)).
A valid ELF header is still not Linux GNU ABI, licence or dependency
closure proof. The supplied bundle stays unchanged.

Run the low-memory, read-only probe on the existing WSL kit to reproduce the
architecture blocker (exit 2 is expected). Use a reviewed full commit identity;
the source-clean check also reports the local preparation edits until committed.

```powershell
wsl.exe -d Ubuntu --exec python3 -B /mnt/x/Repoz/QOR-OS/tools/image/pi_preflight.py --source /mnt/x/Repoz/QOR-OS --qt-target /home/godmode/Qt/6.12.0/gcc_64 --expected-commit 3a5ddc35f3ec59dcb1cf17a7a07cb79e273d2572
python -B -m unittest discover -s tests/tools -p test_pi_preflight.py -v
```

These probes do not install, mount, build, configure networking, execute target
code or contact unrelated projects' services. Scratch sizing and qualified image inputs deliberately
remain failed/unqualified checks; this preparation probe cannot certify a build.
Tests use synthetic bytes and temporary ordinary files, not a Pi or emulator.

### Builder, graphics and payload qualification still required

The [`rpi-image-gen` candidate](https://github.com/raspberrypi/rpi-image-gen/blob/fb44a4cde4ac54847ef1966957149e2c2b716fdf/README.adoc)
supports native Debian Bookworm/Trixie ARM64 hosts; x86/QEMU/container routes are
outside its formal support. Its namespace operations require `CAP_SYS_ADMIN` or
equivalent permission. Its private tool sysroot must be verified against an exact
generator commit rather than assuming every tool must be on PATH. The minimal
example disables login passwords and is not a desktop recipe. Do not run
`install_deps.sh`, grant privileges or use an unrestricted container by following
this guide. Any such action needs its exact package/configuration changes reviewed
and approved first. No native ARM64 builder has been selected or provisioned.

Candidate source pin: `fb44a4cde4ac54847ef1966957149e2c2b716fdf`, observed using
`git ls-remote https://github.com/raspberrypi/rpi-image-gen.git HEAD`.
Its [fixed-revision dependency manifest](https://github.com/raspberrypi/rpi-image-gen/blob/fb44a4cde4ac54847ef1966957149e2c2b716fdf/depends)
identifies the proposed native-builder package changes: coreutils, bash, python3,
python3-yaml, python3-debian, dpkg-dev, mmdebstrap, debian-archive-keyring, podman,
uidmap, dbus-user-session, zip, dosfstools, e2fsprogs, grep, rsync, curl,
ca-certificates, xz-utils, mtools, pv, btrfs-progs, dctrl-tools, uuid-runtime,
fdisk, python3-jsonschema, cryptsetup, python3-pip, make, build-essential,
autoconf, automake, libtool, autopoint, flex, gettext, pkgconf and cmake.
This is an upstream package-name review, not an approved installation or resolved
package-version lock. First choose the separate supported builder, inspect its
existing packages and give the owner the exact install delta and namespace action
before requesting approval. Do not apply that package list to this Windows/WSL
host or the Kali installation. The pinned source checkout is preserved under
`build/pi-builder-2026-11-10/rpi-image-gen/`; its HEAD and clean status were
checked. No package installer or generator build was run. Its private recipes pin
bdebstrap 0.7.0 and genimage 20 archives by SHA-256; those tools have not been
downloaded/built/qualified here. `rpi5` requires `rpi-linux-2712`, whose package is
`linux-image-rpi-2712`. `image-rpios` supplies MBR, FAT boot and ext4/btrfs root
layouts; this is a Pi boot path, not the amd64 bootc/UEFI path. The upstream
minimal suite also includes SSH and has locked passwords: do not use it as the
generic Q Pi desktop/login configuration. Desktop packages, disabled remote
access, local provisioning and immutable package inputs still need an actual
recipe and validation against the selected builder/base.

The [Pi 5 layer](https://raspberrypi.github.io/rpi-image-gen/layer/rpi5.html)
uses the 2712 kernel with **16-KiB pages**. Verify every shipped executable,
library, Qt plugin and helper for ARM64 GNU/Linux, that page size, loader and
resolved dependencies. Android/Windows ARM64 kits and QEMU header probes do not
supply that evidence. [Qt's Linux ARM desktop reference](https://doc.qt.io/qt-6.12/supported-platforms.html)
is Pi 5 with 8 GB on Ubuntu 24.04; installer ARM64 binaries use glibc 2.39.
Check actual base/kit ABI compatibility and the commercial entitlement for all
needed modules, native host tools and private packaging. Do not use a distro's
older Qt or infer entitlement from Qt source files being present.

The probe also reads the literal `QT_QMAKE_TARGET_MKSPEC` assignment in
`lib/cmake/Qt6/qt.toolchain.cmake` as bounded text, never as executable CMake.
Its `qt_linux_desktop_metadata` check refuses Windows, Android and HarmonyOS
target metadata, missing/ambiguous/dynamic assignments and unsupported bracket
syntax. Linux GCC/Clang metadata alone does not certify ARM64, GNU ABI,
16-KiB dependency closure or licence scope; those checks remain separate.
On 10 November the installed Windows/Android/HarmonyOS 6.12.0 kits are refused
by this new check. The WSL `gcc_64` kit reports `linux-g++` but is x86-64.
The owner reports Enterprise Qt; no commercial Linux ARM64 kit or supported
native ARM64 builder has thereby been verified. No account file or credential
was read and no installer was run.

Keep the supported base's Pi 5 kernel/modules, device trees/overlays, boot
partition/configuration and Pi firmware files together. Do not transplant this
repository's amd64 GRUB/OVMF/bootc VM route. EEPROM/host boot updates are outside
scope. Preserve supported Mesa V3D/DRM/KMS and Wayland integration, input,
NetworkManager, BlueZ and audio. On hardware, record the actual renderer and Qt
platform backend; llvmpipe/lavapipe is software evidence, not hardware graphics.
Power supply, cooling and throttling also require actual Pi checks.
For the physical setup, verify a suitable **5-V/5-A USB-C supply** and cooling
for sustained builds against the [Pi 5 specification](https://www.raspberrypi.com/products/raspberry-pi-5/).
Use micro-HDMI-to-HDMI for the display and temporary USB input; USB-C is the
power connection, not a display output. No accessory has been purchased or configured.

Initial development-tool candidates are Git, Python, CMake, Ninja, pkg-config,
GCC/G++, GDB, a terminal and a small editor. Determine their exact resolved
versions in the eventual base manifest; nothing was installed in this review.
QOR payload scope must be proved independently:

- `QQ_PLAYER_ONLY` omits Studio/agent/tests but still requires Quick 3D. Studio
  and the shared runtime import Quick 3D; `runtime/Scene.qml` also imports Physics
  unconditionally. A later 2D Studio needs a measured dependency refactor, a
  2D-only sample/edit/save/build/play test and render-on-demand checks on Pi.
  First separate the 3D scene/physics imports and editor viewport/gizmos from
  the 2D runtime/editor path, then make CMake's 3D dependency conditional. Merely
  hiding a viewport leaves QML imports and linked libraries in the dependency
  closure. Preserve project formats; test with Quick 3D/Physics absent, then
  measure startup, idle/peak RAM, save/reopen and packaged 2D play on the Pi.
- Existing shell apps use QOR services and are not automatically standalone
  desktop apps. Select native working clients without substituting simulator
  services. No QOR app is currently verified on ARM64.
- `tools/dev/products.sh` pins a Linux **x64** PDFium archive. Do not run that
  download as an ARM64 Media recipe; qualify a separate pinned ARM64 input and
  licence/dependency closure first. Do not weaken asset provenance checks.
- Later remote heavy builds are optional and user-controlled. Qualify a specific
  private builder, access scope, Qt licence, job/resource bounds and returned
  artifact source/architecture/dependencies. No paid service, tunnel, SDK copying,
  automatic offload or shared credentials are configured by this plan.

### Native QOR application build

`products/qq/CMakePresets.json` now supplies `q-pi-release` and `q-pi-asan` for
the actual QQ sources, with Studio, Player and tests enabled. These use CMake
and Ninja, one compile job, warnings as errors and QOR hardening. The native
host gate runs before compiler discovery, rejecting x86-64, cross compilation
and WSL. Both the maximum and common linker page sizes are 16 KiB: the latter
preserves read-only relocation protection on that kernel
([GNU ld](https://sourceware.org/binutils/docs/ld/Options.html)). Every Qt/plugin/
system dependency still needs its separate ELF layout and ABI checks.

First supply an existing native Debian/Raspberry Pi OS Trixie ARM64 builder
and its entitled Qt 6.12 Linux ARM64 kit. The current machine has neither:
WSL and its `gcc_64` kit are x86-64; the installed Windows/Android ARM64 kits
are unsuitable. No remote builder access is configured by this procedure.
Required native application tools are CMake 3.27+, Ninja, GNU C++, pkg-config,
Python and Git. The kit must include Core, Gui/GuiPrivate, Multimedia, Network,
Qml/QmlPrivate, Quick, Quick 3D/Physics, Quick Controls, Test and D-Bus, with
Shader Tools for QML assets. Qualify these through the source's `find_package`
and QML imports, not a guessed desktop-only Qt subset. Commercial entitlement,
glibc/C++ ABI, graphics/audio/input dependencies and Secret Service still need
verification. No installation or licence acceptance is implicit.

After source/kit/dependency/resource qualification, hold the builder's existing
QOR heavy-job lock and check `vm_resources.require_headroom` using
`vm_resources.linux_available_memory` on that native host. Size source, kit,
cache and build outputs on its actual backing disk. There is no measured budget
yet; this host's 10 November snapshot had about 30.0 GiB free on C: and 31.4 GiB
on X:, while WSL advertised 883.7 GiB. That virtual capacity is not usable as a
physical budget. Do not copy the SDK into an image without the confirmed scope.

On the qualified builder, with `QOR_QT_DIR` set to that kit, run from
`products/qq/` (these commands were **not** run on ARM64 here):

```bash
cmake --preset q-pi-release
cmake --build --preset q-pi-release
python3 ../../tools/ci/check_hardening.py ../../build/q-pi-qq-release/qq
```

Use the existing product harness for hidden display/isolated-keyring tests.
Its new `--preset` option configures the native preset and tests that same
`<build-root>/qq` cache, preserving the preset's sanitizer mode, page sizes and
single-job build. Do not run raw CTest on a builder's real desktop/keyring. The
matching CTest presets are for an already isolated environment:

```bash
# From the repository root, still inside the single guarded heavy job:
tools/dev/products.sh --product qq --preset q-pi-release --qt "$QOR_QT_DIR" --build-root build/q-pi-qq-release --attempts 1
# Repeat with q-pi-asan and build/q-pi-qq-asan for the sanitizer build/tests.
```

The harness requires Qt Wayland Compositor for its hidden display and
`dbus-daemon`, `python3-dbus` and `python3-gi` for its private session/keyring.
It reports the renderer it actually expects; software is not Pi GPU evidence,
and its existing performance floor is not waived. `--preset` refuses other
products and unknown preset names before building or downloading dependencies.
The sanitizer preset must also build and run the hostile-input suites before
acceptance. Real Pi graphics, a local saved/reopened project, image composition
and physical boot remain separate gates. This session passed the portable
resource/ELF/preset-refusal suites: 59 Windows cases plus four platform skips,
and all 63 cases under x86-64 WSL. These are not ARM64 application results.

### Q Pi first boot and Bluetooth bootstrap

This is an unapplied plan. No media is connected or authorised. Preserve Kali;
its media and partitions are never candidates for this preparation.

1. **Preferred first boot: temporary USB keyboard/mouse and HDMI.** Bluetooth
   devices are unavailable until paired. Use the supported base's first-user
   workflow to choose a local account/password, disable autologin and require
   the real login before the desktop. Enter Wi-Fi credentials and regulatory
   country locally through the base networking interface. Ethernet is optional.
   Pair and trust the Bluetooth keyboard/mouse using the base desktop, then test
   reconnect at the login screen and after a cold boot before removing USB input.
2. **Optional later headless bootstrap:** after approval for one exact Pi/media,
   account, public key and private network, use the supported base's
   [Imager customisation](https://www.raspberrypi.com/documentation/computers/getting-started.html)
   to enter Wi-Fi/country, a chosen local account and SSH **public-key** access
   locally. Do not send credentials through chat or logs. Explicitly disable
   autologin: account creation alone does not disable it. A custom generator image
   must separately implement and test this protocol; Imager compatibility cannot
   be assumed. Do not use the obsolete boot-partition Wi-Fi-file shortcut.
3. Require key-only SSH for the intended ordinary account, no root SSH and no
   password SSH. Verify these settings on the actual selected image; package
   presence or an Imager setting is not proof of effective daemon configuration.
   Confirm the Pi's host-key fingerprint through a trusted local console/channel
   before first SSH; discovery or `ssh-keyscan` alone is not authentication. Without
   that trusted route, first remote authentication remains unresolved. Never
   bypass a changed-host-key warning. No public port forwarding or cloud enrolment.
4. For Bluetooth pairing through SSH, a working BlueZ authentication agent and
   the devices' passkey/confirmation workflow must be tested. Do not promise a
   remote desktop or pairing at the greeter merely because SSH works. A separately
   approved base remote-desktop service must have verified authentication and
   private-network scope before use; USB input remains the recovery path.
5. A reusable image has no Wi-Fi password, account password, private key, provider
   credential, SSH host key or shared machine identity. Per-device provisioning
   necessarily writes private configuration to that one device, never the golden
   image or repository. Generate unique host identity/keys on first boot, with
   interruption/retry tests that preserve existing state. SSH stays disabled in
   the generic image until expressly provisioned. No local desktop/project save
   should require Internet access.

### Pi image and hardware acceptance

Before delivery, build only to a new regular file in an approved, sized scratch
directory. Record clean source, base/generator/package identities, commands,
native exits, failure/skips, Qt/dependency manifests, notices, software bill of
materials, compressed/decompressed hashes and resource observations. Confined
static checks must verify partition bounds, filesystems, Pi 5 boot assets,
target ABI/alignment, real login/autologin policy, first-boot identity and optional
SSH policy. First-boot expansion must target only the verified booted media,
never an arbitrary attached disk. No Pi-capable emulation has run; emulation
would still not prove this board's boot firmware, graphics or radios.

Only after a separate exact **spare-media** selection, backup and write
authorisation: verify the Pi 5/ARM64 identity and kernel page size; complete real
login; test HDMI/scaling, hardware rendering, Wi-Fi, Bluetooth reconnection at
login, input, audio, USB/Ethernet, power/cooling and shutdown. Open a native QOR
app, save/reopen a local project and compile/run a small native program, offline
as well as online. Test interrupted setup, three reboots with persistent state
and a 30-minute workload/idle check with measured resource use. Record failures
and unavailable features. No ARM64 build, image assembly, static image validation,
Pi boot or hardware test has passed yet. The existing VM proof below is separate.

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

The earlier clean `feed626` proof records five actual VM phases with native exit 0: normal
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
and accepts detachment. The fresh clean 5b7d321 image includes that correction and
passes stock installation, but its normal UI reboot still fails.

A subsequent diagnostic installation captures the real post-install state:
`/run/bootc/storage` remains bound to the installed partition after bootc exits 0.
The unchanged guard correctly reports the disk active. Targeted diagnostic cleanup
of that verified mount makes the strict guard pass and the actual UI button reboot.
A separate real-kernel control leaves a temporary RAM mount visible after an ordinary
child exits; `unshare --mount --propagation private` contains the mount and tears it
down with its child. Both run in the existing installer context with SELinux enforcing.
The local backend now uses that isolation for its supervised bootc invocation and
requires the strict target guard after successful exit, before announcing completion.
The image recipe checks the existing `unshare` executable. No mounted-target refusal
is relaxed. Diagnostic cleanup is not part of the packaged flow or normal proof.
The diagnostic records are in
`build/vm-ui-reboot-2026-11-08/reboot-state-diagnostic/` and are not relabelled as
normal installation proof.

The fresh clean `66df3f7` image and same-revision compiler pass the normal packaged
route on a new 8-GiB disk. Stock installation exits 0 with a complete 2,044-byte log;
the unchanged post-install guard passes and the actual UI Reboot button produces a
kernel restart and native VM exit 0. No manual mount cleanup, debug shell or guest-agent
power command is used for that live phase. Two installed boots have no ISO/CD mount.
The first completes the real three-page welcome from pending; after a restricted-agent
reboot, the next finds the same completed record before welcome runs. All three native
VM phases exit 0 with three distinct boot IDs. The final verifier accepts
`build/vm-ui-reboot-2026-11-08/isolated-ui-proof/accepted-isolated-ui-proof.json`.
The ISO is 3,601,670,144 bytes, SHA-256
`9251ca6d0799d987a30962faf32fb0278c9a03697210751cd377f7a2384fc575`.
Both new image branches pass 14 fatal-warning bootc checks, with zero warnings and
one skipped check. The live installation peaks at 15 tasks within the unchanged
64-task limit, with four I/O workers and no memory-limit, out-of-memory or task-limit
hits. All launch RAM checks retain the 3-GiB minimum. The VM is shut down and the
approved non-database service pause is restored with PostgreSQL healthy. The earlier
`feed626` proof, both normal UI failures and all diagnostic outputs remain separate.

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
`qor-session.service`, QOR identity and non-development build contracts from both
`qor-setup --build-capabilities` and `prism --build-capabilities`. The latter returns
before graphics, audio, service connections or user-state access. It reports
`schema:1`, the compiled `development` boolean and an `authentication` boolean.
The device guard requires `development:false` and `authentication:true`, rejects
unknown fields/types and refuses an omitted probe. Current real device builds
report `authentication:false`, matching their locked session without an authentication
backend. The future authenticated contract used by structural tests is an inert
fixture only. No real authenticated session is claimed or enabled by these checks.
The compiled query is checked in Release, device and ASan/UBSan builds with an
unavailable graphics platform and empty private user directories; it exits 0 and
writes no user state. The real device binary's unavailable authentication is refused
by the guard using an inert filesystem fixture. Both command-line modes also pass
on Windows. Hardening and all ten existing QML lint commands pass, while all three
Linux Prism build warning gates still fail on 12 pre-existing Wayland base-type
registration warnings. The bounded headless checks clear no device-build or session
gate. Their private evidence and preserved failures are under
`build/device-session-2026-11-08/`.
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

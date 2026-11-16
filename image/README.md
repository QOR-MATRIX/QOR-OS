# image — the QOR OS image (Phase 3)

> **Recorded private x86-64 VM evidence (7–8 November 2026):** an ISO booted
> automatically through the branded UEFI menu into the live OS. Private stock
> installation, actual installer UI reboot and media-free welcome persistence passed
> at the revisions recorded below; this is not a rerun of today's tree or Q Pi proof.
> The base is **Fedora Atomic with bootc**
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

The owner's 12 November request is a **single downloadable QOR OS image containing
both full QOR OS and Q Pi**, booting supported hardware and selecting a compatible
edition. Its feasibility assessment is deferred until Prism regressions, full
core-enabled OS/product validation and runner isolation pass. No combined image
layout, boot chain or installer backend has been adopted. The assessment must use
explicit architecture, firmware and hardware profiles; ARM does not imply Q Pi.
Uncertain detection must offer a compatible manual choice or an unsupported
notice, never guess. Any supported alternative needs its tradeoffs explained, and
readiness requires testing on the intended hardware. Shared Lumen behavior,
accessibility, visual quality and measured performance remain acceptance criteria.

The owner will supply storage when builds and the installation plan are ready.
No physical-media write, formatting or replacement is authorised in this increment.
Older card observations and scoped authorisations below remain historical; they
do not select or approve a current target. Q Pi work resumes alongside QOR OS only
after the preceding milestones pass. This paragraph records requirements, not a
feasibility conclusion or a change to the accepted base-image decisions.

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

### Pi 5 builder bootstrap, before Q Pi

The owner's temporary **stock Pi OS builder** remains unreachable in the latest
authorised diagnostic on 11 November. From the PC's Wi-Fi interface, its hostname
did not resolve, the last address timed out before SSH authentication, and one
bounded mDNS query for that hostname returned no address. No subnet scan, new
host trust or network setting change was made. These results cannot identify
power, boot, addressing or network isolation as the cause. Check the Pi's power/
status LEDs, then HDMI boot state and current address; do not reset or reflash it.
Current diagnostic evidence: `build/q-pi-native-2026-11-11/session-02/`.
The local ABI-audit increment made no connection attempts. Earlier on 11 November,
authenticated observations confirmed Pi 5 Model B Rev 1.1,
native ARM64, Debian 13/Trixie, kernel `6.18.50+rpt-rpi-2712` and 16-KiB pages.
The supplied initial receipt reports 8,058 MiB total RAM and Debian point version
13.7; subsequent bounded probes observed about 7,450 MiB available and
114,904,260,608 free bytes on the home/root filesystem. These are snapshots,
not an application/image scratch budget. Power/cooling, throttling, desktop and
autologin policy still need verification. A **stock bootstrap OS**
provides the local desktop on which to inspect the board and later build/test
QOR applications. It is not the eventual **Q Pi image**, which must include
qualified native QOR applications, deployment dependencies and its own tested
first-boot policy. Installing a stock OS does not complete Q Pi.

The owner approved first-use SSH host trust after risk disclosure; it is **not
independently verified host identity**. The retained ED25519 fingerprint matched
on fresh connections. After two private password-authenticated diagnostics, the
owner separately installed and authorised a dedicated restricted key. Fresh
key-only connections succeeded with batch mode, explicit identity selection,
strict host checking and no PTY/agent/X11/port forwarding. No password or private
key content entered this repository or its logs. Stop on host-key change; do not
replace trust entries. This builder-specific grant does not enable SSH or embed
credentials in a generic Q Pi image. The latest owner request authorises a
reviewed standard builder-package transaction, including CMake, once connected;
it does not approve new terms, credentials or service/network/security changes.
Evidence: `build/q-pi-native-2026-11-11/session-01/`, including the original supplied
receipt, authenticated observations, source hashes and native test log. The
80 native resource/coordinator passes are historical evidence, not a live
connection or application build. The generic first-boot/physical-receipt design
below remains a separate untested flow.

The official [OS catalogue](https://www.raspberrypi.com/software/operating-systems/)
was rechecked on 10 November: the basic **64-bit Raspberry Pi OS Desktop** is
Trixie (Debian 13), dated 6 October 2026, kernel 6.18, advertised download
1,301 MB and installed storage 6,432 MB. The advertised hash still matches
`pi/preparation.json`. These are upstream metadata, not a downloaded/hash-verified
artifact or an installed Pi observation. Select Desktop, not Full, Lite, 32-bit
or Legacy, for this route. The official
[getting-started guide](https://www.raspberrypi.com/documentation/computers/getting-started.html)
recommends at least 32 GB for Desktop; that boot minimum does **not** size Qt,
source builds, sanitizers or image composition.

The earlier Windows-reader observation of the Samsung EVO nominal 128-GB card was
Disk 2, Generic Mass-Storage, **128177930240 bytes** (about 119.4 GiB), MBR,
`D:` BOOT/FAT32 and an unlettered partition. This owner-supplied observation is
identification only; neither the filesystem label nor that layout identifies
its OS, contents, health or Pi 5 bootability. This continuation did not rescan or
mount it. **The owner's 10 November update authorises this Samsung card as the
eventual Q Pi image target, including erasure of its old Kali/data, once the image
and first-boot plan are ready.** That replaces the earlier blanket no-card-write
scope only for this eventual Q Pi image. It does not authorise a stock bootstrap
image, standalone formatting, credentials, provisioning or security/access changes.
The [action-time write gates](#authorised-q-pi-target-and-action-time-write-gates)
still apply; no Q Pi image is ready and this session performed no media write.
The separately established stock builder does not prove which physical card is
currently in the Windows reader. Reidentify all media at action time. All other
devices and installations remain outside scope.

One card can hold the stock OS, working tree and Qt/application build files if
its **measured** free local filesystem capacity covers the phase's peak plus
reserve. An image output may also be an ordinary file there if the larger image
budget fits. Never overwrite the card that the builder is booted from, or write
an output image onto its backing block device. Separate scratch storage is
needed only when measured capacity, I/O reliability or performance is inadequate;
a second boot card is not required merely to inspect or compile on the Pi.
For later Q Pi boot testing, spare boot media preserves the working builder and
is the least disruptive option. The owner has authorised eventual Q Pi reuse
of the Samsung card and discard of its old Kali/data. If a separately approved
bootstrap later uses that card, preserve its QOR source/evidence and necessary
builder recovery material in a verified **off-card** location before replacement.
Reusing a builder's card requires shutdown followed by an external writer (or a separately qualified alternate
boot medium). The image and sole backup cannot live only on the card being
erased. Fresh identification and action-time erasure confirmation still apply;
using a different card requires its own target authorisation.
No extra storage purchase is presumed.

Use the [headless first-boot design](#q-pi-first-boot-and-bluetooth-bootstrap)
only after its image integration and exact bootstrap/access actions are authorised.
USB/HDMI is optional recovery. If an existing
approved card already boots suitable Trixie, inspect it before considering any
reimage. For stock bootstrap, an unidentified existing OS remains a stop condition,
not permission to replace it; this does not revoke the scoped eventual Q Pi write.
Read-only inventory may proceed over verified, scoped SSH or a local console once
access is authorised; bootstrap setup, package installation and image generation are
different actions.

### Read-only native bootstrap qualification

After an authorised boot and verified local/SSH access, use the existing tools only. These commands neither install
nor enable services, contact devices on the network, or inspect other disks.
Run in the reviewed QOR checkout on the Pi; obtaining that checkout requires an
approved transfer route and a verified revision, not invented SSH access.

```bash
python3 -B tools/image/pi_preflight.py --bootstrap-only
uname -m
uname -r
getconf LONG_BIT
getconf PAGE_SIZE
dpkg --print-architecture
grep -E '^(MemTotal|MemAvailable):' /proc/meminfo
findmnt -n -o SOURCE,FSTYPE,OPTIONS -T .
df -B1 --output=source,fstype,size,used,avail,target -- .
command -v git python3 cmake ninja pkg-config gcc g++ readelf dbus-daemon
# Optional only when already present; no sudo or installation to obtain them:
vcgencmd get_throttled
vcgencmd measure_temp
```

Record individual exits and missing commands rather than suppressing failures.
Expected architecture is `aarch64`, userspace 64, Debian architecture `arm64`,
page size **16384**. The new `--bootstrap-only` mode reads bounded Pi 5 model
and public Trixie identity metadata plus the running page size and the existing
3-GiB RAM guard. It needs no Qt kit, Git subprocess, package manager or source
commit argument. It rejects other boards/OS routes, x86, WSL, 4-KiB pages and
unmeasured/insufficient RAM. Exit 0 means only those observations passed;
`builder_qualified` stays false. Debian/Raspbian identity fields do not authenticate
a stock Raspberry Pi OS artifact. It cannot prove board authenticity, 8-GB capacity,
Desktop installation/login, tools, cooling, scratch, entitlement or native apps.
Missing/new board metadata needs review; never edit the probe to disguise it.

Inspect root and the **explicit chosen** scratch path with `findmnt`/`df`, not
an all-device scan. Repeat against that path if it differs from the checkout.
Keep the distinction between total board RAM, kernel `MemAvailable` and the
guard's cgroup-constrained available bytes. Swap is not headroom. The bootstrap
review's initial 10 November observation measured 4,445 MiB available with WSL stopped, against
3,072 MiB; C: 28,731,830,272 and X: 33,714,049,024 free bytes. Those numbers
measure this Windows host only and are preserved under
`build/q-pi-bootstrap-2026-11-10/review-01/`; recheck before jobs.
The 11 November native observations above supersede the earlier absence of Pi
RAM/free-space evidence; the full application/image peak budget remains unmeasured.

Size application and image phases separately. For application work measure
source, entitled kit, Release and ASan outputs, temporary build/test files and
reserve on the actual volume. For composition also measure package caches,
expanded roots, private generator tools, raw and compressed outputs using the
existing seven-component scratch schema below. Count concurrent copies; an
already-resident kit need not be allocated twice, but SDK staging/export may
need another copy. Record measurement commands, selected inputs and peak
method; unknown values stay unknown. Do not turn 128 GB nominal capacity or
upstream OS size into a guessed build budget. The Linux scratch guard accepts
supported local disk filesystems, including an on-board card's ext4 root when
identified as such; Windows removable-reader capacity is not a qualified build
volume. Observations reserve no space and must be repeated before each phase.

### Review the package delta without installing

**11 November concrete proposal, not installation approval:** the stock builder
has GCC/G++ 14.2.0, binutils 2.44, Ninja 1.12.1, Python 3.13.5, Git, pkgconf,
build-essential, D-Bus and its Python bindings. CMake is absent. The installed
`libc6` is `2.41-12+rpt1+deb13u4`, and `libstdc++6` is `14.2.0-19`; these do not
qualify the unseen Qt kit. No `qtpaths6`/`qmake6` or usual Qt directory was found.

The prescribed `--simulate --no-download --no-remove` APT commands exited **100**
with `Unable to fetch some archives`; that failure is preserved, not called a
passing install plan. A separate read-only `apt.Cache(memonly=True)` resolver
proposal, under the native lock/RAM guard, selected four new packages and no
removals/downgrades or replacements of installed packages. It called no update,
fetch or commit operation, and the package-status digest stayed unchanged.
The [APT in-memory cache interface](https://apt-team.pages.debian.net/python-apt/library/apt.cache.html)
distinguishes marking a proposal from committing it. Existing cached origins
report Debian/stable/main and trusted; index freshness and downloaded artifacts
have not been independently authenticated in this session.

| New package | Exact candidate | Download bytes | Installed bytes |
|---|---|---:|---:|
| `cmake:arm64` | `3.31.6-2` | 9,905,888 | 36,104,192 |
| `cmake-data` (`all`) | `3.31.6-2` | 2,268,208 | 12,168,192 |
| `libjsoncpp26:arm64` | `1.9.6-3` | 72,912 | 287,744 |
| `librhash1:arm64` | `1.4.5-1` | 128,888 | 281,600 |

Total: **12,375,896 download bytes; 48,841,728 installed bytes**, before staging
copies and reserve. Candidate SHA-256 digests, origins, dependencies and the
installed-state digest are in `native-package-proposal.json` beside the evidence.
These are repository metadata digests, not checksums of downloaded files.
The owner's latest request authorises installing the necessary standard build
prerequisites through a reviewed transaction. This historical proposal must first
be rechecked against the connected builder. Under its existing coordinator and
fresh resource checks, stage the matching four packages in a new confined
directory, verify their hashes/provenance and review maintainer scripts before
installation. No index refresh or service/security change is implicit. The
download command below remains **unexecuted**:

```bash
apt-get download cmake:arm64=3.31.6-2 cmake-data=3.31.6-2 libjsoncpp26:arm64=1.9.6-3 librhash1:arm64=1.4.5-1
```

Refuse an unavailable version, changed hash/origin, new dependency or unreviewed
script/service effect; do not fix it by adding foreign suites, forcing dependencies
or accepting a larger delta. Preserve the pre-install state and receipts. A
rollback requires a separately reviewed removal of only this approved delta;
never run broad autoremove/purge or delete the builder/evidence. Qt is a separate
entitlement/artifact decision and is not supplied by these Debian packages.

Do this on the authorised native builder, using **existing APT indexes**. There
is no exact install delta until that board's installed packages, candidates and
resolver output have been observed. Do not run `apt update`, `install_deps.sh`,
an installer or `sudo` in this review. Stale/missing indexes or inaccessible APT
configuration make the result provisional; a later approved index refresh needs
a fresh simulation and review before an installation decision.

Keep two request lists. The **application-builder** starting list is:
`git python3 cmake ninja-build pkgconf build-essential binutils dbus-daemon
python3-dbus python3-gi`. These do not supply commercial Qt. A debugger/editor
and graphics/audio development packages are separately reviewed additions based
on the exact selected kit; do not blindly install a complete Qt source-build
dependency list for a binary kit. The **image-generator** list is derived from
the pinned `depends` manifest below, only when composition is needed. This
avoids making Podman/namespaces prerequisites for a normal QQ compile.

For the generator, verify clean source at
`fb44a4cde4ac54847ef1966957149e2c2b716fdf` and SHA-256 of its `depends` file:
`9f6b1b343fe87e8b98e9d5ea0e3ee48ffcbefdb12decb8fa107bd96748f5c8d7`.
Read the manifest as data: each uncommented line
is `category:command[:package]`; use the package field when present, otherwise
the command. Deduplicate `all`, `bootstrap` and `build` entries. Compare the
result with the reviewed names below; no shell sourcing or upstream installer
execution. Verify additional selected layers' dependency manifests separately.

In Bash, choose **one** reviewed list, not both automatically. For the initial
application review the complete commands are:

```bash
packages=(git python3 cmake ninja-build pkgconf build-essential binutils dbus-daemon python3-dbus python3-gi)
LC_ALL=C dpkg-query -W -f='${binary:Package}\t${Architecture}\t${Version}\t${db:Status-Status}\n' "${packages[@]}"
LC_ALL=C apt-mark showhold
LC_ALL=C apt-cache policy "${packages[@]}"
LC_ALL=C apt-get --simulate --no-download --no-remove --verbose-versions install "${packages[@]}"
```

Missing `dpkg-query` entries can produce exit 1; retain that exit and every
missing name. Simulation exit 0 is a **proposal**, not an installed state.
[APT's simulation](https://manpages.debian.org/trixie/apt/apt-get.8.en.html)
does not lock the package database and non-root access may hide configuration.
Run with no concurrent package transaction; retain warnings. Read its complete
`Inst`, `Conf` and any removal/error lines, including dependencies and recommended
packages. Do not add `--no-install-recommends`, force/ignore flags or remove
protections simply to obtain a smaller/green proposal.

Before requesting installation, prepare a receipt for **every changed package**:
name/architecture, old installed state/version, proposed exact version,
new/upgrade/configure/removal action, reason (direct/dependency/recommendation),
approved origin/suite, authenticated package/index identity, download bytes and
installed-space change. Query the expanded delta's candidates as well as direct
requests. Inspect the candidate package metadata and maintainer-script/service
effects before approval, particularly Podman, user namespaces and session D-Bus.
Do not dump APT auth/config files, repository credentials or private URLs into
evidence. Keep local private details out of the sanitised receipt.

Review any held/broken packages, removals, downgrades, foreign architecture,
unauthenticated source, unexpected origin or unbounded storage as a stop.
Record index age/provenance and installed-state identity. Once exact versions
are selected, rerun the same **simulation** with reviewed
`name:arm64=version` or architecture-appropriate `name=version` arguments for the
complete delta; require the same resolved result. The later installation approval
must cover that transaction and its service/permission effects. The current
owner authorisation covers reviewed standard builder packages; unexpected effects
remain outside it. If state or candidates change, invalidate the receipt and
review the new concrete delta before proceeding.

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
reserves no space. Windows requires `DRIVE_FIXED` and refuses removable/network
volumes. Linux requires an ext4/XFS/Btrfs/F2FS mount with a `/dev/` source and a
matching device number; overlay/tmpfs and ordinary network mounts fail. Neither
that Linux check nor Windows's drive type attests physical backing. The Linux
check does not resolve loop/device-mapper layers or inspect removability: review
the selected backing storage separately. WSL observations are refused.
A Windows check can observe a local scratch volume but does not size a
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

The following is the **historical** read-only WSL refusal command (exit 2), not
a reason to start WSL again. Reuse its architecture evidence. A fresh probe needs
resident-WSL host headroom and a reviewed full commit identity; the source-clean
check reports local preparation edits until the owner authorises a commit.

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
and approved first. The physical Pi 5 now builds native applications, with the
focused results below. Its image-generation toolchain, complete dependency
closure and measured image budget remain unqualified.

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
package-version lock. First establish the supported native Pi builder, inspect its
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

Primary-source recheck on 10 November: Debian Trixie publishes
[glibc 2.41](https://packages.debian.org/trixie/libc6),
[G++ 14](https://packages.debian.org/trixie/g%2B%2B) and
[CMake 3.31.6](https://packages.debian.org/trixie/cmake). Those nominal versions
meet the documented version floors; this is an inference about prerequisites,
not evidence of the chosen Pi filesystem's packages or symbol compatibility.
Qt's Ubuntu ARM desktop reference and its separate Boot2Qt/Yocto configuration
do not qualify a Raspberry Pi OS kit. Record the actual kit provenance, module
entitlement, compiler/C++ runtime, base package versions and dependency results
on the selected native builder before accepting it.

Do not substitute older Boot2Qt download instructions: the
[6.12 changelog](https://doc.qt.io/Boot2Qt/b2qt-changelog.html) says prebuilt
images are no longer offered through the installer or customer portal and
target images must be built. Its Yocto route remains separate from Q Pi's
adopted Trixie desktop; neither route supplies an entitlement or target-ABI
result merely because the owner's account is Enterprise.

The probe also reads the literal `QT_QMAKE_TARGET_MKSPEC` assignment in
`lib/cmake/Qt6/qt.toolchain.cmake` as bounded text, never as executable CMake.
Its `qt_linux_desktop_metadata` check refuses Windows, Android and HarmonyOS
target metadata, missing/ambiguous/dynamic assignments and unsupported bracket
syntax. Linux GCC/Clang metadata alone does not certify ARM64, GNU ABI,
16-KiB dependency closure or licence scope; those checks remain separate.
On 10 November the installed Windows/Android/HarmonyOS 6.12.0 kits are refused
by this new check. The WSL `gcc_64` kit reports `linux-g++` but is x86-64.
The owner reports Enterprise Qt; that alone proves no commercial Linux ARM64 kit.
The native stock builder has since been observed as described above, but no Qt
kit was present. No account file or credential was read and no installer was run.

**11 November commercial artifact candidates.** The official
[installer catalogue](https://download.qt.io/official_releases/online_installers/)
lists `qt-online-installer-linux-arm64-online.run`. The public
[6.12 ARM64 package metadata](https://download.qt.io/online/qtsdkrepository/linux_arm64/desktop/qt6_6120/qt6_6120/Updates.xml)
lists `qt.qt6.6120.linux_gcc_arm64`, version `6.12.0-0-202609280346`, with
Ubuntu-24.04/AArch64 archive names. Add-on candidate roots are
`qt.qt6.6120.addons.qtmultimedia`, `.qtquick3d`, `.qtquick3dphysics`, `.qtshadertools`
and `.qtwaylandcompositor`, each with a `.linux_gcc_arm64` child. Resolve the
actual entitled dependency graph (including Quick Timeline where required),
docs/examples/tools selections and download/expanded sizes; public component
existence is not proof the owner's commercial account offers that artifact.
SpatialAudio belongs to Qt Multimedia, not a guessed separate installer module.

Qt's [6.12 supported-platform reference](https://doc.qt.io/qt-6.12/supported-platforms.html)
uses Pi 5/8 GB with Ubuntu 24.04/GCC 13 for ARM desktop and documents glibc 2.39
for ARM installer binaries. Trixie is unlisted; the installed newer libc/compiler
does not prove ABI or 16-KiB compatibility. Ubuntu remains an unadopted fallback.
The [Enterprise Debian route](https://doc.qt.io/qt-6/qt-debian-packages.html)
lists ARM64 Jammy and Bookworm/GLES, not Trixie, and names evaluation or **Qt for
Device Creation** entitlement. Do not add those repositories or foreign-suite
packages to this builder. Do not use credential-in-URL examples.

Before Qt staging, obtain the existing subscription's confirmation for native
Linux ARM64 6.12, every required add-on/private header, the isolated harness's
Wayland Compositor, and Trixie/16-KiB operation (or an entitled source build).
Separately confirm runtime/plugin redistribution and SDK/development-tool
distribution. Record artifact/version/module manifest and hashes without account
identifiers, credentials or new terms. Qt's
[build-system licensing note](https://doc.qt.io/qt-6.12/qt6-buildsystem.html)
does not make a configure switch proof of contractual entitlement. No commercial
artifact has been selected, downloaded, installed or accepted here.

Keep the supported base's Pi 5 kernel/modules, device trees/overlays, boot
partition/configuration and Pi firmware files together. Do not transplant this
repository's amd64 GRUB/OVMF/bootc VM route. EEPROM/host boot updates are outside
scope. Preserve supported Mesa V3D/DRM/KMS and Wayland integration, input,
NetworkManager, BlueZ and audio. On hardware, record the actual renderer and Qt
platform backend; llvmpipe/lavapipe is software evidence, not hardware graphics.
Power supply, cooling and throttling also require actual Pi checks.
For the physical setup, verify a suitable **5-V/5-A USB-C supply** and cooling
for sustained builds against the [Pi 5 specification](https://www.raspberrypi.com/products/raspberry-pi-5/).
For optional display/input recovery, use micro-HDMI-to-HDMI and temporary USB input; USB-C is the
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

### Commercial Qt kit and dependency acceptance

Keep the Trixie route adopted in QOR-0025. Compare alternatives explicitly:

| Route | Primary-source support evidence | Unresolved qualification |
|---|---|---|
| Pi 5, stock Raspberry Pi OS 64-bit Desktop Trixie | Raspberry Pi's OS catalogue; native Debian Trixie ARM64 is a supported `rpi-image-gen` host | Qt 6.12's desktop table does not list Trixie; actual entitled kit, native tools, complete ABI and 16-KiB dependencies must be checked |
| Pi 5/8 GB, Ubuntu 24.04 ARM64 | Qt documents this ARM desktop reference, GCC 13 and installer binaries built with glibc 2.39 | Not the adopted Q Pi base or the generator's listed supported native host; OS/kit/page-size/dependency measurements still absent |

Ubuntu is a possible **Qt reference reproduction fallback**, not an automatic
image-builder replacement. If Trixie cannot be qualified, present the evidence
and obtain a decision before changing the base; a changed adopted decision needs
a superseding QOR record. Do not reimage the only card to try it implicitly.
Bookworm's generator support does not make its older libc suitable for an
Ubuntu-built kit. Newer Trixie libc alone does not prove compatibility with the
kit's libstdc++, OpenSSL, Wayland/XCB, Mesa/EGL/GLES, audio/media or plugin stack.

The owner's Enterprise statement is not a kit receipt or distribution grant.
On 11 November, a sanitized local MaintenanceTool inventory confirmed the Windows
MSVC2022 Qt 6.12.0 kit plus Multimedia, Quick 3D/Physics and Shader Tools. The
targeted installed-package query found no Linux ARM64 row; this says nothing
about the account's offered downloads. Browser access was unavailable, so no
account session or entitlement was inspected. The smallest remaining private
owner check is Qt Account's Licenses/Downloads: exact active product (Application
Development or Device Creation) and whether Qt 6.12 Linux ARM64 Desktop is offered.
Share only those facts, never account/licence identifiers or credential files.
Qt documents that [installer components depend on the licence](https://doc.qt.io/qt-6/qt-online-installation.html);
[MaintenanceTool `list`](https://doc.qt.io/qt-6/get-and-install-qt-cli.html)
is installed inventory, not current commercial entitlement. The sanitized
receipt is `build/q-pi-native-2026-11-11/session-02/qt-inventory-review.json`.
Resolve these questions with the owner's existing entitlement records or Qt's
written answer, without uploading credentials or accepting new terms:

1. Which exact **commercial Qt 6.12 patch, Linux GNU ARM64 artifact/package IDs**
   and native ARM64 tools are available under the existing entitlement? Are
   Trixie/Pi 5 and a 16-KiB kernel supported, or must an entitled source build
   and separately measured source-build budget be used? Record provenance and
   hashes, not an assumed `gcc_arm64` directory name.
2. Does the entitlement cover QQ's Core, Gui/private headers, Network, Multimedia,
   Qml/private headers, Quick, Quick Controls, Quick 3D, **Quick 3D Physics**,
   **Spatial Audio**, Test and D-Bus, plus Shader Tools and Wayland client/plugins
   and the **Wayland Compositor** needed by the isolated harness? Confirm native
   `moc`, `rcc`, `qmlcachegen`, `qmltyperegistrar`, `qmllint`, `qmlimportscanner`,
   `qml` and `qsb` as applicable; a runtime-only kit cannot compile/test QQ.
3. What is permitted for this developer's Pi build machine, private application
   deployment, the eventual device image, and **SDK/development tools** shipped
   on it? Runtime redistribution and copying an SDK are separate scopes. Record
   required notices and third-party obligations. Do not infer rights from a
   downloadable file or from module source availability.

The module inventory is checked against `products/qq/CMakeLists.txt`, runtime
and Player private-header dependencies, `Scene.qml`'s Physics import and the
SpatialAudio imports in `Music.qml`, `Sound.qml`, `Voices.qml` and `World.qml`.
Qt documents [Physics](https://doc.qt.io/qt-6.12/qtquick3dphysics-index.html) as
a QML-only API; a successful CMake component search can therefore miss its runtime
absence. [Spatial Audio](https://doc.qt.io/qt-6.12/qtspatialaudio-index.html) is a
separate module, currently labelled Technology Preview. The
[Wayland Compositor module](https://doc.qt.io/qt-6.12/qtwaylandcompositor-index.html)
has its own licensing/dependency requirements. No installer was opened here.

On the native builder, record installed `libc6`, `libstdc++6`, compiler/binutils
and selected graphics/audio package versions with `dpkg-query`, then run the
bounded `--qt-only` gate. Build a manifest covering the selected kit's native
tools, application/runtime files, QML/platform/image/media plugins and helpers;
run the [static payload audit](#static-target-payload-audit). Use trusted
`readelf -lW -dW --version-info` on explicit files to record interpreter,
`DT_NEEDED`, paths and required/provided `GLIBC_*`, `GLIBCXX_*`, `CXXABI_*` versions.
That audit is static and cannot certify actual symbol/loader resolution. After
provenance and entitlement checks, native configure/link/import/launch tests
must demonstrate the target's actual resolution without the host's libraries
leaking into a staging-root scan. QML import scanning/deployment and the real
isolated suites must cover dynamically loaded plugins absent from `DT_NEEDED`.
Never run `ldd` on untrusted supplied binaries. Check **every selected ELF** for
16-KiB load compatibility, including system dependencies and sanitizer runtimes;
linker flags on QOR executables alone do not fix a 4-KiB Qt plugin or loader.

### Static target payload audit

`tools/image/pi_payload.py` checks an explicit target-file inventory on this host
without running ARM64 code, `ldd`, Qt tools or a kit's CMake. It neither walks an
entire SDK nor searches the host for dependencies. Supply an existing ordinary
target-root directory and a UTF-8 JSON manifest with exactly `schema: 1` and
`files`. Each file entry has exactly:

- `path`: a portable, case-distinct POSIX path relative to that root;
- `sha256`: the expected 64 lowercase hexadecimal digits from the reviewed
  build/export receipt, not a fabricated value;
- `role`: `application`, `library`, `plugin`, `helper` or `loader`.

Include every selected executable, Qt/system library, QML/platform plugin and
helper, with each dependency and interpreter explicitly listed. Relative symlinks
may resolve inside the root; an escaping or host-absolute link is refused unless
it actually resolves inside the selected root. The interpreter path embedded in
ELF is matched to a manifest `loader` entry, never opened as a host path. A
manifest of Qt Core alone does not cover a product. Native applications have
been built, but no complete staged deployment manifest is provided here;
selected executable checks do not establish the whole dependency closure.
Use a quiescent staging directory: identity and timestamp checks detect ordinary
concurrent changes, not a hostile writer able to restore that metadata.

```bash
python3 -B tools/image/pi_payload.py --root /qualified/target-root --manifest /qualified/payload-files.json
```

The JSON report records file hashes, ARM64 ELF load layout, interpreter,
`DT_NEEDED`, `DT_SONAME`, `DT_RPATH` and `DT_RUNPATH`. These fields come from
[ELF program headers](https://gabi.xinuos.com/elf/07-pheader.html) and
[dynamic metadata](https://gabi.xinuos.com/elf/08-dynamic.html), including tables
beyond the first MiB. Every needed name must have exactly one candidate among
the listed files; each candidate is checked in turn. Missing, ambiguous,
wrong-architecture, incompatible-layout, changed-hash and malformed inputs fail
with exit 2. Exit 0 means only `static_audit_passed`; `build_qualified` stays
false. The tool writes no target file and produces its report on stdout.

The same audit now reads GNU `DT_VERNEED`/`DT_VERDEF` label tables and their
count tags, following [glibc's structures](https://raw.githubusercontent.com/bminor/glibc/master/elf/elf.h)
and [label matching](https://raw.githubusercontent.com/bminor/glibc/master/elf/dl-version.c).
It validates paired tags, revision, relative chains, counts, mapped/nonoverlapping
records, indices and ELF label hashes. `version_needs` and `version_definitions`
retain observations; `version_label_checks` records the unique dependency candidate
and missing labels. Only the first definition name supplies a label; parents and
base names do not. Exact labels must agree with that candidate: a higher-looking
`GLIBC`/`GLIBCXX` label or a label in another library is insufficient.

This qualification policy is deliberately stricter than glibc's loader: missing
weak requirements and unversioned providers also refuse the audit. Absent version
tables produce empty observations, not a claim that unversioned symbols resolve.
Unsupported forms fail closed. Review a refusal before changing policy; never
delete requirements or omit a dependency to obtain a passing report.

Bounds are 256 KiB of manifest, 512 files, 512 MiB per ELF and 2 GiB aggregate
file content, with an 8-MiB report ceiling. The reader visits at most 4,096
combined version records/auxiliaries, reads at most 16 MiB of metadata and emits
at most 64 KiB of strings per file, counting repeated labels and parents. ELF
dynamic tables and individual strings have further bounds. Larger inputs need a reviewed
policy change, not an unchecked fallback. Use the existing QOR lock and memory
guard when auditing a substantial real payload; the portable tests use tiny
temporary synthetic files.

Name/label agreement is **not actual dynamic-loader resolution or symbol ABI proof**.
The audit does not resolve RPATH/RUNPATH or the target loader cache, inspect
per-symbol `DT_VERSYM` bindings/default visibility or relocations, or select
`dlopen`/QML plugins. It does not certify manifest
completeness, source authenticity, entitlement, hardware or image readiness.
Use [Qt's deployment support](https://doc.qt.io/qt-6.12/qt-deploy-runtime-dependencies.html)
and QML import analysis on the qualified builder for complete selection, then
check actual target resolution and execution. CMake's
[dependency scanner](https://cmake.org/cmake/help/v3.31/command/file.html#get-runtime-dependencies)
can consult host `ldconfig`; a scan on this Windows/WSL host cannot establish
the target's dependency closure.

The local version-label increment and validation status are recorded under
`build/q-pi-abi-2026-11-11/session-01/`; the owner's requested validation
continuations are in adjacent `session-02/` and `session-03/`, and [OWNER](../OWNER.md).
After authorised RAM recovery, all **59 focused ELF/payload tests pass on Windows
Python, zero failures/skips**, at the unchanged candidate source hashes. Original
Windows coordination and the 3,072-MiB guard remain in force. These synthetic
ELF regression cases are not real Qt artifacts or native/Pi acceptance. When
access and approved tooling/kit staging are available, audit the actual selected
ARM64 payload, then perform native loader and Qt/QOR smoke tests separately.

### Native Linux heavy-job coordination

`tools/dev/heavy.py` is the repository's **native Linux counterpart**, not a
replacement for the original Windows `msvcrt` coordinator. Windows and WSL must
continue using the original wrapper and its actual lock. There is no cross-host
lock: an authorised remote launch must respect each host's coordinator when
performing heavy work there. The 11 November native qualification established
the Pi's permanent per-account lock; Windows performed only lightweight SSH
transport while its own heavy-job RAM guard was below the floor.

The native launcher uses one designated ordinary build account's passwd home,
at `.local/state/qor-os/heavy.lock`, independently of checkout, working directory,
`HOME` and `XDG_STATE_HOME`. All native QOR workers must use that same account,
mount namespace and permanent inode. It rejects root, WSL, unsupported/network
mounts, linked files, unsafe ownership/modes and changed directory/lock identities.
The existing local-filesystem probe checks mount metadata, not physical backing.
Do not relocate, replace or delete this lock, change permissions to bypass a
refusal, or run a second coordinator under another account/container. The scope
is cooperative coordination on that host, **not isolation from that UID/root,
coordination across mount namespaces or automatic interception of raw builds**.

Admission takes a nonblocking exclusive OS lock and then calls the unchanged
`vm_resources.require_headroom` with `linux_available_memory`: at least 3,072 MiB
of available RAM, including visible cgroup limits, with no lower-floor option.
It records no argv, PID, credentials or caller-supplied path. Busy exits 75;
invalid/unsafe/unmeasurable/incomplete state exits 2. Insufficient RAM reports
available, required and shortfall bytes. Check-only mode creates no files and
reserves nothing; an uninitialised coordinator refuses it. Resource snapshots
are not reservations; measured scratch, build concurrency and all other guards
remain separate. Child output is inherited unchanged: use only reviewed commands
that already obey the no-secrets-in-logs rule, never secret arguments.

Before spawning, the launcher durably writes a fixed `heavy.active` marker,
passes the locked descriptor to the immediate child and becomes a child
subreaper (the kernel returns orphaned descendants to it). Normal completion
removes only its own marker after the kernel reports no children remain. The
permanent lock is closed, never explicitly unlocked or unlinked. SIGINT/SIGTERM/
SIGHUP send TERM to the launched process group, followed by KILL only if its
direct child remains unreaped after a bounded wait. Once that child is reaped,
no further group signals are sent; surviving descendants retain the marker.
The command deadline defaults to four hours, configurable from
1 to 86,400 seconds; timeout exits 124 after confirmed drain, and interrupted
completion returns 128 plus the signal number. Surviving/detached descendants,
supervisor death or uncertain cleanup retain the marker and block later work.
An inherited descriptor alone cannot account for descendants that close it.

After builder/access/resource qualification, first run this bounded fixture suite
as the designated account from the repository root, preserving stdout/stderr,
exit status, source hashes, Python/kernel identity and resource observations under
`build/`. It needs no Qt and makes no network/device/security changes:

```bash
python3 -B tools/dev/heavy.py --timeout-seconds 120 -- python3 -B -m unittest discover -s tests/tools -p test_heavy.py -v
python3 -B tools/dev/heavy.py --check
```

The suite's Linux cases exercise actual temporary-file locks, inherited
descriptors, child adoption, signals and crash markers in dedicated subprocesses.
Their filesystem qualification is deliberately mocked, and the escaped-child
case uses a controlled deadline after a readiness handshake. Fixtures never use
or remove the production lock. They do not prove production mount/namespace scope,
ARM64 application execution, image boot or hardware. Separately verify the real
account/path/inode and existing QOR workers on the intended host before admitting
an application build. Historical Windows evidence on 10 November is **28 portable
passes, 11 Linux skips**, plus the Windows CLI's expected refusal, under
`build/q-pi-lock-2026-11-10/review-01/`. On 11 November, **80 tests passed with zero
failures/skips on the actual aarch64 Pi**: 39 coordinator cases and 41 resource
cases. The real Linux lock/process cases ran. The production lease held the
unchanged RAM guard, native child supervisor and tests; CLI `--check` passed
afterward. Ten hash-checked Python files (128,941 bytes) remain in a unique owned
`qor-qualification-20261111-*` home directory for evidence. Do not delete its
production `.local/state/qor-os/heavy.lock`, copy the coordinator into another
lock domain, or repeat completed tests without changed code or a new concern.

The initial native RAM failure is preserved: this Pi advertises no cgroup v2
memory controller, so `memory.max/current` legitimately do not exist. The corrected
reader checks bounded controller metadata at every ancestor, reads required
memory counters wherever advertised, and rechecks topology, membership and
mounts. Missing advertised counters, mixed/v1 or hidden layouts and changed
metadata still refuse. There is no swap fallback or reduced floor, and no kernel
setting was changed. This follows the
[kernel's controller interfaces](https://docs.kernel.org/admin-guide/cgroup-v2.html#enabling-and-disabling).
An uncontrolled leaf still inherits any limiting memory ancestor. On this full
v2 hierarchy without that controller, the host's `MemAvailable` constrains admission.

**Interrupted-run recovery:** stop submitting QOR work; retain logs and the
constant marker as evidence. A stale PID/note or a free lock is insufficient:
review the original job and its QOR descendants in the same account/namespace,
and establish that none remain. Never stop unrelated services. If quiescence
cannot be established, keep the builder blocked. Once established, recovery is
a separately reviewed operation holding the existing OS lock and rechecking
directory/lock/marker ownership and inode identity before removing only
`heavy.active`. There is intentionally no force/clear switch or automatic stale
recovery. Never delete `heavy.lock`, and never use an unguarded `rm` as recovery.

The design follows [Linux flock lifetime semantics](https://man7.org/linux/man-pages/man2/flock.2.html),
[child subreaper semantics](https://man7.org/linux/man-pages/man2/PR_SET_CHILD_SUBREAPER.2const.html),
[directory-relative opens](https://man7.org/linux/man-pages/man2/open.2.html) and
[Python subprocess descriptor/session behavior](https://docs.python.org/3/library/subprocess.html).
These explain the implementation; local fixtures and documentation do not
substitute for its native qualification.

### Native QOR application build

`products/qq/CMakePresets.json` now supplies `q-pi-release` and `q-pi-asan` for
the actual QQ sources, with Studio, Player and tests enabled. These use CMake
and Ninja, one compile job, warnings as errors and QOR hardening. The native
host gate runs before compiler discovery, rejecting x86-64, cross compilation
and WSL. Both the maximum and common linker page sizes are 16 KiB: the latter
preserves read-only relocation protection on that kernel
([GNU ld](https://sourceware.org/binutils/docs/ld/Options.html)). Every Qt/plugin/
system dependency still needs its separate ELF layout and ABI checks.

The stock Pi now has authorised restricted access, CMake 3.31.6 and the owner's
Qt 6.12.0 Linux ARM64 kit. On 12 November native parser smokes passed in Release
and sanitizer configurations, and the real Prism's seven selected Release suites
passed after a test-only focus-activation correction. The same focused native
ASan/UBSan suites also pass without skips or sanitizer diagnostics. These builds
exclude the core, use software rendering and do not claim leak checking. QQ's
native Release preset also built Studio, Player and tests without warnings;
16 programs passed hardening/layout checks and six non-rendering cases passed.
Full products, hardware and image acceptance remain pending. Source/kit checks and exact scopes are recorded in
`build/q-pi-connect-2026-11-12/session-01/`. Trixie is still not Qt's listed Pi
reference OS; owner-reported Enterprise entitlement is not a redistribution review.
WSL and its `gcc_64` kit are x86-64; Windows/Android ARM64 kits remain unsuitable.

On 13 November the existing pinned core integration also built on this Pi with
Rust 1.98.1, its `desktop` features and unchanged Cargo.lock. Its C bridge test
passes nine Qt Test results without failures/skips, and the executable passes
hardening and ARM64/16-KiB checks. Rust's `proc-macro-error2` 2.0.1 dependency
warning remains. This focused target does not qualify the complete native OS,
product/deployment closure or image. Receipts:
`build/development-2026-11-13/session-01/native-core-20261113/`.

Later on 13 November the full core-enabled development build completed and all
27 selected non-GPU suites passed after correcting the demonstration fixtures'
missing ARM64 platform metadata. Hardening passes 42 programs and page-layout
checks pass 44 linked executable/library files; QML lint has zero warnings.
QQ's complete scene-data and Player sandbox suites also pass. The Rust dependency
warning remains, and these software checks do not qualify the complete product,
dependency/deployment closure, physical devices or image. The source snapshot,
fixture overlay, initial failures and final results are retained in
`build/development-2026-11-13/session-02/`.

Required native application tools are CMake 3.27+, Ninja, GNU C++, pkg-config,
Python and Git. The kit must include Core, Gui/GuiPrivate, Multimedia, Network,
Qml/QmlPrivate, Quick, Quick 3D/Physics, Spatial Audio, Quick Controls, Test and D-Bus, with
Shader Tools for QML assets. Qualify these through the source's `find_package`
and QML imports, not a guessed desktop-only Qt subset. Commercial entitlement,
glibc/C++ ABI, graphics/audio/input dependencies and Secret Service still need
verification. No installation or licence acceptance is implicit.

Before normal Qt package discovery, the native preset invokes host Python for
`pi_preflight.py --qt-only --qt-target "$QOR_QT_DIR"`. It requires regular root
config/version entry points and matching literal Qt6/Core 6.12 versions, refusing
ambiguous/dynamic versions, non-Linux target metadata and a Qt Core library that
is not shared ARM64 with the required page layout. Conflicting normal, cached or
environment package paths are refused without rewriting those inputs; `Qt6_DIR`
names the checked root config directory. This small check
does not inspect the entire kit or establish the entitlement/dependency gates.
An explicitly supplied CMake toolchain runs before the top-level include; the
check does not sandbox arbitrary user-selected CMake code. Keep such inputs
separately reviewed and pinned.

After source/kit/dependency/resource qualification, inspect actual QOR workers
and qualify [native coordination](#native-linux-heavy-job-coordination). Keep
native jobs serial through `tools/dev/heavy.py`; the original Windows/WSL lock
remains unchanged. Leave execution blocked until the intended host's actual
coordination is verified. Do not disturb a release shell or an unrelated service.

The smallest next native Qt milestone now has a concrete entry point in
[`tests/pi/CMakeLists.txt`](../tests/pi/CMakeLists.txt) and its
[`presets`](../tests/pi/CMakePresets.json). It compiles the **existing** Prism
command-line source and `tst_prismoptions.cpp` in development-option and
device-option variants, using commercial Qt 6.12 Core/Test. The tests use
[QTEST_GUILESS_MAIN](https://doc.qt.io/qt-6/qtest.html#QTEST_GUILESS_MAIN), which
creates a core application without a graphical application. Neither variant
starts a simulator, compositor, display, service, keyring or physical device.
The existing native/static-kit gate is included even for direct `-S` configuration;
hardening, warnings as errors, both 16-KiB linker settings, one build/test job and
one autogen job are retained. The sanitizer preset adds address/undefined checks;
CTest refuses an empty suite and each test has a 30-second timeout. The presets
do not acquire a lock themselves: every command below requires the existing
coordinator, freshly qualified host/resources, measured scratch budget and
reviewed commercial kit/dependencies. No native smoke build has run.

From `tests/pi/`, after those prerequisites:

```bash
python3 ../../tools/dev/heavy.py -- cmake --preset q-pi-smoke
python3 ../../tools/dev/heavy.py -- cmake --build --preset q-pi-smoke
python3 ../../tools/dev/heavy.py -- python3 ../../tools/ci/check_hardening.py ../../build/q-pi-smoke
# Inspect both executables, Qt Core/Test and the loader/dependency ELF closure
# before execution, using the explicit payload audit and reviewed manifest.
python3 ../../tools/dev/heavy.py -- ctest --preset q-pi-smoke
python3 ../../tools/dev/heavy.py -- cmake --preset q-pi-smoke-asan
python3 ../../tools/dev/heavy.py -- cmake --build --preset q-pi-smoke-asan
python3 ../../tools/dev/heavy.py -- python3 ../../tools/ci/check_hardening.py ../../build/q-pi-smoke-asan
# Review the sanitizer runtime/dependency closure too before execution.
python3 ../../tools/dev/heavy.py -- ctest --preset q-pi-smoke-asan
```

Passing would show only these two real QOR parser variants compiled and ran with
that native Core/Test kit. It does not qualify the compositor, complete ABI,
graphics, other Qt modules or an image. Windows regressions for preset parsing
and wrong-host refusal were added but not started: the 11 November admission
refused 2,741 MiB against the unchanged 3,072-MiB minimum. Source review found no
blocking issue. See `build/q-pi-native-2026-11-11/session-02/` for the refusal;
the unchanged 59 ELF/payload fixture passes remain separate historical evidence.

Then exercise bounded QML imports including Physics and SpatialAudio
and the isolated harness prerequisites. Configure the unchanged `q-pi-release` preset and build
`qq-player` for initial QOR feedback, then complete the full Studio/Player/tests,
hardening, zero-warning lint and sanitizer sequence below. No smoke or QOR build
has run yet; do not substitute system Qt or lower the 6.12/module requirements.

Immediately before each heavy phase, run `--bootstrap-only` again for the native
3-GiB check and review the measured application/scratch budget. Use
`tools/dev/heavy.py`, which calls the unchanged native RAM guard while holding
its lock; the no-argument
`tools/image/vm_resources.py` CLI is the Windows guard. Keep build/test concurrency
at one and the existing sanitizer/resource limits. Image-generator namespace
qualification is separate: no `sudo`, capability grant, privileged container or
security relaxation is needed or authorised just to inspect this Pi or compile
an ordinary application. Do not copy the SDK into an image without confirmed scope.

On the qualified builder, with `QOR_QT_DIR` set to that kit, run from
`products/qq/` (the Release build has native evidence; the full product test and
sanitizer sequence below remains a separate gate):

```bash
python3 ../../tools/dev/heavy.py -- cmake --preset q-pi-release
python3 ../../tools/dev/heavy.py -- cmake --build --preset q-pi-release
python3 ../../tools/dev/heavy.py -- cmake --build --preset q-pi-release --target all_qmllint
python3 ../../tools/dev/heavy.py -- python3 ../../tools/ci/check_hardening.py ../../build/q-pi-qq-release/qq
```

Use the existing product harness for hidden display/isolated-keyring tests.
Its new `--preset` option configures the native preset and tests that same
`<build-root>/qq` cache, preserving the preset's sanitizer mode, page sizes and
single-job build. Do not run raw CTest on a builder's real desktop/keyring. The
matching CTest presets are for an already isolated environment:

```bash
# From the repository root; one guarded command at a time, never nested:
python3 tools/dev/heavy.py -- tools/dev/products.sh --product qq --preset q-pi-release --qt "$QOR_QT_DIR" --build-root build/q-pi-qq-release --attempts 1
python3 tools/dev/heavy.py -- tools/dev/products.sh --product qq --preset q-pi-asan --qt "$QOR_QT_DIR" --build-root build/q-pi-qq-asan --attempts 1
python3 tools/dev/heavy.py -- python3 tools/ci/check_hardening.py build/q-pi-qq-asan/qq
# From products/qq after the isolated sanitizer harness:
# python3 ../../tools/dev/heavy.py -- cmake --build --preset q-pi-asan --target all_qmllint
```

The harness requires Qt Wayland Compositor for its hidden display and
`dbus-daemon`, `python3-dbus` and `python3-gi` for its private session/keyring.
It reports the renderer it actually expects; software is not Pi GPU evidence,
and its existing performance floor is not waived. `--preset` refuses other
products and unknown preset names before building or downloading dependencies.
The sanitizer preset must also build and run the hostile-input suites before
acceptance. Real Pi graphics, a local saved/reopened project, image composition
and physical boot remain separate gates. Preserve each command's exit/log and
the exact source/kit/base/renderer identities; no zero-warning or test gate is
waived. Historical WSL/Windows results and the current portable fixture passes
are not ARM64 application results. Run repository hooks for the final source;
native acceptance still requires all relevant builds/tests, hardening and zero
QML lint warnings. Capture failures/skips before choosing an independent next task.

Only after application acceptance, qualified generator inputs/namespace actions
and measured image budget may composition begin. The full read-only image probe
then takes `--source <checkout> --expected-commit <reviewed-40-digit-commit>
--qt-target <kit> --scratch-root <existing-dir> --scratch-budget <measured.json>`.
Its `qualified_image_inputs` blocker is deliberately always false today: a
successful bootstrap or application build does not remove that unfinished gate.
Do not fake receipts or lower it to start an image job. No accepted Q Pi recipe
or generator execution command is offered until those inputs are concrete.

### Q Pi first boot and Bluetooth bootstrap

The current preparation route is **headless Wi-Fi and key-only SSH**, with no
pre-paired Bluetooth, keyboard, mouse or monitor required. USB/HDMI remains an
optional recovery route, not a prerequisite for headless enrolment. This updates
the earlier local-input preference within QOR-0025's already scoped headless
option. **Preparation is authorised; real provisioning/access is not.** The
Samsung card's eventual Q Pi write grant does not approve stock bootstrap,
credential creation, Wi-Fi configuration, enabling SSH or ongoing assistant access.
Normal local console/desktop login remains mandatory, with both autologin modes
off. No headless Pi boot, Wi-Fi association or SSH login has been tested.

#### Supported Trixie provisioning interface

The [Raspberry Pi cloud-init announcement](https://www.raspberrypi.com/news/cloud-init-on-raspberry-pi-os/)
introduced `meta-data`, `network-config` and `user-data` on the FAT boot partition
for Trixie images from 24 November 2025. Review against the **actual image version**,
not just the name Trixie: initial October 2025 images used a different first-run
interface. The current reviewed reference is **Imager v2.0.12**, commit
`84b03d96cd4c390ebe475a4b5db5735e5535e9a6`, whose
[versioned format documentation](https://github.com/raspberrypi/rpi-imager/blob/84b03d96cd4c390ebe475a4b5db5735e5535e9a6/doc/os_customisation_formats.md)
describes `cloudinit-rpi`. No Imager installation or write occurred.

The stock [vendor cloud-init configuration](https://github.com/RPi-Distro/rpi-cloud-init-mods/blob/05b72290f6f6da9343296bed99d3657ff8a3fb1c/etc/cloud/cloud.cfg.d/99_raspberry-pi.cfg)
selects NoCloud data from `file:///boot/firmware`. Networking uses version-2
`network-config`, Netplan and NetworkManager, with the correct Wi-Fi regulatory
country. Do not use a boot-partition `wpa_supplicant.conf`, empty `ssh` file,
legacy `firstrun.sh` or a guessed `userconf` recipe as proof of this interface.
A boot FAT partition on the connected card is not proof these components exist.

A custom Q Pi artifact needs a reviewed `.rpi-imager-manifest` with `os_list`,
`init_format: "cloudinit-rpi"`, Pi 5 device selection, a local `file:///...` artifact
URL and **actual** compressed/extracted sizes and hashes, matching the pinned
[Imager schema/helper](https://github.com/raspberrypi/rpi-imager/blob/84b03d96cd4c390ebe475a4b5db5735e5535e9a6/doc/local_json/create_local_json.py).
An arbitrary local image defaults to no customization. Declaring a format does
not implement it. The pinned `rpi-image-gen` [cloud-init example](https://github.com/raspberrypi/rpi-image-gen/blob/fb44a4cde4ac54847ef1966957149e2c2b716fdf/contrib/src/cloud-init/README.md)
uses separate CIDATA media; its `image.json` is IDP provisioning metadata, not an
Imager manifest. Q Pi must deliberately integrate the boot-FAT datasource, pinned
packages, first-boot ordering and desktop/login policy. There is no accepted
recipe or populated Imager manifest yet. No stock image may be substituted.

#### Per-device accounts, secrets and disabled autologin

After separate action-time provisioning approval, the owner supplies Wi-Fi/country,
a unique ordinary local account/password and its SSH public key through a trusted
local interface. Do not collect these in chat. Private signing keys stay on the
approved Windows side; host private keys stay on the Pi. No shared/default user
credentials, host keys, machine identity, Wi-Fi settings or pairing state enter
the generic image or repository. Per-device seed files and cloud-init caches can
contain credentials/hashes: keep them outside `build/`, prompts and logs; restrict
physical/access exposure and review retention. Deleting seed files is not secure
erasure of flash and must not be claimed as such. Never dump cloud-init user-data,
NetworkManager connection files or whole logs to obtain a diagnostic.

The [released Imager generator](https://github.com/raspberrypi/rpi-imager/blob/84b03d96cd4c390ebe475a4b5db5735e5535e9a6/src/customization_generator.cpp)
uses `user.name`, `user.shell`, `user.lock_passwd: false`, a locally produced
`user.passwd` hash, `user.ssh_authorized_keys`, explicit `user.sudo: null`, and
`ssh_pwauth: false` for the password-required local account/key-only SSH path.
This describes fields, not a runnable seed. `passwd` applies on account creation;
reapplying it does not establish that an existing account changed. Preserve the
admin-password requirement; never inherit passwordless sudo. Do not lock the local
password account as an SSH-hardening shortcut. The released source removed the
older `enable_ssh: true` field because it was ignored; enabling SSH instead emits
`systemctl enable --now ssh` in `runcmd`. **Omit that activation for Q Pi's gated
receipt phase**; a reviewed QOR integration must activate access only after its
matching owner-approved marker and scope. Generic SSH stays disabled.

`ssh_pwauth: false` alone does not prove all authentication policy. Before access,
validate effective `sshd -T` for the selected account/address: public-key-only
`AuthenticationMethods`, root/password/keyboard-interactive/GSSAPI login off,
no agent/X11/port forwarding and only the intended ordinary account/key. Review
an explicit source-address restriction and key expiry for the approved window
using [OpenSSH authorized-key options](https://man.openbsd.org/sshd.8). Expiry
prevents later authentication; it does not terminate an existing session or create
an execution sandbox. The approval must identify the one board/account, client
identity, endpoint/private network, allowed operations and start/expiry window.
An untrusted/unsynchronised Pi clock blocks expiry-based activation; no silent time,
agent or network-setting fix is authorised. Do not silently renew access or run privileged commands.

There is no `cloudinit-rpi` autologin Boolean. Supported
[`raspi-config nonint do_boot_behaviour B3`](https://github.com/RPi-Distro/raspi-config/blob/22cea75ecbcb8e6f6e2e80a7f565d6cfe154bc02/raspi-config)
selects a graphical login while disabling both console and LightDM autologin;
B1 alone does not clear the desktop setting. Running B3 late cannot prove there
was no earlier automatic login. Q Pi must ship both modes disabled and gate
getty/display startup until account setup/policy checks complete. The reviewed
[cloud-init Raspberry Pi account hook](https://github.com/canonical/cloud-init/blob/29d50e12104e264f8881d3abe0f7acc6f3e442b2/cloudinit/distros/raspberry_pi_os.py)
and [rename-user helper](https://github.com/RPi-Distro/userconf-pi/blob/2dec0368f79b084d9def4e4cfb0cf0720d3ff30c/rename-user)
can enter first-run wizard/autologin setup. Their actual image versions and ordering
must be qualified before declaring this policy satisfied. No setting was changed.

#### First host identity without local input

Prefer native per-device host-key generation over putting a private host key in
cloud-init data. Stock Trixie's [generation service](https://github.com/RPi-Distro/raspberrypi-sys-mods/blob/15fa44812659b8cfab0b8605ac2b525c1277ff51/debian/raspberrypi-sys-mods.regenerate_ssh_host_keys.service)
uses `ConditionFirstBoot=yes`, removes existing host keys, then runs `ssh-keygen -A`
before SSH/first-boot completion; it has no automatic restart policy. Vendor
cloud-init defaults leave deletion/generation to that service. An interrupted
first boot can therefore regenerate identity. The generator's separate SSH layer
has different lifecycle/defaults and enables SSH/password authentication unless
explicitly changed. Select **one** generation owner; do not combine them blindly.
[systemd machine identity](https://www.freedesktop.org/software/systemd/man/latest/machine-id.html)
must also be unique on first boot and preserved afterward; never publish its value.

The proposed QOR enrolment sequence is deliberately independent of Wi-Fi success:

1. During later approved personalization, retain a public per-device UUIDv4,
   unique `qpi-...` hostname and the verified artifact digest in a separate Windows
   write record. Supply that public binding to the future first-boot hook. The
   generic image cannot contain its own final whole-image hash. A receipt echoes
   that binding; it does not authenticate provenance by itself.
2. Start with remote access gated off. Complete account/network configuration,
   login-policy checks and `first-boot-complete.target`. Record the public host
   key, then perform a controlled stability reboot. If generation or provisioning
   failed, stop enrolment; a finished service process alone is not success.
3. After reboot, require the same instance binding and public key. Only then
   publish `qor-headless-host.json` to the selected boot FAT and gracefully power
   off. The record contains only schema/kind, instance UUID, hostname, image
   digest, Ed25519 **public** host key and its SHA-256 fingerprint. No address,
   SSID, password, username, private key, machine ID or log dump is exported.
4. After confirmed shutdown, the owner moves that exact card into this Windows
   reader. Verify the public receipt against the independently retained write
   record, including a separate hostname comparison, and preserve its public key/fingerprint in a dedicated pin file outside
   the repo. This physical chain of custody is the trust channel, not a network
   announcement. A changed/missing/malformed receipt or uncertain card stops the
   process. Hostname matching and `ssh-keyscan` cannot replace this trust step.
5. Only after a separate account/key/network/access-window approval, personalize
   a non-secret access marker bound to that instance, image digest and pinned
   fingerprint; then return the card and boot. The native gate must validate it
   and the effective SSH/login policy before opening access. The marker is not a
   secret or remote authentication mechanism; physical-media trust and owner
   approval remain essential. First access performs only the bounded identity
   probe before any separately scoped build command.

This is a **QOR design built on supported primitives**, not a turnkey upstream
feature. Hook installation/order, stable-reboot checks, public-receipt publication,
shutdown, marker consumption/expiry and Wi-Fi-independent recovery are **not yet
integrated or qualified in an image**. Do not remove/power-cycle the card merely
because a timer elapsed. Until graceful shutdown is demonstrated, stop and use
an expressly approved physical recovery route. The physical receipt exchange
requires no keyboard, mouse, Bluetooth pairing or local monitor.

#### Implemented preparation and Windows assistant access

`tools/image/pi_headless.py` implements bounded public receipt validation,
SHA-256 public-key fingerprint calculation and an offline Windows OpenSSH argument
plan. It reads no private key and has no network, SSH execution, service or block
device operation. `publish_receipt()` is a library helper for later hook integration,
tested only against temporary ordinary directories. Exclusive creation never
replaces an existing entry: identical receipts can be reused; changed or partial
receipts fail. It is **not** an atomic FAT/power-loss durability guarantee. A failed
flush is not completion; native persistence and shutdown remain separate gates.
Inputs are at most 8 KiB, reject duplicate/unknown fields, malformed keys and unsafe
paths; path ancestors must be trusted and quiescent, not concurrently controlled
by a hostile writer. Symlinks/reparse points at selected files/directories fail.
Receipt construction strips public-key comments so account/email text is not
exported. Validation of a received receipt instead refuses a commented key as
noncanonical. The helper never claims a successful boot or authenticated access.

Read-only public-receipt check, with public values from the independent record:

This helper compares the UUID and image digest, and an optional prior fingerprint.
It validates hostname syntax but does not compare it with the independent record;
the reviewer must perform that comparison separately.

```powershell
.venv/Scripts/python.exe -B tools/image/pi_headless.py --receipt <public-receipt.json> --instance-id <recorded-uuid> --image-sha256 <recorded-image-digest>
```

The real Windows client was observed at
`C:/Windows/System32/OpenSSH/ssh.exe`: **OpenSSH_for_Windows_9.5p2**, LibreSSL 3.8.2.
[Microsoft supports](https://learn.microsoft.com/en-us/windows-server/administration/openssh/openssh-overview)
this client for SSH management of Linux. This assistant's local command tool can
invoke that existing client after approval; an online chat or a VS Code sidebar
alone confers no Pi access. No Windows SSH server, firewall change, tunnel,
forwarded port, Raspberry Pi Connect or persistent agent setup is needed by this
client route. Availability/configuration parsing is not a successful network login.

The plan CLI adds `--fingerprint <independently-pinned-fingerprint>`, `--address
<one-private-IPv4>`, `--user <approved-account>`, `--identity-file <literal-Windows-path>`
and `--known-hosts-file <literal-Windows-path>`. It returns **argv data**, not a shell
script, and never executes it. Do not feed it to `Invoke-Expression` or concatenate
it into a command string. A real plan contains the supplied identity-file path:
keep it transient, or remove private paths before retaining sanitised evidence.
A later approved invocation must verify the pin file
contains exactly the generated public line and apply a 30-second outer timeout.
The prepared first command is fixed and fails on a failed subcommand: architecture,
kernel page size and numeric user ID. `verify_probe_output()` also requires a zero
exit and exactly ARM64/16384/non-root results, with a 1-KiB output bound; banners,
extra lines and wrong values fail. This checks output only, not the authenticity
of the transport. It does not install or configure anything. IPv6/public endpoints require
a separately reviewed extension; this candidate accepts one RFC1918 IPv4 address.

The generated options disable user/system SSH configuration (`-F none`), proxy
commands/jumps, local commands, agent/port/X11 forwarding and connection sharing.
They require the pinned Ed25519 key with `StrictHostKeyChecking=yes`, no automatic
key update/DNS trust, public-key authentication only, one connection attempt and
no interactive prompts. Paths are literal absolute Windows paths; expansion
syntax and additional trust-file injection are rejected. Read-only `ssh -G`
validation tests argument interpretation, **not** reachability or authentication.
The plan disables agent use: an approved credential must already be usable by the
client without a prompt. An encrypted key requiring unlocking remains an access
blocker; do not ask for its passphrase in chat or silently enable/store it in an
agent. Any owner-controlled temporary signing/unlock arrangement needs its own
approval and a reviewed plan change. Do not inspect existing private-key files.

#### Discovery, retries and recovery

For the approved private LAN, try only the preselected unique hostname through
normal `.local` resolution, or have the owner identify the exact Pi in the router's
DHCP lease view. Treat both as address **candidates**. Record one candidate and
verify it is in the approved network; do not scan subnets, enumerate unrelated
devices, change router settings or invent an address. Multicast DNS may fail or
collide on Windows/network equipment; use the owner-confirmed lease instead of
opening firewall ports or installing a discovery service. An optional, approved
single-endpoint TCP-22 check proves reachability only. Authentication needs the
physically obtained host pin **and** the specifically authorised client identity.

One bounded SSH attempt follows pin/credential approval. A key mismatch stops
immediately; never use `StrictHostKeyChecking=no`, `accept-new`, blind `ssh-keyscan`
trust, or deletion of known-host entries to retry. Wrong account/key is not fixed
by allowing passwords/root. Preserve only sanitised phase/exit/timing metadata;
do not record seed data, passwords, private paths/values or broad debug logs.
A successful probe establishes that scoped session only; future actions and access
renewal remain subject to the approved purpose/window.

| Failure/interruption | Required behavior and recovery |
|---|---|
| Before bootstrap completes | Keep SSH closed, retain the same instance seed; inspect sanitised phase state locally through the approved recovery method. Do not assume cloud-init retries every failed module. |
| Stability reboot changes the host key | Stop before publication/access. Investigate generation ownership and first-boot completion; do not replace a pinned identity automatically. |
| Partial/failed receipt publication or flush | Do not signal completion or open access. Keep the partial evidence; explicit recovery of that public receipt is separate from changing host keys. |
| Receipt complete but marker absent | Reboots preserve identity and remain closed. Owner can repeat the physical public-receipt retrieval; no new credentials or shared identity are created. |
| Wi-Fi unavailable | Bounded wait, then stop; check country, supported band, credentials, signal and AP isolation privately. The qualified offline receipt/shutdown path must still work. |
| Hostname unresolved | Use only the owner's exact DHCP lease/candidate. No broad discovery or firewall workaround. |
| SSH timeout/refusal | Distinguish no network, closed service, incomplete provisioning and expired/unapproved access. Preserve the failure; no retry loop or automatic enablement. |
| Lost pin, changed host key or reimage | Re-establish trust through the physical channel and new explicit approval; do not reuse a stale marker. |

Practical fallbacks are an owner-approved Ethernet connection to the existing
private router (no bridge/Internet sharing or router changes), returning the
powered-off card to the reader for reviewed recovery, or optional HDMI/temporary
USB input. Ethernet does not bypass SSH authentication or host-key trust. If
neither safe shutdown nor network access is available, report that physical
recovery is required rather than guaranteeing a zero-input rescue. Do not run
`cloud-init clean`, alter instance ID, delete host keys, reapply password hashes,
reimage or relax login settings as an automatic repair. Cloud-init's
[first-boot/cache rules](https://docs.cloud-init.io/en/latest/explanation/first_boot.html)
make those operations potentially identity-changing. Bluetooth can be paired later
under approval; it plays no role in the initial headless trust/access path.

### Stop conditions and the next owner actions

Stop the affected phase for wrong/unknown board or OS, absent approved access/recovery,
unverified image/media/required recovery, broken normal login or unexpected remote access,
unstable power/cooling/storage, non-16-KiB kernel or dependency, insufficient or
unmeasured resources, uncoordinated workers, unentitled/incomplete Qt, unresolved
ABI/plugins, unapproved package/namespace changes, or dirty/unreviewed build
source. Do not retry heavy jobs or change guards to mask these conditions.
Continue independent repository/research work where possible.

The smallest next actions are: confirm the Pi's current boot/OS and power/cooling,
the intended private Wi-Fi and physical card-return/recovery route (no credentials
in chat); choose preservation or separately approved stock bootstrap if needed;
and confirm the exact commercial Qt artifact/module and
device/SDK scope. No password, token, remote account or purchase is needed in
chat. Once headless integration is qualified, separately approved per-device
provisioning and authenticated access or an optional local console can produce
the real package delta, RAM and storage measurements. Only **after those receipts
exist** request the exact installation/native-job actions. The Samsung card is
already the authorised eventual Q Pi target; any needed off-card builder recovery
or alternative media arrangement is resolved before its action-time write gates.

Keep evidence under a new `build/` attempt: source/patch hashes, bootstrap JSON,
sanitised package proposal, kit identities, capacities, commands/exits, failures
and skips. Preserve the earlier `q-pi-qualification-2026-11-10/session-01`
606-pass/70-skip Windows run and its initial corrected-driver failure; these
portable results do not become ARM64 evidence. Do not overwrite failed attempts
or personal backups. Repository edits are reversible without touching original
Prism changes. An eventual package installation is not assumed reversible by
uninstalling: preserve its approved recovery path first. Reimaging the only card
requires the off-card preservation/recovery path above for any later builder
state that must survive; the owner has already authorised discarding old Kali/data.

### Authorised Q Pi target and action-time write gates

The owner's 10 November update, also recorded as planning metadata in
`pi/preparation.json`, selects the plugged-in Samsung EVO nominal 128-GB microSD
for the **eventual Q Pi image**, including old Kali/data erasure. Do not ask again
for that broad intent. This is not a completed write receipt, proof of current
device identity, approval of an unready image, or permission to substitute a
stock builder image. If stock bootstrap is needed first, prepare its concrete
artifact, purpose, package/access boundaries and media/recovery decision for
separate approval **before** writing it.

Immediately before an eventual Q Pi write:

1. Reidentify the exact attached card from current **capacity and USB reader/device
   identity**, corroborating the physical card and layout. Match the intended
   Samsung target, not merely the last `Disk 2`, `D:` or BOOT label. Verify it is
   neither a system nor a currently booted disk, and that no builder is running
   from it. Do not inspect or operate unrelated devices to guess the target.
   Ambiguous readers/identities, changed capacity or uncertain system/boot status
   require stopping, not selecting the closest-looking disk.
2. Verify the selected **Q Pi** artifact's provenance and immutable source/base/
   package/Qt receipts; recompute its recorded checksum immediately before use.
   Check Pi 5/ARM64 boot assets, 16-KiB target/dependency compatibility, partition
   bounds and expanded write size against the freshly observed capacity. Review
   native application/build evidence, static image validation and the first-boot
   plan. Hardware acceptance follows boot testing; it cannot be claimed from
   these pre-write checks. A stock OS, a partial image or unverified input is not
   an authorised substitute. Today the image and these receipts do not exist.
3. Preserve build evidence and any necessary recovery material **off the target**.
   Old Kali/data on this card is authorised for discard; do not invent a requirement
   to retain it. Confirm separately any later builder state/data that must survive.
   Present the exact current device identity/capacity, image identity/hash/size and
   irreversible erasure scope for the required **action-time confirmation**. The
   owner's update expressly retains that confirmation and other applicable safety
   checks. If the device or image changes afterward, stop and revalidate/reconfirm.
4. After confirmation, use the approved writer for that verified target only.
   Flush the completed write and use its verified read-back or an equivalent full
   written-image-range comparison against the verified uncompressed image.
   Record the verification method, covered byte range, exit/status and result;
   a writer exit 0 or visible BOOT partition alone does not prove the write.
   A failed or interrupted write remains unaccepted: preserve logs and stop for
   diagnosis, without a blind retry against a remembered disk number.
5. Report the exact artifact/device identification, checksum, write/read-back
   outcomes and any errors/skips. Keep **write verified** separate from **Pi boot,
   login and hardware accepted**. Account credentials, pairing/network provisioning
   and security/remote-access changes remain separately gated.

This is a procedure only. No writer was launched, card content read, device
reconfigured or irreversible-erasure confirmation requested in this update.

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

Only after the scoped Samsung-target authorisation and all
[action-time write gates](#authorised-q-pi-target-and-action-time-write-gates)
are satisfied, or equivalent separate authorisation for different test media:
verify the Pi 5/ARM64 identity and kernel page size; complete real
login; test HDMI/scaling, hardware rendering, Wi-Fi, Bluetooth reconnection at
login, input, audio, USB/Ethernet, power/cooling and shutdown. Open a native QOR
app, save/reopen a local project and compile/run a small native program, offline
as well as online. Test interrupted setup, three reboots with persistent state
and a 30-minute workload/idle check with measured resource use. Record failures
and unavailable features. No ARM64 build, image assembly, static image validation,
Pi boot or hardware test has passed yet. The existing VM proof below is separate.

### Pi kernel WAIT_BO backport review

This is a source review candidate, prepared 16 November at QOR commit `4937e1a`.
It is not a bootable kernel, an installed fix, vendor qualification or Q Pi
acceptance. Preparation authorizes no installation, module loading or reboot.
The artifact manifest is [pi/kernel-wait-bo/manifest.json](pi/kernel-wait-bo/manifest.json).
The original frame deadline/assertion and separate software-renderer failure
remain unchanged. See [the measured kernel defect](../docs/PERFORMANCE.md#exact-running-kernel-wait-audit-2026-11-16).

#### Review contents and provenance

| Artifact | Purpose |
| --- | --- |
| [Patch 1](pi/kernel-wait-bo/0001-v3d-preserve-zero-timeout-poll.patch) | Two-line production change: zero remains a poll; positive conversion and subsequent handling stay unchanged. |
| [Patch 2](pi/kernel-wait-bo/0002-v3d-wait-bo-kunit.patch) | Separate, default-off kernel regression with six cases. |
| [Test configuration](pi/kernel-wait-bo/kunit.config) | Overlay for a separate KUnit test kernel, never the production configuration. |
| [Series](pi/kernel-wait-bo/series) | Review order; production uses patch 1 only. The negative control uses patch 2 only. |

Base: [Raspberry Pi Linux `cff533aec2fa601846766b32ff57204e0a61bed7`](https://github.com/raspberrypi/linux/tree/cff533aec2fa601846766b32ff57204e0a61bed7),
declared by installed source package `linux` `1:6.18.50-1+rpt1` for running
`6.18.50+rpt-rpi-2712`. Config SHA256:
`8914d802785a04fbb68a00e88e90774db7aa2ec33b9fa1e28f3296be3ebbcff8`.
Compiler observed: Debian GCC 14.2.0-19, AArch64; pages 16 KiB; timer `HZ=250`.

The fix is a deliberately narrow semantic backport of upstream Linux
[`226666556a9b`](https://github.com/torvalds/linux/commit/226666556a9b3b51f04f9dad83662e5b9c78a325).
It bypasses the existing conversion only for zero. It does not copy the shared
helper migration or its separate overflow changes, so it requires no new DRM
core symbol/header. Review positive timeout behavior against the unchanged
helper and elapsed/error branches. The kernel changes retain GPL licensing;
the new kernel test is GPL-2.0-only. Nothing here changes the QOR product licence.

The two patches have zero context. **Exact preimage and patch hash checks are
mandatory**, not optional offset matching. In a new, clean kernel checkout,
verify the full HEAD equals the manifest's base commit, all listed preimage
SHA256 values match, all required-absent paths are absent, and both patch hashes
match. Refuse any mismatch. Then use `git apply --check --unidiff-zero` before
applying the selected patches with `git apply --unidiff-zero`. After the full
test series, verify every postimage hash. For production, only `v3d_bo.c` changes;
the Kconfig/Makefile/test source additions are not production prerequisites.

Keep the full vendor source/package provenance when producing a kernel: the
small hash-matched source fixture used for object checks is not a complete
source/package reproduction. Downloaded build dependencies must have reviewed
repository signatures, immutable versions and hashes before use. No dependency
installation or full kernel download occurred in this preparation.

#### What the regression protects

The main KUnit case supplies a private GEM object, a synthetic handle table and
a real unsignaled DMA fence. It calls the actual `v3d_wait_bo_ioctl`, GEM lookup,
reservation iterator and default waiter. Sixteen zero-timeout probes must return
ETIME, leave the fence unsignaled and cause no voluntary context switch in the
calling task. Elapsed nanoseconds are evidence only, not a new speed threshold.
Involuntary preemption has a separate counter. An unpatched kernel is expected
to expose its timed sleep; this negative control must actually run before the
test is accepted as demonstrated regression coverage.

The remaining cases cover an idle zero poll, busy positive expiry, already
signaled positive success, invalid padding and invalid handle. All six names
must appear in complete KTAP results. Missing, skipped, aborted or unexecuted
cases are not passes. The fixture performs no GPU work and does not test real
userspace ioctl dispatch, hardware buffer allocation or pixels. Separate
integration checks must exercise a valid busy V3D buffer, positive completion
while blocked, interruption/restart and buffer lifetime. Already-signaled
success is not asynchronous-completion coverage.

#### Isolated builder qualification: blocked on prerequisites

Read-only qualification at QOR commit `645e35f` (16 November, session-07)
reconfirms the exact running source/config identities above. GCC 14.2.0-19,
binutils 2.44-3, make 4.4.1-2, bc, OpenSSL development headers, pahole, cpio
and dynamic BusyBox are installed. **bison, flex and QEMU are not.**
libelf-dev is also absent; whether it is needed must follow the final build features, not a generic dependency list.
The stock config has `DEBUG_INFO_NONE=y`, with no enabled OBJTOOL/BTF option.
The known source fixture contains selected files only; `/usr/src` supplies
headers/kbuild, not a complete pinned kernel source tree. Windows has none of
the checked ARM64 compiler/QEMU/parser tools on PATH and no running WSL
distribution. No stopped environment is started as a workaround.

Existing bubblewrap 0.12.0 and unprivileged user namespaces work. A bounded
probe creates private process/network/mount namespaces, drops capabilities,
binds tools read-only and writes only an ephemeral private work directory.
The probe confirms absence of `/boot`, the user's home, DRM/input/audio/KVM
devices and desktop sockets; only loopback exists in its network namespace.
This qualifies the namespace mechanism, **not a complete builder or VM**.
No dependency download/install, package-index refresh, host setting, service,
kernel/module load or Pi reboot occurs. Admission reports 7,281 MiB available
against the unchanged 3,072-MiB floor; these are inventory snapshots, not
sustained admission with a kernel compiler or VM resident.

Unprivileged `apt-get --simulate --no-install-recommends install bison flex
qemu-system-arm` against existing metadata reports **13 additions, zero
upgrades/removals**. Adding libelf-dev would make 15 additions. Neither
simulation installs anything. Observed candidate versions:

| Missing prerequisite | Candidate from existing metadata |
| --- | --- |
| bison | `2:3.8.2+dfsg-1+b2` |
| flex | `2.6.4-8.2+b4` |
| libelf-dev (conditional build dependency) | `0.192-4` |
| qemu-system-arm | `1:10.0.13+ds-0+deb13u1` |

This simulation describes the current Pi's dependency delta, not the complete
closure of a fresh root filesystem. Metadata versions/hashes are recorded,
but package bytes, signature chains and a frozen builder root are not yet
verified. Do not turn the simulation into a host install command.

The minimal proposed setup is an **owner-authorized private ARM64 Debian
Trixie builder root** under a new QOR `build/` directory. Provision bison, flex
and qemu-system-arm with their complete signed, version/hash-pinned closure,
with GCC 14.2.0-19 and the existing build prerequisites. Add libelf-dev only
if the reviewed build requires it. Do not run package scripts against the
host root. Verify the root's dependency closure and
tool versions; mount it read-only for builds, exposing only dedicated source,
output and evidence paths. No host home, boot, device or service passthrough.
Keep provisioning separate from the offline build/test namespace. A reviewed
builder image with the same verified contents is an alternative, not an
already available or approved artifact.

After provisioning, acquire and verify the complete pinned source and vendor
package provenance. Retain `/boot/config-6.18.50+rpt-rpi-2712` by its pinned
hash; use separate unpatched/test-patch-only and corrected/two-patch trees.
Preserve ARM64, 16-KiB pages, HZ250 and `CONFIG_PREEMPT=y` in both. Review
every `olddefconfig` difference; test-only KUnit and unique local versions are
explicit differences from the stock config, never an exact-stock-config claim.
Keep `DRM_V3D=m`: the current test overlay links the suite into `v3d.ko`, so
booting a kernel alone does **not** execute it. A private initramfs must carry
that kernel's module dependency closure, a verified userspace loader and its
runtime libraries, and load V3D inside the guest. Confirm the suite actually
appears in KTAP before interpreting results. Do not load it on the live Pi.

Use a disposable ARM64 QEMU `virt` guest with software emulation (TCG), one
CPU and initially 1,024 MiB guest RAM, no network, graphics, audio, host disk
or KVM passthrough. Pin the available machine/CPU model after inspecting the
installed QEMU; verify support for 16-KiB pages by actual guest boot. PL011
serial, GIC, initramfs and ELF support are present in the observed stock
config; this is not proof the complete config boots on `virt`. No Pi firmware
or hardware is emulated/qualified by this test. The
[QEMU platform documentation](https://www.qemu.org/docs/master/system/arm/virt.html)
describes this distinction. Record any necessary guest-only config differences
and apply them identically to both controls; stop for review if they affect
the wait/preemption path. Do not switch to an unrelated default KUnit config.

The outer original heavy coordinator owns serial compilation and the VM.
Require 30 one-second resident admission samples and continuous 3,072-MiB
headroom; a snapshot cannot reserve it. Size source/output/package scratch
before starting. Record fixed build/guest time limits before execution; a
timeout is an incomplete run, never the expected defect. Preserve complete
console/KTAP, command/config/tool hashes, elapsed time, memory/CPU samples and
exit/timeout status. The first control must complete before running the patch.
Kernel sanitizer/lock checks follow only after the focused pair is valid.

#### Regression review and actual run matrix

All six cases are **not run** on both unpatched and patched kernels in
session-07: prerequisites are unavailable. There is no KTAP output, runtime,
case skip or test timeout to report. Do not convert blocked cases into passes
or claim an observed negative-control failure. Session-06 object compilation
is separate and was not repeated.

| Case | Unpatched | Patched |
| --- | --- | --- |
| `wait_busy_poll_does_not_sleep` | Not run | Not run |
| `wait_idle_poll_succeeds` | Not run | Not run |
| `wait_busy_positive_expires` | Not run | Not run |
| `wait_idle_positive_succeeds` | Not run | Not run |
| `wait_invalid_padding_is_rejected` | Not run | Not run |
| `wait_invalid_handle_is_rejected` | Not run | Not run |

Static review against the exact target confirms the fixture's handle refers
to its private GEM object and the real WRITE fence is included by the READ
reservation query used by `wait_all=true`. The unsignaled fence is not filtered
out. Its absent `enable_signaling` callback does not signal it automatically;
its absent custom waiter selects `dma_fence_default_wait`. The original zero
converts to one jiffy and reaches `schedule_timeout`; the patch keeps zero and
returns busy without that sleep. The voluntary-switch samples bracket only the
handler, excluding KUnit logging. Keep PREEMPT, HZ and page size unchanged:
voluntary-preemption configurations can introduce unrelated scheduling into
`might_sleep`. This path review supports the fixture but does not execute it.

Only the busy-poll case is expected to distinguish old/new code. Accept its
negative control only if setup succeeds, the fence stays unsignaled, every
poll returns ETIME with remaining timeout zero, and the failure is specifically
the voluntary-switch assertion. Other failures, warnings, allocation errors,
missing cases, module-load errors or guest timeouts invalidate that conclusion.
If attribution remains uncertain, capture one guest-only scheduler/call-path
trace before changing any code; do not relax the assertion. Expect the other
five cases to pass on both versions, but no such result is claimed yet.

The production patch still changes only the zero conversion. Positive
rounding/clamping, interruptible waits, remaining-time subtraction and
ETIME-to-EAGAIN handling are identical. Padding fails before waiting; invalid
handles return EINVAL; negative restart errors propagate through the unchanged
handler. Fence completion error status is not newly interpreted. Existing
coverage includes positive expiry and already-signaled success, **not**
completion while blocked, restart signals, large/clamped timeouts or raw
userspace dispatch. Those remain explicit integration/review gaps. No patch,
test assertion, timeout or product behavior changes in session-07.

Evidence: `build/development-2026-11-16/session-07/`, especially
`builder-recovery-inventory.json`, `sandbox-qualification.json`,
`fence-path-source.json` and `regression-results.json`. Session-06 is preserved.

#### Pre-install qualification sequence

1. Use a new isolated kernel build environment with the original heavy-job
   coordinator and `tools/image/vm_resources.py`: one job, serial compilation,
   sustained resident admission and at least 3,072 MiB available throughout.
   Keep all outputs under a new `build/` receipt. Never stop unrelated work.
   The current Pi lacks bison, flex and QEMU; its running kernel has KUnit off.
   Qualify those prerequisites in an isolated builder before attempting a full
   build or kernel-test VM. Do not enable KUnit or load a test module in the
   current desktop kernel as a workaround.
2. Prepare three separate source/output trees from the pinned base: unpatched
   control plus patch 2; corrected test plus patches 1 and 2; production plus
   patch 1 only. Copy the verified 16-KiB configuration into each output tree.
   Give each a distinct `LOCALVERSION`, preserving the base suffix and adding
   a clearly recorded trial suffix. Check the resulting `kernelrelease` differs
   from every installed release. Do not predict it from a filename. The Pi
   [kernel build guide](https://www.raspberrypi.com/documentation/computers/linux_kernel.html)
   documents the 2712 configuration and distinct kernel versions.
3. Run `olddefconfig` in the separate output directory, inspect every config
   change, and refuse an unintended page-size, graphics, security or service
   change. Production has the original config apart from the unique version;
   test changes include the explicit KUnit overlay. Build/link kernel, modules
   and device trees with `-j1`, warnings reviewed, and record source/config,
   compiler, kernel/module build IDs, dependency closure and artifact hashes.
   A changed object alone is insufficient. Stage modules under a private
   `INSTALL_MOD_PATH`, never `/lib/modules` on the live Pi.
4. In a disposable kernel-test VM, run only `v3d-wait-bo` first. Capture the
   unpatched busy-poll failure, then all six patched passes with no skips.
   Preserve errors and complete KTAP. Repeat the focused corrected tests with
   kernel address checking (KASAN) and lock checking in a separate test config;
   record actual support, failures and skips. No such execution has occurred.
5. After those gates and a verified recovery route, submit the exact candidate
   hashes/config, proposed boot selection and rollback target for installation
   approval. A custom build is still a trial, not vendor-supported acceptance.
   Do not replace a loaded `v3d.ko`, unload the desktop driver or mix modules from
   different kernel releases.
6. Following separately authorized candidate boot, verify actual release,
   module identities, 16-KiB pages and V3D/ES/Qt backend again. Run the unchanged
   focused Agent test in the existing render-node-only sandbox. Compare matched
   callback/phase data and complete images; retain the 97 unresolved outliers.
   Keep compositor/cache conditions constant and disclose kernel/config changes.
   Qualify real ioctl completion/interruption/lifetime behavior. Resume QOR
   Release, sanitizer and broader gates only on actual focused resolution.
   Restore the recorded baseline after the bounded trial; preserve all evidence.

#### Rollback plan: distinguish running from next boot

Read-only inventory finds `auto_initramfs=1`, `arm_64bit=1`, no explicit kernel
override, and no `tryboot.txt` or `autoboot.txt`. Current normal firmware files
match **6.18.60**, although **6.18.50 is running**:

| Preserved file | SHA256 |
| --- | --- |
| `/boot/firmware/config.txt` | `90a446821551a9df109ec1a77a79f6518fe974517d982d371c5e9efd6bbc98c3` |
| `kernel_2712.img` / 6.18.60 vmlinuz | `f5b98801c4ec2eef5fbc2f4261a353d642d4a059e31231f418434ef2fffcc83d` |
| `initramfs_2712` / 6.18.60 initrd | `f921995b146241c8c25111031a000af30bdbf4a6f1c6bbbbdfef55b7a8eb091e` |
| `/boot/vmlinuz-6.18.50+rpt-rpi-2712` | `154f0ea1e14bb1075077dfd1bdc63c1502bc0b17b0354628d515b4fbd3a4198d` |
| `/boot/initrd.img-6.18.50+rpt-rpi-2712` | `063222c943130886b15020c8e736e2c9d7418342f847f13fb8a805d31d0a3e7b` |

Session-07 verifies this actual machine is a **Raspberry Pi 5 Model B Rev 1.1**,
booting from SD: `/dev/mmcblk0p1` is FAT at `/boot/firmware` and `p2` is the
ext4 root. The current user can read but not write the boot directory. Firmware
reports `57db150d`, built 5 November 2025; bootloader properties are retained.
These observations do not prove tryboot, watchdog recovery or media access.

Both stock initramfs listings contain only their respective module release.
The old package retains 411 readable DTB/overlay files under
`/usr/lib/modules/6.18.50+rpt-rpi-2712/dtb/`; the new one retains 415.
The normal Pi 5 DTB matches 6.18.60 (SHA256
`03ce4742452ad52e3e8ec2259a0c705b496970e2b75e6e0b4e5446475ea1b926`),
whereas the old Pi 5 DTB is
`a5165cb562035f479ea9f5a81cedf7b6596ea32a0598608c4dc3b77ada909c60`.
The inspected V3D Pi 5 overlay matches both releases. Thus retaining the old
kernel/initramfs alone does not establish a matched old boot selection.
Per-file hashes are in the private inventory; whole-module-tree integrity,
offline backup readback and recovery boot have not been demonstrated.

The practical recovery route is physical SD access with a card reader and a
spare verified medium, plus a Linux-capable recovery environment for the ext4
root/module files. Before a trial, preserve the original card, prepare a
complete matched stock recovery set on the separate medium, and verify its
actual stock boot, desktop and remote access. A person at the Pi must be able
to select that medium if the guest-independent physical trial cannot boot.
Neither a reader/spare card nor physical access was verified this session.
Windows can edit the FAT selection file, as the
[Pi configuration guide](https://www.raspberrypi.com/documentation/computers/config_txt.html)
describes, but this alone does not recover the ext4 module set. Do not format,
clone, change boot order or attempt tryboot until the exact media and operation
are separately approved. The verified result here is retained component
availability and the mismatch, **not a tested recovery mechanism**.

These hashes identify current artifacts, not a tested rollback. A normal reboot
is not a return to the running 6.18.50 baseline. Before any trial:

- Re-inventory at action time. Preserve the whole boot selection/config/include
  chain, kernel, matching initramfs, DTBs/overlays and versioned module tree in
  a private verified backup, including permissions. Keep credentials and any
  private command-line data out of logs/repository. Do not overwrite or remove
  either installed stock kernel. Verify that the recovery set is internally
  matched; do not pair the older kernel with newer unreviewed device trees.
- Prefer a separate recovery/test medium. Prove its selected stock kernel,
  desktop, networking and remote access boot successfully **before** trialing
  the candidate. Returning specifically to 6.18.50 requires that matched
  recovery set and a verified boot; merely finding its files is insufficient.
  Alternatively qualify stock 6.18.60 as a new baseline first, then rebase and
  repeat the controlled comparison; do not call that a same-baseline rollback.
- Stage candidate kernel, matching initramfs/device trees/overlays and modules
  under unique names/directories on the test medium. Raspberry Pi's
  [boot configuration documentation](https://www.raspberrypi.com/documentation/computers/config_txt.html)
  describes separate OS prefixes and initramfs selection. Verify the complete
  selected set: prefix fallback must not silently boot stock artifacts. An
  optional one-shot tryboot route needs firmware/support and recovery testing;
  it is not qualified here and cannot guarantee recovery from a hung kernel.
- Arrange local console/media access and a person able to power-cycle/reselect
  recovery media if networking fails. Do not depend solely on Tailscale or Pi
  Connect for recovery from a nonbooting kernel. Define a bounded boot window
  before starting; failure to regain access triggers the recovery procedure,
  not another unattended retry.

On trial failure or completion, preserve diagnostics, stop only the trial's
workers, and return to the preverified recovery selection/media. If the trial
cannot boot, recover offline from the other computer/console. Restore the exact
recorded selection files rather than guessing settings. After boot, verify
`uname -r`, page size, loaded-module build IDs, desktop/remote access and stock
file hashes against the recovery receipt. Leave candidate files quarantined
until review; removing them is not part of rollback. Record the actual result.
No installation, backup-copy, boot selection, reboot or recovery drill was
performed by this preparation.

#### Verified preparation, remaining gates

The native guarded job passes both patch checks/applications, verifies patched
hashes, compiles original and changed `v3d_bo.o` plus the KUnit object with `W=1`
and `-Werror`, inspects the compiled zero bypass, then reverses both patches and
verifies exact original hashes. For the test object only, the compile command
defines `CONFIG_KUNIT=1`; the installed config remains unchanged. There is no
full link, KUnit execution, test-module load or bootable artifact. All 61 memory
samples exceed the resource floor; minimum 7,153 MiB.

Receipts: `build/development-2026-11-16/session-06/`. Full kernel build/link,
unpatched/patched and KASAN kernel tests, busy hardware ioctl integration,
recovery drill, original QOR focused test/images and all acceptance gates remain
unrun in this preparation. PR #10 stays draft. The next concrete action is to
qualify an isolated kernel-test builder and run the unpatched negative control
and patched KUnit suite before proposing installation.

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
retry loop. Earlier host-service orchestration is historical and superseded by
the current QOR-only resource policy. Never restore or rerun retired cross-project
wrappers or inspect/manage unrelated services to satisfy this guard.

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
hits. All launch RAM checks retain the 3-GiB minimum. The recorded run ended with
the VM shut down; this audit did not launch it or refresh host-service state. The earlier
`feed626` proof, both normal UI failures and all diagnostic outputs remain separate.

QOR RAM-preflight failures and resume boundaries remain dated evidence. The
accepted continuation used the same installed disk without a reset. Old
cross-project orchestration is not a current prerequisite or a reusable procedure;
current work uses only QOR's existing lock/resource guard. The earlier checked proof is
`v12b-installation-proof/full-sequence-result.json`. Actual screenshots, immutable
input records, native failures and the exact resume history live in
`build/vm-install-2026-11-06/`. The later actual-UI result is the separate
8 November accepted receipt named above. These ignored artifacts are private development
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

This subsection preserves the initial installer checkpoint. Later private VM
installation and reboot receipts are documented in
[the disposable VM lab](#private-disposable-vm-lab); the earlier absences below
must not be read as a current inventory of completed work.

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

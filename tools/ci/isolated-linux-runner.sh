#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#
# The inside of tools/ci/isolated-linux-runner.ps1: runs as root in the qor-ci WSL distribution. Read that script first.
#
#   isolated-linux-runner.sh provision        wsl.conf, packages, the runner user, Rust, the seal and the job sandbox
#   isolated-linux-runner.sh restore-binfmt   puts back what a first start took from the kernel's one binfmt_misc table
#   isolated-linux-runner.sh runner-install   second stage: the Actions runner, unregistered
#   isolated-linux-runner.sh runner-once ...  second stage: one registered, ephemeral job (token on stdin)
#
# What this file is for, from what was measured on 1 November 2026 under WSL 2.7.11, kernel 6.18:
#
# /etc/wsl.conf is not the boundary. With automount and interop off, a process with no privilege still opened AF_VSOCK to
# the host on port 50002, where Windows' 9P file server accepted an attach to C:\ as the owner's account: everything on
# the drive, with no mount and no /mnt/c. Every distribution in the VM has that door, and no setting closes it.
#
# So a job runs in a sandbox the kernel enforces, whose filter refuses AF_VSOCK (qor-ci-job, below). The boundary is that
# sandbox. `wsl -d qor-ci -u runner` is not in it: only someone already on Windows can start one, and
# check_runner_boundary.sh fails there on purpose.
#
# Traps in what the distributions share. They have their own mount, pid, ipc and uts namespaces, and share the kernel,
# the user namespace and the network namespace:
#   - /dev is ONE devtmpfs for every distribution: a file made in it here appears in the other. Never chmod or rm a node;
#     bind over it, which stays in this namespace.
#   - /mnt/wsl is one tmpfs, propagated to every distribution. Make it private here before unmounting it.
#   - uid 1000 here would be the same kernel identity as uid 1000 there. So runner has its own.
#   - Loopback is one: 127.0.0.1 here reaches a listener in the other distribution. Measured, on a test's own socket.
#     A firewall rule would change the other distribution too, so the sandbox's filter is per-cgroup instead.
#   - binfmt_misc is ONE table in the kernel, and the other distribution's Windows programs run by its WSLInterop entry.
#     Measured on 1 November 2026: this distribution, with interop off, emptied it twice over, and cmd.exe stopped working
#     in the other one with "Exec format error". systemd-binfmt flushes the table when it starts, and WSL only protects
#     that unit where interop is on. And with this distribution's mount of the table in place, stopping the distribution
#     emptied it even with that unit masked; with the mount gone, it did not. Why is not known: that is what was measured.
#     So the unit is masked and the seal unmounts the table. Never run `systemctl stop systemd-binfmt`.
#   - systemd's other units at boot act on the same shared things: sysctl on the kernel, networkd on the one eth0, udev on
#     the one /dev. They are masked too. A distribution's first start cannot be stopped from doing this, as it starts
#     before anything can be put in it: restore-binfmt repairs that.

set -euo pipefail

readonly RUNNER_UID=2001
readonly RUST_VERSION=1.98.1
readonly RUST_TARGET=x86_64-unknown-linux-gnu
# The published hashes, each compared with Rust's own channel manifest for this version when pinned.
readonly RUSTC_SHA256=e974f036b28565f37c0f3bd92ddefa809bee16c04f9dcf07b9ed96e05aaaf7c4
readonly CARGO_SHA256=ea1de9f9e23107d97ee2b41a72c552f34064a593da503789218387aee59f3ba4
readonly RUST_STD_SHA256=fa3ff450172a16c026944030230c5069947af93c728d9179971d44e5e0cfb561
# The version of the existing runner, and the hash GitHub's release notes and its asset digest both give.
readonly ACTIONS_RUNNER_VERSION=2.338.1
readonly ACTIONS_RUNNER_SHA256=08e9ccb3604d2462c24b5e33604dc30810296ca15cc65f23cffd4317eb144c37

say() { printf '== %s\n' "$*"; }

write_wsl_conf() {
    say "wsl.conf"
    cat > /etc/wsl.conf << 'EOF'
# qor-ci: tools/ci/isolated-linux-runner.sh. Necessary, and not enough on its own: read that file's top.
[automount]
enabled=false
mountFsTab=false
ldconfig=false

[interop]
enabled=false
appendWindowsPath=false

# Takes away the two 9P mounts of Windows' driver store under /usr/lib/wsl. /dev/dxg stays: the seal covers it.
[gpu]
enabled=false
appendLibPath=false

[boot]
systemd=true

[user]
default=runner

# WSL's is a link into /mnt/wsl, made again at each session. The seal writes a real file instead.
[network]
generateResolvConf=false
EOF
    rm -f /etc/ld.so.conf.d/ld.wsl.conf
}

# First, before this distribution can start again: read the traps at the top. Each of these acts on what is shared.
mask_shared_units() {
    say "no unit here that writes to what the distributions share"
    systemctl mask --quiet systemd-binfmt.service proc-sys-fs-binfmt_misc.automount proc-sys-fs-binfmt_misc.mount \
        systemd-sysctl.service systemd-modules-load.service \
        systemd-networkd.service systemd-networkd.socket systemd-networkd-wait-online.service \
        systemd-resolved.service systemd-timesyncd.service \
        systemd-udevd.service systemd-udevd-control.socket systemd-udevd-kernel.socket systemd-udev-trigger.service \
        systemd-udev-settle.service
}

# From stdin, the lines the other distribution's own boot registers. Only touches the table by a private mount of its own,
# which goes again. The mount is the same table the kernel has for everybody: that is the point.
restore_binfmt() {
    local table name line
    table=$(mktemp -d)
    mount -t binfmt_misc binfmt_misc "$table"
    mount --make-private "$table"
    for f in /usr/lib/binfmt.d/*.conf; do
        [ -e "$f" ] || continue
        name=$(sed -n 's/^:\([^:]*\):.*/\1/p' "$f" | head -n 1)
        if [ -n "$name" ] && [ -e "$table/$name" ]; then echo -1 > "$table/$name"; fi
    done
    if [ ! -e "$table/WSLInterop" ] && [ ! -e "$table/WSLInterop-late" ]; then
        echo ':WSLInterop:M::MZ::/init:FP' > "$table/register"
    fi
    while IFS= read -r line; do
        line=${line%$'\r'}
        case "$line" in
            :*)
                name=${line#:}
                name=${name%%:*}
                if [ ! -e "$table/$name" ]; then printf '%s\n' "$line" > "$table/register"; fi
                ;;
        esac
    done
    say "the kernel's binfmt_misc table: $(find "$table" -mindepth 1 -maxdepth 1 -printf '%f ' | sed 's/register //; s/status //')"
    umount "$table"
    rmdir "$table"
}

install_packages() {
    say "packages (what acts for a user goes first: cron or polkit would run a job's work outside its sandbox)"
    export DEBIAN_FRONTEND=noninteractive SUDO_FORCE_REMOVE=yes
    apt-get update -qq
    apt-get purge -y -qq sudo cron anacron at snapd cloud-init polkitd pkexec openssh-server openssh-client \
        landscape-client ubuntu-pro-client wsl-pro-service unattended-upgrades apport ubuntu-wsl > /dev/null
    apt-get autoremove -y -qq --purge > /dev/null
    # docs/BUILDING.md's list for WSL2 and build.yml's, without Weston: nothing in `ctest -LE gpu` starts it.
    apt-get install -y -qq --no-install-recommends \
        build-essential cmake ninja-build git python3 pkg-config ca-certificates curl xz-utils file binutils \
        libgl1-mesa-dev libegl1-mesa-dev libgles-dev libdrm-dev libgbm-dev libvulkan-dev libgl1-mesa-dri \
        libxkbcommon-dev libxkbcommon-x11-dev libxkbcommon-x11-0 libfontconfig1-dev libfreetype-dev libdbus-1-3 \
        libdbus-1-dev libwayland-dev wayland-protocols libinput-dev libxcb-cursor0 libxcb-icccm4 libxcb-keysyms1 \
        libxcb-shape0 libpulse0 libssl-dev > /dev/null
    # Ubuntu's cmake ships a throwaway signing key for Windows Store apps. Nothing here needs it, and taking it away keeps
    # check_runner_boundary.sh's search free of exceptions.
    find /usr/share -name 'Windows_TemporaryKey.pfx' -delete
}

make_runner_user() {
    say "the runner user: uid $RUNNER_UID, no password, no sudo, no group but its own"
    if id -u runner > /dev/null 2>&1; then
        if [ "$(id -u runner)" != "$RUNNER_UID" ]; then
            pkill -u runner || true
            groupmod -g "$RUNNER_UID" runner
            usermod -u "$RUNNER_UID" -g "$RUNNER_UID" runner
            chown -R runner:runner /home/runner
        fi
    else
        groupadd --gid "$RUNNER_UID" runner
        useradd --uid "$RUNNER_UID" --gid runner --create-home --shell /bin/bash runner
    fi
    usermod --groups "" runner
    passwd --lock runner > /dev/null
    passwd --lock root > /dev/null
}

strip_setuid() {
    say "no program that raises privilege"
    find / -xdev \( -perm -4000 -o -perm -2000 \) -type f -exec chmod u-s,g-s {} + 2> /dev/null || true
    if command -v getcap > /dev/null; then
        getcap -r /usr /opt 2> /dev/null | awk '{print $1}' | xargs -r -n1 setcap -r
    fi
}

install_rust() {
    if [ "$(/opt/rust/bin/rustc --version 2> /dev/null | awk '{print $2}')" = "$RUST_VERSION" ]; then
        say "Rust $RUST_VERSION is in /opt/rust"
        return
    fi
    say "Rust $RUST_VERSION, each part checked by SHA-256, into /opt/rust (root's, so a job cannot change it)"
    local work part
    work=$(mktemp -d)
    while read -r part sha; do
        local file="$part-$RUST_VERSION-$RUST_TARGET.tar.xz"
        curl --proto '=https' --tlsv1.2 -sSf "https://static.rust-lang.org/dist/$file" -o "$work/$file"
        echo "$sha  $work/$file" | sha256sum --check --quiet
        mkdir "$work/$part"
        tar -xJf "$work/$file" -C "$work/$part" --strip-components=1
        "$work/$part/install.sh" --prefix=/opt/rust --disable-ldconfig > /dev/null
    done << EOF
rustc $RUSTC_SHA256
cargo $CARGO_SHA256
rust-std $RUST_STD_SHA256
EOF
    rm -rf "$work"
    /opt/rust/bin/rustc --version
}

install_seal() {
    say "the seal: WSL puts these back at every start, so a unit takes them away at every start"
    install -d -m 0755 /etc/qor-ci /usr/local/libexec
    cat > /usr/local/libexec/qor-ci-seal << 'EOF'
#!/bin/bash
# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
# Takes what WSL mounts into every distribution out of this one's mount namespace. Nothing here reaches another
# distribution: read the trap about /mnt/wsl in tools/ci/isolated-linux-runner.sh before changing the order.
set -u
readonly MOUNTS="/proc/sys/fs/binfmt_misc /mnt/wsl /mnt/wslg /tmp/.X11-unix /usr/lib/wsl/lib /usr/lib/wsl/drivers"

# Private first, or an unmount here would propagate to every peer.
for m in $MOUNTS; do
    if mountpoint -q "$m"; then mount --make-rprivate "$m"; fi
done

# What WSL says about DNS is in /mnt/wsl, and /etc/resolv.conf must not lean on it. A link left by a first start goes.
if [ -s /mnt/wsl/resolv.conf ]; then
    content=$(cat /mnt/wsl/resolv.conf)
    rm -f /etc/resolv.conf
    printf '%s\n' "$content" > /etc/resolv.conf
fi

for m in $MOUNTS; do
    while mountpoint -q "$m"; do umount -R -l "$m" || break; done
done

# WSL's door to the GPU, which froze this laptop three times (docs/CI.md): /dev/null, as tools/dev/products.sh's fence
# recognises. A bind, never a chmod: /dev is one filesystem across distributions.
if [ -c /dev/dxg ] && [ "$(stat -c %t:%T /dev/dxg)" != 1:3 ]; then mount --bind /dev/null /dev/dxg; fi

# /init serves Windows' programs on these sockets, whatever wsl.conf says. WSL's own processes here are root's.
if [ -d /run/WSL ]; then chmod 0700 /run/WSL; fi

status=0
for m in $MOUNTS; do
    if mountpoint -q "$m"; then
        echo "qor-ci-seal: $m is still mounted" >&2
        status=1
    fi
done
if [ -c /dev/dxg ] && [ "$(stat -c %t:%T /dev/dxg)" != 1:3 ]; then
    echo "qor-ci-seal: /dev/dxg is still the GPU" >&2
    status=1
fi
exit "$status"
EOF
    chmod 0755 /usr/local/libexec/qor-ci-seal

    cat > /etc/systemd/system/qor-ci-seal.service << 'EOF'
[Unit]
Description=Seal qor-ci off from Windows (tools/ci/isolated-linux-runner.sh)
DefaultDependencies=no
After=sysinit.target
Before=basic.target

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/usr/local/libexec/qor-ci-seal

[Install]
WantedBy=basic.target
EOF
    systemctl enable --quiet qor-ci-seal.service
    # WSL's generator asks for this, to bring WSLg's X socket back over /tmp.
    systemctl mask --quiet wslg.service
    # Nothing under runner starts a manager of its own: it would run a job's requests outside the sandbox.
    systemctl mask --quiet user@"$RUNNER_UID".service
    rmdir /mnt/c /mnt/x 2> /dev/null || true

    # An abstract socket held outside any job, so that check_runner_boundary.sh can prove a job cannot reach one. Nothing
    # ever reads it: a job's connect is refused, and a connect sends nothing even from a plain session.
    cat > /etc/systemd/system/qor-ci-canary.socket << 'EOF'
[Unit]
Description=An abstract socket for the boundary check to fail to reach

[Socket]
ListenDatagram=@qor-ci-outside

[Install]
WantedBy=sockets.target
EOF
    cat > /etc/systemd/system/qor-ci-canary.service << 'EOF'
[Unit]
Description=Never needed: see qor-ci-canary.socket

[Service]
Type=oneshot
ExecStart=/bin/true
EOF
    systemctl enable --quiet qor-ci-canary.socket
}

install_sandbox() {
    say "the job sandbox"
    cat > /etc/qor-ci/sandbox.conf << 'EOF'
# Every job runs under these, by qor-ci-job and by the second stage's runner. One list, in systemd's own words.
User=runner
Group=runner
WorkingDirectory=/home/runner

# The door. AF_VSOCK is how an unprivileged process reached Windows' file server. Only these families may be created.
RestrictAddressFamilies=AF_UNIX AF_INET AF_INET6 AF_NETLINK
# io_uring can open a socket without calling socket(), past that filter. A 32-bit call goes by another route too.
SystemCallFilter=~io_uring_setup io_uring_enter io_uring_register
SystemCallArchitectures=native
# Not ptrace: LeakSanitizer needs it, and Yama and Landlock limit it to a job's own children.
SystemCallFilter=~@mount @module @reboot @swap @raw-io @obsolete @cpu-emulation @clock
# What the one shared kernel has most to fear from a process with no privilege, and no build or test needs.
SystemCallFilter=~bpf perf_event_open userfaultfd add_key request_key keyctl
SystemCallErrorNumber=EPERM

# No way up, and no way out of the filter.
NoNewPrivileges=yes
CapabilityBoundingSet=
AmbientCapabilities=
RestrictNamespaces=yes
RestrictSUIDSGID=yes
RestrictRealtime=yes
LockPersonality=yes
KeyringMode=private
ProtectKernelTunables=yes
ProtectKernelModules=yes
ProtectKernelLogs=yes
ProtectControlGroups=yes
ProtectClock=yes
ProtectHostname=yes
ProtectProc=invisible

# What it can see. PrivateDevices leaves no /dev/dxg, /dev/vsock or disks. /run holds D-Bus, systemd's sockets and the
# interop sockets: an empty one means nothing outside the sandbox can be asked to start anything.
PrivateDevices=yes
PrivateTmp=yes
ProtectSystem=strict
ReadWritePaths=/home/runner
TemporaryFileSystem=/run:ro
InaccessiblePaths=-/mnt -/init -/usr/lib/wsl -/root -/boot
BindReadOnlyPaths=/etc/qor-ci/job-resolv.conf:/etc/resolv.conf

# The network, by cgroup, so it holds for this unit alone: the other distribution shares this network namespace. A job
# reaches the internet, not Windows' services through the NAT gateway nor the other distribution's loopback. WSL's DNS
# address is shut too: it is local, so whatever listens on every address in the namespace answers on it, as a listener
# there did when it was open. So a job's DNS is public (job-resolv.conf). One to a line: systemd-run takes these names only
# as a whole value.
IPAddressDeny=localhost
IPAddressDeny=link-local
IPAddressDeny=multicast
IPAddressDeny=10.0.0.0/8
IPAddressDeny=100.64.0.0/10
IPAddressDeny=172.16.0.0/12
IPAddressDeny=192.168.0.0/16
IPAddressDeny=fc00::/7

# One machine (docs/CI.md).
CPUQuota=800%
MemoryHigh=66%
MemoryMax=75%
TasksMax=8192

Environment=PATH=/opt/rust/bin:/opt/qt/6.12.0/gcc_64/bin:/usr/local/bin:/usr/bin:/bin
Environment=QOR_QT_DIR=/opt/qt/6.12.0/gcc_64
Environment=CMAKE_BUILD_PARALLEL_LEVEL=8
Environment=CARGO_BUILD_JOBS=8
Environment=LANG=C.UTF-8
Environment=XDG_RUNTIME_DIR=/tmp/runtime-runner
EOF

    # The resolvers a job uses: public, for the reason in sandbox.conf. One place to change them.
    cat > /etc/qor-ci/job-resolv.conf << 'EOF'
nameserver 1.1.1.1
nameserver 8.8.8.8
EOF

    cat > /usr/local/libexec/qor-ci-scope << 'EOF'
#!/usr/bin/python3 -I
# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Runs a command that can reach no abstract Unix socket nor signal any process outside itself and its children.

Abstract sockets belong to the network namespace, and qor-ci shares that with the other distribution. Fails closed.
"""
import ctypes
import os
import sys

SYS_LANDLOCK_CREATE_RULESET = 444
SYS_LANDLOCK_RESTRICT_SELF = 446
LANDLOCK_CREATE_RULESET_VERSION = 1
SCOPE_ABSTRACT_UNIX_SOCKET = 1
SCOPE_SIGNAL = 2
PR_SET_NO_NEW_PRIVS = 38


class Ruleset(ctypes.Structure):
    _fields_ = [("handled_access_fs", ctypes.c_uint64), ("handled_access_net", ctypes.c_uint64), ("scoped", ctypes.c_uint64)]


def main():
    libc = ctypes.CDLL(None, use_errno=True)
    abi = libc.syscall(SYS_LANDLOCK_CREATE_RULESET, None, 0, LANDLOCK_CREATE_RULESET_VERSION)
    if abi < 6:
        sys.exit(f"qor-ci-scope: this kernel's Landlock is ABI {abi}, and scoping needs 6. Running nothing.")
    ruleset = Ruleset(0, 0, SCOPE_ABSTRACT_UNIX_SOCKET | SCOPE_SIGNAL)
    fd = libc.syscall(SYS_LANDLOCK_CREATE_RULESET, ctypes.byref(ruleset), ctypes.sizeof(ruleset), 0)
    if fd < 0 or libc.prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) or libc.syscall(SYS_LANDLOCK_RESTRICT_SELF, fd, 0):
        sys.exit(f"qor-ci-scope: Landlock refused, {os.strerror(ctypes.get_errno())}. Running nothing.")
    os.close(fd)
    os.makedirs(os.environ.get("XDG_RUNTIME_DIR", "/tmp/runtime-runner"), mode=0o700, exist_ok=True)
    os.execvp(sys.argv[1], sys.argv[1:])


if __name__ == "__main__":
    main()
EOF
    chmod 0755 /usr/local/libexec/qor-ci-scope

    cat > /usr/local/sbin/qor-ci-job << 'EOF'
#!/bin/bash
# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
# As root, from outside: wsl -d qor-ci -u root --exec /usr/local/sbin/qor-ci-job COMMAND... runs it as runner, in the sandbox, and answers
# with its exit code. Refuses if the seal does not hold.
set -euo pipefail
if [ "$#" -eq 0 ]; then
    echo "usage: /usr/local/sbin/qor-ci-job COMMAND [ARGUMENT...]" >&2
    exit 2
fi
/usr/local/libexec/qor-ci-seal
properties=()
while IFS= read -r line; do
    case "$line" in '' | '#'*) continue ;; esac
    properties+=(-p "$line")
done < /etc/qor-ci/sandbox.conf
exec systemd-run --quiet --wait --pipe --collect --service-type=exec --unit="qor-ci-job-$$" "${properties[@]}" \
    -- /usr/local/libexec/qor-ci-scope "$@"
EOF
    chmod 0755 /usr/local/sbin/qor-ci-job

    cat > /usr/local/libexec/qor-ci-prove << 'EOF'
#!/bin/bash
# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
# In the sandbox, as runner: build.yml's Linux steps, on the commit in ~/qor-os.bundle. A bundle, because qor-os is private
# and nothing here holds a credential. demiurge-chain and Corrosion come from GitHub, anonymously, as they do in CI.
set -uo pipefail
commit=$1
export CTEST_OUTPUT_ON_FAILURE=1
cd "$HOME" || exit 1
git init --quiet qor-os
cd qor-os || exit 1
git fetch --quiet ../qor-os.bundle HEAD || exit 1
git checkout --quiet --detach "$commit" || exit 1
echo "== Commit $(git rev-parse HEAD), with $(g++ --version | head -n 1), $(cmake --version | head -n 1), $(rustc --version)"
step() {
    echo "== $*"
    "$@"
    local code=$?
    echo "== exit $code: $*"
    if [ "$code" -ne 0 ]; then failed="$failed [$*]"; fi
    return "$code"
}
failed=""
step cmake --preset ci-linux || exit 1
step cmake --build --preset ci-linux || exit 1
step python3 tools/ci/check_hardening.py build/ci-linux
step cmake --build --preset ci-linux --target all_qmllint
step ctest --preset ci-linux -LE gpu
if [ -n "$failed" ]; then
    echo "== Failed:$failed"
    exit 1
fi
echo "== Everything passed"
EOF
    chmod 0755 /usr/local/libexec/qor-ci-prove
}

provision() {
    mask_shared_units
    write_wsl_conf
    install_packages
    make_runner_user
    strip_setuid
    install_rust
    install_seal
    install_sandbox
    say "provisioned"
}

# ---------------------------------------------------------------------------------------------------------------------
# Second stage. Registers nothing until runner-once is given a token, and this repository's scripts run it only when
# isolated-linux-runner.ps1 -Action Register is asked to.

runner_install() {
    say "the Actions runner $ACTIONS_RUNNER_VERSION, checked by SHA-256, unregistered, kept in /opt/actions-runner"
    local file="actions-runner-linux-x64-$ACTIONS_RUNNER_VERSION.tar.gz" work
    apt-get install -y -qq --no-install-recommends libicu74 liblttng-ust1t64 libkrb5-3 zlib1g > /dev/null
    if [ ! -x /opt/actions-runner/bin/Runner.Listener ]; then
        work=$(mktemp -d)
        curl --proto '=https' --tlsv1.2 -sSfL \
            "https://github.com/actions/runner/releases/download/v$ACTIONS_RUNNER_VERSION/$file" -o "$work/$file"
        echo "$ACTIONS_RUNNER_SHA256  $work/$file" | sha256sum --check --quiet
        install -d -m 0755 /opt/actions-runner
        tar -xzf "$work/$file" -C /opt/actions-runner --no-same-owner
        rm -rf "$work"
    fi
    /usr/local/sbin/qor-ci-job /opt/actions-runner/bin/Runner.Listener --version
}

# One job, from an empty home: the token from stdin, never an argument here, never printed. --ephemeral makes GitHub
# take the registration away after one job, and so the next needs a new token, which is why this is called in a loop
# from Windows, where gh is, and never holds a credential in here. Nothing persists: the next job meets a new home.
runner_once() {
    local url=$1 name=$2 labels=$3 token
    IFS= read -r token
    token=${token%$'\r'}
    if [ -z "$token" ]; then
        echo "runner-once: no token on stdin" >&2
        return 2
    fi
    find /home/runner -mindepth 1 -delete
    cp -a /opt/actions-runner /home/runner/actions-runner
    chown -R runner:runner /home/runner/actions-runner
    # shellcheck disable=SC2016 # expanded by the job's bash, with the token read from its own stdin.
    printf '%s\n' "$token" | /usr/local/sbin/qor-ci-job bash -c \
        'IFS= read -r t; cd "$HOME/actions-runner" && exec ./config.sh --unattended --ephemeral --disableupdate --replace --url "$1" --token "$t" --name "$2" --labels "$3" --work _work > /dev/null' \
        config "$url" "$name" "$labels"
    unset token
    /usr/local/sbin/qor-ci-job /home/runner/actions-runner/run.sh
}

case "${1:-}" in
    provision) provision ;;
    restore-binfmt) restore_binfmt ;;
    runner-install) runner_install ;;
    runner-once)
        shift
        if [ "$#" -ne 3 ]; then
            echo "usage: isolated-linux-runner.sh runner-once URL NAME LABELS < token" >&2
            exit 2
        fi
        runner_once "$@"
        ;;
    *)
        echo "usage: isolated-linux-runner.sh provision | restore-binfmt | runner-install | runner-once URL NAME LABELS" >&2
        exit 2
        ;;
esac

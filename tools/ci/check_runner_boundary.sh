#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#
# Can a CI job here reach the owner's files or Windows? Run as the job would run, from outside:
#
#   wsl -d qor-ci -u root --exec /usr/local/sbin/qor-ci-job /usr/local/libexec/qor-ci-check
#
# It exits non-zero and names each failure. A run in a plain `wsl -d qor-ci -u runner` fails, on purpose: that session is
# not in the sandbox, and has the door to Windows' file server that the sandbox exists to close
# (tools/ci/isolated-linux-runner.sh). Passing says what was tried, and no more: it does not make this a VM per job.
#
#   check_runner_boundary.sh                  every check, on this machine
#   check_runner_boundary.sh --offline        without the check that a job can reach the internet
#   check_runner_boundary.sh --files-only ROOT HOME
#                                             only what is found by name, under ROOT, with HOME as the home. For this
#                                             script's tests: it says nothing about a machine.
#
# It looks for keys by path alone and prints only paths: it never opens a file it finds. On the network it knocks only on
# what it listens on itself, and sends the NAT gateway one empty datagram that the filter should stop. It connects to
# Windows' file server and says nothing to it. It only looks at /dev/dxg, never opens it: GPU use from WSL froze this
# machine.

set -u

failures=()
passes=0

pass() {
    passes=$((passes + 1))
    printf 'pass  %-30s %s\n' "$1" "$2"
}
fail() {
    failures+=("$1")
    printf 'FAIL  %-30s %s\n' "$1" "$2"
}
# pass or fail by whether a list of what was found is empty.
none_of() {
    local name=$1 about=$2 found=$3
    if [ -z "$found" ]; then
        pass "$name" "$about"
    else
        fail "$name" "$about: found $(printf '%s' "$found" | tr '\n' ' ' | cut -c1-400)"
    fi
}

# ---------------------------------------------------------------------------------------------------------------------
# What is found by name. Each takes ROOT, which is / on a machine, and HOME.

# Credentials a job could use, by the name the tools that write them give them.
find_credentials() {
    local home=$2 path
    for path in .ssh .aws .azure .gnupg .config/gh .config/gcloud .config/git/credentials .npmrc .pypirc .netrc \
        .git-credentials .docker/config.json .cargo/credentials .cargo/credentials.toml .local/share/Qt/qtaccount.ini \
        .qt/qtaccount.ini .env; do
        if [ -e "$home/$path" ] || [ -L "$home/$path" ]; then printf '%s\n' "$home/$path"; fi
    done
}

# Everything find may walk under ROOT: the filesystems a job's files are on, and not what the kernel makes up.
walk() {
    local root=${1%/}
    find "${root:-/}" \( -path "$root/proc" -o -path "$root/sys" -o -path "$root/dev" \) -prune -o "${@:2}" -print 2> /dev/null
}

find_account_file() {
    walk "$1" -iname 'qtaccount*.ini' -readable
}

# Keys, by path alone. The Store's have no exception anywhere (tools/store/build_store.py names them). A key by a general
# name may be a dependency's own test data, which holds nothing of the owner's, so those trees alone are skipped.
find_store_keys() {
    walk "$1" \( -name store-signing -o -name .qor-os -o -name .qtos -o -name store.pem -o -name 'developer-*.pem' \) -readable
}
find_private_keys() {
    local root=${1%/}
    find "${root:-/}" \( -path "$root/proc" -o -path "$root/sys" -o -path "$root/dev" \
        -o -path '*/.cargo/registry' -o -path '*/_deps' -o -path '*/target' -o -path "$root/usr/share/doc" \) -prune -o \
        \( -name id_rsa -o -name id_dsa -o -name id_ecdsa -o -name id_ed25519 -o -name '*.ppk' \
        -o -iname '*private*key*' -o -iname '*private*.pem' -o -name '*.p12' -o -name '*.pfx' \) -type f -readable -print 2> /dev/null
}

check_files() {
    local root=$1 home=$2
    none_of no-credentials "no credential of the tools a job might find" "$(find_credentials "$root" "$home")"
    none_of no-qt-account "no Qt account file anywhere readable" "$(find_account_file "$root")"
    none_of no-store-key "no Store signing key or its folder anywhere readable" "$(find_store_keys "$root")"
    none_of no-private-key "no private key by name anywhere readable" "$(find_private_keys "$root")"
}

if [ "${1:-}" = --files-only ]; then
    if [ "$#" -ne 3 ]; then
        echo "usage: check_runner_boundary.sh --files-only ROOT HOME" >&2
        exit 2
    fi
    echo "Only what is found by name, under $2. This says nothing about a machine."
    check_files "$2" "$3"
    if [ "${#failures[@]}" -gt 0 ]; then
        printf 'FAILED: %s\n' "${failures[*]}"
        exit 1
    fi
    printf 'All %d passed.\n' "$passes"
    exit 0
fi

OFFLINE=no
if [ "${1:-}" = --offline ]; then OFFLINE=yes; fi

if ! command -v python3 > /dev/null; then
    echo "FAIL  python3 is missing, so nothing below could be tested"
    exit 1
fi
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# Runs a short Python probe. Its last line says what happened; it exits 0 if the door is shut.
probe() {
    python3 -I -c "$1" "${@:2}" 2>&1 | tail -n 1
    return "${PIPESTATUS[0]}"
}

echo "As $(id -un) (uid $(id -u)), on $(uname -r), at $(date -u +%Y-%m-%dT%H:%M:%SZ)"

# ---------------------------------------------------------------------------------------------------------------------
# Windows' files

drives=""
for d in /mnt/? /mnt/wsl /mnt/wslg; do
    case "$d" in /mnt/wsl | /mnt/wslg) continue ;; esac
    if [ -e "$d" ]; then drives="$drives $d"; fi
done
none_of no-windows-drive "no Windows drive under /mnt" "$drives"

none_of no-windows-filesystem "nothing mounted from Windows" \
    "$(awk '{ for (i = 7; i <= NF; i++) if ($i == "-") { if ($(i + 1) ~ /^(9p|drvfs|lxfs|wslfs|virtiofs)$/) print $5 " (" $(i + 1) ")"; break } }' /proc/self/mountinfo)"

mkdir "$work/x"
if mount -t drvfs C: "$work/x" > /dev/null 2>&1 || mountpoint -q "$work/x"; then
    fail drvfs-mount-refused "mount -t drvfs C: succeeded"
    umount "$work/x" 2> /dev/null
else
    pass drvfs-mount-refused "mount -t drvfs C: is refused"
fi

if ! command -v unshare > /dev/null; then
    pass drvfs-refused-in-userns "no unshare here"
elif unshare -Urm true > /dev/null 2>&1; then
    # shellcheck disable=SC2016 # expanded by the inner shell.
    if unshare -Urm sh -c 'mount -t drvfs C: "$1" || mount -t 9p -o trans=fd,rfd=0,wfd=0 none "$1"' sh "$work/x" > /dev/null 2>&1; then
        fail drvfs-refused-in-userns "a user namespace could mount it"
    else
        fail no-user-namespaces "a user namespace can be made, though the mount in it is refused"
        pass drvfs-refused-in-userns "refused, in a user namespace of its own"
    fi
else
    pass drvfs-refused-in-userns "no user namespace can be made here to try it from"
fi

# The door the two settings in wsl.conf leave open: Windows' 9P server, in reach of any process that opens AF_VSOCK.
detail=$(probe '
import socket, sys
try:
    s = socket.socket(socket.AF_VSOCK, socket.SOCK_STREAM)
except OSError as e:
    print("socket(AF_VSOCK) refused: %s" % e); sys.exit(0)
s.settimeout(3)
try:
    s.connect((2, 50002)); print("connected to Windows file server: host port 50002"); sys.exit(1)
except OSError as e:
    print("the socket opened, and port 50002 answered: %s" % e); sys.exit(1)
')
if [ "$?" -eq 0 ]; then pass no-vsock "$detail"; else fail no-vsock "$detail"; fi

# io_uring opens a socket without calling socket(), past the filter above.
detail=$(probe '
import ctypes, sys
libc = ctypes.CDLL(None, use_errno=True)
params = ctypes.create_string_buffer(120)
fd = libc.syscall(425, 1, params)
if fd < 0:
    print("io_uring_setup refused: errno %d" % ctypes.get_errno()); sys.exit(0)
print("io_uring_setup returned a ring"); sys.exit(1)
')
if [ "$?" -eq 0 ]; then pass no-io-uring "$detail"; else fail no-io-uring "$detail"; fi

# ---------------------------------------------------------------------------------------------------------------------
# Windows' programs

found=""
for program in cmd.exe powershell.exe pwsh.exe wsl.exe wslhost.exe explorer.exe; do
    if command -v "$program" > /dev/null 2>&1; then found="$found $program"; fi
done
none_of no-windows-programs "no Windows program on PATH" "$found"

case ":$PATH:" in
    *:/mnt/*) fail no-windows-path "PATH holds a Windows folder: $PATH" ;;
    *) pass no-windows-path "PATH holds no Windows folder" ;;
esac

none_of no-binfmt-interop "no WSLInterop in binfmt_misc" "$(ls -d /proc/sys/fs/binfmt_misc/WSLInterop* 2> /dev/null)"

# A two-byte file, which Windows could not run even if the request reached it.
printf 'MZ' > "$work/fake.exe"
chmod +x "$work/fake.exe"
if [ -x /init ] && [ -r /init ]; then
    if timeout 10 /init "$work/fake.exe" > /dev/null 2>&1 < /dev/null; then
        fail init-interop-refused "/init ran it"
    else
        pass init-interop-refused "/init can be run here, and refuses"
    fi
else
    pass init-interop-refused "/init, which serves interop, cannot be run"
fi

none_of no-interop-env "no WSL_INTEROP in the environment" "${WSL_INTEROP:-}"

detail=$(probe '
import glob, os, socket, sys
paths = glob.glob("/run/WSL/*_interop")
if os.environ.get("WSL_INTEROP"):
    paths.append(os.environ["WSL_INTEROP"])
reached = []
for path in paths:
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(2)
    try:
        s.connect(path); reached.append(path)
    except OSError:
        pass
    finally:
        s.close()
print("reached: %s" % " ".join(reached) if reached else "no interop socket can be reached")
sys.exit(1 if reached else 0)
')
if [ "$?" -eq 0 ]; then pass no-interop-socket "$detail"; else fail no-interop-socket "$detail"; fi

# ---------------------------------------------------------------------------------------------------------------------
# Privilege

if [ "$(id -u)" = 0 ]; then fail not-root "this is root"; else pass not-root "not root"; fi
# The owner's account in the other distribution is uid 1000, and the two share a user namespace.
if [ "$(id -u)" = 1000 ]; then fail own-uid "uid 1000 is the owner's kernel identity"; else pass own-uid "not the other distribution's uid 1000"; fi

if ! command -v sudo > /dev/null; then
    pass no-sudo "no sudo"
elif sudo -n true > /dev/null 2>&1 < /dev/null; then
    fail no-sudo "sudo works without a password"
else
    pass no-sudo "sudo is here and refuses"
fi
if su -c true > /dev/null 2>&1 < /dev/null; then fail no-su "su worked"; else pass no-su "su is refused"; fi

found=""
for group in $(id -nG); do
    case "$group" in docker | adm | sudo | wheel | disk | root | lxd | kvm | systemd-journal) found="$found $group" ;; esac
done
none_of no-privileged-group "in no group that raises privilege" "$found"

detail=$(probe '
import glob, socket, sys
paths = ["/var/run/docker.sock", "/run/docker.sock"] + glob.glob("/mnt/wsl/docker-desktop*/**/*.sock", recursive=True)
reached = []
for path in paths:
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(2)
    try:
        s.connect(path); reached.append(path)
    except OSError:
        pass
    finally:
        s.close()
print("reached: %s" % " ".join(reached) if reached else "no Docker socket can be reached")
sys.exit(1 if reached else 0)
')
if [ "$?" -eq 0 ]; then pass no-docker "$detail"; else fail no-docker "$detail"; fi

none_of no-setuid "no setuid or setgid program" \
    "$(find / -xdev \( -path /proc -o -path /sys -o -path /dev \) -prune -o \( -perm -4000 -o -perm -2000 \) -type f -print 2> /dev/null)"

status_of() { awk -v key="$1:" '$1 == key { print $2 }' /proc/self/status; }
if [ "$(status_of NoNewPrivs)" = 1 ]; then pass no-new-privileges "no process here can gain privilege"; else fail no-new-privileges "NoNewPrivs is not set"; fi
if [ "$(status_of Seccomp)" = 2 ]; then pass system-call-filter "a system-call filter holds"; else fail system-call-filter "no system-call filter holds"; fi
if [ "$(status_of CapEff)" = 0000000000000000 ] && [ "$(status_of CapBnd)" = 0000000000000000 ]; then
    pass no-capabilities "no capability, and none to be had"
else
    fail no-capabilities "CapEff $(status_of CapEff), CapBnd $(status_of CapBnd)"
fi

# ---------------------------------------------------------------------------------------------------------------------
# Files: credentials and keys

check_files / "$HOME"

others=""
for d in /home/* /root; do
    if [ "$d" != "$HOME" ] && [ -r "$d" ] && [ -x "$d" ]; then others="$others $d"; fi
done
none_of no-other-home "no other home can be read" "$others"

# ---------------------------------------------------------------------------------------------------------------------
# What WSL shares between distributions and with Windows

found=""
for path in /mnt/wslg /mnt/wsl /usr/lib/wsl/lib/libd3d12.so /usr/lib/wsl/drivers; do
    if [ -e "$path" ] && [ -n "$(ls -A "$path" 2> /dev/null || echo unreadable-but-present)" ]; then found="$found $path"; fi
done
none_of no-wsl-shares "nothing of /mnt/wslg, /mnt/wsl or Windows' driver store" "$found"

found=""
for name in DISPLAY WAYLAND_DISPLAY PULSE_SERVER DBUS_SESSION_BUS_ADDRESS WSLENV WSL2_GUI_APPS_ENABLED; do
    if [ -n "${!name:-}" ]; then found="$found $name"; fi
done
none_of no-session-environment "no display, sound or bus in the environment" "$found"

# WSLg's sockets carry the desktop, the microphone and the clipboard bridge to Windows. Connects, and sends nothing.
detail=$(probe '
import glob, socket, sys
paths = glob.glob("/tmp/.X11-unix/*") + glob.glob("/mnt/wslg/runtime-dir/*") + glob.glob("/mnt/wslg/PulseServer") + glob.glob("/run/user/*/wayland-*") + glob.glob("/run/user/*/pulse/native")
reached = []
for path in paths:
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(2)
    try:
        s.connect(path); reached.append(path)
    except OSError:
        pass
    finally:
        s.close()
for name in ("/tmp/.X11-unix/X0", "/tmp/.X11-unix/X1"):
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(2)
    try:
        s.connect("\0" + name); reached.append("abstract " + name)
    except OSError:
        pass
    finally:
        s.close()
print("reached: %s" % " ".join(reached) if reached else "no WSLg socket can be reached")
sys.exit(1 if reached else 0)
')
if [ "$?" -eq 0 ]; then pass no-wslg-socket "$detail"; else fail no-wslg-socket "$detail"; fi

# Abstract sockets belong to the network namespace, which the distributions share. To know this process cannot reach one
# made outside it, it tries one: systemd holds @qor-ci-outside for this test (qor-ci-canary.socket). A datagram connect
# sends nothing and wakes nothing, and Landlock's scope refuses it.
detail=$(probe '
import errno, socket, sys
s = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
try:
    s.connect("\0qor-ci-outside")
    print("an abstract socket held outside this job can be reached"); sys.exit(1)
except PermissionError:
    print("an abstract socket held outside this job is refused"); sys.exit(0)
except OSError as e:
    print("cannot tell: the canary answers %s" % e); sys.exit(1)
')
if [ "$?" -eq 0 ]; then pass abstract-sockets-scoped "$detail"; else fail abstract-sockets-scoped "$detail"; fi

# A pipe here would have the kernel run a helper as root on a job's crash, outside everything above.
case "$(cat /proc/sys/kernel/core_pattern 2> /dev/null)" in
    '|'*) fail no-core-helper "a crash runs a helper: $(cut -c1-80 /proc/sys/kernel/core_pattern)" ;;
    *) pass no-core-helper "a crash runs no helper" ;;
esac

# Looks at the device, and never opens it.
if [ ! -e /dev/dxg ]; then
    pass gpu-unusable "/dev/dxg is not here"
elif [ "$(stat -c %t:%T /dev/dxg)" = 1:3 ]; then
    pass gpu-unusable "/dev/dxg is /dev/null"
else
    fail gpu-unusable "/dev/dxg is the GPU, device $(stat -c %t:%T /dev/dxg)"
fi

found=""
for device in /dev/sd? /dev/vd? /dev/nvme?n? /dev/kmsg /dev/mem /dev/kvm; do
    if [ -r "$device" ]; then found="$found $device"; fi
done
none_of no-raw-devices "no disk or kernel device can be read" "$found"

if [ -r /proc/1/root/. ] && [ -x /proc/1/root ]; then fail no-init-root "pid 1's root can be walked"; else pass no-init-root "pid 1's root cannot be walked"; fi

# ---------------------------------------------------------------------------------------------------------------------
# The network, which the distributions share

# Its own listener, so as to knock on nobody else's door: a test in the other distribution may be listening.
detail=$(probe '
import socket, sys
listener = socket.socket()
listener.bind(("127.0.0.1", 0))
listener.listen(1)
port = listener.getsockname()[1]
s = socket.socket()
s.settimeout(2)
try:
    s.connect(("127.0.0.1", port))
    print("127.0.0.1 is open, and the other distribution shares it"); sys.exit(1)
except OSError as e:
    print("127.0.0.1 is closed to this job: %s" % e); sys.exit(0)
')
if [ "$?" -eq 0 ]; then pass no-shared-loopback "$detail"; else fail no-shared-loopback "$detail"; fi

gateway=$(python3 -I -c '
import socket, struct
for line in open("/proc/net/route").read().splitlines()[1:]:
    fields = line.split()
    if fields[1] == "00000000":
        print(socket.inet_ntoa(struct.pack("<L", int(fields[2], 16))))
        break
')
if [ -z "$gateway" ]; then
    fail no-windows-network "cannot tell where the gateway is, so cannot test it"
else
    # Windows, by WSL's NAT. A TCP connect that times out could be Windows' own firewall, which is not this boundary. A
    # datagram tells: the filter refuses it on this side, at once. Port 9, discard, with nothing in it.
    detail=$(probe '
import socket, sys
gateway = sys.argv[1]
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
try:
    s.sendto(b"", (gateway, 9))
    print("the way to Windows at %s is open: a datagram left" % gateway); sys.exit(1)
except PermissionError:
    print("the filter refuses the way to Windows at %s" % gateway); sys.exit(0)
except OSError as e:
    print("the way to Windows at %s: %s" % (gateway, e)); sys.exit(1)
' "$gateway")
    if [ "$?" -eq 0 ]; then pass no-windows-network "$detail"; else fail no-windows-network "$detail"; fi
fi

# WSL's DNS address is local, so whatever listens on every address in the shared namespace answers on it. Read the DNS
# server from nowhere: the address is WSL's own, and a job's DNS is elsewhere.
detail=$(probe '
import socket, sys
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
try:
    s.sendto(b"", ("10.255.255.254", 9))
    print("10.255.255.254 is open, and all-address listeners in the shared namespace answer on it"); sys.exit(1)
except PermissionError:
    print("the filter refuses WSL local DNS address"); sys.exit(0)
except OSError as e:
    print("10.255.255.254: %s" % e); sys.exit(1)
')
if [ "$?" -eq 0 ]; then pass no-wsl-local-address "$detail"; else fail no-wsl-local-address "$detail"; fi

# A boundary that stopped everything would pass the rest: a job has to be able to work.
if [ "$OFFLINE" = no ]; then
    detail=$(probe '
import socket, sys
s = socket.socket()
s.settimeout(8)
try:
    s.connect(("github.com", 443)); print("github.com:443 can be reached, by name")
except OSError as e:
    print("github.com:443: %s" % e); sys.exit(1)
')
    if [ "$?" -eq 0 ]; then pass job-can-work "$detail"; else fail job-can-work "$detail"; fi
fi

touch "$HOME/.qor-boundary-check" 2> /dev/null && rm -f "$HOME/.qor-boundary-check" && home=yes || home=no
if [ "$home" = yes ]; then pass home-writable "the home can be written"; else fail home-writable "the home cannot be written"; fi
if touch /usr/.qor-boundary-check 2> /dev/null; then
    rm -f /usr/.qor-boundary-check
    fail system-read-only "the system can be written"
else
    pass system-read-only "the system cannot be written"
fi

echo
if [ "${#failures[@]}" -gt 0 ]; then
    printf 'FAILED %d of %d: %s\n' "${#failures[@]}" "$((${#failures[@]} + passes))" "${failures[*]}"
    exit 1
fi
printf 'All %d passed. What remains is in docs/CI.md: one kernel and VM with the other distribution, and the internet.\n' "$passes"

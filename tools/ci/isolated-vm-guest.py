#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""What runs inside qor-ci-vm for tools/ci/isolated-vm.ps1 -Action Prove: it attacks the boundary from the inside.

The script on Windows sends this file in, then runs it twice, with what to attack as JSON on stdin:

    python3 -I prove.py --level user     as runner, who has no privilege, and is what a job would be
    python3 -I prove.py --level root     as root, who owns the guest's kernel and can say anything on its network

It prints one line for each attempt:

    PASS  category  what it tried: what happened
    FAIL  category  what it tried: what happened
    UNRUN category  what it could not try, and why
    NOTE  category  something worth reading that decides nothing

and ends with an END line. Its exit status is 0 only if there was at least one PASS and no FAIL or UNRUN.

A failed attack is a PASS. An attack that could not be tried is an UNRUN, which fails the run, because it shows nothing. A TCP
refusal from something outside the guest is a FAIL: "refused" means a packet got there and something answered, whether or not
that port had a listener, so it shows the road is open. Only silence and "no route" count as shut.

The controls are what stop "everything was refused" from meaning "the network is down". They must succeed, before the attacks
and after them, and if they do not the run is an UNRUN however quiet the attacks were.

Everything here means something only in a real VM on Hyper-V. Under WSL the "where" check refuses to call the run a pass, and the
vsock line fails there, as it should. Anything answered with --fs-root is a stand-in and says so in its END line, and the script on
Windows refuses it.

Nothing here reads a file of the host's or writes to one. A connection that succeeds is closed at once, with nothing said on it.
"""

from __future__ import annotations

import argparse
import errno
import ipaddress
import json
import os
import re
import selectors
import socket
import ssl
import struct
import subprocess
import sys
import tempfile
import time

AF_VSOCK = 40
VMADDR_CID_HOST = 2
# Where Windows' 9P file server answered from inside WSL, with C:\ behind it (docs/CI.md).
NINE_P_PORT = 50002

TIMED_OUT = -1
NO_FAMILY = -2

CONNECT_TIMEOUT = 2.0
SWEEP_TIMEOUT = 1.0
SWEEP_WIDTH = 1000
ATTACK_WIDTH = 64
FRAME_WAIT = 3.0
CLOCK_WAIT = 90.0
CLOCK_STEP = 5.0
CLOCK_LIMIT = 300.0

CATEGORIES = {
    "user": ("where", "control", "vmbus", "mounts", "devices", "vsock", "host-tcp", "host-sweep", "host-udp", "lan", "wsl"),
    "root": (
        "where",
        "control",
        "vmbus",
        "mounts",
        "devices",
        "vsock",
        "host-tcp",
        "host-udp",
        "lan",
        "wsl",
        "host-spoof",
        "host-loopback",
        "icmp",
        "icmp6",
    ),
}

# The kernel's own names for them: tools/hv/lsvmbus. On a VM with every integration service off, these are what is left.
VMBUS_EXPECTED = {
    "ba6163d9-04a1-4d29-b605-72e2ffb1dc7f": "Synthetic SCSI Controller",
    "f8615163-df3e-46c5-913f-f2d2f965ed0e": "Synthetic network adapter",
    "f912ad6d-2b17-48ea-bd65-f927a61c7684": "Synthetic keyboard",
    "cfa8b69e-5b4a-4cc0-b98b-8ba1a1f3f95a": "Synthetic mouse",
    "da0a7802-e377-4aac-8e77-0558eb1073f8": "Synthetic framebuffer adapter",
    "276aacf4-ac15-426c-98dd-7521ad3f01fe": "Reserved system device",
    "f8e65716-3cb3-4a06-9a60-1889c5cccab5": "Reserved system device",
    "3375baf4-9e15-4b30-b765-67acb10d607b": "Reserved system device",
}
VMBUS_MUST_BE_THERE = ("ba6163d9-04a1-4d29-b605-72e2ffb1dc7f", "f8615163-df3e-46c5-913f-f2d2f965ed0e")
# What each of these would be, for the line that says it should not be here. Anything else is named by its number.
VMBUS_FORBIDDEN = {
    "0e0b6031-5213-4934-818b-38d90ced39db": "Shutdown integration service",
    "9527e630-d0ae-497b-adce-e80ab0175caf": "Time Synchronization integration service",
    "57164f39-9115-4e78-ab55-382f3bd5422d": "Heartbeat integration service",
    "a9a0f4e7-5a45-4d96-b827-8a841e8c03e6": "Data Exchange integration service",
    "35fa2e29-ea23-4236-96ae-3a6ebacba440": "Backup (VSS) integration service",
    "34d14be3-dee4-41c8-9ae7-6b174977c192": "Guest Services, the host's file copy",
    "525074dc-8985-46e2-8057-a307dc18a502": "Dynamic Memory",
    "32412632-86cb-44a2-9b5c-50d1417354f5": "IDE controller, which a generation 2 VM does not have",
    "2f9bcc4a-0069-4af3-b76b-6fd0be528cda": "Fibre Channel adapter",
    "8c2eaf3d-32a7-4b09-ab99-bd1f1c86b501": "RDMA adapter",
    "44c4f61d-4444-4400-9d52-802e27ede19f": "PCI Express pass-through",
}

# What brings a host's or another machine's files into a Linux filesystem tree.
SHARED_FILESYSTEMS = re.compile(r"^(9p|drvfs|lxfs|wslfs|virtiofs|cifs|smb3|smbfs|nfs|nfs4|vboxsf|vmhgfs|ceph|afs|davfs|fuse\..*)$")

SECURE_BOOT_VARIABLE = "/sys/firmware/efi/efivars/SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c"
WSL_SIGNS = ("/run/WSL", "/proc/sys/fs/binfmt_misc/WSLInterop", "/usr/lib/wsl", "/mnt/wsl")


class Report:
    def __init__(self, level: str, stand_in: bool):
        self.level = level
        self.stand_in = stand_in
        self.counts = {"PASS": 0, "FAIL": 0, "UNRUN": 0}
        self.seen: set[str] = set()

    def line(self, status: str, category: str, text: str) -> None:
        self.counts[status] += 1
        self.seen.add(category)
        print(f"{status:<5} {category:<13} {text}", flush=True)

    def note(self, category: str, text: str) -> None:
        print(f"{'NOTE':<5} {category:<13} {text}", flush=True)

    def finish(self) -> int:
        # A category that printed nothing was not attacked, and silence must never read as a pass.
        for category in CATEGORIES[self.level]:
            if category not in self.seen:
                self.line("UNRUN", category, "this check printed nothing, so it shows nothing")
        passed, failed, unrun = self.counts["PASS"], self.counts["FAIL"], self.counts["UNRUN"]
        stand_in = "yes" if self.stand_in else "no"
        print(f"END level={self.level} passed={passed} failed={failed} unrun={unrun} stand-in={stand_in}", flush=True)
        return 0 if passed > 0 and failed == 0 and unrun == 0 else 1


class Run:
    def __init__(self, level: str, targets: dict, report: Report, fs_root: str = "/"):
        self.level = level
        self.targets = targets
        self.report = report
        self.fs_root = fs_root
        self.started = time.monotonic()

    def path(self, absolute: str) -> str:
        return os.path.join(self.fs_root, absolute.lstrip("/"))

    def read(self, absolute: str) -> str:
        with open(self.path(absolute), encoding="utf-8", errors="replace") as file:
            return file.read()


def errno_name(code: int) -> str:
    return errno.errorcode.get(code, str(code))


def effective_uid() -> int:
    """-1 where there are no uids, which is only ever this file's tests on Windows."""
    return os.geteuid() if hasattr(os, "geteuid") else -1


# ---------------------------------------------------------------------------------------------------------------------
# Sockets. Kept apart from what is decided about them, so that the tests can stand in for the far side.


def connect_many(addresses, timeout: float, width: int) -> dict:
    """Tries each (ip, port) at once, width at a time. Returns {(ip, port): 0, an errno, or TIMED_OUT}. Sends nothing."""
    results: dict = {}
    waiting: dict = {}
    selector = selectors.DefaultSelector()
    pending = iter(addresses)
    exhausted = False

    def begin(address) -> None:
        family = socket.AF_INET6 if ":" in address[0] else socket.AF_INET
        try:
            sock = socket.socket(family, socket.SOCK_STREAM)
        except OSError as error:
            results[address] = error.errno or errno.EIO
            return
        sock.setblocking(False)
        code = sock.connect_ex(address)
        if code in (errno.EINPROGRESS, errno.EWOULDBLOCK, errno.EALREADY):
            waiting[sock] = (address, time.monotonic() + timeout)
            selector.register(sock, selectors.EVENT_WRITE)
        else:
            results[address] = code
            sock.close()

    def end(sock, code: int) -> None:
        address, _ = waiting.pop(sock)
        selector.unregister(sock)
        sock.close()
        results[address] = code

    try:
        while True:
            while not exhausted and len(waiting) < width:
                try:
                    begin(next(pending))
                except StopIteration:
                    exhausted = True
            if not waiting:
                if exhausted:
                    return results
                continue
            # Every one has the same timeout, so the oldest is the next to expire.
            _, nearest = next(iter(waiting.values()))
            for key, _ in selector.select(max(0.0, nearest - time.monotonic())):
                end(key.fileobj, key.fileobj.getsockopt(socket.SOL_SOCKET, socket.SO_ERROR))
            now = time.monotonic()
            for sock in [sock for sock, (_, deadline) in waiting.items() if deadline <= now]:
                end(sock, TIMED_OUT)
    finally:
        for sock in list(waiting):
            sock.close()
        selector.close()


def connect_vsock(cid: int, port: int, timeout: float) -> int:
    """0 if connected, NO_FAMILY if this kernel has no such sockets, TIMED_OUT or an errno if not. Says nothing."""
    try:
        sock = socket.socket(AF_VSOCK, socket.SOCK_STREAM)
    except OSError as error:
        return NO_FAMILY if error.errno == errno.EAFNOSUPPORT else (error.errno or errno.EIO)
    sock.settimeout(timeout)
    try:
        sock.connect((cid, port))
        return 0
    except TimeoutError:
        return TIMED_OUT
    except OSError as error:
        return error.errno or errno.EIO
    finally:
        sock.close()


def run_command(arguments: list[str], timeout: float = 30.0) -> tuple[int, str]:
    done = subprocess.run(arguments, capture_output=True, text=True, timeout=timeout, check=False)
    return done.returncode, (done.stderr or done.stdout).strip()[:300]


def https_status(host: str, port: int, timeout: float) -> str:
    """The first line of an answer over TLS with the certificate checked. Raises if there is none."""
    with socket.create_connection((host, port), timeout=timeout) as raw:
        with ssl.create_default_context().wrap_socket(raw, server_hostname=host) as tls:
            tls.settimeout(timeout)
            tls.sendall(f"HEAD / HTTP/1.1\r\nHost: {host}\r\nConnection: close\r\n\r\n".encode("ascii"))
            return tls.recv(256).decode("ascii", "replace").splitlines()[0]


def resolve(name: str) -> list[str]:
    return sorted({entry[4][0] for entry in socket.getaddrinfo(name, 443, socket.AF_INET, socket.SOCK_STREAM)})


def dns_query(name: str) -> bytes:
    labels = b"".join(bytes([len(part)]) + part.encode("ascii") for part in name.split("."))
    return struct.pack("!HHHHHH", 0x5152, 0x0100, 1, 0, 0, 0) + labels + b"\0" + struct.pack("!HH", 1, 1)


def udp_dns(address: str, timeout: float) -> tuple[str, str]:
    """(PASS, FAIL or UNRUN, what happened) for one DNS question to address."""
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(timeout)
        try:
            sock.connect((address, 53))
            sock.send(dns_query("github.com"))
            sock.recv(512)
            return "FAIL", "it answered"
        except TimeoutError:
            return "PASS", f"no answer in {timeout:g} s"
        except OSError as error:
            if error.errno == errno.ECONNREFUSED:
                return "FAIL", "a refusal came back, so a packet got there"
            if error.errno in (errno.EHOSTUNREACH, errno.ENETUNREACH):
                return "PASS", errno_name(error.errno)
            return "UNRUN", f"{errno_name(error.errno or 0)}, which shows nothing"


def checksum(data: bytes) -> int:
    if len(data) % 2:
        data += b"\0"
    total = sum(struct.unpack(f"!{len(data) // 2}H", data))
    while total >> 16:
        total = (total & 0xFFFF) + (total >> 16)
    return ~total & 0xFFFF


def syn_frame(source_mac: bytes, gateway_mac: bytes, source_ip: str, target_ip: str, source_port: int, target_port: int) -> bytes:
    """An Ethernet frame holding a TCP SYN, for the addresses and ports the kernel would not send one from or to."""
    src, dst = socket.inet_aton(source_ip), socket.inet_aton(target_ip)
    ip = struct.pack("!BBHHHBBH4s4s", 0x45, 0, 40, 0x5152, 0x4000, 64, socket.IPPROTO_TCP, 0, src, dst)
    ip = ip[:10] + struct.pack("!H", checksum(ip)) + ip[12:]
    tcp = struct.pack("!HHIIBBHHH", source_port, target_port, 0x51525354, 0, 5 << 4, 0x02, 64240, 0, 0)
    pseudo = src + dst + struct.pack("!BBH", 0, socket.IPPROTO_TCP, len(tcp))
    tcp = tcp[:16] + struct.pack("!H", checksum(pseudo + tcp)) + tcp[18:]
    return gateway_mac + source_mac + struct.pack("!H", 0x0800) + ip + tcp


def tcp_reply(frame: bytes, our_ip: str) -> tuple[str, int, int, int] | None:
    """(from ip, from port, to port, TCP flags) if frame is TCP sent to our_ip, or None."""
    if len(frame) < 54 or frame[12:14] != b"\x08\x00" or frame[23] != socket.IPPROTO_TCP:
        return None
    if frame[30:34] != socket.inet_aton(our_ip):
        return None
    start = 14 + (frame[14] & 0x0F) * 4
    if len(frame) < start + 14:
        return None
    from_port, to_port = struct.unpack("!HH", frame[start : start + 4])
    return socket.inet_ntoa(frame[26:30]), from_port, to_port, frame[start + 13]


def open_frame_socket(interface: str):
    sock = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(0x0800))
    sock.bind((interface, 0x0800))
    return sock


def send_syns(interface: str, source_mac: bytes, gateway_mac: bytes, source_ip: str, attempts: list, wait: float) -> dict:
    """attempts is a list of (ip, port, source port). Returns {attempt: None or the reply}, listening after all have gone."""
    answers: dict = dict.fromkeys(attempts)
    with open_frame_socket(interface) as sock:
        for target_ip, target_port, source_port in attempts:
            sock.send(syn_frame(source_mac, gateway_mac, source_ip, target_ip, source_port, target_port))
        deadline = time.monotonic() + wait
        while time.monotonic() < deadline:
            sock.settimeout(max(0.05, deadline - time.monotonic()))
            try:
                reply = tcp_reply(sock.recv(2048), source_ip)
            except TimeoutError:
                break
            if not reply:
                continue
            from_ip, from_port, to_port, flags = reply
            for attempt in attempts:
                # A frame to loopback gets no reply from loopback: whatever answers for that port and source port counts.
                if attempt[1] == from_port and attempt[2] == to_port and (attempt[0] == from_ip or attempt[0].startswith("127.")):
                    answers[attempt] = f"flags 0x{flags:02x} from {from_ip}"
    return answers


def open_icmp(version: int):
    if version == 6:
        return socket.socket(socket.AF_INET6, socket.SOCK_RAW, socket.IPPROTO_ICMPV6)
    return socket.socket(socket.AF_INET, socket.SOCK_RAW, socket.IPPROTO_ICMP)


def ping_v4(addresses: list[str], wait: float) -> set[str]:
    """The addresses that answered an ICMP echo."""
    answered: set[str] = set()
    ident = os.getpid() & 0xFFFF
    message = struct.pack("!BBHHH", 8, 0, 0, ident, 1) + b"qor-ci-vm"
    message = message[:2] + struct.pack("!H", checksum(message)) + message[4:]
    with open_icmp(4) as sock:
        for address in addresses:
            sock.sendto(message, (address, 0))
        deadline = time.monotonic() + wait
        while time.monotonic() < deadline:
            sock.settimeout(max(0.05, deadline - time.monotonic()))
            try:
                packet, peer = sock.recvfrom(2048)
            except TimeoutError:
                break
            start = (packet[0] & 0x0F) * 4
            if packet[start] == 0 and struct.unpack("!H", packet[start + 4 : start + 6])[0] == ident:
                answered.add(peer[0])
    return answered & set(addresses)


def ping_v6_all_nodes(interface: str, own: set[str], wait: float) -> set[str]:
    """Who else on this link answers an ICMPv6 echo to all nodes."""
    answered: set[str] = set()
    message = struct.pack("!BBHHH", 128, 0, 0, os.getpid() & 0xFFFF, 1) + b"qor-ci-vm"
    with open_icmp(6) as sock:
        sock.sendto(message, ("ff02::1", 0, 0, socket.if_nametoindex(interface)))
        deadline = time.monotonic() + wait
        while time.monotonic() < deadline:
            sock.settimeout(max(0.05, deadline - time.monotonic()))
            try:
                packet, peer = sock.recvfrom(2048)
            except TimeoutError:
                break
            address = str(ipaddress.IPv6Address(peer[0].split("%")[0]))
            if packet[0] == 129 and address not in own:
                answered.add(address)
    return answered


# ---------------------------------------------------------------------------------------------------------------------
# What is decided about what happened.


def judge_tcp(code: int, timeout: float) -> tuple[str, str]:
    if code == 0:
        return "FAIL", "connected"
    if code == errno.ECONNREFUSED:
        return "FAIL", "refused, so a packet got there and something answered"
    if code == errno.ECONNRESET:
        return "FAIL", "reset, so a packet got there and something answered"
    if code in (TIMED_OUT, errno.ETIMEDOUT):
        return "PASS", f"no answer in {timeout:g} s"
    if code in (errno.EHOSTUNREACH, errno.ENETUNREACH, errno.EHOSTDOWN):
        return "PASS", errno_name(code)
    return "UNRUN", f"{errno_name(code)}, which shows nothing"


def judge_vsock(code: int, level: str) -> tuple[str, str]:
    if code == 0:
        return "FAIL", "connected to a service on the host"
    if code == NO_FAMILY:
        if level == "root":
            return "UNRUN", "this kernel offers root no AF_VSOCK, so nothing was tried"
        return "PASS", "this kernel offers no AF_VSOCK to this process"
    if code in (TIMED_OUT, errno.ETIMEDOUT, errno.ECONNRESET, errno.ECONNREFUSED, errno.ENODEV, errno.ENETUNREACH, errno.EHOSTUNREACH):
        return "PASS", "no answer in time" if code == TIMED_OUT else errno_name(code)
    return "UNRUN", f"{errno_name(code)}, which shows nothing"


# ---------------------------------------------------------------------------------------------------------------------
# The checks. Each prints its own lines. Any of them raising becomes an UNRUN, in guarded().


def check_where(run: Run) -> None:
    report = run.report
    vendor = product = ""
    try:
        vendor = run.read("/sys/class/dmi/id/sys_vendor").strip()
        product = run.read("/sys/class/dmi/id/product_name").strip()
    except OSError:
        pass
    has_vmbus = os.path.isdir(run.path("/sys/bus/vmbus/devices"))
    release = ""
    try:
        release = run.read("/proc/sys/kernel/osrelease").strip()
    except OSError:
        pass
    signs = [sign for sign in WSL_SIGNS if os.path.exists(run.path(sign))]
    if "microsoft" in release.lower() or "wsl" in release.lower():
        signs.append(f"kernel {release}")
    if vendor == "Microsoft Corporation" and product == "Virtual Machine" and has_vmbus and not signs:
        report.line("PASS", "where", f"a Hyper-V VM with its own kernel: {vendor}, {product}, kernel {release}")
    else:
        about = f"vendor '{vendor}', product '{product}', VMBus {'seen' if has_vmbus else 'not seen'}, WSL's marks {signs or 'none'}"
        report.line("UNRUN", "where", f"this is not a Hyper-V VM ({about}), so what follows says nothing about one")

    uid = effective_uid()
    if (uid == 0) == (run.level == "root") and uid != -1:
        report.line("PASS", "where", f"running as uid {uid}, as level {run.level} should")
    else:
        report.line("UNRUN", "where", f"level {run.level} is running as uid {uid}, so it does not show what it claims to")

    try:
        with open(run.path(SECURE_BOOT_VARIABLE), "rb") as file:
            value = file.read()
    except OSError as error:
        report.line("UNRUN", "where", f"cannot read the firmware's Secure Boot setting: {error}")
    else:
        if value[-1:] == b"\x01":
            report.line("PASS", "where", "the firmware says Secure Boot is on")
        else:
            report.line("FAIL", "where", "the firmware says Secure Boot is off")

    try:
        report.note("where", f"kernel lockdown: {run.read('/sys/kernel/security/lockdown').strip()}")
    except OSError:
        report.note("where", "kernel lockdown: not readable here")


def check_control_loopback(run: Run) -> None:
    """Whatever says "nothing connected" below must be able to say "connected"."""
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        address = listener.getsockname()
        code = connect_many([address], max(CONNECT_TIMEOUT, 5.0), 1)[address]
    if code == 0:
        run.report.line("PASS", "control", f"tcp to this guest's own {address[0]}:{address[1]}: connected, so a connection is seen when there is one")
    else:
        run.report.line("UNRUN", "control", f"tcp to this guest's own listener: {errno_name(code)}, so a failure below would show nothing")


def check_control_network(run: Run, when: str) -> None:
    report = run.report
    name = run.targets["control_host"]
    try:
        addresses = resolve(name)
    except OSError as error:
        report.line("UNRUN", "control", f"DNS {when}, {name}: {error}, so the network is down and the attacks show nothing")
    else:
        private = [a for a in addresses if not ipaddress.ip_address(a).is_global]
        if addresses and not private:
            report.line("PASS", "control", f"DNS {when}, {name}: {' '.join(addresses)}")
        else:
            report.line("UNRUN", "control", f"DNS {when}, {name}: {' '.join(addresses) or 'nothing'} is not a public answer")
    try:
        status = https_status(name, 443, 15.0)
    except (OSError, IndexError, ValueError) as error:
        report.line("UNRUN", "control", f"HTTPS {when}, {name}: {error}, so the network is down and the attacks show nothing")
    else:
        if status.startswith("HTTP/"):
            report.line("PASS", "control", f"HTTPS {when}, {name}, certificate checked: {status}")
        else:
            report.line("UNRUN", "control", f"HTTPS {when}, {name}: answered '{status[:80]}', which is not HTTP")


def check_control_clock(run: Run) -> None:
    """The clock comes from the virtual firmware and NTP, as the time integration service is off, and TLS needs it near right."""
    host_time = float(run.targets["host_time"])
    deadline = time.monotonic() + CLOCK_WAIT
    while True:
        # Elapsed by the monotonic clock, which NTP cannot step: a correction must show up as the drift closing.
        drift = time.time() - (host_time + (time.monotonic() - run.started))
        if abs(drift) <= CLOCK_LIMIT:
            run.report.line("PASS", "control", f"the clock is within {CLOCK_LIMIT:g} s of Windows': {drift:+.0f} s")
            return
        if time.monotonic() >= deadline:
            run.report.line("UNRUN", "control", f"the clock is {drift:+.0f} s from Windows', so a TLS failure below would show nothing")
            return
        time.sleep(min(CLOCK_STEP, max(0.0, deadline - time.monotonic())))


def default_route(run: Run) -> tuple[str, str]:
    """(interface, gateway) from the kernel's routing table."""
    for line in run.read("/proc/net/route").splitlines()[1:]:
        fields = line.split()
        if fields[1] == "00000000":
            return fields[0], socket.inet_ntoa(struct.pack("<L", int(fields[2], 16)))
    raise OSError("no default route")


def gateway_mac(run: Run, gateway: str) -> str:
    for line in run.read("/proc/net/arp").splitlines()[1:]:
        fields = line.split()
        if fields[0] == gateway and fields[2] != "0x0":
            return fields[3]
    raise OSError(f"no link-layer address for {gateway}")


def check_control_link(run: Run) -> None:
    _, gateway = default_route(run)
    if gateway != run.targets["gateway"]:
        run.report.line("UNRUN", "control", f"the default route is {gateway}, not {run.targets['gateway']}, so these attacks are aimed wrong")
        return
    run.report.line("PASS", "control", f"the gateway {gateway} is at {gateway_mac(run, gateway)}, so this guest is on its link")


def check_vmbus(run: Run) -> None:
    report = run.report
    root = run.path("/sys/bus/vmbus/devices")
    if not os.path.isdir(root):
        report.line("UNRUN", "vmbus", "no VMBus here to look at")
        return
    found = {}
    for device in sorted(os.listdir(root)):
        with open(os.path.join(root, device, "class_id"), encoding="ascii") as file:
            found[device] = file.read().strip().strip("{}").lower()
    classes = set(found.values())
    missing = [VMBUS_EXPECTED[c] for c in VMBUS_MUST_BE_THERE if c not in classes]
    if missing:
        report.line("UNRUN", "vmbus", f"no {', '.join(missing)}, which this guest cannot run without, so this is not the list it seems")
    for device, class_id in found.items():
        if class_id in VMBUS_EXPECTED:
            report.line("PASS", "vmbus", f"{device}: {VMBUS_EXPECTED[class_id]}, which is expected")
        else:
            about = VMBUS_FORBIDDEN.get(class_id, "not a device this VM was given")
            report.line("FAIL", "vmbus", f"{device}: class {class_id}, {about}")


def check_mounts(run: Run) -> None:
    report = run.report
    shared = []
    total = 0
    for line in run.read("/proc/self/mountinfo").splitlines():
        total += 1
        after = line.split(" - ", 1)
        if len(after) == 2 and SHARED_FILESYSTEMS.match(after[1].split()[0]):
            shared.append(f"{line.split()[4]} ({after[1].split()[0]})")
    if shared:
        report.line("FAIL", "mounts", f"something is mounted from elsewhere: {' '.join(shared)}")
    else:
        report.line("PASS", "mounts", f"read {total} mounts: none is 9P, drvfs, virtiofs, SMB, NFS or FUSE")
    for folder in ("/mnt", "/media"):
        try:
            inside = os.listdir(run.path(folder))
        except FileNotFoundError:
            inside = []
        if inside:
            report.line("FAIL", "mounts", f"{folder} holds {' '.join(sorted(inside))}")
        else:
            report.line("PASS", "mounts", f"{folder} is empty")
    if run.level != "root":
        return
    with tempfile.TemporaryDirectory() as folder:
        for fs_type, options, source in (
            ("9p", "trans=virtio,version=9p2000.L", "C:"),
            ("9p", "trans=virtio,version=9p2000.L", "drvfs"),
            ("virtiofs", None, "C:"),
            ("drvfs", None, "C:"),
        ):
            command = ["mount", "-t", fs_type] + (["-o", options] if options else []) + [source, folder]
            code, said = run_command(command)
            if code == 0:
                run_command(["umount", folder])
                report.line("FAIL", "mounts", f"{' '.join(command[:-1])}: it mounted")
            else:
                report.line("PASS", "mounts", f"{' '.join(command[:-1])}: refused, {said or 'with no reason given'}")


def check_devices(run: Run) -> None:
    report = run.report
    for node, about in (
        ("/dev/dxg", "WSL's door to the GPU"),
        ("/dev/vmbus/hv_kvp", "the Data Exchange service's device"),
        ("/dev/vmbus/hv_vss", "the backup service's device"),
        ("/dev/vmbus/hv_fcopy", "the host's file copy device"),
    ):
        if os.path.exists(run.path(node)):
            report.line("FAIL", "devices", f"{node}, {about}, is here")
        else:
            report.line("PASS", "devices", f"{node}, {about}, is not here")

    for folder, pattern, about in (
        ("/dev/dri", r"^(renderD|card)\d+$", "a GPU"),
        ("/dev", r"^uio\d+$", "a VMBus device handed to user space"),
        ("/dev/vfio", r"^\d+$", "a device for pass-through"),
    ):
        try:
            names = sorted(name for name in os.listdir(run.path(folder)) if re.match(pattern, name))
        except FileNotFoundError:
            names = []
        if names:
            report.line("FAIL", "devices", f"{folder} holds {' '.join(names)}, {about}")
        else:
            report.line("PASS", "devices", f"{folder} holds no node for {about}")

    disks = sorted(name for name in os.listdir(run.path("/sys/block")) if not re.match(r"^(loop|ram|zram|dm-|md)", name))
    if disks == ["sda"]:
        report.line("PASS", "devices", "one disk, sda, and no DVD drive")
    else:
        report.line("FAIL", "devices", f"the block devices should be sda alone, and are: {' '.join(disks) or 'none'}")

    try:
        pci = sorted(os.listdir(run.path("/sys/bus/pci/devices")))
    except FileNotFoundError:
        pci = []
    if pci:
        report.line("FAIL", "devices", f"PCI devices, which a generation 2 VM has only by pass-through: {' '.join(pci)}")
    else:
        report.line("PASS", "devices", "no PCI device, so nothing has been passed through")

    flags = re.findall(r"^flags\s*:\s*(.*)$", run.read("/proc/cpuinfo"), re.M)
    if not flags:
        report.line("UNRUN", "devices", "/proc/cpuinfo names no CPU flags")
    elif re.search(r"\b(vmx|svm)\b", " ".join(flags)):
        report.line("FAIL", "devices", "the CPU offers virtualisation to this guest, so it could run a hypervisor of its own")
    else:
        report.line("PASS", "devices", "the CPU offers this guest no virtualisation of its own")

    if os.path.exists(run.path("/var/lib/hyperv")):
        report.line("FAIL", "devices", "/var/lib/hyperv is here, where Data Exchange leaves what the host says about itself")
    else:
        report.line("PASS", "devices", "no /var/lib/hyperv, where Data Exchange would leave what the host says about itself")


def check_vsock(run: Run) -> None:
    report = run.report
    if run.level == "root":
        code, said = run_command(["modprobe", "hv_sock"])
        if code == 0:
            report.line("PASS", "vsock", "modprobe hv_sock: loaded, so Hyper-V sockets are there to attack with")
        else:
            report.line("UNRUN", "vsock", f"modprobe hv_sock: {said}, so root's attack on Hyper-V sockets is not armed")
    ports = sorted({NINE_P_PORT, *(int(port) for port in run.targets["vsock_ports"])})
    for port in ports:
        about = "the 9P route that reached C:\\ from WSL" if port == NINE_P_PORT else "a Hyper-V socket"
        status, what = judge_vsock(connect_vsock(VMADDR_CID_HOST, port, CONNECT_TIMEOUT), run.level)
        report.line(status, "vsock", f"AF_VSOCK to the host, port {port}, {about}: {what}")
    # Not the host's address, for the kernels and transports that take any of them.
    for cid in (0, 1, 3):
        status, what = judge_vsock(connect_vsock(cid, NINE_P_PORT, CONNECT_TIMEOUT), run.level)
        report.line(status, "vsock", f"AF_VSOCK to cid {cid}, port {NINE_P_PORT}: {what}")


def attack_tcp(run: Run, category: str, addresses: list[str], ports: list[int]) -> None:
    if not addresses or not ports:
        run.report.line("UNRUN", category, f"nothing to try: {len(addresses)} addresses and {len(ports)} ports")
        return
    attempts = [(address, port) for address in addresses for port in ports]
    results = connect_many(attempts, CONNECT_TIMEOUT, ATTACK_WIDTH)
    for address, port in attempts:
        status, what = judge_tcp(results[(address, port)], CONNECT_TIMEOUT)
        shown = f"[{address}]" if ":" in address else address
        run.report.line(status, category, f"tcp {shown}:{port}: {what}")


def check_host_tcp(run: Run) -> None:
    targets = run.targets
    addresses = sorted({targets["gateway"], *targets["host_addresses"], *targets["host_addresses_v6"]})
    attack_tcp(run, "host-tcp", addresses, sorted(set(targets["host_ports"])))


def raise_file_limit(wanted: int) -> int:
    try:
        import resource
    except ImportError:
        return wanted
    soft, hard = resource.getrlimit(resource.RLIMIT_NOFILE)
    target = wanted + 16 if hard == resource.RLIM_INFINITY else min(wanted + 16, hard)
    if soft < target:
        resource.setrlimit(resource.RLIMIT_NOFILE, (target, hard))
        soft = target
    return max(1, min(wanted, soft - 16))


def check_host_sweep(run: Run) -> None:
    """Whatever Windows was not found listening on when the list was made. The ports nobody thought of."""
    report = run.report
    first, last = run.targets["sweep"]
    gateway = run.targets["gateway"]
    results = connect_many([(gateway, port) for port in range(first, last + 1)], SWEEP_TIMEOUT, raise_file_limit(SWEEP_WIDTH))
    counts: dict = {}
    noisy = 0
    for (_, port), code in sorted(results.items()):
        status, what = judge_tcp(code, SWEEP_TIMEOUT)
        if status == "PASS":
            counts[what] = counts.get(what, 0) + 1
        else:
            noisy += 1
            report.line(status, "host-sweep", f"tcp {gateway}:{port}: {what}")
    if not results:
        report.line("UNRUN", "host-sweep", "no port was tried")
    elif not noisy:
        said = ", ".join(f"{count} {what}" for what, count in sorted(counts.items()))
        report.line("PASS", "host-sweep", f"tcp {gateway}, every port from {first} to {last}: {said}")


def check_host_udp(run: Run) -> None:
    gateway = run.targets["gateway"]
    status, what = udp_dns(gateway, CONNECT_TIMEOUT)
    run.report.line(status, "host-udp", f"a DNS question to {gateway}:53: {what}")


def check_lan(run: Run) -> None:
    attack_tcp(run, "lan", sorted(set(run.targets["lan"])), sorted(set(run.targets["lan_ports"])))


def check_wsl(run: Run) -> None:
    targets = run.targets
    attack_tcp(run, "wsl", sorted(set(targets["wsl"])), sorted({*targets["host_ports"], *targets["loopback_ports"]}))


def frame_context(run: Run) -> tuple[str, bytes, bytes, str]:
    interface, gateway = default_route(run)
    mac = bytes.fromhex(gateway_mac(run, gateway).replace(":", ""))
    own = bytes.fromhex(run.read(f"/sys/class/net/{interface}/address").strip().replace(":", ""))
    return interface, own, mac, run.targets["vm_ip"]


def attack_frames(run: Run, category: str, attempts: list, describe) -> None:
    interface, own, mac, source_ip = frame_context(run)
    answers = send_syns(interface, own, mac, source_ip, attempts, FRAME_WAIT)
    for attempt in attempts:
        if answers[attempt]:
            run.report.line("FAIL", category, f"{describe(attempt)}: answered, {answers[attempt]}")
        else:
            run.report.line("PASS", category, f"{describe(attempt)}: no answer in {FRAME_WAIT:g} s")


def check_host_spoof(run: Run) -> None:
    """A rule that lets SSH's replies back by their source port would let these through, from root, who can write them."""
    gateway = run.targets["gateway"]
    attempts = [(gateway, port, source) for port in (135, 139, 445, 3389, 5985, 5986) for source in (22, 53)]
    attack_frames(run, "host-spoof", attempts, lambda a: f"a SYN to {a[0]}:{a[1]} from source port {a[2]}")


def check_host_loopback(run: Run) -> None:
    """Windows' 127.0.0.1, where WSL's localhost relay listens, by a frame the guest's kernel would never send."""
    ports = sorted(set(run.targets["loopback_ports"]))[:256]
    if not ports:
        run.report.line("UNRUN", "host-loopback", "Windows was found listening on no loopback port, so there is nothing to try")
        return
    attempts = [("127.0.0.1", port, 40000 + index) for index, port in enumerate(ports)]
    attack_frames(run, "host-loopback", attempts, lambda a: f"a SYN by raw frame to Windows' 127.0.0.1:{a[1]}")


def check_icmp(run: Run) -> None:
    targets = run.targets
    addresses = sorted({targets["gateway"], *targets["host_addresses"], *targets["lan"], *targets["wsl"]})
    answered = ping_v4(addresses, FRAME_WAIT)
    for address in addresses:
        if address in answered:
            run.report.line("FAIL", "icmp", f"ping {address}: it answered")
        else:
            run.report.line("PASS", "icmp", f"ping {address}: no answer in {FRAME_WAIT:g} s")


def own_ipv6_addresses(run: Run) -> set[str]:
    own = set()
    for line in run.read("/proc/net/if_inet6").splitlines():
        own.add(str(ipaddress.IPv6Address(bytes.fromhex(line.split()[0]))))
    return own


def check_icmp6(run: Run) -> None:
    interface, _ = default_route(run)
    answered = ping_v6_all_nodes(interface, own_ipv6_addresses(run), FRAME_WAIT)
    if answered:
        run.report.line("FAIL", "icmp6", f"all-nodes ping on {interface}: answered by {' '.join(sorted(answered))}")
    else:
        run.report.line("PASS", "icmp6", f"all-nodes ping on {interface}: nobody else answered in {FRAME_WAIT:g} s")


def checks_for(level: str) -> list:
    before = [
        ("where", check_where),
        ("control", check_control_loopback),
        ("control", check_control_clock),
        ("control", lambda run: check_control_network(run, "before the attacks")),
        ("control", check_control_link),
        ("vmbus", check_vmbus),
        ("mounts", check_mounts),
        ("devices", check_devices),
        ("vsock", check_vsock),
        ("host-tcp", check_host_tcp),
    ]
    middle = [("host-sweep", check_host_sweep)] if level == "user" else []
    after = [("host-udp", check_host_udp), ("lan", check_lan), ("wsl", check_wsl)]
    if level == "root":
        after += [("host-spoof", check_host_spoof), ("host-loopback", check_host_loopback), ("icmp", check_icmp), ("icmp6", check_icmp6)]
    after.append(("control", lambda run: check_control_network(run, "after the attacks")))
    return before + middle + after


def guarded(run: Run, category: str, check) -> None:
    """A check that breaks has tried nothing, or not everything. It says so, and the rest still run."""
    try:
        check(run)
    except Exception as error:  # noqa: BLE001 - the point: anything at all becomes a failure of the run, on one line.
        run.report.line("UNRUN", category, f"this check broke: {type(error).__name__}: {error}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--level", choices=("user", "root"), required=True)
    parser.add_argument("--fs-root", default="/", help="for this file's tests: read /proc, /sys and /dev from here")
    arguments = parser.parse_args(argv)

    report = Report(arguments.level, arguments.fs_root != "/")
    print(f"BEGIN level={arguments.level} uid={effective_uid()}", flush=True)
    targets = json.load(sys.stdin)
    report.note(
        "where",
        f"attacking {len(targets['host_addresses'])} addresses of this machine, {len(targets['lan'])} on its LAN, "
        f"{len(targets['wsl'])} of WSL's and {len(targets['host_ports'])} ports",
    )
    run = Run(arguments.level, targets, report, arguments.fs_root)
    for category, check in checks_for(arguments.level):
        guarded(run, category, check)
    return report.finish()


if __name__ == "__main__":
    sys.exit(main())

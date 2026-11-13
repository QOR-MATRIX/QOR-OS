# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Fixed public observations for the sealed installation lab, never guest exec."""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import select
import stat
import sys
import time
import uuid

from guest_inventory import expected_target, text

RECORD = b'{"schema":1,"completed":true}\n'
STATE = Path("/var/home/qortest/.local/state/QOR OS/qor-setup/completed.json")
EVENT_PORT = "dev.qorsync.vm-evidence.0"
AGENT_PORT = "org.qemu.guest_agent.0"
INSTALL_LOG = Path("/run/qor-vm-install/bootc.log")


def port_node(name):
    if name not in (EVENT_PORT, AGENT_PORT):
        raise ValueError("Unknown private port")
    node = (Path("/dev/virtio-ports") / name).resolve(strict=True)
    if node.parent != Path("/dev") or not re.fullmatch(r"vport[0-9]+p[0-9]+", node.name):
        raise ValueError("Unexpected virtual port node")
    entry = Path("/sys/class/virtio-ports") / node.name
    if (not re.fullmatch(r"/sys/devices/pci0000:00/0000:00:05\.0/virtio[0-9]+/virtio-ports/vport[0-9]+p[0-9]+",
                         str(entry.resolve(strict=True))) or text(entry / "name") != name):
        raise ValueError("Unexpected virtual port ancestry")
    info = node.stat()
    if not stat.S_ISCHR(info.st_mode) or text(entry / "dev") != f"{os.major(info.st_rdev)}:{os.minor(info.st_rdev)}":
        raise ValueError("Port node and kernel identity disagree")
    return node


def record_descriptor(path):
    if os.open in os.supports_dir_fd and hasattr(os, "O_DIRECTORY"):
        directory = os.open(path.anchor, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        try:
            for part in path.parts[1:-1]:
                next_directory = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=directory)
                os.close(directory)
                directory = next_directory
            return os.open(path.name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=directory)
        finally:
            os.close(directory)
    # Windows hosts run only inert file fixtures. Guest Linux always uses the
    # descriptor-relative no-follow walk above, including every parent.
    for parent in path.parents:
        if parent.is_symlink() or (parent.exists() and not parent.is_dir()):
            raise ValueError("Linked state ancestry")
    if path.is_symlink():
        raise ValueError("Linked state")
    return os.open(path, os.O_RDONLY)


def setup_observation(path=STATE):
    # Read only this application's public completion marker. Unknown bytes,
    # paths and errors are never copied into diagnostics or evidence.
    try:
        descriptor = record_descriptor(path)
        with os.fdopen(descriptor, "rb") as stream:
            info = os.fstat(stream.fileno())
            if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1 or info.st_size != len(RECORD):
                return {"state": "invalid"}
            if stream.read(len(RECORD) + 1) != RECORD:
                return {"state": "invalid"}
    except FileNotFoundError:
        return {"state": "pending"}
    except (OSError, ValueError):
        return {"state": "invalid"}
    return {"state": "complete", "record_sha256": hashlib.sha256(RECORD).hexdigest()}


def public_identity():
    expected = expected_target()
    boot = str(uuid.UUID(text(Path("/proc/sys/kernel/random/boot_id"))))
    return {"schema": 1, "vm_uuid": expected["vm_uuid"], "boot_id": boot,
            "source_revision": expected["source_revision"], "image_sha256": expected["image_sha256"]}


def send(descriptor, observation, deadline):
    raw = json.dumps(observation, separators=(",", ":")).encode() + b"\n"
    if len(raw) > 4096:
        raise ValueError("Oversized public event")
    while raw:
        remaining = deadline - time.monotonic()
        if remaining <= 0 or not select.select([], [descriptor], [], remaining)[1]:
            raise ValueError("Public event channel timed out")
        try:
            written = os.write(descriptor, raw)
        except BlockingIOError:
            continue
        if written <= 0:
            raise ValueError("Public event channel closed")
        raw = raw[written:]


def event(kind):
    if kind not in ("before-welcome", "after-welcome", "installed-boot"):
        raise ValueError("Unknown public event")
    observation = {**public_identity(), "event": kind, "setup": setup_observation()}
    if kind == "after-welcome":
        observation["process_success"] = (os.environ.get("SERVICE_RESULT") == "success"
                                          and os.environ.get("EXIT_CODE") == "exited"
                                          and os.environ.get("EXIT_STATUS") == "0")
    node = port_node(EVENT_PORT)
    descriptor = os.open(node, os.O_WRONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        send(descriptor, observation, time.monotonic() + 5)
    finally:
        os.close(descriptor)
    return observation


def installation_log(code):
    identity = public_identity()
    if type(code) is not int or not -255 <= code <= 255:
        raise ValueError("Unknown backend result")
    # Only this fixed, bounded log from the offline, credential-free bootc
    # transaction is exported. This is not a file-reading RPC.
    descriptor = record_descriptor(INSTALL_LOG)
    with os.fdopen(descriptor, "rb") as stream:
        info = os.fstat(stream.fileno())
        if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1 or info.st_size > 1200000:
            raise ValueError("Unknown installation diagnostics")
        raw = stream.read(1200001)
    if len(raw) > 1200000:
        raise ValueError("Oversized installation diagnostics")
    checksum = hashlib.sha256(raw).hexdigest()
    node = port_node(EVENT_PORT)
    descriptor = os.open(node, os.O_WRONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        deadline = time.monotonic() + 20
        for offset in range(0, len(raw), 2048):
            send(descriptor, {**identity, "event": "install-log-chunk", "offset": offset,
                              "base64": base64.b64encode(raw[offset:offset + 2048]).decode("ascii")}, deadline)
        send(descriptor, {**identity, "event": "install-result", "exit_code": code,
                          "log_bytes": len(raw), "log_sha256": checksum}, deadline)
    finally:
        os.close(descriptor)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("check-agent", "before-welcome", "after-welcome", "installed-boot"))
    args = parser.parse_args()
    try:
        if args.action == "check-agent":
            expected_target()
            port_node(AGENT_PORT)
        else:
            event(args.action)
        return 0
    except (OSError, ValueError, TypeError, KeyError):
        print("QOR VM session observation refused; preserve this guest's diagnostics.", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

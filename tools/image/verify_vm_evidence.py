# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Validate fresh private VM evidence; synthetic fixtures never establish a boot."""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import uuid

from vmguard import WORKSPACE, confined, identity, unlinked, verify
from vm_payload import unique_object
from vm_session import RECORD

RECORD_SHA256 = hashlib.sha256(RECORD).hexdigest()


def read(path, bound):
    path = unlinked(path)
    if identity(path)[2] > bound:
        raise ValueError("Oversized evidence")
    with path.open("rb") as stream:
        raw = stream.read(bound + 1)
    if len(raw) > bound:
        raise ValueError("Evidence grew beyond its bound")
    return raw


def observations(root, expected, phase):
    launch = json.loads(read(root / "launch.json", 64 * 1024), object_pairs_hook=unique_object)
    if not isinstance(launch, dict) or launch.get("phase") != phase or launch.get("allowlist") != expected:
        raise ValueError("Evidence belongs to a different VM, target or phase")
    qemu = json.loads(read(root / "qemu-command.json", 64 * 1024))
    if not isinstance(qemu, list) or any(not isinstance(item, str) for item in qemu):
        raise ValueError("Unknown QEMU launch evidence")
    if (qemu.count("-uuid") != 1 or qemu.index("-uuid") + 1 >= len(qemu)
            or qemu[qemu.index("-uuid") + 1] != expected["vm_uuid"]):
        raise ValueError("QEMU evidence belongs to a different VM")
    if phase == "installed" and any("media=cdrom" in item or item == "-cdrom" for item in qemu):
        raise ValueError("Installation media remained attached")
    raw = read(root / "guest-events.jsonl", 4 * 1024**2)
    events = []
    for line in raw.splitlines():
        if not line or len(line) > 4096:
            raise ValueError("Missing or oversized guest event")
        event = json.loads(line, object_pairs_hook=unique_object)
        if (not isinstance(event, dict) or event.get("schema") != 1 or type(event["schema"]) is not int
                or event.get("source_revision") != expected["source_revision"]
                or event.get("image_sha256") != expected["image_sha256"] or event.get("vm_uuid") != expected["vm_uuid"]
                or str(uuid.UUID(event["boot_id"])) != event["boot_id"]):
            raise ValueError("Guest source, image or boot identity differs")
        events.append(event)
    if not events or len({event["boot_id"] for event in events}) != 1:
        raise ValueError("One actual guest boot is required per evidence directory")
    return events


def completed(value):
    return value == {"state": "complete", "record_sha256": RECORD_SHA256}


def welcome(events, already_complete):
    before = [event for event in events if event.get("event") == "before-welcome"]
    after = [event for event in events if event.get("event") == "after-welcome"]
    if len(before) != 1 or len(after) != 1 or after[0].get("process_success") is not True:
        raise ValueError("Welcome has no single successful process result")
    if events.index(before[0]) >= events.index(after[0]) or not completed(after[0].get("setup")):
        raise ValueError("Welcome completion was not observed")
    if already_complete:
        if not completed(before[0].get("setup")):
            raise ValueError("Completion did not persist before the next welcome process")
    elif before[0].get("setup") != {"state": "pending"}:
        raise ValueError("First installed welcome did not begin pending")
    return before[0]["boot_id"]


def install_log(events):
    raw = bytearray()
    result = None
    for event in events:
        if event.get("event") == "install-log-chunk":
            if result is not None or type(event.get("offset")) is not int or event["offset"] != len(raw):
                raise ValueError("Installation log is stale, duplicated or out of order")
            raw.extend(base64.b64decode(event["base64"], validate=True))
            if len(raw) > 1200000:
                raise ValueError("Oversized installation log")
        elif event.get("event") == "install-result":
            if result is not None:
                raise ValueError("Ambiguous installation result")
            result = event
    if (result is None or type(result.get("exit_code")) is not int or result["exit_code"] != 0
            or not raw or type(result.get("log_bytes")) is not int or result["log_bytes"] != len(raw)
            or result.get("log_sha256") != hashlib.sha256(raw).hexdigest()):
        raise ValueError("No complete successful installation log")
    return bytes(raw)


def validate(expected, install, first, second):
    log = install_log(install)
    first_boot = welcome(first, False)
    second_boot = welcome(second, True)
    if len({install[0]["boot_id"], first_boot, second_boot}) != 3:
        raise ValueError("A stale boot was reused as reboot evidence")
    return {"schema": 1, "source_revision": expected["source_revision"], "image_sha256": expected["image_sha256"],
            "vm_uuid": expected["vm_uuid"], "disk_serial": expected["disk"]["serial"],
            "installed_boot_ids": [first_boot, second_boot], "completion_record_sha256": RECORD_SHA256,
            "install_log_sha256": hashlib.sha256(log).hexdigest(),
            "scope": "private VM evidence only; cancellation, visual review and hardware remain separate"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--allowlist", type=Path, required=True)
    parser.add_argument("--install", type=Path, required=True)
    parser.add_argument("--first", type=Path, required=True)
    parser.add_argument("--second", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True, help="Fresh private build directory for checked logs/result")
    args = parser.parse_args()
    try:
        expected = verify(args.allowlist)
        install = observations(args.install, expected, "live")
        result = validate(expected, install, observations(args.first, expected, "installed"),
                          observations(args.second, expected, "installed"))
        output = confined(args.output, WORKSPACE)
        output.mkdir(mode=0o700, exist_ok=False)
        result["evidence_sha256"] = {
            f"{phase}/{name}": hashlib.sha256(read(root / name, 4 * 1024**2)).hexdigest()
            for phase, root in (("install", args.install), ("first", args.first), ("second", args.second))
            for name in ("launch.json", "qemu-command.json", "guest-events.jsonl")}
        with (output / "install.log").open("xb") as stream:
            stream.write(install_log(install))
        with (output / "result.json").open("x", encoding="utf-8") as stream:
            json.dump(result, stream, indent=2)
            stream.write("\n")
        print(json.dumps(result, indent=2))
        return 0
    except (OSError, ValueError, TypeError, KeyError):
        parser.exit(1, "VM evidence is incomplete or inconsistent; no installation milestone accepted\n")


if __name__ == "__main__":
    raise SystemExit(main())

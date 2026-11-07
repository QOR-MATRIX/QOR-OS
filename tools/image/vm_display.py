# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Select the sealed VM's VirtIO display, never an assumed DRM card number."""
import json
import os
from pathlib import Path
import re
import stat
import sys

from guest_inventory import expected_target, text

CONFIG = Path("/run/qor-vm-display/kms.json")


def select_display(observations):
    candidates = []
    for card in observations:
        if (not isinstance(card, dict)
                or set(card) != {"path", "sysfs", "driver", "pci_driver", "vendor", "device_id",
                                     "virtio_sysfs", "major_minor", "connected"}):
            raise ValueError("Incomplete display observation")
        if (card["driver"] == "virtio_gpu" and card["pci_driver"] == "virtio-pci"
                and card["vendor"] == "0x1af4" and card["device_id"] == "0x1050"
                and card["connected"] is True
                and isinstance(card["sysfs"], str)
                and re.fullmatch(r"/sys/devices/pci0000:00/0000:00:01\.0/drm/card[0-9]+", card["sysfs"])
                and isinstance(card["virtio_sysfs"], str)
                and re.fullmatch(r"/sys/devices/pci0000:00/0000:00:01\.0/virtio[0-9]+", card["virtio_sysfs"])
                and isinstance(card["path"], str) and re.fullmatch(r"/dev/dri/card[0-9]+", card["path"])
                and Path(card["sysfs"]).name == Path(card["path"]).name
                and isinstance(card["major_minor"], str) and re.fullmatch(r"[0-9]+:[0-9]+", card["major_minor"])):
            candidates.append(card)
    if len(candidates) != 1:
        raise ValueError("No single connected display at the sealed VM's VirtIO slot")
    return candidates[0]


def collect():
    cards = []
    for entry in Path("/sys/class/drm").iterdir():
        if not re.fullmatch(r"card[0-9]+", entry.name):
            continue
        device = (entry / "device").resolve()
        if str(device) != "/sys/devices/pci0000:00/0000:00:01.0":
            continue
        children = [child for child in device.glob("virtio*")
                    if (child / "driver").resolve().name == "virtio_gpu"]
        if len(children) != 1:
            raise ValueError("Missing or ambiguous VirtIO graphics child")
        node = Path("/dev/dri") / entry.name
        info = node.stat()
        major_minor = text(entry / "dev")
        if not stat.S_ISCHR(info.st_mode) or major_minor != f"{os.major(info.st_rdev)}:{os.minor(info.st_rdev)}":
            raise ValueError("DRM node and kernel identity disagree")
        connectors = entry.parent.glob(entry.name + "-*/status")
        cards.append({"path": str(node), "sysfs": str(entry.resolve()),
                      "driver": (children[0] / "driver").resolve().name,
                      "pci_driver": (device / "driver").resolve().name,
                      "virtio_sysfs": str(children[0].resolve()),
                      "vendor": text(device / "vendor"), "device_id": text(device / "device"),
                      "major_minor": major_minor,
                      "connected": any(text(path) == "connected" for path in connectors)})
    return select_display(cards)


def main():
    try:
        # Refuse before any display discovery outside the sealed private VM.
        expected = expected_target()
        card = collect()
        for parent in CONFIG.parents:
            if parent.is_symlink() or not parent.is_dir():
                raise ValueError("Linked or unavailable runtime configuration folder")
        # This public device selector is consumed by the unprivileged welcome.
        # Its parent stays root-owned; no credentials or user data are written.
        descriptor = os.open(CONFIG, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o644)
        with os.fdopen(descriptor, "w") as stream:
            json.dump({"device": card["path"], "hwcursor": False}, stream)
            stream.write("\n")
        print(json.dumps({"type": "vm-display", "display": card, "vm_uuid": expected["vm_uuid"]}), flush=True)
        return 0
    except (OSError, ValueError, TypeError, KeyError):
        print("QOR VM display could not be verified. Use the diagnostics console; no automatic retry.", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())

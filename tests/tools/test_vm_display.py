# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Inert display observations: no real graphics device is opened by this suite."""
import importlib
import io
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/image"))
display = importlib.import_module("vm_display")


class VmDisplay(unittest.TestCase):
    def setUp(self):
        self.card = {"path": "/dev/dri/card1",
                     "sysfs": "/sys/devices/pci0000:00/0000:00:01.0/drm/card1",
                     "driver": "virtio_gpu", "pci_driver": "virtio-pci", "vendor": "0x1af4", "device_id": "0x1050",
                     "virtio_sysfs": "/sys/devices/pci0000:00/0000:00:01.0/virtio0",
                     "major_minor": "226:1", "connected": True}

    def test_card_one_is_selected_without_assuming_card_zero(self):
        firmware = {**self.card, "path": "/dev/dri/card0", "driver": "simpledrm"}
        self.assertEqual(display.select_display([firmware, self.card]), self.card)

    def test_wrong_ancestry_driver_node_and_disconnected_display_refused(self):
        for field, value in (("path", "/dev/dri/card0"), ("driver", "amdgpu"), ("major_minor", "bad"),
                             ("sysfs", self.card["sysfs"].replace("01.0", "02.0")), ("connected", False),
                             ("pci_driver", "unexpected"), ("vendor", "0xffff"), ("device_id", "0xffff"),
                             ("virtio_sysfs", self.card["virtio_sysfs"].replace("01.0", "02.0"))):
            with self.subTest(field=field), self.assertRaises(ValueError):
                display.select_display([{**self.card, field: value}])

    def test_the_same_verified_device_can_be_card_zero(self):
        card = {**self.card, "path": "/dev/dri/card0", "sysfs": self.card["sysfs"].replace("card1", "card0"),
                "major_minor": "226:0"}
        self.assertEqual(display.select_display([card]), card)

    def test_missing_or_ambiguous_display_refused(self):
        for cards in ([], [{}], [self.card, self.card]):
            with self.subTest(count=len(cards)), self.assertRaises(ValueError):
                display.select_display(cards)

    def test_missing_firmware_contract_fails_before_display_discovery(self):
        with patch.object(display, "expected_target", side_effect=ValueError), \
                patch.object(display, "collect") as collect, patch.object(display.sys, "stderr", io.StringIO()):
            self.assertEqual(display.main(), 1)
            collect.assert_not_called()


if __name__ == "__main__":
    unittest.main()

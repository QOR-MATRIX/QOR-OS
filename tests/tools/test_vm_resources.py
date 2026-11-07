# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Memory rejection must precede VM creation or an expensive ISO hash."""
import importlib
from pathlib import Path
import sys
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/image"))
resources = importlib.import_module("vm_resources")
launcher = importlib.import_module("vmrun")
builder = importlib.import_module("build_vm_probe")


class VmResources(unittest.TestCase):
    def test_budget_and_unknown_availability(self):
        self.assertEqual(resources.require_headroom("fixture", reader=lambda: 4 * resources.GIB)["minimum_bytes"],
                         3 * resources.GIB)
        for available in (327 * 1024**2, 3 * resources.GIB - 1, -1, True, None):
            with self.subTest(available=available), self.assertRaises(ValueError):
                resources.require_headroom("fixture", reader=lambda: available)

    def test_low_memory_prevents_vm_hash_output_and_launch(self):
        with patch.object(launcher, "verify", return_value={}), \
                patch.object(launcher, "require_headroom", side_effect=ValueError), \
                patch.object(launcher, "checked_iso") as image, \
                patch.object(launcher, "docker_command") as docker:
            with self.assertRaises(ValueError):
                launcher.run(Path("fixture/allowlist.json"), Path("fixture/media.iso"),
                             Path("fixture/evidence"), "sha256:" + "a" * 64, "live")
            image.assert_not_called()
            docker.assert_not_called()

    def test_low_memory_prevents_image_build_and_log_creation(self):
        log = Mock()
        with patch.object(builder, "require_headroom", side_effect=ValueError), \
                patch.object(builder.subprocess, "run") as run:
            with self.assertRaises(ValueError):
                builder.checked_image_build(["inert-build-fixture"], log)
            run.assert_not_called()
            log.open.assert_not_called()


if __name__ == "__main__":
    unittest.main()

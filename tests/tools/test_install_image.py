# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Image assembly and rejection tests; device files/probes here are stand-ins."""
from pathlib import Path
import importlib.util
import json
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools/image"))
import assemble
import check_image
from assets import INPUTS


class InstallImage(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.repo = Path(self.temp.name).resolve() / "repo"
        names = (*INPUTS, "kit/branding/manifest.json", "kit/branding/qor-mark.png",
                 "kit/branding/qor-mark.png.license", "image/branding/qor-os.plymouth",
                 "image/branding/qor-os.script.in", "image/branding/plymouthd.conf",
                 "setup/Welcome.qml", "tools/image/check_image.py", "image/os-release")
        for name in names:
            path = self.repo / name
            path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(REPO / name, path)
        shutil.copytree(REPO / "image/installer", self.repo / "image/installer")
        self.output = self.repo / "build/image-payload"

    def test_real_assets_are_in_complete_payload(self):
        self.assertEqual(assemble.stage(self.output, self.repo), 25)
        self.assertEqual(assemble.verify(self.output, self.repo), 25)
        brand = json.loads((self.output / (assemble.BRAND + "branding.desc")).read_text())
        self.assertEqual(brand["strings"]["productName"], "QOR OS")
        self.assertEqual((self.output / (assemble.BRAND + "qor-mark.png")).read_bytes(),
                         (self.repo / "kit/branding/qor-mark.png").read_bytes())
        self.assertFalse((self.output / "etc/calamares/settings.conf").exists())
        caps = json.loads((self.output / "usr/share/qor-os/installer/capabilities.json").read_text())
        self.assertIsNone(caps["bootcDeploymentAdapter"])
        self.assertEqual(caps["storageModesProved"], [])

    def test_candidate_disk_and_reboot_actions_are_unselected(self):
        files = assemble.payload(self.repo)
        self.assertIn(b"initialPartitioningChoice: none", files[assemble.INACTIVE + "partition.conf"])
        self.assertIn(b"restartNowMode: never", files[assemble.INACTIVE + "finished.conf"])
        settings = files[assemble.INACTIVE + "settings.conf.in"]
        self.assertIn(b"prompt-install: true", settings)
        self.assertIn(b"@VERIFIED_DISK_REVIEW_MODULE@", settings)
        self.assertIn(b"@VERIFIED_BOOTC_DEPLOYMENT_SEQUENCE@", settings)

    def test_missing_or_modified_entries_fail_without_overwriting(self):
        assemble.stage(self.output, self.repo)
        target = self.output / (assemble.BRAND + "branding.desc")
        target.write_bytes(b"unfinished work")
        with self.assertRaises(ValueError):
            assemble.stage(self.output, self.repo)
        self.assertEqual(target.read_bytes(), b"unfinished work")

    def test_incomplete_payload_fails(self):
        assemble.stage(self.output, self.repo)
        (self.output / (assemble.BRAND + "qor-mark.png")).unlink()
        with self.assertRaises(ValueError):
            assemble.verify(self.output, self.repo)

    def test_unresolved_branding_field_fails_before_staging(self):
        path = self.repo / "image/installer/branding.desc.in"
        path.write_text(path.read_text().replace("QOR OS", "@UNKNOWN@"))
        with self.assertRaises(ValueError):
            assemble.stage(self.output, self.repo)
        self.assertFalse(self.output.exists())

    def test_output_cannot_target_host_filesystem(self):
        with self.assertRaises(ValueError):
            assemble.stage(self.repo / "usr", self.repo)
        self.assertFalse((self.repo / "usr").exists())

    def test_changed_logo_source_is_refused(self):
        (self.repo / "kit/QorMark.qml").write_text("changed source")
        with self.assertRaises(ValueError):
            assemble.stage(self.output, self.repo)

    def test_mutable_image_tags_are_refused_without_docker(self):
        with patch.object(assemble.subprocess, "run") as run:
            with self.assertRaises(ValueError):
                assemble.existing_image("fedora:latest")
            run.assert_not_called()

    def test_unavailable_digest_does_not_pull(self):
        with patch.object(assemble.subprocess, "run") as run:
            run.return_value.returncode = 1
            with self.assertRaises(ValueError):
                assemble.existing_image("registry.invalid/qor/base@sha256:" + "a" * 64)
            self.assertEqual(run.call_count, 1)
            self.assertEqual(run.call_args.args[0][:3], ["docker", "image", "inspect"])

    def test_device_build_requires_approved_base_before_docker(self):
        assemble.stage(self.output, self.repo)
        with patch.object(assemble.subprocess, "run") as run:
            with self.assertRaises(ValueError):
                assemble.build_device(self.output, None, self.repo)
            run.assert_not_called()

    def device_fixture(self):
        for name in check_image.REQUIRED:
            path = self.repo / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"inert structural-test stand-in, never executed")
        (self.repo / "usr/lib/os-release").write_bytes((REPO / "image/os-release").read_bytes())

    def test_missing_device_components_block_before_probe(self):
        with patch.object(check_image.subprocess, "run") as run:
            self.assertEqual(len(check_image.check(self.repo, lambda _: self.fail("must not probe"))), 5)
            run.assert_not_called()

    def test_development_application_cannot_be_device_payload(self):
        self.device_fixture()
        self.assertTrue(check_image.check(self.repo, lambda _: {"schema": 1, "development": True, "firstRun": True}))

    def test_valid_structural_fixture_is_not_install_proof(self):
        self.device_fixture()
        self.assertEqual(check_image.check(self.repo, lambda _: {"schema": 1, "development": False, "firstRun": True}), [])

    def test_unknown_or_malformed_capabilities_fail(self):
        self.device_fixture()
        for data in (None, [], {"schema": True, "development": False, "firstRun": True},
                     {"schema": 1, "development": False, "firstRun": 1}):
            self.assertTrue(check_image.check(self.repo, lambda _, data=data: data))

    def test_live_or_active_installer_is_blocked(self):
        self.device_fixture()
        marker = self.repo / "usr/lib/qor-os/live-session"
        marker.parent.mkdir(parents=True)
        marker.touch()
        self.assertIn("Live installation backend", " ".join(check_image.check(self.repo)))
        config = self.repo / "etc/calamares/settings.conf"
        config.parent.mkdir(parents=True)
        config.write_text("inert test data")
        self.assertIn("active installer", " ".join(check_image.check(self.repo)))


if __name__ == "__main__":
    unittest.main()

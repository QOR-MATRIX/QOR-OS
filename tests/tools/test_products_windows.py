# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""The real PowerShell product wrapper, with product builds stood in for; no Qt windows or kit are used."""

import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO = pathlib.Path(__file__).resolve().parents[2]
POWERSHELL = shutil.which("pwsh")


@unittest.skipUnless(POWERSHELL, "needs PowerShell 7")
class ProductsWindows(unittest.TestCase):
    def run_products(self, behaviours, attempts=1, inherited=0):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            wrapper = root / "tools/dev/products.ps1"
            wrapper.parent.mkdir(parents=True)
            shutil.copy2(REPO / "tools/dev/products.ps1", wrapper)
            for name, path in (("qq", "products/qq/build.ps1"), ("arqade", "products/arqade/qt/build.ps1"),
                               ("media", "tools/qor-media/build.ps1")):
                script = root / path
                script.parent.mkdir(parents=True, exist_ok=True)
                body = {
                    "pass": "Write-Host 'stand-in passed'",
                    "exit": "exit 17",
                    "native": "& $env:QOR_TEST_PYTHON -c 'import sys; sys.exit(23)'",
                    "throw": "throw 'stand-in failure'",
                    "flaky": "if (-not (Test-Path (Join-Path $env:QOR_TEST_MARKS 'flaked'))) { "
                             "New-Item (Join-Path $env:QOR_TEST_MARKS 'flaked') | Out-Null; exit 17 }; exit 0",
                }[behaviours[name]]
                script.write_text("param($Qt, $Build)\n"
                                  f"Add-Content (Join-Path $env:QOR_TEST_MARKS 'ran') '{name}'\n{body}\n", encoding="utf-8")
            driver = root / "run.ps1"
            driver.write_text(f"$global:LASTEXITCODE = {inherited}\n"
                              "$PSNativeCommandUseErrorActionPreference = $false\n"
                              f"& '{wrapper}' -Qt unused -BuildRoot '{root / 'build'}' -Attempts {attempts}\n", encoding="utf-8")
            done = subprocess.run([POWERSHELL, "-NoProfile", "-NonInteractive", "-File", str(driver)],
                                  env=dict(os.environ, QOR_TEST_MARKS=str(root), QOR_TEST_PYTHON=sys.executable),
                                  capture_output=True, text=True, timeout=60)
            ran = (root / "ran").read_text(encoding="utf-8-sig").splitlines()
            return done.returncode, done.stdout + done.stderr, ran

    def test_a_script_exit_failure_is_not_success(self):
        code, said, ran = self.run_products({"qq": "exit", "arqade": "pass", "media": "pass"})
        self.assertNotEqual(code, 0, said)
        self.assertNotIn("Products built and tested:", said)
        self.assertEqual(ran, ["qq", "arqade", "media"])

    def test_a_native_failure_is_not_success(self):
        code, said, _ = self.run_products({"qq": "native", "arqade": "pass", "media": "pass"})
        self.assertNotEqual(code, 0, said)
        self.assertNotIn("Products built and tested:", said)

    def test_a_throw_is_reported_and_other_products_still_run(self):
        code, said, ran = self.run_products({"qq": "throw", "arqade": "pass", "media": "pass"})
        self.assertNotEqual(code, 0, said)
        self.assertEqual(ran, ["qq", "arqade", "media"])
        self.assertNotIn("Products built and tested:", said)

    def test_a_later_success_is_named(self):
        code, said, ran = self.run_products({"qq": "flaky", "arqade": "pass", "media": "pass"}, attempts=2)
        self.assertEqual(code, 0, said)
        self.assertEqual(ran, ["qq", "qq", "arqade", "media"])
        self.assertIn("Passed on a second attempt: qq", said)

    def test_a_failure_in_the_last_product_fails_the_run(self):
        code, said, ran = self.run_products({"qq": "pass", "arqade": "pass", "media": "exit"})
        self.assertNotEqual(code, 0, said)
        self.assertEqual(ran, ["qq", "arqade", "media"])
        self.assertNotIn("Products built and tested:", said)

    def test_a_stale_inherited_exit_does_not_fail_success(self):
        code, said, ran = self.run_products({"qq": "pass", "arqade": "pass", "media": "pass"}, inherited=93)
        self.assertEqual(code, 0, said)
        self.assertEqual(ran, ["qq", "arqade", "media"])
        self.assertIn("Products built and tested:", said)


if __name__ == "__main__":
    unittest.main()

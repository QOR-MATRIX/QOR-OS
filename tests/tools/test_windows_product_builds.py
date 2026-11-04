# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Real product build scripts, fake build tools and real CTest; never launch a Qt program or download anything."""

import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO = pathlib.Path(__file__).resolve().parents[2]
POWERSHELL = shutil.which("pwsh")
CTEST = shutil.which("ctest") or (str(pathlib.Path("C:/Qt/Tools/CMake_64/bin/ctest.exe")) if os.name == "nt" else None)
SCRIPTS = ("products/qq/build.ps1", "products/arqade/qt/build.ps1", "tools/qor-media/build.ps1")


@unittest.skipUnless(os.name == "nt" and POWERSHELL and CTEST and pathlib.Path(CTEST).is_file(), "needs Windows, PowerShell and CTest")
class WindowsProductBuilds(unittest.TestCase):
    def run_build(self, script_path, mode, *, old_record=False, build_fails=False, refuse_cleanup=False):
        made = tempfile.TemporaryDirectory()
        self.addCleanup(made.cleanup)
        root = pathlib.Path(made.name)
        build = root / "build"
        build.mkdir()
        for path in (*SCRIPTS, "tools/dev/products-tests.ps1", "tools/dev/products-report.py"):
            if (REPO / path).exists():
                (root / path).parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(REPO / path, root / path)
        kit = root / "kit/6.12.0/msvc2022_64"
        vs = root / "vs"
        for path in (root / "kit/Tools/CMake_64/bin/cmake.exe", root / "kit/Tools/CMake_64/bin/ctest.exe",
                     root / "kit/Tools/Ninja/ninja.exe", vs / "VC/Auxiliary/Build/vcvars64.bat"):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.touch()
        media = (REPO / SCRIPTS[2]).read_text(encoding="utf-8")
        release = re.search(r"\$pdfiumRelease = '([^']+)'", media)[1].replace("/", "-")
        digest = re.search(r"\$pdfiumSha256 = '([^']+)'", media)[1]
        pdfium = root / "appdata/qor-media-pdfium" / release
        pdfium.mkdir(parents=True)
        (pdfium / "pdfium-win-x64.tgz").touch()
        (pdfium / "PDFiumConfig.cmake").touch()
        self.old_record = build / "Testing/prior/Test.xml"
        if old_record:
            self.old_record.parent.mkdir(parents=True)
            self.old_record.write_text('<Site><Testing><Test Status="passed"><Name>stand_in</Name><Results/></Test></Testing></Site>')
            testing = self.old_record.parent.parent
            (testing / "Temporary").mkdir()
            (testing / "TAG").write_text("prior\nExperimental\n")
            (testing / "Temporary/LastTest_prior.log").write_text("1/1 Testing: stand_in\nTotals: 1 passed, 0 failed, 0 skipped\n")
        (build / "tests").mkdir()
        (build / "CTestTestfile.cmake").write_text('subdirs("tests")\n')
        (root / "suite.py").write_text(
            "import pathlib,sys,time\npathlib.Path(sys.argv[1]).write_text('ran')\n"
            "if len(sys.argv) > 2:\n"
            "    active = pathlib.Path(sys.argv[2])\n"
            "    active.mkdir()\n"
            "    try: time.sleep(0.6)\n"
            "    finally: active.rmdir()\n"
            "print('********* Start testing of Stand *********')\n"
            f"print('Totals: 1 passed, {int(mode == 'fails')} failed, 0 skipped, 0 blacklisted, 1ms')\n"
            f"sys.exit({77 if mode == 'skips' else int(mode == 'fails')})\n")
        test = (f'add_test(stand_in "{pathlib.Path(sys.executable).as_posix()}" -I "{(root / "suite.py").as_posix()}" '
                f'"{(root / "ran").as_posix()}")\nset_tests_properties(stand_in PROPERTIES SKIP_RETURN_CODE 77)\n')
        if mode == "concurrent":
            test = "".join(f'add_test({name} "{pathlib.Path(sys.executable).as_posix()}" -I "{(root / "suite.py").as_posix()}" '
                           f'"{(root / "ran").as_posix()}" "{(root / "active").as_posix()}")\n' for name in ("first", "second"))
        (build / "tests/CTestTestfile.cmake").write_text("" if mode == "empty" else test)
        driver = root / "driver.ps1"
        driver.write_text(r'''
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
Set-Item -LiteralPath "Function:global:$vswhere" -Value { $env:QOR_TEST_VS }
function global:cmd {
    New-Item $env:QOR_TEST_BUILD_CALLED -ItemType File -Force | Out-Null
    $global:LASTEXITCODE = [int]$env:QOR_TEST_BUILD_EXIT
}
function global:Get-FileHash { [pscustomobject]@{ Hash = $env:QOR_TEST_PDF_HASH } }
function global:Invoke-WebRequest { throw 'A stand-in test must never download anything.' }
Set-Item -LiteralPath "Function:global:$env:QOR_TEST_FAKE_CTEST" -Value {
    if ($env:QOR_TEST_CTEST_NOOP -eq '1') { $global:LASTEXITCODE = 0; return }
    & $env:QOR_TEST_CTEST @args
}
function global:python { & $env:QOR_TEST_PYTHON @args }
function global:Remove-Item {
    [CmdletBinding()] param([string]$LiteralPath, [switch]$Force)
    if ($env:QOR_TEST_REFUSE_CLEANUP -eq '1' -and $LiteralPath.EndsWith('Test.xml')) { throw 'Record cleanup refused.' }
    Microsoft.PowerShell.Management\Remove-Item @PSBoundParameters
}
& $env:QOR_TEST_SCRIPT -Qt $env:QOR_TEST_KIT -Build $env:QOR_TEST_BUILD
''', encoding="utf-8")
        done = subprocess.run([POWERSHELL, "-NoProfile", "-NonInteractive", "-File", str(driver)],
                              env=dict(os.environ, LOCALAPPDATA=str(root / "appdata"), QOR_TEST_VS=str(vs),
                                       CTEST_PARALLEL_LEVEL="2" if mode == "concurrent" else "1",
                                       QOR_TEST_BUILD_EXIT="17" if build_fails else "0", QOR_TEST_PDF_HASH=digest,
                                       QOR_TEST_BUILD_CALLED=str(root / "build-called"), QOR_TEST_REFUSE_CLEANUP="1" if refuse_cleanup else "0",
                                       QOR_TEST_FAKE_CTEST=str(root / "kit/Tools/CMake_64/bin/ctest.exe"), QOR_TEST_CTEST=CTEST,
                                       QOR_TEST_CTEST_NOOP="1" if mode == "noop" else "0", QOR_TEST_PYTHON=sys.executable,
                                       QOR_TEST_SCRIPT=str(root / script_path), QOR_TEST_KIT=str(kit), QOR_TEST_BUILD=str(build)),
                              capture_output=True, text=True, timeout=60)
        self.build_was_called = (root / "build-called").exists()
        return done.returncode, done.stdout + done.stderr, (root / "ran").exists()

    def test_no_registered_suites_is_not_success(self):
        for script in SCRIPTS:
            with self.subTest(script=script):
                code, said, ran = self.run_build(script, "empty")
                self.assertNotEqual(code, 0, said)
                self.assertFalse(ran)

    def test_registered_nested_suite_really_runs(self):
        for script in SCRIPTS:
            with self.subTest(script=script):
                code, said, ran = self.run_build(script, "passes")
                self.assertEqual(code, 0, said)
                self.assertTrue(ran, said)
                self.assertIn("stand_in: 1 passed", said)

    def test_failing_or_skipped_suite_does_not_pass(self):
        for script in SCRIPTS:
            for mode in ("fails", "skips"):
                with self.subTest(script=script, mode=mode):
                    code, said, ran = self.run_build(script, mode)
                    self.assertNotEqual(code, 0, said)
                    self.assertTrue(ran)

    def test_noop_ctest_cannot_reuse_an_old_pass(self):
        for script in SCRIPTS:
            with self.subTest(script=script):
                code, said, ran = self.run_build(script, "noop", old_record=True)
                self.assertNotEqual(code, 0, said)
                self.assertFalse(ran)

    def test_failed_build_invalidates_old_test_record(self):
        for script in SCRIPTS:
            with self.subTest(script=script):
                code, said, ran = self.run_build(script, "passes", old_record=True, build_fails=True)
                self.assertNotEqual(code, 0, said)
                self.assertFalse(ran)
                self.assertFalse(self.old_record.exists(), "A build failure left a previous passing test record")

    def test_failed_invalidation_stops_before_build_or_tests(self):
        for script in SCRIPTS:
            with self.subTest(script=script):
                code, said, ran = self.run_build(script, "passes", old_record=True, refuse_cleanup=True)
                self.assertNotEqual(code, 0, said)
                self.assertFalse(ran)
                self.assertFalse(self.build_was_called)
                self.assertIn("Record cleanup refused", said)

    def test_inherited_parallelism_cannot_overlap_product_suites(self):
        for script in SCRIPTS:
            with self.subTest(script=script):
                code, said, ran = self.run_build(script, "concurrent")
                self.assertEqual(code, 0, said)
                self.assertTrue(ran)
                self.assertIn("first: 1 passed", said)
                self.assertIn("second: 1 passed", said)


if __name__ == "__main__":
    unittest.main()

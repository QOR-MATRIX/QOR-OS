# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Tests for tools/ci/check_runner_boundary.sh, and for what tools/ci/isolated-linux-runner.sh and .ps1 say.

What a real distribution does can only be seen in one: `isolated-linux-runner.ps1 -Action Check`. These hold what can be
held anywhere: the check's search by name, and the lines of the sandbox that each exist because something got through
without them. Nothing here starts WSL.
"""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
CHECK = REPO / "tools" / "ci" / "check_runner_boundary.sh"
INSIDE = REPO / "tools" / "ci" / "isolated-linux-runner.sh"
OUTSIDE = REPO / "tools" / "ci" / "isolated-linux-runner.ps1"
MARKER = "the-contents-of-this-file-must-never-be-printed"


def find_bash() -> str | None:
    if os.name == "nt":
        # Never WSL's bash.exe, which is first on some PATHs and would start a distribution.
        for root in (os.environ.get("ProgramW6432"), os.environ.get("ProgramFiles")):
            candidate = Path(root or "") / "Git" / "bin" / "bash.exe"
            if candidate.is_file():
                return str(candidate)
        return None
    return shutil.which("bash")


BASH = find_bash()


@unittest.skipUnless(BASH, "needs bash, and on Windows Git's")
class TheSearchByName(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        self.root = Path(folder.name)
        self.home = self.root / "home" / "runner"
        self.home.mkdir(parents=True)

    def run_check(self, *arguments: str) -> subprocess.CompletedProcess:
        arguments = arguments or ("--files-only", self.root.as_posix(), self.home.as_posix())
        return subprocess.run(
            [BASH, CHECK.as_posix(), *arguments], capture_output=True, text=True, encoding="utf-8", timeout=120, check=False
        )

    def plant(self, relative: str, under: Path | None = None) -> Path:
        path = (under or self.root) / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(MARKER, encoding="utf-8")
        return path

    def assert_named(self, name: str, shown: str | None = None) -> None:
        done = self.run_check()
        self.assertEqual(done.returncode, 1, done.stdout)
        self.assertRegex(done.stdout, rf"(?m)^FAIL\s+{re.escape(name)}\s")
        self.assertIn(name, done.stdout.splitlines()[-1], "the last line names each failure")
        if shown:
            self.assertIn(shown, done.stdout)

    def assert_passes(self) -> None:
        done = self.run_check()
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertNotIn("FAIL", done.stdout)

    def test_a_clean_home_passes(self):
        self.assert_passes()

    def test_each_credential_is_found_and_named(self):
        for relative, shown in (
            (".ssh/config", ".ssh"),
            (".aws/credentials", ".aws"),
            (".config/gh/hosts.yml", ".config/gh"),
            (".npmrc", ".npmrc"),
            (".netrc", ".netrc"),
            (".git-credentials", ".git-credentials"),
            (".docker/config.json", ".docker/config.json"),
            (".cargo/credentials.toml", ".cargo/credentials.toml"),
            (".local/share/Qt/qtaccount.ini", ".local/share/Qt/qtaccount.ini"),
            (".env", ".env"),
        ):
            with self.subTest(relative):
                self.plant(relative, self.home)
                self.assert_named("no-credentials", shown)
                shutil.rmtree(self.home, ignore_errors=True)
                self.home.mkdir(parents=True)

    @unittest.skipIf(os.name == "nt", "a link needs a privilege there")
    def test_a_link_to_nowhere_still_counts(self):
        (self.home / ".ssh").symlink_to("/nowhere")
        self.assert_named("no-credentials", ".ssh")

    def test_a_qt_account_is_found_anywhere(self):
        self.plant("opt/somewhere/deep/qtaccount.ini")
        self.assert_named("no-qt-account", "qtaccount.ini")

    def test_the_stores_keys_are_found_by_their_names(self):
        for relative in (
            "elsewhere/store.pem",
            "elsewhere/developer-org.example.pem",
            "home/other/.qor-os/store-signing/a-file",
            "home/other/.qtos/a-file",
        ):
            with self.subTest(relative):
                planted = self.plant(relative)
                self.assert_named("no-store-key", planted.name if planted.suffix == ".pem" else None)
                shutil.rmtree(self.root / relative.split("/")[0])
                self.home.mkdir(parents=True, exist_ok=True)

    def test_the_stores_keys_have_no_exception_anywhere(self):
        # A dependency's tree excuses a key by a general name, and never one by the Store's.
        self.plant(".cargo/registry/src/a-crate/tests/store.pem", self.home)
        self.assert_named("no-store-key", "store.pem")

    def test_a_private_key_by_a_general_name_is_found(self):
        for relative in ("id_rsa", "id_ed25519", "work/my_private_key.pem", "work/signing_private.pem", "work/a.p12", "work/a.pfx", "work/a.ppk"):
            with self.subTest(relative):
                planted = self.plant(relative, self.home)
                self.assert_named("no-private-key", planted.name)
                planted.unlink()

    def test_the_stores_public_key_is_fine(self):
        self.plant("qor-os/store/keys/store.pub", self.home)
        self.plant("etc/ssl/certs/a-public-certificate.pem")
        self.assert_passes()

    def test_a_dependencys_own_test_data_is_skipped(self):
        self.plant(".cargo/registry/src/a-crate/tests/id_rsa", self.home)
        self.plant("qor-os/build/ci-linux/_deps/a-library/tests/private_key.pem", self.home)
        self.plant("qor-os/target/release/build/x/private_key.pem", self.home)
        self.assert_passes()

    def test_a_folder_with_a_keys_name_is_not_a_key(self):
        (self.home / "private_key_tests").mkdir()
        self.assert_passes()

    def test_contents_are_never_printed(self):
        self.plant(".ssh/id_ed25519", self.home)
        self.plant("elsewhere/store.pem")
        self.plant("elsewhere/qtaccount.ini")
        done = self.run_check()
        self.assertEqual(done.returncode, 1)
        self.assertNotIn(MARKER, done.stdout + done.stderr)

    def test_every_failure_is_named_together(self):
        self.plant(".npmrc", self.home)
        self.plant("elsewhere/store.pem")
        done = self.run_check()
        self.assertEqual(done.returncode, 1)
        last = done.stdout.splitlines()[-1]
        self.assertIn("no-credentials", last)
        self.assertIn("no-store-key", last)

    def test_it_says_this_proves_nothing_about_a_machine(self):
        self.assertIn("says nothing about a machine", self.run_check().stdout)

    def test_a_wrong_call_is_not_a_pass(self):
        self.assertEqual(self.run_check("--files-only", self.root.as_posix()).returncode, 2)


class TheSandbox(unittest.TestCase):
    """Each line here was measured. Without it, something got through on 1 November 2026."""

    @classmethod
    def setUpClass(cls):
        cls.inside = INSIDE.read_text(encoding="utf-8")
        found = re.search(r"cat > /etc/qor-ci/sandbox\.conf << 'EOF'\n(.*?)\nEOF\n", cls.inside, re.S)
        assert found, "the sandbox's list is not where this test looks"
        cls.lines = [line for line in found.group(1).splitlines() if line and not line.startswith("#")]

    def values(self, key: str) -> list[str]:
        return [line.split("=", 1)[1] for line in self.lines if line.startswith(key + "=")]

    def test_the_door_to_windows_file_server_is_shut(self):
        # An unprivileged process opened AF_VSOCK, and Windows' 9P server attached it to C:\.
        self.assertEqual(self.values("RestrictAddressFamilies"), ["AF_UNIX AF_INET AF_INET6 AF_NETLINK"])

    def test_the_filter_cannot_be_walked_around(self):
        # io_uring opens a socket without socket(), and a 32-bit call goes by another route.
        denied = " ".join(self.values("SystemCallFilter"))
        for call in ("io_uring_setup", "io_uring_enter", "io_uring_register"):
            self.assertIn(call, denied)
        self.assertEqual(self.values("SystemCallArchitectures"), ["native"])
        self.assertEqual(self.values("RestrictNamespaces"), ["yes"])
        self.assertEqual(self.values("NoNewPrivileges"), ["yes"])
        self.assertEqual(self.values("CapabilityBoundingSet"), [""])

    def test_ptrace_stays_for_the_sanitizers(self):
        # LeakSanitizer needs it, and the same suites run under it on this runner.
        self.assertNotIn("@debug", " ".join(self.values("SystemCallFilter")))
        self.assertNotIn("ptrace", " ".join(self.values("SystemCallFilter")))

    def test_nothing_outside_can_be_asked_to_act(self):
        self.assertEqual(self.values("TemporaryFileSystem"), ["/run:ro"])
        self.assertEqual(self.values("PrivateDevices"), ["yes"])
        self.assertEqual(self.values("ProtectSystem"), ["strict"])
        self.assertEqual(self.values("ReadWritePaths"), ["/home/runner"])
        for path in ("-/mnt", "-/init", "-/usr/lib/wsl"):
            self.assertIn(path, " ".join(self.values("InaccessiblePaths")))

    def test_it_runs_as_runner(self):
        self.assertEqual(self.values("User"), ["runner"])
        self.assertEqual(self.values("Group"), ["runner"])

    def test_the_network_shuts_windows_and_the_other_distribution_out(self):
        denied = self.values("IPAddressDeny")
        for prefix in ("localhost", "link-local", "multicast", "10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16", "100.64.0.0/10", "fc00::/7"):
            self.assertIn(prefix, denied, "one to a line: systemd-run takes the names only as a whole value")
        # 10.255.255.254, WSL's DNS, was allowed once, and a listener on all addresses in the namespace answered on it.
        self.assertEqual(self.values("IPAddressAllow"), [])

    def test_one_machine(self):
        self.assertEqual(self.values("CPUQuota"), ["800%"])
        self.assertIn("CMAKE_BUILD_PARALLEL_LEVEL=8", self.values("Environment"))
        self.assertIn("CARGO_BUILD_JOBS=8", self.values("Environment"))

    def test_wsl_conf_says_what_it_must(self):
        found = re.search(r"cat > /etc/wsl\.conf << 'EOF'\n(.*?)\nEOF\n", self.inside, re.S)
        self.assertIsNotNone(found)
        conf = found.group(1)
        for section, setting in (
            ("automount", "enabled=false"),
            ("automount", "mountFsTab=false"),
            ("interop", "enabled=false"),
            ("interop", "appendWindowsPath=false"),
            ("boot", "systemd=true"),
            ("user", "default=runner"),
            ("gpu", "enabled=false"),
            ("network", "generateResolvConf=false"),
        ):
            with self.subTest(f"{section}.{setting}"):
                self.assertRegex(conf, rf"\[{section}\]\n(?:[^\[\n]*\n)*?{re.escape(setting)}")

    def test_units_that_write_to_what_is_shared_are_masked_before_anything_else(self):
        # With interop off, systemd-binfmt emptied the kernel's one table, and the other distribution's cmd.exe stopped.
        body = re.search(r"mask_shared_units\(\) \{\n(.*?)\n\}", self.inside, re.S).group(1)
        for unit in ("systemd-binfmt.service", "systemd-sysctl.service", "systemd-networkd.service", "systemd-udevd.service"):
            self.assertIn(unit, body)
        provision = re.search(r"\nprovision\(\) \{\n(.*?)\n\}", self.inside, re.S).group(1)
        self.assertEqual(provision.split()[0], "mask_shared_units")

    def test_nothing_ever_stops_binfmt(self):
        # Its ExecStop empties the table for every distribution. The warning about it, in a comment, may say so.
        code = "\n".join(line for line in self.inside.splitlines() if not line.lstrip().startswith("#"))
        self.assertNotRegex(code, r"systemctl\s+stop\s+\S*binfmt")
        self.assertNotRegex(code, r"--now[^\n]*binfmt")

    def test_the_seal_goes_private_before_it_unmounts(self):
        # /mnt/wsl is one tmpfs propagated to every distribution: an unmount that is not private first takes it from them.
        seal = re.search(r"cat > /usr/local/libexec/qor-ci-seal << 'EOF'\n(.*?)\nEOF\n", self.inside, re.S).group(1)
        self.assertLess(seal.index("--make-rprivate"), seal.index("umount"))
        mounts = re.search(r'MOUNTS="([^"]+)"', seal).group(1).split()
        for mount in ("/proc/sys/fs/binfmt_misc", "/mnt/wsl", "/mnt/wslg", "/tmp/.X11-unix"):
            self.assertIn(mount, mounts)

    def test_the_seal_never_changes_a_device_node(self):
        # /dev is one filesystem across distributions: a chmod here would take the other's GPU away.
        seal = re.search(r"cat > /usr/local/libexec/qor-ci-seal << 'EOF'\n(.*?)\nEOF\n", self.inside, re.S).group(1)
        self.assertIn("mount --bind /dev/null /dev/dxg", seal)
        code = "\n".join(line for line in seal.splitlines() if not line.lstrip().startswith("#"))
        self.assertNotRegex(code, r"(chmod|chown|rm|mknod)\s+[^\n]*/dev/")

    def test_runner_is_not_the_other_distributions_user(self):
        # The distributions share a user namespace: uid 1000 here is the owner's kernel identity there.
        uid = int(re.search(r"readonly RUNNER_UID=(\d+)", self.inside).group(1))
        self.assertNotEqual(uid, 1000)
        self.assertGreater(uid, 1000)

    def test_the_downloads_are_pinned_and_checked(self):
        for name in ("RUSTC_SHA256", "CARGO_SHA256", "RUST_STD_SHA256", "ACTIONS_RUNNER_SHA256"):
            self.assertRegex(self.inside, rf"readonly {name}=[0-9a-f]{{64}}\n")
        self.assertEqual(self.inside.count("sha256sum --check"), 2)
        self.assertNotRegex(self.inside, r"curl[^\n]*\|\s*(sudo\s+)?(ba)?sh")
        self.assertNotIn("rustup", self.inside)

    def test_no_bypass_of_the_qt_licence_check(self):
        # The kit's moc says how to turn its licence check off. That is the owner's decision, and nothing here takes it.
        self.assertNotIn("QTFRAMEWORK_BYPASS_LICENSE_CHECK", self.inside)
        self.assertNotIn("QTFRAMEWORK_BYPASS_LICENSE_CHECK", OUTSIDE.read_text(encoding="utf-8"))


class TheScriptOutside(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        text = OUTSIDE.read_text(encoding="utf-8")
        without_help = re.sub(r"<#.*?#>", "", text, flags=re.S)
        cls.code = "\n".join(line for line in without_help.splitlines() if not line.lstrip().startswith("#"))

    def test_it_never_stops_wsl_or_copies_a_distribution(self):
        self.assertNotIn("--shutdown", self.code)
        self.assertNotIn("--export", self.code)

    def test_it_can_only_stop_or_remove_qor_ci(self):
        self.assertIn("$distro = 'qor-ci'", self.code)
        self.assertNotRegex(self.code, r"param\([^)]*\$[Dd]istro\b")
        for command in ("--terminate", "--unregister"):
            for line in (line for line in self.code.splitlines() if command in line and "wsl.exe" in line):
                self.assertIn(f"{command} $distro", line)

    def test_it_takes_only_the_kit_from_the_other_home(self):
        lines = [line for line in self.code.splitlines() if "tar --create" in line]
        self.assertEqual(len(lines), 1)
        self.assertRegex(lines[0], r"--file=- 6\.12\.0/gcc_64 \|")
        for word in ("qtaccount", ".local", ".ssh", ".config", "--export"):
            self.assertNotIn(word, lines[0])

    def test_it_refuses_an_account_file_that_arrives(self):
        self.assertRegex(self.code, r"-iname 'qtaccount\*'")
        self.assertRegex(self.code, r"if \(\$found\) \{ throw")

    def test_what_it_finds_cannot_be_swallowed(self):
        # In a function, a program's output would become the return value, and `exit (Invoke-Check)` exited 0 on a failure.
        check = re.search(r"function Invoke-Check \{\n(.*?)\n\}", self.code.replace("\r\n", "\n"), re.S).group(1)
        self.assertIn("| Out-Host", check)

    def test_registering_is_a_stage_of_its_own(self):
        self.assertEqual(len(re.findall(r"^\s*Invoke-Register\s*$", self.code, re.M)), 1)
        self.assertRegex(self.code.replace("\r\n", "\n"), r"'Register' \{\n\s*Assert-Distro\n\s*Install-Inside\n\s*Invoke-Register\n")

    def test_a_token_goes_from_gh_to_stdin_and_nowhere_else(self):
        code = self.code.replace("\r\n", "\n")
        self.assertRegex(code, r"registration-token\" --jq \.token 2>\$null \|\n\s*wsl\.exe")
        self.assertNotRegex(code, r"\$\w*[Tt]oken\w*\s*=")


if __name__ == "__main__":
    unittest.main()

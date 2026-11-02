# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Tests for tools/ci/isolated-vm.ps1 and isolated-vm-guest.py.

There is no VM here to run them against, and nothing here makes one. These hold what can be held without one: that the script
parses and names only real parameters, its seed, its image reader and digest check, the order and limits of what it does (against
stand-ins for Hyper-V, with module autoloading off so that the real commands cannot be reached), its verdict, and the guest script's
judgement of what comes back (against stand-ins for the far side). None of it says a boundary holds. That needs -Action Prove in
a real VM.
"""

from __future__ import annotations

import contextlib
import errno
import hashlib
import importlib.util
import io
import json
import os
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib
from pathlib import Path
from unittest import mock

try:
    import yaml
except ImportError:
    yaml = None

REPO = Path(__file__).resolve().parents[2]
TOOL = REPO / "tools" / "ci" / "isolated-vm.ps1"
GUEST = REPO / "tools" / "ci" / "isolated-vm-guest.py"
STAND_INS = Path(__file__).resolve().parent / "isolated_vm_stand_ins.ps1"
PWSH = shutil.which("pwsh")
HIDDEN = {"creationflags": subprocess.CREATE_NO_WINDOW} if os.name == "nt" else {}

spec = importlib.util.spec_from_file_location("isolated_vm_guest", GUEST)
guest = importlib.util.module_from_spec(spec)
sys.modules["isolated_vm_guest"] = guest
spec.loader.exec_module(guest)


def run_pwsh(body: str, environment: dict | None = None, timeout: int = 300) -> subprocess.CompletedProcess:
    """Runs body with the script's definitions and the stand-ins loaded, and with no way to reach a real Hyper-V command."""
    with tempfile.TemporaryDirectory() as folder:
        driver = Path(folder) / "driver.ps1"
        driver.write_text(
            "$global:PSModuleAutoLoadingPreference = 'None'\n"
            "Import-Module Microsoft.PowerShell.Management, Microsoft.PowerShell.Utility\n"
            "$ErrorActionPreference = 'Stop'\n"
            f". '{TOOL}' -Root $env:QOR_TEST_ROOT\n"
            f". '{STAND_INS}'\n"
            f"{body}\n",
            encoding="utf-8",
        )
        env = dict(os.environ)
        env.update(environment or {})
        return subprocess.run(
            [PWSH, "-NoProfile", "-NonInteractive", "-File", str(driver)],
            capture_output=True,
            text=True,
            encoding="utf-8",
            timeout=timeout,
            env=env,
            check=False,
            **HIDDEN,
        )


# ---------------------------------------------------------------------------------------------------------------------
# Disks to read: QCOW2 written from the format's own description (QEMU's docs/interop/qcow2.rst), and a GPT with real checksums.


def gpt_disk(size: int) -> bytes:
    disk = bytearray(size)
    disk[510:512] = b"\x55\xaa"
    entries = bytearray(128 * 128)
    entries[0:16] = bytes(range(16))
    entries_crc = zlib.crc32(bytes(entries))
    last = size // 512 - 1

    def header(here: int, other: int, entries_lba: int) -> bytes:
        h = bytearray(512)
        h[0:8] = b"EFI PART"
        h[8:12] = struct.pack("<I", 0x00010000)
        h[12:16] = struct.pack("<I", 92)
        h[24:32] = struct.pack("<Q", here)
        h[32:40] = struct.pack("<Q", other)
        h[72:80] = struct.pack("<Q", entries_lba)
        h[80:84] = struct.pack("<I", 128)
        h[84:88] = struct.pack("<I", 128)
        h[88:92] = struct.pack("<I", entries_crc)
        h[16:20] = struct.pack("<I", zlib.crc32(bytes(h[:92])))
        return bytes(h)

    disk[512:1024] = header(1, last, 2)
    disk[1024:1024 + len(entries)] = entries
    backup_entries = last - 32
    disk[backup_entries * 512 : backup_entries * 512 + len(entries)] = entries
    disk[last * 512 : last * 512 + 512] = header(last, 1, backup_entries)
    for offset in range(64 * 1024, size - 64 * 1024, 64 * 1024):
        disk[offset : offset + 16] = struct.pack(">QQ", offset, offset ^ 0x5152)
    return bytes(disk)


def qcow2(
    raw: bytes,
    version: int = 3,
    cluster_bits: int = 16,
    compress: bool = True,
    zero_flag_on: tuple = (),
    plain_on: tuple = (),
    size: int | None = None,
) -> bytes:
    """raw as a QCOW2 file. Empty clusters are left unallocated, as qemu-img leaves them."""
    cluster = 1 << cluster_bits
    size = len(raw) if size is None else size
    count = (len(raw) + cluster - 1) // cluster
    l2_entries = cluster // 8
    l1_needed = max(1, (count + l2_entries - 1) // l2_entries)
    l1_at = cluster
    l2_at = l1_at + (((l1_needed * 8 + cluster - 1) // cluster) or 1) * cluster
    data_at = l2_at + l1_needed * cluster
    tables = [bytearray(cluster) for _ in range(l1_needed)]
    data = bytearray()
    x = 62 - (cluster_bits - 8)
    for index in range(count):
        piece = raw[index * cluster : (index + 1) * cluster].ljust(cluster, b"\0")
        table, slot = divmod(index, l2_entries)
        if index in zero_flag_on:
            # A preallocated cluster that reads as zeros: its offset is real, holds bytes, and must not be read.
            while (data_at + len(data)) % cluster:
                data.append(0)
            entry = (1 << 63) | (data_at + len(data)) | 1
            data.extend(piece)
        elif not any(piece):
            continue
        elif compress and index not in plain_on:
            packer = zlib.compressobj(wbits=-12)
            packed = packer.compress(piece) + packer.flush()
            offset = data_at + len(data)
            extra = (offset + len(packed) - 1) // 512 - offset // 512
            entry = (1 << 62) | offset | (extra << x)
            data.extend(packed)
        else:
            while (data_at + len(data)) % cluster:
                data.append(0)
            entry = (1 << 63) | (data_at + len(data))
            data.extend(piece)
        tables[table][slot * 8 : slot * 8 + 8] = struct.pack(">Q", entry)
    header = bytearray(cluster)
    header[0:4] = b"QFI\xfb"
    header[4:8] = struct.pack(">I", version)
    header[20:24] = struct.pack(">I", cluster_bits)
    header[24:32] = struct.pack(">Q", size)
    header[36:40] = struct.pack(">I", l1_needed)
    header[40:48] = struct.pack(">Q", l1_at)
    if version == 3:
        header[96:100] = struct.pack(">I", 4)
        header[100:104] = struct.pack(">I", 104)
    l1 = bytearray(l2_at - l1_at)
    for index in range(l1_needed):
        l1[index * 8 : index * 8 + 8] = struct.pack(">Q", (1 << 63) | (l2_at + index * cluster))
    return bytes(header) + bytes(l1) + b"".join(bytes(t) for t in tables) + bytes(data)


def vhd_geometry(size: int) -> tuple[int, int, int]:
    total = min(size // 512, 65535 * 16 * 255)
    if total >= 65535 * 16 * 63:
        sectors, heads = 255, 16
        product = total // sectors
    else:
        sectors = 17
        product = total // sectors
        heads = max(4, (product + 1023) // 1024)
        if product >= heads * 1024 or heads > 16:
            sectors, heads = 31, 16
            product = total // sectors
        if product >= heads * 1024:
            sectors, heads = 63, 16
            product = total // sectors
    return product // heads, heads, sectors


def read_iso(data: bytes) -> dict:
    """Walks both trees of an ISO by the standard's own offsets, trusting nothing but them. Returns what each holds."""

    def walk(descriptor: int, decode) -> dict:
        d = data[descriptor * 2048 : (descriptor + 1) * 2048]
        root = d[156:190]
        extent = struct.unpack("<I", root[2:6])[0]
        length = struct.unpack("<I", root[10:14])[0]
        table_at = struct.unpack("<I", d[140:144])[0]
        assert struct.unpack("<I", data[table_at * 2048 + 2 : table_at * 2048 + 6])[0] == extent, "the L path table points elsewhere"
        big_at = struct.unpack(">I", d[148:152])[0]
        assert struct.unpack(">I", data[big_at * 2048 + 2 : big_at * 2048 + 6])[0] == extent, "the M path table points elsewhere"
        folder = data[extent * 2048 : extent * 2048 + length]
        found = {}
        at = 0
        while at < len(folder) and folder[at]:
            size = folder[at]
            record = folder[at : at + size]
            assert struct.unpack(">I", record[6:10])[0] == struct.unpack("<I", record[2:6])[0], "a both-endian extent disagrees"
            assert struct.unpack(">I", record[14:18])[0] == struct.unpack("<I", record[10:14])[0], "a both-endian length disagrees"
            name = record[33 : 33 + record[32]]
            if name not in (b"\0", b"\1"):
                lba = struct.unpack("<I", record[2:6])[0]
                bytes_long = struct.unpack("<I", record[10:14])[0]
                found[decode(name)] = data[lba * 2048 : lba * 2048 + bytes_long]
            at += size
        return found

    assert data[32769:32774] == b"CD001", "no volume descriptor at sector 16"
    assert data[16 * 2048] == 1 and data[17 * 2048] == 2 and data[18 * 2048] == 255
    return {
        "label": data[16 * 2048 + 40 : 16 * 2048 + 72].decode("ascii").strip(),
        "joliet_label": data[17 * 2048 + 40 : 17 * 2048 + 72].decode("utf-16-be").strip(),
        "escape": data[17 * 2048 + 88 : 17 * 2048 + 91],
        "plain": walk(16, lambda n: n.decode("ascii")),
        "joliet": walk(17, lambda n: n.decode("utf-16-be")),
    }


# ---------------------------------------------------------------------------------------------------------------------


# Windows alone: the script is for Windows, and these have only ever been seen to run there. CI's Linux runner has pwsh, and runs
# only the two classes below that read the file's syntax, and the guest script's tests.
@unittest.skipUnless(PWSH and os.name == "nt", "needs PowerShell 7 on Windows")
class WithPwsh(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        self.folder = Path(folder.name)
        self.env = {
            "LOCALAPPDATA": str(self.folder / "local"),
            "QOR_TEST_ROOT": str(self.folder / "root"),
        }


@unittest.skipUnless(PWSH, "needs PowerShell 7")
class TheScriptParses(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        self.folder = Path(folder.name)

    def test_it_parses(self):
        done = subprocess.run(
            [
                PWSH,
                "-NoProfile",
                "-NonInteractive",
                "-Command",
                "$e = $null; $t = $null; $null = [System.Management.Automation.Language.Parser]::ParseFile("
                f"'{TOOL}', [ref] $t, [ref] $e); $e | ForEach-Object {{ $_.Message }}; exit $e.Count",
            ],
            capture_output=True,
            text=True,
            timeout=120,
            check=False,
            **HIDDEN,
        )
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)

    def test_a_fault_in_the_text_would_be_seen(self):
        broken = self.folder / "broken.ps1"
        broken.write_text(TOOL.read_text(encoding="utf-8").replace("function Step(", "function Step((", 1), encoding="utf-8")
        done = subprocess.run(
            [
                PWSH,
                "-NoProfile",
                "-NonInteractive",
                "-Command",
                f"$e = $null; $t = $null; $null = [System.Management.Automation.Language.Parser]::ParseFile('{broken}', [ref] $t, [ref] $e); exit $e.Count",
            ],
            capture_output=True,
            text=True,
            timeout=120,
            check=False,
            **HIDDEN,
        )
        self.assertNotEqual(done.returncode, 0)


# What the script may never call, by the AST and so whatever a comment says.
FORBIDDEN_COMMANDS = {
    "start-process",
    "add-localgroupmember",
    "add-adgroupmember",
    "new-localuser",
    "register-scheduledtask",
    "new-scheduledtask",
    "new-scheduledtaskaction",
    "schtasks",
    "schtasks.exe",
    "new-service",
    "sc.exe",
    "set-service",
    "set-vmhost",
    "set-vmswitch",
    "set-netfirewallprofile",
    "set-netfirewallrule",
    "set-netfirewallhypervvmsetting",
    "new-netfirewallhypervrule",
    "enable-vmintegrationservice",
    "enable-windowsoptionalfeature",
    "gh",
    "gh.exe",
    "git-credential",
    "curl",
    "curl.exe",
}


@unittest.skipUnless(PWSH, "needs PowerShell 7")
class WhatTheScriptCalls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        done = subprocess.run(
            [
                PWSH,
                "-NoProfile",
                "-NonInteractive",
                "-Command",
                "$e = $null; $t = $null; $a = [System.Management.Automation.Language.Parser]::ParseFile("
                f"'{TOOL}', [ref] $t, [ref] $e);"
                "$a.FindAll({ param($n) $n -is [System.Management.Automation.Language.CommandAst] }, $true) | "
                "ForEach-Object { $_.GetCommandName() } | Where-Object { $_ } | Sort-Object -Unique",
            ],
            capture_output=True,
            text=True,
            timeout=120,
            check=True,
            **HIDDEN,
        )
        cls.commands = {line.strip().lower() for line in done.stdout.splitlines() if line.strip()}

    def test_the_list_is_real(self):
        self.assertIn("new-vm", self.commands)
        self.assertIn("new-netfirewallrule", self.commands)

    def test_it_never_elevates_or_grants_or_schedules_or_touches_a_host_setting(self):
        self.assertEqual(self.commands & FORBIDDEN_COMMANDS, set())

    def test_nothing_about_qt_or_a_runner_or_a_token_is_in_it(self):
        text = TOOL.read_text(encoding="utf-8").lower() + GUEST.read_text(encoding="utf-8").lower()
        for word in ("qtaccount", "qt_installer", "install_qt", "registration-token", "actions-runner", "qor_linux_runner", "gh api", "gh variable", "config.sh"):
            with self.subTest(word):
                self.assertNotIn(word, text)

    def test_the_only_remove_commands_take_fixed_names(self):
        text = TOOL.read_text(encoding="utf-8")
        for line in text.splitlines():
            if "Remove-VM " in line and "-Name" in line:
                self.assertIn("$script:VmName", line)
            if "Remove-VMSwitch " in line:
                self.assertIn("$script:SwitchName", line)
            if "Remove-NetNat " in line:
                self.assertIn("$script:NatName", line)


@unittest.skipUnless(PWSH and os.name == "nt", "needs Windows' Hyper-V module, though never its service")
class EveryParameterIsReal(unittest.TestCase):
    """Every parameter and fixed value against the real command's own description, which reads nothing of Hyper-V's state."""

    def test_each_named_parameter_and_enum_value_exists(self):
        check = (
            "Import-Module Hyper-V, NetNat, NetSecurity, NetTCPIP, NetAdapter, DnsClient\n"
            "$e = $null; $t = $null\n"
            f"$a = [System.Management.Automation.Language.Parser]::ParseFile('{TOOL}', [ref] $t, [ref] $e)\n"
            "$own = @($a.FindAll({ param($n) $n -is [System.Management.Automation.Language.FunctionDefinitionAst] }, $true) | ForEach-Object Name)\n"
            "$bad = @(); $seen = 0\n"
            "foreach ($c in $a.FindAll({ param($n) $n -is [System.Management.Automation.Language.CommandAst] }, $true)) {\n"
            "  $name = $c.GetCommandName()\n"
            "  if (-not $name -or $own -contains $name) { continue }\n"
            "  $info = Get-Command $name -ErrorAction SilentlyContinue\n"
            "  if (-not $info -or $info.Source -notin 'Hyper-V','NetNat','NetSecurity','NetTCPIP','NetAdapter','DnsClient') { continue }\n"
            "  $elements = $c.CommandElements\n"
            "  for ($i = 1; $i -lt $elements.Count; $i++) {\n"
            "    $el = $elements[$i]\n"
            "    if ($el -isnot [System.Management.Automation.Language.CommandParameterAst]) { continue }\n"
            "    $seen++\n"
            "    $p = $info.Parameters[$el.ParameterName]\n"
            "    if (-not $p) { $bad += \"$name -$($el.ParameterName): no such parameter\"; continue }\n"
            "    $type = $p.ParameterType; $inner = [Nullable]::GetUnderlyingType($type); if ($inner) { $type = $inner }\n"
            "    if ($type.IsArray) { $type = $type.GetElementType() }\n"
            "    if ($type.IsEnum -and $i + 1 -lt $elements.Count -and $elements[$i + 1] -is [System.Management.Automation.Language.StringConstantExpressionAst]) {\n"
            "      $value = $elements[$i + 1].Value\n"
            "      if ($value -notin [enum]::GetNames($type)) { $bad += \"$name -$($el.ParameterName) ${value}: not one of $([enum]::GetNames($type) -join ',')\" }\n"
            "    }\n"
            "  }\n"
            "}\n"
            "\"checked $seen\"; $bad; exit $bad.Count\n"
        )
        done = subprocess.run([PWSH, "-NoProfile", "-NonInteractive", "-Command", check], capture_output=True, text=True, timeout=180, check=False, **HIDDEN)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        seen = int(done.stdout.split()[1])
        self.assertGreater(seen, 60, "it checked too little to mean anything")


class TheSeed(WithPwsh):
    KEY = "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIStandInPublicKeyStandInPublicKeyStandIn qor-ci-vm"

    def make(self, name: str = "seed.iso") -> Path:
        path = self.folder / name
        done = run_pwsh(
            f"$null = Initialize-Settings\nNew-SeedIso (Get-SeedFiles '{self.KEY}') '{path}'\n",
            self.env,
        )
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        return path

    def test_it_is_an_iso_labelled_as_nocloud_looks_for_with_all_three_files_in_both_trees(self):
        iso = read_iso(self.make().read_bytes())
        self.assertEqual(iso["label"], "cidata")
        self.assertEqual(iso["joliet_label"], "cidata")
        self.assertEqual(iso["escape"], b"%/E")
        self.assertEqual(sorted(iso["joliet"]), ["meta-data;1", "network-config;1", "user-data;1"])
        self.assertEqual(sorted(iso["plain"]), ["META_DAT.;1", "NETWORK_.;1", "USER_DAT.;1"])
        self.assertEqual(iso["joliet"]["user-data;1"], iso["plain"]["USER_DAT.;1"])

    def test_the_same_settings_make_the_same_bytes(self):
        self.assertEqual(self.make("a.iso").read_bytes(), self.make("b.iso").read_bytes())

    def test_what_it_says(self):
        iso = read_iso(self.make().read_bytes())["joliet"]
        user = iso["user-data;1"].decode("utf-8")
        self.assertTrue(user.startswith("#cloud-config\n"), "cloud-init ignores user-data without this first line")
        self.assertNotIn("\r", user, "a carriage return from this file's line ends would reach the guest")
        self.assertIn(self.KEY, user)
        self.assertIn("lock_passwd: true", user)
        self.assertIn("ssh_pwauth: false", user)
        self.assertIn("AllowUsers qor", user)
        network = iso["network-config;1"].decode("utf-8")
        self.assertIn("10.213.57.2/29", network)
        self.assertIn("via: 10.213.57.1", network)
        self.assertIn("instance-id:", iso["meta-data;1"].decode("utf-8"))

    def test_it_holds_no_secret_and_nothing_of_qt_or_github(self):
        data = self.make().read_bytes()
        lowered = data.replace(b"\0", b"").lower()
        for word in (b"private key", b"password:", b"token", b"qtaccount", b"github", b"ghp_", b"qt6"):
            with self.subTest(word):
                self.assertNotIn(word, lowered)

    @unittest.skipUnless(yaml, "needs PyYAML")
    def test_each_file_is_the_yaml_cloud_init_expects(self):
        iso = read_iso(self.make().read_bytes())["joliet"]
        user = yaml.safe_load(iso["user-data;1"])
        self.assertEqual(user["users"][0]["name"], "qor")
        self.assertEqual(user["users"][0]["ssh_authorized_keys"], [self.KEY])
        self.assertTrue(user["users"][0]["lock_passwd"])
        self.assertEqual(user["users"][1]["name"], "runner")
        self.assertNotIn("sudo", user["users"][1], "runner is what a job would be, with no privilege")
        self.assertNotIn("ssh_authorized_keys", user["users"][1])
        self.assertFalse(user["package_update"])
        network = yaml.safe_load(iso["network-config;1"])
        self.assertEqual(network["version"], 2)
        nic = network["ethernets"]["nic0"]
        self.assertEqual(nic["addresses"], ["10.213.57.2/29"])
        self.assertFalse(nic["dhcp4"])
        self.assertFalse(nic["accept-ra"])
        self.assertEqual(nic["nameservers"]["addresses"], ["1.1.1.1", "8.8.8.8"])
        self.assertEqual(yaml.safe_load(iso["meta-data;1"])["local-hostname"], "qor-ci-vm")

    @unittest.skipUnless(os.name == "nt", "needs Windows' tar, which is libarchive and reads ISO 9660")
    def test_another_reader_finds_the_same_files(self):
        path = self.make()
        out = self.folder / "out"
        out.mkdir()
        tar = Path(os.environ["SystemRoot"]) / "System32" / "tar.exe"
        done = subprocess.run([str(tar), "-xf", str(path), "-C", str(out)], capture_output=True, text=True, timeout=60, check=False, **HIDDEN)
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertEqual(sorted(p.name for p in out.iterdir()), ["meta-data", "network-config", "user-data"])
        self.assertTrue((out / "user-data").read_bytes().startswith(b"#cloud-config\n"))

    def test_a_private_key_is_refused_on_the_way_in(self):
        shape = "-----BEGIN OPENSSH " + "PRIVATE KEY-----"
        done = run_pwsh(f"$null = Initialize-Settings\ntry {{ $null = Get-SeedFiles '{shape}'; 'WROTE IT' }} catch {{ \"refused: $($_.Exception.Message)\" }}\n", self.env)
        self.assertIn("refused:", done.stdout)
        self.assertNotIn("WROTE IT", done.stdout)

    def test_a_public_key_file_holding_anything_else_is_refused(self):
        shape = "-----BEGIN OPENSSH " + "PRIVATE KEY-----"
        done = run_pwsh(
            "$null = Initialize-Settings\n"
            "$null = New-Item -ItemType Directory -Force -Path $script:KeyDir\n"
            f"Set-Content -LiteralPath \"$script:KeyFile.pub\" -Value '{shape}'\n"
            "try { $null = Get-GuestPublicKey; 'TOOK IT' } catch { \"refused: $($_.Exception.Message)\" }\n",
            self.env,
        )
        self.assertIn("refused:", done.stdout)
        self.assertNotIn("TOOK IT", done.stdout)
        self.assertNotIn(shape, done.stdout, "what it refused is not printed")


class TheImageReader(WithPwsh):
    def convert(self, data: bytes, most: str = "") -> tuple[subprocess.CompletedProcess, Path]:
        source, target = self.folder / "image.img", self.folder / "out.vhd"
        source.write_bytes(data)
        done = run_pwsh(
            f"try {{ $size = Convert-Qcow2ToVhd '{source}' '{target}' {most}; \"SIZE=$size\" }} catch {{ \"REFUSED: $($_.Exception.Message)\"; exit 1 }}\n",
            self.env,
        )
        return done, target

    def assert_converts(self, raw: bytes, data: bytes):
        done, target = self.convert(data)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertIn(f"SIZE={len(raw)}", done.stdout)
        written = target.read_bytes()
        self.assertEqual(written[: len(raw)], raw, "the disk came out different")
        return written

    def test_compressed_plain_and_unallocated_clusters_all_read_back(self):
        raw = bytearray(gpt_disk(4 * 1024 * 1024))
        raw[1024 * 1024 : 1024 * 1024 + 70000] = os.urandom(70000)  # does not compress: a cluster may span sectors
        for version in (2, 3):
            with self.subTest(version=version):
                self.assert_converts(bytes(raw), qcow2(bytes(raw), version=version, plain_on=(8, 9)))

    def test_a_cluster_marked_as_zeros_reads_as_zeros_whatever_its_offset_holds(self):
        # Version 3 alone: the format says this bit is always 0 in version 2.
        cluster = 64 * 1024
        raw = bytearray(gpt_disk(4 * 1024 * 1024))
        zeroed = bytearray(raw)
        zeroed[40 * cluster : 41 * cluster] = bytes(cluster)
        source = bytearray(raw)
        source[40 * cluster : 41 * cluster] = b"\xaa" * cluster
        self.assert_converts(bytes(zeroed), qcow2(bytes(source), version=3, zero_flag_on=(40,)))

    def test_every_cluster_size_the_real_tools_use(self):
        raw = gpt_disk(1024 * 1024)
        for bits in (12, 16, 20):
            with self.subTest(bits=bits):
                self.assert_converts(raw, qcow2(raw, cluster_bits=bits))

    def test_a_disk_past_one_l2_table(self):
        # With 4 KB clusters one L2 table covers 2 MB, so this needs several.
        raw = gpt_disk(9 * 1024 * 1024)
        self.assert_converts(raw, qcow2(raw, cluster_bits=12))

    def test_the_footer_is_one_hyper_v_can_read(self):
        raw = gpt_disk(3 * 1024 * 1024 + 512 * 8)
        written = self.assert_converts(raw, qcow2(raw))
        whole = (len(raw) + 1024 * 1024 - 1) // (1024 * 1024) * 1024 * 1024
        self.assertEqual(len(written), whole + 512, "whole megabytes, then the footer")
        footer = written[-512:]
        self.assertEqual(footer[0:8], b"conectix")
        self.assertEqual(struct.unpack(">Q", footer[16:24])[0], 0xFFFFFFFFFFFFFFFF)
        self.assertEqual(struct.unpack(">Q", footer[48:56])[0], whole)
        self.assertEqual(struct.unpack(">I", footer[60:64])[0], 2, "fixed")
        blanked = footer[:64] + b"\0\0\0\0" + footer[68:]
        self.assertEqual(struct.unpack(">I", footer[64:68])[0], ~sum(blanked) & 0xFFFFFFFF)
        cylinders, heads, sectors = struct.unpack(">HBB", footer[56:60])
        self.assertEqual((cylinders, heads, sectors), vhd_geometry(whole))

    def test_the_geometry_on_values_worked_by_hand_from_the_specification(self):
        self.assertEqual(vhd_geometry(1024 * 1024), (30, 4, 17))
        done = run_pwsh(
            "$f = New-VhdFooter 1048576 ([guid]::Empty); \"$($f[56]),$($f[57]),$($f[58]),$($f[59])\"\n"
            "$f = New-VhdFooter (64GB) ([guid]::Empty); \"$($f[56]),$($f[57]),$($f[58]),$($f[59])\"\n",
            self.env,
        )
        lines = done.stdout.split()
        self.assertEqual(lines[0], "0,30,4,17", done.stdout + done.stderr)
        cylinders, heads, sectors = vhd_geometry(64 * 1024**3)
        self.assertEqual(lines[1], f"{cylinders >> 8},{cylinders & 255},{heads},{sectors}")

    def assert_refused(self, data: bytes, because: str, most: str = ""):
        done, target = self.convert(data, most)
        self.assertNotEqual(done.returncode, 0, f"it took this: {done.stdout}")
        self.assertIn(because, done.stdout + done.stderr)
        self.assertFalse(target.exists(), "a refused image left a disk behind")

    def test_what_it_does_not_read_it_refuses(self):
        raw = gpt_disk(1024 * 1024)
        good = bytearray(qcow2(raw))

        def changed(offset: int, value: bytes) -> bytes:
            data = bytearray(good)
            data[offset : offset + len(value)] = value
            return bytes(data)

        for data, because in (
            (b"not an image at all, and long enough to have a header, more or less, of some sort, yes", "magic"),
            (changed(0, b"XFI\xfb"), "magic"),
            (changed(4, struct.pack(">I", 4)), "version 4"),
            (changed(8, struct.pack(">Q", 4096)), "backing file"),
            (changed(32, struct.pack(">I", 1)), "encrypted"),
            (changed(72, struct.pack(">Q", 1)), "incompatible_features"),  # dirty
            (changed(72, struct.pack(">Q", 2)), "incompatible_features"),  # corrupt
            (changed(72, struct.pack(">Q", 8)), "incompatible_features"),  # zstd
            (changed(72, struct.pack(">Q", 16)), "incompatible_features"),  # extended L2
            (changed(20, struct.pack(">I", 8)), "cluster_bits"),
            (changed(20, struct.pack(">I", 22)), "cluster_bits"),
            (changed(24, struct.pack(">Q", 513)), "sectors"),
            (changed(24, struct.pack(">Q", 0)), "sectors"),
            (changed(36, struct.pack(">I", 0)), "L1 table"),
            (b"", "ends"),
        ):
            with self.subTest(because=because, first=data[:8]):
                self.assert_refused(data, because)

    def test_it_will_not_be_told_it_holds_a_huge_disk(self):
        good = bytearray(qcow2(gpt_disk(1024 * 1024)))
        good[24:32] = struct.pack(">Q", 1 << 50)
        self.assert_refused(bytes(good), "more than")

    def test_a_file_cut_short_is_refused(self):
        raw = bytearray(gpt_disk(1024 * 1024))
        raw[200000:400000] = os.urandom(200000)
        data = qcow2(bytes(raw))
        self.assert_refused(data[: len(data) - 150000], "")

    @staticmethod
    def first_compressed_cluster(data: bytes) -> int:
        """Where the first compressed cluster's data starts, by the tables in a file of qcow2() above."""
        bits = struct.unpack(">I", data[20:24])[0]
        l1_at = struct.unpack(">Q", data[40:48])[0]
        l2_at = struct.unpack(">Q", data[l1_at : l1_at + 8])[0] & 0x00FFFFFFFFFFFE00
        x = 62 - (bits - 8)
        for slot in range((1 << bits) // 8):
            entry = struct.unpack(">Q", data[l2_at + slot * 8 : l2_at + slot * 8 + 8])[0]
            if entry & (1 << 62):
                return entry & ((1 << x) - 1)
        raise AssertionError("no compressed cluster in that file")

    def test_compressed_data_that_is_not_deflate_is_refused(self):
        data = bytearray(qcow2(gpt_disk(1024 * 1024)))
        # Its first byte says a block type that does not exist. Damage anywhere else in the stream may not show: neither raw deflate
        # nor QCOW2 has a checksum, and incompressible data is stored as it is. That is what the digest before this reader is for.
        data[self.first_compressed_cluster(bytes(data))] = 0xFF
        self.assert_refused(bytes(data), "converted")

    def test_compressed_data_that_ends_short_of_a_cluster_is_refused(self):
        data = bytearray(qcow2(gpt_disk(1024 * 1024)))
        packer = zlib.compressobj(wbits=-12)
        short = packer.compress(b"short") + packer.flush()
        at = self.first_compressed_cluster(bytes(data))
        data[at : at + len(short)] = short
        self.assert_refused(bytes(data), "gave 5 bytes")


class TheDiskCheck(WithPwsh):
    def check(self, data: bytes) -> subprocess.CompletedProcess:
        path = self.folder / "disk.vhd"
        path.write_bytes(data)
        return run_pwsh(f"try {{ Assert-GptDisk '{path}' {len(data)}; 'GOOD' }} catch {{ \"REFUSED: $($_.Exception.Message)\"; exit 1 }}\n", self.env)

    def test_a_real_gpt_passes(self):
        done = self.check(gpt_disk(1024 * 1024))
        self.assertIn("GOOD", done.stdout, done.stdout + done.stderr)

    def test_each_part_read_wrongly_is_caught(self):
        good = gpt_disk(1024 * 1024)
        for name, offset, because in (
            ("the boot signature", 510, "boot sector"),
            ("the primary header's name", 512, "primary GPT header"),
            ("the primary header's fields", 512 + 40, "primary GPT header's checksum"),
            ("the primary partition list", 1024 + 5, "primary GPT partition list"),
            ("the backup header", len(good) - 512 + 40, "backup GPT header's checksum"),
            ("the backup partition list", len(good) - 512 - 32 * 512 + 5, "backup GPT partition list"),
        ):
            with self.subTest(name):
                data = bytearray(good)
                data[offset] ^= 0xFF
                done = self.check(bytes(data))
                self.assertNotEqual(done.returncode, 0, done.stdout)
                self.assertIn(because, done.stdout + done.stderr)


class TheDigest(WithPwsh):
    def test_a_wrong_file_is_refused_and_nothing_comes_of_it(self):
        wrong = self.folder / "wrong.img"
        wrong.write_bytes(b"this is not Ubuntu")
        env = dict(self.env, QOR_TEST_IMAGE=str(wrong))
        done = run_pwsh(
            "$null = Initialize-Settings\n"
            "$null = New-Item -ItemType Directory -Force -Path $script:Settings.Root\n"
            "try { Get-PinnedImage; 'TOOK IT' } catch { \"REFUSED: $($_.Exception.Message)\" }\n"
            "\"image there: $(Test-Path -LiteralPath $script:ImageFile)\"\n"
            "\"part there: $(Test-Path -LiteralPath \"$script:ImageFile.part\")\"\n",
            env,
        )
        self.assertIn("REFUSED:", done.stdout, done.stdout + done.stderr)
        self.assertIn(hashlib.sha256(b"this is not Ubuntu").hexdigest(), done.stdout, "it says what it got")
        self.assertIn("image there: False", done.stdout)
        self.assertIn("part there: False", done.stdout)
        self.assertNotIn("TOOK IT", done.stdout)

    def test_the_right_file_is_taken(self):
        right = self.folder / "right.img"
        right.write_bytes(b"this stands in for Ubuntu")
        env = dict(self.env, QOR_TEST_IMAGE=str(right))
        digest = hashlib.sha256(b"this stands in for Ubuntu").hexdigest()
        done = run_pwsh(
            f"$script:ImageSha256 = '{digest}'\n$null = Initialize-Settings\n"
            "$null = New-Item -ItemType Directory -Force -Path $script:Settings.Root\n"
            "Get-PinnedImage\n\"image there: $(Test-Path -LiteralPath $script:ImageFile)\"\n",
            env,
        )
        self.assertIn("image there: True", done.stdout, done.stdout + done.stderr)

    def test_an_image_already_here_is_checked_again_and_refused_if_it_changed(self):
        right = self.folder / "right.img"
        right.write_bytes(b"this stands in for Ubuntu")
        env = dict(self.env, QOR_TEST_IMAGE=str(right))
        digest = hashlib.sha256(b"this stands in for Ubuntu").hexdigest()
        done = run_pwsh(
            f"$script:ImageSha256 = '{digest}'\n$null = Initialize-Settings\n"
            "$null = New-Item -ItemType Directory -Force -Path $script:Settings.Root\n"
            "Get-PinnedImage\n"
            "Set-Content -LiteralPath $script:ImageFile -Value 'changed since'\n"
            "try { Get-PinnedImage; 'TOOK IT' } catch { \"REFUSED: $($_.Exception.Message)\" }\n",
            env,
        )
        self.assertIn("REFUSED:", done.stdout, done.stdout + done.stderr)
        self.assertNotIn("TOOK IT", done.stdout)

    def test_the_pin_is_the_one_ubuntu_published(self):
        text = TOOL.read_text(encoding="utf-8")
        self.assertIn("https://cloud-images.ubuntu.com/releases/noble/release-20261017/ubuntu-24.04-server-cloudimg-amd64.img", text)
        self.assertIn("04e8f2461a16a44e78a370284471f29616cdd9baf1893df25807b71f985b6c94", text)


class TheNetworkPlan(WithPwsh):
    def test_addresses_and_refusals(self):
        done = run_pwsh(
            "$p = Get-NetworkPlan '10.213.57.0'; \"$($p.Cidr) $($p.HostIp) $($p.VmIp)\"\n"
            "foreach ($bad in '10.213.57.1', '8.8.8.0', '169.254.0.0', 'nonsense', '::1') {\n"
            "  try { $null = Get-NetworkPlan $bad; \"TOOK $bad\" } catch { \"refused $bad\" }\n"
            "}\n"
            "\"$(Test-InRange '10.213.57.3' '10.213.57.0/29') $(Test-InRange '10.213.57.8' '10.213.57.0/29') $(Test-InRange '1.2.3.4' '0.0.0.0/0')\"\n"
            "\"$(Test-PrivateAddress '192.168.1.1') $(Test-PrivateAddress '100.100.1.1') $(Test-PrivateAddress '1.1.1.1')\"\n",
            self.env,
        )
        self.assertIn("10.213.57.0/29 10.213.57.1 10.213.57.2", done.stdout, done.stdout + done.stderr)
        self.assertNotIn("TOOK", done.stdout)
        self.assertEqual(done.stdout.count("refused"), 5)
        self.assertIn("True False True", done.stdout)
        self.assertIn("True True False", done.stdout)


# What Create should have set, each as Hyper-V would read it back. A planted fault for each, which Check must name.
PLANTS = [
    ("secure-boot", "$vm.Firmware.SecureBoot = 'Off'"),
    ("secure-boot-template", "$vm.Firmware.SecureBootTemplate = 'MicrosoftWindows'"),
    ("boot-no-network", "$vm.Firmware.BootOrder = @([pscustomobject] @{ BootType = 'Network' })"),
    ("vm-generation", "$vm.Generation = 1"),
    ("vm-mark", "$vm.Notes = 'someone else'"),
    ("checkpoint-type", "$vm.CheckpointType = 'Standard'"),
    ("automatic-checkpoints", "$vm.AutomaticCheckpointsEnabled = $true"),
    ("checkpoints-held", "$vm.Snapshots = @([pscustomobject] @{ Name = 'x' })"),
    ("start-action", "$vm.AutomaticStartAction = 'Start'"),
    ("stop-action", "$vm.AutomaticStopAction = 'Save'"),
    ("dynamic-memory", "$vm.Memory.DynamicMemoryEnabled = $true"),
    ("memory-bytes", "$vm.Memory.Startup = 12GB"),
    ("processors", "$vm.Processor.Count = 20"),
    ("nesting", "$vm.Processor.ExposeVirtualizationExtensions = $true"),
    ("enhanced-session-transport", "$vm.EnhancedSessionTransportType = 'HvSocket'"),
    ("battery-passthrough", "$vm.BatteryPassthroughEnabled = $true"),
    ("tpm", "$vm.Tpm = $true"),
    ("integration-services-on", "($vm.Services | Where-Object Name -EQ 'Key-Value Pair Exchange').Enabled = $true"),
    ("integration-services-on", "($vm.Services | Where-Object Name -EQ 'Guest Service Interface').Enabled = $true"),
    ("disks", "$vm.Disks = @($vm.Disks) + @([pscustomobject] @{ Path = 'C:\\Windows\\sneaky.vhdx' })"),
    ("dvd-drives", "$vm.Dvds.Add([pscustomobject] @{ Path = 'C:\\sneaky.iso'; ControllerNumber = 0; ControllerLocation = 2 })"),
    ("gpu-partitions", "$vm.Gpu = @([pscustomobject] @{ Name = 'gpu' })"),
    ("assigned-devices", "$vm.Assigned = @([pscustomobject] @{ Name = 'device' })"),
    ("fibre-channel", "$vm.Fibre = @([pscustomobject] @{ Name = 'hba' })"),
    ("adapter-switch", "$vm.Adapter.SwitchName = 'someone-elses-switch'"),
    ("mac-spoofing", "$vm.Adapter.MacAddressSpoofing = 'On'"),
    ("dhcp-guard", "$vm.Adapter.DhcpGuard = 'Off'"),
    ("router-guard", "$vm.Adapter.RouterGuard = 'Off'"),
    ("port-mirroring", "$vm.Adapter.PortMirroringMode = 'Source'"),
    ("teaming", "$vm.Adapter.AllowTeaming = 'On'"),
    ("device-naming", "$vm.Adapter.DeviceNaming = 'On'"),
    ("port-acls", "$vm.Acls.RemoveAt(0)"),
    ("port-acls", "$vm.Acls.Add([pscustomobject] @{ Direction = 'Outbound'; Action = 'Allow'; RemoteAddress = '192.168.1.0/24' })"),
    ("switch-type", "$global:World.Switches['qor-ci-vm'].SwitchType = 'External'"),
    ("switch-mark", "$global:World.Switches['qor-ci-vm'].Notes = 'someone else'"),
    ("host-address", "$global:World.Addresses[0].PrefixLength = 16"),
    ("adapter-bindings-on", "($global:World.Bindings['vEthernet (qor-ci-vm)'] | Where-Object ComponentID -EQ 'ms_server').Enabled = $true"),
    ("nats", "$global:World.Nats['docker'] = [pscustomobject] @{ Name = 'docker'; InternalIPInterfaceAddressPrefix = '172.30.0.0/16' }"),
    ("nat-range", "$global:World.Nats['qor-ci-vm'].InternalIPInterfaceAddressPrefix = '10.0.0.0/8'"),
    ("firewall-profiles", "$global:World.Profiles[2].Enabled = 'False'"),
    ("qor-ci-vm-block-by-adapter", "$global:World.Rules['qor-ci-vm-block-by-adapter'].Enabled = 'False'"),
    ("qor-ci-vm-block-by-adapter", "$global:World.Rules['qor-ci-vm-block-by-adapter'].Action = 'Allow'"),
    ("qor-ci-vm-block-by-adapter", "$global:World.Rules['qor-ci-vm-block-by-adapter'].InterfaceAlias = 'Wi-Fi'"),
    ("qor-ci-vm-block-by-address", "$global:World.Rules.Remove('qor-ci-vm-block-by-address')"),
    ("qor-ci-vm-block-by-address", "$global:World.Rules['qor-ci-vm-block-by-address'].RemoteAddress = '10.0.0.0/8'"),
]


class WithAWorld(WithPwsh):
    """A real, small QCOW2 with a real GPT, through the real reader and digest check, into a stand-in Hyper-V."""

    def setUp(self):
        super().setUp()
        image = qcow2(gpt_disk(2 * 1024 * 1024))
        path = self.folder / "pinned.img"
        path.write_bytes(image)
        self.env["QOR_TEST_IMAGE"] = str(path)
        self.pin = f"$script:ImageSha256 = '{hashlib.sha256(image).hexdigest()}'\n"

    def world(self, body: str) -> subprocess.CompletedProcess:
        done = run_pwsh(
            self.pin + body + "\n'CALLS:'\n$global:World.Calls\n'SSH:'\n$global:World.Ssh\n",
            self.env,
        )
        self.assertNotIn("is not recognized", done.stdout + done.stderr, "a command has no stand-in, and would have been the real one")
        return done

    def create(self) -> str:
        return "$code = Invoke-Main 'Create'; \"CODE=$code\"\n"

    @staticmethod
    def calls(done: subprocess.CompletedProcess) -> list[str]:
        text = done.stdout.split("CALLS:", 1)[1].split("SSH:", 1)[0]
        return [line for line in text.splitlines() if line.strip()]

    @staticmethod
    def ssh(done: subprocess.CompletedProcess) -> list[str]:
        return [line for line in done.stdout.split("SSH:", 1)[1].splitlines() if line.strip()]


class Create(WithAWorld):
    def test_an_ordinary_account_is_refused_before_anything_is_touched(self):
        done = self.world("$global:World.Admin = $false\n" + self.create())
        self.assertIn("CODE=1", done.stdout, done.stdout + done.stderr)
        self.assertIn("needs an administrator", done.stdout)
        self.assertIn("never elevates itself", done.stdout)
        self.assertEqual(self.calls(done), [])
        self.assertFalse((self.folder / "root").exists())

    def test_from_nothing_it_makes_what_it_should_and_says_each_step_first(self):
        done = self.world(self.create() + "\"KEY: $(Test-Path -LiteralPath $script:KeyFile)\"\n\"SEED: $(Test-Path -LiteralPath $script:SeedFile)\"\n\"DONE: $($script:Settings.FirstBootDone)\"\n\"STATE: $($global:World.VMs['qor-ci-vm'].State)\"\n\"UP: $($global:World.GuestUp)\"\n")
        self.assertIn("CODE=0", done.stdout, done.stdout + done.stderr)
        self.assertIn("== 1. Check this machine", done.stdout)
        calls = self.calls(done)
        joined = "\n".join(calls)
        for call in (
            "New-VMSwitch -Name qor-ci-vm -SwitchType Internal",
            "New-NetIPAddress -InterfaceAlias vEthernet (qor-ci-vm) -IPAddress 10.213.57.1 -PrefixLength 29",
            "New-NetNat -Name qor-ci-vm -InternalIPInterfaceAddressPrefix 10.213.57.0/29",
            "New-NetFirewallRule -Name qor-ci-vm-block-by-adapter",
            "New-NetFirewallRule -Name qor-ci-vm-block-by-address",
            "New-VM -Name qor-ci-vm -Generation 2",
            "Set-VMFirmware -VMName qor-ci-vm -EnableSecureBoot On -SecureBootTemplate MicrosoftUEFICertificateAuthority",
            "Set-VM -Name qor-ci-vm -CheckpointType Disabled",
            "Add-VMNetworkAdapterAcl -VMName qor-ci-vm -Direction Outbound -Action Deny -RemoteIPAddress 192.168.0.0/16",
            "Add-VMNetworkAdapterAcl -VMName qor-ci-vm -Direction Outbound -Action Deny -RemoteIPAddress ::/0",
            "Add-VMNetworkAdapterAcl -VMName qor-ci-vm -Direction Outbound -Action Allow -RemoteIPAddress 10.213.57.1",
            "Convert-VHD",
            "Resize-VHD",
            "Save-Image https://cloud-images.ubuntu.com/releases/noble/release-20261017/",
            "Remove-VMDvdDrive -VMName qor-ci-vm",
        ):
            with self.subTest(call):
                self.assertIn(call, joined)
        self.assertEqual(sum("Disable-VMIntegrationService" in c for c in calls), 6, "every service off")
        # Nothing boots before the read-back, and it is read before the seed is removed and again after.
        self.assertLess(joined.index("Add-VMDvdDrive"), joined.index("Start-VM"))
        self.assertLess(joined.index("New-NetFirewallRule"), joined.index("Start-VM"), "no boot before the rules are there")
        self.assertLess(joined.index("Add-VMNetworkAdapterAcl"), joined.index("Start-VM"), "no boot before the ACL is there")
        self.assertIn("KEY: True", done.stdout)
        self.assertIn("SEED: False", done.stdout)
        self.assertIn("DONE: True", done.stdout)
        self.assertIn("STATE: Running", done.stdout)
        self.assertIn("UP: True", done.stdout)
        ssh = self.ssh(done)
        self.assertTrue(ssh[0].startswith("first: "), "only the first contact may take a new host key")
        self.assertFalse(ssh[-1].startswith("first: "), "the last must match the kept host key")
        self.assertIn("sudo -n touch /etc/cloud/cloud-init.disabled", ssh)
        # The last thing it says is the next command, for an ordinary terminal.
        self.assertIn("NOT as administrator", done.stdout)

    def test_every_setting_it_should_make_reads_back_right(self):
        done = self.world(self.create() + "$code = Invoke-Main 'Check'; \"CHECK=$code\"\n")
        self.assertIn("CHECK=0", done.stdout, done.stdout + done.stderr)
        self.assertNotIn("BAD", done.stdout)

    def test_a_second_run_changes_nothing_it_already_made(self):
        done = self.world(
            self.create() + "$before = $global:World.Calls.Count\n$global:World.Calls.Clear()\n" + self.create()
        )
        self.assertEqual(done.stdout.count("CODE=0"), 2, done.stdout + done.stderr)
        calls = self.calls(done)
        for made_once in ("New-VM ", "New-VMSwitch", "New-NetNat", "New-NetFirewallRule", "New-NetIPAddress", "Add-VMNetworkAdapterAcl", "Save-Image", "Convert-VHD", "Add-VMDvdDrive", "Start-VM"):
            with self.subTest(made_once):
                self.assertEqual([c for c in calls if c.startswith(made_once)], [])

    def test_a_run_again_after_stopping_at_each_step_finishes_the_job(self):
        for step in ("New-VMSwitch", "New-NetIPAddress", "New-NetNat", "New-NetFirewallRule", "New-VM", "Set-VMFirmware", "Add-VMNetworkAdapterAcl", "Add-VMDvdDrive", "Start-VM", "Remove-VMDvdDrive", "Convert-VHD"):
            with self.subTest(step):
                shutil.rmtree(self.folder / "root", ignore_errors=True)
                shutil.rmtree(self.folder / "local", ignore_errors=True)
                done = self.world(
                    f"$global:World.FailAt = '{step}'\n"
                    "$first = Invoke-Main 'Create'; \"FIRST=$first\"\n"
                    "$global:World.FailAt = ''\n"
                    "$second = Invoke-Main 'Create'; \"SECOND=$second\"\n"
                    "$check = Invoke-Main 'Check'; \"CHECK=$check\"\n"
                )
                self.assertIn("FIRST=1", done.stdout, done.stdout + done.stderr)
                self.assertIn("STOPPED at step", done.stdout)
                self.assertIn(f"the stand-in for {step} was told to fail", done.stdout)
                self.assertIn("SECOND=0", done.stdout, done.stdout + done.stderr)
                self.assertIn("CHECK=0", done.stdout, done.stdout)

    def test_a_setting_that_does_not_stick_stops_it_before_anything_boots(self):
        # The command says nothing went wrong, and only reading it back shows otherwise.
        done = self.world("function Set-VMProcessor { $null = Write-Call 'Set-VMProcessor' $args }\n" + self.create())
        self.assertIn("CODE=1", done.stdout, done.stdout + done.stderr)
        self.assertIn("BAD  nesting", done.stdout)
        self.assertIn("Nothing has been booted", done.stdout)
        self.assertNotIn("Start-VM", "\n".join(self.calls(done)))

    def test_it_stops_at_the_first_failure_and_goes_no_further(self):
        done = self.world("$global:World.FailAt = 'New-NetNat'\n" + self.create())
        self.assertIn("CODE=1", done.stdout)
        self.assertIn("STOPPED at step", done.stdout)
        joined = "\n".join(self.calls(done))
        self.assertNotIn("New-NetFirewallRule", joined)
        self.assertNotIn("New-VM ", joined)
        self.assertNotIn("Start-VM", joined)

    def test_what_is_not_its_own_stops_it_and_stays(self):
        for what, plant, says in (
            ("switch", "$global:World.Switches['qor-ci-vm'] = [pscustomobject] @{ Name = 'qor-ci-vm'; Notes = ''; SwitchType = 'External' }", "did not make it"),
            ("vm", "$global:World.VMs['qor-ci-vm'] = [pscustomobject] @{ Name = 'qor-ci-vm'; Notes = ''; State = 'Off' }", "did not make it"),
            ("another NAT", "$global:World.Nats['docker'] = [pscustomobject] @{ Name = 'docker'; InternalIPInterfaceAddressPrefix = '172.30.0.0/16' }", "one per host"),
            ("a route over its range", "$global:World.Routes = @([pscustomobject] @{ DestinationPrefix = '10.0.0.0/8'; InterfaceAlias = 'VPN'; NextHop = '0.0.0.0' })", "overlaps"),
            ("firewall off", "$global:World.Profiles[1].Enabled = 'False'", "is off"),
            ("too little memory", "$global:World.FreeMemory = 0.8", "One heavy job at a time"),
            ("too little disk", "$global:World.FreeDisk = 5", "wants 40"),
        ):
            with self.subTest(what):
                shutil.rmtree(self.folder / "root", ignore_errors=True)
                shutil.rmtree(self.folder / "local", ignore_errors=True)
                done = self.world(plant + "\n" + self.create())
                self.assertIn("CODE=1", done.stdout, done.stdout + done.stderr)
                self.assertIn(says, done.stdout)
                joined = "\n".join(self.calls(done))
                self.assertNotIn("Remove-", joined)
                self.assertNotIn("Start-VM", joined)

    def test_a_folder_that_holds_someone_elses_files_is_refused(self):
        root = self.folder / "root"
        root.mkdir()
        (root / "theirs.txt").write_text("not this script's")
        done = self.world(self.create())
        self.assertIn("CODE=1", done.stdout, done.stdout + done.stderr)
        self.assertIn("already holds files", done.stdout)
        self.assertEqual(self.calls(done), [])
        self.assertTrue((root / "theirs.txt").exists())

    def test_a_drive_top_is_refused(self):
        done = self.world("try { $null = Assert-SafeRoot 'C:\\'; 'TOOK IT' } catch { \"refused: $($_.Exception.Message)\" }\n")
        self.assertIn("refused", done.stdout, done.stdout + done.stderr)
        self.assertNotIn("TOOK IT", done.stdout)

    def test_a_second_choice_of_place_is_refused_once_one_is_made(self):
        done = self.world(self.create() + "$script:Asked['MemoryGB'] = 8\n$code = Invoke-Main 'Create'; \"AGAIN=$code\"\n")
        self.assertIn("AGAIN=1", done.stdout, done.stdout + done.stderr)
        self.assertIn("-Action Remove first", done.stdout)

    def test_a_guest_that_never_answers_stops_it_with_the_way_to_look(self):
        done = self.world("$global:World.GuestNeverAnswers = $true\n$script:RealWait = ${function:Wait-Guest}\nfunction Wait-Guest([int] $Seconds, [switch] $FirstContact) { $script:LastSshError = 'connection timed out'; return $false }\n" + self.create())
        self.assertIn("CODE=1", done.stdout, done.stdout + done.stderr)
        self.assertIn("vmconnect.exe localhost qor-ci-vm", done.stdout)
        self.assertIn("owner's decision", done.stdout, "a Secure Boot refusal is not turned off here")
        joined = "\n".join(self.calls(done))
        self.assertNotIn("EnableSecureBoot Off", joined)


class Check(WithAWorld):
    def test_each_planted_fault_is_named(self):
        body = [self.create(), "$vm = $global:World.VMs['qor-ci-vm']\n"]
        # One process, as a world takes a moment to make. Each plant is undone by putting a saved copy back.
        for index, (name, plant) in enumerate(PLANTS):
            body.append(
                f"$savedVm = $global:World.VMs['qor-ci-vm'] | ConvertTo-Json -Depth 6\n"
                f"$savedAcls = @($vm.Acls)\n"
                f"$savedRules = @{{}}; foreach ($k in $global:World.Rules.Keys) {{ $savedRules[$k] = $global:World.Rules[$k].PSObject.Copy() }}\n"
                f"$savedNats = @{{}}; foreach ($k in $global:World.Nats.Keys) {{ $savedNats[$k] = $global:World.Nats[$k].PSObject.Copy() }}\n"
                f"$savedSwitch = $global:World.Switches['qor-ci-vm'].PSObject.Copy()\n"
                f"$savedAddress = $global:World.Addresses[0].PrefixLength\n"
                f"$savedProfile = $global:World.Profiles[2].Enabled\n"
                f"$savedBinding = ($global:World.Bindings['vEthernet (qor-ci-vm)'] | Where-Object ComponentID -EQ 'ms_server').Enabled\n"
                f"$savedFacts = @{{ G=$vm.Generation; N=$vm.Notes; C=$vm.CheckpointType; A=$vm.AutomaticCheckpointsEnabled; S=$vm.AutomaticStartAction; T=$vm.AutomaticStopAction; E=$vm.EnhancedSessionTransportType; B=$vm.BatteryPassthroughEnabled; Tpm=$vm.Tpm; SB=$vm.Firmware.SecureBoot; SBT=$vm.Firmware.SecureBootTemplate; BO=$vm.Firmware.BootOrder; D=$vm.Memory.DynamicMemoryEnabled; M=$vm.Memory.Startup; P=$vm.Processor.Count; X=$vm.Processor.ExposeVirtualizationExtensions; Disks=$vm.Disks; Snap=$vm.Snapshots; Gpu=$vm.Gpu; As=$vm.Assigned; F=$vm.Fibre; AS2=$vm.Adapter.SwitchName; Mac=$vm.Adapter.MacAddressSpoofing; Dh=$vm.Adapter.DhcpGuard; Ro=$vm.Adapter.RouterGuard; Mi=$vm.Adapter.PortMirroringMode; Te=$vm.Adapter.AllowTeaming; Dn=$vm.Adapter.DeviceNaming; Sv=@($vm.Services | ForEach-Object Enabled) }}\n"
                f"{plant}\n"
                f"$code = Invoke-Main 'Check'; \"PLANT {index} EXIT $code\"\n"
                # And back.
                "$vm.Generation=$savedFacts.G; $vm.Notes=$savedFacts.N; $vm.CheckpointType=$savedFacts.C; $vm.AutomaticCheckpointsEnabled=$savedFacts.A; $vm.AutomaticStartAction=$savedFacts.S; $vm.AutomaticStopAction=$savedFacts.T; $vm.EnhancedSessionTransportType=$savedFacts.E; $vm.BatteryPassthroughEnabled=$savedFacts.B; $vm.Tpm=$savedFacts.Tpm; $vm.Firmware.SecureBoot=$savedFacts.SB; $vm.Firmware.SecureBootTemplate=$savedFacts.SBT; $vm.Firmware.BootOrder=$savedFacts.BO; $vm.Memory.DynamicMemoryEnabled=$savedFacts.D; $vm.Memory.Startup=$savedFacts.M; $vm.Processor.Count=$savedFacts.P; $vm.Processor.ExposeVirtualizationExtensions=$savedFacts.X; $vm.Disks=$savedFacts.Disks; $vm.Snapshots=$savedFacts.Snap; $vm.Gpu=$savedFacts.Gpu; $vm.Assigned=$savedFacts.As; $vm.Fibre=$savedFacts.F; $vm.Adapter.SwitchName=$savedFacts.AS2; $vm.Adapter.MacAddressSpoofing=$savedFacts.Mac; $vm.Adapter.DhcpGuard=$savedFacts.Dh; $vm.Adapter.RouterGuard=$savedFacts.Ro; $vm.Adapter.PortMirroringMode=$savedFacts.Mi; $vm.Adapter.AllowTeaming=$savedFacts.Te; $vm.Adapter.DeviceNaming=$savedFacts.Dn\n"
                "$i=0; foreach ($s in $vm.Services) { $s.Enabled = $savedFacts.Sv[$i]; $i++ }\n"
                "$vm.Dvds.Clear(); $vm.Acls.Clear(); foreach ($a in $savedAcls) { $vm.Acls.Add($a) }\n"
                "$global:World.Rules = $savedRules; $global:World.Nats = $savedNats; $global:World.Switches['qor-ci-vm'] = $savedSwitch; $global:World.Addresses[0].PrefixLength = $savedAddress; $global:World.Profiles[2].Enabled = $savedProfile\n"
                "($global:World.Bindings['vEthernet (qor-ci-vm)'] | Where-Object ComponentID -EQ 'ms_server').Enabled = $savedBinding\n"
                f"$code = Invoke-Main 'Check'; \"RESTORED {index} EXIT $code\"\n"
            )
        done = self.world("".join(body))
        self.assertIn("CODE=0", done.stdout, done.stdout + done.stderr)
        sections = done.stdout.split("PLANT ")[1:]
        self.assertEqual(len(sections), len(PLANTS), done.stdout[-2000:] + done.stderr)
        previous = done.stdout.split("PLANT ")[0]
        for index, (name, plant) in enumerate(PLANTS):
            with self.subTest(name=name, plant=plant):
                self.assertTrue(sections[index].startswith(f"{index} EXIT 1"), f"Check did not fail: {sections[index][:40]}")
                self.assertIn(f"RESTORED {index} EXIT 0", sections[index], "the test's undoing did not undo it, so what follows shows nothing")
                # What Check printed comes before its "PLANT n EXIT" line, so at the end of the section before.
                before = previous if index == 0 else sections[index - 1]
                self.assertRegex(before, rf"BAD  {name}:", "Check did not name it")

    def test_an_ordinary_account_is_told_it_cannot_read_a_vm(self):
        done = self.world(self.create() + "$global:World.Admin = $false\n$code = Invoke-Main 'Check'; \"CHECK=$code\"\n")
        self.assertIn("CHECK=1", done.stdout)
        self.assertIn("needs an administrator", done.stdout)


class Remove(WithAWorld):
    def test_it_takes_exactly_what_it_made_and_nothing_else(self):
        (self.folder / "pinned-copy").write_text("x")
        done = self.world(
            self.create()
            + "$global:World.Calls.Clear()\n"
            + "$code = Invoke-Main 'Remove'; \"REMOVE=$code\"\n"
            + "\"VMS: $($global:World.VMs.Keys -join ',')\"\n"
            + "\"SWITCHES: $($global:World.Switches.Keys -join ',')\"\n"
            + "\"RULES: $($global:World.Rules.Keys -join ',')\"\n"
            + "\"NATS: $($global:World.Nats.Keys -join ',')\"\n"
            + "\"ADDRESSES: $($global:World.Addresses.Count)\"\n"
        )
        self.assertIn("REMOVE=0", done.stdout, done.stdout + done.stderr)
        self.assertIn("VMS: someone-elses-vm\n", done.stdout)
        self.assertIn("SWITCHES: someone-elses-switch\n", done.stdout)
        self.assertIn("RULES: someone-elses-rule\n", done.stdout)
        self.assertIn("NATS: \n", done.stdout)
        self.assertIn("ADDRESSES: 0", done.stdout)
        self.assertFalse((self.folder / "root").exists(), "its folder went")
        self.assertFalse((self.folder / "local" / "qor-ci-vm").exists(), "the key went")
        allowed = {
            "Stop-VM -Name qor-ci-vm -TurnOff -Force",
            "Remove-VM -Name qor-ci-vm -Force",
            "Remove-NetFirewallRule -Name qor-ci-vm-block-by-adapter",
            "Remove-NetFirewallRule -Name qor-ci-vm-block-by-address",
            "Remove-NetNat -Name qor-ci-vm -Confirm: False",
            "Remove-NetIPAddress -InterfaceAlias vEthernet (qor-ci-vm) -IPAddress 10.213.57.1 -Confirm: False -ErrorAction SilentlyContinue",
            "Remove-VMSwitch -Name qor-ci-vm -Force",
        }
        self.assertEqual(set(self.calls(done)) - allowed, set(), "it called something else")
        self.assertEqual(allowed - set(self.calls(done)), set(), "it left something")

    def test_a_second_run_finds_nothing_to_do(self):
        done = self.world(self.create() + "$first = Invoke-Main 'Remove'\n$global:World.Calls.Clear()\n$code = Invoke-Main 'Remove'; \"AGAIN=$code\"\n")
        self.assertIn("AGAIN=0", done.stdout, done.stdout + done.stderr)
        self.assertEqual(self.calls(done), [])

    def test_a_folder_with_something_else_in_it_stays_with_that_in_it(self):
        done = self.world(
            self.create()
            + "Set-Content -LiteralPath (Join-Path $script:Settings.Root 'owners-note.txt') -Value 'mine'\n"
            + "$code = Invoke-Main 'Remove'; \"REMOVE=$code\"\n"
            + "$code = Invoke-Main 'Remove'; \"AGAIN=$code\"\n"
        )
        self.assertIn("REMOVE=0", done.stdout, done.stdout + done.stderr)
        self.assertIn("AGAIN=0", done.stdout, done.stdout + done.stderr)
        self.assertTrue((self.folder / "root" / "owners-note.txt").exists())
        self.assertFalse((self.folder / "root" / "disk").exists())

    def test_what_has_its_name_and_not_its_mark_stays(self):
        for what, plant in (
            ("vm", "$global:World.VMs['qor-ci-vm'].Notes = 'someone else'"),
            ("switch", "$global:World.Switches['qor-ci-vm'].Notes = 'someone else'"),
            ("rule", "$global:World.Rules['qor-ci-vm-block-by-address'].Description = 'someone else'"),
        ):
            with self.subTest(what):
                shutil.rmtree(self.folder / "root", ignore_errors=True)
                shutil.rmtree(self.folder / "local", ignore_errors=True)
                done = self.world(self.create() + plant + "\n$code = Invoke-Main 'Remove'; \"REMOVE=$code\"\n")
                self.assertIn("REMOVE=1", done.stdout, done.stdout + done.stderr)
                self.assertIn("did not make it, so it stays", done.stdout)

    def test_an_ordinary_account_is_refused(self):
        done = self.world(self.create() + "$global:World.Admin = $false\n$global:World.Calls.Clear()\n$code = Invoke-Main 'Remove'; \"REMOVE=$code\"\n")
        self.assertIn("REMOVE=1", done.stdout)
        self.assertEqual(self.calls(done), [])


class StartAndStop(WithAWorld):
    def test_an_ordinary_account_cannot_start_it_and_is_told_the_choices_and_nothing_changes(self):
        done = self.world(self.create() + "$global:World.Admin = $false\n$global:World.Calls.Clear()\n$code = Invoke-Main 'Start'; \"START=$code\"\n")
        self.assertIn("START=2", done.stdout, done.stdout + done.stderr)
        self.assertIn("Hyper-V Administrators", done.stdout)
        self.assertIn("Recommended: 1", done.stdout)
        self.assertEqual(self.calls(done), [])

    def test_an_ordinary_account_can_stop_it_by_ssh_alone(self):
        done = self.world(self.create() + "$global:World.Admin = $false\n$global:World.Calls.Clear()\n$code = Invoke-Main 'Stop'; \"STOP=$code\"\n\"STATE: $($global:World.VMs['qor-ci-vm'].State)\"\n")
        self.assertIn("STOP=0", done.stdout, done.stdout + done.stderr)
        self.assertIn("STATE: Off", done.stdout)
        self.assertEqual(self.calls(done), [], "no Hyper-V command")
        self.assertIn("sudo -n systemctl poweroff", self.ssh(done))

    def test_a_guest_that_will_not_stop_is_reported_and_an_ordinary_account_cannot_force_it(self):
        done = self.world(self.create() + "$global:World.Admin = $false\n$global:World.GuestRefusesToStop = $true\n$global:World.Calls.Clear()\n$code = Invoke-Main 'Stop'; \"STOP=$code\"\n")
        self.assertIn("STOP=1", done.stdout, done.stdout + done.stderr)
        self.assertIn("Stop-VM -Name qor-ci-vm -TurnOff -Force", done.stdout)
        self.assertEqual(self.calls(done), [])

    def test_an_administrator_turns_off_a_guest_that_will_not_stop(self):
        done = self.world(self.create() + "$global:World.GuestRefusesToStop = $true\n$global:World.Calls.Clear()\n$code = Invoke-Main 'Stop'; \"STOP=$code\"\n")
        self.assertIn("STOP=0", done.stdout, done.stdout + done.stderr)
        self.assertIn("Stop-VM -Name qor-ci-vm -TurnOff -Force", self.calls(done))

    def test_start_refuses_when_memory_will_not_fit(self):
        done = self.world(self.create() + "$null = Invoke-Main 'Stop'\n$global:World.FreeMemory = 0.8\n$global:World.Calls.Clear()\n$code = Invoke-Main 'Start'; \"START=$code\"\n")
        self.assertIn("START=1", done.stdout, done.stdout + done.stderr)
        self.assertIn("One heavy job at a time", done.stdout)
        self.assertEqual(self.calls(done), [])


class ProveVerdict(WithAWorld):
    @staticmethod
    def lines(level: str, **change) -> list[str]:
        categories = guest.CATEGORIES[level]
        body = [f"PASS  {c} tried: nothing" for c in categories]
        passed = len(categories)
        return body + [f"END level={level} passed={passed} failed=0 unrun=0 stand-in=no"]

    def verdict(self, level: str, code: int, lines: list[str]) -> subprocess.CompletedProcess:
        path = self.folder / "lines.txt"
        path.write_text("\n".join(lines), encoding="utf-8")
        return run_pwsh(
            f"$lines = [string[]] @(Get-Content -LiteralPath '{path}')\n"
            f"$v = Get-LevelVerdict '{level}' {code} $lines\n"
            "\"OK=$($v.Ok)\"\n$v.Reasons\n",
            self.env,
        )

    def test_every_line_passing_with_an_end_is_the_one_way_through(self):
        for level in ("user", "root"):
            with self.subTest(level):
                done = self.verdict(level, 0, self.lines(level))
                self.assertIn("OK=True", done.stdout, done.stdout + done.stderr)

    def test_each_way_of_not_having_shown_it_fails(self):
        good = self.lines("user")
        for name, code, lines, says in (
            ("a failed attack", 1, [good[0].replace("PASS ", "FAIL "), *good[1:-1], "END level=user passed=10 failed=1 unrun=0 stand-in=no"], "got through"),
            ("a check that did not run", 1, [good[0].replace("PASS ", "UNRUN"), *good[1:-1], "END level=user passed=10 failed=0 unrun=1 stand-in=no"], "could not run"),
            ("an exit status of 0 over a failed line", 0, [good[0].replace("PASS ", "FAIL "), *good[1:]], "got through"),
            ("all passing with a bad exit status", 1, good, "exited 1"),
            ("cut short", 0, good[:-1], "no END line"),
            ("cut short partway", 255, good[:3], "no END line"),
            ("nothing at all", 0, [], "no passing line"),
            ("an END that lies about its count", 0, [*good[:-1], "END level=user passed=99 failed=0 unrun=0 stand-in=no"], "does not match"),
            ("an END for the other level", 0, [*good[:-1], f"END level=root passed={len(good) - 1} failed=0 unrun=0 stand-in=no"], "says it is level root"),
            ("a run on stand-ins", 0, [*good[:-1], f"END level=user passed={len(good) - 1} failed=0 unrun=0 stand-in=yes"], "stand-ins"),
            ("a category left out", 0, [*good[1:-1], f"END level=user passed={len(good) - 2} failed=0 unrun=0 stand-in=no"], "says nothing about where"),
        ):
            with self.subTest(name):
                done = self.verdict("user", code, lines)
                self.assertIn("OK=False", done.stdout, done.stdout + done.stderr)
                self.assertIn(says, done.stdout)

    def test_the_two_lists_of_categories_are_one_list(self):
        done = run_pwsh("foreach ($k in 'user','root') { \"$k=\" + ($script:Categories[$k] -join ',') }\n", self.env)
        self.assertIn("user=" + ",".join(guest.CATEGORIES["user"]), done.stdout, done.stdout + done.stderr)
        self.assertIn("root=" + ",".join(guest.CATEGORIES["root"]), done.stdout)

    def test_what_the_real_guest_script_prints_is_what_this_reads(self):
        # All the way through, from the guest script's own output, stopped only by its being on stand-ins.
        with GuestWorld() as world:
            code, printed = world.run("user")
        self.assertEqual(code, 0, printed)
        done = self.verdict("user", code, printed.splitlines())
        self.assertIn("OK=False", done.stdout, done.stdout + done.stderr)
        self.assertIn("stand-ins", done.stdout)
        self.assertEqual(sum("level user" in line for line in done.stdout.splitlines()), 1, "and for no other reason: " + done.stdout)

    def prove(self, plant: str = "") -> subprocess.CompletedProcess:
        user = json.dumps(self.lines("user"))
        root = json.dumps(self.lines("root"))
        path_u, path_r = self.folder / "u.json", self.folder / "r.json"
        path_u.write_text(user)
        path_r.write_text(root)
        return self.world(
            self.create()
            + "$global:World.Admin = $false\n"
            + f"$u = Get-Content -Raw -LiteralPath '{path_u}' | ConvertFrom-Json\n"
            + f"$r = Get-Content -Raw -LiteralPath '{path_r}' | ConvertFrom-Json\n"
            + "$global:World.SshAnswers['*--level user*'] = [pscustomobject] @{ ExitCode = 0; Lines = [string[]] $u; Error = '' }\n"
            + "$global:World.SshAnswers['*--level root*'] = [pscustomobject] @{ ExitCode = 0; Lines = [string[]] $r; Error = '' }\n"
            + "$global:World.Calls.Clear()\n"
            + plant
            + "\n$code = Invoke-Main 'Prove'; \"PROVE=$code\"\n"
            + "\"UP: $($global:World.GuestUp)\"\n"
        )

    def test_a_clean_run_exits_zero_and_powers_off_and_claims_only_what_it_tried(self):
        done = self.prove()
        self.assertIn("PROVE=0", done.stdout, done.stdout + done.stderr)
        self.assertIn("UP: False", done.stdout, "it holds no memory after")
        self.assertIn("says what was tried and no more", done.stdout)
        self.assertNotRegex(done.stdout.lower(), r"boundary (holds|is proven|is proved)")
        self.assertEqual(self.calls(done), [], "no Hyper-V command, from an ordinary account")
        reports = list((self.folder / "local" / "qor-ci-vm").glob("prove-*.txt"))
        self.assertEqual(len(reports), 1)
        self.assertIn("Commit ", reports[0].read_text(encoding="utf-8"))

    def test_keep_running_leaves_it_up(self):
        done = self.prove("$KeepRunning = $true\n")
        self.assertIn("PROVE=0", done.stdout, done.stdout + done.stderr)
        self.assertIn("UP: True", done.stdout)

    def test_a_firewall_rule_gone_fails_it_whatever_the_guest_says(self):
        done = self.prove("$global:World.Rules.Remove('qor-ci-vm-block-by-adapter')\n")
        self.assertIn("PROVE=1", done.stdout, done.stdout + done.stderr)
        self.assertIn("PROVE FAILED", done.stdout)
        self.assertIn("qor-ci-vm-block-by-adapter", done.stdout)

    def test_a_guest_that_says_it_failed_fails_it(self):
        done = self.prove("$global:World.SshAnswers['*--level root*'].ExitCode = 1\n")
        self.assertIn("PROVE=1", done.stdout, done.stdout + done.stderr)

    def test_a_stopped_guest_is_a_plain_failure_that_attacks_nothing(self):
        done = self.world(self.create() + "$null = Invoke-Main 'Stop'\n$global:World.Admin = $false\n$script:Dummy = 1\nfunction Wait-Guest([int] $Seconds, [switch] $FirstContact) { $script:LastSshError = 'connection timed out'; return $false }\n$code = Invoke-Main 'Prove'; \"PROVE=$code\"\n")
        self.assertIn("PROVE=1", done.stdout, done.stdout + done.stderr)
        self.assertIn("only an administrator can start it", done.stdout)
        self.assertIn("nothing is shown", done.stdout)

    def test_it_refuses_to_run_as_an_administrator(self):
        done = self.world(self.create() + "$code = Invoke-Main 'Prove'; \"PROVE=$code\"\n")
        self.assertIn("PROVE=1", done.stdout, done.stdout + done.stderr)
        self.assertIn("ordinary terminal", done.stdout)

    def test_with_no_create_there_is_nothing_to_attack(self):
        done = self.world("$global:World.Admin = $false\n$code = Invoke-Main 'Prove'; \"PROVE=$code\"\n")
        self.assertIn("PROVE=1", done.stdout, done.stdout + done.stderr)
        self.assertIn("-Action Create comes first", done.stdout)

    def test_a_guests_escape_codes_are_not_passed_to_the_terminal(self):
        done = run_pwsh("Protect-Text \"PASS  x $([char] 27)[2Jhidden$([char] 7)$([char] 0x9B)31m\"\n", self.env)
        self.assertNotIn("\x1b", done.stdout, done.stdout)
        self.assertNotIn("\x07", done.stdout)
        self.assertNotIn("\u009b", done.stdout)
        self.assertIn("hidden", done.stdout)

    def test_what_it_aims_at(self):
        done = self.world(
            "$null = Initialize-Settings\n"
            "$t = Get-ProveTargets\n"
            "foreach ($k in $t.Keys) { \"$k=\" + (@($t[$k]) -join ',') }\n"
        )
        got = dict(line.split("=", 1) for line in done.stdout.splitlines() if "=" in line and not line.startswith(" "))
        self.assertEqual(got["gateway"], "10.213.57.1", done.stdout + done.stderr)
        self.assertEqual(got["vm_ip"], "10.213.57.2")
        self.assertEqual(got["host_addresses"], "172.22.0.1,192.168.1.50")
        self.assertEqual(got["host_addresses_v6"], "2001:db8::5", "link-local and loopback are not routes")
        # The router, by its route and as DNS, and a neighbour. Not a public resolver, nor multicast, nor the VM itself.
        self.assertEqual(got["lan"], "192.168.1.1,192.168.1.20")
        self.assertEqual(got["wsl"], "172.22.0.1,172.22.12.5")
        self.assertEqual(got["loopback_ports"], "8123")
        ports = got["host_ports"].split(",")
        for port in ("445", "3389", "5985", "5986", "139", "49664"):
            with self.subTest(port):
                self.assertIn(port, ports)
        self.assertNotIn("8123", ports, "a loopback listener is aimed at by raw frame, not by address")
        vsock = got["vsock_ports"].split(",")
        self.assertIn("50002", vsock, "the 9P route")
        self.assertIn("2761", vsock, "what is registered for Hyper-V sockets")
        self.assertEqual(got["sweep"], "1,65535")
        self.assertEqual(got["control_host"], "github.com")


@unittest.skipUnless(PWSH and os.name == "nt", "needs Windows' ssh-keygen and icacls")
class TheRealKey(WithPwsh):
    def run_real(self, body: str) -> subprocess.CompletedProcess:
        # The real New-GuestKey: with no stand-ins, in a folder of the test's. It touches nothing of Hyper-V.
        with tempfile.TemporaryDirectory() as folder:
            driver = Path(folder) / "driver.ps1"
            driver.write_text(f". '{TOOL}' -Root $env:QOR_TEST_ROOT\n{body}\n", encoding="utf-8")
            return subprocess.run([PWSH, "-NoProfile", "-NonInteractive", "-File", str(driver)], capture_output=True, text=True, timeout=120, env={**os.environ, **self.env}, check=False, **HIDDEN)

    def test_it_is_made_for_this_account_alone_and_never_printed(self):
        done = self.run_real(
            "$null = Initialize-Settings\nNew-GuestKey\n\"PUBLIC: $(Get-GuestPublicKey)\"\n"
            "(Get-Acl -LiteralPath $script:KeyDir).Access | ForEach-Object { \"ACE: $($_.IdentityReference)\" }\n"
            "\"INHERITS: $(-not (Get-Acl -LiteralPath $script:KeyDir).AreAccessRulesProtected)\"\n"
            "(Get-Acl -LiteralPath $script:KeyFile).Access | ForEach-Object { \"KEY ACE: $($_.IdentityReference)\" }\n"
            "& $script:SshKeygenExe -y -f $script:KeyFile | Out-Null; \"USABLE: $LASTEXITCODE\"\n"
            "New-GuestKey\n"
        )
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertRegex(done.stdout, r"PUBLIC: ssh-ed25519 \S+ qor-ci-vm")
        self.assertIn("INHERITS: False", done.stdout)
        self.assertIn("USABLE: 0", done.stdout, "OpenSSH reads it under those permissions")
        self.assertIn("already in", done.stdout, "a second run keeps the key")
        for line in done.stdout.splitlines():
            if "ACE:" in line:
                self.assertRegex(line, r"(?i)(SYSTEM|" + os.environ.get("USERNAME", "") + r")", f"someone else can read it: {line}")
        private = (self.folder / "local" / "qor-ci-vm" / "ssh" / "id_ed25519").read_text()
        body = "".join(private.splitlines()[1:-1])
        self.assertNotIn(body[:40], done.stdout + done.stderr, "the private key was printed")
        self.assertNotIn("PRIVATE KEY", done.stdout + done.stderr)

    def test_ssh_reads_the_options_it_is_given(self):
        done = self.run_real(
            "$null = Initialize-Settings\n"
            "$a = Get-SshArguments\n"
            "& $script:SshExe @a -G | Select-String -Pattern '^(stricthostkeychecking|userknownhostsfile|globalknownhostsfile|forwardagent|forwardx11|batchmode|identitiesonly|permitlocalcommand|updatehostkeys|hostname|user|requesttty) ' | ForEach-Object { $_.Line }\n"
            "'---'\n"
            "$a = Get-SshArguments -FirstContact\n"
            "& $script:SshExe @a -G | Select-String -Pattern '^stricthostkeychecking ' | ForEach-Object { $_.Line }\n"
        )
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        later, first = done.stdout.split("---")
        for expected in ("stricthostkeychecking true", "forwardagent no", "forwardx11 no", "batchmode yes", "identitiesonly yes", "permitlocalcommand no", "updatehostkeys false", "hostname 10.213.57.2", "user qor", "requesttty false"):
            with self.subTest(expected):
                self.assertIn(expected, later)
        self.assertIn("known_hosts", later.replace("\\", "/"))
        self.assertIn("stricthostkeychecking accept-new", first)


# ---------------------------------------------------------------------------------------------------------------------
# The guest script, in this process, with the far side stood in for. Nothing here opens AF_VSOCK or a raw socket.

TARGETS = {
    "vm_ip": "10.213.57.2",
    "gateway": "10.213.57.1",
    "control_host": "github.com",
    "host_addresses": ["192.168.1.50", "172.22.0.1"],
    "host_addresses_v6": ["2001:db8::5"],
    "host_ports": [445, 3389, 5985],
    "loopback_ports": [8123],
    "lan": ["192.168.1.1"],
    "lan_ports": [22, 445],
    "wsl": ["172.22.0.1", "172.22.12.5"],
    "vsock_ports": [2761, 50002],
    "sweep": [1, 40],
}


class FakeFrames:
    """A raw socket that remembers what was sent and has only what it was told to hand back, each time it is opened."""

    def __init__(self, replies=()):
        self.sent = []
        self.replies = list(replies)
        self.at = 0

    def __enter__(self):
        self.at = 0
        return self

    def __exit__(self, *_):
        return False

    def send(self, frame):
        self.sent.append(frame)

    def settimeout(self, _):
        pass

    def next_reply(self):
        if self.at < len(self.replies):
            self.at += 1
            return self.replies[self.at - 1]
        raise TimeoutError

    def recv(self, _):
        return self.next_reply()

    def sendto(self, frame, address):
        self.sent.append((frame, address))

    def recvfrom(self, _):
        return self.next_reply()


class GuestWorld:
    """A stand-in tree for what the script reads, and stand-ins for what it reaches, all shut until a test opens one."""

    def __enter__(self):
        self.folder = tempfile.TemporaryDirectory()
        self.root = Path(self.folder.name)
        self.answers = {}
        self.vsock = {}
        self.https = ["HTTP/1.1 200 OK", "HTTP/1.1 200 OK"]
        self.dns = ["140.82.121.4"]
        self.dns_error = None
        self.commands = {}
        self.frames = FakeFrames()
        self.icmp4 = FakeFrames()
        self.icmp6 = FakeFrames()
        self.udp = ("PASS", "no answer in 2 s")
        self.targets = json.loads(json.dumps(TARGETS))
        self.targets["host_time"] = __import__("time").time()
        self.uid = None
        for path, text in (
            ("sys/class/dmi/id/sys_vendor", "Microsoft Corporation\n"),
            ("sys/class/dmi/id/product_name", "Virtual Machine\n"),
            ("proc/sys/kernel/osrelease", "6.8.0-147-generic\n"),
            ("proc/self/mountinfo", "20 1 8:1 / / rw - ext4 /dev/sda1 rw\n21 20 0:5 / /proc rw - proc proc rw\n"),
            ("proc/cpuinfo", "processor : 0\nflags : fpu sse sse2 hypervisor\n"),
            ("proc/net/route", "Iface Destination Gateway Flags\neth0 00000000 0139D50A 0003\n"),
            ("proc/net/arp", "IP HW Flags HWaddr Mask Device\n10.213.57.1 0x1 0x2 00:15:5d:00:00:01 * eth0\n"),
            ("proc/net/if_inet6", "fe800000000000000215 02 40 20 80 eth0\n".replace("fe800000000000000215", "fe80000000000000021500fffe000002")),
            ("sys/class/net/eth0/address", "00:15:5d:00:00:02\n"),
            ("sys/kernel/security/lockdown", "none [integrity] confidentiality\n"),
        ):
            target = self.root / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(text, encoding="utf-8")
        (self.root / "sys/block/sda").mkdir(parents=True)
        (self.root / "sys/block/loop0").mkdir()
        (self.root / "sys/bus/pci/devices").mkdir(parents=True)
        (self.root / "mnt").mkdir()
        (self.root / "media").mkdir()
        (self.root / "dev/dri").mkdir(parents=True)
        self.add_vmbus("scsi", "ba6163d9-04a1-4d29-b605-72e2ffb1dc7f")
        self.add_vmbus("nic", "f8615163-df3e-46c5-913f-f2d2f965ed0e")
        self.add_vmbus("keyboard", "f912ad6d-2b17-48ea-bd65-f927a61c7684")
        efi = self.root / guest.SECURE_BOOT_VARIABLE.lstrip("/")
        efi.parent.mkdir(parents=True)
        efi.write_bytes(b"\x06\x00\x00\x00\x01")
        return self

    def __exit__(self, *_):
        self.folder.cleanup()
        return False

    def add_vmbus(self, name: str, class_id: str) -> None:
        device = self.root / "sys/bus/vmbus/devices" / name
        device.mkdir(parents=True)
        (device / "class_id").write_text("{" + class_id + "}\n", encoding="ascii")

    def connect_many(self, addresses, timeout, width):
        return {a: self.answers.get(a, 0 if a[0] == "127.0.0.1" else guest.TIMED_OUT) for a in addresses}

    def https_status(self, host, port, timeout):
        answer = self.https.pop(0) if self.https else OSError("network is down")
        if isinstance(answer, Exception):
            raise answer
        return answer

    def resolve(self, name):
        if self.dns_error:
            raise self.dns_error
        return self.dns

    def run_command(self, arguments, timeout=30.0):
        return self.commands.get(arguments[0], (1, f"{arguments[0]}: stand-in says no"))

    def run(self, level: str) -> tuple[int, str]:
        uid = self.uid if self.uid is not None else (0 if level == "root" else 2001)
        out = io.StringIO()
        if level == "root":
            self.commands.setdefault("modprobe", (0, ""))
        with (
            mock.patch.object(guest, "connect_many", self.connect_many),
            mock.patch.object(guest, "connect_vsock", lambda cid, port, timeout: self.vsock.get((cid, port), errno.ECONNRESET)),
            mock.patch.object(guest, "https_status", self.https_status),
            mock.patch.object(guest, "resolve", self.resolve),
            mock.patch.object(guest, "run_command", self.run_command),
            mock.patch.object(guest, "udp_dns", lambda address, timeout: self.udp),
            mock.patch.object(guest, "open_frame_socket", lambda interface: self.frames),
            mock.patch.object(guest, "open_icmp", lambda version: self.icmp6 if version == 6 else self.icmp4),
            mock.patch.object(guest, "effective_uid", lambda: uid),
            mock.patch.object(guest, "FRAME_WAIT", 0.05),
            mock.patch.object(guest, "CLOCK_WAIT", 0.0),
            mock.patch.object(socket, "if_nametoindex", lambda name: 2),
            mock.patch.object(sys, "stdin", io.StringIO(json.dumps(self.targets))),
            contextlib.redirect_stdout(out),
        ):
            code = guest.main(["--level", level, "--fs-root", str(self.root)])
        return code, out.getvalue()


class TheGuestsJudgement(unittest.TestCase):
    def test_tcp(self):
        for code, status in (
            (0, "FAIL"),
            (errno.ECONNREFUSED, "FAIL"),
            (errno.ECONNRESET, "FAIL"),
            (guest.TIMED_OUT, "PASS"),
            (errno.ETIMEDOUT, "PASS"),
            (errno.EHOSTUNREACH, "PASS"),
            (errno.ENETUNREACH, "PASS"),
            (errno.EACCES, "UNRUN"),
            (errno.EMFILE, "UNRUN"),
            (errno.ENOBUFS, "UNRUN"),
            (errno.EINVAL, "UNRUN"),
        ):
            with self.subTest(errno.errorcode.get(code, code)):
                self.assertEqual(guest.judge_tcp(code, 2.0)[0], status)

    def test_vsock(self):
        for code, level, status in (
            (0, "user", "FAIL"),
            (0, "root", "FAIL"),
            (guest.NO_FAMILY, "user", "PASS"),
            (guest.NO_FAMILY, "root", "UNRUN"),
            (guest.TIMED_OUT, "user", "PASS"),
            (errno.ECONNRESET, "user", "PASS"),
            (errno.ENODEV, "root", "PASS"),
            (errno.EPERM, "user", "UNRUN"),
            (errno.EACCES, "root", "UNRUN"),
        ):
            with self.subTest(code=code, level=level):
                self.assertEqual(guest.judge_vsock(code, level)[0], status)


class TheGuestsWholeRun(unittest.TestCase):
    def assert_line(self, printed: str, status: str, words: str):
        lines = [line for line in printed.splitlines() if line.startswith(status) and words in line]
        self.assertTrue(lines, f"no {status} line with '{words}' in:\n{printed}")

    def test_with_everything_shut_it_passes_and_says_each_attempt_and_that_it_ran_on_stand_ins(self):
        for level in ("user", "root"):
            with self.subTest(level), GuestWorld() as world:
                code, printed = world.run(level)
                self.assertEqual(code, 0, printed)
                self.assertRegex(printed, rf"END level={level} passed=\d+ failed=0 unrun=0 stand-in=yes")
                for category in guest.CATEGORIES[level]:
                    self.assert_line(printed, "PASS", category)
                self.assert_line(printed, "PASS", "tcp 192.168.1.50:445: no answer")
                self.assert_line(printed, "PASS", "tcp 192.168.1.1:445: no answer")
                self.assert_line(printed, "PASS", "tcp 172.22.12.5:8123: no answer")
                self.assert_line(printed, "PASS", "tcp [2001:db8::5]:3389: no answer")
                self.assert_line(printed, "PASS", "port 50002, the 9P route")
                self.assert_line(printed, "PASS", "HTTPS before the attacks")
                self.assert_line(printed, "PASS", "HTTPS after the attacks")
                self.assert_line(printed, "PASS", "DNS before the attacks")

    def plant_fails(self, plant, status: str, words: str, level: str = "user"):
        with GuestWorld() as world:
            plant(world)
            code, printed = world.run(level)
            self.assertEqual(code, 1, printed)
            self.assert_line(printed, status, words)
            self.assertIn("END level=" + level, printed, "it still finishes, and says so")

    def test_an_attack_that_gets_through_fails_the_run(self):
        for name, plant, words in (
            ("smb on the host", lambda w: w.answers.update({("192.168.1.50", 445): 0}), "tcp 192.168.1.50:445: connected"),
            ("rdp on the gateway", lambda w: w.answers.update({("10.213.57.1", 3389): 0}), "tcp 10.213.57.1:3389: connected"),
            ("a refusal from the host", lambda w: w.answers.update({("10.213.57.1", 5985): errno.ECONNREFUSED}), "tcp 10.213.57.1:5985: refused"),
            ("the LAN", lambda w: w.answers.update({("192.168.1.1", 22): 0}), "tcp 192.168.1.1:22: connected"),
            ("WSL", lambda w: w.answers.update({("172.22.12.5", 8123): 0}), "tcp 172.22.12.5:8123: connected"),
            ("a port nobody listed", lambda w: w.answers.update({("10.213.57.1", 7): 0}), "tcp 10.213.57.1:7: connected"),
            ("the 9P route", lambda w: w.vsock.update({(2, 50002): 0}), "port 50002, the 9P route that reached C:\\ from WSL: connected"),
            ("another Hyper-V socket", lambda w: w.vsock.update({(2, 2761): 0}), "port 2761, a Hyper-V socket: connected"),
            ("DNS on the gateway", lambda w: setattr(w, "udp", ("FAIL", "it answered")), "a DNS question to 10.213.57.1:53: it answered"),
        ):
            with self.subTest(name):
                self.plant_fails(plant, "FAIL", words)

    def test_something_of_the_hosts_on_a_mount_or_a_device_fails_the_run(self):
        def file_at(path, text=""):
            def plant(world):
                target = world.root / path
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text(text)

            return plant

        def mounted(fs_type):
            def plant(world):
                path = world.root / "proc/self/mountinfo"
                path.write_text(path.read_text() + f"30 20 0:9 / /x rw - {fs_type} C: rw\n")

            return plant

        for name, plant, words in (
            ("9p", mounted("9p"), "(9p)"),
            ("virtiofs", mounted("virtiofs"), "(virtiofs)"),
            ("cifs", mounted("cifs"), "(cifs)"),
            ("a FUSE mount", mounted("fuse.vmhgfs-fuse"), "fuse.vmhgfs-fuse"),
            ("a drive", file_at("mnt/c/file"), "/mnt holds c"),
            ("the GPU door", file_at("dev/dxg"), "/dev/dxg"),
            ("a render node", file_at("dev/dri/renderD128"), "renderD128"),
            ("a uio device", file_at("dev/uio0"), "uio0"),
            ("a VFIO group", file_at("dev/vfio/12"), "/dev/vfio holds 12"),
            ("the KVP device", file_at("dev/vmbus/hv_kvp"), "hv_kvp"),
            ("what KVP leaves", file_at("var/lib/hyperv/.kvp_pool_3"), "/var/lib/hyperv"),
            ("the seed's DVD left in", lambda w: (w.root / "sys/block/sr0").mkdir(), "sda sr0"),
            ("a second disk", lambda w: (w.root / "sys/block/sdb").mkdir(), "sda sdb"),
            ("a passed-through device", lambda w: (w.root / "sys/bus/pci/devices/pci-device-1").mkdir(), "pci-device-1"),
            ("nested virtualisation", lambda w: (w.root / "proc/cpuinfo").write_text("flags : fpu vmx\n"), "virtualisation"),
        ):
            with self.subTest(name):
                self.plant_fails(plant, "FAIL", words)

    def test_a_channel_to_the_host_on_vmbus_fails_the_run(self):
        for name, class_id, words in (
            ("data exchange", "a9a0f4e7-5a45-4d96-b827-8a841e8c03e6", "Data Exchange"),
            ("the host's file copy", "34d14be3-dee4-41c8-9ae7-6b174977c192", "file copy"),
            ("shutdown", "0e0b6031-5213-4934-818b-38d90ced39db", "Shutdown"),
            ("time", "9527e630-d0ae-497b-adce-e80ab0175caf", "Time Synchronization"),
            ("heartbeat", "57164f39-9115-4e78-ab55-382f3bd5422d", "Heartbeat"),
            ("vss", "35fa2e29-ea23-4236-96ae-3a6ebacba440", "VSS"),
            ("dynamic memory", "525074dc-8985-46e2-8057-a307dc18a502", "Dynamic Memory"),
            ("pass-through", "44c4f61d-4444-4400-9d52-802e27ede19f", "pass-through"),
            ("something new", "00000000-0000-0000-0000-000000000abc", "not a device this VM was given"),
        ):
            with self.subTest(name):
                self.plant_fails(lambda w, c=class_id: w.add_vmbus("extra", c), "FAIL", words)

    def test_secure_boot_off_fails_the_run(self):
        def plant(world):
            (world.root / guest.SECURE_BOOT_VARIABLE.lstrip("/")).write_bytes(b"\x06\x00\x00\x00\x00")

        self.plant_fails(plant, "FAIL", "Secure Boot is off")

    def test_what_could_not_run_fails_the_run_and_is_never_a_pass(self):
        def drop(path):
            def plant(world):
                target = world.root / path
                if target.is_dir():
                    shutil.rmtree(target)
                else:
                    target.unlink()

            return plant

        for name, plant, words in (
            ("DNS is down", lambda w: setattr(w, "dns_error", OSError("name resolution failed")), "DNS before the attacks"),
            ("DNS says something private", lambda w: setattr(w, "dns", ["192.168.1.1"]), "not a public answer"),
            ("HTTPS is down from the start", lambda w: setattr(w, "https", [OSError("no route"), "HTTP/1.1 200 OK"]), "HTTPS before the attacks"),
            ("HTTPS is not HTTP", lambda w: setattr(w, "https", ["something else", "HTTP/1.1 200 OK"]), "which is not HTTP"),
            ("the network goes partway through", lambda w: setattr(w, "https", ["HTTP/1.1 200 OK", OSError("no route")]), "HTTPS after the attacks"),
            ("the clock is hours out", lambda w: w.targets.update({"host_time": w.targets["host_time"] - 7200}), "from Windows'"),
            ("the gateway has no address", lambda w: (w.root / "proc/net/arp").write_text("IP HW Flags HWaddr Mask Device\n"), "no link-layer address"),
            ("the route is not the one aimed at", lambda w: (w.root / "proc/net/route").write_text("Iface Destination Gateway Flags\neth0 00000000 FE39D50A 0003\n"), "aimed wrong"),
            ("no default route", lambda w: (w.root / "proc/net/route").write_text("Iface Destination Gateway Flags\n"), "no default route"),
            ("the loopback control sees nothing", lambda w: setattr(w, "connect_many", lambda addresses, timeout, width: dict.fromkeys(addresses, guest.TIMED_OUT)), "a failure below would show nothing"),
            ("not a VM", drop("sys/class/dmi/id/sys_vendor"), "not a Hyper-V VM"),
            ("another maker's VM", lambda w: (w.root / "sys/class/dmi/id/sys_vendor").write_text("QEMU\n"), "not a Hyper-V VM"),
            ("no VMBus", drop("sys/bus/vmbus"), "not a Hyper-V VM"),
            ("no NIC on VMBus", drop("sys/bus/vmbus/devices/nic"), "cannot run without"),
            ("WSL", lambda w: (w.root / "run/WSL").mkdir(parents=True), "not a Hyper-V VM"),
            ("WSL's kernel", lambda w: (w.root / "proc/sys/kernel/osrelease").write_text("6.18.5-microsoft-standard-WSL2\n"), "not a Hyper-V VM"),
            ("the wrong uid for the level", lambda w: setattr(w, "uid", 0), "level user is running as uid 0"),
            ("Secure Boot cannot be read", drop(guest.SECURE_BOOT_VARIABLE.lstrip("/")), "Secure Boot setting"),
            ("mounts cannot be read", drop("proc/self/mountinfo"), "this check broke"),
            ("no LAN to attack", lambda w: w.targets.update({"lan": []}), "nothing to try"),
            ("no WSL to attack", lambda w: w.targets.update({"wsl": []}), "nothing to try"),
            ("no host ports", lambda w: w.targets.update({"host_ports": [], "loopback_ports": []}), "nothing to try"),
            ("no sweep", lambda w: w.targets.update({"sweep": [2, 1]}), "no port was tried"),
            ("an attack's own failure", lambda w: w.answers.update({("192.168.1.50", 445): errno.EMFILE}), "tcp 192.168.1.50:445: EMFILE"),
            ("vsock refused by something local", lambda w: w.vsock.update({(2, 50002): errno.EPERM}), "which shows nothing"),
        ):
            with self.subTest(name):
                self.plant_fails(plant, "UNRUN", words)

    def test_root_level_what_gets_through(self):
        def reply_to(world, target_ip, target_port, source_port, flags=0x12):
            ip = struct.pack("!BBHHHBBH4s4s", 0x45, 0, 40, 1, 0, 64, 6, 0, socket.inet_aton(target_ip), socket.inet_aton("10.213.57.2"))
            tcp = struct.pack("!HHIIBBHHH", target_port, source_port, 1, 1, 5 << 4, flags, 1000, 0, 0)
            world.frames.replies.append(b"\0" * 12 + b"\x08\x00" + ip + tcp)

        def ping_from(world, address):
            ip = struct.pack("!BBHHHBBH4s4s", 0x45, 0, 28, 1, 0, 64, 1, 0, socket.inet_aton(address), socket.inet_aton("10.213.57.2"))
            world.icmp4.replies.append((ip + struct.pack("!BBHHH", 0, 0, 0, os.getpid() & 0xFFFF, 1), (address, 0)))

        for name, plant, words in (
            ("a mount of the host's", lambda w: w.commands.update({"mount": (0, ""), "umount": (0, "")}), "it mounted"),
            ("a spoofed source port", lambda w: reply_to(w, "10.213.57.1", 445, 22), "a SYN to 10.213.57.1:445 from source port 22: answered"),
            ("a reset to a spoofed port", lambda w: reply_to(w, "10.213.57.1", 3389, 53, flags=0x14), "a SYN to 10.213.57.1:3389 from source port 53: answered"),
            ("Windows' loopback", lambda w: reply_to(w, "127.0.0.1", 8123, 40000), "Windows' 127.0.0.1:8123: answered"),
            ("Windows' loopback, from any address", lambda w: reply_to(w, "10.213.57.1", 8123, 40000), "Windows' 127.0.0.1:8123: answered"),
            ("a ping", lambda w: ping_from(w, "192.168.1.1"), "ping 192.168.1.1: it answered"),
            ("IPv6 on the link", lambda w: w.icmp6.replies.append((struct.pack("!BBHHH", 129, 0, 0, 1, 1), ("fe80::1%eth0", 0, 0, 2))), "answered by fe80::1"),
        ):
            with self.subTest(name):
                self.plant_fails(plant, "FAIL", words, "root")

    def test_root_level_what_could_not_run(self):
        for name, plant, words in (
            ("hv_sock would not load", lambda w: w.commands.update({"modprobe": (1, "not found")}), "not armed"),
            ("no AF_VSOCK for root", lambda w: w.vsock.update({(2, 50002): guest.NO_FAMILY}), "offers root no AF_VSOCK"),
            ("no loopback ports", lambda w: w.targets.update({"loopback_ports": []}), "nothing to try"),
            ("no address for the wire", lambda w: (w.root / "sys/class/net/eth0/address").unlink(), "this check broke"),
            ("the wrong uid", lambda w: setattr(w, "uid", 2001), "level root is running as uid 2001"),
        ):
            with self.subTest(name):
                self.plant_fails(plant, "UNRUN", words, "root")

    def test_its_own_answer_on_ipv6_does_not_count(self):
        with GuestWorld() as world:
            world.icmp6.replies.append((struct.pack("!BBHHH", 129, 0, 0, 1, 1), ("fe80::215:ff:fe00:2%eth0", 0, 0, 2)))
            code, printed = world.run("root")
            self.assertEqual(code, 0, printed)

    def test_a_check_that_breaks_is_an_unrun_and_the_others_still_run(self):
        with GuestWorld() as world, mock.patch.object(guest, "check_vmbus", side_effect=RuntimeError("surprise")):
            code, printed = world.run("user")
            self.assertEqual(code, 1)
            self.assert_line(printed, "UNRUN", "this check broke: RuntimeError: surprise")
            self.assert_line(printed, "PASS", "HTTPS after the attacks")

    def test_a_category_that_printed_nothing_is_an_unrun(self):
        with GuestWorld() as world:
            world_run = world.run
            with mock.patch.object(guest, "check_lan", lambda run: None):
                code, printed = world_run("user")
            self.assertEqual(code, 1, printed)
            self.assert_line(printed, "UNRUN", "lan")

    def test_no_pass_at_all_is_not_a_pass(self):
        report = guest.Report("user", False)
        with contextlib.redirect_stdout(io.StringIO()):
            for category in guest.CATEGORIES["user"]:
                report.seen.add(category)
            self.assertEqual(report.finish(), 1)

    def test_its_exit_status_is_zero_only_when_every_line_passed(self):
        for status in ("FAIL", "UNRUN"):
            report = guest.Report("user", False)
            with contextlib.redirect_stdout(io.StringIO()):
                for category in guest.CATEGORIES["user"]:
                    report.line("PASS", category, "x")
                report.line(status, "lan", "y")
                self.assertEqual(report.finish(), 1, status)

    def test_a_clock_put_right_by_ntp_while_it_waits_is_a_pass(self):
        # The fix for a real fault: with the wall clock on both sides the drift cancels, and a correction would never show.
        report = guest.Report("user", True)
        now = 1_800_000_000.0
        wall = [now - 7200, now - 7200, now + 5]
        with mock.patch.object(guest.time, "time", lambda: wall.pop(0) if len(wall) > 1 else wall[0]), mock.patch.object(guest.time, "sleep", lambda s: None), mock.patch.object(guest, "CLOCK_WAIT", 60.0), contextlib.redirect_stdout(io.StringIO()) as out:
            run = guest.Run("user", {"host_time": now}, report)
            guest.check_control_clock(run)
        self.assertIn("within", out.getvalue())
        self.assertEqual(report.counts["PASS"], 1, out.getvalue())


class TheFrames(unittest.TestCase):
    def test_a_syn_frame_has_right_checksums_and_addresses_the_kernel_would_never_send(self):
        frame = guest.syn_frame(b"\x02" * 6, b"\x04" * 6, "10.213.57.2", "127.0.0.1", 40000, 8123)
        self.assertEqual(frame[0:6], b"\x04" * 6, "to the gateway's address on the wire")
        self.assertEqual(frame[6:12], b"\x02" * 6)
        self.assertEqual(frame[12:14], b"\x08\x00")
        ip, tcp = frame[14:34], frame[34:54]
        self.assertEqual(guest.checksum(ip), 0)
        pseudo = ip[12:20] + struct.pack("!BBH", 0, 6, 20)
        self.assertEqual(guest.checksum(pseudo + tcp), 0)
        self.assertEqual(socket.inet_ntoa(ip[16:20]), "127.0.0.1")
        self.assertEqual(struct.unpack("!HH", tcp[:4]), (40000, 8123))
        self.assertEqual(tcp[13], 0x02, "SYN")

    def test_the_checksum_on_the_rfc_1071_example(self):
        self.assertEqual(guest.checksum(bytes.fromhex("0001f203f4f5f6f7")), ~0xDDF2 & 0xFFFF)

    def test_a_reply_is_read_and_anything_else_is_not(self):
        frame = guest.syn_frame(b"\x02" * 6, b"\x04" * 6, "10.213.57.1", "10.213.57.2", 445, 22)
        self.assertEqual(guest.tcp_reply(frame, "10.213.57.2"), ("10.213.57.1", 445, 22, 0x02))
        self.assertIsNone(guest.tcp_reply(frame, "10.213.57.9"), "to someone else")
        self.assertIsNone(guest.tcp_reply(frame[:20], "10.213.57.2"), "too short")
        self.assertIsNone(guest.tcp_reply(b"\0" * 12 + b"\x86\xdd" + frame[14:], "10.213.57.2"), "not IPv4")

    def test_a_dns_question_is_well_formed(self):
        query = guest.dns_query("github.com")
        self.assertEqual(struct.unpack("!HHHHHH", query[:12])[2], 1)
        self.assertEqual(query[12:24], b"\x06github\x03com\x00")
        self.assertEqual(query[24:], struct.pack("!HH", 1, 1))


class RealSockets(unittest.TestCase):
    """connect_many, against sockets of this test's own, on this machine's loopback. It knocks on nothing else's door."""

    # Windows' loopback takes about two seconds to say refused.
    TIMEOUT = 5.0

    def test_a_listener_connects_and_a_closed_port_is_refused(self):
        with socket.socket() as listener, socket.socket() as closed:
            listener.bind(("127.0.0.1", 0))
            listener.listen(1)
            closed.bind(("127.0.0.1", 0))
            open_address, closed_address = listener.getsockname(), closed.getsockname()
            results = guest.connect_many([open_address, closed_address], self.TIMEOUT, 4)
        self.assertEqual(results[open_address], 0)
        self.assertEqual(guest.judge_tcp(results[closed_address], self.TIMEOUT)[0], "FAIL", f"it read a refusal as {results[closed_address]}")

    def test_more_than_its_width_all_get_an_answer(self):
        listeners = [socket.socket() for _ in range(7)]
        try:
            for listener in listeners:
                listener.bind(("127.0.0.1", 0))
                listener.listen(1)
            many = [listener.getsockname() for listener in listeners]
            results = guest.connect_many(many, self.TIMEOUT, 2)
        finally:
            for listener in listeners:
                listener.close()
        self.assertEqual(results, dict.fromkeys(many, 0))

    def test_an_address_with_nothing_there_is_shut(self):
        # TEST-NET-1, reserved for documents: one SYN to nowhere, whichever way it is refused.
        results = guest.connect_many([("192.0.2.1", 9)], 0.3, 1)
        self.assertEqual(guest.judge_tcp(results[("192.0.2.1", 9)], 0.3)[0], "PASS", str(results))

    def test_the_control_passes_only_when_a_connection_is_seen(self):
        report = guest.Report("user", True)
        with contextlib.redirect_stdout(io.StringIO()):
            guest.check_control_loopback(guest.Run("user", {}, report))
        self.assertEqual(report.counts["PASS"], 1)


if __name__ == "__main__":
    unittest.main()

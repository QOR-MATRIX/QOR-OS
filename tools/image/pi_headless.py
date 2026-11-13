#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Public-only Q Pi enrolment receipts and offline Windows SSH plans.

Never generates/reads private keys, processes provisioning credentials, executes
SSH, discovers devices, enables services or accesses block devices. publish_receipt writes
one public file in an explicitly selected existing directory; only temporary
ordinary fixture directories are exercised in this preparation.
"""

import argparse
import base64
import binascii
import hashlib
import ipaddress
import json
import os
from pathlib import Path, PureWindowsPath
import re
import stat
import struct
import uuid

MAX_INPUT = 8192
RECEIPT_NAME = "qor-headless-host.json"
FIELDS = {"schema", "kind", "instance_id", "hostname", "image_sha256", "host_key", "host_fingerprint"}
SSH = "C:/Windows/System32/OpenSSH/ssh.exe"
PROBE = "uname -m && getconf PAGE_SIZE && id -u"
PRIVATE_NETWORKS = tuple(ipaddress.IPv4Network(net) for net in
                         ("10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16"))


def unique_fields(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate metadata field")
        result[key] = value
    return result


def parse_json(raw):
    if len(raw) > MAX_INPUT:
        raise ValueError("metadata exceeds its bound")
    try:
        value = json.loads(raw, object_pairs_hook=unique_fields)
    except (UnicodeError, RecursionError, json.JSONDecodeError) as error:
        raise ValueError("invalid metadata encoding or structure") from error
    if not isinstance(value, dict) or any(type(item) not in (str, int) for item in value.values()):
        raise ValueError("receipt must be a flat metadata object")
    return value


def read_public(path):
    """Read only an explicitly selected regular, non-symlink public input."""
    path = Path(path)
    before = path.lstat()
    if (not stat.S_ISREG(before.st_mode) or before.st_size > MAX_INPUT
            or getattr(before, "st_file_attributes", 0) & 0x400):  # Windows reparse point
        raise ValueError("public input must be a bounded regular file")
    with path.open("rb") as stream:
        opened = os.fstat(stream.fileno())
        if not stat.S_ISREG(opened.st_mode) or (before.st_dev, before.st_ino) != (opened.st_dev, opened.st_ino):
            raise ValueError("public input changed during observation")
        raw = stream.read(MAX_INPUT + 1)
        after = os.fstat(stream.fileno())
    if (opened.st_size, opened.st_mtime_ns) != (after.st_size, after.st_mtime_ns) or len(raw) > MAX_INPUT:
        raise ValueError("public input changed or exceeds its bound")
    return raw


def identity_fields(instance_id, hostname, image_sha256):
    if not isinstance(instance_id, str):
        raise ValueError("invalid instance identity")
    try:
        identity = uuid.UUID(instance_id)
    except ValueError as error:
        raise ValueError("invalid instance identity") from error
    if str(identity) != instance_id or identity.version != 4 or identity.variant != uuid.RFC_4122:
        raise ValueError("expected canonical per-device UUIDv4")
    if not isinstance(hostname, str) or not re.fullmatch(r"qpi-[a-z0-9](?:[a-z0-9-]{0,54}[a-z0-9])?", hostname):
        raise ValueError("invalid per-device qpi hostname")
    if not isinstance(image_sha256, str) or not re.fullmatch(r"[0-9a-f]{64}", image_sha256):
        raise ValueError("invalid image digest")


def public_key(raw):
    """Inspect an OpenSSH Ed25519 public blob; no key generation or signing."""
    if not isinstance(raw, str) or len(raw) > 1024:
        raise ValueError("invalid public host key")
    # A .pub comment may contain an account/email; discard it and never emit it.
    line = raw.rstrip("\r\n")
    if any(ord(char) < 32 or ord(char) > 126 for char in line):
        raise ValueError("invalid public host key")
    parts = line.split(" ", 2)
    if len(parts) < 2 or parts[0] != "ssh-ed25519":
        raise ValueError("expected one Ed25519 public host key")
    try:
        blob = base64.b64decode(parts[1], validate=True)
    except (ValueError, binascii.Error) as error:
        raise ValueError("invalid public host key encoding") from error
    prefix = struct.pack(">I", 11) + b"ssh-ed25519" + struct.pack(">I", 32)
    if len(blob) != 51 or not blob.startswith(prefix) or blob[-32:] == bytes(32):
        raise ValueError("invalid public host key structure")
    encoded = base64.b64encode(blob).decode("ascii")
    if parts[1] != encoded:
        raise ValueError("noncanonical public host key encoding")
    fingerprint = "SHA256:" + base64.b64encode(hashlib.sha256(blob).digest()).decode("ascii").rstrip("=")
    return "ssh-ed25519 " + encoded, fingerprint


def make_receipt(instance_id, hostname, image_sha256, key):
    identity_fields(instance_id, hostname, image_sha256)
    canonical, fingerprint = public_key(key)
    return {"schema": 1, "kind": "q-pi-public-host-identity", "instance_id": instance_id,
            "hostname": hostname, "image_sha256": image_sha256,
            "host_key": canonical, "host_fingerprint": fingerprint}


def validate_receipt(value):
    if not isinstance(value, dict) or set(value) != FIELDS:
        raise ValueError("unexpected public receipt fields")
    if type(value["schema"]) is not int or value["schema"] != 1 or value["kind"] != "q-pi-public-host-identity":
        raise ValueError("unsupported public receipt schema")
    rebuilt = make_receipt(value["instance_id"], value["hostname"], value["image_sha256"], value["host_key"])
    if rebuilt != value:
        raise ValueError("public receipt is not canonical or fingerprint mismatches")
    return rebuilt


def verify_receipt(value, *, instance_id, image_sha256, fingerprint=None):
    value = validate_receipt(value)
    if value["instance_id"] != instance_id or value["image_sha256"] != image_sha256:
        raise ValueError("receipt does not match the independently retained write record")
    if fingerprint is not None and value["host_fingerprint"] != fingerprint:
        raise ValueError("host identity changed; automatic replacement refused")
    return value


def publish_receipt(directory, value):
    """Create once, or accept an identical receipt; never replace/repair one.

    Used only in a trusted, quiescent directory. Exclusive creation prevents
    overwriting any existing entry. Power loss may leave a partial file, which
    is deliberately refused on retry. This is not an atomic/FAT durability or
    boot-completion claim. The native hook and safe shutdown remain unqualified.
    """
    value = validate_receipt(value)
    directory = Path(directory)
    info = directory.lstat()
    if not stat.S_ISDIR(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400:
        raise ValueError("receipt destination must be an existing real directory")
    destination = directory / RECEIPT_NAME
    raw = (json.dumps(value, sort_keys=True, indent=2) + "\n").encode("ascii")
    try:
        stream = destination.open("xb")
    except FileExistsError:
        existing = validate_receipt(parse_json(read_public(destination)))
        if existing != value:
            raise ValueError("existing identity differs; replacement refused")
        return "unchanged"
    with stream:
        stream.write(raw)
        stream.flush()
        os.fsync(stream.fileno())
    return "created"


def windows_path(value):
    if not isinstance(value, str) or len(value) > 240 or not re.fullmatch(r"[A-Za-z]:/[A-Za-z0-9_ ./-]+", value):
        raise ValueError("expected a literal absolute Windows file path")
    parts = value[3:].split("/")
    reserved = {"con", "prn", "aux", "nul", *(f"com{i}" for i in range(1, 10)), *(f"lpt{i}" for i in range(1, 10))}
    if any(not part or part in (".", "..") or part.endswith((" ", "."))
           or part.split(".", 1)[0].lower() in reserved for part in parts):
        raise ValueError("unsafe Windows file path")
    return value


def connection_plan(receipt, address, user, identity_file, known_hosts_file):
    """Produce argv, never execute it. Caller supplies separately verified receipt."""
    receipt = validate_receipt(receipt)
    if not isinstance(address, str):
        raise ValueError("expected one approved private IPv4 address")
    try:
        target = ipaddress.IPv4Address(address)
    except ipaddress.AddressValueError as error:
        raise ValueError("expected one approved private IPv4 address") from error
    if str(target) != address or not any(target in network for network in PRIVATE_NETWORKS):
        raise ValueError("expected one approved private IPv4 address")
    if not isinstance(user, str) or not re.fullmatch(r"[a-z][a-z0-9_-]{0,30}", user) or user in ("root", "pi"):
        raise ValueError("expected a selected non-default ordinary account")
    identity_file, known_hosts_file = windows_path(identity_file), windows_path(known_hosts_file)
    if PureWindowsPath(identity_file) == PureWindowsPath(known_hosts_file):
        raise ValueError("identity and public pin paths must differ")
    alias = "qpi-" + receipt["instance_id"]
    options = ["BatchMode=yes", "StrictHostKeyChecking=yes", "CheckHostIP=no", "VerifyHostKeyDNS=no",
               "UpdateHostKeys=no", "HostKeyAlgorithms=ssh-ed25519", "HostKeyAlias=" + alias,
               'UserKnownHostsFile="' + known_hosts_file + '"', "GlobalKnownHostsFile=NUL",
               'IdentityFile="' + identity_file + '"', "IdentityAgent=none", "IdentitiesOnly=yes",
               "PreferredAuthentications=publickey", "PubkeyAuthentication=yes", "PasswordAuthentication=no",
               "KbdInteractiveAuthentication=no", "GSSAPIAuthentication=no", "HostbasedAuthentication=no",
               "NumberOfPasswordPrompts=0", "ProxyCommand=none", "ProxyJump=none", "PermitLocalCommand=no",
               "ClearAllForwardings=yes", "ForwardAgent=no", "ForwardX11=no", "Tunnel=no",
               "ControlMaster=no", "ControlPath=none", "ControlPersist=no", "RequestTTY=no",
               "ConnectTimeout=10", "ConnectionAttempts=1", "ServerAliveInterval=5", "ServerAliveCountMax=2",
               "CanonicalizeHostname=no"]
    argv = [SSH, "-F", "none", "-4", "-T", "-n", "-p", "22"]
    for option in options:
        argv.extend(("-o", option))
    argv.extend(("-l", user, address, PROBE))
    return {"kind": "offline-ssh-plan-not-access-authorisation", "argv": argv,
            "known_hosts_line": alias + " " + receipt["host_key"] + "\n",
            "required_wall_timeout_seconds": 30, "automatic_retries": 0,
            "network_used": False, "authenticated_access_verified": False,
            "requires": ["action-time account/key/endpoint/scope approval", "trusted physical receipt provenance",
                         "exact public pin file content and protected private-key storage",
                         "approved credential usable without prompts; no agent is enabled by this plan",
                         "native server login/expiry policy and effective configuration verification"]}


def verify_probe_output(exit_code, raw):
    """Check only a successful scoped identity probe, not boot/hardware acceptance."""
    if type(exit_code) is not int or exit_code != 0 or not isinstance(raw, bytes) or len(raw) > 1024:
        raise ValueError("identity probe failed or exceeded its bound")
    try:
        lines = raw.decode("ascii").splitlines()
    except UnicodeError as error:
        raise ValueError("invalid identity probe output") from error
    if (len(lines) != 3 or lines[0] not in ("aarch64", "arm64") or lines[1] != "16384"
            or not re.fullmatch(r"[1-9][0-9]{0,9}", lines[2]) or int(lines[2]) >= 2**32):
        raise ValueError("identity probe did not report ARM64/16KiB/non-root execution")
    return {"architecture": lines[0], "page_size": 16384, "uid": int(lines[2]),
            "scope": "probe output only; transport authentication must be checked separately",
            "native_build_qualified": False}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--receipt", type=Path, required=True, help="Explicit public-only JSON receipt")
    parser.add_argument("--instance-id", required=True, help="Independently retained per-device UUIDv4")
    parser.add_argument("--image-sha256", required=True, help="Independently retained written image digest")
    parser.add_argument("--fingerprint", help="Previously pinned public host fingerprint, if enrolled")
    parser.add_argument("--address", help="One already identified/approved private IPv4 endpoint; never resolved here")
    parser.add_argument("--user", help="Selected ordinary local account")
    parser.add_argument("--identity-file", help="Literal Windows credential path; never opened by this tool")
    parser.add_argument("--known-hosts-file", help="Literal Windows public pin path; never written by this tool")
    args = parser.parse_args(argv)
    connection_values = (args.address, args.user, args.identity_file, args.known_hosts_file)
    if any(value is not None for value in connection_values) and (
            not all(value is not None for value in connection_values) or args.fingerprint is None):
        parser.error("an offline SSH plan requires all four connection fields and the independently pinned fingerprint")
    try:
        value = verify_receipt(parse_json(read_public(args.receipt)), instance_id=args.instance_id,
                               image_sha256=args.image_sha256, fingerprint=args.fingerprint)
        plan = connection_plan(value, *connection_values) if args.address is not None else None
    except (OSError, ValueError, TypeError):
        # Input may accidentally contain private data; do not echo it or errors.
        print(json.dumps({"kind": "public-host-receipt-check", "passed": False,
                          "error": "public receipt invalid, changed or inconsistent",
                          "network_used": False, "authenticated_access_verified": False}))
        return 2
    if plan is not None:
        print(json.dumps(plan, indent=2))
        return 0
    print(json.dumps({"kind": "public-host-receipt-check", "passed": True,
                      "host_fingerprint": value["host_fingerprint"], "network_used": False,
                      "authenticated_access_verified": False,
                      "not_proved": ["receipt acquisition trust", "image provenance", "native first-boot completion",
                                     "host-key persistence", "Wi-Fi", "SSH access", "login/autologin policy"]}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

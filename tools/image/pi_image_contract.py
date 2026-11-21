# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Unwired Pi image declaration validator; not a production profile or image probe.

The draft metadata contract is exactly schema=1, profile=<caller policy>, and
architecture="arm64". No profile name is supplied here. Prism's independent
probe output uses its existing schema/development/authentication contract.
Passing validates declarations only: provenance, freshness, actual image/ELF
architecture, Pi model/firmware and a real authenticated session remain unproved.
This module performs no I/O and is not called by image assembly or check_image.
"""
import json
import re

MAX_CONTRACT_BYTES = 8192


def _unique_object(pairs):
    result = {}
    for name, value in pairs:
        if name in result:
            raise ValueError("Duplicate contract field")
        result[name] = value
    return result


def _reject_constant(_):
    raise ValueError("Non-JSON constant")


def _read_object(raw):
    if type(raw) is not bytes or not 0 < len(raw) <= MAX_CONTRACT_BYTES:
        raise ValueError("Missing or oversized contract")
    value = json.loads(raw.decode("utf-8"), object_pairs_hook=_unique_object,
                       parse_constant=_reject_constant)
    if type(value) is not dict:
        raise ValueError("Contract must be an object")
    return value


def validate_pi_image_contract(metadata, prism_capabilities, *, opt_in,
                               expected_profile, prism_exit_code):
    """Return fixed errors, or []; callers must obtain trusted evidence themselves.

    Opt-in must be the boolean True. expected_profile is explicit caller policy,
    not metadata-derived. Generic/x86 callers must retain their existing path.
    No subprocess is run, and neither input nor global state is modified.
    """
    if opt_in is not True:
        return ["Pi image validation requires explicit opt-in"]
    if (type(expected_profile) is not str
            or re.fullmatch(r"[a-z0-9][a-z0-9._-]{0,63}", expected_profile) is None):
        return ["An explicit expected Pi profile is required"]
    try:
        image = _read_object(metadata)
    except (ValueError, RecursionError):
        return ["Invalid Pi image metadata"]
    if (set(image) != {"schema", "profile", "architecture"}
            or type(image.get("schema")) is not int or image["schema"] != 1):
        return ["Unknown or incomplete Pi image metadata contract"]
    if image["profile"] != expected_profile:
        return ["Pi image profile does not match explicit policy"]
    if image["architecture"] != "arm64":
        return ["Pi image architecture must be unambiguous arm64"]
    if type(prism_exit_code) is not int or prism_exit_code != 0:
        return ["A successful independent Prism capability probe is required"]
    try:
        prism = _read_object(prism_capabilities)
    except (ValueError, RecursionError):
        return ["Invalid Prism capability evidence"]
    if (set(prism) != {"schema", "development", "authentication"}
            or type(prism.get("schema")) is not int or prism["schema"] != 1
            or prism.get("development") is not False or prism.get("authentication") is not True):
        return ["Prism lacks a non-development authenticated contract"]
    return []

# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Bounded, non-executing ELF observations for Q Pi payload qualification.

This observes GNU version labels, not symbol binding or full ABI compatibility.
Dynamic fields follow https://gabi.xinuos.com/elf/08-dynamic.html; GNU version
layouts and label matching follow glibc's elf/elf.h and elf/dl-version.c.
Unsupported or ambiguous layouts fail without consulting the host's libraries.
"""

from bisect import bisect_left
import hashlib
import os
from pathlib import Path
import stat
import struct

from pi_preflight import MAX_READ, UINT64_LIMIT, inspect_elf

MAX_FILE_BYTES = 512 * 1024 * 1024
MAX_DYNAMIC_ENTRIES = 4096
MAX_STRING_TABLE_BYTES = 8 * 1024 * 1024
MAX_STRING_BYTES = 4096
MAX_METADATA_BYTES = 16 * 1024 * 1024
MAX_NEEDED = 256
MAX_EMITTED_STRING_BYTES = 64 * 1024
MAX_VERSION_NODES = 4096
DT_VERDEF, DT_VERDEFNUM = 0x6FFFFFFC, 0x6FFFFFFD
DT_VERNEED, DT_VERNEEDNUM = 0x6FFFFFFE, 0x6FFFFFFF


def _identity(info):
    # Windows path stat and descriptor fstat can expose different ctime
    # semantics. Compare ctime separately between observations of the same fd.
    return (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns)


class _Reader:
    def __init__(self, stream, size):
        self.stream = stream
        self.size = size
        self.read_bytes = 0

    def read(self, offset, size):
        if (type(offset) is not int or type(size) is not int or offset < 0 or size < 0
                or offset + size > self.size or offset + size >= UINT64_LIMIT
                or self.read_bytes + size > MAX_METADATA_BYTES):
            raise ValueError("ELF metadata read exceeds its bounds")
        self.read_bytes += size
        self.stream.seek(offset)
        data = self.stream.read(size)
        if len(data) != size:
            raise ValueError("ELF metadata changed or is truncated")
        return data


def _file_offset(loads, address, size):
    if not size or address + size >= UINT64_LIMIT:
        raise ValueError("Invalid ELF virtual metadata range")
    # A partially overlapping mapping is ambiguous too. Zero-filled memory is
    # never treated as file data, even when another load maps the same address.
    overlaps = [row for row in loads
                if address < row[3] + row[6] and row[3] < address + size]
    if len(overlaps) != 1:
        raise ValueError("Unmapped or ambiguous ELF metadata address")
    row = overlaps[0]
    if not row[3] <= address or address + size > row[3] + row[5]:
        raise ValueError("ELF metadata is not wholly file-backed")
    return row[2] + address - row[3]


def _text(raw, *, allow_empty=False):
    if len(raw) > MAX_STRING_BYTES or (not raw and not allow_empty):
        raise ValueError("Invalid ELF string length")
    try:
        value = raw.decode("utf-8")
    except UnicodeDecodeError as error:
        raise ValueError("ELF string is not UTF-8") from error
    if any(not character.isprintable() for character in value):
        raise ValueError("ELF string contains control characters")
    return value


def _string(table, index, *, allow_empty=False):
    if not 0 <= index < len(table):
        raise ValueError("ELF string offset outside its table")
    end = table.find(b"\0", index, min(len(table), index + MAX_STRING_BYTES + 1))
    if end < 0:
        raise ValueError("Unterminated or oversized ELF string")
    return _text(table[index:end], allow_empty=allow_empty)


def _elf_hash(name):
    """System V ELF string hash carried in GNU version records, not GNU_HASH."""
    value = 0
    for byte in name.encode("utf-8"):
        value = ((value << 4) + byte) & 0xFFFFFFFF
        high = value & 0xF0000000
        value = (value ^ (high >> 24)) & ~high
    return value


def _versions(reader, loads, fields, table, emit, needed, reserved):
    """Read label tables only; DT_VERSYM and per-symbol binding remain unchecked."""
    regions = sorted(reserved)
    if any(left[1] > right[0] for left, right in zip(regions, regions[1:])):
        raise ValueError("Overlapping ELF dynamic and string metadata")
    nodes = 0
    indices = set()

    def node(address, layout):
        nonlocal nodes
        nodes += 1
        if nodes > MAX_VERSION_NODES or address % 4:
            raise ValueError("ELF version metadata exceeds node bound or alignment")
        size = struct.calcsize(layout)
        offset = _file_offset(loads, address, size)
        end = offset + size
        position = bisect_left(regions, (offset, end))
        if ((position and regions[position - 1][1] > offset)
                or (position < len(regions) and regions[position][0] < end)):
            raise ValueError("Overlapping ELF version metadata")
        regions.insert(position, (offset, end))
        return struct.unpack(layout, reader.read(offset, size))

    def chain(address, count, layout):
        if not address or not 1 <= count <= MAX_VERSION_NODES:
            raise ValueError("Invalid ELF version chain count or address")
        size = struct.calcsize(layout)
        for index in range(count):
            row = node(address, layout)
            step = row[-1]
            if index == count - 1:
                if step:
                    raise ValueError("ELF version chain exceeds declared count")
            elif step < size or step % 4:
                raise ValueError("ELF version chain ends early or overlaps")
            yield address, row
            address += step

    def label(index, expected_hash=None):
        value = emit(_string(table, index))
        if expected_hash is not None and _elf_hash(value) != expected_hash:
            raise ValueError("ELF version label hash disagrees with its name")
        return value

    def version_index(raw, *, base=False):
        index = raw & 0x7FFF
        if index in indices or (index != 1 if base else index < 2):
            raise ValueError("Invalid or duplicate ELF version index")
        indices.add(index)
        return raw

    for pointer, count in ((DT_VERDEF, DT_VERDEFNUM), (DT_VERNEED, DT_VERNEEDNUM)):
        if (pointer in fields) != (count in fields):
            raise ValueError("Incomplete ELF version pointer/count pair")

    definitions, requirements = [], []
    names = set()
    if DT_VERDEF in fields:
        for address, row in chain(fields[DT_VERDEF], fields[DT_VERDEFNUM], "<HHHHIII"):
            revision, flags, index, count, digest, aux, _ = row
            if revision != 1 or flags & ~3 or aux < 20 or aux % 4:
                raise ValueError("Unsupported ELF version definition")
            labels = [label(item[0], digest if number == 0 else None)
                      for number, (_, item) in enumerate(chain(address + aux, count, "<II"))]
            if len(set(labels)) != len(labels) or labels[0] in names:
                raise ValueError("Duplicate ELF version definition or parent")
            names.add(labels[0])
            definitions.append({"name": labels[0], "hash": digest,
                                "index": version_index(index, base=bool(flags & 1)),
                                "base": bool(flags & 1), "weak": bool(flags & 2),
                                "parents": labels[1:]})

    libraries = set()
    if DT_VERNEED in fields:
        if fields[DT_VERNEEDNUM] > MAX_NEEDED:
            raise ValueError("Too many ELF version requirement libraries")
        for address, row in chain(fields[DT_VERNEED], fields[DT_VERNEEDNUM], "<HHIII"):
            revision, count, filename, aux, _ = row
            if revision != 1 or aux < 16 or aux % 4:
                raise ValueError("Unsupported ELF version requirement")
            filename = label(filename)
            if filename not in needed or filename in libraries:
                raise ValueError("ELF version library is not a unique DT_NEEDED name")
            libraries.add(filename)
            versions, names = [], set()
            for _, item in chain(address + aux, count, "<IHHII"):
                digest, flags, index, name, _ = item
                if flags & ~2:
                    raise ValueError("Unsupported ELF version requirement flags")
                name = label(name, digest)
                if name in names:
                    raise ValueError("Duplicate ELF required version label")
                names.add(name)
                versions.append({"name": name, "hash": digest,
                                 "index": version_index(index), "weak": bool(flags & 2)})
            requirements.append({"file": filename, "versions": versions})
    return definitions, requirements


def _metadata(reader):
    prefix = reader.read(0, min(reader.size, MAX_READ))
    result = inspect_elf(prefix, reader.size)
    if prefix[7] not in (0, 3):
        raise ValueError("Unsupported ELF OS ABI identifier")
    result["osabi"] = prefix[7]
    phoff = struct.unpack_from("<Q", prefix, 32)[0]
    phnum = struct.unpack_from("<H", prefix, 56)[0]
    rows = [struct.unpack_from("<IIQQQQQQ", prefix, phoff + index * 56)
            for index in range(phnum)]
    loads = [row for row in rows if row[0] == 1]
    dynamic = [row for row in rows if row[0] == 2]
    interpreters = [row for row in rows if row[0] == 3]
    if len(dynamic) > 1 or len(interpreters) > 1:
        raise ValueError("Duplicate ELF dynamic or interpreter segment")
    result.update(needed=[], soname=None, interpreter=None, rpath=None, runpath=None,
                  dynamic=bool(dynamic), version_definitions=[], version_needs=[])
    emitted_bytes = 0

    def emit(value):
        nonlocal emitted_bytes
        emitted_bytes += len(value.encode("utf-8"))
        if emitted_bytes > MAX_EMITTED_STRING_BYTES:
            raise ValueError("ELF string observations exceed output bound")
        return value

    if interpreters:
        row = interpreters[0]
        if not 2 <= row[5] <= MAX_STRING_BYTES + 1:
            raise ValueError("Invalid ELF interpreter size")
        raw = reader.read(row[2], row[5])
        if not raw.endswith(b"\0") or b"\0" in raw[:-1]:
            raise ValueError("Invalid ELF interpreter terminator")
        result["interpreter"] = emit(_text(raw[:-1]))
        if not dynamic:
            raise ValueError("ELF interpreter has no dynamic segment")
    if not dynamic:
        return result

    row = dynamic[0]
    if (not row[5] or row[5] % 16 or row[5] > MAX_DYNAMIC_ENTRIES * 16
            or row[5] > row[6] or row[3] + row[6] >= UINT64_LIMIT):
        raise ValueError("Invalid or oversized ELF dynamic segment")
    if _file_offset(loads, row[3], row[5]) != row[2]:
        raise ValueError("ELF dynamic file and virtual addresses disagree")
    data = reader.read(row[2], row[5])
    fields = {}
    needed = []
    ended = False
    for tag, value in struct.iter_unpack("<qQ", data):
        if tag == 0:
            ended = True
            continue
        if ended:
            raise ValueError("ELF dynamic entries follow the terminator")
        if tag == 1:
            needed.append(value)
            if len(needed) > MAX_NEEDED:
                raise ValueError("Too many ELF needed libraries")
        elif tag in (5, 10, 14, 15, 29, DT_VERDEF, DT_VERDEFNUM, DT_VERNEED, DT_VERNEEDNUM):
            if tag in fields:
                raise ValueError("Duplicate ELF dynamic metadata field")
            fields[tag] = value
    if not ended:
        raise ValueError("ELF dynamic segment lacks a terminator")
    if 5 not in fields or 10 not in fields or not 1 <= fields[10] <= MAX_STRING_TABLE_BYTES:
        raise ValueError("Missing or oversized ELF dynamic string table")
    string_offset = _file_offset(loads, fields[5], fields[10])
    table = reader.read(string_offset, fields[10])
    if table[0] or table[-1]:
        raise ValueError("ELF string table lacks boundary terminators")
    result["needed"] = [emit(_string(table, index)) for index in needed]
    if len(set(result["needed"])) != len(result["needed"]):
        raise ValueError("Duplicate ELF needed library")
    for tag, name in ((14, "soname"), (15, "rpath"), (29, "runpath")):
        if tag in fields:
            result[name] = emit(_string(table, fields[tag], allow_empty=tag in (15, 29)))
    result["version_definitions"], result["version_needs"] = _versions(
        reader, loads, fields, table, emit, result["needed"],
        [(row[2], row[2] + row[5]), (string_offset, string_offset + fields[10])])
    return result


def inspect_file(path, maximum_bytes=MAX_FILE_BYTES):
    """Observe one regular ARM64 file without executing it or resolving libraries.

    Read at most 16 MiB of metadata and hash at most 512 MiB in 1-MiB chunks.
    Visit at most 4,096 version nodes. Emit at most 256 needed names and 64 KiB
    of strings including repeated version names and parents.
    File identity/size/timestamps must remain stable through both operations.
    This detects ordinary concurrent mutation, not an adversarial writer able to
    restore metadata; callers must supply a quiescent, trusted staging tree.
    """
    if type(maximum_bytes) is not int or not 64 <= maximum_bytes <= MAX_FILE_BYTES:
        raise ValueError("Invalid ELF file byte budget")
    path = Path(path)
    before = path.stat()
    if not stat.S_ISREG(before.st_mode) or not 64 <= before.st_size <= maximum_bytes:
        raise ValueError("ELF input must be a bounded regular file")
    flags = os.O_RDONLY | getattr(os, "O_BINARY", 0) | getattr(os, "O_NONBLOCK", 0)
    descriptor = os.open(path, flags)
    with os.fdopen(descriptor, "rb") as stream:
        opened = os.fstat(stream.fileno())
        if not stat.S_ISREG(opened.st_mode) or _identity(before) != _identity(opened):
            raise ValueError("ELF input changed while opening")
        result = _metadata(_Reader(stream, opened.st_size))
        stream.seek(0)
        digest = hashlib.sha256()
        remaining = opened.st_size
        while remaining:
            data = stream.read(min(MAX_READ, remaining))
            if not data:
                raise ValueError("ELF input changed while hashing")
            digest.update(data)
            remaining -= len(data)
        extra = stream.read(1)
        final = os.fstat(stream.fileno())
        if (extra or _identity(final) != _identity(opened)
                or final.st_ctime_ns != opened.st_ctime_ns):
            raise ValueError("ELF input changed during inspection")
        if _identity(path.stat()) != _identity(opened):
            raise ValueError("ELF input path changed during inspection")
        return {**result, "sha256": digest.hexdigest(), "file_size": opened.st_size}

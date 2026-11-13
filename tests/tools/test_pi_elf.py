# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Hostile static ELF observations; fixtures never execute ARM64 code."""

import hashlib
import os
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/image"))
import pi_elf as elf

PAGE = 16384
DYNAMIC = 512
INTERP = 320


def fixture(*, needed=("libc.so.6",), soname="libfixture.so", interpreter="/lib/ld-linux-aarch64.so.1",
            rpath=None, runpath="$ORIGIN", second_offset=PAGE):
    """Two load segments with a nonidentity virtual/file mapping for strings."""
    strings = bytearray(b"\0")
    entries = []
    for tag, values in ((1, needed), (14, (soname,)), (15, (rpath,)), (29, (runpath,))):
        for value in values:
            if value is None:
                continue
            entries.append((tag, len(strings)))
            strings.extend(value if isinstance(value, bytes) else value.encode("utf-8"))
            strings.append(0)
    string_offset = second_offset + 128
    string_address = second_offset + PAGE + 128
    entries = [(5, string_address), (10, len(strings)), *entries, (0, 0)]
    dynamic = b"".join(struct.pack("<qQ", *entry) for entry in entries)
    data = bytearray(second_offset + 2 * PAGE)
    data[:7] = b"\x7fELF\x02\x01\x01"
    struct.pack_into("<HHI", data, 16, 3, 183, 1)
    struct.pack_into("<Q", data, 32, 64)
    struct.pack_into("<HHH", data, 52, 64, 56, 4 if interpreter is not None else 3)
    struct.pack_into("<IIQQQQQQ", data, 64, 1, 5, 0, 0, 0, PAGE, PAGE, PAGE)
    struct.pack_into("<IIQQQQQQ", data, 120, 1, 4, second_offset, second_offset + PAGE,
                     0, 2 * PAGE, 2 * PAGE, PAGE)
    struct.pack_into("<IIQQQQQQ", data, 176, 2, 6, DYNAMIC, DYNAMIC, 0,
                     len(dynamic), len(dynamic), 8)
    if interpreter is not None:
        encoded = interpreter.encode("utf-8") + b"\0"
        struct.pack_into("<IIQQQQQQ", data, 232, 3, 4, INTERP, INTERP, 0, len(encoded), len(encoded), 1)
        data[INTERP:INTERP + len(encoded)] = encoded
    data[DYNAMIC:DYNAMIC + len(dynamic)] = dynamic
    data[string_offset:string_offset + len(strings)] = strings
    return data


def static_fixture():
    data = bytearray(120)
    data[:7] = b"\x7fELF\x02\x01\x01"
    struct.pack_into("<HHI", data, 16, 2, 183, 1)
    struct.pack_into("<Q", data, 32, 64)
    struct.pack_into("<HHH", data, 52, 64, 56, 1)
    struct.pack_into("<IIQQQQQQ", data, 64, 1, 5, 0, 0, 0, 120, 120, PAGE)
    return data


def versioned_fixture(*, required=None, defined=(), base_name=None, weak_needs=False,
                      second_offset=PAGE):
    """Synthetic GNU label tables; no actual symbol table or executable code.

    Definitions are (label, parent_labels) pairs; requirements map library names
    to labels. Keep this builder independent of the production version parser.
    """
    required = required or {}
    data = fixture(needed=tuple(required), soname=base_name, interpreter=None,
                   runpath=None, second_offset=second_offset)
    size = struct.unpack_from("<Q", data, 176 + 32)[0]
    entries = list(struct.iter_unpack("<qQ", data[DYNAMIC:DYNAMIC + size]))[:-1]
    string_offset = second_offset + 128
    strings = bytearray(data[string_offset:string_offset + entries[1][1]])
    cursor = second_offset + 8192
    version_index = 2

    def string(name):
        offset = len(strings)
        strings.extend(name.encode("utf-8") + b"\0")
        return offset

    def digest(name):
        result = 0
        for byte in name.encode("utf-8"):
            result = ((result << 4) + byte) & 0xFFFFFFFF
            upper = result & 0xF0000000
            if upper:
                result ^= upper >> 24
            result &= ~upper
        return result

    definitions = ([(base_name, ())] if base_name else []) + list(defined)
    if definitions:
        entries.extend([(0x6FFFFFFC, cursor + PAGE), (0x6FFFFFFD, len(definitions))])
        for number, (name, parents) in enumerate(definitions):
            base = bool(base_name and number == 0)
            labels = (name, *parents)
            length = 20 + 8 * len(labels)
            struct.pack_into("<HHHHIII", data, cursor, 1, int(base),
                             1 if base else version_index, len(labels), digest(name), 20,
                             length if number < len(definitions) - 1 else 0)
            if not base:
                version_index += 1
            for index, label in enumerate(labels):
                struct.pack_into("<II", data, cursor + 20 + 8 * index, string(label),
                                 8 if index < len(labels) - 1 else 0)
            cursor += length
    if required:
        entries.extend([(0x6FFFFFFE, cursor + PAGE), (0x6FFFFFFF, len(required))])
        for number, (filename, labels) in enumerate(required.items()):
            length = 16 + 16 * len(labels)
            struct.pack_into("<HHIII", data, cursor, 1, len(labels), string(filename), 16,
                             length if number < len(required) - 1 else 0)
            for index, label in enumerate(labels):
                struct.pack_into("<IHHII", data, cursor + 16 + 16 * index, digest(label),
                                 2 if weak_needs else 0, version_index, string(label),
                                 16 if index < len(labels) - 1 else 0)
                version_index += 1
            cursor += length
    entries[1] = (10, len(strings))
    entries.append((0, 0))
    struct.pack_into("<QQ", data, 176 + 32, 16 * len(entries), 16 * len(entries))
    for index, entry in enumerate(entries):
        struct.pack_into("<qQ", data, DYNAMIC + 16 * index, *entry)
    assert string_offset + len(strings) < second_offset + 8192
    data[string_offset:string_offset + len(strings)] = strings
    return data


def dynamic_entry(data, tag):
    size = struct.unpack_from("<Q", data, 176 + 32)[0]
    for offset in range(DYNAMIC, DYNAMIC + size, 16):
        if struct.unpack_from("<q", data, offset)[0] == tag:
            return offset
    raise AssertionError("fixture dynamic tag absent")


class ElfAuditTests(unittest.TestCase):
    def inspect(self, data):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "payload.so"
            path.write_bytes(data)
            return elf.inspect_file(path)

    def test_complete_fields_and_content_hash_without_execution(self):
        data = fixture(needed=("libQt6Core.so.6", "libc.so.6"), rpath="/opt/qor/lib")
        result = self.inspect(data)
        self.assertEqual(result["needed"], ["libQt6Core.so.6", "libc.so.6"])
        self.assertEqual(result["soname"], "libfixture.so")
        self.assertEqual(result["interpreter"], "/lib/ld-linux-aarch64.so.1")
        self.assertEqual(result["rpath"], "/opt/qor/lib")
        self.assertEqual(result["runpath"], "$ORIGIN")
        self.assertEqual(result["sha256"], hashlib.sha256(data).hexdigest())
        self.assertEqual(result["file_size"], len(data))
        self.assertEqual(result["selected_page_size"], PAGE)
        self.assertTrue(result["dynamic"])

    def test_dependency_string_table_beyond_prefix(self):
        data = fixture(second_offset=2 * elf.MAX_READ)
        self.assertEqual(self.inspect(data)["needed"], ["libc.so.6"])

    def test_no_dynamic_segment_is_recorded_separately(self):
        result = self.inspect(static_fixture())
        self.assertEqual(result["needed"], [])
        self.assertIsNone(result["soname"])
        self.assertFalse(result["dynamic"])

    def test_plugin_without_interpreter_and_empty_search_path(self):
        result = self.inspect(fixture(interpreter=None, soname=None, runpath=""))
        self.assertIsNone(result["interpreter"])
        self.assertIsNone(result["soname"])
        self.assertEqual(result["runpath"], "")

    def test_architecture_and_every_load_alignment_are_checked(self):
        for offset, fmt, value in ((18, "<H", 62), (120 + 48, "<Q", 4096),
                                   (120 + 16, "<Q", 1), (120 + 32, "<Q", len(fixture()) + 1)):
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                data = fixture()
                struct.pack_into(fmt, data, offset, value)
                self.inspect(data)

    def test_duplicate_dynamic_or_interpreter_segment_is_refused(self):
        for ptype in (2, 3):
            data = fixture()
            struct.pack_into("<I", data, 120, ptype)
            with self.subTest(ptype=ptype), self.assertRaises(ValueError):
                self.inspect(data)

    def test_missing_duplicate_dynamic_fields_and_terminators_are_refused(self):
        for location, tag in ((DYNAMIC, 99), (DYNAMIC + 16, 5),
                              (DYNAMIC + 16, 99), (DYNAMIC + 5 * 16, 99)):
            data = fixture()
            struct.pack_into("<q", data, location, tag)
            with self.subTest(location=location, tag=tag), self.assertRaises(ValueError):
                self.inspect(data)

    def test_non_null_entry_after_terminator_is_refused(self):
        data = fixture()
        struct.pack_into("<q", data, DYNAMIC + 2 * 16, 0)
        with self.assertRaises(ValueError):
            self.inspect(data)

    def test_null_padding_and_ignored_null_value_are_allowed(self):
        data = fixture()
        size = struct.unpack_from("<Q", data, 176 + 32)[0]
        struct.pack_into("<Q", data, DYNAMIC + size - 8, 123)
        struct.pack_into("<QQ", data, 176 + 32, size + 16, size + 16)
        self.assertEqual(self.inspect(data)["needed"], ["libc.so.6"])

    def test_dynamic_segment_size_and_mapping_must_be_consistent(self):
        for offset, value in ((176 + 32, 15), (176 + 32, 65552),
                              (176 + 40, 0), (176 + 16, 513), (176 + 8, 513),
                              (176 + 16, (1 << 64) - 1)):
            data = fixture()
            struct.pack_into("<Q", data, offset, value)
            with self.subTest(offset=offset, value=value), self.assertRaises(ValueError):
                self.inspect(data)

    def test_unmapped_overflowing_and_zero_filled_string_tables_are_refused(self):
        for address in (PAGE + 128, (1 << 64) - 1, 4 * PAGE):
            data = fixture()
            struct.pack_into("<Q", data, DYNAMIC + 8, address)
            # The final address is in the second segment's zero-fill extension.
            struct.pack_into("<Q", data, 120 + 40, 3 * PAGE)
            with self.subTest(address=address), self.assertRaises(ValueError):
                self.inspect(data)

    def test_overlapping_virtual_load_mapping_is_refused(self):
        data = fixture()
        # Both loads now map the first segment, including PT_DYNAMIC.
        struct.pack_into("<Q", data, 120 + 16, 0)
        with self.assertRaises(ValueError):
            self.inspect(data)

    def test_partly_overlapping_virtual_mapping_is_refused(self):
        data = fixture()
        # One whole mapping plus a second mapping over the tail is ambiguous.
        struct.pack_into("<H", data, 56, 5)
        struct.pack_into("<IIQQQQQQ", data, 288, 1, 4, DYNAMIC + 16,
                         DYNAMIC + 16, 0, 16, 16, PAGE)
        with self.assertRaises(ValueError):
            self.inspect(data)

    def test_string_table_size_and_boundaries_are_checked(self):
        for size in (0, elf.MAX_STRING_TABLE_BYTES + 1, 3 * PAGE):
            data = fixture()
            struct.pack_into("<Q", data, DYNAMIC + 24, size)
            with self.subTest(size=size), self.assertRaises(ValueError):
                self.inspect(data)
        for offset in (PAGE + 128, PAGE + 128 + 33):
            data = fixture()
            size = struct.unpack_from("<Q", data, DYNAMIC + 24)[0]
            location = PAGE + 128 if offset == PAGE + 128 else PAGE + 128 + size - 1
            data[location] = 65
            with self.subTest(location=location), self.assertRaises(ValueError):
                self.inspect(data)

    def test_out_of_range_string_offset_and_unterminated_name_are_refused(self):
        data = fixture()
        struct.pack_into("<Q", data, DYNAMIC + 40, 1 << 63)
        with self.assertRaises(ValueError):
            self.inspect(data)
        with self.assertRaises(ValueError):
            self.inspect(fixture(needed=("a" * (elf.MAX_STRING_BYTES + 1),)))

    def test_invalid_or_duplicate_library_names_are_refused(self):
        for needed in (("",), (b"\xff",), ("lib\n.so",), ("lib\u202e.so",),
                       ("lib.so", "lib.so")):
            with self.subTest(needed=needed), self.assertRaises(ValueError):
                self.inspect(fixture(needed=needed))

    def test_interpreter_size_termination_and_dynamic_requirement(self):
        for offset, value in ((232 + 32, 0), (232 + 32, elf.MAX_STRING_BYTES + 2),
                              (232 + 8, 1 << 63)):
            data = fixture()
            struct.pack_into("<Q", data, offset, value)
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                self.inspect(data)
        data = fixture()
        data[INTERP + 4] = 0
        with self.assertRaises(ValueError):
            self.inspect(data)
        data = fixture()
        struct.pack_into("<I", data, 176, 0)
        with self.assertRaises(ValueError):
            self.inspect(data)
        data = fixture()
        size = struct.unpack_from("<Q", data, 232 + 32)[0]
        data[INTERP + size - 1] = 65
        with self.assertRaises(ValueError):
            self.inspect(data)

    def test_nonregular_and_oversized_input_are_refused_before_reading(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(ValueError):
                elf.inspect_file(directory)
            path = Path(directory) / "too-large"
            path.write_bytes(static_fixture())
            with self.assertRaises(ValueError):
                elf.inspect_file(path, maximum_bytes=119)
            for maximum in (True, 0, -1, 63, elf.MAX_FILE_BYTES + 1):
                with self.subTest(maximum=maximum), self.assertRaises(ValueError):
                    elf.inspect_file(path, maximum_bytes=maximum)
            if hasattr(os, "mkfifo"):
                fifo = Path(directory) / "pipe"
                os.mkfifo(fifo)
                with self.assertRaises(ValueError):
                    elf.inspect_file(fifo)

    def test_mutation_during_inspection_is_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "changing"
            path.write_bytes(static_fixture())
            original = elf._metadata

            def mutate(reader):
                result = original(reader)
                with path.open("ab") as stream:
                    stream.write(b"x")
                return result

            with patch.object(elf, "_metadata", side_effect=mutate), self.assertRaises(ValueError):
                elf.inspect_file(path)

    def test_metadata_read_budget_is_enforced(self):
        with patch.object(elf, "MAX_METADATA_BYTES", 100), self.assertRaises(ValueError):
            self.inspect(fixture())

    def test_foreign_os_abi_identifier_is_refused(self):
        for abi in (0, 3):
            data = fixture()
            data[7] = abi
            self.assertEqual(self.inspect(data)["osabi"], abi)
        for abi in (6, 9, 64, 255):
            data = fixture()
            data[7] = abi
            with self.subTest(abi=abi), self.assertRaises(ValueError):
                self.inspect(data)

    def test_needed_count_is_bounded_before_reading_strings(self):
        with self.assertRaisesRegex(ValueError, "Too many ELF needed"):
            self.inspect(fixture(needed=("a",) * (elf.MAX_NEEDED + 1)))

    def test_overlapping_names_cannot_amplify_output(self):
        data = fixture(needed=tuple(f"lib{i}.so" for i in range(17)),
                       soname=None, runpath=None, interpreter=None)
        strings = b"\0" + b"a" * elf.MAX_STRING_BYTES + b"\0"
        data[PAGE + 128:PAGE + 128 + len(strings)] = strings
        struct.pack_into("<Q", data, DYNAMIC + 24, len(strings))
        # Each distinct suffix is legal by the ELF string-table format. The
        # small input must still not expand past the cumulative output limit.
        for index in range(17):
            struct.pack_into("<Q", data, DYNAMIC + (2 + index) * 16 + 8, index + 1)
        with self.assertRaisesRegex(ValueError, "output bound"):
            self.inspect(data)

    def test_output_bound_counts_repeated_strings_before_deduplication(self):
        data = fixture(needed=tuple(f"lib{i}.so" for i in range(17)),
                       soname=None, runpath=None, interpreter=None)
        strings = b"\0" + b"a" * elf.MAX_STRING_BYTES + b"\0"
        data[PAGE + 128:PAGE + 128 + len(strings)] = strings
        struct.pack_into("<Q", data, DYNAMIC + 24, len(strings))
        for index in range(17):
            struct.pack_into("<Q", data, DYNAMIC + (2 + index) * 16 + 8, 1)
        with self.assertRaisesRegex(ValueError, "output bound"):
            self.inspect(data)

    def test_gnu_labels_base_parents_and_weak_needs_are_distinct(self):
        data = versioned_fixture(base_name="libexample.so", defined=(("LIB_2", ("LIB_1",)),),
                                 required={"libc.so.6": ("GLIBC_2.17", "GLIBC_2.34")}, weak_needs=True)
        result = self.inspect(data)
        self.assertEqual(result["version_definitions"][0]["index"], 1)
        self.assertTrue(result["version_definitions"][0]["base"])
        self.assertEqual(result["version_definitions"][1]["parents"], ["LIB_1"])
        needs = result["version_needs"][0]
        self.assertEqual(needs["file"], "libc.so.6")
        self.assertEqual([item["name"] for item in needs["versions"]], ["GLIBC_2.17", "GLIBC_2.34"])
        self.assertEqual(needs["versions"][0]["hash"], 0x06969197)
        self.assertTrue(all(item["weak"] for item in needs["versions"]))

    def test_version_metadata_absence_is_explicit(self):
        for data in (fixture(), static_fixture()):
            result = self.inspect(data)
            self.assertEqual(result["version_definitions"], [])
            self.assertEqual(result["version_needs"], [])

    def test_version_tables_beyond_prefix_use_virtual_mapping(self):
        result = self.inspect(versioned_fixture(second_offset=2 * elf.MAX_READ,
                              defined=(("LIB_1", ()),), required={"libc.so.6": ("GLIBC_2.17",)}))
        self.assertEqual(result["version_definitions"][0]["name"], "LIB_1")
        self.assertEqual(result["version_needs"][0]["versions"][0]["name"], "GLIBC_2.17")

    def test_version_tag_pairs_duplicates_counts_and_pointers(self):
        for pointer, count in ((elf.DT_VERDEF, elf.DT_VERDEFNUM), (elf.DT_VERNEED, elf.DT_VERNEEDNUM)):
            for tag, value, replace_tag in ((pointer, 0, False), (count, 0, False),
                                            (count, elf.MAX_VERSION_NODES + 1, False),
                                            (pointer, 99, True), (count, 99, True),
                                            (count, pointer, True)):
                data = versioned_fixture(defined=(("LIB_1", ()),), required={"libc.so.6": ("GLIBC_2.17",)})
                struct.pack_into("<q" if replace_tag else "<Q", data,
                                 dynamic_entry(data, tag) + (0 if replace_tag else 8), value)
                with self.subTest(tag=tag, value=value, replace_tag=replace_tag), self.assertRaises(ValueError):
                    self.inspect(data)

    def test_version_definition_chain_revision_flags_indices_and_auxiliary(self):
        start = PAGE + 8192
        mutations = [(0, "<H", 2), (2, "<H", 4), (4, "<H", 1), (6, "<H", 0),
                     (6, "<H", 2), (8, "<I", 0), (12, "<I", 0), (12, "<I", 4),
                     (12, "<I", 21), (16, "<I", 28), (20, "<I", 0xFFFFFFFF), (24, "<I", 8)]
        for offset, layout, value in mutations:
            data = versioned_fixture(defined=(("LIB_1", ()),))
            struct.pack_into(layout, data, start + offset, value)
            with self.subTest(offset=offset, value=value), self.assertRaises(ValueError):
                self.inspect(data)

    def test_version_requirement_chain_revision_flags_indices_and_auxiliary(self):
        start = PAGE + 8192
        mutations = [(0, "<H", 2), (2, "<H", 0), (2, "<H", 2), (4, "<I", 0),
                     (8, "<I", 0), (8, "<I", 4), (8, "<I", 17), (12, "<I", 32),
                     (16, "<I", 0), (20, "<H", 1), (22, "<H", 1),
                     (24, "<I", 0xFFFFFFFF), (28, "<I", 16)]
        for offset, layout, value in mutations:
            data = versioned_fixture(required={"libc.so.6": ("GLIBC_2.17",)})
            struct.pack_into(layout, data, start + offset, value)
            with self.subTest(offset=offset, value=value), self.assertRaises(ValueError):
                self.inspect(data)

    def test_version_high_index_bit_is_preserved_but_masks_for_uniqueness(self):
        data = versioned_fixture(required={"libc.so.6": ("GLIBC_2.17", "GLIBC_2.34")})
        struct.pack_into("<H", data, PAGE + 8192 + 22, 0x8002)
        self.assertEqual(self.inspect(data)["version_needs"][0]["versions"][0]["index"], 0x8002)
        struct.pack_into("<H", data, PAGE + 8192 + 38, 2)
        with self.assertRaisesRegex(ValueError, "duplicate ELF version index"):
            self.inspect(data)

    def test_definition_and_requirement_share_one_version_index_namespace(self):
        data = versioned_fixture(defined=(("LIB_1", ()),), required={"libc.so.6": ("GLIBC_2.17",)})
        address = struct.unpack_from("<Q", data, dynamic_entry(data, elf.DT_VERNEED) + 8)[0]
        struct.pack_into("<H", data, address - PAGE + 22, 2)
        with self.assertRaisesRegex(ValueError, "duplicate ELF version index"):
            self.inspect(data)

    def test_version_nodes_cannot_overlap_or_read_zero_fill(self):
        for address in (PAGE + 8192 + PAGE + 1, PAGE + 128 + PAGE, DYNAMIC,
                        4 * PAGE, (1 << 64) - 4):
            data = versioned_fixture(defined=(("LIB_1", ()),))
            struct.pack_into("<Q", data, dynamic_entry(data, elf.DT_VERDEF) + 8, address)
            struct.pack_into("<Q", data, 120 + 40, 3 * PAGE)
            with self.subTest(address=address), self.assertRaises(ValueError):
                self.inspect(data)
        data = versioned_fixture(defined=(("LIB_1", ()), ("LIB_2", ())))
        # The second outer record attempts to alias the first auxiliary record.
        struct.pack_into("<I", data, PAGE + 8192 + 16, 20)
        with self.assertRaises(ValueError):
            self.inspect(data)

    def test_version_duplicates_and_unlisted_requirement_library(self):
        for data in (versioned_fixture(defined=(("LIB_1", ()), ("LIB_1", ()))),
                     versioned_fixture(defined=(("LIB_1", ("LIB_1",)),)),
                     versioned_fixture(required={"libc.so.6": ("GLIBC_2.17", "GLIBC_2.17")})):
            with self.assertRaises(ValueError):
                self.inspect(data)
        data = versioned_fixture(required={"libc.so.6": ("GLIBC_2.17",)})
        # Remove only DT_NEEDED; the Verneed filename remains intact.
        struct.pack_into("<q", data, dynamic_entry(data, 1), 99)
        with self.assertRaisesRegex(ValueError, "unique DT_NEEDED"):
            self.inspect(data)

    def test_combined_version_node_and_repeated_parent_output_bounds(self):
        data = versioned_fixture(defined=(("LIB_1", ()),), required={"libc.so.6": ("GLIBC_2.17",)})
        with patch.object(elf, "MAX_VERSION_NODES", 3), self.assertRaisesRegex(ValueError, "node bound"):
            self.inspect(data)
        data = versioned_fixture(defined=(("LIB_1", ("PARENT",)), ("LIB_2", ("PARENT",))))
        with patch.object(elf, "MAX_EMITTED_STRING_BYTES", 20), self.assertRaisesRegex(ValueError, "output bound"):
            self.inspect(data)


if __name__ == "__main__":
    unittest.main()

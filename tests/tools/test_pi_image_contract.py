# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""In-memory draft contracts only; no image, executable, hardware or real login."""
from pathlib import Path
import json
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/image"))
from pi_image_contract import MAX_CONTRACT_BYTES, validate_pi_image_contract


class PiImageContractTests(unittest.TestCase):
    # Deliberately synthetic policy, never a production profile selection.
    profile = "fixture-pi-arm64"

    def image(self, **changes):
        data = {"schema": 1, "profile": self.profile, "architecture": "arm64"}
        data.update(changes)
        return json.dumps(data).encode()

    def prism(self, **changes):
        data = {"schema": 1, "development": False, "authentication": True}
        data.update(changes)
        return json.dumps(data).encode()

    def check(self, metadata=None, capabilities=None, **changes):
        policy = dict(opt_in=True, expected_profile=self.profile, prism_exit_code=0)
        policy.update(changes)
        return validate_pi_image_contract(self.image() if metadata is None else metadata,
                                          self.prism() if capabilities is None else capabilities,
                                          **policy)

    def test_explicit_synthetic_arm64_contract_passes(self):
        self.assertEqual(self.check(), [])

    def test_opt_in_is_required_and_boolean(self):
        for value in (False, None, 0, 1, "true", [], {}):
            with self.subTest(value=value):
                self.assertTrue(self.check(opt_in=value))

    def test_profile_policy_has_no_inferred_or_production_default(self):
        for value in (None, "", [], 1, "two profiles", "pi/arm64", "a" * 65):
            with self.subTest(value=value):
                self.assertTrue(self.check(expected_profile=value))
        with self.assertRaises(TypeError):
            validate_pi_image_contract(self.image(), self.prism(), opt_in=True, prism_exit_code=0)

    def test_x86_and_other_architectures_are_rejected(self):
        for value in ("amd64", "x86_64", "x64", "i386", "arm", "aarch64", "ARM64"):
            with self.subTest(value=value):
                self.assertTrue(self.check(self.image(architecture=value)))

    def test_wrong_profile_is_rejected(self):
        for value in ("fixture-x86", "generic", "another-pi", "fixture-pi-arm64 "):
            with self.subTest(value=value):
                self.assertTrue(self.check(self.image(profile=value)))

    def test_missing_metadata_fields_are_rejected(self):
        for name in ("schema", "profile", "architecture"):
            value = json.loads(self.image())
            del value[name]
            with self.subTest(name=name):
                self.assertTrue(self.check(json.dumps(value).encode()))

    def test_ambiguous_metadata_fields_are_rejected(self):
        for name in ("profile", "architecture"):
            for value in (None, "", [], ["arm64", "amd64"], {"arm64": True}, True):
                with self.subTest(name=name, value=value):
                    self.assertTrue(self.check(self.image(**{name: value})))
        self.assertTrue(self.check(self.image(arch="amd64")))
        self.assertTrue(self.check(self.image(profiles=[self.profile])))

    def test_duplicate_metadata_fields_even_identical_are_rejected(self):
        for field, value in (("architecture", '"arm64"'), ("architecture", '"amd64"'),
                             ("profile", json.dumps(self.profile)), ("profile", '"fixture-x86"'),
                             ("schema", "1")):
            raw = self.image()[:-1] + (',"' + field + '":' + value + '}').encode()
            with self.subTest(raw=raw):
                self.assertTrue(self.check(raw))

    def test_unknown_metadata_schema_is_rejected(self):
        for value in (True, False, None, 0, 2, "1", 1.0):
            with self.subTest(value=value):
                self.assertTrue(self.check(self.image(schema=value)))

    def test_missing_malformed_and_nonobject_json_is_rejected(self):
        invalid = (None, {}, "{}", b"", b"{", b"[]", b"null", b"true", b"\xff",
                   b'{}{}', b'{"architecture":NaN}', b"[" * 2000 + b"]" * 2000)
        for value in invalid:
            with self.subTest(value=value):
                self.assertTrue(validate_pi_image_contract(value, self.prism(), opt_in=True,
                                expected_profile=self.profile, prism_exit_code=0))
                self.assertTrue(validate_pi_image_contract(self.image(), value, opt_in=True,
                                expected_profile=self.profile, prism_exit_code=0))

    def test_both_json_inputs_are_bounded(self):
        for value in (self.image() + b" " * MAX_CONTRACT_BYTES,
                      self.prism() + b" " * MAX_CONTRACT_BYTES):
            self.assertTrue(self.check(metadata=value))
            self.assertTrue(self.check(capabilities=value))

    def test_missing_false_or_nonboolean_authentication_is_rejected(self):
        value = json.loads(self.prism())
        del value["authentication"]
        self.assertTrue(self.check(capabilities=json.dumps(value).encode()))
        for value in (False, None, 0, 1, "true", [], {}):
            with self.subTest(value=value):
                self.assertTrue(self.check(capabilities=self.prism(authentication=value)))

    def test_development_or_unknown_prism_contract_is_rejected(self):
        for changes in ({"development": True}, {"development": 0}, {"schema": True},
                        {"schema": 2}, {"schema": 1.0}, {"unknown": False}):
            with self.subTest(changes=changes):
                self.assertTrue(self.check(capabilities=self.prism(**changes)))
        value = json.loads(self.prism())
        del value["development"]
        self.assertTrue(self.check(capabilities=json.dumps(value).encode()))

    def test_duplicate_authentication_even_true_is_rejected(self):
        for value in (b"true", b"false"):
            raw = self.prism()[:-1] + b',"authentication":' + value + b'}'
            self.assertTrue(self.check(capabilities=raw))

    def test_failed_or_unreported_probe_cannot_pass_with_valid_stdout(self):
        for value in (None, False, True, -1, 1, "0"):
            with self.subTest(value=value):
                self.assertTrue(self.check(prism_exit_code=value))

    def test_errors_do_not_echo_untrusted_metadata(self):
        marker = "untrusted-marker"
        errors = self.check(self.image(profile=marker))
        self.assertTrue(errors)
        self.assertNotIn(marker, " ".join(errors))


if __name__ == "__main__":
    unittest.main()

# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Actions trust boundaries and failed administration must be visible to callers."""

import contextlib
import copy
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/github"))
import check_actions_policy as policy

SPEC = importlib.util.spec_from_file_location("github_bootstrap", ROOT / "tools/github/bootstrap.py")
bootstrap = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(bootstrap)

WORKFLOW = """on: pull_request
permissions:
  contents: read
jobs:
  check:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1
        with:
          persist-credentials: false
"""


class ActionsPolicyTest(unittest.TestCase):
    def setUp(self):
        self.policy = policy.load_policy()

    def test_repository_workflows_obey_policy(self):
        _, files, uses = policy.check_repository()
        self.assertEqual(files, 3)
        self.assertGreater(uses, 0)

    def test_valid_workflow_and_quoted_false(self):
        for text in (WORKFLOW, WORKFLOW.replace("credentials: false", "credentials: 'false'")):
            self.assertEqual(policy.check_workflow(text, self.policy), 1)

    def test_mutable_truncated_dynamic_local_container_and_unknown_actions_refused(self):
        ref = "actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1"
        for replacement in ("actions/checkout@v7", "actions/checkout@3d3c42e", "${{ inputs.action }}",
                            "./local-action", "docker://unreviewed/image:latest",
                            "other/action@" + "a" * 40):
            with self.subTest(replacement=replacement), self.assertRaises(ValueError):
                policy.check_workflow(WORKFLOW.replace(ref, replacement), self.policy)

    def test_token_write_permissions_refused_at_both_levels(self):
        for text in (WORKFLOW.replace("contents: read", "contents: write"),
                     WORKFLOW.replace("permissions:\n  contents: read", "permissions: write-all"),
                     WORKFLOW.replace("    steps:", "    permissions:\n      pull-requests: write\n    steps:")):
            with self.subTest(text=text), self.assertRaises(ValueError):
                policy.check_workflow(text, self.policy)

    def test_missing_or_persisted_checkout_credentials_refused(self):
        for value in ("true", "0", "null", "${{ inputs.persist }}"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                policy.check_workflow(WORKFLOW.replace("credentials: false", "credentials: " + value), self.policy)
        with self.assertRaises(ValueError):
            policy.check_workflow(WORKFLOW.replace("          persist-credentials: false\n", ""), self.policy)

    def test_privileged_events_refused(self):
        for event in ("pull_request_target", "workflow_run"):
            with self.subTest(event=event), self.assertRaises(ValueError):
                policy.check_workflow(WORKFLOW.replace("on: pull_request", "on: [pull_request, " + event + "]"), self.policy)

    def test_duplicate_keys_aliases_and_malformed_yaml_refused(self):
        for text in (WORKFLOW + "permissions: {}\n", WORKFLOW + "first: &x [1]\nsecond: *x\n",
                     "[unterminated", WORKFLOW.replace("jobs:", "jobs: []\nother:"),
                     WORKFLOW.replace("    steps:", "    steps: 1\n    invalid:")):
            with self.subTest(text=text), self.assertRaises(ValueError):
                policy.check_workflow(text, self.policy)

    def test_oversized_workflow_refused(self):
        with self.assertRaises(ValueError):
            policy.check_workflow(" " * (policy.MAX_BYTES + 1), self.policy)

    def test_nested_reusable_workflow_must_be_reviewed(self):
        text = WORKFLOW.replace("    runs-on: ubuntu-24.04", "    uses: other/repo/.github/workflows/job.yml@" + "a" * 40)
        with self.assertRaises(ValueError):
            policy.check_workflow(text, self.policy)

    def test_policy_rejects_broad_patterns_and_false_boolean_types(self):
        for section, key, value in (("permissions", "sha_pinning_required", False),
                                    ("permissions", "enabled", 1),
                                    ("selected_actions", "github_owned_allowed", True),
                                    ("selected_actions", "verified_allowed", "false"),
                                    ("selected_actions", "patterns_allowed", ["actions/*"]),
                                    ("selected_actions", "patterns_allowed", ["actions/checkout@*"] * 2),
                                    ("workflow_permissions", "can_approve_pull_request_reviews", True)):
            changed = copy.deepcopy(self.policy)
            changed[section][key] = value
            with self.subTest(section=section, key=key), tempfile.TemporaryDirectory() as folder:
                root = Path(folder)
                (root / ".github").mkdir()
                (root / ".github/actions-policy.json").write_text(json.dumps(changed), encoding="utf-8")
                with self.assertRaises(ValueError):
                    policy.load_policy(root)

    def test_policy_rejects_duplicate_json_keys(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / ".github").mkdir()
            (root / ".github/actions-policy.json").write_text('{"permissions": {}, "permissions": {}}', encoding="utf-8")
            with self.assertRaises(ValueError):
                policy.load_policy(root)

    def test_diagnostics_do_not_echo_parser_values(self):
        output = io.StringIO()
        with patch.object(policy, "check_repository", side_effect=ValueError("PRIVATE_TEST_VALUE")), contextlib.redirect_stderr(output):
            self.assertEqual(policy.main(), 1)
        self.assertNotIn("PRIVATE_TEST_VALUE", output.getvalue())

    def test_symlink_is_refused_before_opening(self):
        with patch.object(Path, "is_symlink", return_value=True), patch.object(Path, "open") as opened:
            with self.assertRaises(ValueError):
                policy.read_text(Path("workflow.yml"))
        opened.assert_not_called()


class BootstrapTest(unittest.TestCase):
    def setUp(self):
        self.policy = policy.load_policy()
        self.endpoints = {"actions/permissions": "permissions", "actions/permissions/selected-actions": "selected_actions",
                          "actions/permissions/workflow": "workflow_permissions"}

    def response(self, *args, **kwargs):
        endpoint = args[-1].split("repos/QOR-MATRIX/qor-os/")[1]
        if "-X" in args:
            self.assertEqual(kwargs["payload"], self.policy[self.endpoints[endpoint]])
            return subprocess.CompletedProcess(args, 0, "", "")
        result = copy.deepcopy(self.policy[self.endpoints[endpoint]])
        if "patterns_allowed" in result:
            result["patterns_allowed"].reverse()  # API ordering is not policy drift.
        return subprocess.CompletedProcess(args, 0, json.dumps(result), "")

    def test_apply_requires_all_three_readbacks(self):
        with patch.object(bootstrap, "gh", side_effect=self.response) as gh:
            bootstrap.apply_actions_policy(bootstrap.DEFAULT_REPO)
        self.assertEqual(gh.call_count, 6)
        self.assertIn("-X", gh.call_args_list[0].args)
        self.assertNotIn("-X", gh.call_args_list[-1].args)

    def test_invalid_local_policy_prevents_remote_mutations(self):
        with patch.object(bootstrap, "check_repository", side_effect=ValueError("invalid")), patch.object(bootstrap, "gh") as gh:
            with self.assertRaises(ValueError):
                bootstrap.apply_actions_policy(bootstrap.DEFAULT_REPO)
        gh.assert_not_called()

    def test_rejected_write_stops_without_broadening_permissions(self):
        with patch.object(bootstrap, "gh", side_effect=subprocess.CalledProcessError(1, "gh")) as gh:
            with self.assertRaises(subprocess.CalledProcessError):
                bootstrap.apply_actions_policy(bootstrap.DEFAULT_REPO)
        self.assertEqual(gh.call_count, 1)

    def test_readback_drift_fails(self):
        def drift(*args, **kwargs):
            if "-X" in args:
                return self.response(*args, **kwargs)
            return subprocess.CompletedProcess(args, 0, '{"enabled": true, "allowed_actions": "all"}', "")
        with patch.object(bootstrap, "gh", side_effect=drift), self.assertRaises(ValueError):
            bootstrap.apply_actions_policy(bootstrap.DEFAULT_REPO)

    def test_rejected_ruleset_returns_failure_without_echoing_response(self):
        result = subprocess.CompletedProcess([], 1, "PRIVATE_TEST_VALUE", "PRIVATE_TEST_VALUE")
        output = io.StringIO()
        with patch.object(bootstrap, "gh", return_value=result), contextlib.redirect_stderr(output):
            self.assertFalse(bootstrap.apply_ruleset(bootstrap.DEFAULT_REPO))
        self.assertNotIn("PRIVATE_TEST_VALUE", output.getvalue())

    def test_actions_only_does_not_change_labels_milestones_or_repository_settings(self):
        with patch.object(sys, "argv", ["bootstrap.py", "--actions-only"]), patch.object(bootstrap, "gh", side_effect=self.response) as gh:
            self.assertEqual(bootstrap.main(), 0)
        self.assertTrue(all("/actions/permissions" in call.args[-1] for call in gh.call_args_list))

    def test_cli_reports_failed_write_without_response_values(self):
        output = io.StringIO()
        failure = subprocess.CalledProcessError(1, "gh", output="PRIVATE_TEST_VALUE", stderr="PRIVATE_TEST_VALUE")
        with patch.object(sys, "argv", ["bootstrap.py", "--actions-only"]), patch.object(bootstrap, "gh", side_effect=failure), contextlib.redirect_stderr(output):
            self.assertEqual(bootstrap.main(), 1)
        self.assertNotIn("PRIVATE_TEST_VALUE", output.getvalue())

    def test_ruleset_failure_reaches_cli_exit(self):
        with patch.object(sys, "argv", ["bootstrap.py", "--ruleset"]), patch.object(bootstrap, "apply_settings"), \
                patch.object(bootstrap, "sync_labels"), patch.object(bootstrap, "sync_milestones"), \
                patch.object(bootstrap, "apply_ruleset", return_value=False):
            self.assertEqual(bootstrap.main(), 1)

    def test_rejected_security_setting_is_not_reported_as_applied(self):
        def reject_alerts(*args, **kwargs):
            if args[-1].endswith("/vulnerability-alerts"):
                raise subprocess.CalledProcessError(1, "gh")
            return subprocess.CompletedProcess(args, 0, "{}", "")
        output = io.StringIO()
        with patch.object(bootstrap, "gh", side_effect=reject_alerts), contextlib.redirect_stdout(output):
            with self.assertRaises(subprocess.CalledProcessError):
                bootstrap.apply_settings(bootstrap.DEFAULT_REPO)
        self.assertNotIn("settings: applied", output.getvalue())


if __name__ == "__main__":
    unittest.main()

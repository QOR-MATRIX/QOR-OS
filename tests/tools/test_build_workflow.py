# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""What .github/workflows/build.yml promises about runners and readiness, read from the file itself.

A workflow cannot be run here, so what can be checked is: the order of its steps, what its jobs depend on, and, by running
it, the script that decides whether a job on a kit's runner may start.
"""

import os
import pathlib
import re
import shutil
import subprocess
import sys
import unittest

WORKFLOW = pathlib.Path(__file__).resolve().parents[2] / ".github" / "workflows" / "build.yml"
TEXT = WORKFLOW.read_text(encoding="utf-8")


def job(name):
    """The lines of a job, from its key to the next job's."""
    lines = TEXT.splitlines()
    start = lines.index(f"  {name}:")
    end = next((i for i in range(start + 1, len(lines)) if re.match(r"^  [A-Za-z][\w-]*:$", lines[i])), len(lines))
    return lines[start:end]


def steps(name):
    """A job's steps, in order, as their first line."""
    return [line.strip() for line in job(name) if re.match(r"^      - ", line)]


def preflight_script():
    """The shell of the check for Qt, as written."""
    lines = job("preflight")
    start = next(i for i, line in enumerate(lines) if line.strip() == "run: |") + 1
    return "\n".join(line[10:] for line in lines[start:])


class WorkflowTest(unittest.TestCase):
    def test_nothing_of_the_repository_runs_before_the_boundary(self):
        for name in ("windows", "linux"):
            with self.subTest(job=name):
                order = steps(name)
                self.assertEqual(order[0], "- name: Runner boundary")
                self.assertIn("actions/checkout", order[1])

    def test_a_job_on_a_kits_runner_needs_the_check_for_qt(self):
        windows = "\n".join(job("windows"))
        linux = "\n".join(job("linux"))
        self.assertIn("if: needs.preflight.outputs.windows == 'true'", windows)
        self.assertIn("needs.preflight.outputs.linux == 'true'", linux)
        self.assertIn("WINDOWS_ISOLATED: ${{ vars.QOR_WINDOWS_RUNNER_ISOLATED == 'true' }}", TEXT)
        self.assertIn("LINUX_ISOLATED: ${{ vars.QOR_LINUX_RUNNER_ISOLATED == 'true' }}", TEXT)

    def test_the_linux_boundary_stops_on_the_way_to_windows(self):
        linux = "\n".join(job("linux"))
        for reached in (
            'reachable="$reachable a Windows drive at',
            'reachable="$reachable Windows interop;',
            "socket.socket(socket.AF_VSOCK, socket.SOCK_STREAM)",
            'reachable="$reachable AF_VSOCK,',
            'reachable="$reachable perhaps AF_VSOCK:',
            'reachable="$reachable a privileged group;',
            ".local/share/Qt/qtaccount.ini",
        ):
            with self.subTest(reached=reached):
                self.assertIn(reached, linux)

    def test_the_windows_boundary_stops_on_keys_and_credentials(self):
        windows = "\n".join(job("windows"))
        for held in (".qor-os", ".ssh", "qtaccount.ini"):
            with self.subTest(held=held):
                self.assertIn(held, windows)

    def test_no_failure_is_let_through(self):
        self.assertNotIn("continue-on-error", TEXT)

    def test_products_on_linux_do_not_take_the_floor_as_passed(self):
        # Whether Linux CI lets the frame-rate floor stand is the owner's decision, not yet given.
        self.assertNotIn("--accept-software-floor", TEXT)

    def test_readiness_runs_whatever_the_jobs_did_on_a_machine_of_github(self):
        readiness = "\n".join(job("readiness"))
        self.assertIn("needs: [preflight, windows, linux]", readiness)
        self.assertIn("if: ${{ !cancelled() }}", readiness)
        self.assertIn("runs-on: ubuntu-24.04", readiness)
        self.assertIn("python3 -I tools/ci/release_readiness.py", readiness)
        self.assertNotIn("RUNNER_ISOLATED", readiness)

    def test_readiness_is_told_of_every_step_it_requires(self):
        readiness = "\n".join(job("readiness"))
        for word, expression in {
            "WINDOWS_RUNNER": "needs.preflight.outputs.windows-source",
            "LINUX_RUNNER": "needs.preflight.outputs.linux-source",
            "WINDOWS_JOB": "needs.windows.result",
            "WINDOWS_GPU": "needs.windows.outputs.gpu",
            "WINDOWS_PRODUCTS": "needs.windows.outputs.products",
            "WINDOWS_PRODUCTS_CHANGED": "needs.windows.outputs.products-changed",
            "LINUX_JOB": "needs.linux.result",
            "LINUX_GPU": "needs.linux.outputs.gpu",
            "LINUX_PRISM": "needs.linux.outputs.prism",
            "LINUX_PRODUCTS": "needs.linux.outputs.products",
            "LINUX_PRODUCTS_CHANGED": "needs.linux.outputs.products-changed",
        }.items():
            with self.subTest(word=word):
                self.assertIn(f"{word}: ${{{{ {expression} }}}}", readiness)

    def test_no_job_claims_the_products_ran_unrelaxed_on_a_graphics_card(self):
        # Until a job runs the products on a Linux machine with a graphics card, readiness is told nothing of it, and so reads
        # "not run". A job that adds it must set it from what that run did. Not in the readiness job, nor anywhere it could
        # reach it from, such as the workflow's own env.
        self.assertNotIn("LINUX_PRODUCTS_GPU", TEXT)

    def test_readiness_decides_by_its_script_alone(self):
        readiness = job("readiness")
        # Its verdict is the script's exit, with nothing after it to turn a failure into a pass.
        self.assertIn("        run: python3 -I tools/ci/release_readiness.py", readiness)
        # No step of it may be skipped: a skipped step leaves the job green.
        self.assertEqual([line for line in readiness if re.match(r"^        if:", line)], [])
        # Whether this is a release is said exactly, so unchanged products cannot stand down at a tag or a manual run.
        self.assertIn("          IS_RELEASE: ${{ startsWith(github.ref, 'refs/tags/') || github.event_name == 'workflow_dispatch' }}", readiness)

    def test_what_changes_the_products_is_everything_they_are_built_and_checked_with(self):
        patterns = re.findall(r"'(\^\(products/[^']*)'", TEXT)
        self.assertEqual(len(patterns), 2, "one list for each platform")
        self.assertEqual(patterns[0], patterns[1], "the two platforms ask about the same files")
        changes = re.compile(patterns[0])
        repository = WORKFLOW.parents[2]
        named = {".github/workflows/build.yml", ".github/qt-packages.json", "docs/design/tokens.json"}
        # What the products' builds include from outside their own folders.
        for cmake in ("products/qq/CMakeLists.txt", "products/arqade/qt/CMakeLists.txt", "tools/qor-media/CMakeLists.txt"):
            folder = (repository / cmake).parent
            for included in re.findall(r"include\((\.\./[^)\s]+)\)", (repository / cmake).read_text(encoding="utf-8")):
                named.add((folder / included).resolve().relative_to(repository).as_posix())
        # What the products' steps run, and the tools the products' scripts call.
        for name in ("windows", "linux"):
            lines = job(name)
            for i, line in enumerate(lines):
                if line.strip() == "if: steps.products.outputs.run == 'true'":
                    run = next(l for l in lines[i + 1 : i + 4] if l.strip().startswith("run:"))
                    named.update(path.replace("\\", "/") for path in re.findall(r"tools[\\/][\w./\\-]+\.\w+", run))
        for script in ("tools/dev/products.sh", "tools/dev/products.ps1"):
            named.add(script)
            named.update(re.findall(r"tools/dev/[\w.-]+\.(?:py|qml|sh|ps1)", (repository / script).read_text(encoding="utf-8")))
        self.assertIn("cmake/QorHardening.cmake", named)
        self.assertIn("tools/ci/check_hardening.py", named)
        for path in sorted(named):
            with self.subTest(path=path):
                self.assertTrue((repository / path).is_file(), "named, so it must be there")
                self.assertTrue(changes.match(path), "a change to it must run the products")

    def test_each_job_says_what_its_required_steps_did(self):
        for name, outputs in {
            "windows": {"gpu": "steps.gpu-tests.outcome", "products": "steps.product-suites.outcome", "products-changed": "steps.products.outputs.run"},
            "linux": {
                "gpu": "steps.gpu-tests.outcome",
                "prism": "steps.prism-protocol.outcome",
                "products": "steps.product-suites.outcome",
                "products-changed": "steps.products.outputs.run",
            },
        }.items():
            lines = job(name)
            for key, expression in outputs.items():
                with self.subTest(job=name, key=key):
                    self.assertIn(f"      {key}: ${{{{ {expression} }}}}", lines)
                    step = expression.split(".")[1]
                    self.assertIn(f"        id: {step}", lines)

    @unittest.skipIf(sys.platform == "win32", "bash on Windows may be WSL's, and a suite hears nothing of the machine")
    def test_a_kit_without_isolation_never_runs_even_with_installer_secrets(self):
        bash = shutil.which("bash")
        self.assertTrue(bash)
        for kit, isolated, secrets, expected in (
            ("true", "true", "false", "true"),
            ("true", "true", "true", "true"),
            ("true", "false", "false", "false"),
            ("true", "false", "true", "false"),
            ("false", "false", "true", "true"),
            ("false", "true", "true", "true"),
            ("false", "false", "false", "false"),
            ("false", "true", "false", "false"),
        ):
            with self.subTest(kit=kit, isolated=isolated, secrets=secrets):
                self.assertEqual(self._decide(bash, kit, isolated, secrets), expected)

    def _decide(self, bash, kit, isolated, secrets):
        import tempfile

        with tempfile.TemporaryDirectory() as folder:
            output = pathlib.Path(folder) / "output"
            summary = pathlib.Path(folder) / "summary"
            env = {
                "PATH": os.environ.get("PATH", ""),
                "HAS_WINDOWS_KIT": kit,
                "HAS_LINUX_KIT": kit,
                "WINDOWS_ISOLATED": isolated,
                "LINUX_ISOLATED": isolated,
                "HAS_SECRETS": secrets,
                "GITHUB_OUTPUT": str(output),
                "GITHUB_STEP_SUMMARY": str(summary),
            }
            subprocess.run([bash, "-euo", "pipefail", "-c", preflight_script()], env=env, check=True, capture_output=True, text=True)
            said = dict(line.split("=", 1) for line in output.read_text(encoding="utf-8").splitlines())
            self.assertEqual(said["windows"], said["linux"])
            return said["linux"]


if __name__ == "__main__":
    unittest.main()

# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Build a private live probe or guarded installation candidate, never a release."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import uuid

from vmguard import unlinked
from vm_payload import validate as validate_payload
from vm_resources import require_headroom

REPO = Path(__file__).resolve().parents[2]


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def diagnostics_failed(log: str) -> bool:
    # BuildKit prints the RUN source too; a quoted grep pattern is not an
    # emitted diagnostic. Accept only actual tool output with a time prefix.
    return bool(re.search(r"^(?:#\d+ \d+(?:\.\d+)? )?(?:dracut\[E\]:|dracut-install: ERROR|FAILED:|"
                          r"Lint warning:|Warnings:\s*[1-9][0-9]*\b)", log, re.MULTILINE))


def runtime_provenance(runtime: Path, receipt_path: Path):
    receipt_path = unlinked(receipt_path)
    if not receipt_path.is_relative_to(REPO / "build") or receipt_path.stat().st_size > 2 * 1024**2:
        raise ValueError("A bounded private runtime receipt is required")
    return verify_runtime_receipt(runtime, json.loads(receipt_path.read_text(encoding="utf-8")), digest(receipt_path))


def verify_runtime_receipt(runtime: Path, receipt: dict, receipt_sha256: str):
    if not isinstance(receipt, dict) or not isinstance(receipt.get("files"), dict):
        raise ValueError("Missing runtime file provenance")
    revision = receipt.get("source_revision", receipt.get("source_head", ""))
    if not isinstance(revision, str) or not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise ValueError("The runtime's actual compiler source revision is unknown")
    observed = {path.relative_to(runtime).as_posix(): digest(path) for path in runtime.rglob("*") if path.is_file()}
    if observed != receipt["files"]:
        raise ValueError("Runtime files differ from their compiler receipt")
    return {"schema": 1, "profile": "private-display-probe-NOT-installation",
            "compiler_source_revision": revision, "compiler_source_dirty": receipt.get("source_dirty", "not-recorded"),
            "fresh_build_proved": bool(receipt.get("fresh_build_inputs")),
            "receipt_sha256": receipt_sha256, "files": observed}


def plain_tree(root: Path):
    for path in (root, *root.rglob("*")):
        if (path.is_symlink() or getattr(path, "is_junction", lambda: False)()
                or path.resolve() != path.absolute() or not (path.is_file() or path.is_dir())):
            raise ValueError("Only unlinked input files and directories are accepted")
        if path.is_file() and path.stat().st_nlink != 1:
            raise ValueError("Hard-linked image inputs are refused")


def installation_inputs(runtime_record, revision, dirty):
    if (dirty or runtime_record["compiler_source_dirty"] is not False
            or runtime_record["compiler_source_revision"] != revision or not runtime_record["fresh_build_proved"]
            or "usr/bin/qor-install" not in runtime_record["files"]):
        raise ValueError("Installation requires a fresh device runtime from this exact clean revision")


def build_command(context, tools_ref, revision, target, tag, installation):
    return ["docker", "build", "--network=none", "--pull=false", "--file", str(context / "Probe.Containerfile"),
            "--build-arg", "QOR_VM_TOOLS=" + tools_ref, "--build-arg", "QOR_SOURCE_REVISION=" + revision,
            "--build-arg", "QOR_VM_INSTALLATION=" + ("1" if installation else "0"),
            "--target", target, "--tag", tag, str(context)]


def checked_image_build(argv, path):
    require_headroom("VM image build")
    with path.open("x", encoding="utf-8") as log:
        code = subprocess.run(argv, cwd=REPO, stdout=log, stderr=subprocess.STDOUT).returncode
    if code or diagnostics_failed(path.read_text(encoding="utf-8", errors="replace")):
        raise ValueError("Image build failed; preserve its exact log")


def offline_payload(context, output, tools_ref, tools_id, revision):
    tag = "qor-os/vm-deploy:" + uuid.uuid4().hex
    checked_image_build(build_command(context, tools_ref, revision, "deploy", tag, True), output / "deploy-build.log")
    image = subprocess.check_output(["docker", "image", "inspect", "--format", "{{.Id}}", tag], text=True).strip()
    archive = output / "deployment-image.tar"
    require_headroom("Offline image export")
    subprocess.run(["docker", "save", "--output", str(archive), image], check=True)
    payload = output / "offline-payload"
    payload.mkdir(exist_ok=False)
    command = ["docker", "run", "--rm", "--pull", "never", "--network", "none", "--cap-drop", "ALL",
               "--security-opt", "no-new-privileges", "--pids-limit", "32", "--cpus", "1", "--memory", "512m",
               "--tmpfs", "/tmp:rw,nosuid,nodev,size=32m",
               "--mount", f"type=bind,source={archive},target=/input/source.tar,readonly",
               "--mount", f"type=bind,source={payload},target=/output", tools_id,
               "/bin/bash", "-ceu", "mkdir /output/tmp; exec skopeo --tmpdir /output/tmp copy --format oci docker-archive:/input/source.tar oci:/output/install-source"]
    require_headroom("Offline payload conversion")
    with (output / "oci-conversion.log").open("xb") as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
    root = payload / "install-source"
    plain_tree(root)
    index = json.loads((root / "index.json").read_text(encoding="utf-8"))
    manifests = index.get("manifests", [])
    if len(manifests) != 1 or not re.fullmatch(r"sha256:[0-9a-f]{64}", manifests[0].get("digest", "")):
        raise ValueError("Conversion did not produce one content-addressed image")
    receipt = {"schema": 2, "source_revision": revision, "manifest_sha256": manifests[0]["digest"][7:],
               "profile": "private-vm-lab", "encryption": False}
    validate_payload(root, receipt, lambda path: path.open("rb"))
    (payload / "install-source.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    return payload, {"image": image, "archive_sha256": digest(archive), "receipt": receipt, "conversion_command": command}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--runtime-record", type=Path, required=True)
    parser.add_argument("--branding", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--tools-ref", required=True)
    parser.add_argument("--tools-id", required=True)
    parser.add_argument("--installation", action="store_true", help="Include the guarded offline VM installer; no readiness claim")
    args = parser.parse_args()
    output = args.output.absolute()
    for path in (output, args.runtime.absolute(), args.branding.absolute()):
        if (not path.resolve().is_relative_to(REPO / "build") or path.is_symlink()
                or getattr(path, "is_junction", lambda: False)()):
            raise ValueError("Only private QOR build directories are accepted")
    if not re.fullmatch(r"sha256:[0-9a-f]{64}", args.tools_id):
        raise ValueError("An exact local tools image ID is required")
    resources = require_headroom("VM image preparation")
    tooling_lock = json.loads((REPO / "image/vm/tooling-input.json").read_text(encoding="utf-8"))
    if (tooling_lock.get("schema") != 1 or tooling_lock.get("tools_image") != args.tools_id
            or tooling_lock.get("rpm_manifest_sha256") != digest(REPO / "image/vm/tooling-rpms.txt")):
        raise ValueError("Tooling differs from the reviewed input snapshot")
    image_id = subprocess.check_output(["docker", "image", "inspect", "--format", "{{.Id}}", args.tools_ref], text=True).strip()
    if image_id != args.tools_id:
        raise ValueError("The tools image differs from the recorded input")
    plain_tree(args.runtime.absolute())
    plain_tree(args.branding.absolute())
    runtime_record = runtime_provenance(args.runtime.absolute(), args.runtime_record.absolute())
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=REPO, text=True).strip()
    dirty = bool(subprocess.check_output(["git", "status", "--porcelain=v1"], cwd=REPO))
    if args.installation:
        installation_inputs(runtime_record, revision, dirty)
    subprocess.run([sys.executable, str(REPO / "tools/image/assemble.py"), "verify", "--output", str(args.branding)], check=True)
    output.mkdir(parents=False, exist_ok=False)
    context = output / "context"
    context.mkdir()
    shutil.copytree(args.runtime, context / "runtime")
    shutil.copytree(args.branding, context / "branding")
    (context / "runtime-provenance.json").write_text(json.dumps(runtime_record, indent=2) + "\n")
    if sys.platform != "win32":
        raise ValueError("This lab currently uses the existing Windows host for its reviewed menu derivative")
    subprocess.run(["pwsh", "-NoProfile", "-File", str(REPO / "tools/branding/resize_menu_mark.ps1"),
                    "-Output", str(context / "menu-mark.png")], check=True)
    for name in ("Probe.Containerfile", "display-probe.service", "build-iso.sh", "display.service",
                 "first-run.service", "guest-agent.service", "installer.service"):
        shutil.copyfile(REPO / "image/vm" / name, context / name)
    shutil.copyfile(REPO / "tools/image/check_vm_runtime.py", context / "check_vm_runtime.py")
    for name in ("vmguard.py", "guest_inventory.py", "vm_install_backend.py", "vm_display.py", "vm_payload.py", "vm_session.py"):
        shutil.copyfile(REPO / "tools/image" / name, context / name)
    inputs = {str(path.relative_to(context)): digest(path) for path in context.rglob("*") if path.is_file()}
    record = {"profile": "private-vm-installation-candidate" if args.installation else "live-display-probe-NOT-installation", "tools_image": image_id,
              "source_revision": revision, "source_dirty": dirty, "host_memory_preflight": resources,
              "runtime": {key: value for key, value in runtime_record.items() if key != "files"},
              "host_tool_sources": {name: digest(REPO / name) for name in (
                  "tools/image/build_vm_probe.py", "tools/image/assemble.py", "tools/branding/assets.py",
                  "tools/branding/resize_menu_mark.ps1", "tools/image/check_vm_runtime.py",
                  "tools/image/vm_resources.py",
                  "image/vm/tooling-input.json", "image/vm/tooling-rpms.txt")},
              "inputs": inputs, "boot_proved": False, "installation_proved": False}
    (output / "build-inputs.json").write_text(json.dumps(record, indent=2) + "\n")
    payload = None
    if args.installation:
        payload, record["deployment"] = offline_payload(context, output, args.tools_ref, image_id, revision)
    live_tag = "qor-os/vm-live-probe:" + uuid.uuid4().hex
    command = build_command(context, args.tools_ref, revision, "live", live_tag, args.installation)
    require_headroom("VM live-image build")
    with (output / "build.log").open("w", encoding="utf-8") as log:
        code = subprocess.run(command, cwd=REPO, stdout=log, stderr=subprocess.STDOUT).returncode
    if code:
        print("Live probe build failed; inspect the build log", file=sys.stderr)
        return code
    log_text = (output / "build.log").read_text(encoding="utf-8", errors="replace")
    if diagnostics_failed(log_text):
        print("The builder returned success but initramfs diagnostics failed; artifact not accepted", file=sys.stderr)
        return 1
    live_id = subprocess.check_output(["docker", "image", "inspect", "--format", "{{.Id}}", live_tag], text=True).strip()
    if subprocess.check_output(["docker", "image", "inspect", "--format", "{{.Id}}", args.tools_ref], text=True).strip() != image_id:
        raise ValueError("The tooling tag changed during the build")
    artifact = output / "artifact"
    artifact.mkdir()
    compose = ["docker", "run", "--rm", "--pull", "never", "--network", "none", "--cap-drop", "ALL",
               "--cap-add", "DAC_READ_SEARCH", "--security-opt", "no-new-privileges", "--pids-limit", "32", "--cpus", "1",
               "--memory", "768m", "--tmpfs", "/tmp:rw,nosuid,nodev,size=64m", "--env", "QOR_IMAGE_ROOT=/",
               "--env", "QOR_ISO_TREE=/out/iso", "--mount", f"type=bind,source={artifact},target=/out",
               "--mount", f"type=bind,source={context / 'build-iso.sh'},target=/compose.sh,readonly",
               live_id, "/usr/bin/bash", "/compose.sh"]
    if payload:
        compose[-3:-3] = ["--env", "QOR_VM_INSTALLATION=1", "--mount", f"type=bind,source={payload},target=/qor-payload,readonly"]
    require_headroom("ISO composition")
    with (output / "compose.log").open("wb") as log:
        code = subprocess.run(compose, stdout=log, stderr=subprocess.STDOUT).returncode
    if code:
        print("Bounded ISO composition failed; artifact not accepted", file=sys.stderr)
        return code
    record["live_image"] = live_id
    record["compose_command"] = compose
    iso = artifact / ("qor-os-vm-install.iso" if args.installation else "qor-os-live-probe.iso")
    if not iso.is_file() or iso.stat().st_size < 64 * 1024**2:
        raise ValueError("No complete boot artifact was produced")
    if args.installation and (subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=REPO, text=True).strip() != revision
                              or subprocess.check_output(["git", "status", "--porcelain=v1"], cwd=REPO)):
        raise ValueError("Sources changed; no installation image receipt accepted")
    record.update(artifact=str(iso.relative_to(output)), artifact_bytes=iso.stat().st_size, artifact_sha256=digest(iso))
    (output / "build-result.json").write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps({"artifact": str(iso), "sha256": record["artifact_sha256"], "boot_proved": False, "installation_proved": False}))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        raise SystemExit("Live probe refused: " + str(error))

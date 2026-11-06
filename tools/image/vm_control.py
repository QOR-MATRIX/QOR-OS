# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Container-local QMP only: observe, keyboard input, or stop this test VM."""
import argparse
import json
import os
from pathlib import Path
import re
import socket
import uuid


def request(command):
    if os.environ.get("QOR_VM_PHASE") not in ("live", "installed"):
        raise ValueError("Control is available only inside the owned test container")
    expected = json.loads(Path("/vm/allowlist.json").read_text())
    expected_uuid = str(uuid.UUID(expected["vm_uuid"]))
    with socket.create_connection(("127.0.0.1", 4444), timeout=10) as connection:
        stream = connection.makefile("rwb")

        def receive():
            line = stream.readline(1024 * 1024 + 1)
            if not line or len(line) > 1024 * 1024:
                raise ValueError("Missing or oversized QMP response")
            return json.loads(line)

        receive()
        for payload in ({"execute": "qmp_capabilities"}, {"execute": "query-uuid"}, command):
            stream.write(json.dumps(payload).encode() + b"\n")
            stream.flush()
            while True:
                response = receive()
                if "return" in response or "error" in response:
                    if "error" in response:
                        raise ValueError(response["error"])
                    break
            if payload["execute"] == "query-uuid" and response["return"].get("UUID", "").lower() != expected_uuid:
                raise ValueError("QMP endpoint belongs to another VM; operation refused")
        return response


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("status", "screenshot", "key", "powerdown", "quit"))
    parser.add_argument("value", nargs="?", default="")
    args = parser.parse_args()
    if args.action == "screenshot":
        if not re.fullmatch(r"[a-z0-9-]+\.png", args.value):
            raise ValueError("Screenshot basename required")
        command = {"execute": "screendump", "arguments": {"filename": "/evidence/" + args.value, "format": "png"}}
    elif args.action == "key":
        if not re.fullmatch(r"[a-z0-9_-]{1,20}", args.value):
            raise ValueError("Single QEMU key code required")
        command = {"execute": "send-key", "arguments": {"keys": [{"type": "qcode", "data": args.value}], "hold-time": 80}}
    else:
        command = {"execute": {"status": "query-status", "powerdown": "system_powerdown", "quit": "quit"}[args.action]}
    print(json.dumps(request(command)))


if __name__ == "__main__":
    main()

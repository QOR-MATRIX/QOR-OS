# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""Container-local QMP only: observe, keyboard input, or stop this test VM."""
import argparse
import json
import os
from pathlib import Path
import re
import socket
import uuid

AGENT_RPCS = frozenset(("guest-sync-delimited", "guest-ping", "guest-info", "guest-shutdown"))


def agent_policy(result):
    commands = result.get("supported_commands", []) if isinstance(result, dict) else []
    if (not isinstance(commands, list) or not commands or len(commands) > 512
            or any(not isinstance(item, dict) or not isinstance(item.get("enabled"), bool) for item in commands)):
        raise ValueError("Missing guest-agent policy evidence")
    enabled = [item.get("name") for item in commands if item["enabled"]]
    if len(enabled) != len(AGENT_RPCS) or set(enabled) != AGENT_RPCS:
        raise ValueError("Guest agent exposes unexpected operations")
    return {"enabled": sorted(AGENT_RPCS), "other_rpcs_disabled": len(commands) - len(enabled)}


def guest_request(action):
    if action not in ("guest-info", "guest-reboot", "guest-powerdown"):
        raise ValueError("Unknown guest operation")
    # This also validates the owned container and exact QMP VM UUID before
    # opening the separate agent channel. No endpoint is published to the host.
    request({"execute": "query-status"})
    with socket.create_connection(("127.0.0.1", 4446), timeout=10) as connection:
        stream = connection.makefile("rwb")
        nonce = uuid.uuid4().int & ((1 << 53) - 1)
        stream.write(b"\xff" + json.dumps({"execute": "guest-sync-delimited", "arguments": {"id": nonce}}).encode() + b"\n")
        stream.flush()
        synchronized = False
        for _ in range(16):
            line = stream.readline(8193)
            if not line or len(line) > 8192:
                raise ValueError("Missing or oversized guest-agent response")
            if b"\xff" in line:
                response = json.loads(line.rsplit(b"\xff", 1)[1])
                if response.get("return") == nonce:
                    synchronized = True
                    break
        if not synchronized:
            raise ValueError("Guest agent did not synchronize")
        stream.write(b'{"execute":"guest-info"}\n')
        stream.flush()
        line = stream.readline(131073)
        if not line or len(line) > 131072:
            raise ValueError("Missing or oversized guest policy")
        policy = agent_policy(json.loads(line).get("return"))
        if action == "guest-info":
            return policy
        mode = "reboot" if action == "guest-reboot" else "powerdown"
        stream.write(json.dumps({"execute": "guest-shutdown", "arguments": {"mode": mode}}).encode() + b"\n")
        stream.flush()
        # guest-shutdown deliberately has no success response. Only actual
        # guest shutdown/boot evidence can establish that it completed.
        return {"requested": mode, "completion_proved": False, "policy": policy}


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
    parser.add_argument("action", choices=("status", "screenshot", "key", "powerdown", "quit",
                                          "guest-info", "guest-reboot", "guest-powerdown"))
    parser.add_argument("value", nargs="?", default="")
    args = parser.parse_args()
    if args.action.startswith("guest-"):
        print(json.dumps(guest_request(args.action)))
        return
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

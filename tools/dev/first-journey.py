#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
"""A project becomes a game, the Library finds it, and the Player plays the copy.

The real programs, from the end of QQ Studio's work to the start of a game:

1. `qq-studio --build` makes a game of a project on disk, on the person's list of
   trusted projects, and says so on standard output with nothing else asked of it.
2. The real QQ Player plays what was built: its logic builds and its model loads.
3. Real Prism, with QOR_PRISM_BUILD naming a build of it, finds the game in its
   Library, the copy and the project side by side, with the title the build
   gave it, and is asked to play the copy. What it starts is recorded.
4. The real Player plays exactly what Prism asked for.
5. The project changes, and the game does not until it is built again.

This is the only place the two sides meet: the manifest a real build writes, read
by the real Library. Each side's own suite has its own copy of what the other says.

What this does not show. A person's work in the Studio is tst_gamebuild's, Play
and Build side by side among it. Prism here draws in software with no graphics
card, so what it starts in step 3 is prism-test-qq-player, and step 4 is the real
Player on a display of its own, not the real Player in a Prism window. That needs
a real graphics card (docs/BUILDING.md).

What it cannot see of a played game: only a top-level Prop that failed to load,
logic that did not build and a scene that did not start. A sound or a prefab that
is missing plays as silence or nothing, so that is what the build refuses first.

    tools/dev/first-journey.py --qq-studio <exe> --qq-player <exe> --model <orb.gltf>

Exit status: 0 when every step passed, 1 when one failed, and 77 when all that
ran passed and Prism's steps did not run, for want of QOR_PRISM_BUILD.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

NOT_RUN = 77


class Failed(Exception):
    pass


def check(condition: bool, what: str) -> None:
    if not condition:
        raise Failed(what)
    print(f"ok: {what}", flush=True)


def scene_text(colour: str) -> str:
    return (
        'import QQ\n\nScene {\n    name: "journey"\n'
        f'    skyTop: "{colour}"\n    skyHorizon: "{colour}"\n'
        '    Prop { name: "Orb"; position: Qt.vector3d(30, 30, -30); source: "../assets/orb.gltf" }\n'
        '    Behaviour { name: "Rule"; source: "../logic/rule.qml"; target: "Orb" }\n}\n'
    )


def make_project(project: Path, model: Path, data: Path) -> None:
    (project / "scenes").mkdir(parents=True)
    (project / "logic").mkdir()
    (project / "assets").mkdir()
    (project / ".git").mkdir()
    (project / "scenes" / "journey.qml").write_text(scene_text("#ff0000"), encoding="utf-8")
    (project / "logic" / "rule.qml").write_text("import QQ\n\nLogic {\n}\n", encoding="utf-8")
    shutil.copy(model, project / "assets" / "orb.gltf")
    # What stays behind: a game holds what a game can use.
    (project / ".git" / "config").write_text("[remote]\n", encoding="utf-8")
    (project / ".env").write_text("TOKEN=stays-behind\n", encoding="utf-8")
    (project / "notes.txt").write_text("a person's notes\n", encoding="utf-8")
    # On the person's own list, as QQ Studio keeps it when they say they trust a project: --build takes nothing else.
    (data / "qor-os").mkdir(parents=True)
    (data / "qor-os" / "own-projects.json").write_text(json.dumps([str(project)]), encoding="utf-8")


def build(studio: str, project: Path, env: dict[str, str]) -> dict:
    done = subprocess.run(
        [studio, "--project", str(project), "--build", "scenes/journey.qml"],
        env={name: value for name, value in env.items() if name != "QT_FORCE_STDERR_LOGGING"},
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=120,
        check=False,
    )
    for line in done.stdout.splitlines():
        if line.startswith("QQ-BUILD "):
            return json.loads(line.split("QQ-BUILD ", 1)[1])
    raise Failed(f"qq-studio --build said no result on standard output, exit {done.returncode}: {done.stdout} {done.stderr}")


def play(player: str, arguments: list[str], out: Path, env: dict[str, str]) -> dict:
    """The real Player, as asked, with its measurement written to a file. What it measured."""
    done = subprocess.run(
        [player, "--measure", str(out), *arguments],
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=180,
        check=False,
    )
    if done.returncode != 0 or not out.exists():
        raise Failed(f"the Player, on {arguments}, exit {done.returncode}: {done.stdout}")
    return json.loads(out.read_text(encoding="utf-8"))


def check_played(measured: dict, scene: str) -> None:
    check(measured.get("problem") == "", f"it plays, with no problem: {measured.get('problem')!r}")
    check(measured.get("logicErrors") == [], f"its logic builds: {measured.get('logicErrors')}")
    check(measured.get("unloaded") == [], f"its model loads: {measured.get('unloaded')}")
    check(measured.get("scene") == Path(scene).as_uri(), f"what played is {measured.get('scene')}")


def find_program(root: Path, name: str) -> str:
    for found in sorted(root.rglob(name)):
        if found.is_file() and os.access(found, os.X_OK):
            return str(found)
    raise Failed(f"no {name} under {root}")


def prism_steps(build_folder: Path, home: Path, scene: str, project_scene: str, title: str) -> list[str]:
    """Prism's Library is asked to play the game. What Prism started the Player with."""
    runtime = home / "runtime"
    runtime.mkdir(mode=0o700)
    asked = home / "prism-asked"
    stand_in = find_program(build_folder, "prism-test-qq-player")
    recorder = home / "player-recorder"
    recorder.write_text(f'#!/bin/sh\nprintf "%s\\n" "$@" > "{asked}"\nexec "{stand_in}" "$@"\n', encoding="utf-8")
    recorder.chmod(0o755)

    item = f"qq:{scene}"
    title = f"{Path(scene).name} — Test QQ Player"
    script = home / "journey.e2e.json"
    script.write_text(
        json.dumps(
            {
                "description": "The Library finds a game QQ Studio built, and plays the copy.",
                "steps": [
                    {"wait": 3000},
                    {"log": "The Library has found the built game, ahead of anything else"},
                    {"expect": {"path": "library.games.0", "equals": item}},
                    {"log": "With the title the build gave it, which only a manifest the Library believes can say"},
                    {"expect": {"path": "library.gameSources.0", "equals": "built"}},
                    {"expect": {"path": "library.gameTitles.0", "equals": title}},
                    {"log": "And the project itself beside it, as the person's own, as it stands: two things, told apart"},
                    {"expect": {"path": "library.mine.0", "equals": f"qq:{project_scene}"}},
                    {"expect": {"path": "library.recent", "equals": ""}},
                    {"log": "Asked to play it, Prism starts the Player"},
                    {"call": "runAction", "args": ["library.play", {"id": item}]},
                    {"waitFor": {"window": {"title": title}, "timeout": 30000}},
                    {"expect": {"path": "products.qq-player.running", "equals": True}},
                    {"expect": {"path": "library.recent", "equals": item}},
                ],
            }
        ),
        encoding="utf-8",
    )

    env = dict(os.environ)
    # As tools/dev/prism-products.py runs it: nothing of the session it runs in, and software only. Its home too, or its Library
    # would read the machine's real Documents, Movies, Music and Pictures (AGENTS.md).
    for name in ("DISPLAY", "WAYLAND_DISPLAY", "XDG_ACTIVATION_TOKEN", "QT_QPA_PLATFORM"):
        env.pop(name, None)
    env.update(
        {
            "HOME": str(home),
            "XDG_RUNTIME_DIR": str(runtime),
            "XDG_DATA_HOME": str(home / "data"),
            "XDG_CONFIG_HOME": str(home / "config"),
            "QT_QPA_PLATFORM": "offscreen",
            "QT_QUICK_BACKEND": "software",
            "GALLIUM_DRIVER": "llvmpipe",
            "LIBGL_ALWAYS_SOFTWARE": "1",
            "QT_FORCE_STDERR_LOGGING": "1",
            "QOR_AUDIO_OUTPUT": "null",
            "QOR_GAMEPADS": "virtual",
            "QOR_TOUCHPAD": "off",
            "QOR_ARQADE": find_program(build_folder, "prism-test-arqade"),
            "QOR_MEDIA": find_program(build_folder, "prism-test-media"),
            "QOR_QQ_STUDIO": find_program(build_folder, "prism-test-qq-studio"),
            "QOR_QQ_PLAYER": str(recorder),
        }
    )
    command = [find_program(build_folder, "prism"), "--unlocked", "--size", "1920x1200", "--socket", "wayland-journey", "--script", str(script)]
    done = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=180, check=False)
    if done.returncode != 0:
        raise Failed(f"Prism's script, exit {done.returncode}:\n{done.stdout}")
    print("Prism's script passed", flush=True)
    return asked.read_text(encoding="utf-8").splitlines()


def journey(args: argparse.Namespace, home: Path) -> int:
    # A name past what a title may be, so that the cut the build makes is put to the Library's own limit.
    project = home / "Documents" / ("journey-" * 12 + "end")
    make_project(project, Path(args.model), home / "data")
    # Only where a game goes and what it saves: the display the run was given stays its own.
    env = dict(os.environ, XDG_DATA_HOME=str(home / "data"))

    print("== The build", flush=True)
    first = build(args.qq_studio, project, env)
    check(first.get("ok") is True, f"built: {first.get('problem')!r}")
    folder = Path(first["folder"])
    scene = first["scene"]
    check(folder.parent == home / "data" / "qor-os" / "games", f"in the games folder the Library reads: {folder}")
    check(first["build"] == 1, "its first build")
    check(first.get("compiled") is False, f"and it says nothing was compiled: {first.get('note')!r}")
    manifest = json.loads((folder / "game.json").read_text(encoding="utf-8"))
    check(
        sorted(manifest["files"]) == ["assets/orb.gltf", "logic/rule.qml", "scenes/journey.qml"],
        f"it holds what a game can use: {sorted(manifest['files'])}",
    )
    check(not (folder / ".env").exists() and not (folder / ".git").exists() and not (folder / "notes.txt").exists(), "and nothing else")

    print("== The real Player on what was built", flush=True)
    check_played(play(args.qq_player, [scene], home / "first.json", env), scene)

    print("== Prism's Library", flush=True)
    prism = os.environ.get("QOR_PRISM_BUILD", "")
    ran_prism = bool(prism)
    if ran_prism:
        asked = prism_steps(Path(prism), home, scene, str(project / "scenes" / "journey.qml"), first["title"])
        check(asked[-1] == scene, f"Prism started the Player on the copy: {asked}")
        print("== The real Player on what Prism asked for", flush=True)
        check_played(play(args.qq_player, asked, home / "asked.json", env), scene)
    else:
        print("Not run: QOR_PRISM_BUILD names no build of Prism", flush=True)

    print("== The project changes", flush=True)
    project_scene = project / "scenes" / "journey.qml"
    project_scene.write_text(scene_text("#00ff00"), encoding="utf-8")
    check("#ff0000" in Path(scene).read_text(encoding="utf-8"), "the game has not changed")
    second = build(args.qq_studio, project, env)
    check(second.get("ok") is True and second["folder"] == first["folder"] and second["build"] == 2, "built again: the same game, its second build")
    check("#00ff00" in Path(scene).read_text(encoding="utf-8"), "and now it has")

    shutil.rmtree(project)
    print("== The project gone", flush=True)
    check_played(play(args.qq_player, [scene], home / "last.json", env), scene)
    return 0 if ran_prism else NOT_RUN


def main() -> int:
    parser = argparse.ArgumentParser(description=(__doc__ or "").splitlines()[0])
    parser.add_argument("--qq-studio", required=True)
    parser.add_argument("--qq-player", required=True)
    parser.add_argument("--model", required=True, help="A glTF model the project holds")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="qor-first-journey-") as temporary:
        # Resolved, as what the programs say is: a temporary folder may lie behind a link.
        home = Path(temporary).resolve()
        try:
            return journey(args, home)
        except Failed as failed:
            print(f"FAILED: {failed}", file=sys.stderr, flush=True)
            return 1
        except subprocess.TimeoutExpired as late:
            print(f"FAILED: did not finish in time: {late.cmd}", file=sys.stderr, flush=True)
            return 1


if __name__ == "__main__":
    sys.exit(main())

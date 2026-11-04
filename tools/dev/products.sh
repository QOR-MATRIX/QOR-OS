#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#
# Builds and tests the products QOR OS carries from demiurge-chain (QQ, ARQADE
# and QOR Media) on Linux: the counterpart of tools/dev/products.ps1.
#
# Their own scripts build them on Windows only, with PowerShell that calls
# MSVC, as demiurge-chain did. The Linux build lives here, outside the
# products (docs/UPSTREAM.md). Each product is configured with CMake and
# Ninja against the Qt kit, built, and every suite it registers with CTest is
# run. A product whose suites fail has its failed suites run once more, as
# products.ps1 and CI's GPU suites do; a genuine failure fails both attempts.
#
#   tools/dev/products.sh                          # all three
#   tools/dev/products.sh --product qq,arqade
#   tools/dev/products.sh --product qq --tests tst_agent   # the suites whose names match
#   tools/dev/products.sh --qt ~/Qt/6.12.0/gcc_64 --build-root ~/qor-build/products --attempts 1
#   tools/dev/products.sh --display session        # on this desktop, to watch
#   tools/dev/products.sh --display prism          # inside Prism
#   tools/dev/products.sh --gpu                    # under WSL, on the GPU, which has frozen the PC
#   tools/dev/products.sh --build-only             # built, and no suite run
#   tools/dev/products.sh --accept-software-floor  # the QQ Player's frame-rate floor stands, if drawn in software and all that failed
#
# After each product it says what each suite did, by name: passed, failed and
# skipped, what drew it, and each case failed or skipped, with why (and in a CI
# job's summary). At the end it names a suite that passed only on a later attempt.
#
# It sets QOR_EXPECTED_RENDERER to what the machine's devices say should draw
# here, and a suite whose checks differ in software stops if its renderer says
# otherwise, whichever way round (docs/CI.md#what-linux-runs).
#
# Under WSL the run is fenced from the GPU, unless --gpu is given. On 28
# October three runs that drew on it through Direct3D 12 hung the PC: a suite
# stuck in the driver, out of reach of ctest's timeout and of any kill, and
# Windows frozen later (docs/CI.md). So the script runs itself again in a
# mount namespace of its own where /dev/dxg, WSL's door to the GPU, is
# /dev/null: Mesa draws in software (llvmpipe), and nothing (OpenGL, Vulkan,
# CUDA or video decoding) reaches the GPU. Where systemd's user manager runs,
# the run is also held to half the processors and three quarters of WSL's
# memory, so the PC stays usable and a runaway suite is stopped inside it.
# In software the QQ Player's floor of 20 frames a second fails, and a pass says
# nothing of a real GPU, real sound or a real controller (docs/CI.md).
#
# The suites open real windows and drive them with input. By default
# (--display offscreen) they open them on a display of the run's own,
# tools/dev/products-display.qml: a compositor on Qt's offscreen platform that
# keeps each window at its own size and gives the newest the keyboard, as a
# desktop's does. There is no X display, the sound goes nowhere, and the
# session bus and its keyring are the run's own, so nothing reaches this
# desktop's screen, speakers or keyring, and nothing on it can take a test
# window's keyboard. It needs the kit's Qt Wayland Compositor, as Prism does,
# and for the keyring dbus-daemon, python3-dbus and python3-gi.
# --display prism opens them inside Prism instead, just as hidden; Prism opens
# each window at the size it asks for, within its work area. It finds Prism
# in build/ci-linux or build/linux-*, or takes --prism. --display session uses
# the session's own display and sound (Wayland, X11 or WSLg). QOR Media needs
# Qt PDF in the kit (the installer's extensions.qtpdf) and PDFium, which this
# script fetches once at the release QOR Media's build.ps1 pins, and checks
# against its SHA-256.
set -euo pipefail

REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
QT=${QOR_QT_DIR:-$HOME/Qt/6.12.0/gcc_64}
BUILD_ROOT=$REPO/build/products-linux
PRODUCTS=qq,arqade,media
ATTEMPTS=2
CONFIG=Release
SHOW=offscreen
PRISM=${QOR_PRISM:-}
GPU=no
TESTS=
BUILD_ONLY=no
ACCEPT_FLOOR=no
SUITE_TIMEOUT=900
DRAWN=
ARGS=("$@")

usage() { sed -n '3,/^set -euo/p' "$0" | sed -e '$d' -e 's/^# \{0,1\}//'; }

while [ $# -gt 0 ]; do
    case $1 in
        --qt) QT=$2; shift 2 ;;
        --build-root) BUILD_ROOT=$2; shift 2 ;;
        --product) PRODUCTS=$2; shift 2 ;;
        --attempts) ATTEMPTS=$2; shift 2 ;;
        --tests) TESTS=$2; shift 2 ;;
        --display) SHOW=$2; shift 2 ;;
        --prism) PRISM=$2; shift 2 ;;
        --gpu) GPU=yes; shift ;;
        --build-only) BUILD_ONLY=yes; shift ;;
        --accept-software-floor) ACCEPT_FLOOR=yes; shift ;;
        -h | --help) usage; exit 0 ;;
        *) echo "Unknown argument: $1" >&2; usage >&2; exit 2 ;;
    esac
done
case $ATTEMPTS in 1 | 2 | 3) ;; *) echo "--attempts must be 1, 2 or 3" >&2; exit 2 ;; esac
case $SHOW in offscreen | prism | session) ;; *) echo "--display must be offscreen, prism or session" >&2; exit 2 ;; esac
[ -f "$QT/lib/cmake/Qt6/Qt6Config.cmake" ] || { echo "Not a Qt kit: $QT" >&2; exit 2; }
if [ "$SHOW" = offscreen ] && [ ! -f "$QT/qml/QtWayland/Compositor/XdgShell/qmldir" ]; then
    echo "The kit has no Qt Wayland Compositor (qt.qt6.6120.addons.qtwaylandcompositor), which the" >&2
    echo "offscreen display needs. Install it, or run the suites on this desktop with --display session." >&2
    exit 2
fi
if [ "$SHOW" = prism ] && [ -z "$PRISM" ]; then
    for preset in ci-linux linux-release linux-debug; do
        if [ -x "$REPO/build/$preset/prism/compositor/prism" ]; then
            PRISM=$REPO/build/$preset/prism/compositor/prism
            break
        fi
    done
fi
if [ "$SHOW" = prism ] && [ ! -x "$PRISM" ]; then
    echo "No Prism to run the suites in. Build it (cmake --build --preset linux-release --target prism)," >&2
    echo "name one with --prism, or run them on this desktop with --display session." >&2
    exit 2
fi

# The fence (see the top). Under WSL /dev/dxg is the GPU; inside the fence it
# is /dev/null, device 1:3.
DXG=
if [ -c /dev/dxg ]; then
    if [ "$(stat -c %t:%T /dev/dxg)" = 1:3 ]; then DXG=fenced; else DXG=gpu; fi
fi
fence() {
    local cpus=$((($(nproc) + 1) / 2))
    if ! unshare --user --map-root-user --mount true 2> /dev/null; then
        echo "Under WSL the run hides the GPU in a namespace of its own, and this system allows" >&2
        echo "none (unprivileged user namespaces). --gpu runs on the GPU, which has frozen this PC." >&2
        exit 2
    fi
    # As root of a namespace of its own, /dev/null over /dev/dxg; then back to
    # the developer's own user id, in a namespace inside it.
    # shellcheck disable=SC2016 # the inner bash expands them
    local run=(unshare --user --map-root-user --mount -- bash -c \
        'mount --bind /dev/null /dev/dxg && exec unshare --user --map-user="$1" --map-group="$2" -- "${@:3}"' \
        fence "$(id -u)" "$(id -g)" "$REPO/tools/dev/products.sh" "${ARGS[@]}")
    export CMAKE_BUILD_PARALLEL_LEVEL=${CMAKE_BUILD_PARALLEL_LEVEL:-$cpus} LP_NUM_THREADS=${LP_NUM_THREADS:-$cpus}
    if systemd-run --user --scope --quiet true 2> /dev/null; then
        echo "== Held to $cpus of $(nproc) processors and three quarters of WSL's memory"
        exec systemd-run --user --scope --quiet -p CPUQuota=$((cpus * 100))% -p MemoryHigh=66% -p MemoryMax=75% \
            -- "${run[@]}"
    fi
    echo "== No systemd user manager, so the run is fenced from the GPU but may use the whole PC"
    exec "${run[@]}"
}
[ "$DXG" != gpu ] || [ "$GPU" = yes ] || fence

declare -A SOURCES=([qq]=products/qq [arqade]=products/arqade/qt [media]=tools/qor-media)

# PDFium for QOR Media: the release tools/qor-media/build.ps1 pins, for Linux.
PDFIUM_RELEASE=chromium/8111
PDFIUM_SHA256=bdc60126399f39fe768566a18d4be055a7b3681bc9e248e8a6656b4f6cae0aa2
pdfium() {
    local dir=${XDG_CACHE_HOME:-$HOME/.cache}/qor-media-pdfium/${PDFIUM_RELEASE//\//-}
    local archive=$dir/pdfium-linux-x64.tgz
    mkdir -p "$dir"
    if [ ! -f "$archive" ]; then
        curl -sSfL -o "$archive" \
            "https://github.com/bblanchon/pdfium-binaries/releases/download/$PDFIUM_RELEASE/pdfium-linux-x64.tgz" >&2
    fi
    if ! echo "$PDFIUM_SHA256  $archive" | sha256sum --check --status; then
        rm -f "$archive"
        echo "PDFium's archive is not the pinned release; it was removed. Run again to fetch it." >&2
        return 1
    fi
    [ -f "$dir/PDFiumConfig.cmake" ] || tar -xzf "$archive" -C "$dir"
    echo "$dir"
}

# Qt Multimedia's PipeWire backend crashes where libpipewire is installed but
# no PipeWire daemon runs (WSLg); PulseAudio serves there, as in qor::audio.
if [ -z "${QT_AUDIO_BACKEND:-}" ] && [ ! -S "${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/pipewire-0" ]; then
    export QT_AUDIO_BACKEND=pulseaudio
fi
# Under WSLg, Mesa draws in software (llvmpipe) unless told to reach the GPU
# through Direct3D 12 (prism/README.md). Fenced, it draws in software whatever
# the environment says; with --gpu, through Direct3D 12.
if [ "$DXG" = fenced ]; then
    export GALLIUM_DRIVER=llvmpipe
    echo "== Fenced from the GPU: /dev/dxg is /dev/null here, and Mesa draws in software"
elif [ "$DXG" = gpu ] && [ -z "${GALLIUM_DRIVER:-}" ]; then
    export GALLIUM_DRIVER=d3d12
fi
# What should draw here, from the machine's devices: fenced, or with no door to a GPU at all (a sandbox without devices, a
# hosted runner), software is all Mesa has. That is an expectation. The suites whose checks depend on it ask the renderer, and
# stop if it says otherwise, whichever way round (products/qq/tests/drawnin.h, tst_player): set here always, so that the
# environment cannot say it for us, and a machine with a GPU falling back on software is a failure and not an easier run.
if [ "$DXG" = fenced ] || { [ -z "$DXG" ] && ! compgen -G '/dev/dri/renderD*' > /dev/null; }; then
    DRAWN="drawn in software"
    export QOR_EXPECTED_RENDERER=software
    export LP_NUM_THREADS=${LP_NUM_THREADS:-$((($(nproc) + 1) / 2))}
    # Measured on 1 November 2026, with a frame of Orbital taking over a second: Rift Survivor's hull took 13½ minutes to
    # break, over QtTest's limit of five on a case, and tst_games about 50 minutes, over the 15 CTest gets below. tst_games'
    # limits on what a pilot may take follow the first of these (playingLimit).
    export QTEST_FUNCTION_TIMEOUT=${QTEST_FUNCTION_TIMEOUT:-1800000}
    SUITE_TIMEOUT=5400
    echo "== Drawn in software, expected: a pass here says nothing of a GPU, real sound or a real controller"
else
    export QOR_EXPECTED_RENDERER=gpu
    echo "== A GPU expected: a suite that finds itself drawn in software fails"
fi
export QT_FORCE_STDERR_LOGGING=1
export PATH=$QT/bin:$PATH

# What a result needs beside it: where it ran. A limit on the run is its own cgroup's, if it can be read from in here.
say_where() {
    local group limit
    echo "== At $(git -C "$REPO" rev-parse --short=12 HEAD 2> /dev/null || echo 'no commit known'), on $(uname -sr), $(nproc) processors"
    echo "== LP_NUM_THREADS=${LP_NUM_THREADS:-unset} QOR_EXPECTED_RENDERER=$QOR_EXPECTED_RENDERER WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-unset} QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-unset}"
    group=/sys/fs/cgroup$(sed -n 's/^0:://p' /proc/self/cgroup 2> /dev/null || true)
    while [ -n "$group" ] && [ "$group" != /sys/fs/cgroup ]; do
        for limit in cpu.max memory.high memory.max; do
            [ -r "$group/$limit" ] && echo "== $limit of ${group#/sys/fs/cgroup}: $(cat "$group/$limit")"
        done
        group=${group%/*}
    done
    awk '/^MemTotal/ { printf "== Memory in this system: %d MB\n", $2 / 1024 }' /proc/meminfo
}

# The display for the length of the run, products-display.qml or Prism, on
# the offscreen platform, which composes in memory. With no X display and a
# sound server that does not exist, nothing the suites do can reach the
# desktop. Its settings and data are the run's own, so a Prism the developer
# uses keeps theirs.
#
# The session bus is the run's own too, with a keyring on it,
# tools/dev/secret-service.py: QQ Studio keeps its agent's key in the Secret
# Service on Linux (QOR-0011), and the suites keep clear of the developer's.
DISPLAY_PID=
DISPLAY_HOME=
BUS_PID=
KEYRING_PID=
stop_display() {
    local pid
    for pid in "$DISPLAY_PID" "$KEYRING_PID" "$BUS_PID"; do
        [ -n "$pid" ] || continue
        kill "$pid" 2>/dev/null || true
        wait "$pid" 2>/dev/null || true
    done
    [ -z "$DISPLAY_HOME" ] || rm -rf "$DISPLAY_HOME"
}
start_keyring() {
    local log=$BUILD_ROOT/secret-service.log address
    if ! command -v dbus-daemon > /dev/null || ! python3 -c 'import dbus, gi' 2> /dev/null; then
        echo "No keyring for the run: it needs dbus-daemon, python3-dbus and python3-gi. QQ's agent suite will fail." >&2
        return 0
    fi
    { read -r address && read -r BUS_PID; } < <(dbus-daemon --session --fork --print-address=1 --print-pid=1)
    export DBUS_SESSION_BUS_ADDRESS=$address
    python3 "$REPO/tools/dev/secret-service.py" > "$log" 2>&1 &
    KEYRING_PID=$!
    local _
    for _ in $(seq 100); do
        grep -q '^Secret Service ready$' "$log" 2> /dev/null && return 0
        kill -0 "$KEYRING_PID" 2> /dev/null || break
        sleep 0.1
    done
    echo "The run's keyring did not start; its log is $log" >&2
    exit 1
}
start_display() {
    export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/run/user/$(id -u)}
    local socket=qor-products-$$ log=$BUILD_ROOT/display.log command
    if [ "$SHOW" = prism ]; then
        command=("$PRISM" --unlocked --socket "$socket" --size 1920x1080)
    else
        command=(qml "$REPO/tools/dev/products-display.qml" -- "$socket")
    fi
    DISPLAY_HOME=$(mktemp -d)
    trap stop_display EXIT
    mkdir -p "$BUILD_ROOT"
    start_keyring
    unset DISPLAY
    export QT_AUDIO_BACKEND=pulseaudio PULSE_SERVER=unix:$DISPLAY_HOME/no-sound
    XDG_CONFIG_HOME=$DISPLAY_HOME/config XDG_DATA_HOME=$DISPLAY_HOME/data \
        QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software "${command[@]}" > "$log" 2>&1 &
    DISPLAY_PID=$!
    local _
    for _ in $(seq 300); do
        [ -S "$XDG_RUNTIME_DIR/$socket" ] && break
        kill -0 "$DISPLAY_PID" 2>/dev/null || break
        sleep 0.1
    done
    if [ ! -S "$XDG_RUNTIME_DIR/$socket" ]; then
        echo "The $SHOW display did not start; its log is $log" >&2
        exit 1
    fi
    export WAYLAND_DISPLAY=$socket QT_QPA_PLATFORM=wayland
    echo "== The suites' display: ${command[0]##*/} at $socket, offscreen; its log is $log"
}
[ "$SHOW" = session ] || [ "$BUILD_ONLY" = yes ] || start_display
[ "$BUILD_ONLY" = yes ] || say_where

# What each suite of the attempt just made said: its counts, and every case that failed or skipped, by name, with why. In
# a job, the step's summary says it too.
report() { # name build
    local said
    said=$(python3 "$REPO/tools/dev/products-report.py" "$2") || return 0
    echo "== $1: what its suites said"
    printf '%s\n' "$said"
    if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
        { echo "### $1 on Linux${DRAWN:+, $DRAWN}"; echo '```'; printf '%s\n' "$said"; echo '```'; } >> "$GITHUB_STEP_SUMMARY"
    fi
}

# The QQ Player's floor of 20 frames a second is the one failure --accept-software-floor lets stand, and only where this script
# expected software and the Player says Mesa's software renderer drew it: tst_player the only suite that did not pass, ended
# normally with the one failure as its exit status, that check in that case its only failure, and the Player drawing at least a
# frame a second (products-report.py).
only_the_floor_failed() { # build
    [ "$ACCEPT_FLOOR" = yes ] && [ -n "$DRAWN" ] && python3 "$REPO/tools/dev/products-report.py" --only-the-floor "$1"
}

# CTest's record is written as a run ends, so one stopped part way would leave its log beside the last run's record.
# Invalidate it before configuring too: a failed configure or build must not leave old passing evidence looking current.
forget_the_last_record() { # build
    local testing=$1/Testing
    # A glob cannot enumerate an unreadable directory: rm -f would see no records and silently keep the old ones.
    if [ -e "$testing" ] && { [ ! -r "$testing" ] || [ ! -x "$testing" ]; }; then
        echo "Cannot inspect old CTest records in $testing, so nothing was run." >&2
        return 1
    fi
    rm -f "$testing"/*/Test.xml || return 1
}

build_and_test() { # name
    local name=$1 source=$REPO/${SOURCES[$1]} build=$BUILD_ROOT/$1
    forget_the_last_record "$build" || return 1
    local extra=()
    if [ "$name" = media ]; then
        local dir
        dir=$(pdfium) || return 1
        extra+=("-DPDFium_DIR=$dir")
    fi
    cmake -S "$source" -B "$build" -G Ninja -DCMAKE_PREFIX_PATH="$QT" -DCMAKE_BUILD_TYPE=$CONFIG \
        -DQT_QML_GENERATE_QMLLS_INI=OFF "${extra[@]}" || return 1
    cmake --build "$build" || return 1
    [ "$BUILD_ONLY" = no ] || return 0

    local attempt select=() drawing=()
    [ -z "$TESTS" ] || select=(-R "$TESTS")
    # ARQADE keeps one window through all its cases. On Qt's threaded render loop, on Wayland, with frames as slow as these,
    # each turn of the event loop then took one frame more than the last: 13 frames, 15 seconds, by the first lock of
    # Orbital's ten, and in time nothing else ever ran (docs/CI.md). On the basic loop each takes one.
    [ -z "$DRAWN" ] || [ "$name" != arqade ] || drawing=(QSG_RENDER_LOOP=basic)
    local failed=()
    for attempt in $(seq "$ATTEMPTS"); do
        forget_the_last_record "$build" || return 1
        if [ "$attempt" -eq 1 ]; then
            if env "${drawing[@]}" ctest --test-dir "$build" -T Test --no-tests=error --output-on-failure --timeout "$SUITE_TIMEOUT" "${select[@]}" \
                && python3 "$REPO/tools/dev/products-report.py" --passed "$build"; then
                report "$name" "$build"
                return 0
            fi
        else
            echo "== $name: the failed suites again (attempt $attempt of $ATTEMPTS): ${failed[*]}"
            # CTest's word is not enough: with nothing to run, or the wrong suites, it could still answer 0.
            if env "${drawing[@]}" ctest --test-dir "$build" -T Test --no-tests=error --output-on-failure --timeout "$SUITE_TIMEOUT" --rerun-failed "${select[@]}" \
                && python3 "$REPO/tools/dev/products-report.py" --passed-again "${failed[@]}" "$build"; then
                report "$name" "$build"
                RETRIED+=("$name: ${failed[*]}")
                return 0
            fi
        fi
        report "$name" "$build"
        if only_the_floor_failed "$build"; then
            echo "== $name: failed only the QQ Player's floor of 20 frames a second, drawn in software, which --accept-software-floor lets stand"
            FLOOR_STOOD+=("$name")
            return 0
        fi
        mapfile -t failed < <(python3 "$REPO/tools/dev/products-report.py" --failed "$build" || true)
    done
    return 1
}

FAILED=()
RETRIED=()
FLOOR_STOOD=()
IFS=, read -r -a WANTED <<< "$PRODUCTS"
for name in "${WANTED[@]}"; do
    [ -n "${SOURCES[$name]:-}" ] || { echo "Unknown product: $name (qq, arqade, media)" >&2; exit 2; }
    echo "== $name"
    build_and_test "$name" || FAILED+=("$name")
done

if [ -n "$DISPLAY_PID" ] && ! kill -0 "$DISPLAY_PID" 2>/dev/null; then
    echo "The $SHOW display stopped during the run, so what ran after had none; its log is $BUILD_ROOT/display.log"
    FAILED+=(display)
fi
for retried in "${RETRIED[@]}"; do
    echo "Failed once, and passed only on a later attempt: $retried"
done
[ ${#FLOOR_STOOD[@]} -eq 0 ] || echo "Failed the QQ Player's floor, let stand in software: ${FLOOR_STOOD[*]}"
if [ ${#FAILED[@]} -gt 0 ]; then
    echo "Failed: ${FAILED[*]}"
    exit 1
fi
if [ "$BUILD_ONLY" = yes ]; then
    echo "Products built: ${WANTED[*]}"
elif [ ${#FLOOR_STOOD[@]} -gt 0 ]; then
    echo "Products built and tested, and not all passed, ${DRAWN:-on a GPU}: ${WANTED[*]}; see above"
else
    echo "Products built and tested${DRAWN:+, $DRAWN}: ${WANTED[*]}"
fi

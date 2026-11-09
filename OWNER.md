# Where QOR OS stands

For Andrew, 9 November 2026. Accepted private image and compiler: clean 66df3f7.

The latest private installer ISO (boot media image) is 3,601,670,144 bytes.
Its checksum is 9251ca6d0799d987a30962faf32fb0278c9a03697210751cd377f7a2384fc575.
Evidence is in build/vm-ui-reboot-2026-11-08/isolated-ui-proof/.
It uses Fedora bootc (whole-image deployment), a checked offline OCI payload
(content-addressed image), a locked local qortest account and Qt's existing interface.

## Accepted private installation

The image installs on a fresh 8-GiB disk and reboots through the actual
installer button. Two boots without installation media prove welcome starts pending,
completes through its three pages and remains complete after one installed reboot.
All three virtual-machine (VM) phases exit 0 with three distinct boot identifiers.
The earlier feed626 Cancel, corrupt-payload refusal and two installed reboots remain
separate. Both normal UI failures and diagnostic cleanup are preserved.
The backend confines bootc's mounts and rechecks the strict disk guard before completion.
SELinux, the kernel's access-control system, stays enforcing without new permissions.

## Full device preparation

The full image guard now checks Prism's compiled capabilities, rather than its
filename alone. Its query runs without graphics or user-state writes. Simulator
and device binaries report authentication unavailable, and the guard refuses them.
Real sign-in and service daemons remain unfinished; this adds no unlock or session.
The accepted installer image is unchanged; no newer image is accepted.

## Checks and remaining work

All 18 Windows image cases and 21 Linux image/archive cases pass. The two command-line
suites pass 11/12 cases on Windows, Linux Release and ASan/UBSan (memory and
undefined-behaviour instrumentation), with no failures/skips. All ten existing QML
lint commands (static checks of Qt interfaces) pass. Hardening (exploit-protection checks) finds no shortcomings in
the four checked build folders.

The three Linux Prism builds exit 0, but their strict warning gates fail on 12
pre-existing warnings about Wayland (the display protocol) type information. Earlier logs reproduce them;
those failures remain open. Full OS/product and isolated-CI gates are unrun.

The build/VM RAM floor stays 3 GiB; bounded headless checks use a 1-GiB limit.
Failed preflights and harness attempts are preserved. Kintrek services are restored,
running or restarting; PostgreSQL is healthy. Disks and other checkouts are preserved.
Host boot, security, network and runner settings are unchanged.

Next: Prism's type metadata, then authenticated device startup and real daemons.
Hardware, Secure Boot, encryption, recovery, rollback, repeatable-build and release
gates remain open.

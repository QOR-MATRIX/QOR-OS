# Where QOR OS stands

For Andrew, 16 November 2026. The focused Pi graphics regression passes in
isolated normal and memory/lock test kernels. Physical Pi, QOR products,
hosted integration checks and Release acceptance remain separate and open.

The normal unpatched guest has five passes and the expected busy-poll failure:
all 16 immediate checks sleep once. The corrected normal guest passes all six
KUnit (Linux kernel unit) cases, with no voluntary scheduling in those polls.

The separate diagnostic configuration enables KASAN (kernel memory-error
detection), lockdep (lock dependency validation) and atomic-sleep checks.
Its primary run passes all six unchanged cases, with zero skips/timeouts,
verified instrumentation/runtime symbols and lockdep active throughout.
It exits in 150.437 seconds within the unchanged 300-second guest bound.
The 37 reviewed config changes preserve 16-kibibyte pages, 250 timer ticks
per second, original scheduling and 32-bit compatibility support.

The first debug build exposes a vendor USB declaration omission; a separate
test-only overlay moves that existing declaration without changing functions
or debug macros. Both kernel and seven modules then build without warnings.
The two-line V3D (Pi graphics driver) fix and six regression cases are unchanged.
An optional traced run times out after its complete passing primary; it remains
incomplete. A clean primary-only rerun retains every memory/lock check and
original bound. Unused loader-helper and compiler failures remain recorded.
No kernel fault-injection, lock torture or multicore stress is claimed.

Across diagnostic preparation, failed/retried builds, guest attempts and stock
inventory, 3930 resource samples stay at or above 6,934 MiB (mebibytes),
above the unchanged 3,072-MiB floor. Original QOR source/binary/image, host
package, running kernel and boot identities are checked at preservation.
Complete stock module trees are hashed; no media or boot action occurs.

The Pi still runs 6.18.50 while normal boot files select 6.18.60. Neither
offline backup readback nor a stock recovery boot is verified. A matched
6.18.50 recovery medium must boot before a separately approved production
trial; full production packaging and real GPU integration are still pending.
No candidate installation, physical boot or live module load is authorized.

The original Studio result remains three passes and one failure (`times > 5`
is three), with later notice/cursor assertions unreached. Its separate software
failure and 97 image outliers stay open. Native kernel checks do not qualify
application sanitizers, hosted CI (automated integration), images or Release.
Pull request #10 stays draft; consolidation planning remains separate.

Next: physically verify a matched stock 6.18.50 recovery boot on independent
media, under specific authorization. The canonical recovery/trial/return plan
is in `image/README.md`; receipts: `build/development-2026-11-16/session-11/`.

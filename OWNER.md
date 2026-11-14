# Where QOR OS stands

For Andrew, 14 November 2026. QQ's loaded-game collision with Qt's graphics
view is fixed, and its callers have been exercised on the separate Linux builder.

`Game.playingScene` now holds the loaded game; Qt keeps its own graphics-view
root. The Player and ARQADE use the new property, including arena polling.
The regression first reproduced the old collision and now checks loading,
replacement and release. Standalone test views own hidden windows so Qt can
retire their scene managers; queued destruction is drained between cases.

The full QQ build passes without compiler or license-client diagnostics.
Its scene suite passes 57 results. ASan/UBSan, the memory and undefined-behavior
checks, also pass all 57 offscreen with leak detection enabled. The four affected
QQ playback functions and four ARQADE caller functions each pass six Qt results,
including setup/cleanup, without failures or skips. ARQADE's checks include
opening and leaving a game, creating a match and receiving an opponent's join.
Executable hardening, the exploit-mitigation checks, passes for both products.
These are software-graphics results, not physical graphics, audio or input proof.
An evidence-wrapper child-status error is documented separately from the passing
commands and independent final source/cache/lock audit.

Limits remain explicit. QML, Qt's interface language, has 102 runtime lint
warnings. Store playback still exceeds 60 seconds; Orb Run's later timer wait
is unresolved. ARQADE retains build diagnostics and a context warning on return
from a game. Wayland, the display protocol, retains a 96-byte allocation under
sanitizers even in a numeric-only case. Its sanitizer exit remains failed.
No warning, leak check, test assertion or internal deadline was suppressed.

The branch `fix/native-play-clock-waits` also retains your approved six-wait
software-physics patch. Draft pull request #10 is unmerged. The other 13 QOR
checkouts were clean when checked. Upstream draft #118 remains at `846d9ba`
with its dependency audit failed; the fixed core-library revision is unchanged
at `6a4822d`. Both Qt CI workers, the automated build/test workers, are offline;
qualified isolation and full product acceptance remain outstanding.

Next: resolve the remaining playback, display-leak and lint findings, complete
the product suites, qualify isolated workers and clear the upstream audit.
The original job locks and 3-GiB memory floor remain. No unrelated service,
production deployment, device image, package or release was changed or published.

Current evidence is in `build/development-2026-11-13/session-11/`, including
initial failures and scope corrections; the approved timing proof remains in
session-10. The roadmap is the sole record of outstanding gates.

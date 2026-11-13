# Where QOR OS stands

For Andrew, 13 November 2026. Main is pushed at `3e1e2a6`, following merge
`0c45307`. New work is on `feat/developer-discovery`, not merged into main.

## Developer access

GitHub's homepage links the canonical documentation index. The README links
progress, the masterplan, developer guidance and security; all five milestones
were synchronized and read back. GitHub setup now handles international text
correctly on Windows and retains the homepage link. Its 23 local tests pass.
Main's hosted Lint and history scan pass. Release readiness fails because the
required Qt jobs remain paused; their workers are offline.

The new branch adds offline discovery for 14 integration areas. Python 3.12's
standard library supplies `list`, `show` and `check`, with structured JSON for
agents. Entries identify source, prerequisites and action boundaries. Validation
refuses malformed metadata, unsafe paths and misleading upstream links. The
tool reads no linked source content and performs no tool execution, sign-in,
network request, installation or device action. It is not the installable SDK
(software development kit) or an authority grant.

QOR Launcher and DRC-369 assets remain upstream in demiurge-chain. The current
QOR OS core bridge exposes four version/amount commands, without asset or agent
authorization operations. Source links identify the active implementations;
the frozen upstream root SDK is not a supported route. QQ's existing Model
Context Protocol (MCP) tools retain their same-user pipe and trust boundaries.

## Evidence and continuation

All 14 discovery tests pass locally, including the core-pin drift check. The
broader tooling run stopped at
3,061 MiB available RAM against the unchanged 3,072-MiB floor; its 72 completed
test lines do not constitute a suite pass. Local all-file hooks were held at
3,071 MiB; they and hosted Lint are pending. Seven pinned upstream source objects
and the changed guides' local links were verified separately. Evidence is retained
in `build/development-2026-11-13/session-04/`.

Next, specify and test read-only Launcher/DRC-369 bridge capabilities upstream,
then review the core pin and approval requirements. Reproducible app/asset
onboarding is the other developer priority. The Rust dependency warning, full
product/sanitizer and creator/player journeys, device acceptance and isolated Qt
CI (automated builds) remain open. Branch protection remains unavailable on the
private repository's current plan. No release or hardware gate was closed.

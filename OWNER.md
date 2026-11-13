# Where QOR OS stands

For Andrew, 13 November 2026. Main is pushed at `3e1e2a6`, after integration
merge `0c45307`. Developer discovery and domain policy are on
`feat/developer-discovery`; their integration status is recorded with the commit.

## Developer access and domains

GitHub's homepage field is verified as QORSYNC.DEV. The README links progress,
the masterplan, developer guidance and security. Five milestones were synchronized
and read back. QOR-0026 records the public domain and approved service subdomains.
Tracked-text checks refuse retired hosts, including encoded links, without
printing matched content. Legacy contact references were removed without
inventing replacement mailboxes.

Offline developer discovery covers 14 areas using Python's standard library.
`list`, `show` and `check` give agents structured source, prerequisite and action
boundary information. They perform no network, execution, account or device action.
The installable SDK, the software development kit, remains planned.

Launcher and DRC-369 asset work remain upstream. The core bridge, the shell's
command interface, has four version/amount commands only. Upstream domain source
maintenance is on `session/qorsync-domains`, with developer links and frozen
destinations corrected. Vercel confirms the homepage domain assignments and
Cloudflare nameservers, but project access fails. Service-domain migration,
mail, installed settings and released clients are not yet accepted.

## Evidence and continuation

Hosted Lint at `5a853c2` passed 706 Python tests with 124 platform skips and 63
ARQADE rule tests. Fourteen discovery tests passed on Windows; the initial domain
and GitHub-policy run passed 30 tests. The final expanded local validation was
refused at 3,064 MiB available against the unchanged 3,072-MiB floor. Earlier
incomplete local runs remain preserved, and final hosted checks are separate.
Upstream's 33 homepage checks and Launcher registry check pass locally. Evidence
is in `build/development-2026-11-13/session-04/`.

The pinned-core dependency audit failed with four vulnerability advisories and
13 warnings; upstream `74a1014` retains the affected versions. Remediate and test
upstream before adding bridge capabilities or moving the pin. No advisory was
ignored, and an inventory finding is not a demonstrated exploit.

Qt CI, automated native builds, stays paused pending isolated workers. Native
product/sanitizer journeys, hardware and release gates remain open; Q Pi stays
on hold. Branch protection is unavailable on the private repository's current
plan. No image, package, release or deployment was published.

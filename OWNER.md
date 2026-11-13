# Where QOR OS stands

For Andrew, 13 November 2026. Main is pushed at `3002419`, including integration
`0c45307` and developer/domain work through `0cdae3d`. The next branch is
`feat/core-audit-remediation`; it prepares the upstream repair procedure.

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
maintenance is in draft PR #118, with developer links and frozen destinations
corrected. Vercel confirms the homepage domain assignments and
Cloudflare nameservers, but project access fails. Service-domain migration,
mail, installed settings and released clients are not yet accepted.

## Evidence and continuation

Hosted Lint at `0cdae3d` passed 714 Python tests with 124 platform skips (838
total) and all 63 ARQADE rule tests, plus the configured source checks.
Fourteen discovery tests passed on Windows; the initial domain
and GitHub-policy run passed 30 tests. The final expanded local validation was
refused at 3,064 MiB available against the unchanged 3,072-MiB floor. Earlier
incomplete local runs remain preserved. Git's normal commit hooks passed.
Upstream's 33 homepage checks and Launcher registry check pass locally. Evidence
is in `build/development-2026-11-13/session-04/`.

The pinned-core dependency audit failed with four vulnerability advisories and
13 warnings; upstream `74a1014` retains the affected versions. Remediate and test
upstream before adding bridge capabilities or moving the pin. No advisory was
ignored, and an inventory finding is not a demonstrated exploit.
`docs/UPSTREAM.md` now gives the advisory-backed procedure, feature review and
required tests. Dependency edits and native verification are still outstanding.

Qt CI, automated native builds, stays paused pending isolated workers. Native
product/sanitizer journeys, hardware and release gates remain open; Q Pi stays
on hold. Branch protection is unavailable on the private repository's current
plan. No image, package or release was published. GitHub reports successful
automatic Vercel preview checks; no production promotion was performed.

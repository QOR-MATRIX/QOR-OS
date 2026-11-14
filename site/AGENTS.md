# QOR OS homepage

This folder is the canonical QORSYNC.DEV homepage source (QOR-0027). The existing
Vercel project is Astra Matrix / qor, building this repository's `site/` directory.
`hosting.json` records the non-secret expected binding; local linkage is not proof
of the remote Git connection or deployed revision.

Run `npm ci --ignore-scripts`, `npm run build`, `npm run check` and
`npm audit --audit-level=low` here with Node 24. Only `dist/` is public output.
Never deploy the repository root or upload environment files. Preserve the
same-origin content security policy and analytics privacy tests.

The imported source retains Apache-2.0; `upstream-source.json` records its hashes
before QOR OS changes. Mark changed carried files near their top. Retain font,
SDK and source notices in the built output. New QOR-only source follows root
SPDX rules. The existing signed Launcher/Mobile release URLs remain upstream
until their actual assets move; primary project/developer links point to QOR OS.

Maintain the existing visual design. Current facts come from OWNER and the sole
roadmap; a software test, private image or source change does not prove a public
release, hardware support or live deployment.

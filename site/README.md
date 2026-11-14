# QORSYNC.DEV homepage

The canonical homepage is developed in **QOR-MATRIX/QOR-OS**, in this folder.
Its existing Vercel project is [Astra Matrix / QOR](https://vercel.com/astramatrix/qor).
The production branch is `main`, project root `site`, and public output `dist`.
[QOR-0027](../docs/decisions/QOR-0027-the-homepage-lives-in-qor-os.md) records the
transfer from demiurge-chain; [HOSTING.md](../docs/HOSTING.md) owns the connection
procedure and verification evidence. `hosting.json` holds non-secret expected IDs.

## Develop and check

Use Node 24. From the repository root:

```sh
npm ci --prefix site --ignore-scripts --no-fund
npm run build --prefix site
npm run check --prefix site
npm audit --prefix site --audit-level=low
```

Serve `site/dist/` with a static server for local viewing. The site stays static,
without a framework: HTML content, the existing obsidian/gilt design, local fonts
and the WebGL background. Reduced-motion and hidden-tab behaviour are preserved.
The build copies an explicit list of public assets, notices and the unmodified
Analytics browser module. It does not serve the repository, package manifests,
development scripts or `node_modules/`. Install lifecycle scripts stay disabled.

The Homepage workflow runs the same checks without deployment credentials.
Content checks validate the canonical project links, real local document targets,
release hashes and expected hosting/build settings. SDK tests check redaction,
browser privacy opt-outs, preview/local exclusion and the public build boundary.
Dependabot proposes weekly package updates for review.

## Analytics

`@vercel/analytics` is pinned to 2.0.1 with a package integrity lock. Its plain
JavaScript entry point initialises only on `https://qorsync.dev/`. Do Not Track
and Global Privacy Control opt-outs prevent initialisation. Only homepage
pageviews are accepted: query strings, fragments and extra fields are removed;
custom events and other paths/origins are refused. No account integration exists.
The script and collection endpoints remain same-origin under the content security
policy; referrer policy is `no-referrer`, with development event logging disabled.
HTML responses set `Cache-Control: public, max-age=0, must-revalidate, no-transform`.
This prevents Cloudflare's automatic beacon injection from adding analytics outside
those controls, as described in its [setup guide](https://developers.cloudflare.com/web-analytics/get-started/).
Asset-specific cache lifetimes remain unchanged. Verify the delivered HTML against
the Git source after changing hosting; a successful origin build alone misses proxy edits.
Dashboard activation and a working live endpoint require separate verification.
See Vercel's [quickstart](https://vercel.com/docs/analytics/quickstart).

## Content and provenance

Update `index.html`, structured data and `llms.txt` together. Current QOR OS facts
come from OWNER and the sole roadmap. No image or public OS download is implied
by software qualification. Keep the existing style and accessible semantic HTML.
Images come from `scripts/render-images.mjs`; no new image render is needed for
copy-only ownership changes.

Existing Launcher/Mobile downloads remain the actual signed upstream release
assets. A release change updates the menu/hero buttons, download card, version,
size, SHA-256, structured data and `llms.txt` together, then `sitemap.xml`.
Never replace an asset URL with a nonexistent QOR OS release.

`upstream-source.json` records hashes carried from demiurge-chain `d4c51d7`.
Apache-2.0 source and OFL font notices accompany the built site; the Analytics
module carries MIT. Follow `AGENTS.md` for changes. This is the only active
homepage source; the upstream copy is a historical predecessor.

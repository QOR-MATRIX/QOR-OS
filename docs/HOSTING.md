# Homepage hosting

QOR OS's public homepage is `https://qorsync.dev`. Its source is this repository,
`QOR-MATRIX/QOR-OS`, under `site/` ([QOR-0027](decisions/QOR-0027-the-homepage-lives-in-qor-os.md)).
The chain, QOR ID, Social, ARQADE web application and pinned core remain upstream.

## Expected connection

| Setting | Value |
|---|---|
| GitHub repository | `QOR-MATRIX/QOR-OS` (repository ID `1435244138`) |
| Vercel dashboard | [Astra Matrix / QOR](https://vercel.com/astramatrix/qor) |
| Team | `astramatrix`, `team_gq3IWtz1kK0aO7kzMrrk6N6a` |
| Existing project | `qor`, `prj_pUNenZCSC3eh2tyJdHbHCt9rUlW0` |
| Production branch | `main` |
| Root directory / framework | `site` / Other (static site) |
| Node | `24.x` |
| Install | `npm ci --ignore-scripts --no-fund` |
| Build / output | `npm run build` / `dist` |
| Existing ignored-build command | `git diff --quiet HEAD^ HEAD -- .` (skips unchanged site content) |
| Public domains | `qorsync.dev`; `www.qorsync.dev` redirects to the apex with 308 |

These are non-secret identifiers, also recorded in `site/hosting.json`. Never
create a replacement project to work around inaccessible settings. Preserve the
existing domain bindings, credentials and production site while preparing changes.

## Verify and maintain

1. Use the authenticated Vercel connector when it can read this project. On
   13 November its project lookups returned 404 despite renewed team approval;
   the authenticated Vercel CLI could read the exact project and team. A connector
   404 is not proof of deletion or a need to recreate the project.
2. Inspect `qor` with explicit `--scope astramatrix`, and confirm the project ID
   above before changing anything. Never print full API project objects, which
   can include environment values; retain only selected non-secret configuration.
3. Check the connected Git repository's numeric ID and `main` branch. A local
   `.vercel/project.json` file alone is not proof of that remote connection.
4. Keep dashboard overrides equal to the checked-in `site/vercel.json`. The old
   no-op install/build commands and output `.` bypass the new build and are obsolete.
5. Run the homepage checks and inspect a preview before changing the live site.
   Verify the production deployment's repository, branch and commit, then HTTPS
   response, primary GitHub link, canonical metadata, redirects and security headers.
   Source changes alone are not proof of deployment or active analytics.

The source build publishes only `site/dist/`. No Qt binary, private repository
tree, dependency directory or environment file belongs in that output. This site
needs no application secret. Do not pull or copy production environment files as
part of local homepage work. Production changes remain within the owner's requested scope.

## Dated evidence

Initial authenticated inspection on 13 November found this same project and
domain mapping, but Git still pointed to demiurge-chain repository `1399861408`.
Root `site` and Node 24 were correct; install/build were no-ops and output was `.`.
PR #8 merged the migration at `1dc617bbd8851e8031587da7ccd400a0db3b2a24`.
Authenticated readback now confirms QOR OS repository `1435244138`, `main`,
root `site`, the build settings above and unchanged verified domain bindings.
The initial production alias served deployment `dpl_FH1YfDm24Cteyk4aitsu7G4E9pxH`,
whose Git source is that exact QOR OS main commit. Its preview built successfully;
five key assets matched reviewed source, apart from Vercel's identified preview toolbar.

Initial live checks confirmed the canonical repository link, HTTPS, six security headers,
the `www` 308 redirect, unavailable private build files and an HTTP 200 Analytics
script. Web Analytics is enabled on the existing project; dashboard collection
has not been exercised. Of 47 public assets, 45 matched Git/package bytes exactly.
Cloudflare added its automatic beacon to both HTML files; the same-origin security
policy blocked it. The header correction adds `no-transform`, documented by
[Cloudflare](https://developers.cloudflare.com/web-analytics/get-started/), to
prevent that injection while retaining the asset cache rules.

PR #9 subsequently merged that correction at
`f4c9863a282b21e0dcc2f86517fc29d77022fd8f`. The existing Git integration automatically
deployed it as `dpl_E41rwWtgMm8imti425U8YAkEaUMt`; authenticated readback confirms
the QORSYNC.DEV alias, QOR OS repository, `main` branch and exact commit agree.
All 47 delivered public files now match reviewed Git or locked-package bytes,
including both HTML pages, with no appended Cloudflare beacon. The HTML
`Cache-Control` value, six security headers, canonical links, `www` 308 redirect,
seven private-file 404 responses and HTTP 200 Analytics script all pass.
The local Node 24 build passes 36 content checks and six privacy/output tests;
the dependency audit reports no vulnerabilities. The protected preview also
matched five selected assets and all seven configured headers before merging.
Dashboard collection was not exercised. Current receipts are preserved in
`build/development-2026-11-13/session-09/`; native Qt readiness remains separate
and failed while its required isolated workers are unavailable.

The first test-only preview was correctly canceled by the existing ignored-build
command. The merged source revision built normally; no skip or security check was
disabled. Safe configuration, source and deployment receipts, the former live alias
for rollback, and the initial failed byte check are preserved in
`build/development-2026-11-13/session-08/`.

Official references: [Vercel Git connections](https://vercel.com/docs/cli/git)
and [project Git settings](https://vercel.com/docs/project-configuration/git-settings).

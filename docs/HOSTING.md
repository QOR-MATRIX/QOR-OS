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
The migration and final verification receipts belong in
`build/development-2026-11-13/session-08/`. Record the verified resulting commit
and deployment here when the cut-over completes.

Official references: [Vercel Git connections](https://vercel.com/docs/cli/git)
and [project Git settings](https://vercel.com/docs/project-configuration/git-settings).

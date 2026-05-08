# Workflow Notes

## Configure Pages DNS

`configure-pages-dns.yml` is a manual workflow for finishing the mcujs.org and
mcujs.com GitHub Pages cutover.

Use `mode=verify` first. It prints the Dynadot API request plan, checks current
DNS resolution, and attempts GitHub Pages health checks.

Use `mode=apply` only after:

- the repository secret `DYNADOT_API_KEY` exists, and
- the GitHub-hosted runner IP printed by the workflow is allowed in Dynadot API
  settings.

Dynadot API access is IP-restricted. If the GitHub-hosted runner IP is not
allowed, run the local helper from an already-whitelisted machine instead:

```bash
DYNADOT_API_KEY=... scripts/configure-pages-dns.sh --apply
scripts/configure-pages-dns.sh --verify
```

Use `mode=enable-https` only after DNS points to GitHub Pages and GitHub has
issued certificates for both custom domains.

## Docs Fallback Sync

The main `docs.yml` workflow can trigger the fallback mirror workflow in
`mcu-js/mcu-js.github.io` after a successful Pages deployment.

This is optional. It only runs when the `mcu-js/mcujs` repository has a
`FALLBACK_SYNC_TOKEN` secret that can call the repository dispatch API for
`mcu-js/mcu-js.github.io`.

Without that secret, `docs.yml` logs a skip message and continues normally.

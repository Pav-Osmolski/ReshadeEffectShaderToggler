# Maintain the documentation website

The site targets **https://pav-osmolski.github.io/ReshadeEffectShaderToggler/** and uses the same Zensical engine as the DS4Windows documentation. Canonical guides stay in `docs/`; the repository README stays a concise entry point. The site stages these files rather than maintaining separate copies of guide text.

## Local preview

Use Python 3.12+ in a separate environment:

```powershell
python -m venv .venv
./.venv/Scripts/python.exe -m pip install -r website/requirements.txt
./.venv/Scripts/python.exe website/prepare.py
./.venv/Scripts/zensical.exe build --clean
./.venv/Scripts/python.exe website/check.py
./.venv/Scripts/python.exe website/serve.py
```

Open `http://127.0.0.1:8765/ReshadeEffectShaderToggler/` in the local browser. Regenerate/rebuild after editing. No deployment is needed for review. Generated staging/output and the virtual environment are ignored by Git.

## Source layout

| Path | Purpose |
|---|---|
| `docs/` | Canonical Markdown, banner and historical records |
| `README.md` / `CHANGELOG.md` | Repository introduction and change history |
| `zensical.toml` | Navigation, site URL, theme and Markdown extensions |
| `website/overrides/` | Homepage and shared page metadata templates |
| `website/assets/` | Theme CSS and small REST icon |
| `website/prepare.py` | Stage docs, adapt repository links and generate release data |
| `website/check.py` | Check generated local links, anchors, assets and release controls |
| `website/releases.json` | Known published version fallback for offline previews |

The original WebP banner is copied without conversion. Historical notes and validation pages remain built/linked but are excluded from search ranking and marked noindex, so current guides are easier to find.

## Release data

Local preview uses the checked-in known-published release snapshot. `website/prepare.py --refresh-releases` uses the GitHub CLI (`gh`, available on GitHub-hosted runners) to query GitHub for stable Modern/Legacy releases and verifies their metadata; it does not infer publication from an untagged source version. Downloads link to release pages, so old `release.zip` assets and future versioned archives both remain accessible.

## GitHub Pages

The prepared workflow validates final PR review events targeting `main` and publishes documentation-affecting pushes from `main`. Legacy carries matching docs but does not publish a second competing site. Concurrency serialises publication and cancels superseded PR previews.

After approval, configure this repository's Pages source as **GitHub Actions** and allow the `github-pages` environment to deploy from `main`. Publication needs Pages write and an identity token; ordinary build jobs keep read-only repository access. Do not enable hosting or push this local overhaul before the preview is approved.

The release publisher explicitly requests a `main`-ref site refresh after either stable line is published, because GITHUB_TOKEN release events do not start another workflow automatically. A normal docs update uses the documentation workflow, not extra native release runs.

Before sign-off, check desktop/mobile layout, light/dark modes, search, sidebar navigation, code-copy controls, download destinations and the project subpath. [Release procedure](RELEASING.md).

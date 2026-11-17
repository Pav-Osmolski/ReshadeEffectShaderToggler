"""Stage canonical Markdown and assets; never change the source docs."""
import argparse
import json
from pathlib import Path
import posixpath
import re
import shutil
import subprocess
from urllib.parse import quote, unquote, urlsplit
import yaml

ROOT = Path(__file__).resolve().parents[1]
STAGE = ROOT / '.pages/docs'
REPO = 'https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler'


def destination(source):
    return Path('repository-readme.md') if source == Path('README.md') else source


def rewrite(match, source, documents):
    before, target, after = match.groups()
    parsed = urlsplit(target)
    if parsed.scheme or parsed.netloc or not parsed.path or parsed.path.startswith('/'):
        return match.group(0)
    resolved = Path(posixpath.normpath((source.parent / unquote(parsed.path)).as_posix()))
    if resolved.as_posix().startswith('../'):
        raise ValueError(f'Link escapes repository: {source}: {target}')
    if resolved in documents:
        target = posixpath.relpath(destination(resolved).as_posix(), destination(source).parent.as_posix())
    elif (ROOT / resolved).is_file() and not resolved.as_posix().startswith('docs/'):
        target = REPO + '/blob/main/' + quote(resolved.as_posix())
    elif (ROOT / resolved).is_dir() and not resolved.as_posix().startswith('docs/'):
        target = REPO + '/tree/main/' + quote(resolved.as_posix())
    else:
        return match.group(0)
    if parsed.fragment:
        target += '#' + parsed.fragment
    return before + target + after


def release_data(refresh):
    data = json.loads((ROOT / 'website/releases.json').read_text(encoding='utf-8'))
    if refresh:
        # Use the installed GitHub CLI's authenticated HTTPS configuration. It
        # also avoids relying on a separate Python CA bundle for local previews.
        releases = json.loads(subprocess.check_output([
            'gh', 'api', 'repos/Pav-Osmolski/ReshadeEffectShaderToggler/releases?per_page=100'],
            encoding='utf-8', timeout=30))
        for line, suffix in [('modern', '680'), ('legacy', '580')]:
            candidates = [r for r in releases if not r['draft'] and not r['prerelease']
                          and re.fullmatch(r'v\d+\.\d+\.\d+\.' + suffix, r['tag_name'])]
            if not candidates:
                raise RuntimeError(f'No stable {line} release returned by GitHub')
            release = max(candidates, key=lambda r: r['published_at'])
            data[line] = {'version': release['tag_name'][1:], 'url': release['html_url']}
    for line, suffix in [('modern', '680'), ('legacy', '580')]:
        entry = data[line]
        if not re.fullmatch(r'\d+\.\d+\.\d+\.' + suffix, entry['version']):
            raise ValueError(f'Invalid {line} version')
        if entry['url'] != REPO + '/releases/tag/v' + entry['version']:
            raise ValueError(f'Unexpected {line} release URL')
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--refresh-releases', action='store_true')
    args = parser.parse_args()
    releases = release_data(args.refresh_releases)
    expected = ROOT.resolve() / '.pages' / 'docs'
    if STAGE.resolve() != expected or not expected.is_relative_to(ROOT.resolve()):
        raise RuntimeError('Refusing to clear a directory outside the generated staging path')
    if STAGE.exists():
        shutil.rmtree(STAGE)
    STAGE.mkdir(parents=True)
    documents = {p.relative_to(ROOT) for p in (ROOT / 'docs').rglob('*.md')}
    documents.update([Path('README.md'), Path('CHANGELOG.md')])
    historical = {'maintenance-release-validation.md', 'release-candidate-validation-20261117.md',
                  'vulkan-ui-setting-validation-20261117.md'}
    for source in sorted(documents):
        text = (ROOT / source).read_text(encoding='utf-8-sig')
        if source == Path('README.md'):
            # GitHub parses Markdown inside this HTML block; the site renderer
            # needs an explicit attribute in its generated copy only.
            text = text.replace('<div align="center">', '<div align="center" markdown="1">')
        text = re.sub(r'(\]\()([^\s)]+)(\))', lambda m: rewrite(m, source, documents), text)
        if source.name in historical or (source.parts[:2] == ('docs', 'releases') and source.name != 'README.md'):
            text = '---\nsearch:\n  exclude: true\n---\n' + text
        target = STAGE / destination(source)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding='utf-8')
    for source in (ROOT / 'docs').rglob('*'):
        if source.is_file() and source.suffix != '.md':
            target = STAGE / source.relative_to(ROOT)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
    shutil.copytree(ROOT / 'website/assets', STAGE / 'assets')
    home = (ROOT / 'website/index.md').read_text(encoding='utf-8')
    values = {f'{line}_{field}': value for line, entry in releases.items() for field, value in entry.items()}
    home = home.replace('template: home.html', 'template: home.html\n' + yaml.safe_dump(values, sort_keys=False).strip())
    (STAGE / 'index.md').write_text(home, encoding='utf-8')
    (STAGE / 'assets/releases.json').write_text(json.dumps(releases, indent=2) + '\n', encoding='utf-8')
    (STAGE / 'robots.txt').write_text('User-agent: *\nAllow: /\nSitemap: https://pav-osmolski.github.io/ReshadeEffectShaderToggler/sitemap.xml\n', encoding='utf-8')
    print(f'Staged {len(documents)} canonical pages. Modern {releases["modern"]["version"]}; Legacy {releases["legacy"]["version"]}.')


if __name__ == '__main__':
    main()

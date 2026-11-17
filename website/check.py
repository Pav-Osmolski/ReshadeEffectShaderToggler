"""Check generated local links, fragment anchors, image assets and release controls."""
from html.parser import HTMLParser
import json
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
SITE = ROOT / '.pages/site'
PREFIX = '/ReshadeEffectShaderToggler/'


class Page(HTMLParser):
    def __init__(self, text):
        super().__init__()
        self.ids = set()
        self.links = []
        self.feed(text)

    def handle_starttag(self, tag, attrs):
        values = dict(attrs)
        if 'id' in values:
            self.ids.add(values['id'])
        if tag == 'a' and 'name' in values:
            self.ids.add(values['name'])
        for key in ('href', 'src'):
            if values.get(key):
                self.links.append(values[key])


def main():
    pages = {p: Page(p.read_text(encoding='utf-8')) for p in SITE.rglob('*.html')}
    if not pages or SITE / 'index.html' not in pages:
        raise RuntimeError('Build the documentation website first')
    errors = []
    checked = 0
    for source, page in pages.items():
        for link in page.links:
            parsed = urlsplit(link)
            if parsed.scheme or parsed.netloc:
                continue
            path = unquote(parsed.path)
            if path.startswith('/'):
                if not path.startswith(PREFIX):
                    errors.append(f'{source.relative_to(SITE)}: link outside project subpath: {link}')
                    continue
                target = SITE / path[len(PREFIX):]
            else:
                target = source.parent / path if path else source
            target = target.resolve()
            if not target.is_relative_to(SITE.resolve()):
                errors.append(f'{source.relative_to(SITE)}: link escapes site: {link}')
                continue
            if target.is_dir():
                target /= 'index.html'
            if not target.exists():
                errors.append(f'{source.relative_to(SITE)}: missing {link}')
                continue
            if parsed.fragment and target in pages and unquote(parsed.fragment) not in pages[target].ids:
                errors.append(f'{source.relative_to(SITE)}: missing anchor {link}')
            checked += 1
    home = (SITE / 'index.html').read_text(encoding='utf-8')
    releases = json.loads((SITE / 'assets/releases.json').read_text(encoding='utf-8'))
    for entry in releases.values():
        if entry['version'] not in home or entry['url'] not in home:
            errors.append('Homepage release version/link does not match staged metadata')
    for asset in ('assets/rest-mark.svg', 'docs/images/REST.webp'):
        if not (SITE / asset).is_file():
            errors.append('Missing brand asset: ' + asset)
    readme = (SITE / 'repository-readme/index.html').read_text(encoding='utf-8')
    if '<img' not in readme or 'docs/images/REST.webp' not in readme or '<h1' not in readme:
        errors.append('Repository README preview must render its banner and Markdown heading')
    if errors:
        raise RuntimeError('\n'.join(errors))
    print(f'Checked {len(pages)} HTML pages and {checked} local links/assets/fragments. Both release controls and brand assets are present.')


if __name__ == '__main__':
    main()

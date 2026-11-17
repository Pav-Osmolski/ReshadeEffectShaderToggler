"""Serve the local build under its real GitHub Pages project subpath."""
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit

SITE = Path(__file__).resolve().parents[1] / '.pages/site'
PREFIX = '/ReshadeEffectShaderToggler/'


class Handler(SimpleHTTPRequestHandler):
    def do_GET(self):
        if urlsplit(self.path).path == '/':
            self.send_response(302)
            self.send_header('Location', PREFIX)
            self.end_headers()
            return
        super().do_GET()

    def translate_path(self, path):
        if not urlsplit(path).path.startswith(PREFIX):
            return str(SITE / '__outside_project_subpath__')
        return super().translate_path(path[len(PREFIX)-1:])


if __name__ == '__main__':
    if not (SITE / 'index.html').exists():
        raise SystemExit('Build the documentation website first')
    print('Local preview: http://127.0.0.1:8765' + PREFIX, flush=True)
    ThreadingHTTPServer(('127.0.0.1', 8765), partial(Handler, directory=str(SITE))).serve_forever()

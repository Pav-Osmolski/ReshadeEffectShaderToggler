"""Exercise the real packager with isolated payloads, including failure cases."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

spec = importlib.util.spec_from_file_location('package_build', 'scripts/package-build.py')
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)


class PackagingTests(unittest.TestCase):
    def fixture(self, root):
        manifest = json.loads(Path('scripts/package-manifest.json').read_text(encoding='utf-8'))
        for mapping in manifest.values():
            for target, source in mapping.items():
                path = root / source
                if source in ('docs', 'Shaders'):
                    path /= 'example.txt'
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('Fixture: ' + target, encoding='utf-8')
        (root / 'src/version.h').write_text('#define REST_VERSION_STRING "1.7.2.680"\n')
        (root / 'scripts').mkdir(exist_ok=True)
        (root / 'scripts/package-manifest.json').write_text(json.dumps(manifest), encoding='utf-8')

    def test_ci_release_content_and_checksums(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self.fixture(root)
            commit = 'a' * 40
            packager.package(root, 'test', commit)
            packager.package(root, 'release', commit)
            with zipfile.ZipFile(root / 'test-package/REST-test-1.7.2.680-aaaaaaaaaaaa.zip') as ci, \
                 zipfile.ZipFile(root / 'release.zip') as release:
                self.assertEqual(set(ci.namelist()), set(release.namelist()))
                self.assertIn('CHANGELOG.md', release.namelist())
                self.assertIn('Shaders/example.txt', release.namelist())
                self.assertIn('docs/example.txt', release.namelist())
                for name in release.namelist():
                    if name != 'BUILD-INFO.txt':
                        self.assertEqual(ci.read(name), release.read(name))
                self.assertIn(commit, release.read('BUILD-INFO.txt').decode())
                self.assertIn('not a published stable release', ci.read('BUILD-INFO.txt').decode())
            for output in (root, root / 'test-package'):
                lines = (output / 'SHA256SUMS.txt').read_text().splitlines()
                self.assertEqual(len(lines), 2)
                for line in lines:
                    sha, name = line.split(maxsplit=1)
                    self.assertEqual(hashlib.sha256((output / name).read_bytes()).hexdigest(), sha)
                with zipfile.ZipFile(output / 'BG3-Configs-D3D11+Vulkan.zip') as configs:
                    self.assertIn('D3D11/ReshadeEffectShaderToggler.ini', configs.namelist())
                    self.assertIn('Vulkan/ReshadeEffectShaderToggler.ini', configs.namelist())
                    self.assertIn('PROVENANCE.json', configs.namelist())

    def test_missing_input_fails_before_archive_is_created(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self.fixture(root)
            (root / 'CHANGELOG.md').unlink()
            with self.assertRaises(FileNotFoundError):
                packager.package(root, 'release', 'a' * 40)
            self.assertFalse((root / 'release.zip').exists())

    def test_archive_traversal_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self.fixture(root)
            path = root / 'scripts/package-manifest.json'
            manifest = json.loads(path.read_text())
            manifest['main']['../outside.txt'] = 'README.md'
            path.write_text(json.dumps(manifest))
            with self.assertRaises(ValueError):
                packager.package(root, 'release', 'a' * 40)


if __name__ == '__main__':
    unittest.main()

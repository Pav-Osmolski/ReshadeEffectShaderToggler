"""Keep docs cheap without allowing release promotions to bypass native checks."""
import importlib.util
from pathlib import Path
import unittest
import yaml

spec = importlib.util.spec_from_file_location('ci_scope', 'scripts/ci-scope.py')
scope = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scope)


class CiScopeTests(unittest.TestCase):
    def test_docs_only(self):
        self.assertFalse(scope.needs_native(['README.md', 'docs/RELEASING.md'], 'pull_request'))

    def test_mixed_or_unknown_changes_build(self):
        for path in ('src/Main.cpp', 'deps/reshade', '.gitmodules', 'new-build-input',
                     '.github/workflows/release.yml', 'scripts/validate-binaries.ps1',
                     'tests/test_ci_scope.py', 'Shaders/Copy.fx'):
            with self.subTest(path=path):
                self.assertTrue(scope.needs_native(['README.md', path], 'pull_request'))

    def test_promotion_and_explicit_build_are_always_native(self):
        self.assertTrue(scope.needs_native(['docs/releases/v1.7.2.680.md'], 'push',
                                           'Release v1.7.2.680\n\nPromotion'))
        self.assertTrue(scope.needs_native([], 'workflow_dispatch'))

    def test_single_validation_workflow_and_release_gate(self):
        workflow = yaml.load(Path('.github/workflows/msbuild.yml').read_text(encoding='utf-8'),
                             Loader=yaml.BaseLoader)
        self.assertNotIn('synchronize', workflow['on']['pull_request']['types'])
        self.assertNotIn('paths', workflow['on']['pull_request'])
        self.assertIn('draft == false', workflow['jobs']['build']['if'])
        self.assertIn('github.event.pull_request.number || github.ref', workflow['concurrency']['group'])
        self.assertEqual(workflow['jobs']['tag']['needs'], 'build')
        self.assertIn("github.event_name == 'push'", workflow['jobs']['tag']['if'])
        self.assertFalse(Path('.github/workflows/workflow-checks.yml').exists())
        self.assertFalse(Path('.github/workflows/tag-release.yml').exists())


if __name__ == '__main__':
    unittest.main()

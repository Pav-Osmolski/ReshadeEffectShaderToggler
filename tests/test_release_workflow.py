"""Exercise the actual tagger script with isolated git/gh fakes; never touch GitHub."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import yaml


class ReleaseRoutingTests(unittest.TestCase):
    def run_tagger(self, branch='main', version='1.7.2.680', title=None,
                   existing=False, existing_sha=None, published=False, notes=True):
        workflow = yaml.safe_load(Path('.github/workflows/msbuild.yml').read_text(encoding='utf-8-sig'))
        script = workflow['jobs']['tag']['steps'][1]['run']
        bash = Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / 'Git/bin/bash.exe'
        if os.name != 'nt':
            bash = Path(shutil.which('bash'))
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root/'src').mkdir()
            (root/'src/version.h').write_text(f'#define REST_VERSION_STRING "{version}"\n', encoding='utf-8')
            (root/'docs/releases').mkdir(parents=True)
            if notes:
                (root/f'docs/releases/v{version}.md').touch()
            (root/'fake').mkdir()
            fake_git = '''#!/usr/bin/env bash
case "$1" in
  merge-base) exit 0 ;;
  show-ref) exit "$MOCK_TAG_STATUS" ;;
  rev-parse) printf '%s\\n' "$MOCK_TAG_SHA" ;;
  tag|push) printf 'git %s\\n' "$*" >> commands ;;
  *) exit 90 ;;
esac
'''
            fake_gh = '''#!/usr/bin/env bash
if [[ "$1 $2" == 'release view' ]]; then exit "$MOCK_RELEASE_STATUS"; fi
printf 'gh %s\\n' "$*" >> commands
'''
            for name, content in [('git', fake_git), ('gh', fake_gh)]:
                (root/'fake'/name).write_text(content, encoding='utf-8', newline='\n')
                (root/'fake'/name).chmod(0o755)
            (root/'run.sh').write_text('export PATH="$PWD/fake:$PATH"\n'+script, encoding='utf-8', newline='\n')
            env = dict(os.environ, RELEASE_BRANCH=branch, RELEASE_SHA='1'*40,
                       COMMIT_MESSAGE=title or f'Release v{version}',
                       MOCK_TAG_STATUS='0' if existing else '1',
                       MOCK_TAG_SHA=existing_sha or '1'*40,
                       MOCK_RELEASE_STATUS='0' if published else '1')
            result = subprocess.run([str(bash), '--noprofile', '--norc', 'run.sh'],
                                    cwd=root, env=env, capture_output=True, text=True)
            log = (root/'commands').read_text() if (root/'commands').exists() else ''
            return result, log

    def test_release_notes_only_promotion_triggers_build(self):
        workflow = yaml.load(Path('.github/workflows/msbuild.yml').read_text(encoding='utf-8-sig'), Loader=yaml.BaseLoader)
        self.assertIn('docs/releases/**', workflow['on']['push']['paths'])
        self.assertNotIn('synchronize', workflow['on']['pull_request']['types'])

    def test_new_modern_release(self):
        result, log = self.run_tagger()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('git push origin refs/tags/v1.7.2.680', log)
        self.assertIn('gh workflow run release.yml --ref v1.7.2.680', log)

    def test_new_legacy_release(self):
        result, log = self.run_tagger(branch='legacy/reshade-5.x', version='1.6.6.580')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('--ref v1.6.6.580', log)

    def test_existing_tag_same_commit(self):
        result, log = self.run_tagger(existing=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn('git push', log)
        self.assertIn('gh workflow run', log)

    def test_existing_tag_different_commit(self):
        result, log = self.run_tagger(existing=True, existing_sha='2'*40)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(log, '')

    def test_already_published_is_immutable(self):
        result, log = self.run_tagger(existing=True, published=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(log, '')

    def test_wrong_branch_suffix_is_rejected(self):
        result, log = self.run_tagger(branch='legacy/reshade-5.x')
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(log, '')

    def test_wrong_title_is_rejected(self):
        result, log = self.run_tagger(title='Release v1.7.2.680-unreviewed')
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(log, '')

    def test_missing_notes_is_rejected(self):
        result, log = self.run_tagger(notes=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(log, '')


if __name__ == '__main__':
    unittest.main()

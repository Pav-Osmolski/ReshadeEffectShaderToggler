"""Select native compilation from the complete event diff, never a truncated API list."""
import json
import os
from pathlib import Path
import subprocess


def needs_native(paths, event_name, message=''):
    if event_name == 'workflow_dispatch' or message.startswith('Release v'):
        return True
    # Unknown file types are conservative. Only known documentation/assets skip C++.
    def documentation(path):
        return (path.startswith(('docs/', '.github/ISSUE_TEMPLATE/'))
                or path in ('LICENSE', '.gitignore', '.gitattributes')
                or ('/' not in path and path.lower().endswith('.md')))
    return any(not documentation(path) for path in paths)


def main():
    event_name = os.environ['GITHUB_EVENT_NAME']
    event = json.loads(Path(os.environ['GITHUB_EVENT_PATH']).read_text(encoding='utf-8'))
    paths = []
    if event_name == 'pull_request':
        pr = event['pull_request']
        # Three-dot diff excludes unrelated changes on the target branch.
        revisions = [pr['base']['sha'] + '...' + pr['head']['sha']]
    elif event_name == 'push' and event.get('before', '').strip('0'):
        revisions = [event['before'], event['after']]
    else:
        revisions = None
    if revisions:
        result = subprocess.run(['git', 'diff', '--name-only', '-z', *revisions, '--'],
                                check=True, capture_output=True)
        paths = result.stdout.decode('utf-8').rstrip('\0').split('\0') if result.stdout else []
    # A new branch or unknown event has no reliable diff: perform full validation.
    native = revisions is None or needs_native(
        paths, event_name, event.get('head_commit', {}).get('message', ''))
    value = str(native).lower()
    with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as output:
        output.write(f'native={value}\n')
    with open(os.environ['GITHUB_STEP_SUMMARY'], 'a', encoding='utf-8') as output:
        output.write('Native x64/x86 build selected.\n' if native else
                     'Documentation-only change: workflow/script checks; native compilation skipped.\n')


if __name__ == '__main__':
    main()

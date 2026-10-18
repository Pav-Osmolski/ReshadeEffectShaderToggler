"""Reject malformed workflow YAML before it reaches a release trigger."""
from pathlib import Path
import yaml

class WorkflowLoader(yaml.SafeLoader):
    pass

# GitHub uses YAML 1.2: 'on' is a key, not the YAML 1.1 boolean True.
WorkflowLoader.yaml_implicit_resolvers = {
    key: [(tag, regex) for tag, regex in rules if tag != 'tag:yaml.org,2002:bool']
    for key, rules in yaml.SafeLoader.yaml_implicit_resolvers.items()
}

for path in sorted(Path('.github/workflows').glob('*.yml')):
    workflow = yaml.load(path.read_text(encoding='utf-8-sig'), Loader=WorkflowLoader)
    assert isinstance(workflow, dict) and 'on' in workflow and 'jobs' in workflow, path
    assert workflow.get('permissions') is not None, f'{path}: declare token permissions'
    for job in workflow['jobs'].values():
        assert 'timeout-minutes' in job, f'{path}: declare a timeout'
        for step in job.get('steps', []):
            assert ('run' in step) != ('uses' in step), f'{path}: invalid step'
    print(f'Validated {path}')

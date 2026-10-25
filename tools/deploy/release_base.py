"""Последний доступный опубликованный релиз, являющийся предком текущего коммита."""

import argparse
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
from staging_deploy import validate_release


def api(path):
    return json.loads(subprocess.check_output(['gh', 'api', path], text=True))


def previous_release(repository, revision):
    page = 1
    while True:
        runs = api(
            f'repos/{repository}/actions/workflows/services.yml/runs'
            f'?branch=main&event=workflow_dispatch&status=success&per_page=100&page={page}'
        )['workflow_runs']
        if not runs:
            return None
        for run in runs:
            ancestor = subprocess.run(
                ['git', 'merge-base', '--is-ancestor', run['head_sha'], revision], cwd=ROOT
            )
            if ancestor.returncode == 1:
                continue
            ancestor.check_returncode()
            artifacts = api(f"repos/{repository}/actions/runs/{run['id']}/artifacts?per_page=100")
            matches = [
                artifact
                for artifact in artifacts['artifacts']
                if artifact['name'] == f"staging-release-{run['head_sha']}"
                and not artifact['expired']
            ]
            if not matches:
                continue
            if len(matches) != 1:
                raise ValueError(f"Неоднозначный artifact релиза в запуске {run['id']}")
            archive = subprocess.check_output(
                ['gh', 'api', f"repos/{repository}/actions/artifacts/{matches[0]['id']}/zip"]
            )
            with zipfile.ZipFile(io.BytesIO(archive)) as contents:
                release = json.loads(contents.read('release.json'))
            validate_release(release)
            if release['revision'] != run['head_sha']:
                raise ValueError('revision в artifact не соответствует успешной публикации')
            print(f"Базовая публикация: {run['id']}, revision {release['revision']}", flush=True)
            return release
        page += 1


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repository', default=os.environ.get('GITHUB_REPOSITORY'), required=False)
    parser.add_argument('--revision', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if not args.repository:
        parser.error('Нужен --repository или GITHUB_REPOSITORY')
    args.output.unlink(missing_ok=True)
    release = previous_release(args.repository, args.revision)
    if release is None:
        print('Доступного базового релиза нет: требуется полная сборка', flush=True)
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(release, indent=2) + '\n')
        if output := os.environ.get('GITHUB_OUTPUT'):
            with open(output, 'a') as stream:
                stream.write(f'path={args.output}\n')

#!/usr/bin/env python3
"""Удалённое обновление staging после резервного копирования и проверки восстановления."""

import argparse
import base64
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import uuid

SERVICES = ('configs', 'broker', 'admin_bff')


def validate_release(release):
    if (
        not isinstance(release, dict)
        or not isinstance(release.get('revision'), str)
        or not re.fullmatch(r'[0-9a-f]{40}', release['revision'])
    ):
        raise ValueError('Релиз должен содержать полный git SHA')
    images = release.get('images')
    if not isinstance(images, dict) or set(images) != set(SERVICES):
        raise ValueError('Нужны образы трёх сервисов')
    for name, image in images.items():
        pattern = rf"ghcr\.io/makrus1/broker-{name.replace('_', '-')}@sha256:[0-9a-f]{{64}}"
        if not isinstance(image, str) or not re.fullmatch(pattern, image):
            raise ValueError(f'Неверный образ сервиса {name}')


def remote_main(settings):
    # Код этой функции запускается стандартным Python Ubuntu через SSH.
    import fcntl

    release = settings['release']
    validate_release(release)
    root = Path(settings['repository']).expanduser().resolve()
    state = root / '.staging'
    if not (state / 'credentials.json').is_file():
        raise ValueError('Нет credentials.json: команда обновляет только установленный staging')
    lock_path = Path.home() / '.cache/broker-staging/deploy.lock'
    lock_path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    with lock_path.open('a') as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise ValueError('Другой staging-деплой уже выполняется') from None

        def run(*command, capture=False):
            return subprocess.run(
                command,
                cwd=root,
                check=True,
                text=True,
                stdout=subprocess.PIPE if capture else sys.stderr,
                pass_fds=(lock.fileno(),),
            )

        if run('git', 'status', '--porcelain', capture=True).stdout.strip():
            raise ValueError('В checkout VM есть локальные изменения; деплой остановлен')
        run('git', 'fetch', 'origin', 'main')
        run('git', 'merge-base', '--is-ancestor', release['revision'], 'origin/main')
        previous = run('git', 'rev-parse', 'HEAD', capture=True).stdout.strip()
        kubectl = ['kubectl', '--context', settings['context'], '-n', settings['namespace']]
        os.environ['KUBECONFIG'] = str(Path.home() / '.kube/config')
        current = json.loads(run(*kubectl, 'get', 'deployments', '-o', 'json', capture=True).stdout)
        before = {
            'checkout': previous,
            'images': {
                item['metadata']['name']: item['spec']['template']['spec']['containers'][0]['image']
                for item in current['items']
            },
        }
        print(json.dumps({'status': 'ready', 'before': before}), flush=True)
        if sys.stdin.readline().strip() != 'APPLY':
            raise ValueError('Бэкап не подтверждён; обновление не выполнялось')

        history = state / 'releases'
        history.mkdir(mode=0o700, exist_ok=True)
        record = history / (
            datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S') + '-' + uuid.uuid4().hex[:8]
        )
        record.mkdir(mode=0o700)
        release_path = record / 'release.json'
        release_path.write_text(json.dumps(release, indent=2) + '\n')
        (record / 'before.json').write_text(json.dumps(before, indent=2) + '\n')
        try:
            run('git', 'checkout', '--detach', release['revision'])
            python = str(root / '.venv-gen/bin/python')
            run(python, '-m', 'pip', 'install', '-r', 'requirements.txt')
            run(python, 'scripts/staging.py', 'render', '--release', str(release_path))
            plan = json.loads((state / 'plan.json').read_text())
            if plan['namespace'] != settings['namespace']:
                raise ValueError('Namespace релиза отличается от namespace резервной копии')
            run(python, 'scripts/staging.py', 'apply', '--context', settings['context'])
        except BaseException:
            (record / 'status').write_text('failed\n')
            raise
        (record / 'status').write_text('success\n')
        print(json.dumps({'status': 'success', 'revision': release['revision']}), flush=True)


def deploy(release, host, context, namespace, repository, backup_root):
    from staging_backup import backup, verify

    validate_release(release)
    settings = dict(release=release, context=context, namespace=namespace, repository=repository)
    source = base64.b64encode(Path(__file__).read_bytes()).decode()
    payload = base64.b64encode(json.dumps(settings).encode()).decode()
    code = (
        'import base64,json;'
        f'exec(compile(base64.b64decode("{source}"),"staging_deploy.py","exec"),'
        'globals_dict:={"__name__":"broker_remote","__file__":"staging_deploy.py"});'
        f'globals_dict["remote_main"](json.loads(base64.b64decode("{payload}")))'
    )
    remote = subprocess.Popen(
        ['ssh', '-T', '-o', 'BatchMode=yes', host, 'python3 -u -c ' + shlex.quote(code)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        text=True,
    )
    try:
        line = remote.stdout.readline()
        if not line:
            raise ValueError('Удалённая подготовка не выполнена; смотри сообщения SSH выше')
        ready = json.loads(line)
        if ready.get('status') != 'ready':
            raise ValueError('VM не подтвердила готовность к резервному копированию')
        stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S') + '-' + uuid.uuid4().hex[:8]
        output = backup_root.expanduser().resolve() / stamp
        print(f'Резервная копия: {output}', flush=True)
        backup(output, context, host=host, namespace=namespace)
        verify(output)
        (output / 'before.json').write_text(json.dumps(ready['before'], indent=2) + '\n')
        (output / 'target-release.json').write_text(json.dumps(release, indent=2) + '\n')
        print('Восстановление проверено. Обновляю staging.', flush=True)
        remote.stdin.write('APPLY\n')
        remote.stdin.flush()
        remote.stdin.close()
        result = remote.stdout.read()
        if remote.wait() != 0:
            raise ValueError(f'Деплой завершился ошибкой. Бэкап сохранён: {output}')
        if json.loads(result).get('status') != 'success':
            raise ValueError('Не получено подтверждение готовности сервисов')
        print(f"Staging обновлён: {release['revision']}")
    finally:
        if not remote.stdin.closed:
            remote.stdin.close()
        # EOF снимает ожидание APPLY; при обрыве после APPLY состояние проверяется в VM.
        if remote.poll() is None:
            try:
                remote.wait(timeout=10)
            except subprocess.TimeoutExpired:
                remote.terminate()
                remote.wait()
        remote.stdout.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--release', type=Path, required=True)
    parser.add_argument('--backup-root', type=Path, required=True)
    parser.add_argument('--host', default='broker-staging')
    parser.add_argument('--context', required=True)
    parser.add_argument('--namespace', default='broker-staging')
    parser.add_argument('--repository', default='~/projects/Broker')
    args = parser.parse_args()
    deploy(
        json.loads(args.release.read_text()),
        args.host,
        args.context,
        args.namespace,
        args.repository,
        args.backup_root,
    )


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError, KeyboardInterrupt) as exc:
        raise SystemExit(str(exc) or 'Операция прервана')

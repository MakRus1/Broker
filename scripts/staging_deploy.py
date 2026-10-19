#!/usr/bin/env python3
"""Удалённое обновление staging после резервного копирования и проверки восстановления."""

import argparse
import io
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tarfile
import uuid

SERVICES = ('configs', 'broker', 'admin_bff')
DEPLOY_PROTOCOL = 2


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


def selection(services=None, infrastructure=False):
    services = list(SERVICES) if services is None else list(services)
    if len(set(services)) != len(services) or set(services) - set(SERVICES):
        raise ValueError('Неверный или повторяющийся сервис')
    if not services and not infrastructure:
        raise ValueError('Не выбраны сервисы или инфраструктура')
    return services


def deployment_state(items):
    result = {}
    for item in items:
        name = item['metadata']['name']
        if name not in {service.replace('_', '-') for service in SERVICES}:
            continue
        template = item['spec']['template']
        status = item.get('status', {})
        result[name] = {
            'image': template['spec']['containers'][0]['image'],
            'revision': template['metadata'].get('annotations', {}).get('broker/revision'),
            'generation': item['metadata'].get('generation'),
            'observed_generation': status.get('observedGeneration'),
            'ready_replicas': status.get('readyReplicas', 0),
            'replicas': item['spec'].get('replicas', 1),
            'volumes': template['spec'].get('volumes', []),
        }
    return result


def write_json(path, value):
    temporary = path.with_suffix('.tmp')
    temporary.touch(mode=0o600, exist_ok=True)
    temporary.write_text(json.dumps(value, indent=2) + '\n')
    temporary.replace(path)


def remote_main(settings):
    # Код этой функции запускается стандартным Python Ubuntu через SSH.
    import fcntl
    from staging_backup import backup, verify_cluster, prune

    release = settings['release']
    validate_release(release)
    services = selection(settings.get('services'), settings.get('infrastructure', False))
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
        if (state / 'lan/enabled').exists():
            try:
                run('git', 'cat-file', '-e', release['revision'] + ':scripts/staging_lan.py')
            except subprocess.CalledProcessError:
                raise ValueError('LAN включён: релиз без поддержки HTTPS запрещён') from None
        source = run(
            'git', 'show', release['revision'] + ':scripts/staging.py', capture=True
        ).stdout
        if '--infra-only' not in source or '--service' not in source:
            raise ValueError('Релиз не поддерживает выборочный деплой')
        previous = run('git', 'rev-parse', 'HEAD', capture=True).stdout.strip()
        kubectl = ['kubectl', '--context', settings['context'], '-n', settings['namespace']]
        os.environ['KUBECONFIG'] = str(Path.home() / '.kube/config')
        current = json.loads(run(*kubectl, 'get', 'deployments', '-o', 'json', capture=True).stdout)
        before = {
            'services': deployment_state(current['items']),
            'checkout': previous,
            'images': {
                item['metadata']['name']: item['spec']['template']['spec']['containers'][0]['image']
                for item in current['items']
            },
        }
        stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S') + '-' + uuid.uuid4().hex[:8]
        backup_root = Path(settings['backup_root']).expanduser().resolve()
        output = backup_root / stamp
        print(f'Резервная копия на сервере: {output}', flush=True)
        backup(output, settings['context'], namespace=settings['namespace'])
        (output / 'before.json').write_text(json.dumps(before, indent=2) + '\n')
        (output / 'target-release.json').write_text(json.dumps(release, indent=2) + '\n')
        image = json.loads((root / 'tools/local/images.json').read_text())['images']['postgres']
        verify_cluster(output, settings['context'], settings['namespace'], image)
        (output / 'verified').write_text(datetime.now(timezone.utc).isoformat() + '\n')
        print('Восстановление на сервере проверено. Обновляю staging.', flush=True)

        history = state / 'releases'
        history.mkdir(mode=0o700, exist_ok=True)
        record = history / (
            datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S') + '-' + uuid.uuid4().hex[:8]
        )
        record.mkdir(mode=0o700)
        release_path = record / 'release.json'
        release_path.write_text(json.dumps(release, indent=2) + '\n')
        (record / 'before.json').write_text(json.dumps(before, indent=2) + '\n')
        (record / 'backup-path').write_text(str(output) + '\n')
        write_json(
            record / 'selection.json',
            {'services': services, 'infrastructure': settings.get('infrastructure', False)},
        )
        try:
            run('git', 'checkout', '--detach', release['revision'])
            python = str(root / '.venv-gen/bin/python')
            run(python, '-m', 'pip', 'install', '-r', 'requirements.txt')
            run(python, 'scripts/staging.py', 'render', '--release', str(release_path))
            plan = json.loads((state / 'plan.json').read_text())
            if plan['namespace'] != settings['namespace']:
                raise ValueError('Namespace релиза отличается от namespace резервной копии')
            arguments = ['--service', services[0]] if services else ['--infra-only']
            for service in services[1:]:
                arguments += ['--service', service]
            if services and settings.get('infrastructure', False):
                arguments += ['--infrastructure']
            run(python, 'scripts/staging.py', 'apply', '--context', settings['context'], *arguments)
        except BaseException:
            (record / 'status').write_text('failed\n')
            raise
        finally:
            # Сохраняем состояние кластера даже при частично завершившемся обновлении.
            try:
                actual = json.loads(
                    run(*kubectl, 'get', 'deployments', '-o', 'json', capture=True).stdout
                )
                snapshot = {'services': deployment_state(actual['items']), 'record': record.name}
                write_json(record / 'after.json', snapshot)
                write_json(state / 'deployed.json', snapshot)
            except Exception as exc:
                (record / 'status').write_text('failed\n')
                raise RuntimeError('Не удалось сохранить фактическое состояние кластера') from exc
        (record / 'status').write_text('success\n')
        prune(backup_root, settings['keep_backups'])
        print(f"Staging обновлён: {release['revision']}. Бэкап: {output}", flush=True)


def deploy(
    release,
    host,
    context,
    namespace,
    repository,
    backup_root,
    keep_backups=7,
    *,
    services=None,
    infrastructure=False,
):
    validate_release(release)
    services = selection(services, infrastructure)
    if keep_backups < 1:
        raise ValueError('Нужно хранить хотя бы одну копию')
    settings = dict(
        release=release,
        services=services,
        infrastructure=infrastructure,
        context=context,
        namespace=namespace,
        repository=repository,
        backup_root=str(backup_root),
        keep_backups=keep_backups,
    )
    # Передаём только два скрипта и описание релиза во временный каталог VM.
    # Дампы и credentials.json не проходят через вызывающую машину.
    bundle = io.BytesIO()
    with tarfile.open(fileobj=bundle, mode='w') as archive:
        for name in ('staging_deploy.py', 'staging_backup.py'):
            archive.add(Path(__file__).with_name(name), arcname=name)
        data = json.dumps(settings).encode()
        item = tarfile.TarInfo('settings.json')
        item.size = len(data)
        item.mode = 0o600
        archive.addfile(item, io.BytesIO(data))
    command = '''set -eu
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT
trap 'exit 130' INT
trap 'exit 143' HUP TERM
tar -xf - -C "$temporary"
python3 -u "$temporary/staging_deploy.py" --remote-settings "$temporary/settings.json"
'''
    subprocess.run(
        ['ssh', '-T', '-o', 'BatchMode=yes', host, command],
        input=bundle.getvalue(),
        check=True,
    )


def main():
    if len(sys.argv) == 3 and sys.argv[1] == '--remote-settings':
        remote_main(json.loads(Path(sys.argv[2]).read_text()))
        return
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--service', action='append', choices=SERVICES)
    parser.add_argument('--infrastructure', action='store_true')
    parser.add_argument('--infra-only', action='store_true')
    parser.add_argument('--release', type=Path, required=True)
    parser.add_argument(
        '--backup-root',
        default='~/.local/share/broker-staging/backups',
        help='Каталог бэкапов на сервере',
    )
    parser.add_argument('--keep-backups', type=int, default=7)
    parser.add_argument('--host', default='broker-staging')
    parser.add_argument('--context', required=True)
    parser.add_argument('--namespace', default='broker-staging')
    parser.add_argument('--repository', default='~/projects/Broker')
    args = parser.parse_args()
    if args.infra_only and args.service:
        parser.error('--infra-only несовместим с --service')
    deploy(
        json.loads(args.release.read_text()),
        args.host,
        args.context,
        args.namespace,
        args.repository,
        args.backup_root,
        args.keep_backups,
        services=[] if args.infra_only else args.service,
        infrastructure=args.infrastructure or args.infra_only,
    )


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError, KeyboardInterrupt) as exc:
        raise SystemExit(str(exc) or 'Операция прервана')

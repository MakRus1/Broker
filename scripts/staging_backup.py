#!/usr/bin/env python3
"""Резервная копия staging на компьютере оператора и проверка восстановления в Docker."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
DATABASES = ('broker', 'configs', 'keycloak')


def checksum(path):
    digest = hashlib.sha256()
    with path.open('rb') as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def backup(output, context, *, host=None, namespace='broker-staging'):
    output = output.expanduser().resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        raise ValueError('Каталог копии уже существует: перезапись запрещена')
    temporary = Path(tempfile.mkdtemp(prefix='.backup-', dir=output.parent))

    def command(*args):
        cmd = [
            'kubectl',
            '--context',
            context,
            '-n',
            namespace,
            'exec',
            'postgres-0',
            '-c',
            'postgres',
            '--',
            *args,
        ]
        if host:
            # Kubeconfig остаётся в VM, дампы передаются бинарным SSH-потоком без TTY.
            return ['ssh', '-T', host, 'KUBECONFIG="$HOME/.kube/config" ' + shlex.join(cmd)]
        return cmd

    try:
        files = {}
        for database in DATABASES:
            path = temporary / f'{database}.dump'
            with path.open('xb') as target:
                os.chmod(path, 0o600)
                subprocess.run(
                    command('pg_dump', '-U', 'broker', '-Fc', '--no-password', database),
                    check=True,
                    stdout=target,
                )
            if path.stat().st_size == 0:
                raise ValueError(f'Пустая копия {database}')
            files[path.name] = checksum(path)
        version = subprocess.check_output(command('pg_dump', '--version'), text=True).strip()
        metadata = {
            'version': 1,
            'created_at': datetime.now(timezone.utc).isoformat(),
            'context': context,
            'namespace': namespace,
            'pg_dump': version,
            'files': files,
        }
        manifest = temporary / 'manifest.json'
        manifest.write_text(json.dumps(metadata, indent=2) + '\n')
        manifest.chmod(0o600)
        temporary.rename(output)
    finally:
        if temporary.exists():
            shutil.rmtree(temporary)
    return output


def validate(directory):
    metadata = json.loads((directory / 'manifest.json').read_text())
    expected = {f'{name}.dump' for name in DATABASES}
    if metadata.get('version') != 1 or set(metadata.get('files', {})) != expected:
        raise ValueError('Неверный manifest.json копии')
    for name, digest in metadata['files'].items():
        if checksum(directory / name) != digest:
            raise ValueError(f'Контрольная сумма {name} не совпадает')


def verify(directory):
    validate(directory)
    image = json.loads((ROOT / 'tools/local/images.json').read_text())['images']['postgres']
    name = 'broker-restore-check-' + uuid.uuid4().hex[:12]

    def docker(*args, **kwargs):
        return subprocess.run(['docker', *args], check=True, **kwargs)

    try:
        # Изолированный PostgreSQL: без портов, сети и постоянных томов.
        docker(
            'run',
            '-d',
            '--name',
            name,
            '--network',
            'none',
            '--tmpfs',
            '/var/lib/postgresql/data:rw',
            '-e',
            'POSTGRES_HOST_AUTH_METHOD=trust',
            image,
            stdout=subprocess.DEVNULL,
        )
        deadline = time.monotonic() + 60
        while subprocess.run(
            ['docker', 'exec', name, 'pg_isready', '-h', '127.0.0.1', '-U', 'postgres'],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        ).returncode:
            if time.monotonic() >= deadline:
                raise ValueError('Проверочный PostgreSQL не запустился за 60 секунд')
            time.sleep(1)
        for database in DATABASES:
            docker('exec', name, 'createdb', '-U', 'postgres', database)
            with (directory / f'{database}.dump').open('rb') as source:
                docker(
                    'exec',
                    '-i',
                    name,
                    'pg_restore',
                    '-U',
                    'postgres',
                    '--exit-on-error',
                    '--no-owner',
                    '--no-acl',
                    '-d',
                    database,
                    stdin=source,
                )
    finally:
        subprocess.run(['docker', 'rm', '-f', '-v', name], stdout=subprocess.DEVNULL, check=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    create = commands.add_parser('create')
    create.add_argument('--output', type=Path, required=True)
    create.add_argument('--context', required=True)
    create.add_argument(
        '--host', help='SSH-алиас VM из ~/.ssh/config; без него используется локальный kubectl'
    )
    create.add_argument('--namespace', default='broker-staging')
    check = commands.add_parser('verify')
    check.add_argument('directory', type=Path)
    args = parser.parse_args()
    if args.command == 'create':
        print(backup(args.output, args.context, host=args.host, namespace=args.namespace))
    else:
        verify(args.directory.expanduser().resolve())
        print('Контрольные суммы и восстановление трёх БД проверены. Контейнер удалён.')


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as exc:
        raise SystemExit(str(exc))

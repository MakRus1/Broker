#!/usr/bin/env python3
"""Серверная проверка манифестов на временном K3s без доступа к пользовательским кластерам."""

import argparse
import json
import tempfile
from pathlib import Path
import subprocess
import time
import uuid

K3S_IMAGE = 'rancher/k3s:v1.36.5-k3s1@sha256:057cfd5a9d51b89f1f0ee139c28d5d5fdd5aa7ddeac2742a0ba90c6dbc0329bb'


def wait_for_runtime_node(command, timeout=120):
    # /readyz подтверждает готовность API, но kubelet регистрируется позже.
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        nodes = subprocess.run(
            [*command, 'get', 'nodes', '-o', 'name'], capture_output=True, text=True
        )
        if nodes.returncode == 0 and nodes.stdout.strip():
            remaining = max(1, int(deadline - time.monotonic()))
            subprocess.run(
                [
                    *command, 'wait', '--for=condition=Ready', 'node', '--all',
                    f'--timeout={remaining}s',
                ],
                check=True,
            )
            return
        time.sleep(min(2, max(0, deadline - time.monotonic())))
    raise RuntimeError('Узел временного K3s не зарегистрировался за отведённое время')


def check(directory, runtime=False):
    name = 'broker-schema-' + uuid.uuid4().hex[:12]
    try:
        subprocess.run(
            [
                'docker',
                'run',
                '-d',
                '--privileged',
                '--cgroupns=host',
                '--volume',
                '/sys/fs/cgroup:/sys/fs/cgroup:rw',
                '--name',
                name,
                '--tmpfs',
                '/run',
                '--tmpfs',
                '/var/run',
                K3S_IMAGE,
                'server',
                '--disable',
                'traefik',
                '--disable',
                'servicelb',
            ],
            check=True,
        )
        command = ['docker', 'exec', '-i', name, 'kubectl']
        apply_flags = ['--server-side', '--field-manager=broker-staging', '--force-conflicts']
        for attempt in range(60):
            ready = subprocess.run([*command, 'get', '--raw=/readyz'], capture_output=True)
            if ready.returncode == 0:
                break
            time.sleep(2)
        else:
            raise RuntimeError('Временный K3s не запустился')
        if runtime:
            wait_for_runtime_node(command)
        plan = json.loads((directory / 'plan.json').read_text())
        version = subprocess.run(
            [*command, 'version', '-o', 'json'], check=True, text=True, capture_output=True
        )
        if json.loads(version.stdout)['serverVersion']['gitVersion'] != plan['kubernetes_version']:
            raise RuntimeError('Версия проверочного K3s отличается от platform.yaml')
        for phase in ('namespace', 'infrastructure', 'migrations', 'services'):
            args = [*command, 'apply', *apply_flags, '-f', '-']
            if phase != 'namespace':
                args.append('--dry-run=server')
            subprocess.run(args, input=(directory / f'{phase}.yaml').read_bytes(), check=True)
        if runtime:
            release = json.loads((directory / 'release.json').read_text())
            tags = [
                f"ghcr.io/makrus1/broker-{service.replace('_', '-')}:{release['revision']}"
                for service in release['images']
            ]
            with tempfile.TemporaryFile() as archive:
                subprocess.run(['docker', 'save', *tags], stdout=archive, check=True)
                archive.seek(0)
                subprocess.run(
                    ['docker', 'exec', '-i', name, 'ctr', 'images', 'import', '-'],
                    stdin=archive,
                    check=True,
                )
            for tag, image in zip(tags, release['images'].values()):
                subprocess.run(
                    ['docker', 'exec', name, 'ctr', 'images', 'tag', tag, image], check=True
                )
            plan = json.loads((directory / 'plan.json').read_text())
            ns = ['-n', plan['namespace']]

            def run(*args):
                subprocess.run([*command, *args], check=True)

            subprocess.run(
                [*command, 'apply', *apply_flags, '-f', '-'],
                input=(directory / 'infrastructure.yaml').read_bytes(),
                check=True,
            )
            run(*ns, 'rollout', 'status', 'statefulset/postgres', '--timeout=300s')
            run(*ns, 'rollout', 'status', 'deployment/keycloak', '--timeout=300s')
            subprocess.run(
                [*command, 'apply', *apply_flags, '-f', '-'],
                input=(directory / 'migrations.yaml').read_bytes(),
                check=True,
            )
            for job in plan['jobs']:
                run(*ns, 'wait', '--for=condition=complete', 'job/' + job, '--timeout=180s')
            subprocess.run(
                [*command, 'apply', *apply_flags, '-f', '-'],
                input=(directory / 'services.yaml').read_bytes(),
                check=True,
            )
            for service in release['images']:
                run(
                    *ns,
                    'rollout',
                    'status',
                    'deployment/' + service.replace('_', '-'),
                    '--timeout=300s',
                )
            run(*ns, 'get', 'pods,pvc')

    except Exception:
        # Сохраняем причину остановки до удаления временного кластера.
        subprocess.run(['docker', 'stats', '--no-stream'], check=False)
        subprocess.run(['docker', 'inspect', '--format', '{{json .State}}', name], check=False)
        subprocess.run(['docker', 'logs', '--tail', '100', name], check=False)
        raise
    finally:
        subprocess.run(['docker', 'rm', '-f', '-v', name], check=False, capture_output=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, default=Path('.staging'))
    parser.add_argument('--runtime', action='store_true')
    args = parser.parse_args()
    check(args.directory, args.runtime)

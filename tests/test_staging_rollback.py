"""Откат восстанавливает один сервис и никогда не применяет миграции."""

from copy import deepcopy
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import service_history
import staging_rollback


def objects(service, revision):
    metadata = {
        'name': service.replace('_', '-'),
        'namespace': 'ns',
        'generation': 1,
        'labels': {'broker/service': service},
    }
    deployment = {
        'apiVersion': 'apps/v1',
        'kind': 'Deployment',
        'metadata': deepcopy(metadata),
        'spec': {
            'replicas': 1,
            'template': {
                'metadata': {'annotations': {'broker/revision': revision * 40}},
                'spec': {
                    'containers': [
                        {
                            'name': service,
                            'image': f'ghcr.io/makrus1/broker-{service}@sha256:' + revision * 64,
                        }
                    ],
                    'volumes': [
                        {'name': 'config', 'configMap': {'name': f'{service}-config-{revision}'}},
                        {'name': 'vars', 'secret': {'secretName': f'{service}-vars-{revision}'}},
                    ],
                },
            },
        },
        'status': {
            'readyReplicas': 1,
            'updatedReplicas': 1,
            'availableReplicas': 1,
            'observedGeneration': 1,
        },
    }
    svc = {
        'apiVersion': 'v1',
        'kind': 'Service',
        'metadata': deepcopy(metadata),
        'spec': {'ports': [{'port': 8080}], 'selector': {'service': service}},
    }
    return [deployment, svc]


def snapshot(service, revision, healthy=True):
    return {
        'healthy': healthy,
        'objects': [service_history.clean(obj) for obj in objects(service, revision)],
    }


class HistoryTest(unittest.TestCase):
    def test_default_uses_previous_distinct_healthy_state(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            record = root / '20261019T200000-aaaaaaaa'
            record.mkdir()
            (record / 'status').write_text('success')
            (record / 'before-services.json').write_text(
                json.dumps({'broker': snapshot('broker', 'a')})
            )
            (record / 'after-services.json').write_text(
                json.dumps({'broker': snapshot('broker', 'b')})
            )
            identifier, value = service_history.choose(root, 'broker', snapshot('broker', 'b'))
            self.assertTrue(identifier.endswith(':before'))
            self.assertEqual(value, snapshot('broker', 'a'))
            (record / 'status').write_text('failed')
            with self.assertRaises(ValueError):
                service_history.choose(
                    root, 'broker', snapshot('broker', 'b'), record.name + ':after'
                )
            self.assertTrue(
                service_history.choose(root, 'broker', snapshot('broker', 'b'))[0].endswith(
                    ':before'
                )
            )
            with self.assertRaises(ValueError):
                service_history.choose(root, 'broker', snapshot('broker', 'b'), '../escape:before')

    def test_foreign_objects_are_rejected(self):
        value = snapshot('configs', 'a')
        with self.assertRaises(ValueError):
            service_history.validate_target(value, 'broker', 'ns')
        value = snapshot('broker', 'a')
        value['objects'].append({'kind': 'Job'})
        with self.assertRaises(ValueError):
            service_history.validate_target(value, 'broker', 'ns')

    def test_incomplete_rollout_is_not_a_healthy_snapshot(self):
        raw = objects('broker', 'a')
        raw[0]['status']['updatedReplicas'] = 0
        run = Mock(return_value=Mock(stdout=json.dumps({'items': raw})))
        self.assertFalse(
            service_history.capture(['kubectl'], ['broker'], run=run)['broker']['healthy']
        )


class RollbackTest(unittest.TestCase):
    def scenario(self, *, preview=False, fail=False, missing_config=False, lock_busy=False):
        import fcntl

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in ('broker', 'configs'):
                path = root / f'services/{name}'
                path.mkdir(parents=True)
                (path / 'service.yaml').write_text('version: 1')
            record = root / '.staging/releases/20261019T200000-aaaaaaaa'
            record.mkdir(parents=True)
            (record / 'status').write_text('success')
            (record / 'before-services.json').write_text(
                json.dumps({'broker': snapshot('broker', 'a')})
            )
            cluster = objects('broker', 'b') + objects('configs', 'b')
            neighbor = deepcopy(cluster[2:])
            commands = []

            def run(command, **kwargs):
                commands.append(command)
                if 'deployments,services' in command or 'deployments' in command:
                    return Mock(
                        stdout=json.dumps(
                            {
                                'items': (
                                    cluster
                                    if 'deployments,services' in command
                                    else [obj for obj in cluster if obj['kind'] == 'Deployment']
                                )
                            }
                        )
                    )
                if 'jsonpath={.immutable}' in command:
                    if missing_config:
                        raise subprocess.CalledProcessError(1, command)
                    return Mock(stdout='true')
                if 'apply' in command:
                    items = json.loads(kwargs['input'])['items']
                    self.assertEqual({obj['kind'] for obj in items}, {'Deployment', 'Service'})
                    self.assertTrue(all(obj['metadata']['name'] == 'broker' for obj in items))
                    for obj in items:
                        index = next(
                            i
                            for i, current in enumerate(cluster)
                            if current['kind'] == obj['kind']
                            and current['metadata']['name'] == 'broker'
                        )
                        status = cluster[index].get('status')
                        cluster[index] = deepcopy(obj)
                        cluster[index]['metadata']['generation'] = 2
                        if status:
                            cluster[index]['status'] = {**status, 'observedGeneration': 2}
                    if fail:
                        raise subprocess.CalledProcessError(1, command)
                return Mock(stdout='')

            lock_path = root / '.cache/broker-staging/deploy.lock'
            lock_path.parent.mkdir(parents=True)
            with lock_path.open('a') as held, patch.dict(
                os.environ, {'GITHUB_STEP_SUMMARY': ''}
            ), patch('staging_rollback.Path.home', return_value=root), patch(
                'staging_rollback.subprocess.run', side_effect=run
            ), patch(
                'sys.stdout', new_callable=io.StringIO
            ):
                if lock_busy:
                    fcntl.flock(held, fcntl.LOCK_EX | fcntl.LOCK_NB)
                if fail or missing_config or lock_busy:
                    with self.assertRaises((subprocess.CalledProcessError, ValueError)):
                        staging_rollback.execute(root, 'ctx', 'ns', 'broker', apply=not preview)
                else:
                    staging_rollback.execute(root, 'ctx', 'ns', 'broker', apply=not preview)
            self.assertEqual(cluster[2:], neighbor)
            self.assertFalse(
                any(
                    'pg_restore' in command
                    or 'psql' in command
                    or any('migrate' in str(arg) for arg in command)
                    for command in commands
                )
            )
            if preview or missing_config or lock_busy:
                self.assertFalse(any('apply' in command for command in commands))
                self.assertEqual(len(list(record.parent.iterdir())), 1)
            else:
                created = next(path for path in record.parent.iterdir() if path != record)
                self.assertEqual(
                    (created / 'status').read_text().strip(), 'failed' if fail else 'success'
                )
                self.assertTrue((created / 'after-services.json').exists())
                self.assertTrue((root / '.staging/deployed.json').exists())
                restored = next(
                    obj
                    for obj in cluster
                    if obj['kind'] == 'Deployment' and obj['metadata']['name'] == 'broker'
                )
                self.assertTrue(
                    restored['spec']['template']['spec']['containers'][0]['image'].endswith(
                        'a' * 64
                    )
                )

    def test_apply_restores_only_selected_service_and_saves_state(self):
        self.scenario()

    def test_preview_does_not_mutate(self):
        self.scenario(preview=True)

    def test_missing_immutable_configuration_stops_before_apply(self):
        self.scenario(missing_config=True)

    def test_partial_failure_saves_observed_state(self):
        self.scenario(fail=True)

    def test_shared_lock_prevents_overlap_with_deploy(self):
        self.scenario(lock_busy=True)

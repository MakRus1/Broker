"""Кластеры БД, изоляция владельцев и перенос без изменения исходных манифестов."""

import base64
from copy import deepcopy
import json
from pathlib import Path
import sys
import tempfile
import unittest
import yaml
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from platform_resources import load
import staging_postgres as postgres
from staging_postgres_migrate import import_manifest
from staging_manifest import render
from test_deployment import RELEASE


class PostgresTest(unittest.TestCase):
    def test_operator_install_uses_verified_local_manifest_without_network(self):
        with patch('urllib.request.urlopen', side_effect=AssertionError('network forbidden')), patch.object(
            postgres.subprocess, 'run'
        ) as run:
            postgres.install(ROOT, ['kubectl'])
        self.assertEqual(run.call_count, 3)
        self.assertEqual(run.call_args_list[0].args[0],
            ['kubectl', 'apply', '--server-side', '-f', '-'])
        objects = list(yaml.safe_load_all(run.call_args_list[0].kwargs['input']))
        deployment = next(obj for obj in objects if obj['kind'] == 'Deployment'
            and obj['metadata']['name'] == 'cnpg-controller-manager')
        profile = load(ROOT / 'tools/deploy/postgres/platform.yaml', 'staging')['application']
        self.assertEqual(deployment['spec']['replicas'], profile['replicas'])
        self.assertEqual(deployment['spec']['template']['spec']['containers'][0]['resources'],
            profile['resources'])

    def test_operator_checksum_mismatch_stops_before_cluster_mutations(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            target = root / 'tools/deploy/postgres'
            target.mkdir(parents=True)
            (target / 'operator.json').write_text(
                (ROOT / 'tools/deploy/postgres/operator.json').read_text())
            (target / 'operator.yaml').write_text('changed manifest')
            with patch.object(postgres.subprocess, 'run') as run:
                with self.assertRaisesRegex(ValueError, 'SHA256'):
                    postgres.install(root, ['kubectl'])
            run.assert_not_called()

    def test_each_owner_has_independent_cluster_secret_and_connection(self):
        with tempfile.TemporaryDirectory() as directory:
            phases = render(RELEASE, Path(directory))
            clusters = [obj for obj in phases['databases'] if obj['kind'] == 'Cluster']
            passwords = [
                obj['stringData']['password']
                for obj in phases['databases']
                if obj['kind'] == 'Secret'
            ]
            self.assertEqual(len(clusters), len(set(passwords)))
            self.assertFalse(
                any(obj['kind'] == 'StatefulSet' for obj in phases['infrastructure'])
            )
            vars = next(
                obj
                for obj in phases['services']
                if obj['kind'] == 'Secret'
                and obj['metadata']['name'].startswith('finance-vars')
            )
            self.assertIn(
                'finance-postgres-rw.broker-staging.svc.cluster.local',
                base64.b64decode(vars['data']['config_vars.yaml']).decode(),
            )
            for cluster in clusters:
                owner = cluster['metadata']['labels']['broker/database']
                self.assertEqual(cluster['spec']['bootstrap']['initdb']['database'], owner)
                self.assertEqual(cluster['metadata']['name'], postgres.cluster_name(owner))
            again = render(RELEASE, Path(directory))
            self.assertEqual(again['databases'], phases['databases'])

    def test_three_instances_require_different_nodes_and_individual_storage(self):
        profile = load(ROOT / 'services/finance/platform.yaml', 'staging', postgres=True)[
            'postgres'
        ]
        profile['instances'] = 3
        obj = postgres.render('finance', profile, 'test', 'local-path', 'image', 'password')[1]
        self.assertEqual(obj['spec']['instances'], 3)
        self.assertEqual(obj['spec']['affinity']['podAntiAffinityType'], 'required')
        self.assertEqual(obj['spec']['storage']['size'], '10Gi')

    def test_migration_imports_one_database_and_preserves_regular_manifest(self):
        profile = load(ROOT / 'services/finance/platform.yaml', 'staging', postgres=True)[
            'postgres'
        ]
        objects = postgres.render('finance', profile, 'test', 'local-path', 'image', 'password')
        before = deepcopy(objects)
        imported = import_manifest(objects, ['finance'])
        self.assertEqual(
            imported[1]['spec']['bootstrap']['initdb']['import']['databases'], ['finance']
        )
        self.assertEqual(objects, before)

    def test_primary_is_resolved_again_after_failover(self):
        with patch.object(
            postgres.subprocess,
            'check_output',
            side_effect=[
                json.dumps({'status': {'currentPrimary': f'finance-postgres-{n}'}})
                for n in (1, 2)
            ],
        ):
            self.assertEqual(postgres.primary(['kubectl'], 'finance'), 'finance-postgres-1')
            self.assertEqual(postgres.primary(['kubectl'], 'finance'), 'finance-postgres-2')
        with patch.object(postgres.subprocess, 'check_output', return_value='{"status": {}}'):
            with self.assertRaises(ValueError):
                postgres.primary(['kubectl'], 'finance')


class MigrationSafetyTest(unittest.TestCase):
    def test_failed_backup_verification_keeps_source_and_does_not_create_targets(self):
        import staging_postgres_migrate as migration

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'plan.json').write_text(
                json.dumps(
                    {
                        'namespace': 'test',
                        'environment': 'staging',
                        'database_owners': ['finance'],
                    }
                )
            )
            (root / 'release.json').write_text('{"images": {"finance": "image"}}')
            replies = [
                json.dumps({'items': []}),
                json.dumps({'spec': {'replicas': 1}}),
                json.dumps(
                    {'items': [{'metadata': {'name': 'finance'}, 'spec': {'replicas': 2}}]}
                ),
            ]
            with (
                patch.object(migration, 'install'),
                patch.object(migration.subprocess, 'check_output', side_effect=replies),
                patch.object(migration.subprocess, 'run') as run,
                patch.object(migration, 'backup'),
                patch.object(
                    migration, 'verify_cluster', side_effect=ValueError('restore failed')
                ),
            ):
                with self.assertRaisesRegex(ValueError, 'restore failed'):
                    migration.migrate(root, 'test', root / 'backups')
            commands = [call.args[0] for call in run.call_args_list]
            self.assertTrue(
                any(
                    'deployment/finance' in command and '--replicas=0' in command
                    for command in commands
                )
            )
            self.assertFalse(
                any(
                    'apply' in command or 'statefulset/postgres' in command
                    for command in commands
                )
            )
            state = json.loads((root / 'postgres-migration.json').read_text())
            self.assertEqual(state['replicas'], {'finance': 2})

    def test_existing_target_prevents_reimport_and_scaling(self):
        import staging_postgres_migrate as migration

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'plan.json').write_text('{"namespace": "test", "environment": "staging"}')
            with (
                patch.object(migration, 'install'),
                patch.object(
                    migration.subprocess,
                    'check_output',
                    return_value='{"items": [{"metadata": {"name": "finance-postgres"}}]}',
                ),
                patch.object(migration.subprocess, 'run') as run,
            ):
                with self.assertRaisesRegex(ValueError, 'уже существуют'):
                    migration.migrate(root, 'test', root / 'backups')
            run.assert_not_called()


class MigrationIdentityTest(unittest.TestCase):
    def test_connection_change_creates_new_job_instead_of_mutating_old_one(self):
        import staging_manifest
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            first = render(RELEASE, path)
            with patch.object(staging_manifest.staging_postgres, 'host', side_effect=lambda owner: owner + '-different-primary'):
                second = render(RELEASE, path)
            def jobs(phases):
                return {obj['metadata']['labels']['broker/service']: obj['metadata']['name']
                    for obj in phases['migrations'] if obj['kind'] == 'Job'}
            self.assertEqual(jobs(first).keys(), jobs(second).keys())
            self.assertTrue(all(jobs(first)[owner] != jobs(second)[owner] for owner in jobs(first)))

"""Проверка ресурсов, манифестов и сохранности секретов личного staging."""

from copy import deepcopy
import json
import base64
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
sys.path.insert(0, str(ROOT / 'scripts/openapi_gen'))
from deployment import merge, validate
import staging

RELEASE = {
    'revision': 'a' * 40,
    'images': {
        name: f"ghcr.io/makrus1/broker-{name.replace('_', '-')}@sha256:" + 'b' * 64
        for name in staging.SERVICES
    },
}


class DeploymentTest(unittest.TestCase):
    def test_keycloak_theme_and_organization_backend_are_provisioned(self):
        with tempfile.TemporaryDirectory() as directory:
            phases = staging.render(RELEASE, Path(directory))
            keycloak = next(
                obj
                for obj in phases['infrastructure']
                if obj['kind'] == 'Deployment' and obj['metadata']['name'] == 'keycloak'
            )
            pod = keycloak['spec']['template']['spec']
            theme = next(volume for volume in pod['volumes'] if volume['name'] == 'theme')
            theme_config = next(
                obj
                for obj in phases['infrastructure']
                if obj['metadata']['name'] == theme['configMap']['name']
            )
            self.assertIn('login__theme.properties', theme_config['data'])
            self.assertTrue(
                any(
                    item['path'] == 'login/resources/css/broker.css'
                    for item in theme['configMap']['items']
                )
            )
            self.assertTrue(
                any(
                    obj['kind'] == 'Deployment' and obj['metadata']['name'] == 'organizations'
                    for obj in phases['services']
                )
            )

    def test_layered_resources_and_invalid_values(self):
        result = merge(
            {
                'replicas': 1,
                'resources': {
                    'requests': {'cpu': '100m', 'memory': '128Mi'},
                    'limits': {'memory': '512Mi'},
                },
            },
            {'resources': {'requests': {'cpu': '250m'}}},
            {'replicas': 2},
        )
        self.assertEqual(result['replicas'], 2)
        self.assertEqual(result['resources']['requests'], {'cpu': '250m', 'memory': '128Mi'})
        for value in [
            None,
            {'replicas': True},
            {'replicas': 0},
            {'replicas': -1},
            {'typo': 1},
            {'resources': {'requests': {'cpu': '0'}}},
            {'resources': {'limits': {'memory': 'wrong'}}},
            {'resources': {'requests': {'memory': '1Gi'}, 'limits': {'memory': '512Mi'}}},
        ]:
            with self.subTest(value=value), self.assertRaises(ValueError):
                validate(value)

    def test_stable_render_secrets_probes_and_migrations(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'state'
            phases = staging.render(RELEASE, path)
            original = {file.name: file.read_bytes() for file in path.iterdir()}
            self.assertEqual(staging.render(RELEASE, path), phases)
            self.assertEqual({file.name: file.read_bytes() for file in path.iterdir()}, original)
            for file in path.iterdir():
                self.assertEqual(file.stat().st_mode & 0o777, 0o600)
            objects = [obj for phase in phases.values() for obj in phase]
            for obj in objects:
                self.assertRegex(obj['metadata']['name'], r'^[a-z0-9][a-z0-9-]*$')
                if obj['kind'] == 'Secret':
                    self.assertNotIn('Secret', obj.get('data', {}))
            apps = [obj for obj in phases['services'] if obj['kind'] == 'Deployment']
            self.assertEqual(len(apps), len(staging.SERVICES))
            bff = next(app for app in apps if app['metadata']['name'] == 'admin-bff')
            self.assertEqual(bff['spec']['strategy']['type'], 'Recreate')
            variables = next(
                obj
                for obj in phases['services']
                if obj['kind'] == 'Secret' and obj['metadata']['name'].startswith('configs-vars')
            )
            self.assertIn(
                'postgres.broker-staging.svc.cluster.local',
                base64.b64decode(variables['data']['config_vars.yaml']).decode(),
            )

            shipment_vars = next(
                obj
                for obj in phases['services']
                if obj['kind'] == 'Secret' and obj['metadata']['name'].startswith('shipments-vars')
            )
            dispatcher = yaml.safe_load(base64.b64decode(shipment_vars['data']['config_vars.yaml']))
            self.assertTrue(dispatcher['dispatcher-enabled'])
            self.assertEqual(
                dispatcher['dispatcher-client-secret'],
                (path / 'shipments-dispatcher-secret').read_text(),
            )
            self.assertIn(
                'keycloak.broker-staging.svc.cluster.local', dispatcher['dispatcher-token-url']
            )

            for app in apps:
                spec = app['spec']['template']['spec']
                self.assertFalse(spec['automountServiceAccountToken'])
                container = spec['containers'][0]
                self.assertIn('@sha256:', container['image'])
                self.assertEqual(container['readinessProbe']['httpGet']['path'], '/ping/ready')
                self.assertEqual(container['livenessProbe']['httpGet']['path'], '/ping')
                self.assertTrue(container['securityContext']['runAsNonRoot'])
            self.assertEqual(
                len([obj for obj in phases['migrations'] if obj['kind'] == 'Job']),
                sum(
                    (ROOT / 'services' / name / 'postgresql/schemas').is_dir()
                    for name in staging.SERVICES
                ),
            )
            realm = next(
                obj
                for obj in phases['infrastructure']
                if obj['kind'] == 'Secret' and obj['metadata']['name'].startswith('keycloak-realm')
            )
            data = json.loads(base64.b64decode(realm['data']['broker-realm.json']).decode())
            self.assertEqual({user['username'] for user in data['users']}, {'author', 'reviewer'})
            self.assertNotIn('${', base64.b64decode(realm['data']['broker-realm.json']).decode())

    def test_release_validation_happens_before_writes(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'state'
            release = deepcopy(RELEASE)
            release['images']['broker'] = 'ghcr.io/makrus1/broker:latest'
            with self.assertRaises(ValueError):
                staging.render(release, path)
            self.assertFalse(path.exists())

    def test_configuration_versions_preserve_rollback(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / 'repo'
            root.mkdir()
            shutil.copy(ROOT / 'platform.yaml', root)
            shutil.copytree(
                ROOT / 'services', root / 'services', ignore=shutil.ignore_patterns('__pycache__')
            )
            shutil.copytree(ROOT / 'tools/local', root / 'tools/local')
            shutil.copytree(ROOT / 'schemas/permissions', root / 'schemas/permissions')
            path = Path(directory) / 'state'
            first = staging.render(RELEASE, path, root=root)
            manifest = root / 'services/configs/service.yaml'
            manifest.write_text(manifest.read_text() + '\ndeployment:\n  replicas: 2\n')
            second = staging.render(RELEASE, path, root=root)
            deployment = next(
                obj
                for obj in second['services']
                if obj['kind'] == 'Deployment' and obj['metadata']['name'] == 'configs'
            )
            self.assertEqual(deployment['spec']['replicas'], 2)
            # Изменение только replicas не переименовывает конфиг и не роняет rollback.
            self.assertEqual(
                [obj['metadata']['name'] for obj in first['services']],
                [obj['metadata']['name'] for obj in second['services']],
            )
            bff = root / 'services/admin_bff/service.yaml'
            bff.write_text(bff.read_text() + '\ndeployment:\n  replicas: 2\n')
            with self.assertRaisesRegex(ValueError, 'сессий'):
                staging.render(RELEASE, path, root=root)

    def test_selected_manifests_exclude_other_services_and_migrations(self):
        from unittest.mock import patch
        import yaml

        for services, infrastructure in [(['admin_bff'], False), (['configs'], False), ([], True)]:
            with self.subTest(services=services), tempfile.TemporaryDirectory() as directory:
                path = Path(directory)
                staging.render(RELEASE, path)

                def check(directory, context, selected, include_infra, paths, jobs):
                    self.assertEqual((selected, include_infra), (services, infrastructure))
                    for manifest in paths.values():
                        if manifest:
                            objects = list(yaml.safe_load_all(manifest.read_text()))
                            self.assertTrue(objects)
                            self.assertTrue(
                                all(
                                    obj['metadata']['labels']['broker/service'] in services
                                    for obj in objects
                                )
                            )
                    self.assertEqual(bool(jobs), services == ['configs'])

                with patch('staging._apply', side_effect=check) as apply:
                    staging.apply(path, 'ctx', services, infrastructure)
                    apply.assert_called_once()

    def test_selection_rejected_before_kubectl(self):
        from unittest.mock import patch

        for services in ([], ['unknown'], ['configs', 'configs']):
            with self.subTest(services=services), patch('staging.subprocess.run') as run:
                with self.assertRaises(ValueError):
                    staging.apply(Path('/missing'), 'ctx', services, False)
                run.assert_not_called()

    def test_apply_requires_successful_migrations(self):
        from unittest.mock import patch
        import subprocess

        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            staging.render(RELEASE, path)
            calls = []

            def run(args, **kwargs):
                calls.append(args)
                if 'version' in args:
                    return subprocess.CompletedProcess(
                        args,
                        0,
                        stdout=json.dumps({'serverVersion': {'gitVersion': 'v1.36.5+k3s1'}}),
                    )
                if 'wait' in args:
                    raise subprocess.CalledProcessError(1, args)
                return subprocess.CompletedProcess(args, 0, stdout='')

            with patch('organization_identity.configure_cluster'), patch(
                'staging.subprocess.run', side_effect=run
            ), self.assertRaises(subprocess.CalledProcessError):
                staging.apply(path, 'staging-test')
            self.assertFalse(any('services.yaml' in str(part) for args in calls for part in args))

    def test_apply_rejects_changed_database_password_before_mutations(self):
        from unittest.mock import patch
        import subprocess
        import base64

        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            staging.render(RELEASE, path)
            calls = []

            def run(args, **kwargs):
                calls.append(args)
                if 'version' in args:
                    result = {'serverVersion': {'gitVersion': 'v1.36.5+k3s1'}}
                else:
                    result = {'data': {'password': base64.b64encode(b'previous-password').decode()}}
                return subprocess.CompletedProcess(args, 0, stdout=json.dumps(result))

            with patch('staging.subprocess.run', side_effect=run), self.assertRaisesRegex(
                ValueError, 'Пароль'
            ):
                staging.apply(path, 'staging-test')
            self.assertFalse(any('apply' in args for args in calls))

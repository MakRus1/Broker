"""HTTPS, доступ к служебным endpoint и подключение телеметрии staging."""

import base64
import json
from pathlib import Path
import sys
import tempfile
import subprocess
import shutil
from unittest.mock import patch
import unittest

import yaml

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import staging_manifest
import staging_lan
from staging_identity import keycloak_script
from test_deployment import RELEASE


class LanTest(unittest.TestCase):
    def test_https_and_monitoring_are_explicitly_enabled(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'lan').mkdir()
            (root / 'lan/enabled').write_text('true\n')
            phases = staging_manifest.render(RELEASE, root)
            monitoring = phases['monitoring']
            ingresses = [obj for obj in monitoring if obj['kind'] == 'Ingress']
            self.assertEqual(len(ingresses), 4)
            self.assertTrue(
                all(obj['spec']['tls'][0]['secretName'] == 'staging-lan-tls' for obj in ingresses)
            )
            self.assertFalse(
                any(
                    obj['kind'] == 'Service' and obj['spec'].get('type', 'ClusterIP') != 'ClusterIP'
                    for obj in monitoring
                )
            )
            self.assertEqual(
                len([obj for obj in monitoring if obj['kind'] == 'PersistentVolumeClaim']), 5
            )
            variables = next(
                obj
                for obj in phases['services']
                if obj['kind'] == 'Secret' and obj['metadata']['name'].startswith('admin-bff-vars-')
            )
            values = yaml.safe_load(base64.b64decode(variables['data']['config_vars.yaml']))
            self.assertTrue(values['admin-secure-cookie'])
            web_variables = next(
                obj
                for obj in phases['services']
                if obj['kind'] == 'Secret' and obj['metadata']['name'].startswith('web-bff-vars-')
            )
            web = yaml.safe_load(base64.b64decode(web_variables['data']['config_vars.yaml']))
            self.assertEqual(web['web-origin'], 'https://web.broker.home.arpa')
            self.assertTrue(web['web-secure-cookie'])
            self.assertFalse(web['auth-allow-http'])
            self.assertTrue(web['web-token-url'].startswith('https://keycloak.'))
            realm_secret = next(
                obj
                for obj in phases['infrastructure']
                if obj['kind'] == 'Secret' and obj['metadata']['name'].startswith('keycloak-realm-')
            )
            realm = json.loads(base64.b64decode(realm_secret['data']['broker-realm.json']))
            clients = {client['clientId']: client for client in realm['clients']}
            self.assertEqual(
                clients['broker-web']['redirectUris'],
                ['https://web.broker.home.arpa/auth/callback'],
            )
            self.assertEqual(
                clients['broker-admin']['redirectUris'],
                ['https://admin.broker.home.arpa/auth/callback'],
            )
            self.assertIn(
                'web.broker.home.arpa', {obj['spec']['rules'][0]['host'] for obj in ingresses}
            )

            self.assertFalse(values['auth-allow-http'])
            self.assertTrue(values['admin-token-url'].startswith('https://keycloak.'))
            self.assertTrue(values['otlp-enabled'])
            bff_config = next(
                obj
                for obj in phases['services']
                if obj['kind'] == 'ConfigMap'
                and obj['metadata']['name'].startswith('admin-bff-config-')
            )
            static = yaml.safe_load(bff_config['data']['static_config.yaml'])
            self.assertEqual(
                static['components_manager']['components']['otlp-logger']['sinks']['logs'],
                'default',
            )
            grafana = next(
                obj
                for obj in monitoring
                if obj['kind'] == 'Deployment' and obj['metadata']['name'] == 'grafana'
            )
            env = {
                item['name']: item
                for item in grafana['spec']['template']['spec']['containers'][0]['env']
            }
            self.assertEqual(env['GF_AUTH_ANONYMOUS_ENABLED']['value'], 'false')
            self.assertIn('secretKeyRef', env['GF_SECURITY_ADMIN_PASSWORD']['valueFrom'])
            collector = next(
                obj
                for obj in monitoring
                if obj['kind'] == 'ConfigMap'
                and obj['metadata']['name'].startswith('collector-settings-')
            )
            telemetry = yaml.safe_load(collector['data']['config.yaml'])
            receiver = telemetry['receivers']['filelog/broker']
            self.assertEqual(receiver['compression'], 'auto')
            self.assertTrue(receiver['retry_on_failure']['enabled'])
            self.assertEqual(telemetry['processors']['batch']['send_batch_max_size'], 256)
            self.assertEqual(staging_manifest.render(RELEASE, root), phases)

    def test_web_localhost_origin_is_separate_without_lan(self):
        with tempfile.TemporaryDirectory() as directory:
            phases = staging_manifest.render(RELEASE, Path(directory))
            variables = next(
                obj
                for obj in phases['services']
                if obj['kind'] == 'Secret' and obj['metadata']['name'].startswith('web-bff-vars-')
            )
            values = yaml.safe_load(base64.b64decode(variables['data']['config_vars.yaml']))
            self.assertEqual(values['web-origin'], 'http://localhost:14080')
            self.assertFalse(values['web-secure-cookie'])

    @unittest.skipUnless(shutil.which('openssl'), 'Нужен OpenSSL')
    def test_adding_web_domain_renews_leaf_without_rotating_ca(self):
        help_text = subprocess.run(['openssl', 'x509', '-help'], capture_output=True, text=True)
        if '-checkhost' not in help_text.stdout + help_text.stderr:
            self.skipTest('Нужен OpenSSL с -checkhost; на macOS добавьте openssl@3/bin в PATH')
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            ca, cert, key = staging_lan.ensure_tls(folder, ['admin.broker.home.arpa'])
            original_ca = ca.read_bytes()
            original_ca_key = (folder / 'ca.key').read_bytes()
            original_cert = cert.read_bytes()
            staging_lan.ensure_tls(folder, ['admin.broker.home.arpa', 'web.broker.home.arpa'])
            self.assertEqual(ca.read_bytes(), original_ca)
            self.assertEqual((folder / 'ca.key').read_bytes(), original_ca_key)
            self.assertNotEqual(cert.read_bytes(), original_cert)
            renewed = cert.read_bytes()
            staging_lan.ensure_tls(folder, ['admin.broker.home.arpa', 'web.broker.home.arpa'])
            self.assertEqual(cert.read_bytes(), renewed)
            subprocess.run(
                [
                    'openssl',
                    'verify',
                    '-CAfile',
                    str(ca),
                    '-verify_hostname',
                    'web.broker.home.arpa',
                    str(cert),
                ],
                check=True,
                capture_output=True,
            )

    def test_existing_keycloak_sync_adds_web_without_changing_passwords(self):
        script = keycloak_script('https://admin.broker.home.arpa', 'https://web.broker.home.arpa')
        self.assertIn('clientId=broker-web', script)
        self.assertIn('create clients', script)
        self.assertIn('https://web.broker.home.arpa/auth/callback', script)
        self.assertIn('--uusername author --cclientid broker-logistics', script)
        self.assertIn('--uusername reviewer --cclientid broker-logistics', script)
        self.assertNotIn('set-password', script)
        self.assertNotIn('create users', script)
        with self.assertRaises(ValueError):
            keycloak_script('https://admin.broker.home.arpa', 'https://web.broker.home.arpa/path')

    def test_lan_apply_prepares_tls_and_synchronizes_both_browser_origins(self):
        import staging

        with tempfile.TemporaryDirectory() as directory:
            state = Path(directory)
            (state / 'lan').mkdir()
            (state / 'lan/enabled').write_text('true\n')
            staging_manifest.render(RELEASE, state)

            def run(args, **kwargs):
                stdout = (
                    json.dumps({'serverVersion': {'gitVersion': 'v1.36.5+k3s1'}})
                    if 'version' in args
                    else ''
                )
                return subprocess.CompletedProcess(args, 0, stdout=stdout)

            with patch('staging.subprocess.run', side_effect=run), patch(
                'staging_lan.initialize'
            ) as initialize, patch('staging_lan.sync_keycloak') as sync:
                staging.apply(state, 'test-context', [], True)
            initialize.assert_called_once_with(staging.ROOT, state, 'test-context')
            sync.assert_called_once_with(
                'test-context',
                'broker-staging',
                'https://admin.broker.home.arpa',
                'https://web.broker.home.arpa',
            )

    def test_enabled_lan_refuses_to_silently_replace_missing_ca(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            (folder / 'enabled').write_text('true\n')
            with self.assertRaisesRegex(ValueError, 'CA'):
                staging_lan.ensure_tls(folder, ['admin.broker.home.arpa'])

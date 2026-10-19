"""HTTPS, доступ к служебным endpoint и подключение телеметрии staging."""

import base64
import json
from pathlib import Path
import sys
import tempfile
import unittest

import yaml

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import staging_manifest
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
            self.assertEqual(len(ingresses), 3)
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

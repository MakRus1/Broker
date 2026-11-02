"""Ресурсные панели используют реальные метрики Kubernetes и не смешивают владельцев."""

import json
from pathlib import Path
import re
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import staging_resource_monitoring as resources


class ResourceMonitoringTest(unittest.TestCase):
    def test_panels_are_appended_without_changing_existing_ones(self):
        text = (ROOT / 'services/finance/monitoring/grafana.json').read_text()
        base = json.loads(text)
        result = json.loads(resources.dashboard(text, postgres=True))
        self.assertEqual(result['panels'][: len(base['panels'])], base['panels'])
        self.assertEqual(len(result['panels']), len({p['id'] for p in result['panels']}))
        self.assertTrue(
            any(p['title'] == 'PostgreSQL: задержка репликации' for p in result['panels'])
        )
        for panel in result['panels'][len(base['panels']) :]:
            for target in panel.get('targets', []):
                self.assertIn('$service', target['expr'])
                self.assertIn('$environment', target['expr'])
                self.assertIn('$pod', target['expr'])
        self.assertNotIn(
            'PostgreSQL: primary / replica', resources.dashboard(text, postgres=False)
        )
        self.assertEqual(
            resources.dashboard(text, postgres=True), resources.dashboard(text, postgres=True)
        )

    def apply_labels(self, values):
        values = dict(values)
        for rule in resources.labels(['finance', 'transport_locations'], 'testing'):
            joined = ';'.join(values.get(label, '') for label in rule.get('source_labels', []))
            matched = re.fullmatch(rule.get('regex', '(.*)'), joined)
            if rule.get('action') == 'keep':
                if not matched:
                    return None
            elif matched:
                values[rule['target_label']] = rule['replacement']
        return values

    def test_labels_distinguish_application_and_database_and_drop_other_services(self):
        app = self.apply_labels({'pod': 'finance-ghjkm54-abc12'})
        database = self.apply_labels({'pod': 'finance-postgres-2'})
        self.assertEqual(app['broker_workload'], 'application')
        self.assertEqual(database['broker_workload'], 'postgres')
        self.assertEqual(database['broker_service'], 'finance')
        self.assertEqual(database['environment'], 'testing')
        self.assertEqual(
            self.apply_labels({'pod': 'transport-locations-postgres-1'})['broker_service'],
            'transport_locations',
        )
        self.assertIsNone(self.apply_labels({'pod': 'some-other-service-ab123cd-abc12'}))

    def test_scraping_uses_verified_tls_and_namespace_scoped_discovery(self):
        jobs = resources.scrape_configs('broker-testing', ['finance'], 'testing')
        import yaml

        self.assertNotIn('&id', yaml.safe_dump(jobs))
        cadvisor = next(job for job in jobs if job['job_name'] == 'kubernetes-cadvisor')
        self.assertIn('ca_file', cadvisor['tls_config'])
        self.assertNotIn('insecure_skip_verify', cadvisor['tls_config'])
        self.assertEqual(cadvisor['metric_relabel_configs'][0]['regex'], 'broker-testing')
        postgres = next(job for job in jobs if job['job_name'] == 'postgresql-instances')
        self.assertEqual(
            postgres['kubernetes_sd_configs'][0]['namespaces']['names'], ['broker-testing']
        )
        objects = resources.objects(ROOT, 'broker-staging', 'staging')
        roles = [obj for obj in objects if obj['kind'] == 'ClusterRole']
        self.assertEqual(
            {
                resource
                for role in roles
                for rule in role['rules']
                for resource in rule['resources']
            },
            {'nodes', 'nodes/proxy'},
        )
        self.assertFalse(any('*' in rule['verbs'] for role in roles for rule in role['rules']))

"""Состав, фильтры и воспроизводимость Grafana-дашбордов сервисов."""

import json
from pathlib import Path
import sys
import tempfile
import unittest
import yaml

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tests'))
from support.generator_repository import copy_generator_repository
sys.path.insert(0, str(ROOT / 'scripts/openapi_gen'))
from dashboards import provision, render
from generate import UniqueLoader, run


class DashboardsTest(unittest.TestCase):
    def test_sections_follow_features_and_panel_ids_are_stable(self):
        base = json.loads(render('example', {}))
        full = json.loads(
            render(
                'example',
                {
                    'postgresql': {'enabled': True},
                    'grpc': {'enabled': True},
                },
            )
        )
        self.assertFalse(any(panel['title'] == 'PostgreSQL' for panel in base['panels']))
        self.assertTrue(any(panel['title'] == 'PostgreSQL' for panel in full['panels']))
        self.assertTrue(any(panel['title'] == 'gRPC-сервер' for panel in full['panels']))
        lookup = {panel['title']: panel['id'] for panel in full['panels']}
        for panel in base['panels']:
            self.assertEqual(panel['id'], lookup[panel['title']])
        self.assertEqual(len(full['panels']), len({panel['id'] for panel in full['panels']}))
        for panel in full['panels']:
            for target in panel.get('targets', []):
                self.assertIn('$service', target['expr'])
                self.assertIn('$environment', target['expr'])
                if panel['type'] != 'logs':
                    self.assertIn('$instance', target['expr'])
        text = render('example', {})
        self.assertNotIn('histogram_quantile', text)
        self.assertIn('percentile=\\"p95\\"', text)
        self.assertNotIn('http_status', text)
        self.assertEqual(text, render('example', {}))

    def test_outbox_panels_follow_library_and_do_not_sum_shared_queue(self):
        dashboard = json.loads(render('queued', {'libraries': ['common', 'outbox']}))
        lookup = {panel['title']: panel for panel in dashboard['panels']}
        self.assertIn('Доставка сообщений', lookup)
        queue = lookup['Outbox: сообщений в очереди']['targets'][0]['expr']
        self.assertIn('max by(outbox_topic)', queue)
        self.assertNotIn('sum(', queue)
        active = lookup['Outbox: активные доставки']['targets'][0]['expr']
        self.assertIn('sum by(outbox_topic)', active)
        self.assertIn('Outbox: последний снимок успешен', lookup)
        plain = json.loads(render('plain', {}))
        self.assertFalse(any(panel['title'] == 'Доставка сообщений' for panel in plain['panels']))
        with self.assertRaisesRegex(ValueError, 'libraries: outbox'):
            render('plain', {'dashboard': {'sections': ['outbox']}})

    def test_customization_and_long_service_names(self):
        spec = {
            'dashboard': {
                'title': 'Сервис поставок',
                'description': 'Проверяем очереди',
                'refresh': '30s',
                'tags': ['logistics'],
                'sections': ['http'],
                'panels': [
                    {
                        'title': 'Очередь',
                        'expr': 'sum(queue_size{service="$service"})',
                        'unit': 'short',
                        'legend': 'Очередь',
                    }
                ],
            }
        }
        dashboard = json.loads(render('shipment', spec))
        self.assertEqual(dashboard['title'], 'Сервис поставок')
        self.assertEqual(dashboard['refresh'], '30s')
        self.assertIn('logistics', dashboard['tags'])
        custom = next(panel for panel in dashboard['panels'] if panel['title'] == 'Очередь')
        self.assertEqual(custom['targets'][0]['expr'], spec['dashboard']['panels'][0]['expr'])
        first = json.loads(render('x' * 80, {}))['uid']
        second = json.loads(render('x' * 79 + 'y', {}))['uid']
        self.assertLessEqual(len(first), 40)
        self.assertNotEqual(first, second)

    def test_service_dashboard_has_only_metric_sources_and_no_log_panels(self):
        for spec in [{}, {'libraries': ['common', 'outbox']}]:
            dashboard = json.loads(render('example', spec))
            variables = {item['name'] for item in dashboard['templating']['list']}
            self.assertFalse({'level', 'loki'} & variables)
            self.assertFalse(any(panel['type'] == 'logs' for panel in dashboard['panels']))
            self.assertNotIn('Loki', render('example', spec))
            if 'libraries' in spec:
                row = next(panel for panel in dashboard['panels'] if panel['title'] == 'Доставка сообщений')
                self.assertEqual(row['id'], 700)

    def test_invalid_settings_are_rejected(self):
        invalid = [
            None,
            {'unknown': True},
            {'title': ''},
            {'title': 4},
            {'refresh': '0s'},
            {'tags': 'tag'},
            {'tags': [None]},
            {'sections': ['unknown']},
            {'sections': ['logs']},
            {'sections': ['http', 'http']},
            {'sections': ['postgresql']},
            {'sections': ['grpc']},
            {'panels': {}},
            {'panels': [{}]},
            {'panels': [{'title': 'X', 'expr': 'x', 'url': 'https://example.invalid'}]},
        ]
        for value in invalid:
            with self.subTest(value=value), self.assertRaises(ValueError):
                render('example', {'dashboard': value})

    def test_provisioning_is_read_only_and_does_not_publish_stale_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'tools/local/grafana').mkdir(parents=True)
            (root / 'tools/local/grafana/dashboard.json').write_text('{}\n')
            service = root / 'services/example'
            (service / 'monitoring').mkdir(parents=True)
            (service / 'service.yaml').write_text('version: 1\n')
            path = service / 'monitoring/grafana.json'
            path.write_text(render('example', {}))
            source = path.read_bytes()
            target = root / 'provisioned'
            provision(root, target, UniqueLoader)
            self.assertEqual((target / 'service-example.json').read_bytes(), source)
            path.write_text('{}')
            with self.assertRaisesRegex(ValueError, 'make gen'):
                provision(root, target, UniqueLoader)
            self.assertEqual(path.read_text(), '{}')
            self.assertEqual((target / 'service-example.json').read_bytes(), source)
            (service / 'service.yaml').unlink()
            provision(root, target, UniqueLoader)
            self.assertFalse((target / 'service-example.json').exists())

    def test_check_and_build_do_not_rewrite_dashboard(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            copy_generator_repository(root)
            copy = root / 'services/broker'
            run(copy)
            path = copy / 'monitoring/grafana.json'
            original = path.read_text()
            path.write_text('{}\n')
            for arguments in ({'check': True}, {'output': Path(directory) / 'build'}):
                with self.subTest(arguments=arguments), self.assertRaisesRegex(
                    ValueError, 'make gen'
                ):
                    run(copy, **arguments)
                self.assertEqual(path.read_text(), '{}\n')
            run(copy)
            self.assertEqual(path.read_text(), original)

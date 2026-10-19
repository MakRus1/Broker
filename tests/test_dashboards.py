"""Состав, фильтры и воспроизводимость Grafana-дашбордов сервисов."""

import json
from pathlib import Path
import sys
import tempfile
import unittest
import yaml

ROOT = Path(__file__).resolve().parent.parent
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
        service = ROOT / 'services/broker'
        # Работаем в отдельной копии, сохраняя paths библиотек исходного репозитория.
        import shutil

        with tempfile.TemporaryDirectory() as directory:
            copy = Path(directory) / 'broker'
            shutil.copytree(service, copy, ignore=shutil.ignore_patterns('__pycache__'))
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

"""Одна команда обновляет отслеживаемую генерацию, не привязывая lock к чужим DTO."""
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch
import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
sys.path.insert(0, str(ROOT / 'scripts/openapi_gen'))
from support.generator_repository import copy_generator_repository
from generate import UniqueLoader, load, digest, run
from clients import client_outputs
from export_clients import export
from update import update


class UpdateTest(unittest.TestCase):
    def test_provider_changes_refresh_clients_without_consumer_lock_churn(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            copy_generator_repository(root)
            provider = root / 'services/broker'
            consumer = root / 'services/consumer'
            shutil.copytree(provider, consumer)
            manifest = yaml.safe_load((consumer / 'service.yaml').read_text())
            manifest['clients'] = {'api': {'service': 'broker', 'transport': 'http'}}
            (consumer / 'service.yaml').write_text(yaml.safe_dump(manifest))
            export(consumer)
            with patch('clients.ROOT', root):
                update(provider)
                lock = (consumer / 'docs/api/codegen.lock').read_bytes()
                before = client_outputs(consumer, UniqueLoader, load)
                source = provider / 'docs/api/new.yaml'
                source.write_text(yaml.safe_dump({'openapi': '3.0.3', 'paths': {
                    '/fresh': {'post': {'responses': {'204': {'description': 'Готово'}}}}}}))
                changed = update(provider)
                self.assertNotIn('consumer', changed)
                self.assertIn('configs', changed)
                self.assertEqual(lock, (consumer / 'docs/api/codegen.lock').read_bytes())
                after = client_outputs(consumer, UniqueLoader, load)
                self.assertNotEqual(before, after)
                self.assertIn('Fresh::Response Fresh(', after['include/clients/api/client.hpp'])
                source.write_text(yaml.safe_dump({'openapi': '3.0.3', 'paths': {
                    '/fresh': {'post': {'responses': {'200': {'description': 'Готово', 'content': {
                        'application/json': {'schema': {'type': 'object', 'properties': {
                            'extra': {'type': 'string'}}}}}}}}}}}))
                update(provider)
                self.assertEqual(lock, (consumer / 'docs/api/codegen.lock').read_bytes())
                self.assertIn('extra', client_outputs(consumer, UniqueLoader, load)['include/clients/api/client.hpp'])
                for service in (provider, consumer, root / 'services/configs'):
                    run(service, check=True)
                # У критичной зависимости hash схемы находится в отслеживаемом
                # статическом конфиге. Его команда тоже обновляет автоматически.
                manifest['critical_dependencies'] = {'configs': {'BROKER_HTTP_CLIENT_CONFIG': {
                    'max_age_seconds': 5}}}
                (consumer / 'service.yaml').write_text(yaml.safe_dump(manifest))
                update(consumer)
                source.write_text(source.read_text().replace('/fresh:', '/fresh_updated:'))
                self.assertIn('consumer', update(provider))
                run(consumer, check=True)
                # Повтор команды не переписывает файлы и не находит зависимых обновлений.
                self.assertEqual(update(provider), ['broker'])

    def test_check_does_not_render_build_only_clients(self):
        service = ROOT / 'services/web_bff'
        # Проверка проверяет актуальность, но не форматирует неотслеживаемые C++ DTO.
        with patch('generate.client_outputs', side_effect=AssertionError('rendered')):
            try:
                run(service, check=True)
            except ValueError as error:
                self.assertIn('codegen.lock is stale', str(error))

    def test_invalid_contract_is_not_treated_as_stale_generation(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            copy_generator_repository(root)
            source = root / 'services/broker/docs/api/bad.yaml'
            source.write_text('openapi: 3.0.3\npaths: { /broken: {get: {operationId: forbidden, responses: {204: {description: empty}}}}}\n')
            with self.assertRaises(ValueError):
                update(root / 'services/broker')

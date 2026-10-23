"""Новая зависимость из service.yaml не должна обращаться к localhost в Docker."""

import importlib.util
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
spec = importlib.util.spec_from_file_location(
    'local_environment', Path(__file__).resolve().parents[1] / 'scripts/local-env.py'
)
local_environment = importlib.util.module_from_spec(spec)
spec.loader.exec_module(local_environment)


class LocalClientsTest(unittest.TestCase):
    def test_declared_clients_use_service_addresses(self):
        self.assertEqual(
            local_environment.client_variables('counterparties')['client-organizations-base-url'],
            'http://organizations:8130',
        )
        self.assertEqual(
            local_environment.client_variables('shipments')['client-counterparties-base-url'],
            'http://counterparties:8120',
        )
        clients = local_environment.client_variables('admin_bff')
        self.assertEqual(clients['client-shipments-base-url'], 'http://shipments:8100')
        self.assertEqual(clients['client-config_admin-base-url'], 'http://configs:8092')

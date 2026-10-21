"""Смена адреса Hyper-V не должна оставаться незамеченной при деплое."""

from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from staging_network import validate_addresses


class NetworkTest(unittest.TestCase):
    def check(self, node='172.21.109.80', api='172.21.109.80'):
        validate_addresses(
            [{'addr_info': [{'family': 'inet', 'local': '172.21.109.80'}]}],
            {'items': [{'status': {'addresses': [{'type': 'InternalIP', 'address': node}]}}]},
            {'items': [{'addressType': 'IPv4', 'endpoints': [{'addresses': [api]}]}]},
        )

    def test_current_addresses(self):
        self.check()

    def test_old_node_or_api_address_blocks_deployment(self):
        for node, api in [('172.23.230.37', '172.21.109.80'), ('172.21.109.80', '172.23.230.37')]:
            with self.subTest(node=node, api=api), self.assertRaisesRegex(
                ValueError, 'restart k3s'
            ):
                self.check(node, api)

    def test_missing_api_address_blocks_deployment(self):
        with self.assertRaises(ValueError):
            validate_addresses([], {'items': [{}]}, {'items': []})

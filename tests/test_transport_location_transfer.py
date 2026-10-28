"""Однократный перенос не включается при обычном повторном деплое."""

import sys
from pathlib import Path
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import transport_location_transfer as transfer


class TransferTest(unittest.TestCase):
    def test_completed_does_not_read_or_write_sources(self):
        calls = []
        with patch.object(transfer, 'query', return_value=transfer.NAME + '\n') as query:
            transfer.transfer(lambda name: calls.append(name) or [name])
        self.assertEqual(calls, ['transport_locations'])
        self.assertEqual(query.call_count, 1)

    def test_partial_first_deploy_stops_before_scaling(self):
        with patch.object(transfer, 'completed', return_value=False), patch.object(
            transfer.subprocess, 'run'
        ) as run:
            with self.assertRaises(ValueError):
                transfer.configure_cluster(['kubectl'], 'staging', ['transport_locations'])
        run.assert_not_called()

    def test_completed_cluster_does_not_stop_business_services(self):
        with patch.object(transfer, 'completed', return_value=True), patch.object(
            transfer.subprocess, 'run'
        ) as run:
            transfer.configure_cluster(['kubectl'], 'staging', ['transport_locations'])
        run.assert_not_called()

    def test_copy_terminator_cannot_become_sql(self):
        with self.assertRaises(ValueError):
            transfer.import_sql('a\n\\.\nCOMMIT;\n', '')


class TransferRecoveryTest(unittest.TestCase):
    def test_old_writers_resume_only_when_transfer_has_not_committed(self):
        consumers = ['transport_locations', 'shipments', 'transportations', 'admin_bff', 'web_bff']
        for committed in (False, True):
            with self.subTest(committed=committed):

                def output(command, **kwargs):
                    return '{"items": []}' if 'pod' in command else '{"spec": {"replicas": 2}}'

                with patch.object(
                    transfer, 'completed', side_effect=[False, committed]
                ), patch.object(
                    transfer, 'transfer', side_effect=RuntimeError('connection lost')
                ), patch.object(
                    transfer.subprocess, 'check_output', side_effect=output
                ), patch.object(
                    transfer.subprocess, 'run'
                ) as run:
                    with self.assertRaises(RuntimeError):
                        transfer.configure_cluster(['kubectl'], 'staging', consumers)
                restored = [call for call in run.call_args_list if '--replicas=2' in call.args[0]]
                self.assertEqual(len(restored), 0 if committed else 2)

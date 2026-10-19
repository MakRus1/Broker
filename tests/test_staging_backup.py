"""Проверка целостности копий и отсутствия частичного результата при ошибках."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import staging_backup


class BackupTest(unittest.TestCase):
    def test_failed_dump_does_not_publish_partial_backup(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'copy'
            with patch(
                'staging_backup.subprocess.run',
                side_effect=subprocess.CalledProcessError(1, 'pg_dump'),
            ):
                with self.assertRaises(subprocess.CalledProcessError):
                    staging_backup.backup(output, 'default')
            self.assertEqual(list(Path(directory).iterdir()), [])

    def test_binary_stream_integrity_and_existing_copy_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'copy'
            commands = []

            def dump(command, **kwargs):
                commands.append(command)
                kwargs['stdout'].write(b'PGDMP\x00\xff\r\n')

            with patch('staging_backup.subprocess.run', side_effect=dump), patch(
                'staging_backup.subprocess.check_output', return_value='pg_dump 16\n'
            ):
                staging_backup.backup(output, 'default', host='broker-staging')
            staging_backup.validate(output)
            self.assertTrue(
                all(command[:3] == ['ssh', '-T', 'broker-staging'] for command in commands)
            )
            with self.assertRaises(ValueError):
                staging_backup.backup(output, 'default')
            staging_backup.validate(output)
            (output / 'configs.dump').write_bytes(b'broken')
            with self.assertRaisesRegex(ValueError, 'configs.dump'):
                staging_backup.validate(output)

    def test_invalid_manifest_rejected_before_docker(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'manifest.json').write_text(
                json.dumps({'version': 1, 'files': {'../secret': 'x'}})
            )
            with patch('staging_backup.subprocess.run') as run:
                with self.assertRaises(ValueError):
                    staging_backup.verify(root)
                run.assert_not_called()

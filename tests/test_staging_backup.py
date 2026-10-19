"""Проверка целостности копий и отсутствия частичного результата при ошибках."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import staging_backup


class BackupTest(unittest.TestCase):
    def make_copy(self, root):
        root.mkdir()
        files = {}
        for database in staging_backup.DATABASES:
            path = root / f'{database}.dump'
            path.write_bytes(b'PGDMP')
            files[path.name] = staging_backup.checksum(path)
        (root / 'manifest.json').write_text(json.dumps({'version': 1, 'files': files}))

    def test_new_database_is_dumped_and_restored_without_registry_change(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'copy'

            def dump(command, **kwargs):
                kwargs['stdout'].write(b'PGDMP')

            with patch(
                'staging_backup.subprocess.check_output',
                side_effect=['broker\nconfigs\nkeycloak\nnew_service\n', 'pg_dump 16\n'],
            ), patch('staging_backup.subprocess.run', side_effect=dump):
                staging_backup.backup(output, 'ctx')
            self.assertTrue((output / 'new_service.dump').is_file())
            with patch('staging_backup.subprocess.run') as run:
                staging_backup.verify_cluster(output, 'ctx', 'ns', 'image')
            restores = [call.args[0] for call in run.call_args_list if 'pg_restore' in call.args[0]]
            self.assertEqual(len(restores), 4)
            self.assertTrue(any('new_service' in command for command in restores))

    def test_retention_keeps_newest_verified_and_preserves_other_directories(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for index in range(9):
                copy = root / f'20261019T1700{index:02d}-abcdef12'
                self.make_copy(copy)
                (copy / 'verified').touch()
            unverified = root / 'unverified'
            self.make_copy(unverified)
            (root / 'other').mkdir()
            staging_backup.prune(root, 7)
            self.assertEqual(len(list(root.iterdir())), 9)
            self.assertFalse((root / '20261019T170000-abcdef12').exists())
            self.assertTrue((root / '20261019T170008-abcdef12').exists())
            self.assertTrue(unverified.exists())
            self.assertTrue((root / 'other').exists())

    def test_failed_cluster_restore_removes_only_temporary_pod(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / 'copy'
            self.make_copy(root)
            commands = []
            pod = None

            def run(command, **kwargs):
                nonlocal pod
                commands.append(command)
                if 'create' in command:
                    pod = json.loads(kwargs['input'])
                if 'pg_restore' in command:
                    raise subprocess.CalledProcessError(1, command)
                return Mock()

            with patch('staging_backup.subprocess.run', side_effect=run):
                with self.assertRaises(subprocess.CalledProcessError):
                    staging_backup.verify_cluster(root, 'ctx', 'ns', 'image')
            name = pod['metadata']['name']
            self.assertTrue(name.startswith('backup-check-'))
            self.assertEqual(
                commands[-1],
                [
                    'kubectl',
                    '--context',
                    'ctx',
                    '-n',
                    'ns',
                    'delete',
                    'pod',
                    name,
                    '--wait=true',
                    '--timeout=90s',
                ],
            )
            self.assertFalse(pod['spec']['automountServiceAccountToken'])
            self.assertEqual(pod['spec']['volumes'], [{'name': 'data', 'emptyDir': {}}])
            self.assertIn('listen_addresses=', pod['spec']['containers'][0]['args'])

    def test_failed_dump_does_not_publish_partial_backup(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'copy'
            with patch(
                'staging_backup.subprocess.check_output', return_value='broker\nconfigs\nkeycloak\n'
            ), patch(
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
                'staging_backup.subprocess.check_output',
                side_effect=['broker\nconfigs\nkeycloak\n', 'pg_dump 16\n'],
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

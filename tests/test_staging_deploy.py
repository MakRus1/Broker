"""Обновление VM разрешается только после проверенного бэкапа."""

import io
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import staging_deploy

RELEASE = {
    'revision': 'a' * 40,
    'images': {
        name: f"ghcr.io/makrus1/broker-{name.replace('_', '-')}@sha256:" + 'b' * 64
        for name in staging_deploy.SERVICES
    },
}


class Input(io.StringIO):
    def close(self):
        self.sent = self.getvalue()
        super().close()


class DeployTest(unittest.TestCase):
    @unittest.skipIf(sys.platform == 'win32', 'Блокировка выполняется в Ubuntu VM')
    def test_parallel_deploy_stops_before_git_and_backup(self):
        import fcntl

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / '.staging').mkdir()
            (root / '.staging/credentials.json').write_text('{}')
            lock_path = root / '.cache/broker-staging/deploy.lock'
            lock_path.parent.mkdir(parents=True)
            with lock_path.open('a') as lock:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                with patch('staging_deploy.Path.home', return_value=root), patch(
                    'staging_deploy.subprocess.run'
                ) as run:
                    with self.assertRaisesRegex(ValueError, 'Другой staging-деплой'):
                        staging_deploy.remote_main(dict(release=RELEASE, repository=str(root)))
                    run.assert_not_called()

    def process(self):
        process = Mock()
        process.stdin = Input()
        process.stdout = io.StringIO(
            json.dumps({'status': 'ready', 'before': {'checkout': 'c' * 40}})
            + '\n'
            + json.dumps({'status': 'success'})
            + '\n'
        )
        process.wait.return_value = 0
        process.poll.return_value = 0
        return process

    def test_failed_restore_never_sends_apply(self):
        process = self.process()
        with tempfile.TemporaryDirectory() as directory:
            with patch('staging_deploy.subprocess.Popen', return_value=process), patch(
                'staging_backup.backup'
            ), patch('staging_backup.verify', side_effect=ValueError('restore failed')):
                with self.assertRaisesRegex(ValueError, 'restore failed'):
                    staging_deploy.deploy(RELEASE, 'vm', 'ctx', 'ns', '~/repo', Path(directory))
        self.assertEqual(process.stdin.sent, '')

    def test_verified_backup_precedes_apply(self):
        process = self.process()
        events = []

        def backup(path, *args, **kwargs):
            self.assertEqual(process.stdin.getvalue(), '')
            path.mkdir()
            events.append('backup')

        def verify(path):
            self.assertEqual(events, ['backup'])
            self.assertEqual(process.stdin.getvalue(), '')
            events.append('verify')

        with tempfile.TemporaryDirectory() as directory:
            with patch('staging_deploy.subprocess.Popen', return_value=process), patch(
                'staging_backup.backup', side_effect=backup
            ), patch('staging_backup.verify', side_effect=verify):
                staging_deploy.deploy(RELEASE, 'vm', 'ctx', 'ns', '~/repo', Path(directory))
            self.assertTrue(next(Path(directory).iterdir()).joinpath('before.json').is_file())
        self.assertEqual(events, ['backup', 'verify'])
        self.assertEqual(process.stdin.sent, 'APPLY\n')

    def test_invalid_release_stops_before_ssh(self):
        bad = {**RELEASE, 'images': {**RELEASE['images'], 'broker': 'evil/image:latest'}}
        with patch('staging_deploy.subprocess.Popen') as start:
            with self.assertRaises(ValueError):
                staging_deploy.deploy(bad, 'vm', 'ctx', 'ns', '~/repo', Path('/unused'))
            start.assert_not_called()

    @unittest.skipIf(sys.platform == 'win32', 'Блокировка выполняется в Ubuntu VM')
    def test_remote_eof_never_checks_out_or_applies(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / '.staging').mkdir()
            (root / '.staging/credentials.json').write_text('{}')
            commands = []

            def run(command, **kwargs):
                commands.append(command)
                text = ''
                if command[:3] == ('git', 'rev-parse', 'HEAD'):
                    text = 'c' * 40
                elif command[0] == 'kubectl':
                    text = '{"items": []}'
                return Mock(stdout=text)

            with patch.dict(os.environ), patch(
                'staging_deploy.subprocess.run', side_effect=run
            ), patch('staging_deploy.Path.home', return_value=root), patch(
                'staging_deploy.sys.stdin', io.StringIO('')
            ), patch(
                'staging_deploy.sys.stdout', io.StringIO()
            ):
                with self.assertRaisesRegex(ValueError, 'Бэкап не подтверждён'):
                    staging_deploy.remote_main(
                        dict(release=RELEASE, repository=str(root), context='ctx', namespace='ns')
                    )
            self.assertFalse(any(command[:2] == ('git', 'checkout') for command in commands))
            self.assertFalse((root / '.staging/releases').exists())

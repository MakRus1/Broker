"""Бэкапы и проверка восстановления выполняются в VM до обновления."""

from contextlib import ExitStack
import io
import json
import os
from pathlib import Path
import sys
import tarfile
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


class DeployTest(unittest.TestCase):
    def test_invalid_release_stops_before_ssh(self):
        bad = {**RELEASE, 'images': {**RELEASE['images'], 'broker': 'evil/image:latest'}}
        with patch('staging_deploy.subprocess.run') as start:
            with self.assertRaises(ValueError):
                staging_deploy.deploy(bad, 'vm', 'ctx', 'ns', '~/repo', '~/backups')
            start.assert_not_called()

    def test_caller_sends_only_scripts_and_release(self):
        with patch('staging_deploy.subprocess.run') as run:
            staging_deploy.deploy(RELEASE, 'vm', 'ctx', 'ns', '~/repo', '~/backups')
        command = run.call_args.args[0]
        self.assertEqual(command[:6], ['ssh', '-T', '-o', 'BatchMode=yes', 'vm', command[5]])
        with tarfile.open(fileobj=io.BytesIO(run.call_args.kwargs['input'])) as archive:
            self.assertEqual(
                set(archive.getnames()), {'staging_deploy.py', 'staging_backup.py', 'settings.json'}
            )
            settings = json.load(archive.extractfile('settings.json'))
        self.assertEqual(settings['backup_root'], '~/backups')
        self.assertEqual(settings['keep_backups'], 7)

    @unittest.skipIf(sys.platform == 'win32', 'Блокировка выполняется в Ubuntu VM')
    def test_failed_restore_never_checks_out_or_applies(self):
        self.check_remote(fail_restore=True)

    @unittest.skipIf(sys.platform == 'win32', 'Блокировка выполняется в Ubuntu VM')
    def test_verified_backup_precedes_apply(self):
        self.check_remote(fail_restore=False)

    def check_remote(self, *, fail_restore):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            (root / '.staging').mkdir()
            (root / '.staging/credentials.json').write_text('{}')
            (root / 'tools/local').mkdir(parents=True)
            (root / 'tools/local/images.json').write_text('{"images":{"postgres":"test-image"}}')
            events = []

            def run(command, **kwargs):
                events.append(command[0:2])
                text = ''
                if command[:3] == ('git', 'rev-parse', 'HEAD'):
                    text = 'c' * 40
                elif command[0] == 'kubectl':
                    text = '{"items": []}'
                elif 'render' in command:
                    (root / '.staging/plan.json').write_text('{"namespace":"ns"}')
                return Mock(stdout=text)

            def backup(path, *args, **kwargs):
                events.append('backup')
                path.mkdir(parents=True)

            def verify(*args):
                events.append('verify')
                if fail_restore:
                    raise ValueError('restore failed')

            settings = dict(
                release=RELEASE,
                repository=str(root),
                context='ctx',
                namespace='ns',
                backup_root=str(root / 'backups'),
                keep_backups=7,
            )
            with ExitStack() as stack:
                stack.enter_context(patch.dict(os.environ))
                stack.enter_context(patch('staging_deploy.Path.home', return_value=root))
                stack.enter_context(patch('staging_deploy.subprocess.run', side_effect=run))
                stack.enter_context(patch('staging_deploy.sys.stdout', io.StringIO()))
                stack.enter_context(patch('staging_backup.backup', side_effect=backup))
                stack.enter_context(patch('staging_backup.verify_cluster', side_effect=verify))
                prune = stack.enter_context(patch('staging_backup.prune'))
                if fail_restore:
                    with self.assertRaisesRegex(ValueError, 'restore failed'):
                        staging_deploy.remote_main(settings)
                    self.assertNotIn(('git', 'checkout'), events)
                    self.assertFalse((root / '.staging/releases').exists())
                    prune.assert_not_called()
                else:
                    staging_deploy.remote_main(settings)
                    self.assertLess(events.index('backup'), events.index('verify'))
                    self.assertLess(events.index('verify'), events.index(('git', 'checkout')))
                    record = next((root / '.staging/releases').iterdir())
                    self.assertEqual((record / 'status').read_text(), 'success\n')
                    prune.assert_called_once_with(root / 'backups', 7)

    @unittest.skipIf(sys.platform == 'win32', 'Блокировка выполняется в Ubuntu VM')
    def test_parallel_deploy_stops_before_git_and_backup(self):
        import fcntl

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
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

"""Диагностика ручного стенда не ждёт бесконечно и не читает логи приложений."""

import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('ci_local', ROOT / 'scripts/ci-local.py')
local = importlib.util.module_from_spec(spec)
spec.loader.exec_module(local)


class DiagnosticsTest(unittest.TestCase):
    def test_before_compose_exists_no_docker_commands(self):
        with tempfile.TemporaryDirectory() as directory, mock.patch.object(
            local.subprocess, 'run'
        ) as run:
            local.diagnostics(Path(directory))
            run.assert_not_called()

    def test_status_and_build_log_have_deadlines(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / '.local').mkdir()
            (root / '.local/compose.yaml').touch()
            with mock.patch.object(local.subprocess, 'run') as run:
                local.diagnostics(root)
            self.assertEqual(len(run.call_args_list), 2)
            commands = [call.args[0] for call in run.call_args_list]
            self.assertEqual(commands[0][-2:], ['ps', '--all'])
            self.assertEqual(commands[1][-1], 'prepare')
            self.assertTrue(all(call.kwargs['timeout'] == 10 for call in run.call_args_list))
            with mock.patch.object(
                local.subprocess, 'run', side_effect=subprocess.TimeoutExpired('docker', 10)
            ):
                local.diagnostics(root)

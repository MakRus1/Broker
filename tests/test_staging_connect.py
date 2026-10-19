"""Завершение удалённых пробросов без подключения к настоящему кластеру."""

import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest


@unittest.skipUnless(sys.platform == 'linux', 'Требуется Bash Ubuntu, как на staging')
class StagingConnectTest(unittest.TestCase):
    def test_forward_failure_stops_other_forward(self):
        self._check_cleanup(fail=True)

    def test_disconnect_stops_both_forwards(self):
        self._check_cleanup(fail=False)

    def _check_cleanup(self, *, fail):
        source = (Path(__file__).resolve().parents[1] / 'scripts/staging-connect.ps1').read_text()
        remote = source.split("$remoteScript = @'\n", 1)[1].split("\n'@", 1)[0]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            stub = root / 'kubectl'
            stub.write_text(
                '#!/bin/bash\n'
                'name=keycloak\n'
                '[[ "$*" == *admin-bff* ]] && name=admin-bff\n'
                'trap \'touch "$MARKERS/$name.stopped"; exit 0\' TERM\n'
                'touch "$MARKERS/$name.started"\n'
                'if [[ "$FAIL" == 1 && "$name" == admin-bff ]]; then\n'
                '  while [[ ! -f "$MARKERS/keycloak.started" ]]; do sleep 0.05; done\n'
                '  exit 7\n'
                'fi\n'
                'while true; do sleep 0.05; done\n'
            )
            stub.chmod(0o700)
            process = subprocess.Popen(
                ['bash', '-c', remote],
                env={
                    **os.environ,
                    'PATH': f'{root}:/usr/bin:/bin',
                    'MARKERS': str(root),
                    'FAIL': '1' if fail else '0',
                },
                start_new_session=True,
            )
            try:
                if not fail:
                    deadline = time.monotonic() + 5
                    while not all(
                        (root / f'{name}.started').exists() for name in ('admin-bff', 'keycloak')
                    ):
                        if time.monotonic() > deadline:
                            self.fail('Пробросы не запустились')
                        time.sleep(0.05)
                    process.send_signal(signal.SIGHUP)
                self.assertEqual(process.wait(timeout=5), 7 if fail else 143)
                self.assertTrue((root / 'keycloak.stopped').exists())
                if not fail:
                    self.assertTrue((root / 'admin-bff.stopped').exists())
            finally:
                # Удаляем только отдельную группу процессов этого теста при зависании.
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                process.wait()

"""Таймауты настройки Keycloak не оставляют зависший kubectl и не раскрывают секреты."""
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import keycloak_command
import organization_identity
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts/openapi_gen'))


class KeycloakCommandTest(unittest.TestCase):
    def test_real_hanging_process_is_killed_and_waited(self):
        with self.assertRaisesRegex(RuntimeError, 'превышен таймаут') as raised:
            keycloak_command.run([sys.executable, '-c', 'import time; time.sleep(30)',
                                  'secret-command-value'], operation='проверка', timeout=.1,
                                  input='secret-input-value', text=True, capture_output=True)
        self.assertNotIn('secret-command-value', str(raised.exception))
        self.assertNotIn('secret-input-value', str(raised.exception))
        self.assertIsNone(raised.exception.__cause__)

    def test_success_and_failure_are_not_retried(self):
        with patch('keycloak_command.subprocess.run') as execute:
            keycloak_command.run(['kubectl'], operation='проверка')
            execute.assert_called_once_with(['kubectl'], check=True,
                                            timeout=keycloak_command.COMMAND_TIMEOUT)
        with patch('keycloak_command.subprocess.run', side_effect=subprocess.CalledProcessError(1, ['kubectl'])) as execute:
            with self.assertRaises(subprocess.CalledProcessError):
                keycloak_command.run(['kubectl'], operation='проверка')
            self.assertEqual(execute.call_count, 1)

    def test_cleanup_after_login_timeout_does_not_hide_original_error(self):
        with patch('keycloak_command.subprocess.run', side_effect=[
            subprocess.TimeoutExpired(['secret'], 120),
            subprocess.TimeoutExpired(['cleanup'], 15),
        ]) as execute, self.assertWarnsRegex(RuntimeWarning, 'временные файлы'):
            with self.assertRaisesRegex(RuntimeError, 'авторизация служебной настройки'):
                organization_identity.configure_cluster('default', 'broker-staging', state_dir=Path('/tmp'))
        self.assertEqual(execute.call_count, 2)
        self.assertEqual(execute.call_args_list[1].kwargs['timeout'], 15)

    def test_admin_api_timeout_runs_cleanup(self):
        with patch('keycloak_command.subprocess.run') as execute, \
             patch('service_accounts.load', return_value={}), \
             patch('service_accounts.provision', side_effect=lambda api, *args: api('clients')):
            execute.side_effect = [subprocess.CompletedProcess([], 0),
                                   subprocess.TimeoutExpired(['secret'], 120),
                                   subprocess.CompletedProcess([], 0)]
            with self.assertRaisesRegex(RuntimeError, 'запрос к Admin API'):
                organization_identity.configure_cluster('default', 'broker-staging', state_dir=Path('/tmp'))
            self.assertEqual(execute.call_count, 3)
            self.assertEqual(execute.call_args.kwargs['timeout'], 15)

    def test_browser_sync_is_bounded(self):
        import staging_identity
        with patch('staging_identity.keycloak_script', return_value='sensitive-script'), \
             patch('keycloak_command.subprocess.run', side_effect=subprocess.TimeoutExpired(['secret'], 120)) as execute:
            with self.assertRaisesRegex(RuntimeError, 'синхронизация браузерных клиентов'):
                staging_identity.sync_keycloak('default', 'broker-staging', 'https://admin.example', 'https://web.example')
            self.assertEqual(execute.call_args.kwargs['timeout'], 120)
            self.assertEqual(execute.call_count, 1)

    def test_cleanup_failure_is_reported_after_success(self):
        with patch('keycloak_command.subprocess.run') as execute, \
             patch('service_accounts.load', return_value={}), \
             patch('service_accounts.provision'):
            execute.side_effect = [subprocess.CompletedProcess([], 0),
                                   subprocess.TimeoutExpired(['cleanup'], 15)]
            with self.assertRaisesRegex(RuntimeError, 'очистка временных файлов'):
                organization_identity.configure_cluster('default', 'broker-staging',
                                                        state_dir=Path('/tmp'), configure_users=False)

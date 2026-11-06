"""Ограниченное ожидание kubectl exec при настройке Keycloak."""
import subprocess

COMMAND_TIMEOUT = 120
CLEANUP_TIMEOUT = 15


def run(command, *, operation, timeout=COMMAND_TIMEOUT, **kwargs):
    try:
        return subprocess.run(command, timeout=timeout, check=True, **kwargs)
    except subprocess.TimeoutExpired:
        # TimeoutExpired содержит команду и stdin, где могут быть секреты.
        raise RuntimeError(
            f'Keycloak: {operation}: превышен таймаут {timeout} с. '
            'Локальный kubectl остановлен; проверьте состояние Keycloak и повторите деплой.'
        ) from None

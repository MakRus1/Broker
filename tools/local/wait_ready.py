"""Проверка readiness внутри контейнера обновляемого локального сервиса."""

import argparse
import time
import urllib.error
import urllib.request


def wait(port, timeout):
    deadline = time.monotonic() + timeout
    url = f'http://127.0.0.1:{port}/ping/ready'
    last_error = ''
    while time.monotonic() < deadline:
        try:
            with urllib.request.urlopen(
                url, timeout=min(2, max(0.1, deadline - time.monotonic()))
            ) as response:
                if response.status == 200:
                    print(f'Readiness OK: {url}', flush=True)
                    return
                last_error = f'HTTP {response.status}'
        except (urllib.error.URLError, OSError) as error:
            last_error = str(error)
        time.sleep(min(1, max(0, deadline - time.monotonic())))
    raise RuntimeError(f'Readiness не достигнута за {timeout} с: {url}: {last_error}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, required=True)
    parser.add_argument('--timeout', type=float, default=120)
    args = parser.parse_args()
    if not 1 <= args.port <= 65535 or args.timeout <= 0:
        parser.error('Нужны корректный порт и положительный timeout')
    wait(args.port, args.timeout)

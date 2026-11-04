"""HTTP и временные туннели для регистрации релизных каталогов."""
from contextlib import contextmanager
import json
import socket
import subprocess
import tempfile
import time

def request_json(url, body, token=None, form=False):
    from urllib.request import Request, urlopen
    from urllib.parse import urlencode
    from urllib.error import HTTPError
    data = urlencode(body).encode() if form else json.dumps(body).encode()
    headers = {'Content-Type': 'application/x-www-form-urlencoded' if form else 'application/json'}
    if token:
        headers['Authorization'] = 'Bearer ' + token
    try:
        with urlopen(Request(url, data=data, headers=headers), timeout=30) as response:
            return json.load(response)
    except HTTPError as error:
        # Не включаем токены, секреты и тело HTTP-запроса в ошибку пайплайна.
        raise RuntimeError(f'Регистрация каталога: HTTP {error.code}') from None



@contextmanager
def forward_service(kubectl, service):
    with socket.socket() as listener:
        listener.bind(('127.0.0.1', 0))
        port = listener.getsockname()[1]
    with tempfile.TemporaryFile() as log:
        process = subprocess.Popen([*kubectl, 'port-forward', '--address=127.0.0.1',
            'service/' + service, f'{port}:8080'], stdout=log, stderr=log)
        try:
            for _ in range(100):
                if process.poll() is not None:
                    raise RuntimeError('Не удалось открыть туннель к ' + service)
                try:
                    with socket.create_connection(('127.0.0.1', port), timeout=.2):
                        break
                except OSError:
                    time.sleep(.1)
            else:
                raise RuntimeError('Таймаут туннеля к ' + service)
            yield f'http://127.0.0.1:{port}'
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()


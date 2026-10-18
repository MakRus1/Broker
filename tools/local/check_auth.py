"""Проверка настоящих токенов Keycloak без изменения конфигов."""
import json
import urllib.error
import urllib.request
from config_api import ConfigAdmin


def check():
    base = 'http://127.0.0.1:18092'
    admin = ConfigAdmin(base)
    for user, path, body, expected in [
        (None, '/admin/configs/list', None, 401),
        ('author', '/admin/configs/list', None, 200),
        ('reviewer', '/admin/configs/list', None, 200),
        ('author', '/admin/configs/drafts/approve', {}, 403),
        ('reviewer', '/admin/configs/drafts/create', {}, 403),
    ]:
        headers = {'Content-Type': 'application/json'}
        if user:
            headers['Authorization'] = 'Bearer ' + admin.token(user)
        request = urllib.request.Request(base + path,
            data=None if body is None else json.dumps(body).encode(), headers=headers)
        try:
            with urllib.request.urlopen(request, timeout=5) as response:
                status = response.status
        except urllib.error.HTTPError as error:
            status = error.code
        assert status == expected, (user, path, status, expected)
    print('Keycloak: подпись токенов, audience, доступ и разграничение ролей — OK', flush=True)


if __name__ == '__main__':
    check()

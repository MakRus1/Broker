"""Обновляем только настройки локального OIDC-клиента, не трогая пользователей и роли."""
import json
from pathlib import Path
import time
import urllib.parse
import urllib.request

secrets = json.loads(Path('/local/auth-secrets.json').read_text())
base = 'http://keycloak:8080'
credentials = urllib.parse.urlencode({
    'grant_type': 'password', 'client_id': 'admin-cli',
    'username': 'admin', 'password': secrets['admin-password'],
}).encode()
for attempt in range(90):
    try:
        with urllib.request.urlopen(base + '/realms/master/protocol/openid-connect/token',
                                    data=credentials, timeout=3) as response:
            token = json.load(response)['access_token']
        break
    except OSError:
        if attempt == 89:
            raise RuntimeError('Локальный Keycloak недоступен; проверьте bootstrap-администратора') from None
        time.sleep(1)

headers = {'Authorization': 'Bearer ' + token, 'Content-Type': 'application/json'}
url = base + '/admin/realms/broker/clients'
with urllib.request.urlopen(urllib.request.Request(url + '?clientId=broker-admin', headers=headers), timeout=5) as response:
    clients = json.load(response)
if len(clients) != 1:
    raise RuntimeError('Не найден единственный локальный клиент broker-admin')
client = clients[0]
client['redirectUris'] = ['http://localhost:13080/auth/callback']
client['attributes'] = {**client.get('attributes', {}),
                        'post.logout.redirect.uris': 'http://localhost:13080/'}
with urllib.request.urlopen(urllib.request.Request(url + '/' + client['id'], method='PUT',
        headers=headers, data=json.dumps(client).encode()), timeout=5):
    pass
print('Локальный OIDC-клиент админки настроен')

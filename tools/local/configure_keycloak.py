"""Настраиваем локальные OIDC-клиенты и разделяем права тестовых пользователей."""
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

# Подключение пользовательского приложения к существующему realm не меняет пароли.
def api(path, value=None, method=None):
    request = urllib.request.Request(base + '/admin/realms/broker/' + path,
        headers=headers, method=method or ('POST' if value is not None else 'GET'),
        data=None if value is None else json.dumps(value).encode())
    with urllib.request.urlopen(request, timeout=5) as response:
        data = response.read()
        return json.loads(data) if data else None

realm = json.loads(Path('/workspaces/broker/tools/local/keycloak/realm.template.json').read_text()
                   .replace('${shipments-dispatcher-secret}', secrets['shipments-dispatcher-secret']))
for desired in realm['clients']:
    if desired['clientId'] not in ('broker-logistics', 'broker-web', 'shipments-dispatcher'):
        continue
    found = api('clients?clientId=' + desired['clientId'])
    if found:
        api('clients/' + found[0]['id'], {**found[0], **desired}, method='PUT')
    else:
        api('clients', desired)
resource = api('clients?clientId=broker-logistics')[0]['id']
for role in realm['roles']['client']['broker-logistics']:
    existing = api('clients/' + resource + '/roles')
    if not any(item['name'] == role['name'] for item in existing):
        api('clients/' + resource + '/roles', role)
roles = {role['name']: role for role in api('clients/' + resource + '/roles')}
users = []
for username, names in (
    ('author', ['shipments.read', 'shipments.write', 'transportations.read', 'transportations.write', 'counterparties.read', 'counterparties.write']),
    ('reviewer', ['shipments.read', 'transportations.read', 'counterparties.read']),
):
    user = api('users?username=' + username + '&exact=true')[0]
    api('users/' + user['id'] + '/role-mappings/clients/' + resource, [roles[name] for name in names])
    users.append(user['id'])
dispatcher = api('clients?clientId=shipments-dispatcher')[0]['id']
account = api('clients/' + dispatcher + '/service-account-user')
api('users/' + account['id'] + '/role-mappings/clients/' + resource, [roles['transportations.create']])
users.append(account['id'])
Path('/cache/logistics-users.json').write_text(json.dumps(users))
print('Локальные роли и клиенты логистики настроены')

# Выдача полномочий платформы author допустима только в локальном bootstrap.
import sys
sys.path.insert(0, '/workspaces/broker/scripts')
from organization_identity import configure
configure(api, secrets['organizations-manager-secret'], platform_admin='author')

# Только тестовые технические клиенты: права берутся из Organizations, не из JWT.
platform_accounts = {}
for client_id in ['local-author', 'local-reviewer']:
    client = api('clients?clientId=' + client_id)[0]
    mapper = {'name': 'broker-logistics-audience', 'protocol': 'openid-connect',
        'protocolMapper': 'oidc-audience-mapper', 'consentRequired': False,
        'config': {'included.client.audience': 'broker-logistics', 'access.token.claim': 'true', 'id.token.claim': 'false'}}
    client['protocolMappers'] = [item for item in client.get('protocolMappers', []) if item['name'] != mapper['name']] + [mapper]
    api('clients/' + client['id'], client, method='PUT')
    platform_accounts[client_id] = api('clients/' + client['id'] + '/service-account-user')['id']
Path('/cache/platform-users.json').write_text(json.dumps(platform_accounts))

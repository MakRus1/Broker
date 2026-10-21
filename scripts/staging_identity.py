"""Обновление браузерных клиентов существующего личного staging без смены паролей."""

import base64
import json
from pathlib import Path
import shlex
import subprocess
from urllib.parse import urlsplit


def keycloak_script(admin_origin, web_origin, *, root=Path(__file__).resolve().parents[1]):
    for origin in (admin_origin, web_origin):
        parsed = urlsplit(origin)
        if (
            parsed.scheme not in ('http', 'https')
            or not parsed.hostname
            or parsed.path
            or parsed.query
            or parsed.fragment
            or parsed.username
            or parsed.password
        ):
            raise ValueError('Ожидается origin без пути и параметров')
    realm = json.loads((root / 'tools/local/keycloak/realm.template.json').read_text())
    # Важен порядок: сначала ресурс ролей, затем браузерные клиенты.
    clients = {client['clientId']: client for client in realm['clients']}
    script = '''set -eu
umask 077
folder=$(mktemp -d)
trap 'rm -rf "$folder"' EXIT
config=$folder/session
kcadm=/opt/keycloak/bin/kcadm.sh
$kcadm config credentials --server http://localhost:8080 --realm master --user admin --password "$KC_BOOTSTRAP_ADMIN_PASSWORD" --config "$config" >/dev/null
'''
    for client_id in ('broker-logistics', 'broker-web', 'broker-admin'):
        client = clients[client_id]
        origin = {'broker-web': web_origin, 'broker-admin': admin_origin}.get(client_id)
        if origin:
            client['redirectUris'] = [origin + '/auth/callback']
            client['webOrigins'] = [origin]
            client['attributes']['post.logout.redirect.uris'] = origin + '/'
        encoded = base64.b64encode(json.dumps(client).encode()).decode()
        script += f'''printf '%s' '{encoded}' | base64 -d > "$folder/client.json"
id=$($kcadm get clients -r broker -q clientId={client_id} --fields id --format csv --noquotes --config "$config")
if test -z "$id"; then
    id=$($kcadm create clients -r broker -f "$folder/client.json" -i --config "$config")
fi
'''
        if origin:
            # Для существующих клиентов меняем только точные callback/logout адреса.
            updates = {
                'redirectUris': [origin + '/auth/callback'],
                'webOrigins': [origin],
                'attributes."post.logout.redirect.uris"': origin + '/',
            }
            args = ' '.join(
                '-s ' + shlex.quote(key + '=' + json.dumps(value)) for key, value in updates.items()
            )
            script += f'$kcadm update "clients/$id" -r broker --config "$config" {args}\n'
        if client_id == 'broker-logistics':
            for role in realm['roles']['client']['broker-logistics']:
                name = shlex.quote(role['name'])
                script += f'''if ! $kcadm get "clients/$id/roles/"{name} -r broker --config "$config" >/dev/null 2>&1; then
    $kcadm create "clients/$id/roles" -r broker --config "$config" -s name={name} >/dev/null
fi
'''
    for user in realm['users']:
        if user['username'] not in ('author', 'reviewer'):
            continue
        roles = user.get('clientRoles', {}).get('broker-logistics', [])
        if roles:
            args = ' '.join('--rolename ' + shlex.quote(role) for role in roles)
            script += f'$kcadm add-roles -r broker --config "$config" --uusername {shlex.quote(user["username"])} --cclientid broker-logistics {args}\n'
    return script


def sync_keycloak(context, namespace, admin_origin, web_origin):
    script = keycloak_script(admin_origin, web_origin)
    subprocess.run(
        [
            'kubectl',
            '--context',
            context,
            '-n',
            namespace,
            'exec',
            'deployment/keycloak',
            '--',
            'sh',
            '-c',
            script,
        ],
        check=True,
    )

"""Идемпотентная настройка Keycloak для управления сотрудниками."""


def configure(api, secret, *, platform_admin=None):
    api(
        '',
        {
            'loginTheme': 'broker',
            'displayName': 'Broker',
            'internationalizationEnabled': True,
            'supportedLocales': ['ru', 'en'],
            'defaultLocale': 'ru',
        },
        method='PUT',
    )
    clients = api('clients?clientId=organizations-manager')
    desired = {
        'clientId': 'organizations-manager',
        'protocol': 'openid-connect',
        'enabled': True,
        'publicClient': False,
        'serviceAccountsEnabled': True,
        'standardFlowEnabled': False,
        'directAccessGrantsEnabled': False,
        'fullScopeAllowed': False,
        'secret': secret,
    }
    if clients:
        api('clients/' + clients[0]['id'], {**clients[0], **desired}, method='PUT')
    else:
        api('clients', desired)
    manager = api('clients?clientId=organizations-manager')[0]['id']
    account = api('clients/' + manager + '/service-account-user')
    management = api('clients?clientId=realm-management')[0]['id']
    names = {'manage-users', 'query-users', 'view-users'}
    roles = [role for role in api('clients/' + management + '/roles') if role['name'] in names]
    if {role['name'] for role in roles} != names:
        raise ValueError('В Keycloak отсутствуют необходимые роли управления пользователями')
    api('users/' + account['id'] + '/role-mappings/clients/' + management, roles)
    api('clients/' + manager + '/scope-mappings/clients/' + management, roles)
    # Служебная метка недоступна для изменения через пользовательский профиль.
    profile = api('users/profile')
    attributes = profile['attributes']
    marker = {
        'name': 'broker-provisioning-id',
        'multivalued': False,
        'permissions': {'view': ['admin'], 'edit': ['admin']},
        'validations': {'length': {'max': 36}},
    }
    found = next((item for item in attributes if item['name'] == marker['name']), None)
    if found != marker:
        profile['attributes'] = [item for item in attributes if item['name'] != marker['name']] + [
            marker
        ]
        api('users/profile', profile, method='PUT')
    browser = api('clients?clientId=broker-admin')[0]
    mappers = browser.get('protocolMappers', [])
    if not any(item.get('name') == 'broker-logistics-audience' for item in mappers):
        mapper = {
            'name': 'broker-logistics-audience',
            'protocol': 'openid-connect',
            'protocolMapper': 'oidc-audience-mapper',
            'consentRequired': False,
            'config': {
                'included.client.audience': 'broker-logistics',
                'access.token.claim': 'true',
                'id.token.claim': 'false',
            },
        }
        api('clients/' + browser['id'] + '/protocol-mappers/models', mapper)
    resource = api('clients?clientId=broker-logistics')[0]['id']
    role_path = 'clients/' + resource + '/roles'
    roles = api(role_path)
    if not any(role['name'] == 'platform.admin' for role in roles):
        api(
            role_path,
            {'name': 'platform.admin', 'description': 'Управление всеми организациями платформы'},
        )
    if platform_admin:
        users = api('users?username=' + platform_admin + '&exact=true')
        if len(users) != 1:
            raise ValueError(
                'Не найден единственный пользователь для начального администратора платформы'
            )
        role = next(role for role in api(role_path) if role['name'] == 'platform.admin')
        api('users/' + users[0]['id'] + '/role-mappings/clients/' + resource, [role])


def configure_cluster(context, namespace, secret, *, platform_admin=None):
    import json
    import subprocess
    import uuid
    from urllib.parse import parse_qsl, urlsplit

    folder = '/tmp/broker-organizations-' + uuid.uuid4().hex
    command = [
        'kubectl',
        '--context',
        context,
        '-n',
        namespace,
        'exec',
        '-i',
        'deployment/keycloak',
        '--',
        'sh',
        '-c',
    ]
    subprocess.run(
        command
        + [
            'umask 077; mkdir "$1"; /opt/keycloak/bin/kcadm.sh config credentials '
            '--server http://localhost:8080 --realm master --user admin '
            '--password "$KC_BOOTSTRAP_ADMIN_PASSWORD" --config "$1/session" >/dev/null',
            'sh',
            folder,
        ],
        check=True,
        capture_output=True,
    )

    def api(path, value=None, method=None):
        parsed = urlsplit(path)
        verb = {'GET': 'get', 'POST': 'create', 'PUT': 'update'}[
            method or ('POST' if value is not None else 'GET')
        ]
        args = [
            '/opt/keycloak/bin/kcadm.sh',
            verb,
            parsed.path,
            '-r',
            'broker',
            '--config',
            folder + '/session',
        ]
        for key, item in parse_qsl(parsed.query):
            args += ['-q', key + '=' + item]
        if value is not None:
            args += ['-f', folder + '/request.json']
        result = subprocess.run(
            command
            + [
                'umask 077; folder=$1; shift; cat > "$folder/request.json"; exec "$@"',
                'sh',
                folder,
                *args,
            ],
            input='' if value is None else json.dumps(value),
            text=True,
            check=True,
            capture_output=True,
        )
        return json.loads(result.stdout) if verb == 'get' else None

    try:
        configure(api, secret, platform_admin=platform_admin)
    finally:
        subprocess.run(command + ['rm -rf "$1"', 'sh', folder], check=True, capture_output=True)


if __name__ == '__main__':
    import argparse
    from pathlib import Path

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--context', default='default')
    parser.add_argument('--namespace', default='broker-staging')
    parser.add_argument('--platform-admin')
    parser.add_argument('--state-dir', type=Path, default=Path('.staging'))
    args = parser.parse_args()
    configure_cluster(
        args.context,
        args.namespace,
        (args.state_dir / 'organizations-manager-secret').read_text().strip(),
        platform_admin=args.platform_admin,
    )
    print('Управление сотрудниками в Keycloak настроено')

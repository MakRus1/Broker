#!/usr/bin/env python3
"""Рендер манифестов личного staging без обращения к Kubernetes."""

import hashlib
import base64
import json
import os
from pathlib import Path
import re
import secrets
import sys
from urllib.parse import urlsplit

import yaml
import staging_lan

sys.path.insert(0, str(Path(__file__).parent / 'openapi_gen'))
from deployment import merge
from generate import UniqueLoader
from migrations import discover, render as migration_sql

ROOT = Path(__file__).resolve().parents[1]
from service_catalog import discover as discover_services

SERVICES = discover_services()


def read_yaml(path):
    return yaml.load(path.read_text(), Loader=UniqueLoader)


def private_write(path, text):
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    os.fchmod(descriptor, 0o600)
    with os.fdopen(descriptor, 'w') as target:
        target.write(text)


def credentials(directory):
    directory.mkdir(parents=True, exist_ok=True, mode=0o700)
    directory.chmod(0o700)
    path = directory / 'credentials.json'
    if not path.exists():
        keys = ('postgres-password', 'admin-password', 'author-password', 'reviewer-password')
        private_write(
            path, json.dumps({key: secrets.token_urlsafe(32) for key in keys}, indent=2) + '\n'
        )
    values = json.loads(path.read_text())
    if set(values) != set(
        ('postgres-password', 'admin-password', 'author-password', 'reviewer-password')
    ):
        raise ValueError('credentials.json: неверный набор ключей')
    if any(
        not isinstance(value, str) or not re.fullmatch(r'[A-Za-z0-9_-]{32,}', value)
        for value in values.values()
    ):
        raise ValueError('credentials.json: ожидаются сгенерированные пароли')
    return values


def document(kind, name, namespace, **fields):
    if kind == 'Secret' and 'stringData' in fields:
        fields['data'] = {
            key: base64.b64encode(value.encode()).decode()
            for key, value in fields.pop('stringData').items()
        }
    api = {'Deployment': 'apps/v1', 'StatefulSet': 'apps/v1', 'Job': 'batch/v1'}.get(kind, 'v1')
    return {
        'apiVersion': api,
        'kind': kind,
        'metadata': {'name': name.replace('_', '-'), 'namespace': namespace},
        **fields,
    }


def hashed(name, data):
    digest = hashlib.sha256(json.dumps(data, sort_keys=True).encode()).hexdigest()[:12]
    return f"{name.replace('_', '-')}-{digest}"


def service(name, namespace, ports, headless=False):
    spec = {'selector': {'app.kubernetes.io/name': name}, 'ports': ports}
    if headless:
        spec['clusterIP'] = 'None'
    return document('Service', name, namespace, spec=spec)


def workload(
    kind, name, namespace, container, *, replicas=1, volumes=None, annotations=None, init=None
):
    pod = {'automountServiceAccountToken': False, 'containers': [container]}
    if volumes:
        pod['volumes'] = volumes
    if init:
        pod['initContainers'] = init
    labels = {'app.kubernetes.io/name': name}
    spec = {
        'replicas': replicas,
        'selector': {'matchLabels': labels},
        'template': {
            'metadata': {'labels': labels, 'annotations': annotations or {}},
            'spec': pod,
        },
    }
    if kind == 'Deployment':
        spec.update(
            {
                'revisionHistoryLimit': 3,
                'progressDeadlineSeconds': 300,
                'strategy': {
                    'type': 'RollingUpdate',
                    'rollingUpdate': {'maxSurge': 1, 'maxUnavailable': 0},
                },
            }
        )
    else:
        spec['serviceName'] = name
    return document(kind, name, namespace, spec=spec)


def secret_env(name, secret, key):
    return {'name': name, 'valueFrom': {'secretKeyRef': {'name': secret, 'key': key}}}


def port(name, number):
    return {'name': name, 'port': number, 'targetPort': number}


def render(release, directory, *, root=ROOT, environment='staging'):
    services = discover_services(root)
    platform = read_yaml(root / 'platform.yaml')
    if set(platform) != {'version', 'environments'} or platform['version'] != 1:
        raise ValueError('platform.yaml: ожидаются version: 1/environments')
    settings = platform['environments'][environment]
    allowed = {
        'namespace',
        'kubernetes_version',
        'storage_class',
        'cluster_domain',
        'database_storage',
        'admin_origin',
        'web_origin',
        'auth_origin',
        'defaults',
        'services',
    }
    if set(settings) - {'lan'} != allowed or set(settings['services']) - set(services):
        raise ValueError('platform.yaml: неверные поля окружения или состав сервисов')
    namespace = settings['namespace']
    if not re.fullmatch(r'[a-z](?:[a-z0-9-]{0,61}[a-z0-9])?', namespace):
        raise ValueError('platform.yaml: неверный namespace')
    lan = staging_lan.settings(settings, directory)
    if lan:
        settings['admin_origin'] = 'https://' + lan['admin_host']
        settings['web_origin'] = 'https://' + lan['web_host']
        settings['auth_origin'] = 'https://' + lan['auth_host']
    for key in ('admin_origin', 'web_origin', 'auth_origin'):
        url = urlsplit(settings[key])
        if not lan and (
            url.scheme != 'http' or url.hostname != 'localhost' or not url.port or url.path
        ):
            raise ValueError(f'platform.yaml: {key} ожидает http://localhost:<port>')
    if set(release) != {'revision', 'images'} or not re.fullmatch(
        r'[0-9a-f]{40}', release['revision']
    ):
        raise ValueError('release.json: ожидается полный git SHA и images')
    if set(release['images']) != set(services) or any(
        not re.fullmatch(r'ghcr.io/[a-z0-9_./-]+@sha256:[0-9a-f]{64}', image)
        for image in release['images'].values()
    ):
        raise ValueError('release.json: нужны образы всех сервисов GHCR с digest')
    deployments = {}
    for name in services:
        manifest = read_yaml(root / f'services/{name}/service.yaml')
        deployments[name] = merge(
            settings['defaults'], manifest.get('deployment', {}), settings['services'].get(name, {})
        )
        # Сессии BFF пока хранятся в памяти одного процесса.
        if name in ('admin_bff', 'web_bff') and deployments[name]['replicas'] != 1:
            raise ValueError(f'{name}: несколько реплик требуют общего хранилища сессий')
    if not re.fullmatch(r'[a-z0-9]+(?:[.-][a-z0-9]+)*', settings['cluster_domain']):
        raise ValueError('platform.yaml: неверный cluster_domain')
    if not re.fullmatch(r'[1-9][0-9]*(?:Mi|Gi)', settings['database_storage']):
        raise ValueError('platform.yaml: database_storage ожидает размер в Mi/Gi')
    addresses = {
        name: f"{name.replace('_', '-')}.{namespace}.svc.{settings['cluster_domain']}"
        for name in ('postgres', 'configs', 'keycloak', *services)
    }
    values = credentials(directory)
    images = json.loads((root / 'tools/local/images.json').read_text())['images']
    ns = {'apiVersion': 'v1', 'kind': 'Namespace', 'metadata': {'name': namespace}}
    infra = []
    infra.append(
        document(
            'Secret',
            'database',
            namespace,
            type='Opaque',
            stringData={'password': values['postgres-password']},
        )
    )
    databases = [
        name
        for name in services
        if read_yaml(root / f'services/{name}/service.yaml')
        .get('postgresql', {})
        .get('enabled', False)
    ]
    init_sql = ''.join(f'CREATE DATABASE {name};\n' for name in [*databases, 'keycloak'])
    infra.append(document('ConfigMap', 'database-init', namespace, data={'init.sql': init_sql}))
    infra.append(
        document(
            'PersistentVolumeClaim',
            'database',
            namespace,
            spec={
                'accessModes': ['ReadWriteOnce'],
                'storageClassName': settings['storage_class'],
                'resources': {'requests': {'storage': settings['database_storage']}},
            },
        )
    )
    pg = {
        'name': 'postgres',
        'image': images['postgres'],
        'env': [
            {'name': 'POSTGRES_USER', 'value': 'broker'},
            {'name': 'POSTGRES_DB', 'value': 'postgres'},
            secret_env('POSTGRES_PASSWORD', 'database', 'password'),
            {'name': 'PGDATA', 'value': '/var/lib/postgresql/data/pgdata'},
        ],
        'ports': [{'containerPort': 5432}],
        'resources': {
            'requests': {'cpu': '100m', 'memory': '128Mi'},
            'limits': {'memory': '512Mi'},
        },
        'volumeMounts': [
            {'name': 'data', 'mountPath': '/var/lib/postgresql/data'},
            {'name': 'init', 'mountPath': '/docker-entrypoint-initdb.d', 'readOnly': True},
        ],
        'readinessProbe': {
            'exec': {'command': ['pg_isready', '-h', '127.0.0.1', '-U', 'broker']},
            'periodSeconds': 5,
        },
        'startupProbe': {
            'exec': {'command': ['pg_isready', '-h', '127.0.0.1', '-U', 'broker']},
            'periodSeconds': 5,
            'failureThreshold': 60,
        },
    }
    infra += [
        service('postgres', namespace, [port('postgres', 5432)], True),
        workload(
            'StatefulSet',
            'postgres',
            namespace,
            pg,
            volumes=[
                {'name': 'data', 'persistentVolumeClaim': {'claimName': 'database'}},
                {'name': 'init', 'configMap': {'name': 'database-init'}},
            ],
        ),
    ]
    # В staging импортируются только браузерные пользователи, без тестовых service accounts.
    realm = json.loads((root / 'tools/local/keycloak/realm.template.json').read_text())
    realm['clients'] = [
        client
        for client in realm['clients']
        if client['clientId'] in ('configs-admin', 'broker-admin', 'broker-logistics', 'broker-web')
    ]
    realm['users'] = [user for user in realm['users'] if user['username'] in ('author', 'reviewer')]
    for client_id, origin in (
        ('broker-admin', settings['admin_origin']),
        ('broker-web', settings['web_origin']),
    ):
        browser_client = next(
            client for client in realm['clients'] if client['clientId'] == client_id
        )
        browser_client['redirectUris'] = [origin + '/auth/callback']
        browser_client['webOrigins'] = [origin]
        browser_client['attributes']['post.logout.redirect.uris'] = origin + '/'

    realm_text = json.dumps(realm)
    for key, value in values.items():
        realm_text = realm_text.replace('${' + key + '}', value)
    realm_secret = hashed('keycloak-realm', realm_text)
    infra.append(
        document(
            'Secret',
            realm_secret,
            namespace,
            type='Opaque',
            stringData={'broker-realm.json': realm_text},
            immutable=True,
        )
    )
    infra.append(
        document(
            'Secret',
            'keycloak-admin',
            namespace,
            type='Opaque',
            stringData={'password': values['admin-password']},
        )
    )
    keycloak = {
        'name': 'keycloak',
        'image': images['keycloak'],
        'args': ['start', '--import-realm'],
        'env': [
            {'name': 'KC_DB', 'value': 'postgres'},
            {
                'name': 'KC_DB_URL',
                'value': f"jdbc:postgresql://{addresses['postgres']}:5432/keycloak",
            },
            {'name': 'KC_DB_USERNAME', 'value': 'broker'},
            secret_env('KC_DB_PASSWORD', 'database', 'password'),
            {'name': 'KC_BOOTSTRAP_ADMIN_USERNAME', 'value': 'admin'},
            secret_env('KC_BOOTSTRAP_ADMIN_PASSWORD', 'keycloak-admin', 'password'),
            {'name': 'KC_HOSTNAME', 'value': settings['auth_origin']},
            {'name': 'KC_HTTP_ENABLED', 'value': 'true'},
            {'name': 'KC_HEALTH_ENABLED', 'value': 'true'},
            {'name': 'JAVA_OPTS_KC_HEAP', 'value': '-Xms128m -Xmx512m'},
        ],
        'ports': [{'containerPort': 8080}, {'containerPort': 9000}],
        'resources': {'requests': {'cpu': '250m', 'memory': '512Mi'}, 'limits': {'memory': '1Gi'}},
        'volumeMounts': [
            {'name': 'realm', 'mountPath': '/opt/keycloak/data/import', 'readOnly': True}
        ],
        'readinessProbe': {'httpGet': {'path': '/health/ready', 'port': 9000}, 'periodSeconds': 5},
        'startupProbe': {
            'httpGet': {'path': '/health/started', 'port': 9000},
            'periodSeconds': 5,
            'failureThreshold': 60,
        },
    }
    if lan:
        keycloak['env'] += [
            {'name': 'KC_PROXY_HEADERS', 'value': 'xforwarded'},
            {'name': 'KC_HTTPS_CERTIFICATE_FILE', 'value': '/tls/tls.crt'},
            {'name': 'KC_HTTPS_CERTIFICATE_KEY_FILE', 'value': '/tls/tls.key'},
            {'name': 'KC_HTTP_MANAGEMENT_SCHEME', 'value': 'http'},
        ]
        keycloak['volumeMounts'].append({'name': 'tls', 'mountPath': '/tls', 'readOnly': True})
    kc = workload(
        'Deployment',
        'keycloak',
        namespace,
        keycloak,
        volumes=[{'name': 'realm', 'secret': {'secretName': realm_secret}}],
    )
    kc['spec']['strategy'] = {'type': 'Recreate'}
    if lan:
        kc['spec']['template']['spec']['volumes'].append(
            {'name': 'tls', 'secret': {'secretName': 'staging-lan-tls'}}
        )
    infra += [service('keycloak', namespace, [port('http', 8080), port('https', 8443)]), kc]
    migrations, apps, jobs = [], [], []
    for name in services:
        app_start, migration_start = len(apps), len(migrations)
        static = read_yaml(root / f'services/{name}/configs/static_config.yaml')
        variables = read_yaml(root / f'services/{name}/configs/config_vars.yaml')
        variables.update(
            {
                'environment': environment,
                'is-testing': False,
                'worker-threads': 2,
                'worker-fs-threads': 1,
                'config-server-url': f"http://{addresses['configs']}:8090",
                'config-updates-enabled': name != 'configs',
                'config-cache-path': '',
                'otlp-enabled': False,
                'admin-assets-dir': '/app/assets',
                'web-assets-dir': '/app/web-assets',
                'web-origin': settings['web_origin'],
                'web-token-url': f"http://{addresses['keycloak']}:8080/realms/broker/protocol/openid-connect/token",
                'web-revoke-url': f"http://{addresses['keycloak']}:8080/realms/broker/protocol/openid-connect/revoke",
                'web-secure-cookie': False,
                'client-counterparties-base-url': f'http://{addresses["counterparties"]}:8080',
                'client-shipments-base-url': f"http://{addresses['shipments']}:8080",
                'client-transportations-base-url': f"http://{addresses['transportations']}:8080",
                'client-config_admin-base-url': f"http://{addresses['configs']}:8092",
                'auth-issuer': settings['auth_origin'] + '/realms/broker',
                'auth-jwks-url': f"http://{addresses['keycloak']}:8080/realms/broker/protocol/openid-connect/certs",
                'auth-allow-http': True,
                'admin-origin': settings['admin_origin'],
                'admin-token-url': f"http://{addresses['keycloak']}:8080/realms/broker/protocol/openid-connect/token",
                'admin-revoke-url': f"http://{addresses['keycloak']}:8080/realms/broker/protocol/openid-connect/revoke",
                'admin-secure-cookie': False,
                'pg-connection': f"postgresql://broker:{values['postgres-password']}@{addresses['postgres']}:5432/{name}",
            }
        )
        if lan:
            internal_auth = f"https://{addresses['keycloak']}:8443"
            variables.update(
                {
                    'admin-secure-cookie': True,
                    'web-secure-cookie': True,
                    'web-token-url': internal_auth + '/realms/broker/protocol/openid-connect/token',
                    'web-revoke-url': internal_auth
                    + '/realms/broker/protocol/openid-connect/revoke',
                    'auth-allow-http': False,
                    'auth-jwks-url': internal_auth + '/realms/broker/protocol/openid-connect/certs',
                    'admin-token-url': internal_auth
                    + '/realms/broker/protocol/openid-connect/token',
                    'admin-revoke-url': internal_auth
                    + '/realms/broker/protocol/openid-connect/revoke',
                    'otlp-enabled': True,
                    'otlp-endpoint': f'collector.{namespace}.svc.{settings["cluster_domain"]}:4317',
                }
            )
        components = static['components_manager']['components']
        if lan:
            # Логи читаются из stdout; OTLP используется для трасс, без двойной доставки.
            components['otlp-logger']['sinks']['logs'] = 'default'
        components['server']['listener-monitor']['address'] = '0.0.0.0'
        config_data = {'static_config.yaml': yaml.safe_dump(static, sort_keys=False)}
        vars_data = {'config_vars.yaml': yaml.safe_dump(variables, sort_keys=False)}
        config_name, vars_name = hashed(name + '-config', config_data), hashed(
            name + '-vars', vars_data
        )
        apps += [
            document('ConfigMap', config_name, namespace, data=config_data, immutable=True),
            document(
                'Secret', vars_name, namespace, type='Opaque', stringData=vars_data, immutable=True
            ),
        ]
        container = {
            'name': name.replace('_', '-'),
            'image': release['images'][name],
            'resources': deployments[name]['resources'],
            'ports': [
                {'containerPort': variables['server-port']},
                {'containerPort': variables['monitor-port']},
            ],
            'securityContext': {
                'runAsNonRoot': True,
                'runAsUser': 10001,
                'allowPrivilegeEscalation': False,
                'readOnlyRootFilesystem': True,
                'capabilities': {'drop': ['ALL']},
            },
            'volumeMounts': [
                {'name': 'config', 'mountPath': '/etc/broker', 'readOnly': True},
                {'name': 'vars', 'mountPath': '/etc/broker-secrets', 'readOnly': True},
                {'name': 'tmp', 'mountPath': '/tmp'},
            ],
            'startupProbe': {
                'httpGet': {'path': '/ping/ready', 'port': variables['monitor-port']},
                'periodSeconds': 3,
                'failureThreshold': 60,
            },
            'readinessProbe': {
                'httpGet': {'path': '/ping/ready', 'port': variables['monitor-port']},
                'periodSeconds': 5,
            },
            'livenessProbe': {
                'httpGet': {'path': '/ping', 'port': variables['monitor-port']},
                'periodSeconds': 10,
                'failureThreshold': 3,
            },
        }
        extra_volumes = []
        if lan:
            container['volumeMounts'].append(
                {
                    'name': 'ca',
                    'mountPath': '/etc/ssl/certs/ca-certificates.crt',
                    'subPath': 'ca-certificates.crt',
                    'readOnly': True,
                }
            )
            extra_volumes.append({'name': 'ca', 'configMap': {'name': 'staging-lan-ca'}})
        apps += [
            workload(
                'Deployment',
                name,
                namespace,
                container,
                replicas=deployments[name]['replicas'],
                volumes=[
                    {'name': 'config', 'configMap': {'name': config_name}},
                    {'name': 'vars', 'secret': {'secretName': vars_name}},
                    {'name': 'tmp', 'emptyDir': {}},
                    *extra_volumes,
                ],
                annotations={'broker/revision': release['revision']},
            ),
            service(
                name,
                namespace,
                [
                    port('http', variables['server-port']),
                    port('monitor', variables['monitor-port']),
                ],
            ),
        ]
        schema = root / f'services/{name}/postgresql/schemas/db_1'
        if name in databases:
            sql = migration_sql(discover(schema)) if schema.exists() else ''
            # initdb выполняется только на пустом томе; новый сервис создаёт БД перед миграцией.
            sql = (
                f"SELECT 'CREATE DATABASE {name}' WHERE NOT EXISTS (SELECT FROM pg_database WHERE datname = '{name}')\n"
                "\\gexec\n"
                f"\\connect {name}\n" + sql
            )
            migration_name = hashed(name + '-migrate', [sql, images['postgres']])
            migrations.append(
                document(
                    'ConfigMap',
                    migration_name,
                    namespace,
                    data={'migrate.sql': sql},
                    immutable=True,
                )
            )
            job = document(
                'Job',
                migration_name,
                namespace,
                spec={
                    'backoffLimit': 2,
                    'activeDeadlineSeconds': 180,
                    'template': {
                        'metadata': {'labels': {'app.kubernetes.io/name': name + '-migration'}},
                        'spec': {
                            'automountServiceAccountToken': False,
                            'restartPolicy': 'Never',
                            'containers': [
                                {
                                    'name': 'migrate',
                                    'image': images['postgres'],
                                    'command': [
                                        'psql',
                                        '-h',
                                        'postgres',
                                        '-U',
                                        'broker',
                                        '-d',
                                        'postgres',
                                        '-v',
                                        'ON_ERROR_STOP=1',
                                        '-f',
                                        '/sql/migrate.sql',
                                    ],
                                    'env': [secret_env('PGPASSWORD', 'database', 'password')],
                                    'resources': {
                                        'requests': {'cpu': '50m', 'memory': '32Mi'},
                                        'limits': {'memory': '128Mi'},
                                    },
                                    'volumeMounts': [
                                        {'name': 'sql', 'mountPath': '/sql', 'readOnly': True}
                                    ],
                                }
                            ],
                            'volumes': [{'name': 'sql', 'configMap': {'name': migration_name}}],
                        },
                    },
                },
            )
            migrations.append(job)
            jobs.append(migration_name)
        for obj in [*apps[app_start:], *migrations[migration_start:]]:
            obj['metadata'].setdefault('labels', {})['broker/service'] = name
    for obj in apps:
        if obj['kind'] == 'Deployment' and obj['metadata']['name'] == 'admin-bff':
            obj['spec']['strategy'] = {'type': 'Recreate'}
    phases = {
        'namespace': [ns],
        'infrastructure': infra,
        'migrations': migrations,
        'services': apps,
    }
    if lan:
        phases['monitoring'] = staging_lan.render(root, namespace, settings, lan)
    for phase, objects in phases.items():
        private_write(directory / f'{phase}.yaml', yaml.safe_dump_all(objects, sort_keys=False))
    private_write(
        directory / 'plan.json',
        json.dumps(
            {
                'namespace': namespace,
                'revision': release['revision'],
                'jobs': jobs,
                'kubernetes_version': settings['kubernetes_version'],
                'lan': bool(lan),
                'admin_origin': settings['admin_origin'],
                'web_origin': settings['web_origin'],
            },
            indent=2,
        )
        + '\n',
    )
    return phases

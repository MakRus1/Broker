#!/usr/bin/env python3
"""HTTPS и мониторинг личного staging; приватный CA остаётся на сервере."""

import argparse
import base64
import json
from pathlib import Path
import re
import secrets
import subprocess

import yaml

NAMES = ('grafana', 'prometheus', 'loki', 'tempo', 'collector')


def settings(platform, directory):
    if not (directory / 'lan/enabled').exists():
        return None
    value = platform.get('lan', {})
    if set(value) != {'admin_host', 'auth_host', 'grafana_host', 'telemetry_storage'}:
        raise ValueError('platform.yaml: неверные поля lan')
    for key in ('admin_host', 'auth_host', 'grafana_host'):
        if not re.fullmatch(r'[a-z0-9-]+(?:\.[a-z0-9-]+)+', value[key]):
            raise ValueError(f'Неверное имя lan.{key}')
    if len({value[key] for key in ('admin_host', 'auth_host', 'grafana_host')}) != 3:
        raise ValueError('LAN-имена должны различаться')
    if not re.fullmatch(r'[1-9][0-9]*Gi', value['telemetry_storage']):
        raise ValueError('lan.telemetry_storage ожидает Gi')
    return value


def initialize(root, directory, context):
    from staging_manifest import document, private_write, read_yaml

    cfg = read_yaml(root / 'platform.yaml')['environments']['staging']
    ns = cfg['namespace']
    folder = directory / 'lan'
    folder.mkdir(parents=True, exist_ok=True, mode=0o700)
    folder.chmod(0o700)
    # Сохраняем ключи между запусками; перевыпуск CA не выполняется автоматически.
    ca = folder / 'ca.crt'
    key = folder / 'ca.key'
    if ca.exists() != key.exists():
        raise ValueError('Неполный CA: восстановите ca.crt и ca.key из резервной копии')
    if not ca.exists():
        subprocess.run(
            [
                'openssl',
                'req',
                '-x509',
                '-newkey',
                'rsa:3072',
                '-nodes',
                '-keyout',
                str(key),
                '-out',
                str(ca),
                '-days',
                '3650',
                '-subj',
                '/CN=Broker Staging Local CA',
                '-addext',
                'basicConstraints=critical,CA:TRUE',
                '-addext',
                'keyUsage=critical,keyCertSign,cRLSign',
            ],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    hosts = [cfg['lan'][name] for name in ('admin_host', 'auth_host', 'grafana_host')]
    hosts.append(f'keycloak.{ns}.svc.{cfg["cluster_domain"]}')
    cert, leaf_key = folder / 'tls.crt', folder / 'tls.key'
    if not cert.exists() or not leaf_key.exists():
        subprocess.run(
            [
                'openssl',
                'req',
                '-new',
                '-newkey',
                'rsa:2048',
                '-nodes',
                '-keyout',
                str(leaf_key),
                '-out',
                str(folder / 'tls.csr'),
                '-subj',
                f'/CN={hosts[0]}',
            ],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        private_write(
            folder / 'extensions.cnf',
            'subjectAltName=' + ','.join('DNS:' + host for host in hosts) + '\n'
            'basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\n'
            'extendedKeyUsage=serverAuth\n',
        )
        subprocess.run(
            [
                'openssl',
                'x509',
                '-req',
                '-in',
                str(folder / 'tls.csr'),
                '-CA',
                str(ca),
                '-CAkey',
                str(key),
                '-CAcreateserial',
                '-out',
                str(cert),
                '-days',
                '397',
                '-extfile',
                str(folder / 'extensions.cnf'),
            ],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    for path in folder.iterdir():
        path.chmod(0o600)
    for host in hosts:
        subprocess.run(
            ['openssl', 'x509', '-in', str(cert), '-noout', '-checkhost', host], check=True
        )
    subprocess.run(['openssl', 'verify', '-CAfile', str(ca), str(cert)], check=True)
    password_path = folder / 'grafana-password'
    if not password_path.exists():
        private_write(password_path, secrets.token_urlsafe(32))
    objects = [
        document(
            'Secret',
            'staging-lan-tls',
            ns,
            type='kubernetes.io/tls',
            data={
                'tls.crt': base64.b64encode(cert.read_bytes()).decode(),
                'tls.key': base64.b64encode(leaf_key.read_bytes()).decode(),
            },
        ),
        document(
            'ConfigMap',
            'staging-lan-ca',
            ns,
            data={
                'ca-certificates.crt': Path('/etc/ssl/certs/ca-certificates.crt').read_text()
                + '\n'
                + ca.read_text()
            },
        ),
        document(
            'Secret',
            'grafana-admin',
            ns,
            type='Opaque',
            stringData={'password': password_path.read_text()},
        ),
    ]
    subprocess.run(
        [
            'kubectl',
            '--context',
            context,
            'apply',
            '--server-side',
            '--field-manager=broker-staging-lan',
            '-f',
            '-',
        ],
        input=yaml.safe_dump_all(objects),
        text=True,
        check=True,
    )
    private_write(folder / 'enabled', 'true\n')
    print(f'LAN подготовлен. Публичный CA: {ca}. Пароль Grafana: {password_path}')


def render(root, ns, platform, lan):
    from staging_manifest import document, hashed, port, read_yaml, service, workload

    images = json.loads((root / 'tools/local/images.json').read_text())['images']
    configs = {}
    prometheus = read_yaml(root / 'tools/local/prometheus.yaml')
    targets = []
    for name in ('broker', 'configs', 'admin_bff'):
        variables = read_yaml(root / f'services/{name}/configs/config_vars.yaml')
        targets.append(
            {
                'targets': [f'{name.replace("_", "-")}:{variables["monitor-port"]}'],
                'labels': {'environment': 'staging', 'service': name},
            }
        )
    prometheus['global']['scrape_interval'] = '15s'
    prometheus['scrape_configs'] = [
        {
            'job_name': 'services',
            'metrics_path': '/metrics',
            'honor_labels': True,
            'static_configs': targets,
        }
    ]
    configs['prometheus'] = {'config.yaml': yaml.safe_dump(prometheus)}
    configs['loki'] = {'config.yaml': (root / 'tools/local/loki.yaml').read_text()}
    configs['tempo'] = {'config.yaml': (root / 'tools/local/tempo.yaml').read_text()}
    collector = read_yaml(root / 'tools/local/collector.yaml')
    collector['receivers'] = {'otlp': collector['receivers']['otlp']}
    for name in ('broker', 'configs', 'admin_bff'):
        receiver = read_yaml(root / 'tools/local/collector.yaml')['receivers']['filelog/' + name]
        receiver['include'] = [
            f'/var/log/pods/{ns}_{name.replace("_", "-")}-*_*/{name.replace("_", "-")}/*.log*'
        ]
        receiver['include_file_path'] = True
        receiver['max_concurrent_files'] = 2
        receiver['compression'] = 'auto'
        receiver['retry_on_failure'] = {'enabled': True, 'max_elapsed_time': '0s'}
        receiver['operators'].insert(0, {'type': 'container', 'id': 'container-parser'})
        collector['receivers']['filelog/' + name] = receiver
    collector['processors']['resource/local']['attributes'][0]['value'] = 'staging'
    collector['processors']['memory_limiter'].update({'limit_mib': 384, 'spike_limit_mib': 64})
    collector['processors']['batch'].update({'send_batch_size': 256, 'send_batch_max_size': 256})
    collector['exporters']['otlp_http/loki']['sending_queue'] = {
        'num_consumers': 1,
        'queue_size': 64,
        'storage': 'file_storage',
    }
    collector['service']['pipelines']['logs']['receivers'] = [
        'filelog/' + name for name in ('broker', 'configs', 'admin_bff')
    ]
    configs['collector'] = {'config.yaml': yaml.safe_dump(collector)}
    datasources = read_yaml(root / 'tools/local/grafana/datasources/local.yaml')
    # Compose экранирует $, а Kubernetes передаёт содержимое ConfigMap напрямую.
    configs['grafana'] = {'datasources.yaml': yaml.safe_dump(datasources).replace('$$', '$')}
    dashboards = {
        f'{name}.json': (root / f'services/{name}/monitoring/grafana.json').read_text()
        for name in ('broker', 'configs', 'admin_bff')
    }
    provider = read_yaml(root / 'tools/local/grafana/dashboards/provider.yaml')
    provider['providers'][0]['name'] = 'Broker staging'
    dashboards['provider.yaml'] = yaml.safe_dump(provider)
    objects = []
    dashboard_name = hashed('grafana-dashboards', dashboards)
    objects.append(document('ConfigMap', dashboard_name, ns, data=dashboards))
    ports = {'grafana': 3000, 'prometheus': 9090, 'loki': 3100, 'tempo': 3200, 'collector': 4317}
    paths = {
        'grafana': '/var/lib/grafana',
        'prometheus': '/prometheus',
        'loki': '/loki',
        'tempo': '/var/var/tempo',
        'collector': '/state',
    }
    for name in NAMES:
        config_name = hashed(name + '-settings', configs[name])
        objects.append(document('ConfigMap', config_name, ns, data=configs[name]))
        objects.append(
            document(
                'PersistentVolumeClaim',
                name,
                ns,
                spec={
                    'accessModes': ['ReadWriteOnce'],
                    'storageClassName': platform['storage_class'],
                    'resources': {
                        'requests': {
                            'storage': (
                                '1Gi'
                                if name in ('grafana', 'collector')
                                else lan['telemetry_storage']
                            )
                        }
                    },
                },
            )
        )
        container = {
            'name': name,
            'image': images[name],
            'resources': {
                'requests': {'cpu': '50m', 'memory': '128Mi'},
                'limits': {'memory': '512Mi'},
            },
            'ports': [{'containerPort': ports[name]}],
            'volumeMounts': [
                {'name': 'data', 'mountPath': paths[name]},
                {'name': 'settings', 'mountPath': '/etc/broker-monitoring', 'readOnly': True},
            ],
        }
        volumes = [
            {'name': 'data', 'persistentVolumeClaim': {'claimName': name}},
            {'name': 'settings', 'configMap': {'name': config_name}},
        ]
        if name == 'grafana':
            container['env'] = [
                {'name': key, 'value': value}
                for key, value in {
                    'GF_SERVER_ROOT_URL': 'https://' + lan['grafana_host'],
                    'GF_SECURITY_COOKIE_SECURE': 'true',
                    'GF_USERS_ALLOW_SIGN_UP': 'false',
                    'GF_AUTH_ANONYMOUS_ENABLED': 'false',
                    'GF_PLUGINS_PREINSTALL_DISABLED': 'true',
                    'GF_PLUGINS_PREINSTALL_AUTO_UPDATE': 'false',
                    'GF_ANALYTICS_CHECK_FOR_UPDATES': 'false',
                    'GF_ANALYTICS_REPORTING_ENABLED': 'false',
                }.items()
            ]
            container['env'].append(
                {
                    'name': 'GF_SECURITY_ADMIN_PASSWORD',
                    'valueFrom': {'secretKeyRef': {'name': 'grafana-admin', 'key': 'password'}},
                }
            )
            container['volumeMounts'] += [
                {
                    'name': 'settings',
                    'mountPath': '/etc/grafana/provisioning/datasources/local.yaml',
                    'subPath': 'datasources.yaml',
                },
                {
                    'name': 'dashboards',
                    'mountPath': '/var/lib/grafana/dashboards',
                    'readOnly': True,
                },
                {
                    'name': 'dashboards',
                    'mountPath': '/etc/grafana/provisioning/dashboards/provider.yaml',
                    'subPath': 'provider.yaml',
                },
            ]
            volumes.append({'name': 'dashboards', 'configMap': {'name': dashboard_name}})
        else:
            flags = {
                'prometheus': [
                    '--config.file=/etc/broker-monitoring/config.yaml',
                    '--storage.tsdb.path=/prometheus',
                    '--storage.tsdb.retention.time=3d',
                    '--storage.tsdb.retention.size=4GB',
                ],
                'loki': ['-config.file=/etc/broker-monitoring/config.yaml'],
                'tempo': ['-config.file=/etc/broker-monitoring/config.yaml'],
                'collector': ['--config=/etc/broker-monitoring/config.yaml'],
            }
            container['args'] = flags[name]
        if name == 'collector':
            container['securityContext'] = {'runAsUser': 0}
            container['volumeMounts'].append(
                {'name': 'logs', 'mountPath': '/var/log/pods', 'readOnly': True}
            )
            volumes.append(
                {'name': 'logs', 'hostPath': {'path': '/var/log/pods', 'type': 'Directory'}}
            )
        probe_port = 13133 if name == 'collector' else ports[name]
        probe_path = {
            'grafana': '/api/health',
            'prometheus': '/-/ready',
            'loki': '/ready',
            'tempo': '/ready',
            'collector': '/',
        }[name]
        container['readinessProbe'] = {
            'httpGet': {'path': probe_path, 'port': probe_port},
            'periodSeconds': 5,
        }
        deployment = workload('Deployment', name, ns, container, volumes=volumes)
        deployment['spec']['strategy'] = {'type': 'Recreate'}
        # local-path создаёт каталог от root; fsGroup обеспечивает запись сервисному UID.
        deployment['spec']['template']['spec']['securityContext'] = {
            'fsGroup': 1000 if name == 'tempo' else (472 if name == 'grafana' else 65534)
        }
        service_ports = [port('http' if name != 'collector' else 'otlp', ports[name])]
        if name == 'tempo':
            service_ports.append(port('otlp', 4317))
        objects += [deployment, service(name, ns, service_ports)]
    for host, backend, number in (
        (lan['admin_host'], 'admin-bff', 8180),
        (lan['auth_host'], 'keycloak', 8080),
        (lan['grafana_host'], 'grafana', 3000),
    ):
        objects.append(
            {
                'apiVersion': 'networking.k8s.io/v1',
                'kind': 'Ingress',
                'metadata': {
                    'name': backend + '-lan',
                    'namespace': ns,
                    'annotations': {
                        'traefik.ingress.kubernetes.io/router.entrypoints': 'websecure'
                    },
                },
                'spec': {
                    'ingressClassName': 'traefik',
                    'tls': [{'hosts': [host], 'secretName': 'staging-lan-tls'}],
                    'rules': [
                        {
                            'host': host,
                            'http': {
                                'paths': [
                                    {
                                        'path': '/',
                                        'pathType': 'Prefix',
                                        'backend': {
                                            'service': {'name': backend, 'port': {'number': number}}
                                        },
                                    }
                                ]
                            },
                        }
                    ],
                },
            }
        )
    return objects


def sync_keycloak(context, namespace, admin_origin):
    # Импорт realm не обновляет уже существующий клиент: изменяем только redirect URI.
    script = r'''set -eu
config=$(mktemp)
trap 'rm -f "$config"' EXIT
kcadm=/opt/keycloak/bin/kcadm.sh
$kcadm config credentials --server http://localhost:8080 --realm master --user admin --password "$KC_BOOTSTRAP_ADMIN_PASSWORD" --config "$config" >/dev/null
id=$($kcadm get clients -r broker -q clientId=broker-admin --fields id --format csv --noquotes --config "$config")
$kcadm update "clients/$id" -r broker --config "$config" -s 'redirectUris=["ORIGIN/auth/callback"]' -s 'webOrigins=["ORIGIN"]' -s 'attributes."post.logout.redirect.uris"="ORIGIN/"'
'''.replace('ORIGIN', admin_origin)
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


def main():
    from staging_manifest import ROOT

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['init'])
    parser.add_argument('--context', required=True)
    parser.add_argument('--output', type=Path, default=ROOT / '.staging')
    args = parser.parse_args()
    initialize(ROOT, args.output, args.context)


if __name__ == '__main__':
    main()
